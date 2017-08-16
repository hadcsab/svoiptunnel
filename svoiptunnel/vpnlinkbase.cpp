#include "common.h"
#include "vpnlinkbase.h"
#include "log.h"

//Accepted socket timer 12 * 5s = 60s
const unsigned int ACTIVITY_TIMER_ACCEPTED_DEFAULT( 12 );

//Client socket timer after PING
const unsigned int ACTIVITY_TIMER_CLIENT_DEFAULT( 2 );

VPNLinkBase::VPNLinkBase( boost::asio::io_service& io_service, VPNLinkBase::Mode_t mode ):
	_timer( io_service, boost::posix_time::seconds(0) ),	//we want to trigger the first occurance right away
	_pEvents( NULL ),
	_mode( mode ),
	_bConnected( false ),
	_activityTimer( 0 )
{
}

VPNLinkBase::~VPNLinkBase()
{
}

void VPNLinkBase::setEvents( VPNLinkEvents* pEvents )
{
	_pEvents = pEvents;
}

void VPNLinkBase::Connect()
{
#ifdef _DEBUG
	log_debug() << "connect client" << endl;
#endif//_DEBUG
	_mess._header._msgType = EConnect;
	_mess._header._ch = 0xffffffff;
	_mess._header._size = 0;
	send( (const char*)&_mess, sizeof(SMessage::Header) );
	_activityTimer = ACTIVITY_TIMER_CLIENT_DEFAULT;
}

void VPNLinkBase::Ping()
{
	_mess._header._msgType = EPing;
	_mess._header._ch = 0xffffffff;
	_mess._header._size = 0;
	send( (const char*)&_mess, sizeof(SMessage::Header) );
}

void VPNLinkBase::Disconnect()
{
#ifdef _DEBUG
	log_debug() << "disconnect client" << endl;
#endif//_DEBUG
	if( _bConnected && (_mode == eClient || _mode == eAccepted) )
	{
		_mess._header._msgType = EDisconnect;
		_mess._header._ch = 0xffffffff;
		_mess._header._size = 0;
		send( (const char*)&_mess, sizeof(SMessage::Header) );
		_bConnected = false;
		_activityTimer = 0;
	}
}

void VPNLinkBase::onAccept( boost::shared_ptr<VPNLinkBase> accepted )
{
#ifdef _DEBUG
	log_debug() << "accept vpn client from " << accepted->remote_endpoint().address().to_string() << ":" << accepted->remote_endpoint().port() << endl;
#endif//_DEBUG

	_VpnLinkMap.insert( make_pair(accepted->remote_endpoint(), accepted ) );
	accepted->setEvents( _pEvents );
}

void VPNLinkBase::onReceive( const boost::asio::ip::detail::endpoint& remote_endpoint, const boost::asio::const_buffer& buf )
{
	if( _mode == eServer )
		return;

	if( boost::asio::buffer_size(buf) >= sizeof(SMessage::Header) )
	{
		if( _mode == eClient )
			_activityTimer = ACTIVITY_TIMER_CLIENT_DEFAULT;
		else
			_activityTimer = ACTIVITY_TIMER_ACCEPTED_DEFAULT;

		const SMessage* pMsg = boost::asio::buffer_cast<const SMessage*>(buf);

		switch( pMsg->_header._msgType )
		{
		case EConnect:
		{
			if( _mode == eAccepted )
			{
				log_info() << "VPN client CONNECT from " << remote_endpoint.address().to_string() << ":" << remote_endpoint.port() << endl;
				if( _pEvents && !_bConnected )
					_pEvents->onConnect( remote_endpoint.address().to_string(), remote_endpoint.port() );
				_bConnected = true;

				//reply with CONNECTOK
				_mess._header._msgType = EConnectOK;
				_mess._header._ch = 0xffffffff;
				_mess._header._size = 0;
				send( (const char*)&_mess, sizeof(SMessage::Header) );
			}
			break;
		}
		case EConnectOK:
		{
			if( _mode == eClient )
			{
				log_info() << "CONNECTED to " << remote_endpoint.address().to_string() << ":" << remote_endpoint.port() << endl;
				_bConnected = true;
			}
			break;
		}
		case EDisconnect:
		{
			if( _mode == eClient || _mode == eAccepted )
			{
				log_info() << "VPN client DISCONNECT from " << remote_endpoint.address().to_string() << ":" << remote_endpoint.port() << endl;
				if( _pEvents )
					_pEvents->onDisconnect( remote_endpoint.address().to_string(), remote_endpoint.port() );

				//make connection inactive
				_bConnected = false;
				_activityTimer = 0;
			}
			break;
		}
		case EPing:
		{
			if( _mode == eAccepted )
			{
				//reply with PING
				_mess._header._msgType = EPing;
				_mess._header._ch = 0xffffffff;
				_mess._header._size = 0;
				send( (const char*)&_mess, sizeof(SMessage::Header) );
			}
			break;
		}
		case ESignalling:
		{
			if( _pEvents )
				_pEvents->onSignalling( this, pMsg->_header._ch, boost::asio::const_buffer(pMsg->_data, pMsg->_header._size) );
			break;
		}
		case ESetupData:
		{
			if( _pEvents )
				_pEvents->onSetupData( this, pMsg->_dataSetupMsg.idx, pMsg->_dataSetupMsg.port, string( pMsg->_dataSetupMsg.callid, pMsg->_header._size - sizeof(unsigned short) - sizeof(unsigned short) ) );
			break;
		}
		case EData:
		{
			if( _pEvents )
				_pEvents->onReceiveData( this, pMsg->_header._ch, boost::asio::const_buffer( pMsg->_data, pMsg->_header._size ) );
		}
		default:
			return;
		}
	}
}

