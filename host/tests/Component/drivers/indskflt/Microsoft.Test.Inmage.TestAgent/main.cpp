// This main.cpp is written keeping linux platform in mind.
#include<iostream>
#include "PairDetails.h"
#include "PlatformAPIs.h"
#include "CDiskDevice.h"
#include "CFileDevice.h"
#include "S2Agent.h"
#include "inmsafecapis.h"
#include "DataProcessor.h"
#include "boost/thread.hpp"

using namespace std;
using namespace PairInfo;

std::map<std::string, SubTestConfig_t*> validSubTestsMap;
std::map<unsigned long, unsigned long> chunk_drate_map;

/*
    "512k",
    "1k",
    "2k",
    "4k",
    "8k",
    "16k",
    "32k",
    "64k",
    "128k",
    "256k",
    "512k",
    "1mb",
    "4mb",
    "8mb",
    "9mb"
*/

const unsigned int TC_VAL = 2;

bool PairDetails::CreateDiskNameToNumMapping()
{
    cout << endl << "Entered " << __FUNCTION__ << endl;
    bool bRet = true;
    std::string strDiskPrefix = "/dev/sd";
    std::string strDisk;
    char buf[2] = { 0 };
    buf[0] = 'a';

    for (int i = 0, num = 0; i < 26; i++, num++)
    {
        //Repeat the loop for /dev/sdx
        memset((void*)buf, 0x00, sizeof(char));
        buf[0] = 'a' + i;;
        strDisk = strDiskPrefix + std::string((const char*)buf);
        m_mapDiskNameToDiskNum[strDisk] = num;

        //Repeat the loop for /dev/hdx
        if (num == 25)
        {
            num = 100;
            i = -1;
            strDiskPrefix = "/dev/hd";
        }

        //Need to extend this for volume groups or some other disk types
    }
    cout << endl << "Exited " << __FUNCTION__ << endl;
    return bRet;
}

int PairDetails::GetDiskNumFromDiskName(const std::string & strDiskName)
{
    cout << endl << "Entered " << __FUNCTION__ << endl;
    return m_mapDiskNameToDiskNum[strDiskName];
}

std::string PairDetails::GetDiskNameFromDiskNum(const int nDiskNum)
{
    cout << endl << "Entered " << __FUNCTION__ << endl;
    std::string strDiskName;
    std::map<std::string, int>::iterator iter = m_mapDiskNameToDiskNum.begin();
    while (iter != m_mapDiskNameToDiskNum.end())
    {
        if ((*iter).second == nDiskNum)
        {
            strDiskName = iter->second;
            break;
        }
        else
            iter++;
    }
    cout << endl << "Exited " << __FUNCTION__ << endl;
    return iter->first;
}

void PairDetails::SetPairType(PT pt)
{
    cout << endl << "Entered: " << __FUNCTION__ << endl;
    m_pairType = pt;
}

PT PairDetails::GetPairType()
{
    cout << endl << "Entered: " << __FUNCTION__ << endl;
    return m_pairType;
}

void PairDetails::SetLogFileName(std::string strLog)
{
    cout << endl << "Entered: " << __FUNCTION__ << endl;
    m_logFileName = strLog;
}

std::string PairDetails::GetLogFileName()
{
    cout << endl << "Entered: " << __FUNCTION__ << endl;
    return m_logFileName;
}

void PairDetails::SetSourceDevName(std::string sd)
{
    cout << endl << "Entered: " << __FUNCTION__ << endl;
    m_strSourceDeviceName = sd;
}

std::string PairDetails::GetSourceDevName()
{
    cout << endl << "Entered: " << __FUNCTION__ << endl;
    return m_strSourceDeviceName;
}

void PairDetails::SetTargetDevName(std::string td)
{
    cout << endl << "Entered: " << __FUNCTION__ << endl;
    m_strTargetDeviceName = td;
}

void PairDetails::SetSourceDevice(IBlockDevice* pSrcDev)
{
    cout << endl << "Entered: " << __FUNCTION__ << endl;
    if (pSrcDev)
        m_srcDevice = pSrcDev;
}

void PairDetails::SetSubTest(std::string st)
{
    cout << endl << "Entered: " << __FUNCTION__ << endl;
    m_strSubTest = st;
}

std::string PairDetails::GetSubTest()
{
    cout << endl << "Entered: " << __FUNCTION__ << endl;
    return m_strSubTest;
}

IBlockDevice* PairDetails::GetSourceDevice()
{
    cout << endl << "Entered: " << __FUNCTION__ << endl;
    return m_srcDevice;
}

