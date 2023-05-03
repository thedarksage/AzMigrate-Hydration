
///
/// \file fragentmajor.cpp
///
/// \brief
///

#include <iostream>
#include <fstream>
#include <string>

#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>

#include "fragent.h"

/*
 * CFileReplicationAgent::getFxAllowedDirsPath
 *
 * creates a FX allowed dirs path
 */
void CFileReplicationAgent::getFxAllowedDirsPath(boost::filesystem::path& fxPath)
{
    // on non windows fx gets installed under a different directy then vx
    // we need the path to the vx transport dir so try to find it from drscout.conf
    std::string path;
    std::ifstream drscoutConf("/usr/local/drscout.conf");
    if (drscoutConf.good()) {
        std::string line;
        while (!drscoutConf.eof()) {
            line.clear();
            std::getline(drscoutConf, line);
            if (!line.empty() && boost::algorithm::starts_with(line, "InstallDirectory=")) {
                std::string::size_type idx = line.find_first_of("=");
                if (std::string::npos != idx) {
                    path = line.substr(idx + 1);
                    boost::trim(path);
                }
                break;
            }
        }
    }
    if (path.empty()) {
        // not found so assume only Fx was installed and OK to us Fx path
        path = m_sv_host_agent_params.AgentInstallPath;
    }
    if ('/' == path[path.size() - 1]) {
        path.erase(path.size() - 1);
    }
    fxPath = path;
    fxPath /= "transport";
    fxPath /= "fxallowed";
}
