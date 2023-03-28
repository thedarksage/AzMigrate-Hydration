#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <uuid/uuid.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/prctl.h>

#include <sys/ioctl.h>
#include <iostream>
#include <vector>
#include <map>
#include <string>

#include "ace/os_include/os_fcntl.h"
#include "ace/OS.h"
#include "ace/Process_Manager.h"

#include <boost/thread/thread.hpp>
#include <boost/regex.hpp>
#include "boost/chrono.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "event.h"
#include "vacpunix.h"
#include "devicefilter.h"
#include "localconfigurator.h"
#include "portable.h"
#include "logger.h"
#include "StreamEngine.h"
#include "VacpUtil.h"
#include "configurator2.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "host.h"
#include "VacpErrorCodes.h"

#include "common/Trace.h"
#include "AADAuthProvider.h"
#include "VacpConf.h"
#include "VacpConfDefs.h"
#include "createpaths.h"

#include "setpermissions.h"
#include "utilfdsafeclose.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"

using namespace std;
using namespace boost::chrono;
using namespace AADAuthLib;
using namespace TagTelemetry;
using namespace TagTelemetry::NsVacpOutputKeys;

Configurator* TheConfigurator = 0; // singleton

const unsigned short INM_UUID_STR_LEN = 36;

// Command line options
//
#define OPTION_VOLUME					"v"
#define OPTION_TAGNAME					"t"
#define OPTION_EXCLUDEAPP				"x"
#define OPTION_HELP					    "h"
#define OPTION_REMOTE					"remote"
#define OPTION_SERVER_IP				"serverip"
#define OPTION_SERVER_PORT				"serverport"
#define OPTION_SERVERDEVICE				"serverdevice"
#define DEFAULT_SERVER_PORT				20003
#define OPTION_SYNC_TAG					"sync"
#define OPTION_TAG_TIMEOUT              "tagtimeout"
#define OPTION_CRASH_CONSISTENCY        "cc"

#define COMMAND_DMSETUP					"dmsetup"
#define OPTION_SUSPEND					"suspend"
#define OPTION_RESUME					"resume"
#define SEPARATOR_COMMA					","
#define SEPARATOR_COLON					":"
#define OPTION_PRESCRIPT                "prescript"
#define OPTION_POSTSCRIPT               "postscript"
#define OPTION_SYSTEMLEVEL				"systemlevel"
#define OPTION_DISTRIBUTED_VACP         "distributed"
#define OPTION_MASTER_NODE              "mn"
#define OPTION_COORDINATOR_NODES        "cn"
#define OPTION_DISTRIBUTED_PORT         "dport"
#define OPTION_VERBOSE                  "verbose"
#define OPTION_BASELINE                 "baseline"
#define OPTION_TAGGUID                  "tagguid"
#define OPTION_PARALLEL_RUN             "parallel"
#define OPTION_VTOA_MODE                "vtoa"
#define OPTION_IR_MODE                  "irmode"
#define OPTION_MASTER_NODE_FINGERPRINT  "mnf"
#define OPTION_RCM_SETTINGS_PATH        "rcmSetPath"
#define OPTION_PROXY_SETTINGS_PATH      "proxySetPath"


#define TIMESTAMPLEN            15
#define MAX_TAG_LENGTH          200
#define MAX_LENGTH_OF_ALL_TAGS  2048

#define VACP_TAG_IOBARRIER_TIMEOUT   300 // in ms
#define VACP_TAG_IOCTL_COMMIT_TIMEOUT  40000 // in ms

// for single node crash tag 
#define VACP_CRASHTAG_MAX_RETRY_TIME   10000 // in ms, 10 sec
#define VACP_CRASHTAG_RETRY_WAIT_TIME  1000 // in ms, 1 sec

#define VACP_TAG_IOCTL_MAX_RETRY_TIME   500 // in ms
#define VACP_TAG_IOCTL_RETRY_WAIT_TIME  100

// vacp should not write to any filesystem as that could have been frozen by the vacp
// use cout/inm_printf for any messages that are to be printed on console and inm_printf for the debug messages.

#define DebugPrintf inm_printf

#ifdef DEBUG
#define XDebugPrintf inm_printf
#define XDebugWPrintf wprintf
#else
#define XDebugPrintf
#define XDebugWPrintf
#endif

extern int inm_init_file_log(const std::string&);
extern void inm_printf(const char* format, ... );
extern void inm_printf(short logLevel, const char* format, ... );
extern void inm_log_to_buffer_start();
extern void inm_log_to_buffer_end();
extern void NextLogFileName(const std::string& logFilePath,
    std::string& preTarget,
    std::string& target,
    const std::string& prefix = std::string(""));
extern bool RenameLogFile(const std::string& logFilePath, const std::string& newLogFilePath, std::string& err);

std::string g_strLogFilePath;

extern bool GetIPAddressSetFromNicInfo(strset_t&, std::string&);

bool GetFSTagSupportedVolumes(std::set<std::string> & supportedFsVolumes);

bool ParseVolumeList(set<string>& sVolumes ,char* tokens, const char* seps);
bool ParseVolume(std::set<std::string> &volumes, std::set<std::string> &disks, char *token);
string uuid_gen(void);


CrashConsistency* CrashConsistency::m_theCrashC = NULL;
boost::mutex CrashConsistency::m_theCrashCLock;

AppConsistency* AppConsistency::m_theAppC = NULL;
boost::mutex AppConsistency::m_theAppCLock;

BaselineConsistency* BaselineConsistency::m_theBaseline = NULL;
boost::mutex BaselineConsistency::m_theBaselineLock;

boost::shared_ptr<Event> g_Quit;

static const std::string CommunicatorTypeSched = "Scheduler";

FailMsgList g_VacpFailMsgsList;
FailMsgList g_MultivmFailMsgsList;
VacpLastError g_vacpLastError;

unsigned long long        g_lastAppScheduleTime;
unsigned long long        g_lastCrashScheduleTime;
std::string               g_strScheduleFilePath;

const std::string AppPolicyLogsPath = "//ApplicationPolicyLogs//";
const std::string EtcPath = "//etc";
const std::string ConsistencyScheduleFile = "consistencysched.json";
const std::string LastAppConsistencyScheduleTime = "LastAppConsistencyScheduleTime";
const std::string LastCrashConsistencyScheduleTime = "LastCrashConsistencyScheduleTime";

//Configurator* cxConfigurator = NULL;
int32_t vacpServSocket = -1;
struct sockaddr_in vacpServer;
bool gbRemoteConnection = false;
bool gbRemoteSend = false;
bool gbPreScript = false;
bool gbPostScript = false;
bool gbSystemLevel = false;
set<string> gInputVolumes;
set<string> gVolumesToBeTagged;
std::set<std::string> gDiskList;
std::set<std::string> gProtectedDiskList;
string gstrPreScript;
string gstrPostScript;
bool gbFSTag = true;
u_short gVacpServerPort = DEFAULT_SERVER_PORT;
string gstrVacpServer;
int gwait_time = 0;
bool gbCrashTag = false;

enum VACP_LOG_LEVEL 
{
    VACP_LOG_INFO = 1,
    VACP_LOG_DEBUG = 2,
};

short gsVerbose;

bool gbissueSyncTags = false;

bool gbBaselineTag = false;
std::string gStrBaselineId;
std::string gStrTagGuid;

set<string> gInputTags;

typedef struct _VACP_SERVER_RESPONSE_ 
{
    int iResult; // return value of server success or failure
    int val;	// Success or Failure code
    char str[1024];  //string related to val		
}VACP_SERVER_RESPONSE;

#define INVOLFLT_FEATURE_MASK_TAGV2 0x2
#define INVOLFLT_FEATURE_MASK_IOBARRIER 0x2
DRIVER_VERSION gdriverVersion;

bool g_bDistributedVacp = false;
bool g_bMasterNode = false;
bool g_bCoordNodes = false;
bool g_bThisIsMasterNode = false;
bool g_bDistMasterFailedButHasToFacilitate = false;
bool g_bDistributedVacpFailed = false;
ServerCommunicator* g_pServerCommunicator = NULL;
extern unsigned short g_DistributedPort;
bool g_bDistributedTagRevokeRequest = false;


std::vector<string> g_vCoordinatorNodes;
std::vector<string> g_vCoordinatorNodeLevels;
std::vector<string> g_vLocalHostIps;
std::string g_strLocalIpAddress;
std::string g_strLocalHostName;
std::string g_strMasterHostName;

bool gbParallelRun = false;
bool gbVToAMode = false;
bool gbIRMode = false;
unsigned long gExitCode = 0;

VacpConfigPtr VacpConfig::m_pVacpConfig;

uint32_t gnParallelRunScheduleCheckInterval = 10; // in secs
uint32_t gnRemoteScheduleCheckInterval = 30; // in secs

char * g_cstrVolumes = NULL;
boost::mutex    g_diskInfoMutex;

std::string     g_strMasterNodeFingerprint;
std::string     g_strRcmSettingsPath;
std::string     g_strProxySettingsPath;

std::string     g_strCommandline;

bool            g_bSignaleQuitFromCrashConsisency = false;

std::string getUniqueSequence()
{
    std::stringstream ssUSeq;
    ssUSeq << std::hex << GetTimeInSecSinceEpoch1970();
    DebugPrintf("\nTimeStamp - UniqueSequence = %s\n", ssUSeq.str().c_str());
    return ssUSeq.str();
}

bool operator <(const TagInfo_t & t1, const TagInfo_t & t2)
{
    if((t1.tagLen == t2.tagLen) && ( 0 == memcmp(t1.tagData, t2.tagData,t1.tagLen)))
        return false;

    return true;
}

std::string GetFormattedFailMsg(const FailMsgList &failMsgList, const std::string &delm)
{
    std::stringstream ss;
    if (failMsgList.size())
    {
        FailMsgList::const_iterator cit = failMsgList.begin();

        for (cit; cit != failMsgList.end(); ++cit)
        {
            ss << cit->getMsg();
            ss << delm;
        }
    }
    return ss.str();
}

std::string  GetLocalSystemTime()
{
    time_t time;
    struct tm * timeinfo;
    char time_in_readable_format [80];
	bool bRet = true;
    unsigned long msecs;

    if ( ACE_OS::time(&time) != -1)
    {
        msecs = ACE_OS::gettimeofday().msec();
        timeinfo = ACE_OS::localtime(&time);
        if ( ACE_OS::strftime(time_in_readable_format,80,"%Y-%m-%d-%H:%M:%S",timeinfo) == 0 )
            bRet = false;
    }
    else
    {
        bRet = false;
    }

    std::string strTime;
    if (bRet)
    {
        strTime = time_in_readable_format;
        char strMsecs[20] = {0};
        inm_sprintf_s(strMsecs, ARRAYSIZE(strMsecs), "%lu", msecs);
        strTime += " "; 
        strTime += strMsecs;
    }

    return strTime;
}

void ExitVacp(int hr)
{
    inm_printf("Entered ExitVacp() at %s\n",GetLocalSystemTime().c_str());

	if (g_bThisIsMasterNode && g_pServerCommunicator)
	{
		g_pServerCommunicator->StopCommunicator(CommunicatorTypeSched);
	}

    std::stringstream ssFailMsgs;
    std::string VacpFailMsgs = GetFormattedFailMsg(g_VacpFailMsgsList, VACP_FAIL_MSG_DELM);
    if (VacpFailMsgs.length())
    {
        ssFailMsgs << vacpFailMsgsTok;
        ssFailMsgs << VacpFailMsgs;
        ssFailMsgs << "\n";

        // First error code is exit code
        FailMsgList::const_iterator itr = g_VacpFailMsgsList.begin();
        for (itr; itr != g_VacpFailMsgsList.end(); itr++)
        {
            if ((VACP_MODULE_INFO != itr->m_errorCode) && (hr != itr->m_errorCode))
            {
                hr = itr->m_errorCode;
                break;
            }
        }
    }
    std::string MultivmFailMsgs = GetFormattedFailMsg(g_MultivmFailMsgsList, MULTIVM_FAIL_MSG_DELM);
    if (MultivmFailMsgs.length())
    {
        ssFailMsgs << multivmFailMsgsTok;
        ssFailMsgs << MultivmFailMsgs;
        ssFailMsgs << "\n";
    }
    if (ssFailMsgs.tellp())
    {
        inm_printf("%s\n", ssFailMsgs.str().c_str());
    }

	inm_printf("\nExited at %s with 0x%x", GetLocalSystemTime().c_str(), hr);
	exit(hr);
}

void usage(char * myname)
{

#ifdef VXFSVACP
    inm_printf("This tool can be used to issue consistancy bookmarks to vxfs volumes also, not available with standard distribution.\n");
#endif

    inm_printf ("Usage:\n");
    inm_printf("%s { [-remote -serverdevice <device1,device2,..> -t <tag1,..> -serverip <serve IPAddress>]} [-serverport <ServerPort>] | [-x] | [-h]\n\n", myname);

#if defined(sun) | defined (linux) |defined (SV_AIX)

    inm_printf("%s { [-v <vol1,..> |all ] | [-remote -serverdevice <device1,device2,..>] } [-t <tag1,..>] [-serverip <server IPAddress>] [-serverport <ServerPort>] [-x] [-h] [-prescript <custom prescript>] [-postscript <custom postscript>] [-systemlevel] [-distributed -mn <IPAddress> -cn <IPAddress,IPAddress..> [-dport <port>]]\n\n", myname);

    inm_printf(" -v <vol1,vol2,..>\n");		
    inm_printf("	 Specify volumes on which tag has to be generated.\n");
    inm_printf("	 Specify \"all\" to generate tags on all volumes in the system\n");
    inm_printf("	 Eg: /dev/sda1;/dev/sda2 etc\n\n");
    inm_printf(" all      Issue tags to all protected volumes\n\n");
#endif


    inm_printf(" -t <tagName1,tagNam2,..>\n");
    inm_printf("	 Specifies one or more bookmark tags.\n");	
    inm_printf("	 Eg: \"WeeklyBackup\" \"AfterQuartelyResults\" etc.\n\n");
    inm_printf("	 NOTE: Tags cannot be issued to dismounted Volumes\n");
    inm_printf("	 unless -x option is specified.\n\n");

    inm_printf(" -x \t To generate tags w/o any consistency mechanism (Supported in future)\n");
    inm_printf("    \t This option must be specified along with -v and -t options.\n\n");
    
    inm_printf(" -prescript \t A custom script that is invoked before quiescing the volumes and issuing of tags.\n\n");
    inm_printf(" -postscript \t A custome script that is invoked after successfully issuing the tags.\n\n");
    
    inm_printf(" -systemlevel \t Issues a consistency bookmark for all of the volumes in the system. Equivalent to vacp -v all.\n\n");

#if defined (linux)
    inm_printf(" -cc\n");
    inm_printf("    \t Specifies crash consistency tags to be issued.\n");
    inm_printf("    \t This option should be used only when crash consistency is sufficient.\n\n");
#endif

    inm_printf(" -remote");
    inm_printf(" \tThis switch is used for array based protections.\n");

    inm_printf(" -serverdevice <device1,device2,..>\n");
    inm_printf("    \t Specifies server device names separated by comma on which tag is to be applied.\n\n");
    inm_printf("    \t For fabric, serverdevice is LUN IDs.\n\n");

    inm_printf(" -serverip <server IP Address>\n");
    inm_printf("    \t Specifies server IP address where VACP Server is running.\n");
    inm_printf("    \t Example -serverip 10.0.10.24\n\n");

    inm_printf(" -serverport <Server Port>\n");
    inm_printf("    \t Specifies vacp server port number that to be used by VACP client to connect to\n");
    inm_printf("    \t vacp server. This is an optional parameter. \n");
    inm_printf("    \t If not specified it will use 20003 port number.\n\n");

    inm_printf(" -distributed -mn <IPv4Address> -cn <IPv4Address[:IPv4Address...][,IPv4Address[:IPv4Address...]...]> [-dport <port>]\n");
    inm_printf("    \t Specifies distributed vacp to be used.\n");
    inm_printf("    \t -mn master node IPv4Address. \n");
    inm_printf("    \t -cn coordinator nodes IPv4Addresses. \n");
    inm_printf("    \t -dport the port number to use for sockets. Default is 20004.\n\n");

    inm_printf(" -h \t Print the usage message.\n");
    inm_printf("    \t This option should not be combined with any other option.\n\n");

    inm_printf("Example:\n");

#if defined(sun) | defined (linux) |defined (SV_AIX)
    inm_printf("Example for normal setup:\n");
    inm_printf("vacp -v /dev/sda1,/dev/sda2 -t TAG1\n\n");
    inm_printf("vacp -v /dev/sda1,/dev/sda2 -t TAG1 -prescript\"./custom_prescript\" -postscript \"./custom_postscript\"\n\n");
    inm_printf("vacp -systemlevel -t TAG3\n\n");
#if defined (linux)
    inm_printf("vacp -systemlevel -cc -t TAG1\n\n");
#endif
    inm_printf("Example for client-server setup:\n");
    inm_printf("vacp -v VolGroup00-LogVol01,VolGroup00-LogVol00 -remote -serverdevice VG_XenStorage--d3cad621--52f6--3565--5cba--a225c249e5b9-LV--a7340a24--2776--4c8e--9434--88d1103801bf,VG_XenStorage--d3cad621--52f6--3565--5cba--a225c249e5b9-LV--a7340a24--2776--4c8e--9434--88d1103801ae -t TAG1 -serverip 10.0.214.10 -serverport 20003\n");
    inm_printf("vacp -systemlevel -remote -serverdevice VG_XenStorage--d3cad621--52f6--3565--5cba--a225c249e5b9-LV--a7340a24--2776--4c8e--9434--88d1103801bf,VG_XenStorage--d3cad621--52f6--3565--5cba--a225c249e5b9-LV--a7340a24--2776--4c8e--9434--88d1103801ae -t TAG1 -serverip 10.0.214.10 -serverport 20003\n");
    inm_printf("\nExample for distributed nodes:\n");
    inm_printf("vacp -systemlevel -t TAG1 -distributed -mn 10.0.1.10 -cn 10.0.1.11,10.0.1.12 -dport 20004\n\n");
    inm_printf("vacp -v /dev/sda1,/dev/sdb1 -t TAG1 -distributed -mn 10.0.1.10 -cn 10.0.1.11,10.0.1.12 -dport 20004\n");
    inm_printf("vacp -v /dev/sda1,/dev/sdb1 -t TAG1 -distributed -mn 10.0.1.10 -cn 10.0.1.11,10.0.1.12:10.0.1.13,10.0.1.14 -dport 20004\n");
    inm_printf("Note: use colon (:) seperated coordinator node list in case of tiered applications. The master node will be considered as the bottom most tier and application and filesystem freeze on master node happens after all other nodes freeze is complete.\n");
#else
    inm_printf("Example for client-server setup:\n");
    inm_printf("vacp -remote -serverdevice VG_XenStorage--d3cad621--52f6--3565--5cba--a225c249e5b9-LV--a7340a24--2776--4c8e--9434--88d1103801bf,VG_XenStorage--d3cad621--52f6--3565--5cba--a225c249e5b9-LV--a7340a24--2776--4c8e--9434--88d1103801ae -t TAG1 -serverip 10.0.214.10 -serverport 20003\n");	
#endif


    inm_printf("\n NOTE: If no other option is specified, -h is a default option.\n\n");

}
    
//
// Function: GetVolumesToBeTagged
//   Given the list of input volumes, input applications, and FileSystem Consistency
//   generate the list of affected volumes.
//   Currently, we can issue filesystem consistency tag only if the
//   volume is mounted and the filesystem is a journaled filesystem
//
bool GetVolumesToBeTagged(set<string> & inputVolumes, /* set<string> & inputAppNames *,*/
                        set<string> & affectedVolumes, const bool FsTag)
{

    bool rv = true;
    char * token = NULL;
    string unsupportedVolumes;

    do 
    {
        affectedVolumes.clear();

        if (!FsTag)
        {
            affectedVolumes.insert(inputVolumes.begin(), inputVolumes.end());
            rv = true;
            break;
        }

        set<string> supportedFsVolumes;
        if(!GetFSTagSupportedVolumes(supportedFsVolumes))
        {
            rv = false;
            break;
        }

        set<string>::const_iterator viter(inputVolumes.begin());
        set<string>::const_iterator vend(inputVolumes.end());
        while(viter != vend)
        {
            string devname = *viter;
            if(supportedFsVolumes.find(devname) == supportedFsVolumes.end())
            {                       
                // driver will not ensure fs consistency for unmounted volumes
                unsupportedVolumes += devname;
                unsupportedVolumes += "\n";
            }

            // TODO: when the protected disk list is available to the VACP,
            // use that instead of the discovered disk list 
            if(gDiskList.find(devname) != gDiskList.end())
            {                       
                affectedVolumes.insert(devname); 
            }

            viter++;
        }

        if(!unsupportedVolumes.empty())
        {
            inm_printf("Only mounted volumes are supported for filesystem consistency.\n");
            inm_printf("Following volumes will be excluded from file system consistency:\n");
            inm_printf("%s\n", unsupportedVolumes.c_str());
        }

    } while (0);

    return rv;
}

