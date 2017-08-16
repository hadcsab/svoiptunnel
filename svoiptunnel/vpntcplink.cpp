#include "vpntcplink.h"
#include "log.h"

VPNTCPLink::VPNTCPLink( boost::asio::io_service& io_service, const string& addr, unsigned short port ):
	VPNLinkBase( io_service, VPNLinkBase::eClient ),
	_io_service( io_service ),
	_socket( new tcpsocket( io_service ) )
{
	_socket->setEvents( this );
	_socket->connect( addr, port );
}

VPNTCPLink::VPNTCPLink( boost::asio::io_service& io_service, unsigned short nListenPort ):
	VPNLinkBase( io_service, VPNLinkBase::eServer ),
	_io_service( io_service ),
	_socket( new tcpsocket( io_service ) )
{
	_socket->setEvents( this );
	_socket->listen( nListenPort );
}

VPNTCPLink::VPNTCPLink( boost::asio::io_service& io_service, boost::shared_ptr<tcpsocket> accepted ):
	VPNLinkBase( io_service, VPNLinkBase::eAccepted ),
	_io_service( io_service ),
	_socket( accepted )
{
	_socket->setEvents( this );
	_socket->start_receive();

	string s = _socket->remote_endpoint().address().to_string();
	s.append( ":" );
	s.append( boost::lexical_cast<string>( _socket->remote_endpoint().port() ) );
	generateId( s );

	onLinkReady();
}

VPNTCPLink::~VPNTCPLink()
{
}

void VPNTCPLink::onConnected()
{
	onLinkReady();
}

void VPNTCPLink::onReceive( const boost::asio::const_buffer& buf )
{
	const tcp::endpoint& tcpep = _socket->remote_endpoint();
	detail::endpoint ep( tcpep.address(), tcpep.port() );
	VPNLinkBase::onReceive( ep, buf );
}

void VPNTCPLink::onAccept( boost::shared_ptr<tcpsocket> pClient )
{
	boost::shared_ptr<VPNLinkBase> link( new VPNTCPLink( _io_service, pClient ) );

	VPNLinkBase::onAccept( link );
}

void VPNTCPLink::onError( const boost::system::error_code& error )
{
}

void VPNTCPLink::onClose()
{
	onLinkDown();
}

void VPNTCPLink::send( const boost::asio::const_buffer& buf )
{
	_socket->send( buf );
}

void VPNTCPLink::send( const char* buf, std::size_t len )
{
	_socket->send( buf, len );
}
