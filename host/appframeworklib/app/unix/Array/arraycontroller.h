#ifndef ARRAY_CONTROLLER_H
#define ARRAY_CONTROLLER_H
#include "app/appcontroller.h"

class ArrayController ;
typedef boost::shared_ptr<ArrayController> ArrayControllerPtr ;

class ArrayController : public AppController
{
    SVERROR Process();
    void DiscoverArray() ;
    SVERROR prepareTarget ( );
    ApplicationPolicy m_policy ;
    static ArrayControllerPtr m_instancePtr ;
    ArrayController(ACE_Thread_Manager*) ;
    SVERROR DiscoverLun(const std::string &str);

    public:
    virtual void ProcessMsg(SV_ULONG policyId) ;
    virtual int open(void *args = 0);
    virtual int close(u_long flags = 0);
    virtual int svc() ;
    static ArrayControllerPtr getInstance(ACE_Thread_Manager* tm) ;
    virtual void PrePareFailoverJobs()
    {}
    virtual void UpdateConfigFiles()
    {}
    virtual bool IsItActiveNode()
    {
        return true;
    }
    virtual void MakeAppServicesAutoStart()
    {}
    virtual void PreClusterReconstruction()
    {}
    virtual bool StartAppServices()
    {
        return true;
    }
    SVERROR configureMirror(const std::string &mSettings, std::string&);
    SVERROR setWriteSplit(const std::string &mSettings, std::string&);
    SVERROR mirrorForceDelete(const std::string &mSettings, std::string&);
    SVERROR deleteWriteSplit(const std::string &mSettings, std::string&);
    SVERROR mirrorMonitor(const std::string &mSettings, std::string&);
    SVERROR monitorWriteSplit(const std::string &mSettings, std::string&);
    SVERROR createLunMap(const std::string &mSettings);
    SVERROR createLunMap(const std::string &mSettings, std::string&);
    SVERROR Scsi3Reserve(const std::string &, std::string&);
    SVERROR mountRetentionLun(const std::string &, std::string&);
    SVERROR deleteLunMap(const std::string &mSettings, std::string&);
    int Run(const std::string &mSettings, std::string&);
    SVERROR executeCommand(const std::string &, std::string &);

    SVERROR OnMirrorConfiguration();
    SVERROR OnExecuteCommand();
    SVERROR OnMirrorForceDelete();
    SVERROR OnPrepareTarget();
    SVERROR OnTargetLunMap();
    SVERROR OnReplicationApplianceHeartbeat();
    SVERROR OnMirrorMonitor();
    typedef SVERROR (ArrayController::*PolicyHandler_t) (void);
    std::map <std::string, PolicyHandler_t> m_policyHandlerMap;
};
#endif
