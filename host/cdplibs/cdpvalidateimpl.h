//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdpvalidateimpl.h
//
// Description: 
//

#ifndef CDPVALIDATEIMPL__H
#define CDPVALIDATEIMPL__H

#include <sstream>

#include <boost/shared_ptr.hpp>

class CDPValidateImpl
{
public:

	CDPValidateImpl() {}

	virtual ~CDPValidateImpl() {}
	virtual bool validate() = 0;

protected:

private:

	CDPValidateImpl(CDPValidateImpl const & );
    CDPValidateImpl& operator=(CDPValidateImpl const & );

};

#endif

