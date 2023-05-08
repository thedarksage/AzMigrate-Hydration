#ifndef SQLADO_H
#define SQLADO_H
#include "ado.h"
#ifndef _WINDOWS_
#include <Windows.h>
#endif
#include <boost/shared_ptr.hpp>
#include <map>
#include "mssql/mssqlmetadata.h"

using namespace AdoNS;

class SQLAdo : public InmAdo
{
    _bstr_t m_conStr ;
    _ConnectionPtr m_pConn ;    
public:
    virtual ~SQLAdo() ;
    SVERROR init() ;
    SVERROR connect();
    SVERROR execute(_bstr_t query, _RecordsetPtr& recordsetPtr) ;
    _bstr_t getConnStr() ;
    void setConnStr(_bstr_t) ;
    
    virtual SVERROR connect(const std::string sqlInstance, const std::string& dbName) = 0 ;
    virtual SVERROR getDbNames(std::list<std::string>& dbNames) = 0 ;
    virtual SVERROR discoverDatabase(const std::string& dbName, MSSQLDBMetaData&) = 0 ;
    virtual SVERROR getServerProperties(std::map<std::string, std::string>&) = 0 ;
    virtual SVERROR getDbProperties(std::string, std::map<std::string, std::string>&) = 0 ;
    static SQLAdo* getSQLAdoImpl(InmProtectedAppType type) ;
} ;

class SQL2000Ado : public SQLAdo
{
public:
    SVERROR connect(const std::string sqlInstance, const std::string& dbName) ;
    SVERROR getDbNames(std::list<std::string>& dbNames) ;
    SVERROR discoverDatabase(const std::string& dbName, MSSQLDBMetaData&) ;
    SVERROR getDbProperties(std::string, std::map<std::string, std::string>&) ;
    SVERROR getServerProperties(std::map<std::string, std::string>&) ;
} ;

class SQL2005Ado : public SQLAdo
{
public:
    SVERROR connect(const std::string sqlInstance, const std::string& dbName) ;
    SVERROR getDbNames(std::list<std::string>& dbNames) ;
    SVERROR discoverDatabase(const std::string& dbName, MSSQLDBMetaData&) ;
    SVERROR getDbProperties(std::string, std::map<std::string, std::string>&) ;
    SVERROR getServerProperties(std::map<std::string, std::string>&) ;
} ;

class SQL2008Ado : public SQLAdo
{
public:
    SVERROR connect(const std::string sqlInstance, const std::string& dbName) ;
    SVERROR getDbNames(std::list<std::string>& dbNames) ;
    SVERROR discoverDatabase(const std::string& dbName, MSSQLDBMetaData&) ;
    SVERROR getDbProperties(std::string, std::map<std::string, std::string>&) ;
    SVERROR getServerProperties(std::map<std::string, std::string> &) ;
} ;

class SQL2012Ado : public SQLAdo
{
public:
    SVERROR connect(const std::string sqlInstance, const std::string& dbName) ;
    SVERROR getDbNames(std::list<std::string>& dbNames) ;
    SVERROR discoverDatabase(const std::string& dbName, MSSQLDBMetaData&) ;
    SVERROR getDbProperties(std::string, std::map<std::string, std::string>&) ;
    SVERROR getServerProperties(std::map<std::string, std::string> &) ;
} ;

SVERROR getMailBoxServerNameProperty(SQLAdo*, std::string, std::list<std::string>&);
#endif
