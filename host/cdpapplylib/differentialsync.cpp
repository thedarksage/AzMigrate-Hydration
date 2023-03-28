#include <string.h>
#include <sstream>
#include <cstdio>
#include <vector>

#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_sys_stat.h>
#include <ace/OS_NS_time.h>
#include <ace/OS_NS_sys_time.h>
#include <ace/File_Lock.h>

#include <boost/bind.hpp>

#include "globs.h"
#include "defines.h"
#include "differentialsync.h"
#include "dataprotectionexception.h"
#include "hostagenthelpers_ported.h"
#include "theconfigurator.h"
#include "configurevxagent.h"
#include "fileconfigurator.h"
#include "volumeinfo.h"
#include "cdputil.h"
#include "configwrapper.h"
#include "inmageex.h"

#ifndef SV_WINDOWS
#include <sys/statvfs.h>
#endif

using namespace std;

static unsigned const MAX_COMPRESSION_RATIO = 3;

// message types
static int const DIFF_QUIT            = 0x2000;
static int const DIFF_APPLY_DATA      = 0x2001;
static int const DIFF_APPLY_ALL_DATA  = 0x2002;
static int const DIFF_GET_DATA_DONE   = 0x2003;
static int const DIFF_APPLY_DATA_DONE = 0x2004;
static int const DIFF_GET_INFO_DONE   = 0x2005;
static int const DIFF_GET_DATA_ERROR  = 0x2006;
static int const DIFF_EXCEPTION       = 0x2007;
static int const DIFF_SEQPT_REACHED   = 0x2008;
static int const DIFF_APPLYUPTO_NEXTSEQPT = 0x2009;
static int const DIFF_GET_DATAWORKER_DONE = 0x200A;
static int const DIFF_GET_DATAWORKER_EXCEPTION = 0x200B;

// message priority
static int const DIFF_NORMAL_PRIORITY = 0x01;
static int const DIFF_HIGH_PRIORITY   = 0x10;

// how long to wait for a windows message
static int const WindowsWaitTimeInSeconds = 5;

// ==========================================================================
// DifferentialSync
// ==========================================================================
// --------------------------------------------------------------------------
// constructor
// --------------------------------------------------------------------------
DifferentialSync::DifferentialSync(VOLUME_GROUP_SETTINGS const & volumeGroupSettings, DiffSyncArgs const & args)
    : DataProtectionSync(volumeGroupSettings),
	m_GroupInfo(args, volumeGroupSettings, &m_VolumesCache.m_Vs)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_TransportSettings = TransportSettings();
    m_GroupInfo.SetTargets();
}

DifferentialSync::DifferentialSync(VOLUME_GROUP_SETTINGS const & volumeGroupSettings,
                                   int id,
                                   int agentType,
                                   std::vector<std::string> visibleVolumes,
                                   const std::string& programeName,
                                   const std::string& agentSide,
                                   const std::string& syncType,
                                   const std::string& diffsLocation,
                                   const std::string& cacheLocation,
                                   const std::string& searchPattern, 
                                   const bool &syncThroughThreads)
: DataProtectionSync(volumeGroupSettings, syncThroughThreads),
    m_GroupInfo(id,
                agentType,
                visibleVolumes,
                programeName,
                agentSide,
                syncType,
                diffsLocation,
                cacheLocation,
                searchPattern, 
				volumeGroupSettings, 
				&m_VolumesCache.m_Vs)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_TransportSettings = TransportSettings();	
    m_GroupInfo.SetTargets();
}

// --------------------------------------------------------------------------
// top level diff data processing
// --------------------------------------------------------------------------
void DifferentialSync::Start()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    SVProactorTask * proactorTask = NULL;

    try {

        LocalConfigurator localconfigurator;
        SV_INT dpExitTime = localconfigurator.getDataprotectionExitTime();

        const SV_INT snapshotInterval = localconfigurator.getSnapshotInterval();
        const SV_INT notifyDiffsInterval = localconfigurator.getNotifyCxDiffsInterval();
        SV_ULONG MinRuns = localconfigurator.getMaxRunsPerInvocation();

        ACE_Time_Value startTime = ACE_OS::gettimeofday();
        ACE_Time_Value snapshotTime, notifyTime;

        SV_ULONG NumberofRuns = 0;


        StartProactor(proactorTask, m_ProactorThrMgr);

        while (!Quit())
        {

            ++NumberofRuns;

            // Set up things for next run
            // drain any pending messages from last loop run
            // reset values in groupinfo and all volume infos
            // reset all volume Infos

            m_MsgQueue.flush();
            m_GroupInfo.reset();

            // Now we are ready for next run

            // Bug #9034
            // To take care when the system time moves backward
            // (presenttime < snapshottime)

            ACE_Time_Value presentTime = ACE_OS::gettimeofday();

            if((presentTime.sec() - snapshotInterval >= snapshotTime.sec())
               || (presentTime.sec() < snapshotTime.sec()))
            {
                ProcessSnapshotRequests();
                snapshotTime = presentTime;
            }

            if(QuitRequested(0))
                break;

            if (!SnapshotOnlyMode()) {

                if(!m_GroupInfo.AvailableToProcessDiffs()) {
                    break;
                }

                if (!GetDifferentialInfos()) {
                    break;
                }

                ApplyConsistencyPolicies();

                if (!HaveDiffsToApply()) {
                    Idle(1);
                } else {

                    ACE_Time_Value fromTime = ACE_OS::gettimeofday ();
                    if (!ProcessDifferentials()) {
                        break;
                    }
                    ACE_Time_Value toTime = ACE_OS::gettimeofday ();
                    time_t timeToApplyDiffs = toTime.sec() - fromTime.sec();

                    // MAYBE: only call this every nth time

                    // Bug #9034
                    // To take care when the system time moves backward
                    // (currenttime < notifytime)

                    ACE_Time_Value currentNotifyTime = ACE_OS::gettimeofday();
                    if(((currentNotifyTime.sec() - notifyDiffsInterval >= notifyTime.sec())
                        || (currentNotifyTime.sec() < notifyTime.sec())) && (timeToApplyDiffs > 0))
                    {
                        NotifyCxDiffsDrained(timeToApplyDiffs);
                        notifyTime = currentNotifyTime;
                    }
                }
            }

            if(QuitRequested(0))
                break;

            ACE_Time_Value currentTime = ACE_OS::gettimeofday();
            if(currentTime.sec() - startTime.sec() > dpExitTime)
            {
                if(!MinRuns || (NumberofRuns >= MinRuns))
                    break;
            }
        }
        Stop();

    }

    catch (DataProtectionException& dpe) {
        DebugPrintf(dpe.LogLevel(), dpe.what());
    }
    catch (std::exception& e) {
        DebugPrintf(SV_LOG_ERROR, "FAILED Differentialsync::Start exception caught: %s\n", e.what());
    }
    catch (...){
        DebugPrintf(SV_LOG_FATAL, "Eception throun from DifferentialSync::Start()\n");
    }

    Stop();

    StopProactor(proactorTask, m_ProactorThrMgr);
}


