#ifndef MONITOR__HANDLER__H
#define MONITOR__HANDLER__H

#include "Handler.h"
#include "confschema.h"
#include "confschemamgr.h"
#include "retentioninformation.h"
#include "planobj.h"
#include "appscenarioobject.h"
#include "scenariodetails.h"
#include "protectionpairobject.h"
#include "alertobject.h"
#include "pairsobj.h"
#include "scenarioretentionpolicyobj.h"
#include "scenarioretentionwindowobj.h"

class MonitorHandler :
	public Handler
{
    INM_ERROR validate(FunctionInfo& request) ;
    bool GetErrorCodeFromSystemStatus(const std::string& status,
                                      INM_ERROR& errCode, 
                                      std::string& errMsg,
                                      std::string& protectionstatus) ;

    void GetSystemStatusPg(INM_ERROR errCode,
                           const std::string& errMsg,
                           const std::string& protectionStatus,
                           ParameterGroup& systemStatusPg) ;
    void GetRetentionPolicyPg(ParameterGroup& pg) ;
    void GetConfigureProtectionPolicyPg(ParameterGroup& pg) ;
	void GetLogRotationSettingsPg(ParameterGroup& logrotationSettingsPg) ;
    void GetNonProtectedVolumesPg(ParameterGroup& nonprotectedVolumePg) ;
	void GetProtectedVolumesPg(ParameterGroup& protectedVolumePg); 
    void GetExcludedVolumesPg(ParameterGroup& excludeVolumesPg) ;
    void GetConsistencyOptions(ParameterGroup& consistencyOptions) ;
    void GetReplicationOptionsPg(ParameterGroup& replOptionsPg) ;
    
    void GetVolumeInformationFromScenario(const std::string& srcVolume,
                                          ParameterGroup& volumePg) ;

    void GetRetentionSettingsPg(const std::string& srcVolume, 
                                ParameterGroup& retentionSetttingsPg) ;

    void GetVolumeInformationFromProtection(const std::string& srcVolume,
                                            ParameterGroup& volumePg) ;

    void GetVolumeLevelProtectionInfo(ParameterGroup& responsePg) ;

    void GetCommonRestorePointsPg(const std::list<std::string>& protectedVolumes, ParameterGroup& commonRecoveryInfoPg) ;

    void GetVolumeLevelInformationForLVWPD(ParameterGroup& responsePg, 
                                           const std::list<std::string>& protectedVolumes) ;

    INM_ERROR polulateAlertResponse(FunctionInfo& request,ConfSchema::Group &alertGroup,ConfSchema::Group &tmpAlertGroup) ;
    void SaveEvents(const std::string& dir) ;
    void CheckRepositoryValidity( const std::string& repoDevice, const std::string& sparsefile) ;
    void CollectStatistics(const std::string& tgtPath) ;
    void CollectDriveStatistics( const std::string& volumeName, const std::string& logDir) ;
	bool isAllProtectedVolumesPaused(std::list <std::string> &protectedVolumes);
	INM_ERROR HealthStatus(FunctionInfo& request); 
	void GetIOParameters(ParameterGroup& ioparamsPg) ;
public:
	MonitorHandler(void);
	~MonitorHandler(void);
	virtual INM_ERROR process(FunctionInfo& request);
    INM_ERROR GetErrorLogPath(FunctionInfo& request) ;
    INM_ERROR ListVolumesWithProtectionDetails(FunctionInfo& request);
    INM_ERROR GetProtectionDetails(FunctionInfo& request) ;
    INM_ERROR DownloadAlerts(FunctionInfo& request);
	INM_ERROR DownloadAudit(FunctionInfo& request);
	INM_ERROR polulateAuditResponse(FunctionInfo& request,ConfSchema::Group& auditGroup);
};


#endif /* MONITOR__HANDLER__H */
