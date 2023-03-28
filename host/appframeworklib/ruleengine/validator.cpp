#include "appglobals.h"
#include "validator.h"
#include <boost/lexical_cast.hpp>
std::map<InmRuleIds, std::pair<std::string, std::string> > Validator::m_DescriptionLookUpMap ;
std::map<InmRuleIds, std::map<InmRuleErrorCode, std::pair<std::string, std::string> > > Validator::m_errorLookUpMap ;


Validator::Validator(const std::string& name, const std::string& description, InmRuleIds ruleId)
{
    m_ruleName = name ;
    m_ruleDesc = description ;
    m_ruleStat = INM_RULESTAT_NONE ;
    m_RuleId = ruleId ;
}



Validator::Validator(InmRuleIds ruleId)
{
    m_ruleStat = INM_RULESTAT_NONE ;
    m_RuleId = ruleId ;
}
    
std::string Validator::rulename()
{
    return m_ruleName ;
}
std::string Validator::description()
{
    return m_ruleDesc ;
}


void Validator::setStatus(InmRuleStatus status)
{
    m_ruleStat = status ;
}
InmRuleStatus Validator::getStatus()
{
    return m_ruleStat ;
}
std::string Validator::statusToString()
{
    switch(m_ruleStat)
    {
        case INM_RULESTAT_NONE :
            return "UnKnown" ;
            break ;
        case INM_RULESTAT_PROGRESS :
            return "In Progress" ;
            break ;
        case INM_RULESTAT_PASSED :
            return "Passed" ;
            break ;
        case INM_RULESTAT_FAILED :
            return "Failed" ;
            break ;
        case INM_RULESTAT_CANT_FIX :
            return "Can't be Fixed Automatically" ;
            break ;
        case INM_RULESTAT_FIXED :
            return "Fixed" ;
            break ;
        case INM_RULESTAT_SKIPPED :
            return "Skipped" ;
            break ;
        default :
            return "UnKnown" ;
            break ;
    }
}

InmRuleIds Validator::getRuleId()
{
	return m_RuleId;
}

InmRuleErrorCode Validator::getRuleExitCode()
{
	return m_ErrorCode;
}

void Validator::setRuleExitCode(InmRuleErrorCode errCode)
{
	m_ErrorCode = errCode;
}
void Validator::setRuleMessage(std::string& errMessage)
{
    m_ErrMessage = errMessage;
}
void Validator::setFailedRules(const std::list<InmRuleIds> failedRules) 
{
    m_failedRules = failedRules ;
}

void Validator::setDependentRules(const std::list<InmRuleIds> dependentRules)
{
    m_dependentRules = dependentRules ;
}

bool Validator::isDependentRuleFailed(std::string& ruleName) 
{
    bool dependentRuleFailed = false;
    std::list<InmRuleIds>::const_iterator iter = m_dependentRules.begin() ;
    while( iter != m_dependentRules.end() )
    {
        if( std::find(m_failedRules.begin(), m_failedRules.end(),  *iter) != m_failedRules.end() )
        {
            dependentRuleFailed = true ;
            ruleName = GetNameAndDescription(*iter).first.c_str() ;
            DebugPrintf(SV_LOG_DEBUG, "The dependent Rule Failed %s\n", GetNameAndDescription(*iter).first.c_str()) ;
            break ;
        }
        iter++ ;
    }
    return dependentRuleFailed ;
}

std::string Validator::getRuleMessage()
{
    return m_ErrMessage ;
}

