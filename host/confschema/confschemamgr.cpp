#include "confschemamgr.h"
#include "logger.h"
#include "localconfigurator.h"
#include "confschemaparser.h"
#include "portablehelpers.h"
#include "inmstrcmp.h"
#include "inmageex.h"
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>
ConfSchemaMgrPtr ConfSchemaMgr::m_confSchemaMgrPtr ;
ConfSchemaMgrPtr ConfSchemaMgr::CreateInstance(const std::string& repolocation,
											   const std::string& repoLocationForHost )
{
	m_confSchemaMgrPtr.reset( new ConfSchemaMgr(repolocation, repoLocationForHost ) ) ;
    if( !m_confSchemaMgrPtr )
    {
		throw "Failed to create the confschema readerwriter" ;
	}	
	m_confSchemaMgrPtr->Reset() ;
    return m_confSchemaMgrPtr ;
    }
ConfSchemaMgrPtr ConfSchemaMgr::GetInstance()
    {
    if( !m_confSchemaMgrPtr )
    {
		throw "ConfSchemaMgr Instance not yet created" ;
    }
    return m_confSchemaMgrPtr ;
}
ConfSchemaMgr::ConfSchemaMgr(const std::string& repolocation,  const std::string& repositoryPathForHost )
{
    if( !repositoryPathForHost.empty() )
    {
		std::string configdir = repositoryPathForHost ;
		if( repositoryPathForHost.length() > 0 && 
			repositoryPathForHost[repositoryPathForHost.length() - 1 ] != ACE_DIRECTORY_SEPARATOR_CHAR )
		{
			configdir += ACE_DIRECTORY_SEPARATOR_CHAR ;
		}
		DebugPrintf(SV_LOG_DEBUG, "ConfSchemaMgr repo location %s, repository path for host %s\n", 
			repolocation.c_str(), repositoryPathForHost.c_str()) ;
		setRepoLocation(repolocation) ;
        setRepoLocationForHost( configdir ) ;
    }
}

bool ConfSchemaMgr::GetCorruptedGroups (std::set <std::string> &corruptedGroups)
{
	std::string configDir = getConfigDirLocation() ;
    bool bRet = false ;
    ACE_stat stat ;
    ACE_DIR * ace_dir = NULL ;
    ACE_DIRENT * dirent = NULL ;
    if( configDir.length() > 1 && configDir[configDir.length() - 1 ] == ACE_DIRECTORY_SEPARATOR_CHAR )
    {
        configDir.erase( configDir.length() - 1 ) ;
    }
    ace_dir = sv_opendir(configDir.c_str()) ;
    if( ace_dir != NULL )
    {
        do
        {
            dirent = ACE_OS::readdir(ace_dir) ;
            bool isDir = false ;
            if( dirent != NULL )
            {
                std::string fileName = ACE_TEXT_ALWAYS_CHAR(dirent->d_name);
                if( strcmp(fileName.c_str(), "." ) == 0 || strcmp(fileName.c_str(), ".." ) == 0 )
                {
                    continue ;
                }
                if( fileName.find("volumes_config.xml.corrupt") != std::string::npos )
                {
                    corruptedGroups.insert(REPOSITORIES_GROUP);
                    corruptedGroups.insert(VOLUME_GROUP);
                    bRet = true ;
                }
                if (fileName.find("scenario_config.xml.corrupt") != std::string::npos )
                {
                    corruptedGroups.insert(PLAN_GROUP);
                    bRet = true;
                }
                if (fileName.find ("protectionpair_config.xml.corrupt") != std::string::npos)
                {
                    corruptedGroups.insert(PROTECTIONPAIRS_GROUP);
                    bRet = true;
                }
                if (fileName.find ("policy_config.xml.corrupt") != std::string::npos)
                {
                    corruptedGroups.insert(POLICY_GROUP);
                    bRet = true;
                }
                if (fileName.find ("policyinstance_config.xml.corrupt") != std::string::npos)
                {
                    corruptedGroups.insert(POLICY_INSTANCE_GROUP);
                    bRet = true;
                }
                if (fileName.find ("audit_config.xml.corrupt") != std::string::npos)
                {
                    corruptedGroups.insert(AUDITCONFIG_GROUP);
                    bRet = true;
                }
                if (fileName.find ("alert_config.xml.corrupt") != std::string::npos)
                {
                    corruptedGroups.insert(ALERTS_GROUP);
                    bRet = true;
                }
                if (fileName.find ("agent_config.xml.corrupt") != std::string::npos)
                {
                    corruptedGroups.insert(AGENTCONFIG_GROUP);
                    bRet = true;
                }
                if (fileName.find("event_queue.xml.corrupt") != std::string::npos)
                {
                    corruptedGroups.insert(EVENTQUEUE_GROUP) ;
                    bRet = true ;
                }
            }
        } while ( dirent != NULL  ) ;
		ACE_OS::closedir( ace_dir ) ;
    }
    return bRet;
}
bool ConfSchemaMgr::ShouldReconstructRepo()
{
	std::string configDir = getConfigDirLocation() ;
    bool ret = false ;
    ACE_stat stat ;
    ACE_DIR * ace_dir = NULL ;
    ACE_DIRENT * dirent = NULL ;
    if( configDir.length() > 1 && configDir[configDir.length() - 1 ] == ACE_DIRECTORY_SEPARATOR_CHAR )
    {
        configDir.erase( configDir.length() - 1 ) ;
    }
    ace_dir = sv_opendir(configDir.c_str()) ;
    if( ace_dir != NULL )
    {
        do
        {
            dirent = ACE_OS::readdir(ace_dir) ;
            bool isDir = false ;
            if( dirent != NULL )
            {
                std::string fileName = ACE_TEXT_ALWAYS_CHAR(dirent->d_name);
                if( strcmp(fileName.c_str(), "." ) == 0 || strcmp(fileName.c_str(), ".." ) == 0 )
                {
                    continue ;
                }
                if( fileName.find(".corrupt") != std::string::npos )
                {
                    ret = true ;
                }
            }
        } while ( dirent != NULL  ) ;
    }
    return ret ;
}