// --------------------------------------------------------------------------
// posts a message to the message queue
// --------------------------------------------------------------------------
void DifferentialSync::PostMsg(int msgId, int priority)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    ACE_Message_Block *mb = new ACE_Message_Block(0, msgId);
    mb->msg_priority(priority);
    m_MsgQueue.enqueue_prio(mb);
}

// send abort to snapshots which have been aborted by the user from the UI
void DifferentialSync::StopAbortedSnapshots(SnapshotTasks_t tasks,
                                            boost::shared_ptr< ACE_Message_Queue<ACE_MT_SYNCH> > mq)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    SnapshotTasks_t::iterator iter(tasks.begin());
    SnapshotTasks_t::iterator end(tasks.end());

    for (/* empty */; iter != end; ++iter) {
        string id = (*iter).first;
        if(TheConfigurator ->getVxAgent().isSnapshotAborted(id))
        {
            SnapshotTaskPtr taskptr = (*iter).second;
            taskptr ->PostMsg(CDP_ABORT, CDP_HIGH_PRIORITY);
        }
    }

}

// --------------------------------------------------------------------------
// waits for the snapshots tasks to finish
// it handles state change and progress updates and notifies cx
// it also checks for service quit requests
// if quit requested tells tasks to quit
// --------------------------------------------------------------------------
void DifferentialSync::WaitForSnapshotTasks(SnapshotTasks_t tasks,
                                            boost::shared_ptr< ACE_Message_Queue<ACE_MT_SYNCH> > mq)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    //__asm int 3;
    bool quit = false;
    unsigned TaskDoneCount = 0;

    while(true)
    {
        ACE_Message_Block *mb;
        ACE_Time_Value waitTime = ACE_Time_Value::max_time;
        if (-1 == mq -> dequeue_head(mb, &waitTime))
        {
            if (errno != EWOULDBLOCK && errno != ESHUTDOWN)
            {
                quit = true;
                break;
            }
        } else {

            CDPMessage * msg = (CDPMessage*)mb ->base();

            //Send update to cx
            NotifyCxOnSnapshotProgress(msg);

            if (msg -> executionState == SNAPSHOT_REQUEST::Complete
                || msg -> executionState == SNAPSHOT_REQUEST::Failed)
                ++TaskDoneCount;

            delete msg;
            mb -> release();

            if(TaskDoneCount >= tasks.size())
                break;
        }

        if(ServiceRequestedQuit(0))
        {
            quit = true;
            break;
        }

#if 0 // BUG 617 DISABLED (enabled once cx is ready)
        StopAbortedSnapshots(tasks, mq);
#endif
    }

    if(quit)
    {
        // need to tell all snapshot tasks to quit
        SnapshotTasks_t::iterator iter(tasks.begin());
        SnapshotTasks_t::iterator end(tasks.end());
        for (/* empty */; iter != end; ++iter) {
            SnapshotTaskPtr taskptr = (*iter).second;
            taskptr ->PostMsg(CDP_QUIT, CDP_HIGH_PRIORITY);
        }
    }

    // Note:Do not call threadmanager.wait
    // as this function is invoked from a thread managed
    // by threadmanager
    //m_ThreadManager.wait();

    //wait for snapshot tasks
    SnapshotTasks_t::iterator iter(tasks.begin());
    SnapshotTasks_t::iterator end(tasks.end());
    for (/* empty */; iter != end; ++iter) {
        SnapshotTaskPtr taskptr = (*iter).second;
        m_ThreadManager.wait_task(taskptr.get());
    }

    //drain any pending messages
    while(true)
    {
        ACE_Message_Block *mb;
        ACE_Time_Value waitTime;
        if (-1 == mq -> dequeue_head(mb, &waitTime))
        {
            if (errno == EWOULDBLOCK || errno == ESHUTDOWN)
            {
                break;
            }
        }

        CDPMessage * msg = (CDPMessage*)mb ->base();

        //Send update to cx
        NotifyCxOnSnapshotProgress(msg);

        delete msg;
        mb -> release();
    }

}

