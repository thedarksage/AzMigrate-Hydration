//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdpv1fixdb.h
//
// Description: 
//


#ifndef CDPV1FIXDBIMPL__H
#define CDPV1FIXDBIMPL__H

#include <stdio.h>
#include <string>
#include "svtypes.h"
#include "cdpfixdbimpl.h"


class CDPV1fixdbImpl:public CDPfixdbImpl
{
public:

    CDPV1fixdbImpl(const std::string &dbname, const std::string &tempdir);
    virtual ~CDPV1fixdbImpl(){}
	virtual bool fixDBErrors(int flags);

private:

    bool redate(int flags);
	bool fixPhysicalSize();
    int redateDATFile      (const char *fileName, const SV_ULONGLONG offset);
    int OnHeader            (FILE *in, const unsigned long size, const unsigned long flags);
    int OnHeaderV2          (FILE *in, const unsigned long size, const unsigned long flags);
    int OnDirtyBlocksData   (FILE *in, const unsigned long size, const unsigned long flags);
    int OnDirtyBlocksDataV2 (FILE *in, const unsigned long size, const unsigned long flags);
    int OnDirtyBlocks       (FILE *in, const unsigned long size, const unsigned long flags);
    int OnDRTDChanges       (FILE *in, const unsigned long size, const unsigned long flags);
    int OnTimeStampInfo     (FILE *in, const unsigned long size, const unsigned long flags);
    int OnTimeStampInfoV2   (FILE *in, const unsigned long size, const unsigned long flags);
    int OnUDT               (FILE *in, const unsigned long size, const unsigned long flags);
    size_t seek_and_fwrite   (const void *buf, size_t size, size_t nitems, FILE *fpcontext, long );

    bool copyFile(const std::string &fromFile, const std::string &toDir);

    std::string db_name,tempDir;

};

#endif
