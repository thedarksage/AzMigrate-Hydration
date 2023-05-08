#ifndef PERSIST_STORE_H
#define PERSIST_STORE_H
#include "ace/OS_NS_sys_stat.h"
#include <string>
#include <list>
#include <map>
#include "connection.h"
#include "svtypes.h"

#define CX_UPDATE_NOT_PENDING   0
#define CX_UPDATE_PENDING       1
#define CX_UPDATE_FAILED        2
#define CX_UPDATE_SUCCESSFUL    3
#define CX_UPDATE_PENDING_NOLOGUPLOAD	4


//enum InmCxUpdateState
//{
//	CX_UPDATE_NOT_PENDING,
//	CX_UPDATE_PENDING,
//	CX_UPDATE_FAILED,
//	CX_UPDATE_SUCCESSFUL
//};
struct CxRequestCache
{
	SV_ULONG sequneceNumber;
	SV_ULONG policyId;
    SV_ULONGLONG instanceId;
	std::string updateString;
	SV_ULONG status;
    SV_ULONG noOfRetries;
};
class CxUpdateCache ;
typedef boost::shared_ptr<CxUpdateCache> CxUpdateCachePtr ;

class CxUpdateCache
{
public:
	virtual void Init(const std::string& dbPath) = 0 ;
	virtual void AddCxUpdateRequest(CxRequestCache&) = 0 ;
	virtual void AddCxUpdateRequestEx(CxRequestCache&) = 0 ;
	virtual void GetPendingUpdates(std::list<CxRequestCache>&) = 0 ;
	virtual void GetUpdateStatus(SV_ULONG,CxRequestCache&) = 0 ;
	virtual void DeleteFromUpdateCache(SV_ULONG policyId) = 0;
    virtual void UpdateCxUpdateRequest(const CxRequestCache&) = 0 ;
	static CxUpdateCachePtr getInstance();
};
typedef boost::shared_ptr<CxUpdateCache> CxUpdateCachePtr ;

class SqliteCxUpdateCache : public CxUpdateCache
{
	void CreateCxUpdateInfoTable();
public:
	SqliteConnection m_con;
	void Init(const std::string& dbPath);
	void AddCxUpdateRequest(CxRequestCache&);
	void AddCxUpdateRequestEx(CxRequestCache&);
	void GetPendingUpdates(std::list<CxRequestCache>&);
	void GetUpdateStatus(SV_ULONG,CxRequestCache&) ;
	void DeleteFromUpdateCache(SV_ULONG policyId);
    void UpdateCxUpdateRequest(const CxRequestCache&);
};
typedef boost::shared_ptr<SqliteCxUpdateCache> SqliteCxUpdateCachePtr ;

#endif
