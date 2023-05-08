#include "appglobals.h"
#include "cxupdatecache.h"
#include <sstream>
#include <boost/lexical_cast.hpp>

CxUpdateCachePtr CxUpdateCache::getInstance()
{
	static CxUpdateCachePtr cxUpdateCachePtr;
    if( cxUpdateCachePtr.get() == NULL )
    {
        cxUpdateCachePtr.reset(new SqliteCxUpdateCache) ;
    }
    return cxUpdateCachePtr ;
}

void SqliteCxUpdateCache::Init(const std::string& sDbpath)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	m_con.setDbPath(sDbpath) ;
	m_con.open() ;
	CreateCxUpdateInfoTable();
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void SqliteCxUpdateCache::CreateCxUpdateInfoTable()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	std::stringstream select_query, create_query ;

	select_query << " SELECT name FROM sqlite_master WHERE type=\'table\' AND name=\'t_CxUpdateInfo\' ;" ;
	
	create_query << " CREATE TABLE t_CxUpdateInfo  " ;
    create_query << "(\"c_SequenceNO\" INTEGER PRIMARY KEY AUTOINCREMENT , " ;
	create_query << "\"c_PolicyId\" INTEGER NOT NULL , " ;
    create_query << "\"c_InstanceId\" INTEGER NOT NULL , " ;
	create_query << "\"c_CxUpdateString\" TEXT NOT NULL, "; 
	create_query << "\"c_Status\" INTEGER NOT NULL , ";
    create_query <<"\"c_Retries\" INTEGER NOT NULL) ;"; 
	m_con.execQuery(select_query.str(), set) ;
	if( set.getSize() == 0 )
	{
		m_con.execQuery(create_query.str(), set) ;
		std::stringstream indexing_query ;
		indexing_query << "CREATE INDEX masterIdx1 ON t_CxUpdateInfo ( c_SequenceNO ) " ;
		m_con.execQuery(indexing_query.str(), set ) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteCxUpdateCache::GetPendingUpdates(std::list<CxRequestCache>& cxRequestList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	std::list<CxRequestCache>::iterator iter = cxRequestList.begin() ;
	std::stringstream query ;
	DBResultSet set ;
	query.str("") ;
	query << "SELECT " ;
    query << "c_SequenceNO , ";
	query << "c_PolicyId , " ;
    query << "c_InstanceId , " ;
	query << "c_CxUpdateString , " ;
	query << "c_Status , " ;
    query << "c_Retries " ;
	query << "FROM t_CxUpdateInfo " ;
	query << "WHERE  " ;
	query << "c_Status != " << CX_UPDATE_NOT_PENDING << " AND "; 
    query << "c_Status != " << CX_UPDATE_SUCCESSFUL  << " ORDER BY c_SequenceNO ASC;"; 
	m_con.execQuery(query.str(), set) ;
	for(size_t rowIdx = 0 ; rowIdx < set.getSize() ; rowIdx++ )
	{
        DBRow row = set.nextRow();
        CxRequestCache requestObj;
        requestObj.sequneceNumber = boost::lexical_cast<SV_ULONG>(row.nextVal().getString());
        requestObj.policyId = boost::lexical_cast<SV_ULONG>(row.nextVal().getString());
        requestObj.instanceId = boost::lexical_cast<SV_ULONGLONG>(row.nextVal().getString());
	    requestObj.updateString = row.nextVal().getString();
	    requestObj.status = boost::lexical_cast<SV_ULONG>(row.nextVal().getString());
        requestObj.noOfRetries = boost::lexical_cast<SV_ULONG>(row.nextVal().getString());
           cxRequestList.push_back(requestObj) ; 
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void EscapeString( std::string& tempString )
{
    std::string itemString = "'" ;
    std::string::size_type leftIndex =  tempString.find_first_of( itemString, 0 ) ;
            
    while( leftIndex != std::string::npos )
    {
        char found = tempString.at( leftIndex ) ;
        if( found == '\n' ||found == '\r' || found == '\t' )
            tempString.insert( leftIndex, "\\" ) ;
        else 
            tempString.insert( leftIndex, "\'" ) ;
        if( leftIndex + 2 != std::string::npos )
            leftIndex = tempString.find_first_of( itemString, leftIndex + 2 ) ;
        else
            break ;
    }
    return ;
}

void SqliteCxUpdateCache::AddCxUpdateRequest(CxRequestCache& update)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	DBRow row ;
	std::stringstream insert_query,select_query ;
    std::string updateString ;
    updateString = update.updateString ;
    EscapeString( updateString ) ;
    DebugPrintf(SV_LOG_DEBUG, "Update String %s\n", updateString .c_str() ) ;
	insert_query << "INSERT INTO t_CxUpdateInfo " ;
	insert_query << "( c_PolicyId ,c_InstanceId,c_CxUpdateString,c_Status,c_Retries) " ;
	insert_query << " VALUES ( " << update.policyId << " , " ;
    insert_query << update.instanceId << " , " ;
	insert_query << "'"<< updateString << "'"<< ", " ;
    insert_query << update.status << " , " ;
	insert_query << update.noOfRetries << " ) ; " ;
	
    select_query << "SELECT c_SequenceNO FROM t_CxUpdateInfo ";
    select_query << " WHERE c_PolicyId = " << update.policyId << " ORDER BY c_SequenceNO DESC; ";

	m_con.execQuery( insert_query.str(), set ) ;
	m_con.execQuery( select_query.str(), set ) ;

	if( set.getSize() != 0 )
	{
        DBRow row = set.nextRow();
        update.sequneceNumber = boost::lexical_cast<SV_ULONG>(row.nextVal().getString());
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteCxUpdateCache::GetUpdateStatus(SV_ULONG policyId,CxRequestCache& cxRequest)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	std::stringstream  select_query ;

	select_query << "SELECT c_PolicyId,c_InstanceId,c_CxUpdateString,c_Status FROM t_CxUpdateInfo " ;
	select_query << " WHERE c_PolicyId = " << policyId << ";" ;
	
	m_con.execQuery(select_query.str(), set) ; 
	if( set.getSize() != 0 )
	{
		DBRow row = set.nextRow();	
        cxRequest.policyId = boost::lexical_cast<SV_ULONG>(row.nextVal().getString());
        cxRequest.instanceId = boost::lexical_cast<SV_ULONGLONG>(row.nextVal().getString());
		cxRequest.updateString = row.nextVal().getString();
	    cxRequest.status = boost::lexical_cast<SV_ULONG>(row.nextVal().getString());
	}
	else
	{
		cxRequest.status = CX_UPDATE_NOT_PENDING ;
		cxRequest.policyId = policyId ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteCxUpdateCache::DeleteFromUpdateCache(SV_ULONG policyId)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	std::stringstream query ;
	DBResultSet set ;	
	
	query << "DELETE FROM t_CxUpdateInfo WHERE " ;
	query << " c_PolicyId = " << policyId << ";" ;
	m_con.execQuery(query.str(), set) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void SqliteCxUpdateCache::UpdateCxUpdateRequest(const CxRequestCache& update)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	std::stringstream query ;
	if(update.status == CX_UPDATE_SUCCESSFUL)
	{
		query << "DELETE FROM t_CxUpdateInfo WHERE ";
		query << "c_SequenceNO = "<< update.sequneceNumber;
	}
	else
	{
		query << "UPDATE t_CxUpdateInfo SET c_Status = " << update.status ;
		query << " WHERE c_SequenceNO = " << update.sequneceNumber << ";" ;
	}
    m_con.execQuery(query.str(), set ) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;

}

void SqliteCxUpdateCache::AddCxUpdateRequestEx(CxRequestCache & update)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DBResultSet set ;
	std::stringstream delete_query ;

	delete_query << "DELETE FROM t_CxUpdateInfo WHERE " << " c_PolicyId = " << update.policyId << ";" ;
	m_con.execQuery(delete_query.str(), set);
	
	AddCxUpdateRequest(update);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}
