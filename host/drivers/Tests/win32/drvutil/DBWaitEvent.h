#ifndef _DRVUTIL_DB_WAIT_EVENT_H_
#define _DRVUTIL_DB_WAIT_EVENT_H_
#include <string>

class CDBWaitEvent
{
public:
    CDBWaitEvent(HANDLE hEvent, std::wstring& VolumeGUID);
    ~CDBWaitEvent();

    bool IsVolumeGUID(std::wstring& VolumeGUID) const
    {
        return (VolumeGUID == m_VolumeGUID);
    }

    HANDLE  GetEventHandle() const
    {
        return m_hEvent;
    }

    void CloseHandle()
    {
        if (NULL != m_hEvent) {
            ::CloseHandle(m_hEvent);
            m_hEvent = NULL;
        }
    }

private:
    // Can not copy this object, as the event handle
    // will be closed on first destructor.
    CDBWaitEvent(const CDBWaitEvent& DBWaitEvent);
    CDBWaitEvent& operator=(const CDBWaitEvent &rhs);

private:
    HANDLE  m_hEvent;
    std::wstring m_VolumeGUID;
};


#endif // _DRVUTIL_DB_WAIT_EVENT_H_