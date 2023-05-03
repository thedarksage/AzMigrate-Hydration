#ifndef TRANSPORT_H
#define TRANSPORT_H
#include <vector>
#include <string>
#include "filesystemobj.h"
#include <curl/curl.h>

struct httpStreamObj
{
	httpStreamObj(ACE_HANDLE hd=ACE_INVALID_HANDLE, const std::string& fileName="") ;
	ACE_HANDLE m_hd ;
	std::string m_fileName ;
} ;

struct httpInfo
{
	CURLcode m_code ;
	std::string m_url ;
	std::string m_encodedUrl ;
	std::string m_effectiveUrl ;
	long m_responseCode ;
	double m_totalTime ;
	double m_totalBytes ;
	double m_speed ;
public:
	httpInfo(CURLcode code=CURLE_OK) ;
	//setters
	void setPerformCode(CURLcode code)  ;
	void setResponseCode(int code) ;
	void setUrl(const std::string& url) ;
	void setEncodedUrl(const std::string& url) ;
	void setEffectiveUrl(const std::string& url) ;
	void setTotalTime(double time) ;
	void setTotalBytes(double bytes) ;
	void setSpeed(double speed) ;
	//getters
	CURLcode getPerformCode()  ;
	int getResponseCode() ;
	std::string getUrl() ;
	std::string getEncodedUrl() ;
	std::string getEffectiveUrl() ;
	double getTotalTime() ;
	double getTotalBytes() ;
	double getSpeed() ;
	void dumpInfo() ;
} ;

class transport
{
public:
	virtual bool uploadFile(const std::string& localName, const std::string& remoteName ) = 0 ;
	virtual bool uploadFile(const fileSystemObj& fileObj, const std::string& remoteName) = 0 ;
	virtual bool uploadDir(const std::string& localName, const std::string& remoteName ) = 0 ;
	virtual bool uploadDir(const fileSystemObj& fileObj, const std::string& remoteName) = 0 ;
	virtual bool deleteFile(const std::string& remotename) = 0 ;
	virtual bool deleteDir(const std::string& remotename) = 0 ;
	virtual bool fileExists(const std::string& remoteName) = 0 ;


	static bool m_transportInited ;
} ;

class httpTransport : public transport
{
	httpInfo m_httpInfo ;
	void encodeUrl(const std::string& url, std::string& encodedUrl) ;
	void SetTimeouts(CURL *curl) ;
public:
	httpTransport() ;
	bool uploadFile(const std::string& localName, const std::string& remoteName ) ;
	bool uploadFile(const fileSystemObj& fileObj, const std::string& remoteName) ;
	bool uploadDir(const std::string& localName, const std::string& remoteName )  ;
	bool uploadDir(const fileSystemObj& fileObj, const std::string& remoteName) ;
	bool deleteFile(const std::string& remotename) ;
	bool deleteDir(const std::string& remotename) ;
	bool fileExists(const std::string& remoteName) ;
	httpInfo getHttpInfo() { return m_httpInfo ; }
	static bool initTransport() ;
	static bool cleanupTransport() ;

} ;

class nfsTransport: public transport 
{
public:
	bool uploadFile(const std::string& localName, const std::string& remoteName ) ;
	bool uploadFile(const fileSystemObj& fileObj, const std::string& remoteName) ;
	bool uploadDir(const std::string& localName, const std::string& remoteName ) ;
	bool uploadDir(const fileSystemObj& fileObj, const std::string& remoteName) ;
	bool deleteFile(const std::string& remotename) ;
	bool deleteDir(const std::string& remotename) ;
	bool fileExists(const std::string& remoteName) ;
	static bool initTransport() ;
	static bool cleanupTransport() ;
} ;

class cifsTransport : public transport 
{
public:
	bool uploadFile(const std::string& localName, const std::string& remoteName ) ;
	bool uploadFile(const fileSystemObj& fileObj, const std::string& remoteName) = 0 ;
	bool uploadDir(const std::string& localName, const std::string& remoteName ) ;
	bool uploadDir(const fileSystemObj& fileObj, const std::string& remoteName) ;
	bool deleteFile(const std::string& remotename) ;
	bool deleteDir(const std::string& remotename) ;
	bool fileExists(const std::string& remoteName) ;
	static bool initTransport() ;
	static bool cleanupTransport() ;
} ;
#endif

