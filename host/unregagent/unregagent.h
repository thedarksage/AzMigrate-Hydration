#ifndef UNREGAGENT_H
#define UNREGAGENT_H

#ifdef SV_WINDOWS
    #include "resource.h"
    #include <windows.h>
    #include <atlbase.h>
#endif

SVERROR WorkOnUnregister(const char* pszHostType,const bool askuser);

#endif // ifndef UNREGAGENT_H
