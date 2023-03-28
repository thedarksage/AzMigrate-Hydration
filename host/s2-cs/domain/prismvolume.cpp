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
#include "boost/ref.hpp"
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
#include "cxmediator.h"
#include "npwwnif.h"
#include "mirrorerror.h"
#include "mirrorermajor.h"
#include "volumeinfocollector.h"
#include "volumereporter.h"
#include "volumereporterimp.h"
#include "datacacher.h"
#ifdef SV_AIX
#include "filterdrvifminor.h"
#endif

////////////////////
//
extern Configurator* TheConfigurator;

/*
 * FUNCTION NAME :      PrismVolume
 *
 * DESCRIPTION :    Default Constructor
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : NONE
 *
 */
PrismVolume::PrismVolume(void)
    : m_bProtecting(false),
      m_pPrismVolumeInfo(NULL),
      m_ProtectEvent(false, false),
      /* m_ResyncEvent(false,true, NULL, USYNC_PROCESS), */
      m_pDeviceStream(NULL),
      m_liQuitInfo(0),
      m_WaitTimeForResyncEvent(MAX_NSECS_FOR_RESYNC_EVENT),
      m_pDeviceFilterFeatures(NULL),
      m_pHostId(NULL),
      m_pMirrorError(NULL),
      m_pVolumeSummaries(NULL),
      m_WaitTimeForSrcLunsValidity(NSECS_TOWAITFORSRC)
{
}


/*
 * FUNCTION NAME :      PrismVolume
 *
 * DESCRIPTION :    Copy Constructor
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : NONE
 *
 */
PrismVolume::PrismVolume(const PrismVolume& rhs)
    : m_sSourceLUNID(rhs.GetSourceLUNID()),
      /* TODO: should we do new after getting object from rhs ? */
      m_pPrismVolumeInfo(NULL),
      m_bProtecting(false),
      m_pDeviceStream(NULL),
      m_ProtectEvent(rhs.m_ProtectEvent),
      /* m_ResyncEvent(rhs.m_ResyncEvent), */
      m_liQuitInfo(0),
      m_WaitTimeForResyncEvent(MAX_NSECS_FOR_RESYNC_EVENT),
      m_pDeviceFilterFeatures(rhs.m_pDeviceFilterFeatures),
      m_pHostId(rhs.m_pHostId),
      m_pMirrorError(rhs.m_pMirrorError),
      m_pVolumeSummaries(rhs.m_pVolumeSummaries),
      m_WaitTimeForSrcLunsValidity(NSECS_TOWAITFORSRC)
{
}


/*
 * FUNCTION NAME :      PrismVolume
 *
 * DESCRIPTION :    Constructor
 *
 * INPUT PARAMETERS :    prism volume info
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : NONE
 *
 */
PrismVolume::PrismVolume(const PRISM_VOLUME_INFO &PrismVolumeInfo, 
                         const DeviceFilterFeatures *pDeviceFilterFeatures, 
                         const char *pHostId, MirrorError *pMirrorError, 
                         const VolumeSummaries_t *pVolumeSummaries)
    :  m_sSourceLUNID(PrismVolumeInfo.sourceLUNID),
       m_bProtecting(false),
       m_pPrismVolumeInfo(new PRISM_VOLUME_INFO(PrismVolumeInfo)),
       m_ProtectEvent(false, false),
       /* m_ResyncEvent(false,true, NULL, USYNC_PROCESS), */
       m_pDeviceStream(NULL),
       m_liQuitInfo(0),
       m_pDeviceFilterFeatures(pDeviceFilterFeatures),
       m_pHostId(pHostId),
       m_pMirrorError(pMirrorError),
       m_pVolumeSummaries(pVolumeSummaries),
      m_WaitTimeForSrcLunsValidity(NSECS_TOWAITFORSRC)
{
    m_WaitTimeForResyncEvent = GetMirrorResyncEventWaitTime();
}


/*
 * FUNCTION NAME :  WaitOnQuitEvent
 *
 * DESCRIPTION :    Waits on the protect event for specified number of seconds. This is in case
 *                  quit is signalled during wait,the protection thread can exit gracefully
 *
 * INPUT PARAMETERS :  Number of seconds to wait
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns the wait state. refer the wait_state enum data type
 *
 */
WAIT_STATE PrismVolume::WaitOnQuitEvent(long int seconds)
{
    WAIT_STATE waitRet = m_ProtectEvent.Wait(seconds);
    return waitRet;
}


/*
 * FUNCTION NAME :  Stop
 *
 * DESCRIPTION :    This interface is defined in runnable class and is implemented for thread
 *                  stop operation.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns the thread stop status
 *
 */
THREAD_FUNC_RETURN PrismVolume::Stop(const long int PARAM)
{
    DebugPrintf(SV_LOG_DEBUG,"Stopping protection for LUN ID %s. Thread Shutting down..\n",GetSourceLUNID().c_str());
    return (THREAD_FUNC_RETURN) StopProtection(PARAM);
}


/*
 * FUNCTION NAME :
 *
 * DESCRIPTION :
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : NONE
 *
 */
THREAD_FUNC_RETURN PrismVolume::Run()
{
    return (THREAD_FUNC_RETURN) Protect();
}


/*
 * FUNCTION NAME :  StopProtection
 *
 * DESCRIPTION :    Sets the stop protection information to m_liQuitInfo, wakes up the thread in
 *                   case it is waiting on the driver to signal arrival of data
 *
 * INPUT PARAMETERS :  long int liParam stores quit information,signalling whether quit is called for immediately
 *                      or quit is called as timout has occurred,or a quit request is called
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS/SV_FAILURE
 *
 */
int PrismVolume::StopProtection(const long int liParam)
{
    int iStatus = SV_SUCCESS;
    m_liQuitInfo = liParam;
    m_ProtectEvent.Signal(true);
    iStatus = WakeUpThreads();

    return iStatus;
}


/*
 * FUNCTION NAME :      ~PrismVolume
 *
 * DESCRIPTION :    Destructor
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : NONE
 *
 */
PrismVolume::~PrismVolume()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    Destroy();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
}


/*
 * FUNCTION NAME :  IsStopRequested
 *
 * DESCRIPTION :    Inspects the stop event state to determine if stop protection is requested
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns true if stop protection is signalled else false
 *
 */
const bool PrismVolume::IsStopRequested() const
{
    return m_ProtectEvent.IsSignalled();
}


/*
 * FUNCTION NAME :  Destroy
 *
 * DESCRIPTION :    Destroy cleans up the prismvolume. Deletes the device stream
 *                  and prism volume info
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : NONE
 *
 */
