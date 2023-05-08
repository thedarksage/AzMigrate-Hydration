#ifndef AGENT_CONFIG_H
#define AGENT_CONFIG_H
#include <boost/shared_ptr.hpp>
#include "agentconfigobject.h"
#include "confschema.h"
#include "svtypes.h"
#include "portablehelpers.h"
#include "InmXmlParser.h"
class AgentConfig ;
typedef boost::shared_ptr<AgentConfig> AgentConfigPtr ;

class AgentConfig
{
    ConfSchema::Group* m_agentConfigGrp ;
    ConfSchema::PatchHistoryObject m_patchHistoryAttrNamesObj ;
    ConfSchema::AgentConfigObject m_agentConfigAttrNamesObj ;
    bool m_isModified ;
    AgentConfig() ;
    static AgentConfigPtr m_agentConfigPtr ;
    void loadAgentConfigGroup() ;
    void GetAgentConfigObj(ConfSchema::Object** agentConfigObj) ;
    void UpdatePatchHistory(const std::string& patchHistory) ;
    void GetPatchHistoryGrp(ConfSchema::Group** patchHistoryGrp) ;
public:
    bool isModified() ;
    static AgentConfigPtr GetInstance() ;
    void UpdateAgentConfiguration(const std::string& hostId,
                                    const std::string& agentVersion,
                                    const std::string& driverVersion,
                                    const std::string& hostname,
                                    const std::string& ipaddress,
                                    const ParameterGroup& ospg,
                                    SV_ULONGLONG compatibilityNo,
                                    const std::string& agentInstallPath,
                                    OS_VAL osVal,
                                    const std::string& zone,
                                    const std::string& sysVol,
                                    const std::string& patchHistory,
                                    const std::string& agentMode,
                                    const std::string& prod_version,
                                    ENDIANNESS e,
                                    const ParameterGroup& cpuinfopg ) ;
    std::string GetAgentVersion() ;
    void GetCPUInfo(std::list<std::map<std::string, std::string> >& cpuInfos) ;
    void GetOSInfo(std::map<std::string, std::string>& osInfo) ;
	std::string getHostId() ;
	std::string getInstallPath() ;
	std::string GetSystemVol() ;
} ;
#endif