// --------------------------------------------------------------------------
// waits for the Get and Apply tasks to finish
// it also checks for service quit requests
// if quite requested tells tasks to quit
// --------------------------------------------------------------------------
bool DifferentialSync::WaitForProcessDifferentialTasks(ApplyDifferentialDataTasks_t& applyTasks)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool done = false;
    bool ok = true;

    unsigned currapplyTaskCount = (SV_UINT) applyTasks.size();
    unsigned applyTaskSeqPtReached = 0;
    unsigned applyTaskDoneCount  = 0;

    // sequence point in terms of applied data's time in hundred nano seconds
    SV_ULONGLONG cbSeqCountInNSecs =
        TheConfigurator ->getVxAgent().getSequenceCountInMsecs() * 10000;

    ApplyDifferentialDataTasks_t::iterator appiter(applyTasks.begin());
    ApplyDifferentialDataTasks_t::iterator append(applyTasks.end());
    for (/* empty */; appiter != append; ++appiter)
    {
        (*appiter)->PostMsg(DIFF_APPLY_DATA, DIFF_HIGH_PRIORITY);
    }

    while (!done)
    {
        if (QuitRequested())
        {
            ok = false;
            break;
        }

        ACE_Message_Block *mb;
        ACE_Time_Value wait = ACE_OS::gettimeofday ();
        int waitSeconds = WindowsWaitTimeInSeconds;
        wait.sec (wait.sec () + waitSeconds);
        if (-1 == m_MsgQueue.dequeue_head(mb, &wait))
        {
            if (errno == EWOULDBLOCK || errno == ESHUTDOWN)
            {
                continue;
            }
            ok = false;
            break;
        }

        switch(mb->msg_type())
        {

            case DIFF_QUIT:
            case DIFF_EXCEPTION:

                DebugPrintf(SV_LOG_DEBUG, "%s Got Exception/Quit message\n", FUNCTION_NAME);
                done = true;
                ok = false;
                // Now Set Quit Requested (m_Quit) so that any one waiting for free disk/memory knows that
                // they should come  out of the loop
                SignalQuit();
                break;


            case DIFF_APPLY_DATA_DONE:

                DebugPrintf(SV_LOG_DEBUG, "%s Got DIFF_APPLY_DATA_DONE message\n", FUNCTION_NAME);

                --currapplyTaskCount;
                ++applyTaskDoneCount;

                if(applyTaskDoneCount >= applyTasks.size())
                {
                    done = true;
                    break;
                }

                // if there is only one pending apply task
                // it can apply all the pending diffs, it need not
                // wait for next sequence point
                if(currapplyTaskCount == 1)
                {

                    // all threads have reached sequence point, let them fetch the next sequence
                    ApplyDifferentialDataTasks_t::iterator applyiter(applyTasks.begin());
                    ApplyDifferentialDataTasks_t::iterator applyend(applyTasks.end());
                    for (/* empty */; applyiter != applyend; ++applyiter)
                    {
                        (*applyiter)->PostMsg(DIFF_APPLY_ALL_DATA, DIFF_HIGH_PRIORITY);
                    }

                    break;
                }


                if(applyTaskSeqPtReached >= currapplyTaskCount)
                {
                    applyTaskSeqPtReached = 0;

                    IncrementSyncPoint(cbSeqCountInNSecs);

                    // all threads have reached sequence point, let them fetch the next sequence
                    ApplyDifferentialDataTasks_t::iterator applyiter(applyTasks.begin());
                    ApplyDifferentialDataTasks_t::iterator applyend(applyTasks.end());
                    for (/* empty */; applyiter != applyend; ++applyiter)
                    {
                        (*applyiter)->PostMsg(DIFF_APPLYUPTO_NEXTSEQPT, DIFF_HIGH_PRIORITY);
                    }

                    break;
                }

                break;

            case DIFF_SEQPT_REACHED:

                DebugPrintf(SV_LOG_DEBUG, "%s Got DIFF_SEQPT_REACHED message\n", FUNCTION_NAME);

                ++applyTaskSeqPtReached;
                if(applyTaskSeqPtReached >= currapplyTaskCount)
                {
                    applyTaskSeqPtReached = 0;
                    IncrementSyncPoint(cbSeqCountInNSecs);

                    // all threads have reached sequence point, let them fetch the next sequence
                    ApplyDifferentialDataTasks_t::iterator applyiter(applyTasks.begin());
                    ApplyDifferentialDataTasks_t::iterator applyend(applyTasks.end());
                    for (/* empty */; applyiter != applyend; ++applyiter)
                    {
                        (*applyiter)->PostMsg(DIFF_APPLYUPTO_NEXTSEQPT, DIFF_HIGH_PRIORITY);
                    }
                }

                break;

            default:
                DebugPrintf(SV_LOG_DEBUG, "%s Got unknown message\n", FUNCTION_NAME);
                ok = false;
                done = true;
                break;
        }
        mb->release();
    }


    // TODO: for now play it safe and tell them to exit even if we got here as a
    // result of all the tasks exiting. there is no harm posting messages to exited tasks.
    // eventually we can be smarter about this
    DebugPrintf(SV_LOG_DEBUG, "Posting DIFF_QUIT message to all\n");

    ApplyDifferentialDataTasks_t::iterator applyiter(applyTasks.begin());
    ApplyDifferentialDataTasks_t::iterator applyend(applyTasks.end());
    for (/* empty */; applyiter != applyend; ++applyiter)
    {
        (*applyiter)->PostMsg(DIFF_QUIT, DIFF_HIGH_PRIORITY);
    }

    m_ThreadManager.wait();
    return ok;
}

