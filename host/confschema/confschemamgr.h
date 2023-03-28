#ifndef CONF__SCHEMA__MGR__H__
#define CONF__SCHEMA__MGR__H__
#include <boost/shared_ptr.hpp>
#include "confschema.h"

const char ALERTS_GROUP[] = "Alerts";
const char POLICY_GROUP[] = "Policy";
const char POLICY_INSTANCE_GROUP[] = "PolicyInstance";
const char PROTECTIONPAIRS_GROUP[] = "ProtectionPairs";
const char RETENTIONLOG_POLICY_GROUP[] = "RetentionLogPolicy";
const char PLAN_GROUP[] = "Plan";
const char SNAPSHOTS_GROUP[] = "Snapshots";
const char SNAPSHOT_INSTANCES_GROUP[] = "SnapshotInstances";
const char VOLUME_GROUP[] = "Volume";
const char REPOSITORIES_GROUP[] = "Repositories";
const char AGENTCONFIG_GROUP[] = "AgentConfiguration" ;
const char AUDITCONFIG_GROUP[] = "Audit" ;
const char EVENTQUEUE_GROUP[]  = "Events" ;
class ConfSchemaMgr ;
typedef boost::shared_ptr<ConfSchemaMgr> ConfSchemaMgrPtr ;
class ConfSchemaMgr
{
private:
    static ConfSchemaMgrPtr m_confSchemaMgrPtr ;
    ConfSchema::GroupNames_t groupnames;
    /* TODO: Unneeded groups are also filled in for now.
    *       All these should go away with good interface
    *       and hiding unneeded groups */
    bool IsGroupChanged( const std::string& grpName,
                         const ConfSchema::Group& grp ) ;
    ConfSchema::Groups_t m_Groups;
    ConfSchema::Groups_t m_OrigGroups ;
    bool ShouldBackupConfigFile( const std::string& schemaType ) ;
    void WriteConfFile( const std::string& file, 
                        const std::list<std::string>& bkpfiles,
                        const ConfSchema::Groups_t& grps) ;
	std::string m_repoPathForHost, m_repoLocation ;
public:
    std::list<std::string> GetSchemaFilePathForBkp( const std::string& schemaType ) ;
    bool ShouldReconstructRepo() ;
    /* This should return failure only if read fails;
    * if schema not yet created, then it should return
    * success but with empty schema; caller will fill 
    * and handle all cases of being empty (first time)
    * and all ... */

    /* TODO: use the variable number of args */
    ConfSchemaMgr(const std::string& repolocation,  const std::string& repositoryLocation ) ;

    void PrintGroups(void);
    ConfSchema::Groups_t& getGroups(void);
    std::string GetConfigNameInHumanFormat(const std::string& schemaType) ;
    std::set<std::string> GetAllGroups( const std::set<std::string>& grps ) ;
    std::string GetSchemaFilePath(const std::string& schemaType);
    bool checkFileExistence(const std::string& FileName);
	static ConfSchemaMgrPtr CreateInstance(const std::string& repolocation, const std::string& repoLocationForHost) ;
    static ConfSchemaMgrPtr GetInstance( ) ;
    bool WriteConfSchema();
    void setRepoLocationForHost( const std::string& repoLocation ) ;
    const std::string& getRepoLocationForHost() const ;
    void GetConfigurationFiles( std::vector<std::string>& configFiles) ;
    void ReadConfSchema(const std::string& grpName) ;
	std::string getConfigDirLocation() const ;
	void setRepoLocation(const std::string& repolocation)  ;
	const std::string& getRepoLocation() const ;
    void Reset() ;
    std::set<std::string> GetModifiedGroups() ;
	bool GetCorruptedGroups (std::set <std::string> &corruptedGroups);
};

#endif /* CONF__SCHEMA__MGR__H__ */