void PairDetails::SetTargetDevice(IBlockDevice* pTgtDev)
{
    cout << endl << "Entered: " << __FUNCTION__ << endl;
    if (pTgtDev)
        m_tgtDevice = pTgtDev;
}

IBlockDevice* PairDetails::GetTargetDevice()
{
    cout << endl << "Entered: " << __FUNCTION__ << endl;
    return m_tgtDevice;
}

std::string PairDetails::GetTargetDevName()
{
    cout << endl << "Entered: " << __FUNCTION__ << endl;
    return m_strTargetDeviceName;
}

void usage()
{
    cout << endl << "Entered: " << __FUNCTION__ << endl;
    cout << endl << "indskflt_ct --tc=ditest \
                    \n\t--loggerPath=<log file path> \
                    \n\t--pair[ \
                    \n\t\t-type=d-f \
                    \n\t\t-sd=<source device name> \
                    \n\t\t-td=<target file path> \
                    \n\t\t-subtest=<sub test name> \
                    \n\t\t-log=<target log path> \
                    \n\t ] \
                    \n\n\t--pair[ \
                    \n\t\t-type=f-f \
                    \n\t\t-sd=<source file name> \
                    \n\t\t-td=<target file name> \
                    \n\t\t-subtest=<sub test name> \
                    \n\t\t-log=<target log path> \
                    \n\t ] \n \
                    \n\t One can add some more pairs as shown above. \
                    \n\t \"types\" can be one of d-d or d-f or f-f or f-d \
                    \n\ttarget file name will be appended with the <source device name> and ends with .tgt extension. \
                    \n\ttarget log file will have the <sourcedevice>.log file name";
}

void printValidSubTests()
{
    std::map<std::string, SubTestConfig_t* >::iterator iter;

    cout << "Valid sub tests are:\n";

    for(iter = validSubTestsMap.begin(); iter != validSubTestsMap.end(); iter++)
    {
        cout << iter->first << "\n";
    }
}


std::vector<PairDetails *>g_vPairDetails;
std::string g_strLoggerPath;

