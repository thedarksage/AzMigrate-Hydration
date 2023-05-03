#include "connection.h"
#include "defs.h"
#include "icatexception.h"
#include <iostream>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include "configvalueobj.h"

typedef ACE_Guard<ACE_Mutex> AutoGuard ;
ACE_Mutex dbLock ;

TypedValue::TypedValue()
{
	m_type	= TYPENULL;
	u.dval	= 0;
	m_szVal	= "";
}

TypedValue::TypedValue(const TypedValue& tval)
{
	switch(tval.getType())
	{
	case TYPENULL:
		u.dval = 0;
		m_type = TYPENULL;
		break;

	case TYPEINT: u.ival = tval.getInt();
		m_type = TYPEINT;
		break;

	case TYPEFLOAT: u.fval = tval.getFloat();
		m_type = TYPEFLOAT;
		break;

	case TYPEDOUBLE: u.dval = tval.getDouble();
		m_type = TYPEDOUBLE;
		break;

	case TYPECHAR: u.cval = tval.getChar();
		m_type = TYPECHAR;
		break;

	case TYPESTRING: m_type = TYPESTRING;
		break;
	}

	m_szVal = tval.getString();
}


TypedValue::TypedValue(int val)
{
	u.dval	= 0;
	m_type	= TYPEINT; 
	u.ival	= val;
	m_szVal	= "";
}

TypedValue::TypedValue(double val)
{
	m_type	= TYPEDOUBLE; 
	u.fval	= (float) val;
	m_szVal	= "";
}

TypedValue::TypedValue(char val)
{
	m_type	= TYPECHAR; 
	u.cval	= val;
	m_szVal	= "";
}

TypedValue::TypedValue(std::string val)
{
	m_type = TYPESTRING; 
	m_szVal = val;
	u.dval	= 0;
}

VALUETYPE TypedValue::getType() const 
{
	return m_type;
}

int TypedValue::getInt() const 
{
	return u.ival;
}

float TypedValue::getFloat() const 
{
	return u.fval;
}

double TypedValue::getDouble() const 
{
	return u.dval;
}

char TypedValue::getChar() const 
{
	return u.cval;
}

std::string TypedValue::getString() const 
{
	return m_szVal;
}
TypedValue::~TypedValue()
{
}	

DBRow::DBRow()
{
	m_Cnt	= 0;
	m_Size	= 0;
	m_vCells.clear();
}

DBRow::DBRow(std::vector<TypedValue> vCells)
{
	m_vCells.assign(vCells.begin(), vCells.end());
	m_Cnt		= 0;
	m_Size		= m_vCells.size();
}

void DBRow::push(TypedValue val)
{
	m_vCells.push_back(val);
	m_Size = m_vCells.size();
}

TypedValue DBRow::nextVal()
{
	if(m_Cnt < m_Size)
	{
		return m_vCells[m_Cnt++];
	}
	throw "Value Out of bound";
}

size_t DBRow::getSize()
{
	return m_Size;
}
DBRow::~DBRow()
{
	m_vCells.clear();
	m_Cnt = 0;
	m_Size = 0;
}

DBResultSet::DBResultSet()
{
	m_Cnt	= 0;
	m_Size  = 0;
}

void DBResultSet::clear()
{
	m_Cnt = 0 ;
	m_Size = 0 ;
	m_vRows.clear() ;
}

DBResultSet::DBResultSet(std::vector<DBRow> vRows)
{
	m_vRows.assign(vRows.begin(), vRows.end());
	m_Cnt	= 0;
	m_Size	= m_vRows.size();
}

DBRow DBResultSet::nextRow()
{
	if(m_Cnt < m_Size)
	{
		return m_vRows[ m_Cnt++ ];
	}

	throw "Value Out of bound";
}

void DBResultSet::push(DBRow row)
{
	m_vRows.push_back(row);
	m_Size = m_vRows.size();
}

bool DBResultSet::isEmpty()
{
	return m_vRows.size() == 0 ? true : false;
}

