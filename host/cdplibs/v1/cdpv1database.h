//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdpv1database.h
//
// Description: 
//

#ifndef CDPV1DATABASE__H
#define CDPV1DATABASE__H

#include "cdpglobals.h"
#include "cdpv1globals.h"
#include "cdpv1tables.h"
#include "file.h"

#include <boost/shared_ptr.hpp>
#include <sqlite3x.hpp>


class CDPV1Database
{
public:

    CDPV1Database(const std::string & dbpath);
    virtual ~CDPV1Database();


    const std::string & dbname() const { return m_dbname; }
    bool Exists();

    bool Connect();
    bool Disconnect();
    bool connected() { if(!m_con) return true; else return false; }

    bool BeginTransaction();
    bool CommitTransaction();

    virtual SVERROR isempty(std::string table);
    SVERROR  VersionTblExists();
    bool read(CDPVersion &);
    bool write(CDPVersion &);
    bool InitializeVersionTable(const CDPVersion & );





    bool initialize();
    bool getrecoveryrange(SV_ULONGLONG & start, SV_ULONGLONG & end, 
        SV_ULONGLONG & datafiles);

    bool get_cdp_inode(const std::string & filePath,
        bool& partiallyApplied,
        SV_USHORT recovery_accuracy,
        DiffPtr & change_metadata,
        //bool & alreadapplied,
        bool preallocate,
        CDPV1Inode & inode,
        bool isInDiffSync);

    bool update_cdp(const std::string & filePath,
        SV_USHORT recovery_accuarcy,
        DiffPtr & change_metadata,
        DiffPtr & cdp_metadata,
        CDPV1Inode & inode,
        bool isInDiffSync);

    bool clear_applied_entry(const std::string & filePath);

    SV_ULONGLONG MetaDataSpace() const   { return File::GetSizeOnDisk(m_dbname); }
    void SetResyncMode(bool set=true)          { m_ResyncMode = set; }
    bool isResyncMode()                        { return (m_ResyncMode == true); }

    // routines for accuracy mode
    void SetAccuracyMode(SV_USHORT accuracymode)    {  m_AccuracyMode= accuracymode; }
    SV_USHORT AccuracyMode()                        { return (m_AccuracyMode); }

    bool OlderVersionExists();
    SVERROR  SuperBlockExists();
    SVERROR  InodeTableExists();
    SVERROR  EventTableExists();
    SVERROR  PendingEventTableExists();
    SVERROR  AppliedTableExists();
    SVERROR  DataDirTableExists();
    SVERROR  TimeRangeTableExists();
    SVERROR  PendingTimeRangeTableExists();

    bool CreateRequiredTables();
    bool GetRevision(double & rev);
    bool Upgrade(double oldrevision);
    bool UpgradeRev0();
    bool UpgradeRev1();
    bool UpgradeRev2();
    //function to fix the date issue(Bug #6218)
    bool redate(const std::string & tblname, std::vector<std::string> & columns);

    bool InitializeSuperBlockTable(const CDPV1SuperBlock &);
    bool InitializeDataDirs(std::list<std::string> &);

    bool Insert(const DiffPtr & diffptr, CDPV1Inode & inode);

    //Added second parameter preAllocate for making preallocation configurable. See Bug#6984
    SVERROR AllocateNewInodeIfRequired(DiffPtr &,
        bool preAllocate,
        const std::string& cdpDataFileVerPrefix,
        bool& partiallyApplied);

    class Reader
    {
    public:
        Reader(CDPV1Database & db, const std::string & query );
        ~Reader() { close(); }

        SVERROR read(CDPV1SuperBlock &);
        SVERROR read(CDPV1DataDir &);
        bool read(std::vector<CDPV1DataDir> &);
        SVERROR read(CDPV1Inode &, bool ReadDiff);
        SVERROR read(CDPV1Event &); 
        SVERROR read(CDPV1PendingEvent &); 
        SVERROR read(CDPV1TimeRange &);   
        SVERROR read(CDPV1PendingTimeRange &);   

        bool read(std::vector<CDPV1Event> &);
        bool read(std::vector<CDPV1PendingEvent> &);
        bool read(std::vector<CDPV1TimeRange> &);
        bool read(std::vector<CDPV1PendingTimeRange> &);
        SVERROR read(CDPV1AppliedDiffInfo &);

        void close(); 

    private:

        CDPV1Database & m_db;
        std::string m_query;
        bool m_rv;

        boost::shared_ptr<sqlite3x::sqlite3_command> m_cmd;
        sqlite3x::sqlite3_reader m_reader;

    };

