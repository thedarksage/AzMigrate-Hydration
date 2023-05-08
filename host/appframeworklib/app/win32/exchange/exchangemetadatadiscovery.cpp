#include "exchangemetadatadiscovery.h"
#include "host.h"
#include "portablehelpersmajor.h"
#include "util/common/util.h"
#include "registry.h"
#include <sstream>
boost::shared_ptr<ExchangeMetaDataDiscoveryImpl> ExchangeMetaDataDiscoveryImpl::m_instance ;

boost::shared_ptr<ExchangeMetaDataDiscoveryImpl> ExchangeMetaDataDiscoveryImpl::getInstance()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTETED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    if( m_instance.get() == NULL )
    {
        m_instance.reset( new ExchangeMetaDataDiscoveryImpl() ) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ; 
    return m_instance ;
}
bool GetVolumePath(const std::string& filePath, std::string& volPath)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool retVal = false ;
    int statRetVal ;
    ACE_stat stat ;

    do
    {
        if( (statRetVal = sv_stat( getLongPathName(filePath.c_str()).c_str(), &stat )) != 0 )
        {
            DebugPrintf(SV_LOG_WARNING, "Unable to stat %s. This can be ignored if the volume that contains the file/folder is not available\n", filePath.c_str()) ;
            break ;
        }
        CHAR driveLetter[1024] ;
        // PR#10815: Long Path support
        if (SVGetVolumePathName(filePath.c_str(), driveLetter, ARRAYSIZE(driveLetter)) == FALSE)
        {
            DebugPrintf(SV_LOG_ERROR, "SVGetVolumePathName for %s is failed with error %ld \n", filePath.c_str(), GetLastError()) ;
            break ;
        }
        if(  (statRetVal = sv_stat( getLongPathName(filePath.c_str()).c_str(), &stat )) != 0 )
        {
            DebugPrintf(SV_LOG_WARNING, "Unable to stat %s. This can be ignored if the volume that contains file/folder is not available\n", filePath.c_str()) ;
            break ;
        }
        retVal = true ;
        volPath = driveLetter ;
    } while( false ) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retVal ;
}