void ConfSchemaMgr::setRepoLocationForHost( const std::string& repoLocation )
{
	m_repoPathForHost = repoLocation ;
    return ;
}

const std::string& ConfSchemaMgr::getRepoLocationForHost() const
{
    return m_repoPathForHost ;
}
void ConfSchemaMgr::setRepoLocation(const std::string& repolocation) 
{
	m_repoLocation = repolocation ;
}
const std::string& ConfSchemaMgr::getRepoLocation() const
{
	return m_repoLocation ;
}

std::string ConfSchemaMgr::getConfigDirLocation() const
{
	std::string configDirPath = m_repoPathForHost ;
	if( configDirPath.length() > 0 && configDirPath[configDirPath.length() - 1 ] != ACE_DIRECTORY_SEPARATOR_CHAR )
	{
		configDirPath += ACE_DIRECTORY_SEPARATOR_CHAR ;
	}
	return configDirPath + "configstore" ;
}

void ConfSchemaMgr::ReadConfSchema(const std::string& grpname)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bret = true;
    ConfSchema::Groups_t grpmap ;
    std::string filename = GetSchemaFilePath(grpname) ; 
    if(checkFileExistence(filename))
    {
        ConfSchema::ConfSchemaValueObject csvo;
        csvo.XmlConfileLoad(filename);
        grpmap = csvo.getGroupMap();
        ConfSchema::ConstGroupsIter_t grpIter = grpmap.begin() ;
        while( grpIter != grpmap.end() )
        {
            m_Groups.insert( *grpIter ) ;
            m_OrigGroups.insert( *grpIter ) ;
            grpIter++ ;
        }
    }
    if( grpmap.find( grpname ) == grpmap.end() )
    {
        m_Groups.insert(std::make_pair(grpname,ConfSchema::Group()));
        m_OrigGroups.insert( std::make_pair(grpname,ConfSchema::Group()) ) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


void ConfSchemaMgr::Reset()
{
    m_Groups.clear() ;
    m_OrigGroups.clear() ;
}

bool ConfSchemaMgr::IsGroupChanged( const std::string& grpname,
                                   const ConfSchema::Group& grp )
{
    bool bRet = false ;
    if( m_OrigGroups.find( grpname ) != m_OrigGroups.end() )
    {
        ConfSchema::Group& origGrp = m_OrigGroups.find( grpname )->second ;
        if( !(grp == origGrp) )
        {
            bRet = true ;
        }
    }
    else
    {
        bRet = true ;
    }
    return bRet ;
}
std::set<std::string> ConfSchemaMgr::GetModifiedGroups(void)
{
    ConfSchema::GroupsIter_t currentGrpIter = m_Groups.begin() ;
    std::set<std::string> modifiedGrps ;
    while( currentGrpIter != m_Groups.end() )
    {
        if( IsGroupChanged( currentGrpIter->first, currentGrpIter->second ) )
        {
            modifiedGrps.insert( currentGrpIter->first ) ;
        }
        //If there is no backup file present on the system we should create a one.
        //This could happen during the upgrade case.
        if( ShouldBackupConfigFile(currentGrpIter->first ) )
        {
            std::list<std::string> bkpFiles ;
            bkpFiles = GetSchemaFilePathForBkp( currentGrpIter->first ) ;
            std::list<std::string>::const_iterator bkfileIter ;
            bkfileIter = bkpFiles.begin() ;
            while( bkfileIter != bkpFiles.end() )
            {
                if( !boost::filesystem::exists( *bkfileIter ) )
                {
                    modifiedGrps.insert( currentGrpIter->first ) ;
                    break ;
                }
                bkfileIter++ ;
            }
        }
        currentGrpIter++ ;
    }
    modifiedGrps = GetAllGroups( modifiedGrps ) ;
    return modifiedGrps ;
}
void ConfSchemaMgr::WriteConfFile( const std::string& file, 
                                  const std::list<std::string>& bkpfiles,
                                  const ConfSchema::Groups_t& grps)
{
    bool bret = false;
    std::string errmsg;
    ConfSchema::ConfSchemaValueObject csvo;
    std::list<std::string>::const_iterator bkpFileIter ;
    bkpFileIter = bkpfiles.begin() ;
    while( bkpFileIter != bkpfiles.end() )
    {
        csvo.setGroupMap(grps);
        csvo.XmlConfileSave(*bkpFileIter);
        bret = SyncFile(*bkpFileIter, errmsg);
        if (!bret)
        {
            throw INMAGE_EX("failed to sync config file: ")(*bkpFileIter)(" with error message: ")(errmsg);
        }
        bkpFileIter++ ;
    }
    csvo.setGroupMap(grps);
    csvo.XmlConfileSave(file);
    bret = SyncFile(file, errmsg);
    if (!bret)
    {
        throw INMAGE_EX("failed to sync config file: ")(file)(" with error message: ")(errmsg);
    }
}
bool ConfSchemaMgr::WriteConfSchema()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bret = false;
    std::set<std::string> grpNames = GetModifiedGroups( ) ;
    std::string errmsg;
    std::set<std::string>::const_iterator grpNameIter = grpNames.begin();
    std::map<std::string, ConfSchema::Groups_t> fileNamegrpsMap ;
    std::map<std::string, std::list<std::string> > bkpFilesMap ;
    while(grpNameIter != grpNames.end())
    {
        std::string SchemaFilePath = GetSchemaFilePath(*grpNameIter);
        std::string bkpFile ;
        if( ShouldBackupConfigFile( *grpNameIter ) )
        {
            std::list<std::string> bkpFiles ;
            bkpFiles = GetSchemaFilePathForBkp( *grpNameIter ) ;
            bkpFilesMap.insert( std::make_pair( SchemaFilePath, bkpFiles ) ) ;
        }
        if( fileNamegrpsMap.find( SchemaFilePath ) == fileNamegrpsMap.end() )
        {
            fileNamegrpsMap.insert( std::make_pair( SchemaFilePath, ConfSchema::Groups_t() ) ) ;
        }

        ConfSchema::Groups_t& groupsByFile = fileNamegrpsMap.find( SchemaFilePath ) ->second ;
        groupsByFile.insert( std::make_pair( *grpNameIter,
            m_Groups.find(*grpNameIter)->second)) ;

        grpNameIter++;
    }
    std::map<std::string, ConfSchema::Groups_t>::iterator fileNameGrpsIter ;
    fileNameGrpsIter = fileNamegrpsMap.begin() ;
    while( fileNameGrpsIter != fileNamegrpsMap.end() )
    {
        std::list<std::string> bkpfiles ; 
        if( bkpFilesMap.find( fileNameGrpsIter->first ) !=  bkpFilesMap.end() )
        {
            bkpfiles = bkpFilesMap.find( fileNameGrpsIter->first )->second ;
        }
		DebugPrintf(SV_LOG_DEBUG, "Persising %s\n", fileNameGrpsIter->first.c_str()) ;
        WriteConfFile( fileNameGrpsIter->first, bkpfiles, fileNameGrpsIter->second ) ;
        fileNameGrpsIter++ ;
    }
    m_Groups.clear() ;
    m_OrigGroups.clear() ;
    return bret;
}

