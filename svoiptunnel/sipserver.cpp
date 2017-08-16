#include "common.h"
#include "util.h"
#include "config.h"
#include "sipserver.h"

sipserver::sipserver( boost::asio::io_service& io_service, unsigned short nListenPort ):
	_io_service( io_service ),
	_timer( io_service, boost::posix_time::seconds(30) )	//we want to trigger the first occurance right away
{
	_socket = boost::shared_ptr<udpsocket>( new udpsocket( io_service, (UDPSocketEvents*)this ) );
	_socket->listen( nListenPort );

	_timer.async_wait( boost::bind( &sipserver::timer_tick, this, boost::asio::placeholders::error ) );
}

sipserver::~sipserver()
{
	_socket->close();
}

//receive from signalling channel
void sipserver::onAccept( boost::shared_ptr<udpsocket> pClient, const boost::asio::const_buffer& buf )
{
	boost::asio::ip::detail::endpoint remote_endpoint( pClient->remote_endpoint().address(), pClient->remote_endpoint().port() );

	//identify sipendpoint or create new
	unsigned ch = 0;
	string sEndpoint = sipendpoint::getId( remote_endpoint.address().to_string(), remote_endpoint.port() );
	AddrMap_t::iterator addrItr = _addrMap.find( sEndpoint );
	if( addrItr == _addrMap.end() )
	{
		for( ch = 0; ch < _clientVec.size(); ++ch )
		{
			if( NULL == _clientVec[ch] )
				break;
		}

		//new unregistered remote endpoint
		_addrMap.insert( make_pair( sEndpoint, ch ) );

		boost::shared_ptr<sipendpoint> pSipEp( new sipendpoint( pClient, this, ch, remote_endpoint.address().to_string(), remote_endpoint.port() ) );
		if( ch < _clientVec.size() )
			_clientVec[ch] = pSipEp;
		else
			_clientVec.push_back( pSipEp );
	}
	else
		ch = addrItr->second;

	_clientVec[ch]->onReceive( remote_endpoint, buf );
}

//VPNLinkEvents
//receive from vpn channel
void sipserver::onConnect( const string& address, unsigned short port )
{
	//TODO
}

//VPNLinkEvents
//receive from vpn channel
void sipserver::onDisconnect( const string& address, unsigned short port )
{
	//TODO
}

//VPNLinkEvents
//receive from vpn channel
/// @param vpnLinkPtr pointer to the VPN link receiving this message
void sipserver::onSignalling( VPNLinkBase* vpnLinkPtr, unsigned ch, const boost::asio::const_buffer& buf )
{
#ifdef _DEBUG
	const char* pSrc = boost::asio::buffer_cast<const char*>(buf);
	size_t len = boost::asio::buffer_size(buf);
	string s(pSrc, len);
	log_info() << "received on VPN" << endl << s << endl;
#endif//_DEBUG

	//the SIP endpoint
	processOutgoing( vpnLinkPtr, ch, buf );
}

//VPNLinkEvents
/// @brief called when data received from vpn side
/// @param vpnLinkPtr pointer to the VPN link receiving this message
/// @param port Other side's local port number
/// @param buf data buffer to send
void sipserver::onReceiveData( VPNLinkBase* vpnLinkPtr, unsigned short port, const boost::asio::const_buffer& buf )
{
	DataPortMap_t::iterator it = _dataPortMap.find( port );
	if( it != _dataPortMap.end() )
		it->second->send( buf );
}

//VPNLinkEvents
/// @brief setup data link with given attributes, received from vpn side
/// @param vpnLinkPtr pointer to the VPN link receiving this message
/// @param idx index of the data port for this call
/// @param vpnPort 
/// @param callid call identifier
void sipserver::onSetupData( VPNLinkBase* vpnLinkPtr, unsigned short idx, unsigned short port, const string& callid )
{
	boost::shared_ptr<sipcall> pCall;

	CallMap_t::iterator itCall = _callMap.find( callid );
	if( itCall == _callMap.end() )
		pCall = _callMap.insert( make_pair( callid, boost::shared_ptr<sipcall>( new sipcall( _io_service, this, vpnLinkPtr, callid ) ) ) ).first->second;
	else
		pCall = itCall->second;

	boost::shared_ptr<sipdataconn> dataConn = pCall->getDataLink( idx );
	//assign port number to data port object identified by callid and idx
	dataConn->linkVpnPort( port );

	_dataPortMap[dataConn->localPort()] = dataConn;
}

