//                                       
// Copyright (c) 2008 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : cdpmgr.h
// 
// Description: 
//

#ifndef CDPMGR__H
#define CDPMGR__H

#include <ace/Task.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include "ace/Recursive_Thread_Mutex.h"

#include <boost/shared_ptr.hpp>

#include "configurator2.h"
#include "initialsettings.h"
#include "retentioninformation.h"
#include "cdplock.h"

typedef boost::shared_ptr< ACE_Message_Queue<ACE_MT_SYNCH> > ACE_Shared_MQ;

class ReporterTask;
typedef boost::shared_ptr<ReporterTask> ReporterTaskPtr;

class PolicyMgrTask;
typedef boost::shared_ptr<PolicyMgrTask> PolicyMgrTaskPtr;
typedef std::map<std::string,PolicyMgrTaskPtr> PolicyMgrTasks_t;
typedef std::pair<std::string,PolicyMgrTaskPtr> PolicyMgrTaskPair_t;

class ScheduleTask;
typedef boost::shared_ptr<ScheduleTask> ScheduleTaskPtr;
typedef std::map<std::string,ScheduleTaskPtr> ScheduleTasks_t;
typedef std::pair<std::string,ScheduleTaskPtr> ScheduleTaskPair_t;

//Changes for bug id 17548
class RetentionSpaceMonitorTask;
typedef boost::shared_ptr<RetentionSpaceMonitorTask> RetentionSpaceMonitorTaskPtr;

struct cdp_recoveryrangekey_t
{
    cdp_recoveryrangekey_t(SV_ULONGLONG start = 0, std::string volume = "")
    {
        start_ts = start;
        tgtvolname = volume;
    }

    SV_ULONGLONG start_ts;    
    std::string tgtvolname;
};

struct cdp_recoveryrangekey_cmp_t
{
    bool operator()(const cdp_recoveryrangekey_t & lhs, const cdp_recoveryrangekey_t & rhs) const
    {
        if (lhs.start_ts < rhs.start_ts)
            return true;
        return false;
    }
};

typedef  std::multiset<cdp_recoveryrangekey_t, cdp_recoveryrangekey_cmp_t> cdp_recoveryrange_set_t;

struct CdpTimeRange
{
    SV_ULONGLONG startts;
    SV_ULONGLONG endts;
    CdpTimeRange(){startts = 0; endts = 0;}
};

typedef std::map<std::string,CdpTimeRange> CdpTimeRanges_t;
typedef std::pair<std::string,CdpTimeRange> CdpTimeRangePair_t;


struct CdpMoveMsg
{	
    std::string oldlocation;
    std::string newlocation;
};


struct CdpDelMsg
{
    bool vsnapcleanup;
    bool cdpcleanup;
};

struct CdpDisableMsg
{
    std::string dbname;
};

enum SCHEDULEACTION {CDPUNKNOWN = 0,CDPMOVEACTION=1, CDPPAIRDELETEACTION=2,CDPDISABLEACTION=3};

class CDPMgr : public sigslot::has_slots<>
{
public:

    CDPMgr(int argc, char ** argv);
    ~CDPMgr();
    bool run();

    CDP_SETTINGS getCdpSettings(const std::string & volname, bool current = true);

protected:

private:

    CDPMgr(CDPMgr const & );
    CDPMgr& operator=(CDPMgr const & );

    bool init();
    bool uninit();

    bool initializeTal();
    bool SetupLocalLogging();
    bool AnotherInstanceRunning();
    bool InitializeConfigurator();

    bool StopConfigurator();
    bool StopLocalLogging();

    void ConfigChangeCallback(InitialSettings);

    bool StopAllTasks();
    bool NotifyTimeOut();
    bool AllTasksExited();
    bool ReapDeadTasks();

    bool CopyCDPSettings(const CDPSETTINGS_MAP & cdp_settings,bool current = true);
    bool StartNewPolicyMgrs(InitialSettings &);
    bool StartScheduleJobs(InitialSettings &);

    PolicyMgrTask * getPolicyMgr(const std::string & volname);
    ScheduleTask * getScheduleTask(const std::string & volname);
    bool CDPEnabled(const CDP_SETTINGS & cdp_settings);
    bool CDPDisabled(const CDP_SETTINGS & cdp_settings);

    bool cleanupPending(const CDP_SETTINGS & cdp_settings);

    bool isPairDeleted(VOLUME_SETTINGS & volsettings, 
        const std::string & volname,
        CdpDelMsg &msg);

