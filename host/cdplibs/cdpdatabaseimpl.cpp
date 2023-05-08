//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpdatabaserdr.cpp
//
// Description: 
//

#include "cdputil.h"
#include "cdpdatabaseimpl.h"

#include "portablehelpers.h"
#include "inmageex.h"

#include <sstream>

using namespace sqlite3x;
using namespace std;

CDPDatabaseImpl::CDPDatabaseImpl( const std::string & dbname ):
m_dbname(dbname),
m_dbdir(dirname_r(dbname.c_str())), good_status(true)
{
	m_io_count = 0;
	m_io_bytes = 0;
}

CDPDatabaseImpl::~CDPDatabaseImpl()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());

	try
	{
		disconnect();
	}
	catch (std::exception & ex)
	{
		DebugPrintf(SV_LOG_ERROR, "Caught exception %s in %s\n", ex.what(), FUNCTION_NAME);
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s for %s\n",FUNCTION_NAME, m_dbname.c_str());
}



void CDPDatabaseImpl::connect()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	do
	{
		if (m_con)
		{
			break;
		}

		// PR#10815: Long Path support
		sqlite3_connection * con = new sqlite3_connection(getLongPathName(m_dbname.c_str()).c_str());
	
		m_con.reset(con);
		m_con -> setbusyhandler(CDPUtil::cdp_busy_handler, NULL);

	} while (FALSE);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void CDPDatabaseImpl::disconnect()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	m_reader.close();

	if(m_trans)
	{
		m_trans ->rollback();
		m_trans.reset();
	}
	m_cmd.reset();
	m_con.reset();

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

void CDPDatabaseImpl::BeginTransaction()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	sqlite3_transaction * trans = new sqlite3_transaction(*m_con, true, sqlite3_transaction::sqlite3txn_immediate);
	m_trans.reset(trans);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void CDPDatabaseImpl::CommitTransaction()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = false;

	m_trans ->commit();
	m_trans.reset();
	rv = true;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void CDPDatabaseImpl::executestmts(std::vector<std::string> const & stmts)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	
	std::vector<std::string>::const_iterator stmtsIter = stmts.begin();
	std::vector<std::string>::const_iterator stmtsEnd = stmts.end();

	for(; stmtsIter != stmtsEnd; ++stmtsIter)
	{
		m_con->executenonquery(*stmtsIter);
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


bool CDPDatabaseImpl::isempty(std::string const & table)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	do
	{
		std::stringstream query;
		query << "select 1 from " << table <<" limit 1" ;

		boost::shared_ptr<sqlite3_command> cmd(new sqlite3_command(*m_con, query.str()));
		sqlite3_reader reader= cmd -> executereader();

		if(reader.read())
		{
			rv = false;
			break;
		}

	} while (0); 

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv ;
}

void CDPDatabaseImpl::update_io_stats(cdp_io_stats_t stats)
{
	m_io_count += stats.read_io_count;
	m_io_bytes += stats.read_bytes_count;
}

cdp_io_stats_t CDPDatabaseImpl::get_io_stats()
{
	cdp_io_stats_t stats;

	stats.read_io_count = m_io_count;
	stats.read_bytes_count = m_io_bytes;

	return stats;
}