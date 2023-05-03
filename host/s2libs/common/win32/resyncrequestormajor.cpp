/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : resyncrequestor_port.cpp
 *
 * Description: Windows specific port of resyncrequestor.cpp
 */

#include <windows.h>
#include "devicefilter.h"
#include "hostagenthelpers.h"
#include "configurevxtransport.h"

#include <string>
#include <sstream>
#include <exception>
#include <boost/lexical_cast.hpp>

#include "transport_settings.h"
#include "configurevxagent.h"
#include "configurator2.h"
#include "resyncrequestor.h"
#include "cdputil.h"
#include "inmsafecapis.h"
#include "portablehelpersmajor.h"
#include "InmFltInterface.h"
#include "AgentHealthIssueNumbers.h"

#define INDSKFLT_ERROR_UNCLEAN_GLOBAL_SHUTDOWN_MARKER_value 0x7585
#define ERROR_TO_REG_UNCLEAN_SYS_SHUTDOWN                   INDSKFLT_ERROR_UNCLEAN_GLOBAL_SHUTDOWN_MARKER_value

#define INDSKFLT_ERROR_NO_MEMORY_value 0x7532
#define ERROR_TO_REG_NO_MEM_FOR_WORK_QUEUE_ITEM             INDSKFLT_ERROR_NO_MEMORY_value

/*
 * FUNCTION NAME : ReportResyncRequired
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
bool ResyncRequestor::ReportResyncRequired(
    Configurator*       cxConfigurator,
    std::string         volumeName,
	const SV_ULONGLONG  &ts,
	const SV_ULONG      resyncErrVal,
    char const*         reasonTxt,
    const std::string   &driverName)
{

	bool bOk = true;
	HTTP_CONNECTION_SETTINGS httpSettings = cxConfigurator->getHttpSettings();
    std::string hostId = cxConfigurator->getHostId();

    std::stringstream errMsg;
    std::string errorString;
	
    GetErrorMsg(resyncErrVal, reasonTxt, volumeName, driverName, errorString);
    
    try {
		if (INMAGE_DISK_FILTER_DOS_DEVICE_NAME != driverName)
			FormatVolumeNameForCxReporting(volumeName);

		std::string resyncTime;
		if(!CDPUtil::ToDisplayTimeOnConsole(ts, resyncTime))
		{
			DebugPrintf(SV_LOG_ERROR,
                "%s CDPUtil::ToDisplayTimeOnConsole failed for time %lld.\n",
                FUNCTION_NAME,
                ts);
            resyncTime = boost::lexical_cast<std::string>(ts);
		}

        errMsg << "id="   << hostId
               << "&vol=" << volumeName
			   << "&timestamp=" << ts ;
		
		std::stringstream dbgErrMsg;
		/* &'s are interpreted by httpd and never printed on host log.
		 * This one to store error message without &s
		 */
        dbgErrMsg << "id="   << hostId
                  << " vol=" << volumeName
                  << " timestamp=" << resyncTime ;

        if (!errorString.empty())
		{ 
            errMsg << "&err=" << errorString;
            dbgErrMsg << " err=" << errorString;
        }
        
        const std::string &resyncReasonCode = m_ResyncRequiredError.GetResyncReasonCode(resyncErrVal);
        errMsg << "&errcode=" << resyncErrVal << ". Marking resync for the device " << volumeName << " with resyncReasonCode=" << resyncReasonCode;
        dbgErrMsg << " errcode=" << resyncErrVal << ". Marking resync for the device " << volumeName << " with resyncReasonCode=" << resyncReasonCode;

        DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, dbgErrMsg.str().c_str());
        ResyncReasonStamp resyncReasonStamp;
        STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);

        cxConfigurator->getVxAgent().setSourceResyncRequired(volumeName, errMsg.str(), resyncReasonStamp);
      
    } catch (std::exception& e) {
        bOk = false;
        DebugPrintf(SV_LOG_ERROR,
            "%s exception caught: %s\n", 
            FUNCTION_NAME,
            e.what());
    }
    catch (...)
    {
        bOk = false;
        DebugPrintf(SV_LOG_ERROR,
            "%s unknown exception caught: %s\n",
            FUNCTION_NAME);
    }
    return bOk;
}