size_t DBResultSet::getSize()
{
	return m_Size;
}
DBResultSet::~DBResultSet()
{
	m_vRows.clear();
	m_Cnt = 0;
	m_Size = 0;
}

Connection::Connection()
{
}

SqliteConnection::SqliteConnection()
{
}

void SqliteConnection::setDbPath(const std::string& dbFileName)
{
	m_dbFileName = dbFileName ;
}

SqliteConnection::SqliteConnection(const std::string& dbFileName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	m_dbFileName = dbFileName ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
} 

bool SqliteConnection::open()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	bool bRet = false ;
	try
	{
		m_con.reset(new (std::nothrow) sqlite3x::sqlite3_connection()) ;
		if( m_con == NULL )
		{
			throw icatOutOfMemoryException("Failed to create connection object\n") ;
		}
		m_con->open(m_dbFileName.c_str()) ; 
		bRet = true ;
	}
	catch(sqlite3x::database_error de)
	{
		DebugPrintf(SV_LOG_ERROR, "Unable to open database %s\n", m_dbFileName.c_str()) ;
		throw icatException(de.what()) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return bRet;
}
void SqliteConnection::close()
{
	m_con->close() ;
}

bool SqliteConnection::execQuery(const std::string& query, DBResultSet &set)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    AutoGuard lock(dbLock) ;
	bool retVal = false;
	set.clear() ;
	try
	{
		sqlite3x::sqlite3_command cmd(*m_con, query.c_str()) ;
		sqlite3x::sqlite3_reader reader= cmd.executereader() ;
		
		while(reader.read())
		{
			int colIndex  = 0 ;
			DBRow row ;
			try
			{
				while(true)
				{
					TypedValue val(reader.getstring(colIndex)) ;
					row.push(val) ;
					colIndex ++ ;
				}
			}
			catch(std::out_of_range )
			{
				//empty handler
			}
			set.push(row) ;
		}
		retVal = true ;
	}
	catch(sqlite3x::database_error de)
	{
			DebugPrintf(SV_LOG_ERROR, "Failed to query the database.. %s\n", de.what()) ;
			DebugPrintf(SV_LOG_ERROR, "Query is %s\n", query.c_str()) ;
			throw icatException("Query to db failed\n") ;
	}
	DebugPrintf(SV_LOG_DEBUG, "Result Set Size : %d Query : %s\n", set.getSize(), query.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return retVal ; 
}
SqliteConnection::~SqliteConnection()
{
	close() ;
}



bool SqliteConnection::beginTransaction()
{
	bool retVal = true ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	m_trans.reset(new (std::nothrow) sqlite3x::sqlite3_transaction(*m_con, false)) ;
	if( m_trans.get() == NULL )
	{
		retVal = false ;
		throw icatException("Unable to create sqlite3_transaction object\n") ;
	}
	try
	{
		m_trans->begin() ;
	}
	catch(sqlite3x::database_error de)
	{
		DebugPrintf(SV_LOG_ERROR, "database_error %s\n", de.what()) ;
		retVal = false ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return retVal ;
}

bool SqliteConnection::rollbackTransaction()
{
	bool retVal = true ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	if( m_trans.get() == NULL )
	{
		retVal = false ;
		throw icatException("Invalid sqlite3_transaction object\n") ;
	}
	try
	{
		m_trans ->rollback() ;
	}
	catch(sqlite3x::database_error de)
	{
		DebugPrintf(SV_LOG_ERROR, "database_error %s\n", de.what()) ;
		retVal = false ;
	}
	m_trans.reset() ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return retVal ;
}
bool SqliteConnection::commitTransaction()
{
	bool retVal  = true ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	if( m_trans.get() == NULL )
	{
		retVal  = false ;
		throw icatException("Invalid sqlite3_transaction object\n") ;

	}
	try
	{
		m_trans ->commit();
	}
	catch(sqlite3x::database_error de)
	{
		DebugPrintf(SV_LOG_ERROR, "database_error %s\n", de.what()) ;
		retVal  = false ;
	}
	m_trans.reset() ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return retVal ;
}

