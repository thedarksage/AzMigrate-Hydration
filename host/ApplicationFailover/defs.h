#ifndef DEFS__H
#define DEFS__H

#include "svtypes.h"
#include "globs.h"
#include <string>
#include <set>
#include <iostream>
#include "../common/version.h"

//Failover Script names
#define SOURCE_FAILOVER_SCRIPT_EXCHANGE_PLANNED		"src_failover_exch_planned.bat"
#define SOURCE_FAILOVER_SCRIPT_EXCHANGE_UNPLANNED	"src_failover_exch_unplanned.bat"
#define TARGET_FAILOVER_SCRIPT_EXCHANGE_PLANNED		"tgt_failover_exch_planned.bat"
#define TARGET_FAILOVER_SCRIPT_EXCHANGE_UNPLANNED	"tgt_failover_exch_unplanned.bat"
#define SOURCE_FAILOVER_SCRIPT_MSSQL_PLANNED		"src_failover_sql_planned.bat"
#define SOURCE_FAILOVER_SCRIPT_MSSQL_UNPLANNED		"src_failover_sql_unplanned.bat"
#define TARGET_FAILOVER_SCRIPT_MSSQL_PLANNED		"tgt_failover_sql_planned.bat"
#define TARGET_FAILOVER_SCRIPT_MSSQL_UNPLANNED		"tgt_failover_sql_unplanned.bat"
#define SOURCE_FAILOVER_SCRIPT_FILESERVER_PLANNED	"src_failover_fileserver_planned.bat"
#define SOURCE_FAILOVER_SCRIPT_FILESERVER_UNPLANNED	"src_failover_fileserver_unplanned.bat"
#define TARGET_FAILOVER_SCRIPT_FILESERVER_PLANNED	"tgt_failover_fileserver_planned.bat"
#define TARGET_FAILOVER_SCRIPT_FILESERVER_UNPLANNED	"tgt_failover_fileserver_unplanned.bat"

//Failback Script names
#define SOURCE_FAILBACK_SCRIPT_EXCHANGE_PLANNED		"src_failback_exch_planned.bat"
#define SOURCE_FAILBACK_SCRIPT_EXCHANGE_UNPLANNED	"src_failback_exch_unplanned.bat"
#define TARGET_FAILBACK_SCRIPT_EXCHANGE_PLANNED		"tgt_failback_exch_planned.bat"
#define TARGET_FAILBACK_SCRIPT_EXCHANGE_UNPLANNED	"tgt_failback_exch_unplanned.bat"
#define SOURCE_FAILBACK_SCRIPT_MSSQL_PLANNED		"src_failback_sql_planned.bat"
#define SOURCE_FAILBACK_SCRIPT_MSSQL_UNPLANNED		"src_failback_sql_unplanned.bat"
#define TARGET_FAILBACK_SCRIPT_MSSQL_PLANNED		"tgt_failback_sql_planned.bat"
#define TARGET_FAILBACK_SCRIPT_MSSQL_UNPLANNED		"tgt_failback_sql_unplanned.bat"
#define SOURCE_FAILBACK_SCRIPT_FILESERVER_PLANNED	"src_failback_fileserver_planned.bat"
#define SOURCE_FAILBACK_SCRIPT_FILESERVER_UNPLANNED	"src_failback_fileserver_unplanned.bat"
#define TARGET_FAILBACK_SCRIPT_FILESERVER_PLANNED	"tgt_failback_fileserver_planned.bat"
#define TARGET_FAILBACK_SCRIPT_FILESERVER_UNPLANNED	"tgt_failback_fileserver_unplanned.bat"

//Exchange Consistency script name
#define EXCHANGE_CONSISTENCY_NAME		"exchange_consistency_validation.bat"
#define EXCHANGE2007_CONSISTENCY_NAME	"exchange2007_consistency_validation.bat"
#define EXCHANGE2010_CONSISTENCY_NAME	"exchange2010_consistency_validation.bat"
#define EXCHANGE_CONSISTENCY_VERIFIER	"ExchangeVerify.vbs"

// Exchange Service Names
#define EXCHANGE_INFORMATION_STORE		"MSExchangeIS"
#define EXCHANGE_MANAGEMENT				"MSExchangeMGMT"
#define EXCHANGE_MTA					"MSExchangeMTA"
#define EXCHANGE_ROUTING_ENGINE			"RESvc"
#define EXCHANGE_SYSTEM_ATTENDANT		"MSExchangeSA"
#define EXCHANGE_REPLICATION			"MSExchangeRepl"
#define EXCHANGE_TRANSPORT				"MSExchangeTransport"