void PrismVolume::Destroy()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());

    if ( NULL != m_pDeviceStream )
    {
        m_pDeviceStream->Close();
        delete m_pDeviceStream;
    }
    m_pDeviceStream = NULL;

    if ( NULL != m_pPrismVolumeInfo )
        delete m_pPrismVolumeInfo;
    m_pPrismVolumeInfo = NULL;

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
}


/*
 * FUNCTION NAME :  CreateDeviceStream
 *
 * DESCRIPTION :    Creates the device stream used to interact with the inmage filter driver
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
int PrismVolume::CreateDeviceStream()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());

    int iStatus = SV_SUCCESS;
    if ( NULL != m_pDeviceStream )
        delete m_pDeviceStream;

    m_pDeviceStream = NULL;
    Device dev(INMAGE_FILTER_DEVICE_NAME);
    m_pDeviceStream = new DeviceStream(dev);
    iStatus = m_pDeviceStream->Open(DeviceStream::Mode_Open | DeviceStream::Mode_RW | DeviceStream::Mode_ShareRW);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    return iStatus;
}


/*
 * FUNCTION NAME :  Init
 *
 * DESCRIPTION :    Initializes PrismVolume. 
 *                  initializes device streams and for windows initialize the resync notify event
 *
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if initialization completes successfully.
 *
 */
int PrismVolume::Init()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    int iStatus = SV_FAILURE;

    if ( IsInitialized() )
    {
        DebugPrintf(SV_LOG_DEBUG, "Already PrismVolume is initialized\n");
        iStatus = SV_SUCCESS;
    }
	else if (m_pDeviceFilterFeatures->IsMirrorSupported())
    {
		DebugPrintf(SV_LOG_DEBUG, "mirroring supported: %s for source LUN ID %s\n", m_pDeviceFilterFeatures->IsMirrorSupported()? YES : NO,
                                  GetSourceLUNID().c_str());
        iStatus = CreateDeviceStream();
        if (SV_SUCCESS == iStatus)
        {
            iStatus = RegisterEventsWithDriver();
        }
        if (SV_SUCCESS == iStatus)
        {
			m_WaitTimeForSrcLunsValidity = TheConfigurator->getVxAgent().getWaitTimeForSrcLunsValidity() ;
            m_bInitialized = true;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "involflt driver does not support mirroring but CX has asked mirroring for "
                                  "source LUN ID %s\n", GetSourceLUNID().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    return iStatus;
}

/*
 * FUNCTION NAME :  IsProtecting
 *
 * DESCRIPTION :    Inspects protection status
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns true is volume is protected else false
 *
 */
const bool PrismVolume::IsProtecting() const
{
    return m_bProtecting;
}

/*
 * FUNCTION NAME :  GetPrismVolumeInfo
 *
 * DESCRIPTION :    stores the PRISM_VOLUME_INFO instance
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns the instance of PRISM_VOLUME_INFO
 *
 */
const PRISM_VOLUME_INFO& PrismVolume::GetPrismVolumeInfo() const
{
    return *m_pPrismVolumeInfo;
}

/*
 * FUNCTION NAME :  Protect
 *
 * DESCRIPTION :    Commences protection of volume. Checks if initialization is complete in a loop.
 *                  Issues a start filtering ioctl,such that differentials will be drained in profiling mode too.
 *                  Starts replication when cdp direction is SOURCE.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCESS/SV_FAILURE
 *
 */
int PrismVolume::Protect()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    int iStatus = SV_SUCCESS;

    while( !IsStopRequested() && (!IsInitialized()))
    {
        if(SV_SUCCESS != Init())
        {
            DebugPrintf(SV_LOG_ERROR, 
            "The source LUN ID: %s is not initialized." 
            "Retrying after %lu seconds\n", GetSourceLUNID().c_str(), 
            NSECS_RETRYINIT);
        }
        if ( !IsInitialized() )
        {
            WaitOnQuitEvent(NSECS_RETRYINIT);
        }
        else
        {
            break;
        }
    }

    if (!IsStopRequested() && (true == IsInitialized()) && (false == IsProtecting()))
    {
        m_bProtecting = true;
        iStatus = Mirror();
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    return iStatus;
}


/*
 * FUNCTION NAME :
 *
 * DESCRIPTION :
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : NONE
 *
 */
int PrismVolume::ReturnHome()
{
    // PENDING: Maybe when return home is implemented this might be used.
    assert(!"Not Implemented as yet.");
    return SV_FAILURE;
}


/*
 * FUNCTION NAME :  ShouldQuit
 *
 * DESCRIPTION :    Checks if sentinel can exit. In case it is throttled then it returns
 *                  true if exit is signalled.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns true if sentinel can exit else false
 *
 */
bool PrismVolume::ShouldQuit() const
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    if ( IsStopRequested() && (PROCESS_QUIT_IMMEDIATELY == m_liQuitInfo) )
    {
        return true;
    }
    else if ( IsStopRequested() && (PROCESS_QUIT_TIME_OUT == m_liQuitInfo) )
    {
        return true;
    }
    else if ( IsStopRequested() && (PROCESS_QUIT_REQUESTED == m_liQuitInfo) )
    {
        return true;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    return false;
}


/*
 * FUNCTION NAME :  GetSourceLUNID
 *
 * DESCRIPTION :
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns the name of the volume being protected as a std::string
 *
 */
const std::string& PrismVolume::GetSourceLUNID(void) const
{
    return m_sSourceLUNID;
}


/*
 * FUNCTION NAME :  GetMirrorResyncEventWaitTime
 *
 * DESCRIPTION :    Acquires the time to wait before requesting driver for dirty block,
 *                  If there are no dirty blocks for processings.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : returns the number of seconds to wait as a on unsingned long
 *
 */
unsigned long PrismVolume::GetMirrorResyncEventWaitTime(void)
{
    unsigned long waitTime = TheConfigurator->getVxAgent().getMirrorResyncEventWaitTime() ;
    if (0 == waitTime)
    {
        /* TODO: should this be assigned to MIN value ? */
        waitTime = MAX_NSECS_FOR_RESYNC_EVENT;
    }
    return waitTime;
}


int PrismVolume::UpdateMirrorState(
                                   const PRISM_VOLUME_INFO::MIRROR_ERROR errcode,
                                   const std::string &errstring,
                                   const bool &bshouldretryuntilquit,
                                   const unsigned long nsecstowaitforretry
                                  )
{
    int iStatus = SV_FAILURE;
    CxMediator cxmr;

    std::stringstream msg;
    msg << "mirror request state " << m_pPrismVolumeInfo->mirrorState 
        << ", errcode " << errcode << " for source LUN ID " 
        << GetSourceLUNID();
    do
    { 
        iStatus = cxmr.UpdateMirrorState(TheConfigurator, 
                                         m_pPrismVolumeInfo->sourceLUNID, 
                                         m_pPrismVolumeInfo->mirrorState, 
                                         errcode, 
                                         errstring);

        if(iStatus != SV_SUCCESS)
        {
            DebugPrintf(SV_LOG_ERROR, "FAILED: updating %s to CX\n", msg.str().c_str());

            if (bshouldretryuntilquit)
            {
                /* TODO: Should this be WARNING ? */
                DebugPrintf(SV_LOG_ERROR, "Retrying after %lu seconds.\n", nsecstowaitforretry);
                WaitOnQuitEvent(nsecstowaitforretry);
            }                
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "succeeded in updating %s to CX\n", msg.str().c_str());
            break;
        }
    } while (!IsStopRequested() && bshouldretryuntilquit);

    return iStatus;
}


