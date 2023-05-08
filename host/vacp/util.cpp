#include "stdafx.h"

using namespace std;

#include "diskutils.h"
#include "util.h"
#include "Communicator.h"
#include "VssWriter.h"
#include "VssRequestor.h"
#include "StreamEngine/StreamRecords.h"
#include "StreamEngine/StreamEngine.h"
#include "vacp.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include <Wbemidl.h>
#include "VacpErrorCodes.h"
#include "vacpwmiutils.h"

#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <boost/asio.hpp>

#pragma comment(lib, "wbemuuid.lib")

#define INMXXDONE() goto End
#define INMXXEXITD() End:
#define INMBUFSIZE            MAX_PATH
#define INMFILESYSNAMEBUFSIZE MAX_PATH

#define MAX_REG_KEY_SIZE_IN_CHARS 255

#define WIN2K8_SERVER_VER_MAJOR 0x06
#define WIN2K8_SERVER_VER_MINOR 0x00
#define WIN2K8_SERVER_VER_R2_MAJOR 0x06
#define WIN2K8_SERVER_VER_R2_MINOR 0x01

extern HANDLE		ghControlDevice;
extern OVERLAPPED	gOverLapIO;
extern OVERLAPPED	gOverLapTagIO;
extern OVERLAPPED	gOverLapRevokationIO;
extern bool			gbTagSend;
extern DWORD		gdwNumberOfVolumes;
extern DWORD		gdwTagTimeout;
extern GUID			gTagGUID;
extern bool			gbGetSyncVolumeStatus;
extern bool			gbGetVolumeStatus;
extern vector<string> gvVolumes;
extern PVOID gpTagOutputData;
extern HRESULT ghrErrorCode;
extern bool    gbSkipUnProtected;
extern bool    gbIsOSW2K3SP1andAbove;
extern bool    gbIOCompleteTimeOutOccurred;
extern bool gbPreScript;
extern bool gbPostScript;
extern std::string gstrPreScript;
extern std::string gstrPostScript;
extern bool g_bDistributedVacp;
extern bool g_bThisIsMasterNode;
extern std::string g_strLocalHostName;
extern std::string g_strLocalIpAddress;

extern void inm_printf(const char * format, ...);

#if(_WIN32_WINNT > 0x500)
extern VSS_ID g_ProviderGuid;
#endif
extern bool gbVerify;
extern bool gbUseDiskFilter;
extern bool gbUseInMageVssProvider;

extern FailMsgList g_VacpFailMsgsList;
extern FailMsgList g_MultivmFailMsgsList;
extern VacpLastError g_vacpLastError;

#define GUID_LEN	36
#define MAX_GUID_LEN 40


std::vector<Writer2Application_t>       vDynamicVssWriter2Apps;

// a mutex to protect the following gobal lists
boost::mutex g_diskInfoMutex;

std::vector<std::string>                g_vMountPoints;
std::map<std::wstring, std::string>     gDiskGuidMap;
std::map<std::wstring, std::string>     g_DynamicDiskIds;
std::map<std::wstring, std::string>     g_StorageSpaceDiskIds;
std::map<std::wstring, std::string>     g_StoragePoolDiskIds;

Writer2Application_t StaticVssWriter2Applications[] = {
    {"Oracle VSS Writer", "Oracle", STREAM_REC_TYPE_ORACLE_TAG,"","",""},
    {"System Writer", "SystemFiles", STREAM_REC_TYPE_SYSTEMFILES_TAG,"","",""},
    {"Registry Writer", "Registry", STREAM_REC_TYPE_REGISTRY_TAG,"","",""},
    {"MSDEWriter", "SQL", STREAM_REC_TYPE_SQL_TAG,"","",""},
    {"Event Log Writer", "EventLog", STREAM_REC_TYPE_EVENTLOG_TAG,"","",""},
    {"WMI Writer", "WMI", STREAM_REC_TYPE_WMI_DATA_TAG,"","",""},
    {"COM+ REGDB Writer", "COM+REGDB", STREAM_REC_TYPE_COM_REGDB_TAG,"","",""},
    {"FileSystem", "FS", STREAM_REC_TYPE_FS_TAG,"","",""},
    {"BookmarkEvent", "USERDEFINED", STREAM_REC_TYPE_USERDEFINED_EVENT,"","",""},
    {"Microsoft Exchange Writer", "Exchange", STREAM_REC_TYPE_EXCHANGE_TAG,"","",""},
    {"IIS Metabase Writer", "IISMETABASE", STREAM_REC_TYPE_IISMETABASE_TAG,"","",""},
    {"Certificate Authority", "CA", STREAM_REC_TYPE_CA_TAG,"","",""},
    {"NTDS", "AD", STREAM_REC_TYPE_AD_TAG,"","",""},
    {"Dhcp Jet Writer", "DHCP", STREAM_REC_TYPE_DHCP_TAG,"","",""},
    {"BITS Writer", "BITS", STREAM_REC_TYPE_BITS_TAG,"","",""},
    {"WINS Jet Writer", "WINS", STREAM_REC_TYPE_WINS_TAG,"","",""},
    {"Removable Storage Manager", "RSM", STREAM_REC_TYPE_RSM_TAG,"","",""},
    {"TermServLicensing", "TSL", STREAM_REC_TYPE_TSL_TAG,"","",""},
    {"FRS Writer", "FRS", STREAM_REC_TYPE_FRS_TAG,"","",""},
    {"Microsoft Writer (Bootable State)", "BootableState", STREAM_REC_TYPE_BS_TAG,"","",""},
    {"Microsoft Writer (Service State)", "ServiceState", STREAM_REC_TYPE_SS_TAG,"","",""},
    {"Cluster Service Writer", "ClusterService", STREAM_REC_TYPE_CLUSTER_TAG,"","",""},
    {"SqlServerWriter", "Sql2005", STREAM_REC_TYPE_SQL2005_TAG,"","",""},
    {"SqlServerWriter", "Sql2008", STREAM_REC_TYPE_SQL2008_TAG,"","",""},
    {"SqlServerWriter", "Sql2012", STREAM_REC_TYPE_SQL2012_TAG,"","",""},
    {"Microsoft Exchange Writer", "ExchangeIS", STREAM_REC_TYPE_EXCHANGEIS_TAG,"Exchange Information Store","",""},
    {"Microsoft Exchange Writer", "ExchangeRepl", STREAM_REC_TYPE_EXCHANGEREPL_TAG,"Exchange Replication Service","",""},
    {"SharePoint Services Writer","SharePoint",STREAM_REC_TYPE_SHAREPOINT_TAG,"","",""},
    {"OSearch VSS Writer","OSearch",STREAM_REC_TYPE_OSEARCH_TAG,"","",""},
    {"SPSearch VSS Writer","SPSearch",STREAM_REC_TYPE_SPSEARCH_TAG,"","",""},
    {"Microsoft Hyper-V VSS Writer","HyperV",STREAM_REC_TYPE_HYPERV_TAG,"","",""},
    {"ASR Writer","ASR",STREAM_REC_TYPE_ASR_TAG,"","",""},
    {"Shadow Copy Optimization Writer","ShadowCopyOptimization",STREAM_REC_TYPE_SC_OPTIMIZATION_TAG,"","",""},
    {"MSSearch Service Writer","MSSearch",STREAM_REC_TYPE_MSSEARCH_TAG,"","",""},
    {"InMageTagGuidDummyWriter","InMageTagGuidDummyWriter",STREAM_REC_TYPE_TAGGUID_TAG,"","",""},
    {"Task Scheduler Writer","TaskScheduler",STREAM_REC_TYPE_TASK_SCHEDULER_TAG,"","",""},
    {"VSS Metadata Store Writer","VSSMetaDataStore",STREAM_REC_TYPE_VSS_METADATA_STORE_TAG,"","",""},
    {"Performance Counters Writer","PerformanceCounter",STREAM_REC_TYPE_PERFORMANCE_COUNTER_TAG,"","",""},
    {"IIS Config Writer","IISConfig",STREAM_REC_TYPE_IIS_CONFIG_TAG,"","",""},
    {"FSRM Writer","FSRM",STREAM_REC_TYPE_FSRM_TAG,"","",""},
    {"ADAM Writer",	"ADAM", STREAM_REC_TYPE_ADAM_TAG,"","",""},
    {"Cluster Database", "ClusterDB", STREAM_REC_TYPE_CLUSTER_DB_TAG,"","",""},
    {"NPS VSS Writer","NPS",STREAM_REC_TYPE_NPS_TAG,"","",""},
    {"TS Gateway Writer","TSG", STREAM_REC_TYPE_TSG_TAG,"","",""},
    {"DFS Replication service writer","DFSR",STREAM_REC_TYPE_DFSR_TAG,"","",""},
    {"VMWare VSS Writer","VMWare",STREAM_REC_TYPE_VMWARESRV_TAG,"","",""},
    {"SystemLevel","SystemLevel",STREAM_REC_TYPE_SYSTEMLEVEL,"","",""},
    {"Application VSS Writer","AppVssWriter",STREAM_REC_TYPE_DYNMICALLY_DISCOVERED_NEW_VSSWRITER,"","",""},
    { "Crash", "CRASH", STREAM_REC_TYPE_CRASH_TAG, "", "", ""},
    { "Hydration", "HYDRATION", STREAM_REC_TYPE_HYDRATION, "", "", "" },
    { "Baseline", "BASELINE", STREAM_REC_TYPE_BASELINE_TAG, "", "", "" },
    { "ClusterInfo", "CLUSTERINFO", STREAM_REC_TYPE_CLUSTER_INFO_TAG, "", "", "" }
};
HRESULT ComInitialize();

unsigned long long GetTimeInSecSinceEpoch1970()
{
    ULONGLONG secSinceEpoch = boost::posix_time::time_duration(boost::posix_time::second_clock::universal_time()
        - boost::posix_time::ptime(boost::gregorian::date(1970, 1, 1))).total_seconds();

    return secSinceEpoch;
}

DWORD GetVolumeMountPoints(const std::string& volumeGuid, std::list<std::string>& mountPoints, std::string &errmsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    DWORD dwRet = ERROR_SUCCESS;
    DWORD returnLength = 0;
    std::string strbuffer(MAX_PATH + 1, '\0');
    do
    {
        RtlSecureZeroMemory(&strbuffer[0], strbuffer.length());

        if (!GetVolumePathNamesForVolumeName(volumeGuid.c_str(), &strbuffer[0], strbuffer.length(), &returnLength))
        {
            dwRet = GetLastError();
            if (ERROR_MORE_DATA == dwRet)
            {
                dwRet = ERROR_SUCCESS;
                continue;
            }

            std::stringstream ss;
            ss << FUNCTION_NAME << ": GetVolumePathNamesForVolumeName failed with error " << dwRet << " for the volume " << volumeGuid;
            errmsg = ss.str();
            DebugPrintf(SV_LOG_ERROR, "%s\n", ss.str().c_str());
            break;
        }

        for (TCHAR* path = &strbuffer[0]; path[0] != '\0'; path += lstrlen(path) + 1)
            mountPoints.push_back(path);

        break;

    } while (true);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return dwRet;
}

// Get the volume's root path name for the given path
HRESULT  GetVolumeRootPath(const char *src, char *dst, unsigned int dstlen, bool validateMountPoint)
{
    HRESULT hr = S_OK;
    size_t len;
    DWORD retval = 0;
    DWORD dwError = 0;
    char tmp[MAX_PATH+1];

    do
    {
        if (src == NULL || dst == NULL)
        {
            hr = E_FAIL;
            break;
        }

        (void)memset(dst, 0, dstlen);
        //dst[dstlen-1] = '\0';

        //Expand EnvironmentStrings
        retval = ExpandEnvironmentStrings(src, tmp, MAX_PATH);

        if (retval == 0)
        {
            dwError =  GetLastError();
            hr = E_FAIL; 
            DebugPrintf("FAILED: ExpandEnvironmentStrings failed, Error = [%d], hr = 0x%x\n", dwError, hr);
            break;
        }

        // Add the backslash termination, if needed
        len = strlen(tmp);

        if (tmp[len-1] != '\\')
        {
            tmp[len] = '\\';
            tmp[len+1] = '\0';
        }
        
        //Get Volume Mount point name, i.e, Root path of the volume
        retval = GetVolumePathName(tmp, dst, dstlen);

        if (retval == 0)
        {
            dwError =  GetLastError();
            hr = E_FAIL; 
            
            DebugPrintf("FAILED: GetVolumePathName failed,Error =[%d], hr = 0x%x\ntem= %s\n dst= %s\ndstlen = %d\n", dwError,hr,tmp,dst,dstlen);
            break;
        }

        // if volume mount point validation is false,
        // do not perform volume mount point validation.
        
        // This is useful while enumerating VSS applications

        // Volumes affected by VSS applcations may be stale.i.e, volumes may not exist 
        // but VSS writers may be returning stale information.

        if (validateMountPoint == false)
        {
            break;
        }

        //Get Volume Name for VOlume Mount Point
        char volumeName[MAX_PATH+1];
        (void)memset(volumeName, '\0', sizeof(volumeName));
        DWORD dwDriveType = GetDriveType(dst);
        if(dwDriveType  !=  DRIVE_FIXED)
        {
            break;
        }
        retval = GetVolumeNameForVolumeMountPoint(dst, volumeName, MAX_PATH);

        if (retval == 0)
        {
            //TODO: BUGBUG: Check for GetLastError()
            hr = E_FAIL; 
            DebugPrintf("FAILED: GetVolumeNameForVolumeMountPoint failed for %s, Win32 Error: [%d]\n",dst, GetLastError());
            break;
        }

        //Get Unqiue Volume Name
        char volumeUniqueName[MAX_PATH+1];
        (void)memset(volumeUniqueName, '\0', sizeof(volumeUniqueName));
        retval = GetVolumeNameForVolumeMountPoint(volumeName, volumeUniqueName, MAX_PATH);

        if (retval == 0)
        {
            //TODO: BUGBUG: Check for GetLastError()
            hr = E_FAIL; 
            DebugPrintf("FAILED: GetVolumeNameForVolumeMountPoint failed for %s, Win32 Error: [%d]\n",dst, GetLastError());
            break;
        }
        
    } while(FALSE);

    return hr;	
}

bool IsValidVolume(const char *srcVol, bool validateMountPoint)
{
    HRESULT hr = S_OK;
    char RootPath[MAX_PATH+1];
    char SrcPath[MAX_PATH+1];

    size_t len;
    
    if (srcVol == NULL)
    {
        return false;
    }

    // Copy srcVol to local buffer, SrcPath
    len = strlen(srcVol);
    inm_memcpy_s(SrcPath, sizeof(SrcPath),srcVol, len+1);

    //SrcPath should always need to have trailing slash.
    //Add trailing slash, if needed
    if (SrcPath[len-1] != '\\')
    {
        SrcPath[len] = '\\';
        SrcPath[++len] = '\0';
    }

    //Get Mount point of the srcVol
    hr = GetVolumeRootPath(SrcPath, RootPath, MAX_PATH, validateMountPoint);

    if (hr != S_OK)
    {
        return false;
    }

    //Check if the device type is fixed hard disk or not
    if (validateMountPoint && DRIVE_FIXED != GetDriveType(SrcPath))
    {
        XDebugPrintf("%s is NOT a fixed hard drive\n",SrcPath);
        return false;
    }

    return((strncmp(SrcPath, RootPath, len)==0));
}

#if (_WIN32_WINNT > 0x500)
bool IsValidApplication(const char *appName)
{
    unsigned int size = sizeof(StaticVssWriter2Applications)/ sizeof(Writer2Application_t);

    for(unsigned int index = 0; index <size; index++)
    {
        if (stricmp(StaticVssWriter2Applications[index].ApplicationName.c_str(), appName) == 0)
        {
            return true;
        }
    }
    std::vector<Writer2Application_t>::iterator iterDynamicVssWriters = vDynamicVssWriter2Apps.begin();
    while(iterDynamicVssWriters != vDynamicVssWriter2Apps.end())
    {
        if (stricmp((*iterDynamicVssWriters).ApplicationName.c_str(), appName) == 0)
      {
        return true;
      }
      iterDynamicVssWriters++;
    }
    return false;
}
#endif

// This function returns VolumeIndex for the given volume
// It returns 0 for A, 1 for B, 2 for C and so on.
HRESULT GetVolumeIndexFromVolumeName(const char *volumeName, DWORD *pVolumeIndex, bool validateMountPoint)
{
    HRESULT hr = S_OK;

    do
    {
        if(IsValidVolumeEx(volumeName) == false || pVolumeIndex == NULL)
        {
            hr = E_FAIL;
            //inm_printf("\n****InvalidVolume = %s\n",pVolumeIndex);//CHECK
            break;
        }

        if (volumeName[0] >= 'A' && volumeName[0] <= 'Z')
        {
            *pVolumeIndex = (volumeName[0] - 'A');
            break;
        }

        if (volumeName[0] >= 'a' && volumeName[0] <= 'z')
        {
            *pVolumeIndex = (volumeName[0] - 'a');
            break;
        }

        hr = E_FAIL;

    } while(FALSE);

    return hr;
}


