//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdpcli.h
//
// Description: 
//

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <boost/shared_ptr.hpp>

#include <iostream>
#include <iomanip>
#include <sstream>

#include "cdputil.h"


#ifdef SV_WINDOWS
#include "StreamEngine.h"
#include "StreamRecords.h"
#elif SV_UNIX
#include "devicefilter.h"
#endif

#include "cdpsnapshotrequest.h"
#include "cdpsnapshot.h"
#include "configurator2.h"
#include "configurevxagent.h"

#include "VsnapUser.h"
#include "cdpcoalesce.h"

#define ZFS "zfs"

typedef boost::shared_ptr<SnapshotTask> SnapshotTaskPtr;
typedef std::map<std::string, SnapshotTaskPtr> SnapshotTasks_t;
typedef std::pair<std::string, SnapshotTaskPtr> SNAPSHOTTASK_PAIR;

typedef std::map<std::string, VOLUME_STATE> SRC_NAMESTATE;
typedef std::pair<std::string, VOLUME_STATE> SRC_NAMESTATEPAIR; 

typedef std::map<std::string,std::string> NVPairs;

typedef boost::shared_ptr< ACE_Message_Queue<ACE_MT_SYNCH> >  ACE_Shared_MQ;

struct vsnapsizemountpoint
{
    SV_ULONGLONG size;
    std::string mountpoint;
    vsnapsizemountpoint():size(0){mountpoint.clear();}
};

class CDPCli
{
public:
    typedef void (*cdpProgressCallback_t)(const CDPMessage * msg, void *);

	CDPCli(int argc, char ** argv);
	~CDPCli();
	bool run();
    void set_progress_callback(cdpProgressCallback_t, void *);
    void set_no_console_output() { m_write_to_console = false; } 
    
    // PR# 16608
    // Users of cdpcli as an API can call below methods to
    // skip initialization and destruction of 
    // 1) Local logging
    // 2) TAL
    // 3) Configurator
    // 4) Platform dependencies (like COM on windows)
    // 5) Quit event
    // 
    // if they are already taking care of initialization and
    // destruction of these themselves
    //
    // Note 1: Please look into the code for initialization and destruction
    // of local logging and quit event. You should be initializing/destroying
    // them as we do in case of caller being cli.
    //
    // Note 2: You can call these method before calling run method.
    // 

    void skip_log_initialization() { m_skip_log_init = true; }
    void skip_tal_initialization() { m_skip_tal_init = true; }
    void skip_config_initialization() { m_skip_config_init = true; }
    void skip_platformdeps_initialization() { m_skip_platformdeps_init = true; }
    void skip_quitevent_initialization() { m_skip_quitevent_init  = true; }

    bool request_quit()
    { 
        m_quit = true;
        return true;
    }

protected:

    bool isCallerCli();
	bool SetupLocalLogging(bool callerCli);
	bool initializeTal();
	bool InitializeConfigurator();
	bool StopLocalLogging();
	bool StopConfigurator();
	void InitializePlatformDeps(void);
    void DeInitializePlatformDeps(void);
	bool init();
	bool uninit();

	void help(int argc, char **argv);
	void usage(char * progname, const std::string & operation ="", bool error=true);  
	bool parseInput();

	bool listcommonpt();
	bool showsummary();
	bool validate();
	bool printiodistribution();
	bool listevents(SV_ULONGLONG &eventCount);
	bool snapshot();
	bool recover();
	bool rollback();
	//added new function for source volume rollback  bugid:6701
	bool SetRollbackFlag();
	bool SetSnapshotFlag();
    bool SetSkipCheckFlag();
    bool SetSrcVolumeSize(SV_ULONGLONG & srcVolSz);
	bool IsStopReplicationFlagSet();
	bool SetDeleteRetention();
	//move retention
	bool moveRetentionLog();
	bool getSparsePolicySummary();
	bool displaySparseIOStatisticsFromDB(const std::string& dbname,const std::map<SV_ULONGLONG,SV_ULONGLONG> & policyList,const SV_ULONGLONG & profileinterval, bool verbose);
	bool getSparsePolicyListFromInput(const std::string & spasePolicies,SV_ULONGLONG & value);
	bool ShouldOperationRun(const std::string& volume, bool & shouldrun);


