//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : diffinfo.cpp
//
// Description:
//

#include <sstream>
#include <iostream>
#include <locale>

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

#include "diffinfo.h"
#include "volumeinfo.h"
#include "dataprotectionexception.h"
#include "theconfigurator.h"
#include "dataprotectionutils.h"
#include "configurevxagent.h"
#include "cdpplatform.h"
#include "decompress.h"
#include "differentialsync.h"

#if SV_WINDOWS
#include "hostagenthelpers.h"       // ProcessAndApplyDeltaFiles()
#endif

using namespace std;

// ==========================================================================
// DiffInfo
// ==========================================================================
// --------------------------------------------------------------------------
// constructor
// --------------------------------------------------------------------------
DiffInfo::DiffInfo(VolumeInfo* volumeInfo, std::string const & location, char const * id, long size)
: m_Id(id),
m_Size(size),
m_VolumeInfo(volumeInfo),
m_SortCriterion(id, volumeInfo->Name())
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_DiffLocation = m_VolumeInfo->DiffLocation();
    m_DiffLocation += '/';
    m_DiffLocation += m_Id;
    m_bDataInMemory = false;
    m_DataBuffer.reset();
    m_DataLen = 0;

    m_FullCacheLocation = m_VolumeInfo ->CacheLocation();
    
    m_FullCacheLocation += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    m_FullCacheLocation += m_Id;
	m_ReadyToApply = true;
}


// --------------------------------------------------------------------------
// compares DiffInfos using sort criterion based on end time
// --------------------------------------------------------------------------
bool DiffInfo::CompareEndTime(DiffInfoPtr const & rhs) const 
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return m_SortCriterion.Lessthan(rhs->m_SortCriterion);
}

// --------------------------------------------------------------------------
// opens the volume
// --------------------------------------------------------------------------
bool DiffInfo::OpenVolumeExclusive()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return m_VolumeInfo->OpenVolumeExclusive();
}

// --------------------------------------------------------------------------
// compares cutoff time with end time returns ture only if cutofftime
// is greater then the end time
// --------------------------------------------------------------------------
bool DiffInfo::CutoffTimeGreaterThenEndTime(DiffSortCriterion const & cutoffTime)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return !cutoffTime.Lessthan(m_SortCriterion);
}

// --------------------------------------------------------------------------
// compares cutoff time with end time return true only if end time 
// is then cutofftime
// --------------------------------------------------------------------------
bool DiffInfo::EndTimeLessThenCutoffTime(DiffSortCriterion const & cutoffTime)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return m_SortCriterion.Lessthan(cutoffTime);
}


// --------------------------------------------------------------------------
// sets the full cache location with the diff file name and
// sets ok to apply to let ApplyDiff know it can apply the diff
// --------------------------------------------------------------------------
void DiffInfo::SetReadyToApply(std::string const & cacheLocation)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_FullCacheLocation = cacheLocation;
    m_bDataInMemory = false;
    m_DataBuffer.reset();
    m_DataLen = 0;
    m_ReadyToApply = true;
}

// --------------------------------------------------------------------------
// sets the full cache location with the diff file name and
// sets ok to apply to let ApplyDiff know it can apply the diff
// --------------------------------------------------------------------------
void DiffInfo::SetReadyToApply(std::string const & cacheLocation, char * source, SV_ULONG sourceLen)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_FullCacheLocation = cacheLocation;

    // just in case someone called SetReadyToApply 
    //  multiple times without releasing previous memory
    m_DataBuffer.reset();

    // use smart pointer to track 
    // need to pass in the deallocator here to
    // make sure it will use free
    // the assumption that te buffer source is allocated using malloc
    // or equivalent
    m_DataBuffer.reset(source, free);

    m_DataLen = sourceLen;
    m_bDataInMemory = true;
    m_ReadyToApply = true;
}

// ----------------------------------------------------------------------
// clear the memory
// note no deallocator here so that a future call 
// to reset while it is 0 will use delete not
// free. delete is safe to call with 0, free
// is not
// -----------------------------------------------------------------------
void DiffInfo::ReleaseBuffer()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if(m_DataBuffer)
        m_VolumeInfo -> AdjustMemoryUsage(true, m_DataLen);

    m_DataBuffer.reset();
    m_DataLen = 0;
    m_bDataInMemory = false;
}

// --------------------------------------------------------------------------
// returns the name of the volume
// --------------------------------------------------------------------------
char const * DiffInfo::VolumeInfoName()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return m_VolumeInfo->Name();
}

// --------------------------------------------------------------------------
// returns the cache location
// --------------------------------------------------------------------------
std::string const & DiffInfo::CacheLocation()
{
    return m_VolumeInfo->CacheLocation();
}

//----------------------------------------------------------------------------
TRANSPORT_PROTOCOL DiffInfo:: Protocol()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return m_VolumeInfo->Protocol();
}

// --------------------------------------------------------------------------
// returns secure setting (true = secure, false = non secure)
// --------------------------------------------------------------------------
bool DiffInfo::Secure()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    return m_VolumeInfo->Secure();
}

