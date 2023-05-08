//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpv3tables.h
//
// Description: 
//

#ifndef CDPV3TABLES__H
#define CDPV3TABLES__H

#include "cdpglobals.h"
#include "svtypes.h"
#include "cdpv3globals.h"

/// 
/// Version Table
///

#define CDPV3VERSIONTBLNAME            "t_version"

/// Columns for Version  table
///
///

#define CDPV3_NUM_VERSIONTBL_COLS	2
#define CDPV3_VERSIONTBL_C_VERSION_COL	0
#define CDPV3_VERSIONTBL_C_REVISION_COL	1

const char CDPV3_VERSIONTBL_COLS[CDPV3_NUM_VERSIONTBL_COLS][CDPV3_MAX_COL_LEN] = 
{ "c_version REAL",
"c_revision REAL"
};

///
/// Version Information in memory
///
struct CDPV3Version
{
    double c_version;
    double c_revision;
};



///
/// RecoveryRanges Table
///
#define CDPV3RECOVERYRANGESTBLNAME       "t_recoveryranges"


/// Columns for RecoveryRanges  table
///
///

#define CDPV3_NUM_RECOVERYRANGESTBL_COLS		        8
#define CDPV3_RECOVERYRANGESTBL_C_ENTRYNO_COL		    0
#define CDPV3_RECOVERYRANGESTBL_C_TSFC_COL			    1
#define CDPV3_RECOVERYRANGESTBL_C_TSLC_COL			    2
#define CDPV3_RECOVERYRANGESTBL_C_TSFCSEQ_COL		    3
#define CDPV3_RECOVERYRANGESTBL_C_TSLCSEQ_COL		    4
#define CDPV3_RECOVERYRANGESTBL_C_GRANULARITY_COL	    5
#define CDPV3_RECOVERYRANGESTBL_C_ACCURACY_COL		    6
#define CDPV3_RECOVERYRANGESTBL_C_CXUPDATESTATUS_COL	7

const char CDPV3_RECOVERYRANGESTBL_COLS[CDPV3_NUM_RECOVERYRANGESTBL_COLS][CDPV3_MAX_COL_LEN] = 
{ 
"c_entryno INTEGER PRIMARY KEY AUTOINCREMENT",
"c_tsfc INTEGER",
"c_tslc INTEGER",
"c_tsfcseq INTEGER",
"c_tslcseq INTEGER",
"c_granularity INTEGER",
"c_accuracy INTEGER",
"c_cxupdatestatus INTEGER"
};

///
/// RecoveryRanges Information in memory
///

struct CDPV3RecoveryRange
{
	SV_ULONGLONG    c_entryno;
    SV_ULONGLONG	c_tsfc;
    SV_ULONGLONG	c_tslc; 
    SV_ULONGLONG	c_tsfcseq;
    SV_ULONGLONG	c_tslcseq;
    SV_USHORT		c_granularity;
    SV_USHORT		c_accuracy;
    SV_USHORT		c_cxupdatestatus;

    CDPV3RecoveryRange()
        :c_entryno(0),c_tsfc(0),
        c_tslc(0),c_tsfcseq(0),
        c_tslcseq(0),c_granularity(0), c_accuracy(0),
        c_cxupdatestatus(0)
    {

    }

    CDPV3RecoveryRange(const CDPV3RecoveryRange& rhs)
        :c_entryno(rhs.c_entryno),c_tsfc(rhs.c_tsfc),
        c_tslc(rhs.c_tslc),c_tsfcseq(rhs.c_tsfcseq),
        c_tslcseq(rhs.c_tslcseq),c_granularity(rhs.c_granularity), 
        c_accuracy(rhs.c_accuracy), c_cxupdatestatus(rhs.c_cxupdatestatus)
    {

    }

