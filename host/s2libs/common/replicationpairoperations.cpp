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


#include "transport_settings.h"
#include "configurevxagent.h"
#include "configurator2.h"
#include "replicationpairoperations.h"
#include "cdputil.h"

/*
 * FUNCTION NAME : PauseReplication
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
bool ReplicationPairOperations::PauseReplication(
			Configurator* cxConfigurator,
			std::string volumeName,
			int hostType,
			char const* reasonTxt,
			long errorCode)
{
	bool bOk = true;

	std::string hostId = cxConfigurator->getHostId();
	std::stringstream errMsg;

	try
	{		
		errMsg << "id=" << hostId << "&vol=" << volumeName.c_str() << "&hosttype=" << hostType;

		std::stringstream dbgErrMsg; 
		/* 
		 * &'s are interpreted by httpd and never printed on host log.
		 * This one to store error message without &s
		 */
		dbgErrMsg << "id=" << hostId << " vol=" << volumeName.c_str() << " hostType=" << hostType;;
	

		if (0 != reasonTxt)
		{
			errMsg << "&err=" << reasonTxt;
			dbgErrMsg << " reason=" << reasonTxt;
		}
		
		errMsg << "&errCode=" << errorCode;
		dbgErrMsg << "&errCode=" << errorCode;

		bOk = cxConfigurator->getVxAgent().pauseReplicationFromHost(volumeName, errMsg.str());

		//Make the log message as eroor to send it to the cx logs.
		DebugPrintf(SV_LOG_ERROR,"Note: Replication is paused for : %s\n", dbgErrMsg.str().c_str()); 
	}
	catch (...)
	{
		bOk = false;
		DebugPrintf("ReplicationPairOperations::PauseReplication FAILED. LINE %d in FILE %s.\n", 
			LINE_NO, FILE_NAME );
	}
	return bOk;
}

/*
 * FUNCTION NAME : ResumeReplication
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
bool ReplicationPairOperations::ResumeReplication(
			Configurator* cxConfigurator,
			std::string volumeName,
			int hostType,
			char const* reasonTxt)
{
	bool bOk = true;

	std::string hostId = cxConfigurator->getHostId();
	std::stringstream errMsg;

	try
	{		
		errMsg << "id=" << hostId << "&vol=" << volumeName.c_str()  << "&hosttype=" << hostType;			

		std::stringstream dbgErrMsg; 
		/* 
		 * &'s are interpreted by httpd and never printed on host log.
		 * This one to store error message without &s
		 */
		dbgErrMsg << "id=" << hostId << " vol=" << volumeName.c_str() << " hostType=" << hostType;
	

		if (0 != reasonTxt)
		{
			errMsg << "&err=" << reasonTxt;
			dbgErrMsg << " reason=" << reasonTxt;
		}
		


		bOk = cxConfigurator->getVxAgent().resumeReplicationFromHost(volumeName, errMsg.str());
		//Make the log message as error to send it to the cx logs.
		DebugPrintf(SV_LOG_ERROR,"Note: Replication is resumed for: %s\n", dbgErrMsg.str().c_str()); 
	}
	catch (...)
	{
		bOk = false;
		DebugPrintf("ReplicationPairOperations::ResumeReplication FAILED. LINE %d in FILE %s.\n", 
			LINE_NO, FILE_NAME );
	}
	return bOk;
}
