//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpv3metadatareadahead.cpp
//
// Description: 
//

#include "cdpv3metadatareadahead.h"
#include "portablehelpersmajor.h"
#include "inmsafeint.h"
#include "inmageex.h"

#ifdef SV_UNIX
#include "portablehelpersminor.h"
#endif /* SV_UNIX */


cdpv3metadata_readahead_t::cdpv3metadata_readahead_t(size_t threadcount, unsigned int readahead_length)
:m_thrcount(threadcount),m_readahead_length(readahead_length),m_quit(false)
{
    for(int i =0; i < m_thrcount ; ++i)
        m_threads.create_thread(boost::bind(&cdpv3metadata_readahead_t::svc, this));
}

cdpv3metadata_readahead_t::~cdpv3metadata_readahead_t()
{
    quit();
    m_threads.join_all();
    m_metadatafiles.clear();
    m_timestamps.clear();
}

void cdpv3metadata_readahead_t::quit()
{
    boost::unique_lock<boost::mutex> lock(m_mutex);
    m_quit = true;
    m_cond.notify_all();
}

void cdpv3metadata_readahead_t::read_ahead(const SV_ULONGLONG& timestamp, const std::string & folder)
{
    boost::unique_lock<boost::mutex> lock(m_mutex);

    if(m_metadatafiles.find(timestamp) == m_metadatafiles.end())
    {
        m_timestamps.push_back(timestamp);
        std::string metadatapathname = cdpv3metadatafile_t::getfilepath(timestamp,folder);
        cdpv3metadatafileptr_t metadatafileptr(new cdpv3metadatafile_t(metadatapathname, -1, 4194304,false));
        m_metadatafiles.insert(std::make_pair(timestamp,metadatafileptr));
        m_cond.notify_all();
    }
}

cdpv3metadatafileptr_t cdpv3metadata_readahead_t::get_metadata_ptr(const SV_ULONGLONG & timestamp)
{
    boost::unique_lock<boost::mutex> lock(m_mutex);
    if(m_metadatafiles.find(timestamp) != m_metadatafiles.end())
        return m_metadatafiles.find(timestamp) ->second;
    else
        return  boost::shared_ptr<cdpv3metadatafile_t>();
}

void cdpv3metadata_readahead_t::close(const SV_ULONGLONG & timestamp)
{
    boost::unique_lock<boost::mutex> lock(m_mutex);
    m_metadatafiles.erase(timestamp);
}



void cdpv3metadata_readahead_t::svc()
{
    SV_ULONGLONG timestamp = 0;
    cdpv3metadatafileptr_t metadatafileptr;
    try
    {

        while(1)
        {
            {
                boost::unique_lock<boost::mutex> lock(m_mutex);

                if(m_quit)
                    return;

                if(m_timestamps.empty())
                {
                    m_cond.wait(lock);
                    continue;
                }

                timestamp  = m_timestamps.front();
                m_timestamps.pop_front();
                if(m_metadatafiles.find(timestamp) != m_metadatafiles.end())
                {
                    metadatafileptr = m_metadatafiles.find(timestamp) ->second;
                } else
                {
                    continue;
                }
            }

            SV_ULONGLONG filesize = metadatafileptr ->getfilesize();
            if(filesize)
            {
                size_t length_to_read = std::min<SV_ULONGLONG>(filesize, m_readahead_length);
				//If the file is bigger than 4M, we read the last part of the file ahead
				//issuing fadvise will help in keeping as much of data from the file in buffer cache.
				posix_fadvise_wrapper(metadatafileptr -> GetHandle(),0,0,INM_POSIX_FADV_WILLNEED);
                SV_ULONGLONG off;
                INM_SAFE_ARITHMETIC(off = InmSafeInt<SV_ULONGLONG>::Type(filesize) - length_to_read, INMAGE_EX(filesize)(length_to_read))
                metadatafileptr -> read_ahead(off, length_to_read);
                metadatafileptr -> set_ready_state();
            }
        }
    } 
    catch( exception const& e )
    {
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s.\n",
            FUNCTION_NAME, e.what());
    }
    catch ( ... )
    {
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception.\n",
            FUNCTION_NAME);
    }

}

