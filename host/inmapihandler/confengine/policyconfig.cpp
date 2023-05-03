#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <boost/lexical_cast.hpp>
#include <sstream>
#include "policyconfig.h"
#include "portable.h"
#include "confschemamgr.h"
#include "portablehelpers.h"
#include "InmsdkGlobals.h"
#include "inmstrcmp.h"
#include "applicationsettings.h"
#include "xmlizeapplicationsettings.h"
#include "serializeapplicationsettings.h"
#include "confschemareaderwriter.h"
#include "util.h"
#include "inmsdkutil.h"
#include "auditconfig.h"
#include "agentconfig.h"

PolicyConfigPtr PolicyConfig::m_policyConfigPtr  ;

bool PolicyConfig::GetPolicyObjectsGroup(ConfSchema::Group** policyObjsGrp)
{
    bool bRet = false ;
    if( !m_policyGroup->m_Objects.size() )
    {
        m_policyGroup->m_Objects.insert(std::make_pair( 
            boost::lexical_cast<std::string>(time(NULL)), ConfSchema::Object()));
    }
    ConfSchema::Object& policyConfigObj = m_policyGroup->m_Objects.begin()->second ;
    if( policyConfigObj.m_Groups.find( "PolicyObjects" ) == policyConfigObj.m_Groups.end() )
    {
        policyConfigObj.m_Groups.insert(std::make_pair( "PolicyObjects", ConfSchema::Group())) ;
    }
    *policyObjsGrp = &(policyConfigObj.m_Groups.find( "PolicyObjects" )->second) ;
    return bRet ;
}



bool PolicyConfig::GetPolicyGlobalParamsGroup(ConfSchema::Group** globalParamsGrp)
{
    bool bRet = false ;
    if( !m_policyGroup->m_Objects.size() )
    {
        m_policyGroup->m_Objects.insert(std::make_pair( 
            boost::lexical_cast<std::string>(time(NULL)), ConfSchema::Object()));
    }
    ConfSchema::Object& policyConfigObj = m_policyGroup->m_Objects.begin()->second ;
    if( policyConfigObj.m_Groups.find( "GlobalParams" ) == policyConfigObj.m_Groups.end() )
    {
        policyConfigObj.m_Groups.insert(std::make_pair( "GlobalParams", ConfSchema::Group())) ;
    }
    *globalParamsGrp = &(policyConfigObj.m_Groups.find( "GlobalParams" )->second ) ;
    return bRet ;
}

SV_ULONGLONG PolicyConfig::GetNewPolicyId()
{
    ConfSchema::Group* globalParamsGrp ;
    SV_ULONGLONG policyId ;
    GetPolicyGlobalParamsGroup(&globalParamsGrp) ;
    if( !globalParamsGrp->m_Objects.size() )
    {
        globalParamsGrp->m_Objects.insert(std::make_pair(boost::lexical_cast<std::string>(time(NULL)), 
                        ConfSchema::Object())) ;
    }
    ConfSchema::Object& globalParamsObj =  globalParamsGrp->m_Objects.begin()->second;
    if( globalParamsObj.m_Attrs.find( "AvailablePolicyId" ) == globalParamsObj.m_Attrs.end() )
    {
        globalParamsObj.m_Attrs.insert( std::make_pair( "AvailablePolicyId", "1")) ;
    }
    ConfSchema::AttrsIter_t attrIter = globalParamsObj.m_Attrs.find( "AvailablePolicyId" ) ;
    policyId = boost::lexical_cast<SV_ULONGLONG>(attrIter->second) ;
    attrIter->second = boost::lexical_cast<std::string>(++policyId) ;
    return policyId ;
}


PolicyConfigPtr PolicyConfig::GetInstance()
{
    if( !m_policyConfigPtr )
    {
        m_policyConfigPtr.reset( new PolicyConfig() ) ;
    }
    m_policyConfigPtr->loadPolicyConfig() ;
    return m_policyConfigPtr ;
}

bool PolicyConfig::isModified() 
{
    return m_isChanged ;
}
void PolicyConfig::loadPolicyConfig()
{
    ConfSchemaReaderWriterPtr confSchemaReaderWriter = ConfSchemaReaderWriter::GetInstance() ;
    m_policyInstanceGrp = confSchemaReaderWriter->getGroup(m_polInstanceAttrNamesObj.GetName());
    m_policyGroup = confSchemaReaderWriter->getGroup(m_policyAttrNamesObj.GetName());
}

