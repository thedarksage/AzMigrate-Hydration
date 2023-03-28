#include <string>
#include <sstream>
#include <map>
#include <windows.h>
#include "error.h"
#include "operatingsystem.h"
#include "operatingsysteminfomajor.h"
#include "portablehelpersmajor.h"

int OperatingSystem::SetOsSpecificInfo(std::string &errmsg)
{
	m_OsVal = SV_WIN_OS;
	int iStatus = SetOsReadableName(errmsg);
    iStatus = SetSystemType(errmsg);
	if (SV_SUCCESS == iStatus)
	{
		iStatus = SetOsVersionBuild(errmsg);
	}

	//  Get System Directory
	TCHAR szSysDir[SV_MAX_PATH] = { 0 };
	if (!GetSystemDirectory(szSysDir, sizeof(szSysDir)))
	{
		DebugPrintf(
			        SV_LOG_ERROR,
					"Cloud not get system directory. GetSystemDirectory failed with error:%lu\n",
					GetLastError()
				   );

		iStatus = SV_FAILURE;
	}
	else
	{
		m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::SYSTEMDIR, szSysDir));
	}

	//  Get the System Volume from System Directory path.
	TCHAR szSystemDrive[SV_MAX_PATH] = { 0 };
	if (!GetVolumePathName(szSysDir, szSystemDrive, sizeof(szSystemDrive)))
	{
		DebugPrintf(
			        SV_LOG_ERROR,
			        "Could not get the system drive. GetVolumePathName failed with error:%lu\n",
			        GetLastError()
				   );
		iStatus = SV_FAILURE;
	}
	else
	{
		m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::SYSTEMDRIVE, szSystemDrive));
	}

	//  Get the Disk Extents of sytem drive.
	disk_extents_t system_drive_extents;
	if (ERROR_SUCCESS != GetVolumeDiskExtents(szSystemDrive, system_drive_extents))
	{
		DebugPrintf ( SV_LOG_ERROR, "Could not get the system drive extents.\n" );
		iStatus = SV_FAILURE;
	}
	else
	{
		//  Format the string for system drive disk extents as shown below:
		//  disk-id-1:offset-1:length-1; ...;disk-id-N : offset-N : length-N;
		std::stringstream stream_drive_extent;
		disk_extents_iter_t iextent = system_drive_extents.begin();
		for ( ; iextent != system_drive_extents.end(); iextent++ )
		{
			stream_drive_extent 
				<< iextent->disk_id << ":"
				<< iextent->offset  << ":"
				<< iextent->length  << ";";
		}

		m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::SYSTEMDRIVE_DISKEXTENTS, stream_drive_extent.str()));
	}

	return iStatus;
}


int OperatingSystem::SetOsReadableName(std::string &errmsg)
{
    int iStatus = SV_FAILURE;

	WmiOSClassRecordProcessor p(&m_OsInfo);
    GenericWMI gwmi(&p);

    SVERROR sv = gwmi.init();
    if (sv != SVS_OK)
    {
		DebugPrintf(SV_LOG_ERROR, "Failed to initialize the generic wmi\n");
    }
    else
    {
		gwmi.GetData("Win32_OperatingSystem");

		std::string name;
		name = "Windows";
		ACE_utsname utsName;
		if (ACE_OS::uname(&utsName) >= 0)
		{
			iStatus = SV_SUCCESS;
			name = utsName.release;
			name += utsName.version;
			m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::NAME, name));
            m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::DISTRO_NAME, "Windows"));
		}
		else
		{
			errmsg = "ACE_OS::uname failed with error ";
			errmsg += ACE_OS::strerror(ACE_OS::last_error());
		}
	}

	return iStatus;
}


int OperatingSystem::SetSystemType(std::string &errmsg)
{
    int iStatus = SV_FAILURE;

    WmiCSClassRecordProcessor p(&m_OsInfo);
    GenericWMI gwmi(&p);

    SVERROR sv = gwmi.init();
    if (sv != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to initialize the generic wmi\n");
    }
    else
    {
        gwmi.GetData("Win32_ComputerSystem");
        iStatus = SV_SUCCESS;
    }
    return iStatus;
}


int OperatingSystem::SetOsVersionBuild(std::string &errmsg)
{
	int iStatus = SV_FAILURE;

	OSVERSIONINFOEX o;
	memset(&o, 0, sizeof o);
	o.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

	bool bset = GetVersionEx((OSVERSIONINFO*)&o);
	if (bset)
	{
		iStatus = SV_SUCCESS;
		std::stringstream ssmj, ssmi, ssbn;
		ssmj << o.dwMajorVersion;
		ssmi << o.dwMinorVersion;
		ssbn << o.dwBuildNumber;
		m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::MAJOR_VERSION, ssmj.str()));
		m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::MINOR_VERSION, ssmi.str()));
		m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::BUILD, ssbn.str()));
	}
	else
	{
		std::stringstream ss;
		ss << GetLastError();
		DebugPrintf(SV_LOG_ERROR, "GetVersionEx failed with errno %s\n", ss.str().c_str());
	}

	return iStatus;
}