bool ValidateSnapshotVolumes(DWORD dwDriveMask)
{
    //Specify the VolumeName
    char szVolumeName[4];
    
    szVolumeName[0] = 'X';
    szVolumeName[1] = ':';
    szVolumeName[2] = '\\';
    szVolumeName[3] = '\0';

    for( int i = 0; i < MAX_DRIVES && dwDriveMask ; i++ )
    {
        DWORD dwMask = 1<<i;

        if(0 == ( dwDriveMask &  dwMask ) ) 
        {
            continue;
        }

        //Clear i-th bit
        dwDriveMask &= ~(dwMask);

        szVolumeName[0] = 'A' + i;

        if (IsValidVolumeEx(szVolumeName) == false)
        {
            return false;
        }
    }

    return true;
}

#if (_WIN32_WINNT > 0x500)
const char *MapVssWriter2Application(const char *VssWriterName)
{
  unsigned int size = 0;

  size = sizeof(StaticVssWriter2Applications)/sizeof(Writer2Application_t);

  for(unsigned int index = 0; index < size; index++)
  {
      const char *wName = StaticVssWriter2Applications[index].VssWriterName.c_str();
      const char *aName = StaticVssWriter2Applications[index].ApplicationName.c_str();

      if (stricmp(wName, VssWriterName) == 0)
      {
            return aName;
      }
  }

  std::vector<Writer2Application_t>::iterator iterDynamicVssWriters = vDynamicVssWriter2Apps.begin();
  while(iterDynamicVssWriters != vDynamicVssWriter2Apps.end())
  {
      if (stricmp((*iterDynamicVssWriters).ApplicationName.c_str(), VssWriterName) == 0)
    {
      return VssWriterName;
    }
    iterDynamicVssWriters++;
  }

  return ((const char *)NULL);
}
#endif

#if (_WIN32_WINNT > 0x500)
const char *MapApplication2VssWriter(const char *AppName)
{
  unsigned int size = 0;
  std::string strOracleVssWriter = AppName;
  if((strOracleVssWriter.find("Oracle") != strOracleVssWriter.npos) ||
    (strOracleVssWriter.find("oracle") != strOracleVssWriter.npos) )
  {
    return "Oracle VSS Writer";//AppName;
  }

  size = sizeof(StaticVssWriter2Applications)/sizeof(Writer2Application_t);

  for(unsigned int index = 0; index < size; index++)
  {
      const char *wName = StaticVssWriter2Applications[index].VssWriterName.c_str();
      const char *aName = StaticVssWriter2Applications[index].ApplicationName.c_str();

      if (stricmp(aName, AppName) == 0)
      {
            return wName;
      }
  }
  std::vector<Writer2Application_t>::iterator iterDynamicVssWriters = vDynamicVssWriter2Apps.begin();
  while(iterDynamicVssWriters != vDynamicVssWriter2Apps.end())
  {
      if (stricmp((*iterDynamicVssWriters).ApplicationName.c_str(), AppName) == 0)
    {
          return (*iterDynamicVssWriters).ApplicationName.c_str();
    }
    iterDynamicVssWriters++;
  }

  return ((const char *)NULL);
}
#endif


#if (_WIN32_WINNT >= 0x502)
const char *MapWriterName2VssWriter(const char *AppName)
{
  unsigned int size = 0;
  std::string strOracleVssWriter = AppName;
   if((strOracleVssWriter.find("Oracle") != strOracleVssWriter.npos) ||
    (strOracleVssWriter.find("oracle") != strOracleVssWriter.npos) )
  {
    return "Oracle VSS Writer";//AppName;
  }

  size = sizeof(StaticVssWriter2Applications)/sizeof(Writer2Application_t);

  for(unsigned int index = 0; index < size; index++)
  {
      const char *wName = StaticVssWriter2Applications[index].VssWriterInstanceName.c_str();
      const char *aName = StaticVssWriter2Applications[index].ApplicationName.c_str();

      if (stricmp(aName, AppName) == 0)
      {
            return wName;
      }
  }
  std::vector<Writer2Application_t>::iterator iterDynamicVssWriters = vDynamicVssWriter2Apps.begin();
  while(iterDynamicVssWriters != vDynamicVssWriter2Apps.end())
  {
      if (stricmp((*iterDynamicVssWriters).ApplicationName.c_str(), AppName) == 0)
    {
          return (*iterDynamicVssWriters).ApplicationName.c_str();
    }
    iterDynamicVssWriters++;
  }

  return ((const char *)NULL);
}
#endif


#if (_WIN32_WINNT > 0x500)
void PrintVssWritersInfo(vector<VssAppInfo_t> &AppInfo)
{	
    inm_printf("\n\n");
    for(unsigned int iApp = 0; iApp < AppInfo.size(); iApp++)
    {
        const char *appName = MapVssWriter2Application(AppInfo[iApp].AppName.c_str());
        DWORD volumeBitMask = AppInfo[iApp].dwDriveMask;
        unsigned int totalVolumes = AppInfo[iApp].totalVolumes;
#if(_WIN32_WINNT >= 0x502)
        const char *writerInstanceName = AppInfo[iApp].szWriterInstanceName.c_str();
#endif

        inm_printf("\n\n\n%d:)",iApp+1);

        if (appName == NULL)
        {
            inm_printf("\tApplication Name = %s(%s)\n", "UNKNOWN", AppInfo[iApp].AppName.c_str());
        }
        else
        {
            inm_printf("\tApplication Name = %s\n", appName);
        }
    #if(_WIN32_WINNT >= 0x502)
        if (writerInstanceName != NULL)
        {
            inm_printf("\tWriter Instance  Name = %s\n", writerInstanceName);
        }
    #endif
        //inm_printf("\tTotal Volumes    = %d\n", totalVolumes);
        inm_printf("\tAffected Volumes = ");
        
        for (unsigned int iVol = 0; iVol < AppInfo[iApp].affectedVolumes.size(); iVol++)
        {
            inm_printf("%s ", AppInfo[iApp].affectedVolumes[iVol].c_str());
        }

        if (AppInfo[iApp].affectedVolumes.size() == 0)
        {
            inm_printf("None ");
        }
        //inm_printf("\n");
        
        //Print Affect Volume mount points only when Mountpoint volume is available
        if(AppInfo[iApp].volMountPoint_info.dwCountVolMountPoint != 0)
        {
            inm_printf("\tAffected Volume Mount Points = ");
        }
        for(unsigned int iMountPoint = 0; iMountPoint < AppInfo[iApp].volMountPoint_info.dwCountVolMountPoint; iMountPoint++)
        {
            string strVolMPName  =  AppInfo[iApp].volMountPoint_info.vVolumeMountPoints[iMountPoint].strVolumeMountPoint;
            string strVolName = AppInfo[iApp].volMountPoint_info.vVolumeMountPoints[iMountPoint].strVolumeName;
            //inm_printf("%s - %s \n", strVolName.c_str(),strVolMPName.c_str());
            inm_printf("%s ", strVolName.c_str());
        }
        inm_printf("\n");

        if (AppInfo[iApp].ComponentsInfo.size())
        {
            //inm_printf("\tTotal Components    = %d\n", AppInfo[iApp].ComponentsInfo.size());
            for (unsigned int iComp = 0, cIndex = 1; iComp < AppInfo[iApp].ComponentsInfo.size(); iComp++)
            {
                if (AppInfo[iApp].ComponentsInfo[iComp].isSelectable == false &&
                    AppInfo[iApp].ComponentsInfo[iComp].isTopLevel == false)
                    continue;

                if (!AppInfo[iApp].ComponentsInfo[iComp].captionName.empty() &&
                    (stricmp(AppInfo[iApp].ComponentsInfo[iComp].componentName.c_str(), AppInfo[iApp].ComponentsInfo[iComp].captionName.c_str()) != 0))
                    inm_printf("\tComponent[%d]: %s (%s) \n", cIndex, AppInfo[iApp].ComponentsInfo[iComp].componentName.c_str(), AppInfo[iApp].ComponentsInfo[iComp].captionName.c_str());
                else
                    inm_printf("\tComponent[%d]: %s \n", cIndex, AppInfo[iApp].ComponentsInfo[iComp].componentName.c_str());

                cIndex++;

            }
        }
    }
}
#endif

#if (_WIN32_WINNT > 0x500)

void PrintFormattedVssWritersInfo(vector<VssAppInfo_t> &AppInfo, std::vector<AppParams_t> &CmdLineAppList)
{	
    inm_printf("\n\n");

    for(unsigned int iCmdLineApp = 0; iCmdLineApp < CmdLineAppList.size(); iCmdLineApp++)
    {
        std::string cmdAppName = CmdLineAppList[iCmdLineApp].m_applicationName;
        
        string volumeBuffer = "";		

        unsigned int iApp = 0;
        bool bFound = false;

        for(; iApp < AppInfo.size(); iApp++)
        {
            const char *appName = MapVssWriter2Application(AppInfo[iApp].AppName.c_str());
            
            unsigned int totalVolumes = AppInfo[iApp].totalVolumes;

            // Skip new writers
            if (appName == NULL)
                continue;

            //Skip non-matching application
            if (!boost::iequals(cmdAppName, appName))
                continue;

            bFound = true;
            break;
        }

        if (bFound)
        {
            for (unsigned int iVol = 0; iVol < AppInfo[iApp].affectedVolumes.size(); iVol++)
            {	
                string volName = AppInfo[iApp].affectedVolumes[iVol];
                volumeBuffer.append(volName);
                volumeBuffer.append(" ");
            }

            for (unsigned int iVol = 0; iVol < AppInfo[iApp].volMountPoint_info.dwCountVolMountPoint; iVol++)
            {	
                string volName = AppInfo[iApp].volMountPoint_info.vVolumeMountPoints[iVol].strVolumeName;
                volumeBuffer.append(volName);
                volumeBuffer.append(" ");
            }
        }

        inm_printf("Application=%s;AffectedVolumes=%s\n", cmdAppName.c_str(), volumeBuffer.c_str());
    }
    
}
#endif

BOOL GetVolumesList(vector<const char *> &LogicalVolumes)
{
    /* 
        The below code is borrowed from RegisterHost function defined in Hostagenthelpers.cpp
    */

    DWORD dwDrives = GetLogicalDrives();
    
    if (dwDrives == 0)
    {
        DebugPrintf("FAILED GetLogicalDrivers() %s:%d \n",__FILE__,__LINE__);
        HRESULT hresult = VACP_GET_LOGICAL_DRIVES_FAILED;
        ghrErrorCode = VACP_GET_LOGICAL_DRIVES_FAILED;
        std::string errmsg = "GetLogicalDrivers failed.";
        ADD_TO_VACP_FAIL_MSGS(VACP_GET_LOGICAL_DRIVES_FAILED, errmsg, hresult);
        return FALSE;
    }

    for( int i = 0; i < MAX_DRIVES; i++ )
    {
        DWORD dwDriveMask = 1<<i;
        
        if( 0 == ( dwDrives &  dwDriveMask ) )
        {
            continue;
        }

        char *szVolumeName = new (nothrow) char[4];

        if (szVolumeName == NULL)
        {
            DebugPrintf("%s: FAILED to allocate memory for volume name.\n", __FUNCTION__);

            HRESULT hresult = VACP_MEMORY_ALLOC_FAILURE;
            ghrErrorCode = VACP_MEMORY_ALLOC_FAILURE;
            std::string errmsg = "failed to allocate memory for boot volume.";
            ADD_TO_VACP_FAIL_MSGS(VACP_MEMORY_ALLOC_FAILURE, errmsg, hresult);
            return FALSE;
        }

        szVolumeName[ 0 ] = 'A' + i;
        szVolumeName[ 1 ] = ':';
        szVolumeName[ 2 ] = '\\';
        szVolumeName[ 3 ] = '\0';
        
        if (IsValidVolumeEx(szVolumeName) == true)
        {
            XDebugPrintf("Adding Volume %s \n",szVolumeName);
            LogicalVolumes.push_back(szVolumeName);
            continue;
        }
        else
        {
            XDebugPrintf("Skipping Volume %s \n",szVolumeName);
            delete[] szVolumeName;
        }
    }

    return TRUE;
}

#if (_WIN32_WINNT >= 0x0501)
BOOL ProcessVolMntPntsForGivenVol (HANDLE hVol, char *volGuidBuf, int iBufSize,vector<const char *> &LogicalVolumes)
{
   BOOL bFlag								= FALSE; // result flag holding ProcessVolumeMountPoint
   BOOL bFixedDrive							= TRUE; //Flag to find the status of volume
   HANDLE hMountPoint						= NULL; // handle for Mount Point
   char volMountPointBuf[INMBUFSIZE]			= {0}; // A buffer to hold Volume Mount Point
   char volBuf[INMBUFSIZE]						= {0}; // A buffer to hold Volume
   char MtPtGuid[INMBUFSIZE]					= {0};  // target of mount at mount point
   char volGuidMtPtBufPath[INMBUFSIZE]			= {0};   // construct a complete path here
   char FileSysNameBuf[INMFILESYSNAMEBUFSIZE]  = {0};
   DWORD dwSysFlags							= 0;   // flags that describe the file system
   ULONG returnedLength						= 0;
   
   if ( DRIVE_FIXED != GetDriveType(volGuidBuf))	
   {
       bFixedDrive = FALSE;
   }
   if(TRUE == bFixedDrive)
   {
        // Is this volume an NTFS file system?
        GetVolumeInformation( volGuidBuf,
                              NULL, 
                              0, 
                              NULL, 
                              NULL,
                              &dwSysFlags, 
                              FileSysNameBuf,
                              INMFILESYSNAMEBUFSIZE);

        // If Reparse Points are supported then Volume Mounts Points will be supported
        if (! (dwSysFlags & FILE_SUPPORTS_REPARSE_POINTS))
        {	
        }
        else
        {
            // Start processing mount points on this volume.
            hMountPoint = FindFirstVolumeMountPoint(volGuidBuf, volMountPointBuf,INMBUFSIZE);

            if (hMountPoint == INVALID_HANDLE_VALUE)
            {	
            }	
            else
            {
                static UINT nMtPtIndex = 0;
                do
                {
                    memset((void*)volGuidMtPtBufPath,0x00,INMBUFSIZE);
                    memset((void*)MtPtGuid,0x00,INMBUFSIZE);
                    memset((void*)volBuf,0x00,INMBUFSIZE);
                    returnedLength = 0;
                    inm_strncpy_s(volBuf,ARRAYSIZE(volBuf),volGuidBuf,INMBUFSIZE);
                    inm_strcat_s(volBuf,ARRAYSIZE(volBuf), volMountPointBuf);
                    bFlag = GetVolumeNameForVolumeMountPoint( volBuf,MtPtGuid,INMBUFSIZE);
                    if (!bFlag)
                    {	
                    }
                    else
                    {						
                        if(GetVolumePathNamesForVolumeName(MtPtGuid,volGuidMtPtBufPath,INMBUFSIZE,&returnedLength))
                        {														
                            string sMountPoint = volGuidMtPtBufPath;
                            if(sMountPoint.length() > 0)
                            {
                              g_vMountPoints.push_back(sMountPoint);
                              nMtPtIndex += 1;
                            }
                        }
                    }
                    // get the next mount point 
                    memset((void*)volMountPointBuf,0x00,INMBUFSIZE);
                    bFlag = FindNextVolumeMountPoint( hMountPoint,volMountPointBuf,INMBUFSIZE);//check why only of BUFSIZE

                }while(bFlag);

                FindVolumeMountPointClose(hMountPoint);
            }
        }
   }
   return (bFlag);
}

