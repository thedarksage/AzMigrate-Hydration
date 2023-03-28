#include <sstream>
#include "InmsdkGlobals.h"
#include "inm_md5.h"
#include <boost/lexical_cast.hpp>
#include "inmageex.h"
#include <boost/algorithm/string/case_conv.hpp>
#include "inmsdkutil.h"
#include "portable.h"
#include "portablehelpers.h"
#include "localconfigurator.h"
#include "host.h"
#include <boost/filesystem/operations.hpp>
#include "../s2libs/foundation/file.h"
#include "confschemamgr.h"
#include "confengine/agentconfig.h"
#include "../inmsafecapis/inmsafecapis.h"

std::string getErrorMessage(INMAGE_ERROR_CODE errCode)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME); 
    std::string errMsg;
    switch(errCode)
    {
    case E_FUNCTION_NOT_EXECUTED:
		errMsg = "Operation not allowed. Do system level resume to continue";
        break;
    case E_UNKNOWN:
        errMsg = "Unknown Error";
        break;
    case E_INTERNAL:
        errMsg = "Internal server error";
        break;
    case E_FUNCTION_NOT_SUPPORTED:
        errMsg = "Function not supported";
        break;
    case E_AUTHENTICATION:
        errMsg = "Authentication failure";
        break;
    case E_AUTHORIZATION:
        errMsg = "Authorization failure";
        break;
    case E_FORMAT:
        errMsg = "Format error";
        break;
    case E_SUCCESS:
        errMsg = "Success";
        break;
    case E_INSUFFICIENT_PARAMS:
        errMsg = "Insufficient parameters";
        break;
    case E_WRONG_PARAM:
        errMsg = "Wrong parameter";
        break;
    case E_NO_DATA_FOUND:
        errMsg = "No data found";
        break;
    case E_UNKNOWN_FUNCTION:
        errMsg = "Unknown Function";
        break;
        //============>Host
    case E_NO_AGENT_WITH_GIUD:
        errMsg = "No Agent found with the GUID";
        break;
    case E_HOST_UNDER_PRETECTION:
        errMsg = "The Host is already under protection";
        break;
    case E_NO_HOST_WITH_THE_NAME:
        errMsg = "No host found with the name";
        break;
    case E_NO_HOST_WITH_THE_IP:
        errMsg = "No host  found with the IP";
        break;
    case E_NO_HOSTS_FOUND:
        errMsg = "No hosts found";
        break;
    case E_NO_HOST_WITH_THE_GUID:
        errMsg = "No host found with the system GUID";
        break;
    case E_HOST_NOT_UNDER_PROTECTION:
        errMsg = "The Host is not under protection";
        break;
    case E_SERVICE_RUNNING :
        errMsg = "Service still running.";
        break;
        //=============>Volume
    case E_ENUMARATE_VOLUMES:
        errMsg = "Unable to enumerate the volumes";
        break;
    case E_BOOT_VOLUME_CANNOT_EXCLUDE:
        errMsg = "Boot volume cannot excluded";
        break;
    case E_VOLUME_PROTECTION_INTACT:
        errMsg = "Volume Protection is intact or Initial sync is in progress No Need to resume protection";
        break;
    case E_NO_VOLUME_WITH_THE_NAME:
        errMsg = "No Volume found with the name";
        break;
    case E_VOLUME_NOT_PROTECTED:
        errMsg = "The volume is not protected or deletion is in progress";
        break;
    case E_VOLUME_NOT_ELIGIBLE_DUE_TO_LESS_CAPACITY:
        errMsg = "The Volume is not eligible due to less capacity" ;
        break ;
    case E_SYSTEM_VOLUME_CANNOT_EXCLUDE :
        errMsg = "System Reserved Partition cannot be excluded" ;
        break ;
    case E_EXCLUDED_VOLUME_DOESNT_EXIST_ON_SYSTEM :
        errMsg = "Excluded Volume is not found on the system" ;
        break ;
    case E_VOLUME_ALREADY_PAUSED:
        errMsg = "The Volume is already paused or pause in progress" ;
        break ;
    case E_VOLUME_ALREADY_PAUSE_TRACKED :
        errMsg = "The Volume is already pause tracked or pause tracking in progress" ;
        break ;
    case E_VOLUME_TRACKING_NOT_PAUSED :
        errMsg = "Volume Tracking is not paused" ;
        break ;
    case E_PHYSICALSNAPSHOT_TARGET_SAME_AS_SOURCE_VOLUME :
        errMsg = "The target volume for physical snapshot is same as source volume" ;
        break ;
    case E_PHYSICALSNAPSHOT_TARGET_IS_PROTECTED_VOLUME:
        errMsg = "The target volume for physical snapshot is a protected volume" ;
        break ;
    case E_PHYSICALSNAPSHOT_TARGET_IS_INUSE:
        errMsg = "The target volume for physical snapshot is being used by another physical snapshot request" ;
        break ;
    case E_NO_VOLUMES_FOUND_FOR_SYSTEM:
        errMsg = "No volume found for the system" ;
        break ;
    case E_MOUNT_POINT_ALREADY_INUSE:
        errMsg = "The mount point is already being used" ;
        break ;
    case E_MOUNT_POINT_BASE_VOLUME_DOESNT_EXIST:
        errMsg = "The Mount point base volume does not exist" ;
        break ;
    case E_MOUNT_POINT_BASE_IS_NON_PHYSICAL_DEVICE:
        errMsg = "The mount point’s base volume is not a physical device" ;
        break ;
    case E_VOLUME_STATE_UNRESUMABLE:
        errMsg = "Volume state is not resumeable" ;
        break ;
    case E_VOLUME_ALREADY_PROTECTED :
        errMsg = "Volume is already protected" ;
        break ;
    case E_VOLUME_NOT_OUTOFSYNC :
        errMsg = "The volume is not out of sync";
        break;
    case E_NO_VOLUME_ELIGIBLE_FOR_ISSUE_CONSISTENCY :
        errMsg = "No Volume is eligible for issuing consistency" ;
        break ;
    case E_REPOSITORY_VOLUME_CANNOTBE_PROTECTED:
        errMsg = "Can not be protected as this is a backup location volume" ;
        break ;
    case E_VOLUME_RAW :
        errMsg = "A Raw volume can not be protected" ;
        break ;
    case E_VOLUME_QUEUED_FOR_RESYNC :
        errMsg = "Volume is Scheduled for Resync";
        break ;
    case E_QUEUED_VOLUME_NOT_PROTECTED :
        errMsg = "Queued Volume is not Paused";
        break;
    case E_QUEUED_VOLUME_NOT_PAUSED :
        errMsg = "Queued Volume cannot be Paused";
        break;
    case E_VOLUME_RESIZED :
        errMsg = "Volume is Resized";
        break;
	case E_VOLUME_HAVING_MULTIPLE_ACCESS_POINTS :
		errMsg = "Same volume that is accessible via more than one mount point/drive letter is not supported" ;
		break ;
        //=============>Repository
    case E_REPO_VOL_HAS_INSUCCICIENT_STORAGE:
        errMsg = "Backup location does not have sufficient storage space";
        break;
    case E_NO_REPO_CONFIGERED:
        errMsg = "No Backup location configured";
        break;
    case E_NO_REPO_DEVICE_FOUND:
        errMsg = "No Backup location device found.";
        break;
    case E_REPO_NAME_ALREADY_EXIST:
        errMsg = "Specified backup location name already exist";
        break;
    case E_STORAGE_PATH_NOT_FOUND:
        errMsg = "Backup location not found";
        break;
    case E_NO_REPO_WITH_THE_NAME:
        errMsg = "No Backup location found with the given name";
        break;
    case E_REPO_ALREADY_CONFIGURED:
        errMsg = "Backup location is already configured" ;
        break ;
    case E_REPO_CREATION_INPROGRESS:
        errMsg = "Backup location creation is pending" ;
        break ;
    case E_REPO_UNSUPPORTED_SECTORSIZE :
        errMsg = "Choose another backup location that has 512 bytes as physical sector size for configuring backup location" ;
        break ;
        //==============>VM Metadata
    case E_NO_METADATA_FOUND:
        errMsg = "No Metadata found";
        break;
        //=============>Restore
    case E_NO_RESTORE_POINT:
        errMsg = "No restore points available";
        break;
    case E_NO_COMMON_CONS_PNT_FOR_ALL_VOLS:
        errMsg = "No common consistency point available for all the volumes";
        break;
    case E_NO_RECOVERY_WITH_COMMON_TIME_FOR_ALL_VOLS:
        errMsg = "No recovery with common time available for all the volumes";
        break;
    case E_NO_CONS_RESTORE_POINT:
        errMsg = "The chosen restore point is not available";
        break;
    case E_NO_RENT_TIME_RANGE_WITH_GUARANTEED_ACCURACY:
        errMsg = "No Retention time range available with guaranteed accuracy";
        break;
        //==============>Protection
    case E_NO_PROTECTIONS:
        errMsg = "No Protections Found";
        break;
    case E_PROTECTIONS_FOUND:
        errMsg = "Protections Found" ;
        break ;
    case E_DELETE_PROTECTION_INPROGRESS:
        errMsg = "Delete Protection is already in progress";
        break;
    case E_VOLPACK_PROVISION_FAIL:
        errMsg = "Target Volume Provisioning Failed";
        break;
    case E_SCENARIO_DELETION_INPROGRESS:
        errMsg = "Scenario Deletion is in Progress, Not able to add Volumes to the protection";
        break;
    case E_ADD_VOLUME_PROTECTION_INPROGRESS:
        errMsg = "AddVolume/DeleteBackup Protection is in Progress ";
        break;
        //==============>Recovery
    case E_RECOVERY_VOL_FAIL:
        errMsg = "Recovery of the volume is failed";
        break;
    case E_RECOVERY_INPROGRESS:
        errMsg = "Recovery is in progress";
        break;
        //==============>Snapshot
    case E_EXPORT_SNAPSHOT_FAIL:
        errMsg = "Exporting snapshot failed";
        break;
        //==============>MBR Info
    case E_REPO_NOT_AVAILABLE:
        errMsg = "Backup location is not accessible.";
        break;
    case E_NO_MBR_INFO:
        errMsg = "No MBR information found for the system";
        break ;
    case E_NO_VOLUME_OUTOFSYNC :
        errMsg = "No volume found out of sync" ;
        break ;
    case E_SYSTEM_PROTECTION_INTACT:
        errMsg = "System Protection is intact" ;
        break ;

    case E_SYSTEM_ALREADY_PAUSED:
        errMsg = "The system is already Paused" ;
        break ;
	case E_SYSTEM_RECOVERY_NOT_POSSIBLE:
		errMsg = "System recovery is not possible until the boot/system volume is protected. It can be protected later." ;
		break ;
    case E_SYSTEMBUSY:
        errMsg = "Configuration data is in inconsistent state. Refer to the recent events for further action." ;
        break ;
    case E_WRITE_CACHE_ENABLED:
        errMsg = "Write Cache should be disabled. " ;
        break ;
	case E_INVALID_CREDENTIALS:
        errMsg = "Unable to access the backup location due to login failure.. Please try with proper credentials" ;
        break ;
	case E_REPO_NORW_ACCESS:
        errMsg = "Insufficient privileges to access the backup location. Change the permission level to perform read/write.." ;
        break ;
	case E_UNKNOWN_CREDENTIALS :
		errMsg = "The credentials to access the backup location are not available." ;
		break ;
	case E_INVALID_REPOSITORY  :
		errMsg = "It is not a valid backup location" ;
		break ;
	case E_REPO_UNABLETOLOCK :
		errMsg = "Unable to synchronize the access to backup location" ;
		break ;
	case E_SAMEREPOSITORY_PROTECTEDVOL_DISK :
		errMsg = "" ;
		break ;
    default:
        errMsg = "Undefined error";
        break;
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME); 
	return errMsg.c_str();
}

