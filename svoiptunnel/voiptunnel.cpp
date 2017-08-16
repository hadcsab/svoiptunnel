#include "common.h"
#include "util.h"
#include "log.h"
#include "config.h"
#include "sipserver.h"
#include "vpnudplink.h"
#include "vpndtlslink.h"
#include "vpntcplink.h"
#include "vpntlslink.h"

//voiptunnel.exe -m s -p 5000 -l 5050
//voiptunnel.exe -m c -l 5060 -h 192.168.1.101 -p 5000
//voiptunnel.exe -m c -l 5061 -h 192.168.1.101 -p 5000

//TODO:
// read options from config file
// command line can have the config filename or it can be set in environment variable
// config file default is voiptunnel.config
//
// support log file rotation

//sip port
//media port map
//session

enum ETunnelMode {
	EMODEINVALID,
	ECLIENT,
	ESERVER
};

enum ETunnelType {
	EINVALIDTUNNEL,
	EUDPTUNNEL,
	EDTLSTUNNEL,
	ETCPTUNNEL,
	ETLSTUNNEL,
	EHTTPTUNNEL,
	EHTTPSTUNNEL,
	EWEBSOCKETSTUNNEL
};

const string g_sConfigFile("voiptunnel.config");
const string g_sVersionFile("version");

