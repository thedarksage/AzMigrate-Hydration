#include <cstring>
#include <iostream>
#include <string>
#include <list>
#include <sstream>
#include <exception>
#include <cstdlib>
#include <cmath>
#include <sstream>
#include <fstream>
#include "ace/ACE.h"
#include "ace/Process_Manager.h"
#include "boost/shared_ptr.hpp"
#include "boost/shared_array.hpp"
#include "devicefilter.h"
#include "entity.h"
#include "error.h"
#include "portable.h"
#include "svtypes.h"
#include "globs.h"
#include "s2libsthread.h"
#include "runnable.h"
#include "event.h"
#include "devicestream.h"
#include "resyncrequestor.h"
#include "portableheaders.h"
#include "svdparse.h"
#include "prismvolume.h"
#include "configurator2.h"
#include "configurevxagent.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"

////////////////////
//
extern Configurator* TheConfigurator;


/*
 * FUNCTION NAME :  WakeUpThreads
 *
 * DESCRIPTION :  TODO: 
 *                sets the resync event; but we should never report this; This should
 *                make the threads time out prematurely but currently this is false 
 *                resync and we should check quit event and quit before reporting any
 *                resync; This needs further design cleanups
 *
 * INPUT PARAMETERS :  NONE
 *                    
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS/SV_FAILURE
 *
 */
int PrismVolume::WakeUpThreads(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    int iStatus = SV_SUCCESS;

    /*
    if ( IsProtecting() )
    {
         * TODO: If we are signalling this, 
         * we should first check should quit
         * since this is set even though resync 
         * is not required; This is like 
         * waking up all threads
        m_ResyncEvent.Signal(true); 
    }
    */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    return iStatus;
}


/*
 * FUNCTION NAME :  RegisterEventsWithDriver
 *
 * DESCRIPTION :    Registers resync required event with driver
 *
 * INPUT PARAMETERS :  NONE
 *
 * OUTPUT PARAMETERS :  NONE
 *
 * NOTES :
 *
 * return value : return SV_SUCCESS if registration succeeds else SV_FAILURE
 *
 */
int PrismVolume::RegisterEventsWithDriver(void)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    int iStatus = SV_SUCCESS;

    /* Register m_ResyncEvent with driver once
     * driver structure is ready */

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME,GetSourceLUNID().c_str());
    return iStatus;
}


bool PrismVolume::DoScsiIDsMatchTheGiven(const std::list<std::string> &devices, const std::string &refid)
{
    /* do not know currently */
    return false;
}


int PrismVolume::WaitForMirrorEvent(void)
{
    return SV_FAILURE;
}