//
// Function: ParseTag
//  fill in the list of input tags
//

bool ParseTag(set<string> & tags, char *token)
{
    bool rv = true;
    tags.insert(string(token));
    return rv;
}

bool Consistency::BuildAndAddStreamBasedTag(set<TagInfo_t> & finaltags, unsigned short tagId, const char * inputTag, const std::string &sTagGuid)
{
    bool rv = true;

    do
    {
        unsigned long TotalTagLen = 0;
        unsigned long TagLen = 0;
        unsigned long TagHeaderLen = 0;

        INM_SAFE_ARITHMETIC(TagLen = InmSafeInt<unsigned long>::Type((unsigned long)strlen(inputTag)) + 1, INMAGE_EX(strlen(inputTag)))

        if (TagLen <= ((unsigned long)0xFF - sizeof(STREAM_REC_HDR_4B)))
        {
            TagHeaderLen = sizeof(STREAM_REC_HDR_4B);				
        }
        else
        {
            ASSERT(TagLen <= ((unsigned long)(-1) - sizeof(STREAM_REC_HDR_8B)));
            TagHeaderLen = sizeof(STREAM_REC_HDR_8B);
        }
        
        unsigned long TagGuidLen;

        //Append with 2 \0s to avoid alignment issues on Solaris sparc
        INM_SAFE_ARITHMETIC(TagGuidLen = InmSafeInt<unsigned long>::Type((unsigned long)sTagGuid.size()) + 2, INMAGE_EX(sTagGuid.size())) 

        char strTagGuid[TagGuidLen];
        // make sure there is no un-initialized data being sent with the tag
        memset(strTagGuid,0,TagGuidLen);
        inm_memcpy_s(strTagGuid, sizeof(strTagGuid), sTagGuid.c_str(), sTagGuid.size() + 1); //Copy including '\0'

        unsigned long ulDataLength;
        INM_SAFE_ARITHMETIC(ulDataLength = InmSafeInt<unsigned long>::Type(TagGuidLen) + TagLen, INMAGE_EX(TagGuidLen)(TagLen))
        
        StreamEngine en(eStreamRoleGenerator);
        rv = en.GetDataBufferAllocationLength(ulDataLength,&TotalTagLen);		
        if(!rv)
        {
            inm_printf("\nFILE = %s, LINE = %d, GetDataBufferAllocationLength() failed for Tag GUID\n",__FILE__,__LINE__);
            break;
        }
        //since there are two headers, we need to add header size again to the total length.
        //To add the header length again(for the second header), call this function GetDataBufferAllocationLength() again
        unsigned long buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<unsigned long>::Type(ulDataLength) + TagHeaderLen, INMAGE_EX(ulDataLength)(TagHeaderLen))
        rv = en.GetDataBufferAllocationLength(buflen, &TotalTagLen);
        if (!rv)
        {
            inm_printf("FILE = %s, LINE = %d, GetDataBufferAllocationLength() failed\n", __FILE__, __LINE__);
            break;
        }

        char *buffer = new char[TotalTagLen];		
        rv = en.RegisterDataSourceBuffer(buffer, TotalTagLen);		
        if (!rv)
        {
            inm_printf("FILE = %s, LINE = %d, GetDataBufferAllocationLength() failed\n", __FILE__, __LINE__);
            break;
        }
        //Generate a Header to store the Tag Guid			
        m_tagTelInfo.Add(TagGuidKey, std::string((char*)strTagGuid));
        inm_printf("\nGenerating Header for TagGuid: %s \n",strTagGuid);
                    
        rv = en.BuildStreamRecordHeader(STREAM_REC_TYPE_TAGGUID_TAG,TagGuidLen);

        if(!rv)
        {
            inm_printf("\nFILE = %s, LINE = %d, BuildStreamRecordHeader() for TagGuid failed.\n",__FILE__,__LINE__);
            break;
        }			


        //Fill in the Tag Guid in the Data Part of the Stream
        rv = en.BuildStreamRecordData((void*)strTagGuid,TagGuidLen);		
        if(!rv)
        {
            inm_printf("\nFILE = %s, LINE = %d, BuildStreamRecordData() for TagGuid failed,\n", __FILE__,__LINE__);
            break;
        }
        //Again set the stream state as waiting for Header as the actual Tag Data header should be filled now
        en.SetStreamState(eStreamWaitingForHeader);

        ulDataLength = (unsigned long)strlen(inputTag) + 1;

        if(!en.BuildStreamRecordHeader(tagId, ulDataLength))
        {
            inm_printf( "Function %s @LINE %d in FILE %s : in BuildStreamRecordHeader\n", 
                FUNCTION_NAME, LINE_NO, FILE_NAME);
            rv = false;
            break;
        }

        if(!en.BuildStreamRecordData((void *)inputTag, ulDataLength))
        {
            inm_printf( "Function %s @LINE %d in FILE %s : in BuildStreamRecordData\n", 
                FUNCTION_NAME, LINE_NO, FILE_NAME);
            rv = false;
            break;
        }

        TagInfo_t t;
        t.tagData = buffer;
        t.tagLen = (unsigned short) TotalTagLen;

        finaltags.insert(t);
        rv = true;

    } while (0);

    return rv;
}

//
// Function: GenerateTagNames
//  given the list of input user tags,applications and FS tag, generate
//  stream based tags
//
bool Consistency::GenerateTagNames(set<TagInfo_t> & finaltags, set<string> & inputtags, 
                      /*set<string & inputApps, */  const bool FSTag, std::string & uuid, std::string& uniqueIdStr)
{

    bool rv = true;

    if (!m_bDistributedVacp)
    {
        if(uniqueIdStr.empty())
        {
            uniqueIdStr = getUniqueSequence();
        }

        if (gStrTagGuid.empty())
            uuid = uuid_gen();
        else
            uuid = gStrTagGuid;
    }

    inm_printf("\nGenerating tag names with GUID %s ...\n", uuid.c_str());

    do
    {
        if(FSTag && !gbBaselineTag)
        {
            const string appName = "FS";
            unsigned short tagId;
            VacpUtil::AppNameToTagType(appName,tagId);

            const char * fsTagData = "FileSystem";

            char TagString[1024];
            inm_sprintf_s(TagString, ARRAYSIZE(TagString), "%s%s", fsTagData, uniqueIdStr.c_str());
            std::string TagKey = "Tag: ";
            TagKey += std::string(TagString);
            m_tagTelInfo.Add(TagKey, std::string());
            inm_printf("\nTag: %s \n", TagString);
            if(!BuildAndAddStreamBasedTag(finaltags,tagId,TagString, uuid))
            {
                rv = false;
                break;
            }

            if (m_bDistributedVacp)
            {
                const char * fsTagData = "DistributedFileSystem";
                inm_sprintf_s(TagString, ARRAYSIZE(TagString), "%s%s", fsTagData, uniqueIdStr.c_str());
                std::string TagKey = "Tag: ";
                TagKey += std::string(TagString);
                m_tagTelInfo.Add(TagKey, std::string());
                inm_printf("\nTag: %s \n", TagString);

                if(!BuildAndAddStreamBasedTag(finaltags,tagId,TagString, uuid))
                {
                    rv = false;
                    break;
                }
            }
        }

        if (gbSystemLevel)
        {
            const string appName = "SYSTEMLEVEL";
            unsigned short tagId;
            VacpUtil::AppNameToTagType(appName,tagId);

            const char * crTagData = "SystemLevelTag";

            char TagString[TAG_MAX_LENGTH];
            inm_strcpy_s(TagString, TAG_MAX_LENGTH, crTagData);
            inm_strcat_s(TagString, TAG_MAX_LENGTH, uniqueIdStr.c_str());
            std::string TagKey = "Tag: ";
            TagKey += std::string(TagString);
            m_tagTelInfo.Add(TagKey, std::string());
            inm_printf("\nTag: %s \n", TagString);
            if(!BuildAndAddStreamBasedTag(finaltags,tagId,TagString, uuid))
            {
                rv = false;
                break;
            }
        }

        if (IS_CRASH_TAG(m_TagType))
        {
            const string appName = "CRASH";
            unsigned short tagId;
            VacpUtil::AppNameToTagType(appName,tagId);

            const char * crTagData = "CrashTag";

            char TagString[TAG_MAX_LENGTH];
            inm_strcpy_s(TagString, TAG_MAX_LENGTH, crTagData);
            inm_strcat_s(TagString, TAG_MAX_LENGTH, uniqueIdStr.c_str());
            std::string TagKey = "Tag: ";
            TagKey += std::string(TagString);
            m_tagTelInfo.Add(TagKey, std::string());
            inm_printf("\nTag: %s \n", TagString);
            if(!BuildAndAddStreamBasedTag(finaltags,tagId,TagString, uuid))
            {
                rv = false;
                break;
            }

            if (m_bDistributedVacp)
            {
                const char * crTagData = "DistributedCrashTag";
                inm_sprintf_s(TagString, ARRAYSIZE(TagString), "%s%s", crTagData, uniqueIdStr.c_str());
                std::string TagKey = "Tag: ";
                TagKey += std::string(TagString);
                m_tagTelInfo.Add(TagKey, std::string());
                inm_printf("\nTag: %s \n", TagString);

                if(!BuildAndAddStreamBasedTag(finaltags,tagId,TagString, uuid))
                {
                    rv = false;
                    break;
                }
            }
        }

        if (gbBaselineTag)
        {
            const string appName = "BASELINE";
            unsigned short tagId;
            VacpUtil::AppNameToTagType(appName,tagId);
            inm_printf("%s %s \t TagType = 0x%x\n", BaselineTagKey.c_str(), gStrBaselineId.c_str(), tagId);
            if(!BuildAndAddStreamBasedTag(finaltags, tagId, gStrBaselineId.c_str(), uuid))
            {
                rv = false;
                break;
            }
        }

        if (!m_hydrationTag.empty())
        {
            const string appName = "HYDRATION";
            unsigned short tagId;
            VacpUtil::AppNameToTagType(appName,tagId);

            inm_printf("%s %s \t TagType = 0x%x\n", HydrationTagKey.c_str(), m_hydrationTag.c_str(), tagId);
            if(!BuildAndAddStreamBasedTag(finaltags, tagId, m_hydrationTag.c_str(), uuid))
            {
                rv = false;
                break;
            }
        }

        set<string>::const_iterator tagiter = inputtags.begin();
        set<string>::const_iterator tagend = inputtags.end();

        while(tagiter != tagend )
        {
            const string appName = "USERDEFINED";
            string inputtag = *tagiter;

            unsigned short tagId;
            VacpUtil::AppNameToTagType(appName, tagId);

            char TagString[1024];
            inm_sprintf_s(TagString, ARRAYSIZE(TagString), "%s", inputtag.c_str());
            std::string TagKey = "Tag: ";
            TagKey += std::string(TagString);
            m_tagTelInfo.Add(TagKey, std::string());
            inm_printf( "Tag: %s \n", TagString);
            if(!BuildAndAddStreamBasedTag(finaltags,tagId,TagString, uuid))
            {
                rv = false;
                break;
            }	
            ++tagiter;
        }

        if(tagiter != tagend)
        {
            rv = false;
            break;
        }

    } while (0);

    return rv;
}


bool ConnToVacpServer (u_short port, const char* vacpServerIP)
{
    bool rv = true;

    // Create a socket for connecting to server
    vacpServSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (vacpServSocket < 0) 
    {
        inm_printf("Error creating socket(): %d\n", errno);
        return false;
    }

    // The sockaddr_in structure specifies the address family,
    // IP address, and port of the server to be connected to.
    vacpServer.sin_family = AF_INET;
    vacpServer.sin_addr.s_addr = inet_addr( vacpServerIP );
    vacpServer.sin_port = htons(port);

    inm_printf("\nENTERED: ConnToVacpServer\n");

    do 
    {
        if( connect( vacpServSocket, (struct sockaddr *) &vacpServer, sizeof(vacpServer) ) < 0) 
        {
            inm_printf("Error: connect() failed with error : %d\n", errno);						
            inm_printf( "Error: Couldn't connect to remote server: %s, port: %d\n", vacpServerIP, port );
            rv = false;
            break;
        }
        else 
        {
            gbRemoteConnection = true;
            inm_printf( "Connected to %s at port %d\n", vacpServerIP, port);
        }
    }while(0);

    inm_printf( "EXITED: ConnToVacpServer\n");

    return rv;
}

bool prepare(u_short vacpServPort, const string vacpServerIP)
{
    bool rv = true;


    //Check if connection established already
    if(!gbRemoteConnection)
    {		
        if( ConnToVacpServer( vacpServPort, vacpServerIP.c_str()) )
        {
            inm_printf("Connected to Server...\n");			
        }
        else
        {
            inm_printf("\nCannot continue...\n");
            rv = false;
        }
    }
    else
    {
        inm_printf("\nConnection to Vacp Server already exists\n");
    }
    return rv;	
}

bool ParseVolumeList(std::set<std::string>& sVolumes, std::set<std::string> &sDisks,char* tokens, const char* seps)
{
    char* token;

    token = strtok(tokens,seps);
    while( token != NULL )
    {
        if( !ParseVolume(sVolumes, sDisks, token) )
        {
            inm_printf("Invalid volume: %s", token);
            return false;
        }
        token = strtok( NULL, seps );
    }   
    return true;
}

bool ParseTagList(set<string>& sTags ,char* tokens, const char* seps)
{
    char* token;
    uint16_t totalLengthOfAllTags = 0;

    token = strtok(tokens,seps);
    while( token != NULL )
    {
        if (strlen(token) > MAX_TAG_LENGTH)
        {
            inm_printf("Error: string length of tag %s is more than supported maximum of %d\n",
                token , MAX_TAG_LENGTH);
            return false;
        }

        ParseTag(sTags, token);
        totalLengthOfAllTags += strlen(token);

        if (totalLengthOfAllTags > MAX_LENGTH_OF_ALL_TAGS)
        {
            inm_printf("Error: total size of tags is more than supported maximum of %d\n",
                MAX_LENGTH_OF_ALL_TAGS);
            return false;
        }

        token = strtok( NULL, seps );
    }

    return true;
}

void ParseServerDeviceList(set<string>& sDevices, char* tokens, const char* seps)
{
    char* token;

    token = strtok(tokens,seps);
    while( token != NULL )
    {
        sDevices.insert(string(token));
        token = strtok( NULL, seps );
    }
}

bool ParseServerIpList(vector<string>& sNodes, char* tokens, const char* seps)
{
    char* token;

    token = strtok(tokens,seps);
    while( token != NULL )
    {
        sNodes.push_back(string(token));
        token = strtok( NULL, seps );
    }
    return true;
}

int GetLocalIPs()
{
    strset_t localIps; 
    std::string error;
    if(!GetIPAddressSetFromNicInfo(localIps, error))
    {
        inm_printf("Failed to get the local IP address %s.\n", error.c_str());
        return VACP_IP_ADDRESS_DISCOVERY_FAILED;
    }

    inm_printf("\n\n");
    strset_t::iterator ipIter = localIps.begin();
    for (/* empty */; ipIter != localIps.end(); ipIter++)
    {
        inm_printf("\tIPv4 address %s\n",ipIter->c_str() );
        g_vLocalHostIps.push_back(*ipIter);
    }


    char lInmMachineName[512] = {0};
    int nRet = gethostname(lInmMachineName,sizeof(lInmMachineName));
    if ( nRet != 0)
    {
          inm_printf("Failed to get the local hostname%s.\n", error.c_str());
          return VACP_HOSTNAME_DISCOVERY_FAILED;
    }
 
    g_strLocalHostName.assign(lInmMachineName);

    if (g_strLocalHostName.empty())
    {
        inm_printf("Failed to get the local hostname%s.\n", error.c_str());
        return 1;
    }

    return 0;
}

void Consistency::ExitDistributedConsistencyMode()
{
    inm_printf("ENTERED: %s\n", __FUNCTION__);
    if (m_bDistributedVacp && m_pCommunicator)
    {
        if (gbParallelRun)
        {
            m_tagTelInfo.Add(m_pCommunicator->GetTagTelemetryInfo().GetMap());
        }
        
        if (g_bThisIsMasterNode && g_pServerCommunicator)
        {
            g_pServerCommunicator->StopCommunicator(m_strType);
        }
        else
        {
            m_pCommunicator->CloseCommunicator();
        }
        m_pCommunicator.reset();
        m_bDistributedVacp = false;
    }

    inm_printf("EXITING: %s\n", __FUNCTION__);

    return;
}

void PersistConsistencyScheduleTime(uint64_t currentPolicyRunTime, TAG_TYPE TagType)
{
    namespace bpt = boost::property_tree;
    static boost::mutex s_schedlueLock;
    uint64_t appSchedTime = 0, crashSchedTime = 0;
    
    boost::mutex::scoped_lock quard(s_schedlueLock);

    try {
        if (boost::filesystem::exists(g_strScheduleFilePath))
        {
            bpt::ptree pt;
            bpt::json_parser::read_json(g_strScheduleFilePath, pt);

            appSchedTime = pt.get<uint64_t>(LastAppConsistencyScheduleTime, 0);
            crashSchedTime = pt.get<uint64_t>(LastCrashConsistencyScheduleTime, 0);

            DebugPrintf("%s: LastAppConsistencyScheduleTime :%lld, LastCrashConsistencyScheduleTime : %lld\n",
                __FUNCTION__, appSchedTime, crashSchedTime);

            DebugPrintf("%s: update %s : %lld\n",
                __FUNCTION__,
                IS_CRASH_TAG(TagType) ? "CrashConsistency" : "AppConsistency",
                currentPolicyRunTime);
        }

        bpt::ptree pt;
        if (!IS_CRASH_TAG(TagType))
        {
            pt.put(LastAppConsistencyScheduleTime, currentPolicyRunTime);
            pt.put(LastCrashConsistencyScheduleTime, crashSchedTime);
        }
        else
        {
            pt.put(LastAppConsistencyScheduleTime, appSchedTime);
            pt.put(LastCrashConsistencyScheduleTime, currentPolicyRunTime);
        }

        bpt::json_parser::write_json(g_strScheduleFilePath, pt);
    }
    catch (const std::exception &e)
    {
        DebugPrintf("%s : updating consistency schedule file failed. %s\n",
            __FUNCTION__,
            e.what());
    }
    catch (...)
    {
        DebugPrintf("%s : updating consistency schedule file failed with unknown exception\n",
            __FUNCTION__);
    }
    return;
}

