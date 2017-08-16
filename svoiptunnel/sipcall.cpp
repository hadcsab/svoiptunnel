#include "common.h"
#include "log.h"
#include "util.h"
#include "config.h"
#include "sipmessage.h"
#include "sipdataconn.h"
#include "sipcall.h"
#include "sipdataconn.h"
#include "sipserver.h"

sipcall::sipcall( boost::asio::io_service& io_service, sipserver *sipServer, VPNLinkBase* vpnPtr, string callId ):
	_io_service( io_service ),
	_vpnPtr( vpnPtr ),
	_sipServer( sipServer ),
	_callId( callId )
{
}

sipcall::~sipcall()
{
	for( unsigned short idx = 0; idx < _dataConnVec.size(); ++idx )
		delDataPort( idx );
}

unsigned int sipcall::processIncomingSDP( const char* pSrc, size_t len )
{
	unsigned mediaIdx = 0;
	//search for default c= line
	bool checkforCDefault = true;
	string cline;
	string mline;
	string defaultConnAddr;
	bool haveSDPVer = false;
	const char* pLine = pSrc;
	size_t contentLength = len;

	// loop through the SDP lines
	while( len > 1 )
	{
		if( *pSrc == '\r' && *(pSrc+1) == '\n' )
		{
			//we have a complete line in pLine
			unsigned lineLen = pSrc - pLine;

			//ref. this: http://www.ietf.org/mail-archive/web/sip/current/msg17684.html
			//check for v=0 as the first line
			if( !haveSDPVer && sipmessage::_sVLine.compare( 0, string::npos, pLine, sipmessage::_sVLine.length() ) )
			{
				//first line is not v=0, we skip processing all the other lines
				pSrc = pLine;
				break;
			}
			else
				haveSDPVer = true;

			if( lineLen >= sipmessage::_sCLine.length() && !sipmessage::_sCLine.compare( 0, string::npos, pLine, sipmessage::_sCLine.length() ) )
			{
				if( checkforCDefault )
					checkforCDefault = !processCLine(string(pLine, lineLen), defaultConnAddr);
				else
				{
					//just store address
					cline.assign( pLine, lineLen );
				}
			}
			else if( lineLen >= sipmessage::_sMLine.length() && !sipmessage::_sMLine.compare( 0, string::npos, pLine, sipmessage::_sMLine.length() ) )
			{
				if( !mline.empty() )
				{
					string addr = defaultConnAddr;
					unsigned short port;
					if( processMLine( mline, port ) &&
						(cline.empty() || processCLine( cline, addr )) )
					{
						//if we have a m line then we won't have further c lines for default, just for conn specific entries
						checkforCDefault = false;
						addDataPort( addr, port, mediaIdx++ );
					}

					cline.clear();
				}

				mline.assign( pLine, lineLen );
			}

			++pSrc;
			--len;
			pLine = ++pSrc;
			--len;
		}
		else
		{
			++pSrc;
			--len;
		}
	}

	if( !mline.empty() )
	{
		string addr = defaultConnAddr;
		unsigned short port;
		if( processMLine( mline, port ) &&
			(cline.empty() || processCLine( cline, addr )) )
		{
			//if we have a m line then we won't have further c lines for default, just for conn specific entries
			addDataPort( addr, port, mediaIdx++ );
		}
	}

	return contentLength;
}

string sipcall::processOutgoingSDP( const char* pSrc, size_t len )
{
	const char* pLine = pSrc;
	unsigned mediaIdx = 0;
	string sTransMsg;

	// loop through the SDP lines
	while( len > 1 )
	{
		if( *pSrc == '\r' && *(pSrc+1) == '\n' )
		{
			//we have a complete line in pLine
			unsigned lineLen = pSrc - pLine;

			//replace address in c line
			if( lineLen >= sipmessage::_sCLine.length() && !sipmessage::_sCLine.compare( 0, string::npos, pLine, sipmessage::_sCLine.length() ) )
			{
				// c=IN IP4 84.0.11.151
				sTransMsg.append( "c=IN IP4 " ).append( Config::instance().getPublicIPAddress() ).append("\r\n");
				//sTransMsg.append( reg_replace("^(c=\\s+\\S)(\\s+)", string(pLine, lineLen), string("\\1").append( Config::instance().getPublicIPAddress() ) ) );
			}
			else if( lineLen >= sipmessage::_sMLine.length() && !sipmessage::_sMLine.compare( 0, string::npos, pLine, sipmessage::_sMLine.length() ) )
			{
				//we have to open data link for the outgoing data communication and send the port number in the m line

				unsigned short port = addDataPort( "", 0, mediaIdx++ );
				sTransMsg.append( reg_replace("^(m=\\S*\\s)(\\d*)(\\s.*)", string(pLine, lineLen), string("\\1").append( boost::lexical_cast<string>(port) ).append("\\3").append("\r\n") ) );
			}
			else
			{
				sTransMsg.append( pLine, lineLen + 2 );
			}

			++pSrc;
			--len;
			pLine = ++pSrc;
			--len;
		}
		else
		{
			++pSrc;
			--len;
		}
	}

	return sTransMsg;
}

