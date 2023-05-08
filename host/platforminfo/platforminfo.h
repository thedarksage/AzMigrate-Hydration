#ifndef PLATFORM_INFO_H
#define PLATFORM_INFO_H

#include <string>
#include "specificplatforminfo.h"

class PlatformInfo
{
public:
    PlatformInfo();
    ~PlatformInfo();
    std::string GetCustomizedDeviceParent(const std::string &device, std::string &errmsg);
    std::string GetCustomizedAttributeValue(const std::string &name, const std::string &attribute, std::string &errmsg);

private:
    SpecificPlatformInfo m_SpecificPlatformInfo;
};

#endif /* PLATFORM_INFO_H */
