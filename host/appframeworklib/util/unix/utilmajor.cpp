#include "utilmajor.h"
#include <sstream>
#include <fstream>
#include <ace/File_Lock.h>
#include "appcommand.h"
#include "config/applocalconfigurator.h"
#include "controller.h"


bool applyLabel(const std::string& deviceName, const std::string& label, const std::string& outPutFileName, std::string& statusMessage)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRetValue = false;
	std::string deviceNameStr = deviceName;
	std::string labelStr = label;
	std::stringstream cmdStream;
	cmdStream << LABEL_CMD << " " << deviceNameStr << " " << labelStr;
    AppCommand objAppCommand(cmdStream.str(), 180, outPutFileName);
	unsigned long exitCode = -1;
	std::string statusMessageStr;
    if(objAppCommand.Run(exitCode, statusMessageStr, Controller::getInstance()->m_bActive) != SVS_OK)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to spawn e2label command:  %s\n", cmdStream.str().c_str());
	}
	else
	{
		if(exitCode ==0)
		{
			DebugPrintf(SV_LOG_INFO,"Successfully applied the label for: %s\n", deviceNameStr.c_str());
			bRetValue = true;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to applly label for: . Error Code :%d\n",deviceNameStr.c_str(), exitCode);
		}
	}
    statusMessage = statusMessageStr;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;	
	return bRetValue;
}

bool makeFileSystem(const std::string& deviceName, const std::string& fileSystemType, const std::string& outPutFileName, std::string& statusMessage)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRetValue = false;
	std::string deviceNameStr = deviceName;
	std::string fsType = fileSystemType;
	AppLocalConfigurator localConfig;
    std::string mkfsShellScriptNamePath = localConfig.getApplicatioAgentCachePath();
	mkfsShellScriptNamePath += "mkfscommand.sh";
	std::ofstream ofs;
	ofs.open(mkfsShellScriptNamePath.c_str(), std::ios::trunc);
	if(ofs.is_open())
	{
		ofs << "#!/bin/sh" <<std::endl ;
		ofs << "echo y | mkfs -t $1 $2" <<std::endl ;
		ofs << "exit $?" <<std::endl ;
		ofs.close();
        std::stringstream chmodCmd;
        chmodCmd << "chmod 777 " << mkfsShellScriptNamePath;
        AppCommand chmodAppCommand(chmodCmd.str(), 0, outPutFileName);
        unsigned long exitCode = -1;
        std::string statusMessageStr;
        if(chmodAppCommand.Run(exitCode, statusMessageStr, Controller::getInstance()->m_bActive) != SVS_OK)
        {
        	DebugPrintf(SV_LOG_ERROR,"Failed to spawn mkfs command:  %s\n", chmodCmd.str().c_str());
     	}
    	else
    	{
        	if(exitCode ==0)
        	{
            	DebugPrintf(SV_LOG_INFO,"Successfully given execute perms to %s \n.", mkfsShellScriptNamePath.c_str());
        	}
        	else
        	{
            	DebugPrintf(SV_LOG_ERROR,"Failed to give execute perms to: %s, ErrorCode :%d\n", mkfsShellScriptNamePath.c_str(), exitCode);
        		statusMessage = "Failed to open file";
        		statusMessage += mkfsShellScriptNamePath;
        		return bRetValue;
        	}
    	}
   }
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to open file %s \n", mkfsShellScriptNamePath.c_str());
		statusMessage = "Failed to open file";
		statusMessage += mkfsShellScriptNamePath;
		return bRetValue;
	}
    std::stringstream cmdStream;	
	cmdStream << mkfsShellScriptNamePath << " " << fsType << " " << deviceNameStr ;
    AppCommand objAppCommand(cmdStream.str(), 0, outPutFileName);
	unsigned long exitCode = -1;
    std::string statusMessageStr;
	if(objAppCommand.Run(exitCode, statusMessageStr, Controller::getInstance()->m_bActive) != SVS_OK)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to spawn mkfs command:  %s\n", cmdStream.str().c_str());
	}
	else
	{
		if(exitCode ==0)
		{
			DebugPrintf(SV_LOG_INFO,"Successfully created filesystem on: %s\n", deviceNameStr.c_str());
			bRetValue = true;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to create filesystem for: . Error Code :%d\n",deviceNameStr.c_str(), exitCode);
		}
	}	
    statusMessage += statusMessageStr;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;	
	return bRetValue;
}

bool isValidNfsPath(const std::string& nfsPath)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRetValue = false;	
	
	bRetValue = true;	 // need to implememt.
	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;	
	return bRetValue;	
}