boost::shared_ptr<sipdataconn> sipcall::getDataLink( unsigned short idx )
{
	if( _dataConnVec.size() <= idx )
	{
		_dataConnVec.resize( idx + 1 );
		boost::shared_ptr<sipdataconn> dataConn( new sipdataconn( _io_service, _vpnPtr ) );
		_dataConnVec[idx] = dataConn;
	}

	return _dataConnVec[idx];
}

unsigned short sipcall::addDataPort( const string& addr, unsigned short port, unsigned short idx )
{
	if( _dataConnVec.size() <= idx )
	{
		_dataConnVec.resize( idx + 1 );
		boost::shared_ptr<sipdataconn> dataConn( new sipdataconn( _io_service, _vpnPtr ) );
		_dataConnVec[idx] = dataConn;
	}

	//in this case we just need to reserve a UDP port and no need to connect it yet, as we don't yet know the other side's port number
	unsigned short localPort = _dataConnVec[idx]->localPort();
	if( port )
		_dataConnVec[idx]->connect( addr, port );

	// send setupData command on VPN
	_vpnPtr->SetupDataConn( _callId, idx, localPort );
	_sipServer->registerDataPort( localPort, _dataConnVec[idx] );

	return localPort;
}

//link port information to the dataport at index idx
void sipcall::linkDataPort( unsigned short vpnPort, unsigned short idx )
{
	if( _dataConnVec.size() <= idx )
	{
		_dataConnVec.resize( idx + 1 );
		boost::shared_ptr<sipdataconn> dataConn( new sipdataconn( _io_service, _vpnPtr ) );
		_dataConnVec[idx] = dataConn;
	}

	_dataConnVec[idx]->linkVpnPort( vpnPort );
}

void sipcall::delDataPort( unsigned short idx )
{
	if( idx >= _dataConnVec.size() )
		return;

	_sipServer->unregisterDataPort( _dataConnVec[idx]->localPort() );

	boost::shared_ptr<sipdataconn> pNULL;
	_dataConnVec[idx]->close();
	_dataConnVec[idx] = pNULL;
}

/// @desc process m line in SDP content
/// @param pLine text line to be processed
/// @param port output parameter to receive the port number parsed
bool sipcall::processMLine( const string& line, unsigned short& port )
{
	// m=audio 8000 RTP/AVP 0 8 96 3 13 101
	vector<string> values;
	values.resize(2);
	boost::dynamic_bitset<> b( 2 );
	b.set();
	if( reg_grep("^m=\\S+\\s+(\\d+)\\s+(.*)", b, line, values) )
	{
		//should create unconnected UDP here until response m-line arrives
		port = boost::lexical_cast<unsigned short>( values[0] );
		return true;
	}
	return false;
}

/// @desc process c line in SDP content
/// @param pLine text line to be processed
/// @param addr output parameter to receive the address parsed
bool sipcall::processCLine( const string& line, string& addr )
{
	// c=IN IP4 84.0.11.151
	vector<string> values;
	values.resize(2);
	boost::dynamic_bitset<> b(2);
	b.set();

	/*
	c=IN IP4 192.168.1.100
	t=0 0
	m=audio 20838 RTP/AVP 0 104 97 101
	a=rtpmap:0 PCMU/8000
	a=rtpmap:97 iLBC/8000
	a=fmtp:97 mode=30
	a=rtpmap:104 speex/8000
	a=fmtp:104 mode=3;mode=any
	a=rtpmap:101 telephone-event/8000
	a=fmtp:101 0-16
	a=sendrecv
	*/

	if( reg_grep("^c=IN\\s+(\\S+)\\s+(\\S+)", b, line, values ) )
	{
		//values[0] can be IP4 and IP6
		addr = values[1];
		return true;
	}
	return false;
}