// --------------------------------------------------------------------------
// notifies cx diffs were drained, this is let cx know diffs are being drained
// --------------------------------------------------------------------------
void DifferentialSync::NotifyCxDiffsDrained(const time_t& timeToApplyDiffs)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    GroupInfo::VolumeInfos_t::iterator iter(m_GroupInfo.Targets().begin());
    GroupInfo::VolumeInfos_t::iterator end(m_GroupInfo.Targets().end());
    for (/* empty */; iter != end; ++iter) {
        if ((*iter)->OkToProcessDiffs()) {

            //
            // PR #5393
            // wrap all configurator snapshot related calls
            //
            SV_ULONGLONG applyRate = 0;
            if(timeToApplyDiffs != 0)
            {
                applyRate = ((*iter)->GetTotalDiffSize())/timeToApplyDiffs;
            }
            notifyCxDiffsDrained((*TheConfigurator),(*iter)->Name(), applyRate);
        }
    }
}

//Bug #8025
/*
 * FUNCTION NAME :  DifferentialSync::GetFileList
 *
 * DESCRIPTION : Acquires the lock. and then gets the list of differential files
 *
 *
 *
 * INPUT PARAMETERS : i. cachelocation ii. mode
 *
 * OUTPUT PARAMETERS : list of files
 *
 * NOTES :
 *
 *
 * return value : false if either getting filelist failed or acquiring the lock fails
 *
 */
bool DifferentialSync::GetFileList(CxTransport::ptr cxTransport, const std::string& cacheLocation, FileInfos_t& fileInfos)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n", FUNCTION_NAME, cacheLocation.c_str());

    bool rv = true;

    do
    {
        std::string fullLocation = cacheLocation;
        fullLocation += ACE_DIRECTORY_SEPARATOR_CHAR_A;
        fullLocation += m_GroupInfo.SearchPattern();

        const std::string lockPath = cacheLocation + ACE_DIRECTORY_SEPARATOR_CHAR_A + "pending_diffs.lck";
        // PR#10815: Long Path support
        ACE_File_Lock lock(getLongPathName(lockPath.c_str()).c_str(), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
        ACE_Guard<ACE_File_Lock> guard(lock);

        if(!guard.locked())
        {
            DebugPrintf(SV_LOG_DEBUG,
                        "\n%s encountered error (%d) (%s) while trying to acquire lock %s.",
                        FUNCTION_NAME,ACE_OS::last_error(),
                        ACE_OS::strerror(ACE_OS::last_error()),lockPath.c_str());
            rv = false;
            break;
        }

        if (!cxTransport->listFile(fullLocation, fileInfos))
        {
            DebugPrintf(SV_LOG_ERROR,
                        "FAILED %s cxTransport->GetFileList %s failed with error:%s\n",
                        FUNCTION_NAME, fullLocation.c_str(), cxTransport->status());
            rv = false;
            break;
        }

    } while (0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n", FUNCTION_NAME);

    return rv;
}


// --------------------------------------------------------------------------
// retrieves diff info from the server
// --------------------------------------------------------------------------
bool DifferentialSync::GetDifferentialInfo(VolumeInfo::Ptr volumeInfo)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    // TODO: need away to abort this if a quit is requested as the list could be large
    int entries = 0;

    CxTransport::ptr cxTransport;
    try 
    {
        cfs::CfsPsData cfsPsData = getCfsPsData();
        cxTransport.reset(new CxTransport(TRANSPORT_PROTOCOL_FILE, TransportSettings(), volumeInfo->Secure(), cfsPsData.m_useCfs, cfsPsData.m_psId));
    }
    catch (std::exception const& e)
    {
        DebugPrintf(SV_LOG_ERROR, "%s create CxTransport failed: %s\n", FUNCTION_NAME, e.what());
        return false;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR, "%s create CxTransport failed: unknonw error\n", FUNCTION_NAME);
        return false;
    }

    FileInfos_t fileInfos;

    if(!GetFileList(cxTransport, volumeInfo ->CacheLocation(), fileInfos))
    {
        DebugPrintf(SV_LOG_ERROR,
                    "FAILED DifferentialSync::GetDifferentialInfo %s %s failed\n",
                    FUNCTION_NAME, volumeInfo ->CacheLocation().c_str());
        return false;
    }

    // now copy the filelist data
    FileInfos_t::iterator iter(fileInfos.begin());
    FileInfos_t::iterator iterEnd(fileInfos.end());
    for (/* empty */; iter != iterEnd; ++iter) {
        DiffInfo::DiffInfoPtr diffInfo(new DiffInfo(volumeInfo.get(),
                                                    m_GroupInfo.DiffLocation(),
                                                    (*iter).m_name.c_str(),
                                                    (*iter).m_size));
        volumeInfo->Add(diffInfo);
        volumeInfo->AddToTotalSize((*iter).m_size);
    }

    return true;
}

