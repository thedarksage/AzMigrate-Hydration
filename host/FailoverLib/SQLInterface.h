#pragma once

#define INC_OLE2
#define UNICODE
#define _UNICODE

#include <string>
#include <list>
#include <map>
#include <windows.h>
#include <stdio.h>
#include "svtypes.h"
#include "ADInterface.h"
#include <initguid.h>
#include <olectl.h>
#include <tchar.h>
#include <comutil.h>

#import "C:\Program Files\Common Files\System\ADO\msado15.dll" no_namespace rename("EOF", "EndOfFile")
#define SQLSERVERLIST "servers.txt"
#define _MAX_COL        25
#define _MAX_COL_FMT    "%-25s "

class SQLInterface
{
public:
	SQLInterface(void);
	~SQLInterface(void);

	SVERROR getSQLInstalledInstances(std::list<std::string> &hostInstances, const std::string& host,const std::string& appName);
	SVERROR getDatabases(const std::string &host, std::map<std::string, std::list<std::string> > &dbs, std::string &version);	
	SVERROR getSQLVolumes(const std::string& host, std::list<std::string> &volumes);
	bool InitSQLServer();
	bool IsDotNetInstalled();
	bool QueryRegistryValue(HKEY &hRegKey,std::string sRegValueName, DWORD &dwRegValueType, DWORD &dwSize, char** PerfData);
	SVERROR getInstFromRegistry(std::list<std::string>& hostInstances, const std::string& host, REGSAM RegKeyAccess);

    //Connects to the database of the SQL Server
	bool ConnectSqlserver(_ConnectionPtr &pInmSQLConn, const std::wstring sqlInstance, const std::wstring& dbName );

	//Executes SQL statement
    bool ExecuteSQLQuery(_ConnectionPtr &pInmSQLConn,_bstr_t query,_RecordsetPtr &pDBRecordSet);

	//Gives all the databases and their physical names of a SQL Server
	SVERROR getAllDatabasesInfo(const std::string &host, std::map<std::string, std::list<std::string> > &dbs);

	//Displays the principal and mirror roles of all databases that are configured for mirroring in a SQL Server
	void displayMirrorRoles(const std::string sqlInstance);
	

private:
	void DisplayError();


	//Stores all the SQL instance names and their corresponding database names with their corresponding PRINCIPAL / MIRROR role of a Server. 
	std::map<const std::string,std::map<std::string,std::string> > P_M_databases;
	
};