//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license agreement with InMage.
//
// File:       cdpfixdb.cpp
//
// Description: 
//

#include "cdpfixdb.h"
#include "portablehelpers.h"

#include "cdpv1fixdbimpl.h"
#include "cdpv3fixdbimpl.h"

CDPfixdb::CDPfixdb(const std::string & dbname,const std::string &tempDir)
{
	if(basename_r(dbname.c_str()) == CDPV1DBNAME)
    {
        m_impl = new CDPV1fixdbImpl(dbname,tempDir);
    }
    else if(basename_r(dbname.c_str()) == CDPV3DBNAME)
    {
         m_impl = new CDPV3fixdbImpl(dbname);
    }else
    {
        DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s unsupported dbpath:%s\n",
            FUNCTION_NAME, dbname.c_str());
    }
}

CDPfixdb::~CDPfixdb()
{
	delete m_impl;
}

bool CDPfixdb::fixDBErrors(int flags)
{
	return m_impl->fixDBErrors(flags);
}