// --------------------------------------------------------------------------
// applies the groups consistency policies
// --------------------------------------------------------------------------
void DifferentialSync::ApplyConsistencyPolicies()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return m_GroupInfo.ApplyConsistencyPolicies();
}

bool DifferentialSync::ProcessSnapshotRequests()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    //__asm int 3;
    GroupInfo::VolumeInfoList_t volInfos;
    svector_t bookmarks;
    bookmarks.clear();

    GroupInfo::VolumeInfos_t::iterator grp_iter(m_GroupInfo.Targets().begin());
    GroupInfo::VolumeInfos_t::iterator grp_end(m_GroupInfo.Targets().end());
    for( /* empty */ ; grp_iter != grp_end; ++grp_iter)
    {
        if(TheConfigurator->getPairState((*grp_iter) -> Name()) == VOLUME_SETTINGS::PAIR_PROGRESS)
        {
            if((*grp_iter) -> OkToProcessDiffs())
                volInfos.push_back((*grp_iter).get());
        }
    }

    if(volInfos.empty())
        return true;

    return ProcessSnapshotRequests(volInfos, bookmarks);
}

bool DifferentialSync::ProcessSnapshotRequests(GroupInfo::VolumeInfoList_t & volInfos, const svector_t & bookmarks)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    //__asm int 3;
    bool ok =true;
    SnapshotTasks_t snapshotTasks;
    boost::shared_ptr< ACE_Message_Queue<ACE_MT_SYNCH> > mq(new ACE_Message_Queue<ACE_MT_SYNCH>() );

    GroupInfo::VolumeInfoList_t::iterator grp_iter(volInfos.begin());
    GroupInfo::VolumeInfoList_t::iterator grp_end(volInfos.end());
    for (/* empty */; grp_iter != grp_end; ++grp_iter)
    {
        if(!(*grp_iter) -> OkToProcessDiffs())
            continue;

        string fmtName = (*grp_iter) -> Name();
        FormatVolumeName(fmtName);

        SNAPSHOT_REQUESTS requests;

        try
        {
            requests = TheConfigurator ->getSnapshotRequests(fmtName, bookmarks );
        }
        catch ( ContextualException& ce )
        {
            DebugPrintf(SV_LOG_ERROR,
                        "\ngetSnapshotRequests call to cx failed with exception %s.\n",ce.what());
        }
        catch ( ... )
        {
            DebugPrintf(SV_LOG_ERROR,
                        "\ngetSnapshotRequests call to cx failed with unknown exception.\n");
        }

        SNAPSHOT_REQUESTS::iterator requests_iter = requests.begin();
        SNAPSHOT_REQUESTS::iterator requests_end = requests.end();

        for( /* empty */ ; requests_iter != requests_end; ++requests_iter)
        {
            std::string id = (*requests_iter).first;
            SNAPSHOT_REQUEST::Ptr request = (*requests_iter).second;
            if (request -> operation == SNAPSHOT_REQUEST::ROLLBACK)
                (*grp_iter) -> MarkForRollback();

            if ( request ->eventBased )
            {
                //
                // PR #5393
                // wrap all configurator snapshot related calls
                // move the snapshot instance to ready on arrival of event
                // if it fails, ignore and continue to serve other requests
                //
                if(!makeSnapshotActive( (*TheConfigurator),request -> id))
                    continue;
            }

            SnapshotTaskPtr snapTask(new SnapshotTask(id, mq,request, &m_ThreadManager));
            if( -1 == snapTask ->open())
            {
                DebugPrintf(SV_LOG_ERROR, "FAILED DifferentialSync::ProcessSnapshotRequests vol=%%s snapTask.open: %d\n", (*grp_iter) -> Name(), ACE_OS::last_error());
                ok = false;
                continue;
            }

            snapshotTasks.insert(snapshotTasks.end(), SNAPSHOTTASK_PAIR(id,snapTask));
        }
    }

    if(snapshotTasks.empty())
        return ok;

    WaitForSnapshotTasks(snapshotTasks, mq);

    return ok;
}


//
// Function to invoke appropriate configurator call depending on CDPMessage Update or progress
//

