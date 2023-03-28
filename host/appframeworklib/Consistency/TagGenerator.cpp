#include "TagGenerator.h"
#include <ace/OS_NS_sys_time.h>
#include <sstream>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include "appscheduler.h"
#ifdef SV_WINDOWS
#include "system_win.h"
#endif
#include "appcommand.h"


#ifdef SV_WINDOWS
	#include "../common/hostagenthelpers_ported.h" 
	#include "../Log/logger.h"
#endif
SV_ULONGLONG TagGenerator::m_LastSuccessfulBkpTime = 0 ;
SV_ULONGLONG TagGenerator::m_FullBkpInterval = 0 ;
bool TagGenerator::m_isFullbackupRequired = false ;
bool TagGenerator::m_isFullbackPropertyChecked = false ;

ACE_Recursive_Thread_Mutex TagGenerator::m_lock ;

void TagGenerator::sanitizeVacpCommmand(std::string &strVacpCommand)
{
	 DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	 size_t index = strVacpCommand.find_first_of(" ");
	 std::string vacpBinName = "";
#ifdef SV_WINDOWS
	vacpBinName = "vacp.exe";
#else
	vacpBinName = "vacp";
#endif
	 if(index != std::string::npos)
	 {
		 strVacpCommand = strVacpCommand.substr(index+1,strVacpCommand.length());
		 index = strVacpCommand.find(vacpBinName.c_str());

		 if(index != std::string::npos) //not needed but keeping to ensure sanity
		 {
			strVacpCommand = strVacpCommand.substr(index+ strlen(vacpBinName.c_str()),strVacpCommand.length());
		 }
	 }

	 DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
std::string TagGenerator::getVacpPath()
{
	 DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	 std::string strVacpAbsolutePath ;
	 strVacpAbsolutePath += m_objLocalConfigurator.getInstallPath();
	
	 strVacpAbsolutePath += ACE_DIRECTORY_SEPARATOR_CHAR ;
#ifdef SV_WINDOWS
	 OSVERSIONINFOEX osVersionInfo ;
	 getOSVersionInfo(osVersionInfo) ;
	// if Product is installed on Windows XP
	 if((osVersionInfo.dwMajorVersion == 5) && (osVersionInfo.dwMinorVersion == 1))
	 {
		 strVacpAbsolutePath += std::string("vacpxp.exe");
	 }
	 //If Product is installed on Windows 2000
	 else if((osVersionInfo.dwMajorVersion == 5) && (osVersionInfo.dwMinorVersion == 0))
	 {
		 strVacpAbsolutePath += "vacp2k.exe";
	 }
	 else
	 {
		 strVacpAbsolutePath += "vacp.exe";
	 }
	 
#else
	strVacpAbsolutePath += ACE_DIRECTORY_SEPARATOR_CHAR ;
	strVacpAbsolutePath += "bin" ;
	strVacpAbsolutePath += ACE_DIRECTORY_SEPARATOR_CHAR ;
    strVacpAbsolutePath += "vacp" ;
#endif
	 DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	 return strVacpAbsolutePath;
}
//#ifdef SV_WINDOWS
//SVERROR TagGenerator::listVssProps(std::list<VssProviderProperties>& providersList)
//{
//	SVERROR retStatus = SVS_OK;
//    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
//	//AutoGuard guard(m_lock) ;
//	listVssProviders(providersList);
//	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
//	return retStatus;
//}
//#endif
/* => Reasons for commenting the above code:
		 1: Due to the lock, doscovery controler thread is blocking for long times as other threads holds the lock while issuing tags.
		    But this function is not spawnig vacp.
		 2: This function is not touching any member-data, and not calling any function which use vacp.
*/
bool TagGenerator::issueConsistenyTag(const std::string &outputFileName,SV_ULONG &exitCode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	AutoGuard guard(m_lock) ;
	bool bRetValue = false;
	sanitizeVacpCommmand(m_strCommandToExecute);
	//If "-t" and "-verify" both are present,remove -t and its tag name
	verifyVacpCommand(m_strCommandToExecute);
	//Append time stamp to tagname
	appendTimeStampToTagName(m_strCommandToExecute);
	exitCode = 1 ;
	std::string strCommnadToExecute ;
	strCommnadToExecute += std::string("\"");
	strCommnadToExecute += std::string(getVacpPath());
	strCommnadToExecute += std::string("\"");
	strCommnadToExecute += std::string(" ");
	strCommnadToExecute += m_strCommandToExecute;

	m_strCommandToExecute = std::string("");
	m_strCommandToExecute = strCommnadToExecute;

	time_t ltime;
	struct tm *today;
	time( &ltime );
	today = gmtime( &ltime );
	bool fullbackup = false ;
	if( !m_isFullbackPropertyChecked )
	{
		m_isFullbackupRequired = m_objLocalConfigurator.IsFullBackupRequired() ;
		m_FullBkpInterval = m_objLocalConfigurator.GetFullBackupInterval() ;
		m_isFullbackPropertyChecked = true ;
	}
	LocalConfigurator configurator ;
	if( m_isFullbackupRequired )
	{
		if( m_LastSuccessfulBkpTime == 0 )
		{
			m_LastSuccessfulBkpTime = configurator.GetLastFullBackupTimeInGmt() ;
		}
		if( m_LastSuccessfulBkpTime == 0 )
		{
			m_LastSuccessfulBkpTime = ltime ;
		}
		DebugPrintf(SV_LOG_DEBUG, "Last successful backup time " ULLSPEC "and current time " ULLSPEC "m_FullBkpInterval%ld\n", m_LastSuccessfulBkpTime, ltime, m_FullBkpInterval ) ;
		if( ltime > ( m_LastSuccessfulBkpTime + m_FullBkpInterval ) )
		{
			fullbackup = true ;
			m_strCommandToExecute += " -f" ;
		}
	}
	DebugPrintf(SV_LOG_INFO,"\n The vacp command to execute : %s\n",m_strCommandToExecute.c_str());
	
	std::string strOutputFile;
	bool bIsTempFile = false;
	if(outputFileName.empty())
	{
		strOutputFile = generateLogFilePath(m_strCommandToExecute);
		bIsTempFile = true;
	}
	else
	{
		strOutputFile = outputFileName;
	}
    ACE_OS::unlink(outputFileName.c_str()) ;
    AppCommand objAppCommand(m_strCommandToExecute,m_uTimeOut,strOutputFile,true);
	void *h = 0;
#ifdef SV_WINDOWS
	h = Controller::getInstance()->getDuplicateUserHandle();
#endif
    if(objAppCommand.Run(exitCode,m_stdOut, Controller::getInstance()->m_bActive, "", h) != SVS_OK)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to spawn vacp binary.\n");
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Successfully spawned the vacp binary. \n");
		bRetValue = true;
	}
#ifdef SV_WINDOWS
	if(h)
	{
		CloseHandle(h);
	}
#endif
	if( exitCode == 0 && fullbackup )
	{
		configurator.SetLastFullBackupTimeInGmt(ltime) ;
		m_LastSuccessfulBkpTime = ltime ;
	}
	if(bIsTempFile)
	{
		ACE_OS::unlink(strOutputFile.c_str());
	}
	DebugPrintf(SV_LOG_INFO,"\n The exit code from vacp process is : %d",exitCode);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRetValue;
}

