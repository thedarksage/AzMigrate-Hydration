//
// Copyright (c) 2006 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : dataprotectionutils.cpp
//
// Description:
//

#include <string>
#include <sstream>

#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_sys_time.h>
#include <ace/OS_NS_dirent.h>
#include <ace/Default_Constants.h>
#include <ace/ACE.h>

#include "dataprotectionutils.h"
#include "theconfigurator.h"
#include "configurevxagent.h"
#include "dataprotectionexception.h"
#include "differentialsync.h"

#include "transport_settings.h"
#include "file.h"
#include "error.h"

#include "cdpdatabase.h"
#include "cdputil.h"
#include "volumeinfo.h"
#include "decompress.h"

using namespace std;

boost::mutex DatapathTransportErrorLogger::ms_DataPathTransportErrorLock;

boost::mutex DiskReadErrorLogger::ms_DiskReadErrorLock;

boost::mutex g_psmutex;

SV_ULONGLONG DatapathThrottleLogger::s_prevLogTime = 0;
SV_UINT DatapathThrottleLogger::s_datapathThrottleCount = 0;
SV_ULONGLONG DatapathThrottleLogger::s_cumulativeTimeInThrottleSec = 0;
boost::mutex DatapathThrottleLogger::s_datapathThrottleLock;

const SV_UINT LogRateControlIntervalInSec = 600;

bool OnSameRootVolume(std::string & pathA, std::string & pathB)
{
    // TODO: GetVolumeRooPath should in general never fail
    // unless we pass wrong arguments
    // If it fails, should we throw an exception?
    std::string RootA, RootB;
    if(!GetVolumeRootPath(pathA, RootA)) {
        DebugPrintf(SV_LOG_ERROR, "Failure: OnSameVolumeRoot - GetVolumeRootPath for %s\n",
                    pathA.c_str());
        return false;
    }

    if(!GetVolumeRootPath(pathB, RootB)){
        DebugPrintf(SV_LOG_ERROR, "Failure: OnSameVolumeRoot - GetVolumeRootPath for %s\n",
                    pathB.c_str());
        return false;
    }

    if (boost::iequals(RootA, RootB))
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: SameVolumeRoot %s for %s and %s\n", FUNCTION_NAME,
            RootA.c_str(), pathA.c_str(), pathB.c_str());
        return true;
    }

    return false;
}

/** Fast Sync TBC - JobId Changes - BSR
 *  Check fileName is old or not
 *  Check if the file name has job id or not
 *  (no.of _ are 2 if no jobid in the file name, 3 if job is part of the file name)
 *  If Job Id is not available, go ahead and process it (current process)
 *  If Job Id is available, check if jobid == current JobId
 *  The file name formats are (without job id) :
 *  completed_<hcd/sdni/sync>_timestamp.dat
 *  completed_hcd_nomore.dat
 *  completed_sdni_nomore.dat
 *  completed_sync_nomore.dat
 *  The file name formats are (with jobid):
 *  completed_<hcd/sdni/sync>_jobid_timestamp.dat
 *  completed_hcd_jobid_nomore.dat
 *  completed_sdni_jobid_nomore.dat
 *  completed_sync_jobid_nomore.dat
 **/
/** Use count to get the no.of _s
 *  if count > 2, find the jobid in the file name.
 *  If found return 1 **/

