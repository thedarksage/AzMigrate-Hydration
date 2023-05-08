#ifndef _REPLICATION_MANAGER_H
#define _REPLICATION_MANAGER_H

#include "IPlatformAPIs.h"

#include "BlockDevice/IBlockDevice.h"

#include "svdparse.h"
#include "svtypes.h"
#include "SVDStream.h"

#include "SegStream.h"
#include "InmageTestException.h"

#include "DataProcessor.h"
#include "InmageIoctlController.h"
#include "ReplicationWorker.h"

#include "boost/thread/mutex.hpp"
#include "boost/shared_ptr.hpp"

#include <vector>
#include <map>
#include <list>

#ifdef SV_WINDOWS
#ifdef REPLICATIONMANAGER_EXPORTS
#define REPLICATIONMANAGER_API __declspec(dllexport)
#else
#define REPLICATIONMANAGER_API __declspec(dllimport)
#endif
#else
#define REPLICATIONMANAGER_API
#endif


class CReplicationWorker;


// This class is exported from the InmageAgent.dll
class REPLICATIONMANAGER_API ReplicationManager

{
private:
    boost::shared_ptr<ILogger>                          m_pLogger;
    IPlatformAPIs*                                      m_platformAPIs;
    map<int, boost::shared_ptr<CReplicationWorker> >    m_deviceTransMap;
    static ReplicationManager*                          s_pInstance;
    static boost::mutex                                 s_instanceLock;

    volatile static SV_LONG                             s_numInstances;
    CInmageIoctlController*                             m_ioctlController;
    static bool                                         m_bInitialized;
    ReplicationManager(string logfile,
        IPlatformAPIs* platformAPIs, CInmageIoctlController* pIoctlController);

public:
    static ReplicationManager* GetInstance(string logfile,
        IPlatformAPIs* platformAPIs, CInmageIoctlController* pIoctlController);

    ~ReplicationManager();
    void Release();

    void* SetFiltering(IBlockDevice* sourceDevice,
        IDataProcessor* dataProcessor, ILogger* logger, CInmageIoctlController* pIoctlController);

    void StartReplication(void* handle);
    void ClearDifferentials(void* handle);
    void StartFiltering(void* handle);
    void ResyncStartNotification(void* handle);
    void ResyncEndNotification(void* handle);
    void StopFiltering(void* handle, bool delBitmapFile);
};

#endif
