#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include "agentconfig.h"
#include "portablehelpers.h"
#include "InmsdkGlobals.h"
#include "confschemareaderwriter.h"


AgentConfigPtr AgentConfig::m_agentConfigPtr ;

AgentConfigPtr AgentConfig::GetInstance()
{
    if( !m_agentConfigPtr )
    {
        m_agentConfigPtr.reset( new AgentConfig() ) ;
    }
    m_agentConfigPtr->loadAgentConfigGroup();
    return m_agentConfigPtr ;
}

void AgentConfig::loadAgentConfigGroup()
{
    ConfSchemaReaderWriterPtr confSchemaReaderWriter = ConfSchemaReaderWriter::GetInstance() ;
    m_agentConfigGrp =  confSchemaReaderWriter->getGroup( m_agentConfigAttrNamesObj.GetName()) ;
}

AgentConfig::AgentConfig()
{
    m_isModified = false ;
}

void AgentConfig::GetAgentConfigObj(ConfSchema::Object** agentConfigObj)
{
    if( !m_agentConfigGrp->m_Objects.size() )
    {
        ConfSchema::Object agentConfigObj ;
        ConfSchema::Attrs_t& agentCfgAttrs = agentConfigObj.m_Attrs ;
        agentCfgAttrs.insert( std::make_pair(m_agentConfigAttrNamesObj.GetAgentVersionAttrName(), "")) ;
        agentCfgAttrs.insert( std::make_pair(m_agentConfigAttrNamesObj.GetAgentInstallPathAttrName(), "")) ;
        agentCfgAttrs.insert( std::make_pair(m_agentConfigAttrNamesObj.GetAgentModeAttrName(), "")) ;
        agentCfgAttrs.insert( std::make_pair(m_agentConfigAttrNamesObj.GetCompatibilityAttrName(), "")) ;
        agentCfgAttrs.insert( std::make_pair(m_agentConfigAttrNamesObj.GetDriverVersionAttrName(), "")) ;
        agentCfgAttrs.insert( std::make_pair(m_agentConfigAttrNamesObj.GetEndianNessAttrName(), "")) ;
        agentCfgAttrs.insert( std::make_pair(m_agentConfigAttrNamesObj.GetHostIdAttrName(), "")) ;
        agentCfgAttrs.insert( std::make_pair(m_agentConfigAttrNamesObj.GetHostNameAttrName(), "")) ;
        agentCfgAttrs.insert( std::make_pair(m_agentConfigAttrNamesObj.GetIpAddressAttrName(), "")) ;
        agentCfgAttrs.insert( std::make_pair(m_agentConfigAttrNamesObj.GetOsValAttrName(), "")) ;
        agentCfgAttrs.insert( std::make_pair(m_agentConfigAttrNamesObj.GetProcessorArchitectureAttrName(), "")) ;
        agentCfgAttrs.insert( std::make_pair(m_agentConfigAttrNamesObj.GetProductVersionAttrName(), "")) ;
        agentCfgAttrs.insert( std::make_pair(m_agentConfigAttrNamesObj.GetSystemVolumeAttrName(), "")) ;
        agentCfgAttrs.insert( std::make_pair(m_agentConfigAttrNamesObj.GetTimeZoneAttrName(), "")) ;
        m_agentConfigGrp->m_Objects.insert( std::make_pair( "agentconfig", agentConfigObj));
    }
    *agentConfigObj = &(m_agentConfigGrp->m_Objects.begin()->second) ;
}
void AgentConfig::GetPatchHistoryGrp(ConfSchema::Group** patchHistoryGrp)
{
    ConfSchema::Object* agentConfigObj ;
    GetAgentConfigObj(&agentConfigObj) ;
    if( agentConfigObj->m_Groups.find( m_patchHistoryAttrNamesObj.GetName() )
            == agentConfigObj->m_Groups.end() )
    {
        agentConfigObj->m_Groups.insert( std::make_pair( m_patchHistoryAttrNamesObj.GetName(), 
            ConfSchema::Group())) ;
    }
    *patchHistoryGrp = &(agentConfigObj->m_Groups.find( m_patchHistoryAttrNamesObj.GetName())->second) ;
}