bool ParseCmdLineArgs(int argc, char* argv[])
{
    cout << endl << "Entered: " << __FUNCTION__ << endl;
    if (argc < 1)
    {
        cout << endl << "No command line arguments specified. Hence exiting." << endl;
        return false;
    }

    //usage();
    //Read all the arguments passed on the command line
    std::vector<std::string> vArgs;
    std::string strArg;
    int argIndex = 0;
    for (int i = 1; i < argc; i++)//start filling from the first argument onwards
    {
        strArg = argv[i];
        vArgs.push_back(strArg);
    }

    //Now parse the arguments
    strArg = "";
    std::string strPairType;
    std::string strSourceDevName;
    std::string strTargetDevName;
    std::string strSubTest;
    std::string strLogFileName;
    int index = 0;
    bool bParseResult = false;
    TC testCase = TC_NONE;

    while (index < (argc - 1))//start reading from the first argument
    {
        strArg = vArgs[index++];
        if ((0 == strArg.compare("--tc=ditest")) ||
            (0 == strArg.compare("--tc=barrierhonour")) ||
            (0 == strArg.compare("--tc=barrierhonourwithouttag")) ||
            g_vPairDetails.size() > 0)
        {
            if (0 == strArg.compare("--tc=ditest"))
            {
                testCase = TC_DI;
            }
            else if (0 == strArg.compare("--tc=barrierhonour"))
            {
                testCase = TC_BARRIER_HONOUR;
            }
            else if (0 == strArg.compare("--tc=barrierhonourwithouttag"))
            {
                testCase = TC_BARRIER_HONOUR_WITHOUT_TAG;
            }
            if (g_vPairDetails.size() == 0)
            {
                strArg = vArgs[index++];
                if (strArg.compare("--loggerPath=") > 0)
                {
                    g_strLoggerPath = strArg.substr(13, strArg.size());
                }
                strArg = vArgs[index++];
            }
            if (0 == strArg.compare("--pair["))
            {
                PairDetails *pd = new PairDetails(testCase);
                strArg = vArgs[index++];
                if (strArg.compare("-type=") > 0)
                {
                    strPairType = strArg.substr(6, strArg.size());
                    if (0 == strPairType.compare("d-f"))
                    {
                        pd->SetPairType(PT_D2F);
                    }
                    else if (0 == strPairType.compare("d-d"))
                    {
                        pd->SetPairType(PT_D2D);
                    }
                    else if (0 == strPairType.compare("f-d"))
                    {
                        pd->SetPairType(PT_F2D);
                    }
                    else if (0 == strPairType.compare("f-f"))
                    {
                        pd->SetPairType(PT_F2F);
                    }
                    else
                    {
                        cout << endl << "Invalid Pair Type found." << endl;
                        bParseResult = false;
                        break;
                    }
                }

                //Read Source Device Name
                strArg = vArgs[index++];
                if (strArg.compare("-sd=") > 0)
                {
                    strSourceDevName = strArg.substr(4, strArg.size());
                    pd->SetSourceDevName(strSourceDevName);
                }
                else
                {
                    cout << endl << "Source Device not found in the command line." << endl;
                    bParseResult = false;
                    break;
                }

                //Read Target Device Name
                strArg = vArgs[index++];
                if (strArg.compare("-td=") > 0)
                {
                    strTargetDevName = strArg.substr(4, strArg.size());
                    pd->SetTargetDevName(strTargetDevName);
                }
                else
                {
                    cout << endl <<
                        "TargetDevice's name or path is not found in the command line." << endl;
                    bParseResult = false;
                    break;
                }

                //Read data load type
                strArg = vArgs[index++];
                if (strArg.compare("-subtest=") > 0)
                {
                    strSubTest = strArg.substr(9, strArg.size());
                    pd->SetSubTest(strSubTest);

                    std::map<std::string, SubTestConfig_t* >::iterator iter =
                        validSubTestsMap.find(strSubTest);
                    if (iter != validSubTestsMap.end())
                    {
                        pd->subtestConfig = validSubTestsMap[strSubTest];
                        pd->maxBytesPerSec = chunk_drate_map[pd->subtestConfig->chunkSize];
                    }
                    else
                    {
                        cout << endl << "SubTest name "<<strSubTest << " is not valid" << endl;
                        printValidSubTests();
                        bParseResult = false;
                        break;
                    }
                }
                else
                {
                    cout << endl << "SubTest name not found in the command line." << endl;
                    bParseResult = false;
                    break;
                }

                //Read pair's log file
                strArg = vArgs[index++];
                if (strArg.compare("-log=") > 0)
                {
                    strLogFileName = strArg.substr(5, strArg.size());
                    pd->SetLogFileName(strLogFileName);
                    bParseResult = true;
                    g_vPairDetails.push_back(pd);
                }
                else
                {
                    cout << endl << "Log file name is not found in the command line." << endl;
                    bParseResult = false;
                    break;
                }
                //Read closing ]
                strArg = vArgs[index++];
                if (0 == strArg.compare("]"))
                {
                    cout << endl <<
                        "Parsed the details of one replication pair successfully." << endl;
                }
                else
                {
                    cout << endl <<
                        "Closing ] is not found on the command line for one of the pair details."
                         << endl;
                    bParseResult = false;
                    break;
                }
            }
            else
            {
                bParseResult = false;
                break;
            }
        }
    }
    if (!bParseResult)
    {
        cout << endl << "Invalid command line arguments. Check the usage:" << endl;
        usage();
    }
    return bParseResult;
}

#define SOFFSET 4096