	//
    // PR#15803: CDPLI snapshots,recover, rollback functionalities should 
    //          provide an option to avoid direct i/o
    //
    bool do_not_usedirectio_option();

	bool use_fsaware_copy();

    // todo: create a copy of retention database (to be implemented)
	bool copy()     { return false; }
	bool vsnap();
	bool virtualvolume();

	// hide/unhide routines
	bool hide();
	bool unhidero();
	bool unhiderw();
	bool unhideall();
	bool verifiedtag();
	//lock/unlock routines
	bool lockbookmark();
	bool unlockbookmark();

	// fix the errors in the database
	bool fixerrors();

	bool displaystatistics();

	bool genCdpData();
	bool WriteSVDFile(std::string& destDir,
		SV_ULONGLONG& starttime,
		SV_ULONGLONG& endtime,
		std::vector<SVD_DIRTY_BLOCK>& drtds,
		SV_ULONGLONG& iosWritten,
		SV_ULONGLONG& noOfIOsPerCoalesceInterval,
		SV_INT& progressPercent,
		SV_INT& prevProgressPercent);

	bool RemountVsnaps();

	bool isvalidmountpoint(const std::string &src, std::string &strerr);

	bool hide_cxdown(const std::string & volumename);
	bool unhidero_cxdown(const std::string & volumename, const std::string & mountpt, const std::string & fstype);
	bool unhiderw_cxdown(const std::string & volumename, const std::string & mountpt, const std::string & fstype);

	bool SendRequestToCx(const std::string & volumename, VOLUME_STATE vs, const std::string & mountpt = "");
	bool PollForRequestCompletion(const std::string & volumename, VOLUME_STATE vs);
	//#15949 :
	bool showReplicationPairs();
	bool showSnapshotRequests();
	bool listTargetVolumes();
	bool listProtectedVolumes();
	bool displaysavings();
	#ifdef SV_WINDOWS
	bool reattachDismountedVolumes();
	#endif //End of SV_WINDOWS
    bool displaymetadata();
	bool readvolume();
	bool volumecompare();
	bool deleteStaleFiles();
	bool deleteUnusablePoints();
	bool registermt();
	bool processCsJobs();
	

private:

	bool isProtectedVolume(const std::string & volumename) const;
	bool isTargetVolume(const std::string & volumename) const;
    bool containsRetentionFiles(const std::string & volumename) const;
	bool isResyncing(const std::string & volumename) const;
	std::string getFSType(const std::string & volumename) const;
	SV_ULONGLONG getDiffsPendingInCX(const std::string & volumename) const;
	SV_ULONGLONG getCurrentRpo(const std::string & volumename) const;
	SV_ULONGLONG getApplyRate(const std::string & volumename) const;
	SV_ULONGLONG CurrentDiskUsage( std::string &cachelocation );

	// get the retention database path for specified volume
	bool getCdpDbName(const std::string& volume, std::string & dbpath);

	// get the host volume group settings from local persistent store
	bool getHostVolumeGroupSettings(HOST_VOLUME_GROUP_SETTINGS & settings);

	// display informational messages to user
	void DisplayNote1();
	void DisplayNote2();
	void DisplayNote3();

	// check if svagent is running
	bool ReplicationAgentRunning(bool &brunning);

	//
	// utility routines to parse the user input and fill snapshot pair information
	//
	bool getsnapshotpairs(SNAPSHOT_REQUESTS &requests);

	//
	// utility routines to parse the user input and create recovery options
	//
	bool getrecoverypairsanddb(SNAPSHOT_REQUESTS &requests);
	bool getrecoverypoint(SNAPSHOT_REQUESTS &requests);
	bool getprepostscript(SNAPSHOT_REQUESTS &requests, std::string & prescript, std::string & postscript);

