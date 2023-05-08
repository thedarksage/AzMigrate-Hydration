#include "sqlado.h"
#include <sstream>
#include <atlconv.h>
#include "util\common\util.h"
using namespace AdoNS;
SQLAdo* SQLAdo::getSQLAdoImpl(InmProtectedAppType type)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SQLAdo *sqlAdoPtr;
    switch( type )
    {
    case INM_APP_MSSQL2000 :
        sqlAdoPtr = new SQL2000Ado();
        break ;
    case INM_APP_MSSQL2005 :
        DebugPrintf(SV_LOG_DEBUG, "Created INM_APP_MSSQL2005 Ado Object\n") ;
        sqlAdoPtr = new SQL2005Ado();
        break ;
    case INM_APP_MSSQL2008 :
        DebugPrintf(SV_LOG_DEBUG, "Created INM_APP_MSSQL2008 Ado Object\n") ;
        sqlAdoPtr = new SQL2008Ado();
        break ;
	case INM_APP_MSSQL2012 :
        DebugPrintf(SV_LOG_DEBUG, "Created INM_APP_MSSQL2012 Ado Object\n") ;
        sqlAdoPtr = new SQL2012Ado();
        break ;
    default:
        DebugPrintf(SV_LOG_ERROR, "Invalid MS SQL Application Type\n") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return sqlAdoPtr ;
}

SVERROR SQLAdo::init()
{
    SVERROR bRet = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    if( InmAdo::init() != SVS_OK )
    {
        DebugPrintf(SV_LOG_ERROR, "Initialization Failed\n") ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "initialization was successful\n") ;
        bRet = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;

}
_bstr_t SQLAdo::getConnStr()
{
    return m_conStr ;
}
void SQLAdo::setConnStr(_bstr_t conStr)
{
    DebugPrintf(SV_LOG_DEBUG, "Connection String is %S\n", conStr.GetBSTR()) ;
    m_conStr = conStr ;
}
SQLAdo::~SQLAdo()
{
    if( m_pConn != NULL && m_pConn->State != adStateClosed)
    {
        m_pConn->Close();
        m_pConn = NULL ;
    }
}
SVERROR SQLAdo::connect()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    HRESULT hr ;
    m_pConn = NULL ;
    try
    {
        hr = m_pConn.CreateInstance((__uuidof(Connection)));
        if( FAILED(hr) )
        {
            DebugPrintf(SV_LOG_ERROR, "Connection::CreateInstance Failed. [ERROR]: %ld \n", hr) ;
        }
        else
        {
            m_pConn->ConnectionTimeout = 30 ;
            hr = m_pConn->Open(m_conStr,"","",0) ;
            if( FAILED(hr) )
            {
                DebugPrintf(SV_LOG_ERROR, "Connection::Open Failed\n") ;
                DebugPrintf(SV_LOG_ERROR, "Connection String is %S\n", m_conStr.GetBSTR()) ;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "Connection Opened Successfully\n") ;
                bRet = SVS_OK ;
            }
        }

    }
    catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_WARNING, "Com Error %S\n", ce.Description().GetBSTR()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR SQLAdo::execute(_bstr_t query, _RecordsetPtr& recordsetPtr)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    recordsetPtr = NULL ;
    try
    {
        DebugPrintf(SV_LOG_DEBUG, "Query is %S\n", query.GetBSTR()) ;
        recordsetPtr = m_pConn->Execute(query,NULL,adOptionUnspecified);
        bRet = SVS_OK ;
    }
    catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


