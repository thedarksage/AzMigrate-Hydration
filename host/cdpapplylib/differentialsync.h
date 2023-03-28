//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : differentialsync.h
//
// Description:
//

#ifndef DIFFERENTIALSYNC_H
#define DIFFERENTIALSYNC_H

#include <string>
#include <list>
#include <map>

#include <ace/Manual_Event.h>

#include <boost/shared_ptr.hpp>

#include <ace/Task.h>

#include "svtypes.h"
#include "programargs.h"
#include "volumegroupsettings.h"
#include "volumeinfo.h"
#include "diffinfo.h"
#include "groupinfo.h"
#include "transport_settings.h"
#include "cxtransport.h"
#include "dataprotectionsync.h"
#include "cdpsnapshot.h"
#include "svproactor.h"

class DifferentialSync : public DataProtectionSync {
public:
    DifferentialSync(VOLUME_GROUP_SETTINGS const & volumeGroupSettings, DiffSyncArgs const & args);
    DifferentialSync(VOLUME_GROUP_SETTINGS const & volumeGroupSettings,
                     int id,
                     int agentType,
                     std::vector<std::string> visibleVolumes,
                     const std::string& programeName,
                     const std::string& agentSide,
                     const std::string& syncType,
                     const std::string& diffsLocation,
                     const std::string& cacheLocation,
                     const std::string& searchPattern,
                     const bool &syncThroughThreads);
    void Start();
    void UpdateVolumeGroupSettings(VOLUME_GROUP_SETTINGS& volumeGroupSettings) { } // TODO: implement
    bool ProcessSnapshotRequests(GroupInfo::VolumeInfoList_t & volInfos, const svector_t & bookmarks);
    GroupInfo::VolumeInfos_t& GetTargets() 
        { 
            return m_GroupInfo.Targets();
        }
    bool NotifyCxOnSnapshotProgress(const CDPMessage * msg);

protected:

    class GetDifferentialInfoTask : public ACE_Task<ACE_MT_SYNCH>
    {
    public:
        GetDifferentialInfoTask(DifferentialSync* diffSync,
                                VolumeInfo::Ptr target,
                                ACE_Thread_Manager* threadManager = 0)
            : ACE_Task<ACE_MT_SYNCH>(threadManager),
              m_DiffSync(diffSync),
              m_Target(target)
            { }
        virtual int open(void *args = 0);
        virtual int close(u_long flags = 0);
        virtual int svc();

    private:
        DifferentialSync* m_DiffSync;
        VolumeInfo::Ptr m_Target;
    };

    typedef boost::shared_ptr<GetDifferentialInfoTask> GetDifferentialInfoTaskPtr;
    typedef boost::shared_ptr< ACE_Message_Queue<ACE_MT_SYNCH> > ACE_Shared_MQ;

    class ApplyDifferentialDataTask : public ACE_Task<ACE_MT_SYNCH>
    {
    public:
        ApplyDifferentialDataTask(DifferentialSync* diffSync,
                                  DiffInfo::DiffInfosOrderedEndTime_t* diffInfos,
                                  bool m_VolumeGroup,
                                  ACE_Thread_Manager* threadManager = 0)
            : ACE_Task<ACE_MT_SYNCH>(threadManager),
              m_DiffSync(diffSync),
              m_DiffInfos(diffInfos),
              m_CurDiffInfoIter(diffInfos->begin()),
              m_EndDiffInfoIter(diffInfos->end()),
              m_CrossedSeqPoint(false),
            m_VG(m_VolumeGroup)
            {}
        virtual int open(void *args = 0);
        virtual int close(u_long flags = 0);
        virtual int svc();
        void PostMsg(int msgId, int priority);

    protected:

        bool ApplyDiffs();
        bool Quit();
        bool CrossedSeqPoint();


    private:
        DifferentialSync* m_DiffSync;
        DiffInfo::DiffInfosOrderedEndTime_t* m_DiffInfos;
        DiffInfo::DiffInfosOrderedEndTime_t::iterator m_CurDiffInfoIter;
        DiffInfo::DiffInfosOrderedEndTime_t::iterator m_EndDiffInfoIter;
        bool m_CrossedSeqPoint;

        // this variable is true if the pair is part of volume group.
        bool m_VG;
    };

    typedef boost::shared_ptr<ApplyDifferentialDataTask> ApplyDifferentialDataTaskPtr;
    typedef std::list<ApplyDifferentialDataTaskPtr> ApplyDifferentialDataTasks_t;

    void NotifyCxDiffsDrained(const time_t& timeToApplyDiffs);
    void ApplyConsistencyPolicies();

    bool ProcessDifferentials();
    bool GetDifferentialInfos();
    bool GetDifferentialInfo(VolumeInfo::Ptr volumeInfo);
    bool GetFileList(CxTransport::ptr cxTransport, const std::string& cacheLocation, FileInfos_t& fileInfos);

    bool ApplyData(DiffInfo::DiffInfoPtr diffInfo, bool lastFileToProcess);

    bool ProcessSnapshotRequests();

    SV_ULONGLONG TargetCount() { return m_GroupInfo.Targets().size();}

    bool HaveDiffsToApply() { return m_GroupInfo.HaveDiffsToApply(); }
    bool SnapshotOnlyMode() { return m_GroupInfo.SnapshotOnlyMode(); }

    template<class Task, class TaskPtr> bool ProcessAllTargets();

    void PostMsg(int msgId, int priority);
    bool WaitForProcessDifferentialTasks(ApplyDifferentialDataTasks_t& applyTasks);
    void WaitForSnapshotTasks(SnapshotTasks_t, boost::shared_ptr< ACE_Message_Queue<ACE_MT_SYNCH> >);
    void StopAbortedSnapshots(SnapshotTasks_t, boost::shared_ptr< ACE_Message_Queue<ACE_MT_SYNCH> >);


    void CalculateSyncPoint();
    SV_ULONGLONG SyncPoint();
    void IncrementSyncPoint(const SV_ULONGLONG & increment);


private:
    // no copying DifferentialSync
    DifferentialSync(const DifferentialSync& differentialSync);
    DifferentialSync& operator=(const DifferentialSync& differentialSync);


    GroupInfo m_GroupInfo;
    Lockable m_SyncPointLock;
    SV_ULONGLONG m_SyncPoint;

    TRANSPORT_CONNECTION_SETTINGS m_TransportSettings;

    ACE_Thread_Manager m_ThreadManager;

    // we use seperate thread manager for the proactor
    // task. The reason we choose this approach is as follows:
    // At various code places, m_ThreadManager waits for all tasks
    // to exit after posting a quit message on task message queue
    // We do not want to wait for the proactor at all of these places.
    // Either we have to wait only on those tasks for which quit
    // message was posted or we need a seperate thread manager.
    ACE_Thread_Manager m_ProactorThrMgr;

    ACE_Message_Queue<ACE_MT_SYNCH> m_MsgQueue;

};

void DiffSyncUsage(char const * name);

#endif // ifndef DIFFERENTIALSYNC__H
