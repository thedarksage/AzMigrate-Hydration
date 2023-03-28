#ifndef PRISM_VOLUME__H
#define PRISM_VOLUME__H

#include <string>
#include <set>
#include <map>
#include <vector>
#include <list>
#include <algorithm>
#include "svtypes.h"
#include "entity.h"
#include "event.h"
#include "volumegroupsettings.h"
#include "devicestream.h"
#include "runnable.h"
#include "prismsettings.h"
#include "devicefilterfeatures.h"
#include "mirrorerror.h"
#include "mirrorer.h"
#include "volumereporter.h"


/* TODO: inherited from protectedgroupvolumes.cpp, these
 * ought to have been signed values. Need to change */
const unsigned long MAX_NSECS_FOR_RESYNC_EVENT = 65 ;
const unsigned long MIN_NSECS_FOR_RESYNC_EVENT = 30;

const unsigned long NSECS_RETRYINIT = 30;

/* waits to do before doing any retry */
const unsigned long NSECS_RETRYMIRRORSETUP = 30;
const unsigned long NSECS_RETRYMIRRORDELETE = 30;
/* TODO: 5 may be very less value ??? */
const unsigned long NSECS_RETRY_SEND_MIRRORSTATE = 5;
const unsigned long NSECS_RETRYMIRRORRESYNCEVENT = 30;
const unsigned int NSECS_TOWAITFORSRC = 180;

class PrismVolume :
    public Entity, public Runnable
{
public:
	PrismVolume(const PRISM_VOLUME_INFO &PrismVolumeInfo, 
                const DeviceFilterFeatures *pDeviceFilterFeatures, 
                const char *pHostId, MirrorError *pMirrorError, 
                const VolumeSummaries_t *pVolumeSummaries);
    virtual ~PrismVolume();

    /* init completed */
    const bool IsProtecting() const;
    /* Does mirror setup, and delete. 
     * If mirror setup, calls mirror setup ioclt
     * and then calls WaitForMirrorEvent
     * If mirror already setup, calls directly WaitForMirrorEvent
     * If mirror deleted, exits */
    int Protect();
    int WakeUpThreads(void);
    int RegisterEventsWithDriver(void);
    int Mirror(void);
    /* same as the of host replication thread */
    int StopProtection(const long int);
    /* Creates only the device stream */
    int Init();
    /* Like replicate, main body does only wait for mirror event */
    int WaitForMirrorEvent(void);
    int SetupMirror(Mirrorer *pmirrorer, const VolumeReporter::VolumeReport_t &vr);
    int DeleteMirror(Mirrorer *pmirrorer);
    
    /* not implemented; just keep it */
    int ReturnHome();

    /* Like get volume settings; not in use for now */
    const PRISM_VOLUME_INFO& GetPrismVolumeInfo() const;

    virtual THREAD_FUNC_RETURN Run();
    virtual THREAD_FUNC_RETURN Stop(const long int PARAM=0);

    /* called from destructor */
    void Destroy();

    const std::string & GetSourceLUNID(void) const;

private:
    /* has to be present */
    WAIT_STATE WaitOnQuitEvent(long int);
    /* has to be there */
    const bool IsStopRequested() const;
    /* has to be there */
    bool ShouldQuit() const;
    PrismVolume(const PrismVolume&);
    PrismVolume();
    PrismVolume& operator=(const PrismVolume&);

    /* Like WaitForData; called from WaitForMirrorEvent */
    WAIT_STATE WaitForResyncEvent();

    int CreateDeviceStream(void);
    unsigned long GetMirrorResyncEventWaitTime(void);
    void PrintPrismVolumeInfo(void);
    unsigned char GetPrismVolumeInfoValidStatus(const VolumeSummaries_t &vs, 
                                                VolumeReporter::VolumeReport_t &vr, 
                                                PRISM_VOLUME_INFO::MIRROR_ERROR *perrcode);
    bool DoScsiIDsMatchTheGiven(const std::list<std::string> &devices, const std::string &refid);
    int UpdateMirrorState(
                          const PRISM_VOLUME_INFO::MIRROR_ERROR errcode,
                          const std::string &errstring,
                          const bool &bshouldretryuntilquit,
                          const unsigned long nsecstowaitforretry
                         );
    bool ReportResyncEvent(const SV_ULONGLONG ullOutOfSyncErrorCode,
                           const char *pErrorStringForResync, 
                           const SV_ULONGLONG ullOutOfSyncTS);
    bool GetSourceVolumeReport(const VolumeSummaries_t &vs, VolumeReporter::VolumeReport_t &vr, PRISM_VOLUME_INFO::MIRROR_ERROR *perrcode);
    bool GetATs(const VolumeReporter::VolumeReport_t &vr, ATLunNames_t &atlunnames);
    int AreSettingsValid(VolumeReporter::VolumeReport_t &vr);

private:
    long int m_liQuitInfo;                 //stores the reason for quitting. refer s2/sentinel.cpp
    std::string m_sSourceLUNID;            //stores ID of the source LUN; mostly for display purposes 
    bool m_bProtecting;                    //stores protection status, whether is being protected?
    PRISM_VOLUME_INFO* m_pPrismVolumeInfo; //stores a copy of the prism volume info; In construct do the new; delete in destroy
    Event m_ProtectEvent;                  //used to signal start and stop of protection
    /* Event m_ResyncEvent; */             //windows only: windows uses event instead of ioctl; enable once windows is ready
    DeviceStream* m_pDeviceStream;         //points to the device stream used to interact with the filter driver
    unsigned long m_WaitTimeForResyncEvent;     //resync event wait time; there has to be a value for this from drscout.conf
    const DeviceFilterFeatures *m_pDeviceFilterFeatures;   //lets keep this 
    const char *m_pHostId;                          //lets keep this
    MirrorError *m_pMirrorError;
    const VolumeSummaries_t *m_pVolumeSummaries;   //lets keep this 
    unsigned int m_WaitTimeForSrcLunsValidity;
};


#endif /* PRISM_VOLUME__H */
