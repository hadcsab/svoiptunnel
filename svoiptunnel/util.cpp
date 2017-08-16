#include "common.h"
#include <iostream>

#include "util.h"
#include "log.h"

bool getLocalIP( set<string>& localIPSet )
{
	char ac[80];
#ifdef WIN32
	if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR) {
		log_error() << "Error " << WSAGetLastError() << " when getting local host name." << endl;
#else
	if (gethostname(ac, sizeof(ac)) == -1) {
		log_error() << "Error " << errno << " when getting local host name." << endl;
#endif
		return false;
	}
	log_info() << "Host name is " << ac << "." << endl;

	struct hostent *phe = gethostbyname(ac);
	if (phe == 0) {
		log_error() << "Yow! Bad host lookup." << endl;
		return false;
	}

	for (int i = 0; phe->h_addr_list[i] != 0; ++i) {
		struct in_addr addr;
		memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
		log_info() << "getLocalIP.Address " << i << ": " << inet_ntoa(addr) << endl;
		localIPSet.insert( inet_ntoa(addr) );
	}

	return true;
}

bool isLocalIP( const string& ipAddr )
{
	//TODO
	return true;
}

bool reg_grep( const std::string& regx, const boost::dynamic_bitset<>& idxSet, const std::string& text, std::vector<std::string>& output )
{
	boost::regex e(regx);
	boost::smatch what;
	if( boost::regex_match( text, what, e, boost::match_extra ) )
	{
		size_t index = idxSet.find_first();
		unsigned pos = 0;
		while( index != boost::dynamic_bitset<>::npos )
		{
			//index 0 would mean the whole string
			if( index < what.size() && pos < output.size() )
				output[pos] = what[index+1];
			else
				break;

			index = idxSet.find_next( index );
			++pos;
		}
		return true;
	}
	return false;
}
/*
bool reg_grep( const std::string& regx, unsigned idx, const std::string& text, std::string& output )
{
	boost::regex e(regx);
	boost::smatch what;
	if( boost::regex_match( text, what, e, boost::match_extra ) )
	{
		if( idx < what.size() )
		{
			output = what[idx];
			return true;
		}
	}
	return false;
}
*/
std::string reg_replace( const std::string& regx, const std::string& text, const std::string& newtext )
{
	boost::regex e(regx);
	return boost::regex_replace( text, e, newtext );
}
