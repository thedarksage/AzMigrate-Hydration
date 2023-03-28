// S2Agent.cpp : Defines the exported functions for the DLL application.
//

#include "S2Agent.h"
#include "CFileDevice.h"
#include <map>

const ULONG ulMinDataPoolSizeInMB = 256;

#ifdef SV_UNIX
using namespace PairInfo;
const std::string FILTERING_MODE = "Filtering Mode/Write Order State";
const std::string DATA_MODE = "Data/Data";
const std::string BITMAP_MODE = "Bitmap";
#endif

/// <summary>
/// Constructor for creating CS2 object
/// </summary>
/// <param name="replicationType">Replication type. It can be data or, WAL replication</param>
/// <param name="testRoot">Test Root. Logs are created in this folder.</param>
/// <returns>handle to CS2 object</returns>
CS2Agent::CS2Agent(int replicationType, const char *testRoot) :
m_replicationStarted(false),
m_replicationType(replicationType),
m_testRoot(testRoot)
{
#ifdef SV_WINDOWS
    std::string agentLog = m_testRoot + "\\" + "agent.log";
    std::string s2Log = m_testRoot + std::string("\\s2.log");
#else
    std::string agentLog = m_testRoot + std::string("/agent.log");
    std::string s2Log = m_testRoot + std::string("/s2.log");
#endif
    cout << endl << "\nCreating the s2Logger object..." << endl;
    m_s2Logger = new CLogger(s2Log, true);
    m_s2Logger->LogInfo("Entering: %s", __FUNCTION__);
    m_s2Logger->LogInfo("\nagentLog file is :%s\n", agentLog.c_str());
    m_s2Logger->LogInfo("\nCreated the s2logger file and it is :%s\n", s2Log.c_str());

    m_s2Logger->LogInfo("Creating the PlatformAPIs object...\n");
#ifdef SV_WINDOWS
    m_platformApis = new CWin32APIs();
#else
    m_platformApis = new PlatformAPIs(false);//It resolves to either Win32APIs or UnixAPIs;
#endif

    m_s2Logger->LogInfo("Creating the InmageIoctlController object...\n");
    m_ioctlController = new CInmageIoctlController(m_platformApis, m_s2Logger);

    m_s2Logger->LogInfo("Creating the Replication Manager instance...\n");
    m_replicationManager = ReplicationManager::GetInstance(agentLog,
        m_platformApis, m_ioctlController);

    m_s2Logger->LogInfo("Exiting: %s", __FUNCTION__);
}

CS2Agent::~CS2Agent()
{
    m_replicationManager->Release();
    delete m_ioctlController;

    // Delete Data Processor
    for (map<int, IDataProcessor*>::iterator it = m_dataProcessorMap.begin();
        m_dataProcessorMap.end() != it; it++)
    {
        delete it->second;
    }

    // Delete DiskMap
    for (map<int, IBlockDevice*>::iterator it = m_diskMap.begin(); m_diskMap.end() != it; it++)
    {
        delete it->second;
    }

    // Delete Logger
    for (map<int, ILogger*>::iterator it = m_loggerMap.begin(); m_loggerMap.end() != it; it++)
    {
        it->second->Flush();
        delete it->second;
    }

    m_s2Logger->Flush();
    delete m_s2Logger;

    delete m_platformApis;
}

#ifdef SV_UNIX

bool takenbarrier = false;
bool needfsync = false;
bool issuedfsync = false;

IBlockDevice* CS2Agent::GetDisk(int diskNum)
{
    return m_diskMap[diskNum];
}

void CS2Agent::ProcessStartNotify()
{
    m_s2Logger->LogError("Entering %s", __FUNCTION__);

    int inherit_environment =1;
    std::string startNotifyCmd = "/usr/local/ASR/Vx/bin/inm_dmit --op=start_notify";
	int cmd_buf_len = startNotifyCmd.size() +1;

	ACE_Process_Options options(inherit_environment, cmd_buf_len);

	options.command_line(startNotifyCmd.c_str());

	ACE_Process process;

	pid_t pid = process.spawn(options);

	if(pid == ACE_INVALID_PID)
	{
        throw CAgentException(
            "\nIn %s Failed to spawn process for start notify ioctl\n",
            __FUNCTION__);
	}
    m_processStartNotify = &process;

    m_s2Logger->LogError("Exiting %s", __FUNCTION__);
}

void CS2Agent::KillStartNotify()
{
    m_s2Logger->LogError("Entering %s", __FUNCTION__);
    m_processStartNotify->kill(SIGKILL);
    m_s2Logger->LogError("Exiting %s", __FUNCTION__);

}

void CS2Agent::PauseDraining()
{
    map<int, IDataProcessor*>::iterator it = m_dataProcessorMap.begin();
    for (; it != m_dataProcessorMap.end(); it++)
    {
        it->second->PauseDraining();
    }
}

void CS2Agent::AllowDraining()
{
    map<int, IDataProcessor*>::iterator it = m_dataProcessorMap.begin();
    for (; it != m_dataProcessorMap.end(); it++)
    {
        it->second->AllowDraining();
    }
}

#endif

/// <summary>
/// Method for setting replication between source and target block device.
/// </summary>
/// <param name="source">Source Block Device.</param>
/// <param name="target">Target Block Device.</param>
/// <param name="logFile">Replication log is put in this file.</param>
/// <returns>true always</returns>
bool CS2Agent::AddReplicationSettings(IBlockDevice* source, IBlockDevice *target, const char* logFile)
{
    if (m_diskIdSet.find(source->GetDeviceId()) != m_diskIdSet.end())
    {
        m_s2Logger->LogError("%s: DiskId %s is already added in the set",
            __FUNCTION__, (source->GetDeviceId()).c_str());
        return false;
    }

    m_diskIdSet.insert(source->GetDeviceId());

    m_sourceTargetMap[source->GetDeviceNumber()] = target;
    m_diskMap[source->GetDeviceNumber()] = source;

    if (m_replicationType == DataReplication)
    {
        m_diskMap[target->GetDeviceNumber()] = target;
    }

    m_loggerMap[source->GetDeviceNumber()] = new CLogger(logFile);
    return true;
}