void GenerateSeqWrites(PairDetails *pd)
{
    string srcDiskName = pd->GetSourceDevName();
    unsigned long long chunkSize = pd->subtestConfig->chunkSize;
    //issue direct IO to source disk
    CDiskDevice* srcDisk = new CDiskDevice(srcDiskName, new PlatformAPIs(true));
    unsigned long long devBlockSize = srcDisk->GetDevBlockSize();
    char* buffer = NULL;
    unsigned long long offset = SOFFSET;
    unsigned long long devSize = srcDisk->GetDeviceSize();
    unsigned long dwBytes;
    int ret = 0;
    unsigned long totalbytespersec = 0;
    boost::posix_time::ptime tick;
    boost::posix_time::time_duration elapsedTime;
    char pattern = 'a';
    int numWritesForBitmapMode = 0;

    if ((chunkSize % devBlockSize) != 0)
        throw CInmageDataThreadException(
            "%s: Chunk size %llu is not multiple of device block size %llu",
            __FUNCTION__, chunkSize, devBlockSize);

    ret = posix_memalign((void **)&buffer, devBlockSize, chunkSize);
    if (ret != 0)
        throw CInmageDataThreadException(
            "%s: Failed to allocate aligned buffer, chunk size %llu, device block size %llu",
            __FUNCTION__, chunkSize, devBlockSize);

    cout << __FUNCTION__ <<
        ": Starting DI test with Seq IO, chunk size: " << chunkSize <<
        ", data rate: " << pd->maxBytesPerSec << "\n";

    memset(buffer, pattern, chunkSize);
    tick = boost::posix_time::microsec_clock::local_time();

    try
    {
        while (!pd->testComplete)
        {
            if (pd->pauseDataThread) {
                cout << __FUNCTION__ << "Pause Data Thread is set, sleeping for 5s" << endl;
                boost::this_thread::sleep(boost::posix_time::seconds(5));
                continue;
            }

            if (chunkSize > pd->maxBytesPerSec) {
                cout << __FUNCTION__ <<
                    ": Churn size is bigger than maximum Data rate, not issuing writes\n";
                break;
            }

            assert(offset < devSize);

            srcDisk->SeekFile(BEGIN, offset);

            dwBytes = 0;
            ret = srcDisk->Write(buffer, chunkSize, dwBytes);
            if (!ret) {
                cout << __FUNCTION__ << ": Write to " << srcDiskName << " failed" << endl;
                break;
            }
            if (dwBytes != chunkSize)
                cout << __FUNCTION__ << ": Requested write for " << chunkSize <<
                    " bytes, but written " << dwBytes << " bytes\n";

            totalbytespersec += chunkSize;

            if ((totalbytespersec + chunkSize) >= pd->maxBytesPerSec) {
                bool success = srcDisk->Fsync();
                if (!success)
                    cout << __FUNCTION__ << ": Fsync failed\n";
                elapsedTime = boost::posix_time::microsec_clock::local_time() - tick;
                long millisecs = elapsedTime.total_milliseconds();
                if (millisecs < 1000) {
                    boost::this_thread::sleep(boost::posix_time::milliseconds(1000 - millisecs));
                }
                totalbytespersec = 0;
                tick = boost::posix_time::microsec_clock::local_time();
            }
            offset += dwBytes;
            if (offset >= devSize) {
                offset = SOFFSET;
                pattern++;
                memset(buffer, pattern, chunkSize);
                cout << __FUNCTION__ << ": Writing Pattern " << pattern << endl;
            }
        }
    }
    catch (exception& ex)
    {
        printf("%s", ex.what());
        exit(1);  // Failed, exit process to return error code to test framework
    }

    cout << endl << "Exited " << __FUNCTION__ << endl;
}

void GenerateRandomWrites(PairDetails *pd)
{
    string srcDiskName = pd->GetSourceDevName();
    unsigned long long chunkSize = pd->subtestConfig->chunkSize;
    //issue direct IO to source disk
    CDiskDevice* srcDisk = new CDiskDevice(srcDiskName, new PlatformAPIs(true));
    unsigned long long devBlockSize = srcDisk->GetDevBlockSize();
    char* buffer = NULL;
    unsigned long long offset = 0;
    unsigned long long devSize = srcDisk->GetDeviceSize();
    unsigned long dwBytes;
    int ret = 0;
    unsigned long totalbytespersec = 0;
    boost::posix_time::ptime tick;
    boost::posix_time::time_duration elapsedTime;
    char pattern = 'a';
    int numWritesForBitmapMode = 0;

    if ((chunkSize % devBlockSize) != 0)
        throw CInmageDataThreadException(
            "%s: Chunk size %llu is not multiple of device block size %llu",
            __FUNCTION__, chunkSize, devBlockSize);

    ret = posix_memalign((void **)&buffer, devBlockSize, chunkSize);
    if (ret != 0)
        throw CInmageDataThreadException(
            "%s: Failed to allocate aligned buffer, chunk size %llu, device block size %llu",
            __FUNCTION__, chunkSize, devBlockSize);

    cout << __FUNCTION__ << ": Starting DI test with Random IO, chunk size: " <<
        chunkSize << ", data rate: " << pd->maxBytesPerSec << "\n";

    boost::mt19937 rng;
    boost::uniform_int<unsigned long long> dist((SOFFSET/devBlockSize), (devSize/devBlockSize));
    boost::variate_generator<boost::mt19937, boost::uniform_int<unsigned long long> > 
                                            offsetgen(rng, dist);
    try
    {
        tick = boost::posix_time::microsec_clock::local_time();
        while (!pd->testComplete)
        {
            if (pd->pauseDataThread) {
                cout << __FUNCTION__ << "Pause Data Thread is set, sleeping for 5s" << endl;
                boost::this_thread::sleep(boost::posix_time::seconds(5));
                continue;
            }

            if (chunkSize > pd->maxBytesPerSec) {
                cout << __FUNCTION__ <<
                    ": Churn size is bigger than maximum Data rate, not issuing writes\n";
                break;
            }

            offset = offsetgen() * devBlockSize;
            assert(offset < devSize);

            memset(buffer, pattern, chunkSize);

            srcDisk->SeekFile(BEGIN, offset);

            dwBytes = 0;
            ret = srcDisk->Write(buffer, chunkSize, dwBytes);
            if (!ret) {
                cout << __FUNCTION__ << ": Write to " << srcDiskName << " failed" << endl;
                break;
            }
            if (dwBytes != chunkSize)
                cout << __FUNCTION__ << ": Requested write for " << chunkSize <<
                    " bytes, but written " << dwBytes << " bytes\n";

            totalbytespersec += chunkSize;

            if ((totalbytespersec + chunkSize) >= pd->maxBytesPerSec) {
                bool success = srcDisk->Fsync();
                if (!success)
                    cout << __FUNCTION__ << ": Fsync failed\n";
                elapsedTime = boost::posix_time::microsec_clock::local_time() - tick;
                long millisecs = elapsedTime.total_milliseconds();
                if (millisecs < 1000) {
                    boost::this_thread::sleep(boost::posix_time::milliseconds(1000 - millisecs));
                }
                totalbytespersec = 0;
                tick = boost::posix_time::microsec_clock::local_time();
            }

            pattern++;
        }
    }
    catch (exception& ex)
    {
        printf("%s", ex.what());
        exit(1);  // Failed, exit process to return error code to test framework
    }

    cout << endl << "Exited " << __FUNCTION__ << endl;
}