void Consistency::InitSchedule()
{
    // set default values to be used in case the settings are not available
    m_interval = IS_CRASH_TAG(m_TagType) ? 300 : 3600;
    //m_exitTime = steady_clock::time_point::max();
    m_exitTime = steady_clock::now() + seconds(24 * 60 * 60);

    if (g_bDistributedVacp)
    {
        if (IS_CRASH_TAG(m_TagType))
            m_CommTimeouts.SetTimeoutsForCrashConsistency();
        else
            m_CommTimeouts.SetTimeoutsForAppConsistency();
    }

    VacpConfigPtr pVacpConf = VacpConfig::getInstance();
    if (pVacpConf.get() == NULL)
        return;

    long exitTime = 0;
    if (pVacpConf->getParamValue(VACP_CONF_PARAM_EXIT_TIME, exitTime))
    {
        if (exitTime)
            m_exitTime = steady_clock::now() + seconds(exitTime);
    }
    
    long schedInterval = 0;
    if (pVacpConf->getParamValue(VACP_CONF_PARAM_REMOTE_SCHEDULE_INTERVAL, schedInterval))
    {
        if (schedInterval)
            gnRemoteScheduleCheckInterval = schedInterval;
    }

    long paramInterval = 0;
    if (pVacpConf->getParamValue(
                IS_CRASH_TAG(m_TagType) ? VACP_CONF_PARAM_CRASH_CONSISTENCY_INTERVAL : VACP_CONF_PARAM_APP_CONSISTENCY_INTERVAL,
                paramInterval))
    {
        m_interval = paramInterval;
    }
    else
    {
        DebugPrintf("The %s interval not found in vacp.conf file. Using default values.\n",
            IS_CRASH_TAG(m_TagType) ? VACP_CONF_PARAM_CRASH_CONSISTENCY_INTERVAL : VACP_CONF_PARAM_APP_CONSISTENCY_INTERVAL);
    }

    bool timeMovedBack = false;
    uint64_t currentTime = GetTimeInSecSinceEpoch1970();
    if ((currentTime < g_lastCrashScheduleTime) || (currentTime < g_lastAppScheduleTime))
    {
        DebugPrintf("Time moved backward, scheduling immediate.\n");
        timeMovedBack = true;
    }

    uint64_t lastSheduleTime = IS_CRASH_TAG(m_TagType) ? g_lastCrashScheduleTime : g_lastAppScheduleTime;
    if (!timeMovedBack && lastSheduleTime)
    {
        if (lastSheduleTime < currentTime)
            m_prevRunTime = steady_clock::now() - seconds(currentTime - lastSheduleTime);
    }
    else
    {
        m_prevRunTime = steady_clock::now() - seconds(m_interval);
        DebugPrintf("No previous run found, scheduling immediate.\n");
    }

    if (!g_bDistributedVacp)
        return;

    if (IS_CRASH_TAG(m_TagType))
        m_CommTimeouts.SetCustomTimeoutsForCrashConsistency(pVacpConf->getParams());
    else
        m_CommTimeouts.SetCustomTimeoutsForAppConsistency(pVacpConf->getParams());


    CommunicatorScheduler *pCommSched = CommunicatorScheduler::GetCommunicatorScheduler();
    if (lastSheduleTime)
        pCommSched->SetSchedule(m_strType, m_prevRunTime + seconds(m_interval));
    else
        pCommSched->SetSchedule(m_strType, steady_clock::now());
}

void Consistency::SetSchedule()
{
    m_prevRunTime = steady_clock::now();
    PersistConsistencyScheduleTime(GetTimeInSecSinceEpoch1970(), m_TagType);

    if (!g_bDistributedVacp)
        return;

    if (!g_bThisIsMasterNode)
        return;

    CommunicatorScheduler *pCommSched = CommunicatorScheduler::GetCommunicatorScheduler();

    // this is to set the current schedule for the client nodes immediately
    // as master node has started the multi-vm protocol
    pCommSched->SetSchedule(m_strType, m_prevRunTime);
}

void Consistency::SetNextSchedule()
{
    if (!g_bDistributedVacp)
        return;

    // set the next schedule to local on client node as well. If the remote schedule
    // ever succeeds, the schedule will be set to remote interval

    CommunicatorScheduler *pCommSched = CommunicatorScheduler::GetCommunicatorScheduler();

    // on the master node add additinal gnRemoteScheduleCheckInterval sec delay for the
    // client nodes to start communicating as the client node is checking the schedule every
    // gnRemoteScheduleCheckInterval sec
    seconds clientBufferTime(0);
    if (g_bThisIsMasterNode)
        clientBufferTime = seconds(gnRemoteScheduleCheckInterval);

    pCommSched->SetSchedule(m_strType, m_prevRunTime + seconds(m_interval) + clientBufferTime);
}


steady_clock::time_point Consistency::GetSchedule(bool bSignalQuit)
{
    if (bSignalQuit && (steady_clock::now() > m_exitTime))
    {
        g_Quit->Signal(true);
        return steady_clock::time_point::max();
    }

    if (!gbParallelRun)
        return steady_clock::now();

    if (!g_bDistributedVacp && IS_CRASH_TAG(m_TagType))
        return m_prevRunTime + seconds(m_interval);

    if (!g_bDistributedVacp && IS_APP_TAG(m_TagType))
    {
        unsigned long long systemUpTime = 0;
        if ((m_lastRunErrorCode == VACP_E_DRIVER_IN_NON_DATA_MODE) &&
            (Host::GetInstance().GetSystemUptimeInSec(systemUpTime)) &&
            (systemUpTime < (m_interval / 2)))
        {
            m_bInternallyRescheduled = true;
            // TODO : make it a config param
            seconds retryinterval(300);
            return m_prevRunTime + retryinterval;
        }
        else
        {
            m_bInternallyRescheduled = false;
            return m_prevRunTime + seconds(m_interval);
        }
    }

    if (g_bThisIsMasterNode)
        return m_prevRunTime + seconds(m_interval);

    CommunicatorScheduler *pCommSched = CommunicatorScheduler::GetCommunicatorScheduler();
    return pCommSched->GetSchedule(m_strType);
}


