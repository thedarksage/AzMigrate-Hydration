#include "StdAfx.h"
#include "sqlinterface.h"
#include <atlbase.h>
#include "defs.h"
#include <comutil.h>
#include <atlconv.h>
#include <sstream>
#include <string>
#include <stdio.h>
#include <windows.h>
#include <atlcom.h>
#include <set>
#include "portablehelpers.h"
#include "inmsafecapis.h"

//#import "libid:35145a33-e434-49dc-92cf-e056c508525a"  named_guids
//#define  CLSID_InMageSQL "{46A951AC-C2D9-48E0-97BE-91F3C9E7B065}"
typedef  HRESULT  (__stdcall *FNPTR_INMGET_COR_SYS_DIR) ( LPWSTR pbuffer, DWORD cchBuffer, DWORD* dwlength);
typedef BOOL (WINAPI *LPFN_INMISWOW64PROCESS) (HANDLE, PBOOL);

using namespace std;
//using namespace InMageSQL;
//extern string convertoToUpper(const string szStringToConvert, size_t length);
bool SMOAvailable=false;
SQLInterface::SQLInterface(void)
{

}

SQLInterface::~SQLInterface(void)
{

}

SVERROR 
SQLInterface::getSQLInstalledInstances(list<string>& hostInstances,const string& host,const string& appName)
{
	if(_stricmp(appName.c_str(),MSSQL)==0 || _stricmp(appName.c_str(),MSSQL2005)==0 || _stricmp(appName.c_str(),MSSQL2008)==0 || _stricmp(appName.c_str(),MSSQL2012)==0)
	{
		REGSAM RegKeyAccess = KEY_ALL_ACCESS;
		
		BOOL bInmIsWow64 = FALSE;
		LPFN_INMISWOW64PROCESS fnInmIsWow64Process = (LPFN_INMISWOW64PROCESS) GetProcAddress(
		GetModuleHandle(TEXT("kernel32")),"IsWow64Process");
		cout<<endl<<"Discovering SQL Instances of local machine: "<<endl;
		if (NULL != fnInmIsWow64Process)
		{
			if (!fnInmIsWow64Process(GetCurrentProcess(),&bInmIsWow64))
			{
				cout<<"\n[Error]:"<<GetLastError()<<" Failed to determine if the host is a 32 bit or a 64 bit platform (bitness)."<<endl;
			  return SVE_FAIL;
			}
		}
		if(bInmIsWow64==TRUE)
		{
			RegKeyAccess=KEY_WOW64_64KEY|KEY_ALL_ACCESS;
			if(getInstFromRegistry(hostInstances,host,RegKeyAccess)==SVE_FAIL)
			{
				cout<<"Failed to get SQL Instance Names"<<endl;
				return SVE_FAIL;
			}
		}
		RegKeyAccess=KEY_ALL_ACCESS;
		if(getInstFromRegistry(hostInstances,host,RegKeyAccess)==SVE_FAIL)
		{
			cout<<"Failed to get SQL Instance Names"<<endl;
			return SVE_FAIL;
		}
		if(hostInstances.empty())
		{
			cout<<"Couldn't find SQL Server with the name "<<host<<endl;
			return SVE_FAIL;
		}
		return SVS_OK;
	}//end of if block
	
	return SVE_FAIL;
}