// MSSQL Services names
#define MSSQL_SERVICE					"MSSQLSERVER"
#define MSSQL_SERVER_AGENT				"SQLSERVERAGENT"
// MSSQL database configuration file. The acutal file name will be prefixed with the host name
// For examples host "prodserver" will have the name "prodserver_sql_config.dat"
#define MSSQL_CONFIG_FILE				"_sql_config.dat"
#define EXCHANGE_CONFIG_FILE			"_exchange_config.dat"
#define EXCHANGE_LOG_CONFIG_FILE		"_exchange_log_config.dat"
#define CONSISTENCY_TAG_FILE			"consistency_tag.txt"
#define RETLOG_CONFIG_FILE				"_retlog_config.dat"
#define FILESERVER_CONFIG_FILE			"_fileserver_config.dat"
#define AUDIT_CONFIG_FILE				"_AuditFile.conf"
// MSSQL database configuration file Tokens
#define HOST_NAME						"HOST NAME"
#define DATABASE_NAME					"DATABASE NAME"
#define DATABASE_CONFIG					"DATABASE"
#define FILENAME						"FILE NAME"
#define VOLUMES							"VOLUMES"
#define VERSION							"VERSION"
#define DISCOVERY_VERSION				"DISCOVERY VERSION"
#define DISCOVERY_VERSION_STRING		"1.2"
// ServicePrincipalName attributes required for authenticating an outlook client 2003 and above with an exchange server
#define EXCHANGE_SERVICE_PRINCIPAL_NAME		"servicePrincipalName"
#define EXCHANGE_HTTP						"HTTP"

// Application Names
#define EXCHANGE		"Exchange"
#define EXCHANGE2007	"Exchange2007"
#define EXCHANGE2010	"Exchange2010"
#define MSSQL			"SQL"
#define MSSQL2005		"SQL2005"
#define MSSQL2008		"SQL2008"
#define MSSQL2012		"SQL2012"
#define FILESERVER      "fileserver"

//Type of tag to be used for rollback
#define FS_TAGTYPE				     "FS"
#define USERDEFINED_TAGTYPE          "USERDEFINED"
#define EXCHANGEIS_TAGTYPE		     "ExchangeIS"
#define EXCHANGEREPLICATION_TAGTYPE  "ExchangeRepl"
#define EXCHANGE_TAGTYPE			 "Exchange"
#define EXCHANGE2007_TAGTYPE	     "Exchange2007"
#define EXCHANGE2010_TAGTYPE	     "Exchange2010"
#define MSSQL_TAGTYPE				 "SQL"
#define MSSQL2005_TAGTYPE			 "SQL2005"
#define MSSQL2008_TAGTYPE			 "SQL2008"
#define MSSQL2012_TAGTYPE			 "SQL2012"

