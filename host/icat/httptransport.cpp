#include "transport.h"
#include<assert.h>
#include<ace/OS_NS_sys_stat.h>
#include<ace/OS_NS_fcntl.h>
#include <ace/OS_NS_stdio.h>
#include <ace/OS_NS_string.h>
#include<iostream>
#include "logger.h"
#include "defs.h"
#include "icatexception.h"
#include <boost/shared_array.hpp>
#include <sstream>
#include "configvalueobj.h"
#include "archivecontroller.h"

#define SAFE_CURL_EASY_CLEANUP(curl) do { if( curl != NULL ) {  curl_easy_cleanup(curl) ; curl = NULL ; } } while(false)

httpInfo::httpInfo(CURLcode code)
{
	m_code =	code ;
}
//setters
void httpInfo::setPerformCode(CURLcode code)
{
	m_code = code ;
}
void httpInfo::setResponseCode(int code)
{
	m_responseCode = code ;
}
void httpInfo::setUrl(const std::string& url)
{
	m_url = url ;
}
void httpInfo::setEncodedUrl(const std::string& url)
{
	m_encodedUrl = url ;
}
void httpInfo::setEffectiveUrl(const std::string& url)
{
	m_effectiveUrl = url ;
}
void httpInfo::setTotalTime(double time)
{
	m_totalTime = time ;
}
void httpInfo::setTotalBytes(double bytes)
{
	m_totalBytes = bytes ;
}
void httpInfo::setSpeed(double speed)
{
	m_speed = speed ;
}
//getters
CURLcode httpInfo::getPerformCode() 
{
	return m_code ;
}
int httpInfo::getResponseCode()
{
	return m_responseCode ;
}
std::string httpInfo::getUrl()
{
	return m_url ;
}
std::string httpInfo::getEncodedUrl()
{
	return m_encodedUrl ;
}
std::string httpInfo::getEffectiveUrl()
{
	return m_effectiveUrl ;
}
double httpInfo::getTotalTime()
{
	return m_totalTime ;
}
double httpInfo::getTotalBytes()
{
	return m_totalBytes ;
}
double httpInfo::getSpeed()
{
	return m_speed ;
}

void httpInfo::dumpInfo()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DebugPrintf(SV_LOG_DEBUG, "curl perform code : %d\n", m_code) ;
	DebugPrintf(SV_LOG_DEBUG, "Url : %s\n", m_url.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "Encoded url :%s\n", m_encodedUrl.c_str()) ;
	if( m_code == CURLE_OK )
	{
		DebugPrintf(SV_LOG_DEBUG, "Curl response Code :%d\n", m_responseCode) ;
		DebugPrintf(SV_LOG_DEBUG, "Effective url :%s\n",m_effectiveUrl.c_str()) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

bool transport::m_transportInited = false ;

httpStreamObj::httpStreamObj(ACE_HANDLE hd, const std::string&fileName) :
m_hd( hd ), 
m_fileName(fileName)
{
}
httpTransport::httpTransport()
{

}
bool httpTransport::initTransport()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	bool retVal = false ;
	if( curl_global_init(CURL_GLOBAL_ALL) == 0 )
	{
		m_transportInited = true ;
		DebugPrintf(SV_LOG_DEBUG, "curl_global_init is successful\n") ;
		retVal = true ;
	}
	else
	{
		throw icatHttpTransportException("curl_global_init failed to initialize the transport\n") ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return retVal ;
}

bool httpTransport::cleanupTransport()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	curl_global_cleanup() ;
	m_transportInited = false ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return true ;
}
/*
* This is generic read call back function. This is used for http post.
*  ptr : pointer to data
*  size : size of the data
*  nmemb : size of each data member
*  stream : stream object used to read from
*/
size_t read_function( void *ptr, size_t size, size_t nmemb, void *stream) 
{
	httpStreamObj *obj = (httpStreamObj*) stream ;
	return ACE_OS::read(obj->m_hd, ptr, size * nmemb) ;
}

/*
* This is generic read call back function. This is used for http get.
*  ptr : pointer to data
*  size : size of the data
*  nmemb : size of each data member
*  stream : stream object used to write to
*/
size_t write_function( void *ptr, size_t size, size_t nmemb, void *stream)
{
	return size ;
}