bool  DifferentialSync::NotifyCxOnSnapshotProgress(const CDPMessage * msg)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    int timeval = 0 , rpoint = 0 , state =0 ;
    SV_ULONGLONG VsnapId=0;

    if(msg ->operation == SNAPSHOT_REQUEST::PIT_VSNAP
       || msg ->operation == SNAPSHOT_REQUEST::RECOVERY_VSNAP
       || msg ->operation == SNAPSHOT_REQUEST::VSNAP_UNMOUNT)
    {

        if(msg ->type != CDPMessage::MSG_STATECHANGE )
            return true;

        if(msg ->executionState != SNAPSHOT_REQUEST::Failed)
            return true;
    }


    std::stringstream cxstring;
    if (msg ->type == CDPMessage::MSG_STATECHANGE)
    {
        if(msg ->executionState == SNAPSHOT_REQUEST::SnapshotPrescriptRun
           || msg ->executionState == SNAPSHOT_REQUEST::RecoveryPrescriptRun
           || msg ->executionState == SNAPSHOT_REQUEST::RecoveryPrescriptRun
           || msg ->executionState == SNAPSHOT_REQUEST::SnapshotPostscriptRun
           || msg ->executionState == SNAPSHOT_REQUEST::RecoveryPostscriptRun
           //|| msg ->executionState == SNAPSHOT_REQUEST::RollbackPrescriptRun
           || msg ->executionState == SNAPSHOT_REQUEST::RollbackPostScriptRun)
            return true;

        switch (msg ->executionState)
        {
            case SNAPSHOT_REQUEST::Ready:
                state = 1 ;
                break;

            case SNAPSHOT_REQUEST::Complete:
                state = 4 ;
                break;

            case SNAPSHOT_REQUEST::Failed:
                state = 5 ;
                break;

            case SNAPSHOT_REQUEST::SnapshotStarted:
            case SNAPSHOT_REQUEST::RecoveryStarted:
                state = 2 ;
                break;

            case SNAPSHOT_REQUEST::SnapshotInProgress:
            case SNAPSHOT_REQUEST::RecoverySnapshotPhaseInProgress:
                state = 3 ;
                break;

            case SNAPSHOT_REQUEST::RecoveryRollbackPhaseStarted:
            case SNAPSHOT_REQUEST::RollbackStarted:
                state = 6 ;
                break;

            case SNAPSHOT_REQUEST::RecoveryRollbackPhaseInProgress:
            case SNAPSHOT_REQUEST::RollbackInProgress:
                state = 7 ;
                break;
        }
        if(state)
        {
            //
            // PR #5393
            // wrap all configurator snapshot related calls
            //
            if(!notifyCxOnSnapshotStatus((*TheConfigurator), msg->id , timeval ,VsnapId, msg ->err , state))
            {
                return false;
            }
        }


    }
    else        if (msg ->type == CDPMessage::MSG_PROGRESSUPDATE)
    {
        //
        // PR #5393
        // wrap all configurator snapshot related calls
        //
        if(!notifyCxOnSnapshotProgress((*TheConfigurator),msg->id , msg->progress , rpoint))
        {
            return false;
        }

    }

    return true;
}

// --------------------------------------------------------------------------
// launches the apply and get tasks and waits for them to finish
// --------------------------------------------------------------------------
bool DifferentialSync::ProcessDifferentials()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool ok = true;

    // apply diff data tasks
    ApplyDifferentialDataTasks_t applyTasks;

    CalculateSyncPoint();

    GroupInfo::VolumeInfos_t::iterator iter(m_GroupInfo.Targets().begin());
    GroupInfo::VolumeInfos_t::iterator end(m_GroupInfo.Targets().end());
    for (/* empty */; iter != end; ++iter)
    {

        if ((*iter)->OkToProcessDiffs())
        {

            ApplyDifferentialDataTaskPtr applyTask(new ApplyDifferentialDataTask(this,
                                                                                 &((*iter)->DiffInfos()),
                                                                                 (TargetCount() > 1),
                                                                                 &m_ThreadManager));
            if (-1 == applyTask ->open())
            {
                DebugPrintf(SV_LOG_ERROR,
                            "FAILED DifferentialSync::ProcessDifferentials applyTask.open: %d\n",
                            ACE_OS::last_error());
                ok = false;
                break;
            }
            applyTasks.push_back(applyTask);

        }
    }

    // if there was an error while creating any task,
    // ensure all previously created tasks also quit
    if( false == ok)
    {

        ApplyDifferentialDataTasks_t::iterator applyiter(applyTasks.begin());
        ApplyDifferentialDataTasks_t::iterator applyend(applyTasks.end());
        for (/* empty */; applyiter != applyend; ++applyiter)
        {
            (*applyiter)->PostMsg(DIFF_QUIT, DIFF_HIGH_PRIORITY);
        }

        // wait till all threads exit
        m_ThreadManager.wait();

        return false;
    }

    if(!WaitForProcessDifferentialTasks(applyTasks))
        return false;

    return ok;
}

// --------------------------------------------------------------------------
// calculates the time from which we need to start syncing up
// --------------------------------------------------------------------------
void DifferentialSync::CalculateSyncPoint()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    AutoLock SetSyncPointlock( m_SyncPointLock );
    m_SyncPoint = 0;

    GroupInfo::VolumeInfos_t::iterator iter(m_GroupInfo.Targets().begin());
    GroupInfo::VolumeInfos_t::iterator end(m_GroupInfo.Targets().end());
    for (/* empty */; iter != end; ++iter)
    {

        if ((*iter)->OkToProcessDiffs())
        {
            DiffInfo::DiffInfosOrderedEndTime_t diffInfos = (*iter)->DiffInfos();
            DiffInfo::DiffInfosOrderedEndTime_t::iterator diff_iter(diffInfos.begin());
            DiffInfo::DiffInfosOrderedEndTime_t::iterator diff_end(diffInfos.end());
            if(( diff_iter != diff_end ) && ( m_SyncPoint < (*diff_iter) -> EndTime() ) )
                m_SyncPoint = (*diff_iter) -> EndTime();
        }
    }

    string syncpt;
    CDPUtil::ToDisplayTimeOnConsole(m_SyncPoint, syncpt);
    DebugPrintf(SV_LOG_DEBUG, "SYNCPOINT: %s\n", syncpt.c_str());
}