bool isValidCIFSPath(const std::string& vfsPath)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRetValue = false;	
	
	bRetValue = true;	 // need to implememt.
	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;	
	return bRetValue;	
}
bool mountNFS(const std::string& shareName, const std::string& mountPointName, const std::string& outPutFileName, std::string&  statusMessage)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRetValue = false;
	std::string shareNameStr = shareName;
	std::string mountPointNameStr = mountPointName;
	std::stringstream cmdStream;
	cmdStream << MOUNTNFS_CMD << " " << shareNameStr << " " << mountPointNameStr;
    AppCommand objAppCommand(cmdStream.str(), 180, outPutFileName);
	unsigned long exitCode = -1;
	std::string statusMessageStr;
    if(objAppCommand.Run(exitCode, statusMessageStr, Controller::getInstance()->m_bActive) != SVS_OK)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to spawn mount -t nfs command:  %s\n", cmdStream.str().c_str());
	}
	else
	{
		if(exitCode ==0)
		{
			DebugPrintf(SV_LOG_INFO,"Successfully mounted nfs on: %s\n", shareName.c_str());
			bRetValue = true;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to spawn mount -t nfs command: %s Error Code :%d\n",shareName.c_str(), exitCode);
		}
	}		
    statusMessage = statusMessageStr; 
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;	
	return bRetValue;	
}

bool mountCIFS(const std::string& shareName, const std::string& mountPointName, const std::string& username, const std::string& passwd, const std::string& domainname, const std::string& outPutFileName, std::string&  statusMessage)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRetValue = false;
	std::string shareNameStr = shareName;
	std::string mountPointNameStr = mountPointName;	
	std::string userNameStr = username;
	std::string passwdStr = passwd;
	std::string domainNameStr = domainname;
	std::stringstream cmdStream;
	cmdStream << MOUNTCIFS_CMD << " " << shareNameStr << " " << mountPointNameStr << " -o user=" << domainNameStr << "/" << userNameStr << "%" << passwdStr;
    AppCommand objAppCommand(cmdStream.str(), 180, outPutFileName, true, true);
	unsigned long exitCode = -1;
    std::string statusMessageStr;
	if(objAppCommand.Run(exitCode, statusMessageStr, Controller::getInstance()->m_bActive) != SVS_OK)
	{
	    std::stringstream cmdStreamToLog;
	    cmdStreamToLog << MOUNTCIFS_CMD << " " << shareNameStr << " " << mountPointNameStr << " -o user=" << domainNameStr << "/" << userNameStr;
		DebugPrintf(SV_LOG_ERROR,"Failed to spawn mount -t nfs command:  %s\n", cmdStreamToLog.str().c_str());
	}
	else
	{
		if(exitCode ==0)
		{
			DebugPrintf(SV_LOG_INFO,"Successfully mounted nfs on: %s\n", shareName.c_str());
			bRetValue = true;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to spawn mount -t nfs command: %s Error Code :%d\n",shareName.c_str(), exitCode);
		}
	}		
    statusMessage = statusMessageStr;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;	
	return bRetValue;
}

SVERROR shareFile(const std::string& shareFileName, const std::string& sharePathName)
{
	SVERROR retStatus = SVS_FALSE;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;	
	std::stringstream stream;
	stream << "[" << shareFileName << "]" << std::endl;
	stream << "comment = sharing files" << std::endl; 
	stream << "path = " << sharePathName << std::endl;
    stream << "browseable = yes" << std::endl;
	std::string lockPath =  SAMBA_CONF;
	lockPath += ".lck";
    ACE_File_Lock lock(getLongPathName(lockPath.c_str()).c_str(),O_CREAT | O_RDWR, SV_LOCK_PERMISSIONS,0);
    if(lock.acquire())
	{
		//back up conf file.
		std::ifstream src(SAMBA_CONF, std::ifstream::in);
		std::ofstream dest(SAMBA_CONF_COPY, std::ifstream::out);
		if(src.is_open() && dest.is_open())
		{
		    int length;
			char * buffer;
			src.seekg (0, std::ios::end);
			length = src.tellg();
			src.seekg (0, std::ios::beg);
			// allocate memory:
			buffer = new (std::nothrow) char[length];
  		    // read data as a block:
			if(buffer)
			{
				src.read (buffer, length);
				dest.write (buffer,length); ; // write the content
				
				dest.close (); // close destination file
				src.close (); // close source file
				delete[] buffer;
			}
		}
		else
		{
			 DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s :failed to open %s \n", FUNCTION_NAME, LINE_NO, FILE_NAME, SAMBA_CONF);
		}
		//add entries to conf file
		std::ofstream ofs(SAMBA_CONF, std::ifstream::app);
        if(!ofs.is_open())
        {
            DebugPrintf(SV_LOG_ERROR, "Function %s @LINE %d in FILE %s :failed to open %s \n",  FUNCTION_NAME, LINE_NO, FILE_NAME, SAMBA_CONF);
        }
		else
		{
			ofs << stream.str();
			ofs.close();
			retStatus = SVS_OK;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to obtain lock on %s \n", SAMBA_CONF);
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;	
	return retStatus;
}
