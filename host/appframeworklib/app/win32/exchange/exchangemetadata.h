#ifndef EXCHANGE_METADATA_H
#define EXCHANGE_METADATA_H
#include <boost/shared_ptr.hpp>
#include <list>
#include <string>
#include "appglobals.h"

#define EXCHANGE_EDITION_STANDARD    "Standard Edition"
#define EXCHANGE_EDITION_ENTERPRISE  "Enterprise Edition"

enum ExchangeMdbType
{
    EXCHANGE_NONE_MDB,
	EXCHANGE_PRIV_MDB,
	EXCHANGE_PUB_MDB,
	EXCHNAGE_COPY_MDB
} ;

enum MdbOnLineStatus
{
    MDB_STAT_ONLINE,
    MDB_STAT_OFFLINE
} ;
struct Exchange2k10MDBExtraMetaData
{
	bool m_isDAGParticipant;
	std::string m_guid;
	std::string m_mountInfo;
	std::string m_dagName;
	std::string m_msExchOwningServer;
	std::string m_msExchESEParamLogFilePath;
    std::string m_msExchESEParamLogFileVol ;
	std::list<std::string> m_exchangeMDBCopysList;
	std::list<std::string> m_participantsServersList;
	Exchange2k10MDBExtraMetaData()
	{
		m_isDAGParticipant = false;
		m_exchangeMDBCopysList.clear();
		m_participantsServersList.clear();
	}
};
struct ExchangeMDBMetaData
{
	ExchangeMdbType m_type ;
	std::string m_edbFilePath ;
    std::string m_streamingDB;
    std::string m_edbVol ;
    std::string m_streamingVol ;
    std::string m_distinguishedName ;
    std::string m_edbCopyFilePath ;
    std::string m_status ;
    std::string m_mdbName ;
	std::string m_defaultPublicFolderDB;
	std::string m_defaultPublicFolderDBName;
	std::string m_defaultPublicFolderDBs_StorageGRPName;
	std::string m_defaultPublicFolderDBs_MailBoxServerName;
    std::list<std::string> m_volumeList ;
	Exchange2k10MDBExtraMetaData m_exch2k10MDBmetaData;
    std::list<std::pair<std::string, std::string> > getProperties() ;
    std::string summary() ;
	std::string errString;
	long errCode;
	bool m_bRecoveryDatabase;
	ExchangeMDBMetaData()
	{
		m_bRecoveryDatabase = false;
		m_volumeList.clear();
	}
	
} ;	

struct ExchangeStorageGroupMetaData
{
	std::string m_storageGrpName ;
	std::string m_dn ;
	std::string m_msExchStandbyCopyMachines;
	std::string m_logFilePath ;
	std::string m_logVolumePath ;
	std::string m_systemPath ;
	std::string m_systemVolumePath;
	std::string m_logFilePrefix ;
    bool m_lcrEnabled ;
    std::string m_CopylogFilePath ;
    std::string m_CopysystemPath ;
	std::list<ExchangeMDBMetaData> m_mdbMetaDataList; 
    std::list<std::string> m_volumeList ;
    std::list<std::pair<std::string, std::string> > getProperties() ;

    std::string summary() ;
	std::string errString;
	long errCode;
} ;


struct ExchangeMetaData
{
    std::list<ExchangeStorageGroupMetaData> m_storageGrps ;
    std::list<std::string> m_volumeList ;
    std::string summary() ;
	void getPublicFolderDBsMailBoxServerNameList(std::list<std::string>&) ;
	std::string errString;
	long errCode;
} ;

struct Exchange2k10MetaData
{
	bool m_isDAGParticipant;
	std::string m_dagName;

	std::list<ExchangeMDBMetaData> m_mdbMetaDataList ;
	std::list<std::string> m_volumePathNameList;
	std::list<std::string> m_DAGParticipantsServerList;
    std::string summary() ;
	void getPublicFolderDBsMailBoxServerNameList(std::list<std::string>&) ;
	long errCode;
	std::string errString;
	Exchange2k10MetaData()
	{
		m_isDAGParticipant = false;
		m_mdbMetaDataList.clear();
		m_volumePathNameList.clear();
		m_DAGParticipantsServerList.clear();
	}
} ;

typedef boost::shared_ptr<ExchangeMetaData> ExchangeMetaDataPtr ;
#endif