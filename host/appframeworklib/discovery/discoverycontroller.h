#ifndef __DISCOVERY__CONTROLLER_H
#define __DISCOVERY__CONTROLLER_H

#include <boost/shared_ptr.hpp>
#include <ace/Task.h>
#include <ace/Mutex.h>

#ifdef SV_WINDOWS
#include "win32/hostdiscovery.h"
#endif

typedef ACE_Message_Queue<ACE_MT_SYNCH> ACE_SHARED_MQ ;

class DiscoveryController ;
typedef boost::shared_ptr<DiscoveryController> DiscoveryControllerPtr ;
class DiscoveryController : public ACE_Task<ACE_MT_SYNCH>
{
    void prepareInstalledAppsList(std::list<InmProtectedAppType>& apps) ;
    ACE_SHARED_MQ m_MsgQueue ;
    std::set<std::string> m_selectedApplicationNameSet;
public:
    ACE_SHARED_MQ& Queue() ;
	boost::shared_ptr<DiscoveryInfo> m_disCoveryInfo;
#ifdef SV_WINDOWS
	HostLevelDiscoveryInfo m_hostLevelDiscoveryInfo;
	HostLevelDiscoveryInfo GetHostLevelDiscoveryInfo()
	{
		return m_hostLevelDiscoveryInfo;
	}
	void updateHostLevelDiscoveryInfo();
#endif
	std::list<ReadinessCheckInfo> m_ReadinesscheckInfoList;
    DiscoveryController(ACE_Thread_Manager* threadManager) ;
	virtual int open(void *args = 0) ;
    virtual int close(u_long flags = 0) ;
    virtual int svc() ;
	SVERROR init(const std::set<std::string>& selectedApplicationNameSet) ;
    //void DoWork() ;
    void ProcessMsg(SV_ULONG policyId) ;
    void ProcessRequests() ;
    void PostMsg(int priority, SV_ULONG policyId) ;
    static bool createInstance() ;
    static DiscoveryControllerPtr getInstance() ;
    static DiscoveryControllerPtr m_discoveryControllerPtr ;
    void start() ;
	SVERROR UpdateToCx(SV_ULONG policyId);
} ;


#endif
