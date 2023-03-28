#include <iostream>
#include <fstream>

#include "consistencythread-cs.h"
#include "configwrapper.h"
#include "logger.h"
#include "localconfigurator.h"
#include "inmalertdefs.h"
#include "host.h"
#include "svutils.h"
#include "VacpConfDefs.h"

#ifdef SV_WINDOWS
#include "hydrationconfigurator.h"
#endif

#include <boost/chrono.hpp>
#include <boost/tokenizer.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

ConsistencyThreadCS::ConsistencyThreadCS(Configurator* configurator) :
    m_configurator(configurator)
{
}

ConsistencyThreadCS::~ConsistencyThreadCS()
{
}

SVSTATUS ConsistencyThreadCS::GetProtectedDeviceIds(std::set<std::string> &deviceIds) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    SVSTATUS status = SVS_OK;
    InitialSettings settings;
    HOST_VOLUME_GROUP_SETTINGS volGroups;

    volGroups = m_configurator->getHostVolumeGroupSettings();

    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_constiterator cIterVolGrp = volGroups.volumeGroups.begin();
    for (/*empty*/; cIterVolGrp != volGroups.volumeGroups.end(); cIterVolGrp++)
    {
        if (cIterVolGrp->direction == SOURCE)
        {
            VOLUME_GROUP_SETTINGS::volumes_constiterator cIterVol = cIterVolGrp->volumes.begin();
            for (/*empty*/; cIterVol != cIterVolGrp->volumes.end(); ++cIterVol)
            {
                deviceIds.insert(cIterVol->first);
            }
        }
    }

    if (deviceIds.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "%s: no protected disks discovered.\n", FUNCTION_NAME);
        status = SVE_FAIL;
        return status;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}

void ConsistencyThreadCS::GetConsistencyIntervals(uint64_t &crashInterval, uint64_t &appInterval)
{
    boost::chrono::steady_clock::time_point currentTime = boost::chrono::steady_clock::now();
    if (currentTime > m_prevSettingsFetchTime + boost::chrono::seconds(CONSISTENCY_SETTINGS_FETCH_INTERVAL))
    {
        // TODO 1809 - A-A, V-A
        // Participating nodes and schedule interval should be provided by vacp in vacp consistency log only
        // This is required because settings may have changed in a window by time this function is called and actual tag inserted.
        try
        {
            LocalConfigurator lc; // TODO: make member
            const std::string csCacheFile(lc.getConsistencySettingsCachePath());
            boost::system::error_code ec;
            if (boost::filesystem::exists(csCacheFile, ec))
            {

                std::string csCacheLockFile(csCacheFile);
                csCacheLockFile += ".lck";
                if (!boost::filesystem::exists(csCacheLockFile, ec))
                {
                    std::ofstream tmpLockFile(csCacheLockFile.c_str());
                    tmpLockFile.close();
                }
                boost::interprocess::file_lock fileLock(csCacheLockFile.c_str());
                boost::interprocess::scoped_lock<boost::interprocess::file_lock> fileLockGuard(fileLock);
                std::ifstream hcsInCacheFile(csCacheFile.c_str());
                std::string strcsInCacheFile((std::istreambuf_iterator<char>(hcsInCacheFile)), (std::istreambuf_iterator<char>()));
                hcsInCacheFile.close();
                m_consistencySettings = JSON::consumer<CS::ConsistencySettings>::convert(strcsInCacheFile);
                m_prevSettingsFetchTime = boost::chrono::steady_clock::now();
            }
            else
            {
                DebugPrintf(SV_LOG_ALWAYS, "%s: ConsistencySettings cached file \"%s\" does not exist. Error code: %d\n", FUNCTION_NAME, csCacheFile.c_str(), ec.value());
            }
        }
        catch (const std::exception &e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: caught an exception %s\n", FUNCTION_NAME, e.what());
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: caught an unknown exception\n", FUNCTION_NAME);
        }
    }
    else
    {
        // TODO 1809
        // This is unexpected behaviour
        // Empty values for interval and participating nodes will be inserted
        // Should get rid of this dependency with logging required info in vacp log itself as mentioned above
    }

    crashInterval = m_consistencySettings.m_CrashConsistencyInterval;

    appInterval = m_consistencySettings.m_AppConsistencyInterval;
}

bool ConsistencyThreadCS::IsMasterNode() const
{
    const std::string strMasterNode = GetValueForPropertyKey(m_consistencySettings.m_CmdOptions, OPT_MASTER_NODE);
    if (!strMasterNode.empty())
    {
        // Multi-vm case
        return (boost::iequals(strMasterNode, Host::GetInstance().GetIPAddress()) ||
            boost::iequals(strMasterNode, Host::GetInstance().GetHostName()));
    }
    
    // Single-vm case
    return true;
}

void ConsistencyThreadCS::SendAlert(InmAlert &inmAlert) const
{
    SendAlertToCx(SV_ALERT_SIMPLE, SV_ALERT_MODULE_CONSISTENCY, inmAlert);
}

void ConsistencyThreadCS::AddDiskInfoInTagStatus(const TagTelemetry::TagType &tagType,
    const std::string &cmdOutput,
    TagTelemetry::TagStatus &tagStatus) const
{
    std::set<std::string> deviceIds;
    GetProtectedDeviceIds(deviceIds);
    ADD_INT_TO_MAP(tagStatus, TagTelemetry::NUMOFDISKINPOLICY, deviceIds.size());

    std::string diskList;
    std::set<std::string>::const_iterator citr = deviceIds.begin();
    for (/*empty*/; citr != deviceIds.end(); citr++)
    {
        diskList += *citr;
        diskList += ",";
    }

    ADD_STRING_TO_MAP(tagStatus, TagTelemetry::DISKIDLISTINPOLICY, diskList);
}

uint32_t ConsistencyThreadCS::GetNoOfNodes()
{
    uint32_t NoOfNodes = 1; // Default 1 count for master node
    const std::string strClientNodes = GetValueForPropertyKey(m_consistencySettings.m_CmdOptions, OPT_CLIENT_NODE);
    if (!strClientNodes.empty())
    {
        boost::char_separator<char> sep(",");
        typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;
        tokenizer_t cnTokens(strClientNodes, sep);

        for (tokenizer_t::const_iterator cpmtItr(cnTokens.begin()); cpmtItr != cnTokens.end(); cpmtItr++)
        {
            NoOfNodes++;
        }
    }

    return NoOfNodes;
}

bool ConsistencyThreadCS::IsTagFailureHealthIssueRequired() const
{ 
    //Not needed in legacy
    return false;
}