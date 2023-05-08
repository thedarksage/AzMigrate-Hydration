#ifndef APPDEVICEREPORTER_H
#define APPDEVICEREPORTER_H

#include "volumegroupsettings.h"
#include <list>
#include <set>

/* application devices which are not
     * identified by volumeinfocollector
     * itself */
typedef struct AppDevices
{
    VolumeSummary::Vendor m_Vendor;
    std::string m_Name;                 /* can be empty; if empty, the vg name sent will be string(vendor) */
    std::set<std::string> m_Devices;

    AppDevices()
    {
        m_Vendor = VolumeSummary::UNKNOWN_VENDOR;
    }

}AppDevices_t ;

typedef std::list<AppDevices_t> AppDevicesList_t;    /* any API filling in application devices should append to this */
typedef AppDevicesList_t::const_iterator ConstAppDevicesListIter_t;


bool getAppDevicesList(AppDevicesList_t& appDevicesList);
void printAppDevicesList(const AppDevicesList_t& appDevicesList);

#endif

