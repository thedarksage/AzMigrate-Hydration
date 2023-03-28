#ifndef __ORACLE_VALIDATORS_H
#define __ORACLE_VALIDATORS_H
#include "ruleengine/validator.h"
#include "applicationsettings.h"
#include "OracleDiscovery.h"


class OracleTestValidator: public Validator
{

    OracleTestValidator(const std::string& name,
                const std::string& description,
                InmRuleIds ruleId) ;

	public:
		OracleTestValidator(InmRuleIds ruleId);
		SVERROR evaluate();
		SVERROR fix();
		bool canfix();
		
};

class OracleConsistencyValidator: public Validator
{
	OracleAppVersionDiscInfo m_oracleAppInfo;
	OracleConsistencyValidator(const std::string& name,
                const std::string& description,
                const OracleAppVersionDiscInfo &appInfo,
                InmRuleIds ruleId) ;

	public:
		OracleConsistencyValidator(const OracleAppVersionDiscInfo &appInfo, InmRuleIds ruleId);
		SVERROR evaluate();
		SVERROR fix();
		bool canfix();
};

class OracleDatabaseRunValidator: public Validator
{
	OracleAppVersionDiscInfo m_oracleAppInfo;
	OracleDatabaseRunValidator(const std::string& name,
                const std::string& description,
                const OracleAppVersionDiscInfo &appInfo,
                InmRuleIds ruleId) ;

	public:
		OracleDatabaseRunValidator(const OracleAppVersionDiscInfo &appInfo, InmRuleIds ruleId);
		SVERROR evaluate();
		SVERROR fix();
		bool canfix();
};

class OSVersionCompatibilityCheckValidator: public Validator
{
	std::string m_srcOSVersion;
	std::string m_tgtOSVersion;

	OSVersionCompatibilityCheckValidator(const std::string &name,
		const std::string& description,
		std::string &srcOSVersion,
		std::string &tgtOSVersion,
		InmRuleIds ruleId);

	public:
		OSVersionCompatibilityCheckValidator(std::string &srcOSVersion, std::string &tgtOSVersion, InmRuleIds ruleId);
		SVERROR evaluate();
		SVERROR fix();
		bool canfix();

};

class DatabaseVersionCompatibilityCheckValidator: public Validator
{
	std::string m_srcDatabaseVersion;
	std::string m_tgtDatabaseVersion;

	DatabaseVersionCompatibilityCheckValidator(const std::string &name,
		const std::string& description,
		std::string &srcDatabaseVersion,
		std::string &tgtDatabaseVersion,
		InmRuleIds ruleId);

	public:
		DatabaseVersionCompatibilityCheckValidator(std::string &srcDatabaseVersion, std::string &tgtDatabaseVersion, InmRuleIds ruleId);
		SVERROR evaluate();
		SVERROR fix();
		bool canfix();

};


class OracleDatabaseShutdownValidator: public Validator
{
	std::list<std::map<std::string, std::string> > m_oracleAppInfo;

	OracleDatabaseShutdownValidator(const std::string& name,
                const std::string& description,
                const std::list<std::map<std::string, std::string> > &appInfo,
                InmRuleIds ruleId) ;

	public:
		OracleDatabaseShutdownValidator(const std::list<std::map<std::string, std::string> > &appInfo, InmRuleIds ruleId);
		SVERROR evaluate();
		SVERROR fix();
		bool canfix();
};

class OracleDatabaseConfigValidator: public Validator
{
	std::list<std::map<std::string, std::string> > m_oracleAppInfo;
	std::list<std::map<std::string, std::string> > m_tgtVolInfo;

	OracleDatabaseConfigValidator(const std::string& name,
                const std::string& description,
                const std::list<std::map<std::string, std::string> > &appInfo,
                const std::list<std::map<std::string, std::string> > &volInfo,
                InmRuleIds ruleId) ;

	public:
		OracleDatabaseConfigValidator(const std::list<std::map<std::string, std::string> > &appInfo, 
                    const std::list<std::map<std::string, std::string> > &volInfo,
                    InmRuleIds ruleId);
		SVERROR evaluate();
		SVERROR fix();
		bool canfix();
};

#endif
