
#include "PushInstallService.h"
#include "cdputil.h"

using namespace PI;

void PushInstallService::runPushInstaller()
{
   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    do
    {
        if(m_pushProxyThread.get() == NULL )
        {
            m_pushProxyThread.reset(new PushProxy());
            DebugPrintf(SV_LOG_DEBUG, "Activating the PushProxy Thread..\n ") ;
            m_pushProxyThread->open();
        }
        m_pushProxyThread->thr_mgr()->wait(); 
    }while(!m_pushProxyThread->stopped());    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void PushInstallService::stopPushInstaller()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	m_pushProxyThread->stop();
	CDPUtil::SignalQuit() ;
    m_pushProxyThread->thr_mgr()->wait();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void  PushInstallService::deletePushInstaller()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    if( m_pushProxyThread.get() == NULL )
    {
		m_pushProxyThread.reset(new PushProxy());
    }
    m_pushProxyThread->onServiceDeletion() ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}
