#ifndef ARRAY_CONTROLLER_HB_H
#define ARRAY_CONTROLLER_HB_H
#include "app/appcontroller.h"

class ArrayHBController ;
typedef boost::shared_ptr<ArrayHBController> ArrayHBControllerPtr ;

class ArrayHBController : public AppController
{
    SVERROR Process();
    ApplicationPolicy m_policy ;
    static ArrayHBControllerPtr m_instancePtr ;
    ArrayHBController(ACE_Thread_Manager*) ;

    public:
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
    virtual void ProcessMsg(SV_ULONG policyId) ;
    virtual int open(void *args = 0);
    virtual int close(u_long flags = 0);
    virtual int svc() ;
    static ArrayHBControllerPtr getInstance() ;
    int Run(const std::string &mSettings, std::string&);
};
#endif
