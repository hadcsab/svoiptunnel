#ifndef __LOG_H_
#define __LOG_H_

using namespace std;

class Log
{
protected:
	Log();
	Log( ostream* pstream );
	virtual ~Log();

public:
	static Log& instance();
	static void destroy();

	virtual Log& operator << ( const std::string& str );
	virtual Log& operator << ( ostream& (*fn)(ostream&) );
	virtual Log& operator << ( unsigned i );
	virtual Log& operator << ( int i );

protected:
	static Log* _pLog;
	ostream* _pstream;
};

Log& log_info();
Log& log_debug();
Log& log_error();
Log& log_warning();

class LogFile: public Log
{
protected:
	LogFile( const string& fileName );
	virtual ~LogFile();
	bool Open();
	void Close();

public:
	static Log& instance( const string& fileName );

private:
	string _fileName;
	ofstream _stream;
};

#endif//__LOG_H_
