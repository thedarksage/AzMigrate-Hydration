
//
//  File: prockill.h
//
//  Description:
//

#ifndef PROCKILL_H
#define PROCKILL_H

#include <string>
#include <map>



class ProcKill {
public:
    typedef std::map<int, std::string> childPids_t;

    static bool stopProc(std::string const& procName, int sig, bool forceIfNeeded, int waitSeconds, bool stopChildrenOnSigKill);

protected:
    static int convertToPid(char const* str);

    static bool findProcPid(std::string const& procName, int& pid, std::string& procPath);
    static bool isPidForProcName(std::string const& pidStatName, std::string const& procName);
    static bool waitForProcToExit(std::string const& proc, int waitSeconds);
    static bool stopChildren(int parentPid, int waitSeconds);
    static bool isChildProc(std::string const& pidStatName, int parentPid);

    static void findChildPids(int parentPid, childPids_t& childPids);


private:

};


#endif // PROCKILL_H