SVERROR
SQLInterface::getDatabases(const string &host, map<string, list<string> > &dbs, string& serverVersion)
{
	USES_CONVERSION;
	SVERROR result = SVS_OK;
	//The following commented code is for Principal and Mirror databases discovery support.
	/*if(strcmpi(serverVersion.c_str(),MSSQL2005)== 0||strcmpi(serverVersion.c_str(),MSSQL2008)== 0){
		result=getAllDatabasesInfo(host,dbs);
		return result;
	}*/
	
	if(SMOAvailable)
	 {
		//try{ 
		//    HRESULT hr = CoInitialize(NULL);
		//	IDiscoverPtr pDiscoversql;
  //          HRESULT hRes = pDiscoversql.CreateInstance(__uuidof(SqlClass));
		//	list<string> dbConfig; 
		//	string dbnames=pDiscoversql->getDB(host.c_str());	
		//	if(dbnames.empty())
		//	{
		//		cout<<">>>>> Failed to get database names of the SQL Server: "<<host<<endl;
		//		return SVE_FAIL;
		//	}
		//	cout<<"\n Connecting to "<<host.c_str()<<"\n";
		//	string DataBase=dbnames.c_str();
		//	string DBtoken;
		//	istringstream iss1(DataBase);
		//	while(getline(iss1,DBtoken,';'))
		//	{
		//	   string paths=pDiscoversql->getDatabaseInfo(host.c_str(),DBtoken.c_str());
		//	   if(paths.empty())
		//	   {
		//		   cout<<">>>>> Failed to get database information of the database: "<<DBtoken<<endl;
		//		   cout<<"Either the user doesnot have permissions or the database is not accessible."<<endl;
		//		   return SVE_FAIL;
		//	   }
		//	   string DBInfo=paths.c_str();
		//	   string DBinfotoken;
		//	   istringstream iss2(DBInfo);
		//	   while(getline(iss2,DBinfotoken,';'))
		//	   {
		//		  dbConfig.push_back(DBinfotoken);
		//	   }

		//	   dbs[DBtoken]=dbConfig; // Now filling the Map with Databases and ther Corresponding mdf and ldf.
		//	   dbConfig.clear();      //clears the list that contains the Paths info otherwise all paths wil come
		//	}
		//  }
		//  catch(...)
		//  {
		//	cout<<"ERROR:Failed in Retrieving SQL DataBasesInformation"<<endl;
		//	return SVE_FAIL;
	 //     }
	 }
	return result;
}

SVERROR
SQLInterface::getSQLVolumes(const string& host, list<string> &volumes)
{
	SVERROR result = SVS_OK;
	return result;
}
bool SQLInterface::IsDotNetInstalled()
{
	HINSTANCE hDotNetDLL = LoadLibrary(TEXT("mscoree.dll"));
	if(NULL!=hDotNetDLL)
	{ 
		return true;
	}
	else 
		return false;	
}
bool
SQLInterface::InitSQLServer ()
{
	//HRESULT hResult;
	//if (FAILED(hResult = CoInitialize (NULL))) {
	//	cout << "Failed to Initialize COM\n";
	//	return FALSE;
	//}

	//IDiscoverPtr pDiscoversql;
 //   USES_CONVERSION;
	////Register InMageSQL DLL
	//ADInterface ad;	
	//string installLocation=" \""+ad.getAgentInstallPath()+"\\InMageSQL.dll\" /codebase /tlb /silent";
	//bool bIsDotNetFrameWorkInstalled =SQLInterface::IsDotNetInstalled();
	//if(bIsDotNetFrameWorkInstalled)
	//{	
	//	HINSTANCE hDotNetDLL = LoadLibrary(TEXT("mscoree.dll"));
	//	FNPTR_INMGET_COR_SYS_DIR   GetCORSystemDirectory = NULL;
	//	GetCORSystemDirectory = (FNPTR_INMGET_COR_SYS_DIR) GetProcAddress (hDotNetDLL,"GetCORSystemDirectory"); 
	//			
	//	if(GetCORSystemDirectory!=NULL)
	//	{					
	//		WCHAR appNameBuf[MAX_PATH + 1];
	//		DWORD length; 
	//		HRESULT hr = GetCORSystemDirectory(appNameBuf,MAX_PATH,&length);
	//		inm_wcscat_s( appNameBuf,ARRAYSIZE(appNameBuf), L"RegAsm.exe" );
	//		STARTUPINFO lInmStartupInfo;
	//		PROCESS_INFORMATION lInmProcessInfo;
	//		ZeroMemory( &lInmStartupInfo, sizeof(lInmStartupInfo) );
	//		lInmStartupInfo.cb = sizeof(lInmStartupInfo);
	//		ZeroMemory( &lInmProcessInfo, sizeof(lInmProcessInfo) );	
	//		if(!CreateProcessA(W2A(appNameBuf),(LPSTR)installLocation.c_str(),NULL, NULL,FALSE, 0,NULL,NULL,(STARTUPINFOA*)&lInmStartupInfo,&lInmProcessInfo ) )
	//		{
	//			cout<<"Registering InMageSQL Process failed "<<GetLastError()<<endl;
	//		}
	//				
	//		WaitForSingleObject( lInmProcessInfo.hProcess, INFINITE );
	//		CloseHandle( lInmProcessInfo.hThread ); 
	//		CloseHandle( lInmProcessInfo.hProcess );
	//	}	
	//}
	//else
	//{
	//	cout<<".NET FrameWork Not Installed So the Discovery  "<<endl;
	//	return false;
	//}

	//HRESULT hRes=CoCreateInstance(InMageSQL::CLSID_SqlClass, NULL, CLSCTX_INPROC_SERVER,InMageSQL::IID_IDiscover,(LPVOID*)&pDiscoversql);
	//if(FAILED(hRes))
	//{		
	//	cout<<" CoCreate Instance of SQLSMO is Also failed."<<endl;
	//	return FALSE;
	//}
	//else
	//{
	//	SMOAvailable=true;
	//}  
	return TRUE;
}


