#ifndef SIPSERVER_H
#define SIPSERVER_H

#include <iostream>
#include <string>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include "log.h"
#include "udpsocket.h"
#include "vpnlinkbase.h"
#include "sipendpoint.h"

class sipserver: public VPNLinkEvents, public UDPSocketEvents
{
	friend class sipendpoint;
	friend class sipcall;

public:
	sipserver( boost::asio::io_service& io_service, unsigned short nListenPort );
	virtual ~sipserver();

	void SetVPNLink( VPNLinkBase* vpnPtr )
	{
		_vpnPtr = vpnPtr;
		_vpnPtr->setEvents( this );
	}

private:
	//UDPSocketEvents
	//receive from signalling channel
	virtual void onAccept( boost::shared_ptr<udpsocket> pClient, const boost::asio::const_buffer& buf );

	//VPNLinkEvents
	virtual void onConnect( const string& address, unsigned short port );
	virtual void onDisconnect( const string& address, unsigned short port );

	//receive from vpn channel
	virtual void onSignalling( VPNLinkBase* vpnLinkPtr, unsigned ch, const boost::asio::const_buffer& buf );
	virtual void onReceiveData( VPNLinkBase* vpnLinkPtr, unsigned short port, const boost::asio::const_buffer& buf );
	virtual void onSetupData( VPNLinkBase* vpnLinkPtr, unsigned short idx, unsigned short port, const string& callid );

	void processIncoming( sipendpoint* pSipEp, unsigned int ch, const boost::asio::const_buffer& buf );
	void processOutgoing( VPNLinkBase* vpnLinkPtr, unsigned int ch, const boost::asio::const_buffer& buf );

	void registerDataPort( unsigned short port, boost::shared_ptr<sipdataconn> dataConn );
	void unregisterDataPort( unsigned short port );
	void closeEndpoint( const string& id );

private:
	//timer callback
	void timer_tick( const boost::system::error_code& error );

private:
	boost::asio::io_service& _io_service;
	boost::asio::deadline_timer _timer;
	boost::shared_ptr<udpsocket> _socket;
	VPNLinkBase* _vpnPtr;

	//address to channel map
	typedef std::map<string, unsigned> AddrMap_t;
	AddrMap_t _addrMap;

	//vector of clients indexed by channel
	std::vector<boost::shared_ptr<sipendpoint> > _clientVec;

	//map callid to call object
	typedef map<string, boost::shared_ptr<sipcall> > CallMap_t;
	CallMap_t _callMap;

	//map UDP data port to data connection object
	typedef map<unsigned short, boost::shared_ptr<sipdataconn> > DataPortMap_t;
	DataPortMap_t _dataPortMap;
};

#endif//SIPSERVER_H