SVERROR ExchangeMetaDataDiscoveryImpl::init(const std::string& dnsname)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    if( m_ldapHelper.get() == NULL )
    {
        m_ldapHelper.reset(new ExchangeLdapHelper() ) ;
    }
    if( m_ldapHelper->init(dnsname) == SVS_OK )
    {
        bRet = SVS_OK ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "ExchangeLdapHelper::init Failed\n") ;
        m_ldapHelper.reset() ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR ExchangeMetaDataDiscoveryImpl::getMDBMetaData(const std::string storageGrpName, const std::string& mdbName, const ExchangeMdbType& mdbType, ExchangeMDBMetaData& mdbMetaData)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    std::stringstream stream;
	mdbMetaData.errCode = DISCOVERY_FAIL;
    std::string driveletter ;
    std::multimap<std::string, std::string> attrMap ;
    bool failed = false  ;
    if( m_ldapHelper->getMDBAttributes(m_hostname, storageGrpName, mdbType, mdbName, attrMap) == SVS_OK )
    {
        mdbMetaData.m_mdbName = mdbName ;
        mdbMetaData.m_type = mdbType ;
        mdbMetaData.m_distinguishedName = attrMap.find("distinguishedName")->second ;
        mdbMetaData.m_edbFilePath = attrMap.find("msExchEDBFile")->second ;
		if( attrMap.find("msExchHomePublicMDB") != attrMap.end() )
		{
		     mdbMetaData.m_defaultPublicFolderDB = attrMap.find("msExchHomePublicMDB")->second ;
			std::string defaultPublicFolderDB = mdbMetaData.m_defaultPublicFolderDB;

			size_t index1 = defaultPublicFolderDB.find_first_of("=");
			size_t index2 = defaultPublicFolderDB.find_first_of(",");
			index1++;
			mdbMetaData.m_defaultPublicFolderDBName = defaultPublicFolderDB.substr(index1, index2-index1 );

			defaultPublicFolderDB = defaultPublicFolderDB.substr(index2+1);
			index1 = defaultPublicFolderDB.find_first_of("=");
			index2 = defaultPublicFolderDB.find_first_of(",");
			index1++;
			mdbMetaData.m_defaultPublicFolderDBs_StorageGRPName = defaultPublicFolderDB.substr(index1, index2-index1 );

			defaultPublicFolderDB = defaultPublicFolderDB.substr(index2+1);
			index2 = defaultPublicFolderDB.find_first_of(",");
			defaultPublicFolderDB = defaultPublicFolderDB.substr(index2+1);
			index1 = defaultPublicFolderDB.find_first_of("=");
			index2 = defaultPublicFolderDB.find_first_of(",");
			index1++;
			mdbMetaData.m_defaultPublicFolderDBs_MailBoxServerName = defaultPublicFolderDB.substr(index1, index2-index1 );

		}
		if( attrMap.find("msExchHasLocalCopy") != attrMap.end() )
        {
            if( attrMap.find("msExchHasLocalCopy")->second.compare("1") == 0 )
            {
                mdbMetaData.m_edbCopyFilePath = attrMap.find("msExchCopyEDBFile")->second ;
            }
        }
        DebugPrintf(SV_LOG_DEBUG, "edbFilePath %s\n", mdbMetaData.m_edbFilePath.c_str()) ;
        if( GetVolumePath(mdbMetaData.m_edbFilePath, driveletter) )
        {
            DebugPrintf(SV_LOG_DEBUG, "Drive Letter for edb file path %s\n", driveletter.c_str()) ;
            if( find(mdbMetaData.m_volumeList.begin(), mdbMetaData.m_volumeList.end(), driveletter) == mdbMetaData.m_volumeList.end() )
            {
                mdbMetaData.m_volumeList.push_back(driveletter) ;
            }
            mdbMetaData.m_edbVol = driveletter ;

            
        }
        else
        {
            stream << "Unable to get the volume path name for: " << mdbMetaData.m_edbFilePath << std::endl ;
            stream << "This can be ignored if the volume that contains " << mdbMetaData.m_edbFilePath << " is not available " << std::endl ;
            failed = true ;
        }
        if( attrMap.find("msExchCopyEDBFile") != attrMap.end() )
        {
            mdbMetaData.m_edbCopyFilePath = attrMap.find("msExchCopyEDBFile")->second ;
            DebugPrintf(SV_LOG_DEBUG, "Copy edbFilePath %s\n", mdbMetaData.m_edbCopyFilePath.c_str()) ;
            if( GetVolumePath(mdbMetaData.m_edbCopyFilePath, driveletter) )
            {
                DebugPrintf(SV_LOG_DEBUG, "Drive Letter for copy edb file path %s\n", driveletter.c_str()) ;
                if( find(mdbMetaData.m_volumeList.begin(), mdbMetaData.m_volumeList.end(), driveletter) == mdbMetaData.m_volumeList.end() )
                {
                    mdbMetaData.m_volumeList.push_back(driveletter) ;
                }
            }
            else
            {
                stream << "Unable to get the volume path name for: " << mdbMetaData.m_edbCopyFilePath << std::endl ;
                stream << "This can be ignored if the volume that contains " << mdbMetaData.m_edbCopyFilePath << " is not available " << std::endl ;
                failed = true ;
            }
        }
		if( attrMap.find("msExchOwningServer") != attrMap.end() )
		{
			mdbMetaData.m_exch2k10MDBmetaData.m_msExchOwningServer = attrMap.find("msExchOwningServer")->second;
			DebugPrintf(SV_LOG_DEBUG, "m_msExchOwningServer: %s\n", mdbMetaData.m_exch2k10MDBmetaData.m_msExchOwningServer.c_str()) ;
		}
		if(attrMap.find("msExchSLVFile") != attrMap.end())
		{
			mdbMetaData.m_streamingDB = attrMap.find("msExchSLVFile")->second;
			DebugPrintf(SV_LOG_DEBUG, "streamingDB: %s\n", mdbMetaData.m_streamingDB.c_str()) ;
            if( GetVolumePath(mdbMetaData.m_streamingDB, driveletter) )
			{
                DebugPrintf(SV_LOG_DEBUG, "Drive Letter for streamingDB %s\n", driveletter.c_str()) ;
				if( find(mdbMetaData.m_volumeList.begin(), mdbMetaData.m_volumeList.end(), driveletter) == mdbMetaData.m_volumeList.end() )
				{
					mdbMetaData.m_volumeList.push_back(driveletter) ;
				}
                mdbMetaData.m_streamingVol = driveletter ;

			}
			else
			{
                stream << "Unable to get the volume path name for: " << mdbMetaData.m_streamingDB << std::endl ;
                stream << "This can be ignored if the volume that contains " << mdbMetaData.m_streamingDB << " is not available " << std::endl ;
                failed = true ;
			}
		}
        mdbMetaData.m_status = attrMap.find("msExchEDBOffline")->second ;
        bRet = SVS_OK ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "ExchangeLdapHelper::getMDBAttributes Failed for %s\n", mdbName.c_str()) ;
        stream << "Failed to get MDB Attributes for : "<<mdbName;
        failed = true ;
    }
    if( !failed )
    {
        mdbMetaData.errCode = DISCOVERY_SUCCESS;
        stream << "Successfully got MDB Attributes for : " <<mdbName; 
        bRet = SVS_OK ;
    }
    else
    {
        mdbMetaData.errCode = DISCOVERY_FAIL ;  
        bRet = SVS_FALSE ;
        stream << "Failed to get MDB Attributes for : "<<mdbName;
    }
    mdbMetaData.errString = stream.str();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR ExchangeMetaDataDiscoveryImpl::getStorageGroupMetaData(const std::string storageGrpName, ExchangeStorageGroupMetaData& storageGrpMetaData, enum DiscoveryOption)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    bool failed = false ;
    storageGrpMetaData.errCode = DISCOVERY_FAIL;
    std::stringstream stream;
    std::multimap<std::string, std::string> attributesMap ;
    std::string driveletter ;
    if( m_ldapHelper->getStorageGroupAttributes(m_hostname, storageGrpName, attributesMap) == SVS_OK )
    {
		std::multimap<std::string, std::string>::iterator attributesMapIter;
		attributesMapIter = attributesMap.find("distinguishedName");
		if(attributesMapIter != attributesMap.end())
			storageGrpMetaData.m_dn = attributesMapIter->second ;

		attributesMapIter = attributesMap.find("msExchESEParamLogFilePath");
		if(attributesMapIter != attributesMap.end())
			storageGrpMetaData.m_logFilePath = attributesMapIter->second ;

        DebugPrintf(SV_LOG_DEBUG, "Log FilePath %s\n", storageGrpMetaData.m_logFilePath.c_str()) ;
        if( GetVolumePath(storageGrpMetaData.m_logFilePath, driveletter) )
        {
            DebugPrintf(SV_LOG_DEBUG, "Drive Letter for log file path %s\n", driveletter.c_str()) ;
			storageGrpMetaData.m_logVolumePath = driveletter;
        }
        else
        {
            stream << "Failed to get the log volume path for: " << storageGrpMetaData.m_logFilePath;
			DebugPrintf(SV_LOG_ERROR, "Failed to get storage group log path volume\n");
            failed = true ;
        }

		attributesMapIter = attributesMap.find("msExchESEParamBaseName");
		if(attributesMapIter != attributesMap.end())
			storageGrpMetaData.m_logFilePrefix = attributesMapIter->second ;

		attributesMapIter = attributesMap.find("cn");
		if(attributesMapIter != attributesMap.end())
			storageGrpMetaData.m_storageGrpName  = attributesMapIter->second ;

		attributesMapIter = attributesMap.find("msExchESEParamSystemPath");
		if(attributesMapIter != attributesMap.end())
			storageGrpMetaData.m_systemPath = attributesMapIter->second ;
        
        driveletter = "";
		if( GetVolumePath(storageGrpMetaData.m_systemPath, driveletter) )
		{
			DebugPrintf(SV_LOG_DEBUG, "Drive Letter for system path %s\n", driveletter.c_str()) ;
			sanitizeVolumePathName(driveletter);
			storageGrpMetaData.m_systemVolumePath = driveletter;
		}
		else
		{
			failed = true;
			stream << "Failed to get the system volume path for: " << storageGrpMetaData.m_systemPath;
			DebugPrintf(SV_LOG_ERROR, "Failed to get storage group system path volume\n");
		}
        
		attributesMapIter = attributesMap.find("msExchStandbyCopyMachines");
		if( attributesMapIter != attributesMap.end() )
		    storageGrpMetaData.m_msExchStandbyCopyMachines = attributesMapIter->second ;

		DebugPrintf(SV_LOG_DEBUG, " The Value of m_msExchStandbyCopyMachines %s\n", storageGrpMetaData.m_msExchStandbyCopyMachines.c_str() );
		storageGrpMetaData.m_lcrEnabled = false ;
        if( attributesMap.find("msExchHasLocalCopy") != attributesMap.end() )
        {
            if( attributesMap.find("msExchHasLocalCopy")->second.compare("1") == 0 )
            {
                storageGrpMetaData.m_lcrEnabled = true ;
				if( attributesMap.find("msExchESEParamCopyLogFilePath") != attributesMap.end() )
					storageGrpMetaData.m_CopylogFilePath = attributesMap.find("msExchESEParamCopyLogFilePath")->second ;
				if( attributesMap.find("msExchESEParamCopySystemPath") != attributesMap.end() )
					storageGrpMetaData.m_CopysystemPath = attributesMap.find("msExchESEParamCopySystemPath")->second ;
            }
        }
        std::list<std::string> mdbList;
        std::list<std::string>::iterator mdbIter ;
        if( m_ldapHelper->getMDBs(m_hostname, storageGrpName, EXCHANGE_PRIV_MDB, mdbList) == SVS_OK )
        {
            mdbIter = mdbList.begin() ;
            while( mdbIter != mdbList.end() )
            {
                ExchangeMDBMetaData mdbMetaData ;
                mdbMetaData.m_volumeList.clear() ;
                if( getMDBMetaData(storageGrpName, *mdbIter, EXCHANGE_PRIV_MDB, mdbMetaData) != SVS_OK )
                {
                    failed = true ;
                    DebugPrintf(SV_LOG_WARNING, "Unable to get mdb metadata for storage grp name : %s and mdb name : %s\n", storageGrpName.c_str(), mdbIter->c_str()) ;
                    stream << "Unable to get MDB metadata for: " << mdbIter->c_str();
                    stream<< "stroage group name: "<< storageGrpName;
                     
                }
                else
                {
                    std::list<std::string>::iterator volIter = mdbMetaData.m_volumeList.begin() ;
                    while( volIter != mdbMetaData.m_volumeList.end() )
                    {
                        if( find(storageGrpMetaData.m_volumeList.begin(), storageGrpMetaData.m_volumeList.end(), *volIter) == storageGrpMetaData.m_volumeList.end() )
                        {
                            storageGrpMetaData.m_volumeList.push_back(*volIter) ;
                        }
                        volIter++ ;
                    }
                }
                    storageGrpMetaData.m_mdbMetaDataList.push_back(mdbMetaData) ;
                mdbIter++ ;
            }
        }
        else
        {
            failed = true ;
            DebugPrintf(SV_LOG_ERROR, "ExchangeLdapHelper::getMdbs Failed for %s\n", storageGrpName.c_str()) ;
            stream << "Failed to get Mdbs for storage group "<< storageGrpName;
        }
        mdbList.clear() ;
        if( m_ldapHelper->getMDBs(m_hostname, storageGrpName, EXCHANGE_PUB_MDB, mdbList) == SVS_OK )
        {
            mdbIter = mdbList.begin() ;
            while( mdbIter != mdbList.end() )
            {
                ExchangeMDBMetaData mdbMetaData ;
                mdbMetaData.m_volumeList.clear() ;
                if( getMDBMetaData(storageGrpName, *mdbIter, EXCHANGE_PUB_MDB, mdbMetaData) != SVS_OK )
                {
                    failed = true ;
                    DebugPrintf(SV_LOG_WARNING, "Unable to get mdb metadata for storage grp name : %s and mdb name : %s\n", storageGrpName.c_str(), mdbIter->c_str()) ;
                     stream << "Unable to get MDB metadata for: " << mdbIter->c_str();
                    stream<< "stroage group name: "<< storageGrpName;
                }
                else
                {
                    std::list<std::string>::iterator volIter = mdbMetaData.m_volumeList.begin() ;
                    while( volIter != mdbMetaData.m_volumeList.end() )
                    {
                        if( find(storageGrpMetaData.m_volumeList.begin(), storageGrpMetaData.m_volumeList.end(), *volIter) == storageGrpMetaData.m_volumeList.end() )
                        {
                            storageGrpMetaData.m_volumeList.push_back(*volIter) ;
                        }
                        volIter++ ;
                    }
                }
                    storageGrpMetaData.m_mdbMetaDataList.push_back(mdbMetaData) ;
                mdbIter++ ;
            }
        }
        else
        {
            failed = true ;
            DebugPrintf(SV_LOG_ERROR, "ExchangeLdapHelper::getMdbs Failed for %s\n", storageGrpName.c_str()) ;
            stream << "Failed to get Mdbs for storage group "<< storageGrpName;
        }
    }
    else
    {
        failed = true ;
        DebugPrintf(SV_LOG_ERROR, "ExchangeLdapHelper::getStorageGroupAttributes Failed for %s\n", storageGrpName.c_str()) ;
        stream << "Failed to get storage group attributes for  storage group "<< storageGrpName;
    }
    if( !failed )
    {
        bRet = SVS_OK ;
        storageGrpMetaData.errCode = DISCOVERY_SUCCESS;
        stream << "Successfully got storage group attributes for  storage group "<< storageGrpName;

    }
    else
    {
        storageGrpMetaData.errCode = DISCOVERY_FAIL ;
        bRet = SVS_FALSE ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    storageGrpMetaData.errString = stream.str();
    return bRet ;
}

