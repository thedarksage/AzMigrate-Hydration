#include <sstream>
#include "portable.h"
#include "portablehelpers.h"
#include "AgentProcess.h"
#include "event.h"
#include <ace/OS_NS_sys_wait.h>
#include "servicemajor.h"

//
// PR # 6091
// Routine declaration for deleting any stale quit event files
//
extern void DeleteAgentQuitEventFiles( std::string const& filename );

// --------------------------------------------------------------------------
// AgentId
// --------------------------------------------------------------------------
bool AgentId::Less(AgentId  & rhs) const 
{
	/**
	* 1 TO N sync: 
    *             
    * LOGIC IS: when device id match, which will match every 3  mins
    *           since service executes this every 3 mins, 
    *           if our end point is empty (or) our end point is not empty and end point matches,
    *           then
    *               compare the group ID
    *           else
    *               compare the end points
    * We are safe here since device will again match for source dataprotection forks only.
    * and we are sure in this case m_endpointDeviceId is not empty
    * In a case let us say CacheMgr has come, device ID will match with CacheMgr but 
    * m_endpointDeviceId with be empty and group gets compared which is what we want 
	*/
    if (m_deviceId == rhs.Id()) 
    {
        if (m_endpointDeviceId.empty() || (m_endpointDeviceId == rhs.EndPointId()))
        {
            return (m_Group < rhs.m_Group);
        }
        else
        {
            return (m_endpointDeviceId < rhs.EndPointId());
        }
    } 
    else 
    {
        return (m_deviceId < rhs.Id());
    }
} 

// --------------------------------------------------------------------------
// AgentProcess
// --------------------------------------------------------------------------
int AgentProcess::PostQuitMessage()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	//Signal Agent's quitEvent
	int iRet = 0;

	if (m_ProcessId != ACE_INVALID_PID)
    {
		std::stringstream pid;
        pid << m_ProcessId;

		if ( NULL == m_quitEvent ) 
		{
			/* first time, signal quit immediately */
			DebugPrintf(SV_LOG_DEBUG, "Signalling event for Process with PID:%s\n", pid.str().c_str());
		    CreateQuitEvent();
			if (0 == m_quitEvent->signal())
                DebugPrintf(SV_LOG_DEBUG, "Process %s, signalled successfully\n", pid.str().c_str());
            else 
			{
                DebugPrintf(SV_LOG_ERROR, "Failed to signal process %s.\n", pid.str().c_str());
                iRet = -1;
            }
		} 
		else 
		{
			const long int TIME_TO_WAIT_FORQUITEVENT_INSECS = 1;
			ACE_Time_Value time_value;
			time_value.set(TIME_TO_WAIT_FORQUITEVENT_INSECS, 0);
			if (0 == m_quitEvent->wait(&time_value, 0))
			{
				DebugPrintf(SV_LOG_DEBUG, "Quit Event for process %s already signalled.\n", pid.str().c_str());
			}
			else if (ETIME == errno)
			{
				DebugPrintf(SV_LOG_DEBUG, "Quit Event was not signalled properly earlier for process %s, hence re-signalling the quit\n", pid.str().c_str());
				if (0 == m_quitEvent->signal())
				{
					DebugPrintf(SV_LOG_DEBUG, "Process %s, re-signalled successfully\n", pid.str().c_str());
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to re-signal process %s.\n", pid.str().c_str());
					iRet = -1;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "For process %s, Wait on quit event failed. Service state is not good\n", pid.str().c_str());
				iRet = -1;
			}
		}
	}
	else
	{
		/* TODO: print any name or something other than pid ? */
		DebugPrintf(SV_LOG_ERROR, "For posting quit event to child process, pid is invalid. This should not happen.\n");
		iRet = -1;
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	
	return iRet;
}

bool AgentProcess::Start(ACE_Process_Manager *pm, const char * exePath, const char * args)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s with %s %s\n", FUNCTION_NAME, exePath, args);

	ACE_Process_Options options; 

	options.handle_inheritance(false);

	options.command_line ("\"%s\" %s", exePath, args);

