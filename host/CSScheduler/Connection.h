#ifndef __CONNECTION_H_
#define __CONNECTION_H_

#include <vector>
#include "TypedValue.h"
#include "CXConnection.h"
#include "queue.h"

using namespace std;
class DBRow
{
	vector<TypedValue> m_InmvCells;
	int m_lnmCnt;
	int m_InmSize;
public:
	DBRow()
	{
		m_lnmCnt	= 0;
		m_InmSize	= 0;
		m_InmvCells.clear();
	}

	DBRow(vector<TypedValue> vCells)
	{
		m_InmvCells.assign(vCells.begin(), vCells.end());
		m_lnmCnt		= 0;
		m_InmSize		= m_InmvCells.size();
	}

	void push(TypedValue val)
	{
		m_InmvCells.push_back(val);
		m_InmSize = m_InmvCells.size();
	}

	TypedValue nextVal()
	{
		if(m_lnmCnt < m_InmSize)
		{
			return m_InmvCells[m_lnmCnt++];
		}
		throw "Value Out of bound";
	}

	unsigned int getSize()
	{
		return m_InmSize;
	}
	~DBRow()
	{
		m_InmvCells.clear();
		m_lnmCnt = 0;
		m_InmSize = 0;
	}
};

class DBResultSet
{
	vector<DBRow> m_vRows;
	int m_InmSize;
	int m_lnmCnt;
	int m_rowsUpdated;

public:

	DBResultSet()
	{
		m_lnmCnt	= 0;
		m_InmSize  = 0;
		m_rowsUpdated = 0;
	}

	DBResultSet(vector<DBRow> vRows)
	{
		m_vRows.assign(vRows.begin(), vRows.end());
		m_lnmCnt	= 0;
		m_InmSize	= m_vRows.size();
		m_rowsUpdated = 0;
	}

	DBRow nextRow()
	{
		if(m_lnmCnt < m_InmSize)
		{
			return m_vRows[ m_lnmCnt++ ];
		}

		throw "Value Out of bound";
	}

	void setRowsUpdated(int nRowsUpdated)
	{
		m_rowsUpdated = nRowsUpdated;
	}

	int getRowsUpdated() const
	{
		return m_rowsUpdated;
	}

	void push(DBRow row)
	{
		m_vRows.push_back(row);
		m_InmSize = m_vRows.size();
	}

	bool isEmpty()
	{
		return m_vRows.size() == 0 ? true : false;
	}

	unsigned int getSize()
	{
		return m_InmSize;
	}
	~DBResultSet()
	{
		m_vRows.clear();
		m_lnmCnt = 0;
		m_InmSize = 0;
	}
};

class Connection
{
public:


	virtual int close() = 0;
	
	virtual DBResultSet execQuery(const char* InmszSQL) = 0;
	virtual DBResultSet execQuery(const char* InmszSQL,bool bProcedure) = 0;
	virtual ~Connection(){ }
	virtual int getLastError() const = 0;
};

#endif
