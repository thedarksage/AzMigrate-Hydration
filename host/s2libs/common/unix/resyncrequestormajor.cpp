/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : resyncrequestor_port.cpp
 *
 * Description: Linux specific port of resyncrequestor.cpp
 */

#include <string>
#include <sstream>
#include <exception>
#include <boost/lexical_cast.hpp>

#include "devicefilter.h"

#include "transport_settings.h"
#include "configurevxagent.h"
#include "configurator2.h"
#include "resyncrequestor.h"
#include "cdputil.h"
#include "inmageex.h"
#include "AgentHealthIssueNumbers.h"
#include "involflt.h"

static const SV_ULONGLONG NumHundredNanoSecBetweenUnixAndLdapEpoch = 116444736000000000ULL;

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
		Configurator*   cxConfigurator,
		std::string       volumeName,
		const SV_ULONGLONG & ts,
		const SV_ULONG resyncErrVal,
		char        const* reasonTxt,
        const std::string &driverName)
{
	bool bOk = true;
	SV_ULONG errTxtLen = 1024;

	std::string hostId = cxConfigurator->getHostId();
	std::stringstream errMsg;

	try
	{
		std::string resyncTime;
        SV_ULONGLONG unixtime = ts + NumHundredNanoSecBetweenUnixAndLdapEpoch;
		if(!CDPUtil::ToDisplayTimeOnConsole(unixtime, resyncTime))
		{
			DebugPrintf(SV_LOG_ERROR,
                "%s : CDPUtil::ToDisplayTimeOnConsole failed for time %lld.\n",
                FUNCTION_NAME,
                unixtime);

            resyncTime = boost::lexical_cast<std::string>(unixtime);
		}
		errMsg << "id="   << hostId
			<< "&vol=" << volumeName
			<< "&timestamp=" << ts ;

		std::stringstream dbgErrMsg; 
		/* 
		 * &'s are interpreted by httpd and never printed on host log.
		 * This one to store error message without &s
		 */
		dbgErrMsg << "id="   << hostId
			      << " vol=" << volumeName
				  << " timestamp=" << resyncTime ;

		errMsg << "&err=";
		dbgErrMsg << " err=";

		std::string serr;
		if (m_ResyncRequiredError.IsUserSpaceErrorCode(resyncErrVal)) {
			serr = m_ResyncRequiredError.GetErrorMessage(resyncErrVal);
			serr += ". ";
			serr += reasonTxt;
		} else
			serr = reasonTxt;

		errMsg << serr;
		dbgErrMsg << serr;
        const std::string &resyncReasonCode = m_ResyncRequiredError.GetResyncReasonCode(resyncErrVal);
		errMsg << "&errcode=" << resyncErrVal
            << ". Marking resync for the device " << volumeName << " with resyncReasonCode=" << resyncReasonCode;
		dbgErrMsg << " errcode=" << resyncErrVal
            << ". Marking resync for the device " << volumeName << " with resyncReasonCode=" << resyncReasonCode;
		
		DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, dbgErrMsg.str().c_str());
        ResyncReasonStamp resyncReasonStamp;
        STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);

		cxConfigurator->getVxAgent().setSourceResyncRequired(volumeName, errMsg.str(), resyncReasonStamp);
		
	}
    catch( ContextualException& exception )
    {
		bOk = false;
        DebugPrintf( SV_LOG_ERROR,
                     "Failed to update source resync required. Call failed with: %s\n",
                     exception.what() ) ;
    }
	catch (...)
	{
		bOk = false;
		DebugPrintf("ResyncRequestor::ReportResyncRequired FAILED. LINE %d in FILE %s.\n", 
			LINE_NO, FILE_NAME );
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

    std::string resyncTime;
    SV_ULONGLONG unixtime = ts + NumHundredNanoSecBetweenUnixAndLdapEpoch;
    if (!CDPUtil::ToDisplayTimeOnConsole(unixtime, resyncTime))
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s CDPUtil::ToDisplayTimeOnConsole failed for time %lld.\n",
            FUNCTION_NAME,
            unixtime);

        resyncTime = boost::lexical_cast<std::string>(unixtime);
    }

    errMsg << "vol=" << volumeName
        << " timestamp=" << resyncTime
        << " err=";

    std::string serr;
    if (m_ResyncRequiredError.IsUserSpaceErrorCode(resyncErrVal)) {
        serr = m_ResyncRequiredError.GetErrorMessage(resyncErrVal);
        serr += ". ";
        serr += reasonTxt;
    }
    else
        serr = reasonTxt;

    errMsg << serr;
    errMsg << " errcode=" << resyncErrVal;

    DebugPrintf(SV_LOG_SEVERE, "Resync is required because: %s\n", errMsg.str().c_str());

    return true;
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
        DebugPrintf(SV_LOG_ERROR,
            "%s: There is no need for this function in Linux.\n",
            FUNCTION_NAME);
        return true;
}


