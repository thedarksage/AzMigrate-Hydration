//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdp_resync_txn.h
//
// Description: 
//

#ifndef CDPRESYNCTXN__H
#define CDPRESYNCTXN__H

#include <string>
#include <vector>
#include <boost/thread/mutex.hpp>

class cdp_resync_txn
{
public:
	typedef boost::shared_ptr<cdp_resync_txn> ptr;
	static std::string getfilepath(const std::string & retention_folder);

	cdp_resync_txn(const std::string & filepath, const std::string & resync_jobid)
		:m_filepath(filepath),m_jobid(resync_jobid),m_init(false) {}

	~cdp_resync_txn() { (void) flush(); }

	bool init();
	bool add_entry(const std::string & sync_filename);
	bool is_entry_present(const std::string & sync_filename);
	bool delete_entry(const std::string & sync_filename);
	bool flush();

private:
	boost::mutex m_mutex;
	std::string m_filepath;
	std::string m_jobid;
	std::vector<std::string> m_partially_applied_resync_files;
	bool m_init;
};

#endif