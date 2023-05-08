#ifndef REPOSITORY__HANDLER__H
#define REPOSITORY__HANDLER__H

#include "Handler.h"
#include "localconfigurator.h"
#include <iostream>
#include <fstream>
#include <cstring>
struct RepoDeviceDetails
{
    SV_ULONGLONG capacity ;
    SV_ULONGLONG freeSpace ;
	SV_INT writeCacheState;
} ;

struct VolumeDetails 
{
    SV_ULONGLONG capacity ;
    SV_ULONGLONG freeSpace ;
	SV_INT writeCacheState;
	std::string mountPoint;
	std::string volumeLabel;
	bool isSystemVolume;
} ;

class RepositoryHandler :
	public Handler
{
	void GetEligibleRepoDevices(std::map<std::string,RepoDeviceDetails>& repoDevDetails) ;
    bool IsDeviceAvailableinSystem(std::string& VolumeName, VolumeSummary& foundDevice ) ;
    void DeleteRepositoryContents(bool bShouldDeleteProtection) ;
    bool CopyRetentionData(const std::string& srcVolName, 
                                          const std::string& existingRepo,
                                          const std::string& newRepo ) ;

    bool CopyRepositoryContents( const std::string& existingRepo, 
                                 const std::string& newRepo ) ;

    bool CopyVolpackFiles(const std::string& srcVolName, 
                          const std::string& existingRepo,
                          const std::string& newRepo,
                          const LocalConfigurator& lc,
                          bool compressedVolpacks, InmSparseAttributeState_t sparseattr_state) ;
    bool CopyConfigurationFiles( const std::string& existingRepo,
                                 const std::string& newRepo )  ;
    bool ModifyPathsInConfigFiles( const std::string& existingRepo,
                                   const std::string& newRepo )  ;
    bool ChangeVolpackEntriesInDrScoutConf( const std::string& existingRepo,
                                            const std::string& newRepo )  ;
    bool ChangeConfigurationEntries( const std::string& existingRepo,
                                     const std::string& newRepo )  ;
    bool CopyMbrDirectory( const std::string& existingRepo, 
                           const std::string& newRepo ) ;

	//bool ShouldMoveRepository( FunctionInfo& request , const std::string& newRepo, const std::string& username, const std::string& password) ;
	//bool ShouldArchiveRepository( FunctionInfo& request , const std::string& newRepo, const std::string& username,  const std::string& password ) ;	
	bool CopyLogsDirectory( const std::string& existingRepo, 
                                         const std::string& newRepo );
	INM_ERROR ModifyThePathsIfRequired(const std::string& existRepo, 
										const std::string& newRepo,
									   LocalConfigurator& lc) ;
	bool ReMountVolpacks(LocalConfigurator& configurator,
						 const std::string& existingRepo, 
						 const std::string& newRepo, bool force=false ) ;

	void RemCredentialsFromRegistry() ;
	void UnmountAllVirtualVolumes() ;
	static std::string m_domain, m_username, m_password ;
	void DoStopFilteringForAllVolumes() ;
public:
    void DeleteOldRepositoryContents( std::string& oldRepoPath, bool stopFiltering=false ) ;
	INM_ERROR ChangeBackupLocation(FunctionInfo& finfo) ;
	INM_ERROR ArchiveRepository(FunctionInfo& finfo) ;
	RepositoryHandler(void);
	~RepositoryHandler(void);
	virtual INM_ERROR process(FunctionInfo& request);
	virtual INM_ERROR validate(FunctionInfo& request);
	void GetEligibleVolumes(std::map<std::string,VolumeDetails>& eligibleVolumes);
	INM_ERROR ListRepositoryDevices(FunctionInfo& request);
	INM_ERROR ListConfiguredRepositories(FunctionInfo& request);
	bool IsValidRepository() ;
	std::string m_progresslogfile ;
	bool isSameRepoOrNot(std::string& repoPath, std::string& newRepoPath );
	void ReportProgress(std::stringstream& msg, bool isError = true ) ;
    INM_ERROR ReconstructRepo(FunctionInfo& request) ;
	INM_ERROR SetupRepository(FunctionInfo& request);
    INM_ERROR DeleteRepository(FunctionInfo& request); 
    INM_ERROR MoveRepository(FunctionInfo& request) ;
	INM_ERROR SetCompression(FunctionInfo& request) ;
	INM_ERROR ShouldMoveOrArchiveRepository(FunctionInfo& request) ;
	bool GetCredentialsFromRequest(const FunctionInfo& request, 
								   std::string& user,
								   std::string& password,
								   std::string& domain,
								   std::string& msg) ;
	INM_ERROR UpdateCredentials(FunctionInfo& request) ;
	INM_ERROR MoveRepository(const std::string& repoDevice, std::string& errMsg, bool isGuestAccess) ;
	INM_ERROR ArchiveRepository(const std::string& repoDevice, std::string& errMsg) ;
	void ModifyConfigurationContents(const std::string& existingRepo, 
                                     const std::string& newRepo ) ;
    void RemoveAgentConfigData() ;
	bool CopyDirectoryContents(const std::string& src, const std::string& dest,bool recursive=true);
	bool get_filenames_withpattern(const std::string & dirname,const std::string& pattern,std::vector<std::string>& filelist );
	bool CleanAllDirectoryContent(const std::string & dbpath);
	bool CopyFile(const std::string & SourceFile, const std::string & DestinationFile, unsigned long& err);
	bool isBackuplocationAndInstallationVolumeSame (const std::string &newRepoPath); 
	//Bug 20159 - Method to get the Disk Type
	std::string getBusType(std::string devicename);
	bool IsVolumdProvisioningUnProvisioningInProgress();
};

#endif /*REPOSITORY__HANDLER__H*/
