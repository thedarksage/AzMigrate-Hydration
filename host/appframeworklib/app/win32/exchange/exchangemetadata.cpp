#include "exchangemetadata.h"
#include "host.h"
#include <sstream>
#include <set>
std::list<std::pair<std::string, std::string> > ExchangeMDBMetaData::getProperties()
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME );
    std::list<std::pair<std::string, std::string> > propertyList; 
    std::string storeType ;
    propertyList.push_back(std::make_pair("Mail Store Name", m_mdbName)) ;
    propertyList.push_back(std::make_pair("Offline Status", m_status)) ;
	if(m_exch2k10MDBmetaData.m_guid.empty() == false)
	{
		propertyList.push_back(std::make_pair("GUID", m_exch2k10MDBmetaData.m_guid)) ;
	}
	if(m_exch2k10MDBmetaData.m_mountInfo.empty() == false)
	{
		propertyList.push_back(std::make_pair("MountInfo", m_exch2k10MDBmetaData.m_mountInfo)) ;
	}
    
	propertyList.push_back(std::make_pair("Exchange Database Location", m_edbFilePath)) ;
	if(m_streamingDB.empty() == false)
	{
		propertyList.push_back(std::make_pair("Streaming Database Location", m_streamingDB)) ;
	}
	
	if( m_exch2k10MDBmetaData.m_dagName.empty() == false )
	{
		propertyList.push_back(std::make_pair("Exchange LogFile Location", m_exch2k10MDBmetaData.m_msExchESEParamLogFilePath)) ;// for exchange 2010 only we will show mailbox level.
		std::string hostName = Host::GetInstance().GetHostName();
		if(strcmpi( m_exch2k10MDBmetaData.m_dagName.c_str(), hostName.c_str()) != 0)
		{
			propertyList.push_back(std::make_pair("DataBase Availability Group", "TRUE")) ;
			propertyList.push_back(std::make_pair("DAG Name", m_exch2k10MDBmetaData.m_dagName)) ;
			propertyList.push_back(std::make_pair("Owning Server", m_exch2k10MDBmetaData.m_msExchOwningServer)) ;
			std::stringstream copyHostsStream;
			std::list<std::string>::iterator copyHostIter = m_exch2k10MDBmetaData.m_exchangeMDBCopysList.begin();
			while( copyHostIter != m_exch2k10MDBmetaData.m_exchangeMDBCopysList.end() )
			{					   
				copyHostsStream << *copyHostIter << " ";
				copyHostIter++;
			}
			if( copyHostsStream.str().empty() == false )
			{
				propertyList.push_back(std::make_pair("Copy Hosts ", copyHostsStream.str())) ;
			}
			else
			{
				DebugPrintf( SV_LOG_DEBUG, "There is no copies for Mail Store %s\n", m_mdbName.c_str());
			}
			std::stringstream participantsHostsStream;
			std::list<std::string>::iterator particiPantsHostIter = m_exch2k10MDBmetaData.m_participantsServersList.begin();
			while( particiPantsHostIter != m_exch2k10MDBmetaData.m_participantsServersList.end() )
			{
				participantsHostsStream << *particiPantsHostIter <<" ";
				particiPantsHostIter++;
			}
			if( participantsHostsStream.str().empty() == false )
			{
				propertyList.push_back(std::make_pair("Participant Hosts ", participantsHostsStream.str())) ;
			}
		}
		else
		{
			propertyList.push_back(std::make_pair("DataBase Availability Group", "FALSE")) ;
		}
	}
    if( this->m_type == EXCHANGE_PRIV_MDB )
    {   
        storeType = "Private Mail Store" ;
    }
    else if( this->m_type == EXCHANGE_PUB_MDB )
    {
        storeType = "Public Mail Store" ;
    }
    propertyList.push_back(std::make_pair("Mail Store Type", storeType)) ;
    if( m_edbCopyFilePath.compare("") != 0 )
    {
        propertyList.push_back(std::make_pair("Copy of Exchange Database File", this->m_edbCopyFilePath)) ;
    }
    propertyList.push_back(std::make_pair("Distinguished Name", this->m_distinguishedName)) ;
	if( this->m_type == EXCHANGE_PRIV_MDB )
    { 
		if(this->m_defaultPublicFolderDBName.empty() == false)
		{
		propertyList.push_back(std::make_pair("Public Folder Database Name", this->m_defaultPublicFolderDBName)) ;
		}
		if(this->m_defaultPublicFolderDBs_StorageGRPName.empty() == false)
		{
		propertyList.push_back(std::make_pair("Public Folder Database's Srorage Group Name", this->m_defaultPublicFolderDBs_StorageGRPName)) ;
		}
		if(this->m_defaultPublicFolderDBs_MailBoxServerName.empty() == false)
		{
		propertyList.push_back(std::make_pair("Public Folder Database's Mail Box Server Name", this->m_defaultPublicFolderDBs_MailBoxServerName)) ;
		}
	}
	std::stringstream volumeStram;
	std::list<std::string>::iterator volListIter = m_volumeList.begin();
	while( volListIter != m_volumeList.end() )
	{
		volumeStram << " "<< *volListIter << " ";
		volListIter++;
	}
	if( !m_volumeList.empty() )
	{
		propertyList.push_back(std::make_pair("Volumes ", volumeStram.str())) ;
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME );
	return propertyList ;
}
 std::list<std::pair<std::string, std::string> >ExchangeStorageGroupMetaData::getProperties()
{
    std::list<std::pair<std::string, std::string> > propertyList; 
    propertyList.push_back(std::make_pair("Storage Group Name", this->m_storageGrpName)) ;
    propertyList.push_back(std::make_pair("Transaction Log Location", this->m_logFilePath)) ;
    propertyList.push_back(std::make_pair("Log File Prefix", this->m_logFilePrefix)) ;
    propertyList.push_back(std::make_pair("System Path Location", this->m_systemPath)) ;
    propertyList.push_back(std::make_pair("Distinguished Name", this->m_dn)) ;
	propertyList.push_back(std::make_pair("msExchStandbyCopyMachines", this->m_msExchStandbyCopyMachines)) ;
    if( this->m_lcrEnabled )
    {
        propertyList.push_back(std::make_pair("Local Continuos Replication Status", "ENABLED")) ;
        propertyList.push_back(std::make_pair("Copy of Transaction Log Location", m_CopylogFilePath)) ;
        propertyList.push_back(std::make_pair("Copy of System Path Location", m_CopysystemPath)) ;
    }
    else
    {
        propertyList.push_back(std::make_pair("Local Continuos Replication Status", "DISABLED")) ;        
    }
    return propertyList ;
    
}
void ExchangeMetaData::getPublicFolderDBsMailBoxServerNameList(std::list<std::string>& publicFolderHosts)
{
	std::list<ExchangeStorageGroupMetaData>::iterator iter = m_storageGrps.begin() ;
	std::set<std::string> sampleSet ;
	while( iter != m_storageGrps.end() )
	{
		std::list<ExchangeMDBMetaData>::iterator mdbIter = iter->m_mdbMetaDataList.begin()  ;
		while( mdbIter != iter->m_mdbMetaDataList.end() )
		{
			if( mdbIter->m_type == EXCHANGE_PRIV_MDB )
			{
				sampleSet.insert(mdbIter->m_defaultPublicFolderDBs_MailBoxServerName) ;
			}
			mdbIter++ ;
		}		
		iter++ ;
	}
	publicFolderHosts.insert(publicFolderHosts.begin(), sampleSet.begin(), sampleSet.end()) ;
}