/*
* This is generic read call back function. This is used to read header.
*  ptr : pointer to data
*  size : size of the data
*  nmemb : size of each data member
*  stream : stream object used to write to
*/
size_t header_function( void *ptr, size_t size, size_t nmemb, void *stream)
{
	return size ;
}

bool httpTransport::uploadFile(const std::string& localName, const std::string& remoteName )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	CURL* curl = NULL ;
	bool retVal = false ;
	ACE_stat stat ;
    double bytes_sent;
	char* effectiveUrl ;
	httpStreamObj streamObj ;
    ConfigValueObject *conf = ConfigValueObject::getInstance() ;
    archiveControllerPtr arc_controller = archiveController::getInstance() ;
	ACE_HANDLE hd = ACE_INVALID_HANDLE ;
	
	// PR#10815: Long Path support
	if( sv_stat(getLongPathName(localName.c_str()).c_str(), &stat) != 0 ) 
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to stat %s. Error: %d\n", localName.c_str(), errno) ;
		throw icatFileNotFoundException(localName) ;
	}
	
	// PR#10815: Long Path support
	hd = ACE_OS::open(getLongPathName(localName.c_str()).c_str(), O_RDONLY) ;
	if( hd == ACE_INVALID_HANDLE )
	{
		DebugPrintf(SV_LOG_DEBUG, "Unable to open %s. Error: %d\n", localName.c_str(), errno) ;
		throw icatFileIOException(localName) ;
	}
	streamObj.m_fileName = localName ;
	streamObj.m_hd = hd ;

	curl = curl_easy_init() ;
	try 
	{
		if( NULL == curl )
		{
			SAFE_ACE_CLOSE_HANDLE( hd ) ;
			throw icatHttpTransportException("curl_easy_init is failed to initialze the curl handle\n") ;
		}
		
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_function);
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
		curl_easy_setopt(curl, CURLOPT_PUT, 1L);
		std::string encodedUrl ;
		encodeUrl(remoteName, encodedUrl) ;
		m_httpInfo.m_encodedUrl = encodedUrl ;
		m_httpInfo.m_url = remoteName ;
		curl_easy_setopt(curl, CURLOPT_URL, encodedUrl.c_str());
		curl_easy_setopt(curl, CURLOPT_READDATA, &streamObj);
		curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)stat.st_size);
		SetTimeouts(curl) ;

		m_httpInfo.m_code = curl_easy_perform(curl) ;

		if( m_httpInfo.m_code == CURLE_OK )
		{
			curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD, &m_httpInfo.m_speed) ;
			curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &m_httpInfo.m_totalTime) ;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &m_httpInfo.m_responseCode) ;
            curl_easy_getinfo(curl, CURLINFO_SIZE_UPLOAD, &bytes_sent) ;
            curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effectiveUrl) ;
            m_httpInfo.m_effectiveUrl = effectiveUrl ;
            arc_controller->m_tranferStat.total_Num_Of_Bytes_Sent += bytes_sent;
			if( !(m_httpInfo.m_responseCode >= 200 && m_httpInfo.m_responseCode <= 299) )
			{
				throw icatHttpTransportException(m_httpInfo.m_code, m_httpInfo.m_responseCode) ;
			}
			else 
			{
                arc_controller->m_tranferStat.total_Num_Of_Transfered_Files++;
                DebugPrintf(SV_LOG_DEBUG, " ----Statistics for %s file---- \n",localName.c_str()) ;
                DebugPrintf(SV_LOG_DEBUG, " Response code: %d, Transferred Bytes: %lf bytes,  Time taken: %lf secs, Speed: %lf bytes/sec \n", m_httpInfo.m_responseCode, bytes_sent, m_httpInfo.m_totalTime, m_httpInfo.m_speed ) ;
                retVal =  true ;
			}
		}
		else
		{
			throw icatHttpTransportException(m_httpInfo.m_code, 0) ;
		}
	}
	catch(icatTransportException& ex)
	{
		SAFE_CURL_EASY_CLEANUP(curl) ;
		SAFE_ACE_CLOSE_HANDLE( hd ) ;
		throw ex ;
	}

	SAFE_CURL_EASY_CLEANUP(curl) ;
	SAFE_ACE_CLOSE_HANDLE( hd ) ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	return retVal ;
}