void TagGenerator::getTagName(const std::string &strVacpCommand, std::string& tagName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string leftString = "";
    std::string rightString = "";
    std::string::size_type index = strVacpCommand.find(" -t ");
    if(index == std::string::npos)
    {
        index = strVacpCommand.find(" -T ");
    }
    if(index != std::string::npos)
    {
        std::string::size_type index2 = strVacpCommand.find(" -", index+2);
        if(index2 != std::string::npos)
        {
            tagName = strVacpCommand.substr(index+4, index2-(index+4));
        }
        else
        {
            tagName = strVacpCommand.substr(index+4, strVacpCommand.size()-(index+4));
        }

        // DebugPrintf(SV_LOG_DEBUG, "Index %d Index2 %d\n", index, index2);
    }

    DebugPrintf(SV_LOG_DEBUG, "TAG_NAME = %s\n", tagName.c_str());
}

void TagGenerator::getDeviceListStr(const  std::string & strVacpCommand, std :: string & devices)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string leftString = "";
    std::string rightString = "";
    std::string::size_type index = strVacpCommand.find("-v ");
    if(index == std::string::npos)
    {
        index = strVacpCommand.find("-V ");
    }
    if(index != std::string::npos)
    {
        std::string::size_type index2 = strVacpCommand.find(" -", index+2);
        if(index2 != std::string::npos)
        {
            devices = strVacpCommand.substr(index+3, index2-(index+3));
        }
        else
        {
            devices = strVacpCommand.substr(index+3, strVacpCommand.size()-(index+3));
        }

        // DebugPrintf(SV_LOG_DEBUG, "Index %d Index2 %d\n", index, index2);
    }

    DebugPrintf(SV_LOG_DEBUG, "DEVICES for VACP = %s\n", devices.c_str());
}