bool ResyncRequestor::ReportResyncRequired(
    std::string         volumeName,
    const SV_ULONGLONG  &ts,
    const SV_ULONG      resyncErrVal,
    char const*         reasonTxt,
    const std::string   &driverName)
{
    std::stringstream errMsg;
    std::string errorString;

    GetErrorMsg(resyncErrVal, reasonTxt, volumeName, driverName, errorString);

    std::string resyncTime;
    if (!CDPUtil::ToDisplayTimeOnConsole(ts, resyncTime))
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s CDPUtil::ToDisplayTimeOnConsole failed for time %lld.\n",
            FUNCTION_NAME,
            ts);
        resyncTime = boost::lexical_cast<std::string>(ts);
    }

    errMsg << "vol=" << volumeName
           << " timestamp=" << resyncTime
           << " errcode=" << resyncErrVal;

    if (!errorString.empty())
    {
        errMsg << " err=" << errorString;
    }

    DebugPrintf(SV_LOG_SEVERE,
        "Resync is required because: %s\n",
        errMsg.str().c_str());

    return true;
}

void ResyncRequestor::GetErrorMsg(
    unsigned long       resyncErrVal,
    char const*         reasonTxt,
    std::string         &volumeName,
    const std::string   &driverName,
    std::string         &errMsg)
{
    std::vector<char> errTxt(1024, '\0');

    if (0 == resyncErrVal) {
        inm_sprintf_s(&errTxt[0], errTxt.size(), "Driver requested resync, but error code is 0");
    }
    else if (resyncErrVal <= ERROR_TO_REG_MAX_ERROR) {
        if (0 == strlen(reasonTxt)) {
            inm_sprintf_s(&errTxt[0], errTxt.size(), "VolumeOutOfSyncErrorDescription was blank");
        }
        else {
            inm_sprintf_s(&errTxt[0], errTxt.size(), reasonTxt);
        }
    }
    else if (m_ResyncRequiredError.IsUserSpaceErrorCode(resyncErrVal)) {
        std::string serr = m_ResyncRequiredError.GetErrorMessage(resyncErrVal);
        serr += ". ";
        serr += reasonTxt;
        inm_sprintf_s(&errTxt[0], errTxt.size(), serr.c_str());
    }
    else {
        bool getDrvErrMsg = true;
        std::string deviceID = volumeName;
        if (INMAGE_DISK_FILTER_DOS_DEVICE_NAME != driverName) {
            FormatVolumeNameForMountPoint(volumeName);
            std::vector<char>   volGuid(MAX_PATH);

            if (GetVolumeNameForVolumeMountPoint(volumeName.c_str(), &volGuid[0], sizeof(volGuid)))
                deviceID = &volGuid[0];
            else {
                DWORD err = GetLastError();
                std::stringstream sserr;
                sserr << err;
                DebugPrintf(SV_LOG_ERROR,
                    "%s error: %s for api GetVolumeNameForVolumeMountPoint for device %s\n",
                    FUNCTION_NAME, sserr.str().c_str(), volumeName.c_str());
                getDrvErrMsg = false;
            }
        }

        if (getDrvErrMsg) {
            DebugPrintf(SV_LOG_DEBUG, "volume name: %s, device ID: %s.\n", volumeName.c_str(), deviceID.c_str());

            // look up the text from involflt.sys /indskflt.sys
            std::vector<char const *>   volumes(100, "");

            // manifest events start from 10001 in the indskflt driver.
            const USHORT LegacyMCEventIDStart = 0, LegacyMCEventIDEnd = 10000;
            // truncate and get the least significant 16-bits.
            USHORT resyncEvtID = (USHORT)resyncErrVal;

            int ind = 0;

            // MC events will have parameters in the format string starting from %2, while
            // manifest events will have parameters in the format string starting from %1.
            if (resyncEvtID >= LegacyMCEventIDStart && resyncEvtID <= LegacyMCEventIDEnd) {
                volumes[ind++] = ""; // %1 - MC - Dummy. Not used in formatting.
            }

            // assert that index should always start from 1 for Volume filter.
            assert(driverName == INMAGE_DISK_FILTER_DOS_DEVICE_NAME || ind == 1);

            // provide proper stucture that the driver errmsg can use to display the drive
            volumes[ind++] = volumeName.c_str(); // %2 - MC | %1 - Man
            volumes[ind++] = deviceID.c_str();   // %3 - MC | %2 - Man
            volumes[ind++] = ""; // %4 - MC | %3 - Man - Additional parameter used in few messages.

            if (!GetDriverErrorMsg(resyncErrVal, &volumes[0], &errTxt[0], errTxt.size(), driverName)) {
                inm_sprintf_s(&errTxt[0], errTxt.size(), "unable to retrieve resync error text");
            }
        }
    }

    errMsg = &errTxt[0];

    return;
}

