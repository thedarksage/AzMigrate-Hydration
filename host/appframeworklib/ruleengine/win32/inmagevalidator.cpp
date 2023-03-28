#include <atlconv.h>
#include <sstream>
#include <set>
#include <ace/Process_Manager.h>
#include <boost/lexical_cast.hpp>

#include "inmageex.h"
#include "inmagevalidator.h"
#include "system.h"
#include "registry.h"
#include "config/applocalconfigurator.h"
#include "wmi/systemwmi.h"
#include "Consistency\TagGenerator.h"
#include <vss.h>
#include "util.h"
#include "portablehelpersmajor.h"
#define VACP_E_SUCCESS 0
#define VACP_E_GENERIC_ERROR 10001
#define VACP_E_INVOLFLT_DRIVER_NOT_INSTALLED 10002
#define VACP_E_INVALID_COMMAND_LINE 0x80004005L

InmNATIPValidator::InmNATIPValidator(const std::string& name, 
                                     const std::string& description, InmRuleIds ruleId)
:Validator(name, description, ruleId)
{
}

InmNATIPValidator::InmNATIPValidator(InmRuleIds ruleId)
:Validator(ruleId)
{
}

SVERROR InmNATIPValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    std::string configuredIpFx, configuredIPVx ;
	std::stringstream stream;
	InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
    InmRuleErrorCode errorCode = INM_ERROR_NONE ;
	std::list<std::string> ipAddresses ;        
    try
    {
        WMISysInfoImpl sysInfoImpl ;
        std::list<NetworkAdapterConfig> nwAdapterList ;
        if( sysInfoImpl.loadNetworkAdapterConfiguration(nwAdapterList)  != SVS_OK )
        {
            stream << "Failed to get the network adapter configuration" << std::endl ;
        }
        else
        {
            std::list<NetworkAdapterConfig>::iterator nwAdapterListIter = nwAdapterList.begin() ;
            while( nwAdapterListIter != nwAdapterList.end() )
            {
                for( unsigned long i = 0 ; i < nwAdapterListIter->m_no_ipsConfigured ; i++ )
                {
                    ipAddresses.push_back(nwAdapterListIter->m_ipaddress[i]) ;
                }
                nwAdapterListIter++ ;
            }
        }
        if( ipAddresses.size() > 1 )
        {
            LocalConfigurator configurator ;
            bool natEnabled = false ;

            SV_LONGLONG value ;

            if( getDwordValue("SOFTWARE\\SV Systems\\FileReplicationAgent", "UseConfiguredIP", value) != SVS_OK )
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to read the UseConfiguredIP from registry rooted at SOFTWARE\\SV Systems\\FileReplicationAgent\n") ;            
			    stream << "Failed to read the UseConfiguredIP from registry rooted at SOFTWARE\\SV Systems\\FileReplicationAgent\n";
                ruleStatus = INM_RULESTAT_FAILED ;
            }
            else
            {
                if( value != 0 )
                {
                    stream << "Configured IP is enabled for FX" << std::endl ;
                    if( getStringValue("SOFTWARE\\SV Systems\\FileReplicationAgent", "ConfiguredIP", configuredIpFx) != SVS_OK )
                    {
                        DebugPrintf(SV_LOG_ERROR, "Failed to read the ConfiguredIP from registry rooted at SOFTWARE\\SV Systems\\FileReplicationAgent\n") ;
					    stream << "Failed to read the UseConfiguredIP from registry rooted at SOFTWARE\\SV Systems\\FileReplicationAgent\n";
                        ruleStatus = INM_RULESTAT_FAILED ;
                    }
                    else
                    {
                        stream << "The Configured IP for FX is " << configuredIpFx << std::endl ;
                        if( configurator.getUseConfiguredIpAddress() )
                        {
                            configuredIPVx = configurator.getConfiguredIpAddress() ;
                            stream << "The configured IP For Vx is : " << configuredIPVx <<std::endl;
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_DEBUG, "Configureed IP is not enabled for vx\n") ;
                            stream << "Configureed IP is not enabled for VX " << std::endl ;
                            ruleStatus = INM_RULESTAT_FAILED ;
                        }
                    }
                }
                else
                {
                    stream << "UseConfiguredIP key is not enabled in registry for FX" << std::endl ;
                    ruleStatus = INM_RULESTAT_FAILED ;
                }
            }
        }
        else
        {
            stream << "No of ipaddresses configured for the host are only 1" << std::endl ;
            ruleStatus = INM_RULESTAT_PASSED ;
        }
    }
    catch(ContextualException& ce)
    {
        stream << "error while performing NAT IP Check " << ce.what() << std::endl ;
        ruleStatus = INM_RULESTAT_FAILED ;
    }

    if( ruleStatus == INM_RULESTAT_FAILED )
    {
        errorCode = NAT_IP_ADDRESS_CHECK_ERROR ;
    }
    else if ( ruleStatus == INM_RULESTAT_PASSED )
    {
        errorCode = RULE_PASSED ;
    }
    else
    {
		if(strcmpi(configuredIpFx.c_str(), configuredIPVx.c_str()) != 0)
        {
            stream << "The configured IP for Vx and Fx should be same" << std::endl ;
            errorCode = NAT_IP_ADDRESS_CHECK_ERROR ;
        }
        else
        {
            if( std::find(ipAddresses.begin(), ipAddresses.end(), configuredIpFx)  == ipAddresses.end() )
            {
                stream << "Configured IP for Fx not found amond IPs available for the host " << std::endl ;
                ruleStatus = INM_RULESTAT_FAILED ;
            }
            if( std::find(ipAddresses.begin(), ipAddresses.end(), configuredIPVx)  == ipAddresses.end() )
            {
                stream << "Configured IP for Vx not found amond IPs available for the host " << std::endl ;
                ruleStatus = INM_RULESTAT_FAILED ;
            }
            if( ruleStatus == INM_RULESTAT_FAILED )
            {
                errorCode = NAT_IP_ADDRESS_CHECK_ERROR ;
            }
            else
            {
                errorCode = RULE_PASSED ;
                ruleStatus = INM_RULESTAT_PASSED ;
            }
        }
    }
    setStatus(ruleStatus) ;
    setRuleExitCode(errorCode) ;
    setRuleMessage(stream.str()) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
