//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdpv2writer.h
//
// Description: 
//

#ifndef CDPV2WRITER__H
#define CDPV2WRITER__H

#include <ace/Synch.h>
#include <ace/Proactor.h>
#include <ace/Asynch_IO.h>
#include <ace/Message_Block.h>
#include <ace/Atomic_Op.h>

#include "cdpv1database.h"
#include "bitmap.h"
#include "inm_md5.h"
#include "diffsortcriterion.h"
#include "cdputil.h"

#include "sharedalignedbuffer.h"

#include "cdpv3bitmap.h"
#include "cdpv3databaseimpl.h"
#include "cdpv3transaction.h"

#include "portablehelpersmajor.h"
#include "snapshotrequestprocessor.h"
#include "flushandholdwritetxn.h"
#include "cdpredologfile.h"
#include <boost/logic/tribool.hpp>

#include "cdp_resync_txn.h"

#ifdef SV_UNIX
#include "portablehelpersminor.h"
#endif /* SV_UNIX */

#include "usecfs.h"

#include "azureagent.h"
#include "extendedlengthpath.h"

#include "genericprofiler.h"

//
// these structures are used when
// DICheck and DIverify are enabled
// digest = checksum of the data from the differential file
// volumedigest = checksum of the data from the volume after data has been written to the volume
//
struct DiffChecksum
{
    SV_OFFSET_TYPE offset;
    SV_LONGLONG length;
    unsigned char digest[INM_MD5TEXTSIGLEN/2];
    unsigned char volumedigest[INM_MD5TEXTSIGLEN/2];
};


//
// filename = differential file name
// volume_cksums_count = no. of entries for which volumedigest has been computed
// 
struct FileChecksums
{
    std::string filename;
    SV_UINT volume_cksums_count;
    std::vector<DiffChecksum> checksums;

    FileChecksums() : volume_cksums_count(0){}
    void clear() { filename.clear(); volume_cksums_count = 0; checksums.clear(); }
};

//
// common structure used by performance routine
//
struct PerfTime
{
#ifdef WIN32
    LARGE_INTEGER perfCount;
#else
    ACE_Time_Value perfCount;
#endif
};

//
// to keep track of memory allocations
// 
struct cdp_memorymap_key_t
{
    cdp_memorymap_key_t(ACE_HANDLE h, const SV_OFFSET_TYPE &offset)
        :handle(h),offset(offset) 
        {
        };

    ACE_HANDLE handle;
    SV_OFFSET_TYPE offset;
};

struct cdp_memorymap_key_cmp_t
{
    bool operator()(const cdp_memorymap_key_t & lhs, const cdp_memorymap_key_t & rhs) const
        {
            if (lhs.handle < rhs.handle)
                return true;

            if(lhs.handle == rhs.handle)
                return (lhs.offset < rhs.offset);

            return false;
        }
};
typedef std::map<cdp_memorymap_key_t, SharedAlignedBuffer, cdp_memorymap_key_cmp_t> cdp_memoryallocs_t;

//
// 1) used for sparse retention when there are multiple retention files, to update retention
// 2) used for virtual volumes when there are multiple sparse files, to update the volume
//

struct cdp_async_writehandle_t
{
    cdp_async_writehandle_t(ACE_HANDLE &handle, ACE_Asynch_Write_File_Ptr ptr)
        :handle(handle), asynch_handle(ptr)
        {
        }

    ACE_HANDLE handle;
    ACE_Asynch_Write_File_Ptr asynch_handle;
};

typedef std::map<std::string, cdp_async_writehandle_t> cdp_asyncwritehandles_t;

//
// used for virtual volumes when there are multiple sparse files to read cow data
// 

typedef boost::shared_ptr<ACE_Asynch_Read_File> ACE_Asynch_Read_File_Ptr;
struct cdp_async_readhandle_t
{
    cdp_async_readhandle_t(ACE_HANDLE &handle, ACE_Asynch_Read_File_Ptr ptr)
        :handle(handle), asynch_handle(ptr)
        {
        }

    ACE_HANDLE handle;
    ACE_Asynch_Read_File_Ptr asynch_handle;
};

typedef std::map<std::string, cdp_async_readhandle_t> cdp_asyncreadhandles_t;

typedef std::map<std::string, ACE_HANDLE> cdp_syncwritehandles_t;
typedef std::map<std::string, ACE_HANDLE> cdp_syncreadhandles_t;

//
// structure cdp_cow_overlap_t,cdp_cow_nonoverlap_t, cpd_cow_location_t used 
// for sparse retention. cow data for overlap portion comes from 
// differential file, whereas nooverlap is read from the volume.
//
struct cdp_cow_overlap_t
{
    SV_UINT relative_voloffset;
    SV_UINT length;
    SV_OFFSET_TYPE fileoffset;
};

struct cdp_cow_nonoverlap_t
{
    SV_UINT relative_voloffset;
    SV_UINT length;
};

struct cdp_cow_location_t
{
    std::vector<cdp_cow_overlap_t> overlaps;
    std::vector<cdp_cow_nonoverlap_t> nonoverlaps;
};

typedef std::map<SV_UINT,  cdp_cow_location_t> cdp_cow_locations_t;

struct cdp_drtdv2_cmpfileid_t
{
    bool operator()(const cdp_drtdv2_fileid_t & lhs, const cdp_drtdv2_fileid_t & rhs) const
        {
            if (lhs.get_fileid() < rhs.get_fileid())
                return true;

            if(lhs.get_fileid() == rhs.get_fileid())
                return (lhs.get_file_offset() < rhs.get_file_offset());

            return false;
        }
};
typedef std::set<cdp_drtdv2_fileid_t,cdp_drtdv2_cmpfileid_t> cdp_drtdv2s_fileid_t;
typedef cdp_drtdv2s_fileid_t::const_iterator cdp_drtdv2s_fileid_constiter_t; 


BOOST_TRIBOOL_THIRD_STATE(ServiceQuitRequested);


class CDPV2Writer : public ACE_Handler
{
public:

    // ===================================================
    //  public interfaces
    //   
    //  1) constructor
    //    CDPV2Writer::CDPV2Writer
    //
    //  2) initialization routine
    //     init()
    //
    //  3) Apply Differential
    //     Applychanges
    //
    // ====================================================

    typedef boost::shared_ptr<CDPV2Writer> Ptr;

    CDPV2Writer(const std::string & volumename,
                const SV_ULONGLONG & source_capacity,
                const CDP_SETTINGS & settings,
                bool diffsync,
                cdp_resync_txn * resync_txn_mgr = 0,
				AzureAgentInterface::AzureAgent::ptr AzureAgent = AzureAgentInterface::AzureAgent::ptr(),
                const VolumeSummaries_t *pVolumeSummariesCache = 0,  ///< volumes cache to get to disk device name from disk name
                Profiler pSyncApply = ProfilerFactory::getProfiler(PASSIVE_PROFILER),
                const bool hideBeforeApplyingSyncData = true
                );

    virtual ~CDPV2Writer();

    bool init();

    virtual bool Applychanges(const std::string & filename, 
                              long long& totalBytesApplied,
                              char* source = 0,
                              const SV_ULONG sourceLength = 0,
                              SV_ULONGLONG* offset = 0,
                              SharedBitMap*bm = 0,
                              int ctId = 0,
							  bool lastFileToProcess = false);

    // ============================================================
    //  Differential parsing routines
    //    parseInMemoryFile
    //    parseOnDiskFile
    // ============================================================

    bool parseOnDiskFile(const std::string & filePath, DiffPtr & metadata);

protected:

private:

    void setbiodirect(BasicIo::BioOpenMode &mode);
    std::string getNextCowFileName(const cdp_timestamp_t & , const cdp_timestamp_t &,const SV_UINT);
    bool error() { return m_error ; }

    // ========================================================
    //  replication mode and cdp state routines
    //  (invoked from init routine)
    //  isInInitialSync
    //  detect_cdp_version
    //  isInDiffSync
    // ========================================================

