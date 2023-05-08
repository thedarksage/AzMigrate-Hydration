//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : MovingSQLDbs.cpp
//
// Description: Used to move MsSql datbases from one location to another location
//

#include <windows.h>
#include "MovingSQLDBs.h"
#include "shlobj.h"
#include "clusapi.h"
#include "atlcom.h"
#include <string.h>
#include <winsvc.h>
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "win32/terminateonheapcorruption.h"


using namespace std;

//printing copyright notice
CopyrightNotice displayCopyrightNotice;

int _tmain(int argc, char* argv[])
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
	int iParsedOptions = 1;
	string sqlservername;
	string targetpath;
	string SpecificDbname;
	bool bmovealldbs = false;
	bool bMoveSpecificDb = false;
	USES_CONVERSION;
	wstring WSqlServerName;
	wstring WSpecificDbname;
	bool bCluster = false;

   
	for (;iParsedOptions < argc; iParsedOptions++)
	{
		if(argv[iParsedOptions][0] == '-')
		{
			if(_strcmpi(argv[iParsedOptions],"-server") == 0)
			{   
				iParsedOptions++;
                if((iParsedOptions>=argc)||(argv[iParsedOptions][0] == '-'))  
                {
                    cout << "\nMissing parameter for server \n\n";
					ShowHelp();
					return 0;
                }

				sqlservername = argv[iParsedOptions];
				WSqlServerName = A2W(sqlservername.c_str());
			}
	        
			else if(_strcmpi(argv[iParsedOptions],"-newpath") == 0)
			{
				iParsedOptions++;
				
		        if((iParsedOptions>=argc)||(argv[iParsedOptions][0] == '-'))
				{
                    cout << "\nMissing parameter for newpath \n\n";
					ShowHelp();
					return 0;
                }
	    		targetpath = argv[iParsedOptions];
			}
			else if(_strcmpi(argv[iParsedOptions],"-db") == 0)
			{
				iParsedOptions++;
	            if((iParsedOptions>=argc)||(argv[iParsedOptions][0] == '-'))
                {
                    cout << "\nMissing parameter for db \n\n";
					ShowHelp();
					return 0;
                }
				SpecificDbname = argv[iParsedOptions];
				WSpecificDbname = A2W(SpecificDbname.c_str());
				bMoveSpecificDb = true;

			}
			else if(_strcmpi(argv[iParsedOptions],"-moveall") == 0)
			{
				bmovealldbs = true;
			}
			else if(_strcmpi(argv[iParsedOptions],"-h") == 0)
			{
				ShowHelp();
				return 0;
			}
			else 
			{
				ShowHelp();
				return 0;	
			}
		}   
	}
	if(argc<6 || argc>7)
	{
		ShowHelp();
		return 0;
	}
	if((bMoveSpecificDb==true)&&(bmovealldbs == true))
	{
		cout <<"\nEnter any one option -db (or) -moveall";
		ShowHelp();
		return 0;
	}
	if((bMoveSpecificDb==false)&&(bmovealldbs == false))
	{
		ShowHelp();
		return 0;
	}
	if(targetpath.empty())
	{
		cout<<"\nPlease mention the newpath to move the databases"<<endl;
		ShowHelp();
		return 0;
	}
	if(_stricmp(SpecificDbname.c_str(),";")==0)
	{
		cout<<"\nPlease mention the Database names to move"<<endl;
		ShowHelp();
		return 0;
	}

	size_t found = targetpath.find("\"");
	if (found!=string::npos)
	{
		cout<<"\nPlease make sure the given new path is not end with '\\' character"<<endl;
		ShowHelp();
		return false;
	}

	list<std::wstring> dbNames;
	_ConnectionPtr pSQLConn = NULL;
	_CommandPtr pSQLCmd = NULL;

	HRESULT hRes = S_OK;
	//Initialize the COM Library
	CoInitialize(NULL);
	
	if(ConnectSqlserver(pSQLConn,WSqlServerName,L"master")==false)
	{
		cout<<"Connection failed"<<endl;
		return 0;
	}
	
	_RecordsetPtr pDBRecordSet ;
	 pDBRecordSet.CreateInstance (__uuidof (Recordset)); 

 
 	if((SpecificDbname!="" )&& !bmovealldbs)
	{
		wstring WDbsToMove;
		if(SpecificDbname.find_first_of(';') != std::string::npos)
		{
		  char szBuffer[512];
		  char *next_token;
		  strcpy_s(szBuffer, SpecificDbname.c_str()) ;
		  char* pszToken = strtok_s(szBuffer,";",&next_token);
		  for(  ;(NULL != pszToken);(pszToken = strtok_s( NULL,";",&next_token)))
			{
			  WDbsToMove = A2W(pszToken);   
			  dbNames.push_back(WDbsToMove);
			}
		}
		else
		{
			dbNames.push_back(WSpecificDbname);
		}
	}
	else
	{
		_bstr_t query ;
		query = "use master SELECT name FROM sysdatabases";
	
		if(ExecuteSQLQuery(pSQLConn,query,pDBRecordSet))
		{
			_variant_t dbNameVar; 
			if(pDBRecordSet != NULL)
			{
				while (!pDBRecordSet->GetEndOfFile())
				{
						dbNameVar = pDBRecordSet->GetCollect(L"name");						  				
						dbNames.push_back(dbNameVar.bstrVal) ;
						pDBRecordSet->MoveNext();
						dbNameVar.Clear(); 
				}
				pDBRecordSet->Close() ;
			}
		}
		else
		{
			cout<<"Error while executing the Query"<<query<<endl;
			return false;
		}
	}
	cout<<"\nDiscovering the original path of DataBases\n"<<endl;
	//to store all dbs actual names with pair of  logical names and the physical paths 
	//multimap< std::wstring,pair<wstring,wstring> > dbLogicalnamesPaths;
	bool bresult = true;
	multimap<string,sqlinfo> Dbinfo;
	
	//one more multimap with map inside dbname as key and the value as a map(dblogicalname ,dbpath)
	multimap<string,std::pair<wstring,wstring>>Dbconfiguration;


	std::list<std::wstring>::const_iterator dbNameIter  = dbNames.begin() ;
	while( dbNameIter != dbNames.end() )
	{
		string DbNameToquery = W2A((*dbNameIter).c_str());
	    cout<<"dbname is :"<< DbNameToquery.c_str()<<endl;
		
		_bstr_t query ;
		//query = "use ";
		//query += (*dbNameIter).c_str();
		//query += " select distinct name,filename from sys.sysfiles" ;

		query = "use ";
		query += "\"" ;
		query +=(*dbNameIter).c_str();
		query += "\"";
		query += " select distinct name,filename from sysfiles";
	    
	    bresult = ExecuteSQLQuery(pSQLConn,query,pDBRecordSet);
		if(!bresult)
		{
			cout<<"\nFailed while executing the query : "<< query <<endl;
			return false;
		}
		else
		{
			_variant_t dbName,dbPath;

			if( pDBRecordSet != NULL )
			{
				while (!pDBRecordSet->GetEndOfFile())
				 {
					dbName = pDBRecordSet->GetCollect(L"name");
					dbPath = pDBRecordSet->GetCollect(L"filename");
		        
					cout <<"DB Logical Name :"<< static_cast<char *>(_bstr_t(dbName.bstrVal))<< std::endl;
					cout <<"DB Logical Path :"<< static_cast<char *>(_bstr_t(dbPath.bstrVal))<< std::endl;
			
					sqlinfo objDbNamesPaths(dbName.bstrVal,dbPath.bstrVal);
		    		Dbinfo.insert(make_pair(DbNameToquery,objDbNamesPaths));
					Dbconfiguration.insert(make_pair(DbNameToquery, make_pair(dbName.bstrVal, dbPath.bstrVal)));
					pDBRecordSet->MoveNext();
				  }
				  pDBRecordSet->Close() ;
			  }
		 }

	 dbNameIter++;
	}

	//creating the target directory if not exsisted
	//removing trailing spaces before creating or checking directory existence

	trim(targetpath);
	
	int Path = SHCreateDirectoryEx(NULL,targetpath.c_str(),NULL) ;
		
	if(Path	== ERROR_SUCCESS )
	{
		cout<< "Directory : " << targetpath.c_str()<< " successfully created "<<endl;
	}
	else if((Path == ERROR_FILE_EXISTS)||(Path == ERROR_ALREADY_EXISTS))
	{
	    cout<< "Directory : " << targetpath.c_str()<< " already existed"<<endl;
	}
	else if(Path == ERROR_BAD_PATHNAME)
	{
		cout<< "Directory : " << targetpath.c_str()<<" was set to a relative path"<<endl;
		return false;
	}
	else if(Path == ERROR_PATH_NOT_FOUND)
	{
		cout<< "Directory : " << targetpath.c_str()<< " The path may contain an invalid entry"<<endl;
		return false;
	}
	else if(Path == ERROR_FILENAME_EXCED_RANGE)
	{
		cout<< "Directory : " << targetpath.c_str()<< " The path is too long "<<endl;
		return false;
	}

	//checking for cluster or not
	std::list<std::string>PropertyNames;
	std::map<string,string>PropertywithValues;

	_bstr_t query ;
	query = "SELECT " ;
	query += "CONVERT(nvarchar(256), SERVERPROPERTY('IsClustered')) AS 'Is Clustered',";
	query += "CONVERT(nvarchar(256), SERVERPROPERTY('InstanceName')) AS 'Instance Name', ";
	query += "CONVERT(nvarchar(256), SERVERPROPERTY('edition')) AS 'SQLEdition', " ;
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('ServerName')) AS 'Server Name' " ;

	PropertyNames.push_back("Is Clustered") ;
	PropertyNames.push_back("Instance Name");
	PropertyNames.push_back("SQLEdition");
	PropertyNames.push_back("Server Name");
	string value;
  
    std::list<std::string>::const_iterator PropertyNamesIter = PropertyNames.begin();
	try
	{
		if(ExecuteSQLQuery(pSQLConn,query,pDBRecordSet)==true)
		{
			if( pDBRecordSet != NULL )
            {
				 while( PropertyNamesIter != PropertyNames.end())
					{
						if( (pDBRecordSet->GetCollect((*PropertyNamesIter).c_str())).bstrVal != NULL )
						{
							value = W2A((pDBRecordSet->GetCollect((*PropertyNamesIter).c_str())).bstrVal);
							if( value.compare("NULL") != 0  && value.length() != 0 )
							{
								PropertywithValues.insert(make_pair(*PropertyNamesIter,value));
							}
							else
							{
								cout<< "The value for server property "<<PropertyNamesIter->c_str()<<" is NULL\n";
							}
						}

						PropertyNamesIter++;
					}
				pDBRecordSet->Close() ;
			}
		}
	}
	catch(_com_error ce)
    {
        cout<<"Error while executing SQL Propeties Query\n";
		wcout<<ce.Description().GetBSTR()<<endl;
		return false;
    }
    if(PropertywithValues["Is Clustered"]== "1")
	{
		bCluster = true;
		cout<<"Sqlserver is part of MSCS cluster\n";
	}
	else if(PropertywithValues["Is Clustered"]== "0")
	{
		cout<<"Sqlserver is not part of MSCS cluster\n";
	}
     
	string SQLServerEdition;
    SQLServerEdition = PropertywithValues["SQLEdition"];
	size_t editionbit = SQLServerEdition.find("64");
	if(editionbit!= string::npos)
	{
		bSql64bitEdition = true;
	}
	else
	{
		bSql64bitEdition = false;
	}
	
	getInstanceServiceNamesOrClusResourceNames(sqlservername,bCluster);

	wstring Dblogicalname;
	wstring Dbpath;
	multimap<std::string,sqlinfo >::iterator DbsIter = Dbinfo.begin();  

	//its temporary fix we are getting master dbname twise in the loop(for .mdf and .ldf)
	//but in master case we are moving both files at a time so we need to skip execution 
	//of moving master for the second time
	int masterCount = 0;

	for(;DbsIter != Dbinfo.end(); DbsIter++)
	{ 
		string OriginalDbName = (*DbsIter).first;
		//After move db file we are restarting the sqlserver services so we are loosing sql connection.
		//we are creating a connection again in next iteration of this for loop
		if(pSQLConn == NULL)
		{
			//To move Master database we dont need connection here
			if(_stricmp(OriginalDbName.c_str(),"master")!=0)
			{
				if(ConnectSqlserver(pSQLConn,WSqlServerName,L"master")==false)
    			{
	    			cout<<"Connection failed"<<endl;
					return false;
				}
			}
		}

		if(_stricmp(OriginalDbName.c_str(),"master")!=0)
		{
			string Dbname = (*DbsIter).first;
			Dblogicalname = (*DbsIter).second.strDbLogicalname; 
			Dbpath = (*DbsIter).second.strDbpath;

			//Need to takecare of mssqlresource database (It will not be shown in the discovery)
			if(!MoveDb(pSQLConn,pDBRecordSet,Dbname ,Dblogicalname, Dbpath, targetpath))
			{
			    cout<<"Failed while moving the database"<<endl;
			    return false;
			}
			else
			{
			  //closing the connection before stopping the services
				pSQLConn->Close();
				if(StopMSSQLServices(sqlservername,bCluster)==false)
				{
					cout<<"Failed to stop sqlservices"<<endl;
					return false;
				}

				if(!CopyDbfilesWithACLs(Dbpath,targetpath))
				{
					wcout<<"Failed to copy database"<<Dbpath.c_str()<<endl;
					return false;
				}

				if(StartMSSQLServices(sqlservername,bCluster)==false)
				{
					cout<<"Failed to start the mssql services"<<endl;
					return false;
				}
			 }
		}
		else if(masterCount == 0)
		{
			pair< multimap< string, pair< wstring, wstring > >::iterator, multimap< string, pair< wstring, wstring > >::iterator > DbconfigInfoPairs =  
			Dbconfiguration.equal_range("master");
			multimap< string, pair< wstring, wstring > >::iterator srcIter = DbconfigInfoPairs.first;
			multimap< string, pair< wstring, wstring > >::iterator endIter = DbconfigInfoPairs.second;
			
			if(!MoveMasterDatabase(sqlservername,bCluster,targetpath,false))
			{
				cout<<"Failed to move the master database"<<endl;
				return false;
			}
			else
			{   
				//closing the connection before stopping the services
				if(pSQLConn!=NULL)
				{
					pSQLConn->Close();
				}
				if(StopMSSQLServices(sqlservername,bCluster)==false)
				{
					cout<<"Failed to stop sqlservices"<<endl;
					return false;
				}
				for (; srcIter != endIter; srcIter++)
				{	
					wstring Dbfilename = (*srcIter).second.second;
					if(!CopyDbfilesWithACLs(Dbfilename,targetpath))
					{
						wcout<<"Failed to copy database"<<Dbfilename.c_str()<<endl;
						return false;
					}
				}
				if(StartMSSQLServices(sqlservername,bCluster)==false)
				{
					cout<<"Failed to start the mssql services"<<endl;
					return false;
				}
				cout<<"****************************************************"<<endl;
				cout<<"Successfully moved : Master Database to the target path "<<targetpath.c_str()<< endl;
				cout<<"****************************************************"<<endl;
				masterCount++;
			}
		}
	
		//making pcon to NULL before creating connection again
		pSQLConn = NULL;
	}//end of for llop

	cout<<"\n*************Successfully Moved the databases****************"<<endl;
  
    //clenup the required things
	CoUninitialize();
	if( pSQLConn != NULL && pSQLConn->State != adStateClosed)
    {
        pSQLConn->Close();
		pSQLConn = NULL;
		cout<<"Connection closed successfully"<<endl;
    }

  return 0;
}