void Validator::CreateNameDescriptionLookupMap()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_DescriptionLookUpMap.insert(std::make_pair(HOST_NAME_CHECK ,
                                            std::make_pair("Host Name Rule",
                                                           "Exchange Server name should not match with the forbidden Names"))) ;
	
    m_DescriptionLookUpMap.insert(std::make_pair(NESTED_MOUNTPOINT_CHECK ,
                                            std::make_pair("Nested Mount points Rule", 
                                                           "Nested mount points are not supported by Scout"))) ;

	m_DescriptionLookUpMap.insert(std::make_pair(SYSREM_DRIVE_DATA_CHECK,
                                            std::make_pair("Application data on system drive Rule", 
                                                           "Checks whether the application data is on system drive or not"))) ;

	m_DescriptionLookUpMap.insert(std::make_pair(MULTIPLE_NIC_CHECK,
                                            std::make_pair("Multi NICs Rule", 
                                                           "Multiple NICs Configuration"))) ;

	m_DescriptionLookUpMap.insert(std::make_pair(LCR_ENABLED_CHECK,
                                            std::make_pair("LCR Configuration Rule",
                                                           "Checks whether the LCR is enabled for any Storage Group/Mail Store"))) ;
	
    m_DescriptionLookUpMap.insert(std::make_pair(CCR_ENABLED_CHECK,
                                            std::make_pair("CCR Configuration Rule", 
                                                           "Checks whether the CCR is enabled for any Storage Group/Mail Store"))) ;

    m_DescriptionLookUpMap.insert(std::make_pair(SCR_ENABLED_CHECK,
                                            std::make_pair("SCR Configuration Rule", 
                                                           "Checks whether the SCR is enabled for any Storage Group/Mail Store"))) ;

    m_DescriptionLookUpMap.insert(std::make_pair(VERSION_MISMATCH_CHECK,
                                            std::make_pair("Application Version Comparison Rule", 
                                                           "Checks Application versions at source and target are matching ot not"))) ;
	
    m_DescriptionLookUpMap.insert(std::make_pair(ADMINISTRATIVE_GROUPS_MISMATCH_CHECK,
                                            std::make_pair("Administrative Group Comparison Rule",
                                                           "Checks whether Source and Target belong to same administrative group on the domain or not"))) ;
    
    m_DescriptionLookUpMap.insert(std::make_pair(DOMAIN_MISMATCH_CHECK,
                                            std::make_pair("Domain Comparison Rule",
                                                           "Checks whether Source and Target belong to same domain or not"))) ;
    
    m_DescriptionLookUpMap.insert(std::make_pair(SERVICE_STATUS_CHECK,
                                            std::make_pair("Services Rule",
                                                           "Checking Services Running State"))) ;
    
    m_DescriptionLookUpMap.insert(std::make_pair(SQL_DB_STATUS_CHECK,
                                            std::make_pair("SQL Database Status Rule",
                                                           "Checks whether the databases are online or not"))) ;
    
    m_DescriptionLookUpMap.insert(std::make_pair(FIREWALL_CHECK,
                                            std::make_pair("Firewall Status Rule",
                                                           "Checks whether the firewall is on or not"))) ;
    
    m_DescriptionLookUpMap.insert(std::make_pair(DOMAIN_CHECK,
                                            std::make_pair("Domain Name",
                                                           "Checking domain name"))) ;
    
    m_DescriptionLookUpMap.insert(std::make_pair(DNS_UPDATE_CHECK,
                                            std::make_pair("DNS Update Privilege Rule",
                                                           "Validating User's DNS Update Privileges"))) ;

    m_DescriptionLookUpMap.insert(std::make_pair(AD_UPDATE_CHECK,
                                            std::make_pair("AD Update Privilege Rule", 
                                                           "Validating User's AD Update Privileges"))) ;

    m_DescriptionLookUpMap.insert(std::make_pair(CONFIGURED_IP_CHECK,
                                            std::make_pair("NAT IP Rule",
                                                           "Validating NAT IP Status"))) ;

    m_DescriptionLookUpMap.insert(std::make_pair(CONSISTENCY_TAG_CHECK,
                                            std::make_pair("Consistency Tag On Volumes Rule",
                                            "Checks whether a Tag can be issued on database Volumes"))) ;
    m_DescriptionLookUpMap.insert(std::make_pair(VOLUME_CAPACITY_CHECK,
                                            std::make_pair("Volume Capacity Rule",
                                                           "Target volume capacities are not in sync with source volume capacities"))) ;
    m_DescriptionLookUpMap.insert(std::make_pair(SQL_DATAROOT_CHECK,
                                            std::make_pair("SQL Data root check", 
                                                           "Source and target SQL instance data roots should be the same path"))) ;
    m_DescriptionLookUpMap.insert(std::make_pair(DYNAMIC_DNS_UPDATE_CHECK, 
                                            std::make_pair("Dynamic DNS Update Configuration Rule", 
                                                            "Checks whether the dynamic DNS update is enabled or not"))) ;
    m_DescriptionLookUpMap.insert(std::make_pair(DNS_AVAILABILITY_CHECK, 
                                            std::make_pair("DNS Availability Rule", 
                                                            "Checks whether the DNS is available or not"))) ;
	m_DescriptionLookUpMap.insert(std::make_pair(DC_AVAILABILITY_CHECK, 
                                            std::make_pair("DC Availability Rule", 
                                                            "Checks whether the DC is available or not"))) ;
	m_DescriptionLookUpMap.insert(std::make_pair(HOST_DC_CHECK, 
                                            std::make_pair("Host as DC Rule", 
                                                            "Checks whether the host itself is a DC or not"))) ;
	
	m_DescriptionLookUpMap.insert(std::make_pair(SINGLE_MAILSTORE_COPY_CHECK, 
                                            std::make_pair("Single Mailstore Copy Configuration Rule", 
                                                            "Checks whether any of mail store is having copies on other servers."))) ;
	m_DescriptionLookUpMap.insert(std::make_pair(SQL_INSTANCE_CHECK, 
                                            std::make_pair("SQL Instance Name Check", 
                                                            "Checking whether SQL Instances name at source and target are same or not"))) ;
    m_DescriptionLookUpMap.insert(std::make_pair(CACHE_DIR_CAPACITY_CHECK, 
                                            std::make_pair("Cache drive free space availablity Rule", 
                                                            "Checks whether Cache drive has enough free space or not"))) ;
    m_DescriptionLookUpMap.insert(std::make_pair(VSS_ROLEUP_CHECK, 
                                            std::make_pair("VSS hotfix availability Rule", 
                                                            "Checks whether VSS supported hotfix availabile or not"))) ;
    m_DescriptionLookUpMap.insert(std::make_pair(PAGE_FILE_VOLUME_CHECK, 
                                            std::make_pair("Page File on application data volume Rule", 
                                                            "Checks whether any application volume has page file or not"))) ;

    m_DescriptionLookUpMap.insert(std::make_pair(DB_INSTALL_CHECK,
                                            std::make_pair("Database Installation Verification",
                                                            "Checks whether Oracle database is installed and running or not")));

    m_DescriptionLookUpMap.insert(std::make_pair(DUMMY_LUNZ_CHECK,
                                            std::make_pair("Applicance Target LUNZ Verification",
                                                            "Checks whether appliance target lunz is visible.")));

    m_DescriptionLookUpMap.insert(std::make_pair(DB_SHUTDOWN_CHECK,
                                            std::make_pair("Database Shutdown Verification",
                                                            "Checks whether Oracle database is shutdown or not")));

    m_DescriptionLookUpMap.insert(std::make_pair(TARGET_VOLUME_IN_USE_CHECK,
                                            std::make_pair("Secondary Server Volume In Use Verification",
                                                            "Checks whether secondary server volumes are in use by Filesystems or Volume managers.")));

	/*m_DescriptionLookUpMap.insert(std::make_pair(MTA_MISMATCH_CHECK,
                                            std::make_pair("MTA Comparison Rule",
                                                           "Checks the MTA enabled status in Source and Target"))) ;*/
	m_DescriptionLookUpMap.insert(std::make_pair(CM_MIN_SPACE_PER_PAIR_CHECK, 
                                            std::make_pair("Cache Memory free space per pair availablity Rule", 
                                                            "Checks whether Cache memory has enough free space per pair or not"))) ;
    m_DescriptionLookUpMap.insert(std::make_pair(DB_ACTIVE_CHECK,
                                            std::make_pair("DB2 Instance and Database Installation Verification",
                                                            "Checks whether DB2 instance is installed and running or not and DB2 database is active or not")));
    m_DescriptionLookUpMap.insert(std::make_pair(DB2_CONSISTENCY_TAG_CHECK,
                                            std::make_pair("Consistency Tag On Volumes Rule",
                                            "Checks whether a Tag can be issued on database Volumes"))) ;
    m_DescriptionLookUpMap.insert(std::make_pair(DB2_SHUTDOWN_CHECK,
                                            std::make_pair("DB2 Instance and Database Shutdown Verification",
                                                            "Checks whether DB2 instance is shutdown or not and DB2 database is uncataloged or not")));
    m_DescriptionLookUpMap.insert(std::make_pair(DB2_CONFIG_CHECK,
                                            std::make_pair("Secondary server configuration check for DB2",
                                                            "Verify if secondary server supports the primary server DB2 configurations")));

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void Validator::CreateErrorLookupMap()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::map<InmRuleErrorCode, std::pair<std::string, std::string> > tempMap;

	tempMap.insert(std::make_pair(HOSTNAME_MATCH_ERROR,
								  std::make_pair("Host name is matching with one of the forbidden names. ",
											"Please change the host name to make sure Scout can protect the server and avoid following names: \
                                            1.	Microsoft \
                                            2.	Exchange \
                                            3.	Configuration \
                                            4.	Service \
                                            5.	Groups \
                                            6.	Administrative \
                                            7.	Any names of  \
                                                a.	Storage groups \
                                                b.	Mail stores \
                                                c.	Information Store \
")));
	m_errorLookUpMap.insert(std::make_pair(HOST_NAME_CHECK,tempMap));
	tempMap.clear();


	tempMap.insert(std::make_pair(GET_VOLUME_NAME_FAILED_ERROR,
								  std::make_pair("Application’s Volume name cannot be retrieved.",
											"Please make sure the application is configured correctly.")));
	tempMap.insert(std::make_pair(NESTED_MOUNT_POINT_ERROR,
								  std::make_pair("Nested mount points are detected at the agent.",
											"Nested Mount points are not supported by InMage Scout. Please reconfigure the application avoiding nested mount points to continue protection.")));
	m_errorLookUpMap.insert(std::make_pair(NESTED_MOUNTPOINT_CHECK,tempMap));
	tempMap.clear();

	tempMap.insert(std::make_pair(APPLICATION_DATA_ON_SYSTEM_DRIVE_ERROR,
								  std::make_pair("Application’s Data is on System drive. Scout does not support this configuration",
											"Please move application’s data from system drive to other drive to continue protection")));
	m_errorLookUpMap.insert(std::make_pair(SYSREM_DRIVE_DATA_CHECK,tempMap));

	tempMap.clear();

	tempMap.insert(std::make_pair(MULTI_NIC_VALIDATION_ERROR,
								  std::make_pair("Multiple NICs are found with multiple IP Addresses.",
											"Multiple NIC configuration are not supported at the time of failover. \
                                            Please use Scout Host configuration tool to set the IP from available List")));
	m_errorLookUpMap.insert(std::make_pair(MULTIPLE_NIC_CHECK,tempMap));
	tempMap.clear();

    tempMap.insert(std::make_pair(LCR_ENABLED_ERROR,
								  std::make_pair("Local Continuos replication is enabled for the storage groups",
											"Disable Local Continuos Replication using Exchange System Manager to continue protection")));
	m_errorLookUpMap.insert(std::make_pair(LCR_ENABLED_CHECK,tempMap));

	tempMap.clear();
	tempMap.insert(std::make_pair(CCR_ENABLED_ERROR,
								  std::make_pair("Continuos Cluster Replication is configured for exchange on localhost",
											"Cannot support Cluster Continuous Replication configuration. \
                                            Re-configure Exchange without CCR to continue protection")));
	m_errorLookUpMap.insert(std::make_pair(CCR_ENABLED_CHECK,tempMap));

	tempMap.clear();
	tempMap.insert(std::make_pair(VERSION_MISMATCH_ERROR,
								  std::make_pair("Version number of Source and Target does not matches",
											"Make sure version at Source and target are of same application version to continue protection. \
                                            Version mismatch would cause failover to fail")));
	m_errorLookUpMap.insert(std::make_pair(VERSION_MISMATCH_CHECK,tempMap));
	tempMap.clear();
	tempMap.insert(std::make_pair(EXCHANGE_ADMINGROUP_MISMATCH_ERROR,
								  std::make_pair("Administrative Group  Name of source and target exchange is not matching",
											"Make sure Administrative Group of Source and Target are same to continue Exchange Protection")));
	m_errorLookUpMap.insert(std::make_pair(ADMINISTRATIVE_GROUPS_MISMATCH_CHECK,tempMap));
	tempMap.clear();
	tempMap.insert(std::make_pair(EXCHANGE_DC_ERROR,
								  std::make_pair("DC is not matching",
											"Source and target machines do not belong to same domain level. \
                                            Choose target that is at same domain level as Source to continue protection")));
	m_errorLookUpMap.insert(std::make_pair(DOMAIN_MISMATCH_CHECK,tempMap));
	tempMap.clear();

	tempMap.insert(std::make_pair(SERVICE_NOT_FOUND,
								  std::make_pair("The following service is not found:",
											"Make sure the service is installed and running")));
	tempMap.insert(std::make_pair(SERVICE_STATUS_ERROR,
								  std::make_pair("The Above service is not running on the agent:",
											"Make sure the service is installed and running")));
	m_errorLookUpMap.insert(std::make_pair(SERVICE_STATUS_CHECK,tempMap));
	tempMap.clear();

	tempMap.insert(std::make_pair(SQL_DB_ONLINE_ERROR,
								  std::make_pair("Following SQL Databases are offline",
											"Bring the offline databases to Online state to protect")));
	m_errorLookUpMap.insert(std::make_pair(SQL_DB_STATUS_CHECK,tempMap));
	tempMap.clear();
	

	tempMap.insert(std::make_pair(FIREWALL_STATUS_CHECK_ERROR,
								  std::make_pair("Failed to check Firewall status",
											" 1. Go to Control Panel \
											  2. Go to Administrative Tasks \
											  3. Change the firewall status appropriately ")));
	
	m_errorLookUpMap.insert(std::make_pair(FIREWALL_CHECK,tempMap));
	tempMap.clear();

	tempMap.insert(std::make_pair(GET_DOMAIN_ERROR,
								  std::make_pair("Failed to get the Domain Name of the local machine",
											"Please add the system to domain to continue full protection")));
														
	m_errorLookUpMap.insert(std::make_pair(DOMAIN_CHECK,tempMap));
	tempMap.clear();

	tempMap.insert(std::make_pair(DNS_SERVER_QUERY_ERROR,
								  std::make_pair("Failed to Query DNS server over",
											"DNS server could not be reached. \
                                            Please check connectivity from the agent machine to DNS")));

	tempMap.insert(std::make_pair(DNS_SERVER_REDIRECT_ERROR,
								  std::make_pair("Failed to Query DNS server over",
											"DNS server could not be reached. \
                                            Please check connectivity from the agent machine to DNS")));
	tempMap.insert(std::make_pair(DNS_UPDATE_ERROR,
								  std::make_pair("User does not have the privilege to modify the DNS record",
											"Please provide the sufficient privilege to service logon user to modify the DNS")));

	m_errorLookUpMap.insert(std::make_pair(DNS_UPDATE_CHECK,tempMap));
	tempMap.clear();

	tempMap.insert(std::make_pair(AD_PRIVILEGES_CHECK_FAILED_ERROR,
								  std::make_pair("Failed to Check ADPrivileges",
											"Please provide the sufficient Privileges \
                                            to service logon user to complete the AD replication")));
	m_errorLookUpMap.insert(std::make_pair(AD_UPDATE_CHECK,tempMap));
	tempMap.clear();

	tempMap.insert(std::make_pair(CONFIGURED_IP_RETRIEVAL_ERROR,
								  std::make_pair("Failed to retrieve the configured IP for Scout",
										   " Failed to retrieve the configured IP for Scout.")));
	
	tempMap.insert(std::make_pair(NAT_IP_ADDRESS_CHECK_ERROR,
								  std::make_pair("Configured NAT IP is not present in IP address list",
											"Configure correct IP address")));
	
	m_errorLookUpMap.insert(std::make_pair(CONFIGURED_IP_CHECK,tempMap));
	tempMap.clear();

	tempMap.insert(std::make_pair(VACP_PROCESS_CREATION_ERROR,
								  std::make_pair("Failed to spawn vacp process",
											"Please check the binary vacp binary available in the installation path. \
                                            If available, report to Scout support team")));
	tempMap.insert(std::make_pair(VACP_TAG_ISSUE_ERROR,
								  std::make_pair("Failed to issue tag",
                                           "Tag issue is not been successful.  Following could be one of the reasons: \
											1.VSS provided compatibility with Scout \
											2.Scout filter driver is not in the data mode \
											3.Application has too many databases and volumes \
											4.Application VSS writer service is not running \
											Please check for the above. For more help, contact Scout Support.")));

	m_errorLookUpMap.insert(std::make_pair(CONSISTENCY_TAG_CHECK,tempMap));
	tempMap.clear();
		
	tempMap.insert(std::make_pair(VOLUME_CAPACITY_ERROR,
								  std::make_pair("Target volume capacities are not in sync with source volume capacities",
											"Please create same no.of volumes with identical capacities at target. \
                                            Create one volume for retention. The capacity of the same should match retention policy requirements.")));
	
	m_errorLookUpMap.insert(std::make_pair(VOLUME_CAPACITY_CHECK,tempMap));
	tempMap.clear();
	
	tempMap.insert(std::make_pair(SQL_DATA_ROOT_ERROR,
								  std::make_pair("Source and target SQL instance data roots should be the same path",
											"Reconfigure SQL instance at target to use the same data root path as source")));
	m_errorLookUpMap.insert(std::make_pair(SQL_DATAROOT_CHECK,tempMap));
	tempMap.clear();
	
	tempMap.insert(std::make_pair(SCR_ENABLED_ERROR,
								  std::make_pair("SCR Enabled on the source cluster",
											"Please disable SCR configuration on source server to continue protection")));
	
	m_errorLookUpMap.insert(std::make_pair(SCR_ENABLED_CHECK,tempMap));
	tempMap.clear();
	
	tempMap.insert(std::make_pair(DUPLICATE_MAILSTORE_COPY_ERROR, 
								  std::make_pair("Duplicate Mailstore Copy Error ",
											"Please supress this error")));

	m_errorLookUpMap.insert(std::make_pair(SINGLE_MAILSTORE_COPY_CHECK,tempMap));
	tempMap.clear();
    tempMap.insert(std::make_pair(DYNAMIC_DNS_UPDATE_ERROR, 
                                    std::make_pair("Dynamic Dns Update Error", 
                                            "Please turn off dynamic update on each network interface to continue protection."))) ;
    m_errorLookUpMap.insert(std::make_pair(DYNAMIC_DNS_UPDATE_CHECK,tempMap));
	tempMap.clear();
    
    tempMap.insert(std::make_pair(DNS_AVAILABILITY_ERROR, 
								  std::make_pair("DNS not available",
											"Make sure DNS is reachable to continue protection")));
	m_errorLookUpMap.insert(std::make_pair(DNS_AVAILABILITY_CHECK,tempMap));
	tempMap.clear();
	
	tempMap.insert(std::make_pair(DC_AVAILABILITY_ERROR, 
								  std::make_pair("DC not available",
											"Make sure dc is reachable to continue protection")));
	m_errorLookUpMap.insert(std::make_pair(DC_AVAILABILITY_CHECK,tempMap));
	tempMap.clear();
	tempMap.insert(std::make_pair(HOST_DC_ERROR, 
								  std::make_pair("Host is not DC",
											"Host itself is a domain controller. Re-configure to continue protection")));
	m_errorLookUpMap.insert(std::make_pair(HOST_DC_CHECK,tempMap));
	tempMap.clear();
    tempMap.insert(std::make_pair(SQL_INSTANCENAME_MISMATCH_ERROR, 
								  std::make_pair("SQL Instances name at source and target are not same",
											"Re-configure SQL instance at target for failover to work")));
	m_errorLookUpMap.insert(std::make_pair(SQL_INSTANCE_CHECK,tempMap));
    tempMap.clear();
    tempMap.insert(std::make_pair(CACHE_DIR_CAPACITIY_ERROR, 
                                    std::make_pair("Cache Drive has not enough free space",
                                        "Make additional space available, configure a different cache directory path")));
    m_errorLookUpMap.insert(std::make_pair(CACHE_DIR_CAPACITY_CHECK,tempMap));
    tempMap.clear();

	tempMap.insert(std::make_pair(VSS_ROLEUP_ERROR, 
                                  std::make_pair("VSS Roll Up Check",
                                     "Checks Whether the VSS supported host fix s available or not.")));
    m_errorLookUpMap.insert(std::make_pair(VSS_ROLEUP_CHECK,tempMap));
    tempMap.clear();
    
    tempMap.insert(std::make_pair(PAGE_FILE_VOLUME_ERROR, 
                                  std::make_pair("Applications volumes contains Page file configuration",
                                     "Ensure no Page file is configured on the primary server volumes.")));
    m_errorLookUpMap.insert(std::make_pair(PAGE_FILE_VOLUME_CHECK,tempMap));
    tempMap.clear();

    tempMap.insert(std::make_pair(DATABASE_NOT_RUNNING,
                				  std::make_pair("Oracle Database is not running",
                    				  "Start the oracle database")));
    m_errorLookUpMap.insert(std::make_pair(DB_INSTALL_CHECK,tempMap));
    tempMap.clear();

    tempMap.insert(std::make_pair(DUMMY_LUNZ_NOT_FOUND_ERROR,
                				  std::make_pair("Appliance Target LUNZ is not visible.",
                    				  "Verify the zoning between Primary server and Appliance.")));
    m_errorLookUpMap.insert(std::make_pair(DUMMY_LUNZ_CHECK,tempMap));
    tempMap.clear();

    tempMap.insert(std::make_pair(ORACLE_RECOVERY_LOG_ERROR,
                				  std::make_pair("Oracle Archive Log not enabled.", 
                    				  "Enable Oracle Archive logs.")));
    m_errorLookUpMap.insert(std::make_pair(DB_INSTALL_CHECK,tempMap));
    tempMap.clear();

    tempMap.insert(std::make_pair(DATABASE_NOT_SHUTDOWN,
                				  std::make_pair("Oracle Database is not shutdown",
                    				  "Shutdown the oracle database")));
    m_errorLookUpMap.insert(std::make_pair(DB_SHUTDOWN_CHECK,tempMap));
    tempMap.clear();

    tempMap.insert(std::make_pair(TARGET_VOLUME_IN_USE_ERROR,
                				  std::make_pair("Secondary server volumes are in use.",
                    				  "Unmount filesystems or remove from volume managers.")));
    m_errorLookUpMap.insert(std::make_pair(TARGET_VOLUME_IN_USE_CHECK,tempMap));
    tempMap.clear();

	/*tempMap.insert(std::make_pair(EXCHANGE_MTA_MISMATCH_ERROR,
								  std::make_pair("MTA status is not matching",
											"MTA is enabled in Source & in Target its disabled or vice-versa. \
                                            Enable or Disable MTA in both source & target.")));
	m_errorLookUpMap.insert(std::make_pair(MTA_MISMATCH_CHECK,tempMap));
	tempMap.clear();*/

    tempMap.insert(std::make_pair(CM_MIN_SPACE_PER_PAIR_ERROR, 
                                    std::make_pair("Cache Drive has not enough free space",
                                        "Make additional space available, configure a different cache directory path")));
    m_errorLookUpMap.insert(std::make_pair(CM_MIN_SPACE_PER_PAIR_CHECK,tempMap));
	tempMap.clear();

    tempMap.insert(std::make_pair(DB2_DATABASE_NOT_ACTIVE,
                				  std::make_pair("DB2 Database is not active",
                    				  "activate the Db2 Database")));
    m_errorLookUpMap.insert(std::make_pair(DB_ACTIVE_CHECK,tempMap));
    tempMap.clear();
    
    tempMap.insert(std::make_pair(DB2_INSTANCE_NOT_SHUTDOWN,
                				  std::make_pair("DB2 database instance is not shutdown",
                    				  "Deactivate the DB2 database Instance")));
    m_errorLookUpMap.insert(std::make_pair(DB2_SHUTDOWN_CHECK,tempMap));
    tempMap.clear();

    tempMap.insert(std::make_pair(DB2_CONSISTENCY_CHECK_ERROR,
                				  std::make_pair("DB2 Consistency validation is failed",
                    				  "Cannot freeze vxfs filesystem to issue tags")));
    m_errorLookUpMap.insert(std::make_pair(DB2_CONSISTENCY_TAG_CHECK,tempMap));
    tempMap.clear();

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


