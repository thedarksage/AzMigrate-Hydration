/*
#include<atlbase.h>
#include "portablehelpers.h" 

std::string getInstallationPath();
std::string getAgentPath(std::string);

std::string getInstallationPath()
{
    std::string installPath;
    installPath = getAgentPath("SOFTWARE\\SV Systems\\VxAgent");
    if(installPath.empty() == true)
    {
        installPath = getAgentPath("SOFTWARE\\SV Systems\\FileReplicationAgent"); 
        if(installPath.empty() == true)
        {
            installPath = ".";
        }
    }

    return installPath; 
}

std::string getAgentPath(std::string section)
{
    CRegKey reg;
    char szPathname[ MAX_PATH ] = "";
    ULONG cch = sizeof( szPathname );
    LONG rc1 = reg.Open( HKEY_LOCAL_MACHINE, section.c_str(), KEY_READ );
    if( ERROR_SUCCESS != rc1 ) 
    {
        DebugPrintf(SV_LOG_ERROR, "\n Couldn't open FileReplicationAgent registry key %s",section.c_str());
        return "";
    }    
    rc1 = reg.QueryStringValue( "InstallDirectory", szPathname, &cch );
    if( ERROR_SUCCESS != rc1 )
    {
        DebugPrintf(SV_LOG_ERROR, "\n Couldn't read config pathname from registry %s",section.c_str());
        return "";
    }
    return std::string( szPathname );
}

SV_ULONGLONG convertStringtoULL( const std::string& strVal ) 
{
    return (SV_ULONGLONG) _atoi64( strVal.c_str() ) ;
}
char* getPathSeparator() 
{
    return "\\" ;
}
  
*/
