#ifndef __MYSQL_CONNECTION_H_
#define __MYSQL_CONNECTION_H_

#include "TypedValue.h"
#include "Connection.h"
#include <stdio.h>
#include "util.h"
#include "Log.h"
#include "common.h"
#include <cstring>
#include <stdlib.h>
#include "securityutils.h"


#define	INM_SQL_QUERY_TYPE_SELECT	 "SELECT"
#define	INM_SQL_QUERY_TYPE_UPDATE	 "UPDATE"
#define	INM_SQL_QUERY_TYPE_INSERT	 "INSERT"
#define	INM_SQL_QUERY_TYPE_DELETE	 "DELETE"
#define	INM_SQL_QUERY_TYPE_PROCEDURE "PROCEDURE"


#define	CX_E_COMM_INVALID_FORMAT		-1
#define	CX_E_COMM_AUTHE_ERROR			-2
#define	CX_E_COMM_AUTHO_ERROR			-3
#define	CX_E_COMM_FUNC_NOT_SUPPORTED	-4
#define	CX_E_COMM_INT_SRV_ERROR			-5
#define	CX_E_COMM_UNKOWN				-6
#define	CX_E_COMM_ACCESS_KEY_NOT_FOUND	-7
#define	CX_E_COMM_HTTP_ERROR			-10
#define	CX_E_XML_PARSE_ERROR			-11
#define	CX_E_SUCCESS					 0
#define	CX_E_PARAM_MISSING				 1
#define	CX_E_NO_DATA					 3
#define	CX_E_XML_RESP_FORMAT_ERROR		 10
#define	CX_E_DB_CONN_ERROR				 3001
#define	CX_E_QUERY_EXEC_ERROR			 3002
#define	CX_E_TRAN_ERROR					 3003


class CXConnection: public Connection
{
		
        int m_InmErrorCode;
public:
        
        CXConnection()
         {
         	 m_InmErrorCode = 0;
         }
  

	string GetQueryType(char char_query)
	{
		switch(char_query)
		{
		case 'u':
		case 'U': return INM_SQL_QUERY_TYPE_UPDATE;
		case 'i':
		case 'I': return INM_SQL_QUERY_TYPE_INSERT;
		case 'd':
		case 'D': return INM_SQL_QUERY_TYPE_DELETE;
		case 's':
		case 'S': return INM_SQL_QUERY_TYPE_SELECT;
		}
		return "";
	}
	int close()
	{
		return 0;
	}
	
	DBResultSet execQuery(const char* InmszSQL)
	{
		return execQuery(InmszSQL,false);
	}
	DBResultSet execQuery(const char* szSQL, bool bProcedure)
	{
		DBResultSet lInmrset;

		//1. Construct xml request string
		std::string query_type,xml_request,query(szSQL),xml_response;
		if(bProcedure) {
			query_type = INM_SQL_QUERY_TYPE_PROCEDURE;
		}
		else if(!query.empty()){
			query_type = GetQueryType(query[query.find_first_not_of(" ")]);
		}

		std::string Reqest_id = securitylib::genRandNonce(32,true);
		GenerateXmlRequest(Reqest_id,query_type,query,xml_request);
		//Logger::Log(__FILE__,__LINE__,0,1, "The xml request : %s", xml_request.c_str());
		
		//2. Call PostToCX() to call cx-api
		if(PostToCX(SCH_CONF::GetCxApiUrl(),xml_request,xml_response,SCH_CONF::UseHttps()))
		{
			//3. Parse the xml response from previous call to get the DBResultSet
			//Logger::Log(__FILE__,__LINE__,0,1, "The xml response : %s", xml_response.c_str());
			if(ParseXmlResponse(xml_response,lInmrset,m_InmErrorCode)) {
				Logger::Log(__FILE__,__LINE__,2,1, "Failure in parsing the xml response : [%s]", xml_response.c_str());
				m_InmErrorCode = CX_E_XML_RESP_FORMAT_ERROR;
			} else {
				m_InmErrorCode = CX_E_SUCCESS;
			}
		}
		else
		{
			//Failure in executing the query.
			m_InmErrorCode = -10; //curl/http-server error
			Logger::Log(__FILE__,__LINE__,2,1, "Faile to post the xml request : [%s]", xml_request.c_str());
		}
		
		return lInmrset;
	}
    