ConfSchema::Groups_t& ConfSchemaMgr::getGroups(void)
{
    return m_Groups;
}

void ConfSchemaMgr::PrintGroups(void)
{
    DebugPrintf(SV_LOG_DEBUG, "mgr groups:\n");
    for (ConfSchema::GroupsIter_t i = m_Groups.begin(); i != m_Groups.end(); ++i)
    {
        DebugPrintf(SV_LOG_DEBUG, "type: %s\n", i->first.c_str());
        i->second.Print();
    }
    DebugPrintf(SV_LOG_DEBUG, "mgr groups:\n");
}
std::set<std::string> ConfSchemaMgr::GetAllGroups( const std::set<std::string>& grps )
{
    std::set<std::string> groups ;
    groups = grps ;
    if( grps.find( VOLUME_GROUP ) != grps.end() || 
        grps.find( REPOSITORIES_GROUP ) != grps.end())
    {
        groups.insert( VOLUME_GROUP ) ;
        groups.insert( REPOSITORIES_GROUP ); 
    }
    if( grps.find( SNAPSHOTS_GROUP ) != grps.end() || 
        grps.find( SNAPSHOT_INSTANCES_GROUP ) != grps.end())
    {
        groups.insert( SNAPSHOTS_GROUP ) ;
        groups.insert( SNAPSHOT_INSTANCES_GROUP ) ;
    }
    return groups ;
}

