#include "StdAfx.h"

using namespace std;

#include "argparser.h"
#include "VssWriter.h"
#include "vacp.h"
#include "util.h"
#include "VacpErrorCodes.h"

#include <atlconv.h>

#include <list>

#include <boost/regex.hpp>

extern bool gbIsOSW2K3SP1andAbove;
extern bool gbRemoteSend;
extern bool	gbSyncTag;
extern bool gbVerify;
extern bool gbPersistSnapshots;
extern bool gbDeleteSnapshots;
extern bool gbPersist;
extern bool bDoNotIncludeWritrs;
extern bool g_bCSIp;
extern std::string g_strCxIp;
extern DWORD g_dwCxPort;
extern bool gbPrint;

extern bool    gbTagLifeTime;
extern bool    gbLTForEver;
extern bool    gbIgnoreNonDataMode;//continues to issue the tag even if one of the volumes is not in data mode

extern unsigned long long    gulLTMins;//LT:LifeTime in minutes
extern unsigned long long    gulLTHours;//60 minutes
extern unsigned long long    gulLTDays;//24 hours
extern unsigned long long    gulLTWeeks ;//7 days
extern unsigned long long    gulLTMonths;//30 days
extern unsigned long long    gulLTYears ;//365 days
extern bool gbSkipUnProtected;
extern HRESULT ghrErrorCode;
extern bool gbEnumSW;
extern bool gbIncludeAllApplications;
extern bool gbSystemLevelTag;
extern bool gbAllVolumes;
extern bool gbPreScript;
extern bool gbPostScript;
extern std::string gstrPreScript;
extern std::string gstrPostScript;

extern bool gbCrashTag;
extern bool gbBaselineTag;
extern std::string gStrBaselineId;
extern std::string gStrTagGuid;

extern bool gbDeleteAllVacpPersistedSnapshots;
extern bool gbDeleteSnapshotIds;
extern bool gbDeleteSnapshotSets;

extern bool g_bDistributedVacp;
extern bool g_bMasterNode;
extern bool g_bCoordNodes;
extern bool g_bThisIsMasterNode;
extern std::string g_strMasterHostName;
extern std::string g_strMasterNodeFingerprint;
extern std::string g_strLocalHostName;
extern std::string g_strLocalIpAddress;
extern std::string g_strRcmSettingsPath;
extern std::string g_strProxySettingsPath;
extern strset_t g_vLocalHostIps;
extern unsigned short g_DistributedPort;

extern short gsVerbose;
extern bool  gbParallelRun;
extern bool  gbVToAMode;
extern bool  gbIRMode;

extern FailMsgList g_VacpFailMsgsList;

extern void inm_printf(const char * format, ...);


TCHAR chSRAppList[] = {"SystemFiles;Registry;EventLog;WMI;COM+REGDB;FS;CA;AD;FRS;BootableState;\
ServiceState;ClusterService;VSSMetaDataStore;PerformanceCounter;;ADAM;ClusterDB;TSG;DFSR;ASR"};
std::string strNewApps;