//Application Failover/Failback Command Line Options
#define OPTION_FAILOVER					"-failover"
#define OPTION_SOURCE_HOST				"-s"
#define OPTION_TARGET_HOST				"-t"
#define OPTION_SOURCE_HOST_IP			"-ip"
#define OPTION_APPLICATION				"-app"
#define OPTION_BUILTIN					"-builtIn"
#define OPTION_DISCOVER					"-discover"
#define OPTION_PLANNED					"-planned"
#define OPTION_UNPLANNED				"-unplanned"
#define OPTION_TAG						"-tag"
#define OPTION_VERIFY_CONSISTENCY		"-verifyconsistency"
#define OPTION_HOST						"-host"
#define DOMAINNAME						"-domain"
#define STORAGE_GROUP					"-sg"
#define TIMESTAMP						"-time"
#define OPTION_PF_SOURCE_HOST			"-pfs"
#define OPTION_PF_TARGET_HOST			"-pft"
#define OPTION_APPTAG					"-apptag"
#define OPTION_RETAIN_DUPLICATE_SPN		"-retainDuplicateSPN"
#define OPTION_HELP1					"-h"
#define OPTION_HELP2					"-help"
#define OPTION_TAGTYPE					"-tagType"
#define OPTION_CONSISTENCY				"-applytag"
#define OPTION_BOOKMARK					"-bookmark"
#define OPTION_TAGTYPE					"-tagType"  
#define OPTION_VIRTUALSERVER			"-virtualserver"
#define OPTION_SOURCEVS					"-sourcevs"
#define OPTION_TARGETVS					"-targetvs"
#define OPTION_MTASERVER				"-mta"
#define OPTION_DO_AD_REPLICATION		"-doadreplication"
#define OPTION_NO_AD_REPLICATION		"-noadreplication"   //Backward comaptibility
#define OPTION_NO_DNS_FAILOVER			"-nodnsfailover"
#define OPTION_FAILBACK					"-failback"
#define OPTION_FASTFAILBACK				"-fastfailback"
#define OPTION_CRASH_CONSISTENCY		"-crashconsistency"
#define OPTION_USE_LOCAL_ACCOUNT		"-useuseraccount" 
#define OPTION_VERIFY_PERMISSIONS		"-verifypermissions"
#define OPTION_AUDIT					"-audit"  
#define OPTION_HIDEVOLUME				"-hidevolume"
#define OPTION_CONFIG			        "-config"
#define OPTION_REG						"-reg"
#define OPTION_PASSIVE_COPY				"-passivecopy"
#define OPTION_SOURCE_CAS				"-sourceCAS"
#define OPTION_TARGET_CAS				"-targetCAS"
#define OPTION_SOURCE_HT				"-sourceHT"
#define OPTION_TARGET_HT				"-targetHT"
//#define OPTION_USE_CNAME_DNS_RECORD     "-useCnameDnsRecord"
#define OPTION_USE_HOST_DNS_RECORD      "-useHostDnsRecord"
#define OPTION_IN_RESYNC				"-InResync"
#define OPTION_SLD_ROOT_DOMAIN			"-sldRoot" //Single label dns name used for root domain
//directory
#define AUDIT_DIRECTORY				"Audit"
#define AUDIT_SECTION				"FAILOVER_AUDIT"
#define AUDIT_KEY					"VOLUMES"

// Failover Tags
#define EXCHANGE_CONSISTENCY_POINT	"Exchange consistency point"
// Application type for user defined tags
#define USER_DEFINED_APPLICATON		"UserDefined"

enum operation { LIST_EVENT, ROLLBACK_VOLUME, TAG_ACCURACY, COMMON_TAG ,COMMON_TIME };

//Cluster Resources for Applications
#define RESOURCE_EXCHANGE_VIRTUAL_SERVER	"NetworkName"
#define RESOURCE_EXCHANGE_INFORMATON_STORE	"Microsoft Exchange Information Store"
#define RESOURCE_EXCHANGE_SYSTEM_ATTENDANT	"Microsoft Exchange System Attendant"
#define RESOURCE_EXCHANGE_MTA				"Microsoft Exchange Message Transfer Agent"
#define RESOURCE_SQL_SERVER					"SQL Server"
#define CLUSTER_NETWORK_NAME_RESOURCE_TYPE		 "Network Name"
#define CLUSTER_FILESHARE_RESOURCE_TYPE		     "File Share"
#define CLUSTER_RESOURCE_NETOWRK_NAME_PROPERTY	 "Name"
#define CLUSTER_RESOURCE_FILE_SHARE_PROPERTY     "Path"

#define RESOURCE_OFFLINE_ATTEMPTS			100	//Attempts of 3 seconds each

// Script Location Directory relative to Agent Install Path
#define FAILOVER_DIRECTORY					   "Failover"
#define DATA_DIRECTORY						   "Data"
#define CONSISTENCY_DIRECTORY				   "Consistency"
#define CONSISTENCY_VALIDATION_DIRECTORY	   "ConsistencyValidation"
#define SEQUENCE_NUM_FILE					   "SequenceNum.status"
#define EXCHANGE_CONSISTENCY_SCRIPT			   "exchange_consistency.bat"
#define EXCHANGE2007_CONSISTENCY_SCRIPT	       "exchange2007_consistency.bat"
#define EXCHANGE2010_CONSISTENCY_SCRIPT	       "exchange2010_consistency.bat"
#define EXCHANGE_CONSISTENCY_FSTAG_SCRIPT	   "exchange_consistency_fstag.bat"
#define EXCHANGE2007_CONSISTENCY_FSTAG_SCRIPT  "exchange2007_consistency_fstag.bat"
#define EXCHANGE2010_CONSISTENCY_FSTAG_SCRIPT  "exchange2010_consistency_fstag.bat"
#define SQL_CONSISTENCY_SCRIPT				   "Sql_consistency.bat"
#define SQL2005_CONSISTENCY_SCRIPT		       "Sql2005_consistency.bat"
#define SQL2005_CONSISTENCY_FSTAG_SCRIPT	   "Sql2005_consistency_fstag.bat"
#define SQL2008_CONSISTENCY_SCRIPT		       "Sql2008_consistency.bat"
#define SQL2008_CONSISTENCY_FSTAG_SCRIPT	   "Sql2008_consistency_fstag.bat"
#define SQL2012_CONSISTENCY_SCRIPT		       "Sql2012_consistency.bat"
#define SQL2012_CONSISTENCY_FSTAG_SCRIPT	   "Sql2012_consistency_fstag.bat"