/// <summary>
/// Method for setting replication between source device number and target device number.
/// </summary>
/// <param name="uiSrcDisk">Source Device Number.</param>
/// <param name="uiDestDisk">Target Device Number.</param>
/// <param name="logFile">Replication log is put in this file.</param>
/// <returns>true if replication is setup correctly. False otherwise.</returns>
bool CS2Agent::AddReplicationSettings(int uiSrcDisk, int uiDestDisk, const char* logFile)
{
    if (m_replicationStarted || (uiSrcDisk == uiDestDisk) ||
        (m_sourceTargetMap.find(uiSrcDisk) != m_sourceTargetMap.end()))
    {
        return false;
    }

    IBlockDevice* srcDevice = new CDiskDevice(uiSrcDisk, m_platformApis);
    if (m_diskIdSet.find(srcDevice->GetDeviceId()) != m_diskIdSet.end())
    {
        m_s2Logger->LogError("%s: DiskId %s is already added in disk set",
            __FUNCTION__, (srcDevice->GetDeviceId()).c_str());
        delete srcDevice;
        return false;
    }

    IBlockDevice* destDevice = new CDiskDevice(uiDestDisk, m_platformApis);
    return AddReplicationSettings(srcDevice, destDevice, logFile);
}

/// <summary>
/// Method for setting replication between source device number and target file.
/// Target is always file.
/// </summary>
/// <param name="uiSrcDisk">Source Device Number.</param>
/// <param name="szfileName">Target Filename.</param>
/// <param name="logFile">Replication log is put in this file.</param>
/// <returns>true if replication is setup correctly. False otherwise.</returns>
bool CS2Agent::AddReplicationSettings(int uiSrcDisk, const char* szfileName, const char* logFile)
{
    // Currently Once replication is started
    // Adding new disk is not supported
    if (m_replicationStarted || (m_diskMap.end() != m_diskMap.find(uiSrcDisk)))
    {
        m_loggerMap[uiSrcDisk]->LogError("%s: Disk %d is already getting replicated",
            __FUNCTION__, uiSrcDisk);
        return false;
    }

    IBlockDevice* srcDevice = new CDiskDevice(uiSrcDisk, m_platformApis);
    CRawFile* destDevice = NULL;
    bool bReturn = false;

    try
    {
        // Create a file of same size as that of source disk
        destDevice = new CRawFile(szfileName, srcDevice->GetDeviceSize(), m_platformApis);

        bReturn = AddReplicationSettings(srcDevice, destDevice, logFile);
    }
    catch (exception& ex)
    {
        m_s2Logger->LogError("%s: Disk %d Error: %s", __FUNCTION__, uiSrcDisk, ex.what());
        return false;
    }

    return bReturn;
}

/// <summary>
/// Resets TSO for given source disk.
/// </summary>
/// <param name="uiSrcDisk">Source Device Number.</param>
/// <returns>VOID.</returns>
void CS2Agent::ResetTSO(int uiSrcDisk)
{
    if (m_diskMap.end() == m_diskMap.find(uiSrcDisk))
    {
        return;
    }
#ifdef SV_UNIX
    m_dataProcessorMap[uiSrcDisk]->ResetTSO();
#endif
}

/// <summary>
/// Waits for next TSO for given source disk.
/// </summary>
/// <param name="uiSrcDisk">Source Device Number.</param>
/// <returns>VOID.</returns>
void CS2Agent::WaitForTSO(int uiSrcDisk)
{
    if (m_diskMap.end() == m_diskMap.find(uiSrcDisk))
    {
        return;
    }

    m_s2Logger->LogError("%s source disk : %d wait start", __FUNCTION__, uiSrcDisk);
    m_dataProcessorMap[uiSrcDisk]->WaitForTSO(TIMEOUT_INFINITE);
    m_s2Logger->LogError("%s source disk : %d wait completed", __FUNCTION__, uiSrcDisk);
}