std::string getStrErrCode(INM_ERROR errCode)
{
    std::stringstream ss;
    ss << errCode;
    return ss.str();
}

std::string strToUpper(const std::string& str)
{
    std::string tempStr = str;
    for (size_t i=0; i<tempStr.length(); i++)
    {
        tempStr[i] = toupper(tempStr[i]);
    }
    return tempStr;
}

std::string md5(const std::string& pttern)
{
    unsigned char computedHash[16];
    INM_MD5_CTX ctx;
    INM_MD5Init(&ctx);
    INM_MD5Update(&ctx, (unsigned char*)pttern.c_str(), pttern.size()) ;
    INM_MD5Final(computedHash, &ctx);
    char md5chksum[INM_MD5TEXTSIGLEN + 1];

    // Get the string representation of the checksum
    for (int i = 0; i < INM_MD5TEXTSIGLEN/2; i++) {
        inm_sprintf_s((md5chksum + i*2),(ARRAYSIZE(md5chksum)-(i*2)), "%02X", computedHash[i]);

    }
    return std::string(md5chksum);
}


std::string getVersion()
{
    return "1.0";
}
bool RemoveHostIDEntry()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool ok = false ;

#ifdef SV_WINDOWS
    WSADATA wsaData;
    int err = WSAStartup(MAKEWORD(1,1), &wsaData);
    if (err != 0) 
    {
        DebugPrintf(SV_LOG_DEBUG,"Could not find a usable WINSOCK DLL\n");
    }
