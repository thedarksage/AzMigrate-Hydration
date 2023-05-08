#include "CxCommunicator.h"
#include "ace/config-all.h"
#include "ace/RW_Mutex.h"
#include "ace/OS_NS_unistd.h"
#include "ace/os_include/os_netdb.h"
#include <exception>

#include "configurator2.h"
#include "fileconfigurator.h"
#include "portable.h"
#include "logger.h"

#include "inmageex.h"

int CxCommunicator::open(void *args)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED CxCommunicator::%s\n",FUNCTION_NAME);
    return this->activate(THR_BOUND);
}

int CxCommunicator::close(u_long flags)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED CxCommunicator::%s\n",FUNCTION_NAME);
    return 0;
}

int CxCommunicator::handle_signal (int signum, siginfo_t * , ucontext_t * )
{
    DebugPrintf("Entered CxCommunicator::handle_signal. Received signal is %d\n", signum);
    if (signum == SIGINT || signum == SIGTERM)
    {
        DebugPrintf("Stopping CxCommunicator Thread. Received signal %d\n", signum);
	quit = true;
	return -1;
    }
    return 0;
}

int CxCommunicator::svc()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED CxCommunicator::%s\n",FUNCTION_NAME);
    HOST_VOLUME_GROUP_SETTINGS m_HostVolumeGroupSettings;
    std::string ret_string;
    Configurator* cxConfigurator = NULL;
    MapLunIdToAtLun mapluntoLunId;
    
    while (!quit)
    {
        try 
        {
            DebugPrintf(SV_LOG_DEBUG,"Fetching settings from Cx.\n");
	    if (!InitializeConfigurator(&cxConfigurator, USE_CACHE_SETTINGS_IF_CX_NOT_AVAILABLE, PHPSerialize))
            {
                DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to instantiate configurator. Retrying...\n");
	        ACE_OS::sleep(5);
	        continue;
            }
            cxConfigurator->GetCurrentSettings();
	    m_HostVolumeGroupSettings = cxConfigurator->getHostVolumeGroupSettings();

        } 

        catch( ContextualException &exception )
        {
            DebugPrintf( SV_LOG_ERROR,
                "CxCommunicator::svc() - function call to CX failed with exception %s\n",
                exception.what() );
	    ACE_OS::sleep(5);
        }
                            
        catch(...) 
        {
            DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to instantiate configurator. Got Exceptiion. Retrying...\n");
	    ACE_OS::sleep(5);
	    continue;
        }

        if ( m_HostVolumeGroupSettings.volumeGroups.size() > 0 )
        {
            mapluntoLunId.clear();
            for( HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator VolumeGroupSettingsIter = m_HostVolumeGroupSettings.volumeGroups.begin(); VolumeGroupSettingsIter != m_HostVolumeGroupSettings.volumeGroups.end(); VolumeGroupSettingsIter++ )
            {
                for(VOLUME_GROUP_SETTINGS::volumes_iterator volumes = VolumeGroupSettingsIter->volumes.begin();volumes != VolumeGroupSettingsIter->volumes.end();volumes++)
                {
                    if( volumes->second.sanVolumeInfo.isSanVolume )
                    {
		        DebugPrintf(SV_LOG_DEBUG,"CxCommunicator::svc - LunId is %s - ATLun is %s\n", volumes->second.sanVolumeInfo.physicalLunId.c_str(), volumes->second.sanVolumeInfo.virtualName.c_str());
                        mapluntoLunId.insert(std::make_pair<std::string, std::string> (volumes->second.sanVolumeInfo.physicalLunId, volumes->second.sanVolumeInfo.virtualName)); 
                    }
                }
            }
        }

        if( mapluntoLunId != m_MapLunIdToAtLun )
        {
            ACE_Write_Guard<ACE_RW_Thread_Mutex> g (m_RWMutex);
	    m_MapLunIdToAtLun = mapluntoLunId;
        }
	
        LocalConfigurator lc;
	unsigned int delay = lc.getVacpToCxDelay();
        DebugPrintf(SV_LOG_DEBUG,"Sleeping for %d seconds...\n", delay);
	delay /= 10;
	while(delay-- > 0 && !quit)
		ACE_OS::sleep(10);
    }
DebugPrintf(SV_LOG_DEBUG,"EXITED CxCommunicator::%s\n",FUNCTION_NAME);
}

std::string CxCommunicator::getAtLun(const std::string& lunId) 
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED CxCommunicator::%s\n",FUNCTION_NAME);

    ACE_Read_Guard<ACE_RW_Thread_Mutex> g (m_RWMutex);
    MapLunIdToAtLun::iterator it = m_MapLunIdToAtLun.find(lunId);
    if( it != m_MapLunIdToAtLun.end() )
    {
        DebugPrintf(SV_LOG_DEBUG,"In CxCommunicator::getAtLun - Found At Lun %s for LunId %s\n", it->second.c_str(), lunId.c_str());
        return it->second;
    }
    DebugPrintf(SV_LOG_DEBUG,"In CxCommunicator::getAtLun - At Lun is not found for LunID %s\n", lunId.c_str());
    return "";
}
