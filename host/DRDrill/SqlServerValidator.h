#ifndef __SQL_SERVER_VALIDATOR_H
#define __SQL_SERVER_VALIDATOR_H

#include "ProductValidator.h"
#include "SqlServerCollector.h"

class SqlServerValidator : public ProtectionValidator
  {
    public:
		SqlServerValidator(void);
		~SqlServerValidator(void);
      //virtual bool Create()= 0;
      bool ValidateMsSqlDataOnSysDrive(SqlServerCollector sqlData);
      
      //Only Online Validations can be done
      ULONG GetNoOfDatabases();
      bool SpawnThreads();//a maximum of 5 threads
    
    private:
      SqlServerCollector m_sqlValidations;

      //Add Sql Specific Validation Results here
      
  };
#endif