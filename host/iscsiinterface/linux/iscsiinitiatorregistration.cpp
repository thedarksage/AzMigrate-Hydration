
//
//  File: iscsiinitiatorregistration.cpp
//
//  Description:
//

#include <fstream>
#include <cerrno>
#include <string>

#include "atloc.h"
#include "configurevxagent.h"
#include "configurator2.h"
#include "logger.h"

int getIScsiInitiatorNodeName(std::string& nodeName)
{
    std::ifstream initiatorNameFile("/etc/iscsi/initiatorname.iscsi");
    if (!initiatorNameFile) {
        if (ENOENT == errno) {
            return 0;
        }
        return errno;
    }

    std::string tag;

    std::getline(initiatorNameFile, tag, '=');
    std::getline(initiatorNameFile, nodeName, '\n');

    return 0;
}

bool registerIScsiInitiator()
{
    static Configurator* configurator = 0;
    static std::string registerName;

    try {
        std::string name;
        if (0 != getIScsiInitiatorNodeName(name)) {
            return false;
        }

        if (!name.empty() && name != registerName) {
            if (!InitializeConfigurator(&configurator, USE_ONLY_CACHE_SETTINGS, PHPSerialize)) {
                DebugPrintf(SV_LOG_ERROR, "InitializeConfigurator failed\n");
                return false;
            }
            
            registerName = name;
            configurator->getVxAgent().registerIScsiInitiatorName(registerName);
        }
        return true;

    } catch (std::exception& e) {
        DebugPrintf(SV_LOG_WARNING, "%s failed: %s\n", CATCH_LOC, e.what());
    }

    return false;
}