#define SQL_CONSISTENCY_FSTAG_SCRIPT	       "Sql_consistency_fstag.bat"
#define AUTO_GENERATED_TAG_DATA			       "consistency_tag.txt"
#define FILESERVER_CONSISTENCY_FSTAG_SCRIPT	   "FileServer_consistency_fstag.bat"

//global functions
size_t findPosIgnoreCase(const std::string szSource, const std::string szSearch);
std::string convertoToLower(const std::string szStringToConvert, size_t length);

//registry keys
const char SV_ROOT_KEY[] = "SOFTWARE\\SV Systems";

// Depenedent Services Configuration File 
#define STOP_KEY		    "STOP"
#define START_KEY			"START"
#define SQLINSTANCE_KEY	    "MSSQL_INSTANCE_NAMES"
#define MSSQLINSTANCENAME   "SQL SERVER NAME"
#define SQL2000_SECTION     "SQL2000"
#define SQL2005_SECTION     "SQL2005"
#define SQL2008_SECTION     "SQL2008"
#define SQL2012_SECTION     "SQL2012"
#define EXCHANGE_SECTION    "EXCHANGE"
#define EXCHANGE2010_SECTION "EXCHANGE2010"
#define EXCHANGE2007_SECTION "EXCHANGE2007"
#define EXCHANGE2003_SECTION "EXCHANGE2003"
#define FILESERVER_SECTION	"FILESERVER"
#define VACPEXE             "vacp.exe"
//for NAT configuration
#define APPLICATION_DATA_DIRECTORY    "Application Data"
#define ETC_DIRECTORY                 "etc"
#define DRSCOUT_CONF_FILE             "drscout.conf"
#define DEPENDENT_SERVICES_CONF_FILE  "FailoverServices.conf"
#define DRSCOUT_CONF_FILE_SECTION     "vxagent"
#define DRSCOUT_CONF_HOSTNAME_KEY     "UseConfiguredHostname"
#define DRSCOUT_CONF_HOSTNAME         "ConfiguredHostname"
#define NATING_INFO                   "NATINFO"
#define OPTION_RETENTIONLOG           "-retention"
#define NAME_SIZE                     512
#define MSSQL_DIRECTORY               "Mssql"
#define FAILBACK_IP_ADDRESS           "Failback_IP_Address"
#define CNAME_RECORD_FAILBACK         "CNameFailback"

//Prefix for Job result file
#define PREFIX_FAILOVER	"Failover_"

// Discovery machanism
enum discoverFrom { QUERYSOURCE, READ_TRANSMITTED_FILE, READ_DISCOVERY_FILE };
enum operationToPerformOnCluster { FIND_VIRTUAL_SERVER_GROUP = 0, FIND_FILESHARES_VOLUMES_OF_VIRTUALSERVER, OFFLINE_VIRTUALSERVER_RESOURCE};

#define REGISTER_BUFSIZE 16384
#define DELAY 30

//Lanmanserver Registry Names
#define LANMANSERVER_SHARE_REG_KEY			"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\lanmanserver\\Shares"
#define LANMANSERVER_SECURITY_REG_KEY	    L"[HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\lanmanserver\\Shares\\Security]"
#define REGISTRY_EXPORTED_FILE_EXT			".reg"


class CopyrightNotice
{	
public:
	CopyrightNotice()
	{
		std::cout<<"\n"<<INMAGE_COPY_RIGHT<<"\n\n";
	}
};
#endif
