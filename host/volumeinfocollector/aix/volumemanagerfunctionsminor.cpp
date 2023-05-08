#include <string>
#include<boost/algorithm/string/replace.hpp>
#include <sstream>
#include <cctype>
#include <cstring>
#include <vector>
#include <map>
#include "volumemanagerfunctions.h"
#include "volumemanagerfunctionsminor.h"
#include "logger.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "portablehelpersminor.h"
#include "utilfunctionsmajor.h"
#include "utildirbasename.h"

std::string GetPartitionNumberFromNameText(const std::string &partition)
{
	DebugPrintf(SV_LOG_ERROR, "%s not implemented \n", FUNCTION_NAME) ;
	return "" ;
}
std::string GetRawDeviceName(const std::string &dskname)
{
    std::string rdskname = dskname;
    boost::algorithm::replace_first(rdskname, "/dev/hdisk", "/dev/rhdisk");
    boost::algorithm::replace_first(rdskname, DMPDIR, RDMPDIR);
    boost::algorithm::replace_first(rdskname, DSKDIR, RDSKDIR);
    return rdskname;
}

bool GetDiskDevts(const std::string &onlydisk, std::set<dev_t> &devts)
{
   
    bool bfounddsktyp = false;

    struct stat64 volStat;
    if ((0 == stat64(onlydisk.c_str(), &volStat)) && volStat.st_rdev)
    {
        devts.insert(volStat.st_rdev);
        bfounddsktyp = true;
    }

    return bfounddsktyp;
}
std::string GetOnlyDiskName(const std::string FullDiskName)
{

	DebugPrintf(SV_LOG_ERROR, "%s not implemented \n", FUNCTION_NAME) ;
    return "";
}

std::string GetOnlyDiskName(const std::string &devname)
{
	
 	DebugPrintf(SV_LOG_DEBUG, "GetOnlyDiskName for %s\n", devname.c_str()) ;
	return devname;
}