std::pair<std::string, std::string> Validator::GetNameAndDescription(InmRuleIds ruleId)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::string ruleName ;
	std::string ruleDescr ;
    std::pair<std::string, std::string> nameDescriptionPair ;
	if(m_DescriptionLookUpMap.find(ruleId) != m_DescriptionLookUpMap.end())
    {
		DebugPrintf(SV_LOG_DEBUG,"RuleId found in Map\n");
        nameDescriptionPair = m_DescriptionLookUpMap.find(ruleId)->second ;
	}
	else
	{
        DebugPrintf(SV_LOG_WARNING, "Unknown Rule Id %d\n", ruleId) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

    return nameDescriptionPair ;
}
std::pair<std::string,std::string> Validator::GetErrorData(InmRuleIds ruleId, InmRuleErrorCode errCode)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::pair<std::string,std::string> retPair;
	std::map<InmRuleErrorCode, std::pair<std::string, std::string> > tempMap;
	if(m_errorLookUpMap.find(ruleId) != m_errorLookUpMap.end())
	{
		DebugPrintf(SV_LOG_DEBUG,"RuleId found in Map\n");
		tempMap = m_errorLookUpMap.find(ruleId)->second;
		if(tempMap.find(errCode) != tempMap.end())
		{
			DebugPrintf(SV_LOG_DEBUG,"ErrCode found in Map\n");		
			retPair = tempMap.find(errCode)->second;
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG,"ErrCode not found in Map\n");
		}
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,"RuleId not found in Map\n");
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	
	return retPair;
}