    bool isCDPMoved(VOLUME_SETTINGS & volsettings, 
        const std::string & volname,
        CdpMoveMsg &msg); 

    bool StartReporterTask();

    int m_argc;
    char ** m_argv;

    Configurator* m_configurator;
    bool m_bconfig;
    bool m_blocalLog;

    ACE_Thread_Manager m_ThreadManager;
    typedef ACE_Guard<ACE_Recursive_Thread_Mutex> AutoGuard;
    CDPLock::Ptr m_cdplock;
    ReporterTaskPtr m_reporterTaskPtr;

	mutable ACE_Recursive_Thread_Mutex m_lockTasks;
    PolicyMgrTasks_t m_policymgrtasks;
    ScheduleTasks_t m_scheduleTasks;

    mutable ACE_Recursive_Thread_Mutex m_lockSettings;
    CDPSETTINGS_MAP m_cdpsettings;
    CDPSETTINGS_MAP m_oldcdpsettings;
};


class PolicyMgrTask: public ACE_Task<ACE_MT_SYNCH>
{

public:

    PolicyMgrTask(CDPMgr* retentionmgr, Configurator* configurator,
        const std::string &retention_volname, 
        const std::string &retention_mountpoint, 
        ACE_Thread_Manager* threadManager = 0)
        : ACE_Task<ACE_MT_SYNCH>(threadManager),
        m_retentionmgr(retentionmgr),
        m_configurator(configurator),
        m_retentionvolname(retention_volname),
        m_retentionmtpt(retention_mountpoint),
        m_bDead(false),
        m_bShouldQuit(false),
        m_bTimeSliceExpired(false),
        m_cx_update_per_target_volume(false),
        m_cx_event_timerange_rec_per_batch(0),
        m_cx_send_updates_atonce(false),
        m_delete_unusable_pts(false),
		m_delete_stalefiles(false),
        m_cdpfreespace_trigger(0),
        m_space_tofree(0),
        m_cdpfreespacecheck_interval(0),
        m_cdpfreespacecheck_ts(0), 
        m_cdppolicycheck_interval(0),
        m_cdppolicycheck_ts(0),
        m_lowspace(false),
        m_purge_onlowspace_alert(false),
        m_alertupdate_interval(0),
        m_alertupdate_ts(0),
        m_thresholdcrossed_pct(0),
        m_cxupdates_interval(0),
        m_cx_cdpdiskusage_update_interval(0),
		m_delete_pendingupdates(0),
		m_delete_pendingupdates_interval(0),
		m_cx_deletepending_updates_ts(0),
        m_cxupdates_ts(0),
        m_cx_cdpdiskusage_updates_ts(0),
		m_cdpreserved_space(0)

    { }

    virtual int open(void *args = 0);
    virtual int close(u_long flags = 0);
    virtual int svc();
    void PostMsg(int msgId, int priority);
    bool isDead()  { return m_bDead ;}

protected:

    void Idle(int seconds = 0);
    bool QuitRequested(int seconds = 0);
    SV_UINT FetchNextMessage(ACE_Message_Block **, int seconds = 0);
    void MarkDead() { m_bDead = true; }
    bool SendPauseRequestToCX(const std::string & volumeName, const std::string & pauseMsg);

private:
    bool IsPause(const std::string & volname);
    bool isMoveRetention(const std::string & volname);
    bool isPairDeleted(const std::string & volname);
	bool isPairFlushAndHoldState(const std::string & volname);
    bool getCdpSettings();

    bool ProcessNextMessage();
    bool TimeSliceExpired() { return m_bTimeSliceExpired; }	
    bool initCDP(const CDP_SETTINGS & cdpsetting);
    bool PerformStaleFileCleanup();
    bool DeleteUnusableRecoveryPoints();
    bool ProcessPoliciesForRetentionVolume();
    bool EnforcePolicies(const std::string & volname , const CDP_SETTINGS & cdpsetting);
    bool EnforceTimeAndSpacePolicies(const std::string & volname , const CDP_SETTINGS & cdpsetting);
    bool FreeSpaceOnInsufficientRetentionDiskSpace();
    SV_ULONGLONG getInsufficientSpaceToFree(){return m_space_tofree;}

    bool IsInsufficientDiskSpaceOccured();
    void SetMinimumAvalableFreeSpace();
    bool sendalerts();
    bool updatecx(); 
    bool updatecx_nextbatch(bool & anyrecords_pending);
    bool updateCdpInformation(const HostCDPInformation & hostCdpInfo);

    bool updateCdpDiskUsageToCX();