bool PolicyConfig::GetScheduleType(SV_ULONGLONG policyId, SV_UINT& scheduleType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object* policyObj = NULL ;
    bool bRet = false ;
    if( GetPolicyObject( policyId, &policyObj ) )
    {
        ConfSchema::Attrs_t& policyAttrs = policyObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = policyAttrs.find( m_policyAttrNamesObj.GetScheduleTypeAttrName() ) ;
        scheduleType = boost::lexical_cast<SV_UINT>( attrIter->second ) ;
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

PolicyConfig::PolicyConfig()
{
    m_policyGroup = NULL ;
    m_policyInstanceGrp = NULL ;
    m_isChanged = false ;
}

bool PolicyConfig::GetPolicyObject( SV_ULONGLONG policyId, 
                                    ConfSchema::Object** policyObj )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Group* policyObjectsGroup ;
    GetPolicyObjectsGroup(&policyObjectsGroup) ;
    ConfSchema::ObjectsIter_t policyObjIter = find_if(policyObjectsGroup->m_Objects.begin(),
                              policyObjectsGroup->m_Objects.end(),
                              ConfSchema::EqAttr(m_policyAttrNamesObj.GetIdAttrName(),
                              (boost::lexical_cast<std::string>(policyId)).c_str())) ;
    if( policyObjIter != policyObjectsGroup->m_Objects.end() )
    {
        bRet = true ;
        *policyObj = &(policyObjIter->second) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool PolicyConfig::GetPolicyInstanceObj( SV_ULONGLONG policyId, SV_LONGLONG instanceId, ConfSchema::Object** policyInstanceObj, bool matchInstanceId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::ObjectsIter_t policyInstanceObjIter ;
    policyInstanceObjIter = m_policyInstanceGrp->m_Objects.begin() ;
    while( policyInstanceObjIter != m_policyInstanceGrp->m_Objects.end() )
    {
        ConfSchema::AttrsIter_t attrIter ;
        SV_ULONGLONG lpolicyId, lpolicyInstanceId ;
        attrIter = policyInstanceObjIter->second.m_Attrs.find( m_polInstanceAttrNamesObj.GetPolicyIdAttrName()) ;
        lpolicyId = boost::lexical_cast<SV_ULONGLONG> (attrIter->second) ;
        attrIter = policyInstanceObjIter->second.m_Attrs.find( m_polInstanceAttrNamesObj.GetInstanceIdAttrName()) ;
        lpolicyInstanceId = boost::lexical_cast<SV_ULONGLONG> (attrIter->second) ;
        if( lpolicyId == policyId )
        {
            bool proceed = false ;
            if( matchInstanceId )
            {
                if( lpolicyInstanceId == 0 || lpolicyInstanceId == instanceId )
                {
                    proceed = true ;
                }
            }
            else
            {
                proceed = true ;
            }
            if( proceed == true )
            {
                attrIter = policyInstanceObjIter->second.m_Attrs.find( m_polInstanceAttrNamesObj.GetInstanceIdAttrName()) ;
                attrIter->second = boost::lexical_cast<std::string> (instanceId) ; ;
                *policyInstanceObj = &(policyInstanceObjIter->second) ;
                bRet = true ;
                break ;
            }
        }
        policyInstanceObjIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool PolicyConfig::IsPolicyAvailable(SV_ULONGLONG policyId)
{
    ConfSchema::Object* policyObj = NULL ;
    bool bRet = false ;
    if( GetPolicyObject(policyId, &policyObj) )
    {
        bRet = false ;
    }
    return bRet ;
}


void PolicyConfig::UpdateRunOncePolicy( SV_ULONGLONG policyId, 
                                        SV_LONGLONG policyInstanceId,
                                        const std::string& status,
                                        const std::string& log )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object* policyInstanceObj ;
    if( GetPolicyInstanceObj( policyId, policyInstanceId, &policyInstanceObj, false) )
    {
        ConfSchema::Attrs_t& policyInstanceAttrs = policyInstanceObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = policyInstanceAttrs.find( m_polInstanceAttrNamesObj.GetStatusAttrName() ) ;
        attrIter->second = status ;
    }
    else
    {
        throw "Failed to get the instance for the policy Id " + boost::lexical_cast<std::string>( policyId ) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void PolicyConfig::UpdateRunEveryPolicy( SV_ULONGLONG policyId, 
                                         SV_LONGLONG policyInstanceId,
                                         const std::string& status,
                                         const std::string& log )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object* policyInstanceObj ;
    ConfSchema::Object* policyObj ;
    if( GetPolicyObject( policyId, &policyObj ) )
    {
        bool insertNewInstance = false ;
        if( !GetPolicyInstanceObj(policyId, policyInstanceId, &policyInstanceObj, true ) )
        {
            insertNewInstance = true ;
        }
        ConfSchema::AttrsIter_t attrIter; 
        if( insertNewInstance )
        {
            ConfSchema::Object newInstanceObj ;
            attrIter = policyObj->m_Attrs.find( m_policyAttrNamesObj.GetScenarioIdAttrName() ) ;
            int scenarioId = boost::lexical_cast<int>(attrIter->second) ;
            ConfSchema::Attrs_t& policyInstanceAttrs = newInstanceObj.m_Attrs ;
            std::string uniqueId = boost::lexical_cast<std::string>(time(NULL));

            policyInstanceAttrs.insert(std::make_pair(m_polInstanceAttrNamesObj.GetPolicyIdAttrName(),
                boost::lexical_cast<std::string>(policyId)));    

            policyInstanceAttrs.insert(std::make_pair(m_polInstanceAttrNamesObj.GetInstanceIdAttrName(),
                            boost::lexical_cast<std::string>(policyInstanceId)));

            policyInstanceAttrs.insert(std::make_pair(m_polInstanceAttrNamesObj.GetStatusAttrName(),status));

            policyInstanceAttrs.insert(std::make_pair(m_polInstanceAttrNamesObj.GetExecutionLogAttrName(),log));

            policyInstanceAttrs.insert(std::make_pair(m_polInstanceAttrNamesObj.GetUniqueIdAttrName(),uniqueId));

            policyInstanceAttrs.insert(std::make_pair(m_polInstanceAttrNamesObj.GetDependentInstanceIdAttrName(),""));

            policyInstanceAttrs.insert(std::make_pair(m_polInstanceAttrNamesObj.GetScenarioIdAttrName(),
                boost::lexical_cast<std::string>(scenarioId)));
            ACE_OS::sleep(1) ;
            m_policyInstanceGrp->m_Objects.insert(make_pair(boost::lexical_cast<std::string>(time(NULL)),  
                                    newInstanceObj)) ;
        }
        else
        {
            ConfSchema::Attrs_t& policyInstanceAttrs = policyInstanceObj->m_Attrs ;
            attrIter = policyInstanceAttrs.find( m_polInstanceAttrNamesObj.GetStatusAttrName() ) ;
            attrIter->second = status ;
            if( InmStrCmp<NoCaseCharCmp>(status.c_str(), "Failed") == 0 || 
                InmStrCmp<NoCaseCharCmp>(status.c_str(), "Success") == 0 || 
                InmStrCmp<NoCaseCharCmp>(status.c_str(), "Skipped") == 0 )
            {
                attrIter = policyObj->m_Attrs.find( m_policyAttrNamesObj.GetTotalRunsCountAttrName()) ;
                SV_ULONGLONG totalCount = boost::lexical_cast<SV_ULONGLONG>(attrIter->second) ;
                attrIter->second = boost::lexical_cast<std::string>(++totalCount) ;
                if( InmStrCmp<NoCaseCharCmp>(status.c_str(), "Failed") == 0 )
                {
                    attrIter = policyObj->m_Attrs.find( m_policyAttrNamesObj.GetFailedRunsCountAttrName()) ;
                    SV_ULONGLONG failedCount = boost::lexical_cast<SV_ULONGLONG>(attrIter->second) ;
                    attrIter->second = boost::lexical_cast<std::string>(++failedCount) ;
                }
                else if( InmStrCmp<NoCaseCharCmp>(status.c_str(), "Skipped") == 0 )
                {
                    attrIter = policyObj->m_Attrs.find( m_policyAttrNamesObj.GetSkippedRunsCountAttrName()) ;
                    SV_ULONGLONG skippedCount = boost::lexical_cast<SV_ULONGLONG>(attrIter->second) ;
                    attrIter->second = boost::lexical_cast<std::string>(++skippedCount) ;

                }
                else 
                {
                    attrIter = policyObj->m_Attrs.find( m_policyAttrNamesObj.GetSuccessRunsCountAttrName()) ;
                    SV_ULONGLONG successCount = boost::lexical_cast<SV_ULONGLONG>(attrIter->second) ;
                    attrIter->second = boost::lexical_cast<std::string>(++successCount) ;
                }
            }
        }
    }
    else
    {
        throw "Didn't find the policy in the policy objects with policy id" + 
            boost::lexical_cast<std::string>(policyId) ;
    }
    
}
void PolicyConfig::RemoveOldInstances(SV_ULONGLONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::ObjectsIter_t objIter = m_policyInstanceGrp->m_Objects.begin() ;
    SV_UINT successCount, failureCount, skippedCount ;
    successCount = 0 ;
    failureCount = 0 ;
    skippedCount = 0 ;
    while( objIter != m_policyInstanceGrp->m_Objects.end() )
    {
        bool removeOld = false ;
        ConfSchema::Attrs_t& policyInstanceAttrs = objIter->second.m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = policyInstanceAttrs.find( m_polInstanceAttrNamesObj.GetPolicyIdAttrName() ) ;
        SV_ULONGLONG lpolicyId = boost::lexical_cast<SV_ULONGLONG>(attrIter->second) ;
        if( policyId == lpolicyId )
        {
            attrIter = policyInstanceAttrs.find( m_polInstanceAttrNamesObj.GetStatusAttrName()) ;
            std::string& status = attrIter->second ;
            if( InmStrCmp<NoCaseCharCmp>(status.c_str(), "Success") == 0 )
            {
                successCount++ ;
            }
            else if( InmStrCmp<NoCaseCharCmp>(status.c_str(), "Failed") == 0 )
            {
                failureCount++ ;            
            }
            else if( InmStrCmp<NoCaseCharCmp>(status.c_str(), "Skipped") == 0 )
            {
                skippedCount++ ;
            }
            if( successCount > 5 || failureCount > 5 || skippedCount > 5 )
            {
                removeOld = true ;
            }
        }
        ConfSchema::ObjectsIter_t tempObjIter = objIter ;
        objIter++ ;
        if( removeOld )
        {
            m_policyInstanceGrp->m_Objects.erase( tempObjIter->first ) ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

POLICYUPDATE_ENUM PolicyConfig::UpdatePolicy(SV_ULONGLONG policyId, 
                                SV_LONGLONG policyInstanceId, 
                                const std::string& status,
                                const std::string& log)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    POLICYUPDATE_ENUM policyUpdate = APP_POLICY_UPDATE_COMPLETED ;
    SV_UINT scheduleType ;
    if( GetScheduleType( policyId, scheduleType ) )
    {
        if( scheduleType == 2 )
        {
            UpdateRunOncePolicy( policyId, policyInstanceId, status, log ) ;
        }
        else if( scheduleType == 1 )
        {
            UpdateRunEveryPolicy( policyId, policyInstanceId, status, log ) ;
            if( InmStrCmp<NoCaseCharCmp>(status.c_str(), "Failed") == 0 ||
                InmStrCmp<NoCaseCharCmp>(status.c_str(), "Success") == 0 || 
                InmStrCmp<NoCaseCharCmp>(status.c_str(), "Skipped") == 0 )
            {
                RemoveOldInstances( policyId ) ;
            }
        }
        else
        {
            policyUpdate = APP_POLICY_UPDATE_FAILED ;
        }
    }
    else
    {
        policyUpdate = APP_POLICY_UPDATE_DELETED ;
    }        
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return policyUpdate ;
}


void PolicyConfig::PopulateConsistencyCmd( const ConsistencyOptions& consistencyOptions,
                                           const std::list<std::string>& volumes,
                                           std::string& consistencyCmd)
{
    std::stringstream vacpCmdStream ;
	vacpCmdStream << AgentConfig::GetInstance()->getInstallPath() ;
    vacpCmdStream << "\\";
    vacpCmdStream << "vacp.exe " ;

    if( InmStrCmp<NoCaseCharCmp>(consistencyOptions.tagType, "Application") == 0 )
    {
        vacpCmdStream << "-a all";    
    }
    else
    {
        vacpCmdStream << "-a sr";    
    }
    if( volumes.size() > 0 )
    {
        vacpCmdStream << " -v " ;
        std::list<std::string>::const_iterator volIter = volumes.begin() ;
		vacpCmdStream << "\"";
        while( volIter != volumes.end() )
        {
            vacpCmdStream << volIter->c_str() ;
            if( volIter->length() > 1 )
            {
                vacpCmdStream << ";" ;
            }
            else
            {
                vacpCmdStream << ":;" ;
            }
            volIter++ ;
        }
		vacpCmdStream << "\"";
		if ( !consistencyOptions.tagName.empty())
			vacpCmdStream << " -t " << consistencyOptions.tagName;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "No volumes while preparing the consistency command\n") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "Consistency Command : %s\n", vacpCmdStream.str().c_str()) ;
    consistencyCmd = vacpCmdStream.str() ;
}


SV_LONGLONG ConvertToSeconds(const std::string& humanReadableInterval)
{
    std::string str = humanReadableInterval ;
    boost::algorithm::to_lower( str ) ;
    SV_LONGLONG interval = 600 ; //default 
    size_t index ;
    if( (index = str.find( "minute" )) != std::string::npos )
    {
        str = str.substr(0, index) ;
        boost::algorithm::replace_all(str, " ", "") ;
        interval = boost::lexical_cast<SV_LONGLONG>(str) ;
        interval = interval * 60 ;
    }
    else if( (index = str.find( "hour" ))!= std::string::npos ||
             (index = str.find( "hr" ))!= std::string::npos   )
    {
        str = str.substr(0, index) ;
        boost::algorithm::replace_all(str, " ", "") ;
        interval = boost::lexical_cast<SV_LONGLONG>(str) ;
        interval = interval * 60 * 60 ;
    }
    else if( (index = str.find( "day" ))!= std::string::npos )
    {
        str = str.substr(0, index) ;
        boost::algorithm::replace_all(str, " ", "") ;
        interval = boost::lexical_cast<SV_LONGLONG>(str) ;
        interval = interval * 60 * 60 * 24 ;
    }
    else if( (index = str.find( "week" ))!= std::string::npos )
    {
        str = str.substr(0, index) ;
        boost::algorithm::replace_all(str, " ", "") ;
        interval = boost::lexical_cast<SV_LONGLONG>(str) ;
        interval = interval * 60 * 60 * 24 * 7;
    }
    DebugPrintf(SV_LOG_DEBUG, "Intrval in Human Readable fmt %s. after converting to seconds %ld\n",
        humanReadableInterval.c_str(), interval) ;
    return interval ;
}

SV_ULONGLONG PolicyConfig::CreateConsistencyPolicy(  const ConsistencyOptions& consistencyOptions,
                                                const std::list<std::string>& volumes,
                                                int scenarioId,
                                                bool runEvery,
                                                bool enabled)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::string policyId = "0" ;
    ConfSchema::Object policyObject ;
    std::string consistencyCmd ;
    PopulateConsistencyCmd( consistencyOptions, volumes,  consistencyCmd) ;
    time_t excludeFrom, excludeTo ;
    excludeFrom = excludeTo = 0 ;
    int scheduleType ;
	int scheduleInterval = 0 ;
    policyId = boost::lexical_cast<std::string>(GetNewPolicyId());
    if( !enabled )
    {
        scheduleType = 0 ;
    }
    else 
    {
        if( runEvery )
        {
            scheduleType = 1 ;				
        }
        else
        {
            scheduleType = 2 ;
        }
    }
    if( scheduleType != 2 )
    {
        GetExcludeFromAndTo(consistencyOptions.exception, excludeFrom, excludeTo) ;
        scheduleInterval = ConvertToSeconds(consistencyOptions.interval) ;
    }
    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetconsistencyCommandAttrName(),
                                                consistencyCmd)) ;

    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetContextAttrName(), 
                                                "PROTECTION")) ;
    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetDoNotSkipAttrName(),
                                                "1")) ;
    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetExcludeFromAttrName(),
                                            boost::lexical_cast<std::string>(excludeFrom))) ;
    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetExcludeToAttrName(),
                                                boost::lexical_cast<std::string>(excludeTo))) ;

    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetHostIdAttrName(),
                                                "HOSTID")) ;

    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetIdAttrName(),
                                                policyId)) ;
    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetIntervalAttrName(),
        boost::lexical_cast<std::string>(scheduleInterval))) ;

    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetPolicyDataAttrName(),
                                                "")) ;

    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetPolicyTimestampAttrName(),
                                                boost::lexical_cast<std::string>(time(NULL)))) ;

    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetScenarioIdAttrName(), 
                                                boost::lexical_cast<std::string>(scenarioId))) ;

    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetScheduleTypeAttrName(), 
                                            boost::lexical_cast<std::string>(scheduleType))) ;

    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetTypeAttrName(), 
                                "Consistency")) ;
    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetSuccessRunsCountAttrName(), "0")) ;
    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetFailedRunsCountAttrName(), "0")) ;
    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetSkippedRunsCountAttrName(), "0")) ;
    policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetTotalRunsCountAttrName(), "0")) ;
    ConfSchema::Group* policyObjectsGrp ;
    GetPolicyObjectsGroup(&policyObjectsGrp) ;
    policyObjectsGrp->m_Objects.insert(std::make_pair( policyId, policyObject)) ;

    ConfSchema::Object policyInstanceObj ;
    policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetDependentInstanceIdAttrName(), "")) ;
    policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetExecutionLogAttrName(), "")) ;
    policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetInstanceIdAttrName(), "0")) ;
    policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetPolicyIdAttrName(), policyId)) ;
    policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetScenarioIdAttrName(),
                                                            boost::lexical_cast<std::string>(scenarioId))) ;
    policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetStatusAttrName(), "")) ;
    policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetUniqueIdAttrName(), 
                                                             boost::lexical_cast<std::string>(time(NULL)))) ;
    ACE_OS::sleep(1) ;
    m_policyInstanceGrp->m_Objects.insert(std::make_pair( boost::lexical_cast<std::string>(time(NULL)),
                                                            policyInstanceObj)) ;
    AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
    std::string auditmsg ;
    auditmsg = "Creating consistency policy with policy Id " + policyId + ". " ;
    auditmsg += "Consistency level is " ;
    auditmsg += consistencyOptions.tagType ;
    auditmsg += ". " ;
    auditmsg += "Consistency command is : " ;
    auditmsg += consistencyCmd ;
    auditConfigPtr->LogAudit(auditmsg) ;
        
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return boost::lexical_cast<SV_ULONGLONG>(policyId) ;
}


