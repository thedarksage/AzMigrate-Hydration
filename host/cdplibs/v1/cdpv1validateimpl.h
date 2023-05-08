//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdpv1database.h
//
// Description: 
//

#ifndef CDPV1VALIDATEIMPL__H
#define CDPV1VALIDATEIMPL__H

#include "cdpvalidate.h"

#include <sstream>

class CDPV1ValidateImpl :public CDPValidateImpl
{
public:

	CDPV1ValidateImpl(const std::string &dbname, bool verbose);
	virtual ~CDPV1ValidateImpl() {}
	virtual bool validate();

protected:

private:

	CDPV1ValidateImpl(CDPV1ValidateImpl const & );
    CDPV1ValidateImpl& operator=(CDPV1ValidateImpl const & );
	bool IsDataFile(const std::string & fname);

	std::string m_dbname;
	bool   m_verbose;
};

#endif


