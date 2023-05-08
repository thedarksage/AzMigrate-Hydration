#ifndef RESYNC_HANDLER_H_
#define RESYNC_HANDLER_H_

#include "Handler.h"
#include "confschema.h"
#include "protectionpairobject.h"
#include "volumesobject.h"
#include "inmapihandlerdefs.h"

class ResyncHandler :
	public Handler
{
public:  
    ResyncHandler();
	~ResyncHandler(void);
	virtual INM_ERROR process(FunctionInfo& f);
	virtual INM_ERROR validate(FunctionInfo& f);
  
    INM_ERROR setResyncTransitionStepOneToTwo(FunctionInfo &f);
    INM_ERROR getResyncStartTimeStampFastsync(FunctionInfo &f);
    INM_ERROR sendResyncStartTimeStampFastsync(FunctionInfo &f);
    INM_ERROR sendResyncEndTimeStampFastsync(FunctionInfo &f);
    INM_ERROR getResyncEndTimeStampFastsync(FunctionInfo &f);
    INM_ERROR setLastResyncOffsetForDirectSync(FunctionInfo &f);
    INM_ERROR getClearDiffs(FunctionInfo &f);
    INM_ERROR setResyncProgressFastsync(FunctionInfo &f);
private:
    void ValidateAndUpdateRequestPGForLastResyncOffsetUpdate(FunctionInfo &f);    
    void ReadAndUpdateProtectionPairGroupForResynLastOffsetByte(ConstParametersIter_t & srcDeviceNameIter,
                                                                 ConstParametersIter_t & tgtDeviceNameIter,
                                                                 ConstParametersIter_t & offsetIter,
                                                                 ConstParametersIter_t & jobIdIter);
    void UpdateLastResyncOffsetByteForDirectSync(ConstParametersIter_t & srcDeviceNameIter,
                                                                 ConstParametersIter_t & tgtDeviceNameIter,
                                                                 ConstParametersIter_t & offsetIter,
                                                                 ConstParametersIter_t & jobIdIter,
                                                                 ConfSchema::Group & protectionPairGroup);

    void ValidateAndUpdateRequestPGForGetResyncStartTimestampFastSync(FunctionInfo &f,ParameterGroup &responsePG);
    
    void ReadProtectionPairSchemaForResyncStartTimestampFastSync(ConstParametersIter_t & srcDeviceNameIter,
                                                                 ConstParametersIter_t & tgtDeviceNameIter,
                                                                 ConstParametersIter_t & operNameIter,
                                                                 ConstParametersIter_t & jobIdIter,
                                                                 ParameterGroup &responsePG);
    
    void GetResyncStartTimestampForFastSync(ConstParametersIter_t & srcDeviceNameIter,
                                                                 ConstParametersIter_t & tgtDeviceNameIter,
                                                                 ConstParametersIter_t & operNameIter,
                                                                 ConstParametersIter_t & jobIdIter,
                                                                 ConfSchema::Group & protectionPairGroup,
                                                                 ParameterGroup &responsePG);
    void ValidateAndUpdateRequestPGToSetResyncTransition(FunctionInfo &f);

    void ReadAndUpdateProtectionPairSchemaToSetResyncTransition(ConstParametersIter_t & srcDeviceNameIter,
                                                                 ConstParametersIter_t & tgtDeviceNameIter,
                                                                 ConstParametersIter_t & jobIdIter);

    bool UpdateResyncTransitionFromStepOneToStepTwo(ConstParametersIter_t & srcDeviceNameIter,
                                                                 ConstParametersIter_t & tgtDeviceNameIter,
                                                                 ConstParametersIter_t & jobIdIter,
                                                                 ConfSchema::Group & protectionPairGroup,
                                                                 ConfSchema::Group & volumeGroup);


    void ValidateAndUpdateRequestPGToSetStartTimestampForFastSync(FunctionInfo &f);

    void ReadAndUpdateProtectionPairSchemaToSetTimeStampForFastSync(ConstParametersIter_t & srcDeviceNameIter,
                                                                 ConstParametersIter_t & tgtDeviceNameIter,
                                                                 ConstParametersIter_t & timeStampIter,
                                                                 ConstParametersIter_t & seqNoIter,
                                                                 ConstParametersIter_t & jobIdIter);

    void UpdateStartTimeStampForFastSync(ConstParametersIter_t & srcDeviceNameIter,
                                                                 ConstParametersIter_t & tgtDeviceNameIter,
                                                                 ConstParametersIter_t & timeStampIter,
                                                                 ConstParametersIter_t & seqNoIter,
                                                                 ConstParametersIter_t & jobIdIter,
                                                                 ConfSchema::Group & protectionPairGroup);

    void ValidateAndUpdateRequestPGToSetEndTimestampForFastSync(FunctionInfo &f);

    void ReadAndUpdateProtectionPairSchemaToSetEndTimeStampForFastSync(ConstParametersIter_t & srcDeviceNameIter,
                                                                 ConstParametersIter_t & tgtDeviceNameIter,
                                                                 ConstParametersIter_t & timeStampIter,
                                                                 ConstParametersIter_t & seqNoIter,
                                                                 ConstParametersIter_t & jobIdIter);

  
    void UpdateEndimeStampForFastSync(ConstParametersIter_t & srcDeviceNameIter,
                                                                 ConstParametersIter_t & tgtDeviceNameIter,
                                                                 ConstParametersIter_t & timeStampIter,
                                                                 ConstParametersIter_t & seqNoIter,
                                                                 ConstParametersIter_t & jobIdIter,
                                                                 ConfSchema::Group & protectionPairGroup);

    void ValidateAndUpdateRequestPGForGetResyncEndTimestampFastSync(FunctionInfo &f,ParameterGroup &responsePG);
    
    void ReadProtectionPairSchemaForResyncEndTimestampFastSync(ConstParametersIter_t & srcDeviceNameIter,
                                                                 ConstParametersIter_t & tgtDeviceNameIter,
                                                                 ConstParametersIter_t & jobIdIter,
                                                                 ParameterGroup &responsePG);
    
    void GetResyncEndTimestampForFastSync(ConstParametersIter_t & srcDeviceNameIter,
                                                                 ConstParametersIter_t & tgtDeviceNameIter,
                                                                 ConstParametersIter_t & jobIdIter,
                                                                 ConfSchema::Group & protectionPairGroup,
                                                                 ParameterGroup &responsePG);
    void ValidateAndUpdateRequestPGForResyncProgress(FunctionInfo &f);
    void ReadProtectionPairGroupAndUpdateResyncProgess(ConstParametersIter_t & srcDeviceNameIter,
                                                                 ConstParametersIter_t & tgtDeviceNameIter,
                                                                 ConstParametersIter_t & bytesIter,
                                                                 ConstParametersIter_t & matchedIter,
                                                                 ConstParametersIter_t & jobIdIter);
    void UpdateResyncProgess(ConstParametersIter_t & srcDeviceNameIter,
                                                                 ConstParametersIter_t & tgtDeviceNameIter,
                                                                 ConstParametersIter_t & bytesIter,
                                                                 ConstParametersIter_t & matchedIter,
                                                                 ConstParametersIter_t & jobIdIter,
                                                                 ConfSchema::Group & protectionPairGroup);
    SV_LONGLONG GetVolumeCapacity(const std::string & volumeName ,ConfSchema::Group &volumeGroup);


};


#endif /* RESYNC__HANDLER__H_ */
