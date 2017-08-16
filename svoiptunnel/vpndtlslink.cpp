#include "common.h"
#include "vpndtlslink.h"
#include "log.h"

//Client mode
VPNDTLSLink::VPNDTLSLink( boost::asio::io_service& io_service, const string& addr, unsigned short port ):
	VPNLinkBase( io_service, VPNLinkBase::eClient ),
	_io_service( io_service ),
	_socket( new udpsocket( io_service ) )
{
	_socket->setEvents( this );
	_socket->connect( addr, port );

	_ctx = SSL_CTX_new(DTLS_method());

	//https://www.openssl.org/docs/apps/ciphers.html
	SSL_CTX_set_cipher_list(_ctx, "ALL");

	if (!SSL_CTX_use_certificate_file(_ctx, "certs/server.crt", SSL_FILETYPE_PEM))
		log_error() << "ERROR: no certificate found!" << endl;

	if (!SSL_CTX_use_PrivateKey_file(_ctx, "certs/server.key", SSL_FILETYPE_PEM))
		log_error() << "ERROR: no private key found!" << endl;

	if (!SSL_CTX_check_private_key(_ctx))
		log_error() << "ERROR: invalid private key!" << endl;

	SSL_CTX_set_verify_depth(_ctx, 2);

	/* Create BIO */
	_rbio = BIO_new(BIO_s_mem());
	_wbio = BIO_new(BIO_s_mem());
	_ssl = SSL_new(_ctx);
	SSL_set_bio( _ssl, _rbio, _wbio );
	SSL_set_connect_state( _ssl );

	SSL_connect( _ssl );
	_handshaking = true;
	doSend();
}

//Server mode
VPNDTLSLink::VPNDTLSLink( boost::asio::io_service& io_service, unsigned short nListenPort ):
	VPNLinkBase( io_service, VPNLinkBase::eServer ),
	_io_service( io_service ),
	_socket( new udpsocket( io_service ) ),
	_mode( eSSLServer ),
	_ctx( NULL ),
	_ssl( NULL ),
	_rbio( NULL ),
	_wbio( NULL ),
	_handshaking( false )
{
	_ctx = SSL_CTX_new(DTLS_server_method());

	//https://www.openssl.org/docs/apps/ciphers.html
	SSL_CTX_set_cipher_list(_ctx, "ALL");

	//disable session cache
	SSL_CTX_set_session_cache_mode(_ctx, SSL_SESS_CACHE_OFF);

	if (!SSL_CTX_use_certificate_file(_ctx, "certs/server.crt", SSL_FILETYPE_PEM))
		log_error() << "ERROR: no certificate found!" << endl;

	if (!SSL_CTX_use_PrivateKey_file(_ctx, "certs/server.key", SSL_FILETYPE_PEM))
		log_error() << "ERROR: no private key found!" << endl;

	if (!SSL_CTX_check_private_key(_ctx))
		log_error() << "ERROR: invalid private key!" << endl;

	/* Client has to authenticate */
	SSL_CTX_set_verify(_ctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, dtls_verify_callback);

	//SSL_CTX_set_read_ahead(_ctx, 1);
	SSL_CTX_set_cookie_generate_cb(_ctx, sslbase::generate_cookie);
	SSL_CTX_set_cookie_verify_cb(_ctx, sslbase::verify_cookie);

	_socket->setEvents( this );
	_socket->listen( nListenPort );
}

//Accept mode
VPNDTLSLink::VPNDTLSLink( boost::asio::io_service& io_service, boost::shared_ptr<udpsocket> accepted, SSL_CTX* ctx ):
	VPNLinkBase( io_service, VPNLinkBase::eAccepted ),
	_io_service( io_service ),
	_socket( accepted ),
	_mode( eSSLAccept ),
	_ctx( ctx ),
	_handshaking( false )
{
	_socket->setEvents( this );

	string s = _socket->remote_endpoint().address().to_string();
	s.append( ":" );
	s.append( boost::lexical_cast<string>(_socket->remote_endpoint().port()) );
	generateId( s );

	/* Set new fd and set BIO to connected */
	_rbio = BIO_new(BIO_s_mem());
	_wbio = BIO_new(BIO_s_mem());

	_ssl = SSL_new(_ctx);
	SSL_set_bio( _ssl, _rbio, _wbio );
	SSL_set_accept_state(_ssl);

	_handshaking = true;
	doSend();
}