// --------------------------------------------------------------------------
// applies diff data and cleans up the diff data from cache and server
// --------------------------------------------------------------------------
bool DifferentialSync::ApplyData(DiffInfo::DiffInfoPtr diffInfo, bool lastFileToProcess)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    assert(m_GroupInfo.DifferentialOnlyMode() || m_GroupInfo.SnapshotAndDifferentialMode());
	return diffInfo->ApplyData(this, m_PreAllocate, lastFileToProcess);
}

// --------------------------------------------------------------------------
// general function to run tasks that need to operate on all primary targets
// this is for tasks that don't need to process a message queue
// --------------------------------------------------------------------------
template<class Task, class TaskPtr>
bool DifferentialSync::ProcessAllTargets()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::list<TaskPtr> tasks;

    GroupInfo::VolumeInfos_t::iterator iter(m_GroupInfo.Targets().begin());
    GroupInfo::VolumeInfos_t::iterator end(m_GroupInfo.Targets().end());
    for (/* empty */; iter != end; ++iter) {
        if ((*iter)->OkToProcessDiffs()) {
            TaskPtr task(new Task(this, (*iter), &m_ThreadManager));
            task->open();
            tasks.push_back(task);
        }
    }

    // TODO: what if quit is requested while waiting for tasks to finish?
    // instead of a wait, should use something like WaitForProcessDifferentialTasks
    // or even better refactor WaitForProcessDifferentialTasks to be more general
    // and handle all task waits
    // would also need to add a Quit() function to these tasks
    m_ThreadManager.wait();

    return !QuitRequested(0); // don't wait just check
}

// --------------------------------------------------------------------------
// asks all primary targets to get their diff info
// --------------------------------------------------------------------------
bool DifferentialSync::GetDifferentialInfos()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return ProcessAllTargets<GetDifferentialInfoTask, GetDifferentialInfoTaskPtr>();
}

// ==========================================================================
// tasks
// ==========================================================================


// GetDifferentialInfoTask
// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int DifferentialSync::GetDifferentialInfoTask::open(void *args)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return this->activate(THR_BOUND);
}

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int DifferentialSync::GetDifferentialInfoTask::close(u_long flags)
{
    return 0;
}

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// get diff info for the target volume
// --------------------------------------------------------------------------
int DifferentialSync::GetDifferentialInfoTask::svc()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    try
    {
        m_DiffSync->GetDifferentialInfo(m_Target);
    } catch (DataProtectionException& dpe) {
        DebugPrintf(dpe.LogLevel(), dpe.what());
    } catch (std::exception& e) {
        DebugPrintf(SV_LOG_ERROR, e.what());
    }
    return 0;
}

// ApplyDifferentialDataTask
// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int DifferentialSync::ApplyDifferentialDataTask::open(void *args)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return this->activate(THR_BOUND);
    return 0;
}

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// --------------------------------------------------------------------------
int DifferentialSync::ApplyDifferentialDataTask::close(u_long flags)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return 0;
}

// --------------------------------------------------------------------------
// virtual function required by ACE_Task (see ACE docs)
// the main message loop for apllying diff data
// waits for messages and acts upon them
// --------------------------------------------------------------------------
int DifferentialSync::ApplyDifferentialDataTask::svc()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    try
    {

        bool done = false;
        while(!done)
        {

            ACE_Message_Block *mb;
            if (-1 == this->msg_queue()->dequeue_head(mb))
            {
                m_DiffSync->PostMsg(DIFF_EXCEPTION, DIFF_HIGH_PRIORITY);
                break;
            }

            switch(mb->msg_type())
            {

                case DIFF_QUIT:
                    DebugPrintf(SV_LOG_DEBUG, "%s Got DIFF_QUIT message\n", FUNCTION_NAME);

                    done = true;
                    break;

                case DIFF_APPLY_DATA:

                    DebugPrintf(SV_LOG_DEBUG, "%s Got DIFF_APPLY_DATA message\n", FUNCTION_NAME);

                    if(m_CrossedSeqPoint)
                        break;

                    if (!ApplyDiffs())
                    {
                        // encountered error,break out of while loop
                        m_DiffSync->PostMsg(DIFF_QUIT, DIFF_HIGH_PRIORITY);
                        done = true;
                        break;
                    }

                    if (m_CurDiffInfoIter == m_EndDiffInfoIter)
                    {
                        m_DiffSync->PostMsg(DIFF_APPLY_DATA_DONE, DIFF_NORMAL_PRIORITY);
                        done = true;
                        break;
                    }

                    break;

                case DIFF_APPLY_ALL_DATA:

                    DebugPrintf(SV_LOG_DEBUG, "%s Got DIFF_APPLY_ALL_DATA message\n", FUNCTION_NAME);

                    m_CrossedSeqPoint = false;

                    // there is only apply task pending
                    // so we consider it as non VG pair
                    m_VG = false;

                    if (!ApplyDiffs())
                    {
                        // encountered error,break out of while loop
                        m_DiffSync->PostMsg(DIFF_QUIT, DIFF_HIGH_PRIORITY);
                        done = true;
                        break;
                    }

                    if (m_CurDiffInfoIter == m_EndDiffInfoIter)
                    {
                        m_DiffSync->PostMsg(DIFF_APPLY_DATA_DONE, DIFF_NORMAL_PRIORITY);
                        done = true;
                        break;
                    }

                    break;

                case DIFF_APPLYUPTO_NEXTSEQPT:

                    DebugPrintf(SV_LOG_DEBUG, "%s Got DIFF_APPLYUPTO_NEXTSEQPT message\n", FUNCTION_NAME);

                    m_CrossedSeqPoint = false;

                    if (!ApplyDiffs())
                    {
                        // encountered error,break out of while loop
                        m_DiffSync->PostMsg(DIFF_QUIT, DIFF_HIGH_PRIORITY);
                        done = true;
                        break;
                    }

                    if (m_CurDiffInfoIter == m_EndDiffInfoIter)
                    {
                        m_DiffSync->PostMsg(DIFF_APPLY_DATA_DONE, DIFF_NORMAL_PRIORITY);
                        done = true;
                        break;
                    }

                    break;

                default:

                    DebugPrintf(SV_LOG_ERROR, "%s Got an unknown message\n", FUNCTION_NAME);
                    m_DiffSync->PostMsg(DIFF_QUIT, DIFF_HIGH_PRIORITY);
                    done = true;
                    break;
            }
            mb->release();
        }

    } catch (DataProtectionException& dpe) {
        DebugPrintf(dpe.LogLevel(), dpe.what());
        m_DiffSync->PostMsg(DIFF_EXCEPTION, DIFF_HIGH_PRIORITY);
    } catch (std::exception& e) {
        DebugPrintf(SV_LOG_ERROR, e.what());
        m_DiffSync->PostMsg(DIFF_EXCEPTION, DIFF_HIGH_PRIORITY);
    } catch (...) {
        DebugPrintf(SV_LOG_ERROR,
                    "DifferentialSync::ApplyDifferentialDataTask::svc caught an unknown exception\n");
        m_DiffSync->PostMsg(DIFF_EXCEPTION, DIFF_HIGH_PRIORITY);
    }

    return 0;
}

