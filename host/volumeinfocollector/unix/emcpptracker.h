
//                                       
// File       : emcpptracker.h
// 
// Description: tracking EMC PowerPath devices
// 


#ifndef _EMCPP__TRACKER__H_
#define _EMCPP__TRACKER__H_ 

#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <vector>

#include <boost/lexical_cast.hpp>

#include "configurator2.h"
#include "localconfigurator.h"
#include "volumemanagerfunctions.h"
#include "portablehelpersmajor.h"

#define POWERMTARGS "display dev=all"
#define PSEUDONAMETOK "\'Pseudo name=\'"
#define PSEUDONAMEVALUE "-F\'=\' \'{ print $2 }\'"


class EqEMCPP: public std::unary_function<std::string, bool>
{
    std::string m_OnlyDiskName;
    
public:
    explicit EqEMCPP(const std::string &name)
    {
        m_OnlyDiskName = GetOnlyDiskName(name);
    }

    bool operator()(const std::string &emcname) const 
    {
        std::string emconlydiskname = GetOnlyDiskName(emcname);
        bool bisemcpp = (emconlydiskname == m_OnlyDiskName);
        return bisemcpp;
    }
};

class EMCPPTracker {
public:

    typedef std::vector<std::string> EMCPPNames_t;

    bool IsEMCPP(std::string const & name) const 
    {
        EMCPPNames_t::const_iterator iter = find_if(m_EmcPPDevices.begin(), m_EmcPPDevices.end(), EqEMCPP(name));
        bool bisemcpp = (m_EmcPPDevices.end() != iter);
        return bisemcpp;
    }

    EMCPPTracker()
    {
        LocalConfigurator theLocalConfigurator;
        std::string powermtcmd(theLocalConfigurator.getPowermtCmd());
        std::string grepcmd(theLocalConfigurator.getGrepCmd());
        std::string awkcmd(theLocalConfigurator.getAwkCmd());
        std::string cmd(powermtcmd);
        cmd += " ";
        cmd += POWERMTARGS;
        cmd += " ";
        cmd += "|";
        cmd += " ";
        cmd += grepcmd;
        cmd += " ";
        cmd += PSEUDONAMETOK;
        cmd += " ";
        cmd += "|";
        cmd += " ";
        cmd += awkcmd;
        cmd += " ";
        cmd += PSEUDONAMEVALUE;
        
        std::stringstream results;

        /* SAMPLE OUTPUT:
        [root@cent51-201 ~]# powermt display dev=all | grep 'Pseudo name='
        Pseudo name=emcpowerd
        Pseudo name=emcpowerc
        Pseudo name=emcpowerb
        Pseudo name=emcpowera
        Pseudo name=emcpowere
        [root@cent51-201 ~]#
        */

        if (executePipe(cmd, results))
        {
            while (!results.eof())
            {
                std::string ppname;
                results >> ppname;
                if (ppname.empty())
                {
                    break;
                }
                std::string ppnode = ppname;
                if (UNIX_PATH_SEPARATOR[0] != ppname[0])
                {
                    ppnode = NATIVEDISKDIR;
                    ppnode += UNIX_PATH_SEPARATOR;
                    ppnode += ppname;
                }
                m_EmcPPDevices.push_back(ppnode);
            }
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "Failed to execute powermt\n");
        }
    }

private:
    EMCPPNames_t m_EmcPPDevices;
};

#endif /* _EMCPP__TRACKER__H_ */