// **********************************************************************
// display error information
// **********************************************************************
void
SQLInterface::DisplayError()
{
	LPERRORINFO	pErrorInfo = NULL;
	BSTR	strDescription, strSource;

	GetErrorInfo(0,&pErrorInfo);

	pErrorInfo->GetDescription(&strDescription);
	pErrorInfo->GetSource(&strSource);

	_tprintf(TEXT(" ERROR: %s\n"),strSource);
	_tprintf(TEXT(" %s\n"),strDescription);

	pErrorInfo->Release();
	
	SysFreeString(strDescription);
	SysFreeString(strSource);
	
	return;
}

bool SQLInterface:: QueryRegistryValue(HKEY &hRegKey,string sRegValueName,DWORD &dwRegValueType,DWORD &dwSize,char** lpData)
{
	DWORD lstatus=0;
	lstatus=RegQueryValueExA(hRegKey, sRegValueName.c_str(), NULL, &dwRegValueType, NULL, &dwSize);
	if(lstatus==ERROR_SUCCESS)
	{
		*lpData = (char*) malloc(dwSize);
		if (*lpData == NULL)
		 {
			cout<<"\nError:Unable to allocate a memory of size "<<dwSize<<"on the Heap!\n";
			return false;
		 }
		 if(RegQueryValueExA(hRegKey,sRegValueName.c_str(), NULL, &dwRegValueType, (LPBYTE)*lpData,&dwSize) == ERROR_SUCCESS)
		 {
			return true;
		 }
	}
	else
	{
		if(lstatus==ERROR_FILE_NOT_FOUND)
		{
			return false;
		}
		cout << "\nError reading key.\nERROR:"<<lstatus<<endl;
		return false;
	}
	return true;
}