bool CS2Agent::StartReplication(SV_ULONGLONG currentOffset, SV_ULONGLONG endOffset, bool doIR)
{
    IDataProcessor* pDataProcessor;

    map<int, IBlockDevice*>::iterator it = m_sourceTargetMap.begin();
    IInmageChecksumProvider* chksumProvider;

    try
    {
#ifdef SV_WINDOWS
        if(endOffset) m_ioctlController->StopFiltering(std::string(), true, true);
        m_excludeBlockProvider = new CWin32ExcludeDiskBlocks(m_platformApis,
            m_s2Logger, m_ioctlController);
#else
        m_ioctlController->StopFiltering(std::string(), true, true);
        m_ioctlController->ServiceShutdownNotify(0);
#endif
        for (; it != m_sourceTargetMap.end(); it++)
        {
            IBlockDevice* pSrcDisk = m_diskMap[it->first];
            IBlockDevice* pTargetDisk = it->second;

            int srcDiskNum = pSrcDisk->GetDeviceNumber();
            int tgtDiskNum = pTargetDisk->GetDeviceNumber();

            ILogger* logger = m_loggerMap[srcDiskNum];

            if (DataReplication == m_replicationType)
            {
                pDataProcessor = new CDataApplier(pSrcDisk, pTargetDisk, m_platformApis, logger);
            }
            else if (BestEffortWalReplication == m_replicationType)
            {
#ifdef SV_WINDOWS
                pDataProcessor = new CDataWALProcessor(srcDiskNum,
                    m_testRoot, m_platformApis, logger);
#endif
            }

            m_dataProcessorMap[srcDiskNum] = pDataProcessor;

#ifdef SV_WINDOWS
            chksumProvider = new CInmageMD5ChecksumProvider(
                m_excludeBlockProvider->GetExcludeBlocks(pSrcDisk->GetDeviceId()), m_s2Logger);
#else
            std::list<ExtentInfo> emptyList;
            chksumProvider = new CInmageMD5ChecksumProvider(emptyList,m_s2Logger);
#endif
            m_md5ChksumProvider[srcDiskNum] = chksumProvider;

            void* handle = m_replicationManager->SetFiltering(pSrcDisk,
                pDataProcessor, logger, m_ioctlController);
            m_replicationHandle.insert(pair<int, void*>(srcDiskNum, handle));
        }

        for (it = m_sourceTargetMap.begin(); it != m_sourceTargetMap.end(); it++)
        {
            IBlockDevice* pSrcDisk = m_diskMap[it->first];
            IBlockDevice* pTargetDisk = it->second;

            int srcDiskNum = pSrcDisk->GetDeviceNumber();
            int tgtDiskNum = pTargetDisk->GetDeviceNumber();

            // Do initial sync
            if (endOffset != 0)
            {
                endOffset = pTargetDisk->GetDeviceSize();
                m_replicationManager->StartFiltering(m_replicationHandle[srcDiskNum]);
                m_replicationManager->ClearDifferentials(m_replicationHandle[srcDiskNum]);
                if (doIR)
                {
                    m_dataProcessorMap[srcDiskNum]->InitialSync(currentOffset, endOffset);
                }
            }
            m_replicationManager->StartReplication(m_replicationHandle[srcDiskNum]);
        }

#ifdef SV_UNIX
        //Sleep for stacking to happen
        boost::this_thread::sleep(boost::posix_time::milliseconds(30 * 1000));
        AllowDraining();
        WaitForConsistentState(false);
#endif
    }
    catch (exception& ex)
    {
        m_s2Logger->LogError("%s %s", __FUNCTION__, ex.what());
        return false;
    }

    return true;
}

bool CS2Agent::InsertCrashTag(string tagContext, list<string> taglist)
{
    bool status = true;

    try
    {
#ifdef SV_WINDOWS
        status = m_ioctlController->LocalCrashConsistentTagIoctl(tagContext,
            taglist, (SV_ULONG) TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS, 100);
#else
        status = m_ioctlController->LocalStandAloneCrashConsistentTagIoctl(
            tagContext, taglist, (SV_ULONG)TAG_ALL_PROTECTED_VOLUME_IOBARRIER, 360000);
#endif
    }
    catch (exception& ex)
    {
        m_s2Logger->LogError("%s %s", __FUNCTION__, ex.what());
        status = false;
    }

    return status;
}

bool CS2Agent::CompareFileContents(std::string srcFilePath, std::string tgtFilePath)
{
    m_s2Logger->LogInfo("Entering function %s\n", __FUNCTION__);

    std::ifstream f1(srcFilePath.c_str(), std::ifstream::binary | std::ifstream::ate);
    std::ifstream f2(tgtFilePath.c_str(), std::ifstream::binary | std::ifstream::ate);

    if (f1.fail() || f2.fail()) {
        m_s2Logger->LogError("Failed to open either source or, target file\n");
        return false; //file problem
    }

    if (f1.tellg() != f2.tellg()) {
        m_s2Logger->LogError("Size mismatch between source file: %llu and target file: %llu\n",
            f1.tellg(), f2.tellg());
        return false; //size mismatch
    }

    //seek back to beginning and use std::equal to compare contents
    f1.seekg(0, std::ifstream::beg);
    f2.seekg(0, std::ifstream::beg);

    return std::equal(std::istreambuf_iterator<char>(f1.rdbuf()),
        std::istreambuf_iterator<char>(),
        std::istreambuf_iterator<char>(f2.rdbuf()));
}

bool CS2Agent::AreFilesEqual(std::string srcFilePath, std::string tgtFilePath)
{
    m_s2Logger->LogInfo("Entering function %s\n", __FUNCTION__);

    m_s2Logger->LogInfo("%s:Comparing source file : %s, target file : %s\n",
        __FUNCTION__, srcFilePath.c_str(), tgtFilePath.c_str());
    if (CompareFileContents(srcFilePath, tgtFilePath))
    {
        m_s2Logger->LogInfo("Checksum matched between file %s and %s\n",
            srcFilePath.c_str(), tgtFilePath.c_str());
        return true;
    }

    m_s2Logger->LogError("Checksum mismatched between file %s and %s.. Doing detailed analysis\n",
        srcFilePath.c_str(), tgtFilePath.c_str());

    ifstream::pos_type srcSize, tgtSize;
    unsigned long long chunksize = 1024;
    vector<char> srcBuffer(chunksize), tgtBuffer(chunksize);

    std::ifstream srcFile(srcFilePath.c_str(), ios::binary);
    std::ifstream tgtFile(tgtFilePath.c_str(), ios::binary);

    srcSize = srcFile.seekg(0, ifstream::end).tellg();
    tgtSize = tgtFile.seekg(0, ifstream::end).tellg();

    if (srcSize != tgtSize)
    {
        return false;
    }

    srcFile.seekg(0, ifstream::beg);
    tgtFile.seekg(0, ifstream::beg);

    unsigned long long bytesRemaining = srcSize;
    unsigned long long currentOffset = 0;
    bool status = true;

    while (bytesRemaining)
    {
        unsigned long long bytesToRead = (bytesRemaining < chunksize) ? bytesRemaining : chunksize;
        bytesRemaining -= bytesToRead;

        srcFile.read(&srcBuffer[0], bytesToRead);
        tgtFile.read(&tgtBuffer[0], bytesToRead);

        if (0 != memcmp(&srcBuffer[0], &tgtBuffer[0], bytesToRead))
        {
            m_s2Logger->LogError("checksum failed at offset = 0x%x", currentOffset);
#ifdef SV_UNIX
             m_s2Logger->LogError("Source has pattern %c, Target has pattern %c\n",
                srcBuffer[0], tgtBuffer[0]);
#endif
            status = false;
        }
        currentOffset += bytesToRead;
    }

    return status;
}

