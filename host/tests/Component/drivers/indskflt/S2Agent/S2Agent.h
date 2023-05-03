#ifndef _S2_AGENT_H_
#define _S2_AGENT_H_

#include "IPlatformAPIs.h"
#include "Logger.h"
#include "IBlockDevice.h"

#include "DataProcessor.h"
#include "InmageIoctlController.h"
#include "ReplicationManager.h"

#include "boost/shared_ptr.hpp"
#include "boost/unordered_set.hpp"

#include "boost/uuid/uuid.hpp"
#include "boost/uuid/uuid_io.hpp"
#include "boost/uuid/uuid_generators.hpp"
#include "boost/lexical_cast.hpp"

#include "ChecksumProvider.h"
#ifdef SV_WINDOWS
#include "Microsoft.Test.Inmage.ExlcudeBlocks.h"
#include "Microsoft.Test.Inmage.Win32ExcludeDiskBlocks.h"
#else
#include "PairDetails.h"
#endif

#include <map>

#ifdef SV_WINDOWS
#ifdef S2AGENT_EXPORTS
#define S2AGENT_API __declspec(dllexport)
#else
#define S2AGENT_API __declspec(dllimport)
#endif
#else
#define S2AGENT_API
#endif


class S2AGENT_API CS2Agent
{
private:
    map<int, IBlockDevice*>                     m_sourceTargetMap;
    map<int, IInmageChecksumProvider*>          m_md5ChksumProvider;
    map<int, IBlockDevice*>                     m_diskMap;
    map<int, ILogger*>                          m_loggerMap;
    map<int, IDataProcessor*>                   m_dataProcessorMap;
    map<int, void*>                             m_replicationHandle;
    CInmageIoctlController*                     m_ioctlController;
    IPlatformAPIs*                              m_platformApis;
    bool                                        m_replicationStarted;
    ReplicationManager*                         m_replicationManager;
    int                                         m_replicationType;
    std::string                                 m_testRoot;
    ILogger*                                    m_s2Logger;
#ifdef SV_WINDOWS
    IInmageExlcudeBlocks*                       m_excludeBlockProvider;
#else
    ACE_Process*                                m_processStartNotify;
#endif
    boost::unordered_set<std::string>           m_diskIdSet;
    bool AddReplicationSettings(IBlockDevice* source, IBlockDevice *target, const char* logFile);

public:
    CS2Agent(int replicationType, const char *testRoot);
    bool AddReplicationSettings(int uiSrcDisk, int uiDestDisk, const char *logFile);
    bool AddReplicationSettings(int uiSrcDisk, const char* szfileName, const char* logFile);
#ifdef SV_WINDOWS
    void StartSVAgents(unsigned long ulFlags);
    void SetDriverFlags(etBitOperation flag);
    void GetConsistentImages();
    bool SyncData();
    bool SetDriverConfig(SV_ULONG ulSystemPhysicalRamInMB);
#else
    IBlockDevice* GetDisk(int diskNum);
    void WaitForAllowDraining();
    void PauseDraining();
    void AllowDraining();
    void ProcessStartNotify();
    void KillStartNotify();
    void ResumeDataThread(std::vector<PairInfo::PairDetails *> &pairDetails);
    void PauseDataThread(std::vector<PairInfo::PairDetails *> &pairDetails, int sleepTime = 0);
    bool GetChecksum(CHAR srcPath[], CHAR tgtPath[], std::string &tag, bool waitForTag = true);
    bool GetVolumeStats(std::string &volStats);
    void WaitForTSO();
    void ResetTSO();
    bool BarrierHonour(std::vector<PairInfo::PairDetails *> pairDetails, bool verifyTag = true);
    bool WaitForDataMode();
    bool VerifyBitmapMode();
#endif
    bool StartReplication(SV_ULONGLONG currentOffset, SV_ULONGLONG endOffset,
        bool doIR = true);
    bool CompareFileContents(std::string srcFilePath, std::string tgtFilePath);
    void WaitForTSO(int uiSrcDisk);
    void ResetTSO(int uiSrcDisk);
    bool InsertCrashTag(std::string tagContext, list<std::string> tagName);
    bool WaitForTAG(int uiSrcDisk, std::string tag, int timeout);
    bool WaitForConsistentState(bool dontApply);
    void StopFiltering();
    void StartFiltering();
    bool CreateDrainBarrierOnTag(std::string &tag);
    void ReleaseDrainBarrier();
    std::string GenerateUuid();
    bool Validate();
    bool AreFilesEqual(std::string srcFile, std::string tgtFile);
    void GetSourceCheckSum(CHAR srcPath[], std::string &tag, std::string &tagContext);
    bool GetTargetCheckSum(CHAR tgtPath[], std::string &tag,
        std::string &tagContext, bool waitForTag = true);
    ~CS2Agent();
};

typedef enum _ReplicationType
{
    DataReplication = 0,
    BestEffortWalReplication = 1,
    AllOrNoneWalReplication = 2
}ReplicationType;

#endif

