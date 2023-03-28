//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpv1tables.h
//
// Description: 
//

#ifndef CDPV1TABLES__H
#define CDPV1TABLES__H


#include <boost/shared_ptr.hpp>

#include "retentionsettings.h"
#include "cdpglobals.h"
#include "svtypes.h"
#include "cdpdrtd.h"
#include "cdpv1globals.h"

/// 
/// Version Table
///

#define CDPV1VERSIONTBLNAME            "t_version"

/// Columns for Version  table
///
///

#define CDPV1_NUM_VERSIONTBL_COLS   2
#define VERSIONTBL_C_VERSION_COL  0
#define VERSIONTBL_C_REVISION_COL 1

///
/// Columns for Version table
///

const char CDPV1_VERSION_COLS[CDPV1_NUM_VERSIONTBL_COLS][CDPV1_MAX_COL_LEN] = 
{ "c_version REAL PRIMARY KEY",
"c_revision REAL",
};

///
/// Version Information in memory
///
struct CDPVersion
{
    double c_version;
    double c_revision;
};

///
/// SuperBlock Table
///
#define CDPV1SUPERBLOCKTBLNAME       "t_superblock"


/// Columns for SuperBlock  table
///
///

#define CDPV1_NUM_SUPERBLOCK_COLS       7
#define SUPERBLOCK_C_TYPE_COL           0
#define SUPERBLOCK_C_LOOKUPPAGESIZE_COL 1
#define SUPERBLOCK_C_PHYSICALSPACE_COL  2
#define SUPERBLOCK_C_TSFC_COL           3
#define SUPERBLOCK_C_TSLC_COL           4
#define SUPERBLOCK_C_NUMINODES_COL      5
#define SUPERBLOCK_C_EVENTSUMMARY_COL   6

const char CDPV1_SUPERBLOCK_COLS[CDPV1_NUM_SUPERBLOCK_COLS][CDPV1_MAX_COL_LEN] = 
{ "c_type INTEGER",
"c_lookuppagesize INTEGER",
"c_physicalspace INTEGER",
"c_tsfc INTEGER",
"c_tslc INTEGER",
"c_numinodes INTEGER",
"c_eventsummary BLOB"
};


///
/// SuperBlock Information in memory
///

struct CDPV1SuperBlock
{
    CDP_LOGTYPE    c_type;
    SV_ULONGLONG   c_lookuppagesize; 
    SV_ULONGLONG   c_physicalspace;
    SV_ULONGLONG   c_tsfc;
    SV_ULONGLONG   c_tslc;
    SV_LONGLONG   c_numinodes;
    EventSummary_t c_eventsummary;
};

///
///
/// Data Directories table
#define CDPV1DATADIRTBLNAME          "t_datadir"

///
/// Columns for DataDir table
///

#define CDPV1_NUM_DATADIR_COLS  7
#define DATADIR_C_DIRNUM_COL    0
#define DATADIR_C_DIR_COL       1
#define DATADIR_C_FREESPACE_COL 2
#define DATADIR_C_USEDSPACE_COL 3
#define DATADIR_C_ALLOCATED_COL 4
#define DATADIR_C_TSFC_COL      5
#define DATADIR_C_TSLC_COL      6

const char CDPV1_DATADIR_COLS[CDPV1_NUM_DATADIR_COLS][CDPV1_MAX_COL_LEN] = 
{ "c_dirnum INTEGER PRIMARY KEY AUTOINCREMENT",
"c_dir BLOB UNIQUE ON CONFLICT IGNORE",
"c_freespace INTEGER DEFAULT 0", 
"c_usedspace INTEGER DEFAULT 0",
"c_allocated  INTEGER DEFAULT 0",
"c_tsfc INTEGER DEFAULT 0",
"c_tslc INTEGER DEFAULT 0"
};


///
/// Data Dir information in memory
///