void CS2Agent::GetSourceCheckSum(CHAR srcPath[], std::string &tag, std::string &tagContext)
{
    m_s2Logger->LogInfo("ENTERED %s", __FUNCTION__);
    m_s2Logger->LogInfo("%s Tag Context : %s, tag : %s",
        __FUNCTION__, tagContext.c_str(), tag.c_str());

    map<int, IBlockDevice*>::iterator it;
    for (it = m_sourceTargetMap.begin(); it != m_sourceTargetMap.end(); it++)
    {
        IBlockDevice* pSrcDisk = m_diskMap[it->first];

#ifdef SV_WINDOWS
        inm_sprintf_s(srcPath, MAX_STRING_SIZE, "%s\\%s-%d-src.chksum",
            m_testRoot.c_str(), tag.c_str(), it->first);
#else
        inm_sprintf_s(srcPath, MAX_STRING_SIZE, "%s/%s-%d-src.chksum",
            m_testRoot.c_str(), tag.c_str(), it->first);
#endif
        m_s2Logger->LogInfo("%s Source Path : %s", __FUNCTION__, srcPath);

        if (m_platformApis->PathExists(srcPath))
        {
            if (0 != remove(srcPath))
            {
                m_s2Logger->LogError("Failed to delete file %s", srcPath);
                // Revoke tag
#ifdef SV_WINDOWS
                m_ioctlController->ReleaseWrites(tagContext,
                    TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS);
                m_ioctlController->CommitTag(tagContext, TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS);
#else
                m_ioctlController->ReleaseWrites(tagContext, TAG_ALL_PROTECTED_VOLUME_IOBARRIER);
                m_ioctlController->CommitTag(tagContext, TAG_ALL_PROTECTED_VOLUME_IOBARRIER);
#endif
                boost::uuids::uuid uuid = boost::uuids::random_generator()();
                tag = boost::lexical_cast<std::string>(uuid);
            }
        }

        std::ofstream output(srcPath, std::ifstream::binary);

        m_md5ChksumProvider[pSrcDisk->GetDeviceNumber()]->GetCheckSum(pSrcDisk, output);
        output.close();
    }
    m_s2Logger->LogInfo("EXITED %s", __FUNCTION__);
}

bool CS2Agent::GetTargetCheckSum(CHAR tgtPath[], std::string &tag,
    std::string &tagContext, bool waitForTag)
{
    m_s2Logger->LogInfo("ENTERED %s", __FUNCTION__);
    m_s2Logger->LogInfo("%s Tag Context : %s, tag : %s, Wait FOr Tag : %d",
        __FUNCTION__, tagContext.c_str(), tag.c_str(), waitForTag);

    bool status = true;
    map<int, IBlockDevice*>::iterator it;

    for (it = m_sourceTargetMap.begin(); it != m_sourceTargetMap.end(); it++)
    {
        IBlockDevice* pSrcDisk = m_diskMap[it->first];
        IBlockDevice* pTargetDisk = it->second;
        if (waitForTag && !m_dataProcessorMap[it->first]->WaitForTag(tag, TIMEOUT_INFINITE, false))
        {
            status = false;
            break;
        }

#ifdef SV_WINDOWS
        inm_sprintf_s(tgtPath, MAX_STRING_SIZE, "%s\\%s-%d-tgt.chksum",
            m_testRoot.c_str(), tag.c_str(), it->first);
#else
        inm_sprintf_s(tgtPath, MAX_STRING_SIZE, "%s/%s-%d-tgt.chksum",
            m_testRoot.c_str(), tag.c_str(), it->first);
#endif
        m_s2Logger->LogInfo("%s Target Path : %s", __FUNCTION__, tgtPath);
        if (m_platformApis->PathExists(tgtPath) && (0 != remove(tgtPath)))
        {
            m_s2Logger->LogError("%s: File %s exists failed to remove err=%d",
                __FUNCTION__, tgtPath, m_platformApis->GetLastError());
            status = false;
            break;
        }

        m_s2Logger->LogInfo("%s: Start: Calculating checksum for disk %d",
            __FUNCTION__, pSrcDisk->GetDeviceNumber());
        std::ofstream output(tgtPath, std::ifstream::binary);
        m_md5ChksumProvider[pSrcDisk->GetDeviceNumber()]->GetCheckSum(pTargetDisk, output);
        m_s2Logger->LogInfo("%s: Done: Calculating checksum for disk %d",
            __FUNCTION__, pSrcDisk->GetDeviceNumber());
        output.close();
    }

    m_s2Logger->LogInfo("EXITED %s", __FUNCTION__);
    return status;
}

#ifdef SV_UNIX
void CS2Agent::ResumeDataThread(std::vector<PairDetails *> &pairDetails)
{
    std::vector<PairDetails *>::iterator iter;
    for (iter = pairDetails.begin(); iter != pairDetails.end(); iter++)
    {
        PairDetails *pd = (*iter);
        pd->pauseDataThread = false;
    }

}

void CS2Agent::PauseDataThread(std::vector<PairDetails *> &pairDetails, int sleepTime)
{
    std::vector<PairDetails *>::iterator iter;
    for (iter = pairDetails.begin(); iter != pairDetails.end(); iter++)
    {
        PairDetails *pd = (*iter);
        pd->pauseDataThread = true;
        if (sleepTime)
        {
            boost::this_thread::sleep(boost::posix_time::seconds(sleepTime));
        }
    }

}