#endif

    std::string smbConfFile ;
    ACE_Configuration_Heap m_inifile ;
	LocalConfigurator lc ;
	std::string repoLocation = lc.getRepositoryLocation();
#ifdef SV_WINDOWS
    if( repoLocation.length() == 1 )
    {
        repoLocation += ":" ;
    }
#endif
    if( repoLocation[repoLocation.length() - 1 ] != ACE_DIRECTORY_SEPARATOR_CHAR_A )
    {
        repoLocation += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
    }

    std::string repoConfigPath = repoLocation + "repository.ini" ;
    std::string repoConfigPathString = repoConfigPath;
	std::string localHostID = lc.getHostId();

    if( m_inifile.open() == 0 )
    {
        ACE_Ini_ImpExp importer( m_inifile );
		std::string errmsg;
		if( boost::filesystem::exists( repoConfigPath ) )
		{
			if ( importer.import_config(ACE_TEXT_CHAR_TO_TCHAR(repoConfigPath.c_str())) == 0)
			{
				ACE_Configuration_Section_Key sectionKey; 
				ACE_TString value;
				ACE_Configuration_Section_Key shareSectionKey;
				int sectionIndex = 0 ;
				ACE_TString tStrsectionName;

				while( m_inifile.enumerate_sections(m_inifile.root_section(), sectionIndex, tStrsectionName) == 0 )
				{
					m_inifile.open_section(m_inifile.root_section(), tStrsectionName.c_str(), 0, sectionKey) ;
					std::string hostname = ACE_TEXT_ALWAYS_CHAR(tStrsectionName.c_str());
					ACE_TString tStrValue;
					m_inifile.get_string_value(sectionKey, ACE_TEXT_CHAR_TO_TCHAR("hostid"), tStrValue) ; 
					std::string hostId = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());
					if (localHostID == hostId)
					{
						int errorCode = m_inifile.remove_section( m_inifile.root_section(), ACE_TEXT_CHAR_TO_TCHAR(hostname.c_str()), 1) ;
					}
					sectionIndex++; 
				}
				if( importer.export_config(ACE_TEXT_CHAR_TO_TCHAR(repoConfigPath.c_str())) ==  0 )
				{
					ok = true ;
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "failed to store the host details in the %s\n",repoConfigPath.c_str() ) ;
				}
				if ( SyncFile(repoConfigPath,errmsg) != true)
				{
					DebugPrintf (SV_LOG_ERROR, "Error : %s ",errmsg.c_str()); 
				}
				if( File::GetSizeOnDisk(repoConfigPath) == 0 )
				{
					ACE_OS::unlink( repoConfigPath.c_str()) ;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "failed to import %s\n",repoConfigPath.c_str() ) ;
			}
		}
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return ok ;
}
