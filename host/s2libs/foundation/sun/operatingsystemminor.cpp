#include <string>
#include <sstream>
#include <map>
#include "error.h"
#include "operatingsystem.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "portablehelpersminor.h"

int OperatingSystem::SetOsSpecificInfo(std::string &errmsg)
{
	m_OsVal = SV_SUN_OS;
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
    std::string sunos_release_Value;
    struct utsname st_uts;
    std::string name = "Sunname";
    if ( uname( &st_uts ) == -1 )
    {
		errmsg = "Failed to stat out uname ";
		iStatus = SV_FAILURE;
		return iStatus;
    }
    const int COUNT = 257;
    char sytem_info[COUNT] = {'\0'};
    if( sysinfo( SI_ARCHITECTURE, sytem_info, COUNT ) == -1 )
    {
		errmsg = "Failed to stat out sysinfo ";
		iStatus = SV_FAILURE;
		return iStatus;
    }
    std::string uts_releaseValue;
    uts_releaseValue = st_uts.release;
    removeStringSpaces( uts_releaseValue );
    std::string systemArchitecture( sytem_info );
    removeStringSpaces( systemArchitecture );
    bool bis32BitMachine;
    bis32BitMachine = false;
    if( uts_releaseValue.compare("5.8") == 0 && systemArchitecture.find("sparc") != std::string::npos )
    {
        if( bis32BitMachine )
            name="Solaris-5-8-Sparc-32";
        else
            name="Solaris-5-8-Sparc";
    }
    else if( uts_releaseValue.compare("5.9") == 0 && systemArchitecture.find("sparc") != std::string::npos )
    {
        if( bis32BitMachine )
            name="Solaris-5-9-Sparc-32";
        else
            name="Solaris-5-9-Sparc";
    }
    else if( uts_releaseValue.compare("5.10") == 0 && ( systemArchitecture.find("i386") != std::string::npos || systemArchitecture.find("x86") != std::string::npos ) )
    {
        if( bis32BitMachine )
            name="Solaris-5-10-x86-32";
        else
            name="Solaris-5-10-x86-64";
    }
	else if( uts_releaseValue.compare("5.10") == 0 && systemArchitecture.find("sparc") != std::string::npos )
    {
        if( bis32BitMachine )
            name="Solaris-5-10-Sparc-32";
        else
            name="Solaris-5-10-Sparc";
    }
    else if( uts_releaseValue.compare("5.11") == 0 ) 
    {
	if( getFileContent( "/etc/release", sunos_release_Value ) == SVS_OK )
	{
		removeStringSpaces( sunos_release_Value );
		if(sunos_release_Value.find("Oracle Solaris") != std::string::npos )	
		{
			if( systemArchitecture.find("i386") != std::string::npos || systemArchitecture.find("x86") != std::string::npos ) 
			{
				if( bis32BitMachine )
	            		name="Solaris-5-11-x86-32";
      			  	else
            			name="Solaris-5-11-x86-64";
			}
			else if(systemArchitecture.find("sparc") != std::string::npos)
			{
		        if( bis32BitMachine )
            			name="Solaris-5-11-Sparc-32";
		        else
            			name="Solaris-5-11-Sparc";    
    			}
		}
		else if (sunos_release_Value.find("OpenSolaris") != std::string::npos)
		{
			if( systemArchitecture.find("i386") != std::string::npos || systemArchitecture.find("x86") != std::string::npos ) 
			{
				if( bis32BitMachine )
	            		name="OpenSolaris-5-11-x86-32";
      			  	else
            			name="OpenSolaris-5-11-x86-64";
			}
			else if(systemArchitecture.find("sparc") != std::string::npos)
			{
		        if( bis32BitMachine )
            			name="OpenSolaris-5-11-Sparc-32";
		        else
            			name="OpenSolaris-5-11-Sparc";    
    			}

		}
	}
    }	
    else
    {
		errmsg = " Un Supported SunOs Flavour.. ";
		iStatus = SV_FAILURE;
    }

	if (SV_SUCCESS == iStatus)
	{
        m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::NAME, name));
		m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::DISTRO_NAME, name));
	}

    return iStatus;
}


int OperatingSystem::SetOsVersionBuild(std::string &errmsg)
{
	return SV_SUCCESS;
}