bool CS2Agent::GetChecksum(CHAR srcPath[], CHAR tgtPath[], std::string &tag, bool waitForTag)
{
    m_s2Logger->LogInfo("ENTERED %s", __FUNCTION__);

    bool status = true;
    if (!waitForTag)
    {
        boost::uuids::uuid uuid = boost::uuids::random_generator()();
        tag = boost::lexical_cast<std::string>(uuid);
    }

    m_s2Logger->LogInfo("%s: Getting source and target check sum for tag : %s",
        __FUNCTION__, tag.c_str());
    GetSourceCheckSum(srcPath, tag, tag);
    status = GetTargetCheckSum(tgtPath, tag, tag, waitForTag);
    if (!status)
    {
        m_s2Logger->LogInfo("%s: Error status is failure", __FUNCTION__);
        status = false;
    }

    m_s2Logger->LogInfo("EXITED %s", __FUNCTION__);
    return status;

}

bool CS2Agent::GetVolumeStats(std::string &volStats)
{
    m_s2Logger->LogInfo("ENTERED %s", __FUNCTION__);

    bool status = true;
    volStats = "";
    map<int, IBlockDevice*>::iterator it;
    for (it = m_sourceTargetMap.begin(); it != m_sourceTargetMap.end(); it++)
    {
        IBlockDevice* pSrcDisk = m_diskMap[it->first];
        std::string volGuid = pSrcDisk->GetDeviceId();
        std::string stats;
        if(!m_ioctlController->GetVolumeStats(volGuid, stats))
        {
            status = false;
            m_s2Logger->LogError("%s : GetVolumeStats failed for disk : %s",
                __FUNCTION__, volGuid.c_str());
            break;
        }
        volStats += stats;
    }

    m_s2Logger->LogInfo("Volume Stats : \n%s", volStats.c_str());
    m_s2Logger->LogInfo("EXITED %s", __FUNCTION__);
    return status;
}

bool CS2Agent::VerifyBitmapMode()
{
    m_s2Logger->LogInfo("ENTERED %s", __FUNCTION__);
    bool status = false;
    std::string volStats;

    if (!GetVolumeStats(volStats))
    {
        return false;
    }

    std::istringstream volStatsStream(volStats);
    std::string volStatsLine;
    while (std::getline(volStatsStream, volStatsLine))
    {
        if (volStatsLine.find(FILTERING_MODE) == std::string::npos)
        {
            continue;
        }

        m_s2Logger->LogInfo("%s : %s", __FUNCTION__, volStatsLine.c_str());
        if (volStatsLine.find(BITMAP_MODE) != std::string::npos)
        {
            status = true;
            break;
        }
    }

    m_s2Logger->LogInfo("EXITED %s", __FUNCTION__);
    return status;
}

bool CS2Agent::WaitForDataMode()
{
    m_s2Logger->LogInfo("ENTERED %s", __FUNCTION__);
    bool status = true;
    int sleepTime = 20;
    int WaitTime = 1200;
    boost::chrono::system_clock::time_point endTime =
        boost::chrono::system_clock::now() + boost::chrono::seconds(WaitTime);
    do
    {
        std::string volStats;
        status = GetVolumeStats(volStats);
        if (!status)
        {
            break;
        }
        bool found = false;

        std::istringstream volStatsStream(volStats);
        std::string volStatsLine;
        while (std::getline(volStatsStream, volStatsLine))
        {
            if (volStatsLine.find(FILTERING_MODE) == std::string::npos)
            {
                continue;
            }

            m_s2Logger->LogInfo("%s : %s", __FUNCTION__, volStatsLine.c_str());
            if (volStatsLine.find(DATA_MODE) != std::string::npos)
            {
                found = true;
                break;
            }
        }
        if (found)
        {
            break;
        }

        boost::chrono::system_clock::time_point curTime = boost::chrono::system_clock::now();
        if (curTime > endTime)
        {
            m_s2Logger->LogError("%s : Failed to switch to data mode.", __FUNCTION__);
            status = false;
            break;
        }
        boost::this_thread::sleep(boost::posix_time::seconds(sleepTime));
    } while(true);

    m_s2Logger->LogInfo("EXITED %s", __FUNCTION__);
    return status;
}

/*
 * 1. No writes happening on source disk, no pending data for replication and no inflight writes
 * 2. Start drainer.
 * 3. Match md5sum of source and target disk
 * 4. Create barrier and hold for 2 min
 * 5. Start write thread on source disk
 * 6. Sleep for 1 min.
 * 7. Stop write thread on source disk.
 * 8. Insert tag
 * 9. Remove the barrier.
 * 10. Drain data till tag.
 * 11. Get md5sum of source and target, since no data was drained after the barrier start,
 *     hence md5sum of target will be same as old, source md5sum should differ as we have
 *     issued writes on the disk.
 * 12. Drain all writes
 * 13. md5sum of target should match with source disk, source md5sum should remain same.
 * */

void CS2Agent::WaitForTSO()
{
    m_s2Logger->LogInfo("ENTERED %s", __FUNCTION__);
    map<int, IBlockDevice*>::iterator it;
    for (it = m_sourceTargetMap.begin(); it != m_sourceTargetMap.end(); it++)
    {
        IBlockDevice* pSrcDisk = m_diskMap[it->first];
        WaitForTSO(pSrcDisk->GetDeviceNumber());
    }

    m_s2Logger->LogInfo("EXITED %s", __FUNCTION__);
}

void CS2Agent::ResetTSO()
{
    m_s2Logger->LogInfo("ENTERED %s", __FUNCTION__);
    map<int, IBlockDevice*>::iterator it;
    for (it = m_sourceTargetMap.begin(); it != m_sourceTargetMap.end(); it++)
    {
        IBlockDevice* pSrcDisk = m_diskMap[it->first];
        ResetTSO(pSrcDisk->GetDeviceNumber());
    }

    m_s2Logger->LogInfo("EXITED %s", __FUNCTION__);
}

