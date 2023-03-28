//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : MovingSQLDbs.h
//
// Description: 
//
#define _USING_V110_SDK71_ //Ref TFS Task-946014
#include <string>
#include <iostream>
#include <vector>
#include <string>
#include <list>
#include <map>
#include "atlbase.h"
#include "resapi.h"
#include "version.h"

#import "C:\Program Files\Common Files\System\ADO\msado15.dll" \
no_namespace rename("EOF", "EndOfFile")

#define RESOURCE_OFFLINE_ATTEMPTS  10	
#define RESOURCE_ONLINE_ATTEMPTS 10

using namespace std;

class CopyrightNotice
{	
public:
	CopyrightNotice()
	{
		std::cout<<"\n"<<INMAGE_COPY_RIGHT<<"\n\n";
	}
};
struct sqlinfo
{
	
 wstring strDbLogicalname;
 wstring strDbpath;
 public:
	sqlinfo();
	sqlinfo(wstring dbLogicalname,wstring Dbpath)
	{
		strDbLogicalname = dbLogicalname;
        strDbpath = Dbpath;
	}
};

bool ConnectSqlserver(_ConnectionPtr &pSQLConn,const std::wstring sqlInstance,const std::wstring& dbName );
bool ExecuteSQLQuery(_ConnectionPtr &pSQLConn,_bstr_t query,_RecordsetPtr &recordsetPtr);
bool MoveDb(_ConnectionPtr &pSQLConn,_RecordsetPtr &recordsetPtr,const string &Dbname ,wstring &Dblogicalname, wstring &Dbpath, string &targetedpath);
bool OpenRegistryKey(HKEY &hKey,string sRegKeyName);
bool QueryRegistryValue(HKEY &hKey,string sRegValueName,DWORD &dwType,DWORD &dwSize,char** PerfData);
bool SetRegistryKeyValueData(HKEY &hKey,string sRegValueName,DWORD &dwType,DWORD &dwSize,const char* PerfData);
bool MoveMasterDatabase(string &SqlserverName,bool &bcluster,string &targetedpath,bool bCheckClusOrNot);
bool getInstanceServiceNamesOrClusResourceNames(string &SqlserverName,bool &bCluster);
bool StartMSSQLServices(string &SqlserverName,bool &bcluster);
bool StopMSSQLServices(string &SqlserverName,bool &bcluster);
bool stopService(const string& serviceName);
bool startService(const string& serviceName);
bool CopyDbfilesWithACLs(wstring &SrcPath, string &Tgtpath);
bool OfflineorOnlineResource(const string& resourceType, const string& appName,string m_VirtualServerName,bool bMakeOnline);
string convertoToLower(const string szlInmStringToConvert, size_t linmLength);
string convertoToUpper(const string szlInmStringToConvert, size_t lInmLength);
void trim(std::string& szlInmInput, std::string szlInmTrimCharacters = " \n\b\t\a\r\xc\\");
void ShowHelp();
vector<string> vInstServicesOrResourcenames;
bool bSql64bitEdition = false;