struct CDPV1DataDir
{
    SV_ULONG     c_dirnum;
    std::string      c_dir;
    SV_ULONGLONG c_freespace;
    SV_ULONGLONG c_usedspace;
    SV_ULONGLONG c_allocated;
    SV_ULONGLONG c_tsfc;
    SV_ULONGLONG c_tslc;
};


///
/// Inode Table
///
#define CDPV1INODETBLNAME            "t_inode"

///
/// Columns for Inode table 
///


#define CDPV1_NUM_INODE_COLS      14
#define INODE_C_INODENO_COL       0
#define INODE_C_TYPE_COL          1
#define INODE_C_DIRNUM_COL        2
#define INODE_C_DATAFILE_COL      3
#define INODE_C_STARTSEQ_COL      4
#define INODE_C_STARTTIME_COL     5
#define INODE_C_ENDSEQ_COL        6
#define INODE_C_ENDTIME_COL       7
#define INODE_C_PHYSICALSIZE_COL  8
#define INODE_C_LOGICALSIZE_COL   9
#define INODE_C_NUMDIFFS_COL      10
#define INODE_C_DIFFINFOS_COL     11
#define INODE_C_NUMEVENTS_COL     12
#define INODE_C_VERSION_COL       13


const char CDPV1_INODE_COLS[CDPV1_NUM_INODE_COLS][CDPV1_MAX_COL_LEN] = 
{ "c_inodeno INTEGER PRIMARY KEY AUTOINCREMENT",
"c_type   INTEGER",
"c_dirnum INTEGER", 
"c_datafile BLOB",
"c_startseq INTEGER",
"c_starttime INTEGER",   
"c_endseq  INTEGER",
"c_endtime INTEGER", 
"c_physicalsize INTEGER",
"c_logicalsize INTEGER", 
"c_numdiffs INTEGER",
"c_diffinfos BLOB DEFAULT NULL",
"c_numevents INTEGER",
"c_version INTEGER DEFAULT 0"
};


///
/// Writes to the data file are performed as differential files
/// CDPV1DiffInfo contains information for a differential file 
/// 
///


struct CDPV1DiffInfo
{
    SV_OFFSET_TYPE   start;
    SV_OFFSET_TYPE   end;
    SV_ULONGLONG     tsfcseq;
    SV_ULONGLONG  tsfc;
    SV_ULONGLONG  tslcseq;
    SV_ULONGLONG  tslc;
    SV_UINT       ctid;
    SV_ULONG      numchanges;
    cdp_drtdv2s_t     changes;
};

typedef boost::shared_ptr< CDPV1DiffInfo > CDPV1DiffInfoPtr;
typedef std::vector< CDPV1DiffInfoPtr >         CDPV1DiffInfos_t;
typedef CDPV1DiffInfos_t::iterator         CDPV1DiffInfosIterator; 
typedef CDPV1DiffInfos_t::const_iterator   CDPV1DiffInfosConstIterator; 
typedef CDPV1DiffInfos_t::reverse_iterator CDPV1DiffInfosRevIter; 

///
/// In Memory Inode
///
struct CDPV1Inode
{
    SV_LONGLONG  c_inodeno;
    SV_UINT      c_type;
    SV_ULONG     c_dirnum;
    std::string  c_datadir;
    std::string  c_datafile;
    SV_ULONGLONG c_startseq;
    SV_ULONGLONG c_starttime;
    SV_ULONGLONG c_endseq;
    SV_ULONGLONG c_endtime;
    SV_ULONGLONG c_physicalsize;
    SV_ULONGLONG c_logicalsize;
    SV_UINT      c_numdiffs;
    CDPV1DiffInfos_t c_diffinfos;
    SV_UINT      c_numevents;
    SV_UINT      c_version;
};

///
/// Event Table
///
#define CDPV1EVENTTBLNAME            "t_event"

///
/// Columns for Event table
/// TODO: we are just keeping the names and not the type
///

