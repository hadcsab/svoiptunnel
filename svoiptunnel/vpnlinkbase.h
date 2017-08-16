#ifndef __VPNLINKBASE_H_
#define __VPNLINKBASE_H_

#include "common.h"

using namespace std;
using namespace boost::asio::ip;

class VPNLinkBase;
class VPNLinkEvents
{
public:
	virtual void onConnect( const string& address, unsigned short port ) = 0;
	virtual void onDisconnect( const string& address, unsigned short port ) = 0;
	virtual void onSignalling( VPNLinkBase* vpnLinkPtr, unsigned ch, const boost::asio::const_buffer& buf ) = 0;
	virtual void onReceiveData( VPNLinkBase* vpnLinkPtr, unsigned short port, const boost::asio::const_buffer& buf ) = 0;
	virtual void onSetupData( VPNLinkBase* vpnLinkPtr, unsigned short idx, unsigned short port, const string& callid ) = 0;
};

class VPNLinkBase
{
protected:
	enum { _bufferSize = 1500 };

	enum EMsgType
	{
		EConnect,
		EConnectOK,
		EPing,
		EDisconnect,
		ESetupData,
		ESignalling = 0x0a,
		EData
	};

public:
	enum Mode_t
	{
		eClient,
		eServer,
		eAccepted
	};

public:
	VPNLinkBase( boost::asio::io_service& io_service, Mode_t mode );
	virtual ~VPNLinkBase() = 0;

	void setEvents( VPNLinkEvents* pEvents );
	VPNLinkEvents* getEvents() const { return _pEvents; }

	void Connect();
	void Ping();
	void Disconnect();
	void SetupDataConn( string callid, unsigned short idx, unsigned short port );
	bool isActive() const { return _activityTimer == 0 ? false : true; }
	void sendData( unsigned short port, const boost::asio::const_buffer& buf );
	bool sendSignalling( int ch, const boost::asio::const_buffer& buf, const string& endpointId );
	const string& getId() const { return _id; };

public:
	virtual void onReceive( const boost::asio::ip::detail::endpoint& remote_endpoint, const boost::asio::const_buffer& buf );
	virtual void onAccept( boost::shared_ptr<VPNLinkBase> accepted );
	virtual boost::asio::ip::detail::endpoint remote_endpoint() = 0;
	virtual void send( const boost::asio::const_buffer& buf ) = 0;
	virtual void send( const char* buf, std::size_t len ) = 0;

protected:
	void onLinkReady();
	void onLinkDown();

	void generateId( const string& s );

protected:
	boost::asio::deadline_timer _timer;
	VPNLinkEvents* _pEvents;
	Mode_t _mode;
	bool _bConnected;
	unsigned int _activityTimer;

	typedef map< boost::asio::ip::detail::endpoint, boost::shared_ptr<VPNLinkBase> > VPNClientMap_t;
	VPNClientMap_t _VpnLinkMap;

	//VPN message
	struct SMessage
	{
		struct Header{
			unsigned int _msgType;
			unsigned int _ch;
			unsigned int _size;
		} _header;
		union
		{
			char _data[_bufferSize];
			struct
			{
				unsigned short idx;
				unsigned short port;
				char callid[_bufferSize - sizeof(unsigned short) - sizeof(unsigned short)];
			} _dataSetupMsg;
		};
	};
	SMessage _mess;

private:
	//timer callback
	void timer_tick( const boost::system::error_code& error );

private:
	//connection id used in branch parameter
	string _id;
};

#endif//__VPNLINKBASE_H_