// --------------------------------------------------------------------------
// applies diff data and cleans up the diff data from cache and server
// --------------------------------------------------------------------------
bool DiffInfo::ApplyData(DifferentialSync * diffSync, bool preAllocate, bool lastFileToProcess)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    // always delete the cache file
    //boost::shared_ptr<void> guard((void*)NULL, boost::bind(&DeleteLocalFile, FullCacheLocation()));



        //__asm int 3;
        CDPV2Writer::Ptr cdpwriter;
		long long totalBytesApplied = 0;
        if(!m_VolumeInfo ->GetCDPV2Writer(cdpwriter))
        {
            return false;
        }

        if(!cdpwriter ->Applychanges(FullCacheLocation(), 
            totalBytesApplied, 0, 0, 0, 0,
			ContinuationId(), lastFileToProcess))
        {
			//If Applychanges() call fails, chances for it to succeed in the next run are less.
			//For this reason if dataprotection exits without any delay it will be launched imediately and
			//the same error may be hit again and again. To avoid this we add configurable delay
			LocalConfigurator lc;
			CDPUtil::QuitRequested(lc.DPDelayBeforeExitOnError());
            return false;
        }
    return true;
}

bool DiffInfo::EndsInDotgz()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::string::size_type idx = m_Id.rfind(".gz");
    if (std::string::npos == idx || (m_Id.length() - idx) > 3) {
        return false; // uncompressed;
    }
    return true;
}

bool DiffInfo::tsoFile()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    std::string::size_type idx = m_Id.find("_tso_");
    if (std::string::npos == idx ) {
        return false; // not a tso file
    }
    return true;
}

bool DiffInfo::Uncompress()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    //__asm int 3;
    char * pgunzip =  NULL;
    SV_ULONG gunzipLen = 0;

    if(IfDataOnDisk())
    {
        return UncompressIfNeeded();
    }

    // it is already uncompressed
    if(!EndsInDotgz())
        return true;

    GzipUncompress uncompresser;

    // let's try to uncompress it in memory
    if(uncompresser.UnCompress(m_DataBuffer.get() , m_DataLen, &pgunzip, gunzipLen)) 
    {
        // remove the .gz
        std::string::size_type idx1 = m_Id.rfind(".gz");
        m_Id.erase(idx1, m_Id.length());

        std::string::size_type idx2 = m_FullCacheLocation.rfind(".gz");
        m_FullCacheLocation.erase(idx2, m_FullCacheLocation.length());

        // adjust the memory usage
        SV_ULONG adjustment =0;
        bool decrement = ( gunzipLen < m_DataLen );
        if( decrement )	{
            adjustment = m_DataLen - gunzipLen;
        } else {
            adjustment = gunzipLen - m_DataLen;
        }

        // reset the databuffer to point to uncompressed buffer
        m_DataBuffer.reset(pgunzip, free);
        m_VolumeInfo ->AdjustMemoryUsage(decrement, adjustment);
        m_DataLen = gunzipLen;

    } else {  // uncompress to memory failed. let's try to uncompress onto disk

        // first free up any memory allocated by to memory uncompressor
        if(pgunzip) {
            free(pgunzip);
            pgunzip = NULL;
        }

        // remove the .gz
        std::string::size_type idx1 = m_Id.rfind(".gz");
        m_Id.erase(idx1, m_Id.length());

        std::string::size_type idx2 = m_FullCacheLocation.rfind(".gz");
        m_FullCacheLocation.erase(idx2, m_FullCacheLocation.length());

        if(!uncompresser.UnCompress(m_DataBuffer.get() , m_DataLen, m_FullCacheLocation)) {
            DebugPrintf(SV_LOG_ERROR, "DiffInfo::GzipUncompress failed for %s.gz\n", m_Id.c_str());
            return false;
        }

        // release memory for next download
        ReleaseBuffer();		
    }

    return true;
}


bool DiffInfo::UncompressIfNeeded()
{  
	bool rv = false;
	DebugPrintf(SV_LOG_DEBUG,"Inside %s %s\n",FUNCTION_NAME, m_FullCacheLocation.c_str());
	do
	{
		GzipUncompress uncompresser;
		std::string uncompressedFileName = m_FullCacheLocation;

		std::string::size_type idx = m_FullCacheLocation.rfind(".gz");
		if (std::string::npos == idx || (m_FullCacheLocation.length() - idx) > 3) 
		{
			return true; // uncompress not needed;
		}

		uncompressedFileName.erase(idx, uncompressedFileName.length());
		if(!uncompresser.UnCompress(m_FullCacheLocation,uncompressedFileName))
		{
			std::ostringstream msg;
			msg << "Uncompress failed for "
				<< m_FullCacheLocation
				<< "\n";

			DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());

			// remove partial uncompressed file
			ACE_OS::unlink(getLongPathName(uncompressedFileName.c_str()).c_str());
			rv = false;
			break;
		} else
		{
			// remove the .gz
			ACE_OS::unlink(getLongPathName(m_FullCacheLocation.c_str()).c_str());
			m_FullCacheLocation.erase(idx, m_FullCacheLocation.length());
			rv = true;
			break;
		}

	}while(false);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}