SVERROR SQL2000Ado::connect(const std::string sqlInstance, const std::string& dbName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _bstr_t strCon = "Driver={SQL Server};" ;
    strCon += "Server=" ;
    strCon += sqlInstance.c_str() ;
    strCon += ";" ;
    strCon += "Database=" ;
    strCon += dbName.c_str() ;
    strCon += ";" ;
    strCon += "Trusted_Connection=yes" ;
    setConnStr(strCon) ;
    if( SQLAdo::connect() != SVS_OK )
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to Connect to SQL 2000 database\n") ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Successfully Connected to SQL 2000 database\n") ;
        bRet = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR SQL2000Ado::getDbNames(std::list<std::string>& dbNames)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _bstr_t query ;
    USES_CONVERSION;
    query = "SELECT name FROM master.dbo.sysdatabases" ;
    _RecordsetPtr pRecordSet = NULL ;
    _variant_t dbNameVar ; 
    try
    {
        if( execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL )
            {
                while (!pRecordSet->GetEndOfFile())
                {
                    dbNameVar = pRecordSet->GetCollect(L"name");
                    if( dbNameVar.bstrVal != NULL )
					{
						dbNames.push_back(W2A(dbNameVar.bstrVal)) ;
						DebugPrintf(SV_LOG_DEBUG, "DB Name :%S\n", dbNameVar.bstrVal);
					}
                    dbNameVar.Clear() ;
                    pRecordSet->MoveNext();
                }

                pRecordSet->Close() ;
                bRet = SVS_OK ;
                DebugPrintf(SV_LOG_DEBUG, "No of databases attached %d\n", dbNames.size()) ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to determine the dbnames\n") ;
        }
    }
    catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR SQL2000Ado::discoverDatabase(const std::string& dbName, MSSQLDBMetaData& database)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    std::stringstream stream;
    database.errCode = DISCOVERY_SUCCESS;
    _bstr_t query ;
    query = "SELECT filename FROM " ;
    query += "\"";
    query += dbName.c_str() ;
    query += "\"";
    query += ".dbo.sysfiles;" ;

    USES_CONVERSION ;
    _RecordsetPtr pRecordSet = NULL ;
    _variant_t dbFileVar ; 
    database.m_dbFiles.clear() ;
    database.m_dbOnline = false ;
    database.m_dbName = dbName ;
    try
    {
        if( execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL )
            {
                while (!pRecordSet->GetEndOfFile())
                {
                    dbFileVar = pRecordSet->GetCollect(L"filename");
					if( dbFileVar.bstrVal != NULL )
					{
						database.m_dbFiles.push_back(W2A(dbFileVar.bstrVal)) ;
						DebugPrintf(SV_LOG_DEBUG, "db file name :%S\n", dbFileVar.bstrVal);
					}
                    dbFileVar.Clear() ;
                    pRecordSet->MoveNext();
                }
                pRecordSet->Close() ;
                bRet = SVS_OK ;
                database.m_dbOnline = true ;
                stream << "DB : " << dbName << " status is ONLINE";
                DebugPrintf(SV_LOG_DEBUG, "No of database file names %d\n", database.m_dbFiles.size()) ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to determine the database file names\n") ;
            stream << "Unable to determine the database file names";
            database.errCode = DISCOVERY_FAIL;
        }
    }
    catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
        database.errCode = DISCOVERY_FAIL;
        stream << "Got exception while querying for dbname "  << ce.Description().GetBSTR();
    }    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    database.errorString = stream.str();
    return bRet ;
}


SVERROR SQL2005Ado::connect(const std::string sqlInstance, const std::string& dbName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _bstr_t strCon = "Provider=SQLNCLI;" ;
    strCon += "Server=" ;
    strCon += sqlInstance.c_str() ;
    strCon += ";" ;
    strCon += "Database=" ;
    strCon += dbName.c_str() ;
    strCon += ";" ;
    strCon += "Trusted_Connection=yes" ;
    setConnStr(strCon) ;
    if( SQLAdo::connect() != SVS_OK )
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to Connect to SQL 2005 database\n") ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Successfully Connected to SQL 2005 database\n") ;
        bRet = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR SQL2005Ado::getDbNames(std::list<std::string>& dbNames)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _bstr_t query ;
    query = "SELECT name FROM sys.databases" ;
    USES_CONVERSION ;
    _RecordsetPtr pRecordSet = NULL ;
    _variant_t dbNameVar ; 
    try
    {
        if( execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL )
            {
                while (!pRecordSet->GetEndOfFile())
                {
                    dbNameVar = pRecordSet->GetCollect(L"name");
					if( dbNameVar.bstrVal != NULL )
					{
						dbNames.push_back(W2A(dbNameVar.bstrVal)) ;
						DebugPrintf(SV_LOG_DEBUG, "DB Name :%S\n", dbNameVar.bstrVal);
					}
                    dbNameVar.Clear() ;
                    pRecordSet->MoveNext();
                }
                pRecordSet->Close() ;
                bRet = SVS_OK ;
                DebugPrintf(SV_LOG_DEBUG, "No of databases attached %d\n", dbNames.size()) ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to determine the dbnames\n") ;
        }
    }
    catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR SQL2005Ado::discoverDatabase(const std::string& dbName, MSSQLDBMetaData& database)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    std::stringstream stream;
    database.errCode = DISCOVERY_SUCCESS;
    _bstr_t query ;
    query = "SELECT physical_name FROM  " ;
    query += "\"";
    query += dbName.c_str() ;
    query += "\"";
    query += ".sys.database_files;" ;

    USES_CONVERSION ;
    _RecordsetPtr pRecordSet = NULL ;
    _variant_t dbFileVar ; 
    database.m_dbFiles.clear() ;
    database.m_dbName = dbName ;
    database.m_dbOnline = false ;
    try
    {
        if( execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL )
            {
                while (!pRecordSet->GetEndOfFile())
                {
                    dbFileVar = pRecordSet->GetCollect(L"physical_name");
					if( dbFileVar.bstrVal != NULL )
					{
						database.m_dbFiles.push_back(W2A(dbFileVar.bstrVal)) ;
						DebugPrintf(SV_LOG_DEBUG, "db file name :%S\n", dbFileVar.bstrVal);
					}
                    dbFileVar.Clear() ;
                    pRecordSet->MoveNext();
                }
                pRecordSet->Close() ;
                bRet = SVS_OK ;
                 stream << "DB : " << dbName << " status is ONLINE";
                DebugPrintf(SV_LOG_DEBUG, "No of database file names %d\n", database.m_dbFiles.size()) ;
                database.m_dbOnline = true ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to determine the database file names\n") ;
            stream << "Unable to determine the database file names";
            database.errCode = DISCOVERY_FAIL;
        }
    }
    catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
        database.errCode = DISCOVERY_FAIL;
        stream << "Got exception while querying for dbname "  << ce.Description().GetBSTR();
    }    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    database.errorString = stream.str();
    return bRet ;
}



