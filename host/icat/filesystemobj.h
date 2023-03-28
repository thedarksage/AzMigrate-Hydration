#ifndef  FILE_SYSTEM_OBJ_H
#define FILE_SYSTEM_OBJ_H
#include <string>
#include "ace/OS_NS_sys_stat.h"
#include "defs.h"

class fileSystemObj
{
	std::string m_absPath ;
	std::string m_relPath ;
	ACE_stat m_stat ;
	std::string m_fileName ;
	unsigned long long m_folderId ;
	unsigned long long m_contentSrcId ;
	std::string m_url ;
	ICAT_OPERATION m_operation ;
	bool m_isDir ;
public:
	fileSystemObj() {}
	fileSystemObj(const std::string& absPath,
                    const std::string& relPath,
                    std::string m_fileName,
                    unsigned long long folderId ,
                    unsigned long long contentSrcId,
                    ACE_stat& stat,
					ICAT_OPERATION operation = ICAT_OPER_ARCH_FILE ) ;

	fileSystemObj(std::string url, unsigned long long folderId, bool isDir, std::string fileName) ;
	bool isDir() ;
	unsigned long long getFolderId() ;
	unsigned long long getContentSrcId() ;
	std::string getRelPath() ;
	std::string absPath() ;
	ACE_stat getStat() ;
	std::string fileName() ;
	std::string getUrl() { return m_url ; }
	ICAT_OPERATION getOperation() { return m_operation ; }
} ;
#endif