bool CS2Agent::BarrierHonour(std::vector<PairDetails *> pairDetails)
{
    m_s2Logger->LogInfo("ENTERED %s", __FUNCTION__);

    bool status = true;

    CHAR srcPathOrig[MAX_PATH];
    CHAR srcPathCurr[MAX_PATH];
    CHAR srcPathFinal[MAX_PATH];
    CHAR tgtPathOrig[MAX_PATH];
    CHAR tgtPathCurr[MAX_PATH];
    CHAR tgtPathFinal[MAX_PATH];
    std::string volStats;
    std::string tag;

    try
    {
        do
        {

            m_s2Logger->LogInfo("%s : Stopping writes on disk.", __FUNCTION__);
            PauseDataThread(pairDetails);

            m_s2Logger->LogInfo("%s : Waiting for TSO file.", __FUNCTION__);
            WaitForTSO();

            m_s2Logger->LogInfo("%s : Get checksum for source and target.", __FUNCTION__);
            status = GetChecksum(srcPathOrig, tgtPathOrig, tag, false);
            if (!status)
            {
                goto cleanup;
            }

            status = AreFilesEqual(srcPathOrig, tgtPathOrig);
            if (status)
            {
                m_s2Logger->LogInfo(
                    "%s : Original checksum verification for Tag %s,"
                    " source path : %s, target path : %s succeeded",
                    __FUNCTION__, tag.c_str(), srcPathOrig, tgtPathOrig);
            }
            else
            {
                m_s2Logger->LogError(
                    "%s : Original checksum verification for Tag %s,"
                    " source path : %s, target path : %s failed",
                    __FUNCTION__, tag.c_str(), srcPathOrig, tgtPathOrig);

                goto cleanup;
            }

            boost::uuids::uuid uuid = boost::uuids::random_generator()();
            tag = boost::lexical_cast<std::string>(uuid);
            std::list<std::string> taglist;
            taglist.push_back(tag);

            m_s2Logger->LogInfo("%s : Creating drain barrier for Tag %s",
                __FUNCTION__, tag.c_str());
            if (!CreateDrainBarrierOnTag(tag))
            {
                return false;
            }
            m_s2Logger->LogInfo("%s : Creating io barrier for Tag %s", __FUNCTION__, tag.c_str());
            m_ioctlController->HoldWrites(tag, TAG_ALL_PROTECTED_VOLUME_IOBARRIER, 600*1000);

            m_s2Logger->LogInfo("%s : Resuming writes on disk", __FUNCTION__);
            ResumeDataThread(pairDetails);
            boost::this_thread::sleep(boost::posix_time::seconds(15));

            m_s2Logger->LogInfo("%s : Insert tag to the disk %s", __FUNCTION__, tag.c_str());
            m_ioctlController->InsertDistTag(tag, TAG_ALL_PROTECTED_VOLUME_IOBARRIER,
                taglist);

            m_s2Logger->LogInfo("%s : Stopping writes on disk", __FUNCTION__);
            PauseDataThread(pairDetails);

            m_s2Logger->LogInfo("%s : Releasing barrier for Tag %s", __FUNCTION__, tag.c_str());
            m_ioctlController->ReleaseWrites(tag, TAG_ALL_PROTECTED_VOLUME_IOBARRIER);
            m_ioctlController->CommitTag(tag, TAG_ALL_PROTECTED_VOLUME_IOBARRIER);

            ResetTSO();

            status = GetVolumeStats(volStats);
            if (!status)
            {
                goto cleanup;
            }

            status = GetChecksum(srcPathCurr, tgtPathCurr, tag);
            if (!status)
            {
                goto cleanup;
            }

            status = !AreFilesEqual(srcPathCurr, tgtPathCurr);
            if (!status)
            {
                m_s2Logger->LogError(
                    "%s : Invalid state for Tag %s, source path : %s, target path : %s",
                    __FUNCTION__, tag.c_str(), srcPathCurr, tgtPathCurr);
                goto cleanup;
            }

            status = AreFilesEqual(tgtPathOrig, tgtPathCurr);
            if (status)
            {
                m_s2Logger->LogInfo(
                    "%s : Current checksum verification for Tag %s,"
                    " source path : %s, target path : %s succeeded",
                    __FUNCTION__, tag.c_str(), tgtPathOrig, tgtPathCurr);
            }
            else
            {
                m_s2Logger->LogError(
                    "%s : Comparison failed for Tag %s, source path : %s, target path : %s",
                    __FUNCTION__, tag.c_str(), tgtPathOrig, tgtPathCurr);
                goto cleanup;
            }

            m_s2Logger->LogInfo("%s : Releasing Drain barrier", __FUNCTION__);
            ReleaseDrainBarrier();

            m_s2Logger->LogInfo("%s : Waiting for TSO file.", __FUNCTION__);
            WaitForTSO();

            status = GetVolumeStats(volStats);
            if (!status)
            {
                goto cleanup;
            }

            status = GetChecksum(srcPathFinal, tgtPathFinal, tag, false);
            if (!status)
            {
                goto cleanup;
            }

            status = AreFilesEqual(srcPathCurr, srcPathFinal);
            if (!status)
            {
                m_s2Logger->LogError(
                    "%s : Comparison failed for Tag %s, source path : %s, target path : %s",
                    __FUNCTION__, tag.c_str(), srcPathCurr, srcPathFinal);
                goto cleanup;
            }

            status = AreFilesEqual(srcPathFinal, tgtPathFinal);
            if (!status)
            {
                m_s2Logger->LogError(
                    "%s : Comparison failed for Tag %s, source path : %s, target path : %s",
                    __FUNCTION__, tag.c_str(), srcPathFinal, tgtPathFinal);
                goto cleanup;
            }

        } while(false);
    }
    catch (exception& ex)
    {
        m_s2Logger->LogError("%s %s", __FUNCTION__, ex.what());
        status = false;
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
    }

cleanup:

    ResetTSO();
    ReleaseDrainBarrier();
    ResumeDataThread(pairDetails);

    m_s2Logger->LogInfo("EXITED %s", __FUNCTION__);
    return status;
}
#endif