/*
 * FUNCTION NAME :  ReportResyncEvent
 *
 * DESCRIPTION :    Reports to the cx if a resync required flag is set in the dirty block being processed.
 *                  Reports the err code and the err msg along with the volume name, if resync required flag is set.
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : NONE
 *
 */
bool PrismVolume::ReportResyncEvent(const SV_ULONGLONG ullOutOfSyncErrorCode,
                                    const char *pErrorStringForResync, 
                                    const SV_ULONGLONG ullOutOfSyncTS)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    bool bReportedSuccessfully = false;
    ResyncRequestor resyncRequestor;

    if(resyncRequestor.ReportPrismResyncRequired(TheConfigurator,
                                                 GetSourceLUNID(),
                                                 ullOutOfSyncTS,
                                                 ullOutOfSyncErrorCode,
                                                 pErrorStringForResync))
    {
        bReportedSuccessfully = true;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,
                    "FAILED: source LUN ID: %s. Unable to report resync required to CX.\n",
                    GetSourceLUNID().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    return bReportedSuccessfully;
}


/* TODO: printing volume info should be member
 *       function of PrismVolumeInfo */
void PrismVolume::PrintPrismVolumeInfo(void)
{
    std::stringstream msg;
    msg << ", source lun capacity: " << m_pPrismVolumeInfo->sourceLUNCapacity 
        << ", mirror state: " << m_pPrismVolumeInfo->mirrorState
        << ", at lun number: " << m_pPrismVolumeInfo->applianceTargetLUNNumber;
    DebugPrintf(SV_LOG_DEBUG, "Prism Volume Info:\n");
    DebugPrintf(SV_LOG_DEBUG, "source lun id: %s\n", m_pPrismVolumeInfo->sourceLUNID.c_str());
    DebugPrintf(SV_LOG_DEBUG, "%s\n", msg.str().c_str());
    DebugPrintf(SV_LOG_DEBUG, "source lun names:\n");
    for_each(m_pPrismVolumeInfo->sourceLUNNames.begin(), m_pPrismVolumeInfo->sourceLUNNames.end(), PrintString);
    DebugPrintf(SV_LOG_DEBUG, "at lun name: %s\n", m_pPrismVolumeInfo->applianceTargetLUNName.c_str());
    DebugPrintf(SV_LOG_DEBUG, "PI wwns:\n");
    for_each(m_pPrismVolumeInfo->physicalInitiatorPWWNs.begin(), m_pPrismVolumeInfo->physicalInitiatorPWWNs.end(), PrintString);
    DebugPrintf(SV_LOG_DEBUG, "AT wwns:\n");
    for_each(m_pPrismVolumeInfo->applianceTargetPWWNs.begin(), m_pPrismVolumeInfo->applianceTargetPWWNs.end(), PrintString);
}


unsigned char PrismVolume::GetPrismVolumeInfoValidStatus(const VolumeSummaries_t &vs, 
                                                         VolumeReporter::VolumeReport_t &vr, 
                                                         PRISM_VOLUME_INFO::MIRROR_ERROR *perrcode)
{
    PrintPrismVolumeInfo();
    unsigned char status = 0;

    if (m_pPrismVolumeInfo->sourceLUNID.empty())
    {
        status |= PRISM_VOLUME_INFO::SOURCE_LUN_ID_INVALID;
        /* settings have to change */
        *perrcode = PRISM_VOLUME_INFO::SrcLunInvalid;
    }
    if (0 == m_pPrismVolumeInfo->sourceLUNCapacity)
    {
        status |= PRISM_VOLUME_INFO::SOURCE_LUN_CAPACITY_INVALID;
        /* settings have to change */
        *perrcode = PRISM_VOLUME_INFO::SrcCapacityInvalid;
    }
    if (m_pPrismVolumeInfo->applianceTargetLUNName.empty())
    {
        status |= PRISM_VOLUME_INFO::ATLUN_NAME_INVALID;
        /* settings have to change */
        *perrcode = PRISM_VOLUME_INFO::ATLunNameInvalid;
    }
    if (m_pPrismVolumeInfo->physicalInitiatorPWWNs.empty())
    {
        status |= PRISM_VOLUME_INFO::PI_PWWNS_INVALID;
        /* settings have to change */
        *perrcode = PRISM_VOLUME_INFO::PIPwwnsInvalid;
    }
    if (m_pPrismVolumeInfo->applianceTargetPWWNs.empty())
    {
        status |= PRISM_VOLUME_INFO::AT_PWWNS_INVALID;
        /* settings have to change */
        *perrcode = PRISM_VOLUME_INFO::ATPwwnsInvalid;
    }
    switch (m_pPrismVolumeInfo->mirrorState)
    {
        case PRISM_VOLUME_INFO::MIRROR_SETUP_PENDING:    /* fall through */
        case PRISM_VOLUME_INFO::MIRROR_SETUP_COMPLETED:
        case PRISM_VOLUME_INFO::MIRROR_DELETE_PENDING:
        case PRISM_VOLUME_INFO::MIRROR_SETUP_PENDING_RESYNC_CLEARED:
            break;
        default:
            status |= PRISM_VOLUME_INFO::MIRROR_STATE_INVALID;
            /* settings have to change */
            *perrcode = PRISM_VOLUME_INFO::MirrorStateInvalid;
            break;
    }
    /* NOTE: order is important here since perrcode is 
     * being used by caller to once again do volume collection
     * or not */
    status |= GetSourceVolumeReport(vs, vr, perrcode) ? 0 : PRISM_VOLUME_INFO::SOURCE_LUN_NAMES_INVALID;

    return status;
}