BOOL GetMountPointsList(vector<const char *> &LogicalVolumes)
{
    boost::mutex::scoped_lock guard(g_diskInfoMutex);

   inm_printf("\n Getting Mount Points List...\n");
   char volGuid[INMBUFSIZE]	= {0};  // buffer to hold GUID of a volume.
   HANDLE hVol				= NULL; // handle of a Volume to be scanned.
   BOOL bFlag				= FALSE;// result of Processing each Volume.

   g_vMountPoints.clear();

   // Scan all the volumes starting with the first volume.
   hVol = FindFirstVolume (volGuid, INMBUFSIZE );

   if (hVol == INVALID_HANDLE_VALUE)
   {
      inm_printf ("\n\tNo volumes available to get the list of Mount Points!\n");
      return FALSE;
   }
   do
   {
       ProcessVolMntPntsForGivenVol(hVol,volGuid,INMBUFSIZE,LogicalVolumes);
   }while(FindNextVolume(hVol,volGuid,INMBUFSIZE));
   
   bFlag = FindVolumeClose(hVol);

   //Now insert all the collected Mount Points into the LogicalVlumes
   for(size_t i = 0 ; i < g_vMountPoints.size(); i++)
   {
       size_t mountPointSize = 0;
       INM_SAFE_ARITHMETIC(mountPointSize = InmSafeInt<unsigned long>::Type(g_vMountPoints[i].size()) + 1, INMAGE_EX(g_vMountPoints[i].size()))
       char *szMountPoint = new (std::nothrow) char[mountPointSize];

       if (szMountPoint == NULL)
       {
           DebugPrintf("%s: FAILED to allocate %d bytes memory for mount point.\n",
               __FUNCTION__,
               mountPointSize);

           HRESULT hresult = VACP_MEMORY_ALLOC_FAILURE;
           ghrErrorCode = VACP_MEMORY_ALLOC_FAILURE;
           std::string errmsg = "failed to allocate memory for mount point.";
           ADD_TO_VACP_FAIL_MSGS(VACP_MEMORY_ALLOC_FAILURE, errmsg, hresult);
           return FALSE;
       }
       inm_strcpy_s(szMountPoint, mountPointSize, g_vMountPoints[i].c_str());
       LogicalVolumes.push_back(szMountPoint);
   }

   //Display the Mount Points that are collected.
   for (auto it = g_vMountPoints.begin(); it != g_vMountPoints.end(); it++)
   {
       inm_printf("\t Volume Mount Point Path is : %s\n",it->c_str());
   }
   return (bFlag);
}
#endif

void GetApplicationsList(std::vector<std::string> &Applications)
{
    std::vector<Writer2Application_t>::iterator iterDynamicVssWriters = vDynamicVssWriter2Apps.begin();
    while(iterDynamicVssWriters != vDynamicVssWriter2Apps.end())
    {
      Applications.push_back((*iterDynamicVssWriters).ApplicationName);
      iterDynamicVssWriters++;
    }
}

#if (_WIN32_WINNT > 0x500)
DWORD MapApplication2VolumeBitMask(const char *AppName, vector<VssAppInfo_t> &AppInfo)
{
    const char *wName = MapApplication2VssWriter(AppName);

    if (wName == NULL)
        return 0;

    return MapVssWriter2VolumeBitMask(wName, AppInfo);
}
#endif

#if (_WIN32_WINNT > 0x500)
void MapApplication2VolumeList(const char *AppName, vector<VssAppInfo_t> &AppInfo, 
                               vector<std::string> &Vols)
{
    const char *wName = MapApplication2VssWriter(AppName);

    if (wName == NULL)
        return ;

    return MapVssWriter2VolumeList(wName, AppInfo,Vols);
}
#endif

#if (_WIN32_WINNT > 0x500)
void MapVssWriter2VolumeList(const char *VssWriterName, vector<VssAppInfo_t> &AppInfo,
                                 vector<std::string> &Vols)
{
    for(unsigned int iApp = 0; iApp < AppInfo.size(); iApp++)
    {
        VssAppInfo_t vssApp = AppInfo[iApp];

        if (strnicmp(VssWriterName, vssApp.AppName.c_str(), strlen(VssWriterName)) == 0)
        {
            Vols.insert(Vols.end(),vssApp.affectedVolumes.begin(), vssApp.affectedVolumes.end());
        }
    }
    return ;
}
#endif

#if (_WIN32_WINNT > 0x500)
DWORD MapVssWriter2VolumeBitMask(const char *VssWriterName, vector<VssAppInfo_t> &AppInfo)
{
    DWORD dwDriveMask = 0;
    for(unsigned int iApp = 0; iApp < AppInfo.size(); iApp++)
    {
        VssAppInfo_t vssApp = AppInfo[iApp];

        if (stricmp(VssWriterName, vssApp.AppName.c_str()) == 0)
        {
            dwDriveMask |= vssApp.dwDriveMask;
            /*if(stricmp(VssWriterName,"Microsoft Exchange Writer") == 0)
            {
                dwDriveMask |= vssApp.dwDriveMask;
                continue;
            }
            else
            {
                dwDriveMask = vssApp.dwDriveMask;
                break;
            }*/
        }
    }
    
    return dwDriveMask;
}
#endif


#if (_WIN32_WINNT >= 0x502)
DWORD MapApplication2VolumeBitMaskEx(const char *AppName, const char *WriterInstanceName, vector<VssAppInfo_t> &AppInfo,bool bWriterInstanceName)
{
    const char *wName;
    if(bWriterInstanceName)
    {
        wName = MapWriterName2VssWriter(AppName);
    }
    else
    {
        wName = MapApplication2VssWriter(AppName);
        
    }

    if (wName == NULL)
        return 0;

    return MapVssWriter2VolumeBitMaskEx(wName, wName, AppInfo,bWriterInstanceName);
}

DWORD MapVssWriter2VolumeBitMaskEx(const char *VssWriterName, const char *VssWriterInstanceName, vector<VssAppInfo_t> &AppInfo,bool bWriterInstanceName)
{
    DWORD dwDriveMask = 0;
    for(unsigned int iApp = 0; iApp < AppInfo.size(); iApp++)
    {
        VssAppInfo_t vssApp = AppInfo[iApp];
        

        if(bWriterInstanceName)
        {
            if(VssWriterInstanceName && !vssApp.szWriterInstanceName.empty())
            {
                if(!stricmp(VssWriterInstanceName, vssApp.szWriterInstanceName.c_str()))
                {
                    dwDriveMask = vssApp.dwDriveMask;
                    break;
                }
            }
        }
        else
        {
          std::string strVssWriter = VssWriterName;
          if((strVssWriter.find("Oracle") != strVssWriter.npos) ||
              (strVssWriter.find("oracle") != strVssWriter.npos))
          {
            strVssWriter = vssApp.AppName;
            if((strVssWriter.find("Oracle") != strVssWriter.npos) ||
              (strVssWriter.find("oracle") != strVssWriter.npos))
            {
              dwDriveMask |= vssApp.dwDriveMask;
            }
          }
          else
          {
              if (stricmp(VssWriterName, vssApp.AppName.c_str()) == 0)
            {
                dwDriveMask |= vssApp.dwDriveMask;
            }
          }
        }
    }
    
    return dwDriveMask;
}

void MapApplication2VolumeListEx(const char *AppName,
    const char *VssWriterInstanceName,
    std::vector<VssAppInfo_t> &AppInfo, 
    std::vector<std::string> &Vols,
    bool bWriterInstanceName)
{

    const char *wName;
    std::string strOracleVssWriter = AppName;
     if((strOracleVssWriter.find("Oracle") != strOracleVssWriter.npos) ||
    (strOracleVssWriter.find("oracle") != strOracleVssWriter.npos) )
    {
      wName = "Oracle VSS Writer";//AppName;
    }
    else
    {
      if(bWriterInstanceName)
      {
          wName = MapWriterName2VssWriter(AppName);
      }
      else
      {
          wName= MapApplication2VssWriter(AppName);
      }

      if (wName == NULL)
          return ;
    }

    return MapVssWriter2VolumeListEx(wName,wName, AppInfo,Vols,bWriterInstanceName);
}

void MapVssWriter2VolumeListEx(const char *VssWriterName,const char *VssWriterInstanceName, vector<VssAppInfo_t> &AppInfo,
                                 vector<std::string> &Vols, bool bWriterInstanceName)
{
    for(unsigned int iApp = 0; iApp < AppInfo.size(); iApp++)
    {
        VssAppInfo_t vssApp = AppInfo[iApp];
    
        if(bWriterInstanceName)
        {
            if(VssWriterInstanceName && !vssApp.szWriterInstanceName.empty())
            {
                if(!stricmp(VssWriterInstanceName, vssApp.szWriterInstanceName.c_str()))
                {
                    Vols.insert(Vols.end(),vssApp.affectedVolumes.begin(), vssApp.affectedVolumes.end());
                    break;
                }
            }
        }
        else
        {
          std::string strVssOracleWriter = vssApp.AppName;
          if(strVssOracleWriter.find("Oracle VSS Writer") != strVssOracleWriter.npos)
          {
            Vols.insert(Vols.end(),vssApp.affectedVolumes.begin(), vssApp.affectedVolumes.end());
          }
          else
          {
            if (strnicmp(VssWriterName, vssApp.AppName.c_str(),strlen(VssWriterName)) == 0)
            {
                Vols.insert(Vols.end(),vssApp.affectedVolumes.begin(), vssApp.affectedVolumes.end());
            }
          }
        }
    }
    return ;
}

#endif


HRESULT MapApplication2StreamRecType(const char *AppName, USHORT &usStreamRecType)
{
    
    unsigned int size = 0;
    std::string strOracleVssWriter = AppName;
     if((strOracleVssWriter.find("Oracle") != strOracleVssWriter.npos) ||
    (strOracleVssWriter.find("oracle") != strOracleVssWriter.npos) )
    {
      usStreamRecType = STREAM_REC_TYPE_ORACLE_TAG;
      return S_OK;
    }
    size = sizeof(StaticVssWriter2Applications)/sizeof(Writer2Application_t);

    for(unsigned int index = 0; index < size; index++)
    {
        const char *aName = StaticVssWriter2Applications[index].ApplicationName.c_str();
        USHORT StrRecType = StaticVssWriter2Applications[index].usStreamRecType;
        if (stricmp(AppName, aName) == 0)
        {
          usStreamRecType = StrRecType;
          return S_OK;
        }
    }
    std::vector<Writer2Application_t>::iterator iterDynamicVssWriters = vDynamicVssWriter2Apps.begin();
    while(iterDynamicVssWriters != vDynamicVssWriter2Apps.end())
    {
        if (stricmp((*iterDynamicVssWriters).ApplicationName.c_str(), AppName) == 0)
      {
        usStreamRecType = /*STREAM_REC_TYPE_SYSTEMLEVEL*/STREAM_REC_TYPE_DYNMICALLY_DISCOVERED_NEW_VSSWRITER;
        return S_OK;
      }
      iterDynamicVssWriters++;
    }
    
    return E_FAIL;
}

HRESULT MapStreamRecType2Application(const char **tagString, USHORT usStreamRecType)
{
    
    unsigned int size = 0;

    *tagString = "NULL";
    size = sizeof(StaticVssWriter2Applications)/sizeof(Writer2Application_t);

    for(unsigned int index = 0; index < size; index++)
    {
        const char *aName = StaticVssWriter2Applications[index].ApplicationName.c_str();
        USHORT StrRecType = StaticVssWriter2Applications[index].usStreamRecType;

        if (usStreamRecType == StrRecType)
        {
            *tagString = aName;
            return S_OK;
        }
    }
    //MCHECK:Add DynamicVssWriter2Apps List
    return E_FAIL;
}

HRESULT ValidateTagData(TagData_t *pTagData, bool bPrintTagDataAsString)
{
    HRESULT hr = S_OK;

    if (pTagData == NULL || pTagData->data == NULL || pTagData->len == 0)
        return E_FAIL;

    StreamEngine *en = new StreamEngine();

    hr = en->ValidateStreamRecord(pTagData->data, (ULONG)pTagData->len);

    if (hr != S_OK)
    {
        delete en;
        return E_FAIL;
    }
    
    hr = en->SetStreamRole(eStreamRoleParser);

    if (hr != S_OK)
    {
        delete en;
        return E_FAIL;
    }

    hr = en->RegisterDataSourceBuffer(pTagData->data, (ULONG)pTagData->len);

    if (hr != S_OK)
    {
        delete en;
        return E_FAIL;
    }

    USHORT Tag = 0;
    ULONG DataLength = 0;

    hr = en->GetRecordHeader(&Tag, &DataLength);

    if (hr != S_OK)
    {
        delete en;
        return E_FAIL;
    }

    if (bPrintTagDataAsString)
    {
        const char *tagString = NULL;
    
        hr = MapStreamRecType2Application(&tagString, Tag);
        if (hr != S_OK)
        {
            //DebugPrintf("\nInvalid stream record type found.");
            inm_printf("\nInvalid stream record type found.");
            delete en;
            return E_FAIL;
        }
    
        // XDebugPrintf("\nValidating Tag = 0x%04x(%s), DataLength = 0x%x(%d) ...\n", (ULONG)Tag, tagString, DataLength, DataLength);
    }
    else
    {
        // XDebugPrintf("\nValidating Tag = 0x%04x, DataLength = 0x%x(%d) ...\n", (ULONG)Tag, DataLength, DataLength);
    }

    if (DataLength > 0)
    {
        char *Data = new char[DataLength];

        hr = en->GetRecordData(Data, DataLength, DataLength);
        if (hr != S_OK)
        {
            delete en;
            return E_FAIL;
        }

        delete Data;
    }

    delete en;

    return S_OK;
}

std::string getUniqueSequence()
{
    std::stringstream ssUSeq;
    ssUSeq << std::hex << GetTimeInSecSinceEpoch1970();
    DebugPrintf("TimeStamp - UniqueSequence = %s\n", ssUSeq.str().c_str());
    return ssUSeq.str();
}


// check if volume locked by us. 
// as a convenience if the fsType buffer is supplied also return what type of file system it is
bool IsVolumeLocked(const char *pszVolume)
{
    bool ret = false;
    XDebugPrintf(_T("\nEntered IsVolumeLocked\n"));
    if ( IsThisNtfsHiddenVolume(pszVolume) == S_OK ) {

        ret = true;
        INMXXDONE();
    }
    if ( IsThisFATfsHiddenVolume(pszVolume) == S_OK )
    {
      ret = true;
      INMXXDONE();
    }
INMXXEXITD()
    XDebugPrintf("\nExit IsVolumeLocked with retrun value =%d\n", ret);
    return ret;
}