    bool isInInitialSync();
    void detect_cdp_version();
    bool isActionPendingOnVolume();
	
    unsigned int get_cdp_version() 
	{ 
            return m_cdpversion; 
	}

    bool isInDiffSync()
	{ 
            return m_isInDiffSync; 
	}

    // if cdp version is 3
    // this routine will be called from initialize routine
    // this basically sets the path and other limits
    bool initialize_cdp_bitmap_v3();


    // =========================================================
    // differential reading routines:
    //   canReadFileInToMemory
    //   readdatatomemory
    // =========================================================

    bool canReadFileInToMemory(const std::string& filePath, const SV_ULONGLONG & filesize);
    bool readdatatomemory(const std::string & filePath, char ** diff_data, 
                          SV_ULONG & diff_len, SV_ULONGLONG filesize);

    // ===========================================================
    //  uncompress routines:
    //    isCompressedFile
    //    InMemoryToInMemoryUncompress
    //    uncompressInMemoryToDisk
    //    uncompressOnDisk
    // ===========================================================

    bool isCompressedFile(const std::string& filePath);
    bool InMemoryToInMemoryUncompress(const std::string & filePath, 
                                      const char * diff_data, 
                                      SV_ULONG& diff_len,
                                      char ** pgunzip, 
                                      SV_ULONG &  gunziplen);

    bool uncompressInMemoryToDisk(std::string & filePath, 
                                  const char * diff_data, 
                                  SV_ULONG & diff_len);

    bool uncompressOnDisk(std::string& filePath);

    // ===============================================================
    //  exception handling:
    // 
    //  1) compressed file corruption
    //     renameCorruptedFileAndSetResyncRequired
    //     persistResyncRequired
    // ===============================================================

    bool renameCorruptedFileAndSetResyncRequired(const std::string& filePath);

    // ============================================================
    //  Differential parsing routines
    //    parseInMemoryFile
    //    parseOnDiskFile
    // ============================================================

    bool parseInMemoryFile(const std::string & filePath, const char * diff_data, 
                           const SV_ULONG & diff_len, DiffPtr & metadata);

    // =========================================================
    //  exception and validation routines:
    //  
    //  1) Functions for performing ordering checks
    //        canSkipDifferential
    //        isDiffFromCurrentResyncSession
    //        isDiffEndTimeGreaterThanStartTime
    //        isInOrderDiff
    //        isFirstFile
    //        isSequentialFile
    //        isInOrderDueToS2CommitFailure
    //        isOODDueToS2CommitFailure
    //        isCrashEventDiff
    //        isDiffPriorToCrashEvent
    //        isDiffPriorToProcessedFile
    //        isMissingDiffAvailable
    //        isMissingDiffAvailableInCacheDir
    //        isMissingDiffAvailableOnPS
    //   
    //  2) resync files
    //        isResyncDataAlreadyApplied
    //  
    //  3) validate if the file can be applied
    //         persistSourceSystemCrashEvent
    //         isFirstFileForThisResyncSession
    //         isSourceSystemCrashEventSet
    // =========================================================

    bool isDiffFromCurrentResyncSession(const std::string & filePath);

    bool canSkipDifferential(const std::string& filePath, bool& stopApplyingDiffs);
    bool isInOrderDiff(const std::string & filePath);
    bool isFirstFile(const std::string & filePath);
    bool isSequentialFile(const std::string & filePath);
    bool isInOrderDueToS2CommitFailure(const std::string & filePath);
    bool isOODDueToS2CommitFailure(const std::string& filePath);
    bool isDiffEndTimeGreaterThanStartTime(const std::string& filePath);
    bool isCrashEventDiff(const std::string& filePath);
    bool isDiffPriorToCrashEvent(const std::string& filePath, const std::string& crashEventDiffFileName);
    bool isDiffPriorToProcessedFile(const std::string& filePath);
    bool isMissingDiffAvailable(const std::string& filePath);
    bool isMissingDiffAvailableInCacheDir(const std::string& filePath,
                                          const TRANSPORT_CONNECTION_SETTINGS& transportsettings);
    bool isSilentDataIntergityIssueDetected(const std::string& filePath,
                                          const TRANSPORT_CONNECTION_SETTINGS& transportsettings);
    bool isMissingDiffAvailableOnPS(const std::string& filePath,
                                    const TRANSPORT_CONNECTION_SETTINGS& transportsettings);

    bool isResyncDataAlreadyApplied(SV_ULONGLONG* offset,
                                    SharedBitMap*bm, 
                                    DiffPtr change_metadata);

    bool persistSourceSystemCrashEvent(const std::string& filePath);

    bool isFirstFileForThisResyncSession(const DiffSortCriterion& sortCriterion);

    bool isSourceSystemCrashEventSet(std::string& crashEventDiffFileName);

    // =========================================================
    //  cdpv1 routines
    //   
    //  1) partial transaction exists? 
    //      isPartiallyApplied
    // 
    //  2) previous (partial) transaction
    //       get_partial_transaction_information
    //       update_partial_transaction_information
    //
    //  3) update_end_timestamp_information
    //     
    //
    //  4) routines to remember the last applied file
    //     update_previous_diff_information
    //
    // =========================================================


    bool isPartiallyApplied(const std::string& diffFileName,
                            const CDPLastRetentionUpdate& lastCDPtxn,
                            bool& partiallyApplied);

    bool get_partial_transaction_information(std::string& fileapplied,
                                             SV_UINT& changesapplied);

    bool update_partial_transaction_information(const std::string& fileapplied,
                                                const SV_UINT& changesapplied);

    bool update_end_timestamp_information(const SV_ULONGLONG& sequenceNumber,
                                          const SV_ULONGLONG& endTimeStamp,
                                          const SV_UINT& continuationId);


    bool update_previous_diff_information(const std::string& filename,
                                          const SV_UINT& version);

    // ====================================================
    //  timestamp order validation routines 
    // ====================================================

    bool verify_file_version(const std::string &diffname,DiffPtr change_metadata);
    bool verify_increasing_timestamp(const std::string & diffname,DiffPtr change_metadata);
    bool adjust_timestamp(DiffPtr change_metadata, const cdpv3transaction_t &txn);
    bool adjust_timestamp(DiffPtr change_metadata, const CDPLastRetentionUpdate& lastCDPtxn);


    // ==========================================================
    //   snapshot routine
    //     process_event_snapshots
    // ==========================================================

    bool process_event_snapshots(DiffPtr change_metadata);

    // ===========================================================
    //  replication state change from resync step 1 to step 2
    //     update_quasi_state
    // ===========================================================

    bool update_quasi_state(DiffPtr change_metadata);

    // ============================================================
    //  routines to determine recovery accuracy
    //     set_accuracy_mode
    // ============================================================

	bool check_and_setresync_on_cs_ifrequired();

    bool set_accuracy_mode(const std::string & filePath,
                           DiffPtr change_metadata);

    // =======================================================
    // update volume without retention enabled
    // =======================================================


    bool updatevolume_nocdp(const std::string & volumename,
                            const std::string & filePath, 
                            char * diff_data,
                            DiffPtr change_metadata);

    bool construct_nonoverlapping_drtds(const cdp_drtdv2s_constiter_t & drtdsIter, 
                                        const cdp_drtdv2s_constiter_t & drtdsEnd, 
                                        cdp_drtdv2s_t & drtds_to_apply);

    bool disjoint(const cdp_drtdv2_t & x, const cdp_drtdv2_t & y);
    bool contains(const cdp_drtdv2_t & original_drtd, const cdp_drtdv2_t & x, const cdp_drtdv2_t & y,
                  cdp_cow_overlap_t & overlap);
    bool r_overlap(const cdp_drtdv2_t & original_drtd,const cdp_drtdv2_t & x, const cdp_drtdv2_t & y,
                   cdp_drtdv2_t & r_nonoverlap,
                   cdp_cow_overlap_t &overlap);
    bool l_overlap(const cdp_drtdv2_t & original_drtd,const cdp_drtdv2_t & x, const cdp_drtdv2_t & y,
                   cdp_drtdv2_t & l_nonoverlap,
                   cdp_cow_overlap_t &overlap);
    bool iscontained(const cdp_drtdv2_t & original_drtd,const cdp_drtdv2_t & x, const cdp_drtdv2_t & y, 
                     cdp_drtdv2_t & l_nonoverlap,
                     cdp_drtdv2_t & r_nonoverlap,
                     cdp_cow_overlap_t &overlap);