/*
 * FUNCTION NAME :  Mirror
 *
 * DESCRIPTION :    Does the main job of mirror setups, delete and resync wait
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
int PrismVolume::Mirror(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    Mirrorer *pmirrorer = 0;
    VolumeReporter::VolumeReport_t vr;

    int iStatus = AreSettingsValid(vr); 

    if ((!IsStopRequested()) && (SV_SUCCESS == iStatus) && 
        ((PRISM_VOLUME_INFO::MIRROR_SETUP_PENDING == m_pPrismVolumeInfo->mirrorState) ||  
        (PRISM_VOLUME_INFO::MIRROR_SETUP_PENDING_RESYNC_CLEARED == m_pPrismVolumeInfo->mirrorState) || 
        (PRISM_VOLUME_INFO::MIRROR_DELETE_PENDING == m_pPrismVolumeInfo->mirrorState)))
    {
        try
        {
            pmirrorer = new MirrorerMajor;
        }
        catch (std::bad_alloc)
        {
            iStatus = SV_FAILURE;
            DebugPrintf(SV_LOG_ERROR, "Failed to allocate memory for mirrorer\n");
        }
    }
  
    /* TODO: add all checks later */
    if ((!IsStopRequested()) && (SV_SUCCESS == iStatus) && 
        ((PRISM_VOLUME_INFO::MIRROR_SETUP_PENDING == m_pPrismVolumeInfo->mirrorState) ||  
        (PRISM_VOLUME_INFO::MIRROR_SETUP_PENDING_RESYNC_CLEARED == m_pPrismVolumeInfo->mirrorState)))
    {
        iStatus = SetupMirror(pmirrorer, vr);
        if (SV_SUCCESS == iStatus)
        {
            /* If we succeed sending completed status to CX, then
            * we can change the status to PRISM_VOLUME_INFO::MIRROR_SETUP_COMPLETED, and
            * wait for resync event; later settings will change we fall in completed state 
            * next time we start (or), should we return from here for CX to change state ? 
            * This ioctl is idempotent */
            /* TODO: should we do this ? looks like we should not ? may be CX might do some state changes ?   */
            m_pPrismVolumeInfo->mirrorState = PRISM_VOLUME_INFO::MIRROR_SETUP_COMPLETED;
        }
    }
 
    if ((!IsStopRequested()) && (SV_SUCCESS == iStatus) &&
        (PRISM_VOLUME_INFO::MIRROR_SETUP_COMPLETED == m_pPrismVolumeInfo->mirrorState))
    {
        /* Wait for resync */
        iStatus = WaitForMirrorEvent();
    }

    if ((!IsStopRequested()) && (SV_SUCCESS == iStatus) && 
        (PRISM_VOLUME_INFO::MIRROR_DELETE_PENDING == m_pPrismVolumeInfo->mirrorState))
    {
        /* We should not ourselves transition from delete completed to cleanup pending */
        /* Should handle EINVAL from driver and assume that deletion did happen. This 
         * is for cases, where in delete happened in driver, but failed to update CX */
        iStatus = DeleteMirror(pmirrorer);
    }

    if (pmirrorer)
    {
        delete pmirrorer;
        pmirrorer = 0;
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    return iStatus;
}


/*
 * FUNCTION NAME :  SetupMirror
 *
 * DESCRIPTION :    Gives mirror create ioctl to driver and 
 *                  sends to CX the status; If there is any 
 *                  failure, it retries after waiting for some
 *                  time until it quits (call IsStopRequested)
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
int PrismVolume::SetupMirror(Mirrorer *pmirrorer, const VolumeReporter::VolumeReport_t &vr)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    int iStatus = SV_FAILURE;

    bool bshouldformioctlinput = false;
    ATLunNames_t atlunnames;
    if (!IsStopRequested())
    {
        bshouldformioctlinput = GetATs(vr, atlunnames);
        if (false == bshouldformioctlinput)
        {
            DebugPrintf(SV_LOG_ERROR, "For %s, mirror setup ioctl cannot be issued since " 
                                      "discovering AT Luns failed\n",
                                      GetSourceLUNID().c_str());
        }
    }

    /* 
    * While giving create mirror ioctl to driver,
    * if mirror state is MIRROR_SETUP_PENDING_RESYNC_CLEARED,
    * then in create ioctl, set this resync flag
    */
    bool bshouldissueioctl = false;
    if (IsStopRequested())
    {
        DebugPrintf(SV_LOG_DEBUG, "quit is requested for source LUN ID %s before forming input to mirror setup ioctl\n", 
                    GetSourceLUNID().c_str());
    }
    else if (bshouldformioctlinput)
    {
        do
        {
            /* message recorded for failure, to
             * send to CX */
            PRISM_VOLUME_INFO::MIRROR_ERROR errcode = PRISM_VOLUME_INFO::NoError;
            iStatus = pmirrorer->FormMirrorSetupInput(*m_pPrismVolumeInfo, atlunnames, vr, &errcode); 
            if (SV_SUCCESS == iStatus)
            {
                bshouldissueioctl = true;
                break;
            }
            else
            {
                pmirrorer->RemoveMirrorSetupInput();
                std::string errstr = m_pMirrorError->GetMirrorErrorMessage(errcode);
                DebugPrintf(SV_LOG_ERROR, "Filling mirror configuration information failed with %s, for source LUN ID %s\n",
                                          errstr.c_str(), GetSourceLUNID().c_str());
                bool bshouldretrysenduntilquit = false;
                unsigned long nsecstowaitforretry = 0;
                if (ERR_AGAIN != iStatus)
                {
                    bshouldretrysenduntilquit = true;
                    nsecstowaitforretry = NSECS_RETRY_SEND_MIRRORSTATE;
                }
                int iupdatestatus = UpdateMirrorState(errcode, errstr, bshouldretrysenduntilquit, nsecstowaitforretry);
                if (SV_SUCCESS != iupdatestatus)
                {
                    DebugPrintf(SV_LOG_ERROR, "FAILED: updating mirror state as MIRROR_SETUP_FAILED to CX for source LUN ID %s\n",
                                              GetSourceLUNID().c_str());
                }
                if (ERR_AGAIN == iStatus)
                {
                    /* like memory allocation failures etc., */
                    DebugPrintf(SV_LOG_ERROR, "Retrying to form mirror setup ioclt input after %lu seconds\n", NSECS_RETRYMIRRORSETUP);
                    WaitOnQuitEvent(NSECS_RETRYMIRRORSETUP);
                }
                else
                {
                    /* do not retry if not asked by FormMirrorSetupInput; 
                     * since this can be due to invalid prism volume info */
                    break;
                }
            }
        } while (!IsStopRequested());
    }