bool ConnectSqlserver(_ConnectionPtr &pSQLConn,const std::wstring sqlInstance,const std::wstring& dbName )
{  
	bool bRet = true;
	_bstr_t strCon = "Driver={SQL Server};" ;
        strCon += "Server=" ;
        strCon += sqlInstance.c_str();
        strCon += ";" ;
        strCon += "Database=" ;
	    strCon += dbName.c_str();
        strCon += ";" ;
        strCon += "Trusted_Connection=yes" ;

    HRESULT hr ;
    pSQLConn = NULL ;
    try
    {
        hr = pSQLConn.CreateInstance((__uuidof(Connection)));
        if( FAILED(hr) )
        {           
			cout<<"Connection::CreateInstance Failed\n"<<endl;
			bRet = false;
        }
        else
        {
			pSQLConn->ConnectionTimeout = 30 ;
            hr = pSQLConn->Open(strCon,"","",0) ;
            if( FAILED(hr) )
            {
			   cout<< "Connection::Open Failed\n";
			   bRet = false;
            }
            else
            {
               cout<< "Connection Opened Successfully\n" ;
            }
        }
    }
    catch(_com_error ce)
    {
		bRet = false;
		cout<<"\nError while trying to conncet to SqlServer"<<endl;
		wcout<<ce.Description().GetBSTR()<<endl;
    }
	return bRet;
}