#define CDPV1_NUM_EVENT_COLS         9 
#define EVENT_C_EVENTNO_COL          0
#define EVENT_C_INODENO_COL          1
#define EVENT_C_EVENTTYPE_COL        2
#define EVENT_C_EVENTTIME_COL        3
#define EVENT_C_EVENTVALUE_COL       4
#define EVENT_C_EVENTMODE_COL        5 
#define EVENT_C_IDENTIFIER_COL		 6
#define EVENT_C_VERIFIED_COL		 7
#define EVENT_C_COMMENT_COL		     8

const char CDPV1_EVENT_COLS[CDPV1_NUM_EVENT_COLS][CDPV1_MAX_COL_LEN] = 
{ "c_eventno INTEGER PRIMARY KEY AUTOINCREMENT",
"c_inodeno INTEGER NOT NULL", 
"c_eventtype INTEGER NOT NULL", 
"c_eventtime INTEGER NOT NULL",  
"c_eventvalue BLOB" ,
"c_eventmode INTEGER DEFAULT 1"  ,
"c_identifier BLOB DEFAULT NULL" ,
"c_verified INTEGER DEFAULT 0",
"c_comment BLOB DEFAULT NULL"
};



///
/// In memory Event
///

struct CDPV1Event
{
    SV_ULONGLONG c_eventno;
    SV_ULONGLONG c_inodeno;
    SV_USHORT    c_eventtype;
    SV_ULONGLONG c_eventtime;
    std::string  c_eventvalue;
    SV_USHORT    c_eventmode;   
	std::string  c_identifier;
	SV_USHORT	 c_verified;
	std::string  c_comment;
};

///
/// Events Pending update to cx
/// 
#define CDPV1PENDINGEVENTTBLNAME  "t_pendingevent"


///
/// Columns for Events Pending update to cx table
/// 


#define CDPV1_NUM_PENDINGEVENT_COLS         10 
#define PENDINGEVENT_C_EVENTNO_COL          0
#define PENDINGEVENT_C_INODENO_COL          1
#define PENDINGEVENT_C_EVENTTYPE_COL        2
#define PENDINGEVENT_C_EVENTTIME_COL        3
#define PENDINGEVENT_C_EVENTVALUE_COL       4
#define PENDINGEVENT_C_VALID_COL            5
#define PENDINGEVENT_C_EVENTMODE_COL        6 
#define PENDINGEVENT_C_IDENTIFIER_COL       7
#define PENDINGEVENT_C_VERIFIED_COL		    8
#define PENDINGEVENT_C_COMMENT_COL	  	    9

const char CDPV1_PENDINGEVENT_COLS[CDPV1_NUM_PENDINGEVENT_COLS][CDPV1_MAX_COL_LEN] = 
{ "c_eventno INTEGER PRIMARY KEY AUTOINCREMENT",
"c_inodeno INTEGER NOT NULL", 
"c_eventtype INTEGER NOT NULL", 
"c_eventtime INTEGER NOT NULL",  
"c_eventvalue BLOB",
"c_valid INTEGER DEFAULT 1",
"c_eventmode INTEGER DEFAULT 1"  , 
"c_identifier BLOB DEFAULT NULL",
"c_verified INTEGER DEFAULT 0",
"c_comment BLOB DEFAULT NULL"
};



///
/// In memory Pending Event
///

struct CDPV1PendingEvent
{
    SV_ULONGLONG c_eventno;
    SV_ULONGLONG c_inodeno;
    SV_USHORT    c_eventtype;
    SV_ULONGLONG c_eventtime;
    std::string c_eventvalue;
    bool        c_valid;
    SV_USHORT   c_eventmode;  
	std::string  c_identifier;
	SV_USHORT	 c_verified;
	std::string  c_comment;
};

///
/// TimeRange Table
///
#define CDPV1TIMERANGETBLNAME       "t_timerange"

///
/// Columns for TimeRange Table
///