bool ConfSchemaMgr::ShouldBackupConfigFile( const std::string& schemaType )
{
    if(InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),PLAN_GROUP)==0 || 
        InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),PROTECTIONPAIRS_GROUP)==0  ||
        InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),AGENTCONFIG_GROUP)==0 ||
        InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),REPOSITORIES_GROUP) == 0 ||
        InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),VOLUME_GROUP) == 0 )
    {
        return true ;
    }
    else
    {
        return false ;
    }
}

std::list<std::string> ConfSchemaMgr::GetSchemaFilePathForBkp( const std::string& schemaType )
{
    std::list<std::string> bkpfiles;
	std::string configFile  ;
    configFile = GetSchemaFilePath( schemaType ) ;
    boost::algorithm::replace_all( configFile, "configstore", "configstore_bkp" ) ;
    std::string bkpdir = getConfigDirLocation() ;
    boost::algorithm::replace_all( bkpdir, "configstore", "configstore_bkp" ) ;
    SVMakeSureDirectoryPathExists( bkpdir.c_str() ) ;
    boost::algorithm::replace_all( configFile, ".xml", ".bkp.1" ) ;
    bkpfiles.push_back( configFile ) ;
    boost::algorithm::replace_all( configFile, ".bkp.1", ".bkp.2" ) ;
    bkpfiles.push_back( configFile ) ;
    return bkpfiles ;
}