    bool contains(const cdp_drtdv2_t & x, const cdp_drtdv2_t & y);
    bool r_overlap(const cdp_drtdv2_t & x, const cdp_drtdv2_t & y,
                   cdp_drtdv2_t & r_nonoverlap);
    bool l_overlap(const cdp_drtdv2_t & x, const cdp_drtdv2_t & y,
                   cdp_drtdv2_t & l_nonoverlap);
    bool iscontained(const cdp_drtdv2_t & x, const cdp_drtdv2_t & y, 
                     cdp_drtdv2_t & l_nonoverlap,
                     cdp_drtdv2_t & r_nonoverlap);




    // =======================================================
    //   
    // updatevolume
    //     updatevolume_asynch_inmem_difffile
    //        get_mem_required
    //            initiate_next_async_write
    //              handle_write_file
    //        flush_io
    // 							   
    //    updatevolume_synch_inmem_difffile
    //        write_sync_drtds_to_volume
    //           write_sync_drtd_to_volume
    // 
    //    updatevolume_asynch_ondisk_difffile
    //        readDRTDFromBio
    // 
    //    updatevolume_synch_ondisk_difffile 
    //
    // =======================================================

    bool updatevolume(const std::string & volumename,
                      const std::string& filePath,
                      const char *const diff_data, 
                      const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start,
                      const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end);

    // asynchronous write to volume, data available in memory
    bool updatevolume_asynch_inmem_difffile(const std::string & volumename,
                                            const char *const diff_data, 
                                            const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start,
                                            const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end);

    bool get_mem_required(const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                          const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end,
                          SV_UINT & mem_required);

    bool initiate_next_async_write(ACE_Asynch_Write_File_Ptr asynch_handle, 
                                   const std::string & filename,
                                   const char * buffer,
                                   const SV_UINT &length,
                                   const SV_OFFSET_TYPE& offset);

    void handle_write_file (const ACE_Asynch_Write_File::Result &result);   

    bool flush_io(ACE_HANDLE vol_handle, const std::string & volumename);

    // synchronous write to volume, data available in memory
    bool updatevolume_synch_inmem_difffile(const std::string & volumename,
                                           const char *const diff_data, 
                                           const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                                           const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end);

    bool get_drtds_limited_by_memory(const cdp_drtdv2s_constiter_t & start, 
                                     const cdp_drtdv2s_constiter_t & end, 
                                     const SV_ULONGLONG & maxUsableMemory, 
                                     cdp_drtdv2s_constiter_t & out_drtds_start,
                                     cdp_drtdv2s_constiter_t & out_drtds_end, 
                                     SV_ULONGLONG &memRequired);

    bool write_sync_drtds_to_volume(char *buffer, 
                                    const cdp_drtdv2s_constiter_t & drtds_to_write_begin,
                                    const cdp_drtdv2s_constiter_t & drtds_to_write_end);

    bool write_sync_drtd_to_volume(char *buffer, 
                                   const cdp_drtdv2_t &drtd);


    // asynchronous write to volume, data available on disk
    bool updatevolume_asynch_ondisk_difffile(const std::string & volumename,
                                             const std::string& filePath, 
                                             const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                                             const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end);

    //
    // read data from 
    //     disk diff file for volume update/reading overlapping cow data
    // or  volume for reading cow data
    //
    bool readDRTDFromBio(BasicIo &bIo,	char *buffer,
                         const SV_OFFSET_TYPE & offset,
                         const SV_UINT & length);


    // synchronous write to volume, data available on disk
    bool updatevolume_synch_ondisk_difffile(const std::string & volumename,
                                            const std::string& filePath, 
                                            const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                                            const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end);


    // =====================================================
    // 
    //   Vsnap map updates:
    //    updatevsnaps  
    //
    // =====================================================


    bool updatevsnaps_v1(const std::string & volumename, 
                         const std::string & cdp_data_file,
                         const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                         const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end);



    // ==========================================================
    //   
    //  DI checks
    //   calculate_checksums_from_volume
    //    issue_pagecache_advise
    //    ComputeHash
    //   WriteChecksums
    // 
    // ==========================================================

    bool calculate_checksums_from_volume(std::string const & volumename, 
                                         std::string const & filename,  
                                         const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start,
                                         const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end);

    bool issue_pagecache_advise(ACE_HANDLE & vol_handle, 
                                const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                                const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end,
                                int advise = INM_POSIX_FADV_WILLNEED);

    void ComputeHash(char const* buffer, 
                     SV_LONGLONG length, 
                     unsigned char digest[INM_MD5TEXTSIGLEN/2]);

    bool WriteChecksums(std::string const & volumename, 
                        std::string const & filename);

    // ==========================================================
    // 
    // Performance measurement routines
    // 
    // ==========================================================

    void getPerfTime(struct PerfTime& perfTime);
    double getPerfTimeDiff(struct PerfTime& perfStartTime, struct PerfTime& perfEndTime);
    bool WriteProfilingData(std::string const & volumename);


    // ========================================================
    //  update cdp cow data routines
    //     updatecdp
    //       updatecdpv1
    //       updatecdpv3
    // =========================================================

    bool updatecdp(const std::string & volumename,
                   const std::string & filePath, 
                   char * diff_data,
                   DiffPtr change_metadata);

    bool updatecdpv1(const std::string & volumename,
                     const std::string & filePath, 
                     char * diff_data,
                     DiffPtr change_metadata);


    // =========================================================
    //   cdpv1 routines
    //  
    //  1) Metadata
    //     fill_cdp_metadata_v1 (create metadata to memory)
    //     fill_cdp_header_v1  
    //     write_cdp_header_v1
    //     write_cow_data
    // =========================================================

    bool fill_cdp_metadata_v1(DiffPtr change_metadata, 
                              DiffPtr & cdp_metadata, 
                              SV_ULONGLONG baseoffset,
                              SV_ULONG & cdp_hdr_len,
                              SV_UINT & skip_bytes,
                              SV_OFFSET_TYPE & drtdStartOffset,
                              SV_ULONG & cdp_data_len);

    bool fill_cdp_header_v1(DiffPtr cdp_metadata,
                            char * cdp_header,
                            const SV_ULONGLONG & baseoffset,
                            const SV_ULONG & cdp_hdr_len,
                            const SV_UINT & skip_bytes,
                            const SV_OFFSET_TYPE & drtdStartOffset);

    bool write_cdp_header_v1(std::string cdp_data_file, 
                             DiffPtr & cdp_metadata,                        
                             const SV_ULONGLONG & baseoffset,
                             const SV_ULONG & cdp_hdr_len,
                             const SV_UINT & skip_bytes,
                             const SV_OFFSET_TYPE & drtdStartOffset);

    bool write_cow_data(const std::string & cdp_data_file, 
                        ACE_HANDLE & cdp_handle,
                        const char * cdp_data,
                        const SV_ULONG & cdp_buffer_len);


    // =================================================
    //  updatecdpv1 routines
    //
    //  routines to fetch non overlapping drtds:
    //    get_next_nonoverlapping_batch
    //    is_overlap
    // =================================================

    bool get_next_nonoverlapping_batch(const cdp_drtdv2s_constiter_t & drtds_begin, 
                                       const cdp_drtdv2s_constiter_t & drtds_end, 
                                       cdp_drtdv2s_constiter_t & nonoverlapping_batch_start,
                                       cdp_drtdv2s_constiter_t & nonoverlapping_batch_end);


    bool is_overlap(const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start,
                    const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end,
                    const cdp_drtdv2_t & drtd);



