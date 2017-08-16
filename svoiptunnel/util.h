#ifndef __UTIL_H
#define __UTIL_H

#include <string>
#include <set>

using namespace std;

bool getLocalIP( set<string>& localIPSet);
bool isLocalIP( const string& ipAddr );

// regex grep
bool reg_grep(const std::string& regx, const boost::dynamic_bitset<>& idxSet, const std::string& text, std::vector<std::string>& output);
// regex sed
std::string reg_replace( const std::string& regx, const std::string& text, const std::string& newtext);

#endif //__UTIL_H
