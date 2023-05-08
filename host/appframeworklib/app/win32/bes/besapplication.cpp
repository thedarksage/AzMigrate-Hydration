#include "besapplication.h"
#include "registry.h"

BESAplication::BESAplication()
{
	isInstalled();
}

bool BESAplication::isInstalled() 
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    m_bInstalled = false;
    if( isKeyAvailable("SOFTWARE\\Research In Motion\\BlackBerry Enterprise Server"))
    {
        m_bInstalled = true;
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return m_bInstalled;
}

SVERROR BESAplication::discoverApplication()
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    SVERROR retStatus = SVS_OK ;
    if( m_bInstalled )
    {
        retStatus = m_BESAplicationImplPtr->discoverBESData(m_discoveredData);
    }
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return retStatus;
}

SVERROR BESAplication::discoverMetaData()
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    SVERROR retStatus = SVS_OK ;
    if( m_bInstalled )
    {
        retStatus = m_BESAplicationImplPtr->discoverBESMetaData(m_discoveredMetaData);
    }
	dumpDiscoveredInfo();
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return retStatus;
}

void BESAplication::dumpDiscoveredInfo()
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
	if( m_bInstalled )
	{
		DebugPrintf( SV_LOG_DEBUG, "Discoverd BES Data. \n" );
		DebugPrintf( SV_LOG_DEBUG, "##################### \n" );
		DebugPrintf( SV_LOG_DEBUG, "\tBES Setup Details \n" );
		DebugPrintf( SV_LOG_DEBUG, "\t****************** \n" );
		DebugPrintf( SV_LOG_DEBUG, "\t\tBES Server Name: %s\n", m_discoveredData.m_configServerName.c_str() );
		DebugPrintf( SV_LOG_DEBUG, "\t\tVersion: %s\n", m_discoveredData.m_installVersion.c_str());
		DebugPrintf( SV_LOG_DEBUG, "\t\tInstall Path: %s\n", m_discoveredData.m_installPath.c_str() );
		DebugPrintf( SV_LOG_DEBUG, "\t\tKey Store User Name: %s\n\n", m_discoveredData.m_configKeyStoreUserName.c_str() );
	
		DebugPrintf( SV_LOG_DEBUG, "\tBES Configuration Details. \n" );
		DebugPrintf( SV_LOG_DEBUG, "\t********************** \n" );
		std::list<std::string>::iterator mailboxServerNameIter = m_discoveredMetaData.m_exchangeServerNameList.begin();
		while(mailboxServerNameIter != m_discoveredMetaData.m_exchangeServerNameList.end())
		{
			DebugPrintf( SV_LOG_DEBUG, "\t\tMailBox Server Name: %s \n", mailboxServerNameIter->c_str());
			mailboxServerNameIter++;
		}
		DebugPrintf( SV_LOG_DEBUG, "\n");
		DebugPrintf( SV_LOG_DEBUG, "\t\tDataBase Name: %s\n", m_discoveredMetaData.m_dataBaseName.c_str() );
		DebugPrintf( SV_LOG_DEBUG, "\t\tDataBase Server FQN: %s\n", m_discoveredMetaData.m_dataBseServerMachineName.c_str() );

		DebugPrintf( SV_LOG_DEBUG, "#####################\n" );
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
}

void BESAplication::clean()
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    m_discoveredData.m_services.clear();
	m_discoveredMetaData.m_exchangeServerNameList.clear();
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
}

std::string BESAplication::getSummary()
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
    std::stringstream summaryStream;
    summaryStream << "\n BES Summary "<< std::endl;
    summaryStream << m_discoveredData.getSummary();
    summaryStream << m_discoveredMetaData.getSummary();
    DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
    return summaryStream.str();
}
