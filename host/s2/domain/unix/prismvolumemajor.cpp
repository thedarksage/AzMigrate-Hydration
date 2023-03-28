#include <cstring>
#include <iostream>
#include <string>
#include <list>
#include <sstream>
#include <exception>
#include <cstdlib>
#include <cmath>
#include <sstream>
#include <fstream>
#include "ace/ACE.h"
#include "ace/Process_Manager.h"
#include "boost/shared_ptr.hpp"
#include "boost/shared_array.hpp"
#include "devicefilter.h"
#include "entity.h"
#include "error.h"
#include "portable.h"
#include "svtypes.h"
#include "globs.h"
#include "s2libsthread.h"
#include "runnable.h"
#include "event.h"
#include "devicestream.h"
#include "resyncrequestor.h"
#include "portableheaders.h"
#include "svdparse.h"
#include "prismvolume.h"
#include "configurator2.h"
#include "configurevxagent.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "deviceidinformer.h"
#include "mirrorerror.h"
#include "mirrorermajor.h"
#include "inmsafecapis.h"

////////////////////
//
extern Configurator* TheConfigurator;

/*
 * FUNCTION NAME :  WakeUpThreads
 *
 * DESCRIPTION :    Issues wake up all threads ioctl
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS/SV_FAILURE
 *
 */
int PrismVolume::WakeUpThreads(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    /* Initialized to success since if m_pDeviceStream 
     * not initialized, we return success */
    int iStatus = SV_SUCCESS;

    if (m_pDeviceStream)
    {
        //calling stop on the replication threads will set the quit event. so just wakeup all those threads which are sleeping.
        if ( m_pDeviceStream->IsOpen() )
        {
            DebugPrintf(SV_LOG_DEBUG,"Sentinel is notifying driver to wake up all threads\n.");
            if ( (iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_WAKEUP_ALL_THREADS) ) != SV_SUCCESS )
            {
                DebugPrintf(SV_LOG_ERROR,"FAILED: Sentinel notification to driver to wake up all threads failed for source LUN ID %s\n.", 
                            GetSourceLUNID().c_str());
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG,"SUCCEEDED: Sentinel notification to driver to wake up all threads succeeded: \n.");
            }
        }
    } /* End of if (m_pDeviceStream) */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    return iStatus;
}


/*
 * FUNCTION NAME :  RegisterEventsWithDriver
 *
 * DESCRIPTION :    Does nothing as of now
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if registration succeeds else SV_FAILURE
 *
 */
int PrismVolume::RegisterEventsWithDriver(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    int iStatus = SV_SUCCESS;

    /* For now nothing in unix */
    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    return iStatus;
}


/*
 * FUNCTION NAME :  WaitForMirrorEvent
 *
 * DESCRIPTION :    Waits for any sort of mirror event; 
 *                  currently resync. 
 *                  TODO: Needs some redesign
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : SV_SUCCESS / SV_FAILURE
 *
 */
int PrismVolume::WaitForMirrorEvent(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    int iStatus = SV_FAILURE;

    inm_resync_notify_info_t rn;
    bool bshouldclearrn = true;

    do
    {
        if (bshouldclearrn)
        {
            memset(&rn, 0, sizeof rn);
            inm_strncpy_s(rn.rsin_src_scsi_id, ARRAYSIZE(rn.rsin_src_scsi_id),m_pPrismVolumeInfo->sourceLUNID.c_str(), INM_MAX_SCSI_ID_SIZE - 1);
            rn.timeout_in_sec = m_WaitTimeForResyncEvent;
        }

        iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_MIRROR_EXCEPTION_NOTIFY, &rn);

        if(iStatus != SV_SUCCESS)
        {
            DebugPrintf(SV_LOG_ERROR, "FAILED: mirror event wait ioctl: %d for source LUN ID %s\n", iStatus, GetSourceLUNID().c_str());
            WaitOnQuitEvent(NSECS_RETRYMIRRORRESYNCEVENT);
        }
        else
        {
            /* TODO: checking this way is not correct in both s2 and s2libs
             * Need to correct in both prism volume and protected volumes (wait db ioctl) */
            if (ETIMEDOUT == m_pDeviceStream->GetErrorCode())
            {
                DebugPrintf(SV_LOG_DEBUG, "INFO: source LUN ID %s. mirror exception notify ioctl timed out\n", 
                            GetSourceLUNID().c_str());
            }
            else if (INM_SET_RESYNC_REQ_FLAG & rn.rsin_flag)
            {
                DebugPrintf(SV_LOG_DEBUG, "INFO: source LUN ID %s. resync required is set by driver\n", GetSourceLUNID().c_str());
                bool bresyncreqdreported = ReportResyncEvent(rn.rsin_resync_err_code, 
                                                             rn.rsin_err_string_resync, 
                                                             rn.rsin_out_of_sync_time_stamp);
                if (bresyncreqdreported)
                {
                    /* TODO: should the INM_SET_RESYNC_REQ_FLAG be cleared ? */
                    rn.rsin_flag |= INM_RESET_RESYNC_REQ_FLAG;
                    /* TODO: should we need to memset inm_resync_notify_info_t except
                     * the flag and the time stamp */
                }
                else
                {
                    WaitOnQuitEvent(NSECS_RETRYMIRRORRESYNCEVENT);
                    /* TODO: should we need to memset whole inm_resync_notify_info_t */
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "mirror exception notify ioctl succeeded (no time out), "
                            " without setting resync required for source LUN ID %s\n", GetSourceLUNID().c_str());
                WaitOnQuitEvent(NSECS_RETRYMIRRORRESYNCEVENT);
            }
        }
 
        bshouldclearrn = (0 == (INM_RESET_RESYNC_REQ_FLAG & rn.rsin_flag));
        
    } while(!IsStopRequested());

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    return iStatus;
}


bool PrismVolume::DoScsiIDsMatchTheGiven(const std::list<std::string> &devices, const std::string &refid)
{
    bool barevalid = !devices.empty();
    DeviceIDInformer f;

    std::list<std::string>::const_iterator siter = devices.begin();
    for ( /* empty */ ; siter != devices.end() ; siter++)
    {
        const std::string &source = *siter;
        std::string id = f.GetID(source);
        if (id.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "could not id for %s with error %s\n", source.c_str(), f.GetErrorMessage().c_str());
        }
        if (id.empty() || (id != refid))
        {
            barevalid = false;
            break;
        }
    }

    return barevalid;
}


