#ifndef SNAPSHOTS_HANDLER
#define SNAPSHOTS_HANDLER
#include "Handler.h"
#include "confschema.h"
#include "inmapihandlerdefs.h"
#include "snapshotsobject.h"
#include "snapshotinstancesobject.h"

class SnapShotHandler : public Handler
{
public:
    INM_ERROR process(FunctionInfo& f) ;
    INM_ERROR notifyCxOnSnapshotStatus(FunctionInfo &f );
    INM_ERROR notifyCxOnSnapshotProgress(FunctionInfo& f);
    INM_ERROR makeSnapshotActive(FunctionInfo& f);
    INM_ERROR notifyCxOnSnapshotCreation(FunctionInfo& f);
    INM_ERROR notifyCxOnSnapshotDeletion(FunctionInfo& f);
    INM_ERROR deleteVirtualSnapshot(FunctionInfo& f);
private:
    INM_ERROR getSrrJobs(FunctionInfo& f) ;
    void FillAppToSchemaAttrs(ConfSchema::SnapshotInstancesObject &snapInsObj, AppToSchemaAttrs_t &apptoschemaattrs);
    void ValidateAndUpdateRequestPGToNotifyCxOnSnapshotStatus(FunctionInfo & f);
    void ReadSnapshotInstanceGroupAndUpdateSnapshotStatus(ConstParametersIter_t & snapshotIdIter,
                                                                 ConstParametersIter_t & timeStringIter,
                                                                 ConstParametersIter_t & vsnapIdIter,
                                                                 ConstParametersIter_t & errMessageIter,
                                                                 ConstParametersIter_t & statusIter);
    void UpdateNotifyCxOnSnapshotStatus(ConstParametersIter_t & snapshotIdIter,
                                                                 ConstParametersIter_t & timeStringIter,
                                                                 ConstParametersIter_t & vsnapIdIter,
                                                                 ConstParametersIter_t & errMessageIter,
                                                                 ConstParametersIter_t & statusIter,
                                                                 ConfSchema::Group & snapInstancesGroup,
                                                                 ConfSchema::Group & volumeGroup);
    void UpdateDeviceStateInVolumeGroup(const std::string & volumeName ,ConfSchema::Group &volumeGroup,bool bDelete=false);
    void ValidateAndUpdateRequestPGNotifyCxOnSnapshotProgress(FunctionInfo & f);
    void ReadSnapshotInstanceGroupAndUpdateSnapshotProgresss(ConstParametersIter_t & vsnapIdIter,
                                                                 ConstParametersIter_t & percentCompleteIter,
                                                                 ConstParametersIter_t & rpointIter);
    void UpdateNotifyCxOnSnapshotProgress(ConstParametersIter_t & vsnapIdIter,
                                                                 ConstParametersIter_t & percentCompleteIter,
                                                                 ConstParametersIter_t & rpointIter,
                                                                 ConfSchema::Group & snapInstancesGroup);
    void ValidateAndUpdateRequestPGToMakeSnapshotActive(FunctionInfo & f);
    void ReadSnapshotInstanceGroupAndMakeSnapshotActive(ConstParametersIter_t & snapshotIdIter);
    void MakeSnapshotActive(ConstParametersIter_t & snapshotIdIter,ConfSchema::Group & snapInstancesGroup);
    void ValidateAndUpdateRequestPGToNotifyCxOnSnapshotCreation(FunctionInfo & f,ParameterGroup& responsePG);
    void ReadSnapshotInstancesGroupAndUpdateToNotifyCxOnSnapshotCreation(ParameterGroupsIter_t & vsnapPersistInfoPGIter,ParameterGroup& responsePG);
    void UpdateToNotifyCxOnSnapshotCreation(ParameterGroupsIter_t & vsnapPersistInfoPGIter,ConfSchema::Group & snapInstancesGroup,AppToSchemaAttrs_t &apptoschemaattrs,ParameterGroup& responsePG);
    void ValidateAndUpdateRequestPGToNotifyCxOnSnapshotDeletion(FunctionInfo & f,ParameterGroup & responsePG);
    void ReadSnapshotInstancesGroupAndUpdateToNotifyCxOnSnapshotDeletion(ParameterGroupsIter_t & vsnapDeleteInfoIter,ParameterGroup & responsePG);
    void UpdateToNotifyCxOnSnapshotDeletion(ParameterGroupsIter_t & vsnapDeleteInfoIter,
                                                         ConfSchema::Group & snapInstancesGroup,
                                                         ParameterGroup & responsePG);
    void ValidateAndUpdateRequestPGToDeleteVirtualSnapshot(FunctionInfo & f);
    void ReadSnapshotInstanceGroupAndDeleteVirtualSnapshot(ConstParametersIter_t & vsnapIdIter);
    void DeleteSnapshots(ConstParametersIter_t & vsnapIdIter,
                                         ConfSchema::Group & snapInstancesGroup,
                                         ConfSchema::Group & snapshotGroup,
                                         ConfSchema::Group & volumeGroup);
} ;
#endif
