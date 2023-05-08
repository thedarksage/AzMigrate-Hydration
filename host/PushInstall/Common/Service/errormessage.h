#ifndef INMAGE_PI_ERRORMESSAGE_H
#define INMAGE_PI_ERRORMESSAGE_H

#include <string>
#include <map>

namespace PI {

	enum INSTALLATION_EXIT_STATUS
	{
		AGENT_INSTALLER_FILE_NOT_FOUND = 1001, //EP0880
		AGENT_INSTALLATION_SUCCESS = 0,
		AGENT_INSTALLATION_INTERNAL_ERROR = 1, //EP0888
		AGENT_INSTALL_INVALID_PARAMS = 2, //EP0889
		AGENT_INSTALL_INVALID_COMMANDLINE_ARGUMENTS = 3, //EP0913
		AGENT_INSTALL_FATAL_ERROR = 4, //EP0870
		AGENT_INSTALL_PREPARE_STAGE_FAILED = 7, //EP0871
		AGENT_INSTALL_PREPARE_STATE_FAILED_REBOOTREQUIRED = 8, //EP0872
		AGENT_INSTALL_SPACE_NOT_AVAILABLE = 10, //EP0907
		AGENT_EXISTS_DIFF_VERSION = 11, //EP0864
		AGENT_EXISTS_SAME_VERSION = 12, // Job completed
		AGENT_EXISTS_REGISTEREDWITH_DIFF_CS = 13, //EP0867
		AGENT_INSTALL_INVALID_CS_IP_PORT_PASSPHRASE = 14,//EP0873
		AGENT_INSTALL_PREV_INSTALL_DIR_DELETE_FAILED = 15, //EP0882
		AGENT_INSTALL_DOTNET35_MISSING = 16, // EP0885
		AGENT_INSTALLED_REGISTER_FAILED = 17, // EP0886
		AGENT_INSTALL_VMWARE_TOOLS_NOT_RUNNING = 18, //EP0908
		AGENT_INSTALLER_ALREADY_RUNNING = 41, // EP0890
		AGENT_INSTALL_UNSUPPORTED_OS = 42, //EP0874
		AGENT_INSTALL_SERVICES_STOP_FAILED = 43, //EP0875
		AGENT_INSTALL_SERVICES_START_FAILED = 44, //EP0876
		AGENT_INSTALL_VSS_INSTALLATION_FAILED = 45, //EP0909
		AGENT_INSTALL_DRIVER_MANIFEST_INSTALLATION_FAILED = 46, //EP0910
		AGENT_INSTALL_DRIVER_ENTRY_REGISTRY_UPDATE_FAILED = 47, // EP0914
		AGENT_INSTALL_FAILED_TO_SET_SERVICE_STARTUP_TYPE = 48, //EP0945
		AGENT_INSTALL_VSS_INSTALLATION_FAILED_OUT_OF_MEMORY = 50, //EP0943
		AGENT_INSTALL_VSS_INSTALLATION_FAILED_DATABASE_LOCKED = 51, //EP0938
		AGENT_INSTALL_VSS_INSTALLATION_FAILED_SERVICE_ALREADY_EXISTS = 52, //EP0939
		AGENT_INSTALL_VSS_INSTALLATION_FAILED_SERVICE_MARKED_FOR_DELETION = 53, //EP0940
		AGENT_INSTALL_VSS_INSTALLATION_FAILED_CSSCRIPT_ACCESS_DENIED = 54, //EP0941
		AGENT_INSTALL_VSS_INSTALLATION_FAILED_PATH_NOT_FOUND = 55, //EP0942
		AGENT_INSTALL_FAILED_TO_CREATE_SYMLINKS = 56, // EP0953
		AGENT_INSTALL_FAILED_TO_INSTALL_AND_CONFIGURE_SERVICES = 57, // EP0954
		AGENT_INSTALL_PREV_INSTALL_REBOOT_PENDING = 62, //EP0883
		AGENT_INSTALL_FAILED_WITH_VSS_ISSUE = 63, // EP0887
		AGENT_INSTALL_BOOT_DRIVER_NOT_AVAILABLE = 64, // EP0900
		AGENT_INSTALL_FAILED_ON_UNIFIED_SETUP = 65, // EP0901
		AGENT_INSTALL_MASTER_TARGET_EXISTS = 75, //EP0911
		AGENT_INSTALL_PLATFORM_MISMATCH = 76, //EP0912
		AGENT_INSTALL_FAILED_OUT_OF_MEMORY = 90, // EP0967
		AGENT_INSTALL_HOST_NAME_NOT_FOUND = 91, //EP0918
		AGENT_INSTALLER_OS_MISMATCH = 92, // EP0891
		AGENT_INSTALL_THIRDPARTY_NOTICES_NOT_AVAILABLE = 93, //EP0922
		AGENT_INSTALL_LOCALE_NOT_AVAILABLE = 94, // EP0923
		AGENT_INSTALL_RPM_COMMAND_NOT_AVAILABLE = 95, // EP0924
		AGENT_INSTALL_MISSING_PARAMS = 96, // EP0892
		AGENT_SETUP_FAILED_WITH_ERRORS = 97, //EP0964
		AGENT_SETUP_COMPLETED_WITH_WARNINGS = 98, //EP0965
		AGENT_INSTALL_PREREQS_FAIL = 99, // EP0915
		AGENT_INSTALL_PASSPHRASE_NOT_FOUND = 102, // EP0894
		AGENT_INSTALL_CERT_GEN_FAIL = 110, //EP0936
		AGENT_INSTALL_DISABLE_SERVICE_FAIL = 111, //EP0916
		AGENT_INSTALL_LINUX_INSUFFICIENT_DISK_SPACE = 112, // EP0935
		AGENT_INSTALL_VX_SERVICE_STOP_FAIL = 115, // EP0895
		AGENT_INSTALL_FX_SERVICE_STOP_FAIL = 116, // EP0895
		AGENT_INSTALL_INSUFFICIENT_PRIVILEGE = 119, // EP0896
		AGENT_INSTALL_PREV_INSTALL_NOT_FOUND = 120, // EP0897
		AGENT_INSTALL_KERNEL_NOT_SUPPORTED = 145, // EP0898
		AGENT_INSTALL_LINUX_VERSION_NOT_SUPPORTED = 147, // EP0899
		AGENT_INSTALL_SLES_VERSION_NOT_SUPPORTED = 148, // EP0899
		AGENT_INSTALL_OPERATING_SYSTEM_NOT_SUPPORTED = 149, //EP0921
		AGENT_INSTALL_DRIVER_INSTALLATION_FAILED = 150, // EP0929
		AGENT_INSTALL_UPGRADE_PLATFORM_CHANGE_NOT_SUPPORTED = 151, //EP0917
		AGENT_INSTALL_CONFIG_DIRECTORY_CREATION_FAILED = 152, // EP0925
		AGENT_INSTALL_CONF_FILE_COPY_FAILED = 153, // EP0926
		AGENT_INSTALL_INSTALLATION_DIRECTORY_ABS_PATH_NEEDED = 156, //EP0919
		AGENT_INSTALL_TEMP_FOLDER_CREATION_FAILED = 158, // EP0928
		AGENT_INSTALL_RPM_INSTALLATION_FAILED = 159, // EP0927
		AGENT_INSTALL_INITRD_BACKUP_FAILED = 162, // EP0946
		AGENT_INSTALL_INITRD_RESTORE_FAILED = 163, // EP0947
		AGENT_INSTALL_INITRD_IMAGE_UPDATE_FAILED = 164, // EP0948
		AGENT_INSTALL_SNAPSHOT_DRIVER_IN_USE = 166, // EP0931
		AGENT_INSTALL_SNAPSHOT_DRIVER_UNLOAD_FAILED = 167, // EP0932
		AGENT_INSTALL_HOST_ENTRY_NOTFOUND = 174, // EP0877
		AGENT_INSTALL_INSUFFICIENT_SPACE_IN_ROOT = 176, //EP0906
		AGENT_INSTALL_PARTITION_SPACE_NOT_AVAILABLE = 177, // EP0920
		AGENT_INSTALL_INVALID_VX_INSTALLATION_OPTIONS = 179, // EP0933
		AGENT_INSTALL_CACHE_FOLDER_CREATION_FAILED = 180, // EP0930
		AGENT_INSTALL_INSUFFICIENT_DISKSPACE = 189, //EP0878
		AGENT_INSTALL_CREATE_TEMPDIR_FAILED = 195, //EP0879
		AGENT_INSTALL_MUTEX_ACQUISITION_FAILED = 1618, // EP0934
		AGENT_INSTALL_COMPLETED_BUT_REQUIRES_REBOOT = 1641, // EP0884
		AGENT_INSTALL_COMPLETED_BUT_RECOMMENDS_REBOOT = 3010, // EP0902
		AGENT_INSTALL_COMPLETED_BUT_RECOMMENDS_REBOOT_LINUX = 209, // EP0903
		AGENT_INSTALL_CURRENT_KERNEL_NOT_SUPPORTED = 208, // EP0904
		AGENT_INSTALL_LIS_COMPONENTS_NOT_AVAILABLE = 178, // EP0905
		AGENT_INSTALL_MSI_INSTALLATION_FAILED = 1603, // EP0944
		PI_BIOS_ID_MISMATCH = 1999, // EP0966 - bios id mismatch for vCenter discovered VMs
		//// PI_BIOS_ID_MISMATCH = 1999, // EP0970 - bios id mismatch for physical machines
		PI_INTERNAL_ERROR = 2001, // EP2001
		AGENT_INSTALL_EXIT_STATUS_FILE_NOT_FOUND = 2002, // EP2002
		AGENT_INSTALL_PREREQ_JSON_NOT_FOUND = 2003, //EP2003
		AGENT_SETUP_EXTENDED_ERRORS_JSON_NOT_FOUND = 2004, //EP2004
		PI_FQDN_MISMATCH = 2005 // EP2005
	};

#ifdef SV_WINDOWS

