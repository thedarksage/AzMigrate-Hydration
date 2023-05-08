
//
//  File: prockill.cpp
//
//  Description:
//

#include <cerrno>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>
#include <fstream>

#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>

#include "../prockill.h"

bool ProcKill::stopProc(std::string const& procName, int sig, bool forceIfNeeded, int waitSeconds, bool stopChildrenOnSigKill)
{
    std::string name("(");
    name += procName;
    name += ")";
    
    int pid;

    std::string procPath;
    
    if (findProcPid(name, pid, procPath)) {
        if (SIGKILL == sig and stopChildrenOnSigKill) {
            stopChildren(pid, waitSeconds);
        }
        if (0 != kill(pid, sig)) {
            return false;
        }

        if (!waitForProcToExit(procPath, waitSeconds) && forceIfNeeded) {
            if (stopChildrenOnSigKill) {
                stopChildren(pid, waitSeconds);
            }
            if (0 != kill(pid, SIGKILL)) {
                return false;
            }

            return waitForProcToExit(procPath, waitSeconds);
        }
        return true;
    }
    return false;
}

bool ProcKill::findProcPid(std::string const& procName, int& pid, std::string& procPath)
{
    DIR* procDir = opendir("/proc");
    if (0 == procDir) {
        return false;
    }

    boost::shared_ptr<void> procDirGuard(static_cast<void*>(0),
                                         boost::bind(boost::type<int>(), &closedir, procDir));

    dirent* dirEnt;
    
    while (0 != (dirEnt = readdir(procDir))) {
        if (DT_DIR == dirEnt->d_type && (0 != (pid = convertToPid(dirEnt->d_name)))) {
            procPath = "/proc/";
            procPath += dirEnt->d_name;
            procPath += "/stat";
            if (isPidForProcName(procPath, procName)) {
                return true;
            }
        }
    }

    return false;
}


int ProcKill::convertToPid(char const* str)
{
    try {
        return boost::lexical_cast<int>(str);
    } catch (...) {
        return 0;
    }
}

bool ProcKill::isPidForProcName(std::string const& pidStatName, std::string const& procName)
{
    std::ifstream statFile(pidStatName.c_str());

    std::string pidProcName;
    std::string pidStr;

    statFile >> pidStr >> pidProcName;

    return (procName == pidProcName);
}

bool ProcKill::waitForProcToExit(std::string const& proc, int waitSeconds)
{
    bool waitInfinate = (waitSeconds <= 0);
        
    struct stat buffer;
    while (0 == stat(proc.c_str(), &buffer)) {
        sleep(1);
        if (!waitInfinate) {
            if (--waitSeconds <= 0 ) {
                return false;
            }
        }
    }

    return true;
}

bool ProcKill::stopChildren(int parentPid, int waitSeconds)
{

    bool allStopped = true;
    
    childPids_t childPids;
    findChildPids(parentPid, childPids);
    childPids_t::iterator iter(childPids.begin());
    childPids_t::iterator iterEnd(childPids.end());
    for (/* empty */; iter != iterEnd; ++iter) {
        if (0 != kill((*iter).first, SIGKILL)) {
            return false;
        }

        if (!waitForProcToExit((*iter).second, waitSeconds)) {
            allStopped = false;
        }
    }

    return allStopped;
}

void ProcKill::findChildPids(int parentPid, childPids_t& childPids)
{
    DIR* procDir = opendir("/proc");
    if (0 == procDir) {
        return;
    }

    boost::shared_ptr<void> procDirGuard(static_cast<void*>(0),
                                         boost::bind(boost::type<int>(), &closedir, procDir));

    int pid;

    std::string procPath;
    
    dirent* dirEnt;
    
    while (0 != (dirEnt = readdir(procDir))) {
        if (DT_DIR == dirEnt->d_type && (0 != (pid = convertToPid(dirEnt->d_name)))) {
            procPath = "/proc/";
            procPath += dirEnt->d_name;
            procPath += "/stat";
            if (isChildProc(procPath, parentPid)) {
                childPids.insert(std::make_pair(pid, procPath));
            }
        }
    }
}

bool ProcKill::isChildProc(std::string const& pidStatName, int parentPid)
{
    std::ifstream statFile(pidStatName.c_str());

    std::string pidProcName;
    std::string pidStr;
    std::string state;
    std::string parentPidStr;
    

    statFile >> pidStr >> pidProcName >> state >> parentPidStr;

    return (convertToPid(parentPidStr.c_str()) == parentPid);
}
