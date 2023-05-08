//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license agreement with InMage.
//
// File:       cdpvalidate.cpp
//
// Description: 
//

#include "cdpvalidate.h"
#include "portablehelpers.h"

#include "cdpv1validateimpl.h"
#include "cdpv3validateimpl.h"

CDPValidate::CDPValidate(const std::string & dbname,bool verbose)
{
	if(basename_r(dbname.c_str()) == CDPV1DBNAME)
    {
        m_impl = new CDPV1ValidateImpl(dbname,verbose);
    }
    else if(basename_r(dbname.c_str()) == CDPV3DBNAME)
    {
         m_impl = new CDPV3ValidateImpl(dbname);
    }else
    {
        DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s unsupported dbpath:%s\n",
            FUNCTION_NAME, dbname.c_str());
    }
}

CDPValidate::~CDPValidate()
{
	delete m_impl;
}

bool CDPValidate::validate()
{
	return m_impl->validate();
}