    // =====================================================
    //  updatecdpv1 routines
    //    
    //   Asynchronous COW update:
    //    update_cow_data_asynch_v1
    //      get_drtds_limited_by_memory
    //      issue_pagecache_advise
    //      read_async_drtds_from_volume
    //          initiate_next_async_read
    //          handle_read_file
    //      wait_for_xfr_completion
    //      write_cow_date (declared earlier)
    // =======================================================


    bool update_cow_data_asynch_v1(const std::string & volumename,
                                   const std::string & cdp_data_file,
                                   const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                                   const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end);

    // asynchronous reads from volume
    bool read_async_drtds_from_volume(const std::string & volumename, 
                                      char * cdp_data,
                                      const cdp_drtdv2s_constiter_t & drtds_to_read_begin,
                                      const cdp_drtdv2s_constiter_t & drtds_to_read_end);

    bool initiate_next_async_read(ACE_Asynch_Read_File & vol_handle, 
                                  char * cdp_data,  
                                  const SV_UINT &length,
                                  const SV_OFFSET_TYPE& offset);

    void handle_read_file (const ACE_Asynch_Read_File::Result &result);

    bool wait_for_xfr_completion();

    bool wait_for_xfr_completion(const SV_UINT & iocount);

    // ====================================================
    //  updatecdpv1 routines
    //    
    //  Synchronous COW update
    //     update_cow_data_synch_v1
    //        read_sync_drtds_from_volume
    //            readDRTDFromBio
    //         write_cow_data (declared earlier)
    //
    // =====================================================  

    bool update_cow_data_synch_v1(const std::string & volumename,
                                  const std::string & cdp_data_file,
                                  const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                                  const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end);

    // synchronous reads from volume
    bool read_sync_drtds_from_volume(const std::string & volumename,
                                     char *buffer, 
                                     const cdp_drtdv2s_constiter_t & drtds_to_read_begin,
                                     const cdp_drtdv2s_constiter_t & drtds_to_read_end);

    // ============================================
    // resource management routines
    // ============================================
	
    void add_memory_allocation(ACE_HANDLE h, const SV_OFFSET_TYPE &offset,const SV_UINT & mem_consumed, SharedAlignedBuffer ptr);
    bool release_memory_allocation(ACE_HANDLE h, const SV_OFFSET_TYPE &offset, const SV_UINT & mem_to_release);

    bool canIssueNextIo(const SV_UINT & mem_required);
    void incrementPendingIos();
    void decrementPendingIos();
    void incrementMemoryUsage(const SV_UINT & mem_consumed);
    void decrementMemoryUsage(const SV_UINT & mem_released);


    // ==============================================
    // sparse retention
    // ==============================================

    bool updatecdpv3(const std::string & volumename,
                     const std::string & filePath, 
                     char * diff_data,
                     DiffPtr change_metadata);


    // ============================================
    // metadata routines
    //  read_metadata_from_mdfile_v3
    //  rollback_partial_mdwrite_v3
    //  get_metadatafile_size_v3
    //  get_cowmetadata_for_timerange_v3
    //    get_cow_fileid_writeoffset_v3
    //      get_cow_filename_v3
    //      get_cowdatafile_size_v3
    //    construct_cow_location_v3
    //  write_metadata_v3
    // ============================================

    bool read_metadata_from_mdfile_v3(const std::string &metadata_filepath,
                                      cdpv3metadata_t &cow_metadata);

    bool rollback_partial_mdwrite_v3(const std::string &metadata_filepath,
                                     const SV_UINT &mdfilesize);

    bool get_metadatafile_size_v3(const std::string & metadata_filepath,
                                  SV_UINT & mdfilesize);

    bool get_cowmetadata_for_timerange_v3(const SV_ULONGLONG &current_part_hrstart, 
                                          const SV_ULONGLONG &current_part_hrend,
                                          DiffPtr change_metadata,
                                          const cdpv3transaction_t & txn,
                                          cdpv3metadata_t &cow_metadata);

    bool get_cow_fileid_writeoffset_v3(const SV_OFFSET_TYPE &read_voloffset, 
                                       const SV_UINT &read_vollength,
                                       const SV_ULONGLONG & newts,
                                       const cdpv3transaction_t & txn,
                                       cdpv3metadata_t  & cow_metadata,
                                       SV_UINT & write_fileid,
                                       SV_OFFSET_TYPE &write_offset);

    bool get_cow_fileid_writeoffset_v3(const SV_OFFSET_TYPE &read_voloffset, 
                                       const SV_UINT &read_vollength,
                                       cdpv3metadata_t  & cow_metadata,
                                       SV_UINT & write_fileid,
                                       SV_OFFSET_TYPE &write_offset,
                                       cdp_timestamp_t & old_iotime,
                                       cdp_timestamp_t & new_iotime);
    std::string get_cow_filename_v3(const SV_OFFSET_TYPE & read_voloffset,
                                    const cdp_timestamp_t & old_iotime,
                                    const cdp_timestamp_t & new_iotime);

    bool get_cowdatafile_size_v3(const std::string & cow_filepath,
                                 SV_ULONGLONG & filesize);


    bool construct_cow_location_v3(DiffPtr change_metadata, 
                                   SV_INT drtd_start,
                                   SV_INT drtd_end,
                                   const cdp_drtdv2_t & cow_drtd,
                                   cdp_cow_location_t & cow_locations);

    bool write_metadata_v3(const std::string &metadata_filepath,
                           const cdpv3metadata_t & cow_metadata);

    void dump_metadata_v3(const std::string & volumename,const std::string &filename,DiffPtr change_metadata);
    void dump_metadata_v3(const std::string & volumename,const cdpv3metadata_t & metadata);
    void dump_cowlocations(const std::string & volumename,const cdpv3metadata_t & metadata);

    // ==============================================
    // volume metadata routines
    // ==============================================

    bool get_volmetadata_for_timerange_v3(const SV_ULONGLONG &current_part_hrstart, 
                                          const SV_ULONGLONG &current_part_hrend,
                                          DiffPtr change_metadata,
                                          cdpv3metadata_t &vol_metadata);

    // =============================================
    //  cow update routines
    // =============================================

    bool update_cow_v3(const std::string &volumename,
                       const std::string & diffpath, 
                       char * diff_data,
                       const cdpv3metadata_t & cow_metadata);

    // =================================================
    //
    // Asynchronous COW update routines:
    // 
    //  updatecow_asynch_inmem_difffile_v3
    //    read_async_cowdrtds_inmem_difffile_v3
    //       read_async_cowdata_inmem_difffile_v3
    //    write_async_cowdata
    //       get_cows_belonging_to_same_file
    //          get_cdp_data_filepath
    //       get_async_writehandle
    //
    // ==================================================
	
    bool updatecow_asynch_inmem_difffile_v3(const std::string &volumename, 
                                            char * diff_data,
                                            const cdp_drtdv2s_t & drtds);

    bool read_async_cowdrtds_inmem_difffile_v3(const std::string & volumename,
                                               const char * diff_data,
                                               char * cow_data,
                                               const cdp_drtdv2s_constiter_t & drtds_to_read_begin,
                                               const cdp_drtdv2s_constiter_t & drtds_to_read_end,
                                               SV_UINT & drtd_counter, int size);

    bool read_async_cowdata_inmem_difffile_v3(const std::string & volumename,
                                              const char * diff_data,
                                              char * cow_data,
											  const cdp_drtdv2_fileid_t & drtd, int cow_data_size);

    bool write_async_cowdata(const char * cdp_data,
                             const cdp_drtdv2s_constiter_t & drtds_to_write_begin,
                             const cdp_drtdv2s_constiter_t & drtds_to_write_end);

    bool get_cows_belonging_to_same_file(
        const cdp_drtdv2s_fileid_constiter_t & cow_drtds_begin,
        const cdp_drtdv2s_fileid_constiter_t &cow_drtds_end,
        cdp_drtdv2s_fileid_constiter_t &cow_drtds_samefile_iter,
        cdp_drtdv2s_fileid_constiter_t &cow_drtds_samefile_end,
        std::string & cdp_data_filepath, 
        SV_OFFSET_TYPE & cow_offset,
        SV_UINT &cow_length);