	extern const std::map<std::string, std::string> errcodeToErrname;

	inline std::string GetErrorNameForErrorcode(const std::string &errorcode)
	{
		std::map<std::string, std::string>::const_iterator keyIter = errcodeToErrname.find(errorcode);
		if (keyIter != errcodeToErrname.end())
		{
			return keyIter->second;
		}
		return std::string();
	}

	extern const std::map<std::string, std::string> errCodeTags;

	inline std::string GetTagsForErrorcode(const std::string &errorcode)
	{
		std::map<std::string, std::string>::const_iterator keyIter = errCodeTags.find(errorcode);
		if (keyIter != errCodeTags.end())
		{
			return keyIter->second;
		}
		return std::string();
	}

#endif

	extern const std::string psIp;
	extern const std::string psName;
	extern const std::string sourceIp;
	extern const std::string sourceName;
	extern const std::string csIp;
	extern const std::string csPort;
	extern const std::string operatingSystemName;
	extern const std::string pushLogPath;
	extern const std::string serviceName;
	extern const std::string errCode;
	extern const std::string vCenterIp;
	extern const std::string remoteUAPath;
	extern const std::string sourceOs;
	extern const std::string cspsConfigToolPath;
	extern const std::string installPath;
	extern const std::string passphrasePath;
	extern const std::string fwdLink;
	extern const std::string linkForMobInstall;
	extern const std::string vxServiceDisplayName;
	extern const std::string fxServiceDisplayName;
	extern const std::string supportedLinuxVersionLink;
	extern const std::string supportedDriversLink;

