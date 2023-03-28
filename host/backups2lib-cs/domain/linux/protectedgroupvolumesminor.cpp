#include <cstring>
#include <string>
#include <list>
#include <sstream>
#include "runnable.h"
#include "volumegroupsettings.h"
#include "protectedgroupvolumes.h"
#include "cxmediator.h"
#include "devicestream.h"
#include "inmsafecapis.h"

extern Configurator* TheConfigurator;

int ProtectedGroupVolumes::ProcessLastIOATLUNRequest(void)
{
    int iStatus = SV_FAILURE;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s: %s\n",FUNCTION_NAME,GetSourceVolumeName().c_str());

    if (!m_pVolumeSettings->atLunStatsRequest.physicalInitiatorPWWNs.empty())
    {
        std::list<std::string>::size_type npis = m_pVolumeSettings->atLunStatsRequest.physicalInitiatorPWWNs.size();
        size_t atlsnbytes = sizeof (ATLunLastHostIOTimeStamp) + ((npis - 1) * sizeof (WWPN_DATA));
        ATLunLastHostIOTimeStamp *patls;
        patls = (ATLunLastHostIOTimeStamp *)calloc(1, atlsnbytes);
        if (patls)
        {
            /* since size of uuid is GUID_SIZE_IN_CHARS */
            inm_strncpy_s (patls->uuid, ARRAYSIZE(patls->uuid), m_pVolumeSettings->atLunStatsRequest.atLUNName.c_str(), GUID_SIZE_IN_CHARS);
            patls->wwpn_count = npis;
            std::list<std::string>::const_iterator pistart = m_pVolumeSettings->atLunStatsRequest.physicalInitiatorPWWNs.begin();
            std::list<std::string>::const_iterator piend = m_pVolumeSettings->atLunStatsRequest.physicalInitiatorPWWNs.begin();
            std::list<std::string>::const_iterator piiter = pistart;
            for (std::list<std::string>::size_type i = 0; i < npis; i++, piiter++)
            {
                const std::string &pi = *piiter;
                inm_strncpy_s(patls->wwpn_data[i].wwpn, ARRAYSIZE(patls->wwpn_data[i].wwpn), pi.c_str(), MAX_WWPN_SIZE - 1);
            }

            iStatus = m_pDeviceStream->IoControl(IOCTL_INMAGE_AT_LUN_LAST_HOST_IO_TIMESTAMP, patls);
            if (iStatus)
            {
                DebugPrintf(SV_LOG_DEBUG, "driver gave " ULLSPEC " for last io time stamps for source volume %s," 
                                          " AT LUN name %s\n",
                                          patls->timestamp, GetSourceVolumeName().c_str(), patls->uuid);
                CxMediator cxmr;
                /* should we check ts == 0 ? */
                iStatus = cxmr.SendLastIOTimeStampOnATLUN(TheConfigurator, GetSourceVolumeName(), m_pVolumeSettings->atLunStatsRequest, patls->timestamp);
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "ioctl asking driver for last io time stamps for source volume %s, at lun name %s failed\n",
                                          GetSourceVolumeName().c_str(), patls->uuid);
            }

            free (patls);
        }
        else
        {
            std::stringstream msg;
            msg << "Allocation of " << atlsnbytes 
                << " for supplying PI list to driver"
                << " failed before issuing at lun stats ioctl\n";
            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "For source volume %s, the list of initiator pwwns sent from "
                                  "CX is empty for last IO timestamp request\n", GetSourceVolumeName().c_str());
    }
 
 
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s: %s\n",FUNCTION_NAME, GetSourceVolumeName().c_str());
    return iStatus;
}