bool CS2Agent::CreateDrainBarrierOnTag(std::string &tag)
{
    m_s2Logger->LogInfo("ENTERED %s", __FUNCTION__);
    bool status = true;
    map<int, IBlockDevice*>::iterator it;

    for (it = m_sourceTargetMap.begin(); it != m_sourceTargetMap.end(); it++)
    {
        IBlockDevice* pSrcDisk = m_diskMap[it->first];
        IBlockDevice* pTargetDisk = it->second;
        if (!m_dataProcessorMap[it->first]->CreateDrainBarrierOnTag(tag))
        {
            m_loggerMap[it->first]->LogError("Tag %s is already received for disk %d",
                tag.c_str(), it->first);
            status = false;
            break;
        }
    }

    m_s2Logger->LogInfo("EXITED %s", __FUNCTION__);
    return status;
}

void CS2Agent::ReleaseDrainBarrier()
{
    m_s2Logger->LogInfo("ENTERED %s", __FUNCTION__);
    map<int, IBlockDevice*>::iterator it;

    for (it = m_sourceTargetMap.begin(); it != m_sourceTargetMap.end(); it++)
    {
        m_s2Logger->LogInfo("%s: Releasing drain barrier for disk: %d", __FUNCTION__, it->first);
        m_dataProcessorMap[it->first]->ReleaseDrainBarrier();
    }

    m_s2Logger->LogInfo("EXITED %s", __FUNCTION__);
}

bool CS2Agent::Validate()
{
    bool status = true;
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    std::string tag = boost::lexical_cast<std::string>(uuid);
#ifdef SV_WINDOWS
    uuid = boost::uuids::random_generator()();
    std::string tagContext = boost::lexical_cast<std::string>(uuid);
    int sleepTime = 1; //in seconds
#else
    std::string tagContext = tag;
    int sleepTime = 30; //in seconds
#endif

    m_s2Logger->LogInfo("%s : Tag Context : %s, Tag GUID : %s",
        __FUNCTION__, tagContext.c_str(), tag.c_str());
    std::list<std::string> taglist;
    taglist.push_back(tag);

    CHAR    srcPath[MAX_PATH];
    CHAR    tgtPath[MAX_PATH];
    map<int, IBlockDevice*>::iterator it;

    if (!CreateDrainBarrierOnTag(tag))
    {
        return false;
    }
    bool heldwrites = false;
    for (int i = 0; i < 10; i++)
    {
        try
        {
#ifdef SV_WINDOWS
            m_ioctlController->HoldWrites(tagContext,
                TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS | TAG_INFO_FLAGS_TAG_IF_ZERO_INFLIGHT_IO,
                3600 * 1000);
            m_ioctlController->InsertDistTag(tagContext,
                TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS, taglist);
#else
            m_ioctlController->HoldWrites(tagContext, TAG_ALL_PROTECTED_VOLUME_IOBARRIER, 60*1000);
            // Wait for inflight IOs to complete
            boost::this_thread::sleep(boost::posix_time::seconds(1));
            m_ioctlController->InsertDistTag(tagContext,
                TAG_ALL_PROTECTED_VOLUME_IOBARRIER, taglist);
#endif
            heldwrites = true;

            GetSourceCheckSum(srcPath, tag, tagContext);

#ifdef SV_WINDOWS
            m_ioctlController->ReleaseWrites(tagContext, TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS);
            m_ioctlController->CommitTag(tagContext,
                TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS | TAG_INFO_FLAGS_COMMIT_TAG);
#else
            m_ioctlController->ReleaseWrites(tagContext, TAG_ALL_PROTECTED_VOLUME_IOBARRIER);
            m_ioctlController->CommitTag(tagContext, TAG_ALL_PROTECTED_VOLUME_IOBARRIER);
#endif
            status = true;
            break;
        }
        catch (exception& ex)
        {
            m_s2Logger->LogError("%s %s", __FUNCTION__, ex.what());
            status = false;
			boost::this_thread::sleep(boost::posix_time::seconds(sleepTime));
        }
    }

    if (!status)
    {
        for (it = m_sourceTargetMap.begin(); it != m_sourceTargetMap.end(); it++)
        {
            m_dataProcessorMap[it->first]->ReleaseDrainBarrier();
        }

        m_s2Logger->LogError("%s: failed to insert tag", __FUNCTION__);
        return false;
    }

    // Now get checksum of target
    status = GetTargetCheckSum(tgtPath, tag, tagContext);
    if (!status)
    {
        m_s2Logger->LogInfo("%s: Error status is failure", __FUNCTION__);
        goto cleanup;
    }

    for (it = m_sourceTargetMap.begin(); it != m_sourceTargetMap.end(); it++)
    {
#ifdef SV_WINDOWS
        inm_sprintf_s(srcPath, MAX_STRING_SIZE, "%s\\%s-%d-src.chksum",
            m_testRoot.c_str(), tag.c_str(), it->first);
        inm_sprintf_s(tgtPath, MAX_STRING_SIZE, "%s\\%s-%d-tgt.chksum",
            m_testRoot.c_str(), tag.c_str(), it->first);
#endif
        status = AreFilesEqual(srcPath, tgtPath);
        if (!status) {
            m_s2Logger->LogError("Disk: %d Tag %s comparision failed", it->first, tag.c_str());
        }
    }

cleanup:
    ReleaseDrainBarrier();

    return status;

}

bool CS2Agent::WaitForTAG(int uiSrcDisk, string tag, int timeout)
{
    bool status = false;

    if (m_diskMap.end() == m_diskMap.find(uiSrcDisk))
    {
        m_s2Logger->LogError("%s %d is not filtered", __FUNCTION__, uiSrcDisk);
        return false;
    }

    try
    {
        status = m_dataProcessorMap[uiSrcDisk]->WaitForTag(tag, timeout, false);
    }
    catch (exception& ex)
    {
        m_s2Logger->LogError("%s %s", __FUNCTION__, ex.what());
    }
    m_dataProcessorMap[uiSrcDisk]->ReleaseDrainBarrier();
    return status;
}