#ifdef SV_AIX
    if (IsStopRequested())
    {
        DebugPrintf(SV_LOG_DEBUG, "quit is requested for source LUN ID %s before giving max transfer size to involflt\n", 
                    GetSourceLUNID().c_str());
    }
    else if (bshouldissueioctl)
    {
        bshouldissueioctl = false;
        DebugPrintf(SV_LOG_DEBUG, "For %s, proceeding to give max transfer size to involflt\n",
                                  GetSourceLUNID().c_str());
        do
        {
            FilterDrvIfMinor drvifminor;

            for (std::list<std::string>::const_iterator lunit = m_pPrismVolumeInfo->sourceLUNNames.begin(); 
                 lunit != m_pPrismVolumeInfo->sourceLUNNames.end();
                 lunit++)
            {
                iStatus = drvifminor.PassMaxTransferSizeIfNeeded(m_pDeviceFilterFeatures, *lunit, *m_pDeviceStream, vr);
                if (SV_SUCCESS != iStatus)
                {
                    break;
                }
            }

            if(iStatus != SV_SUCCESS)
            {
                /* TODO: add exception with cx later */
                PRISM_VOLUME_INFO::MIRROR_ERROR errcode = PRISM_VOLUME_INFO::MirrorErrorUnknown;
                std::string errstr = m_pMirrorError->GetMirrorErrorMessage(errcode);
                DebugPrintf(SV_LOG_ERROR, "FAILED: giving max transfer size to involflt, for source LUN ID %s with error message %s\n", 
                                          GetSourceLUNID().c_str(), errstr.c_str()); 
                bool bshouldretryuntilquit = false;
                int iUpdateStatus = UpdateMirrorState(errcode, errstr, bshouldretryuntilquit, 0);
                if (SV_SUCCESS != iUpdateStatus)
                {
                    DebugPrintf(SV_LOG_ERROR, "FAILED: updating mirror state as error to CX failed for source LUN ID %s,as max transfer size ioctl failed\n",
                                              GetSourceLUNID().c_str());
                }
                DebugPrintf(SV_LOG_ERROR, "Retrying giving max transfer size to involflt setup after %lu seconds\n", NSECS_RETRYMIRRORSETUP);
                WaitOnQuitEvent(NSECS_RETRYMIRRORSETUP);
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "giving max transfer size to involflt succeeded for source LUN ID %s\n", GetSourceLUNID().c_str());
                bshouldissueioctl = true;
                break;
            }
        } while(!IsStopRequested());
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "For %s, mirror setup ioctl cannot be issued since " 
                                  "could not form its input\n",
                                  GetSourceLUNID().c_str());
    }
#endif

    if (IsStopRequested())
    {
        DebugPrintf(SV_LOG_DEBUG, "quit is requested for source LUN ID %s before issuing mirror set up ioctl\n", 
                    GetSourceLUNID().c_str());
    }
    else if (bshouldissueioctl)
    {
        DebugPrintf(SV_LOG_DEBUG, "For %s, proceeding to issue mirror setup ioctl\n",
                                  GetSourceLUNID().c_str());
        do
        {
            PRISM_VOLUME_INFO::MIRROR_ERROR errcode = PRISM_VOLUME_INFO::NoError;
            iStatus = pmirrorer->SetupMirror(*m_pPrismVolumeInfo, *m_pDeviceStream, &errcode);
            if(iStatus != SV_SUCCESS)
            {
                /* send failure state to CX and need not worry about reached or not
                 * since the next time, also same state will come */
                std::string errstr = m_pMirrorError->GetMirrorErrorMessage(errcode);
                DebugPrintf(SV_LOG_ERROR, "FAILED: mirror setup ioctl: %d for source LUN ID %s with error message %s\n", 
                                          iStatus, GetSourceLUNID().c_str(), errstr.c_str()); 
                bool bshouldretryuntilquit = false;
                int iUpdateStatus = UpdateMirrorState(errcode, errstr, bshouldretryuntilquit, 0);
                if (SV_SUCCESS != iUpdateStatus)
                {
                    DebugPrintf(SV_LOG_ERROR, "FAILED: updating mirror state as MIRROR_SETUP_FAILED to CX for source LUN ID %s.\n",
                                              GetSourceLUNID().c_str());
                }
                DebugPrintf(SV_LOG_ERROR, "Retrying mirror setup after %lu seconds\n", NSECS_RETRYMIRRORSETUP);
                WaitOnQuitEvent(NSECS_RETRYMIRRORSETUP);
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "mirror setup succeeded for source LUN ID %s\n", GetSourceLUNID().c_str());
                break;
            }
        } while(!IsStopRequested());

        if (!IsStopRequested() && (SV_SUCCESS == iStatus))
        {
            PRISM_VOLUME_INFO::MIRROR_ERROR errcode = PRISM_VOLUME_INFO::NoError;
            std::string errstr;
            bool bshouldretryuntilquit = true;
            iStatus = UpdateMirrorState(errcode, errstr, bshouldretryuntilquit, NSECS_RETRY_SEND_MIRRORSTATE);
            if (SV_SUCCESS != iStatus)
            {
                DebugPrintf(SV_LOG_ERROR, "FAILED: updating mirror state as MIRROR_SETUP_COMPLETED to CX for source LUN ID %s\n",
                                          GetSourceLUNID().c_str());
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "For %s, mirror setup ioctl cannot be issued since " 
                                  "could not form its input\n",
                                  GetSourceLUNID().c_str());
    }

    pmirrorer->RemoveMirrorSetupInput();
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    return iStatus;
}


