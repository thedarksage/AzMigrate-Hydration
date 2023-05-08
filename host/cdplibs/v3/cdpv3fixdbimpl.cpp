//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpv3validateimpl.cpp
//
// Description: 
//

#include "cdpv3fixdbimpl.h"
#include "cdpv3databaseimpl.h"
#include "portablehelpers.h"

#include <sstream>
#include <fstream>

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"

using namespace sqlite3x;
using namespace std;

CDPV3fixdbImpl::CDPV3fixdbImpl(const std::string & dbname)
{
	m_databaseimpl = new CDPV3DatabaseImpl(dbname);
}

CDPV3fixdbImpl::~CDPV3fixdbImpl()
{
	delete m_databaseimpl;
}

bool CDPV3fixdbImpl::fixDBErrors(int flags)
{
	return m_databaseimpl->delete_stalefiles();
}



