#ifndef TARGET__HANDLER__H_
#define TARGET__HANDLER__H_

#include "Handler.h"
#include "confschema.h"
#include "protectionpairobject.h"
#include "inmapihandlerdefs.h"

class TargetHandler :
	public Handler
{
public:
	TargetHandler(void);
	~TargetHandler(void);
	virtual INM_ERROR process(FunctionInfo& f);
	virtual INM_ERROR validate(FunctionInfo& f);
    
    INM_ERROR notifyCxDiffsDrained(FunctionInfo& f);
    INM_ERROR cdpStopReplication(FunctionInfo& f);
    INM_ERROR sendEndQuasiStateRequest(FunctionInfo& f);
    INM_ERROR setTargetResyncRequired(FunctionInfo& f);
    INM_ERROR updateVolumeAttribute(FunctionInfo& f);
    INM_ERROR getCurrentVolumeAttribute(FunctionInfo& f);
    INM_ERROR updateCDPInformationV2(FunctionInfo& f);
    INM_ERROR updateCleanUpActionStatus(FunctionInfo& f);
    INM_ERROR updateRetentionInfo(FunctionInfo& f);
    INM_ERROR sendAlertToCx(FunctionInfo& f);
    INM_ERROR setPauseReplicationStatus(FunctionInfo& f);
    INM_ERROR getHostRetentionWindow(FunctionInfo& f);
    INM_ERROR updateCdpDiskUsage(FunctionInfo& f);
	INM_ERROR updateFlushAndHoldWritesPendingStatus(FunctionInfo& f);
	INM_ERROR updateFlushAndHoldResumePendingStatus(FunctionInfo& f);
	INM_ERROR getFlushAndHoldRequestSettings(FunctionInfo& f);
	INM_ERROR pauseReplicationFromHost(FunctionInfo& f);
	INM_ERROR resumeReplicationFromHost(FunctionInfo& f);
private:
    void ValidateAndUpdateRequestPGForEndQuasiState(FunctionInfo &f);
    void ReadAndUpdateProtectionPairGroupForEndQuasiState(ConstParametersIter_t & tgtDeviceNameIter);
    void UpdateEndQuasiState(ConstParametersIter_t & tgtDeviceNameIter,ConfSchema::Group & protectionPairGroup);
    void ValidateAndUpdateRequestPGToSetResyncRequired(FunctionInfo &f);
    void UpdateProtectionPairGroupForToSetResyncRequired(ConstParametersIter_t & tgtDeviceNameIter,
                                                                    ConstParametersIter_t & errorStringIter);
    void UpdateResyncRequiredState(ConstParametersIter_t & tgtDeviceNameIter,
                                              ConstParametersIter_t & errorStringIter,
                                              ConfSchema::Group & protectionPairGroup);
    void ReadProtectionPairGroupAndGetHostRetentionWindow(FunctionInfo &f);
    void GetHostRetentionWindowFromSchema(FunctionInfo &f ,const ConfSchema::Group & protectionPairGroup);
    void FillRetentionWindowPG(ParameterGroup & retentionWindowPG,const ConfSchema::Object  &protectionPairObject);
    void ValidateRequestPGAndUpdateTheCleanpUpAction(FunctionInfo &f);
    void ReadAndUpdateProtectionPairGroupForCleanUpAction(ConstParametersIter_t & tgtDeviceNameIter,
                                                                     ConstParametersIter_t & cleanUpActionStringIter);
    void UpdateProtectionPairGroupForCleanUpAction(ConstParametersIter_t & tgtDeviceNameIter,
                                                              ConstParametersIter_t & cleanUpActionStringIter,
                                                              ConfSchema::Group & protectionPairGroup);
    
    void ValidateAndUpdateRequestPGToSetReplicationStatus(FunctionInfo &f);
    void UpdateProtectionPairGroupToSetPauseReplicationStatus(ConstParametersIter_t & deviceNameIter,
                                                                         ConstParametersIter_t & hostTypeIter,
                                                                        ConstParametersIter_t & actionStringIter);
    void UpdatePauseReplicationStatus(ConstParametersIter_t & deviceNameIter,
                                              ConstParametersIter_t & hostTypeIter,
                                              ConstParametersIter_t & actionStringIter,
                                              ConfSchema::Group & protectionPairGroup);
    void ValidateAndUpdateRequestPGToUpdateCDPInformation(FunctionInfo &f);
    void UpdateProtectionPairGroupForToSetCdpInforamtion(ConstParameterGroupsIter_t & cdpInfoMapIter);
    void UpdateCDPInformationToProrectionPairGroup(ConstParameterGroupsIter_t & cdpInfoMapIter,
                                                              ConfSchema::Group & protectionPairGroup,
                                                              ConfSchema::Group & volumeGroup);
    void UpdateCdpSummaryInfoToProtectionPairGroup(ConfSchema::Object & protectionPairObject,
                                                              ConfSchema::Group &volumeGroup,
                                                              const ParameterGroup & cdpSummaryPG);
    void UpdateCurrentLogSize(ConfSchema::Object & protectionPairObject,const ParameterGroup & cdpSummaryPG);
    void UpdateVolumeFreeSpace(ConfSchema::Object & protectionPairObject,ConfSchema::Group & volumeGroup,const ParameterGroup & cdpSummaryPG);
    void GetCleanUpOptionsString(const std::string & cleanUpActionStr,std::string &cleanUpOptions);
    void UpdateRetentionDiskUsageInfo(const std::string& tgtVolume,
                                      const ParameterGroup& cdpretentiondiskusagepg) ;
    void UpdateTargetDiskUsageInfo(const std::string& tgtVolume,
                                   const ParameterGroup& cdptargetdiskusagePg) ;
    //void getTimeinDisplayFormat( const SV_ULONGLONG& eventTime, std::string& timeInDisplayForm );
   // static unsigned int m_Count;
};


#endif /* TARGET__HANDLER__H_ */
