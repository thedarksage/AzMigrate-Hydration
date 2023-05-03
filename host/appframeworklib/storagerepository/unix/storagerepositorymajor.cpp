#include "storagerepositorymajor.h"
#include "utilmajor.h"
#include "portablehelpersmajor.h"
#include <sys/stat.h>
#include <sys/statvfs.h>
#include "appcommand.h"
#include "inmageex.h"
#include "serializeapplicationsettings.h"
#include "controller.h"

StorageRepositoryObjPtr StorageRepository::getSetupRepoObj(std::map<std::string, std::string>& properties, const std::string& outputFileName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	StorageRepositoryObjPtr SetupRepoObjPtr;
	if(properties.find("TYPE") != properties.end())
	{
		std::string setuRepType = properties.find("TYPE")->second;
		if(strcmpi(setuRepType.c_str(), "DISK")== 0)
		{
			SetupRepoObjPtr.reset(new DiskStorageRepository(properties, outputFileName));
		}
		else if(strcmpi(setuRepType.c_str(), "LOGICALVOLUME")== 0 || strcmpi(setuRepType.c_str(), "PARTITION")== 0)
		{
			SetupRepoObjPtr.reset(new VolumeStorageRepository(properties, outputFileName));
		}	
		else if(strcmpi(setuRepType.c_str(), "PATH") == 0)
		{
			SetupRepoObjPtr.reset(new NFSStorageRepository(properties, outputFileName));
		}
		else if(strcmpi(setuRepType.c_str(), "CIFS") == 0)
		{
			SetupRepoObjPtr.reset(new CIFSNFSStorageRepository(properties, outputFileName));
		}
		else
		{
			throw AppException("InValid Repository Type found.. Cannot proceed");
		}
	}
	else
	{
		throw AppException("Repositiory Type is not found in the policy parameters");
		DebugPrintf(SV_LOG_ERROR,"TYPE key is not available in the map.\n");
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return SetupRepoObjPtr;
}
StorageRepositoryObjPtr StorageRepository::getSetupRepoObjV2(const std::string& outputFileName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	StorageRepositoryObjPtr SetupRepoObjPtr;
	SetupRepoObjPtr.reset(new VGRepository(outputFileName)) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return SetupRepoObjPtr ;
}

bool validateMountPoint(const std::string& deviceNamePath, const std::string& mountPointNamePath)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRetValue = false;

	std::string tempDeviceName = "";
	std::string givenDeviceName = deviceNamePath;
	if(GetDeviceNameForMountPt(mountPointNamePath.c_str(), tempDeviceName))
	{
		// it is already mounted. // TO. DO. need to check what the tempDeviceName will be.
		if(strcmpi(givenDeviceName.c_str(), tempDeviceName.c_str()) ==0)
		{
			bRetValue = true;
		}
	}
	else
	{
		// not yet mounted.
		bRetValue = true; 
	}
	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRetValue;	
}

DiskStorageRepository::DiskStorageRepository(std::map<std::string,std::string>& propeties, const std::string& outputFile):StorageRepository(outputFile, "DISK")
{
	std::map<std::string, std::string>::iterator propsMapTempIter = propeties.end(); 
	std::map<std::string, std::string>::iterator propsMapEndIter = propeties.end(); 
	propsMapTempIter = propeties.find("DeviceName");
	if(propsMapTempIter != propsMapEndIter)
	{
		m_deviceNamePath = propsMapTempIter->second;
	}
	propsMapTempIter = propeties.find("MountPoint");
	if(propsMapTempIter != propsMapEndIter)
	{
		m_mountPointNamePath = propsMapTempIter->second;
	}
	propsMapTempIter = propeties.find("Label");
	if(propsMapTempIter != propsMapEndIter)
	{
		m_label = propsMapTempIter->second;
	}
	propsMapTempIter = propeties.find("FileSystem");
	if(propsMapTempIter != propsMapEndIter)
	{
		m_fileSystemType = propsMapTempIter->second;
	}
	propsMapTempIter = propeties.find("CreateFS");
	if(propsMapTempIter != propsMapEndIter)
	{
		m_createFs = propsMapTempIter->second;
	}	
}

