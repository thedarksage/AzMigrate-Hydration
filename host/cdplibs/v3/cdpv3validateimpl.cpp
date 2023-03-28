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

#include "cdpv3validateimpl.h"
#include "cdpv3databaseimpl.h"
#include "cdpv3metadatafile.h"
#include "portablehelpers.h"
#include "cdputil.h"
#include "file.h"
#include "localconfigurator.h"

#include <sstream>
#include <fstream>

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"

using namespace sqlite3x;
using namespace std;

CDPV3ValidateImpl::CDPV3ValidateImpl(const std::string & dbname)
{
	m_databaseinstance = new CDPV3DatabaseImpl(dbname);
}

CDPV3ValidateImpl::~CDPV3ValidateImpl()
{
	delete m_databaseinstance;
}

bool CDPV3ValidateImpl::validate()
{
	return m_databaseinstance->validate();
}