// --------------------------------------------------------------------------
// checks if a quit message is in the queue. allows multiple diffs to be
// applied but still be responsive to quit request
// --------------------------------------------------------------------------
bool DifferentialSync::ApplyDifferentialDataTask::Quit()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    ACE_Message_Block *mb;
    ACE_Time_Value waitTime;
    if (-1 == this->msg_queue()->peek_dequeue_head(mb, &waitTime)) {
        if (errno == EWOULDBLOCK || errno == ESHUTDOWN) {
            return false;
        }
        return true;
    }
    // we only peeked so don't release the message block
    return (DIFF_QUIT == mb->msg_type());
}

bool  DifferentialSync::ApplyDifferentialDataTask::CrossedSeqPoint()
{
    if((*m_CurDiffInfoIter) -> EndTime() >= m_DiffSync ->SyncPoint())
        return true;

    return false;
}

// --------------------------------------------------------------------------
// applies diffs in order until a diff is hit that can't be applied yet or
// it has applied all diffs or a quit is sitting on the message queue
// --------------------------------------------------------------------------
bool DifferentialSync::ApplyDifferentialDataTask::ApplyDiffs()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	DiffInfo::DiffInfosOrderedEndTime_t::iterator diffIter;

    while (m_CurDiffInfoIter != m_EndDiffInfoIter)
    {
        if(!(*m_CurDiffInfoIter)->ReadyToApply())
            break;

        if(m_VG && CrossedSeqPoint())
        {
            m_DiffSync->PostMsg(DIFF_SEQPT_REACHED, DIFF_HIGH_PRIORITY);
            m_CrossedSeqPoint = true;
            break;
        }

		diffIter = m_CurDiffInfoIter;
		++diffIter;

		if (Quit() || !m_DiffSync->ApplyData(*m_CurDiffInfoIter, (diffIter == m_EndDiffInfoIter)))
        {
            return false;
        }

        if((*m_CurDiffInfoIter) -> IfDataInMemory())
        {
            (*m_CurDiffInfoIter) ->ReleaseBuffer();
        }

        ++m_CurDiffInfoIter;
    }

    if(m_VG && (m_CurDiffInfoIter == m_EndDiffInfoIter))
    {
        m_DiffSync->PostMsg(DIFF_SEQPT_REACHED, DIFF_HIGH_PRIORITY);
    }

    return true;
}

// --------------------------------------------------------------------------
// posts a message to the message queue
// --------------------------------------------------------------------------
void DifferentialSync::ApplyDifferentialDataTask::PostMsg(int msgId, int priority)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    ACE_Message_Block *mb = new ACE_Message_Block(0, msgId);
    mb->msg_priority(priority);
    msg_queue()->enqueue_prio(mb);
}

// -----------------------------------------------------------------------------
// Each Apply thread applies data only upto the sync point
// then waits for the rest of the apply thread to catch up to the sync point
// before proceeding ahead
// ------------------------------------------------------------------------------
SV_ULONGLONG DifferentialSync::SyncPoint()
{
    AutoLock SetLock(m_SyncPointLock);
    return m_SyncPoint;
}

// -----------------------------------------------------------------------------
// after all apply threads have applied data upto the sync point
// update the sync point
// -----------------------------------------------------------------------------
void DifferentialSync::IncrementSyncPoint(const SV_ULONGLONG & increment)
{
    AutoLock SetLock(m_SyncPointLock);
    m_SyncPoint += increment;
}
