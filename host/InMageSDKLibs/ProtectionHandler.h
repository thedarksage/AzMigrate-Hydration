#ifndef PROTECTION__HANDLER__H
#define PROTECTION__HANDLER__H

#include "Handler.h"
#include "confschema.h"
#include "confschemamgr.h"

#include "planobj.h"
#include "appscenarioobject.h"
#include "consistencypolicyobj.h"
#include "protectionpairobject.h"
#include "applicationsettings.h"
#include "pairinfoobj.h"
#include "scenariodetails.h"
#include "scenarioretentionpolicyobj.h"
#include "scenarioretentionwindowobj.h"
#include "pairsobj.h"
#include "apppolicyobject.h"
#include "confengine/scenarioconfig.h"
#include "VolumeHandler.h"

class ProtectionHandler :
	public Handler
{
    INM_ERROR CanExcludeVolumes(FunctionInfo& request, 
                                const std::list<std::string>& excludedVolumes,
                                std::string& volName) ;

    void GetVolumeLevelPg(const std::string& volumeName,
                          INM_ERROR volError,
                          const std::string& msg,
                          ParameterGroup& pg) ;

    INM_ERROR GetErrorCodeFromPairState(VOLUME_SETTINGS::PAIR_STATE pairState) ;

    void GetVolumeListFromPg(const ParameterGroup& pg, std::list<std::string>& volumeList) ;
    
    INM_ERROR PauseTracking(const std::list<std::string>& volumesList,
                            bool isRescueMode,
                            FunctionInfo& request) ;


    INM_ERROR ResumeTracking(const std::list<std::string>& volumes, bool isRescueMode, FunctionInfo& request) ;

    INM_ERROR ResumeProtection(const std::list<std::string>& volumes, FunctionInfo& request) ;
    
	INM_ERROR ResyncProtection(const std::list<std::string>& explicitSyncList,
                               const std::list<std::string>& outofSyncList,
                               FunctionInfo& request) ;
    INM_ERROR CheckForEnoughFreeSpace( const std::list<std::string>& volumes,
                                       std::string& notenougfreespacemsg) ;
public:
    INM_ERROR PauseProtection(const std::list<std::string>& volumes, FunctionInfo& f) ;
	ProtectionHandler(void);
	~ProtectionHandler(void);
	virtual INM_ERROR process(FunctionInfo& request);
	virtual INM_ERROR validate(FunctionInfo& request);
	INM_ERROR SetupBackupProtection(FunctionInfo& request);
	INM_ERROR SpaceRequired (FunctionInfo &request);
	INM_ERROR DeleteBackupProtection(FunctionInfo& request);
	INM_ERROR ResyncProtection(FunctionInfo& request);
	INM_ERROR ResumeProtectionWithResync(FunctionInfo& request);
	INM_ERROR PauseTracking(FunctionInfo& request);
	INM_ERROR PauseTrackingForVolume(FunctionInfo& request);
    INM_ERROR PauseProtection(FunctionInfo& f);
    INM_ERROR PauseProtectionForVolume(FunctionInfo& f);
    INM_ERROR ResumeProtection(FunctionInfo& f);
    INM_ERROR ResumeProtectionForVolume(FunctionInfo& f);
    INM_ERROR ResumeTrackingForVolume(FunctionInfo& request) ;
    INM_ERROR ResumeTracking(FunctionInfo& request) ;
    INM_ERROR IssueConsistency(FunctionInfo& request);
	INM_ERROR AddVolumes(FunctionInfo& request);
    INM_ERROR ListHosts(FunctionInfo& request) ;
	INM_ERROR ListHostsMinimal(FunctionInfo& request) ;
	INM_ERROR SetLogRotationInterval(FunctionInfo& request) ;
    INM_ERROR IsDeletionAllowed( std::list <std::string> &volumeList );
	bool GetHostDetailsToRepositoryConf(std::list<std::map<std::string, std::string> >& hostInfoList) ;	
	void getVolumesForDeletion(FunctionInfo& request,std::list<std::string> &deleteVolumeList);
	bool isAllProtectedVolumesPaused(std::list <std::string> &protectedVolumes);
	bool isAllProtectedVolumesResumed(std::list <std::string> &protectedVolumes);
	bool IsRepositoryInIExists();
	bool isHostNameChanged (std::string &currentName);
	INM_ERROR SpaceCheck(FunctionInfo& request);
	void DeleteVolpackEntry (std::string &volumeName);
	bool DeleteSparseFiles (std::string &volumeName , std::string &errMsg); 
};

void GetPairDetails( int rpoPolicy,
                    RetentionPolicy& retentionPolicy,
                    const std::string& repoVolume,
                    const std::string& tgtHostId,
                    const std::list<std::string>& protectableVolumes,
                    const std::map<std::string, volumeProperties>& volumePropertiesMap,
                    std::map<std::string, PairDetails>& pairDetailsMap);

