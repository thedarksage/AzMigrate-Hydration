#include <Windows.h>
#include "DBWaitEvent.h"


CDBWaitEvent::CDBWaitEvent(HANDLE hEvent, std::wstring &VolumeGUID)
            : m_hEvent(hEvent), m_VolumeGUID(VolumeGUID)
{
    return;
}

CDBWaitEvent::~CDBWaitEvent()
{
    if (NULL != m_hEvent) {
        ::CloseHandle(m_hEvent);
        m_hEvent = NULL;
    }
}