std::string ConfSchemaMgr::GetConfigNameInHumanFormat(const std::string& schemaType)
{
    std::string ConfigNameInHumanFormat;

    if((InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),VOLUME_GROUP)==0)||
        (InmStrCmp<NoCaseCharCmp>(schemaType,REPOSITORIES_GROUP)==0))
    {
        ConfigNameInHumanFormat =  "Volume";
    }
    else if(InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),PLAN_GROUP)==0)
    {
        ConfigNameInHumanFormat =  "Scenario";
    }
    else if(InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),PROTECTIONPAIRS_GROUP)==0)
    {
        ConfigNameInHumanFormat = "Protection";
    }
    else if( InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),POLICY_GROUP)==0 )
    {
        ConfigNameInHumanFormat = "Policy";
    }
    else if(InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),POLICY_INSTANCE_GROUP)==0 )
    {
        ConfigNameInHumanFormat = "Policy instance";
    }
    else if(InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),ALERTS_GROUP)==0)
    {
        ConfigNameInHumanFormat = "Alert";
    }
    else if(InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),RETENTIONLOG_POLICY_GROUP)==0)
    {
        ConfigNameInHumanFormat =  "Retention";
    }
    else if((InmStrCmp<NoCaseCharCmp>(schemaType,SNAPSHOTS_GROUP)==0)||
        (InmStrCmp<NoCaseCharCmp>(schemaType,SNAPSHOT_INSTANCES_GROUP)==0))
    {
        ConfigNameInHumanFormat =  "Snapshots";
    }
    else if( InmStrCmp<NoCaseCharCmp>( schemaType, AGENTCONFIG_GROUP) == 0 )
    {
        ConfigNameInHumanFormat =  "Agent" ;
    }
    else if( InmStrCmp<NoCaseCharCmp>( schemaType, AUDITCONFIG_GROUP ) == 0 )
    {
        ConfigNameInHumanFormat =  "Audit" ;
    }
    else if( InmStrCmp<NoCaseCharCmp>( schemaType, EVENTQUEUE_GROUP) == 0 )
    {
        ConfigNameInHumanFormat =  "Event" ;
    }
    
    return ConfigNameInHumanFormat;
}

std::string ConfSchemaMgr::GetSchemaFilePath(const std::string& schemaType)
{
    std::string SchemaFileName;

    if((InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),VOLUME_GROUP)==0)||
        (InmStrCmp<NoCaseCharCmp>(schemaType,REPOSITORIES_GROUP)==0))
    {
        SchemaFileName =  "volumes_config.xml";
    }
    else if(InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),PLAN_GROUP)==0)
    {
        SchemaFileName =  "scenario_config.xml";
    }
    else if(InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),PROTECTIONPAIRS_GROUP)==0)
    {
        SchemaFileName = "protectionpair_config.xml";
    }
    else if( InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),POLICY_GROUP)==0 )
    {
        SchemaFileName = "policy_config.xml";
    }
    else if(InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),POLICY_INSTANCE_GROUP)==0 )
    {
        SchemaFileName = "policyinstance_config.xml";
    }
    else if(InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),ALERTS_GROUP)==0)
    {
        SchemaFileName = "alert_config.xml";
    }
    else if(InmStrCmp<NoCaseCharCmp>(schemaType.c_str(),RETENTIONLOG_POLICY_GROUP)==0)
    {
        SchemaFileName =  "retentionsettings_config.xml";
    }
    else if((InmStrCmp<NoCaseCharCmp>(schemaType,SNAPSHOTS_GROUP)==0)||
        (InmStrCmp<NoCaseCharCmp>(schemaType,SNAPSHOT_INSTANCES_GROUP)==0))
    {
        SchemaFileName =  "snapshots_config.xml";
    }
    else if( InmStrCmp<NoCaseCharCmp>( schemaType, AGENTCONFIG_GROUP) == 0 )
    {
        SchemaFileName =  "agent_config.xml" ;
    }
    else if( InmStrCmp<NoCaseCharCmp>( schemaType, AUDITCONFIG_GROUP ) == 0 )
    {
        SchemaFileName =  "audit_config.xml" ;
    }
    else if( InmStrCmp<NoCaseCharCmp>( schemaType, EVENTQUEUE_GROUP ) == 0 )
    {
        SchemaFileName =  "event_queue.xml" ;
    }
    
	std::string configDir = getConfigDirLocation() ;
	SchemaFileName = configDir + ACE_DIRECTORY_SEPARATOR_STR_A + SchemaFileName ;
    return SchemaFileName;
}

