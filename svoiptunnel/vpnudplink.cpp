#include "vpnudplink.h"
#include "log.h"

VPNUDPLink::VPNUDPLink( boost::asio::io_service& io_service, const string& addr, unsigned short port ):
	VPNLinkBase( io_service, VPNLinkBase::eClient ),
	_io_service( io_service ),
	_socket( new udpsocket( io_service ) )
{
	_socket->setEvents( this );
	_socket->connect( addr, port );
	onLinkReady();
}

VPNUDPLink::VPNUDPLink( boost::asio::io_service& io_service, unsigned short nListenPort ):
	VPNLinkBase( io_service, VPNLinkBase::eServer ),
	_io_service( io_service ),
	_socket( new udpsocket( io_service ) )
{
	_socket->setEvents( this );
	_socket->listen( nListenPort );
}

VPNUDPLink::VPNUDPLink( boost::asio::io_service& io_service, boost::shared_ptr<udpsocket> accepted ):
	VPNLinkBase( io_service, VPNLinkBase::eAccepted ),
	_io_service( io_service ),
	_socket( accepted )
{
	_socket->setEvents( this );

	string s = _socket->remote_endpoint().address().to_string();
	s.append( ":" );
	s.append( boost::lexical_cast<string>(_socket->remote_endpoint().port()) );
	generateId( s );

	onLinkReady();
}

VPNUDPLink::~VPNUDPLink()
{
}

void VPNUDPLink::onReceive( const boost::asio::ip::detail::endpoint& remote_endpoint, const boost::asio::const_buffer& buf )
{
	VPNLinkBase::onReceive( remote_endpoint, buf );
}

void VPNUDPLink::onAccept( boost::shared_ptr<udpsocket> accepted, const boost::asio::const_buffer& buf )
{
	boost::shared_ptr<VPNLinkBase> link( new VPNUDPLink( _io_service, accepted ) );
	VPNLinkBase::onAccept( link );

	//pass the received data to the new socket
	boost::asio::ip::detail::endpoint ep( accepted->remote_endpoint().address(), accepted->remote_endpoint().port() );
	link->onReceive( ep, buf );
}

void VPNUDPLink::onError( const boost::system::error_code& error )
{
}

void VPNUDPLink::send( const boost::asio::const_buffer& buf )
{
	_socket->send( buf );
}

void VPNUDPLink::send( const char* buf, std::size_t len )
{
	_socket->send( buf, len );
}
