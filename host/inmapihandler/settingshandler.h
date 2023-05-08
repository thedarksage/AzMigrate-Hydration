#ifndef SETTINGS__HANDLER__H_
#define SETTINGS__HANDLER__H_

#include "Handler.h"
#include "inmapihandlerdefs.h"
#include "confschema.h"
#include "protectionpairobject.h"
#include "volumesobject.h"
#include "retevtpolicyobject.h"
#include "retlogpolicyobject.h"
#include "retwindowobject.h"




enum PAUSE_TYPES
{
    PAUSE_BY_USER,
    MOVE_RETENTION,
    INSUFFICIENT_DISK_SPACE
};

class SettingsHandler :
	public Handler
{
public:
	SettingsHandler(void);
	~SettingsHandler(void);
	virtual INM_ERROR process(FunctionInfo& f);
	virtual INM_ERROR validate(FunctionInfo& f);
    /* TODO: why are these publics ? */
    /*       compare ignore case through 
     *       c++. not stricmp */
	INM_ERROR GetInitialSettings(FunctionInfo &f);
	INM_ERROR GetCurrentInitialSettingsV2(FunctionInfo &f);
   
private:
    INM_ERROR FillTransportSettings(ConfSchema::Object &protectionpairobj,
                                    ParameterGroup &srcts,
                                    ParameterGroup &tgtts);
    INM_ERROR UpdateInitialSettings(FunctionInfo &f);
    INM_ERROR FillHostVolumeGroupAndCDPSettings(ParameterGroup &hvgpg,
                                                ParameterGroup &cdpspg);
    /* TODO: add CDP_DIRECTION later */
    INM_ERROR UpdateVolumeGroupAndCDPSettings(const int &srcindex,
                                              ConfSchema::Object &protectionpairobj,
                                              ParameterGroup &vgspg, 
                                              ParameterGroup &cdpspg);
    INM_ERROR UpdateVolumeAndCDPSettings(ConfSchema::Object &protectionpairobj, 
                                         ParameterGroup &srcv,
                                         ParameterGroup &tgtv, 
                                         ParameterGroup &cdpspg);
    INM_ERROR FillVolumeSettings(ConfSchema::Object &protectionpairobj,
                                 ConfSchema::Object &srcvolobj,
                                 ConfSchema::Object &tgtvolobj,
                                 ParameterGroup &srcvs,
                                 ParameterGroup &tgtvs);
    INM_ERROR FillCurrentTransportSettings(ParameterGroup &pg);
    INM_ERROR FillInitialSettings(FunctionInfo &f);
    bool SetNeededGroups();
    void FillATLunStatsReq(ParameterGroup &pg);
    void FillThrottleSettings(ParameterGroup &pg);
    void FillSanVolumeInfo(ParameterGroup &pg);
    void GetPairState(ConfSchema::Object &protectionpairobject, int & srcPairState, int& tgtPairState) ;
    INM_ERROR FillCdpSettings(ConfSchema::Object &protectionpairobject,
                              ConfSchema::Object &tgtvolobject,
                              ParameterGroup &cdpsetting);
    INM_ERROR FillCDPPoliciesAndConfInfo(const std::string &tgtname,
                                         ConfSchema::Object &retlogpolicyobj,
                                         ParameterGroup &cdpsetting,
                                         ParameterGroup &cdppolicypg);
    INM_ERROR FillCDPWindowAndConfInfo(const std::string &tgtname, 
                                       ConfSchema::Group &retwindowgrp,
                                       ParameterGroup &cdpsetting,
                                       ParameterGroup &cdppolicypg);
    INM_ERROR FillUserBookMarks(ConfSchema::Object &retwindowobj,
                                ParameterGroup &userbookmarkspg);
    bool GetVersion(FunctionInfo &f, int &version);
    std::string GetCleanUpActionString(ConfSchema::Object &protectionpairobj);
    std::string GetMaintainenceActionString(ConfSchema::Object &protectionpairobj, bool source);
private:
    ConfSchema::Group *m_pProtectionPairGroup;
    ConfSchema::Group *m_pVolumesGroup;
    ConfSchema::ProtectionPairObject m_ProtectionPairObj;
    ConfSchema::VolumesObject m_VolumesObj;
    ConfSchema::RetEventPoliciesObject m_RetEvtPoliciesObj;
    ConfSchema::RetLogPolicyObject m_RetLogPolicyObj;
    ConfSchema::RetWindowObject m_RetWindowObj;
};


#endif /* SETTINGS__HANDLER__H_ */