    bool get_cdp_data_filepath(const SV_UINT & idx, std::string & filepath);

    bool get_async_writehandle(const std::string & filename, ACE_HANDLE & handle, 
                               ACE_Asynch_Write_File_Ptr & ptr);

    // =========================================================
    //
    // Asynchronous COW update routines:
    //
    //  updatecow_asynch_ondisk_difffile_v3
    //    read_async_cowdrtds_ondisk_difffile_v3
    //      read_async_cowdata_ondisk_difffile_v3
    //    write_async_cowdata
    //       get_cows_belonging_to_same_file
    //          get_cdp_data_filepath
    //       get_async_writehandle
    //
    // =========================================================

    bool updatecow_asynch_ondisk_difffile_v3(const std::string &volumename, 
                                             const std::string & diffpath,
                                             const cdp_drtdv2s_t & drtds);

    bool read_async_cowdrtds_ondisk_difffile_v3(const std::string &volumename, 
                                                BasicIo & diffBio,
                                                char * cow_data,
                                                const cdp_drtdv2s_constiter_t & drtds_to_read_begin,
                                                const cdp_drtdv2s_constiter_t & drtds_to_read_end,
                                                SV_UINT & drtd_counter);

    bool read_cowoverlapdata_ondisk_difffile_v3(BasicIo & diffBio,
                                                char * cow_data,
                                                const cdp_drtdv2_fileid_t & drtd);

    bool read_async_cownonoverlapdata_v3(const std::string &volumename, 
                                         char * cow_data,
                                         const cdp_drtdv2_fileid_t & drtd);



    // =================================================
    //
    // Synchronous COW update routines:
    // 
    //  updatecow_synch_inmem_difffile_v3
    //    read_sync_cowdrtds_inmem_difffile_v3
    //       read_sync_cowdata_inmem_difffile_v3
    //    write_sync_cowdata
    //       get_cows_belonging_to_same_file
    //          get_cdp_data_filepath
    //       get_sync_writehandle
    //
    // ==================================================

    bool updatecow_synch_inmem_difffile_v3(const std::string &volumename, 
                                           char * diff_data,
                                           const cdp_drtdv2s_t & drtds);

    bool read_sync_cowdrtds_inmem_difffile_v3(const std::string &volumename, 
                                              const char * diff_data,
                                              char * cow_data,
                                              const cdp_drtdv2s_constiter_t & drtds_to_read_begin,
                                              const cdp_drtdv2s_constiter_t & drtds_to_read_end,
                                              SV_UINT & drtd_counter, int size);

    bool read_sync_cowdata_inmem_difffile_v3(const std::string &volumename, 
                                             const char * diff_data,
                                             char * cow_data,
											 const cdp_drtdv2_fileid_t & drtd, int cow_data_size);

    bool read_sync_drtd_from_volume(ACE_HANDLE & vol_handle,
                                    char * buffer,
                                    const SV_OFFSET_TYPE & offset,
                                    const SV_UINT & length);

    bool write_sync_cowdata(const char * cdp_data,
                            const cdp_drtdv2s_constiter_t & drtds_to_write_begin,
                            const cdp_drtdv2s_constiter_t & drtds_to_write_end);

    bool get_sync_writehandle(const std::string & filename, ACE_HANDLE & handle);

    // =========================================================
    //
    // Synchronous COW update routines:
    //
    //  updatecow_synch_ondisk_difffile_v3
    //    read_sync_cowdrtds_ondisk_difffile_v3
    //      read_sync_cowdata_ondisk_difffile_v3
    //    write_sync_cowdata
    //       get_cows_belonging_to_same_file
    //          get_cdp_data_filepath
    //       get_sync_writehandle
    //
    // =========================================================

    bool updatecow_synch_ondisk_difffile_v3(const std::string &volumename, 
                                            const std::string & diffpath,
                                            const cdp_drtdv2s_t & drtds);

    bool read_sync_cowdrtds_ondisk_difffile_v3(const std::string &volumename,
                                               BasicIo & diffBio,
                                               char * cow_data,
                                               const cdp_drtdv2s_constiter_t & drtds_to_read_begin,
                                               const cdp_drtdv2s_constiter_t & drtds_to_read_end,
                                               SV_UINT & drtd_counter);

    bool read_sync_cowdata_ondisk_difffile_v3(const std::string &volumename,
                                              BasicIo & diffBio,
                                              char * cow_data,
                                              const cdp_drtdv2_t & drtd,
                                              const SV_UINT &drtd_no);


    // =====================================================
    // 
    //   Vsnap map updates:
    //    updatevsnaps  
    //
    // =====================================================
    bool updatevsnaps_v3(const std::string & volumename, 
                         const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                         const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end);

    // =============================================
    // bitmap routines
    // =============================================

    bool set_ts_inbitmap_v3(const cdpv3metadata_t & cow_metadata,const cdpv3transaction_t & txn);


    // ===========================================
    // db update routines
    // ===========================================

    bool update_dbrecord_v3(const cdpv3metadata_t & cow_metadata, const cdpv3transaction_t & txn );

    // =======================================
    // changes for Bugid 17548
    //========================================
    bool reserve_retention_space();
    bool IsInsufficientRetentionSpaceOccured();

    // =============================================================
    //  unused routines:
    // 
    //    isTsoFile
    //    ParseTsoFile
    //    get_previous_diff_information
    //
    // =============================================================

    bool isTsoFile(const std::string & filePath);
    bool ParseTsoFile(const std::string & filePath, DiffPtr & diffptr);

    bool get_previous_diff_information(std::string& filename,
                                       SV_UINT& version);


    // obsolete as part of sparse retention optimizations
    bool write_async_drtds_to_volume(ACE_Asynch_Write_File_Ptr handle, 
                                     const std::string & volumename,
                                     char * diff_data,
                                     const cdp_drtdv2s_constiter_t & nonoverlapping_batch_start, 
                                     const cdp_drtdv2s_constiter_t & nonoverlapping_batch_end);

    bool EndQuasiState();

    // =============================================
    // copy, assignment not allowed
    // =============================================
    CDPV2Writer(CDPV2Writer const & );
    CDPV2Writer& operator=(CDPV2Writer const & );


    // =============================================
    // data members
    // =============================================

    std::string m_volumename;
    SV_ULONGLONG m_srccapacity;
    CDP_SETTINGS m_settings;
    bool m_isInDiffSync;
    bool m_resyncRequired;
    bool m_init;
    bool m_dbrecord_fetched;
	
    // to be initialized in init routine 
    bool m_cdpenabled;
    std::string m_dbname;
    std::string m_cdpdir;

    //
    // 0 = if cdp is not enabled
    // 1 = cdpv1.db
    // 2 = cdpv3.db
    SV_UINT m_cdpversion;

    // configuration variables to be initialized in init
    OS_VAL m_baseline;
    SV_ULONGLONG m_minDiskFreeSpaceForUncompression;
    SV_UINT m_resync_chunk_size;
    int m_noOfUncompressRetries;
    int m_uncompressRetryInterval;
    SV_UINT m_maxMemoryForDrtds;
    bool m_ignoreCorruptedDiffs;
    bool m_allowOutOfOrderTS;
    bool m_allowOutOfOrderSeq;
    bool m_preallocate;
    bool m_useAsyncIOs;
    bool m_UseBlockingIoForCurrentInvocation;
    bool m_useUnBufferedIo;
    bool m_cdpCompressed;
    bool m_checkDI;
    bool m_verifyDI;
    unsigned long m_minpendingios;
    unsigned long m_maxpendingios;
    std::string m_appliedInfoDir;
    SV_ULONGLONG m_maxcdpv3FileSize;

