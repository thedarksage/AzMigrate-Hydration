#ifndef APPDATA_H
#define APPDATA_H

#include<vector>
using namespace std;
#include "ruleengine/validator.h"
#include <map>
#include <list>
#include "mssql/mssqlapplication.h"
#include "exchange/exchangeapplication.h"
#include "fileserver/fileserverapplication.h"
#include "bes/besapplication.h"
#include <boost/lexical_cast.hpp>
#include <boost\thread.hpp>
#include <stdlib.h>
#include <string>
#include "discovery/discoveryfactory.h"
#include "util/common/util.h"
#include "portablehelpersmajor.h"

#define SEARCH_APP_HTREEITEM		0
#define SEARCH_INSTANCE_HTREEITEM	1
#define UPDATE_TREE_STEP_2			2
#define UPDATE_TREE_STEP_3			3

enum RWIZARD_CX_UPDATE_RESULT
{
	RWIZARD_CX_UPATE_SUCCESS = 0,
	RWIZARD_CX_UPATE_FAILED,
	RWIZARD_CX_UPATE_DELETED,
	RWIZARD_CX_UPATE_DUPLICATE,
	RWIZARD_CX_UPATE_TRASPORT_NO_RESULT,
	RWIZARD_CX_UPATE_TRASPORT_EXCEPTION,
	RWIZARD_CX_UPATE_UNMARSHAL_EXCEPTION,
	RWIZARD_CX_UPATE_UNKNOWN_EXCEPTION
};
typedef enum _DISCOVERY_FAILURE
{
	DISCOVERY_SUCCESSFUL,
	DISCOVERY_FAILED_EXCHANGE,
	DISCOVERY_FAILED_MSSQL,
	DISCOVERY_FAILED_FILESERVER,
	DISCOVERY_FAILED_BES,
	DISCOCVERY_FAILED
}DiscoveryError,DiscoveryResult;
bool getVolumePath(const std::string& filePath, std::string& volPath);

class AppTreeFormat
{
public:
	CString m_appName;
	vector<CString> m_instancevector;
	map<CString , vector<CString> > m_DBmap;
};
class AppData
{
public:
	~AppData(void);
	void getAppData(void);
	CString appType(int choice);

	static void DestroyInstance()
	{
		boost::mutex::scoped_lock gaurd(m_AppDataMutex);
		if (m_pAppData)
		{
			delete m_pAppData;
			m_pAppData = NULL;
		}
	}

	static AppData* GetInstance()
	{
		if (!m_pAppData){
			boost::mutex::scoped_lock gaurd(m_AppDataMutex);
			if (!m_pAppData)
				m_pAppData = new (std::nothrow) AppData();
		}
		return m_pAppData;
	}

protected:
	static boost::mutex m_AppDataMutex;
	vector<AppTreeFormat> vector_appTree;
	static AppData* m_pAppData;
	AppData(void);

public:
	MSSQLApplication m_sqlApp;
	ExchangeApplication m_pExchangeApp;
	FileServerAplication m_fileservApp;
    BESAplication m_besApp;

	std::list<ValidatorPtr> m_sysLevelRules;
	std::map<CString, std::list<ValidatorPtr> > m_appLevelRules;
	std::map<CString, std::list<ValidatorPtr> > m_versionLevelRules;
	std::map<CString, std::list<ValidatorPtr> > m_sgLevelRules; //MS Exchange Server specific
	std::map<CString, std::list<ValidatorPtr> > m_instanceRulesMap ; //MS SQL Server specific
	HostLevelDiscoveryInfo m_hldiscInfo;
	DiscoveryInfo* m_discoveyInfo;
	RWIZARD_CX_UPDATE_RESULT m_error;
	std::string m_ErrorMsg;
	int m_retryCount;

	bool m_bCreateTxtFile;
	CString m_strSummary;
	bool isVersion(CString appName);
	bool isInstance(CString instanceName);
	bool isDatabase(CString dbName,CString instanceName);
	void initAppTreeVector(void);
	vector<AppTreeFormat> getAppTreeVector()
	{
		return vector_appTree;
	}
	bool isStorageGroup(CString strSGName,CString strHostName);
	bool isMailBox(CString strMailBoxName, CString strSGName,CString strHostName);
	void ValidateRules(void);
	void GenerateSummary();
	void Create(void);
	void CreateSummaryTextFile(bool bCreate=true);
	void Clear(void);
	bool isServNTName(CString strSelItem);
	void getExistedAppList();

	bool prepareDiscoveryUpdate();
	bool fillSystemDiscoveryInfo();
	bool fillExchangeDiscoveryInfo();
	void convertToExchange2010DiscoveryInfo(const ExchAppVersionDiscInfo& exchVerDiscoveryInfo, 
                                                                const Exchange2k10MetaData& exch2010Metadata, 
                                                                ExchangeDiscoveryInfo& exchdiscoveryInfo);
	void convertToExchangeDiscoveryInfo(const ExchAppVersionDiscInfo& exchDiscoveryInfo, 
                                                                const ExchangeMetaData& exchMetadata, 
                                                                ExchangeDiscoveryInfo& exchdiscoveryInfo);
	bool fillMsSQLDiscoveryInfo();
	void fillSQLBasicData(const std::string instanceName,
		std::map<std::string,std::string>& instnceInfoMap);
	void fillSQLMetaData(const std::string& instanceName, std::map<std::string, MSSQLDBMetaDataInfo>& sqlMetaDatInfoMap);
	bool fillFileServerDiscoveryInfo();
	SVERROR fillFileServerDiscoveryData( std::string& networkName, FileServerInfo& fsInfo);
	DiscoveryError isDiscoverySuccess(std::string & errorMessage);
};
#endif