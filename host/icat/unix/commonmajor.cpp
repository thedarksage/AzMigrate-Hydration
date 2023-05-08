#include "../common.h"
#include <stdlib.h>

std::string getLogFilePath() 
{
    return "/var/log" ;
}

std::string getInstallationPath()
{
    return "" ;
}

std::string getAgentPath(std::string section)
{
    return "" ;
}

SV_ULONGLONG convertStringtoULL( const std::string& strVal ) 
{
    return (SV_ULONGLONG) strtoull( strVal.c_str(), NULL, 10 ) ;
}
char* getPathSeparator() 
{
    return "/" ;
}
