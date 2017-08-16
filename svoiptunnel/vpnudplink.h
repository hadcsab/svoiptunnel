#ifndef __VPNUDPLINK_H_
#define __VPNUDPLINK_H_

#include "common.h"
#include "vpnlinkbase.h"
#include "udpsocket.h"

using namespace std;
using namespace boost::asio::ip;

class VPNUDPLink: public VPNLinkBase, public UDPSocketEvents
{
public:
	//client mode
	VPNUDPLink( boost::asio::io_service& io_service, const string& addr, unsigned short port );
	//server mode
	VPNUDPLink( boost::asio::io_service& io_service, unsigned short nListenPort );

	virtual ~VPNUDPLink();
	virtual boost::asio::ip::detail::endpoint remote_endpoint()
	{
		boost::asio::ip::detail::endpoint ep( _socket->remote_endpoint().address(), _socket->remote_endpoint().port() );
		return ep;
	}

private:
	//accepted mode
	VPNUDPLink( boost::asio::io_service& io_service, boost::shared_ptr<udpsocket> accepted );

private:
	virtual void onReceive( const boost::asio::ip::detail::endpoint& remote_endpoint, const boost::asio::const_buffer& buf );
	virtual void onAccept( boost::shared_ptr<udpsocket> accepted, const boost::asio::const_buffer& buf );
	virtual void onError( const boost::system::error_code& error );
	virtual void send( const boost::asio::const_buffer& buf );
	virtual void send( const char* buf, std::size_t len );

private:
	boost::asio::io_service& _io_service;
	boost::shared_ptr<udpsocket> _socket;
};

#endif//__VPNUDPLINK_H_