#ifdef SV_UNIX
    int flags = 0;
    for (int i = 0 ; i < getdtablesize(); ++i) 
    {
        if ((flags = fcntl(i, F_GETFD)) != -1)
            fcntl(i, F_SETFD, flags | FD_CLOEXEC);
    }
#endif

    m_ProcessId = pm->spawn (options);
    if ( m_ProcessId != ACE_INVALID_PID) 
    {
        isAlive = true;

		//Assigning each child process of svagents to JobObject so that when svagents crash/exits all the child process will be terminated	
		#ifdef SV_WINDOWS
		if (SERVICE::instance()->AssignChildProcessToJobObject(m_ProcessId) != true)
		{
			DebugPrintf(SV_LOG_ERROR, "AgentProcess::Start AssignChildProcessToJobObject() failed for the process-Id =%ld ,exepath = %s and args = %s \n", m_ProcessId, exePath, args);
		}
		#endif
		
    }
    else
    {        
        DebugPrintf(SV_LOG_ERROR, "FAILED AgentProcess::Start CreateProcess %s args %s: %d\n", exePath, args, ACE_OS::last_error());
        return false;
    }
    return true;
}


bool AgentProcess::Stop(ACE_Process_Manager *pm)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bStopped = false;
    if (0 == pm->terminate(m_ProcessId))
    {
        DebugPrintf(SV_LOG_ALWAYS,
            "%s: stopped process %s with pid %d\n", 
            FUNCTION_NAME,
            DeviceName().c_str(),
            m_ProcessId);
        bStopped = true;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: failed to stop process %s with pid %d\n",
            FUNCTION_NAME,
            DeviceName().c_str(),
            m_ProcessId);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bStopped;
}


/*
 * FUNCTION NAME :     AgentQuitEventName
 *
 * DESCRIPTION :   utility routine to return the quitevent file name based on process id
 *
 * INPUT PARAMETERS : process id 
 *
 * OUTPUT PARAMETERS :  none
 *
 * NOTES : 
 *
 * return value : quit event file name
 *
*/
static std::string AgentQuitEventName( int processId )
{
    std::stringstream ssEventName;
    ssEventName << AGENT_QUITEVENT_PREFIX << processId;
    return ssEventName.str();
}

AgentProcess::~AgentProcess()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DestroyQuitEvent();

    //
    // PR #6091
    // delete any stale quit event files
    // Agents create quit events, which are named events visible across processes. 
    // On Unix, this is implemented with named events. 
    // These events create memory mapped files that contain the event state. 
    // The events contain mutexes. When dataprotection crashes or is terminated, 
    // these files are left on disk, possibly in a locked or invalid state. 
    // A theory, unproven, is this may be why some dataprotection instances have been
    // observed to stop responding on exit. Just in case, when an Agent Process exits, we look for 
    // its quit event file after we've deleted our quit event instance. 
    // The file should not exist if a nice cleanup occurred. 
    // But if a file is left, then we delete it. 
    // This, in addition to deleting such files on start-up, should prevent their stale
    // state from being re-used.
    //
    DeleteAgentQuitEventFiles( AgentQuitEventName( m_ProcessId ) );
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    
}

void AgentProcess::CreateQuitEvent()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    if ( m_ProcessId != ACE_INVALID_PID) 
    {
        DestroyQuitEvent();
		// PR#10815: Long Path support
        m_quitEvent = new ACE_Manual_Event(0, USYNC_PROCESS,
								ACE_TEXT_CHAR_TO_TCHAR(AgentQuitEventName( m_ProcessId ).c_str()) );
    }
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    
}

void AgentProcess::DestroyQuitEvent()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	if (NULL != m_quitEvent)
	{
		delete m_quitEvent;
		m_quitEvent = NULL;
	}

    m_quitEvent = NULL;
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    
}

