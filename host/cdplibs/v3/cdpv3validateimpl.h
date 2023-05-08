//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdpv3validateimpl.h
//
// Description: 
//

#ifndef CDPV3VALIDATEIMPL__H
#define CDPV3VALIDATEIMPL__H

#include "cdpvalidateimpl.h"
#include "cdpv3databaseimpl.h"

#include <sstream>

class CDPV3ValidateImpl :public CDPValidateImpl
{
public:

	CDPV3ValidateImpl(const std::string &dbname);
	virtual ~CDPV3ValidateImpl();
	virtual bool validate();

protected:

private:

	CDPV3ValidateImpl(CDPV3ValidateImpl const & );
    CDPV3ValidateImpl& operator=(CDPV3ValidateImpl const & );

	std::string m_dbname;
	bool   m_verbose;
	CDPV3DatabaseImpl * m_databaseinstance;
};

#endif