bool TagGenerator::issueConsistenyTagForOracle(const std::string &outputFileName, const std::string &dbName, SV_ULONG &exitCode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AutoGuard guard(m_lock) ;
    bool bRetValue = false;
    sanitizeVacpCommmand(m_strCommandToExecute);
    //If "-t" and "-verify" both are present,remove -t and its tag name
    verifyVacpCommand(m_strCommandToExecute);
    //Append time stamp to tagname
    std::string tag, devices;
    getDeviceListStr(m_strCommandToExecute, devices);

    appendTimeStampToTagName(m_strCommandToExecute);
    getTagName(m_strCommandToExecute, tag);

    
    exitCode = 1 ;
    std::string strCommnadToExecute ;
    strCommnadToExecute += m_objLocalConfigurator.getInstallPath();
    strCommnadToExecute += "/scripts/oracle_consistency.sh consistency ";//std::string("\"");
    strCommnadToExecute += dbName;
    strCommnadToExecute += std::string(" ");
    strCommnadToExecute += tag;
    strCommnadToExecute += std::string(" ");
    strCommnadToExecute += devices;

    m_strCommandToExecute = std::string("");
    m_strCommandToExecute = strCommnadToExecute;
    

    DebugPrintf(SV_LOG_INFO,"\n The consistency command to execute : %s\n",m_strCommandToExecute.c_str());
    ACE_OS::unlink(outputFileName.c_str()) ;
    AppCommand objAppCommand(m_strCommandToExecute,m_uTimeOut,outputFileName, true);
	void *h = 0;
#ifdef SV_WINDOWS
	h = Controller::getInstance()->getDuplicateUserHandle();
#endif
    if(objAppCommand.Run(exitCode,m_stdOut, Controller::getInstance()->m_bActive, "", h) != SVS_OK)
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to spawn consistency script.\n");
    }
    else
    {
        DebugPrintf(SV_LOG_INFO,"Successfully spawned the consistency script. \n");
        bRetValue = true;
    }
#ifdef SV_WINDOWS
	if(h)
	{
		CloseHandle(h);
	}
#endif
    DebugPrintf(SV_LOG_INFO,"\n The exit code from consistency script is : %d\n",exitCode);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRetValue;
}