	inline std::string getPlaceHolderValue(std::map<std::string, std::string> placeHolders, const std::string &key)
	{
		std::map<std::string, std::string>::iterator keyIter = placeHolders.find(key);
		if (keyIter != placeHolders.end())
		{
			return keyIter->second;
		}
		return std::string();
	}

	inline std::string getCommonErrorMessageAndRecommendedAction(std::string errorCode)
	{
		int errCode = atoi(errorCode.c_str());
		if (errCode == AGENT_INSTALL_VX_SERVICE_STOP_FAIL)
		{
			return vxServiceDisplayName;
		}
		else if (errCode == AGENT_INSTALL_FX_SERVICE_STOP_FAIL)
		{
			return fxServiceDisplayName;
		}
	}

	inline std::string getRecommendedActionForServiceError(std::string errorCode)
	{
		int errCode = atoi(errorCode.c_str());
		if (errCode == AGENT_INSTALL_VX_SERVICE_STOP_FAIL)
		{
			return vxServiceDisplayName;
		}
		else if (errCode == AGENT_INSTALL_FX_SERVICE_STOP_FAIL)
		{
			return fxServiceDisplayName;
		}
	}

	inline std::string getErrorEP0865(std::map<std::string, std::string> placeHolders){

		std::string  errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0865.";
		errMessage += " Either the source machine is not running, or there are network";
		errMessage += " connectivity issues between the process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ")";
		errMessage += " and the source machine.\n";
		errMessage += "Ensure the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " is accessible from the process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ").\n";
		errMessage += "In addition, the following is needed for push installation to complete successfully:\n";
		errMessage += "1. Allow \"File and Printer Sharing\" and \"Windows Management Instrumentation (WMI)\" in the Windows Firewall.";
		errMessage += " Under Windows Firewall settings, select the option \"Allow an app or";
		errMessage += " feature through Firewall\". Select File and Printer Sharing and WMI";
		errMessage += " options for all profiles.\n";
		errMessage += "2. Ensure that the user account has administrator rights on the source machine.\n";
		errMessage += "3. Disable remote User Account Control(UAC) if you are using local administrator account to install mobility service.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete. See ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0866(std::map<std::string, std::string> placeHolders){