SVERROR ExchangeMetaDataDiscoveryImpl::getExchangeMetaData(const std::string& exchHost, ExchangeMetaData& metadata,
														   enum DiscoveryOption)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    std::stringstream stream;
	metadata.errCode = DISCOVERY_FAIL;
    bool failed = false ;
    m_hostname = exchHost ;
    std::list<std::string> storageGroupsList;
    if( m_ldapHelper->getStorageGroups(m_hostname, storageGroupsList) == SVS_OK )
    {
        std::list<std::string>::iterator storageGrpIter = storageGroupsList.begin();
        while( storageGrpIter != storageGroupsList.end() )
        {
            ExchangeStorageGroupMetaData storageGrpMetaData ;
            if( getStorageGroupMetaData(*storageGrpIter, storageGrpMetaData) != SVS_OK )
            {
                failed = true ;
                stream << "Unable to get storage group meta data: " << storageGrpIter->c_str();
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "Storage Group %s and Number of volumes %d\n", storageGrpMetaData.m_storageGrpName.c_str(), storageGrpMetaData.m_volumeList.size()) ;
                std::list<std::string>::iterator volIter = storageGrpMetaData.m_volumeList.begin() ;
                while( volIter != storageGrpMetaData.m_volumeList.end() )
                {
                    if( find(metadata.m_volumeList.begin(), metadata.m_volumeList.end(), *volIter) == metadata.m_volumeList.end() )
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Adding %s to metadata volumes list\n", volIter->c_str()) ;
                        metadata.m_volumeList.push_back(*volIter) ;
                    }
                    volIter++ ;
                }
            }
                metadata.m_storageGrps.push_back( storageGrpMetaData) ;
            storageGrpIter++ ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "ExchangeLdapHelper::getStorageGroups Failed\n") ;
        stream << "Failed  to get storage groups";
        failed = true ;
    }
    if( !failed )
    {
        bRet = SVS_OK ;
        metadata.errCode = DISCOVERY_SUCCESS;
        stream << "Successfully got Metadata Information";
    }
    else
    {
        metadata.errCode = DISCOVERY_FAIL ;
        bRet = SVS_FALSE ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    metadata.errString = stream.str();
    return bRet ;
}