/*
 * FUNCTION NAME : GetDriverErrorMsg
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
bool ResyncRequestor::GetDriverErrorMsg(
    unsigned long       errCode,
    char const* const*  volumes,
    char*               errTxt,
    int                 errTxtLen,
    const std::string   &driverName)
{
    bool bOk = false;
    HMODULE hModule = NULL; 

    std::vector<char>   drvString(MAX_PATH);
    std::vector<char>   systemRoot(MAX_PATH);

    if (0 == GetSystemDirectory(&systemRoot[0], systemRoot.size())) {
        DebugPrintf(SV_LOG_ERROR, "%s : GetSystemDirectrty() failed with error: %d.\n", 
                     FUNCTION_NAME, GetLastError());
        return false;
    }

    inm_sprintf_s(&drvString[0], drvString.size(), "%s\\drivers\\%s", &systemRoot[0], ((INMAGE_DISK_FILTER_DOS_DEVICE_NAME == driverName) ? "indskflt.sys" : "involflt.sys"));

    hModule = LoadLibraryEx(&drvString[0], NULL, LOAD_LIBRARY_AS_DATAFILE);

    if (NULL == hModule) {
        DebugPrintf(SV_LOG_ERROR, "%s : LoadLibraryEx() failed with error: %d.\n", 
            FUNCTION_NAME, GetLastError());
        return false;
    }

    if (0 == FormatMessage(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ARGUMENT_ARRAY,
                               hModule,
                               errCode,
                               0,
                               errTxt,
                               errTxtLen,
                               (va_list *)volumes)
    ) 
    {
        DebugPrintf(SV_LOG_ERROR, "%s : FormatMessage() failed for errorCode 0x%x with error: %d.\n", 
            FUNCTION_NAME, errCode, GetLastError());
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "%s : Resync Message: %s.\n", FUNCTION_NAME, errTxt);
        bOk = true;
    }

    FreeLibrary(hModule);
    return bOk;
}


bool ResyncRequestor::ReportPrismResyncRequired(
			Configurator* cxConfigurator,
			const std::string &sourceLunId,
			const SV_ULONGLONG ts,
			const SV_ULONGLONG resyncErrVal,
			char const* reasonTxt)
{
    /* NOTE: should not get called */
    return false;
}

std::string ResyncRequiredError::GetResyncReasonCode(const SV_ULONG &errorcode)
{
    std::string resyncReasonCode;

    switch (errorcode)
    {
    case (SV_ULONG)IO_OUT_OF_TRACKING_SIZE:
    case E_RESYNC_REQUIRED_REASON_FOR_RESIZE:
        resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::DiskResize;
        break;

    case ERROR_TO_REG_NO_MEM_FOR_WORK_QUEUE_ITEM:
    case ERROR_TO_REG_OUT_OF_MEMORY_FOR_DIRTY_BLOCKS:
        resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::LowMemory;
        break;

    case ERROR_TO_REG_UNCLEAN_SYS_SHUTDOWN:
        resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::UncleanSystemShutdown;
        break;

    default:
        resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::SourceInternalError;
        break;
    }

    return resyncReasonCode;
}
