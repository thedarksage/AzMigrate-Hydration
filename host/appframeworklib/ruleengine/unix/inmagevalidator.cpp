#include <sstream>
#include <set>
#include <ace/Process_Manager.h>
#include <ace/File_Lock.h>
#include <boost/lexical_cast.hpp>

#include "inmageex.h"
#include "inmagevalidator.h"
#include "config/applocalconfigurator.h"
#include "Consistency/TagGenerator.h"
#include "util/common/util.h"
#include "../common/portablehelpers.h"
#include "npwwnif.h"
#include "cdputil.h"
#include "appcommand.h"
#include "controller.h"

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>

VolTagIssueValidator::VolTagIssueValidator(const std::string& name, 
                                           const std::string& description,
                                           const std::list<std::string>& volList,
										   InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_volList(volList)
{

}

VolTagIssueValidator::VolTagIssueValidator(const std::list<std::string>& volList,
										   const std::string strVacpOptions,
                                           InmRuleIds ruleId)
:Validator(ruleId),
m_strVacpOptions(strVacpOptions),
m_volList(volList)
{
    m_volList = volList ;

}
SVERROR VolTagIssueValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet  = SVS_OK ;
    InmRuleStatus status = INM_RULESTAT_NONE ;
	InmRuleErrorCode ruleStatus = RULE_STAT_NONE;
	
	status = INM_RULESTAT_PASSED ;
	ruleStatus = RULE_PASSED;
    setStatus(status) ;
	setRuleExitCode(ruleStatus);
	std::string message = "Success";
	setRuleMessage(message);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR VolTagIssueValidator::fix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet  = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
bool VolTagIssueValidator::canfix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet  = false;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

VolumeCapacityCheckValidator::VolumeCapacityCheckValidator(const std::string& name, 
                                       const std::string& description, 
                                       const std::list<std::map<std::string, std::string> > &volCapacitiesInfo,
                                       InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_volCapacitiesInfo(volCapacitiesInfo)
{
}

VolumeCapacityCheckValidator::VolumeCapacityCheckValidator(const std::list<std::map<std::string, std::string> > &volCapacitiesInfo,
                                       InmRuleIds ruleId)
:Validator(ruleId),
m_volCapacitiesInfo(volCapacitiesInfo)
{
}

SVERROR VolumeCapacityCheckValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_OK ;
    InmRuleStatus status = INM_RULESTAT_PASSED ;
    std::stringstream resultStream ;
	InmRuleErrorCode ruleStatus = RULE_PASSED;

	std::list<std::map<std::string, std::string> >::const_iterator volCapacitiesIter = m_volCapacitiesInfo.begin();
    while(volCapacitiesIter != m_volCapacitiesInfo.end())
    {
        SV_ULONGLONG srcCapacity = boost::lexical_cast<SV_ULONGLONG>(volCapacitiesIter->find("srcVolCapacity")->second);
        SV_ULONGLONG tgtCapacity = boost::lexical_cast<SV_ULONGLONG>(volCapacitiesIter->find("tgtVolCapacity")->second);
        if(tgtCapacity < srcCapacity)
        {
            resultStream << "Target volume size is less than source volume size " << std::endl ;
            resultStream << "The Source Volume Name : " << volCapacitiesIter->find("srcVolName")->second << std::endl;
            resultStream << "The Target Volume Name : " << volCapacitiesIter->find("tgtVolName")->second << std::endl;
            resultStream << "The Source Volume size : " << srcCapacity << std::endl;
            resultStream << "The Target Volume size : " << tgtCapacity << std::endl;
            bRet = SVS_FALSE ;
        }
        volCapacitiesIter++;
    }
	
    if(bRet == SVS_FALSE )
	{
		ruleStatus = VOLUME_CAPACITY_ERROR;
        status = INM_RULESTAT_FAILED ;
	}
    else
    {
        resultStream << "Target volume capacities are in sync with source volume capacities" << std::endl;
    }

	setStatus(status) ;
	setRuleExitCode(ruleStatus);
	std::string message = resultStream.str();
	setRuleMessage(message);	
	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool VolumeCapacityCheckValidator::canfix()
{
    return false ;
}

SVERROR VolumeCapacityCheckValidator::fix()
{
    return SVS_FALSE ;
}

CacheDirCapacityCheckValidator::CacheDirCapacityCheckValidator(const std::string& name, 
                                       const std::string& description, 
                                       InmRuleIds ruleId)
:Validator(name, description, ruleId)
{
}

CacheDirCapacityCheckValidator::CacheDirCapacityCheckValidator(InmRuleIds ruleId)
:Validator(ruleId)
{
}

SVERROR CacheDirCapacityCheckValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
    InmRuleErrorCode ruleErrorCode = RULE_STAT_NONE;
    std::stringstream resultStram, tempStream ;
    bool isAvailableDiskSpace = false;
    AppLocalConfigurator configurator ;
    std::string cachedir = configurator.getCacheDirectory();
    SVMakeSureDirectoryPathExists(cachedir.c_str()) ;
    unsigned long long insVolFreeSpace = 0, insVolCapacity = 0;
    unsigned long long expectedfreespace = 0;
    if(GetInstallVolCapacityAndFreeSpace(cachedir, insVolFreeSpace, insVolCapacity) == true)
    {
        unsigned long long pct = configurator.getMinCacheFreeDiskSpacePercent();
        unsigned long long expectedfreespace1  = (insVolCapacity * pct) / 100 ;
        unsigned long long expectedfreespace2  = static_cast<unsigned long long>(configurator.getMinCacheFreeDiskSpace());
        expectedfreespace = ((expectedfreespace1) < (expectedfreespace2)) ? (expectedfreespace1) : (expectedfreespace2);
        expectedfreespace += 1;
        if (insVolFreeSpace > expectedfreespace ) 
        {
            isAvailableDiskSpace = true;
        }
        tempStream << "Cache Directory: " << cachedir << ", " << std::endl;
        tempStream << "Total Volume Capacity:  " << insVolCapacity << " B, " << std::endl;
        tempStream << "Available Drive FreeSpace: " << insVolFreeSpace << " B, " << std::endl;
        tempStream << "Expected Minimum Drive Free Space: "  << expectedfreespace << " B, " << std::endl;
        float freeSpacePercent = 0, expectedFreeSpacePercet = 0;
        if(insVolCapacity != 0)
        {
            insVolFreeSpace = insVolFreeSpace*100;
            expectedfreespace = expectedfreespace*100;
            freeSpacePercent = insVolFreeSpace/insVolCapacity;
            expectedFreeSpacePercet = expectedfreespace/insVolCapacity;
            tempStream << "Available FreeSpace : " << freeSpacePercent << "% " << std::endl;
            tempStream << "Expected Minimum FreeSpace : " << expectedFreeSpacePercet << "%" << std::endl;
        }                  
        DebugPrintf(SV_LOG_DEBUG,"checkcachedirspace: %s\n", resultStram.str().c_str());
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "getinstallvolcapacityandfreespace() failed.\n");
        resultStram << "failed to caliculate catche drives free space." << std::endl;
    }
    if(isAvailableDiskSpace == true)
    {
        DebugPrintf(SV_LOG_DEBUG,"Cache Drive has enough free sapce\n");
        resultStram << "Cache Drive has enough free sapce \n" ;
        ruleStatus = INM_RULESTAT_PASSED;
        ruleErrorCode = RULE_PASSED;
        bRet = SVS_OK;
    }
    else
    {
        resultStram << "Cache Drive has not enough free sapce\n" ;
        ruleStatus = INM_RULESTAT_FAILED;
        ruleErrorCode = CACHE_DIR_CAPACITIY_ERROR;
    }
    resultStram << tempStream.str();
    setStatus(ruleStatus) ;
    setRuleExitCode(ruleErrorCode);
    std::string message = resultStram.str();
    setRuleMessage(message);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool CacheDirCapacityCheckValidator::canfix()
{
    return false ;
}

SVERROR CacheDirCapacityCheckValidator::fix()
{
    return SVS_FALSE ;
}

CMMinSpacePerPairCheckValidator::CMMinSpacePerPairCheckValidator(const std::string& name, 
                                       const std::string& description,
                                       const SV_ULONGLONG& totalNumberOfPairs,
                                       InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_totalNumberOfPairs(totalNumberOfPairs)
{
}

CMMinSpacePerPairCheckValidator::CMMinSpacePerPairCheckValidator(const SV_ULONGLONG& totalNumberOfPairs, InmRuleIds ruleId)
:Validator(ruleId),
m_totalNumberOfPairs(totalNumberOfPairs)
{
}

SVERROR CMMinSpacePerPairCheckValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
	InmRuleErrorCode ruleErrorCode = RULE_STAT_NONE;
    std::stringstream resultStream, tempStream ;
    bool isAvailableDiskSpace = false;
	AppLocalConfigurator configurator ;
    std::string cachedir = configurator.getCacheDirectory();
	SVMakeSureDirectoryPathExists(cachedir.c_str()) ;
	unsigned long long insVolFreeSpace = 0, insVolCapacity = 0;
	unsigned long long expectedFreeSpace = 0, consumedSpace = 0;
    unsigned long long totalSpaceForCM = 0, totalCMSpaceReq = 0;
	if(GetInstallVolCapacityAndFreeSpace(cachedir, insVolFreeSpace, insVolCapacity) == true)
	{
        cachedir.erase(cachedir.end()-1);
        consumedSpace = getDirectoryConsumedSpace(cachedir);
        expectedFreeSpace = static_cast<unsigned long long>(configurator.getCMMinReservedSpacePerPair());
        totalSpaceForCM = consumedSpace + insVolFreeSpace;
        totalCMSpaceReq = expectedFreeSpace * m_totalNumberOfPairs;
        if(totalSpaceForCM >= totalCMSpaceReq)
        {
            isAvailableDiskSpace = true;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "CheckCacheDirSpace low on local cache disk space:free space\n");
        }
        tempStream << "Cache Directory: " << cachedir << ", " << std::endl;
        tempStream << "Total Drive Capacity: " << insVolCapacity << " B, " << std::endl;
        tempStream << "Available Drive FreeSpace: " << insVolFreeSpace << " B, " << std::endl;
        tempStream << "Expected Minimum Reserved Space per pair: " << expectedFreeSpace << " B, " << std::endl;
        tempStream << "The Total Number of Pairs: " << m_totalNumberOfPairs << ", " << std::endl;
        tempStream << "Total Cache Memory Required for all the pairs: " << totalCMSpaceReq << " B, " << std::endl;
        tempStream << "Total Cache Memory Available including consumed Space: " << totalSpaceForCM << " B, " << std::endl;
        DebugPrintf(SV_LOG_DEBUG,"checkcachedirspace: %s\n", cachedir.c_str());
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "getinstallvolcapacityandfreespace() failed.\n");
		resultStream << "failed to caliculate catche drives free space." << std::endl;
	}
	if(isAvailableDiskSpace == true)
    {
        DebugPrintf(SV_LOG_DEBUG,"Cache Drive has enough free sapce\n");
        resultStream << "Cache Drive has enough free sapce \n" ;
        ruleStatus = INM_RULESTAT_PASSED;
        ruleErrorCode = RULE_PASSED;
        bRet = SVS_OK;
    }
    else
    {
        resultStream << "Cache Drive has not enough free sapce\n" ;
        ruleStatus = INM_RULESTAT_FAILED;
        ruleErrorCode = CM_MIN_SPACE_PER_PAIR_ERROR;
    }
	resultStream << tempStream.str();
    setStatus(ruleStatus) ;
	setRuleExitCode(ruleErrorCode);
	std::string message = resultStream.str();
	setRuleMessage(message);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SV_ULONGLONG CMMinSpacePerPairCheckValidator::getDirectoryConsumedSpace(std::string& cachedir)
{
    SV_ULONGLONG consumedSpace = 0;
    struct stat FileInfo;
    std::string fileName = cachedir;
    struct dirent *readDirPtr;
    DIR * openDirPtr;
    
    openDirPtr = opendir(fileName.c_str());
    if (openDirPtr == NULL)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to open the directory %s, ERROR %d\n", fileName.c_str(), errno);
        return consumedSpace;
    }
    while ((readDirPtr = readdir(openDirPtr)) != NULL)
    {
        std::string dirCheck = cachedir + "/" + readDirPtr->d_name;
        if (lstat(dirCheck.c_str(), &FileInfo) < 0)
        {
            DebugPrintf(SV_LOG_DEBUG, "Failed to open the directory, ERROR %d\n", errno) ;
        }
        //finding sub-directories
        if ( S_ISDIR( FileInfo.st_mode ))
        {
            if((strcmp(readDirPtr->d_name, ".") != 0) && (strcmp(readDirPtr->d_name, "..") != 0 ))
            {                
                std::string dirName = fileName + "/" + readDirPtr->d_name;
                consumedSpace = consumedSpace + (unsigned long long)getDirectoryConsumedSpace(dirName);
            }
        }
        else
        {
            std::string file = fileName + "/" + readDirPtr->d_name;
            if (lstat(file.c_str(), &FileInfo) < 0)
            {
                DebugPrintf(SV_LOG_DEBUG, "Failed to get the structure of the directory %s, ERROR %d\n", fileName.c_str(), errno) ;
            }
            consumedSpace = consumedSpace + (unsigned long long)FileInfo.st_size;
        }
    }
    closedir(openDirPtr);
    return consumedSpace;
}

