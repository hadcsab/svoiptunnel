#ifndef SIPDATACONN_H
#define SIPDATACONN_H

#include "udpsocket.h"
#include "vpnlinkbase.h"

class sipdataconn: public udpsocket
{
public:
	sipdataconn( boost::asio::io_service& io_service, VPNLinkBase* vpnPtr, const string& addr, unsigned short port );
	sipdataconn( boost::asio::io_service& io_service, VPNLinkBase* vpnPtr );
	virtual ~sipdataconn();

	void linkVpnPort( unsigned short vpnPort );

protected:
	virtual void onReceive( const udp::endpoint& remote_endpoint, const boost::asio::const_buffer& buf );

private:
	VPNLinkBase* _vpnPtr;
	unsigned short _vpnPort;
};

#endif//SIPDATACONN_H