/*
 * FUNCTION NAME :  DeleteMirror
 *
 * DESCRIPTION :    Gives mirror delete ioctl to driver and 
 *                  sends to CX the status; If there is any 
 *                  failure, it retries after waiting for some
 *                  time until it quits (call IsStopRequested)
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
int PrismVolume::DeleteMirror(Mirrorer *pmirrorer)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    int iStatus = SV_FAILURE;

    if (!IsStopRequested())
    {
        /* first always do delete mirroring */
        do
        {
            PRISM_VOLUME_INFO::MIRROR_ERROR errcode = PRISM_VOLUME_INFO::NoError;
            iStatus = pmirrorer->DeleteMirror(*m_pPrismVolumeInfo, *m_pDeviceStream, &errcode);
            if(iStatus != SV_SUCCESS)
            {
                DebugPrintf(SV_LOG_ERROR, "FAILED: mirror delete ioctl: %d for source LUN ID %s\n", iStatus, GetSourceLUNID().c_str());
                /* send failure state to CX and need not worry about reached or not
                 * since the next time, also same state will come */
                std::string errstr = m_pMirrorError->GetMirrorErrorMessage(errcode);
                bool bshouldretryuntilquit = false;
                int iUpdateStatus = UpdateMirrorState(errcode, errstr, bshouldretryuntilquit, 0);
                if (SV_SUCCESS != iUpdateStatus)
                {
                    DebugPrintf(SV_LOG_ERROR, "FAILED: updating mirror state as MIRROR_DELETE_FAILED to CX for source LUN ID %s\n",
                                              GetSourceLUNID().c_str());
                }
                DebugPrintf(SV_LOG_ERROR, "Retrying mirror setup after %lu seconds\n", NSECS_RETRYMIRRORDELETE);
                WaitOnQuitEvent(NSECS_RETRYMIRRORDELETE);
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "mirror delete ioctl succeeded for source LUN ID %s\n", GetSourceLUNID().c_str());
                break;
            }
        } while(!IsStopRequested());
        
        if (!IsStopRequested())
        {
            if (SV_SUCCESS == iStatus)
            {
                std::list<std::string> applianceTargetPWWNs;
                std::list<std::string> applianceNetworkAddress;

                for (std::list<std::string>::iterator atListIter = m_pPrismVolumeInfo->applianceTargetPWWNs.begin();
                        atListIter != m_pPrismVolumeInfo->applianceTargetPWWNs.end(); atListIter++)
                {
                    std::string applianceTargetPwwn = *atListIter;

                    std::size_t found = applianceTargetPwwn.find(";");
                    if (found == std::string::npos)
                        applianceTargetPWWNs.push_back(applianceTargetPwwn);
                    else
                    {
                        applianceTargetPWWNs.push_back(applianceTargetPwwn.substr(0, found));

                        std::size_t found1 = applianceTargetPwwn.find(";",found+1);
                        while (found1 != std::string::npos)
                        {
                            std::string ipAddress = applianceTargetPwwn.substr(found+1, found1);
                            applianceNetworkAddress.push_back(ipAddress);
                            found=found1;
                            found1 = applianceTargetPwwn.find(";",found+1);
                        }

                        if (found1 == std::string::npos)
                        {
                            std::string ipAddress = applianceTargetPwwn.substr(found+1, found1);
                            applianceNetworkAddress.push_back(ipAddress);
                        }

                    }
                }

                /* deletion is always p0 not s2 */
                PIAT_LUN_INFO piatluninfo(m_pPrismVolumeInfo->sourceLUNID,
                                          m_pPrismVolumeInfo->applianceTargetLUNNumber,
                                          m_pPrismVolumeInfo->applianceTargetLUNName,
                                          m_pPrismVolumeInfo->physicalInitiatorPWWNs,
                                          applianceTargetPWWNs,
                                          VolumeSummary::DISK,
                                          applianceNetworkAddress);

                bool bdeletedatluns = false;
                DebugPrintf(SV_LOG_DEBUG, "Going to delete AT Luns for source LUN ID %s\n", GetSourceLUNID().c_str());
                do
                {
                    PRISM_VOLUME_INFO::MIRROR_ERROR errcode = PRISM_VOLUME_INFO::NoError;
                    std::string errstr;

                    bdeletedatluns = DeleteATLunNames(piatluninfo, boost::ref(m_ProtectEvent));
                    if (IsStopRequested())
                    {
                        break;
                    }
                    if (bdeletedatluns)
                    {
                        DebugPrintf(SV_LOG_DEBUG, "deleted AT Lun names for %s\n", GetSourceLUNID().c_str());
                        break;
                    }
                    else 
                    {
                        errcode = PRISM_VOLUME_INFO::AtlunDeletionFailed;
                        errstr = m_pMirrorError->GetMirrorErrorMessage(errcode);
                        DebugPrintf(SV_LOG_ERROR, "For %s, deleting AT Luns failed\n",
                                                  GetSourceLUNID().c_str());
                    }

                    bool bshouldretryuntilquit = false;
                    int iUpdateStatus = UpdateMirrorState(errcode, errstr, bshouldretryuntilquit, 0);
                    if (SV_SUCCESS != iUpdateStatus)
                    {
                        DebugPrintf(SV_LOG_ERROR, "FAILED: updating mirror state as MIRROR_DELETE_FAILED to CX for source LUN ID %s.\n",
                                                  GetSourceLUNID().c_str());
                    }
                    /* TODO: should this be error ? */
                    DebugPrintf(SV_LOG_WARNING, "Retrying deleting AT Luns after %lu seconds\n", NSECS_RETRYMIRRORDELETE);
                    WaitOnQuitEvent(NSECS_RETRYMIRRORDELETE);
                     
                } while (!IsStopRequested());

                if (IsStopRequested())
                {
                    iStatus = SV_FAILURE;
                    DebugPrintf(SV_LOG_DEBUG, "quit is requested for source LUN ID %s after "
                                              "mirror delete ioctl succeeded but before doing "
                                              "at luns deletion\n", GetSourceLUNID().c_str());
                }
                else if (bdeletedatluns)
                {
                    PRISM_VOLUME_INFO::MIRROR_ERROR errcode = PRISM_VOLUME_INFO::NoError;
                    std::string errstr;
                    bool bshouldretryuntilquit = true;
                    iStatus = UpdateMirrorState(errcode, errstr, bshouldretryuntilquit, NSECS_RETRY_SEND_MIRRORSTATE);
                    if (SV_SUCCESS != iStatus)
                    {
                        DebugPrintf(SV_LOG_ERROR, "FAILED: updating mirror state as MIRROR_DELETE_COMPLETED to CX for source LUN ID %s\n",
                                                  GetSourceLUNID().c_str());
                    }
                }
            }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "quit is requested for source LUN ID %s while issuing mirror delete ioctl\n", 
                        GetSourceLUNID().c_str());
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "quit is requested for source LUN ID %s before issuing mirror delete ioctl\n", 
                    GetSourceLUNID().c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    return iStatus;
}