void Consistency::InitializeCommunicator()
{
    if (!g_bDistributedVacp)
        return;
    
    std::string errmsg;
    m_bDistributedVacp = true;
    m_DistributedProtocolState = COMMUNICATOR_STATE_INITIALIZED;
    m_DistributedProtocolStatus = 0;

    if (g_bThisIsMasterNode)
    {
        m_tagTelInfo.Add(MasterNodeRoleKey, std::string());
    }
    else
    {
        m_tagTelInfo.Add(ClientNodeRoleKey, std::string());
    }
    DebugPrintf("MultiVmRole:%s\n", g_bThisIsMasterNode ? "Master" : "Client");

    //Populate the Msgbucket for each coordinator node w.r.t. Master
    std::set<std::string> clientNodes;
    std::vector<string>::iterator iterCoordNodes = g_vCoordinatorNodes.begin();
    for (/*empty*/; iterCoordNodes != g_vCoordinatorNodes.end(); iterCoordNodes++)
    {
        clientNodes.insert(*iterCoordNodes);
    }

    if (g_bThisIsMasterNode)
    {
        //Get the communicator to get communication functionality
        g_pServerCommunicator = ServerCommunicator::GetServerCommunicator();
        if (!g_pServerCommunicator)
        {
            m_DistributedProtocolStatus = ERROR_FAILED_TO_GET_COMMUNICATOR;
            errmsg = "Failed to get the communicator.Error = ERROR_FAILED_TO_GET_COMMUNICATOR.";
            DebugPrintf("\n%s \n", errmsg.c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, errmsg, ERROR_FAILED_TO_GET_COMMUNICATOR);
            ExitDistributedConsistencyMode();
            return;
        }

        int nRet = 0;
        steady_clock::time_point endTime = steady_clock::now();
        endTime += milliseconds(m_CommTimeouts.m_serverCreateSockMaxTime->msec());

        do
        {
            std::set<std::string> commTypes;

            // start the scheduler only in case of parallel run.
            // otherwise the caller will schedule VACP
            //
            if (gbParallelRun)
                commTypes.insert(CommunicatorTypeSched);

            if (g_pServerCommunicator->Initialize(clientNodes, commTypes))
                break;

            nRet = -1;
            DebugPrintf("\n Failed to create the server socket. Error = ERROR_CREATING_SERVER_SOCKET.\n");

            ACE_OS::sleep(*m_CommTimeouts.m_serverCreateSockAttemptFrequency);

        } while ((steady_clock::now() < endTime));


        if (nRet)
        {
            m_DistributedProtocolStatus = ERROR_CREATING_SERVER_SOCKET;
            errmsg = "Failed to create the server socket. This Vacp will not issue a Distributed tag, but it will issue a LOCAL Tag only.";
            errmsg += " Error: ";
            errmsg += g_pServerCommunicator->m_lastError.m_errMsg;
            DebugPrintf("\n%s \n", errmsg.c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, errmsg, ERROR_CREATING_SERVER_SOCKET);
            ExitDistributedConsistencyMode();
            return;
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_SERVER_SOCK_CREATE_COMPLETED;
        inm_printf("\nThe number of Client nodes = %d\n", g_vCoordinatorNodes.size());
    }
    else
    {
        // get the schedule times from master node only if vacp is
        // spawned in parallel run mode.
        //
        if (!gbParallelRun)
            return;

        // the crash tag thread is getting the schedules for both app and crash so skip for app
        if (!IS_CRASH_TAG(m_TagType))
            return;

        int nRet = 0;
        CommunicatorPtr commPtr;
        steady_clock::time_point endTime = steady_clock::now();
        endTime += milliseconds(m_CommTimeouts.m_clientConnectSockMaxTime->msec());

        //Connect to the Master Node's Server Socket
        do
        {
            commPtr.reset(new (std::nothrow) Communicator(CommunicatorTypeSched));
            if (commPtr.get() == NULL)
            {
                nRet = -1;
                DebugPrintf("%s: Failed to create the client socket as memory allocation failed.\n",
                            __FUNCTION__);
                break;
            }

            nRet = commPtr->CreateClientConnection();

            if (!nRet)
                break;

            DebugPrintf("\n Failed to create the client socket for scheduler. Error = ERROR_CREATING_CLIENT_SOCKET at time %s\nRetrying...\n", GetLocalSystemTime().c_str());
            ACE_OS::sleep(*m_CommTimeouts.m_clientConnectSockAttemptFrequency);

        } while (steady_clock::now() < endTime);

        if (nRet)
        {
            m_DistributedProtocolStatus = ERROR_CREATING_CLIENT_SOCKET;
            std::string errmsg = "This Vacp will no longer issue a Distributed Tag but, it will issue a LOCAL TAG.";
            errmsg += " Error: ";
            errmsg += commPtr->m_lastError.m_errMsg;
            DebugPrintf("\n%s \n", errmsg.c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, errmsg, ERROR_CREATING_CLIENT_SOCKET);
            commPtr->CloseCommunicator();
            ExitDistributedConsistencyMode();
            return;
        }

        CommunicatorScheduler *pCommSched = CommunicatorScheduler::GetCommunicatorScheduler();

        endTime = steady_clock::now();
        endTime += milliseconds(m_CommTimeouts.m_clientConnectSockMaxTime->msec());
        std::string strData;
        do
        {
            DebugPrintf("Requesting consistency schedule from master node.\n");
            nRet = commPtr->SendMessageToMasterNode(ENUM_MSG_SCHEDULE_CMD, strData);
            if (!nRet)
            {
                DebugPrintf("Waiting for consistency schedule from master node...\n");

                if (commPtr->WaitForMsg(ENUM_MSG_SCHEDULE_ACK) != -1)
                {
                    DebugPrintf("Received consistency schedule from master node.\n");

                    commPtr->CloseCommunicator();
                    nRet = 0;
                    break;
                }

                DebugPrintf("Failed to get consistency schedule from master node.\n");
                nRet = -1;
            }

            std::string errmsg = "Error in getting schedules.";
            DebugPrintf("\n%s Retrying...\n", errmsg.c_str());

            ACE_OS::sleep(*m_CommTimeouts.m_clientConnectSockAttemptFrequency);

        } while (steady_clock::now() < endTime);

        if (0 != nRet)
        {
            m_DistributedProtocolStatus = ERROR_FAILED_TO_GET_CONSISTENCY_SCHEDULES;
            std::string errmsg = "Error in getting consistency schedules.";
            DebugPrintf("\n%s \n", errmsg.c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, errmsg, ERROR_FAILED_TO_GET_CONSISTENCY_SCHEDULES);
            commPtr->CloseCommunicator();
            ExitDistributedConsistencyMode();
            return;
        }

        std::string strSchedules = pCommSched->GetSchedules();
        DebugPrintf("Consistency schedules received %s.\n", strSchedules.c_str());
    }
}

void Consistency::StartCommunicator()
{
    if (!m_bDistributedVacp)
        return;

    std::string errmsg;

    if (g_bThisIsMasterNode)
    {
        if (!g_pServerCommunicator->StartCommunicator(m_strType))
        {
            m_DistributedProtocolStatus = ERROR_CREATING_SERVER_SOCKET;
            errmsg = "Failed to create the server socket. Error: ";
            errmsg += g_pServerCommunicator->m_lastError.m_errMsg;
            errmsg += "\nThis Vacp will not issue a Distributed tag, but it will issue a LOCAL Tag only.";
            DebugPrintf("\n%s \n", errmsg.c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, errmsg, ERROR_CREATING_SERVER_SOCKET);
            ExitDistributedConsistencyMode();
            return;
        }

        DebugPrintf("\nWaiting for VacpSpawned Ackowledgement from the Coordinator Nodes...\n");

        m_pCommunicator = g_pServerCommunicator->GetCommunicator(m_strType);
        if (m_pCommunicator->WaitForMsg(ENUM_MSG_SPAWN_ACK) == -1)
        {
            std::stringstream ss;
            ss << "Wait Time for SPAWN_VACP_ACK from all the coordinator nodes expired. ";
            ss << "Master Vacp Node will ignore the node from which SPAWN_VACP_ACK is not received. Time:";
            ss << GetLocalSystemTime().c_str();
            DebugPrintf("\n%s \n", ss.str().c_str());
        }

        g_pServerCommunicator->StopAcceptingClients(m_strType);
        std::string notConnectedNodes= m_pCommunicator->CheckForMissingSpawnVacpAck();
        if (!notConnectedNodes.empty())
        {
            notConnectedNodes += SearchEndKey;
            m_tagTelInfo.Add(NotConnectedNodes, notConnectedNodes);
            DebugPrintf("\n%s %s\n", NotConnectedNodes.c_str(), notConnectedNodes.c_str());
        }
        m_DistributedProtocolState = COMMUNICATOR_STATE_SERVER_SPAWN_ACK_COMPLETED;
    }
    else
    {
        int nRet = 0;
        steady_clock::time_point endTime = steady_clock::now();
        endTime += milliseconds(m_CommTimeouts.m_clientConnectSockMaxTime->msec());

        //Connect to the Master Node's Server Socket
        do
        {
            m_pCommunicator.reset(new (std::nothrow) Communicator(m_strType));

            if (m_pCommunicator.get() == NULL)
            {
                nRet = -1;
                DebugPrintf("%s: Failed to create the client socket as memory allocation failed.\n",
                    __FUNCTION__);
                break;
            }

            nRet = m_pCommunicator->CreateClientConnection();

            if (!nRet)
                break;

            if (!gbParallelRun)
            {
                // try compatibility mode with older version of master node
                bool bCompatMode = true;
                nRet = m_pCommunicator->CreateClientConnection(bCompatMode);

                if (!nRet)
                    break;
            }

            DebugPrintf("\n Failed to create the client socket. Error = ERROR_CREATING_CLIENT_SOCKET at time %s\nRetrying...\n", GetLocalSystemTime().c_str());
            ACE_OS::sleep(*m_CommTimeouts.m_clientConnectSockAttemptFrequency);

        } while (steady_clock::now() < endTime);

        if (nRet)
        {
            errmsg = "This Vacp will no longer issue a Distributed Tag but, it will issue a LOCAL TAG.";
            errmsg += " Error: ";
            errmsg += m_pCommunicator->m_lastError.m_errMsg;
            DebugPrintf("\n%s \n", errmsg.c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, errmsg, ERROR_CREATING_CLIENT_SOCKET);
            ExitDistributedConsistencyMode();
            return;
        }

        //Send message to server socket stating that vacp is spawned in this coordinator node.
        //First Prepare the data as a packet
        stPacket VacpSpawnedPkt;
        std::string strData;
        strData.clear();
        nRet = m_pCommunicator->SendMessageToMasterNode(ENUM_MSG_SPAWN_ACK, strData);
        if (0 != nRet)
        {
            m_DistributedProtocolStatus = ERROR_FAILED_TO_SEND_SPAWNED_ACK;
            std::string errmsg = "Error in sending VacpSpawned Acknowledgement.";
            DebugPrintf("\n%s \n", errmsg.c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, errmsg, ERROR_FAILED_TO_SEND_SPAWNED_ACK);
            ExitDistributedConsistencyMode();
            return;
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_CLIENT_SPAWN_ACK_COMPLETED;
    }
}

void Consistency::DistributedTagGuidPhase(std::string & uuid, const set<std::string> & inputtags, std::string& uniqueIdStr, set<std::string> & distributedInputtags)
{
    if (!m_bDistributedVacp)
        return;

    std::vector<std::string>vDistributedBookMarks;
    std::string strUniqIdMarker = ";UniqueId=";
    std::string strUserTagsMarker = ";UserTagNames=";

    do {
       if(g_bThisIsMasterNode)
       {
        if (m_pCommunicator->GetCoordNodeCount() <= 0)
        {
           ExitDistributedConsistencyMode();
           return;
        }
         inm_printf("\nGenerating Tag Identifiers from the Master Node and sending it to all Coordinator Nodes...\n");

         std::string strTagGuid_Id_Name = uuid;
         strTagGuid_Id_Name += strUniqIdMarker;
         strTagGuid_Id_Name += uniqueIdStr;
         strTagGuid_Id_Name += strUserTagsMarker;

         set<string>::const_iterator tagiter = inputtags.begin();
         while(tagiter != inputtags.end())
         {
            strTagGuid_Id_Name += *tagiter;
            strTagGuid_Id_Name +=";";
            tagiter++;
         }

         //Send this Guid to the Co-ordinating Vacp Nodes
         int nRet = m_pCommunicator->SendMessageToEachCoordinatorNode(ENUM_MSG_SEND_TAG_GUID,
                                                                     strTagGuid_Id_Name);
         if(0 != nRet)
         {
           m_DistributedProtocolStatus = ERROR_FAILED_TO_SEND_TAG_GUID_TO_COORDINATOR_NODES;
           inm_printf("\nError: Failed to send tag GUID to coordinator nodes.\n");
           inm_printf("Hence this Vacp will NOT issue a Distributed Tag, but will issue a Local Tag.\n");
           ExitDistributedConsistencyMode();
           break;
         }
         
         m_DistributedProtocolState = COMMUNICATOR_STATE_SERVER_TAG_GUID_COMPLETED;
       }
       else
       {
         inm_printf("\nWaiting for the TagGuid to be sent by Master Vacp Node...\n");

         if (m_pCommunicator->WaitForMsg(ENUM_MSG_SEND_TAG_GUID) == -1)
         {
           m_DistributedProtocolStatus = ERROR_FAILED_TO_RECEIVE_TAG_GUID_FROM_MASTER_NODE;
           inm_printf("\nError: Failed to get the tag GUID from the Master Node %s\n",g_strMasterHostName.c_str());
           inm_printf("Hence this Vacp will NOT issue a Distributed Tag but it will issue a Local Tag.\n");
           ExitDistributedConsistencyMode();
         }
         else
         {
           //Interpret the received TagGuid_Id_Name from Master Node;
           std::string tagData = m_pCommunicator->GetReceivedData();
           int nIndex =  tagData.find(strUniqIdMarker);
           string strTemp = tagData.substr(0,nIndex);
           if(strTemp.size() > 0)
           {
            uuid = strTemp;
            strTemp.clear();
            int nTemp = tagData.find(strUserTagsMarker);
            strTemp = tagData.substr((nIndex + strUniqIdMarker.length()),nTemp - (nIndex + strUniqIdMarker.length()));
            if(strTemp.size() > 0)
            {
              uniqueIdStr = strTemp;

              strTemp.clear();
              strTemp = tagData.substr(nTemp + strUserTagsMarker.length(), tagData.npos);
              nIndex = 0;
              char*pStart = NULL;
              char* pIndex = (char*) strTemp.c_str();
              pStart = pIndex;
              int nCounter = 0;
              char* pBuf = new (std::nothrow) char[strTemp.size()];
              if(pBuf != NULL)
              {
                memset((void*)pBuf,0x00,strTemp.size());
                while(*pIndex != '\0')
                {
                  if(*pIndex ==';')
                  {
                    if(nCounter == 0)
                    {
                      pIndex++;
                      pStart = pIndex;
                      continue;
                    }
                    else if(nCounter != 0)
                    {
                      inm_memcpy_s(pBuf, (strTemp.size()), pStart, nCounter);
                      vDistributedBookMarks.push_back(std::string(pBuf));
                      memset((void*)pBuf,0x00,nCounter);
                      pStart = pIndex;
                      nCounter = 0;
                      continue;
                    }
                  }
                  pIndex++;
                  nCounter++;
                }
                distributedInputtags.clear();
                std::vector<std::string>::iterator iterDistBMs = vDistributedBookMarks.begin();
                while(iterDistBMs != vDistributedBookMarks.end())
                {
                  distributedInputtags.insert((*iterDistBMs).c_str());
                  iterDistBMs++;
                }
              }
              delete [] pBuf;
              pBuf = NULL;
            }
            else
            {
              inm_printf("\nError: Received invalid TagGuid from Master Node.\n");
              inm_printf("Hence this Vacp will NOT issue a Distributed Tag, but it will issue a Local Tag.\n");
              ExitDistributedConsistencyMode();
            }
           }
           else
           {
             inm_printf("\nError: Received invalid TagGuid from Master Node.\n");
             inm_printf("Hence this Vacp will NOT issue a Distributed Tag, but it will issue a Local Tag.\n");
             ExitDistributedConsistencyMode();
           }
         }
       }
       
       m_DistributedProtocolState = COMMUNICATOR_STATE_CLIENT_TAG_GUID_COMPLETED;
       
    }while(0);

    return;
}

void Consistency::PreparePhase()
{
    if (!m_bDistributedVacp)
        return;

    if (g_bThisIsMasterNode)
    {
        if (m_pCommunicator->GetCoordNodeCount() <= 0)
        {
            ExitDistributedConsistencyMode();
            return;
        }

        //Send Message to each coordinator node to start the Prepare phase
        std::string strData;
        strData.clear();
        
        DebugPrintf("\nSending PREPARE_CMD to all the Client nodes from Master at %s...\n", m_pCommunicator->GetLocalSystemTime().c_str());
        
        int nRet = m_pCommunicator->SendMessageToEachCoordinatorNode(
            ENUM_MSG_PREPARE_CMD,
            strData);

        if (0 != nRet)
        {
            m_DistributedProtocolStatus = ERROR_FAILED_TO_SEND_PREPARE_CMD;
            std::string errmsg = "As this Master node failed to send PREPARE_CMD to Clients, it will no longer issue a Distributed Tag, but it will issue a LOCAL Tag.";
            inm_printf("\n%s \n", errmsg.c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, errmsg, ERROR_FAILED_TO_SEND_PREPARE_CMD);

            ExitDistributedConsistencyMode();
            return;
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_SERVER_PREPARE_CMD_COMPLETED;
    }
    else // client node
    {
        //Wait for the command from Master Node to perform "Prepare" step.
        DebugPrintf("\nWaiting for the command from Master Node to perform Prepare step @ %s...\n", GetLocalSystemTime().c_str());
        if (m_pCommunicator->WaitForMsg(ENUM_MSG_PREPARE_CMD) == -1)
        {
            m_DistributedProtocolStatus = ERROR_FAILED_TO_GET_PREPARE_CMD;
            std::stringstream ss;
            ss << "Failed to get PREPARE_CMD from Master Node. Hence this Vacp will NOT issue a Distributed Tag, but will issue a Local Tag";
            DebugPrintf("\n%s \n", ss.str().c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, ss.str(), ERROR_FAILED_TO_GET_PREPARE_CMD);

            ExitDistributedConsistencyMode();
            return;
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_CLIENT_PREPARE_CMD_COMPLETED;
    }

    return;
}

void Consistency::PrepareAckPhase()
{
    if (!m_bDistributedVacp)
        return;

    if (g_bThisIsMasterNode)
    {
        if (m_pCommunicator->GetCoordNodeCount() <= 0)
        {
            ExitDistributedConsistencyMode();
            return;
        }

        XDebugPrintf("\nWaiting for the Prepare Acknowledgement from Coordinator Nodes before proceeding for Quiesce step...\n");

        if (m_pCommunicator->WaitForMsg(ENUM_MSG_PREPARE_ACK) == -1)
        {
            std::stringstream ss;
            ss << "Wait Time for PREPARE_ACK from all the coordinator nodes expired. ";
            ss << "Master Vacp Node will ignore the node from which PREPARE_ACK is not received. Time:";
            ss << GetLocalSystemTime().c_str();
            DebugPrintf("\n%s \n", ss.str().c_str());

            if (!m_DistributedProtocolStatus)
            {
                m_DistributedProtocolStatus = ERROR_TIMEOUT_WAITING_FOR_PREPARE_ACK;
                AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, "ERROR_TIMEOUT_WAITING_FOR_PREPARE_ACK", ERROR_TIMEOUT_WAITING_FOR_PREPARE_ACK);
            }
        }
            
        m_pCommunicator->CheckForMissingPrepareAck();
        m_DistributedProtocolState = COMMUNICATOR_STATE_SERVER_PREPARE_ACK_COMPLETED;
    }
    else //client node
    {
        if ((m_status & CONSISTENCY_STATUS_PREPARE_SUCCESS) != CONSISTENCY_STATUS_PREPARE_SUCCESS)
        {
            ExitDistributedConsistencyMode();
            return;
        }
        //Send Acknowledgement for Prepare command 
        std::string strData;
        strData.clear();

        int nRet = m_pCommunicator->SendMessageToMasterNode(ENUM_MSG_PREPARE_ACK, strData);
        if (0 != nRet)
        {
            m_DistributedProtocolStatus = ERROR_FAILED_TO_SEND_PREPARE_ACK;
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, "ERROR_FAILED_TO_SEND_PREPARE_ACK", ERROR_FAILED_TO_SEND_PREPARE_ACK);
            ExitDistributedConsistencyMode();
            return;
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_CLIENT_PREPARE_ACK_COMPLETED;
    }

    return;
}

void Consistency::QuiescePhase()
{
    if (!m_bDistributedVacp)
        return;

    if (g_bThisIsMasterNode)
    {
        if (m_pCommunicator->GetCoordNodeCount() <= 0)
        {
            ExitDistributedConsistencyMode();
            return;
        }

        //Send Commands to Co-ordinating/Client nodes to Perform Quiesce
        std::string strData;
        strData.clear();
        int nRet = m_pCommunicator->SendMessageToEachCoordinatorNode(
            ENUM_MSG_QUIESCE_CMD,
            strData);
        if (0 != nRet)
        {
            std::string errmsg = "Failed to send QUIESCE_CMD to the coordinator nodes. Hence this Vacp will NOT issue a Distributed Tag but it will issue a Local Tag.";
            DebugPrintf("\n%s\n", errmsg.c_str());
            if (!m_DistributedProtocolStatus)
            {
                AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, errmsg, ERROR_FAILED_TO_SEND_QUIESCE_CMD);
                m_DistributedProtocolStatus = ERROR_FAILED_TO_SEND_QUIESCE_CMD;
            }
            ExitDistributedConsistencyMode();
            return;
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_SERVER_QUIESCE_CMD_COMPLETED;
    }
    else //client node
    {
        DebugPrintf("\nWaiting for the Quiesce command to be received from Master Vacp Node ...at %s\n", GetLocalSystemTime().c_str());
        if (m_pCommunicator->WaitForMsg(ENUM_MSG_QUIESCE_CMD) == -1)
        {
            std::stringstream ss;
            ss << "TimeOut occurred while waiting for QUIESCE_CMD from the Master Node at " << GetLocalSystemTime().c_str();
            ss << ". Hence this Vacp will NOT issue a Distributed Tag,but it will continue to issue a Local Tag.";
            DebugPrintf("\n%s\n", ss.str().c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, ss.str(), ERROR_FAILED_TO_GET_QUIESCE_CMD);
            m_DistributedProtocolStatus = ERROR_FAILED_TO_GET_QUIESCE_CMD;
            ExitDistributedConsistencyMode();
            return;
        }

        DebugPrintf("\nQuiesce command received from Master Vacp Node at %s\n", GetLocalSystemTime().c_str());
        m_DistributedProtocolState = COMMUNICATOR_STATE_CLIENT_QUIESCE_CMD_COMPLETED;
    }

    return;
}

void Consistency::QuiesceAckPhase()
{
    if (!m_bDistributedVacp)
        return;

    if (g_bThisIsMasterNode)
    {
        if (m_pCommunicator->GetCoordNodeCount() <= 0)
        {
            ExitDistributedConsistencyMode();
            return;
        }

        XDebugPrintf("\nWaiting for Quiesce Acknowledgement from Coordinator Nodes at %s...\n", GetLocalSystemTime().c_str());
            
        if (m_pCommunicator->WaitForMsg(ENUM_MSG_QUIESCE_ACK) == -1)
        {
            if (!m_DistributedProtocolStatus)
            {
                AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, "ERROR_TIMEOUT_WAITING_FOR_QUIESCE_ACK", ERROR_TIMEOUT_WAITING_FOR_QUIESCE_ACK);
                m_DistributedProtocolStatus = ERROR_TIMEOUT_WAITING_FOR_QUIESCE_ACK;
            }
        }

        m_pCommunicator->CheckForMissingQuiesceAck();

    }
    else // client node
    {
        //Send Acknowledgement for Quiesce done
        std::string strData;
        strData.clear();

        if ((m_status & CONSISTENCY_STATUS_QUIESCE_SUCCESS) != CONSISTENCY_STATUS_QUIESCE_SUCCESS)
        {
            strData += "Error: QUIESCE Failed on the Coordinator Node ";
            strData += g_strLocalIpAddress;
            int nRet = m_pCommunicator->SendMessageToMasterNode(
                ENUM_MSG_STATUS_INFO,
                strData);
            if (0 != nRet)
            {
                std::stringstream ss;
                ss << "Failed to send status - " << strData.c_str() << " - to Master Node.Error = ";
                DebugPrintf("\n%s Error = %d\n", ss.str().c_str(), nRet);
                AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, ss.str().c_str(), nRet);
            }
            else
            {
                DebugPrintf("\nSent status - %s - to Master Node.", strData.c_str());
            }
            
            ExitDistributedConsistencyMode();
            return;
        }

        int nRet = m_pCommunicator->SendMessageToMasterNode(ENUM_MSG_QUIESCE_ACK, strData);
        if (0 != nRet)
        {
            m_DistributedProtocolStatus = ERROR_FAILED_TO_SEND_QUIESCE_ACK;
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, "ERROR_FAILED_TO_SEND_QUIESCE_ACK", ERROR_FAILED_TO_SEND_QUIESCE_ACK);
            ExitDistributedConsistencyMode();
            return;
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_CLIENT_QUIESCE_ACK_COMPLETED;
    }

    return;
}

void Consistency::TagPhase()
{
    DWORD dwWaitForIssueTag;

    if (!m_bDistributedVacp)
        return;

    if (g_bThisIsMasterNode)
    {
        if (m_pCommunicator->GetCoordNodeCount() <= 0)
        {
            ExitDistributedConsistencyMode();
            return;
        }

        //Send command to Coordinator nodes to commit/issue the tag
        std::string strData;
        strData.clear();
        
        int nRet = m_pCommunicator->SendMessageToEachCoordinatorNode(
            ENUM_MSG_TAG_COMMIT_CMD,
            strData);
        if (0 != nRet)
        {
            if (!m_DistributedProtocolStatus)
            {
                AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, "ERROR_SENDING_TAG_COMMIT_COMMAND", ERROR_SENDING_TAG_COMMIT_COMMAND);
                m_DistributedProtocolStatus = ERROR_SENDING_TAG_COMMIT_COMMAND;
            }
            ExitDistributedConsistencyMode();
            return;
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_SERVER_INSERT_TAG_CMD_COMPLETED;
    }
    else
    {
        XDebugPrintf("\nWaiting For IssueTag Command from the Master Node(Time:%s)...\n", GetLocalSystemTime().c_str());

        if (m_pCommunicator->WaitForMsg(ENUM_MSG_TAG_COMMIT_CMD) == -1)
        {
            m_DistributedProtocolStatus = ERROR_FAILED_TO_GET_INSERT_TAG_CMD;
            std::stringstream ss;
            ss << "TimeOut occurred while waiting for INSERT TAG from the Master Node at " << GetLocalSystemTime().c_str() << ". Hence this Vacp will NOT issue a Distributed Tag,but it will continue to issue a Local Tag.";
            DebugPrintf("\n%s\n", ss.str().c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, ss.str(), ERROR_FAILED_TO_GET_INSERT_TAG_CMD);
            ExitDistributedConsistencyMode();
            return;
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_CLIENT_INSERT_TAG_CMD_COMPLETED;
    }

    return;
}

void Consistency::TagAckPhase()
{
    if (!m_bDistributedVacp)
        return;
    
    if (g_bThisIsMasterNode)
    {
        if (m_pCommunicator->GetCoordNodeCount() <= 0)
        {
            ExitDistributedConsistencyMode();
            return;
        }
        
        XDebugPrintf("\nWaiting for Tag Acknowledgement from Coordinator Nodes...\n");
        if (m_pCommunicator->WaitForMsg(ENUM_MSG_TAG_COMMIT_ACK) == -1)
        {
            if (!m_DistributedProtocolStatus)
            {
                m_DistributedProtocolStatus = ERROR_TIMEOUT_WAITING_FOR_INSERT_TAG_ACK;
            }

            std::stringstream ss;
            ss << "ERROR_TIMEOUT_WAITING_FOR_INSERT_TAG_ACK";
            DebugPrintf("\n%s\n", ss.str().c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, ss.str(), ERROR_TIMEOUT_WAITING_FOR_INSERT_TAG_ACK);
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_CLIENT_INSERT_TAG_ACK_COMPLETED;
        m_pCommunicator->CheckForMissingCommitTagAck();
    }
    else // client node
    {
        //Send Acknowledgement to Master Vacp Node that it has received the Tag Commit Command
        std::string strData;
        strData.clear();

        if ((m_status & CONSISTENCY_STATUS_INSERT_TAG_SUCCESS) != CONSISTENCY_STATUS_INSERT_TAG_SUCCESS)
        {
            strData += "Error: TAG Failed on the Coordinator Node ";
            strData += g_strLocalIpAddress;
            int nRet = m_pCommunicator->SendMessageToMasterNode(
                ENUM_MSG_STATUS_INFO,
                strData);
            if (0 != nRet)
            {
                std::stringstream ss;
                ss << "Failed to send status - " << strData.c_str() << " - to Master Node.Error = " << nRet;
                DebugPrintf("\n%s\n", ss.str().c_str());
                AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, ss.str(), ERROR_FAILED_TO_SEND_STATUS_INFO_TO_MASTER);
            }
            else
            {
                DebugPrintf("\nSent status - %s - to Master Node.", strData.c_str());
            }
            
            ExitDistributedConsistencyMode();
            return;
        }

        int nRet = m_pCommunicator->SendMessageToMasterNode(
            ENUM_MSG_TAG_COMMIT_ACK,
            strData);
        if (0 != nRet)
        {
            m_DistributedProtocolStatus = ERROR_SENDING_TAG_COMMIT_ACK;
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, "ERROR_SENDING_TAG_COMMIT_ACK", m_DistributedProtocolStatus);
            ExitDistributedConsistencyMode();
            return;
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_CLIENT_QUIESCE_ACK_COMPLETED;
    }

    return;
}

void Consistency::ResumePhase()
{
    if (!m_bDistributedVacp)
        return;

    if (g_bThisIsMasterNode)
    {
        if (m_pCommunicator->GetCoordNodeCount() <= 0)
        {
            ExitDistributedConsistencyMode();
            return;
        }

        //Send RESUME_CMD to all the Client nodes
        std::string strData;
        strData.clear();
        int nRet = m_pCommunicator->SendMessageToEachCoordinatorNode(
            ENUM_MSG_RESUME_CMD,
            strData);
        if (0 != nRet)
        {
            if (!m_DistributedProtocolStatus)
            {
                m_DistributedProtocolStatus = ERROR_SENDING_TAG_COMMIT_COMMAND;
                AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, "ERROR_SENDING_TAG_COMMIT_COMMAND", ERROR_SENDING_TAG_COMMIT_COMMAND);
            }
            ExitDistributedConsistencyMode();
            return;
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_SERVER_RESUME_CMD_COMPLETED;
    }
    else // client node
    {
        //Wait for the command from Master Node to perform "Prepare" step.
        if (m_pCommunicator->WaitForMsg(ENUM_MSG_RESUME_CMD) == -1)
        {
            std::string errmsg = "Failed to receive RESUME_CMD from the Master.";
            DebugPrintf("\n%s\n", errmsg.c_str());
            m_DistributedProtocolStatus = ERROR_FAILED_TO_GET_RESUME_CMD;
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, errmsg, ERROR_FAILED_TO_GET_RESUME_CMD);
            ExitDistributedConsistencyMode();
            return;
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_CLIENT_RESUME_CMD_COMPLETED;
    }

    return;
}


void Consistency::ResumeAckPhase()
{
    if (!m_bDistributedVacp)
        return;

    if (g_bThisIsMasterNode)
    {
        /*if (m_pCommunicator->GetCoordNodeCount() <= 0)
        {
            ExitDistributedConsistencyMode();
            return;
        }*/


        DebugPrintf("\nWaiting for Resume Acknowledgement from the Coordinator Nodes...\n");
        if (m_pCommunicator->WaitForMsg(ENUM_MSG_RESUME_ACK) == -1)
        {
            if (!m_DistributedProtocolStatus)
            {
                m_DistributedProtocolStatus = ERROR_TIMEOUT_WAITING_FOR_RESUME_ACK;
                AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, "ERROR_TIMEOUT_WAITING_FOR_RESUME_ACK", ERROR_TIMEOUT_WAITING_FOR_RESUME_ACK);
            }
        }
        
        m_pCommunicator->CheckForMissingResumeAck();

        m_DistributedProtocolState = COMMUNICATOR_STATE_SERVER_RESUME_ACK_COMPLETED;
    }
    else
    {
        //Send Acknowledgement for Resume Done
        std::string strData;
        strData.clear();

        if ((m_status & CONSISTENCY_STATUS_RESUME_SUCCESS) != CONSISTENCY_STATUS_RESUME_SUCCESS)
        {
            ExitDistributedConsistencyMode();
            return;
        }

        int nRet = m_pCommunicator->SendMessageToMasterNode(ENUM_MSG_RESUME_ACK, strData);
        if (0 != nRet)
        {
            m_DistributedProtocolStatus = ERROR_FAILED_TO_SEND_RESUME_ACK;
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, "ERROR_FAILED_TO_SEND_RESUME_ACK", ERROR_FAILED_TO_SEND_RESUME_ACK);
            return;
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_CLIENT_RESUME_ACK_COMPLETED;
    }

    return;
}

bool GetDriverVersion(DRIVER_VERSION *version)
{
    bool status = true;
    int m_Driver = open(INMAGE_FILTER_DEVICE_NAME, O_RDWR );

    if (-1 == m_Driver) 
    {
        inm_printf( "Function %s @LINE %d in FILE %s :unable to open %s\n",
            FUNCTION_NAME, LINE_NO, FILE_NAME, INMAGE_FILTER_DEVICE_NAME);
        status = false;
    }
    else
    {
        if(ioctl(m_Driver, IOCTL_INMAGE_GET_DRIVER_VERSION, version ) < 0)
        {
            inm_printf( "ioctl failed: to get driver version. errno %d\n", errno);
            status = false;
            close(m_Driver);
        }
        else
        {
            inm_printf("InVolFlt driver version : %d.%d.%d.%d\n", 
                    version->ulDrMajorVersion,
                    version->ulDrMinorVersion,
                    version->ulDrMinorVersion2,
                    version->ulDrMinorVersion3);
            status = true;
            close(m_Driver);
        }
    }

    return status;
}

bool DriverSupportsTagV2()
{
    if (gdriverVersion.ulDrMinorVersion3 & INVOLFLT_FEATURE_MASK_TAGV2 == INVOLFLT_FEATURE_MASK_TAGV2)
        return true;
    else
        return false;
}

bool DriverSupportsIoBarrier()
{
#if defined (linux)
    if (gdriverVersion.ulDrMinorVersion3 & INVOLFLT_FEATURE_MASK_IOBARRIER == INVOLFLT_FEATURE_MASK_IOBARRIER)
        return true;
    else
        return false;
#else
        return false;
#endif
}

#if defined (linux)
bool DriverSupportsUserModeFreeze()
{
    if (gdriverVersion.ulDrMinorVersion3 & INVOLFLT_FEATURE_MASK_TAGV2 == INVOLFLT_FEATURE_MASK_TAGV2)
        return true;
    else
        return false;
}
#endif

bool DriverSupportsSyncTags()
{
    if (gdriverVersion.ulDrMinorVersion3 & INVOLFLT_FEATURE_MASK_TAGV2 == INVOLFLT_FEATURE_MASK_TAGV2)
        return false;
    else
        return true;
}

unsigned int Consistency::CreateTagIoctlBufferForOlderDriver(char ** tagBuffer, const set<std::string> &affectedVolumes, const set<TagInfo_t> &finalTags)
{
    // create ioctl buffer
    // in following form:
    //	Flags + TotalNumberOfVolumes + 
    // first DeviceName Length + First Device Name 
    // second DeviceName Length + Second Device Name
    // ...
    // + TotalNumberOfTags 
    // + TagLenOfFirstTag + FirstTagData 
    // + TagLenofSecondTag + SecondTagData ...

    unsigned int flags = 0;		
    size_t numTags = finalTags.size();
    size_t numVolumes = affectedVolumes.size();

    // by default, we want to issue tags atomically across volumes
    flags |= TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP;

    //For sun we are freezing FS from userspace
#if defined(sun) | defined (SV_AIX)
    // TBD
    // flags |= TAG_FS_FROZEN_IN_USERSPACE;
#endif

//     if(FSTag)
    flags |= TAG_FS_CONSISTENCY_REQUIRED;

    uint32_t total_buffer_size;
    INM_SAFE_ARITHMETIC(total_buffer_size = sizeof(flags) + sizeof(numVolumes) + ( numVolumes * InmSafeInt<size_t>::Type(sizeof(unsigned short))) /* + size of each volume */ + sizeof(numTags) + ( numTags * InmSafeInt<size_t>::Type(sizeof(unsigned short))) /* + size of each tag */, INMAGE_EX(sizeof(flags))(sizeof(numVolumes))(numVolumes)(sizeof(unsigned short))(sizeof(numTags))(numTags)(sizeof(unsigned short)))

    // add the size of each volume name length

    set<string>::const_iterator viter = affectedVolumes.begin();
    set<string>::const_iterator vend = affectedVolumes.end();

    while(viter != vend)
    {
        string devname = *viter;
        INM_SAFE_ARITHMETIC(total_buffer_size += InmSafeInt<size_t>::Type(strlen(devname.c_str())), INMAGE_EX(total_buffer_size)(strlen(devname.c_str())))
        ++viter;
    }

    set<TagInfo_t>::const_iterator titer = finalTags.begin();
    set<TagInfo_t>::const_iterator tend = finalTags.end();

    while(titer != tend)
    {
        INM_SAFE_ARITHMETIC(total_buffer_size += InmSafeInt<unsigned short>::Type((*titer).tagLen), INMAGE_EX(total_buffer_size)((*titer).tagLen))
        ++titer;
    }

    if (gbissueSyncTags)
    {
        // To add uuid
        INM_SAFE_ARITHMETIC(total_buffer_size += InmSafeInt<size_t>::Type(sizeof (unsigned short)) + INM_UUID_STR_LEN, INMAGE_EX(total_buffer_size)(sizeof (unsigned short)))

        // Align to 4-byte boundary
        size_t rem;
        INM_SAFE_ARITHMETIC(rem = total_buffer_size % InmSafeInt<size_t>::Type(sizeof(int)), INMAGE_EX(total_buffer_size)(sizeof(int)))
        size_t sizeofintlessrem;
        INM_SAFE_ARITHMETIC(sizeofintlessrem = InmSafeInt<size_t>::Type(sizeof(int)) - rem, INMAGE_EX(sizeof(int))(rem))
        size_t add = rem ? sizeofintlessrem : 0;
        INM_SAFE_ARITHMETIC(total_buffer_size += InmSafeInt<size_t>::Type(add), INMAGE_EX(total_buffer_size)(add))

        uint32_t status_offset = total_buffer_size;

        INM_SAFE_ARITHMETIC(total_buffer_size += (InmSafeInt<size_t>::Type(sizeof (int)) * numVolumes) /*return status for each volume*/, INMAGE_EX(total_buffer_size)(sizeof (int))(numVolumes))
    }

    // char *buffer = static_cast<char*>(calloc(total_buffer_size,1));
    char *buffer = new char [total_buffer_size];
    if(buffer == NULL)
    {
        inm_printf( "Function %s @LINE %d in FILE %s :insufficient memory\n", FUNCTION_NAME, LINE_NO, FILE_NAME);
        return 0;
    }

    inm_printf( "\nSending Following Tag Request ...\n");

    uint32_t current_buf_size = 0; // size of buffer currently filled

    if (gbissueSyncTags)
    {
        inm_memcpy_s(buffer + current_buf_size,(total_buffer_size-current_buf_size), &INM_UUID_STR_LEN, sizeof (INM_UUID_STR_LEN));
        current_buf_size += sizeof(INM_UUID_STR_LEN);

        string sync_tag_uuid = uuid_gen();

        inm_memcpy_s(buffer + current_buf_size, (total_buffer_size-current_buf_size), sync_tag_uuid.c_str(), INM_UUID_STR_LEN);
        current_buf_size += INM_UUID_STR_LEN;
    }


    inm_memcpy_s(buffer + current_buf_size, (total_buffer_size-current_buf_size), &flags, sizeof (flags));
    current_buf_size += sizeof(flags);

    inm_printf( "Flags = %d\n", flags);

    inm_memcpy_s(buffer + current_buf_size , (total_buffer_size-current_buf_size), &numVolumes, sizeof (numVolumes));
    current_buf_size += sizeof(numVolumes);

    inm_printf( "Num. Volumes = %d\n", numVolumes);

    unsigned short curvol = 1;
    viter = affectedVolumes.begin();
    while(viter != vend)
    {
        string devname = *viter;
        unsigned short len = devname.size();
        inm_memcpy_s(buffer + current_buf_size, (total_buffer_size-current_buf_size), &len, sizeof(unsigned short)); ;
        current_buf_size += sizeof(len);

        inm_memcpy_s(buffer + current_buf_size, (total_buffer_size-current_buf_size), devname.c_str(), len);
        current_buf_size += len;

        inm_printf( "Volume: %d Name: %s Length:%d\n", curvol, devname.c_str(), devname.size());

        ++viter;
        ++curvol;
    }

    inm_memcpy_s(buffer + current_buf_size, (total_buffer_size-current_buf_size), &numTags, sizeof(unsigned short));;
    current_buf_size += sizeof(numTags);
    inm_printf( "Num. Tags = %d\n", numTags);

    unsigned short curtag = 1;
    titer = finalTags.begin();
    while(titer != tend)
    {
        inm_memcpy_s(buffer + current_buf_size, (total_buffer_size-current_buf_size), &(*titer).tagLen, sizeof(unsigned short)) ;
        current_buf_size += sizeof(unsigned short);

        inm_memcpy_s(buffer + current_buf_size, (total_buffer_size-current_buf_size), (*titer).tagData, (*titer).tagLen);
        current_buf_size += (*titer).tagLen;

        inm_printf( "Tag: %d Length:%d\n", curtag, (*titer).tagLen);

        ++titer;
        ++curtag;
    }

    *tagBuffer = buffer;
    return total_buffer_size;
}

unsigned int Consistency::CreateTagIoctlBuffer(char ** tagBuffer, const set<std::string> &volumes, const set<TagInfo_t> &tags)
{

    if ( gbRemoteSend || !DriverSupportsTagV2() )
        return CreateTagIoctlBufferForOlderDriver(tagBuffer, volumes, tags);

    // create ioctl buffer using tag_info_t
    unsigned int flags = 0;		
    size_t numTags = tags.size();
    size_t numVolumes = volumes.size();

    // by default, we want to issue tags atomically across volumes
    flags |= TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP;

    //For sun we are freezing FS from userspace
#if defined(sun) | defined (SV_AIX)
    // flags |= TAG_FS_FROZEN_IN_USERSPACE;
#endif

    if (IS_CRASH_TAG(m_TagType) && gbSystemLevel)
    {
        numVolumes = 0;
        flags |= TAG_ALL_PROTECTED_VOLUME_IOBARRIER;
    }

    if (!IS_CRASH_TAG(m_TagType))
        flags |= TAG_FS_CONSISTENCY_REQUIRED;

    uint32_t total_buffer_size;

    INM_SAFE_ARITHMETIC(total_buffer_size = sizeof(tag_info_t_v2) + ( numVolumes * InmSafeInt<size_t>::Type(sizeof(volume_info_t))) + ( numTags * InmSafeInt<size_t>::Type(sizeof(tag_names_t))) , INMAGE_EX(sizeof(tag_info_t_v2))(numVolumes)(sizeof(volume_info_t))(numTags)(sizeof(tag_names_t)))

    // char *buffer = static_cast<char*>(calloc(total_buffer_size,1));
    char *buffer = new (std::nothrow) char [total_buffer_size];
    if(buffer == NULL)
    {
        inm_printf( "Error: %s failed to allocate %u bytes.\n", FUNCTION_NAME, total_buffer_size);
        return 0;
    }

    tag_info_t_v2 *tag_info = (tag_info_t_v2 *) buffer;
    
    volume_info_t *vol_info = (volume_info_t *) (buffer + sizeof(tag_info_t_v2));
    tag_names_t *tag_names = (tag_names_t*) (buffer + sizeof(tag_info_t_v2) + numVolumes * sizeof(volume_info_t));

    tag_info->flags = flags;
    tag_info->nr_vols = numVolumes;
    tag_info->nr_tags = numTags;

    if (IS_CRASH_TAG(m_TagType))
        tag_info->timeout = 0;
    else
    {
        long drainBarrierTimeout = VACP_TAG_IOCTL_COMMIT_TIMEOUT;

        VacpConfigPtr pVacpConf = VacpConfig::getInstance();

        if (pVacpConf.get() != NULL)
        {
            if (pVacpConf->getParamValue(VACP_CONF_PARAM_DRAIN_BARRIER_TIMEOUT, drainBarrierTimeout))
            {
                if (drainBarrierTimeout < VACP_TAG_IOCTL_COMMIT_TIMEOUT)
                {
                    inm_printf("\nDrain Barrier Timeout %ld is less than allowed minimum. Setting to default value %d.\n", 
                                    drainBarrierTimeout, VACP_TAG_IOCTL_COMMIT_TIMEOUT);

                    drainBarrierTimeout = VACP_TAG_IOCTL_COMMIT_TIMEOUT;
                }
                else if (drainBarrierTimeout > VACP_APP_TAG_COMMIT_TIMEOUT)
                {
                    inm_printf("\nDrain Barrier Timeout %ld is more than allowed maximum. Setting to max value %d.\n", 
                                    drainBarrierTimeout, VACP_APP_TAG_COMMIT_TIMEOUT);

                    drainBarrierTimeout = VACP_APP_TAG_COMMIT_TIMEOUT;
                }
                else
                    inm_printf("\nDrain Barrier Timeout is set to custom value of %ld ms.\n", 
                                    drainBarrierTimeout);
            }
        }

        tag_info->timeout = drainBarrierTimeout;
    }

    inm_memcpy_s(tag_info->tag_guid, sizeof(tag_info->tag_guid), m_strContextUuid.c_str(), INM_UUID_STR_LEN);

    tag_info->vol_info = vol_info;
    tag_info->tag_names = tag_names;

    inm_printf( "Flags = 0x%x\n", flags);
    inm_printf( "Num. Volumes = %d\n", numVolumes);

    if (numVolumes != 0)
    {
        set<string>::const_iterator viter = volumes.begin();

        unsigned short curvol = 1;
        while(viter != volumes.end())
        {
            string volname = *viter;
            vol_info->flags = flags;
            vol_info->status = 0;
            inm_strcpy_s(vol_info->vol_name, ARRAYSIZE(vol_info->vol_name), volname.c_str());

            inm_printf( "Volume: %d Name: %s Length:%d\n", curvol, vol_info->vol_name, volname.size());

            vol_info++;
            ++viter;
            ++curvol;
        }
    }

    set<TagInfo_t>::const_iterator titer = tags.begin();

    inm_printf( "Num. Tags = %d\n", numTags);

    unsigned short curtag = 1;
    while(titer != tags.end())
    {
        inm_memcpy_s(tag_names->tag_name, TAG_MAX_LENGTH, (*titer).tagData, (*titer).tagLen);
        tag_names->tag_len = (*titer).tagLen;
        inm_printf( "Tag: %d Length:%d\n", curtag, (*titer).tagLen);

        tag_names++;
        ++titer;
        ++curtag;
    }

    *tagBuffer = buffer;
    return total_buffer_size;
}

bool SendTagsToRemoteServer(char * buffer, unsigned int bufSize, std::string vacpServer, unsigned short vacpServerPort)
{
    bool rv = true;
    do
    {
#if defined (sun)
        // Flags |= TAG_FS_FROZEN_IN_USERSPACE; // in case remote tag, volumes were always frozen from userspace
#endif
        // create an ACE process to run the generated script
        int result = 0;
        set<string>::const_iterator deviceIter;
        //Connect to server
        if( !prepare(vacpServerPort, vacpServer) )
        {
            rv = false;
            break;
        }

        //Issue packets to remote server
        bool bResult = false;
        uint32_t uiReturn = 0;
        VACP_SERVER_RESPONSE tVacpServerResponse;

        //Send tags to remote vacp server
        inm_printf("Sending tags to the remote server ...\n");
        inm_printf("Bytes passed to RemoteServer = %d \n", bufSize);

        // uint32_t tmplongdata = htonl(total_buffer_size);
        uint32_t tmplongdata = htonl(bufSize);
        unsigned short us = 1;
        char arch = *((char *)&us);//will yeild 1 on little endians and 0 on big endians.

        const char *svdarch = (arch)?"SVDL":"SVDB";
        inm_printf ("Arch is %s\n", svdarch);

        //Send Arch  to server
        if(send(vacpServSocket, svdarch, 4 , 0) < 0)
        {
            inm_printf ( "FAILED Couldn't send total arch data to remote server");
            rv = false;
            break;
        }				

        //Send request buffer to server
        if(send(vacpServSocket, (char *)&tmplongdata, sizeof tmplongdata, 0) < 0)
        {
            inm_printf ( "FAILED Couldn't send total Tag data length to remote server");
            rv = false;
            break;
        }				
        else if(send(vacpServSocket, (char*)buffer, bufSize, 0) < 0)
        {
            inm_printf ( "FAILED Couldn't send Tag data to remote server");
            rv = false;
            break;
        }
        //Receive response buffer from server
        else if(recv(vacpServSocket,(char*)&tVacpServerResponse, sizeof tVacpServerResponse,0) <= 0)
        {
            inm_printf ( "FAILED Couldn't get acknowledgement from remote server");
            rv = false;
            break;
        }

        //Validate response
        tVacpServerResponse.iResult  = ntohl(tVacpServerResponse.iResult);
        if(tVacpServerResponse.iResult > 0)
        {
            bResult = true;
        }
        if(buffer)
        {
            free(buffer);
        }					

        tVacpServerResponse.val = ntohl(tVacpServerResponse.val);
        tVacpServerResponse.str[tVacpServerResponse.val] = '\0';

        inm_printf ("INFO: VacpServerResponse:[%d] %s\n", tVacpServerResponse.iResult, tVacpServerResponse.str);

        if(bResult)
        {
            //inm_printf( "Successfully sent tags to following volumes,\n");					
            inm_printf( "Successfully sent tags to the Remote Server ...\n");
            rv = true;
        }
        else
        {
            inm_printf( "FAILED to Send Tag to the Remote Server for following Server devices\n");
            inm_printf("Error code:%d", errno);					
            inm_printf("\n");
            rv = false;
        }
    } while (0);

    return rv;
}

bool Consistency::SendTagsToLocalDriver(const char * buffer)
{
    bool rv = true;
    int m_Driver = open(INMAGE_FILTER_DEVICE_NAME, O_RDWR );

    if (-1 == m_Driver) 
    {
        inm_printf( "Function %s @LINE %d in FILE %s :unable to open %s\n",
            FUNCTION_NAME, LINE_NO, FILE_NAME, INMAGE_FILTER_DEVICE_NAME);
        m_exitCode = VACP_DRIVER_OPEN_FAILED;
        rv = false;
        return rv;
    }

    if (IS_CRASH_TAG(m_TagType) && !m_bDistributedVacp)
    {
        steady_clock::time_point endTime = steady_clock::now();
        endTime += milliseconds(VACP_CRASHTAG_MAX_RETRY_TIME);

        do 
        {
            m_perfCounter.StartCounter("IoBarrier");

            int nRet = ioctl(m_Driver, IOCTL_INMAGE_IOBARRIER_TAG_VOLUME, (tag_info_t_v2 *)buffer);
            if (nRet < 0)
            {
                int nError = errno;
                if (nError == EAGAIN)
                {
                    if (steady_clock::now() < endTime)
                    {
                        ACE_Time_Value tv;
                        tv.msec(VACP_CRASHTAG_RETRY_WAIT_TIME);
                        ACE_OS::sleep(tv);
                        continue;
                    }

                    inm_printf( "ioctl IOCTL_INMAGE_IOBARRIER_TAG_VOLUME failed: tag failed after retries for %d ms. errno %d\n", VACP_CRASHTAG_MAX_RETRY_TIME, nError);
                    m_exitCode = VACP_BARRIER_HOLD_FAILED;
                }
                else
                {
                    inm_printf( "ioctl IOCTL_INMAGE_IOBARRIER_TAG_VOLUME failed: tag could not be issued. errno %d\n", nError);
                }
                if (nError == 1)
                {
                    m_exitCode = VACP_E_DRIVER_IN_NON_DATA_MODE;
                }
                rv = false;
                std::string errMsg("Tags are not issuedy as ioctl IOCTL_INMAGE_IOBARRIER_TAG_VOLUME failed.");
                inm_printf("%s\n", errMsg.c_str());
                AddToVacpFailMsgs(VACP_MODULE_FAILURE_CODE, errMsg, m_exitCode);
            }
            else if (nRet == 1)
            {
                rv = false;
                m_exitCode = VACP_DRIVER_NOT_INSERTED_TAG_ON_ALL_DISKS;
                std::string errMsg("Tags are issued on some of the devices only.");
                inm_printf("%s\n", errMsg.c_str());
                AddToVacpFailMsgs(VACP_MODULE_FAILURE_CODE, errMsg, m_exitCode);
            }
            else
            {
                m_perfCounter.EndCounter("IoBarrier");
                uint64_t TagInsertTime = GetTimeInSecSinceEpoch1970();
                m_tagTelInfo.Add(TagInsertTimeKey, boost::lexical_cast<std::string>(TagInsertTime));
                inm_printf("TagInsertTime: %lld \n", TagInsertTime);
                inm_printf("\nTags successfully issued.\n");
            }
            break;
        } while (true);

    }
    else if (DriverSupportsTagV2())
    {
        steady_clock::time_point endTime = steady_clock::now();
        endTime += milliseconds(VACP_TAG_IOCTL_MAX_RETRY_TIME);

        do
        {
            tag_info_t_v2 *taginfo = (tag_info_t_v2 *) buffer;

            int nRet = ioctl(m_Driver, IOCTL_INMAGE_TAG_VOLUME_V2, (tag_info_t_v2 *)buffer);
            if (nRet < 0)
            {
                int nError = errno;

                if (nError == EAGAIN)
                {
                    if (steady_clock::now() < endTime)
                    {
                        ACE_Time_Value tv;
                        tv.msec(VACP_TAG_IOCTL_RETRY_WAIT_TIME);
                        ACE_OS::sleep(tv);
                        continue;
                    }

                    inm_printf( "ioctl IOCTL_INMAGE_TAG_VOLUME_V2 failed: insert tag failed after retries for %d ms. errno %d\n", VACP_TAG_IOCTL_MAX_RETRY_TIME, nError);
                    m_exitCode = VACP_INSERT_TAG_FAILED;
                }
                else
                {
                    inm_printf( "ioctl IOCTL_INMAGE_TAG_VOLUME_V2 failed: insert tag could not be issued. errno %d\n", nError);
                    m_exitCode = VACP_INSERT_TAG_FAILED;
                }
                tag_info_t_v2 *tag_info = (tag_info_t_v2 *) buffer;
                volume_info_t *vol_info = tag_info->vol_info;
                for (int i = 0; i<tag_info->nr_vols; i++)
                {
                    if ((vol_info->status != 0) && ((vol_info->status & STATUS_TAG_WO_METADATA) != 0))
                    {
                        m_exitCode = VACP_E_DRIVER_IN_NON_DATA_MODE;
                        break;
                    }
                    vol_info++;
                }
                rv = false;
                std::string errMsg("Tags are not issuedy as ioctl IOCTL_INMAGE_TAG_VOLUME_V2 failed.");
                inm_printf("%s\n", errMsg.c_str());
                AddToVacpFailMsgs(VACP_MODULE_FAILURE_CODE, errMsg, m_exitCode);
            }
            else if (nRet == 1)
            {
                inm_printf("Tags are issued on some of the devices only.\n");
                m_exitCode = VACP_DRIVER_NOT_INSERTED_TAG_ON_ALL_DISKS;
            }
            else
            {
                inm_printf( "Tags are issued on all the devices.\n");
            }

            if (nRet >= 0)
            {
                uint64_t TagInsertTime = GetTimeInSecSinceEpoch1970();
                m_tagTelInfo.Add(TagInsertTimeKey, boost::lexical_cast<std::string>(TagInsertTime));
                inm_printf("TagInsertTime: %lld \n", TagInsertTime);
            }

            break;

        } while (true);
    }
    else
    {
        if (gbissueSyncTags)
        {
            if(ioctl(m_Driver, IOCTL_INMAGE_SYNC_TAG_VOLUME, buffer) < 0)
            {
                int nError = errno;
                inm_printf( "ioctl IOCTL_INMAGE_SYNC_TAG_VOLUME failed: sync tags could not be issued. errno %d\n", nError);
                m_exitCode = nError;
                rv = false;
            }
            else
                inm_printf("\nSync tags successfully issued.\n");
        }
        else
        {
            if(ioctl(m_Driver, IOCTL_INMAGE_TAG_VOLUME, buffer) < 0)
            {
                int nError = errno;
                inm_printf( "ioctl IOCTL_INMAGE_TAG_VOLUME failed: tags could not be issued. errno %d\n", nError);
                m_exitCode = VACP_INSERT_TAG_FAILED;
                rv = false;
            }
            else
                inm_printf("\nTags successfully issued.\n");
        }
        if(rv)
        {
            uint64_t TagInsertTime = GetTimeInSecSinceEpoch1970();
            m_tagTelInfo.Add(TagInsertTimeKey, boost::lexical_cast<std::string>(TagInsertTime));
            inm_printf("TagInsertTime: %lld \n", TagInsertTime);
        }
    }

    close(m_Driver);
    return rv;
}

bool Consistency::CommitRevokeTags()
{
    bool rv = true;

    bool commit = ((m_status & CONSISTENCY_STATUS_RESUME_SUCCESS) == CONSISTENCY_STATUS_RESUME_SUCCESS) ? true : false;
    
    // if the resume command is not received, should revoke the tag
    if ( (m_DistributedProtocolStatus == ERROR_FAILED_TO_GET_RESUME_CMD) ||
         (m_DistributedProtocolStatus == ERROR_TIMEOUT_WAITING_FOR_INSERT_TAG_ACK) )
        commit = false;
    
    const char * msg = commit ? "Commit" : "Revoke";

    DebugPrintf("\n\n%s the tags with the driver ...\n", msg);

    if (!commit)
        m_status &= ~CONSISTENCY_STATUS_INSERT_TAG_SUCCESS;


    int m_Driver = open(INMAGE_FILTER_DEVICE_NAME, O_RDWR );

    if (-1 == m_Driver) 
    {
        inm_printf( "Function %s @LINE %d in FILE %s :unable to open %s\n",
            FUNCTION_NAME, LINE_NO, FILE_NAME, INMAGE_FILTER_DEVICE_NAME);
        rv = false;
        return rv;
    }


    flt_tag_commit_t tinfo;
    tinfo.ftc_flags = commit ? TAG_COMMIT : TAG_REVOKE;
    inm_memcpy_s(tinfo.ftc_guid, sizeof(tinfo.ftc_guid), m_strContextUuid.c_str(), INM_UUID_STR_LEN);

    if(ioctl(m_Driver, IOCTL_INMAGE_TAG_COMMIT_V2, &tinfo) < 0)
    {
        if (commit)
        {
            inm_printf("\n\nioctl failed: tags could not be commited. Tags will be dropped by driver. errno %d\n", errno);
            rv = false;
        }
    }

    if (!commit)
        rv = false;

    if (rv)
    {
        uint64_t TagCommitTime = GetTimeInSecSinceEpoch1970();
        m_tagTelInfo.Add(TagCommitTimeKey, boost::lexical_cast<std::string>(TagCommitTime));
        inm_printf("Tags successfully commited.\n");
    }

    close(m_Driver);

    return rv;
}

bool Consistency::CheckTagsStatus(const char *buffer, const unsigned int size, const int wait_time, const set<string> volumes)
{
    bool rv = true;

    if (DriverSupportsTagV2())
    {

        tag_info_t_v2 *tag_info = (tag_info_t_v2 *) buffer;
        
        volume_info_t *vol_info = tag_info->vol_info;

        int count_inserted_volumes = 0;
        std::stringstream ss_tag_not_inserted_volumes;

        for (int i = 0; i<tag_info->nr_vols; i++)
        {
            if (vol_info->status != 0)
            {
                if ((vol_info->status & STATUS_TAG_WO_METADATA) != 0)
                {
                    inm_printf("Warning: tags not issued for %s as it is not in write order mode.\n", vol_info->vol_name);

                    if (!ss_tag_not_inserted_volumes.str().empty())
                        ss_tag_not_inserted_volumes << "; ";
                    ss_tag_not_inserted_volumes << vol_info->vol_name << ":reason - non-write order mode";

                    // retain any prev set exit code
                    if (!m_exitCode)
                        m_exitCode = VACP_E_DRIVER_IN_NON_DATA_MODE;
                }
                else if (vol_info->status == 1)
                {
                    inm_printf("Warning: tags not issued for %s as tag status code=1.\n", vol_info->vol_name);

                    if (!ss_tag_not_inserted_volumes.str().empty())
                        ss_tag_not_inserted_volumes << "; ";
                    ss_tag_not_inserted_volumes << vol_info->vol_name <<":reason - other";

                    // overwrite exit code with this error for other errors
                    m_exitCode = VACP_DRIVER_NOT_INSERTED_TAG_ON_ALL_DISKS;
                }
                else if (vol_info->status == 2)
                    inm_printf("Warning: tags not issued for %s as it is not protected.\n", vol_info->vol_name);

            }
            else
                count_inserted_volumes++;


            vol_info++;
        }

        if (tag_info->nr_vols)
        {
            m_tagTelInfo.Add(TagInsertKey, boost::lexical_cast<std::string>(count_inserted_volumes));
            inm_printf("Tag is inserted for %d disks.\n",count_inserted_volumes);

            if (!ss_tag_not_inserted_volumes.str().empty())
            {
                rv = false;
                std::string errMsg("Tag not issued disks: ");
                errMsg += ss_tag_not_inserted_volumes.str();
                inm_printf("%s\n", errMsg.c_str());
                AddToVacpFailMsgs(VACP_MODULE_FAILURE_CODE, errMsg, m_exitCode);
            }
        }
    }
    else if (gbissueSyncTags)
    {
        unsigned int numVolumes = volumes.size();
        unsigned int status_offset = size - (sizeof (int) * numVolumes);


        bool anyTagsPending = false;

        int *status = (int *) (buffer + status_offset);

        for (int i = 0; i < numVolumes; i++)
        {
            if ( status[i] == STATUS_PENDING)
            {
                anyTagsPending = true;
            }
        }

        if (!anyTagsPending)
        {
            inm_printf("All sync tags have been processed succesfully.\n");
            return rv;
        }

        //ThawDevices(volumes); // TODO: check if this call required here

        unsigned short total_status_buffer_size =  sizeof (unsigned short) // length of the guid
                                        + INM_UUID_STR_LEN // guid
                                        + sizeof (unsigned short); // wait time
        status_offset = total_status_buffer_size; //remember where status starts

        //room to store status
        INM_SAFE_ARITHMETIC(total_status_buffer_size += InmSafeInt<size_t>::Type(sizeof (int)) * numVolumes, INMAGE_EX(total_status_buffer_size)(sizeof(int)))

        // char * stat_buffer = static_cast<char*>(calloc(total_status_buffer_size,1));
        char *stat_buffer = new char [total_status_buffer_size];
        if(!stat_buffer)
        {
            rv = false;
            inm_printf( "Function %s @LINE %d in FILE %s :insufficient memory\n",
            FUNCTION_NAME, LINE_NO, FILE_NAME);
            return rv;
        }

        unsigned int current_buf_size = 0;

        inm_memcpy_s(stat_buffer + current_buf_size, total_status_buffer_size-current_buf_size, &INM_UUID_STR_LEN, sizeof (INM_UUID_STR_LEN));
        current_buf_size += sizeof(INM_UUID_STR_LEN);

        inm_memcpy_s(stat_buffer + current_buf_size, total_status_buffer_size-current_buf_size, buffer, INM_UUID_STR_LEN);
        current_buf_size += INM_UUID_STR_LEN;

        inm_memcpy_s(stat_buffer + current_buf_size, total_status_buffer_size-current_buf_size, &wait_time, sizeof (wait_time));
        current_buf_size += sizeof(wait_time);

        status = (int *) (stat_buffer + status_offset); //reset status to start of status in buffer

        int m_Driver;
        m_Driver = open(INMAGE_FILTER_DEVICE_NAME, O_RDWR );

        if (-1 == m_Driver) 
        {
            inm_printf( "Function %s @LINE %d in FILE %s :unable to open %s\n",
                FUNCTION_NAME, LINE_NO, FILE_NAME, INMAGE_FILTER_DEVICE_NAME);
            return false;
        }

        if(ioctl(m_Driver, IOCTL_INMAGE_GET_TAG_VOLUME_STATUS, stat_buffer) < 0)
        {
            rv = false;
            inm_printf( "Function %s @LINE %d in FILE %s :IOCTL_INMAGE_GET_TAG_VOLUME_STATUS  returned error\n",
                FUNCTION_NAME, LINE_NO, FILE_NAME);
            close(m_Driver);
            return rv;
        }

        /*check for tag status for all volumes */
        anyTagsPending = false;
        for (int i = 0; i < numVolumes; i++)
        {
            if (status[i] == STATUS_PENDING)
            {
                anyTagsPending = true;
                set<string>::const_iterator it = volumes.begin() ;

                for(int j=i;j>0;--j)
                    ++it;

                inm_printf ("Function %s : Tag for %s is still pending\n", FUNCTION_NAME, (*it).c_str());
            }
        }

        if(!anyTagsPending)
            inm_printf ("All Tags processed.\n");
        else
        {
            inm_printf ("Warning : Tags were not processed within the time out period.\n");
            rv = false;
        }

        close(m_Driver);
    }

    return rv;
}

static void LogCallback(LogLevel logLevel, const char *msg)
{
    inm_printf(msg);
}

void RunCrashConsistency()
{
    CrashConsistency::CreateInstance();
    CrashConsistency::Get().InitSchedule();

    if (gbParallelRun && (0 == CrashConsistency::Get().GetConsistencyInterval()))
    {
        inm_printf("%s: not starting crash consistency as interval in parallel run is 0.\n", __FUNCTION__);
        return;
    }

    do {
        CrashConsistency::Get().InitializeCommunicator();
        steady_clock::time_point next = CrashConsistency::Get().GetSchedule(g_bSignaleQuitFromCrashConsisency);
        if (next <= steady_clock::now())
        {
            CrashConsistency::Get().SetSchedule();
            CrashConsistency::Get().Process();
            CrashConsistency::Get().SetNextSchedule();
        }

        if (gbParallelRun)
            g_Quit->Wait((g_bDistributedVacp && !g_bThisIsMasterNode) ? gnRemoteScheduleCheckInterval: gnParallelRunScheduleCheckInterval);

    } while (gbParallelRun && !g_Quit->IsSignalled());
}

void RunAppConsistency()
{
    AppConsistency::CreateInstance();
    AppConsistency::Get().InitSchedule();

    if (gbParallelRun && (0 == AppConsistency::Get().GetConsistencyInterval()))
    {
        inm_printf("%s: not starting crash consistency as interval in parallel run is 0.\n", __FUNCTION__);
        g_bSignaleQuitFromCrashConsisency = true;
        return;
    }

    do{
        AppConsistency::Get().InitializeCommunicator();
        steady_clock::time_point next = AppConsistency::Get().GetSchedule(true);
        if (next <= steady_clock::now())
        {
            AppConsistency::Get().SetSchedule();
            AppConsistency::Get().Process();
            AppConsistency::Get().SetNextSchedule();
        }

        if (gbParallelRun)
            g_Quit->Wait((g_bDistributedVacp && !g_bThisIsMasterNode) ? gnRemoteScheduleCheckInterval : gnParallelRunScheduleCheckInterval);

    } while (gbParallelRun && !g_Quit->IsSignalled());
}

void RunBaselineConsistency()
{
    BaselineConsistency::CreateInstance();

    BaselineConsistency::Get().Process();
}

void InitQuitEvent()
{
    char EventName[256];

    pid_t processId = ACE_OS::getpid();

    if (ACE_INVALID_PID != processId)
    {
        inm_sprintf_s(EventName, ARRAYSIZE(EventName), "%s%d", AGENT_QUITEVENT_PREFIX, processId);
        g_Quit.reset(new Event(false, false, EventName));
    }
}

void GetLastConsistencyScheduleTime()
{
    namespace bpt = boost::property_tree;
    bpt::ptree pt;

    try {
        if (!boost::filesystem::exists(g_strScheduleFilePath))
            return;

        bpt::json_parser::read_json(g_strScheduleFilePath, pt);

        g_lastAppScheduleTime = pt.get<uint64_t>(LastAppConsistencyScheduleTime, 0);
        g_lastCrashScheduleTime = pt.get<uint64_t>(LastCrashConsistencyScheduleTime, 0);
    }
    catch (const std::exception &e)
    {
        DebugPrintf("%s : reading consistency schedule file failed. %s\n",
            __FUNCTION__,
            e.what());
        return;
    }
    catch (...)
    {
        DebugPrintf("%s : reading consistency schedule file failed with unknown exception\n",
            __FUNCTION__);
        return;
    }

    DebugPrintf("%s: LastAppConsistencyScheduleTime :%lld, LastCrashConsistencyScheduleTime : %lld\n",
        __FUNCTION__, g_lastAppScheduleTime, g_lastCrashScheduleTime);

    return;
}

unsigned long ExecuteScript(const std::string& strScript)
{

    inm_printf("execute script : %s\n", strScript.c_str());
    unsigned long rv = 0;

    if (strScript.empty())
        return rv;

    ACE_Process_Options scriptPo;
    //prepend ./ to the prescript
    std::string strExecutePostScript = "./";
    if(strScript.find("/") == strScript.npos)
    {
        strExecutePostScript += strScript;
        scriptPo.command_line(strExecutePostScript.c_str());
    }
    else
    {
        scriptPo.command_line(strScript.c_str());
    }
    pid_t scriptPid = ACE_Process_Manager::instance()->spawn(scriptPo);
    if(ACE_INVALID_PID == scriptPid)
    {
        inm_printf("Failed to execute the script : %s\n", strScript.c_str());
        rv = ACE_INVALID_PID;
    }
    else
    {
        ACE_exitcode scriptExitCode = 0;
        ACE_Process_Manager::instance()->wait(scriptPid, &scriptExitCode);
        if(scriptExitCode != 0)
        {
            inm_printf("Script : %s \n failed with Exit Code: %d\n",
                strScript.c_str(),
                scriptExitCode);

            rv = scriptExitCode;
        }
   }
   scriptPo.release_handles();
   return rv;
}

bool DiscoverDisksAndVolumes(bool bFSTag, std::set<std::string>& volumesToBeTagged)
{
    boost::mutex::scoped_lock guard(g_diskInfoMutex);

    bool rv = false;
    gInputVolumes.clear();
    gDiskList.clear();
    gVolumesToBeTagged.clear();

    rv = ParseVolumeList(gInputVolumes, gDiskList, g_cstrVolumes, SEPARATOR_COMMA);
    if(!rv)
    {
        inm_printf("\nParseVolumeList failed\n");
        return rv;
    }

    rv = GetVolumesToBeTagged(gInputVolumes, volumesToBeTagged, bFSTag);
    if(!rv)
    {
        inm_printf("\nGetVolumesToBeTagged failed\n");
        return rv;
    }

    return rv;
}

bool IsValidGuid(const std::string& strGuid)
{
    const char *regexForGuid =  "^[{]?[0-9a-fA-F]{8}-([0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}[}]?$";
    boost::regex guidRegex(regexForGuid);

    return boost::regex_match(strGuid, guidRegex);
}

int main(int argc, char ** argv)
{
    /*
    1. parse
    2. validate each volume
    3. generate tag identifiers
    4. issue tag
    5. print success/failure.

    */

    for (int i = ACE::max_handles () - 1; i >= 0; i--)
        ACE_OS::close (i);

    /* predefining file descriptors 0, 1 and 2
    * so that children never accidentally write
    * anything to valid file descriptors of
    * VACP like driver or vacp.log etc,. */
    const char DEV_NULL[] = "/dev/null";
    int fdin, fdout, fderr;

    fdin = fdout = fderr = -1;
    fdin = open(DEV_NULL, O_RDONLY);
    fdout = open(DEV_NULL, O_WRONLY);
    fderr = open(DEV_NULL, O_WRONLY);

    boost::shared_ptr<void> fdGuardIn(static_cast<void*>(0), boost::bind(fdSafeClose, fdin));
    boost::shared_ptr<void> fdGuardOut(static_cast<void*>(0), boost::bind(fdSafeClose, fdout));
    boost::shared_ptr<void> fdGuardErr(static_cast<void*>(0), boost::bind(fdSafeClose, fderr));

    set_resource_limits();
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    MaskRequiredSignals();

    //Disable logging into the  system messages file (/var/log/messages)

    string agentInstallPath;
    std::string confPath, vacpLogFile, confFilePath;
    try {
        LocalConfigurator localconfigurator;	

        SetLogLevel(SV_LOG_DISABLE);
        SetLogRemoteLogLevel(SV_LOG_DISABLE);
        SetDaemonMode();

        agentInstallPath = localconfigurator.getInstallPath();
        g_strLogFilePath = localconfigurator.getLogPathname();
        confPath = agentInstallPath + EtcPath;
        vacpLogFile += g_strLogFilePath;
        vacpLogFile += AppPolicyLogsPath;
        vacpLogFile += VACP_LOG_FILE_NAME;
    }
    catch( ContextualException &exception )
    {
        std::stringstream ss;
        ss << "vacp failed with exception: " << exception.what();
        DebugPrintf("\n%s \n", ss.str().c_str());
        // TBD: AddToVacpFailMsgs(VACP_E_VACP_MODULE_FAIL, errmsg, VACP_READ_CONFIG_FAILED);
        ExitVacp(VACP_READ_CONFIG_FAILED);
    }
    
    inm_init_file_log(vacpLogFile);
    try {
        securitylib::setPermissions(vacpLogFile);
    }
    catch(ErrorException &ec)
    {
        DebugPrintf("Exception: %s Failed to set the permissions for file %s\n", ec.what(), vacpLogFile.c_str());
    }
    catch(...)
    {
        DebugPrintf("Generic exception: Failed to set the permissions for file %s\n", vacpLogFile.c_str());
    }

    Trace::Init("", LogLevelAlways, boost::bind(&LogCallback, _1, _2));

    confFilePath = confPath + "/" + VACP_CONF_FILE_NAME;
    g_strScheduleFilePath = confPath + "/" + ConsistencyScheduleFile;

    VacpConfigPtr pVacpConf = VacpConfig::getInstance();

    if (pVacpConf.get() != NULL)
    {
        pVacpConf->Init(confFilePath);

        std::map<std::string, std::string> &params = pVacpConf->getParams();
        std::map<std::string, std::string>::iterator iterp = params.begin();
        while (iterp != params.end())
        {
            XDebugPrintf("Param %s Value %s \n", iterp->first.c_str(), (*iterp).second.c_str());
            iterp++;
        }

        if (!gbVToAMode)
        {
            if (!pVacpConf->getParamValue(VACP_CONF_PARAM_MASTER_NODE_FINGERPRINT, g_strMasterNodeFingerprint))
            {
                DebugPrintf("Master node fingerprint is not found. Consistency will fail in case of multi-VM parallel run.\n");
            }

            if (!pVacpConf->getParamValue(VACP_CONF_PARAM_RCM_SETTINGS_PATH, g_strRcmSettingsPath))
            {
                DebugPrintf("RCM settings path is not found. Consistency will fail in case of multi-VM parallel run.\n");
            }

            if (!pVacpConf->getParamValue(VACP_CONF_PARAM_PROXY_SETTINGS_PATH, g_strProxySettingsPath))
            {
                DebugPrintf("Proxy settings path is not found.\n");
            }

            DebugPrintf("%s %s %s\n", g_strMasterNodeFingerprint.c_str(), g_strRcmSettingsPath.c_str(), g_strProxySettingsPath.c_str());
        }
    
    }
    else
    {
        DebugPrintf("Failed reading params from %s. Using defaults.\n", confFilePath.c_str());
    }


    bool rv = true;
    set<string> inputDevices;


    bool terminate = false;
    string volumes;

    
    std::stringstream ssCommandLine;
    for(int i=0; i<argc; i++)
    {
        ssCommandLine << argv[i] << " ";
    }
    
    g_strCommandline = ssCommandLine.str();
    
    inm_printf("Command Line: %s\n", ssCommandLine.str().c_str());

    // check usage
    if(argc == 1)
    {
        usage(argv[0]);
        return VACP_INVALID_ARGUMENT;
    }

    if(argc > 1 && argv[1] == "-h")
    {
        usage(argv[0]);
        return 0;
    }

    int exitCode = VACP_INVALID_ARGUMENT;

    for (int i = 1; i < argc; i++)
    {
        if(argv[i][0] == '-') 
        {
#if defined(sun) | defined (linux) |defined (SV_AIX)
            if(strcasecmp((argv[i]+1),OPTION_VOLUME) == 0)
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                if(volumes.size() == 0)
                {
                    volumes = argv[i];
                }
                else
                {
                    volumes += ",";
                    volumes += argv[i];
                }
                // No validation in case of remote tag generation
                // ParseServerDeviceList(inputVolumes, argv[i], SEPARATOR_COMMA);		
            }
            else
#endif	
            if(strcasecmp((argv[i]+1),OPTION_PRESCRIPT) == 0)
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                        terminate = true;
                        break;
                }
                gstrPreScript = argv[i];
                gbPreScript = true;
            }
            else if(strcasecmp((argv[i]+1),OPTION_POSTSCRIPT) == 0)
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                gstrPostScript = argv[i];
                gbPostScript = true;
            }
            else if(strcasecmp((argv[i]+1),OPTION_TAGNAME) == 0)
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                if (ParseTagList(gInputTags, argv[i], SEPARATOR_COMMA) == false)
                {
                    terminate = true;
                    break;
                }
            }
            else if(strcasecmp((argv[i]+1),OPTION_SERVERDEVICE) == 0)
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                ParseServerDeviceList(inputDevices, argv[i], SEPARATOR_COMMA);
            }
            else if(strcasecmp((argv[i]+1),OPTION_REMOTE) == 0)
            {			
                gbRemoteSend = true;
            }			
            else if(strcasecmp((argv[i]+1),OPTION_SERVER_IP) == 0)
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }			
                if(strlen(argv[i]) > 15)
                {	
                    inm_printf("Invalid Server IP passed");
                    inm_printf("Error in input");
                }
                gstrVacpServer = argv[i];				
            }
            else if(strcasecmp((argv[i]+1),OPTION_SERVER_PORT) == 0)
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                gVacpServerPort = atoi(argv[i]);
            }
            else if(strcasecmp((argv[i]+1),OPTION_SYNC_TAG) == 0)
            {
                gbissueSyncTags = true;
            }
            else if(strcasecmp((argv[i]+1), OPTION_TAG_TIMEOUT) == 0)
            {
                if(((i+1) < argc) && (argv[i+1][0] != '-'))
                {
                    gwait_time = atoi(argv[i+1]);
                    i++;
                }
            }
            else if(strcasecmp((argv[i]+1),OPTION_EXCLUDEAPP) == 0)
            {
                gbFSTag = false;
            }
            else if(strcasecmp((argv[i]+1),OPTION_SYSTEMLEVEL) == 0)
            {
                if(volumes.size() > 0)
                {
                    volumes += ",all";
                }
                else
                {
                    volumes += "all";
                }
                gbSystemLevel = true;
            }