bool CMMinSpacePerPairCheckValidator::canfix()
{
    return false ;
}

SVERROR CMMinSpacePerPairCheckValidator::fix()
{
    return SVS_FALSE ;
}

ApplianceTargetLunCheckValidator::ApplianceTargetLunCheckValidator(const std::string& name,
          const std::string& description,
          const std::string& atLunNumber,
          const std::string& atLunName,
          const std::list<std::string>&    physicalInitiatorPwwns,
          const std::list<std::string>&    applianceTargetPwwns,
          InmRuleIds  ruleId)
:Validator(name, description, ruleId),
m_atLunNumber(atLunNumber),
m_atLunName(atLunName),
m_physicalInitiatorPwwns(physicalInitiatorPwwns),
m_applianceTargetPwwns(applianceTargetPwwns)
{

}

ApplianceTargetLunCheckValidator::ApplianceTargetLunCheckValidator(const std::string& atLunNumber,
          const std::string&  atLunName,
          const std::list<std::string>&    physicalInitiatorPwwns,
          const std::list<std::string>&    applianceTargetPwwns,
          InmRuleIds  ruleId)
:Validator(ruleId),
m_atLunNumber(atLunNumber),
m_atLunName(atLunName),
m_physicalInitiatorPwwns(physicalInitiatorPwwns),
m_applianceTargetPwwns(applianceTargetPwwns)
{

}

