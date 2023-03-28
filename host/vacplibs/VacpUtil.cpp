
#include <iostream>
#include <string>
#include <algorithm>
#include "StreamRecords.h"
#include "VacpUtil.h"
using namespace std;

bool VacpUtil::AppNameToTagType(const string & app, unsigned short & type)
{
    bool rv = true;

    do
    {
        string AppName = app;
        std::transform(AppName.begin(), AppName.end(), AppName.begin(), (int(*)(int)) toupper);

        if(AppName == "ORACLE") 
            type  = STREAM_REC_TYPE_ORACLE_TAG;
        else if(AppName == "SYSTEMFILES")
            type = STREAM_REC_TYPE_SYSTEMFILES_TAG;
        else if(AppName == "REGISTRY")
            type = STREAM_REC_TYPE_REGISTRY_TAG;
        else if(AppName == "SQL")
            type = STREAM_REC_TYPE_SQL_TAG;
        else if(AppName == "EVENTLOG")
            type  = STREAM_REC_TYPE_EVENTLOG_TAG;
        else if(AppName == "WMI")
            type  = STREAM_REC_TYPE_WMI_DATA_TAG;
        else if(AppName == "COM+REGDB")
            type = STREAM_REC_TYPE_COM_REGDB_TAG;
        else if(AppName == "FS")
            type = STREAM_REC_TYPE_FS_TAG;
        else if(AppName == "USERDEFINED")
            type = STREAM_REC_TYPE_USERDEFINED_EVENT;
        else if(AppName == "EXCHANGE")
            type  = STREAM_REC_TYPE_EXCHANGE_TAG;
        else if(AppName == "IISMETABASE")
            type = STREAM_REC_TYPE_IISMETABASE_TAG;
        else if(AppName == "CA")
            type = STREAM_REC_TYPE_CA_TAG;
        else if(AppName == "AD") 
            type   = STREAM_REC_TYPE_AD_TAG;
        else if(AppName == "DHCP")
            type   = STREAM_REC_TYPE_DHCP_TAG;
        else if(AppName == "BITS")
            type    = STREAM_REC_TYPE_BITS_TAG;
        else if(AppName == "WINS")
            type    = STREAM_REC_TYPE_WINS_TAG;
        else if(AppName == "RSM")
            type     = STREAM_REC_TYPE_RSM_TAG;
        else if(AppName == "TSL")
            type   = STREAM_REC_TYPE_TSL_TAG;
        else if(AppName == "FRS") 
            type     = STREAM_REC_TYPE_FRS_TAG;
        else if(AppName == "BOOTABLESTATE")
            type = STREAM_REC_TYPE_BS_TAG;
        else if(AppName == "SERVICESTATE")
            type = STREAM_REC_TYPE_SS_TAG;
        else if(AppName == "CLUSTERSERVICE")
            type  = STREAM_REC_TYPE_CLUSTER_TAG;
        else if(AppName == "SQL2005")
            type  = STREAM_REC_TYPE_SQL2005_TAG;
        else if(AppName == "EXCHANGEIS")
            type  = STREAM_REC_TYPE_EXCHANGEIS_TAG;
        else if(AppName == "EXCHANGEREPL")
            type  = STREAM_REC_TYPE_EXCHANGEREPL_TAG;
        else if(AppName == "SQL2008")
            type  = STREAM_REC_TYPE_SQL2008_TAG;
		else if(AppName == "SQL2012")
            type  = STREAM_REC_TYPE_SQL2012_TAG;
        else if(AppName == "SHAREPOINT")
            type  = STREAM_REC_TYPE_SHAREPOINT_TAG;
        else if(AppName == "OSEARCH")
            type  = STREAM_REC_TYPE_OSEARCH_TAG;
        else if(AppName == "SPSEARCH")
            type  = STREAM_REC_TYPE_SPSEARCH_TAG;
        else if(AppName == "HYPERV")
            type  = STREAM_REC_TYPE_HYPERV_TAG;
        else if(AppName == "ASR")
            type  = STREAM_REC_TYPE_ASR_TAG;
        else if(AppName == "SHADOWCOPYOPTIMIZATION")
            type  = STREAM_REC_TYPE_SC_OPTIMIZATION_TAG;
        else if(AppName == "MSSEARCH")
            type  = STREAM_REC_TYPE_MSSEARCH_TAG;
		else if(AppName == "INMAGETAGGUIDDUMMYWRITER")
            type  = STREAM_REC_TYPE_TAGGUID_TAG;
		else if(AppName == "TASKSCHEDULER")
            type  = STREAM_REC_TYPE_TASK_SCHEDULER_TAG;
		else if(AppName == "VSSMETADATASTORE")
            type  = STREAM_REC_TYPE_VSS_METADATA_STORE_TAG;
		else if(AppName == "PERFORMANCECOUNTER")
            type  = STREAM_REC_TYPE_PERFORMANCE_COUNTER_TAG;
		else if(AppName == "IISCONFIG")
            type  = STREAM_REC_TYPE_IIS_CONFIG_TAG;
		else if(AppName == "FSRM")
            type  = STREAM_REC_TYPE_FSRM_TAG;
		else if(AppName == "ADAM")
            type  = STREAM_REC_TYPE_ADAM_TAG;
		else if(AppName == "CLUSTERDB")
            type  = STREAM_REC_TYPE_CLUSTER_DB_TAG;
		else if(AppName == "NPS")
            type  = STREAM_REC_TYPE_NPS_TAG;
		else if(AppName == "TSG")
            type  = STREAM_REC_TYPE_TSG_TAG;
		else if(AppName == "DFSR")
            type  = STREAM_REC_TYPE_DFSR_TAG;
		else if(AppName == "VMWARE")
            type  = STREAM_REC_TYPE_VMWARESRV_TAG;
        else if(AppName == "TAGLIFETIME")
            type = STREAM_REC_TYPE_LIFETIME;
        else if(AppName == "SYSTEMLEVEL")
            type = STREAM_REC_TYPE_SYSTEMLEVEL;
		else if(AppName == "APPVSSWRITER")
			type = STREAM_REC_TYPE_DYNMICALLY_DISCOVERED_NEW_VSSWRITER;
        else if(AppName == "CRASH")
            type = STREAM_REC_TYPE_CRASH_TAG;
        else if (AppName == "BASELINE")
            type = STREAM_REC_TYPE_BASELINE_TAG;
        else if (AppName == "HYDRATION")
            type = STREAM_REC_TYPE_HYDRATION;
        else if (AppName == "CLUSTERINFO")
            type = STREAM_REC_TYPE_CLUSTER_INFO_TAG;
        else
        {
            

            cout << "Error detected  " << "in Function: " << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << "Invalid Application:" << AppName << "\n"
                << "Please Specify a valid application name\n\n";

            rv = false;
            break;
        }

    } while ( false );

    return rv;
}