SVERROR SQL2008Ado::connect(const std::string sqlInstance, const std::string& dbName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _bstr_t strCon = "Provider=SQLNCLI10;" ;
    strCon += "Server=" ;
    strCon += sqlInstance.c_str() ;
    strCon += ";" ;
    strCon += "Database=" ;
    strCon += dbName.c_str() ;
    strCon += ";" ;
    strCon += "Trusted_Connection=yes" ;
    setConnStr(strCon) ;
    if( SQLAdo::connect() != SVS_OK )
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to Connect to SQL 2008 database\n") ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Successfully Connected to SQL 2008 database\n") ;
        bRet = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR SQL2008Ado::getDbNames(std::list<std::string>& dbNames)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _bstr_t query ;
    query = "SELECT name FROM master.dbo.sysdatabases" ;
    USES_CONVERSION ;
    _RecordsetPtr pRecordSet = NULL ;
    _variant_t dbNameVar ; 
    try
    {
        if( execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL )
            {
                while (!pRecordSet->GetEndOfFile())
                {
                    dbNameVar = pRecordSet->GetCollect(L"name");
					if( dbNameVar.bstrVal != NULL )
					{
						dbNames.push_back(W2A(dbNameVar.bstrVal)) ;
						DebugPrintf(SV_LOG_DEBUG, "DB Name :%S\n", dbNameVar.bstrVal);
					}
                    dbNameVar.Clear() ;
                    pRecordSet->MoveNext();
                }
                pRecordSet->Close() ;
                bRet = SVS_OK ;
                DebugPrintf(SV_LOG_DEBUG, "No of databases attached %d\n", dbNames.size()) ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to determine the dbnames\n") ;
        }
    }
    catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR SQL2008Ado::discoverDatabase(const std::string& dbName, MSSQLDBMetaData& database)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    std::stringstream stream;
    database.errCode = DISCOVERY_SUCCESS;
    _bstr_t query ;
    query = "SELECT physical_name FROM  " ;
    query += "\"";
    query += dbName.c_str() ;
    query += "\"";
    query += ".sys.database_files;" ;

    USES_CONVERSION ;
    _RecordsetPtr pRecordSet = NULL ;
    _variant_t dbFileVar ; 
    database.m_dbFiles.clear() ;
    database.m_dbOnline = false ;
    database.m_dbName = dbName ;
    try
    {
        if( execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL )
            {
                while (!pRecordSet->GetEndOfFile())
                {
                    dbFileVar = pRecordSet->GetCollect(L"physical_name");
					if( dbFileVar.bstrVal != NULL )
					{
						database.m_dbFiles.push_back(W2A(dbFileVar.bstrVal)) ;
						DebugPrintf(SV_LOG_DEBUG, "db file name :%S\n", dbFileVar.bstrVal);
					}
                    dbFileVar.Clear() ;
                    pRecordSet->MoveNext();
                }
                pRecordSet->Close() ;
                bRet = SVS_OK ;
                database.m_dbOnline = true ;
                stream << "DB : " << dbName << " status is ONLINE";
                DebugPrintf(SV_LOG_DEBUG, "No of database file names %d\n", database.m_dbFiles.size()) ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to determine the database file names\n") ;
            stream << "Unable to determine the database file names";
            database.errCode = DISCOVERY_FAIL;
        }
    }

   catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
        database.errCode = DISCOVERY_FAIL;
        stream << "Got exception while querying for dbname "  << ce.Description().GetBSTR();
    }    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    database.errorString = stream.str();
    return bRet ;
}


