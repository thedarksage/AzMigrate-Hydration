#ifndef VOLUME_CONFIG_H
#define VOLUME_CONFIG_H
#include <boost/shared_ptr.hpp>

#include "hostdiscoveryhandler.h"

#include "volumesobject.h"
#include "VolumeHandler.h"
#include "repositoriesobject.h"

class VolumeConfig ;
typedef boost::shared_ptr<VolumeConfig> VolumeConfigPtr ;

class VolumeConfig
{
    std::list<std::string> m_removedProtectedVolumes ;
    bool m_isModified ;
    bool m_firstRegistration ;
    ConfSchema::VolumesObject m_volumeAttrNamesObj ;
    ConfSchema::VolumeResizeHistoryObject m_volResizeHistoryAttrNamesObj ;
    ConfSchema::RepositoriesObject m_repositoryAttrNamesObj ;
    ConfSchema::Group* m_VolumeGroup ;
    ConfSchema::Group* m_RepositoryGroup ;
    static VolumeConfigPtr m_volumeConfigPtr ;
    void loadVolumeGrp() ;

  

    void FillAppToSchemaAttrs(AppToSchemaAttrs_t &apptoschemaattrs, 
                                const HostRegisterType_t regtype) ;

    void trimSlashesForNames(ConfSchema::Object &o) ;

    void UpdateVolumeHistory(ConfSchema::Object *pobject, 
                             ConstParameterGroupsIter_t iter) ;
    VolumeConfig() ;
    void SendAlertIfNewVolume(ConfSchema::Object *pobject, 
                              ConstParameterGroupsIter_t iter) ;

    void UpdateStaticInfo(const ParameterGroup& individualVolPg) ;

	void UpdateDynamicInfo(const ParameterGroup& individualVolPg , std::map <std::string,SV_INT> & volumeLockMap);

    void SanitizeVolName(std::string& pathname) ;

    bool IsFirsRegistration() ;

    void UpdateVolumeHistory( const std::string& volumeName, 
                              SV_ULONGLONG oldRawCapacity, 
                              SV_ULONGLONG rawCapacity, 
                              SV_ULONGLONG oldCapacity, 
                              SV_ULONGLONG capacity) ;

    bool CanUpdate(const std::string& volumeName, int locked, int volumeType, SV_ULONGLONG volumeCapacity , SV_ULONGLONG rawCapacity) ;

    void AlertOnDiskCachingIfRequired() ;
    

    std::string GetBootVolume( ) ;

public:
	 void GetVolumeResizeHistory(ConfSchema::Object* volumeObj,
                                std::map<time_t, volumeResizeDetails>& volumeResizeHistory) ;

    bool GetVolumeObjectByVolumeName(const std::string& volumeName, ConfSchema::Object** volObj) ;
	void ChangeRepoPath(const std::string& repoPath) ;
	bool IsValidRepository() ;
    void GetRepositoryVolumes(std::list<std::string>& repoVolumes) ;

	void GetVolumesWithTypeIDOne(const std::string& volPgName,const ParameterGroup& volumePg,std::list<std::string> &newVolumesWithTypeOne);

    void GetVolumes(std::list<std::string>& volumes) ;

    std::map<std::string, volumeProperties> GetGeneralVolumes(std::list<std::string>& volumes) ;

    void GetVirtualVolumes(std::list<std::string>& volumes) ;

    void GetVolPackVolumes(std::list<std::string>& volumes) ;

    void GetProtectableVolumes(const std::list<std::string>& protectedVolumes,
                               std::list<std::string>& protectableVolumes) ;

    bool IsVolumeAvailable(const std::string& volumeName) ;

    bool IsTargetVolumeAvailable(const std::string& volumeName) ;

    void GetSystemVolumes(std::list<std::string>& systemVolumes) ;

    bool IsSystemVolume(const std::string& volumeName) ;

    bool IsBootVolume(const std::string& volumeName) ;

    bool GetCapacity(const std::string& volumeName, SV_LONGLONG& capacity) ;

    bool GetRawSize(const std::string& volumeName, SV_LONGLONG& rawSize) ;


    bool GetVolumeProperties( std::map<std::string, volumeProperties>& volumePropertiesMap) ;

