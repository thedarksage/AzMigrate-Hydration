#ifndef HOSTCONFIGPROCESSMGR__H
#define HOSTCONFIGPROCESSMGR__H


#include <string>
#include "svtypes.h"

bool execProcUsingPipe(const std::string& cmdline, std::string& err_msg, const bool isOutputRequire = false, std::string& out = std::string(""));

#endif