	//
	// utility routines to parse the user input and create rollback options
	//
	bool getrollbackpairsanddb(SNAPSHOT_REQUESTS &requests);
	bool getrollbackpairsanddbfromcx(SNAPSHOT_REQUESTS &requests);
	void getrollbackcleanupmsg(SNAPSHOT_REQUESTS & requests);

	// utility routines to parse the user input and create vsnap options


	bool getvsnappairsanddb(SNAPSHOT_REQUESTS& requests);

	//to upgrade persistent store from 5.1 to 5.5
	bool upgradePersistentStore();

#ifndef SV_WINDOWS
	// upgrade routine to recreate vsnaps from 5.0
	bool loadNewDriverAndRecreateVsnaps();
	void printvsnapandvirtualvolumeinfo(UnixVsnapMgr& vsnapmgr);
	bool capturevsnapinfo( UnixVsnapMgr& vsnapmgr, std::map< std::string, std::pair<VsnapContextInfo, vsnapsizemountpoint> >& ctxinfoMap, std::string& failedVsnaps );
	bool removevsnaps( UnixVsnapMgr& vsnapmgr, const std::map< std::string, std::pair<VsnapContextInfo, vsnapsizemountpoint> >& ctxinfoMap);
	bool unloadvsnapdriver(void);
	bool loadvsnapdriver(void);
	void recreatevsnaps(UnixVsnapMgr& vsnapmgr, const std::map< std::string, std::pair< VsnapContextInfo, vsnapsizemountpoint> >& ctxinfoMap);
#endif

	bool recovervsnappairs();

	// utility routines to generate a unique devicename for vsnaps created from cdpcli
	unsigned long long GetUniqueVsnapId();
	// 
	// added to resolve the bug 5276
	//
	bool CreatePendingActionFile(const std::string &volumename, VOLUME_STATE volumeState);

	// 
	// routines for snapshot,recovery, rollback
	//
	bool ProcessSnapshotRequests(SNAPSHOT_REQUESTS &requests, const std::string & prescript, const std::string & postscript, bool parallel);  
	bool WaitForSnapshotTasks( SnapshotTasks_t , ACE_Shared_MQ &);
	void StopAbortedSnapshots( SnapshotTasks_t , ACE_Shared_MQ &);
	void PrintSnapshotStatusMessage(const CDPMessage * msg);


	bool printsummary(std::string const & dbname, bool show_space_usage, bool verbose);
	bool printiopattern(std::string const & dbname);

	CDPCli(CDPCli const & );
	CDPCli& operator=(CDPCli const & );

	int argc;
	char ** argv;

	svector_t validoptions;
	svector_t singleoptions;
	NVPairs   nvpairs;
	std::string    m_operation;
	ACE_Thread_Manager m_ThreadMgr;

	Configurator* m_configurator;
	bool m_bconfig;
	bool m_blocalLog;
    bool m_write_to_console;
    bool m_quit;

    bool m_skip_log_init;
    bool m_skip_tal_init;
    bool m_skip_config_init;
    bool m_skip_platformdeps_init;
    bool m_skip_quitevent_init;


   void (*progress_callback_fn)(const CDPMessage * msg, void *);
   void * progress_callback_param;

   bool quit_requested() { return m_quit; }

   	// Recovery operation during resync require replication to be paused
	// this requires CS to be reachable.If CS is not avaialable, a workaround could be
	// stop the vx services / ensure that the dataprotection is not running for the pair
	// and pass skip_replication_pause option to cdpcli
	// with skip_replication_pause  option, cdpcli tries to perform recovery operations without
	// sending pause request to CS.

	bool SkipReplicationPause();

	void Print_RollbackStats(RollbackStats_t stats);
};




// todo: why are these global functions?
// todo: value of polling interval is currently hard coded
bool GetPollingInterval(SV_ULONG &pollingInterval);         
bool IsAbsolutePath(const std::string &mountPoint);