bool PrismVolume::GetSourceVolumeReport(const VolumeSummaries_t &vs, 
                                        VolumeReporter::VolumeReport_t &vr, 
                                        PRISM_VOLUME_INFO::MIRROR_ERROR *perrcode)
{
    bool bfilled = false;

    if (!m_pPrismVolumeInfo->sourceLUNNames.empty())
    {
        VolumeReporter *pvr = new VolumeReporterImp();
        vr.m_ReportLevel = VolumeReporter::ReportVolumeLevel;
        std::list<std::string>::const_iterator iter = m_pPrismVolumeInfo->sourceLUNNames.begin();
        const std::string &firstsrc = *iter;
        pvr->GetVolumeReport(firstsrc, vs, vr);
        pvr->PrintVolumeReport(vr);

        bool bismatched = false;
        do
        {
            if (!vr.m_bIsReported)
            {
                DebugPrintf(SV_LOG_ERROR, "source volume %s is not collected by volumeinfocollector for source lun id %s\n", 
                                          firstsrc.c_str(), m_pPrismVolumeInfo->sourceLUNID.c_str());
                /* retry */
                *perrcode = PRISM_VOLUME_INFO::SrcNotReported;
                break;
            }
            if (vr.m_Id != m_pPrismVolumeInfo->sourceLUNID)
            {
                DebugPrintf(SV_LOG_ERROR, "lun id %s of source volume %s does not match lun id %s in settings\n", 
                                          vr.m_Id.c_str(), firstsrc.c_str(), m_pPrismVolumeInfo->sourceLUNID.c_str());
                /* settings have to change */
                *perrcode = PRISM_VOLUME_INFO::SrcLunInvalid;
                break;
            }

            bismatched = true;
            for (iter++; iter != m_pPrismVolumeInfo->sourceLUNNames.end(); iter++)
            {
                const std::string &nextsrc = *iter;
                VolumeReporter::VolumeReport_t nextvr;
                nextvr.m_ReportLevel = vr.m_ReportLevel;
                pvr->GetVolumeReport(nextsrc, vs, nextvr);
                pvr->PrintVolumeReport(nextvr);
                std::stringstream errormsg;
                errormsg << "subsequent source volume " 
                         << nextsrc;
                do 
                {
                    if (!nextvr.m_bIsReported)
                    {
                        errormsg << " not collected by volume info collecter";
                        /* retry */
                        *perrcode = PRISM_VOLUME_INFO::SrcNotReported;
                        bismatched = false;
                        break;
                    }
                    bismatched = (nextvr.m_Id == vr.m_Id);
                    if (false == bismatched)
                    {
                        errormsg << " id: " << nextvr.m_Id
                                 << " not matching the first source volume " << firstsrc
                                 << " id: " << vr.m_Id;
                        /* settings have to change */
                        *perrcode = PRISM_VOLUME_INFO::SrcLunInvalid;
                        break;
                    }
                    bismatched = (nextvr.m_VolumeType == vr.m_VolumeType);
                    if (false == bismatched)
                    {
                        bismatched = (((nextvr.m_VolumeType == VolumeSummary::EXTENDED_PARTITION) && (vr.m_VolumeType == VolumeSummary::PARTITION)) ||
                                      ((nextvr.m_VolumeType == VolumeSummary::PARTITION) && (vr.m_VolumeType == VolumeSummary::EXTENDED_PARTITION)));
                    }
                    if (false == bismatched)
                    {
                        errormsg << " type: " << StrDeviceType[nextvr.m_VolumeType]
                                 << " not matching the first source volume " << firstsrc
                                 << " type: " << StrDeviceType[vr.m_VolumeType];
                        /* settings have to change */
                        *perrcode = PRISM_VOLUME_INFO::SrcsTypeMismatch;
                        break;
                    }
                    bismatched = (nextvr.m_bIsMultipathNode == vr.m_bIsMultipathNode);
                    if (false == bismatched)
                    {
                        errormsg << " is multipath property: " << STRBOOL(nextvr.m_bIsMultipathNode)
                                 << " not matching the first source volume " << firstsrc
                                 << " is multipath property: " << STRBOOL(vr.m_bIsMultipathNode);
                        /* settings have to change */
                        *perrcode = PRISM_VOLUME_INFO::SrcsIsMulpathMismatch;
                        break;
                    }
                    bismatched = (nextvr.m_Vendor == vr.m_Vendor);
                    if (false == bismatched)
                    {
                        errormsg << " vendor: " << StrVendor[nextvr.m_Vendor]
                                 << " not matching the first source volume " << firstsrc
                                 << " vendor: " << StrVendor[vr.m_Vendor];
                        /* settings have to change */
                        *perrcode = PRISM_VOLUME_INFO::SrcsVendorMismatch;
                        break;
                    }
                } while (0);

                if (false == bismatched)
                {
                    DebugPrintf(SV_LOG_ERROR, "For source lun id %s, %s\n", m_pPrismVolumeInfo->sourceLUNID.c_str(), 
                                              errormsg.str().c_str());
                    break;
                }
            }

        } while (0);
        bfilled = bismatched;
        delete pvr;
    }
    else
    {
        /* settings have to change */
        DebugPrintf(SV_LOG_ERROR, "source volume list for source lun id %s is empty\n", 
                                  m_pPrismVolumeInfo->sourceLUNID.c_str());
        *perrcode = PRISM_VOLUME_INFO::SrcDevScsiIdErr;
    }

    return bfilled;
}