SVERROR SQLInterface::getInstFromRegistry(list<string>& hostInstances,const string& host,REGSAM RegKeyAccess)
{
	SVERROR result = SVS_OK;
	HKEY hRegKey;
	DWORD dwRegValueType=REG_MULTI_SZ;
	DWORD dwSize;
	DWORD lstatus=0;
	char* lpData;
	string subKey;
	vector<string>ALLInstanceNames;
	char hostName[512];
	int ret = 0;
	ret = gethostname(hostName,sizeof(hostName));
	if ( ret != 0)
	{
		cout << "\n[ERROR] Could not get the host name of this machine. Error Code = " << ret << endl;
		return SVE_FAIL;
	}
	//Open registry key
	lstatus=RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SOFTWARE\\Microsoft\\Microsoft SQL Server",0,RegKeyAccess,&hRegKey);
	if(lstatus!=ERROR_SUCCESS)
	{
		if(lstatus==ERROR_FILE_NOT_FOUND)
		{
			return SVS_OK;
		}
		cout<<"Failed to open registry key HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Microsoft SQL Server. [Error]:"<<lstatus<<endl;
		RegCloseKey(hRegKey);
		return SVE_FAIL;
	}
	//Query for registry value
	if(QueryRegistryValue(hRegKey,"InstalledInstances",dwRegValueType,dwSize,&lpData)==true)
	{
		string str,str1 ;
		for(size_t i=0;i<dwSize;i++)
		{
			if(lpData[i] !=  '\0')
			{
				str += lpData[i];
			}
			else
			{
				str1 = str;
				if(!str1.empty())
				{
					ALLInstanceNames.push_back(str1);
				}
				cout<<"  "<<str1;
				str.clear();
			}
		}
		cout<<endl<<endl;
		free(lpData);
	}
	RegCloseKey(hRegKey);	
	
	dwRegValueType = REG_SZ;
	string InstanceName;
	HKEY ClusKey;
	vector<string>::iterator ALLInstancesIter = ALLInstanceNames.begin();
	bool isSQL2000Instance;
	string intermediateInstanceName="";
	string version="";

	for(;ALLInstancesIter != ALLInstanceNames.end();ALLInstancesIter++)
	{
		isSQL2000Instance=true;
		lstatus=RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SOFTWARE\\Microsoft\\Microsoft SQL Server\\Instance Names\\SQL",0,RegKeyAccess,&hRegKey);
		if(lstatus==ERROR_SUCCESS)
		{
			if(QueryRegistryValue(hRegKey,(*ALLInstancesIter),dwRegValueType,dwSize,&lpData)==true)
			{
				intermediateInstanceName=string(lpData);
				subKey ="SOFTWARE\\Microsoft\\Microsoft SQL Server\\"+intermediateInstanceName+"\\Setup";
				isSQL2000Instance=false;
				free(lpData);
			}		
		}
		RegCloseKey(hRegKey);
		if(isSQL2000Instance)
		{
			if((*ALLInstancesIter)=="MSSQLSERVER")
			{
				subKey="SOFTWARE\\Microsoft\\MSSQLSERVER\\Setup";
			}
			else
			{
				subKey="SOFTWARE\\Microsoft\\Microsoft SQL Server\\"+(*ALLInstancesIter)+"\\Setup";
			}				

		}
		//Determining the version of sqlserver
		if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,subKey.c_str(),0,RegKeyAccess,&hRegKey)==ERROR_SUCCESS)
		{
			if(QueryRegistryValue(hRegKey,"Patchlevel",dwRegValueType,dwSize,&lpData)==true)
			{
				version=strtok(lpData,".");					
				if( version.compare("10")==0 )
					version=MSSQL2008;
				else if(version.compare("9")==0 )
					version=MSSQL2005;					
				else if( version.compare("8")==0)
					version=MSSQL;
				free(lpData);
			}		
			RegCloseKey(hRegKey);
		}
		else
		{
			cout<<"\nFailed to determine the version of SQL Instance: "<<(*ALLInstancesIter)<<endl;
			result=SVE_FAIL;
			break;
		}

			if(version==MSSQL2008 || version==MSSQL2005)
			{
				subKey ="SOFTWARE\\Microsoft\\Microsoft SQL Server\\"+intermediateInstanceName+"\\Cluster";
			}
			else
			{
				if((*ALLInstancesIter)=="MSSQLSERVER")
				{
					subKey="SOFTWARE\\Microsoft\\MSSQLSERVER\\Cluster";
				}
				else
				{
					subKey="SOFTWARE\\Microsoft\\Microsoft SQL Server\\"+(*ALLInstancesIter)+"\\Cluster";
				}
			}	 
			//Checking for given Sqlserver is cluster virtual server or not
			if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,subKey.c_str(),0,RegKeyAccess,&ClusKey)!=ERROR_SUCCESS)
			{
				if(_stricmp(host.c_str(),hostName)!=0)
				{
					cout<<"SQL Instance "<<host.c_str()<<" not found"<<endl;
					result= SVE_FAIL;
					break;
				}
				if((*ALLInstancesIter)=="MSSQLSERVER")
				{
					InstanceName=host;
				}
				else
				{
					InstanceName=host+"\\"+(*ALLInstancesIter);
				}
				hostInstances.push_back(InstanceName);
				cout<<"SQL Server Name:"<<InstanceName<<endl;
			}
			else
			{
				if(QueryRegistryValue(ClusKey,"ClusterName",dwRegValueType,dwSize,&lpData)==true)
				{
					string temp = string(lpData);
					if((*ALLInstancesIter)=="MSSQLSERVER")
					{
						InstanceName=temp;
					}
					else
					{
						InstanceName=temp+"\\"+(*ALLInstancesIter);
					}
					if((_stricmp(host.c_str(),temp.c_str())==0)^(_stricmp(host.c_str(),hostName)==0))
					{
						hostInstances.push_back(InstanceName);
					}
					cout<<"Virtual Server Name : "<<InstanceName<<endl;
					free(lpData);
				}
			}//end of else
			RegCloseKey(ClusKey);
		
	}//end of for loop
	return result;
}

