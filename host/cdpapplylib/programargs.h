//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : programargs.h
//
// Description: 
//

#ifndef PROGRAMARGS_H
#define PROGRAMARGS_H

#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <svtypes.h>
#include "portablehelpers.h"

struct ProgramArgs;

struct SyncArgs {
    AGENT_RESTART_STATUS m_AgentRestartStatus;
};

struct FastOffloadSyncArgs : SyncArgs {
    FastOffloadSyncArgs() : m_SourceSide(false) { }
    bool Parse(ProgramArgs& args);

    bool m_SourceSide;

    std::string m_DeviceName;            
    /**
    * 1 TO N sync: m_EndPointDeviceName and m_GroupId added which holds target 
    *              device only in case of source
    */
    std::string m_EndPointDeviceName;            
    int m_GroupId;
};

struct DiffSyncArgs : SyncArgs {
    DiffSyncArgs() : m_Id(0), m_AgentType(0) { }
    bool Parse(ProgramArgs& args);

    int m_Id;
    int m_AgentType;

    std::vector<std::string> m_VisibleVolumes;

    std::string m_ProgramName;
    std::string m_AgentSide;
    std::string m_SyncType;
    std::string m_DiffLocation; 
    std::string m_CacheLocation;
    std::string m_SearchPattern;    
};

struct DirectSyncArgs : SyncArgs
{
	DirectSyncArgs():m_blockSize(0){}
	std::string m_srcDeviceName; 
	std::string m_trgDeviceName;
	SV_ULONG m_blockSize;
	bool b_cow;
};

struct ProgramArgs {
	ProgramArgs() : m_Argc(0), m_Argv(0), m_SourceSide(false), m_DiffSync(false), m_FastSync(0), m_DirectSync(false), m_blockSize(1024) {}
    bool Parse(int argc, char* argv[]);
    bool FastOffloadSyncArgsParse() { return m_FastOffloadSyncArgs.Parse(*this); }
    bool DiffSyncArgsParse() { return m_DiffSyncArgs.Parse(*this); }
    std::string getLogDir();

    int m_Argc;

    char** m_Argv;

    bool m_SourceSide;
    bool m_DiffSync;
    bool m_FastSync;
	bool m_DirectSync;

	SV_ULONG m_blockSize;

    FastOffloadSyncArgs m_FastOffloadSyncArgs;
	DiffSyncArgs m_DiffSyncArgs;
    DirectSyncArgs  m_directSyncArgs;
};


#endif // ifndef PROGRAMARGS__H
