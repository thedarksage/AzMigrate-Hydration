#include <string>
#include <sstream>
#include <map>
#include <odmi.h>
#include "error.h"
#include "operatingsystem.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "portablehelpersminor.h"

int OperatingSystem::SetOsSpecificInfo(std::string &errmsg)
{
	m_OsVal = SV_AIX_OS;
	int iStatus = SetOsReadableName(errmsg);
	if (SV_SUCCESS == iStatus)
	{
		iStatus = SetOsVersionBuild(errmsg);
	}

	return iStatus;
}


int OperatingSystem::SetOsReadableName(std::string &errmsg)
{
    int iStatus = SV_SUCCESS;

	std::string name="AIX";
    struct utsname st_uts;
	if ( uname( &st_uts ) == -1 )
	{
		errmsg = "Failed to stat out uname ";
		iStatus = SV_FAILURE;
		return iStatus;
	}
    std::string versionValue, releaseValue;
    versionValue = st_uts.version;        
    removeStringSpaces( versionValue );
    releaseValue = st_uts.release;
    removeStringSpaces( releaseValue );
	if( versionValue.compare("5") == 0 )
	{
		if( releaseValue.compare("2") == 0 )
		{
			name="AIX52";
		}
		else if( releaseValue.compare("3") == 0 )
		{
			name="AIX53";
		}
	}
	else if( versionValue.compare("6") == 0 )
	{	
		if( releaseValue.compare("1") == 0 )
		{
			name="AIX61";
		}
	}
	else if( versionValue.compare("7") == 0 )
	{	
		if( releaseValue.compare("1") == 0 )
		{
			name="AIX71";
		}
	}

	m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::NAME, name));
	m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::DISTRO_NAME, name));

    return iStatus;
}


int OperatingSystem::SetOsVersionBuild(std::string &errmsg)
{
	return SV_SUCCESS;
}
