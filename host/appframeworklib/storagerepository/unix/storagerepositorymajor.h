#ifndef __FACTORY_MAJOR__H___
#define __FACTORY_MAJOR__H___
#include "storagerepository/storagerepository.h"
#include "applicationsettings.h"

class DiskStorageRepository : public StorageRepository
{
public:  
	std::string m_mountPointNamePath;
	std::string	m_label;
	std::string m_fileSystemType;
	std::string m_createFs;
	
public:
	DiskStorageRepository(std::map<std::string,std::string>&, const std::string&);
	SVERROR setupDiskRepository();
    SVERROR setupRepository();
	SVERROR setupRepositoryV2(ApplicationPolicy&, SetupRepositoryStatus&) ;
};
class VGRepository : public StorageRepository
{
public:
	VGRepository( const std::string& ) ;
	SVERROR setupRepository() { return SVS_FALSE ; }
	SVERROR setupRepositoryV2(ApplicationPolicy&, SetupRepositoryStatus&) ;
} ;

class VolumeStorageRepository : public StorageRepository
{
	std::string m_mountPointNamePath;
	std::string m_fileSystemType;
	std::string m_createFs;
public:
	VolumeStorageRepository(std::map<std::string,std::string>&, const std::string&);
	SVERROR setupVolumeRepository();	
    SVERROR setupRepository();
	SVERROR setupRepositoryV2(ApplicationPolicy&, SetupRepositoryStatus&) ;
};

class NFSStorageRepository : public StorageRepository
{
	std::string m_mountPointNamePath;	
public:
	NFSStorageRepository(std::map<std::string,std::string>&, const std::string&);
	SVERROR setupNFSPathRepository();
    SVERROR setupRepository();
	SVERROR setupRepositoryV2(ApplicationPolicy&, SetupRepositoryStatus&) ;
};

class CIFSNFSStorageRepository : public StorageRepository
{
public:
	std::string m_mountPointNamePath;
	std::string m_userName;
	std::string m_passwd;
	std::string m_domainName;
	CIFSNFSStorageRepository(std::map<std::string,std::string>&, const std::string&);
	SVERROR setupCIFSPathRepository();
    SVERROR setupRepository();
	SVERROR setupRepositoryV2(ApplicationPolicy&, SetupRepositoryStatus&) ;
};

#endif
