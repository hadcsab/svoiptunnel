#ifndef __UDPSOCKET_H_
#define __UDPSOCKET_H_

using namespace std;
using namespace boost::asio::ip;

class udpsocket;

class UDPSocketEvents
{
public:
	virtual void onReceive( const boost::asio::ip::detail::endpoint& remote_endpoint, const boost::asio::const_buffer& buf ) {};
	virtual void onAccept( boost::shared_ptr<udpsocket> accepted, const boost::asio::const_buffer& buf ) {};
	virtual void onError( const boost::system::error_code& error ) {};
	virtual void onClose() {};
};

class udpsocket: public boost::enable_shared_from_this<udpsocket>
{
protected:
	enum { _bufferSize = 1500 };

public:
	//unbound socket
	udpsocket( boost::asio::io_service& io_service, UDPSocketEvents* pEvents = NULL );
	virtual ~udpsocket();

	bool connect( const string& addr, unsigned short port );
	bool listen( unsigned short nListenPort );
	bool bind();
	bool bindandconnect( const udp::endpoint& local_endpoint, const udp::endpoint& remote_endpoint );
	void close();

	void setEvents( UDPSocketEvents* pEvents ) { _pEvents = pEvents; };
#ifdef _WIN32
	void setServerSocket( boost::shared_ptr<udpsocket> server ) { _serverSocket = server; }
	void clearClient( const udp::endpoint& remote_endpoint ) { _clientMap.erase( remote_endpoint ); }
#endif//_WIN32

	virtual void onReceive( const udp::endpoint& remote_endpoint, const boost::asio::const_buffer& buf );
	virtual void onAccept( const udp::endpoint& local_endpoint, const udp::endpoint& remote_endpoint, const boost::asio::const_buffer& buf );
	virtual void onError( const boost::system::error_code& error );
	virtual void onClose();

	void send( const boost::asio::const_buffer& buf );
	void send( const char* buf, std::size_t len );
	void sendto( const udp::endpoint& remote_endpoint, const boost::asio::const_buffer& buf );
	void sendto( const udp::endpoint& remote_endpoint, const char* buf, std::size_t len );

	unsigned short localPort() const { return _localPort; }
	int socket() { return _socket.native_handle(); }

	const udp::endpoint remote_endpoint() const { return _socket.remote_endpoint(); }

private:
	void start_receive();
	void handle_receive( const boost::system::error_code& error, std::size_t bytes_transferred );
	void start_listen();
	void handle_listen( const boost::system::error_code& error, std::size_t bytes_transferred );
	void handle_sent( const boost::system::error_code& error, std::size_t bytes_transferred );

protected:
//this could be moved to separate class udpserversocket
//for windows we need a map to bind the clients as the socket layer doesn't really connect
#ifdef _WIN32
	map<udp::endpoint, boost::shared_ptr<udpsocket> > _clientMap;
	boost::shared_ptr<udpsocket> _serverSocket;
#endif//_WIN32

	UDPSocketEvents* _pEvents;
	udp::socket _socket;
	bool _listening;
	// this could be move to udpserversocket
	udp::endpoint _remote_endpoint;
	boost::array<char, _bufferSize> _recv_buffer;
	unsigned short _localPort;
};

#endif//__UDPSOCKET_H_