bool InmNATIPValidator::canfix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    return bRet ;
}

SVERROR InmNATIPValidator::fix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    return SVS_FALSE ;
}


InmNATNameValidator::InmNATNameValidator(const std::string& name, 
                                         const std::string& description,
                                         bool natenabled,
                                         InmRuleIds ruleId)
:Validator(name,description, ruleId),
m_natEnabled(natenabled)
{
    
}


InmNATNameValidator::InmNATNameValidator(bool natenabled,
                                         InmRuleIds ruleId)
:Validator(ruleId)
{
    m_natEnabled = natenabled ;
}
bool InmNATNameValidator::canfix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    return bRet ;
}

SVERROR InmNATNameValidator::fix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENETERD %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    if( canfix() )
    {
           
    }
    else
    {
        setStatus(INM_RULESTAT_FAILED) ;
        bRet = SVS_OK ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

SVERROR InmNATNameValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    SVERROR bRet = SVS_FALSE ;
    std::string hostname ;
    SV_LONGLONG value ;
    LocalConfigurator configurator ;
    bool natEnabled = false ;

    setStatus(INM_RULESTAT_FAILED) ;
    DebugPrintf(SV_LOG_DEBUG, "Checking whether Configured Hostame is being used for fx or not\n") ;
    
    if( getDwordValue("SOFTWARE\\SV Systems\\FileReplicationAgent", "UseConfiguredHostname", value) != SVS_OK )
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to read the UseConfiguredHostname from registry rooted at SOFTWARE\\SV Systems\\FileReplicationAgent\n") ;            
        std::cout<<"NAT Hostame Validation Check Failed for FX\n" ;
    }
    else
    {
        if( value == 1 )
        {
            DebugPrintf(SV_LOG_DEBUG, "UseConfiguredHostname key is enabled in registry\n") ;
            if( getStringValue("SOFTWARE\\SV Systems\\FileReplicationAgent", "ConfiguredHostname", hostname) != SVS_OK )
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to read the ConfiguredHostname from registry rooted at SOFTWARE\\SV Systems\\FileReplicationAgent\n") ;
                std::cout<<"NAT Hostname Validation Check Failed for FX\n" ; 
            }
            else
            {
                natEnabled = true ;
                DebugPrintf(SV_LOG_DEBUG, "The Hostname address is set as %s\n", hostname.c_str()) ;
            }
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "UseConfiguredHostname key is not enabled in registry\n") ;
        }
    }

    if( natEnabled == m_natEnabled )
    {
        std::cout<<"NAT Hostname Check Passed for FX\n" <<std::endl ;

        natEnabled = false ;

        DebugPrintf(SV_LOG_DEBUG, "Checking whether Configured Hostname is being used for vx or not\n") ;
        if( configurator.getUseConfiguredHostname() )
        {
            hostname = configurator.getConfiguredHostname() ;
            DebugPrintf(SV_LOG_DEBUG, "The configured Hostname for the VX is %s\n", hostname.c_str()) ;
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "Configureed Hostname is not enabled for vx\n") ;
        }

        if( natEnabled == m_natEnabled )
        {
            std::cout<<"NAT Hostname Check Passed for VX\n" <<std::endl ;
            setStatus(INM_RULESTAT_PASSED) ;
            bRet = SVS_OK ;
        }
        else
        {
            std::cout<<"NAT Hostname Check Failed for VX\n" <<std::endl ;
        }
    }
    else
    {
        std::cout<<"NAT Hostname Check Failed for FX\n" <<std::endl ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

VolTagIssueValidator::VolTagIssueValidator(const std::string& name, 
                                           const std::string& description,
                                           const std::list<std::string>& volList,
										   InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_volList(volList)
{

}

VolTagIssueValidator::VolTagIssueValidator(const std::list<std::string>& volList,
										   const std::string strVacpOptions,
                                           InmRuleIds ruleId)
:Validator(ruleId),
m_strVacpOptions(strVacpOptions),
m_volList(volList)
{
    m_volList = volList ;

}
SVERROR VolTagIssueValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet  = SVS_OK ;
    InmRuleStatus status = INM_RULESTAT_NONE ;
	InmRuleErrorCode ruleStatus = RULE_STAT_NONE;
    std::stringstream args, stream;
	ACE_Process_Options options;
    ACE_TEXT_STARTUPINFO* startupInfo = options.startup_info() ;
    startupInfo->dwFlags = STARTF_USESHOWWINDOW ;
    startupInfo->wShowWindow = SW_HIDE ;
    options.handle_inheritance(false);
    std::list<std::string>::const_iterator volIter = m_volList.begin() ;
    int index = 0 ;
    std::string dependentRule ;
    if( isDependentRuleFailed(dependentRule) )
    {
        stream <<  "Skipping this rule as dependent rule : " << dependentRule << " failed " << std::endl;
        status = INM_RULESTAT_SKIPPED ;
    }
    else
    {
      //  stream << " Volumes on which consistency tags cannot be issued: " << std::endl << std::endl;
        AppLocalConfigurator localConfigurator ;
	    std::string vacpPath;
	    vacpPath = localConfigurator.getInstallPath() ;
	    if(vacpPath.empty() || vacpPath.find_last_of("\\") == std::string::npos )
	    {
		    ruleStatus = VACP_PROCESS_CREATION_ERROR;
		    setStatus(INM_RULESTAT_FAILED) ;
		    setRuleExitCode(ruleStatus);
		    setRuleMessage(std::string("vacp binary is not availavle") );
		    return SVS_FALSE;
	    }
	    if(vacpPath.find_last_of("\\") != vacpPath.size() )
	    {
		    vacpPath += "\\";
	    }
    	
	 	if(!m_strVacpOptions.empty())
		{
			if(strcmpi(m_strVacpOptions.c_str(), "N/A") == 0)
            {
                stream << "Consistency Options not set.. No need to check for the ability to issue the book mark " << std::endl ;
                status = INM_RULESTAT_PASSED ;
                ruleStatus = RULE_STAT_NONE;
            }
            else
            {
			    std::string strTempVacpCommand = std::string(" -verify");
			    m_strVacpOptions += strTempVacpCommand ;
                stream << "Consistency verify options used :" ;

			    DebugPrintf(SV_LOG_INFO,"The vacp verify command is %s",strTempVacpCommand.c_str());
				TagGenerator obj(m_strVacpOptions,localConfigurator.getConsistencyTagIssueTimeLimit());
			    SV_ULONG exitCode = 0x1;

			    if(obj.issueConsistenyTag("",exitCode)) //as per Suresh suggestion since we are not able to find policy id till now
			    {
				    if(exitCode !=  VACP_E_SUCCESS )			
				    {
					    bRet = SVS_FALSE; 
					    status = INM_RULESTAT_FAILED ;
                    }
                    if( exitCode == VSS_E_WRITER_INFRASTRUCTURE )
                        ruleStatus = VSS_E_INFRASTRUCATURE ;
                    else if(exitCode !=  VACP_E_SUCCESS )
					    ruleStatus = VACP_TAG_ISSUE_ERROR;
					stream << obj.getCommandStr() << " ";
                    FormatVacpErrorCode( stream, exitCode ) ;
				    stream <<obj.stdOut()<<std::endl;
			    }
			    else
			    {
				    bRet = SVS_FALSE; 
				    status = INM_RULESTAT_FAILED ;
				    ruleStatus = VACP_PROCESS_CREATION_ERROR;
			    }
            }
		}
	    else
	    {
            std::string tempVolumeStr;
            args.str("") ;
		    vacpPath += "vacp.exe";
            if( m_volList.size() > 0 )
            {
                while( volIter != m_volList.end() )
		        {
			        std::string volume = *volIter ;
                    sanitizeVolumePathName(volume);
                    tempVolumeStr += volume;
                    if( volume.length() == 1 )
                    {
                        tempVolumeStr += ":;" ;
                    }
                    else
                    {
                        tempVolumeStr += ";" ;
                    }
                    volIter++ ;
                }
                args << "-v " << tempVolumeStr << " -verify" ;
                options.command_line ("%s %s", vacpPath.c_str(), args.str().c_str());    
                pid_t processId = ACE_Process_Manager::instance ()->spawn (options);
                if ( processId == ACE_INVALID_PID) 
                {
                    stream << vacpPath <<" "  << args.str() << std::endl ;
                    stream << " Failed to spawn a process.. error " << ACE_OS::last_error() << std::endl << std::endl;
                    status = INM_RULESTAT_FAILED ;
                    ruleStatus = VACP_PROCESS_CREATION_ERROR;
                }
                else
                {
                    ACE_exitcode exitCode = 0 ;
                    ACE_Process_Manager::instance ()->wait(processId, &exitCode) ;
                    DebugPrintf(SV_LOG_DEBUG, "%s %s exited with %d\n", vacpPath.c_str(), args.str().c_str(), exitCode) ;
                    if( exitCode != 0 )
                    {
                        //stream << " " << ++index<< ". " << volIter->c_str() ;
						stream << "vacp failed." << std::endl << " Command: " << vacpPath.c_str() << " " <<args.str().c_str() << std::endl;
                        status = INM_RULESTAT_FAILED ;
                        ruleStatus = VACP_TAG_ISSUE_ERROR;
                    }
                }
				options.release_handles();
            }
            else
            {
                stream <<"\n\n Cant find any volumes to issue the tag \n" <<std::endl;
                status = INM_RULESTAT_FAILED ;
                ruleStatus = VACP_TAG_ISSUE_ERROR;
            }
	    }
        if( status == INM_RULESTAT_FAILED )
        {
            stream <<"\n\n Cannot issue tags on the volumes\n" <<std::endl;
        }
        else
        {
            status = INM_RULESTAT_PASSED ;
		    ruleStatus = RULE_PASSED;
        }
    }
    setStatus(status) ;
	setRuleExitCode(ruleStatus);
	setRuleMessage(stream.str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
/*
void VolTagIssueValidator::FormatVacpErrorCode( std::stringstream& stream, SV_ULONG& exitCode )
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME ) ;
	if(exitCode != 0)
	{
		stream << "Vacp failed to issue the consistency. Failed with Error Code:" ;
		switch( exitCode )
		{
		case VACP_E_GENERIC_ERROR: //(10001) 
			stream << "10001 (Vacp Generic Error)" ;
			break;
		case VACP_E_INVOLFLT_DRIVER_NOT_INSTALLED: //(10002) 
			stream << "10002. (Scout Filter driver is not loaded. Ensure VX is installed and machine is rebooted)" ;
			break;
		case VACP_E_INVALID_COMMAND_LINE: 
			stream << "0x80004005L. ( Invalid command line. Use Vacp -h to know more about command line options )" ;             
			break;
		case VSS_E_BAD_STATE: stream << "0x80042301L . ( The backup components object is not initialized )" ; 
			break;
		case VSS_E_PROVIDER_ALREADY_REGISTERED      : stream << "0x80042303L . ( Provider already registered )" ; 
			break;
		case VSS_E_PROVIDER_NOT_REGISTERED  : stream << "0x80042304L . ( Provider not registered )" ; 
			break;
		case VSS_E_PROVIDER_VETO    : stream << "0x80042306L . ( Provider Veto )" ; 
			break;
		case VSS_E_PROVIDER_IN_USE  : stream << "0x80042307L . ( Provider already in use. Retry after some time )" ; 
			break;
		case VSS_E_OBJECT_NOT_FOUND : stream << "0x80042308L . ( VSS object not found )" ; 
			break;
		case VSS_S_ASYNC_PENDING    : stream << "0x42309L . ( VSS async pending )" ; 
			break;
		case VSS_S_ASYNC_FINISHED   : stream << "0x4230aL . ( VSS async finished )" ; 
			break;
		case VSS_S_ASYNC_CANCELLED  : stream << "0x4230bL . ( VSS async cacelled )" ; 
			break;
		case VSS_E_VOLUME_NOT_SUPPORTED     : stream << "0x8004230cL . ( VSS does not support issuing tag on one of the volumes )" ; 
			break;
		case VSS_E_VOLUME_NOT_SUPPORTED_BY_PROVIDER : stream << "0x8004230eL . ( Provider does not support issuing tag on one of the volumes )" ; 
			break;
		case VSS_E_OBJECT_ALREADY_EXISTS    : stream << "0x8004230dL . ( VSS object already exists )" ; 
			break;
		case VSS_E_UNEXPECTED_PROVIDER_ERROR: 
			stream << "0x8004230fL . ( VSS unexpected provider error )" ; 
			break;
		case VSS_E_CORRUPT_XML_DOCUMENT     : 
			stream << "0x80042310L . ( VSS XML document corrupted )" ; 
			break;
		case VSS_E_INVALID_XML_DOCUMENT     : 
			stream << "0x80042311L . ( invalid XML document )" ; 
			break;
		case VSS_E_MAXIMUM_NUMBER_OF_VOLUMES_REACHED: 
			stream << "0x80042312L . ( No. of volumes reached the VSS limit )" ; 
			break;
		case VSS_E_FLUSH_WRITES_TIMEOUT     : 
			stream << "0x80042313L . ( VSS flush writes timedout )" ;
			break;
		case VSS_E_HOLD_WRITES_TIMEOUT      : 
			stream << "0x80042314L . (  VSS hold writes timedout )" ; 
			break;
		case VSS_E_UNEXPECTED_WRITER_ERROR  : 
			stream << "0x80042315L . ( Unexpected writer error )" ; 
			break;
		case VSS_E_SNAPSHOT_SET_IN_PROGRESS : 
			stream << "0x80042316L . ( VSS snapshot in progress )" ; 
			break;
		case VSS_E_MAXIMUM_NUMBER_OF_SNAPSHOTS_REACHED      : 
			stream << "0x80042317L . ( No. of Snapshots reached the VSS limit )" ; 
			break;
		case VSS_E_WRITER_INFRASTRUCTURE    : 
			stream << "0x80042318L . ( The writer infrastructure is not operating properly. Pleae follow the mentioned fix steps )" ; 
			break;
		case VSS_E_WRITER_NOT_RESPONDING    : 
			stream << "0x80042319L . ( VSS Writer is not responding )" ; 
			break;
		case VSS_E_WRITER_ALREADY_SUBSCRIBED: 
			stream << "0x8004231aL . ( VSS Writer is alredy subscribed )" ; 
			break;
		case VSS_E_UNSUPPORTED_CONTEXT      : 
			stream << "0x8004231bL . ( VSS is in unsupported context )" ; 
			break;
		case VSS_E_VOLUME_IN_USE    : 
			stream << "0x8004231dL . ( Volume is already in use. Retry after some time )" ; 
			break;
		case VSS_E_MAXIMUM_DIFFAREA_ASSOCIATIONS_REACHED    : 
			stream << "0x8004231eL . ( Maximun diff area association reached )" ; 
			break;
		case VSS_E_INSUFFICIENT_STORAGE     : 
			stream << "0x8004231fL . ( No sufficient space available to issue the tag )" ; 
			break;
		case VSS_E_NO_SNAPSHOTS_IMPORTED    : 
			stream << "0x80042320L . ( No snapshots are imported )" ; 
			break;
		case VSS_S_SOME_SNAPSHOTS_NOT_IMPORTED      : 
			stream << "0x80042321L . ( Some of the snapshots are not imported )" ; 
			break;
		case VSS_E_MAXIMUM_NUMBER_OF_REMOTE_MACHINES_REACHED: 
			stream << "0x80042322L . ( No. of remote machines reached the limit )" ; 
			break;
		case VSS_E_REMOTE_SERVER_UNAVAILABLE: 
			stream << "0x80042323L . ( Remote server is not available )" ; 
			break;
		case VSS_E_REMOTE_SERVER_UNSUPPORTED: 
			stream << "0x80042324L . ( Remote server does not support this operation )" ; 
			break;
		case VSS_E_REVERT_IN_PROGRESS       : 
			stream << "0x80042325L . ( VSS revert is in progress )" ; 
			break;
		case VSS_E_REVERT_VOLUME_LOST       : 
			stream << "0x80042326L . ( Revert volume is lost)" ; 
			break;
		case VSS_E_REBOOT_REQUIRED  :
			stream << "0x80042327L. ( Reboot required to perform this operation )" ; 
			break;
		}
		stream << std::endl ;
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME ) ;
}
*/

SVERROR VolTagIssueValidator::fix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet  = SVS_FALSE ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
bool VolTagIssueValidator::canfix()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet  = false;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

VersionEqualityValidator::VersionEqualityValidator(const std::string& name, 
                                const std::string& description, 
                                const std::string& srcVersion,
                                const std::string& tgtVersion,
								InmRuleIds ruleId,
								const std::string& srcEdition,
								const std::string& tgtEdition)
:Validator(name, description, ruleId),
 m_srcVersion(srcVersion),
 m_tgtVersion(tgtVersion),
 m_srcEdition(srcEdition),
 m_tgtEdition(tgtEdition)
{
}

VersionEqualityValidator::VersionEqualityValidator(const std::string& srcVersion,
                                const std::string& tgtVersion,
								InmRuleIds ruleId,
								const std::string& srcEdition,
								const std::string& tgtEdition)
:Validator(ruleId),
 m_srcVersion(srcVersion),
 m_tgtVersion(tgtVersion),
 m_srcEdition(srcEdition),
 m_tgtEdition(tgtEdition)
{
}
bool VersionEqualityValidator::canfix()
{
    return false ;
}

SVERROR VersionEqualityValidator::fix()
{
    return SVS_FALSE ;
}

SVERROR VersionEqualityValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    InmRuleStatus status ;
	InmRuleErrorCode errorCode = RULE_STAT_NONE ;
    std::stringstream resultStram ;
    std::string dependentRule ;
    if( isDependentRuleFailed(dependentRule) )
    {
        resultStram <<  "Skipping this rule as dependent rule : " << dependentRule << " failed " << std::endl;
        status = INM_RULESTAT_SKIPPED ;
    }
    else
    {
		if(strcmpi(m_srcVersion.c_str(), m_tgtVersion.c_str()) == 0)
        {
           resultStram << "Source And Target have same application \n" ;
           resultStram << "Version: " << m_srcVersion << std::endl ;
		   if(m_srcEdition.empty() || m_tgtEdition.empty())
		   {
			   resultStram << std::endl << "Warning: Edition information is not vailable. Skipping editions comparision." << std::endl;
			   bRet = SVS_OK ;
		   }
		   else if(strcmpi(m_srcEdition.c_str(),m_tgtEdition.c_str()) == 0)
		   {
			   resultStram << "Source and Target have same edition: " << m_srcEdition << std::endl;
			   bRet = SVS_OK;
		   }
		   else
		   {
			   resultStram << "Source and Target have different editions." << std::endl
				   << "Source Edition: " << m_srcEdition << std::endl
				   << "Target Edition: " << m_tgtEdition << std::endl;
		   }
           
        }
        else
        {
			if(strcmpi(m_tgtVersion.c_str(), "") != 0)
            {
                resultStram << "Source And Target application have Differnet versions \n";
                resultStram << " Source application Version: " << m_srcVersion << std::endl ;
                resultStram << " Target application Version: "<< m_tgtVersion << std::endl ;
            }
            else
            {
                resultStram << "Target does not have the version installed \n";
                resultStram << " Source application Version: " << m_srcVersion << std::endl ;
            }
        }
    }
	if(bRet == SVS_OK)
	{
		status = INM_RULESTAT_PASSED;
        errorCode = RULE_PASSED;
	}
	else
	{
		status = INM_RULESTAT_FAILED;
		errorCode = VERSION_MISMATCH_ERROR;
	}
	setStatus(status) ;
	setRuleExitCode(errorCode);
	setRuleMessage(resultStram.str()) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


DomainEqualityValidator::DomainEqualityValidator(const std::string& name, 
                                const std::string& description, 
                                const std::string& srcDomain,
                                const std::string& tgtDomain,
                                InmRuleIds ruleId)
:Validator(name, description, ruleId),
 m_srcDomain(srcDomain),
 m_tgtDomain(tgtDomain)
{
}

DomainEqualityValidator::DomainEqualityValidator(const std::string& srcDomain,
                                const std::string& tgtDomain,
                                InmRuleIds ruleId)
:Validator(ruleId),
 m_srcDomain(srcDomain),
 m_tgtDomain(tgtDomain)
{
}

bool DomainEqualityValidator::canfix()
{
    return false ;
}

SVERROR DomainEqualityValidator::fix()
{
    return SVS_FALSE ;
}

SVERROR DomainEqualityValidator::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_FALSE ;
    InmRuleStatus status ;
    std::stringstream resultStram ;

	if(strcmpi(m_srcDomain.c_str(), m_tgtDomain.c_str()) == 0)
    {
        resultStram << " Source And Target Machines Belong to same domain \n" ;
        resultStram << "Domain Name: " << m_srcDomain << std::endl ;
        status = INM_RULESTAT_PASSED;
        bRet = SVS_OK ;
    }
    else
    {

        resultStram << " Source And Target Machines are in Two Differnet Domains \n";
        resultStram << " Source Domain Name: " << m_srcDomain << std::endl ;
        resultStram << " Target Domain Name: "<< m_tgtDomain << std::endl ;
        status = INM_RULESTAT_FAILED;
    }
    setStatus(status) ;
	setRuleMessage(resultStram.str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

VolumeCapacityCheckValidator::VolumeCapacityCheckValidator(const std::string& name, 
                                       const std::string& description, 
									   const std::list<std::map<std::string, std::string>> mapVolCapacities,
                                       InmRuleIds ruleId)
:Validator(name, description, ruleId),
m_VolCapacities(mapVolCapacities)
{
}

VolumeCapacityCheckValidator::VolumeCapacityCheckValidator(const std::list<std::map<std::string, std::string>> mapVolCapacities,
                                       InmRuleIds ruleId)
:Validator(ruleId),
m_VolCapacities(mapVolCapacities)
{
}

SVERROR VolumeCapacityCheckValidator::evaluate()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SVERROR bRet = SVS_OK ;
    InmRuleStatus status = INM_RULESTAT_PASSED ;
    std::stringstream resultStream ;
	std::string dependentRule ;
	InmRuleErrorCode ruleStatus = RULE_PASSED;

	if( isDependentRuleFailed(dependentRule) )
    {
        resultStream <<  "Skipping this rule as dependent rule : " << dependentRule << " failed " << std::endl;
        status = INM_RULESTAT_SKIPPED ;
        ruleStatus = INM_ERROR_NONE ;
    }
    else
    {
		std::list<std::map<std::string, std::string> >::const_iterator volCapacitiesIter = m_VolCapacities.begin();
		while(volCapacitiesIter != m_VolCapacities.end())
		{
			if((volCapacitiesIter->find("srcVolCapacity")) != volCapacitiesIter->end())
			{
				SV_ULONGLONG srcCapacity = boost::lexical_cast<SV_ULONGLONG>(volCapacitiesIter->find("srcVolCapacity")->second);
				SV_ULONGLONG tgtCapacity = boost::lexical_cast<SV_ULONGLONG>(volCapacitiesIter->find("tgtVolCapacity")->second);
				if(tgtCapacity < srcCapacity)
				{
					resultStream << "Target volume size is less than source volume size " << std::endl ;
					resultStream << "The Source Volume Name : " << volCapacitiesIter->find("srcVolName")->second << std::endl;
					resultStream << "The Target Volume Name : " << volCapacitiesIter->find("tgtVolName")->second << std::endl;
					resultStream << "The Source Volume size : " << srcCapacity << std::endl;
					resultStream << "The Target Volume size : " << tgtCapacity << std::endl;
					bRet = SVS_FALSE ;
				}
			}
			volCapacitiesIter++;
		}
		
		if(bRet == SVS_FALSE )
		{
			ruleStatus = VOLUME_CAPACITY_ERROR;
			status = INM_RULESTAT_FAILED ;
		}
		else
		{
			resultStream << "Target volume capacities are in sync with source volume capacities" << std::endl;
		}
	}

	setStatus(status) ;
	setRuleExitCode(ruleStatus);
	std::string message = resultStream.str();
	setRuleMessage(message);	
	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool VolumeCapacityCheckValidator::canfix()
{
    return false ;
}

SVERROR VolumeCapacityCheckValidator::fix()
{
    return SVS_FALSE ;
}

TargetVolumeAvaialabilityValiadator::TargetVolumeAvaialabilityValiadator( const std::string& name, const std::string& description, const std::list<std::string> targetVolumes, 
                                                                       InmRuleIds ruleId ) : Validator( name, description, ruleId )
    {
    this->targetVolumeList = targetVolumes ;
    }

TargetVolumeAvaialabilityValiadator::TargetVolumeAvaialabilityValiadator( const std::list<std::string> targetVolumes, 
                                                                       InmRuleIds ruleId  ) : Validator( ruleId )
    {
    this->targetVolumeList = targetVolumes ;
    }

SVERROR TargetVolumeAvaialabilityValiadator::evaluate()
    {
        SVERROR returnValue = SVS_OK ;
        InmRuleStatus status = INM_RULESTAT_PASSED ;
        std::stringstream resultStream ;
	    InmRuleErrorCode ruleStatus = RULE_PASSED;
        
        if( CheckVolumesAccess( targetVolumeList, targetVolumeList.size() ) == false )
        {
            returnValue = SVS_FALSE ;
            status = INM_RULESTAT_FAILED ;
            ruleStatus = TARGET_VOLUME_NOTAVAILABLE_ERROR ;
            resultStream << "One of the tempDB volumes are not available at the target" << std::endl ;
            //setRuleStatus( INM_RULESTAT_FAILED, TARGET_VOLUME_NOTAVAILABLE_ERROR, resultStream ) ;
        }
        else
        {
            resultStream << "Corresponding volume at target is available for each tempdb volume at source" << std::endl ;
        }
        setStatus(status) ;
	    setRuleExitCode(ruleStatus);
	    setRuleMessage(resultStream.str());
        return returnValue ;
    }

bool TargetVolumeAvaialabilityValiadator::canfix()
{
    return false ;
}

SVERROR TargetVolumeAvaialabilityValiadator::fix()
{
    return SVS_FALSE ;
}

PageFileVoumeCheck::PageFileVoumeCheck(const std::string& name, 
                                       const std::string& description,
                                       const std::list<std::string>& volList,
                                       InmRuleIds ruleId)
                                       :Validator(name, description, ruleId),
                                       m_volList(volList)
{

}

PageFileVoumeCheck::PageFileVoumeCheck(const std::list<std::string>& volList,
                                           InmRuleIds ruleId)
:Validator(ruleId),
m_volList(volList)
{
    m_volList = volList ;

}
SVERROR PageFileVoumeCheck::evaluate()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR bRet = SVS_FALSE ;
	InmRuleStatus ruleStatus = INM_RULESTAT_NONE ;
	InmRuleErrorCode ruleErrorCode = RULE_STAT_NONE;
	std::stringstream resultStream ;
    std::string volumename;
    bool bPageFileVolume = false;
	std::string systemVolume;
	if(getEnvironmentVariable("SystemDrive", systemVolume) == SVS_OK)
	{
		if(systemVolume.length() == 2)
			systemVolume += "\\";
	}

    std::list<std::string>::iterator volumeIter = m_volList.begin();
    while(volumeIter != m_volList.end())
    {
        volumename = *volumeIter;
        if(IsDrive(volumename))
        {
			if(0 == strcmpi(systemVolume.c_str(),volumename.c_str()))
			{
				DebugPrintf(SV_LOG_INFO," Volume \" %s \" is a system volume. Skipping from pagefile_on_application_volumes check",volumename.c_str());
			}
			else
			{
				DWORD pageDrives = getpagefilevol();
				DWORD dwSystemDrives = 0;
				if('\\' == volumename[0])
					dwSystemDrives = 1 << ( toupper( volumename[4] ) - 'A' );
				else
					dwSystemDrives = 1 << ( toupper( volumename[0] ) - 'A' );

				if(pageDrives & dwSystemDrives)
				{
					resultStream << "The volume ";
					resultStream << volumename ;
					resultStream << "contains pagefile configuration\n";
					DebugPrintf(SV_LOG_DEBUG,"The volume  %s contains pagefile\n",volumename.c_str());
					ruleStatus = INM_RULESTAT_FAILED;
					ruleErrorCode = PAGE_FILE_VOLUME_ERROR;
					bPageFileVolume = true;
				}
			}
        }
        volumeIter++;
    }
    if(!bPageFileVolume)
    {
        resultStream << "No application volumes contains page file configurations";
        DebugPrintf(SV_LOG_DEBUG,"The volume contains pagefile\n");
        ruleStatus = INM_RULESTAT_PASSED;
        ruleErrorCode = RULE_PASSED;
        bRet = SVS_OK;
    }
    setStatus(ruleStatus) ;
    setRuleExitCode(ruleErrorCode);
    setRuleMessage(resultStream.str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool PageFileVoumeCheck::canfix()
{
    return false ;
}


SVERROR PageFileVoumeCheck::fix()
{
    return SVS_FALSE ;
}