SVERROR SQL2012Ado::connect(const std::string sqlInstance, const std::string& dbName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _bstr_t strCon = "Provider=SQLNCLI11;" ;
    strCon += "Server=" ;
    strCon += sqlInstance.c_str() ;
    strCon += ";" ;
    strCon += "Database=" ;
    strCon += dbName.c_str() ;
    strCon += ";" ;
    strCon += "Trusted_Connection=yes" ;
    setConnStr(strCon) ;
    if( SQLAdo::connect() != SVS_OK )
    {
        DebugPrintf(SV_LOG_WARNING, "Failed to Connect to SQL 2012 database\n") ;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Successfully Connected to SQL 2012 database\n") ;
        bRet = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR SQL2012Ado::getDbNames(std::list<std::string>& dbNames)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _bstr_t query ;
    query = "SELECT name FROM master.dbo.sysdatabases" ;
    USES_CONVERSION ;
    _RecordsetPtr pRecordSet = NULL ;
    _variant_t dbNameVar ; 
    try
    {
        if( execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL )
            {
                while (!pRecordSet->GetEndOfFile())
                {
                    dbNameVar = pRecordSet->GetCollect(L"name");
					if( dbNameVar.bstrVal != NULL )
					{
						dbNames.push_back(W2A(dbNameVar.bstrVal)) ;
						DebugPrintf(SV_LOG_DEBUG, "DB Name :%S\n", dbNameVar.bstrVal);
					}
                    dbNameVar.Clear() ;
                    pRecordSet->MoveNext();
                }
                pRecordSet->Close() ;
                bRet = SVS_OK ;
                DebugPrintf(SV_LOG_DEBUG, "No of databases attached %d\n", dbNames.size()) ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to determine the dbnames\n") ;
        }
    }
    catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
SVERROR SQL2012Ado::discoverDatabase(const std::string& dbName, MSSQLDBMetaData& database)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    std::stringstream stream;
    database.errCode = DISCOVERY_SUCCESS;
    _bstr_t query ;
    query = "SELECT physical_name FROM  " ;
    query += "\"";
    query += dbName.c_str() ;
    query += "\"";
    query += ".sys.database_files;" ;

    USES_CONVERSION ;
    _RecordsetPtr pRecordSet = NULL ;
    _variant_t dbFileVar ; 
    database.m_dbFiles.clear() ;
    database.m_dbOnline = false ;
    database.m_dbName = dbName ;
    try
    {
        if( execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL )
            {
                while (!pRecordSet->GetEndOfFile())
                {
                    dbFileVar = pRecordSet->GetCollect(L"physical_name");
					if( dbFileVar.bstrVal != NULL )
					{
						database.m_dbFiles.push_back(W2A(dbFileVar.bstrVal)) ;
						DebugPrintf(SV_LOG_DEBUG, "db file name :%S\n", dbFileVar.bstrVal);
					}
                    dbFileVar.Clear() ;
                    pRecordSet->MoveNext();
                }
                pRecordSet->Close() ;
                bRet = SVS_OK ;
                database.m_dbOnline = true ;
                stream << "DB : " << dbName << " status is ONLINE";
                DebugPrintf(SV_LOG_DEBUG, "No of database file names %d\n", database.m_dbFiles.size()) ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to determine the database file names\n") ;
            stream << "Unable to determine the database file names";
            database.errCode = DISCOVERY_FAIL;
        }
    }

catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
        database.errCode = DISCOVERY_FAIL;
        stream << "Got exception while querying for dbname "  << ce.Description().GetBSTR();
    }    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    database.errorString = stream.str();
    return bRet ;
}
void buildServerPropertiesCmd(InmProtectedAppType type, _bstr_t& query, std::list<std::string>& colNames)
{

    query = "SELECT " ;
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('Collation')) AS Collation, ";
    if( type != INM_APP_MSSQL2000 )
    {
        query += "CONVERT(nvarchar(256), SERVERPROPERTY('BuildClrVersion')) AS 'Build CLR Version', " ;
    }
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('CollationId')) AS CollationId, ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('ComputerNamePhysicalNetBIOS')) AS 'Computer Name Physical Net BIOS', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('ComparisonStyle')) AS 'Comparison Style', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('Edition')) AS Edition, ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('EditionID')) AS 'Edition Id', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('Engine Edition')) as 'Engine Edition', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('InstanceName')) AS 'Instance Name', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('IsClustered')) AS 'Is Clustered', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('IsFullTextInstalled')) AS 'Is FullText Installed', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('IsIntegratedSecurityOnly')) AS 'Is Integrated Security Only', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('IsSingleUser')) AS 'Is Single User', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('IsSyncWithBackup')) AS 'Is Sync With Backup', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('LCID')) AS 'LCID', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('LicenseType')) AS 'License Type', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('MachineName')) AS 'Machine Name', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('NumLicenses')) AS 'Num Licenses', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('ProcessID')) AS 'Process ID', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('ProductVersion')) AS 'Product Version', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('ProductLevel')) AS 'Product Level', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('ResourceLastUpdateDateTime')) AS 'Resource Last Update Date Time', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('ResourceVersion')) AS 'Resource Version',  ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('FilestreamShareName')) AS 'File Stream Share Name',  ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('FilestreamConfiguredLevel')) AS 'File Stream Configured Level',  ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('FilestreamEffectiveLevel')) AS 'File Stream Effective Level', ";
    query += "CONVERT(nvarchar(256), SERVERPROPERTY('ServerName')) AS 'Server Name' " ;;
    colNames.push_back("Collation") ;
    colNames.push_back("Build CLR Version") ;
    colNames.push_back("Computer Name Physical Net BIOS") ;
    colNames.push_back("CollationId") ;
    colNames.push_back("Comparison Style") ;
    colNames.push_back("Edition") ;
    colNames.push_back("Edition Id") ;
    colNames.push_back("Engine Edition") ;
    colNames.push_back("Instance Name") ;
    colNames.push_back("Is Clustered") ;
    colNames.push_back("Is FullText Installed") ;
    colNames.push_back("Is Integrated Security Only") ;
    colNames.push_back("Is Single User") ;
    colNames.push_back("Is Sync With Backup") ;
    colNames.push_back("LCID") ;
    colNames.push_back("License Type") ;
    colNames.push_back("Machine Name") ;
    colNames.push_back("Num Licenses") ;
    colNames.push_back("Process ID") ;
    colNames.push_back("Product Version") ;
    colNames.push_back("Product Level") ;
    colNames.push_back("Resource Last Update Date Time") ;
    colNames.push_back("Resource Version") ;
    colNames.push_back("File Stream Share Name") ;
    colNames.push_back("File Stream Configured Level") ;
    colNames.push_back("File Stream Effective Level") ;
    colNames.push_back("Server Name") ;
}