#if defined (linux)
            else if(strcasecmp((argv[i]+1),OPTION_CRASH_CONSISTENCY) == 0)
            {
                gbCrashTag = true;
                gbFSTag = false;
            }
#endif
            else if((stricmp((argv[i]+1),OPTION_DISTRIBUTED_VACP) == 0))
            {
              g_bDistributedVacp = true;
            }
            else if((stricmp((argv[i]+1),OPTION_MASTER_NODE) == 0))
            {
              i++;
              if((i >= argc) || (argv[i][0] == '-'))
              {
                terminate = true;
                break;
              }
              g_bMasterNode = true;
              g_strMasterHostName = argv[i];
              
              if (g_strMasterHostName.empty())
              {
                terminate = true;
                break;
              }

            }
            else if((stricmp((argv[i]+1),OPTION_COORDINATOR_NODES) == 0))
            {
              i++;
              if((i >= argc)|| (argv[i][0] == '-'))
              {
                terminate = true;
                break;
              }
              g_bCoordNodes = true;

              ParseServerIpList(g_vCoordinatorNodeLevels, argv[i], SEPARATOR_COLON);

              for(int cnt = 0; cnt < g_vCoordinatorNodeLevels.size(); cnt++)
              {
                std::string strCoordIps = g_vCoordinatorNodeLevels.at(cnt);
                
                size_t CoordIpSize;
                INM_SAFE_ARITHMETIC(CoordIpSize = InmSafeInt<unsigned long>::Type(strCoordIps.length()) + 1, INMAGE_EX(strCoordIps.length()))
                char * cstrCoordIps = NULL;
                cstrCoordIps = new (std::nothrow) char[CoordIpSize];
                if (cstrCoordIps == NULL)
                {
                    terminate = true;
                    break;
                }
                inm_strcpy_s(cstrCoordIps, strCoordIps.length() + 1, strCoordIps.c_str());
                ParseServerIpList(g_vCoordinatorNodes, cstrCoordIps, SEPARATOR_COMMA);
                delete[] cstrCoordIps;
              }
              if (terminate)
              {
                  break;
              }
              std::vector<string>::iterator iterCoordNodes = g_vCoordinatorNodes.begin();
              for (/*empty*/; iterCoordNodes != g_vCoordinatorNodes.end(); iterCoordNodes++)
              {
                  boost::algorithm::to_lower(*iterCoordNodes);
              }
            }
            else if((stricmp((argv[i]+1),OPTION_DISTRIBUTED_PORT) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                g_DistributedPort = (unsigned short)atoi(argv[i]);
            }
            else if(strcasecmp((argv[i]+1),OPTION_HELP) == 0)
            {
                if (argc != 2)
                    inm_printf("Error in input");
                else
                {
                    terminate = true;
                    break;
                }
            }
            else if((stricmp((argv[i]+1),OPTION_VERBOSE) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                gsVerbose = (short)atoi(argv[i]);
            }
            else if((stricmp((argv[i]+1),OPTION_BASELINE) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                        terminate = true;
                        break;
                }
                gStrBaselineId = argv[i];
                gbBaselineTag = true;
            }
            else if((stricmp((argv[i]+1),OPTION_TAGGUID) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                gStrTagGuid = argv[i];
                if (!IsValidGuid(gStrTagGuid))
                {
                    inm_printf("Error in input - tagguid not in valid format");
                    terminate = true;
                    break;
                }
            }
            else if((stricmp((argv[i]+1),OPTION_PARALLEL_RUN) == 0))
            {
                gbParallelRun = true;
            }
            else if ((stricmp((argv[i] + 1), OPTION_VTOA_MODE) == 0))
            {
                gbVToAMode = true;
            }
            else if ((stricmp((argv[i] + 1), OPTION_IR_MODE) == 0))
            {
                gbIRMode = true;
            }
            else if (stricmp((argv[i] + 1), OPTION_MASTER_NODE_FINGERPRINT) == 0)
            {
                i++;
                g_strMasterNodeFingerprint = std::string(argv[i]);
            }
            else if (stricmp((argv[i] + 1), OPTION_RCM_SETTINGS_PATH) == 0)
            {
                i++;
                g_strRcmSettingsPath = std::string(argv[i]);
            }
            else if (stricmp((argv[i] + 1), OPTION_PROXY_SETTINGS_PATH) == 0)
            {
                i++;
                g_strProxySettingsPath = std::string(argv[i]);
            }
            else
            {
                inm_printf("Invalid option: %s\n", argv[i]+1);
                inm_printf("Error in input\n");
                terminate = true;
                break;
            }
        }
        else
        {
            terminate = true;
            break;
        }
    }

    if (!terminate)
    do {

        if (gbissueSyncTags)
        {
            inm_printf("Error: sync tags not supported.\n");
            terminate = true;
            break;
        }

        if (gbRemoteSend && gbissueSyncTags)
        {
            terminate = true;
            inm_printf("Error: Sync tags are not supported in with remote tags.\n");
            break;
        }

        if (gbParallelRun && !gStrTagGuid.empty())
        {
            terminate = true;
            inm_printf("Error: tagguid is not valid for parallel run.\n");
            break;
        }

        if (g_bDistributedVacp && gbRemoteSend)
        {
            terminate = true;
            inm_printf("Error: Distributed tags are not supported in with remote tags.\n");
            break;
        }

        if (g_bDistributedVacp && gbissueSyncTags)
        {
            terminate = true;
            inm_printf("Error: Distributed tags are not supported in with sync tags.\n");
            break;
        }
                
        if (!gbRemoteSend )
            if (!GetDriverVersion(&gdriverVersion))
            {
                terminate = true;
                exitCode = VACP_DRIVER_OPEN_FAILED;
                inm_printf("Error: Failed to get involflt driver version\n");
                break;
            }
    
        if (gbCrashTag && !DriverSupportsIoBarrier())
        {
            terminate = true;
            exitCode = VACP_INVALID_DRIVER_VERSION;
            inm_printf("Error: crash consistency is not supported with this driver version.\n");
            break;
        }

        if (g_bDistributedVacp && !DriverSupportsTagV2() )
        {
            terminate = true;
            exitCode = VACP_INVALID_DRIVER_VERSION;
            inm_printf("Error: Distributed tags are not supported with this driver version.\n");
            break;
        }

        if (g_bDistributedVacp) 
            if (!(g_bMasterNode || g_bCoordNodes))
                g_bDistributedVacp=false; // or terminate=true;

        if (GetLocalIPs() != 0)
        {
            if (g_bDistributedVacp)
            {
                terminate = true;
                break;
            }
        }

        if (g_bDistributedVacp)
        {
            std::set<string> sCoordNodes(g_vCoordinatorNodes.begin(), g_vCoordinatorNodes.end());
            if (g_vCoordinatorNodes.size() != sCoordNodes.size())
            {
                inm_printf("Error: duplicate entries found in coordinator nodes.\n");
                terminate = true;
                break;
            }

            DebugPrintf("Validating master node: %s\n", g_strMasterHostName.c_str());
            struct sockaddr_in sockaddr;
            if (inet_pton(AF_INET, g_strMasterHostName.c_str(), &sockaddr) != 1)
            {
                if (boost::iequals(g_strMasterHostName, g_strLocalHostName))
                {
                    boost::to_lower(g_strMasterHostName);
                    g_bThisIsMasterNode = true;
                }
            } 
            else
            {
                std::string strMasterNodeIP = g_strMasterHostName;
                std::vector<std::string>::iterator iterLocalIps = g_vLocalHostIps.begin();
                while (iterLocalIps != g_vLocalHostIps.end())
                {
                    DebugPrintf("Comparing master node: %s:%s\n", strMasterNodeIP.c_str(), (*iterLocalIps).c_str());
                    if (boost::iequals(strMasterNodeIP, *iterLocalIps))
                    {
                        g_bThisIsMasterNode = true;
                        break;
                    }
                    iterLocalIps++;
                }
            }

            DebugPrintf("Validating coordinator nodes\n");
            bool localNodeIsCoordNode = false;

            for(int cnt = 0; cnt < g_vCoordinatorNodes.size(); cnt++)
            {
                std::string strCoordIp = g_vCoordinatorNodes.at(cnt);

                if (inet_pton(AF_INET, strCoordIp.c_str(), &sockaddr) != 1)
                {
                    if (boost::iequals(g_strLocalHostName, strCoordIp))
                    {
                        localNodeIsCoordNode = true;
                        g_strLocalIpAddress = strCoordIp; 
                        boost::to_lower(g_strLocalIpAddress);
                        break;
                    }
                } 
                else
                {
                    std::vector<std::string>::iterator iterLocalIps = g_vLocalHostIps.begin();
                    while(iterLocalIps != g_vLocalHostIps.end())
                    {
                        if(boost::iequals(strCoordIp,*iterLocalIps))
                        {
                            localNodeIsCoordNode = true;
                            g_strLocalIpAddress = strCoordIp; 
                            boost::to_lower(g_strLocalIpAddress);
                            break;
                        }
                        iterLocalIps++;
                    }
                }
            }

            if (localNodeIsCoordNode && g_bThisIsMasterNode)
            {
                DebugPrintf("A node cannot be in both -mn and -cn option.\n");
                terminate = true;
                break;
            }

            if (!localNodeIsCoordNode && !g_bThisIsMasterNode)
            {
                DebugPrintf("This node is neither master node nor coordinator node. A local consistency tag will be issued.\n");
                g_bDistributedVacp = false; 
                break;
            }

            if (!gbVToAMode && !g_strMasterNodeFingerprint.empty() && g_strRcmSettingsPath.empty())
            {
                DebugPrintf("Options -rcmSetPath should be used with -mnf.\n");
                terminate = true;
                break;
            }
        }

        if (gbParallelRun && pVacpConf->getParams().empty())
        {
            DebugPrintf("\nConfig parameters are required in parallel mode.\n");
            exitCode = VACP_CONF_PARAMS_NOT_SUPPLIED;
            terminate = true;
        }

    } while(false);

    if(terminate)
    {
        return(exitCode);
    }

    exitCode = 0;


    size_t lenStrVols;
    INM_SAFE_ARITHMETIC(lenStrVols = InmSafeInt<size_t>::Type(volumes.size())+1, INMAGE_EX(volumes.size()))
    g_cstrVolumes = new char [lenStrVols];
    inm_strcpy_s(g_cstrVolumes, lenStrVols, volumes.c_str());

    if (!DiscoverDisksAndVolumes(gbFSTag, gVolumesToBeTagged))
    {
        inm_printf("\nDiscoverDisksAndVolumes failed\n");
        return VACP_DISK_DISCOVERY_FAILED;
    }

    if(!gbRemoteSend && gVolumesToBeTagged.size() < 1)
    {
        inm_printf("Please specify atleast one volume/device \n");
        return VACP_INVALID_ARGUMENT;
    }            

    GetLastConsistencyScheduleTime();
    InitQuitEvent();
    g_Quit->Signal(false);
    
    if (g_bDistributedVacp && !g_strMasterNodeFingerprint.empty())
    {
        AADAuthProvider::Initialize();
    }

    boost::thread_group thread_group;
    
    if (gbParallelRun)
    {
        if (gbCrashTag)
        {
            boost::thread *ccthread = new boost::thread(boost::bind(RunCrashConsistency));
            thread_group.add_thread(ccthread);
        }

        boost::thread *acthread = new boost::thread(boost::bind(RunAppConsistency));
        thread_group.add_thread(acthread);


    }
    else
    {
        if (gbBaselineTag)
        {
            RunBaselineConsistency();
        }
        else if (gbCrashTag)
        {
            boost::thread *ccthread = new boost::thread(boost::bind(RunCrashConsistency));
            thread_group.add_thread(ccthread);
        }
        else
        {
            boost::thread *acthread = new boost::thread(boost::bind(RunAppConsistency));
            thread_group.add_thread(acthread);
        }
    }
    
    while (gbParallelRun && !(*g_Quit)(10));

    thread_group.join_all();

    if (g_bDistributedVacp && !g_strMasterNodeFingerprint.empty())
    {
        AADAuthProvider::Uninitialize();
    }

    DebugPrintf("%s EXITING\n", __FUNCTION__);

    if (gbParallelRun)
    {
        ExitVacp(0);
    }
    else
    {
        ExitVacp(gExitCode);
    }
}