std::string ExchangeMetaData::summary()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ExchangeStorageGroupMetaData>::iterator storageGrpIter = m_storageGrps.begin() ;
    std::stringstream stream ;
    stream << std::endl ;
    stream << std::endl ;
    while( storageGrpIter != m_storageGrps.end() )
    {
        stream << storageGrpIter->summary() << std::endl ;
        storageGrpIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return stream.str() ;
}

std::string ExchangeStorageGroupMetaData::summary()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream stream ;
    std::list<std::pair<std::string, std::string> > propertyList = getProperties() ;
    stream << "\t\tMicrosoft Exchange Storage Group Information" <<std::endl << std::endl ;
    std::list<std::pair<std::string, std::string> >::const_iterator propertyIter = propertyList.begin() ;
    while( propertyIter != propertyList.end() )
    {
        stream << "\t\t\t " << propertyIter->first << "\t\t : " << propertyIter->second <<std::endl ;
        propertyIter++ ;
    }
    if( m_mdbMetaDataList.size() > 0 )
    {
        std::list<ExchangeMDBMetaData>::iterator mdbIter = m_mdbMetaDataList.begin() ;
        stream << "\n\t\t\tDatabases:" <<std::endl << std::endl ;
        while( mdbIter != m_mdbMetaDataList.end() )
        {
            stream << mdbIter->summary() ;
            mdbIter++ ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return stream.str();
}

std::string ExchangeMDBMetaData::summary()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream stream ;
    std::list<std::pair<std::string, std::string> > propertyList = getProperties() ;
    std::list<std::pair<std::string, std::string> >::const_iterator propertyIter = propertyList.begin() ;
    while( propertyIter != propertyList.end() )
    {
        stream << "\t\t\t\t " << propertyIter->first << "\t\t\t : " << propertyIter->second <<std::endl ;
        propertyIter++ ;
    }
    stream << std::endl ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return stream.str() ;
}

std::string Exchange2k10MetaData::summary()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream stream ;
	std::list<ExchangeMDBMetaData>::iterator mdbMetaDataListIter = m_mdbMetaDataList.begin();
	while( mdbMetaDataListIter != m_mdbMetaDataList.end() )
	{	
		stream << mdbMetaDataListIter->summary();
		mdbMetaDataListIter++;
	}
    stream << std::endl ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return stream.str() ;
}