SVERROR ExchangeMetaDataDiscoveryImpl::Display(const ExchangeMetaData& metadata)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ExchangeStorageGroupMetaData>::const_iterator storageGrpIter = metadata.m_storageGrps.begin() ;
    std::list<std::string>::const_iterator volIter ;
    while( storageGrpIter != metadata.m_storageGrps.end() )
    {
        DebugPrintf(SV_LOG_DEBUG, "Dn %s\n", storageGrpIter->m_dn.c_str()) ;
        DebugPrintf(SV_LOG_DEBUG, "Log File Path %s\n", storageGrpIter->m_logFilePath.c_str()) ;
        DebugPrintf(SV_LOG_DEBUG, "Log File Prefix %s\n", storageGrpIter->m_logFilePrefix.c_str()) ;
        DebugPrintf(SV_LOG_DEBUG, "Storage Group Name %s\n", storageGrpIter->m_storageGrpName.c_str()) ;
        DebugPrintf(SV_LOG_DEBUG, "System Path %s\n", storageGrpIter->m_systemPath.c_str()) ;
        DebugPrintf(SV_LOG_DEBUG, "Volume Information at Storage Group Level\n") ;
        volIter = storageGrpIter->m_volumeList.begin() ;
        while( volIter != storageGrpIter->m_volumeList.end() )
        {
            DebugPrintf(SV_LOG_DEBUG, "\t%s\n", volIter->c_str()) ;
            volIter++ ;
        }

        std::list<ExchangeMDBMetaData>::const_iterator mdbIter = storageGrpIter->m_mdbMetaDataList.begin() ;
        while( mdbIter != storageGrpIter->m_mdbMetaDataList.end() )
        {
            DebugPrintf(SV_LOG_DEBUG, "\tDn %s\n", mdbIter->m_distinguishedName.c_str()); 
            DebugPrintf(SV_LOG_DEBUG, "\tmdb name %s\n", mdbIter->m_mdbName.c_str()) ;
            DebugPrintf(SV_LOG_DEBUG, "\tedbCopyFilePath %s\n", mdbIter->m_edbCopyFilePath.c_str()) ;
            DebugPrintf(SV_LOG_DEBUG, "\tedbFilePath %s\n", mdbIter->m_edbFilePath.c_str()) ;
            DebugPrintf(SV_LOG_DEBUG, "\tStatus %s\n", mdbIter->m_status.c_str()) ;
            DebugPrintf(SV_LOG_DEBUG, "\tmdb Type %d\n", mdbIter->m_type) ;
            DebugPrintf(SV_LOG_DEBUG, "\tVolume Information at MDB Level\n") ;
            volIter = mdbIter->m_volumeList.begin() ;
            while( volIter != mdbIter->m_volumeList.end() )
            {
                DebugPrintf(SV_LOG_DEBUG, "\t\t%s\n", volIter->c_str()) ;
                volIter++ ;
            }
            mdbIter++ ;
        }
        storageGrpIter++ ;
    }
    volIter = metadata.m_volumeList.begin() ;
    DebugPrintf(SV_LOG_DEBUG, "Volume Information at Exchange Metadata Level\n") ;
    while( volIter != metadata.m_volumeList.end() )
    {
        DebugPrintf(SV_LOG_DEBUG, "\t\t%s\n", volIter->c_str()) ;
        volIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return SVS_OK ;
}

