#ifndef _HYDRATION_CONFIGRUATOR_
#define _HYDRATION_CONFIGRUATOR_

#include <map>
#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/property_tree/ptree.hpp>

#ifdef WIN32
#define SV_HYDRATION_CONF_FILE              "hydration.json"
#endif

typedef bool(*RequirementValidator)(boost::property_tree::ptree const&, std::map<std::string, std::string>&);

typedef struct _HydrationInfo{
    bool            bNeedsHydration;
    std::string     descriptionTag;

    _HydrationInfo(bool b, std::string t){
        bNeedsHydration = b;
        descriptionTag = t;
    }
}HydrationInfo;

class HydrationConfigurator
{
private:
    static std::map<std::string, std::string> GetConfigProperties(std::string config, std::string propertyseperator, std::string pairSeperator);
    static bool IsSupportedHost(std::string supportedConfigs, std::string &description);
    static bool IsHydrationNeededOnTarget(std::map<std::string, std::string> configuration, std::string& description);
    static std::string GenerateHydrationTag(std::string supportedTargetsTag, std::string descriptionTag);
    static std::string GenerateDescriptionProperty(std::string description);
    static std::string GenerateSummaryHydrationTag(std::string targetsDescription);
    static std::string SerializeDescriptionTag(std::map<std::string, std::string> systemStateMap);
    static std::string SerializeSupportedTargetsProperty(std::map<std::string, boost::shared_ptr<HydrationInfo>> targetHydrationMap);

public:
    static std::string GetHydrationTag(std::string hydrationConfigFile);
};

class HydrationProvider
{
public:
    static bool IsHydrationRequired(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskSystemState);
};

#endif