string uuid_gen ()
{
    uuid_t id;
    char key[INM_UUID_STR_LEN + 1] = "";
    uuid_generate(id);
    uuid_unparse(id, key);
    return key;
}

bool CrashConsistency::Quiesce()
{
    if (m_bDistributedVacp)
    {
        long ioBarrierTimeout = VACP_TAG_IOBARRIER_TIMEOUT;

        VacpConfigPtr pVacpConf = VacpConfig::getInstance();
        if (pVacpConf.get() != NULL)
        {
            if (pVacpConf->getParamValue(VACP_CONF_PARAM_IO_BARRIER_TIMEOUT, ioBarrierTimeout))
            {
                if ( ioBarrierTimeout < VACP_TAG_IOBARRIER_TIMEOUT )
                {
                    inm_printf("\nIO Barrier Timeout %ld is less than allowed minimum. Setting to default value %d.\n", 
                                ioBarrierTimeout, VACP_TAG_IOBARRIER_TIMEOUT);

                    ioBarrierTimeout = VACP_TAG_IOBARRIER_TIMEOUT;
                }
                else if ( ioBarrierTimeout > VACP_IOBARRIER_TIMEOUT)
                {
                    inm_printf("\nIO Barrier Timeout %ld is more than allowed maximum. Setting to default max value %d.\n", 
                                ioBarrierTimeout, VACP_IOBARRIER_TIMEOUT);

                    ioBarrierTimeout = VACP_IOBARRIER_TIMEOUT;
                }
                else
                    inm_printf("\nIO Barrier Timeout is set to custom value of %ld ms.\n", 
                                ioBarrierTimeout);
            }
        }

        m_perfCounter.StartCounter("IoBarrier");

        if (!CreateIoBarrier(ioBarrierTimeout))
        {
            inm_printf("CreateIoBarrier failed, error = 0x%x\n", m_exitCode);
            // For master: wait for clients status and exit, for client send message in communicator and then exit
            if (!g_bThisIsMasterNode)
            {
                Conclude(m_exitCode);
            }
            return false;
        }

        m_bDistributedIoBarrierCreated = true;
    }
    return true;
}