void GenerateMixedWrites(PairDetails *pd)
{
    string srcDiskName = pd->GetSourceDevName();
    //issue direct IO to source disk
    CDiskDevice* srcDisk = new CDiskDevice(srcDiskName, new PlatformAPIs(true));
    unsigned long devBlockSize = srcDisk->GetDevBlockSize();
    char* buffer = NULL;
    unsigned long offset = SOFFSET;
    unsigned long devSize = srcDisk->GetDeviceSize();
    unsigned long dwBytes;
    unsigned long totalbytespersec = 0;
    int ret = 0;
    boost::posix_time::ptime chunkSizeTick;
    boost::posix_time::ptime dataRateTick;
    boost::posix_time::time_duration chunkSizeElapsedTime;
    boost::posix_time::time_duration dataRateElapsedTime;
    char pattern = 'a';
    bool issueRandomIO = true; //flips periodically
    unsigned long maxChunkSize = 9 * 1048576;
    int numWritesForBitmapMode = 0;
    std::map<unsigned long, unsigned long>::iterator iter = chunk_drate_map.begin();

    unsigned long chunkSize = iter->first;
    unsigned long mbps = iter->second;
    if (chunkSize < devBlockSize) {
        chunkSize = devBlockSize;
        mbps = chunk_drate_map[devBlockSize];
    }


    ret = posix_memalign((void **)&buffer, devBlockSize, maxChunkSize);
    if (ret != 0)
        throw CInmageDataThreadException(
            "%s: Failed to allocate aligned buffer, chunk size %llu, device block size %llu",
            __FUNCTION__, chunkSize, devBlockSize);


    boost::mt19937 rng;
    boost::uniform_int<unsigned long long> dist((SOFFSET/devBlockSize), (devSize/devBlockSize));
    boost::variate_generator<boost::mt19937, boost::uniform_int<unsigned long long> > 
                                            offsetgen(rng, dist);
    printf("\n%s: Starting with %s IO and chunk size %llu at data rate %llu\n", __FUNCTION__,
                        (issueRandomIO ? "Random" : "Seq"), chunkSize, mbps);
    try
    {
        chunkSizeTick = boost::posix_time::microsec_clock::local_time();
        dataRateTick = boost::posix_time::microsec_clock::local_time();
        while (!pd->testComplete)
        {
            if (pd->pauseDataThread) {
                cout << __FUNCTION__ << "Pause Data Thread is set, sleeping for 5s" << endl;
                boost::this_thread::sleep(boost::posix_time::seconds(5));
                continue;
            }

            if ((chunkSize % devBlockSize) != 0)
                throw CInmageDataThreadException(
                    "%s: Chunk size %llu is not multiple of device block size %llu",
                    __FUNCTION__, chunkSize, devBlockSize);

            if (chunkSize > mbps)
                throw CInmageDataThreadException(
                    "%s: Chunk size %llu is larger than Data rate %llu",
                    __FUNCTION__, chunkSize, mbps);

            if (issueRandomIO)
                offset = offsetgen() * devBlockSize;
            else {
                offset = (offset + chunkSize) % devSize;
                if (offset <= 0)
                    offset = SOFFSET;
            }
            assert(offset < devSize);

            memset(buffer, pattern, maxChunkSize);

            srcDisk->SeekFile(BEGIN, offset);

            dwBytes = 0;
            ret = srcDisk->Write(buffer, chunkSize, dwBytes);
            if (!ret) {
                cout << __FUNCTION__ << ": Write to " << srcDiskName << " failed" << endl;
                break;
            }
            if (dwBytes != chunkSize)
                cout << __FUNCTION__ << ": Requested write for " << chunkSize <<
                    " bytes, but written " << dwBytes << " bytes\n";

            totalbytespersec += chunkSize;

            if (!pd->forceBitmapMode)
            {
                if ((totalbytespersec + chunkSize) >= mbps) {
                    bool success = srcDisk->Fsync();
                    if (!success)
                        cout << __FUNCTION__ << ": Fsync failed\n";
                    dataRateElapsedTime = boost::posix_time::microsec_clock::local_time() - dataRateTick;
                    long millisecs = dataRateElapsedTime.total_milliseconds();
                    if (millisecs < 1000) {
                        boost::this_thread::sleep(boost::posix_time::milliseconds(1000 - millisecs));
                    }
                    totalbytespersec = 0;
                    dataRateTick = boost::posix_time::microsec_clock::local_time();
                }
            } else {
                numWritesForBitmapMode++;
                // Need to issue min HIGH_WATER_MARK writes to force bitmap mode, but our 
                // each write is split into multiple writes by the time driver receives
                if (!pd->doneForceBitmapMode && (numWritesForBitmapMode > 4000))
                {
                    bool success = srcDisk->Fsync();
                    if (!success)
                        cout << __FUNCTION__ << ": Fsync failed\n";
                    pd->doneForceBitmapMode = true;
                    cout << __FUNCTION__ << ": Issued " <<
                        numWritesForBitmapMode << " to force bitmap mode\n";
                }
            }

            pattern++;

            if (pd->forceBitmapMode) {
                chunkSize = 8 * 1048576;
                issueRandomIO = true;
                // while forcing bitmap mode issues at full speed, mbps used after bitmap mode
                mbps = 10 * 1048576;
            }
            else
            {
                chunkSizeElapsedTime =
                    boost::posix_time::microsec_clock::local_time() - chunkSizeTick;
                long seconds = chunkSizeElapsedTime.total_seconds();
                if (seconds > 30) {
                    iter++;
                    if (iter == chunk_drate_map.end()) {
                        iter = chunk_drate_map.begin();
                        issueRandomIO = !issueRandomIO;
                    }
                    chunkSize = iter->first;
                    mbps = iter->second;

                    printf("\n%s: Issuing %s IO with chunk size %llu at data rate %llu\n",
                        __FUNCTION__, (issueRandomIO ? "Random" : "Seq"), chunkSize, mbps);
                    chunkSizeTick = boost::posix_time::microsec_clock::local_time();
                    dataRateTick = boost::posix_time::microsec_clock::local_time();
                }
            }
        }
    }
    catch (CBlockDeviceException& ex)
    {
        printf("%s CBlockDeviceException : %s", __FUNCTION__, ex.what());
        exit(1);  // Failed, exit process to return error code to test framework
    }

    cout << endl << "Exited " << __FUNCTION__ << endl;
}

