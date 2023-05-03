//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpv1globals.h
//
// Description: 
//

#ifndef CDPV1GLOBALS__H
#define CDPV1GLOBALS__H

///
/// Max Length of any column
///
const int CDPV1_MAX_COL_LEN      = 100;

const double    CDPVER0                = 0.0;
const double    CDPVER1                = 1.0;
const double    CDPVER                 = CDPVER1;

const double    CDPVER1REV0            = 0.0;
const double    CDPVER1REV1            = 1.0;
const double    CDPVER1REV2            = 2.0;
const double    CDPVER1REV3            = 3.0;
const double    CDPVERCURRREV          = CDPVER1REV3;

//
// PR#6362: Per I/O TimeStamp Changes
// strucure of inode table is modified to 
// accommadate timestamp and sequence no. per change
// To support upgrade of inode table of an existing older
// retention database
// version field has been added to the inode table
// if the version is old, data would be read in old format
// and then converted to new format
// 
// support added in version 1:
//   diff type ie non split i/o, split i/o, split continuation, split end
// support added in version 2:
//   per i/o information
//
#define CDPV1_INODE_VERSION0             0
#define CDPV1_INODE_VERSION1             1
#define CDPV1_INODE_PERIOVERSION         CDPV1_INODE_VERSION1
#define CDPV1_INODE_CURVER               CDPV1_INODE_VERSION1
#endif
