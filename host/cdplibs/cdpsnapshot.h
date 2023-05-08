//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdpsnapshot.h
//
// Description: 
//

#ifndef CDPSNAPSHOT__H
#define CDPSNAPSHOT__H

#include <ace/Task.h>
#include <ace/Process_Manager.h>
#include "configurator2.h"
#include "cdpsnapshotrequest.h"
#include "localconfigurator.h"
#include "volume.h"
#include "cdpvolume.h"
#include "cdpdatabase.h"
#include <set>
#include <boost/logic/tribool.hpp>

#include "datacacher.h"

#define BLOCK_NUM(offset,size) offset/size
#define INDEX_IN_BITMAP_ARRAY(a) a/NBITSINBYTE
#define BIT_POSITION_IN_DATA(a) a%NBITSINBYTE
#define ALIGN_OFFSET(offset,size_to_align) (offset/size_to_align)*size_to_align
#define OFFSET_TO_CLUSTER_NUM(offset,bytespercluster) offset/bytespercluster

struct BlockUsageInfo
{
	SV_OFFSET_TYPE startoffset;
	SV_ULONGLONG starting_blocknum;
	SV_ULONGLONG block_size;
	SV_ULONGLONG block_count;
	SV_BYTE * bitmap;
	BlockUsageInfo(){
		startoffset = 0;
		block_size = 0;
		block_count = 0;
		bitmap = NULL;
	}
	~BlockUsageInfo(){
		if(bitmap)
			free(bitmap);
	}
};

typedef struct RollbackStats
{
	SV_UINT retentionfile_read_io;
	SV_UINT tgtvol_write_io;
	SV_ULONGLONG retentionfile_read_bytes;
	SV_ULONGLONG tgtvol_write_bytes;
	SV_ULONGLONG metadatafile_read_bytes;
	SV_UINT metadatafile_read_io;
	SV_ULONGLONG firstchange_timeStamp;
	std::string rollback_operation_starttime;
	std::string rollback_operation_endtime;
	SV_ULONGLONG lastchange_timeStamp;
	SV_ULONGLONG Recovery_Period;
}RollbackStats_t;

BOOST_TRIBOOL_THIRD_STATE(FsAwareCopyNotSupported);

class SnapshotTask : public ACE_Task<ACE_MT_SYNCH> {

public:

    SnapshotTask(std::string id,
        boost::shared_ptr< ACE_Message_Queue<ACE_MT_SYNCH> > mq,                                    
        SNAPSHOT_REQUEST::Ptr request,
        ACE_Thread_Manager* threadManager = 0) 
        : ACE_Task<ACE_MT_SYNCH>(threadManager),
        m_request(request),
        m_stateMgr(mq),
        m_id(id),
		m_stopReplicationAtCX(true)
    { m_quit = false; 
    m_state = SNAPSHOT_REQUEST::NotStarted;
    m_progress = 0;
	m_time_remaining_in_secs = 0;
    m_errno = CDPSUCCESS;
    m_bconfig = false;
    m_configurator = NULL;
    m_destCapacity = 0;
    m_err.clear();
	m_srcVolRollback = false;
    m_skipCheck = false;
	m_directio = true;
	m_cdp_max_snapshot_rw_size = 0;
	m_max_rw_size = 0;
	m_sector_size = 0;
	m_use_fsawarecopy = false;

    }

    virtual int open(void *args = 0);
    virtual int close(u_long flags = 0);
    virtual int svc();
    void PostMsg(int msgId, int priority);
    bool Quit(int waitSeconds =0);
	//added new member variable m_srcVolRollback if the rollback volume is source volume 
	//this method is to set the varible for source volume  bugid:6701
	void SetVolumeRollback(bool rollback){m_srcVolRollback=rollback;}
    void SetSkipCheck(bool skipCheck){m_skipCheck=skipCheck;}
	//Added for fixing bug#7906
	void SetStopReplicationFlag(bool stopReplication){m_stopReplicationAtCX=stopReplication;}
    unsigned int GetRetries();
    unsigned int GetRetryDelay();
	
	// PR#15803 - 7.0> CDPLI snapshots,recover, rollback functionalities 
    //                 should provide an option to avoid direct i/o
    void do_not_use_directio() { m_directio = false; }
    void use_directio()        { m_directio = true; }
	void SetUseFSAwareCopy(bool useFSAwareCopy) {m_use_fsawarecopy = useFSAwareCopy;}
	
bool IsDiskOrPartitionValidDestination(const std::set<std::string> & disknames, std::string &errormsg); 
bool IsValidDeviceToProceed(const std::string & volumename,std::string &errormsg);
RollbackStats_t get_io_stats();

protected:
    bool snapshot();
    bool recover();
    bool rollback();
    bool vsnap_mount();
    bool vsnap_unmount();