    CDPV3RecoveryRange& operator=( CDPV3RecoveryRange const& rhs )
    {
        if(this == &rhs)
            return *this;

        c_entryno = rhs.c_entryno;
        c_tsfc = rhs.c_tsfc;
        c_tslc = rhs.c_tslc;
        c_tsfcseq = rhs.c_tsfcseq;
        c_tslcseq = rhs.c_tslcseq; 
        c_granularity = rhs.c_granularity;
        c_accuracy = rhs.c_accuracy; 
        c_cxupdatestatus = rhs.c_cxupdatestatus;
        return *this;
    }

	bool operator ==(const CDPV3RecoveryRange & rhs) const
	{
		return (c_entryno == rhs.c_entryno &&
			c_tsfc == rhs.c_tsfc &&
			c_tslc == rhs.c_tslc &&
			c_tsfcseq == rhs.c_tsfcseq &&
			c_tslcseq == rhs.c_tslcseq &&
			c_granularity == rhs.c_granularity &&
			c_accuracy ==  rhs.c_accuracy &&
			c_cxupdatestatus == rhs.c_cxupdatestatus );
	}

	bool operator != (const CDPV3RecoveryRange & rhs) const
	{
		return !(*this == rhs);
	}

	bool operator < (const CDPV3RecoveryRange & rhs) const
	{
        // assume we are only comparing non-overlapping ranges
        if (c_tsfc < rhs.c_tsfc)
            return true;
        else if(c_tsfc > rhs.c_tsfc)
            return false;

        if(c_tsfcseq < rhs.c_tsfcseq)
            return true;
        else if(c_tsfcseq > rhs.c_tsfcseq)
            return false;

        return false;
    }

};


///
///
/// BookMarks table
#define CDPV3BOOKMARKSTBLNAME          "t_bookmarks"

///
/// Columns for BookMarks table
///

#define CDPV3_NUM_BOOKMARKSTBL_COLS		            13
#define CDPV3_BOOKMARKSTBL_C_ENTRYNO_COL			0
#define CDPV3_BOOKMARKSTBL_C_APPTYPE_COL			1
#define CDPV3_BOOKMARKSTBL_C_VALUE_COL		    	2
#define CDPV3_BOOKMARKSTBL_C_ACCURACY_COL		    3
#define CDPV3_BOOKMARKSTBL_C_TIMESTAMP_COL		    4
#define CDPV3_BOOKMARKSTBL_C_SEQUENCE_COL	   	    5
#define CDPV3_BOOKMARKSTBL_C_IDENTIFIER_COL         6
#define CDPV3_BOOKMARKSTBL_C_VERIFIED_COL           7
#define CDPV3_BOOKMARKSTBL_C_COMMENT_COL            8
#define CDPV3_BOOKMARKSTBL_C_CXUPDATESTATUS_COL	    9
#define CDPV3_BOOKMARKSTBL_C_COUNTER_COL	       10
#define CDPV3_BOOKMARKSTBL_C_LOCKED_COL    11
#define CDPV3_BOOKMARKSTBL_C_LIFETIME_COL  12

const char CDPV3_BOOKMARKSTBL_COLS[CDPV3_NUM_BOOKMARKSTBL_COLS][CDPV3_MAX_COL_LEN] = 
{
"c_entryno INTEGER PRIMARY KEY AUTOINCREMENT",
"c_apptype INTEGER",
"c_value BLOB",
"c_accuracy INTEGER DEFAULT 1", 
"c_timestamp INTEGER NOT NULL",
"c_sequence  INTEGER NOT NULL",
"c_identifier BLOB DEFAULT NULL",
"c_verified INTEGER DEFAULT 0",
"c_comment BLOB DEFAULT NULL",
"c_cxupdatestatus INTEGER DEFAULT 0",
"c_counter INTEGER DEFAULT 0",
"c_lockstatus INTEGER DEFAULT 0",
"c_lifetime INTEGER DEFAULT 0"
};

///
/// BookMarks information in memory
///