void DItest_Cleanup(CS2Agent &s2Agent)
{
    cout << endl << "Entered " << __FUNCTION__ << endl;

    std::vector<PairDetails *>::iterator iter = g_vPairDetails.begin();
    while (iter != g_vPairDetails.end())
    {
        PairDetails *pd = (*iter);
        pd->pauseDataThread = false;
        pd->testComplete = true;  // stop the data thread
        pd->dataThread.join();
        iter++;
    }
    s2Agent.StopFiltering();
    sleep(10); // wait for stop filtering

    cout << endl << "Exited " << __FUNCTION__ << endl;
}

bool DItest_Validate(CS2Agent &s2Agent, TC testCase)
{
    cout << endl << "Entered " << __FUNCTION__ << endl;
    bool ret = s2Agent.WaitForDataMode();
    if (!ret)
    {
        return false;
    }

    cout << endl << "Validating the replicated disk..." << endl;
    if (testCase == TC_DI)
    {
        ret = s2Agent.Validate();
    }
    else if (testCase == TC_BARRIER_HONOUR)
    {
        ret = s2Agent.BarrierHonour(g_vPairDetails);
    }
    else if (testCase == TC_BARRIER_HONOUR_WITHOUT_TAG)
    {
        ret = s2Agent.BarrierHonour(g_vPairDetails, false);
    }

    if (ret)
    {
        printf("\nDI Test Passed\n");
    }
    else
    {
        printf("\nDI Test Failed\n");
    }

    cout << endl << "Exited " << __FUNCTION__ << endl;
    return ret;
}