HRESULT IsThisNtfsHiddenVolume(/* in */ const char *szVolume)
{
    HRESULT hr = S_OK;
    char *pchlInmDeltaBuffer = NULL;
    DWORD cblInmRead = 0;
    DWORD cblInmReturned = 0;
    DISK_GEOMETRY lInmdisk_geometry = {0};
    DWORD dwlInmSectorSize = 0;
    ULONG sv_ulongLengthWritten = 0;
    LONG lInmlowpart = 0;
    LONG lInmhighpart = 0;
    LONG lInmlowpartPrev = 0;
    LONG lInmhighpartPrev = 0;
    HRESULT hRC = E_FAIL;
    BOOL bRevertFilePtr = FALSE;
    HANDLE lInmhandleVolume = INVALID_HANDLE_VALUE;
    DWORD dwlInmPtr = NULL;
    DWORD dwInmError ;
    XDebugPrintf("Entered IsThisNtfsHiddenVolume\n");

    do {
        
        XDebugPrintf("Opening volume: %s\n", (char*)szVolume);
        hr = OpenVolume( &lInmhandleVolume, (char*)szVolume );
        if( FAILED( hr ) || (INVALID_HANDLE_VALUE == lInmhandleVolume))
        {
            XDebugPrintf("OpenVoume() Failed. Error= %d\n", GetLastError());
            break;
        }
        XDebugPrintf("Successfully open the volume: %s\n", (char*)szVolume);

        if( 0 == DeviceIoControl( lInmhandleVolume,IOCTL_DISK_GET_DRIVE_GEOMETRY,NULL,0,
            &lInmdisk_geometry,sizeof( DISK_GEOMETRY ),&cblInmReturned,NULL ) )
        {
            hr = HRESULT_FROM_WIN32( GetLastError() );
            XDebugPrintf("DeviceIoctl-IOCTL_DISK_GET_DRIVE_GEOMETRY failed Error:%d\n", GetLastError());
            break;
        }

        dwlInmSectorSize = lInmdisk_geometry.BytesPerSector;
        pchlInmDeltaBuffer = new char[ dwlInmSectorSize ];
        if( NULL == pchlInmDeltaBuffer )
        {
            XDebugPrintf("Failed to allocate memory. Error: %d\n", GetLastError());
            hr = E_OUTOFMEMORY;
            break;
        }

        // Note the previous file pointer offset so we can restore it before exit
        dwlInmPtr= SetFilePointer( lInmhandleVolume, lInmlowpartPrev, &( lInmhighpartPrev ), FILE_CURRENT );
        if( INVALID_SET_FILE_POINTER == dwlInmPtr)
        {
             dwInmError = GetLastError();
             if(dwInmError !=NO_ERROR)
             {
                XDebugPrintf("SetFilePointer(FILE_CURRENT) Failed. Error:%d\n", GetLastError());
                hr = HRESULT_FROM_WIN32(GetLastError());
                break;
            }
        }

        // Revert to the previous file seek offset, even in cases of error
        bRevertFilePtr = TRUE;

        dwlInmPtr= SetFilePointer( lInmhandleVolume, lInmlowpart, &( lInmhighpart ), FILE_BEGIN );
        if( INVALID_SET_FILE_POINTER == dwlInmPtr)
        {
             dwInmError = GetLastError();
             if(dwInmError !=NO_ERROR)
             {
                XDebugPrintf("SetFilePointer(FILE_CURRENT) Failed. Error:%d\n", GetLastError());
                hr = HRESULT_FROM_WIN32(GetLastError());
                break;
            }
        }

        if( !ReadFile( lInmhandleVolume, pchlInmDeltaBuffer, dwlInmSectorSize, &cblInmRead, NULL ) )
        {
            XDebugPrintf("ReadFile Failed. Error:%d\n", GetLastError());         
            hr = HRESULT_FROM_WIN32( GetLastError() );
             break;
        }

        // Check if this is of input FS type
        XDebugPrintf("Checking filesystem type...\n");
        if (0 == memcmp(&pchlInmDeltaBuffer[SV_NTFS_MAGIC_OFFSET], SV_MAGIC_SVFS, SV_FSMAGIC_LEN) )
        {
            XDebugPrintf("Found SV type.\n");
            hRC = S_OK;
        }
        else
        {
            char szRootPathName[MAX_PATH+2] = {0};
            char szVolumeNameBuffer[MAX_PATH+1] = { 0 };
            DWORD nVolumeNameSize = MAX_PATH;;
            DWORD dwVolumeSerialNumber;
            DWORD dwLength;
            DWORD dwFileSystemFlag;
            char szFileSystemNameBuffer[MAX_PATH+1] = {0};
            DWORD dwFSNameSize = MAX_PATH;

            // need to check whether given volume name has "\" as last charcter or not
            //if not there we need to add this
            //This is required for GetVolumeInformation() API as it expect dive or mountpoints must have trailing backslash.

            inm_strcpy_s(szRootPathName,ARRAYSIZE(szRootPathName),szVolume);
            if(szRootPathName[_tcslen(szRootPathName) -1] != _T('\\'))
            {
                inm_strcat_s(szRootPathName,ARRAYSIZE(szRootPathName),"\\");
            }

            if(!GetVolumeInformation((char*)szRootPathName,szVolumeNameBuffer,nVolumeNameSize,&dwVolumeSerialNumber,
                &dwLength,&dwFileSystemFlag,szFileSystemNameBuffer,dwFSNameSize))
            {
                XDebugPrintf("GetVolumeinformation API failed. Error :%d", GetLastError());
            }
            else
            {
                XDebugPrintf("FileSystem type is %s\n", szFileSystemNameBuffer);
            }
        }
    
    } while (FALSE);

  

    // Close any handles
    if(lInmhandleVolume !=INVALID_HANDLE_VALUE)
    {
        HRESULT hrCloseVolume = CloseVolume(lInmhandleVolume);
    }

    // Cleanup allocated memory
    if(pchlInmDeltaBuffer)
    {
        delete[] pchlInmDeltaBuffer;
    }

    XDebugPrintf("Exit IsThisNtfsHiddenVolume hRC = %x\n",hRC );
    if(HRESULT_FROM_WIN32(21)  == hr)//if the VSS_E_OBJECT_NOT_FOUND .This error code also means that the device is busy in the case of cluster passive disks and so we should skip them too.
    {
      inm_printf("\nThe device is busy. Mostly, it could be a passive/offline/inactive cluster disk on this node!\n");
      hRC = S_OK;
    }
    return hRC;
}

///
/// Returns: True if the volume is FATFS Hidden volume
///          False otherwise
///
HRESULT IsThisFATfsHiddenVolume(/* in */ const char *szVolume)
{
    HRESULT hr = S_OK;
    char *pchInmDeltaBuffer = NULL;
    DWORD cbInmRead = 0;
    DWORD cbInmReturned = 0;
    DISK_GEOMETRY lInmdisk_geometry = {0};
    DWORD dwInmSectorSize = 0;
    ULONG sv_ulongLengthWritten = 0;
    LONG lInmlowpart = 0;
    LONG lInmhighpart = 0;
    LONG lInmlowpartPrev = 0;
    LONG lInmhighpartPrev = 0;
    HRESULT hRC = E_FAIL;
    BOOL bRevertFilePtr = FALSE;
    HANDLE lInmhandleVolume = INVALID_HANDLE_VALUE;
    VacpBpb *lInmbpb;
    DWORD dwPtr = NULL;
    DWORD dwError;

    XDebugPrintf("Entered IsThisFATfsHiddenVolume\n");
    do {

        XDebugPrintf("Opening volume: %s\n", (char*)szVolume);
        
        hr = OpenVolume( &lInmhandleVolume, (char*)szVolume );
        if( FAILED( hr ) || INVALID_HANDLE_VALUE == lInmhandleVolume )
        {
            XDebugPrintf("OpenVoume() Failed. Error:%d\n", GetLastError());
            break;
        }
        XDebugPrintf("Successfully open the volume: %s\n", (char*)szVolume);

        if( 0 == DeviceIoControl( lInmhandleVolume,
            IOCTL_DISK_GET_DRIVE_GEOMETRY,
            NULL,
            0,
            &lInmdisk_geometry,
            sizeof( DISK_GEOMETRY ),
            &cbInmReturned,
            NULL ) )
        {
            hr = HRESULT_FROM_WIN32( GetLastError() );
            XDebugPrintf("DeviceIoctl-IOCTL_DISK_GET_DRIVE_GEOMETRY failed Error:%d\n", GetLastError());
            break;
        }

        dwInmSectorSize = lInmdisk_geometry.BytesPerSector;
        pchInmDeltaBuffer = new char[ dwInmSectorSize ];
        if( NULL == pchInmDeltaBuffer )
        {
            XDebugPrintf("Failed to allocate memory. Error: %d\n", GetLastError());
            hr = E_OUTOFMEMORY;
            break;
        }

        // Note the previous file pointer offset so we can restore it before exit
        dwPtr= SetFilePointer( lInmhandleVolume, lInmlowpartPrev, &( lInmhighpartPrev ), FILE_CURRENT );
        if( INVALID_SET_FILE_POINTER == dwPtr)
        {
             dwError = GetLastError();
             if(dwError !=NO_ERROR)
             {
                XDebugPrintf("SetFilePointer(FILE_CURRENT) Failed. Error:%d\n", GetLastError());
                hr = HRESULT_FROM_WIN32(GetLastError());
                break;
            }
        }
        // Revert to the previous file seek offset, even in cases of error
        bRevertFilePtr = TRUE;

        dwPtr= SetFilePointer( lInmhandleVolume, lInmlowpart, &( lInmhighpart ), FILE_BEGIN );
        if( INVALID_SET_FILE_POINTER == dwPtr)
        {
             dwError = GetLastError();
             if(dwError !=NO_ERROR)
             {
                XDebugPrintf("SetFilePointer(FILE_CURRENT) Failed. Error:%d\n", GetLastError());
                hr = HRESULT_FROM_WIN32(GetLastError());
                break;
            }
        }
        
        //if( FALSE == ReadFile( lInmhandleVolume, pchInmDeltaBuffer, dwInmSectorSize, &cbInmRead, NULL ) )
        if(!ReadFile( lInmhandleVolume, pchInmDeltaBuffer, dwInmSectorSize, &cbInmRead, NULL ))
        {
            XDebugPrintf("ReadFile Failed. Error:%d\n", GetLastError());         
            hr = HRESULT_FROM_WIN32( GetLastError() );
            break;
        }

        // Check if this is FAT
        // The heuristic used is as follows
        // if inm BPB BS_jmpBoot[0] is either 0xEB + 1 or 0xE9 + 1 (Unconditional Branch)
        //   and
        // if sector_zero[510] = 0x55 && sector_zero[511] == 0xAA
        XDebugPrintf(_T("Checking file system type...\n"));
        lInmbpb = (VacpBpb *)(pchInmDeltaBuffer);
        if ( ( lInmbpb->BS_jmpBoot[0] == ( BS_JMP_BOOT_VALUE_0xEB + 1 ) ) || 
             ( lInmbpb->BS_jmpBoot[0] == ( BS_JMP_BOOT_VALUE_0xE9 + 1 ) ) ) 
        {
            if ( pchInmDeltaBuffer[ FST_SCTR_OFFSET_510 ] == FST_SCTR_OFFSET_510_VALUE && 
                 pchInmDeltaBuffer[ FST_SCTR_OFFSET_511 ] == FST_SCTR_OFFSET_511_VALUE ) 
            {
                if ( ( 0 != memcmp( &pchInmDeltaBuffer[ SV_NTFS_MAGIC_OFFSET ], SV_MAGIC_NTFS, SV_FSMAGIC_LEN ) ) &&
                     ( 0 != memcmp( &pchInmDeltaBuffer[ SV_NTFS_MAGIC_OFFSET ], SV_MAGIC_SVFS, SV_FSMAGIC_LEN ) ) )
                {
                    XDebugPrintf(_T("Found SV type. \n"));
                    hRC = S_OK;
                }
            }
        }
        if(hRC !=S_OK)
        {
            char szRootPathName[MAX_PATH+2] = {0};
            char szVolumeNameBuffer[MAX_PATH];;
            DWORD nVolumeNameSize = MAX_PATH;;
            DWORD dwVolumeSerialNumber;
            DWORD dwLength;
            DWORD dwFileSystemFlag;
            char szFileSystemNameBuffer[MAX_PATH];
            DWORD dwFSNameSize = MAX_PATH;

            // need to check whether given volume name has "\" as last charcter or not
            //if not there we need to add this
            //This is required for GetVolumeInformation() API as it expect dive or mountpoints must have trailing backslash.

            inm_strcpy_s(szRootPathName,ARRAYSIZE(szRootPathName),szVolume);
            if(szRootPathName[_tcslen(szRootPathName) -1] != _T('\\'))
            {
                inm_strcat_s(szRootPathName,ARRAYSIZE(szRootPathName),"\\");
            }

            if(!GetVolumeInformation(szRootPathName,szVolumeNameBuffer,nVolumeNameSize,&dwVolumeSerialNumber,&dwLength,
                    &dwFileSystemFlag,szFileSystemNameBuffer,dwFSNameSize))
            {
                XDebugPrintf("GetVolumeinformation API failed. Error :%d", GetLastError());
            }
            else
            {
                XDebugPrintf("FileSystem type is %s\n", szFileSystemNameBuffer);
            }
        }

    } while (FALSE);

    // Revert to original volume handle seek offset


    // Close any handles
    if(lInmhandleVolume !=INVALID_HANDLE_VALUE)
    {
        HRESULT hrCloseVolume = CloseVolume(lInmhandleVolume);
    }

    // Cleanup allocated memory
    if(pchInmDeltaBuffer)
    {
        delete[] pchInmDeltaBuffer;
    }

    XDebugPrintf("Exit IsThisFATfsHiddenVolume hRC = %x\n",hRC );
    if(HRESULT_FROM_WIN32(21)  == hr)//if the VSS_E_OBJECT_NOT_FOUND .This error code also means that the device is busy in the case of cluster passive disks and so we should skip them too.
    {
      inm_printf("\nThe device is busy. Mostly, it could be a passive/offline/inactive cluster disk on this node!\n");
      hRC = S_OK;
    }
    return hRC;
}



HRESULT OpenVolume( /* out */ HANDLE *pHandleVolume,
                   /* in */  const char   *pch_VolumeName
                   )
{
    XDebugPrintf("Entered OpenVolume\n");
    
    HRESULT hr = OpenVolumeExtended (pHandleVolume, pch_VolumeName, GENERIC_READ | GENERIC_WRITE);

    XDebugPrintf("Exit OpenVolume. hr =%x\n",hr);
    return hr;

}


/// Opens the given volume given its name and a pointer to a handle.


HRESULT OpenVolumeExtended( /* out */ HANDLE *pInmHandleVolume,
                           /* in */  const char   *pch_InmVolumeName,
                           /* in */  DWORD  dwInmMode
                           )
{
    HRESULT hr = S_OK;
    char *pch_InmDestVolume = NULL;
    XDebugPrintf("Entered OpenVolumeExtended\n");

    do
    {
       if( ( NULL == pInmHandleVolume ) ||
            ( NULL == pch_InmVolumeName ) )
        {
            XDebugPrintf("Invalid argument passed 1\n");
            hr = E_INVALIDARG;
            break;
        }
        if (0 != (dwInmMode & GENERIC_WRITE)) {
            TCHAR rgtszSystemDirectory[ MAX_PATH + 1 ];
            if( 0 == GetSystemDirectory( rgtszSystemDirectory, MAX_PATH + 1 ) )
            {
                XDebugPrintf("GetSystemDirecotry() API Failed. Error :%d\n", GetLastError());
                hr = HRESULT_FROM_WIN32( GetLastError() );
                break;
            }
            if( toupper( pch_InmVolumeName[ 0 ] ) == toupper( rgtszSystemDirectory[ 0 ] ) )
            {
                XDebugPrintf("Invalid argument passed 2\n");
                hr = E_INVALIDARG;
                break;
            }           
        }

        pch_InmDestVolume = new char[ MAX_PATH ];
        if( NULL == pch_InmDestVolume )
        {
            XDebugPrintf("Failed to allocate memory. Error :%d\n", GetLastError());
            hr = E_OUTOFMEMORY;
            break;
        }

        //to keep existing intact for normal drive letter
        //for Mountpoints it we pass directly volume name
        if(IsItVolumeMountPoint(pch_InmVolumeName))
        {
            char pszVolume[MAX_PATH] = {0};
            inm_strcpy_s(pszVolume,ARRAYSIZE(pszVolume),pch_InmVolumeName);
            inm_strcat_s(pszVolume,ARRAYSIZE(pszVolume),"\\");
            GetVolumeNameForVolumeMountPoint(pszVolume,pch_InmDestVolume,MAX_PATH);
            size_t i = strlen(pch_InmDestVolume);
            pch_InmDestVolume[i-1] = '\0';
            
            
        }
        else
        {
            char pszVolume[] = { 'X', ':', '\0' };
            pszVolume[0] = pch_InmVolumeName[0]; // get drive letter

            inm_strcpy_s( pch_InmDestVolume,MAX_PATH, "\\\\.\\" );
            inm_strcat_s( pch_InmDestVolume,MAX_PATH, pszVolume );
        }
        
        XDebugPrintf("Opening %s volume\n", pch_InmDestVolume);
        *pInmHandleVolume = CreateFile( pch_InmDestVolume,
            dwInmMode,
            FILE_SHARE_WRITE | FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL );
        if( INVALID_HANDLE_VALUE == *pInmHandleVolume )
        {
            XDebugPrintf("CreateFile faild. Error :%d\n", GetLastError());
            hr = HRESULT_FROM_WIN32( GetLastError() );
          
            break;
        }

      }
    while( FALSE );

    //Cleanup
    if(pch_InmDestVolume)
    {
        delete[] pch_InmDestVolume;
    }

    XDebugPrintf("Exit OpenVolumeExtended. hr= %x\n",hr);
    return( hr );
}


HRESULT CloseVolume( HANDLE InmhandleVolume )
{
    HRESULT hr = S_OK;

    do
    {
        if( ( INVALID_HANDLE_VALUE != InmhandleVolume ) &&
            !CloseHandle( InmhandleVolume ) )
        {
            hr = HRESULT_FROM_WIN32( GetLastError() );
        }
    }
    while( FALSE );

    return( hr );
}

#if (_WIN32_WINNT > 0x500)
bool GetApplicationComponentInfo(const char *AppName, const char *CompName,  vector<VssAppInfo_t> &AppInfo, ComponentInfo_t &AppComponent)
{
    const char *wName = MapApplication2VssWriter(AppName);

    if (wName == NULL)
        return false;

    for(unsigned int iApp = 0; iApp < AppInfo.size(); iApp++)
    {
        VssAppInfo_t vssApp = AppInfo[iApp];

        if (stricmp(wName, vssApp.AppName.c_str()) == 0)
        {
            for (unsigned int iComp = 0; iComp < vssApp.ComponentsInfo.size(); iComp++)
            {
                ComponentInfo_t cInfo = vssApp.ComponentsInfo[iComp];
        
                if ((stricmp(cInfo.componentName.c_str(), CompName) == 0) || (stricmp(cInfo.captionName.c_str(), CompName) == 0))
                {
                    AppComponent = cInfo;
                    return true;
                }
            
            }
            
        }
    }

    return false;

}
//Fill AppSummary with the information of all components.
bool FillAppSummaryAllComponentsInfo(AppSummary_t &ASum, vector<VssAppInfo_t> &AppInfo )
{
    XDebugPrintf("ENTERED:FillAppSummaryAllComponentsInfo\n");
    bool bRet = true;
    const char *wName = MapApplication2VssWriter(ASum.m_appName.c_str());
    if (wName == NULL)
        return false;

    for(unsigned int iApp = 0; iApp < AppInfo.size(); iApp++)
    {
        VssAppInfo_t &vssApp = AppInfo[iApp];
        if (stricmp(wName, vssApp.AppName.c_str()) == 0)
        {
          for (unsigned int iComp = 0; iComp < vssApp.ComponentsInfo.size(); iComp++)
          {
            ComponentInfo_t &cInfo = vssApp.ComponentsInfo[iComp];
            ComponentSummary_t cSummary;
            cSummary.fullPath = cInfo.fullPath;//Copy Full Path
            cSummary.bIsSelectable = cInfo.isSelectable;//Assign isSelectable for snapshot property
            cSummary.ComponentCaption = cInfo.captionName;//Copy Catpion name
            cSummary.ComponentLogicalPath = cInfo.logicalPath;//Copy logical path
            cSummary.ComponentName = cInfo.componentName;//Copy Component name
            ASum.m_vComponents.push_back(cSummary);
          }
        }
        else
        {
          bRet = false;
        }
    }
    return bRet;
}
#endif


