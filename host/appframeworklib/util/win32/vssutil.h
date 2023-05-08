#ifndef __VSS_UTIL__H
#define __VSS_UTIL__H
#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <list>
#include <map>
#include "svtypes.h"
#include <string>

struct VssProviderProperties
{
	std::map<std::string, std::string> proverPropertiesMap;
    std::list<std::pair<std::string, std::string> > getPropList() ;

} ;

struct VSSWriterProperties
{
    std::string m_writerName ;
    std::string m_writerId ;
    std::string m_writerInstanceId ;
    std::string m_state ;
    std::string m_lastError ;
    std::list<std::pair<std::string, std::string> > getPropList() ;
} ;
SVERROR listVssProviders(std::list<VssProviderProperties>& providersList) ;
//SVERROR listWriterInstances(std::list<VSSWriterProperties>& writers) ;
#endif