int DItest_with_directIO()
{
    bool test_fail = false;
    bool forceBitmapMode = false;
    bool doIR = true;

    try
    {
        cout << endl << "Creating S2Agent object..." << endl;
        CS2Agent s2Agent(DataReplication, g_strLoggerPath.c_str());

        std::vector<PairDetails *>::iterator iter = g_vPairDetails.begin();
        while (iter != g_vPairDetails.end())
        {
            PairDetails *pd = (*iter);
            cout << endl << "Test Type : " << pd->m_testCase << endl;
            pd->testComplete = false;
            pd->doneForceBitmapMode = false;
            pd->forceBitmapMode = false;
            if (PT_D2F == pd->GetPairType())
            {
                if (pd->subtestConfig->subtestType == DIWITHOUTIR)
                {
                    forceBitmapMode = false;
                    doIR = false;
                    pd->dataThread = boost::thread(GenerateMixedWrites, pd);
                }
                else
                {
                    if (pd->subtestConfig->ioType == MIXED)
                    {
                        pd->dataThread = boost::thread(GenerateMixedWrites, pd);
                        forceBitmapMode = true;
                    }
                    else
                    {
                        if (pd->subtestConfig->ioType == SEQ)
                            pd->dataThread = boost::thread(GenerateSeqWrites, pd);
                        else
                            pd->dataThread = boost::thread(GenerateRandomWrites, pd);
                    }
                }

                int nSrcDiskNum = pd->GetDiskNumFromDiskName(pd->GetSourceDevName());
                s2Agent.AddReplicationSettings(nSrcDiskNum,
                    (pd->GetTargetDevName()).c_str(), (pd->GetLogFileName()).c_str());

                //Get the source device size
                unsigned long long ullSrcDevSize = (s2Agent.GetDisk(nSrcDiskNum))->GetDeviceSize();
                //Start Replication with IR
                bool ret = s2Agent.StartReplication(0, ullSrcDevSize, doIR);
                if (!ret)
                {
                    throw CAgentException("\n%s: StartReplication failed\n", __FUNCTION__);
                }
            }
            else if (PT_D2D == pd->GetPairType())
            {
                int nSrcDiskNum = pd->GetDiskNumFromDiskName(pd->GetSourceDevName());
                int nTgtDiskNum = pd->GetDiskNumFromDiskName(pd->GetTargetDevName());
                string strLogFile = pd->GetLogFileName();
                s2Agent.AddReplicationSettings(nSrcDiskNum, nTgtDiskNum, strLogFile.c_str());

                //Get the source device size
                unsigned long long ullSrcDevSize = (s2Agent.GetDisk(nSrcDiskNum))->GetDeviceSize();
                //Start Replication with IR
                bool ret = s2Agent.StartReplication(0, ullSrcDevSize, true);
                if (!ret)
                {
                    throw CAgentException("\n%s: StartReplication failed\n", __FUNCTION__);
                }
            }

            iter++;
        }

        if (forceBitmapMode)
        {
            printf("\nForce bitmap mode is set. Pause Draining to switch to bitmap mode\n");
            s2Agent.PauseDraining();
            for (iter = g_vPairDetails.begin(); iter != g_vPairDetails.end(); iter++)
            {
                PairDetails *pd = (*iter);
                pd->doneForceBitmapMode = false;
                pd->forceBitmapMode = true;
                // Wait till driver switch to bitmap mode
                while(!s2Agent.VerifyBitmapMode())
                {
                    boost::this_thread::sleep(boost::posix_time::seconds(30));
                }

                printf("\nVerified bitmap mode using volume stats.\n");
                pd->doneForceBitmapMode = true;
                pd->forceBitmapMode = false;
            }
            printf("\nAllow Draining to recover from bitmap mode\n");
            s2Agent.AllowDraining();
        }

        // Wait for driver switch back to data mode
        TC testCase = TC_NONE;
        for (iter = g_vPairDetails.begin(); iter != g_vPairDetails.end(); iter++)
        {
            PairDetails *pd = (*iter);
            testCase = pd->m_testCase;
            // Wait for driver switch back to data mode
            if (pd->subtestConfig->subtestType != DIWITHOUTIR)
            {
                pd->pauseDataThread = true;
            }
        }

        bool ret = DItest_Validate(s2Agent, testCase);
        if (!ret)
        {
            test_fail = true;
        }
        DItest_Cleanup(s2Agent);

        cout << "\nExiting " << __func__ << endl;
    }
    catch (exception& ex)
    {
        printf("%s", ex.what());
        test_fail = true;
        exit(1);  // Failed, exit process to kill data thread
    }

    return test_fail;
}