void PolicyConfig::GetVolpackPolicyData(const std::string& srcVol,
										const std::string& tgtVol,
                                        const std::map<std::string,volumeProperties>& volpropmap,
                                        VOLPACK_ACTION action,
                                        std::string& volpackPolicyData)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    VolPacks volpack ;
    volpack.m_version = "1.0" ;
    std::string exception ;
    exception = getConfigStorePath() ;
	std::list<std::string> volumes ;
        std::map<std::string,volumeProperties>::const_iterator volpropiter = volpropmap.find(srcVol) ;
	if( volpropiter != volpropmap.end() )
    {
	    
		std::string volpackPath =   tgtVol ;
		if( volpackPath.find( exception) == 0 )
		{
			std::string converToLower = ToLower( volpackPath.substr( exception.length() ) ) ;
			volpackPath.replace( exception.length(), converToLower.length(), converToLower) ;
		}
	  
		VolPackAction volpackAction;
		volpackAction.m_action = action ;
		volpackAction.m_size = boost::lexical_cast<SV_LONGLONG>(volpropiter->second.rawSize) ;
		volpack.m_volPackActions.insert(std::make_pair( volpackPath, volpackAction)) ;
		std::stringstream policyDataStream, volumesStream ;
		policyDataStream <<  CxArgObj<VolPacks>(volpack);
		volpackPolicyData = policyDataStream.str() ;
	}
	else
	{
        DebugPrintf(SV_LOG_ERROR, "Didn't find volume properties for %s\n", srcVol.c_str()) ;
	}
    DebugPrintf( SV_LOG_DEBUG, "VolpackPolicyData: %s\n", volpackPolicyData.c_str() ) ;
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return ;
}
void PolicyConfig::CreateVolumeProvisioningPolicy(const std::map<std::string, std::string>& srcTgtVolMap,
                                                     const std::map<std::string,volumeProperties>& volpropmap,
                                                     int scenarioId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::map<std::string, std::string>::const_iterator srcTgtMapIter = srcTgtVolMap.begin() ;
	while( srcTgtMapIter != srcTgtVolMap.end() )
	{
		std::string policyId = "0" ;
		std::string volpackPolicyData ;
		GetVolpackPolicyData(srcTgtMapIter->first, srcTgtMapIter->second, 
							 volpropmap, VOLPACK_CREATE, volpackPolicyData) ;
		policyId = boost::lexical_cast<std::string>(GetNewPolicyId()) ;
		ConfSchema::Object policyObject ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetconsistencyCommandAttrName(),
													"")) ;

		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetContextAttrName(), 
													"PROTECTION")) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetDoNotSkipAttrName(), "1")) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetExcludeFromAttrName(), "0")) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetExcludeToAttrName(), "0")) ;

		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetHostIdAttrName(), "HOSTID")) ;

		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetIdAttrName(), policyId)) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetIntervalAttrName(), "0")) ;

		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetPolicyDataAttrName(), volpackPolicyData)) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetVolpacksForVolumes(), srcTgtMapIter->first)) ;
	    
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetPolicyTimestampAttrName(),
													boost::lexical_cast<std::string>(time(NULL)))) ;

		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetScenarioIdAttrName(), 
													boost::lexical_cast<std::string>(scenarioId))) ;

		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetScheduleTypeAttrName(), 
												boost::lexical_cast<std::string>(2))) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetSuccessRunsCountAttrName(), "0")) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetFailedRunsCountAttrName(), "0")) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetTotalRunsCountAttrName(), "0")) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetSkippedRunsCountAttrName(), "0")) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetTypeAttrName(), 
									"VolumeProvisioningV2")) ;
		ConfSchema::Group* policyObjectsGrp ;
		GetPolicyObjectsGroup(&policyObjectsGrp) ;
		policyObjectsGrp->m_Objects.insert(std::make_pair( policyId, policyObject)) ;

		ConfSchema::Object policyInstanceObj ;
		policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetDependentInstanceIdAttrName(), "")) ;
		policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetExecutionLogAttrName(), "")) ;
		policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetInstanceIdAttrName(), "0")) ;
		policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetPolicyIdAttrName(), policyId)) ;
		policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetScenarioIdAttrName(),
																boost::lexical_cast<std::string>(scenarioId))) ;
		policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetStatusAttrName(), "")) ;
		policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetUniqueIdAttrName(), 
																 boost::lexical_cast<std::string>(time(NULL)))) ;
		ACE_OS::sleep(1) ;
		m_policyInstanceGrp->m_Objects.insert(std::make_pair( boost::lexical_cast<std::string>(time(NULL)),
																policyInstanceObj)) ;
		AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
		std::string auditmsg ;
		auditmsg = "Volume provisioning is started for " ;
		auditmsg += srcTgtMapIter->first ;
		auditConfigPtr->LogAudit(auditmsg) ;
		srcTgtMapIter++ ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void PolicyConfig::EnableVolumeUnProvisioningPolicy()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::ObjectsIter_t policyObjIter ;
    ConfSchema::Group* policyObjsGroup ;
    GetPolicyObjectsGroup(&policyObjsGroup) ;
    policyObjIter = policyObjsGroup->m_Objects.begin() ;
    while( policyObjIter != policyObjsGroup->m_Objects.end() )
    {
        SV_ULONGLONG policyId ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = policyObjIter->second.m_Attrs.find( m_policyAttrNamesObj.GetIdAttrName() ) ;
        policyId = boost::lexical_cast<SV_ULONGLONG>(attrIter->second) ;
        std::string policyType = GetPolicyType( &(policyObjIter->second) ) ;
        SV_UINT scheduleType ;
        if( GetScheduleType( policyId, scheduleType ) && 
            scheduleType == 0 && 
            policyType.compare("VolumeProvisioningV2") == 0 )
        {
                EnablePolicy(&(policyObjIter->second), true ) ;
                AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
                std::string auditmsg ;
                auditmsg = "Starting Volume Un-Provisioning " ;
                auditConfigPtr->LogAudit(auditmsg) ;
        }
        policyObjIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void PolicyConfig::CreateVolumeUnProvisioningPolicy(const std::map<std::string, std::string>& srcTgtVolMap,
                                                       const std::map<std::string,volumeProperties>& volpropmap,
                                                       int scenarioId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::map<std::string, std::string>::const_iterator srcTgtVolIter = srcTgtVolMap.begin() ;
	while( srcTgtVolIter != srcTgtVolMap.end() )
	{
		std::string policyId = "0" ;
		std::string volpackPolicyData ;
		GetVolpackPolicyData(srcTgtVolIter->first, srcTgtVolIter->second, volpropmap, VOLPACK_DELETE, volpackPolicyData) ;
		policyId = boost::lexical_cast<std::string>(GetNewPolicyId()) ;
		ConfSchema::Object policyObject ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetconsistencyCommandAttrName(),
													"")) ;

		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetContextAttrName(), 
													"PROTECTION")) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetDoNotSkipAttrName(), "1")) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetExcludeFromAttrName(), "0")) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetExcludeToAttrName(), "0")) ;

		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetHostIdAttrName(), "HOSTID")) ;

		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetIdAttrName(), policyId)) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetIntervalAttrName(), "0")) ;

		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetPolicyDataAttrName(), volpackPolicyData)) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetVolpacksForVolumes(), srcTgtVolIter->first)) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetPolicyTimestampAttrName(),
													boost::lexical_cast<std::string>(time(NULL)))) ;

		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetScenarioIdAttrName(), 
													boost::lexical_cast<std::string>(scenarioId))) ;

		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetScheduleTypeAttrName(), 
												boost::lexical_cast<std::string>(0))) ;

		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetTypeAttrName(), 
									"VolumeProvisioningV2")) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetSuccessRunsCountAttrName(), "0")) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetFailedRunsCountAttrName(), "0")) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetTotalRunsCountAttrName(), "0")) ;
		policyObject.m_Attrs.insert(std::make_pair( m_policyAttrNamesObj.GetSkippedRunsCountAttrName(), "0")) ;
		ConfSchema::Group* policyObjsGroup ;
		GetPolicyObjectsGroup(&policyObjsGroup) ;
		policyObjsGroup->m_Objects.insert(std::make_pair( policyId, policyObject)) ;

		ConfSchema::Object policyInstanceObj ;
		policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetDependentInstanceIdAttrName(), "")) ;
		policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetExecutionLogAttrName(), "")) ;
		policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetInstanceIdAttrName(), "0")) ;
		policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetPolicyIdAttrName(), policyId)) ;
		policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetScenarioIdAttrName(),
																boost::lexical_cast<std::string>(scenarioId))) ;
		policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetStatusAttrName(), "")) ;
		policyInstanceObj.m_Attrs.insert(std::make_pair( m_polInstanceAttrNamesObj.GetUniqueIdAttrName(), 
																 boost::lexical_cast<std::string>(time(NULL)))) ;
		ACE_OS::sleep(1) ;
		m_policyInstanceGrp->m_Objects.insert(std::make_pair( boost::lexical_cast<std::string>(time(NULL)),
																policyInstanceObj)) ;
		AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
		std::string auditmsg ;
		auditmsg = "Creating Volume Un-Provisioning policy" ;
		auditConfigPtr->LogAudit(auditmsg) ;
		srcTgtVolIter++ ;
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool PolicyConfig::IsPolicyEnabled(ConfSchema::Object* policyObj)
{
    ConfSchema::AttrsIter_t attrIter ;
    bool bRet = false ;
    attrIter = policyObj->m_Attrs.find( m_policyAttrNamesObj.GetScheduleTypeAttrName() ) ;
    if( attrIter->second.compare("0") != 0 )
    {
        DebugPrintf(SV_LOG_DEBUG, "Policy %d is enabled\n") ;
        bRet = true ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Policy %d is disabled\n") ;
    }
    return bRet ;
}

