#ifndef __VPNTLSLINK_H_
#define __VPNTLSLINK_H_

#include "common.h"

#include "vpnlinkbase.h"
#include "tcpsocket.h"
#include "sslbase.h"

using namespace std;
using namespace boost::asio::ip;

class VPNTLSLink: public VPNLinkBase, public TCPSocketEvents, public sslbase
{
public:
	//client mode
	VPNTLSLink( boost::asio::io_service& io_service, const string& addr, unsigned short port );
	//server mode
	VPNTLSLink( boost::asio::io_service& io_service, unsigned short nListenPort );

	virtual ~VPNTLSLink();
	virtual boost::asio::ip::detail::endpoint remote_endpoint()
	{
		boost::asio::ip::detail::endpoint ep( _socket->remote_endpoint().address(), _socket->remote_endpoint().port() );
		return ep;
	}

private:
	//accepted mode
	VPNTLSLink( boost::asio::io_service& io_service, boost::shared_ptr<tcpsocket> accepted, SSL_CTX* ctx );
	void doSend();

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

	EMode _mode;
	SSL_CTX *_ctx;
	SSL *_ssl;
	BIO *_rbio;
	BIO *_wbio;
	bool _handshaking;
};

#endif//__VPNTLSLINK_H_
