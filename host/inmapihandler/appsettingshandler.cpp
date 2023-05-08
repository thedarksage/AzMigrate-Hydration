#include <utility>
#include "appsettingshandler.h"
#include "apinames.h"
#include "inmstrcmp.h"
#include "volumegroupsettings.h"
#include "portablehelpers.h"
#include "inmdefines.h"
#include "configurecxagent.h"
#include "confengine/confschemareaderwriter.h"
#include "apppolicyobject.h"
#include "generalupdate.h"
#include "xmlizeapplicationsettings.h"
#include "serializeapplicationsettings.h"
#include "util.h"
#include "inmsdkexception.h"
#include "inmsdkutil.h"

AppSettingsHandler::AppSettingsHandler(void):
m_pProtectionPairGroup(0), m_pPlanGroup(0),m_pAppPolicyGroup(0),m_pAppPolicyInstanceGroup(0)
{
}
AppSettingsHandler::~AppSettingsHandler(void)
{
}
bool AppSettingsHandler::GetPolicyObjsGroup(ConfSchema::Group* appPolicyGroup,
                                            ConfSchema::Group** policyObjsGrp)
{
    bool bRet = false ;
    if( appPolicyGroup->m_Objects.size() )
    {
        ConfSchema::Object& apppolicyConfigObj = appPolicyGroup->m_Objects.begin()->second ;
        if( apppolicyConfigObj.m_Groups.find( "PolicyObjects" ) != 
            apppolicyConfigObj.m_Groups.end() )
        {
            *policyObjsGrp = &(apppolicyConfigObj.m_Groups.find( "PolicyObjects" )->second) ;
            bRet = true ;
        }
    }
    return bRet ;
}
INM_ERROR AppSettingsHandler::process(FunctionInfo& f)
{
	Handler::process(f);
	INM_ERROR errCode = E_SUCCESS;

	if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, GET_APP_SETTINGS) == 0)
	{
		errCode = GetApplicationSettings(f);
        
	}
	else
	{

	}

	return Handler::updateErrorCode(errCode);
}


INM_ERROR AppSettingsHandler::validate(FunctionInfo& f)
{
	INM_ERROR errCode = Handler::validate(f);
	if(E_SUCCESS != errCode)
		return errCode;
	return errCode;
}

