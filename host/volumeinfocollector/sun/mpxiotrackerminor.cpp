#include <string>
#include <cstring>
#include "mpxiotracker.h"
#include "localconfigurator.h"
#include "portablehelpers.h"

bool MpxioTracker::IsMpxioName(std::string const & name)
{
    std::string realName;
    VolumeInfoCollectorInfo::SymbolicLinks_t symbolicLinks;
    GetSymbolicLinksAndRealName(name, symbolicLinks, realName);
    std::vector<std::string>::const_iterator iter;
    bool bismpxioname = false;
    for (iter = m_DrvNames.begin(); iter != m_DrvNames.end(); iter++)
    {
        const std::string &drvname = (*iter);
        if (NULL != strstr(realName.c_str(), drvname.c_str()))
        {
            bismpxioname = true;
            break;
        } 
    }

    return bismpxioname;
}

void MpxioTracker::LoadMpxioDrvNames(void)
{
    const std::string delim = ",";
    LocalConfigurator lc;
    std::string drvnames = lc.getMpxioDrvNames();
    Tokenize(drvnames, m_DrvNames, delim);
}


MpxioTracker::MpxioTracker()
{
    LoadMpxioDrvNames();
}
