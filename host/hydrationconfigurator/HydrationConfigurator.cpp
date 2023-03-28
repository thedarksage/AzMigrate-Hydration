//#ifdef SV_WINDOWS
#include "stdafx.h"
#include "vacpwmiutils.h"
//#endif

#include "HydrationConfigurator.h"
#include <iostream>

#include <boost/shared_ptr.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/path.hpp>


#include <boost/algorithm/string/compare.hpp>
#include <boost/algorithm/string/find.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/function.hpp>


#define NO_HYDRATION_ATTRIB                 "_no_hydration"
#define DESCRIPTION_SEPARATOR               ":"
#define PROPERTY_LIST_SEPARATOR             ";"
#define DESCRIPTION_ATTRIB                  "_desc"

bool ValidateHypervisor(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskState);
bool ValidateOperatingSystem(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskState);

std::map<std::string, RequirementValidator>    g_requirementMap = {
    { "hypervisor", ValidateHypervisor },
    { "os", ValidateOperatingSystem }
};

namespace HydrationAttributes{
    const char HYPERVISOR_NAME[] = "hv";
};

bool ValidateHypervisor(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskState)
{
    bool                isSupportedHypervisor = false;
    static bool                isVirtual = false;
    static bool         isHypervisorDetected = false;
    static std::string         hypervisorName;

    if (!isHypervisorDetected)
    {
        DebugPrintf(SV_LOG_DEBUG, "Determining Hypervisor information\n");
        if (IsVirutalMachine(isVirtual, hypervisorName)) {
            isHypervisorDetected = true;
            if (!isVirtual) {
                hypervisorName = "physical";
            }
        }
    }

    BOOST_FOREACH(boost::property_tree::ptree::value_type it, input) {
        std::string value = it.second.get_value<std::string>();
        if (boost::iequals(hypervisorName, value)) {
            isSupportedHypervisor = true;
            break;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "Hypervisor %s is %s supported\n", ((isSupportedHypervisor) ? "" : "not"), hypervisorName.c_str());

    onDiskState[HydrationAttributes::HYPERVISOR_NAME] = hypervisorName;
    return isSupportedHypervisor;
}

bool ValidateOperatingSystem(boost::property_tree::ptree const& input, std::map<std::string, std::string>& onDiskSystemState)
{
    bool    isHydrationNeeded = false;
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME);
    BOOST_FOREACH(boost::property_tree::ptree::value_type it, input) {
        if (boost::iequals(it.first, "windows")) {
            isHydrationNeeded = HydrationProvider::IsHydrationRequired(it.second, onDiskSystemState);
            if (!isHydrationNeeded) {
                DebugPrintf(SV_LOG_DEBUG, "Hydration is not needed for windows\n");
                return true;
            }
            else {
                DebugPrintf(SV_LOG_DEBUG, "Hydration is needed for windows\n");
                return false;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME);
    return false;
}

/*
* Description:
*      This function returns hydration tag for current machine.
* Input:
*       Hydration Configuration file
*      Returns:
*          Hydration tag.
*/
std::string HydrationConfigurator::GetHydrationTag(std::string hydrationConfigFile)
{
    std::map<std::string, std::string>                          osRequirementMap;
    std::map<std::string, std::string>                          targetRequirementMap;
    boost::property_tree::ptree                                 configProperty;
    std::map<std::string, boost::shared_ptr<HydrationInfo>>     targetHydrationMap;
    std::string                                                 description;
    std::string                                                 hydrationTag = "";


    try {
        boost::property_tree::ptree     jsonRoot;
        boost::property_tree::read_json(hydrationConfigFile, jsonRoot);

        boost::property_tree::ptree     noHydTargets = jsonRoot.get_child("_no_hydration_targets");

        std::map<std::string, std::string>                          actualState;
        std::string                                             supportedTargets = "";
        BOOST_FOREACH(boost::property_tree::ptree::value_type targetListIter, noHydTargets) {

            BOOST_FOREACH(boost::property_tree::ptree::value_type targetIter, targetListIter.second) {
                bool           isHydrationNeeded = false;
                std::string     target = targetIter.first;

                DebugPrintf(SV_LOG_DEBUG, "Validating Target: %s\n", target.c_str());

                boost::property_tree::ptree     targetRoot = targetIter.second;

                BOOST_FOREACH(boost::property_tree::ptree::value_type it2, targetRoot) {
                    if (g_requirementMap.find(it2.first) == g_requirementMap.end()) {
                        DebugPrintf(SV_LOG_ERROR, "%s has no handler\n", it2.first.c_str());
                        actualState[it2.first] = "no";
                        isHydrationNeeded = true;
                        continue;
                    }

                    bool isSupported = g_requirementMap[it2.first](it2.second, actualState);
                    if (isSupported) {
                        DebugPrintf(SV_LOG_DEBUG, "Validation PASSED for property %s\n", it2.first.c_str());
                    }
                    else{
                        DebugPrintf(SV_LOG_DEBUG, "Validation FAILED for property %s\n", it2.first.c_str());
                        isHydrationNeeded = true;
                    }
                }

                if (!isHydrationNeeded) {
                    DebugPrintf(SV_LOG_DEBUG, "No Hydration is required for target %s\n", target.c_str());
                    if (!supportedTargets.empty()) {
                        supportedTargets += "'";
                    }
                    supportedTargets += target;
                }
                else {
                    DebugPrintf(SV_LOG_DEBUG, "Hydration is required for target %s\n", target.c_str());
                }
            }

            std::string supportedTargetTag = "";

            if (!supportedTargets.empty()) {
                supportedTargetTag = (std::string(NO_HYDRATION_ATTRIB) + std::string(DESCRIPTION_SEPARATOR) + supportedTargets);
            }

            std::string descriptionTag = GenerateDescriptionProperty(SerializeDescriptionTag(actualState));
            hydrationTag = GenerateHydrationTag(supportedTargetTag, descriptionTag);
        }
    }
    catch (std::exception& ex){
        hydrationTag = GenerateDescriptionProperty(ex.what());
    }

    // Prepare Hydration Tag
    return hydrationTag;
}

/*
* Description:
*      This function generates hydration tag for current machine.
* Input:
*       Hydration Summary Tag
*       Hydration Description Tag
* Returns:
*          Hydration tag.
*/
std::string HydrationConfigurator::GenerateHydrationTag(std::string supportedTargetsTag, std::string descriptionTag)
{
    if (supportedTargetsTag.empty()){
        return descriptionTag;
    }
    else if (descriptionTag.empty()){
        return supportedTargetsTag;
    }

    return (supportedTargetsTag + PROPERTY_LIST_SEPARATOR + descriptionTag);
}

/*
* Description:
*      This function generates description hydration tag for current machine.
*          input is of format: SanPolicy:0
*          output is _desc:SanPolicy:0
* Input:
*       Description
* Returns:
*          Hydration Description tag.
*/
std::string HydrationConfigurator::GenerateDescriptionProperty(std::string description)
{
    if (description.empty()){
        return description;
    }

    return std::string(DESCRIPTION_ATTRIB) + std::string(DESCRIPTION_SEPARATOR) + description;
}

/*
* Description:
*      This function generates summary hydration tag for current machine.
*          input is of format: azure,aws
*          output is _no_hydr:azure,aws
* Input:
*
* Returns:
*          Hydration Summary tag.
*/
std::string HydrationConfigurator::GenerateSummaryHydrationTag(std::string targetsDescription)
{
    if (targetsDescription.empty()){
        return targetsDescription;
    }

    return std::string(NO_HYDRATION_ATTRIB) + std::string(DESCRIPTION_SEPARATOR) + targetsDescription;
}

/*
* Description:
*      This function serializes all source property map into a description tag.
*          Input consists of input properties like,
*          SanPolicy   1
*          dhcp        1
*          output is of form SanPolicy:1,dhcp:1
* Input:
*       Map consisting of mismatching source property. Like if sanpolicy needed is 1 but it is 0.
* Returns:
*          Hydration Description.
*/
std::string HydrationConfigurator::SerializeDescriptionTag(std::map<std::string, std::string> systemStateMap)
{
    std::string     description;

    BOOST_FOREACH(auto it, systemStateMap) {
        if (!description.empty()) {
            description += ",";
        }
        description += it.first;
        description += "=";
        description += it.second;
    }

    return description;
}

/*
* Description:
*      This function serializes supported hosts tag.
*          Input consists of input properties like,
*          azure        {no_hydration, <source-property-description>}
*          output is of form
*          _azure,_aws
* Input:
*       Map consisting of supported and unsupported targets.
* Returns:
*          Supported target string.
*/
std::string HydrationConfigurator::SerializeSupportedTargetsProperty(std::map<std::string, boost::shared_ptr<HydrationInfo>> targetHydrationMap)
{
    std::string     supportedTargetsProperty;

    std::map<std::string, boost::shared_ptr<HydrationInfo>>::const_iterator it = targetHydrationMap.begin();
    for (; targetHydrationMap.end() != it; it++) {
        if (!supportedTargetsProperty.empty()) {
            supportedTargetsProperty += ",";
        }

        if (!it->second->bNeedsHydration) {
            supportedTargetsProperty += it->first;
        }
    }

    return supportedTargetsProperty;
}

