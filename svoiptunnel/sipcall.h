#ifndef SIPCALL_H
#define SIPCALL_H

using namespace std;

class sipserver;

class sipcall
{
public:
	sipcall( boost::asio::io_service& io_service, sipserver *sipServer, VPNLinkBase* vpnPtr, string callId );
	~sipcall();

	unsigned int processIncomingSDP( const char* pSrc, size_t len );
	string processOutgoingSDP( const char* pSrc, size_t len );
	void linkDataPort( unsigned short vpnPort, unsigned short idx );
	boost::shared_ptr<sipdataconn> getDataLink( unsigned short idx );

private:
	bool processMLine( const string& line, unsigned short& port );
	bool processCLine( const string& line, string& addr );
	unsigned short addDataPort( const string& addr, unsigned short port, unsigned short idx );
	void delDataPort( unsigned short idx );

private:
	boost::asio::io_service& _io_service;
	VPNLinkBase* _vpnPtr;
	sipserver* _sipServer;

	//callid of the call
	string _callId;

	//map id to their connection objects
	typedef vector<boost::shared_ptr<sipdataconn> > DataConnVec_t;
	DataConnVec_t _dataConnVec;
};

#endif//SIPCALL_H
