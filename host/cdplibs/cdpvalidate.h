//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdpvalidate.h
//
// Description: 
//

#ifndef CDPVALIDATE__H
#define CDPVALIDATE__H

#include <sstream>
#include "cdpvalidateimpl.h"

#include <boost/shared_ptr.hpp>

class CDPValidate
{
public:
	CDPValidate(const std::string &dbname,bool verbose = false);

	virtual ~CDPValidate() ;
	virtual bool validate();

protected:

private:

	CDPValidate(CDPValidate const & );
    CDPValidate& operator=(CDPValidate const & );

	CDPValidateImpl* m_impl;

};

#endif