bool httpTransport::uploadFile(const fileSystemObj& fileObj, const std::string& remoteName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	CURL* curl = NULL ;
	char* effectiveUrl ;
	bool retVal = false ;
    double bytes_sent;
	ACE_stat stat ;
    ConfigValueObject *conf = ConfigValueObject::getInstance() ;
	archiveControllerPtr arc_controller = archiveController::getInstance() ;
    ACE_HANDLE hd = ACE_INVALID_HANDLE ;
	httpStreamObj streamObj ;

	fileSystemObj fileSysObj = const_cast <fileSystemObj&> (fileObj) ; 
	// PR#10815: Long Path support
	hd = ACE_OS::open(getLongPathName(fileSysObj.absPath().c_str()).c_str(), O_RDONLY) ;

	if( hd == ACE_INVALID_HANDLE )
	{
		DebugPrintf(SV_LOG_ERROR, "Unable to open %s. Error: %d\n", fileSysObj.absPath().c_str(), errno) ;
		throw icatFileNotFoundException(fileSysObj.absPath()) ;
	}
	stat = fileSysObj.getStat() ;
	streamObj.m_fileName = fileSysObj.absPath() ;
	streamObj.m_hd = hd ;
	curl = curl_easy_init() ;
	try
	{
		if( NULL == curl )
		{
			SAFE_ACE_CLOSE_HANDLE(hd) ;
			DebugPrintf(SV_LOG_ERROR, "curl_easy_init is failed\n") ;
			throw icatHttpTransportException("curl_easy_init failed") ;
		}
		
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_function);
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
		curl_easy_setopt(curl, CURLOPT_PUT, 1L);
		std::string encodedUrl ;
		encodeUrl(remoteName, encodedUrl) ;
		m_httpInfo.m_encodedUrl = encodedUrl ;
		m_httpInfo.m_url = remoteName ;
		curl_easy_setopt(curl, CURLOPT_URL, encodedUrl.c_str());
		curl_easy_setopt(curl, CURLOPT_READDATA, &streamObj);
		curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)stat.st_size);
		SetTimeouts(curl) ;

		m_httpInfo.m_code = curl_easy_perform(curl) ;
		if( m_httpInfo.m_code == CURLE_OK )
		{
			curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD, &m_httpInfo.m_speed) ;
			curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &m_httpInfo.m_totalTime) ;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &m_httpInfo.m_responseCode) ;
            curl_easy_getinfo(curl, CURLINFO_SIZE_UPLOAD, &bytes_sent) ;
            curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effectiveUrl) ;
            m_httpInfo.m_effectiveUrl = effectiveUrl ;
            arc_controller->m_tranferStat.total_Num_Of_Bytes_Sent += bytes_sent;
			if( !(m_httpInfo.m_responseCode >= 200 && m_httpInfo.m_responseCode <= 299) )
			{
				DebugPrintf(SV_LOG_DEBUG, "Curl Response Code : %d\n", m_httpInfo.m_responseCode) ;
				retVal = false ;
				throw icatHttpTransportException(m_httpInfo.m_code, m_httpInfo.m_responseCode) ;
			}
			else 
			{
                arc_controller->m_tranferStat.total_Num_Of_Transfered_Files++;
                DebugPrintf(SV_LOG_DEBUG, " ----Statistics for %s file---- \n",fileSysObj.absPath().c_str()) ;
                DebugPrintf(SV_LOG_DEBUG, " Response code: %d, Transferred Bytes: %lf bytes,  Time taken: %lf secs, Speed: %lf bytes/sec \n", m_httpInfo.m_responseCode, bytes_sent, m_httpInfo.m_totalTime, m_httpInfo.m_speed ) ;

                retVal =  true ;
			}
		}
		else
		{
			retVal = false ;
			throw icatHttpTransportException(m_httpInfo.m_code, m_httpInfo.m_responseCode) ;
		}
	}
	catch(icatHttpTransportException &ex)
	{
		SAFE_CURL_EASY_CLEANUP(curl) ;
		SAFE_ACE_CLOSE_HANDLE( hd ) ;
		throw ex ;

	}
	catch(icatTransportException &ex)
	{
		SAFE_CURL_EASY_CLEANUP(curl) ;
		SAFE_ACE_CLOSE_HANDLE( hd ) ;
		throw ex ;
	}
	SAFE_ACE_CLOSE_HANDLE(hd) ;
	SAFE_CURL_EASY_CLEANUP(curl) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return retVal ;
}
bool httpTransport::uploadDir(const std::string& localName, const std::string& remoteName )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	throw icatUnsupportedOperException(ICAT_OPER_ARCH_DIR) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return true ;
}
bool httpTransport::uploadDir(const fileSystemObj& fileObj, const std::string& remoteName) 
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	throw icatUnsupportedOperException(ICAT_OPER_ARCH_DIR) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return true ;
}

