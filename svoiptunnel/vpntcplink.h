#ifndef __VPNTCPLINK_H_
#define __VPNTCPLINK_H_

#include "common.h"
#include "vpnlinkbase.h"
#include "tcpsocket.h"

using namespace std;
using namespace boost::asio::ip;

class VPNTCPLink: public VPNLinkBase, public TCPSocketEvents
{
public:
	//client mode
	VPNTCPLink( boost::asio::io_service& io_service, const string& addr, unsigned short port );
	//server mode
	VPNTCPLink( boost::asio::io_service& io_service, unsigned short nListenPort );

	virtual ~VPNTCPLink();
	virtual boost::asio::ip::detail::endpoint remote_endpoint()
	{
		boost::asio::ip::detail::endpoint ep( _socket->remote_endpoint().address(), _socket->remote_endpoint().port() );
		return ep;
	}

private:
	//accepted mode
	VPNTCPLink( boost::asio::io_service& io_service, boost::shared_ptr<tcpsocket> accepted );

private:
	virtual void onConnected();
	virtual void onReceive( const boost::asio::const_buffer& buf );
	virtual void onAccept( boost::shared_ptr<tcpsocket> pClient );
	virtual void onError( const boost::system::error_code& error );
	virtual void onClose();
	virtual void send( const boost::asio::const_buffer& buf );
	virtual void send( const char* buf, std::size_t len );

private:
	boost::asio::io_service& _io_service;
	boost::shared_ptr<tcpsocket> _socket;
};

#endif//__VPNTCPLINK_H_
