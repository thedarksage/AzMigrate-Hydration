//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpv3globals.h
//
// Description: 
//

#ifndef CDPV3GLOBALS__H
#define CDPV3GLOBALS__H

///
/// Max Length of any column
///
const int CDPV3_MAX_COL_LEN      = 100;

const double    CDPV3VER0                = 3.0;
const double    CDPV3VER                 = CDPV3VER0;

const double    CDPV3VER3REV0            = 0.0;
const double    CDPV3VER3REV1            = 1.0;
const double    CDPV3VER3REV2            = 2.0;
const double    CDPV3VERCURRREV          = CDPV3VER3REV2;

//  
// Status of information that has to be sent to CX
//  0 : cx add pending
//  1 : cx add complete
//
const unsigned int CS_ADD_PENDING    = 0;
const unsigned int CS_ADD_DONE       = 1;
const unsigned int CS_DELETE_PENDING = 2;


//
// type flags per metadata record
//
#define DIFFTYPE_PERIO_BIT_SET	1
#define DIFFTYPE_DIFFSYNC_BIT_SET	2
#define DIFFTYPE_NONSPLITIO_BIT_SET	4

const unsigned int CDP_BASE_YEAR = 2009;

const std::string CDPV3_MD_PREFIX = "cdpv30_md_";
const std::string CDPV3_MD_SUFFIX = ".mdat";

const std::string CDPV3_DATA_PREFIX = "cdpv30_";
const std::string CDPV3_DATA_SUFFIX = ".dat";

const std::string CDPV3_COALESCED_PREFIX = "cdpv30_del_";
const std::string CDPV3_COALESCED_SUFFIX = ".log";

const std::string CDPV3_FILE_PATTERN = "cdpv3*";

const std::string CDPV3_STALE_FILES_DELLOG = "cdpv30_stale_files_deletion.log";

const std::string CDPV3_UNUSABLE_RECOVERYPTS_DELLOG = "cdpv30_unusable_recoverypts_deletion.log";
#endif