bool AppConsistency::Quiesce()
{
#if defined (linux) | defined (SV_AIX)
    if (gbSystemLevel)
    {
        if (!m_appConPtr->Freeze())
        {
            m_exitCode = VACP_APP_FREEZE_FAILED;
            m_appConPtr->Unfreeze();
            return false;
        }
    }
#endif

    m_perfCounter.StartCounter("FsFreeze");
    if (!FreezeDevices(gInputVolumes))
    {
        m_exitCode = VACP_FS_FREEZE_FAILED;
        ThawDevices(gInputVolumes);
#if defined (linux) | defined (SV_AIX)
        if (gbSystemLevel)
        {
            m_appConPtr->Unfreeze();
        }
#endif
        return false;
    }
    return true;
}
    
void CrashConsistency::CheckDistributedQuiesceState()
{
    if (m_bDistributedIoBarrierCreated && !m_bDistributedVacp)
    {
        // moved from distributed crash to local crash mode
        // need to release the barrier and let the local barrier+tag ioctl to succeed
        RemoveIoBarrier();
        m_perfCounter.EndCounter("IoBarrier");

        // change the context guid to indicate the switch
        m_strContextUuid = uuid_gen();

        m_bDistributedIoBarrierCreated = false;
    }
}

bool CrashConsistency::Resume()
{
    bool rv = true;
    // handle a case where the distributed tag is inserted but later mode is switched
    // to local tag. here the barrier is removed but the tag should not be commited
    if (m_bDistributedVacp || (m_bDistributedIoBarrierCreated && !m_bDistributedVacp))
    {
        rv = RemoveIoBarrier();

        m_perfCounter.EndCounter("IoBarrier");

        if (!rv || (m_bDistributedIoBarrierCreated && !m_bDistributedVacp))
        {
            return false;
        }
    }
    return true;
}

bool AppConsistency::Resume()
{
    bool rv = true;
    if (!ThawDevices(gInputVolumes))
    {
        m_exitCode = VACP_FS_THAW_FAILED;
        rv = false;
    }
    m_perfCounter.EndCounter("FsFreeze");

#if defined (linux) | defined (SV_AIX)
    if (gbSystemLevel)
        if (!m_appConPtr->Unfreeze())
        {
            m_exitCode = VACP_APP_THAW_FAILED;
            rv = false;
        }
#endif
    return rv;
}

void Consistency::UpdateStartTimeTelInfo()
{
    std::string localSystemTime = boost::lexical_cast<std::string>(GetTimeInSecSinceEpoch1970());
    m_tagTelInfo.Add(VacpSysTimeKey, localSystemTime);
    m_tagTelInfo.Add(VacpStartTimeKey, localSystemTime);
    DebugPrintf("SysTimeKey %s .\n", localSystemTime.c_str());
    DebugPrintf("VacpStartTime %s .\n", localSystemTime.c_str());
}

