///
/// @file unregworker.h
/// Define helper functions for unregistering host agent
///
#ifndef UNREG_WORKER__H
#define UNREG_WORKER__H


SVERROR WorkOnUnregister(const char* pszHostType, const bool askuser);
void GetConsoleYesOrNoInput(char & ch);

#endif