void AgentConfig::UpdatePatchHistory(const std::string& patchHistory)
{
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;
    boost::char_separator<char> patchHistorySeperator("|");
    tokenizer_t patchHistoryTokens(patchHistory, patchHistorySeperator);
    tokenizer_t::iterator patchHistoryIterB(patchHistoryTokens.begin());
    tokenizer_t::iterator patchHistoryIterE(patchHistoryTokens.end());
    boost::char_separator<char> patchDetailSeperator(",") ;
    while( patchHistoryIterB != patchHistoryIterE )
    {
        const std::string& patchDetails = *patchHistoryIterB ;
        tokenizer_t patchDetailTokens(patchDetails, patchDetailSeperator);
        tokenizer_t::iterator patchDetailIterB(patchDetailTokens.begin());
        tokenizer_t::iterator patchDetailIterE(patchDetailTokens.end());
        int idx = 0 ;
        std::string patchName, patchInstallTime, patchDir, patchProduct ;
        while( patchDetailIterB != patchDetailIterE )
        {
            switch( ++idx )
            {
                case 1:
                    patchName = *patchDetailIterB ;
                    break ;
                case 2:
                    patchInstallTime = *patchDetailIterB ;
                    break ;
                case 3:
                     patchDir = *patchDetailIterB ;
                     break ;
                case 4 :
                    patchProduct = *patchDetailIterB ;;
                    break ;
            }
            if( idx == 4 )
            {
                ConfSchema::Group* patchHistoryGrp ;
                GetPatchHistoryGrp(&patchHistoryGrp) ;
                if( patchHistoryGrp->m_Objects.find( patchName ) == 
                        patchHistoryGrp->m_Objects.end() )
                {
                    ConfSchema::Object patchObj ;
                    patchObj.m_Attrs.insert( std::make_pair( m_patchHistoryAttrNamesObj.GetPatchInstallDateAttrName(),
                                patchInstallTime)) ;
                    patchObj.m_Attrs.insert( std::make_pair( m_patchHistoryAttrNamesObj.GetPatchNameAttrName(),
                                patchName)) ;
                    patchObj.m_Attrs.insert( std::make_pair( m_patchHistoryAttrNamesObj.GetPatchPathAttrName(),
                                patchDir)) ;
                    patchObj.m_Attrs.insert( std::make_pair( m_patchHistoryAttrNamesObj.GetPatchProductAttrName(),
                                patchProduct )) ;
                    patchHistoryGrp->m_Objects.insert( std::make_pair( patchName, patchObj)) ;
                }
            }

            patchDetailIterB++ ;
        }
        patchHistoryIterB++ ;
    }
}

