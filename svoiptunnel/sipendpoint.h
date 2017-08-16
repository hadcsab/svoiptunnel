#ifndef SIPENDPOINT_H
#define SIPENDPOINT_H

#include "log.h"
#include "udpsocket.h"
#include "vpnlinkbase.h"
#include "sipdataconn.h"
#include "sipmessage.h"
#include "sipcall.h"

class sipserver;

class sipendpoint: public UDPSocketEvents
{
public:
	sipendpoint( boost::shared_ptr<udpsocket> socket, sipserver* sipServer, unsigned int ch, const string& addr, unsigned short port );
	virtual ~sipendpoint();

	/// generate id from address and port
	static string getId( const string& addr, unsigned short port );

	//UDPSocketEvents interface
	virtual void onReceive( const boost::asio::ip::detail::endpoint& remote_endpoint, const boost::asio::const_buffer& buf );
	virtual void onError( const boost::system::error_code& error );
	virtual void onClose();

	const string getAddr() const { return _sAddr; }
	unsigned short getPort() const { return _nPort; }
	void send( const string& str );
	void check_if_valid();

private:
	boost::shared_ptr<udpsocket> _socket;
	boost::posix_time::ptime _lastActiveTime;
	sipserver* _sipServer;
	unsigned int _ch;
	//IP address of the endpoint
	string _sAddr;
	//UDP port number of the endpoint (signalling)
	unsigned short _nPort;
};

#endif//SIPENDPOINT_H