bool canProcess( const std::string& fileName, const std::string& jobId)
{
    DebugPrintf ( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    int underScoreCount =  -1;
    int index = -1 ;
    int index1 = -1 ;
    std::string jobIdTemp ;
    bool bValue = false ;
    const int nJobIdPosition = 2;

    do
    {
        underScoreCount++ ;
        if( underScoreCount > nJobIdPosition )
        {
            jobIdTemp = fileName.substr(index1, jobId.length());
            bValue = (0 == jobId.compare(jobIdTemp));

            DebugPrintf( SV_LOG_INFO, "JobId from the file: %s \n Current Job ID: %s \n",
                         jobIdTemp.c_str(),
                         jobId.c_str() ) ;
            break ;
        }
        index1 = ++index ;
        index = fileName.find( "_", index1 ) ;
    }while ( index != std::string::npos ) ;

    if (underScoreCount == nJobIdPosition)
    {
        bValue = true;
    }
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return bValue ;
}


std::string GetPrintableCheckSum(const unsigned char *hash)
{
    char printablecs[INM_MD5TEXTSIGLEN + 1] = {'\0'};

    for (int i = 0; i < INM_MD5TEXTSIGLEN/2; i++) 
    {
        inm_sprintf_s((printablecs + i * 2), (ARRAYSIZE(printablecs) - (i*2)), "%02X", hash[i]);
    }

    return printablecs;
}

bool IsRcmMode()
{
#ifdef DP_SYNC_RCM
    return true;
#else
    return false;
#endif
}

//
// Returns the PS component installation path.
//
std::string GetPSInstallPathFromRegistry()
{
#ifdef SV_WINDOWS
    static std::string psinstallpath;
    std::list<InstalledProduct> psProducts;
    const std::string PsKeyName("9");
    if (psinstallpath.empty())
    {
        boost::mutex::scoped_lock guard(g_psmutex);
        if (psinstallpath.empty())
        {
            if (GetInMageInstalledProductsFromRegistry(PsKeyName, psProducts) != SVS_OK)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to fetch PS product details from registry.\n");
                throw std::exception("Failed to get PS installation path from registry.");
            }
            else
            {
                psinstallpath = psProducts.front().installLocation;
            }
        }
    }

    DebugPrintf(SV_LOG_ALWAYS, "PS install location from registry: %s\n", psinstallpath.c_str());
    return psinstallpath;
#else
    return std::string();
#endif
}

//
// LogDatapathTransportError does data path transport error logging rate control with fix interval of 10 mins.
//
void DatapathTransportErrorLogger::LogDatapathTransportError(const std::string& errorMsg,
    const boost::chrono::steady_clock::time_point errorMsgTime)
{
    boost::mutex::scoped_lock guard(ms_DataPathTransportErrorLock);

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    // Always log first exception - for first log initialize prevLogTime with minus 60 mins.
    static boost::chrono::steady_clock::time_point prevLogTime = boost::chrono::steady_clock::now() - boost::chrono::seconds(3600);

    if ((prevLogTime + boost::chrono::seconds(600)) <= errorMsgTime)
    {
        prevLogTime = errorMsgTime;
        DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errorMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

//
// LogDiskReadError does disk read error logging rate control with fix interval of 10 mins.
//
void DiskReadErrorLogger::LogDiskReadError(const std::string& errorMsg,
    const boost::chrono::steady_clock::time_point errorMsgTime)
{
    boost::mutex::scoped_lock guard(ms_DiskReadErrorLock);

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    // Always log first exception - for first log initialize prevLogTime with minus 60 mins.
    static boost::chrono::steady_clock::time_point prevLogTime = boost::chrono::steady_clock::now() - boost::chrono::seconds(3600);

    if ((prevLogTime + boost::chrono::seconds(600)) <= errorMsgTime)
    {
        prevLogTime = errorMsgTime;
        DebugPrintf(SV_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errorMsg.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

//
// LogDatapathThrottle does Datapath Throttle logging rate control with fix interval of 10 mins
//
void DatapathThrottleLogger::LogDatapathThrottleInfo(const std::string &msg,
    const boost::chrono::steady_clock::time_point &msgTime,
    const SV_ULONGLONG &ttlsec)
{
    boost::mutex::scoped_lock guard(s_datapathThrottleLock);

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    const boost::chrono::seconds msgTimeInSec =
        boost::chrono::duration_cast<boost::chrono::seconds>(msgTime.time_since_epoch());
    
    s_datapathThrottleCount++;
    s_cumulativeTimeInThrottleSec += ttlsec;

    if ((msgTimeInSec.count() - s_prevLogTime) >= LogRateControlIntervalInSec)
    {
        DebugPrintf(SV_LOG_ALWAYS, "%s: %s\n", FUNCTION_NAME, msg.c_str());

        if (s_prevLogTime)
        {
            std::stringstream ss;
            ss << "device throttled " << s_datapathThrottleCount << " times in last " << msgTimeInSec.count() - s_prevLogTime
                << " sec total throttle time=" << s_cumulativeTimeInThrottleSec << " sec";
            DebugPrintf(SV_LOG_ALWAYS, "%s: %s\n", FUNCTION_NAME, ss.str().c_str());
        }

        s_prevLogTime = msgTimeInSec.count();
        s_datapathThrottleCount = 0;
        s_cumulativeTimeInThrottleSec = 0;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}