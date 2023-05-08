#include "specificplatforminfo.h"


SpecificPlatformInfo::SpecificPlatformInfo()
{
}


SpecificPlatformInfo::~SpecificPlatformInfo()
{
}


std::string SpecificPlatformInfo::GetCustomizedDeviceParent(const std::string &device, std::string &errmsg)
{
    errmsg = "Not implemented";
    return std::string();
}


std::string SpecificPlatformInfo::GetCustomizedAttributeValue(const std::string &name, const std::string &attribute, std::string &errmsg)
{
    errmsg = "Not implemented";
    return std::string();
}
