#ifndef ARCHIVE_OBJECT_H
#define ARCHIVE_OBJECT_H
#include <boost/shared_ptr.hpp>
#include <vector>
#include <string>

#include "filesystemobj.h"
#include "transport.h"
#include "defs.h"

#define HCAP_HTTP_CONN_TIMEOUT  60

class archiveObj
{
public:
	virtual ICAT_OPER_STATUS archiveFile(const fileSystemObj& obj) = 0;
	virtual ICAT_OPER_STATUS archiveDirectory(const fileSystemObj& obj) = 0;
	virtual ICAT_OPER_STATUS deleteFile(const fileSystemObj& obj) = 0 ;
	virtual ICAT_OPER_STATUS deleteDirectory(const fileSystemObj& obj) = 0 ;
	virtual ICAT_OPER_STATUS specifyMetaData(const fileSystemObj& obj) = 0 ;

	void setArchiveAddress(const std::string& address) ;
	void setPort(int port) ;
	void setArchiveRootDirectory(const std::string& rootDirectory) ;
	std::string getArchiveAddress() ;
	int getPort(int port) ;

	std::string archvieRootDirectory() ;
	std::string m_archiveAddress ;
	std::string m_archiveRootDir ;
	int m_port ;
} ;

class ArchiveObj : public archiveObj
{
public:
	ArchiveObj() ;
	ArchiveObj(const std::string& hcapAddress) ;
	ICAT_OPER_STATUS archiveFile(const fileSystemObj& obj) = 0 ;
	ICAT_OPER_STATUS archiveDirectory(const fileSystemObj& obj) = 0 ;
	ICAT_OPER_STATUS deleteFile(const fileSystemObj& obj) = 0 ;
	ICAT_OPER_STATUS deleteDirectory(const fileSystemObj& obj) = 0 ;
	ICAT_OPER_STATUS specifyMetaData(const fileSystemObj& obj) = 0 ;
} ;

class HttpArchiveObj :public ArchiveObj
{
	httpTransport m_transport ;
	std::string m_remoteDirPath ;  //This is relative to http://<ip/dnsname>/fcfs_data/
	static std::string m_baseUrl ;
public:
	HttpArchiveObj() ;
	HttpArchiveObj(const std::string& hcapAddress, const std::string& remDirPath) ;
	bool init() ;
	ICAT_OPER_STATUS archiveFile(const fileSystemObj& obj) ;
	ICAT_OPER_STATUS archiveDirectory(const fileSystemObj& obj) ;
	ICAT_OPER_STATUS deleteFile(const fileSystemObj& obj) ;
	ICAT_OPER_STATUS deleteDirectory(const fileSystemObj& obj) ;
	ICAT_OPER_STATUS specifyMetaData(const fileSystemObj& obj) ;
	void prepareDataUrl(const std::string& relPath, std::string& url) ;


	static std::string prepareDateStr() ;
	static std::string prepareBaseUrl() ;
	static std::string m_startTime ;
} ;

class CifsNfsArchiveObj : public ArchiveObj
{
	
public:
	ICAT_OPER_STATUS archiveFile(const fileSystemObj& obj) ;
	ICAT_OPER_STATUS archiveDirectory(const fileSystemObj& obj) ;
	ICAT_OPER_STATUS deleteFile(const fileSystemObj& obj) ;
	ICAT_OPER_STATUS deleteDirectory(const fileSystemObj& obj) ;
	ICAT_OPER_STATUS specifyMetaData(const fileSystemObj& obj) ;
	static std::string m_basePath ;
	std::string prepareBasePath() ;
	void prepareFullPath(const std::string& relPath, std::string& fullPath)  ;
	std::string prepareDateStr() ;
	static std::string m_startTime ;
} ;
typedef boost::shared_ptr<archiveObj> archiveObjPtr ;
typedef boost::shared_ptr<ArchiveObj> ArchiveObjPtr ;
typedef boost::shared_ptr<HttpArchiveObj> HttpArchiveObjPtr ;
typedef boost::shared_ptr<CifsNfsArchiveObj> CifsArchiveObjPtr ;
#endif

