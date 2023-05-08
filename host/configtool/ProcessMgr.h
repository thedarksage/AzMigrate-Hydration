#ifndef PROCESSMGR__H
#define PROCESSMGR__H
//#pragma once
//#include <Windef.h>
//#include <windows.h>
//
#include "portablehelpers.h"
#include "logger.h"
#include "portable.h"
#include "sstream"
#include "hostconfigprocessmgr.h"


int GetExitCodeProcessSmart(HANDLE hProcess, DWORD* pdwOutExitCode);
bool ExecuteProc(const std::string& cmd, DWORD& exitcode, std::stringstream& errmsg);

#endif