bool ExecuteSQLQuery(_ConnectionPtr &pSQLConn,_bstr_t query,_RecordsetPtr &recordsetPtr )
{
   	bool bRet = true;
    try
    {   
	  recordsetPtr = NULL ;
      recordsetPtr = pSQLConn->Execute(query,NULL,adOptionUnspecified);
	}
    catch(_com_error e)
    {
		bRet = false;
		cout<<"[error]"<<"Failed to execute query\n";
		wcout<<e.Description().GetBSTR()<<endl;		
        return false;
    }
  return bRet;
}

bool MoveDb(_ConnectionPtr &pSQLConn,_RecordsetPtr &recordsetPtr,const string &Dbname ,wstring &Dblogicalname, wstring &Dbpath, string &targetedpath)
{
   	USES_CONVERSION;
	string TargetedPathwithDbName;
  	string mdfORldfName;
	mdfORldfName = W2A(Dbpath.c_str());
	string DBsConvertedLogicalname = W2A(Dblogicalname.c_str());

	trim(mdfORldfName);
	trim(DBsConvertedLogicalname);
  
	size_t pos = mdfORldfName.find_last_of("\\");
	mdfORldfName.assign(mdfORldfName, pos+1, mdfORldfName.length());

    //Need to trim the target path end character with (\\) later.
    TargetedPathwithDbName = targetedpath + string("\\") + mdfORldfName;

	_bstr_t query ;
	query = "ALTER DATABASE ";
	query += "\"";
	query += Dbname.c_str();
	query += "\"";
	query += " MODIFY FILE (NAME = ";
	query += "\"";
	query += DBsConvertedLogicalname.c_str();
	query += "\"";
	query += ", FILENAME = '" ;
	query += TargetedPathwithDbName.c_str();
	query +=  "');"   ;
	
	cout<<"executing query :\n"<< query <<endl;
	if(!ExecuteSQLQuery(pSQLConn,query,recordsetPtr))
	{
		cout<<"Failed to execute the query\n";//<<query<<endl;
		return false;
	}
	if(recordsetPtr->State == adStateClosed)
	{   cout<<"****************************************************"<<endl;
		cout<<"Successfully moved :"<< Dbname.c_str()<< " to the target path "<<TargetedPathwithDbName.c_str()<< endl;
		cout<<"****************************************************"<<endl;
		return true;
	}
	else
	{
		cout<<"failed to move db:"<< Dbname.c_str()<<endl;
		return false;
	}

  return true;
}

bool OpenRegistryKey(HKEY &hKey,string sRegKeyName)
{
    //first Lets try to open keys HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Microsoft SQL Server  //key options to be set KEY_ALL_ACCESS
	//if it is 64 bit edition add option KEY_WOW64_64KEY otherwise not
	DWORD lRet;
	if(bSql64bitEdition)
	{
		lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE,sRegKeyName.c_str(),0, KEY_ALL_ACCESS|KEY_WOW64_64KEY,&hKey);
	}
	else
	{
		lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE,sRegKeyName.c_str(),0, KEY_ALL_ACCESS,&hKey);
	}

	if(lRet != ERROR_SUCCESS)
	{
		cout<<"\n[ERROR][" << GetLastError() << "]failed with this error"<<endl;
		cout<<"Failed to open  registry key : \n"<<sRegKeyName.c_str()<<endl;
		RegCloseKey(hKey);
		return false;
	}
	return true;
}

