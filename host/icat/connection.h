#ifndef __CONNECTION_H_
#define __CONNECTION_H_

#include "defs.h"

#include <vector>
#include <sqlite3x.hpp>

enum VALUETYPE {TYPENULL,TYPEINT,TYPEFLOAT,TYPEDOUBLE,TYPECHAR,TYPESTRING};

class TypedValue
{
	union{
		int ival;
		char cval;
		float fval;
		double dval;
	}u;
	std::string m_szVal;
	VALUETYPE m_type;
public:

	TypedValue() ;
	TypedValue(const TypedValue& tval) ;
	TypedValue(int val) ;
	TypedValue(double val) ;
	TypedValue(char val) ;
	TypedValue(std::string val) ;
	VALUETYPE getType() const  ;
	int getInt() const  ;
	float getFloat() const ; 
	double getDouble() const  ;
	char getChar() const ;
	std::string getString() const ;
	~TypedValue() ;
};

class DBRow
{
	std::vector<TypedValue> m_vCells;
	size_t m_Cnt;
	size_t m_Size;
public:
	DBRow() ;
	DBRow(std::vector<TypedValue> vCells) ;
	void push(TypedValue val) ;
	TypedValue nextVal() ;
	size_t getSize() ;
	~DBRow() ;
};

class DBResultSet
{
	std::vector<DBRow> m_vRows;
	size_t m_Size;
	size_t m_Cnt;

public:
	DBResultSet() ;
	DBResultSet(std::vector<DBRow> vRows) ;
	DBRow nextRow()  ;
	void push(DBRow row) ;
	bool isEmpty()  ;
	size_t getSize() ;
	void clear() ;
	~DBResultSet()  ;
};


class Connection
{
public:
	Connection() ;
	virtual bool open() = 0;
	virtual void close() = 0;
	virtual bool execQuery(const std::string& query, DBResultSet& set) = 0;
	virtual bool beginTransaction() = 0 ;
	virtual bool rollbackTransaction() = 0 ;
	virtual bool commitTransaction() = 0 ;
	virtual ~Connection(){ }
};

class SqliteConnection : Connection
{
	boost::shared_ptr<sqlite3x::sqlite3_connection> m_con ;
	boost::shared_ptr<sqlite3x::sqlite3_transaction> m_trans ;
	std::string m_dbFileName ;
public:
	SqliteConnection() ;
	void setDbPath(const std::string& dbFileName) ;
	SqliteConnection(const std::string& dbFileName) ;
	bool open() ;
	void close() ;
	bool execQuery(const std::string& query, DBResultSet & set) ;
	bool beginTransaction() ;
	bool rollbackTransaction() ;
	bool commitTransaction() ;
	~SqliteConnection() ;
};

#endif
