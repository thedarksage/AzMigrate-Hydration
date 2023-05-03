/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 * File       : resyncrequestor_port_xspool.cpp
 * Description: XenServer specific port of resyncrequestor.cpp
 */

#include <string>
#include <sstream>
#include <exception>

#include "devicefilter.h"

#include "transport_settings.h"
#include "configurevxagent.h"
#include "configurator2.h"
#include "resyncrequestor.h"
#include "cdputil.h"
#include "inmsafecapis.h"

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
		const SV_ULONG resyncErrVal,
		char        const* reasonTxt)
{
	bool bOk = true;
	char errTxt[1024] = {0};
	SV_ULONG errTxtLen = 1024;

	std::string hostId = cxConfigurator->getHostId();
	std::stringstream errMsg;

	if (0 == resyncErrVal)
	{
		inm_strcpy_s(errTxt,ARRAYSIZE(errTxt), "Driver requested resync, but error code is 0");
	}
	else if (resyncErrVal <= ERROR_TO_REG_MAX_ERROR)
	{
		if (0 == strlen(reasonTxt))
		{
			inm_strcpy_s(errTxt, ARRAYSIZE(errTxt),"VolumeOutOfSyncErrorDescription was blank");
		}
		else
		{
			inm_strcpy_s(errTxt,ARRAYSIZE(errTxt),reasonTxt);
		}
	} 
	try
	{		
		errMsg << "id=" << hostId << "&vol=" << volumeName.c_str();			

		std::stringstream dbgErrMsg; 
		/* 
		 * &'s are interpreted by httpd and never printed on host log.
		 * This one to store error message without &s
		 */
		dbgErrMsg << "id=" << hostId << " vol=" << volumeName.c_str();
	

		if (0 != errTxt)
		{
			errMsg << "&err=" << errTxt;
			dbgErrMsg << " err=" << errTxt;
		}
		
		DebugPrintf(SV_LOG_SEVERE,"Resync is required because: %s\n", dbgErrMsg.str().c_str()); 
		//Make the log message as severe to send it to the cx logs.
		cxConfigurator->getVxAgent().setXsPoolSourceResyncRequired(volumeName,errMsg.str());		
	}
	catch (...)
	{
		bOk = false;
		DebugPrintf("ResyncRequestor::ReportResyncRequired FAILED. LINE %d in FILE %s.\n", 
			LINE_NO, FILE_NAME );
	}
	return bOk;
}