bool IsEnoughSpaceInRepo( SV_ULONGLONG& repoCapacity, 
						 std::map<std::string,PairDetails>& pairDetails, 
						 SV_ULONGLONG& requiredFreeSpace,SV_ULONGLONG & protectedVolumeOverHead );
bool IsEnoughSpaceInRepo( SV_ULONGLONG& repoCapacity, SV_ULONGLONG totalProtectedCapacity, 
						 SV_ULONGLONG fileSystemOccupiedSize, RetentionPolicy& retPolicy, 
						 SV_ULONGLONG& requiredCapacity,SV_ULONGLONG & protectedVolumeOverHead );
bool IsEnoughSpaceInRepo (SV_ULONGLONG &repoFreeSpace,std::list<std::string> &eligibleVolumes,
						  SV_ULONGLONG &requiredFreeSpace,SV_ULONGLONG & protectedVolumeOverHead);

bool IsEnoughSpaceInRepoWithOutXMLRead ( SV_ULONGLONG& repoFreeSpace, SV_ULONGLONG protectedCapacity, SV_ULONGLONG fileSystemOccupiedSize,
						 RetentionPolicy& retPolicy, SV_ULONGLONG& requiredSpace, SV_ULONGLONG& protectedVolumeOverHead);

bool checkSpaceAvailability (const SV_ULONGLONG requiredFreeSpace,const SV_ULONGLONG repoFreeSpace , 
							 const SV_ULONGLONG & protectedVolumeOverHead);
double GetRepoLogsSpace ( ) ;
double GetRetentionPolicyDays (const double dNoOptimizedDuration ,const double dOptimizedDuration);
double GetAnticipatedNonOverlappedIOSize (const double nonOverlappedChangeRate ,const double  dlfileSystemOccupiedSize ,const double retentionPolicyDays);
double GetVolpackSize (const double anticipatedNonOverlappedIOSize ,const double fileSystemOccupiedSize ,const double dltotalProtectedCapacity, SV_UINT compressVolpacks);
double GetCompressedVolpackSize (const double anticipatedNonOverlappedIOSize ,const double fileSystemOccupiedSize ,const double dltotalProtectedCapacity);
double GetCDPStoreNotOptimisedSpace (const double fileSystemOccupiedSize ,const double totalChangeRate ,const double dNoOptimizedDuration);
double GetCDPStoreOptimisedUsage (const double fileSystemOccupiedSize,const double totalChangeRate ,const double dOptimizedDuration ,const int sparseSavingFactor);
double GetTotalCDPSpaceNoCompression (const double cdpStoreNotOptimisedSpace,const double cdpStoreOptimisedUsage);
double GetToatlCDPSpaceWithCompression (const double totalCDPSpaceNoCompression ,const double compressionBenifits);
double TotalSpaceRequired (const double volPackSize,const double totalCDPSpaceNoCompression,const double ntfsOverHead,const double inmageSpace);
bool bCheckSpaceAvailability (SV_ULONGLONG repoCapacity,SV_ULONGLONG totalProtectedCapacity, 
							  SV_ULONGLONG fileSystemOccupiedSize,SV_ULONGLONG &requiredCapacity,
							  double dNoOptimizedDuration,double dOptimizedDuration,SV_ULONGLONG &protectedVolumeOverHead);
SV_ULONGLONG GetTotalFileSystemOccupiedSize(std::map<std::string, PairDetails> &pairDetailsMap);
SV_ULONGLONG GetTotalProtectedCapacity(std::map<std::string, PairDetails> &pairDetailsMap);
std::string GetTagName (FunctionInfo &request);
SV_ULONGLONG convertGBtoBytes (const double totalSize);
bool GetTotalCapacity(const std::list<std::string> &addVolumeList , 
											 SV_ULONGLONG &protectedCapacity);
bool GetTotalFileSystemUsedSpace(const std::list<std::string> &eligibleVolumes,
										 SV_ULONGLONG &fileSystemUsedSpace);
SV_ULONGLONG GetProtectedVolumeOverHead (void);
void GetCapacityAndFsOccupiedSize( const std::list<std::string>& eligiblevolumes,
								   SV_ULONGLONG& totalCapacity, 
								   SV_ULONGLONG& totalFsOccupiedSize );
double TotalSpaceRequiredWithCompression (const double volPackSize,const double totalCDPSpaceNoCompression,
										  const double ntfsOverHead, const double inmageSpace);
