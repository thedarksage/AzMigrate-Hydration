#include "confschema.h"
#include "logger.h"
#include "portable.h"

namespace ConfSchema
{
    void Object::Print(void)
    {
        DebugPrintf(SV_LOG_DEBUG, "object -- start\n");
        DebugPrintf(SV_LOG_DEBUG, "attributes:\n");
        for (AttrsIter_t i = m_Attrs.begin(); i != m_Attrs.end(); ++i)
        {
            DebugPrintf(SV_LOG_DEBUG, "name: %s\n", i->first.c_str());
            DebugPrintf(SV_LOG_DEBUG, "value: %s\n", i->second.c_str());
        }
        DebugPrintf(SV_LOG_DEBUG, "groups:\n");
        for (GroupsIter_t i = m_Groups.begin(); i != m_Groups.end(); ++i)
        {
            DebugPrintf(SV_LOG_DEBUG, "type: %s\n", i->first.c_str());
            i->second.Print();
        }
        DebugPrintf(SV_LOG_DEBUG, "object -- end\n");
    }

    void Reference::Print(void)
    {
        DebugPrintf(SV_LOG_DEBUG, "reference -- start\n");
        DebugPrintf(SV_LOG_DEBUG, "source: %s\n", m_Source.c_str());
        DebugPrintf(SV_LOG_DEBUG, "reference attributes:\n");
        for (ReferenceAttrsIter_t i = m_ReferenceAttrs.begin(); i != m_ReferenceAttrs.end(); ++i)
        {
            DebugPrintf(SV_LOG_DEBUG, "referer: %s\n", i->first.c_str());
            DebugPrintf(SV_LOG_DEBUG, "reference: %s\n", i->second.c_str());
        }
        DebugPrintf(SV_LOG_DEBUG, "reference -- end\n");
    }
    
    
    void Group::Print(void)
    {
        DebugPrintf(SV_LOG_DEBUG, "group -- start\n");
        DebugPrintf(SV_LOG_DEBUG, "objects:\n");
        for (ObjectsIter_t i = m_Objects.begin(); i != m_Objects.end(); ++i)
        {
			i->second.Print();
           // i->Print();
        }
        DebugPrintf(SV_LOG_DEBUG, "references:\n");
        for (ReferencesIter_t i = m_References.begin(); i != m_References.end(); ++i)
        {
            DebugPrintf(SV_LOG_DEBUG, "type: %s\n", i->first.c_str());
            i->second.Print();
        }
        DebugPrintf(SV_LOG_DEBUG, "group -- end\n");
    }
    bool Object::operator==( Object const& obj) const
    {
        if( m_Groups == obj.m_Groups && 
            m_ObjAttrbGrps == obj.m_ObjAttrbGrps && 
            m_Attrs == obj.m_Attrs )
        {
            return true ;
        }
        else
        {
            return false ;
        }
    }

    bool Group::operator==( Group const& grp) const
    {
        if( m_Objects == grp.m_Objects &&
            m_References == grp.m_References )
        {
            return true ;
        }
        else
        {
            return false ;
        }
    }
    bool AttributeGroup::operator==( AttributeGroup const& attrbGrp) const
    {
        if( m_Attrs == attrbGrp.m_Attrs && 
            m_AttrbGroups == attrbGrp.m_AttrbGroups )
        {
            return true ;
        }
        else
        {
            return false ;
        }
    }
    bool Reference::operator==( Reference const& ref) const
    {
        if( ref.m_ReferenceAttrs == m_ReferenceAttrs )
        {
            return true ;
        }
        else
        {
            return false ;
        }
    }
}

