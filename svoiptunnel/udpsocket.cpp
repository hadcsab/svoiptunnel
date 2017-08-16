#include "common.h"
#include "log.h"
#include "udpsocket.h"

//setsockopt
//TODO: use UDP_CORK, TCP_CORK on linux to hold back sent packets
//limit to max packet size aprox 1498 bytes

//unconnected socket
udpsocket::udpsocket( boost::asio::io_service& io_service, UDPSocketEvents* pEvents ):
	_pEvents( pEvents ),
	_socket( io_service ),
	_listening( false ),
	_localPort( 0 )
{
}

udpsocket::~udpsocket()
{
}

bool udpsocket::connect( const string& addr, unsigned short port )
{
	try {
		//resolve address
		udp::resolver resolver( _socket.get_io_service() );
		udp::resolver::query query( addr, boost::lexical_cast<string>( port ) );
		udp::resolver::iterator iter = resolver.resolve(query);
		udp::resolver::iterator end; // End marker.
		if( iter != end )
		{
			_remote_endpoint = *iter++;
			_socket.connect( _remote_endpoint );
			_localPort = _socket.local_endpoint().port();

			start_receive();
		}
		else
		{
			log_error() << "Cannot resolve host " << addr << endl;
			return false;
		}
	}
	catch( std::exception& e )
	{
		log_error() << "error while connecting to " << addr << ":" << e.what() << endl;
		return false;
	}

	return true;
}

bool udpsocket::listen( unsigned short nListenPort )
{
	try {
		_socket.open( udp::v4() );
		_socket.set_option( boost::asio::socket_base::reuse_address(true) );
		_socket.bind( udp::endpoint(udp::v4(), nListenPort) );

		_localPort = nListenPort;
		_listening = true;
		start_listen();
	}
	catch( std::exception& e )
	{
		log_error() << "error while listen on " << nListenPort << ":" << e.what() << endl;
		return false;
	}

	return true;
}

bool udpsocket::bind()
{
	try {
		_socket.open( udp::v4() );
		_socket.set_option( boost::asio::socket_base::reuse_address(true) );
		_socket.bind( udp::endpoint(udp::v4(), 0) );

		_localPort = _socket.local_endpoint().port();
	}
	catch( std::exception& e )
	{
		log_error() << "error while binding to any port:" << e.what() << endl;
		return false;
	}

	return true;
}

bool udpsocket::bindandconnect( const udp::endpoint& local_endpoint, const udp::endpoint& remote_endpoint )
{
	try {
		_socket.open( udp::v4() );
		_socket.set_option( boost::asio::socket_base::reuse_address(true) );
		_socket.bind( local_endpoint );
		_remote_endpoint = remote_endpoint;
		_socket.connect( remote_endpoint );
		_localPort = local_endpoint.port();
		start_receive();
	}
	catch( std::exception& e )
	{
		log_error() << "error while binding to " << _localPort << ":" << e.what() << endl;
		return false;
	}

	return true;
}

void udpsocket::close()
{
	_socket.close();

#ifdef _WIN32
	if( _serverSocket )
		_serverSocket->clearClient( _remote_endpoint );
#endif//_WIN32

	onClose();
}

void udpsocket::onReceive( const udp::endpoint& remote_endpoint, const boost::asio::const_buffer& buf )
{
	if( _pEvents )
	{
		boost::asio::ip::detail::endpoint ep( remote_endpoint.address(), remote_endpoint.port() );
		_pEvents->onReceive( ep, buf );
	}
}

void udpsocket::onAccept( const udp::endpoint& local_endpoint, const udp::endpoint& remote_endpoint, const boost::asio::const_buffer& buf )
{
	if( _pEvents )
	{
		boost::shared_ptr<udpsocket> accepted( new udpsocket( _socket.get_io_service() ) );
		accepted->bindandconnect( local_endpoint, remote_endpoint );
#ifdef _WIN32
		accepted->setServerSocket( shared_from_this() );
		_clientMap.insert( make_pair(remote_endpoint, accepted) );
#endif//_WIN32

		_pEvents->onAccept( accepted, buf );
	}
}

void udpsocket::onError( const boost::system::error_code& error )
{
	if( _pEvents )
		_pEvents->onError( error );
}

void udpsocket::onClose()
{
	if( _pEvents )
		_pEvents->onClose();
}

void udpsocket::start_listen()
{
	_socket.async_receive_from(
		boost::asio::buffer(_recv_buffer), 
		_remote_endpoint,
		boost::bind(&udpsocket::handle_listen, 
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
		)
	);
}