std::string Validator::getErrorString(InmRuleErrorCode errCode)
{
	std::string ErrorString = "UNKOWN_ERROR";
	switch(errCode)
	{
		case INM_ERROR_NONE:
			ErrorString = "INM_ERROR_NONE";
			  break;
		case RULE_PASSED:
			ErrorString = "RULE_PASSED";
			  break;
		case HOSTNAME_MATCH_ERROR:
			ErrorString = "HOSTNAME_MATCH_ERROR";
			  break;
		case GET_VOLUME_NAME_FAILED_ERROR:
			ErrorString = "GET_VOLUME_NAME_FAILED_ERROR";
			  break;
		case NESTED_MOUNT_POINT_ERROR:
			ErrorString = "NESTED_MOUNT_POINT_ERROR";
			  break;
		case APPLICATION_DATA_ON_SYSTEM_DRIVE_ERROR:
			ErrorString = "APPLICATION_DATA_ON_SYSTEM_DRIVE_ERROR";
			  break;
		case MULTI_NIC_VALIDATION_ERROR:
			ErrorString = "MULTI_NIC_VALIDATION_ERROR";
			  break;
		case LCR_ENABLED_ERROR:
			ErrorString = "LCR_ENABLED_ERROR";
			  break;
		case CCR_ENABLED_ERROR:
			ErrorString = "CCR_ENABLED_ERROR";
			  break;
		case VERSION_MISMATCH_ERROR:
			ErrorString = "VERSION_MISMATCH_ERROR";
			  break;
		case EXCHANGE_ADMINGROUP_MISMATCH_ERROR:
			ErrorString = "EXCHANGE_ADMINGROUP_MISMATCH_ERROR";
			  break;
		case EXCHANGE_DC_ERROR:
			ErrorString = "EXCHANGE_DC_ERROR";
			  break;
		case SERVICE_NOT_FOUND:
			ErrorString = "SERVICE_NOT_FOUND";
			  break;
		case SERVICE_STATUS_ERROR:
			ErrorString = "SERVICE_STATUS_ERROR";
			  break;
		case SQL_DB_ONLINE_ERROR:
			ErrorString = "SQL_DB_ONLINE_ERROR";
			  break;
		case FIREWALL_STATUS_CHECK_ERROR:
			ErrorString = "FIREWALL_STATUS_CHECK_ERROR";
			  break;
		case DYNAMIC_DNS_UPDATE_ERROR:
			ErrorString = "DYNAMIC_DNS_UPDATE_ERROR";
			  break;
		case GET_DOMAIN_ERROR:
			ErrorString = "GET_DOMAIN_ERROR";
			  break;
		case DNS_SERVER_QUERY_ERROR:
			ErrorString = "DNS_SERVER_QUERY_ERROR";
			break;
		case DNS_SERVER_REDIRECT_ERROR:
			ErrorString = "DNS_SERVER_REDIRECT_ERROR";
			break;
		case DNS_UPDATE_ERROR:
			ErrorString = "DNS_UPDATE_ERROR";
			break;
		case AD_PRIVILEGES_CHECK_FAILED_ERROR:
			ErrorString = "AD_PRIVILEGES_CHECK_FAILED_ERROR";
			break;
		case CONFIGURED_IP_RETRIEVAL_ERROR:
			ErrorString = "CONFIGURED_IP_RETRIEVAL_ERROR";
			break;
		case NAT_IP_ADDRESS_CHECK_ERROR:
			ErrorString = "NAT_IP_ADDRESS_CHECK_ERROR";
			break;
		case VACP_PROCESS_CREATION_ERROR:
			ErrorString = "VACP_PROCESS_CREATION_ERROR";
			  break;
		case VACP_TAG_ISSUE_ERROR:
			ErrorString = "VACP_TAG_ISSUE_ERROR";
			break;
		case VOLUME_CAPACITY_ERROR:
			ErrorString = "VOLUME_CAPACITY_ERROR";
			break;
		case SQL_DATA_ROOT_ERROR:
			ErrorString = "SQL_DATA_ROOT_ERROR";
			break;
		case SCR_ENABLED_ERROR:
			ErrorString = "SCR_ENABLED_ERROR";
			break;
		case RULE_STAT_NONE:
			ErrorString = "RULE_STAT_NONE";
			break;
		case DUPLICATE_MAILSTORE_COPY_ERROR:
			ErrorString = "DAG_ERROR";
			break;
		case DNS_AVAILABILITY_ERROR:
			ErrorString = "DNS_AVAILABILITY_ERROR";
			break;
		case DC_AVAILABILITY_ERROR:
			ErrorString = "DC_AVAILABILITY_ERROR";
			break;
		case HOST_DC_ERROR:
			ErrorString = "HOST_DC_ERROR";
			break;
        case SQL_INSTANCENAME_MISMATCH_ERROR:
			ErrorString = "SQL_INSTANCENAME_MISMATCH_ERROR";
			break;    
        case CACHE_DIR_CAPACITIY_ERROR:
            ErrorString = "CACHE_DIR_CAPACITIY_ERROR";
            break; 
		case VSS_ROLEUP_ERROR:
            ErrorString = "VSS_ROLEUP_ERROR";
            break; 
        case PAGE_FILE_VOLUME_ERROR:
            ErrorString = "PAGE_FILE_VOLUME_ERROR";
            break;
        case DATABASE_NOT_RUNNING:
            ErrorString = "ORACLE_DATABASE_NOT_RUNNING";
            break;
		/*case EXCHANGE_MTA_MISMATCH_ERROR:
			ErrorString = "EXCHANGE_MTA_MISMATCH_ERROR";
			  break;*/
	    case CM_MIN_SPACE_PER_PAIR_ERROR:
            ErrorString = "CM_MIN_SPACE_PER_PAIR_ERROR";
            break;
        case DB2_DATABASE_NOT_ACTIVE:
            ErrorString = "DB2_DATABASE_NOT_ACTIVE";
            break;
        case DB2_INSTANCE_NOT_SHUTDOWN:
            ErrorString = "DB2_INSTANCE_NOT_SHUTDOWN";
            break;
        case DB2_CONSISTENCY_CHECK_ERROR:
            ErrorString = "DB2_CONSISTENCY_CHECK_ERROR";
            break;

	}
	return ErrorString;
}