bool VacpUtil::TagTypeToAppName(unsigned short TagType, string &AppName)
{
    bool rv = true;

    switch(TagType)
    {
    case STREAM_REC_TYPE_ORACLE_TAG:
        AppName = "ORACLE";
        break;
    case STREAM_REC_TYPE_SYSTEMFILES_TAG:
        AppName = "SystemFiles";
        break;
    case STREAM_REC_TYPE_REGISTRY_TAG:
        AppName = "Registry";
        break;
    case STREAM_REC_TYPE_SQL_TAG:
        AppName = "SQL";
        break;
    case STREAM_REC_TYPE_EVENTLOG_TAG:
        AppName = "EventLog";
        break;
    case STREAM_REC_TYPE_WMI_DATA_TAG:
        AppName = "WMI";
        break;
    case STREAM_REC_TYPE_COM_REGDB_TAG:
        AppName = "COM+REGDB";
        break;
    case STREAM_REC_TYPE_FS_TAG:
        AppName = "FS";
        break;
    case STREAM_REC_TYPE_USERDEFINED_EVENT:
        AppName = "USERDEFINED";
        break;
    case STREAM_REC_TYPE_EXCHANGE_TAG:
        AppName = "Exchange";
        break;
    case STREAM_REC_TYPE_IISMETABASE_TAG:
        AppName = "IISMETABASE";
        break;
    case STREAM_REC_TYPE_CA_TAG:
        AppName = "CA";
        break;
    case STREAM_REC_TYPE_AD_TAG:
        AppName = "AD";
        break;
    case STREAM_REC_TYPE_DHCP_TAG:
        AppName = "DHCP";
        break;
    case STREAM_REC_TYPE_BITS_TAG:
        AppName = "BITS";
        break;
    case STREAM_REC_TYPE_WINS_TAG:
        AppName = "WINS";
        break;
    case STREAM_REC_TYPE_RSM_TAG:
        AppName = "RSM";
        break;
    case STREAM_REC_TYPE_TSL_TAG:
        AppName = "TSL";
        break;
    case STREAM_REC_TYPE_FRS_TAG:
        AppName = "FRS";
        break;
    case STREAM_REC_TYPE_BS_TAG:
        AppName = "BootableState";
        break;
    case STREAM_REC_TYPE_SS_TAG:
        AppName = "ServiceState";
        break;
    case STREAM_REC_TYPE_CLUSTER_TAG:
        AppName = "ClusterService";
        break;
    case STREAM_REC_TYPE_SQL2005_TAG:
        AppName = "Sql2005";
        break;
    case STREAM_REC_TYPE_EXCHANGEIS_TAG:
        AppName = "ExchangeIS";
        break;
    case STREAM_REC_TYPE_EXCHANGEREPL_TAG:
        AppName = "ExchangeRepl";
        break;

    case STREAM_REC_TYPE_SQL2008_TAG:
        AppName = "SQL2008";
        break;
	
	case STREAM_REC_TYPE_SQL2012_TAG:
        AppName = "SQL2012";
        break;

    case STREAM_REC_TYPE_SHAREPOINT_TAG:
        AppName = "SharePoint";
        break;

    case STREAM_REC_TYPE_OSEARCH_TAG:
        AppName = "OSearch";
        break;

    case STREAM_REC_TYPE_SPSEARCH_TAG:
        AppName = "SPSearch";
        break;

    case STREAM_REC_TYPE_REVOCATION_TAG:
        AppName = "RevocationTag";
        break;

    case STREAM_REC_TYPE_HYPERV_TAG:
        AppName = "HyperV";
        break;

    case STREAM_REC_TYPE_ASR_TAG:
        AppName = "ASR";
        break;

    case STREAM_REC_TYPE_SC_OPTIMIZATION_TAG:
        AppName = "ShadowCopyOptimization";
        break;

    case STREAM_REC_TYPE_MSSEARCH_TAG:
        AppName = "MSSearch";
        break;
	
	case STREAM_REC_TYPE_TAGGUID_TAG:
        AppName = "InMageTagGuidDummyWriter";
        break;
	case STREAM_REC_TYPE_TASK_SCHEDULER_TAG:
        AppName = "TaskScheduler";
        break;
	case STREAM_REC_TYPE_VSS_METADATA_STORE_TAG:
        AppName = "VSSMetaDataStore";
        break;
	case STREAM_REC_TYPE_PERFORMANCE_COUNTER_TAG:
        AppName = "PerformanceCounter";
        break;
	case STREAM_REC_TYPE_IIS_CONFIG_TAG:
        AppName = "IISConfig";
        break;
	case STREAM_REC_TYPE_FSRM_TAG:
        AppName = "FSRM";
        break;
	case STREAM_REC_TYPE_ADAM_TAG:
        AppName = "ADAM";
        break;
	case STREAM_REC_TYPE_CLUSTER_DB_TAG:
        AppName = "ClusterDB";
        break;
	case STREAM_REC_TYPE_NPS_TAG:
        AppName = "NPS";
        break;
	case STREAM_REC_TYPE_TSG_TAG:
        AppName = "TSG";
        break;
	case STREAM_REC_TYPE_DFSR_TAG:
        AppName = "DFSR";
        break;
	case STREAM_REC_TYPE_VMWARESRV_TAG:
        AppName = "VMWARE";
        break;	
    case STREAM_REC_TYPE_LIFETIME:
        AppName = "TagLifeTime";
        break;
    case STREAM_REC_TYPE_SYSTEMLEVEL:
        AppName = "SystemLevel";
        break;
	case STREAM_REC_TYPE_DYNMICALLY_DISCOVERED_NEW_VSSWRITER:
		AppName = "APPVSSWRITER";
		break;
    case STREAM_REC_TYPE_CRASH_TAG:
        AppName = "CRASH";
		break;
    case STREAM_REC_TYPE_BASELINE_TAG:
        AppName = "BASELINE";
        break;
    case STREAM_REC_TYPE_HYDRATION:
        AppName = "HYDRATION";
        break;
    default:

        //cout << "Error detected  " << "in Function: " << FUNCTION_NAME 
        //    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n"
        //    << "Error: Invalid Record Type:" << TagType << "\n\n";

        rv = false;
        break;
    }

    return rv;
}

SV_USHORT VacpUtil::Swap(const SV_USHORT x)
{
        SV_USHORT svus = 1;
        SV_UCHAR isLendian = *(SV_UCHAR*)&svus;
        if(!isLendian)
		return ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8));
	return x;
}

SV_ULONG VacpUtil::Swap(const SV_ULONG x)
{
        SV_USHORT svus = 1;
        SV_UCHAR isLendian = *(SV_UCHAR*)&svus;
        if(!isLendian)
		return  ((x>>24) | ((x<<8) & 0x00FF0000) | ((x>>8) & 0x0000FF00) | (x<<24)) ;
	return x;
}
