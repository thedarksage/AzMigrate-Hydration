// InmageAgent.cpp : Defines the exported functions for the DLL application.
//

#include "ReplicationManager.h"

boost::mutex            ReplicationManager::s_instanceLock;
volatile SV_LONG        ReplicationManager::s_numInstances = 0;
ReplicationManager*     ReplicationManager::s_pInstance = NULL;

// This is the constructor of a class that has been exported.
// see ReplicationManager.h for the class definition
#ifdef SV_WINDOWS
ReplicationManager::ReplicationManager(std::string logfile,
    IPlatformAPIs* platformAPIs, CInmageIoctlController* pIoctlController) :
m_ioctlController(pIoctlController)
{
    m_pLogger.reset(new CLogger(logfile));
    m_pLogger->LogInfo("Entering: %s\n", __FUNCTION__);
    m_platformAPIs = platformAPIs;
    m_pLogger->LogInfo("Exiting: %s\n", __FUNCTION__);
}
#else
ReplicationManager::ReplicationManager(std::string logfile,
    IPlatformAPIs* platformAPIs, CInmageIoctlController* pIoctlController)
{
    ILogger* logger = new CLogger(logfile);
    m_pLogger.reset(logger);
    m_platformAPIs = platformAPIs;
    m_pLogger->LogInfo("Creating the InmageIoctlController object for S2...\n");
    m_ioctlController = new CInmageIoctlController(m_platformAPIs, logger);
    m_pLogger->LogInfo("Exiting: %s\n", __FUNCTION__);
}
#endif

ReplicationManager* ReplicationManager::GetInstance(string logfile,
    IPlatformAPIs* platformAPIs, CInmageIoctlController* pIoctlController)
{
    if (s_numInstances)
    {
        s_instanceLock.lock();
        ++s_numInstances;
        s_instanceLock.unlock();
        return s_pInstance;
    }

    s_instanceLock.lock();
    if (0 == s_numInstances)
    {
#ifdef SV_WINDOWS
        s_pInstance = new ReplicationManager(logfile, platformAPIs, pIoctlController);
#else
        s_pInstance = new ReplicationManager(logfile, platformAPIs, NULL);
#endif
    }
    ++s_numInstances;
    s_instanceLock.unlock();

    return s_pInstance;
}

ReplicationManager::~ReplicationManager()
{
#ifdef SV_UNIX
    delete m_ioctlController;
#endif
    m_pLogger->LogInfo("Entering: %s\n", __FUNCTION__);
    m_pLogger->Flush();
}

void ReplicationManager::Release()
{
    s_instanceLock.lock();
    --s_numInstances;
    if (s_numInstances <= 0)
    {
        delete s_pInstance;
    }
    s_instanceLock.unlock();
}

void* ReplicationManager::SetFiltering(IBlockDevice* sourceDevice,
    IDataProcessor *pDataProcessor, ILogger *logger, CInmageIoctlController* pIoctlController)
{
    int srcDiskNum = sourceDevice->GetDeviceNumber();

    // Validate inputs
    if ((NULL == sourceDevice) || (NULL == pDataProcessor) || (NULL == logger))
    {
        throw CAgentException("Invalid input");
    }

    std::string wszDiskId = sourceDevice->GetDeviceId();

    if (m_deviceTransMap.find(srcDiskNum) != m_deviceTransMap.end())
    {
        throw CAgentException("%s %s is already addeded", __FUNCTION__, wszDiskId.c_str());
    }

    PGET_DB_TRANS_DATA pGetDbTrans = (PGET_DB_TRANS_DATA)malloc(sizeof(GET_DB_TRANS_DATA));
    if (NULL == pGetDbTrans)
    {
        throw CAgentException("%s: Failed to allocate pGetDbTrans", __FUNCTION__);
    }

    memset(pGetDbTrans, 0, sizeof(GET_DB_TRANS_DATA));

    pGetDbTrans->CommitCurrentTrans = true;
    pGetDbTrans->BufferSize = sourceDevice->GetMaxRwSizeInBytes();
    pGetDbTrans->m_sourceDevice = sourceDevice;
    pGetDbTrans->ulPollIntervalInMilliSeconds = 30 * 1000;
    pGetDbTrans->ulRunTimeInMilliSeconds = 30 * 60 * 1000;

    CReplicationWorker* pReplicationProcessor = new CReplicationWorker(pGetDbTrans,
        m_platformAPIs, logger, pDataProcessor, pIoctlController);
    m_deviceTransMap[srcDiskNum].reset(pReplicationProcessor);

    return ((void*)pReplicationProcessor);
}

void ReplicationManager::StartReplication(void* handle)
{
    if (NULL == handle)
    {
        throw CAgentException("%s: Invalid Handle", __FUNCTION__);
    }

    CReplicationWorker* pReplicationProcessor = (CReplicationWorker*)handle;
    pReplicationProcessor->StartReplication();
}

void ReplicationManager::ClearDifferentials(void* handle)
{
    if (NULL == handle)
    {
        throw CAgentException("%s: Invalid Handle", __FUNCTION__);
    }

    CReplicationWorker* pReplicationProcessor = (CReplicationWorker*)handle;
    pReplicationProcessor->ClearDifferentials();
}

void ReplicationManager::StartFiltering(void* handle)
{
    if (NULL == handle)
    {
        throw CAgentException("%s: Invalid Handle", __FUNCTION__);
    }

    CReplicationWorker* pReplicationProcessor = (CReplicationWorker*)handle;
    pReplicationProcessor->StartFiltering();

    m_pLogger->LogError("%s: Exited", __FUNCTION__);
}

void ReplicationManager::StopFiltering(void* handle, bool delBitmapFile = true)
{
    if (NULL == handle)
    {
        throw CAgentException("%s: Invalid Handle", __FUNCTION__);
    }

    CReplicationWorker* pReplicationProcessor = (CReplicationWorker*)handle;
    pReplicationProcessor->StopFiltering(delBitmapFile);
    m_pLogger->LogError("StopFiltering exited");
}

void ReplicationManager::ResyncStartNotification(void* handle)
{
    if (NULL == handle)
    {
        throw CAgentException("%s: Invalid Handle", __FUNCTION__);
    }

    CReplicationWorker* pReplicationProcessor = (CReplicationWorker*)handle;
    pReplicationProcessor->ResyncStartNotification();
}

void ReplicationManager::ResyncEndNotification(void* handle)
{
    if (NULL == handle)
    {
        throw CAgentException("%s: Invalid Handle", __FUNCTION__);
    }

    CReplicationWorker* pReplicationProcessor = (CReplicationWorker*)handle;
    pReplicationProcessor->ResyncEndNotification();
}