void ResyncRequestor::GetErrorMsg(
    unsigned long       resyncErrVal,
    char const*         reasonTxt,
    std::string         &volumeName,
    const std::string   &driverName,
    std::string         &errMsg
    )
{
    DebugPrintf(SV_LOG_ERROR,
        "%s: There is no need for this function in Linux.\n",
        FUNCTION_NAME);
    return;
}

/*
 * FUNCTION NAME : ReportPrismResyncRequired
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
bool ResyncRequestor::ReportPrismResyncRequired(
			Configurator* cxConfigurator,
			const std::string &sourceLunId,
			const SV_ULONGLONG ts,
			const SV_ULONGLONG resyncErrVal,
			char const* reasonTxt)
{
	bool bOk = true;
	SV_ULONG errTxtLen = 1024;

	std::string hostId = cxConfigurator->getHostId();
	std::stringstream errMsg;

	try
	{
		std::string resyncTime;
        SV_ULONGLONG unixtime = ts + NumHundredNanoSecBetweenUnixAndLdapEpoch;
		if(!CDPUtil::ToDisplayTimeOnConsole(unixtime, resyncTime))
		{
			DebugPrintf(SV_LOG_ERROR,
                "%s : CDPUtil::ToDisplayTimeOnConsole failed for time %lld.\n",
                FUNCTION_NAME,
                unixtime);
            resyncTime = boost::lexical_cast<std::string>(unixtime);
		}

		errMsg << "id="   << hostId
			<< "&sourcelunid=" << sourceLunId
			<< "&timestamp=" << ts ;

		std::stringstream dbgErrMsg; 
		/* 
		 * &'s are interpreted by httpd and never printed on host log.
		 * This one to store error message without &s
		 */
		dbgErrMsg << "id="   << hostId
			      << " sourcelunid=" << sourceLunId
				  << " timestamp=" << resyncTime ;

		errMsg << "&err=" << reasonTxt;
		dbgErrMsg << " err=" << reasonTxt;
		errMsg << "&errcode=" << resyncErrVal;
		dbgErrMsg << " errcode=" << resyncErrVal;
		
		DebugPrintf(SV_LOG_SEVERE,"Resync is required because: %s\n", dbgErrMsg.str().c_str()); 
		//Make the log message as severe to send it to the cx logs.
		bOk = cxConfigurator->getVxAgent().setPrismResyncRequired(sourceLunId, errMsg.str());
        if (bOk)
        {
            DebugPrintf(SV_LOG_DEBUG, "successfully reported resync required message: %s\n", dbgErrMsg.str().c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "cx has returned update failure for resync required message: %s\n", dbgErrMsg.str().c_str());
        }
	}
    catch( ContextualException& exception )
    {
		bOk = false;
        DebugPrintf( SV_LOG_ERROR,
                     "Failed to update prism resync required. Call failed with: %s\n",
                     exception.what() ) ;
    }
	catch (...)
	{
		bOk = false;
		DebugPrintf("ResyncRequestor::ReportPrismResyncRequired FAILED. LINE %d in FILE %s.\n", 
			LINE_NO, FILE_NAME );
	}
	return bOk;
}

std::string ResyncRequiredError::GetResyncReasonCode(const SV_ULONG &errorcode)
{
    std::string resyncReasonCode;

    switch (errorcode)
    {
    case ERROR_TO_REG_OUT_OF_BOUND_IO:
    case E_RESYNC_REQUIRED_REASON_FOR_RESIZE:
        resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::DiskResize;
        break;

    case ERROR_TO_REG_NO_MEM_FOR_WORK_QUEUE_ITEM:
    case ERROR_TO_REG_FAILED_TO_ALLOC_BIOINFO:
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