void VPNLinkBase::timer_tick( const boost::system::error_code& error )
{
	if( error == boost::asio::error::operation_aborted )
		return;

	if( _activityTimer )
		--_activityTimer;

	if( _mode == eServer )
	{
		VPNClientMap_t::iterator it = _VpnLinkMap.begin();
		while( it != _VpnLinkMap.end() )
		{
			if( !it->second->isActive() )
			{
#ifdef _DEBUG
				log_debug() << "removing client object" << endl;
#endif//_DEBUG
				VPNClientMap_t::iterator it1 = it++;
				_VpnLinkMap.erase( it1 );
				continue;
			}

			++it;
		}
	}
	else if( _mode == eClient )
	{
		if( _bConnected )
		{
			if( _activityTimer )
				Ping();
			else
				_bConnected = false;
		}
		else
			Connect();
	}
	else //_mode == eAccepted
	{
		if( !_activityTimer )
		{
			_bConnected = false;
			//return here to avoid timer's re-queue
			return;
		}
	}

	//reschedule timer
	_timer.expires_at( _timer.expires_at() + boost::posix_time::seconds(5) );
	_timer.async_wait( boost::bind( &VPNLinkBase::timer_tick, this, boost::asio::placeholders::error ) );
}

void VPNLinkBase::SetupDataConn( string callid, unsigned short idx, unsigned short port )
{
	_mess._header._msgType = ESetupData;
	_mess._header._ch = 0xffffffff;
	_mess._header._size = sizeof(idx) + sizeof(port) + callid.length();
	_mess._dataSetupMsg.idx = idx;
	_mess._dataSetupMsg.port = port;
	memcpy(_mess._dataSetupMsg.callid, callid.c_str(), callid.length() );
	send( (const char*)&_mess, sizeof(SMessage::Header) + _mess._header._size );
	_activityTimer = 0;
}

void VPNLinkBase::sendData( unsigned short port, const boost::asio::const_buffer& buf )
{
	if( !_bConnected )
		return;

	_mess._header._msgType = EData;
	_mess._header._ch = port;
	_mess._header._size = boost::asio::buffer_size(buf);
	memcpy(_mess._data, boost::asio::buffer_cast<const char*>(buf), _mess._header._size );
	send( (const char*)&_mess, sizeof(SMessage::Header) + _mess._header._size );
}

/// @param vpnId is the identifier of the vpn link
bool VPNLinkBase::sendSignalling( int ch, const boost::asio::const_buffer& buf, const string& vpnId )
{
	_mess._header._msgType = ESignalling;
	_mess._header._ch = ch;
	_mess._header._size = boost::asio::buffer_size(buf);
	memcpy(_mess._data, boost::asio::buffer_cast<const char*>(buf), _mess._header._size );

	//TODO: this has to be made available for sendData and link to be gathered once and used then...
	if( _mode == eServer && !vpnId.empty() )
	{
		//TODO: put here a map of links based on id
		//find client of vpn server to send on
		VPNClientMap_t::iterator it = _VpnLinkMap.begin();
		while( it != _VpnLinkMap.end() )
		{
			if( it->second && it->second->getId() == vpnId )
			{
				it->second->send( (const char*)&_mess, sizeof(SMessage::Header) + _mess._header._size );
				return true;
			}
			++it;
		}
		return false;
	}
	else if( !_bConnected )
		return false;

	send( (const char*)&_mess, sizeof(SMessage::Header) + _mess._header._size );
	return true;
}

/// this function has to be called after link level is up
void VPNLinkBase::onLinkReady()
{
	if( _mode == eAccepted )
		_activityTimer = ACTIVITY_TIMER_ACCEPTED_DEFAULT;
	else if( _mode == eClient )
	{
		_activityTimer = ACTIVITY_TIMER_CLIENT_DEFAULT;
		Connect();
	}

	//reschedule timer
	_timer.expires_at( _timer.expires_at() + boost::posix_time::seconds(5) );
	_timer.async_wait( boost::bind( &VPNLinkBase::timer_tick, this, boost::asio::placeholders::error ) );
}

/// this function is called when the link is going down
void VPNLinkBase::onLinkDown()
{
	_activityTimer = 0;
	_timer.cancel();
}

void VPNLinkBase::generateId( const string& s )
{
	boost::hash<std::string> string_hash;
	_id = boost::lexical_cast<string>( string_hash( s ) );
}