VPNDTLSLink::~VPNDTLSLink()
{
	SSL_CTX_free( _ctx );
}

void VPNDTLSLink::doSend()
{
	if( BIO_pending( _wbio ) )
	{
		int bytes = 0;

		while( (bytes = BIO_pending( _wbio )) > 0 )
		{
			vector<char> data( bytes );
			bytes = BIO_read( _wbio, data.data(), data.size() );

			if( bytes > 0 )
				_socket->send( data.data(), data.size() );
			else if( ssl_is_fatal_error( ssl_get_error( _ssl, bytes ) ) )
				log_error() << "SSL error receiving in SSL_send" << endl;

			if( _handshaking && SSL_is_init_finished( _ssl ) )
				_handshaking = false;
		}
	}
}

void VPNDTLSLink::onReceive( const boost::asio::ip::detail::endpoint& remote_endpoint, const boost::asio::const_buffer& buf )
{
	int bytes = BIO_write( _rbio, boost::asio::buffer_cast<const char*>(buf), boost::asio::buffer_size(buf) );
	if( ssl_is_fatal_error( ssl_get_error(_ssl, bytes) ) || 
		(bytes != (int)boost::asio::buffer_size(buf) && !BIO_should_retry( _rbio )) )
		log_error() << "SSL error receiving while BIO_write" << endl;

	if( _handshaking )
	{
		int ret = 0;
		if( _mode == eSSLAccept )
			ret = SSL_accept( _ssl );
		else if( _mode == eSSLClient )
			ret = SSL_connect( _ssl );
		//we shouldn't receive on eSSLServer mode

		if( ret > 0 )
		{
			log_info() << "DTLS handshake completed successfully with " << remote_endpoint.address().to_string() << ":" << remote_endpoint.port() << endl;
			_handshaking = false;
			onLinkReady();
		}
		else if( ssl_is_fatal_error( ssl_get_error(_ssl, ret) ) )
			log_error() << "SSL error receiving in SSL_read" << endl;

		doSend();
	}

	if( !_handshaking )
	{
		vector<char> data( SSL_BUFFER_SIZE );
		bytes = SSL_read( _ssl, data.data(), data.size() );
		if( bytes > 0 )
		{
			boost::asio::ip::detail::endpoint ep( remote_endpoint.address(), remote_endpoint.port() );
			VPNLinkBase::onReceive( ep, boost::asio::buffer(data.data(), bytes) );
		}
		else if( ssl_is_fatal_error( ssl_get_error( _ssl, bytes ) ) )
			log_error() << "SSL error receiving in SSL_read" << endl;
	}
}

void VPNDTLSLink::onAccept( boost::shared_ptr<udpsocket> accepted, const boost::asio::const_buffer& buf )
{
	boost::shared_ptr<VPNLinkBase> link( new VPNDTLSLink( _io_service, accepted, _ctx ) );
	VPNLinkBase::onAccept( link );

	//pass the received data to the new socket
	boost::asio::ip::detail::endpoint ep( accepted->remote_endpoint().address(), accepted->remote_endpoint().port() );
	link->onReceive( ep, buf );
}

void VPNDTLSLink::send( const boost::asio::const_buffer& buf )
{
	if( _handshaking )
		return;

	int bytes = SSL_write( _ssl, boost::asio::buffer_cast<const void*>( buf ), boost::asio::buffer_size( buf ) );
	if( !ssl_is_fatal_error( ssl_get_error( _ssl, bytes ) ) )
		doSend();
}

void VPNDTLSLink::send( const char* buf, std::size_t len )
{
	if( _handshaking )
		return;

	int bytes = SSL_write( _ssl, buf, len );
	if( !ssl_is_fatal_error( ssl_get_error( _ssl, bytes ) ) )
		doSend();
}