	bool deleteAllPendingUpdates();

    CDPSETTINGS_MAP m_policies;
    CdpTimeRanges_t m_cdptimeranges;
    CDPMgr * m_retentionmgr;
    Configurator* m_configurator;
    std::string m_retentionvolname;
    std::string m_retentionmtpt;
    bool m_bDead;
    bool m_bShouldQuit;
    bool m_bTimeSliceExpired;
    bool m_cx_update_per_target_volume;
    SV_ULONG m_cx_event_timerange_rec_per_batch;
    bool m_cx_send_updates_atonce;
    bool m_delete_unusable_pts;
	bool m_delete_stalefiles;

    SV_ULONGLONG m_cdpfreespace_trigger;
    SV_ULONGLONG m_space_tofree;
    SV_ULONG m_cdpfreespacecheck_interval;
    ACE_Time_Value m_cdpfreespacecheck_ts; 


    SV_ULONG m_cdppolicycheck_interval;
    ACE_Time_Value m_cdppolicycheck_ts;


    bool m_lowspace;
    bool m_purge_onlowspace_alert;
    SV_ULONG m_alertupdate_interval;
    ACE_Time_Value m_alertupdate_ts; 
    SV_UINT m_thresholdcrossed_pct;

    SV_ULONG m_cxupdates_interval;
    SV_ULONG m_cx_cdpdiskusage_update_interval;
	SV_ULONG m_delete_pendingupdates;
	SV_ULONG m_delete_pendingupdates_interval;
    ACE_Time_Value m_cxupdates_ts;
    ACE_Time_Value m_cx_cdpdiskusage_updates_ts;
    ACE_Time_Value m_cx_deletepending_updates_ts;
    //Changes for bug id 17548
	SV_ULONGLONG m_cdpreserved_space;

    InitialSettings m_initialsettings;

};

class ReporterTask : public ACE_Task<ACE_MT_SYNCH>
{
public:
    ReporterTask(CDPMgr* retentionmgr,Configurator* configurator, ACE_Thread_Manager* threadManager)
        : ACE_Task<ACE_MT_SYNCH>(threadManager),
        m_retentionmgr(retentionmgr),
        m_configurator(configurator),
        m_bDead(false)
    {
    }
    virtual int open(void *args =0);
    virtual int close(u_long flags =0);
    virtual int svc();
    bool isDead() { return m_bDead ;}

    void PostMsg(int msgId, int priority);

protected:
    void Idle(int seconds = 0);
    bool QuitRequested(int seconds = 0);
    void MarkDead() { m_bDead = true; }

private:
    bool VsnapUpdationRequestToCX(SV_UINT nBatchRequest);

    bool m_bDead;

    CDPMgr * m_retentionmgr;
    Configurator * m_configurator;
};

class ScheduleTask : public ACE_Task<ACE_MT_SYNCH>
{
public:
    ScheduleTask(CDPMgr* retentionmgr, Configurator* configurator,
        const std::string &volname, void * actionmsg,SCHEDULEACTION action,
        const std::string & dbname, ACE_Thread_Manager* threadManager = 0)
        : ACE_Task<ACE_MT_SYNCH>(threadManager),
        m_retentionmgr(retentionmgr),
        m_configurator(configurator),
        m_volname(volname),
        m_action(action),
        m_actionmsg(actionmsg),
        m_dbname(dbname),
        m_bDead(false),
        m_bstarted(false)
    {}
    virtual int open(void *args =0);
    virtual int close(u_long flags =0);
    virtual int svc();
    void PostMsg(int msgId, int priority);
    bool isDead() { return m_bDead ;}
    bool isStart() { return m_bstarted;}
    void MarkStart() {m_bstarted = true;}
    std::string getDbName() { return m_dbname;}

protected:
    SV_UINT FetchNextMessage(ACE_Message_Block **, int seconds = 0);
    bool QuitRequested(int seconds = 0);
    void MarkDead() { m_bDead = true; }

private:
    bool ProcessPairDeletion(CdpDelMsg * delmsg);
    bool DisableRetention();
    bool MoveRetention(CdpMoveMsg * movemsg);	
    bool DeleteVsnapsForTargetVolume(const std::string & DeviceName, std::string & errorMessage);
    bool IsPairDeletionInPogress();

    CDPMgr * m_retentionmgr;
    Configurator * m_configurator;
    std::string m_volname;
    bool m_bstarted;
    bool m_bDead;
    void * m_actionmsg;
    std::string m_dbname;
    SCHEDULEACTION m_action;
};

#endif