bool QueryRegistryValue(HKEY &hKey,string sRegValueName,DWORD &dwType,DWORD &dwSize,char** PerfData)
{    	
   if(RegQueryValueEx(hKey,sRegValueName.c_str(), NULL, &dwType, NULL,&dwSize)== ERROR_SUCCESS)
   {
	   *PerfData = (char*) malloc(dwSize);
	   if (NULL == *PerfData)
       {
		   cout << "Insufficient memory.\n";
       }
	   memset(*PerfData,0,dwSize);
       if (RegQueryValueEx(hKey,sRegValueName.c_str(), NULL, &dwType, (LPBYTE)*PerfData,&dwSize) == ERROR_SUCCESS)
	   {
         return true;
	   }
   }
   else
   {
	 cout << "Error reading key value  "<< sRegValueName.c_str()<<"\n";
	 return false;
   }
  return true;
}

bool SetRegistryKeyValueData(HKEY &hKey,string sRegValueName,DWORD &dwType,DWORD &dwSize,const char* PerfData)
{
	dwSize = DWORD((_tcslen(PerfData) + 1) * sizeof(char));
   
	if(RegSetValueEx(hKey,sRegValueName.c_str(), 0, REG_SZ, (LPBYTE)PerfData,dwSize)!= ERROR_SUCCESS)
	{
		cout<<"Failed to set data for registry key value"<<PerfData<<endl;
		return false;
	}

	return true;
}
bool MoveMasterDatabase(string &SqlserverName,bool &bcluster,string &targetedpath,bool bCheckClusOrNot)
{   
	SqlserverName = convertoToUpper(SqlserverName,SqlserverName.length());
	
	size_t pos =  SqlserverName.find_last_of("\\");
    string InstanceName,ServerName;
	string ClusterInstanceName;
	if(pos != string::npos)
	{
		ServerName   = SqlserverName.substr(0,pos);
		InstanceName = SqlserverName.substr(pos+1);
	}

	HKEY hKey;
	DWORD dwType=REG_MULTI_SZ;
	DWORD dwSize;
	char* PerfData = (char*) malloc(256* sizeof(CHAR));
	string VserverName;

	//opening registry
	if(OpenRegistryKey(hKey,"SOFTWARE\\Microsoft\\Microsoft SQL Server")==false)
	{
		cout<<"failed to open registry key"<<endl;
		return false;
	}
	
	//querying registry
    vector<string>ALLInstanceNames;
	if(QueryRegistryValue(hKey,"InstalledInstances",dwType,dwSize,&PerfData)==true)
	{
	   string tempstr,tempstr1 ;
       for(size_t i=0;i<dwSize;i++)
	   {   
		  if(PerfData[i] !=  '\0')
		   {
			 tempstr += PerfData[i];
		   }
		  else
		   {	
			 tempstr1 = tempstr;
			 if(!tempstr1.empty())
			 ALLInstanceNames.push_back(tempstr1);
			 tempstr.clear();
		   }
		}
	 }
    RegCloseKey(hKey);
    
	if(OpenRegistryKey(hKey,"SOFTWARE\\Microsoft\\Microsoft SQL Server\\Instance Names\\SQL")==false)
	{
	  cout<<"failed to open registry key"<<endl;
	  return false;
	}
	map<string,string>IntanceWithRegmidPaths;
    dwType = REG_SZ;
    string RegMidPath;
	
	vector<string>::iterator ALLInstanceSIter = ALLInstanceNames.begin();
	for(;ALLInstanceSIter != ALLInstanceNames.end();ALLInstanceSIter++)
	{
		if(QueryRegistryValue(hKey,(*ALLInstanceSIter),dwType,dwSize,&PerfData)==true)
		{
			RegMidPath = string(PerfData);
			IntanceWithRegmidPaths.insert(make_pair((*ALLInstanceSIter),RegMidPath));

			HKEY ClusKey;
			if(bcluster == true)
			{
				if(!OpenRegistryKey(ClusKey,"SOFTWARE\\Microsoft\\Microsoft SQL Server\\"+ RegMidPath + "\\Cluster"))
				{
					cout<<"Failed to open Cluster registery Key"<<endl;
					return false;
				}
				if(QueryRegistryValue(ClusKey,"ClusterName",dwType,dwSize,&PerfData)==true)
				{
					string temp = string(PerfData);
					if(_stricmp(SqlserverName.c_str(),temp.c_str())==0)
					{
						cout<<"Given Sqlserver is cluster virtual server"<<endl;
						ClusterInstanceName = (*ALLInstanceSIter);
						RegCloseKey(ClusKey);
						break;
					}
				}
			}

		}//end of first If condition
	}//end of for loop
	
	RegCloseKey(hKey);

	string SqlParameterKey;
	RegMidPath.clear();
	if(!InstanceName.empty())
	{  
		RegMidPath = IntanceWithRegmidPaths[InstanceName];
	}
	else
	{
		RegMidPath = IntanceWithRegmidPaths["MSSQLSERVER"];
	}
	
	SqlParameterKey = "SOFTWARE\\Microsoft\\Microsoft SQL Server\\" + RegMidPath + "\\MSSQLServer\\Parameters";

	if(OpenRegistryKey(hKey,SqlParameterKey)==false)
	{
		cout<<"failed to open registry key : "<<SqlParameterKey.c_str()<<endl;
		return false;
	}
	else
	{
		string tgtmasterDbpath  = "-d" + targetedpath + "\\"+"master.mdf";
		string tgtmasterLogpath = "-l" + targetedpath + "\\"+"mastlog.ldf";
		if(!SetRegistryKeyValueData(hKey,"SQLArg0",dwType,dwSize,tgtmasterDbpath.c_str()))
		 {
			cout<<"failed to update registry key value"<<endl;
			return false;
		 }
			
		if(!SetRegistryKeyValueData(hKey,"SQLArg2",dwType,dwSize,tgtmasterLogpath.c_str()))
		 {
			cout<<"failed to update registry key value"<<endl;
			return false;
		 }

	}

	RegCloseKey(hKey);
	
   free(PerfData);
   return true;
}
bool getInstanceServiceNamesOrClusResourceNames(string &SqlserverName,bool &bCluster)
{
	string szSqlServer, szSqlServerAgent, insName;
	size_t slashPos;
    if(!bCluster)
	{
		szSqlServer = "MSSQLSERVER";
		szSqlServerAgent = "SQLSERVERAGENT";		
		insName = SqlserverName;
		slashPos = string::npos;

		slashPos = insName.find_first_of('\\');
		if( slashPos != string::npos )
		{
			insName.assign(insName, slashPos+1, insName.length());
			szSqlServer = "MSSQL$" + insName;
			szSqlServerAgent = "SQLAgent$" + insName;
		}
	}
	else
	{		
		szSqlServer = "SQL Server";
		szSqlServerAgent = "SQL Server Agent";		
		insName = SqlserverName;
	/*	slashPos = string::npos;

		slashPos = insName.find_first_of('\\');
		if( slashPos != string::npos )
		{
			insName.assign(insName, slashPos+1, insName.length());
			szSqlServer = "SQL Server(" + insName + ")";
			szSqlServerAgent = "SQL Server Agent(" + insName + ")";;
		}*/
	}
	vInstServicesOrResourcenames.push_back(szSqlServerAgent);
	vInstServicesOrResourcenames.push_back(szSqlServer);

   return true;
}
bool StopMSSQLServices(string &SqlserverName,bool &bcluster)
{
	vector<string>::const_iterator iSvcIter;
	if(bcluster == false)
	{
		cout << "Stopping MSSQL services\n";				
		for( iSvcIter = vInstServicesOrResourcenames.begin(); iSvcIter != vInstServicesOrResourcenames.end(); iSvcIter++)
		{
			if( stopService(*iSvcIter) == false )
			{
				cout << "Failed to stop the service " << iSvcIter->c_str() << endl;
				return false;
			}
		}		
	}
	else
	{
		for( iSvcIter = vInstServicesOrResourcenames.begin(); iSvcIter != vInstServicesOrResourcenames.end(); iSvcIter++)
		{
			if (OfflineorOnlineResource(*iSvcIter,"sql",SqlserverName,false)==false)
			{
				cout << "\n[ERROR] Failed to Offline Cluster SQL Server resource\n";
				return false;
			}
		}
	}

 return true;
}
bool StartMSSQLServices(string &SqlserverName,bool &bcluster)
{
	vector<string>::const_iterator iSvcIter;
	if(bcluster == false)
	{
		cout << "Starting MSSQL services\n";
		for( iSvcIter = vInstServicesOrResourcenames.begin(); iSvcIter != vInstServicesOrResourcenames.end(); iSvcIter++)
		{
			if( startService(*iSvcIter) == false )
			{
				cout << "Failed to start the service " << iSvcIter->c_str() << endl;		
				return false;
			}
		}
	}
	else
	{
		for( iSvcIter = vInstServicesOrResourcenames.begin(); iSvcIter != vInstServicesOrResourcenames.end(); iSvcIter++)
		{
			if (OfflineorOnlineResource(*iSvcIter,"sql",SqlserverName,true)==false)
			{
				cout << "\n[ERROR] Failed to Online Cluster SQL Server resource\n";
				return false;
			}
		}
	}

  return true;
}
bool stopService(const string& serviceName)
{
	SC_HANDLE hInmSCService;        

    SERVICE_STATUS_PROCESS  svcStatusProcess;
    SERVICE_STATUS svcStatus;
    DWORD actualSize = 0;
	ULONG dwRet = 0;
	LPVOID lpMsgBuf;

	SC_HANDLE hInmSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (NULL == hInmSCManager) {
		cout << "Failed to Get handle to Windows Services Manager in OpenSCMManager\n";
        return false;
    }

	// stop requested service
	hInmSCService = ::OpenService(hInmSCManager, serviceName.c_str(), SERVICE_ALL_ACCESS); 
    if (NULL != hInmSCService) 
	{ 
		if (::QueryServiceStatusEx(hInmSCService, SC_STATUS_PROCESS_INFO, (LPBYTE)&svcStatusProcess, sizeof(svcStatusProcess), &actualSize) && SERVICE_RUNNING == svcStatusProcess.dwCurrentState) {
            // stop service
			if (::ControlService(hInmSCService, SERVICE_CONTROL_STOP, &svcStatus)) {
                // wait for it to actually stop
				cout << "Waiting for the service " << serviceName.c_str() << " to stop\n";
				int retry = 1;
                do {
                    Sleep(1000); 
				} while (::QueryServiceStatusEx(hInmSCService, SC_STATUS_PROCESS_INFO, (LPBYTE)&svcStatusProcess, sizeof(svcStatusProcess), &actualSize) && SERVICE_STOPPED != svcStatusProcess.dwCurrentState && retry++ <= 600);
				if (::QueryServiceStatusEx(hInmSCService, SC_STATUS_PROCESS_INFO, (LPBYTE)&svcStatusProcess, sizeof(svcStatusProcess), &actualSize) && SERVICE_STOPPED == svcStatusProcess.dwCurrentState) {
					cout << "Successfully stopped the service: " << serviceName.c_str() << "\n";
					::CloseServiceHandle(hInmSCService); 					
					return true;
				}
            }
		}
		else
		{
			cout << "The service " << serviceName.c_str() << " is not running\n";
			//close the service handle
			::CloseServiceHandle(hInmSCService); 
			return true;
		}
		// Collect the error ahead of other statement ::CloseServiceHandle()
		dwRet = GetLastError();
		//close the service handle
		::CloseServiceHandle(hInmSCService); 
	}
	
	if( !dwRet )
		dwRet = GetLastError();
	::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                    NULL, dwRet, 0, (LPTSTR)&lpMsgBuf, 0, NULL);
	if(dwRet)
	{
		printf("\n[ServiceName: %s]Error:[%d] %s\n",serviceName.c_str(),dwRet,(LPCTSTR)lpMsgBuf);
	}		

	// Free the buffer
	LocalFree(lpMsgBuf);
		
	if(dwRet == ERROR_SERVICE_DOES_NOT_EXIST)
	{
		//cout << "\nService doesn't exist: " << serviceName.c_str() << "\n";
		return true;
	}

	// Return failure in stopping the service
    return false;	
}
bool startService(const string& serviceName)
{
	SC_HANDLE hInmSCService;        

    SERVICE_STATUS_PROCESS  svcStatusProcess;
    DWORD actualSize = 0;
	ULONG dwRet = 0;
	LPVOID lpMsgBuf;

	SC_HANDLE hInmSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (NULL == hInmSCManager) {
		cout << "Failed to Get handle to Windows Services Manager in OpenSCMManager\n";
        return false;
    }

	// start requested service
	hInmSCService = ::OpenService(hInmSCManager, serviceName.c_str(), SERVICE_ALL_ACCESS); 
    if (NULL != hInmSCService) { 
		if (::QueryServiceStatusEx(hInmSCService, SC_STATUS_PROCESS_INFO, (LPBYTE)&svcStatusProcess, sizeof(svcStatusProcess), &actualSize) && SERVICE_RUNNING == svcStatusProcess.dwCurrentState) {            
			cout << "The service " << serviceName.c_str() << " is already running\n";
			return true;
        }
		else if (::StartService(hInmSCService, 0, NULL) != 0)
		{	
			cout << "Waiting for the service to start: " << serviceName.c_str() << "\n";
			int retry = 1;
            do {
                Sleep(1000); 
			} while (::QueryServiceStatusEx(hInmSCService, SC_STATUS_PROCESS_INFO, (LPBYTE)&svcStatusProcess, sizeof(svcStatusProcess), &actualSize) && SERVICE_RUNNING != svcStatusProcess.dwCurrentState && retry++ <= 600);
			
			if (::QueryServiceStatusEx(hInmSCService, SC_STATUS_PROCESS_INFO, (LPBYTE)&svcStatusProcess, sizeof(svcStatusProcess), &actualSize) && SERVICE_RUNNING == svcStatusProcess.dwCurrentState) {
					cout << "Successfully started the service: " << serviceName.c_str() << "\n";
					::CloseServiceHandle(hInmSCService); 					
					return true;
				}
		 }
		
		// Collect the error ahead of other statement ::CloseServiceHandle()
		dwRet = GetLastError();

		// Close the service handle
		::CloseServiceHandle(hInmSCService); 
    }
	
	if( !dwRet )
	{
		dwRet = GetLastError();
		::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                    NULL, dwRet, 0, (LPTSTR)&lpMsgBuf, 0, NULL);
	}
	if(dwRet)
	{
		printf("\n[ServiceName: %s]Error:[%d] %s\n",serviceName.c_str(),dwRet,(LPCTSTR)lpMsgBuf);
	}		

	// Free the buffer
	LocalFree(lpMsgBuf);

	if(dwRet == ERROR_SERVICE_DOES_NOT_EXIST)
	{
		//cout << "Service doesn't exist: " << serviceName.c_str() << "\n";s
		return true;
	}
	return false;	
}