SVERROR SQLInterface::getAllDatabasesInfo(const string &host, map<string, list<string> > &dbs)
{
	USES_CONVERSION;
	wstring wInmSQLServerName;
	bool bResult = true;
	list <string> dbFiles;
	_ConnectionPtr pInmSQLConn = NULL;
	string sqlServerName= ToLower( host);
	
	try
	{
		wInmSQLServerName = A2W(sqlServerName.c_str());
		//Initialize the COM Library
		CoInitialize(NULL);

		//ConnectSqlserver(pConn,L"localhost",L"master");
		if(ConnectSqlserver (pInmSQLConn,wInmSQLServerName,L"master")==false)
		{
			wcout<<"Couldn't connect to SQL Server: "<<wInmSQLServerName<<endl;
			CoUninitialize();
			return SVE_FAIL;
		}
		_RecordsetPtr pDBRecordSet ;
		pDBRecordSet.CreateInstance (__uuidof (Recordset)); 
		_bstr_t query ;
		//SQL Query to get the list of databases and their Physical names.
		query = "select db_name(database_id) as dbname,physical_name from sys.master_files";
	    
		bResult = ExecuteSQLQuery(pInmSQLConn,query,pDBRecordSet);
		if(!bResult)
		{
			cout<<"\nFailed while executing the query : "<<query<<endl;
			goto cleanup;
		}
		else
		{
			_variant_t vDBPath, vDBName;
			set<string> dbNames;
			string sDBName, sDBPath, sPrevDBName;
			if( pDBRecordSet != NULL )
			{
				while (!pDBRecordSet->GetEndOfFile())
				{
					sDBName=pDBRecordSet->GetCollect(L"dbname");
					vDBPath = pDBRecordSet->GetCollect(L"physical_name");
					sDBName=_bstr_t(vDBName.bstrVal);
					sDBPath=_bstr_t(vDBPath.bstrVal);
					if(dbNames.empty())
					{
						dbNames.insert(sDBName);
					}
					if(dbNames.find(sDBName)==dbNames.end())
					{
						dbNames.insert(sDBName);
						dbs[sPrevDBName]=dbFiles;
						dbFiles.clear();
					}
					dbFiles.push_back(sDBPath);
					sPrevDBName=sDBName;
					pDBRecordSet->MoveNext();
				}
				dbs[sPrevDBName]=dbFiles;
				dbFiles.clear();
				//Closing Recordset
				pDBRecordSet->Close() ;
			}
			//SQL Query to get the list of principal and mirror databases.
			query="select db_name(database_id) as dbname,mirroring_role_desc from sys.database_mirroring where mirroring_guid is not null";
			bResult = ExecuteSQLQuery(pInmSQLConn,query,pDBRecordSet);
			if(!bResult)
			{
				cout<<"\nFailed while executing the query : "<<query<<endl;
				goto cleanup;
			}
			if( pDBRecordSet != NULL )
			{
				_variant_t mirror_role;
				map<string,string> dbroles;
				string smirror_role;
				while (!pDBRecordSet->GetEndOfFile())
				{
					sDBName=pDBRecordSet->GetCollect(L"dbname");
					mirror_role=pDBRecordSet->GetCollect(L"mirroring_role_desc");
					sDBName=_bstr_t(vDBName.bstrVal);					
					smirror_role=_bstr_t(mirror_role.bstrVal);					
					if(strcmpi(smirror_role.c_str(),"PRINCIPAL")==0)
					{
						dbroles[sDBName]="PRINCIPAL";
					}
					else
					{
						dbroles[sDBName]="MIRROR";
					}
					pDBRecordSet->MoveNext();
				}
				if(!dbroles.empty())
				P_M_databases[sqlServerName]=dbroles;
				pDBRecordSet->Close() ;
			}

		}
	}
	catch(...)
	{
		cout<<"Failed to retrieve Databases Information of "<<host<<". [ERROR]: "<<GetLastError()<<endl;
		bResult=false;
	}
  cleanup:
	CoUninitialize();	
	pInmSQLConn->Close();
	if(bResult)
	{
		return SVS_OK;
	}
	else
	{
		return SVE_FAIL;
	}
}