TagArgParser::TagArgParser(CLIRequest_t * ctx, int argc, _TCHAR* argv[]): m_Context(ctx)
{
    m_Context->DoNotIncludeWriters = false;
    m_Context->bFullBackup = false;
    m_Context->bPrintAppList = false;
    m_Context->bSkipChkDriverMode= false;
    m_Context->bSkipUnProtected= true;//false; by-default Vacp will have "-su" switch behaviour

    m_Context->bWriterInstance = false;

    m_Context->bApplication = false;
    m_Context->bTag = false;
    m_Context->bServerIP = false;
    m_Context->bServerDeviceID = false;
    m_Context->usServerPort= DEFAULT_SERVER_PORT; 
    m_Context->bRemote = false;
    m_Context->bProvider = false;
    
    //default method of applying tag is Asynchronous. User has to specify of sync to use synchronous tag
    m_Context->bSyncTag = false; 
    m_Context->bDoNotIssueTag = false;
    m_Context->bVerify = false;
    
    m_Context->bTagTimeout = false;
    m_Context->dwTagTimeout = INFINITE;

    m_Context->bDistributed = false;
    m_Context->bMasterNode = false;
    m_Context->bCoordNodes = false;
    m_Context->bDelete = false;
    m_Context->bPersist = false;
    m_Context->bEnumSW = false;
    m_Context->bCSIP = false;
    m_Context->bFullBackup = false;
    m_Context->bDeleteAllVacpPersitedSnapshots =false;
    m_Context->bDeleteSnapshotIds = false;
    m_Context->bDeleteSnapshots = false;
    m_Context->bDeleteSnapshotSets = false;
    m_Context->bIgnoreNonDataMode = false;
    m_Context->bPrintAppList = false;
    m_Context->bTagLifeTime = false;
    m_Context->bTagLTForEver = false;
    m_Context->TagType = INVALID;
    
    m_Context->distributedTagGuid = GUID_NULL;

    
    bool terminate = false;
    for (int i = 1; i < argc; i++)
    {
        if(argv[i][0] == '-') 
        {
            if((stricmp((argv[i]+1),OPTION_VOLUME) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                ParseArguments(m_Context->Volumes,argv[i]);
            }
            else if((stricmp((argv[i]+1),OPTION_VOLUME_GUID) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                ParseArguments(m_Context->VolumeGuids,argv[i]);
            }
            
            else if((stricmp((argv[i]+1),OPTION_TAGNAME) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                m_Context->bTag = true;
                if (!ParseTagList(m_Context->BookmarkEvents, argv[i]))
                {
                    terminate = true;
                    break;
                }

            }
            else if((stricmp((argv[i]+1),OPTION_SKIPDRVCHK) == 0))
            {
                m_Context->bSkipChkDriverMode= true;
            }
            else if((stricmp((argv[i]+1),OPTION_SKIP_UNPROTECTED) == 0))
            {
                m_Context->bSkipUnProtected= true;
                gbSkipUnProtected = true;
            }
            else if((stricmp((argv[i]+1),OPTION_APPLICATION) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                //check if "-a sr" option is used for system recovery
                std::string strSRToken =argv[i];
                size_t nSize = strSRToken.find("sr");
                if((nSize != strSRToken.npos) && (nSize >= 0))
                {
                    
                    if(ExtractAppsWithSR(string(argv[i]),strNewApps))
                    {
                        ParseApplicationArguments(m_Context->m_applications,(_TCHAR*)(strNewApps.c_str()));
                    }
                }
                else
                {
                    m_Context->bApplication = true;
                    ParseApplicationArguments(m_Context->m_applications,argv[i]);	
                }
                nSize = strSRToken.find("all");
                if((nSize != strSRToken.npos) && (nSize >= 0))
                {
                  gbIncludeAllApplications = true;
                }
            }
            else if((stricmp((argv[i]+1),OPTION_WRITER_INSTANCE_NAME) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }

                m_Context->bWriterInstance = true;
                ParseApplicationArguments(m_Context->m_applications,argv[i]);	
            }
            else if((stricmp((argv[i]+1),OPTION_PRINT) == 0))
            {
                if (argc > 3)
                        throw new exception("Error in input");

                if (argc == 3)
                {
                    ParseApplicationArguments(m_Context->m_applications,argv[++i]);						
                }
                m_Context->bPrintAppList = true;				
                gbPrint = true;
            }
            else if((stricmp((argv[i]+1),OPTION_EXCLUDEAPP) == 0))
            {
                m_Context->DoNotIncludeWriters = true;
                bDoNotIncludeWritrs = true;
            }
            else if((stricmp((argv[i]+1),OPTION_FULLBACKUP) == 0))
            {
                m_Context->bFullBackup = true;
            }
            else if((stricmp((argv[i]+1),OPTION_NOTAG) == 0))
            {
                m_Context->bDoNotIssueTag = true;
            }
            else if((stricmp((argv[i]+1),OPTION_PROVIDER) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                m_Context->bProvider = true;
                m_Context->strProviderID = argv[i];

            }
            else if((stricmp((argv[i]+1),OPTION_SYNC_TAG) == 0))
            {
                m_Context->bSyncTag = true;
                gbSyncTag = true;
            }
            else if((stricmp((argv[i]+1),OPTION_TAG_TIMEOUT) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                m_Context->bTagTimeout = true;
                m_Context->dwTagTimeout= atoi(argv[i]);
            }
            
            else if((stricmp((argv[i]+1),OPTION_PERSIST_SNAPSHOTS) == 0))
            {
                m_Context->bPersist = true;
                gbPersist = true;
                gbPersistSnapshots = true;
            }
            else if((stricmp((argv[i]+1),OPTION_DISTRIBUTED_VACP) == 0))
            {
              m_Context->bDistributed = true;
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
              m_Context->bMasterNode = true;
              g_bMasterNode = true;
              g_strMasterHostName = string(argv[i]);
              std::string err;
              if (ValidateAndGetIPAddress(g_strMasterHostName, err).empty())
              {
                  // ValidateAndGetIPAddress return empty when g_strMasterHostName is FQDN.
                  // For V2A we use ipaddress and for A2A we use HostName extracted from FQDN
                  const size_t found = g_strMasterHostName.find('.'); // For A2A and V2A RCM -mn is supplied as FQDN
                  if (found != std::string::npos)
                  {
                      g_strMasterHostName = g_strMasterHostName.substr(0, found);
                  }
              }
              m_Context->strMasterHostName = g_strMasterHostName;
              inm_printf("TagArgParser: g_strMasterHostName=%s\n", g_strMasterHostName.c_str());
            }
            else if((stricmp((argv[i]+1),OPTION_COORDINATOR_NODES) == 0))
            {
              i++;
              if((i >= argc)|| (argv[i][0] == '-'))
              {
                terminate = true;
                break;
              }
              m_Context->bCoordNodes = true;
              g_bCoordNodes = true;
              std::vector<const char*> tmpvCoordinatorNodes;
              ParseCoordinatorNodesArguments(tmpvCoordinatorNodes, argv[i]);
              {
                  std::string err;
                  auto itr = tmpvCoordinatorNodes.begin();
                  for (/*empty*/; itr != tmpvCoordinatorNodes.end(); ++itr)
                  {
                      inm_printf("Adding coordinator node: %s\n", *itr);
                      std::string cnode(*itr);
                      std::string err;
                      if (ValidateAndGetIPAddress(cnode, err).empty())
                      {
                          // ValidateAndGetIPAddress return empty when cnode is FQDN.
                          // For V2A we use ipaddress and for A2A we use HostName extracted from FQDN
                          const size_t found = cnode.find('.'); // For A2A and V2A RCM -cn is supplied as FQDN
                          if (found != std::string::npos)
                          {
                              cnode = cnode.substr(0, found);
                          }
                      }
                      m_Context->vCoordinatorNodes.push_back(cnode);
                      inm_printf("TagArgParser: Added coordinator node: %s\n", cnode.c_str());
                  }
              }
            }
            else if ((stricmp((argv[i] + 1), OPTION_CLUSTER) == 0))
            {
                m_Context->bSharedDiskClusterContext = true;
            }
            else if((stricmp((argv[i]+1),OPTION_DISTRIBUTED_PORT) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                m_Context->usDistributedPort= atoi(argv[i]);
                g_DistributedPort = (unsigned short)atoi(argv[i]);
            }
            else if (stricmp((argv[i] + 1), OPTION_MASTER_NODE_FINGERPRINT) == 0)
            {
                i++;
                m_Context->strMasterNodeFingerprint = g_strMasterNodeFingerprint = std::string(argv[i]);
            }
            else if (stricmp((argv[i] + 1), OPTION_RCM_SETTINGS_PATH) == 0)
            {
                i++;
                m_Context->strRcmSettingsPath = g_strRcmSettingsPath = std::string(argv[i]);
            }
            else if (stricmp((argv[i] + 1), OPTION_PROXY_SETTINGS_PATH) == 0)
            {
                i++;
                m_Context->strProxySettingsPath = g_strProxySettingsPath = std::string(argv[i]);
            }
            else if((stricmp((argv[i]+1),OPTION_REMOTE) == 0))
            {
                m_Context->bRemote = true;
                gbRemoteSend = true;
            }
            else if((stricmp((argv[i]+1),OPTION_SERVER_DEVICE) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                m_Context->bServerDeviceID = true;
                ParseServerDeviceIDArguments(m_Context->vServerDeviceID,argv[i]);
            }
            else if((stricmp((argv[i]+1),OPTION_SERVER_IP) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                m_Context->bServerIP = true;
                if(strlen(argv[i]) > 15)
                {	
                    inm_printf("Invalid Server IP passed");
                    throw new exception("Error in input");
                }
                m_Context->strServerIP = argv[i];
                
            }
            else if((stricmp((argv[i]+1),OPTION_SERVER_PORT) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                m_Context->usServerPort = atoi(argv[i]);
            }
            
            else if((stricmp((argv[i]+1),OPTION_CS_IP) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                m_Context->bCSIP = true;
                g_bCSIp = true;
                if(strlen(argv[i]) > 15)
                {	
                    inm_printf("Invalid Configuration Server IP passed");
                    throw new exception("Error in input");
                }
                m_Context->strCSServerIP = argv[i];
                g_strCxIp = m_Context->strCSServerIP;
                
            }
            else if((stricmp((argv[i]+1),OPTION_CS_PORT) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                m_Context->usCSServerPort = atoi(argv[i]);
                g_dwCxPort = m_Context->usCSServerPort;
            }
            
            else if((stricmp((argv[i]+1),OPTION_DEL_SNAPSHOTS) == 0))
            {
                i++;
                if((i >= argc) || (argv[i][0] == '-'))
                {
                  terminate = true;
                  break;
                }
                else if(stricmp(argv[i],OPTION_DELETE_ALL_VACP_SNAPSHOTS) == 0)
                {
                  m_Context->bDeleteAllVacpPersitedSnapshots = true;
                  gbDeleteAllVacpPersistedSnapshots = true;
                }
                else if(stricmp(argv[i],OPTION_DELETE_SNAPSHOTIDS) == 0)
                {				        
                  i++;
                  if((i > argc) || (argv[i][0] == '-'))
                  {
                    terminate = true;
                    break;
                  }
                  else
                  {
                    ParseSnapshotArguments(m_Context->vDeleteSnapshotIds,argv[i]);
                    m_Context->bDeleteSnapshotIds = true;
                    gbDeleteSnapshotIds = true;
                  }
                }
                else if(stricmp(argv[i],OPTION_DELETE_SNAPSHOTSETS) == 0)
                {				
                  i++;
                  if((i > argc) || (argv[i][0] == '-'))
                  {
                    terminate = true;
                    break;
                  }
                  else
                  {
                    ParseSnapshotSetArguments(m_Context->vDeleteSnapshotSetIds,argv[i]);
                    m_Context->bDeleteSnapshotSets = true;
                    gbDeleteSnapshotSets = true;
                  }
                }
                m_Context->bDelete = true;
                gbDeleteSnapshots = true;
            }
            else if((stricmp((argv[i]+1),OPTION_VACP_VERIFY) == 0))
            {
                m_Context->bVerify	= true;
                gbVerify			= true;
                
            }
            else if (stricmp((argv[i]+1),OPTION_ENUMERATE_SYSTEMWRITER) == 0)
            {
              m_Context->bEnumSW = true;//enumerate System Writer
              gbEnumSW = true;
            }
            // TagLifeTime options
            else if((stricmp((argv[i]+1),OPTION_TAG_LIFE_TIME) == 0))
            {							
                m_Context->bTagLifeTime = true;
                gbTagLifeTime = true;
            }
            else if((stricmp((argv[i]+1),OPTION_TAG_LIFE_TIME_FOREVER) == 0))
            {					
                m_Context->bTagLTForEver = true;
                gbLTForEver = true;
            }
            else if((stricmp((argv[i]+1),OPTION_IGNORE_NON_DATA_MODE) == 0))
            {					
                m_Context->bIgnoreNonDataMode = true;
                gbIgnoreNonDataMode = true;
            }
            else if((stricmp((argv[i]+1),OPTION_TAG_LIFE_TIME_MINS) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                m_Context->ulMins = atoi(argv[i]);
                gulLTMins = m_Context->ulMins;
            }
            else if((stricmp((argv[i]+1),OPTION_TAG_LIFE_TIME_HOURS) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                m_Context->ulHours = atoi(argv[i]);
                gulLTHours = m_Context->ulHours;
            }
            else if((stricmp((argv[i]+1),OPTION_TAG_LIFE_TIME_DAYS) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                m_Context->ulDays = atoi(argv[i]);
                gulLTDays = m_Context->ulDays;
            }
            else if((stricmp((argv[i]+1),OPTION_TAG_LIFE_TIME_WEEKS) == 0))
            {
                i++;
                if((i >= argc)||(argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                m_Context->ulWeeks = atoi(argv[i]);
                gulLTWeeks = m_Context->ulWeeks;
            }
            else if ((stricmp((argv[i] + 1), OPTION_TAG_LIFE_TIME_MONTHS) == 0))
            {
                i++;
                if ((i >= argc) || (argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                m_Context->ulMonths = atoi(argv[i]);
                gulLTMonths = m_Context->ulMonths;
            }
            else if ((stricmp((argv[i] + 1), OPTION_TAG_LIFE_TIME_YEARS) == 0))
            {
                i++;
                if ((i >= argc) || (argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                m_Context->ulYears = atoi(argv[i]);
                gulLTYears = m_Context->ulYears;
            }
            else if ((stricmp((argv[i] + 1), OPTION_SYSTEMLEVELTAG) == 0))
            {
                gbSystemLevelTag = true;
                gbAllVolumes = true;
                ParseArguments(m_Context->Volumes, "all");
                }
                else if ((stricmp((argv[i] + 1), OPTION_PRESCRIPT) == 0))
                {
                i++;
                if ((i >= argc) || (argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                gbPreScript = true;
                gstrPreScript = argv[i];
            }
            else if ((stricmp((argv[i] + 1), OPTION_POSTSCRIPT) == 0))
            {
                i++;
                if ((i >= argc) || (argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                gbPostScript = true;
                gstrPostScript = argv[i];
                }
                else if ((stricmp((argv[i] + 1), OPTION_HELP) == 0) || (stricmp((argv[i] + 1), OPTION_QUESTIONMARK) == 0))
                {
                if (argc != 2)
                {
                    ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
                    throw new exception("Error in input");
                }
                else
                {
                    ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
                    terminate = true;
                    break;
                }
            }
            else if (stricmp((argv[i] + 1), OPTION_CRASH_CONSISTENCY) == 0)
            {
                m_Context->TagType = CRASH;
                gbCrashTag = true;
                }
                else if (0 == stricmp((argv[i] + 1), OPTION_HYDRATION))
                {
                i++;
                if (i >= argc)
                {
                    terminate = true;
                    break;
                }
                m_Context->strHydrationInfo = argv[i];
            }
            else if (stricmp((argv[i] + 1), OPTION_BASELINE) == 0)
            {
                i++;
                if ((i >= argc) || (argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                m_Context->bTag = true;
                if (!ParseTagList(m_Context->BookmarkEvents, argv[i]))
                {
                    terminate = true;
                    break;
                }
                m_Context->TagType = BASELINE;
                gbBaselineTag = true;
                gStrBaselineId = argv[i];
            }
            else if (stricmp((argv[i] + 1), OPTION_TAG_GUID) == 0)
            {
                i++;
                if ((i >= argc) || (argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                gStrTagGuid = argv[i];
                if (!IsValidGuid(gStrTagGuid))
                {
                    inm_printf("Error in input - tagguid not in valid format.\n");
                    terminate = true;
                    break;
                }
            }
            // for delayed start
            else if (stricmp((argv[i] + 1), OPTION_DELAYED_START) == 0)
            {
                i++;
                if ((i >= argc) || (argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                int delay = atoi(argv[i]);
                Sleep(delay * 1000);
            }
            else if ((stricmp((argv[i] + 1), OPTION_VERBOSE) == 0))
            {
                i++;
                if ((i >= argc) || (argv[i][0] == '-'))
                {
                    terminate = true;
                    break;
                }
                gsVerbose = (short)atoi(argv[i]);
            }
            else if ((stricmp((argv[i] + 1), OPTION_PARALLEL_RUN) == 0))
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
            else
            {
                ghrErrorCode = VACP_E_INVALID_COMMAND_LINE;
                throw new exception("Error in input");
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

    if (gbSystemLevelTag && (!gbCrashTag || gbParallelRun))
    {
        m_Context->bApplication = true;
        gbIncludeAllApplications = true;
        ParseApplicationArguments(m_Context->m_applications, "all");
    }

    // tagguid should be unique for each run in parallel mode
    if (gbParallelRun && !gStrTagGuid.empty())
    {
        terminate = true;
        inm_printf("Error in input - tagguid is not valid for parallel run\n");
    }
    
    if(terminate)
    {
        usage();
        ADD_TO_VACP_FAIL_MSGS(VACP_INVALID_ARGUMENT, "VACP_INVALID_ARGUMENT", VACP_INVALID_ARGUMENT);
        ExitVacp(VACP_INVALID_ARGUMENT,true);
    }
}


void TagArgParser::ParseArguments(vector<const char *>& cont ,_TCHAR* tokens, _TCHAR* seps)
{
    using std::string;
    USES_CONVERSION;

    _TCHAR* token;

    token = _tcstok(tokens,seps);
     
   while( token != NULL )
   {
        if(IsDuplicateEntryPresent(cont,token))
        {
            inm_printf("\nWARNING: Duplicate entry found for [%s] in passed parameters. Skipping it\n", token);
            
        }
        else
        {
            cont.push_back( T2CA(token) );
        }
        token = _tcstok( NULL, seps );
   }
   
}

void TagArgParser::ParseArguments(std::set<std::string>& cont, _TCHAR* tokens, _TCHAR* seps)
{
    USES_CONVERSION;

    _TCHAR* token;

    token = _tcstok(tokens, seps);

    while (token != NULL)
    {
        {
            cont.insert(T2CA(token));
        }
        token = _tcstok(NULL, seps);
    }
}

bool TagArgParser::ParseTagList(vector<const char*>& cont, _TCHAR* tokens, _TCHAR* seps)
{
    using std::string;
    USES_CONVERSION;

    _TCHAR* token;
    UINT16 totalLengthOfAllTags = 0;

    token = _tcstok(tokens, seps);

    while (token != NULL)
    {
        if (strlen(token) > VACP_MAX_TAG_LENGTH)
        {
            inm_printf("Error: string length of tag %s is more than supported maximum of %d.\n", token, VACP_MAX_TAG_LENGTH);
            return false;
        }

        if (IsDuplicateEntryPresent(cont, token))
        {
            inm_printf("\nWARNING: Duplicate entry found for [%s] in passed parameters. Skipping it\n", token);

        }
        else
        {
            cont.push_back(T2CA(token));
            totalLengthOfAllTags += (UINT16)strlen(token);

            if (totalLengthOfAllTags > MAX_LENGTH_OF_ALL_TAGS)
            {
                inm_printf("Error: total size of tags %d is more than supported maximum of %d.\n", totalLengthOfAllTags, MAX_LENGTH_OF_ALL_TAGS);
                return false;
            }
        }
        token = _tcstok(NULL, seps);
    }

    return true;
}


void TagArgParser::ParseApplicationArguments(std::vector<AppParams_t>& vAppParams,_TCHAR* tokens, _TCHAR* seps)
{
    using std::string;
    USES_CONVERSION;

    std::vector<const char *> AppArgs;

    ParseArguments(AppArgs,tokens);  //Tokens in the form:    AppName1:Componet1:Component2 , AppName2:Component1

    for (unsigned int iApp = 0; iApp < AppArgs.size(); iApp++)
    {
        const char *Args = AppArgs[iApp]; //args in the form        ApplicationName:<Component1>:<Component2>
        
        _TCHAR* token;
        _TCHAR* delim = "/";

        token = _tcstok((char *)Args, delim);

        if (token != NULL)
        {
            AppParams_t appParam;

            appParam.m_applicationName = T2CA(token); //First token MUST be always Applicatin Name

            token = _tcstok( NULL, delim );   // AppName and Component Names are delimited by ":" (Colon)
           
            while( token != NULL )
            {
                appParam.m_vComponentNames.push_back(T2CA(token)); //Get Component Name
        
                token = _tcstok( NULL, delim );
            }
            if (IsDupilicateAppCmpPresent(vAppParams, appParam))
            {
                inm_printf("\nWARNING: Duplicate entry found for [%s] in passed parameters. Skipping it\n", appParam.m_applicationName);
            }
            else
            {
                vAppParams.push_back(appParam);
            }
       }

    }
}

void TagArgParser::ParseSnapshotArguments(vector<const char *>& container, _TCHAR* tokens, _TCHAR* seps)
{
  ParseArguments(container,tokens);
}

void TagArgParser::ParseSnapshotSetArguments(vector<const char *>& container, _TCHAR* tokens, _TCHAR* seps)
{
  ParseArguments(container,tokens);
}

void TagArgParser::ParseServerDeviceIDArguments(vector<const char *>& cont ,_TCHAR* tokens, _TCHAR* seps)
{
    using std::string;
    USES_CONVERSION;

    ParseArguments(cont,tokens);  //Tokens in the form:   Device1,Device2...
}

void TagArgParser::ParseCoordinatorNodesArguments(vector<const char *>& cont ,_TCHAR* tokens, _TCHAR* seps)
{
    using std::string;
    USES_CONVERSION;

    ParseArguments(cont,tokens);  //Tokens in the form:   Device1,Device2...
}

TagArgParser::~TagArgParser()
{
 
}

#ifdef TEST
void TagArgParser::Print()
{
    std::stringstream ss;    
    for( vector<AppParams_t*>::iterator iter = m_Context->Applications.begin(); iter != m_Context->applications.end(); iter++)
    {
        ss << "AppName : " << iter->ApplicationName << " ";
        
        for( vector<_TCHAR*>::iterator iter2 = iter->ComponentNames.begin(); iter2 != iter->ComponentNames.end(); iter2++)
        {
            ss << "<< " << *iter2 << ">> ";        
        }

        ss << endl;
    }

    for( vector<_TCHAR*>::iterator iter = m_Context->Volumes.begin(); iter != m_Context->volumes.end(); iter++)
    {
        ss << *iter << endl;
    }
     ss << "";
    for( vector<_TCHAR*>::iterator iter = m_Context->BookmarkEvents.begin(); iter != m_Context->tags.end(); iter++)
    {
        ss << *iter << endl;
    } 

    inm_printf("%s\n", ss.string().c_str());
}

#endif

bool TagArgParser::IsDuplicateEntryPresent(vector<const char *>& cont,_TCHAR* token)
{
    bool ret = false;
    for(unsigned int i = 0; i < cont.size();i++)
    {
        if(!strcmp(cont[i],token))
        {
            ret = true;
            break;
        }
    }
    return ret;
}

bool TagArgParser::IsDupilicateAppCmpPresent(vector<AppParams_t>& cont,AppParams_t token)
{
    bool ret = false;
    for(unsigned int i = 0; i < cont.size();i++)
    {
        if(boost::iequals(cont[i].m_applicationName,token.m_applicationName))
        {
            for(unsigned int j = 0; cont[i].m_vComponentNames.size(); j++)
            {
                if(boost::iequals(cont[i].m_vComponentNames[j], token.m_vComponentNames[0]))
                {
                    ret = true;
                    break;
                }
            }
        }
    }
    return ret;

}

bool TagArgParser::IsValidGuid(const std::string& strGuid)
{
    const char *regexForGuid = "^[{]?[0-9a-fA-F]{8}-([0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}[}]?$";
    boost::regex guidRegex(regexForGuid);

    return boost::regex_match(strGuid, guidRegex);
}