INM_ERROR AppSettingsHandler::GetApplicationSettings(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    INM_ERROR errCode = E_UNKNOWN;
    try
    {
        ValidateAndUpdateRequestPGForApplicationSettings(f);
        errCode = E_SUCCESS;
    }
    catch(GroupNotFoundException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(InvalidArgumentException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(SchemaReadFailedException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(SchemaUpdateFailedException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
     catch(PairNotFoundException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(AttributeNotFoundException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(ContextualException &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    catch(const boost::bad_lexical_cast &ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Got Exception : %s\n", ex.what()) ;
    }
    //TODO: set proper error code in each exception. For now SUCCESS and FAILED
    insertintoreqresp(f.m_ResponsePgs, cxArg ( errCode ), "1");
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return errCode;
}

void AppSettingsHandler::ValidateAndUpdateRequestPGForApplicationSettings(FunctionInfo &f)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;   
    std::stringstream stream;
    const char * const INDEX[] = {"2"};
    ConstParameterGroupsIter_t policyTimeStampPGIter = f.m_RequestPgs.m_ParamGroups.find(INDEX[0]);
    if (f.m_RequestPgs.m_ParamGroups.end() != policyTimeStampPGIter)
    {
        DebugPrintf(SV_LOG_DEBUG,"FunctionInfo contains valid policy time stamp pg at index %s\n",INDEX[0]);
    }
        
    ParameterGroup responsePG;
    ReadGroupsAndFillApplicationSettings(policyTimeStampPGIter,responsePG);
    f.m_ResponsePgs.m_ParamGroups.insert(std::make_pair("1", responsePG));
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}
void AppSettingsHandler::ReadGroupsAndFillApplicationSettings(ConstParameterGroupsIter_t & policyTimeStampPGIter,ParameterGroup & responsePG)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    std::stringstream stream;
    ConfSchema::ApplicationPolicyObject appPolicyObj;
    const char *pPolicyObjectName = m_AppPolicyObj.GetName();
    const char *pPlanObjectName = m_PlanObj.GetName();
    const char *pProtectionPairName   = m_ProtectionPairObj.GetName();
    const char *pPolicyInstanceObjName = m_AppPolicyInstanceObj.GetName();
    ConfSchemaReaderWriterPtr confschemaRdrWrtr = ConfSchemaReaderWriter::GetInstance() ;
    m_pAppPolicyGroup = confschemaRdrWrtr->getGroup(pPolicyObjectName) ;
    m_pPlanGroup = confschemaRdrWrtr->getGroup(pPlanObjectName) ;
    m_pProtectionPairGroup = confschemaRdrWrtr->getGroup(pProtectionPairName) ;
    m_pAppPolicyInstanceGroup = confschemaRdrWrtr->getGroup(pPolicyInstanceObjName);


    FillApplicationSettings(policyTimeStampPGIter,responsePG);
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void AppSettingsHandler::FillApplicationSettings(ConstParameterGroupsIter_t & policyTimeStampPGIter,ParameterGroup & pg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    pg.m_Params.insert(std::make_pair(NS_ApplicationSettings::m_version, ValueType("string","1.0")));

    std::pair<ParameterGroupsIter_t, bool> appPoliciesPair = pg.m_ParamGroups.insert(std::make_pair(NS_ApplicationSettings::m_policies, ParameterGroup()));
    ParameterGroupsIter_t &appPoliciesPGIter = appPoliciesPair.first;
    ParameterGroup &appPoliciesPG = appPoliciesPGIter->second;

    std::pair<ParameterGroupsIter_t, bool> protectionSettingsPair = pg.m_ParamGroups.insert(std::make_pair(NS_ApplicationSettings::m_AppProtectionsettings, ParameterGroup()));
    ParameterGroupsIter_t &protectionSettingsPairIter = protectionSettingsPair.first;
    ParameterGroup &protectionSettingsPG = protectionSettingsPairIter->second;

    unsigned long timeoutInSecs = 900;
    insertintoreqresp(pg, cxArg ( timeoutInSecs ), NS_ApplicationSettings::timeoutInSecs);

    pg.m_ParamGroups.insert(std::make_pair(NS_ApplicationSettings::remoteCxs, ParameterGroup()));

    FillApplicationPolicySettings(appPoliciesPG,policyTimeStampPGIter);
    FillApplicationProtectionSettings(protectionSettingsPG);
    
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}


void AppSettingsHandler::FillApplicationPolicySettings(ParameterGroup &appPolicyPG,ConstParameterGroupsIter_t &policyTimeStampPGIter)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    std::list<std::string> deletedpolicies;
    if (m_pAppPolicyGroup)
    {
        ConfSchema::Object_t::size_type i = 0;
        ConfSchema::Group* policyObjsGrp ;
        if( GetPolicyObjsGroup( m_pAppPolicyGroup, &policyObjsGrp) )
        {
            GetDeletedPoliciesList(*m_pAppPolicyGroup,policyTimeStampPGIter,deletedpolicies);
            for (ConfSchema::ObjectsIter_t oiter = policyObjsGrp->m_Objects.begin(); oiter != policyObjsGrp->m_Objects.end(); oiter++, i++)
            {
                ConfSchema::Object &o = oiter->second;
                UpdateApplicationPolicySettingsPG(i,o,appPolicyPG,policyTimeStampPGIter,deletedpolicies);
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"Policy group is empty\n");
        const ParameterGroup &policyTimeStampPG = policyTimeStampPGIter->second;
        ConstParametersIter_t paramIter  = policyTimeStampPG.m_Params.begin();
        int  srcIndex = 0;
        while(paramIter  != policyTimeStampPG.m_Params.end())
        {
            std::stringstream srcss;
            srcss << srcIndex;
            std::string pgName = NS_ApplicationSettings::m_policies;
            pgName += "[";
            pgName += srcss.str();
            pgName += "]";

            std::pair<ParameterGroupsIter_t, bool> appPolicyPair = appPolicyPG.m_ParamGroups.insert(std::make_pair(pgName, ParameterGroup()));
            ParameterGroupsIter_t &appPolicyIter = appPolicyPair.first;
            ParameterGroup &appPolicyParamGroup = appPolicyIter->second; 

            appPolicyParamGroup.m_Params.insert(std::make_pair(NS_ApplicationPolicy::m_version, ValueType("string","1.0")));

            std::pair<ParameterGroupsIter_t, bool> policyPropsPair = appPolicyPG.m_ParamGroups.insert(std::make_pair(NS_ApplicationPolicy::m_policyProperties, ParameterGroup()));
            ParameterGroupsIter_t &policyPropsIter = policyPropsPair.first;
            ParameterGroup &propertyPG = policyPropsIter->second;

            DebugPrintf(SV_LOG_DEBUG,"Setting scheduleType as deleted for policy 5s\n",paramIter->first.c_str());
            propertyPG.m_Params.insert(std::make_pair("PolicyId",ValueType("string",paramIter->first)));
            propertyPG.m_Params.insert(std::make_pair("ScheduleType",ValueType("string","-1")));

            std::string policyData;
            appPolicyParamGroup.m_Params.insert(std::make_pair(NS_ApplicationPolicy::m_policyData, ValueType("string","")));
            srcIndex++;
            paramIter++;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void AppSettingsHandler::GetDeletedPoliciesList(ConfSchema::Group &appPolicyGroup,ConstParameterGroupsIter_t &policyTimeStampPGIter,std::list<std::string> & deletedpolicies)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    std::string policyId;
    const ParameterGroup &policyTimeStampPG = policyTimeStampPGIter->second;
    ConstParametersIter_t paramIter  = policyTimeStampPG.m_Params.begin();
    ConfSchema::Group* policyObjsGrp ;
    if( GetPolicyObjsGroup(&appPolicyGroup, &policyObjsGrp) )
    {
        while(paramIter  != policyTimeStampPG.m_Params.end())
        {
			bool exists = false ;
            for (ConfSchema::ObjectsIter_t oiter = policyObjsGrp->m_Objects.begin(); oiter != policyObjsGrp->m_Objects.end(); oiter++)
            {
                    ConfSchema::Object &objIter = oiter->second;
                    FetchValueFromMap(objIter.m_Attrs,m_AppPolicyObj.GetIdAttrName(),policyId); 
                    const std::string & policyIdStr = paramIter->first;
                    if(policyId.compare(policyIdStr) == 0)
                    {
                    	exists = true ;
                        DebugPrintf(SV_LOG_DEBUG,"Policy present in schema\n");
                        break ;
                    }
            }
            if( !exists )
            {
				deletedpolicies.push_back(policyId);
			}
            paramIter++;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void AppSettingsHandler::UpdateApplicationPolicySettingsPG(int srcIndex,
                                                                ConfSchema::Object &objIter,
                                                                ParameterGroup & appPoliciesPG,
                                                                ConstParameterGroupsIter_t &policyTimeStampPGIter,
                                                                std::list<std::string> & deletedpolicies)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;

     std::stringstream srcss;
     srcss << srcIndex;
     std::string pgName = NS_ApplicationSettings::m_policies;
     pgName += "[";
     pgName += srcss.str();
     pgName += "]";

     std::pair<ParameterGroupsIter_t, bool> appPolicyPair = appPoliciesPG.m_ParamGroups.insert(std::make_pair(pgName, ParameterGroup()));
     ParameterGroupsIter_t &appPolicyIter = appPolicyPair.first;
     ParameterGroup &appPolicyPG = appPolicyIter->second; 
     
     appPolicyPG.m_Params.insert(std::make_pair(NS_ApplicationPolicy::m_version, ValueType("string","1.0")));

     std::pair<ParameterGroupsIter_t, bool> policyPropsPair = appPolicyPG.m_ParamGroups.insert(std::make_pair(NS_ApplicationPolicy::m_policyProperties, ParameterGroup()));
     ParameterGroupsIter_t &policyPropsIter = policyPropsPair.first;
     ParameterGroup &propertyPG = policyPropsIter->second;

     UpdateApplicationPolicyPropertiesPG(propertyPG,objIter,policyTimeStampPGIter,deletedpolicies);
     
     std::string policyData;
     FetchValueFromMap(objIter.m_Attrs,m_AppPolicyObj.GetPolicyDataAttrName(),policyData);
     appPolicyPG.m_Params.insert(std::make_pair(NS_ApplicationPolicy::m_policyData, ValueType("string",policyData)));
     
     DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void AppSettingsHandler::UpdateApplicationPolicyPropertiesPG(ParameterGroup &propertyPG,
                                                                  ConfSchema::Object &objIter,
                                                                  ConstParameterGroupsIter_t &policyTimeStampPGIter,
                                                                  std::list<std::string> & deletedpolicies)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    
    std::string policyId;
    std::string timeStamp;
    std::string policyStatus;
    std::string scheduleType;
    
    FetchValueFromMap(objIter.m_Attrs,m_AppPolicyObj.GetIdAttrName(),policyId);
    FetchValueFromMap(objIter.m_Attrs,m_AppPolicyObj.GetPolicyTimestampAttrName(),timeStamp);
    FetchValueFromMap(objIter.m_Attrs,m_AppPolicyObj.GetScheduleTypeAttrName(),scheduleType);

    bool bDeleted = false;
    std::list<std::string>::iterator listIter = deletedpolicies.begin();
    while(listIter != deletedpolicies.end())
    {
        if(policyId.compare(listIter->c_str()) == 0)
        {
            DebugPrintf(SV_LOG_DEBUG,"Marking policy as deleted\n");
            propertyPG.m_Params.insert(std::make_pair("ScheduleType",ValueType("string","-1")));
			propertyPG.m_Params.insert(std::make_pair("PolicyId",ValueType("string",policyId)));
            bDeleted = true;
            break;
        }
        listIter++;
    }
    if(bDeleted == false && scheduleType.compare("0") != 0 )
    {

    if(isPolicyNeedToSend(policyId,timeStamp,scheduleType,policyTimeStampPGIter))
    {
        propertyPG.m_Params.insert(std::make_pair("PolicyId",ValueType("string",policyId)));
        propertyPG.m_Params.insert(std::make_pair("Timestamp",ValueType("string",timeStamp)));
       
       
        std::string policyType;
        FetchValueFromMap(objIter.m_Attrs,m_AppPolicyObj.GetTypeAttrName(),policyType);
        propertyPG.m_Params.insert(std::make_pair("PolicyType",ValueType("string",policyType)));
       
        std::string valueStr;  
        FetchValueFromMap(objIter.m_Attrs,m_AppPolicyObj.GetScheduleTypeAttrName(),valueStr);
        propertyPG.m_Params.insert(std::make_pair("ScheduleType",ValueType("string",valueStr)));

        valueStr.clear();
        FetchValueFromMap(objIter.m_Attrs,m_AppPolicyObj.GetIntervalAttrName(),valueStr);
        propertyPG.m_Params.insert(std::make_pair("ScheduleInterval",ValueType("string",valueStr)));
        
        valueStr.clear();
        FetchValueFromMap(objIter.m_Attrs,m_AppPolicyObj.GetExcludeFromAttrName(),valueStr);
        propertyPG.m_Params.insert(std::make_pair("ExcludeFrom",ValueType("string",valueStr)));
        
        valueStr.clear();
        FetchValueFromMap(objIter.m_Attrs,m_AppPolicyObj.GetExcludeToAttrName(),valueStr);
        propertyPG.m_Params.insert(std::make_pair("ExcludeTo",ValueType("string",valueStr)));

        valueStr.clear();
        FetchValueFromMap(objIter.m_Attrs,m_AppPolicyObj.GetScenarioIdAttrName(),valueStr);
        propertyPG.m_Params.insert(std::make_pair("ScenarioType",ValueType("string",valueStr)));
        
        valueStr.clear();
        FetchValueFromMap(objIter.m_Attrs,m_AppPolicyObj.GetContextAttrName(),valueStr);
        propertyPG.m_Params.insert(std::make_pair("Context",ValueType("string",valueStr)));

        valueStr.clear();
        FetchValueFromMap(objIter.m_Attrs,m_AppPolicyObj.GetScenarioIdAttrName(),valueStr);
        propertyPG.m_Params.insert(std::make_pair("ScenarioId",ValueType("string",valueStr)));

        valueStr.clear();
        FetchValueFromMap(objIter.m_Attrs,m_AppPolicyObj.GetPolicyTimestampAttrName(),valueStr);
        propertyPG.m_Params.insert(std::make_pair("policyTimestamp",ValueType("string",valueStr)));
         
        
        if(policyType.compare("Consistency") == 0)
        {
            FetchValueFromMap(objIter.m_Attrs,m_AppPolicyObj.GetconsistencyCommandAttrName(),valueStr);
            propertyPG.m_Params.insert(std::make_pair("ConsistencyCmd",ValueType("string",valueStr)));
            try
            {
                valueStr.clear();
                FetchValueFromMap(objIter.m_Attrs,m_AppPolicyObj.GetDoNotSkipAttrName(),valueStr);
            }
            catch(ContextualException& ex)
            {
                valueStr = "0" ;
            }
            propertyPG.m_Params.insert(std::make_pair("DoNotSkip",ValueType("string",valueStr)));
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"Policy %s already present at agent and there is no change in settings",policyId.c_str());
    }     
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

bool AppSettingsHandler::isPolicyNeedToSend(const std::string & policyId,
                                            const std::string &timeStamp,
                                            const std::string &scheduleType,
                                            ConstParameterGroupsIter_t & policyTimeStampPGIter)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    bool bRet = false;
    std::string pgName = policyTimeStampPGIter->first;
    const ParameterGroup &policyTimeStampPG = policyTimeStampPGIter->second;
    ConstParametersIter_t paramIter  = policyTimeStampPG.m_Params.find(policyId);
    if(paramIter != policyTimeStampPG.m_Params.end())
    {
        const ValueType &vt = paramIter->second;
        if(timeStamp.compare(vt.value) == 0)
        {
            //policy exist at agent
            DebugPrintf(SV_LOG_DEBUG,"No change in policy setting\n");
        }
        else
        {
            //policy  exist at agent but modified
            DebugPrintf(SV_LOG_DEBUG, "Policy modified\n");
            bRet = true;

        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"Policy not present at agent\n");
        bRet = true;
    }
    if(scheduleType == "2")
    {
        DebugPrintf(SV_LOG_DEBUG,"Run once policy. Need to check policy instance table\n");
        if(m_pAppPolicyInstanceGroup)
        {
            if(bRet == true )
            {
                ConfSchema::Group &policyInstanceGroup = *m_pAppPolicyInstanceGroup;
                ConfSchema::ObjectsIter_t  policyInstanceObjIter = find_if(policyInstanceGroup.m_Objects.begin(),
                                                                    policyInstanceGroup.m_Objects.end(), 
                                                                    ConfSchema::EqAttr(m_AppPolicyInstanceObj.GetPolicyIdAttrName(),
                                                                                       policyId.c_str()));
                if(policyInstanceObjIter != policyInstanceGroup.m_Objects.end())
                {
                    std::string valueStr;
                    FetchValueFromMap(policyInstanceObjIter->second.m_Attrs,m_AppPolicyInstanceObj.GetStatusAttrName(),valueStr);
                    if(valueStr == "Failed" || valueStr == "Success" || valueStr == "Skipped")
                    {
                        DebugPrintf(SV_LOG_DEBUG,"Policy found instance table so no need send again\n");            
                        bRet = false;
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_DEBUG,"Policy status is not success or failed so sending\n");            
                        bRet = true;
                    }
                }
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG,"Policy instance table is not empty and policy is not present at agent\n");
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"For run every policy need not check policy instance table\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return bRet;
}

void AppSettingsHandler::FillApplicationProtectionSettings(ParameterGroup &protecionSettingsPG)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    if (m_pPlanGroup)
    {
        ConfSchema::Group &planGroup = *m_pPlanGroup;
        ConfSchema::Object_t::size_type i = 0;
        for (ConfSchema::ObjectsIter_t oiter = planGroup.m_Objects.begin(); oiter != planGroup.m_Objects.end(); oiter++)
        {
            ConfSchema::Object &planObj = oiter->second;
            ConfSchema::GroupsIter_t scenarioGroupIter = planObj.m_Groups.find(m_AppScenarionObj.GetName());
            ConfSchema::Group &scenarioGroup = scenarioGroupIter->second;
            for (ConfSchema::ObjectsIter_t scenarioIter  = scenarioGroup.m_Objects.begin(); scenarioIter != scenarioGroup.m_Objects.end(); scenarioIter++)
            {    
                ConfSchema::Object &secnarioObj = scenarioIter->second;                 
                UpdateApplicationProtectionSettingsPG(secnarioObj,protecionSettingsPG);
            }
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"Plan information not present in schema\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void AppSettingsHandler::UpdateApplicationProtectionSettingsPG(ConfSchema::Object &objIter,ParameterGroup & protecionSettingsPG)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    
    std::string scenarioId;
    FetchValueFromMap(objIter.m_Attrs,m_AppScenarionObj.GetScenarioIdAttrName(),scenarioId);

    std::pair<ParameterGroupsIter_t, bool> appProtecionPropsPair = protecionSettingsPG.m_ParamGroups.insert(std::make_pair(scenarioId, ParameterGroup()));
    ParameterGroupsIter_t &appProtectionPropsIter = appProtecionPropsPair.first;
    ParameterGroup &appPairProtecionSettingsPG = appProtectionPropsIter->second;

    std::pair<ParameterGroupsIter_t, bool> protecionsettingsPropsPair = appPairProtecionSettingsPG.m_ParamGroups.insert(std::make_pair(NS_AppProtectionSettings::m_properties, ParameterGroup()));
    ParameterGroupsIter_t &protectionsettingsPropsIter = protecionsettingsPropsPair.first;
    ParameterGroup &protecionpropertyPG = protectionsettingsPropsIter->second;

    appPairProtecionSettingsPG.m_Params.insert(std::make_pair(NS_AppProtectionSettings::m_version,ValueType("string","1.0")));
    
    appPairProtecionSettingsPG.m_Params.insert(std::make_pair(NS_AppProtectionSettings::srcDiscoveryInformation,ValueType("string","")));
    
    std::pair<ParameterGroupsIter_t, bool> protecionAppVolumesPair = appPairProtecionSettingsPG.m_ParamGroups.insert(std::make_pair(NS_AppProtectionSettings::appVolumes, ParameterGroup()));
    ParameterGroupsIter_t &protectionAppVolumesIter = protecionAppVolumesPair.first;
    ParameterGroup &protecionAppVolumesPG = protectionAppVolumesIter->second;

    std::pair<ParameterGroupsIter_t, bool> protecionPairInfoPair = appPairProtecionSettingsPG.m_ParamGroups.insert(std::make_pair(NS_AppProtectionSettings::Pairs, ParameterGroup()));
    ParameterGroupsIter_t &protectionPairIter = protecionPairInfoPair.first;
    ParameterGroup &protectionPairPG = protectionPairIter->second;

    appPairProtecionSettingsPG.m_ParamGroups.insert(std::make_pair(NS_AppProtectionSettings::CustomVolume,ParameterGroup()));
     
    appPairProtecionSettingsPG.m_ParamGroups.insert(std::make_pair(NS_AppProtectionSettings::instanceList,ParameterGroup()));
    
    UpdateApplicationProtectionPropertiesPG(protecionpropertyPG,objIter);
    
    FillApplicationProtecionAppVolumesPG(protecionAppVolumesPG);
    FillApplicationProtecionPairPG(protectionPairPG);
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}


void AppSettingsHandler::UpdateApplicationProtectionPropertiesPG(ParameterGroup &propertyPG,ConfSchema::Object &objIter)
{
    
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    std::string scenarioId;
    FetchValueFromMap(objIter.m_Attrs,m_AppScenarionObj.GetScenarioIdAttrName(),scenarioId);
    propertyPG.m_Params.insert(std::make_pair("scenarioId",ValueType("string",scenarioId)));
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}
void AppSettingsHandler::FillApplicationProtecionAppVolumesPG(ParameterGroup &protecionAppVolumesPG)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    if (m_pProtectionPairGroup)
    {
        ConfSchema::Group &protectionPairGroup = *m_pProtectionPairGroup;
        ConfSchema::Object_t::size_type i = 0;
        for (ConfSchema::ObjectsIter_t oiter = protectionPairGroup.m_Objects.begin(); oiter != protectionPairGroup.m_Objects.end(); oiter++, i++)
        {
            ConfSchema::Object &o = oiter->second;
            UpdateApplicationProtecionAppVolumesPG(i,o,protecionAppVolumesPG);
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;

}
void AppSettingsHandler::UpdateApplicationProtecionAppVolumesPG(int srcIndex,ConfSchema::Object & pairObject,ParameterGroup &protecionAppVolumesPG)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    std::stringstream srcss;
    std::string appVolumeName;
    FetchValueFromMap(pairObject.m_Attrs,m_ProtectionPairObj.GetSrcNameAttrName(),appVolumeName);
     
    srcss << srcIndex;
    std::string pgName = NS_AppProtectionSettings::appVolumes;
    pgName += "[";
    pgName += srcss.str();
    pgName += "]";
    protecionAppVolumesPG.m_Params.insert(std::make_pair(pgName,ValueType("string",appVolumeName)));
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}


void AppSettingsHandler::FillApplicationProtecionPairPG(ParameterGroup &protectionPairPG)
 {
     DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
     if (m_pProtectionPairGroup)
     {
         ConfSchema::Group &protectionPairGroup = *m_pProtectionPairGroup;
         ConfSchema::Object_t::size_type i = 0;
         ConfSchema::ProtectionPairObject propairAttrsObj ;
         for (ConfSchema::ObjectsIter_t oiter = protectionPairGroup.m_Objects.begin(); oiter != protectionPairGroup.m_Objects.end(); oiter++)
         {
             std::string value ;
             int executionState, replstate, pairState ;
             FetchValueFromMap(oiter->second.m_Attrs,propairAttrsObj.GetReplStateAttrName(),value);
             replstate = boost::lexical_cast<int>(value) ;
             FetchValueFromMap(oiter->second.m_Attrs,propairAttrsObj.GetExecutionStateAttrName(),value);
             executionState = boost::lexical_cast<int>(value) ;
             FetchValueFromMap(oiter->second.m_Attrs, propairAttrsObj.GetPairStateAttrName(), value ) ;
             pairState = boost::lexical_cast<int>(value) ;
             bool bNoPair = ( replstate == 7 && executionState == PAIR_IS_QUEUED ) || ( pairState == VOLUME_SETTINGS::PAUSE_TACK_COMPLETED ) ;
             
             if( !bNoPair )//Filling only if pair is not Queued (resync 1 and queued)
             {
                 ConfSchema::Object &o = oiter->second;
                 UpdateApplicationProtecionPairPG(i,o,protectionPairPG);
                 i++ ;
             }
         }
     }
     else
     {
         DebugPrintf(SV_LOG_DEBUG,"Protection pair group is empty\n");
     }
     DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
 }

void AppSettingsHandler::UpdateApplicationProtecionPairPG(int srcIndex,ConfSchema::Object & pairObject,ParameterGroup &protectionPairPG)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    std::string volumeName;
    FetchValueFromMap(pairObject.m_Attrs,m_ProtectionPairObj.GetSrcNameAttrName(),volumeName);
    std::pair<ParameterGroupsIter_t, bool> replicationPair = protectionPairPG.m_ParamGroups.insert(std::make_pair(volumeName,ParameterGroup()));
    ParameterGroupsIter_t &replicationIter = replicationPair.first;
    ParameterGroup &replicationPairPG = replicationIter->second;

    FillApplicationReplicationPairPG(replicationPairPG,pairObject);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

 void AppSettingsHandler::FillApplicationReplicationPairPG(ParameterGroup &replicationPairPG,ConfSchema::Object & pairObject)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    std::pair<ParameterGroupsIter_t, bool> replicationPairPropsPair = replicationPairPG.m_ParamGroups.insert(std::make_pair(NS_ReplicationPair::m_properties, ParameterGroup()));
    ParameterGroupsIter_t &replicationPairPropsIter = replicationPairPropsPair.first;
    ParameterGroup &replicationPairPropetiesPG = replicationPairPropsIter->second;

    replicationPairPG.m_Params.insert(std::make_pair(NS_ReplicationPair::m_version,ValueType("string","1.0")));

    UpdateApplicationReplicationPairPG(replicationPairPropetiesPG,pairObject);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

 void AppSettingsHandler::UpdateApplicationReplicationPairPG(ParameterGroup &replicationPairPropetiesPG,ConfSchema::Object &pairObject)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ; 
    
    std::string valueStr;
    FetchValueFromMap(pairObject.m_Attrs,m_ProtectionPairObj.GetTgtNameAttrName(),valueStr);
    replicationPairPropetiesPG.m_Params.insert(std::make_pair("RemoteMountPoint",ValueType("string",valueStr)));

    std::string pairState;
    FetchValueFromMap(pairObject.m_Attrs,m_ProtectionPairObj.GetReplStateAttrName(),valueStr);
    if(valueStr == "0")
    {
        pairState = "Diff";
    }
    else
    {
        pairState = "Resync";
    }
    replicationPairPropetiesPG.m_Params.insert(std::make_pair("PairState",ValueType("string",pairState)));
    
    FetchValueFromMap(pairObject.m_Attrs,m_ProtectionPairObj.GetShouldResyncAttrName(),valueStr);
    replicationPairPropetiesPG.m_Params.insert(std::make_pair("ResyncRequired",ValueType("string",valueStr)));
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}
