//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdpfixdbimpl.h
//
// Description: 
//

#ifndef CDPFIXDBIMPL__H
#define CDPFIXDBIMPL__H

#include <sstream>

#include <boost/shared_ptr.hpp>

#define FIX_TIME			0x0001
#define FIX_STALEINODES		0x0002
#define FIX_PHYSICALSIZE	0x0004

class CDPfixdbImpl
{
public:

	CDPfixdbImpl() {}

	virtual ~CDPfixdbImpl() {}
	virtual bool fixDBErrors(int flags) = 0;

protected:

private:

	CDPfixdbImpl(CDPfixdbImpl const & );
    CDPfixdbImpl& operator=(CDPfixdbImpl const & );

};

#endif