// PArams:
// [out] wInmstring    : o/p buffer to store converted wide character string
// [in] wInmstringsize : size of wInmstring including terminating NULL char
// [in] tInmstring     : null terminated multibyte characters string
size_t tcstowcs(PWCHAR wInmstring, const size_t wInmstringsize, const PTCHAR tInmstring)
{
    size_t size = 0;
#ifdef UNICODE
    inm_wcscpy_s(wInmstring, wInmstringsize, tInmstring);
    size = wcslen(wInmstring);
#else
    mbstowcs_s(&size, wInmstring, wInmstringsize, tInmstring, wInmstringsize - 1);
#endif
    return size;
}

bool
MountPointToGUID(PTCHAR VolumeMountPoint, WCHAR *VolumeGUID, ULONG ulBufLen)
{
    std::vector<WCHAR> vMountPoint;
    WCHAR   UniqueVolumeName[60];

    if(IsItVolumeMountPoint(VolumeMountPoint))
    {
        ULONG MountPointLength; 
        INM_SAFE_ARITHMETIC(MountPointLength = InmSafeInt<ULONG>::Type((ULONG)_tcslen(VolumeMountPoint)) + 1, INMAGE_EX(_tcslen(VolumeMountPoint)))
        if(VolumeMountPoint[_tcslen(VolumeMountPoint) -1] != _T('\\')) {
            INM_SAFE_ARITHMETIC(MountPointLength += InmSafeInt<int>::Type(1), INMAGE_EX(MountPointLength))
        }
        vMountPoint.resize(MountPointLength, '\0');
        tcstowcs(&vMountPoint[0], vMountPoint.size(), VolumeMountPoint);
        vMountPoint[MountPointLength - 2] = _T('\\');
        vMountPoint[MountPointLength - 1] = _T('\0');
    }
    else
    {
        vMountPoint.resize(4, '\0');
        vMountPoint[0] = (WCHAR)VolumeMountPoint[0];
        vMountPoint[1] = L':';
        vMountPoint[2] = L'\\';
        vMountPoint[3] = L'\0';
    }
	const ULONG GUIDMinBuffLen = GUID_SIZE_IN_CHARS * sizeof(WCHAR);
    if (ulBufLen < GUIDMinBuffLen)
    {	
		inm_printf(" Error = VolumeGUID buffer length %lu is less than min expected buffer length %lu.\n", ulBufLen, GUIDMinBuffLen);
        return false;
    }

    if (0 == GetVolumeNameForVolumeMountPointW(vMountPoint.data(), UniqueVolumeName, _countof(UniqueVolumeName))) {
        _tprintf(_T("MountPointToGUID failed\n"));
        inm_printf(" Error = %d\n",GetLastError());
        return false;
    }

    inm_memcpy_s(VolumeGUID, ulBufLen, &UniqueVolumeName[11], (GUID_SIZE_IN_CHARS * sizeof(WCHAR)));
    //inm_printf("%s is mapped to Unique volume ",VolumeMountPoint);
    //wprintf(L"%s\n", UniqueVolumeName); 
        
    return true;
}


// Get the volume's root path name for the given path
HRESULT  GetVolumeRootPathEx(const char *src, char *dst, unsigned int dstlen, bool& validateMountPoint)
{
    HRESULT hr = S_OK;
    size_t len;
    DWORD retval = 0;
    DWORD dwError =0;
    char tmp[MAX_PATH+1];
    char szVolumePath[MAX_PATH +1] = {0};

    do
    {
        if (src == NULL || dst == NULL)
        {
            hr = E_FAIL;
            break;
        }

        (void)memset(dst, 0, dstlen);
        //dst[dstlen-1] = '\0';

        //Expand EnvironmentStrings
        retval = ExpandEnvironmentStrings(src, tmp, MAX_PATH);

        if (retval == 0)
        {
            dwError = GetLastError();
            hr = E_FAIL; 
            DebugPrintf("FAILED: ExpandEnvironmentStrings failed,Error = [%d], hr = 0x%x\n ", dwError,hr);
            break;
        }

        // Add the backslash termination, if needed
        len = strlen(tmp);

        if (tmp[len-1] != '\\')
        {
            tmp[len] = '\\';
            tmp[len+1] = '\0';
        }
        
        //Get Volume Mount point name, i.e, Root path of the volume
        retval = GetVolumePathName(tmp, dst, dstlen);

        if (retval == 0)
        {
            dwError = GetLastError();
            hr = E_FAIL; 
            DebugPrintf("\ntemp = %s\n dst = %s\n dstlen = %d\n FAILED: GetVolumePathName failed, Error =[%d], hr = 0x%x\n",tmp,dst,dstlen,dwError,hr);
            break;
        }

        // if volume mount point validation is false,
        // do not perform volume mount point validation.
        
        // This is useful while enumerating VSS applications

        // Volumes affected by VSS applcations may be stale.i.e, volumes may not exist 
        // but VSS writers may be returning stale information.

        /*if (validateMountPoint == false)
        {
            break;
        }*/
#if (_WIN32_WINNT >= 0x0501)
        if(strstr(dst,"\\\\?\\Volume"))
        {
            DWORD dwVolPathSize = 0;
            GetVolumePathNamesForVolumeName(dst,szVolumePath,MAX_PATH,&dwVolPathSize);
            memset(dst, 0, dstlen);
            inm_strcpy_s(dst, dstlen, szVolumePath);
        }
        if(strstr(dst,"\\\\?\\GLOBALROOT"))
        {
            DWORD dwVolPathSize = 0;
            GetVolumePathNamesForVolumeName(dst,szVolumePath,MAX_PATH,&dwVolPathSize);
            memset(dst, 0, dstlen);
            inm_strcpy_s(dst, dstlen, szVolumePath);
        }
#endif
        if(IsItVolumeMountPoint(dst))
        {
            if(IsItVolumeMountPoint(dst))
            {
                validateMountPoint = true;
            }
            else
            {
                hr = E_FAIL;
                inm_printf("%s:%d Encountered Invalid volume %s \n", __FILE__, __LINE__,dst);
                break;
            }
        }
        else
        {
            break;
        }
        //Get Volume Name for VOlume Mount Point
        char volumeName[MAX_PATH+1];
        (void)memset(volumeName, '\0', sizeof(volumeName));
        retval = GetVolumeNameForVolumeMountPoint(dst, volumeName, MAX_PATH);

        if (retval == 0)
        {
            dwError = GetLastError();
            hr = E_FAIL; 
            DebugPrintf("FAILED: GetVolumeNameForVolumeMountPoint failed, Error=[%d], hr = 0x%x\n", dwError,hr);
            break;
        }

        //Get Unqiue Volume Name
        char volumeUniqueName[MAX_PATH+1];
        (void)memset(volumeUniqueName, '\0', sizeof(volumeUniqueName));
        retval = GetVolumeNameForVolumeMountPoint(volumeName, volumeUniqueName, MAX_PATH);

        if (retval == 0)
        {
            dwError = GetLastError();
            hr = E_FAIL; 
            DebugPrintf("FAILED: GetVolumeNameForVolumeMountPoint failed, Error = [%d]hr = 0x%x\n", dwError, hr);
            break;
        }
        inm_strcpy_s(dst, dstlen, volumeUniqueName);
    } while(FALSE);

    return hr;	
}


bool IsValidVolumeEx(const char *srcVol,
    bool bCheckIfItCanBeSnapshot,
    CComPtr<IVssBackupComponentsEx> ptrVssBackupComponentsEx)
{
    USES_CONVERSION;
    bool bRet = false;
    HRESULT hr = S_OK;
    //char RootPath[MAX_PATH+1];
    //char SrcPath[MAX_PATH+1];
    char SrcPath[2048];

    size_t len;
    
    if (srcVol == NULL)
    {
        return false;
    }

    // Copy srcVol to local buffer, SrcPath
    len = strlen(srcVol);
    if(0 == len)
    {
        return false;
    }
    inm_memcpy_s(SrcPath,sizeof(SrcPath), srcVol, len+1);

    //SrcPath should always need to have trailing slash.
    //Add trailing slash, if needed
    if (SrcPath[len-1] != '\\')
    {
        SrcPath[len] = '\\';
        SrcPath[++len] = '\0';
    }

    //Get Mount point of the srcVol
    //hr = GetVolumeRootPathEx(SrcPath, RootPath, MAX_PATH, validateMountPoint);

    //if (hr != S_OK)
    //{
    //	return false;
    //}

    //Check if the device type is fixed hard disk or not
    UINT uDriveType = GetDriveType(SrcPath);	
    if (DRIVE_FIXED != uDriveType)
    {
        inm_printf("%s is NOT a fixed hard drive\n",SrcPath);
        if(!gbSkipUnProtected)
        {
            ghrErrorCode = VACP_E_NON_FIXED_DISK;
        }
        switch(uDriveType)
        {
            case DRIVE_UNKNOWN:
            {
                inm_printf("\n %s is UNKNOWN_DRIVE\n",SrcPath);
                break;
            }
            case DRIVE_NO_ROOT_DIR:
            {
                inm_printf("\n %s is DRIVE_NO_ROOT_DIR\n",SrcPath);
                break;
            }
            case DRIVE_REMOVABLE:
            {
                inm_printf("\n %s is DRIVE_REMOVABLE\n",SrcPath);
                break;
            }
            case DRIVE_FIXED:
            {
                inm_printf("\n %s is DRIVE_FIXED\n",SrcPath);
                break;
            }
            case DRIVE_REMOTE:
            {
                inm_printf("\n %s is DRIVE_REMOTE\n",SrcPath);
                break;
            }
            case DRIVE_CDROM:
            {
                inm_printf("\n %s is DRIVE_CDROM\n",SrcPath);
                break;
            }
            case DRIVE_RAMDISK:
            {
                inm_printf("\n %s is DRIVE_RAMDISK\n",SrcPath);
                break;
            }
        }
        return false;
    }

    BOOL bIsVolSupported = FALSE;

    bRet = true;
    if(!bCheckIfItCanBeSnapshot)
        return bRet;

    //Check if the Volume can be snapshotted by VSS. Especially the InMage's vnap volumes cannot be snapshotted by VSS.
    if (ptrVssBackupComponentsEx != NULL)
    {
        if (SUCCEEDED(hr = ptrVssBackupComponentsEx->IsVolumeSupported(GUID_NULL,
            A2W(SrcPath),
            &bIsVolSupported)))
        {
            if (!bIsVolSupported)
            {
                inm_printf("\nThis volume %s is not supported by VSS and hence skipping it.\n", SrcPath);
            }
        }
        else
        {
            inm_printf("\nSkipping volume %s as IsVolumeSupported failed with error hr = %0x.\n", SrcPath, hr);
        }

        return bIsVolSupported;
    }

    do
    {
        HRESULT hr = S_FALSE;
        CComPtr<IVssBackupComponents> pVssBackupComponents = NULL;
        CComPtr<IVssBackupComponentsEx> pVssBackupComponentsEx = NULL;
        if(S_OK != (hr = CreateVssBackupComponents(&pVssBackupComponents)))
        {
            inm_printf("\n Failed to CreateVssBackupComponents hr = %0x\n",hr);
            break;
        }
        if(S_OK != (hr = pVssBackupComponents->QueryInterface(IID_IVssBackupComponentsEx,(void**)&pVssBackupComponentsEx)))
        {
            inm_printf("\n Failed to QueryInterface for IVssBackupComponentsEx, hr = %0x\n",hr);
            break;
        }
        if(S_OK !=(hr = pVssBackupComponentsEx->InitializeForBackup()))
        {
            inm_printf("\n Failed to InitializeForBackup, hr = %0x\n",hr);
            break;
        }
        
        if(S_OK !=(hr = (pVssBackupComponentsEx->SetBackupState (true,true,VSS_BT_COPY,false))))
        {
            inm_printf("\n Failed to Set Backup State, hr = %0x\n",hr);
            break;
        }
        if(S_OK != (hr = pVssBackupComponentsEx->SetContext(VSS_CTX_BACKUP)))
        {
            inm_printf("\n Failed to set contex for VssBackupComponentsEx, hr = %0x\n",hr);
            break;
        }

        // using GUID_NULL for provider id so that VSS checks the Microsoft VSS provider
        // InMage VSS Provider returns false when snapshot is not in-progress
        if(SUCCEEDED(hr = pVssBackupComponentsEx->IsVolumeSupported(GUID_NULL,
                                              A2W(SrcPath),
                                              &bIsVolSupported)))
        {
            if(FALSE == bIsVolSupported)
            {
                inm_printf("\nThis volume %s is not supported by VSS and hence skipping it.\n",SrcPath);
                break;
            }
        }
        else
        {
            inm_printf("\n hr = %0x\n",hr);
        }
    }while(false);
    
    if(FALSE == bIsVolSupported)
    {
      bRet= false;
    }
    else if(TRUE == bIsVolSupported)
    {
      bRet= true;
    }
    return bRet;
}

// This function returns VolumeIndex for the given volume
// It returns 0 for A, 1 for B, 2 for C and so on.
HRESULT GetVolumeIndexFromVolumeNameEx(const char *volumeName, DWORD *pVolumeIndex, bool validateMountPoint)
{
    HRESULT hr = S_OK;
    char szVolumePath[MAX_PATH+1] = {0};
    char const* pVolumePath = volumeName;
    if(strlen(volumeName) == 0)
    {
      inm_printf("\n It is an empty volume.\n");
      return hr;
    }

    do
    {
#if (_WIN32_WINNT >= 0x0501)
        if(strstr(volumeName,"\\\\?\\Volume"))
        {
            DWORD dwVolPathSize = 0;

            GetVolumePathNamesForVolumeName(volumeName,szVolumePath,MAX_PATH,&dwVolPathSize);
            if(strlen((const char*)szVolumePath) > 0)
            {
              pVolumePath = szVolumePath;
            }
            else
            {
              inm_printf("\nThere is no Mount Point for this volume %s\n",volumeName);
              *pVolumeIndex = 0;
              return hr;
            }
        }
        if(strstr(volumeName,"\\\\?\\GLOBALROOT"))
        {
            DWORD dwVolPathSize = 0;

            GetVolumePathNamesForVolumeName(volumeName,szVolumePath,MAX_PATH,&dwVolPathSize);
            if(strlen((const char*)szVolumePath) > 0)
            {
              pVolumePath = szVolumePath;
            }
            else
            {
              inm_printf("\nThere is no Mount Point for this volume %s\n",volumeName);
              *pVolumeIndex = 0;
              return hr;
            }
        }
#endif
        if(IsValidVolumeEx(pVolumePath) == false || pVolumeIndex == NULL)
        {
            hr = E_FAIL;
            inm_printf("\nFILE = %s ;LINE = %d; ****InvalidVolume = %s\n",__FILE__,__LINE__,pVolumePath);//CHECK
            break;
        }

        if(validateMountPoint)
        {
            *pVolumeIndex = 'A' + 29;
            //inm_printf("\n *** It's a Mount Point; Volume Index = %d\n",pVolumeIndex);//CHECK
            break;
        }
        if (pVolumePath[0] >= 'A' && pVolumePath[0] <= 'Z')
        {
            *pVolumeIndex = (pVolumePath[0] - 'A');
            break;
        }

        if (pVolumePath[0] >= 'a' && pVolumePath[0] <= 'z')
        {
            *pVolumeIndex = (pVolumePath[0] - 'a');
            break;
        }
        hr = E_FAIL;
        OSVERSIONINFO osVerInfo;
        ZeroMemory(&osVerInfo,sizeof(osVerInfo));
        osVerInfo.dwOSVersionInfoSize = sizeof(osVerInfo);
        BOOL bVer = ::GetVersionEx(&osVerInfo);
        if((osVerInfo.dwMajorVersion >= WIN2K8_SERVER_VER_MAJOR/*i.e for W2K8 R2*/)&&
                    (osVerInfo.dwMinorVersion >= WIN2K8_SERVER_VER_MINOR))//this condition applies to both W2K8 and W2K8R2
        {
            //inm_printf("\n FILE = %s ; LINE = %d; GetVolumeIndex is success for W2K8 and W2K8R2!\n",__FILE__,__LINE__);
            hr = S_OK;
        }
    } while(FALSE);
    return hr;
}

