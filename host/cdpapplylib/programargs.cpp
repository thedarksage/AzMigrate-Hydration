//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : programargs.cpp
//
// Description: 
//

#include <boost/lexical_cast.hpp>

#include "programargs.h"

bool ProgramArgs::Parse(int argc, char* argv[])
{    
    m_Argc = argc;
    m_Argv = argv;

    if (argc < 3) {
        return false;
    }

    if (0 == strcmp(argv[1], "target")) {                
        m_SourceSide = false;
    } else if (0 == strcmp(argv[1], "source")) {                
        m_SourceSide = true;        
    } else if (0 == strcmp(argv[1], "directsync")){
		m_DirectSync = true;
	}
	else{
        return false;       
    }

	if(!m_DirectSync)
	{
		if (0 == strcmp(argv[2], "diff")) {
			m_DiffSync = true;        
			return DiffSyncArgsParse();
		} else { 
			if (0 == strcmp(argv[2], "fast")) {
				m_FastSync = true;
				m_DiffSync = false;
			} else if (0 == strcmp(argv[2], "offload")) {
				m_FastSync = false;
				m_DiffSync = false;
			} else {
				return false;
			}

			return FastOffloadSyncArgsParse();
		}
	}
	else
	{
		if( argc < 4 )
		{
			return false;
		}
		m_directSyncArgs.m_srcDeviceName = argv[2];
		m_directSyncArgs.m_trgDeviceName = argv[3];
		if(argc > 4)
		{
			m_directSyncArgs.m_blockSize = ACE_OS::atoi(argv[4]);
		}
		if(m_directSyncArgs.m_blockSize == 0)
			m_directSyncArgs.m_blockSize = 1024;
		return true;
	}

    return false;  
}
    
bool FastOffloadSyncArgs::Parse(ProgramArgs& args)
{
    bool bretval = false;
    /**
    * 1 TO N sync: source has 6 args now
    *              target still has 4 args
    */
    //TODO: Add hash defines for all hard coded values
    m_SourceSide = args.m_SourceSide;
    int argc;

    if (m_SourceSide)
    {
        argc = 7;
        if (!args.m_FastSync && !args.m_DirectSync)
        {
            argc = 4;
        }

        if (argc == args.m_Argc)
        {
            m_DeviceName = args.m_Argv[3];
            if (7 == args.m_Argc)
            {
                m_EndPointDeviceName = args.m_Argv[4];
                m_GroupId = atoi(args.m_Argv[5]);
                m_AgentRestartStatus = (AGENT_RESTART_STATUS)atoi(args.m_Argv[6]);
            }
            bretval = true;
        }
    }
    else
    {
        argc = 5;
        if (argc == args.m_Argc)
        {
            m_DeviceName = args.m_Argv[3];
            m_AgentRestartStatus = (AGENT_RESTART_STATUS)atoi(args.m_Argv[4]);
            bretval = true;
        }
    }
 
    return bretval; 
}

bool DiffSyncArgs::Parse(ProgramArgs& args)
{   
    if (9 != args.m_Argc) {
        return false;
    }

    m_ProgramName    = args.m_Argv[0];
    m_AgentSide      = args.m_Argv[1];
    m_SyncType       = args.m_Argv[2];
    m_DiffLocation   = args.m_Argv[3];
    m_CacheLocation  = args.m_Argv[4];
    m_Id             = boost::lexical_cast<int>(args.m_Argv[5]);  
    m_SearchPattern  = args.m_Argv[6];
    m_AgentType      = boost::lexical_cast<int>(args.m_Argv[7]);

    //visible drives
    std::string vDrives  = args.m_Argv[8];
    std::string delimiters = ";";

    std::string::size_type nextPos = vDrives.find_first_not_of(delimiters);
    std::string::size_type pos     = vDrives.find_first_of(delimiters);

    while (std::string::npos != pos || std::string::npos != nextPos)
    {
        m_VisibleVolumes.push_back(vDrives.substr(nextPos, pos - nextPos));
        nextPos = vDrives.find_first_not_of(delimiters, pos);
        pos = vDrives.find_first_of(delimiters, nextPos);
    }

    return true;
}

std::string ProgramArgs::getLogDir()
{
    std::string logDir = Vxlogs_Folder;

    if(m_DiffSync) {
        logDir += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
        logDir += Diffsync_Folder;
        logDir += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
        logDir += boost::lexical_cast<std::string>(m_DiffSyncArgs.m_Id);
        logDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    } else {
        logDir += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
        logDir += Resync_Folder;
        logDir += ACE_DIRECTORY_SEPARATOR_CHAR_A ;

        /**
        * 1 TO N sync: If directsync is not there,
        * then:
        * 1. For source, logDir is source\volgrpid\target\
        * 2. For target, still target\          
        * else
        * for direct sync, always source\target
        */

	    if(!m_DirectSync)
        {
		    logDir += GetVolumeDirName(m_FastOffloadSyncArgs.m_DeviceName);
            if (m_SourceSide && m_FastSync)
            {
                logDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
                std::stringstream strgrpid;
                strgrpid << m_FastOffloadSyncArgs.m_GroupId;
                logDir += strgrpid.str();
                logDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
                logDir += GetVolumeDirName(m_FastOffloadSyncArgs.m_EndPointDeviceName);
            }
        }
	    else
        {
		    logDir += GetVolumeDirName(m_directSyncArgs.m_srcDeviceName);
            logDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            logDir += GetVolumeDirName(m_directSyncArgs.m_trgDeviceName);
        }
        logDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    }

    return logDir;
}