struct CDPV3BookMark
{
	SV_ULONGLONG    c_entryno;
    SV_USHORT		c_apptype;
    std::string		c_value;
    SV_USHORT		c_accuracy;
    SV_ULONGLONG	c_timestamp;
    SV_ULONGLONG	c_sequence;
	std::string     c_identifier;
	SV_USHORT       c_verified;
	std::string     c_comment;
    SV_USHORT		c_cxupdatestatus;
	SV_USHORT       c_counter;
	SV_USHORT       c_lockstatus;
    SV_ULONGLONG    c_lifetime;

    CDPV3BookMark()
        :c_entryno(0),c_apptype(0),
        c_accuracy(0),c_timestamp(0),
        c_sequence(0),c_verified(0), c_cxupdatestatus(0),
        c_counter(0),c_lockstatus(0),c_lifetime(0)
    {

    }

    CDPV3BookMark(const CDPV3BookMark& rhs)
        :c_entryno(rhs.c_entryno),c_apptype(rhs.c_apptype),
        c_accuracy(rhs.c_accuracy),c_timestamp(rhs.c_timestamp),
        c_sequence(rhs.c_sequence),c_verified(rhs.c_verified), 
        c_cxupdatestatus(rhs.c_cxupdatestatus),
        c_counter(rhs.c_counter),c_lockstatus(rhs.c_lockstatus),
        c_value(rhs.c_value),c_identifier(rhs.c_identifier),
        c_comment(rhs.c_comment),c_lifetime(rhs.c_lifetime)
    {

    }

    CDPV3BookMark& operator=( CDPV3BookMark const& rhs )
    {
        if(this == &rhs)
            return *this;

        c_entryno = rhs.c_entryno;
        c_apptype = rhs.c_apptype;
        c_accuracy = rhs.c_accuracy;
        c_timestamp = rhs.c_timestamp;
        c_sequence = rhs.c_sequence;
        c_verified = rhs.c_verified;
        c_cxupdatestatus = rhs.c_cxupdatestatus;
        c_counter = rhs.c_counter;
        c_lockstatus = rhs.c_lockstatus;
        c_value = rhs.c_value; 
        c_identifier = rhs.c_identifier;
        c_comment =rhs.c_comment;
        c_lifetime = rhs.c_lifetime;
        return *this;
    }

    bool operator ==(const CDPV3BookMark & rhs) const
    {
        return ( c_entryno == rhs.c_entryno &&
            c_apptype == rhs.c_apptype &&
            c_accuracy == rhs.c_accuracy &&
            c_timestamp == rhs.c_timestamp &&
            c_sequence == rhs.c_sequence &&
            c_verified == rhs.c_verified &&
            c_cxupdatestatus == rhs.c_cxupdatestatus &&
            c_counter == rhs.c_counter &&
            c_lockstatus == rhs.c_lockstatus &&
            c_value == rhs.c_value &&
            c_identifier == rhs.c_identifier &&
            c_comment == rhs.c_comment &&
            c_lifetime == rhs.c_lifetime);
    }

	bool operator != (const CDPV3BookMark & rhs) const
	{
		return !(*this == rhs);
	}

	bool operator < (const CDPV3BookMark & rhs) const
	{
        if(c_timestamp < rhs.c_timestamp)
            return true;
        else if(c_timestamp > rhs.c_timestamp)
            return false;

        if(c_sequence < rhs.c_sequence)
            return true;
        else if(c_sequence > rhs.c_sequence)
            return false;

        if(c_apptype < rhs.c_apptype)
            return true;
        else if(c_apptype > rhs.c_apptype)
            return false;

        if(c_identifier < rhs.c_identifier)
            return true;
        else if(c_identifier > rhs.c_identifier)
            return false;

        return false;
    }

	SV_USHORT IsLockedBookmark(){ return c_lockstatus ;}
	SV_USHORT IsUnLockedBookmark(){ return !c_lockstatus ;}
};

#endif
