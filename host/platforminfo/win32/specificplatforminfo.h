#ifndef SPECIFIC_PLATFORM_INFO_H
#define SPECIFIC_PLATFORM_INFO_H

#include <string>

class SpecificPlatformInfo
{
public:
    SpecificPlatformInfo();
    ~SpecificPlatformInfo();
    std::string GetCustomizedDeviceParent(const std::string &device, std::string &errmsg);
    std::string GetCustomizedAttributeValue(const std::string &name, const std::string &attribute, std::string &errmsg);
};

#endif /* SPECIFIC_PLATFORM_INFO_H */