void builddbPropertiesCmd(InmProtectedAppType type, std::string dbname, _bstr_t& query, std::list<std::string>& colNames)
{
    std::stringstream stream ;
    stream << "SELECT " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'Collation')) AS 'Collation' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'ComparisonStyle')) AS 'Comparison Style' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsAnsiNullDefault')) AS 'Is Ansi Null Default' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsAnsiNullsEnabled')) AS 'Is Ansi Nulls Enabled' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsAnsiPaddingEnabled')) AS 'Is Ansi Padding Enabled' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsAnsiWarningsEnabled')) AS 'Is Ansi Warnings Enabled' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsArithmeticAbortEnabled')) AS 'Is Arithmetic Abort Enabled' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsAutoClose')) AS 'Is Auto Close' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsAutoCreateStatistics')) AS 'Is Auto Create Statistics' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsAutoShrink')) AS 'Is Auto Shrink' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsAutoUpdateStatistics')) AS 'Is Auto Update Statistics' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsCloseCursorsOnCommitEnabled')) AS 'Is Close Cursors On CommitEnabled' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsFulltextEnabled')) AS 'Is Full text Enabled' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsInStandBy')) AS 'Is In StandBy' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsLocalCursorsDefault')) AS 'Is Local Cursors Default' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsMergePublished')) AS 'Is Merge Published' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsNullConcat')) AS 'Is Null Concat' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsNumericRoundAbortEnabled')) AS 'Is Numeric Round Abort Enabled' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsPublished')) AS 'Is Published' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsQuotedIdentifiersEnabled')) AS 'Is Quoted Identifiers Enabled' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsRecursiveTriggersEnabled')) AS 'Is Recursive Triggers Enabled' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsSubscribed')) AS 'Is Subscribed' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'IsTornPageDetectionEnabled')) AS 'Is Torn Page Detection Enabled' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'LCID')) AS 'LCID' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'Recovery')) AS 'Recovery' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'SQLSortOrder')) AS 'SQL SortOrder' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'Status')) AS 'Status' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'Updateability')) AS 'Updateability' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'UserAccess')) AS 'User Access' , " ;
    stream << "CONVERT(nvarchar(256), DATABASEPROPERTYEX('" << dbname << "' , 'Version')) AS 'Version'" ;
    query = stream.str().c_str() ;
    colNames.push_back("Collation") ;
    colNames.push_back("Comparison Style") ;
    colNames.push_back("Is Ansi Null Default") ;
    colNames.push_back("Is Ansi Nulls Enabled") ;
    colNames.push_back("Is Ansi Padding Enabled") ;
    colNames.push_back("Is Ansi Warnings Enabled") ;
    colNames.push_back("Is Arithmetic Abort Enabled") ;
    colNames.push_back("Is Auto Close") ;
    colNames.push_back("Is Auto Create Statistics") ;
    colNames.push_back("Is Auto Shrink") ;
    colNames.push_back("Is Auto Update Statistics") ;
    colNames.push_back("Is Close Cursors On CommitEnabled") ;
    colNames.push_back("Is Full text Enabled") ;
    colNames.push_back("Is In StandBy") ;
    colNames.push_back("Is Local Cursors Default") ;
    colNames.push_back("Is Merge Published") ;
    colNames.push_back("Is Null Concat") ;
    colNames.push_back("Is Numeric Round Abort Enabled") ;
    colNames.push_back("Is Published") ;
    colNames.push_back("Is Quoted Identifiers Enabled") ;
    colNames.push_back("Is Recursive Triggers Enabled") ;
    colNames.push_back("Is Subscribed") ;
    colNames.push_back("Is Torn Page Detection Enabled") ;
    colNames.push_back("LCID") ;
    colNames.push_back("Recovery") ;
    colNames.push_back("SQL SortOrder") ;
    colNames.push_back("Status") ;
    colNames.push_back("Updateability") ;
    colNames.push_back("User Access") ;
    colNames.push_back("Version") ;
}
SVERROR SQL2005Ado::getServerProperties(std::map<std::string, std::string>& propertyMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _RecordsetPtr pRecordSet = NULL ;
    std::string value ;
    _bstr_t query ;
    std::list<std::string> colNames ;
    USES_CONVERSION ;
    buildServerPropertiesCmd(INM_APP_MSSQL2005, query, colNames) ;

    std::list<std::string>::const_iterator colNameIter = colNames.begin();
    try
    {
        if( execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL )
            {
                while( colNameIter != colNames.end())
                {
					if( (pRecordSet->GetCollect((*colNameIter).c_str())).bstrVal != NULL )
					{
						value = W2A((pRecordSet->GetCollect((*colNameIter).c_str())).bstrVal);
						if( value.compare("NULL") != 0  && value.length() != 0 )
						{
							DebugPrintf(SV_LOG_DEBUG, "Property Name : %s\n", colNameIter->c_str()) ;
							DebugPrintf(SV_LOG_DEBUG, "Value : %s\n", value.c_str()) ;
							propertyMap.insert(std::make_pair(*colNameIter, value)) ;
						}
						else
						{
							DebugPrintf(SV_LOG_DEBUG, "The value for server property %s is null\n", *colNameIter) ;
						}
					}
                    colNameIter++ ;
                }
                pRecordSet->Close() ;
                bRet = SVS_OK ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to determine the sever properties\n") ;
        }
    }
    catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


SVERROR SQL2008Ado::getServerProperties(std::map<std::string, std::string>& propertyMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _RecordsetPtr pRecordSet = NULL ;
    std::string value ;
    _bstr_t query ;
    std::list<std::string> colNames ;
    USES_CONVERSION ;
    buildServerPropertiesCmd(INM_APP_MSSQL2008, query, colNames) ;

    std::list<std::string>::const_iterator colNameIter = colNames.begin();
    try
    {
        if( execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL )
            {
                while( colNameIter != colNames.end() )
                {
					if( (pRecordSet->GetCollect((*colNameIter).c_str())).bstrVal != NULL )
					{
						value = W2A((pRecordSet->GetCollect((*colNameIter).c_str())).bstrVal);
						if( value.compare("NULL") != 0 && value.length() != 0 )
						{
							DebugPrintf(SV_LOG_DEBUG, "Property Name : %s\n", colNameIter->c_str()) ;
							DebugPrintf(SV_LOG_DEBUG, "Value : %s\n", value.c_str()) ;
							propertyMap.insert(std::make_pair(*colNameIter, value)) ;
						}
						else
						{
							DebugPrintf(SV_LOG_DEBUG, "The value for server property %s is null\n", *colNameIter) ;
						}
					}
                    colNameIter++ ;
                }
                pRecordSet->Close() ;
                bRet = SVS_OK ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to determine the sever properties\n") ;
        }
    }
    catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


SVERROR SQL2012Ado::getServerProperties(std::map<std::string, std::string>& propertyMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _RecordsetPtr pRecordSet = NULL ;
    std::string value ;
    _bstr_t query ;
    std::list<std::string> colNames ;
    USES_CONVERSION ;
    buildServerPropertiesCmd(INM_APP_MSSQL2012, query, colNames) ;

    std::list<std::string>::const_iterator colNameIter = colNames.begin();
    try
    {
        if( execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL )
            {
                while( colNameIter != colNames.end() )
                {
					if( (pRecordSet->GetCollect((*colNameIter).c_str())).bstrVal != NULL )
					{
						value = W2A((pRecordSet->GetCollect((*colNameIter).c_str())).bstrVal);
						if( value.compare("NULL") != 0 && value.length() != 0 )
						{
							DebugPrintf(SV_LOG_DEBUG, "Property Name : %s\n", colNameIter->c_str()) ;
							DebugPrintf(SV_LOG_DEBUG, "Value : %s\n", value.c_str()) ;
							propertyMap.insert(std::make_pair(*colNameIter, value)) ;
						}
						else
						{
							DebugPrintf(SV_LOG_DEBUG, "The value for server property %s is null\n", *colNameIter) ;
						}
					}
                    colNameIter++ ;
                }
                pRecordSet->Close() ;
                bRet = SVS_OK ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to determine the sever properties\n") ;
        }
    }
    catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}





SVERROR SQL2000Ado::getServerProperties(std::map<std::string, std::string>& propertyMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _RecordsetPtr pRecordSet = NULL ;
    std::string value ;
    _bstr_t query ;
    std::list<std::string> colNames ;
    USES_CONVERSION ;
    buildServerPropertiesCmd(INM_APP_MSSQL2000, query, colNames) ;

    std::list<std::string>::const_iterator colNameIter = colNames.begin();
    try
    {
        if( execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL )
            {
                while( colNameIter != colNames.end() )
                {
					if( (pRecordSet->GetCollect((*colNameIter).c_str())).bstrVal != NULL )
					{
						value = W2A((pRecordSet->GetCollect((*colNameIter).c_str())).bstrVal) ;
						if( value.compare("NULL") != 0 && value.length() != 0 )
						{
							DebugPrintf(SV_LOG_DEBUG, "Property Name : %s\n", colNameIter->c_str()) ;
							DebugPrintf(SV_LOG_DEBUG, "Value : %s\n", value.c_str()) ;
							propertyMap.insert(std::make_pair(*colNameIter, value)) ;
						}
						else
						{
							DebugPrintf(SV_LOG_DEBUG, "The value for server property %s is null\n", *colNameIter) ;
						}
					}
                    colNameIter++ ;
                }
                pRecordSet->Close() ;
                bRet = SVS_OK ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to determine the sever properties\n") ;
        }
    }
    catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR SQL2000Ado::getDbProperties(std::string dbName, std::map<std::string, std::string>& dbPropertiesMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _RecordsetPtr pRecordSet = NULL ;
    std::string value ;
    _bstr_t query ;
    std::list<std::string> colNames ;
    USES_CONVERSION ;
    builddbPropertiesCmd(INM_APP_MSSQL2000, dbName, query, colNames) ;

    std::list<std::string>::const_iterator colNameIter = colNames.begin();
    try
    {
        if( execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL )
            {
                while( colNameIter != colNames.end())
                {
					if( (pRecordSet->GetCollect((*colNameIter).c_str())).bstrVal != NULL )
					{
						value = W2A((pRecordSet->GetCollect((*colNameIter).c_str())).bstrVal);
						if( value.compare("NULL") != 0  && value.length() != 0 )
						{
							DebugPrintf(SV_LOG_DEBUG, "Property Name : %s\n", colNameIter->c_str()) ;
							DebugPrintf(SV_LOG_DEBUG, "Value : %s\n", value.c_str()) ;
							dbPropertiesMap.insert(std::make_pair(*colNameIter, value)) ;
						}
						else
						{
							DebugPrintf(SV_LOG_DEBUG, "The value for server property %s is null\n", *colNameIter) ;
						}
					}
                    colNameIter++ ;
                }
                pRecordSet->Close() ;
                bRet = SVS_OK ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to determine the database properties\n") ;
        }
    }
    catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR SQL2005Ado::getDbProperties(std::string dbName, std::map<std::string, std::string>& dbPropertiesMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _RecordsetPtr pRecordSet = NULL ;
    std::string value ;
    _bstr_t query ;
    std::list<std::string> colNames ;
    USES_CONVERSION ;
    builddbPropertiesCmd(INM_APP_MSSQL2005, dbName, query, colNames) ;

    std::list<std::string>::const_iterator colNameIter = colNames.begin();
    try
    {
        if( execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL )
            {
                while( colNameIter != colNames.end())
                {
					if( (pRecordSet->GetCollect((*colNameIter).c_str())).bstrVal != NULL )
					{
						value = W2A((pRecordSet->GetCollect((*colNameIter).c_str())).bstrVal) ;
						if( value.compare("NULL") != 0  && value.length() != 0 )
						{
							DebugPrintf(SV_LOG_DEBUG, "Property Name : %s\n", colNameIter->c_str()) ;
							DebugPrintf(SV_LOG_DEBUG, "Value : %s\n", value.c_str()) ;
							dbPropertiesMap.insert(std::make_pair(*colNameIter, value)) ;
						}
						else
						{
							DebugPrintf(SV_LOG_DEBUG, "The value for server property %s is null\n", *colNameIter) ;
						}
					}
                    colNameIter++ ;
                }
                pRecordSet->Close() ;
                bRet = SVS_OK ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to determine the database properties\n") ;
        }
    }
    catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR SQL2008Ado::getDbProperties(std::string dbName, std::map<std::string, std::string>& dbPropetiesMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _RecordsetPtr pRecordSet = NULL ;
    std::string value ;
    _bstr_t query ;
    std::list<std::string> colNames ;
    USES_CONVERSION ;
    builddbPropertiesCmd(INM_APP_MSSQL2008, dbName, query, colNames) ;

    std::list<std::string>::const_iterator colNameIter = colNames.begin();
    try
    {
        if( execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL )
            {
                while( colNameIter != colNames.end())
                {
					if( (pRecordSet->GetCollect((*colNameIter).c_str())).bstrVal != NULL )
					{
						value = W2A((pRecordSet->GetCollect((*colNameIter).c_str())).bstrVal);
						if( value.compare("NULL") != 0  && value.length() != 0 )
						{
							DebugPrintf(SV_LOG_DEBUG, "Property Name : %s\n", colNameIter->c_str()) ;
							DebugPrintf(SV_LOG_DEBUG, "Value : %s\n", value.c_str()) ;
							dbPropetiesMap.insert(std::make_pair(*colNameIter, value)) ;
						}
						else
						{
							DebugPrintf(SV_LOG_DEBUG, "The value for server property %s is null\n", *colNameIter) ;
						}
					}
                    colNameIter++ ;
                }
                pRecordSet->Close() ;
                bRet = SVS_OK ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to determine the database properties\n") ;
        }
    }
    catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


SVERROR SQL2012Ado::getDbProperties(std::string dbName, std::map<std::string, std::string>& dbPropetiesMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _RecordsetPtr pRecordSet = NULL ;
    std::string value ;
    _bstr_t query ;
    std::list<std::string> colNames ;
    USES_CONVERSION ;
    builddbPropertiesCmd(INM_APP_MSSQL2012, dbName, query, colNames) ;

    std::list<std::string>::const_iterator colNameIter = colNames.begin();
    try
    {
        if( execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL )
            {
                while( colNameIter != colNames.end())
                {
					if( (pRecordSet->GetCollect((*colNameIter).c_str())).bstrVal != NULL )
					{
						value = W2A((pRecordSet->GetCollect((*colNameIter).c_str())).bstrVal);
						if( value.compare("NULL") != 0  && value.length() != 0 )
						{
							DebugPrintf(SV_LOG_DEBUG, "Property Name : %s\n", colNameIter->c_str()) ;
							DebugPrintf(SV_LOG_DEBUG, "Value : %s\n", value.c_str()) ;
							dbPropetiesMap.insert(std::make_pair(*colNameIter, value)) ;
						}
						else
						{
							DebugPrintf(SV_LOG_DEBUG, "The value for server property %s is null\n", *colNameIter) ;
						}
					}
                    colNameIter++ ;
                }
                pRecordSet->Close() ;
                bRet = SVS_OK ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to determine the database properties\n") ;
        }
    }
    catch(_com_error ce)
    {
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}




SVERROR getMailBoxServerNameProperty(SQLAdo* SQLAdoImplPtr, std::string queryString, std::list<std::string>& mailBoxServerNameList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    _bstr_t query  = queryString.c_str();
    USES_CONVERSION ;
    _RecordsetPtr pRecordSet = NULL ;
    _variant_t valueVar ; 
    try{
        if( SQLAdoImplPtr->execute(query, pRecordSet) == SVS_OK )
        {
            if( pRecordSet != NULL && !pRecordSet->GetEndOfFile() )
            {
                while (!pRecordSet->GetEndOfFile())
                {
                    valueVar.Clear() ;
                    valueVar = pRecordSet->GetCollect(L"ServerDN");
                    if( valueVar.bstrVal != NULL )
                    {
                        std::string serverDN;
						if( valueVar.bstrVal != NULL )
						{
							serverDN = W2A(valueVar.bstrVal);
						}
                        DebugPrintf(SV_LOG_DEBUG, "ServerDN: %S\n", serverDN.c_str());
                        std::string searchString1 = "cn=Servers/cn=";
                        std::string searchString2 = "/cn";
                        std::string::size_type index1 = serverDN.find(searchString1.c_str());
                        if(index1 != std::string::npos)
                        {
                            std::string::size_type firstPos = index1+searchString1.size();
                            serverDN = serverDN.substr(firstPos);

                            index1 = serverDN.find(searchString2.c_str());
                            if(index1 != std::string::npos)
                            {
                                serverDN = serverDN.substr(0, index1);
                                if( isfound(mailBoxServerNameList, serverDN) == false )
                                {
                                    mailBoxServerNameList.push_back(serverDN);
                                }
                            }
                        }
                    }
                    valueVar.bstrVal = NULL;    
                    pRecordSet->MoveNext();
                }
                pRecordSet->Close() ;
                bRet = SVS_OK ;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "RecordSet is Empty for the cmd --> %s \n", queryString.c_str()) ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "execute Failed..\n") ;
        }
    }
    catch(_com_error ce){
        DebugPrintf(SV_LOG_ERROR, "Com Error %S\n", ce.Description().GetBSTR()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void PrintComError(_com_error &e) {
   _bstr_t bstrSource(e.Source());
   _bstr_t bstrDescription(e.Description());

   // Print Com errors.  
   DebugPrintf("Error Code = %08lx\n", e.Error());
   DebugPrintf("\tCode meaning = %s\n", e.ErrorMessage());
   DebugPrintf("\tSource = %s\n", (LPCSTR) bstrSource);
   DebugPrintf("\tDescription = %s\n", (LPCSTR) bstrDescription);
}
