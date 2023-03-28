#include "filesystemobj.h"
#include <ace/SString.h>
#include "ace/OS.h"
#include "defs.h"

fileSystemObj::fileSystemObj(const std::string& absPath, 
							const std::string& relPath, 
							std::string fileName, 
							unsigned long long contentSrcId, 
							unsigned long long folderId , 
							ACE_stat& stat,
							ICAT_OPERATION operation )
{
	m_absPath = absPath;
	m_relPath = relPath ;
	m_stat = stat ;
	m_contentSrcId = contentSrcId ;
	m_folderId = folderId ;
	m_fileName = fileName; 
	m_operation = operation;
	m_isDir = false ;
}

fileSystemObj::fileSystemObj(std::string url, unsigned long long folderId, bool isDir, std::string fileName) 
{
	m_url = url ;
	m_folderId = folderId ;
	m_fileName = fileName ;
	m_operation = ICAT_OPER_DEL_FILE ;
	m_isDir = isDir ;
}
bool fileSystemObj::isDir()
{
	return  m_isDir;
}
std::string fileSystemObj::getRelPath()
{
	return m_relPath ;
}
std::string fileSystemObj::absPath()
{
	return m_absPath ;
}
ACE_stat fileSystemObj::getStat()
{
	return m_stat ;
}

unsigned long long fileSystemObj::getFolderId()
{
	return m_folderId ;
}

std::string fileSystemObj::fileName()
{
	return m_fileName ;
}