bool TagGenerator::issueConsistenyTagForDb2(const std::string &outputFileName, const std::string &instance, const std::string &dbName, SV_ULONG &exitCode)
{
  DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
  AutoGuard guard(m_lock) ;
  bool bRetValue = false;
  sanitizeVacpCommmand(m_strCommandToExecute);
  //If "-t" and "-verify" both are present,remove -t and its tag name
  verifyVacpCommand(m_strCommandToExecute);
  //Append time stamp to tagname
  std::string tag, devices;
  getDeviceListStr(m_strCommandToExecute, devices);

  appendTimeStampToTagName(m_strCommandToExecute);
  getTagName(m_strCommandToExecute, tag);
  
  exitCode = 1 ;
  std::string strCommnadToExecute ;
  strCommnadToExecute += m_objLocalConfigurator.getInstallPath();
  strCommnadToExecute += "/scripts/db2_consistency.sh consistency ";//std::string("\"");
  strCommnadToExecute += instance;
  strCommnadToExecute += std::string(" ");
  strCommnadToExecute += dbName;
  strCommnadToExecute += std::string(" ");
  strCommnadToExecute += tag;
  strCommnadToExecute += std::string(" ");
  strCommnadToExecute += devices;
  m_strCommandToExecute = std::string("");
  m_strCommandToExecute = strCommnadToExecute;
  
  DebugPrintf(SV_LOG_INFO,"\n The consistency command to execute : %s\n",m_strCommandToExecute.c_str());
  ACE_OS::unlink(outputFileName.c_str()) ;
  AppCommand objAppCommand(m_strCommandToExecute,m_uTimeOut,outputFileName, true);
  void *h = 0;
#ifdef SV_WINDOWS
  h = Controller::getInstance()->getDuplicateUserHandle();
#endif
  if(objAppCommand.Run(exitCode,m_stdOut, Controller::getInstance()->m_bActive, "", h) != SVS_OK)
  {
   DebugPrintf(SV_LOG_ERROR,"Failed to spawn consistency script.\n");
  }
  else
  {
   DebugPrintf(SV_LOG_INFO,"Successfully spawned the consistency script. \n");
   bRetValue = true;
  }
#ifdef SV_WINDOWS
  if(h)
  {
   CloseHandle(h);
  }
#endif
  DebugPrintf(SV_LOG_INFO,"\n The exit code from consistency script is : %d\n",exitCode);
  DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
  return bRetValue;
}