void udpsocket::handle_listen( const boost::system::error_code& error, std::size_t bytes_transferred )
{
	if( !error || error == boost::asio::error::message_size )
	{
#ifdef _WIN32
		map<udp::endpoint, boost::shared_ptr<udpsocket> >::iterator it = _clientMap.find( _remote_endpoint );
		if( it != _clientMap.end() )
			it->second->onReceive( _remote_endpoint, boost::asio::const_buffer( _recv_buffer.data(), bytes_transferred ) );
		else
			onAccept( _socket.local_endpoint(), _remote_endpoint, boost::asio::const_buffer( _recv_buffer.data(), bytes_transferred ) );
#else//_WIN32
		onAccept( _socket.local_endpoint(), _remote_endpoint, boost::asio::const_buffer( _recv_buffer.data(), bytes_transferred ) );
#endif//_WIN32
	}
	else if( error == boost::asio::error::operation_aborted )
		return;
	else
	{
#ifdef _DEBUG
		log_error() << "error receiving from " << _remote_endpoint.address().to_string() << ":" << _remote_endpoint.port() << ": " << error.value() << " " << error.message() << endl;
#endif//_DEBUG

		//raise error
		onError( error );
	}

	start_listen();
}

void udpsocket::start_receive()
{
	_socket.async_receive(
		boost::asio::buffer(_recv_buffer), 
		boost::bind(&udpsocket::handle_receive, 
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
		)
	);
}

void udpsocket::handle_receive( const boost::system::error_code& error, std::size_t bytes_transferred )
{
	if( !error || error == boost::asio::error::message_size )
	{
		onReceive( _remote_endpoint, boost::asio::const_buffer( _recv_buffer.data(), bytes_transferred ) );
		start_receive();
	}
	else if( error == boost::asio::error::operation_aborted )
		return;
	else
	{
#ifdef _DEBUG
		log_error() << "error receiving from " << _remote_endpoint.address().to_string() << ":" << _remote_endpoint.port() << ": " << error.value() << " " << error.message() << endl;
#endif//_DEBUG

		//raise error
		onError( error );
	}
}

void udpsocket::handle_sent( const boost::system::error_code& error, std::size_t bytes_transferred )
{
	if( error )
	{
#ifdef _DEBUG
		log_error() << "error sending to " << _remote_endpoint.address().to_string() << ":" << _remote_endpoint.port() << ": " << error.value() << " " << error.message() << endl;
#endif//_DEBUG

		//raise error
		onError( error );
	}
}

void udpsocket::send( const boost::asio::const_buffer& buf )
{
	try {
		_socket.async_send( boost::asio::buffer( boost::asio::buffer_cast<const char*>( buf ), boost::asio::buffer_size( buf ) ), 
			boost::bind(&udpsocket::handle_sent, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred) );
	}
	catch( std::exception& e )
	{
		log_error() << "error while sending to " << _remote_endpoint.address().to_string() << ":" << _remote_endpoint.port() << ": " << e.what() << endl;
	}
}

void udpsocket::send( const char* buf, std::size_t len )
{
	try {
		_socket.async_send( boost::asio::buffer( buf, len ), 
			boost::bind(&udpsocket::handle_sent, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred) );
	}
	catch( std::exception& e )
	{
		log_error() << "error while sending to " << _remote_endpoint.address().to_string() << ":" << _remote_endpoint.port() << ": " << e.what() << endl;
	}
}

void udpsocket::sendto( const udp::endpoint& remote_endpoint, const boost::asio::const_buffer& buf )
{
	try {
		_socket.async_send_to( boost::asio::buffer( boost::asio::buffer_cast<const char*>( buf ), boost::asio::buffer_size( buf ) ), remote_endpoint, 
			boost::bind(&udpsocket::handle_sent, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred) );
	}
	catch( std::exception& e )
	{
		log_error() << "error while sending to " << remote_endpoint.address().to_string() << ":" << remote_endpoint.port() << ": " << e.what() << endl;
	}
}

void udpsocket::sendto( const udp::endpoint& remote_endpoint, const char* buf, std::size_t len )
{
	try {
		_socket.async_send_to( boost::asio::buffer( buf, len ), remote_endpoint, 
			boost::bind(&udpsocket::handle_sent, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred) );
	}
	catch( std::exception& e )
	{
		log_error() << "error while sending to " << remote_endpoint.address().to_string() << ":" << remote_endpoint.port() << ": " << e.what() << endl;
	}
}
