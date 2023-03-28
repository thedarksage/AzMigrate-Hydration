// vacp.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "..\hydrationconfigurator\HydrationConfigurator.h"

using namespace std;
#define SECURITY_WIN32 

#include <Security.h> 
#include <direct.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <conio.h>
#include <atlbase.h>
#include "util.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/chrono.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>


#include "InMageVssProvider_i.c"
#include "VssWriter.h"
#include "StreamEngine/StreamRecords.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "VacpProviderCommon.h"

#include "VssRequestor.h"
#include "CmdRequest.h"
#include "ArgParser.h"
#include "devicefilter.h"
#include "vacp.h"
#include "WinInet.h"
#include <stdlib.h>
#include "..\\config\\vacp_tag_info.h"
#include "inmsafecapis.h"
#include "..\\common\\win32\\terminateonheapcorruption.h"
#include "vacprpc.h"
#include "PerfCounter.h"
#include "VacpConf.h"
#include "VacpErrorCodes.h"
#include "common/Trace.h"
#include "AADAuthProvider.h"
#include "vsadmin.h"
#include <atlconv.h> // Use string conversion macros used in ATL
#include "createpaths.h"
#include "deviceutils.h"
#include "AgentHealthIssueNumbers.h"

using namespace boost::chrono;
using namespace AADAuthLib;
using namespace TagTelemetry;
using namespace TagTelemetry::NsVacpOutputKeys;

#define TAG_FLAG_NON_ATOMIC		0
#define TAG_FLAG_ATOMIC			1
#define TAG_VOLUME_INPUT_FLAGS_DEFERRED_TILL_COMMIT_SNAPSHOT    0x0010
#define VSS_SNAPSHOTSET_LIMIT 64
//#define SPAWN_WAIT_TIME 180000

#define VSS_PROV_DELAYED_STOP_WAIT_TIME     1800 // in sec, 30 min
#define VSS_PROV_IMMEDIATE_STOP_WAIT_TIME   1    // in sec

const char *TagStatusStringArray[] = 
{
    "Commited",
    "Pending",
    "Deleted",
    "Dropped",
    "Invalid GUID",
    "Volume filtering stopped",
    "Unattempted",
    "Failure",
};

BaselineConsistency* BaselineConsistency::m_theBaseline = NULL;
boost::mutex BaselineConsistency::m_theBaselineLock;

CrashConsistency* CrashConsistency::m_theCrashC = NULL;
boost::mutex CrashConsistency::m_theCrashCLock;

AppConsistency* AppConsistency::m_theAppC = NULL;
boost::mutex AppConsistency::m_theAppCLock;

boost::mutex g_sendTagsCommonLock;

boost::mutex g_driverPreCheckLock;

//Distributed Vacp ...Coordinated Flushing of Nodes
/*const unsigned int PREPARE_WAIT_TIME = 60000;
const unsigned int QUIESCE_WAIT_TIME = 180000;
const unsigned int RESUME_WAIT_TIME = 180000;*/

bool g_bDistributedVacp = false;
bool g_bMasterNode = false;
bool g_bCoordNodes = false;
bool g_bThisIsMasterNode = false;
bool g_bDistMasterFailedButHasToFacilitate = false;
bool g_bDistributedVacpFailed = false;
ServerCommunicator* g_pServerCommunicator = NULL;
extern unsigned short g_DistributedPort;
bool g_bDistributedTagRevokeRequest = false;
//Distributed Vacp

bool    gbPrint   = false;
bool	gbVerify  = false;
bool	gbTagSend = false;
bool	gbRetryable = false;
DWORD	gdwRetryOperation = 0;
DWORD	gdwMaxRetryOperation  = 0;
DWORD	gdwMaxSnapShotInProgressRetryCount = 90;
bool	gbIsOSW2K3SP1andAbove = false;
bool	gbRemoteConnection = false;
bool	gbRemoteSend = false;
DWORD   gdwNumberOfVolumes = 0;
DWORD   gdwTagTimeout = 0;
bool	gbGetSyncVolumeStatus = false;
bool	gbGetVolumeStatus = false;
HANDLE  ghControlDevice = NULL ;
GUID	gTagGUID;
GUID	gAlternateTagGUID;
bool	gbIssueTag = true;
bool	gbSyncTag = false;
bool    gbDoNotIncludeWriters = false;
bool    gbPersist = false;
bool    bDoNotIncludeWritrs = false;
bool    gbUseInMageVssProvider = true;//by-default InMagVssProvider will be used
HRESULT ghrErrorCode = S_OK;
bool    gbEnumSW = false;
bool	gbCrashTag = false;
bool	gbUseDiskFilter = true;
std::set<std::string>        gExcludedVolumes;

bool        gbBaselineTag = false;
std::string gStrBaselineId;

std::string gStrTagGuid;

bool    gbTagLifeTime		= false;//Tag Life Time related global variables...added for sparse retention enhancements
bool    gbLTForEver			= false;
bool    gbIgnoreNonDataMode = false;//continues to issue the tag even if one of the volumes is not in data mode
bool    gbSkipUnProtected = true;//by-default Vacp will have "-su switch"behaviour
bool    gbIOCompleteTimeOutOccurred = false;

unsigned long long    gulLTMins   = 0;//LT:LifeTime in minutes
unsigned long long    gulLTHours = 0;//60 minutes
unsigned long long    gulLTDays  = 0;//24 hours
unsigned long long    gulLTWeeks   = 0;//7 days
unsigned long long    gulLTMonths  = 0;//30 days
unsigned long long    gulLTYears   = 0;//365 days

unsigned long long gullLifeTime = 0;


unsigned long gulDistCrashTagStartTime, gulDistCrashTagEndTime, gulDistCrashTagDiffTime;

extern const string gstrInMageVssProviderGuid;
PVOID gpTagOutputData;
vector<string> gvVolumes;
vector<const char* >vGroupVolumes;
std::set<std::string> gvValidVolumes;
bool bMoreVolGroupsAvailable = false;
OVERLAPPED gOverLapIO = { 0, 0, 0, 0, NULL /* No event. */ };
OVERLAPPED gOverLapTagIO = { 0, 0, 0, 0, NULL /* No event. */ };
OVERLAPPED gOverLapRevokationIO = {0,0,0,0,NULL/* No event.*/};
OVERLAPPED gOverLapBarrierIO = { 0, 0, 0, 0, NULL };

extern std::map<wstring, string> gDiskGuidMap;
extern std::map<std::wstring, std::string>     g_DynamicDiskIds;
extern std::map<std::wstring, std::string>     g_StorageSpaceDiskIds;
extern std::map<std::wstring, std::string>     g_StoragePoolDiskIds;

extern unsigned char *gpInMageVssWriterGUID;


char szWriteOrderState[4][32] = {"UnInitialized", "Bitmap", "MetaData", "Data" };
string volumePathNames[4096];
int gShadowCopyTrackId;
bool gbPersistSnapshots;
bool gbDeleteSnapshots;
bool gbIncludeAllApplications = false;
bool gbSystemLevelTag = false;
bool gbAllVolumes = false;
ULONG ulProcessId = 0;
bool gbPreScript = false;
bool gbPostScript = false;
std::string gstrPreScript;
std::string gstrPostScript;

extern std::vector<Writer2Application_t> vDynamicVssWriter2Apps;
#if(_WIN32_WINNT > 0x500)
VSS_ID g_ProviderGuid;
#endif
#if (_WIN32_WINNT >= 0x502)
  IVssBackupComponents *gpVssBackupComponentsForDel = NULL;
  IVssBackupComponentsEx *gpVssBackupComponentsExForDel = NULL;
#endif

bool gbDeleteAllVacpPersistedSnapshots = false;
bool gbDeleteSnapshotIds = false;
bool gbDeleteSnapshotSets = false;

std::string                 g_strLocalHostName;
std::string                 g_strMasterHostName;
std::string                 g_strMasterNodeFingerprint;
std::string                 g_strLocalIpAddress;
std::string                 g_strRcmSettingsPath;
std::string                 g_strProxySettingsPath;
std::vector<std::string>    g_vstrCoordNodes;
strset_t                    g_vLocalHostIps;
std::string                 g_strCommandline;

DRIVER_VERSION g_inmFltDriverVersion;

//VacpConfig* VacpConfig::m_pVacpConfig = NULL;
//string VacpConfig::m_strConfFilePath;
//VacpConfig *gpVacpConf;

VacpConfigPtr VacpConfig::m_pVacpConfig;

short gsVerbose = 0;
bool  gbParallelRun = false;
bool  gbVToAMode = false;
bool  gbIRMode = false;
ULONG gExitCode = 0;

FailMsgList g_VacpFailMsgsList;
FailMsgList g_MultivmFailMsgsList;
VacpLastError g_vacpLastError;

boost::shared_ptr<Event> g_Quit;

static const std::string CommunicatorTypeSched = "Scheduler";

const std::string AppPolicyLogsPath = "\\Application Data\\ApplicationPolicyLogs";
const std::string AppDataEtcPath = "\\Application Data\\etc";
const std::string ConsistencyScheduleFile = "consistencysched.json";
const std::string LastAppConsistencyScheduleTime = "LastAppConsistencyScheduleTime";
const std::string LastCrashConsistencyScheduleTime = "LastCrashConsistencyScheduleTime";
const std::string DrivesMappingFile = "\\failover\\data\\inmage_drive.txt";

HRESULT InitializeCom();
void UninitializeCom();

uint32_t gnParallelRunScheduleCheckInterval = 10; // in secs

uint32_t gnRemoteScheduleCheckInterval = 30; // in secs

void PersistConsistencyScheduleTime(CLIRequest_t &CmdRequest,
    uint64_t currentPolicyRunTime);


extern void inm_printf(const char * format, ...);
extern void inm_printf(short logLevl, const char* format, ...);
extern void inm_log_to_buffer_start();
extern void inm_log_to_buffer_end();
extern int inm_init_file_log(const std::string&);
extern void NextLogFileName(const std::string& logFilePath,
    std::string& preTarget,
    std::string& target,
    const std::string& prefix = std::string(""));
extern bool RenameLogFile(const std::string& logFilePath, const std::string& newLogFilePath, std::string& err);

std::string g_strLogFilePath;

std::string g_strHydrationFilePath;

bool            g_bSignaleQuitFromCrashConsisency = false;

//InMageVssProvider support
#if (_WIN32_WINNT > 0x500)
    CComPtr<IVacpProvider> pInMageVssProvider = NULL;
    class VssAppConsistencyProvider;
    vector<VSS_ID> gvSnapshotSets;
    VssAppConsistencyProvider *gptrVssAppConsistencyProvider = NULL;

    #define INMAGE_VSS_PROVIDER_KEY "SOFTWARE\\SV Systems\\InMageVssProvider\\"
    #define INMAGE_VSS_PROVIDER_REMOTE_KEY "SOFTWARE\\SV Systems\\InMageVssProvider\\Remote\\"
    #define INMAGE_VSS_PROVIDER_LOCAL_KEY "SOFTWARE\\SV Systems\\InMageVssProvider\\Local\\"
    #define INMAGE_REMOTE_TAG_ISSUE					"Remote Tag Issue"
    #define INMAGE_SYNC_TAG_ISSUE					"Sync Tag Issue"
    #define INMAGE_TAG_REACHED_STATUS				"Tag Reached Status"
    #define INMAGE_TAG_DATA_BUFFER					"Buffer"
    #define INMAGE_TAG_DATA_BUFFER_LEN				"BufferLen"
    #define INMAGE_OUT_BUFFER_LEN					"OutBufferLen"
    #define INMAGE_REMOTE_TAG_PORT_NO				"Port No"
    #define INMAGE_REMOTE_TAG_SERVER_IP				"Server Ip"
    #define INMAGE_EVENTVIEWER_LOGGING_KEY			"EventViewerLoggingFlag"


#endif

    #define QUERY_CX_FOR_LICENSE_INFO "get_app_license.php"
    #define APP_LEVEL_CONSISTENCY   2
    #define FS_LEVEL_CONSISTENCY    1
    #define CRASH_LEVEL_CONSISTENCY 0
    #define ERROR_LICENSE_LEVEL		-1
    #define DEFAULT_CS_IP_PORT_NUMBER 80

    typedef int SV_INT;
    typedef unsigned long  SV_ULONG;

    HRESULT VacpPostToCxServer( const char *pszSVServerName,
                       SV_INT InmHttpPort,
                       const char *pchInmPostURL,
                       char *pchInmPostBuffer,
                       SV_ULONG dwInmPostBufferLength );
    int GetLicenseType(char * pchLicenseString);

    extern HRESULT ComInitialize();

std::string g_strCxIp = "";
bool g_bCSIp = false;//Configuration Server IP Address (equivalent to Cx Server IP Address
DWORD g_dwCxPort = DEFAULT_CS_IP_PORT_NUMBER;
int g_nLicenseType = ERROR_LICENSE_LEVEL;

CopyrightNotice displayCopyrightNotice;

//Convert the life time parameters into nano seconds
unsigned long long ConvertLifeTimeToNanoSecs()
{
    unsigned long long ullNanoSecs = 0;
    if(gbLTForEver)
    {
        ullNanoSecs = static_cast<unsigned long long> (~0);
    }
    else
    {
        ullNanoSecs = 
        /*(gulLTMins * 60 * 1000 * 1000 * 10)					+
        (gulLTHours * 60 * 60 * 1000 * 1000 * 10)				+
        (gulLTDays * 24 * 60 * 60 * 1000 * 1000 * 10)			+
        (gulLTWeeks * 7 * 24 * 60 * 60 * 1000 * 1000 * 10)		+
        (gulLTMonths * 30 * 24 * 60 * 60 * 1000 * 1000 * 10)	+
        (gulLTYears * 12 * 30 * 24 * 60 * 60 * 1000 * 1000 * 10);*/

        //first convert everything into seconds
        ullNanoSecs =((gulLTMins * 60 ) +
        (gulLTHours * 60 * 60) +
        (gulLTDays * 24 * 60 * 60) +
        (gulLTWeeks * 7 * 24 * 60 * 60) +
        (gulLTMonths * 30 * 24 * 60 * 60) +
        (gulLTYears * 12 * 30 * 24 * 60 * 60)) * 1000 * 1000 * 10;
    }
    return ullNanoSecs;
}

void usage()
{

    wprintf(L"\nUsage:\n");
    
#if (_WIN32_WINNT >= 0x502)
    //Windows 2003 and above

    if(gbIsOSW2K3SP1andAbove)
    {
        //Windows 2003 + SP1 above
      wprintf(L"vacp.exe [-a [sr;]<app1[</comp1>..];..>][-v <vol1;..>][-guid <volguid1;..>][-verify][-t <tag1;..>][-w <writer instance anme;..>][-f][-x][-p<app1;..>][-s][-provider <Provider ID>][-notag][-sync][-tagtimeout <Timeout value in second>][-remote -serverdevice <device1,device2..> -serverip <server IP> -serverport <server port>][-enumSW] [-systemlevel [-cc]] [-distributed -mn <MasterNode IP Address> -cn<clientnode1_IP,clientnode2_IP;...> [-dport <port number>]] [-prescript<custom prescript>] [-postscript <custom prescript>] [-tagLifeTime { [-forever] [-years <no of years>] [-months <no of months>] [-weeks <no of weeks>] [-days <no of days>] [-hours <no of hours>] [-minutes <no of minutes>] } { -persist}{-delete {all|[shadowcopyids id1;id2;...] [shadowcopysets setid1;setid2;...]}] } [-h]\n\n");
    }
    else
    {	//Windows 2003 without SP1
      wprintf(L"vacp.exe [-a [sr;]<app1[</comp1>..];..>][-v <vol1;..>][-guid <volguid1;..>][-verify][-t <tag1;..>][-f][-x][-p<app1;..>][-s][-provider <Provider ID>][-notag][-sync][-tagtimeout <Timeout value in second>][-remote -serverdevice <device1,device2..> -serverip <server IP> -serverport <server port> ] [-enumSW][-systemlevel] [-distributed -mn <MasterNode IP Address> -cn<clientnode1_IP,clientnode2_IP;...> [-dport <port number>]] [-prescript <custom prescript>] [-postscript <custom prescript>][-h]\n\n");
    }
#elif (_WIN32_WINNT == 0x501)
    //Windows XP
    wprintf(L"vacpxp.exe [-a [sr;]<app1[</comp1>..];..>][-v <vol1;..>][-guid <volguid1;..>][-verify][-t <tag1;..>][-x][-p<app1;..>][-s][-provider <Provider ID>][-notag][-sync][-tagtimeout <Timeout value in second>][-remote -serverdevice <device1,device2..> -serverip <server IP> -serverport <server port> ][-systemlevel] [-distributed -mn <MasterNode IP Address> -cn<clientnode1_IP,clientnode2_IP;...> [-dport <port number>]] [-prescript <custom prescript>] [-postscript <custom postscript>][-h]\n\n");
#else
    //Windows 2000
    wprintf(L"vacp2k.exe -v <vol1;..> -t <tag1;..> [-s][-sync][-tagtimeout <Timeout value in second>][-h]\n\n");
#endif


#if (_WIN32_WINNT > 0x500)
    wprintf(L" -a [sr;]<appName1[</comp1></compt2>..];appName2[</comp1></compt2>..];..>\n\n");
    wprintf(L"    \t Specifies one or more applications with zero or more components.\n");
    wprintf(L"    \t sr: stands for System Recovery. If sr option is used then the following writers\n");
    wprintf(L"    \t are automatically added as part of applications...\n");
    wprintf(L"    \t SystemFiles;Registry;EventLog;WMI;COM+REGDB;FS;CA;AD;FRS;BootableState;\n");
    wprintf(L"    \t ServiceState;ClusterService;VSSMetaDataStore;PerformanceCounter;;ADAM;ClusterDB;TSG;DFSR\n");
    wprintf(L"    \t When component names(optional) are mentioned, consistency would be\n");	
    wprintf(L"    \t ensured only for those components, otherwise consistency of entire\n");		
    wprintf(L"    \t application would be ensured. Specify \"all\" to generate\n");
    wprintf(L"    \t consistency tags for all applications.\n");
    wprintf(L"    \t Eg: sql/masterdb/testdb;exchange/\"First Storage Group\";systemfiles etc\n\n");
    wprintf(L"    \t Eg: sql/masterdb/testdb;exchange/\"First Storage Group\";systemfiles etc\n\n");
#endif
    wprintf(L" -v <vol1;vol2;..>\n\n");
    wprintf(L"    \t Specifies one or more volumes or volume mount points.\n"); 
    wprintf(L"    \t Specify \"all\" to generate tags on all volumes in the system\n");
    #if (!(_WIN32_WINNT > 0x500))
        wprintf(L"    \t (except mount points).\n");
    #endif
    wprintf(L"    \t Eg: c:;d:;y:;x:\\Data1;x:\\Data2 etc.\n");
    wprintf(L"    \t Here Data1 and Data2 are volume mount points\n\n");
    wprintf(L" -verify\n\n");
    wprintf(L"    \t Vacp Verifies if it can take a snapshot or not.Also it will not issue a tag!\n\n");

#if (_WIN32_WINNT >= 0x0501)
    wprintf(L" -guid <volguid1;volguid2;..>\n\n");
    wprintf(L"    \t Specifies one or more volume GUIDs or volume mount point GUIDs.\n"); 
    wprintf(L"    \t Eg: C4E7FA17-C0D0-4139-880F-E0874B907FA0;18EF281B-2EE8-4095-84DB-C938FE82987D\n");
    wprintf(L"    \t Here in the above example two volume GUIDs are separated by a semi colon.\n\n");
#endif
        
    wprintf(L" -t <tagName1;tagNam2;..>\n\n");
    wprintf(L"    \t Specifies one or more bookmark tags.\n");
    wprintf(L"    \t Eg: \"WeeklyExchangeBackup\";\"AfterQuartelyResults\" etc\n\n");
    wprintf(L"    \t NOTE: Tags cannot be issued to Locked Volumes\n");
    wprintf(L"    \t       unless -x option is specified.\n\n");

    
#if (_WIN32_WINNT >= 0x502)
    if(gbIsOSW2K3SP1andAbove)
    {
        wprintf(L" -w \t Specifies one or more VSS aware application writer instance name.\n");
        wprintf(L"    \t When application writer instance name is provided consistency would be\n");
        wprintf(L"    \t ensured only for those writers.\n");
        wprintf(L"    \t Eg: ExchangeIS (Exchange Information Store)\n");
        wprintf(L"    \t     ExchangeRepl (Exchange Replication Service) \n\n");

        //-persist option
        wprintf(L" -persist \t Persists the Volume Shadow Copy Service Snapshosts to the disk.\n\n");

        //-delete option
        wprintf(L" -delete {all|[shadowcopyids id1;id2;...] [shadowcopysets setid1;setid2;...]}\n");
        wprintf(L"	\tall          Deletes all the shadow copies persisted by Vacp in all the volumes.WARNING:This option ");
        wprintf(L"	\t             \twill delete all the snapshots in the system. Exercise this option only when you want to ");
        wprintf(L"	\t             \tdelete all the snapshots in the system.\n\n");
        wprintf(L"	\tshadowcopyids\t List of semiconlon separated Shadow Copy IDs to be deleted.\n\n");
        wprintf(L"	\tshadowcopysets\t List of semiconlon separated Shadow Copy Sets to be deleted.\n\n");

    }
    wprintf(L" -f \t Perform \"FULL\" backup. Default is \"COPY\" based backup\n");
    wprintf(L"    \t FULL backup updates application specific logs w/ backup history\n");
    wprintf(L"    \t COPY backup copies files on disk regardless of the state of each\n");
    wprintf(L"    \t file's backup history\n\n");
#endif
    
#if (_WIN32_WINNT > 0x500)
    wprintf(L" -x \t Exclude all applications to generate tags w/o any\n");
    wprintf(L"    \t consistency mechanism. This option must be specified\n");
    wprintf(L"    \t along with -v and -t options. -a and -x are mutually exclusive.\n\n");

    wprintf(L" -p [<appName1;appName2;..>]\n\n");
    wprintf(L"    \t Prints the affected volumes of specified applications. \n");
    wprintf(L"    \t Specify \"all\" to print all applications and their affected volumes\n");	
    wprintf(L"    \t If no application name is specified, detailed information of all\n");
    wprintf(L"    \t applications will be displayed. This option should not be combined\n");
    wprintf(L"    \t with any other option.\n\n");
    wprintf(L"    \t NOTE: If no other option is specified, -p is a default option.\n\n");
#endif	
    wprintf(L" -s \t Skip driver mode check before sending tags to the driver.\n");
    wprintf(L"    \t By defult VACP checks driver mode for each given volume.\n");
    wprintf(L"    \t If driver is not in write ordered data mode for a protected volume, \n");
    wprintf(L"    \t it will not send tag to the driver.\n\n");

    wprintf(L" -provider <Provider ID>\n\n");
    wprintf(L"    \t Specifies VSS provider ID that VACP should use.\n");
    wprintf(L"    \t By defult VACP uses Microsoft Software Shadow copy provider.\n");
    wprintf(L"    \t Example -provider f5974134-8c9d-8975-af86-671ad95b20d7 \n\n");

    wprintf(L" -notag\t When specify, VACP will not create and issue the tag.\n");
    wprintf(L"    \t It simply creates snapshot for the consistency and delete it.\n\n");
    wprintf(L" -sync Perform synchronous tag operation.\n");
    wprintf(L"    \t When specified VACP will wait forever till tag is committed by the\n");
    wprintf(L"    \t driver. To provide timeout use -tagtimeout option\n");
    wprintf(L"    \t without -tagtimeout option timeout is INFINITE\n\n");
    
    wprintf(L" -tagtimeout <Timeout value in second>\n\n");
    wprintf(L"    \t This tagtimeout is applicable for synchronous tag only.\n");
    wprintf(L"    \t If tag is not committed till timeout, VACP will consider this as.\n");
    wprintf(L"    \t failure. Timeout does NOT mean that after timeout tag will be\n");
    wprintf(L"    \t dropped. Depending upon situation after timeout, tag may or may \n");
    wprintf(L"    \t not be committed.\n\n");

#if	(_WIN32_WINNT >= 0x501)
    wprintf(L" -remote This switch is used only in the Array based protection. It is required for sending tag to remote vacp server.\n\n");
    
    wprintf(L" -serverdevice <device1;device2;device3..>\n\n");
    wprintf(L"    \t Specifies device name separated by semicolon on which tag is\n");
    wprintf(L"    \t to be applied. For fabric, serverdevice is LUN IDs.\n\n");

    wprintf(L" -serverip <Server IP Address>\n\n");
    wprintf(L"    \t Specifies server IP address where VACP Server is running.\n");
    wprintf(L"    \t Example -serverip 10.0.10.24\n\n");
    
    wprintf(L" -serverport <Server Port>\n\n");
    wprintf(L"    \t Specifies vacp server port number that to be used by VACP client to\n");
    wprintf(L"    \t connect to vacp server. This is an optional parameter.\n");
    wprintf(L"    \t If not specified it will use 20003 port number.\n\n");

    wprintf(L" -enumSW\n\n");
    wprintf(L"    \t Specifies to enumerate all the System Writer Components/DBs/Files VSS way.\n");
    wprintf(L"    \t This switch will result in Vacp taking more time to discover all the components,\n");
    wprintf(L"    \t databases and files affected by the System Writer of VSS. But this approach will \n");
    wprintf(L"    \t be fool-proof as to not miss out any of the VSS's System Writer components in the snapshot.\n\n");

    wprintf(L" -systemlevel [-cc]\n\n");
    wprintf(L"   \t This switch will quiesce all the applications and all the volumes available in the system.\n");
    wprintf(L"   \t This switch is equivalent to calling vacp.exe -a all -v all.\n\n");
    wprintf(L" -cc \n");
    wprintf(L"   \t This switch provides crash consistency.\n\n");

    wprintf(L" -distributed \n");
    wprintf(L"    \t Specifies to issue a distributed tag across systems.\n\n");
    wprintf(L" -mn <MasterNode IP Address>\n");
    wprintf(L"    \t Specifies the IP address of the Master Node while issuing a Distributed Tag.\n\n");
    wprintf(L" -cn <clientnode1_IP,clientnode2_IP;...>\n");
    wprintf(L"    \t Specifies the IP addresses of client nodes separated by comma or semicolon.\n\n");
    wprintf(L" -dport <port number>\n\n");
    wprintf(L"    \t Port number used for Distributed Vacp communication on the Master Node.\n\n");

    wprintf(L" -prescript <custom prescript>\n\n");
    wprintf(L"    \t A custom script that is invoked before quiescing the volumes and issuing of tags..\n");
    wprintf(L"    \t Example -prescript C:\\Program Files\\PreTest.exe\n\n");

    wprintf(L" -postscript <custom postscript>\n\n");
    wprintf(L"    \t A custom script that is invoked after successfully issuing the tags..\n");
    wprintf(L"    \t Example -postscript C:\\Program Files\\PostTest.exe\n\n");

    wprintf(L" -tagLifeTime [-forever] {[-years <no of years>] [-months <no of months>] [-weeks <no of weeks>] [-days <no of days>] [-hours <no of hours>] [-minutes <no of minutes>]} \n\n");
    wprintf(L" -tagLifeTime\n");
    wprintf(L"    \t Renders life time to a tag and after the specified life time, the tag expires.\n\n");
    wprintf(L" -forever\n");
    wprintf(L"    \t This bookmark is never deleted in the sparce retention database.\n\n");
    wprintf(L"    \t The forever switch overrides all the other optional switches specified.\n\n");
    wprintf(L" -years <no of years>\n");
    wprintf(L"    \t This bookmark is deleted in the sparce retention database after the specified years from the time of its generation \n\n");
    wprintf(L" -months <no of months>\n");
    wprintf(L"    \t This bookmark is deleted in the sparce retention database after the specified months from the time of its generation\n\n");
    wprintf(L" -weeks <no of weeks>\n");
    wprintf(L"    \t This bookmark is deleted in the sparce retention database after the specified weeks from the time of its generation\n\n");
    wprintf(L" -days <no of days>\n");
    wprintf(L"    \t This bookmark is deleted in the sparce retention database after the specified days from the time of its generation\n\n");
    wprintf(L" -hours <no of hours>\n");
    wprintf(L"    \t This bookmark is deleted in the sparce retention database after the specified hours from the time of its generation\n\n");
    wprintf(L" -minutes <no of minutes>\n");
    wprintf(L"    \t This bookmark is deleted in the sparce retention database after the specified minutes from the time of its generation\n\n");
    wprintf(L"    \t All the optinal switches years, months,weeks,days,hours and minutes can be used together also\n\n");	

    wprintf(L" -h \t Print the usage message.\n");
    wprintf(L"    \t This option should not be combined with any other option.\n");
    wprintf(L"\n");

    wchar_t wzBinaryName[16] = {0};
#if (_WIN32_WINNT > 0x501)
    inm_wcscpy_s(wzBinaryName,ARRAYSIZE(wzBinaryName),L"vacp.exe");
#elif (_WIN32_WINNT == 0x501)
    inm_wcscpy_s(wzBinaryName,ARRAYSIZE(wzBinaryName),L"vacpxp.exe");
#endif
    wprintf(L"Example:\n");
    wprintf(L"Example for normal setup:\n");
    wprintf(L"%s -v D:;E:;K:\\mountpoint; -t TAG1\n",wzBinaryName);	
#if (_WIN32_WINNT >= 0x0501)
    wprintf(L"%s -guid C4E7FA17-C0D0-4139-880F-E0874B907FA0;18EF281B-2EE8-4095-84DB-C938FE82987D\n",wzBinaryName);
#endif
    wprintf(L"%s -a SQL2005 -t TAG2\n",wzBinaryName);	
    wprintf(L"%s -a SQL2005 -provider f5974134-8c9d-8975-af86-671ad95b20d7 -t TAG3\n",wzBinaryName);	
    wprintf(L"%s -a SQL2005 -t TAG4 -sync\n",wzBinaryName);	
    wprintf(L"%s -v D:;K:\\mountpoint;F: -t TAG5 -sync -tagtimeout 90\n",wzBinaryName);	
    wprintf(L"%s -v D:;K:\\mountpoint;F: -t TAG5 -sync -tagtimeout 90 -enumSW\n",wzBinaryName);
    wprintf(L"%s -systemlevel\n",wzBinaryName);
    wprintf(L"%s -systemlevel -t testtag1\n",wzBinaryName);
    wprintf(L"%s -systemlevel -v D:;E:: -t testtag2\n",wzBinaryName);
    wprintf(L"%s -systemlevel -cc\n", wzBinaryName);
    
    wprintf(L"\n");	
    wprintf(L"Example for client-server setup:\n",wzBinaryName);
    wprintf(L"%s -v D:;E:;F: -t TAG1 -remote -serverdevice /dev/mnt1;/dev/mnt2 -serverip 11.3.214.10 -serverport 20003 \n",wzBinaryName);
    wprintf(L"%s -a SQL2005 -t SQL2005TAG1 -remote -serverdevice /dev/mnt1;/dev/mnt2  -serverip 11.3.214.10 \n\n",wzBinaryName);
    wprintf(L"%s -a SQL2005 -t SQL2005TAG1 -remote -serverdevice /dev/mnt1;/dev/mnt2  -serverip 11.3.214.10 -enumSW\n\n",wzBinaryName);

    wprintf(L"\n");
    wprintf(L"Example for Verification of VSS:\n",wzBinaryName);
    wprintf(L"%s -v D:;K:\\mountpoint;E:;F: -verify\n\n",wzBinaryName);
    wprintf(L"%s -a SQL2005 -verify\n\n",wzBinaryName);

    wprintf(L"\n%s –systemlevel –t testtag –taglifetime forever",wzBinaryName);
    wprintf(L"\n%s –a sql2008 –t sqltesttag –taglifetime –years 1 –months 2 –weeks 3 –days 4 –hours 5 –minutes 30",wzBinaryName);
    wprintf(L"\n%s –a sql2005 –t sql2005tag –taglifetime –days 250",wzBinaryName);
    wprintf(L"\n%s –a sql –t sqltag –taglifetime –hours 12 –minutes 30",wzBinaryName);
    if(gbIsOSW2K3SP1andAbove)
    {
        wprintf(L"%s -v D:;K:\\mountpoint;F: -t TAG5 -persist\n",wzBinaryName);
        //wprintf(L"%s -v D: -t TAG5 -delete\n",wzBinaryName);
        //wprintf(L"%s -delete\n",wzBinaryName);
        wprintf(L"%s -delete all\n",wzBinaryName);
        wprintf(L"WARNING:-delete all option will delete all the snapshots in the system. Exercise this option only when you want to delete all the snapshots.\n");
        //wprintf(L"%s -delete self\n",wzBinaryName);
        //wprintf(L"%s -delete last\n",wzBinaryName);
        //wprintf(L"%s -delete vols <vol1;vol2;mntpoint1;mntpoint2;....>\n",wzBinaryName);
        //wprintf(L"%s -delete apps <app1;app2;...>\n",wzBinaryName);
        wprintf(L"%s -delete shadowcopyids <snapshot-guid1;snapshot-guid2;...>\n",wzBinaryName);
        wprintf(L"%s -delete shadowcopysets <snapshotset-guid1;snapshotset-guid2;...>\n",wzBinaryName);
    }

    wprintf(L"\n");
    wprintf(L"Example for issuing a distributed tag across systems\nodes:\n");
    wprintf(L"%s -systemlevel -t dTag1 -distributed -mn 10.0.1.5 -cn 10.0.1.6;10.0.1.7;10.0.1.8;10.0.1.9,10.0.1.95; -dport 20004\n\n",wzBinaryName);
    wprintf(L"If -dport switch is not used then the default port used for issuing Distributed Tag is 20004\n");
    wprintf(L"%s -sytemlevel -t dTag -distributed -mn 10.0.1.5 -cn 10.0.1.6;10.0.1.7;10.0.1.8;10.0.1.9,10.0.1.95;\n\n",wzBinaryName);
#endif //#if (_WIN32_WINNT >= 0x501)
}

#if (_WIN32_WINNT >= 0x502)

//Clean the VssBackupComponentsEx
void CleanVssBkpObj()
{
  if(gpVssBackupComponentsExForDel)
  {
    gpVssBackupComponentsExForDel->Release();
    gpVssBackupComponentsExForDel = NULL;
  }
  /*if(gpVssBackupComponentsForDel)
  {
    gpVssBackupComponentsForDel->Release();
    gpVssBackupComponentsForDel = NULL;
  }*/
}
//Create VssBackupComponentsEx
bool GetVssBkpObj(IVssBackupComponentsEx** ppBkpObj)
{
  bool bFlag = false;
  HRESULT hrDelete;
  if((gpVssBackupComponentsForDel == NULL) || (gpVssBackupComponentsExForDel == NULL))
  {
    do
    {
      CHECK_SUCCESS_RETURN_VAL(CreateVssBackupComponents(&gpVssBackupComponentsForDel),hrDelete);
      CHECK_SUCCESS_RETURN_VAL(gpVssBackupComponentsForDel->QueryInterface(IID_IVssBackupComponentsEx,\
                                              (void**)&gpVssBackupComponentsExForDel),hrDelete);
    
      if((S_OK == hrDelete) && 
        (gpVssBackupComponentsForDel != NULL) && 
        (gpVssBackupComponentsExForDel != NULL))
      {
        CHECK_SUCCESS_RETURN_VAL(gpVssBackupComponentsExForDel->InitializeForBackup(), hrDelete);
      
        CHECK_SUCCESS_RETURN_VAL(gpVssBackupComponentsExForDel->SetBackupState (
                                  true,       /* component based backup */
                                  true,        /* Bootable system state backup */
                                  VSS_BT_COPY, /* FULL/COPY backup MCHECK as to why not VSS_BT_FULL*/
                                  false        /* No partial file backup */
                                  ), hrDelete); 
        CHECK_SUCCESS_RETURN_VAL(gpVssBackupComponentsExForDel->SetContext(VSS_CTX_APP_ROLLBACK), hrDelete);
        *ppBkpObj = gpVssBackupComponentsExForDel;
        bFlag = true;
      }
    }while(false);
    if(S_OK != hrDelete)
    {
      bFlag =false;
    }
  }
  else
  {
    *ppBkpObj = gpVssBackupComponentsExForDel;
    bFlag = true;
  }
  return bFlag;
}
//Delete the Shadow Copies created by Vacp...read the shadow copy list from registry
bool DeleteVacpShadowCopies()
{
  bool bFlag = false;
  return bFlag;
}
//Delete all shadow copies in the system
bool DeleteShadowCopies()
{
  bool bFlag = true;
  IVssBackupComponentsEx* pVssBkpCmpExObj = NULL;
  do
  {
    if(false == GetVssBkpObj(&pVssBkpCmpExObj))
    {
      bFlag = false;
      break;
    }
    // Get the list of all shadow copies. 
    CComPtr<IVssEnumObject> pIEnumSnapshots;
    HRESULT hr = pVssBkpCmpExObj->Query( GUID_NULL, 
                                      VSS_OBJECT_NONE, 
                                      VSS_OBJECT_SNAPSHOT, 
                                      &pIEnumSnapshots );

    //CHECK_COM_ERROR(hr, L"m_pVssObject->Query(GUID_NULL, VSS_OBJECT_NONE, VSS_OBJECT_SNAPSHOT, &pIEnumSnapshots )")
    DWORD dwErr = GetLastError();

    // If there are no shadow copies, just return
    if (hr == S_FALSE) 
    {
        inm_printf("\nThere are no shadow copies on the system.Error = %d\n",dwErr);
        bFlag = false;
        break;
    } 

    // Enumerate all shadow copies. Delete each one
    VSS_OBJECT_PROP Prop;
    VSS_SNAPSHOT_PROP& Snap = Prop.Obj.Snap;
    
    while(true)
    {
        // Get the next element
        ULONG ulFetched;
        hr = pIEnumSnapshots->Next( 1, &Prop, &ulFetched );
        //CHECK_COM_ERROR(hr, L"pIEnumSnapshots->Next( 1, &Prop, &ulFetched )")

        // We reached the end of list
        if (ulFetched == 0)
            break;

        // Automatically call VssFreeSnapshotProperties on this structure at the end of scope
        SmartSnapshotPropMgr snapshotPropAutoCleanup(&Snap);

        // Print the deleted shadow copy...
        inm_printf("\n\tDeleting shadow copy % 8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x  \
          on %s from provider % 8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x  [0x%08lx]...", 
            GUID_PRINTF_ARG(Snap.m_SnapshotId),
            Snap.m_pwszOriginalVolumeName,
            GUID_PRINTF_ARG(Snap.m_ProviderId),
            Snap.m_lSnapshotAttributes);

        // Perform the actual deletion
        LONG lSnapshots = 0;
        VSS_ID idNonDeletedSnapshotID = GUID_NULL;
        hr = pVssBkpCmpExObj->DeleteSnapshots(
            Snap.m_SnapshotId, 
            VSS_OBJECT_SNAPSHOT,
            FALSE,
            &lSnapshots,
            &idNonDeletedSnapshotID);

        if (FAILED(hr))
        {
            inm_printf("\n");
            inm_printf("Shadow Copy that could.not be deleted:8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x Error=%0x",\
              GUID_PRINTF_ARG(idNonDeletedSnapshotID),hr);
            bFlag = false;
        }
        else if (S_OK == hr)
        {
          inm_printf("...Done.\n");
        }
    }
  }while(false);
    if(!bFlag)
    {
      inm_printf("\nOne or more snapshots could not be deleted.\n");
    }
  return bFlag;
}
//Delete last shadow copy taken by Vacp
bool DeleteLastShadowCopy()
{
  bool bFlag = false;
  return bFlag;
}

//Delete a specified ShadowCopyID
bool DeleteShadowCopy(vector<const char *> vShadowCopies,VACP_DELETE_TYPE delType)
{
  bool bFlag = true;
  HRESULT hr = S_OK;
  std::string strShadowCopy;
  VSS_ID ShadowCopyId;
  IVssBackupComponentsEx* pVssBkpCmpExObj = NULL;
  vector<const char*>::const_iterator iterShadowCopy = vShadowCopies.begin();
  if(iterShadowCopy != vShadowCopies.end())
  {
    if(false == GetVssBkpObj(&pVssBkpCmpExObj))
    {
      bFlag = false;
      return false;
    }
  }
  while(iterShadowCopy != vShadowCopies.end())
  {
    strShadowCopy = *iterShadowCopy;
    if(UuidFromString((unsigned char*)strShadowCopy.c_str(),(UUID*)&ShadowCopyId) == RPC_S_OK)
    {
      LONG lSnapshots = 0;
      VSS_OBJECT_TYPE VssObjType;
      if(delType == Enum_SnapshotId)
      {
        VssObjType = VSS_OBJECT_SNAPSHOT;
      }
      else if (delType == Enum_SnapshotSetId)
      {
        VssObjType = VSS_OBJECT_SNAPSHOT_SET;
      }
      VSS_ID idNonDeletedSnapshotID = GUID_NULL;
      inm_printf("\nTrying to delete the Shadow Copy %s ...\n.",strShadowCopy.c_str());
      hr = pVssBkpCmpExObj->DeleteSnapshots(
          ShadowCopyId, 
          VssObjType,
          FALSE,
          &lSnapshots,
          &idNonDeletedSnapshotID);

      if (FAILED(hr))
      {
          inm_printf("\n\tError while deleting Shadow Copy:  8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x Error=%0x",\
            GUID_PRINTF_ARG(idNonDeletedSnapshotID),hr);
          bFlag = false;
      }
      else if (S_OK == hr)
      {
        inm_printf("\nSuccessfully deleted the Shadow Copy.%s\n",strShadowCopy.c_str());
      }
    }
    else
    {
      inm_printf("\n Unable to convert the given snapshotID into proper VSS_ID format.\n");
      bFlag = false;
      break;
    }
    iterShadowCopy++;
  }
  return bFlag;
}

bool DeleteShadowCopySet(vector<const char *> vShadowCopySets)
{
  bool bFlag = false;
  return bFlag;
}

//Delete Shadow Copies from the given list of volumes
bool DeleteShadowCopy(vector<std::string>vVols)
{
  bool bFlag = false;
  return bFlag;
}
#endif

bool  GetIpAddressFromHostName(char* lpszHostName,std::string& strIpAddress)
{
  bool bRet = false;
  struct hostent *lInmremoteHost;
  struct in_addr lInmaddr;
  strIpAddress.clear();
  do
  {
      if(isalpha(lpszHostName[0]))
      {
        DebugPrintf("\nCalling gethostbyname with %s\n",lpszHostName);
        lInmremoteHost = gethostbyname(lpszHostName);
      }
      else
      {
        DebugPrintf("\nCalling gethostbyaddress with %s\n",lpszHostName);
        lInmaddr.S_un.S_addr = inet_addr(lpszHostName);
        if(lInmaddr.S_un.S_addr == INADDR_NONE)
        {
          DebugPrintf("\nThe IP address entered must be a legal and valid address.\n");
          bRet= false;
          break;
        }
        else
        {
          lInmremoteHost = gethostbyaddr((char*)&lInmaddr,4,AF_INET);
        }
      }

      if(lInmremoteHost == NULL)
      {
        DWORD dwError = WSAGetLastError();
        if(dwError != 0)
        {
          if(dwError == WSAHOST_NOT_FOUND)
          {
            DebugPrintf("\nThis Host is not found.\n");
            bRet = false;
            break;
          }
          else if(dwError == WSANO_DATA)
          {
            DebugPrintf("\nNo data record found/available.\n");
            bRet = false;
            break;
          }
          else
          {
            DebugPrintf("\nFunction failed with the error = %d\n",dwError);
            bRet = false;
            break;
          }
        }
      }
      else
      {
        DebugPrintf("\nFunction returned:\n");
        DebugPrintf("\nOfficial Name : %s\n",lInmremoteHost->h_name);
        DebugPrintf("\nAlternate Name:%s\n", *(lInmremoteHost->h_aliases));
        DebugPrintf("\nAddress Type  : ");
        switch(lInmremoteHost->h_addrtype)
        {
          case AF_INET:
            DebugPrintf("Address Family is AF_INET\n");
            break;
          case AF_INET6:
            DebugPrintf("Address Family is AF_INET v6\n");
            break;
          case AF_NETBIOS:
            DebugPrintf("Address Family is AF_NETBIOS\n");
            break;
          default:
            DebugPrintf("%d\n",lInmremoteHost->h_addrtype);
            break;
        }
        DebugPrintf("\tAddress Length : %d\n",lInmremoteHost->h_length);
        lInmaddr.s_addr = *(u_long*)lInmremoteHost->h_addr_list[0];
        strIpAddress =  string(inet_ntoa(lInmaddr));
        DebugPrintf("\t First IP Address: %s\n",strIpAddress.c_str());
        bRet = true;
        break;
      }
  }while(false);
  return bRet;
}

// to get current process Id and command line arguments which we have passed and domain name\username  
void GetCmdLineargsandProcessId(void)
{
    DebugPrintf("\nTime: %s\n", GetLocalSystemTime().c_str());

    g_strCommandline = std::string(GetCommandLine());

    BOOLEAN bRetVal = true;

    try
    {
        g_strLocalHostName = GetHostName();
    }
    catch (std::exception const& e)
    {
        DebugPrintf("GetHostName failed with exception %s\n", e.what());
        ExitVacp(VACP_HOSTNAME_DISCOVERY_FAILED, true);
    }

    //Get the current process ID    
    ulProcessId = GetCurrentProcessId();
    DebugPrintf("Process ID: %d\n", ulProcessId); 
}

// this is an RPC for the VSS Provider in CommitSnapshots
// to issue tags when the Volsnap FS freeze if complete
boolean HandleQuiescePhase(unsigned char *pszString)
{

    XDebugPrintf("Handle Quiesce Phase: %s\n", pszString);
    if (gptrVssAppConsistencyProvider != NULL)
    {
        HRESULT hr;
        uint32_t consistencyStatus = AppConsistency::Get().GetStatus();
        consistencyStatus |= CONSISTENCY_STATUS_QUIESCE_SUCCESS;
        AppConsistency::Get().SetStatus(consistencyStatus);

        AppConsistency::Get().QuiesceAckPhase(); // TBD: use singleton object
        
        AppConsistency::Get().TagPhase();
        
        do {

            ACSRequestGenerator *gen = new (std::nothrow) ACSRequestGenerator();
            if (gen == NULL)
            {
                hr = HRESULT_FROM_WIN32(GetLastError());
                std::stringstream ss;
                ss << "HandleQuiescePhase: ACSRequestGenerator failed with " << std::hex << hr;
                DebugPrintf("%s.\n", ss.str().c_str());
                AppConsistency::Get().AddToVacpFailMsgs(VACP_E_COMMIT_SEND_TAG_PHASE_FAILED, ss.str(), hr);
                break;
            }

            hr = gen->GenerateTagIdentifiers(gptrVssAppConsistencyProvider->GetACSRequest());

            delete gen;

            if (hr != S_OK)
            {
                DebugPrintf("GenerateTagIdentifiers failed with 0x%x, exiting... \n", hr);
                break;
            }

            hr = gbUseDiskFilter ? gptrVssAppConsistencyProvider->SendTagsToDiskDriver() : gptrVssAppConsistencyProvider->SendTagsToDriver();
        
        } while (false);

        if (hr != S_OK)
        {
            DebugPrintf("\nFailed to send tags to the driver. Error 0x%x\n", hr);
            gbTagSend = false;
        }
        else
        {
            uint32_t consistencyStatus = AppConsistency::Get().GetStatus();
            consistencyStatus |= CONSISTENCY_STATUS_INSERT_TAG_SUCCESS;
            AppConsistency::Get().SetStatus(consistencyStatus);
            gbTagSend = true;
        }
                
        gptrVssAppConsistencyProvider->SetApplicationConsistencyState(gbTagSend);

        AppConsistency::Get().TagAckPhase();

        AppConsistency::Get().ResumePhase();

    }
    return gbTagSend;
}

// this is an RPC callback for the VSS Provider to check if
// the snapshot is intiated by the VACP
boolean IsSnapshotFromVacp(unsigned char * pszString)
{
    return true;
}


DWORD ProviderCommunicationHandler(void)
{
    XDebugPrintf("ProviderCommunicatioinHandler: Enter\n");
    DWORD dwErr = 0;
    
    if (gptrVssAppConsistencyProvider == NULL)
    {
        DebugPrintf("ProviderCommunicatioinHandler: gptrVssAppConsistencyProvider is not initialized.\n");
        return dwErr;
    }

    VSS_ID snapshotset = gptrVssAppConsistencyProvider->GetRequestorPtr()->GetCurrentSnapshotSetId();
    unsigned char *strGuid;
    UuidToString(&snapshotset, (unsigned char**)&strGuid);

    RPC_STATUS status;
    unsigned char * pszProtocolSequence = (unsigned char *)"ncacn_np";
    unsigned char * pszSecurity = NULL;
    unsigned int    cMinCalls = 1;
    unsigned int    fDontWait = FALSE;
    const size_t    MAX_ENDPOINT_PATH_SIZE = 50;

    char * pszEndPoint = new (nothrow) char[MAX_ENDPOINT_PATH_SIZE];
    if (pszEndPoint == NULL)
    {
        inm_printf("%s: failed to allocate memory for endpoint\n", __FUNCTION__);
        return dwErr;
    }

    memset(pszEndPoint, 0, MAX_ENDPOINT_PATH_SIZE);
    inm_strcpy_s(pszEndPoint, MAX_ENDPOINT_PATH_SIZE, "\\pipe\\");
    inm_strcat_s(pszEndPoint, MAX_ENDPOINT_PATH_SIZE, (char *)strGuid);
    RpcStringFree(&strGuid);
    strGuid = NULL;

    XDebugPrintf("Pipe GUID= %s\n", pszEndPoint);

    do
    {
        status = RpcServerUseProtseqEp(pszProtocolSequence,
            RPC_C_LISTEN_MAX_CALLS_DEFAULT,
            (unsigned char *)pszEndPoint,
            pszSecurity);

        if (status)
        {
            inm_printf("Error: failed to setup vacp rpc RpcServerUseProtseqEp. Status %d\n", status);
            break;
        }

        status = RpcServerRegisterIf(vacprpc_v1_0_s_ifspec,
            NULL,
            NULL);

        if (status)
        {
            inm_printf("Error: failed to setup vacp rpc RpcServerRegisterIf. Status %d\n", status);
            break;
        }

        status = RpcServerListen(cMinCalls,
            RPC_C_LISTEN_MAX_CALLS_DEFAULT,
            fDontWait);

        if (status)
        {
            inm_printf("Error: failed to setup vacp rpc RpcServerListen. Status %d\n", status);
            break;
        }
    } while (false);

    XDebugPrintf("ProviderCommunicatioinHandler: Exit\n");
    return dwErr;
}

void __RPC_FAR * __RPC_USER midl_user_allocate(size_t len)
{
    return(malloc(len));
}

void __RPC_USER midl_user_free(void __RPC_FAR * ptr)
{
    free(ptr);
}


HRESULT Consistency::DiscoverDisksAndVolumes()
{
    // Get Disk List
    HRESULT hresult = GetDiskInfo();
    if (hresult != S_OK)
    {
        inm_printf("\n Unable to get the disk guid/signature on local system.\n");
        return hresult;
    }

    std::vector<const char *> LogicalVolumes;
    if (!IS_CRASH_TAG(m_TagType))
    {
        if (!GetVolumesList(LogicalVolumes) ||
            !GetMountPointsList(LogicalVolumes))
        {
            return E_FAIL;
        }
    }

    m_CmdRequest.vDisks.clear();
    m_CmdRequest.vDiskGuids.clear();
    m_CmdRequest.Volumes.clear();

    m_CmdRequest.m_diskGuidMap = gDiskGuidMap;
    std::map<std::wstring, std::string>::iterator diskIter = m_CmdRequest.m_diskGuidMap.begin();
    for (; diskIter != m_CmdRequest.m_diskGuidMap.end(); diskIter++)
    {
        m_CmdRequest.vDisks.insert(diskIter->second);
        m_CmdRequest.vDiskGuids.insert(diskIter->first);
    }

    if (!IS_CRASH_TAG(m_TagType))
    {
        for (unsigned int index = 0; index < LogicalVolumes.size(); index++)
        {
            std::vector<std::string> disknames;

            hresult = GetDiskNamesForVolume(LogicalVolumes[index], disknames);

            if (S_OK != hresult)
            {
                DebugPrintf("GetDiskNamesForVolume() Failed for volume %s. Error= %d\n", LogicalVolumes[index], hresult);
                continue;
            }

            std::vector<std::string>::iterator diskNameIter = disknames.begin();
            for (; diskNameIter != disknames.end(); diskNameIter++)
            {
                std::string diskname = *diskNameIter;
                std::set<std::string>::iterator diskIter = m_CmdRequest.vDisks.begin();

                for (; diskIter != m_CmdRequest.vDisks.end(); diskIter++)
                {
                    if (diskname.compare(*diskIter) == 0)
                    {
                        m_CmdRequest.Volumes.insert(LogicalVolumes[index]);
                        break;
                    }
                }

                if (diskIter != m_CmdRequest.vDisks.end())
                    break;
            }
        }

        std::vector<std::string> bootableVolumes;
        hresult = GetBootableVolumes(bootableVolumes);
        if (hresult == S_OK)
        {
            std::vector<std::string>::iterator bootVolIter = bootableVolumes.begin();
            for (; bootVolIter != bootableVolumes.end(); bootVolIter++)
            {
                size_t bootVolSize;
                INM_SAFE_ARITHMETIC(bootVolSize = InmSafeInt<unsigned long>::Type(bootVolIter->length()) + 1, INMAGE_EX(bootVolIter->length()))
                char *bootVolume = new (nothrow) char[bootVolSize];
                if (NULL == bootVolume)
                {
                    inm_printf("%s: failed to allocate %d bytes memory for boot volume.\n",
                        __FUNCTION__,
                        bootVolIter->length());

                    hresult = VACP_MEMORY_ALLOC_FAILURE;
                    ghrErrorCode = VACP_MEMORY_ALLOC_FAILURE;
                    std::string errmsg = "failed to allocate memory for boot volume.";
                    ADD_TO_VACP_FAIL_MSGS(VACP_MEMORY_ALLOC_FAILURE, errmsg, hresult);
                    break;
                }
                inm_strcpy_s(bootVolume, bootVolIter->length() + 1, bootVolIter->c_str());
                m_CmdRequest.Volumes.insert(bootVolume);
            }
        }
    }

    return hresult;
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
    }
    m_bDistributedVacp = false;
    m_AcsRequest.bDistributed = false;

    inm_printf("EXITING: %s\n", __FUNCTION__);

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
    if ((currentTime < m_CmdRequest.m_lastCrashScheduleTime) || (currentTime < m_CmdRequest.m_lastAppScheduleTime))
    {
        DebugPrintf("Time moved backward, scheduling immediate.\n");
        timeMovedBack = true;
    }

    uint64_t lastSheduleTime = IS_CRASH_TAG(m_TagType) ? m_CmdRequest.m_lastCrashScheduleTime : m_CmdRequest.m_lastAppScheduleTime;
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
    PersistConsistencyScheduleTime(m_CmdRequest, GetTimeInSecSinceEpoch1970());

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
    if (bSignalQuit && (steady_clock::now() >  m_exitTime))
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
        uint64_t systemUpTime = 0;
        if ((m_lastRunErrorCode == VACP_E_DRIVER_IN_NON_DATA_MODE) &&
            GetSystemUptimeInSec(systemUpTime) &&
            (systemUpTime < (m_interval / 2)))
        {
            m_bInternallyRescheduled = true;
            // TBD: fetch from config param
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
    m_DistributedProtocolStatus = 0;

    if (!g_bDistributedVacp)
        return;

    DWORD dwWait = 0;
    std::string errmsg;
    m_bDistributedVacp = true;
    m_DistributedProtocolState = COMMUNICATOR_STATE_INITIALIZED;

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
    auto iterCoordNodes = m_CmdRequest.vCoordinatorNodes.begin();
    for (/*empty*/; iterCoordNodes != m_CmdRequest.vCoordinatorNodes.end(); iterCoordNodes++)
    {
        boost::algorithm::to_lower(*iterCoordNodes);
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
        inm_printf("\nThe number of Client nodes = %d\n", m_CmdRequest.vCoordinatorNodes.size());
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

            DebugPrintf("\n Failed to create the client socket. Error = ERROR_CREATING_CLIENT_SOCKET at time %s\nRetrying...", GetLocalSystemTime().c_str());
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

            DebugPrintf("Failed to create the client socket. Error = ERROR_CREATING_CLIENT_SOCKET at time %s\nRetrying...\n", GetLocalSystemTime().c_str());
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

void Consistency::DistributedTagGuidPhase()
{
    HRESULT hr = S_OK;
    std::string strTagGuid_Id_Name;
    std::string strTemp;
    int nIndex = 0;
    int nTemp = 0;
    DWORD dwWait = 0;
    std::string strUniqIdMarker = ";UniqueId=";
    std::string strUserTagsMarker = ";UserTagNames=";
    std::string errmsg;
    std::stringstream ssErrMsg;

    if (!m_bDistributedVacp)
        return;

    if (g_bThisIsMasterNode)
    {

        if (m_pCommunicator->GetCoordNodeCount() <= 0)
        {
            ExitDistributedConsistencyMode();
            return;
        }

        DebugPrintf("\nGenerating Tag Identifiers from the Master Vacp Node and sending it to all Coordinator Nodes...\n");

        if (!gStrTagGuid.empty())
        {
            RPC_STATUS rstatus = UuidFromString((RPC_CSTR)gStrTagGuid.c_str(), &m_CmdRequest.distributedTagGuid);

            if (rstatus != RPC_S_OK)
            {
                hr = E_FAIL;
                ssErrMsg << "Failed to parse tag guid " << gStrTagGuid << " with status " << rstatus << ".";
            }
        }
        else
        {
            hr = CoCreateGuid(&(m_CmdRequest.distributedTagGuid));
            if (hr != S_OK)
            {
                ssErrMsg << "CoCreateGuid failed with status " << hr << ".";
            }
        }

        if (hr != S_OK)
        {
            m_DistributedProtocolStatus = ERROR_FAILED_TO_SEND_TAG_GUID_TO_COORDINATOR_NODES;
            ssErrMsg << " Hence this Vacp will NOT issue a Distributed Tag, but will issue a Local Tag";
            DebugPrintf("\n%s \n", ssErrMsg.str().c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, ssErrMsg.str(), ERROR_FAILED_TO_SEND_TAG_GUID_TO_COORDINATOR_NODES);
            ExitDistributedConsistencyMode();
            return;
        }

        m_CmdRequest.distributedTagUniqueId = getUniqueSequence();


        unsigned char *pszTagGuid;
        RPC_STATUS rstatus = UuidToString(&(m_CmdRequest.distributedTagGuid), &pszTagGuid);

        if (rstatus != RPC_S_OK)
        {
            m_DistributedProtocolStatus = ERROR_FAILED_TO_SEND_TAG_GUID_TO_COORDINATOR_NODES;
            errmsg = "UuidToString failed. Hence this Vacp will NOT issue a Distributed Tag, but will issue a Local Tag";
            DebugPrintf("\n%s \n", errmsg.c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, errmsg, m_DistributedProtocolStatus);
            ExitDistributedConsistencyMode();
            return;
        }

        strTagGuid_Id_Name.assign((char*)pszTagGuid, strlen((char*)pszTagGuid));
        RpcStringFree(&pszTagGuid);

        strTagGuid_Id_Name += strUniqIdMarker;
        strTagGuid_Id_Name += m_CmdRequest.distributedTagUniqueId;
        
        strTagGuid_Id_Name += strUserTagsMarker;
        if (m_CmdRequest.BookmarkEvents.size() > 0)
        {
            for (unsigned int iEvent = 0; iEvent < m_CmdRequest.BookmarkEvents.size(); iEvent++)
            {
                const char *Bookmark = m_CmdRequest.BookmarkEvents[iEvent];
                strTagGuid_Id_Name += Bookmark;
                strTagGuid_Id_Name += ";";
            }

        }

        //Send this Guid to the Co-ordinating Vacp Nodes
        int nRet = m_pCommunicator->SendMessageToEachCoordinatorNode(ENUM_MSG_SEND_TAG_GUID,
            strTagGuid_Id_Name);
        if (0 != nRet)
        {
            m_DistributedProtocolStatus = ERROR_FAILED_TO_SEND_TAG_GUID_TO_COORDINATOR_NODES;
            errmsg = "SendMessageToEachCoordinatorNode failed. Hence this Vacp will NOT issue a Distributed Tag, but will issue a Local Tag";
            DebugPrintf("\n%s \n", errmsg.c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, errmsg, m_DistributedProtocolStatus);
            ExitDistributedConsistencyMode();
            return;
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_SERVER_TAG_GUID_COMPLETED;
    }
    else
    {
        DebugPrintf("\nWaiting for the TagGuid to be sent by Master Vacp Node...\n");
        if (m_pCommunicator->WaitForMsg(ENUM_MSG_SEND_TAG_GUID) == -1)
        {
            std::stringstream ss;
            ss << "Failed to get the Tag Guid from the Master Node " << g_strMasterHostName.c_str() << " Hence this Vacp will NOT issue a Distributed Tag, but will issue a Local Tag";
            DebugPrintf("\n%s \n", ss.str().c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, ss.str(), ERROR_FAILED_TO_RECEIVE_TAG_GUID_FROM_MASTER_NODE);
            ExitDistributedConsistencyMode();
            return;
        }

        std::string tagData = m_pCommunicator->GetReceivedData();
        //Interpret the received TagGuid_Id_Name from Master Node;
        nIndex = tagData.find(strUniqIdMarker);
        strTemp = tagData.substr(0, nIndex);
            
        if (strTemp.empty())
        {
            errmsg = "Received invalid TagGuid from Master Node. Hence this Vacp will NOT issue a Distributed Tag, but will issue a Local Tag";
            DebugPrintf("\n%s \n", errmsg.c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, errmsg, ERROR_RECEIVED_INVALID_TAG_GUID_FROM_MASTER_NODE);
            ExitDistributedConsistencyMode();
            return;
        }
                
        RPC_STATUS rstatus = UuidFromString((RPC_CSTR)strTemp.c_str(), &m_CmdRequest.distributedTagGuid);

        if (rstatus != RPC_S_OK)
        {
            m_DistributedProtocolStatus = ERROR_FAILED_TO_RECEIVE_TAG_GUID_FROM_MASTER_NODE;
            errmsg = "UuidToString failed. Hence this Vacp will NOT issue a Distributed Tag, but will issue a Local Tag";
            DebugPrintf("\n%s \n", errmsg.c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, errmsg, m_DistributedProtocolStatus);
            ExitDistributedConsistencyMode();
            return;
        }

        nTemp = tagData.find(strUserTagsMarker);
        m_CmdRequest.distributedTagUniqueId = tagData.substr((nIndex + strUniqIdMarker.length()), nTemp - (nIndex + strUniqIdMarker.length()));
        if (m_CmdRequest.distributedTagUniqueId.empty())
        {
            errmsg = "Received invalid TagGuid from Master Node. Hence this Vacp will NOT issue a Distributed Tag, but will issue a Local Tag";
            DebugPrintf("\n%s \n", errmsg.c_str());
            AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, errmsg, ERROR_RECEIVED_INVALID_TAG_GUID_FROM_MASTER_NODE);
            ExitDistributedConsistencyMode();
            return;
        }

        strTemp.clear();
        strTemp = tagData.substr(nTemp + strUserTagsMarker.length(), tagData.npos);
        nIndex = 0;
        char*pStart = NULL;
        char* pIndex = (char*)strTemp.c_str();
        pStart = pIndex;
        int nCounter = 0;
        while (*pIndex != '\0')
        {
            if (*pIndex == ';')
            {
                if (nCounter == 0)
                {
                    pIndex++;
                    pStart = pIndex;
                    continue;
                }
                else if (nCounter != 0)
                {
                    unsigned long pbufSize;

                    INM_SAFE_ARITHMETIC(pbufSize = (InmSafeInt<unsigned long>::Type(nCounter) + 1), INMAGE_EX(nCounter))

                    char* pBuf = new (std::nothrow) char[pbufSize];

                    if (pBuf == NULL)
                    {
                        m_DistributedProtocolStatus = ERROR_FAILED_TO_RECEIVE_TAG_GUID_FROM_MASTER_NODE;
                        std::stringstream ss;
                        ss << "DistributedTagGuidPhase: failed to allocate " << pbufSize << " bytes of memory. Hence this Vacp will NOT issue a Distributed Tag, but will issue a Local Tag";
                        DebugPrintf("\n%s \n", ss.str().c_str());
                        AddToMultivmFailMsgs(VACP_MULTIVM_FAILED, ss.str(), m_DistributedProtocolStatus);
                        ExitDistributedConsistencyMode();
                        return;
                    }

                    memset((void *)pBuf, 0x00,pbufSize);
                    inm_memcpy_s(pBuf, pbufSize, pStart, nCounter);
                    m_CmdRequest.vDistributedBookmarkEvents.push_back(pBuf);
                    pStart = pIndex;
                    nCounter = 0;
                    continue;
                }
            }
            pIndex++;
            nCounter++;
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_CLIENT_TAG_GUID_COMPLETED;
    }
            
    return;
}

HRESULT Consistency::PreparePhase()
{
    HRESULT hr = S_OK;

    std::string errmsg;
    hr = GenerateDrivesMappingFile(errmsg);
    if (hr != S_OK)
    {
        std::stringstream ss;
        ss << "PreparePhase failed with error: " << errmsg;
        DebugPrintf("%s.\n", ss.str().c_str());
        AddToVacpFailMsgs(VACP_E_PREPARE_PHASE_FAILED, ss.str(), VACP_E_PREPARE_PHASE_FAILED);
        return hr;
    }

    if (!m_bDistributedVacp)
    {
        return hr;
    }

    if (g_bThisIsMasterNode)
    {
        if (m_pCommunicator->GetCoordNodeCount() <= 0)
        {
            ExitDistributedConsistencyMode();
            return hr;
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
            return hr;
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
            return hr;
        }

        m_DistributedProtocolState = COMMUNICATOR_STATE_CLIENT_PREPARE_CMD_COMPLETED;
    }

    return hr;
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

HRESULT CrashConsistency::CreateIoBarrier(const ACSRequest_t &Request)
{
    HRESULT hr = S_OK;

    DebugPrintf("\n\nCreating the IO barrier in disk driver ...\n");

    DWORD   IoctlCmd = IOCTL_INMAGE_HOLD_WRITES;
    DWORD dwReturn = 0;
    ULONG ulBytesCopied = sizeof(CRASH_CONSISTENT_HOLDWRITE_INPUT);
    
    char * ioctlBuffer = (char *)malloc(ulBytesCopied);

    if (ioctlBuffer == NULL)
    {
        std::stringstream ss;
        ss << "CreateIoBarrier: failed to allocate " << ulBytesCopied << " bytes.";
        m_vacpLastError.m_errMsg = ss.str();
        m_vacpLastError.m_errorCode = VACP_MEMORY_ALLOC_FAILURE;
        DebugPrintf("%s\n", ss.str(), ulBytesCopied);
        return VACP_MEMORY_ALLOC_FAILURE;
    }

    CRASH_CONSISTENT_HOLDWRITE_INPUT *freezeInput = (CRASH_CONSISTENT_HOLDWRITE_INPUT *)ioctlBuffer;

    inm_memcpy_s(freezeInput->TagContext, ulBytesCopied, m_strContextGuid.c_str(), GUID_SIZE_IN_CHARS);
    
    freezeInput->ulFlags = TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS;
    

    LONG ioBarrierTimeout = VACP_IOBARRIER_TIMEOUT;

    VacpConfigPtr pVacpConf = VacpConfig::getInstance();
    if (pVacpConf.get() != NULL)
    {
        if (pVacpConf->getParamValue(VACP_CONF_PARAM_IO_BARRIER_TIMEOUT, ioBarrierTimeout))
        {
            if (ioBarrierTimeout < VACP_IOBARRIER_TIMEOUT)
            {
                DebugPrintf("\nIO Barrier Timeout %ld ms is less than allowed minimum. Setting to default value %d ms.\n",
                    ioBarrierTimeout, VACP_IOBARRIER_TIMEOUT);

                ioBarrierTimeout = VACP_IOBARRIER_TIMEOUT;
            }
            else if (ioBarrierTimeout > VACP_IOBARRIER_MAX_TIMEOUT)
            {
                DebugPrintf("\nIO Barrier Timeout %ld ms is more than allowed maximum. Setting to default maximum %d ms.\n",
                    ioBarrierTimeout, VACP_IOBARRIER_MAX_TIMEOUT);

                ioBarrierTimeout = VACP_IOBARRIER_MAX_TIMEOUT;
            }
            else
                DebugPrintf("\nIO Barrier Timeout is set to custom value of %ld ms.\n",
                                ioBarrierTimeout);
        }
    }

    freezeInput->ulTimeout = (ULONG)ioBarrierTimeout;

    if (m_bDistributedVacp && (DISKFLT_MV3_TAG_PRECHECK_SUPPORT == (g_inmFltDriverVersion.ulDrMinorVersion3 & DISKFLT_MV3_TAG_PRECHECK_SUPPORT)))
    {
        unsigned char *strGuid;
        RPC_STATUS rstatus = UuidToString(&Request.distributedTagGuid, (unsigned char**)&strGuid);
        if (rstatus != RPC_S_OK)
        {
            hr = HRESULT_FROM_WIN32(rstatus);
            m_vacpLastError.m_errMsg = "CreateIoBarrier: UuidToString failed";
            m_vacpLastError.m_errorCode = hr;
            DebugPrintf("%s, status = 0x%x\n", m_vacpLastError.m_errMsg.c_str(), hr);
            return hr;
        }
        inm_memcpy_s(freezeInput->TagMarkerGUID, GUID_SIZE_IN_CHARS, strGuid, GUID_SIZE_IN_CHARS);
        RpcStringFree(&strGuid);
        strGuid = NULL;
    }
    
    DWORD startTime = GetTickCount();
    while (true)
    {
        m_perfCounter.StartCounter("IoBarrier");
        m_perfCounter.StartCounter("CreateIoBarrier");

        BOOL bResult = DeviceIoControl(
            ghControlDevice,
            (DWORD)IoctlCmd,
            ioctlBuffer,
            ulBytesCopied,
            (PVOID)NULL,
            0,
            &dwReturn,
            &gOverLapBarrierIO);

        DWORD dwShowErr = GetLastError();

        m_perfCounter.EndCounter("CreateIoBarrier");

        if (!bResult)
        {
            if (ERROR_IO_PENDING == dwShowErr)
            {
                XDebugPrintf("IOCTL_INMAGE_HOLD_WRITES: DeviceIoControl returned pending %d.\n", dwShowErr);
                hr = S_OK;
                break;
            }
            else if (ERROR_BUSY == dwShowErr)
            {
                DWORD currTime = GetTickCount();
                if (currTime > startTime)
                {
                    DWORD diffTime = currTime - startTime;
                    if (diffTime < VACP_IOBARRIER_MAX_RETRY_TIME)
                    {
                        Sleep(VACP_IOBARRIER_RETRY_WAIT_TIME);
                        continue;
                    }

                    DebugPrintf("IOCTL_INMAGE_HOLD_WRITES: DeviceIoControl returned %d, Bytes returned = %d, elapsed time %d\n", dwShowErr, dwReturn, diffTime);
                }
                else
                {
                    // GetTickCout wrapped we may wait additionally for
                    // another MAX_RETRY_TIME
                    startTime = currTime;
                    Sleep(VACP_IOBARRIER_RETRY_WAIT_TIME);
                    continue;
                }
            }
            else
            {
                DebugPrintf("IOCTL_INMAGE_HOLD_WRITES: DeviceIoControl failed with error = %d, BytesReturn = %d\n", dwShowErr, dwReturn);
            }
            if (dwShowErr == 22)
            {
                hr = VACP_E_DRIVER_IN_NON_DATA_MODE;
                m_vacpLastError.m_errMsg = "IOCTL_INMAGE_HOLD_WRITES failed";
                m_vacpLastError.m_errorCode = hr;
            }
            else
            {
                hr = VACP_BARRIER_HOLD_FAILED;
                m_vacpLastError.m_errMsg = "IOCTL_INMAGE_HOLD_WRITES failed";
                m_vacpLastError.m_errorCode = hr;
            }
            break;
        }
        else
        {
            hr = VACP_BARRIER_HOLD_FAILED;
            std::stringstream ss;
            ss << "IOCTL_INMAGE_HOLD_WRITES failed: DeviceIoControl completed without pending, BytesReturn = " << dwReturn;
            m_vacpLastError.m_errMsg = ss.str();
            m_vacpLastError.m_errorCode = hr;
            DebugPrintf("%s\n", ss.str().c_str());
            break;
        }
    }

    if (ioctlBuffer != NULL)
        free(ioctlBuffer);

    return hr;
}

HRESULT CrashConsistency::ReleaseIoBarrier()
{
    HRESULT hr = S_OK;

    DebugPrintf("\n\nReleasing the IO barrier in disk driver ...\n");

    DWORD   IoctlCmd = IOCTL_INMAGE_RELEASE_WRITES;
    DWORD dwReturn = 0;
    ULONG ulBytesCopied = sizeof(CRASH_CONSISTENT_INPUT);
    
    OVERLAPPED overLapReleaseBarrier = { 0, 0, 0, 0, NULL };
    overLapReleaseBarrier.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (overLapReleaseBarrier.hEvent == NULL)
    {
        DWORD err = GetLastError();
        std::stringstream ss;
        ss << "CreateEvent failed with " << err;
        m_vacpLastError.m_errMsg = ss.str();
        DebugPrintf(" %s\n", ss.str().c_str());
        return VACP_CREATE_EVENT_FAILED;
    }

    char * ioctlBuffer = (char *)malloc(ulBytesCopied);

    if (ioctlBuffer == NULL)
    {
        CloseHandle(overLapReleaseBarrier.hEvent);
        return VACP_MEMORY_ALLOC_FAILURE;
    }

    memset((void*)ioctlBuffer, 0x00, ulBytesCopied);

    CRASH_CONSISTENT_INPUT *releaseInput = (CRASH_CONSISTENT_INPUT *)ioctlBuffer;

    inm_memcpy_s(releaseInput->TagContext, ulBytesCopied, m_strContextGuid.c_str(), GUID_SIZE_IN_CHARS);

    m_perfCounter.StartCounter("ReleaseIoBarrier");

    BOOL bResult = DeviceIoControl(
        ghControlDevice,
        (DWORD)IoctlCmd,
        ioctlBuffer,
        ulBytesCopied,
        (PVOID)NULL,
        0,
        &dwReturn,
        &overLapReleaseBarrier);

    DWORD dwShowErr = GetLastError();

    m_perfCounter.EndCounter("ReleaseIoBarrier");
    m_perfCounter.EndCounter("IoBarrier");

    if (!bResult)
    {
        DebugPrintf("IOCTL_INMAGE_RELEASE_WRITES: DeviceIoControl returned %d, Bytes returned = %d\n", dwShowErr, dwReturn);
        if (dwShowErr == 22)
        {
            hr = VACP_E_DRIVER_IN_NON_DATA_MODE;
        }
        else
        {
            hr = VACP_BARRIER_RELEASE_FAILED;
        }

        if (ERROR_IO_PENDING == dwShowErr)
        {
            std::stringstream ss;
            if (GetOverlappedResult(ghControlDevice, &overLapReleaseBarrier, &dwReturn, true))
            {
                ss << "IOCTL_INMAGE_RELEASE_WRITES: GetOverlappedResult succeeded and returned = " << dwReturn;
            }
            else
            {
                DWORD irpStatus = (DWORD)overLapReleaseBarrier.Internal;
                ss << "IOCTL_INMAGE_RELEASE_WRITES: GetOverlappedResult failed. status is " << irpStatus;
            }
            m_vacpLastError.m_errMsg = ss.str();
            DebugPrintf("%s.\n", ss.str());
        }
    }
    else
    {
        hr = S_OK;
        XDebugPrintf("IOCTL_INMAGE_RELEASE_WRITES: Succeeded, bytes returned = %d\n", dwReturn);
    }

    if (hr == S_OK)
        DebugPrintf("\nReleased IO barrier in disk driver.\n");

    CloseHandle(overLapReleaseBarrier.hEvent);

    if (ioctlBuffer != NULL)
        free(ioctlBuffer);

    return hr;
}


HRESULT Consistency::CommitRevokeTags(ACSRequest_t &Request)
{
    HRESULT hr = S_OK;
    DWORD dwReturn = 0;
    char * ioctlBuffer = NULL;
    PVOID pTagOutputData = NULL;
    std::string errmsg;

    bool commit = ((m_status & CONSISTENCY_STATUS_RESUME_SUCCESS) == CONSISTENCY_STATUS_RESUME_SUCCESS) ? true : false;
    
    // if the resume command is not received, should revoke the tag
    if ( (m_DistributedProtocolStatus == ERROR_FAILED_TO_GET_RESUME_CMD) ||
         (m_DistributedProtocolStatus == ERROR_TIMEOUT_WAITING_FOR_INSERT_TAG_ACK) )
        commit = false;
    
    char * msg = commit ? "Commit" : "Revoke";

    DebugPrintf("\n\n%s the tags with the driver ...\n", msg);

    if (!commit)
        m_status &= ~CONSISTENCY_STATUS_INSERT_TAG_SUCCESS;

    
    ULONG ulBytesCopied = sizeof(CRASH_CONSISTENT_INPUT);
    DWORD IoctlCmd = IS_CRASH_TAG(m_TagType) ? IOCTL_INMAGE_COMMIT_DISTRIBUTED_CRASH_TAG : IOCTL_INMAGE_COMMIT_TAG_DISK;
    std::string strIoctlCmd = IS_CRASH_TAG(m_TagType) ? "IOCTL_INMAGE_COMMIT_DISTRIBUTED_CRASH_TAG" : "IOCTL_INMAGE_COMMIT_TAG_DISK";

    OVERLAPPED overLapCommitTag = { 0, 0, 0, 0, NULL };
    overLapCommitTag.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (overLapCommitTag.hEvent == NULL)
    {
        DebugPrintf(" CreateEvent failed with %d\n", GetLastError());
        return E_FAIL;
    }

    
    do {

        char * ioctlBuffer = (char *)malloc(ulBytesCopied);

        if (ioctlBuffer == NULL)
        {
            hr = E_FAIL;
            std::stringstream ss;
            ss << "CommitRevokeTags: failed to allocate " << ulBytesCopied << " bytes.";
            DebugPrintf("%s: %s\n", strIoctlCmd.c_str(), ss.str().c_str());
            m_vacpLastError.m_errMsg = ss.str();
            m_vacpLastError.m_errorCode = hr;
            break;
        }

        memset((void*)ioctlBuffer, 0x00, ulBytesCopied);

        PCRASH_CONSISTENT_INPUT commitInput = (PCRASH_CONSISTENT_INPUT)ioctlBuffer;
        inm_memcpy_s(commitInput->TagContext, ulBytesCopied, m_strContextGuid.c_str(), GUID_SIZE_IN_CHARS);
        commitInput->ulFlags = commit ? TAG_INFO_FLAGS_COMMIT_TAG : 0;

        ULONG   ulOutputLength = IS_CRASH_TAG(m_TagType) ? 0 : SIZE_OF_TAG_DISK_STATUS_OUTPUT_DATA(Request.vProtectedDisks.size());

        DebugPrintf("CommitRevokeTags: Output Data Length %lu\n", ulOutputLength);

        PVOID pTagOutputData = NULL;
        if (ulOutputLength)
        {
            pTagOutputData = malloc(ulOutputLength);
            if (pTagOutputData == NULL)
            {
                hr = E_FAIL;
                std::stringstream ss;
                ss << "CommitRevokeTags: failed to allocate " << ulOutputLength << " bytes.";
                DebugPrintf("%s: %s\n", strIoctlCmd.c_str(), ss.str().c_str());
                m_vacpLastError.m_errMsg = ss.str();
                m_vacpLastError.m_errorCode = hr;
                break;
            }

            memset(pTagOutputData, 0x00, ulOutputLength);

            PTAG_DISK_STATUS_OUTPUT pTagDiskOutput = (PTAG_DISK_STATUS_OUTPUT)pTagOutputData;
            pTagDiskOutput->ulNumDisks = Request.vProtectedDisks.size();
        }

        m_perfCounter.StartCounter("CommitRevoke");

        BOOL bResult = DeviceIoControl(
            ghControlDevice,
            (DWORD)IoctlCmd,
            ioctlBuffer,
            ulBytesCopied,
            pTagOutputData,
            ulOutputLength,
            &dwReturn,
            &overLapCommitTag);

        DWORD dwShowErr = GetLastError();
        m_perfCounter.EndCounter("CommitRevoke");
        m_perfCounter.EndCounter("DrainBarrier");

        if (!bResult)
        {
            DebugPrintf("%s: DeviceIoControl returned %d, Bytes returned = %d\n", strIoctlCmd.c_str(), dwShowErr, dwReturn);
            hr = E_FAIL;

            if (ERROR_IO_PENDING == dwShowErr)
            {
                if (GetOverlappedResult(ghControlDevice, &overLapCommitTag, &dwReturn, true))
                {
                    DebugPrintf("CommitRevokeTags: GetOverlappedResult succeeded and returned = 0x%x\n", dwReturn);
                }
                else
                {
                    DWORD irpStatus = (DWORD)overLapCommitTag.Internal;
                    errmsg = "CommitRevokeTags: GetOverlappedResult failed.";
                    DebugPrintf("%s status is 0x%x.\n", errmsg.c_str(), irpStatus);
                    m_vacpLastError.m_errMsg = errmsg;
                    m_vacpLastError.m_errorCode = irpStatus;
                }
            }
            else
            {
                m_vacpLastError.m_errMsg = "CommitRecoverTags: " + std::string(msg) + " failed.";
                m_vacpLastError.m_errorCode = dwShowErr;
                DebugPrintf("%s status is 0x%x.\n", m_vacpLastError.m_errMsg.c_str(), m_vacpLastError.m_errorCode);
            }
        }
        else
        {
            XDebugPrintf("%s: Succeeded, Bytes returned = %d\n", strIoctlCmd.c_str(), dwReturn);
            hr = S_OK;
        }
        
        if ((dwReturn >= sizeof(TAG_DISK_STATUS_OUTPUT)) && !IS_CRASH_TAG(m_TagType))
        {
            PTAG_DISK_STATUS_OUTPUT pOutput = (PTAG_DISK_STATUS_OUTPUT)pTagOutputData;

            if (pOutput->ulNumDisks != Request.vProtectedDisks.size())
            {
                DebugPrintf("Tag status is returned for %lu disks out of %u issued disks.\n",
                    pOutput->ulNumDisks,
                    Request.vProtectedDisks.size());
            }
            else
            {
                int numTaggedDisks = 0;
                std::set<std::string>::iterator iter = Request.vProtectedDisks.begin();
                for (int i = 0; i < Request.vProtectedDisks.size(); i++, iter++)
                {
                    DebugPrintf("Tag status for disk %s is 0x%x.\n", iter->c_str(), pOutput->TagStatus[i]);
                    if (pOutput->TagStatus[i] == 0)
                        numTaggedDisks++;
                }

                m_tagTelInfo.Add(TagInsertKey, boost::lexical_cast<std::string>(numTaggedDisks));
                DebugPrintf("Tag is inserted for %d disks.\n", numTaggedDisks);
            }
        }

        ULONGLONG TagCommitTime = GetTimeInSecSinceEpoch1970();
        m_tagTelInfo.Add(TagCommitTimeKey, boost::lexical_cast<std::string>(TagCommitTime));
        DebugPrintf("TagCommitTime: %lld \n", TagCommitTime);
        
    } while (false);

    if (!commit)
        hr = E_FAIL;

    if (overLapCommitTag.hEvent != INVALID_HANDLE_VALUE)
    {
        CloseHandle(overLapCommitTag.hEvent);
        overLapCommitTag.hEvent = INVALID_HANDLE_VALUE;
    }

    if (ioctlBuffer != NULL)
        free(ioctlBuffer);

    if (pTagOutputData != NULL)
        free(pTagOutputData);

    return hr;
}

HRESULT Consistency::CreateContextGuid()
{
    HRESULT hr = S_OK;
    std::string errmsg;

    GUID contextGuid;
    hr = CoCreateGuid(&contextGuid);

    if (hr != S_OK)
    {
        errmsg = "CreateContextGuid: CoCreateGuid failed.";
        DebugPrintf("%s. Error Code 0x%x\n", errmsg.c_str(), hr);
        AddToVacpFailMsgs(VACP_E_VACP_MODULE_FAIL, errmsg, hr);
        return hr;
    }
    
    char *uuidBuffer;
    RPC_STATUS rstatus = UuidToString(&contextGuid, (RPC_CSTR*)&uuidBuffer);

    if (rstatus != RPC_S_OK)
    {
        hr = HRESULT_FROM_WIN32(rstatus);
        errmsg = "CreateContextGuid: UuidToString failed.";
        DebugPrintf("%s. Error Code 0x%x\n", errmsg.c_str(), hr);
        AddToVacpFailMsgs(VACP_E_VACP_MODULE_FAIL, errmsg, hr);
        return hr;
    }

    m_strContextGuid.assign(uuidBuffer);
    RpcStringFree((RPC_CSTR*)&uuidBuffer);

    return hr;
}

HRESULT driverPreCheck(ACSRequest_t &ACSRequest)
{
    boost::unique_lock<boost::mutex> lock(g_driverPreCheckLock);
    HRESULT hr = S_OK;
    if (DISKFLT_MV3_TAG_PRECHECK_SUPPORT != (g_inmFltDriverVersion.ulDrMinorVersion3 & DISKFLT_MV3_TAG_PRECHECK_SUPPORT))
    {
        hr = S_OK;
        return hr;
    }

    if (ACSRequest.vDisks.empty())
    {
        std::stringstream ss;
        ss << "Skipping driverPreCheck: No disks found.";
        DebugPrintf("%s\n", ss.str().c_str());
        AddToVacpFailMsgs(VACP_MODULE_INFO, ss.str(), VACP_MODULE_INFO, ACSRequest.TagType);
        hr = S_OK;
        return hr;
    }
    
    // Calculate the total input buffer size required
    ULONG GuidSize = GUID_SIZE_IN_CHARS * sizeof(WCHAR);
    ULONG tagBufferLen = sizeof(TAG_PRECHECK_INPUT)+(ACSRequest.vDisks.size() * GuidSize);
    char *pTagDataBuffer = new (std::nothrow) char[tagBufferLen];
    if (pTagDataBuffer == NULL)
    {
        std::stringstream ss;
        ss << "driverPreCheck: Failed to allocate " << tagBufferLen << " bytes.";
        DebugPrintf("%s\n", ss.str().c_str());
        hr = VACP_MEMORY_ALLOC_FAILURE;
        AddToVacpFailMsgs(VACP_PRECHECK_FAILED, ss.str(), hr, ACSRequest.TagType);
        return hr;
    }
    memset((void*)pTagDataBuffer, 0x00, tagBufferLen);

    PTAG_PRECHECK_INPUT pPrecheckInput = (PTAG_PRECHECK_INPUT)pTagDataBuffer;
    
    unsigned char *strGuid = NULL;
    RPC_STATUS rstatus;
    if (ACSRequest.bDistributed)
    {
        rstatus = UuidToString(&ACSRequest.distributedTagGuid, (unsigned char**)&strGuid);
    }
    else
    {
        rstatus = UuidToString(&ACSRequest.localTagGuid, (unsigned char**)&strGuid);
    }
    if (rstatus != RPC_S_OK)
    {
        hr = HRESULT_FROM_WIN32(rstatus);
        std::string errmsg = "driverPreCheck: UuidToString failed";
        DebugPrintf("%s, status = 0x%x\n", errmsg.c_str(), hr);
        AddToVacpFailMsgs(VACP_PRECHECK_FAILED, errmsg, hr, ACSRequest.TagType);
        return hr;
    }
    
    inm_memcpy_s(pPrecheckInput->TagMarkerGUID, tagBufferLen, strGuid, GUID_SIZE_IN_CHARS);
    RpcStringFree(&strGuid);
    strGuid = NULL;

    if (IS_CRASH_TAG(ACSRequest.TagType))
    {
        pPrecheckInput->ulFlags = ACSRequest.bDistributed ? TAG_INFO_FLAGS_DISTRIBUTED_CRASH : TAG_INFO_FLAGS_LOCAL_CRASH;
    }
    else
    {
        pPrecheckInput->ulFlags = ACSRequest.bDistributed ? TAG_INFO_FLAGS_DISTRIBUTED_APP : TAG_INFO_FLAGS_LOCAL_APP;
    }

    if (IS_BASELINE_TAG(ACSRequest.TagType))
    {
        pPrecheckInput->ulFlags |= TAG_INFO_FLAGS_BASELINE_APP;
    }

    pPrecheckInput->usNumDisks = ACSRequest.vDisks.size();

    // Update bytes copied
    unsigned long ulBytesCopied = offsetof(TAG_PRECHECK_INPUT, DiskIdList);

    std::set<std::string>::const_iterator diskIter = ACSRequest.vDisks.begin();
    for (diskIter; diskIter != ACSRequest.vDisks.end(); diskIter++)
    {
        std::wstring diskSignature;
        if (!ACSRequest.GetDiskGuidFromDiskName(*diskIter, diskSignature))
        {
            hr = VACP_PRECHECK_FAILED;
            std::stringstream ss;
            ss << "driverPreCheck: GetDiskGuidFromDiskName() FAILED for " << (*diskIter);
            DebugPrintf("%s\n", ss.str().c_str());
            AddToVacpFailMsgs(VACP_PRECHECK_FAILED, ss.str(), hr, ACSRequest.TagType);

            delete pTagDataBuffer;
            pTagDataBuffer = NULL;
            return hr;
        }

        unsigned short signatureSize = diskSignature.size() * sizeof(WCHAR);
        inm_memcpy_s(pTagDataBuffer + ulBytesCopied, (tagBufferLen - ulBytesCopied), diskSignature.c_str(), signatureSize);
        ulBytesCopied += GuidSize;
    }
    if (ulBytesCopied > tagBufferLen)
    {
        // If we are here then this is bug
        hr = VACP_PRECHECK_FAILED;
        std::stringstream ss;
        ss << "driverPreCheck: ulBytesCopied=" << ulBytesCopied << " exceeded tagBufferLen=" << tagBufferLen << ".";
        DebugPrintf("%s\n", ss.str().c_str());
        AddToVacpFailMsgs(VACP_PRECHECK_FAILED, ss.str(), hr, ACSRequest.TagType);
        return hr;
    }

    BOOL bResult = false;
    DWORD dwBytesReturned = 0;
    DWORD dwErr = 0;
    ULONG ulOutputLength = SIZE_OF_TAG_DISK_STATUS_OUTPUT_DATA(ACSRequest.vDisks.size());
    const string strIoctlCmd = "IOCTL_INMAGE_TAG_PRECHECK";
    PVOID pTagOutputData = malloc(ulOutputLength);
    if (pTagOutputData == NULL)
    {
        hr = VACP_MEMORY_ALLOC_FAILURE;
        std::stringstream ss;
        ss << "driverPreCheck: Failed to allocate " << ulOutputLength << " bytes.";
        DebugPrintf("%s\n", ss.str().c_str());
        AddToVacpFailMsgs(VACP_PRECHECK_FAILED, ss.str(), hr, ACSRequest.TagType);

        delete pTagDataBuffer;
        pTagDataBuffer = NULL;
        return hr;
    }

    const unsigned long IoctlCmd = IOCTL_INMAGE_TAG_PRECHECK;

    bResult = DeviceIoControl(
        ghControlDevice,
        (DWORD)IoctlCmd,
        pTagDataBuffer,
        tagBufferLen,
        pTagOutputData,
        ulOutputLength,
        &dwBytesReturned,
        NULL);

    if (!bResult)
    {
        dwErr = GetLastError();
        hr = HRESULT_FROM_WIN32(dwErr);
        std::stringstream ss;
        ss << "DeviceIoControl failed for " << strIoctlCmd << ". Status: " << hr;
        DebugPrintf("%s\n", ss.str().c_str());
        AddToVacpFailMsgs(VACP_PRECHECK_FAILED, ss.str(), hr, ACSRequest.TagType);
        return hr;
    }

    if (dwBytesReturned != ulOutputLength)
    {
        hr = VACP_PRECHECK_FAILED;
        std::stringstream ss;
        ss << "DeviceIoControl failed for " << strIoctlCmd << ". Ecpected OutputLength=" << ulOutputLength << ", BytesReturned=" << dwBytesReturned << "." << hr;
        DebugPrintf("%s\n", ss.str().c_str());
        AddToVacpFailMsgs(VACP_PRECHECK_FAILED, ss.str(), hr, ACSRequest.TagType);
    }
    
    PTAG_DISK_STATUS_OUTPUT pTagDiskStatusOutput = (PTAG_DISK_STATUS_OUTPUT)pTagOutputData;

    if (pTagDiskStatusOutput->ulNumDisks != ACSRequest.vDisks.size())
    {
        hr = VACP_PRECHECK_FAILED;
        std::stringstream ss;
        ss << strIoctlCmd << "driverPreCheck: IOCTL_INMAGE_TAG_PRECHECK o/p TAG_DISK_STATUS_OUTPUT->ulNumDisks not matching ProtectedDisks size " << ACSRequest.vProtectedDisks.size();
        DebugPrintf("%s\n", ss.str().c_str());
        AddToVacpFailMsgs(VACP_PRECHECK_FAILED, ss.str(), hr, ACSRequest.TagType);
        return hr;
    }

    std::stringstream ssDataWosDisks;
    std::stringstream ssNonDataWosDisks;
    std::stringstream ssInvalidGUIDDisks;
    std::stringstream ssFilteringStoppedDisks;

    std::stringstream ssOther;
    diskIter = ACSRequest.vDisks.begin();
    for (int i = 0; i < pTagDiskStatusOutput->ulNumDisks; i++, diskIter++)
    {
        if (pTagDiskStatusOutput->TagStatus[i] == etTagStatus::ecTagStatusTagDataWOS)
        {
            ACSRequest.vProtectedDisks.insert((*diskIter));
            ssDataWosDisks << *diskIter;
            ssDataWosDisks << "=";
            ssDataWosDisks << pTagDiskStatusOutput->TagStatus[i];
            ssDataWosDisks << ", ";
        }
        else if (pTagDiskStatusOutput->TagStatus[i] == etTagStatus::ecTagStatusTagNonDataWOS)
        {
            ACSRequest.vProtectedDisks.insert((*diskIter));
            ssNonDataWosDisks << *diskIter;
            ssNonDataWosDisks << "=";
            ssNonDataWosDisks << pTagDiskStatusOutput->TagStatus[i];
            ssNonDataWosDisks << ", ";
        }
        else if (pTagDiskStatusOutput->TagStatus[i] == etTagStatus::ecTagStatusInvalidGUID)
        {
            ssInvalidGUIDDisks << *diskIter;
            ssInvalidGUIDDisks << "=";
            ssInvalidGUIDDisks << pTagDiskStatusOutput->TagStatus[i];
            ssInvalidGUIDDisks << ", ";
        }
        else if (pTagDiskStatusOutput->TagStatus[i] == etTagStatus::ecTagStatusFilteringStopped)
        {
            ssFilteringStoppedDisks << *diskIter;
            ssFilteringStoppedDisks << "=";
            ssFilteringStoppedDisks << pTagDiskStatusOutput->TagStatus[i];
            ssFilteringStoppedDisks << ", ";
        }
        else
        {
            ssOther << *diskIter;
            ssOther << "=InvalidStatus ";
            ssOther << pTagDiskStatusOutput->TagStatus[i];
            ssOther << ", ";
        }
    }

    if (ssFilteringStoppedDisks.tellp())
    {
        std::stringstream ss;
        ss << "Following disks are in FilteringStopped state [" << ssFilteringStoppedDisks.str() << "]";
        DebugPrintf("%s\n", ss.str().c_str());
        AddToVacpFailMsgs(VACP_MODULE_INFO, ss.str(), VACP_MODULE_INFO, ACSRequest.TagType);
    }

    if (ssInvalidGUIDDisks.tellp())
    {
        std::stringstream ss;
        ss << "Following disks are in InvalidGUID state [" << ssInvalidGUIDDisks.str() << "]";
        DebugPrintf("%s\n", ss.str().c_str());
        AddToVacpFailMsgs(VACP_MODULE_INFO, ss.str(), VACP_MODULE_INFO, ACSRequest.TagType);
    }

    if (ssOther.tellp())
    {
        std::stringstream ss;
        ss << "Following disks are in Unknown state [" << ssOther.str() << "]";
        DebugPrintf("%s\n", ss.str().c_str());
        AddToVacpFailMsgs(VACP_MODULE_INFO, ss.str(), VACP_MODULE_INFO, ACSRequest.TagType);
    }

    if (ssNonDataWosDisks.tellp())
    {
        std::stringstream ss;
        ss << "Following disks are in NON WOS mode [" << ssNonDataWosDisks.str() << "]";
        DebugPrintf("%s\n", ss.str().c_str());
        if (!IS_CRASH_TAG(ACSRequest.TagType))
        {
            hr = VACP_E_DRIVER_IN_NON_DATA_MODE;
            AddToVacpFailMsgs(VACP_PRECHECK_FAILED, ss.str(), hr, ACSRequest.TagType);
            return hr;
        }
        AddToVacpFailMsgs(VACP_MODULE_INFO, ss.str(), VACP_MODULE_INFO, ACSRequest.TagType);
    }

    if (ACSRequest.vDisks.size() && (0 == ACSRequest.vProtectedDisks.size()))
    {
        hr = VACP_NO_PROTECTED_VOLUMES;

        std::stringstream ss;
        ss << "No protected disks found.\n";
        if (ssInvalidGUIDDisks.tellp())      ss << "\t InvalidGUIDDisks: [" << ssInvalidGUIDDisks.str() << "]\n";
        if (ssFilteringStoppedDisks.tellp()) ss << "\t FilteringStoppedDisks: [" << ssFilteringStoppedDisks.str() << "]\n";
        if (ssOther.tellp())                 ss << "\t Unknown Status Disks: [" << ssOther.str() << "]";

        DebugPrintf("%s.\n", ss.str().c_str());
        AddToVacpFailMsgs(VACP_PRECHECK_FAILED, ss.str(), hr, ACSRequest.TagType);
        return hr;
    }

    if (ssDataWosDisks.tellp())
    {
        std::stringstream ss;
        ss << "Following disks are in WOS mode [" << ssDataWosDisks.str() << "]";
        DebugPrintf("%s\n", ss.str().c_str());
    }

    if (pTagOutputData != NULL)
    {
        free(pTagOutputData);
        pTagOutputData = NULL;
    }
    if (pTagDataBuffer != NULL)
    {
        delete pTagDataBuffer;
        pTagDataBuffer = NULL;
    }
    return hr;
}

void Consistency::UpdateStartTimeTelInfo()
{
    std::string universalTime = boost::lexical_cast<std::string>(GetTimeInSecSinceEpoch1970());
    m_tagTelInfo.Add(VacpSysTimeKey, universalTime);
    m_tagTelInfo.Add(VacpStartTimeKey, universalTime);
    DebugPrintf("SysTimeKey %s .\n", universalTime.c_str());
    DebugPrintf("VacpStartTime %s .\n", universalTime.c_str());
}

void Consistency::IRModeStamp()
{
    HRESULT hr = VACP_ONE_OR_MORE_DISK_IN_IR;
    std::string errmsg = "One or more disk in IR/resync - skipping consistency.";
    AddToVacpFailMsgs(VACP_MODULE_INFO, errmsg, hr);
    DebugPrintf("%s, hr = 0x%x\n", errmsg.c_str(), hr);
    Conclude(hr);
}

void CrashConsistency::Process()
{
    UpdateStartTimeTelInfo();

    m_tagTelInfo.Add(VacpCommand, g_strCommandline);
    DebugPrintf("\nCommand Line: %s\n", g_strCommandline.c_str());

    DebugPrintf("%s %s .\n", TagType.c_str(), CRASH_TAG);
    m_tagTelInfo.Add(TagType, CRASH_TAG);

    m_vacpErrorInfo.clear();//Clear all the earlier run Health Issues
    HRESULT hr = S_OK;
    m_status = CONSISTENCY_STATUS_INITIALIZED;

    if (gbIRMode)
    {
        return IRModeStamp();
    }

    ACSRequestGenerator *gen = new (std::nothrow) ACSRequestGenerator();
    if (gen == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        std::string errmsg = "Failed to allocate ACSRequestGenerator.";
        AddToVacpFailMsgs(VACP_E_VACP_MODULE_FAIL, errmsg, hr);
        DebugPrintf("%s, hr = 0x%x\n", errmsg.c_str(), hr);
        return Conclude(VACP_MEMORY_ALLOC_FAILURE);
    }

    // Generate Hydration Tag if it is available
    // We have following window where system state can change
    //          100ms   for local crash consistency
    //          60s     for local app consistency
    //          120s    for distributed consistency
    //  During this window if boot drivers reg changes
    //  failover VM may not boot up.
    std::string hydrationTag;
    if (gbVToAMode)
    {
        hydrationTag = HydrationConfigurator::GetHydrationTag(g_strHydrationFilePath);
    }

    StartCommunicator();

    inm_log_to_buffer_start();
    // note: the inm_log_to_buffer_end() gets called in Conclude()
    // for both success and error cases. So it should not be called
    // within the Process() scope

    DistributedTagGuidPhase();

    hr = PreparePhase();
    if (hr != S_OK)
    {
        DebugPrintf("PreparePhase failed.\n");
        return Conclude(VACP_E_PREPARE_PHASE_FAILED);
    }

    hr = gen->GenerateACSRequest(m_CmdRequest, m_AcsRequest, CRASH);
    if (hr != S_OK)
    {
        DebugPrintf("GenerateACSRequest failed, hr = 0x%x\n",hr);
        return Conclude(hr);
    }

    m_AcsRequest.TagType = CRASH;
    m_AcsRequest.bDistributed = m_bDistributedVacp;

    // driver pre-check is not mandatory for crash consistency
    // however the driver requires it for telemetry 
    // just ignore if there are any errors.
    driverPreCheck(m_AcsRequest);

    m_VacpFailMsgsList.clear();

    hr = CreateContextGuid();
    if (hr != S_OK)
    {
        return Conclude(hr);
    }

    m_AcsRequest.m_contextGuid = m_strContextGuid;

    m_AcsRequest.m_hydrationTag = hydrationTag;

    m_status |= CONSISTENCY_STATUS_PREPARE_SUCCESS;

    PrepareAckPhase();

    QuiescePhase();

    bool bDistributedIoBarrierCreated = false;
    
    m_perfCounter.StartCounter("all driver phases");
    if (m_bDistributedVacp && m_IoBarrierRequired)
    {
        hr = CreateIoBarrier(m_AcsRequest);
        if (hr != S_OK)
        {
            AddToVacpFailMsgs(VACP_E_VACP_MODULE_FAIL, m_vacpLastError.m_errMsg, hr);
            return Conclude(hr);
        }

        bDistributedIoBarrierCreated = true;
    }

    m_status |= CONSISTENCY_STATUS_QUIESCE_SUCCESS;

    QuiesceAckPhase();

    TagPhase();

    if (bDistributedIoBarrierCreated && !m_bDistributedVacp)
    {
        // moved from distributed crash to local crash mode
        // need to release the barrier and let the local barrier+tag ioctl to succeed
        ReleaseIoBarrier();
        m_perfCounter.EndCounter("IoBarrier");

        bDistributedIoBarrierCreated = false;

        // change the context guid to indicate the switch
        hr = CreateContextGuid();
        if (hr != S_OK)
        {
            return Conclude(hr);
        }

        m_AcsRequest.m_contextGuid = m_strContextGuid;

        DebugPrintf("Issuing local tag...\n");
    }

    hr = gen->GenerateTagIdentifiers(m_AcsRequest);
    if (hr != S_OK)
    {
        std::string errmsg = "GenerateTagIdentifiers failed";
        DebugPrintf("%s, hr = %x0x \n", errmsg.c_str(), hr);
        AddToVacpFailMsgs(VACP_E_VACP_MODULE_FAIL, errmsg, hr);
        return Conclude(hr);
    }

    hr = gbUseDiskFilter ? SendTagsToDiskDriver(m_AcsRequest) : SendTagsToDriver(m_AcsRequest);
    if (hr != S_OK)
    {
        DebugPrintf("SendTagsToDriver failed, hr = 0x%x \n", hr);
        return Conclude(hr);
    }

    if (m_bDistributedVacp || !IS_CRASH_TAG(m_TagType))
        m_status |= CONSISTENCY_STATUS_INSERT_TAG_SUCCESS;
    else
        m_status |= CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS;


    TagAckPhase();

    ResumePhase();

    if (bDistributedIoBarrierCreated)
    {
        hr = ReleaseIoBarrier();

        if (hr != S_OK) 
        {
            AddToVacpFailMsgs(VACP_E_VACP_MODULE_FAIL, m_vacpLastError.m_errMsg, hr);
            return Conclude(hr);
        }
    }

    m_status |= CONSISTENCY_STATUS_RESUME_SUCCESS;

    ResumeAckPhase();

    // if it is a local tag, skip commit-revoke
    if ((m_status & CONSISTENCY_STATUS_INSERT_TAG_SUCCESS) == CONSISTENCY_STATUS_INSERT_TAG_SUCCESS)
    {
        hr = CommitRevokeTags(m_AcsRequest);
        if (hr != S_OK)
        {
            return Conclude(hr);
        }

        m_status |= CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS;
    }

    m_perfCounter.EndCounter("all driver phases");

    if (m_bDistributedVacp && m_IoBarrierRequired && bDistributedIoBarrierCreated)
    {
        if (HasOverlappedIoCompleted(&gOverLapBarrierIO))
        {
            XDebugPrintf("FILE %s: LINE %d, Overlapped IO completed after CommitTags()\n", __FILE__, __LINE__);
        }
        else
        {
            XDebugPrintf("FILE %s: LINE %d, Overlapped IO did not complete after CommitTags()\n", __FILE__, __LINE__);

            DWORD dwReturn = 0;
            if (GetOverlappedResult(ghControlDevice, &gOverLapBarrierIO, &dwReturn, true))
            {
                XDebugPrintf("ProcessCrashConsistency: GetOverlappedResult succeeded and returned = %d\n", dwReturn);
            }
            else
            {
                DWORD tagIrpStatus = (DWORD)gOverLapBarrierIO.Internal;
                DebugPrintf("ProcessCrashConsistency: GetOverlappedResult failed. Tag IOCTL status is %d after CommitTags()\n", tagIrpStatus);
            }

        }

    }

    if (gen != NULL)
        delete gen;

    if (m_bDistributedVacp)
    {
        if ((m_status & CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS) == CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS)
        {
            m_tagTelInfo.Add(MultivmTagKey, std::string());
            inm_printf("Exiting gracefully ...(Issued a DISTRIBUTED TAG on this Node.)\n");

            if (g_bThisIsMasterNode)
                m_pCommunicator->ShowDistributedVacpReport();

            return Conclude(0);
        }
        else
        {
            if (g_bThisIsMasterNode)
            {
                inm_printf("\nMaster Node has failed to issue the tag but it has facilitated other Client Nodes to issue a Distributed Tag.\n");

                m_pCommunicator->ShowDistributedVacpReport();
            }

            if (m_DistributedProtocolStatus)
                hr = m_DistributedProtocolStatus;
            else
                hr = E_FAIL;

            inm_printf("Concluding with status 0x%x.\n", hr);

            return Conclude(hr);
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
            if (m_DistributedProtocolStatus)
                hr = m_DistributedProtocolStatus;
            else
                hr = E_FAIL;

            inm_printf("Concluding with status 0x%x.\n", hr);
            return Conclude(hr);
        }
    }
}

void BaselineConsistency::Process()
{
    UpdateStartTimeTelInfo();

    DebugPrintf("\nCommand Line: %s\n", g_strCommandline.c_str());

    DebugPrintf("%s %s \n", TagType.c_str(), BASELINE_TAG);
    m_tagTelInfo.Add(TagType, BASELINE_TAG);

    HRESULT hr = S_OK;
    m_status = CONSISTENCY_STATUS_INITIALIZED;

    ACSRequestGenerator *gen = new (std::nothrow) ACSRequestGenerator();
    if (gen == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        std::string errmsg = "Failed to allocate ACSRequestGenerator.";
        AddToVacpFailMsgs(VACP_E_VACP_MODULE_FAIL, errmsg, hr);
        DebugPrintf("%s, hr = 0x%x\n", errmsg.c_str(), hr);
        return Conclude(VACP_MEMORY_ALLOC_FAILURE);
    }

    hr = gen->GenerateACSRequest(m_CmdRequest, m_AcsRequest, BASELINE);
    if (hr != S_OK)
    {
        DebugPrintf("GenerateACSRequest failed, hr = 0x%x\n", hr);
        return Conclude(hr);
    }

    m_AcsRequest.TagType = BASELINE;
    m_AcsRequest.bDistributed = m_bDistributedVacp;

    // driver pre-check is not mandatory for baseline consistency
    // however the driver requires it for telemetry
    // just ignore if there are any errors.
    driverPreCheck(m_AcsRequest);

    m_VacpFailMsgsList.clear();

    hr = CreateContextGuid();
    if (hr != S_OK)
    {
        return Conclude(hr);
    }

    m_AcsRequest.m_contextGuid = m_strContextGuid;

    m_perfCounter.StartCounter("all driver phases");

    hr = gen->GenerateTagIdentifiers(m_AcsRequest);
    if (hr != S_OK)
    {
        std::string errmsg = "GenerateTagIdentifiers failed";
        DebugPrintf("%s, hr = %x0x \n", errmsg.c_str(), hr);
        AddToVacpFailMsgs(VACP_E_VACP_MODULE_FAIL, errmsg, hr);
        return Conclude(hr);
    }

    hr = SendTagsToDiskDriver(m_AcsRequest);
    if (hr != S_OK)
    {
        DebugPrintf("SendTagsToDriver failed, hr = 0x%x \n", hr);
        return Conclude(hr);
    }

    m_status |= CONSISTENCY_STATUS_INSERT_TAG_SUCCESS;
    m_status |= CONSISTENCY_STATUS_RESUME_SUCCESS;

    hr = CommitRevokeTags(m_AcsRequest);
    if (hr != S_OK)
    {
        return Conclude(hr);
    }
    m_status |= CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS;

    m_perfCounter.EndCounter("all driver phases");

    if (gen != NULL)
        delete gen;

    m_tagTelInfo.Add(LocalTagKey, std::string());
    inm_printf("Exiting gracefully ...(Issued a LOCAL TAG)\n");
    return Conclude(0);
}

static void LogCallback(unsigned int logLevel, const char *msg)
{
    inm_printf(logLevel, msg);
}

void RunBaselineConsistency(const CLIRequest_t &CmdRequest)
{
    BaselineConsistency::CreateInstance(CmdRequest);

    BaselineConsistency::Get().Process();
}

void RunCrashConsistency(const CLIRequest_t &CmdRequest)
{
    CrashConsistency::CreateInstance(CmdRequest, !gbBaselineTag);
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

void RunAppConsistency(const CLIRequest_t &CmdRequest)
{
    AppConsistency::CreateInstance(CmdRequest);
    AppConsistency::Get().InitSchedule();

    if (gbParallelRun && (0 == AppConsistency::Get().GetConsistencyInterval()))
    {
        inm_printf("%s: not starting app consistency as interval in parallel run is 0.\n", __FUNCTION__);
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
            StopLongRunningInMageVssProvider(VSS_PROV_DELAYED_STOP_WAIT_TIME);

        if (gbParallelRun)
            g_Quit->Wait((g_bDistributedVacp && !g_bThisIsMasterNode) ? gnRemoteScheduleCheckInterval : gnParallelRunScheduleCheckInterval);

    } while (gbParallelRun && !g_Quit->IsSignalled());
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

void GetLastConsistencyScheduleTime(CLIRequest_t &CmdRequest)
{
    namespace bpt = boost::property_tree;
    bpt::ptree pt;

    try {
        if (!boost::filesystem::exists(CmdRequest.m_strScheduleFilePath))
            return;

        bpt::json_parser::read_json(CmdRequest.m_strScheduleFilePath, pt);

        CmdRequest.m_lastAppScheduleTime = pt.get<uint64_t>(LastAppConsistencyScheduleTime, 0);
        CmdRequest.m_lastCrashScheduleTime = pt.get<uint64_t>(LastCrashConsistencyScheduleTime, 0);
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
        __FUNCTION__, CmdRequest.m_lastAppScheduleTime, CmdRequest.m_lastCrashScheduleTime);

    return;
}

void PersistConsistencyScheduleTime(CLIRequest_t &CmdRequest,
        uint64_t currentPolicyRunTime)
{
    namespace bpt = boost::property_tree;
    static boost::mutex s_schedlueLock;
    uint64_t appSchedTime = 0, crashSchedTime = 0;
    
    boost::mutex::scoped_lock quard(s_schedlueLock);

    try {
        if (boost::filesystem::exists(CmdRequest.m_strScheduleFilePath))
        {
            bpt::ptree pt;
            bpt::json_parser::read_json(CmdRequest.m_strScheduleFilePath, pt);

            appSchedTime = pt.get<uint64_t>(LastAppConsistencyScheduleTime, 0);
            crashSchedTime = pt.get<uint64_t>(LastCrashConsistencyScheduleTime, 0);

            DebugPrintf("%s: LastAppConsistencyScheduleTime :%lld, LastCrashConsistencyScheduleTime : %lld\n",
                __FUNCTION__, appSchedTime, crashSchedTime);

            DebugPrintf("%s: update %s : %lld\n",
                __FUNCTION__,
                IS_CRASH_TAG(CmdRequest.TagType) ? "CrashConsistency" : "AppConsistency",
                currentPolicyRunTime);
        }

        bpt::ptree pt;
        if (!IS_CRASH_TAG(CmdRequest.TagType))
        {
            pt.put(LastAppConsistencyScheduleTime, currentPolicyRunTime);
            pt.put(LastCrashConsistencyScheduleTime, crashSchedTime);
        }
        else
        {
            pt.put(LastAppConsistencyScheduleTime, appSchedTime);
            pt.put(LastCrashConsistencyScheduleTime, currentPolicyRunTime);
        }

        bpt::json_parser::write_json(CmdRequest.m_strScheduleFilePath, pt);
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

ULONG _tmain(int argc, _TCHAR* argv[])
{
    TerminateOnHeapCorruption();
    init_inm_safe_c_apis();

    int nRepeat = 0;
    DWORD dwWait = 0;
    //To solve the memory alignment issue in IA64 platform
    SetErrorMode(SEM_NOALIGNMENTFAULTEXCEPT);

    std::string errmsg;

    Trace::Init("", LogLevelAlways, boost::bind(&LogCallback, _1, _2));

    HRESULT hr = S_OK;
    CLIRequest_t CmdRequest;

    GetCmdLineargsandProcessId();

    hr = ParseCommandLineArgs(argc, argv, CmdRequest);
    if (hr != S_OK)
        ExitVacp(VACP_INVALID_ARGUMENT, true);

    hr = InitAgentSettings(argv, CmdRequest);
    if (FAILED(hr))
        ExitVacp(VACP_CONF_PARAMS_NOT_SUPPLIED, true);
    
    if (gbParallelRun || IS_APP_TAG(CmdRequest.TagType))
    {
        if (FAILED(InitializeCom()))
            exit(VACP_COM_INIT_FAILED);
    }

    boost::shared_ptr<void> guardCom(static_cast<void*>(0), boost::bind(&UninitializeCom));

    std::string errMsg;

    if (g_bDistributedVacp)
    {
        GetIPAddressInAddrInfo(g_strLocalHostName, g_vLocalHostIps, errMsg);
        if (g_vLocalHostIps.empty())
        {
            DebugPrintf("\nError getting the IP Addresses of the machine %s, error: %s\n", g_strLocalHostName.c_str(), errMsg.c_str());
            exit(VACP_IP_ADDRESS_DISCOVERY_FAILED);
        }

        strset_t::iterator it = g_vLocalHostIps.begin();
        while (it != g_vLocalHostIps.end())
        {
            std::string ip = *it;
            if (std::string::npos == ip.find(':'))
                inm_printf("\tIPv4 address %s\n", ip.c_str());
            it++;
        }
    }

    //Create Event for Synchronous tag
    gOverLapIO.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    gOverLapTagIO.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    gOverLapRevokationIO.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    gOverLapBarrierIO.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    if ((gOverLapIO.hEvent == NULL) ||
        (gOverLapTagIO.hEvent == NULL) ||
        (gOverLapRevokationIO.hEvent == NULL) ||
        (gOverLapBarrierIO.hEvent == NULL))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        std::string errmsg = "CreateEvent failed.";
        inm_printf("%s\n", errmsg.c_str()); 
        ADD_TO_VACP_FAIL_MSGS(VACP_CREATE_EVENT_FAILED, errmsg, hr);
        ExitVacp(VACP_CREATE_EVENT_FAILED, true);
    }

    //check whether OS is above W2K3 SP1. 
    // it is requried since writer Instance name is available only for W2K3SP1 and above.
    OSVERSIONINFOEX osVersionInfo = { 0 };
    osVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    GetVersionEx((LPOSVERSIONINFO)&osVersionInfo);
    gbIsOSW2K3SP1andAbove = 
        ((osVersionInfo.dwMajorVersion > 5) ||
            ((osVersionInfo.dwMajorVersion >= 5) &&
            (osVersionInfo.dwMinorVersion >= 2) &&
            (osVersionInfo.wServicePackMajor >= 1)));

    if (!gbVerify)
    {
        ghControlDevice = CreateFile(
            INMAGE_DISK_FILTER_DOS_DEVICE_NAME,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            NULL);

        if (ghControlDevice == INVALID_HANDLE_VALUE)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            errmsg = "Couldn't get handle to filter device.";
            inm_printf("\nERROR: %s, error = 0x%x\n", errmsg.c_str(), hr);
            ADD_TO_VACP_FAIL_MSGS(VACP_DRIVER_OPEN_FAILED, errmsg, hr);
            ExitVacp(VACP_DRIVER_OPEN_FAILED, true);
        }

        const unsigned long driverVersionIoctlCmd = IOCTL_INMAGE_GET_VERSION;
        DWORD dwReturn = 0;
        BOOL bResult = DeviceIoControl(
            ghControlDevice,
            driverVersionIoctlCmd,
            NULL,
            0,
            (void*)&g_inmFltDriverVersion,
            sizeof(g_inmFltDriverVersion),
            &dwReturn,
            NULL);
        if (!bResult)
        {
            DWORD dwErr = GetLastError();
            DebugPrintf("IOCTL_INMAGE_GET_VERSION failed: %d\n", dwErr);
            ExitVacp(VACP_E_VACP_MODULE_FAIL, true);
        }
        else
        {
            DebugPrintf("Driver version:\nMajor Version: %d\n Minor Version: %d\n Minor Version2: %d\n Minor Version3:%d\n",
                g_inmFltDriverVersion.ulDrMajorVersion, g_inmFltDriverVersion.ulDrMinorVersion,
                g_inmFltDriverVersion.ulDrMinorVersion2, g_inmFltDriverVersion.ulDrMinorVersion3);
        }
    }

    StopLongRunningInMageVssProvider(VSS_PROV_DELAYED_STOP_WAIT_TIME);

    if (g_bDistributedVacp)
    {
        HRESULT hr = ValidateMultiVmConsistencySettings(CmdRequest);
        if (FAILED(hr))
            ExitVacp(hr, true);

        if (g_vLocalHostIps.size() <= 0)
        {
            g_bDistributedVacp = false;
            g_bThisIsMasterNode = false;
            errmsg = "No IP addresses are obtained from the local machine and hence it CANNOT issue a Distributed tag. But it will issue a LOCAL tag only.";
            DebugPrintf("\n%s \n", errmsg.c_str());
            ADD_TO_MULTIVM_FAIL_MSGS(VACP_MODULE_INFO, errmsg, VACP_MODULE_INFO);
        }
    }
    
    DWORD dwWaitResult = 0;
    if(gbPreScript)
    {
      DWORD dwExitCode = ExecuteScript(gstrPreScript,ST_PRESCRIPT);
    }

    InitQuitEvent();

    if (g_bDistributedVacp && !g_strMasterNodeFingerprint.empty())
    {
        AADAuthProvider::Initialize();
    }

    boost::thread_group thread_group;

    if (gbParallelRun)
    {
        if (gbBaselineTag)
        {
            ExitVacp(VACP_INVALID_ARGUMENT, true);
        }
        else if (gbCrashTag)
        {
            boost::thread *ccthread = new boost::thread(boost::bind(RunCrashConsistency, CmdRequest));
            thread_group.add_thread(ccthread);
        }
        if (gbSystemLevelTag)
        {
            boost::thread *acthread = new boost::thread(boost::bind(RunAppConsistency, CmdRequest));
            thread_group.add_thread(acthread);
        }
    }
    else
    {
        if (gbBaselineTag)
        {
            RunBaselineConsistency(CmdRequest);
        }
        else if (gbCrashTag)
        {
            RunCrashConsistency(CmdRequest);
        }
        else if (gbSystemLevelTag)
        {
            // Get Disk List
            HRESULT hresult = GetDiskInfo();
            if (hresult != S_OK)
            {
                inm_printf("\n Unable to get the disk guid/signature on local system.\n");
                ExitVacp(VACP_DISK_DISCOVERY_FAILED, true);
            }

            hresult = ValidateAppConsistencySettings(CmdRequest);
            if (hresult != S_OK)
                ExitVacp(hresult, true);

            RunAppConsistency(CmdRequest);
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
        ExitVacp(0, false);
    }
    else
    {
        ExitVacp(gExitCode, true);
    }

}

void AppConsistency::CleanupVssProvider()
{
    const uint32_t MAX_VSS_PROV_UNREGISTER_RETRIES = 3;

    for (auto numRetries = 0; numRetries < MAX_VSS_PROV_UNREGISTER_RETRIES; numRetries++)
    {
        HRESULT unreghr = UnregisterInMageVssProvider();
        if (unreghr == S_OK)
        {
            DebugPrintf("Successfully unregistered VSS Provider.\n");
            break;
        }
        else
        {
            DebugPrintf("VSS Provider unregister failed with 0x%08x, retrying...\n", unreghr);
        }
    }

    StopLongRunningInMageVssProvider(VSS_PROV_IMMEDIATE_STOP_WAIT_TIME);

    if (pInMageVssProvider)
    {
        DebugPrintf("Releasing InMageVssProvider\n");
        pInMageVssProvider.Release();
        pInMageVssProvider = NULL;
    }

    if (gptrVssAppConsistencyProvider != NULL)
    {
        gptrVssAppConsistencyProvider->UnInitialize();//Check if this is required or not?
        delete gptrVssAppConsistencyProvider;
        gptrVssAppConsistencyProvider = NULL;
    }

    return;
}

void AppConsistency::Process()
{
    std::string universalTime = boost::lexical_cast<std::string>(GetTimeInSecSinceEpoch1970());
    m_tagTelInfo.Add(VacpSysTimeKey, universalTime);
    DebugPrintf("SysTimeKey %s .\n", universalTime.c_str());
    
    m_tagTelInfo.Add(VacpCommand, g_strCommandline);
    DebugPrintf("\nCommand Line: %s\n", g_strCommandline.c_str());

    DebugPrintf("%s %s .\n", TagType.c_str(), APP_TAG);
    m_tagTelInfo.Add(TagType, APP_TAG);

    m_vacpErrorInfo.clear();//Clear all the earlier run Health Issues.
    m_vacpErrorInfo.SetAttribute(VacpAttributes::LOG_VERSION, std::string(VACP_ERROR_VERSION));
    m_vacpErrorInfo.SetAttribute(VacpAttributes::LOG_PATH, g_strLogFilePath);

    //
    // Process the system level application consistency tags
    // all the disks that are protected and their volumes
    // required for system level recovery should be part
    // of VSS snapshot
    //

    std::string errmsg;
    HRESULT hr = S_OK;
    m_status = CONSISTENCY_STATUS_INITIALIZED;

    if (gbIRMode)
    {
        return IRModeStamp();
    }

    if (gbParallelRun)
    {
        // redo a discovery 
        HRESULT hresult = DiscoverDisksAndVolumes();
        if (hresult != S_OK)
        {
            inm_printf("\n Unable to get the disks and volumes on local system.\n");
            return Conclude(VACP_DISK_DISCOVERY_FAILED);
        }
    }

    // if there are more than max snapshot volume limit 
    // a system level recovery with app bookmarks not supported
    //
    if (m_CmdRequest.Volumes.size() > VSS_SNAPSHOTSET_LIMIT)
    {
        std::stringstream ss;
        ss << "Error: application consistency with more than " << VSS_SNAPSHOTSET_LIMIT << " volumes is not supported.";
        errmsg = ss.str();
        DebugPrintf("\n%s \n", errmsg.c_str());
        AddToVacpFailMsgs(VACP_E_VACP_MODULE_FAIL, errmsg, ERROR_NODE_HAS_MORE_VOLS_THAN_SNAPSHOT_SET_LIMIT);
        return Conclude(ERROR_NODE_HAS_MORE_VOLS_THAN_SNAPSHOT_SET_LIMIT);
    }

    universalTime = boost::lexical_cast<std::string>(GetTimeInSecSinceEpoch1970());
    m_tagTelInfo.Add(VacpStartTimeKey, universalTime);
    DebugPrintf("VacpStartTime %s .\n", universalTime.c_str());

    ACSRequestGenerator *gen = new (nothrow) ACSRequestGenerator();

    if (gen == NULL)
    {
        hr = VACP_MEMORY_ALLOC_FAILURE;
        DebugPrintf("Memory allocation for ACSRequestGenerator failed, exiting... \n");
        return Conclude((ULONG)hr);
    }

    // Generate Hydration Tag if it is available
    // We have following window where system state can change
    //          100ms   for local crash consistency
    //          60s     for local app consistency
    //          120s    for distributed consistency
    //  During this window if boot drivers reg changes
    //  failover VM may not boot up.
    std::string hydrationTag;
    if (gbVToAMode)
    {
        hydrationTag = HydrationConfigurator::GetHydrationTag(g_strHydrationFilePath);
    }

    StartCommunicator();

    DistributedTagGuidPhase();

    hr = PreparePhase();
    if (hr != S_OK)
    {
        DebugPrintf("PreparePhase failed.\n");
        return Conclude((ULONG)hr);
    }

    hr = gen->GenerateACSRequest(m_CmdRequest, m_AcsRequest, APP);
    if (hr != S_OK)
    {
        DebugPrintf("GsenerateACSRequest failed, exiting... \n");
        delete gen;
        return Conclude((ULONG)hr);
    }

    m_AcsRequest.bDistributed = m_bDistributedVacp;

    hr = driverPreCheck(m_AcsRequest);
    if (hr != S_OK)
    {
        DebugPrintf("Driver precheck failed.\n");
        delete gen;
        return Conclude((ULONG)hr);
    }

    hr = CreateContextGuid();
    if (hr != S_OK)
    {
        delete gen;
        return Conclude((ULONG)hr);
    }

    m_AcsRequest.m_contextGuid = m_strContextGuid;
    RPC_STATUS rstatus;
    unsigned char *pszErrTagGuid;
    std::string strErrTagGuid;
    if (m_AcsRequest.bDistributed)
    {
        rstatus = UuidToString(&m_AcsRequest.distributedTagGuid,(unsigned char**)&pszErrTagGuid);
        DebugPrintf("\nDistributed Tag Guid:%s\n", pszErrTagGuid);
    }
    else
    {
        rstatus = UuidToString(&m_AcsRequest.localTagGuid, (unsigned char**)&pszErrTagGuid);
        DebugPrintf("\local Tag Guid:%s\n", pszErrTagGuid);
    }
    if (rstatus != RPC_S_OK)
    {
        hr = HRESULT_FROM_WIN32(rstatus);
        std::string errmsg = "Failed to convert UUID to a string.";
        DebugPrintf("%s, status = 0x%x\n", errmsg.c_str(), hr);
        AddToVacpFailMsgs(VACP_PRECHECK_FAILED, errmsg, hr);
    }
    else if (rstatus == RPC_S_OK)
    {
        strErrTagGuid.assign((char*)pszErrTagGuid, strlen((char*)pszErrTagGuid));
        RpcStringFree(&pszErrTagGuid);
    }

    m_vacpErrorInfo.SetAttribute(VacpAttributes::TAG_GUID, strErrTagGuid);
    m_vacpErrorInfo.SetAttribute(VacpAttributes::TAG_TYPE, APP_TAG);

    m_AcsRequest.m_hydrationTag = hydrationTag;

    InMageVssRequestorPtr vssRequestorPtr = gen->GetVssRequestorPtr();

    if (vssRequestorPtr.get() == NULL)
    {
        hr = ERROR_FAILED_TO_INITIALIZE_VSS_REQUESTOR;
        DebugPrintf("Failed to get Vss Requestor.\n");
        delete gen;
        return Conclude(hr);
    }

    //gptrVssAppConsistencyProvider = new (nothrow)VssAppConsistencyProvider(m_AcsRequest, m_CmdRequest.bFullBackup);
    gptrVssAppConsistencyProvider = new (nothrow)VssAppConsistencyProvider(m_AcsRequest, vssRequestorPtr);

    if (!gptrVssAppConsistencyProvider)
    {
        hr = VACP_MEMORY_ALLOC_FAILURE;
        DebugPrintf("Failed to create gptrVssAppConsistencyProvider.\n");
        delete gen;
        return Conclude(VACP_MEMORY_ALLOC_FAILURE);
    }

    hr = gptrVssAppConsistencyProvider->Initialize();
    if (hr != S_OK)
    {
        DebugPrintf("Failed to initialize VssAppConsistencyProvider 0x%08x.\n", hr);
        delete gptrVssAppConsistencyProvider;
        delete gen;//Check if this is required or not?
        return Conclude((ULONG)hr);
    }

    DebugPrintf("Preparing the applications for consistency ...\n");

    m_perfCounter.StartCounter("AppPrepare");
    hr = gptrVssAppConsistencyProvider->Prepare(m_CmdRequest);
    if (hr != S_OK)
    {
        DebugPrintf("VssAppConsistencyProvider Prepare failed 0x%08x.\n", hr);
        delete gen;
        return Conclude((ULONG)hr);
    }
    else
    {
        m_status |= CONSISTENCY_STATUS_PREPARE_SUCCESS;
    }

    PrepareAckPhase();

    m_perfCounter.EndCounter("AppPrepare");

    QuiescePhase();

    DebugPrintf("\nFreezing the applications for consistency ...\n");

    inm_log_to_buffer_start();
    // note: the inm_log_to_buffer_end() gets called in Conclude()
    // for both success and error cases. So it should not be called
    // within the Process() scope

    m_perfCounter.StartCounter("AppQuiesce");

    hr = gptrVssAppConsistencyProvider->Quiesce();
    if (hr != S_OK)
    {
        if ((m_status & CONSISTENCY_STATUS_INSERT_TAG_SUCCESS) == CONSISTENCY_STATUS_INSERT_TAG_SUCCESS)
        {
            // the tag are issued when the VSS Provider callback
            // if the post snapshot operations failed revoke the tags
            hr = CommitRevokeTags(m_AcsRequest);
            if (hr != S_OK)
            {
                errmsg = "FAILED to revoke tags.";
                XDebugPrintf("FILE %s: LINE %d, %s\n", __FILE__, __LINE__, errmsg.c_str());
                errmsg += m_vacpLastError.m_errMsg;
                AddToVacpFailMsgs(VACP_E_COMMIT_REVOKE_TAG_PHASE_FAILED, errmsg, m_vacpLastError.m_errorCode);
            }
        }

        if ((m_status & CONSISTENCY_STATUS_QUIESCE_SUCCESS) == CONSISTENCY_STATUS_QUIESCE_SUCCESS)
        {
            // if QUIESCE is set to success in the VSS Provider call back
            // reset here
            m_status &= ~CONSISTENCY_STATUS_QUIESCE_SUCCESS;
        }

        delete gen;
        return Conclude((ULONG)hr);
    }

    m_perfCounter.EndCounter("AppQuiesce");

    DebugPrintf("Resuming the applications after consistency check-point\n");
    hr = gptrVssAppConsistencyProvider->Resume();
    if (hr != S_OK)
    {
        DebugPrintf("Resume() failed. \n");
        AddToVacpFailMsgs(VACP_E_RESUME_PHASE_FAILED, m_vacpLastError.m_errMsg, m_vacpLastError.m_errorCode);
        hr = CommitRevokeTags(m_AcsRequest);
                
        if (hr != S_OK)
        {
            errmsg = "FAILED to revoke tags.";
            XDebugPrintf("FILE %s: LINE %d, %s\n", __FILE__, __LINE__, errmsg.c_str());
            errmsg += m_vacpLastError.m_errMsg;
            AddToVacpFailMsgs(VACP_E_COMMIT_REVOKE_TAG_PHASE_FAILED, errmsg, m_vacpLastError.m_errorCode);
        }

        delete gen;
        return Conclude((ULONG)hr);
    }
    else
    {
        m_status |= CONSISTENCY_STATUS_RESUME_SUCCESS;
    }
    
    ResumeAckPhase();

    if ((m_status & CONSISTENCY_STATUS_INSERT_TAG_SUCCESS) == CONSISTENCY_STATUS_INSERT_TAG_SUCCESS)
    {

        hr = CommitRevokeTags(m_AcsRequest);
        if (hr != S_OK)
        {
            errmsg = "FAILED to commit tags.";
            XDebugPrintf("FILE %s: LINE %d, %s\n", __FILE__, __LINE__, errmsg.c_str());
            errmsg += m_vacpLastError.m_errMsg;
            AddToVacpFailMsgs(VACP_E_COMMIT_REVOKE_TAG_PHASE_FAILED, errmsg, m_vacpLastError.m_errorCode);
        }
        else
        {
            m_status |= CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS;
        }
    }

    if (gen != NULL)
    {
        delete gen;
    }

    if(m_bDistributedVacp)
    {
        if ((m_status & CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS) == CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS)
        {
            m_tagTelInfo.Add(MultivmTagKey, std::string());
            DebugPrintf("Exiting gracefully ...(Issued a DISTRIBUTED TAG on this Node.)\n");

            return Conclude(0);
        }
        else
        {
            if (g_bThisIsMasterNode)
            {
                DebugPrintf("\nMaster Node has failed to issue the tag but it has facilitated other Client Nodes to issue a Distributed Tag.\n");
                DebugPrintf("\nPlease check this vacp output.\n");
            }

            if (m_DistributedProtocolStatus)
                hr = m_DistributedProtocolStatus;
            else
                hr = E_FAIL;

            DWORD status = ~m_status & CONSISTENCY_STATUS_SUCCESS_BITS;
            DebugPrintf("Local tag status 0x%x.\n", status);
        }
    }
    else
    {
        if ((m_status & CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS) == CONSISTENCY_STATUS_COMMIT_TAG_SUCCESS)
        {
            m_tagTelInfo.Add(LocalTagKey, std::string());
            DebugPrintf("Exiting gracefully ...(Issued a LOCAL TAG)\n");
            return Conclude(0);
        }
        else
        {
            if (hr == S_OK)
                hr = E_FAIL;

            if (m_DistributedProtocolStatus)
                hr = m_DistributedProtocolStatus;
          
            DWORD status = ~m_status & CONSISTENCY_STATUS_SUCCESS_BITS;
            DebugPrintf("Local tag status 0x%x.\n", status);
        }
    }
    
    DebugPrintf("Concluding with status 0x%x.\n", hr);
    return Conclude(hr);
}
 
HRESULT ParseCommandLineArgs(int argc, _TCHAR* argv[], CLIRequest_t &CmdRequest)
{
    HRESULT hr = S_OK;
    std::string errmsg;
    
    inm_printf("\nParsing command line arguments ....\n");

    try
    {
        TagArgParser *parser = new TagArgParser(&CmdRequest, argc, argv);

    }
    catch (...) {
        hr = VACP_INVALID_ARGUMENT;
    }

    if (hr != S_OK)
    {
        std::string errmsg = "Could not parse the command line options";
        inm_printf("%s\n", errmsg.c_str());
        ADD_TO_VACP_FAIL_MSGS(VACP_INVALID_ARGUMENT, errmsg, VACP_INVALID_ARGUMENT);
        return hr;
    }

    hr = Validatecommandline(CmdRequest);
    
    return hr;
}

HRESULT InitAgentSettings(_TCHAR* argv[], CLIRequest_t &CmdRequest)
{
    HRESULT hr = S_OK;

    std::string agentInstallPath, vacpLogFile, confPath, confFilePath, scheduleFilePath;
    if (S_OK == getAgentInstallPath(agentInstallPath))
    {
        if (agentInstallPath.empty())
        {
            DebugPrintf("\ASR Mobility Agent Install path is empty.\n");
            ExitVacp(VACP_GET_INSTALLPATH_FAILED, true);
        }
        confPath = agentInstallPath + AppDataEtcPath;
        g_strLogFilePath = agentInstallPath + AppPolicyLogsPath + "\\";
    }
    else
    {
        string cmdPath = argv[0];
        confPath = cmdPath.substr(0, cmdPath.find_last_of('\\'));
        g_strLogFilePath = cmdPath + "\\";
    }
    vacpLogFile += g_strLogFilePath;
    vacpLogFile += VACP_LOG_FILE_NAME;
    g_strHydrationFilePath = confPath + "\\" + SV_HYDRATION_CONF_FILE;

    inm_init_file_log(vacpLogFile);

    confFilePath = confPath + "\\" + VACP_CONF_FILE_NAME;
    scheduleFilePath = confPath + "\\" + ConsistencyScheduleFile;
    CmdRequest.m_strScheduleFilePath = scheduleFilePath;
    GetLastConsistencyScheduleTime(CmdRequest);

    VacpConfigPtr pVacpConf = VacpConfig::getInstance();

    if (pVacpConf != NULL)
    {
        pVacpConf->Init(confFilePath);

        if (gbParallelRun && pVacpConf->getParams().empty())
        {
            DebugPrintf("\nConfig parameters are required in parallel mode.\n");
            ExitVacp(VACP_CONF_PARAMS_NOT_SUPPLIED, true);
        }

        std::map<std::string, std::string> &params = pVacpConf->getParams();
        std::map<std::string, std::string>::iterator iterp = params.begin();
        while (iterp != params.end())
        {
            XDebugPrintf("Param %s Value %s\n", iterp->first.c_str(), (*iterp).second.c_str());
            iterp++;
        }

        if (!gbVToAMode)
        {
            if (!pVacpConf->getParamValue(VACP_CONF_PARAM_MASTER_NODE_FINGERPRINT, g_strMasterNodeFingerprint))
            {
                if (gbParallelRun)
                    DebugPrintf("Master node fingerprint is not found. Consistency will fail in case of multi-VM parallel run.\n");
            }

            if (!pVacpConf->getParamValue(VACP_CONF_PARAM_RCM_SETTINGS_PATH, g_strRcmSettingsPath))
            {
                if (gbParallelRun)
                    DebugPrintf("RCM settings path is not found. Consistency will fail in case of multi-VM parallel run.\n");
            }

            if (!pVacpConf->getParamValue(VACP_CONF_PARAM_PROXY_SETTINGS_PATH, g_strProxySettingsPath))
            {
                if (gbParallelRun)
                    DebugPrintf("Proxy settings path is not found.\n");
            }

            DebugPrintf("%s %s %s\n", g_strMasterNodeFingerprint.c_str(), g_strRcmSettingsPath.c_str(), g_strProxySettingsPath.c_str());
        }
            
    }
    else
    {
        DebugPrintf("Failed reading params from %s. Using defaults.\n", confFilePath.c_str());
    }

    return hr;
}

HRESULT ValidateMultiVmConsistencySettings(const CLIRequest_t &CmdRequest)
{
    HRESULT hr = S_OK;
    std::string errmsg;

    if (g_bDistributedVacp)
    {
        // check if the master node argument is a valid ip
        std::string err;
        DebugPrintf("Validating master node: %s\n", g_strMasterHostName.c_str());
        if (ValidateAndGetIPAddress(g_strMasterHostName, err).empty())
        {
            if (boost::iequals(g_strMasterHostName, g_strLocalHostName))
            {
                boost::to_lower(g_strMasterHostName);
                g_bThisIsMasterNode = true;
            }
        }
        else
        {
            // argument is a valid IP
            std::string strMasterNodeIP = g_strMasterHostName;
            strset_t::iterator iterLocalIps = g_vLocalHostIps.begin();
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
        std::vector<std::string>::const_iterator iterCoordinator = CmdRequest.vCoordinatorNodes.begin();

        for (/*empty*/; iterCoordinator != CmdRequest.vCoordinatorNodes.end(); iterCoordinator++)
        {
            std::string err;
            if (ValidateAndGetIPAddress(*iterCoordinator, err).empty())
            {
                // the command line argument is hostname
                // check if it matches local host name

                if (boost::iequals(g_strLocalHostName, *iterCoordinator))
                {
                    localNodeIsCoordNode = true;
                    g_strLocalIpAddress = *iterCoordinator;
                    boost::to_lower(g_strLocalIpAddress);
                    break;
                }
            }
            else
            {
                strset_t::iterator iterLocalIps = g_vLocalHostIps.begin();
                while (iterLocalIps != g_vLocalHostIps.end())
                {
                    DebugPrintf("Check for coord node:%s:%s\n", (*iterCoordinator).c_str(), (*iterLocalIps).c_str());

                    if (boost::iequals(*iterCoordinator, *iterLocalIps))
                    {
                        if (localNodeIsCoordNode)
                        {
                            errmsg = "Found multiple local IP addresses in the coordinator node list.";
                            errmsg += "Prev node: " + g_strLocalIpAddress;
                            errmsg += "Curr node: " + *iterCoordinator;
                            DebugPrintf("%s\n", errmsg.c_str());
                            hr = VACP_E_INVALID_COMMAND_LINE;
                            ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
                            ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, errmsg, VACP_E_INVALID_COMMAND_LINE);
                            goto end;
                        }

                        localNodeIsCoordNode = true;
                        g_strLocalIpAddress = *iterCoordinator;
                    }
                    iterLocalIps++;
                }
            }
        }

        if (localNodeIsCoordNode && g_bThisIsMasterNode)
        {
            errmsg = "A node cannot be in both -mn and -cn option.";
            DebugPrintf("\n%s\n", errmsg.c_str());
            hr = VACP_E_INVALID_COMMAND_LINE;
            ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
            ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, errmsg, VACP_E_INVALID_COMMAND_LINE);
            goto end;
        }

        if (!localNodeIsCoordNode && !g_bThisIsMasterNode)
        {
            DebugPrintf("\nThis node is neither master node nor coordinator node. A local consistency tag will be issued.\n");
            g_bDistributedVacp = false;
        }
    }

end:
    return hr;
}

HRESULT ValidateAppConsistencySettings(CLIRequest_t &CmdRequest)
{
    USES_CONVERSION;

    HRESULT hr = S_OK;
    std::string errmsg;
    bool bIncludeAllVolumes = false;
    bool bIncludeAllApplications = false;
    bool bVolumesSkipped = false;
    vector<const char *> LogicalVolumes;
    std::stringstream       ssError;
    std::set<std::string>::iterator volIter;

    if (!IsDriverLoaded(INMAGE_DISK_FILTER_DOS_DEVICE_NAME))
    {
        if (!gbVerify && !gbPrint)
        {
            errmsg = "Involflt driver is not loaded on local system. Can not continue.";
            inm_printf("\nERROR:%s\n VACP_E_INVOLFLT_DRIVER_NOT_INSTALLED: Error Code= %d \nExiting...\n", errmsg.c_str(), VACP_E_INVOLFLT_DRIVER_NOT_INSTALLED);
            hr = VACP_E_INVOLFLT_DRIVER_NOT_INSTALLED;
            ghrErrorCode = VACP_E_INVOLFLT_DRIVER_NOT_INSTALLED;
            ADD_TO_VACP_FAIL_MSGS(VACP_E_INVOLFLT_DRIVER_NOT_INSTALLED, errmsg, VACP_E_INVOLFLT_DRIVER_NOT_INSTALLED);
            goto end;
        }
        else
        {
            inm_printf("\nWarning: Neither InDiskFlt nor InVolFlt driver is loaded on local system.");
        }
    }

    if (gbUseDiskFilter)
    {
        CmdRequest.m_diskGuidMap = gDiskGuidMap;
        // consider the input GUID as the disk signature/GUID and process them
        for (unsigned int iDiskGuid = 0; iDiskGuid < CmdRequest.VolumeGuids.size(); iDiskGuid++)
        {
            std::wstring strDiskGuidVal = A2W(CmdRequest.VolumeGuids[iDiskGuid]);
            std::map<wstring, string>::iterator iter = gDiskGuidMap.find(strDiskGuidVal);
            if (iter == gDiskGuidMap.end())
            {
                inm_printf("Diskguid %s not found on local system.\n", CmdRequest.VolumeGuids[iDiskGuid]);
                hr = VACP_DISK_DISCOVERY_FAILED;
                ghrErrorCode = VACP_DISK_DISCOVERY_FAILED;
                goto end;
            }
            else
            {
                CmdRequest.vDisks.insert(iter->second);
                CmdRequest.vDiskGuids.insert(iter->first);
            }
        }

        // clean up the VolumeGuids
        CmdRequest.VolumeGuids.clear();
    }
    else
    {
        //GUID processing
        //Convert Volume and Mount Point GUIDs into Volumes
        //Convert the Volume GUIDs (-guid)into Volumes and Mount Points here for accepting Volume GUIDs instead of Volumes(-v).	   
        char volumePathName[MAX_PATH + 1] = { 0 };

        DWORD returnedLength = 0;
        for (unsigned int iVolGuid = 0; iVolGuid < CmdRequest.VolumeGuids.size(); iVolGuid++)
        {
            //if the user enters the GUIDs in the following format \\?\Volume{<GUID>}\ then inform him to enter only GUIDs
            const char* strVolPtr = CmdRequest.VolumeGuids[iVolGuid];
            string strVolTmp = strVolPtr;
            if (strVolTmp.find("\\\\?\\Volume{") != string::npos)
            {
                inm_printf("\n Enter only GUIDs. No need to enter GUIDs in the following format...\n \\\\?\\Volume{<GUID>}\\ \n");
                hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
                ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
                goto end;
            }

            memset((void*)volumePathName, 0x00, sizeof(volumePathName));
            returnedLength = 0;
            string strVolGuidPrefix = "\\\\?\\Volume{";
            string strVolGuidVal = CmdRequest.VolumeGuids[iVolGuid];
            string strVolGuidSuffix = "}\\";
            string strVolGuid = strVolGuidPrefix + strVolGuidVal + strVolGuidSuffix;
            BOOL bGotVol = GetVolumePathNamesForVolumeName(strVolGuid.c_str(),
                volumePathName,
                MAX_PATH,
                &returnedLength);
            if (TRUE == bGotVol)
            {
                std::string sVol = volumePathName;
                volumePathNames[iVolGuid] = sVol;
                CmdRequest.Volumes.insert(volumePathNames[iVolGuid]);
            }
        }
    }


    volIter = CmdRequest.Volumes.begin();
    for (/*empty*/; volIter != CmdRequest.Volumes.end(); volIter++)
    {
        const char *volName = volIter->c_str();

        if ((stricmp("all", volName) == 0)/* || (true == gbAllVolumes)*/)
        {
            bIncludeAllVolumes = true;
            continue;
        }

        if (gbUseDiskFilter)
        {
            if (!IsItValidDiskName(volName))
            {
                std::stringstream ss;
                ss << "Encountered Invalid disk " << volName;
                inm_printf("%s \n", ss.str().c_str());
                hr = VACP_E_INVALID_COMMAND_LINE;
                ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
                if (ghrErrorCode != S_OK)
                {
                    hr = ghrErrorCode;
                }
                else
                {
                    ghrErrorCode = VACP_E_NON_FIXED_DISK;
                    hr = VACP_E_NON_FIXED_DISK;//E_FAIL;
                }
                ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, ss.str(), hr);
                break;
            }
            else
            {
                CmdRequest.m_diskGuidMap = gDiskGuidMap;
                std::map<wstring, string>::iterator diskGuidIter = gDiskGuidMap.begin();
                for (; diskGuidIter != gDiskGuidMap.end(); diskGuidIter++)
                {
                    if (diskGuidIter->second.compare(volName) == 0)
                    {
                        CmdRequest.vDisks.insert(diskGuidIter->second);
                        CmdRequest.vDiskGuids.insert(diskGuidIter->first);
                        break;
                    }
                }

                if (diskGuidIter == gDiskGuidMap.end())
                {
                    std::stringstream ss;
                    ss << "disk not found on local system " << volName;
                    inm_printf("%s \n", ss.str().c_str());
                    ghrErrorCode = VACP_E_NON_FIXED_DISK;
                    hr = VACP_E_NON_FIXED_DISK;//E_FAIL;
                    ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, ss.str(), hr);
                    break;
                }
            }
        }
        else
        {
            if (IsItVolumeMountPoint(volName))
            {
                XDebugPrintf("%s: is volume mount point.\n", volName);
            }
            if (!gbSkipUnProtected)
            {
                if (IsValidVolumeEx(volName) == false)
                {
                    inm_printf("Encountered Invalid volume %s \n", volName);
                    if (ghrErrorCode != S_OK)
                    {
                        hr = ghrErrorCode;
                    }
                    else
                    {
                        ghrErrorCode = VACP_E_NON_FIXED_DISK;
                        hr = VACP_E_NON_FIXED_DISK;//E_FAIL;
                    }
                    break;
                }
            }

            if (CmdRequest.DoNotIncludeWriters == false && IsVolumeLocked(volName) == true)
            {
                bVolumesSkipped = true;
                inm_printf("WARN: Volume \"%s\" is locked. Skipping it...\n\n", volName);
                CmdRequest.Volumes.erase(volIter++);
                continue;
            }
        }

    }

    if (gbUseDiskFilter)
    {
        // clean up the Volumes as they are already moved to Disks
        CmdRequest.Volumes.clear();
    }

    if (hr != S_OK)
    {
        return hr;
    }

    if (!GetVolumesList(LogicalVolumes) ||
            !GetMountPointsList(LogicalVolumes)) //This will help vacp -v all in considering the Mount Points 
    {
            return E_FAIL;
    }

    if (bIncludeAllVolumes)
    {
        //Erase old list and re-create a new list
        CmdRequest.Volumes.clear();

        if (gbUseDiskFilter)
        {
            CmdRequest.m_diskGuidMap = gDiskGuidMap;
            std::map<wstring, string>::iterator diskIter = gDiskGuidMap.begin();
            for (; diskIter != gDiskGuidMap.end(); diskIter++)
            {
                CmdRequest.vDisks.insert(diskIter->second);
                CmdRequest.vDiskGuids.insert(diskIter->first);
            }
        }
        else
        {
            for (unsigned int index = 0; index < LogicalVolumes.size(); index++)
            {
                if (CmdRequest.DoNotIncludeWriters == false && IsVolumeLocked(LogicalVolumes[index]) == true)
                {
                    inm_printf("\nWARN: Volume \"%s:\" is locked. Skipping it...\n", LogicalVolumes[index]);
                    continue;
                }

                if (IsValidVolumeEx(LogicalVolumes[index]) == false)
                {
                    if (!gbSkipUnProtected)
                    {
                        inm_printf("Encountered Invalid volume %s \n", LogicalVolumes[index]);

                        if (ghrErrorCode != S_OK)
                        {
                            hr = ghrErrorCode;
                        }
                        else
                        {
                            hr = E_FAIL;
                        }
                        break;
                    }
                }
                CmdRequest.Volumes.insert(LogicalVolumes[index]);
            }
        }
    }

    if (gbUseDiskFilter)
    {
        for (unsigned int index = 0; index < LogicalVolumes.size(); index++)
        {
            vector<string> disknames;

            hr = GetDiskNamesForVolume(LogicalVolumes[index], disknames);

            if (S_OK != hr)
            {
                DebugPrintf("GetDiskNamesForVolume() Failed for volume %s. Error= %d\n", LogicalVolumes[index], hr);
                continue;
            }

            vector<string>::iterator diskNameIter = disknames.begin();
            for (; diskNameIter != disknames.end(); diskNameIter++)
            {
                string diskname = *diskNameIter;
                std::set<std::string>::iterator diskIter = CmdRequest.vDisks.begin();

                for (; diskIter != CmdRequest.vDisks.end(); diskIter++)
                {
                    if (diskname.compare(*diskIter) == 0)
                    {
                        CmdRequest.Volumes.insert(LogicalVolumes[index]);
                        break;
                    }
                }

                if (diskIter != CmdRequest.vDisks.end())
                    break;
            }
        }

        std::vector<std::string> bootableVolumes;
        hr = GetBootableVolumes(bootableVolumes);
        if (hr == S_OK)
        {
            std::vector<std::string>::iterator bootVolIter = bootableVolumes.begin();
            for (; bootVolIter != bootableVolumes.end(); bootVolIter++)
            {
                size_t bootVolSize;
                INM_SAFE_ARITHMETIC(bootVolSize = InmSafeInt<unsigned long>::Type(bootVolIter->length()) + 1, INMAGE_EX(bootVolIter->length()))
                char *bootVolume = new (nothrow) char[bootVolSize];
                if (NULL == bootVolume)
                {
                    inm_printf("%s: failed to allocate %d bytes memory for boot volume.\n",
                        __FUNCTION__,
                        bootVolIter->length());
                    hr = VACP_MEMORY_ALLOC_FAILURE;
                    ghrErrorCode = VACP_MEMORY_ALLOC_FAILURE;
                    errmsg = "failed to allocate memory for boot volume.";
                    ADD_TO_VACP_FAIL_MSGS(VACP_MEMORY_ALLOC_FAILURE, errmsg, hr);
                    break;
                }
                inm_strcpy_s(bootVolume, bootVolIter->length() + 1, bootVolIter->c_str());
                CmdRequest.Volumes.insert(bootVolume);
            }
        }
    }

    if (hr != S_OK)
    {
        ADD_TO_VACP_FAIL_MSGS(VACP_PARSE_COMMAND_LINE_ARGS_FAILED, g_vacpLastError.m_errMsg, g_vacpLastError.m_errorCode);
        return hr;
    }

    if (gbSkipUnProtected) //drop non-existent volumes from CmdRequest.Volumes
    {
        std::set<std::string>::const_iterator iterValidVol = CmdRequest.Volumes.begin();
        for (/*empty*/; iterValidVol != CmdRequest.Volumes.end(); iterValidVol++)
        {
            if (0 == iterValidVol->compare("all"))
            {
                gvValidVolumes.insert(*iterValidVol);
                continue;
            }
            else
            {
                if ((*iterValidVol)[0] == '\\')
                {
                    gvValidVolumes.insert((*iterValidVol));
                }
                else if (IsValidVolumeEx(iterValidVol->c_str()))
                {
                    gvValidVolumes.insert(*iterValidVol);
                }
                else
                {
                    inm_printf("\n %s is an Invalid Volume and hence will be dropped from the snapshot set.\n", (*iterValidVol));
                }
            }
        }

        if (gvValidVolumes.size() > 0)
        {
            //Remove the existing vector and fill it up with only valid volumes
            CmdRequest.Volumes = gvValidVolumes;
        }
    }

    InMageVssRequestor *vr = NULL;
    vr = new (std::nothrow) InMageVssRequestor();
    if (vr)
    {
        vr->Initialize();
    }

    //Get the applications/writers from the dynamic list 
    if (vr)
    {
        std::vector<VssAppInfo_t> AppInfo;
        hr = vr->GetVssWritersList(AppInfo, CmdRequest, true);
        if (hr != S_OK)
        {
            DebugPrintf("GetVssWritersList failed with hr = 0x%08x\n", hr);
            ADD_TO_VACP_FAIL_MSGS(VACP_PARSE_COMMAND_LINE_ARGS_FAILED, g_vacpLastError.m_errMsg, g_vacpLastError.m_errorCode);
            delete vr;
            ExitVacp(hr, true);
        }
        else
        {
            //Fill the dynamic VSSWriters2Apps list with the gathered VssAppsInfo
            std::vector<VssAppInfo_t>::iterator iterVssAppInfo = AppInfo.begin();
            while (iterVssAppInfo != AppInfo.end())
            {
                Writer2Application_t *pWriter2App = new Writer2Application_t();
                if (pWriter2App)
                {
                    pWriter2App->VssWriterName = (*iterVssAppInfo).AppName;
                    pWriter2App->ApplicationName = (*iterVssAppInfo).AppName;
                    pWriter2App->usStreamRecType = /*STREAM_REC_TYPE_SYSTEMLEVEL*/STREAM_REC_TYPE_DYNMICALLY_DISCOVERED_NEW_VSSWRITER;
                    pWriter2App->VssWriterInstanceName = (*iterVssAppInfo).szWriterInstanceName;
                    pWriter2App->VssWriterInstanceId = (*iterVssAppInfo).idInstance;
                    pWriter2App->VssWriterId = (*iterVssAppInfo).idWriter;
                    vDynamicVssWriter2Apps.push_back(*pWriter2App);

                    if (gbIncludeAllApplications)
                    {
                        AppParams_t Aparam((*iterVssAppInfo).AppName.c_str());
                        CmdRequest.m_applications.push_back(Aparam);
                    }


                    /*AppSummary_t *pAppSummary = new _AppSummary(((*iterVssAppInfo).AppName),NULL,0);
                    if(pAppSummary)
                    {
                    Request.Applications.push_back(pAppSummary);
                    }*/
                }
                iterVssAppInfo++;
            }
        }
        delete vr;
    }

    for (unsigned int iApp = 0; iApp < CmdRequest.m_applications.size(); iApp++)
    {
        if (boost::iequals(CmdRequest.m_applications[iApp].m_applicationName, "all"))
        {
            bIncludeAllApplications = true;
            gbIncludeAllApplications = true;
            continue;
        }

        if (IsValidApplication(CmdRequest.m_applications[iApp].m_applicationName.c_str()) == false)
        {
            std::stringstream ss;
            ss << "Encountered Invalid application " << CmdRequest.m_applications[iApp].m_applicationName;
            inm_printf("%s. \n", ss.str().c_str());
            hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
            ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
            ADD_TO_VACP_FAIL_MSGS(VACP_PARSE_COMMAND_LINE_ARGS_FAILED, ss.str(), hr);
            break;
        }
    }


    if (hr != S_OK)
    {
        return hr;
    }

    if (bIncludeAllApplications == true)
    {
        std::vector<std::string> VssApplications;

        GetApplicationsList(VssApplications);

        //Erase old list and re-create a new list
        CmdRequest.m_applications.erase(CmdRequest.m_applications.begin(), CmdRequest.m_applications.end());

        for (unsigned int index = 0; index < VssApplications.size(); index++)
        {
            AppParams_t Aparam(VssApplications[index].c_str());
            CmdRequest.m_applications.push_back(Aparam);

            if (IsValidApplication(VssApplications[index].c_str()) == false)
            {
                std::stringstream ss;
                ss << "Encountered Invalid application " << VssApplications[index];
                inm_printf("%s. \n", ss.str().c_str());
                hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
                ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
                ADD_TO_VACP_FAIL_MSGS(VACP_PARSE_COMMAND_LINE_ARGS_FAILED, ss.str(), hr);
                break;
            }
        }
    }

    if (hr != S_OK)
    {
        return hr;
    }

    if (CmdRequest.DoNotIncludeWriters == true && (CmdRequest.m_applications.size() > 0))
    {
        errmsg = "-x and -a options are mutually exclusive. Please specify either of them.";
        inm_printf("%s\n", errmsg.c_str());
        hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
        ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
        ADD_TO_VACP_FAIL_MSGS(VACP_PARSE_COMMAND_LINE_ARGS_FAILED, errmsg, hr);
        goto end;
    }

    if (CmdRequest.bSkipChkDriverMode == true &&
        ((CmdRequest.Volumes.size() <= 0) &&
         (CmdRequest.m_applications.size() <= 0) && 
         (CmdRequest.VolumeGuids.size() <= 0)))//If not Volumes atleast VolumeGuids should be present
    {
        errmsg = "-s option can NOT be used without one of -a or -v or -guid option.";
        inm_printf("%s\n", errmsg.c_str());
        hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
        ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
        ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, errmsg, hr);
        goto end;

    }

    if (CmdRequest.DoNotIncludeWriters == true && 
        (((CmdRequest.Volumes.size() <= 0) &&  
         (CmdRequest.BookmarkEvents.size() <= 0))))
    {
        ssError << "Volumes count: " << CmdRequest.Volumes.size() << " Bookmark count: " << CmdRequest.BookmarkEvents.size();
        ssError << " -x option requires -v and -t options to be specified mandatory.";
        errmsg = ssError.str();
        inm_printf("%s\n", errmsg.c_str());
        hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
        ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
        ADD_TO_VACP_FAIL_MSGS(VACP_PARSE_COMMAND_LINE_ARGS_FAILED, errmsg, hr);
        goto end;
    }

    if ( (CmdRequest.bPrintAppList == false) &&
         (CmdRequest.m_applications.size() == 0) &&
         ((CmdRequest.Volumes.size() == 0) && (CmdRequest.vDisks.size() == 0)) &&
        ((CmdRequest.VolumeGuids.size() == 0) && (CmdRequest.vDiskGuids.size() == 0)) &&
        (CmdRequest.bDelete == false)
        )
    {
        if ((bVolumesSkipped == true) && (CmdRequest.bDelete == false))
        {
            errmsg = "ERROR:Zero valid volumes  or VolumeGUIDs are found. There must be one or more valid volumes or VolumeGUIDs present...";
            inm_printf("\n%s \n", errmsg.c_str());
            hr = VACP_E_ZERO_EFFECTED_VOLUMES;//E_FAIL;
            ghrErrorCode = VACP_E_ZERO_EFFECTED_VOLUMES;
            ADD_TO_VACP_FAIL_MSGS(VACP_PARSE_COMMAND_LINE_ARGS_FAILED, errmsg, hr);
            goto end;
        }
        else
        {
            if (CmdRequest.BookmarkEvents.size() != 0)
            {
                errmsg = "-t option can NOT be used without one of -a or -v or -guid option.";
                inm_printf("%s\n", errmsg.c_str());
                hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
                ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
                ADD_TO_VACP_FAIL_MSGS(VACP_PARSE_COMMAND_LINE_ARGS_FAILED, errmsg, hr);
                goto end;
            }
            else
            {
                // Print app info, if no arguments are specified
                CmdRequest.bPrintAppList = true;
            }
        }
    }

    if (!IS_CRASH_TAG(CmdRequest.TagType) &&
        (0 == CmdRequest.m_applications.size()) &&
        ((0 == gvValidVolumes.size()) && CmdRequest.vDisks.size() == 0) &&
        (false == CmdRequest.bPrintAppList) &&
        (false == gbDeleteSnapshots)
        )
    {
        errmsg = "Warning: There are no Valid Volumes/Mount-Points available in the volume list.";
        inm_printf("\n%s\n", errmsg.c_str());
        ghrErrorCode = VACP_E_NON_FIXED_DISK; // | VACP_E_NO_PROTECTED_VOLUMES ;
        hr = VACP_E_NON_FIXED_DISK;
        ADD_TO_VACP_FAIL_MSGS(VACP_PARSE_COMMAND_LINE_ARGS_FAILED, errmsg, hr);
        return ghrErrorCode;
    }

#if (_WIN32_WINNT > 0x500)
    if (CmdRequest.bPrintAppList)
    {
        InMageVssRequestor *vr = new InMageVssRequestor();
        vector<VssAppInfo_t> AppInfo;

        if (CmdRequest.m_applications.size() == 0)
        {
            hr = vr->GatherVssAppsInfo(AppInfo, CmdRequest, true);
            if (S_OK == hr)
            {
                PrintVssWritersInfo(AppInfo);
            }
        }
        else
        {
            hr = vr->GatherVssAppsInfo(AppInfo, CmdRequest, false);
            if (S_OK == hr)
            {
                PrintFormattedVssWritersInfo(AppInfo, CmdRequest.m_applications);
            }
        }
        delete vr;
    }
#endif 

end:
    return hr;
}

HRESULT Validatecommandline(const CLIRequest_t &CmdRequest)
{
    HRESULT hr = S_OK;
    bool bIncludeAllVolumes = false;
    bool bIncludeAllApplications = false;
    bool bVolumesSkipped = false;
    vector<const char *> LogicalVolumes;
    std::string errmsg;
    std::stringstream       ssError;
    std::set<std::string>::iterator volIter;

    if (!gbUseDiskFilter && IS_CRASH_TAG(CmdRequest.TagType))
    {
        DebugPrintf("\nOptions -cc is not valid with volume filter driver\n");
        hr = VACP_E_INVALID_COMMAND_LINE;
        ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
        goto end;
    }

    if (IS_CRASH_TAG(CmdRequest.TagType) && (gbPreScript || gbPostScript))
    {
        DebugPrintf("\nOptions pre or post script options not supported with crash consistency\n");
        hr = VACP_E_INVALID_COMMAND_LINE;
        ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
        goto end;
    }

    bool bProvider = CmdRequest.bProvider;
    if (bProvider && gbUseDiskFilter)
    {
        DebugPrintf("\nOptions -provider is not valid with disk filter driver\n");
        hr = VACP_E_INVALID_COMMAND_LINE;
        ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
        goto end;
    }

    if (gbSyncTag && gbUseDiskFilter)
    {
        DebugPrintf("\nOptions -sync is not valid with disk filter driver\n");
        hr = VACP_E_INVALID_COMMAND_LINE;
        ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
        goto end;
    }

    if (!bProvider && !gbUseDiskFilter)//then make Microsoft VSS Provider as the default provider by default
    {
        gbUseInMageVssProvider = false;
    }
    else//if -provider switch is used then
    {
        if (CmdRequest.bProvider)
        {
            std::string strProviderID = CmdRequest.strProviderID;
            UUID uuidProvider;
            UUID uuidInMageVssProvider;
            if (UuidFromString((unsigned char*)strProviderID.c_str(),
                (UUID*)&uuidProvider) == RPC_S_OK)
            {
                if (UuidFromString((unsigned char*)gstrInMageVssProviderGuid.c_str(),
                    (UUID*)&uuidInMageVssProvider) == RPC_S_OK)
                {
                    gbUseInMageVssProvider = IsEqualGUID(uuidProvider, uuidInMageVssProvider);
                }
            }
        }
    }


    if (CmdRequest.bVerify && CmdRequest.bSkipUnProtected)
    {
        gbSkipUnProtected = false;
    }
    if (g_bDistributedVacp)
    {
        if (!gbVToAMode &&
            !g_strMasterNodeFingerprint.empty() &&
            (g_strRcmSettingsPath.empty() || g_strProxySettingsPath.empty()))
        {
            errmsg = "Options -rcmSetPath and -proxySetPath should be used with -mnf.";
            DebugPrintf("\n%s\n", errmsg.c_str());
            hr = VACP_E_INVALID_COMMAND_LINE;
            ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
            ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, errmsg, VACP_E_INVALID_COMMAND_LINE);
            goto end;
        }
    }
    if (g_bDistributedVacp)
    {
        if ((!CmdRequest.bMasterNode) || (!CmdRequest.bCoordNodes))
        {
            errmsg = "Options -mn and -cn should be used with -distrib.";
            DebugPrintf("\n%s\n", errmsg.c_str());
            hr = VACP_E_INVALID_COMMAND_LINE;
            ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
            ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, errmsg, VACP_E_INVALID_COMMAND_LINE);
            goto end;
        }
    }
    if ((g_bMasterNode) && (!g_bDistributedVacp))
    {
        errmsg = "Option -distrib should be used when -mn option is used.";
        DebugPrintf("\n%s\n", errmsg.c_str());
        hr = VACP_E_INVALID_COMMAND_LINE;
        ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
        ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, errmsg, VACP_E_INVALID_COMMAND_LINE);
        goto end;
    }
    if ((g_bCoordNodes) && (!g_bDistributedVacp))
    {
        errmsg = "Option -distrib should be used when -cn option is used.";
        DebugPrintf("\n%s\n", errmsg.c_str());
        hr = VACP_E_INVALID_COMMAND_LINE;
        ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
        ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, errmsg, VACP_E_INVALID_COMMAND_LINE);
        goto end;
    }

    if (gbRemoteSend && IS_CRASH_TAG(CmdRequest.TagType))
    {
        DebugPrintf("\nOption -remote cannot be used when -cc option is used.\n");
        hr = VACP_E_INVALID_COMMAND_LINE;
        ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
        goto end;
    }

    if (gbRemoteSend)
    {
        if (g_bDistributedVacp)
        {
            errmsg = "-distributed option can not be used with -remote option.";
            inm_printf("%s\n", errmsg.c_str());
            hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
            ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
            ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, errmsg, hr);
            goto end;
        }

        if ((CmdRequest.bRemote == true) && (CmdRequest.bSkipChkDriverMode == true))
        {
            inm_printf("-s option can not be used with -remote option\n");
            hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
            ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
            goto end;
        }
        if ((CmdRequest.bRemote == true) && ((CmdRequest.bServerIP == false) || (CmdRequest.bServerDeviceID == false)))
        {
            if ((CmdRequest.bServerIP == false) && (CmdRequest.bServerDeviceID == false))
            {
                errmsg = "-serverdevice and -serverip options are missing";
            }
            else if (CmdRequest.bServerIP == false)
            {
                errmsg = "-serverip option is missing";
            }
            else if (CmdRequest.bServerDeviceID == false)
            {
                errmsg = "-serverdevice option is missing";
            }

            hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
            ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
            inm_printf("%s\n", errmsg.c_str());
            ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, errmsg, hr);
            goto end;
        }
    }

    if ((CmdRequest.bDoNotIssueTag == true) && (CmdRequest.bTag == true))
    {
        inm_printf("-NoTAG option can NOT be used with -t option. Please specify either of them\n");
        hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
        ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
        goto end;
    }

    if ((CmdRequest.bDoNotIssueTag == true) && (CmdRequest.DoNotIncludeWriters == true))
    {
        inm_printf("-NoTAG option can NOT be used with -x option.\n");
        hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
        ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
        goto end;
    }

    if (CmdRequest.bPersist &&  gbUseInMageVssProvider)
    {
        inm_printf("-persist option can NOT be used with InMage VSS Provider.\n");
        hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
        ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
        goto end;
    }

    if (gbIsOSW2K3SP1andAbove)
    {
        if ((CmdRequest.bWriterInstance == true) && (CmdRequest.bApplication == true))
        {
            errmsg = "-w option can NOT be used with -a option. Please specify either of them.";
            inm_printf("%s\n", errmsg.c_str());
            hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
            ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
            ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, errmsg, hr);
            goto end;
        }
        if ((CmdRequest.bWriterInstance == true) && (CmdRequest.m_applications.size() <= 0))
        {
            errmsg = "ERROR:No writer name is provided with -w option.";
            inm_printf("%s\n", errmsg.c_str());
            hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
            ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
            ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, errmsg, hr);
            goto end;
        }

        if ((CmdRequest.DoNotIncludeWriters == true) && (CmdRequest.bWriterInstance == true))
        {
            errmsg = "-w option can NOT be used with -x option. Please specify either of them.";
            inm_printf("%s\n", errmsg.c_str());
            hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
            ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
            ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, errmsg, hr);
            goto end;
        }
        if ((CmdRequest.bApplication == true) && (CmdRequest.bDelete == true))
        {
            errmsg = "-a option can NOT be used with -delete option.Please specify either of them.";
            inm_printf("%s\n", errmsg.c_str());
            hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
            ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
            ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, errmsg, hr);
            goto end;
        }
        if ((CmdRequest.Volumes.size() > 0) && (CmdRequest.bDelete == true))
        {
            errmsg = "-v option can NOT be used with -delete option.Please specify either of them.";
            inm_printf("%s\n", errmsg.c_str());
            hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
            ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
            ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, errmsg, hr);
            goto end;
        }
        if ((CmdRequest.VolumeGuids.size() > 0) && (CmdRequest.bDelete == true))
        {
            errmsg = "-guid option can NOT be used with -delete option.Please specify either of them.";
            inm_printf("%s\n", errmsg.c_str());
            hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
            ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
            ADD_TO_VACP_FAIL_MSGS(VACP_E_INVALID_COMMAND_LINE, errmsg, hr);
            goto end;
        }
    }
    
    if ((CmdRequest.DoNotIncludeWriters == true) && (gbSystemLevelTag))
    {
        errmsg = "-x and -systemlevel options are mutually exclusive. Please specify either of them.";
        inm_printf("%s\n", errmsg.c_str());
        hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
        ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
        ADD_TO_VACP_FAIL_MSGS(VACP_PARSE_COMMAND_LINE_ARGS_FAILED, errmsg, hr);
        goto end;
    }
    
    if((!CmdRequest.bSyncTag) && CmdRequest.bTagTimeout)
    {
        errmsg = "-tagtimeout option can NOT be used without -sync option.";
        inm_printf("%s \n", errmsg.c_str());
        hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
        ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
        ADD_TO_VACP_FAIL_MSGS(VACP_PARSE_COMMAND_LINE_ARGS_FAILED, errmsg, hr);
        goto end;
    }

    
    if(CmdRequest.bProvider)
    {
        VSS_ID uuidProviderID = {0};
        if(UuidFromString((unsigned char*)CmdRequest.strProviderID.c_str(),(UUID*)&uuidProviderID)== RPC_S_INVALID_STRING_UUID)
        {
            std::stringstream ss;
            ss << "An invalid provider ID " << CmdRequest.strProviderID.c_str() << " passed.";
            inm_printf("\n==============================================\n");
            inm_printf("ERROR:%s.\n", ss.str().c_str());
            inm_printf("==============================================\n");
            hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
            ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
            ADD_TO_VACP_FAIL_MSGS(VACP_PARSE_COMMAND_LINE_ARGS_FAILED, ss.str(), hr);
            goto end;
        }
    }

    if((CmdRequest.bVerify) && (CmdRequest.bTag))
    {
        errmsg = "-verify and -t options cannot be used together as they both conflict each other.";
        inm_printf("%s \n", errmsg.c_str());
        hr = VACP_E_INVALID_COMMAND_LINE;//E_FAIL;
        ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
        ADD_TO_VACP_FAIL_MSGS(VACP_PARSE_COMMAND_LINE_ARGS_FAILED, errmsg, hr);
        goto end;
    }

end:
    return hr;
}

#if (_WIN32_WINNT > 0x500)
HRESULT VssAppConsistencyProvider::CreateInMageVssProviderRegEntries()
{
    HRESULT hr = S_OK;
    do
    {
        //Create INMAGE_VSS_PROVIDER_KEY subkey.
        LONG lResCreateInMageRegKey = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                                            INMAGE_VSS_PROVIDER_KEY,
                                            0,
                                            0,
                                            0,
                                            KEY_ALL_ACCESS,
                                            0,
                                            (PHKEY)&hInMageRegKey,
                                            0);
        if(ERROR_SUCCESS != lResCreateInMageRegKey)
        {
            inm_printf("\nCould not create the required registry entries! Error Code = %u",lResCreateInMageRegKey);
            hr = E_FAIL;
            break;
        }

        //Create EventViewerLoggingFlag in the registry
        DWORD dwEventViewerLoggingFlag = 0;
        LSTATUS lStatus = RegSetValueEx(hInMageRegKey,
                                        INMAGE_EVENTVIEWER_LOGGING_KEY,
                                        0,
                                        REG_DWORD,
                                        (LPBYTE)&dwEventViewerLoggingFlag,
                                        sizeof(DWORD));
        if(ERROR_SUCCESS != lStatus)
        {
            inm_printf("\n Error in setting the EventViewerLogginigFlag registry entry. Error Code = %u\n",lStatus);
            hr = E_FAIL;
            break;
        }

        //Create INMAGE_VSS_PROVIDER_REMOTE_KEY sub key
        LONG lResCreateInMageRemoteSubKey = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                                                        INMAGE_VSS_PROVIDER_REMOTE_KEY,
                                                        0,
                                                        0,
                                                        0,
                                                        KEY_ALL_ACCESS,
                                                        0,
                                                        (PHKEY)&hInMageRemoteSubKey,
                                                        0);
        if(ERROR_SUCCESS != lResCreateInMageRemoteSubKey)
        {
            inm_printf("\nCould not create/set \"Remote \" sub key in the registry.Error Code = %u",lResCreateInMageRemoteSubKey);
            hr = E_FAIL;
            break;
        }


        //Create INMAGE_VSS_PROVIDER_LOCAL_KEY sub key
        LONG lResCreateInMageLocalSubKey = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                                                        INMAGE_VSS_PROVIDER_LOCAL_KEY,
                                                        0,
                                                        0,
                                                        0,
                                                        KEY_ALL_ACCESS,
                                                        0,
                                                        &hInMageLocalSubKey,
                                                        0);
        if(ERROR_SUCCESS != lResCreateInMageLocalSubKey)
        {
            inm_printf("\nCould not create/set \"Local \" sub key in the registry.Error Code = %u",lResCreateInMageLocalSubKey);
            hr = E_FAIL;
            break;
        }
    }while(FALSE);

    if(hr != S_OK)
    {
        if(hInMageRemoteSubKey)
        {
            RegCloseKey(hInMageRemoteSubKey);
            hInMageRemoteSubKey = NULL;
        }
        if(hInMageLocalSubKey)
        {
            RegCloseKey(hInMageLocalSubKey);
            hInMageLocalSubKey = NULL;
        }
        if(hInMageRegKey)
        {
            RegCloseKey(hInMageRegKey);
            hInMageRegKey = NULL;
        }
    }
    return hr;
}
HRESULT VssAppConsistencyProvider::IsItRemoteTagIssue(USHORT uRemoteSend)
{
    HRESULT hr = S_OK;
    //Set Values for RemoteTagIssue	
    DWORD uRemote = 0;
    LSTATUS lStatus = RegSetValueEx(hInMageRegKey,INMAGE_REMOTE_TAG_ISSUE,0,REG_DWORD,(LPBYTE)&uRemote,sizeof(DWORD));
    if(ERROR_SUCCESS != lStatus)
    {
        inm_printf("\nError in setting the RemoteTagIssue value in the registry.Error Code = %u\n",lStatus);
        hr = E_FAIL;
    }	
    return hr;
}
HRESULT VssAppConsistencyProvider::IsItSyncTagIssue(USHORT uSyncTag)
{
    HRESULT hr = S_OK;
    //Set Values for SyncTagIssue
    DWORD uSync = 0;
    LSTATUS lStatus = RegSetValueEx(hInMageRegKey,INMAGE_SYNC_TAG_ISSUE,0,REG_DWORD,(LPBYTE)&uSync,sizeof(DWORD));
    if(ERROR_SUCCESS != lStatus)
    {
        inm_printf("\nError in setting the sync tag issue status in the registry.Error Code = %u",lStatus);
        hr = E_FAIL;
    }
    return hr;
}
HRESULT VssAppConsistencyProvider::EstablishRemoteConInfoForInMageVssProvider(USHORT uPort,
                                                                              const unsigned char* serverIp)
{
    HRESULT hr = S_OK;
    do
    {
        //Set Values under the subkey Remote for PortNo
        DWORD uPortNo = (DWORD)uPort;
        LSTATUS lStatus = RegSetValueEx(hInMageRemoteSubKey,INMAGE_REMOTE_TAG_PORT_NO,0,REG_DWORD,(LPBYTE)&uPortNo,sizeof(DWORD));
        if(ERROR_SUCCESS != lStatus)
        {
            inm_printf("\nCould not create/set \"Port No \" value in the registry.Error Code = %u",lStatus);
            hr = E_FAIL;
            break;
        }
        //Set Values under the subkey Remote for ServerIP
        //lStatus = RegSetValueEx(hInMageRemoteSubKey,"Server Ip",0,REG_SZ,(LPBYTE)(strServerIp.c_str()),strlen(strServerIp.c_str())+1);
        lStatus = RegSetValueEx(hInMageRemoteSubKey,
                                INMAGE_REMOTE_TAG_SERVER_IP,
                                0,
                                REG_SZ,
                                (LPBYTE)(serverIp),
                                (DWORD)(size_t)strlen((const char*)serverIp)+1);
        if(ERROR_SUCCESS != lStatus)
        {
            inm_printf("\nCould not create/set \"Server Ip \" value in the registry.Error Code = %u",lStatus);
            hr = E_FAIL;
            break;
        }
    }while(FALSE);
    
    return hr;
}
HRESULT VssAppConsistencyProvider::StoreRemoteTagsBuffer(unsigned char* TagDataBuffer,
                                                         ULONG ulTagBufferLen)
{
    HRESULT hr = S_OK;
    do
    {
        //Set the Remote Tags buffer to send the tags using socket by the InMageVssProvider.
        LSTATUS lStatus = RegSetValueEx(hInMageRemoteSubKey,"Buffer",0,REG_BINARY,(LPBYTE)TagDataBuffer,ulTagBufferLen);
        if(ERROR_SUCCESS != lStatus)
        {
            inm_printf("\nError in storing the remote tags binary buffer data in the registry.Error Code = %u",lStatus);
            hr = E_FAIL;
            break;
        }
        //Set Values under the subkey Remote for BufLen
        unsigned long ulBufLen = ulTagBufferLen;
        lStatus = RegSetValueEx(hInMageRemoteSubKey,"BufferLen",0,REG_DWORD,(PBYTE)&ulBufLen,sizeof(DWORD));
        if(ERROR_SUCCESS != lStatus)
        {
            inm_printf("\nError in storing the remote tags length of buffer in the registry.Error Code = %u",lStatus);
            hr = E_FAIL;
            break;
        }
    }while(FALSE);
    return hr;
}
HRESULT VssAppConsistencyProvider::StoreLocalTagsBuffer(unsigned char* TagDataBuffer, 
                                                        ULONG ulTagBufferLen,
                                                        ULONG ulOutBufLen)
{
    HRESULT hr = S_OK;
    do
    {		
        //Set Values under the subkey Local for Buffer
        LSTATUS lStatus = RegSetValueEx(hInMageLocalSubKey,"Buffer",0,REG_BINARY,(LPBYTE)TagDataBuffer,ulTagBufferLen);
        if(ERROR_SUCCESS != lStatus)
        {
            inm_printf("\nError in storing the binary buffer data in the registry.Error Code = %u",lStatus);
            hr = E_FAIL;
            break;
        }
        //Set Values under the subkey Local for BufLen
        lStatus = RegSetValueEx(hInMageLocalSubKey,"BufferLen",0,REG_DWORD,(PBYTE)&ulTagBufferLen,sizeof(DWORD));
        if(ERROR_SUCCESS != lStatus)
        {
            inm_printf("\nError in storing the length of buffer in the registry.Error Code = %u",lStatus);
            hr = E_FAIL;
            break;
        }
        //Set Values under the subkey Local for OutBufLen		
        lStatus = RegSetValueEx(hInMageLocalSubKey,"OutBufferLen",0,REG_DWORD,(PBYTE)&ulOutBufLen,sizeof(DWORD));
        if(ERROR_SUCCESS != lStatus)
        {
            inm_printf("\nError in storing the length of buffer in the registry.Error Code = %u",lStatus);
            hr = E_FAIL;
            break;
        }
    }while(FALSE);
    return hr;
}
HRESULT VssAppConsistencyProvider::IsTagIssuedSuccessfullyDuringCommitTime(unsigned long* ulTagSent)
{
    HRESULT hr = S_OK;
    do
    {
        LSTATUS lStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                       INMAGE_VSS_PROVIDER_KEY,
                                       0,
                                       KEY_READ|KEY_WOW64_64KEY,
                                       (PHKEY)&hInMageRegKey);
        if(ERROR_SUCCESS != lStatus)
        {
            inm_printf("\nError in reading the buffer from the registry.Error Code = %u",lStatus);
            hr = E_FAIL;
            break;
        }
        else
        {
            //Get the value of the Tag Reached Status Key
            DWORD dwTagReachedStatus = 0;
            DWORD dwType = REG_DWORD;
            DWORD dwOutTagReachedStatus = sizeof(DWORD);
            
            lStatus = RegQueryValueEx(hInMageRegKey,
                                      INMAGE_TAG_REACHED_STATUS,
                                      0,
                                      &dwType,
                                      (LPBYTE)&dwTagReachedStatus,
                                      (LPDWORD)&dwOutTagReachedStatus);
            if(ERROR_SUCCESS != lStatus)
            {
                inm_printf("\nError in reading the buffer from the registry.Error Code = %u\n",lStatus);
                hr = E_FAIL;
                break;
            }
            else
            {
                *ulTagSent = dwTagReachedStatus;
            }
        }
    }while(FALSE);
    return hr;
}

#if (_WIN32_WINNT > 0x500)

HRESULT GetInMageVssProviderInstance()
{
    HRESULT hr = S_OK;

    if (pInMageVssProvider == NULL)
    {
        hr = CoCreateInstance(CLSID_VacpProvider,
            0,
            CLSCTX_LOCAL_SERVER,
            IID_IVacpProvider,
            (void**)&pInMageVssProvider);

        if (hr != S_OK)
        {
            DebugPrintf("\nVssProvider is either not installed or the service is not running! . hr = 0x%08x \n", hr);
            //std::stringstream errorCode;
            //errorCode << std::hex << hr;
            std::map<std::string, std::string> errors;
            //errors.insert({ AgentHealthIssueCodes::VMLevelHealthIssues::VssProviderMissing::HealthCode, errorCode.str() });
            std::string strTagGuid = AppConsistency::Get().m_vacpErrorInfo.get(VacpAttributes::TAG_GUID);
            AppConsistency::Get().m_tagTelInfo.Add(TagGuidKey, strTagGuid);
            AppConsistency::Get().m_vacpErrorInfo.Add(AgentHealthIssueCodes::VMLevelHealthIssues::VssProviderMissing::HealthCode, errors);

            //Adding VSS Provider error to Telemetry
            AppConsistency::Get().m_tagTelInfo.Add(VssProviderError, std::string("ASR VSS Provider is not installed."));

            return hr;
        }
    }

    return hr;
}

HRESULT RegisterInMageVssProvider()
{
    HRESULT hr = S_OK;
    hr = GetInMageVssProviderInstance();
    if (hr != S_OK)
        return hr;

    USHORT flags = VACP_FLAGS_REGISTER_VSS_PROVIDER;
    hr = RegisterWithVss(flags);
    if (hr != S_OK)
    {
        std::stringstream streamVssRegError;
        streamVssRegError << std::string("\nUnable to register ASR VSS Provider(VSS_E_PROVIDER_NOT_REGISTERED). VSS Provider Error = 0x");
        streamVssRegError << std::hex << hr;
        inm_printf(streamVssRegError.str().c_str());
        std::string strTagGuid = AppConsistency::Get().m_vacpErrorInfo.get(VacpAttributes::TAG_GUID);
        AppConsistency::Get().m_tagTelInfo.Add(TagGuidKey, strTagGuid);
        AppConsistency::Get().m_tagTelInfo.Add(VssProviderError, streamVssRegError.str());
        std::map<std::string, std::string> errors;
        AppConsistency::Get().m_vacpErrorInfo.Add(AgentHealthIssueCodes::VMLevelHealthIssues::VssProviderMissing::HealthCode, errors);
    }

    return hr;
}


HRESULT UnregisterInMageVssProvider()
{
    HRESULT hr = S_OK;
 
    USHORT flags = VACP_FLAGS_UNREGISTER_VSS_PROVIDER;
    hr = RegisterWithVss(flags);
    if (hr != S_OK)
    {
        inm_printf("\nUnable to unregister Provider with VSS. hr = 0x%08x \n", hr);
    }

    return hr;
}


HRESULT VssAppConsistencyProvider::HandleInMageVssProvider()
{
    HRESULT hr = S_OK;
    DWORD dwThreadId;
    std::string errmsg;

    if (!m_hVssProviderCallbackThread)
    {
        DWORD dwThreadId;
        //Create a thread to listen continously and accept connections
        m_hVssProviderCallbackThread = CreateThread(0,
            0,
            (LPTHREAD_START_ROUTINE)ProviderCommunicationHandler,
            NULL,
            0,
            &dwThreadId);

        if (m_hVssProviderCallbackThread == NULL)
        {
            DWORD dwErr = GetLastError();
            errmsg = "Failed to create a thread to communicate with provider.";
            inm_printf("\n%s Error = %d\n", errmsg.c_str(), dwErr);
            g_vacpLastError.m_errMsg = errmsg.c_str();
            g_vacpLastError.m_errorCode = dwErr;
            return dwErr;
        }

        XDebugPrintf("Vss Provider Callback Thread Id %d.\n", dwThreadId);
    }
    else
    {
        XDebugPrintf("Vss Provider Callback is already setup %ld.\n", m_hVssProviderCallbackThread);
    }


    hr = RegisterInMageVssProvider();
    if (SUCCEEDED(hr))
    {
        VSS_ID snapshotset = m_vssRequestorPtr->GetCurrentSnapshotSetId();
        hr = pInMageVssProvider->RegisterSnapshotSetId(snapshotset);
        if (!SUCCEEDED(hr))
        {
            errmsg = "Unable to access RegisterSnapshotSetId method from the InMageVssProvider's COM interface.";
            inm_printf("\n %s hr = 0x%08x \n", errmsg.c_str(), hr);
            AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg.c_str();
            AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
            return hr;
        }

        if (gbVerify)
        {
            hr = pInMageVssProvider->SetVerifyMode();
            if (!SUCCEEDED(hr))
            {
                errmsg = "Unable to set the InMageVssProvider into verify mode.";
                inm_printf("\n %s hr = 0x%08x \n", errmsg.c_str(), hr);
                AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg.c_str();
                AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
                return hr;
            }
        }
    }
    else
    {
        errmsg = "RegisterInMageVssProvider() failed .";
        inm_printf("\n %s hr = 0x%08x \n", errmsg.c_str(), hr);
        AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg.c_str();
        AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
        return hr;
    }

    
    return hr;
}

RPC_STATUS VssAppConsistencyProvider::StopRpcServer()
{
    XDebugPrintf("%s: ENTERED\n", __FUNCTION__);

    RPC_STATUS status;

    status = RpcMgmtStopServerListening(NULL);
    if (status)
    {
        inm_printf("Error: RpcMgmtStopServerListening failed. Status %d\n", status);
        return(status);
    }

    status = RpcServerUnregisterIf(vacprpc_v1_0_s_ifspec, NULL, FALSE);
    if (status)
    {
        inm_printf("Error: RpcServerUnregisterIf failed. Status %d\n", status);
        return(status);
    }

    if (m_hVssProviderCallbackThread)
    {
        XDebugPrintf("%s: Waiting for the VSS Provider callback thread to quit.\n", __FUNCTION__);
        WaitForSingleObject(m_hVssProviderCallbackThread, INFINITE);
        CloseHandle(m_hVssProviderCallbackThread);
        m_hVssProviderCallbackThread = NULL;

        XDebugPrintf("%s: VSS Provider callback thread has quit.\n", __FUNCTION__);
    }

    XDebugPrintf("%s: EXITED\n", __FUNCTION__);

    return status;
}

HRESULT StopLongRunningInMageVssProvider(ULONG ulMaxWaitTimeInSec)
{
    const std::string serviceName = "Azure Site Recovery VSS Provider";
    const unsigned int MAX_SERVICE_STOP_ATTEMPTS = 5;
    HRESULT hr = S_OK;
    DWORD dwErr = S_OK;
    SC_HANDLE hSCManager = NULL, hService = NULL;
    HANDLE hProc = NULL;

    do {
        hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == hSCManager)
        {
            dwErr = GetLastError();
            DebugPrintf("Failed to get handle to Windows Services Manager in OpenSCMManager %d.\n", dwErr);
            break;
        }

        SERVICE_STATUS_PROCESS  serviceProcessStatus;
        SERVICE_STATUS serviceStatus;
        DWORD actualSize = 0, dwRet = 0;
        hService = OpenService(hSCManager,
            serviceName.c_str(),
            SERVICE_ALL_ACCESS);
        if (NULL == hService)
        {
            dwErr = GetLastError();
            DebugPrintf("OpenService() failed to get handle to %s with error %d.\n",
                serviceName.c_str(),
                dwErr);
            break;
        }

        if (!QueryServiceStatusEx(hService,
            SC_STATUS_PROCESS_INFO,
            (LPBYTE)&serviceProcessStatus,
            sizeof(serviceProcessStatus),
            &actualSize))
        {
            dwErr = GetLastError();
            DebugPrintf("QueryServiceStatusEx() failed to get status for %s with error %d.\n",
                serviceName.c_str(),
                dwErr);
            break;
        }

        if (SERVICE_STOPPED == serviceProcessStatus.dwCurrentState)
        {
            break;
        }

        ULARGE_INTEGER uliStartTime, uliCurrTime;
        FILETIME startTime, exitTime, kernelTime, userTime;
        hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, serviceProcessStatus.dwProcessId);
        if (NULL == hProc)
        {
            dwErr = GetLastError();
            DebugPrintf("OpenProcess() failed for PID %d with error %d.\n",
                serviceProcessStatus.dwProcessId,
                dwErr);
            break;
        }

        if (!GetProcessTimes(hProc, &startTime, &exitTime, &kernelTime, &userTime))
        {
            dwErr = GetLastError();
            DebugPrintf("GetProcessTimes() failed for PID %d with error %d.\n",
                serviceProcessStatus.dwProcessId,
                dwErr);
            break;
        }

        uliStartTime.HighPart = startTime.dwHighDateTime;
        uliStartTime.LowPart = startTime.dwLowDateTime;

        SYSTEMTIME systemTime;
        GetSystemTime(&systemTime);

        FILETIME curTime;
        SystemTimeToFileTime(&systemTime, &curTime);
        uliCurrTime.LowPart = curTime.dwLowDateTime;
        uliCurrTime.HighPart = curTime.dwHighDateTime;

        ULARGE_INTEGER uliElapsedTime;
        uliElapsedTime.QuadPart = uliCurrTime.QuadPart - uliStartTime.QuadPart;
       
        // convert to sec from 100-nano sec
        uliElapsedTime.QuadPart /= 10000000;
        DebugPrintf("Elapsed time for VSS Prov service %lld.\n", uliElapsedTime.QuadPart);

        if ((uliElapsedTime.QuadPart + 1) < (ulMaxWaitTimeInSec ? ulMaxWaitTimeInSec : VSS_PROV_DELAYED_STOP_WAIT_TIME))
        {
            break;
        }

        int nAttempts = MAX_SERVICE_STOP_ATTEMPTS;

        do {
            if (ControlService(hService, SERVICE_CONTROL_STOP, &serviceStatus))
            {
                DebugPrintf("Waiting for the service %s to stop.\n", serviceName.c_str());
                int retry = 1, nRetries = 5;

                do {
                    Sleep(1000);
                } while (QueryServiceStatusEx(hService,
                        SC_STATUS_PROCESS_INFO,
                        (LPBYTE)&serviceProcessStatus,
                        sizeof(serviceProcessStatus),
                        &actualSize) &&
                    SERVICE_STOPPED != serviceProcessStatus.dwCurrentState &&
                    retry++ <= nRetries);
            }

            if (QueryServiceStatusEx(hService,
                SC_STATUS_PROCESS_INFO,
                (LPBYTE)&serviceProcessStatus,
                sizeof(serviceProcessStatus),
                &actualSize) &&
                SERVICE_STOPPED == serviceProcessStatus.dwCurrentState)
            {
                DebugPrintf("Successfully stopped the service: %s.\n", serviceName.c_str());
                break;
            }

            nAttempts--;
            if (nAttempts)
                DebugPrintf("Retrying to stop service %s \n", serviceName.c_str());

        } while (nAttempts > 0);


        if (SERVICE_STOPPED != serviceProcessStatus.dwCurrentState)
        {
            DebugPrintf("Terminating process with id %d as the service did not stop.\n",
                serviceProcessStatus.dwProcessId);

            BOOL bResult = TerminateProcess(hProc, ERROR_VACP_TERMINATING_VSS_PROVIDER_EXIT_CODE);
            if (!bResult)
            {
                dwErr = GetLastError();
                DebugPrintf("TerminateProcess() for process id %d failed with error  %d.\n",
                    serviceProcessStatus.dwProcessId,
                    dwErr);
                break;
            }

            const DWORD dwWaitTime = 30000; // milli secs
            DWORD dwWaitResult = WaitForSingleObject(hProc, dwWaitTime);
            if (WAIT_TIMEOUT == dwWaitResult)
            {
                DebugPrintf("Process not terminated even after %d ms.\n", dwWaitTime);
            }
        }

    } while (false);

    if (hService)
        CloseServiceHandle(hService);
    if (hSCManager)
        CloseServiceHandle(hSCManager);
    if (hProc)
        CloseHandle(hProc);

    hr = HRESULT_FROM_WIN32(dwErr);

    return hr;
}

#endif

HRESULT InitializeCom()
{
    HRESULT hr=S_OK;
    std::string errmsg;

    XDebugPrintf("ENTERED: InitializeCom\n");

    do
    {
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

        if (hr != S_OK)
        {
            errmsg = "CoInitializeEx() failed.";
            DebugPrintf("FAILED: %s. hr = 0x%08x \n.", errmsg.c_str(), hr);
            break;
        }

        hr = CoInitializeSecurity
            (
            NULL,                                
            -1,                                  
            NULL,                                
            NULL,                                
            RPC_C_AUTHN_LEVEL_CONNECT,           
            RPC_C_IMP_LEVEL_IMPERSONATE,         
            NULL,                                
            EOAC_NONE,                           
            NULL                                 
            );
    

        // Check if the securiy has already been initialized by someone in the same process.
        if (hr == RPC_E_TOO_LATE)
        {
            hr = S_OK;
        }

        if (hr != S_OK)
        {
            errmsg = "CoInitializeSecurity() failed.";
            DebugPrintf("FAILED: %s. hr = 0x%08x \n.", errmsg.c_str(), hr);
            CoUninitialize();
            break;
        }

    } while(FALSE);

    XDebugPrintf("EXITED: InitializeCom\n");
    return hr;
}

void UninitializeCom()
{
    CoUninitialize();
}

HRESULT VssAppConsistencyProvider::PrepareForRemoteServer()
{
    HRESULT hr = S_OK;
    XDebugPrintf("ENTERED: VssAppConsistencyProvider::PrepareForRemoteServer\n");

    if(gbDoNotIncludeWriters)
    {
        if(gbRemoteSend)
        {
            ULONG ulStartTime  = 0;
            ULONG ulEndTime  = 0;
            SYSTEMTIME startSystime = {0};
            SYSTEMTIME endSystime = {0};
            FILETIME startFiletime = {0};
            FILETIME endFiletime = {0};
            if(!gbRemoteConnection)
            {
                GetLocalTime(&startSystime);
                SystemTimeToFileTime(&startSystime,&startFiletime);
                if(EstablishRemoteConnection(m_ACSRequest.usServerPort, m_ACSRequest.strServerIP.c_str())== S_OK)
                {
                    GetLocalTime(&endSystime);
                    SystemTimeToFileTime(&endSystime,&endFiletime);		
                    ULARGE_INTEGER  real_time;
                    real_time.HighPart = endFiletime.dwHighDateTime - startFiletime.dwHighDateTime;
                    real_time.LowPart = endFiletime.dwLowDateTime - startFiletime.dwLowDateTime;
                    XDebugPrintf("Elapsed time for Establishing connection: %f milli seconds\n", (double)real_time.QuadPart/10000000 );
                }
                else
                {
                    DebugPrintf("\nCan not continue...\n");
                    hr = E_FAIL;
                }
            }
        }
    }
    XDebugPrintf("EXITED: VssAppConsistencyProvider::Prepare\n");
    return hr;
}

HRESULT VssAppConsistencyProvider::Prepare(CLIRequest_t CmdRequest)
{
    HRESULT hr = S_OK;
    std::string errmsg;
    XDebugPrintf("ENTERED: VssAppConsistencyProvider::Prepare\n");

    do
    {
        if ((m_dwProtectedDriveMask == 0) && (gbDeleteSnapshotSets == false)) //m_dwProtectedDriveMask is same as m_ACSRequest.volumeBitMask
        {
          bool bHasValidComponent = false;
          std::vector<AppSummary_t>::iterator iter = m_ACSRequest.Applications.begin();
          while(iter != m_ACSRequest.Applications.end())
          {
            if(true == iter->bHasValidComponent)
            {
              bHasValidComponent = true;
              break;
            }
            iter++;
          }
          if(false == bHasValidComponent)
          {
            hr = E_FAIL;
            errmsg = "No ValidComponent present.";
            DebugPrintf("%s.\n", errmsg.c_str());
            AppConsistency::Get().AddToVacpFailMsgs(VACP_E_PREPARE_PHASE_FAILED, errmsg, hr);
            break;
          }
        }

        if (!gbUseInMageVssProvider)
        {
            //Initialize the writer first
            vwriter = new (std::nothrow) InMageVssWriter(m_ACSRequest.volMountPoint_info, m_ACSRequest.volumeBitMask, m_ACSRequest.Applications, IsInitialized());

            if (vwriter == NULL)
            {
                hr = E_FAIL;
                errmsg = "failed to create InMage VSS Writer.";
                DebugPrintf("ERROR: %s.\n", errmsg.c_str());
                AppConsistency::Get().AddToVacpFailMsgs(VACP_E_PREPARE_PHASE_FAILED, errmsg, hr);
                break;
            }

            if (gbRemoteSend)
            {
                ULONG ulStartTime = 0;
                ULONG ulEndTime = 0;
                SYSTEMTIME startSystime = { 0 };
                SYSTEMTIME endSystime = { 0 };
                FILETIME startFiletime = { 0 };
                FILETIME endFiletime = { 0 };
                if (!gbRemoteConnection)
                {
                    GetLocalTime(&startSystime);
                    SystemTimeToFileTime(&startSystime, &startFiletime);
                    if (EstablishRemoteConnection(m_ACSRequest.usServerPort, m_ACSRequest.strServerIP.c_str()) == S_OK)
                    {
                        GetLocalTime(&endSystime);
                        SystemTimeToFileTime(&endSystime, &endFiletime);
                        ULARGE_INTEGER  real_time;
                        real_time.HighPart = endFiletime.dwHighDateTime - startFiletime.dwHighDateTime;
                        real_time.LowPart = endFiletime.dwLowDateTime - startFiletime.dwLowDateTime;
                        XDebugPrintf("Elapsed time for Establishing connection: %f milli seconds\n", (double)real_time.QuadPart / 10000000);
                    }
                    else
                    {
                        errmsg = "VssAppConsistencyProvider::Prepare: EstablishRemoteConnection failed. Can not continue...";
                        DebugPrintf("\n%s \n", errmsg.c_str());
                        AppConsistency::Get().AddToVacpFailMsgs(VACP_E_PREPARE_PHASE_FAILED, errmsg, E_FAIL);
                        hr = E_FAIL;
                        break;
                    }
                }
            }
            
            vwriter->RegisterCallBackHandler(GenericCallBackHandler, (void *)this);

            bool bStatus = vwriter->Initialize();

            if (bStatus != true)
            {
                DebugPrintf("FAILED: Initialize() failed. driveMask = 0x%x \n", m_dwProtectedDriveMask);
                DeleteWriter();
                AppConsistency::Get().AddToVacpFailMsgs(VACP_E_PREPARE_PHASE_FAILED, AppConsistency::Get().m_vacpLastError.m_errMsg, AppConsistency::Get().m_vacpLastError.m_errorCode);
                hr = E_FAIL;
                break;
            }
        }

        ////Initialize the requestor
        //vrequestor = new (std::nothrow) InMageVssRequestor(m_ACSRequest, IsInitialized());

        //if (vrequestor == NULL)
        //{
        //    hr = E_FAIL;
        //    errmsg = "failed to create InMage VSS Requester.";
        //    DebugPrintf("ERROR:%s\n", errmsg.c_str());
        //    AppConsistency::Get().AddToVacpFailMsgs(VACP_E_PREPARE_PHASE_FAILED, errmsg, E_FAIL);
        //    DeleteWriter();
        //    break;
        //}

        /*
        hr = vrequestor->DiscoverWritersTobeEnabled(CmdRequest);// MCHECK, I don't think this call is needed as is being called later anyway.Observation: If we comment this block, then only InMageVssWriter is coming, other writers are not coming.
        if (hr != S_OK)
        {
            std::stringstream ss;
            ss << "DiscoverWritersTobeEnabled failed. driveMask = " << std::hex << m_dwProtectedDriveMask;
            DebugPrintf("FAILED: %s, hr = 0x%x\n", ss.str().c_str(), hr);
            ss << " " << AppConsistency::Get().m_vacpLastError.m_errMsg;
            AppConsistency::Get().AddToVacpFailMsgs(VACP_E_PREPARE_PHASE_FAILED, ss.str(), AppConsistency::Get().m_vacpLastError.m_errorCode);
            DeleteWriter();
            DeleteRequestor();
            break;
        }
        */

        hr = m_vssRequestorPtr->PrepareVolumesForConsistency(CmdRequest, this->m_bDoFullBackup);
        if (hr != S_OK)
        {
            std::stringstream ss;
            ss << "PrepareVolumesForConsistency failed. driveMask = " << std::hex << m_dwProtectedDriveMask;
            DebugPrintf("FAILED: %s, hr = 0x%x\n", ss.str().c_str(), hr);
            ss << " " << AppConsistency::Get().m_vacpLastError.m_errMsg;
            
            if (AppConsistency::Get().m_vacpLastError.m_errMsg.empty() ||
                (AppConsistency::Get().m_vacpLastError.m_errorCode == 0))
            {
                AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
                ss << "hr = 0x" << std::hex << hr;
            }

            AppConsistency::Get().AddToVacpFailMsgs(VACP_E_PREPARE_PHASE_FAILED, ss.str(), AppConsistency::Get().m_vacpLastError.m_errorCode);
            DeleteWriter();
            DeleteRequestor();
            break;
        }


    } while (FALSE);

    XDebugPrintf("EXITED: VssAppConsistencyProvider::Prepare\n");

    return hr;
}

HRESULT VssAppConsistencyProvider::Quiesce()
{
    XDebugPrintf("ENTERED: VssAppConsistencyProvider::Quiesce\n");

    HRESULT hr = S_OK;
    do
    {
        if ((m_dwProtectedDriveMask == 0) &&(gbDeleteSnapshotSets == false))
        {
          bool bHasValidComponent = false;
          std::vector<AppSummary_t>::iterator iter = m_ACSRequest.Applications.begin();
          while(iter != m_ACSRequest.Applications.end())
          {
            if(true == iter->bHasValidComponent)
            {
              bHasValidComponent = true;
              break;
            }
            iter++;
          }
          if(false == bHasValidComponent)
          {
            hr = E_FAIL;
            std::string errmsg = "No ValidComponent present.";
            DebugPrintf("%s.\n", errmsg.c_str());
            AppConsistency::Get().AddToVacpFailMsgs(VACP_E_QUIESCE_PHASE_FAILED, errmsg, hr);
            break;
          }
        }

        if ((!gbUseInMageVssProvider && vwriter == NULL) || m_vssRequestorPtr.get() == NULL)
        {
            hr = E_FAIL;
            break;
        }

        hr = m_vssRequestorPtr->FreezeVolumesForConsistency();
        if (S_OK != hr)
        {
            AppConsistency::Get().AddToVacpFailMsgs(VACP_E_QUIESCE_PHASE_FAILED, AppConsistency::Get().m_vacpLastError.m_errMsg, AppConsistency::Get().m_vacpLastError.m_errorCode);
        }

    } while(FALSE);

    XDebugPrintf("EXITED: VssAppConsistencyProvider::Quiesce\n");

    return hr;
}

HRESULT VssAppConsistencyProvider::Resume()
{
    USES_CONVERSION;
    XDebugPrintf("ENTERED: VssAppConsistencyProvider::Resume\n");

    HRESULT hr = S_OK;

    do
    {
        if ((m_dwProtectedDriveMask == 0) && (gbDeleteSnapshotSets == false))
        {
            hr = E_FAIL;
            break;
        }

        if ((!gbUseInMageVssProvider && vwriter == NULL) || m_vssRequestorPtr.get() == NULL)
        {
            hr = E_FAIL;
            break;
        }
        if(gbDeleteSnapshotSets)
        {
            hr = m_vssRequestorPtr->ProcessDeleteSnapshotSets();
            if(hr != S_OK)
            {
                inm_printf("\nEncountered error while deleting the snapshots in the snapshotset.\n");
                AppConsistency::Get().AddToVacpFailMsgs(VACP_E_RESUME_PHASE_FAILED, AppConsistency::Get().m_vacpLastError.m_errMsg, AppConsistency::Get().m_vacpLastError.m_errorCode);
                hr = m_vssRequestorPtr->CleanupSnapShots();
            }
            else
            {
                inm_printf("\nSuccessfully deleted all the snapshots available in the system.\n");
            }
        }
        else if(!gbPersistSnapshots)
        {	
            hr = m_vssRequestorPtr->CleanupSnapShots();
        }
        else
        {
            hr = ProcessExposingSnapshots();
        }		
    } while(FALSE);

    XDebugPrintf("EXITED: VssAppConsistencyProvider::Resume\n");

    return hr;
}

HRESULT VssAppConsistencyProvider::Cancel()
{
    XDebugPrintf("ENTERED: VssAppConsistencyProvider::Cancel\n");

    HRESULT hr = S_OK;

    do
    {
        if ((m_dwProtectedDriveMask == 0) && (gbDeleteSnapshotSets == false))
        {
            hr = E_FAIL;
            break;
        }

        DeleteWriter();
        DeleteRequestor();

    } while(FALSE);

    XDebugPrintf("EXITED: VssAppConsistencyProvider::Cancel\n");

    return hr;
}


HRESULT  VssAppConsistencyProvider::SendTagsToDriver()
{
    return AppConsistency::Get().SendTagsToDriver(this->m_vssRequestorPtr->GetACSRequest());
}

HRESULT  VssAppConsistencyProvider::SendTagsToDiskDriver()
{
    return AppConsistency::Get().SendTagsToDiskDriver(this->GetACSRequest());
}
HRESULT VssAppConsistencyProvider::SendRevocationTagsToDriver()
{
    return AppConsistency::Get().SendRevocationTagsToDriver(this->m_vssRequestorPtr->GetACSRequest());
}

void GenericCallBackHandler(void *context)
{
    HRESULT hr = S_OK;
    XDebugPrintf("Entered GenericCallBackHandler() \n");

    VssAppConsistencyProvider *vprovider;

    vprovider = (VssAppConsistencyProvider *)context;

    ACSRequest_t Request = vprovider->GetACSRequest();

    if(gbIssueTag)
    {
        if(!gbUseInMageVssProvider)
        {
            if(gbRemoteSend)
            {
                //Send tags to the remote server(CX server)
                hr = vprovider->SendTagsToRemoteServer(Request);	
            }
            else
            {
                //Send tags to the involflt driver
                hr = vprovider->SendTagsToDriver();
            }
        }
    }
    else
    {
        gbTagSend = true;
    }
    if(!gbSyncTag)
    {
        if (hr != S_OK)
        {
            inm_printf("\n SetApplicationConsistencyState(false)\n");
            vprovider->SetApplicationConsistencyState(false);
        }
        else
        {
            vprovider->SetApplicationConsistencyState(true);
            DebugPrintf("\n SetApplicationConsistencyState(true).\n");
        }
    }
    XDebugPrintf("Exited GenericCallBackHandler() \n");
}
#endif

bool DriveLetterToGUID(char DriveLetter, WCHAR *VolumeGUID, ULONG ulBufLen)
{
    WCHAR   MountPoint[4];
    WCHAR   UniqueVolumeName[60];

    MountPoint[0] = (WCHAR)DriveLetter;
    MountPoint[1] = L':';
    MountPoint[2] = L'\\';
    MountPoint[3] = L'\0';

    if (ulBufLen  < (GUID_SIZE_IN_CHARS * sizeof(WCHAR)))
        return false;

    if (0 == GetVolumeNameForVolumeMountPointW((WCHAR *)MountPoint, (WCHAR *)UniqueVolumeName, 60)) {
        _tprintf(_T("DriveLetterToGUID failed\n"));
        return false;
    }

    inm_wmemcpy_s(VolumeGUID, ulBufLen, &UniqueVolumeName[11], GUID_SIZE_IN_CHARS);

    return true;
}

HRESULT Consistency::SendTagsToDriver(ACSRequest_t &Request)
{
    // Common function to crash and app so use global lock
    boost::unique_lock<boost::mutex> lock(g_sendTagsCommonLock);

    using std::string;
    USES_CONVERSION;    // declare locals used by the ATL macros
    HRESULT hr = S_OK;

    bool bDontIssueTag = false;
    char szVolumePath[MAX_PATH] = {0};
    char szVolumeNameGUID[MAX_PATH] = {0};
    DWORD dwVolPathSize = 0;
    
    BOOL bRet = 0;
    unsigned char * strAlternateTagGuid = NULL;
    DWORD driveMask = 0;
    
    vector<string> vVolumes;
    vector<string> vProtectedVolumes;
    vector<WCHAR *> vVolumeGUID;
    vector<WCHAR *> vProtectedVolumeGUID;
    
    vector<TagData_t *> vTags;
    driveMask = Request.volumeBitMask;

    if (driveMask == 0)
    {
        // No volumes were selected. just return;
        DebugPrintf("\nNo volumes are selected.\n");
        return E_FAIL;
    }


    //remove duplicate list of volumes 
    vector<std::string>::iterator volumeNameIter = Request.vApplyTagToTheseVolumes.begin();
    vector<std::string>::iterator volumeNameEnd= Request.vApplyTagToTheseVolumes.end();
    while(volumeNameIter != volumeNameEnd)
    {
        std::string volname = *volumeNameIter;
        if(!IsVolumeAlreadyExists(vVolumes,volname.c_str()))
        {
            vVolumes.push_back(volname);
            gvVolumes.push_back(volname);
        }
        ++volumeNameIter;
    }

    char szVolumeName[3];

    szVolumeName[0] = 'X';
    szVolumeName[1] = ':';
    szVolumeName[2] = '\0';

    unsigned long GuidSize = GUID_SIZE_IN_CHARS * sizeof(WCHAR);
     
    //Get Volume GUIDs
     vector<std::string>::iterator volIter = vVolumes.begin();
     vector<std::string>::iterator volend = vVolumes.end();
     while(volIter != volend)
     {
         std::string volname = *volIter;
         WCHAR *VolumeGUID = new WCHAR[GUID_SIZE_IN_CHARS];
         MountPointToGUID((PTCHAR)volname.c_str(),VolumeGUID,GuidSize);
         vVolumeGUID.push_back(VolumeGUID);
         ++volIter;
     }
     
     string strVolName;
     for(unsigned int i = 0; i < Request.tApplyTagToTheseVolumeMountPoints_info.dwCountVolMountPoint; i++)
     {
        strVolName = Request.tApplyTagToTheseVolumeMountPoints_info.vVolumeMountPoints[i].strVolumeName; 
        WCHAR *VolumeGUID = new WCHAR[GUID_SIZE_IN_CHARS];
        MountPointToGUID((PTCHAR)strVolName.c_str(),VolumeGUID,GuidSize);
        vVolumeGUID.push_back(VolumeGUID);
        gvVolumes.push_back(strVolName);
        
     }

    unsigned long totalTagDataLen = 0;

    for (unsigned int tagIndex = 0; tagIndex < Request.TagLength.size(); tagIndex++)
    {
        TagData_t *tData = new TagData_t();

        tData->len = Request.TagLength[tagIndex];
        tData->data = (char *)Request.TagData[tagIndex];
        vTags.push_back(tData);
        INM_SAFE_ARITHMETIC(totalTagDataLen += InmSafeInt<unsigned short>::Type(tData->len), INMAGE_EX(totalTagDataLen)(tData->len))
    }

    // zero tags, return
    if (totalTagDataLen == 0)
    {
        return E_FAIL;
    }

    if(gbSyncTag)
    {
        hr = CoCreateGuid(&gTagGUID);
        if(hr !=S_OK)
        {
            return E_FAIL;
        }
        unsigned char *strTagGuid;
        UuidToString(&gTagGUID,(unsigned char**)&strTagGuid);
        DebugPrintf("Sync Tag GUID= %s\n",(char*)strTagGuid);
        RpcStringFree((unsigned char**)&strTagGuid);
    }
    if(Request.bDistributed)
    {
      hr = CoCreateGuid(&gAlternateTagGUID);
      if(hr != S_OK)
      {
        return E_FAIL;
      }
      UuidToString(&gAlternateTagGUID,(unsigned char**)&strAlternateTagGuid);
      //inm_printf("\nAlternate Tag GUID = %s\n",(char*)strAlternateTagGuid);
      //RpcStringFree((unsigned char**)&strAlternateTagGuid); MCHECK TO BE MOVED TO END
    }

    unsigned long tagBufferLen = 0;
    unsigned long ulBytesCopied = 0;

    // IOCTL data will be in the form:
    //
    // Flags      + TotalNumberOfVolumes + GUID_oF_each_volume + TotalNumberOfTags + TagLenOfFirstTag + FirstTagData + TagLenofSecondTag + SecondTagData ...
    // <--ULONG-->< ---- ULONG -------> < GUID * WCHAR ----->  <----- ULONG -----> <--- USHORT ---->  <VariableLen>  <--- USHORT ---->  <VariableLen> ...

    // TotalTagLen = 3 * ULONG + (NumOfVolumes * GUID_SIZE_IN_WCHARS) + (NumTags * USHORT) + FirstTagDataLen + SecondTagDataLen ...
    
    //tagBufferLen = (totalTagDataLen * sizeof(TCHAR) + vTags.size() * sizeof(USHORT) + vVolumeGUID.size() * GUID_SIZE_IN_CHARS * sizeof(WCHAR) + sizeof(ULONG) * 2 );

    //Synchronous tag
    size_t firstlen;
    INM_SAFE_ARITHMETIC(firstlen = InmSafeInt<size_t>::Type(sizeof(unsigned long)) * 3, INMAGE_EX(sizeof(unsigned long)))
    unsigned long secondlen;
    INM_SAFE_ARITHMETIC(secondlen = InmSafeInt<unsigned long>::Type((unsigned long) vVolumeGUID.size()) * GuidSize, INMAGE_EX((unsigned long) vVolumeGUID.size())(GuidSize))
    size_t thirdlen;
    INM_SAFE_ARITHMETIC(thirdlen = (unsigned long)vTags.size() * InmSafeInt<size_t>::Type(sizeof(unsigned short)), INMAGE_EX((unsigned long)vTags.size())(sizeof(unsigned short)))
    if(gbSyncTag)
    {
        INM_SAFE_ARITHMETIC(tagBufferLen = InmSafeInt<size_t>::Type(sizeof(GUID)) + (firstlen + secondlen + thirdlen + totalTagDataLen), INMAGE_EX(sizeof(GUID))(firstlen)(secondlen)(thirdlen)(totalTagDataLen))
    }
    else
    {
        INM_SAFE_ARITHMETIC(tagBufferLen = (InmSafeInt<size_t>::Type(firstlen) + secondlen + thirdlen + totalTagDataLen), INMAGE_EX(firstlen)(secondlen)(thirdlen)(totalTagDataLen))
    }
    char *TagDataBuffer = new char[tagBufferLen];
    char *AlternateTagDataBuffer = new char[tagBufferLen];//Used when the Distributed tag fails and it has to insert a local tag.

    if(gbSyncTag)
    {
        //First data must be GUID
        inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &gTagGUID, sizeof(GUID));
        if(Request.bDistributed)
        {
          inm_memcpy_s(AlternateTagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &gTagGUID, sizeof(GUID));
        }
        ulBytesCopied += sizeof(GUID);
    }

    #if (_WIN32_WINNT == 0x500)//Only for Win2K case
        unsigned long tagflags = TAG_FLAG_ATOMIC; 
    #else
    unsigned long tagflags;
    if(bDoNotIncludeWritrs)
    {
         tagflags = TAG_FLAG_ATOMIC; 
    }
    else
    {
         tagflags = TAG_VOLUME_INPUT_FLAGS_DEFERRED_TILL_COMMIT_SNAPSHOT|TAG_FLAG_ATOMIC; 
    }
    
    #endif
    /*if(!gbSyncTag)
    {
        tagflags = tagflags | TAG_VOLUME_INPUT_FLAGS_NO_WAIT_FOR_COMMIT;
    }*/

    inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &tagflags, sizeof(unsigned long));
    if(Request.bDistributed)
    {
      inm_memcpy_s(AlternateTagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),&tagflags, sizeof(unsigned long));
    }
    ulBytesCopied += sizeof(unsigned long);
    //DebugPrintf("\n Copy Flags; ulBytesCopied = %d\n",ulBytesCopied);

    //Copy # of GUIDs
    unsigned long ulNumberOfVolumes = (unsigned long) vVolumeGUID.size();
    inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),&ulNumberOfVolumes, sizeof(unsigned long));
    if(Request.bDistributed)
    {
      inm_memcpy_s(AlternateTagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &ulNumberOfVolumes, sizeof(unsigned long));
    }
    ulBytesCopied += sizeof(unsigned long);
    //DebugPrintf("\n Copy Number of GUIDs; ulBytesCopied = %d\n",ulBytesCopied);

    //Copy all GUIDs
    for(unsigned int i = 0; i < vVolumeGUID.size(); i++)
    {
        inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), vVolumeGUID[i], GuidSize);
        if(Request.bDistributed)
        {
          inm_memcpy_s(AlternateTagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied), vVolumeGUID[i], GuidSize);
        }
        ulBytesCopied += GuidSize;
        //DebugPrintf("\n Add Volume GUID; ulBytesCopied = %d\n",ulBytesCopied);
    }
    unsigned long ulNumberOfTags = (unsigned long) vTags.size();
    inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &ulNumberOfTags, sizeof(unsigned long));
    if(Request.bDistributed)
    {
      inm_memcpy_s(AlternateTagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),&ulNumberOfTags, sizeof(unsigned long));
    }
    ulBytesCopied += sizeof(unsigned long);
    //DebugPrintf("\n Number of Tags; ulBytesCopied = %d\n",ulBytesCopied);

    //Copy all Tags
    for(unsigned int i = 0; i < vTags.size(); i++)
    {
        TagData_t *tData = vTags[i];

        hr = ValidateTagData(tData);
        if (hr != S_OK)
        {
            DebugPrintf("ValidateTagData() FAILED \n");
            /*delete [] TagDataBuffer;//MCHECK
            delete [] AlternateTagDataBuffer;//MCHECK
            RpcStringFree((unsigned char**)&strAlternateTagGuid); //MCHECK*/
            return hr;
        }

        //Copy tag length
        inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &tData->len, sizeof(unsigned short));
        if(Request.bDistributed)
        {
          inm_memcpy_s(AlternateTagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &tData->len, sizeof(unsigned short));
        }
        ulBytesCopied += sizeof(unsigned short);
        //DebugPrintf("\n Copy the tag length; ulBytesCopied = %d\n",ulBytesCopied);

        //Copy the actual tag data
        inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), tData->data, tData->len);
        if (Request.bDistributed)
        {
            inm_memcpy_s((void*)(&(tData->data[4])), (tData->len - 4), (const void*)strAlternateTagGuid, 36);//Copying the Alternate Tag GUID for every tag.
          //inm_printf("\nAlternate TagGuid = %s\n",(char*)strAlternateTagGuid);
          inm_memcpy_s(AlternateTagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), tData->data, tData->len);
        }
        ulBytesCopied += tData->len;
        //DebugPrintf("\n Copy the actual tag data; ulBytesCopied = %d\n",ulBytesCopied);
    }
    
    // Open the filter driver
    ghControlDevice = CreateFile( INMAGE_FILTER_DOS_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL ); 

    // Check if the handle is valid or not
    if( ghControlDevice == INVALID_HANDLE_VALUE || ghControlDevice == NULL )
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        DebugPrintf( "\nFAILED Couldn't get handle to filter device, err %d\n", GetLastError() );
        /*delete [] TagDataBuffer;//MCHECK
        delete [] AlternateTagDataBuffer;//MCHECK
        RpcStringFree((unsigned char**)&strAlternateTagGuid); //MCHECK*/
        return hr;
    }
    
    //if -s option is provided VACP will skip checking driver mode
    if(!SkipChkDriverMode(Request))
    {
        //Check whether driver is in data mode or not
        DebugPrintf("\nChecking driver write order state for the given volumes\n");
        int iTotalVolumes = (int)vVolumeGUID.size();
        int iNoOfVolumesInDataMode = 0; //use iNoOfVolumesInDataMode varible to find out whether 
        etWriteOrderState DriverWriteOrderState;
        DWORD dwLastError = 0;
        char szBuf[GUID_SIZE_IN_CHARS+1] = {0};
            
        for(int i = 0; i < iTotalVolumes;i++)
        {
            if(CheckDriverStat(ghControlDevice,vVolumeGUID[i],&DriverWriteOrderState,&dwLastError))
            {
				size_t countCopied = 0;
                wcstombs_s(&countCopied, szBuf, ARRAYSIZE(szBuf), vVolumeGUID[i], ARRAYSIZE(szBuf)-1);
                inm_sprintf_s(szVolumeNameGUID, ARRAYSIZE(szVolumeNameGUID), "\\\\?\\Volume{%s}\\", szBuf);
#if (_WIN32_WINNT >= 0x0501)
                bRet = GetVolumePathNamesForVolumeName(szVolumeNameGUID,szVolumePath,MAX_PATH-1,&dwVolPathSize);
#else
                bRet = 0;
#endif

                //For each volume driver state may be different. if given volume is not set for 
                //replication pair its driver state is ecWriteOrderStateUnInitialized. 
                //Driver must be in write order data state for Exact tag. if not tag will be either Approximate or 
                //Not guaranteed. By default vacp checks the driver state and than issue the tag. If driver write order state 
                //for any one of the the given set of volumes in not data mode, vacp will not issue tag.
                //To skip driver state check, user should provide -s option from command line. Here
                //vacp will issue the tag irrespective of mode of the driver.
                if(DriverWriteOrderState >= 4)
                {
                    if(bRet)
                    {
                        DebugPrintf("\nFor volume %s driver write order state is: %d (UKNOWN).",szVolumePath, DriverWriteOrderState);
                    }
                    else
                    {
                        DebugPrintf("\nFor volume \\\\?\\Volume{%s}\\ driver write order state is: %d (UKNOWN).",szBuf, DriverWriteOrderState);
                    }
                    continue;
                }
                if(DriverWriteOrderState == ecWriteOrderStateData) 
                {
                    iNoOfVolumesInDataMode++;
                    if(bRet)
                    {
                        DebugPrintf("\nFor volume %s driver write order state is: %s",szVolumePath, szWriteOrderState[DriverWriteOrderState]);;
                    }
                    else
                    {
                        DebugPrintf("\nFor volume \\\\?\\Volume{%s}\\ driver write order state is: %s ",szBuf, szWriteOrderState[DriverWriteOrderState]);;
                    }
                }
                else if(DriverWriteOrderState == ecWriteOrderStateUnInitialized)
                {
                    if(bRet)
                    {
                        DebugPrintf("\nVolume %s  is not part of replication pair", szVolumePath);
                    }
                    else
                    {
                        DebugPrintf("\nVolume \\\\?\\Volume{%s}\\  is not part of replication pair", szBuf);
                    }
                }
                else
                {
                    if(bRet)
                    {
                        inm_printf("\nFor volume %s driver write order state is: %s",szVolumePath, szWriteOrderState[DriverWriteOrderState]);;
                    }
                    else
                    {
                        inm_printf("\nFor volume \\\\?\\Volume{%s}\\ driver write order state is: %s",szBuf, szWriteOrderState[DriverWriteOrderState]);;
                    }

                }
                ZeroMemory(szBuf,GUID_SIZE_IN_CHARS+1);
                ZeroMemory(szVolumeNameGUID,MAX_PATH);
            }
            else
            {
                DebugPrintf("ERROR: CheckDriverState() failed");
            }
        }
        inm_printf("\n");
        if(iNoOfVolumesInDataMode < iTotalVolumes)
        {
            inm_printf("Driver mode for one or all the give volume(s) are not in write ordered data mode.\n");
            inm_printf("Not able to send tags to the driver.");
            ghrErrorCode = VACP_E_DRIVER_IN_NON_DATA_MODE;
            inm_printf("\n VACP_E_DRIVER_IN_NON_DATA_MODE: Error Code = %d\n",VACP_E_DRIVER_IN_NON_DATA_MODE);
            bDontIssueTag = true;
            if (Request.bDistributed)
            {
              if(g_bThisIsMasterNode)
              {
                g_bDistMasterFailedButHasToFacilitate = true;//means,driver in NON-Data Mode
                bDontIssueTag = true;
              }
              else
              {
                g_bDistributedVacpFailed = true;
                ghrErrorCode = ERROR_DISTRIBUED_MASTER_IN_NON_DATA_MODE;
                Request.bDistributed = false;
                goto end;
              }
            }
            else
            {
              ghrErrorCode = VACP_E_DRIVER_IN_NON_DATA_MODE;
              goto end;
            }
        }
    }
    else
    {
        inm_printf("\nDriver mode checking skipped.\n");
    }

    if(!gbSyncTag)
    {
        //Issue IOCTL to driver
        BOOL	bResult = false;
        DWORD	dwReturn = 0;
        DWORD   IoctlCmd = IOCTL_INMAGE_TAG_VOLUME;
        ULONG   ulOutputLength = SIZE_OF_TAG_VOLUME_STATUS_OUTPUT_DATA(vVolumeGUID.size());
        gpTagOutputData = (PTAG_VOLUME_STATUS_OUTPUT_DATA)malloc(ulOutputLength);  
        gdwTagTimeout =  Request.dwTagTimeout;
        DWORD dwWaitForIssueTag = 0;

        XDebugPrintf("Bytes passed to driver IOCTL = %d \n",ulBytesCopied);

        if(false == bDontIssueTag)
        {
          inm_printf("Sending tags to the driver ...\n");
          if(g_bDistributedVacpFailed)
          {
            inm_printf("\nIssuing tag with alternate TagGuid %s as Distributed tag has failed...\n",(char*)strAlternateTagGuid);
            bResult = DeviceIoControl(
                         ghControlDevice, 
                         (DWORD)IoctlCmd, 
                         AlternateTagDataBuffer, 
                         ulBytesCopied,
                         (PVOID)gpTagOutputData, 
                          ulOutputLength, 
                         &dwReturn, 
                         &gOverLapTagIO //a valid OVERLAPPED structure for getting event completion notification
                         );
          }
          else
          {
            bResult = DeviceIoControl(
                         ghControlDevice, 
                         (DWORD)IoctlCmd, 
                         TagDataBuffer, 
                         ulBytesCopied,
                         (PVOID)gpTagOutputData, 
                          ulOutputLength, 
                         &dwReturn, 
                         &gOverLapTagIO //a valid OVERLAPPED structure for getting event completion notification
                         );
          }
         DWORD dwShowErr = GetLastError();
         if(0 == bResult)//when isuing of IOCTL failed
         {
              gbTagSend  = false;
              if (ERROR_IO_PENDING != dwShowErr)
              {
                  //DebugPrintf("IOCTL_INMAGE_TAG_VOLUME: DeviceIoControl failed with error = %d, BytesReturn = %d\n",GetLastError(), dwReturn);
                  inm_printf( "FAILED to issue IOCTL_INMAGE_TAG_VOLUME ioctl for following volumes\n");
                  inm_printf("Error code:%d\n",dwShowErr);
                
                  vector<std::string>::iterator volIter = vVolumes.begin();
                  vector<std::string>::iterator volend = vVolumes.end();
                  while(volIter != volend)
                  {
                      std::string volName = *volIter;
                      inm_printf("%s\t", volName.c_str());
                      ++volIter;
                  }
                  for( unsigned int i = 0; i < Request.tApplyTagToTheseVolumeMountPoints_info.dwCountVolMountPoint; i++)
                  {
                      XDebugPrintf("%s \t",Request.tApplyTagToTheseVolumeMountPoints_info.vVolumeMountPoints[i].strVolumeName.c_str());
                  }
                  inm_printf("\n");
                  hr = E_FAIL;
              }
              else//Pending IO
              {
                  //gbTagSend = true;MCHECK 
                  DebugPrintf("Tags IO Pending at Time: %s\n",GetLocalSystemTime().c_str());
                  gbGetVolumeStatus = true;
                  hr = S_OK;
              }
         }
         else//when IOCTL is issued successfully
         {   
             gbGetSyncVolumeStatus = false;
              if (GetOverlappedResult(ghControlDevice, &gOverLapTagIO, &dwReturn, FALSE))
              {	
                  hr = S_OK;
              }
              else
              {	
                  gbTagSend = false;
                  DebugPrintf("========================================\n");
                  DebugPrintf("Win32 error occurred [%d]\n",GetLastError());
                  DebugPrintf("Failed to commit tags to one or all volumes\n");
                  DebugPrintf("========================================\n");
                  hr = E_FAIL;
              }
         }
        }
    }
    else //if it is a -sync Tag
    {
        //Issue IOCTL to driver
        BOOL	bResult = false;
        DWORD	dwReturn = 0;
        DWORD   RetValue=0;
        gdwNumberOfVolumes = (DWORD) vVolumeGUID.size();
        DWORD   IoctlCmd = IOCTL_INMAGE_SYNC_TAG_VOLUME;
        ULONG   ulOutputLength = SIZE_OF_TAG_VOLUME_STATUS_OUTPUT_DATA(vVolumeGUID.size());
        gpTagOutputData = (PTAG_VOLUME_STATUS_OUTPUT_DATA)malloc(ulOutputLength);    

        gdwTagTimeout =  Request.dwTagTimeout;

        DebugPrintf("Bytes passed to driver IOCTL = %d \n",ulBytesCopied);

        if(false == bDontIssueTag)
        {
          inm_printf("Sending tags to the driver ...\n");
          if(g_bDistributedVacpFailed)
          {
            inm_printf("\nIssuing tag with alternate TagGuid %s as Distributed tag has failed...\n",(char*)strAlternateTagGuid);
            bResult = DeviceIoControl(
                          ghControlDevice, 
                          (DWORD)IOCTL_INMAGE_SYNC_TAG_VOLUME, 
                          AlternateTagDataBuffer, 
                          ulBytesCopied,
                          (PVOID)gpTagOutputData, 
                          ulOutputLength, 
                          &dwReturn, 
                          &gOverLapIO
                          );
          }
          else
          {
            bResult = DeviceIoControl(
                          ghControlDevice, 
                          (DWORD)IOCTL_INMAGE_SYNC_TAG_VOLUME, 
                          TagDataBuffer, 
                          ulBytesCopied,
                          (PVOID)gpTagOutputData, 
                          ulOutputLength, 
                          &dwReturn, 
                          &gOverLapIO
                          );
          }
          if (0 == bResult) 
          {
              gbTagSend  = false;
              if (ERROR_IO_PENDING != GetLastError())
              {
                  DebugPrintf("IOCTL_INMAGE_SYNC_TAG_VOLUME: DeviceIoControl failed with error = %d, BytesReturn = %d\n",GetLastError(), dwReturn);
                  gbGetSyncVolumeStatus = false;
                  if (dwReturn >= ulOutputLength) 
                  {
                      PrintTagStatusOutputData((PTAG_VOLUME_STATUS_OUTPUT_DATA)gpTagOutputData, vVolumes);
                  }	
                  hr = E_FAIL;
              }
              else//Pending IO
              {
                  DebugPrintf("Tags IO Pending\n");	
                  gbGetSyncVolumeStatus = true;
                  hr = S_OK;
              }
          }
          else
          {
              gbGetSyncVolumeStatus = false;
              if (GetOverlappedResult(ghControlDevice, &gOverLapIO, &dwReturn, FALSE))
              {
                  if (((PTAG_VOLUME_STATUS_OUTPUT_DATA)gpTagOutputData)->Status == ecTagStatusCommited)
                  {
                      gbTagSend = true;
                      DebugPrintf("\n==============================\n");
                      DebugPrintf("Tags are successfully committed\n");
                      DebugPrintf("===============================\n");
                      DebugPrintf("BytesReturn = %d\n", dwReturn);
                      hr = S_OK;
                  }
                  else 
                  {
                      gbTagSend = false;
                      DebugPrintf("===========================================\n");
                      DebugPrintf("Failed to commit tags to one or all volumes\n");
                      DebugPrintf("===========================================\n");
                      DebugPrintf("BytesReturn = %d\n", dwReturn);
                      hr = E_FAIL;
                  }
                  if (dwReturn >= ulOutputLength) 
                  {
                      PrintTagStatusOutputData((PTAG_VOLUME_STATUS_OUTPUT_DATA)gpTagOutputData, gvVolumes);
                  }
              }
              else
              {	
                  gbTagSend = false;
                  DebugPrintf("========================================\n");
                  DebugPrintf("Win32 error occurred [%d]\n",GetLastError());
                  DebugPrintf("Failed to commit tags to one or all volumes\n");
                  DebugPrintf("========================================\n");
                  hr = E_FAIL;
              }
          }
      }
        
      
    }

end: 

     if(ghrErrorCode != S_OK)
     {
         return ghrErrorCode;
     }
     else
     {
        return hr;
     }
     return hr;
}

// Issues TAG IOCTL to disk filter driver
// in case of app and multi-vm crash tags, the IOCTL will be pending with driver.
// it is completed on commit/revoke IOCTL
// Returns
//    S_OK on Success
//    E_FAIL on failure
//
HRESULT Consistency::SendTagsToDiskDriver(ACSRequest_t &Request)
{
    // Common function to crash and app so use global lock
    boost::unique_lock<boost::mutex> lock(g_sendTagsCommonLock);

    std::string errmsg;
    using std::string;
    USES_CONVERSION;    // declare locals used by the ATL macros
    HRESULT hr = S_OK;
    bool bDontIssueTag = false;
    
    DWORD dwVolPathSize = 0;
    BOOL bRet = 0;
    unsigned char * strAlternateTagGuid = NULL;
    DWORD driveMask = 0;

    vector<string> vVolumes;
    vector<string> vProtectedVolumes;
    vector<WCHAR *> vVolumeGUID;
    vector<WCHAR *> vProtectedVolumeGUID;
    vector<TagData_t *> vTags;

    driveMask = Request.volumeBitMask;

    unsigned long GuidSize = GUID_SIZE_IN_CHARS * sizeof(WCHAR);

    unsigned long totalTagDataLen = 0;

    for (unsigned int tagIndex = 0; tagIndex < Request.TagLength.size(); tagIndex++)
    {
        TagData_t *tData = new (std::nothrow) TagData_t();

        if (tData == NULL)
        {
            errmsg = "SendTagsToDiskDriver Error: failed to allocate memory for tag data.";
            DebugPrintf("%s\n", errmsg.c_str());
            hr = E_FAIL;
            AddToVacpFailMsgs(VACP_E_COMMIT_SEND_TAG_PHASE_FAILED, errmsg, hr);
            return hr;
        }

        tData->len = Request.TagLength[tagIndex];
        tData->data = (char *)Request.TagData[tagIndex];
        vTags.push_back(tData);
        INM_SAFE_ARITHMETIC(totalTagDataLen += InmSafeInt<unsigned short>::Type(tData->len), INMAGE_EX(totalTagDataLen)(tData->len))
    }

    
    if ( (hr == E_FAIL) || (totalTagDataLen == 0) || (totalTagDataLen > DISK_DRIVER_MAX_TAGDATA_LEN) )
    {
        for (unsigned int i = 0; i < vTags.size(); i++)
            delete vTags[i];

        vTags.clear();
    
        std::stringstream ss;
        // zero tags, return
        if (totalTagDataLen == 0)
        {
            ss << "SendTagsToDiskDriver Error: zero tag data length.";
        }

        // driver can handle a maximum of "3.5k - (buffer for bookkeeping)"
        if (totalTagDataLen > DISK_DRIVER_MAX_TAGDATA_LEN)
        {
            ss << "SendTagsToDiskDriver Error: Tag data length " << totalTagDataLen << " is greater than the driver allowed maximum " << DISK_DRIVER_MAX_TAGDATA_LEN << ".";
        }

        DebugPrintf("%s\n", ss.str().c_str());
        AddToVacpFailMsgs(VACP_E_COMMIT_SEND_TAG_PHASE_FAILED, ss.str(), hr);
        return hr;
    }


    unsigned long tagBufferLen = 0;
    unsigned long ulBytesCopied = 0;

    // IOCTL data will be in the form:
    //
    // GUID   + Flags     + Timeout + TotalNumberOfVolumes + GUID_oF_each_volume + TotalNumberOfTags + TagLenOfFirstTag + FirstTagData + TagLenofSecondTag + SecondTagData ...
    //<-GUID-> <--ULONG-->< -ULONG- ><--------ULONG-------> < GUID * WCHAR ----->  <----- ULONG -----> <--- USHORT ---->  <VariableLen>  <--- USHORT ---->  <VariableLen> ...


    size_t firstlen = 0;
    unsigned long secondlen = 0;
    size_t thirdlen = 0;
    
    unsigned long ulNumberOfDisks = IS_CRASH_TAG(Request.TagType) ? 0 : (unsigned long)Request.vProtectedDisks.size();
    if (!IS_CRASH_TAG(Request.TagType)) inm_printf("SendTagsToDiskDriver: NumberOfProtectedDisks %d \n", ulNumberOfDisks);
    unsigned long ulNumberOfTags = (unsigned long)vTags.size();

    if (IS_CRASH_TAG(Request.TagType))
    {
        firstlen = sizeof(CRASHCONSISTENT_TAG_INPUT);
    }
    else
    {
        firstlen = GUID_SIZE_IN_CHARS;

        INM_SAFE_ARITHMETIC(firstlen += InmSafeInt<size_t>::Type(sizeof(unsigned long)) * 4, INMAGE_EX(firstlen)(sizeof(unsigned long)))

        INM_SAFE_ARITHMETIC(secondlen = InmSafeInt<unsigned long>::Type((unsigned long)ulNumberOfDisks) * GuidSize, INMAGE_EX((unsigned long)GuidSize)(secondlen))
    }
    
    INM_SAFE_ARITHMETIC(thirdlen = InmSafeInt<size_t>::Type(sizeof(unsigned short)) * ulNumberOfTags, INMAGE_EX((unsigned long)ulNumberOfTags)(sizeof(unsigned short)))
    
    
    INM_SAFE_ARITHMETIC(tagBufferLen = (InmSafeInt<size_t>::Type(firstlen) + secondlen + thirdlen + totalTagDataLen), INMAGE_EX(firstlen)(secondlen)(thirdlen)(totalTagDataLen))

    DebugPrintf("\nFirstLen %d - SecondLen %d - ThirdLen %d - Total Tag Buffer Len: %d\n", firstlen, secondlen, thirdlen, tagBufferLen);

    char *TagDataBuffer = new (std::nothrow)  char[tagBufferLen];

    if (TagDataBuffer == NULL)
    {
        hr = E_FAIL;
        std::stringstream ss;
        ss << "SendTagsToDiskDriver Error: Failed to allocate " << tagBufferLen << " bytes.";
        DebugPrintf("%s\n", ss.str().c_str());
        AddToVacpFailMsgs(VACP_E_COMMIT_SEND_TAG_PHASE_FAILED, ss.str(), hr);

        for (unsigned int i = 0; i < vTags.size(); i++)
            delete vTags[i];        
        
        return hr;
    }

    memset((void*)TagDataBuffer, 0x00, tagBufferLen);

    ULONG tagflags = 0;
    

    if (IS_CRASH_TAG(Request.TagType))
    {
        CRASHCONSISTENT_TAG_INPUT *pcrashTag = (CRASHCONSISTENT_TAG_INPUT *)TagDataBuffer;
        inm_memcpy_s(pcrashTag->TagContext, tagBufferLen, Request.m_contextGuid.c_str(), GUID_SIZE_IN_CHARS);
        pcrashTag->ulFlags = TAG_INFO_FLAGS_INCLUDE_ALL_FILTERED_DISKS |TAG_INFO_FLAGS_IGNORE_DISKS_IN_NON_WO_STATE;

        ulBytesCopied += sizeof(CRASH_CONSISTENT_INPUT);

        pcrashTag->ulNumOfTags = ulNumberOfTags;
        ulBytesCopied += sizeof(ULONG);
    }
    else
    {
        LONG drainTimeout = VACP_TAG_DISK_IOCTL_COMMIT_TIMEOUT;

        VacpConfigPtr pVacpConf = VacpConfig::getInstance();
        if (pVacpConf.get() != NULL)
        {
            LONG drainMaxTimeout = VACP_TAG_DISK_IOCTL_COMMIT_MAX_TIMEOUT;
            pVacpConf->getParamValue(VACP_CONF_PARAM_TAG_COMMIT_MAX_TIMEOUT, drainMaxTimeout);
            if(pVacpConf->getParamValue(VACP_CONF_PARAM_DRAIN_BARRIER_TIMEOUT, drainTimeout))
            {
                if (drainTimeout < VACP_TAG_DISK_IOCTL_COMMIT_TIMEOUT)
                {
                    DebugPrintf("\nDrain Barrier Timeout %ld ms is less than allowed minimum. Setting to default value %d ms.\n",
                        drainTimeout, VACP_TAG_DISK_IOCTL_COMMIT_TIMEOUT);

                    drainTimeout = VACP_TAG_DISK_IOCTL_COMMIT_TIMEOUT;
                }
                else if (drainTimeout > drainMaxTimeout)
                {
                    DebugPrintf("\nDrain Barrier Timeout %ld ms is more than allowed maximum. Setting to default maximum %d ms.\n",
                        drainTimeout, drainMaxTimeout);

                    drainTimeout = drainMaxTimeout;
                }
                else
                    DebugPrintf("\nDrain Barrier Timeout is set to custom value of %ld ms.\n",
                    drainTimeout);
            }
        }
        
        ULONG tagtimeout = (ULONG)drainTimeout;

        tagflags = TAG_INFO_FLAGS_IGNORE_DISKS_IN_NON_WO_STATE;
        if (DISKFLT_MV3_TAG_PRECHECK_SUPPORT == (g_inmFltDriverVersion.ulDrMinorVersion3 & DISKFLT_MV3_TAG_PRECHECK_SUPPORT))
        {
            if (IS_CRASH_TAG(Request.TagType))
            {
                Request.bDistributed ? tagflags |= TAG_INFO_FLAGS_DISTRIBUTED_CRASH : tagflags |= TAG_INFO_FLAGS_LOCAL_CRASH;
            }
            else
            {
                Request.bDistributed ? tagflags |= TAG_INFO_FLAGS_DISTRIBUTED_APP : tagflags |= TAG_INFO_FLAGS_LOCAL_APP;
            }
            if (IS_BASELINE_TAG(Request.TagType))
            {
                tagflags |= TAG_INFO_FLAGS_BASELINE_APP;
            }
        }
        inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen - ulBytesCopied), Request.m_contextGuid.c_str(), GUID_SIZE_IN_CHARS);
        ulBytesCopied += GUID_SIZE_IN_CHARS;

        inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen - ulBytesCopied), &tagflags, sizeof(ULONG));
        ulBytesCopied += sizeof(ULONG);

        inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen - ulBytesCopied), &tagtimeout, sizeof(ULONG));
        ulBytesCopied += sizeof(ULONG);

        inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen - ulBytesCopied), &ulNumberOfDisks, sizeof(ULONG));
        ulBytesCopied += sizeof(ULONG);

        std::set<std::string>::iterator diskIter = Request.vProtectedDisks.begin();
        for (; diskIter != Request.vProtectedDisks.end(); diskIter++)
        {
            std::wstring diskSignature;
            if (!Request.GetDiskGuidFromDiskName(*diskIter, diskSignature))
            {
                hr = E_FAIL;
                std::stringstream ss;
                ss << "SendTagsToDiskDriver Error: GetDiskGuidFromDiskName() FAILED for " << (*diskIter).c_str();
                DebugPrintf("%s\n", ss.str().c_str());
                AddToVacpFailMsgs(VACP_E_COMMIT_SEND_TAG_PHASE_FAILED, ss.str(), hr);

                delete TagDataBuffer;

                for (unsigned int i = 0; i < vTags.size(); i++)
                    delete vTags[i];

                return hr;
            }

            unsigned short signatureSize = diskSignature.size() * sizeof(WCHAR);

            inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen - ulBytesCopied), diskSignature.c_str(), signatureSize);
            ulBytesCopied += GuidSize;
        }

        inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen - ulBytesCopied), &ulNumberOfTags, sizeof(unsigned long));
        ulBytesCopied += sizeof(unsigned long);
    }

    //Copy all Tags
    for (unsigned int i = 0; i < vTags.size(); i++)
    {
        TagData_t *tData = vTags[i];

        hr = ValidateTagData(tData);
        if (hr != S_OK)
        {
            hr = E_FAIL;
            std::stringstream ss;
            ss << "SendTagsToDiskDriver Error: ValidateTagData() FAILED";
            DebugPrintf("%s\n", ss.str().c_str());
            AddToVacpFailMsgs(VACP_E_COMMIT_SEND_TAG_PHASE_FAILED, ss.str(), hr);

            delete TagDataBuffer;

            for (unsigned int i = 0; i < vTags.size(); i++)
                delete vTags[i];

            return hr;
        }
        
        {
            //Copy tag length
            inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen - ulBytesCopied), &tData->len, sizeof(unsigned short));
            ulBytesCopied += sizeof(unsigned short);

            //Copy the actual tag data
            inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen - ulBytesCopied), tData->data, tData->len);
            ulBytesCopied += tData->len;
        }
    }

    // release prev allocated memory for tags
    for (unsigned int i = 0; i < vTags.size(); i++)
        delete vTags[i];

    //Issue IOCTL to driver
    BOOL	bResult = false;
    DWORD	dwReturn = 0;
    DWORD	dwShowErr;
        
    ULONG   ulOutputLength = IS_CRASH_TAG(Request.TagType) ?
                                sizeof(CRASHCONSISTENT_TAG_OUTPUT) : 
                                SIZE_OF_TAG_DISK_STATUS_OUTPUT_DATA(Request.vProtectedDisks.size());

    DWORD   IoctlCmd = IS_CRASH_TAG(Request.TagType) ?
                            ((Request.bDistributed) ?
                            IOCTL_INMAGE_DISTRIBUTED_CRASH_TAG :
                            IOCTL_INMAGE_IOBARRIER_TAG_DISK) :
                        IOCTL_INMAGE_TAG_DISK;

    string strIoctlCmd = IS_CRASH_TAG(Request.TagType) ?
                                ((Request.bDistributed) ?
                                "IOCTL_INMAGE_DISTRIBUTED_CRASH_TAG" : 
                                "IOCTL_INMAGE_IOBARRIER_TAG_DISK") : 
                            "IOCTL_INMAGE_TAG_DISK";

    gpTagOutputData = malloc(ulOutputLength);
    if (gpTagOutputData == NULL)
    {
        hr = E_FAIL;
        std::stringstream ss;
        ss << "SendTagsToDiskDriver Error: " << strIoctlCmd.c_str() << ": failed to allocate " << ulOutputLength << " bytes";
        DebugPrintf("%s\n", ss.str().c_str());
        AddToVacpFailMsgs(VACP_E_COMMIT_SEND_TAG_PHASE_FAILED, ss.str(), hr);
        delete TagDataBuffer;
        return hr;;
    }
        
    memset(gpTagOutputData, 0x00, ulOutputLength);
    if (!IS_CRASH_TAG(Request.TagType))
    {
        PTAG_DISK_STATUS_OUTPUT pTagDiskOutput = (PTAG_DISK_STATUS_OUTPUT)gpTagOutputData;
        pTagDiskOutput->ulNumDisks = Request.vProtectedDisks.size();
    }
        
    DWORD dwWaitForIssueTag = 0;

    if (false == bDontIssueTag)
    {
        DWORD startTime = GetTickCount();
            
        while (true)
        {
            m_perfCounter.StartCounter("DrainBarrier");
            m_perfCounter.StartCounter("TagIoctl");

            bResult = DeviceIoControl(
                ghControlDevice,
                (DWORD)IoctlCmd,
                TagDataBuffer,
                ulBytesCopied,
                gpTagOutputData,
                ulOutputLength,
                &dwReturn,
                &gOverLapTagIO);

            dwShowErr = GetLastError();

            m_perfCounter.EndCounter("TagIoctl");

            if (!bResult)
            {
                std::stringstream ss;
                if (ERROR_IO_PENDING == dwShowErr)
                {
                    if (!IS_CRASH_TAG(Request.TagType))
                    {
                        DebugPrintf("%s: DeviceIoControl returned pending %d, Bytes returned = %d\n",
                            strIoctlCmd.c_str(), dwShowErr, dwReturn);
                        gbTagSend = true;
                        hr = S_OK;
                        break;
                    }
                    else
                    {
                        // for either single node or multi-node crash tag IOCTL, status
                        // ERROR_IO_PENDING is an error case
                        DebugPrintf("%s: Overlapped IO is pending.\n", strIoctlCmd.c_str());
                        if (GetOverlappedResult(ghControlDevice, &gOverLapTagIO, &dwReturn, true))
                        {
                            ss << "SendTagsToDiskDriver Error: " << strIoctlCmd.c_str() << " GetOverlappedResult succeeded and returned = " << std::hex << dwReturn << ". ";
                        }
                        else
                        {
                            DWORD irpStatus = (DWORD)gOverLapTagIO.Internal;
                            ss << "SendTagsToDiskDriver Error: " << strIoctlCmd.c_str() << " GetOverlappedResult failed. status is " << std::hex << dwReturn << ". ";
                        }
                    }
                }
                else if ((!IS_CRASH_TAG(Request.TagType) || (IS_CRASH_TAG(Request.TagType) && !Request.bDistributed)) &&
                         (ERROR_BUSY == dwShowErr))
                {
                    ULONGLONG maxTime = IS_CRASH_TAG(Request.TagType) ? VACP_CRASHTAG_MAX_RETRY_TIME : VACP_TAG_IOCTL_MAX_RETRY_TIME;
                    ULONGLONG waitTime = IS_CRASH_TAG(Request.TagType) ? VACP_CRASHTAG_RETRY_WAIT_TIME : VACP_TAG_IOCTL_RETRY_WAIT_TIME;

                    DWORD currTime = GetTickCount();
                    if (currTime > startTime)
                    {
                        ULONGLONG diffTime = currTime - startTime;
                        if (diffTime < maxTime)
                        {
                            Sleep(waitTime);
                            continue;
                        }

                        ss << "SendTagsToDiskDriver Error: " << strIoctlCmd.c_str() << " returned " << dwShowErr << ", Bytes returned = " << dwReturn << ", elapsed time " << diffTime;
                    }
                    else
                    {
                        // GetTickCout wrapped. this may wait for additional 
                        // MAX_RETRY TIME
                        startTime = currTime;
                        Sleep(waitTime);
                        continue;
                    }
                }
                else
                {
                    ss << "SendTagsToDiskDriver Error: " << strIoctlCmd.c_str() << " returned " << dwShowErr << ", Bytes returned = " << dwReturn;
                }
                
                hr = HRESULT_FROM_WIN32(dwShowErr);
                
                if (dwShowErr == 2)
                {
                    hr = VACP_E_DRIVER_IN_NON_DATA_MODE;
                }

                DebugPrintf("%s\n", ss.str().c_str());
                AddToVacpFailMsgs(VACP_E_COMMIT_SEND_TAG_PHASE_FAILED, ss.str(), hr);
            }
            else
            {
                if (!IS_CRASH_TAG(Request.TagType))
                {
                    hr = E_FAIL;
                    std::stringstream ss;
                    ss << "SendTagsToDiskDriver Error: " << strIoctlCmd.c_str() << " DeviceIoControl completed without pending, BytesReturned = " << dwReturn;
                    AddToVacpFailMsgs(VACP_E_COMMIT_SEND_TAG_PHASE_FAILED, ss.str(), hr);
                }
                else
                {
                    hr = S_OK;
                    gbTagSend = true;
                    DebugPrintf("%s: Succeeded, BytesReturned = %d\n", strIoctlCmd.c_str(), dwReturn);
                }
            }
            break;
        }

        if ((ERROR_IO_PENDING != dwShowErr) && (dwReturn != 0))
        {
            if (IS_CRASH_TAG(Request.TagType))
            {
                PCRASHCONSISTENT_TAG_OUTPUT pOutput = (PCRASHCONSISTENT_TAG_OUTPUT)gpTagOutputData;
                m_tagTelInfo.Add(TagInsertKey, boost::lexical_cast<std::string>(pOutput->ulNumTaggedDisks));
                DebugPrintf("Tag is inserted for %d disks.\n", pOutput->ulNumTaggedDisks);
            }
            else
            {
                int numTaggedDisks = 0;
                PTAG_DISK_STATUS_OUTPUT pOutput = (PTAG_DISK_STATUS_OUTPUT)gpTagOutputData;
                for (int i = 0; i < pOutput->ulNumDisks; i++)
                {
                    DebugPrintf("Tag status for disk %d is 0x%x.\n", i, pOutput->TagStatus[i]);
                    if (pOutput->TagStatus[i] == 0)
                        numTaggedDisks++;
                }

                m_tagTelInfo.Add(TagInsertKey, boost::lexical_cast<std::string>(numTaggedDisks));
                DebugPrintf("Tag is inserted for %d disks.\n", numTaggedDisks);
            }


            if (gpTagOutputData != NULL)
            {
                free(gpTagOutputData);
                gpTagOutputData = NULL;
            }

        }
        ULONGLONG TagInsertTime = GetTimeInSecSinceEpoch1970();
        m_tagTelInfo.Add(TagInsertTimeKey, boost::lexical_cast<std::string>(TagInsertTime));
        DebugPrintf("TagInsertTime: %lld \n", TagInsertTime);
    }


    if (TagDataBuffer != NULL)
        delete TagDataBuffer;

    return hr;
}

void PrintTagStatusOutputData(PTAG_VOLUME_STATUS_OUTPUT_DATA StatusOutputData, vector<string> &vVolumes)
{
    inm_printf("\nNumber of Volumes: %d\n", StatusOutputData->ulNoOfVolumes);
    if (StatusOutputData->Status < ecTagStatusMaxEnum)
        inm_printf("Tag status: %s\n", TagStatusStringArray[StatusOutputData->Status]);
    else
        inm_printf("Invalid Tag value: %d\n", StatusOutputData->Status);

    for(ULONG i = 0; i < StatusOutputData->ulNoOfVolumes ; i++)
    {
       if (StatusOutputData->VolumeStatus[i] < ecTagStatusMaxEnum) 
       {
            inm_printf("Status of the tag for %s volume is  %s\n", vVolumes[i].c_str(), TagStatusStringArray[StatusOutputData->VolumeStatus[i]]);    
       } 
       else
       {
            inm_printf("Invalid Status value %s for volume %d\n", vVolumes[i].c_str(), StatusOutputData->VolumeStatus[i], i);
        }
    }
}
HRESULT Consistency::SendRevocationTagsToDriver(ACSRequest_t &Request)
{
    // Common function to crash and app so use global lock
    boost::unique_lock<boost::mutex> lock(g_sendTagsCommonLock);

    using std::string;
    USES_CONVERSION;    // declare locals used by the ATL macros

    char szVolumePath[MAX_PATH] = {0};
    char szVolumeNameGUID[MAX_PATH] = {0};
    DWORD dwVolPathSize = 0;
    BOOL bRet = 0;
    std::string errmsg;

    HRESULT hr = S_OK;
    DWORD driveMask = 0;
    HANDLE hControlDevice = NULL ;
    vector<string> vVolumes;
    vector<WCHAR *> vVolumeGUID;
    vector<TagData_t *> vTags;

    //reset gbTagSend as we have not taken any backup or backup failed.
    gbTagSend = false;

    if (Request.RevocationTagLength == 0 || Request.RevocationTagDataPtr == NULL)
    {
        XDebugPrintf("\nNo revocation tags found, skip sendings tags to driver...\n");
        return S_OK;
    }

    driveMask = Request.volumeBitMask;

    if (driveMask == 0)
    {
        hr = E_FAIL;
        // No volumes were selected. just return;
        errmsg = "SendRevocationTagsToDriver: No volumes are selected.";
        XDebugPrintf("\n%s...\n", errmsg.c_str());
        g_vacpLastError.m_errMsg = errmsg.c_str();
        g_vacpLastError.m_errorCode = hr;
        return E_FAIL;
    }

    //remove duplicate list of volumes 
    vector<std::string>::iterator volumeNameIter = Request.vApplyTagToTheseVolumes.begin();
    vector<std::string>::iterator volumeNameEnd= Request.vApplyTagToTheseVolumes.end();
    while(volumeNameIter != volumeNameEnd)
    {
        std::string volname = *volumeNameIter;
        if(!IsVolumeAlreadyExists(vVolumes,volname.c_str()))
        {
            vVolumes.push_back(volname);
        }
        ++volumeNameIter;
    }


    char szVolumeName[3];

    szVolumeName[0] = 'X';
    szVolumeName[1] = ':';
    szVolumeName[2] = '\0';

    unsigned long GuidSize = GUID_SIZE_IN_CHARS * sizeof(WCHAR);
     
    //Get Volume GUIDs
     vector<std::string>::iterator volIter = vVolumes.begin();
     vector<std::string>::iterator volend = vVolumes.end();
     while(volIter != volend)
     {
         std::string volname = *volIter;
         WCHAR *VolumeGUID = new WCHAR[GUID_SIZE_IN_CHARS];
         MountPointToGUID((PTCHAR)volname.c_str(),VolumeGUID,GuidSize);
         vVolumeGUID.push_back(VolumeGUID);
         ++volIter;
     }
     
     string strVolName;
     for(unsigned int i = 0; i < Request.tApplyTagToTheseVolumeMountPoints_info.dwCountVolMountPoint; i++)
     {
        strVolName = Request.tApplyTagToTheseVolumeMountPoints_info.vVolumeMountPoints[i].strVolumeName; 
        WCHAR *VolumeGUID = new WCHAR[GUID_SIZE_IN_CHARS];
        MountPointToGUID((PTCHAR)strVolName.c_str(),VolumeGUID,GuidSize);
        vVolumeGUID.push_back(VolumeGUID);		
     }

    unsigned long totalTagDataLen = 0;

    TagData_t *tData = new TagData_t();

    tData->len = Request.RevocationTagLength;
    tData->data = (char *)Request.RevocationTagDataPtr;

    vTags.push_back(tData);

    INM_SAFE_ARITHMETIC(totalTagDataLen += InmSafeInt<unsigned short>::Type(tData->len), INMAGE_EX(totalTagDataLen)(tData->len))
    
    unsigned long tagBufferLen = 0;
    unsigned long ulBytesCopied = 0;

    // IOCTL data will be in the form:
    //
    // Flags      + TotalNumberOfVolumes + GUID_oF_each_volume + TotalNumberOfTags + TagLenOfFirstTag + FirstTagData + TagLenofSecondTag + SecondTagData ...
    // <--ULONG-->< ---- ULONG -------> < GUID * WCHAR ----->  <----- ULONG -----> <--- USHORT ---->  <VariableLen>  <--- USHORT ---->  <VariableLen> ...

    // TotalTagLen = 3 * ULONG + (NumOfVolumes * GUID_SIZE_IN_WCHARS) + (NumTags * USHORT) + FirstTagDataLen + SecondTagDataLen ...
    
    //tagBufferLen = (totalTagDataLen * sizeof(TCHAR) + vTags.size() * sizeof(USHORT) + vVolumeGUID.size() * GUID_SIZE_IN_CHARS * sizeof(WCHAR) + sizeof(ULONG) * 2 );

    size_t firstlen;
    INM_SAFE_ARITHMETIC(firstlen = InmSafeInt<size_t>::Type(sizeof(unsigned long)) * 3, INMAGE_EX(sizeof(unsigned long)))
    unsigned long secondlen;
    INM_SAFE_ARITHMETIC(secondlen = InmSafeInt<unsigned long>::Type((unsigned long) vVolumeGUID.size()) * GuidSize, INMAGE_EX((unsigned long) vVolumeGUID.size())(GuidSize))
    size_t thirdlen;
    INM_SAFE_ARITHMETIC(thirdlen = (unsigned long)vTags.size() * InmSafeInt<size_t>::Type(sizeof(unsigned short)), INMAGE_EX((unsigned long)vTags.size())(sizeof(unsigned short)))
    INM_SAFE_ARITHMETIC(tagBufferLen = (InmSafeInt<size_t>::Type(firstlen) + secondlen + thirdlen + totalTagDataLen), INMAGE_EX(firstlen)(secondlen)(thirdlen)(totalTagDataLen))

    char *TagDataBuffer = new char[tagBufferLen];

    //We should use atomic operation as all the volume should have the same timestamp
    unsigned long tagflags = TAG_FLAG_ATOMIC; //ForRevocationTags, always the flag should be TAG_FLAG_ATOMIC
    /*#if (_WIN32_WINNT == 0x500)//Only for Win2K case1
        unsigned long tagflags = TAG_FLAG_ATOMIC; 
    #else
        unsigned long tagflags;
        if(bDoNotIncludeWritrs)
        {
             tagflags = TAG_FLAG_ATOMIC; 
        }
        else
        {
             tagflags = TAG_VOLUME_INPUT_FLAGS_DEFERRED_TILL_COMMIT_SNAPSHOT|TAG_FLAG_ATOMIC; 
        }
    #endif*/

    inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),&tagflags, sizeof(unsigned long));
    ulBytesCopied += sizeof(unsigned long);

    //Copy # of GUIDs
    unsigned long ulNumberOfVolumes = (unsigned long) vVolumeGUID.size();
    inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),&ulNumberOfVolumes, sizeof(unsigned long));
    ulBytesCopied += sizeof(unsigned long);

    //Copy all GUIDs
    for(unsigned int i = 0; i < vVolumeGUID.size(); i++)
    {
        inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), vVolumeGUID[i], GuidSize);
        ulBytesCopied += GuidSize;
    }

    unsigned long ulNumberOfTags = (unsigned long) vTags.size();
    inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &ulNumberOfTags, sizeof(unsigned long));
    ulBytesCopied += sizeof(unsigned long);

    //Copy all Tags
    for(unsigned int i = 0; i < vTags.size(); i++)
    {
        TagData_t *tData = vTags[i];

        hr = ValidateTagData(tData, false); //Do NOT print Revocation Tags
        if (hr != S_OK)
        {
            errmsg = "SendRevocationTagsToDriver: ValidateTagData FAILED for Revocation tags.";
            XDebugPrintf("\n%s...\n", errmsg.c_str());
            g_vacpLastError.m_errMsg = errmsg.c_str();
            g_vacpLastError.m_errorCode = hr;
            return hr;
        }

        //Copy tag length
        inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &tData->len, sizeof(unsigned short));
        ulBytesCopied += sizeof(unsigned short);

        //Copy the actual tag data
        inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),tData->data, tData->len);
        
        ulBytesCopied += tData->len;
    }
    
    // Open the filter driver
    hControlDevice = CreateFile( INMAGE_FILTER_DOS_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL ); 

    // Check if the handle is valid or not
    if( hControlDevice == INVALID_HANDLE_VALUE || hControlDevice == NULL )
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        errmsg = "SendRevocationTagsToDriver failed: Couldn't get handle to filter device.";
        XDebugPrintf("%s, Error: %d\n", hr);
        g_vacpLastError.m_errMsg = errmsg.c_str();
        g_vacpLastError.m_errorCode = hr;
        return hr;
    }
    
    
    //Issue IOCTL to driver
    BOOL	bResult = false;
    DWORD	dwReturn = 0;
    DWORD   IoctlCmd = IOCTL_INMAGE_TAG_VOLUME;
    ULONG   ulOutputLength = SIZE_OF_TAG_VOLUME_STATUS_OUTPUT_DATA(vVolumeGUID.size());
    gpTagOutputData = (PTAG_VOLUME_STATUS_OUTPUT_DATA)malloc(ulOutputLength);    

    //XDebugPrintf("Bytes passed to driver IOCTL = %d \n",ulBytesCopied);
    inm_printf("Sending revocation tags to the driver ...\n");
    bResult = DeviceIoControl(
                    hControlDevice, 
                    (DWORD)IoctlCmd, 
                    TagDataBuffer, 
                    ulBytesCopied,
                    (PVOID)gpTagOutputData, 
                    ulOutputLength, 
                    &dwReturn, 
                    &gOverLapRevokationIO //a valid OVERLAPPED structure for getting RevokationIO event completion notification
                    );

    if(bResult)//when IOCTL is issued successfully
    {
        /*XDebugPrintf( "Successfully issued IOCTL_INMAGE_TAG_VOLUME ioctl for following volumes:\n");

        vector<std::string>::iterator volIter = vVolumes.begin();
        vector<std::string>::iterator volend = vVolumes.end();
        while(volIter != volend)
        {
            std::string volName = *volIter;
            XDebugPrintf("%s\t", volName.c_str());
            ++volIter;
        }
        for( unsigned int i = 0; i < Request.tApplyTagToTheseVolumeMountPoints_info.dwCountVolMountPoint; i++)
        {
            XDebugPrintf("%s \t",Request.tApplyTagToTheseVolumeMountPoints_info.vVolumeMountPoints[i].strVolumeName.c_str());
        }
        

     //volumeBitMask = 0x%x(%d)\n", driveMask, driveMask);
        
        inm_printf( "\nSuccessfully sent revocation tags to the driver ...\n");
        hr = S_OK;*/
        gbGetSyncVolumeStatus = false;//Check whether it is really needed or not
        if(GetOverlappedResult(hControlDevice,&gOverLapRevokationIO,&dwReturn,FALSE))
        {
            hr = S_OK;
        }
        else
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            gbTagSend = false;//Check if it is really needed or not
            errmsg = "SendRevocationTagsToDriver failed: Failed to commit tags to one or all volumes.";
            DebugPrintf("===========================================\n");
            DebugPrintf("%s Error: %d\n", errmsg.c_str(), hr);
            DebugPrintf("========================================\n");
            g_vacpLastError.m_errMsg = errmsg.c_str();
            g_vacpLastError.m_errorCode = hr;
            hr = E_FAIL;
        }
    }
    else//when IOCTL is not issued successfully
    {
        /*inm_printf( "FAILED to issue IOCTL_INMAGE_TAG_VOLUME ioctl for following volumes\n");

        vector<std::string>::iterator volIter = vVolumes.begin();
        vector<std::string>::iterator volend = vVolumes.end();
        while(volIter != volend)
        {
            std::string volName = *volIter;
            inm_printf("%s\t", volName.c_str());
            ++volIter;
        }
        for( unsigned int i = 0; i < Request.tApplyTagToTheseVolumeMountPoints_info.dwCountVolMountPoint; i++)
        {
            XDebugPrintf("%s \t",Request.tApplyTagToTheseVolumeMountPoints_info.vVolumeMountPoints[i].strVolumeName.c_str());
        }
        inm_printf("\n");
        hr = E_FAIL;*/
        gbTagSend = false;
        DWORD errval = GetLastError();
        if (ERROR_IO_PENDING != errval)
        {
            hr = HRESULT_FROM_WIN32(errval);
            std::stringstream ss;
            ss << "SendRevocationTagsToDriver failed. Error: " << hr << " There are no sufficient data blocks in InVolFlt driver. ";
            ss << "FAILED to issue IOCTL_INMAGE_TAG_VOLUME ioctl for following volumes: ";
            vector<std::string>::iterator volIter = vVolumes.begin();
            vector<std::string>::iterator volend = vVolumes.end();
            while(volIter != volend)
            {
                ss << *volIter << "\t";
                ++volIter;
            }
            for( unsigned int i = 0; i < Request.tApplyTagToTheseVolumeMountPoints_info.dwCountVolMountPoint; i++)
            {
                XDebugPrintf("%s \t",Request.tApplyTagToTheseVolumeMountPoints_info.vVolumeMountPoints[i].strVolumeName.c_str());
            }
            hr = E_FAIL;
            inm_printf("%s\n", ss.str().c_str());
            g_vacpLastError.m_errMsg = ss.str();
            g_vacpLastError.m_errorCode = hr;
        }
        else
        {
            DebugPrintf("Tags IO Pending\n");
            hr = S_OK;
        }
    }

    // Close the Handle to the control device
    if( !(hControlDevice == INVALID_HANDLE_VALUE || hControlDevice == NULL) )
    {
        CloseHandle(hControlDevice);
    }
    return hr;
}

#if (_WIN32_WINNT > 0x500)
HRESULT Consistency::SendRemoteTagsBufferToInMageVssProvider(ACSRequest_t &Request)
{
    using std::string;
    USES_CONVERSION;    // declare locals used by the ATL macros

    BOOL bRet = 0;

    HRESULT hr = S_OK;
    DWORD driveMask = 0;
    vector<TagData_t *> vTags;
    vector<ServerDeviceID_t *> vServerDeviceID_t;
    unsigned long ServerDeviceIDSize = 0;

    driveMask = Request.volumeBitMask;

    if (driveMask == 0)
    {
        // No volumes were selected. just return;
        DebugPrintf("\nNo volumes are selected.\n");
        return E_FAIL;
    }

    unsigned long  totalServerDeviceIDLength = 0;
    //remove debuplicate list of volumes 
    for(size_t i = 0; i < Request.vServerDeviceID.size();i++)
    {
        if(!IsServerDeviceIDAlreadyExists(vServerDeviceID_t,Request.vServerDeviceID[i].c_str()))
        {	
            ServerDeviceID_t *tServerDeviceID= new ServerDeviceID_t();

            tServerDeviceID->len = (unsigned short)Request.vServerDeviceID[i].length();
            tServerDeviceID->data = (char*)Request.vServerDeviceID[i].c_str();
        
            vServerDeviceID_t.push_back(tServerDeviceID);

            INM_SAFE_ARITHMETIC(totalServerDeviceIDLength += InmSafeInt<unsigned short>::Type(tServerDeviceID->len), INMAGE_EX(totalServerDeviceIDLength)(tServerDeviceID->len))
        }
    }
     
    unsigned long totalTagDataLen = 0;
    for (unsigned int tagIndex = 0; tagIndex < Request.TagLength.size(); tagIndex++)
    {
        TagData_t *tData = new TagData_t();

        tData->len = Request.TagLength[tagIndex];
        tData->data = (char *)Request.TagData[tagIndex];

        vTags.push_back(tData);

        INM_SAFE_ARITHMETIC(totalTagDataLen += InmSafeInt<unsigned short>::Type(tData->len), INMAGE_EX(totalTagDataLen)(tData->len))
    }

    // zero tags, return
    if (totalTagDataLen == 0)
    {
        return E_FAIL;
    }


    unsigned long tagBufferLen = 0;
    unsigned long ulBytesCopied = 0;

    // IOCTL data will be in the form:
    //
    // Flags      + TotalNumberOfServerDeviceIDs + FirstDeviceIDLength + FirstDevice   + SeconddeviceIDLenth +  SecondDevice   +....+ TotalNumberOfTags + TagLenOfFirstTag + FirstTagData + TagLenofSecondTag + SecondTagData ...
    // <--ULONG-->< ---- USHORT ----------------> <----- USHORT----->  <VariableLength>  <----- USHORT----->    <VariableLength>       <----- ULONG -----> <--- USHORT ---->  <VariableLen>  <--- USHORT ---->  <VariableLen> ...
    // <--ULONG-->< ---- USHORT ----------------> <----- USHORT----->  <VariableLength>  <----- USHORT----->    <VariableLength>       <----- USHORT-----> <--- USHORT ---->  <VariableLen>  <--- USHORT ---->  <VariableLen> ...
    //tagBufferLen = (totalTagDataLen * sizeof(TCHAR) + vTags.size() * sizeof(USHORT) + vVolumeGUID.size() * GUID_SIZE_IN_CHARS * sizeof(WCHAR) + sizeof(ULONG) * 2 );

    /*
    // Flags + TotalNumberOfLUNs + FirstLUNIDsize +FirstLUNID+SecondLUNIDSize+ SecondLUNID+...N LUNIDSize + N LUNID + TotalNumberOfTags + TagLenOfFirstTag + FirstTagData + TagLenofSecondTag + SecondTagData ...
    Input Tag Stream Structure
    _________________________________________________________________________________________________________________________________________________________________________________
    |           |                   |                    |                 |    |                  |                 |              |         |          |     |         |           |
    |   Flags   | No. of DEVICEID(n)|  DeviceID1 size    | DeviceID 1 data | ...| DeviceID n size  | DeviceID n Data | Tag Len (k)  |Tag1 Size| Tag1 data| ... |Tagk Size| Tagk Data |
    |_(4 Bytes)_|____(2 Bytes)_____ |____(2 Bytes)_______|_________________|____|__(2 Bytes)_______|_________________|__(2 Bytes)___|_2 Bytes_|__________|_____|_________|___________|

    */
    
    size_t secondlen;
    INM_SAFE_ARITHMETIC(secondlen = InmSafeInt<size_t>::Type(sizeof(unsigned short))* 2, INMAGE_EX(sizeof(unsigned short)))
    size_t thirdlen;
    INM_SAFE_ARITHMETIC(thirdlen = InmSafeInt<size_t>::Type(sizeof(unsigned short)) * vServerDeviceID_t.size(), INMAGE_EX(sizeof(unsigned short))(vServerDeviceID_t.size()))
    size_t fifthlen;
    INM_SAFE_ARITHMETIC(fifthlen = InmSafeInt<size_t>::Type(sizeof(unsigned short)) * vTags.size(), INMAGE_EX(sizeof(unsigned short))(vTags.size()))
    INM_SAFE_ARITHMETIC(tagBufferLen = (unsigned long)(InmSafeInt<size_t>::Type(sizeof(unsigned long)) + secondlen + thirdlen + totalServerDeviceIDLength  + fifthlen + totalTagDataLen), INMAGE_EX(sizeof(unsigned long))(secondlen)(thirdlen)(totalServerDeviceIDLength)(fifthlen)(totalTagDataLen))
    
    //char *TagDataBuffer = new char[tagBufferLen];
    unsigned char *TagDataBuffer = new unsigned char[tagBufferLen];

    //We should use atomic operation as all the volume should have the same timestamp
    //unsigned long tagflags = TAG_VOLUME_INPUT_FLAGS_DEFERRED_TILL_COMMIT_SNAPSHOT;//TAG_FLAG_ATOMIC;
    #if (_WIN32_WINNT == 0x500)//Only for Win2K case
        unsigned long tagflags = TAG_FLAG_ATOMIC; 
    #else
        unsigned long tagflags;
        /*if(bDoNotIncludeWritrs)
        {
             tagflags = TAG_FLAG_ATOMIC; 
        }
        else
        {
             tagflags = TAG_VOLUME_INPUT_FLAGS_DEFERRED_TILL_COMMIT_SNAPSHOT|TAG_FLAG_ATOMIC; //Only atomic flag is enough I think
        }*/
        tagflags = TAG_FLAG_ATOMIC; //When InMageVssProvider is being used then no need to send the tag with TAG_VOLUME_INPUT_FLAGS_DEFERRED_TILL_COMMIT_SNAPSHOT flag!
    #endif
    inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),&tagflags, sizeof(unsigned long));
    ulBytesCopied += sizeof(unsigned long);

    //Copy # of Device IDs
    unsigned short ulNumberOfDevices = (unsigned short) vServerDeviceID_t.size();
    inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),&ulNumberOfDevices, sizeof(unsigned short));
    ulBytesCopied += sizeof(unsigned short);

    //Copy all Device IDs
    for(unsigned int i = 0; i < vServerDeviceID_t.size(); i++)
    {
        ServerDeviceID_t *tServerDeviceID = vServerDeviceID_t[i];
        
        //Copy Device ID length
        inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &tServerDeviceID ->len, sizeof(unsigned short));
        ulBytesCopied += sizeof(unsigned short);

        //Copy Device ID
        inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied),tServerDeviceID ->data , tServerDeviceID ->len);
        ulBytesCopied += tServerDeviceID ->len;
    }

    unsigned short ulNumberOfTags = (unsigned short) vTags.size();
    inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &ulNumberOfTags, sizeof(unsigned long));
    ulBytesCopied += sizeof(unsigned short);

    //Copy all Tags
    for(unsigned int i = 0; i < vTags.size(); i++)
    {
        TagData_t *tData = vTags[i];

        hr = ValidateTagData(tData);
        if (hr != S_OK)
        {
            DebugPrintf("ValidateTagData() FAILED \n");
            return hr;
        }

        //Copy tag length
        inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),&tData->len, sizeof(unsigned short));
        ulBytesCopied += sizeof(unsigned short);

        //Copy the actual tag data
        inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), tData->data, tData->len);
        
        ulBytesCopied += tData->len;
    }
    
    //Issue packets to remote server
    BOOL	bResult = false;
    DWORD	dwReturn = 0;
    //XDebugPrintf("Bytes passed to RemoteServer = %d \n",ulBytesCopied);
    inm_printf("Sending tags to the remote server ...\n");
    unsigned long tmplongdata = htonl(tagBufferLen);
    #if (_WIN32_WINNT > 0x500)
        if(pInMageVssProvider)
        {
            //inm_printf("\nFrom Vacp: Buffer for Remote Server to InMageVssProvider = %s\n Length = %ul\n",TagDataBuffer,tagBufferLen);
            //hr = pInMageVssProvider->StoreRemoteTagsBuffer((unsigned char*)TagDataBuffer,tagBufferLen);
            hr = gptrVssAppConsistencyProvider->StoreRemoteTagsBuffer((unsigned char*)TagDataBuffer,tagBufferLen);
        }
        else
        {
            inm_printf("\n InMageVssProvider interface is invalid. hr = 0x%08x \n",hr);
            hr = E_FAIL;
        }
        

        if (TagDataBuffer) 
        {
                delete[] TagDataBuffer;	
                TagDataBuffer = NULL;
        }
    #endif
    return hr;

}
//#if (_WIN32_WINNT > 0x500)
HRESULT Consistency::SendLocalTagsBufferToInMageVssProvider(ACSRequest_t &Request)
{
    using std::string;
    USES_CONVERSION;    // declare locals used by the ATL macros

    char szVolumePath[MAX_PATH] = {0};
    char szVolumeNameGUID[MAX_PATH] = {0};
    DWORD dwVolPathSize = 0;
    BOOL bRet = 0;

    HRESULT hr = S_OK;
    DWORD driveMask = 0;
    vector<string> vVolumes;
    vector<WCHAR *> vVolumeGUID;
    vector<TagData_t *> vTags;

    driveMask = Request.volumeBitMask;

    if (driveMask == 0)
    {
        // No volumes were selected. just return;
        DebugPrintf("\nNo volumes are selected.\n");
        return E_FAIL;
    }

    //remove debuplicate list of volumes 
    vector<std::string>::iterator volumeNameIter = Request.vApplyTagToTheseVolumes.begin();
    vector<std::string>::iterator volumeNameEnd= Request.vApplyTagToTheseVolumes.end();
    while(volumeNameIter != volumeNameEnd)
    {
        std::string volname = *volumeNameIter;
        if(!IsVolumeAlreadyExists(vVolumes,volname.c_str()))
        {
            vVolumes.push_back(volname);
            gvVolumes.push_back(volname);
        }
        ++volumeNameIter;
    }
    char szVolumeName[3];

    szVolumeName[0] = 'X';
    szVolumeName[1] = ':';
    szVolumeName[2] = '\0';
    
    unsigned long GuidSize = GUID_SIZE_IN_CHARS * sizeof(WCHAR);
     
    //Get Volume GUIDs
     vector<std::string>::iterator volIter = vVolumes.begin();
     vector<std::string>::iterator volend = vVolumes.end();
     while(volIter != volend)
     {
         std::string volname = *volIter;
         WCHAR *VolumeGUID = new WCHAR[GUID_SIZE_IN_CHARS];
         MountPointToGUID((PTCHAR)volname.c_str(),VolumeGUID,GuidSize);
         vVolumeGUID.push_back(VolumeGUID);
         ++volIter;
     }
     
     string strVolName;
     for(unsigned int i = 0; i < Request.tApplyTagToTheseVolumeMountPoints_info.dwCountVolMountPoint; i++)
     {
        strVolName = Request.tApplyTagToTheseVolumeMountPoints_info.vVolumeMountPoints[i].strVolumeName; 
        WCHAR *VolumeGUID = new WCHAR[GUID_SIZE_IN_CHARS];
        MountPointToGUID((PTCHAR)strVolName.c_str(),VolumeGUID,GuidSize);
        vVolumeGUID.push_back(VolumeGUID);
        gvVolumes.push_back(strVolName);		
     }

    unsigned long totalTagDataLen = 0;

    for (unsigned int tagIndex = 0; tagIndex < Request.TagLength.size(); tagIndex++)
    {
        TagData_t *tData = new TagData_t();

        tData->len = Request.TagLength[tagIndex];
        tData->data = (char *)Request.TagData[tagIndex];

        vTags.push_back(tData);

        INM_SAFE_ARITHMETIC(totalTagDataLen += InmSafeInt<unsigned short>::Type(tData->len), INMAGE_EX(totalTagDataLen)(tData->len))
    }

    // zero tags, return
    if (totalTagDataLen == 0)
    {
        return E_FAIL;
    }

    if(gbSyncTag)
    {
        hr = CoCreateGuid(&gTagGUID);
        if(hr !=S_OK)
        {
            return E_FAIL;
        }
        unsigned char *strTagGuid;
        UuidToString(&gTagGUID,(unsigned char**)&strTagGuid);
        DebugPrintf("Tag GUID= %s\n",(char*)strTagGuid);
        RpcStringFree((unsigned char**)&strTagGuid);
    }

    unsigned long tagBufferLen = 0;
    unsigned long ulBytesCopied = 0;

    // IOCTL data will be in the form:
    //
    // Flags      + TotalNumberOfVolumes + GUID_oF_each_volume + TotalNumberOfTags + TagLenOfFirstTag + FirstTagData + TagLenofSecondTag + SecondTagData ...
    // <--ULONG-->< ---- ULONG -------> < GUID * WCHAR ----->  <----- ULONG -----> <--- USHORT ---->  <VariableLen>  <--- USHORT ---->  <VariableLen> ...

    // TotalTagLen = 3 * ULONG + (NumOfVolumes * GUID_SIZE_IN_WCHARS) + (NumTags * USHORT) + FirstTagDataLen + SecondTagDataLen ...
    
    //tagBufferLen = (totalTagDataLen * sizeof(TCHAR) + vTags.size() * sizeof(USHORT) + vVolumeGUID.size() * GUID_SIZE_IN_CHARS * sizeof(WCHAR) + sizeof(ULONG) * 2 );

    //Synchronous tag
    size_t firstlen;
    INM_SAFE_ARITHMETIC(firstlen = InmSafeInt<size_t>::Type(sizeof(unsigned long)) * 3, INMAGE_EX(sizeof(unsigned long)))
    unsigned long secondlen;
    INM_SAFE_ARITHMETIC(secondlen = InmSafeInt<unsigned long>::Type((unsigned long) vVolumeGUID.size()) * GuidSize, INMAGE_EX((unsigned long) vVolumeGUID.size())(GuidSize))
    size_t thirdlen;
    INM_SAFE_ARITHMETIC(thirdlen = (unsigned long)vTags.size() * InmSafeInt<size_t>::Type(sizeof(unsigned short)), INMAGE_EX((unsigned long)vTags.size())(sizeof(unsigned short)))
    if(gbSyncTag)
    {
        INM_SAFE_ARITHMETIC(tagBufferLen = InmSafeInt<size_t>::Type(sizeof(GUID)) + (firstlen + secondlen + thirdlen + totalTagDataLen), INMAGE_EX(sizeof(GUID))(firstlen)(secondlen)(thirdlen)(totalTagDataLen))
    }
    else
    {
        INM_SAFE_ARITHMETIC(tagBufferLen = (InmSafeInt<size_t>::Type(firstlen) + secondlen + thirdlen + totalTagDataLen), INMAGE_EX(firstlen)(secondlen)(thirdlen)(totalTagDataLen))
    }
    //char *TagDataBuffer = new char[tagBufferLen];
    unsigned char *TagDataBuffer = new unsigned char[tagBufferLen];


    if(gbSyncTag)
    {
        //First data must be GUID
        inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),&gTagGUID, sizeof(GUID));
        ulBytesCopied += sizeof(GUID);
    }
        unsigned long tagflags = TAG_FLAG_ATOMIC; 


    inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &tagflags, sizeof(unsigned long));
    ulBytesCopied += sizeof(unsigned long);
    //DebugPrintf("\n Copy Flags; ulBytesCopied = %d\n",ulBytesCopied);

    //Copy # of GUIDs
    unsigned long ulNumberOfVolumes = (unsigned long) vVolumeGUID.size();
    inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied), &ulNumberOfVolumes, sizeof(unsigned long));
    ulBytesCopied += sizeof(unsigned long);
    //DebugPrintf("\n Copy Number of GUIDs; ulBytesCopied = %d\n",ulBytesCopied);

    //Copy all GUIDs
    for(unsigned int i = 0; i < vVolumeGUID.size(); i++)
    {
        inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), vVolumeGUID[i], GuidSize);
        ulBytesCopied += GuidSize;
        //DebugPrintf("\n Add Volume GUID; ulBytesCopied = %d\n",ulBytesCopied);
    }

    unsigned long ulNumberOfTags = (unsigned long) vTags.size();
    inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &ulNumberOfTags, sizeof(unsigned long));
    ulBytesCopied += sizeof(unsigned long);
    //DebugPrintf("\n Number of Tags; ulBytesCopied = %d\n",ulBytesCopied);

    //Copy all Tags
    for(unsigned int i = 0; i < vTags.size(); i++)
    {
        TagData_t *tData = vTags[i];

        hr = ValidateTagData(tData);
        if (hr != S_OK)
        {
            DebugPrintf("ValidateTagData() FAILED \n");
            return hr;
        }

        //Copy tag length
        inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &tData->len, sizeof(unsigned short));
        ulBytesCopied += sizeof(unsigned short);
        //DebugPrintf("\n Copy the tag length; ulBytesCopied = %d\n",ulBytesCopied);

        //Copy the actual tag data
        inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), tData->data, tData->len);
        
        ulBytesCopied += tData->len;
        //DebugPrintf("\n Copy the actual tag data; ulBytesCopied = %d\n",ulBytesCopied);
    }
    ULONG   ulOutputLength = SIZE_OF_TAG_VOLUME_STATUS_OUTPUT_DATA(vVolumeGUID.size());
    //call the InMageVssProvider method here to send the buffer and length
    //this information will be used by the InMageVssProvider later in the CommitSnapshots() call
    inm_printf("Sending tag data to the InMageVssProvider ...\n");
    //#if (_WIN32_WINNT > 0x500)
        if(pInMageVssProvider)
        {
            //inm_printf("\n From Vacp: Buffer for Ioctl being sent to Inmage VssProvider is = %s\n Length = %u\n",TagDataBuffer,ulBytesCopied);		
            //hr = pInMageVssProvider->StoreLocalTagsBuffer((unsigned char*)TagDataBuffer,ulBytesCopied,ulOutputLength);
            hr = gptrVssAppConsistencyProvider->StoreLocalTagsBuffer((unsigned char*)TagDataBuffer,ulBytesCopied,ulOutputLength);
        }
        else
        {
            inm_printf("\nInMageVssProvider interface is invalid.hr = 0x%08x \n",hr);
            hr = E_FAIL;
        }

        if (TagDataBuffer) 
        {
            delete[] TagDataBuffer;	
            TagDataBuffer = NULL;
        }
    //#endif

     return hr;
}
//#endif

#endif

bool SkipChkDriverMode(ACSRequest_t& acsRequest)
{
    return acsRequest.bSkipChkDriverMode;
}

int ReadVACPRegistryValue()
{
    int iRetVal = 0;
    HKEY hkResult = NULL;
    DWORD dwBufSize = 30;
    bool bUseDefaultValue = true;
    DWORD dwType = REG_DWORD;
    
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,VACP_REG_KEY,0,KEY_READ|KEY_WOW64_64KEY,&hkResult) == ERROR_SUCCESS)
    {
        if(RegQueryValueEx(hkResult,MAX_RETRYOPERATION,NULL,&dwType,(BYTE*)&gdwMaxRetryOperation,&dwBufSize) == ERROR_SUCCESS)
        {
            //Minumum must be 0 and Maximum limit is 50
            //else default value will be used.
            if((gdwMaxRetryOperation > 50) || (gdwMaxRetryOperation < 0))
            {
                gdwMaxRetryOperation = 0;
            }
        }

        if(RegQueryValueEx(hkResult,MAX_SNAPSHOT_IN_PROGRESS_RETRY_COUNT,NULL,&dwType,(BYTE*)&gdwMaxSnapShotInProgressRetryCount,&dwBufSize) == ERROR_SUCCESS)
        {
            //Minumum must be 0 and Maximum limit is 100
            //else default value will be used. Default value is 5
            if((gdwMaxSnapShotInProgressRetryCount > 100) || (gdwMaxSnapShotInProgressRetryCount < 0))
            {
                gdwMaxSnapShotInProgressRetryCount = 5;
            }
        }
    }
    
    if(hkResult)
    {
        RegCloseKey(hkResult);
    }
    return iRetVal;
}


//This function first covert given volume name like F: or F:\ to rspective volume GUID
//and comparing GUID of each targetVlumeList and addVolume. This is required as 
// some of the Windows API take F: and F:\ as valid path name. If we compare these two path name
//whcih is obviously not going to match but if their GUID is same. 
//SO we have to compare GUID of the two volumes.
bool IsVolumeAlreadyExists(vector<std::string>& targetVolumeList,const char* addVolume )
{
    bool ret = false;
    size_t cVolumes = targetVolumeList.size();
    char *pTargetzVolume = NULL;
    char *pAddVolume = NULL;
    DWORD dwVolumeLength = NULL;

    char szTargetzVolumeGUID[MAX_PATH] = {0};
    char szAddVolumeGUID[MAX_PATH] = {0};

    //check whether "\" is at the end of drive is avilabe. if not add it.
    //It is requried for GetVolumeNameForVolumeMountPoint() function.
    INM_SAFE_ARITHMETIC(dwVolumeLength = InmSafeInt<ULONG>::Type((ULONG)_tcslen(addVolume)) + 1 /* + sizeof(CHAR) */, INMAGE_EX((ULONG)_tcslen(addVolume)))
    if(addVolume[_tcslen(addVolume) -1] != _T('\\'))
    {
        INM_SAFE_ARITHMETIC(dwVolumeLength+=InmSafeInt<int>::Type(1), INMAGE_EX(dwVolumeLength))
    }
    
    pAddVolume  = (PCHAR)calloc(dwVolumeLength,sizeof(CHAR));
    inm_strcpy_s(pAddVolume, dwVolumeLength, addVolume);
    pAddVolume[dwVolumeLength - 2] = _T('\\');
    pAddVolume[dwVolumeLength - 1] = _T('\0');
    GetVolumeNameForVolumeMountPoint(pAddVolume,szAddVolumeGUID,MAX_PATH-1);

    for(DWORD  i = 0; i <cVolumes; i++)
    {
    
        memset(szTargetzVolumeGUID,0,MAX_PATH); 
        INM_SAFE_ARITHMETIC(dwVolumeLength = InmSafeInt<DWORD>::Type((DWORD) targetVolumeList[i].length()) + 1, INMAGE_EX((DWORD) targetVolumeList[i].length()))
        if(targetVolumeList[i].at(targetVolumeList[i].length()-1) != '\\')
        {
            INM_SAFE_ARITHMETIC(dwVolumeLength+=InmSafeInt<int>::Type(1), INMAGE_EX(dwVolumeLength))
        }
        
        pTargetzVolume  = (PCHAR)calloc(dwVolumeLength, sizeof(CHAR));
        inm_strcpy_s(pTargetzVolume, dwVolumeLength, targetVolumeList[i].c_str());
        pTargetzVolume[dwVolumeLength - 2] = _T('\\');
        pTargetzVolume[dwVolumeLength - 1] = _T('\0');

        GetVolumeNameForVolumeMountPoint(pTargetzVolume,szTargetzVolumeGUID,MAX_PATH-1);
        if(!_stricmp(szTargetzVolumeGUID, szAddVolumeGUID))
        {
            ret = true;
            break;
        }
    
        if(pTargetzVolume)
        {
            free(pTargetzVolume);
            pTargetzVolume = NULL;
        }
    }
    
    if(pTargetzVolume)
    {
        free(pTargetzVolume);
        pTargetzVolume = NULL;
    }

    if(pAddVolume)
    {
        free(pAddVolume);
        pAddVolume = NULL;
    }

    return ret;
}
#if (_WIN32_WINNT > 0x500)
HRESULT VssAppConsistencyProvider::EstablishRemoteConnection (u_short port, const char* ProcessServerIP)
{
    HRESULT hr = S_OK;

    // Create a SOCKET for connecting to server
    ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ClientSocket == INVALID_SOCKET) 
    {
        inm_printf("Error at socket(): %ld\n", WSAGetLastError());
        WSACleanup();

        return E_FAIL;
    }

    // The sockaddr_in structure specifies the address family,
    // IP address, and port of the server to be connected to.
    sockaddr_in clientService; 
    clientService.sin_family = AF_INET;
    clientService.sin_addr.s_addr = inet_addr( ProcessServerIP );
    clientService.sin_port = htons(port);

    XDebugPrintf("ENTERED: VssAppConsistencyProvider::EstablishRemoteConnection\n");
    do 
    {
        if ( connect( ClientSocket, (SOCKADDR*) &clientService, sizeof(clientService) ) == SOCKET_ERROR) 
        {
            inm_printf("Failed to connect: %ld\n", WSAGetLastError());
            WSACleanup();
            
            DebugPrintf( "FAILED Couldn't able to connect to remote server: %s, port: %d\n", ProcessServerIP, port );
            hr = E_FAIL;
            break;
        }
        else 
        {
            gbRemoteConnection = true;
            DebugPrintf("\nConnected to %s at port %d\n",ProcessServerIP, port);
        }
    }while(0);
    XDebugPrintf("EXITED: VssAppConsistencyProvider::EstablishRemoteConnection\n");
    return hr;
}

HRESULT VssAppConsistencyProvider::SendTagsToRemoteServer(ACSRequest_t &Request)
{
    using std::string;
    USES_CONVERSION;    // declare locals used by the ATL macros

    BOOL bRet = 0;

    HRESULT hr = S_OK;
    DWORD driveMask = 0;
    vector<TagData_t *> vTags;
    vector<ServerDeviceID_t *> vServerDeviceID_t;
    unsigned long ServerDeviceIDSize = 0;

    driveMask = Request.volumeBitMask;

    if (driveMask == 0)
    {
        // No volumes were selected. just return;
        DebugPrintf("\nNo volumes are selected.\n");
        return E_FAIL;
    }

    unsigned long  totalServerDeviceIDLength = 0;
    //remove debuplicate list of volumes 
    for(size_t i = 0; i < Request.vServerDeviceID.size();i++)
    {
        if(!IsServerDeviceIDAlreadyExists(vServerDeviceID_t,Request.vServerDeviceID[i].c_str()))
        {	
            ServerDeviceID_t *tServerDeviceID= new ServerDeviceID_t();

            tServerDeviceID->len = (unsigned short)Request.vServerDeviceID[i].length();
            tServerDeviceID->data = (char*)Request.vServerDeviceID[i].c_str();
        
            vServerDeviceID_t.push_back(tServerDeviceID);

            INM_SAFE_ARITHMETIC(totalServerDeviceIDLength += InmSafeInt<unsigned short>::Type(tServerDeviceID->len), INMAGE_EX(totalServerDeviceIDLength)(tServerDeviceID->len))
        }
    }
     
    unsigned long totalTagDataLen = 0;
    for (unsigned int tagIndex = 0; tagIndex < Request.TagLength.size(); tagIndex++)
    {
        TagData_t *tData = new TagData_t();

        tData->len = Request.TagLength[tagIndex];
        tData->data = (char *)Request.TagData[tagIndex];

        vTags.push_back(tData);

        INM_SAFE_ARITHMETIC(totalTagDataLen += InmSafeInt<unsigned short>::Type(tData->len), INMAGE_EX(totalTagDataLen)(tData->len))
    }

    // zero tags, return
    if (totalTagDataLen == 0)
    {
        return E_FAIL;
    }


    unsigned long tagBufferLen = 0;
    unsigned long ulBytesCopied = 0;

    // IOCTL data will be in the form:
    //
    // Flags      + TotalNumberOfServerDeviceIDs + FirstDeviceIDLength + FirstDevice   + SeconddeviceIDLenth +  SecondDevice   +....+ TotalNumberOfTags + TagLenOfFirstTag + FirstTagData + TagLenofSecondTag + SecondTagData ...
    // <--ULONG-->< ---- USHORT ----------------> <----- USHORT----->  <VariableLength>  <----- USHORT----->    <VariableLength>       <----- ULONG -----> <--- USHORT ---->  <VariableLen>  <--- USHORT ---->  <VariableLen> ...
    // <--ULONG-->< ---- USHORT ----------------> <----- USHORT----->  <VariableLength>  <----- USHORT----->    <VariableLength>       <----- USHORT-----> <--- USHORT ---->  <VariableLen>  <--- USHORT ---->  <VariableLen> ...
    //tagBufferLen = (totalTagDataLen * sizeof(TCHAR) + vTags.size() * sizeof(USHORT) + vVolumeGUID.size() * GUID_SIZE_IN_CHARS * sizeof(WCHAR) + sizeof(ULONG) * 2 );

    /*
    // Flags + TotalNumberOfLUNs + FirstLUNIDsize +FirstLUNID+SecondLUNIDSize+ SecondLUNID+...N LUNIDSize + N LUNID + TotalNumberOfTags + TagLenOfFirstTag + FirstTagData + TagLenofSecondTag + SecondTagData ...
    Input Tag Stream Structure
    _________________________________________________________________________________________________________________________________________________________________________________
    |           |                   |                    |                 |    |                  |                 |              |         |          |     |         |           |
    |   Flags   | No. of DEVICEID(n)|  DeviceID1 size    | DeviceID 1 data | ...| DeviceID n size  | DeviceID n Data | Tag Len (k)  |Tag1 Size| Tag1 data| ... |Tagk Size| Tagk Data |
    |_(4 Bytes)_|____(2 Bytes)_____ |____(2 Bytes)_______|_________________|____|__(2 Bytes)_______|_________________|__(2 Bytes)___|_2 Bytes_|__________|_____|_________|___________|

    */
    
    size_t secondlen;
    INM_SAFE_ARITHMETIC(secondlen = InmSafeInt<size_t>::Type(sizeof(unsigned short))* 2, INMAGE_EX(sizeof(unsigned short)))
    size_t thirdlen;
    INM_SAFE_ARITHMETIC(thirdlen = InmSafeInt<size_t>::Type(sizeof(unsigned short)) * vServerDeviceID_t.size(), INMAGE_EX(sizeof(unsigned short))(vServerDeviceID_t.size()))
    size_t fifthlen;
    INM_SAFE_ARITHMETIC(fifthlen = InmSafeInt<size_t>::Type(sizeof(unsigned short)) * vTags.size(), INMAGE_EX(sizeof(unsigned short))(vTags.size()))
    INM_SAFE_ARITHMETIC(tagBufferLen = (unsigned long)(InmSafeInt<size_t>::Type(sizeof(unsigned long)) + secondlen + thirdlen + totalServerDeviceIDLength  + fifthlen + totalTagDataLen), INMAGE_EX(sizeof(unsigned long))(secondlen)(thirdlen)(totalServerDeviceIDLength)(fifthlen)(totalTagDataLen))
    
    char *TagDataBuffer = new char[tagBufferLen];

    //We should use atomic operation as all the volume should have the same timestamp
    //unsigned long tagflags = TAG_VOLUME_INPUT_FLAGS_DEFERRED_TILL_COMMIT_SNAPSHOT;//TAG_FLAG_ATOMIC;
    #if (_WIN32_WINNT == 0x500)//Only for Win2K case
        unsigned long tagflags = TAG_FLAG_ATOMIC; 
    #else
        unsigned long tagflags;
        if(bDoNotIncludeWritrs)
        {
             tagflags = TAG_FLAG_ATOMIC; 
        }
        else
        {
             tagflags = TAG_VOLUME_INPUT_FLAGS_DEFERRED_TILL_COMMIT_SNAPSHOT|TAG_FLAG_ATOMIC; 
        }
    #endif
    inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),&tagflags, sizeof(unsigned long));
    ulBytesCopied += sizeof(unsigned long);

    //Copy # of Device IDs
    unsigned short ulNumberOfDevices = (unsigned short) vServerDeviceID_t.size();
    inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &ulNumberOfDevices, sizeof(unsigned short));
    ulBytesCopied += sizeof(unsigned short);

    //Copy all Device IDs
    for(unsigned int i = 0; i < vServerDeviceID_t.size(); i++)
    {
        ServerDeviceID_t *tServerDeviceID = vServerDeviceID_t[i];
        
        //Copy Device ID length
        inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),&tServerDeviceID ->len, sizeof(unsigned short));
        ulBytesCopied += sizeof(unsigned short);

        //Copy Device ID
        inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied),tServerDeviceID ->data , tServerDeviceID ->len);
        ulBytesCopied += tServerDeviceID ->len;
    }

    unsigned short ulNumberOfTags = (unsigned short) vTags.size();
    inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),&ulNumberOfTags, sizeof(unsigned long));
    ulBytesCopied += sizeof(unsigned short);

    //Copy all Tags
    for(unsigned int i = 0; i < vTags.size(); i++)
    {
        TagData_t *tData = vTags[i];

        hr = ValidateTagData(tData);
        if (hr != S_OK)
        {
            DebugPrintf("ValidateTagData() FAILED \n");
            return hr;
        }

        //Copy tag length
        inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &tData->len, sizeof(unsigned short));
        ulBytesCopied += sizeof(unsigned short);

        //Copy the actual tag data
        inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), tData->data, tData->len);
        
        ulBytesCopied += tData->len;
    }
    
    //Issue packets to remote server
    BOOL	bResult = false;
    DWORD	dwReturn = 0;
    XDebugPrintf("Bytes passed to RemoteServer = %d \n",ulBytesCopied);

    inm_printf("Sending tags to the remote server ...\n");
    VACP_SERVER_RESPONSE tVacpServerResponse;

    unsigned long tmplongdata = htonl(tagBufferLen);
    do 
    {	
        if (::send(ClientSocket,(char *)&tmplongdata, sizeof tmplongdata,0) == SOCKET_ERROR)
        {
            DebugPrintf ("FAILED Couldn't able to send total Tag data length to remote server");
            hr = E_FAIL;
            break;
        }
    
        else if (::send(ClientSocket,(char*)TagDataBuffer, tagBufferLen,0) == SOCKET_ERROR)
        {
            DebugPrintf ("FAILED Couldn't able to send Tag data to remote server");
            hr = E_FAIL;
            break;
        }
        else if (::recv(ClientSocket,(char*)&tVacpServerResponse, sizeof tVacpServerResponse,0) <= 0)
        {
            DebugPrintf ("FAILED Couldn't able to get acknowledgement from remote server");
            hr = E_FAIL;
            break;
        }

        tVacpServerResponse.iResult  = ntohl(tVacpServerResponse.iResult);
        if(tVacpServerResponse.iResult > 0)
        {
            bResult = true;
        }
        if (TagDataBuffer) 
            delete[] TagDataBuffer;
        

        tVacpServerResponse.val = ntohl(tVacpServerResponse.val);
        tVacpServerResponse.str[tVacpServerResponse.val] = '\0';
        
        DebugPrintf ("INFO: VacpServerResponse:[%d] %s\n", tVacpServerResponse.iResult, tVacpServerResponse.str);
    
    } while(0);

    if(bResult)
    {
        gbTagSend = true;
        XDebugPrintf( "Successfully sent tags to following Server devices:\n");
        for (size_t i = 0; i < vServerDeviceID_t.size(); i++)
        {
            std::string strServerDeviceID = vServerDeviceID_t[i]->data;
            XDebugPrintf("%s\t", strServerDeviceID.c_str());
        }
        
        DebugPrintf( "\nSuccessfully sent tags to the Remote Server ...\n");
        hr = S_OK;
    }
    else
    {
        DebugPrintf( "FAILED to Send Tag to the Remote Server for following Server devices\n");
        DebugPrintf("Error code:%d",GetLastError());

        for (size_t i = 0; i < vServerDeviceID_t.size(); i++)
        {
            std::string strServerDeviceID = vServerDeviceID_t[i]->data;
            XDebugPrintf("%s\t", strServerDeviceID.c_str());
        }
        inm_printf("\n");
        hr = E_FAIL;
    }
    
    vServerDeviceID_t.clear();
    vTags.clear();

    return hr;

}

HRESULT VssAppConsistencyProvider::ProcessExposingSnapshots()
{
    HRESULT hr;
    CHAR szSystemDirectory[MAX_PATH * 2] = {0};
    CHAR szSystemVolumeDriveLetter[MAX_PATH * 2] = {0};
    string strShadowCopyMountVolume;
    
    SYSTEMTIME _tSystemTime;

    if(GetSystemDirectory(szSystemDirectory,(MAX_PATH * 2)))
    {	
        //After getting the SystemDirectory, extract the Dirve Letter of the System Directory
        //if an invalid path is given, then it will give the Boot Volume Drive Letter
        if(0 == GetVolumePathName((LPCTSTR)szSystemDirectory,szSystemVolumeDriveLetter,(MAX_PATH * 2)))
        {
            XDebugPrintf("\nError: Error in getting the System Directory of the System.\n");
            XDebugPrintf("\nHence, the persistent shadow copy cannot be mounted.\n");
            XDebugPrintf("\nCleaning(Deletion) of the Snapshots.\n");
            m_vssRequestorPtr->CleanupSnapShots();
        }
        else
        {
            string strShadowCopyPath;
            strShadowCopyPath = string(szSystemVolumeDriveLetter);
            char chSysTimeBuf[MAX_PATH] = {0};
            //GetSystemTime(&_tSystemTime);
            GetLocalTime(&_tSystemTime);
            string strShadowCopyName;

            //Give the Name of the ShadowCopy Mounted Volume in the format
            //Year+Month+Day+Hour+Minute+Second+MilliSecond as the Volume Mount Point Name
            itoa(_tSystemTime.wYear,chSysTimeBuf,10);//Year
            strShadowCopyName = strShadowCopyName + string(chSysTimeBuf);
            memset((void*)chSysTimeBuf,0x00,MAX_PATH);
            strShadowCopyName = strShadowCopyName + string("-");

            itoa(_tSystemTime.wMonth,chSysTimeBuf,10);//Month
            strShadowCopyName = strShadowCopyName + string(chSysTimeBuf);
            memset((void*)chSysTimeBuf,0x00,MAX_PATH);
            strShadowCopyName = strShadowCopyName + string("-");

            itoa(_tSystemTime.wDay,chSysTimeBuf,10);//Day
            strShadowCopyName = strShadowCopyName + string(chSysTimeBuf);
            memset((void*)chSysTimeBuf,0x00,MAX_PATH);
            strShadowCopyName = strShadowCopyName + string("-");

            itoa(_tSystemTime.wHour,chSysTimeBuf,10);//Hour
            strShadowCopyName = strShadowCopyName + string(chSysTimeBuf);
            memset((void*)chSysTimeBuf,0x00,MAX_PATH);
            strShadowCopyName = strShadowCopyName + string("-");

            itoa(_tSystemTime.wMinute,chSysTimeBuf,10);//Minute
            strShadowCopyName = strShadowCopyName + string(chSysTimeBuf);
            memset((void*)chSysTimeBuf,0x00,MAX_PATH);
            strShadowCopyName = strShadowCopyName + string("-");

            itoa(_tSystemTime.wSecond,chSysTimeBuf,10);//Second
            strShadowCopyName = strShadowCopyName + string(chSysTimeBuf);
            memset((void*)chSysTimeBuf,0x00,MAX_PATH);
            strShadowCopyName = strShadowCopyName + string("-");
            itoa(_tSystemTime.wMilliseconds,chSysTimeBuf,10);//MilliSecond
            strShadowCopyName = strShadowCopyName + string(chSysTimeBuf);
            memset((void*)chSysTimeBuf,0x00,MAX_PATH);
            strShadowCopyName = strShadowCopyName + string("-");
            strShadowCopyName = strShadowCopyName + string("Set");
            string strShadowCopyMountVolume = strShadowCopyPath + strShadowCopyName;
            if(ENOENT== mkdir(strShadowCopyMountVolume.c_str()))
            {
                DWORD dwErr = GetLastError();
                inm_printf("\n**********ERROR************ = %d",dwErr);
                XDebugPrintf("\nError: Error in Creating the Directory %s.\n",strShadowCopyMountVolume.c_str());
                XDebugPrintf("\nHence, the persistent shadow copy cannot be mounted.\n");
                XDebugPrintf("\nCleaning(Deletion) of the Snapshots.\n");
                m_vssRequestorPtr->CleanupSnapShots();
            }			
            else
            {
                //if the Directory for the mount volume is created successfully,
                //then Mount the Shadow copy in that folder
                hr = m_vssRequestorPtr->ExposeShadowCopiesLocally(strShadowCopyMountVolume);
                if(S_OK != hr)
                {
                    XDebugPrintf("\nError: Error in Exposing the current Snapshot/Shadow Copy.\n");
                    XDebugPrintf("\nHence, the persistent shadow copy cannot be mounted.\n");
                    XDebugPrintf("\nCleaning(Deletion) of the Snapshots.\n");
                    m_vssRequestorPtr->CleanupSnapShots();
                }					
            }				
        }
    }
    return hr;
}
/*HRESULT VssAppConsistencyProvider::SendTagsToRemoteServeViaConfigurator(ACSRequest_t &Request,bool bRevocationTag)
{
    using std::string;
    USES_CONVERSION;    // declare locals used by the ATL macros

    BOOL bRet = 0;

    HRESULT hr = S_OK;
    DWORD driveMask = 0;
    vector<TagData_t *> vTags;
    vector<ServerDeviceID_t *> vServerDeviceID_t;
    unsigned long ServerDeviceIDSize = 0;;

    driveMask = Request.volumeBitMask;

    if (driveMask == 0)
    {
        // No volumes were selected. just return;
        DebugPrintf("\nNo volumes are selected.\n");
        return E_FAIL;
    }

    unsigned long  totalServerDeviceIDLength = 0;
    VACP_SERVER_CMD_ARG vacpServerCmdArg;
    vacpServerCmdArg.tagFlag = 0;
    vacpServerCmdArg.bRevocationTag = bRevocationTag;

    //remove debuplicate list of volumes 
    for(size_t i = 0; i < Request.vServerDeviceID.size();i++)
    {
        if(!IsVolumeAlreadyExists(vacpServerCmdArg.vDeviceId,Request.vServerDeviceID[i].c_str()))
        {	
            vacpServerCmdArg.vDeviceId.push_back(Request.vServerDeviceID[i]);
        }
    }
     
    for (unsigned int tagIndex = 0; tagIndex < Request.vacpServerCmdArg.vTagIdAndData.size(); tagIndex++)
    {

        TAG_IDENTIFIER_AND_DATA tagIDandData;
        tagIDandData.tagIdentifier= Request.vacpServerCmdArg.vTagIdAndData[tagIndex].tagIdentifier;
        tagIDandData.strTagName = Request.vacpServerCmdArg.vTagIdAndData[tagIndex].strTagName;
        vacpServerCmdArg.vTagIdAndData.push_back(tagIDandData);
    }


    unsigned long totalTagDataLen = 0;
    for (unsigned int tagIndex = 0; tagIndex < Request.TagLength.size(); tagIndex++)
    {
        TagData_t *tData = new TagData_t();

        tData->len = Request.TagLength[tagIndex];
        tData->data = (char *)Request.TagData[tagIndex];

        hr = ValidateTagData(tData);
        if (hr != S_OK)
        {
            DebugPrintf("FILE = %s, LINE = %d, ValidateTagData() FAILED \n",__FILE__, __LINE__);
            return hr;
        }
   }
    
    //Issue packets to remote server
    BOOL	bResult = false;
    DWORD	dwReturn = 0;
    //XDebugPrintf("Bytes passed to RemoteServer = %d \n",ulBytesCopied);

    inm_printf("Sending tags to the remote server ...\n");


    //TO DO
    //send the tag to the VACP server using configurator


    //*******************DUMMY CODE************************************************
    //HTTP request though configurator

    //SVERROR rc = SVS_OK;
    //LocalConfigurator config;
    //char pszURL[32];

    //char szBuffer[ REGISTER_BUFSIZE ];
    //char *pszGetBuffer = NULL;

    //do 
    //{
        // post information to CX server
        /*if (strlen(szBuffer) > 0 && !bCxdown ) 
        {			
            if(PostToSVServer( config.getHttp().ipAddress, config.getHttp().port, pszURL, TagDataBuffer,tagBufferLen).failed())
            {
                inm_printf( "\Failed to post to CX server: " << config.getHttp().ipAddress\n");
                inm_printf( "\Verify if CX server is running.\n");
                hr = E_FAIL;
                break;
            }
            else
            {
                if( GetFromSVServer( config.getHttp().ipAddress, config.getHttp().port,pszGetURL,&pszGetBuffer ).failed() )
                {
                    bResult = 0
                    hr = E_FAIL;
                }	
            }* /

    //*************************************************************************
    if(bResult)
    {
        gbTagSend = true;
        XDebugPrintf( "Successfully sent tags to following devices:\n");
        for (size_t i = 0; i < vServerDeviceID_t.size(); i++)
        {
            std::string strDeviceID= vServerDeviceID_t[i]->data;
            XDebugPrintf("%s\t", strDeviceID.c_str());
        }
        
        inm_printf( "\nSuccessfully sent tags to the Remote Server ...\n");
        hr = S_OK;
    }
    else
    {
        inm_printf( "FAILED to Send Tag to the Remote Server for following devices:\n");
        inm_printf("Error code:%d",GetLastError());

        for (size_t i = 0; i < vServerDeviceID_t.size(); i++)
        {
            std::string strDeviceID= vServerDeviceID_t[i]->data;
            XDebugPrintf("%s\t", strDeviceID.c_str());
        }
        inm_printf("\n");
        hr = E_FAIL;
    }
    
    vServerDeviceID_t.clear();
    vTags.clear();

    return hr;

}
*/
HRESULT VssAppConsistencyProvider::SendRevocationTagsToRemoteServer(ACSRequest_t &Request)
{
    using std::string;
    USES_CONVERSION;    // declare locals used by the ATL macros

    BOOL bRet = 0;
    std::string errmsg;

    HRESULT hr = S_OK;
    DWORD driveMask = 0;
    vector<TagData_t *> vTags;
    vector<ServerDeviceID_t *> vServerDeviceID_t;
    //unsigned long deviceSize = 0;;

    //reset gbTagSend as we have not taken any backup or backup failed.
    gbTagSend = false;

    if (Request.RevocationTagLength == 0 || Request.RevocationTagDataPtr == NULL)
    {
        XDebugPrintf("\nNo revocation tags found, skip sendings tags to driver...\n");
        return S_OK;
    }

    driveMask = Request.volumeBitMask;

    if (driveMask == 0)
    {
        // No volumes were selected. just return;
        errmsg = "SendRevocationTagsToRemoteServer: No volumes are selected.";
        DebugPrintf("\n%s\n", errmsg);
        AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg;
        AppConsistency::Get().m_vacpLastError.m_errorCode = E_FAIL;
        return E_FAIL;
    }

    unsigned long  totalDeviceIDLength = 0;
    //remove debuplicate list of volumes 
    for(size_t i = 0; i < Request.vServerDeviceID.size();i++)
    {
        if(!IsServerDeviceIDAlreadyExists(vServerDeviceID_t,Request.vServerDeviceID[i].c_str()))
        {	
            ServerDeviceID_t *tServerDeviceID = new ServerDeviceID_t();

            tServerDeviceID->len = (unsigned short)Request.vServerDeviceID[i].length();
            tServerDeviceID->data = (char*)Request.vServerDeviceID[i].c_str();
        
            vServerDeviceID_t.push_back(tServerDeviceID);

            INM_SAFE_ARITHMETIC(totalDeviceIDLength += InmSafeInt<unsigned short>::Type(tServerDeviceID->len), INMAGE_EX(totalDeviceIDLength)(tServerDeviceID->len))
        }
    }

    unsigned long totalTagDataLen = 0;

    TagData_t *tData = new TagData_t();

    tData->len = Request.RevocationTagLength;
    tData->data = (char *)Request.RevocationTagDataPtr;

    vTags.push_back(tData);

    INM_SAFE_ARITHMETIC(totalTagDataLen += InmSafeInt<unsigned short>::Type(tData->len), INMAGE_EX(totalTagDataLen)(tData->len))
    
    unsigned long tagBufferLen = 0;
    unsigned long ulBytesCopied = 0;

    // IOCTL data will be in the form:
    //
    // Flags      + TotalNumberOfDeviceIDs + FirstDeviceIDLength  +  FirstDeviceID   + SeconddeviceIDLenth   +  SecondDeviceID    +....+ TotalNumberOfTags + TagLenOfFirstTag + FirstTagData + TagLenofSecondTag + SecondTagData ...
    // <--ULONG-->< ---- ULONG -----------> <--------USHORT----->   <VariableLength>   <----- USHORT----->     <VariableLength>          <----- ULONG -----> <--- USHORT ---->  <VariableLen>  <--- USHORT ---->  <VariableLen> ...
    //tagBufferLen = (totalTagDataLen * sizeof(TCHAR) + vTags.size() * sizeof(USHORT) + vVolumeGUID.size() * GUID_SIZE_IN_CHARS * sizeof(WCHAR) + sizeof(ULONG) * 2 );

    /*
    // Flags + TotalNumberOfLUNs + FirstLUNIDsize +FirstLUNID+SecondLUNIDSize+ SecondLUNID+...N LUNIDSize + N LUNID + TotalNumberOfTags + TagLenOfFirstTag + FirstTagData + TagLenofSecondTag + SecondTagData ...
    Input Tag Stream Structure
    __________________________________________________________________________________________________________________________________________________________________
    |           |                |                 |              |    |               |              |              |         |          |     |         |           |
    |   Flags   | No. of LUNID(n)|  LunID1 size    | LUNID 1 data | ...| LUNID n size  | LUNID n Data | Tag Len (k)  |Tag1 Size| Tag1 data| ... |Tagk Size| Tagk Data |
    |_(4 Bytes)_|____(4 Bytes)__ |____(2 Bytes)____|______________|____|__(2 Bytes)____|______________|__(4 Bytes)___|_2 Bytes_|__________|_____|_________|___________|

    */

    size_t firstlen;
    INM_SAFE_ARITHMETIC(firstlen = InmSafeInt<size_t>::Type(sizeof(unsigned long)) * 3, INMAGE_EX(sizeof(unsigned long)))
    size_t secondlen;
    INM_SAFE_ARITHMETIC(secondlen = InmSafeInt<size_t>::Type(sizeof(unsigned short)) * vServerDeviceID_t.size(), INMAGE_EX(sizeof(unsigned short))(vServerDeviceID_t.size()))
    size_t fourthlen;
    INM_SAFE_ARITHMETIC(fourthlen = InmSafeInt<size_t>::Type(sizeof(unsigned short)) * vTags.size(), INMAGE_EX(sizeof(unsigned short))(vTags.size()))
    INM_SAFE_ARITHMETIC(tagBufferLen = (unsigned long)(InmSafeInt<size_t>::Type(firstlen) + secondlen + totalDeviceIDLength  + fourthlen + totalTagDataLen), INMAGE_EX(firstlen)(secondlen)(totalDeviceIDLength)(fourthlen)(totalTagDataLen))

    char *TagDataBuffer = new char[tagBufferLen];

    //We should use atomic operation as all the volume should have the same timestamp
    unsigned long tagflags = TAG_FLAG_ATOMIC; //For Revocation, always the flag shold be TAG_FLAG_ATOMIC
    /*#if (_WIN32_WINNT == 0x500)//Only for Win2K case
        unsigned long tagflags = TAG_FLAG_ATOMIC; 
    #else
        unsigned long tagflags;
        if(bDoNotIncludeWritrs)
        {
             tagflags = TAG_FLAG_ATOMIC; 
        }
        else
        {
             tagflags = TAG_VOLUME_INPUT_FLAGS_DEFERRED_TILL_COMMIT_SNAPSHOT|TAG_FLAG_ATOMIC; 
        }
    #endif*/

    inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),&tagflags, sizeof(unsigned long));
    ulBytesCopied += sizeof(unsigned long);

    //Copy # of LUIDs
    unsigned long ulNumberOfLUNs = (unsigned long) vServerDeviceID_t.size();
    inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &ulNumberOfLUNs, sizeof(unsigned long));
    ulBytesCopied += sizeof(unsigned long);

    //Copy all LUIDs
    for(unsigned int i = 0; i < vServerDeviceID_t.size(); i++)
    {
        ServerDeviceID_t *tServerDeviceID = vServerDeviceID_t[i];
        
        //Copy Device ID length
        inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),&tServerDeviceID->len, sizeof(unsigned short));
        ulBytesCopied += sizeof(unsigned short);

        //Copy Device ID
        inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied),tServerDeviceID->data , tServerDeviceID->len);
        ulBytesCopied += tServerDeviceID->len;
    }

    unsigned long ulNumberOfTags = (unsigned long) vTags.size();
    inm_memcpy_s(TagDataBuffer + ulBytesCopied,(tagBufferLen-ulBytesCopied), &ulNumberOfTags, sizeof(unsigned long));
    ulBytesCopied += sizeof(unsigned long);

    //Copy all Tags
    for(unsigned int i = 0; i < vTags.size(); i++)
    {
        TagData_t *tData = vTags[i];

        hr = ValidateTagData(tData,false); //do not print revocationt tag
        if (hr != S_OK)
        {
            errmsg = "SendRevocationTagsToRemoteServer: ValidateTagData failed.";
            DebugPrintf("\n%s\n", errmsg);
            AppConsistency::Get().m_vacpLastError.m_errMsg = errmsg;
            AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
            return hr;
        }

        //Copy tag length
        inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),&tData->len, sizeof(unsigned short));
        ulBytesCopied += sizeof(unsigned short);

        //Copy the actual tag data
        inm_memcpy_s(TagDataBuffer + ulBytesCopied, (tagBufferLen-ulBytesCopied),tData->data, tData->len);
        
        ulBytesCopied += tData->len;
    }
    
    //Issue IOCTL to driver
    BOOL	bResult = false;
    DWORD	dwReturn = 0;
    XDebugPrintf("Bytes passed to RemoteServer = %d \n",ulBytesCopied);

    inm_printf("Sending Revocation tags to the remote server ...\n");
    //TagInfo data;
    unsigned long tmplongdata = htonl(tagBufferLen);

    //char pszURL[32];
    //DWORD hostIdSize = 256;
    //LocalConfigurator config;

    // detect all exchange servers on this domain
    /*ADInterface ad;
    list<string> serverCN;
    SVERROR rc = SVS_OK;
*/

    //// post information to CX server
    //if (strlen(szBuffer) > 0 && !bCxdown ) 
    //{			
    //	if(PostToSVServer( config.getHttp().ipAddress, config.getHttp().port, pszURL, TagDataBuffer, tagBufferLen ).failed())
    //	{
    //		cout << "Failed to post to CX server: " << config.getHttp().ipAddress << endl;
    //		cout << "Verify if CX server is running." << endl;
    //		rc = SVE_FAIL;
    //		break;
    //	}
    //	else
    //	{
    //		cout << "Successfully Posted tag information to CX\n"<<endl;	
    //		bResult	 = true;
    //	}
    //}

    /*do 
    {	
        if (cliStream.send_n ((void *)&tmplongdata, sizeof tmplongdata) == -1)
        {
            DebugPrintf ("FAILED Couldn't able to send total Tag data length to remote server");
            hr = E_FAIL;
            break;
        }
    
        else if (cliStream.send_n ((void *)TagDataBuffer, tagBufferLen) == -1)
        {
            DebugPrintf ("FAILED Couldn't able to send Tag data to remote server");
            hr = E_FAIL;
            break;
        }
        
        else if (cliStream.recv_n ((void *)&data, sizeof data) <= 0)
        {
            DebugPrintf ("FAILED Couldn't able to get acknowledgement from remote server");
            hr = E_FAIL;
            break;
        }
        bResult	 = true;

        //if (TagDataBuffer) 
        //	delete[] TagDataBuffer;
    
        data.val = ntohl(data.val);
        data.str[data.val] = '\0';
        DebugPrintf ("INFO: data.val: %d, data.str: %s\n", data.val, data.str);
    
    } while(0);*/

    if(bResult)
    {
        gbTagSend = true;
        XDebugPrintf( "Successfully sent tags to following Server devices:\n");
        for (size_t i = 0; i < vServerDeviceID_t.size(); i++)
        {
            std::string strServerDeviceID = vServerDeviceID_t[i]->data;
            XDebugPrintf("%s\t", strServerDeviceID.c_str());
        }
        
        inm_printf( "\nSuccessfully sent tags to the Remote Server ...\n");
        hr = S_OK;
    }
    else
    {
        hr = GetLastError();
        std::stringstream ss;
        ss << "FAILED to Send Tag to the Remote Server for following server devices: ";
        for (size_t i = 0; i < vServerDeviceID_t.size(); i++)
        {
            ss << vServerDeviceID_t[i]->data << "\t";
        }
        inm_printf("\n%s Error code:%d\n", ss.str().c_str(), hr);
        AppConsistency::Get().m_vacpLastError.m_errMsg = ss.str();
        AppConsistency::Get().m_vacpLastError.m_errorCode = hr;
    }

    if (TagDataBuffer)
    {
            delete[] TagDataBuffer;
            TagDataBuffer = NULL;
    }

//end: 
    vServerDeviceID_t.clear();
    vTags.clear();

    return hr;

}

bool IsServerDeviceIDAlreadyExists(vector<ServerDeviceID_t*>& vServerDeviceID_t, const char* pServerDeviceID)
{
    bool ret = false;
    size_t cDevice = vServerDeviceID_t.size();
    std::string strServerDeviceId;
    
    for(DWORD  i = 0; i <cDevice; i++)
    { 
        strServerDeviceId = vServerDeviceID_t[i]->data;
        if(!_stricmp(strServerDeviceId.c_str(), pServerDeviceID))
        {
            ret = true;
            break;
        }
    }
    return ret;
}
#endif // WINNT_32 >0x500

HRESULT VacpPostToCxServer( const char *pszSVServerName,
                       SV_INT InmHttpPort,
                       const char *pchInmPostURL,
                       char *pchInmPostBuffer,
                       SV_ULONG dwInmPostBufferLength )
{
    HRESULT rc;
    HRESULT hResult = S_OK;
    DWORD dwQueryResponseLength = 10240;
    char *pQueryResponse = NULL;
    HINTERNET hInminternetSession = NULL;
    HINTERNET hInminternet = NULL;
    HINTERNET hInminternetRequest = NULL;

    do
    {
        inm_printf( "ENTERED VacpPostToCxServer()...\n" );

        if( ( NULL == pchInmPostURL ) ||( NULL == pchInmPostBuffer ) )
        {
            hResult = E_INVALIDARG;
            inm_printf( "FAILED VacpPostToCxServer()... hResult = %08X\n", hResult );
            break;
        }

        if( NULL == hInminternetSession )
        {
            hInminternetSession = InternetOpen( "SVHostAgent",INTERNET_OPEN_TYPE_DIRECT,
                NULL,NULL,INTERNET_FLAG_DONT_CACHE );
            if( NULL == hInminternetSession )
            {
                hResult = HRESULT_FROM_WIN32( GetLastError() );
                inm_printf( "FAILED InternetOpen()... hResult = %08X\n", hResult );
                break;
            }
        }

        if( NULL == hInminternet )
        {
            hInminternet = InternetConnect( hInminternetSession,
                pszSVServerName,
                InmHttpPort,
                "",NULL,INTERNET_SERVICE_HTTP,INTERNET_FLAG_DONT_CACHE,0 );
            if( NULL == hInminternet )
            {
                hResult = HRESULT_FROM_WIN32( GetLastError() );
                inm_printf( "FAILED InternetConnect()... hResult = %08X\n", hResult );
                break;
            }
        }

        hInminternetRequest = HttpOpenRequest( hInminternet,
            "POST",pchInmPostURL,NULL, NULL,NULL,INTERNET_FLAG_DONT_CACHE,0 );
        if( NULL == hInminternetRequest )
        {
            hResult = HRESULT_FROM_WIN32( GetLastError() );
            inm_printf( "FAILED HttpOpenRequest()... URL: %s; hResult = %08X\n", pchInmPostURL, hResult );
            break;
        }

        DWORD dwTimeout = 30 * 1000;
        if(!InternetSetOption(hInminternetRequest, INTERNET_OPTION_RECEIVE_TIMEOUT, &dwTimeout, sizeof(dwTimeout))) {
            inm_printf( "CALLING Failed Setting Timeout()...\n" );
        }

        char* pchExtraHeader = "Content-Type: application/x-www-form-urlencoded\n";

        if( FALSE == HttpAddRequestHeaders( hInminternetRequest,
            pchExtraHeader,
            (DWORD)(strlen( pchExtraHeader )),
            HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE ) )
        {
            hResult = HRESULT_FROM_WIN32( GetLastError() );
            inm_printf( "FAILED HttpAddRequestHeaders()... hResult = %08X\n", hResult );
            break;
        }

        if( FALSE == HttpSendRequest( hInminternetRequest,NULL,0,pchInmPostBuffer,dwInmPostBufferLength ) )
        {
            hResult = HRESULT_FROM_WIN32( GetLastError() );
            inm_printf( "FAILED HttpSendRequest()... hResult = %08X\n", hResult );
            break;
        }

        pQueryResponse = new char[ dwQueryResponseLength ];
        if( NULL == pQueryResponse )
        {
            hResult = E_OUTOFMEMORY;
            break;
        }

        DWORD dwIndex = 0;
        DWORD dwError = 0;

        if( FALSE == HttpQueryInfo( hInminternetRequest,
            HTTP_QUERY_STATUS_CODE,
            pQueryResponse,
            &dwQueryResponseLength,
            NULL ) )
        {
            hResult = HRESULT_FROM_WIN32( GetLastError() );
            inm_printf( "FAILED HttpQueryInfo()... hResult = %08X\n", hResult );
            break;
        }       

        if( HTTP_STATUS_OK != atoi( pQueryResponse ) )
        {
            hResult = E_FAIL;
            inm_printf( "FAILED HTTP_STATUS: %d; hResult = %08X\n", atoi( pQueryResponse ), hResult );
            break;
        }
        DWORD dwNumberOfBytesAvailable = 0;
        if(FALSE == InternetQueryDataAvailable(hInminternetRequest,&dwNumberOfBytesAvailable,0,0))
        {
            hResult = HRESULT_FROM_WIN32( GetLastError());
            inm_printf( "FAILED HttpQueryInfo()... hResult = %08X\n", hResult );
            break;
        }

        DWORD dwNumberOfBytesToRead = dwNumberOfBytesAvailable;
        DWORD dwNumberOfBytesRead = 0;
        if(FALSE == InternetReadFile(hInminternetRequest,pQueryResponse,dwNumberOfBytesToRead,&dwNumberOfBytesRead))
        {
            hResult = HRESULT_FROM_WIN32( GetLastError());
            inm_printf( "FAILED HttpQueryInfo()... hResult = %08X\n", hResult );
            break;
        }
        if(pQueryResponse != NULL)
        {
            inm_memcpy_s((void*)pchInmPostBuffer, dwInmPostBufferLength, (const void*)pQueryResponse, dwNumberOfBytesRead);
        }
    }
    while( FALSE );

    if( ( NULL != hInminternetRequest ) && !InternetCloseHandle( hInminternetRequest ) )
    {
        inm_printf( "FAILED InternetCloseHandle() =f %lu...\n",HRESULT_FROM_WIN32( GetLastError() ));
    }

    if( ( NULL != hInminternet ) && !InternetCloseHandle( hInminternet ) )
    {
        inm_printf("FAILED InternetCloseHandle()= %lu...\n", HRESULT_FROM_WIN32( GetLastError() ));
    }

    if( ( NULL != hInminternetSession ) && !InternetCloseHandle( hInminternetSession ) )
    {
        inm_printf( "FAILED InternetCloseHandle() = %lu...\n",HRESULT_FROM_WIN32( GetLastError()));
    }
    if( pQueryResponse )
    {
        delete[] pQueryResponse;
        pQueryResponse = NULL;
    }

    rc = hResult;
    inm_printf("Exiting %s\n", __FUNCTION__);
    return( rc );
}




int GetLicenseType(char * pchLicenseString)
{
    int iConsistencyLevel = ERROR_LICENSE_LEVEL;
    
    vector<std::string>LicenseElements;
    TCHAR* token;
    TCHAR* delim = (TCHAR*)":";
    token = strtok(pchLicenseString,delim);
    while(token != NULL)
    {
        if((strcmp("001 ",(const char*)token) != 0) && (strcmp("002 ",(const char*)token) != 0))
        {
            LicenseElements.push_back(token);
        }		
        token = strtok(NULL,delim);
    }
    
    if(LicenseElements.size() > 0)
    {
        vector<std::string>::iterator iter;
        iter = LicenseElements.begin();		
        while(iter != LicenseElements.end())
        {
            if(strcmpi(" Non Application",(*iter).c_str()) == 0)
            {
                iConsistencyLevel =  FS_LEVEL_CONSISTENCY;
                break;
            }
            else if( strcmpi(" Application",(*iter).c_str()) ==0)
            {
                iConsistencyLevel= APP_LEVEL_CONSISTENCY;
                break;
            }
            iter++;
        }
    }
    return iConsistencyLevel;
}

HRESULT Consistency::CheckProtectionState(ACSRequest_t& Request)
{
    HRESULT hr = S_OK;
    
    if (!gbUseDiskFilter)
        return hr;

    DebugPrintf("\nChecking driver write order state for the given disk\n");

    std::set<std::string>::iterator diskIter = Request.vProtectedDisks.begin();
    for (; diskIter != Request.vProtectedDisks.end(); diskIter++)
    {
        std::wstring diskSignature;
        if (!Request.GetDiskGuidFromDiskName(*diskIter, diskSignature))
        {
            std::stringstream ss;
            ss << "GetDiskGuidFromDiskName() FAILED for " << (*diskIter).c_str();
            DebugPrintf("%s\n", ss.str().c_str());
            g_vacpLastError.m_errMsg = ss.str();
            g_vacpLastError.m_errorCode = E_FAIL;
            return E_FAIL;
        }

        unsigned short signatureSize = diskSignature.size() * sizeof(WCHAR);

        WCHAR *diskGuid = new (std::nothrow) WCHAR[diskSignature.size()];

        if (diskGuid == NULL)
        {
            std::stringstream ss;
            ss << "failed to allocate " << signatureSize << " number of bytes.";
            inm_printf("Error: %s\n", ss.str().c_str());
            g_vacpLastError.m_errMsg = ss.str();
            g_vacpLastError.m_errorCode = E_FAIL;
            return E_FAIL;
        }

        inm_wmemcpy_s(diskGuid, diskSignature.size(), diskSignature.c_str(), diskSignature.size());

        etWriteOrderState DriverWriteOrderState;
        DWORD dwLastError = 0;

        if (CheckDriverStat(ghControlDevice, diskGuid, &DriverWriteOrderState, &dwLastError, signatureSize))
        {
            if (DriverWriteOrderState != ecWriteOrderStateData)
            {
                std::stringstream ss;
                ss << "For disk " << (*diskIter).c_str() << " driver write order state is : " << szWriteOrderState[DriverWriteOrderState];
                DebugPrintf("\n%s\n", ss.str().c_str());
                g_vacpLastError.m_errMsg = ss.str();
                g_vacpLastError.m_errorCode = VACP_E_DRIVER_IN_NON_DATA_MODE;
                return VACP_E_DRIVER_IN_NON_DATA_MODE;
            }
        }
        else
        {
            std::stringstream ss;
            ss << "Disk " << (*diskIter).c_str() << " is not protected";
            DebugPrintf("\n%s\n", ss.str().c_str());
            g_vacpLastError.m_errMsg = ss.str();
            g_vacpLastError.m_errorCode = S_FALSE;
            return S_FALSE;
        }
    }
    return hr;
}


HRESULT Consistency::CheckAndValidateDriverMode(ACSRequest_t Request)
{
    using std::string;
    USES_CONVERSION;    // declare locals used by the ATL macros

    char szVolumePath[MAX_PATH] = {0};
    char szVolumeNameGUID[MAX_PATH] = {0};
    DWORD dwVolPathSize = 0;
    BOOL bRet = 0;

    HRESULT hr = S_OK;
    DWORD driveMask = 0;	
    vector<string> vVolumes;
    vector<WCHAR *> vVolumeGUID;
    vector<TagData_t *> vTags;

    driveMask = Request.volumeBitMask;

    if (driveMask == 0)
    {
        // No volumes were selected. just return;
        DebugPrintf("\nNo volumes are selected.\n");
        return E_FAIL;
    }

    //remove debuplicate list of volumes 
    vector<std::string>::iterator volumeNameIter = Request.vApplyTagToTheseVolumes.begin();
    vector<std::string>::iterator volumeNameEnd= Request.vApplyTagToTheseVolumes.end();
    while(volumeNameIter != volumeNameEnd)
    {
        std::string volname = *volumeNameIter;
        if(!IsVolumeAlreadyExists(vVolumes,volname.c_str()))
        {
            vVolumes.push_back(volname);
            gvVolumes.push_back(volname);
        }
        ++volumeNameIter;
    }


    char szVolumeName[3];

    szVolumeName[0] = 'X';
    szVolumeName[1] = ':';
    szVolumeName[2] = '\0';

    unsigned long GuidSize = GUID_SIZE_IN_CHARS * sizeof(WCHAR);
     
    //Get Volume GUIDs
     vector<std::string>::iterator volIter = vVolumes.begin();
     vector<std::string>::iterator volend = vVolumes.end();
     while(volIter != volend)
     {
         std::string volname = *volIter;
         WCHAR *VolumeGUID = new WCHAR[GUID_SIZE_IN_CHARS];
         MountPointToGUID((PTCHAR)volname.c_str(),VolumeGUID,GuidSize);
         vVolumeGUID.push_back(VolumeGUID);
         ++volIter;
     }
     
     string strVolName;
     for(unsigned int i = 0; i < Request.tApplyTagToTheseVolumeMountPoints_info.dwCountVolMountPoint; i++)
     {
        strVolName = Request.tApplyTagToTheseVolumeMountPoints_info.vVolumeMountPoints[i].strVolumeName; 
        WCHAR *VolumeGUID = new WCHAR[GUID_SIZE_IN_CHARS];
        MountPointToGUID((PTCHAR)strVolName.c_str(),VolumeGUID,GuidSize);
        vVolumeGUID.push_back(VolumeGUID);
        gvVolumes.push_back(strVolName);
        
     }
    
    //if -s option is provided VACP will skip checking driver mode
    if(!SkipChkDriverMode(Request))
    {
        //Check whether driver is in data mode or not
        DebugPrintf("\nChecking driver write order state for the given volumes\n");
        int iTotalVolumes = (int)vVolumeGUID.size();
        int iNoOfVolumesInDataMode = 0; //use iNoOfVolumesInDataMode varible to find out whether 
        etWriteOrderState DriverWriteOrderState;
        DWORD dwLastError = 0;
        char szBuf[GUID_SIZE_IN_CHARS+1] = {0};
            
        for(int i = 0; i < iTotalVolumes;i++)
        {
            if (CheckDriverStat(ghControlDevice, vVolumeGUID[i], &DriverWriteOrderState, &dwLastError))
            {
				size_t countCopied = 0;
                wcstombs_s(&countCopied, szBuf, ARRAYSIZE(szBuf), vVolumeGUID[i], ARRAYSIZE(szBuf)-1);
                inm_sprintf_s(szVolumeNameGUID, ARRAYSIZE(szVolumeNameGUID), "\\\\?\\Volume{%s}\\", szBuf);
#if (_WIN32_WINNT >= 0x0501)
                bRet = GetVolumePathNamesForVolumeName(szVolumeNameGUID,szVolumePath,MAX_PATH-1,&dwVolPathSize);
#else
                bRet = 0;
#endif

                //For each volume driver state may be different. if given volume is not set for 
                //replication pair its driver state is ecWriteOrderStateUnInitialized. 
                //Driver must be in write order data state for Exact tag. if not tag will be either Approximate or 
                //Not guaranteed. By default vacp checks the driver state and than issue the tag. If driver write order state 
                //for any one of the the given set of volumes in not data mode, vacp will not issue tag.
                //To skip driver state check, user should provide -s option from command line. Here
                //vacp will issue the tag irrespective of mode of the driver.
                if(DriverWriteOrderState >= 4)
                {
                    if(bRet)
                    {
                        DebugPrintf("\nFor volume %s driver write order state is: %d (UKNOWN).",szVolumePath, DriverWriteOrderState);
                    }
                    else
                    {
                        DebugPrintf("\nFor volume \\\\?\\Volume{%s}\\ driver write order state is: %d (UKNOWN).",szBuf, DriverWriteOrderState);
                    }
                    continue;
                }
                if(DriverWriteOrderState == ecWriteOrderStateData) 
                {
                    iNoOfVolumesInDataMode++;
                    if(bRet)
                    {
                        DebugPrintf("\nFor volume %s driver write order state is: %s",szVolumePath, szWriteOrderState[DriverWriteOrderState]);;
                    }
                    else
                    {
                        DebugPrintf("\nFor volume \\\\?\\Volume{%s}\\ driver write order state is: %s ",szBuf, szWriteOrderState[DriverWriteOrderState]);;
                    }
                }
                else if(DriverWriteOrderState == ecWriteOrderStateUnInitialized)
                {
                    if(bRet)
                    {
                        DebugPrintf("\nVolume %s  is not part of replication pair", szVolumePath);
                    }
                    else
                    {
                        DebugPrintf("\nVolume \\\\?\\Volume{%s}\\  is not part of replication pair", szBuf);
                    }
                }
                else
                {
                    if(bRet)
                    {
                        inm_printf("\nFor volume %s driver write order state is: %s",szVolumePath, szWriteOrderState[DriverWriteOrderState]);;
                    }
                    else
                    {
                        inm_printf("\nFor volume \\\\?\\Volume{%s}\\ driver write order state is: %s",szBuf, szWriteOrderState[DriverWriteOrderState]);;
                    }

                }
                ZeroMemory(szBuf,GUID_SIZE_IN_CHARS+1);
                ZeroMemory(szVolumeNameGUID,MAX_PATH);
            }
            else
            {
                DebugPrintf("ERROR: CheckDriverState() failed");
            }
        }
        inm_printf("\n");
        if(iNoOfVolumesInDataMode < iTotalVolumes)
        {
            inm_printf("Driver mode for one or all the give volume(s) are not in write ordered data mode.\n");
            inm_printf("Not able to send tags to the driver.");
            hr = E_FAIL;
            goto end;
        }
    }
    else
    {
        inm_printf("\nDriver mode checking skipped.\n");
    }

end: 
     return hr;
}
#if (_WIN32_WINNT > 0x500)
bool VssAppConsistencyProvider::GetApplicationConsistencyState()
{
    if (gbSyncTag && !gbUseInMageVssProvider)
    {
        return vwriter->GetApplicationConsistencyState();
    }
    else
    {
        return bSentTagsSuccessfully;
    }
}
#endif //(_WIN32_WINNT > 0x500)

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

void Consistency::Conclude(HRESULT hr)
{
    try
    {
        gExitCode = (ULONG)hr; // gExitCode required for v-a

        std::string universalTime = boost::lexical_cast<std::string>(GetTimeInSecSinceEpoch1970());
        DebugPrintf("Entered Conclude() at %s\n", universalTime.c_str());
        m_tagTelInfo.Add(VacpEndTimeKey, universalTime);
        DebugPrintf("VacpEndTime %s .\n", universalTime.c_str());

        if (S_OK != gExitCode) 
        {
            m_vacpErrorInfo.SetAttribute(VacpAttributes::END_TIME, universalTime);
        }

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

        if (IS_APP_TAG(m_TagType))
        {
            AppConsistency::Get().CleanupVssProvider();
        }



        HRESULT hrLocal;

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
                    DebugPrintf( "\niter->m_errorCode = %08x\n",itr->m_errorCode);
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

        if (ghrErrorCode != S_OK)
        {
            hrLocal = ghrErrorCode;
        }
        else
        {
            hrLocal = hr;
        }

        std::stringstream strStatus;
        if (S_OK != hrLocal)
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
                strStatus << (ULONG)hrLocal;
                break;
            }
            strStatus << SearchEndKey;
        }
        if (gbPostScript)
        {
            ExecuteScript(gstrPostScript, ST_POSTSCRIPT);
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

        m_AcsRequest.Reset();

        m_tagTelInfo.Add(VacpExitStatus, boost::lexical_cast<std::string>((ULONG)hr));

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

            NextLogFileName(g_strLogFilePath, strPreVacpLogFile, strVacpLogFile, prefix);
            try
            {
                if (IS_APP_TAG(m_TagType) && (S_OK != hrLocal) && !m_vacpErrorInfo.empty())
                {
                    DebugPrintf("\nVacp Health Issue generated.\n");
                    std::string     tagGuid = m_vacpErrorInfo.get(VacpAttributes::TAG_GUID);
                    if (!tagGuid.empty()) {
                        AppConsistency::Get().m_tagTelInfo.Add(TagGuidKey, std::string((char*)tagGuid.c_str()));
                        DebugPrintf("\nVacp Health Issue generated for Tag Guid: %s\n", tagGuid.c_str());
                        std::stringstream   errorFile;
                        errorFile << g_strLogFilePath << tagGuid << ".json";
                        std::ofstream errFileStream(errorFile.str().c_str(), std::ios::out | std::ios::app);
                        try
                        {
                            std::string     serializedString = JSON::producer<VacpErrorInfo>::convert(m_vacpErrorInfo);
                            DebugPrintf("\nconverted json string is %s\n", serializedString.c_str());
                            errFileStream << serializedString;
                        }
                        catch (JSON::json_exception& jsone)
                        {
                            std::string strJsonErr = "Converting object to JSON string caught json exception. ";
                            strJsonErr += jsone.what();
                            DebugPrintf("\n%s\n", strJsonErr.c_str());
                        }
                        errFileStream.close();
                    }
                }

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

        inm_printf("Exiting with status %lu %s\n", (ULONG)hr, strStatus.str().c_str());

        m_perfCounter.Clear();
        m_tagTelInfo.Clear();

        DebugPrintf("%s : error 0x%x hr 0x%x gExitCode 0x%x\n",
            __FUNCTION__,
            m_lastRunErrorCode,
            hr,
            gExitCode);

        m_lastRunErrorCode = hr;

        bool isExitVacpRequested = false;
        if (S_OK != hrLocal)
        {
            switch (hrLocal)
            {
            case VACP_SHARED_DISK_CLUSTER_INFO_FETCH_FAILED:
            case VACP_SHARED_DISK_CLUSTER_HOST_MAPPING_FETCH_FAILED:
            case VACP_SHARED_DISK_CURRENT_NODE_EVICTED:
            case VACP_SHARED_DISK_CLUSTER_UPDATED:
                DebugPrintf("%s : shared disk error 0x%x hr, quitting VACP \n",
                    __FUNCTION__,
                    hrLocal);
                isExitVacpRequested = true;
                break;
            default :
                break;
            }

            if (isExitVacpRequested)
            {
                g_Quit->Signal(true);
            }
        }
    }
    catch (const std::exception &e)
    {
        DebugPrintf("%s: caught exception %s\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        inm_printf("%s: caught an unknown exception.\n", FUNCTION_NAME);
    }
    return;
}

HRESULT Consistency::GenerateDrivesMappingFile(std::string &errmsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    HRESULT hr = S_OK;

    DWORD dwVolFindError = ERROR_SUCCESS;
    std::map<std::string, std::string> mapVolGuidToDriveLetter;
    //
    // Collect volume-guid to volume-drive-letter maping and persist to '<Agent-Install-Path>/failover/data/inmage_drive.txt'
    //      This file will be given as input to bootup script on recovery
    //

    TCHAR volName[MAX_PATH] = { 0 };
    HANDLE hFindVol = FindFirstVolume(volName, MAX_PATH);
    if (INVALID_HANDLE_VALUE == hFindVol)
    {
        dwVolFindError = GetLastError();
        hr = E_FAIL;
        std::stringstream ss;
        ss << FUNCTION_NAME << ": FindFirstVolume failed with error " << dwVolFindError;
        errmsg = ss.str();
        DebugPrintf(SV_LOG_ERROR, "%s\n", ss.str().c_str());
    }

    while (ERROR_SUCCESS == dwVolFindError)
    {
        std::list<std::string> mntPaths;

        //
        // Ignoring GetVolumeMountPoints return codes and continueing with other volumes
        //
        GetVolumeMountPoints(volName, mntPaths, errmsg);

        for each (std::string volPath in mntPaths)
        {
            //
            // Considering only drive-letters as mount-points does not change automatically.
            //
            if (!volPath.empty() && volPath.length() <= 3)
            {
                mapVolGuidToDriveLetter[volName] = volPath;

                DebugPrintf(SV_LOG_DEBUG, " Found drive letter %s for the volume %s\n", volPath.c_str(), volName);

                break;
            }
        }

        RtlSecureZeroMemory(volName, ARRAYSIZE(volName));
        if (!FindNextVolume(hFindVol, volName, MAX_PATH))
        {
            dwVolFindError = GetLastError();

            if (ERROR_NO_MORE_FILES != dwVolFindError)
            {
                hr = E_FAIL;
                std::stringstream ss;
                ss << FUNCTION_NAME << ": FindNextVolume failed with error " << dwVolFindError;
                errmsg = ss.str();
                DebugPrintf(SV_LOG_ERROR, "%s\n", ss.str().c_str());
            }

            break;
        }
    }

    if (hFindVol != INVALID_HANDLE_VALUE)
    {
        FindVolumeClose(hFindVol);
        hFindVol = INVALID_HANDLE_VALUE;
    }

    if (!mapVolGuidToDriveLetter.empty())
    {
        //
        // Populate the volumeGuid!@!@!DriveLetter!@!@! to '<Agent-Install-Path>/failover/data/inmage_drive.txt'
        //
        std::string drivesFile;
        if (S_OK == getAgentInstallPath(drivesFile))
        {
            drivesFile += DrivesMappingFile;
            try
            {
                if (!boost::filesystem::exists(drivesFile))
                {
                    CreatePaths::createPathsAsNeeded(drivesFile);
                }

                std::ofstream drivesOut(drivesFile, ios::out | ios::trunc);
                for each (auto volGuidDrivePair in mapVolGuidToDriveLetter)
                {
                    drivesOut << volGuidDrivePair.first << "!@!@!"
                        << volGuidDrivePair.second << "!@!@!" << std::endl;
                }

                drivesOut.close();
            }
            catch (const std::exception ex)
            {
                hr = E_FAIL;
                std::stringstream ss;
                ss << FUNCTION_NAME << ": failed to create file " << drivesFile << " with exception " << ex.what();
                errmsg = ss.str();
                DebugPrintf(SV_LOG_ERROR, "%s\n", ss.str().c_str());
            }
            catch (...)
            {
                hr = E_FAIL;
                std::stringstream ss;
                ss << FUNCTION_NAME << ": failed to create file " << drivesFile << " with an unknown exception";
                errmsg = ss.str();
                DebugPrintf(SV_LOG_ERROR, "%s\n", ss.str().c_str());
            }
        }
        else
        {
            hr = E_FAIL;
            std::stringstream ss;
            ss << FUNCTION_NAME << ": failed to get AgentInstallPath ";
            errmsg = ss.str();
            DebugPrintf(SV_LOG_ERROR, "%s\n", ss.str().c_str());
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return hr;
}

CrashConsistency& CrashConsistency::CreateInstance(const CLIRequest_t &CmdRequest, bool IoBarrierRequired)
{
    if (NULL == m_theCrashC)
    {
        boost::unique_lock<boost::mutex> lock(m_theCrashCLock);
        if (NULL == m_theCrashC)
        {
            m_theCrashC = new CrashConsistency(CmdRequest, IoBarrierRequired);
        }
    }
    return *m_theCrashC;
}

CrashConsistency& CrashConsistency::Get()
{
    if (NULL == m_theCrashC)
    {
        throw "CrashConsistency instance not created.";
    }
    return *m_theCrashC;
}

AppConsistency& AppConsistency::CreateInstance(const CLIRequest_t &CmdRequest)
{
    if (NULL == m_theAppC)
    {
        boost::unique_lock<boost::mutex> lock(m_theAppCLock);
        if (NULL == m_theAppC)
        {
            m_theAppC = new AppConsistency(CmdRequest);
        }
    }
    return *m_theAppC;
}

AppConsistency& AppConsistency::Get()
{
    if (NULL == m_theAppC)
    {
        throw "CrashConsistency instance not created.";
    }
    return *m_theAppC;
}

BaselineConsistency& BaselineConsistency::CreateInstance(const CLIRequest_t &CmdRequest)
{
    if (!m_theBaseline)
    {
        boost::unique_lock<boost::mutex> lock(m_theBaselineLock);
        if (!m_theBaseline)
        {
            m_theBaseline = new BaselineConsistency(CmdRequest);
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

void AddToVacpFailMsgs(const int &failureModule, const std::string &errorMsg, const unsigned long &errorCode, const TAG_TYPE tagType)
{
    if (IS_CRASH_TAG(tagType))
        CrashConsistency::Get().AddToVacpFailMsgs(failureModule, errorMsg, errorCode);

    else if (IS_APP_TAG(tagType))
        AppConsistency::Get().AddToVacpFailMsgs(failureModule, errorMsg, errorCode);

    else if (IS_BASELINE_TAG(tagType))
        BaselineConsistency::Get().AddToVacpFailMsgs(failureModule, errorMsg, errorCode);
}

VacpLastError& GetVacpLastError(const TAG_TYPE tagType)
{
    if (IS_CRASH_TAG(tagType))
        return CrashConsistency::Get().m_vacpLastError;

    else if (IS_APP_TAG(tagType))
        return AppConsistency::Get().m_vacpLastError;

    else if (IS_BASELINE_TAG(tagType))
        return BaselineConsistency::Get().m_vacpLastError;
}

void ExitVacp(HRESULT hr, bool bExitWithThisValue)
{
	inm_printf("Entered ExitVacp() at %s\n", GetLocalSystemTime().c_str());

	if (g_bThisIsMasterNode && g_pServerCommunicator)
	{
		g_pServerCommunicator->StopCommunicator(CommunicatorTypeSched);
	}

	SafeCloseHandle(gOverLapIO.hEvent);
	SafeCloseHandle(gOverLapTagIO.hEvent);
	SafeCloseHandle(gOverLapRevokationIO.hEvent);
	SafeCloseHandle(ghControlDevice);

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

	if (!bExitWithThisValue)
	{
		if (ghrErrorCode != S_OK)
		{
			inm_printf("Exited at %s with 0x%x\n",GetLocalSystemTime().c_str(), ghrErrorCode);
			exit(ghrErrorCode);
		}
	}
	
	inm_printf("Exited at %s with 0x%x\n",GetLocalSystemTime().c_str(), hr);
	exit((ULONG)hr);
}
