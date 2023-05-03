#include <fstream>
#include <ace/Configuration.h>
#include "util/exportdevice.h"
#include <ace/Configuration_Import_Export.h>
#include "appcommand.h"
#include "localconfigurator.h"
#include "controller.h"
#include "util/common/util.h"
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>

bool StartIscsiService()
{
	bool bRet = false ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::stringstream stream ;
	SV_ULONG exitCode ;
	std::string output ;
	stream << "/etc/init.d/iscsid start" ;
	AppCommand cmd(stream.str(), 300) ;
	cmd.Run(exitCode, output, Controller::getInstance()->m_bActive ) ;
	if( exitCode == 0 )
	{
		DebugPrintf(SV_LOG_DEBUG, "cmd %s succeeded\n", stream.str().c_str()) ;
	}
	else
	{
		DebugPrintf(SV_LOG_WARNING, "cmd %s is failed with error %d\n", stream.str().c_str(), exitCode) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXIED %s\n", FUNCTION_NAME) ;
	return true ;
}

bool UnpersistSession(const std::string& targetIp, const std::string& targetName)
{
	bool bRet = false ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	DebugPrintf(SV_LOG_DEBUG, "EXIED %s\n", FUNCTION_NAME) ;
	return true ;
}

void getSessionId(const std::string& targetIp, const std::string& targetName, std::string& sessionId)
{
	std::stringstream cmdStream ;
	std::string output ;
	SV_ULONG exitCode ;
	DebugPrintf(SV_LOG_DEBUG, "Target Name %s, Target IP %s\n", targetName.c_str(), targetIp.c_str()) ;
	cmdStream << "iscsiadm -m session -P 0" ;
	char line[1024] ;
	Controller::getInstance()->QuitRequested(10) ;
	AppCommand cmd(cmdStream.str(), 300) ;
	cmd.Run(exitCode, output, Controller::getInstance()->m_bActive) ;
	DebugPrintf(SV_LOG_DEBUG, "Command Output %s\n", output.c_str()) ;
	if( exitCode == 0 )
	{
		std::stringstream stream ;
    	stream.str(output) ;
		char line[1024] ;
		while( stream.getline(line, 1024) && strcmp(line, "") != 0 )
		{
			std::string lineStr = line ;
			DebugPrintf(SV_LOG_DEBUG, "line %s\n", lineStr.c_str()) ;
			if( lineStr.find(targetIp) != std::string::npos )
			{
				if( lineStr.find(targetName) != std::string::npos )
				{
					size_t idx1, idx2 ;
					idx1 = lineStr.find('[') ;
					idx2 = lineStr.find(']') ;
					sessionId = lineStr.substr(idx1+1 , idx2 - idx1 - 1) ;
       	       }
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "Session Id %s\n", sessionId.c_str()) ;
}

void getDeviceFromSessionId(const std::string& sessionId, std::string& devName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream cmdStream ;
	SV_ULONG exitCode ;
	std::string output ;
    cmdStream << "iscsiadm -m session -P 3 -r " << sessionId ;
    char line[1024] ;
	Controller::getInstance()->QuitRequested(10) ;
    AppCommand cmd(cmdStream.str(), 300) ;
    cmd.Run(exitCode, output, Controller::getInstance()->m_bActive) ;
    if( exitCode == 0 )
    {
        std::string sessionId ;
		std::stringstream stream ;
    	stream.str(output) ;
        char line[1024] ;
        while( stream.getline(line, 1024) && strcmp(line, "") != 0 )
        {
            std::string lineStr = line ;
            if( lineStr.find("Attached scsi disk") != std::string::npos )
            {
			    size_t idx1 = lineStr.find("Attached scsi disk ") ;
			    size_t idx2 = lineStr.find("State:") ;
			    devName = lineStr.substr(idx1 + strlen("Attached scsi disk "), idx2 - idx1 - strlen("Attached scsi disk ")) ;
            }
        }
	}
	std::string stuff_to_trim = " \n\b\t\a\r\xc" ; 
	devName.erase( devName.find_last_not_of(stuff_to_trim) + 1) ; 
	devName.erase(0 , devName.find_first_not_of(stuff_to_trim) ) ; 
	DebugPrintf(SV_LOG_DEBUG, "Device Name %s is corresponding to session Id %s\n", devName.c_str(), sessionId.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool LoginToTargetSession(const std::string& targetIp, const std::string& targetname, std::string& deviceNameAfterLogin)
{
	bool bRet = false ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::stringstream cmdStream ;
	std::string sessionId ;
	SV_ULONG exitCode;
	std::string output ;

	getSessionId(targetIp, targetname, sessionId) ;
	if( sessionId.compare("") == 0 )
	{
		DebugPrintf(SV_LOG_DEBUG, "There was a no session for target name %s and target ip %s\n", targetname.c_str(), targetIp.c_str()) ;
		cmdStream.str("") ;
		cmdStream << "iscsiadm --mode discovery --type sendtargets --portal " << targetIp ;
		Controller::getInstance()->QuitRequested(10) ;
		AppCommand cmd(cmdStream.str(), 300) ;
		cmd.Run(exitCode, output, Controller::getInstance()->m_bActive) ;
		cmdStream.str("") ;
		cmdStream.clear() ;
		cmdStream << "iscsiadm --mode node --targetname " << targetname << " --portal " << targetIp << " --login" ;
		Controller::getInstance()->QuitRequested(10) ;
		AppCommand cmd1(cmdStream.str(), 300) ;
		cmd1.Run(exitCode, output, Controller::getInstance()->m_bActive) ;
		if( exitCode == 0 )
		{
			getSessionId(targetIp, targetname, sessionId) ;
			bRet = true ;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Attempt to login to target session was failed with error %d\n", exitCode) ;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "The session Id for target name %s and target ip %s is %s\n", targetname.c_str(), targetIp.c_str(), sessionId.c_str()) ;
		bRet = true ;
	}
	getDeviceFromSessionId(sessionId, deviceNameAfterLogin) ;
	if( deviceNameAfterLogin.compare("") == 0 )
	{
		deviceNameAfterLogin = "/dev/" + deviceNameAfterLogin;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXIED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

bool PersistSession(const std::string& targetIp, const std::string& targetname)
{
	return true ;
}

bool UnbinndInitiators(const std::string& targetname, const std::string& tid, const std::string& initiatorIPs)
{
	bool bRet = false ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SV_ULONG exitCode ;
    std::string output ;
    std::stringstream cmdStream ;
    using namespace boost;
    char_separator<char> sep(",") ;
    std::list<std::string > ipAddresses ;
    boost::tokenizer<boost::char_separator<char> > tokens(initiatorIPs, sep);
    BOOST_FOREACH(std::string t, tokens)
    {
        ipAddresses.push_back(t);
    }
	std::list<std::string>::iterator ipIterBeg, ipIterEnd ;
	ipIterBeg = ipAddresses.begin() ;
	ipIterEnd = ipAddresses.end() ;
	while( ipIterBeg != ipIterEnd )
	{
		cmdStream.str("") ;
		cmdStream.clear() ;
	    cmdStream << "tgtadm --lld iscsi --op unbind --mode target --tid " << tid << " -I ";
	    if( ipIterBeg->compare("") == 0 )
    	{
	        cmdStream << " ALL " ;
    	}
	    else
    	{
	        cmdStream << *ipIterBeg ;
    	}
		Controller::getInstance()->QuitRequested(10) ;
    	AppCommand appCmd(cmdStream.str(), 300) ;
	    appCmd.Run(exitCode, output, Controller::getInstance()->m_bActive);
		if( exitCode == 0 )
		{
			DebugPrintf(SV_LOG_DEBUG, "Initiator unbind was done for the initiator ips %s\n", ipIterBeg->c_str()) ;
			bRet = true ;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Initiator unbind was failed for the initiator ips %s\n", ipIterBeg->c_str()) ;	
			bRet = false ;
			break ;
		}
		ipIterEnd++ ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXIED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

bool RemoveTargetIfRequired(const std::string& targetname, const std::string& tid)
{
	bool bRet = false ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	DebugPrintf(SV_LOG_DEBUG, "EXIED %s\n", FUNCTION_NAME) ;
	return true ;
}

bool LogoutFromTargetSession(const std::string& targetIP, const std::string& targetname )
{
	bool bRet = false ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::stringstream cmdStream ;
	SV_ULONG exitCode ;
	std::string output ;
	cmdStream << "iscsiadm --mode node --targetname " << targetname << " --portal " << targetIP << " --logout" ;
	Controller::getInstance()->QuitRequested(10) ;
    AppCommand cmd1(cmdStream.str(), 300) ;
    cmd1.Run(exitCode, output, Controller::getInstance()->m_bActive) ;
    if( exitCode == 0 )
    {
    	bRet = true ;
    }
    else
    {
    	DebugPrintf(SV_LOG_ERROR, "Attempt to login to target session was failed with error %d\n", exitCode) ;
    }
	DebugPrintf(SV_LOG_DEBUG, "EXIED %s\n", FUNCTION_NAME) ;
	return bRet ;
}


bool StartTgtdService()
{
	bool bRet = false ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bRet = true ;
	SV_ULONG exitCode ;
	std::string output ;
	std::stringstream stream ;
	stream << "/etc/init.d/tgtd start" ;
	AppCommand cmd(stream.str(), 300) ;
	cmd.Run(exitCode, output, Controller::getInstance()->m_bActive);
	if( exitCode == 0 )
	{
		DebugPrintf(SV_LOG_DEBUG, "cmd %s succeeded\n", stream.str().c_str()) ;
	}
	else
	{
		DebugPrintf(SV_LOG_WARNING, "cmd %s is failed with error %d\n", stream.str().c_str(), exitCode) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return true ;
}

bool CreateTargetIfRequired(const std::string& targetName, const std::string& tid, const std::string& deviceName)
{
	bool bRet = false ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::stringstream cmdStream ;
	std::string findExpression ;
	findExpression = "Target " ;
	findExpression += tid ;
	findExpression += ":"  ;
	findExpression += " " ;
	findExpression += targetName ;

	cmdStream << "tgtadm --lld iscsi --mode target --op show" ;
	DebugPrintf(SV_LOG_DEBUG, "tgtadm command %s and Find Expression %s\n", cmdStream.str().c_str(), findExpression.c_str()) ;

	Controller::getInstance()->QuitRequested(10) ;
	AppCommand appCommand(cmdStream.str(), 300) ;
	SV_ULONG exitCode ; 
	std::string output ;
	appCommand.Run(exitCode, output, Controller::getInstance()->m_bActive) ;
	
	if( exitCode == 0 )
	{
		if( output.find(findExpression)	!= std::string::npos )
		{
			DebugPrintf(SV_LOG_DEBUG, "Target %s with tid %s is present in the system\n", targetName.c_str(), tid.c_str()) ;
			bRet = true ;
		}
		else
		{
			cmdStream.str("") ;
			cmdStream.clear() ;
			cmdStream << "tgtadm --lld iscsi --op new --mode target --tid " << tid << " -T " << targetName ;
			DebugPrintf(SV_LOG_DEBUG, "Creating the target %s with tid %s\n", targetName.c_str(), tid.c_str()) ;
			Controller::getInstance()->QuitRequested(10) ;
			AppCommand appCommand(cmdStream.str(), 300) ;
			appCommand.Run(exitCode, output, Controller::getInstance()->m_bActive ) ;
			if( exitCode == 0 )
			{
				DebugPrintf(SV_LOG_DEBUG, "Created new iscsi target %s with tid %s\n", targetName.c_str(), tid.c_str()) ;
				cmdStream.str("") ;
				cmdStream.clear() ;
				cmdStream << "tgtadm --lld iscsi --op new --mode logicalunit --tid "<< tid <<  " --lun " << 1 << "  --backing-store " << deviceName ;
				Controller::getInstance()->QuitRequested(10) ;
				AppCommand appCommand(cmdStream.str(), 300) ;
				appCommand.Run(exitCode, output, Controller::getInstance()->m_bActive) ;
				if( exitCode == 0 )
				{
					DebugPrintf(SV_LOG_DEBUG, "Added lun with id %d to target %s with backing store %s\n", tid.c_str(), targetName.c_str(), deviceName.c_str()) ;
					bRet = true ;
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "adding logical unit to target is failed\n") ;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "cmd %s failed with exit code %d\n", cmdStream.str().c_str(), exitCode) ;
			}
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "command %s failed with exit code %d\n", cmdStream.str().c_str(), exitCode ) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}


bool BindInitiators(const std::string& targetName, const std::string& tid, const std::string& ipaddresses)
{
	bool bRet = false ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SV_ULONG exitCode ;
	std::string output ;
	std::stringstream cmdStream ;
    using namespace boost;
    char_separator<char> sep(",") ;
    std::list<std::string > ipAddresses ;
    boost::tokenizer<boost::char_separator<char> > tokens(ipaddresses, sep);
    BOOST_FOREACH(std::string t, tokens)
    {
        ipAddresses.push_back(t);
    }
    std::list<std::string>::iterator ipIterBeg, ipIterEnd ;
    ipIterBeg = ipAddresses.begin() ;
    ipIterEnd = ipAddresses.end() ;
    while( ipIterBeg != ipIterEnd )
    {
		cmdStream.str("") ;
		cmdStream.clear(); 
		cmdStream << "tgtadm --lld iscsi --op unbind --mode target --tid " << tid << " -I ";
		if( ipaddresses.compare("") == 0 )
		{
			cmdStream << " ALL " ;
		}
		else
		{
			cmdStream << *ipIterBeg;
		}
		Controller::getInstance()->QuitRequested(10) ;
		AppCommand appCmd(cmdStream.str(), 300) ;
		appCmd.Run(exitCode, output, Controller::getInstance()->m_bActive);
		cmdStream.str("") ;
		cmdStream.clear() ;
	    cmdStream << "tgtadm --lld iscsi --op bind --mode target --tid " << tid << " -I ";
   		if( ipaddresses.compare("") == 0 )
	    {
   		    cmdStream << " ALL " ;
	    }
   		else
	    {
   		    cmdStream << *ipIterBeg;
   		 }
		Controller::getInstance()->QuitRequested(10) ;
		AppCommand appCmd1(cmdStream.str(), 300) ;
		appCmd1.Run(exitCode, output, Controller::getInstance()->m_bActive) ;
		if( exitCode == 0 ) 
		{
			DebugPrintf(SV_LOG_DEBUG, "Enabling the target to accept specific initiators\n"); 
			bRet = true ;
		}	
		else
		{
			DebugPrintf(SV_LOG_ERROR, "failed to enable the target %s to accept initiators %s\n", tid.c_str(), ipIterBeg->c_str()) ;
			bRet = false ;
			break ;
		}
		ipIterBeg++ ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

bool PersistExportDetails(const std::string& targetName, const std::string& tid, const std::string& ipaddresses)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::stringstream cmd ;
	cmd << "tgt-admin --dump" ;
	std::string output ;
	SV_ULONG exitCode ;
	AppCommand Persistcmd(cmd.str(), 300) ;
	Persistcmd.Run(exitCode, output, Controller::getInstance()->m_bActive) ;
	std::cout<<output ;
	if( exitCode == 0 )
	{
		DebugPrintf(SV_LOG_ERROR, "Updating the /etc/tgt/targets.conf with current details\n") ;
		std::ofstream outfile("/etc/tgt/targets.conf"); 
		outfile << output ;
		outfile.close() ;
		bRet = true ;
	}
	else
	{
		DebugPrintf(SV_LOG_WARNING, "Failed to Persist the target session information. error %d\n", exitCode) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXIED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

bool ExportDeviceUsingIscsi(const std::string& deviceName, DeviceExportParams& exportParams, std::stringstream& stream)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ;
	std::string initiatorIPs, targetName, Tid ;
	std::string  mountpoint, sessionId, fstype,targetIp ;
	
	std::string operation ;
	

	if( exportParams.m_Exportparams.find("InitiatorIPs") != exportParams.m_Exportparams.end() )
	{
		initiatorIPs = exportParams.m_Exportparams.find("InitiatorIPs")->second ;
	}

	if( exportParams.m_Exportparams.find("TargetName") != exportParams.m_Exportparams.end() )
	{
		targetName = exportParams.m_Exportparams.find("TargetName")->second ;
	}

	if( exportParams.m_Exportparams.find("Tid") != exportParams.m_Exportparams.end() )
	{
		Tid = exportParams.m_Exportparams.find("Tid")->second ;
	}
	
	if( exportParams.m_Exportparams.find("Operation") != exportParams.m_Exportparams.end() )
	{
		operation = exportParams.m_Exportparams.find("Operation")->second ;
	}

	if( exportParams.m_Exportparams.find("MountPoint") != exportParams.m_Exportparams.end() )
	{
		mountpoint = exportParams.m_Exportparams.find("MountPoint")->second ;
	}

	if( exportParams.m_Exportparams.find("SessionId") != exportParams.m_Exportparams.end() )
	{
		sessionId = exportParams.m_Exportparams.find("SessionId")->second ;
	}

	if( exportParams.m_Exportparams.find("FSType") != exportParams.m_Exportparams.end() )
	{
		fstype = exportParams.m_Exportparams.find("FSType")->second ;
	}

	if( exportParams.m_Exportparams.find("TargetIP") != exportParams.m_Exportparams.end() )
	{
		targetIp = exportParams.m_Exportparams.find("TargetIP")->second ;
	}
	DebugPrintf(SV_LOG_DEBUG, "Initator IPs %s\n", initiatorIPs.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "target name %s\n", targetName.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "tid %s\n", Tid.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "operation %s\n", operation.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "mount point %s\n", mountpoint.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "session id %s\n", sessionId.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "fstype %s\n", fstype.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "targetip %s\n", targetIp.c_str()) ;
	if( operation.compare("Export") == 0 )
	{
		if( targetName.compare("") == 0 ||
			Tid.compare("") == 0 ||
			deviceName.compare("") == 0 )
		{
			DebugPrintf(SV_LOG_ERROR, "One of the Non-optional parameters doesn't have proper value\n") ;
		}
		else
		{
			if( StartTgtdService() )
			{
				if( CreateTargetIfRequired(targetName, Tid, deviceName) )
				{
					if( BindInitiators(targetName, Tid, initiatorIPs) )
					{
						if( PersistExportDetails(targetName, Tid, initiatorIPs) )
						{
							bRet = true ;
						}
					}
				}
			}
		}
	}
	else if( operation.compare("Login") == 0 )
	{
		std::string deviecNameAfterLogin ;
		if( targetName.compare("") == 0 ||
			targetIp.compare("") == 0 )
		{
			DebugPrintf(SV_LOG_ERROR, "One of the Non-optional parameters doesn't have proper value\n") ;
		}
		else
		{
			if( StartIscsiService() )
			{
				if( LoginToTargetSession(targetIp, targetName, deviecNameAfterLogin) )
				{
					if( PersistSession(targetIp, targetName) )
					{
						if( fstype.compare("") != 0 &&
							mountpoint.compare("") != 0 )
						{
							SV_ULONG exitCode ;
							std::string output ;
							std::stringstream stream ;
							stream << "mount -t " << fstype << " " << deviecNameAfterLogin << " " << mountpoint ;
							Controller::getInstance()->QuitRequested(10) ;
							AppCommand cmd(stream.str(), 100) ;
							cmd.Run(exitCode, output, Controller::getInstance()->m_bActive) ;
							if( exitCode == 0 )
							{
								bRet = true ;
							}
						}
						else
						{
							DebugPrintf(SV_LOG_DEBUG, "Didnt find any mount point to mount the exported device %s \n", deviceName.c_str()) ;
							bRet = true ;
						}
					}
				}
			}
		}
	}
	else if(operation.compare("Disconnect") == 0 )
	{
		if( Tid.compare("") == 0 )
		{
			DebugPrintf(SV_LOG_ERROR, "One of the Non-optional parameters doesn't have proper value\n") ;	
		}
		else
		{
			if( UnbinndInitiators(targetName, Tid, initiatorIPs) )	
			{
				if( RemoveTargetIfRequired(targetName, Tid) )
				{
					bRet = true ;
				}
			}
		}
	}
	else if(operation.compare("Logout") == 0 )
	{
		if( targetName.compare("") == 0 ||
			targetIp.compare("") == 0 )
		{
			DebugPrintf(SV_LOG_ERROR, "One of the Non-optional parameters doesn't have proper value\n") ;
		}
		else
		{
			if( LogoutFromTargetSession(targetIp, targetName ) )
			{
				if( UnpersistSession(targetIp, targetName) )
				{
					bRet = true ;
				}
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

void GetSmbConfFilePath(std::string& confPath)
{
	confPath = "/etc/samba/smb.conf" ;
}

bool UpdateSmbConfFile(const std::string& path, const std::string& shareName)
{
	bool bRet = false ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool RemoveCIFSShare(const std::string& path, const std::string& shareName, std::stringstream& stream)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool ok = false ;
	std::string smbConfFile ;
	GetSmbConfFilePath(smbConfFile) ;
    ACE_Configuration_Heap m_inifile ;
	if( m_inifile.open() == 0 )
	{
    	ACE_Ini_ImpExp importer( m_inifile );
	    if( importer.import_config(ACE_TEXT_CHAR_TO_TCHAR("/etc/samba/smb.conf")) == 0 )
    	{
			ACE_Configuration_Section_Key sectionKey;
			if( m_inifile.remove_section( m_inifile.root_section(), shareName.c_str(), true ) == 0 )
			{
				DebugPrintf(SV_LOG_DEBUG, "The cifs share %s is removed from the smb conf file\n", shareName.c_str()) ;	
				ok = true ;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to remove the cifs share %s from the smb conf file\n", shareName.c_str()) ;
				stream << "Failed to remove the share information from the smb.conf file for " << shareName ;
			}
			if( ok )
			{
				if(importer.export_config(smbConfFile.c_str()) ==  0)
				{
					ok = true ;
				}
				else
        	    {
        	    	DebugPrintf(SV_LOG_ERROR, "Failed to export the /etc/samba/smb.conf\n") ;
					stream <<"Failed to persist information in smb.conf file after removing the share " << shareName ;
		    	}
			}
		}
	}
	else
	{
		stream << "Failed to import the smb.conf file" ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return ok ;
}

bool CreateCIFSShare(const std::string& path, const std::string& shareName, std::stringstream& stream)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool ok = false ;
	std::string smbConfFile ;
	GetSmbConfFilePath(smbConfFile) ;
	ACE_Configuration_Heap m_inifile ;
	if( m_inifile.open() == 0 )
	{
		ACE_Ini_ImpExp importer( m_inifile );
		if( importer.import_config(ACE_TEXT_CHAR_TO_TCHAR("/etc/samba/smb.conf")) == 0 )
		{
			ACE_Configuration_Section_Key sectionKey; 
			if(m_inifile.open_section( m_inifile.root_section(), ACE_TEXT_CHAR_TO_TCHAR("global"), true , sectionKey ) == 0)
			{
				ACE_TString value;
				if( m_inifile.get_string_value( sectionKey, "security", value) == 0 )
				{
					if( value.compare("share") != 0 )
					{
						if( m_inifile.set_string_value( sectionKey, "security", "share" )  ==  0 ) 
						{
							DebugPrintf(SV_LOG_DEBUG, "The security key value is changed to share\n") ;
							if(importer.export_config(smbConfFile.c_str()) <  0)
							{
								DebugPrintf(SV_LOG_ERROR, "Failed to export the /etc/samba/smb.conf\n") ;
								stream << "Failed to export the /etc/samba/smb.conf" ;
		    				}
							else
							{
								ok = true ;
							}
    					}
						else
						{
							DebugPrintf(SV_LOG_DEBUG, "Failed to se the security to share\n") ;
						 	stream << "Unable to set the security key value to share in section global." ;
						}
					}
					else
					{
						ok = true ;
						DebugPrintf(SV_LOG_DEBUG, "security key value is already set to share\n") ;
					}
					if( ok )
					{
						ok = false ;
						//add the share
						ACE_Configuration_Section_Key shareSectionKey;
						if( m_inifile.open_section( m_inifile.root_section(), shareName.c_str(), true, shareSectionKey) == 0 )
						{
							if( m_inifile.set_string_value( shareSectionKey, "path", path.c_str()  )  ==  0 )							
							{
								if( m_inifile.set_string_value( shareSectionKey, "guest ok", "yes")  ==  0 )							
								{
									if( m_inifile.set_string_value( shareSectionKey, "writable", "no")  ==  0 )							
									{
										if( m_inifile.set_string_value( shareSectionKey, "browsable", "no")  ==  0 )							
										{
											if( m_inifile.set_string_value( shareSectionKey, "hosts allow", "")  ==  0 )							
											{
												ok = true ;
											}
										}
									}
								}
							}
						}
						else
						{
							stream << "Failed to create/update the section for the share "<< shareName << " in smb.conf file" ;
						}
					}
					if( ok )
					{
						DebugPrintf(SV_LOG_DEBUG, "Exporting the smb.conf file after updating the share info\n") ;
						if(importer.export_config(smbConfFile.c_str()) ==  0)
						{
							ok = true ;
						}
						else
                        {
                           DebugPrintf(SV_LOG_ERROR, "Failed to export the /etc/samba/smb.conf\n") ;
							stream << "Failed to persist the share details in smb.conf file" ;
                        }
					}
				}
		    }		
			else
			{	
				DebugPrintf(SV_LOG_ERROR, "Could not read the section global\n") ;
				stream << "Failed open the section global in smb.conf file" ;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "ACE_Ini_ImpExp import_config is failed\n") ;
			stream << "Failed to import the smb.conf file. Check whether the file is in proper format or not" ;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "ACE_Configuration_Heap is Failed\n") ;
		stream << "Failed to import the smb.conf file." ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return ok ;
}

bool ExportDeviceUsingCIFS(const std::string& deviceName, const DeviceExportParams& exportParams, std::stringstream& stream)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ;
	std::string path, shareName ;
	bool exportOp ;

	bool  validParams = false ;
	if( exportParams.m_Exportparams.find("LocalPathName") != exportParams.m_Exportparams.end() )
	{
		path = exportParams.m_Exportparams.find("LocalPathName")->second ;	
		DebugPrintf(SV_LOG_DEBUG, "Local path Name %s\n", path.c_str()) ;
		if( exportParams.m_Exportparams.find("ShareName") != exportParams.m_Exportparams.end() )
		{
			shareName = exportParams.m_Exportparams.find("ShareName")->second ;
			DebugPrintf(SV_LOG_DEBUG, "Share Name %s\n", shareName.c_str()) ;
			if( exportParams.m_Exportparams.find("Operation") != exportParams.m_Exportparams.end() )
			{
				std::string operation = exportParams.m_Exportparams.find("Operation")->second ;
				if( operation.compare("Export") == 0 )
				{
					exportOp = true ;		
				}
				else
				{
					exportOp = false ;
				}
				validParams = true ;
			}	
		}
		else
		{
			stream << "ShareName is not found in the ExportParams. The device " << deviceName << " cannot be exported using cifs" ;
		}
	}
	else
	{
		stream << "LocalPath is not found in the ExportParams. The device " << deviceName << " cannot be exported using cifs" ;
	}
	if( validParams )
	{
		if( exportOp )
		{
			bRet = CreateCIFSShare(path, shareName, stream) ;
		}
		else
		{
			bRet = RemoveCIFSShare(path, shareName, stream) ;	
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}


bool RemoveNFSShare(const std::string& localPath, std::stringstream& stream)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ;
	std::ifstream file ;
	file.open("/etc/exports", std::ifstream::in) ;
	std::string exportEntry ;
	exportEntry = localPath ;
	exportEntry += "\t*(ro,insecure,all_squash)" ;
	std::stringstream exportsStream ;
	if( file.good() )
	{
		while( !file.eof() )
		{
			char line[1024] ;
			file.getline(line, 1024) ;
			std::string lineStr = line ;
			DebugPrintf(SV_LOG_DEBUG, "Line %s and export Entry %s\n", lineStr.c_str(), exportEntry.c_str()) ;
			if( lineStr.compare(exportEntry) == 0 )
			{
				bRet = true ;
			}
			else
			{
				exportsStream << lineStr << std::endl ;
			}
		}
		file.close() ;
	}
	DebugPrintf(SV_LOG_DEBUG, "Stream before writing %s\n", exportsStream.str().c_str()) ;
	if( bRet )
	{
		std::ofstream ofile ;
		ofile.open("/etc/exports", std::ifstream::trunc|std::ifstream::out) ;
		ofile<< exportsStream.str() ; 
		ofile<< std::endl ;
		ofile.close() ;
		bRet = true ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

bool CreateNFSShare(const std::string& localPath, std::stringstream& stream)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ;
	std::ifstream file ;
	file.open("/etc/exports", std::ifstream::in) ;
	std::string exportEntry ;
	exportEntry = localPath ;
	exportEntry += "\t*(ro,insecure,all_squash)" ;
	if( file.good() )
	{
		while( !file.eof() )
		{
			char line[1024] ;
			file.getline(line, 1024) ;
			std::string lineStr = line ;
			if( lineStr.compare(exportEntry) == 0 )
			{
				bRet = true ; //No need to add the entry in the /etc/exports file
			}
		}
		file.close() ;
	}
	
	if( bRet == false )
	{
		std::ofstream ofile ;
		ofile.open("/etc/exports", std::ifstream::app|std::ifstream::out) ;
		ofile<< "\n" ;
		ofile<< exportEntry ;
		bRet = true ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}
bool ExportDeviceUsingNFS(const std::string& deviceName, const DeviceExportParams& exportParams, std::stringstream& stream)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ;
	std::string path, shareName ;
	bool exportOp ;

	bool  validParams = false ;
	if( exportParams.m_Exportparams.find("LocalPathName") != exportParams.m_Exportparams.end() )
	{
		path = exportParams.m_Exportparams.find("LocalPathName")->second ;	
		DebugPrintf(SV_LOG_DEBUG, "Local path Name %s\n", path.c_str()) ;
		if( exportParams.m_Exportparams.find("Operation") != exportParams.m_Exportparams.end() )
		{
			std::string operation = exportParams.m_Exportparams.find("Operation")->second ;
			if( operation.compare("Export") == 0 )
			{
				exportOp = true ;		
			}
			else
			{
				exportOp = false ;
			}
			validParams = true ;
		}	
	}
	else
	{
		stream << "LocalPath is not found in the ExportParams. The device " << deviceName << " cannot be exported using cifs" ;
	}
	if( validParams )
	{
		if( exportOp )
		{
			bRet = CreateNFSShare(path, stream) ;
			//AppCommand nfsstart("/etc/init.d/nfs restart", 300) ;
            AppCommand exportfs( "exportfs -a", 300 ) ;
			SV_ULONG exitCode ;
			std::string output ;
			exportfs.Run(exitCode, output, Controller::getInstance()->m_bActive) ;
			if( exitCode == 0 )
			{
				DebugPrintf(SV_LOG_DEBUG, "Started nfs service on the local host\n") ;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Failure in starting smb service on the local host. Error %d Error String:%s\n", exitCode, output.c_str()) ;
			}
			/*AppCommand nfsreload("/etc/init.d/nfs reload", 300) ;
			if( exitCode == 0 )
			{
				DebugPrintf(SV_LOG_DEBUG, "reloaded the exports\n") ;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "failure to reload the exports %d\n", exitCode) ;
			}*/
		}
		else
		{
			bRet = RemoveNFSShare(path,stream) ;	
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