int main(int argc, char* argv[])
{
    bool test_failed = false;

    // chunsize varies in mixed workload
    validSubTestsMap["mixed"] = new SubTestConfig_t(DITEST, 4096, MIXED);
    validSubTestsMap["4k_seq"] = new SubTestConfig_t(DITEST, 4096, SEQ);
    validSubTestsMap["4k_random"] = new SubTestConfig_t(DITEST, 4096, RANDOM);
    validSubTestsMap["16k_seq"] = new SubTestConfig_t(DITEST, 16*1024, SEQ);
    validSubTestsMap["16k_random"] = new SubTestConfig_t(DITEST, 16*1024, RANDOM);
    validSubTestsMap["64k_seq"] = new SubTestConfig_t(DITEST, 64*1024, SEQ);
    validSubTestsMap["64k_random"] = new SubTestConfig_t(DITEST, 64*1024, RANDOM);
    validSubTestsMap["512k_seq"] = new SubTestConfig_t(DITEST, 512*1024, SEQ);
    validSubTestsMap["512k_random"] = new SubTestConfig_t(DITEST, 512*1024, RANDOM);
    validSubTestsMap["1mb_seq"] = new SubTestConfig_t(DITEST, 1024*1024, SEQ);
    validSubTestsMap["1mb_random"] = new SubTestConfig_t(DITEST, 1024*1024, RANDOM);
    validSubTestsMap["4mb_seq"] = new SubTestConfig_t(DITEST, 4*1024*1024, SEQ);
    validSubTestsMap["4mb_random"] = new SubTestConfig_t(DITEST, 4*1024*1024, RANDOM);
    validSubTestsMap["8mb_seq"] = new SubTestConfig_t(DITEST, 8*1024*1024, SEQ);
    validSubTestsMap["8mb_random"] = new SubTestConfig_t(DITEST, 8*1024*1024, RANDOM);
    validSubTestsMap["9mb_seq"] = new SubTestConfig_t(DITEST, 9*1024*1024, SEQ);
    validSubTestsMap["9mb_random"] = new SubTestConfig_t(DITEST, 9*1024*1024, RANDOM);
    validSubTestsMap["DItestwithoutIR"] = new SubTestConfig_t(DIWITHOUTIR, 4096, MIXED);

    chunk_drate_map[4 * 1024] = 512 * 1024;
    chunk_drate_map[8 * 1024] = 1048576;
    chunk_drate_map[16*1024] = 1048576;
    chunk_drate_map[32*1024] = 1048576;
    chunk_drate_map[64*1024] = 2 * 1048576;
    chunk_drate_map[128*1024] = 2 * 1048576;
    chunk_drate_map[256*1024] = 2 * 1048576;
    chunk_drate_map[512*1024] = 3 * 1048576;
    chunk_drate_map[1048576] = 5 * 1048576;
    chunk_drate_map[4*1048576] = 10 * 1048576;
    chunk_drate_map[8*1048576] = 10 * 1048576;
    chunk_drate_map[9*1048576] = 10 * 1048576;

    cout << endl << "Entered: " << __FUNCTION__ << endl;
    cout << endl << "Disk Filter CVT: DI test case." << endl;
    if (ParseCmdLineArgs(argc, argv))
    {
        cout << endl << "Successfully parsed the command line arguments." << endl;
    }
    else
    {
        cout << endl << "Failed to parse the command line arguments." << endl;
        return -1;
    }

    test_failed = DItest_with_directIO();

    if (test_failed)
        cout << "\nTest failed\n";
    else
        cout << "\nTest passed\n";

    std::vector<PairDetails *>::iterator iter = g_vPairDetails.begin();
    while (iter != g_vPairDetails.end())
    {
        PairDetails *pd = (*iter);
        delete pd->subtestConfig;
        delete pd;
        iter++;
    }

    if (test_failed)
        return -1;

    return 0;
}
