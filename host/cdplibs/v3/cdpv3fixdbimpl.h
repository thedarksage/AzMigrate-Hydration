//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdpv3fixdbimpl.h
//
// Description: 
//

#ifndef CDPV3FIXDBIMPL__H
#define CDPV3FIXDBIMPL__H

#include "cdpfixdbimpl.h"
#include "cdpv3databaseimpl.h"

#include <sstream>

class CDPV3fixdbImpl :public CDPfixdbImpl
{
public:

	CDPV3fixdbImpl(const std::string &dbname);
	virtual ~CDPV3fixdbImpl();
	virtual bool fixDBErrors(int flags);

protected:

private:

	CDPV3fixdbImpl(CDPV3fixdbImpl const & );
    CDPV3fixdbImpl& operator=(CDPV3fixdbImpl const & );

	std::string m_dbname;
	CDPV3DatabaseImpl * m_databaseimpl;
};

#endif