bool IsItVolumeMountPoint(const char* volumeName)
{
    WIN32_FILE_ATTRIBUTE_DATA w32FileData;
    bool ret =false;
        
    if(GetFileAttributesEx(volumeName,GetFileExInfoStandard,&w32FileData))
    {
        if(w32FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY  )
        {
            if(w32FileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT )
                ret = true;
        }
    }

    return ret;

}


#if (_WIN32_WINNT > 0x500)
void GetVolumeMountPointInfoForApp(const char *AppName, vector<VssAppInfo_t> &AppInfo, 
                               VOLMOUNTPOINT_INFO &volMountPoint_info)
{
    const char *wName = MapApplication2VssWriter(AppName);

    for(unsigned int iApp = 0; iApp < AppInfo.size(); iApp++)
    {
        VssAppInfo_t vssApp = AppInfo[iApp];

        if (strnicmp(wName, vssApp.AppName.c_str(), strlen(wName)) == 0)
        {
            for(unsigned int i = 0; i < vssApp.volMountPoint_info.dwCountVolMountPoint; i++)
            {
                if(!IsVolMountPointAlreadyExist(volMountPoint_info, vssApp.volMountPoint_info.vVolumeMountPoints[i].strVolumeMountPoint))
                {
                    volMountPoint_info.dwCountVolMountPoint++;
                    volMountPoint_info.vVolumeMountPoints.push_back(vssApp.volMountPoint_info.vVolumeMountPoints[i]);
                }
            }
            //Remove this to handle Exchange 2007 writers as Exchange 2007 has Information Store and Replication 
            //writer with same name "Microsoft Exchange writer". We have to add both the writers volume.
            //If we use break it will include only onw writer's mountpoints and not other
            //break;   
        }
    }
}

#endif

#if (_WIN32_WINNT >= 0x502)
void GetVolumeMountPointInfoForAppEx(const char *AppName, const char *VssWriterInstanceName, vector<VssAppInfo_t> &AppInfo, 
                               VOLMOUNTPOINT_INFO &volMountPoint_info,bool bWriterInstanceName)
{
    const char *wName;
        
    if(bWriterInstanceName)
    {
        wName = MapWriterName2VssWriter(AppName);
    }
    else
    {
        wName = MapApplication2VssWriter(AppName);
    }

    if(wName == NULL)
    {
        return;
    }
    for(unsigned int iApp = 0; iApp < AppInfo.size(); iApp++)
    {
        VssAppInfo_t vssApp = AppInfo[iApp];

        if(bWriterInstanceName)
        {
            if(wName && !vssApp.szWriterInstanceName.empty())
            {
                if (strnicmp(wName, vssApp.szWriterInstanceName.c_str(), strlen(wName)) == 0)
                {
                    for(unsigned int i = 0; i < vssApp.volMountPoint_info.dwCountVolMountPoint; i++)
                    {
                        if(!IsVolMountPointAlreadyExist(volMountPoint_info, vssApp.volMountPoint_info.vVolumeMountPoints[i].strVolumeMountPoint))
                        {
                            volMountPoint_info.dwCountVolMountPoint++;
                            volMountPoint_info.vVolumeMountPoints.push_back(vssApp.volMountPoint_info.vVolumeMountPoints[i]);
                        }
                    }
                    break;
                }
            }
        }
        else
        {

            if (strnicmp(wName, vssApp.AppName.c_str(), strlen(wName)) == 0)
            {
                for(unsigned int i = 0; i < vssApp.volMountPoint_info.dwCountVolMountPoint; i++)
                {
                    if(!IsVolMountPointAlreadyExist(volMountPoint_info, vssApp.volMountPoint_info.vVolumeMountPoints[i].strVolumeMountPoint))
                    {
                        volMountPoint_info.dwCountVolMountPoint++;
                        volMountPoint_info.vVolumeMountPoints.push_back(vssApp.volMountPoint_info.vVolumeMountPoints[i]);
                    }
                }
                //Removed this 'break' to handle Exchange 2007 writers as Exchange 2007 has Information Store and Replication 
                //writer with same name "Microsoft Exchange writer". We have to add both the writers volume.
                //If we use break it will include only onw writer's mountpoints and not other
                //break;   
            }
        }
    }
}

#endif


bool IsVolMountPointAlreadyExist(VOLMOUNTPOINT_INFO& targetVolMP_info, string volMountPoint)
{
    bool ret = false;
    for(DWORD  i = 0; i <targetVolMP_info.dwCountVolMountPoint; i++)
    {
        if(!strnicmp(targetVolMP_info.vVolumeMountPoints[i].strVolumeMountPoint.c_str(), volMountPoint.c_str(),volMountPoint.size()))
        {
            ret = true;
        }
    }
    return ret;
}


HRESULT GetTagVolumeStatus()
{
    XDebugPrintf("ENTERED: GetTagVolumeStatus\n");
    HRESULT hr = E_FAIL;
    BOOL bResult = NULL;
    DWORD dwReturn = NULL;
    DWORD dwRetValue = NULL;

    do
    {
        if(gbGetVolumeStatus)
        {
            /*ULONG   ulOutputLength = SIZE_OF_TAG_VOLUME_STATUS_OUTPUT_DATA(gdwNumberOfVolumes);
            PTAG_VOLUME_STATUS_OUTPUT_DATA TagOutputData = (PTAG_VOLUME_STATUS_OUTPUT_DATA)malloc(ulOutputLength);
            PTAG_VOLUME_STATUS_INPUT_DATA StatusInputData = (PTAG_VOLUME_STATUS_INPUT_DATA)malloc(sizeof(TAG_VOLUME_STATUS_INPUT_DATA));
            PTAG_VOLUME_STATUS_OUTPUT_DATA StatusOutputData = (PTAG_VOLUME_STATUS_OUTPUT_DATA)malloc(ulOutputLength);

        
            if ((NULL == StatusInputData)||(NULL == StatusOutputData) || (NULL == TagOutputData)) 
            {
                DebugPrintf("Invalid number of volumes\n");
                break;
            }*/

            if(!gbTagSend )
            {
                //validate whether ghControlDevice is pointing to correct 
                if( ghControlDevice == INVALID_HANDLE_VALUE || ghControlDevice == NULL)
                {
                    DebugPrintf("Invalid Involflt driver handle\n");
                    break;
                }

                //For Async tags, the default and only wait period is 120 seconds....a decision taken arbitrarily
                dwRetValue = WaitForSingleObject(gOverLapTagIO.hEvent, 120000);
                
                if (WAIT_TIMEOUT == dwRetValue) 
                {
                    inm_printf("\nTimeout occured for the tag(s) to commit... \nThe tag(s) may or may not reach the target!\n\n");
                    inm_printf("\nTAG_VOLUME_INPUT_FLAGS_DEFERRED_TILL_COMMIT_SNAPSHOT ioctl is not received.\n");
                    gbIOCompleteTimeOutOccurred = true;
                    break;
                } 
                else if (dwRetValue == WAIT_OBJECT_0)
                {
                    //inm_printf("\nIO Wait succeeded.\n");
                    if (GetOverlappedResult(ghControlDevice, &gOverLapTagIO, &dwReturn, FALSE))
                    {
                        hr = S_OK;
                        inm_printf("\nWaiting for IO complete suceeded.\n");
                        gbTagSend = true;
                    }
                    else
                    {		gbTagSend = false;
                            inm_printf("===============================\n");
                            inm_printf("Win32 error occurred [%d]\n",GetLastError());
                            inm_printf("Failed to commit tags to one or all volumes\n");
                            inm_printf("===============================\n");
                            //gbIOCompleteTimeOutOccurred = true;//though it is not timeout, but we should not send revocation tags in this case as well as the original tag itself is dropped by the driver.
                            //The above line is commented to ensure that if in case because of VSS takes more time
                            //to flush and hold the writes then in that case vacp may not recieve the overlapped 
                            //object set and there is a chance that the tag may be accepted by InVolFlt driver 
                            //and sent to target.
                    }
                }
                else
                {
                  inm_printf("\nFailed to commit the tag.\n");
                  gbIOCompleteTimeOutOccurred = true;//though it is not timeout, but we should not send revocation tags in this case as well as the original tag itself is dropped by the driver.
                }
            }
        }
    }while(0);
    // Close the Handle to the control device
    /*if( !(ghControlDevice == INVALID_HANDLE_VALUE || ghControlDevice == NULL))
    {
        CloseHandle(ghControlDevice);
        ghControlDevice = NULL;
    }*/

    XDebugPrintf("EXITED: GetTagVolumeStatus\n");
    return hr;
}
HRESULT GetSyncTagVolumeStatus()
{
    XDebugPrintf("ENTERED: GetSyncTagVolumeStatus\n");
    HRESULT hr = E_FAIL;
    BOOL bResult = NULL;
    DWORD dwReturn = NULL;
    DWORD dwRetValue = NULL;

    do
    {
        if(gbTagSend == true)
        {
            hr = S_OK;
            break;
        }

        if(gbGetSyncVolumeStatus)
        {
            ULONG   ulOutputLength = SIZE_OF_TAG_VOLUME_STATUS_OUTPUT_DATA(gdwNumberOfVolumes);
            PTAG_VOLUME_STATUS_OUTPUT_DATA TagOutputData = (PTAG_VOLUME_STATUS_OUTPUT_DATA)malloc(ulOutputLength);
            PTAG_VOLUME_STATUS_INPUT_DATA StatusInputData = (PTAG_VOLUME_STATUS_INPUT_DATA)malloc(sizeof(TAG_VOLUME_STATUS_INPUT_DATA));
            PTAG_VOLUME_STATUS_OUTPUT_DATA StatusOutputData = (PTAG_VOLUME_STATUS_OUTPUT_DATA)malloc(ulOutputLength);

        
            if ((NULL == StatusInputData)||(NULL == StatusOutputData) || (NULL == TagOutputData)) 
            {
                DebugPrintf("Invalid number of volumes\n");
                break;
            }

            if(!gbTagSend )
            {
                //validate whether ghControlDevice is pointing to correct 
                if( ghControlDevice == INVALID_HANDLE_VALUE || ghControlDevice == NULL)
                {
                    DebugPrintf("Invalid Involflt driver handle\n");
                    break;
                }

                if(gdwTagTimeout == INFINITE)
                {
                    DebugPrintf("Waiting forever till tags get committed... \n");
                    dwRetValue = WaitForSingleObject(gOverLapIO.hEvent, INFINITE);
                }
                else
                {
                    DebugPrintf("Waiting %d secs for the tags to commit... \n",gdwTagTimeout);
                    dwRetValue = WaitForSingleObject(gOverLapIO.hEvent, gdwTagTimeout*1000);
                }
                
                if (WAIT_TIMEOUT == dwRetValue) 
                {
                    DebugPrintf("\nTimeout occured for the tag(s) to commit... \nThe tag(s) may or may not reach the target!\n\n");
                    gbIOCompleteTimeOutOccurred = true;//We should not send revocation tags in this case as well as the original tag itself is dropped by the driver.
                    inm_memcpy_s(&StatusInputData->TagGUID,sizeof(StatusInputData->TagGUID), &gTagGUID, sizeof(GUID));
                    bResult = DeviceIoControl(
                            ghControlDevice, 
                            (DWORD)IOCTL_INMAGE_GET_TAG_VOLUME_STATUS, 
                            StatusInputData, 
                            sizeof(TAG_VOLUME_STATUS_INPUT_DATA),
                            (PVOID)StatusOutputData, 
                            ulOutputLength, 
                            &dwReturn, 
                            NULL
                        );
                    if (bResult && (dwReturn >= ulOutputLength)) 
                    {
                        PrintTagStatusOutputData(StatusOutputData,gvVolumes);
                    }
                } 
                else if (dwRetValue == WAIT_OBJECT_0)
                {
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
                            gbIOCompleteTimeOutOccurred = true;//though it is not timeout, but we should not send revocation tags in this case as well as the original tag itself is dropped by the driver.
                        }
                        if (dwReturn >= ulOutputLength) 
                        {
                            PrintTagStatusOutputData((PTAG_VOLUME_STATUS_OUTPUT_DATA)gpTagOutputData, gvVolumes);
                            gbIOCompleteTimeOutOccurred = true;//though it is not timeout, but we should not send revocation tags in this case as well as the original tag itself is dropped by the driver.
                        }
                    }
                    else
                    {		gbTagSend = false;
                            DebugPrintf("===============================\n");
                            DebugPrintf("Win32 error occurred [%d]\n",GetLastError());
                            DebugPrintf("Failed to commit tags to one or all volumes\n");
                            DebugPrintf("===============================\n");
                            gbIOCompleteTimeOutOccurred = true;//though it is not timeout, but we should not send revocation tags in this case as well as the original tag itself is dropped by the driver.
                    }
                }
                else
                {
                    inm_memcpy_s(&StatusInputData->TagGUID,sizeof(StatusInputData->TagGUID), &gTagGUID, sizeof(GUID));
                    bResult = DeviceIoControl(
                            ghControlDevice, 
                            (DWORD)IOCTL_INMAGE_GET_TAG_VOLUME_STATUS, 
                            StatusInputData, 
                            sizeof(TAG_VOLUME_STATUS_INPUT_DATA),
                            (PVOID)StatusOutputData, 
                            ulOutputLength, 
                            &dwReturn, 
                            NULL
                        );
                    if (bResult && (dwReturn >= ulOutputLength)) 
                    {
                        PrintTagStatusOutputData(StatusOutputData,gvVolumes);
                    }
                    gbIOCompleteTimeOutOccurred = true;//though it is not timeout, but we should not send revocation tags in this case as well as the original tag itself is dropped by the driver.
                }
            }
        }
    }while(0);
    // Close the Handle to the control device
    if( !(ghControlDevice == INVALID_HANDLE_VALUE || ghControlDevice == NULL))
    {
        CloseHandle(ghControlDevice);
        ghControlDevice = NULL;
    }

    XDebugPrintf("EXITED: GetSyncTagVolumeStatus\n");
    return hr;
}
bool IsDriverLoaded(_TCHAR* szDriverName)
{
    _TCHAR* driverName = gbUseDiskFilter ? INMAGE_DISK_FILTER_DOS_DEVICE_NAME : INMAGE_FILTER_DOS_DEVICE_NAME;
    if (_tcscmp(driverName, szDriverName) != 0)
        return false;

    if (ghControlDevice == INVALID_HANDLE_VALUE)
    {
        DebugPrintf("Invalid Involflt driver handle\n");
        return false;
    }

    return true;
}

DWORD CheckDriverStat(HANDLE hVFCtrlDevice, WCHAR* volumeGUID, etWriteOrderState *pDriverWriteOrderState, DWORD *dwLastError, DWORD dwVolumeGUIDSize)
{
    DWORD  bResult = true;
    if(hVFCtrlDevice == NULL)
    {
        hVFCtrlDevice = CreateFile (
                                gbUseDiskFilter ? INMAGE_DISK_FILTER_DOS_DEVICE_NAME : INMAGE_FILTER_DOS_DEVICE_NAME,
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING,
                                gbUseDiskFilter ? FILE_ATTRIBUTE_NORMAL : FILE_FLAG_OVERLAPPED,
                                NULL
                                );

        if( INVALID_HANDLE_VALUE == hVFCtrlDevice )
        {
            bResult = false;
            goto end;
        }
    }

    DWORD dwReturn;
    if (!dwVolumeGUIDSize) 
        dwVolumeGUIDSize  = (sizeof(wchar_t)* GUID_SIZE_IN_CHARS);
    
    bResult = DeviceIoControl(
                    hVFCtrlDevice,
                    (DWORD)IOCTL_INMAGE_GET_VOLUME_WRITE_ORDER_STATE,
                    volumeGUID,
                    dwVolumeGUIDSize,
                    pDriverWriteOrderState,
                    sizeof(etWriteOrderState),
                    &dwReturn,  
                    NULL        // Overlapped
                    );

        *dwLastError = GetLastError();
end:
    return bResult;

}