bool httpTransport::deleteFile(const std::string& remoteName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	CURL* curlhandle = NULL ;
    ConfigValueObject *conf = ConfigValueObject::getInstance() ;

	bool retVal = false ;
	try
	{
		curlhandle = curl_easy_init();
		if( curlhandle == NULL )
		{
			DebugPrintf(SV_LOG_ERROR, "curl_easy_init is failed\n") ;
			throw icatHttpTransportException("curl_easy_init failed") ;
		}

		std::string encodedUrl ;
		encodeUrl(remoteName, encodedUrl) ;
		m_httpInfo.m_url = remoteName ;
		m_httpInfo.m_encodedUrl = encodedUrl ;
		curl_easy_setopt(curlhandle, CURLOPT_URL, encodedUrl.c_str());
		curl_easy_setopt(curlhandle, CURLOPT_DNS_USE_GLOBAL_CACHE, 0) ; 	
		curl_easy_setopt(curlhandle, CURLOPT_NOSIGNAL, 1) ;		
		curl_easy_setopt(curlhandle, CURLOPT_CUSTOMREQUEST, "DELETE");
		SetTimeouts(curlhandle) ;
        m_httpInfo.m_code = curl_easy_perform(curlhandle);
		if( m_httpInfo.m_code == CURLE_OK )
		{
			CURLcode response = curl_easy_getinfo(curlhandle, CURLINFO_RESPONSE_CODE, &m_httpInfo.m_responseCode) ;
			if( !(m_httpInfo.m_responseCode >= 200 && m_httpInfo.m_responseCode <= 299) )
			{
				throw icatHttpTransportException(m_httpInfo.m_code, m_httpInfo.m_responseCode) ;
			}
			else 
			{
				retVal = true  ;
			}
		}
		else
		{
			retVal = false ;
			throw icatHttpTransportException(m_httpInfo.m_code, 0) ;
		}
	}
	catch( icatHttpTransportException & httpex )
	{
		SAFE_CURL_EASY_CLEANUP(curlhandle) ;
		throw httpex ;
	}
	catch( icatTransportException & ex )
	{
		SAFE_CURL_EASY_CLEANUP(curlhandle) ;
		throw ex ;
	}
	SAFE_CURL_EASY_CLEANUP(curlhandle) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return retVal ;
}
bool httpTransport::deleteDir(const std::string& remotename) 
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	throw icatUnsupportedOperException(ICAT_OPER_DEL_DIR) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

