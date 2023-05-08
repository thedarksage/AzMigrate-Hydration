#include "platforminfo.h"

PlatformInfo::PlatformInfo()
{
}


PlatformInfo::~PlatformInfo()
{
}


std::string PlatformInfo::GetCustomizedDeviceParent(const std::string &device, std::string &errmsg)
{
    return m_SpecificPlatformInfo.GetCustomizedDeviceParent(device, errmsg);
}


std::string PlatformInfo::GetCustomizedAttributeValue(const std::string &name, const std::string &attribute, std::string &errmsg)
{
    return m_SpecificPlatformInfo.GetCustomizedAttributeValue(name, attribute, errmsg);
}
