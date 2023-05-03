//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdp_resync_txn.cpp
//
// Description: 
//

#include "cdp_resync_txn.h"
#include "ace/ACE.h"
#include "ace/OS.h"
#include <algorithm>
#include <fstream>

#include "logger.h"
#include "portable.h"
#include "portablehelpers.h"
using namespace std;


//
// cdp_resync_txn file holds the name(s) of the resync files
// which have already been applied to retention
// In case , after retention update, if the apply(writes) to 
// target volume fails or the sync file deletion on PS fails
// we will receive the same sync file again to apply
// If the sync file has already been applied as per cdp_resync_txn file
// retention update will be skipped for the same
// this helps in following: 
//  1) avoiding unnecessary retention update, 
//  2) huge retention file growth in case of continous file deletion failure or volume write failures
// 
// Once the sync file deletion from PS is completed, the entry is removed 
// from cdp_resync_txn file
//


const std::string g_cdpresynctxnfilename = "cdp_resync_txn.dat";

std::string cdp_resync_txn::getfilepath(const std::string & retetion_folder)
{
	return retetion_folder + ACE_DIRECTORY_SEPARATOR_CHAR_A + g_cdpresynctxnfilename;
}


bool cdp_resync_txn::init()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);
	ifstream resync_txnfile_stream(m_filepath.c_str());

	if(resync_txnfile_stream.is_open())
	{
		char data[512] = {'\0' };
		
		// read the resync job id
		memset(data,'\0', 512);
		resync_txnfile_stream.getline(data,512);
		if(resync_txnfile_stream.good())
		{
			std::string jobid = data;
			if(jobid != m_jobid)
			{
				m_init = true; // stale file from previous resync session, no need to read rest of the entries
				DebugPrintf(SV_LOG_DEBUG, "%s is from a previous session, jobid in  the file: %s , current job id: %s \n",
					m_filepath.c_str(), jobid.c_str(), m_jobid.c_str());

			} else // jobid matches, read the rest of the entries
			{
				while(!resync_txnfile_stream.eof())
				{
					memset(data,'\0', 512);
					resync_txnfile_stream.getline(data,512);
					if(!resync_txnfile_stream.good()) break;
					std::string sync_file_name = data;
					if(sync_file_name.empty())
						continue;
					m_partially_applied_resync_files.push_back(sync_file_name);
				}
				if(resync_txnfile_stream.eof()) 
					m_init = true;
			}
		}

	} else 
	{
		m_init = true;
	}

	DebugPrintf(SV_LOG_DEBUG, "Leaving %s\n", FUNCTION_NAME);
	return m_init;
}

bool cdp_resync_txn::add_entry(const std::string & sync_filename)
{
	bool rv = true;
	boost::unique_lock<boost::mutex> lock(m_mutex);
	m_partially_applied_resync_files.push_back(sync_filename);
	return rv;
}


bool cdp_resync_txn::is_entry_present(const std::string & sync_filename)
{
	boost::unique_lock<boost::mutex> lock(m_mutex);
	return ( find(m_partially_applied_resync_files.begin(), m_partially_applied_resync_files.end(),sync_filename) != m_partially_applied_resync_files.end());
}

bool cdp_resync_txn::delete_entry(const std::string & sync_filename)
{
	boost::unique_lock<boost::mutex> lock(m_mutex);
	bool rv = true;
	m_partially_applied_resync_files.erase(std::remove
		(m_partially_applied_resync_files.begin(), m_partially_applied_resync_files.end(), sync_filename),
		m_partially_applied_resync_files.end());

	return rv;
}

bool cdp_resync_txn::flush()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);
	boost::unique_lock<boost::mutex> lock(m_mutex);
	bool rv = true;
	do
	{
		if(m_init)
		{
			std::string tmp_filename = m_filepath + ".tmp";
			ofstream resync_txnfile_stream(tmp_filename.c_str(),ios::trunc);
			if(!resync_txnfile_stream.good())
			{
				DebugPrintf(SV_LOG_ERROR, "open failed for %s, function: %s\n",tmp_filename.c_str(), FUNCTION_NAME);
				rv = false;
				break;
			}

			DebugPrintf(SV_LOG_DEBUG, "Function: %s , adding %s  to %s\n", FUNCTION_NAME,
				m_jobid.c_str(), tmp_filename.c_str());

			resync_txnfile_stream << m_jobid << "\n";
			if(!resync_txnfile_stream.good())
			{
				DebugPrintf(SV_LOG_ERROR, "write [%s] failed for %s, function: %s\n",m_jobid.c_str(),
					tmp_filename.c_str(), FUNCTION_NAME);
				rv = false;
				break;
			}

			std::vector<std::string>::const_iterator it = m_partially_applied_resync_files. begin();
			for ( ; it != m_partially_applied_resync_files.end(); ++it)	{
				std::string sync_file_name = *it;
				
				DebugPrintf(SV_LOG_DEBUG, "Function: %s , appending %s  to %s\n", FUNCTION_NAME,
					sync_file_name.c_str(), tmp_filename.c_str());

				resync_txnfile_stream << sync_file_name << "\n";
				if(!resync_txnfile_stream.good())
				{
					DebugPrintf(SV_LOG_ERROR, "write [%s] failed for %s, function: %s\n",sync_file_name.c_str(),
						tmp_filename.c_str(), FUNCTION_NAME);
					rv = false;
					break;
				}
			}

			if(!rv) break;

			resync_txnfile_stream.flush();
			if(!resync_txnfile_stream.good())
			{
				DebugPrintf(SV_LOG_ERROR, "flush call failed for %s, function: %s\n",
					tmp_filename.c_str(), FUNCTION_NAME);
				rv = false;
				break;
			}

			resync_txnfile_stream.close();
			if(rv && (0 != ACE_OS::rename(tmp_filename.c_str(),m_filepath.c_str())) ) {
				DebugPrintf(SV_LOG_ERROR, "rename failed for %s -> %s, function: %s\n",
					tmp_filename.c_str(), m_filepath.c_str(),FUNCTION_NAME);
				rv = false;
				break;
			}
		}
	} while(0);

	DebugPrintf(SV_LOG_DEBUG, "Leaving %s\n", FUNCTION_NAME);
	return rv;
}