void AgentConfig::UpdateAgentConfiguration(const std::string& hostId,
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
                                           const ParameterGroup& cpuinfopg )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object* agentConfigObj ;
    GetAgentConfigObj(&agentConfigObj) ;
    ConfSchema::Attrs_t& agentConfigAttrs = agentConfigObj->m_Attrs ;
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = agentConfigAttrs.find( m_agentConfigAttrNamesObj.GetAgentVersionAttrName()) ;
    attrIter->second = agentVersion ;
    attrIter = agentConfigAttrs.find( m_agentConfigAttrNamesObj.GetAgentInstallPathAttrName()) ;
    attrIter->second = agentInstallPath ;
    attrIter = agentConfigAttrs.find( m_agentConfigAttrNamesObj.GetAgentModeAttrName()) ;
    attrIter->second = agentMode ;
    attrIter = agentConfigAttrs.find( m_agentConfigAttrNamesObj.GetCompatibilityAttrName()) ;
    attrIter->second = boost::lexical_cast<std::string>(compatibilityNo);
    attrIter = agentConfigAttrs.find( m_agentConfigAttrNamesObj.GetDriverVersionAttrName()) ;
    attrIter->second = driverVersion ;
    attrIter = agentConfigAttrs.find( m_agentConfigAttrNamesObj.GetEndianNessAttrName()) ;
    attrIter->second = boost::lexical_cast<std::string>( e ) ;
    attrIter = agentConfigAttrs.find( m_agentConfigAttrNamesObj.GetHostIdAttrName()) ;
    attrIter->second = hostId ;
    attrIter = agentConfigAttrs.find( m_agentConfigAttrNamesObj.GetHostNameAttrName()) ;
    attrIter->second = hostname ;
    attrIter = agentConfigAttrs.find( m_agentConfigAttrNamesObj.GetIpAddressAttrName()) ;
    attrIter->second = ipaddress ; 
    /*attrIter= agentConfigAttrs.find( m_agentConfigAttrNamesObj.GetOsInfoAttrName()) ;
    attrIter->second = ospg ;*/
    attrIter = agentConfigAttrs.find( m_agentConfigAttrNamesObj.GetOsValAttrName()) ;
    attrIter->second = boost::lexical_cast<std::string>(osVal) ;
    /*attrIter = agentConfigAttrs.find( m_agentConfigAttrNamesObj.GetProcessorArchitectureAttrName()) ;
    attrIter->second = cpuinfopg ;*/
    ConstParameterGroupsIter_t cpuInfoPgIter ; 
    cpuInfoPgIter = cpuinfopg.m_ParamGroups.begin() ;
    ConfSchema::Group cpuInfogrp ;
    while( cpuInfoPgIter != cpuinfopg.m_ParamGroups.end() )
    {
        std::string cpuId = cpuInfoPgIter->first ;
        ConstParameterGroupsIter_t  cpuAttrpgIter ;
        cpuAttrpgIter = cpuInfoPgIter->second.m_ParamGroups.find( "m_Attributes" ) ;
        if( cpuAttrpgIter != cpuInfoPgIter->second.m_ParamGroups.end() )
        {
            const ParameterGroup cpuAttrsPg = cpuAttrpgIter->second ;
            std::string arch, manufacturer, name, cores, logicalProcessors ;
            ConstParametersIter_t paramIter ;
            paramIter = cpuAttrsPg.m_Params.find( "architecture" ) ;
            if( paramIter != cpuAttrsPg.m_Params.end() ) {
                arch = paramIter->second.value ;
            }
            paramIter = cpuAttrsPg.m_Params.find( "manufacturer" ) ;
            if( paramIter != cpuAttrsPg.m_Params.end() ) {
                 manufacturer = paramIter->second.value ;
            }
            paramIter = cpuAttrsPg.m_Params.find( "name" ) ;
            if( paramIter != cpuAttrsPg.m_Params.end() ) {
                 name = paramIter->second.value ;
            }
            paramIter = cpuAttrsPg.m_Params.find( "number_of_cores" ) ;
            if( paramIter != cpuAttrsPg.m_Params.end() ) {
					cores =  paramIter->second.value ;
            }
			else 
			{
				cores = "0";
			}
            paramIter = cpuAttrsPg.m_Params.find( "number_of_logical_processors" ) ;
            if( paramIter != cpuAttrsPg.m_Params.end() ) {
					logicalProcessors = paramIter->second.value ;
            }
			else
			{
				logicalProcessors = "0";
			}
            ConfSchema::Object perCpuAttrObj ;
            
            perCpuAttrObj.m_Attrs.insert( std::make_pair( "cpuId", cpuId )) ;
            perCpuAttrObj.m_Attrs.insert( std::make_pair( "architecture", arch )) ;
            perCpuAttrObj.m_Attrs.insert( std::make_pair( "manufacturer", manufacturer )) ;
            perCpuAttrObj.m_Attrs.insert( std::make_pair( "name" , name )) ;
            perCpuAttrObj.m_Attrs.insert( std::make_pair( "number_of_cores" , cores )) ;
            perCpuAttrObj.m_Attrs.insert( std::make_pair( "number_of_logical_processors" , logicalProcessors )) ;
            cpuInfogrp.m_Objects.insert( std::make_pair( cpuId, perCpuAttrObj )  ) ;
        }
        cpuInfoPgIter++ ;
    }
    agentConfigObj->m_Groups.insert(std::make_pair( "CPU Info", cpuInfogrp)) ;
    ConfSchema::Group osInfogrp ;
    if( ospg.m_ParamGroups.find( "m_Attributes" ) != ospg.m_ParamGroups.end() )
    {
        ParameterGroup osInfoAttrPg = ospg.m_ParamGroups.find( "m_Attributes")->second ;
        ConstParametersIter_t paramIter ;
        std::string build, caption, major_version, minor_version, name ;
        paramIter = osInfoAttrPg.m_Params.find( "build" ) ;
        if( paramIter != osInfoAttrPg.m_Params.end() ) {
            build = osInfoAttrPg.m_Params.find( "build" )->second.value ;
        }
        paramIter = osInfoAttrPg.m_Params.find( "caption" ) ;
        if( paramIter != osInfoAttrPg.m_Params.end() ) {
            caption =  osInfoAttrPg.m_Params.find( "caption" )->second.value ;
        }
        paramIter = osInfoAttrPg.m_Params.find( "major_version" ) ;
        if( paramIter != osInfoAttrPg.m_Params.end() ) {
            major_version =  osInfoAttrPg.m_Params.find( "major_version" )->second.value ;
        }
        paramIter = osInfoAttrPg.m_Params.find( "minor_version" ) ;
        if( paramIter != osInfoAttrPg.m_Params.end() ) {
            minor_version =  osInfoAttrPg.m_Params.find( "minor_version" )->second.value ;
        }
        paramIter = osInfoAttrPg.m_Params.find( "name" ) ;
        if( paramIter != osInfoAttrPg.m_Params.end() ) {
            name =  osInfoAttrPg.m_Params.find( "name" )->second.value ;
        }
        ConfSchema::Object osAttrObj ;
        osAttrObj.m_Attrs.insert( std::make_pair( "build", build )) ;
        osAttrObj.m_Attrs.insert( std::make_pair( "caption", caption )) ;
        osAttrObj.m_Attrs.insert( std::make_pair( "major_version", major_version )) ;
        osAttrObj.m_Attrs.insert( std::make_pair( "minor_version", minor_version )) ;
        osAttrObj.m_Attrs.insert( std::make_pair( "name", name )) ;
        osInfogrp.m_Objects.insert( std::make_pair( "OS Attributes", osAttrObj)) ;
    }
    agentConfigObj->m_Groups.insert(std::make_pair( "OS Info", osInfogrp)) ;
    
    
    attrIter = agentConfigAttrs.find( m_agentConfigAttrNamesObj.GetProductVersionAttrName()) ;
    attrIter->second = prod_version ;
    attrIter = agentConfigAttrs.find( m_agentConfigAttrNamesObj.GetSystemVolumeAttrName()) ;
    attrIter->second = sysVol ;
    attrIter = agentConfigAttrs.find( m_agentConfigAttrNamesObj.GetTimeZoneAttrName()) ;
    attrIter->second = zone ;
    UpdatePatchHistory(patchHistory) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