bool CopyDbfilesWithACLs(wstring &SourcePath, string &Tgtpath)
{
	string SrcPath(SourcePath.begin(), SourcePath.end());
	SrcPath.assign(SourcePath.begin(), SourcePath.end());


    STARTUPINFO lInmStartupInfo;
    PROCESS_INFORMATION lInmProcessInfo;

    ZeroMemory( &lInmStartupInfo, sizeof(lInmStartupInfo) );
    lInmStartupInfo.cb = sizeof(lInmStartupInfo);
    ZeroMemory( &lInmProcessInfo, sizeof(lInmProcessInfo) );
    string ExcuteCommand;
    ExcuteCommand = string("XCOPY") + string(" ") + "\"" + SrcPath + "\"" + string(" ") + "\"" + Tgtpath + "\"" + string(" ") + string(" /O /Y /I /F");
    // Create process to copy the database files
	if( !CreateProcess(NULL, LPSTR(ExcuteCommand.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &lInmStartupInfo, &lInmProcessInfo ) 
    ) 
    {
        cout<<"Copying Database Process failed .\n"<< GetLastError() ;
        return false;
    }
	else
	{
      	 ::ResumeThread(lInmProcessInfo.hThread);
	}
    // Wait until child process exits.
    DWORD RetValue = WaitForSingleObject( lInmProcessInfo.hProcess, INFINITE );
	if(RetValue == WAIT_FAILED)
	{
		cout<<" Wait for single Object has failed"<<endl;
		CloseHandle( lInmProcessInfo.hProcess );
        CloseHandle( lInmProcessInfo.hThread );
	    return false;
	}
    // Close process and thread handles. 
    CloseHandle( lInmProcessInfo.hProcess );
    CloseHandle( lInmProcessInfo.hThread );
 
	return true;
}

bool
OfflineorOnlineResource(const string& resourceType, const string& appName,string m_VirtualServerName,bool bMakeOnline)
{
	USES_CONVERSION;
	WCHAR lInmClustName[MAX_PATH];
	DWORD lInmClustIndex = 0;
	DWORD lInmNameLength = 0;
	DWORD lInmType;
	DWORD lInmStatus = ERROR_SUCCESS;
	bool result = false;
	HRESOURCE hlInmResource = 0;
	string szVirtualServer = string(m_VirtualServerName);
	size_t seperatorSlash = string::npos;

	seperatorSlash = szVirtualServer.find_first_of('\\');
	if( seperatorSlash != string::npos )
	{
		szVirtualServer = szVirtualServer.assign(szVirtualServer, 0, seperatorSlash );
	}

	// try to open cluster now
	HCLUSTER hlInmCluster = OpenCluster(0);
	if (0 == hlInmCluster)
	{
		lInmStatus = GetLastError();
		cout << "[ERROR][" << lInmStatus << "] MoveDbs::OfflineorOnlineResource:OpenCluster failed.\n" 
			 << "Probably not a cluster Configuration\n";
	}
	else
	{
		// open enumerator to cluster resources
		HCLUSENUM hInmClustEnum = ClusterOpenEnum(hlInmCluster, CLUSTER_ENUM_RESOURCE);
		if (0 == hInmClustEnum) {
			cout << "[ERROR][" << lInmStatus << "] MoveDbs::OfflineorOnlineResource:ClusterOpenEnum failed.\n";
        }
		else
		{
			do
			{
				lInmNameLength = sizeof(lInmClustName) / sizeof(WCHAR);
				// enumerate the clusters
				lInmStatus = ClusterEnum(hInmClustEnum, lInmClustIndex, &lInmType, lInmClustName, &lInmNameLength);                    
				if (ERROR_SUCCESS != lInmStatus) {
					if (ERROR_NO_MORE_ITEMS != lInmStatus) {
						// log warning
						cout << "[ERROR][" << lInmStatus << "] MoveDbs::OfflineorOnlineResource:ClusterEnum failed.\n"
							 << " Probably reached end of the list.\n";
					}
				}
				// look for Exchange System Attendant Cluster Resource
				else
				{
					hlInmResource = OpenClusterResource(hlInmCluster, lInmClustName);
					if (0 == hlInmResource)
					{
						lInmStatus = GetLastError();
						if (ERROR_RESOURCE_NOT_FOUND != lInmStatus) {
							cout << "[ERROR][" << lInmStatus << "] MoveDbs::OfflineorOnlineResource:OpenClusterResource failed.\n";
						}
					}
					else
					{
						bool bOffline = false;
						bool bOnLine = false;
						WCHAR *resName;
						DWORD resNameLen = MAX_PATH;
						DWORD bytesReturned;
						DWORD cbBuffSize = MAX_PATH;
						LPVOID lpszType;
						lpszType = LocalAlloc( LPTR, cbBuffSize );

						if( NULL == lpszType )
						{											
							lInmStatus = GetLastError();
                            cout << "[ERROR][" << lInmStatus << "] Local buffer allocation failure...\n";	
							break;
						}

						lInmStatus = ClusterResourceControl(
												hlInmResource,             
												NULL,		
												CLUSCTL_RESOURCE_GET_RESOURCE_TYPE, 
												NULL,            
												0,               
												(LPVOID)lpszType,
												cbBuffSize,
												&bytesReturned);

						if( ERROR_MORE_DATA == lInmStatus )
						{
							LocalFree( lpszType );
							cbBuffSize = bytesReturned;
							
							//cout << "Allocating " << bytesReturned << " bytes...\n";
							lpszType = LocalAlloc( LPTR, cbBuffSize );

							if( NULL == lpszType )
							{											
								lInmStatus = GetLastError();
                                cout << "[ERROR][" << lInmStatus << "] Local buffer allocation failure...\n";	
								break;
							}

							lInmStatus = ClusterResourceControl(
											hlInmResource,             
											NULL,		
											CLUSCTL_RESOURCE_GET_RESOURCE_TYPE, 
											NULL,            
											0,               
											(LPVOID)lpszType,
											cbBuffSize,
											&bytesReturned);
						}
					
						if (ERROR_SUCCESS != lInmStatus)
						{
							cout << "[ERROR][" << lInmStatus << "] MoveDbs::OfflineorOnlineResource:ClusterResourceControl failed.\n" 
								 << "\nFailed to get the resources of type: " << resourceType.c_str() << std::endl;
						} 
						else
						{							
							if (_strcmpi(W2A((WCHAR*)lpszType), resourceType.c_str()) == 0)
							{
								LPWSTR pszPropertyVal = NULL;
								cbBuffSize = MAX_PATH;
								LPVOID lpOutBuffer = NULL;
								LPWSTR pszPropertyName = NULL;

								
								if( (_strcmpi(appName.c_str(), "SQL") == 0) )
								{
									pszPropertyName = L"VirtualServerName";
								}
								else
								{
									cout << "[ERROR] Invalid application type: " << appName.c_str() << endl;
									return false;
								}
								
								lpOutBuffer = LocalAlloc( LPTR, cbBuffSize );
									
								if( NULL == lpOutBuffer )
								{										
									lInmStatus = GetLastError();
									cout << "[ERROR][" << lInmStatus << "] Local buffer allocation failure...\n";
									break;
								}

								lInmStatus = ClusterResourceControl(
													hlInmResource,             
													NULL,		
													CLUSCTL_RESOURCE_GET_PRIVATE_PROPERTIES, 
													NULL,            
													0,               
													lpOutBuffer,									
													cbBuffSize,
													&bytesReturned);

								if( ERROR_MORE_DATA == lInmStatus )
								{
									LocalFree( lpOutBuffer );
									cbBuffSize = bytesReturned;
									
									//cout << "Allocating " << bytesReturned << " bytes...\n";
									lpOutBuffer = LocalAlloc( LPTR, cbBuffSize );

									if( NULL == lpOutBuffer )
									{											
										lInmStatus = GetLastError();
                                        cout << "[ERROR][" << lInmStatus << "] Local buffer allocation failure...\n";	
										break;
									}

									lInmStatus = ClusterResourceControl(
													hlInmResource,             
													NULL,		
													CLUSCTL_RESOURCE_GET_PRIVATE_PROPERTIES, 
													NULL,            
													0,               
													lpOutBuffer,									
													cbBuffSize,
													&bytesReturned);
								}

								if( ERROR_SUCCESS != lInmStatus )
								{									
									resName = (WCHAR*) LocalAlloc( LPTR, resNameLen ); //Name of resource
									if( ERROR_SUCCESS == ResUtilGetResourceName(hlInmResource, resName, &resNameLen ))
									{
                                        cout << "[ERROR][" << lInmStatus << "] Failed to get the private properties of the resource : " 
											 << W2A(resName) << endl;
									}
									else
									{
										cout << "[ERROR][" << lInmStatus << "] Failed to get the private properties of the resource of type : " 
											 << resourceType.c_str() << endl;
									}
									LocalFree( resName );
								}
								else
								{									
									lInmStatus = ResUtilFindSzProperty(lpOutBuffer, cbBuffSize, (LPCWSTR) pszPropertyName, &pszPropertyVal);

									if( ERROR_SUCCESS != lInmStatus )
									{
										resName = (WCHAR*) LocalAlloc( LPTR, resNameLen ); //Name of resource
										if( ERROR_SUCCESS == ResUtilGetResourceName(hlInmResource, resName, &resNameLen ))
										{
											cout << "[ERROR][" << lInmStatus << "] Failed to parse the private properties the resource: " 
												<< W2A(resName) << endl;
										}
										else
										{
											cout << "[ERROR][" << lInmStatus << "] Failed to parse the private properties the resource of type: " 
												<< resourceType.c_str() << endl;
										}
										LocalFree( resName );
									}
									else
									{
										//cout << "Found the virtual server: " << W2A(pszPropertyVal) << endl ;	                                        
									}
									LocalFree( lpOutBuffer );
								}
                                
								if( _strcmpi( szVirtualServer.c_str(), W2A(pszPropertyVal) ) == 0 )
								{
									//cout << endl<< "Processing the virtual server: " << W2A(pszPropertyVal) << endl;
									//Check if already Offline or not
									if(bMakeOnline == false)
									{
										if(GetClusterResourceState(hlInmResource, NULL, NULL, NULL, NULL) == ClusterResourceOffline) 
											
										{
											result = true; //Offline objective is met
											bOffline = false;											
											cout <<"The resource " << resourceType.c_str() <<" is already offline...\n";
										}
										else
										{
											bOffline = true;
										}
									}
									if(bMakeOnline == true)
									{
										if(GetClusterResourceState(hlInmResource, NULL, NULL, NULL, NULL) == ClusterResourceOnline) 
										{
											result = true; 
											bOnLine = false;
											cout <<"The resource " << resourceType.c_str() <<" is already Online...\n";
										}
										else
										{
											bOnLine = true;
										}
									}
									
								}
								else
								{
									// Skip the resource that does not belong to current Virtual server\n";	
									bOffline = false;
									bOnLine = false;
								}

								if(bOffline)
								{									
									resName = (WCHAR*) LocalAlloc( LPTR, resNameLen ); //Name of resource
									if( ERROR_SUCCESS == ResUtilGetResourceName(hlInmResource, resName, &resNameLen ))
										cout << "Attempting to offline the cluster resource: " << W2A(resName) << endl;
									else
										cout << "Attempting to offline the resource of type: " << resourceType.c_str() << endl;
									LocalFree( resName );											

                                    unsigned int retry = RESOURCE_OFFLINE_ATTEMPTS;
									WCHAR nNameRes[MAX_PATH];
									bytesReturned = sizeof(nNameRes);

									// Get the name of the dependent "network name" resource
									while (1) {
										lInmStatus = OfflineClusterResource(hlInmResource);
										if (ERROR_SUCCESS != lInmStatus) {										
											while(retry-- && GetClusterResourceState(hlInmResource, NULL, NULL, NULL, NULL) == ClusterResourceOfflinePending);
											{
												if(retry == RESOURCE_OFFLINE_ATTEMPTS-1 )
													cout << "Waiting for 3 seconds for the cluster resource: " << resourceType.c_str() << " to go offline...\n";
												else
													cout << "Waiting for 3 more seconds for the cluster resource: " << resourceType.c_str() << " to go offline...\n";
												Sleep(3000);
											}
										}									
										if( GetClusterResourceState(hlInmResource, NULL, NULL, NULL, NULL) == ClusterResourceOffline)
										{
											cout << "Successfully Offlined Cluster Resource : " << resourceType.c_str() << std::endl;
											result = true;
											break;
										}
									}
								}//Offline the resource

								
								if(bOnLine)
								{
									
									resName = (WCHAR*) LocalAlloc( LPTR, resNameLen ); //Name of resource
									if( ERROR_SUCCESS == ResUtilGetResourceName(hlInmResource, resName, &resNameLen ))
										cout << "Attempting to online the cluster resource: " << W2A(resName) << endl;
									else
										cout << "Attempting to online the resource of type: " << resourceType.c_str() << endl;
									LocalFree( resName );											

                                    unsigned int retry = RESOURCE_ONLINE_ATTEMPTS;
									WCHAR nNameRes[MAX_PATH];
									bytesReturned = sizeof(nNameRes);

									// Get the name of the dependent "network name" resource
									while (1) {
										lInmStatus = OnlineClusterResource(hlInmResource);
										if (ERROR_SUCCESS != lInmStatus) {										
											while(retry-- && GetClusterResourceState(hlInmResource, NULL, NULL, NULL, NULL) == ClusterResourceOnlinePending);
											{
												if(retry == RESOURCE_ONLINE_ATTEMPTS-1 )
													cout << "Waiting for 3 seconds to cluster resource: " << resourceType.c_str() << " come online...\n";
												else
													cout << "Waiting for 3 more seconds to cluster resource: " << resourceType.c_str() << " come online...\n";
												Sleep(3000);
											}
										}									
										if( GetClusterResourceState(hlInmResource, NULL, NULL, NULL, NULL) == ClusterResourceOnline)
										{
											cout << "Successfully Onlined Cluster Resource : " << resourceType.c_str() << std::endl;
											result = true;
											break;
										}
									}
								}//online resource end
							}
						}
						CloseClusterResource(hlInmResource);
					}
					++lInmClustIndex;
				}
			} while (ERROR_SUCCESS == lInmStatus && result == false);
			// Close Enumerator to Cluster Resources
			ClusterCloseEnum(hInmClustEnum);
		}
		CloseCluster(hlInmCluster);
	}
	return result;
}

void trim(string& szlInmInput, string szlInmTrimCharacters) 
{
    size_t end = szlInmInput.size() - 1, start = 0;
    end = szlInmInput.find_last_not_of(szlInmTrimCharacters);
    if (end == string::npos) end = szlInmInput.size() - 1;

    start = szlInmInput.find_first_not_of(szlInmTrimCharacters);
    if (start == string::npos) start = 0;

    szlInmInput = szlInmInput.substr(start, end - start + 1);
}

string convertoToLower(const string szlInmStringToConvert, size_t lInmLength)
{
    string szlInmConvertedString;
    size_t tmplen;
    INM_SAFE_ARITHMETIC(tmplen = InmSafeInt<size_t>::Type(lInmLength) + 1, INMAGE_EX(lInmLength))
    char *tmp = (char *)malloc(tmplen);
    const char *plInmStringToConvertPtr = szlInmStringToConvert.c_str();

    ZeroMemory(tmp, lInmLength + 1);

    for(unsigned long i = 0; i < lInmLength; i++)
    {
        tmp[i] = tolower(plInmStringToConvertPtr[i]);
    }

    szlInmConvertedString = tmp;
    free(tmp);
    return szlInmConvertedString;
}

string convertoToUpper(const string szlInmStringToConvert, size_t lInmLength)
{
    string szlInmConvertedString;
    size_t tmplen;
    INM_SAFE_ARITHMETIC(tmplen = InmSafeInt<size_t>::Type(lInmLength) + 1, INMAGE_EX(lInmLength))
    char *tmp = (char *)malloc(tmplen);
    const char *plInmStringToConvertPtr = szlInmStringToConvert.c_str();

    ZeroMemory(tmp, lInmLength + 1);

    for(unsigned long i = 0; i < lInmLength; i++)
    {
        tmp[i] = toupper(plInmStringToConvertPtr[i]);
    }

    szlInmConvertedString = tmp;
    free(tmp);
    return szlInmConvertedString;
}

void ShowHelp()
{
	    cout << endl;
        cout << "MoveSQLDB   -server  <SQLServer Name>\n";
		cout << "                     For SQL Named Instance enter server as <Machinename\\Instancename>\n";
		cout << "                     For SQL Cluster enter server as <VirtualServerName>\n";
		cout << "                     For SQL Cluster with Named Instance enter server as <VirtualServerName\\Instancename>\n\n";
		cout << "            -db  <Give the names of the specific databases separated with ';' to move>\n\n";
		cout << "            -moveall <To move all the databases>\n\n"; 
        cout << "            -newpath <New Path to move the databases>\n";
        cout << "                     Ensure that this path is not ending with \\ character\n\n";
        cout << "            [-help/?/-h]\n\n";
        cout << "\nNOTE: -db and -moveall options are mutually exclusive\n";
		cout << "NOTE: The logon user must be a member of SQL user accounts with required priviges to modify databases\n\n";
		cout << "Examples:\n";
		cout << "To move all the databases:\n";
		cout << "MoveSQLDB.exe -server TestSql -moveall -newpath \"E:\\Newdatabasepath\\data\""<<"\n\n";
		cout << "To move all the databases in case of SQL Named Instance:\n";
		cout << "MoveSQLDB.exe -server \"TestSql\\Instance1\" -moveall -newpath \"E:\\Newdatabasepath\\data\""<<"\n\n";
		cout << "To move a single database:\n";
		cout << "MoveSQLDB.exe -server TestSql -db testdb -newpath E:\\Newdatabasepath\\data"<<"\n\n";
		cout << "To move given list of databases:\n";
		cout << "MoveSQLDB.exe -server TestSql -db testdb;testdb1;testdb2 -newpath E:\\Newdatabasepath\\data"<<endl;
}
