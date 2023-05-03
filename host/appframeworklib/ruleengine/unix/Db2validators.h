#ifndef _DB2_VALIDATORS_H
#define _DB2_VALIDATORS_H
#include "ruleengine/validator.h"
#include "applicationsettings.h"
#include "Db2Discovery.h"

class Db2ConsistencyValidator: public Validator
{
    Db2AppInstanceDiscInfo m_db2AppInfo;
    Db2ConsistencyValidator(const std::string& name, const std::string& description, const Db2AppInstanceDiscInfo &appInfo, InmRuleIds ruleId) ;

    public:
    Db2ConsistencyValidator(const Db2AppInstanceDiscInfo &appInfo, InmRuleIds ruleId);
    SVERROR evaluate();
    SVERROR fix();
    bool canfix();
};

class Db2DatabaseRunValidator: public Validator
{
    Db2AppInstanceDiscInfo m_db2AppInfo;
    std::string m_instanceName;
	std::string m_dbName;
    Db2DatabaseRunValidator(const std::string& name, const std::string& description, const Db2AppInstanceDiscInfo &appInfo, std::string instanceName, std::string appDatabaseName, InmRuleIds ruleId) ;

    public:
    Db2DatabaseRunValidator(const Db2AppInstanceDiscInfo &appInfo, std::string instanceName, std::string appDatabaseName, InmRuleIds ruleId);
    SVERROR evaluate();
    SVERROR fix();
    bool canfix();
};

class Db2DatabaseShutdownValidator: public Validator
{
    std::list<std::map<std::string, std::string> > m_db2AppInfo;

    Db2DatabaseShutdownValidator(const std::string& name,const std::string& description,const std::list<std::map<std::string, std::string> > &appInfo,
                                                                    InmRuleIds ruleId) ;

    public:
    Db2DatabaseShutdownValidator(const std::list<std::map<std::string, std::string> > &appInfo, InmRuleIds ruleId);
    SVERROR evaluate();
    SVERROR fix();
    bool canfix();
};

class Db2DatabaseConfigValidator: public Validator
{
   std::list<std::map<std::string, std::string> > m_db2AppInfo;
   std::list<std::map<std::string, std::string> > m_tgtVolInfo;

   Db2DatabaseConfigValidator(const std::string& name,const std::string& description, const std::list<std::map<std::string, std::string> > &appInfo,
                                                         const std::list<std::map<std::string, std::string> > &volInfo,InmRuleIds ruleId) ;

   public:
   Db2DatabaseConfigValidator(const std::list<std::map<std::string, std::string> > &appInfo, const std::list<std::map<std::string, std::string> > &volInfo,
                                                                        InmRuleIds ruleId);
   SVERROR evaluate();
   SVERROR fix();
   bool canfix();
};

#endif
