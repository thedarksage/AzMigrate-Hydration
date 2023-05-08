//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdpfixdb.h
//
// Description: 
//

#ifndef CDPFIXDB__H
#define CDPFIXDB__H

#include <sstream>
#include "cdpfixdbimpl.h"

#include <boost/shared_ptr.hpp>

class CDPfixdb
{
public:
	CDPfixdb(const std::string &dbname,const std::string &tempDir);

	virtual ~CDPfixdb() ;
	virtual bool fixDBErrors(int flags);

protected:

private:

	CDPfixdb(CDPfixdb const & );
    CDPfixdb& operator=(CDPfixdb const & );

	CDPfixdbImpl* m_impl;

};

#endif

