#ifndef __VPNDTLSLINK_H_
#define __VPNDTLSLINK_H_

#include "common.h"

#include "vpnlinkbase.h"
#include "udpsocket.h"
#include "sslbase.h"

using namespace std;
using namespace boost::asio::ip;

class VPNDTLSLink: public VPNLinkBase, public UDPSocketEvents, public sslbase
{
public:
	//client mode
	VPNDTLSLink( boost::asio::io_service& io_service, const string& addr, unsigned short port );
	//server mode
	VPNDTLSLink( boost::asio::io_service& io_service, unsigned short nListenPort );

	virtual ~VPNDTLSLink();
	virtual boost::asio::ip::detail::endpoint remote_endpoint()
	{
		boost::asio::ip::detail::endpoint ep( _socket->remote_endpoint().address(), _socket->remote_endpoint().port() );
		return ep;
	}

private:
	//accepted mode
	VPNDTLSLink( boost::asio::io_service& io_service, boost::shared_ptr<udpsocket> accepted, SSL_CTX* ctx );
	void doSend();

private:
	virtual void onReceive( const boost::asio::ip::detail::endpoint& remote_endpoint, const boost::asio::const_buffer& buf );
	virtual void onAccept( boost::shared_ptr<udpsocket> accepted, const boost::asio::const_buffer& buf );
	virtual void send( const boost::asio::const_buffer& buf );
	virtual void send( const char* buf, std::size_t len );

private:
	boost::asio::io_service& _io_service;
	boost::shared_ptr<udpsocket> _socket;

	EMode _mode;
	SSL_CTX *_ctx;
	SSL *_ssl;
	BIO *_rbio;
	BIO *_wbio;
	bool _handshaking;
};

#endif//__VPNDTLSLINK_H_