double GetVolpackSizeWithOutCompression (const double anticipatedNonOverlappedIOSize ,const double fileSystemOccupiedSize ,const double dltotalProtectedCapacity);
bool GetReplicationOptions(FunctionInfo& request, ReplicationOptions& replicationOptions);
bool GetReplicationOptionsForUpdateRequest(FunctionInfo& request, ReplicationOptions& replicationOptions);
double GetVolpackSizeWithOutCompressionV2 (const double &dlfileSystemSizeOrProtectedCpacity );
double GetVolpackSizeV2 (const double &dlfileSystemSizeOrProtectedCpacity , 
						 const SV_UINT compressVolpacks);
bool bCheckSpaceAvailabilityV2 (SV_ULONGLONG repoFreeSpace,
								SV_ULONGLONG totalProtectedCapacity, 
								SV_ULONGLONG fileSystemOccupiedSize,
								SV_ULONGLONG &requiredSpace,
								double dNoOptimizedDuration,
								double dOptimizedDuration,
								SV_ULONGLONG &protectedVolumeOverHead , 
								bool supportSparseFile ,
								double compressionBenifits = 0.4, 
								double sparseSavingsFactor = 3 ,
								double changeRate = 0.03, 
								double ntfsMetaDataoverHead = 0.13
								); 
bool CheckSpaceAvailability(std::string& repovolumename,
							RetentionPolicy& retPolicy,
							std::list<std::string>& totalProtectVolumes,
							SV_ULONGLONG &requiredSpace,
							SV_ULONGLONG &repoFreeSpace,
							double compressionBenifits, 
							double sparseSavingsFactor,
							double changeRate, 
							double ntfsMetaDataoverHead );
void GetRetentionDuration( RetentionPolicy& retPolicy, double &dNoOptimizedDuration, double &dOptimizedDuration );


double GetAnticipatedNonOverlappedIOSize (	double dNoOptimizedDuration , 
											double dOptimizedDuration , 
											double nonOverlappedChangeRate , 
											double dlfileSystemOccupiedSize ) ;
double GetVolpackSizeV2 ( double dlfileSystemOccupiedSize, double dltotalProtectedCapacity , bool supportSparseFile ) ; 
double GetTotalCDPSpace (double dlfileSystemOccupiedSize, 
						 double totalChangeRate,
						 double dNoOptimizedDuration,
						 double sparseSavingFactor,
						 double dOptimizedDuration,
						 double compressionBenifits) ; 
SV_UINT VirtualVolumeCompressValue (); 
bool IsEnoughSpaceInRepoV2 (	SV_ULONGLONG &repoFreeSpace,
								std::list<std::string> &eligibleVolumes,
								SV_ULONGLONG &requiredSpace,
								SV_ULONGLONG &protectedVolumeOverHead ,
								bool supportSparseFile ,
								double compressionBenifits = 0.4, 
								double sparseSavingsFactor = 3 ,
								double changeRate = 0.03, 
								double ntfsMetaDataoverHead = 0.13); 

bool IsEnoughSpaceInRepoV2(	SV_ULONGLONG& repoFreeSpace, 
							SV_ULONGLONG protectedCapacity, 
							SV_ULONGLONG fileSystemOccupiedSize,
							RetentionPolicy& retPolicy, 
							SV_ULONGLONG& requiredSpace, 
							SV_ULONGLONG& protectedVolumeOverHead, 
							bool supportSparseFile ,
							double compressionBenifits = 0.4, 
							double sparseSavingsFactor = 3 ,
							double changeRate = 0.03, 
							double ntfsMetaDataoverHead = 0.13); 

bool IsEnoughSpaceInRepoWithOutXMLReadV2 (	SV_ULONGLONG& repoFreeSpace, 
											SV_ULONGLONG protectedCapacity, 
											SV_ULONGLONG fileSystemOccupiedSize,
											RetentionPolicy& retPolicy, 
											SV_ULONGLONG& requiredSpace, 
											SV_ULONGLONG& protectedVolumeOverHead , 
											bool supportSparseFile ,
											double compressionBenifits = 0.4, 
											double sparseSavingsFactor = 3 ,
											double changeRate = 0.03, 
											double ntfsMetaDataoverHead = 0.13 ); 

bool IsEnoughSpaceInRepoV2( SV_ULONGLONG& repoFreeSpace, 
							std::map<std::string,PairDetails>& pairDetails, 
							SV_ULONGLONG& requiredSpace,
							SV_ULONGLONG & protectedVolumeOverHead, 
							bool supportSparseFile ,
							double compressionBenifits = 0.4, 
							double sparseSavingsFactor = 3 ,
							double changeRate = 0.03, 
							double ntfsMetaDataoverHead = 0.13);
void isSpaceCheckRequired(FunctionInfo& request, 
						  std::string& spaceCheckRequired); 

#endif /* PROTECTION__HANDLER__H */