std::string AgentConfig::GetAgentVersion()
{
    ConfSchema::Object* agentConfigObj ;
    GetAgentConfigObj(&agentConfigObj) ;
    ConfSchema::AttrsIter_t attrIter ;
    std::string agentVersion ;
    attrIter = agentConfigObj->m_Attrs.find( m_agentConfigAttrNamesObj.GetAgentVersionAttrName() ) ;
    if( attrIter != agentConfigObj->m_Attrs.end() )
    {
        if( attrIter->second.empty() )
        {
            DebugPrintf(SV_LOG_ERROR, "Agent Registration is not done yet. cannot determine agent version\n") ;
        }
        agentVersion = attrIter->second ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Couldn't determine the agent version\n") ;
    }
    return agentVersion ;
}

void AgentConfig::GetCPUInfo(std::list<std::map<std::string, std::string> >& cpuInfos)
{
    ConfSchema::Object* agentConfigObj ;
    GetAgentConfigObj(&agentConfigObj) ;
    ConfSchema::Group cpuInfoGrp ;
    if( agentConfigObj->m_Groups.find( "CPU Info" ) != agentConfigObj->m_Groups.end() )
    {
        cpuInfoGrp = agentConfigObj->m_Groups.find( "CPU Info" )->second ;
        ConfSchema::ObjectsIter_t cpuInfoObjIter = cpuInfoGrp.m_Objects.begin() ;
        while( cpuInfoObjIter != cpuInfoGrp.m_Objects.end() )
        {
            std::map<std::string, std::string> cpuinfoattributesMap ;
            ConfSchema::Object& perCpuAttrObj = cpuInfoObjIter->second ;
            cpuinfoattributesMap.insert( *(perCpuAttrObj.m_Attrs.find( "cpuId" )));
            cpuinfoattributesMap.insert( *(perCpuAttrObj.m_Attrs.find( "architecture" ))) ;
            cpuinfoattributesMap.insert( *(perCpuAttrObj.m_Attrs.find( "manufacturer" ))) ;
            cpuinfoattributesMap.insert( *(perCpuAttrObj.m_Attrs.find( "name" ))) ;
            cpuinfoattributesMap.insert( *(perCpuAttrObj.m_Attrs.find( "number_of_cores" ))) ;
            cpuinfoattributesMap.insert( *(perCpuAttrObj.m_Attrs.find( "number_of_logical_processors" )));
            cpuInfos.push_back( cpuinfoattributesMap ) ;
            cpuInfoObjIter++ ;
        }
    }
}