bool PolicyConfig::IsPolicyEnabled(SV_ULONGLONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool enabled = false ;
    ConfSchema::Object* policyObj ;
    if( GetPolicyObject( policyId, &policyObj) )
    {
        enabled = IsPolicyEnabled( policyObj ) ;
    }
    return enabled ;
}


void PolicyConfig::EnablePolicy( ConfSchema::Object* policyObj, bool runOnce)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = policyObj->m_Attrs.find( m_policyAttrNamesObj.GetScheduleTypeAttrName() ) ;

    if( attrIter->second.compare("0") == 0 )
    {
        DebugPrintf(SV_LOG_DEBUG, "Policy %d is disabled\n") ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Policy %d is enabled\n") ;
    }
    if( runOnce )
    {
        attrIter->second = "2" ;
    }
    else
    {
        attrIter->second = "1" ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void PolicyConfig::EnablePolicy(SV_ULONGLONG policyId, bool runOnce)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool enabled = false ;
    ConfSchema::Object* policyObj ;
    if( GetPolicyObject( policyId, &policyObj ) )
    {
        EnablePolicy( policyObj, runOnce ) ;
    }
}

void PolicyConfig::DisablePolicy(ConfSchema::Object* policyObj)
{
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = policyObj->m_Attrs.find( m_policyAttrNamesObj.GetScheduleTypeAttrName() ) ;
    attrIter->second = "0" ;
}

