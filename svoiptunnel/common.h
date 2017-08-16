#ifndef _COMMON_H
#define _COMMON_H

#ifdef WIN32
// Including SDKDDKVer.h defines the highest available Windows platform.

// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.

#include <SDKDDKVer.h>
#include <tchar.h>
#endif//WIN32

#include <stdio.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <map>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/date.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/array.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/functional/hash.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/enable_shared_from_this.hpp>

//typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket_t;
//typedef boost::asio::buffered_stream<boost::asio::ip::tcp::socket> tcp_socket_t;

#endif//_COMMON_H
