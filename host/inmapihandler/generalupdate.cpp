#include "generalupdate.h"
#include "logger.h"
#include "portable.h"
#include "xmlizevolumegroupsettings.h"
#include "inmstrcmp.h"

GeneralUpdater::GeneralUpdater()
{
}


/* TODO: add return codes ? */
/* TODO: change name of apptoschemaattrs to appschemaattrsmap 
 *       because update can be from apptoschema or schematoapp */
//void GeneralUpdater::UpdatePgToSchema(const char *key, const ParameterGroup &pg, 
//                                      ConfSchema::Group &group, AppToSchemaAttrs_t &apptoschemaattrs)
//{
//    ConstParametersIter_t pgnameiter = pg.m_Params.find(key);
//    if (pg.m_Params.end() == pgnameiter)
//    {
//        DebugPrintf(SV_LOG_ERROR, "could not find name %s in volumes parameter group\n", key);
//    }
//    else
//    {
//        ConfSchema::ObjectsIter_t objiter = find_if(group.m_Objects.begin(), group.m_Objects.end(), 
//                                                   ConfSchema::EqAttr(apptoschemaattrs[key], pgnameiter->second.value.c_str()));
//        ConfSchema::Object *pobject;
//        if (group.m_Objects.end() != objiter)
//        {
//            /* TODO: comment debug prints except errors */
//            DebugPrintf(SV_LOG_DEBUG, "key name %s, present\n", pgnameiter->second.value.c_str());
//            ConfSchema::Object &o = *objiter;
//            pobject = &o;
//        }
//        else
//        {
//            DebugPrintf(SV_LOG_DEBUG, "key name %s, not present\n", pgnameiter->second.value.c_str());
//            ConfSchema::Object o;
//            ConfSchema::ObjectsIter_t niter = group.m_Objects.insert(group.m_Objects.end(), o);
//            ConfSchema::Object &no = *niter;
//            pobject = &no;
//        }
//        CopyAttrsFromPgToSchema(pg, *pobject, apptoschemaattrs);
//    }
//}


//void GeneralUpdater::CopyAttrsFromParameterToSchema(const Parameters_t &paramiter, ConfSchema::Object &o, 
//                                             AppToSchemaAttrs_t &apptoschemaattrs)
//    {
//    for (ConstAppToSchemaAttrsIter_t i = apptoschemaattrs.begin(); i != apptoschemaattrs.end(); i++)
//    {
//        ConstParametersIter_t iter = paramiter.find(i->first);
//        if (iter != paramiter.end())
//        {
//            const ValueType &vt = iter->second;
//            o.m_Attrs.insert(std::make_pair(i->second, vt.value));
//        }
//    }
//}


void GeneralUpdater::CopyAttrsFromPgToSchema(const ParameterGroup &pg, ConfSchema::Object &o, 
                                             AppToSchemaAttrs_t &apptoschemaattrs)
{
    bool bGeneralVolume = false ;
    for (ConstAppToSchemaAttrsIter_t i = apptoschemaattrs.begin(); i != apptoschemaattrs.end(); i++)
    {
        if( InmStrCmp<NoCaseCharCmp>(i->first, NS_VolumeSummary::type) == 0 )
        {
            if( (pg.m_Params.find(i->first) != pg.m_Params.end()) && 
                InmStrCmp<NoCaseCharCmp>(pg.m_Params.find(i->first)->second.value, "5") == 0 )
            {
                bGeneralVolume = true ;
                break ;
            }
        }
    }
    for (ConstAppToSchemaAttrsIter_t i = apptoschemaattrs.begin(); i != apptoschemaattrs.end(); i++)
    {
        ConstParametersIter_t iter = pg.m_Params.find(i->first);
        if (iter != pg.m_Params.end())
        {
            const ValueType &vt = iter->second;
            ConfSchema::AttrsIter_t attrIter = o.m_Attrs.find(i->second);
            if(attrIter != o.m_Attrs.end())
            {
                if( ( InmStrCmp<NoCaseCharCmp>(i->first, NS_VolumeSummary::capacity) == 0  ||
                    InmStrCmp<NoCaseCharCmp>(i->first, NS_VolumeSummary::rawcapacity) == 0 ) && 
                     bGeneralVolume && InmStrCmp<NoCaseCharCmp>(vt.value, "0") == 0 )
                {
                    DebugPrintf(SV_LOG_DEBUG, "Not updating the capacity/rawsize when the value is 0 and volume type is 5\n") ;
                }
                else
                {
                    attrIter->second = vt.value;
                }
            }
            else
            {
                const ValueType &vt = iter->second;
                o.m_Attrs.insert(std::make_pair(i->second, vt.value));
            }
        }
    }
}

void GeneralUpdater::CopyAttrsFromSchemaToPg(ParameterGroup &pg, const ConfSchema::Object &o, 
                                                SchemaToAppAttrs_t &schematoappattrs)
{
    for (ConstSchemaToAppAttrsIter_t i = schematoappattrs.begin(); i != schematoappattrs.end(); i++)
    {
        ConfSchema::ConstAttrsIter_t iter = o.m_Attrs.find(i->first);   
        if (iter != o.m_Attrs.end())
        {
            ValueType vt;
            vt.value= iter->second;
            pg.m_Params.insert(std::make_pair(i->second, vt));
        }
    }
}
