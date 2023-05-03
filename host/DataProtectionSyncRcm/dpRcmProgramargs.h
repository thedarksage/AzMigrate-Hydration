#ifndef DP_RCM_PROGRAMARGS_H
#define DP_RCM_PROGRAMARGS_H

#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <svtypes.h>

#include "inmageex.h"
#include "portablehelpers.h"
#include "dataprotectionexception.h"

enum AgentDirection
{
    AGENT_DIRECTION_SOURCE,
    AGENT_DIRECTION_ON_PREM_TO_AZURE_TARGET,
    AGENT_DIRECTION_AZURE_TO_ON_PREM_REPROTECT_TARGET
};

class DpRcmProgramArgs
{
public:
    explicit DpRcmProgramArgs(int argc, char* argv[])
    {
        /*
        Source:
            <InstallPath>\dataprotectionsyncrcm.exe source <source diskid>
        MT:
            <InstallPath>\dataprotectionsyncrcm.exe target <source hostid> <source diskid> <absolute path of TargetReplicationSettingsFileName>
        On-prem MT:
            <InstallPath>\DataProtectionSyncRcm.exe onpremtarget <source hostid> <source diskid> <absolute path of TargetReplicationSettingsFileName>
        */

        if((argc == 5) && (boost::iequals(argv[1], Agent_Mode_Target) || boost::iequals(argv[1], Agent_Mode_ReprotectTarget)))
        {
            if (boost::iequals(argv[1], Agent_Mode_Target))
            {
                m_AgentDirection = AGENT_DIRECTION_ON_PREM_TO_AZURE_TARGET;
            }
            else if (boost::iequals(argv[1], Agent_Mode_ReprotectTarget))
            {
                m_AgentDirection = AGENT_DIRECTION_AZURE_TO_ON_PREM_REPROTECT_TARGET;;
            }

            m_SourceHostId = argv[2];

            m_DiskId = argv[3];

            m_TargetReplicationSettingsFilePath = argv[4]; // File path which contains target replication settings

            if (m_TargetReplicationSettingsFilePath.empty())
            {
                std::stringstream sserr;
                sserr << FUNCTION_NAME << " failed: Empty config path in args. " << GetArgsStr(argc, argv) << '.';
                throw DataProtectionInvalidConfigPathException(sserr.str().c_str());
            }
            boost::system::error_code ec;
            if (!boost::filesystem::exists(m_TargetReplicationSettingsFilePath, ec))
            {
                std::stringstream sserr;
                sserr << FUNCTION_NAME << " failed: Config path in args does not exists. "
                    << "Error code=" << ec.value() << ". " << GetArgsStr(argc, argv) << '.';
                throw DataProtectionInvalidConfigPathException(sserr.str().c_str());
            }

        }
        else if ((argc == 3) && boost::iequals(Agent_Mode_Source, argv[1]))
        {
            m_AgentDirection = AGENT_DIRECTION_SOURCE;
            m_DiskId = argv[2];
        }
        else
        {
            std::stringstream sserr;
            sserr << FUNCTION_NAME << " failed: Invalid arguments: " << GetArgsStr(argc, argv) << '.';
            throw DataProtectionInvalidArgsException(sserr.str().c_str());
        }

        PrepareLogDir();
    }

    std::string GetDiskId() const
    {
        return m_DiskId;
    }

    std::string GetSourceHostId() const
    {
        return m_SourceHostId;
    }

    std::string GetLogDir() const
    {
        return m_LogDir;
    }

    std::string GetTargetReplicationSettingsFileName() const
    {
        return m_TargetReplicationSettingsFilePath;
    }

    bool IsSourceSide() const
    {
        return m_AgentDirection == AGENT_DIRECTION_SOURCE;
    }

    bool IsOnpremReplicationTargetSide() const
    {
        return m_AgentDirection == AGENT_DIRECTION_ON_PREM_TO_AZURE_TARGET;
    }

    bool IsOnpremReprotectTargetSide() const
    {
        return m_AgentDirection == AGENT_DIRECTION_AZURE_TO_ON_PREM_REPROTECT_TARGET;
    }

    static std::string GetArgsStr(int argc, char* argv[])
    {
        int cnt = 0;
        std::string args;
        while (cnt < argc) { args += argv[cnt++]; args += " "; }
        return args;
    }

private:
    void PrepareLogDir()
    {
        boost::filesystem::path logDir = Vxlogs_Folder;
        logDir /= Resync_Folder;
        if (!IsSourceSide())
        {
            logDir /= GetVolumeDirName(m_SourceHostId);
        }
        logDir /= GetVolumeDirName(m_DiskId);
        m_LogDir = logDir.string() + ACE_DIRECTORY_SEPARATOR_CHAR_A;
    }

private:
    std::string m_DiskId;
    std::string m_SourceHostId;
    std::string m_TargetReplicationSettingsFilePath; // File path which contains target replication settings
    std::string m_LogDir;
    AgentDirection m_AgentDirection;
};


#endif // DP_RCM_PROGRAMARGS_H
