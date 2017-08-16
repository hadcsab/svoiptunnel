#include "common.h"
#include "log.h"
#include "vpnlinkbase.h"
#include "udpsocket.h"
#include "util.h"
#include "config.h"
#include "sipendpoint.h"
#include "sipdataconn.h"
#include "sipserver.h"

sipendpoint::sipendpoint( boost::shared_ptr<udpsocket> socket, sipserver* sipServer, unsigned int ch, const string& addr, unsigned short port ):
	_socket( socket ),
	_sipServer( sipServer ),
	_ch( ch ),
	_sAddr( addr ),
	_nPort( port )
{
	_socket->setEvents( this );
	_lastActiveTime = boost::posix_time::second_clock::local_time();
}

sipendpoint::~sipendpoint()
{
}

string sipendpoint::getId( const string& addr, unsigned short port )
{
	string id = addr;
	id.append(":").append( boost::lexical_cast<string>( port ) );
	return id;
}

void sipendpoint::send( const string& str )
{
#ifdef _DEBUG
	log_info() << "sending to " << _sAddr << ":" << _nPort << endl << str << endl;
#endif//_DEBUG

	_socket->send( str.c_str(), str.length() );
	_lastActiveTime = boost::posix_time::second_clock::local_time();
}

/// @brief called when something is received on signalling channel from remote SIP client or server
/// @param remote_endpoint remote endpoint address
/// @param buf data buffer received
void sipendpoint::onReceive( const boost::asio::ip::detail::endpoint& remote_endpoint, const boost::asio::const_buffer& buf )
{
	_lastActiveTime = boost::posix_time::second_clock::local_time();

#ifdef _DEBUG
	const char* pSrc = boost::asio::buffer_cast<const char*>(buf);
	size_t len = boost::asio::buffer_size(buf);
	string s(pSrc, len);
	log_info() << "received from " << _sAddr << ":" << _nPort << endl << s << endl;
#endif//_DEBUG

	_sipServer->processIncoming( this, _ch, buf );
}

void sipendpoint::onError( const boost::system::error_code& error )
{
	_socket->close();
}

void sipendpoint::onClose()
{
	_sipServer->closeEndpoint( getId( _sAddr, _nPort ) );
}

void sipendpoint::check_if_valid()
{
	boost::posix_time::time_duration diff = boost::posix_time::second_clock::local_time() - _lastActiveTime;

	// 1 minute of incactivity
	if( diff.total_seconds() > 60 )
		_socket->close();
}