void AgentConfig::GetOSInfo(std::map<std::string, std::string>& osInfoAttrMap)
{
    ConfSchema::Object* agentConfigObj ;
    GetAgentConfigObj(&agentConfigObj) ;
    ConfSchema::Group osInfogrp ;
    if( agentConfigObj->m_Groups.find( "OS Info" ) != agentConfigObj->m_Groups.end() )
    {
        osInfogrp = agentConfigObj->m_Groups.find( "OS Info" )->second ;
        ConfSchema::ObjectsIter_t osInfoObjIter = osInfogrp.m_Objects.begin() ;
        if( osInfoObjIter != osInfogrp.m_Objects.end() )
        {
            ConfSchema::Object& osAttrObj = osInfoObjIter->second ;
            osInfoAttrMap.insert(*(osAttrObj.m_Attrs.find("build"))) ;
            osInfoAttrMap.insert(*(osAttrObj.m_Attrs.find( "caption"))) ;
            osInfoAttrMap.insert(*(osAttrObj.m_Attrs.find( "major_version"))) ;
            osInfoAttrMap.insert(*(osAttrObj.m_Attrs.find( "minor_version"))) ;
            osInfoAttrMap.insert(*(osAttrObj.m_Attrs.find( "name"))) ;
        }
    }
}

std::string AgentConfig::getHostId()
{
	ConfSchema::Object* agentConfigObj ;
	GetAgentConfigObj(&agentConfigObj) ;
	ConfSchema::AttrsIter_t attrIter ;
	std::string hostId ;
	attrIter = agentConfigObj->m_Attrs.find( m_agentConfigAttrNamesObj.GetHostIdAttrName() ) ;
	if( attrIter != agentConfigObj->m_Attrs.end() )
	{
		if( attrIter->second.empty() )
		{
			DebugPrintf(SV_LOG_ERROR, "Agent Registration is not done yet. cannot determine Host Id\n") ;
		}
		hostId = attrIter->second ;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Couldn't determine the agent version\n") ;
	}
	return hostId ;
}

std::string AgentConfig::getInstallPath()
{

	ConfSchema::Object* agentConfigObj ;
	GetAgentConfigObj(&agentConfigObj) ;
	ConfSchema::AttrsIter_t attrIter ;
	std::string installpath ;
	attrIter = agentConfigObj->m_Attrs.find( m_agentConfigAttrNamesObj.GetAgentInstallPathAttrName() ) ;
	if( attrIter != agentConfigObj->m_Attrs.end() )
	{
		if( attrIter->second.empty() )
		{
			DebugPrintf(SV_LOG_ERROR, "Agent Registration is not done yet. cannot determine install path\n") ;
		}
		installpath = attrIter->second ;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Couldn't determine the agent version\n") ;
	}
	return installpath ;
}
std::string AgentConfig::GetSystemVol()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	ConfSchema::Object* agentConfigObj ;
	GetAgentConfigObj(&agentConfigObj) ;
	ConfSchema::AttrsIter_t attrIter ;
	std::string systemdir ;
	attrIter = agentConfigObj->m_Attrs.find( m_agentConfigAttrNamesObj.GetSystemVolumeAttrName() ) ;
	if( attrIter != agentConfigObj->m_Attrs.end() )
	{
		if( attrIter->second.empty() )
		{
			DebugPrintf(SV_LOG_ERROR, "Agent Registration is not done yet. cannot determine install path\n") ;
		}
		systemdir = attrIter->second ;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Couldn't determine the system directory\n") ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return systemdir ;
}
