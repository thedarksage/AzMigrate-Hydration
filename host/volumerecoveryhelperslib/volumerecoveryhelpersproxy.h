
#ifndef VOLUMERECOVERYHELPERSPROXY_H
#define VOLUMERECOVERYHELPERSPROXY_H

#include "volproxyutil.h"
#include "proxy.h"
#include "serializevolumerecoveryhelperssettings.h"
#include "xmlizevolumerecoveryhelperssettings.h"
#include "volumerecoveryhelperssettings.h"
#include "serializationtype.h"
#include "serializer.h"
#include "APIController.h"
#include "InmXmlParser.h"
#include "portablehelpers.h"
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>


void emptyCdpProgressCallback(CDPMessage const* msg, void *);
struct LicenseInfo {
    bool m_recoverVolumesEnabled;
    bool m_cloneVolumesEnabled;
    bool m_recoverFilesEnabled;
    bool m_hostToTargetEnabled;
};

class VolumeRecoveryProxy;
typedef boost::shared_ptr<VolumeRecoveryProxy> VolumeRecoveryProxyPtr;
enum RESTORE_TYPE
{
	RESTORE_TYPE_TAG,
	RESTORE_TYPE_TIME
} ;

struct RestoreOptions
{
	RESTORE_TYPE restoreType ;
	std::string restorePoint ;
    std::string restorePointInCdpCliFmt ;
	std::string restoreEvent ;
} ;

class VolumeRecoveryProxy
{
private:
	std::string m_cxipAddress;
	int m_port;
	std::string m_repositoryPath;
	bool m_rescueMode ;
	std::string m_agentGuid ;
	SerializeType_t m_serializerType ;
	static VolumeRecoveryProxyPtr m_volrcy_pxyobj;
	VolumeRecoveryProxy(const std::string &ipAddress, 
						int port, 
						bool isRescueMode);
	VolumeRecoveryProxy(std::string const& repositoryPath, 
						bool isRescueMode) ;
	ListVolumesWithProtectionDetails m_ListVolumesWithProtectionDetails ;
public:
	void setConfigStoreLocation(const std::string& agentguid); 
	void GetAllowedOperations(LicenseInfo& licenseInfo) ;
	void setAgentHostId(const std::string& agentGuid) ;
	~VolumeRecoveryProxy();
	static void createInstance(std::string const& ipAddress, int port, bool isRescueMode);
	static void createInstance(std::string repositaryPath, bool isRescueMode);
	static VolumeRecoveryProxyPtr getInstance();

    SVERROR processAPI( FunctionInfo& functInfo, const std::vector<std::string>& acceptableErrorCodes = std::vector<std::string>(1, "0" ) ) ;
    void RepairConfigStoreIfRequired() ;
    SVERROR ListHosts(HostInfoMap& hostinfo);
	VolumeErrors PauseTrackingForVolumeForAgentGUID(PauseResumeDetails& pauseInfo, const std::string &agentGUID);
	VolumeErrors ResumeTrackingForVolumeForAgentGUID(PauseResumeDetails& resumeInfo,
                                const std::string &agentGUID);

	//does not need CX details. Called after export. Need to provided locally mounted repository path. CX details would be ignored.
	SVERROR ImportMBRForAgentGUID(const std::string& RepositaryMountPoint,SV_ULONGLONG RestorePoint,const std::string& agentGUID,std::string& MBRFilepath);

	//GetRestorePoints API is not used for now
	SVERROR GetRestorePoints(const std::string& repositoryPath,std::string& agentGUID,VolumeRestorePointsMap& volrestoremapobj);

	//does not need CX details. Called after export. Need to provided locally mounted repository path. CX details would be ignored.
	VolumeErrors RecoverVolumeToRestorePointForAgentGUID(VolumeRecoveryDetailsMap &recoveryInfo, std::string &agentGUID) ;

	// does not need CX details. This is physical API.
	VolumeErrors SnapshotVolumeToRestorePointForAgentGUID(VolumeSnapshotDetailsMap &snapshotInfo, std::string &agentGUID) ;