std::string Validator::getInmRuleIdStringFromID(InmRuleIds id)
{
	std::string IDString = "UNKOWN_ID";
	switch(id)
	{
		case HOST_NAME_CHECK:
			IDString = "HOST_NAME_CHECK";
			  break;
		case NESTED_MOUNTPOINT_CHECK:
			IDString = "NESTED_MOUNTPOINT_CHECK";
			  break;
		case SYSREM_DRIVE_DATA_CHECK:
			IDString = "SYSREM_DRIVE_DATA_CHECK";
			  break;
		case MULTIPLE_NIC_CHECK:
			IDString = "MULTIPLE_NIC_CHECK";
			  break;
		case LCR_ENABLED_CHECK:
			IDString = "LCR_ENABLED_CHECK";
			  break;
		case CCR_ENABLED_CHECK:
			IDString = "CCR_ENABLED_CHECK";
			  break;
		case SCR_ENABLED_CHECK:
			IDString = "SCR_ENABLED_CHECK";
			  break;
		case VERSION_MISMATCH_CHECK:
			IDString = "VERSION_MISMATCH_CHECK";
			  break;
		case ADMINISTRATIVE_GROUPS_MISMATCH_CHECK:
			IDString = "ADMINISTRATIVE_GROUPS_MISMATCH_CHECK";
			  break;
		case DOMAIN_MISMATCH_CHECK:
			IDString = "DOMAIN_MISMATCH_CHECK";
			  break;
		case SERVICE_STATUS_CHECK:
			IDString = "SERVICE_STATUS_CHECK";
			  break;
		case SQL_DB_STATUS_CHECK:
			IDString = "SQL_DB_STATUS_CHECK";
			  break;
		case FIREWALL_CHECK:
			IDString = "FIREWALL_CHECK";
			  break;
		case DOMAIN_CHECK:
			IDString = "DOMAIN_CHECK";
			  break;
		case DNS_UPDATE_CHECK:
			IDString = "DNS_UPDATE_CHECK";
			  break;
		case AD_UPDATE_CHECK:
			IDString = "AD_UPDATE_CHECK";
			  break;
		case CONFIGURED_IP_CHECK:
			IDString = "CONFIGURED_IP_CHECK";
			  break;
		case CONSISTENCY_TAG_CHECK:
			IDString = "CONSISTENCY_TAG_CHECK";
			  break;
		case VOLUME_CAPACITY_CHECK:
			IDString = "VOLUME_CAPACITY_CHECK";
			  break;
		case SQL_DATAROOT_CHECK:
			IDString = "SQL_DATAROOT_CHECK";
			  break;
		case SINGLE_MAILSTORE_COPY_CHECK:
			IDString = "SINGLE_MAILSTORE_COPY_CHECK";
			break;
		case DNS_AVAILABILITY_CHECK:
			IDString = "DNS_AVAILABILITY_CHECK";
			break;
		case DC_AVAILABILITY_CHECK:
			IDString = "DC_AVAILABILITY_CHECK";
			break;
		case HOST_DC_CHECK:
			IDString = "HOST_DC_CHECK";
			break;
		case DYNAMIC_DNS_UPDATE_CHECK:
			IDString = "DYNAMIC_DNS_UPDATE_CHECK";
			break;
        case SQL_INSTANCE_CHECK:
            IDString = "SQL_INSTANCE_CHECK";
			break;
        case CACHE_DIR_CAPACITY_CHECK:
            IDString = "CACHE_DIR_CAPACITY_CHECK";
			break;
		case VSS_ROLEUP_CHECK:
            IDString = "VSS_ROLEUP_CHECK";
        case PAGE_FILE_VOLUME_CHECK:
			IDString = "PAGE_FILE_VOLUME_CHECK";
			break;
		case DB_INSTALL_CHECK:
			IDString = "DB_INSTALL_CHECK";
			break;
		case DUMMY_LUNZ_CHECK:
			IDString = "DUMMY_LUNZ_CHECK";
			break;
		case DB_SHUTDOWN_CHECK:
			IDString = "DB_SHUTDOWN_CHECK";
			break;
		case TARGET_VOLUME_IN_USE_CHECK:
			IDString = "TARGET_VOLUME_IN_USE_CHECK";
			break;
		/*case MTA_MISMATCH_CHECK:
			IDString = "MTA_MISMATCH_CHECK";
			break;*/
		case CM_MIN_SPACE_PER_PAIR_CHECK:
            IDString = "CM_MIN_SPACE_PER_PAIR_CHECK";
			break;
		case DB_ACTIVE_CHECK:
			IDString = "DB_ACTIVE_CHECK";
			break;
		case DB2_CONSISTENCY_TAG_CHECK:
			IDString = "DB2_CONSISTENCY_TAG_CHECK";
			  break;
		case DB2_SHUTDOWN_CHECK:
			IDString = "DB2_SHUTDOWN_CHECK";
			break;

	}
	return IDString;
}

void Validator::convertToRedinessCheckStruct(ReadinessCheck& rdyChek)
{

    //TODO : Fix steps
	rdyChek.m_properties.insert(std::make_pair("RuleId", boost::lexical_cast<std::string>(getRuleId()))) ;
    rdyChek.m_properties.insert(std::make_pair("ErrorCode", boost::lexical_cast<std::string>(getRuleExitCode()))) ;
    rdyChek.m_properties.insert(std::make_pair("RuleMessage",getRuleMessage()));
    rdyChek.m_properties.insert(std::make_pair("RuleStatus", statusToString())) ;
}