bool ApplianceTargetLunCheckValidator::QuitRequested(int sec)
{
    return (Controller::getInstance()->QuitRequested(sec));
}

SVERROR ApplianceTargetLunCheckValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    InmRuleStatus status = INM_RULESTAT_FAILED ;
	InmRuleErrorCode ruleStatus = DUMMY_LUNZ_NOT_FOUND_ERROR;

    std::stringstream resultStream ;
    PIAT_LUN_INFO atLunInfo;

    atLunInfo.applianceTargetLUNNumber = boost::lexical_cast<SV_ULONGLONG>(m_atLunNumber);
    atLunInfo.applianceTargetLUNName = m_atLunName;
    atLunInfo.physicalInitiatorPWWNs = m_physicalInitiatorPwwns;
    atLunInfo.sourceType = VolumeSummary::DISK;
    std::list<std::string> applianceTargetPWWNs;

    bool isIscsi = false;
    for (std::list<std::string>::iterator atListIter = m_applianceTargetPwwns.begin();
            atListIter != m_applianceTargetPwwns.end(); atListIter++) 
    {
        std::string applianceTargetPwwn = *atListIter;

        std::size_t found = applianceTargetPwwn.find(";");
        if (found == std::string::npos) 
            atLunInfo.applianceTargetPWWNs.push_back(applianceTargetPwwn);
        else
        {
            isIscsi = true;
            atLunInfo.applianceTargetPWWNs.push_back(applianceTargetPwwn.substr(0, found));

            std::size_t found1 = applianceTargetPwwn.find(";",found+1);
            while (found1 != std::string::npos)
            {
                std::string ipAddress = applianceTargetPwwn.substr(found+1, found1);
                atLunInfo.applianceNetworkAddress.push_back(ipAddress);
                found=found1;
                found1 = applianceTargetPwwn.find(";",found+1);
            }

            if (found1 == std::string::npos)
            {
                std::string ipAddress = applianceTargetPwwn.substr(found+1, found1);
                atLunInfo.applianceNetworkAddress.push_back(ipAddress);
            }

        }
    }

    ATLunNames_t atLuns;

    resultStream << "Lun Number : " << m_atLunNumber <<  "\t" ;
    resultStream << "Lun Name : " << m_atLunName <<  "\t" ;

    resultStream << "Initiator List : ";
    if ( m_physicalInitiatorPwwns.empty())
    {
        resultStream << "Empty.\n";
    }
    else
    {
        std::list<std::string>::const_iterator piListIter = m_physicalInitiatorPwwns.begin();
        while(piListIter != m_physicalInitiatorPwwns.end())
        {
            resultStream << *piListIter <<  "\t" ;
            piListIter++;
        }
    }

    resultStream << "Appliance Target List : " ;
    if ( m_applianceTargetPwwns.empty())
    {
        resultStream << "Empty.\n";
    }
    else
    {
        std::list<std::string>::const_iterator atListIter = m_applianceTargetPwwns.begin();
        while(atListIter != m_applianceTargetPwwns.end())
        {
            resultStream << *atListIter << "\t" ;
            atListIter++;
        }
    }

    if ( !m_applianceTargetPwwns.empty() && (isIscsi || !m_physicalInitiatorPwwns.empty()))
    {
        GetATLunNames(atLunInfo, ApplianceTargetLunCheckValidator::QuitRequested, atLuns);
    }

    if (atLuns.empty())
    {
		DebugPrintf(SV_LOG_ERROR, "Appliance Target Lun not found.\n");
        resultStream << "Appliance Target Lun is not found." <<std::endl ;
    }
    else
    {
		DebugPrintf(SV_LOG_DEBUG, "Appliance Target Lun found.\n");
        bRet = SVS_OK;
        status = INM_RULESTAT_PASSED ;
        ruleStatus = RULE_PASSED;
        resultStream << "Appliance Target Lun is found." <<std::endl ;
        //const ATLunNames_t::const_iterator beginatiter = atLuns.begin();
        //const ATLunNames_t::const_iterator endatiter = atLuns.end();
        //for_each(beginatiter, endatiter, PrintString);
    }

    setStatus(status) ;
	setRuleExitCode(ruleStatus);
	std::string message = resultStream.str();
	setRuleMessage(message);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ApplianceTargetLunCheckValidator::canfix()
{
    return false;
}