VolumeStorageRepository::VolumeStorageRepository(std::map<std::string,std::string>& propeties, const std::string& outputFile):StorageRepository(outputFile, "VOLUME")
{
	std::map<std::string, std::string>::iterator propsMapTempIter = propeties.end(); 
	std::map<std::string, std::string>::iterator propsMapEndIter = propeties.end(); 
	propsMapTempIter = propeties.find("DeviceName");
	if(propsMapTempIter != propsMapEndIter)
	{
		m_deviceNamePath = propsMapTempIter->second;
	}
	propsMapTempIter = propeties.find("MountPoint");
	if(propsMapTempIter != propsMapEndIter)
	{
		m_mountPointNamePath = propsMapTempIter->second;
	}
	propsMapTempIter = propeties.find("FileSystem");
	if(propsMapTempIter != propsMapEndIter)
	{
		m_fileSystemType = propsMapTempIter->second;
	}	
	propsMapTempIter = propeties.find("CreateFS");
	if(propsMapTempIter != propsMapEndIter)
	{
		m_createFs = propsMapTempIter->second;
	}	
	
}

NFSStorageRepository::NFSStorageRepository(std::map<std::string,std::string>& propeties, const std::string& outputFile):StorageRepository(outputFile, "PATH")
{
	std::map<std::string, std::string>::iterator propsMapTempIter = propeties.end(); 
	std::map<std::string, std::string>::iterator propsMapEndIter = propeties.end(); 
	propsMapTempIter = propeties.find("DeviceName");
	if(propsMapTempIter != propsMapEndIter)
	{
		m_deviceNamePath = propsMapTempIter->second;
	}
	propsMapTempIter = propeties.find("MountPoint");
	if(propsMapTempIter != propsMapEndIter)
	{
		m_mountPointNamePath = propsMapTempIter->second;
	}	
}

CIFSNFSStorageRepository::CIFSNFSStorageRepository(std::map<std::string,std::string>& propeties, const std::string& outputFile):StorageRepository(outputFile, "CIFS")
{
	std::map<std::string, std::string>::iterator propsMapTempIter = propeties.end(); 
	std::map<std::string, std::string>::iterator propsMapEndIter = propeties.end(); 
	propsMapTempIter = propeties.find("DeviceName");
	if(propsMapTempIter != propsMapEndIter)
	{
		m_deviceNamePath = propsMapTempIter->second;
	}
	propsMapTempIter = propeties.find("MountPoint");
	if(propsMapTempIter != propsMapEndIter)
	{
		m_mountPointNamePath = propsMapTempIter->second;
	}
	propsMapTempIter = propeties.find("UserName");
	if(propsMapTempIter != propsMapEndIter)
	{
		m_userName = propsMapTempIter->second;
	}
	propsMapTempIter = propeties.find("Password");
	if(propsMapTempIter != propsMapEndIter)
	{
		m_passwd = propsMapTempIter->second;
	}
	propsMapTempIter = propeties.find("Domainname");
	if(propsMapTempIter != propsMapEndIter)
	{
		m_domainName = propsMapTempIter->second;
	}		
}