std::string  GetLocalSystemTime()
{
    SYSTEMTIME sysTime,localTime;
    TIME_ZONE_INFORMATION   TimeZone;
    std::stringstream sSystemTime;
    GetSystemTime(&sysTime);
    GetTimeZoneInformation(&TimeZone);
    SystemTimeToTzSpecificLocalTime(&TimeZone,&sysTime,&localTime);
    sSystemTime<<localTime.wHour<<":"<<localTime.wMinute<<":"<<localTime.wSecond<<":"<<localTime.wMilliseconds;
    return sSystemTime.str();
}

bool GetSystemUptimeInSec(uint64_t& uptime)
{
    uptime = GetTickCount64();
    // convert to secs as tick count is in milli seconds
    uptime /= 1000;
    return true;
}


bool ExtractAppsWithSR(std::string &strApps,std::string &strNewApps)
{	
    string strSRAppList ="SystemFiles;Registry;EventLog;WMI;COM+REGDB;FS;CA;AD;FRS;BootableState;ServiceState;ClusterService;VSSMetaDataStore;PerformanceCounter;;ADAM;ClusterDB;TSG;DFSR";
    string strSR = "sr";
    size_t nSize = strApps.find("sr");
    if(nSize > 0)
    {
        strNewApps = strApps.substr(0,nSize) + strSRAppList +strApps.substr(nSize+strSR.size(),strApps.npos);
    }
    else if (0 == nSize)
    {
        strNewApps = strSRAppList +strApps.substr(nSize+strSR.size(),strApps.npos);
    }
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

DWORD ExecuteScript(std::string strScript,SCRIPT_TYPE scriptType)
{
  STARTUPINFO siScript;
  PROCESS_INFORMATION piScript;
  DWORD dwExitCode = 0;
  DWORD dwScriptErr = 0;
  DWORD dwWaitTime = 0;
  DWORD dwTimeOutErrorCode = 0;
  DWORD dwScriptFailedReason = 0;
  DWORD dwWaitResult = 0;
  std::string script;

  ZeroMemory(&siScript,sizeof(siScript));
  ZeroMemory(&piScript,sizeof(piScript));    
  
  if(scriptType == ST_PRESCRIPT)
  {
    script = "PreScript";
    dwWaitTime = PRESCRIPT_WAIT_TIME;
    dwTimeOutErrorCode = VACP_PRESCRIPT_WAIT_TIME_EXCEEDED;
    dwScriptFailedReason = VACP_E_PRESCRIPT_FAILED;
  }
  else
  {
    script = "PostScript";
    dwWaitTime = POSTSCRIPT_WAIT_TIME;
    dwTimeOutErrorCode = VACP_POSTSCRIPT_WAIT_TIME_EXCEEDED;
    dwScriptFailedReason = VACP_E_POSTSCRIPT_FAILED;
  }
  BOOL bRes = FALSE;
  bRes = CreateProcess(NULL,
                  const_cast<char*>(strScript.c_str()),
                  NULL,
                  NULL,
                  FALSE,
                  0,
                  NULL,
                  NULL,
                  &siScript,
                  &piScript);
 
    //Create a new process for prescript
    if(0 == bRes)
    {
      dwScriptErr = GetLastError();
      inm_printf("\nFailed to invoke %s. Error = %d",script.c_str(),dwScriptErr);
      exit(dwScriptErr);
    }
    else
    {
      //Wait a maximum of 5 minutes for the Prescript process to exit.
      dwWaitResult = WaitForSingleObject(piScript.hProcess,dwWaitTime);
      if (WAIT_TIMEOUT == dwWaitResult)
      {
        inm_printf("A max time out of %d elapsed \n", dwWaitTime);
        inm_printf("and hence terminating the %s process with Exit Code = %d\n", script.c_str(), dwTimeOutErrorCode);
        TerminateProcess(piScript.hProcess,dwTimeOutErrorCode);
        CloseHandle(piScript.hProcess);
        CloseHandle(piScript.hThread);
        exit(dwTimeOutErrorCode);
      }
      else if(WAIT_OBJECT_0 == dwWaitResult)
      {
          inm_printf("Successfully executed the %s script %s\n", script.c_str(), strScript.c_str());
        //DWORD dwExitCode = 0;
        if(!GetExitCodeProcess(piScript.hProcess,&dwExitCode))
        {
          dwExitCode = GetLastError();
          inm_printf("Failed to get the Exit Code of the %s. Error Code =  %d\n", script.c_str(), dwExitCode);
          exit(dwScriptFailedReason);
        }
        else
        {
          if(STILL_ACTIVE != dwExitCode)
          {
            //Check if the exited process returned Success (0) or Failed (non-zero).
            if(dwExitCode != 0)
            {
                inm_printf("The %s Process failed. ExitCode = %d\n", strScript.c_str(), dwExitCode);
              exit(dwScriptFailedReason);
            }
            else
            {
                inm_printf("%s Process executed successfully!\n", strScript.c_str());
            }
          }
        }
        CloseHandle(piScript.hProcess);
        CloseHandle(piScript.hThread);
      }
    }
    return dwExitCode;
}

bool IsThisVolumeProtected(std::string strVol)
{
    bool bRet = true;
    do
    {
            if (IsDriverLoaded(INMAGE_FILTER_DOS_DEVICE_NAME))
            {
                //convert the VolumePathName to Volume GUID
                unsigned long GuidSize = GUID_SIZE_IN_CHARS * sizeof(WCHAR);
                WCHAR *VolumeGUID = new WCHAR[GUID_SIZE_IN_CHARS];
                if (!VolumeGUID)
                {
                    bRet = false;
                    break;
                }
                else
                {
                    memset((void*)VolumeGUID, 0x00, GuidSize);
                }
                if (!MountPointToGUID((PTCHAR)(strVol.c_str()), VolumeGUID, GuidSize))
                {
                    delete VolumeGUID;
                    VolumeGUID = NULL;
                    bRet = false;
                    break;
                }
                //Now check if this Volume GUID is protected or not! or a vsnap or not even existing!	
                etWriteOrderState DriverWriteOrderState;
                DWORD dwLastError = 0;
                if (!CheckDriverStat(ghControlDevice, VolumeGUID, &DriverWriteOrderState, &dwLastError))
                {
                    bRet = false;
                    if (VolumeGUID)
                    {
                        delete VolumeGUID;
                        VolumeGUID = NULL;
                    }
                    inm_printf("\nWarning:This Volume %s is not protected @Time: %s \n", strVol.c_str(), GetLocalSystemTime().c_str());
                    break;
                }
                if (VolumeGUID)
                {
                    delete VolumeGUID;
                    VolumeGUID = NULL;
                }
            }//end of if(IsDriverLoaded(INMAGE_FILTER_DOS_DEVICE_NAME))
    }while(0);

    return bRet;
}


bool IsThisDiskProtected(const std::string &strVol, 
                         const std::map<std::wstring, std::string> &diskGuidMap)
{
    boost::mutex::scoped_lock guard(g_diskInfoMutex);

    bool bRet = true;
    do
    {
            etWriteOrderState DriverWriteOrderState;
            DWORD dwLastError = 0;

            WCHAR *diskGuid = new (std::nothrow) WCHAR[GUID_SIZE_IN_CHARS];
            if (diskGuid == NULL)
            {
                inm_printf("\nError: Failed to allocate %d bytes for diskGuid.\n", GUID_SIZE_IN_CHARS);
                bRet = false;
                break;
            }

            DWORD dwVolumeGUIDSize;
            memset((void*)diskGuid, 0x00, GUID_SIZE_IN_CHARS * sizeof(WCHAR));

            std::map<wstring, string>::const_iterator diskIter = diskGuidMap.begin();
            for (; diskIter != diskGuidMap.end(); diskIter++)
            {
                if (strVol.compare(diskIter->second) == 0)
                {
                    wstring strGuid = diskIter->first;
                    dwVolumeGUIDSize = strGuid.size() * sizeof(WCHAR);
                    inm_wmemcpy_s(diskGuid, GUID_SIZE_IN_CHARS, strGuid.c_str(), strGuid.size());
                    break;
                }
            }

            if (diskIter == diskGuidMap.end())
            {
                bRet = false;
            }
            else if (!CheckDriverStat(ghControlDevice, diskGuid, &DriverWriteOrderState, &dwLastError, dwVolumeGUIDSize))
            {
                bRet = false;
                DebugPrintf("\nWarning:This disk %s is not protected @Time: %s \n", strVol.c_str(), GetLocalSystemTime().c_str());
            }
            
            if (diskGuid)
            {
                delete diskGuid;
            }

    } while (0);

    return bRet;
}


bool IsItValidDiskName(const char *srcDiskName)
{
    boost::mutex::scoped_lock guard(g_diskInfoMutex);

    bool bRet = false;

    std::map<wstring, string>::iterator diskIter = gDiskGuidMap.begin();
    for (; diskIter != gDiskGuidMap.end(); diskIter++)
    {
        string diskName = diskIter->second;
        if (boost::iequals(diskName, srcDiskName))
        {
            bRet = true;
            break;
        }
    }
    return bRet;
}

bool isBootableVolume(char volumeGUID[])
{
    bool bSRV = false;
    if (strlen(volumeGUID) > 2)
    {
        volumeGUID[2] = '.'; /// replace '?' with a  '.'
        volumeGUID[strlen(volumeGUID) - 1] = '\0';

        HANDLE hVol = INVALID_HANDLE_VALUE;
        hVol = CreateFile(volumeGUID, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
        if (hVol == INVALID_HANDLE_VALUE) // cannot open the drive
        {
            DebugPrintf("isBootableVolume(): CreateFile() failed.  Volume GUID %s . HRESULT = %08X\n", volumeGUID, HRESULT_FROM_WIN32(GetLastError()));
        }
        else
        {
            PARTITION_INFORMATION OutBuffer;
            DWORD lpBytesReturned = 0;

            if (DeviceIoControl((HANDLE)hVol,
                IOCTL_DISK_GET_PARTITION_INFO,
                (LPVOID)NULL,
                (DWORD)0,
                &OutBuffer,
                sizeof(PARTITION_INFORMATION),
                &lpBytesReturned,
                (LPOVERLAPPED)NULL) != 0)
            {
                if (OutBuffer.BootIndicator == TRUE)
                {
                    DebugPrintf("Volume %s is boot volume \n", volumeGUID);
                    bSRV = true;
                }
            }
            else
            {
                DebugPrintf("isBootableVolume(): DeviceIoControl() failed.  Volume GUID %s . HRESULT = %08X\n", volumeGUID, HRESULT_FROM_WIN32(GetLastError()));             
            }
            CloseHandle(hVol);
        }

        volumeGUID[2] = '?';
        volumeGUID[strlen(volumeGUID)] = '\\';
    }
    else
    {
        DebugPrintf("Invalid Volume GUID. It is too short. GUID: %s \n", volumeGUID);
    }

    return bSRV;
}

// get the bootable volume that are not mounted
HRESULT GetBootableVolumes(std::vector<std::string> &bootableVolumeGuids)
{
    HRESULT hr = S_OK;
    char volGuid[256] = { 0 };  // buffer to hold GUID of a volume.
    HANDLE pHandle = NULL;
    std::stringstream ss;

    pHandle = FindFirstVolume(volGuid, sizeof(volGuid) - 1);
    if (pHandle == INVALID_HANDLE_VALUE)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        ss << "FindFirstVolume failed";
        DebugPrintf("Error: %s hr = %08X.\n", ss.str().c_str(), hr);
        g_vacpLastError.m_errMsg = ss.str();
        g_vacpLastError.m_errorCode = hr;
        return hr;
    }

    do
    {
        LPTSTR VolumePathName = NULL;
        DWORD rlen = 0;
        bool ret = GetVolumePathNamesForVolumeName(volGuid, VolumePathName, 0, &rlen);

        if (ret)
        {
            hr = HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
            ss << "GetVolumePathNamesForVolumeName returned success when ERROR_MORE_DATA is expected";
            DebugPrintf("Error: %s hr = %08X.\n", ss.str().c_str(), hr);
            g_vacpLastError.m_errMsg = ss.str();
            g_vacpLastError.m_errorCode = hr;
            break;
        }

        if (GetLastError() == ERROR_MORE_DATA)
        {
            size_t pathnamelen;
            INM_SAFE_ARITHMETIC(pathnamelen = InmSafeInt<DWORD>::Type(rlen) * 2, INMAGE_EX(rlen))

            VolumePathName = (LPTSTR)malloc(pathnamelen);
            if (VolumePathName == NULL) 
            {
                hr = HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);
                ss << "GetBootableVolumes: memory allocation for " << pathnamelen << " bytes failed";
                DebugPrintf("Error: %s.\n", ss.str().c_str());
                g_vacpLastError.m_errMsg = ss.str();
                g_vacpLastError.m_errorCode = hr;
                break;
            }

            memset(VolumePathName, 0, pathnamelen);

            if (GetVolumePathNamesForVolumeName(volGuid, VolumePathName, pathnamelen, &rlen))
            {
                // select the volumes that are not mounted
                if (VolumePathName[0] == '\0')
                {
                    if (isBootableVolume(volGuid))
                    {
                        bootableVolumeGuids.push_back(volGuid);
                    }
                }
            }
            else
            {
                ss << "GetVolumePathNamesForVolumeName failed";
                DebugPrintf("Ignoring Error: %s hr = %08X.\n", ss.str().c_str(), hr);
            }
        }
        else
        {
            ss << "GetVolumePathNamesForVolumeName failed";
            DebugPrintf("Ignoring error: %s hr = %08X.\n", ss.str().c_str(), hr);
        }
    } while (FindNextVolume(pHandle, volGuid, sizeof(volGuid) - 1));

    if (!FindVolumeClose(pHandle))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        ss << "FindVolumeClose failed";
        DebugPrintf("Error: %s hr = %08X.\n", ss.str().c_str(), hr);
        g_vacpLastError.m_errMsg = ss.str();
        g_vacpLastError.m_errorCode = hr;
    }

    DebugPrintf("Returning hr = %08X.\n", hr);

    return hr;
}

HRESULT GetDiskNamesForVolume(const char *volName, vector<string> &diskNames)
{
    HRESULT hr = S_OK;

    HANDLE hVol;
    
    std::stringstream ss;

    hr = OpenVolumeExtended(&hVol, volName, GENERIC_READ);
    if (FAILED(hr) || (INVALID_HANDLE_VALUE == hVol))
    {
        ss << "OpenVolumeExtended() Failed for volume " << volName;
        DebugPrintf("%s. Error= %d\n", ss.str().c_str(), hr);
        g_vacpLastError.m_errMsg = ss.str();
        g_vacpLastError.m_errorCode = hr;
        return hr;
    }
    
    int EXPECTEDEXTENTS = 1;
    bool bexit = false;
    VOLUME_DISK_EXTENTS *vde = NULL;
    unsigned int volumediskextentssize;
    DWORD bytesreturned;
    DWORD err;

    do
    {
        if (vde)
        {
            free(vde);
            vde = NULL;
        }

        int nde;
        INM_SAFE_ARITHMETIC(nde = InmSafeInt<int>::Type(EXPECTEDEXTENTS) - 1, INMAGE_EX(EXPECTEDEXTENTS))
        INM_SAFE_ARITHMETIC(volumediskextentssize = sizeof (VOLUME_DISK_EXTENTS)+(nde * InmSafeInt<size_t>::Type(sizeof (DISK_EXTENT))), INMAGE_EX(sizeof (VOLUME_DISK_EXTENTS))(nde)(sizeof (DISK_EXTENT)))
        vde = (VOLUME_DISK_EXTENTS *)calloc(1, volumediskextentssize);

        if (NULL == vde)
        {
            hr = S_FALSE;
            ss << "Failed to allocate " << volumediskextentssize << " bytes for " << EXPECTEDEXTENTS << " expected extents, for IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS";
            DebugPrintf("%s\n", ss.str().c_str());
            g_vacpLastError.m_errMsg = ss.str();
            g_vacpLastError.m_errorCode = hr;
            break;
        }

        bytesreturned = 0;
        err = 0;
        string diskname;
        if (DeviceIoControl(hVol, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, vde, volumediskextentssize, &bytesreturned, NULL))
        {
            for (DWORD i = 0; i < vde->NumberOfDiskExtents; i++)
            {
                DISK_EXTENT &de = vde->Extents[i];
                std::stringstream ssdiskindex;
                ssdiskindex << de.DiskNumber;
                diskname = "\\\\.\\PHYSICALDRIVE";
                diskname += ssdiskindex.str();

                BOOLEAN     isStorageSpaceDisk = FALSE;

                // Check if it is a storage space disk
                // For storage space disks include all storage pool disks
                for (auto diskIter : g_StorageSpaceDiskIds) {
                    if (boost::iequals(diskIter.second, diskname)) {
                        isStorageSpaceDisk = TRUE;
                        DebugPrintf("Volume %s is a storage space volume\n", volName);
                        inm_printf("For volume %s found storage space disk %s\n", volName, diskIter.second.c_str());
                        for (auto pooldiskIter : g_StoragePoolDiskIds) {
                            if (std::find(diskNames.begin(), diskNames.end(), pooldiskIter.second) == diskNames.end()) {
                                diskNames.push_back(pooldiskIter.second);
                                inm_printf("For volume %s found storage pool disk %s\n", volName, pooldiskIter.second.c_str());
                            }
                        }
                        break;
                    }
                }

                if (!isStorageSpaceDisk) {
                    if (std::find(diskNames.begin(), diskNames.end(), diskname) == diskNames.end()) {
                        diskNames.push_back(diskname);
                        inm_printf("For volume %s found disk %s.\n", volName, diskname.c_str());

                    }
                }
            }
            bexit = true;
        }
        else if ((err = GetLastError()) == ERROR_MORE_DATA)
        {
            XDebugPrintf("with EXPECTEDEXTENTS = %d, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS says error more data\n", EXPECTEDEXTENTS);
            EXPECTEDEXTENTS = vde->NumberOfDiskExtents;
        }
        else
        {
            ss << "IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed on volume " << volName << " with error " << err;
            DebugPrintf("%s\n", ss.str().c_str());
            hr = S_FALSE;
            g_vacpLastError.m_errMsg = ss.str();
            g_vacpLastError.m_errorCode = hr;
            bexit = true;
        }
    } while (!bexit);

    if (vde)
    {
        free(vde);
        vde = NULL;
    }


    CloseHandle(hVol);
    return hr;
}