SVERROR ApplianceTargetLunCheckValidator::fix()
{
    return SVS_FALSE;
}

TargetVolumeValidator::TargetVolumeValidator(const std::string& name,
          const std::string& description,
          const std::list<std::map<std::string, std::string> >&    targetVolumeInfo,
          InmRuleIds  ruleId)
:Validator(name, description, ruleId),
m_targetVolumeInfo(targetVolumeInfo)
{

}

TargetVolumeValidator::TargetVolumeValidator(
          const std::list<std::map<std::string, std::string> >&    targetVolumeInfo,
          InmRuleIds  ruleId)
:Validator(ruleId),
m_targetVolumeInfo(targetVolumeInfo)
{

}

SVERROR TargetVolumeValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_OK;
    InmRuleStatus status = INM_RULESTAT_PASSED ;
	InmRuleErrorCode ruleStatus = RULE_PASSED;

    std::stringstream resultStream ;

    std::list<std::map<std::string, std::string> >::iterator volInfoIter = m_targetVolumeInfo.begin();

    AppLocalConfigurator localconfigurator ;
    std::string tgtVolInfoFile = localconfigurator.getInstallPath() + "/etc/TgtVolInfoFile.dat";

    std::string lockFilePath = tgtVolInfoFile + ".lck";
    ACE_File_Lock lock(getLongPathName(lockFilePath.c_str()).c_str(),O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);

    if (lock.acquire_write() == -1)
    {
        DebugPrintf(SV_LOG_ERROR,"\n Error Code: %d \n",ACE_OS::last_error());
        bRet = SVS_FALSE;
    }

    std::ofstream out;

    out.open(tgtVolInfoFile.c_str(), std::ios::trunc);
    if (!out) 
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open %s file\n", tgtVolInfoFile.c_str());
        bRet = SVS_FALSE;
    }
    else
    {
        while (volInfoIter != m_targetVolumeInfo.end())
        {
            std::string deviceName = volInfoIter->find("DeviceName")->second;
            std::string fsType = volInfoIter->find("FileSystemType")->second;
            std::string volType = volInfoIter->find("VolumeType")->second;
            std::string vendor = volInfoIter->find("VendorOrigin")->second;
            std::string mountPoint = volInfoIter->find("MountPoint")->second;
            std::string diskGroup = volInfoIter->find("DiskGroup")->second;
            std::string shouldDestroy = volInfoIter->find("shouldDestroy")->second;

            if (fsType.empty()) fsType = "0";
            if (mountPoint.empty()) mountPoint = "0";
            if (diskGroup.empty()) diskGroup = "0";

            DebugPrintf( SV_LOG_DEBUG, "deviceName: '%s' fsType: '%s' volType: '%s' vendor: '%s' mountPoint: '%s' diskGroup: '%s' shouldDestroy : '%s'\n", deviceName.c_str(), fsType.c_str(), volType.c_str(), vendor.c_str(), mountPoint.c_str(), diskGroup.c_str(), shouldDestroy.c_str()) ;

            std::string line = "DeviceName=" + deviceName + ":";
            line += "FileSystemType=" + fsType + ":";
            line += "VolumeType=" + volType + ":";
            line += "VendorOrigin=" + vendor + ":";
            line += "MountPoint=" + mountPoint + ":";
            line += "DiskGroup=" + diskGroup + ":";
            line += "shouldDestroy=" + shouldDestroy + " ";

            DebugPrintf( SV_LOG_DEBUG, "tgtVolInfo %s\n", line.c_str());

            out << line << std::endl;

            volInfoIter++;
        }

        out.close();

        SV_ULONG exitCode = 1 ;
        std::string strCommnadToExecute ;
        strCommnadToExecute += localconfigurator.getInstallPath();
        strCommnadToExecute += "/scripts/inmvalidator.sh ";
        strCommnadToExecute += "targetreadiness ";
        strCommnadToExecute += tgtVolInfoFile;
        strCommnadToExecute += std::string(" ");

        DebugPrintf(SV_LOG_INFO,"\n The command to execute : %s\n",strCommnadToExecute.c_str());

        std::string outputFileName = "/tmp/inmtgtreadiness.log";
        ACE_OS::unlink(outputFileName.c_str());

        AppCommand objAppCommand(strCommnadToExecute, 0, outputFileName);

        std::string output;

        if(objAppCommand.Run(exitCode, output, Controller::getInstance()->m_bActive) != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to spawn script %s.\n", strCommnadToExecute.c_str());
            bRet = SVS_FALSE;
        }
        else
        {
            DebugPrintf(SV_LOG_INFO,"Successfully spawned the script.\n");
            DebugPrintf(SV_LOG_INFO,"output. %s\n", output.c_str());
            resultStream << output;

            if (exitCode != 0)
            {
                DebugPrintf(SV_LOG_ERROR,"Failed script %s.\n", strCommnadToExecute.c_str());
                bRet = SVS_FALSE;
            }
        }
    }
    
    lock.release();

    if (bRet == SVS_OK)
    {
        resultStream << "Target volumes check success." << std::endl ;
    }
    else
    {
        status = INM_RULESTAT_FAILED;
        ruleStatus = TARGET_VOLUME_IN_USE_ERROR;
    }

    setStatus(status) ;
	setRuleExitCode(ruleStatus);
	std::string message = resultStream.str();
	setRuleMessage(message);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool TargetVolumeValidator::canfix()
{
    return false;
}

SVERROR TargetVolumeValidator::fix()
{
    return SVS_FALSE;
}