    // variables to be set per input file
    bool m_isFileInMemory;
    bool m_isCompressedFile;
    SV_USHORT m_recoveryaccuracy;
    SV_UINT m_ctid;
    boost::shared_ptr<DiffSortCriterion> m_sortCriterion;
    boost::shared_ptr<DiffSortCriterion> m_processedSortCriterion;
    bool m_processedDiffInfoAvailable;
    bool m_setResyncRequiredOnOOD;
    bool m_ignoreOOD;
    CDPLastProcessedFile m_lastFileApplied;
    CDPLastRetentionUpdate m_lastCDPtxn;
    cdpv3transaction_t m_txn;
    FileChecksums m_checksums;

    // variables used for io purpose
    ACE_Thread_Mutex iocount_mutex;
    ACE_Condition<ACE_Thread_Mutex> iocount_cv;
    volatile unsigned long m_pendingios;
    bool m_error;
    bool m_memtobefreed_on_io_completion;
    volatile SV_UINT m_memconsumed_for_io;
    cdp_memoryallocs_t m_buffers_allocated_for_io;

    // ===============================================
    // sparse retention
    // ===============================================

    // bitmap required for figuring out the cdp datafilename
    cdp_bitmap_t m_cdpbitmap;

    std::vector<std::string> m_cdp_data_filepaths;
    cdp_cow_locations_t m_cowlocations;

    cdp_asyncwritehandles_t m_asyncwrite_handles;
    cdp_syncwritehandles_t m_syncwrite_handles;

    cdp_asyncreadhandles_t m_asyncread_handles;
    cdp_syncreadhandles_t m_syncread_handles;

    //Performance Improvement - Avoid open/close of retention files.
    cdp_asyncwritehandles_t m_cdpdata_asyncwrite_handles;
    cdp_syncwritehandles_t m_cdpdata_syncwrite_handles;

    // ==============================================
    // performance counters
    // ==============================================

    bool m_printPerf;
    SV_ULONGLONG m_diffsize;
    double m_total_time;
    double m_diffread_time;
    double m_diffuncompress_time;
    double m_diffparse_time;
    double m_metadata_time;
    double m_cownonoverlap_computation_time;
    double m_volread_time;
    double m_cowwrite_time;
    double m_vsnapupdate_time;
    double m_dboperation_time;
    double m_volnonoverlap_computation_time;
    double m_volwrite_time;
    double m_bitmapread_time;
    double m_bitmapwrite_time;

    // =============================================
    // Avoid multiple call the configurator
    // =============================================

    SV_UINT m_configMemoryForDrtds_Diffsync;
    SV_UINT m_configMemoryForDrtds_Resync;
    SV_UINT m_compressionChunkSize;
    SV_UINT m_compressionBufferSize;
    std::string m_cacheDirectoryPath;
    std::string m_targetCheckSumDir;
    std::string m_installPath;

    // ============================================
    // bitmap variables
    // ============================================

    bool m_bmasyncio;
    bool m_bmcaching;
    unsigned int m_bmcachesize;
    bool m_bmunbuffered_io;
    unsigned int m_bmblocksize;
    unsigned int m_bmblocksperentry;
    bool m_bmcompression;
    unsigned int m_bmmaxios;
    unsigned int m_bmmaxmem_forio;

    // ============================================
    // changes for 
    //  1) Reading/writing directly to virtual volume
    //     through sparse volume instead of going
    //     through virtual volume driver
    //  2) Instead of single sparse file, we are 
    //     using multiple sparse files to avoid
    //     windows ntfs size limitation on sparse
    //     file size when volume is fragmented
    // =============================================

    SV_UINT m_max_rw_size;
    bool m_vsnapdriver_available;
    bool m_volpackdriver_available;
    bool m_isvirtualvolume;
    bool m_ismultisparsefile;
    bool m_sparse_enabled;
    bool m_punch_hole_supported;
    std::string m_multisparsefilename;
    SV_OFFSET_TYPE m_maxsparsefilepartsize;

    bool get_async_readhandle(const std::string & filename,
                              ACE_HANDLE & handle, 
                              ACE_Asynch_Read_File_Ptr & ptr);

    bool get_sync_readhandle(const std::string & filename, 
                             ACE_HANDLE & handle);

    bool close_asyncwrite_handles();
    bool close_asyncread_handles();
    bool close_syncwrite_handles();
    bool close_syncread_handles();

    //=====================================================================
    //Performance Improvement - Avoid open/close of retention files.
    bool get_cdpdata_async_writehandle(const std::string & filename, ACE_HANDLE & handle, 
                                       ACE_Asynch_Write_File_Ptr & ptr);
    bool get_cdpdata_sync_writehandle(const std::string & filename, 
                                      ACE_HANDLE & handle);

    bool close_cdpdata_asyncwrite_handles();
    bool close_cdpdata_syncwrite_handles();
    bool flush_cdpdata_asyncwrite_handles();
    bool flush_cdpdata_syncwrite_handles();
    bool m_cache_cdpfile_handles;
    SV_UINT m_max_filehandles_tocache;
    SV_ULONGLONG m_cdpdata_handles_hr; //For storing the last applied hr time stamp.
    //=====================================================================

    bool all_zeroes(const char * const buffer, const SV_UINT & length);                         
    bool get_writehandle(const std::string & filename,ACE_HANDLE & handle);
    bool punch_hole_sparsefile(const std::string & filename,
                               const SV_OFFSET_TYPE & offset,
                               const SV_UINT & length);

    bool split_drtd_by_sparsefilename(const SV_OFFSET_TYPE & offset,
                                      const SV_UINT & length,
                                      cdp_drtds_sparsefile_t & splitdrtds);

    bool write_async_inmemdrtd_multisparse_volume(const char *const diff_data , 
                                                  const cdp_drtdv2_t & drtd, 
                                                  char * buffer_for_reading_data, int size );

    bool write_async_ondiskdrtd_multisparse_volume(BasicIo & diff_bio,
                                                   const cdp_drtdv2_t & drtd, 
                                                   char * buffer_for_reading_data,int size);

    bool write_sync_drtd_to_multisparse_volume(const cdp_drtdv2_t &drtd,
                                               char * pbuffer_containing_data,int size);

    bool read_async_drtd_from_multisparsefile(char * buffer_to_read_data,
                                              const SV_OFFSET_TYPE & offset,
                                              const SV_UINT & length);

    bool read_sync_drtd_from_multisparsefile(char * buffer,
                                             const SV_OFFSET_TYPE & offset,
                                             const SV_UINT & length);

    // TODO:
    //
    // Note: the below routines can be removed by proper code refactoring
    //   write_sync_inmemdrtds_to_multisparse_volume
    // 

    bool write_sync_inmemdrtds_to_multisparse_volume(char * pbuffer_containing_data,
                                                     const cdp_drtdv2s_constiter_t & drtds_to_write_begin, 
                                                     const cdp_drtdv2s_constiter_t & drtds_to_write_end,int size);

    // =======================================
    // changes for Bugid 17548
    //========================================

    SV_ULONGLONG m_reserved_cdpspace;
    SV_ULONGLONG m_cdpfreespace_trigger;

    SV_ULONGLONG m_sector_size;
    std::string m_volguid;
    ACE_HANDLE  m_volhandle;
    ACE_Asynch_Write_File_Ptr m_wfptr;
    ACE_Asynch_Read_File_Ptr m_rfptr;

    ACE_HANDLE m_vsnapctrldev;

    std::string m_rollback_filepath;
    std::string m_unhidero_filepath;
    std::string m_unhiderw_filepath;
    std::string m_cdpprune_filepath;

    Configurator* m_Configurator;
    TRANSPORT_PROTOCOL m_transport_protocol;
    const bool m_hideBeforeApplyingSyncData;
    bool m_secure_mode;
    bool m_inquasi_state;
    bool m_use_cfs;
    boost::shared_ptr<SnapShotRequestProcessor> m_snapshotrequestprocessor_ptr;

