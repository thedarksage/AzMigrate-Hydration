//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpv3metadatareadahead.h
//
// Description: 
//

#ifndef CDPV3METADATAREADAHEAD__H
#define CDPV3METADATAREADAHEAD__H

#include "cdpv3metadatafile.h"

#include <vector>
#include <map>
#include <vector>
using namespace std;

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/thread.hpp>


class cdpv3metadata_readahead_t
{
public:
    cdpv3metadata_readahead_t(size_t threadcount, unsigned int readahead_length);
    ~cdpv3metadata_readahead_t();
    void read_ahead(const SV_ULONGLONG& timestamp, const std::string & folder);
    cdpv3metadatafileptr_t get_metadata_ptr(const SV_ULONGLONG & timestamp);
    void close(const SV_ULONGLONG & timestamp);

private:
    
    void svc();
    void quit();

    boost::mutex m_mutex;
    boost::condition_variable m_cond;
    size_t m_thrcount;
    unsigned int m_readahead_length;
    bool m_quit;
    std::list<SV_ULONGLONG> m_timestamps;
    std::map<SV_ULONGLONG, cdpv3metadatafileptr_t> m_metadatafiles;
    boost::thread_group m_threads;
};

typedef boost::shared_ptr<cdpv3metadata_readahead_t> cdpv3metadata_readahead_ptr_t;
#endif