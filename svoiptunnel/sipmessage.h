#ifndef SIPMESSAGE_H
#define SIPMESSAGE_H

using namespace std;

//Methods
#define INVITE "INVITE"
#define REGISTER "REGISTER"
#define ACK "ACK"
#define CANCEL "CANCEL"
#define BYE "BYE"
#define OPTIONS "OPTIONS"

//SIP headers
#define TO "To:"
#define VIA "Via:"
#define FROM "From:"
#define CALLID "Call-ID:"
#define CONTACT "Contact:"
#define MAXFORWARDS "Max-Forwards:"
#define CONTENTTYPE "Content-Type:"
#define CONTENTLENGTH "Content-Length:"
#define CSEQ "CSeq:"

//content types
#define CONTENT_SDP "application/sdp"

//SDP headers
#define VLINE "v=0"
#define MLINE "m="
#define CLINE "c="

class sipmessage
{
public:
	// start line methods
	enum RequestMethod_t
	{
		eREGISTER,
		eINVITE,
		eACK,
		eCANCEL,
		eBYE,
		eOPTIONS,
		eUNKNOWN
	};

	// header lines
	static const string _sMsgRegister;
	static const string _sMsgTo;
	static const string _sMsgVia;
	static const string _sMsgContact;
	static const string _sMsgFrom;
	static const string _sMsgCallId;
	static const string _sMsgMaxForwards;
	static const string _sContentType;
	static const string _sContentLength;
	static const string _sCSeq;

	// sdp lines
	static const string _sVLine;
	static const string _sMLine;
	static const string _sCLine;

protected:
	static const char* _methodArray[eUNKNOWN];

public:
	sipmessage();
	~sipmessage();

	bool parse( const boost::asio::const_buffer& buf );
	bool getStartLine( boost::asio::const_buffer& buf ) const { if( !_isValid ) return false; buf = _startline; return true; }
	bool getBody( boost::asio::const_buffer& buf ) const { if( !_isValid ) return false; buf = _body; return true; }
	bool isValid() const { return _isValid; }
	bool isRequest() const { return _isRequest; }
	bool isSuccess() const { return _isSuccess; }
	RequestMethod_t getMethod() const { return _rqMethod; }
	const char* getRespCode() const { return _respCode; }
	bool compareCode( const char* code );

private:
	bool _isValid;
	bool _isRequest;
	bool _isSuccess;
	char _respCode[3];

	//pointers of the message
	boost::asio::const_buffer _startline;
	boost::asio::const_buffer _body;

	RequestMethod_t _rqMethod;
};

#endif//SIPMESSAGE_H
