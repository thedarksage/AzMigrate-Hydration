#ifndef SOURCE__HANDLER__H_
#define SOURCE__HANDLER__H_

#include "Handler.h"
#include "confschema.h"
#include "protectionpairobject.h"
#include "inmapihandlerdefs.h"

class SourceHandler :
	public Handler
{
public:
	SourceHandler(void);
	~SourceHandler(void);
	virtual INM_ERROR process(FunctionInfo& f);
	virtual INM_ERROR validate(FunctionInfo& f);
    INM_ERROR updatePauseTrackingInfo(FunctionInfo& f);
    INM_ERROR updateVolumesPendingChanges(FunctionInfo& f);
    INM_ERROR setSourceResyncRequired(FunctionInfo& f);
	INM_ERROR BackupAgentPauseTrack(FunctionInfo& f) ;
	INM_ERROR BackupAgentPause(FunctionInfo& f) ;
private:
    void ValidateRequestPGAndUpdatePauseTrackingInfo(FunctionInfo &f);
    void ReadProtectionPairGroupAndUpdatePauseTrackingInfo(ConstParameterGroupsIter_t & pauseTrackingInfoMapIter);
    void UpdateProtectionPairGroupWithPauseTrackingInfo(ConstParameterGroupsIter_t & pauseTrackingInfoMapIter,
                                                              ConfSchema::Group & protectionPairGroup);
    void ValidateAndUpdateRequestPGToSetResyncRequired(FunctionInfo &f);
    void UpdateResyncRequiredState(ConstParametersIter_t & srcDeviceNameIter,
                                              ConstParametersIter_t & errorStringIter,
                                              ConfSchema::Group & protectionPairGroup);
    void UpdateProtectionPairGroupForToSetResyncRequired(ConstParametersIter_t & srcDeviceNameIter,
                                                                    ConstParametersIter_t & errorStringIter);
    void ValidateAndUpdateRequestPGToUpdatePendingChanges(FunctionInfo &f);
    void UpdateProtectionPairGroupToUpdatePendingChanges(ConstParameterGroupsIter_t & pendingChangesPGIter);
    void UpdatePendingChangesToProrectionPairGroup(ConstParameterGroupsIter_t & pendingChangesPGIter,
                                                   ConfSchema::Group & protectionPairGroup);
    void UpdatePendingChanges(ConfSchema::Object & protectionPairObject,const ParameterGroup & pendingChangesPG);
    std::string GetRPOInDisplayFormat(const SV_ULONGLONG &currentRPO);
};


#endif /* SOURCE__HANDLER__H_ */