    /*SRM:Start*/
    SV_INT m_pair_state;
    SV_UINT m_cdp_max_redolog_size;
    bool m_cdp_flush_and_hold_writes_retry;
    bool m_cdp_flush_and_hold_resume_retry;
    FLUSH_AND_HOLD_REQUEST m_flush_and_hold_request;
    SV_ULONGLONG m_timeto_pause_apply_ts; //Time stamp in 100 nano seconds
    boost::shared_ptr<FlushAndHoldTxnFile> m_flush_and_hold_txn;
	
    bool isInFlushAndHoldPendingState();
    bool isInFlushAndHoldState();
    bool isInResumePendingState();
    bool getFlushAndHoldRequestSettingsFromCx();
    bool isCrossedRequestedPoint(SV_ULONGLONG diff_startts,bool & app_consitent_point,CDPEvent &cdpevent);

    bool rollback_volume_to_requested_point(CDPDRTDsMatchingCndn &cndn,std::string &error_msg, int &error_num);
    boost::tribool create_redo_logs(CDPDatabase &db, CDPDRTDsMatchingCndn & cndn, SV_UINT &filecount,std::string &errmsg,int &error_num);
    boost::tribool rollback(CDPDatabase &db, CDPDRTDsMatchingCndn & cndn,std::string &errmsg,int &error_num);
    bool replay_redo_logs(std::string &errmsg,int &error_num);
    bool write_redodrtds_to_volume(CdpRedoLogFile & redologfile,cdp_volume_t &tgtvol);
    bool clean_redo_logs(SV_UINT filecount);
    bool is_valid_point(CDPDRTDsMatchingCndn & cndn, std::string &error_msg);

    /*SRM:End*/

    // ===============================================
    // routines to support caching of volume handles
    // ===============================================
    bool m_cache_volumehandle;
    bool open_volume_handle();
    bool close_volume_handle();
	
    cdp_resync_txn * m_resync_txn_mgr;

    // =================================================
    // cfs support
    // =================================================
    cfs::CfsPsData m_cfsPsData;
    
    // =================================================
    // routines to profile io size distribution
    // *** Note ***
    // profiling is available only when sparse
    // retention is enabled
    // =================================================
    void profile_source_io(DiffPtr change_metadata);

    void profile_volume_read();
    void profile_volume_write(const cdpv3metadata_t & vol_metadata);
    void profile_cdp_write(const cdpv3metadata_t & cow_metadata);

    bool write_perf_stats(const std::string &filepath);
    bool write_src_profile_stats(const std::string &filepath);
    bool write_volread_profile_stats(const std::string &filepath);
    bool write_volwrite_profile_stats(const std::string &filepath);
    bool write_cdpwrite_profile_stats(const std::string &filepath);

    bool m_profile_source_iopattern;
    bool m_profile_volread;
    bool m_profile_volwrite;
    bool m_profile_cdpwrite;


    SV_ULONG src_ioLessThan512;
    SV_ULONG src_ioLessThan1K ;
    SV_ULONG src_ioLessThan2K ;
    SV_ULONG src_ioLessThan4K ;
    SV_ULONG src_ioLessThan8K ;
    SV_ULONG src_ioLessThan16K ;
    SV_ULONG src_ioLessThan32K ;
    SV_ULONG src_ioLessThan64K ;
    SV_ULONG src_ioLessThan128K ;
    SV_ULONG src_ioLessThan256K ;
    SV_ULONG src_ioLessThan512K ;
    SV_ULONG src_ioLessThan1M ;
    SV_ULONG src_ioLessThan2M ;
    SV_ULONG src_ioLessThan4M ;
    SV_ULONG src_ioLessThan8M ;
    SV_ULONG src_ioGreaterThan8M ;
    SV_ULONGLONG src_ioTotal ;

    SV_ULONG vr_ioLessThan512;
    SV_ULONG vr_ioLessThan1K ;
    SV_ULONG vr_ioLessThan2K ;
    SV_ULONG vr_ioLessThan4K ;
    SV_ULONG vr_ioLessThan8K ;
    SV_ULONG vr_ioLessThan16K ;
    SV_ULONG vr_ioLessThan32K ;
    SV_ULONG vr_ioLessThan64K ;
    SV_ULONG vr_ioLessThan128K ;
    SV_ULONG vr_ioLessThan256K ;
    SV_ULONG vr_ioLessThan512K ;
    SV_ULONG vr_ioLessThan1M ;
    SV_ULONG vr_ioLessThan2M ;
    SV_ULONG vr_ioLessThan4M ;
    SV_ULONG vr_ioLessThan8M ;
    SV_ULONG vr_ioGreaterThan8M ;
    SV_ULONGLONG vr_ioTotal ;

    SV_ULONG vw_ioLessThan512;
    SV_ULONG vw_ioLessThan1K ;
    SV_ULONG vw_ioLessThan2K ;
    SV_ULONG vw_ioLessThan4K ;
    SV_ULONG vw_ioLessThan8K ;
    SV_ULONG vw_ioLessThan16K ;
    SV_ULONG vw_ioLessThan32K ;
    SV_ULONG vw_ioLessThan64K ;
    SV_ULONG vw_ioLessThan128K ;
    SV_ULONG vw_ioLessThan256K ;
    SV_ULONG vw_ioLessThan512K ;
    SV_ULONG vw_ioLessThan1M ;
    SV_ULONG vw_ioLessThan2M ;
    SV_ULONG vw_ioLessThan4M ;
    SV_ULONG vw_ioLessThan8M ;
    SV_ULONG vw_ioGreaterThan8M ;
    SV_ULONGLONG vw_ioTotal ;

    SV_ULONG cdp_ioLessThan512;
    SV_ULONG cdp_ioLessThan1K ;
    SV_ULONG cdp_ioLessThan2K ;
    SV_ULONG cdp_ioLessThan4K ;
    SV_ULONG cdp_ioLessThan8K ;
    SV_ULONG cdp_ioLessThan16K ;
    SV_ULONG cdp_ioLessThan32K ;
    SV_ULONG cdp_ioLessThan64K ;
    SV_ULONG cdp_ioLessThan128K ;
    SV_ULONG cdp_ioLessThan256K ;
    SV_ULONG cdp_ioLessThan512K ;
    SV_ULONG cdp_ioLessThan1M ;
    SV_ULONG cdp_ioLessThan2M ;
    SV_ULONG cdp_ioLessThan4M ;
    SV_ULONG cdp_ioLessThan8M ;
    SV_ULONG cdp_ioGreaterThan8M ;
    SV_ULONGLONG cdp_ioTotal ;

	//==================================================================================
	// Azure Specific changes
	// When the target is azure, 
	//  For IR/resync - the changes are applied to base blob using MARS agent interface
	//  For resync step2/diffsync
	//   the files are accumulated in <azureCachFolder> until
	//    	The upload pending size reaches/exceeds configured limit OR
	//      session recoverability state is not same as file being processed OR
	//      There are no more files available in PS/cachemgr cache folder
	//   If any of the above conditions are met, it is ready for upload to Azure
	// 
	//===================================================================================

	VOLUME_SETTINGS::TARGET_DATA_PLANE m_targetDataPlane;

	// for the first invocation, you may have been invoked after a crash
	// you need to take special action to clean up files that have been uploaded to azure
	// if there are any pending files for upload, upload them to azure
	// this variable keeps track of the first invocation
	bool m_firstInvocation;
	
	// all files ready for upload to azure and have recoverable points are
	// placed in m_azureCacheFolderForRecoverablePoints
	extendedLengthPath_t m_azureCacheFolderForRecoverablePoints;

	// all files ready for upload to azure and have non-recoverable points are
	// placed in m_azureCacheFolderForNonRecoverablePoints
	extendedLengthPath_t m_azureCacheFolderForNonRecoverablePoints;

	// where are pending upload files
	extendedLengthPath_t m_azurePendingUploadFolder;

	// tracking log for file upload to azure
	extendedLengthPath_t m_azureTrackingUploadLog;

	// all files that have been uploaded to azure but pending deletion are placed in azureUploadedFolder
	extendedLengthPath_t m_azureUploadedFolder;

