#include "Oracleapplication.h"
#include "OracleDiscovery.h"
#include <boost/algorithm/string.hpp>
//#include <atlconv.h>
#include <sstream>
#include "ruleengine/unix/ruleengine.h"
//#include "util/clusterutil.h"
#include "host.h"
//#include "exchangeexception.h"
#include "util/common/util.h"


SVERROR OracleApplication::init()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED OracleApplication::%s\n", FUNCTION_NAME);
    SVERROR bRet = SVS_FALSE ;
    //m_oracleDiscoveryImplPtr.reset( new OracleDiscoveryImpl() ) ;
    if(m_oracleDiscoveryImplPtr != NULL)
    {
        m_oracleDiscoveryImplPtr->getInstance();
    }
    else
    {
        m_oracleDiscoveryImplPtr.reset( new OracleDiscoveryImpl() ) ;
    }
    //m_oraclemetaDataDiscoveryImplPtr.reset( new OracleMetaDataDiscoveryImpl() ) ;
    if( m_oracleDiscoveryImplPtr != NULL )
    {
            bRet = SVS_OK ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "OracleDiscoveryImpl::init failed\n") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED OracleApplication::%s\n", FUNCTION_NAME) ;
    return bRet ;
}
OracleApplication::OracleApplication()
{
    m_isInstalled = false ;
}

SVERROR OracleApplication::discoverApplication()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED OracleApplication::%s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    try
    {
        OracleAppDiscoveryInfo discoveryInfo ;
        if(m_oracleDiscoveryImplPtr == NULL)
        {
            init() ;
        }
        if( m_oracleDiscoveryImplPtr->discoverOracleApplication(/**oracleHostsIter, */discoveryInfo) == SVS_OK )
        {
            bRet = SVS_OK ;
            m_oracleAppDiscoveryInfo = discoveryInfo;

            std::map<std::string, OracleAppVersionDiscInfo> oracleDbDiscoveryInfo = discoveryInfo.m_dbOracleDiscInfo;
            std::map<std::string, OracleAppVersionDiscInfo>::iterator oracleAppDiscIter = oracleDbDiscoveryInfo.begin();

            while(oracleAppDiscIter != oracleDbDiscoveryInfo.end())
            {
                std::string checkDbName = oracleAppDiscIter->first;
                if(m_oracleDiscoveryInfoMap.find(oracleAppDiscIter->first) == m_oracleDiscoveryInfoMap.end())
                {
                    m_oracleDiscoveryInfoMap.insert(std::make_pair(oracleAppDiscIter->first, oracleAppDiscIter->second));
                }
                oracleAppDiscIter++ ;
            }
            //oracleHostsIter++ ;
        }
        else
        {
            // DebugPrintf(SV_LOG_ERROR, "OracleDiscoveryImpl::discoverOracleApplication Failed\n") ;
            bRet=SVS_FALSE;
        }
    }
    catch(...)
    {
        DebugPrintf(SV_LOG_ERROR, "Exception while discovering the Oracle application\n") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED OracleApplication::%s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR OracleApplication::discoverMetaData()
{
    SVERROR bRet = SVS_OK;
    return bRet;
            
}

SVERROR OracleApplication::validateApplicationProtection()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR OracleApplication::validateApplicationFailOver()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR OracleApplication::validateApplicationFailBack()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

std::string OracleApplication::summary(std::list<ValidatorPtr>& list)
{
		std::string stream ;
		return stream.c_str();
}


bool OracleApplication::isInstalled()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ; 
	//if( init("")  != SVS_OK )
    //{
		DebugPrintf(SV_LOG_ERROR, "Error while initilization\n");
	//}
	//else
	//{
 		//prepareOracleHostsList() ;
		//std::list<std::string>::const_iterator iterator = m_oracleHosts.begin() ;
		std::string oracleHost = Host::GetInstance().GetHostName();
        if( !m_isInstalled )
        {
		    //while( iterator != m_oracleHosts.end() )
		    //{
			    if( m_oracleDiscoveryImplPtr->isInstalled(oracleHost) )
			    {
				    DebugPrintf(SV_LOG_DEBUG, "exchange is installed on %s\n", oracleHost.c_str());//iterator->c_str()) ;
				    m_isInstalled = true ;
				    //break ;
			    }
			    //iterator++ ;
	    }
        //}
	//}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return m_isInstalled ;
}
void OracleApplication::clean()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_oracleDiscoveryInfoMap.clear();
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

std::list<std::string> OracleApplication::getActiveInstances()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<std::string> DatabaseNamesList ;
    std::map<std::string, OracleAppVersionDiscInfo>::iterator oracleDiscoveryIter = m_oracleDiscoveryInfoMap.begin() ;
    while( oracleDiscoveryIter != m_oracleDiscoveryInfoMap.end() )
    {
        DebugPrintf(SV_LOG_DEBUG, "pushing the database name %s\n", oracleDiscoveryIter->first.c_str()) ;
        DatabaseNamesList.push_back(oracleDiscoveryIter->first) ;
        oracleDiscoveryIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return DatabaseNamesList ;
}

std::map<std::string, std::list<std::string> > OracleApplication::getVersionInstancesMap()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<std::string, std::list<std::string> > versionInstancesMap ;
    std::list<std::string> InstanceNamesList ;
    std::string version ;
    std::map<std::string, OracleAppVersionDiscInfo>::iterator oracleDiscoveryIter = m_oracleDiscoveryInfoMap.begin() ;
    versionInstancesMap.insert(std::make_pair(version, InstanceNamesList)) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return versionInstancesMap ;
}

