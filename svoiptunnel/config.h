#ifndef CONFIG_H
#define CONFIG_H

#include <map>
#include <string>

#define MAX_CONFIG_LINELEN 256

using namespace std;

class Config
{
private:
	Config( const string& fileName, const string& verFile );
	virtual ~Config();
	bool Read();
	void reload();

public:
	static void initialize( const string& fileName, const string& verFile );
	static Config& instance() { return *pThis; }
	static void release();
	static string valueString( const string& key, const string& def = "" );
	static int valueInt( const string& key, int def = 0 );
	static const string& version() {return pThis->_version;}

	//global configuration values
	const string& getLocalIPAddress() const { return _localIPAddr; }
	const string& getPublicIPAddress() const { return _publicIPAddr; }
	void SetAddresses( const string& localIPAddr, const string& publicIPAddr )
	{
		_localIPAddr = localIPAddr;
		_publicIPAddr = publicIPAddr;
	}

private:
	static Config* pThis;
	string _fileName;
	string _verFileName;
	typedef map<string, string> SItemMap;
	SItemMap _items;
	string _version;

	//local and public IP addresses
	string _localIPAddr;
	string _publicIPAddr;
};

#endif // CONFIG_H