    int getLastError() const
    {
        return m_InmErrorCode;
    }

	virtual ~CXConnection()
    { 
    }
};

class CXConnectionEx: public CXConnection
{
    CXConnection *m_InmCon;
    int bState;         //0->close   1->open

public:
    CXConnectionEx()
    {
        m_InmCon = new CXConnection;
        bState = 1;
    }

    int close()
    {
        return 0;
    }

	DBResultSet execQuery(const char* szSQL)
	{
		return execQuery(szSQL,false);
	}

	DBResultSet execQuery(const char* szSQL, bool bProcedure)
    {
        DBResultSet lInmrset;

	    Logger::Log(__FILE__,__LINE__,0,1, "The query is %s", szSQL);

        bState = 1;
		if( m_InmCon )
			lInmrset = m_InmCon->execQuery(szSQL,bProcedure);

        int ErrorCode = m_InmCon->getLastError();
        if(ErrorCode != 0)
        {
            switch(ErrorCode)
            {
			case CX_E_COMM_ACCESS_KEY_NOT_FOUND:
				Logger::Log(__FILE__,__LINE__,2,1,"error: Access key not found in cx-api request\n") ;
                break;
			case CX_E_COMM_AUTHE_ERROR:
				Logger::Log(__FILE__,__LINE__,2,1,"error: CX-API Authentication failed\n") ;
                break;
			case CX_E_COMM_AUTHO_ERROR:
				Logger::Log(__FILE__,__LINE__,2,1,"error: CX-API Authorization failed\n") ;
                break;
			case CX_E_COMM_FUNC_NOT_SUPPORTED:
				Logger::Log(__FILE__,__LINE__,2,1,"error: Call made to not-supported CX-API\n") ;
                break;
			case CX_E_COMM_HTTP_ERROR:
				Logger::Log(__FILE__,__LINE__,2,1,"error: Http communication error\n") ;
                break;
			case CX_E_COMM_INT_SRV_ERROR:
				Logger::Log(__FILE__,__LINE__,2,1,"error: Internal Server error\n") ;
                break;
			case CX_E_COMM_INVALID_FORMAT:
				Logger::Log(__FILE__,__LINE__,2,1,"error: Invalid request format\n") ;
                break;
			case CX_E_COMM_UNKOWN:
				Logger::Log(__FILE__,__LINE__,2,1,"error: Unkown server error\n") ;
                break;
			case CX_E_DB_CONN_ERROR:
				Logger::Log(__FILE__,__LINE__,2,1,"error: DB Connection error\n") ;
                break;
			case CX_E_NO_DATA:
				Logger::Log(__FILE__,__LINE__,2,1,"error: No data found\n") ;
                break;
			case CX_E_PARAM_MISSING:
				Logger::Log(__FILE__,__LINE__,2,1,"error: Parameter missing\n") ;
                break;
			case CX_E_QUERY_EXEC_ERROR:
				Logger::Log(__FILE__,__LINE__,2,1,"error: SQL Query execution error\n") ;
                break;
			case CX_E_TRAN_ERROR:
				Logger::Log(__FILE__,__LINE__,2,1,"error: SQL Transacton error\n") ;
                break;
			case CX_E_XML_PARSE_ERROR:
				Logger::Log(__FILE__,__LINE__,2,1,"error: Response XML Parse error\n") ;
                break;
			case CX_E_XML_RESP_FORMAT_ERROR:
				Logger::Log(__FILE__,__LINE__,2,1,"error: Response XML format error\n") ;
                break;
            default:
				Logger::Log(__FILE__,__LINE__,2,1,"Unknown cx-connection error: %d\n",ErrorCode) ;
                break;
            }
        }
        return lInmrset;
    }

    virtual ~CXConnectionEx()
    { 
		SAFE_DELETE( m_InmCon );
    }
	

	int getLastError() const
	{
		return m_InmCon->getLastError();
	}
};

Connection* createConnection();
#endif