void Consistency::IRModeStamp()
{
    std::string errmsg = "One or more disk in IR/resync - skipping consistency.";
    DebugPrintf("%s, hr = 0x%x\n", errmsg.c_str(), VACP_ONE_OR_MORE_DISK_IN_IR);
    Conclude(VACP_ONE_OR_MORE_DISK_IN_IR);
}

void Consistency::Process()
{
    UpdateStartTimeTelInfo();

    m_tagTelInfo.Add(VacpCommand, g_strCommandline);
    DebugPrintf("\nCommand Line: %s\n", g_strCommandline.c_str());

    //if Pre-Script option is exercised by Vacp, then first PreScript should be executed
    if (gbPreScript)
    {
        ExecuteScript(gstrPreScript);
    }

    if(IS_CRASH_TAG(m_TagType))
    {
        DebugPrintf("%s %s .\n", TagType.c_str(), CRASH_TAG);
        m_tagTelInfo.Add(TagType, CRASH_TAG);
    }
    else
    {
        DebugPrintf("%s %s .\n", TagType.c_str(), APP_TAG);
        m_tagTelInfo.Add(TagType, APP_TAG);
    }
    
    bool rv = true;
    m_exitCode = 0;
    m_status = CONSISTENCY_STATUS_INITIALIZED;

    if (gbIRMode)
    {
        return IRModeStamp();
    }
    
    std::set<std::string> volumesToBeTagged;
    if (gbParallelRun)
    {
        if (!DiscoverDisksAndVolumes(!IS_CRASH_TAG(m_TagType), volumesToBeTagged))
        {
            return Conclude(VACP_DISK_DISCOVERY_FAILED);
        }
    }
    else
    {
        volumesToBeTagged = gVolumesToBeTagged;
    }


    // Generate Hydration Tag if it is an Azure VM in V2A
    if (gbVToAMode)
    {
        if (IsAgentRunningOnAzureVm())
        {
            DebugPrintf("\nhydration is not required on Azure VM\n");
            m_hydrationTag = "_no_hydration:_azure;_desc:_linux_azure_vm";
        }
    }

    StartCommunicator();

    inm_log_to_buffer_start();

    std::string uniqueIdStr = getUniqueSequence();
    std::string uuid;
    set<string> distributedInputTags;
    
    if(g_bDistributedVacp)
    {
        if (gStrTagGuid.empty())
            uuid = uuid_gen();
        else
            uuid = gStrTagGuid;

        DistributedTagGuidPhase(uuid, gInputTags, uniqueIdStr, distributedInputTags);

        PreparePhase();
    }

    if (gStrTagGuid.empty())
        m_strContextUuid = uuid_gen();
     else
        m_strContextUuid = gStrTagGuid;

    m_status |= CONSISTENCY_STATUS_PREPARE_SUCCESS;

    PrepareAckPhase();

    QuiescePhase();

     do 
    {
        if ((m_status & CONSISTENCY_STATUS_PREPARE_SUCCESS) != CONSISTENCY_STATUS_PREPARE_SUCCESS)
            break;

        m_perfCounter.StartCounter("all driver phases");

        if(!Quiesce())
            break;

        m_status |= CONSISTENCY_STATUS_QUIESCE_SUCCESS;

    } while(false);
    

    QuiesceAckPhase();

    TagPhase();

    //  Go through each tag and generate a stream based tag
    //  If FStag is required, generate a stream based fstag as well
    char * tagIoctlBuffer = NULL;

    do
    {
        if ((m_status & CONSISTENCY_STATUS_QUIESCE_SUCCESS) != CONSISTENCY_STATUS_QUIESCE_SUCCESS)
            break;

        CheckDistributedQuiesceState();

        if (m_bDistributedVacp)
            gInputTags.insert(distributedInputTags.begin(), distributedInputTags.end());

        set<TagInfo_t> finalTags;
        if (!GenerateTagNames(finalTags, gInputTags, /*inputAppNames, */ !IS_CRASH_TAG(m_TagType), uuid, uniqueIdStr))
        {
            break;
        }

        if(finalTags.size() < 1)
        {
            inm_printf("Please specify atleast one tag\n");
            break;
        }

        unsigned int tagBufferSize;
        unsigned int status_offset;

        tagBufferSize = CreateTagIoctlBuffer(&tagIoctlBuffer, volumesToBeTagged, finalTags);

        if (tagBufferSize == 0)
        {
            inm_printf("Error: zero tag buffer size.\n"); 
            break;
        }

        if( gbRemoteSend )
        {
            if (!SendTagsToRemoteServer(tagIoctlBuffer, tagBufferSize, gstrVacpServer, gVacpServerPort))
            {
                delete [] tagIoctlBuffer;
                break;
            }
        } else {

            m_perfCounter.StartCounter("DrainBarrier");

            if (!SendTagsToLocalDriver(tagIoctlBuffer))
            {
                rv = false;
            }

            if (!CheckTagsStatus(tagIoctlBuffer, tagBufferSize, gwait_time, gInputVolumes))
                rv = false;

            delete [] tagIoctlBuffer;

            if (!rv)
            {
                if (m_bDistributedVacp && !g_bThisIsMasterNode)
                {
                    return Conclude(m_exitCode);
                }
                break;
            }
        }
        
        if (m_bDistributedVacp || !IS_CRASH_TAG(m_TagType))
            m_status |= CONSISTENCY_STATUS_INSERT_TAG_SUCCESS;
        else
            m_status |= CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS;

    } while (false);


    TagAckPhase();

    ResumePhase();

    do 
    {
        if ((m_status & CONSISTENCY_STATUS_QUIESCE_SUCCESS) != CONSISTENCY_STATUS_QUIESCE_SUCCESS)
            break;

        if (!Resume())
            break;

        m_status |= CONSISTENCY_STATUS_RESUME_SUCCESS;

    } while (false);

    
    ResumeAckPhase();

    do
    {
        if ((m_status & CONSISTENCY_STATUS_INSERT_TAG_SUCCESS) != CONSISTENCY_STATUS_INSERT_TAG_SUCCESS)
            break;

        rv = CommitRevokeTags();

        m_perfCounter.EndCounter("DrainBarrier");
        m_perfCounter.EndCounter("all driver phases");

        if (!rv)
            break;

        m_status |= CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS;

    } while (false);
    
    if ((m_status & CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS) != CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS)
        rv = false;


    if (m_bDistributedVacp)
    {
        if ((m_status & CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS) == CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS)
        {
            m_tagTelInfo.Add(MultivmTagKey, std::string());
            inm_printf("Exiting gracefully ...(Issued a DISTRIBUTED TAG on this Node.)\n");

            return Conclude(0);
        }
        else
        {
            if (g_bThisIsMasterNode)
            {
                inm_printf("\nMaster Node has failed to issue the tag but it has facilitated other Client Nodes to issue a Distributed Tag.\n");
            }

            int status = ~m_status & CONSISTENCY_STATUS_SUCCESS_BITS;

            inm_printf("Local tag status 0x%x.\n", status);

            if (m_status)
            {
                return Conclude(m_status);
            }
            else
            {
                return Conclude(m_exitCode);
            }
        }
    }
    else
    {
        if ((m_status & CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS) == CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS)
        {
            m_tagTelInfo.Add(LocalTagKey, std::string());
            inm_printf("Exiting gracefully ...(Issued a LOCAL TAG)\n");

            return Conclude(0);
        }
        else
        {
            int status = ~m_status & CONSISTENCY_STATUS_SUCCESS_BITS;
            inm_printf("Local tag status 0x%x.\n", status);
            return Conclude(m_exitCode);
        }
    }
}

void BaselineConsistency::Process()
{
    UpdateStartTimeTelInfo();

    DebugPrintf("\nCommand Line: %s\n", g_strCommandline.c_str());

    DebugPrintf("%s %s \n", TagType.c_str(), BASELINE_TAG);
    m_tagTelInfo.Add(TagType, BASELINE_TAG);
    
    bool rv = true;
    m_status = CONSISTENCY_STATUS_INITIALIZED;

    std::string uniqueIdStr = getUniqueSequence();
    std::string uuid;

    m_strContextUuid = uuid_gen();

    m_perfCounter.StartCounter("all driver phases");

    //  Go through each tag and generate a stream based tag
    //  If FStag is required, generate a stream based fstag as well
    char * tagIoctlBuffer = NULL;

    do
    {
        set<TagInfo_t> finalTags;
        if (!GenerateTagNames(finalTags, gInputTags, /*inputAppNames, */ !IS_CRASH_TAG(m_TagType), uuid, uniqueIdStr))
        {
            break;
        }

        if(finalTags.size() < 1)
        {
            inm_printf("Please specify atleast one tag\n");
            break;
        }

        unsigned int tagBufferSize;
        unsigned int status_offset;

        tagBufferSize = CreateTagIoctlBuffer(&tagIoctlBuffer, gVolumesToBeTagged, finalTags);

        if (tagBufferSize == 0)
        {
            inm_printf("Error: zero tag buffer size.\n");
            break;
        }

        m_perfCounter.StartCounter("DrainBarrier");
        if (!SendTagsToLocalDriver(tagIoctlBuffer))
        {
            rv = false;
        }

        if (!CheckTagsStatus(tagIoctlBuffer, tagBufferSize, gwait_time, gInputVolumes))
            rv = false;

        delete [] tagIoctlBuffer;

        if (!rv)
        {
            return Conclude(m_exitCode);
        }
        
        m_status |= CONSISTENCY_STATUS_INSERT_TAG_SUCCESS;
        m_status |= CONSISTENCY_STATUS_RESUME_SUCCESS;
        
        if (!CommitRevokeTags())
        {
            return Conclude(m_exitCode);
        }

    } while (false);

    m_perfCounter.EndCounter("DrainBarrier");
    m_perfCounter.EndCounter("all driver phases");
    
    m_status |= CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS;


    if ((m_status & CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS) == CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS)
    {
        inm_printf("Exiting gracefully ...(Issued a LOCAL TAG)\n");

        return Conclude(0);
    }
    else
    {
        int status = ~m_status & CONSISTENCY_STATUS_SUCCESS_BITS;
        inm_printf("Local tag status 0x%x.\n", status);
        return Conclude(m_exitCode);
    }
}

void Consistency::FacilitateDistributedConsistency()
{
    switch (m_DistributedProtocolState)
    {
    case COMMUNICATOR_STATE_SERVER_PREPARE_CMD_COMPLETED:
        PrepareAckPhase();
    case COMMUNICATOR_STATE_SERVER_PREPARE_ACK_COMPLETED:
        QuiescePhase();
    case COMMUNICATOR_STATE_SERVER_QUIESCE_CMD_COMPLETED:
        QuiesceAckPhase();
    case COMMUNICATOR_STATE_SERVER_QUIESCE_ACK_COMPLETED:
        ResumePhase();
    case COMMUNICATOR_STATE_SERVER_RESUME_CMD_COMPLETED:
        ResumeAckPhase();
    default:
        ;
    }
    return;
}

void Consistency::Conclude(unsigned long hr)
{
    try
    {
        gExitCode = hr; // gExitCode required for v-a
        std::string localSystemTime = boost::lexical_cast<std::string>(GetTimeInSecSinceEpoch1970());
        DebugPrintf("Entered Conclude() at %s\n", localSystemTime.c_str());
        m_tagTelInfo.Add(VacpEndTimeKey, localSystemTime);
        DebugPrintf("VacpEndTime %s .\n", localSystemTime.c_str());

        if (g_bThisIsMasterNode && m_pCommunicator)
        {
            std::string excludedNodes = m_pCommunicator->ShowDistributedVacpReport();
            if (!excludedNodes.empty())
            {
                excludedNodes += SearchEndKey;
                m_tagTelInfo.Add(ExcludedNodes, excludedNodes);
                DebugPrintf("\n%s %s\n", ExcludedNodes.c_str(), excludedNodes.c_str());
            }
        }

        inm_log_to_buffer_end();

        if (m_bDistributedVacp && g_bThisIsMasterNode && !m_DistributedProtocolStatus)
        {
            // allow the clients to go through the
            // remaining multi-vm phases
            FacilitateDistributedConsistency();
        }

        m_perfCounter.PrintAllElapsedTimes();

        unsigned long hrLocal;

        std::stringstream ssFailMsgs;
        std::string VacpFailMsgs = GetFormattedFailMsg(m_VacpFailMsgsList, VACP_FAIL_MSG_DELM);
        if (VacpFailMsgs.length())
        {
            ssFailMsgs << vacpFailMsgsTok;
            ssFailMsgs << VacpFailMsgs;
            ssFailMsgs << "\n";

            // First error code is exit code
            FailMsgList::const_iterator itr = m_VacpFailMsgsList.begin();
            for (itr; itr != m_VacpFailMsgsList.end(); itr++)
            {
                if ((VACP_MODULE_INFO != itr->m_errorCode) && (hr != itr->m_errorCode))
                {
                    hr = itr->m_errorCode;
                    break;
                }
            }
        }

        m_VacpFailMsgsList.clear();
        
        std::string MultivmFailMsgs = GetFormattedFailMsg(m_MultivmFailMsgsList, MULTIVM_FAIL_MSG_DELM);
        if (MultivmFailMsgs.length())
        {
            ssFailMsgs << multivmFailMsgsTok;
            ssFailMsgs << MultivmFailMsgs;
            ssFailMsgs << "\n";
        }

        m_MultivmFailMsgsList.clear();
        
        if (ssFailMsgs.tellp())
        {
            inm_printf("%s\n", ssFailMsgs.str().c_str());
        }

        hrLocal = hr;
        std::stringstream strStatus;
        if(hrLocal)
        {
            if (m_bDistributedVacp && !g_bThisIsMasterNode)
            {
                strStatus << CoordNodeKey << g_strLocalHostName << ":";
            }
            else
            {
                strStatus << HostKey << g_strLocalHostName << ":";
            }
        
            switch (hrLocal)
            {
            case VACP_E_GENERIC_ERROR:
                strStatus << "Generic error";
                break;
            case VACP_E_INVALID_COMMAND_LINE:
                strStatus << "Invalid command";
                break;
            case VACP_E_INVOLFLT_DRIVER_NOT_INSTALLED:
                strStatus << "Driver not installed";
                break;
            case VACP_E_DRIVER_IN_NON_DATA_MODE:
                strStatus << NonDataModeError;
                break;
            case VACP_E_NO_PROTECTED_VOLUMES:
                strStatus << "No protected volumes";
                break;
            case VACP_E_NON_FIXED_DISK:
                strStatus << "Non fixed disk";
                break;
            case VACP_E_ZERO_EFFECTED_VOLUMES:
                strStatus << "Zero effected volumes";
                break;
            case VACP_E_VACP_SENT_REVOCATION_TAG:
                strStatus << "Revocation tag sent";
                break;
            case VACP_E_VACP_MODULE_FAIL:
                strStatus << "Fail";
                break;
            case VACP_PRESCRIPT_WAIT_TIME_EXCEEDED:
                strStatus << "Prescript wait time exceeded";
                break;
            case VACP_POSTSCRIPT_WAIT_TIME_EXCEEDED:
                strStatus << "Postscript wait time exceeded";
                break;
            case VACP_E_PRESCRIPT_FAILED:
                strStatus << "Prescript failed";
                break;
            case VACP_E_POSTSCRIPT_FAILED:
                strStatus << "Postscript failed";
                break;
            case VACP_FAILED_TO_SEND_TAGS_TO_DRIVER:
                strStatus << "Failed to send tags to driver";
                break;
            default:
                strStatus << (unsigned long)hrLocal;
                break;
            }
            strStatus << SearchEndKey;
        }
        
        if (gbPostScript)
        {
            ExecuteScript(gstrPostScript);
        }

        if (hrLocal && m_bDistributedVacp && !g_bThisIsMasterNode && m_pCommunicator)
        {
            int nRet = m_pCommunicator->SendMessageToMasterNode(ENUM_MSG_STATUS_INFO, strStatus.str());
            if (0 != nRet)
            {
                inm_printf("\nFailed to send status to master node: %s. Error = %d\n", strStatus.str().c_str(), nRet);
            }
            else
            {
                inm_printf("\nSent status to master node: %s\n", strStatus.str().c_str());
            }
        }

        if (m_bDistributedVacp)
        {
            ExitDistributedConsistencyMode();
        }

        m_tagTelInfo.Add(VacpExitStatus, boost::lexical_cast<std::string>(hr));

        if (gbParallelRun || IS_BASELINE_TAG(m_TagType))
        {
            m_tagTelInfo.Add(InternallyRescheduled, boost::lexical_cast<std::string>(m_bInternallyRescheduled));

            std::string prefix;
            if (IS_CRASH_TAG(m_TagType))
            {
                prefix = "crash";
            }
            else if (IS_BASELINE_TAG(m_TagType))
            {
                prefix = "baseline_" + gStrBaselineId;
            }
            else
            {
                prefix = "app";
            }

            std::string strPreVacpLogFile;
            std::string strVacpLogFile;
            
            std::string LogFilePath = g_strLogFilePath;
            LogFilePath += AppPolicyLogsPath;
            NextLogFileName(LogFilePath, strPreVacpLogFile, strVacpLogFile, prefix);
            try
            {
                CreatePaths::createPathsAsNeeded(strPreVacpLogFile);
                std::ofstream logFileStream(strPreVacpLogFile.c_str(), std::ios::out | std::ios::app);
                logFileStream << m_perfCounter.GetTagTelemetryInfo().Get();
                logFileStream << m_tagTelInfo.Get();
                logFileStream << strStatus.str();
                logFileStream << "\n";
                logFileStream << ssFailMsgs.str();
                logFileStream.close();
                
                std::string err;
                if (!RenameLogFile(strPreVacpLogFile, strVacpLogFile, err))
                { 
                    DebugPrintf("%s: Rename failed for %s, error: %s\n", FUNCTION_NAME, strPreVacpLogFile.c_str(), err.c_str());
                }

                securitylib::setPermissions(strVacpLogFile);
            }
            catch (std::exception ex)
            {
                inm_printf("%s: failed to create file %s with exception %s\n", FUNCTION_NAME, strPreVacpLogFile.c_str(), ex.what());
            }
            catch (...)
            {
                inm_printf("%s: failed to create file %s with an unknown exception.\n", FUNCTION_NAME, strPreVacpLogFile.c_str());
            }
        }

        inm_printf("Exiting with status %lu %s\n", hr, strStatus.str().c_str());
            
        m_perfCounter.Clear();
        m_tagTelInfo.Clear();

        DebugPrintf("%s : last vacp error 0x%x hr 0x%x gExitCode 0x%x \n",
                    __FUNCTION__,
                    m_lastRunErrorCode,
                    hr,
                    gExitCode);

        m_lastRunErrorCode = hr;
    }
    catch(const std::exception &e)
    {
        DebugPrintf("%s: caught exception %s\n", __FUNCTION__, e.what());
    }
    catch (...)
    {
        inm_printf("%s: caught an unknown exception.\n", FUNCTION_NAME);
    }

    return;
}

CrashConsistency& CrashConsistency::CreateInstance()
{
    if (NULL == m_theCrashC)
    {
        boost::unique_lock<boost::mutex> lock(m_theCrashCLock);
        if (NULL == m_theCrashC)
        {
            m_theCrashC = new CrashConsistency(g_bDistributedVacp);
        }
    }
    return *m_theCrashC;
}

CrashConsistency& CrashConsistency::Get()
{
    if (NULL == m_theCrashC)
    {
        throw "CrashConsistency instance not created."; // TBD: add telementry
    }
    return *m_theCrashC;
}

AppConsistency& AppConsistency::CreateInstance()
{
    if (NULL == m_theAppC)
    {
        boost::unique_lock<boost::mutex> lock(m_theAppCLock);
        if (NULL == m_theAppC)
        {
            m_theAppC = new AppConsistency(g_bDistributedVacp);
        }
    }
    return *m_theAppC;
}

AppConsistency& AppConsistency::Get()
{
    if (NULL == m_theAppC)
    {
        throw "AppConsistency instance not created."; // TBD: add telementry
    }
    return *m_theAppC;
}

BaselineConsistency& BaselineConsistency::CreateInstance()
{
    if (!m_theBaseline)
    {
        boost::unique_lock<boost::mutex> lock(m_theBaselineLock);
        if (!m_theBaseline)
        {
            m_theBaseline = new BaselineConsistency();
        }
    }
    return *m_theBaseline;
}

BaselineConsistency& BaselineConsistency::Get()
{
    if (NULL == m_theBaseline)
    {
        throw "Baseline instance not created.";
    }
    return *m_theBaseline;
}

