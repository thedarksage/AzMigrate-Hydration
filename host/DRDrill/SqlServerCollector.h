#ifndef __SQL_SERVER_COLLECTOR_H
#define __SQL_SERVER_COLLECTOR_H

#include "Common.h"
using namespace DrDrillNS;

#include "ProductDataCollector.h"

class SqlServerCollector : public ProductDataCollector
{
	public:
		SqlServerCollector(void);
		~SqlServerCollector(void);
      //virtual bool Create()= 0;
      bool CollectSqlData();
      //MsSqlCollector& GetSqlData();
      
	private:
		_tstring GetSqlVersion();
		//Data Members
		_tstring m_strSqlVersion;
    
      
  };
#endif
