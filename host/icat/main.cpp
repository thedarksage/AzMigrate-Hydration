#include <iostream>

#include "transport.h"
#include "archiveobject.h"
#include "filesystemobj.h"
#include "hcapcodes.h"
#include <ace/Task.h>
#include "filelister.h"
#include "filefilter.h"
#include "configvalueobj.h"
#include "archivecontroller.h"
#include "icatexception.h"
#include "connection.h"
#include "resumetracker.h"
#include "transport.h"
#include "logger.h"
#include "configurator2.h"
#include "inmsafecapis.h"

Configurator* TheConfigurator = 0; // singleton

/*
* Function Name: main
* Arguements: 
*                   argc - Number of Args
*                   argv - Argument vector.
* Return Value:
*                  On success : 0
*                  On Failure : -1
* Description:
* Called by: None
* Calls:
*              ConfigValueObject::createInstance
*              ConfigValueObject::getInstance
*              ConfigValueObject::getTCPsendbuffer
*              ConfigValueObject::getTCPrecvbuffer
*              Curl_setsendwindowsize
*              Curl_setrecvwindowsize
*              archiveController::getInstance
*              archiveController::init
*              archiveController::run
*              archiveController::QuitSignalled
*              SetLogLevel
*              SetLogFileName
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/

int ACE_TMAIN(int argc, ACE_TCHAR** argv)
{
    init_inm_safe_c_apis();
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	int retVal = -1 ;
    try
	{
    	ConfigValueObject::createInstance(argc,argv);
	    ConfigValueObject *temp_obj = ConfigValueObject::getInstance() ;
        int sendBufferSize = temp_obj->getTCPsendbuffer();
        int recvBufferSize = temp_obj->getTCPrecvbuffer();
        if(sendBufferSize != 0)
         {
            Curl_setsendwindowsize(sendBufferSize);
        }
        if(recvBufferSize != 0)
        {
            Curl_setrecvwindowsize(recvBufferSize);
        }
        
		SetDaemonMode() ;
		SetLogLevel(temp_obj->getLogLevel());
		SetLogFileName(temp_obj->getLogFilePath().c_str()) ;
		archiveControllerPtr controller = archiveController::getInstance() ;
		DebugPrintf(SV_LOG_DEBUG, "ICAT STARTING\n") ;
		if( controller->init() )
		{
			if(!controller->QuitSignalled())
			{
				controller->run() ;
				retVal = 0 ;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Failed to initialize the controller.\n") ;
		} 
	    controller->dumpTransferStat();
    }
	catch(icatException& ie)
	{
		DebugPrintf("Caught Exception %s\n", ie.what()) ;
	}
	catch(exception& e)
	{
		DebugPrintf(SV_LOG_ERROR, "Unknown exception %s\n", e.what()) ;
	}
       
	DebugPrintf(SV_LOG_DEBUG, "ICAT EXITING\n") ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return retVal ;
}