void TagGenerator::appendTimeStampToTagName(std::string &strVacpCommand)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

	ACE_Time_Value aceTv = ACE_OS::gettimeofday();
	std::string leftString = "";
	std::string rightString = "";
	std::string tagName; 
	getTagName(strVacpCommand, tagName );

	std::string::size_type index = strVacpCommand.find(" -t ");
	if(index == std::string::npos)
	{
		index = strVacpCommand.find(" -T ");
	}
	if(index != std::string::npos && !tagName.empty())
	{
		DebugPrintf(SV_LOG_DEBUG, "Before: %s\n", strVacpCommand.c_str());
		std::string::size_type posTagStart = index+4;
		if(strVacpCommand.length() > posTagStart)
		{
			strVacpCommand.erase(posTagStart, tagName.length());
			trimSpaces(tagName);
			std::string local_time;
			timeInReadableFormat(local_time);
#ifdef SV_WINDOWS
			tagName = "\"" + tagName + "_" + local_time + "\"";
#else
			tagName += "_" + local_time;
#endif
			strVacpCommand.insert(posTagStart,tagName);
		}
		DebugPrintf(SV_LOG_DEBUG, "After: %s\n", strVacpCommand.c_str());
/*		leftString = strVacpCommand.substr(0, index + 4);
		rightString = strVacpCommand.substr((index + 4) , strVacpCommand.size());
		std::string doubleQuote = "\"";
		std::stringstream vacpCommandStream;
		std::string local_time;
		timeInReadableFormat(local_time); 
		std::string::size_type pos =  rightString.find (tagName); 
		if (pos != std::string::npos)
		{
			rightString.insert(pos ,doubleQuote); 
			local_time.append(doubleQuote);
		}
		vacpCommandStream << leftString << rightString << "_" << local_time;
		strVacpCommand = vacpCommandStream.str();
		DebugPrintf (SV_LOG_DEBUG , "Left String is %s \n ",leftString.c_str() ); 
		DebugPrintf (SV_LOG_DEBUG , "Right String is %s \n ", rightString.c_str());*/
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, " -t option is not found or tag is not specified. VACP Command: %s..\n ", strVacpCommand.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


void TagGenerator::verifyVacpCommand(std::string &strVacpCommand)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	DebugPrintf(SV_LOG_DEBUG, "Constistency command to be verified: %s\n", strVacpCommand.c_str()) ;
	std::string lowerVacpCommand = strVacpCommand;
	boost::algorithm::to_lower(lowerVacpCommand) ;
	if( lowerVacpCommand.find("-verify") != std::string::npos )
	{
		std::string::size_type index;
		index = lowerVacpCommand.find(" -x ");
		if(index != std::string::npos)
		{
			std::string leftString = strVacpCommand.substr(0, index+1);
			std::string::size_type index2 = strVacpCommand.find("-", index+2);
			if(index2 != std::string::npos)
			{
				std::string rightString = strVacpCommand.substr(index2, strVacpCommand.size());
				strVacpCommand = leftString + rightString;
			}
			else
			{
				strVacpCommand = leftString;
			}
		}			
		index = strVacpCommand.find(" -t ");
		if(index == std::string::npos)
		{
			index = strVacpCommand.find(" -T ");
		}
		if(index != std::string::npos)
		{
			std::string leftString = strVacpCommand.substr(0, index+1);
			DebugPrintf(SV_LOG_DEBUG, "VACP-ISSUE:leftString for -t: %s\n", leftString.c_str()) ;
			//std::string::size_type index2 = strVacpCommand.find("-", index+2);
			std::string::size_type index2 = strVacpCommand.rfind("-");
			if(index2 != std::string::npos)
			{
				std::string rightString = strVacpCommand.substr(index2, strVacpCommand.size());
				DebugPrintf(SV_LOG_DEBUG, "VACP-ISSUE:rightString for -t: %s\n", rightString.c_str()) ;
				strVacpCommand = leftString + rightString;
				DebugPrintf(SV_LOG_DEBUG, "VACP-ISSUE:total string for -t: %s\n", strVacpCommand.c_str()) ;
			}
			else
			{
				strVacpCommand = leftString;
			}
		}		
	}
	else
	{
		std::string::size_type index = lowerVacpCommand.find(" -x ");
		if(index != std::string::npos)
		{
			index = lowerVacpCommand.find(" -w ");
			if(index != std::string::npos)
			{
				std::string leftString = strVacpCommand.substr(0, index+1);
				std::string::size_type index2 = strVacpCommand.find("-", index+2);
				if(index2 != std::string::npos)
				{
					std::string rightString = strVacpCommand.substr(index2, strVacpCommand.size());
					strVacpCommand = leftString + rightString;
				}
				else
				{
					strVacpCommand = leftString;
				}
			}			
			index = strVacpCommand.find(" -a ");
			if(index == std::string::npos)
			{
				index = strVacpCommand.find(" -A ");
			}
			if(index != std::string::npos)
			{
				std::string leftString = strVacpCommand.substr(0, index+1);
				std::string::size_type index2 = strVacpCommand.find("-", index+2);
				if(index2 != std::string::npos)
				{
					std::string rightString = strVacpCommand.substr(index2, strVacpCommand.size());
					strVacpCommand = leftString + rightString;
				}
				else
				{
					strVacpCommand = leftString;
				}
			}		
		}
		else
		{
			// continueeee.
		}
	}	
	DebugPrintf(SV_LOG_DEBUG, "Verified Constistency Command: %s\n", strVacpCommand.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool timeInReadableFormat(std::string & local_time)
{
	bool bRet;
	time_t rawtime;
	char time_in_readable_format [80];
	struct tm * timeinfo;
	if ( ACE_OS::time ( &rawtime ) != -1) 
	{
		timeinfo = ACE_OS::localtime ( &rawtime );
		if ( ACE_OS::strftime (time_in_readable_format,80,"%Y-%m-%d-%H:%M:%S",timeinfo) == 0 )
			bRet = false;
	}
	else 
	{
		bRet = false;
	}
	local_time = time_in_readable_format;
	return bRet;
}