bool ConfSchemaMgr::checkFileExistence(const std::string& FileName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    struct stat stFileInfo;
    bool blnReturn = true;
    int intStat;

    //Attempt to get the file attributes
    intStat = stat(FileName.c_str(),&stFileInfo);
    if(intStat == 0) 
    {
        //got the file attributes so the file is exist
        blnReturn = true;
        DebugPrintf(SV_LOG_DEBUG,"File :%s is available\n",FileName.c_str());
    } 
    else 
    {
        blnReturn = false;
        DebugPrintf(SV_LOG_DEBUG,"File :%s is not available\n",FileName.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return blnReturn ;
}

void ConfSchemaMgr::GetConfigurationFiles( std::vector<std::string>& configFiles)
{
	std::string configDirLocation = getConfigDirLocation() ;
    std::string conffile = configDirLocation ;
	conffile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	conffile += "volumes_config.xml" ;
    if( checkFileExistence( conffile ) ) 
    {
        configFiles.push_back( conffile) ;
    }
    conffile = configDirLocation ;
	conffile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	conffile += "scenario_config.xml";
    if( checkFileExistence( conffile ) ) 
    {
        configFiles.push_back( conffile) ;
    }
    conffile = configDirLocation ;
	conffile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	conffile +=  "protectionpair_config.xml";
    if( checkFileExistence( conffile ) ) 
    {
        configFiles.push_back( conffile) ;
    }
    conffile = configDirLocation ;
	conffile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	conffile +=  "policy_config.xml" ;
    if( checkFileExistence( conffile ) ) 
    {
        configFiles.push_back( conffile) ;
    }
    conffile = configDirLocation ;
	conffile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	conffile +=  "policyinstance_config.xml" ;
    if( checkFileExistence( conffile ) ) 
    {
        configFiles.push_back( conffile) ;
    }
    conffile = configDirLocation ;
	conffile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	conffile += "alert_config.xml" ;;  
    if( checkFileExistence( conffile ) ) 
    {
        configFiles.push_back( conffile) ;
    }
    conffile = configDirLocation ;
	conffile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	conffile += "retentionsettings_config.xml" ;
    if( checkFileExistence( conffile ) ) 
    {
        configFiles.push_back( conffile) ;
    }
    conffile = configDirLocation ;
	conffile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	conffile += "snapshots_config.xml" ; ; 
    if( checkFileExistence( conffile ) ) 
    {
        configFiles.push_back( conffile) ;
    }
    conffile = configDirLocation ;
	conffile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	conffile += "agent_config.xml" ;
    if( checkFileExistence( conffile ) ) 
    {
        configFiles.push_back( conffile) ;
    }
    conffile = configDirLocation ;
	conffile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	conffile += "audit_config.xml" ;
    if( checkFileExistence( conffile ) ) 
    {
        configFiles.push_back( conffile) ;
    }
    conffile = configDirLocation ;
	conffile += ACE_DIRECTORY_SEPARATOR_CHAR ;
	conffile += "event_queue.xml" ;
    if( checkFileExistence( conffile ) ) 
    {
        configFiles.push_back( conffile) ;
    }
    std::vector<std::string>::const_iterator conffileIter = configFiles.begin() ;
    std::vector<std::string> bkpfiles ;
    while( conffileIter != configFiles.end() )
    {
        std::string file = *conffileIter ;
        boost::algorithm::replace_all( file, "configstore", "configstore_bkp" ) ;
        boost::algorithm::replace_all( file, ".xml", ".bkp.1" ) ;
        if( checkFileExistence( file ) )
        {
            bkpfiles.push_back( file ) ;
        }
        boost::algorithm::replace_all( file, ".bkp.1", ".bkp.2" ) ;
        if( checkFileExistence( file ) )
        {
            bkpfiles.push_back( file ) ;
        }
        boost::algorithm::replace_all( file, ".bkp.2", ".bkp.3" ) ;
        if( checkFileExistence( file ) )
        {
            bkpfiles.push_back( file ) ;
        }
        boost::algorithm::replace_all( file, ".bkp.3", ".bkp.4" ) ;
        if( checkFileExistence( file ) )
        {
            bkpfiles.push_back( file ) ;
        }
        boost::algorithm::replace_all( file, ".bkp.4", ".bkp.5" ) ;
        if( checkFileExistence( file ) )
        {
            bkpfiles.push_back( file ) ;
        }
        conffileIter++ ;
    }
    std::vector<std::string>::const_iterator bkpfileIter = bkpfiles.begin() ;
    while( bkpfileIter != bkpfiles.end() )
    {
        configFiles.push_back( *bkpfileIter ) ;
        bkpfileIter++ ;
    }
}