    bool GetVolumeResizeTimeStamp(const std::string& volumeName, 
                                  const time_t& resyncStartTime,  
                                  time_t& resizeTimeStamp) ;

    bool isRepositoryConfigured(const std::string& repoName) ;

    bool isRepositoryAvailable(const std::string& repoName) ;
    
    void GetRepositoryNameAndPath(std::string& repoName, std::string& repoPath) ;

    bool GetRepositoryVolume(const std::string& repoName, std::string& repoPath) ;
    
    bool GetRepositoryFreeSpace(const std::string& repoName, SV_ULONGLONG& freeSpace) ;

	bool GetRepositoryCapacity(const std::string& repoName, SV_ULONGLONG& capacity);

    bool UpdateVolumeFreeSpace(const std::string& volName, SV_LONGLONG freeSpace) ;

    bool GetfreeSpace( const std::string& volName, SV_LONGLONG& freeSpace ) ;

	bool GetFileSystemUsedSpace (const std::string &volName , SV_ULONGLONG & usedSpace );

    bool AddRepository(const std::string& repoName, const std::string& repoDev) ;

    void UpdateVolumes(const std::string& volPgName,
                       const ParameterGroup& volumePg, 
                       eHostRegisterType regType) ;
    void CreateRepoObjectIfRequired() ;
    bool GetLatestResizeTs(const std::string& volumeName,
                           SV_LONGLONG& latestResizeTs) ;
    
    void GetNonProtectedVolumes(const std::list<std::string>& protectedVolumes,
                                const std::list<std::string>& excludedVolumes,
                                std::list<std::string>& nonprotectedVolumes) ;

    void RemoveMissingVolumes(const std::list<std::string>& registeredVolumes) ;

    
    void UpdateTargetDiskUsageInfo( const std::string& volumeName, SV_ULONGLONG sizeOnDisk, 
                                    SV_ULONGLONG spaceSavedByCompression, 
                                    SV_ULONGLONG spaceSavedByThinProvisioning ) ;

    static VolumeConfigPtr GetInstance(bool loadgrp=true) ;
    void ChangePaths( const std::string& existingRepo, 
                      const std::string& newRepo ) ;
    bool isModified() ;

    bool IsEnoughFreeSpaceInSystemDrive( int volumes, SV_LONGLONG requiredFreeSpace, 
                            std::string& alertMsg ) ;

    bool IsEnougFreeSpaceInVolume( const std::string& volume, int percentFreeSpace, 
                                   std::string& msg) ;
	SV_ULONGLONG GetVolPackVolumeSizeOnDisk (void);
	bool GetVolumeSizeOnDisk(const std::string& volumeName, SV_LONGLONG& volumeSizeOnDisk);
	void DeleteRepositoryObject ();
	bool GetResizeDetails	(const std::string& volumeName,
                             const SV_LONGLONG &latestResizeTs, 
							 volumeResizeDetails &resizeDetails); 
	bool isExtendedOrShrunkVolumeGreaterThanSparseSize  (	const std::string &volumeName, 
															SV_LONGLONG &volumeRawSizeInExistence ); 
	bool isShrunkVolume(const std::string &volumeName ); 
	bool isVirtualVolumeExtended(const std::string &volumeName ); 

	bool SetRawSize(const std::string& volumeName, SV_LONGLONG& rawSize); 
	bool SetIsSparseFilesCreated(const std::string& volumeName,
								const SV_LONGLONG &latestResizeTs);
	void RemoveVolumeResizeHistory(ConfSchema::Object* volumeObj); 
	void RemoveVolumeResizeHistory(const std::list<std::string> &volumeList); 
	bool GetVolumeGroupName(const std::string& volumename, std::string& volumegrpname) ;
	void SetDummyGrps(ConfSchema::Group& volgrp, ConfSchema::Group& repogrp) ;
	bool GetSameVolumeList(const  std::list<std::string>& protectableVolumes,
							  std::list<std::list<std::string> >& sameVolumeList);
} ;
bool DoesDiskContainsProtectedVolumes(const std::string& newrepopath, 
									  const std::list<std::string>& protectedvolumes, 
									  std::string& diskname, std::string& volume) ;
#endif
