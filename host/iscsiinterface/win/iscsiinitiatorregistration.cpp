
//
//  File: iscsiinitiatorregistration.cpp
//
//  Description:
//

#include "atloc.h"
#include "iscsiinterface.h"
#include "configurevxagent.h"
#include "configurator2.h"
#include "logger.h"

bool registerIScsiInitiator()
{
    static Configurator* configurator = 0;
    static std::string registerName;
   
    try {                
        IScsi iScsi;
        std::string name;    
        iScsi.getIScsiInitiatorNodeName(name);
        if (name != registerName) {
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
