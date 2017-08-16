#include "common.h"
#include "util.h"
#include "sipdataconn.h"

sipdataconn::sipdataconn( boost::asio::io_service& io_service, VPNLinkBase* vpnPtr, const string& addr, unsigned short port ):
	udpsocket( io_service ),
	_vpnPtr(vpnPtr),
	_vpnPort(0)
{
	connect( addr, port );
};

sipdataconn::sipdataconn( boost::asio::io_service& io_service, VPNLinkBase* vpnPtr ):
	udpsocket( io_service ),
	_vpnPtr( vpnPtr ),
	_vpnPort( 0 )
{
	bind();
};

sipdataconn::~sipdataconn()
{
};

void sipdataconn::linkVpnPort( unsigned short vpnPort )
{
	_vpnPort = vpnPort;
}

void sipdataconn::onReceive( const udp::endpoint& remote_endpoint, const boost::asio::const_buffer& buf )
{
	_vpnPtr->sendData( _vpnPort, buf );
}