	// does the upload batch contain recoverable points
	bool m_isSessionHavingRecoverablePoints;

	// does the current file contain recoverable points
	bool m_isCurrentFileContainsRecoverablePoints;

	// Azure agent interface
	AzureAgentInterface::AzureAgent::ptr m_AzureAgent;

	// min upload size
	SV_ULONGLONG m_minUploadSize; // 128mb

	// min time gap in secs between two uploads
	SV_UINT m_minTimeGapBetweenUploads; // 120 secs

	// time gap in secs to sleep before checking for any new file arrival
	SV_UINT m_TimeGapBetweenFileArrivalCheck; // 15 secs


	// ordered differential sync files for uploading to Azure
	std::vector<std::string> m_orderedFileNames;

	// data pending for upload to azure
	SV_ULONGLONG m_uploadSize;

	// last upload time value
	ACE_Time_Value m_uploadTimestamp;

	// no more files pending in PS cache
	bool m_isLastFileinPSCache;

	bool m_bLogUploadedFilesToAzure;

    Profiler m_pSyncApply;

	bool isAzureTarget() const { return (m_targetDataPlane == VOLUME_SETTINGS::AZURE_DATA_PLANE); }
	extendedLengthPath_t PendingUploadFolderForRecoverablePoints() const { return m_azureCacheFolderForRecoverablePoints; }
	extendedLengthPath_t PendingUploadFolderForNonRecoverablePoints() const { return m_azureCacheFolderForNonRecoverablePoints; }
	extendedLengthPath_t PendingUploadFolder() const { return m_azurePendingUploadFolder; }
	extendedLengthPath_t UploadedFolder() const { return m_azureUploadedFolder; }

	SV_ULONGLONG azureUploadLimit() const { return m_minUploadSize; }
	bool UploadLimitReached() const 
	{
		if (azureUploadLimit())
			return (m_uploadSize >= azureUploadLimit());

		return false;
	}

	unsigned int MinTimeGapBetweenUploads() const { return m_minTimeGapBetweenUploads; } 
	unsigned int IdleTimeBeforeCheckingNewFileArrival() const { return m_TimeGapBetweenFileArrivalCheck; }
	ACE_Time_Value LastUploadTime() const { return m_uploadTimestamp; }
	void UpdateLastUploadTime() { m_uploadTimestamp = ACE_OS::gettimeofday(); }

	int TimeElapsedSinceLastUpload() const
	{
		ACE_Time_Value presentTime = ACE_OS::gettimeofday();
		return (presentTime.sec() - LastUploadTime().sec());
	}

	int RemainingTimeGapForNextUpload() const
	{ 
		int timeElapsedSinceLastUpload = TimeElapsedSinceLastUpload();
		
		// time on the system may have moved back
		// in thi scenario, consider upload can be done immediately
		if (timeElapsedSinceLastUpload < 0)
			return 0;

		if (timeElapsedSinceLastUpload < m_minTimeGapBetweenUploads)
			return (m_minTimeGapBetweenUploads - timeElapsedSinceLastUpload);

		return 0; 
	}

	bool isUploadTimeLimitElapsed() const 
	{  
		return (0 == RemainingTimeGapForNextUpload());
	}


	bool ReadyForUploadingLastChunk(const std::string &fileToApply) const
	{ 

		unsigned int remainingTimegapForNextUpload = RemainingTimeGapForNextUpload();
		unsigned int idleTimeBeforeCheckingNewFileArrival = IdleTimeBeforeCheckingNewFileArrival();
		std::string cacheLocation = dirname_r(fileToApply.c_str());

		// check every 15secs if a new file has arrived
		while (remainingTimegapForNextUpload > 0)
		{
			// if new files have arrived, return false
			//    so that listhread would fetch next set of files and 
			//    we send them all together
			if (HaveNewFilesArrivedInCacheFolder(cacheLocation))
				return false;

			// if we recieve a quit request, return false for the process to exit gracefully
			if (CDPUtil::QuitRequested(idleTimeBeforeCheckingNewFileArrival))
				return false;

			if (remainingTimegapForNextUpload > idleTimeBeforeCheckingNewFileArrival){
				remainingTimegapForNextUpload -= idleTimeBeforeCheckingNewFileArrival;
			}
			else {
				remainingTimegapForNextUpload = 0;
			}
		}

		// if new files have not arrived even after waiting for 2 mins (min time between 2 uploads)
		//    return true so that already accumulated files are immediately uploaded.
		return true;
	}



	bool isChangeInRecoverabilityState() const { return (m_isSessionHavingRecoverablePoints != m_isCurrentFileContainsRecoverablePoints); }
	void SetResetLastFileToProcess(bool isLastFileinPSCache) { m_isLastFileinPSCache = isLastFileinPSCache; }
	bool lastFileToProcess() const { return m_isLastFileinPSCache; }


	bool isReadyForUpload(const std::string &fileToApply) const
	{
		if (isChangeInRecoverabilityState()
			|| UploadLimitReached()
			|| isUploadTimeLimitElapsed())
			return true;


		if (lastFileToProcess())
		{
			return ReadyForUploadingLastChunk(fileToApply);
		}

		return false;
	}

	bool isFirstInvocation() const { return m_firstInvocation; }
	bool isNewSession() const { return m_orderedFileNames.empty(); }
	bool isPendingFilesForUpload() { return !m_orderedFileNames.empty(); }

	bool AzureApplyPendingFilesFromPreviousSession(const std::string & firstFileInNewSession);
	bool AzureApply(const std::string & fileToApply, bool lastFileToProcess);
	void initAzureSpecifParams(const std::string & fileToApply);
	bool isDiffFromCurrentResyncSession(const DiffSortCriterion & fileInfo);
	void GetPendingDataFromPreviousSession();
	void DeleteStaleOrEmptyAzureUploadFolders();
	void addFilesToCurrentSession(DiffSortCriterion::OrderedEndTime_t & pendingdataFromPreviousSession, bool containsrecoverablePoints);
	void UploadPendingFilesToAzure();
	void TrackFileUploadtoAzure(const std::vector<AzureAgentInterface::AzureDiffLogInfo> & orderedFileInfos, bool isSessionHavingRecoverablePoints);
	void UpdateLastAppliedFileInformation();
	void RenamePendingUploadFolderToUploaded();
	void CleanupCurrrentSession();
	void DeleteAzureFolder(extendedLengthPath_t folderPath);


	// force is passed as true, when you want to change in-progress session state 
	// eg. protection service returns resync required
	void SetSessionRecoverabilityState(bool containsRecoverablePoints, bool force = false);
	bool isSessionContainingRecoverabilePoints() { return m_isSessionHavingRecoverablePoints; }


	// routine used to change state from recoverable to non-recoverable
	// when protection service returns resync required
	void ChangePendingUploadStateToNonRecoverable();

	void FindCurrentFileRecoverabilityState(const std::string & fileToProcess);
	bool isCurrentFileContainingRecoverablePoints() { return m_isCurrentFileContainsRecoverablePoints; }

	std::string MoveCurrentfileToPendingUpload(const std::string & fileToProcess);
	void addFileToCurrentSession(const std::string & fileToProcess);
	
	bool isInResyncStep2();
	bool ReadyToTransitionToDiffSync(const std::string & fileToProcess);
	void TransitionToDiffSync();

	bool ApplyResyncFiletoAzureBlob(const std::string & fileToApply);

	bool HaveNewFilesArrivedInCacheFolder(const std::string& cacheLocation) const;

	//==============================================
	// Changes for disk based replication
	// =============================================

	const VolumeSummaries_t *m_pVolumeSummariesCache; ///< volumes cache to get to disk device name from disk name
	cdp_volume_t::CDP_VOLUME_TYPE m_deviceType;
	std::string m_displayName;
	VolumeSummary::NameBasedOn m_nameBasedOn;

	cdp_volume_t::CDP_VOLUME_TYPE DeviceType() const { return m_deviceType; }

	bool GetDeviceProperties();
	void GetDeviceType();
	bool GetdiskHandle();



};






#endif