    Reader ReadSuperBlock();
    Reader FetchDataDirs();
    Reader FetchDataDirForDirNum(SV_LONG dirnum);
    Reader FetchDataDirForDirName(const std::string & dirname);
    Reader FetchInodesinOldestFirstOrder();
    Reader FetchInodesinLatestFirstOrder();
    Reader FetchOldestInode();
    Reader FetchLatestInode();
    Reader FetchInode(const SV_ULONGLONG & inodeNo);
    Reader FetchLatestDiffSyncInode();
    Reader FetchEventsForInode(const CDPV1Inode &);
    Reader FetchEventsForInode(const  SV_ULONGLONG & inodeNo);
    Reader FetchInodeWithEndTimeGrEq(const SV_ULONGLONG & ts);
    Reader FetchEvents();
    Reader FetchPendingEvents();
    Reader FetchTimeRanges(); 
    Reader FetchPendingTimeRanges(); 
    Reader FetchAppliedTableEntry(const CDPV1AppliedDiffInfo &);
    Reader FetchQueryResult(const std::string & query);

    bool FetchInodeNoForEvent(const std::string & eventvalue, 
        const SV_ULONGLONG & eventtime, SV_ULONGLONG & inodeNo );

    bool FetchInodeNoForEvent(SV_USHORT eventType, const std::string & eventvalue, SV_ULONGLONG & inodeNo);

    bool Insert(const CDPV1DataDir &);
    bool Insert(const CDPV1Inode &);
    bool Insert(const CDPV1Event &);
    bool Insert(const CDPV1PendingEvent &);
    bool Insert(const CDPV1TimeRange &);
    bool Insert(const CDPV1PendingTimeRange &);
    bool Insert(const CDPV1AppliedDiffInfo &);

    bool AdjustTimerangeTable(CDPV1SuperBlock &cdpv1superblock );
    bool AdjustPendingTimerangeTable(CDPV1SuperBlock &superblock );

    bool Update(const CDPV1SuperBlock &);
    bool Update(const CDPV1Inode &);
    bool RevokeTags(const SV_ULONGLONG & inodeNo);
    bool Update(const CDPV1TimeRange &);
    bool Update(const CDPV1PendingTimeRange &);

    bool Add( CDPV1TimeRange &);
    bool Add(CDPV1PendingTimeRange &);
    bool DeleteAllRows(const std::string &); // unused

    bool Delete(const CDPV1Inode &); 
    bool Delete(const CDPV1PendingEvent &);
    bool Delete(const CDPV1Event &);
    bool Delete(const CDPV1TimeRange &); // unused
    bool Delete(const CDPV1PendingTimeRange &);

    bool Delete(const CDPV1AppliedDiffInfo &);

    bool DeleteAppliedEntries(const std::string &);


protected:

    std::string m_dbdir;
    std::string m_dbname;
    boost::shared_ptr<sqlite3x::sqlite3_connection> m_con;
    boost::shared_ptr<sqlite3x::sqlite3_transaction> m_trans;

    SVERROR  TableExists(std::string tablename);
    bool CreateVersionTable();

    CDPVersion m_ver;
    bool m_versionAvailableInCache;

private:

    bool initializeVersion();
    bool initializeSuperBlock();
    bool UpgradeRev0PendingEventTable();
    bool UpgradeRev0EventTable();
    bool UpgradeRev0TimeRangeTable(CDPV1SuperBlock & superblock);
    bool UpgradeRev0PendingTimeRangeTable(CDPV1SuperBlock & superblock);
    bool UpgradeRev1TimeRangeTable();
    bool UpgradeRev1PendingTimeRangeTable();
    bool UpgradeRev1EventTable();
    bool UpgradeRev1PendingEventTable();

    bool SetCacheSize(SV_UINT numpages);

    bool UpgradeRev0InodeTable();
    bool UpgradeRev1InodeTable();

    bool CreateSuperBlockTable();
    bool CreateDataDirTable();
    bool CreateInodeTable();
    bool CreateEventTable();
    bool CreatePendingEventTable();
    bool CreateTimeRangeTable();  
    bool CreatePendingTimeRangeTable();
    bool CreateAppliedTable();
    SVERROR NewInodeRequired(const DiffPtr & diffptr,
        const CDPV1Inode & cdpcurrinode,
        const std::string& cdpDataFileVerPrefix);
    //Added third parameter preAllocate for making preallocation configurable. See Bug#6984
    bool CreateNewInode(const DiffPtr & diffptr,
        const CDPV1Inode & cdpcurrinode,
        bool preAllocate,
        const std::string& cdpDataFileVerPrefix);
    std::string NewDataFileName(const DiffPtr & diffptr, const std::string& cdpDataFileVerPrefix);

    bool AddEvents(const CDPV1Inode &, const DiffPtr & diffptr);

    bool m_ResyncMode;
    //Accuracy mode 
    SV_USHORT m_AccuracyMode;

    CDPV1Database(CDPV1Database const & );
    CDPV1Database& operator=(CDPV1Database const & );
};
#endif