    bool SendStateChange();
    bool SendProgress();
    bool runprescript();
    bool runpostscript();
    bool WaitForProcess(ACE_Process_Manager *, pid_t);
    void fail();
    void failImmediate();

    bool InitializeConfigurator();

    //
    // checks on snapshot source
    // 1) should exist
    // 2) should be hidden volume
    // 3) should be replicated volume in case of recovery/rollback (and vsnap)
    // 4) should not be in resync for physical snapshot (and pit vsnap)

    //
    // checks on snapshot destination
    // 1) should exist
    // 2) should not be protected volume
    // 3) should not be replicated volume
    // 4) should not contain retention files
    // 5) should not contain vsnap map files
    // 6) should not contain volpack data files
    // 7) should not be readonly
    // 8) should not contain mounted volumes
    // 9) should not contain partitions


    // 
    // common checks
    // 1) source and target have to be different
    // 2) target capacity should be greater than source


    bool isProtectedVolume(const std::string & vol) const;
    bool isTargetVolume(const std::string & vol) const;
    bool containsRetentionFiles(const std::string & volumename) const;
    bool containsVsnapFiles(const std::string & volumename) const;
    bool containsMountedVolumes(const std::string & volumename, std::string & mountedvol) const;
    bool IsValidDestination(const std::string & volumename, std::string &errormsg);
    bool isResyncing(const std::string & volumename) const;
    bool sameVolumeCheck(const std::string & source, const std::string & dest) const;
    std::string getFSType(const std::string & vol) const;
    bool computeSrcCapacity(const std::string & volumename, SV_ULONGLONG & capacity);
    bool computDestCapacity(const std::string & volumename, SV_ULONGLONG & capacity);
    bool makeVolumeAvailable(const std::string & volumename, const std::string & mountpoint,const std::string & fsType);
    bool copydata(cdp_volume_t & src, cdp_volume_t & dest, const SV_ULONGLONG & capacity);
    bool removevsnap(const std::string & volumename);
	bool rollback(CDPDatabase &db, CDPDRTDsMatchingCndn & cndn, cdp_volume_t & tgtvol, 
		const SV_ULONGLONG & tsfc,
		const SV_ULONGLONG & tslc);
	bool rollbackPartiallyAppliedChanges(CDPDatabase &db, cdp_volume_t & tgtvol);
	bool readDRTDFromDataFile(BasicIo &bIo, char *buffer, const cdp_rollback_drtd_t &drtd);
	bool write_sync_drtd_to_volume(cdp_volume_t & tgtvol, char *buffer, const cdp_rollback_drtd_t &drtd);


	bool isBlockUsed(SV_ULONGLONG blocknum,BlockUsageInfo &blockusageinfo );
	boost::tribool copydata_forfsaware_volumes(cdp_volume_t & src, cdp_volume_t &dest, const SV_ULONGLONG & capacity,std::string filesystem);
	bool get_used_block_information(std::string devicename, std::string destname, BlockUsageInfo &blockusageinfo);
	std::string GetVsnapVolumeName(const std::string & vol,const std::string & dest);
	void Initialize_RollbackStats();

private:

	bool SetVolumesCache();
	cdp_volume_t::CDP_VOLUME_TYPE GetDeviceType(std::string & name);
	void GetDeviceProperties(const std::string & name, std::string & displayName, cdp_volume_t::CDP_VOLUME_TYPE & devType);


    std::string m_id;
    SNAPSHOT_REQUEST::Ptr m_request;
    boost::shared_ptr< ACE_Message_Queue<ACE_MT_SYNCH> > m_stateMgr;

    SNAPSHOT_REQUEST::ExecutionState m_state;
    SV_ULONG m_progress;
	SV_LONGLONG m_time_remaining_in_secs;
    SV_INT m_errno;
    std::string m_err;
    bool m_quit;
	bool m_srcVolRollback;
    bool m_skipCheck;
	bool m_directio;
    Configurator * m_configurator;
    bool m_bconfig;
    SV_ULONGLONG m_destCapacity;
    LocalConfigurator m_localConfigurator;

	//Added for fixing bug#7906
	bool m_stopReplicationAtCX;
	SV_UINT m_cdp_max_snapshot_rw_size;
	SV_UINT m_max_rw_size;
	SV_ULONGLONG m_sector_size;
	bool m_use_fsawarecopy;
	RollbackStats_t m_rollbackstats;
	
	DataCacher::VolumesCache m_VolumesCache; ///< vic cache

};

typedef boost::shared_ptr<SnapshotTask> SnapshotTaskPtr;
typedef std::map<std::string, SnapshotTaskPtr> SnapshotTasks_t;
typedef std::pair<std::string, SnapshotTaskPtr> SNAPSHOTTASK_PAIR;
#endif