int main(int argc, char* argv[])
{
	bool haveLocalIP = false;
	bool havePublicIP = false;

	// local IP address
	set<string> localIPAddrSet;
	string localIPAddr;

	// public IP address
	//TODO: use UPNP + <what's my ip> service
	string publicIPAddr;

	//log filename
	string logFile;

	//command line parsing example:
	//http://chuckaknight.wordpress.com/2013/03/24/boost-command-line-argument-processing/

	try
	{
		string sConfigFile( g_sConfigFile );
		string sVersionFile( g_sVersionFile );
		ETunnelMode mode = EMODEINVALID;
		ETunnelType type = EINVALIDTUNNEL;
		unsigned short nListenPort = 5060;
		string sTunnelServerAddr( "" );
		int nTunnelServerPort = 0;

		for( int arg = 1; arg < argc; ++arg )
		{
			if( !string(argv[arg]).compare( "-f" ) && ++arg < argc )
			{
				sConfigFile.assign( argv[arg] );
			}
			if( !string(argv[arg]).compare( "-v" ) && ++arg < argc )
			{
				sVersionFile.assign( argv[arg] );
			}
			else if( !string(argv[arg]).compare( "-m" ) && ++arg < argc )
			{
				if( !string(argv[arg]).compare( "c" ) )
					mode = ECLIENT;
				else if( !string(argv[arg]).compare( "s" ) )
					mode = ESERVER;
				else
				{
					log_error() << "invalid option value " << argv[arg] << " for -m" << endl;
					return -1;
				}
			}
			else if( !string(argv[arg]).compare( "-l" ) && ++arg < argc )
			{
				try {
					nListenPort = boost::lexical_cast<unsigned short>( argv[arg] );
				}
				catch( const exception& e ) {
					log_error() << "invalid option value " << argv[arg] << " for -l: " << e.what() << endl;
				}
			}
			else if( !string(argv[arg]).compare( "-h" ) && ++arg < argc )
			{
				sTunnelServerAddr.assign( argv[arg] );
			}
			else if( !string(argv[arg]).compare( "-p" ) && ++arg < argc )
			{
				try {
					nTunnelServerPort = boost::lexical_cast<unsigned>( argv[arg] );
				}
				catch( const exception& e ) {
					log_error() << "invalid option value " << argv[arg] << " for -p: " << e.what() << endl;
				}
			}
			else if( !string(argv[arg]).compare( "-localip" ) && ++arg < argc )
			{
				localIPAddr = argv[arg];
				localIPAddrSet.insert(argv[arg]);
				haveLocalIP = true;
			}
			else if( !string(argv[arg]).compare( "-publicip" ) && ++arg < argc )
			{
				havePublicIP = true;
				publicIPAddr = argv[arg];
			}
			else if( !string(argv[arg]).compare( "-logfile" ) && ++arg < argc )
			{
				logFile = argv[arg];
			}
			else
			{
				log_warning() << "unrecognized option " << argv[arg] << endl;
			}
		}

		if( logFile.empty() )
			Log::instance();
		else
			LogFile::instance( logFile );

		Config::instance().initialize( sConfigFile, sVersionFile );

		log_info() << "started" << endl;

		//init SSL with threading
		sslbase::THREAD_setup();
		OpenSSL_add_ssl_algorithms();
		SSL_load_error_strings();

		//io service
		boost::asio::io_service io_service;

		sipserver* sipServerPtr = new sipserver( io_service, nListenPort );
		VPNLinkBase* vpnPtr = NULL;

		if( mode == EMODEINVALID )
		{
			//read from config file
			string sMode = Config::instance().valueString( "mode" );
			if( sMode == "client" )
				mode = ECLIENT;
			else if( sMode == "server" )
				mode = ESERVER;
		}

		if( type == EINVALIDTUNNEL )
		{
			//read from config file
			string sType = Config::instance().valueString( "type" );
			if( sType == "udp" )
				type = EUDPTUNNEL;
			else if( sType == "dtls" )
				type = EDTLSTUNNEL;
			else if( sType == "tls" )
				type = ETCPTUNNEL;
			else if( sType == "tcp" )
				type = ETLSTUNNEL;
			else if( sType == "http" )
				type = EHTTPTUNNEL;
			else if( sType == "https" )
				type = EHTTPSTUNNEL;
			else if( sType == "websockets" )
				type = EWEBSOCKETSTUNNEL;

			if( type == EINVALIDTUNNEL )
			{
				log_error() << "invalid vpn link type: " << sType << endl;
				return -1;
			}

			log_info() << "vpn link type=" << sType << endl;
		}

		if( !haveLocalIP )
		{
			string sIP = Config::instance().valueString( "localip" );
			if( !sIP.empty() )
			{
				haveLocalIP = true;
				localIPAddr = sIP;
				localIPAddrSet.insert( sIP );
			}
			else
			{
				haveLocalIP = getLocalIP( localIPAddrSet );
				if( haveLocalIP )
					localIPAddr = *localIPAddrSet.begin();
			}
		}

		if( !haveLocalIP )
		{
			log_error() << "local IP address not configured and cannot be found" << endl;
			return -1;
		}

		if( !havePublicIP )
		{
			//TODO: locate PUBLIC IP with UPNP
			//TODO: locate PUBLIC IP with "what's my ip" service
			publicIPAddr = Config::instance().valueString( "publicip" );
			havePublicIP = true;
		}

		if( !havePublicIP )
		{
			log_error() << "public IP address cannot be determined" << endl;
		}

		if( !nTunnelServerPort )
			nTunnelServerPort = Config::instance().valueInt("tunnelserverport");
		if( !nTunnelServerPort )
		{
			log_error() << "tunnel port not set" << endl;
			return -1;
		}

		if( mode == ESERVER )
		{
			log_info() << "server mode" << endl;
			log_info() << "VPN listen port " << nTunnelServerPort << endl;
			log_info() << "listening on " << nListenPort << " for incoming SIP connections" << endl;

			switch( type )
			{
			case EUDPTUNNEL:
				vpnPtr = new VPNUDPLink( io_service, nTunnelServerPort );
				break;
			case EDTLSTUNNEL:
				vpnPtr = new VPNDTLSLink( io_service, nTunnelServerPort );
				break;
			case ETCPTUNNEL:
				vpnPtr = new VPNTCPLink( io_service, nTunnelServerPort );
				break;
			case ETLSTUNNEL:
				vpnPtr = new VPNTLSLink( io_service, nTunnelServerPort );
				break;
			default:
				log_error() << "invalid tunnel type set" << endl;
				return -1;
			}
		}
		else if( mode == ECLIENT )
		{
			if( sTunnelServerAddr.empty() )
				sTunnelServerAddr = Config::instance().valueString("tunnelserveraddr");
			if( sTunnelServerAddr.empty() )
			{
				log_error() << "tunnel server address not set" << endl;
				return -1;
			}

			log_info() << "client mode" << endl;
			log_info() << "server address " << sTunnelServerAddr << ":" << nTunnelServerPort << endl;
			log_info() << "listening on " << nListenPort << " for incoming SIP connections" << endl;

			switch( type )
			{
			case EUDPTUNNEL:
				vpnPtr = new VPNUDPLink( io_service, sTunnelServerAddr, nTunnelServerPort );
				break;
			case EDTLSTUNNEL:
				vpnPtr = new VPNDTLSLink( io_service, sTunnelServerAddr, nTunnelServerPort );
				break;
			case ETCPTUNNEL:
				vpnPtr = new VPNTCPLink( io_service, sTunnelServerAddr, nTunnelServerPort );
				break;
			case ETLSTUNNEL:
				vpnPtr = new VPNTLSLink( io_service, sTunnelServerAddr, nTunnelServerPort );
				break;
			default:
				log_error() << "invalid tunnel type set: " << type << endl;
				return -1;
			}
		}
		else
		{
			log_error() << "mode not set" << endl;
			return -1;
		}

		Config::instance().SetAddresses( localIPAddr, publicIPAddr );
		sipServerPtr->SetVPNLink( vpnPtr );

		io_service.run();

		delete sipServerPtr;
		delete vpnPtr;

		//cleanup SSL and threading
		sslbase::THREAD_cleanup();

		log_info() << "terminated" << endl;
	}
	catch (std::exception& e)
	{
		log_error() << "error: " << e.what() << endl;
		return -1;
	}

	return 0;
}