SVERROR ExchangeMetaDataDiscoveryImpl::get2010MDBMetaData( const std::string& mdbName, const ExchangeMdbType& mdbType, ExchangeMDBMetaData& mdbMetaData )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
	mdbMetaData.errCode = DISCOVERY_FAIL;
    std::string driveletter ;
    std::multimap<std::string, std::string> attrMap ;
	std::string errorString;
	bool failed = false ;
    if( m_ldapHelper->get2010MDBAttributes(m_hostname, mdbType, mdbName, attrMap) == SVS_OK )
    {
        mdbMetaData.m_mdbName = mdbName ;
        mdbMetaData.m_type = mdbType ;
		if(attrMap.find("distinguishedName") != attrMap.end())
			mdbMetaData.m_distinguishedName = attrMap.find("distinguishedName")->second ;

		if(attrMap.find("msExchEDBFile") != attrMap.end())
			mdbMetaData.m_edbFilePath = attrMap.find("msExchEDBFile")->second ;

		if((attrMap.find("msExchRestore") != attrMap.end()) && (attrMap.find("msExchRestore")->second.compare("TRUE") == 0))
		{
			DebugPrintf(SV_LOG_INFO," Igonring the mailstore %s. It is a recovery database.\n",mdbMetaData.m_distinguishedName.c_str());
			mdbMetaData.m_bRecoveryDatabase = true;
		}
		else
		{
			if( attrMap.find("msExchHomePublicMDB") != attrMap.end() )
			{
				mdbMetaData.m_defaultPublicFolderDB = attrMap.find("msExchHomePublicMDB")->second ;
				std::string defaultPublicFolderDB = mdbMetaData.m_defaultPublicFolderDB;

				size_t index1 = defaultPublicFolderDB.find_first_of("=");
				size_t index2 = defaultPublicFolderDB.find_first_of(",");
				index1++;
				mdbMetaData.m_defaultPublicFolderDBName = defaultPublicFolderDB.substr(index1, index2-index1 );

				defaultPublicFolderDB = defaultPublicFolderDB.substr(index2+1);
				index1 = defaultPublicFolderDB.find_first_of("=");
				index2 = defaultPublicFolderDB.find_first_of(",");
				index1++;
				mdbMetaData.m_defaultPublicFolderDBs_StorageGRPName = defaultPublicFolderDB.substr(index1, index2-index1 );

				defaultPublicFolderDB = defaultPublicFolderDB.substr(index2+1);
				index2 = defaultPublicFolderDB.find_first_of(",");
				defaultPublicFolderDB = defaultPublicFolderDB.substr(index2+1);
				index1 = defaultPublicFolderDB.find_first_of("=");
				index2 = defaultPublicFolderDB.find_first_of(",");
				index1++;
				mdbMetaData.m_defaultPublicFolderDBs_MailBoxServerName = defaultPublicFolderDB.substr(index1, index2-index1 );

			}
			if( attrMap.find("msExchHasLocalCopy") != attrMap.end() )
			{
				if( attrMap.find("msExchHasLocalCopy")->second.compare("1") == 0 )
				{
					mdbMetaData.m_edbCopyFilePath = attrMap.find("msExchCopyEDBFile")->second ;
				}
			}
			DebugPrintf(SV_LOG_DEBUG, "edbFilePath %s\n", mdbMetaData.m_edbFilePath.c_str()) ;
			if( GetVolumePath(mdbMetaData.m_edbFilePath, driveletter) )
			{
				DebugPrintf(SV_LOG_DEBUG, "Drive Letter for edb file path %s\n", driveletter.c_str()) ;
				if( find(mdbMetaData.m_volumeList.begin(), mdbMetaData.m_volumeList.end(), driveletter) == mdbMetaData.m_volumeList.end() )
				{
					mdbMetaData.m_volumeList.push_back(driveletter) ;
				}
				mdbMetaData.m_edbVol = driveletter ;
	            
			}
			else
			{
				failed = true ;
			}
			if( attrMap.find("msExchCopyEDBFile") != attrMap.end() )
			{
				mdbMetaData.m_edbCopyFilePath = attrMap.find("msExchCopyEDBFile")->second ;
				DebugPrintf(SV_LOG_DEBUG, "Copy edbFilePath %s\n", mdbMetaData.m_edbCopyFilePath.c_str()) ;
				if( GetVolumePath(mdbMetaData.m_edbCopyFilePath, driveletter) )
				{
					DebugPrintf(SV_LOG_DEBUG, "Drive Letter for copy edb file path %s\n", driveletter.c_str()) ;
					if( find(mdbMetaData.m_volumeList.begin(), mdbMetaData.m_volumeList.end(), driveletter) == mdbMetaData.m_volumeList.end() )
					{
						mdbMetaData.m_volumeList.push_back(driveletter) ;
					}
				}
				else
				{
					failed = true ;
				}
			}
			if( attrMap.find("msExchOwningServer") != attrMap.end() )
			{
				std::string owningServerDn = attrMap.find("msExchOwningServer")->second;
				DebugPrintf(SV_LOG_DEBUG, "owningServerDn: %s\n", owningServerDn.c_str()) ;
				if(owningServerDn.empty() == false)
				{
					mdbMetaData.m_exch2k10MDBmetaData.m_msExchOwningServer = owningServerDn.substr(3, owningServerDn.find_first_of(",")-3);
				}
			}
			if( attrMap.find("msExchMasterServerOrAvailabilityGroup") != attrMap.end() )
			{
				std::string msExchMasterServerOrAvailabilityGroupDn = attrMap.find("msExchMasterServerOrAvailabilityGroup")->second;
				DebugPrintf(SV_LOG_DEBUG, "msExchMasterServerOrAvailabilityGroupDN : %s\n", msExchMasterServerOrAvailabilityGroupDn.c_str()) ;
				if(msExchMasterServerOrAvailabilityGroupDn.empty() == false)
				{
					mdbMetaData.m_exch2k10MDBmetaData.m_dagName = msExchMasterServerOrAvailabilityGroupDn.substr(3, msExchMasterServerOrAvailabilityGroupDn.find_first_of(",")-3);
					std::string hostName = Host::GetInstance().GetHostName();
					if( strcmpi( mdbMetaData.m_exch2k10MDBmetaData.m_dagName.c_str(), hostName.c_str() ) )
					{
						mdbMetaData.m_exch2k10MDBmetaData.m_isDAGParticipant = true;
					}
				
				}
			}
			
			if( attrMap.find("msExchESEParamLogFilePath") != attrMap.end() )
			{
				mdbMetaData.m_exch2k10MDBmetaData.m_msExchESEParamLogFilePath = attrMap.find("msExchESEParamLogFilePath")->second;
				DebugPrintf(SV_LOG_DEBUG, "msExchESEParamLogFilePath : %s\n", mdbMetaData.m_exch2k10MDBmetaData.m_msExchESEParamLogFilePath.c_str()) ;
				if( GetVolumePath(mdbMetaData.m_exch2k10MDBmetaData.m_msExchESEParamLogFilePath, driveletter) )
			{
					DebugPrintf(SV_LOG_DEBUG, "Drive Letter for msExchESEParamLogFilePath %s\n", driveletter.c_str()) ;
					if( find(mdbMetaData.m_volumeList.begin(), mdbMetaData.m_volumeList.end(), driveletter) == mdbMetaData.m_volumeList.end() )
					{
						mdbMetaData.m_volumeList.push_back(driveletter) ;
					}
					mdbMetaData.m_exch2k10MDBmetaData.m_msExchESEParamLogFileVol = driveletter ;

			}
			else
			{
				failed = true ;
			}		
		}
		if(attrMap.find("msExchEDBOffline") != attrMap.end())
			mdbMetaData.m_status = attrMap.find("msExchEDBOffline")->second ;
			if( attrMap.find("objectGUID") != attrMap.end() )
			{
				mdbMetaData.m_exch2k10MDBmetaData.m_guid = attrMap.find("objectGUID")->second;
				if( mdbMetaData.m_exch2k10MDBmetaData.m_isDAGParticipant == true )
				{
					if(getStringValueEx("Cluster\\ExchangeActiveManager\\DbState", mdbMetaData.m_exch2k10MDBmetaData.m_guid, mdbMetaData.m_exch2k10MDBmetaData.m_mountInfo) != SVS_OK)
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to get the registry info from Cluster\\ExchangeActiveManager\\DbState. Key: %s \n", mdbMetaData.m_exch2k10MDBmetaData.m_guid.c_str());
					}
				}
				else
				{
					if(getStringValueEx("SOFTWARE\\Microsoft\\ExchangeServer\\v14\\ActiveManager\\DbState", mdbMetaData.m_exch2k10MDBmetaData.m_guid, mdbMetaData.m_exch2k10MDBmetaData.m_mountInfo) != SVS_OK)
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to get the registry info from SOFTWARE\\Microsoft\\ExchangeServer\\v14\\ActiveManager\\DbState. Key: %s \n", mdbMetaData.m_exch2k10MDBmetaData.m_guid.c_str());
					}
				}
			}
		}
		bRet = SVS_OK ;
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "ExchangeLdapHelper::getMDBAttributes Failed for %s\n", mdbName.c_str()) ;
        errorString = "Failed to get MDB Attributes for : ";
        errorString += mdbName;
        failed = true ;
    }
    if( !failed )
    {
        bRet = SVS_OK ;
		mdbMetaData.errCode = DISCOVERY_SUCCESS;
    }
    else
    {
        mdbMetaData.errCode = DISCOVERY_FAIL ;
		mdbMetaData.errString = errorString;
        bRet = SVS_FALSE ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
	
}
SVERROR ExchangeMetaDataDiscoveryImpl::getExchange2k10MetaData(const std::string& exchHost, Exchange2k10MetaData& metadata,
														   enum DiscoveryOption)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
	metadata.errCode = DISCOVERY_FAIL;
	std::list<std::string> mdbList;
    std::list<std::string>::iterator mdbIter ;
	m_hostname = exchHost ;
    bool failed = false ;
    if( m_ldapHelper->get2010MDBs(m_hostname, mdbList) == SVS_OK )
    {
        mdbIter = mdbList.begin() ;
        while( mdbIter != mdbList.end() )
        {
            ExchangeMDBMetaData mdbMetaData ;
            mdbMetaData.m_volumeList.clear() ;
			mdbMetaData.m_bRecoveryDatabase = false;
            if( get2010MDBMetaData(*mdbIter, EXCHANGE_PRIV_MDB, mdbMetaData) != SVS_OK )
			{
                DebugPrintf(SV_LOG_ERROR, "Failed to get mdb metadata with private db property search\n" ) ;
				ExchangeMDBMetaData public_MDBMetaData ;
				if( get2010MDBMetaData(*mdbIter, EXCHANGE_PUB_MDB, public_MDBMetaData) != SVS_OK )
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to get mdb metadata with Public db property search also\n" ) ;
					failed = true;
				}
				else
				{
					std::list<std::string>::iterator volIter = public_MDBMetaData.m_volumeList.begin();
					while(volIter != public_MDBMetaData.m_volumeList.end())
					{
						if( isfound(metadata.m_volumePathNameList, *volIter) == false )
						{
							metadata.m_volumePathNameList.push_back(*volIter);
						}
						volIter++;
					}
				}
					metadata.m_mdbMetaDataList.push_back(public_MDBMetaData);				

			}
            else
            {
				if(!mdbMetaData.m_bRecoveryDatabase)
				{
					std::list<std::string>::iterator volIter = mdbMetaData.m_volumeList.begin();
					while(volIter != mdbMetaData.m_volumeList.end())
					{
						if( isfound(metadata.m_volumePathNameList, *volIter) == false )
						{
							metadata.m_volumePathNameList.push_back(*volIter);
						}
						volIter++;
					}
					if( mdbMetaData.m_exch2k10MDBmetaData.m_isDAGParticipant && m_ldapHelper->get2010MDBCopyHosts( *mdbIter, mdbMetaData.m_exch2k10MDBmetaData.m_exchangeMDBCopysList ) != SVS_OK )
					{
						DebugPrintf( SV_LOG_ERROR, "Failed to get the Copy host name list \n" );
						failed = true;
					}
					metadata.m_mdbMetaDataList.push_back(mdbMetaData);
				}
				else
				{
					DebugPrintf(SV_LOG_INFO, "Ingoning the mailstore: %s \n",mdbMetaData.m_mdbName.c_str());
				}
			}
            mdbIter++ ;
		}
		bool bDAGParticipantsFound = true;
		if( metadata.m_mdbMetaDataList.size() && !failed )
		{
			std::list<ExchangeMDBMetaData>::iterator exchMDBmetaListIter = metadata.m_mdbMetaDataList.begin();
			metadata.m_dagName = metadata.m_mdbMetaDataList.begin()->m_exch2k10MDBmetaData.m_dagName;			
			std::string hostName = Host::GetInstance().GetHostName();
			if( strcmpi(metadata.m_dagName.c_str(), hostName.c_str()) )
			{
				if( m_ldapHelper->getDAGParticipantsSereversList( metadata.m_dagName, metadata.m_DAGParticipantsServerList ) != SVS_OK)
				{
					DebugPrintf( SV_LOG_ERROR, "getDAGParticipantsSereversList failed.\n" );
					bDAGParticipantsFound = false;
				}
			}
			if( bDAGParticipantsFound )
			{
				while( exchMDBmetaListIter != metadata.m_mdbMetaDataList.end() )
				{
					std::list<std::string>::iterator participantsListIter = metadata.m_DAGParticipantsServerList.begin();
					while( participantsListIter != metadata.m_DAGParticipantsServerList.end() )
					{
						if( (*participantsListIter).empty() == false )
						{
							exchMDBmetaListIter->m_exch2k10MDBmetaData.m_participantsServersList.push_back(*participantsListIter);
						}
						participantsListIter++;

					}
					exchMDBmetaListIter++;
				}			
			}
		}
		bRet = SVS_OK;
		metadata.errCode = DISCOVERY_SUCCESS;
	}
    else
    {
       DebugPrintf(SV_LOG_ERROR, "ExchangeLdapHelper::getMdbs Failed \n") ;
       failed = true ;
    }
    if( !failed )
    {
        bRet = SVS_OK ;
        metadata.errCode = DISCOVERY_SUCCESS;
    }
    else
    {
        metadata.errCode = DISCOVERY_FAIL ;
        bRet = SVS_FALSE ;
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void ExchangeMetaDataDiscoveryImpl::dumpMetaData( const Exchange2k10MetaData& metadata )
{
   DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;	
   		
   DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}