SVERROR DiskStorageRepository::setupDiskRepository()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE;	
	//step 1) format the device. syntax:- mkfs -t ext3 deviceName.
	//step 2) Apply lable on the device. syntax:- e2label deviceName label.
	//step 3) create mount directory. syntax:- mkdir mountPoint	
	//step 4) Mount the device and Make entry in /etc/fstab.			
	std::string statusMessage = "";
	if(m_createFs.compare("0") == 0 || makeFileSystem(m_deviceNamePath, m_fileSystemType, m_outputFile, statusMessage))
	{
		if(m_label.empty() || applyLabel(m_deviceNamePath, m_label, m_outputFile, statusMessage))
		{
			if(!SVMakeSureDirectoryPathExists(m_mountPointNamePath.c_str()).failed())
			{
				if(MountVolume(m_deviceNamePath, m_mountPointNamePath, m_fileSystemType, false))
				{
					DebugPrintf(SV_LOG_DEBUG, "device %s mounted on %s successfully. \n", m_deviceNamePath.c_str(), m_mountPointNamePath.c_str());
					statusMessage = "Device mounted successfully \n" ; 
					retStatus = SVS_OK;
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Mount operation failed. \n");
					statusMessage += "Mount operation failed for ";
					statusMessage += m_deviceNamePath;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to create mount point directory %s.\n", m_mountPointNamePath.c_str());
				statusMessage += "Failed to create mount point directory ";
				statusMessage += m_mountPointNamePath;
			}
		}
	}
	m_statusMessage += statusMessage;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;	
}

SVERROR DiskStorageRepository::setupRepositoryV2(ApplicationPolicy& policy, SetupRepositoryStatus& repoStatus)
{
	return SVS_FALSE ;
}
SVERROR DiskStorageRepository::setupRepository()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE;

	if(!m_deviceNamePath.empty() && !m_mountPointNamePath.empty() && !m_fileSystemType.empty() && !m_createFs.empty())
	{
		if(IsValidDevfile(m_deviceNamePath))
		{
			if(validateMountPoint(m_deviceNamePath, m_mountPointNamePath))
			{
				retStatus = setupDiskRepository();
			}			
		}
		else
		{
			m_statusMessage += "Device: ";
			m_statusMessage += m_deviceNamePath;
			m_statusMessage += " is not a valid device";
			DebugPrintf(SV_LOG_ERROR, "Invalid device %s \n", m_deviceNamePath.c_str());
		}
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "All the required properties are not available. \n");
		m_statusMessage += "All the required properties are not available for device ";
		m_statusMessage += m_deviceNamePath;
	}
	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

SVERROR VolumeStorageRepository::setupVolumeRepository()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE;	
	//step 1) format the device if the m_createFs is non zero. syntax:- mkfs -t ext3 deviceName.
	//step 2) create mount directory. syntax:- mkdir mountPoint.	
	//step 3) Mount the device and Make entry in /etc/fstab.			
	std::string statusMessage = "";
	if(m_createFs.compare("0") == 0 || makeFileSystem(m_deviceNamePath, m_fileSystemType, m_outputFile, statusMessage))
	{
		if(!SVMakeSureDirectoryPathExists(m_mountPointNamePath.c_str()).failed())
		{
			if(MountVolume(m_deviceNamePath, m_mountPointNamePath, m_fileSystemType, false))
			{
				DebugPrintf(SV_LOG_DEBUG, "device %s mounted on %s successfully. \n", m_deviceNamePath.c_str(), m_mountPointNamePath.c_str());
				statusMessage = "Device mounted successfully \n" ;
				retStatus = SVS_OK;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Mount operation failed. \n");
				statusMessage += "Mount operation failed for ";
				statusMessage += m_deviceNamePath;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to create mount point directory %s.\n", m_mountPointNamePath.c_str());
			statusMessage += "Failed to create mount point directory ";
			statusMessage += m_mountPointNamePath;
		}
	}
	m_statusMessage += statusMessage;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

SVERROR VolumeStorageRepository::setupRepositoryV2(ApplicationPolicy& policy, SetupRepositoryStatus& repoStatus)
{
	return SVS_FALSE ;
}
SVERROR VolumeStorageRepository::setupRepository()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE;
	if(!m_deviceNamePath.empty() && !m_mountPointNamePath.empty() && !m_fileSystemType.empty() && !m_createFs.empty())
	{
		if(IsValidDevfile(m_deviceNamePath))
		{
			if(validateMountPoint(m_deviceNamePath, m_mountPointNamePath))
			{
				retStatus = setupVolumeRepository();
			}			
		}
		else
		{
			m_statusMessage += "Device: ";
			m_statusMessage += m_deviceNamePath;
			m_statusMessage += " is not a valid device";
			DebugPrintf(SV_LOG_ERROR, "Invalid device %s \n", m_deviceNamePath.c_str());
		}
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "All the required properties are not available. \n");
		m_statusMessage += "All the required properties are not available for device ";
		m_statusMessage += m_deviceNamePath;
	}
	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

