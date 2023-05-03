//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdpcli.cpp
//
// Description: 
//

#include <sys/types.h>
#include <signal.h>
#include <exception>

#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/bind.hpp>

#include "configwrapper.h"
#include "cdpcli.h"
#include "cdpplatform.h"
#include "cdpcoalesce.h"
#include "cdplock.h"
#include "cdpvalidate.h"
#include "serializationtype.h"

#include "cdpfixdb.h"
#include <fstream>

#include "hostagenthelpers_ported.h"
#include "globs.h"


#include <ace/config.h>
#include <ace/Time_Value.h>
#include <ace/OS_NS_sys_time.h>
#include <ace/Process_Manager.h>
#include <ace/OS_NS_sys_stat.h>
#include <ace/OS_NS_unistd.h>

// new configurator files
#include "configurator2.h"
#include "cdpplatform.h"
#include "logger.h"  

#ifdef SV_WINDOWS
#include "virtualvolume.h"
#include <atlconv.h>
#include "autoFS.h"
#include "csjobprocessor.h"
#else
#include "cdpcliminor.h"
#include "unixvirtualvolume.h"
#endif

#include "localconfigurator.h"

#include "inmageex.h"
#include "inmcommand.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "VacpUtil.h"
#include "HandlerCurl.h"
#include "cdpcoalesce.h"
#include "cdpdrtd.h"
#include "rpcconfigurator.h"
#include "operatingsystem.h"
#include "AgentHealthIssueNumbers.h"
#include "replicationpairoperations.h"

#include "cdpv3metadatafile.h"

#include "inmsafecapis.h"
#include "registermt.h"

using namespace std;

static bool IsRepeatingSlash( char ch1, char ch2 ) 
{ return ((ACE_DIRECTORY_SEPARATOR_CHAR_A == ch1) && (ch2 == ch1)); }

/*
* FUNCTION NAME :  CDPCli::CDPCli
*
* DESCRIPTION : cdpcli constructor 
*
* INPUT PARAMETERS : argc - no. of arguments
*                    argv - arguments based on operation
* 
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/

CDPCli::CDPCli(int argc, char ** argv)
:argc(argc), argv(argv)
{
    m_bconfig = false;
    m_blocalLog = false;
    setIgnoreCxErrors(true);
#ifdef SV_UNIX
    set_resource_limits();
#endif
    progress_callback_fn = 0;
    progress_callback_param = 0;
    m_write_to_console = true;
    m_quit = false;

    // following variables are added for allowing 
    // CDPCLI as API users to perform initialization
    // and de-initialization in calling code

    m_skip_log_init = false;
    m_skip_tal_init = false;
    m_skip_config_init = false;
    m_skip_platformdeps_init = false;
    m_skip_quitevent_init = false;
}

/*
* FUNCTION NAME :  CDPCli::~CDPCli
*
* DESCRIPTION : cdpcli destructor 
*
*
* INPUT PARAMETERS : none
*                    
* 
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
CDPCli::~CDPCli()
{

}

/*
* FUNCTION NAME :  isCallerCli
*
* DESCRIPTION :  
*               Determine if the cdpcli process called by service or commandline
*
* INPUT PARAMETERS : none
*                    
* 
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : false if by service otherwise true
*
*/
bool CDPCli::isCallerCli()
{
    bool rv = true;
    std::string procname;
    procname = getParentProccessName();
    if ( !m_write_to_console || (procname.find("svagent") != std::string::npos))
    {
        rv = false;
    }
    return rv;
}
/*
* FUNCTION NAME :  CDPCli::init
*
* DESCRIPTION :  
*               initialize quit event
*               initialize local logging
*               intiialize configurator
*
* INPUT PARAMETERS : none
*                    
* 
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
bool CDPCli::init()
{
    bool callerCli = true;
    bool rv = true;
    callerCli = isCallerCli();
    do
    {
        if(!initializeTal())
        {
            rv = false;
            break;
        }
        MaskRequiredSignals();
        if(!SetupLocalLogging(callerCli))
        {
            rv = false;
            break;
        }

        if(!m_skip_quitevent_init)
        {
            if(!CDPUtil::InitQuitEvent(callerCli))
            {
                rv = false;
                break;
            }
        }

        InitializePlatformDeps();

        if(!InitializeConfigurator())
        {
            //rv = false;
            //break;
        }
    } while(0);

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::uninit
*
* DESCRIPTION :  
*               stop configurator
*               stop local logging
*               destroy quit event
*
* INPUT PARAMETERS : none
*                    
* 
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
bool CDPCli::uninit()
{
    StopConfigurator();

    if(!m_skip_quitevent_init)
    {
        CDPUtil::UnInitQuitEvent();
    }

    StopLocalLogging();

    if(!m_skip_tal_init)
    {
        tal::GlobalShutdown();
    }


    DeInitializePlatformDeps();
    return true;
}

/*
* FUNCTION NAME :  CDPCli::initializeTal
*
* DESCRIPTION : initialize transport layer 
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::initializeTal()
{
    bool rv = true;

    if(m_skip_tal_init)
        return true;

    try 
    {
        tal::GlobalInitialize();
        LocalConfigurator localConfigurator;

        //
        // PR # 6337
        // set the curl_verbose option based on settings in 
        // drscout.conf
        //
        tal::set_curl_verbose(localConfigurator.get_curl_verbose());

        Curl_setsendwindowsize(localConfigurator.getTcpSendWindowSize());
        Curl_setrecvwindowsize(localConfigurator.getTcpRecvWindowSize());
    }
    catch ( ContextualException& ce )
    {
        rv = false;
        std::cerr << FUNCTION_NAME << " encountered exception " << ce.what() << "\n";
    }
    catch( exception const& e )
    {
        rv = false;
        std::cerr << FUNCTION_NAME << " encountered exception " << e.what() << "\n";
    }
    catch ( ... )
    {
        rv = false;
        std::cerr << FUNCTION_NAME << " encountered unknown exception.\n";
    }

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::SetupLocalLogging
*
* DESCRIPTION : get the local logging settings from configurator 
*               and enable local logging
*
* INPUT PARAMETERS : bool callerCli
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::SetupLocalLogging(bool callerCli)
{
    bool rv = false;

    if(m_skip_log_init)
    {
        m_blocalLog = true;
        return true;
    }

    try 
    {

        LocalConfigurator localconfigurator;
        std::string sLogFileName = localconfigurator.getLogPathname()+ "cdpcli.log";
        SetLogFileName( sLogFileName.c_str() );
        SetLogLevel( localconfigurator.getLogLevel() );		
        SetLogHttpIpAddress(GetCxIpAddress().c_str());
        SetLogHttpPort( localconfigurator.getHttp().port );
        SetLogHostId( localconfigurator.getHostId().c_str() );
        SetLogRemoteLogLevel( localconfigurator.getRemoteLogLevel() );
		SetSerializeType( localconfigurator.getSerializerType() ) ;
		SetLogHttpsOption(localconfigurator.IsHttps());
        if(!callerCli)
        {
            SetDaemonMode();
        }
        m_blocalLog = true;
        rv = true;

    }
    catch(...) 
    {
        std::cerr << "Local Logging initialization failed.\n";
        m_blocalLog = false;
        rv = false;
    }

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::StopLocalLogging
*
* DESCRIPTION : close local log file
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::StopLocalLogging()
{
    if(!m_skip_log_init)
    {
        CloseDebug();
    }
    m_blocalLog = false;
    return true;
}

/*
* FUNCTION NAME :  CDPCli::InitializeConfigurator
*
* DESCRIPTION : initialize cofigurator to fetch initialsettings from 
*               local persistent store.
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/

bool CDPCli::InitializeConfigurator()
{
    bool rv = false;

    do
    {
        try 
        {
            m_configurator = NULL;
            LocalConfigurator lconfigurator; 
            ESERIALIZE_TYPE etype = lconfigurator.getSerializerType() ;
            std::string configcachepath ;

            if(!::InitializeConfigurator(&m_configurator, 
                USE_ONLY_CACHE_SETTINGS, etype, configcachepath ))
            {
                rv = false;
                break;
            }

            m_configurator->Start();
            m_bconfig = true;
            rv = true;
        } 

        catch ( ContextualException& ce )
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered exception %s.\n",
                FUNCTION_NAME, ce.what());
        }
        catch( exception const& e )
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered exception %s.\n",
                FUNCTION_NAME, e.what());
        }
        catch(...) 
        {
            rv = false;
            DebugPrintf(SV_LOG_ERROR, 
                "\n%s encountered unknown exception.\n",
                FUNCTION_NAME);
        }

    } while(0);

    if(!rv)
    {
        DebugPrintf(SV_LOG_INFO, 
            "Replication pair information is not available in local cache.\n"
            "Few cdpcli options will not work\n");
        m_bconfig = false;
    }

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::StopConfigurator
*
* DESCRIPTION : stop cofigurator 
*              
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/

bool CDPCli::StopConfigurator()
{
    if(m_configurator && m_bconfig)
    {
        if(!m_skip_config_init)
        {
            m_configurator ->Stop();
            delete m_configurator;
        }

        m_bconfig = false;
        m_configurator = NULL;
    }

    return true;
}

/*
* FUNCTION NAME : CDPCli::isTargetVolume
*
* DESCRIPTION : check if the specified volume is a target volume
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/

bool CDPCli::isTargetVolume(const std::string & volumename) const
{
    // 
    // Replication Pair configuration information is not available
    // we skip this check
    //
    if(!m_bconfig)
    {
        return false;
    }

    return (m_configurator->isTargetVolume(volumename));
}

/*
* FUNCTION NAME :  CDPCli::containsRetentionFiles
*
* DESCRIPTION : check if specified volume contains retention files 
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/

bool CDPCli::containsRetentionFiles(const std::string & volumename) const
{
    // 
    // Replication Pair configuration information is not available
    // we skip this check
    //
    if(!m_bconfig)
    {
        return false;
    }

    return (m_configurator->containsRetentionFiles(volumename));
}

/*
* FUNCTION NAME :  CDPCli::isProtectedVolume
*
* DESCRIPTION : check if the specified volume is a protected (filtered) volume
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/

bool CDPCli::isProtectedVolume(const std::string & volumename) const
{
    // 
    // Replication Pair configuration information is not available
    // we skip this check
    //
    if(!m_bconfig)
    {
        return false;
    }

    return (m_configurator->isProtectedVolume(volumename));
}

/*
* FUNCTION NAME :  CDPCli::isResyncing
*
* DESCRIPTION : check if specified volume is undergoing resync operation 
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/
bool CDPCli::isResyncing(const std::string & volumename) const
{
    // 
    // Replication Pair configuration information is not available
    // we skip this check
    //
    if(!m_bconfig)
    {
        return false;
    }

    return (m_configurator->isResyncing(volumename));
}

/*
* FUNCTION NAME :  CDPCli::getFSType
*
* DESCRIPTION : get the file system type on volume from configurator
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : filesystem type on the volume
*                "" if it is raw volume / information not available
*
*/
std::string CDPCli::getFSType(const std::string & volumename) const
{

    // 
    // Replication Pair configuration information is not available
    // we skip this check
    //
    if(!m_bconfig)
    {
        return "";
    }

    return(m_configurator ->getVxAgent().getSourceFileSystem(volumename));
}

SV_ULONGLONG CDPCli::getDiffsPendingInCX(const std::string & volumename) const
{
    // 
    // Replication Pair configuration information is not available
    // we skip this check
    //
    if(!m_bconfig)
    {
        return 0;
    }

    return (m_configurator->getDiffsPendingInCX(volumename));
}

SV_ULONGLONG CDPCli::getCurrentRpo(const std::string & volumename) const
{
    // 
    // Replication Pair configuration information is not available
    // we skip this check
    //
    if(!m_bconfig)
    {
        return 0;
    }

    return (m_configurator->getCurrentRpo(volumename));
}

SV_ULONGLONG CDPCli::getApplyRate(const std::string & volumename) const
{
    // 
    // Replication Pair configuration information is not available
    // we skip this check
    //
    if(!m_bconfig)
    {
        return 0;
    }

    return (m_configurator->getApplyRate(volumename));
}

/*
* FUNCTION NAME :  CDPCli::help
*
* DESCRIPTION : print the usage message
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
void CDPCli::help(int argc, char **argv)
{

    if(argc ==3)
    {
        usage(argv[0], argv[2], false);
    }
    else
    {
        usage(argv[0],"",false);
    }
}

/*
* FUNCTION NAME :  CDPCli::parseInput
*
* DESCRIPTION : parse the input and form the name/value pairs
*               
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
* 
*
* return value : none
*
*/
bool CDPCli::parseInput()
{
    bool rv = true;
	NVPairs::iterator iter;
    string args;
    args += " ";
    for (int i = 2; i < argc ; ++i)
    {
        args += argv[i];
        args += " ";
    }

    svector_t tokens = CDPUtil::split(args, " --", 0);
    //changes for #bugId: 12965

    if(tokens.size() > 0)
    {
        if((m_operation != "hide") && (m_operation != "unhide_ro") && (m_operation != "unhide_rw"))
        {
            if(!tokens[0].empty() && (tokens[0] != " "))
            {
                DebugPrintf(SV_LOG_ERROR, "Invalid option: %s \n\n",tokens[0].c_str());
                return false;
            }
        }
    }

    for ( size_t i = 1; i < tokens.size(); ++i)
    {
        CDPUtil::trim(tokens[i]);
        svector_t currarg = CDPUtil::split(tokens[i], "=", 2);
        if (currarg.size() != 2)
        {
            // 
            // special handling for single param options
            //
            CDPUtil::trim(tokens[i]);
            if(currarg.size() == 1 
                && find(singleoptions.begin(), singleoptions.end(), tokens[i].c_str()) != singleoptions.end())
            {
                nvpairs[tokens[i]]="yes";
                continue;
            }

            DebugPrintf(SV_LOG_ERROR, "syntax error near: %s \n\n", tokens[i].c_str());
            usage(argv[0]);
            rv = false;
            break;
        }

        if(find(validoptions.begin(), validoptions.end(), currarg[0]) == validoptions.end())
        {
            DebugPrintf(SV_LOG_ERROR, "Invalid option: %s \n\n", currarg[0].c_str());
            usage(argv[0]);
            rv = false;
            break;
        }

        if(nvpairs.find(currarg[0]) != nvpairs.end())
        {
            DebugPrintf(SV_LOG_ERROR, "%s  has been specified multiple times.\n\n", currarg[0].c_str());
            usage(argv[0]);
            rv = false;
            break;
        }
        CDPUtil::trim(currarg[1]);  

        //
        // Converting the relative path, symbolic link and hard link (only on linux)
        //
        std::string value = currarg[1] ;
        if( currarg[0] == "db" )
        {
            if( !resolvePathAndVerify( value ) )
            {
                DebugPrintf( SV_LOG_ERROR, "Invalid DB Path %s\n", currarg[1].c_str() ) ;
                rv = false ;
                break ;
            }     
        }

        nvpairs[currarg[0]] = value;
    }

    //#15949 : Adding showreplicationpairs and showsnapshotrequests options to cdpcli
    //Special Handling: For showreplication verify the parameters if custom settings is requested.
    if((m_operation == "showreplicationpairs" || m_operation == "showsnapshotrequests")&& nvpairs.find("custom")!=nvpairs.end())
    {
        if(nvpairs.find("ip") == nvpairs.end()
            || nvpairs.find("port") == nvpairs.end()
            || nvpairs.find("hostid") == nvpairs.end())
        {
            rv = false;
            usage(argv[0]);
        }
    }

	iter = nvpairs.find("loglevel");
	if(iter != nvpairs.end())
    {
		int loglevel = boost::lexical_cast<SV_UINT>(iter->second);
		DebugPrintf( SV_LOG_DEBUG, "loglevel %d\n", loglevel );
		if ( loglevel <= SV_LOG_ALWAYS && loglevel >= SV_LOG_DISABLE ) {
			SetLogLevel(loglevel);
		}
		else {
			DebugPrintf(SV_LOG_ERROR,"Invalid loglevel %d, range of loglevel from 0to7\n", loglevel );
			rv = false;
		}
    }

	iter = nvpairs.find("logpath");
	if(iter != nvpairs.end())
    {
		ACE_stat statBuff = {0};
		std::string filename=iter ->second;
        std::string dirpath = dirname_r(filename.c_str());
		SVERROR rc = SVMakeSureDirectoryPathExists( dirpath.c_str() ) ;
		if( rc.failed() )
		{
			DebugPrintf( SV_LOG_ERROR, "Invalid logpath %s provided. directory creation failed with error %s\n",
			dirpath.c_str(), rc.toString() ) ;
			rv = false;
		}
#ifdef SV_WINDOWS
		else {
			SVERROR::SVERROR_TYPE tag = rc.getTag();
			if ( rc.SVERROR_NTSTATUS == tag ) {
				DebugPrintf( SV_LOG_ERROR, "Invalid logpath %s provided. directory creation failed with error %s\n",
				dirpath.c_str(), rc.toString() ) ;
				rv = false;
			}
		}
#endif
	
		DebugPrintf( SV_LOG_DEBUG,"logpath %s\n", filename.c_str() );
		if ( rv != false ) {
			CloseDebug();
			SetLogFileName( filename.c_str() );
		}
    }

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::getCdpDbName
*
* DESCRIPTION : fetch the retention database path for the specified volume 
*               from local persistent store.
*
* INPUT PARAMETERS : target volume name
*
* OUTPUT PARAMETERS : retention database path
*
* NOTES :
* 
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::getCdpDbName(const std::string& input, std::string& dbpath)
{
    bool rv = true;

    // 
    // Replication Pair configuration information is not available
    //
    if(!m_bconfig)
    {
        DebugPrintf(SV_LOG_INFO, "Retention path for %s is not available.\n", input.c_str());
        return false;
    }

    string volname = input;
    if (IsReportingRealNameToCx())
    {
        GetDeviceNameFromSymLink(volname);
    }
    else
    {
        /** 
        * This code is for solaris.
        * If the user has given 
        * Real Name of device, get 
        * the link name of it and 
        * then proceed to process.
        * since CX is having only the
        * link name for solaris agent
        */
        std::string linkname = volname;
        if (GetLinkNameIfRealDeviceName(volname, linkname))
        {
            volname = linkname;
        } 
        else
        {
            DebugPrintf(SV_LOG_ERROR, "@LINE %d in FILE %s Invalid volume specified\n", LINE_NO, FILE_NAME);
            rv = false;
        }
    }

    if(volname != input)
    {
        DebugPrintf(SV_LOG_INFO,
            " %s is a symbolic link to %s \n\n", volname.c_str(), input.c_str());
    }

    FormatVolumeNameForCxReporting(volname);
    FirstCharToUpperForWindows(volname);

    //
    // note: configurator never throws any exception for getCdpDbName
    //
    dbpath = m_configurator->getCdpDbName(volname);
    if(dbpath != "")
    {
        // convert the path to real path (no symlinks)
        if( !resolvePathAndVerify( dbpath ) )
        {
            DebugPrintf( SV_LOG_INFO, 
                "Retention Database Path %s for volume %s is not yet created\n",
                dbpath.c_str(),  volname.c_str() ) ;
            rv = false ;
        }
    }

    return rv;
}


/*
* FUNCTION NAME :  CDPCli::getHostVolumeGroupSettings
*
* DESCRIPTION : fetch the host volume group settings 
*               from local persistent store.
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : host volume group settings
*
* NOTES :
* 
*
* return value : true on success, otherwise false
*
*/
bool CDPCli::getHostVolumeGroupSettings(HOST_VOLUME_GROUP_SETTINGS & settings)
{
    // 
    // Replication Pair configuration information is not available
    //
    if(!m_bconfig)
    {
        DebugPrintf(SV_LOG_INFO, "Replication Pair information is not available.\n");
        return false;
    }

    settings = m_configurator->getHostVolumeGroupSettings();  
    return true;
}

bool CDPCli::ShouldOperationRun(const std::string& volume, bool & shouldrun)
{
    if(!m_bconfig)
    {
        DebugPrintf(SV_LOG_INFO, "Replication Pair information is not available.\n");
        return false;
    }
    return CDPUtil::ShouldOperationRun((*m_configurator),volume,m_operation,shouldrun);
}

/*
* FUNCTION NAME :  CDPCli::listcommonpt
*
* DESCRIPTION : find the common crash consistent for all the replications 
*               in differential sync with media retention enabled.
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES : 
*
*
* return value : true on success, otherwise false
*
*/
bool CDPCli::listcommonpt()
{
    bool rv = true;
    vector<string> dbs;
    NVPairs::iterator iter;
    svector_t devicelist;
    bool newformat= false;

    do
    {
        string devicestr;
        iter = nvpairs.find("volumes");
        if(iter != nvpairs.end())
        {
            devicestr = iter ->second;
            devicelist = CDPUtil::split(devicestr,",");
            if(devicelist.size() == 0)
            {
                DebugPrintf(SV_LOG_INFO, "There is no volume specified with --volumelist option.\n");
                rv = false;
                break;
            }
            newformat = true;
            svector_t::iterator it = devicelist.begin();
            for(;it != devicelist.end();++it)
            {
                string dbpath;
                string device;
                device = (*it);
                CDPUtil::trim(device);
                bool shouldRun = true;
                bool foundstate = ShouldOperationRun(device,shouldRun);
                if(!shouldRun)
                {
                    DebugPrintf(SV_LOG_ERROR,
                        "The retention logs for volume %s are being moved. Please try after sometime. \n",
                        device.c_str());
                    return false;
                }

                if(!getCdpDbName(device,dbpath))
                {
                    DebugPrintf(SV_LOG_INFO, "Unable to get the db name from cache for the volume %s.\n",device.c_str());
                    return false;
                }
                if(dbpath == "") // "" means retention is not configured
                {
                    DebugPrintf(SV_LOG_INFO, "Retention is not enabled for the volume %s.\n",device.c_str());
                    return false;
                }
                dbs.push_back(dbpath);
            }
        }
        else
        {
            HOST_VOLUME_GROUP_SETTINGS hostVolumeGroupSettings; 

            if(!getHostVolumeGroupSettings(hostVolumeGroupSettings))
            {
                return false;
            }

            VOLUME_GROUP_SETTINGS::volumes_iterator volumeIter;
            VOLUME_GROUP_SETTINGS::volumes_iterator volumeEnd;

            HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator 
                volumeGroupIter(hostVolumeGroupSettings.volumeGroups.begin());
            HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator 
                volumeGroupEnd(hostVolumeGroupSettings.volumeGroups.end());

            for (/* empty */; volumeGroupIter != volumeGroupEnd; ++volumeGroupIter) 
            {
                volumeIter = (*volumeGroupIter).volumes.begin();
                volumeEnd = (*volumeGroupIter).volumes.end();
                if  (   TARGET == (*volumeGroupIter).direction && 
                    (   VOLUME_SETTINGS::SYNC_DIFF == (*volumeIter).second.syncMode)) 
                {
                    string dbpath;
                    string device;

                    VOLUME_SETTINGS volsettings = (*volumeIter).second;
                    device = volsettings.deviceName;
                    CDPUtil::trim(device);
                    bool shouldRun = true;
                    bool foundstate = ShouldOperationRun(device,shouldRun);
                    if(!shouldRun)
                    {
                        DebugPrintf(SV_LOG_ERROR,
                            "The retention logs for volume %s are being moved. Please try after sometime. \n",
                            device.c_str());
                        return false;
                    }

                    if(getCdpDbName(device,dbpath))
                    {
                        if(dbpath != "") // "" means retention is not configured
                        {
                            dbs.push_back(dbpath);
                        }
                    }
                }
            }
        }

        string appname;
        SV_ULONGLONG lowts = 0, hights = 0;

        appname.clear();
        iter = nvpairs.find("app");
        if ( iter != nvpairs.end())
        {
            appname = iter ->second;
            CDPUtil::trim(appname);
        }
        iter = nvpairs.find("aftertime");
        if ( iter != nvpairs.end())
        {
            if(!CDPUtil::InputTimeToFileTime(iter ->second, lowts))
            {
                rv = false;
                break;
            }
        }
        iter = nvpairs.find("beforetime");
        if ( iter != nvpairs.end())
        {
            if(!CDPUtil::InputTimeToFileTime(iter ->second, hights))
            {
                rv = false;
                break;
            }
        }

        if(!appname.empty())
        {

            SV_EVENT_TYPE type;
            if(!VacpUtil::AppNameToTagType(appname, type))
            {
                rv = false;
                break;
            }

            CDPEvent cdpevent;
            if(!CDPUtil::MostRecentConsistentPoint(dbs, cdpevent, type, lowts,hights))
            {
                rv = false;
                break;
            }

            if(!newformat)
            {
                SV_TIME svtime;
                CDPUtil::ToSVTime(cdpevent.c_eventtime,svtime);
                stringstream displaytime;

                displaytime  << svtime.wYear   << "/" 
                    << svtime.wMonth  << "/" 
                    << svtime.wDay    << " " 
                    << svtime.wHour   << ":" 
                    << svtime.wMinute << ":" 
                    << svtime.wSecond << ":"
                    << svtime.wMilliseconds << ":" 
                    << svtime.wMicroseconds << ":"
                    << svtime.wHundrecNanoseconds ;

                DebugPrintf(SV_LOG_INFO, "Common Application Consistency Point:\n");
                DebugPrintf(SV_LOG_INFO, "Application Name:%s\n", appname.c_str());
                DebugPrintf(SV_LOG_INFO, "Bookmark:%s\n", cdpevent.c_eventvalue.c_str());
                DebugPrintf(SV_LOG_INFO, "Time:%s\n\n",displaytime.str().c_str());
            }
            if(newformat)
            {
                DebugPrintf(SV_LOG_INFO, "Common Application Consistency Point:\n\n");
                vector<string>::iterator dbIter = dbs.begin();
                svector_t::iterator volIter = devicelist.begin();
                for( /* empty */ ; dbIter != dbs.end(); ++dbIter)
                {
                    CDPMarkersMatchingCndn cndn;
                    cndn.type(cdpevent.c_eventtype);
                    if(!cdpevent.c_identifier.empty())
                    {
                        cndn.identifier(cdpevent.c_identifier);
                    }
                    else
                    {
                        cndn.atTime(cdpevent.c_eventtime);
                    }
                    if(volIter != devicelist.end())
                    {
                        DebugPrintf(SV_LOG_INFO, "Volume Name : %s\n",(*volIter).c_str());
                        ++volIter;
                    }
                    SV_ULONGLONG eventCount = 0;
                    if(!CDPUtil::printevents((*dbIter), cndn,eventCount,true))
                    {
                        DebugPrintf(SV_LOG_INFO, "Unable to fetch the cdpInfo for the device\n");
                    }
                }					
            }
        } else
        {
            SV_ULONGLONG ts;
            SV_ULONGLONG sequenceId =0;
            if(!CDPUtil::MostRecentCCP(dbs, ts,sequenceId,lowts,hights))
            {
                rv = false;
                break;
            }

            SV_TIME svtime;
            CDPUtil::ToSVTime(ts,svtime);
            stringstream displaytime;

            displaytime  << svtime.wYear   << "/" 
                << svtime.wMonth  << "/" 
                << svtime.wDay    << " " 
                << svtime.wHour   << ":" 
                << svtime.wMinute << ":" 
                << svtime.wSecond << ":"
                << svtime.wMilliseconds << ":" 
                << svtime.wMicroseconds << ":"
                << svtime.wHundrecNanoseconds ;

            DebugPrintf(SV_LOG_INFO, 
                "Common Recovery Point: %s\n", displaytime.str().c_str());
            if(sequenceId)
            {
                DebugPrintf(SV_LOG_INFO, "Corresponding I/O  Sequence Point: " ULLSPEC "\n", sequenceId);
            }
        }

    } while(0);

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::getSparsePolicySummary
*
* DESCRIPTION : Calculate the retention window from the retention volume size and the sparse policy  
*               Calculate the retention volume size from the retention window and the sparse policy 
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES : 
*
*
* return value : true on success, otherwise false
*
*/
bool CDPCli::getSparsePolicySummary()
{
    bool rv = true;
    do
    {
        NVPairs::iterator iter;
        SV_ULONGLONG dbProfileTime = 0;
        std::string dbname;
        std::map<SV_ULONGLONG,SV_ULONGLONG> sparseGranularityDuration;
        iter = nvpairs.find("dbprofile");
        if(iter == nvpairs.end())
        {
            DebugPrintf(SV_LOG_INFO, "Database name is not specified (--dbprofile). So the operation cannot be completed.\n");
            rv = false;
            break;
        }
        dbname = iter->second;
        iter = nvpairs.find("profileinterval");
        if(iter != nvpairs.end())
        {
            if(!getSparsePolicyListFromInput(iter->second,dbProfileTime))
            {
                DebugPrintf(SV_LOG_INFO, "profileinterval is not specified in the correct format. So the operation cannot be completed.\n");
                rv = false;
                break;
            }
        }
        bool verbose = false;		
        iter = nvpairs.find("verbose");
        if(iter != nvpairs.end())
        {
            verbose = true;
        }
        iter = nvpairs.find("standardpolicy");
        if(iter != nvpairs.end())
        {
            sparseGranularityDuration[0] = 259200;
            sparseGranularityDuration[3600] = 604800;
            //sparseGranularityDuration[86400] = 1900800;
            sparseGranularityDuration[604800] = 2419200;
            sparseGranularityDuration[2592000] = 15552000;
            cout << "\nTaking Retention Policy as\n\n"
                << "All writes for 3 days\n"
                << "Writes at 1 hour interval for next 7 days\n"
                << "Writes at 1 week interval for 4 weeks\n"
                << "Writes at 1 month interval for 6 months\n\n";
        }
        else
        {
            std::string getUserInput;
            SV_ULONGLONG interval = 0;
            SV_ULONGLONG duration = 0;
            char ch;
            fflush(stdin);
            DebugPrintf(SV_LOG_INFO,"\nProvide retention window in days maintaining every write (eg: 3 days): \n");
            ch = getchar();
            while(ch != '\n')
            {
                getUserInput += ch;
                ch = getchar();
            }
            if(!getSparsePolicyListFromInput(getUserInput,duration))
            {
                fflush(stdin);
                DebugPrintf(SV_LOG_INFO,"Input given is not valid. Enter in the format x hours/days/weeks/months, where x is an integer value.\n");
                break;
            }
            sparseGranularityDuration[0] = 	duration;
            getUserInput.clear();
            fflush(stdin);
            DebugPrintf(SV_LOG_INFO, "\nDo you want to provide coarser granularity (y/n)? (y/n) \n");
            ch = getchar();
            while(getchar()!= '\n');
            if((ch != 'n') && (ch != 'N'))
            {
                fflush(stdin);
                DebugPrintf(SV_LOG_INFO, 
                    "\nEnter coarse retention windows in following format. Enter \"END\" when you have "
                    "entered all retention windows.\n\nGranularity in minutes/hours/days/weeks/months (eg: 1 hour) [ , ]"
                    " Retention window in hours/days/weeks/months.\n\nExample:\n\n"
                    "1 hour,3 days\n"
                    "1 day,3 months\n"
                    "END\n\n");
                while(true)
                {
                    ch = getchar();
                    while(ch != '\n')
                    {
                        getUserInput += ch;
                        ch = getchar();
                    }
                    if(getUserInput.empty())
                    {
                        DebugPrintf(SV_LOG_INFO,"\nIf you have entered all the coarse retention then type \"END\"\n\n");
                        continue;
                    }
                    if(getUserInput == "END")
                    {
                        break;
                    }
                    svector_t tokens = CDPUtil::split(getUserInput,",",2);
                    if(tokens.size() != 2)
                    {
                        fflush(stdin);
                        DebugPrintf(SV_LOG_INFO,"The given format is not proper\n");
                        rv = false;
                        break;
                    }
                    CDPUtil::trim(tokens[0]);
                    CDPUtil::trim(tokens[1]);
                    if(!getSparsePolicyListFromInput(tokens[0],interval))
                    {
                        fflush(stdin);
                        DebugPrintf(SV_LOG_INFO,"The given format is not proper\n");
                        rv = false;
                        break;
                    }
                    if(!getSparsePolicyListFromInput(tokens[1],duration))
                    {
                        fflush(stdin);
                        DebugPrintf(SV_LOG_INFO,"The given format is not proper\n");
                        rv = false;
                        break;
                    }
                    getUserInput.clear();
                    std::map<SV_ULONGLONG,SV_ULONGLONG>::iterator it = sparseGranularityDuration.find(interval);
                    if(it != sparseGranularityDuration.end())
                    {
                        continue;
                    }
                    sparseGranularityDuration[interval] = duration;
                }
            }
        }
        if(!rv)
        {
            break;
        }
        DebugPrintf(SV_LOG_INFO, "computing....\n");
        if(!displaySparseIOStatisticsFromDB(dbname,sparseGranularityDuration,	dbProfileTime,verbose))
        {
            DebugPrintf(SV_LOG_INFO, "Unable to get the Io statistics from the db.\n");
            rv = false;
            break;
        }

    }while(false);
    return rv;
}

/*
* FUNCTION NAME :  CDPCli::getSparsePolicyListFromInput
*
* DESCRIPTION : parse the time input given by the user
*
* INPUT PARAMETERS : string, value
*
* OUTPUT PARAMETERS : none
*
* NOTES : 
*
*
* return value : true on success, otherwise false
*
*/

bool CDPCli::getSparsePolicyListFromInput(const std::string & spasePolicies,SV_ULONGLONG & value)
{
    bool rv = true;
    std::string timeunitstr;
    value = 0;
    try
    {
        do
        {
            svector_t tokens = CDPUtil::split(spasePolicies," ",2);

            if(tokens.size() == 1)
            {
                const char * c = tokens[0].c_str();
                std::string valstr;
                while(*c != '\0' )
                {
                    if(isdigit((int)(*c)))
                    {
                        valstr += (*c);
                        c++;
                    }
                    else
                    {
                        timeunitstr = c;
                        break;
                    }
                }
                value = boost::lexical_cast<SV_ULONGLONG>(valstr.c_str());
                if(!value)
                {
                    DebugPrintf(SV_LOG_INFO,"The given time format %s is not proper\n",spasePolicies.c_str());
                    rv = false;
                    break;
                }
            }
            else if(tokens.size() == 2)
            {
                value = boost::lexical_cast<SV_ULONGLONG>(tokens[0].c_str());
                timeunitstr = tokens[1];
            }
            else
            {
                DebugPrintf(SV_LOG_INFO,"The given time format %s is not proper\n",spasePolicies.c_str());
                rv = false;
                break;
            }

            if((timeunitstr == "minutes") || (timeunitstr == "minute"))
            {
                value *=60;
            }
            else if((timeunitstr == "hours") || (timeunitstr == "hour"))
            {
                value *=3600;
            }
            else if((timeunitstr == "days") || (timeunitstr == "day") )
            {
                value *= 86400;
            }
            else if((timeunitstr == "weeks") || (timeunitstr == "week"))
            {
                value *= 604800;
            }
            else if((timeunitstr == "months") || (timeunitstr == "month"))
            {
                value *= 2592000;
            }
            else if((timeunitstr == "years") || (timeunitstr == "year"))
            {
                value *= 31536000;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO,"The given time unit is not correct. Specified unit must be in hours/days/weeks/months/years\n");
                rv = false;
            }

        } while(false);
    } 
    catch( ... )
    {
        DebugPrintf(SV_LOG_INFO,"The time format specified is not proper\n");
        rv = false;
    }
    return rv;
}

/*
* FUNCTION NAME :  CDPCli::displaySparseIOStatisticsFromDB
*
* DESCRIPTION : get the coalesce iowrites for given granularities intervals, display the retention size required 
*               display the ioparten information...
*
* INPUT PARAMETERS : dbname, policyList, profileinterval
*
* OUTPUT PARAMETERS : none
*
* NOTES : 
* Algorithm:
*      1. get the cdp summary (starttime, end time, retention space)
*      2. set the profile interval(default: retention window, other wise minimum of (profile interval,retention window)
*      3. calculate the iorate
*      4. iterating through each policy
*           for each granularity interval
*              fetch drtd , calculate the colesce writes
*		    push into map(granularity:colesce writes)
*      5. determining the total size needed with sparse and without sparse
*
*
* return value : true on success, otherwise false
*
*/
bool CDPCli::displaySparseIOStatisticsFromDB(const std::string& dbname,const std::map<SV_ULONGLONG,SV_ULONGLONG> & policyList,const SV_ULONGLONG & profileinterval, bool verbose)
{
    bool rv = true;
    std::map<SV_ULONGLONG,LesceStats> statistics;
    ACE_Time_Value sTime = ACE_OS::gettimeofday();
    do
    {
        CDPDatabase database(dbname);
        CDPSummary summary;
        CDPRetentionDiskUsage retentionusage;

        //getting cdp summary to get the info start time, end time,retention size etc.
        if(!database.getcdpsummary(summary))
        {
            DebugPrintf(SV_LOG_INFO,"Error: read failure from retention store %s\n",dbname.c_str());
            rv = false;
            break;
        }

        //getting cdpdiskuage summary to get the info file count,size on disk etc.
        if(!database.get_cdp_retention_diskusage_summary(retentionusage))
        {
            DebugPrintf(SV_LOG_INFO,"Error: read failure from retention store %s\n",dbname.c_str());
            rv = false;
            break;
        }

        //profileIntervalInSec used to store the profile interval given  by user , default the retention window
        SV_ULONGLONG profileIntervalInSec = (summary.end_ts - summary.start_ts)/10000000LL;
        if(profileinterval)
        {
            profileIntervalInSec = (profileIntervalInSec < profileinterval)?profileIntervalInSec:profileinterval;
        }

        //calculating the ioratepersec
        SV_ULONGLONG ioRatePerSec = retentionusage.size_on_disk/profileIntervalInSec;

        //storing the last granularity level for display purpose
        std::string lastgranularitystr = "all writes";

        //iterating through each granularity level 
        //for each granularity, fetching the drtd and determining  the colesce writes
        // and stored the output in the statistic map(granularity,LesceStats)
        std::map<SV_ULONGLONG,SV_ULONGLONG>::const_iterator iter = policyList.begin();
        for(;iter != policyList.end();++iter)
        {
            SV_ULONGLONG granularity = iter->first;
            //checking if its all writes no need to calculate colesce write 
            //simply continuing for the next granularity level
            if(granularity == 0)
                continue;
            //profileIntervalSecondcount is used for to restict user to use the retention window for given interval
            SV_ULONGLONG profileIntervalSecondcount = granularity;
            //starttime is to set the starttime in cndn for fetching drtd
            SV_ULONGLONG starttime = summary.end_ts;
            //recovertime is to set the  recovertime in cndn for fetching drtd
            SV_ULONGLONG recovertime = 0;
            LesceStats lStarts;
            memset(&lStarts,0,sizeof(lStarts));

            //if the retention window is not sufficient for the granularity level
            // it will show a message for taking the previous granularity
            SV_ULONGLONG remainingsec = 0;
            std::stringstream out;
            if((granularity)/86400)
            {
                out << (granularity)/86400 << " days";
                if(remainingsec/3600)
                    out << ", " << remainingsec/3600 << " hours";
            }
            else if ((granularity)/3600)
            {
                out << (granularity)/3600 << " hours";
            }
            else if((granularity)/60)
            {
                out << (granularity)/60 << " minutes";
            }
            if(profileIntervalInSec < profileIntervalSecondcount)
            {
                cout << "\nThe retention window is not sufficient for computing the coalesce write for interval "
                    << out.str() << " . Computing the coalesce write by taking the writes for "
                    << lastgranularitystr << ".\n";
                statistics[granularity] = lStarts;
                continue;
            }
            lastgranularitystr = out.str();
            DrtdsOrderedStartOffset_t final_drtds;
            SV_ULONG FileId =0;

            //setting the recover point and the start point, fetching DRTD from db within the given interval
            while(true)
            {
                recovertime = starttime - ((granularity)* 10000000LL);
                if(recovertime < summary.start_ts)
                {
                    if(summary.start_ts < starttime)
                        recovertime = summary.start_ts;
                    else
                        break;
                }
                CDPDRTDsMatchingCndn cndn;
                cndn.fromTime(starttime);
                cndn.toTime(recovertime);
                CDPDatabaseImpl::Ptr dbread = database.FetchDRTDs(cndn);
                while(true)
                {
                    ACE_Time_Value cTime = ACE_OS::gettimeofday();
                    if(cTime.sec() - sTime.sec() > 120)
                    {
                        fflush(stdout);
                        cout << ".";
                        sTime = cTime;
                    }

                    cdp_rollback_drtd_t rollback_drtd;

                    SVERROR hr = dbread->read(rollback_drtd);
                    if(hr != SVS_OK)
                    {
                        break;
                    }
                    cdp_lesce_drtd_t drtdV3;
                    //constructing a SvDrtdV3 and storing that in diffPtr 
                    drtdV3.VolOffset = rollback_drtd.voloffset;
                    drtdV3.Length = rollback_drtd.length;
                    drtdV3.FileOffset = rollback_drtd.fileoffset;
                    drtdV3.FileId = FileId;
                    coalesce_dirty_block(final_drtds,drtdV3,lStarts);
                }
                ++FileId;
                final_drtds.clear();
                starttime = recovertime + 1;

                if(profileIntervalSecondcount > profileIntervalInSec)
                {
                    break;
                }
                profileIntervalSecondcount += granularity;
            }			
            statistics[granularity] = lStarts;
        }

        cout << "\n";
        //displaying the retention window 
        string datefrom, dateto;
        if((summary.start_ts == 0) || (summary.end_ts == 0))
        {
            cout << setw(26)  << setiosflags( ios::left ) 
                << "Retention Window Range(GMT): "
                << "Not Available" << "\n";
        }
        else 
        {
            if(!CDPUtil::ToDisplayTimeOnConsole(summary.start_ts, datefrom))
            {
                rv = false;
                break;
            }

            if(!CDPUtil::ToDisplayTimeOnConsole(summary.end_ts, dateto))
            {
                rv = false;
                break;
            }

            cout << "Retention Window Range(GMT): "
                << datefrom << " to " << "\n"
                << "                               " << dateto << "\n";
        }

        //displaying the colesce info for granularity
        cout << "\nChange Rate: " << ioRatePerSec << " bytes/sec\n";
        cout << "\n-------------------------------------------------------------------------------\n";
        cout << setw(26)  << setiosflags( ios::left ) << "Retention window"
            << setw(18) << setiosflags( ios::left ) << "Granularity"
            << setiosflags( ios::left ) << "Required size (in bytes)" << endl;
        cout << "-------------------------------------------------------------------------------\n";

        iter = policyList.begin();
        //sizeofSparseRetention to calculate the size of the retention required with sparse policy
        SV_ULONGLONG sizeofSparseRetention = 0;
        //sizeofRetention to calculate the size of the retention required without sparse policy
        SV_ULONGLONG sizeofRetention = 0;
        //lastcoalescewrite to store the colesce writes, so that if the colesce write is not available
        //for the next granularity level it will take the value of lastcoalescewrite 
        SV_ULONGLONG lastcoalescewrite = 0;
        //sparsewindow to calculate the total window size given by user as part of sparse retention
        SV_ULONGLONG sparsewindow = 0;
        SV_ULONGLONG remainingsec = 0;
        std::stringstream out;
        std::stringstream verboseoutput;
        verboseoutput << setw(25)  << setiosflags( ios::left ) << "Coalesce interval"
            << setw(25) << setiosflags( ios::left ) << "Total writes(in bytes)"
            << setw(30) << setiosflags( ios::left ) << "Coalesced Writes(in bytes)"
            << setiosflags( ios::left ) << "Overlapping Percentage\n";
        verboseoutput << "-------------------------------------------------------------------------------------------------------\n";
        for(;iter != policyList.end();++iter)
        {
            SV_ULONGLONG granularity = iter->first;
            SV_ULONGLONG duration = iter->second;
            sparsewindow += duration;
            out.str("");
            remainingsec = (duration)%86400;
            if((iter->second)/86400)
            {
                out << (duration)/86400 << " days";
                if(remainingsec/3600)
                    out << ", " << remainingsec/3600 << " hours";
            }
            else
            {
                out << (duration)/3600 << " hours";
            }
            cout << setw(26)  << setiosflags( ios::left ) << out.str();
            out.str("");
            remainingsec = (granularity)%86400;
            if((iter->first)/86400)
            {
                out << (granularity)/86400 << " days";
                if(remainingsec/3600)
                    out << ", " << remainingsec/3600 << " hours";
            }
            else if ((granularity)/3600)
            {
                out << (granularity)/3600 << " hours";
            }
            else if((granularity)/60)
            {
                out << (granularity)/60 << " minutes";
            }
            else
            {
                out << "all changes";
            }
            cout << setw(18)  << setiosflags( ios::left ) << out.str();
            std::map<SV_ULONGLONG,LesceStats>::iterator iterStatistic = statistics.find(granularity);
            if(granularity)
            {
                if(iterStatistic->second.total_coalesced_writes_len != 0)
                {
                    lastcoalescewrite = ((iterStatistic->second.total_coalesced_writes_len)/profileIntervalInSec);
                }
                //checking for colesce should not be greater than total writes
                //if found to be greater assigning the tatalwrites to the colesce writes
                if(ioRatePerSec < lastcoalescewrite)
                {
                    lastcoalescewrite = ioRatePerSec;
                }
                sizeofSparseRetention += (lastcoalescewrite * (duration));
                sizeofRetention += (ioRatePerSec * (duration));
                cout << lastcoalescewrite * (duration) << endl;
                verboseoutput << setw(25)  << setiosflags( ios::left ) << out.str();
                verboseoutput << setw(25)  << setiosflags( ios::left ) << (ioRatePerSec * (granularity));
                verboseoutput << setw(30) << setiosflags( ios::left ) << (lastcoalescewrite * (granularity));
                verboseoutput << setiosflags( ios::left ) << ((ioRatePerSec - lastcoalescewrite) * 100)/ioRatePerSec << endl;
            }
            else
            {
                sizeofSparseRetention += (ioRatePerSec * (duration));
                sizeofRetention += (ioRatePerSec * (duration));
                lastcoalescewrite = ioRatePerSec;
                cout << lastcoalescewrite * (duration) << endl;
            }

        }
        cout << "-------------------------------------------------------------------------------\n";
        out.str("");
        remainingsec = sparsewindow%86400;
        if(sparsewindow/86400)
        {
            out << sparsewindow/86400 << " days";
        }
        if(remainingsec/3600)
        {
            out << ", " << remainingsec/3600 << " hours";
        }
        cout << setw(44)  << setiosflags( ios::left ) << out.str();
        cout << setiosflags( ios::left ) << sizeofSparseRetention << endl;

        cout << "\nRetention volume size for storing " << out.str() << " of data:\n\n";
        cout << "  Without sparse retention : ";
        if(sizeofRetention/(1024*1024*1024))
        {
            cout << (sizeofRetention/(1024*1024*1024)) + 1 << " gb\n";
        }
        else if(sizeofRetention/(1024*1024))
        {
            cout << (sizeofRetention/(1024*1024)) + 1 << " mb\n";
        }
        else if(sizeofRetention/1024)
        {
            cout << (sizeofRetention/1024) + 1 << " kb\n";
        }
        else
        {
            cout << sizeofRetention << " bytes\n";
        }

        cout << "\n  With sparse retention    : ";
        if(sizeofSparseRetention/(1024*1024*1024))
        {
            cout << (sizeofSparseRetention/(1024*1024*1024)) + 1 << " gb\n";
        }
        else if(sizeofSparseRetention/(1024*1024))
        {
            cout << (sizeofSparseRetention/(1024*1024)) + 1 << " mb\n";
        }
        else if(sizeofSparseRetention/1024)
        {
            cout << (sizeofSparseRetention/1024) + 1 << " kb\n";
        }
        else
        {
            cout << sizeofSparseRetention << " bytes\n";
        }

        SV_ULONGLONG savingpercentage = 0;
        savingpercentage = ((sizeofRetention - sizeofSparseRetention) * 100)/sizeofRetention;
        cout << "\n  Storage Saving percentage: " << savingpercentage << "\%" << endl;

        if(verbose)
        {
            cout << "\nOverlap Information:\n\n";
            cout << verboseoutput.str();
        }
    } while(false);
    return rv;
}
/*
* FUNCTION NAME :  CDPCli::printiodistribution
*
* DESCRIPTION : print the io pattern on the target site 
*               by parsing retention database
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES : 
*
*
* return value : true on success, otherwise false
*
*/
bool CDPCli::printiodistribution()
{
    bool rv = true;

    do
    {
        NVPairs::iterator iter;
        string db_path;
        string volname;

        iter = nvpairs.find("vol");
        if(iter == nvpairs.end())
        {
            DebugPrintf(SV_LOG_ERROR, "please specify the target volume.\n\n") ;
            usage(argv[0], m_operation);
            rv = false;
            break;
        }

        volname = iter ->second;
        if(!getCdpDbName(volname, db_path))
        {
            rv = false;
            break;
        }

        if(db_path == "")
        {
            DebugPrintf(SV_LOG_ERROR, "Retention is not enabled for %s\n", volname.c_str());
            rv = false;
            break;
        }

        if(!printiopattern(db_path))
        {
            rv = false;
            break;
        }

    } while(0);

    return rv;
}


/*
* FUNCTION NAME :  CDPCli::showsummary
*
* DESCRIPTION : display the summary information for the specified 
*               retention database
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES : 
*
*
* return value : true on success, otherwise false
*
*/
bool CDPCli::showsummary()
{
    bool rv = true;
    bool verbose=false;
	bool show_space_usage = false;

    do
    {

        NVPairs::iterator iter;
        string db_path;
        //check whether move retention is on pogress or not
        string deviceName;
        iter = nvpairs.find("vol");
        if(iter != nvpairs.end())
        {
            deviceName = iter ->second;
        }
        if(!deviceName.empty())
        {
            CDPUtil::trim(deviceName);
            bool shouldRun = true;
            bool foundstate = ShouldOperationRun(deviceName,shouldRun);
            if(!shouldRun)
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "The retention logs for volume %s are being moved. Please try after sometime. \n",
                    deviceName.c_str());
                rv = false;
                break;
            }
        }
        iter = nvpairs.find("db");
        if(iter != nvpairs.end())
        {
            db_path = iter ->second;
        }
        else
        {
            iter = nvpairs.find("vol");
            if(iter != nvpairs.end())
            {
                string volname = iter ->second;
                if(!getCdpDbName(volname, db_path))
                {
                    rv = false;
                    break;
                }

                if(db_path == "")
                {
                    DebugPrintf(SV_LOG_ERROR, "Retention is not enabled for %s\n", volname.c_str());
                    rv = false;
                    break;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,
                    "please specify the cdp database or the volume "
                    "whose corresponding cdp database is to be used. \n\n") ;
                usage(argv[0], m_operation);
                rv = false;
                break;
            }
        }

        //check for verbose 
        iter = nvpairs.find("verbose");
        if ( iter != nvpairs.end())
        {
            verbose=true;                                
        }
		
		iter = nvpairs.find("spaceusage");
		if ( iter != nvpairs.end())
		{
			show_space_usage = true;                                
		}

        if(!printsummary(db_path, show_space_usage, verbose))
        {
            rv = false;
            break;
        }


    } while (0);

    return rv;
}


/*
* FUNCTION NAME :  CDPCli::validate
*
* DESCRIPTION : validate the specified retention database
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES : 
*
*
* return value : true on success, otherwise false
*
*/

bool CDPCli::validate()
{
    bool rv = true;

    do
    {

        NVPairs::iterator iter;
        string db_path;

        iter = nvpairs.find("no_vxservice_check");
        if(iter == nvpairs.end())
        {
            bool serviceRunning;
            if(!ReplicationAgentRunning(serviceRunning))  
            {
                rv = false;
                break;
            }

            if(serviceRunning)
            {
                DebugPrintf(SV_LOG_INFO, "Service is running please stop it and try again.\n");
                rv = false;
                break;
            }
        }

        iter = nvpairs.find("db");
        if(iter != nvpairs.end())
        {
            db_path = iter ->second;
        }
        else
        {
            iter = nvpairs.find("vol");
            if(iter != nvpairs.end())
            {
                string volname = iter ->second;
                bool shouldRun = true;
                bool foundstate = ShouldOperationRun(volname,shouldRun);
                if(!shouldRun)
                {
                    DebugPrintf(SV_LOG_ERROR,
                        "The retention logs for volume %s are being moved. Please try after sometime. \n",
                        volname.c_str());
                    rv = false;
                    break;
                }
                if(!getCdpDbName(volname,db_path))
                {
                    rv = false;
                    break;
                }

                if(db_path == "")
                {
                    DebugPrintf(SV_LOG_ERROR, "Retention is not enabled for %s\n", volname.c_str());
                    rv = false;
                    break;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,
                    "please specify the cdp database or the volume "
                    "whose corresponding cdp database is to be validated. \n\n") ;
                usage(argv[0], m_operation);
                rv = false;
                break;
            }
        }

        DisplayNote2();

        CDPValidate validator(db_path);

        rv = validator.validate();

    } while (0);

    return rv;
}
/*
* FUNCTION NAME :  CDPCli::listevents
*
* DESCRIPTION : display the matching events from specified retention database
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : no. of matching events
*
* NOTES : 
*
*
* return value : true on success, otherwise false
*
*/
bool CDPCli::listevents(SV_ULONGLONG &eventCount)
{
    bool rv = true;

    do
    {
        NVPairs::iterator iter;
        string db_path;
        string devicename;
        bool namevaluefomat = false;
        iter = nvpairs.find("vol");
        if(iter != nvpairs.end())
        {
            devicename = iter ->second;

        }
        if(!devicename.empty())
        {
            CDPUtil::trim(devicename);
            bool shouldRun = true;
            bool foundstate = ShouldOperationRun(devicename,shouldRun);
            if(!shouldRun)
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "The retention logs for volume %s are being moved. Please try after sometime. \n",
                    devicename.c_str());
                rv = false;
                break;
            }
        }

        CDPMarkersMatchingCndn cndn;

        iter = nvpairs.find("app");
        if ( iter != nvpairs.end())
        {
            SV_EVENT_TYPE type;
            string appname = iter ->second;
            if(!VacpUtil::AppNameToTagType(appname, type))
            {
                rv = false;
                break;
            }
            cndn.type(type);
        }

        iter = nvpairs.find("event");
        if ( iter != nvpairs.end())
        {		
            cndn.value(iter ->second);
        }

        iter = nvpairs.find("at");
        if ( iter != nvpairs.end())
        {
            if ( (nvpairs.find("after") != nvpairs.end())
                || (nvpairs.find("before") != nvpairs.end()) )
            {
                DebugPrintf(SV_LOG_ERROR, "%s",
                    "Invalid usage: after/before options cannot be used with 'at' option\n");
                usage(argv[0], m_operation);
                rv = false;
                break;
            }

            SV_ULONGLONG attime;
            if(!CDPUtil::InputTimeToFileTime(iter ->second, attime))
            {
                rv = false;
                break;
            }
            cndn.atTime(attime);
        }

        iter = nvpairs.find("aftertime");
        if ( iter != nvpairs.end())
        {	
            SV_ULONGLONG aftertime;
            if(!CDPUtil::InputTimeToFileTime(iter ->second, aftertime))
            {
                rv = false;
                break;
            }
            cndn.afterTime(aftertime);
        }

        iter = nvpairs.find("beforetime");
        if ( iter != nvpairs.end())
        {	
            SV_ULONGLONG beforetime;
            if(!CDPUtil::InputTimeToFileTime(iter ->second, beforetime))
            {
                rv = false;
                break;
            }
            cndn.beforeTime(beforetime);
        }

        iter = nvpairs.find("verified");
        if ( iter != nvpairs.end())
        {
            std::string ver_flag = iter->second;
            CDPUtil::trim(ver_flag);
            if(0 == stricmp(ver_flag.c_str(), "yes"))
                cndn.setverified(VERIFIEDEVENT);
            else
                cndn.setverified(NONVERIFIEDEVENT);
        }
        iter = nvpairs.find("onlylockedbookmarks");
        if ( iter != nvpairs.end())
        {
            cndn.setlockstatus(BOOKMARK_STATE_LOCKED );
        }

        iter = nvpairs.find("namevalueformat");
        if ( iter != nvpairs.end())
            namevaluefomat = true;

        iter = nvpairs.find("sort");
        if ( iter != nvpairs.end())
        {
            string sortcol = iter ->second;
            if ( ( sortcol != "time") 
                && ( sortcol != "app" ) 
                && ( sortcol != "event" ) 
                && ( sortcol != "accuracy" ) )
            {
                DebugPrintf(SV_LOG_ERROR, "%s%s%s",
                    "specified sort column: ", sortcol.c_str(), " is invalid\n");
                usage(argv[0], m_operation);
                rv = false;
                break;
            }
            cndn.sortby(sortcol);
        }

        iter = nvpairs.find("db");
        if(iter != nvpairs.end())
        {
            db_path = iter ->second;
        }
        else
        {
            iter = nvpairs.find("vol");
            if(iter != nvpairs.end())
            {
                string volname = iter ->second;
                if(!getCdpDbName(volname,db_path))
                {
                    rv = false;
                    break;
                }

                if(db_path == "")
                {
                    DebugPrintf(SV_LOG_ERROR, "Retention is not enabled for %s\n", volname.c_str());
                    rv = false;
                    break;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,
                    "please specify the cdp database or the volume "
                    "whose corresponding cdp database is to be used. \n\n") ;
                usage(argv[0], m_operation);
                rv = false;
                break;
            }
        }

        if(!CDPUtil::printevents(db_path, cndn, eventCount,namevaluefomat))
        {
            rv = false;
            break;
        }



    } while (0);

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::DisplayNote1
*
* DESCRIPTION : Informational messages for user
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
*
*
* return value : none
*
*/

void CDPCli::DisplayNote1()
{
    stringstream out;
    out << "Note:\n"

        << "  1. Specifying a volume which is part of any replication\n"
        << "     as either source or destination volume is not allowed\n"
        << "     unless the Replication agent service is stopped.\n\n"
        << "  2. All data on destination volume would be overwritten by\n"
        << "     this operation.\n\n"
        << "  3. Aborting this process during the operation may leave the\n"
        << "     source/destination volume in dismounted or locked state.\n"
        << "     Please check the troubleshooting reference to restore the\n"
        << "     volume state.\n\n";

    DebugPrintf(SV_LOG_INFO, "%s", out.str().c_str());
}

/*
* FUNCTION NAME :  CDPCli::DisplayNote2
*
* DESCRIPTION : Informational messages for user
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
*
*
* return value : none
*
*/
void CDPCli::DisplayNote2()
{
    stringstream out;
    out << "Note:\n"

        << "  Validation of the database requires exclusive access to the database.\n"
        << "  No other application/service will be able to access the database\n"
        << "  while validation is in progress\n\n";

    DebugPrintf(SV_LOG_INFO, "%s", out.str().c_str());
}

/*
* FUNCTION NAME :  CDPCli::DisplayNote3
*
* DESCRIPTION : Informational messages for user
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
*
*
* return value : none
*
*/
void CDPCli::DisplayNote3()
{
    stringstream out;
    out << "Note:\n"

        << "  While Replication status is in resync, hide or unhide operations on\n"
        << "  target volume should not be performed.\n\n";

    DebugPrintf(SV_LOG_INFO,"%s", out.str().c_str());
}

/*
* FUNCTION NAME :  CDPCli::ReplicationAgentRunning
*
* DESCRIPTION : check if svagents is running
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
*
*
* return value : none
*
*/

bool CDPCli::ReplicationAgentRunning(bool & brunning)
{
    bool rv = true;
    brunning = false;
    std::string service_name;
    do
    {

        LocalConfigurator localConfigurator;
        service_name = localConfigurator.getHostAgentName();
        DebugPrintf(SV_LOG_INFO," \nChecking for  Replication agent service (%s) status ...\n",service_name.c_str());

        SV_ULONG state;
        if(!CDPUtil::GetServiceState(service_name, state))
        {
            rv = false;
            break;
        }
#ifdef SV_WINDOWS
        if (state == SERVICE_STOPPED)
        {
            DebugPrintf(SV_LOG_INFO," Replication agent service (%s) is currently stopped\n",service_name.c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_INFO," Replication agent service (%s) is running\n",service_name.c_str());
            brunning = true;
        }
#else
        if (state == 0)
        {
            DebugPrintf(SV_LOG_INFO," Replication agent service (%s) is currently stopped\n",service_name.c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_INFO," Replication agent service (%s) is running\n",service_name.c_str());
            brunning = true;
        }
#endif

    } while (0);

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::snapshot
*
* DESCRIPTION : get a point in time block level copy of the specified volume
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
*
*
* return value : true if the operation succeeds, otherwise false.
*
*/
bool CDPCli::snapshot()
{

    bool rv = true;


    do
    {
        SNAPSHOT_REQUESTS requests; 
        string prescript, postscript;


        if(!getsnapshotpairs(requests))
        {
            rv = false;
            break;
        }

        if(!getprepostscript(requests, prescript, postscript))
        {
            rv = false;
            break;
        }


        bool parallel = true;
        rv = ProcessSnapshotRequests(requests,prescript,postscript,parallel);
        if(!rv)
            break;

    } while (0);

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::recover
*
* DESCRIPTION : get a back in time block level copy of the specified volume
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
*
*
* return value : true if the operation succeeds, otherwise false.
*
*/
bool CDPCli::recover()
{
    bool rv = true;

    do
    {
        SNAPSHOT_REQUESTS requests; 
        string prescript, postscript;

        if(!getrecoverypairsanddb(requests))
        {
            rv = false;
            break;
        }

        if(!getrecoverypoint(requests))
        {
            rv = false;
            break;
        }

        if(!getprepostscript(requests, prescript, postscript))
        {
            rv = false;
            break;
        }

        bool parallel = true;
        rv = ProcessSnapshotRequests(requests,prescript,postscript,parallel);
        if(!rv)
            break;

    } while (0);

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::rollback
*
* DESCRIPTION : rollback the specified volume to back in time 
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
*
*
* return value : true if the operation succeeds, otherwise false.
*
*/
bool CDPCli::rollback()
{
    bool rv = true;

    do
    {
        SNAPSHOT_REQUESTS requests; 
        string prescript, postscript;


        if(!getrollbackpairsanddb(requests))
        {
            rv = false;
            break;
        }

        if(!getrecoverypoint(requests))
        {
            rv = false;
            break;
        }

        if(!getprepostscript(requests, prescript, postscript))
        {
            rv = false;
            break;
        }

        getrollbackcleanupmsg(requests);
        bool parallel = true;
        rv = ProcessSnapshotRequests(requests,prescript,postscript,parallel);
        if(!rv)
            break;

    }while (0);

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::getsnapshotpairs
*
* DESCRIPTION : parse the user input and fill in the snapshot pair
*               details
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
*
*
* return value : true if the operation succeeds, otherwise false.
*
*/
bool CDPCli::getsnapshotpairs(SNAPSHOT_REQUESTS & requests)
{
    NVPairs::iterator iter;
    bool rv = true;
    bool volgrp = false;
    bool notvolgrp = false;
    string src,dest,mtpt;
    int pairnum = 0;
    char pairid[100];

    do
    {
        iter = nvpairs.find("snapshotpairs");
        if(iter != nvpairs.end())
        {
            if(SetSkipCheckFlag())
            {
                DebugPrintf(SV_LOG_ERROR, "The option --skipchecks do not work with --snapshotpairs option.\n");
                rv = false;
                break;
            }
            volgrp = true;
            string token = iter -> second;
            svector_t svpairs = CDPUtil::split(token, ";");
            svector_t::iterator svpairs_iter = svpairs.begin();
            for( ; svpairs_iter != svpairs.end() ; ++svpairs_iter)
            {

                string svpairinfo = *svpairs_iter;
                svector_t svpair = CDPUtil::split(svpairinfo, ",");
                if(svpair.size() < 2 || svpair.size() > 3 )
                {
                    DebugPrintf(SV_LOG_ERROR, "syntax error near %s\n\n", svpairinfo.c_str());
                    usage(argv[0], m_operation);
                    rv = false;
                    break;
                }

                SNAPSHOT_REQUEST::Ptr request(new SNAPSHOT_REQUEST());
                request -> src = svpair[0];
                request -> dest = svpair[1];
                if(svpair.size() > 2)
                    request -> destMountPoint = svpair[2];

                CDPUtil::trim(request -> src);
                CDPUtil::trim(request -> dest);
                CDPUtil::trim(request -> destMountPoint);

                if (IsReportingRealNameToCx())
                {
                    GetDeviceNameFromSymLink(request -> src);
                    GetDeviceNameFromSymLink(request -> dest);
                }
                else
                {
                    std::string srclinkname = request -> src;
                    if (!GetLinkNameIfRealDeviceName(request -> src, srclinkname))
                    {
                        DebugPrintf(SV_LOG_ERROR, "Invalid source volume specified\n");
                        rv = false;
                        break;
                    } 
                    request -> src = srclinkname; 

                    std::string destlinkname = request -> dest;
                    if (!GetLinkNameIfRealDeviceName(request -> dest, destlinkname))
                    {
                        DebugPrintf(SV_LOG_ERROR, "Invalid destination volume specified\n");
                        rv = false;
                        break;
                    } 
                    request -> dest = destlinkname; 
                }

                FirstCharToUpperForWindows(request -> src);
                FirstCharToUpperForWindows(request -> dest);

                if (!request->destMountPoint.empty())
                {
                    std::string strerr;
                    if (!isvalidmountpoint(request->src, strerr))
                    {
                        DebugPrintf(SV_LOG_ERROR, "mountpoint option is not valid for source = %s, destination = %s, mountpoint = %s\n", 
                            request->src.c_str(), request->dest.c_str(), request->destMountPoint.c_str());
                        DebugPrintf(SV_LOG_ERROR, "The reason is: %s\n", strerr.c_str());
                        rv = false;
                        break;
                    } 
                }

                ++pairnum;
				inm_sprintf_s(pairid, ARRAYSIZE(pairid), "clisnapshot%d", pairnum);
                request -> operation = SNAPSHOT_REQUEST::PLAIN_SNAPSHOT;
                request -> id = pairid;
                requests.insert(requests.end(),SNAPSHOT_REQUEST_PAIR(pairid,request));
            }

            if(!rv)
                break;
        }

        iter = nvpairs.find("source");
        if(iter != nvpairs.end())
        {
            src = iter -> second;
            notvolgrp = true;
        }

        iter = nvpairs.find("dest");
        if(iter != nvpairs.end())
        {
            dest = iter -> second;
            notvolgrp = true;
        }

        iter = nvpairs.find("mountpoint");
        if(iter != nvpairs.end())
        {
            mtpt = iter -> second;
            notvolgrp = true;
        }


        if(volgrp && notvolgrp)
        {
            DebugPrintf(SV_LOG_ERROR," \'snapshot pairs\' and (source, dest ,mountpoint) are mutually exclusive options.\n\n");
            usage(argv[0], m_operation);
            rv = false;
            break;
        }

        if(notvolgrp)
        {
            SNAPSHOT_REQUEST::Ptr request(new SNAPSHOT_REQUEST());
            if(src.empty())
            {
                DebugPrintf(SV_LOG_ERROR, " please specify source volume.\n\n");
                usage(argv[0], m_operation);
                rv = false;
                break;
            }	

            if(dest.empty())
            {
                DebugPrintf(SV_LOG_ERROR, " please specify destination volume.\n\n");
                usage(argv[0], m_operation);
                rv = false;
                break;
            }

            request -> src = src;
            request -> dest = dest;
            if(!mtpt.empty())
                request -> destMountPoint = mtpt;

            CDPUtil::trim(request -> src);
            CDPUtil::trim(request -> dest);
            CDPUtil::trim(request -> destMountPoint);

            if (IsReportingRealNameToCx())
            {
                GetDeviceNameFromSymLink(request -> src);
                GetDeviceNameFromSymLink(request -> dest);
            }
            else
            {
                std::string srclinkname = request -> src;
                if (!GetLinkNameIfRealDeviceName(request -> src, srclinkname))
                {
                    DebugPrintf(SV_LOG_ERROR, "Invalid source volume specified\n");
                    rv = false;
                    break;
                } 
                request -> src = srclinkname; 

                std::string destlinkname = request -> dest;
                if (!GetLinkNameIfRealDeviceName(request -> dest, destlinkname))
                {
                    DebugPrintf(SV_LOG_ERROR, "Invalid destination volume specified\n");
                    rv = false;
                    break;
                } 
                request -> dest = destlinkname; 
            }

            FirstCharToUpperForWindows(request -> src);
            FirstCharToUpperForWindows(request -> dest);


            if (!request->destMountPoint.empty())
            {
                std::string strerr;
                if (!isvalidmountpoint(request->src, strerr))
                {
                    DebugPrintf(SV_LOG_ERROR, "mountpoint option is not valid for source = %s, destination = %s, mountpoint = %s\n", 
                        request->src.c_str(), request->dest.c_str(), request->destMountPoint.c_str());
                    DebugPrintf(SV_LOG_ERROR, "The reason is: %s\n", strerr.c_str());
                    rv = false;
                    break;
                } 
            }
            if(SetSkipCheckFlag())
            {
                if(!SetSrcVolumeSize(request->srcVolCapacity))
                {
                    rv = false;
                    break;
                }
            }
			inm_sprintf_s(pairid, ARRAYSIZE(pairid), "clisnapshot%d", pairnum);
            request -> operation = SNAPSHOT_REQUEST::PLAIN_SNAPSHOT;
            request -> id = pairid;
            requests.insert(requests.end(),SNAPSHOT_REQUEST_PAIR(pairid,request));
        }

        if(requests.empty())
        {
            DebugPrintf(SV_LOG_INFO, "Please specify the source and destination volumes for snapshot.\n\n");
            usage(argv[0], m_operation);
            rv = false;
            break;
        }

    } while (0);

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::getrecoverypairsanddb
*
* DESCRIPTION : parse the user input and fill in the recovery pair
*               details, retention database path
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
*   
*
* return value : true if the operation succeeds, otherwise false.
*
*/
bool CDPCli::getrecoverypairsanddb(SNAPSHOT_REQUESTS & requests)
{
    NVPairs::iterator iter;
    bool rv = true;
    bool volgrp = false;
    bool notvolgrp = false;
    string src,dest,mtpt,db;
    int pairnum = 0;
    char pairid[100];

    do
    {
        iter = nvpairs.find("recoverypairs");
        if(iter != nvpairs.end())
        {
            if(SetSkipCheckFlag())
            {
                DebugPrintf(SV_LOG_ERROR, "The option --skipchecks do not work with --recoverypairs option.\n");
                rv = false;
                break;
            }
            volgrp = true;
            string token = iter -> second;
            svector_t svpairs = CDPUtil::split(token, ";");
            svector_t::iterator svpairs_iter = svpairs.begin();
            for( ; svpairs_iter != svpairs.end() ; ++svpairs_iter)
            {

                string svpairinfo = *svpairs_iter;
                svector_t svpair = CDPUtil::split(svpairinfo, ",");
                if(svpair.size() < 2 || svpair.size() > 4 )
                {
                    DebugPrintf(SV_LOG_ERROR, "syntax error near %s\n\n", svpairinfo.c_str());
                    usage(argv[0], m_operation);
                    rv = false;
                    break;
                }
                string devicename = svpair[0];
                CDPUtil::trim(devicename);
                bool shouldRun = true;
                bool foundstate = ShouldOperationRun(devicename,shouldRun);
                if(shouldRun)
                {
                    SNAPSHOT_REQUEST::Ptr request(new SNAPSHOT_REQUEST());
                    request -> src = svpair[0];
                    request -> dest = svpair[1];
                    if(svpair.size() > 2)
                        request -> destMountPoint = svpair[2];

                    CDPUtil::trim(request -> src);
                    CDPUtil::trim(request -> dest);
                    CDPUtil::trim(request -> destMountPoint);

                    if (IsReportingRealNameToCx())
                    {
                        GetDeviceNameFromSymLink(request -> src);
                        GetDeviceNameFromSymLink(request -> dest);
                    }
                    else
                    {
                        std::string srclinkname = request -> src;
                        if (!GetLinkNameIfRealDeviceName(request -> src, srclinkname))
                        {
                            DebugPrintf(SV_LOG_ERROR, "Invalid source volume specified\n");
                            rv = false;
                            break;
                        } 
                        request -> src = srclinkname; 

                        std::string destlinkname = request -> dest;
                        if (!GetLinkNameIfRealDeviceName(request -> dest, destlinkname))
                        {
                            DebugPrintf(SV_LOG_ERROR, "Invalid destination volume specified\n");
                            rv = false;
                            break;
                        } 
                        request -> dest = destlinkname; 
                    }

                    FirstCharToUpperForWindows(request -> src);
                    FirstCharToUpperForWindows(request -> dest);

                    if(svpair.size() > 3)
                    {
                        request -> dbpath = svpair[3];
                        /** Added by BSR - Issue#5288 **/
                        if( !resolvePathAndVerify( request->dbpath ) ) 
                        {
                            DebugPrintf( SV_LOG_ERROR, "Invalid DB path: %s\n", svpair[3].c_str() ) ;
                            rv = false ;
                            break ;
                        }
                        /** End of the change **/
                    } 
                    else  // obtain db path from local cached settings
                    {
                        if(!getCdpDbName(request -> src, request ->dbpath))
                        {
                            rv = false;
                            break;
                        }

                        if(request ->dbpath == "")
                        {
                            DebugPrintf(SV_LOG_ERROR, "Retention is not enabled for %s\n", request -> src.c_str());
                            rv = false;
                            break;
                        }

                    }

                    CDPUtil::trim(request -> dbpath);

                    if (!request->destMountPoint.empty())
                    {
                        std::string strerr;
                        if (!isvalidmountpoint(request->src, strerr))
                        {
                            DebugPrintf(SV_LOG_ERROR, "mountpoint option is not valid for source = %s, destination = %s, mountpoint = %s\n", 
                                request->src.c_str(), request->dest.c_str(), request->destMountPoint.c_str());
                            DebugPrintf(SV_LOG_ERROR, "The reason is: %s\n", strerr.c_str());
                            rv = false;
                            break;
                        } 
                    }

                    ++pairnum;
					inm_sprintf_s(pairid, ARRAYSIZE(pairid), "clirecover%d", pairnum);
                    request -> operation = SNAPSHOT_REQUEST::RECOVERY_SNAPSHOT;
                    request -> id = pairid;
                    requests.insert(requests.end(),SNAPSHOT_REQUEST_PAIR(pairid,request));
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, 
                        "The retention logs for volume %s are being moved. Please try after sometime. \n",
                        devicename.c_str());
                    rv = false;
                    break;
                }
            }

            if(!rv)
                break;
        }

        iter = nvpairs.find("source");
        if(iter != nvpairs.end())
        {
            src = iter -> second;
            notvolgrp = true;
        }

        iter = nvpairs.find("dest");
        if(iter != nvpairs.end())
        {
            dest = iter -> second;
            notvolgrp = true;
        }

        iter = nvpairs.find("mountpoint");
        if(iter != nvpairs.end())
        {
            mtpt = iter -> second;
            notvolgrp = true;
        }

        iter = nvpairs.find("db");
        if(iter != nvpairs.end())
        {
            db = iter -> second;
            notvolgrp = true;
        }


        if(volgrp && notvolgrp)
        {
            DebugPrintf(SV_LOG_ERROR," \'recovery pairs\' and (source, dest ,mountpoint, db) are mutually exclusive options.\n\n");
            usage(argv[0], m_operation);
            rv = false;
            break;
        }

        if(notvolgrp)
        {
            string devicename = src;
            CDPUtil::trim(devicename);
            bool shouldRun = true;
            bool foundstate = ShouldOperationRun(devicename,shouldRun);
            if(shouldRun)
            {
                SNAPSHOT_REQUEST::Ptr request(new SNAPSHOT_REQUEST());
                if(src.empty())
                {
                    DebugPrintf(SV_LOG_ERROR, " please specify source volume.\n\n");
                    usage(argv[0], m_operation);
                    rv = false;
                    break;
                }	

                if(dest.empty())
                {
                    DebugPrintf(SV_LOG_ERROR, " please specify destination volume.\n\n");
                    usage(argv[0], m_operation);
                    rv = false;
                    break;
                }

                request -> src = src;
                request -> dest = dest;
                if(!mtpt.empty())
                    request -> destMountPoint = mtpt;

                CDPUtil::trim(request -> src);
                CDPUtil::trim(request -> dest);
                CDPUtil::trim(request -> destMountPoint);

                if (IsReportingRealNameToCx())
                {
                    GetDeviceNameFromSymLink(request -> src);
                    GetDeviceNameFromSymLink(request -> dest);
                }
                else
                {
                    std::string srclinkname = request -> src;
                    if (!GetLinkNameIfRealDeviceName(request -> src, srclinkname))
                    {
                        DebugPrintf(SV_LOG_ERROR, "Invalid source volume specified\n");
                        rv = false;
                        break;
                    } 
                    request -> src = srclinkname; 

                    std::string destlinkname = request -> dest;
                    if (!GetLinkNameIfRealDeviceName(request -> dest, destlinkname))
                    {
                        DebugPrintf(SV_LOG_ERROR, "Invalid destination volume specified\n");
                        rv = false;
                        break;
                    } 
                    request -> dest = destlinkname; 
                }

                FirstCharToUpperForWindows(request -> src);
                FirstCharToUpperForWindows(request -> dest);

                if(db.empty())
                {
                    if(!getCdpDbName(request ->src,request ->dbpath))
                    {
                        rv = false;
                        break;
                    }

                    if(request ->dbpath == "")
                    {
                        DebugPrintf(SV_LOG_ERROR, "Retention is not enabled for %s\n", request ->src.c_str());
                        rv = false;
                        break;
                    }
                } else
                {
                    request -> dbpath = db;
                }
                CDPUtil::trim(request -> dbpath);

                if (!request->destMountPoint.empty())
                {
                    std::string strerr;
                    if (!isvalidmountpoint(request->src, strerr))
                    {
                        DebugPrintf(SV_LOG_ERROR, "mountpoint option is not valid for source = %s, destination = %s, mountpoint = %s\n", 
                            request->src.c_str(), request->dest.c_str(), request->destMountPoint.c_str());
                        DebugPrintf(SV_LOG_ERROR, "The reason is: %s\n", strerr.c_str());
                        rv = false;
                        break;
                    } 
                }
                if(SetSkipCheckFlag())
                {
                    if(!SetSrcVolumeSize(request->srcVolCapacity))
                    {
                        rv = false;
                        break;
                    }
                }
				inm_sprintf_s(pairid, ARRAYSIZE(pairid), "clirecover%d", pairnum);
                request -> operation = SNAPSHOT_REQUEST::RECOVERY_SNAPSHOT;
                request -> id = pairid;
                requests.insert(requests.end(),SNAPSHOT_REQUEST_PAIR(pairid,request));
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "The retention logs for volume %s are being moved. Please try after sometime. \n",
                    devicename.c_str());
                rv = false;
                break;
            }
        }

        if(requests.empty())
        {
            DebugPrintf(SV_LOG_INFO, "Please specify the source and destination volumes for recovery.\n\n");
            usage(argv[0], m_operation);
            rv = false;
            break;
        }

    } while (0);

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::getrollbackpairsanddb
*
* DESCRIPTION : parse the user input and fill in the rollback volume(s)
*               details, retention database path
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
*   
*
* return value : true if the operation succeeds, otherwise false.
*
*/
bool CDPCli::getrollbackpairsanddb(SNAPSHOT_REQUESTS & requests)
{
    NVPairs::iterator iter;
    bool rv = true;
    bool volgrp = false;
    bool notvolgrp = false;
    string dest,mtpt,db;
    int pairnum = 0;
    bool cleanupflag = false;
    char pairid[100];

    do
    {
        cleanupflag = SetDeleteRetention();
        iter = nvpairs.find("rollbackpairs");
        if(iter != nvpairs.end())
        {
            volgrp = true;
            string token = iter -> second;
            if(token == "all")
            {
                rv = getrollbackpairsanddbfromcx(requests);
            }
            else
            {

                svector_t svpairs = CDPUtil::split(token, ";");
                svector_t::iterator svpairs_iter = svpairs.begin();
                for( ; svpairs_iter != svpairs.end() ; ++svpairs_iter)
                {

                    string svpairinfo = *svpairs_iter;
                    svector_t svpair = CDPUtil::split(svpairinfo, ",");
                    if(svpair.size() < 1 || svpair.size() > 3 )
                    {
                        DebugPrintf(SV_LOG_ERROR, "syntax error near %s\n\n", svpairinfo.c_str());
                        usage(argv[0], m_operation);
                        rv = false;
                        break;
                    }
                    string devicename = svpair[0];
                    CDPUtil::trim(devicename);
                    bool shouldRun = true;
                    bool foundstate = ShouldOperationRun(devicename,shouldRun);
                    if(shouldRun)
                    {
                        SNAPSHOT_REQUEST::Ptr request(new SNAPSHOT_REQUEST());
                        request -> src = svpair[0];
                        request -> dest = svpair[0];
                        if(svpair.size() > 1)
                            request -> destMountPoint = svpair[1];

                        CDPUtil::trim(request -> src);
                        CDPUtil::trim(request -> dest);
                        CDPUtil::trim(request -> destMountPoint);

                        if (IsReportingRealNameToCx())
                        {
                            GetDeviceNameFromSymLink(request -> src);
                        }
                        else
                        {
                            std::string srclinkname = request -> src;
                            if (!GetLinkNameIfRealDeviceName(request -> src, srclinkname))
                            {
                                DebugPrintf(SV_LOG_ERROR, "Invalid rollback volume specified\n");
                                rv = false;
                                break;
                            } 
                            request -> src = srclinkname; 
                        }

                        request -> dest = request -> src;

                        FirstCharToUpperForWindows(request -> src);
                        FirstCharToUpperForWindows(request -> dest);

                        if(svpair.size() > 2)
                        {
                            request -> dbpath = svpair[2];
                            /** Added by BSR - Issue#5288 **/
                            if( !resolvePathAndVerify( request->dbpath ) ) 
                            {
                                DebugPrintf( SV_LOG_ERROR, "Invalid DB path %s\n", svpair[2].c_str() ) ;
                                rv = false;
                                break;
                            }
                            /** End of the change **/
                        } 
                        else  // check if retention path can be obtained from local persistent configuration
                        {
                            if(!getCdpDbName(request -> src, request ->dbpath))
                            {
                                rv = false;
                                break;
                            }

                            if(request ->dbpath == "")
                            {
                                DebugPrintf(SV_LOG_ERROR, "Retention is not enabled for %s\n", request -> src.c_str());
                                rv = false;
                                break;
                            }

                        }

                        CDPUtil::trim(request -> dbpath);

                        if (!request->destMountPoint.empty())
                        {
                            std::string strerr;
                            if (!isvalidmountpoint(request->src, strerr))
                            {
                                DebugPrintf(SV_LOG_ERROR, "mountpoint option is not valid for destination = %s, mountpoint = %s\n", 
                                    request->dest.c_str(), request->destMountPoint.c_str());
                                DebugPrintf(SV_LOG_ERROR, "The reason is: %s\n", strerr.c_str());
                                rv = false;
                                break;
                            } 
                        }

                        ++pairnum;
						inm_sprintf_s(pairid, ARRAYSIZE(pairid), "clirollback%d", pairnum);
                        request -> operation = SNAPSHOT_REQUEST::ROLLBACK;
                        request -> id = pairid;
                        request -> cleanupRetention = cleanupflag;
                        requests.insert(requests.end(),SNAPSHOT_REQUEST_PAIR(pairid,request));
                    }
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, 
                            "The retention logs for volume %s are being moved. Please try after sometime. \n",
                            devicename.c_str());
                        rv = false;
                        break;
                    }
                }
            }

            if(!rv)
                break;
        }


        iter = nvpairs.find("dest");
        if(iter != nvpairs.end())
        {
            dest = iter -> second;
            notvolgrp = true;
        }

        iter = nvpairs.find("mountpoint");
        if(iter != nvpairs.end())
        {
            mtpt = iter -> second;
            notvolgrp = true;
        }

        iter = nvpairs.find("db");
        if(iter != nvpairs.end())
        {
            db = iter -> second;
            notvolgrp = true;
        }


        if(volgrp && notvolgrp)
        {
            DebugPrintf(SV_LOG_ERROR," \'rollbackpairs\' and (dest ,mountpoint, db) are mutually exclusive options.\n\n");
            usage(argv[0], m_operation);
            rv = false;
            break;
        }

        if(notvolgrp)
        {
            string devicename = dest;
            CDPUtil::trim(devicename);
            bool shouldRun = true;
            bool foundstate = ShouldOperationRun(devicename,shouldRun);
            if(shouldRun)
            {
                SNAPSHOT_REQUEST::Ptr request(new SNAPSHOT_REQUEST());
                if(dest.empty())
                {
                    DebugPrintf(SV_LOG_ERROR, " please specify rollback volume.\n\n");
                    usage(argv[0], m_operation);
                    rv = false;
                    break;
                }

                request -> src = dest;
                request -> dest = dest;
                if(!mtpt.empty())
                    request -> destMountPoint = mtpt;

                CDPUtil::trim(request -> src);
                CDPUtil::trim(request -> dest);
                CDPUtil::trim(request -> destMountPoint);

                if (IsReportingRealNameToCx())
                {
                    GetDeviceNameFromSymLink(request -> src);
                }
                else
                {
                    std::string srclinkname = request -> src;
                    if (!GetLinkNameIfRealDeviceName(request -> src, srclinkname))
                    {
                        DebugPrintf(SV_LOG_ERROR, "Invalid rollback volume specified\n");
                        rv = false;
                        break;
                    } 
                    request -> src = srclinkname; 
                }

                request -> dest = request -> src;

                FirstCharToUpperForWindows(request -> src);
                FirstCharToUpperForWindows(request -> dest);

                if(db.empty())
                {
                    if(!getCdpDbName(request -> src, request ->dbpath))
                    {
                        rv = false;
                        break;
                    }

                    if(request ->dbpath == "")
                    {
                        DebugPrintf(SV_LOG_ERROR, "Retention is not enabled for %s\n", request -> src.c_str());
                        rv = false;
                        break;
                    }
                } else
                {
                    request -> dbpath = db;
                }
                CDPUtil::trim(request -> dbpath);

                if (!request->destMountPoint.empty())
                {
                    std::string strerr;
                    if (!isvalidmountpoint(request->src, strerr))
                    {
                        DebugPrintf(SV_LOG_ERROR, "mountpoint option is not valid for destination = %s, mountpoint = %s\n", 
                            request->dest.c_str(), request->destMountPoint.c_str());
                        DebugPrintf(SV_LOG_ERROR, "The reason is: %s\n", strerr.c_str());
                        rv = false;
                        break;
                    } 
                }

				inm_sprintf_s(pairid, ARRAYSIZE(pairid), "clirollback%d", pairnum);
                request -> operation = SNAPSHOT_REQUEST::ROLLBACK;
                request -> id = pairid;
                request -> cleanupRetention = cleanupflag;
                requests.insert(requests.end(),SNAPSHOT_REQUEST_PAIR(pairid,request));
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "The retention logs for volume %s are being moved. Please try after sometime. \n",
                    devicename.c_str());
                rv = false;
                break;
            }
        }

        if(requests.empty())
        {
            DebugPrintf(SV_LOG_INFO, "Please specify the volumes for rollback.\n\n");
            usage(argv[0], m_operation);
            rv = false;
            break;
        }

    } while (0);

    return rv;
}


/*
* FUNCTION NAME :  CDPCli::getrollbackpairsanddbfromcx
*
* DESCRIPTION : fetch all the retention enabled target volumes
*               ,retention path  from local persistent store (cx)
*
*
* INPUT PARAMETERS : none
*
* OUTPUT PARAMETERS : none
*
* NOTES :
*   
*
* return value : true if the operation succeeds, otherwise false.
*
*/
bool CDPCli::getrollbackpairsanddbfromcx(SNAPSHOT_REQUESTS &requests)
{
    bool rv = true;
    int pairnum = 0;
    char pairid[100];
    bool cleanupflag = false;

    do
    {
        cleanupflag = SetDeleteRetention();
        HOST_VOLUME_GROUP_SETTINGS hostVolumeGroupSettings; 

        if(!getHostVolumeGroupSettings(hostVolumeGroupSettings))
        {
            return false;
        }

        VOLUME_GROUP_SETTINGS::volumes_iterator volumeIter;
        VOLUME_GROUP_SETTINGS::volumes_iterator volumeEnd;


        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator 
            volumeGroupIter(hostVolumeGroupSettings.volumeGroups.begin());
        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator 
            volumeGroupEnd(hostVolumeGroupSettings.volumeGroups.end());

        for (/* empty */; volumeGroupIter != volumeGroupEnd; ++volumeGroupIter) 
        {
            volumeIter = (*volumeGroupIter).volumes.begin();
            volumeEnd = (*volumeGroupIter).volumes.end();

            if(TARGET == (*volumeGroupIter).direction)
            {

                for(/* empty*/; volumeIter != volumeEnd; ++volumeIter) 
                {
                    if(VOLUME_SETTINGS::SYNC_DIFF == (*volumeIter).second.syncMode)
                    {
                        VOLUME_SETTINGS volsettings = (*volumeIter).second;
                        std::string devicename = (*volumeIter).second.deviceName;
                        bool shouldRun = true;
                        bool foundstate = ShouldOperationRun(devicename,shouldRun);
                        if(shouldRun)
                        {
                            SNAPSHOT_REQUEST::Ptr request(new SNAPSHOT_REQUEST());
                            request -> src = volsettings.deviceName;
                            request -> dest = volsettings.deviceName;
                            request -> destMountPoint = volsettings.mountPoint;

                            CDPUtil::trim(request -> src);
                            CDPUtil::trim(request -> dest);
                            CDPUtil::trim(request -> destMountPoint);

                            if(!getCdpDbName(request -> src, request ->dbpath))
                            {
                                rv = false;
                                break;
                            }

                            if(request ->dbpath == "")
                            {
                                DebugPrintf(SV_LOG_INFO, "Retention is not enabled for %s. ignoring ...\n", 
                                    request -> src.c_str());
                                continue;
                            }

                            /* This has to be taken care by CX */
                            if (!request->destMountPoint.empty())
                            {
                                std::string strerr;
                                if (!isvalidmountpoint(request->src, strerr))
                                {
                                    DebugPrintf(SV_LOG_ERROR, "mountpoint option is not valid for source = %s, destination = %s, mountpoint = %s\n", 
                                        request->src.c_str(), request->dest.c_str(), request->destMountPoint.c_str());
                                    DebugPrintf(SV_LOG_ERROR, "The reason is: %s\n", strerr.c_str());
                                    rv = false;
                                    break;
                                } 
                            }

                            ++pairnum;
							inm_sprintf_s(pairid, ARRAYSIZE(pairid), "clirollback%d", pairnum);
                            request -> operation = SNAPSHOT_REQUEST::ROLLBACK;
                            request -> id = pairid;
                            request -> cleanupRetention = cleanupflag;
                            requests.insert(requests.end(),SNAPSHOT_REQUEST_PAIR(pairid,request));
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_ERROR, 
                                "The retention logs for volume %s are being moved. Please try after sometime. \n",
                                volsettings.deviceName.c_str());
                            rv = false;
                            break;
                        }
                    }

                    if(!rv)
                        break;
                }
            }

            if(!rv)
                break;
        }

    } while(0);

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::getrecoverypoint
*
* DESCRIPTION : parse the user input and fill in the recovery 
*               point
*
*
* INPUT PARAMETERS : SNAPSHOT_REQUESTS
*
* OUTPUT PARAMETERS : SNAPSHOT_REQUESTS
*
* NOTES :
*   
*
* return value : true if the operation succeeds, otherwise false.
*
*/
bool CDPCli::getrecoverypoint(SNAPSHOT_REQUESTS &requests)
{
    bool rv = true;
    int numpts =0 ;

    bool recentccp =false; // recent crash consistent point
    bool recentfscp = false; // recent fs consistent point
    bool recentappcp = false; // recent app consistent point
    bool pickLatestTag = false;
    bool pickOldestTag = false;

    string low;
    string high;
    string at;
    string app;
    string eventbuf;
    string eventnumbuf;
    string aftertime;
    string beforetime;

    string recoverytag;
    string recoverypt;
    SV_ULONGLONG sequenceId = 0;
    std::string identifier;


    SV_TIME svtime;
    NVPairs::iterator iter;

    do
    {
        iter = nvpairs.find("at");
        if(iter != nvpairs.end())
        {
            numpts +=1;
            at = iter -> second;
        }

        iter = nvpairs.find("app");
        if(iter != nvpairs.end())
            app = iter -> second;

        iter = nvpairs.find("event");
        if(iter != nvpairs.end())
        {
            numpts += 1;
            eventbuf = iter -> second;
        }

        iter = nvpairs.find("picklatest");
        if(iter != nvpairs.end())
        {
            pickLatestTag = true;
        }
        iter = nvpairs.find("pickoldest");
        if(iter != nvpairs.end())
        {
            pickOldestTag = true;
        }
        iter = nvpairs.find("eventnum");
        if(iter != nvpairs.end())
        {
            numpts += 1;
            eventnumbuf = iter -> second;
        }

        iter = nvpairs.find("aftertime");
        if(iter != nvpairs.end())
        {
            numpts += 1;
            aftertime = iter -> second;
        }

        iter = nvpairs.find("beforetime");
        if(iter != nvpairs.end())
        {
            numpts += 1;
            beforetime = iter -> second;
        }

        iter = nvpairs.find("recentcrashconsistentpoint");
        if(iter != nvpairs.end())
        {
            numpts += 1;
            recentccp = true;
        }

        iter = nvpairs.find("recentfsconsistentpoint");
        if(iter != nvpairs.end())
        {
            numpts += 1;
            recentfscp = true;
        }

        iter = nvpairs.find("recentappconsistentpoint");
        if(iter != nvpairs.end())
        {
            numpts += 1;
            recentappcp = true;
        }

        iter = nvpairs.find("timerange");
        if(iter != nvpairs.end())
        {
            string token = iter -> second;
            svector_t svpairs = CDPUtil::split(token, ",");
            if(svpairs.size() != 2)
            {
                DebugPrintf(SV_LOG_ERROR, "syntax error near %s\n", token.c_str());
                rv = false;
                break;
            }
            low = svpairs[0];
            high = svpairs[1];
            CDPUtil::trim(low);
            CDPUtil::trim(high);
        }	

        if(numpts == 0)
        {
            DebugPrintf(SV_LOG_ERROR, "please specify a recovery point.\n");
            rv = false;
            break;
        }

        if(numpts > 1)
        {
            DebugPrintf(SV_LOG_ERROR, "please do not use multiple options for recovery point.\n");
            rv = false;
            break;
        }

        vector<string> dbs;

        SNAPSHOT_REQUESTS::iterator requests_iter = requests.begin();
        SNAPSHOT_REQUESTS::iterator requests_end = requests.end();

        for( /* empty */ ; requests_iter != requests_end; ++requests_iter)
        {
            std::string id = (*requests_iter).first;
            SNAPSHOT_REQUEST::Ptr request = (*requests_iter).second;  
            string db = request -> dbpath;
            dbs.push_back(db);
        }

        CDPEvent cdpevent;

        if(recentfscp)
            app = "FS";
        else if(app.empty())
            app = "USERDEFINED";

        SV_EVENT_TYPE type;
        if(!VacpUtil::AppNameToTagType(app, type))
        {
            rv = false;
            break;
        }

        SV_ULONGLONG afterts = 0, beforets = 0, atts = 0, lowts = 0, hights = 0;

        if(!high.empty())
        {
            if(!CDPUtil::InputTimeToFileTime(high, hights))
            {
                rv = false;
                break;
            }
        }

        if(!low.empty())
        {
            if(!CDPUtil::InputTimeToFileTime(low, lowts))
            {
                rv = false;
                break;
            }
        }

        if(!at.empty())
        {
            if(!CDPUtil::InputTimeToFileTime(at, atts))
            {
                rv = false;
                break;
            }

            bool isconsistent = false; // out parameter
			bool isavailable = false; // out parameter
            // returns true if the time is within recovery time range for all the volumes
            if(!CDPUtil::isCCP(dbs,atts, isconsistent,isavailable))
            {
                rv = false;
                break;
            }

			if(!isavailable)
			{
				DebugPrintf(SV_LOG_INFO, "Note: The specified time %s is not a valid recovery point.\n", at.c_str());
				rv = false;
				break;
			}

            if(!isconsistent)
            {
                DebugPrintf(SV_LOG_INFO, "Note: The specified time %s is not a crash consistent point for all the volumes.\n", at.c_str());
            }


            recoverytag.clear();
            CDPUtil::ToSVTime(atts,svtime);
        }

        if(!aftertime.empty())
        {
            if(!CDPUtil::InputTimeToFileTime(aftertime, afterts))
            {
                rv = false;
                break;
            }

            if(!CDPUtil::MostRecentConsistentPoint(dbs, cdpevent, type, afterts))
            {
                rv = false;
                break;
            }

            recoverytag = cdpevent.c_eventvalue;
            identifier = cdpevent.c_identifier;
            CDPUtil::ToSVTime(cdpevent.c_eventtime,svtime);
        }

        if(!beforetime.empty())
        {
            if(!CDPUtil::InputTimeToFileTime(beforetime, beforets))
            {
                rv = false;
                break;
            }

            //afterts has to be zero
            if(!CDPUtil::MostRecentConsistentPoint(dbs, cdpevent, type, afterts, beforets))
            {
                rv = false;
                break;
            }

            recoverytag = cdpevent.c_eventvalue;
            identifier = cdpevent.c_identifier;
            CDPUtil::ToSVTime(cdpevent.c_eventtime,svtime);
        }


        if(recentccp)
        {
            SV_ULONGLONG ts;
            if(!CDPUtil::MostRecentCCP(dbs, ts,  sequenceId, lowts,hights))
            {
                rv = false;
                break;
            }

            recoverytag.clear();
            CDPUtil::ToSVTime(ts,svtime);
        }

        if(recentfscp || recentappcp)
        {
            if(!CDPUtil::MostRecentConsistentPoint(dbs, cdpevent, type, lowts,hights))
            {
                rv = false;
                break;
            }

            recoverytag = cdpevent.c_eventvalue;
            identifier = cdpevent.c_identifier;
            CDPUtil::ToSVTime(cdpevent.c_eventtime,svtime);
        }

        if(!eventbuf.empty())
        {
            SV_ULONGLONG ts;
            if(pickLatestTag)
            {
                if(!CDPUtil::PickLatestEvent(dbs,type, eventbuf,identifier,ts))
                {
                    rv = false;
                    break;
                }
            } else if(pickOldestTag)
            {
                if(!CDPUtil::PickOldestEvent(dbs,type, eventbuf,identifier,ts))
                {
                    rv = false;
                    break;
                }
            }
            else
            {
                if(!CDPUtil::FindCommonEvent(dbs,type, eventbuf,identifier, ts))
                {
                    rv = false;
                    break;
                }
            }

            recoverytag = eventbuf;
            CDPUtil::ToSVTime(ts,svtime);
        }

        if(!eventnumbuf.empty())
        {
            //only one pair is allowed
            // find the db
            if(dbs.size() > 1)
            {
                DebugPrintf(SV_LOG_ERROR, "option --eventnum is available for single volume recovery.\n");
                rv = false;
                break;
            }

            string db = *dbs.begin();

            const char * c = eventnumbuf.c_str();
            while(*c != '\0' )
            {
                if(!isdigit((int)(*c)))
                {
                    DebugPrintf(SV_LOG_ERROR, "please specify event number as integer greater than zero.\n");
                    rv = false;
                    break;
                }
                c++;
            }
            if(!rv)
                break;
            try
            {
                int eventnum = atoi(eventnumbuf.c_str());
                if(!CDPUtil::getNthEvent(db,type,eventnum, cdpevent))
                {
                    rv = false;
                    break;
                }

                recoverytag = cdpevent.c_eventvalue;
                CDPUtil::ToSVTime(cdpevent.c_eventtime,svtime);
            } catch(exception &)
            {
                DebugPrintf(SV_LOG_ERROR, "please specify event number as integer greater than zero.\n");
                rv = false;
                break;
            }
        }
        if(identifier.empty())
        {
            char buff[200];
			inm_sprintf_s(buff, ARRAYSIZE(buff), "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
                svtime.wYear,
                svtime.wMonth,
                svtime.wDay,
                svtime.wHour,
                svtime.wMinute,
                svtime.wSecond,
                svtime.wMilliseconds,
                svtime.wMicroseconds,
                svtime.wHundrecNanoseconds);

            recoverypt = buff;

            string displaytime;
            CDPUtil::CXInputTimeToConsoleTime(recoverypt,displaytime);
            DebugPrintf(SV_LOG_INFO, 
                "Selected Recovery Point: %s\n", displaytime.c_str());
            if(sequenceId)
            {
                DebugPrintf(SV_LOG_INFO, "Corresponding I/O  Sequence Point: " ULLSPEC "\n", sequenceId);
            }
        }

    } while (0);

    if(rv)
    {
        SNAPSHOT_REQUESTS::iterator requests_iter = requests.begin();
        SNAPSHOT_REQUESTS::iterator requests_end = requests.end();

        for( /* empty */ ; requests_iter != requests_end; ++requests_iter)
        {
            SNAPSHOT_REQUEST::Ptr request = (*requests_iter).second;  
            if(!identifier.empty())
            {
                if(!CDPUtil::getEventByIdentifier(request -> dbpath,recoverypt,identifier))
                {
                    rv = false;
                    break;
                }
                string displaytime;
                CDPUtil::CXInputTimeToConsoleTime(recoverypt,displaytime);
                stringstream displaystr;
                displaystr << "Target volume: " << request -> dest << "\n"
                    << "Selected recovery point: " << displaytime << "\n"
                    << "BookMark: " << recoverytag << "\n"
                    << "Identifier: " << identifier << "\n";
                DebugPrintf(SV_LOG_INFO, "%s\n", displaystr.str().c_str());

            }
            request -> recoverytag = recoverytag;
            request -> recoverypt = recoverypt;
            request -> sequenceId = sequenceId;
        }
    }

    return rv;
}


/*
* FUNCTION NAME :  CDPCli::getprepostscript
*
* DESCRIPTION : parse the user input and fill in the pre/post script 
*               information
*
*
* INPUT PARAMETERS : SNAPSHOT_REQUESTS
*
* OUTPUT PARAMETERS : SNAPSHOT_REQUESTS
*                     prescript and postscript for the group
*
* NOTES :
*   
*
* return value : true if the operation succeeds, otherwise false.
*
*/
bool CDPCli::getprepostscript(SNAPSHOT_REQUESTS &requests, string & prescript, string & postscript)
{

    bool rv = true;
    string prescr, postscr;
    bool runonce = false;
    NVPairs::iterator iter;

    prescript.clear();
    postscript.clear();

    iter = nvpairs.find("prescript");
    if(iter != nvpairs.end())
    {
        prescr = iter -> second;
    }

    iter = nvpairs.find("postscript");
    if(iter != nvpairs.end())
    {
        postscr = iter -> second;
    }

    iter = nvpairs.find("runonce");
    if(iter != nvpairs.end())
    {
        runonce = true;
    }

    if(runonce)
    {
        prescript = prescr;
        postscript = postscr;
    } else 
    {
        SNAPSHOT_REQUESTS::iterator requests_iter = requests.begin();
        SNAPSHOT_REQUESTS::iterator requests_end = requests.end();

        for( /* empty */ ; requests_iter != requests_end; ++requests_iter)
        {
            SNAPSHOT_REQUEST::Ptr request = (*requests_iter).second;  
            request -> prescript = prescr;
            request -> postscript = postscr;
        }
    }

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::ProcessSnapshotRequests
*
* DESCRIPTION : create the snapshot tasks and start them
*               
*
*
* INPUT PARAMETERS : SNAPSHOT_REQUESTS
*                    prescript and postscript for the group
*					 parallel(true to process the requests parallelly)
*					 
*
* OUTPUT PARAMETERS : none
*                     
*
* NOTES : If parallel is set to true, then the tasks are scheduled parallelly.
*		  Otherwise, the tasks are scheduled sequentially
*   
*
* return value : true if the operation succeeds, otherwise false.
*
*/
bool CDPCli::ProcessSnapshotRequests(SNAPSHOT_REQUESTS &requests, 
                                     const string & prescript, const string & postscript, bool parallel)
{
    bool ok =true;

    SnapshotTasks_t snapshotTasks;
    ACE_Shared_MQ mq(new ACE_Message_Queue<ACE_MT_SYNCH>() );

    SNAPSHOT_REQUESTS::iterator requests_iter = requests.begin();
    SNAPSHOT_REQUESTS::iterator requests_end = requests.end();

    for( /* empty */ ; requests_iter != requests_end; ++requests_iter)
    {

        std::string id = (*requests_iter).first;
        SNAPSHOT_REQUEST::Ptr request = (*requests_iter).second;          	
        FormatVolumeName(request -> src); 
        FormatVolumeName(request -> dest);
    }

	
    //
    // step 1 : pre-requisite checks
    //

	bool skip_replication_pause = SkipReplicationPause();

	for( requests_iter = requests.begin() ; requests_iter != requests_end; ++requests_iter)
	{

		std::string id = (*requests_iter).first;
		SNAPSHOT_REQUEST::Ptr request = (*requests_iter).second; 

		if(isResyncing(request -> src))
		{

			if(request -> operation == SNAPSHOT_REQUEST::PLAIN_SNAPSHOT)
			{
				DebugPrintf(SV_LOG_INFO, 
					"%s is undergoing resync. Current point in time snapshot operation is not allowed\n", request ->src.c_str());
				return false;

			} else if(!skip_replication_pause)
			{
				// in order to acquire the volume lock, pause the replication
				DebugPrintf(SV_LOG_INFO,
					"%s is undergoing resync. Sending a Pause Request for the replication to allow recovery operation\n",request ->src.c_str());

				
				int hostType = 1 ; //TODO: use a macro
				string volumeName = request -> src;
				FormatVolumeNameForCxReporting(volumeName);
				string ReasonForPause = "Replication is paused to allow snapshot/recovery operation";
				ReplicationPairOperations pairOp;

				if(pairOp.PauseReplication(m_configurator, volumeName, hostType, ReasonForPause.c_str(),0))
				{
					DebugPrintf(SV_LOG_INFO,"Replication paused for Volume: %s.\n",
						volumeName.c_str());
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to pause replication pair for %s.\n",
						volumeName.c_str());
					return false;
				}
			}
		}
	}

    //
    // step 2: if user has specified volume group level prescript, execute same
    //
    if(!CDPUtil::runscript(prescript))
    {
        return false;
    }

    //
    // step 3: create the snapshot tasks
    //


	for( requests_iter = requests.begin() ; requests_iter != requests_end; ++requests_iter)
	{

		std::string id = (*requests_iter).first;
		SNAPSHOT_REQUEST::Ptr request = (*requests_iter).second; 

        SnapshotTaskPtr snapTask(new SnapshotTask(id, mq,request, &m_ThreadMgr));
        //BugId:6701   setting the flag for source volume if the operation is rollback
        if(request -> operation == SNAPSHOT_REQUEST::ROLLBACK)
        {
            snapTask ->SetVolumeRollback(SetRollbackFlag());
            snapTask ->SetStopReplicationFlag(IsStopReplicationFlagSet());
        }

        if(request -> operation == SNAPSHOT_REQUEST::PLAIN_SNAPSHOT)
        {
            snapTask ->SetVolumeRollback(SetSnapshotFlag());
        }

        if((request -> operation == SNAPSHOT_REQUEST::PLAIN_SNAPSHOT) ||
            (request -> operation == SNAPSHOT_REQUEST::RECOVERY_SNAPSHOT))
        {
            snapTask ->SetUseFSAwareCopy(use_fsaware_copy());
        }

        if((request -> operation == SNAPSHOT_REQUEST::RECOVERY_VSNAP) ||
            (request -> operation == SNAPSHOT_REQUEST::RECOVERY_SNAPSHOT) ||
            (request -> operation == SNAPSHOT_REQUEST::PLAIN_SNAPSHOT) ||
            (request -> operation == SNAPSHOT_REQUEST::ROLLBACK))
        {
            snapTask ->SetSkipCheck(SetSkipCheckFlag());
            if(do_not_usedirectio_option())
                snapTask -> do_not_use_directio();
        }
        if( -1 == snapTask ->open()) 
        {
            DebugPrintf(SV_LOG_INFO, "Internal Error while creating process\n");
            ok = false;
            break;
        }

        snapshotTasks.insert(snapshotTasks.end(), SNAPSHOTTASK_PAIR(id,snapTask));	
        if(!parallel)
        {
            if(!WaitForSnapshotTasks(snapshotTasks, mq))
            {
                ok =false;
            }

            if(snapTask->Quit())
            {
                DebugPrintf(SV_LOG_INFO, "Quit request received. Aborting ...\n");
                ok = false;
                break;	
            }
            snapshotTasks.clear();
        }
    }

    if(parallel)
    {
        if(snapshotTasks.empty())
        {
            return ok;
        }

        //
        // step 4: wait for the tasks to complete
        //         then run the vg level postscript if specified by user
        //
        if(!WaitForSnapshotTasks(snapshotTasks, mq))
        {
            ok = false;
        }
    }

    if(!CDPUtil::runscript(postscript))
    {
        ok = false;
    }

	//Print Rollback stats on console
	for (requests_iter = requests.begin() ; requests_iter != requests_end; ++requests_iter)
	{
		SNAPSHOT_REQUEST::Ptr request = (*requests_iter).second;

		if (request -> operation == SNAPSHOT_REQUEST::ROLLBACK)
		{
			std::string id = (*requests_iter).first;

			SnapshotTasks_t::iterator iter(snapshotTasks.begin());
			SnapshotTasks_t::iterator end(snapshotTasks.end());
			for(; iter != end; ++iter)
			{
				if(id.compare((*iter).first.c_str()) == 0 )
				{
					SnapshotTaskPtr taskptr = (*iter).second;
					RollbackStats_t stats = taskptr -> get_io_stats();
					Print_RollbackStats(stats);
				}
			}
		}
	}


	for( requests_iter = requests.begin() ; requests_iter != requests_end; ++requests_iter)
	{

		std::string id = (*requests_iter).first;
		SNAPSHOT_REQUEST::Ptr request = (*requests_iter).second; 

		if(isResyncing(request -> src))
		{
			if(!skip_replication_pause && (request -> operation != SNAPSHOT_REQUEST::PLAIN_SNAPSHOT))
			{
				// after finishing the recover operation, resume back the replication for
				// resync to progress

				DebugPrintf(SV_LOG_INFO,
					"Sending a Resume replication request for %s\n",request ->src.c_str());

				int hostType = 1 ; //TODO: use a macro
				string volumeName = request -> src;
				FormatVolumeNameForCxReporting(volumeName);
				string ReasoneForResume = "Replication is resumed after completion of snapshot/recovery operation";
				ReplicationPairOperations pairOp;

				if(pairOp.ResumeReplication(m_configurator, volumeName, hostType, ReasoneForResume.c_str()))
				{
					DebugPrintf(SV_LOG_INFO,"Replication resumed for Volume: %s.\n",
						volumeName.c_str());
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to resume replication pair for %s.\n",
						volumeName.c_str());
					return false;
				}
			}
		}
	}

    return ok;
}

/*
* FUNCTION NAME :  CDPCli::StopAbortedSnapshots
*
* DESCRIPTION : send a stop request to snapshot task on abort
*               request by user (CTRL-C)
*
*
* INPUT PARAMETERS : snapshot tasks
*                    message queue to send stop request
*
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : none
*
*/
void CDPCli::StopAbortedSnapshots(SnapshotTasks_t tasks, ACE_Shared_MQ & mq)
{

    SnapshotTasks_t::iterator iter(tasks.begin());
    SnapshotTasks_t::iterator end(tasks.end());
    DebugPrintf(SV_LOG_INFO,"\nAborting .... \n");

    for (/* empty */; iter != end; ++iter) {
        SnapshotTaskPtr taskptr = (*iter).second;
        taskptr ->PostMsg(CDP_ABORT, CDP_HIGH_PRIORITY);			
    }
}

/*
* FUNCTION NAME :  CDPCli::PrintSnapshotStatusMessage
*
* DESCRIPTION : send the status/progress information from tasks
*               to console.
*
*
* INPUT PARAMETERS : status/progress message
*                    
*
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : none
*
*/
void CDPCli::PrintSnapshotStatusMessage(const CDPMessage * msg)
{

    std::string operationState;

    switch(msg -> executionState)
    {
    case -1:
        operationState = "Not started";
        break;

    case 0:
        operationState = "Queued";
        break;

    case 1:
        operationState = "Ready";
        break;

    case 2:
        operationState = "Status: complete";
        break;

    case 3:
        operationState = "Status: failed";
        break;

    case 4:
        operationState = "Snapshot started";
        break;

    case 5:
        operationState = "Executing prescript";
        break;

    case 6:
        operationState = "Copy in progress";
        break;

    case 7:
        operationState = "Executing postscript";
        break;

    case 8:
        operationState = "Recovery started";
        break;

    case 9:
        operationState = "Executing prescript";
        break;

    case 10:
        operationState = "Snapshot phase in progress";
        break;

    case 11:
        operationState = "Snapshot phase complete. starting recovery phase";
        break;

    case 12:
        operationState = "Recovery phase in progress";
        break;

    case 13:
        operationState = "Executing postscript";
        break;

    case 16:
        operationState = "Rollback Started";
        break;

    case 17:
        operationState = "Executing Prescript";
        break;

    case 18:
        operationState = "Rollback InProgress";
        break;

    case 19:
        operationState = "Executing PostScript";
        break;

    case 22:
        operationState = "MapGeneration Started";
        break;

    case 23:
        operationState = "Generating vsnap mapping data";
        break;

    case 24:
        operationState = "Mounting Virtual Snapshot";
        break;

    case 25:
        operationState = "Mount In Progress";
        break;
    }

    std::stringstream taskStatus;

    std::string volname = msg ->dest;
    FormatVolumeNameForCxReporting(volname);

    if (msg ->type == CDPMessage::MSG_STATECHANGE)
    {
        taskStatus <<  endl << volname << ":" << operationState << ". ";
        if( msg -> err.length() ){
            if(msg -> executionState == 3)
            {
                taskStatus << endl << " Error: " << msg -> err << endl;
            }else
            {
                taskStatus << endl << msg -> err << endl;
            }
        }

        DebugPrintf(SV_LOG_INFO,"%s\n", taskStatus.str().c_str());	
    }
    else
    {
        if( (msg ->progress % 10) == 0)
        {
            taskStatus << "\r" << volname << ":" << msg -> progress << "%";
			if(msg -> time_remaining_in_secs > 0)
			{
				SV_LONGLONG time_remaining_in_secs = msg -> time_remaining_in_secs;

				taskStatus << "(" ;
				if(time_remaining_in_secs > 3600)
				{
					taskStatus << " " << time_remaining_in_secs/3600 << "hr" ;
					time_remaining_in_secs = time_remaining_in_secs % 3600;
				}

				if (time_remaining_in_secs >= 60)
				{
					taskStatus << " " << time_remaining_in_secs/60 << "min" ;
					time_remaining_in_secs = time_remaining_in_secs % 60;
				}

				if (time_remaining_in_secs >= 1)
				{
					taskStatus << " " << time_remaining_in_secs << "sec" ;
				}

				taskStatus << " remaining )\n" ;
			} else
			{
				taskStatus << "                               \n";
			}

        } else
        {
            taskStatus << "." ;
        }

        //Once DebugPrintf works fine for this enable it
        DebugPrintf(SV_LOG_INFO,"%s", taskStatus.str().c_str());	
        //cout << "\r" << taskStatus.str().c_str();		
    }
}

/*
* FUNCTION NAME :  CDPCli::WaitForSnapshotTasks
*
* DESCRIPTION : wait for status/progress message from tasks
*               send the status to console
*               if user requests abort, send quit request to all tasks
*               wait for all of them to complete
*
*
* INPUT PARAMETERS : status/progress message
*                    
*
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : none
*
*/
bool CDPCli::WaitForSnapshotTasks(SnapshotTasks_t tasks, ACE_Shared_MQ & mq)
{

    bool quit = false;
    bool rv = true;
    unsigned TaskDoneCount = 0;

    while(true) 
    {
        ACE_Message_Block *mb;
        ACE_Time_Value waitTime = ACE_Time_Value::max_time;
        if (-1 == mq -> dequeue_head(mb, &waitTime)) 
        {
            if (errno != EWOULDBLOCK && errno != ESHUTDOWN)
            {
                quit = true;
                rv = false;
                break;
            }
        } else {

            CDPMessage * msg = (CDPMessage*)mb ->base();
            if(msg -> operation != SNAPSHOT_REQUEST::RECOVERY_VSNAP)
            {
                if(progress_callback_fn)
                {
                    progress_callback_fn(msg,progress_callback_param);
                }
                PrintSnapshotStatusMessage(msg);
            }


            if (msg -> executionState == SNAPSHOT_REQUEST::Complete 
                || msg -> executionState == SNAPSHOT_REQUEST::Failed)
                ++TaskDoneCount;

            if(msg -> executionState == SNAPSHOT_REQUEST::Failed)
                rv = false;

            delete msg;
            mb -> release();

            if(TaskDoneCount >= tasks.size())
                break;
        }

        if(CDPUtil::Quit() || quit_requested()){
            StopAbortedSnapshots(tasks,mq);
            rv = false;
            break;
        }
    }

    if(quit)
    {	
        // need to tell all snapshot tasks to quit
        SnapshotTasks_t::iterator iter(tasks.begin());
        SnapshotTasks_t::iterator end(tasks.end());
        for (/* empty */; iter != end; ++iter) {
            SnapshotTaskPtr taskptr = (*iter).second;
            taskptr ->PostMsg(CDP_QUIT, CDP_HIGH_PRIORITY);
        }
    }

    // Note:Do not call threadmanager.wait
    // as this function is invoked from a thread managed
    // by threadmanager
    //m_ThreadManager.wait();

    //wait for snapshot tasks
    SnapshotTasks_t::iterator iter(tasks.begin());
    SnapshotTasks_t::iterator end(tasks.end());
    for (/* empty */; iter != end; ++iter) {
        SnapshotTaskPtr taskptr = (*iter).second;
        m_ThreadMgr.wait_task(taskptr.get());
    }

    //drain any pending messages
    while(true) 
    {
        ACE_Message_Block *mb;
        ACE_Time_Value waitTime;
        if (-1 == mq -> dequeue_head(mb, &waitTime)) 
        {
            if (errno == EWOULDBLOCK || errno == ESHUTDOWN)
            {
                break ;
            }
        }

        else {

            CDPMessage * msg = (CDPMessage*)mb ->base();
            if(msg -> operation != SNAPSHOT_REQUEST::RECOVERY_VSNAP)
            {
                if(progress_callback_fn)
                {
                    progress_callback_fn(msg,progress_callback_param);
                }
                PrintSnapshotStatusMessage(msg);
            }

            if (msg -> executionState == SNAPSHOT_REQUEST::Complete 
                || msg -> executionState == SNAPSHOT_REQUEST::Failed)
                ++TaskDoneCount;

            if(msg -> executionState == SNAPSHOT_REQUEST::Failed)
                rv = false;


            delete msg;
            mb -> release();

            if(TaskDoneCount >= tasks.size())
                break;
        }
    }

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::usage
*
* DESCRIPTION : display usage/command line syntax
*
*
* INPUT PARAMETERS : progname - program name
*                    operation - operation like snapshot/rollback etc
*                    error - if true, print to stderr
*                            else print to stdout
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : none
*
*/
void CDPCli::usage(char * progname, const std::string & operation, bool error)
{
    stringstream out, err;

    if(operation.empty())
    {
        if(error)
        {
            err << "usage error: please use " << progname << " --h  [operation] "  << "to view usage information\n"
                << "\nwhere operation can be one of the following:\n"
                << " validate                 - To validate retention database\n"
                << " showsummary              - To view summary information\n"
                << " listevents               - To list consistency events\n"
                << " snapshot                 - To take a snapshot\n"
                << " recover                  - To perform recovery snapshot\n"
                << " rollback                 - To perform rollback\n"
                << " hide                     - To hide a volume\n"
                << " unhide_ro                - To unhide a volume in read-only mode\n"
                << " unhide_rw                - To unhide a volume in read-write mode\n"	  
                << " vsnap                    - To perform virtual snapshot\n"
                << " virtualvolume            - To create virtual volume\n"
                << " listcommonpoint          - To list common recovery point\n"
                << " iopattern                - To get the io pattern\n"
                << " fixerrors                - To fix the database issues\n"
                << " displaystatistics        - To display the current replication statistics\n"
                << " getsparsepolicysummary   - To get the retention volume size from the sparse policy\n"
                << " gencdpdata               - To generate diffs with overlapping changes\n"
                << " verifiedtag              - To mark a event as verified\n"
                << " lockbookmark             - To lock specified bookmark and prevent it from\n"
                << "                            getting dropped during coalesce operation\n"
                << " unlockbookmark           - To unlock the specified locked bookmark\n"
                //#15949 : Adding showreplicationpairs and showsnapshotrequests options to cdpcli
                << " showreplicationpairs     - To get the volume replication pair information\n"
                << " showsnapshotrequests     - To display the snap shot request\n"
                << " listtargetvolumes        - To display list of target volumes\n"
                << " listprotectedvolumes     - To display list of protected volumes\n"
                << " displaysavings           - To display space savings due to compression,sparse retention and thin provisioning \n"
				<< " deletestalefiles         - To delete stale files in the retention for the given volume\n"
				<< " deleteunusablepoints     - To delete unusable recovery points in the retention database for the given volume\n";

            //16211 - Option to reattach guid to the corresponding drive letters.
#ifdef SV_WINDOWS
            err << " reattachdismountedvolumes - To reattach the guid to drive letter\n";
			err << " processcsjobs - To process cs jobs.\n";
#endif //End of SV_WINDOWS
            err << " volumecompare            - Debug utility to compare two volumes \n"
                << " readvolume               - Debug utility to read volume data to a file \n";

            DebugPrintf(SV_LOG_ERROR, "%s", err.str().c_str());
        }
        else
        {
            out << "\n" << progname << " usage:\n\n"
                << "To validate retention database:                " 
                //<< progname << " --validate --db=<database path>\n\n"
                << progname << " --validate <validateoptions>\n\n"

                << "To view summary information:                   "
                //<< progname << " --showsummary --db=<database path>\n\n"
                << progname << " --showsummary <showsummaryoptions>\n\n"

                << "To list Consistency Events:                    "
                << progname << " --listevents <listoptions>\n\n"

                << "To mark a event as verified:                   "
                << progname << " --verifiedtag <verifiedoptions>\n\n"

                << "To take a snapshot:                            "
                << progname << " --snapshot <snapshotoptions>\n\n"

                << "To perform Recovery Snapshot:                  "
                << progname << " --recover <recoveroptions>\n\n"

                << "To perform Rollback:                           "
                << progname << " --rollback <rollbackoptions>\n\n"

                << "To hide a volume:                              "
                << progname << " --hide <volumename>\n\n"

                << "To unhide a volume in read-only mode:          "
                << progname << " --unhide_ro <volumename>\n\n"

                << "To unhide a volume in read-write mode:         "
                << progname << " --unhide_rw <volumename>\n\n"

                << "To perform virtual recovery snapshot:          "
                << progname << " --vsnap <vsnapoptions>\n\n"

                << "To create virtual volume:		               "
                << progname << " --virtualvolume <virvoloptions>\n\n"

                << "To list common recovery point:                 "
                << progname << " --listcommonpoint <listcommonpoint options>\n\n"

                << "To get the io distribution:                    "
                << progname << " --iopattern <iopattern options>\n\n"

                << "To fix the database issues:                    "
                << progname << " --fixerrors <fixerrorsoptions>\n\n"

                << "To view replication statistics:                "
                << progname << " --displaystatistics <displaystatisticsoptions>\n\n"

                << "To get the retention volume size from the sparse policy:    "
                << progname << " --getsparsepolicysummary <sparseoptions>\n\n"

                << "To generate diffs files:                       "
                << progname << " --gencdpdata (interactive)\n\n"

                << "To lock specified bookmark and prevent it from getting dropped during coalesce\n"
                << "operation                                      "
                << progname << " --lockbookmark <options>\n\n"

                << "To unlock the specified locked bookmark:       "
                << progname << " --unlockbookmark <options>\n\n"

                //#15949 : Adding showreplicationpairs and showsnapshotrequests options to cdpcli
                << "To view the replication pair information:      "
                << progname << " --showreplicationpairs\n\n"

                << "To view the snap shot requests:                "
                << progname << " --showsnapshotrequests\n\n"

                << "To display a list of target volumes:           "
                << progname << " --listtargetvolumes\n\n"

                << "To display a list of protected volumes:        "
                << progname << " --listprotectedvolumes\n\n" 
                << " To display space savings due to compression,sparse retention and thin provisioning:  "
                << progname << " --displaysavings [--volumes=<comma separated target volume names>] \n\n ";

#ifdef SV_WINDOWS
            out<< "To reattach the missing drive letters with their corresponding guids:        "
                << progname << " --reattachdismountedvolumes\n\n";
            out << "To process jobs submitted by configuration server:"
                << progname << " --processcsjobs\n\n";
#endif //End of SV_WINDOWS

            out << "To compare two volumes:                        "
                << progname << " --volumecompare\n\n"
                << "To read data from volume to file:              "
                << progname << " --readvolume\n\n"

			    << "To delete stale files from the retention of the volume:     "
				<< progname << " --deletestalefiles\n\n"

				<< "To delete all the unusable recovery points and bookmarks for a volume:     "
				<< progname << " --deleteunusablepoints\n\n"

                << "To view usage information:                     "
                << progname << " --h [operation]\n\n";

            DebugPrintf(SV_LOG_INFO, "%s\n", out.str().c_str());
        }
    }
    else
    {
        if(operation == "validate")
        {
            out << "\n" << progname << " usage:\n\n"
                << "To validate retention database: " 
                //<< progname << " --validate --db=<database path>\n\n";
                << progname << " --validate <validateoptions>\n\n"
                << "where validateoptions are:\n\n"
                << " --db=<database path>    path for retention database.(Required) OR\n"
                << " --vol=<target volume>   target volume whose retention database will\n"
                << "                         be validated.(Required)\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }

        else if(operation == "showsummary")
        {
            out << "\n" << progname << " usage:\n\n"
                << "To view summary information:  "
                << progname << " --showsummary <showsummaryoptions>\n\n"
                << "where showsummaryoptions are:\n\n"
                << " --db=<database path>    path for retention database.(Required) OR\n"
                << " --vol=<target volume>   target volume whose retention database will be used \n"
                << "                         to show summary.(Required)\n"
                << " --verbose              for verbose output on the Recovery Time Range.(Optional)\n"
				<< " --spaceusage           specify this option to display space used by retention. (Optional)\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }

        else if(operation == "listcommonpoint")
        {
            out << "\n" << progname << " usage:\n\n"
                << "To list common recovery point: "
                << progname << " --listcommonpoint <listcommonpointoptions>\n\n"
                << "where listcommonpointoptions are:\n\n"
                << " --app=<ACM application> list events for specified application.(Optional)\n\n"
                << " --aftertime=<Time> (Optional)\n"
                << "    list the first common consistency event occurring after the specified time\n"
                << "     Time format: yr/mm/dd hr:min:sec:millisec:usec:nanosec\n\n"
                << " --beforetime=<Time> (Optional)\n"
                << "    list the common latest application consistency event\n"
                << "    occurring before the specified time.\n"
                << "    Time format: yr/mm/dd hr:min:sec:millisec:usec:nanosec\n\n"
                << " --volumes=<volume1,volume2...> (Optional)"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }

        else if(operation == "listevents")
        {
            out << "\n" << progname << " usage:\n\n"
                << "To list Consistency Events:  "
                << progname << " --listevents <listoptions>\n\n"
                << "where listoptions are:\n\n"
                << " --db=<database path>    path for retention database.(Required)OR \n"
                << " --vol=<target volume>   target volume whose retention database will be used\n"
                << "                         to listevents.(Required)\n"
                << " --app=<ACM application> list events for specified application.(Optional)\n"
                << " --event=<value>         search for events with specified value.(Optional)\n"
                << " --at=<time>             list event occuring at specified time.should not\n"
                << "                         be used with after and before options.(Optional)\n"
                << " --beforetime=<date>     list events occuring before specified time.(Optional)\n"
                << " --aftertime=<date>      list events occuring after specified time.(Optional)\n"
                << "                         Time format:yr/mm/dd hr:min:sec:millisec:usec:nanosec\n"
                << " --verified=<yes/no>     list verified events\n"
                << " --namevalueformat       To display the output in name:value format\n"
                << " --sort=<column>         sort results based on specified column.column can be\n"
                << "                         time  - sort based on time(default)\n"
                << "                         app   - sort based on application name\n"
                << "                         event - sort based on event value\n"
                << "                         accuracy - sort based on accuracy mode\n\n"
                << " --onlylockedbookmarks 	 List locked bookmarks.\n"
                << "                         This option is applicable only for replication enabled\n"
                << "                         with sparse retention.\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }

        else if(operation == "verifiedtag")
        {
            out << "\n" << progname << " usage:\n\n"
                << "To mark event as verified:  "
                << progname << " --verifiedtag <verifyoptions>\n\n"
                << "where verifyoptions are:\n\n"
                << " --db=<database path>    path for retention database.(Required)OR \n"
                << " --vol=<target volume>   target volume whose retention database will be used\n"
                << "                         to verifiedtag.(Required)\n"
                << " --comment=<text>        Mark the event as verified with the given comment.(Required)\n"
                << " --app=<ACM application> Mark the event as verified for specified application.\n"
                << "                         With this option one of the option(--event/--time) is mandatory.\n"
                << " --event=<value>         Mark the event as verified for specified value.\n"
                << "                         This option used along with --app option.\n"
                << " --time=<date>           Mark the events as verified for those issued at specified time.\n"
                << " --guid=<identifier>     Mark the events as verified for those having specified identifier\n\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }
        else if(operation == "lockbookmark")
        {
            out << "\n" << progname << " usage:\n\n"
                << "Lock specified bookmark and prevent it from getting dropped during coalesce"
                << " operation:  "
                << progname << " --lockbookmark <options>\n\n"
                << "where options are:\n\n"
                << " --db=<database path>    path for retention database.(Required)OR \n"
                << " --vol=<target volume>   target volume whose retention database will be used\n"
                << "                         to lock.(Required)\n"
                << " --comment=<text>        Mark the bookmark as locked with the given comment.\n"
                << " --app=<ACM application> specify the application name\n"
                << "                         Either of event,time or guid is required along with\n"
                << "                         application name.\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n"
                << " --event=<value>         lock the matching bookmarks.\n"
                << " --time=<date>           lock the bookmarks issued at specified time.\n"
                << " --guid=<identifier>     lock the bookmarks with specified identifier\n\n"
				<< " Examples:\n\n\n"
                << " To lock bookmark(s) issued at specified time:\n"
                << " cdpcli --lockbookmark --vol=F: --time=2011/1/10 2:53:24:294:860:4\n\n"
                << " To lock bookmark(s) with specified guid:\n"
                << " cdpcli --lockbookmark --vol=F: --guid=471ffab9-0c20-4366-9d65-dce0a47aea61\n\n"
                << " To lock a specific bookmark:\n"
                << " cdpcli --lockbookmark --vol=F: --event=tag_01_09_2011_1031AM_64_5196_5196\n";

        }
        else if(operation == "unlockbookmark")
        {
            out << "\n" << progname << " usage:\n\n"
                << "Unlock the specified locked bookmark"
                << " operation:  "
                << progname << " --unlockbookmark <options>\n\n"
                << "where options are:\n\n"
                << " --db=<database path>    path for retention database.(Required)OR \n"
                << " --vol=<target volume>   target volume whose retention database will be used\n"
                << "                         to unlock the locked bookmark.(Required)\n"
                << " --comment=<text>        Mark the locked bookmark as unlocked with the given comment.\n"
                << " --app=<ACM application> specify the application name\n"
                << "                         Either of event,time or guid is required along with\n"
                << "                         application name.\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n"
                << " --event=<value>         unlock the matching locked bookmarks.\n"
                << " --time=<date>           unlock the locked bookmarks issued at specified time.\n"
                << " --guid=<identifier>     unlock the locked bookmarks with specified identifier\n\n"
                << " Examples:\n\n\n"
                << " To unlock the locked bookmark(s) issued at specified time:\n"
                << " cdpcli --unlockbookmark --vol=F: --time=2011/1/10 2:53:24:294:860:4\n\n"
                << " To unlock bookmark(s) with specified guid:\n"
                << " cdpcli --unlockbookmark --vol=F: --guid=471ffab9-0c20-4366-9d65-dce0a47aea61\n\n"
                << " To unlock a specific bookmark:\n"
                << " cdpcli --unlockbookmark --vol=F: --event=tag_01_09_2011_1031AM_64_5196_5196\n";

        }
        else if(operation == "snapshot")
        {
            out << "To take a snapshot:  "
                << progname << " --snapshot <snapshotoptions>\n\n"
                << "where snapshotoptions are:\n\n"
                << "--snapshotpairs=<snapshot pair details>\n"
                << "    Snapshot pair details format:\n"
                << "    Source volume 1, target volume 1,mount point 1;\n"
                << "    Source volume 2, target volume 2, mount point 2; ...\n"
                << "    Where:\n"
                << "    Source Volume: Source Volume for snapshot.\n"
                << "    Target Volume: destination Volume for snapshot.\n"
                << "    Mount Point: Directory name to mount the destination volume.\n"
                << "                 Applicable for UNIX only. (Optional)\n\n"


                << "--dest=<destination volume>\n"
                << "    Destination volume for snapshot\n\n"

                << "--source=<source volume>\n"
                << "    Source Volume for snapshot\n\n"

                << "--mountpoint=<mount point>\n"
                << "    Directory name to mount the destination volume.\n"
                << "    Applicable for UNIX only. (Optional)\n"

                << "NOTE: snapshotpairs and <dest,source,mountpoint> combinations\n" 
                << "      are mutually exclusive.\n\n"

                << "--prescript=<script path>\n"
                << "    Script to be executed before starting the snapshot process.\n"
                << "    This script is executed for each snapshot pair before starting\n"
                << "    the snapshot if \'runonce\' is not specified. Otherwise, it is run\n"
                << "    once before the snapshot process starts.  Snapshot is done only\n"
                << "    if script returns exit code zero.(Optional)\n\n"

                << "--postscript =<script path>\n"
                << "    Script to be executed on completion of the snapshot process.\n"
                << "    This script is executed for each snapshot pair on completion of\n"
                << "    the snapshot if \'runonce\' is not specified. Otherwise, it is run\n"
                << "    once the whole snapshot process completes. (Optional)\n\n"

                << "--sourcevolume\n"
                << "    To take snapshot on source volume\n\n"

                << "--skipchecks\n"
                << "    Allow to perform snapshot operation even for a non target volume.\n"
                << "    with this option --source_raw_capacity is required.\n\n"

                << "--source_uservisible_capacity\n"
                << "    fs capacity of source volume in bytes\n\n"

                << "--source_raw_capacity\n"
                << "    raw capacity of source volume in bytes\n\n"

                << "--runonce\n"
                << "    Based on this option, prescripts and postscripts are executed\n"
                << "    either for each snapshot pair or only once for the whole process.\n\n"

                << " --force=<yes|no|ask>\n"
                << "     specify this option as 'yes' if you want to continue\n"
                << "     the operation even if replication services are running.\n"
                << "     specify 'no' if you want to terminate the operation\n"
                << "     if replication services are running. specify 'ask' if\n"
                << "     user confirmation is required before proceeding with\n"
                << "     the operation. The default value is 'ask'. (Optional)\n\n"

#ifdef SV_WINDOWS
				<< " --fsawarecopy=<yes|no>\n"
				<< "     Specify this option to overide the configuration parameter\n"
				<< "	 used for FS aware copy for NTFS volumes, where only the used\n"
				<< "	 blocks are read and copied to the destination volume. (Optional)\n\n"
#endif
				<< "\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }

        else if(operation == "recover")
        {
            out  << "To perform Recovery Snapshot:  "
                << progname << " --recover <recoveroptions>\n\n"
                << "where recoveroptions are:\n\n"
                << "--recoverypairs=<recovery pairs details>\n"
                << "    Recovery pair details format:\n"
                << "    Source volume 1, target volume 1,mount point 1, retention database 1;\n"
                << "    Source volume 2, target volume 2, mount point 2, retention database 2; ...\n"
                << "    Where:\n"
                << "    Source Volume: Source Volume for recovery.\n"
                << "    Target Volume: destination Volume for recovery.\n"
                << "    Mount Point: Directory name to mount the destination volume.\n"
                << "                 Applicable for UNIX only. (Optional)\n"
                << "    Retention Database: Path for retention database.\n"
                << "     Normally, this is not required and the settings are fetched\n"
                << "     from Central Management server. (Optional)\n\n"

                << "--dest=<destination volume>\n"
                << "    Destination volume for recovery\n\n"

                << "--source=<source volume>\n"
                << "    Source Volume for recovery\n\n"

                << "--mountpoint=<mount point>\n"
                << "    Directory name to mount the destination volume.\n"
                << "    Applicable for UNIX only. (Optional)\n"

                << "--db=<database path>\n"
                << "    Path for retention database.\n"
                << "    Normally, this is not required and the settings are fetched\n"
                << "    from Central Management server. (Optional)\n\n"

                << "NOTE: recoverypairs and dest,source,mountpoint,db combinations\n" 
                << "      are mutually exclusive.\n\n"

                << "--recentcrashconsistentpoint\n"
                << "    Perform Recovery operation on the specified volumes to the\n"
                << "    last common crash consistent point in the specified time range.\n"
                << "    If the time range is not specified, the most recent common crash\n"
                << "    consistent point is chosen.\n\n"

                << "--recentfsconsistentpoint\n"
                << "    Perform Recovery operation on the specified volumes to the\n"
                << "    last common file system consistent point in the specified time range.\n"
                << "    If the time range is not specified, the most recent common file system\n"
                << "    consistent point is chosen.\n\n"

                << "--recentappconsistentpoint\n"
                << "    Perform Recovery operation on the specified volumes to the\n"
                << "    last common application consistent point in the specified time range.\n"
                << "    If the time range is not specified, the most recent common application\n"
                << "    consistent point is chosen.\n\n"

                << "--timerange=<start time, end time>\n"
                << "    This option is used to specify search interval to look for\n"
                << "    the recovery point. This option is used in combination with\n"
                << "    recentcrashconsistentpoint, recentfstconsistentpoint and\n" 
                << "    recentappconsistentpoint.\n"
                << "	The range is exclusive of the start and end time in the range\n" 
                << "	i.e., > start time and < end time\n\n"

                << "--at=<Time>\n"
                << "    Perform Recovery operation on the volumes to the specified timestamp.\n"
                << "    The specified time should be within the recovery time range for all\n"
                << "    the selected volumes.\n\n"

                << "--app=<application name>\n"
                << "    This option is used to restrict searching of events to the specified\n"
                << "    application.\n\n"

                << "--event=<value>\n"
                << "	Recover all the volumes to the specified event.\n"
                << "    The specified event should be available for all the specified volumes.\n\n"

                << "--picklatest\n"
                << "    Recover to the latest event among the events with same name.\n"
                << "    This option is only valid if the option --event is specified.\n\n"

                << "--pickoldest\n"
                << "    Recover to the oldest event among the events with same name.\n"
                << "    This option is only valid if the option --event is specified.\n\n"

                << "--eventnum=<num>\n"
                << "    Recover to the specified event number.\n"
                << "    The counting starts with value 1 starting from latest event to oldest one.\n"
                << "    Note: This option is not available if recoverypairs option is specified.\n\n"

                << "--aftertime=<Time>\n"
                << "    Recover all the volumes to first common consistency event\n"
                << "    occurring after the specified time.\n"
                << "     Time format: yr/mm/dd hr:min:sec:millisec:usec:nanosec\n\n"

                << "--beforetime=<Time>\n"
                << "    Recover all the volumes to a common latest application\n"
                << "    consistency event occurring before the specified time.\n"
                << "    Time format: yr/mm/dd hr:min:sec:millisec:usec:nanosec\n\n"

                << "--prescript=<script path>\n"
                << "    Script to be executed before starting the recovery process.\n"
                << "    This script is executed for each recovery pair before starting\n"
                << "    the recovery if \'runonce\' is not specified. Otherwise, it is run\n"
                << "    once before the recovery process starts.  Recovery is done only\n"
                << "    if script returns exit code zero.(Optional)\n\n"

                << "--postscript =<script path>\n"
                << "    Script to be executed on completion of the recovery process.\n"
                << "    This script is executed for each recovery pair on completion of\n"
                << "    the recovery if \'runonce\' is not specified. Otherwise, it is run\n"
                << "    once the whole recovery process completes. (Optional)\n\n"

                << "--runonce\n"
                << "    Based on this option, prescripts and postscripts are executed\n"
                << "    either for each recovery pair or only once for the whole process.\n\n"

                << "--skipchecks\n"
                << "    Allow to perform recovery operation even for a non target volume.\n"
                << "    with this option --source_raw_capacity is required.\n\n"

                << "--source_uservisible_capacity\n"
                << "    fs capacity of source volume in bytes\n\n"

                << "--source_raw_capacity\n"
                << "    raw capacity of source volume in bytes\n\n"

				<< "--skip_replication_pause\n" 
				<< "    Recovery operation during resync requires replication to be paused\n"
				<< "    this requires CS to be reachable.If CS is not avaialable, a workaround\n"
				<< "    could be to stop the vx services / ensure that the dataprotection is not\n"
				<< "    running for the pair and pass skip_replication_pause option to cdpcli.\n"
				<< "    When skip_replication_pause option is specified, cdpcli tries to perform\n"
				<< "    recovery operations without sending pause request to CS.\n\n"

                << " --force=<yes|no|ask>\n"
                << "     specify this option as 'yes' if you want to continue\n"
                << "     the operation even if replication services are running.\n"
                << "     specify 'no' if you want to terminate the operation\n"
                << "     if replication services are running. specify 'ask' if\n"
                << "     user confirmation is required before proceeding with\n"
                << "     the operation. The default value is 'ask'. (Optional)\n\n"

#ifdef SV_WINDOWS
				<< " --fsawarecopy=<yes|no>\n"
                << "     Specify this option to overide the configuration parameter\n"
                << "	 used for FS aware copy for NTFS volumes, where only the used\n"
                << "	 blocks are read and copied to the destination volume. (Optional)\n\n"
#endif



				<< "\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }

        else if(operation == "rollback")
        {
            out  << "To perform Rollback:  "
                << progname << " --rollback <rollbackoptions>\n\n"
                << "where rollbackoptions are:\n\n"
                << "--rollbackpairs=<rollback pairs details>\n"
                << "    Rollback pair details format:\n"
                << "    target volume 1,mount point 1, retention database 1;\n"
                << "    target volume 2, mount point 2, retention database 2; ...\n"
                << "    Where:\n"
                << "    Target Volume: destination Volume for rollback.\n"
                << "    Mount Point: Directory name to mount the destination volume.\n"
                << "                 Applicable for UNIX only. (Optional)\n"
                << "    Retention Database: Path for retention database.\n"
                << "     Normally, this is not required and the settings are fetched\n"
                << "     from Central Management server. (Optional)\n\n"

                << "--rollbackpairs=all\n"
                << "    use this option if you want to rollback all the target volumes.\n"
                << "    Note: the volumes may have to be manually mounted after the rollback\n"
                << "    completes.\n\n"

                << "--dest=<destination volume>\n"
                << "    Destination volume for rollback\n\n"

                << "--mountpoint=<mount point>\n"
                << "    Directory name to mount the destination volume.\n"
                << "    Applicable for UNIX only. (Optional)\n"

                << "--db=<database path>\n"
                << "    Path for retention database.\n"
                << "    Normally, this is not required and the settings are fetched\n"
                << "    from Central Management server. This is required only when\n"
                << "    communication with central management server is unavailable. (Optional)\n\n"

                << "NOTE: rollbackpairs and dest,mountpoint,db combinations\n" 
                << "      are mutually exclusive.\n\n"

                << "--recentcrashconsistentpoint\n"
                << "    Perform rollback operation on the specified volumes to the\n"
                << "    last common crash consistent point in the specified time range.\n"
                << "    If the time range is not specified, the most recent common crash\n"
                << "    consistent point is chosen.\n\n"

                << "--recentfsconsistentpoint\n"
                << "    Perform rollback operation on the specified volumes to the\n"
                << "    last common file system consistent point in the specified time range.\n"
                << "    If the time range is not specified, the most recent common file system\n"
                << "    consistent point is chosen.\n\n"

                << "--recentappconsistentpoint\n"
                << "    Perform rollback operation on the specified volumes to the\n"
                << "    last common application consistent point in the specified time range.\n"
                << "    If the time range is not specified, the most recent common application\n"
                << "    consistent point is chosen.\n\n"

                << "--timerange=<start time, end time>\n"
                << "    This option is used to specify search interval to look for\n"
                << "    the rollback point. This option is used in combination with\n"
                << "    recentcrashconsistentpoint, recentfstconsistentpoint and\n" 
                << "    recentappconsistentpoint.\n"
                << "	The range is exclusive of the start and end time in the range\n" 
                << "	i.e., > start time and < end time\n\n"

                << "--at=<Time>\n"
                << "    Perform rollback operation on the volumes to the specified timestamp.\n"
                << "    The specified time should be within the recovery time range for all\n"
                << "    the selected volumes.\n\n"

                << "--app=<application name>\n"
                << "    This option is used to restrict searching of events to the specified\n"
                << "    application.\n\n"

                << "--event=<value>\n"
                << "    rollback all the volumes to the specified event.\n"
                << "    The specified event should be available for all the specified volumes.\n\n"

                << "--picklatest\n"
                << "    Rollback to the latest event among the events with same name.\n"
                << "    This option is only valid if the option --event is specified.\n\n"

                << "--pickoldest\n"
                << "    Rollback to the oldest event among the events with same name.\n"
                << "    This option is only valid if the option --event is specified.\n\n"

                << "--eventnum=<num>\n"
                << "    rollback to the specified event number.\n"
                << "    The counting starts with value 1 starting from latest event to oldest one.\n"
                << "    Note: This option is not available if recoverypairs option is specified.\n\n"

                << "--aftertime=<Time>\n"
                << "    rollback all the volumes to first common consistency event\n"
                << "    occurring after the specified time.\n"
                << "     Time format: yr/mm/dd hr:min:sec:millisec:usec:nanosec\n\n"

                << "--beforetime=<Time>\n"
                << "    rollback all the volumes to a common latest application\n"
                << "    consistency event occurring before the specified time.\n"
                << "    Time format: yr/mm/dd hr:min:sec:millisec:usec:nanosec\n\n"

                << "--prescript=<script path>\n"
                << "    Script to be executed before starting the rollback process.\n"
                << "    This script is executed for each rollback pair before starting\n"
                << "    the rollback if \'runonce\' is not specified. Otherwise, it is run\n"
                << "    once before the rollback process starts.  Rollback is done only\n"
                << "    if script returns exit code zero.(Optional)\n\n"

                << "--postscript =<script path>\n"
                << "    Script to be executed on completion of the rollback process.\n"
                << "    This script is executed for each rollback pair on completion of\n"
                << "    the rollback if \'runonce\' is not specified. Otherwise, it is run\n"
                << "    once the whole rollback process completes. (Optional)\n\n"

                <<"--sourcevolumerollback\n"
                <<"		This option is only incase if you need rollback on source volume.\n\n"

                << "--runonce\n"
                << "    Based on this option, prescripts and postscripts are executed\n"
                << "    either for each recovery pair or only once for the whole process.\n\n"

                << " --force=<yes|no|ask>\n"
                << "     specify this option as 'yes' if you want to continue\n"
                << "     the operation even if replication services are running.\n"
                << "     specify 'no' if you want to terminate the operation\n"
                << "     if replication services are running. specify 'ask' if\n"
                << "     user confirmation is required before proceeding with\n"
                << "     the operation. The default value is 'ask'. (Optional)\n\n"

                << "--skipchecks\n"
                << "    Allow to perform rollback operation even for a non target volume.\n\n"

                << "--source_uservisible_capacity\n"
                << "    fs capacity of source volume in bytes\n\n"

                << "--source_raw_capacity\n"
                << "    raw capacity of source volume in bytes\n\n"

				<< "--skip_replication_pause\n" 
				<< "    Rollback operation during resync requires replication to be paused\n"
				<< "    this requires CS to be reachable.If CS is not avaialable, a workaround\n"
				<< "    could be to stop the vx services / ensure that the dataprotection is not\n"
				<< "    running for the pair and pass skip_replication_pause option to cdpcli.\n"
				<< "    When skip_replication_pause option is specified, cdpcli tries to perform\n"
				<< "    recovery operations without sending pause request to CS.\n\n"

                << "--stopreplication=<yes|no>\n"
                << "     specify this option as 'yes' if you want to delete the\n"
                << "     replication pair from CX after rollback.\n"
                << "     specify 'no' if you want to retain the replication pair.\n"
                << "     Note: if 'no' is specified resync will be set for this pair.\n"
                << "     Default is yes. (Optional)\n\n"

                << "--deleteretentionlog=<yes|no>\n"
                << "     specify this option as 'yes' if you want to delete the\n"
                << "     retention logs for this pair.\n"
                << "     specify 'no' if you want to retain the retention logs.\n"
                << "     Default is no. (Optional)\n\n"
				
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }
        else if(operation == "vsnap")
        {
            out << "To perform point-in-time or virtual recovery snapshot:  \n\n"
                << progname << " --vsnap <vsnapoptions>\n\n"
                << "where vsnapoptions are:\n\n"
                << "--vsnappairs=<vsnap pairs details>\n"
                << "	Vsnap pair details format:\n"
                << "	Target volume 1, virtual volume 1, retention database 1, datalogpath 1;\n"
                << "	Target volume 2, virtual volume 2, retention database 2; datalogpath 2...\n"
                << "	Where:\n"
                << "	Target Volume: Replicated Target Volume.\n"
                << "	Virtual Volume: Unused drive letter or a mountpoint. Eg: Z: or /mnt/vsnap1 or E:\\vsnap1.\n"
                << "					Optional for UNIX only.\n"
                << "	Retention Database: Path for retention database.\n"
                << "	 Normally, this is not required and the settings are fetched\n"
                << "	 from Central Management server. (Optional)\n\n"
                << "	Datalogpath: Path to store private datalogs of read-write vsnap.(Optional)\n\n"

                << " --target=<replicated target volume>\n"
                << "          This option is mandatory for mount operation\n\n"
                << " --virtual=<unused drive letter or a mount point>. Eg: Z: \n"
                << "          This option is mandatory for mount and unmount operation\n\n"
                << " --db=<database path> for retention logs (Optional) \n\n"
                << " --force=<Y/N> For forced unmounting if CX is Unavilable(Optional) \n\n"

                << "NOTE: vsnappairs and target,virtual,db,datalogpath combinations\n" 
                << "      are mutually exclusive.\n\n"

                << "--recentcrashconsistentpoint\n"
                << "    Take Recovery Vsnap on the specified volumes to the\n"
                << "    last common crash consistent point in the specified time range.\n"
                << "    If the time range is not specified, the most recent common crash\n"
                << "    consistent point is chosen. This option is only applied with\n"
                << "    --vsnappairs option.\n\n"

                << "--recentfsconsistentpoint\n"
                << "    Take Recovery Vsnap on the specified volumes to the\n"
                << "    last common file system consistent point in the specified time range.\n"
                << "    If the time range is not specified, the most recent common file system\n"
                << "    consistent point is chosen. This option is only applied with\n"
                << "    --vsnappairs option.\n\n"

                << "--recentappconsistentpoint\n"
                << "    Take Recovery Vsnap on the specified volumes to the\n"
                << "    last common application consistent point in the specified time range.\n"
                << "    If the time range is not specified, the most recent common application\n"
                << "    consistent point is chosen. This option is only applied with\n"
                << "    --vsnappairs option.\n\n"

                << "--timerange=<start time, end time>\n"
                << "    This option is used to specify search interval to look for\n"
                << "    the recovery point. This option is used in combination with\n"
                << "    recentcrashconsistentpoint, recentfstconsistentpoint and\n" 
                << "    recentappconsistentpoint.\n"
                << "	The range is exclusive of the start and end time in the range\n" 
                << "	i.e., > start time and < end time\n\n"

                << "--at=<Time>\n"
                << "    Take Recovery Vsnap on the volumes to the specified timestamp.\n"
                << "    The specified time should be within the recovery time range for all\n"
                << "    the selected volumes.\n\n"

                << "--app=<application name>\n"
                << "    This option is used to restrict searching of events to the specified\n"
                << "    application.\n\n"

                << "--aftertime=<Time>\n"
                << "    Take Recovery Vsnap on  all the volumes to first common consistency event\n"
                << "    occurring after the specified time.\n"
                << "    Time format: yr/mm/dd hr:min:sec:millisec:usec:nanosec\n"
                << "    This option is only applied with --vsnappairs option.\n\n"

                << "--beforetime=<Time>\n"
                << "    Take Recovery Vsnap on all the volumes to a common latest application\n"
                << "    consistency event occurring before the specified time.\n"
                << "    Time format: yr/mm/dd hr:min:sec:millisec:usec:nanosec\n"
                << "    This option is only applied with --vsnappairs option.\n\n"

                << " --prescript=<script path>\n"
                << "    Script to be executed before starting the vsnap process.\n"
                << "    This script is executed for each vsnap pair before starting\n"
                << "    the vsnap if \'runonce\' is not specified. Otherwise, it is run\n"
                << "    once before the vsnap process starts.  Vsnap is taken only\n"
                << "    if script returns exit code zero.(Optional)\n\n"

                << " --postscript=<script path>\n"
                << "    Script to be executed on completion of the vsnap process.\n"
                << "    This script is executed for each vsnap pair on completion of\n"
                << "    the vsnap if \'runonce\' is not specified. Otherwise, it is run\n"
                << "    once the whole vsnap process completes. (Optional)\n\n"

                << "--runonce\n"
                << "    Based on this option, prescripts and postscripts are executed\n"
                << "    either for each vsnap pair or only once for the whole process.\n\n"

                << "--skipchecks\n"
                << "    Allow to perform virtual snapshot operation even for a non target volume.\n"
                << "    with this option --source_raw_capacity is required.\n\n"

                << "--source_uservisible_capacity\n"
                << "    fs capacity of source volume in bytes\n\n"

                << "--source_raw_capacity\n"
                << "    raw capacity of source volume in bytes\n\n"

				<< "--skip_replication_pause\n" 
				<< "    Vsnap operation during resync requires replication to be paused\n"
				<< "    this requires CS to be reachable.If CS is not avaialable, a workaround\n"
				<< "    could be to stop the vx services / ensure that the dataprotection is not\n"
				<< "    running for the pair and pass skip_replication_pause option to cdpcli.\n"
				<< "    When skip_replication_pause option is specified, cdpcli tries to perform\n"
				<< "    recovery operations without sending pause request to CS.\n\n"

                << " --time=<recovery time> (Optional)\n"
                << "          Recover to the specified time.\n"
                << "          Time format: yr/mm/dd hr:min:sec:millisec:usec:nanosec\n\n"

                << " --event=<event name> (Optional)\n"
                << "          Recover to the specified event name\n\n"
                << "--picklatest\n"
                << "    Recover to the latest event among the events with same name.\n"
                << "    This option is only valid if the option --event is specified.\n\n"
                << "--pickoldest\n"
                << "    Recover to the oldest event among the events with same name.\n"
                << "    This option is only valid if the option --event is specified.\n\n"

                << " --app=<app name> --eventnum=<event number>  (Optional)\n"
                << "          Recover to the nth consistency point of the specified app\n"
                << "          default value for eventnum is 1.\n\n"
                << " --app=<app name> --event=<event name>(Optional)\n"
                << "          Recover to the specified event name that matches the specified\n"
                << "          application\n\n"
                << " --flags=<ro> or <rw> or <rwt> or <nodelete>.(Optional)\n"
                << "          ro       - mount with read-only access \n"
                << "          rw       - mount with read-write access \n"
                << "          rwt      - mount with read-write with tracking enabled\n"
                << "          nodelete - Don't delete the private logs. This option\n"
                << "                     is used only in unmount operation.\n\n"
                << " --datalogpath=<dir>. \n"
                << "          Path to store private datalogs of read-write vsnap. This option\n"
                << "          is mandatory for rw vsnaps.\n\n"
                << " --vsnapid=<vsnap id>. \n"
                << "          A vsnap volume can be unmounted without deleting its private logs\n"
                << "          Later this vsnap id can be used to apply logs onto a physical\n"
                << "          or virtual volume\n\n"
                << " --op=<mount,unmount,remount,list,unmountall,applytracklog,deletelogs>\n"
                << "          mount		- To mount a Point-In-Time or Recovery vsnap volume\n"
                << "          unmount	- To unmount a vsnap volume\n"
                << "          remount	- To remount all the vsnaps which aren't already mounted\n"
                << "          list	<listoptions>(Optional)\n"
                << "                 	- To list the mounted vsnap volumes\n"
                << "		  where listoptions are:\n"	
                << "			--target=<target-volume>\n"
                << "				- Vsnaps taken on target-volume are listed\n"
                << "			--virtual=<name>\n"
                << "				- Current vsnap is listed\n"
                << "			--verbose\n"
                << "				- Detailed information is provided\n"

                << "          unmountall    - To unmount all mounted vsnap volumes in the system\n"
                << "          applytracklog - To apply the tracked changes of a virtual volume to \n"
                << "                          another volume(physical or writeable virtual volume\n"
                << "          deletelogs    - To delete vsnap logs of a previously unmounted vsnap\n"
                << "                          volume with --flags=nodelete option\n\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n"
                << " Note 1: To take point-in-time vsnap, do not specify \"time\" or \"event\" or\n"
                << "         \"eventnum\" or \"app\" options\n\n"
                << " Note 2: ro, rw, rwt options are used during mount operation. Default is \n"
                << "         read-only access. \"nodelete\" option can be passed using --flags\n"
                << "         option for unmount operation. It tells not to delete the private logs\n"
                << "         created for this vsnap during an unmount operation. Default behavior\n"
                << "         to delete the logs\n\n";
        }

        else if(operation == "virtualvolume")
        {
            std::string sparsefile,virtualvol,sparsevol;
#ifdef SV_WINDOWS
            sparsefile = "C:\\sparse1.dat";
            virtualvol = "C:\\virtualvolume1";
            sparsevol = "C:\\";
#else
            sparsefile = "/home/sparse1.dat";
            virtualvol = "/dev/volpack1";
            sparsevol = "/home/";
#endif
            out << "To create virtual volume as target drive:  \n\n"
                << progname << " --virtualvolume --op=<operation to perform> <options>\n\n"
                << "where operation can be:\n\n"
                << "  createsparsefile     : To create a sparse file\n"
#ifdef SV_WINDOWS
                << "  mount                : To mount a virtual volume\n"
                << "  enablecompression    : To enable compression  on sparse file and/or virtual volume\n"
                << "  disablecompression   : To disable compression on sparse file and/or virtual volume\n"
#else
                << "  createvolume         : To create a virtual volume\n"
#endif			
                << "  unmount              : To delete a virtual volume\n"
                << "  unmountall           : To delete all virtual volumes\n"
                << "  list                 : To list virtual volumes\n"
                << "  resize               : To resize an existing virtual volume\n\n"

                << "Options specific to operation are:\n\n"
                << "  To create a sparse file: \n\n" 
                << "    --filepath=<path of file>  : absolutepath (including file name) for creating the sparse file \n"
                << "    --size=<sizeof file>       : size of the sparse file in megabytes OR\n"
                << "    --sizeinbytes=<sizeof file>: size of the sparse file in bytes\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n"
#ifdef SV_WINDOWS
                << "    --compression=<yes/no>     : specifies whether to enable NTFS compression on sparse file\n"
                << "                                 This parameter is optional.Default compression behavior is\n" 
                << "                                 controlled by 'VirtualVolumeCompression' parameter in Vx\n"
                << "                                 configuration file\n"
                << "    --sparseattribute=<yes/no/yesifavailable> :                                           \n"
                << "                                 specifies whethere to enable NTFS sparse attribute       \n"
                << "                                 This parameter is optional. Default behavior is controlled\n"
                << "                                 by 'VolpackSparseAttribute' parameter in Vx configuration\n" 
                << "                                 file.\n"
                << "                                 possible values:\n"
                << "                                 yes: enable sparse attribute\n"
                << "                                 no: disable sparse attribute\n"
                << "                                 yesifavailable: enable if the underlying volume supports it\n\n"

                << "  Example: To create a sparse file of size 100mb with compression on under " << sparsevol << "\n"
                << "           cdpcli --virtualvolume --op=createsparsefile --filepath=" << sparsefile << "\n"
                << "           --size=100 --compression=yes\n\n\n"
                << "  To mount a virtual volume:\n\n"
                << "    --filepath=<path of file>    : absolute path (including file name) of the sparse file\n"
                << "    --drivename=<unused volume>  : a unused drive letter or mount point\n\n"
                << "  Example: To mount sparse file " << sparsefile << " as a virtual volume on " << virtualvol << "\n"
                << "           cdpcli --virtualvolume --op=mount --filepath=" << sparsefile << "\n"
                << "           --drivename=" << virtualvol << "\n\n\n"				
#else
                << "\n  Example: To create a sparse file of size 100mb under " << sparsevol << "\n"
                << "           cdpcli --virtualvolume --op=createsparsefile --filepath=" << sparsefile << "\n"
                << "           --size=100\n\n\n"
                << "  To create a virtual volume:\n\n"
                << "    --filepath=<path of file>    : absolute path (including file name) of the sparse file\n\n"
                << "  Example: To create virtual volume on sparse file " << sparsefile << "\n"
                << "           cdpcli --virtualvolume --op=createvolume --filepath=" << sparsefile << "\n\n\n"

#endif
                << "  To delete a virtual volume:\n\n"
                << "    --drivename=<virtual volume>  : name of the virtual volume to delete\n"
                << "    --deletesparsefile=<yes/no>   : if specified as 'no', sparse file will not be deleted\n"
                << "                                    if specified as 'yes', sparse file will be deleted\n"
                << "                                    This parameter is optional. default value is yes.\n\n"
                << "  Example: To delete virtual volume " << virtualvol << "\n"
                << "           cdpcli --virtualvolume --op=unmount --drivename=" << virtualvol << "\n\n\n"

                << "  To delete all virtual volumes:\n\n"
                << "    --deletesparsefile=<yes/no> : if specified as 'no', sparse file will not be deleted\n"
                << "                                  if specified as 'yes', sparse file will be deleted\n"
                << "                                  This parameter is optional. default value is yes.\n\n"
                << "  Example: cdpcli --virtualvolume --op=unmountall\n\n\n"

                << "  To list virtual volumes:\n\n"
                << "    --verbose : This parameter is optional. If specified, it will display\n"
                << "                sparse file, raw size of the virtual volumes.\n\n"
                << "  Example: cdpcli --virtualvolume --op=list\n\n\n"

                << "  To resize a virtual volume:\n\n"
                << "    --devicename=<virtual volume name> : name of the virtual volume to resize\n"
                << "    --size=<size in MB>                : specify the new capacity for the volume in MB OR\n"
                << "    --sizeinbytes=<size in bytes>      : specify the new capacity for the volume in bytes\n"
#ifdef SV_WINDOWS

                << "    --sparseattribute=<yes/no/yesifavailable> :                                           \n"
                << "                                 specifies whethere to enable NTFS sparse attribute       \n"
                << "                                 This parameter is optional. Default behavior is controlled\n"
                << "                                 by 'VolpackSparseAttribute' parameter in Vx configuration\n" 
                << "                                 file.\n"
                << "                                 possible values:\n"
                << "                                 yes: enable sparse attribute\n"
                << "                                 no: disable sparse attribute\n"
                << "                                 yesifavailable: enable if the underlying volume supports it\n"
#endif

                << "    \nNote: The resize operation will unmount the virtual volume making all the\n"
                << "          open handles to the volume invalid. It will resize the sparse file\n"
                << "          and remount the virtual volume. This operation does not resize the\n"
                << "          filesystem capacity of the volume.\n\n"
                << "  Example:  To resize the volume " << virtualvol << " to new size 200mb\n" 
                << "           cdpcli --virtualvolume --op=resize --devicename=" << virtualvol << "\n"
                << "            --size=200\n\n\n";
#ifdef SV_WINDOWS				
            out << "  To enable compression on sparse file and/or virtual volume:\n\n"
                << "    --filepath=<sparse file path> : absolute path (including file name) of the sparse file\n"
                << "    --devicename=<virtual volume> : name of the virtual volume\n\n"
                << "    Note: Either of 'devicename' or 'filepath option may be specified.\n"
                << "          Specifying devicename will enable compression on both the virtual\n"
                << "          volume and the underlying sparse file.Compression on virtual volumes\n"
                << "          can be enabled only on those formatted with NTFS filesystem.\n\n"
                << "  Example: cdpcli --virtualvolume --op=enablecompression --devicename=" << virtualvol << "\n\n\n"
                << "  To disable compression on sparse file and/or virtual volume:\n\n"
                << "    --filepath=<sparse file path> : absolute path (including file name) of the sparse file\n"
                << "    --devicename=<virtual volume> : name of the virtual volume\n\n"
                << "    Note: Either of 'devicename' or 'filepath option may be specified.\n"
                << "          Specifying devicename will disable compression on both the virtual\n"
                << "          volume and the underlying sparse file.Compression on virtual volumes\n"
                << "          can be disabled only on those formatted with NTFS filesystem.\n\n"
                << "  Example: cdpcli --virtualvolume --op=disablecompression --devicename=" << virtualvol << "\n\n";
#endif
        }

        else if(operation == "hide")
        {

            out << "To hide a volume:  "
                << progname << " --hide <hideoptions>\n\n"
                << "where hideoptions are:\n\n"
                << "   <volumename>               volume to hide.(Required)\n"				
                << "   --sourcevolume             this option to hide source volume\n"
                << " --force=<yes|no>             specify this option as 'yes' if you want to\n"
                << "                              continue the operation even if central\n"
                << "                              management server is down.specify 'no'if you want\n"
                << "                              to terminate the operation if central management\n"
                << "                              server is down.(Optional)\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }
        else if(operation == "unhide_ro")
        {
            out  << "To unhide a volume in read-only mode: "
                << progname << " --unhide_ro <unhideoptions>\n\n"
                << "where unhideoptions are:\n\n"
                << "   <volumename>               volume to unhide in read only mode.(Required)\n"				
                << " --force=<yes|no>             specify this option as 'yes' if you want to\n"
                << "                              continue the operation even if central\n"
                << "                              management server is down.specify 'no'if you want\n"
                << "                              to terminate the operation if central management\n"
                << "                              server is down.(Optional)\n"
                << " --mountpoint=<directory name>directory name to mount the volume. Applicable\n"
                << "                              for UNIX only. (Required)\n"
                << " --filesystem=<file system name>name of file system to mount. Applicable\n"
                << "                              for UNIX only. (Optional)\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }
        else if(operation == "unhide_rw")
        {
            out << "unhide a volume in read-write mode: "
                << progname << " --unhide_rw <unhideoptions>\n\n"
                << "where unhideoptions are:\n\n"
                << "   <volumename>               volume to unhide in read only mode.(Required)\n"				
                << " --force=<yes|no>             specify this option as 'yes' if you want to\n"
                << "                              continue the operation even if central\n"
                << "                              management server is down.specify 'no'if you want\n"
                << "                              to terminate the operation if central management\n"
                << "                              server is down.(Optional)\n"
                << " --mountpoint=<directory name>directory name to mount the volume. Applicable\n"
                << "                              for UNIX only. (Required)\n"
                << " --filesystem=<file system name>name of file system to mount. Applicable\n"
                << "                              for UNIX only. (Optional)\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }
        else if(operation == "iopattern")
        {
            out << "print the io distribution: "
                << progname << " --iopattern  <options>\n\n"               
                << "where options are:\n\n"
                << " --vol=<target volume>   target volume.(Required)\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }
        else if(operation == "fixerrors")
        {
            out << "To fix the database issues: "
                << progname << " --fixerrors <fixerrorsoptions>\n\n"
                << "where fixerrorsoptions are:\n\n"
                << " --fix=<time,staleinodes,physicalsize>   Type of fix to be done on database.\n"
                << " --db=<database path>         Path to retention database.\n"
                << " --vol=<target volume name>   Name of the target volume of the replication pair\n"
                << "                              for which the DB path is speified.\n"
                << " --tempdir=<temp folder>      Path to temporary folder. This folder should have\n"
                << "                              atleast 320MB + size of the DB file.\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }
        else if(operation == "displaystatistics")
        {
            out << "\n" << progname << " usage:\n\n"
                << "To list replication statistics for a target volume: "
                << progname << " --displaystatistics <displaystatisticoptions>\n\n"
                << "where displaystatisticoptions are:\n\n"
                << " --vol=<target volume name>		Name of the target volume for displaying the statistics.\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }
        else if(operation == "getsparsepolicysummary")
        {
            out << "\n" << progname  << " usage:\n\n"
                << "To get the retention volume size from the sparse policy\n\n"
                << progname << " --getsparsepolicysummary\n"
                << "    --dbprofile=<database path>\n"
                << "    --standardpolicy (To take the default policy)\n"
                << "    --verbose (To view the verbose information)\n"
                << "    --profileinterval=<input in days/weeks/months> The interval which has to be taken from the retention for computation\n\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";

        }
        else if(operation == "gencdpdata")
        {
            out << "\n" << progname << " usage:\n\n"
                << "To generate diffs with overlapping changes(for testing purpose only): "
                << progname << " --gencdpdata (interactive)\n\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }
        //#15949 : Adding showreplicationpairs and showsnapshotrequests options to cdpcli
        else if(operation == "showreplicationpairs")
        {
            out << "\n" << progname << " usage:\n\n"
                << "To show the replication pair information from Cache: "
                << progname << " --showreplicationpairs  OR\n\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n"
                << "To get the replication pair information from a give config file: "
                << progname << " --showreplicationpairs  --file=<file name with path for the settings data>OR\n\n"
                << "To get the replication pair information from CX: "
                << progname << " --showreplicationpairs --cxsettings  OR\n\n"
                << "For Custom settings: "
                << progname << " --showreplicationpairs --custom --ip=<cx-ip> --port=<cx-port> --hostid=<host id> \n";
        }
        else if(operation == "showsnapshotrequests")
        {
            out << "\n" << progname << " usage:\n\n"
                << "To show the replication pair information: "
                << progname << " --showsnapshotrequests OR\n\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n"
                << "For Custom settings: "
                << progname << " --showsnapshotrequests --custom --ip=<cx-ip> --port=<cx-port> --hostid=<host id> \n";
        }
        else if(operation == "listtargetvolumes")
        {
            out << "\n" << progname << " usage:\n\n"
                << "To display list of target volumes: "
                << progname << " --listtargetvolumes\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }
        else if(operation == "listprotectedvolumes")
        {
            out << "\n" << progname << " usage:\n\n"
                << "To display list of protected volumes: "
                << progname << " --listprotectedvolumes\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }
        else if(operation == "displaysavings")
        {
            out << "\n" << progname << " usage:\n\n"
                << "To display space savings due to compression,sparse retention and thin provisioning : "
                << progname << " --displaysavings [--volumes=<comma separated target volume names>]\n" 
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n"
                << " Example: cdpcli --displaysavings --volumes=E,F \n\n";
        }
#ifdef SV_WINDOWS
        else if(operation == "reattachdismountedvolumes")
        {
            out << "\n" << progname << " usage:\n\n"
                << "To reattach the missing drive letters to their correspondig guids: "
                << progname << " --reattachdismountedvolumes <options>\n"
                << "where options are:\n\n"
                << " --noprompt   To disable the prompt while assigning the drive letter to the guid(Optional)\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }
#endif //End of SV_WINDOWS
        else if(operation == "volumecompare")
        {
            out << "\n" << progname << " usage:\n\n"
                << "Debugging utility to compare two volumes: "
                << progname << " --volumecompare <options>\n"
                << "where options are:\n\n"
                <<" --vol1=<volume 1 name> \n"
                <<" --vol2=<volume 2>\n"
                <<" --chunksize=<size in bytes to be read/compared>\n"
                <<" --volumesize=<total bytes of the volume to be compared>\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }
        else if(operation == "readvolume")
        {
            out << "\n" << progname << " usage:\n\n"
                << "Debugging utility to read data from given volume: "
                << progname << " --readvolume <options>\n"
                << "where options are:\n\n"
                <<" --vol=<volume name to be read> \n"
                <<" --filename=<file name to which volume data should be written>\n"
                <<" --chunksize=<size in bytes to be read>\n"
                <<" --startoffset=<volume offset from which data should be read>\n"
                <<" --endoffset=<volume offset until which data should be read>\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
        }
		else if(operation == "deletestalefiles")
		{
            out << "\n" << progname << " usage:\n\n"
                << "To delete stale files in the retention: " 
                << progname << " --deletestalefiles <options>\n\n"
                << "where options are:\n\n"
                << " --db=<database path>    path for retention database.(Required) OR\n"
                << " --vol=<target volume>   target volume whose retention database will\n"
                << "                         be validated.(Required)\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";

		}
		else if(operation == "deleteunusablepoints")
		{
            out << "\n" << progname << " usage:\n\n"
                << "To delete unusbale points in the retention retention database: " 
                << progname << " --deleteunusablepoints <options>\n\n"
                << "where options are:\n\n"
                << " --db=<database path>    path for retention database.(Required) OR\n"
                << " --vol=<target volume>   target volume whose retention database will\n"
                << "                         be validated.(Required)\n"
				<< " --loglevel=<value from 0 to 7>    Loglevel for cdpcli opration.(optional)\n"
				<< " --logpath=<filepath where to log> To change the default logfilepath.(optional)\n";
		}
        
        else
        {
            out << "usage error: please use " << progname << " --h  [operation] "  << "to view usage information\n"
                << "\nwhere operation can be one of the following:\n"
                << " validate                   - To validate retention database\n"
                << " showsummary                - To view summary information\n"
                << " listcommonpoint            - To list common recovery point\n"
                << " listevents                 - To list consistency events\n"
                << " verifiedtag                - To mark events as verified\n"
                << " snapshot                   - To take a snapshot\n"
                << " recover                    - To perform recovery snapshot\n"
                << " rollback                   - To perform rollback\n"
                << " hide                       - To hide a volume\n"
                << " virtualvolume              - To mount virtual Volume\n"
                << " unhide_ro                  - To unhide a volume in read-only mode\n"
                << " unhide_rw                  - To unhide a volume in read-write mode\n"
                << " fixerrors                  - To fix the database issues\n"
                << " displaystatistics          - To view replication statistics\n"
                << " getsparsepolicysummary     - To get the retention volume size from the sparse policy\n"
                << " lockbookmark               - To lock specified bookmark and prevent it from getting dropped during coalesce operation\n"
                << " unlockbookmark	            - To unlock the specified locked bookmark\n"
                //#15949 : Adding showreplicationpairs and showsnapshotrequests options to cdpcli
                << " showreplicationpairs       - To list the replication pair information\n"
                << " showsnapshotrequests       - To display the snap shot requests\n"
                << " listtargetvolumes          - To display a list of target volumes.\n"
                << " listprotectedvolumes       - To display a list of protected volumes.\n";
#ifdef SV_WINDOWS
            out << " reattachdismountedvolumes  - To reattach the missing drive letters to their correspondig guids.\n";
#endif //End of SV_WINDOWS
            out << " volumecompare              - Debugging utility to compare two volumes.\n"
                << " readvolume                 - Debugging utility to read data from volume.\n"
				<< " deletestalefiles           - To delete stale files in the retention folder for given volume.\n"
				<< " deleteunusablepoints       - To delete unusable points from the retention db for a given volume.\n";
                

        }

        if(error)
        {
            DebugPrintf(SV_LOG_ERROR, "%s%s", "usage error:\n", out.str().c_str());
        }
        else
            DebugPrintf(SV_LOG_INFO, "%s\n", out.str().c_str());
    }

}

/*
* FUNCTION NAME :  CDPCli::run
*
* DESCRIPTION : parse the command line and invoke appropriate routines
*
*
* INPUT PARAMETERS : none
*                    
*
* OUTPUT PARAMETERS :  none
*
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::run()
{
    bool rv = true;

    bool m_validate  = false; 
    bool m_copylogs = false; //TODO:not implemented

    bool m_showsummary = false;
    bool m_listevents = false;
    bool m_snapshot = false;
    bool m_recover = false;
    bool m_rollback = false;
    bool m_vsnap = false;
    bool m_virtualvolume = false;
    bool m_hide = false;
    bool m_unhidero = false;
    bool m_unhiderw = false;
    bool m_listcommonpt = false;
    bool m_iopattern = false;

    bool m_fixerrors = false;
    bool m_displaystatistics = false;
    bool m_moveRetention = false;
    bool m_sparsepolicysummary = false;
    bool m_genCdpData = false;
    bool m_unhideall = false;
    bool m_verifiedtag = false;

    bool m_displaysavings = false;

    bool m_lockbookmark = false;
    bool m_unlockbookmark = false;
    //#15949 : Adding showreplicationpairs and showsnapshotrequests options to cdpcli
    bool m_showreplicationpairs = false;
    bool m_showsnapshotrequests = false;
    bool m_listtargetvolumes = false;
    bool m_listprotectedvolumes = false;
    bool m_registerhost = false;
    //16211 - Adding cdpcli option for reattached the guid to the drive letter
#ifdef SV_WINDOWS
    bool m_reattachdismountedvolumes = false;
#endif //End of SV_WINDOWS
    bool m_displaymetadata = false;
    bool m_volumecompare = false;
    bool m_readvolume = false;
	bool m_delete_stalefiles = false;
	bool m_delete_unusable_points = false;
	bool m_registermt = false;
	bool bProcessCsJobs = false;


    if(!init())
        return false;

    do
    {
        DebugPrintf(SV_LOG_INFO, "%s", "\n");

        if (argc < 2)
        {
            usage(argv[0]);
            rv = false;
            break;
        }

        int i = 0;
        std::string cmdline;
        while(i<argc)
        {
                cmdline += argv[i];
                cmdline += " ";
                i++;
        }

        DebugPrintf(SV_LOG_DEBUG,"Command line passed : %s\n",cmdline.c_str());


        if (0 == stricmp(argv[1], "--h")) 
        {
            help(argc, argv);
            rv = true;
            break;
        }

		validoptions.push_back("loglevel");
		validoptions.push_back("logpath");

        if (0 == stricmp(argv[1], "--validate")) 
        {
            m_validate = true;
            m_operation = "validate";
            validoptions.push_back("db");
            validoptions.push_back("vol");
            singleoptions.push_back("no_vxservice_check");  
        }
        else if (0 == stricmp(argv[1], "--iopattern")) 
        {
            m_iopattern = true;
            m_operation = "iopattern";
            validoptions.push_back("vol");
        }
        else if (0 == stricmp(argv[1], "--unhideall")) 
        {
            m_unhideall = true;
            m_operation = "unhideall";
        }
        else if (0 == stricmp(argv[1], "--showsummary")) 
        {
            m_showsummary = true;
            m_operation = "showsummary";
            validoptions.push_back("db");
            validoptions.push_back("vol");
            singleoptions.push_back("verbose");  
			singleoptions.push_back("spaceusage");            
        }
        else if (0 == stricmp(argv[1], "--verifiedtag")) 
        {
            m_verifiedtag = true;
            m_operation = "verifiedtag";
            validoptions.push_back("db");
            validoptions.push_back("vol");
            validoptions.push_back("app");
            validoptions.push_back("event");
            validoptions.push_back("time");
            validoptions.push_back("guid"); 
            validoptions.push_back("comment");
        }
        else if (0 == stricmp(argv[1], "--lockbookmark")) 
        {
            m_lockbookmark = true;
            m_operation = "lockbookmark";
            validoptions.push_back("db");
            validoptions.push_back("vol");
            validoptions.push_back("app");
            validoptions.push_back("event");
            validoptions.push_back("time");
            validoptions.push_back("guid"); 
            validoptions.push_back("comment");
        }
        else if (0 == stricmp(argv[1], "--unlockbookmark")) 
        {
            m_unlockbookmark = true;
            m_operation = "unlockbookmark";
            validoptions.push_back("db");
            validoptions.push_back("vol");
            validoptions.push_back("app");
            validoptions.push_back("event");
            validoptions.push_back("time");
            validoptions.push_back("guid"); 
            validoptions.push_back("comment");
        }
        else if (0 == stricmp(argv[1], "--getsparsepolicysummary")) 
        {
            m_sparsepolicysummary = true;
            m_operation = "getsparsepolicysummary";
            validoptions.push_back("dbprofile");
            singleoptions.push_back("standardpolicy");
            singleoptions.push_back("verbose");
            validoptions.push_back("profileinterval");
        }
        else if (0 == stricmp(argv[1], "--listcommonpoint")) 
        {
            m_listcommonpt = true;
            m_operation = "listcommonpt";              
            validoptions.push_back("app");
            validoptions.push_back("aftertime");
            validoptions.push_back("beforetime");
            validoptions.push_back("volumes");
        }
        else if (0 == stricmp(argv[1], "--listevents")) 
        {
            m_listevents = true;
            m_operation = "listevents";
            validoptions.push_back("db");
            validoptions.push_back("vol");
            validoptions.push_back("app");
            validoptions.push_back("event");
            validoptions.push_back("at");
            validoptions.push_back("aftertime");
            validoptions.push_back("beforetime");
            validoptions.push_back("verified");
            singleoptions.push_back("onlylockedbookmarks");
            singleoptions.push_back("namevalueformat");
            validoptions.push_back("sort");
        }
        else if (0 == stricmp(argv[1], "--copy"))  //TODO: Not yet implemented
        {
            m_copylogs = true;
            m_operation = "copylogs";
            validoptions.push_back("db");
            validoptions.push_back("vol");
            validoptions.push_back("dest");
            validoptions.push_back("starttime");
            validoptions.push_back("endtime");
        }
        else if (0 == stricmp(argv[1], "--snapshot")) 
        {
            m_snapshot = true;
            m_operation = "snapshot";
            validoptions.push_back("snapshotpairs");
            validoptions.push_back("dest");
            validoptions.push_back("source");
            validoptions.push_back("mountpoint");
            validoptions.push_back("prescript");
            validoptions.push_back("postscript");
            validoptions.push_back("force");
            singleoptions.push_back("runonce");
            validoptions.push_back("source_raw_capacity");
            validoptions.push_back("source_uservisible_capacity");
            singleoptions.push_back("skipchecks");
            singleoptions.push_back("sourcevolume");
            singleoptions.push_back("nodirectio");
#ifdef SV_WINDOWS
            validoptions.push_back("fsawarecopy");
#endif
        }
        else if (0 == stricmp(argv[1], "--recover")) 
        {
            m_recover = true;
            m_operation = "recover";
            validoptions.push_back("dest");
            validoptions.push_back("source");
            validoptions.push_back("mountpoint");
            validoptions.push_back("prescript");
            validoptions.push_back("postscript");
            validoptions.push_back("force");
            validoptions.push_back("db");
            validoptions.push_back("at");
            validoptions.push_back("aftertime");
            validoptions.push_back("beforetime");
            validoptions.push_back("eventnum");
            validoptions.push_back("app");
            validoptions.push_back("event");
            validoptions.push_back("recoverypairs");
            singleoptions.push_back("recentcrashconsistentpoint");
            singleoptions.push_back("recentfsconsistentpoint");
            singleoptions.push_back("recentappconsistentpoint");
            validoptions.push_back("timerange");
            validoptions.push_back("source_raw_capacity");
            validoptions.push_back("source_uservisible_capacity");
            singleoptions.push_back("skipchecks");
            singleoptions.push_back("runonce");
            singleoptions.push_back("picklatest");
            singleoptions.push_back("pickoldest");
            singleoptions.push_back("nodirectio");
			singleoptions.push_back("skip_replication_pause");
#ifdef SV_WINDOWS
            validoptions.push_back("fsawarecopy");
#endif
        }
        else if (0 == stricmp(argv[1], "--rollback")) 
        {
            m_rollback = true;
            m_operation = "rollback";
            validoptions.push_back("dest");
            validoptions.push_back("mountpoint");
            validoptions.push_back("prescript");
            validoptions.push_back("postscript");
            validoptions.push_back("force");
            validoptions.push_back("db");
            //validoptions.push_back("vol");
            validoptions.push_back("at");
            validoptions.push_back("aftertime");
            validoptions.push_back("beforetime");
            validoptions.push_back("eventnum");
            validoptions.push_back("app");
            validoptions.push_back("event");
            validoptions.push_back("rollbackpairs");
            validoptions.push_back("stopreplication");
            validoptions.push_back("deleteretentionlog");
            singleoptions.push_back("recentcrashconsistentpoint");
            singleoptions.push_back("recentfsconsistentpoint");
            singleoptions.push_back("recentappconsistentpoint");
            validoptions.push_back("timerange");
            singleoptions.push_back("runonce");
            singleoptions.push_back("sourcevolumerollback");
            validoptions.push_back("source_raw_capacity");
            validoptions.push_back("source_uservisible_capacity");
            singleoptions.push_back("skipchecks");
            singleoptions.push_back("picklatest");
            singleoptions.push_back("pickoldest");
            singleoptions.push_back("nodirectio");
			singleoptions.push_back("skip_replication_pause");
        }
        else if (0 == stricmp(argv[1], "--hide"))
        {
            m_operation = "hide";
            m_hide = true;
            validoptions.push_back("force");		
            singleoptions.push_back("sourcevolume");
        }
        else if (0 == stricmp(argv[1], "--unhide_ro"))
        {
            m_operation = "unhide_ro";
            m_unhidero = true;
            validoptions.push_back("mountpoint");
            validoptions.push_back("filesystem");
            validoptions.push_back("force");
        }
        else if (0 == stricmp(argv[1], "--unhide_rw"))
        {
            m_operation = "unhide_rw";
            m_unhiderw = true;
            validoptions.push_back("mountpoint");
            validoptions.push_back("filesystem");
            validoptions.push_back("force");
        }
        else if ( 0 == stricmp(argv[1], "--vsnap"))
        {
            m_operation = "vsnap";
            m_vsnap = true;
            validoptions.push_back("target");
            validoptions.push_back("virtual");
            validoptions.push_back("db");
            validoptions.push_back("time");
            validoptions.push_back("app");
            validoptions.push_back("event");
            validoptions.push_back("eventnum");
            validoptions.push_back("op");
            validoptions.push_back("vsnapid");
            validoptions.push_back("datalogpath");
            validoptions.push_back("flags");
            validoptions.push_back("prescript");
            validoptions.push_back("postscript");
            validoptions.push_back("force");  
            validoptions.push_back("vsnappairs");
            validoptions.push_back("at");
            validoptions.push_back("aftertime");
            validoptions.push_back("beforetime");
            validoptions.push_back("source_raw_capacity");
            validoptions.push_back("source_uservisible_capacity");
            singleoptions.push_back("recentcrashconsistentpoint");
            singleoptions.push_back("recentfsconsistentpoint");
            singleoptions.push_back("recentappconsistentpoint");
            validoptions.push_back("timerange");
            singleoptions.push_back("softremoval");
            singleoptions.push_back("bypassdriver");
            singleoptions.push_back("runonce");
            singleoptions.push_back("verbose");
            singleoptions.push_back("picklatest");
            singleoptions.push_back("pickoldest");
            singleoptions.push_back("upgradepersistentstore");
            singleoptions.push_back("skipchecks");
			singleoptions.push_back("skip_replication_pause");
        }

        else if ( 0 == stricmp(argv[1], "--virtualvolume"))
        {
            m_operation = "virtualvolume";
            m_virtualvolume = true;
            validoptions.push_back("op");
            validoptions.push_back("filepath");
            validoptions.push_back("drivename");
            validoptions.push_back("size");
            validoptions.push_back("devicename");
            validoptions.push_back("compression");
            // Bug# 3549
            // hidden cdpcli flag for retaining the sparse file and drscout entries
            singleoptions.push_back("softremoval");
            singleoptions.push_back("bypassdriver");
            validoptions.push_back("checkfortarget");
            validoptions.push_back("deletesparsefile");
            singleoptions.push_back("verbose");
            validoptions.push_back("sizeinbytes");
            validoptions.push_back("sparseattribute");
        }
        else if( 0 == stricmp(argv[1], "--fixerrors") )
        {
            m_operation = "fixerrors";
            m_fixerrors = true;
            validoptions.push_back("fix");
            validoptions.push_back("db");
            validoptions.push_back("vol");
            validoptions.push_back("tempdir");
        }
        else if( 0 == stricmp(argv[1], "--displaystatistics"))
        {
            m_operation = "displaystatistics";
            m_displaystatistics = true;
            validoptions.push_back("vol");
        }
        else if( 0 == stricmp(argv[1], "--moveretention"))
        {
            m_operation = "moveretention";
            m_moveRetention = true;
            validoptions.push_back("oldretentiondir");
            validoptions.push_back("newretentiondir");
            validoptions.push_back("vol");
        }
        else if( 0 == stricmp(argv[1], "--gencdpdata") )
        {
            m_operation = "gencdpdata";
            m_genCdpData = true;
        }
        else if( 0 == stricmp(argv[1],"--displaysavings"))
        {
            m_displaysavings = true;
            m_operation = "displaysavings";
            validoptions.push_back("volumes");
        }
        //#15949 : Adding showreplicationpairs and showsnapshotrequests options to cdpcli
        else if( 0 == stricmp(argv[1],"--showreplicationpairs"))
        {
            m_operation = "showreplicationpairs";
            m_showreplicationpairs = true;
            singleoptions.push_back("cxsettings");
            singleoptions.push_back("custom");
            validoptions.push_back("ip");
            validoptions.push_back("port");
            validoptions.push_back("hostid");
            validoptions.push_back("file");
        }
        else if( 0 == stricmp(argv[1],"--showsnapshotrequests"))
        {
            m_operation = "showsnapshotrequests";
            m_showsnapshotrequests = true;
            singleoptions.push_back("custom");
            validoptions.push_back("ip");
            validoptions.push_back("port");
            validoptions.push_back("hostid");
        }
        else if( 0 == stricmp(argv[1],"--listtargetvolumes"))
        {
            m_operation = "listtargetvolumes";
            m_listtargetvolumes = true;
        }
        else if( 0 == stricmp(argv[1],"--listprotectedvolumes"))
        {
            m_operation = "listprotectedvolumes";
            m_listprotectedvolumes = true;
        } else if ( 0 == stricmp(argv[1], "--registerhost"))
        {
            m_operation = "registerhost";
            m_registerhost = true;
        }
        //16211 - Option to reattach the guid to the corresponding drive letters.
#ifdef SV_WINDOWS
        else if(0 == stricmp(argv[1],"--reattachdismountedvolumes"))
        {
            m_reattachdismountedvolumes = true;
            m_operation = "reattachdismountedvolumes";
            singleoptions.push_back("noprompt");
        }
#endif //End of SV_WINDOWS
        else if(0 == stricmp(argv[1], "--displaymetadata"))
        {
            m_displaymetadata = true;
            m_operation = "displaymetadata";
            validoptions.push_back("filename");
        }
        else if(0 == stricmp(argv[1], "--volumecompare"))
        {
            m_volumecompare = true;
            m_operation = "volumecompare";
            validoptions.push_back("vol1");
            validoptions.push_back("vol2");
            validoptions.push_back("chunksize");
            validoptions.push_back("volumesize");
        }
        else if(0 == stricmp(argv[1], "--readvolume"))
        {
            m_readvolume = true;
            m_operation = "readvolume";
            validoptions.push_back("vol");
            validoptions.push_back("filename");
            validoptions.push_back("chunksize");
            validoptions.push_back("startoffset");
            validoptions.push_back("endoffset");
        }
		else if(0 == stricmp(argv[1], "--deletestalefiles"))
		{
			m_delete_stalefiles = true;
			m_operation = "deletestalefiles";
			validoptions.push_back("vol");
			validoptions.push_back("db");
		}
		else if(0 == stricmp(argv[1], "--deleteunusablepoints"))
		{
			m_delete_unusable_points = true;
			m_operation = "deleteunusablepoints";
			validoptions.push_back("vol");
			validoptions.push_back("db");
		}
		else if (0 == stricmp(argv[1], "--registermt"))
		{
			m_registermt = true;
			m_operation = "registermt";
		}
		else if (0 == stricmp(argv[1], "--processcsjobs"))
		{
			bProcessCsJobs = true;
			m_operation = "processcsjobs";
		}
        else
        {
            usage(argv[0]);
            rv = false;
            break;
        }

        rv = parseInput();

        if(!rv)
            break;

        if(m_validate)
        {
            rv = validate();
            break;
        }

        if(m_iopattern)
        {
            rv = printiodistribution();
            break;
        }

        if(m_showsummary)
        {
            rv = showsummary();
            break;
        }

        if(m_listcommonpt)
        {
            rv = listcommonpt();
            break;
        }

        if (m_listevents)
        {
            SV_ULONGLONG eventCount;
            rv = listevents(eventCount);
            break;
        }

        if(m_snapshot)
        {
            rv = snapshot();
            break;
        }

        if(m_recover)
        {
            rv = recover();
            break;
        }

        if(m_rollback)
        {
            rv = rollback();

            break;
        }			

        if(m_vsnap)
        {
            rv = vsnap();
            break;
        }

        if(m_virtualvolume)
        {
            rv=virtualvolume();
            break;
        }

        if(m_hide)
        {
            rv = hide();
            break;
        }

        if(m_unhidero)
        {
            rv = unhidero();
            break;
        }

        if(m_unhiderw)
        {
            rv = unhiderw();
            break;
        }
        if(m_sparsepolicysummary)
        {
            rv = getSparsePolicySummary();
        }
        if(m_fixerrors)
        {
            rv = fixerrors();
            break;
        }

        if(m_verifiedtag)
        {
            rv = verifiedtag();
            break;
        }
        if(m_lockbookmark)
        {
            rv = lockbookmark();
            break;
        }
        if(m_unlockbookmark)
        {
            rv = unlockbookmark();
            break;
        }
        if(m_displaystatistics)
        {
            rv = displaystatistics();
            break;
        }
        if(m_moveRetention)
        {
            rv = moveRetentionLog();
            break;
        }
        if(m_genCdpData)
        {
            rv = genCdpData();
            break;
        }
        if(m_unhideall)
        {
            rv = unhideall();
            break;
        }
        if(m_displaysavings)
        {
            rv = displaysavings();
            break;
        }
        //#15949 :
        if(m_showreplicationpairs)
        {
            rv = showReplicationPairs();
            break;
        }
        if(m_showsnapshotrequests)
        {
            rv = showSnapshotRequests();
            break;
        }

        if(m_listtargetvolumes)
        {
            rv = listTargetVolumes();
            break;
        }

        if(m_listprotectedvolumes)
        {
            rv = listProtectedVolumes();
            break;
        }
        if(m_registerhost)
        {
            rv = (0 == RegisterHost());
            break;
        }
        //16211
#ifdef SV_WINDOWS
        if(m_reattachdismountedvolumes)
        {
            rv = reattachDismountedVolumes();
            break;
        }
#endif //End of SV_WINDOWS
        if(m_displaymetadata)
        {
            rv = displaymetadata();
            break;
        }

        if(m_volumecompare)
        {
            rv = volumecompare();
            break;
        }

        if(m_readvolume)
        {
            rv = readvolume();
            break;
        }

        if(m_delete_stalefiles)
        {
            rv = deleteStaleFiles();
            break;
        }

        if(m_delete_unusable_points)
        {
            rv = deleteUnusablePoints();
            break;
        }

        if (m_registermt)
        {
            rv = registermt();
            break;
        }

        if (bProcessCsJobs)
        {
            rv = processCsJobs();
            break;
        }

    } while (0);

    if(!uninit())
        return false;

    return rv;
}

/*
* FUNTION NAME : CDPCli::genCdpData
*
* DESCRIPTION : Generates svd files based on the specified io size, no of ios etc.
*               These files contain random data.
*
* INPUT PARAMETERS :  none
*   
* OUTPUT PARAMETERS :  none
*
* RETURN VALUE : true if succeeds false otherwise.
*
*/
bool CDPCli::genCdpData()
{
    bool rv = true;

    std::string destDir = ".";
    unsigned long long volumeSize = 0;
    unsigned long long ioSize;
    unsigned long long timePerCoalesceInterval;
    unsigned long long noOfIntervals;
    unsigned long long noOfNonOverlappingIOsPerCoalesceInterval;
    unsigned long long noOfDuplicateIOsPerCoalesceInterval;
    unsigned long long startTime = 0;

    do
    {
        if(argc != 2)
        {
            usage(argv[0], m_operation);
            rv = false;
            break;
        }

        try
        {
            char buffer[260];
            std::string value;

            std::cout << "Specify the dest dir path where diffs should be created: ";
            std::cin.getline(buffer, 260);
            value = buffer;
            if(value != "")
            {
                destDir = value;
            }

            do
            {
                std::cout << "Specify the volume size(in bytes, should be multiple of 512): ";
                std::cin.getline(buffer, 100);
                value = buffer;
                if(value != "")
                {
                    volumeSize = boost::lexical_cast<unsigned long long>(value);
                }
                else
                {
                    std::cout << "Please specify proper volume size." << std::endl;
                }
            }while((volumeSize % 512 != 0) && !CDPUtil::QuitRequested());

            do
            {
                std::cout << "Specify the io size(in bytes, should be multiple of 512): ";
                std::cin.getline(buffer, 100);
                value = buffer;
                if(value != "")
                {
                    ioSize = boost::lexical_cast<unsigned long long>(value);
                }
                else
                {
                    std::cout << "Please specify proper value for io size." << std::endl;
                }
            }while((ioSize % 512 != 0) && !CDPUtil::QuitRequested());

            do
            {
                std::cout << "Specify the coalesce interval(in hours): ";
                std::cin.getline(buffer, 100);
                value = buffer;
                if(value != "")
                {
                    timePerCoalesceInterval = boost::lexical_cast<unsigned long long>(value);
                }
                else
                {
                    std::cout << "Please specify proper coalesce interval." << std::endl;
                }
            }while((timePerCoalesceInterval == 0) && !CDPUtil::QuitRequested());

            do
            {
                std::cout << "Specify the duration(in hours, should be multiple of " << timePerCoalesceInterval << "): ";
                std::cin.getline(buffer, 100);
                value = buffer;
                if(value != "")
                {
                    noOfIntervals = boost::lexical_cast<unsigned long long>(value);
                }
                else
                {
                    std::cout << "Please specify proper duration." << std::endl;
                }
            }while((noOfIntervals % timePerCoalesceInterval != 0) && !CDPUtil::QuitRequested());
            noOfIntervals /= timePerCoalesceInterval;

            do
            {
                std::cout << "Specify no of non overlapping IOs every " << timePerCoalesceInterval
                    << " hours(should be multiple of 100): ";
                std::cin.getline(buffer, 100);
                value = buffer;
                if(value != "")
                {
                    noOfNonOverlappingIOsPerCoalesceInterval = boost::lexical_cast<unsigned long long>(value);
                }
                else
                {
                    std::cout << "Please specify proper non overlapping io count." << std::endl;
                }
            }while((noOfNonOverlappingIOsPerCoalesceInterval % 100 != 0) && !CDPUtil::QuitRequested());

            do
            {
                std::cout << "Specify no of duplicate IOs every " << timePerCoalesceInterval
                    << " hours(should be less than no. of non overlapping IOs): ";
                std::cin.getline(buffer, 100);
                value = buffer;
                if(value != "")
                {
                    noOfDuplicateIOsPerCoalesceInterval = boost::lexical_cast<unsigned long long>(value);
                }
                else
                {
                    std::cout << "Please specify proper duplicate io count." << std::endl;
                }
            }while(noOfDuplicateIOsPerCoalesceInterval > noOfNonOverlappingIOsPerCoalesceInterval
                && !CDPUtil::QuitRequested());

            do
            {
                std::cout << "Specify the start time of first diff to be generated: ";
                std::cin.getline(buffer, 100);
                value = buffer;
            }while(!CDPUtil::InputTimeToFileTime(value, startTime) && !CDPUtil::QuitRequested());
        }
        catch(std::exception e)
        {
            std::stringstream msg;
            msg << "Cought an exception: " << e.what() << " while casting last input" << std::endl;
            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
            rv = false;
            break;
        }
        catch(...)
        {
            std::stringstream msg;
            msg << "Cought an unknown exception while casting last input" << std::endl;
            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
            rv = false;
            break;
        }

        unsigned long long noOfIOsPerCoalesceInterval = noOfNonOverlappingIOsPerCoalesceInterval +
            noOfDuplicateIOsPerCoalesceInterval;

        unsigned long long timePerIOInHNsec = (timePerCoalesceInterval * 36000000000ULL) / noOfIOsPerCoalesceInterval;

        unsigned long long interIOGap =
            (unsigned long long)(volumeSize / (double)noOfNonOverlappingIOsPerCoalesceInterval);
        interIOGap = (interIOGap / 512) * 512;
        if (interIOGap < ioSize)
        {
            std::stringstream msg;
            msg << "Volume is not large enough to hold " << noOfNonOverlappingIOsPerCoalesceInterval
                << " non overlapping IOs" << std::endl;
            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
            rv = false;
            break;
        }

        //std::ofstream out("ios.txt");

        std::vector<SVD_DIRTY_BLOCK> drtblks;
        srand(time(NULL));

        unsigned long long endTime = 0;
        int remainingIntervals = noOfIntervals;
        SV_INT progressPercent, prevProgressPercent;
        SV_ULONGLONG iosWritten = 0;

        while(remainingIntervals-- && !CDPUtil::QuitRequested())
        {
            int random = (rand() % (interIOGap - ioSize) / 512) * 512;

            std::string starttime, endtime;
            if(!CDPUtil::ToDisplayTimeOnConsole(startTime, starttime))
            {
                rv = false;
                break;
            }
            if(!CDPUtil::ToDisplayTimeOnConsole(startTime + (timePerCoalesceInterval * 36000000000ULL), endtime))
            {
                rv = false;
                break;
            }
            std::cout << "Time Interval: " << starttime << " to " << endtime << std::endl;

            progressPercent = 0;
            prevProgressPercent = -1;
            iosWritten = 0;

            unsigned long long remainingIOs = noOfNonOverlappingIOsPerCoalesceInterval;
            while(remainingIOs-- && !CDPUtil::QuitRequested())
            {
                //out << "Offset: " << remainingIOs*interIOGap + random << " Length: " << ioSize << std::endl;
                SVD_DIRTY_BLOCK drtblk;
                drtblk.ByteOffset = remainingIOs*interIOGap + random;
                drtblk.Length = ioSize;
                drtblks.push_back(drtblk);

                if(drtblks.size()*ioSize > 67108864
                    || drtblks.size() == noOfIOsPerCoalesceInterval
                    || drtblks.size() > 4096)
                {
                    endTime = startTime + drtblks.size()*timePerIOInHNsec;
                    if(!WriteSVDFile(destDir,
                        startTime,
                        endTime,
                        drtblks,
                        iosWritten,
                        noOfIOsPerCoalesceInterval,
                        progressPercent,
                        prevProgressPercent))
                    {
                        rv = false;
                        break;
                    }
                    drtblks.clear();
                    startTime = endTime;
                }
            }

            if(!rv)
                break;

            remainingIOs = noOfDuplicateIOsPerCoalesceInterval;
            while(remainingIOs-- && !CDPUtil::QuitRequested())
            {
                //out << "Offset: " << remainingIOs*interIOGap + random << " Length: " << ioSize << std::endl;
                SVD_DIRTY_BLOCK drtblk;
                drtblk.ByteOffset = remainingIOs*interIOGap + random;
                drtblk.Length = ioSize;
                drtblks.push_back(drtblk);

                if(drtblks.size()*ioSize > 67108864
                    || drtblks.size() == noOfIOsPerCoalesceInterval
                    || drtblks.size() > 4096)
                {
                    endTime = startTime + drtblks.size()*timePerIOInHNsec;
                    if(!WriteSVDFile(destDir,
                        startTime,
                        endTime,
                        drtblks,
                        iosWritten,
                        noOfIOsPerCoalesceInterval,
                        progressPercent,
                        prevProgressPercent))
                    {
                        rv = false;
                        break;
                    }
                    drtblks.clear();
                    startTime = endTime;
                }
            }

            if(!rv)
                break;

            if(!drtblks.empty())
            {
                endTime = startTime + drtblks.size()*timePerIOInHNsec;
                if(!WriteSVDFile(destDir,
                    startTime,
                    endTime,
                    drtblks,
                    iosWritten,
                    noOfIOsPerCoalesceInterval,
                    progressPercent,
                    prevProgressPercent))
                {
                    rv = false;
                    break;
                }
                drtblks.clear();
            }

            std::cout << "\nSpace used by non overlapping IOs: " << noOfNonOverlappingIOsPerCoalesceInterval * ioSize
                << " Bytes" << std::endl;
            std::cout << "Space used by duplicate IOs: " << noOfDuplicateIOsPerCoalesceInterval * ioSize
                << " Bytes" << std::endl << std::endl;
        }
    }while(false);

    return rv;
}

/*
* FUNTION NAME : CDPCli::WriteSVDFile
*
* DESCRIPTION : Writes the drtds to svd file. This function is used by genCdpData() alone.
*               This is for testing purpose.
*
* INPUT PARAMETERS :  starttime, endtime, drtds
*   
* OUTPUT PARAMETERS :  none
*
* RETURN VALUE : true if succeeds false otherwise.
*
*/
bool CDPCli::WriteSVDFile(std::string& destDir,
                          SV_ULONGLONG& starttime,
                          SV_ULONGLONG& endtime,
                          std::vector<SVD_DIRTY_BLOCK>& drtds,
                          SV_ULONGLONG& iosWritten,
                          SV_ULONGLONG& noOfIOsPerCoalesceInterval,
                          SV_INT& progressPercent,
                          SV_INT& prevProgressPercent)
{
    bool rv = true;
    ACE_HANDLE hFile = ACE_INVALID_HANDLE;

    std::stringstream fileName;
    fileName << "completed_ediffcompleted_diff_S" << starttime << "_0_E" << endtime << "_0_WE1_"
        << "12345678" << ".dat";

    std::string tempFileName = "tmp_" + fileName.str();
    std::string destFilePath = destDir + ACE_DIRECTORY_SEPARATOR_STR_A + fileName.str();

    do
    {
        // PR#10815: Long Path support
        if( (hFile = ACE_OS::open(getLongPathName(tempFileName.c_str()).c_str(), O_CREAT | O_RDWR)) == ACE_INVALID_HANDLE )
        {
            std::stringstream msg;
            msg << "Failed to open file: " << tempFileName << ". error no: "
                << ACE_OS::last_error() << std::endl;
            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
            rv = false;
            break;
        }

        SVD_PREFIX svdPrefix;
        svdPrefix.tag = SVD_TAG_HEADER1;
        svdPrefix.count = 1;
        svdPrefix.Flags = 0;
        if( ACE_OS::write(hFile, &svdPrefix, SVD_PREFIX_SIZE) != SVD_PREFIX_SIZE )
        {
            std::stringstream msg;
            msg << "Failed writing to file: " << tempFileName << ". error no: "
                << ACE_OS::last_error() << std::endl;
            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
            rv = false;
            break;
        }

        SVD_HEADER1 svdHeader;
        memset(&svdHeader, 0, SVD_HEADER1_SIZE);
        if( ACE_OS::write(hFile, &svdHeader, SVD_HEADER1_SIZE) != SVD_HEADER1_SIZE )
        {
            std::stringstream msg;
            msg << "Failed writing to file: " << tempFileName << ". error no: "
                << ACE_OS::last_error() << std::endl;
            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
            rv = false;
            break;
        }

        svdPrefix.tag = SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE;
        if( ACE_OS::write(hFile, &svdPrefix, SVD_PREFIX_SIZE) != SVD_PREFIX_SIZE )
        {
            std::stringstream msg;
            msg << "Failed writing to file: " << tempFileName << ". error no: "
                << ACE_OS::last_error() << std::endl;
            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
            rv = false;
            break;
        }

        SVD_TIME_STAMP svdTimeStamp;
        memset(&svdTimeStamp.Header, 0, sizeof(svdTimeStamp.Header));
        svdTimeStamp.TimeInHundNanoSecondsFromJan1601 = starttime;
        svdTimeStamp.ulSequenceNumber = 0;
        if( ACE_OS::write(hFile, &svdTimeStamp, SVD_TIMESTAMP_SIZE) != SVD_TIMESTAMP_SIZE )
        {
            std::stringstream msg;
            msg << "Failed writing to file: " << tempFileName << ". error no: "
                << ACE_OS::last_error() << std::endl;
            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
            rv = false;
            break;
        }

        svdPrefix.tag = SVD_TAG_LENGTH_OF_DRTD_CHANGES;
        if( ACE_OS::write(hFile, &svdPrefix, SVD_PREFIX_SIZE) != SVD_PREFIX_SIZE )
        {
            std::stringstream msg;
            msg << "Failed writing to file: " << tempFileName << ". error no: "
                << ACE_OS::last_error() << std::endl;
            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
            rv = false;
            break;
        }

        SV_ULARGE_INTEGER svUlargeInt;
        svUlargeInt.QuadPart = drtds.size()*(SVD_PREFIX_SIZE + SVD_DIRTY_BLOCK_SIZE);
        for(std::vector<SVD_DIRTY_BLOCK>::iterator iter = drtds.begin(); iter != drtds.end(); iter++)
            svUlargeInt.QuadPart += iter->Length;

        if( ACE_OS::write(hFile, &svUlargeInt, SVD_LODC_SIZE) != SVD_LODC_SIZE )
        {
            std::stringstream msg;
            msg << "Failed writing to file: " << tempFileName << ". error no: "
                << ACE_OS::last_error() << std::endl;
            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
            rv = false;
            break;
        }

        SV_LONGLONG bufferSize = drtds.begin()->Length + SVD_DIRTY_BLOCK_SIZE + SVD_PREFIX_SIZE;
        if( bufferSize < 1048576 )
            bufferSize = 1048576;

        boost::shared_array<char> buffer(new char[bufferSize]);
        SV_LONGLONG usedBufferLength = 0;

        for(std::vector<SVD_DIRTY_BLOCK>::iterator iter = drtds.begin();
            iter != drtds.end() && !CDPUtil::QuitRequested();
            iter++)
        {
            if((usedBufferLength + iter->Length + SVD_DIRTY_BLOCK_SIZE + SVD_PREFIX_SIZE) > bufferSize)
            {
                if( ACE_OS::write(hFile, buffer.get(), usedBufferLength) != usedBufferLength )
                {
                    std::stringstream msg;
                    msg << "Failed writing to file: " << tempFileName << ". error no: "
                        << ACE_OS::last_error() << std::endl;
                    DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
                    rv = false;
                    break;
                }
                else
                {
                    usedBufferLength = 0;
                }
            }

            svdPrefix.tag = SVD_TAG_DIRTY_BLOCK_DATA;
            inm_memcpy_s(buffer.get() + usedBufferLength, (bufferSize - usedBufferLength), &svdPrefix, SVD_PREFIX_SIZE);
            usedBufferLength += SVD_PREFIX_SIZE;

            inm_memcpy_s(buffer.get() + usedBufferLength, (bufferSize - usedBufferLength), &*iter, SVD_DIRTY_BLOCK_SIZE);
            usedBufferLength += SVD_DIRTY_BLOCK_SIZE;

            usedBufferLength += iter->Length;

            iosWritten++;

            progressPercent = (SV_UINT)( (iosWritten / (double)noOfIOsPerCoalesceInterval) * 100);
            if(progressPercent != prevProgressPercent)
            {
                std::cout << "\rProgress: " << progressPercent << "%";
                prevProgressPercent = progressPercent;
            }
        }

        if(!rv)
            break;

        if(usedBufferLength != 0)
        {
            if( ACE_OS::write(hFile, buffer.get(), usedBufferLength) != usedBufferLength )
            {
                std::stringstream msg;
                msg << "Failed writing to file: " << tempFileName << ". error no: "
                    << ACE_OS::last_error() << std::endl;
                DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
                rv = false;
                break;
            }
            else
            {
                usedBufferLength = 0;
            }
        }

        svdPrefix.tag = SVD_TAG_TIME_STAMP_OF_LAST_CHANGE;
        if( ACE_OS::write(hFile, &svdPrefix, SVD_PREFIX_SIZE) != SVD_PREFIX_SIZE )
        {
            std::stringstream msg;
            msg << "Failed writing to file: " << tempFileName << ". error no: "
                << ACE_OS::last_error() << std::endl;
            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
            rv = false;
            break;
        }

        svdTimeStamp.TimeInHundNanoSecondsFromJan1601 = endtime;
        if( ACE_OS::write(hFile, &svdTimeStamp, SVD_TIMESTAMP_SIZE) != SVD_TIMESTAMP_SIZE )
        {
            std::stringstream msg;
            msg << "Failed writing to file: " << tempFileName << ". error no: "
                << ACE_OS::last_error() << std::endl;
            DebugPrintf(SV_LOG_ERROR, "%s", msg.str().c_str());
            rv = false;
            break;
        }
    }while(false);

    ACE_OS::close(hFile);

    // PR#10815: Long Path support
    if(rv && ACE_OS::rename(getLongPathName(tempFileName.c_str()).c_str(),
        getLongPathName(destFilePath.c_str()).c_str()) < 0)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to rename %s to %s, error code: %d\n",
            tempFileName.c_str(), fileName.str().c_str(), ACE_OS::last_error());
        rv = false;
    }

    return rv;
}

/*
* FUNTION NAME : CDPCli::displaystatistics
*
* DESCRIPTION : 
*
*   Displays the statistics like RPO, Apply time etc. 
*
* INPUT PARAMETERS :  none
*   
* OUTPUT PARAMETERS :  none
*
* RETURN VALUE : true if succeeds false otherwise.
*
*/
bool CDPCli::displaystatistics()
{
    bool rv = true;
    std::string volumeName;
    NVPairs::iterator iter;

    do
    {
        if(argc < 3)
        {
            usage(argv[0], m_operation);
            rv = false;
            break;
        }

        iter = nvpairs.find("vol");

        std::string formattedVolumeName;
        if(iter != nvpairs.end())
        {
            volumeName = iter->second;
            formattedVolumeName = iter->second;
            CDPUtil::trim(volumeName);
            CDPUtil::trim(formattedVolumeName);

            FormatVolumeName(formattedVolumeName);

            if (IsReportingRealNameToCx())
            {
                GetDeviceNameFromSymLink(formattedVolumeName);
            }
            else
            {
                std::string linkname = formattedVolumeName;
                if (!GetLinkNameIfRealDeviceName(formattedVolumeName, linkname))
                {
                    DebugPrintf(SV_LOG_ERROR, "@LINE %d in FILE %s Invalid volume specified\n", LINE_NO, FILE_NAME);
                    rv = false;
                    break;
                } 
                volumeName = formattedVolumeName = linkname;
            }

            if(formattedVolumeName != volumeName)
                DebugPrintf(SV_LOG_INFO," %s is a symbolic link to %s \n\n",
                formattedVolumeName.c_str(),volumeName.c_str());

            if (!IsFileORVolumeExisting(formattedVolumeName))
            {
                DebugPrintf(SV_LOG_INFO, "%s%s%s", "Volume: " , volumeName.c_str() , " doesn't exist\n\n");
                DebugPrintf(SV_LOG_INFO, "Please specify valid target volume name.\n");
                rv = false;
                break;
            }
        }
        else
        {
            usage(argv[0], m_operation);
            DebugPrintf(SV_LOG_INFO, "Please specify the --vol option.\n");
            rv = false;
            break;
        }
        if(isTargetVolume(volumeName))
        {
            std::string cacheLocation = m_configurator->getVxAgent().getDiffTargetCacheDirectoryPrefix();
            cacheLocation += ACE_DIRECTORY_SEPARATOR_STR_A;
            cacheLocation += m_configurator->getVxAgent().getHostId();
            cacheLocation += ACE_DIRECTORY_SEPARATOR_STR_A;
            cacheLocation += GetVolumeDirName(formattedVolumeName);
            SV_ULONGLONG diffsPendingInTarget = CurrentDiskUsage(cacheLocation);

            SV_ULONGLONG diffsPendingInCX = getDiffsPendingInCX(formattedVolumeName);
            SV_ULONGLONG currentRPO = getCurrentRpo(formattedVolumeName);
            SV_ULONGLONG applyRate = getApplyRate(formattedVolumeName);
            cout << "-----------------------------------------------------\n";
            cout << setw(30) << setiosflags( ios::left ) << "############# REPLICATION STATISTICS #############"<<"\n";
            cout << "-----------------------------------------------------\n";
            cout << setw(30) << setiosflags( ios::left ) << "  Target Volume Name:"
                << setw(20) << setiosflags( ios::left ) << volumeName << "\n";
            cout << setw(30) << setiosflags( ios::left ) << "  Diffs pending in CX:"
                << setw(20) << setiosflags( ios::left ) << diffsPendingInCX << "\n";
            cout << setw(30) << setiosflags( ios::left ) << "  Diffs pending in Target:"
                << setw(20) << setiosflags( ios::left ) << diffsPendingInTarget << "\n";
            cout << setw(30) << setiosflags( ios::left ) << "  Current RPO (secs):"
                << setw(20) << setiosflags( ios::left ) << currentRPO << "\n";
            cout << setw(30) << setiosflags( ios::left ) << "  Apply rate (Bytes/sec):"
                << setw(20) << setiosflags( ios::left ) << applyRate << "\n";
            if(applyRate!=0)
            {
                SV_ULONGLONG applyTime = diffsPendingInTarget / applyRate;
                cout << setw(30) << setiosflags( ios::left ) << "  Apply time (secs):"
                    << setw(20) << setiosflags( ios::left ) << applyTime << "\n\n";
            }
        }
        else
        {
            DebugPrintf(SV_LOG_INFO,"The specified volume %s is not a target volume.\n", volumeName.c_str());
        }

    }while(false);

    return rv;
}

SV_ULONGLONG CDPCli::CurrentDiskUsage( std::string &cachelocation )
{
    bool rv = true;
    SV_ULONGLONG totalUsage = 0;
    SV_ULONGLONG curUsage = 0;

    try
    {
        DebugPrintf(SV_LOG_DEBUG, "ENTERED %s %s\n",FUNCTION_NAME, cachelocation.c_str());

        do
        {
            // open directory
            // PR#10815: Long Path support
            ACE_DIR *dirp = sv_opendir(cachelocation.c_str());

            if (dirp == NULL)
            {
                int lasterror = ACE_OS::last_error();
                if (ENOENT == lasterror || ESRCH == lasterror)
                {
                    rv = true;
                    break;
                } else 
                {
                    DebugPrintf(SV_LOG_ERROR, 
                        "\n%s encountered error %d while reading from %s\n",
                        FUNCTION_NAME, lasterror, cachelocation.c_str()); 
                    rv = false;
                    break;
                }
            }

            struct ACE_DIRENT *dp = NULL;

            do
            {
                ACE_OS::last_error(0);

                // get directory entry
                if ((dp = ACE_OS::readdir(dirp)) != NULL)
                {
                    // find a matching entry
                    const std::string diffMatchFiles = "completed_ediffcompleted_diff_*.dat*" ;
                    if (RegExpMatch(diffMatchFiles.c_str(), ACE_TEXT_ALWAYS_CHAR(dp->d_name)))
                    {
                        string filePath;

                        filePath = cachelocation.c_str();                    
                        filePath += ACE_DIRECTORY_SEPARATOR_CHAR_A;
                        filePath += ACE_TEXT_ALWAYS_CHAR(dp->d_name);

                        ACE_stat s;
                        // PR#10815: Long Path support
                        curUsage = ( sv_stat(getLongPathName(filePath.c_str()).c_str(), &s ) < 0 ) ? 0 : s.st_size;
                        totalUsage += curUsage;
                    }
                }

            } while(dp);

            // close directory
            ACE_OS::closedir(dirp);

        } while(false);

        DebugPrintf(SV_LOG_DEBUG, "EXITED %s %s\n",FUNCTION_NAME, cachelocation.c_str());
    } 
    catch ( ContextualException& ce )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s for %s.\n",
            FUNCTION_NAME, ce.what(), cachelocation.c_str());
    }
    catch( exception const& e )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered exception %s for %s.\n",
            FUNCTION_NAME, e.what(), cachelocation.c_str());
    }
    catch ( ... )
    {
        rv = false;
        DebugPrintf(SV_LOG_ERROR, 
            "\n%s encountered unknown exception for %s.\n",
            FUNCTION_NAME, cachelocation.c_str());
    }

    // TBD: we should avoid overloading return value with output
    return totalUsage;
}

/*
* FUNTION NAME : CDPCli::fixerrors
*
* DESCRIPTION : 
*
*   Used to fix the errors like staleinodes and date issues in the database. 
*
* INPUT PARAMETERS :  none
*   
* OUTPUT PARAMETERS :  none
*
* RETURN VALUE : true if succeeds false otherwise.
*
*/
bool CDPCli::fixerrors()
{
    bool rv = true;
    int flags = 0;
    string db_name, volumeName, tempDir, fix;
    bool tempDirMandatory = false;
    NVPairs::iterator iter;

    do
    {
        iter = nvpairs.find("vol");
        if(iter != nvpairs.end())
        {
            volumeName = iter->second;
            std::string linkname = volumeName;
            if (!GetLinkNameIfRealDeviceName(volumeName, linkname))
            {
                DebugPrintf(SV_LOG_ERROR, "@LINE %d in FILE %s Invalid volume specified\n", LINE_NO, FILE_NAME);
            } 
            volumeName = linkname;
        }
        //else
        //{
        //	DebugPrintf(SV_LOG_INFO, "Please specify the volume.\n");
        //	rv = false;
        //	break;
        //}
        iter = nvpairs.find("db");
        if(iter != nvpairs.end())
        {
            db_name = iter->second;
        }
        else
        {
            DebugPrintf(SV_LOG_INFO, "Please specify the database path.\n");
            rv = false;
            break;
        }

        std::string db_filename =  basename_r(db_name.c_str());
        if(db_filename == CDPV1DBNAME)
        {

            iter = nvpairs.find("fix");
            if(iter != nvpairs.end())
            {
                fix = iter->second;
                svector_t fixoptions = CDPUtil::split(fix, "," );
                svector_t::iterator fixOptionsIter;
                if( (fixOptionsIter = find(fixoptions.begin(), fixoptions.end(), "time")) != fixoptions.end() )
                {
                    flags = flags | FIX_TIME;
                    fixoptions.erase(fixOptionsIter);
                    tempDirMandatory = true;
                }
                if( (fixOptionsIter = find(fixoptions.begin(), fixoptions.end(), "staleinodes")) != fixoptions.end() )
                {
                    flags = flags | FIX_STALEINODES;
                    fixoptions.erase(fixOptionsIter);
                    tempDirMandatory = true;
                }
                if( (fixOptionsIter = find(fixoptions.begin(), fixoptions.end(), "physicalsize")) != fixoptions.end() )
                {
                    flags = flags | FIX_PHYSICALSIZE;
                    fixoptions.erase(fixOptionsIter);
                }
                if(fixoptions.size())
                {
                    DebugPrintf(SV_LOG_INFO, "Invalid fix option specified. Check and try again.\n");
                    rv = false;
                    break;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "--fix option is mandatory.\n");
                rv = false;
                break;
            }
            iter = nvpairs.find("tempdir");
            if(iter != nvpairs.end())
            {
                tempDir = iter->second;
                CDPUtil::trim(tempDir);
                if(tempDir == " ")
                {
                    DebugPrintf(SV_LOG_INFO, "Please provide a valid temp folder.\n");
                    rv = false;
                    break;
                }
            }
            else if(tempDirMandatory)
            {
                DebugPrintf(SV_LOG_INFO, "--tempdir option is mandatory.\n");
                rv = false;
                break;
            }
            else
                tempDir="";
        }

        bool serviceRunning;
        if(!ReplicationAgentRunning(serviceRunning))  
        {
            rv = false;
            break;
        }

        if(serviceRunning)
        {
            DebugPrintf(SV_LOG_INFO, "Service is running please stop it and try again.\n");
            rv = false;
            break;
        }
        else
        {
            tempDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            CDPfixdb fixer(db_name, tempDir);
            rv = fixer.fixDBErrors(flags);
        }
    }while(false);

    return rv;
}


/*
* FUNTION NAME : CDPCli::CreatePendingActionFile
*
* DESCRIPTION : 
*
*   Create a dummy file under the application install directory. 
*   on detection of this file svagents will
*   send a update volume attribute request to CX when it is up
*
* INPUT PARAMETERS :  
*   
*   volumename  : name of the volume on which action is performed
*   volumeState : operation performed (VOLUME_VISIBLE_RO, VOLUME_VISIBLE_RW)
*
* OUTPUT PARAMETERS :  none
*
* RETURN VALUE : true if succeeds false otherwise.
*
*/
bool CDPCli::CreatePendingActionFile(const std::string &volumename, VOLUME_STATE volumeState)
{
    bool returnValue = true;
    try
    {
        do
        {
			string extn; 
			switch(volumeState)
			{
			case VOLUME_VISIBLE_RO:
				extn = ".unhideRO";
				break;
			case VOLUME_VISIBLE_RW:
				extn = ".unhideRW";
				break;
			}


			string UnhideActionsFilePath = CDPUtil::getPendingActionsFilePath(volumename, extn);


            // PR#10815: Long Path support
            ACE_HANDLE h =  ACE_OS::open(getLongPathName(UnhideActionsFilePath.c_str()).c_str(), O_RDWR | O_CREAT, FILE_SHARE_READ);

            if(ACE_INVALID_HANDLE == h) 
            {
                stringstream l_stdfatal;
                l_stdfatal << "Error detected  " << "in Function:" 
                    << FUNCTION_NAME 
                    << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                    << "Error during file (" <<  UnhideActionsFilePath << ")creation.\n"
                    <<  Error::Msg() << "\n"
                    << "Failed to add " << volumename
                    << " in pending unhideRW action list\n";

                DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
                returnValue = false;
            } else {
                ACE_OS::close(h);
            }

            if(volumeState == VOLUME_VISIBLE_RW)
            {
				bool skip_cs_reporting = true;
                const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::DeviceUnlockedInReadWriteMode;
                std::stringstream msg("Target device is unlocked in read-write mode.");
                msg << ". Marking resync for the device " << volumename << " with resyncReasonCode=" << resyncReasonCode;
                DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                ResyncReasonStamp resyncReasonStamp;
                STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);

                CDPUtil::store_and_sendcs_resync_required(volumename, msg.str(), resyncReasonStamp);
            }
        }while(false);
    }
    catch (exception& e)
    {
        DebugPrintf(SV_LOG_ERROR, 
            "Failed to add %s in the pending unhide action list. Failed with the exception:%s\n",
            volumename.c_str(),e.what());
        returnValue = false;
    }
    catch( ... )
    {
        stringstream l_stdfatal;
        l_stdfatal << "Failed to add " << volumename 
            << " in pending unhide action list\n";

        DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
        returnValue = false;
    }
    return returnValue;
}

/*
* FUNCTION NAME : GetUniqueVsnapId
*
* DESCRIPTION : Helper routine to get a unique number for creating vsnap devicename
*
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS :  None
*
* NOTES : 
*
* return value : Unique number 
*
*/
unsigned long long CDPCli::GetUniqueVsnapId()
{
    unsigned long long vsnapId = 0;

    if(!IncrementAndGetVsnapId(vsnapId))
    {
        return vsnapId;
    }

    return vsnapId;
}

//
// FUNCTION NAME : getvsnappairsanddb
//
// DESCRIPTION : 1. Retrieves the vsnap pair details provided using cdpcli
//				--vsnappairs switch. 
//				2. Creates snapshot requests for the pairs retrieved
//				
//				
//
// INPUT PARAMETERS : None
//
// OUTPUT PARAMETERS :  SNAPSHOT_REQUESTS
//
// NOTES : This is added as part of Volume Group Recovery for vsnaps
//
// return value : True if able to create snapshot requests 
//				  False upon encountering errors in parsing
//					or creating snapshot requests
//
//
bool CDPCli::getvsnappairsanddb(SNAPSHOT_REQUESTS& requests)
{
    NVPairs::iterator iter;
    bool rv = true;
    bool volgrp = false;
    bool notvolgrp = false;
    string src, dest, mtpt, db;
    int pairnum = 0;

    do
    {
        iter = nvpairs.find("op");
        if(iter != nvpairs.end())
        {
            if(iter -> second != "mount")
            {
                DebugPrintf(SV_LOG_ERROR, "Vsnap Failed: The only valid operation when using volume group recovery is mount.\n\n");
                rv = false;
                break;
            }
        }
        else
        {			
            DebugPrintf(SV_LOG_INFO,"Vsnap Failed: It's mandatory to specify the operation type using --op \
                                    option. \n\n");
            usage(argv[0], m_operation);
            rv = false;
            break;
        }					


        iter = nvpairs.find("vsnapid");
        if(iter != nvpairs.end())
        {
            DebugPrintf(SV_LOG_ERROR, "Vsnap Failed: vsnapid is not a valid option when using volume group recovery .\n\n");
            rv = false;
            break;
        }


        iter = nvpairs.find("time");
        if(iter != nvpairs.end())
        {
            DebugPrintf(SV_LOG_ERROR, "Vsnap Failed: time is not a valid option when using volume group recovery .\n Use \"--at\" instead\n");
            rv = false;
            break;
        }


        iter = nvpairs.find("vsnappairs");
        if(iter != nvpairs.end())
        {
            if(SetSkipCheckFlag())
            {
                DebugPrintf(SV_LOG_ERROR, "The option --skipchecks do not work with --vsnappairs option.\n");
                rv = false;
                break;
            }
            volgrp = true;
            string token = iter -> second;
            svector_t svpairs = CDPUtil::split(token, ";");
            svector_t::iterator svpairs_iter = svpairs.begin();
            for( ; svpairs_iter != svpairs.end(); ++svpairs_iter)
            {
                string svpairinfo = *svpairs_iter;
                svector_t svpair = CDPUtil::split(svpairinfo, ","); //ALERT when only one string is present and no ',' is present
                if(svpair.size() < 1 || svpair.size() > 4)
                {
                    DebugPrintf(SV_LOG_ERROR, "syntax error near %s\n\n", svpairinfo.c_str());
                    usage(argv[0], m_operation);
                    rv = false;
                    break;
                }

                SNAPSHOT_REQUEST::Ptr request(new SNAPSHOT_REQUEST());
                request -> src = svpair[0];

#ifndef SV_WINDOWS
                std::string t;
                unsigned long long VsnapId = GetUniqueVsnapId();
                if(!VsnapId)
                {
                    DebugPrintf(SV_LOG_ERROR, "Error obtaining unique vsnapid\n");
                    rv = false;
                    break;
                }
                t = boost::lexical_cast<std::string>(VsnapId);
                //std::string prefix("/dev/vs/cli");
                std::string prefix(VSNAP_DEVICE_PREFIX);
                prefix += t; 
#ifdef SV_SUN
                if(m_bconfig && m_configurator->isSourceFullDevice(request -> src))
                {
                }
                else
#endif
                    prefix += VSNAP_SUFFIX;
                request -> dest = prefix;
                if(svpair.size() > 1)
                    request -> destMountPoint = svpair[1];
#else
                if(svpair.size() < 2)
                {
                    DebugPrintf(SV_LOG_INFO, "Source Volume = %s\t Target Volume not specified\n", request->src.c_str());
                    DebugPrintf(SV_LOG_INFO,"Source and Target are required to take a vsnap\n");
                    rv = false;
                    break;
                }
                request -> dest = request -> destMountPoint = svpair[1];
#endif

                CDPUtil::trim(request -> src);
                CDPUtil::trim(request -> dest);
                CDPUtil::trim(request -> destMountPoint);

                FirstCharToUpperForWindows(request -> src);
                FirstCharToUpperForWindows(request -> dest);

                if(svpair.size() > 2 && !svpair[2].empty())
                {
                    request -> dbpath = svpair[2];
                    /** Added by BSR - Issue#5208 **/
                    if(!resolvePathAndVerify(request -> dbpath))
                    {
                        DebugPrintf(SV_LOG_ERROR, "Invalid DB path: %s\n", svpair[2].c_str());
                        rv = false;
                        break;
                    }
                    /** End of the change **/
                }
                else
                {
                    if(!getCdpDbName(request -> src, request -> dbpath))
                    {
                        rv = false;
                        break;
                    }	

                    if(request -> dbpath == "")
                    {
                        DebugPrintf(SV_LOG_ERROR, "Retention is not enabled for %s\n", request -> src.c_str());
                        rv = false;
                        break;
                    }
                }
                if(svpair.size() > 3 && !svpair[3].empty())
                {
                    request -> vsnapdatadirectory = svpair[3];
                    /** Added by BSR - Issue#5288 **/
                    SVERROR hr = SVMakeSureDirectoryPathExists( request -> vsnapdatadirectory.c_str() ) ;
                    if( hr.failed() )
                    {
                        DebugPrintf( SV_LOG_ERROR, "Invalid Data Log Path: %s. Path could not be created with error %s\n", 
                            request -> vsnapdatadirectory.c_str(),
                            hr.toString() ) ;
                        rv = false ;
                        break ;
                    }
                    if( !resolvePathAndVerify( request -> vsnapdatadirectory ) )
                    {
                        DebugPrintf( SV_LOG_ERROR, "Invalid Data Log Path: %s\n", (request -> vsnapdatadirectory).c_str() ) ;
                        rv = false;
                        break;
                    }
                    /** End of the change **/
                }
                else
                {
                    size_t length = strlen(request -> dbpath.c_str());
                    std::vector<char> vdir(length, '\0');
                    DirName(request -> dbpath.c_str(), &vdir[0], vdir.size());
                    if(!request -> destMountPoint.empty())
                    {
                        DebugPrintf( SV_LOG_INFO, "Retention DB Path %s being used for storing vsnap metadata for the pair %s -> %s\n", vdir.data(), request -> src.c_str(), request -> destMountPoint.c_str() );
                    }
                    else
                    {
                        DebugPrintf( SV_LOG_INFO, "Retention DB Path %s being used for storing vsnap metadata for the pair %s -> %s\n", vdir.data(), request -> src.c_str(), request -> dest.c_str() );
                    }
                    request -> vsnapdatadirectory = std::string(vdir.data());
                }

                CDPUtil::trim(request -> dbpath);
                CDPUtil::trim(request -> vsnapdatadirectory);

                if (!request->destMountPoint.empty())
                {
                    std::string strerr;
                    if (!isvalidmountpoint(request->src, strerr))
                    {
                        DebugPrintf(SV_LOG_ERROR, "mountpoint option is not valid for target = %s, mountpoint = %s\n", 
                            request->src.c_str(), request->destMountPoint.c_str());
                        DebugPrintf(SV_LOG_ERROR, "The reason is: %s\n", strerr.c_str());
                        rv = false;
                        break;
                    } 
                }

                ++pairnum;
                boost::format formatIter("clirecovervsnap%1%");
                formatIter % pairnum;
                request -> id = formatIter.str();
                request -> operation = SNAPSHOT_REQUEST::RECOVERY_VSNAP;


                iter = nvpairs.find("flags");
                if( iter == nvpairs.end() )
                {
                    request -> vsnapMountOption = 0;
                }			
                else 
                {
                    if( iter->second == "ro")
                        request -> vsnapMountOption = 0;
                    else if(iter->second == "rw")
                        request -> vsnapMountOption = 1;
                    else if(iter->second == "rwt")
                        request -> vsnapMountOption = 2;
                    else
                    {
                        DebugPrintf(SV_LOG_INFO, "Vsnap Failed: Invalid --flags option specified for mount operation\n");
                        rv = false;
                        break;
                    }
                }

                requests.insert(requests.end(), SNAPSHOT_REQUEST_PAIR(request -> id, request));
            }

            if(!rv)
                break;
        }


        iter = nvpairs.find("target");
        if(iter != nvpairs.end())
        {
            notvolgrp = true;
        }

        iter = nvpairs.find("virtual");
        if(iter != nvpairs.end())
        {
            notvolgrp = true;
        }


        iter = nvpairs.find("db");
        if(iter != nvpairs.end())
        {
            notvolgrp = true;
        }

        iter = nvpairs.find("datalogpath");
        if(iter != nvpairs.end())
        {
            notvolgrp = true;
        }


        if(volgrp && notvolgrp)
        {
            DebugPrintf(SV_LOG_ERROR," \'vsnappairs\' and (target, virtual , db, datalogpath) are mutually exclusive options.\n\n");
            usage(argv[0], m_operation);
            rv = false;
            break;
        }

        if(requests.empty())
        {
            DebugPrintf(SV_LOG_INFO, "Please specify the replication target and virtual  volumes for recovery.\n\n");
            usage(argv[0], m_operation);
            rv = false;
            break;
        }

    } while (0);

    return rv;
}
//
// FUNCTION NAME : recovervsnappairs
//
// DESCRIPTION : 1. Retrieves the vsnap pairs 
//				 2. Retrieves the recovery point
//				 3. Retrieves the prescript, postscript
//				 4. Creates the vsnaps for all the pairs
//
// INPUT PARAMETERS : None
//
// OUTPUT PARAMETERS : None
//
// NOTES : This is added as part of Volume Group Recovery for vsnaps
//
// return value : Success/Failure status of vsnap creation 
//                
//
//
bool CDPCli::recovervsnappairs()
{
    bool rv = true;

    do
    {
        SNAPSHOT_REQUESTS requests;
        string prescript, postscript;

        if(!getvsnappairsanddb(requests))
        {
            rv = false;
            break;
        }
        if(!getrecoverypoint(requests))
        {
            rv = false;
            break;
        }	
        if(!getprepostscript(requests, prescript, postscript))
        {
            rv = false;
            break;
        }
        rv = ProcessSnapshotRequests(requests, prescript, postscript, false);
        if(!rv)
            break;

    } while (0);

    return rv;	
}


//
// TODO:
// TODO: Below Routines require cleanup/review
// TODO:
//

/*
* FUNCTION NAME :  CDPCli::vsnap
*
* DESCRIPTION : 
*
*
* INPUT PARAMETERS : none
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::vsnap()
{
    bool rv = true;

    NVPairs::iterator	iter;
    VsnapSourceVolInfo	*SrcVolInfo = NULL;
    VsnapVirtualVolInfo	*VirtualVolInfo = NULL;
    string				errmsg, VolumeList;

    errmsg.clear();
    VolumeList.clear();

    VsnapMgr *Vsnap;

#ifdef SV_WINDOWS
    WinVsnapMgr obj;
    Vsnap=&obj;

#else
    UnixVsnapMgr unixobj;
    Vsnap=&unixobj;
#endif

    do
    {
        if(!IsVsnapDriverAvailable())
        {
            DebugPrintf(SV_LOG_INFO, "The vsnap driver is not present. so we can not proceed with the vsnap operation.\n");
            return true;
        }

        iter = nvpairs.find("vsnappairs");
        if( iter != nvpairs.end() )
        {
            return	recovervsnappairs(); 		
        }

        bool Mount				= false;
        bool Unmount			= false;
        bool ApplyTrackLog		= false;
        bool Unmountall			= false;
        bool List				= false;
        bool UnmountLogsDelete	= true;
        bool StartTrack			= false;
        bool StopTrack			= false;
        bool DeleteLogs			= false;
        bool Sync				= false;
        bool UpgradePersistentStore			= false;
        bool Remount			= false;

        //Get the type of operation
        iter = nvpairs.find("op");
        if(iter != nvpairs.end())
        {
            if( iter->second == "mount")
                Mount = true;
            else if( iter->second == "unmount")
                Unmount = true;
            else if( iter->second == "applytracklog" )
                ApplyTrackLog = true;
            else if( iter->second == "unmountall" )
                Unmountall = true;
            else if( iter->second == "list" )
                List = true;
            else if( iter->second == "starttrack" )
                StartTrack = true;
            else if( iter->second == "stoptrack" )
                StopTrack = true;
            else if( iter->second == "deletelogs" )
                DeleteLogs = true;
            else if( iter->second == "sync")
                Sync = true;
            else if( iter->second == "upgradepersistentstore")
                UpgradePersistentStore = true;
            else if( iter->second == "remount")
                Remount = true;
            else
            {
                DebugPrintf(SV_LOG_INFO,"Unknown operation specified for --op option\n") ;
                usage(argv[0], m_operation);
                rv = false;
                break;
            }
        }
        else
        {			
            DebugPrintf(SV_LOG_INFO,"Vsnap Failed: It's mandatory to specify the operation type using --op \
                                    option. \n\n");
            usage(argv[0], m_operation);
            rv = false;
            break;
        }					

        if(Remount)
        {
            return RemountVsnaps();
        }

        if(UpgradePersistentStore)
        {
            return upgradePersistentStore();	
        }
        iter = nvpairs.find("aftertime");
        if(iter != nvpairs.end())
        {
            DebugPrintf(SV_LOG_INFO, "The option --aftertime is not supported with --target and --virtual option.\n"
                "This option is only valid with --vsnappairs option. For more info see the usage\n");
            return false;
        }
        iter = nvpairs.find("beforetime");
        if(iter != nvpairs.end())
        {
            DebugPrintf(SV_LOG_INFO, "The option --beforetime is not supported with --target and --virtual option.\n"
                "This option is only valid with --vsnappairs option. For more info see the usage\n");
            return false;
        }
        iter = nvpairs.find("recentcrashconsistentpoint");
        if(iter != nvpairs.end())
        {
            DebugPrintf(SV_LOG_INFO, "The option --recentcrashconsistentpoint is not supported with --target and --virtual option.\n"
                "This option is only valid with --vsnappairs option. For more info see the usage\n");
            return false;
        }
        iter = nvpairs.find("recentfsconsistentpoint");
        if(iter != nvpairs.end())
        {
            DebugPrintf(SV_LOG_INFO, "The option --recentfsconsistentpoint is not supported with --target and --virtual option.\n"
                "This option is only valid with --vsnappairs option. For more info see the usage\n");
            return false;
        }
        iter = nvpairs.find("recentappconsistentpoint");
        if(iter != nvpairs.end())
        {
            DebugPrintf(SV_LOG_INFO, "The option --recentappconsistentpoint is not supported with --target and --virtual option.\n"
                "This option is only valid with --vsnappairs option. For more info see the usage\n");
            return false;
        }
        iter = nvpairs.find("timerange");
        if(iter != nvpairs.end())
        {
            DebugPrintf(SV_LOG_INFO, "The option --timerange is not supported with --target and --virtual option.\n"
                "This option is only valid with --vsnappairs option. For more info see the usage\n");
            return false;
        }


        if(List)
        {
            bool verbose = false;

            iter = nvpairs.find("verbose");
            if(iter != nvpairs.end())
                verbose = true;

            std::string vsnapname, target;

            iter = nvpairs.find("virtual");

            if(iter != nvpairs.end() && !(iter->second.empty()))
                vsnapname = iter->second;

            iter = nvpairs.find("target");

            if(iter != nvpairs.end() && !(iter->second.empty()))
                target = iter->second;

            std::vector<VsnapPersistInfo> vsnaps;

            if(!vsnapname.empty())
            {
                if(!Vsnap->RetrieveVsnapInfo(vsnaps, "this", target, vsnapname))
                {
                    DebugPrintf(SV_LOG_INFO, "Unable to retrieve the vsnap info for %s from persistent store.\n", vsnapname.c_str());
                    rv = false;
                    break;
                }
            }
            else if(!target.empty())
            {
                if(!Vsnap->RetrieveVsnapInfo(vsnaps, "target", target))
                {
                    DebugPrintf(SV_LOG_INFO, "Unable to retrieve the vsnap info associated with %s from persistent store.\n", target.c_str());
                    rv = false;
                    break;
                }
            }
            else
            {
                if(!Vsnap->RetrieveVsnapInfo(vsnaps, "all"))
                {
                    DebugPrintf(SV_LOG_INFO, "Unable to retrieve the vsnap info from persistent store.\n");
                    rv = false;
                    break;
                }
            }

            if(!vsnaps.empty())
            {
                DebugPrintf(SV_LOG_INFO," Following is the list of virtual volumes mounted\n\n");
                std::vector<VsnapPersistInfo>::iterator iter = vsnaps.begin();
                int novols = 1;
                for(; iter != vsnaps.end(); ++iter)
                {
                    bool listflag = false;
                    std::string devName = iter->devicename;
                    if(m_bconfig && m_configurator->isSourceFullDevice(iter->target))
                        devName += "s2";
                    if(IsFileORVolumeExisting(iter->devicename))
                    {
                        listflag = true;
                    }
#ifdef SV_SUN
                    else if(IsFileORVolumeExisting(devName))
                    {
                        listflag = true;
                    }
#endif
                    if(listflag)
                    {
#ifdef SV_WINDOWS

                        DebugPrintf(SV_LOG_INFO, "%d) %s\n", novols,ToLower((*iter).devicename).c_str());
#else
                        std::string sMount = "";
                        std::string mode;
                        if(IsVolumeMounted((*iter).devicename,sMount,mode))
                        {
                            DebugPrintf(SV_LOG_INFO, "%d) %s\t%s\n", novols, (*iter).devicename.c_str(), (*iter).mountpoint.c_str());
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_INFO, "%d) %s\n", novols, (*iter).devicename.c_str());
                        }
#endif
                        novols++;
                        if(verbose)
                        {
                            std::ostringstream out;
                            Vsnap->PrintVsnapInfo(out, (*iter));
                            DebugPrintf(SV_LOG_INFO, "%s", out.str().c_str());
                        }
                    }
                }
            }
            else
            {
                if(!Vsnap->VsnapQuit())
                {
                    DebugPrintf(SV_LOG_INFO, "There are NO virtual volumes mounted\n");
                }

            }
            break;
        }

        VirtualVolInfo = (VsnapVirtualVolInfo *) new VsnapVirtualVolInfo;
        if(!VirtualVolInfo)
        {
            DebugPrintf(SV_LOG_ERROR, "Insufficient memory, retry later..\n\n");
            rv = false;
            break;
        }

        VirtualVolInfo->AccessMode = VSNAP_UNKNOWN_ACCESS_TYPE;
        VirtualVolInfo->AppName.clear();
        VirtualVolInfo->EventName.clear();
        VirtualVolInfo->EventNumber = 0;
        VirtualVolInfo->PrivateDataDirectory.clear();
        VirtualVolInfo->TimeToRecover = 0;
        VirtualVolInfo->VolumeName.clear();
        VirtualVolInfo->VsnapType = VSNAP_CREATION_UNKNOWN;
        VirtualVolInfo->SnapShotId = 0;
        VirtualVolInfo->PreScript.clear();
        VirtualVolInfo->PostScript.clear();
        VirtualVolInfo->ReuseDriveLetter = false;

        if(Unmountall)
        {
            bool deletemapfiles = true;
            bool bypassdriver = false;
            iter = nvpairs.find("softremoval");
            if( iter != nvpairs.end() )
            {
                Vsnap->notifyCx = false;
                deletemapfiles = false;
            }
            iter = nvpairs.find("bypassdriver");
            if( iter != nvpairs.end() )
            {
                bypassdriver = true;
            }

            std::vector<VsnapPersistInfo> passedvsnaps, failedvsnaps;
            Vsnap->UnmountAllVirtualVolumes(passedvsnaps, failedvsnaps, deletemapfiles,bypassdriver);

            //Added the following if else to print proper summary information after unmount all. Fix for bug #5569.

            //Bug #7934
            if(passedvsnaps.empty() && failedvsnaps.empty())
                DebugPrintf(SV_LOG_INFO, "There are NO virtual volumes mounted in the system\n");
            else
            {
                int seq = 1;
                std::vector<VsnapPersistInfo>::iterator iter = passedvsnaps.begin();
                for(; iter != passedvsnaps.end(); ++iter)
                {
                    DebugPrintf(SV_LOG_INFO, "%d) %s - Unmount Successfull\n", seq, iter->devicename.c_str());
                    seq++;
                }
                if(!failedvsnaps.empty())
                {
                    std::vector<VsnapPersistInfo>::iterator iter = failedvsnaps.begin();
                    for(; iter != failedvsnaps.end(); ++iter)
                    {
                        DebugPrintf(SV_LOG_INFO, "%d) %s - Unmount Failed\n", seq, iter->devicename.c_str());
                        seq++;
                    }
                    rv = false;
                }
            }		
            break;
        }

        //Get Virtual Volume
        iter = nvpairs.find("virtual");
        if(iter != nvpairs.end() && !(iter->second.empty()))
        {
            VirtualVolInfo->VolumeName =  iter ->second;
            size_t pos = 0;
            pos = VirtualVolInfo->VolumeName.find("=");      
            if ( pos <= VirtualVolInfo->VolumeName.length() )  
            {
                rv = false;
                usage(argv[0],m_operation);
                break;
            }
        }

        iter = nvpairs.find("flags");
        if( iter == nvpairs.end() )
        {
            VirtualVolInfo->AccessMode = VSNAP_READ_ONLY; //default;
        }			
        else if(Mount)
        {

            if( iter->second == "ro")
                VirtualVolInfo->AccessMode = VSNAP_READ_ONLY;
            else if(iter->second == "rw")
                VirtualVolInfo->AccessMode = VSNAP_READ_WRITE;
            else if(iter->second == "rwt")
                VirtualVolInfo->AccessMode = VSNAP_READ_WRITE_TRACKING;
            else
            {
                DebugPrintf(SV_LOG_INFO, "Vsnap Failed: Invalid --flags option specified for mount operation\n");
                rv = false;
                break;
            }
        } else if(Unmount)
        {
            if( iter->second == "nodelete")
                UnmountLogsDelete = false;
            else
            {
                DebugPrintf(SV_LOG_INFO, "Vsnap Failed: Invalid --flags option specified for unmount operation\n");
                rv = false;
                break;
            }
        }


        //if ( !Unmount && (Vsnap->IsConfigAvailable == false && !Mount))  // Ecase 2934 
        //{
        //    DebugPrintf(SV_LOG_INFO, "Cannot Locate CX Information , If you want to use force the unmount , USE --force option \n");
        //    usage(argv[0],m_operation);
        //    break;
        //}

        if ( Unmount )
        {

            VsnapVirtualInfoList UmountList;

            if (strcmp(VirtualVolInfo->VolumeName.c_str(),"")==0)  // Ecase No 2940
            {
                DebugPrintf(SV_LOG_INFO, "Vsnap Option is not parsed : Verify the Syntax \n");
                rv = false;
                usage(argv[0],m_operation);
                break;
            }


            //if ( (Vsnap->IsConfigAvailable == false ))  // Ecase 2934 
            //{
            //    iter = nvpairs.find("force"); // Ecase 2934
            //    if(iter == nvpairs.end())
            //    {
            //        DebugPrintf(SV_LOG_INFO, "Communication with Central Management Server is currently unavailable; use --force option if you want to still go ahead with the unmount operation\n");
            //        usage(argv[0],m_operation);
            //        break;
            //    }
            //}


            UmountList.push_back(VirtualVolInfo);
            Vsnap->Unmount(UmountList, UnmountLogsDelete);

            if(VirtualVolInfo->Error.VsnapErrorStatus != VSNAP_SUCCESS)
            {
                //Bug #7934
                if(Vsnap->VsnapQuit())
                {
                    DebugPrintf(SV_LOG_INFO, "Vsnap Unmount Operation Aborted as Quit Requested\n");
                }
                else
                {
                    Vsnap->VsnapGetErrorMsg(VirtualVolInfo->Error, errmsg);
                    DebugPrintf(SV_LOG_INFO, "%s\n",errmsg.c_str());
                }
                rv = false;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "%s Unmounted Successfully\n", VirtualVolInfo->VolumeName.c_str());
            }

            UmountList.clear();
            break;
        }
        else if(StartTrack)
        {	
            //Vsnap->StartTracking(VirtualVolInfo);

            if(VirtualVolInfo->Error.VsnapErrorStatus != VSNAP_SUCCESS)
            {
                Vsnap->VsnapGetErrorMsg(VirtualVolInfo->Error, errmsg);
                DebugPrintf(SV_LOG_INFO, "Start Tracking Failed: %s\n", errmsg.c_str());
                rv = false;
            }
            else
                DebugPrintf(SV_LOG_INFO, "Tracking enabled\n");
        }					
        else if (StopTrack)
        {

            //Vsnap->StopTracking(VirtualVolInfo);
            if(VirtualVolInfo->Error.VsnapErrorStatus != VSNAP_SUCCESS)
            {
                Vsnap->VsnapGetErrorMsg(VirtualVolInfo->Error, errmsg);
                DebugPrintf(SV_LOG_INFO, "Stop Tracking Failed: %s\n", errmsg.c_str());
                rv = false;
            }
            else
                DebugPrintf(SV_LOG_INFO, "Tracking Disabled\n");
        }
        else if (Sync)
        {

            Vsnap->Sync(VirtualVolInfo);
            if(VirtualVolInfo->Error.VsnapErrorStatus != VSNAP_SUCCESS)
            {
                Vsnap->VsnapGetErrorMsg(VirtualVolInfo->Error, errmsg);
                DebugPrintf(SV_LOG_INFO, "Sync Failed: %s\n", errmsg.c_str());
                rv = false;
            }
            else
                DebugPrintf(SV_LOG_INFO, "Syncing Done.\n");
        }

        SrcVolInfo = (VsnapSourceVolInfo *) new VsnapSourceVolInfo;
        if(!SrcVolInfo)
        {
            DebugPrintf(SV_LOG_ERROR, "Insufficient memory, retry later..\n\n");
            rv=false;
            break;
        }

        SrcVolInfo->VolumeName.clear();

        //Get Replicated volume
        iter = nvpairs.find("target");
        if(iter != nvpairs.end() && !(iter->second.empty()))
        {	
            SrcVolInfo->VolumeName = iter ->second;
#ifdef SV_UNIX
            string volume = SrcVolInfo->VolumeName;

            if (IsReportingRealNameToCx())
            {
                GetDeviceNameFromSymLink(SrcVolInfo->VolumeName);
            }
            else
            {
                std::string linkname = SrcVolInfo->VolumeName;
                if (!GetLinkNameIfRealDeviceName(SrcVolInfo->VolumeName, linkname))
                {
                    DebugPrintf(SV_LOG_ERROR, "Invalid target volume specified\n");
                    rv = false;
                    break;
                } 
                SrcVolInfo->VolumeName = linkname;
            }

            if(volume != SrcVolInfo->VolumeName)
                DebugPrintf(SV_LOG_INFO," %s is a symbolic link to %s \n\n",volume.c_str(),SrcVolInfo->VolumeName.c_str());
#endif

            if(strlen(SrcVolInfo->VolumeName.c_str())!=0)
                //SrcVolInfo->VolumeName=toupper(*(SrcVolInfo->VolumeName.c_str()));
                SrcVolInfo->VolumeName[0]=toupper(SrcVolInfo->VolumeName[0]);

        }

        iter = nvpairs.find("datalogpath");
        if(iter != nvpairs.end() && !(iter->second.empty()))
        {
            std::string dataLogPath = iter->second ;
#ifdef SV_WINDOWS			
            /*DataLogPath validation is done in resolveAndVerifypath. */
            if(dataLogPath.length() == 1) //If only letter(drive) is passed 
            {
                dataLogPath += std::string(":"); //Append colon to drive letter
            }
#endif

            /** Added by BSR - Issue#5288 **/
            SVERROR hr = SVMakeSureDirectoryPathExists( dataLogPath.c_str() ) ;
            if( hr.failed() )
            {
                DebugPrintf( SV_LOG_ERROR, "Invalid Data Log Path: %s. Path could not be created with error %s\n", 
                    dataLogPath.c_str(),
                    hr.toString() ) ;
                rv = false ;
                break ;
            }
            if( !resolvePathAndVerify( dataLogPath ) )
            {
                DebugPrintf( SV_LOG_ERROR, "Invalid Data Log Path: %s\n", (iter->second).c_str() ) ;
                rv = false;
                break;
            }
            /** End of the change **/
            VirtualVolInfo->PrivateDataDirectory = dataLogPath ; //iter->second;
        }

        iter = nvpairs.find("vsnapid");
        if(iter != nvpairs.end() && !(iter->second.empty())) {			
            try {
                VirtualVolInfo->SnapShotId = boost::lexical_cast<SV_ULONGLONG>(iter->second);
            } catch (boost::bad_lexical_cast& e) {
                rv = false;
                DebugPrintf(SV_LOG_WARNING, "CDPCli::vsnap lecial_cast failed for %s: %s\n", iter->second.c_str(), e.what());
                break;
            }
        }
        if(ApplyTrackLog)
        {

            SrcVolInfo->VirtualVolumeList.push_back(VirtualVolInfo);
            std::vector<VsnapPersistInfo> vsnaps;
            std::vector<VsnapPersistInfo>::iterator it = vsnaps.begin();
            if(!VirtualVolInfo->VolumeName.empty())
            {
                if((!Vsnap->RetrieveVsnapInfo(vsnaps, "this", "", VirtualVolInfo->VolumeName)) && (vsnaps.size() != 1))
                {
                    DebugPrintf(SV_LOG_INFO, "The given vsnap %s is not exist. So could not perform applytracklog.\n", VirtualVolInfo->VolumeName.c_str());
                    rv = false;
                    break;
                }
                it = vsnaps.begin();
                if(it == vsnaps.end())
                {
                    DebugPrintf(SV_LOG_INFO, "The given virtual snapshot %s does not exist. So could not perform applytracklog.\n", VirtualVolInfo->VolumeName.c_str());
                    rv = false;
                    break;
                }
                if(it->accesstype != VSNAP_READ_WRITE_TRACKING)
                {
                    DebugPrintf(SV_LOG_INFO, "The given virtual snapshot %s is not a rwt vsnap. So could not perform applytracklog.\n", VirtualVolInfo->VolumeName.c_str());
                    rv = false;
                    break;
                }
#ifdef SV_SUN
                if(m_bconfig && m_configurator->isSourceFullDevice(it->target))
                {
                    DebugPrintf(SV_LOG_INFO, "The applytracklog is not supported for the vsnaps whose replicated source is a full device.\n");
                    rv = false;
                    break;
                }
#endif
            }

            vsnaps.clear();
            if(!SrcVolInfo->VolumeName.empty())
            {
                if((!Vsnap->RetrieveVsnapInfo(vsnaps, "this", "", SrcVolInfo->VolumeName)) && (vsnaps.size() != 1))
                {
                    DebugPrintf(SV_LOG_INFO, "The given vsnap %s is not exist. So could not perform applytracklog.\n", SrcVolInfo->VolumeName.c_str());
                    rv = false;
                    break;
                }
            }
            it = vsnaps.begin();
            if((it != vsnaps.end()) && (it->accesstype == VSNAP_READ_ONLY))
            {
                DebugPrintf(SV_LOG_INFO, "The given target vsnap %s is a ro vsnap. So could not perform applytracklog.\n", SrcVolInfo->VolumeName.c_str());
                rv = false;
                break;
            }
#ifdef SV_SUN
            if((it != vsnaps.end()) && (m_bconfig) && (m_configurator->isSourceFullDevice(it->target)))
            {
                DebugPrintf(SV_LOG_INFO, "The applytracklog is not supported for the vsnaps whose replicated source is a full device.\n");
                rv = false;
                break;
            }
#endif
            if(Vsnap->ApplyTrackLogs(SrcVolInfo))
                DebugPrintf(SV_LOG_INFO, "Applying Track Logs Completed Successfully.\n");
            else
            {
                Vsnap->VsnapGetErrorMsg(VirtualVolInfo->Error, errmsg);
                DebugPrintf(SV_LOG_INFO, "%s\n", errmsg.c_str());
                DebugPrintf(SV_LOG_INFO, "Applying Track Logs Failed. \n");
                rv = false;
            }

            break;
        }
        if(DeleteLogs)
        {

            // Bug #5527 Checking if the datalogpath and vsnapid are provided prior to deleting the logs
            if(VirtualVolInfo->PrivateDataDirectory.empty() || (!VirtualVolInfo->SnapShotId))
            {
                VirtualVolInfo->Error.VsnapErrorCode = 0;
                VirtualVolInfo->Error.VsnapErrorStatus = VSNAP_DELETE_LOG_FAILED;
                Vsnap->VsnapGetErrorMsg(VirtualVolInfo->Error, errmsg);
                DebugPrintf(SV_LOG_INFO, "%s\n", errmsg.c_str());
                rv = false;
            }
            else
            {

                SrcVolInfo->VirtualVolumeList.push_back(VirtualVolInfo);

                Vsnap->DeleteLogs(SrcVolInfo);

                if(VirtualVolInfo->Error.VsnapErrorStatus != VSNAP_SUCCESS)
                {
                    Vsnap->VsnapGetErrorMsg(VirtualVolInfo->Error, errmsg);
                    DebugPrintf(SV_LOG_INFO, "%s\n", errmsg.c_str());
                    rv = false;
                }
                else
                    DebugPrintf(SV_LOG_INFO, "Completed Successfully\n");
            }
            break;
        }	
        //Get DB path
        iter = nvpairs.find("db");
        if(iter != nvpairs.end() && !(iter->second.empty()))
        {
            SrcVolInfo->RetentionDbPath = iter->second;
        }
        //get the source raw size
        iter = nvpairs.find("skipchecks");
        if(iter != nvpairs.end())
        {
            SrcVolInfo->skiptargetchecks = true;
            iter = nvpairs.find("source_raw_capacity");
            if(iter == nvpairs.end())
            {
                DebugPrintf(SV_LOG_INFO, "The source volume raw size is not given by the user.\n");
                rv = false;
                break;
            }
            SV_ULONGLONG size = 0;
            try {
                size = boost::lexical_cast<SV_ULONGLONG>(iter->second);
            }
            catch (boost::bad_lexical_cast& e)
            {
                DebugPrintf(SV_LOG_INFO, "CDPCli::vsnap lexical_cast failed for %s: %s\n",
                    iter->second.c_str(), e.what());
                rv = false;
                break;
            }
            SrcVolInfo->ActualSize = size;
            iter = nvpairs.find("source_uservisible_capacity");
            if(iter == nvpairs.end())
            {
                DebugPrintf(SV_LOG_INFO, "The source volume fs size is not given by the user.\n");
                rv = false;
                break;
            }
            size = 0;
            try {
                size = boost::lexical_cast<SV_ULONGLONG>(iter->second);
            }
            catch (boost::bad_lexical_cast& e)
            {
                DebugPrintf(SV_LOG_INFO, "CDPCli::vsnap lexical_cast failed for %s: %s\n",
                    iter->second.c_str(), e.what());
                rv = false;
                break;
            }
            SrcVolInfo->UserVisibleSize = size;
        }

        //Get TimeStamp using either --time or --at
        iter = nvpairs.find("time");


        NVPairs::iterator iter2 = nvpairs.find("at");

        if(iter != nvpairs.end() && iter2 != nvpairs.end())
        {
            DebugPrintf(SV_LOG_INFO, "Only one of the two options --time/--at is supported at a time.\n");
            rv = false;
            break;
        }

        if(iter == nvpairs.end())
            iter = iter2;

		

        if ( iter != nvpairs.end() )
        {
            std::string at = iter ->second;
            if(!CDPUtil::InputTimeToFileTime(iter ->second, VirtualVolInfo->TimeToRecover))
            {
                rv = false;
                break;
            }

            if(SrcVolInfo->RetentionDbPath.empty())
            {

                string dbpath;

                string VolName=SrcVolInfo->VolumeName;
                FormatVolumeNameForCxReporting(VolName);
                FirstCharToUpperForWindows(VolName);

                if(!getCdpDbName(VolName,dbpath))
                {
                    rv = false;
                    break;
                }

                if(dbpath == "")
                {
                    DebugPrintf(SV_LOG_ERROR, "Retention is not enabled for %s\n", VolName.c_str());
                    rv = false;
                    break;
                }
                SrcVolInfo->RetentionDbPath = dbpath;
            }

            bool isconsistent = false; // out parameter
			bool isavailable = false; // out parameter
            // returns true if the time is within recovery time range
            if(!CDPUtil::isCCP(SrcVolInfo->RetentionDbPath,VirtualVolInfo->TimeToRecover, isconsistent,isavailable))
            {
                rv = false;
                break;
            }

			if(!isavailable)
			{
				DebugPrintf(SV_LOG_INFO, "Note: The specified time %s is not a valid recovery point.\n", at.c_str());
				rv = false;
				break;
			}

            if(!isconsistent)
            {
                DebugPrintf(SV_LOG_INFO, "Note: The specified time %s is not a crash consistent point.\n", at.c_str());
            }
        }

        //Get application name
        iter = nvpairs.find("app");
        if( iter != nvpairs.end())
        {	
            VirtualVolInfo->AppName = iter ->second;
        }

        //Get Tag
        iter = nvpairs.find("event");
        if( iter != nvpairs.end())
        {
            VirtualVolInfo->EventName = iter -> second;
        }

        iter = nvpairs.find("eventnum");
        if( iter != nvpairs.end())
        {
            if(atoi(iter -> second.c_str())== 0)
            {
                DebugPrintf(SV_LOG_INFO, "Vsnap Failed: Unable to determine the event associated with the specified critera. \n");
                rv = false;
                break;
            }
            else
                VirtualVolInfo->EventNumber = atoi(iter -> second.c_str());
        }



        iter = nvpairs.find("prescript");
        if(iter != nvpairs.end() && !(iter->second.empty()))
        {
            VirtualVolInfo->PreScript = iter->second;
        }

        iter = nvpairs.find("postscript");
        if(iter != nvpairs.end() && !(iter->second.empty()))
        {
            VirtualVolInfo->PostScript = iter->second;
        }

        if(SrcVolInfo->VolumeName.empty())
        {
            DebugPrintf(SV_LOG_INFO,"Target volume is not specified\n");
            rv = false;
            break;
        }

        bool shouldRun = true;
        bool foundstate = ShouldOperationRun(SrcVolInfo->VolumeName,shouldRun);
        if((!shouldRun) && Mount)
        {
            DebugPrintf(SV_LOG_ERROR,
                "The retention logs for volume %s are being moved. Please try after sometime. \n",
                SrcVolInfo->VolumeName.c_str());
            rv = false;
            break;
        }

        if(Mount)
        {
#ifndef SV_WINDOWS

            if (!VirtualVolInfo->VolumeName.empty())
            {
                std::string strerr;
                if (!isvalidmountpoint(SrcVolInfo->VolumeName, strerr))
                {
                    DebugPrintf(SV_LOG_ERROR, "mountpoint option is not valid for target = %s, mountpoint = %s\n", 
                        SrcVolInfo->VolumeName.c_str(), VirtualVolInfo->VolumeName.c_str());
                    DebugPrintf(SV_LOG_ERROR, "The reason is: %s\n", strerr.c_str());
                    rv = false;
                    break;
                } 
            }

            // VSNAPFC - Persistence
            // Setting the VsnapVirtualVolInfo->DeviceName
            // to autogenerated name based on current time
            // which should be unique for every vsnap

            unsigned long long VsnapId = GetUniqueVsnapId();


            VirtualVolInfo->SnapShotId = VsnapId;


            if(!VsnapId)
            {
                DebugPrintf(SV_LOG_ERROR, "Error obtaining unique vsnapid\n");
                rv = false;
                break;
            }
            std::string t;
            t = boost::lexical_cast<std::string>(VsnapId);
            /**
            *
            * NEEDTOCHANGENAME : Need to change here the name.
            *
            */
            std::string prefix(VSNAP_DEVICE_PREFIX);
            prefix += t; 
#ifdef SV_SUN
            if(m_bconfig && m_configurator->isSourceFullDevice(SrcVolInfo->VolumeName))
            {
            }
            else
#endif
                prefix += VSNAP_SUFFIX;

            VirtualVolInfo->DeviceName = prefix;

#endif


            SrcVolInfo->VirtualVolumeList.push_back(VirtualVolInfo);
            if(Vsnap->IsConfigAvailable == false && SrcVolInfo->RetentionDbPath.empty() && VirtualVolInfo->PrivateDataDirectory.empty())
            {
                DebugPrintf(SV_LOG_INFO, "please enter the db path using --db option \n");
                rv = false;
                break;
            }

			bool TimeBased = false, EventBased = false;
			VSNAP_CREATION_TYPE VsnapType = VSNAP_POINT_IN_TIME;

			if(VirtualVolInfo->TimeToRecover != 0)
				TimeBased = true;

			if(!VirtualVolInfo->AppName.empty())
				EventBased = true;

			if(!VirtualVolInfo->EventName.empty())
				EventBased = true;

			if(VirtualVolInfo->EventNumber)
				EventBased = true;

			if(TimeBased || EventBased)
				VsnapType = VSNAP_RECOVERY;
			else
				VsnapType = VSNAP_POINT_IN_TIME;

			bool skip_replication_pause = SkipReplicationPause();

			if(isResyncing(SrcVolInfo ->VolumeName))
			{
				if(VsnapType == VSNAP_POINT_IN_TIME)
				{
					DebugPrintf(SV_LOG_INFO, 
						"%s is undergoing resync. Current point in time snapshot operation is not allowed\n", SrcVolInfo ->VolumeName.c_str());
					rv = false;
					break;
				} else if(!skip_replication_pause)
				{
					// in order to acquire the volume lock, pause the replication
					DebugPrintf(SV_LOG_INFO,
						"%s is undergoing resync. Sending a Pause Request for the replication to allow recovery operation\n",SrcVolInfo ->VolumeName.c_str());

					int hostType = 1 ; //TODO: use a macro
					string volumeName = SrcVolInfo ->VolumeName;
					FormatVolumeNameForCxReporting(volumeName);
					string ReasonForPause = "Replication is paused to allow snapshot/recovery operation";
					ReplicationPairOperations pairOp;

					if(pairOp.PauseReplication(m_configurator, volumeName, hostType, ReasonForPause.c_str(),0))
					{
						DebugPrintf(SV_LOG_INFO,"Replication paused for Volume: %s.\n",
							volumeName.c_str());
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to pause replication pair for %s.\n",
							volumeName.c_str());
						return false;
					}
				}
			}


            Vsnap->Mount(SrcVolInfo);

            if(VirtualVolInfo->Error.VsnapErrorStatus != VSNAP_SUCCESS)
            {
                //Bug# 7934
                if(Vsnap->VsnapQuit())
                {
                    DebugPrintf(SV_LOG_INFO, "Vsnap Operation Aborted as Quit Requested\n");
                }
                else
                {
                    Vsnap->VsnapGetErrorMsg(VirtualVolInfo->Error, errmsg);
                    DebugPrintf(SV_LOG_INFO, "Vsnap Failed: %s\n", errmsg.c_str());
                }
                rv = false;
            }

			if(isResyncing(SrcVolInfo ->VolumeName)  && (VsnapType != VSNAP_POINT_IN_TIME) && !skip_replication_pause)
			{
				// after finishing the recover operation, resume back the replication for
				// resync to progress

				DebugPrintf(SV_LOG_INFO,
					"Sending a Resume replication request for %s\n",SrcVolInfo ->VolumeName.c_str());

				int hostType = 1 ; //TODO: use a macro
				string volumeName = SrcVolInfo ->VolumeName;
				FormatVolumeNameForCxReporting(volumeName);
				string ReasoneForResume = "Replication is resumed after completion of snapshot/recovery operation";
				ReplicationPairOperations pairOp;

				if(pairOp.ResumeReplication(m_configurator, volumeName, hostType, ReasoneForResume.c_str()))
				{
					DebugPrintf(SV_LOG_INFO,"Replication resumed for Volume: %s.\n",
						volumeName.c_str());
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to resume replication pair for %s.\n",
						volumeName.c_str());
					return false;
				}
			}
			
            break;
        }

    } while ( FALSE );

    if(SrcVolInfo)
    {
        SrcVolInfo->VirtualVolumeList.clear();
        delete SrcVolInfo;
    }

    if(VirtualVolInfo)
        delete VirtualVolInfo;

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::upgradePersistentStore
*
* DESCRIPTION : Upgrades the vsnap persistent store to new format
*
*
* INPUT PARAMETERS : none
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*
*   Old format persistent store contains persistence file named 'TargetName__--__VsnapDeviceName'
*   (i.e '<Path to persistentstore>\TargetName__--__VsnapDeviceName')
*
*   This is changed with the long path changes because the TargetName itself can go upto 248 chars.
*
*   In the new format the same persistence file would be stored with VsnapDeviceName in the directory
*   with TargetName( i.e '<Path to persistentstore>\TargetName\VsnapDeviceName')
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::upgradePersistentStore()
{
    bool rv = true;
    bool brunning = true;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    do
    {
        if(!ReplicationAgentRunning(brunning))
        {
            DebugPrintf(SV_LOG_INFO, "Could not find VX agent status. Cannot continue.\n");
            rv = false;
            break;
        }

        if(brunning)
        {
            DebugPrintf(SV_LOG_INFO, "VX agent should be stopped before upgrading persistant store.\n");
            rv = false;
            break;
        }

#ifndef SV_WINDOWS
        if(!loadNewDriverAndRecreateVsnaps())
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to recreate vsnaps with new virtual volume driver\n");
            rv = false;
        }
#endif

        LocalConfigurator localConfigurator;
        std::string vsnapPersistentLocation =  localConfigurator.getVsnapConfigPathname();
        vsnapPersistentLocation += ACE_DIRECTORY_SEPARATOR_CHAR_A;

        ACE_DIR *dir = sv_opendir(vsnapPersistentLocation.c_str());
        if((dir == NULL))
            break;

        struct ACE_DIRENT *dEnt = NULL;
        while ((dEnt = ACE_OS::readdir(dir)) != NULL)
        {
            std::string filename = ACE_TEXT_ALWAYS_CHAR(dEnt->d_name);
            if ( filename == "." || filename == ".." )
                continue;

            std::string oldPersistentFile = vsnapPersistentLocation + filename;
            ACE_stat statBuff;
            if( (sv_stat(getLongPathName(oldPersistentFile.c_str()).c_str(), &statBuff) == 0) )
            {
                if( (statBuff.st_mode & S_IFDIR) == S_IFDIR ) 
                    continue;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Stat failed for %s, error no: %d\n",
                    oldPersistentFile.c_str(), ACE_OS::last_error());

                rv = false;
                //Ignore this error & continue with upgrading the persistent store
                //break;
                continue;
            }

            std::string parent, vsnapname;
            if(!VsnapUtil::parse_persistent_vsnap_filename(filename, parent, vsnapname))
                continue;

            FormatVolumeNameForCxReporting(parent);
            parent = VsnapUtil::construct_vsnap_persistent_name(parent);

            FormatVolumeNameForCxReporting(vsnapname);
            vsnapname = VsnapUtil::construct_vsnap_persistent_name(vsnapname);

            std::string newPersistentDir = vsnapPersistentLocation + parent;
            std::string newPersistentFile = newPersistentDir + ACE_DIRECTORY_SEPARATOR_CHAR_A + vsnapname;
            if(SVMakeSureDirectoryPathExists(newPersistentDir.c_str()).failed())
            {
                DebugPrintf(SV_LOG_ERROR, "Unable to create %s error no: %d\n",
                    newPersistentDir.c_str(), ACE_OS::last_error());
                rv = false;
                //Ignore this error & continue with upgrading the persistent store
                //break;
                continue;
            }

            if(ACE_OS::rename(getLongPathName(oldPersistentFile.c_str()).c_str(),
                getLongPathName(newPersistentFile.c_str()).c_str()) < 0)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to move persistent file %s to %s error no: %d\n",
                    oldPersistentFile.c_str(), newPersistentFile.c_str(), ACE_OS::last_error());
                rv = false;
                //Ignore this error & continue with upgrading the persistent store
                //break;
                continue;
            }
        }

        if(dir)
            ACE_OS::closedir(dir);
    }while(false);

    do
    {
        LocalConfigurator localConfigurator;
        std::string pendingVsnapPersistentLocation =  localConfigurator.getVsnapPendingPathname();
        pendingVsnapPersistentLocation += ACE_DIRECTORY_SEPARATOR_CHAR_A;

        ACE_DIR *dir = sv_opendir(pendingVsnapPersistentLocation.c_str());
        if((dir == NULL))
            break;

        struct ACE_DIRENT *dEnt = NULL;
        while ((dEnt = ACE_OS::readdir(dir)) != NULL)
        {
            std::string filename = ACE_TEXT_ALWAYS_CHAR(dEnt->d_name);
            if ( filename == "." || filename == ".." )
                continue;

            std::string oldPersistentFile = pendingVsnapPersistentLocation + filename;
            ACE_stat statBuff;
            if( (sv_stat(getLongPathName(oldPersistentFile.c_str()).c_str(), &statBuff) == 0) )
            {
                if( (statBuff.st_mode & S_IFDIR) == S_IFDIR ) 
                    continue;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Stat failed for %s, error no: %d\n",
                    oldPersistentFile.c_str(), ACE_OS::last_error());
                rv = false;
                //Ignore this error & continue with upgrading the persistent store
                //break;
                continue;
            }

            std::string parent, vsnapname;
            if(!VsnapUtil::parse_persistent_vsnap_filename(filename, parent, vsnapname))
                continue;

            FormatVolumeNameForCxReporting(parent);
            parent = VsnapUtil::construct_vsnap_persistent_name(parent);

            FormatVolumeNameForCxReporting(vsnapname);
            vsnapname = VsnapUtil::construct_vsnap_persistent_name(vsnapname);

            std::string newPersistentDir = pendingVsnapPersistentLocation + parent;
            std::string newPersistentFile = newPersistentDir + ACE_DIRECTORY_SEPARATOR_CHAR_A + vsnapname;
            if(SVMakeSureDirectoryPathExists(newPersistentDir.c_str()).failed())
            {
                DebugPrintf(SV_LOG_ERROR, "Unable to create %s error no: %d\n",
                    newPersistentDir.c_str(), ACE_OS::last_error());
                rv = false;
                //Ignore this error & continue with upgrading the persistent store
                //break;
                continue;
            }

            if(ACE_OS::rename(getLongPathName(oldPersistentFile.c_str()).c_str(),
                getLongPathName(newPersistentFile.c_str()).c_str()) < 0)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to move persistent file %s to %s error no: %d\n",
                    oldPersistentFile.c_str(), newPersistentFile.c_str(), ACE_OS::last_error());
                rv = false;
                //Ignore this error & continue with upgrading the persistent store
                //break;
                continue;
            }
        }

        if(dir)
            ACE_OS::closedir(dir);
    }while(false);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
* FUNCTION NAME :  CDPCli::virtualvolume
*
* DESCRIPTION : 
*
*
* INPUT PARAMETERS : none
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::virtualvolume()
{
    bool				rv = true;
    NVPairs::iterator	iter;
    NVPairs::iterator	iter1;
    string				filename,volume,filesize,VirVolList;
    char				*file;
    char				*vol;
    SV_ULONGLONG		size=0;

    VirVolume vVolume;
    // PR#10815: Long Path support
    file=new char[SV_MAX_PATH];
    vol=new char[SV_MAX_PATH];
    LocalConfigurator localConfigurator;

    VsnapMgr *Vsnap;

    do
    {
        bool MountVirtualVolume				= false;
        bool UnmountVirtualVolume			= false;
        bool SparseFile						= false;
        bool Unmountall						= false;
        bool DisplayList					= false;
        bool LinuxTempHelp					= false;
        bool UnmountallExceptTgt			= true;
        bool Resize							= false;
        bool ResizeSparseFile				= false;
        bool compressionenable				= false;
        bool compressiondisable				= false;
        bool Compress						= false;
        //Get the type of operation
        iter = nvpairs.find("op");
        if(iter != nvpairs.end())
        {
#ifdef SV_WINDOWS
            if( iter->second == "mount")
#else
            if( iter->second == "createvolume")
#endif
                MountVirtualVolume = true;
            else if( iter->second == "unmount")
                UnmountVirtualVolume = true;
            else if( iter->second== "unmountall")
                Unmountall=true;
            else if(iter->second=="createsparsefile")
                SparseFile=true;
            else if(iter->second=="list")
                DisplayList=true;
            else if(iter->second=="help")
                LinuxTempHelp=true;
            else if(iter->second=="resize")
                Resize=true;
            else if(iter->second=="resizesparsefile")
                ResizeSparseFile=true;
#ifdef SV_WINDOWS
            else if(iter->second=="enablecompression")
            {
                compressionenable = true;
                Compress = true;
            }
            else if(iter->second=="disablecompression")
            {
                compressiondisable = true;
                Compress = true;
            }
#endif
            else
            {
                DebugPrintf(SV_LOG_ERROR,"Unknown operation specified for --op option\n") ;
                usage(argv[0], m_operation);
                rv = false;
                break;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR,"virtualvolume Failed: It's mandatory to specify the operation type using --op \
                                     option. \n\n");
            usage(argv[0], m_operation);
            rv = false;
            break;
        }

        if(MountVirtualVolume)
        {
            iter = nvpairs.find("filepath");
            if(iter != nvpairs.end() && !(iter->second.empty()))
            {	
                filename=iter ->second;
                if((filename[0] == '\\') && (filename[1] == '\\'))
                    filename.erase(unique(filename.begin()+1, filename.end(), IsRepeatingSlash), filename.end());
                else
                    filename.erase(unique(filename.begin(), filename.end(), IsRepeatingSlash), filename.end());			
            }
            /* added condition to verify
            1. if volpack driver is enabled then we should not go ahead with mount operation for multiple sparse file
            2. if sparsefile does not exist then we should not go ahead with mount operation
            */
            std::string sparsefilename = filename ;
            if(vVolume.IsNewSparseFileFormat(filename.c_str()))
            {
                sparsefilename += SPARSE_PARTFILE_EXT;
                sparsefilename += "0";
                if(IsVolpackDriverAvailable())
                {
                    DebugPrintf(SV_LOG_ERROR, "The virtual volumes can't be mounted in the system. As the volpack driver available and the SparseFileMaxSize is not set to 0.\n");
                    break;
                }
            }
            ACE_stat s;
            if( sv_stat( getLongPathName(sparsefilename.c_str()).c_str(), &s ) < 0 )
            {
                DebugPrintf(SV_LOG_INFO,"The sparse file %s does not exist.\n",sparsefilename.c_str());
                break;
            }
            inm_strcpy_s(file, SV_MAX_PATH, filename.c_str());
            iter = nvpairs.find("drivename");
            if(iter != nvpairs.end() && !(iter->second.empty()))
            {	
                volume=iter ->second;
                volume.erase(unique(volume.begin(), volume.end(), IsRepeatingSlash), volume.end());
                FormatVolumeNameForCxReporting(volume);
#ifdef SV_WINDOWS
                std::transform(volume.begin(), volume.end(), volume.begin(), ::tolower);
#endif
                if((volume.size() > 3) && (IsVolpackDriverAvailable()))
                {
                    /* if the sparsefile name is same as mount point and the volpack driver available
                    in this case mount operation will failed as already a file is there in that name*/
                    if(stricmp(volume.c_str(),filename.c_str()) == 0)
                    {
                        DebugPrintf(SV_LOG_ERROR, "The virtual volumes can't be mounted in the system. As the sparse file name is same as drive name\n");
                        break;
                    }
                    if(SVMakeSureDirectoryPathExists(volume.c_str()).failed())
                    {
                        rv = false;
                        break;
                    }
                }
                FormatVolumeName(volume);
#ifdef SV_WINDOWS
				if(IsFileORVolumeExisting(volume.c_str()))
                {
                    DebugPrintf(SV_LOG_ERROR, "The specified Volume %s already exists\n",volume.c_str());
                    rv = false;
                    break;
                }
#endif
                inm_strcpy_s(vol, SV_MAX_PATH, volume.c_str());
            }

#ifdef SV_WINDOWS   
            if((strcmp(filename.c_str(),"")==0)||(strcmp(volume.c_str(),"")==0)	)
#else
            if(strcmp(filename.c_str(),"")==0)
#endif
            {
                usage(argv[0], m_operation);
                rv = false;
                break;
            }

            else
            {
                // TODO: DeletePersistVirtualVolumes name is misleading here
                // it is to check if the virtual volume by given name already exists
                // or a volume is already mounted for given sparse file
                // in such a case, it returns false (canProceed = false)
                // 
                // we need to revisit this piece of code
                //
                bool ifExists=vVolume.DeletePersistVirtualVolumes(volume,filename,true);
                if(ifExists)
                {
#ifdef SV_WINDOWS
                    USES_CONVERSION;
                    bool res = true;
                    if(IsVolpackDriverAvailable())
                        res=vVolume.mountsparsevolume(A2W(file),A2T(vol));
                    else
                        DebugPrintf(SV_LOG_INFO,"Mount Successful\n");
#else
                    bool res = true;
                    if(IsVolpackDriverAvailable())
                    {
                        string device="";
                        res=vVolume.mountsparsevolume(file,"",device);
                        volume=device;
                    }
                    else
                    {
                        volume=filename;
                        DebugPrintf(SV_LOG_INFO,"The devicefile %s has been created succesfully for %s \n",volume.c_str(),filename.c_str());
                    }
#endif
                    if(res==true)
                        PersistVirtualVolumes(filename,volume.c_str());
                    else
                        DebugPrintf(SV_LOG_ERROR, "The virtual volumes can't be mounted in the system\n");
                    break;
                }
            }

        }

        if(Compress)
        {
#ifdef SV_WINDOWS
            do
            {
                std::string drivename;
                bool filespecified = true;
                bool devicespecified = true;
                iter = nvpairs.find("filepath");
                if((iter == nvpairs.end()) || (iter->second.empty()))
                {
                    filespecified = false;
                }
                else
                {
                    filename = iter ->second;
                }
                iter = nvpairs.find("devicename");
                if((iter == nvpairs.end()) || (iter->second.empty()))
                {
                    devicespecified = false;
                }
                else
                {
                    volume=iter ->second;
                    CDPUtil::trim(volume);
                    FormatVolumeNameForCxReporting(volume);
                    std::transform(volume.begin(), volume.end(), volume.begin(), ::tolower);
                    if(volume.size() > 3)
                        drivename = volume;
                    else
                        drivename = volume + ":";

                    FormatVolumeName(volume);
                }
                if(devicespecified && filespecified)
                {
                    DebugPrintf(SV_LOG_ERROR, "Both filepath and device name are specified.\n");
                    usage(argv[0], m_operation);
                    rv = false;
                    break;
                }
                if(!devicespecified && !filespecified)
                {
                    usage(argv[0], m_operation);
                    rv = false;
                    break;
                }
                bool new_sparsefile_format = false;
                if(devicespecified)
                {
                    if(!IsVolPackDevice(volume.c_str(),filename,new_sparsefile_format))
                    {
                        DebugPrintf(SV_LOG_ERROR, "%s is not a virtual volume.\n",
                            volume.c_str());
                        rv = false;
                        break;
                    }

                    if(filename.empty())
                    {
                        DebugPrintf(SV_LOG_ERROR, "unable to fetch sparse file information for %s.\n",
                            volume.c_str());
                        rv = false;
                        break;
                    }
                }
                //enabling compression for each multi part file
                if(vVolume.IsNewSparseFileFormat(filename.c_str()))
                {
                    int i = 0;
                    std::stringstream sparsepartfile;
                    ACE_stat s;
                    while(true)
                    {
                        sparsepartfile.str("");
                        sparsepartfile << filename << SPARSE_PARTFILE_EXT << i;
                        if( sv_stat( getLongPathName(sparsepartfile.str().c_str()).c_str(), &s ) < 0 )
                        {
                            break;
                        }

                        if(compressionenable)
                            EnableCompressonOnFile(sparsepartfile.str() );
                        if(compressiondisable)
                            DisableCompressonOnFile(sparsepartfile.str());
                        i++;
                    }
                }
                else
                {
                    if(compressionenable)
                        EnableCompressonOnFile(filename);
                    if(compressiondisable)
                        DisableCompressonOnFile(filename);
                }
                if(devicespecified && IsVolpackDriverAvailable())
                {
                    if(compressionenable)
                        EnableCompressonOnDirectory(drivename);
                    if(compressiondisable)
                        DisableCompressonOnDirectory(drivename);
                }
            }while(false);
#endif
            break;
        }
        if(ResizeSparseFile)
        {
            do
            {
                iter = nvpairs.find("filepath");
                if((iter == nvpairs.end()) || (iter->second.empty()))
                {
                    usage(argv[0], m_operation);
                    rv = false;
                    break;
                }
                filename = iter ->second;

                InmSparseAttributeState_t  sparseattr_state = E_INM_ATTR_STATE_ENABLEIFAVAILABLE;
                iter = nvpairs.find("sparseattribute");
                if(iter == nvpairs.end())
                {
                    SV_UINT sparseattr_configvalue = localConfigurator.getVolpackSparseAttribute();
                    if(sparseattr_configvalue == E_INM_ATTR_STATE_ENABLED ||
                        sparseattr_configvalue == E_INM_ATTR_STATE_DISABLED ||
                        sparseattr_configvalue == E_INM_ATTR_STATE_ENABLEIFAVAILABLE)
                    {
                        sparseattr_state = static_cast <InmSparseAttributeState_t> (sparseattr_configvalue);
                    } else
                    {
                        DebugPrintf(SV_LOG_ERROR, 
                            "Invalid value for config parameter VolpackSparseAttribute in Vx configuration file.\n");
                        rv = false;
                        break;
                    }
                } else
                {
                    std::string sparseattr_state_param = iter ->second;
                    CDPUtil::trim(sparseattr_state_param);
                    if(!stricmp(sparseattr_state_param.c_str(),"yes"))
                        sparseattr_state = E_INM_ATTR_STATE_ENABLED;
                    else if(!stricmp(sparseattr_state_param.c_str(),"no"))
                        sparseattr_state = E_INM_ATTR_STATE_DISABLED;
                    else if(!stricmp(sparseattr_state_param.c_str(),"yesifavailable"))
                        sparseattr_state = E_INM_ATTR_STATE_ENABLEIFAVAILABLE;
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, 
                            "Invalid value provided for sparseattribute command line parameter.\n");
                        rv = false;
                        break;
                    }
                }

                iter = nvpairs.find("size");
                if((iter == nvpairs.end()) || (iter->second.empty()))
                {
                    usage(argv[0], m_operation);
                    rv = false;
                    break;
                }
                try {
                    size = boost::lexical_cast<SV_ULONGLONG>(iter->second);
                }
                catch (boost::bad_lexical_cast& e)
                {
                    DebugPrintf(SV_LOG_INFO, "CDPCli::virtualvolume lexical_cast failed for %s: %s\n",
                        iter->second.c_str(), e.what());
                    rv = false;
                    break;
                }

                iter = nvpairs.find("sizeinbytes");
                if(iter == nvpairs.end())
                {
                    size *= 1048576ULL;
                }

                bool result = vVolume.resizesparsefile(filename,size,sparseattr_state);
                if(!result)
                {
                    DebugPrintf(SV_LOG_ERROR, "Could not able to resize the sparse file %s.\n",
                        filename.c_str());
                    rv = false;
                }
            }while(false);
            break;
        }

        if(Resize)
        {
            do
            {
                std::string mountpoint;
                iter = nvpairs.find("devicename");
                if((iter == nvpairs.end()) || (iter->second.empty()))
                {
                    usage(argv[0], m_operation);
                    rv = false;
                    break;
                }
                volume=iter ->second;
                CDPUtil::trim(volume);
#ifdef SV_WINDOWS
                FormatVolumeNameForCxReporting(volume);
                std::transform(volume.begin(), volume.end(), volume.begin(), ::tolower);
                if(volume.size() > 3)
                    mountpoint = volume;
#endif

                InmSparseAttributeState_t  sparseattr_state = E_INM_ATTR_STATE_ENABLEIFAVAILABLE;
                iter = nvpairs.find("sparseattribute");
                if(iter == nvpairs.end())
                {
                    SV_UINT sparseattr_configvalue = localConfigurator.getVolpackSparseAttribute();
                    if(sparseattr_configvalue == E_INM_ATTR_STATE_ENABLED ||
                        sparseattr_configvalue == E_INM_ATTR_STATE_DISABLED ||
                        sparseattr_configvalue == E_INM_ATTR_STATE_ENABLEIFAVAILABLE)
                    {
                        sparseattr_state = static_cast <InmSparseAttributeState_t> (sparseattr_configvalue);
                    } else
                    {
                        DebugPrintf(SV_LOG_ERROR, 
                            "Invalid value for config parameter VolpackSparseAttribute in Vx configuration file.\n");
                        rv = false;
                        break;
                    }
                } else
                {
                    std::string sparseattr_state_param = iter ->second;
                    CDPUtil::trim(sparseattr_state_param);
                    if(!stricmp(sparseattr_state_param.c_str(),"yes"))
                        sparseattr_state = E_INM_ATTR_STATE_ENABLED;
                    else if(!stricmp(sparseattr_state_param.c_str(),"no"))
                        sparseattr_state = E_INM_ATTR_STATE_DISABLED;
                    else if(!stricmp(sparseattr_state_param.c_str(),"yesifavailable"))
                        sparseattr_state = E_INM_ATTR_STATE_ENABLEIFAVAILABLE;
                    else
                    {
                        DebugPrintf(SV_LOG_ERROR, 
                            "Invalid value provided for sparseattribute command line parameter.\n");
                        rv = false;
                        break;
                    }
                }

                iter = nvpairs.find("size");
                iter1 = nvpairs.find("sizeinbytes");
                if(((iter == nvpairs.end()) && (iter1 == nvpairs.end()))
                    || ((iter != nvpairs.end()) && (iter1 != nvpairs.end())))
                {
                    usage(argv[0], m_operation);
                    rv = false;
                    break;
                }
                std::string sizestr;
                bool sizeinmb = false;
                if(iter != nvpairs.end())
                {
                    sizestr = iter->second;
                    sizeinmb = true;
                }
                else
                {
                    sizestr = iter1->second;
                }

                try {
                    size = boost::lexical_cast<SV_ULONGLONG>(sizestr);
                    if(sizeinmb)
                        size *= 1048576ULL;
                }
                catch (boost::bad_lexical_cast& e)
                {
                    DebugPrintf(SV_LOG_INFO, "CDPCli::virtualvolume lexical_cast failed for %s: %s\n",
                        sizestr.c_str(), e.what());
                    rv = false;
                    break;
                }

                FormatVolumeName(volume);

                //get the sparsefile for the virtual volume
                std::string filepath;
                bool new_sparsefile_format = false;
                if(!IsVolPackDevice(volume.c_str(),filepath,new_sparsefile_format))
                {
                    DebugPrintf(SV_LOG_ERROR, "%s is not a virtual volume.\n",
                        volume.c_str());
                    rv = false;
                    break;
                }

                if(filepath.empty())
                {
                    DebugPrintf(SV_LOG_ERROR, "unable to fetch sparse file information for %s.\n",
                        volume.c_str());
                    rv = false;
                    break;
                }

                if(isTargetVolume(volume))
                {
                    SV_ULONGLONG volsize = 0;
                    ACE_stat s;
                    if(new_sparsefile_format)
                    {
                        int i = 0;
                        std::stringstream sparsepartfile;
                        while(true)
                        {
                            sparsepartfile.str("");
                            sparsepartfile << filepath << SPARSE_PARTFILE_EXT << i;
                            if( sv_stat( getLongPathName(sparsepartfile.str().c_str()).c_str(), &s ) < 0 )
                            {
                                break;
                            }
                            volsize += s.st_size;
                            i++;
                        }
                    }
                    else
                        volsize  = ( sv_stat( getLongPathName(filepath.c_str()).c_str(), &s ) < 0 ) ? 0 : s.st_size;
                    if(volsize >= (size * 1048576ULL))
                    {
                        DebugPrintf(SV_LOG_INFO, "Shrink of virtual target volume %s is not allowed.\n",
                            volume.c_str());
                        rv = false;
                        break;
                    }
                }
                CDPLock volumelock(volume);
                if(!volumelock.acquire())
                {
                    DebugPrintf(SV_LOG_INFO,
                        "Exclusive lock denied for %s. re-try again after stopping svagent service.\n",
                        volume.c_str());
                    rv = false;
                    break;
                }
                DebugPrintf(SV_LOG_INFO,"Note:  Volume <%s> will be dismounted for resize operation.\n", volume.c_str());
                DebugPrintf(SV_LOG_INFO,"       All opened handles to the volume would be invalid.\n");
                DebugPrintf(SV_LOG_INFO,"       All virtual snapshots to the volume will be unmounted.\n\n");
                //Unmounting all vsnap related to the virtual volume
                VsnapMgr *vsnap;
#ifdef SV_WINDOWS
                WinVsnapMgr wmgr;
                vsnap = &wmgr;
#else
                UnixVsnapMgr umgr;
                vsnap = &umgr;
#endif
                bool nofail = true;
                rv = vsnap->UnMountVsnapVolumesForTargetVolume(volume, nofail);
                if(!rv)
                {
                    DebugPrintf(SV_LOG_ERROR, "Some virtual snapshots are not unmounted for the volume %s\n",
                        volume.c_str());
                    DebugPrintf(SV_LOG_ERROR, "Virtual volume resize operation failed.\n");
                    break;
                }
                //unmont the virtual volume without deleting the sparse file
                bool res=true;
                if(IsVolpackDriverAvailable())
                    res=vVolume.Unmount(volume,false);

                if(!res)
                {
                    DebugPrintf(SV_LOG_ERROR, "unmount failed for %s.\n",
                        volume.c_str());
                    rv = false;
                    break;
                }

                //resize sparsefile
                bool result = vVolume.resizesparsefile(filepath,size,sparseattr_state);
                if(!result)
                {
                    DebugPrintf(SV_LOG_ERROR, "unable to resize the virtual volume %s.\n",volume.c_str());
                    DebugPrintf(SV_LOG_INFO, "Please retry after correcting the errors above.\n");
                    rv = false;
                }
                //mount the volume
                inm_strcpy_s(file, SV_MAX_PATH, filepath.c_str());
                inm_strcpy_s(vol, SV_MAX_PATH, volume.c_str());
#ifdef SV_WINDOWS
                /* Added this not to create directory if volpack driver is not there
                as directory creation will failed if the sparse file name and mount point are same*/

                if(!mountpoint.empty() && (IsVolpackDriverAvailable()))
                {
                    if(SVMakeSureDirectoryPathExists(volume.c_str()).failed())
                    {
                        rv = false;
                        break;
                    }
                }
                USES_CONVERSION;
                if(IsVolpackDriverAvailable())
                    res=vVolume.mountsparsevolume(A2W(file),A2T(vol));
#else
                if(IsVolpackDriverAvailable())
                {
                    string device="";
                    res=vVolume.mountsparsevolume(file,vol,device);
                }
                else
                {
                    DebugPrintf(SV_LOG_INFO,"The devicefile %s has been created succesfully for %s \n",volume.c_str(),filepath.c_str());
                }
#endif

                if(!res)
                {
                    DebugPrintf(SV_LOG_ERROR, "mount failed for %s.\n",
                        volume.c_str());
                    rv = false;
                    break;
                }

                if(rv)
                    DebugPrintf(SV_LOG_INFO, "Virtual volume resized successfully.\n");
            }while(false);
            break;
        }

        if(UnmountVirtualVolume)
        {
            bool deletesparsefile = true;
            bool deletepersistinfo = true;
            iter = nvpairs.find("deletesparsefile");
            if(iter != nvpairs.end() && !(iter->second.empty()))
            {
                if(stricmp(iter->second.c_str(),"no") == 0)
                {
                    deletepersistinfo = true;
                    deletesparsefile = false;
                }
            }
            iter = nvpairs.find("drivename");
            if(iter != nvpairs.end() && !(iter->second.empty()))
            {
                volume=iter ->second;
                CDPUtil::trim(volume);
#ifdef SV_WINDOWS
                FormatVolumeNameForCxReporting(volume);
                std::transform(volume.begin(), volume.end(), volume.begin(), ::tolower);
                FormatVolumeName(volume);
#endif
                std::string sparsefile;
                bool new_volpack_format = false;
                if(!IsVolPackDevice(volume.c_str(),sparsefile,new_volpack_format))
                {
                    DebugPrintf(SV_LOG_INFO,"The device %s is not a virtual volume. \n",volume.c_str());
                    rv = false;
                    break; 
                }

                bool res=true;
                if(IsVolpackDriverAvailable())
                    res=vVolume.Unmount(volume);
                else if(vVolume.IsVolpackUsedAsTarget(volume))
                {
                    DebugPrintf(SV_LOG_INFO,"The volume %s can not be unmounted as it is used as target volume\n", volume.c_str());
                    res = false;
                }

                if(res==true)
                {
                    rv = vVolume.DeletePersistVirtualVolumes(volume,"",false,deletepersistinfo,deletesparsefile);
                    if(rv && !IsVolpackDriverAvailable())
                        DebugPrintf(SV_LOG_INFO,"The volpack device %s has been deleted succesfully \n",volume.c_str());
                }
                else
                    rv = false;
            }
            else
            {
                usage(argv[0], m_operation);
                rv = false;
                break;
            }
            break;
        }
        if(SparseFile)
        {

            iter = nvpairs.find("filepath");
            if(iter != nvpairs.end() && !(iter->second.empty()))
            {
                filename=iter ->second;
                //Added the below condition to check the below case
                /* 1. if single sparse file created in a name then we should not allow to create 
                multisparse file also viseversa
                */
                if((filename[0] == '\\') && (filename[1] == '\\'))
                    filename.erase(unique(filename.begin()+1, filename.end(), IsRepeatingSlash), filename.end());
                else
                    filename.erase(unique(filename.begin(), filename.end(), IsRepeatingSlash), filename.end());

                if(filename[filename.size()-1] == ACE_DIRECTORY_SEPARATOR_CHAR_A)
                {
                    DebugPrintf( SV_LOG_INFO, "Invalid file name %s provided.\n",
                        filename.c_str()) ;
                    rv = false;
                    break;
                }

                ACE_stat s;
                if(sv_stat( getLongPathName(filename.c_str()).c_str(), &s ) == 0)
                {
                    DebugPrintf( SV_LOG_INFO, "Already a file or directory exist in name %s.\n",
                        filename.c_str()) ;
                    rv = false;
                    break;
                }

                std::string sparsefilename = filename + SPARSE_PARTFILE_EXT;
                sparsefilename += "0";
                if(sv_stat( getLongPathName(sparsefilename.c_str()).c_str(), &s ) == 0)
                {
                    DebugPrintf( SV_LOG_INFO, "Already sparse file %s exist.\n",
                        filename.c_str()) ;
                    rv = false;
                    break;
                }

                std::string dirpath = dirname_r(filename.c_str());
                SVERROR hr = SVMakeSureDirectoryPathExists( dirpath.c_str() ) ;
                if( hr.failed() )
                {
                    DebugPrintf( SV_LOG_ERROR, "Invalid path %s provided. directory creation failed with error %s\n",
                        filename.c_str(), hr.toString() ) ;
                    rv = false ;
                    break;
                }
            }

            InmSparseAttributeState_t  sparseattr_state = E_INM_ATTR_STATE_ENABLEIFAVAILABLE;
            iter = nvpairs.find("sparseattribute");
            if(iter == nvpairs.end())
            {
                SV_UINT sparseattr_configvalue = localConfigurator.getVolpackSparseAttribute();
                if(sparseattr_configvalue == E_INM_ATTR_STATE_ENABLED ||
                    sparseattr_configvalue == E_INM_ATTR_STATE_DISABLED ||
                    sparseattr_configvalue == E_INM_ATTR_STATE_ENABLEIFAVAILABLE)
                {
                    sparseattr_state = static_cast <InmSparseAttributeState_t> (sparseattr_configvalue);
                } else
                {
                    DebugPrintf(SV_LOG_ERROR, 
                        "Invalid value for config parameter VolpackSparseAttribute in Vx configuration file.\n");
                    rv = false;
                    break;
                }
            } else
            {
                std::string sparseattr_state_param = iter ->second;
                CDPUtil::trim(sparseattr_state_param);
                if(!stricmp(sparseattr_state_param.c_str(),"yes"))
                    sparseattr_state = E_INM_ATTR_STATE_ENABLED;
                else if(!stricmp(sparseattr_state_param.c_str(),"no"))
                    sparseattr_state = E_INM_ATTR_STATE_DISABLED;
                else if(!stricmp(sparseattr_state_param.c_str(),"yesifavailable"))
                    sparseattr_state = E_INM_ATTR_STATE_ENABLEIFAVAILABLE;
                else
                {
                    DebugPrintf(SV_LOG_ERROR, 
                        "Invalid value provided for sparseattribute command line parameter.\n");
                    rv = false;
                    break;
                }
            }

            iter = nvpairs.find("size");
            iter1 = nvpairs.find("sizeinbytes");
            if(((iter == nvpairs.end()) && (iter1 == nvpairs.end()))
                || ((iter != nvpairs.end()) && (iter1 != nvpairs.end())))
            {
                usage(argv[0], m_operation);
                rv = false;
                break;
            }
            std::string sizestr;
            bool sizeinmb = false;
            if(iter != nvpairs.end())
            {
                sizestr = iter->second;
                sizeinmb = true;
            }
            else
            {
                sizestr = iter1->second;
            }

            /* 
            To check if size is specified as negative
            We can cast the value to signed integer and then back to unsigned but we may lose some data.
            So used the following approach
            */
            CDPUtil::trim(sizestr);
            if(sizestr[0] == '-')
            {
                DebugPrintf(SV_LOG_INFO, "CDPCli::virtualvolume. Sparsefile size  cannot be negative.\n");
                rv = false;
                break;
            }

            try {
                size = boost::lexical_cast<SV_ULONGLONG>(sizestr);
                if(sizeinmb)
                    size *= 1048576ULL;
            }
            catch (boost::bad_lexical_cast& e)
            {
                DebugPrintf(SV_LOG_INFO, "CDPCli::virtualvolume lexical_cast failed for %s: %s\n",
                    sizestr.c_str(), e.what());
                rv = false;
                break;
            }

            if((strcmp(filename.c_str(),"")==0)||(size==0))
            {
                usage(argv[0], m_operation);
                rv = false;
                break;
            }
            else
            {
                if(!vVolume.createsparsefile(filename,size,sparseattr_state))
                {
                    vVolume.deletesparsefile((char *)filename.c_str());
                    rv = false;
                    break;
                }
#ifdef SV_WINDOWS
                bool compressflag = false;
                iter = nvpairs.find("compression");
                if(iter != nvpairs.end() && !(iter->second.empty()))
                {
                    if(stricmp(iter->second.c_str(),"yes") == 0)
                    {
                        compressflag = true;
                    }
                }
                else
                {
                    compressflag = localConfigurator.VirtualVolumeCompressionEnabled();
                }
                if(compressflag)
                {
                    if(vVolume.IsNewSparseFileFormat(filename.c_str() ))
                    {
                        ACE_stat s;
                        int i = 0;
                        std::stringstream sparsepartfile;
                        while(true)
                        {
                            sparsepartfile.str("");
                            sparsepartfile << filename << SPARSE_PARTFILE_EXT << i;
                            if( sv_stat( getLongPathName(sparsepartfile.str().c_str()).c_str(), &s ) < 0 )
                            {
                                break;
                            }
                            EnableCompressonOnFile(sparsepartfile.str());
                            i++;
                        }
                    }
                    else
                        EnableCompressonOnFile(filename);
                }
#endif
                break;
            }
        }
        if(DisplayList)
        {
            bool verbose = false;
            iter = nvpairs.find("verbose");
            if(iter != nvpairs.end())
            {
                verbose = true;
            }
            //            int mode = O_RDONLY;
            int counter = localConfigurator.getVirtualVolumesId();
            int novols = 1;
            while(counter != 0) {
                std::string  compressionenable = "OFF";
                char regData[26];
				inm_sprintf_s(regData, ARRAYSIZE(regData), "VirVolume%d", counter);

                std::string data = localConfigurator.getVirtualVolumesPath(regData);

                std::string sparsefilename;
                std::string volume;

                if (!ParseSparseFileNameAndVolPackName(data, sparsefilename, volume))
                {
                    counter--;
                    continue;
                }
                if(novols == 1)
                {
                    DebugPrintf(SV_LOG_INFO," Following is the list of virtual volumes mounted in the system\n\n");
                }
                FormatVolumeNameForCxReporting(volume);
                DebugPrintf(SV_LOG_INFO, "%d) %s\n", novols, volume.c_str());
                if(verbose)
                {
                    SV_ULONGLONG size = 0;
                    ACE_stat s;
                    bool vv_state = false;
                    //                    VirVolume virtualvolume;
                    if(vVolume.IsNewSparseFileFormat(sparsefilename.c_str() ))
                    {
                        int i = 0;
                        std::stringstream sparsepartfile;
                        while(true)
                        {
                            sparsepartfile.str("");
                            sparsepartfile << sparsefilename << SPARSE_PARTFILE_EXT << i;
                            if( sv_stat( getLongPathName(sparsepartfile.str().c_str()).c_str(), &s ) < 0 )
                            {
                                break;
                            }
                            size += s.st_size;
                            vv_state = true;
                            //added the below condition to determine whether compresion is enabled or not
#ifdef SV_WINDOWS
                            if(i==0)
                            {
                                WIN32_FIND_DATA data;
                                HANDLE h = FindFirstFile(sparsepartfile.str().c_str(),&data);
                                if (h != INVALID_HANDLE_VALUE)
                                {
                                    if(data.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED)
                                        compressionenable = "ON";
                                }
                                FindClose(h);
                            }
#endif
                            i++;
                        }
                    }
                    else
                    {
                        size  = ( sv_stat( getLongPathName(sparsefilename.c_str()).c_str(), &s ) < 0 ) ? 0 : s.st_size;
                        if(size)
                            vv_state = true;
#ifdef SV_WINDOWS
                        WIN32_FIND_DATA data;
                        HANDLE h = FindFirstFile(sparsefilename.c_str(),&data);
                        if (h != INVALID_HANDLE_VALUE)
                        {
                            if(data.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED)
                                compressionenable = "ON";
                        }
                        FindClose(h);
#endif
                    }

                    std::stringstream out;
                    out << "------------------------------------------------\n"
                        << " Virtual Volume : " << volume << "\n"
                        << " Sparse File    : " << sparsefilename << "\n"
                        << " Raw Size       : " << size << " bytes\n"
                        << " Compression    : " << compressionenable << "\n"
                        << " volpack state  : " << (vv_state ? "Active" : "InActive") << "\n";

                    DebugPrintf(SV_LOG_INFO, "%s\n",out.str().c_str());
                }
                novols++;
                counter--;
            }
            if(novols == 1)
            {
                DebugPrintf(SV_LOG_INFO, "There are no virtual volumes found in the system\n");
            }
            break;
        }
        if(Unmountall)
        {
            // Bug 3549 
            // if  --softremoval cdpcli switch is provided, then we pass false which results in
            // the  sparse file not getting  deleted and the virtualvolume entries
            // aren't removed from drscout.conf file

            bool bypassdriver = false;
            bool deletesparsefile = true;
            bool deletepersistinfo = true;
            iter = nvpairs.find("deletesparsefile");
            if(iter != nvpairs.end() && !(iter->second.empty()))
            {
                if(stricmp(iter->second.c_str(),"no") == 0)
                {
                    deletepersistinfo = true;
                    deletesparsefile = false;
                }
            }
            /*
            --checkfortarget=<yes|no>   : if specified as 'no', all virtual volumes will be deleted
            if specified as 'yes', all virtual volumes that are not being
            used as target volumes for replication will be deleted
            default value is yes
            */
            //"--checkfortarget=no" will only come from unistall script
            //As it is from uninstall script, so all the vsnap will be unmounted before calling virtual volume unmount
            iter = nvpairs.find("checkfortarget");
            if((iter != nvpairs.end()) && (stricmp(iter->second.c_str(),"no") == 0))
                UnmountallExceptTgt = false;

            //"--softremoval" will be called from shutdown script
            //As it is from uninstall script, so all the vsnap will be softremoved before doing unmountall virtual volume
            //incase of softremoval we need to softremoval the virtual target volume, so we have to made 
            // the flag UnmountallExceptTgt=false
            iter = nvpairs.find("softremoval");
            if(iter != nvpairs.end())
            {
                UnmountallExceptTgt = false;
                deletepersistinfo = false;
                deletesparsefile = false;
            }
            iter = nvpairs.find("bypassdriver");
            if(iter != nvpairs.end())
            {
                bypassdriver = true;
            }

            rv = vVolume.UnmountAllVirtualVolumes( UnmountallExceptTgt,deletepersistinfo,deletesparsefile,bypassdriver);   

            break;
        }

    } while ( FALSE );

    delete []file;
    delete []vol;
    return rv;
}

/*
* FUNCTION NAME :  CDPCli::hide
*
* DESCRIPTION : 
*
*
* INPUT PARAMETERS : none
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::hide()
{
    bool rv = false;
    bool force = false;
    bool brunning = false;
    bool sourcevolume = false;
    NVPairs::iterator iter;

    do
    {
        if(argc < 3 || argc > 4)
        {
            usage(argv[0], m_operation);
            rv = false;
            break;
        }

        iter = nvpairs.find("force");
        if(iter != nvpairs.end())
        {
            force = true;
        }

        iter = nvpairs.find("sourcevolume");
        if(iter != nvpairs.end())
        {
            sourcevolume = true;
        }

        string formattedVolumeName = argv[2];
        string volumename = argv[2];
        CDPUtil::trim(volumename);
        CDPUtil::trim(formattedVolumeName);

        FormatVolumeName(formattedVolumeName);

        if (IsReportingRealNameToCx())
        {
            GetDeviceNameFromSymLink(formattedVolumeName);
        }
        else
        {
            std::string linkname = formattedVolumeName;
            if (!GetLinkNameIfRealDeviceName(formattedVolumeName, linkname))
            {
                DebugPrintf(SV_LOG_ERROR, "@LINE %d in FILE %s Invalid volume specified\n", LINE_NO, FILE_NAME);
                rv = false;
                break;
            } 
            formattedVolumeName = linkname;
        }

        if(formattedVolumeName != volumename)
            DebugPrintf(SV_LOG_INFO," %s is a symbolic link to %s \n\n",
            formattedVolumeName.c_str(),volumename.c_str());

        LocalConfigurator localConfigurator;
        std::string sparsefile;
        bool new_sparsefile_format = false;
        bool is_volpack = IsVolPackDevice(formattedVolumeName.c_str(),sparsefile,new_sparsefile_format);
        if(!IsVolpackDriverAvailable() && is_volpack)
        {
            DebugPrintf(SV_LOG_INFO,"Note: Specified device %s is a virtual volume. Hide operation is not applicable for virtual volumes.\n",formattedVolumeName.c_str());
            rv = true;
            break;
        }
        if (!IsFileORVolumeExisting(formattedVolumeName))
        {
            DebugPrintf(SV_LOG_INFO, "%s%s%s", "Volume: " , volumename.c_str() , " doesn't exist\n\n");
            rv = false;
            break;
        }

        if(!IsValidDevfile(formattedVolumeName))
        {
            DebugPrintf(SV_LOG_INFO, "Volume %s is not a valid device.\n", volumename.c_str());
            rv = false;
            break;
        }

        std::string mountpoints;
        if(containsMountedVolumes(formattedVolumeName,mountpoints))
        {
            DebugPrintf(SV_LOG_INFO, "%s contains mounted volumes.\n The volume remains unhidden.\n", volumename.c_str());
            rv = false;
            break;
        }
#ifdef SV_WINDOWS
        VOLUME_STATE vs = GetVolumeState(formattedVolumeName.c_str());
        if(vs == VOLUME_HIDDEN)
        {
            DebugPrintf(SV_LOG_INFO, "Volume %s is already in locked (hidden) state.\n", volumename.c_str());
            rv = true;
            break;
        }
#endif

        if(containsRetentionFiles(formattedVolumeName))
        {
            DebugPrintf(SV_LOG_INFO, "Volume %s cannot be hidden (locked).\n",volumename.c_str());
            DebugPrintf(SV_LOG_INFO, "It contains retention files.\n");
            rv = false;
            break;
        }

        if(!sourcevolume && isProtectedVolume(formattedVolumeName))
        {
            DebugPrintf(SV_LOG_INFO, "Volume %s cannot be hidden (locked).\n",volumename.c_str());
            DebugPrintf(SV_LOG_INFO, "It is a protected (source) volume.\n");
            rv = false;
            break;
        }

        if(!isTargetVolume(formattedVolumeName))
        {
            rv= hide_cxdown(formattedVolumeName);
            break;
        }

        if(isResyncing(formattedVolumeName))
        {
            DebugPrintf(SV_LOG_ERROR, "Volume %s is in resync state.\n", volumename.c_str());
            DisplayNote3();
            rv = false;
            break;
        }

		bool shouldrun = false;
		ShouldOperationRun(formattedVolumeName,shouldrun);
		if(!shouldrun)
		{
			stringstream out;
			out << "Note:\n"

				<< "  While Replication status is in flush and hold states, hide or unhide operations on\n"
				<< "  target volume should not be performed.\n\n";

			DebugPrintf(SV_LOG_INFO,"%s", out.str().c_str());
			rv = false;
			break;
		}

        if(!ReplicationAgentRunning(brunning))
        {
            rv = false;
            break;
        }

        if(!brunning)
        {
            rv= hide_cxdown(formattedVolumeName);
            break;
        }

        if(SendRequestToCx(formattedVolumeName,VOLUME_HIDDEN))
        {
            rv = PollForRequestCompletion(formattedVolumeName, VOLUME_HIDDEN);
            break;
        }else
        {
            if(force)
            {
                DebugPrintf(SV_LOG_INFO, "force option specified ... performing hide operation without communicating to Central Management Server.\n");
                rv = hide_cxdown(formattedVolumeName);
                break;
            }else
            {
                DebugPrintf(SV_LOG_INFO, 
                    "Please use --force option if you want to still proceed with the operation.\n");
                rv = false;
                break;
            }
        }

    } while (0);

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::unhidero
*
* DESCRIPTION : 
*
*
* INPUT PARAMETERS : none
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::unhidero()
{
    bool rv = false;
    bool force = false;
    bool brunning = false;
    NVPairs::iterator iter;

    do
    {

        if(argc < 3)
        {
            usage(argv[0], m_operation);
            rv = false;
            break;
        }

        iter = nvpairs.find("force");
        if(iter != nvpairs.end())
        {
            force = true;
        }

        string formattedVolumeName = argv[2];
        string volumename = argv[2];
        CDPUtil::trim(volumename);
        CDPUtil::trim(formattedVolumeName);

        FormatVolumeName(formattedVolumeName);

        if (IsReportingRealNameToCx())
        {
            GetDeviceNameFromSymLink(formattedVolumeName);
        }
        else
        {
            std::string linkname = formattedVolumeName;
            if (!GetLinkNameIfRealDeviceName(formattedVolumeName, linkname))
            {
                DebugPrintf(SV_LOG_ERROR, "@LINE %d in FILE %s Invalid volume specified\n", LINE_NO, FILE_NAME);
                rv = false;
                break;
            } 
            formattedVolumeName = linkname;
        }

        if(formattedVolumeName != volumename)
            DebugPrintf(SV_LOG_INFO," %s is a symbolic link to %s \n\n",
            formattedVolumeName.c_str(),volumename.c_str());

        string mountPoint = "";
        string fsType = "";

        LocalConfigurator localConfigurator;
        std::string sparsefile;
        bool new_sparsefile_format = false;
        bool is_volpack = IsVolPackDevice(formattedVolumeName.c_str(),sparsefile,new_sparsefile_format);
        if(!IsVolpackDriverAvailable() && is_volpack)
        {
            DebugPrintf(SV_LOG_INFO,"Note: Specified device %s is a virtual volume. Unhide operation is not applicable for virtual volumes.\n",formattedVolumeName.c_str());
            rv = true;
            break;
        }
        if (!IsFileORVolumeExisting(formattedVolumeName))
        {
            DebugPrintf(SV_LOG_INFO, "%s%s%s", "Volume: " , volumename.c_str() , " doesn't exist\n\n");
            rv = false;
            break;
        }

#ifdef SV_WINDOWS
        mountPoint = formattedVolumeName;
#else
        iter = nvpairs.find("mountpoint");
        if( iter == nvpairs.end()  )
        {
            DebugPrintf(SV_LOG_INFO, "Please specify the mount point.\n");
            usage(argv[0], m_operation);
            rv = false;
            break;
        }
        else
        {
            mountPoint = iter ->second ;
        }

        if(!IsAbsolutePath(mountPoint))
        {
            DebugPrintf(SV_LOG_INFO,"Specified mount point %s is not an absolute path. \n",
                mountPoint.c_str());
            DebugPrintf(SV_LOG_INFO,"Please specify correct mount point.\n");
            rv = false ;
            break ;
        }

        std::string strerr;
        if (!IsValidMountPoint(mountPoint, strerr))
        {
            DebugPrintf(SV_LOG_ERROR, "Specified mountpoint %s is not a valid mountpoint.\n",
                mountPoint.c_str());
            DebugPrintf(SV_LOG_ERROR, "The reason is: %s\n", strerr.c_str());
            rv = false;
            break;
        }

        if(SVMakeSureDirectoryPathExists( mountPoint.c_str()).failed())
        {
            DebugPrintf(SV_LOG_INFO,"Specified mount point %s could not be created. \n",
                mountPoint.c_str());
            DebugPrintf(SV_LOG_INFO,"Please specify correct mount point.\n");
            rv = false ;
            break ;
        }

        iter = nvpairs.find("filesystem");
        if( iter != nvpairs.end()  )
        {
            fsType = iter ->second ;
        }else
        {
            fsType = getFSType(formattedVolumeName);
        }

        if(fsType == "" )
        {	
            DebugPrintf(SV_LOG_INFO, 
                " filesystem is not specified; mount will attempt to detect filesystem type\n",
                volumename.c_str());
        }

        const std::string ZFS_NAME = ZFS;
        if (ZFS_NAME == fsType)
        {
            DebugPrintf(SV_LOG_INFO, 
                " %s will not be mounted as filesystem type is zfs.\n",
                volumename.c_str());
            rv = false;
            break;
        } 

        if(!stricmp(fsType.c_str(),"ntfs"))
        {
            DebugPrintf(SV_LOG_INFO, 
                " %s will not be mounted as filesystem type is ntfs.\n",
                volumename.c_str());
            rv = false;
            break;
        }

        DebugPrintf(SV_LOG_INFO, "VolumeName: %s\n", volumename.c_str());
        DebugPrintf(SV_LOG_INFO, "FileSystem Type: %s\n", fsType.c_str());
        DebugPrintf(SV_LOG_INFO, "MountPoint: %s\n", mountPoint.c_str());

#endif

        VOLUME_STATE vs = GetVolumeState(formattedVolumeName.c_str());
        if(vs == VOLUME_VISIBLE_RO)
        {
            DebugPrintf(SV_LOG_INFO, "Volume %s is already unhidden in read-only mode.\n", volumename.c_str());
            rv = true;
            break;
        }

        if(isProtectedVolume(formattedVolumeName))
        {
            DebugPrintf(SV_LOG_INFO, "Volume %s cannot be unhidden in read-only mode.\n",volumename.c_str());
            DebugPrintf(SV_LOG_INFO, "It is a protected (source) volume.\n");
            rv = false;
            break;
        }

        if(!isTargetVolume(formattedVolumeName))
        {
            rv = unhidero_cxdown(formattedVolumeName, mountPoint, fsType);
            break;
        }
#ifndef SV_WINDOWS
        else
        {
            std::string fstype_source;
            fstype_source = getFSType(formattedVolumeName);
            if(fstype_source.empty())
            {
                DebugPrintf(SV_LOG_INFO,"The target device %s is a raw volume. Unhide operation is not applicable for raw volumes.\n",
                    formattedVolumeName.c_str());
                rv = false;
                break;
            }
        }
#endif

        if(isResyncing(formattedVolumeName))
        {
            DebugPrintf(SV_LOG_ERROR, "Volume %s is in resync state.\n", volumename.c_str());
            DisplayNote3();
            rv = false ;
            break ;
        }

		bool shouldrun = false;
		ShouldOperationRun(formattedVolumeName,shouldrun);
		if(!shouldrun)
		{
			stringstream out;
			out << "Note:\n"

				<< "  While Replication status is in flush and hold states, hide or unhide operations on\n"
				<< "  target volume should not be performed.\n\n";

			DebugPrintf(SV_LOG_INFO,"%s", out.str().c_str());
			rv = false;
			break;
		}



        if(!ReplicationAgentRunning(brunning))
        {
            rv = false ;
            break ;
        }


        if(!brunning)
        {
            rv = unhidero_cxdown(formattedVolumeName, mountPoint, fsType);
            break;
        }

        if(SendRequestToCx(formattedVolumeName, VOLUME_VISIBLE_RO, mountPoint))
        {
            rv = PollForRequestCompletion(formattedVolumeName, VOLUME_VISIBLE_RO);
            break;
        }else
        {
            if(force)
            {
                DebugPrintf(SV_LOG_INFO, "force option specified ... performing hide operation without communicating to Central Management Server.\n");
                rv = unhidero_cxdown(formattedVolumeName, mountPoint, fsType);
                break;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, 
                    "Please use --force option if you want to still proceed with the operation.\n");
                rv = false;
                break;
            }
        }

    } while (0);

    return rv;
}


/*
* FUNCTION NAME :  CDPCli::unhiderw
*
* DESCRIPTION : 
*
*
* INPUT PARAMETERS : none
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::unhiderw()
{
    bool rv = false;
    bool force = false;
    bool brunning = false;
    NVPairs::iterator iter;

    do
    {

        if(argc < 3)
        {
            usage(argv[0], m_operation);
            rv = false;
            break;
        }

        iter = nvpairs.find("force");
        if(iter != nvpairs.end())
        {
            force = true;
        }

        string formattedVolumeName = argv[2];
        string volumename = argv[2];
        CDPUtil::trim(volumename);
        CDPUtil::trim(formattedVolumeName);

        FormatVolumeName(formattedVolumeName);

        if (IsReportingRealNameToCx())
        {
            GetDeviceNameFromSymLink(formattedVolumeName);
        }
        else
        {
            std::string linkname = formattedVolumeName;
            if (!GetLinkNameIfRealDeviceName(formattedVolumeName, linkname))
            {
                DebugPrintf(SV_LOG_ERROR, "@LINE %d in FILE %s Invalid volume specified\n", LINE_NO, FILE_NAME);
                rv = false;
                break;
            } 
            formattedVolumeName = linkname;
        }

        if(formattedVolumeName != volumename)
            DebugPrintf(SV_LOG_INFO," %s is a symbolic link to %s \n\n",
            formattedVolumeName.c_str(),volumename.c_str());

        string mountPoint = "";
        string fsType = "";

        LocalConfigurator localConfigurator;
        std::string sparsefile;
        bool new_sparsefile_format = false;
        bool is_volpack = IsVolPackDevice(formattedVolumeName.c_str(),sparsefile,new_sparsefile_format);
        if(!IsVolpackDriverAvailable() && is_volpack)
        {
            DebugPrintf(SV_LOG_INFO,"Note: Specified device %s is a virtual volume. Unhide operation is not applicable for virtual volumes.\n",formattedVolumeName.c_str());
            rv = true;
            break;
        }
        if (!IsFileORVolumeExisting(formattedVolumeName))
        {
            DebugPrintf(SV_LOG_INFO, "%s%s%s", "Volume: " , volumename.c_str() , " doesn't exist\n\n");
            rv = false;
            break;
        }

#ifdef SV_WINDOWS
        mountPoint = formattedVolumeName;
#else
        iter = nvpairs.find("mountpoint");
        if( iter == nvpairs.end()  )
        {
            DebugPrintf(SV_LOG_INFO, "Please specify the mount point.\n");
            usage(argv[0], m_operation);
            rv = false;
            break;
        }
        else
        {
            mountPoint = iter ->second ;
        }

        if(!IsAbsolutePath(mountPoint))
        {
            DebugPrintf(SV_LOG_INFO,"Specified mount point %s is not an absolute path. \n",
                mountPoint.c_str());
            DebugPrintf(SV_LOG_INFO,"Please specify correct mount point.\n");
            rv = false;
            break;
        }

        std::string strerr;
        if (!IsValidMountPoint(mountPoint, strerr))
        {
            DebugPrintf(SV_LOG_ERROR, "Specified mountpoint %s is not a valid mountpoint.\n",
                mountPoint.c_str());
            DebugPrintf(SV_LOG_ERROR, "The reason is: %s\n", strerr.c_str());
            rv = false;
            break;
        }

        if(SVMakeSureDirectoryPathExists( mountPoint.c_str()).failed())
        {
            DebugPrintf(SV_LOG_INFO,"Specified mount point %s could not be created. \n",
                mountPoint.c_str());
            DebugPrintf(SV_LOG_INFO,"Please specify correct mount point.\n");
            rv = false;
            break;
        }

        iter = nvpairs.find("filesystem");
        if( iter != nvpairs.end()  )
        {
            fsType = iter ->second ;
        }else
        {
            fsType = getFSType(formattedVolumeName);
        }

        if(fsType == "" )
        {
            DebugPrintf(SV_LOG_INFO, 
                "filesystem is not specified; mount will attempt to detect the filesystem type\n",
                volumename.c_str());
        }

        const std::string ZFS_NAME = ZFS;
        if (ZFS_NAME == fsType)
        {
            DebugPrintf(SV_LOG_INFO, 
                " %s will not be mounted as filesystem type is zfs.\n",
                volumename.c_str());
            rv = false;
            break;
        } 

        if(!stricmp(fsType.c_str(),"ntfs"))
        {
            DebugPrintf(SV_LOG_INFO, 
                " %s will not be mounted as filesystem type is ntfs.\n",
                volumename.c_str());
            rv = false;
            break;
        }

        DebugPrintf(SV_LOG_INFO, "VolumeName: %s\n", volumename.c_str());
        DebugPrintf(SV_LOG_INFO, "FileSystem Type: %s\n", fsType.c_str());
        DebugPrintf(SV_LOG_INFO, "MountPoint: %s\n", mountPoint.c_str());

#endif

        VOLUME_STATE vs = GetVolumeState(formattedVolumeName.c_str());
        if(vs == VOLUME_VISIBLE_RW)
        {
            DebugPrintf(SV_LOG_INFO, "Volume %s is already unhidden in read-write mode.\n", volumename.c_str());
            rv = true;
            break;
        }

        if(!isTargetVolume(formattedVolumeName))
        {
            rv = unhiderw_cxdown(formattedVolumeName, mountPoint, fsType);
            break;
        }
#ifndef SV_WINDOWS
        else
        {
            std::string fstype_source;
            fstype_source = getFSType(formattedVolumeName);
            if(fstype_source.empty())
            {
                DebugPrintf(SV_LOG_INFO,"The target device %s is a raw volume. Unhide operation is not applicable for raw volumes.\n",
                    formattedVolumeName.c_str());
                rv = false;
                break;
            }
        }
#endif

        if(isResyncing(formattedVolumeName))
        {
            DebugPrintf(SV_LOG_ERROR, "Volume %s is in resync state.\n", volumename.c_str());
            DisplayNote3();
            rv = false;
            break;
        }

		bool shouldrun = false;
		ShouldOperationRun(formattedVolumeName,shouldrun);
		if(!shouldrun)
		{
			stringstream out;
			out << "Note:\n"

				<< "  While Replication status is in flush and hold states, hide or unhide operations on\n"
				<< "  target volume should not be performed.\n\n";

			DebugPrintf(SV_LOG_INFO,"%s", out.str().c_str());
			rv = false;
			break;
		}

        if(!ReplicationAgentRunning(brunning))
        {
            rv = false;
            break;
        }

        if(!brunning)
        {
            rv = unhiderw_cxdown(formattedVolumeName, mountPoint, fsType);
            break;
        }

        if(SendRequestToCx(formattedVolumeName, VOLUME_VISIBLE_RW, mountPoint))
        {
            rv = PollForRequestCompletion(formattedVolumeName, VOLUME_VISIBLE_RW);
            break;
        }else
        {
            if(force)
            {
                DebugPrintf(SV_LOG_INFO, "force option specified ... performing hide operation without communicating to Central Management Server.\n");
                rv = unhiderw_cxdown(formattedVolumeName, mountPoint, fsType);
                break;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, 
                    "Please use --force option if you want to still proceed with the operation.\n");
                rv = false;
                break;
            }
        }

    } while (0);

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::hide_cxdown
*
* DESCRIPTION : 
*
*
* INPUT PARAMETERS : none
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::hide_cxdown(const std::string & volumename)
{
    bool rv = true;

    CDPLock::Ptr cdplock(new CDPLock(volumename));
    if(!cdplock ->acquire())
    {
        DebugPrintf(SV_LOG_INFO,
            "Exclusive lock denied for %s. re-try again after stopping svagent service.\n",
            volumename.c_str());
        return false;
    }

    std::string output, error;
    SVERROR sve = HideDrive(volumename.c_str(),volumename.c_str(), output, error);
    if ( sve.succeeded()  )
    {
        DebugPrintf(SV_LOG_INFO, "%s is now hidden\n\n", volumename.c_str());
        rv = true;
    }
    else if ( sve == SVS_FALSE) 
    {
        DebugPrintf(SV_LOG_ERROR, 
            "Cannot hide %s .It is not NTFS or FAT FileSystem \n\n", volumename.c_str());
        rv = false;
    }
    else
    {
        //Bug 4147: This check is added not to display the same failure message again
        if((error.find("CHK_DIR:") == 0))
        {
            error = ".";
        }
        //Bug# 7934
        if(CDPUtil::QuitRequested())
        {
            DebugPrintf(SV_LOG_ERROR, "Hide Aborted as Quit Requested\n");
            DebugPrintf(SV_LOG_ERROR, "Failed to hide %s %s\n\n", volumename.c_str(), error.c_str());
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to hide %s %s\n\n", volumename.c_str(), error.c_str());
        }
        rv = true;
    }

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::unhidero_cxdown
*
* DESCRIPTION : 
*
*
* INPUT PARAMETERS : none
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::unhidero_cxdown(const std::string & volumename, const std::string & mountpt, 
                             const std::string & fstype)
{		
    bool rv = true;

    CDPLock::Ptr cdplock(new CDPLock(volumename));
    if(!cdplock ->acquire())
    {
        DebugPrintf(SV_LOG_INFO,
            "Exclusive lock denied for %s. re-try again after stopping svagent service.\n",
            volumename.c_str());
        return false;
    }

    do
    {	

#ifdef SV_WINDOWS
        SVERROR sve = UnhideDrive_RO(volumename.c_str(),volumename.c_str());
        if ( sve.succeeded() )
        {
            DebugPrintf(SV_LOG_INFO, "%s is now accessible in read-only mode by all applications\n\n",
                volumename.c_str());
            rv = true;
        }
        else if ( sve == SVS_FALSE  )
        {
            DebugPrintf(SV_LOG_INFO, 
                "%s was already unhidden. It is now read-only accessible by all applications\n\n",
                volumename.c_str());
            rv = false;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to unhide %s \n", volumename.c_str());
            rv = false;
        }
#else

        SVERROR sve = UnhideDrive_RO(volumename.c_str(),mountpt.c_str(),fstype.c_str());
        if ( sve.succeeded() )
        {	
            DebugPrintf(SV_LOG_INFO, "%s is mounted on %s in read-only mode.\n\n", 
                volumename.c_str(), mountpt.c_str());
            rv = true;
        }
        else
        {
            //Bug# 7934
            if(CDPUtil::QuitRequested())
            {
                DebugPrintf(SV_LOG_ERROR, "Unhide Aborted as Quit Requested\n");		
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to unhide %s \n", volumename.c_str());		
            }
            rv = false;
        }
#endif
    }while(false);

    if(rv)
    {
        CreatePendingActionFile(volumename, VOLUME_VISIBLE_RO);
    }

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::unhiderw_cxdown
*
* DESCRIPTION : 
*
*
* INPUT PARAMETERS : none
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::unhiderw_cxdown(const std::string & volumename, const std::string & mountpt, 
                             const std::string & fstype)
{
    bool rv = true;
    VsnapMgr *Vsnap;


    CDPLock::Ptr cdplock(new CDPLock(volumename));
    if(!cdplock ->acquire())
    {
        DebugPrintf(SV_LOG_INFO,
            "Exclusive lock denied for %s. re-try again after sucessfully stopping svagent service.\n",
            volumename.c_str());
        return false;
    }

    do
    {


#ifdef SV_WINDOWS
        WinVsnapMgr obj;
        Vsnap=&obj;
        SVERROR sve = UnhideDrive_RW(volumename.c_str(),volumename.c_str());
        if ( sve.succeeded() )
        {

            Vsnap->UnMountVsnapVolumesForTargetVolume(std::string(volumename.c_str()));
            DebugPrintf(SV_LOG_INFO, "%s is now accessible in read-write mode by all applications\n\n",
                volumename.c_str());
            rv = true;
        }
        else if ( sve == SVS_FALSE )
        {
            Vsnap->UnMountVsnapVolumesForTargetVolume(std::string(volumename.c_str()));
            DebugPrintf(SV_LOG_ERROR, 
                "%s was already unhidden. It is now read-write accessible by all applications\n\n", 
                volumename.c_str());			
            rv = false;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to unhide %s \n", volumename.c_str());			
            rv = false;
        }	
#else
        UnixVsnapMgr obj;
        Vsnap=&obj;

        SVERROR sve = UnhideDrive_RW(volumename.c_str(),mountpt.c_str(),fstype.c_str());

        if ( sve.succeeded() )
        {
            Vsnap->UnMountVsnapVolumesForTargetVolume(std::string(volumename.c_str()));
            DebugPrintf(SV_LOG_INFO, "%s is mounted on %s in read-write mode \n\n", 
                volumename.c_str(), mountpt.c_str());
            rv = true;
        }
        else if ( sve == SVE_FAIL  )
        {
            //Bug# 7934
            if(CDPUtil::QuitRequested())
            {
                DebugPrintf(SV_LOG_ERROR, "Unhide aborted as Quit Requested\n");
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to unhide %s \n", volumename.c_str());		
            }
            rv = false;
        }
#endif
    }while(false);
    if(rv) 
    {
        CreatePendingActionFile(volumename, VOLUME_VISIBLE_RW);
    }
    return rv;
}

/*
* FUNCTION NAME :  CDPCli::SendRequestToCx
*
* DESCRIPTION : 
*
*
* INPUT PARAMETERS : none
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::SendRequestToCx(const std::string & volumename,  VOLUME_STATE vs, const std::string & mountpt)
{
    bool rv = false;
    std::string cxVolName = volumename;
	std::string cxMountPoint = mountpt;
    FormatVolumeNameForCxReporting(cxVolName);
	FormatVolumeNameForCxReporting(cxMountPoint);
    FirstCharToUpperForWindows(cxVolName);

    do
    {

        if(!m_bconfig)
        {
            rv = false;
            break;	
        }

        DebugPrintf(SV_LOG_INFO, "Sending the request to Central Management Server ...\n");
        bool status = false;
		bool result = updateVolumeAttribute((*m_configurator), REQUEST_NOTIFY,cxVolName,vs,cxMountPoint,status);
        if( !result || !status )
        {
            DebugPrintf(SV_LOG_INFO, 
                "Request failed ... Communication with Central Management Server may be down.\n");
            rv = false;
            break;
        }

        DebugPrintf(SV_LOG_INFO, "%s", "\n\n Request has been sent successfully to CX server\n\n");
        rv = true;
    } while (0);

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::PollForRequestCompletion
*
* DESCRIPTION : 
*
*
* INPUT PARAMETERS : none
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::PollForRequestCompletion(const std::string & volumename,  VOLUME_STATE vs)
{
    bool rv = false;

    do
    {
        SV_ULONG pollingInterval ;

        if(!GetPollingInterval(pollingInterval))
        {
            DebugPrintf(SV_LOG_INFO, "%s", " Get polling interval failed \n\n");
            break;
        }

        int count = 8;
        do
        {
            count--;
            DebugPrintf(SV_LOG_INFO, "%s", "Waiting for Cx Info change, Checking for desired state\n\n");

            int visibilityStatus = -1;

            visibilityStatus = getCurrentVolumeAttribute((*m_configurator), volumename);

            if( visibilityStatus == OPERATION_FAILED)
            {
                DebugPrintf(SV_LOG_ERROR,"FAILED: to perform specified operation for %s",volumename.c_str());
                rv = false;
                break;
            }
            if( vs == visibilityStatus )
            {
                switch ( vs )
                {			
                case VOLUME_HIDDEN:
                    DebugPrintf(SV_LOG_INFO, "%s is now hidden\n\n", volumename.c_str());
                    rv = true;
                    break;
                case VOLUME_VISIBLE_RO:
                    DebugPrintf(SV_LOG_INFO, 
                        "%s is now accessible in read-only mode by all applications\n\n",
                        volumename.c_str());							
                    rv = true;
                    break;
                case VOLUME_VISIBLE_RW:      
                    bool status = false;
                    const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::DeviceUnlockedInReadWriteMode;
                    std::stringstream msg("Target device is unlocked in read-write mode.");
                    msg << ". Marking resync for the device " << volumename << " with resyncReasonCode=" << resyncReasonCode;
                    DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                    ResyncReasonStamp resyncReasonStamp;
                    STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);

                    bool result = setTargetResyncRequired((*m_configurator), volumename, status, msg.str(), resyncReasonStamp);
                    if(!status || !result)
                    {
                        DebugPrintf(SV_LOG_ERROR,"FAILED: to set Resync Required Flag for %s",
                            volumename.c_str());
                    }
                    DebugPrintf(SV_LOG_INFO, "%s is now accessible in read-write mode by all applications\n\n",
                        volumename.c_str());
                    rv = true;
                    break;
                }
            }
            if (rv)
                break;
        } while ( count && !CDPUtil::QuitRequested(pollingInterval));

        if(CDPUtil::Quit())
        {
            DebugPrintf(SV_LOG_INFO, "\nAborting ...\n");
        } else {
            if(!rv)
                DebugPrintf(SV_LOG_INFO, "\noperation timed out. Requested operation could not be completed "
                "within expected time interval. please wait for few more minutes and  and recheck or try again\n");
        }

    }while(0);
    return rv;
}

/*
* FUNCTION NAME :  CDPCli::moveRetentionLog
*
* DESCRIPTION : this function do the following
*				1. Check the pair is in pause maintenance state or not
*				2. Copy the retention from old location to new location
*				3. Update the new retention database for the new data directories
*				4. Delete the all the Vsnaps
*				5. Delete old retention log
*
* INPUT PARAMETERS : none
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES :
*   
*
* return value : true on success, false otherwise
*
*/
bool CDPCli::moveRetentionLog()
{
    return CDPUtil::moveRetentionLog((*m_configurator), nvpairs);
}

bool GetPollingInterval(SV_ULONG &pollingInterval)
{
    pollingInterval = 30;
    return true;
}

#ifdef SV_UNIX 
bool IsAbsolutePath(const string &mountPoint)
{
    if(mountPoint[0] == '/')
        return true;
    return false;

}
#endif

// This method will return true only if --sourcevolumerollback is given otherwise return false
//BugId:6701
bool CDPCli::SetRollbackFlag()
{
    bool srcVolRollback=false;
    NVPairs::iterator iter;
    iter = nvpairs.find("sourcevolumerollback");
    if(iter != nvpairs.end())
    {
        srcVolRollback=true;
    }
    return srcVolRollback;
}

bool CDPCli::SetSnapshotFlag()
{
    bool srcVol=false;
    NVPairs::iterator iter;
    iter = nvpairs.find("sourcevolume");
    if(iter != nvpairs.end())
    {
        srcVol=true;
    }
    return srcVol;
}

bool CDPCli::SetSkipCheckFlag()
{
    bool skipCheck=false;
    NVPairs::iterator iter;
    iter = nvpairs.find("skipchecks");
    if(iter != nvpairs.end())
    {
        skipCheck=true;
    }
    return skipCheck;
}

bool CDPCli::SetSrcVolumeSize(SV_ULONGLONG & srcVolSz)
{
    bool rv = true;
    srcVolSz=0;
    NVPairs::iterator iter;
    iter = nvpairs.find("source_uservisible_capacity");
    if(iter != nvpairs.end())
    {
        try {
            srcVolSz = boost::lexical_cast<SV_ULONGLONG>(iter->second);
        }
        catch (boost::bad_lexical_cast& e)
        {
            DebugPrintf(SV_LOG_INFO, "CDPCli::SetSrcVolumeSize lexical_cast failed for %s: %s\n",
                iter->second.c_str(), e.what()); 
            srcVolSz = 0;
            rv = false;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_INFO, "CDPCli::SetSrcVolumeSize the option --source_uservisible_capacity is not specified.\n"); 
        srcVolSz = 0;
        rv = false;
    }
    return rv;
}

//
// PR#15803: CDPLI snapshots,recover, rollback functionalities should 
//          provide an option to avoid direct i/o
//
bool CDPCli::do_not_usedirectio_option()
{
    bool rv = false;
    NVPairs::iterator iter;
    iter = nvpairs.find("nodirectio");
    if(iter != nvpairs.end())
    {
        rv = true;
    }
    return rv;
}

bool CDPCli::use_fsaware_copy()
{
    bool rv = false;
    LocalConfigurator localConfigurator;
    rv = localConfigurator.useFSAwareSnapshotCopy();

    NVPairs::iterator it = nvpairs.find("fsawarecopy");
    if(it != nvpairs.end())
    {
        if(stricmp(it->second.c_str(),"yes") == 0)
        {
            rv = true;
        }
        else
        {
            rv = false;
        }
    }

    return rv;
}

void CDPCli::getrollbackcleanupmsg(SNAPSHOT_REQUESTS & requests)
{
    bool cleanupflag = false;

    NVPairs::iterator it = nvpairs.find("deleteretentionlog");
    if(it != nvpairs.end())
    {
        cleanupflag = true;
    }
    SNAPSHOT_REQUESTS::iterator requests_iter = requests.begin();
    SNAPSHOT_REQUESTS::iterator requests_end = requests.end();
    for( /* empty */ ; requests_iter != requests_end; ++requests_iter)
    {
        SNAPSHOT_REQUEST::Ptr request = (*requests_iter).second;
        // PR#10815: Long Path support
        std::vector<char> vdb_dir(SV_MAX_PATH, '\0');
        DirName(request->dbpath.c_str(), &vdb_dir[0], vdb_dir.size());
        if(cleanupflag)
        {
            if(!(request->cleanupRetention))
                DebugPrintf(SV_LOG_INFO,"After completion of rollback, Not deleting the retention db and files in path %s as deleteretention is no.\n", vdb_dir.data());
        }
        else
        {
            DebugPrintf(SV_LOG_INFO,"After completion of rollback, Manualy delete the retention db and files in path %s if retention log is not needed. Since deleteretention option is not found.\n", vdb_dir.data());
        }
    }
}

//This function returns false if --stopreplication=no is given otherwise return true
//See Bug#7906
bool CDPCli::IsStopReplicationFlagSet()
{
    bool stopReplication=true;
    NVPairs::iterator iter;
    iter = nvpairs.find("stopreplication");
    if(iter != nvpairs.end())
    {
        if(iter->second == "yes" || iter->second == "YES")
            stopReplication = true;
        else if(iter->second == "no" || iter->second == "NO")
            stopReplication = false;
    }
    return stopReplication;
}

// This method will return true only if --deleteretention=yes is given otherwise return false
//BugId:6701
bool CDPCli::SetDeleteRetention()
{
    bool cleanupflag = false;
    NVPairs::iterator it = nvpairs.find("deleteretentionlog");
    if(it != nvpairs.end())
    {
        string cleanup = it -> second;
        CDPUtil::trim(cleanup);
        if(cleanup == "yes" || cleanup == "YES")
            cleanupflag = true;
    }
    return cleanupflag;
}
#ifndef SV_WINDOWS

/*
* FUNCTION NAME :  CDPCli::printvsnapandvirtualvolumeinfo
*
* DESCRIPTION : print all the vsnap present in the target mechine and list the virtual volume info
*
* INPUT PARAMETERS :  UnixVsnapMgr
*					*  
* OUTPUT PARAMETERS : 
*					  
* NOTES : 
*
* return value : none
*
*/

void CDPCli::printvsnapandvirtualvolumeinfo(UnixVsnapMgr& vsnapmgr)
{
    std::string volumeList;
    vsnapmgr.GetAllVirtualVolumes(volumeList);
    svector_t vsnapList;

    if(!volumeList.empty())
    {
        vsnapList = CDPUtil::split(volumeList, ",");
    }
    if(vsnapList.empty())
    {
        DebugPrintf(SV_LOG_INFO,"No vsnaps are mounted\n");
    }
    else
    {
        DebugPrintf(SV_LOG_INFO,"List of vsnap: \n");
    }
    for(svector_t::iterator start = vsnapList.begin(); start != vsnapList.end(); ++start)
    {
        svector_t temp = CDPUtil::split((*start), " ",2);
        if(!temp.empty())
        {
            DebugPrintf(SV_LOG_INFO,"%s\n",temp.front().c_str());
        }
    }

    VirVolume vVolume;
    std::string VirVolList;
    vVolume.VirVolumeCheckForRemount(VirVolList,true);
    if(!VirVolList.empty())
    {
        DebugPrintf(SV_LOG_INFO," Following is the list of virtual volumes mounted in the system\n\n");
        char *token = NULL;
        int novols = 1;
        token = strtok((char *)VirVolList.c_str(), ",");
        while(token != NULL)
        {
            DebugPrintf(SV_LOG_INFO, "\t%d) %s\n", novols, token);
            token = strtok(NULL, ",");
            novols++;
        }
    }
    else
        DebugPrintf(SV_LOG_INFO, "There are NO virtual volumes mounted in the system\n");

}

/*
* FUNCTION NAME :  CDPCli::capturevsnapinfo
*
* DESCRIPTION : Retrieves the VsnapContextInfo for all the mounted vsnaps
*
* INPUT PARAMETERS :  UnixVsnapMgr
*					*  
* OUTPUT PARAMETERS : 
*					  a. map of <std::string,  pair<VsnapContextInfo,size> >
*					  b. list of vsnaps for which retrieving contextinfo failed
*					  c. sizes of the vsnaps retrieved
* NOTES : 
*
* return value : true if there is no failure in getting vsnap info, 
*				 false otherwise
*
*/

bool CDPCli::capturevsnapinfo( UnixVsnapMgr& vsnapmgr, std::map< std::string, std::pair<VsnapContextInfo, vsnapsizemountpoint> >& ctxinfoMap, std::string& failedVsnaps )
{

    std::string volumeList;

    // Retrieve the list of mounted vsnaps

    vsnapmgr.GetAllVirtualVolumes(volumeList);

    svector_t vsnapList;

    if(!volumeList.empty())
    {
        vsnapList = CDPUtil::split(volumeList, ",");
    }

    VsnapVirtualVolInfo virtualInfo;
    VsnapContextInfo ctxInfo;

    // For all the vsnaps, retrieves the VsnapContextInfo from the driver

    for(svector_t::iterator start = vsnapList.begin(); start != vsnapList.end(); ++start)
    {
        struct vsnapsizemountpoint vsnapszmnpt;
        svector_t temp = CDPUtil::split((*start), " ",2);
        if(!temp.empty())
        {
            if(temp.size() == 1)
            {
                virtualInfo.DeviceName = temp.front();
                virtualInfo.VolumeName = "";
                vsnapszmnpt.mountpoint = "";
            }
            else
            {
                virtualInfo.DeviceName = temp.front();
                virtualInfo.VolumeName = temp[1];
                vsnapszmnpt.mountpoint = temp[1];
            }
            virtualInfo.Error.VsnapErrorStatus = 0;

            vsnapmgr.VsnapGetVirtualInfoDetaisFromVolume(&virtualInfo, &ctxInfo);

            if(!virtualInfo.Error.VsnapErrorStatus)
            {
                SV_ULONGLONG vsnapsize = 0;
                if (! GetVolSizeWithOpenflag(virtualInfo.DeviceName, O_RDONLY | O_LARGEFILE, vsnapsize))
                {
                    DebugPrintf(SV_LOG_INFO, "Failed to obtain the size of %s\n", virtualInfo.DeviceName.c_str());
                }
                vsnapszmnpt.size = vsnapsize;

                ctxinfoMap.insert(make_pair(virtualInfo.DeviceName, make_pair(ctxInfo, vsnapszmnpt)));
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "Unable to obtain VsnapContextInfo for the vsnap %s\n", virtualInfo.VolumeName.c_str());
                failedVsnaps += virtualInfo.DeviceName;
            }
        }

    }
    return failedVsnaps.empty();
}

/*
* FUNCTION NAME :  CDPCli::removevsnaps
*
* DESCRIPTION : Deletes all the mounted vsnaps without deleting their metadata
*
* INPUT PARAMETERS : UnixVsnapMgr
*  
* OUTPUT PARAMETERS :None 
*					 
* NOTES : 
*
* return value : true if all vsnaps were removed successfully
*				 false if removal of any vsnap failed
*/

bool CDPCli::removevsnaps( UnixVsnapMgr& vsnapmgr, const std::map< std::string, std::pair<VsnapContextInfo, vsnapsizemountpoint> >& ctxinfoMap)
{
    bool rv = true;
    for(std::map<std::string, std::pair<VsnapContextInfo, vsnapsizemountpoint> >::const_iterator iter = ctxinfoMap.begin(); iter != ctxinfoMap.end(); ++iter)
    {
        VsnapVirtualInfoList VirtualList;
        VsnapVirtualVolInfo VirtualInfo;
        VirtualInfo.VolumeName = iter->first;
        VirtualInfo.DeviceName = iter->first;
        VirtualInfo.State = VSNAP_UNMOUNT;
        VirtualList.push_back(&VirtualInfo);
        string s1,s2;
        char snapshotDrive[SV_MAX_PATH];
        inm_strcpy_s(snapshotDrive, ARRAYSIZE(snapshotDrive), iter->first.c_str());
        if(!vsnapmgr.UnmountVirtualVolume(snapshotDrive,ARRAYSIZE(snapshotDrive),&VirtualInfo,s1,s2 ))
        {
            rv = false;
        }
        if(vsnapmgr.VsnapQuit())
        {
            DebugPrintf(SV_LOG_INFO, "Requested service aborted. So some vsnaps may not be unmounted.\n");
            break;
        } 
    }

    return rv;
}

/*
* FUNCTION NAME :  CDPCli::unloadvsnapdriver
*
* DESCRIPTION : Removes the vsnap driver from memory
*
* INPUT PARAMETERS : none
*  
* OUTPUT PARAMETERS : none
*					 
* NOTES : uses "modprobe -r " to unload the driver
*
* return value : true if the driver is unloaded
*				 false if the driver fails to unload
*/
bool CDPCli::unloadvsnapdriver(void)
{
    //static const std::string unloadCommand = "/sbin/modprobe -r linvsnap";
    std::string unloadCommand = UNLOAD_DRVR_COMMAND;
    unloadCommand += " "; 
    unloadCommand += "linvsnap";
    InmCommand unload(unloadCommand);
    InmCommand::statusType status = unload.Run();
    if(status != InmCommand::completed)
    {
        std::ostringstream msg;
        msg << "Failed to unload the Virtual Snapshot driver." << std::endl << "Error = " << unload.StdErr() << std::endl;
        DebugPrintf(SV_LOG_INFO, "%s", msg.str().c_str());
        return false;
    }

    if(unload.ExitCode())
    {
        std::ostringstream msg;
        msg << "Failed to unload the Virtual Snapshot driver." << std::endl << "ExitCode = " << unload.ExitCode() << std::endl;
        if(!unload.StdOut().empty())
        {
            msg << "Output = " << unload.StdOut() << std::endl;
        }
        if(!unload.StdErr().empty())
        {
            msg << "Error = " << unload.StdErr() << std::endl;
        }

        DebugPrintf(SV_LOG_INFO, "%s", msg.str().c_str());
        return false;
    }
    return true;
}

/*
* FUNCTION NAME :  CDPCli::loadvsnapdriver
*
* DESCRIPTION : Loads the vsnap driver into memory
*
* INPUT PARAMETERS : none
*  
* OUTPUT PARAMETERS : none
*					 
* NOTES : uses "modprobe -v" to load the driver
*
* return value : true if the driver is loaded
*				 false if the driver fails to load
*/
bool CDPCli::loadvsnapdriver(void)
{
    //static const std::string loadCommand = "/sbin/modprobe -v linvsnap";
    std::string loadCommand = LOAD_DRVR_COMMAND;
    loadCommand += " ";
    loadCommand += "linvsnap";

    //Bug#16252:Check if the OS is SLES11 or higher, if so we should add an extra argument to load the driver.
    const Object & osinfo = OperatingSystem::GetInstance().GetOsInfo();
    std::string osType;
    ConstAttributesIter_t it = osinfo.m_Attributes.find(NSOsInfo::NAME);
    if (osinfo.m_Attributes.end() != it)
    {
        osType = it->second;
    }
    if(osType.find("SLES")!=std::string::npos)
    {
        osType.erase(0,4);
        osType.erase(osType.find("-"));
        if(atoi(osType.c_str()) >= 11)
        {
            loadCommand += " --allow-unsupported";
        }
    }

    InmCommand load(loadCommand);
    InmCommand::statusType status = load.Run();
    if(status != InmCommand::completed)
    {
        std::ostringstream msg;
        msg << "Failed to load the Virtual Snapshot driver." << std::endl << "Error = " << load.StdErr() << std::endl;
        DebugPrintf(SV_LOG_INFO, "%s", msg.str().c_str());
        return false;
    }

    if(load.ExitCode())
    {
        std::ostringstream msg;
        msg << "Failed to load the Virtual Snapshot driver." << std::endl << "ExitCode = " << load.ExitCode() << std::endl;
        if(!load.StdOut().empty())
        {
            msg << "Output = " << load.StdOut() << std::endl;
        }
        if(!load.StdErr().empty())
        {
            msg << "Error = " << load.StdErr() << std::endl;
        }

        DebugPrintf(SV_LOG_INFO, "%s", msg.str().c_str());
        return false;
    }
    return true;
}
/*
* FUNCTION NAME :  CDPCli::recreatevsnaps
*
* DESCRIPTION : Recreates the vsnaps with the information collected.
*
* INPUT PARAMETERS : Map of <VsnapNameSize, VsnapContextInfo>
*  
* OUTPUT PARAMETERS : none
*					 
* NOTES : 
*
* return value : None
*
* TODO:see if possible to return true if all vsnaps are created successfully, 
*				 false otherwise
*/
void CDPCli::recreatevsnaps(UnixVsnapMgr& vsnapmgr, const std::map< std::string, std::pair< VsnapContextInfo, vsnapsizemountpoint> >& ctxinfoMap)
{

    VsnapSourceVolInfo srcInfo;
    VsnapVirtualVolInfo mountVirtualInfo;


    for(std::map<std::string, std::pair<VsnapContextInfo, vsnapsizemountpoint> >::const_iterator start = ctxinfoMap.begin(); start != ctxinfoMap.end(); ++start)
    {
        DebugPrintf(SV_LOG_INFO, "Recreating vsnap %s\n", start->first.c_str());

        srcInfo.VolumeName = start->second.first.ParentVolumeName;
        srcInfo.RetentionDbPath = start->second.first.RetentionDirectory;
        srcInfo.ActualSize = start->second.second.size; 	

        mountVirtualInfo.VolumeName = start->second.second.mountpoint;
        mountVirtualInfo.DeviceName = start->first;
        mountVirtualInfo.SnapShotId  = start->second.first.SnapShotId;
        mountVirtualInfo.TimeToRecover = start->second.first.RecoveryTime;
        mountVirtualInfo.AccessMode = start->second.first.AccessType;
        mountVirtualInfo.PrivateDataDirectory = start->second.first.PrivateDataDirectory;

        mountVirtualInfo.Error.VsnapErrorStatus = 0;		

        srcInfo.VirtualVolumeList.push_back(&mountVirtualInfo);

        //For remounting vsnaps, the retention path is found from the configurator
        //If the configurator is not available the remount fails and retention path
        //will be lost causing bug#13252. For this reason remount is attempted only
        //when the configurator is available
        if(m_bconfig)
        {
            //Check if vsnap already exists, otherwise remount
            vsnapmgr.Remount(&srcInfo);
        }

        //Persist the vsnap information

        VsnapMountInfo MountData;
        memset(&MountData, 0, sizeof(MountData));
        inm_strncpy_s(MountData.VolumeName, ARRAYSIZE(MountData.VolumeName), srcInfo.VolumeName.c_str(),srcInfo.VolumeName.size());
        inm_strncpy_s(MountData.VirtualVolumeName, ARRAYSIZE(MountData.VirtualVolumeName), mountVirtualInfo.DeviceName.c_str(), mountVirtualInfo.DeviceName.size());
        MountData.VolumeSize = srcInfo.ActualSize;
        MountData.RecoveryTime = mountVirtualInfo.TimeToRecover;
        inm_strncpy_s(MountData.PrivateDataDirectory, ARRAYSIZE(MountData.PrivateDataDirectory), mountVirtualInfo.PrivateDataDirectory.c_str(), mountVirtualInfo.PrivateDataDirectory.size());
        inm_strncpy_s(MountData.RetentionDirectory, ARRAYSIZE(MountData.RetentionDirectory), srcInfo.RetentionDbPath.c_str(), srcInfo.RetentionDbPath.size());
        MountData.AccessType = mountVirtualInfo.AccessMode;
        MountData.SnapShotId = mountVirtualInfo.SnapShotId;

        std::stringstream msg;
        msg << "Persisten info:\n"
            << "VolumeName: " << MountData.VolumeName << "\n"
            << "VirtualVolumeName: " << MountData.VirtualVolumeName << "\n"
            << "VolumeSize: " << MountData.VolumeSize << "\n"
            << "RecoveryTime: " << MountData.RecoveryTime << "\n"
            << "PrivateDataDirectory: " << MountData.PrivateDataDirectory << "\n"
            << "RetentionDirectory: " << MountData.RetentionDirectory << "\n"
            << "AccessType: " << MountData.AccessType << "\n"
            << "SnapShotId: " << MountData.SnapShotId << "\n";
        DebugPrintf(SV_LOG_DEBUG, "%s", msg.str().c_str());

        vsnapmgr.PersistVsnap(&MountData, &mountVirtualInfo);
    }
}


/*
* FUNCTION NAME :  CDPCli::loadNewDriverAndRecreateVsnaps
*
* DESCRIPTION : Upgrade routine to recreate vsnaps
*
*
* INPUT PARAMETERS : none
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES : Algorithm:
*					a. Capture all information regarding currently mounted vsnaps
*					b. Do a softremoval of all the vsnaps
*					c. Unload the vsnap driver
*					d. Load the new driver
*					e. Recreate the vsnaps using the information captured in (a).
*   
*
* return value : true on success, false otherwise
*
*/

bool CDPCli::loadNewDriverAndRecreateVsnaps(void)
{
    std::string failCapture;
    //std::map<std::string, std::pair<VsnapContextInfo, SV_ULONGLONG> > ctxinfoMap;
    std::map<std::string, std::pair<VsnapContextInfo, vsnapsizemountpoint> > ctxinfoMap;
    VirVolume object;
    UnixVsnapMgr vsnapmgr;
    ACE_HANDLE vsnapfd = ACE_INVALID_HANDLE;

    // The following open succeeds if the vsnap driver is loaded. 
    // It fails otherwise.
    // We close it immediately so as not to increase the ref. count.

    if( ( vsnapfd = vsnapmgr.OpenVirtualVolumeControlDevice()) != -1)
    {
        ACE_OS::close(vsnapfd);

        if(!capturevsnapinfo(vsnapmgr, ctxinfoMap, failCapture))
        {
            DebugPrintf(SV_LOG_INFO, "Unable to obtain the vsnap information for %s\nAborting.\n", failCapture.c_str());
            return false;
        }

        if(!ctxinfoMap.empty())
        {
            DebugPrintf(SV_LOG_INFO, "Obtained the Virtual Snapshot Information Successfully.\nUnmounting the vsnaps\n");
        }

        std::map<std::string, std::pair<VsnapContextInfo, vsnapsizemountpoint> >::const_iterator ctxinfoMapIter = ctxinfoMap.begin();
        for(;ctxinfoMapIter != ctxinfoMap.end();ctxinfoMapIter++)
        {
            std::stringstream msg;
            msg << "VsnamName: " << ctxinfoMapIter->first << "\n"
                << "Context information:\n"
                << "SnapShotId: " << ctxinfoMapIter->second.first.SnapShotId << "\n"
                << "VolumeName: " << ctxinfoMapIter->second.first.VolumeName << "\n"
                << "RecoveryTime: " << ctxinfoMapIter->second.first.RecoveryTime << "\n"
                << "RetentionDirectory: " << ctxinfoMapIter->second.first.RetentionDirectory << "\n"
                << "PrivateDataDirectory: " << ctxinfoMapIter->second.first.PrivateDataDirectory << "\n"
                << "ParentVolumeName: " << ctxinfoMapIter->second.first.ParentVolumeName << "\n"
                << "AccessType: " << ctxinfoMapIter->second.first.AccessType << "\n"
                << "IsTrackingEnabled: " << ctxinfoMapIter->second.first.IsTrackingEnabled << "\n";
            DebugPrintf(SV_LOG_DEBUG, "%s", msg.str().c_str());
        }

        if(!removevsnaps(vsnapmgr, ctxinfoMap))
        {
            DebugPrintf(SV_LOG_INFO, "Unable to unmount the vsnaps \nAborting.\n" );
            return false;	
        }

        if(!ctxinfoMap.empty())
        {
            DebugPrintf(SV_LOG_INFO, "Virtual Snapshot Removal Successful.\nUnloading the Virtual Snapshot Driver\n");
        }

        //unmounting all virtual volume
        bool checkfortgt = false;
        bool deletepersistinfo = false;
        bool deletesparsefile = false;
        if(!object.UnmountAllVirtualVolumes(checkfortgt,deletepersistinfo,deletesparsefile))
        {
            DebugPrintf(SV_LOG_INFO, "Some virtual volumes are not removed.\n" );
        }
        if(!unloadvsnapdriver())
        {
            DebugPrintf(SV_LOG_INFO, "Unable to remove the Virtual Snapshot Driver.\nAborting.\n");
            printvsnapandvirtualvolumeinfo(vsnapmgr);
            return false;
        }

        DebugPrintf(SV_LOG_INFO, "Successfully removed the Virtual Snapshot Driver.\nLoading the new Virtual Snapshot Driver\n");
    }
    else 
    {
        DebugPrintf(SV_LOG_INFO, "Virtual Snapshot Driver is not loaded. Loading the new Virtual Snapshot Driver.\n");
    }

    if(!loadvsnapdriver())
    {
        DebugPrintf(SV_LOG_INFO, "Unable to load the new Virtual Snapshot Driver.\nAborting.\n");
        return false;
    }

    // 1 sec sleep so that the vsnap driver is properly loaded

    ACE_OS::sleep(1);

    DebugPrintf(SV_LOG_INFO, "Successfully loaded the new Virtual Snapshot Driver.\n");

    //remounting the virtual volume
    std::string virtualvolumelist="";
    bool islist = false;
    object.VirVolumeCheckForRemount(virtualvolumelist,islist);

    if(!ctxinfoMap.empty())
    {
        DebugPrintf(SV_LOG_INFO, "Recreating the vsnaps\n");
        recreatevsnaps(vsnapmgr, ctxinfoMap);
    }

    return true;

}

#endif


/*
* FUNCTION NAME :  CDPCli::RemountVsnaps
*
* DESCRIPTION : Remounts the vsnaps if not already mounted
*
*
* INPUT PARAMETERS : None
*                    
*                    
* OUTPUT PARAMETERS : none
*                     
*
* NOTES : true if succeed, else false
*		
*   
*
* return value : none
*
*/
bool CDPCli::RemountVsnaps()
{

#ifdef SV_WINDOWS
    WinVsnapMgr mgr;
#else
    UnixVsnapMgr mgr;
#endif
    VsnapMgr* Vsnap = &mgr;

    LocalConfigurator localConfigurator;
    if(localConfigurator.isVsnapLocalPersistenceEnabled())
    {
        return Vsnap->RemountVsnapsFromPersistentStore();
    }
    else
    {
        return Vsnap->RemountVsnapsFromCX();
    }

}


bool CDPCli::isvalidmountpoint(const std::string &src, std::string &strerr)
{
    bool bvalid = true;
    std::string srcfilesystem;
    const std::string ZFS_NAME = ZFS;

    srcfilesystem = getFSType(src);

    if (!srcfilesystem.empty())
    {
        if (ZFS_NAME == srcfilesystem)
        {
            bvalid = false;
            strerr = "Mount point is invalid because source file system is zfs\n";
        }
    }

    return bvalid;
}


bool CDPCli::printsummary( std::string const & dbname, bool show_space_usage, bool verbose )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool rv = true;

    do 
    {
        CDPDatabase db(dbname);

        CDPSummary summary;
        CDPRetentionDiskUsage  retentionusage;

        if(!db.getcdpsummary(summary))
        {
            rv = false;
            break;
        }

        if( show_space_usage && !db.get_cdp_retention_diskusage_summary(retentionusage))
        {
            rv = false;
            break;
        }

        cout << "\n\n";
        cout << setw(26)  << setiosflags( ios::left ) << "Database:"
            << dbname << "\n";

        cout << setw(26)  << setiosflags( ios::left ) << "Version:"
            << summary.version << "\n";

        cout << setw(26)  << setiosflags( ios::left ) << "Revision:"
            <<  summary.revision << "\n";

        if(summary.log_type == CDP_UNDO)
        {
            cout << setw(26)  << setiosflags( ios::left ) << "Log Type:"
                << "Roll-Backward\n";
        }	
        else if(summary.log_type == CDP_REDO)
        {
            cout << setw(26)  << setiosflags( ios::left ) << "Log Type:"
                << "Roll-Forward\n";
        }
		if(show_space_usage)
		{
			cout << setw(26)  << setiosflags( ios::left ) << "Disk Space (app):"
				<< (retentionusage.size_on_disk) << " bytes\n";


			cout << setw(26)  << setiosflags( ios::left )  << "Total Data Files:" 
				<< (retentionusage.num_files) << "\n";
		}

        string datefrom, dateto;
        if((summary.start_ts == 0) || (summary.end_ts == 0))
        {
            cout << setw(26)  << setiosflags( ios::left ) 
                << "Recovery Time Range(GMT): "
                << "Not Available" << "\n";
        }
        else 
        {
            if(!CDPUtil::ToDisplayTimeOnConsole(summary.start_ts, datefrom))
            {
                rv = false;
                break;
            }

            if(!CDPUtil::ToDisplayTimeOnConsole(summary.end_ts, dateto))
            {
                rv = false;
                break;
            }

            cout << "Recovery Time Range(GMT): "
                << datefrom << " to " << "\n"
                << setw(53) << setiosflags( ios::right ) << dateto << "\n";
        }



        cout << resetiosflags(ios::right);

        EventSummary_t::iterator iter_events    = summary.event_summary.begin();
        EventSummary_t::iterator iter_endevents = summary.event_summary.end();

        if(iter_events != iter_endevents)
        {
            cout << "Consistency Event Summary:\n";
            cout << "------------------------------------------------------\n";
            cout << setw(26) << setiosflags( ios::left ) << "Application"
                << setw(13) << setiosflags( ios::left ) << "Num. Events"
                << "\n";
            cout << "------------------------------------------------------\n";
        }

        for ( ; iter_events != iter_endevents ; ++iter_events )
        {
            SV_EVENT_TYPE type = iter_events ->first;
            SV_ULONGLONG num = iter_events ->second;

            string appname;
            if(!VacpUtil::TagTypeToAppName(type, appname))
            {
                cerr << "Error: Invalid ACM Marker found:" << type << "\n";
            }

            cout << setw(26) << setiosflags( ios::left ) << appname
                << setw(13) << setiosflags( ios::left ) << num << "\n";
        }   



        if(verbose) 
        {          
            cout << "\nTime Range Summary :\n";

            CDPTimeRangeMatchingCndn cndn;

            CDPDatabaseImpl::Ptr dbptr = db.FetchTimeRanges(cndn);

            CDPTimeRange cdptimerange;

            cout << "-------------------------------------------------------------------------------\n";
            cout << setw(5)  << setiosflags( ios::left ) << "No."
                << setw(30) << setiosflags( ios::left ) << "StartTime(GMT)"
                << setw(30) << setiosflags( ios::left ) << "EndTime"
                << setiosflags( ios::left ) << "Accuracy" << endl;
            cout << "-------------------------------------------------------------------------------\n";

            SV_ULONGLONG rangeNum = 0;                         
            string displaystarttime,displayendtime;

            SVERROR hr;

            while ( dbptr && ((hr = dbptr->read(cdptimerange)) == SVS_OK ) )
            {                    
                if (!CDPUtil::ToDisplayTimeOnConsole(cdptimerange.c_starttime, displaystarttime))
                {
                    rv =  false;
                    break;
                }
                if (!CDPUtil::ToDisplayTimeOnConsole(cdptimerange.c_endtime, displayendtime))
                {
                    rv =  false;
                    break;
                }
                ++rangeNum;

                string mode;
                if(cdptimerange.c_mode == 0) mode = ACCURACYMODE0;
                else if(cdptimerange.c_mode == 1) mode = ACCURACYMODE1;
                else if(cdptimerange.c_mode == 2) mode = ACCURACYMODE2;

                cout << setw(5)  << setiosflags( ios::left ) << rangeNum
                    << setw(30) << setiosflags( ios::left ) << displaystarttime
                    << setw(30) << setiosflags( ios::left ) << displayendtime
                    << setiosflags( ios::left ) << mode
                    << "\n";
            }

            if (!rv || hr.failed())
            {
                rv = false;
                break;
            }

            cout << "Total Ranges:" << rangeNum << "\n";
        }

    } while (0);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return rv;
}
bool CDPCli::printiopattern(std::string const & dbname)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;
    SV_ULONG ioLessThan512 = 0;
    SV_ULONG ioLessThan1K = 0;
    SV_ULONG ioLessThan2K = 0;
    SV_ULONG ioLessThan4K = 0;
    SV_ULONG ioLessThan8K = 0;
    SV_ULONG ioLessThan16K = 0;
    SV_ULONG ioLessThan64K = 0;
    SV_ULONG ioLessThan256K = 0;
    SV_ULONG ioLessThan1M = 0;
    SV_ULONG ioLessThan4M = 0;
    SV_ULONG ioLessThan8M = 0;
    SV_ULONG ioGreaterThan8M = 0;
    SV_ULONG ioTotal = 0;
    SV_UINT pctLessThan512 = 0;
    SV_UINT pctLessThan1K = 0;
    SV_UINT pctLessThan2K = 0;
    SV_UINT pctLessThan4K = 0;
    SV_UINT pctLessThan8K = 0;
    SV_UINT pctLessThan16K = 0;
    SV_UINT pctLessThan64K = 0;
    SV_UINT pctLessThan256K = 0;
    SV_UINT pctLessThan1M = 0;
    SV_UINT pctLessThan4M = 0;
    SV_UINT pctLessThan8M = 0;
    SV_UINT pctGreaterThan8M = 0;

    do
    {
        /*
        1. check it is version 1 database
        2. get exclusive access
        3. check each inode and fill the iopattern
        */

        CDPDatabase database(dbname);
        CDPDRTDsMatchingCndn cndn;
        CDPDatabaseImpl::Ptr dbptr = database.FetchDRTDs(cndn);

        cdp_rollback_drtd_t rollback_drtd;


        SVERROR hr_read;
        //Bug# 7934
        while ( dbptr && (hr_read = dbptr->read(rollback_drtd)) == SVS_OK && !(CDPUtil::QuitRequested()) )
        {
            if(rollback_drtd.length <= SV_DEFAULT_IO_SIZE_BUCKET_0)
            {
                ++ioLessThan512;
                ++ioTotal;
            }else if(rollback_drtd.length <= SV_DEFAULT_IO_SIZE_BUCKET_1)
            {
                ++ioLessThan1K;
                ++ioTotal;
            }else if(rollback_drtd.length <= SV_DEFAULT_IO_SIZE_BUCKET_2)
            {
                ++ioLessThan2K;
                ++ioTotal;
            }else if(rollback_drtd.length <= SV_DEFAULT_IO_SIZE_BUCKET_3)
            {
                ++ioLessThan4K;
                ++ioTotal;
            }else if(rollback_drtd.length <= SV_DEFAULT_IO_SIZE_BUCKET_4)
            {
                ++ioLessThan8K;
                ++ioTotal;
            }else if(rollback_drtd.length <= SV_DEFAULT_IO_SIZE_BUCKET_5)
            {
                ++ioLessThan16K;
                ++ioTotal;
            }else if(rollback_drtd.length <= SV_DEFAULT_IO_SIZE_BUCKET_6)
            {
                ++ioLessThan64K;
                ++ioTotal;
            }else if(rollback_drtd.length <= SV_DEFAULT_IO_SIZE_BUCKET_7)
            {
                ++ioLessThan256K;
                ++ioTotal;
            }else if(rollback_drtd.length <= SV_DEFAULT_IO_SIZE_BUCKET_8)
            {
                ++ioLessThan1M;
                ++ioTotal;
            }else if(rollback_drtd.length <= SV_DEFAULT_IO_SIZE_BUCKET_9)
            {
                ++ioLessThan4M;
                ++ioTotal;
            }else if(rollback_drtd.length <= SV_DEFAULT_IO_SIZE_BUCKET_10)
            {
                ++ioLessThan8M;
                ++ioTotal;
            } else
            {
                ++ioGreaterThan8M;
                ++ioTotal;
            }

        } // while loop drtds

        if(hr_read.failed())
        {
            rv=false;
            break;
        }
        //Bug# 7934
        if(CDPUtil::QuitRequested())
        {
            rv=false;
            cout << "\n\niopattern operation stopped as Quit Requested.\n";
            break;
        }

        if(ioTotal)
        {
            pctLessThan512 = (ioLessThan512 * 100)/ioTotal;
            pctLessThan1K = (ioLessThan1K * 100)/ioTotal;
            pctLessThan2K = (ioLessThan2K * 100)/ioTotal;
            pctLessThan4K = (ioLessThan4K * 100)/ioTotal;
            pctLessThan8K = (ioLessThan8K * 100)/ioTotal;
            pctLessThan16K = (ioLessThan16K * 100)/ioTotal;
            pctLessThan64K = (ioLessThan64K * 100)/ioTotal;
            pctLessThan256K = (ioLessThan256K * 100)/ioTotal;
            pctLessThan1M = (ioLessThan1M * 100)/ioTotal;
            pctLessThan4M = (ioLessThan4M * 100)/ioTotal;
            pctLessThan8M = (ioLessThan8M * 100)/ioTotal;
            pctGreaterThan8M = (ioGreaterThan8M * 100)/ioTotal;
            pctLessThan512 += (100 - (pctLessThan512 + pctLessThan1K + pctLessThan2K + pctLessThan4K + pctLessThan8K + pctLessThan16K + pctLessThan64K + pctLessThan256K + pctLessThan1M + pctLessThan4M + pctLessThan8M + pctGreaterThan8M));
        }

        cout << "Io Profile:\n";
        cout << "size      %Access %Read %Random Delay Burst Alignment Reply\n";
        if(pctLessThan512)
        {
            // random read from volume
            cout << setw(10) << "512B" 
                << setw(8) << (pctLessThan512/4)
                << setw(6) << 100
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential write to retention
            cout << setw(10) << "512B" 
                << setw(8) << (pctLessThan512/4)
                << setw(6) << 0
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential read from diff file
            cout << setw(10) << "512B" 
                << setw(8) << (pctLessThan512/4)
                << setw(6) << 100
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // random write to volume
            cout << setw(10) << "512B" 
                << setw(8) << pctLessThan512 - (3*(pctLessThan512/4))
                << setw(6) << 0
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";
        }

        if(pctLessThan1K)
        {
            // random read from volume
            cout << setw(10) << "1KB" 
                << setw(8) << (pctLessThan1K/4)
                << setw(6) << 100
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential write to retention
            cout << setw(10) << "1KB" 
                << setw(8) << (pctLessThan1K/4)
                << setw(6) << 0
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential read from diff file
            cout << setw(10) << "1KB" 
                << setw(8) << (pctLessThan1K/4)
                << setw(6) << 100
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // random write to volume
            cout << setw(10) << "1KB" 
                << setw(8) << pctLessThan1K - (3*(pctLessThan1K/4))
                << setw(6) << 0
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";
        }

        if(pctLessThan2K)
        {
            // random read from volume
            cout << setw(10) << "2KB" 
                << setw(8) << (pctLessThan2K/4)
                << setw(6) << 100
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential write to retention
            cout << setw(10) << "2KB" 
                << setw(8) << (pctLessThan2K/4)
                << setw(6) << 0
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential read from diff file
            cout << setw(10) << "2KB" 
                << setw(8) << (pctLessThan2K/4)
                << setw(6) << 100
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // random write to volume
            cout << setw(10) << "2KB" 
                << setw(8) << pctLessThan2K - (3*(pctLessThan2K/4))
                << setw(6) << 0
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";
        }

        if(pctLessThan4K)
        {
            // random read from volume
            cout << setw(10) << "4KB" 
                << setw(8) << (pctLessThan4K/4)
                << setw(6) << 100
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential write to retention
            cout << setw(10) << "4KB" 
                << setw(8) << (pctLessThan4K/4)
                << setw(6) << 0
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential read from diff file
            cout << setw(10) << "4KB" 
                << setw(8) << (pctLessThan4K/4)
                << setw(6) << 100
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // random write to volume
            cout << setw(10) << "4KB" 
                << setw(8) << pctLessThan4K - (3*(pctLessThan4K/4))
                << setw(6) << 0
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";
        }

        if(pctLessThan8K)
        {
            // random read from volume
            cout << setw(10) << "8KB" 
                << setw(8) << (pctLessThan8K/4)
                << setw(6) << 100
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential write to retention
            cout << setw(10) << "8KB" 
                << setw(8) << (pctLessThan8K/4)
                << setw(6) << 0
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential read from diff file
            cout << setw(10) << "8KB" 
                << setw(8) << (pctLessThan8K/4)
                << setw(6) << 100
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // random write to volume
            cout << setw(10) << "8KB" 
                << setw(8) << pctLessThan8K -(3*(pctLessThan8K/4))
                << setw(6) << 0
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";
        }

        if(pctLessThan16K)
        {
            // random read from volume
            cout << setw(10) << "16KB" 
                << setw(8) << (pctLessThan16K/4)
                << setw(6) << 100
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential write to retention
            cout << setw(10) << "16KB" 
                << setw(8) << (pctLessThan16K/4)
                << setw(6) << 0
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential read from diff file
            cout << setw(10) << "16KB" 
                << setw(8) << (pctLessThan16K/4)
                << setw(6) << 100
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // random write to volume
            cout << setw(10) << "16KB" 
                << setw(8) << pctLessThan16K -(3*(pctLessThan16K/4))
                << setw(6) << 0
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";
        }
        if(pctLessThan64K)
        {
            // random read from volume
            cout << setw(10) << "64KB" 
                << setw(8) << (pctLessThan64K/4)
                << setw(6) << 100
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential write to retention
            cout << setw(10) << "64KB" 
                << setw(8) << (pctLessThan64K/4)
                << setw(6) << 0
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential read from diff file
            cout << setw(10) << "64KB" 
                << setw(8) << (pctLessThan64K/4)
                << setw(6) << 100
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // random write to volume
            cout << setw(10) << "64KB" 
                << setw(8) << pctLessThan64K -(3*(pctLessThan64K/4))
                << setw(6) << 0
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";
        }

        if(pctLessThan256K)
        {
            // random read from volume
            cout << setw(10) << "256KB" 
                << setw(8) << (pctLessThan256K/4)
                << setw(6) << 100
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential write to retention
            cout << setw(10) << "256KB" 
                << setw(8) << (pctLessThan256K/4)
                << setw(6) << 0
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential read from diff file
            cout << setw(10) << "256KB" 
                << setw(8) << (pctLessThan256K/4)
                << setw(6) << 100
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // random write to volume
            cout << setw(10) << "256KB" 
                << setw(8) << pctLessThan256K -(3*(pctLessThan256K/4))
                << setw(6) << 0
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";
        }

        if(pctLessThan1M)
        {
            // random read from volume
            cout << setw(10) << "1MB" 
                << setw(8) << (pctLessThan1M/4)
                << setw(6) << 100
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential write to retention
            cout << setw(10) << "1MB" 
                << setw(8) << (pctLessThan1M/4)
                << setw(6) << 0
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential read from diff file
            cout << setw(10) << "1MB" 
                << setw(8) << (pctLessThan1M/4)
                << setw(6) << 100
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // random write to volume
            cout << setw(10) << "1MB" 
                << setw(8) << pctLessThan1M - (3*(pctLessThan1M/4))
                << setw(6) << 0
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";
        }

        if(pctLessThan4M)
        {
            // random read from volume
            cout << setw(10) << "4MB" 
                << setw(8) << (pctLessThan4M/4)
                << setw(6) << 100
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential write to retention
            cout << setw(10) << "4MB" 
                << setw(8) << (pctLessThan4M/4)
                << setw(6) << 0
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential read from diff file
            cout << setw(10) << "4MB" 
                << setw(8) << (pctLessThan4M/4)
                << setw(6) << 100
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // random write to volume
            cout << setw(10) << "4MB" 
                << setw(8) << pctLessThan4M -(3*(pctLessThan4M/4))
                << setw(6) << 0
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";
        }
        if(pctLessThan8M)
        {
            // random read from volume
            cout << setw(10) << "8MB" 
                << setw(8) << (pctLessThan8M/4)
                << setw(6) << 100
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential write to retention
            cout << setw(10) << "8MB" 
                << setw(8) << (pctLessThan8M/4)
                << setw(6) << 0
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential read from diff file
            cout << setw(10) << "8MB" 
                << setw(8) << (pctLessThan8M/4)
                << setw(6) << 100
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // random write to volume
            cout << setw(10) << "8MB" 
                << setw(8) << pctLessThan8M - (3*(pctLessThan8M/4))
                << setw(6) << 0
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";
        }

        if(pctGreaterThan8M)
        {
            // random read from volume
            cout << setw(10) << "16MB" 
                << setw(8) << (pctGreaterThan8M/4)
                << setw(6) << 100
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential write to retention
            cout << setw(10) << "16MB" 
                << setw(8) << (pctGreaterThan8M/4)
                << setw(6) << 0
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // sequential read from diff file
            cout << setw(10) << "16MB" 
                << setw(8) << (pctGreaterThan8M/4)
                << setw(6) << 100
                << setw(8) << 0 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";

            // random write to volume
            cout << setw(10) << "16MB" 
                << setw(8) << pctGreaterThan8M -(3*(pctGreaterThan8M/4))
                << setw(6) << 0
                << setw(8) << 100 
                << setw(6) << 0
                << setw(6) << 1
                << setw(10) << "sector"
                << setw(5) << "none"
                << "\n";
        }

    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;	
}
#ifdef SV_WINDOWS
bool IsSystemDrive( const char * pch_VolumeName )
{
    if (pch_VolumeName)
    {
        TCHAR rgtszSystemDirectory[ MAX_PATH + 1 ];
        if( GetSystemDirectory( rgtszSystemDirectory, MAX_PATH + 1 ))
        {
            if(IsDrive(pch_VolumeName))
            {
                if( toupper( pch_VolumeName[ 0 ] ) == toupper( rgtszSystemDirectory[ 0 ] ) )
                {
                    return (true);
                }
            }
        }
    }
    return (false);
}
#endif

bool CDPCli::unhideall()
{
    bool rv = true;
    SVERROR sve = SVS_OK;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
#ifdef SV_WINDOWS
    std::vector<std::string> hiddenvolList;
    do
    {
        bool serviceRunning;
        if(!ReplicationAgentRunning(serviceRunning))  
        {
            break;
        }

        if(serviceRunning)
        {
            DebugPrintf(SV_LOG_DEBUG, "Service is running please stop it and try again.\n");
            rv = false;
            break;
        }
        char volumeStrings[SV_MAX_PATH] = {0};	
        if(GetLogicalDriveStrings(sizeof(volumeStrings), volumeStrings)) 
        {
            char szTemp[4] = " :\\";
            char* volumeName = volumeStrings;

            do 
            {
                int count = 0;
                VOLUME_STATE vol_state = VOLUME_UNKNOWN;
                while(*volumeName)
                {
                    *(szTemp + count) = *volumeName++;
                    count++;
                }
                *(szTemp + count) = '\0';
                volumeName++;

                if( DRIVE_CDROM     != GetDriveType(szTemp) && 
                    DRIVE_REMOVABLE != GetDriveType(szTemp) )			   
                {
                    char mountPointPath[SV_MAX_PATH] = {0};
                    char mountPoint[SV_MAX_PATH] = {0};
                    do 
                    {
                        HANDLE mntptHdl = FindFirstVolumeMountPoint(szTemp, mountPointPath, sizeof(mountPointPath));
                        if (INVALID_HANDLE_VALUE == mntptHdl)
                            break;
                        inm_strcpy_s(mountPoint, ARRAYSIZE(mountPoint), szTemp);
                        inm_strcat_s(mountPoint, ARRAYSIZE(mountPoint), mountPointPath);
                        VOLUME_STATE vol_state_mntpt = GetVolumeState(mountPoint);
                        if (  VOLUME_VISIBLE_RW  != vol_state_mntpt  && // Exclude Read/Write Visible Drives and 
                            VOLUME_UNKNOWN     != vol_state_mntpt )   // drives whose state is  unknown.
                        {
                            if (!IsSystemDrive(mountPoint))
                            {
                                hiddenvolList.push_back(mountPoint);
                                cout << mountPoint << endl;
                            }
                        }
                        while (FindNextVolumeMountPoint(mntptHdl, mountPointPath, sizeof(mountPointPath)))
                        {
                            inm_strcpy_s(mountPoint, ARRAYSIZE(mountPoint), szTemp);
                            inm_strcat_s(mountPoint, ARRAYSIZE(mountPoint), mountPointPath);
                            VOLUME_STATE vol_state_mntpt = GetVolumeState(mountPoint);
                            if (  VOLUME_VISIBLE_RW  != vol_state_mntpt  && // Exclude Read/Write Visible Drives and 
                                VOLUME_UNKNOWN     != vol_state_mntpt )   // drives whose state is  unknown.
                            {
                                if (!IsSystemDrive(mountPoint))
                                {
                                    hiddenvolList.push_back(mountPoint);
                                    cout << mountPoint << endl;
                                }
                            }
                        }
                        FindVolumeMountPointClose(mntptHdl);
                    } while(false);

                    szTemp[2] = '\0';
                    VOLUME_STATE vol_state = GetVolumeState(szTemp);
                    if (  VOLUME_VISIBLE_RW  != vol_state  && // Exclude Read/Write Visible Drives and 
                        VOLUME_UNKNOWN     != vol_state )   // drives whose state is  unknown.
                    {
                        if (!IsSystemDrive(szTemp))
                        {
                            hiddenvolList.push_back(szTemp);
                            cout << szTemp << endl;
                        }
                    }
                }
            } while (*volumeName);
        }
        //unmounting all vsnap
        std::vector<VsnapPersistInfo> PassedVols;
        std::vector<VsnapPersistInfo> FailedVols;
        VsnapMgr *Vsnap;
        WinVsnapMgr vsnapshot;
        Vsnap=&vsnapshot;
        Vsnap->UnmountAllVirtualVolumes(PassedVols, FailedVols);

        //unhiding the volumes
        std::vector<std::string>::iterator iter = hiddenvolList.begin();
        for(; iter != hiddenvolList.end(); ++iter)
        {
            sve = UnhideDrive_RW((*iter).c_str(),(*iter).c_str());
            if(sve.failed())
            {
                DebugPrintf( SV_LOG_DEBUG,"Failed to unhide %s \n",(*iter).c_str());
            }
        }		
    } while(false);
#endif
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

/*
* FUNCTION NAME : verifiedtag
*
* DESCRIPTION : Mark a tag as verified
*
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS :  None
*
* NOTES : 
*
* return value : on success true otherwise false 
*
*/
bool CDPCli::verifiedtag()
{
    bool rv = true;
    NVPairs::iterator iter;
    string app;
    string eventname;
    string timestamp;
    string guid;
    string comment;
    string dbname;
    do
    {
        iter = nvpairs.find("db");
        if(iter != nvpairs.end())
        {
            dbname = iter ->second;
        }
        else
        {
            iter = nvpairs.find("vol");
            if(iter != nvpairs.end())
            {
                string volname = iter ->second;
                if(!getCdpDbName(volname,dbname))
                {
                    rv = false;
                    break;
                }

                if(dbname == "")
                {
                    DebugPrintf(SV_LOG_ERROR, "Retention is not enabled for %s\n", volname.c_str());
                    rv = false;
                    break;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,
                    "please specify the cdp database or the volume "
                    "whose corresponding cdp database is to be used. \n\n") ;
                usage(argv[0], m_operation);
                rv = false;
                break;
            }
        }

        iter = nvpairs.find("guid");
        if(iter != nvpairs.end())
            guid = iter -> second;

        iter = nvpairs.find("app");
        if(iter != nvpairs.end())
            app = iter -> second;

        iter = nvpairs.find("time");
        if(iter != nvpairs.end())
            timestamp = iter -> second;

        iter = nvpairs.find("event");
        if(iter != nvpairs.end())
            eventname = iter -> second;

        iter = nvpairs.find("comment");
        if(iter != nvpairs.end())
            comment = iter -> second;

        if(guid.empty() && (app.empty() || eventname.empty()) && timestamp.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "The specified inputs are not sufficient to mark a tag as verified. See the usage.\n");
            rv = false;
            break;
        }
        if(comment.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "The --comment option is not specified.\n");
            rv = false;
            break;
        }

        CDPMarkersMatchingCndn cndn;

        cndn.comment(comment);

        if (!app.empty())
        {
            SV_EVENT_TYPE type;
            if(!VacpUtil::AppNameToTagType(app, type))
            {
                DebugPrintf(SV_LOG_ERROR, "Invalid application name specified.\n");
                rv = false;
                break;
            }
            cndn.type(type);
        }
        if ( !eventname.empty())
        {		
            cndn.value(eventname);
        }
        if ( !timestamp.empty())
        {
            SV_ULONGLONG attime;
            if(!CDPUtil::InputTimeToFileTime(timestamp, attime))
            {
                rv = false;
                break;
            }
            cndn.atTime(attime);
        }
        if( !guid.empty())
        {
            cndn.identifier(guid);
        }
        CDPDatabase db(dbname);
        if(!db.UpdateVerifiedEvents(cndn))
        {
            DebugPrintf(SV_LOG_ERROR, "The specified tag is not mark as verified.\n");
            rv = false;
            break;
        }
        DebugPrintf(SV_LOG_INFO, "The specified tag is successfully marked as verified.\n");
    }while(false);
    return rv;
}

/*
* FUNCTION NAME : lockbookmark
*
* DESCRIPTION : Mark a bookmark as locked
*
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS :  None
*
* NOTES : 
*
* return value : on success true otherwise false 
*
*/
bool CDPCli::lockbookmark()
{
    bool rv = true;
    NVPairs::iterator iter;
    string app;
    string eventname;
    string dbname;
    string timestamp;
    string guid;
    string comment;

    do
    {
        iter = nvpairs.find("db");
        if(iter != nvpairs.end())
        {
            dbname = iter ->second;
        }
        else
        {
            iter = nvpairs.find("vol");
            if(iter != nvpairs.end())
            {
                string volname = iter ->second;
                if(!getCdpDbName(volname,dbname))
                {
                    rv = false;
                    break;
                }

                if(dbname == "")
                {
                    DebugPrintf(SV_LOG_ERROR, "Retention is not enabled for %s\n", volname.c_str());
                    rv = false;
                    break;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,
                    "please specify the cdp database or the volume "
                    "whose corresponding cdp database is to be used. \n\n") ;
                usage(argv[0], m_operation);
                rv = false;
                break;
            }
        }

        iter = nvpairs.find("app");
        if(iter != nvpairs.end())
            app = iter -> second;

        iter = nvpairs.find("event");
        if(iter != nvpairs.end())
            eventname = iter -> second;

        iter = nvpairs.find("guid");
        if(iter != nvpairs.end())
            guid = iter -> second;

        iter = nvpairs.find("time");
        if(iter != nvpairs.end())
            timestamp = iter -> second;

        iter = nvpairs.find("comment");
        if(iter != nvpairs.end())
            comment = iter -> second;

        if(eventname.empty() && timestamp.empty() && guid.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "The specified inputs are not sufficient to mark a bookmark as locked. See the usage.\n");
            rv = false;
            break;
        }

        if((!timestamp.empty()) && (!guid.empty()))
        {
            DebugPrintf(SV_LOG_ERROR, "Specify only one of the option(--time/--guid) to mark a bookmark as locked. See the usage.\n");
            rv = false;
            break;
        }

        CDPMarkersMatchingCndn cndn;

        cndn.comment(comment);

        if (!app.empty())
        {
            SV_EVENT_TYPE type;
            if(!VacpUtil::AppNameToTagType(app, type))
            {
                DebugPrintf(SV_LOG_ERROR, "Invalid application name specified.\n");
                rv = false;
                break;
            }
            cndn.type(type);
        }

        if ( !eventname.empty())
        {		
            cndn.value(eventname);
        }

        if ( !timestamp.empty())
        {
            SV_ULONGLONG attime;
            if(!CDPUtil::InputTimeToFileTime(timestamp, attime))
            {
                rv = false;
                break;
            }
            cndn.atTime(attime);
        }

        if( !guid.empty())
        {
            cndn.identifier(guid);
        }
        cndn.setlockstatus(BOOKMARK_STATE_BOTHLOCKED_UNLOCKED);

        CDPDatabase db(dbname);
        std::vector<CDPEvent> cdpEvents;
        SV_USHORT newstatus = BOOKMARK_STATE_LOCKEDBYUSER;
        if(!db.UpdateLockedBookmarks(cndn,cdpEvents,newstatus))
        {
            if(cdpEvents.empty())
                DebugPrintf(SV_LOG_INFO, "There are no matching bookmarks found with specified inputs.\n");
            else
                DebugPrintf(SV_LOG_ERROR, "The specified bookmark is not mark as locked.\n");
            rv = false;
            break;
        }
        DebugPrintf(SV_LOG_INFO, "The following bookmarks are locked\n\n");
        cout << "-------------------------------------------------------------------------------\n";
        cout << setw(5)  << setiosflags( ios::left ) << "No."
            << setw(30) << setiosflags( ios::left ) << "TimeStamp(GMT)"
            << setw(12) << setiosflags( ios::left ) << "Application"     
            << setiosflags( ios::left ) << "Event" << endl;        
        cout << "-------------------------------------------------------------------------------\n";
        SV_ULONGLONG eventNum = 0;
        string appname;
        string displaytime;
        std::vector<CDPEvent>::iterator dbIter = cdpEvents.begin();
        for(;dbIter != cdpEvents.end();++dbIter)
        {
            if (!VacpUtil::TagTypeToAppName((*dbIter).c_eventtype, appname))
            {
                rv =  false;
                break;
            }

            if (!CDPUtil::ToDisplayTimeOnConsole((*dbIter).c_eventtime, displaytime))
            {
                rv =  false;
                break;
            }
            ++eventNum;
            cout << setw(5)  << setiosflags( ios::left ) << eventNum
                << setw(30) << setiosflags( ios::left ) << displaytime
                << setw(12) << setiosflags( ios::left ) << appname
                << setiosflags( ios::left ) << (*dbIter).c_eventvalue << endl;
        }


    }while(false);
    return rv;
}
/*
* FUNCTION NAME : unlockbookmark
*
* DESCRIPTION : Mark a locked bookmark as unlocked
*
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS :  None
*
* NOTES : 
*
* return value : on success true otherwise false 
*
*/
bool CDPCli::unlockbookmark()
{
    bool rv = true;
    NVPairs::iterator iter;
    string app;
    string eventname;
    string dbname;
    string timestamp;
    string guid;
    string comment;

    do
    {
        iter = nvpairs.find("db");
        if(iter != nvpairs.end())
        {
            dbname = iter ->second;
        }
        else
        {
            iter = nvpairs.find("vol");
            if(iter != nvpairs.end())
            {
                string volname = iter ->second;
                if(!getCdpDbName(volname,dbname))
                {
                    rv = false;
                    break;
                }

                if(dbname == "")
                {
                    DebugPrintf(SV_LOG_ERROR, "Retention is not enabled for %s\n", volname.c_str());
                    rv = false;
                    break;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,
                    "please specify the cdp database or the volume "
                    "whose corresponding cdp database is to be used. \n\n") ;
                usage(argv[0], m_operation);
                rv = false;
                break;
            }
        }

        iter = nvpairs.find("app");
        if(iter != nvpairs.end())
            app = iter -> second;

        iter = nvpairs.find("event");
        if(iter != nvpairs.end())
            eventname = iter -> second;

        iter = nvpairs.find("guid");
        if(iter != nvpairs.end())
            guid = iter -> second;

        iter = nvpairs.find("time");
        if(iter != nvpairs.end())
            timestamp = iter -> second;

        iter = nvpairs.find("comment");
        if(iter != nvpairs.end())
            comment = iter -> second;

        if(eventname.empty() && timestamp.empty() && guid.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "The specified inputs are not sufficient to mark a locked bookmark as unlocked. See the usage.\n");
            rv = false;
            break;
        }

        if((!timestamp.empty()) && (!guid.empty()))
        {
            DebugPrintf(SV_LOG_ERROR, "Specify only one of the option(--time/--guid) to mark a locked bookmark as unlocked. See the usage.\n");
            rv = false;
            break;
        }

        CDPMarkersMatchingCndn cndn;

        cndn.comment(comment);

        if (!app.empty())
        {
            SV_EVENT_TYPE type;
            if(!VacpUtil::AppNameToTagType(app, type))
            {
                DebugPrintf(SV_LOG_ERROR, "Invalid application name specified.\n");
                rv = false;
                break;
            }
            cndn.type(type);
        }

        if ( !eventname.empty())
        {		
            cndn.value(eventname);
        }

        if ( !timestamp.empty())
        {
            SV_ULONGLONG attime;
            if(!CDPUtil::InputTimeToFileTime(timestamp, attime))
            {
                rv = false;
                break;
            }
            cndn.atTime(attime);
        }

        if( !guid.empty())
        {
            cndn.identifier(guid);
        }

        cndn.setlockstatus(BOOKMARK_STATE_BOTHLOCKED_UNLOCKED);

        CDPDatabase db(dbname);
        std::vector<CDPEvent> cdpEvents;
        SV_USHORT newstatus = BOOKMARK_STATE_UNLOCKED;
        if(!db.UpdateLockedBookmarks(cndn,cdpEvents,newstatus))
        {
            if(cdpEvents.empty())
                DebugPrintf(SV_LOG_INFO, "There are no matching bookmarks found with specified inputs.\n");
            else
                DebugPrintf(SV_LOG_ERROR, "The specified bookmark is not mark as unlocked.\n");
            rv = false;
            break;
        }
        DebugPrintf(SV_LOG_INFO, "The following bookmarks are unlocked\n\n");
        cout << "-------------------------------------------------------------------------------\n";
        cout << setw(5)  << setiosflags( ios::left ) << "No."
            << setw(30) << setiosflags( ios::left ) << "TimeStamp(GMT)"
            << setw(12) << setiosflags( ios::left ) << "Application"     
            << setiosflags( ios::left ) << "Event" << endl;        
        cout << "-------------------------------------------------------------------------------\n";
        SV_ULONGLONG eventNum = 0;
        string appname;
        string displaytime;
        std::vector<CDPEvent>::iterator dbIter = cdpEvents.begin();
        for(;dbIter != cdpEvents.end();++dbIter)
        {
            if (!VacpUtil::TagTypeToAppName((*dbIter).c_eventtype, appname))
            {
                rv =  false;
                break;
            }

            if (!CDPUtil::ToDisplayTimeOnConsole((*dbIter).c_eventtime, displaytime))
            {
                rv =  false;
                break;
            }
            ++eventNum;
            cout << setw(5)  << setiosflags( ios::left ) << eventNum
                << setw(30) << setiosflags( ios::left ) << displaytime
                << setw(12) << setiosflags( ios::left ) << appname
                << setiosflags( ios::left ) << (*dbIter).c_eventvalue << endl;
        }


    }while(false);
    return rv;
}

///
/// \brief
/// the following routines are added for progress callbacks for recoveryconsoleui wizard
///

void CDPCli::set_progress_callback(void (*callback_fn)(const CDPMessage * msg, void *), void * callback_param )
{
    progress_callback_fn = callback_fn;
    progress_callback_param = callback_param;
}

//#15949 :
/*
* FUNCTION NAME : showReplicationPairs
*
* DESCRIPTION : Show information of replication pairs
*
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS :  None
*
* NOTES :
*
* return value : on success true otherwise false
*
*/


bool CDPCli::showReplicationPairs()
{
    Configurator* configurator = m_configurator;

    if(nvpairs.find("custom")!=nvpairs.end())
    {
        std::string ip = nvpairs["ip"];
        int port = atoi(nvpairs["port"].c_str());
        std::string hostid = nvpairs["hostid"];
        try
        {
            LocalConfigurator lConfigurator ;
            ESERIALIZE_TYPE type = lConfigurator.getSerializerType();
            configurator = new RpcConfigurator( type, ip, port, hostid);
            configurator ->GetCurrentSettings();
        }
        catch ( ContextualException& ce )
        {
            configurator = NULL;
            DebugPrintf(SV_LOG_ERROR, 
                "Configurator instantiation failed cx ip:%s port:%d exception:%s.\n",
                ip.c_str(), port, ce.what());
        }
        catch ( ... )
        {
            configurator = NULL;
            DebugPrintf(SV_LOG_ERROR, 
                "Configurator instantiation failed cx ip:%s port:%d exception:unknown.\n",
                ip.c_str(), port);
        }
    }
    else if(nvpairs.find("cxsettings")!=nvpairs.end())
    {
        try 
        {
            LocalConfigurator lConfigurator ;
            ESERIALIZE_TYPE type = lConfigurator.getSerializerType();
            configurator = new RpcConfigurator(type, FETCH_SETTINGS_FROM_CX);
            configurator ->GetCurrentSettings();
        }
        catch ( ContextualException& ce )
        {
            configurator = NULL;
            DebugPrintf(SV_LOG_ERROR, 
                "Configurator instantiation failed with exception:%s.\n",
                ce.what());
        }
        catch ( ... )
        {
            configurator = NULL;
            DebugPrintf(SV_LOG_ERROR, 
                "Configurator instantiation failed with exception:unknown.\n");
        }

    }
    else if(nvpairs.find("file")!=nvpairs.end())
    {
        std::string fileName = nvpairs["file"];
        try 
        {
            LocalConfigurator lConfigurator ;
            ESERIALIZE_TYPE type = lConfigurator.getSerializerType();
            configurator = new RpcConfigurator(type, fileName);
            configurator ->GetCurrentSettings();
        }
        catch ( ContextualException& ce )
        {
            configurator = NULL;
            DebugPrintf(SV_LOG_ERROR, 
                "Configurator instantiation failed with exception:%s.\n",
                ce.what());
        }
        catch ( ... )
        {
            configurator = NULL;
            DebugPrintf(SV_LOG_ERROR, 
                "Configurator instantiation failed with exception:unknown.\n");
        }

    }

    if(configurator == NULL)
    {
        return false;
    }
    HOST_VOLUME_GROUP_SETTINGS volumeGroupSettings = configurator->getHostVolumeGroupSettings();  
    CDPSETTINGS_MAP cdpSettings = configurator->getInitialSettings().cdpSettings;


    typedef std::list<VOLUME_GROUP_SETTINGS>::iterator VGITER;
    typedef VOLUME_GROUP_SETTINGS::volumes_t::iterator VITER;
    for(VGITER vgiter = volumeGroupSettings.volumeGroups.begin();
        vgiter != volumeGroupSettings.volumeGroups.end(); ++vgiter)
    {
        cout <<"\n==========================================================================================="<<endl<<endl;
        cout << "\nVOLUME GROUP Id #: " << vgiter->id << endl;
        CDPUtil::printVolumeSettings(*vgiter,cdpSettings);
    }

    return true;
}

/*
* FUNCTION NAME : showSnapshotRequests
*
* DESCRIPTION : Show the snap shot request information 
*
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS :  None
*
* NOTES :
*
* return value : on success true otherwise false
*
*/


bool CDPCli::showSnapshotRequests()
{
    string BOOLEAN_STRING[] = { "NO", "YES" };
    cout << "Snapshot Settings: " <<  endl << endl;
    Configurator* configurator = m_configurator;

    if(nvpairs.find("custom")!=nvpairs.end())
    {
        std::string ip = nvpairs["ip"];
        int port = atoi(nvpairs["port"].c_str());
        std::string hostid = nvpairs["hostid"];
        try
        {
            LocalConfigurator lConfigurator ;
            ESERIALIZE_TYPE type = lConfigurator.getSerializerType();
            configurator = new RpcConfigurator( type, ip, port, hostid);
            configurator ->GetCurrentSettings();
        }
        catch ( ContextualException& ce )
        {
            configurator = NULL;
            DebugPrintf(SV_LOG_ERROR, 
                "Configurator instantiation failed cx ip:%s port:%d exception:%s.\n",
                ip.c_str(), port, ce.what());
        }
        catch ( ... )
        {
            configurator = NULL;
            DebugPrintf(SV_LOG_ERROR, 
                "Configurator instantiation failed cx ip:%s port:%d exception:unknown.\n",
                ip.c_str(), port);
        }
    }

    if(configurator == NULL)
    {
        return false;
    }

    try
    {
        SNAPSHOT_REQUESTS snapRequests = configurator->getSnapshotRequests();

        SNAPSHOT_REQUESTS::const_iterator iter;
        int i = 1;
        for ( iter = snapRequests.begin(); iter != snapRequests.end();iter++,  i++) {
            cout << "Snapshot Request [ " << i << " ]  Information:" << endl;
            cout << "--------------------------------------" << endl;
            cout << setw(30) << left << "Snapshot Id"<<": " << iter->first << endl;
            std::stringstream stream;
            SNAPSHOT_REQUEST::Ptr snapreq = iter->second;
            switch (snapreq->operation)
            {
            case SNAPSHOT_REQUEST::UNDEFINED:
                stream << setw(30) << left << "Operation" <<": " << snapreq->operation << '\n';
                break;
            case SNAPSHOT_REQUEST::PLAIN_SNAPSHOT:
                stream << setw(30) << left << "Operation" <<": " << "plain snapshot\n";
                break;
            case SNAPSHOT_REQUEST::PIT_VSNAP:
                stream << setw(30) << left << "Operation" <<": " << "point in time vsnap\n";
                break;
            case SNAPSHOT_REQUEST::RECOVERY_VSNAP:
                stream << setw(30) << left << "Operation" <<": " << "recovery vsnap\n";
                break;
            case SNAPSHOT_REQUEST::VSNAP_UNMOUNT:
                stream << setw(30) << left << "Operation" <<": " << "unmount vsnap\n";
                break;
            case SNAPSHOT_REQUEST::RECOVERY_SNAPSHOT:
                stream << setw(30) << left << "Operation" <<": " << "recovery snapshot\n";
                break;
            case SNAPSHOT_REQUEST::ROLLBACK:
                stream << setw(30) << left << "Operation" <<": " << "rollback\n";
                break;
            }

            stream << setw(30) << left <<"Source Drive" << ": " << snapreq->src << '\n';
            stream << setw(30) << left <<"Destination Drive" << ": " << snapreq->dest <<'\n';
            stream << setw(30) << left <<"Destination Mount Point" << ": " << snapreq->destMountPoint << '\n';
            stream << setw(30) << left << "Source Volume Capacity (in MB)" << ": " << snapreq->srcVolCapacity/(1024*1024) << '\n';
            stream << setw(30) << left << "Pre Script" << ": " << snapreq->prescript << '\n';
            stream << setw(30) << left << "Post Script" << ": " << snapreq->postscript << '\n';
            stream << setw(30) << left << "Data base path" << ": " << snapreq->dbpath << '\n';
            stream << setw(30) << left << "Recovery point" << ": " << snapreq->recoverypt << '\n';
            stream << setw(30) << left << "List of bookmarks:" << '\n';
            for (size_t i =0; i < snapreq->bookmarks.size(); ++i)
            {
                stream << snapreq->bookmarks[i] << '\n';
            }
            stream << '\n';
            stream << setw(30) << left << "vsnap Mount Option"  << ": " << snapreq->vsnapMountOption << '\n';
            stream << setw(30) << left << "vsnap Data directory"  << ": " << snapreq->vsnapdatadirectory << '\n';
            stream << setw(30) << left << "Delete Map "  << ": " << BOOLEAN_STRING[snapreq->deletemapflag] << '\n';
            stream << setw(30) << left << "Event Based"  << ": " << BOOLEAN_STRING[snapreq->eventBased] << '\n';
            stream << setw(30) << left << "Recovery Tag"  << ": " << snapreq -> recoverytag << '\n';
            stream << setw(30) << left << "Sequence Id"  << ": " << snapreq -> sequenceId << '\n';
            stream << "------------------------------------------------------------------" << endl;
            stream << endl << endl; // << endl;
            cout << stream.str();

        }
    }
    catch ( ContextualException& ce )
    {
        DebugPrintf(SV_LOG_ERROR,
            "\ngetSnapshotRequests call to cx failed with exception %s.\n",ce.what());
    }
    catch ( ... )
    {
        DebugPrintf(SV_LOG_ERROR,
            "\ngetSnapshotRequests call to cx failed with unknown exception.\n");
    }
    return true;
}

/*
* FUNCTION NAME : listTargetVolumes
*
* DESCRIPTION : Show the list of target volumes configured
*
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS :  None
*
* NOTES :
*
* return value : on success true otherwise false
*
*/

bool CDPCli::listTargetVolumes()
{
    bool rv = true;
    if(m_configurator == NULL)
    {
        return false;
    }
    svector_t target_volumes_t;
    if(m_configurator->getHostVolumeGroupSettings().gettargetvolumes(target_volumes_t))
    {
        svector_t::const_iterator voliter = target_volumes_t.begin();
        while(voliter != target_volumes_t.end())
        {
            std::cout << *voliter << std::endl;
            voliter++;
        }

    }
    else
    {
        rv = false;
    }
    return rv;
}

/*
* FUNCTION NAME : listProtectedVolumes
*
* DESCRIPTION : Show the list of protected volumes configured
*
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS :  None
*
* NOTES :
*
* return value : on success true otherwise false
*
*/
bool CDPCli::listProtectedVolumes()
{
    if(m_configurator == NULL)
    {
        return false;
    }
    HOST_VOLUME_GROUP_SETTINGS volumeGroupSettings = m_configurator->getHostVolumeGroupSettings();  

    typedef std::list<VOLUME_GROUP_SETTINGS>::iterator VGITER;
    typedef VOLUME_GROUP_SETTINGS::volumes_t::iterator VITER;


    for(VGITER vgiter = volumeGroupSettings.volumeGroups.begin();
        vgiter != volumeGroupSettings.volumeGroups.end(); ++vgiter)
    {
        VOLUME_GROUP_SETTINGS vg = *vgiter;

        if(vg.direction == SOURCE)
        {
            for(VITER viter = vg.volumes.begin();viter != vg.volumes.end(); ++viter)
            {
                cout << viter->first << "\n";
            }

        }

    }

    return true;
}

//16211 - lost mount points after protection from MT
/*
* FUNCTION NAME : reattachDismountedVolumes
*
* DESCRIPTION : Attaches the guid to the corresponding drive letters for 
*               all the entries in the pendingactions(detachedrives.dat).
*				Pending entries indicate that the corresponding volume mount point
*				is deleted as part of Hide/UnHide operations, but we could not reattach it.
*
* INPUT PARAMETERS : None
*
* OUTPUT PARAMETERS :  None
*
* NOTES :
*
* return value : on success true otherwise false
*
*/
#ifdef SV_WINDOWS
bool CDPCli::reattachDismountedVolumes()
{
    //Acquire lock on the guidmap file.
    bool brunning;
    ReplicationAgentRunning(brunning);
    if(brunning)
    {
        DebugPrintf(SV_LOG_DEBUG,"The service svagent is currently runnning. Please stop the service to use this option\n");
        return false;
    }


    LocalConfigurator localConfigurator;
    string cacheDir = localConfigurator.getCacheDirectory();
    cacheDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    cacheDir += "pendingactions";
    cacheDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;
    SVMakeSureDirectoryPathExists(cacheDir.c_str());

    string fileLockName = cacheDir + DETACHED_DRIVE_DATA_FILE + ".lck";
    ACE_File_Lock fileLock(ACE_TEXT_CHAR_TO_TCHAR(fileLockName.c_str()), O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
    ACE_Guard<ACE_File_Lock> autoGuardFileLock(fileLock);

    std::map<std::string,std::string> guidmap = GetDetachedVolumeEntries();
    std::map<std::string,std::string> temp = guidmap;
    std::map<std::string,std::string>::iterator it = guidmap.begin();

    if(guidmap.empty())
    {
        DebugPrintf(SV_LOG_DEBUG,"No volumes to be reatached\n");
        cout<<"No volumes found to be reattached\n";
        return true;
    }

    bool prompt = (nvpairs.find("noprompt")==nvpairs.end());
    while(it != guidmap.end())
    {
        bool attach = true;
        cout<<"Attaching guid "<<it->second<<"  the drive letter "<<it->first<<"\n";
        if(prompt)
        {
            std::string input;
            std::cout<<"Continue (Y/N)"<<std::endl;
            cin >> input;
            if(input.compare("N") == 0)
            {
                attach = false;
            }

        }

        if(attach)
        {
            CDPLock::Ptr cdplock(new CDPLock(it->first));
            if(!cdplock ->acquire())
            {
                DebugPrintf(SV_LOG_INFO,
                    "Exclusive lock denied for %s. re-try again after sucessfully stopping svagent service.\n",
                    it->first.c_str());
                return false;
            }
            if(ReattachDeviceNameToGuid(it->first,it->second))
            {
                DebugPrintf(SV_LOG_DEBUG,"GUID %s is attached to drive %s",it->second.c_str(),it->first.c_str());
                std::map<std::string,std::string>::iterator tempIt = temp.find(it->first);
                temp.erase(tempIt);
            }
        }
        it++;
    }
    UpdateDetachedVolumeEntries(temp);
    return true;
}
#endif //End of SV_WINDOWS


bool CDPCli::displaysavings()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;
    svector_t  devicelist;
    SV_ULONGLONG totalspacesaved  = 0;
    SV_ULONGLONG spacesaved = 0;
    do
    {
        if(!m_bconfig)
        {
            return false;
        }

        //STEP 1 : Get the valid input volumes 
        NVPairs::iterator iter;
        std::string comma_separated_volumesname;
        svector_t target_volumes_t;
        svector_t inputvolumelist;
        if(!m_configurator->getHostVolumeGroupSettings().gettargetvolumes(target_volumes_t))
        {
            DebugPrintf(SV_LOG_ERROR," Failed to get the target volume name list.%s %d\n", FUNCTION_NAME, LINE_NO);
            rv = false;
            break;
        }
        // Check if the volumes are passed from command line
        iter = nvpairs.find("volumes");
        if(iter != nvpairs.end())
        {
            comma_separated_volumesname = iter->second;
        }

        //if the volume names are not supplied from command line
        if(comma_separated_volumesname.empty())
        {			
            //No volume name is supplied from command line
            inputvolumelist.assign(target_volumes_t.begin(),target_volumes_t.end());
        }
        else
        {
            //parse the volume name supplied from command line
            std::string delimiter = std::string(",");
            devicelist = CDPUtil::split(comma_separated_volumesname,delimiter);
            if(devicelist.size() == 0)
            {
                DebugPrintf(SV_LOG_ERROR, "There is no volume specified with --volumes  option.\n");
                rv = false;
                break;
            }
            svector_t::const_iterator volname = devicelist.begin();
            while(volname != devicelist.end())
            {	
                std::string volumename = *volname;
                FormatVolumeNameForCxReporting(volumename);
                FirstCharToUpperForWindows(volumename);

                svector_t::iterator isvolumeprotected;
                //search the volume name supplied from command line in the protected volume list
                isvolumeprotected = find(target_volumes_t.begin(),target_volumes_t.end(),volumename);
                //if volume is in protect target volume list,persist it
                if(isvolumeprotected != target_volumes_t.end())
                {
                    inputvolumelist.push_back(volumename);
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR," Volume : %s is not a target volume. Ignoring it \n",(volumename).c_str());
                    volname++;
                    continue;
                }
                volname++;
            }
        }

        //STEP 2: Process each and every volume


        svector_t::const_iterator voliter = inputvolumelist.begin();
        //iterate through the list of volumes
        for(;voliter != inputvolumelist.end();++voliter)
        {	
            std::string volpackdevice(*voliter);
            CDPRetentionDiskUsage retentionusage;
            CDPTargetDiskUsage  targetusage;
            bool is_volpack = false;

            spacesaved  = 0;
            volpackproperties volpack_instance;

            std::cout << "\n\n" ;
            std::cout << " Volume Name : " << volpackdevice << std::endl;
            std::cout << " ____________________________________" << std::endl;
            std::cout << "\n" ;

            FormatVolumeNameForCxReporting(volpackdevice);
            FirstCharToUpperForWindows(volpackdevice);

            if( 0 == getvolpackproperties(volpackdevice,volpack_instance))
            {
                targetusage.space_saved_by_thinprovisioning = volpack_instance.m_logicalsize - volpack_instance.m_sizeondisk;
                targetusage.size_on_disk = volpack_instance.m_sizeondisk;
                spacesaved += targetusage.space_saved_by_thinprovisioning;
                is_volpack = true;
            }
            else
            {
                if(!getSourceCapacity((*m_configurator), volpackdevice, targetusage.size_on_disk))
                {
                    DebugPrintf(SV_LOG_DEBUG," Failed to get size details for volume %s\n",(volpackdevice).c_str());
                    targetusage.size_on_disk = 0;
                }
            }

            std::cout << setw(45) << setiosflags( ios::left ) << " Target volume's disk usage (in bytes) : " ;
            std::cout << targetusage.size_on_disk << std::endl;

            std::string retentiondbfilepath;
            if(getCdpDbName(*voliter,retentiondbfilepath))
            {				
                if(retentiondbfilepath != "") // "" means retention is not configured
                {
                    CDPDatabase db(retentiondbfilepath);
                    if(!db.get_cdp_retention_diskusage_summary(retentionusage))
                    {
                        DebugPrintf(SV_LOG_DEBUG," Failed to get the disk usage summary for volume : %s.\n",(*voliter).c_str());				
                    }
                }
                else
                    DebugPrintf(SV_LOG_DEBUG, "Retention is not enable for the volume %s.\n",(*voliter).c_str());
            }
            else
                DebugPrintf(SV_LOG_DEBUG, "Unable to get the db name from cache for the volume %s.\n",(*voliter).c_str());


            std::cout << setw(45) << setiosflags( ios::left ) << " Retention disk usage (in bytes) : " ;
            std::cout << retentionusage.size_on_disk << std::endl;


            std::cout << setw(45) << setiosflags( ios::left ) << " Thin provisioning savings (in bytes) : " ;

            if(is_volpack)
                std::cout << targetusage.space_saved_by_thinprovisioning << std::endl;
            else
                std::cout <<  "N/A " << std::endl;

            std::cout << setw(45) <<  setiosflags( ios::left ) << " Compression Space savings (in bytes) : " << retentionusage.space_saved_by_compression << " \n";
            spacesaved  += retentionusage.space_saved_by_compression;

            std::cout << setw(45) <<  setiosflags( ios::left ) << " Sparse retention savings (in bytes) :  ";
            if(retentionusage.space_saved_by_sparsepolicy == CDPV1_SPARSE_SAVINGS_NOTAPPLICABLE)
            {
                std::cout << "N/A " << std::endl;
            }
            else
            {
                std::cout << retentionusage.space_saved_by_sparsepolicy << "  \n";
                spacesaved  += retentionusage.space_saved_by_sparsepolicy;
            }
            std::cout << setw(45) <<  setiosflags( ios::left ) << " Space saved (in bytes) : " << spacesaved << "  \n"; 
            totalspacesaved +=  spacesaved;
        }
    }while(0);
    std::cout << std::endl << std::endl;
    std::cout << setw(45) <<  setiosflags( ios::left ) << " Cumulative Space saved (in bytes) : " << totalspacesaved << " \n";
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}



bool CDPCli::displaymetadata()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    do
    {

        // get the metadata file name
        std::string filename;
        NVPairs::iterator iter;
        iter = nvpairs.find("filename");
        if(iter == nvpairs.end())
        {
            DebugPrintf(SV_LOG_ERROR, "filename is required parameter.\n");
            rv = false;
            break;
        }
        filename = iter ->second;

        cdpv3metadatafile_t md(filename);
        cdpv3metadata_t metadata;
        SVERROR sv = SVS_OK;
        while(1)
        {
            metadata.clear();
            sv = md.read_metadata_asc(metadata);
            if(sv.failed())
            {
                DebugPrintf(SV_LOG_ERROR, "failure reading metadata.\n");
                rv = false;
                break;
            }

            if(sv == SVS_FALSE)
                break;
            std::cout << metadata ;
        }


    }while(0);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

//Debug utility to compare data of two volumes.

bool CDPCli::volumecompare()
{
    bool rv = true;
    ACE_HANDLE handle1, handle2;
    char * buffer1 = NULL,* buffer2= NULL;

    do
    {
        unsigned long long blocknum = 0;
        bool eofreached = false;
        std::string volume1;
        std::string volume2;
        SV_UINT chunksize = 0;
        SV_ULONGLONG totalbytestoberead = 0;

        NVPairs::iterator iter;
        iter = nvpairs.find("vol1");
        if(iter == nvpairs.end())
        {
            DebugPrintf(SV_LOG_ERROR, "vol1 is required parameter.\n");
            rv = false;
            break;
        }
        volume1 = iter ->second;

        iter = nvpairs.find("vol2");
        if(iter == nvpairs.end())
        {
            DebugPrintf(SV_LOG_ERROR, "vol2 is required parameter.\n");
            rv = false;
            break;
        }
        volume2 = iter ->second;

        iter = nvpairs.find("chunksize");
        if(iter == nvpairs.end())
        {
            DebugPrintf(SV_LOG_ERROR, "chunksize is required parameter.\n");
            rv = false;
            break;
        }
        chunksize = boost::lexical_cast<unsigned int>(iter ->second);

        iter = nvpairs.find("volumesize");
        if(iter == nvpairs.end())
        {
            DebugPrintf(SV_LOG_ERROR, "volumesize is required parameter.\n");
            rv = false;
            break;
        }
        totalbytestoberead = boost::lexical_cast<unsigned long long>(iter ->second);

        SV_ULONGLONG remainingbytestoread = totalbytestoberead;

        ssize_t bytesread = 0;
        std::string vol1guid = volume1;
        std::string vol2guid = volume2;

        FormatVolumeNameToGuid(vol1guid);
        FormatVolumeNameToGuid(vol2guid);

        handle1 = ACE_OS::open(vol1guid.c_str(),GENERIC_READ,ACE_DEFAULT_OPEN_PERMS);
        if(handle1 == ACE_INVALID_HANDLE)
        {
            DebugPrintf(SV_LOG_INFO,"Could not open handle for volume %s Error %d\n",volume1.c_str(),ACE_OS::last_error());
            rv = false;
            break;
        }

        handle2 = ACE_OS::open(vol2guid.c_str(),GENERIC_READ,ACE_DEFAULT_OPEN_PERMS);
        if(handle2 == ACE_INVALID_HANDLE)
        {
            DebugPrintf(SV_LOG_INFO,"Could not open handle for volume %s Error %d\n",volume2.c_str(),ACE_OS::last_error());
            rv = false;
            break;
        }

        buffer1 = new (std::nothrow) char[chunksize];
        if(buffer1 == NULL)
        {
            DebugPrintf(SV_LOG_INFO,"Could not allocate the requested memory. Try giving a smaller chunk size\n");
            rv = false;
            break;
        }

        buffer2 = new (std::nothrow) char[chunksize];
        if(buffer2 == NULL)
        {
            DebugPrintf(SV_LOG_INFO,"Could not allocate the requested memory. Try giving a smaller chunk size\n");
            rv = false;
            break;
        }

        while(remainingbytestoread && !eofreached)
        {
            blocknum++;
            ssize_t bytesread1 = 0, bytesread2 = 0;
            ssize_t bytestoread = min(remainingbytestoread,(SV_ULONGLONG)chunksize);

            if((bytesread1 = ACE_OS::read(handle1, buffer1, bytestoread))!=bytestoread)
            {
                eofreached =  true;
                if(ACE_OS::last_error() !=0 && bytesread1 < 0)
                {
                    rv = false;
                    DebugPrintf(SV_LOG_INFO,"\nError reading volume %s, Error %d\n",volume1.c_str(),ACE_OS::last_error());
                    break;
                }
            }

            if((bytesread2 = ACE_OS::read(handle2, buffer2, bytesread1))!=bytestoread)
            {
                eofreached =  true;
                if(ACE_OS::last_error() !=0 && bytesread2 < 0)
                {
                    rv = false;
                    DebugPrintf(SV_LOG_INFO,"\nError reading volume %s, Error %d\n",volume2.c_str(),ACE_OS::last_error());
                    break;
                }
            }

            if(bytesread1 != bytesread2 || memcmp(buffer1,buffer2,bytesread1))
            {
                rv = false;
                DebugPrintf(SV_LOG_INFO,"\nData not same for block : " ULLSPEC"\n",blocknum);
            }

            remainingbytestoread -= bytesread1;

            SV_ULONGLONG progress = ((totalbytestoberead - remainingbytestoread)* 100)/totalbytestoberead ;
            if(eofreached || remainingbytestoread == 0)
            {
                std::cerr<<"\r"<<"Volume comparison done                \n";
            }
            else if(progress%10 == 0)
            {
                std::cerr<<"\r"<<"Volume comparison in progress ( "<<progress<<" %)";
            }

        }

        if(rv)
        {
            DebugPrintf(SV_LOG_INFO,"\nVolumes match.\n");
        }
        else
        {
            DebugPrintf(SV_LOG_INFO,"\nVolumes do not match.\n");
        }

    }while(0);

    if(handle1 != ACE_INVALID_HANDLE)
    {
        ACE_OS::close(handle1);
    }
    if(handle2 != ACE_INVALID_HANDLE)
    {
        ACE_OS::close(handle2);
    }

    if(buffer1)delete buffer1;
    if(buffer2)delete buffer2;

    return rv;
}

//Debug utility to read volume data and output to a given file
bool CDPCli::readvolume()
{
    bool rv = true;
    ACE_HANDLE handle1, handle2;
    char * buffer = NULL;

    do
    {
        unsigned long long blocknum = 0;
        bool eofreached = false;
        std::string volume;
        std::string filename;
        SV_ULONGLONG startoffset;
        SV_ULONGLONG endoffset;
        SV_UINT chunksize = 0;
        SV_ULONGLONG totalbytestoberead = 0;

        NVPairs::iterator iter;
        iter = nvpairs.find("vol");
        if(iter == nvpairs.end())
        {
            DebugPrintf(SV_LOG_ERROR, "vol is required parameter.\n");
            rv = false;
            break;
        }
        volume = iter ->second;

        iter = nvpairs.find("filename");
        if(iter == nvpairs.end())
        {
            DebugPrintf(SV_LOG_ERROR, "filename is required parameter.\n");
            rv = false;
            break;
        }
        filename = iter ->second;

        iter = nvpairs.find("chunksize");
        if(iter == nvpairs.end())
        {
            DebugPrintf(SV_LOG_ERROR, "chunksize is required parameter.\n");
            rv = false;
            break;
        }
        chunksize = boost::lexical_cast<unsigned int>(iter ->second);

        iter = nvpairs.find("startoffset");
        if(iter == nvpairs.end())
        {
            DebugPrintf(SV_LOG_ERROR, "volumesize is required parameter.\n");
            rv = false;
            break;
        }
        startoffset = boost::lexical_cast<unsigned long long>(iter->second);

        iter = nvpairs.find("endoffset");
        if(iter == nvpairs.end())
        {
            DebugPrintf(SV_LOG_ERROR, "volumesize is required parameter.\n");
            rv = false;
            break;
        }
        endoffset = boost::lexical_cast<unsigned long long>(iter->second);

        totalbytestoberead = endoffset - startoffset;

        SV_ULONGLONG remainingbytestoread = totalbytestoberead;

        ssize_t bytesread = 0;
        std::string volumeguid = volume;

        FormatVolumeNameToGuid(volumeguid);

        handle1 = ACE_OS::open(volumeguid.c_str(),GENERIC_READ,ACE_DEFAULT_OPEN_PERMS);
        if(handle1 == ACE_INVALID_HANDLE)
        {
            DebugPrintf(SV_LOG_INFO,"Could not open handle for volume %s Error %d\n",volume.c_str(),ACE_OS::last_error());
            rv = false;
            break;
        }

        handle2 = ACE_OS::open(filename.c_str(),O_RDWR|O_CREAT,ACE_DEFAULT_OPEN_PERMS);
        if(handle2 == ACE_INVALID_HANDLE)
        {
            DebugPrintf(SV_LOG_INFO,"Could not open handle for file %s Error %d\n",filename.c_str(),ACE_OS::last_error());
            rv = false;
            break;
        }

        if(ACE_OS::llseek(handle1,startoffset,SEEK_SET)<0)
        {
            DebugPrintf(SV_LOG_INFO,"Seek on volume to offset " ULLSPEC" failed, Error %d\n",startoffset,ACE_OS::last_error());
            rv = false;
            break;
        }

        buffer = new (std::nothrow) char[chunksize];
        if(buffer == NULL)
        {
            DebugPrintf(SV_LOG_INFO,"Could not allocate the requested memory. Try giving a smaller chunk size\n");
            rv = false;
            break;
        }

        while(remainingbytestoread && !eofreached)
        {
            blocknum++;
            int bytesread = 0;
            unsigned long long bytestoread = min(remainingbytestoread,(SV_ULONGLONG)chunksize);

            if((bytesread = ACE_OS::read(handle1, buffer, bytestoread))!=bytestoread)
            {
                eofreached =  true; 
                if(ACE_OS::last_error() !=0 && bytesread < 0)
                {
                    rv = false;
                    DebugPrintf(SV_LOG_INFO,"\nError reading volume %s, Error %d\n",volume.c_str(),ACE_OS::last_error());
                    break;
                }
            }

            if(ACE_OS::write(handle2, buffer, bytesread) != bytesread)
            {
                rv = false;
                DebugPrintf(SV_LOG_INFO,"Error writing volume data to file, Error %d\n",ACE_OS::last_error());
                break;
            }

            remainingbytestoread -= bytesread;

            SV_ULONGLONG progress = ((totalbytestoberead - remainingbytestoread)* 100)/totalbytestoberead ;

            if(eofreached || remainingbytestoread == 0)
            {
                std::cerr<<"\r"<<"Volume read done                          \n";
            }
            else if(progress%10 == 0)
            {
                std::cerr<<"\r"<<"Volume read in progress ( "<<progress<<" %)";
            }

        }
    }while(0);

    if(handle1 != ACE_INVALID_HANDLE)
    {
        ACE_OS::close(handle1);
    }
    if(handle2 != ACE_INVALID_HANDLE)
    {
        ACE_OS::close(handle2);
    }

    if(buffer)delete buffer;

    return rv;
}
bool CDPCli::deleteStaleFiles()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	do
    {
        NVPairs::iterator iter;
        string db_path;

        iter = nvpairs.find("db");
        if(iter != nvpairs.end())
        {
            db_path = iter ->second;
        }
        else
        {
            iter = nvpairs.find("vol");
            if(iter != nvpairs.end())
            {
                string volname = iter ->second;
                bool shouldRun = true;
                bool foundstate = ShouldOperationRun(volname,shouldRun);
                if(!shouldRun)
                {
                    DebugPrintf(SV_LOG_ERROR,
                        "The retention logs for volume %s are being moved. Please try after sometime. \n",
                        volname.c_str());
                    rv = false;
                    break;
                }
                if(!getCdpDbName(volname,db_path))
                {
                    rv = false;
                    break;
                }

                if(db_path == "")
                {
                    DebugPrintf(SV_LOG_ERROR, "Retention is not enabled for %s\n", volname.c_str());
                    rv = false;
                    break;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,
                    "please specify the cdp database or the volume "
                    "whose corresponding cdp database is to be validated. \n\n") ;
                usage(argv[0], m_operation);
                rv = false;
                break;
            }
        }

		CDPDatabase db(db_path);
		if(!db.delete_stalefiles())
		{
            DebugPrintf(SV_LOG_ERROR, "\nFailed to delete stale files \n");
            rv = false;
            break;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "\nStale files deletion complete\n");
		}

	}while(0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool CDPCli::deleteUnusablePoints()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;

	do
    {
        NVPairs::iterator iter;
        string db_path;

	    iter = nvpairs.find("db");
        if(iter != nvpairs.end())
        {
            db_path = iter ->second;
        }
        else
        {
            iter = nvpairs.find("vol");
            if(iter != nvpairs.end())
            {
                string volname = iter ->second;
                bool shouldRun = true;
                bool foundstate = ShouldOperationRun(volname,shouldRun);
                if(!shouldRun)
                {
                    DebugPrintf(SV_LOG_ERROR,
                        "The retention logs for volume %s are being moved. Please try after sometime. \n",
                        volname.c_str());
                    rv = false;
                    break;
                }
                if(!getCdpDbName(volname,db_path))
                {
                    rv = false;
                    break;
                }

                if(db_path == "")
                {
                    DebugPrintf(SV_LOG_ERROR, "Retention is not enabled for %s\n", volname.c_str());
                    rv = false;
                    break;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,
                    "please specify the cdp database or the volume "
                    "whose corresponding cdp database is to be validated. \n\n") ;
                usage(argv[0], m_operation);
                rv = false;
                break;
            }
        }

		CDPDatabase db(db_path);
		
		if(!db.delete_unusable_recoverypts())
		{
            DebugPrintf(SV_LOG_ERROR, "\nFailed to delete unusable recovery points\n");
            rv = false;
            break;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "\nUnusable recovery points deletion complete\n");
		}

	}while(0);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}


	// Recovery operation during resync require replication to be paused
	// this requires CS to be reachable.If CS is not avaialable, a workaround could be
	// stop the vx services / ensure that the dataprotection is not running for the pair
	// and pass skip_replication_pause option to cdpcli
	// with skip_replication_pause  option, cdpcli tries to perform recovery operations without
	// sending pause request to CS.

bool CDPCli::SkipReplicationPause()
{
	bool skip_pause = false;
	NVPairs::iterator iter;
	iter = nvpairs.find("skip_replication_pause");
	if(iter != nvpairs.end())
	{
		skip_pause = true;
	}
	return skip_pause;
}

void CDPCli::Print_RollbackStats(RollbackStats_t stats)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	std::stringstream out;
	string DisplayString_tslc;
	string DisplayString_tsfc;
	std::string Recovery_period;

	CDPUtil::ToDisplayTimeOnConsole(stats.firstchange_timeStamp, DisplayString_tsfc);

	CDPUtil::ToDisplayTimeOnConsole(stats.lastchange_timeStamp, DisplayString_tslc);

	Recovery_period = CDPUtil::format_time_for_display(stats.Recovery_Period);

	out << "Retention Store Read IOs: " <<  stats.retentionfile_read_io << "\n"
		<< "Retention Store Read bytes: " << stats.retentionfile_read_bytes << "\n"
		<< "Target Volume Write IOs: " << stats.tgtvol_write_io << "\n"
		<< "Target Volume Write bytes: " << stats.tgtvol_write_bytes << "\n"
		<< "Metadatafile read IOs: " << stats.metadatafile_read_io << "\n"
		<< "Metadatafile read bytes: " << stats.metadatafile_read_bytes << "\n"
		<< "First data change Point: " << DisplayString_tsfc << "\n"
		<< "Latest data change Point: " << DisplayString_tslc << "\n"
		<< "Recovery Period: " << Recovery_period << "\n"
		<< "Rollback operation start time: " << stats.rollback_operation_starttime << "\n"
		<< "Rollback operation end time: " << stats.rollback_operation_endtime << "\n";

	DebugPrintf(SV_LOG_INFO, "%s\n", out.str().c_str());
	
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

bool CDPCli::registermt()
{
	return MTRegistration::registerMT();
}

bool CDPCli::processCsJobs()
{
#ifdef SV_WINDOWS
    unsigned int waitTimeInSecs = 60;
    CsJobProcessor jobProcessor;
    return jobProcessor.run(waitTimeInSecs);
#else
    DebugPrintf(SV_LOG_ERROR, "The operation is not supported.\n");
    return false;
#endif
}
