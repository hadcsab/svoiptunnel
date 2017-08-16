#include "common.h"
#include "log.h"
#include "tcpsocket.h"

//unconnected socket
tcpsocket::tcpsocket( boost::asio::io_service& io_service, TCPSocketEvents* pEvents ):
	_pEvents( pEvents ),
	_acceptor( io_service ),
	_socket( io_service ),
	_reconnect( false ),
	_timer( io_service )
{
}

tcpsocket::~tcpsocket()
{
}

void tcpsocket::timed_connect()
{
	//connect after 5 sec
	_timer.expires_from_now( boost::posix_time::seconds(5) );
	_timer.async_wait( boost::bind( &tcpsocket::connect, shared_from_this(), boost::asio::placeholders::error ) );
}

void tcpsocket::connect( const boost::system::error_code& error )
{
	if( error == boost::asio::error::operation_aborted )
		return;

	_socket.async_connect( _remote_endpoint, boost::bind(&tcpsocket::handle_connect, shared_from_this(), boost::asio::placeholders::error) );
}

bool tcpsocket::connect( const string& addr, unsigned short port )
{
	try {
		_socket.open( tcp::v4() );
		_socket.set_option( boost::asio::socket_base::reuse_address(true) );
		_socket.set_option( tcp::no_delay(true) );
		_socket.bind( tcp::endpoint(tcp::v4(), 0) );
		_localPort = _socket.local_endpoint().port();
		_reconnect = true;

		//resolve address
		tcp::resolver resolver( _socket.get_io_service() );
		tcp::resolver::query query( addr, boost::lexical_cast<string>( port ) );
		tcp::resolver::iterator iter = resolver.resolve(query);
		tcp::resolver::iterator end; // End marker.
		if( iter != end )
		{
			_remote_endpoint = *iter++;
			boost::system::error_code ec;
			connect( ec );
		}
		else
			log_error() << "Cannot resolve host " << addr << endl;
	}
	catch( std::exception& e )
	{
		log_error() << "error while connecting to " << addr << ":" << e.what() << endl;
		return false;
	}

	return true;
}

bool tcpsocket::listen( unsigned short nListenPort )
{
	try {
		_acceptor.open( tcp::v4() );
		_acceptor.set_option( boost::asio::socket_base::reuse_address(true) );
		_acceptor.bind( tcp::endpoint(tcp::v4(), nListenPort) );
		_localPort = nListenPort;
		_acceptor.listen();
		_reconnect = false;

		start_accept();
	}
	catch( std::exception& e )
	{
		log_error() << "error while listen on " << nListenPort << ":" << e.what() << endl;
		return false;
	}

	return true;
}

void tcpsocket::close()
{
	_socket.close();
}

void tcpsocket::onReceive( const boost::asio::const_buffer& buf )
{
	if( _pEvents )
		_pEvents->onReceive( buf );
}

void tcpsocket::onAccept( boost::shared_ptr<tcpsocket> pClient )
{
	if( _pEvents )
		_pEvents->onAccept( pClient );
}

void tcpsocket::onError( const boost::system::error_code& error )
{
	if( _pEvents )
		_pEvents->onError( error );
	close();

	if( _reconnect )
		timed_connect();
}

void tcpsocket::onClose()
{
	if( _pEvents )
		_pEvents->onClose();
}

void tcpsocket::start_receive()
{
	_socket.async_receive(
		boost::asio::buffer(_recv_buffer), 
		boost::bind(&tcpsocket::handle_receive, 
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
		)
	);
}

void tcpsocket::start_accept()
{
	boost::shared_ptr<tcpsocket> pSock( new tcpsocket(_acceptor.get_io_service()) );
	_acceptor.async_accept(
		pSock->socket(),
		boost::bind( &tcpsocket::handle_accept, 
			shared_from_this(), 
			pSock, 
			boost::asio::placeholders::error
		)
	);
}

void tcpsocket::handle_connect( const boost::system::error_code& error )
{
	if( !error )
	{
		if( _pEvents )
			_pEvents->onConnected();

		start_receive();
	}
	else
	{
#ifdef _DEBUG
		log_error() << "error connecting:" << error.value() << " " << error.message() << endl;
#endif//_DEBUG

		timed_connect();
	}
}

void tcpsocket::handle_receive( const boost::system::error_code& error, std::size_t bytes_transferred )
{
	if( !error || error == boost::asio::error::message_size )
	{
		onReceive( boost::asio::const_buffer( _recv_buffer.data(), bytes_transferred ) );
		start_receive();
	}
	else if( error == boost::asio::error::operation_aborted )
		return;
	else
	{
#ifdef _DEBUG
		log_error() << "error receiving:" << error.value() << " " << error.message() << endl;
#endif//_DEBUG

		//raise error
		onError( error );
	}
}

void tcpsocket::handle_accept( boost::shared_ptr<tcpsocket> pSock, const boost::system::error_code& error )
{
	if (!error)
	{
		onAccept( pSock );

		start_accept();
	}
	else if( error == boost::asio::error::operation_aborted )
		return;
	else
	{
#ifdef _DEBUG
		log_error() << "error accepting:" << error.value() << " " << error.message() << endl;
#endif//_DEBUG

		//raise error
		onError( error );
	}
}

void tcpsocket::handle_sent( const boost::system::error_code& error, std::size_t bytes_transferred )
{
#ifdef _DEBUG
	if( error )
		log_error() << "error sending:" << error.value() << " " << error.message() << endl;
#endif//_DEBUG
}

void tcpsocket::send( const boost::asio::const_buffer& buf )
{
	try {
		_socket.async_send( boost::asio::buffer( boost::asio::buffer_cast<const char*>( buf ), boost::asio::buffer_size( buf ) ), 
			boost::bind(&tcpsocket::handle_sent, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred) );
	}
	catch( std::exception& e )
	{
		log_error() << "error while sending:" << e.what() << endl;
	}
}

void tcpsocket::send( const char* buf, std::size_t len )
{
	try {
		_socket.async_send( boost::asio::buffer( buf, len ), 
			boost::bind(&tcpsocket::handle_sent, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred) );
	}
	catch( std::exception& e )
	{
		log_error() << "error while sending:" << e.what() << endl;
	}
}