SVERROR NFSStorageRepository::setupNFSPathRepository()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE;
	std::string statusMessage = "";
	if(!SVMakeSureDirectoryPathExists(m_mountPointNamePath.c_str()).failed())
	{
		if(mountNFS(m_deviceNamePath, m_mountPointNamePath, m_outputFile, statusMessage))
		{
			if(AddFSEntry(m_deviceNamePath.c_str(), m_mountPointNamePath.c_str(), "nfs", "rw", true))
			{
				retStatus = SVS_OK;
			}
			else
			{
				statusMessage += "Failed to make nfs entry in /etc/fstab.";
			}
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to create mount point directory %s.\n", m_mountPointNamePath.c_str());
		statusMessage += "Failed to create mount point directory ";
		statusMessage += m_mountPointNamePath;
	}	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

SVERROR NFSStorageRepository::setupRepositoryV2(ApplicationPolicy& policy, SetupRepositoryStatus& repoStatus)
{
	return SVS_FALSE ;
}
SVERROR NFSStorageRepository::setupRepository()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE;
	if(!m_mountPointNamePath.empty())
	{
		if(!SVMakeSureDirectoryPathExists(m_mountPointNamePath.c_str()).failed())
		{
			retStatus = SVS_OK;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Mountpoit name is not available. \n");
		m_statusMessage += "Mountpoit name is not available in the properties";
	}
	// if(!m_deviceNamePath.empty() && !m_mountPointNamePath.empty())
	// {
		// if(isValidNfsPath(m_deviceNamePath))
		// {
			// if(validateMountPoint(m_deviceNamePath, m_mountPointNamePath))
			// {
				// retStatus = SVS_OK;
				// // retStatus = setupNFSPathRepository();// if required we can add.
			// }
		// }
		// else
		// {
			// m_statusMessage += "NFS path: ";
			// m_statusMessage += m_deviceNamePath;
			// m_statusMessage += " is not a valid";
			// DebugPrintf(SV_LOG_ERROR, "Invalid NFS Path: %s \n", m_deviceNamePath.c_str());			
		// }
	// }
	// else
	// {
		// DebugPrintf(SV_LOG_DEBUG, "All the required properties are not available. \n");
		// m_statusMessage += "All the required properties are not available for device ";
		// m_statusMessage += m_deviceNamePath;
	// }	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

SVERROR CIFSNFSStorageRepository::setupCIFSPathRepository()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE;
	std::string statusMessage = "";
	if(!SVMakeSureDirectoryPathExists(m_mountPointNamePath.c_str()).failed())
	{
		if(mountCIFS(m_deviceNamePath, m_mountPointNamePath, m_userName, m_passwd, m_domainName, m_outputFile, statusMessage))
		{
			if(AddCIFS_FSEntry(m_deviceNamePath.c_str(), m_mountPointNamePath.c_str(), "cifs"))
			{
				retStatus = SVS_OK;
			}
			else
			{
				statusMessage += "Failed to make cifs entry in /etc/fstab.";
			}
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to create mount point directory %s.\n", m_mountPointNamePath.c_str());
		statusMessage += "Failed to create mount point directory ";
		statusMessage += m_mountPointNamePath;
	}	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

SVERROR CIFSNFSStorageRepository::setupRepositoryV2(ApplicationPolicy& policy, SetupRepositoryStatus& repoStatus)
{
	return SVS_FALSE ;
}
SVERROR CIFSNFSStorageRepository::setupRepository()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_FALSE;
	if(!m_deviceNamePath.empty() && !m_mountPointNamePath.empty())
	{
		if(isValidCIFSPath(m_deviceNamePath))
		{
			if(validateMountPoint(m_deviceNamePath, m_mountPointNamePath))
			{
				retStatus = setupCIFSPathRepository();
			}
		}
		else
		{
			m_statusMessage += "CIFS path: ";
			m_statusMessage += m_deviceNamePath;
			m_statusMessage += " is not a valid";
			DebugPrintf(SV_LOG_ERROR, "Invalid CIFS Path: %s \n", m_deviceNamePath.c_str());			
		}
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "All the required properties are not available. \n");
		m_statusMessage += "All the required properties are not available for device ";
		m_statusMessage += m_deviceNamePath;
	}	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

VGRepository::VGRepository(const std::string& outputFile):StorageRepository(outputFile, "VOLUME")
{

}

bool createVG(const std::string& vgName, const std::list<std::string>& deviceList, const std::string& outfile)
{
	std::list<std::string>::const_iterator deviceIter = deviceList.begin() ;
	std::stringstream vgCrateCmd ;
	vgCrateCmd << "vgcreate " << vgName << " " ; 
	while( deviceIter != deviceList.end() )
	{
		vgCrateCmd << " " << *deviceIter << " " ; 
		deviceIter++ ;
	}
	DebugPrintf(SV_LOG_DEBUG, "VG Create Command : %s\n", vgCrateCmd.str().c_str()) ;
	AppCommand appCmd(vgCrateCmd.str(), 120, outfile);
	SV_ULONG exitCode ; 
	std::string m_stdOut ;
	if(appCmd.Run(exitCode,m_stdOut, Controller::getInstance()->m_bActive) != SVS_OK)
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to to create VG\n" ) ;
		return false ;
	}
	DebugPrintf(SV_LOG_DEBUG, "Exit Code : %d\n", exitCode) ;
	return true ;
}
bool createLV(const std::string& vgName, const std::string& lvName, int percentOfFreeSpace, const std::string& outfile)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::stringstream lvCrateCmd ;
    lvCrateCmd << "lvcreate -l " << percentOfFreeSpace << "%FREE" << " -n" << lvName << " " << vgName ;
	DebugPrintf(SV_LOG_DEBUG, "lv create command: %s\n", lvCrateCmd.str().c_str()) ;
	AppCommand appCmd(lvCrateCmd.str(), 120, outfile);
    SV_ULONG exitCode ;
    std::string m_stdOut ;
	bool bRet = false ;
    if(appCmd.Run(exitCode,m_stdOut, Controller::getInstance()->m_bActive) == SVS_OK)
    {
		DebugPrintf(SV_LOG_DEBUG, "Exit Code : %d\n", exitCode) ;
    	std::string statusMessage = "";
		std::string deviceName = "/dev/mapper/" + vgName + "-" + lvName  ;
	    if( makeFileSystem(deviceName, "ext3", outfile, statusMessage))
    	{
			std::string mountPoint ;
			mountPoint = "/" ;
			mountPoint += lvName ;
			if(!SVMakeSureDirectoryPathExists(mountPoint.c_str()).failed())
			{
				DebugPrintf(SV_LOG_DEBUG, "Lv %s. Mounting Device %s on %s\n", lvName.c_str(), deviceName.c_str(), mountPoint.c_str()) ;
				if(MountVolume(deviceName, mountPoint, "ext3", false))
				{
					DebugPrintf(SV_LOG_DEBUG, "device %s mounted on %s successfully. \n", deviceName.c_str(), mountPoint.c_str());
					statusMessage = "Device mounted successfully \n" ;
					bRet = true ;
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Mount operation failed. \n");
					statusMessage += "Mount operation failed for ";
					statusMessage += deviceName;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to create mount point directory %s.\n", mountPoint.c_str());
				statusMessage += "Failed to create mount point directory ";
				statusMessage += mountPoint;
			}
    	}
    }
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to create LV %s\n", lvName.c_str()) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "Exit Code : %d\n", exitCode) ;
    return bRet ;
}

SVERROR VGRepository::setupRepositoryV2(ApplicationPolicy& policy, SetupRepositoryStatus& repoStatus)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR bRet = SVS_FALSE ;
	SetupRepositoryDetails setupRepoDetails ;
	try
	{
		setupRepoDetails = unmarshal<SetupRepositoryDetails>(policy.m_policyData) ;
		std::string vgName = setupRepoDetails.m_volumeGrpName ;
		DebugPrintf(SV_LOG_DEBUG, "Volume Group Name : %s\n", setupRepoDetails.m_volumeGrpName.c_str()) ;
		std::list<std::string> deviceList ;
		std::map<std::string, DeviceRole>::const_iterator deviceRoleMapIter = setupRepoDetails.deviceNameRoleMap.begin() ;
		while( deviceRoleMapIter != setupRepoDetails.deviceNameRoleMap.end() )
		{
			deviceList.push_back(deviceRoleMapIter->first) ;
			DebugPrintf(SV_LOG_DEBUG, "Device Name : %s\n", deviceRoleMapIter->first.c_str()) ;
			deviceRoleMapIter++ ;
		}
		if( createVG(vgName, deviceList, m_outputFile) )
		{
			DebugPrintf(SV_LOG_DEBUG, "CacheVolume  percentage %d\n", setupRepoDetails.CacheVolumeSizeInPercent) ;
			DebugPrintf(SV_LOG_DEBUG, "Protection volume percentage %d\n", setupRepoDetails.ProtectionVolumeSizeInPercent) ;
			SV_LONGLONG deviceFreeSpaceInMB ;
			deviceFreeSpaceInMB = 20 * 1024 ;
			DebugPrintf(SV_LOG_DEBUG, "vg free space in MB : %ld\n", deviceFreeSpaceInMB) ;
			SV_LONGLONG cachevolsizeInMb = ( deviceFreeSpaceInMB * setupRepoDetails.CacheVolumeSizeInPercent ) / 100 ;
			SV_LONGLONG protectionvolsizeInMb = ( deviceFreeSpaceInMB * setupRepoDetails.ProtectionVolumeSizeInPercent ) / 100 ;

			DebugPrintf(SV_LOG_DEBUG, "CacheVolume size in MB:%ld\n", cachevolsizeInMb) ;
			DebugPrintf(SV_LOG_DEBUG, "Retention size in MB:%ld\n", protectionvolsizeInMb) ;
			std::string cacheLvName= vgName + "_CacheLv" ;
			if( createLV(vgName, cacheLvName, setupRepoDetails.CacheVolumeSizeInPercent, m_outputFile) )
			{
				std::string protectionLvName = vgName + "_ProtectionLv" ;
				if( createLV(vgName, protectionLvName, 100, m_outputFile) )
				{
					bRet = SVS_OK ;
					std::string cacheDevice = "/dev/mapper/" + vgName + "-" + cacheLvName ;
					std::string protectionDevice = "/dev/mapper/" + vgName + "-" + protectionLvName;
					repoStatus.m_repositoryDetails.insert(std::make_pair("CacheVolume", cacheDevice)) ;
					repoStatus.m_repositoryDetails.insert(std::make_pair("ProtectionVolume", protectionDevice)) ;
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to create protection lv\n") ;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to create cache lv\n") ;
			}
		}
	}
	catch(ContextualException& ex)
	{
		DebugPrintf(SV_LOG_ERROR, "Unmarshal error %s\n", ex.what()) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}
