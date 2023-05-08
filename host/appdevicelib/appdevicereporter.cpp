#include "appdevicereporter.h"
#include "executecommand.h"
#include "volumegroupsettings.h"
#include "portablehelpers.h"
#include <sstream>
#include <fstream>
#include <set>
#include <vector>
#include <iterator>
#include <ace/File_Lock.h>
#include <boost/tokenizer.hpp>
#include "localconfigurator.h"

bool getAppDevicesList(AppDevicesList_t& appDevicesList)
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = true;

    LocalConfigurator localConfigurator;
    if (!localConfigurator.shouldDiscoverOracle())
    {
        DebugPrintf(SV_LOG_DEBUG, "Oracle Discovery not defined. exiting.\n");
        return false;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Oracle Discovery is defined. not exiting.\n");
    }
    std::string installPath = localConfigurator.getInstallPath();
    std::string outputFile = installPath + "etc/applicationDisks.dat";

    std::string lockFilePath = outputFile + ".lck";
    ACE_File_Lock lock(getLongPathName(lockFilePath.c_str()).c_str(),O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
    if (lock.acquire_write() == -1)
    {
        DebugPrintf(SV_LOG_ERROR,"\n Error Code: %d \n",ACE_OS::last_error());
        return false;
    }

    std::string script = installPath + "scripts/oracleappagent.sh displayappdisks ";

    std::string cmd;
    cmd +=  script + outputFile;

    std::stringstream ret;

    if (!executePipe(cmd, ret))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to run the command %s\n", cmd.c_str());
        bRet = false;
    }

    lock.release();

    if (!bRet)
        return bRet;

    AppDevices_t appDevices;

    std::ifstream results(outputFile.c_str(), std::ifstream::in);

    while (results.good() && !results.eof())
    {
        std::string devString;
        results >> devString;
        if(devString.compare(0,3,"ASM") == 0)
        {
            appDevices.m_Vendor = VolumeSummary::ASM;         
        }
        /*
        else if(devString.compare(0,3,"VCS") == 0)
        {
            appDevices.m_Vendor = VolumeSummary::VCS;
        }
        */
        else if(devString.compare(0, 4, "/dev") == 0)
        {
            appDevices.m_Devices.insert(devString);
        }
    }

    if(appDevices.m_Devices.empty())
    {
        DebugPrintf(SV_LOG_DEBUG, "No ASM Disks found\n");
    }
    else
    {
        appDevicesList.push_back(appDevices);
        DebugPrintf(SV_LOG_DEBUG, "ASM Disks found\n");
    }

    return bRet;
}


void printAppDevicesList(const AppDevicesList_t& appDevicesList)
{
    ConstAppDevicesListIter_t iter = appDevicesList.begin();

	while(iter != appDevicesList.end())
	{
		const AppDevices_t& appDevices = *iter;
		DebugPrintf(SV_LOG_DEBUG, "======\n");
		DebugPrintf(SV_LOG_DEBUG, "Application devices list:\n");
		DebugPrintf(SV_LOG_DEBUG, "vendor: %s\n", StrVendor[appDevices.m_Vendor]);
		DebugPrintf(SV_LOG_DEBUG, "Name: %s\n", appDevices.m_Name.c_str());
		DebugPrintf(SV_LOG_DEBUG, "devices:\n");
        for_each(appDevices.m_Devices.begin(), appDevices.m_Devices.end(), PrintString);
		DebugPrintf(SV_LOG_DEBUG, "======\n");
		iter++;
	} 
}