void CS2Agent::StopFiltering()
{
    map<int, IBlockDevice*>::iterator it = m_sourceTargetMap.begin();

    for (; it != m_sourceTargetMap.end(); it++)
    {
        int srcDiskNum = it->first;
        try
        {
            m_replicationManager->StopFiltering(m_replicationHandle[srcDiskNum], false);
        }
        catch (exception& ex)
        {
            m_s2Logger->LogError("%s StopFiltering failed for disk: %d Error: %s",
                __FUNCTION__, srcDiskNum, ex.what());
        }
    }
}

void CS2Agent::StartFiltering()
{
    map<int, IBlockDevice*>::iterator it = m_sourceTargetMap.begin();

    for (; it != m_sourceTargetMap.end(); it++)
    {
        int srcDiskNum = it->first;
        try
        {
            m_replicationManager->StartFiltering(m_replicationHandle[srcDiskNum]);
        }
        catch (exception& ex)
        {
            m_s2Logger->LogError("%s StartFiltering failed for disk: %d Error: %s",
                __FUNCTION__, srcDiskNum, ex.what());
        }
    }
}



bool CS2Agent::WaitForConsistentState(bool dontApply)
{
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    std::string tag = boost::lexical_cast<std::string>(uuid);

    list<string> taglist;
    bool done = false;

    taglist.push_back(tag);

    try
    {
#ifdef SV_UNIX
        bool status = WaitForDataMode();
        if (!status)
        {
            return false;
        }
#endif
        for (int count = 0; count < 100; count++)
        {
            m_s2Logger->LogError("%s : Inserting tag : %s, count : %d",
                __FUNCTION__, tag.c_str(), count);
            done = InsertCrashTag(tag, taglist);
            if (done)
            {
                m_s2Logger->LogError("%s : Inserted tag : %s", __FUNCTION__, tag.c_str());
                break;
            }
            boost::this_thread::sleep(boost::posix_time::milliseconds(30 * 1000));
        }

        // Check if tag was received
        if (!done)
        {
            m_s2Logger->LogError("%s : Tag is not received", __FUNCTION__);
            return false;
        }

        // Wait for all of disks to receive tag
        map<int, IDataProcessor*>::iterator it = m_dataProcessorMap.begin();
        for (; it != m_dataProcessorMap.end(); it++)
        {
            if (!it->second->WaitForTag(tag, TIMEOUT_INFINITE, dontApply))
            {
                m_s2Logger->LogError("%s : Tag is not yet received", __FUNCTION__);
                return false;
            }
            it->second->ReleaseDrainBarrier();
        }
    }
    catch (exception& ex)
    {
        m_s2Logger->LogError("%s Error: %s", __FUNCTION__, ex.what());
    }
    return true;
}
#ifdef SV_WINDOWS
void CS2Agent::StartSVAgents(unsigned long ulFlags)
{
    m_ioctlController->ServiceShutdownNotify(ulFlags);
    m_ioctlController->ProcessStartNotify(0);

    WaitForConsistentState(false);
}

void CS2Agent::SetDriverFlags(etBitOperation flag)
{
    m_ioctlController->SetDriverFlags(0, flag);
}

bool CS2Agent::SyncData()
{
    bool status = true;
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    std::string tag = boost::lexical_cast<std::string>(uuid);

    uuid = boost::uuids::random_generator()();
    std::string tagContext = boost::lexical_cast<std::string>(uuid);

    std::list<std::string> taglist;
    taglist.push_back(tag);

    CHAR    srcPath[MAX_PATH];
    CHAR    tgtPath[MAX_PATH];
    map<int, IBlockDevice*>::iterator it;

    for (it = m_sourceTargetMap.begin(); it != m_sourceTargetMap.end(); it++)
    {
        IBlockDevice* pSrcDisk = m_diskMap[it->first];
        IBlockDevice* pTargetDisk = it->second;
        if (!m_dataProcessorMap[it->first]->CreateDrainBarrierOnTag(tag))
        {
            m_loggerMap[it->first]->LogError("Tag %s is already received for disk %d",
                tag.c_str(), it->first);
            return false;
        }
    }

    for (int i = 0; i < 10; i++)
    {
        try
        {
            m_ioctlController->HoldWrites(tagContext,
                TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS, 3600 * 1000);
            m_ioctlController->InsertDistTag(tagContext,
                TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS, taglist);
            m_ioctlController->ReleaseWrites(tagContext, TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS);
            m_ioctlController->CommitTag(tagContext,
                TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS | TAG_INFO_FLAGS_COMMIT_TAG);
            status = true;
            break;
        }
        catch (exception& ex)
        {
            m_s2Logger->LogError("%s %s", __FUNCTION__, ex.what());
            status = false;
            boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        }
    }

    if (!status)
    {
        m_s2Logger->LogError("%s: failed to insert tag", __FUNCTION__);
        goto cleanup;
    }

    // Now get checksum of target
    for (it = m_sourceTargetMap.begin(); it != m_sourceTargetMap.end(); it++)
    {
        IBlockDevice* pSrcDisk = m_diskMap[it->first];
        IBlockDevice* pTargetDisk = it->second;
        if (!m_dataProcessorMap[it->first]->WaitForTag(tag, TIMEOUT_INFINITE, true))
        {
            status = false;
            break;
        }
    }

    if (!status)
    {
        m_s2Logger->LogInfo("%s: Error status is failure", __FUNCTION__);
        goto cleanup;
    }

cleanup:
    for (it = m_sourceTargetMap.begin(); it != m_sourceTargetMap.end(); it++)
    {
        m_s2Logger->LogInfo("%s: Releasing drain barrier for disk: %d", __FUNCTION__, it->first);
        m_dataProcessorMap[it->first]->ReleaseDrainBarrier();
    }

    return status;
}
#endif
