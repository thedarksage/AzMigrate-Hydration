#ifndef DATAPROTECTIONBASE_H
#define DATAPROTECTIONBASE_H

#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include <ace/Manual_Event.h>

#include "cxtransportlogger.h"
#include "volumegroupsettings.h"

#include "sigslot.h"

class DataProtectionBase : public sigslot::has_slots<>
{
public:
    DataProtectionBase() : m_bLogFileXfer(false) {}

    virtual ~DataProtectionBase() {};

    virtual void Protect() = 0;

protected:
    virtual void ProcessHostVolumeGroupSettings(HOST_VOLUME_GROUP_SETTINGS& HostVolGrpSettings) = 0;

    virtual std::string GetLogDir() = 0;

    void SetUpFileXferLog(const VOLUME_SETTINGS& VolumeSettings)
    {
        /* NOTE: This logging should be removed
         *       in case transport server does
         *       logging itself for file tal */
        if ((m_bLogFileXfer) && (TRANSPORT_PROTOCOL_FILE == VolumeSettings.transportProtocol))
        {
            if (!m_CxTransportXferLogMonitor)
            {
                const std::string DPXFERLOGNAME = "dp.xfer.log";
                const int DPXFERLOGMAXSIZE = 10 * 1024 * 1024;
                const int DPXFERLOGRETAINSIZE = 0;
                const int DPXFERLOGROTATECOUNT = 2;
                boost::filesystem::path dpxferlogPath(GetLogDir());
                dpxferlogPath /= DPXFERLOGNAME;
                cxTransportSetupXferLogging(dpxferlogPath, DPXFERLOGMAXSIZE, DPXFERLOGROTATECOUNT, DPXFERLOGRETAINSIZE, true);
                m_CxTransportXferLogMonitor = cxTransportStartXferLogMonitor();
            }
        }
    }

protected:
    typedef ACE_Guard<ACE_Mutex> LockGuard;

    ACE_Mutex m_Lock;

    boost::shared_ptr<boost::thread> m_CxTransportXferLogMonitor;

    bool m_bLogFileXfer;
};

#endif // DATAPROTECTIONBASE_H