void PolicyConfig::DisablePolicy(SV_ULONGLONG policyId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool enabled = false ;
    ConfSchema::Object* policyObj ;
    if( GetPolicyObject( policyId, &policyObj ) )
    {
        DisablePolicy( policyObj ) ;
    }
}

std::string PolicyConfig::GetPolicyType(ConfSchema::Object* policyObj)
{
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = policyObj->m_Attrs.find( m_policyAttrNamesObj.GetTypeAttrName() ) ;
    std::string policyType = attrIter->second ;
    return policyType ;
}


void PolicyConfig::GetPolicyType(SV_ULONGLONG policyId, std::string& policyType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object* policyObj ;
    if( GetPolicyObject( policyId, &policyObj) )
    {
        policyType = GetPolicyType( policyObj ) ;
    }
}

bool PolicyConfig::isConsistencyPolicyEnabled() 
{
    bool bRet = true ;
    ConfSchema::ObjectsIter_t policyObjIter ;
    ConfSchema::Group* policyObjsGroup ;
    GetPolicyObjectsGroup(&policyObjsGroup) ;
    policyObjIter = policyObjsGroup->m_Objects.begin() ;
    while( policyObjIter != policyObjsGroup->m_Objects.end() )
    {
        SV_ULONGLONG policyId ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = policyObjIter->second.m_Attrs.find( m_policyAttrNamesObj.GetIdAttrName() ) ;
        policyId = boost::lexical_cast<SV_ULONGLONG>(attrIter->second) ;
        std::string policyType = GetPolicyType( &(policyObjIter->second) ) ;
        if( policyType.compare("Consistency") == 0 || 
            policyType.compare("ConsistencyV2") == 0 )
        {
            SV_UINT scheduleType ;
            if( GetScheduleType( policyId, scheduleType ) )
            {
                if( scheduleType == 0 )
                {
                    bRet = false ;
                }
            }
        }
        policyObjIter++ ;
    }
    return bRet ;
}
bool PolicyConfig::PoliciesAvailable()
{
    ConfSchema::Group *policyObjsGroup ;
    GetPolicyObjectsGroup(&policyObjsGroup) ;
    return policyObjsGroup->m_Objects.size() ? true : false ;
}
void PolicyConfig::EnableConsistencyPolicy()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::ObjectsIter_t policyObjIter ;
    ConfSchema::Group *policyObjsGroup ;
    GetPolicyObjectsGroup(&policyObjsGroup) ;
    policyObjIter = policyObjsGroup->m_Objects.begin() ;
    while( policyObjIter != policyObjsGroup->m_Objects.end() )
    {
        SV_ULONGLONG policyId ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = policyObjIter->second.m_Attrs.find( m_policyAttrNamesObj.GetIdAttrName() ) ;
        policyId = boost::lexical_cast<SV_ULONGLONG>(attrIter->second) ;
        std::string policyType = GetPolicyType( &(policyObjIter->second) ) ;
        if( policyType.compare("Consistency") == 0 || 
            policyType.compare("ConsistencyV2") == 0 )
        {
            SV_UINT scheduleType ;
            if( GetScheduleType( policyId, scheduleType ) )
            {
                if( scheduleType == 0 )
                {
                    EnablePolicy(&(policyObjIter->second), false ) ;
                }
            }
        }
        policyObjIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void PolicyConfig::GetSourceVolume(SV_ULONGLONG policyid, std::string& volumename)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    VOLPACK_ACTION volpackAction = VOLPACK_INVALID ;
    ConfSchema::Object* policyObj ;
    if( GetPolicyObject( policyid, &policyObj ) )
    {
        std::string policyData ;
        ConfSchema::AttrsIter_t attrIter ;
		attrIter = policyObj->m_Attrs.find( m_policyAttrNamesObj.GetVolpacksForVolumes() ) ;
        volumename = attrIter->second ;
    }
}
VOLPACK_ACTION PolicyConfig::GetVolPackAction(SV_ULONGLONG policyid)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    VOLPACK_ACTION volpackAction = VOLPACK_INVALID ;
    ConfSchema::Object* policyObj ;
    if( GetPolicyObject( policyid, &policyObj ) )
    {
        std::string policyData ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = policyObj->m_Attrs.find( m_policyAttrNamesObj.GetPolicyDataAttrName() ) ;
        policyData = attrIter->second ;
        VolPacks volpacks = unmarshal<VolPacks>(policyData);
        if( volpacks.m_volPackActions.size() )
        {
            volpackAction = volpacks.m_volPackActions.begin()->second.m_action ;
        }
    }
    return volpackAction ;
}

