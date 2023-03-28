#ifndef _NPWWN__IF__H_
#define _NPWWN__IF__H_ 

#include <string>
#include <cctype>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <map>
#include <set>
#include <boost/function.hpp>
#include "svtypes.h"
#include "volumegroupsettings.h"
#include "prismsettings.h"
#include "inmstrcmp.h"
#include "inmquitfunction.h"

class Event;

typedef std::set<std::string> ATLunNames_t;

struct PIAT_LUN_INFO
{
    std::string sourceLUNID;
    SV_ULONGLONG applianceTargetLUNNumber;
    std::string applianceTargetLUNName;
    std::list<std::string> physicalInitiatorPWWNs;
    std::list<std::string> applianceTargetPWWNs;   
    VolumeSummary::Devicetype sourceType;
    std::list<std::string> applianceNetworkAddress;   

    PIAT_LUN_INFO()
    {
        applianceTargetLUNNumber = 0;
        sourceType = VolumeSummary::UNKNOWN_DEVICETYPE;
    }

    PIAT_LUN_INFO(const std::string &srclunid,
                  const SV_ULONGLONG atlunnumber,
                  const std::string &atlunname,
                  const std::list<std::string> &piwwns,
                  const std::list<std::string> &atwwns,
                  const VolumeSummary::Devicetype srctype,
                  const std::list<std::string> &apNetAddr)
    {
        sourceLUNID = srclunid;
        applianceTargetLUNNumber = atlunnumber;
        applianceTargetLUNName = atlunname;
        physicalInitiatorPWWNs = piwwns;
        applianceTargetPWWNs = atwwns;
        sourceType = srctype;
        applianceNetworkAddress = apNetAddr;
    }
};

void GetATLunNames(const PIAT_LUN_INFO &piatluninfo,
                   QuitFunction_t qf, 
                   ATLunNames_t &atlunnames);

bool DeleteATLunNames(const PIAT_LUN_INFO &piatluninfo, QuitFunction_t qf);

void GetNwwnPwwns(NwwnPwwns_t &npwwns);

void PrintNPWwns(const NwwnPwwns_t &npwwns);

#endif /* _NPWWN__IF__H_ */
