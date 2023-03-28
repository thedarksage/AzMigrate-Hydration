//
// Copyright (c) 2006 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : dataprotectionutils.h
//
// Description:
//

#ifndef DATAPROTECTIONUTILS_H
#define DATAPROTECTIONUTILS_H

#include <string>

#include <boost/chrono.hpp>
#include <boost/thread/mutex.hpp>

#include "hostagenthelpers_ported.h"
#include "volume.h"
#include "volumeinfo.h"
#include "bitmap.h"
#include "cxtransport.h"
#include "svtypes.h"
#include "cdpvolume.h"

class VolumeInfo;
class DifferentialSync;

bool OnSameRootVolume(std::string & pathA, std::string & pathB);

bool canProcess( const std::string& fileName, const std::string& jobId) ;
std::string GetPrintableCheckSum(const unsigned char *hash);

typedef struct VolumeWithOffsetToApply
{
	std::string m_Name;
    SV_LONGLONG m_Offset;
 
    VolumeWithOffsetToApply(const std::string &name,
                            const SV_LONGLONG &offset):
	 m_Name (name),
     m_Offset(offset)
    {
    }
} VolumeWithOffsetToApply_t;

//
// Returns true if current module is running in Rcm mode
//
bool IsRcmMode();

//
// Returns the PS component installation path.
//
std::string GetPSInstallPathFromRegistry();

//
// DatapathTransportErrorLogger defines static function for data path transport error logging rate control
//
class DatapathTransportErrorLogger
{
public:
    static void LogDatapathTransportError(const std::string& errorMsg,
        const boost::chrono::steady_clock::time_point errorMsgTime);

    static boost::mutex ms_DataPathTransportErrorLock;
};

//
// DiskReadErrorLogger defines static function for disk read error logging rate control
//
class DiskReadErrorLogger
{
public:
    static void LogDiskReadError(const std::string& errorMsg,
        const boost::chrono::steady_clock::time_point errorMsgTime);

    static boost::mutex ms_DiskReadErrorLock;
};

//
// DatapathThrottleLogger defines static function for Datapath Throttle logging rate control
//
class DatapathThrottleLogger
{
public:
    static void LogDatapathThrottleInfo(const std::string &msg,
        const boost::chrono::steady_clock::time_point &msgTime,
        const SV_ULONGLONG &ttlsec);

    static SV_ULONGLONG s_prevLogTime;
    static SV_UINT s_datapathThrottleCount;
    static SV_ULONGLONG s_cumulativeTimeInThrottleSec;
    static boost::mutex s_datapathThrottleLock;
};

#endif // ifndef DATAPROTECTIONUTILS_H
