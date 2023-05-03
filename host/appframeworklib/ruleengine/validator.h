#ifndef __VALIDATOR__H
#define __VALIDATOR__H
#include <map>
#include "appglobals.h"
#include <boost/shared_ptr.hpp>
#include "applicationsettings.h"

enum InmRuleErrorCode
{
    RULE_PASSED = 0,
    HOSTNAME_MATCH_ERROR,
    GET_VOLUME_NAME_FAILED_ERROR,
    NESTED_MOUNT_POINT_ERROR,
    APPLICATION_DATA_ON_SYSTEM_DRIVE_ERROR,
    MULTI_NIC_VALIDATION_ERROR,
    LCR_ENABLED_ERROR,
    CCR_ENABLED_ERROR,
    VERSION_MISMATCH_ERROR,
    EXCHANGE_ADMINGROUP_MISMATCH_ERROR,
    EXCHANGE_DC_ERROR,
    SERVICE_NOT_FOUND,
    SERVICE_STATUS_ERROR,
    SQL_DB_ONLINE_ERROR,
    FIREWALL_STATUS_CHECK_ERROR,
    GET_DOMAIN_ERROR,
    DNS_SERVER_QUERY_ERROR,
    DNS_SERVER_REDIRECT_ERROR,
    DNS_UPDATE_ERROR,
    AD_PRIVILEGES_CHECK_FAILED_ERROR,
    CONFIGURED_IP_RETRIEVAL_ERROR,
    NAT_IP_ADDRESS_CHECK_ERROR,
    VACP_PROCESS_CREATION_ERROR,
    VACP_TAG_ISSUE_ERROR,
    VSS_E_INFRASTRUCATURE,
    VOLUME_CAPACITY_ERROR,
    SQL_DATA_ROOT_ERROR,
    SCR_ENABLED_ERROR,
    DYNAMIC_DNS_UPDATE_ERROR,
    DNS_AVAILABILITY_ERROR,
    DC_AVAILABILITY_ERROR,
    HOST_DC_ERROR,
	SQL_INSTANCENAME_MISMATCH_ERROR,
    DUPLICATE_MAILSTORE_COPY_ERROR,
    SQL_INSTALLPATH_ERROR, 
    CACHE_DIR_CAPACITIY_ERROR,
    DATABASE_NOT_RUNNING,
    INM_ERROR_NONE,
    RULE_STAT_NONE,
	INM_MISMATCH_ERROR,
    TARGET_VOLUME_NOTAVAILABLE_ERROR,
	VSS_ROLEUP_ERROR,
    PAGE_FILE_VOLUME_ERROR,
    DUMMY_LUNZ_NOT_FOUND_ERROR,
    ORACLE_RECOVERY_LOG_ERROR,
    DATABASE_NOT_SHUTDOWN,
    TARGET_VOLUME_IN_USE_ERROR,
    DATABASE_CONFIG_ERROR,
	//EXCHANGE_MTA_MISMATCH_ERROR,
	CM_MIN_SPACE_PER_PAIR_ERROR,
    DB2_DATABASE_NOT_ACTIVE,
    DB2_INSTANCE_NOT_SHUTDOWN,
    DB2_CONSISTENCY_CHECK_ERROR
};

enum InmRuleIds
{
    HOST_NAME_CHECK = 1,
    NESTED_MOUNTPOINT_CHECK,
    SYSREM_DRIVE_DATA_CHECK,
    MULTIPLE_NIC_CHECK,
    LCR_ENABLED_CHECK,
    CCR_ENABLED_CHECK,
    VERSION_MISMATCH_CHECK,
    ADMINISTRATIVE_GROUPS_MISMATCH_CHECK,
    DOMAIN_MISMATCH_CHECK,
    SERVICE_STATUS_CHECK,
    SQL_DB_STATUS_CHECK,
    FIREWALL_CHECK,
    DOMAIN_CHECK,
    DNS_UPDATE_CHECK,
    AD_UPDATE_CHECK,
    CONFIGURED_IP_CHECK,
    CONSISTENCY_TAG_CHECK,
    VOLUME_CAPACITY_CHECK,
    SQL_DATAROOT_CHECK,
    SCR_ENABLED_CHECK,
    DYNAMIC_DNS_UPDATE_CHECK,
    DNS_AVAILABILITY_CHECK,
	DC_AVAILABILITY_CHECK,
	HOST_DC_CHECK,
	SQL_INSTANCE_CHECK,
	SINGLE_MAILSTORE_COPY_CHECK,
    SQL_INSTALL_VOLUME_CHECK,
    CACHE_DIR_CAPACITY_CHECK,
	IP_MISMATCH_CHECK,
    TARGET_VOLUME_AVAILABILITY_CHECK,
	VSS_ROLEUP_CHECK,
    PAGE_FILE_VOLUME_CHECK,
    DB_INSTALL_CHECK,
    DUMMY_LUNZ_CHECK,
    DB_SHUTDOWN_CHECK,
    TARGET_VOLUME_IN_USE_CHECK,
    DB_CONFIG_CHECK,
	//MTA_MISMATCH_CHECK,
	CM_MIN_SPACE_PER_PAIR_CHECK,
    DB_ACTIVE_CHECK,
    DB2_SHUTDOWN_CHECK,
    DB2_CONFIG_CHECK,
    DB2_CONSISTENCY_TAG_CHECK
};


class Validator
{
    std::string m_ruleName ;
    std::string m_ruleDesc ;
    InmRuleStatus m_ruleStat ;
    std::string m_resultStr ;
    std::string m_fixStepsStr ;
	InmRuleErrorCode m_ErrorCode;	
	std::string m_ErrMessage;
	InmRuleIds m_RuleId;
	static std::map<InmRuleIds, std::pair<std::string, std::string> >  m_DescriptionLookUpMap ;
	static std::map<InmRuleIds, std::map<InmRuleErrorCode, std::pair<std::string, std::string> > > m_errorLookUpMap ;
    std::list<InmRuleIds> m_dependentRules ;
    std::list<InmRuleIds> m_failedRules ;
public:
    std::string rulename() ;
    std::string description() ;
    Validator(const std::string& name, const std::string& description, InmRuleIds ruleId) ;
    Validator(InmRuleIds ruleId) ;
    
    virtual SVERROR evaluate() = 0 ;
    virtual bool canfix() = 0 ;
    virtual SVERROR fix() = 0 ;
    
    void setStatus(InmRuleStatus status) ; 
    InmRuleStatus getStatus() ;
    std::string statusToString() ;
	InmRuleIds getRuleId();
	InmRuleErrorCode getRuleExitCode();
	void setRuleExitCode(InmRuleErrorCode errCode);
	void setRuleMessage(std::string& errMessage);
	std::string getRuleMessage();
    void setFailedRules(const std::list<InmRuleIds> failedRules) ;
    void setDependentRules(const std::list<InmRuleIds> dependentRules) ;
    bool isDependentRuleFailed(std::string& ruleName)  ;
	static void CreateNameDescriptionLookupMap();
	static void CreateErrorLookupMap();
	static std::pair<std::string, std::string> GetNameAndDescription(InmRuleIds);
	static std::pair<std::string,std::string> GetErrorData(InmRuleIds, InmRuleErrorCode);
	static std::string getErrorString(InmRuleErrorCode);
	static std::string getInmRuleIdStringFromID(InmRuleIds id);
    void convertToRedinessCheckStruct(ReadinessCheck& rdyChek) ;
} ;

typedef boost::shared_ptr<Validator> ValidatorPtr ;
#endif
