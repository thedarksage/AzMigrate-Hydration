#include "agentconfigobject.h"
namespace ConfSchema
{
    AgentConfigObject::AgentConfigObject()
    {
        m_pName = "AgentConfiguration";
        m_pHostIdAttrName = "HostId" ;
        m_pAgentVersionAttrName = "AgentVersion" ;
        m_pDriverVersionAttrNAme = "DriverVersion";
        m_phostNameAttrName = "HostName" ;
        m_pIpAddressAttrName = "IpAddress" ;
        m_pOsInfoAttrName = "OsInfo" ;
        m_pCompatibilityNoAttrName = "CompatibilityNo" ;
        m_pAgentInstallPathAttrName = "AgentInstallPath" ;
        m_pOsValAttrName = "OsValue" ;
        m_pTimeZoneAttrName = "TimeZone" ;
        m_pSystemVolumeAttrName = "SystemVolume" ;
        m_pAgentModeAttrName = "AgentMode" ;
        m_pProductVersionAttrName  = "ProductVersion" ;
        m_pEndianNessAttrName = "EndianNess";
        m_pProcessorArchitectureAttrName = "ProcessorArchitecture";

    }
    PatchHistoryObject::PatchHistoryObject()
    {
        m_pName = "PatchHistory" ;
        m_pPatchNameAttrName = "PatchName" ;
        m_pPatchPathAttrName = "PatchPath";
        m_pPatchInstallDateAttrName = "PatchInstallDate" ;
        m_pPatchProductAttrName = "PatchProduct" ;
    }
}