		std::string  errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0866.";
		errMessage += " Either the source machine is not running, or there are network";
		errMessage += " connectivity issues between the process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ")";
		errMessage += " and the source machine.\n";
		errMessage += "Ensure the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " is accessible from the process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ").\n";
		errMessage += "In addition, the following is needed for push installation to complete successfully:\n";
		errMessage += "1. Root credentials should be provided.\n";
		errMessage += "2. SSH and SFTP services should be running.\n";
		errMessage += "3. /etc/hosts on the source machine must contain entries for all IP addresses of the source machine.\n";
		errMessage += "4. Ensure the source machine Linux variant is supported.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete. See ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0860(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0860.";
		errMessage += " The process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ")";
		errMessage += " could not identify the Linux version of the source machine.\n";
		errMessage += "Ensure the following for push installation to complete successfully:\n";
		errMessage += "1. The source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " is accessible from the process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ").\n";
		errMessage += "2. Root credentials should be provided.\n";
		errMessage += "3. SSH and SFTP services should be running.\n";
		errMessage += "4. /etc/hosts on the source machine must contain entries for all IP addresses of the source machine.\n";
		errMessage += "5. Ensure the source machine Linux variant is supported.\n\n";
		errMessage += "Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete. See ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0851(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0851.";
		errMessage += " The configuration server ";
		errMessage += getPlaceHolderValue(placeHolders, csIp);
		errMessage += " is not accessible.\n";
		errMessage += "Ensure that the process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ")";
		errMessage += " and the configuration server ";
		errMessage += getPlaceHolderValue(placeHolders, csIp);
		errMessage += " are connected to the network, and configuration server is accessible from the process server.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all prerequisities for push installation of the mobility service are complete. See ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0852(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0852.";
		errMessage += " The configuration server ";
		errMessage += getPlaceHolderValue(placeHolders, csIp);
		errMessage += " is not accessible from the process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ")\n";
		errMessage += "Ensure that the process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ")\n";
		errMessage += " and the configuration server ";
		errMessage += getPlaceHolderValue(placeHolders, csIp);
		errMessage += " are connected to the network, and the configuration server is accessible from process server.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete. See ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0853(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0853.";
		errMessage += " Signature validation of the mobility service installer failed on the process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ").\n";
		errMessage += "Signature verification is a security measure and internet connectivity is required for the same.\n";
		errMessage += "1. Ensure that the process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ")";
		errMessage += " has internet connectivity.\n";
		errMessage += "2. Alternatively, the signature validation on the mobility service installer package can be disabled to install the mobility service without the signature validation.\n";
		errMessage += "   a. Run the cspsconfigtool.exe from the installation folder on the process server ";
		errMessage += getPlaceHolderValue(placeHolders, cspsConfigToolPath);
		errMessage += " to launch Microsoft Azure Site Recovery Process Server UI.\n";
		errMessage += "   b. Uncheck the \"Verify Mobility Service software signature before installing on source machines\" option and save.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete. See ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0856(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of Mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0856.";
		errMessage += " Either the source machine is not running, or there are network connectivity";
		errMessage += " issues betweeen process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ")";
		errMessage += " and the source machine.\n";
		errMessage += "1. Ensure the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " is accessible from the process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ")";
		errMessage += "2. Allow \"File and Printer Sharing\" in the Windows Firewall.";
		errMessage += " Under Windows Firewall settings, select the option \"Allow an app through Windows Firewall\"";
		errMessage += " and select File and Printer Sharing for all profiles.\n";
		errMessage += "In addition, the following is needed for push installation to complete successfully:\n";
		errMessage += "1. Allow \"Windows Management Instrumentation(WMI)\" in the Windows Firewall.";
		errMessage += " Under Windows Firewall settings, select the option \"Allow an app through Windows Firewall\"";
		errMessage += " and select WMI for all profiles.\n";
		errMessage += "2. Ensure that the user account has administrator rights on the source machine.\n";
		errMessage += "3. Disable remote User Account Control(UAC) if you are using local administrator account to install the mobility service.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete. See ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0857(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of Mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0857.";
		errMessage += " Either the source machine is not running, or there are network connectivity";
		errMessage += " issues betweeen process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ")";
		errMessage += " and the source machine.\n";
		errMessage += "1. Ensure the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " is accessible from the process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ")";
		errMessage += "2. Ensure SFTP service is running on the source machine.\n\n";
		errMessage += "In addition, the following is needed for push installation to complete successfully:\n";
		errMessage += "1. Root credentials should be provided.\n";
		errMessage += "2. SSH service should be running.\n";
		errMessage += "3. /etc/hosts on the source machine must contain entries for all IP addresses of the source machine.\n";
		errMessage += "4. Ensure the source machine Linux variant is supported.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete. See ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0854(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0854.";
		errMessage += " Either the source machine is not running, or there are network connectivity";
		errMessage += " issues between the process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ").\n\n";
		errMessage += "1. Ensure that the source machine is accessible from the process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ")\n";
		errMessage += "2. Allow \"Windows Management Instrumentation(WMI)\" in the Windows Firewall.";
		errMessage += " Under Windows Firewall settings, select the option \"Allow an app through Windows Firewall\"";
		errMessage += " and select WMI for all profiles.\n";
		errMessage += "In addition, the following is needed for push installation to complete successfully:\n";
		errMessage += "1. Allow \"File and Printer Sharing\" in the Windows Firewall.";
		errMessage += " Under Windows Firewall settings, select the option \"Allow an app through Windows Firewall\"";
		errMessage += " and select File and Printer Sharing for all profiles.\n";
		errMessage += "2. Ensure that the user account has administrator rights on the source machine.\n";
		errMessage += "3. Disable remote User Account Control(UAC) if you are using local administrator account to install mobility service.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete. See";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0855(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0855.";
		errMessage += " Either the source machine is not running, or there are network connectivity";
		errMessage += " issues between the process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ").\n\n";
		errMessage += "1. Ensure that the source machine is accessible from the process server ";
		errMessage += getPlaceHolderValue(placeHolders, psName);
		errMessage += "(";
		errMessage += getPlaceHolderValue(placeHolders, psIp);
		errMessage += ")\n";
		errMessage += "2. Ensure SSH service is running on the source machine.\n\n";
		errMessage += "In addition, the following is needed for push installation to complete successfully:\n";
		errMessage += "1. Root credentials should be provided.\n";
		errMessage += "2. SFTP service should be running.\n";
		errMessage += "3. /etc/hosts on the source machine must contain entries for all IP addresses of the source machine.\n";
		errMessage += "4. Ensure the source machine Linux variant is supported.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete. See";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}


	inline std::string getErrorEP0864(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0864.";
		errMessage += "\nA different version of mobility service is already installed on source machine. ";
		errMessage += ". To install the latest version of mobility service, manually ";
		errMessage += "uninstall the older version, disable protection and retry the operation. See";
		errMessage += fwdLink;
		errMessage += " for guidance on Push install.";

		return errMessage;
	}

	inline std::string getErrorEP0867(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0867.";
		errMessage += " The mobility service is already installed on the source machine";
		errMessage += " and registered to a different configuration server.\n";
		errMessage += "Uninstall the mobility service manually on the source machine,";
		errMessage += " disable protection and retry the operation.";
		errMessage += " See";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0870(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Installation of the mobility service to source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0870.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all ";
		errMessage += "prerequisites for push installation of the mobility service are complete.\n";
		errMessage += " See";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0871(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Installation of the mobility service to source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0871.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all ";
		errMessage += "prerequisites for push installation of the mobility service are complete.\n";
		errMessage += " See";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0872(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Installation of the mobility service to source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0872.";
		errMessage += " Reboot of the source machine is required.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all ";
		errMessage += "prerequisites for push installation of the mobility service are complete.\n";
		errMessage += " See";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0873(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Installation of the mobility service to source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0873.";
		errMessage += " Either the configuration server IP address or HTTPS port is invalid.\n";
		errMessage += "1. Ensure the configuration server IP address and HTTPS port number are correct.\n";
		errMessage += "2. Ensure the configuration server is accessible from source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " using the configuration server IP ";
		errMessage += getPlaceHolderValue(placeHolders, csIp);
		errMessage += " and HTTPS port ";
		errMessage += getPlaceHolderValue(placeHolders, csPort);
		errMessage += ".\n";
		errMessage += "Disable protection and retry the operation after ensuring that all ";
		errMessage += "prerequisites for push installation of the mobility service are complete.\n";
		errMessage += " See";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0874(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Installation of the mobility service to source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0874.";
		errMessage += " Operating System version on the source machine is not supported.\n";
		errMessage += "Ensure that the source machine OS version is supported\n";
		errMessage += "Disable protection and retry the operation after ensuring that all ";
		errMessage += "prerequisites for push installation of the mobility service are complete.\n";
		errMessage += " See";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0875(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Installation of the mobility service to source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0875.";
		errMessage += " Setup was unable to stop one or more required services";
		errMessage += "(Azure Site Recovery VSS Provider/InMage Scout Application Service/";
		errMessage += "InMage Scout FX Agent/InMage Scout VX Agent - Sentinel/Outpost/Cxprocessserver).";
		errMessage += "Please stop them manually and re-try installation.\n";
		errMessage += " See";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0876(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Installation of the mobility service to source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0876.";
		errMessage += " Setup was unable to start one or more required services";
		errMessage += "(Azure Site Recovery VSS Provider/InMage Scout Application Service/";
		errMessage += "InMage Scout FX Agent/InMage Scout VX Agent - Sentinel/Outpost/Cxprocessserver).";
		errMessage += "Please start them manually and re-try installation.\n";
		errMessage += " See";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0877(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Installation of the mobility service to source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0877.";
		errMessage += " There is no valid host entry in /etc/hosts file.\n";
		errMessage += "/etc/hosts file on the source machine must contain entries for all IP addresses of the source machine.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all ";
		errMessage += "prerequisites for push installation of the mobility service are complete.\n";
		errMessage += " See";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0878(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Installation of the mobility service to source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0878.";
		errMessage += " Not enough free space available on the disk.\n";
		errMessage += "Ensure at least 100 MB of space is available on the disk for installation.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all ";
		errMessage += "prerequisites for push installation of the mobility service are complete.\n";
		errMessage += " See";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0879(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Installation of the mobility service to source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0879.";
		errMessage += " Installer could not create temporary directory.\n";
		errMessage += "Ensure the user has administrator rights and write privileges to the system directory on the source machine.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all ";
		errMessage += "prerequisites for push installation of the mobility service are complete.\n";
		errMessage += " See";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0861(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0861.\n";
		errMessage += "Ensure network connectivity exists between the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " and the configuration server ";
		errMessage += getPlaceHolderValue(placeHolders, csIp);
		errMessage += "\nDisable protection and retry the operation after ensuring that all prerequisites for push installation";
		errMessage += " of the mobility service are complete. If the issue persists, contact support with logs located on source machine at ";
		errMessage += getPlaceHolderValue(placeHolders, pushLogPath);
		errMessage += "\nSee ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0862(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += "1. Ensure network connectivity exists between the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " and the configuration server ";
		errMessage += getPlaceHolderValue(placeHolders, csIp);
		errMessage += "\n2. /etc/hosts on the source machine must contain entries";
		errMessage += " for all IP addresses of the source machine.\n";
		errMessage += "\nDisable protection and retry the operation after ensuring that all prerequisites for push installation";
		errMessage += " of the mobility service are complete. If the issue persists, contact support with logs located at ";
		errMessage += getPlaceHolderValue(placeHolders, pushLogPath);
		errMessage += "\nSee ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0868(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Uninstallation of the mobility service failed on the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " with error code EP0868.";
		errMessage += " One or more of the relevant services failed to shut down.\n";
		errMessage += "Uninstall the mobility service manually on the source machine, disable protection";
		errMessage += " and retry the operation after ensuring that all the services pertaining to the mobility service ";
		errMessage += " are stopped.\n";

		return errMessage;
	}

	inline std::string getErrorEP0858(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0858.";
		errMessage += " Either the credentials provided to install mobility service is incorrect";
		errMessage += " or the user account has insufficient privileges.\n";
		errMessage += "Ensure the user credentials provided for the source machine are correct.\n";
		errMessage += "In addition, the following is needed for push installation to complete successfully: \n";
		errMessage += "1. Ensure that the user account has administrator rights on the source machine.\n";
		errMessage += "2. Disable remote User Account Control(UAC) if you are using local adminitrator account to install mobility service.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete. See ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0859(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0859.";
		errMessage += " Either the credentials provided to install mobility service is incorrect";
		errMessage += " or the user account has insufficient privileges.\n";
		errMessage += "Ensure that the root credentials are provided for the source machine.";
		errMessage += "Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete. See ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0880(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Installation of the mobility service failed on the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " with error code EP0880.";
		errMessage += " Mobility service installer file could not be found on the source machine at ";
		errMessage += getPlaceHolderValue(placeHolders, remoteUAPath);
		errMessage += "\nCertain anti-virus applications could delete files copied over";
		errMessage += " from network locations. If your anti-virus application on the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " has such a feature turned on, then stop the anti-virus service.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete.";
		errMessage += " If the issue persists, contact support with the logs located on the source machine at ";
		errMessage += getPlaceHolderValue(placeHolders, pushLogPath);
		errMessage += " See ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0881(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ". The Linux version ";
		errMessage += getPlaceHolderValue(placeHolders, sourceOs);
		errMessage += " of the source machine is unsupported.\nEnsure the following for push installation to complete successfully:\n";
		errMessage += "1. Root credentials should be provided.\n";
		errMessage += "2. SSH and SFTP services should be running.\n";
		errMessage += "3. /etc/hosts on the source machine must contain entries for all IP addresses of the source machine.\n";
		errMessage += "4. Ensure the source machine Linux variant is supported.\n\n";
		errMessage += "Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete. See ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0882(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ". Operation on the installation directory did not succeed.\n";
		errMessage += "Delete the installation directory ";
		errMessage += getPlaceHolderValue(placeHolders, installPath);
		errMessage += " on the source machine.\n";
		errMessage += "Disable protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete. See ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0883(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\n;. A system restart from a previous installation / update is pending.\n";
		errMessage += "Restart the source machine. Disable the protection and retry the operation after ensuring that all prerequisites for push installation of the mobility service are complete. See ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0884(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " completed with an error( ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ").\nSetup requires you to restart the server before replication can be started.\n";

		return errMessage;
	}

	inline std::string getErrorEP0885(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility service installation requires Microsoft .NET 3.5.1 or later as pre-requisite.\n";
		errMessage += "You can install it using Add Roles and Features in Server Manager. See ";
		errMessage += fwdLink;
		errMessage += " for push installation guidance.";

		return errMessage;
	}

	inline std::string getErrorEP0886(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service Installation could not be completed as the registration with the Configuration Server failed.\n";
		errMessage += "1. The Configuration Server is not reachable from the virtual machine.\n";
		errMessage += "2. The Configuration Server IP / Passphrase provided could be incorrect.\n";
		errMessage += "1. Ensure that the Configuration Server is reachable from the virtual machine.\n";
		errMessage += "2. Run the enable protection workflow again.\n";

		return errMessage;
	}

	inline std::string getErrorEP0887(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service Setup is unable to complete installation as the computer has an incorrect setting.\n";
		errMessage += "1. The startup type of Volume Shadow Copy(VSS) service and/or COM + System Application service is set to'Disabled'.\n";
		errMessage += "1. Ensure the startup type of Volume Shadow Copy(VSS) service and COM + System Application service is set to 'Enabled'.\n";
		errMessage += "2.Run the enable protection workflow again.\n";

		return errMessage;
	}

	inline std::string getErrorEP0888(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service Setup failed to complete installation due to an internal error.\n";
		errMessage += "Review the installation logs at ";
		errMessage += getPlaceHolderValue(placeHolders, pushLogPath);
		errMessage += " for more details\n";

		return errMessage;
	}

	inline std::string getErrorEP0889(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service Setup failed to complete as incorrect parameters were passed to the setup.\n";
		errMessage += "Review the command-line arguments for the installer at ";
		errMessage += linkForMobInstall;
		errMessage += "and re - run installer again.";

		return errMessage;
	}

	inline std::string getErrorEP0890(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nAnother instance of Installer is already running.\n";
		errMessage += "Wait for the other instance to complete before you try to launch the installer again.";

		return errMessage;
	}

	inline std::string getErrorEP0891(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service Setup is not compatible with the current operating system. Please retry installation after picking up the correct installer.\n";
		errMessage += "Use the Mobility Service version table under the section \"Install the Mobility Service manually\" in";
		errMessage += linkForMobInstall;
		errMessage += " to identify the right version of the installer for the current operating system.";

		return errMessage;
	}

	inline std::string getErrorEP0892(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service Setup does not have all the required installation arguments.\n";
		errMessage += "Review the command line install syntax from ";
		errMessage += linkForMobInstall;
		errMessage += " and retry installation.";

		return errMessage;
	}

	inline std::string getErrorEP0894(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service Setup is unable to find passphrase file ";
		errMessage += getPlaceHolderValue(placeHolders, passphrasePath);
		errMessage += "Ensure that the\n";
		errMessage += "1) The file exists at the given location.\n2) Your account has access to the file location.";

		return errMessage;
	}

	inline std::string getErrorEP0895(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service Setup is unable to proceed as the ";
		errMessage += getRecommendedActionForServiceError(errCode);
		errMessage += "service could not be stopped.\n";

		return errMessage;
	}

	inline std::string getErrorEP0896(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service  Setup was unable to procced as the logged in user does not have privileges to perform this installation.\n";
		errMessage += "Please use credentials with administrator privileges and retry enable protection";

		return errMessage;
	}

	inline std::string getErrorEP0897(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service Update failed as the software to be updated could not be found.\n";
		errMessage += "Check Add/Remove programs to ensure that the Azure Site Recovery Master Target/Mobility Service is installed before running the upgrade.";

		return errMessage;
	}

	inline std::string getErrorEP0898(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service Setup failed as the kernel version is not supported.\n";
		errMessage += " Please make sure that the supported kernel version is available before installing the software.";

		return errMessage;
	}

	inline std::string getErrorEP0899(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service Setup is not supported on this version of Linux yet.\n";
		errMessage += "Please check the list of supported Linux versions at";
		errMessage += supportedLinuxVersionLink;

		return errMessage;
	}

	inline std::string getErrorEP0900(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nSome of the drivers / services required by the mobility service are missing.\n";
		errMessage += "Please ensure that the drivers / services mentioned at ";
		errMessage += supportedDriversLink;
		errMessage += "are installed.";
		return errMessage;
	}

	inline std::string getErrorEP0901(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nThe server is already running one of the Azure Site Recovery infrastructure Server role (Configuration Server / Scaleout Process Server, Master Target Server).";
		errMessage += "\nAzure Site Recovery Infrastructure Servers cannot be protected. Please try to protect a different server.";
		return errMessage;
	}

	inline std::string getErrorEP0902(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " completed with a warning( ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ").\nSetup requires you to restart the server for some system changes to take effect.\n";

		return errMessage;
	}

	inline std::string getErrorEP0903(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " completed with a warning( ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ").\nSetup requires you to restart the server for some system changes to take effect.\n";

		return errMessage;
	}

	inline std::string getErrorEP0904(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ").\nThis version of Mobility Service does not support the operating system kernel version running on the source machine.\n";

		return errMessage;
	}

	inline std::string getErrorEP0905(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code EP0905.";
		errMessage += " The mobility service setup failed on this computer as";
		errMessage += " the necessary Hyper-V Linux Integration Services components";
		errMessage += "(hv_vmbus, hv_netvsc, hv_storvsc) are not installed on this machine.";

		return errMessage;
	}

	inline std::string getErrorEP0906(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += " There is insufficient free space available on the root partition(/ ).";
		errMessage += " Please ensure that there is at least 2 GB free space available on the";
		errMessage += " root partition before you retry installation.";

		return errMessage;
	}

	inline std::string getErrorEP0907(std::map<std::string, std::string> placeHolders)
	{
		std::string errMessage = "Enable Protection failed to install mobility service on ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += " Insufficient free space available on disk.";

		return errMessage;
	}

	inline std::string getErrorEP0908(std::map<std::string, std::string> placeHolders)
	{
		std::string errMessage = "Enable Protection failed to install mobility service on ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += " VMware tools not installed on server.";

		return errMessage;
	}

	inline std::string getErrorEP0909(std::map<std::string, std::string> placeHolders)
	{
		std::string errMessage = "Enable Protection failed to install mobility service on ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += " Install VSS Provider failed.";

		return errMessage;
	}

	inline std::string getErrorEP0910(std::map<std::string, std::string> placeHolders)
	{
		std::string errMessage = "Enable Protection failed to install mobility service on ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += " Install driver manifest file failed.";

		return errMessage;
	}

	inline std::string getErrorEP0911(std::map<std::string, std::string> placeHolders)
	{
		std::string errMessage = "Enable Protection failed to install mobility service on ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += " Master Target Server role is already installed.";

		return errMessage;
	}

	inline std::string getErrorEP0912(std::map<std::string, std::string> placeHolders)
	{
		std::string errMessage = "Enable Protection failed to install mobility service on ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += " Virtualization platform mismatch detected.";

		return errMessage;
	}

	inline std::string getErrorEP0913(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service Setup failed to complete as incorrect parameters were passed to the setup.\n";
		errMessage += "Review the command-line arguments for the installer at ";
		errMessage += linkForMobInstall;
		errMessage += "and re - run installer again.";

		return errMessage;
	}

	inline std::string getErrorEP0914(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nUpdating driver entry in registry failed.\n";
		errMessage += "Retry the operation after some time.";

		return errMessage;
	}

inline std::string getErrorEP0915(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service Setup failed to complete as one or more pre-requisite checks failed.\n";

		return errMessage;
	}

	inline std::string getErrorEP2001(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service VmWare based installation failed due to an internal error.\n";
		return errMessage;
	}

	inline std::string getErrorEP2002(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service VmWare based installation failed because exit status file was not found.\n";
		return errMessage;
	}

	inline std::string getErrorEP2003(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service installation failed because pre-req checks extended errors json file was not found.\n";
		return errMessage;
	}

	inline std::string getErrorEP2004(std::map<std::string, std::string> placeHolders){

		std::string errMessage = "Push installation of the mobility service to the source machine ";
		errMessage += getPlaceHolderValue(placeHolders, sourceIp);
		errMessage += " failed with error code ";
		errMessage += getPlaceHolderValue(placeHolders, errCode);
		errMessage += ".\nMobility Service setup failed because extended errors json file was not found.\n";
		return errMessage;
	}
}

#endif