bool SQLInterface::ConnectSqlserver(_ConnectionPtr &pInmSQLConn, const std::wstring sqlInstance, const std::wstring& dbName )
{
	_bstr_t strSQLCon = "Driver={SQL Server};" ; //ODBC Driver
    strSQLCon += "Server=" ;
    strSQLCon += sqlInstance.c_str();
    strSQLCon += ";" ;
    strSQLCon += "Database=" ;
	strSQLCon += dbName.c_str();
    strSQLCon += ";" ;
	strSQLCon += "Trusted_Connection=yes" ; //specifies Windows authentication

	HRESULT hr;
	pInmSQLConn = NULL;
	try
	{
		//Creating a connection object
		hr = pInmSQLConn.CreateInstance((__uuidof(Connection)));
		if( FAILED(hr) )
		{
			cout<<"Connection::CreateInstance Failed.[ERROR]:"<<hr<<endl;
			return false;
		}
		else
		{
			//Opening connection to the specified database
			hr = pInmSQLConn->Open(strSQLCon, "", "", adConnectUnspecified) ;
			if( FAILED(hr) )
			{
				wcout<<"Failed to connect SQL Server: "<<sqlInstance<<".[ERROR]:"<<hr<<endl;
				return false;
			}
			wcout<<endl<<"Connected to SQL Server: "<<sqlInstance<<endl ;
			return true;
		}
	}
	catch(_com_error ce)
	{
		cout<<"[ERROR]:"<<ce.Description()<<endl;
		wcout<<"Error while connecting to SqlServer: "<<sqlInstance<<endl;
		return false;
	}
	catch(...)
	{
		cout<<"[ERROR]:"<<GetLastError();
		wcout<<"  Exception occurred when connecting to SqlServer: "<<sqlInstance<<endl;
		return false;
	}
}

bool SQLInterface::ExecuteSQLQuery(_ConnectionPtr &pInmSQLConn,_bstr_t query,_RecordsetPtr &pDBRecordSet)
{
	try
  {
		pDBRecordSet = NULL ;
		//Execute SQL Query
		pDBRecordSet = pInmSQLConn->Execute(query,NULL,adOptionUnspecified);
		return true;
  }
  catch(_com_error ce)
  {
		cout<<"[ERROR]:"<<ce.Description()<<endl;
		cout<<"Failed to execute SQL query"<<endl;
		return false;
	}
}

void SQLInterface:: displayMirrorRoles(const std::string sqlInstance)
{
	if((P_M_databases.find(sqlInstance))!=P_M_databases.end())
	{
		cout<<"List of principal and mirror databases of "<<sqlInstance<<" are: \n";
		map<string,string>::iterator P_M_databasesIter=P_M_databases[sqlInstance].begin();
		for(;P_M_databasesIter!=P_M_databases[sqlInstance].end();P_M_databasesIter++)
		{
			cout<<P_M_databasesIter->first<<":["<<P_M_databasesIter->second<<"]   ";
		}
		cout<<endl;
	}
/*	else
		cout<<"No Principal and Mirror databases found for "<<sqlInstance<<endl;*/
}

