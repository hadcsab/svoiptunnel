#include "common.h"
#include "log.h"
#include "config.h"

Config* Config::pThis = NULL;

unsigned char Encode( char* szBuffer, int nCount, unsigned char nFirstByte )
{
	szBuffer[0] ^= nFirstByte;
	for( int i = 1; i < nCount; i++ )
		szBuffer[i] ^= szBuffer[i - 1];

	return szBuffer[nCount - 1];
}

unsigned char Decode( char* szBuffer, int nCount, unsigned char nFirstByte )
{
	unsigned char ntemp = szBuffer[0];
	szBuffer[0] ^= nFirstByte;
	nFirstByte = ntemp;
	for( int i = 1; i < nCount; i++ )
	{
		ntemp = szBuffer[i];
		szBuffer[i] ^= nFirstByte;
		nFirstByte = ntemp;
	}

	return nFirstByte;;
}

Config::Config( const string &fileName, const string& verFile )
{
	_verFileName = verFile;

	_fileName = fileName;
	Read();
}

bool Config::Read()
{
	ifstream vFile( _verFileName.c_str() );
	if( vFile.fail() )
		log_error() << "error opening version file " << _verFileName << endl;

	vFile >> _version;
	vFile.close();

	log_info() << "version: " << _version << endl;

	_items.clear();
	ifstream file( _fileName.c_str() );
	if( file.good() )
	{
		while( !file.eof() )
		{
			string line;
			file >> line;

			if( line.empty() || line.at(0) == '#' )	//empty or comment line, just skip it
				continue;

			unsigned int pos = line.find('=');
			if( pos == string::npos )
				continue;

			string key = line.substr(0, pos);
			string value = line.substr(pos + 1);

			_items[key] = value;
		}

		file.close();
		return true;
	}
	else
		log_error() << "error opening config file " << _fileName;

	return false;
}

Config::~Config()
{
}

void Config::initialize( const string &fileName, const string &verFile )
{
	if( pThis == NULL )
		pThis = new Config( fileName, verFile );
}

void Config::release()
{
	if( pThis )
	{
		delete pThis;
		pThis = NULL;
	}
}

string Config::valueString( const string &key, const string& def )
{
	if( !pThis )
		return def;

	SItemMap::const_iterator it = pThis->_items.find( key );
	if( it == pThis->_items.end() )
		return def;

	return it->second;
}

int Config::valueInt( const string &key, int def )
{
	if( !pThis )
		return def;

	SItemMap::const_iterator it = pThis->_items.find( key );
	if( it == pThis->_items.end() )
		return def;

	int v = boost::lexical_cast<int>( it->second.c_str() );
	if( v == 0 && *it->second.c_str() != 0 )
		return def;

	return v;
}

void Config::reload()
{
	Read();
}
