
//                                       
// File       : hdlmtracker.h
// 
// Description: tracking Hitachi Dynamic Link (HDLM) devices
// 


#ifndef HDLMTRACKER_H
#define HDLMTRACKER_H

#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <list>
#include <iterator>
#include <algorithm>
#include <functional>
#include <string>


#include <boost/lexical_cast.hpp>

#include "configurator2.h"
#include "localconfigurator.h"
#include "executecommand.h"
#include "utildirbasename.h"
#include "volumemanagerfunctionsminor.h"
#include "volumemanagerfunctions.h"
#include "portablehelpersmajor.h"

class HdlmTracker {
public:

    typedef std::multimap<std::string, std::string> ByHdlmName_t;
    typedef std::pair<std::string, std::string> ByHdlmNamePair_t;

    class EqHDLM: public std::unary_function<std::string, bool>
    {
        std::string m_OnlyDiskName;
    public:
        explicit EqHDLM(const std::string &name)
        {
            m_OnlyDiskName = GetOnlyDiskName(name);
        }
        bool operator()(const ByHdlmNamePair_t &in) const 
        {
            std::string hdlmname = in.first;
            std::string hdlmonlydiskname = GetOnlyDiskName(hdlmname);
            bool bishdlm = (hdlmonlydiskname == m_OnlyDiskName);
            return bishdlm;
        }
    };

    HdlmTracker() {
        LoadHdlm();        
    }


    bool IsHdlm(std::string const & name) const {
        bool bishdlmname = false;
        if (IsHdlmName(name)) {
            bishdlmname = true;
        }

        return bishdlmname;
    }
    
    bool IsHdlmName(std::string const & name) const {
        ByHdlmName_t::const_iterator iter = find_if(m_ByHdlmName.begin(), m_ByHdlmName.end(), EqHDLM(name));
        bool bishdlm = (iter != m_ByHdlmName.end());
        return bishdlm;
    }

    void dump() {
        std::cout << "BY HDLM NAME:\n";
        ByHdlmName_t::iterator iter1(m_ByHdlmName.begin());
        ByHdlmName_t::iterator endIter1(m_ByHdlmName.end());
        for (/* empty*/; iter1 != endIter1; ++iter1) {
            std::cout << "  " << (*iter1).first << " - " << (*iter1).second << '\n';
        }
    }

protected:
    bool LoadHdlm() {
        // NOTE: assumes the command works the same accross all Unicies
        // and is always installed under /opt/DynamiclinkManager
        // might want to use OS specific way to find the actual location
        // e.g. on OSes using rpm the following would work
        // rpm -ql dlnkmgr 2> /dev/null | grep "dlnkmgr$" | xargs file 2> /dev/null | grep executable | grep -v shell | cut -d':' -f1;

        LocalConfigurator theLocalConfigurator;
        std::string dlnkmgrCmd(theLocalConfigurator.getHdlmDlnkmgr());
        std::string cmd(dlnkmgrCmd + " view -drv -t  2> /dev/null");

        std::stringstream results;

        if (!executePipe(cmd, results)) {
            return false;
        }

        std::string pathId;
        std::string hdlmName;
        std::string multiPathName;

        std::string line;

        while (!results.eof()) {
            results >> pathId >> hdlmName >> multiPathName;
            std::getline(results, line);
            if (hdlmName.empty()) {
                break;
            }

            try {
                boost::lexical_cast<long long>(pathId);
                std::string hdlmnode = hdlmName;
                if (hdlmName[0] != UNIX_PATH_SEPARATOR[0])
                {
                    hdlmnode = NATIVEDISKDIR;
                    hdlmnode += UNIX_PATH_SEPARATOR;
                    hdlmnode += hdlmName;
                }
                m_ByHdlmName.insert(std::make_pair(hdlmnode, multiPathName));
            } catch(boost::bad_lexical_cast &) {
                // OK just means we got some extra output
                // this is usual the tatus info for the command just run something like
                // KAPL01001-I The HDLM command completed normally. Operation name = view, completion time = 2008/12/05 13:42:13
            }

            hdlmName.clear();
        }

        return true;
    }
    
    
private:
    
    ByHdlmName_t m_ByHdlmName;

};


#endif // ifndef HDLMTRACKER_H
