#include "common.h"
#include "sipmessage.h"

//start line
const char* sipmessage::_methodArray[eUNKNOWN] = { REGISTER, INVITE, ACK, CANCEL, BYE, OPTIONS };

//SIP header
const string sipmessage::_sMsgRegister( REGISTER );
const string sipmessage::_sMsgTo( TO );
const string sipmessage::_sMsgVia( VIA );
const string sipmessage::_sMsgContact( CONTACT );
const string sipmessage::_sMsgFrom( FROM );
const string sipmessage::_sMsgCallId( CALLID );
const string sipmessage::_sMsgMaxForwards( MAXFORWARDS );
const string sipmessage::_sContentType( CONTENTTYPE );
const string sipmessage::_sContentLength( CONTENTLENGTH );
const string sipmessage::_sCSeq( CSEQ );

//SDP lines
const string sipmessage::_sVLine( VLINE );
const string sipmessage::_sMLine( MLINE );
const string sipmessage::_sCLine( CLINE );

sipmessage::sipmessage():
	_isValid(false),
	_isRequest(false),
	_isSuccess(false)
{
	memset( _respCode, '0', sizeof(_respCode) );
}

sipmessage::~sipmessage()
{
}

bool sipmessage::parse( const boost::asio::const_buffer& buf )
{
	const char* pSrc = boost::asio::buffer_cast<const char*>(buf);
	size_t len = boost::asio::buffer_size(buf);
	const char* pLine = pSrc;

	int method = 0;

	//process first line
	int chrIdx = 0;
	while( len && *pSrc != '\n' )
	{
		//stop if we found the method
		if( !_methodArray[method][chrIdx] )
			break;

		while( method < eUNKNOWN && *pSrc != _methodArray[method][chrIdx] )
		{
			++method;
			chrIdx = 0;
		}

		if( method == eUNKNOWN )
			break;

		++chrIdx;
		++pSrc;
		--len;
	}

	if( method != eUNKNOWN )
	{
		while( len && *pSrc != '\n' )
		{
			++pSrc;
			--len;
		}
		_isRequest = true;
		_isValid = true;
	}
	else
	{
		while( len && *pSrc != ' ' )
		{
			++pSrc;
			--len;
		}
		if( len )
		{
			++pSrc;
			--len;
		}

		int codeLen = 3;
		while( codeLen-- && len && *pSrc != ' ' )
		{
			if( *pSrc >= '0' && *pSrc <= '9' )
			{
				_respCode[2-codeLen] = *pSrc++;
				--len;
				continue;
			}
			return false;
		}

		_isSuccess = (_respCode[0] == '2');

		if( *pSrc != ' ' )
		{
			//message not having 3 digits as it's second argument
			return false;
		}

		while( len && *pSrc != '\n' )
		{
			++pSrc;
			--len;
		}

		_isRequest = false;
		_isValid = true;
	}

	if( len )
	{
		++pSrc;		//to skip the \r (LF)
		--len;
	}

	//remember the start line
	_startline = boost::asio::const_buffer(pLine, pSrc - pLine);

	//remember where the message body starts
	_body = boost::asio::const_buffer(pSrc, len);
	_rqMethod = (RequestMethod_t)method;

	return _isValid;
}

bool sipmessage::compareCode( const char* code )
{
	return memcmp(code, _respCode, sizeof(_respCode)) == 0;
}

/*
	//From: <sip:535237551@192.168.1.103>;tag=12g1207009703898125983m
	sLine = reg_replace( "^(From:\\s+<\\S+:\\S@)([^>]+)(>.*)", sLine, string("\\1").append( sServer ).append("\\3") );
*/