bool httpTransport::fileExists(const std::string& remoteName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	std::string url ; 
    CURL *curl = NULL ;
	bool retCode = false ;
    ConfigValueObject *conf = ConfigValueObject::getInstance() ;

	try
	{
		curl = curl_easy_init() ;
		curl_easy_setopt(curl, CURLOPT_HEADER, 0L ) ;
		std::string encodedUrl ;
		encodeUrl(remoteName, encodedUrl) ;
		m_httpInfo.m_url = remoteName ;
		m_httpInfo.m_encodedUrl = encodedUrl ;
		curl_easy_setopt(curl, CURLOPT_URL, encodedUrl.c_str()) ;
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L) ;
		SetTimeouts(curl) ;
        m_httpInfo.m_code = curl_easy_perform(curl) ;
		if( m_httpInfo.m_code == CURLE_OK )
		{
			CURLcode response = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &m_httpInfo.m_responseCode) ;
			if( !(m_httpInfo.m_responseCode >= 200 && m_httpInfo.m_responseCode <= 299) )
			{
				throw icatHttpTransportException(m_httpInfo.m_code, m_httpInfo.m_responseCode) ;
			}
			else 
			{
				DebugPrintf(SV_LOG_DEBUG, "curl response code is %d\n", m_httpInfo.m_responseCode) ;
				retCode = true ;
			}
		}
		else
		{
			throw icatHttpTransportException(m_httpInfo.m_code, m_httpInfo.m_responseCode) ;
		}
	}
	catch( icatHttpTransportException & ex)
	{
		SAFE_CURL_EASY_CLEANUP(curl) ;
		throw ex ;
	}
	SAFE_CURL_EASY_CLEANUP(curl) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return retCode ;
}


enum HCAP_SERVER_FILEOBJ_TYPE { HCAP_SERVER_FILEOBJ_FILE, HCAP_SERVER_FILEOBJ_DIR, HCAP_SERVER_FILEOBJ_NONE } ;


/*
* This function encodes a given URL.
* params 
* url : Actual URL
* encodedUrl : encoded Url
* Description : The url is divided into tokens whenever one of the `~!#$%^&*()-_=+[{]}|\\;:'\",/<.>? characters is encountered.
*    The :./_- are not encoded.
*/
void httpTransport::encodeUrl(const std::string& url, std::string& encodedUrl)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	std::string archvieRootDir = ConfigValueObject::getInstance()->getArchiveRootDirectory() ;
	archvieRootDir.erase( archvieRootDir.find_last_not_of("/") + 1) ; //erase all trailing spaces
	archvieRootDir.erase(0 , archvieRootDir.find_first_not_of("/") ) ; //erase all leading spaces
	archvieRootDir = "/" + archvieRootDir ;
	archvieRootDir += "/" ;
	std::string str = url.substr(url.find(archvieRootDir) + 11) ;  //only encode the path that is relative to http://<fcfs_data>/fcfs_data/
	std::string delims= "`~!#$%^&*()-_=+[{]}|\\;:'\",/<.>?\t\n " ;
	std::string tokenWithDelim ;
	std::string noEncode= ":./_-";
    size_t index1 = 0 ;
    size_t index2 = 0 ;
	encodedUrl = url.substr(0, url.find(archvieRootDir) + 11) ;
    while(true)
    {
		tokenWithDelim = "" ;
        index2 = str.find_first_of(delims, index1) ;

		if( index2 != std::string::npos )
		{
			if( str.find_first_of(delims, index1 + 1) == std::string::npos )
			{
					tokenWithDelim = str[index1] ;
			}
			else
			{
					tokenWithDelim = str.substr(index1, index2 - index1 + 1) ;
			}
			
			if( tokenWithDelim.find_first_of(noEncode, 0) == std::string::npos )
			{
				char* encodedToken = curl_escape(tokenWithDelim.c_str(), (int) tokenWithDelim.length()) ;
				encodedUrl += encodedToken ;
				curl_free(encodedToken) ;
			}
			else
			{
				encodedUrl += tokenWithDelim ;
			}

 			index1 = index2 + 1 ;
		}
		else
		{
			encodedUrl += str.substr(index1) ;
			break ;
		}
    }
	DebugPrintf(SV_LOG_DEBUG, "URL : %s\n", url.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "Encoded URL : %s\n", encodedUrl.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}



void httpTransport::SetTimeouts(CURL *curl)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	long lConnectTimeoutSeconds, lLowspeedTime, lLowSpeedLimit ;
	lConnectTimeoutSeconds = ConfigValueObject::getInstance()->getConnectionTimeout() ;
    lLowspeedTime = ConfigValueObject::getInstance()->getLowspeedTime() ;
    lLowSpeedLimit = ConfigValueObject::getInstance()->getLowspeedLimit() ;
	curl_easy_setopt(curl,CURLOPT_NOSIGNAL, 1) ; 
	curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT, lConnectTimeoutSeconds);
	curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT, lLowSpeedLimit);
	curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME, lLowspeedTime);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}