//process incomming message
//parse and store the addresses we need to send replies to
void sipserver::processIncoming( sipendpoint* pSipEp, unsigned int ch, const boost::asio::const_buffer& buf )
{
	sipmessage msg;
	if( !msg.parse( buf ) )
		return;

	if( !msg.isRequest() && !msg.isSuccess() )
	{
		//	1xx: Provisional -- request received, continuing to process the
		//		request;
		//
		//	2xx: Success -- the action was successfully received, understood,
		//		and accepted;
		//
		//	3xx: Redirection -- further action needs to be taken in order to
		//		complete the request;
		//
		//	4xx: Client Error -- the request contains bad syntax or cannot be
		//		fulfilled at this server;
		//
		//	5xx: Server Error -- the server failed to fulfill an apparently
		//		valid request;
		//
		//	6xx: Global Failure -- the request cannot be fulfilled at any
		//		server.

		//drop Trying messages received
		if( msg.compareCode( "100" ) )
			return;

		//TODO: handle failed call requests and close open ports for the outgoing call
	}

	boost::asio::const_buffer startline;
	boost::asio::const_buffer body;
	msg.getStartLine( startline );
	msg.getBody( body );

	bool bViaFound = false;
	string endpointId;

	string sTransMsg( boost::asio::buffer_cast<const char*>( startline ), boost::asio::buffer_size( startline ) );
	const char* pSrc = boost::asio::buffer_cast<const char*>( body );
	size_t len = boost::asio::buffer_size( body );
	string contentType;
	unsigned int contentLength = 0;

	string callId;
	unsigned int cseq = 0;
	string cseqRef;

	//loop through the SIP header lines
	const char* pLine = pSrc;
	while( len > 1 )
	{
		if( *pSrc == '\r' && *(pSrc+1) == '\n' )
		{
			//we have a complete line in pLine
			unsigned lineLen = pSrc - pLine;
			string sLine( pLine, lineLen );

			//empty line means end of headers
			if( 0 == lineLen )
			{
				++pSrc;
				--len;
				++pSrc;
				--len;
				break;
			}

			if( msg.isRequest() && 
				lineLen >= sipmessage::_sMsgTo.length() && !sipmessage::_sMsgTo.compare( 0, string::npos, pLine, sipmessage::_sMsgTo.length() ) )
			{
				//To: <sip:testivr3@192.168.1.103>
				vector<string> values;
				values.resize(1);
				boost::dynamic_bitset<> b(1);
				b.set();
				if( reg_grep( "^" TO "\\s<(\\S+)>.*", b, sLine, values) )
				{
					string sTo = values.at(0);
					boost::split( values, sTo, boost::is_any_of("@") );
					if( values.size() == 2 && values.at(1) == Config::instance().getLocalIPAddress() )
					{
						sTo = values[0];
						sTransMsg.append( TO ).append(" <").append(sTo).append("@>\r\n");
					}
					else
					{
						//append to the ouptut message
						sTransMsg.append( pLine, lineLen + 2 );
					}
				}
			}
			else if( msg.isRequest() && 
				lineLen >= sipmessage::_sMsgMaxForwards.length() && !sipmessage::_sMsgMaxForwards.compare( 0, string::npos, pLine, sipmessage::_sMsgMaxForwards.length() ) )
			{
				//	Max-Forwards: has to be decreased
				vector<string> values;
				values.resize(1);
				boost::dynamic_bitset<> b( 1 );
				b.set();
				if( reg_grep("^Max-Forwards:\\s+(\\S+).*", b, sLine, values) )
				{
					int fw = boost::lexical_cast<int>( values[0] );
					if( fw )
						sTransMsg.append( sipmessage::_sMsgMaxForwards ).append( " " ).append( boost::lexical_cast<string>( --fw ) ).append("\r\n");
					else
					{
						//send error reply
						string errMsg("SIP/2.0 483 Too Many Hops\r\n");
						errMsg.append( boost::asio::buffer_cast<const char*>(body), boost::asio::buffer_size(body) );

						pSipEp->send( errMsg );
						return;
					}
				}
			}
			else if( lineLen >= sipmessage::_sContentType.length() && 
				!sipmessage::_sContentType.compare( 0, string::npos, pLine, sipmessage::_sContentType.length() ) )
			{
				vector<string> values;
				values.resize(1);
				boost::dynamic_bitset<> b(1);
				b.set();
				if( reg_grep("^" CONTENTTYPE "\\s+(\\S+)", b, sLine, values) )
					contentType = values[0];
			}
			else if( lineLen >= sipmessage::_sContentLength.length() && 
				!sipmessage::_sContentLength.compare( 0, string::npos, pLine, sipmessage::_sContentLength.length() ) )
			{
				vector<string> values;
				values.resize(1);
				boost::dynamic_bitset<> b(1);
				b.set();
				if( reg_grep("^" CONTENTLENGTH "\\s+(\\d+)", b, sLine, values) )
					contentLength = boost::lexical_cast<unsigned int>( values[0] );
			}
			else if( lineLen >= sipmessage::_sMsgCallId.length() && 
				!sipmessage::_sMsgCallId.compare( 0, string::npos, pLine, sipmessage::_sMsgCallId.length() ) )
			{
				//Call-ID: 14e3248697735878737837k2489rmwp
				vector<string> values;
				values.resize(1);
				boost::dynamic_bitset<> b(1);
				b.set();
				if( reg_grep("^" CALLID "\\s+(\\S+).*", b, sLine, values) )
					callId = values[0];

				//append to the ouptut message
				sTransMsg.append( pLine, lineLen + 2 );
			}
			else if( !msg.isRequest() &&
				!bViaFound && lineLen >= sipmessage::_sMsgVia.length() && !sipmessage::_sMsgVia.compare( 0, string::npos, pLine, sipmessage::_sMsgVia.length() ) )
			{
				// remove first Via row
				//get vpn id from branch
				//Via: SIP/2.0/UDP 192.168.10.1:5070;rport;branch=z9hG4bK94292
				bViaFound = true;

				vector<string> values;
				values.resize(1);
				boost::dynamic_bitset<> b( 1 );
				b.set();
				if( reg_grep("^Via:.*;branch=([^; ]+).*$", b, sLine, values) )
					endpointId = values[0];
			}
			else if( !msg.isRequest() &&
				lineLen >= sipmessage::_sCSeq.length() && !sipmessage::_sCSeq.compare( 0, string::npos, pLine, sipmessage::_sCSeq.length() ) )
			{
				//CSeq: 4711 INVITE
				//check if we have failure on INVITE
				vector<string> values;
				values.resize(2);
				boost::dynamic_bitset<> b( 2 );
				b.set();
				if( reg_grep("^" CSEQ "\\s*(\\d*)\\s*(\\S*)", b, sLine, values) )
				{
					cseq = boost::lexical_cast<unsigned int>( values[0] );
					cseqRef = values[1];
				}

				//append to the ouptut message
				sTransMsg.append( pLine, lineLen + 2 );
			}
			else
			{
				//append to the ouptut message
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

	if( contentLength != len )
		return;

	sipmessage::RequestMethod_t method = msg.getMethod();
	boost::shared_ptr<sipcall> pCall;
	if( !callId.empty() )
	{
		CallMap_t::iterator it = _callMap.find( callId );
		if( method == sipmessage::eCANCEL || method == sipmessage::eBYE || 
			(!msg.isRequest() && !msg.isSuccess() && cseqRef.compare( INVITE )) )
		{
			if( it != _callMap.end() )
				_callMap.erase( it );
			boost::shared_ptr<sipcall> pNULL;
			pCall = pNULL;
		}
		else
		{
			if( it == _callMap.end() )
			{
				if( method == sipmessage::eINVITE )
					pCall = _callMap.insert( make_pair( callId, boost::shared_ptr<sipcall>( new sipcall( _io_service, this, _vpnPtr, callId ) ) ) ).first->second;
			}
			else
				pCall = it->second;
		}
	}

	if( pCall && !contentType.compare( CONTENT_SDP ) )
		contentLength = pCall->processIncomingSDP( pSrc, len );

	// add contenttype and content length here
	if( !contentType.empty() )
		sTransMsg.append( CONTENTTYPE " " ).append( contentType ).append( "\r\n" );
	sTransMsg.append( CONTENTLENGTH " " ).append( boost::lexical_cast<string>( contentLength ) ).append( "\r\n" );
	sTransMsg.append( "\r\n" );

	//append the message body
	sTransMsg.append( pSrc, len );

#ifdef _DEBUG
	log_info() << "sending on VPN " << endl << sTransMsg << endl;
#endif//_DEBUG

	//send the translated message
	boost::asio::const_buffer transbuf( sTransMsg.c_str(), sTransMsg.length() );
	if( msg.isRequest() )
	{
		if( _vpnPtr->sendSignalling( ch, transbuf, "" ) )
		{
			//we have to send back trying asap back to sender
			string tryingMsg("SIP/2.0 100 Trying\r\n");
			tryingMsg.append( boost::asio::buffer_cast<const char*>(body), boost::asio::buffer_size(body) );

			pSipEp->send( tryingMsg );
		}
		else
		{
			//TODO: send failure notification
		}
	}
	else
	{
		//response
		//parse vpnId and ch from endpointId
		//z9hG4bK-<vpnid>-<ch>
		vector<string> strs;
		boost::split( strs, endpointId, boost::is_any_of("-") );
		if( strs.size() == 3 && !strs[0].compare( "z9hG4bK" ) )
		{
			string vpnId = strs[1];
			int chremote = boost::lexical_cast<int>( strs[2] );
			_vpnPtr->sendSignalling( chremote, transbuf, vpnId );
		}
	}
}

void sipserver::processOutgoing( VPNLinkBase* vpnLinkPtr, unsigned int ch, const boost::asio::const_buffer& buf )
{
	//ch is local in response, and it's remote in request
	sipmessage msg;
	if( !msg.parse( buf ) )
		return;

	boost::asio::const_buffer startline;
	boost::asio::const_buffer body;
	msg.getStartLine( startline );
	msg.getBody( body );

	string sTransMsg( boost::asio::buffer_cast<const char*>( startline ), boost::asio::buffer_size( startline ) );
	const char* pSrc = boost::asio::buffer_cast<const char*>( body );
	size_t len = boost::asio::buffer_size( body );
	string contentType;
	unsigned int contentLength = 0;
	string sTo;

	string callId;
	unsigned int cseq = 0;
	string cseqRef;

	const char* pLine = pSrc;

	//identify sipendpoint or create new
	boost::shared_ptr<sipendpoint> pClient;

	while( len > 1 )
	{
		if( *pSrc == '\r' && *(pSrc+1) == '\n' )
		{
			//we have a complete line in pLine
			unsigned lineLen = pSrc - pLine;
			string sLine( pLine, lineLen );

			//empty line means end of headers
			if( 0 == lineLen )
			{
				++pSrc;
				--len;
				++pSrc;
				--len;
				break;
			}

			if( msg.isRequest() && 
				lineLen >= sipmessage::_sMsgTo.length() && !sipmessage::_sMsgTo.compare( 0, string::npos, pLine, sipmessage::_sMsgTo.length() ) )
			{
				//To: <sip:testivr3@192.168.1.103>
				vector<string> values;
				values.resize(1);
				boost::dynamic_bitset<> b(1);
				b.set();
				if( reg_grep( "^" TO "\\s<(\\S+)>.*", b, sLine, values) )
				{
					sTo = values.at(0);
					boost::split( values, sTo, boost::is_any_of("@") );
					if( values.size() == 2 )
					{
						sTo = values.at(1);
						if( sTo.empty() )
							sTo = Config::instance().valueString("defaultsipserver");
						sTransMsg.append( TO ).append(" <").append(values.at(0)).append("@").append(sTo).append(">\r\n");
					}
					else
					{
						//append to the ouptut message
						sTransMsg.append( pLine, lineLen + 2 );
					}
				}
			}
			else if( msg.isRequest() && 
				lineLen >= sipmessage::_sMsgVia.length() && !sipmessage::_sMsgVia.compare( 0, string::npos, pLine, sipmessage::_sMsgVia.length() ) )
			{
				// Via line have to be added before first Via
				string sAddr = Config::instance().getPublicIPAddress();
				sAddr.append(":").append( boost::lexical_cast<string>( _socket->localPort() ) );

				//branch has to begin with z9hG4bK
				string endpointId = string("z9hG4bK-").append( vpnLinkPtr->getId() ).append("-").append( boost::lexical_cast<string>( ch ) );
				sTransMsg.append( "Via: SIP/2.0/UDP " ).append( sAddr ).append( ";rport;branch=" ).append( endpointId ).append( "\r\n" );
				sTransMsg.append( pLine, lineLen + 2 );
			}
			else if( lineLen >= sipmessage::_sContentType.length() && 
				!sipmessage::_sContentType.compare( 0, string::npos, pLine, sipmessage::_sContentType.length() ) )
			{
				vector<string> values;
				values.resize(1);
				boost::dynamic_bitset<> b(1);
				b.set();
				if( reg_grep("^" CONTENTTYPE "\\s+(\\S+)", b, sLine, values) )
					contentType = values[0];
			}
			else if( lineLen >= sipmessage::_sContentLength.length() && 
				!sipmessage::_sContentLength.compare( 0, string::npos, pLine, sipmessage::_sContentLength.length() ) )
			{
				vector<string> values;
				values.resize(1);
				boost::dynamic_bitset<> b(1);
				b.set();
				if( reg_grep("^" CONTENTLENGTH "\\s+(\\d+)", b, sLine, values) )
					contentLength = boost::lexical_cast<unsigned int>( values[0] );
			}
			else if( lineLen >= sipmessage::_sMsgCallId.length() && 
				!sipmessage::_sMsgCallId.compare( 0, string::npos, pLine, sipmessage::_sMsgCallId.length() ) )
			{
				//Call-ID: 14e3248697735878737837k2489rmwp
				vector<string> values;
				values.resize(1);
				boost::dynamic_bitset<> b(1);
				b.set();
				if( reg_grep("^" CALLID "\\s+(\\S+).*", b, sLine, values) )
					callId = values[0];

				//append to the ouptut message
				sTransMsg.append( pLine, lineLen + 2 );
			}
			else if( !msg.isRequest() &&
				lineLen >= sipmessage::_sCSeq.length() && !sipmessage::_sCSeq.compare( 0, string::npos, pLine, sipmessage::_sCSeq.length() ) )
			{
				//CSeq: 4711 INVITE
				//check if we have failure on INVITE
				vector<string> values;
				values.resize(2);
				boost::dynamic_bitset<> b( 2 );
				b.set();
				if( reg_grep("^" CSEQ "\\s*(\\d*)\\s*(\\S*)", b, sLine, values) )
				{
					cseq = boost::lexical_cast<unsigned int>( values[0] );
					cseqRef = values[1];
				}

				//append to the ouptut message
				sTransMsg.append( pLine, lineLen + 2 );
			}
			else
			{
				//append to the ouptut message
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

	if( contentLength != len )
		return;

	sipmessage::RequestMethod_t method = msg.getMethod();
	boost::shared_ptr<sipcall> pCall;
	if( !callId.empty() )
	{
		CallMap_t::iterator it = _callMap.find( callId );
		if( method == sipmessage::eCANCEL || method == sipmessage::eBYE || 
			(!msg.isRequest() && !msg.isSuccess() && cseqRef.compare( INVITE )) )
		{
			if( it != _callMap.end() )
				_callMap.erase( it );
			boost::shared_ptr<sipcall> pNULL;
			pCall = pNULL;
		}
		else
		{
			if( it == _callMap.end() )
			{
				if( method == sipmessage::eINVITE )
					pCall = _callMap.insert( make_pair( callId, boost::shared_ptr<sipcall>( new sipcall( _io_service, this, vpnLinkPtr, callId ) ) ) ).first->second;
			}
			else
				pCall = it->second;
		}
	}

	string sContent;
	if( pCall && !contentType.compare( CONTENT_SDP ) )
	{
		sContent = pCall->processOutgoingSDP( pSrc, len );
		contentLength = sContent.length();
	}
	else if( len )
		sContent.assign( pSrc, len );

	// add contenttype and content length here
	if( !contentType.empty() )
		sTransMsg.append( CONTENTTYPE " " ).append( contentType ).append( "\r\n" );
	sTransMsg.append( CONTENTLENGTH " " ).append( boost::lexical_cast<string>( contentLength ) ).append( "\r\n" );
	sTransMsg.append( "\r\n" );

	//append content (SDP)
	sTransMsg.append( sContent );

	if( msg.isRequest() )
	{
		unsigned short port = Config::instance().valueInt("defaultsipport");

		string sEndpoint = sipendpoint::getId( sTo, port );
		AddrMap_t::iterator addrItr = _addrMap.find( sEndpoint );
		if( addrItr == _addrMap.end() )
		{
			unsigned chlocal = _clientVec.size();

			if( _clientVec.size() < _addrMap.size() )
			{
				//find first empty slot
				chlocal = 0;
				while( chlocal < _clientVec.size() && _clientVec[chlocal] != NULL )
					++chlocal;
			}

			boost::shared_ptr<udpsocket> pSock( new udpsocket(_io_service) );
			if( pSock->connect( sTo, port ) )
			{
				pClient = boost::shared_ptr<sipendpoint>( new sipendpoint( pSock, this, chlocal, sTo, port ) );
				if( chlocal < _clientVec.size() )
					_clientVec[chlocal] = pClient;
				else
					_clientVec.push_back( pClient );

				//new unregistered remote endpoint
				_addrMap.insert( make_pair( sEndpoint, chlocal ) );
			}
		}
		else
			pClient = _clientVec[addrItr->second];
	}
	else //response
	{
		if( ch >= _clientVec.size() || _clientVec[ch] == NULL )
			return;

		pClient = _clientVec[ch];
	}

	//send the translated message
	pClient->send( sTransMsg );
}

void sipserver::registerDataPort( unsigned short port, boost::shared_ptr<sipdataconn> dataConn )
{
	_dataPortMap[port] = dataConn;
}

void sipserver::unregisterDataPort( unsigned short port )
{
	_dataPortMap.erase( port );
}

void sipserver::closeEndpoint( const string& id )
{
	AddrMap_t::iterator addrItr = _addrMap.find( id );
	if( addrItr != _addrMap.end() )
	{
		boost::shared_ptr<sipendpoint> ep;
		_clientVec[addrItr->second] = ep;
		_addrMap.erase( addrItr );
	}
}

void sipserver::timer_tick( const boost::system::error_code& error )
{
	if( error == boost::asio::error::operation_aborted )
		return;

	for( unsigned idx = 0; idx < _clientVec.size(); ++idx )
	{
		if( _clientVec[idx] )
			_clientVec[idx]->check_if_valid();
	}

	//reschedule timer
	_timer.expires_at( _timer.expires_at() + boost::posix_time::seconds(30) );
	_timer.async_wait( boost::bind( &sipserver::timer_tick, this, boost::asio::placeholders::error ) );
}
