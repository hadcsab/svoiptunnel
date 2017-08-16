#ifndef __TCPSOCKET_H_
#define __TCPSOCKET_H_

using namespace std;
using namespace boost::asio::ip;

class tcpsocket;

class TCPSocketEvents
{
public:
	virtual void onConnected() {};
	virtual void onReceive( const boost::asio::const_buffer& buf ) {};
	virtual void onAccept( boost::shared_ptr<tcpsocket> pClient ) {};
	virtual void onError( const boost::system::error_code& error ) {};
	virtual void onClose() {};
};

class tcpsocket: public boost::enable_shared_from_this<tcpsocket>
{
protected:
	enum { _bufferSize = 1500 };

public:
	tcpsocket( boost::asio::io_service& io_service, TCPSocketEvents* pEvents = NULL );
	virtual ~tcpsocket();

	bool connect( const string& addr, unsigned short port );
	bool listen( unsigned short nListenPort );
	void close();

	void setEvents( TCPSocketEvents* pEvents ) { _pEvents = pEvents; };

	virtual void onReceive( const boost::asio::const_buffer& buf );
	virtual void onAccept( boost::shared_ptr<tcpsocket> pClient );

	void send( const boost::asio::const_buffer& buf );
	void send( const char* buf, std::size_t len );
	virtual void onError( const boost::system::error_code& error );
	virtual void onClose();

	unsigned short localPort() const { return _localPort; }
	tcp::socket::lowest_layer_type& socket() { return _socket.lowest_layer(); }

	const tcp::endpoint remote_endpoint() const { return _socket.remote_endpoint(); }

	void start_receive();
	void start_accept();

private:
	void handle_connect( const boost::system::error_code& error );
	void handle_receive( const boost::system::error_code& error, std::size_t bytes_transferred );
	void handle_accept( boost::shared_ptr<tcpsocket> pSock, const boost::system::error_code& error );
	void handle_sent( const boost::system::error_code& error, std::size_t bytes_transferred );
	void connect( const boost::system::error_code& error );

protected:
	void timed_connect();

protected:
	TCPSocketEvents* _pEvents;
	tcp::acceptor _acceptor;
	tcp::socket _socket;
	tcp::endpoint _remote_endpoint;
	bool _reconnect;
	boost::array<char, _bufferSize> _recv_buffer;
	unsigned short _localPort;
	boost::asio::deadline_timer _timer;
};

#endif //__TCPSOCKET_H_