HRESULT GetDiskInfo()
{
    boost::mutex::scoped_lock guard(g_diskInfoMutex);

    HRESULT hr = S_OK;
    std::string errmsg;

    MsftPhysicalDiskRecordProcessor    msftPhysicalDiskProcessor;
    Win32DiskDriveRecordProcessor      diskDriveRecordProcessor;

    WmiHelper                           wmiHelper;

    std::set<std::string>              diskNameSet;
    std::set<std::string>              dynamicDiskNames;
    std::set<std::string>              storagePoolDiskNames;
    std::set<std::string>              storageSpaceDiskNames;
    std::set<std::string>              win32DiskNames;

    std::set<std::string>              nativeStoragePoolDiskNames;
    std::set<std::string>              nativeStorageSpaceDiskNames;
    std::set<std::string>              nativeDiskNames;
    std::set<ULONG>                     diskIndices;

    diskIndices = GetAvailableDiskIndices(MAXIMUM_DISK_INDEX, MAX_CONS_MISSING_INDICES);

    DiskIndexEnumerator                 diskIndexEnumerator(diskIndices);

    hr = wmiHelper.Init();
    if (FAILED(hr)) {
        DebugPrintf(wmiHelper.GetErrorMessage().c_str());
        ADD_TO_VACP_FAIL_MSGS(VACP_DISK_DISCOVERY_FAILED, wmiHelper.GetErrorMessage(), hr);
        return VACP_DISK_DISCOVERY_FAILED;
    }

    if (IsWindows8OrGreater()) {
        // Query Physical Disks
        hr = wmiHelper.GetData(L"Root\\Microsoft\\Windows\\Storage",
            L"MSFT_PhysicalDisk",
            msftPhysicalDiskProcessor);

        if (FAILED(hr)) {
            DebugPrintf(wmiHelper.GetErrorMessage().c_str());
            ADD_TO_VACP_FAIL_MSGS(VACP_DISK_DISCOVERY_FAILED, wmiHelper.GetErrorMessage(), hr);
            return VACP_DISK_DISCOVERY_FAILED;
        }

        diskNameSet = msftPhysicalDiskProcessor.GetDiskNames();
        storagePoolDiskNames = msftPhysicalDiskProcessor.GetStoragePoolDisks();
    }

    hr = wmiHelper.GetData(L"ROOT\\cimv2",
        L"Win32_DiskDrive",
        diskDriveRecordProcessor);
    if (FAILED(hr)) {
        DebugPrintf(wmiHelper.GetErrorMessage().c_str());
        ADD_TO_VACP_FAIL_MSGS(VACP_DISK_DISCOVERY_FAILED, wmiHelper.GetErrorMessage(), hr);
        return VACP_DISK_DISCOVERY_FAILED;
    }

    win32DiskNames = diskDriveRecordProcessor.GetDiskNames();
    diskNameSet.insert(win32DiskNames.begin(), win32DiskNames.end());

    if (IsWindows8OrGreater()) {
        // Query Storage Space Disk
        storageSpaceDiskNames = diskDriveRecordProcessor.GetStorageSpaceDisks();
        diskNameSet.insert(storageSpaceDiskNames.begin(), storageSpaceDiskNames.end());
    }

    nativeDiskNames = diskIndexEnumerator.GetDisks();
    diskNameSet.insert(nativeDiskNames.begin(), nativeDiskNames.end());

    if (IsWindows8OrGreater()) {
        nativeStoragePoolDiskNames = diskIndexEnumerator.GetStoragePoolDisks();
        nativeStorageSpaceDiskNames = diskIndexEnumerator.GetStorageSpaceDisks();

        storagePoolDiskNames.insert(nativeStoragePoolDiskNames.begin(), nativeStoragePoolDiskNames.end());
        storageSpaceDiskNames.insert(nativeStorageSpaceDiskNames.begin(), nativeStorageSpaceDiskNames.end());
    }

    if (!storagePoolDiskNames.empty()) {
        DebugPrintf("\nStoragePoolDisks: \n");
        for (auto it : storagePoolDiskNames) {
            DebugPrintf(" %s", it.c_str());
        }
    }

    if (!storageSpaceDiskNames.empty()) {
        DebugPrintf("\nStorageSpacelDisks: \n");
        for (auto it : storageSpaceDiskNames) {
            DebugPrintf(" %s", it.c_str());
        }
    }

    DebugPrintf("\nAllDisks: \n");
    for (auto it : diskNameSet) {
        DebugPrintf(" %s", it.c_str());
    }
    DebugPrintf("\n");

    // clear any previous discovery info as this call can be called
    // multiple times. Any disk that is not discovered should not be
    // in below maps.
    gDiskGuidMap.clear();
    g_DynamicDiskIds.clear();
    g_StorageSpaceDiskIds.clear();
    g_StoragePoolDiskIds.clear();

    for (auto diskName : diskNameSet)
    {
        DWORD               dwErr;
        DiskDeviceStream    diskDeviceStream(diskName);
        std::wstring        wDeviceId;

        dwErr = diskDeviceStream.Open();
        if (ERROR_SUCCESS != dwErr) {
            DebugPrintf(diskDeviceStream.GetErrorMessage().c_str());
            continue;
        }

        dwErr = diskDeviceStream.GetDeviceId(wDeviceId);
        if (ERROR_SUCCESS != dwErr) {
            DebugPrintf(diskDeviceStream.GetErrorMessage().c_str());
            ADD_TO_VACP_FAIL_MSGS(VACP_DISK_DISCOVERY_FAILED, wmiHelper.GetErrorMessage(), HRESULT_FROM_WIN32(dwErr));
            hr = VACP_DISK_DISCOVERY_FAILED;
            break;
        }

        if (!storageSpaceDiskNames.empty() &&
            (storageSpaceDiskNames.find(diskName) != storageSpaceDiskNames.end())) {
            g_StorageSpaceDiskIds.insert(std::make_pair(wDeviceId, diskName));
            continue;
        }

        gDiskGuidMap.insert(std::make_pair(wDeviceId, diskName));
        if (diskDeviceStream.IsDynamic()) {
            g_DynamicDiskIds.insert(std::make_pair(wDeviceId, diskName));
        }

        if (storagePoolDiskNames.find(diskName) != storagePoolDiskNames.end()) {
            g_StoragePoolDiskIds.insert(std::make_pair(wDeviceId, diskName));
        }
    }

    return hr;
}


HRESULT getAgentInstallPath(string &strInstallPath)
{
    const char VX_AGENT_REG_KEY[] = "SOFTWARE\\SV Systems\\VxAgent";

    char lszValue[MAX_REG_KEY_SIZE_IN_CHARS + 1] = { 0 };
    LONG lRet, lEnumRet;
    HKEY hKey;
    DWORD dwType = REG_SZ;
    DWORD dwSize = MAX_REG_KEY_SIZE_IN_CHARS;

    lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, VX_AGENT_REG_KEY, 0L, KEY_READ| KEY_WOW64_32KEY, &hKey);
    if (lRet != ERROR_SUCCESS)
    {
        DebugPrintf("Failed to open VX Agent's registry keys. RegOpenKeyEx returned %ld\n", lRet);
        return E_FAIL;
    }
    lEnumRet = RegQueryValueEx(hKey, "InstallDirectory", NULL, &dwType, (LPBYTE)&lszValue, &dwSize);
    if (lEnumRet == ERROR_SUCCESS)
    {
        strInstallPath = lszValue;
    }
    else
    {
        DebugPrintf("Failed to fetch the InstallDirectory value for VX agent. RegQueryValueEx returned %ld\n", lEnumRet);
        RegCloseKey(hKey);
        return E_FAIL;
    }

    if (strInstallPath.empty())
    {
        DebugPrintf("InstallDirectory value for VX agent 32-bit registry keys at HKLM\\%s  is empty.\n",
            VX_AGENT_REG_KEY);
    }

    RegCloseKey(hKey);
    return S_OK;
}

std::string convertWstringToUtf8(const std::wstring &wstr)
{
    if (wstr.empty())
        return std::string("");

    const int reqSize = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)&wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strResult(reqSize, 0);
    int ret = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)&wstr[0], (int)wstr.length(), (LPSTR)&strResult[0], strResult.size(), NULL, NULL);
    if (ret == 0)
    {
        DWORD err = GetLastError();
        std::stringstream sserr;
        sserr << "convertWstringToUtf8: WideCharToMultiByte Failed. Error: " << err;
        throw std::exception(sserr.str().c_str());
    }

    return strResult;
}

std::wstring convertUtf8ToWstring(const std::string &str)
{
    if (str.empty())
        return std::wstring(L"");

    const int reqSize = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)&str[0], (int)str.size(), NULL, 0);
    std::wstring wstrResult(reqSize, 0);
    int ret = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)&str[0], (int)str.length(), (LPWSTR)&wstrResult[0], wstrResult.size());
    if (ret == 0)
    {
        DWORD err = GetLastError();
        std::stringstream sserr;
        sserr << "convertWstringToUtf8: MultiByteToWideChar Failed. Error: " << err;
        throw std::exception(sserr.str().c_str());
    }
    return wstrResult;
}

const std::string GetHostName()
{
    std::wstring wszTemp(MAXHOSTNAMELEN + 1, 0);
    size_t wszTempLen = wszTemp.size();
    if (0 == GetComputerNameW((LPWSTR)&wszTemp[0], LPDWORD(&wszTempLen)))
    {
        DWORD err = GetLastError();
        std::stringstream sserr;
        sserr << "GetHostName: GetComputerNameW Failed. Error: " << err;
        throw std::exception(sserr.str().c_str());
    }
    return convertWstringToUtf8(wszTemp.c_str());
}

void GetIPAddressInAddrInfo(const std::string &hostName, strset_t &ipsInAddrInfo, std::string &errMsg)
{
    DebugPrintf("ENTERED GetIPAddressInAddrInfo: hostName=%s\n", hostName.c_str());

    try
    {
        if (!hostName.empty())
        {
            WSADATA wsaData;
            int iretv;

            iretv = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (iretv != 0)
            {
                std::stringstream sserr;
                sserr << "WSAStartup failed. Error: ";
                sserr << iretv;
                errMsg += sserr.str();
                errMsg += ", ";
                return;
            }

            addrinfoW *paddInfo = NULL;
            DWORD dwretv = GetAddrInfoW(convertUtf8ToWstring(hostName).c_str(), NULL, NULL, &paddInfo);
            if (0 == dwretv)
            {
                LPSOCKADDR socaddr_ip = NULL;
                for (addrinfoW *ptmp = paddInfo; ptmp != NULL; ptmp = ptmp->ai_next)
                {
                    if (ptmp->ai_family == AF_INET)
                    {
                        socaddr_ip = (LPSOCKADDR)ptmp->ai_addr;

                        std::wstring ipwsBuffer(MAXIPV4ADDRESSLEN + 1, 0);
                        DWORD ipBuffLen = ipwsBuffer.size();
                        iretv = WSAAddressToStringW(socaddr_ip, (DWORD)ptmp->ai_addrlen, NULL, (LPWSTR)&ipwsBuffer[0], &ipBuffLen);
                        if (0 != iretv){
                            std::stringstream sserr;
                            iretv = WSAGetLastError();
                            sserr << "WSAAddressToStringW failed. Error: ";
                            sserr << iretv;
                            errMsg += sserr.str();
                            errMsg += ", ";
                        }

                        std::string err;
                        std::string ipAddr = ValidateAndGetIPAddress(convertWstringToUtf8(ipwsBuffer.c_str()), err);
                        if (!ipAddr.empty())
                        {
                            ipsInAddrInfo.insert(ipAddr);
                        }
                        else
                        {
                            errMsg += err;
                            errMsg += ", ";
                        }
                    }
                }
                if (NULL != paddInfo)
                {
                    FreeAddrInfoW(paddInfo);
                }
            }
            else
            {
                iretv = WSAGetLastError();
                std::stringstream sserr;
                sserr << "GetAddrInfoW failed. Error: ";
                sserr << iretv;
                errMsg += sserr.str();
                errMsg += ", ";
            }
        }
        else
        {
            errMsg += "HostName is blank.";
        }
    }
    catch (std::exception exc)
    {
        errMsg += exc.what();
    }
    WSACleanup();

    DebugPrintf("EXITED GetIPAddressInAddrInfo\n");
}

// ValidateAndGetIPAddress chacks input ipAddress(es) for an IPv4 private address space format.
// On Windows input is comma saparated IP addresses. For example,
//          10.150.2.120,fe80::6d35:68a9:f2f9:956a,2404:f801:4800:2:6d35:68a9:f2f9:956a
// On Linux input is single IP addresses. For example,
//          10.150.2.135
// It checks input IP address(es) and returns an ipAddress string in IPv4 private address space format.
// If check fails it returns a blank string.
std::string ValidateAndGetIPAddress(const std::string &ipAddress, std::string &errMsg)
{
    DebugPrintf("ENTERED ValidateAndGetIPAddress: ipAddress=%s\n", ipAddress.c_str());
    std::string validIPAddress;

    try{
        typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;
        boost::char_separator<char> ipSep(",");
        tokenizer_t ipTokens(ipAddress, ipSep);
        tokenizer_t::iterator ipItr(ipTokens.begin());
        for (ipItr; ipItr != ipTokens.end(); ++ipItr)
        {
            if (!boost::asio::ip::address::from_string(*ipItr).is_v4())
            {
                errMsg = *ipItr;
                errMsg += " not in IPv4 format";
                continue;
            }

            if (boost::asio::ip::address::from_string(*ipItr).is_loopback())
            {
                errMsg = *ipItr;
                errMsg += " not in IPv4 private address space format";
                continue;
            }

            // check APIPA format
            const std::string strAPIPA_IPAddress = "169.254.";
            if (0 == strAPIPA_IPAddress.compare((*ipItr).substr(0, strAPIPA_IPAddress.length())))
            {
                errMsg = *ipItr;
                errMsg += " not in IPv4 private address space format";
                continue;
            }
            validIPAddress = *ipItr;
            break;
        }
    }
    catch (const std::exception &e){
        errMsg += "caught exception: ";
        errMsg += e.what();
    }
    catch (...){
        errMsg += " caught unknown exception in ValidateAndGetIPAddress";
    }

    DebugPrintf("EXITED ValidateAndGetIPAddress: validIPAddress=%s\n", validIPAddress.c_str());
    return validIPAddress;
}

HRESULT GetLocalIpFromWmi(std::vector<std::string> &ipAddress)
{
    DebugPrintf("ENTERED GetLocalIpFromWmi\n");
    HRESULT                             hr = S_OK;
    WmiHelper                           wmiHelper;
    Win32NetworkAdapterConfigProcessor  nwAdapConfigProcessor;

    hr = wmiHelper.Init();
    if (FAILED(hr))
    {
        DebugPrintf("Com Initialization failed. Error Code %0x\n", hr);
        DebugPrintf("%s Error Code %0x\n", wmiHelper.GetErrorMessage(), hr);
        return hr;
    }

    hr = wmiHelper.GetData(L"Root\\cimv2", L"Win32_NetworkAdapterConfiguration", nwAdapConfigProcessor);
    if (FAILED(hr)) {
        DebugPrintf("%s hr = %x\n", wmiHelper.GetErrorMessage(), hr);
        return hr;
    }

    ipAddress = nwAdapConfigProcessor.GetIPAddresses();
    return hr;
}