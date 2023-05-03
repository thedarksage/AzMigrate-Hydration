#include <windows.h>
#include <iostream>

HANDLE g_stopEvent = NULL;

BOOL WINAPI debuggingWait(DWORD ctrlType)
{
    if (CTRL_C_EVENT == ctrlType) { 
        SetEvent(g_stopEvent);           
    }
    return true;
}

void waitForQuit()
{
    std::cout << "enter ctrl-c to quit" << std::endl;
    g_stopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (0 == g_stopEvent) {
        return;
    }
    SetConsoleCtrlHandler(&debuggingWait, true); 
    WaitForSingleObject(g_stopEvent, INFINITE);
}