	SVERROR ListVolumesWithProtectionDetailsByAgentGUID(ListVolumesWithProtectionDetails& VolProtectionDetails,const std::string& agentGUID) ;
	SVERROR ExportRepositoryOnCIFS(RepoCIFSExportMap_t& repoInfoMap,const std::string& agentGUID="") ;
	SVERROR ExportRepositoryOnNFS(RepoNFSExportMap_t& repoInfoMap,const std::string& agentGUID="") ;

	ExportedRepoDetails GetExportedRepositoryDetailsByAgentGUID(const std::string& repositoryName,
                            const std::string& agentGUID,
                            std::string& localPath);
	VolumeErrors PauseTrackingForAgentGUID(std::string& agentGUID,PauseResumeDetails& pauseInfo);
	VolumeErrors ResumeTrackingForAgentGUID(std::string& agentGUID,PauseResumeDetails& resumeInfo);
	bool DisconnectRepository(const std::string& repositoryName,const std::string& repositoryPath,const std::string& ExportType) ;
    VolumeErrorDetails RecoverVolume(std::string& SrcVol,
									 std::string& snapShotSrcVol,
                                     std::string& snapShotTgtVol,
                                     std::string& retentionPath,
                                     const std::string& srcVisibleCapacity,
                                     const RestoreOptions& restoreOptions,
                                     CDPCli::cdpProgressCallback_t cdpProgressCallback = emptyCdpProgressCallback,
                                     void * cdpProgressCallbackContext = 0,
									 bool fsAwareCopy = true) ;
	VolumeErrorDetails  PerformPhysicalSnapShot(const std::string& srcVol,
												const std::string& snapShotSrcVol, 
												const std::string& snapShotTgtVol,
                                                const std::string& retentionPath, 
												const std::string& srcVisibleCapacity,
                                                const RestoreOptions& restoreOptions,
                                                CDPCli::cdpProgressCallback_t cdpProgressCallback = emptyCdpProgressCallback,
                                                void * cdpProgressCallbackContext = 0,
											    bool fsAwareCopy= true) ;

	VolumeErrorDetails  PerformVirtualSnapShot(std::string& srcVolume,
        std::string& snapShotSrcVol, std::string& snapShotTgtVol,
        std::string& retentionPath,
        const std::string& srcVisibleCapacity,
        const std::string& rawSize,
        const RestoreOptions& restoreOptions) ;

	VolumeErrorDetails DeleteVirtualSnapShot(const std::string& VirtualSnapShotVolume);

	VolumeErrors PauseProtectionForVolumeByAgentGUID(PauseResumeDetails& pauseProInfo,const std::string& agentGUID);
	VolumeErrors ResumeProtectionForVolumeByAgentGUID(PauseResumeDetails& resumeProInfo,const std::string& agentGUID);
    SV_ULONG DisableAutoMount() ;
    SV_ULONG EnableAutoMount() ;
	std::string GetApplicationTagType (const std::string &snapShotTgtVol,const std::string &restoreEvent);
	void GetVolumeProtectionDetails (ListVolumesWithProtectionDetails & VolProtectionDetails, const std::string &agentGUID);
	bool GetFreeDriveLettersForRepoMount(std::list<std::string>& driveList, 
										 const std::string& agentGuid,
										 const std::string& repoPath) ;
	void BringVolumesOnline() ;
	void SendAlert(SV_ALERT_TYPE alertType, const std::string& alertMsg) ;
protected:
	bool ProcessRequestUsingLocalStore(FunctionInfo& funInfo);
	bool ProcessRequestUsingCx(FunctionInfo& funInfo);
	bool Transport(FunctionInfo& funInfo);
	bool createXmlStream(FunctionInfo& funInfo,std::stringstream& ReqXmlStream);
	bool unmountVirtualVolume(std::string& volpackname);
	bool HideVolume(std::string& RecoveryTgtvol);
	SVERROR GetMbrFileList(const std::string& strSourcePath,std::vector<std::string> &files);  
	void SendAlertForRecovery (int start , 
								int recoveryType , 
								int recoveryStatus, 
								std::string formattedSnapShotSrcVol);
} ;

std::string getSourceVolume (std::string &formattedSnapShotSrcVol);

bool RegisterSecurityHandle(const std::string& fqdnusername, 
									  const std::string& password,
									  std::string& errormsg) ;


#endif // VOLUMERECOVERYHELPERSPROXY_H