void PolicyConfig::UpdateConsistencyPolicy(const ConsistencyOptions& consistencyOptions,
                                           const std::list<std::string>& volumes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    ConfSchema::Group* policyObjsGroup ;
    GetPolicyObjectsGroup( &policyObjsGroup ) ;
    ConfSchema::ObjectsIter_t policyObjIter = policyObjsGroup->m_Objects.begin() ; 
    std::string consistencyCmd ;
    PopulateConsistencyCmd(consistencyOptions, volumes, consistencyCmd) ;
    while( policyObjIter != policyObjsGroup->m_Objects.end() )
    {
        ConfSchema::AttrsIter_t attrIter ;
        ConfSchema::Object& policyObj = policyObjIter->second ;
        std::string policyType = GetPolicyType(&policyObj) ;
        if( policyType.compare("Consistency") == 0 ||
            policyType.compare("ConsistencyV2") == 0 )
        {
            attrIter = policyObj.m_Attrs.find( m_policyAttrNamesObj.GetScheduleTypeAttrName()) ;
            if( attrIter->second.compare("1") == 0 )
            {
                attrIter = policyObj.m_Attrs.find( m_policyAttrNamesObj.GetconsistencyCommandAttrName()) ;
                attrIter->second = consistencyCmd ;
                attrIter = policyObj.m_Attrs.find( m_policyAttrNamesObj.GetPolicyTimestampAttrName()) ;
                attrIter->second = boost::lexical_cast<std::string>(time(NULL)) ;
                time_t excludeFrom, excludeTo ;
                GetExcludeFromAndTo(consistencyOptions.exception, excludeFrom, excludeTo) ;
                attrIter  =  policyObj.m_Attrs.find(  m_policyAttrNamesObj.GetExcludeFromAttrName() ) ;
                attrIter->second = boost::lexical_cast<std::string>(excludeFrom) ;
                attrIter  =  policyObj.m_Attrs.find(  m_policyAttrNamesObj.GetExcludeToAttrName() ) ;
                attrIter->second = boost::lexical_cast<std::string>(excludeTo) ;
                attrIter  =  policyObj.m_Attrs.find(   m_policyAttrNamesObj.GetIntervalAttrName() ) ;
                SV_UINT scheduleInterval = ConvertToSeconds(consistencyOptions.interval) ;
                attrIter->second = boost::lexical_cast<std::string>(scheduleInterval) ;
            }
        }
        policyObjIter++ ; 
    }
    AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
    std::string auditmsg ;
    auditmsg = "Updating consistency options. " ;
    auditmsg += "Consistency level is " ;
    auditmsg += consistencyOptions.tagType ;
    auditmsg += ". " ;
    auditmsg += "Consistency command is" ;
    auditmsg += consistencyCmd ;
    auditConfigPtr->LogAudit(auditmsg) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void PolicyConfig::ClearPolicies()
{
    m_policyGroup->m_Objects.clear() ;
    m_policyInstanceGrp->m_Objects.clear() ;
    AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
    std::string auditmsg ;
    auditmsg = "Removing the policies and corresponding instances from configuration store" ;
    std::string appagentCachePath, schedulerCachePath ;
    LocalConfigurator configurator ;
    appagentCachePath = configurator.getCacheDirectory() + "\\" + "etc" + "\\" + "AppAgentCache.dat";
    schedulerCachePath = configurator.getCacheDirectory() + "\\" + "etc" + "\\" + "SchedulerCache.dat" ;
    std::string appagentCacheLoclPath = appagentCachePath + ".lck";
    if( boost::filesystem::exists( appagentCachePath.c_str() ) )
    {
        ACE_File_Lock lock(getLongPathName(appagentCacheLoclPath.c_str()).c_str(),O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
        lock.acquire_read();
        try
        {
            boost::filesystem::remove( appagentCachePath.c_str() ) ;
        }
        catch( std::exception& e)
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to unlink the files %s\n", e.what()) ;
        }
        lock.release() ;
    }
    if( boost::filesystem::exists( schedulerCachePath.c_str() ) )
    {
        try
        {
            boost::filesystem::remove( schedulerCachePath.c_str() ) ;
        }
        catch( std::exception& e)
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to unlink the files %s\n", e.what()) ;
        }
    }

    auditConfigPtr->LogAudit(auditmsg) ;
}

std::list <std::string> PolicyConfig::GetVolumesInVolumeUnProvisioningPolicyData (SV_UINT policyID)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::map<std::string, VolPackAction>::iterator m_volPackActionsIter; 
	std::list <std::string> volumeDeletionList; 
    ConfSchema::Object* policyObj ;
    if( GetPolicyObject( policyID, &policyObj ) )
    {
        std::string policyData ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = policyObj->m_Attrs.find( m_policyAttrNamesObj.GetPolicyDataAttrName() ) ;
        policyData = attrIter->second ;
        VolPacks volpacks = unmarshal<VolPacks>(policyData);
        if( volpacks.m_volPackActions.size() )
        {
            m_volPackActionsIter = volpacks.m_volPackActions.begin();
			while (m_volPackActionsIter != volpacks.m_volPackActions.end() )
			{
				volumeDeletionList.push_back (m_volPackActionsIter->first);
				m_volPackActionsIter ++ ; 
			}
        }
    }
    return volumeDeletionList ;
}

