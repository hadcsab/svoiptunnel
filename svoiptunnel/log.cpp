#include "common.h"
#include "log.h"

Log* Log::_pLog = NULL;

Log& log_info()
{
	Log& log = Log::instance();
	log << boost::posix_time::to_simple_string( boost::posix_time::second_clock::local_time() ) << 
		" INFO ";
	return log;
}

Log& log_debug()
{
	Log& log = Log::instance();
	log << boost::posix_time::to_simple_string( boost::posix_time::second_clock::local_time() ) << 
		" DEBUG ";
	return log;
}

Log& log_warning()
{
	Log& log = Log::instance();
	log << boost::posix_time::to_simple_string( boost::posix_time::second_clock::local_time() ) << 
		" WARN ";
	return log;
}

Log& log_error()
{
	Log& log = Log::instance();
	log << boost::posix_time::to_simple_string( boost::posix_time::second_clock::local_time() ) << 
		" ERR ";
	return log;
}

Log::Log():
	_pstream( &std::cout )
{
}

Log::Log( ostream* pstream ):
	_pstream( pstream )
{
}

Log::~Log()
{
}

Log& Log::instance()
{
	if( !_pLog )
		_pLog = new Log();

	return *_pLog;
}

void Log::destroy()
{
	if( _pLog )
		delete _pLog;
}

Log& Log::operator<<( const std::string& str )
{
	_pstream->write( str.c_str(), str.length() );
	return *this;
}

Log& Log::operator << ( ostream& (*fn)(ostream&) )
{
	fn( *_pstream );
	return *this;
}

Log& Log::operator << ( unsigned i )
{
	*_pstream << i;
	return *this;
}

Log& Log::operator << ( int i )
{
	*_pstream << i;
	return *this;
}

LogFile::LogFile( const string& fileName ):
	Log( &_stream ),
	_fileName( fileName )
{
	Open();
}

LogFile::~LogFile()
{
	Close();
}

Log& LogFile::instance( const string& fileName )
{
	if( !_pLog )
		_pLog = new LogFile( fileName );

	return *_pLog;
}

bool LogFile::Open()
{
	_stream.open( _fileName.c_str(), ios::out | std::ios_base::binary );
	return _stream.good();
}

void LogFile::Close()
{
	_stream.close();
}
