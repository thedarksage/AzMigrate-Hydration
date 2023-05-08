#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>
#include "portablehelpers.h"
#include "logger.h"
#include "globalHealthCollator.h"
#include "inmageex.h"

HealthCollatorPtr DataProtectionGlobalHealthCollator::m_healthCollatorPtr;
boost::mutex DataProtectionGlobalHealthCollator::m_healthCollatorPtrInstanceMutex;
std::string DataProtectionGlobalHealthCollator::m_diskId;
std::string DataProtectionGlobalHealthCollator::m_healthCollatorDirPath;
std::string DataProtectionGlobalHealthCollator::m_hostId;

DataProtectionGlobalHealthCollator::DataProtectionGlobalHealthCollator(const std::string& diskId, const std::string& healthCollatorDirPath,
    const std::string& hostId)  
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    m_diskId = diskId;
    m_healthCollatorDirPath = healthCollatorDirPath;
    m_hostId = hostId;
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void DataProtectionGlobalHealthCollator::init()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    if (m_diskId.empty() || m_healthCollatorDirPath.empty() || m_hostId.empty())
    {
        DebugPrintf(SV_LOG_ERROR, "%s global health collator members are not initialised, insufficient information available to initialize the DataProtectionHealthCollator.\n",
            FUNCTION_NAME);
        throw INMAGE_EX("Global health collator members are not initialised, insufficient information available to initialize the DataProtectionHealthCollator.");
    }
    boost::replace_all(m_diskId, "/", "_");
    boost::filesystem::path dpHealthIssuesFilePath(m_healthCollatorDirPath);
    dpHealthIssuesFilePath /= NSHealthCollator::DPPrefix + NSHealthCollator::UnderScore +
        m_hostId + NSHealthCollator::UnderScore +
        m_diskId + NSHealthCollator::UnderScore  + "global" + NSHealthCollator::ExtJson;
 
    m_healthCollatorPtr.reset(new HealthCollator(dpHealthIssuesFilePath.string()));

    if (!m_healthCollatorPtr)
    {
        throw INMAGE_EX("Failed to create global healthCollator instance.");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

HealthCollatorPtr DataProtectionGlobalHealthCollator::getInstance()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    if (m_healthCollatorPtr == NULL)
    {
        try {
            boost::unique_lock<boost::mutex> lock(m_healthCollatorPtrInstanceMutex);
            if (m_healthCollatorPtr == NULL)
            {
                init();
            }
        }
        catch (const std::exception& e)
        {
            DebugPrintf(SV_LOG_ERROR, "%s failed with an exception : %s.\n", FUNCTION_NAME, e.what());
        }
        catch (...)
        {
            DebugPrintf(SV_LOG_ERROR, "%s failed with an unnknown exception.\n", FUNCTION_NAME);
        }
    }
    
    if (m_healthCollatorPtr == NULL)
    {
        throw INMAGE_EX("Failed to create global healthCollator instance.");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return m_healthCollatorPtr;
}
