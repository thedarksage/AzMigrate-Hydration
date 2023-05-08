#ifndef AGENT_CONFIG_OBJECT_H_
#define AGENT_CONFIG_OBJECT_H_

/* TODO: should not be using these names finally */

namespace ConfSchema
{
    class AgentConfigObject
    {
        const char * m_pName ;
        const char * m_pHostIdAttrName ;
        const char * m_pAgentVersionAttrName ;
        const char * m_pDriverVersionAttrNAme ;
        const char * m_phostNameAttrName ;
        const char * m_pIpAddressAttrName ;
        const char * m_pOsInfoAttrName ;
        const char * m_pCompatibilityNoAttrName ;
        const char * m_pAgentInstallPathAttrName ;
        const char * m_pOsValAttrName ;
        const char * m_pTimeZoneAttrName ;
        const char * m_pSystemVolumeAttrName ;
        const char * m_pAgentModeAttrName ;
        const char * m_pProductVersionAttrName ;
        const char * m_pEndianNessAttrName ;
        const char * m_pProcessorArchitectureAttrName ;
    public:
        AgentConfigObject() ;
        const char * GetName() ;
        const char * GetHostIdAttrName() ;
        const char * GetAgentVersionAttrName() ;
        const char * GetDriverVersionAttrName() ;
        const char * GetHostNameAttrName() ;
        const char * GetIpAddressAttrName() ;
        const char * GetOsInfoAttrName() ;
        const char * GetCompatibilityAttrName() ;
        const char * GetAgentInstallPathAttrName() ;
        const char * GetOsValAttrName() ;
        const char * GetTimeZoneAttrName() ;
        const char * GetSystemVolumeAttrName() ;
        const char * GetAgentModeAttrName() ;
        const char * GetProductVersionAttrName() ;
        const char * GetEndianNessAttrName() ;
        const char * GetProcessorArchitectureAttrName() ;
    } ;
    inline const char * AgentConfigObject::GetName()
    {
        return m_pName ;
    }
    inline const char * AgentConfigObject::GetHostIdAttrName()
    {
        return m_pHostIdAttrName ;
    }
    inline const char * AgentConfigObject::GetAgentVersionAttrName()
    {
        return m_pAgentVersionAttrName ;
    }
    inline const char * AgentConfigObject::GetDriverVersionAttrName()
    {
        return m_pDriverVersionAttrNAme ;
    }
    inline const char * AgentConfigObject::GetHostNameAttrName()
    {
        return m_phostNameAttrName ;
    }
    inline const char * AgentConfigObject::GetIpAddressAttrName()
    {
        return m_pIpAddressAttrName ;
    }
    inline const char * AgentConfigObject::GetOsInfoAttrName()
    {
        return m_pOsInfoAttrName ;
    }
    inline const char * AgentConfigObject::GetCompatibilityAttrName()
    {
        return m_pCompatibilityNoAttrName ;
    }
    inline const char * AgentConfigObject::GetAgentInstallPathAttrName()
    {
        return m_pAgentInstallPathAttrName ;
    }
    inline const char * AgentConfigObject::GetOsValAttrName()
    {
        return m_pOsValAttrName ;
    }
    inline const char * AgentConfigObject::GetTimeZoneAttrName()
    {
        return m_pTimeZoneAttrName ;
    }
    inline const char * AgentConfigObject::GetSystemVolumeAttrName()
    {
        return m_pSystemVolumeAttrName ;
    }
    inline const char * AgentConfigObject::GetAgentModeAttrName()
    {
        return m_pAgentModeAttrName ;
    }
    inline const char * AgentConfigObject::GetProductVersionAttrName()
    {
        return m_pProductVersionAttrName ;
    }
    inline const char * AgentConfigObject::GetEndianNessAttrName()
    {
        return m_pEndianNessAttrName ;
    }
    inline const char * AgentConfigObject::GetProcessorArchitectureAttrName()
    {
        return m_pProcessorArchitectureAttrName ;
    }
    class PatchHistoryObject
    {
        const char * m_pName ;
        const char * m_pPatchNameAttrName ;
        const char * m_pPatchPathAttrName ;
        const char * m_pPatchInstallDateAttrName ;
        const char * m_pPatchProductAttrName ;
    public:
        PatchHistoryObject() ;
        const char * GetName() ;
        const char * GetPatchNameAttrName() ;
        const char * GetPatchPathAttrName() ;
        const char * GetPatchInstallDateAttrName() ;
        const char * GetPatchProductAttrName() ;
    } ;
    inline const char * PatchHistoryObject::GetName()
    {
        return m_pName ;
    }
    inline const char * PatchHistoryObject::GetPatchNameAttrName()
    {
        return m_pPatchNameAttrName ;
    }
    inline const char * PatchHistoryObject::GetPatchPathAttrName()
    {
        return m_pPatchPathAttrName ;
    }
    inline const char * PatchHistoryObject::GetPatchInstallDateAttrName()
    {
        return m_pPatchInstallDateAttrName ;
    }
    inline const char * PatchHistoryObject::GetPatchProductAttrName()
    {
        return m_pPatchProductAttrName ;
    }
}
#endif