#define CDPV1_NUM_TIMERANGE_COLS     6
#define TIMERANGE_C_ENTRYNO_COL      0
#define TIMERANGE_C_STARTTIME_COL    1
#define TIMERANGE_C_ENDTIME_COL      2
#define TIMERANGE_C_MODE_COL         3
#define TIMERANGE_C_STARTSEQ_COL     4
#define TIMERANGE_C_ENDSEQ_COL       5

const char CDPV1_TIMERANGE_COLS[CDPV1_NUM_TIMERANGE_COLS][CDPV1_MAX_COL_LEN] = 
{ "c_entryno INTEGER PRIMARY KEY AUTOINCREMENT",    
"c_starttime INTEGER NOT NULL",
"c_endtime INTEGER NOT NULL",
"c_mode INTEGER DEFAULT 0",
"c_startseq INTEGER DEFAULT 0",
"c_endseq INTEGER DEFAULT 0"
};

///
/// Row fetched from the TimeRange table to memory 
///
struct CDPV1TimeRange
{
    SV_ULONGLONG c_entryno;
    SV_ULONGLONG c_starttime;
    SV_ULONGLONG c_endtime;
    SV_USHORT c_mode;
    SV_ULONGLONG c_startseq;
    SV_ULONGLONG c_endseq;
};

///
/// PendingTimeRange Table
///
#define CDPV1PENDINGTIMERANGETBLNAME       "t_pendingtimerange"


///
/// Columns for PendingTimeRange Table
///

#define CDPV1_NUM_PENDINGTIMERANGE_COLS     6 
#define PENDINGTIMERANGE_C_ENTRYNO_COL      0
#define PENDINGTIMERANGE_C_STARTTIME_COL    1
#define PENDINGTIMERANGE_C_ENDTIME_COL      2
#define PENDINGTIMERANGE_C_MODE_COL         3
#define PENDINGTIMERANGE_C_STARTSEQ_COL     4
#define PENDINGTIMERANGE_C_ENDSEQ_COL       5


const char CDPV1_PENDINGTIMERANGE_COLS[CDPV1_NUM_PENDINGTIMERANGE_COLS][CDPV1_MAX_COL_LEN] = 
{   
    "c_entryno INTEGER PRIMARY KEY AUTOINCREMENT ",    
        "c_starttime INTEGER NOT NULL",
        "c_endtime INTEGER NOT NULL",
        "c_mode INTEGER DEFAULT 0",
        "c_startseq INTEGER DEFAULT 0",
        "c_endseq INTEGER DEFAULT 0"
};

///
/// Row fetched from the TimeRange table to memory 
///
struct CDPV1PendingTimeRange
{    
    SV_ULONGLONG c_entryno;
    SV_ULONGLONG c_starttime;
    SV_ULONGLONG c_endtime;
    SV_USHORT c_mode;
    SV_ULONGLONG c_startseq;
    SV_ULONGLONG c_endseq;
};


///
/// Diff file applied to log but pending update to volume
///
#define CDPV1APPLIEDTBLNAME       "t_applied"

///
/// Columns for Applied table
///

#define CDPV1_NUM_APPLIED_COLS  3
#define APPLIED_C_DIFFFILE_COL    0
#define APPLIED_C_STARTOFFSET_COL 1
#define APPLIED_C_ENDOFFSET_COL   2

const char CDPV1_APPLIED_COLS[CDPV1_NUM_APPLIED_COLS][CDPV1_MAX_COL_LEN] = 
{ "c_difffile BLOB" ,
"c_startoffset INTEGER NOT NULL",
"c_endoffset INTEGER NOT NULL"
};

///
/// In memory Applied Table Entry
///

struct CDPV1AppliedDiffInfo
{
    std::string  c_difffile;
    SV_OFFSET_TYPE c_startoffset;
    SV_OFFSET_TYPE c_endoffset;
};



#endif
