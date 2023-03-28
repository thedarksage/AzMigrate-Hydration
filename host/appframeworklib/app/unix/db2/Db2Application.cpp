#include "Db2Application.h"
#include "Db2Discovery.h"
#include <boost/algorithm/string.hpp>

#include <sstream>
#include "ruleengine/unix/ruleengine.h"

#include "host.h"

#include "util/common/util.h"

SVERROR Db2Application::init()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED Db2Application::%s\n", FUNCTION_NAME);
    SVERROR bRet = SVS_FALSE ;    
    if(m_db2DiscoveryImplPtr != NULL)
    {
        m_db2DiscoveryImplPtr->getInstance();
    }
    else
    {
        m_db2DiscoveryImplPtr.reset( new Db2DiscoveryImpl() ) ;
    }    
    if( m_db2DiscoveryImplPtr != NULL )
    {
            bRet = SVS_OK ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Db2DiscoveryImpl::init failed\n") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED Db2Application::%s\n", FUNCTION_NAME) ;
    return bRet ;
}

Db2Application::Db2Application()
{
    m_isInstalled = false ;
}

SVERROR Db2Application::discoverApplication()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED Db2Application::%s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    try
    {
        Db2AppDiscoveryInfo discoveryInfo ;
        if(m_db2DiscoveryImplPtr == NULL)
        {
            init() ;
        }
        if( m_db2DiscoveryImplPtr->discoverDb2Application(discoveryInfo) == SVS_OK )
        {
            bRet = SVS_OK ;
            m_db2AppDiscoveryInfo = discoveryInfo;

            std::map<std::string, Db2AppInstanceDiscInfo> db2DbDiscoveryInfo = discoveryInfo.m_db2InstDiscInfo;
            std::map<std::string, Db2AppInstanceDiscInfo>::iterator db2AppDiscIter = db2DbDiscoveryInfo.begin();

            while(db2AppDiscIter != db2DbDiscoveryInfo.end())
            {
                std::string checkDbName = db2AppDiscIter->first;				
                if(m_db2DiscoveryInfoMap.find(db2AppDiscIter->first) == m_db2DiscoveryInfoMap.end())
                {
                    m_db2DiscoveryInfoMap.insert(std::make_pair(db2AppDiscIter->first, db2AppDiscIter->second));
                }
                db2AppDiscIter++ ;
            }          
        }
        else
        {
           bRet=SVS_FALSE;
        }
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR, "Exception while discovering the Db2 application\n") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED Db2Application::%s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR Db2Application::discoverMetaData()
{
    SVERROR bRet = SVS_OK;
    return bRet;
            
}

SVERROR Db2Application::validateApplicationProtection()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR Db2Application::validateApplicationFailOver()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR Db2Application::validateApplicationFailBack()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

std::string Db2Application::summary(std::list<ValidatorPtr>& list)
{
		std::string stream ;
		return stream.c_str();
}


bool Db2Application::isInstalled()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ; 
	
		DebugPrintf(SV_LOG_ERROR, "Error while initilization\n");
	
		std::string db2Host = Host::GetInstance().GetHostName();
        if( !m_isInstalled )
        {
		   	if( m_db2DiscoveryImplPtr->isInstalled(db2Host) )
			{
			    DebugPrintf(SV_LOG_DEBUG, "exchange is installed on %s\n", db2Host.c_str());//iterator->c_str()) ;
			    m_isInstalled = true ;				    
			}			   
	    }     
	
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return m_isInstalled ;
}

void Db2Application::clean()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_db2DiscoveryInfoMap.clear();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

std::list<std::string> Db2Application::getActiveInstances()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<std::string> DatabaseNamesList ;
    std::map<std::string, Db2AppInstanceDiscInfo>::iterator db2DiscoveryIter = m_db2DiscoveryInfoMap.begin() ;
    while( db2DiscoveryIter != m_db2DiscoveryInfoMap.end() )
    {
        DebugPrintf(SV_LOG_DEBUG, "pushing the database name %s\n", db2DiscoveryIter->first.c_str()) ;
        DatabaseNamesList.push_back(db2DiscoveryIter->first) ;
        db2DiscoveryIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return DatabaseNamesList ;
}

std::map<std::string, std::list<std::string> > Db2Application::getVersionInstancesMap()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<std::string, std::list<std::string> > versionInstancesMap ;
    std::list<std::string> InstanceNamesList ;
    std::string version ;
    std::map<std::string, Db2AppInstanceDiscInfo>::iterator db2DiscoveryIter = m_db2DiscoveryInfoMap.begin() ;
    versionInstancesMap.insert(std::make_pair(version, InstanceNamesList)) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return versionInstancesMap ;
}