bool PrismVolume::GetATs(const VolumeReporter::VolumeReport_t &vr, ATLunNames_t &atlunnames)
{
    std::list<std::string> applianceTargetPWWNs;
    std::list<std::string> applianceNetworkAddress;

    // TODO get nw addr from targ pwwn
    for (std::list<std::string>::iterator atListIter = m_pPrismVolumeInfo->applianceTargetPWWNs.begin();
            atListIter != m_pPrismVolumeInfo->applianceTargetPWWNs.end(); atListIter++)
    {
        std::string applianceTargetPwwn = *atListIter;

        std::size_t found = applianceTargetPwwn.find(";");
        if (found == std::string::npos)
            applianceTargetPWWNs.push_back(applianceTargetPwwn);
        else
        {
            applianceTargetPWWNs.push_back(applianceTargetPwwn.substr(0, found));

            std::size_t found1 = applianceTargetPwwn.find(";",found+1);
            while (found1 != std::string::npos)
            {
                std::string ipAddress = applianceTargetPwwn.substr(found+1, found1);
                applianceNetworkAddress.push_back(ipAddress);
                found=found1;
                found1 = applianceTargetPwwn.find(";",found+1);
            }

            if (found1 == std::string::npos)
            {
                std::string ipAddress = applianceTargetPwwn.substr(found+1, found1);
                applianceNetworkAddress.push_back(ipAddress);
            }

        }
    }

    PIAT_LUN_INFO piatluninfo(m_pPrismVolumeInfo->sourceLUNID,
                              m_pPrismVolumeInfo->applianceTargetLUNNumber,
                              m_pPrismVolumeInfo->applianceTargetLUNName,
                              m_pPrismVolumeInfo->physicalInitiatorPWWNs,
                              applianceTargetPWWNs,
                              vr.m_VolumeType,
                              applianceNetworkAddress);

    bool bgot = false;
    do
    { 
        atlunnames.clear();
        std::string errstr;
        PRISM_VOLUME_INFO::MIRROR_ERROR errcode = PRISM_VOLUME_INFO::NoError;
        GetATLunNames(piatluninfo, boost::ref(m_ProtectEvent), atlunnames);
        if (IsStopRequested())
        {
            break;
        }
        if (atlunnames.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "For %s, number of AT Luns discovered "
                                      "are zero\n", GetSourceLUNID().c_str());
            errcode = PRISM_VOLUME_INFO::AtlunDiscoveryFailed;
            errstr = m_pMirrorError->GetMirrorErrorMessage(errcode);
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "Got following AT Lun names for %s\n", GetSourceLUNID().c_str());
            for_each(atlunnames.begin(), atlunnames.end(), PrintString);
            bgot = true;
            break;
        }
        bool bshouldretryuntilquit = false;
        int iUpdateStatus = UpdateMirrorState(errcode, errstr, bshouldretryuntilquit, 0);
        if (SV_SUCCESS != iUpdateStatus)
        {
            DebugPrintf(SV_LOG_ERROR, "FAILED: updating mirror state as MIRROR_SETUP_FAILED to CX for source LUN ID %s.\n",
                                      GetSourceLUNID().c_str());
        }
        /* TODO: should this be error ? */
        DebugPrintf(SV_LOG_WARNING, "Retrying discovering AT Luns after %lu seconds\n", NSECS_RETRYMIRRORSETUP);
        WaitOnQuitEvent(NSECS_RETRYMIRRORSETUP);
    } while (!IsStopRequested());

    return bgot;
}


int PrismVolume::AreSettingsValid(VolumeReporter::VolumeReport_t &vr)
{
    const VolumeSummaries_t *pvs = m_pVolumeSummaries;
    VolumeSummaries_t tmpvs;
    unsigned char validstatus = 0;
    int iStatus = SV_FAILURE;
    DataCacher dataCacher;
     
    dataCacher.Init();
    bool bexit = false;
    do
    {
        PRISM_VOLUME_INFO::MIRROR_ERROR errcode = PRISM_VOLUME_INFO::NoError;
        validstatus = GetPrismVolumeInfoValidStatus(*pvs, vr, &errcode);
        if (validstatus)
        {
            if (PRISM_VOLUME_INFO::NoError == errcode)
            {
                /* although this should not occur */
                errcode = PRISM_VOLUME_INFO::MirrorErrorUnknown;
            }
            /* TODO: In any mirror state, if invalid, report to CX and do not proceed */
            DebugPrintf(SV_LOG_ERROR, "For source LUN ID %s, prism settings is invalid. validity status: %u\n", 
                                      GetSourceLUNID().c_str(), validstatus);
            /* ignore all errors in settings except SOURCE_LUN_ID_INVALID when
             * mirror state is MIRROR_DELETE_PENDING or MIRROR_SETUP_COMPLETED */
            if ((0 == (validstatus & PRISM_VOLUME_INFO::SOURCE_LUN_ID_INVALID)) && 
                ((PRISM_VOLUME_INFO::MIRROR_DELETE_PENDING == m_pPrismVolumeInfo->mirrorState) ||
                 (PRISM_VOLUME_INFO::MIRROR_SETUP_COMPLETED == m_pPrismVolumeInfo->mirrorState)))
            {
                DebugPrintf(SV_LOG_ERROR, "Even though setting is invalid (validity status %u), "
                                          "it has non empty source lun ID. Hence continuing with "
                                          "mirror exception notify or delete for source lun id %s\n", 
                                          validstatus, GetSourceLUNID().c_str());
                                          
                iStatus = SV_SUCCESS;
                bexit = true;
            }
            else
            {
                iStatus = SV_FAILURE;
                bool bshouldretryuntilquit = false;
                unsigned long nsecstowaitforretry = 0;
                if (PRISM_VOLUME_INFO::SrcNotReported != errcode)
                {
                    DebugPrintf(SV_LOG_ERROR, "The mirror thread is exiting for source LUN ID %s since invalid settings\n", GetSourceLUNID().c_str());
                    bexit = true;
                    bshouldretryuntilquit = true;
                    nsecstowaitforretry = NSECS_RETRY_SEND_MIRRORSTATE;
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "Rechecking if source luns are being reported for source LUN ID %s after %u seconds\n", 
                                GetSourceLUNID().c_str(), m_WaitTimeForSrcLunsValidity);
                    WaitOnQuitEvent(m_WaitTimeForSrcLunsValidity);
                    if (!IsStopRequested())
                    {
                        vr.clear();
                        tmpvs.clear();
                        VolumeDynamicInfos_t volumeDynamicInfos;
                        VolumeInfoCollector volumeCollector;
                        volumeCollector.GetVolumeInfos(tmpvs, volumeDynamicInfos, false);
                        pvs = &tmpvs;
                        /* TODO: error checking ? */
                        dataCacher.PutVolumesToCache(tmpvs);
                    }
                }
                std::string errstr = m_pMirrorError->GetMirrorErrorMessage(errcode);
                /* TODO: should this be error ? */
                DebugPrintf(SV_LOG_ERROR, "for source lun id %s, %s\n", GetSourceLUNID().c_str(), errstr.c_str());
                int iSendStatus = UpdateMirrorState(errcode, errstr, bshouldretryuntilquit, nsecstowaitforretry);
                if (SV_SUCCESS != iSendStatus)
                {
                    DebugPrintf(SV_LOG_ERROR, "FAILED: updating mirror state as %s to CX for source LUN ID %s\n",
                                              errstr.c_str(), GetSourceLUNID().c_str());
                }
            }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "For source LUN ID %s, prism settings is valid\n", GetSourceLUNID().c_str());
            iStatus = SV_SUCCESS;
            bexit = true;
        }
    } while (!bexit && (!IsStopRequested()));

    return iStatus;
}

