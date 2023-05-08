#include "util.h"
#ifdef SV_WINDOWS
#ifndef _WINDOWS_
#include <Windows.h>
#endif
#include <Winsock2.h>
#include <Windns.h>
#include "portablehelpersmajor.h"
#endif
#include <boost/shared_array.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <functional>
#include <iterator>
#include <algorithm>
#include "appexception.h"
#include "config/applocalconfigurator.h"
#include "appscheduler.h"
#include "portablehelpers.h"
#include "inm_md5.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include <ace/OS_Dirent.h>

#define MAX_GROUP_ID 14
#ifndef SV_NEWLINE_CHAR
#ifdef SV_WINDOWS
#define SV_NEWLINE_CHAR "\r\n"
#else
#define SV_NEWLINE_CHAR "\n"
#endif
#endif
std::string convertTimeToString(time_t timeInSec)
{
	time_t t1 = timeInSec;
	char szTime[128];

    if(timeInSec == 0)
        return "0000-00-00 00:00:00";

	strftime(szTime, 128, "%Y-%m-%d %H:%M:%S", localtime(&t1));

	return szTime;
}

void trimSpaces( std::string& s)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "Before Trim %s\n", s.c_str()) ;
	std::string stuff_to_trim = " \n\b\t\a\r\xc" ; 
	
	s.erase( s.find_last_not_of(stuff_to_trim) + 1) ; //erase all trailing spaces
	s.erase(0 , s.find_first_not_of(stuff_to_trim) ) ; //erase all leading spaces
    DebugPrintf(SV_LOG_DEBUG, "After Trim %s\n", s.c_str()) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

// for case insensitive search in a list..

bool isfound( std::list<std::string>& valueList, std::string searchString )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false;
    std::list<std::string>::iterator valueIter = valueList.begin();
    while( valueIter != valueList.end() )
    {
        if(strcmpi( valueIter->c_str(), searchString.c_str() ) == 0)
        {
            bRet = true;
            break;
        }
        valueIter++;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet;
}

bool isFoundInMap(std::map<std::string, std::string>& valueMap, std::string searchKey)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false;
    std::map<std::string, std::string>::iterator valueIter = valueMap.begin();
    if(!searchKey.empty())
    {
        while( valueIter != valueMap.end() )
        {
            if(strcmpi( valueIter->first.c_str(), searchKey.c_str() ) == 0)
            {
                bRet = true;
                break;
            }
            valueIter++;
        }
    }
    else
        bRet = false;
	
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet;
}

std::list<std::string> tokenizeString(const std::string strStringToBeTokenize,std::string delimiter)
{
   using namespace boost;
   char_separator<char> sep(delimiter.c_str());
   std::list<std::string > tokenList;
   tokenizer<char_separator<char> > tokens(strStringToBeTokenize, sep);
   BOOST_FOREACH(std::string t, tokens)
   {
	   tokenList.push_back(t);
   }
   return tokenList;
}

void convertVolumeNames(std::string& volumeName, const bool& toUpper)
{
	if(volumeName.empty() == false)
	{
		std::string volumeLetter = "";
		volumeLetter = volumeName.substr(0,1);
		if(toUpper)
		{
			boost::algorithm::to_upper(volumeLetter);
		}
		else
		{
			boost::algorithm::to_lower(volumeLetter);
		}
		volumeName.replace(0, 1, volumeLetter);
	}
}

void sanitizeVolumePathName(std::string& volumePath)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s \n", FUNCTION_NAME);
	size_t pathSize = volumePath.size();
	if( pathSize == 2 || pathSize == 3 )
	{
		volumePath.erase(1);
	}	
	else if( pathSize > 3 )
	{
		if( volumePath.find_last_of("/\\") == pathSize-1 )
		{
			volumePath.erase(pathSize-1);
		}
	}
	convertVolumeNames(volumePath);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n", FUNCTION_NAME);
}

std::string getFileContents(const std::string &strFileName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    
    SVERROR bRet = SVS_FALSE ;
	ACE_HANDLE handle = ACE_INVALID_HANDLE;
	SVERROR status = SVS_OK;
	std::string strOutPut;
    std::ifstream is;
    SV_ULONG fileSize = 0 ;
    is.open(strFileName.c_str(), std::ios::in | std::ios::binary);
    if( is.good() )
    {
		is.seekg (0, std::ios::end);
        fileSize = is.tellg();
		is.seekg (0, std::ios::beg);
        is.close() ;
    }
    if( fileSize != 0 )
    {
		handle = ACE_OS::open( strFileName.c_str(), O_RDONLY );
		if( handle == ACE_INVALID_HANDLE )
		{
			strOutPut = std::string("Failed to open file %s for reading .",strFileName.c_str());
		}
		else
		{
            size_t infolen;
            INM_SAFE_ARITHMETIC(infolen = InmSafeInt<SV_ULONG>::Type(fileSize) + 1, INMAGE_EX(fileSize))
			boost::shared_array<char> info (new (std::nothrow) char[infolen]);
			if( fileSize == ACE_OS::read(handle, info.get(), fileSize) )
			{
				info[ fileSize ] = '\0';
				strOutPut = info.get();
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to read the entire  file %s. Requested Read size %d\n",strFileName.c_str(), fileSize );
				strOutPut = std::string("Failed to read entire file : %s",strFileName.c_str());
			}
			ACE_OS::close(handle);
		}
       
    }
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "The file %s's size is empty...\n",strFileName.c_str()) ;
		bRet = SVS_OK ;
	}
	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n", FUNCTION_NAME);
	return strOutPut;
}

SVERROR getSizeOfFile( const std::string& filePath, SV_ULONGLONG& fileSize )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	SVERROR retStatus = SVS_OK;
	fileSize = 0 ;
	std::ifstream inputStream;
	inputStream.open( filePath.c_str(), std::ios::in | std::ios::binary );
	if( inputStream.is_open() && inputStream.good() )
	{
		inputStream.seekg(0, std::ios::beg);
		std::ifstream::pos_type startPos = inputStream.tellg();
		inputStream.seekg(0, std::ios::end);
		fileSize  = inputStream.tellg() - startPos;
		inputStream.close();
	}
	else
	{
		retStatus = SVS_FALSE;
		DebugPrintf(SV_LOG_DEBUG, "Failed to open the file: %s \n", filePath.c_str());
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retStatus;
}

void GetValueFromMap(std::map<std::string,std::string>& propertyMap,const std::string& propertyKey,std::string& KeyValue,bool bOtpional)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::stringstream stream;
    if(propertyMap.find(propertyKey) != propertyMap.end())
    {
        KeyValue = propertyMap.find(propertyKey)->second;
        if(KeyValue.compare("0") == 0)
        {
            KeyValue = "";
        }
    }
    else
    {
		KeyValue = "";
        if(bOtpional == false)
        {
            DebugPrintf(SV_LOG_ERROR,"Key %s not found in property map\n",propertyKey.c_str());  
            stream<<propertyKey << " not found in map";
            throw UnknownKeyException(stream.str());
        }
        DebugPrintf(SV_LOG_DEBUG,"Key %s not found in property map and key is optional\n",propertyKey.c_str());  
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


bool CopyConfFile(std::string const & SourceFile, std::string const & DestinationFile)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG,"SourceFile %s\n",SourceFile.c_str());
    DebugPrintf(SV_LOG_DEBUG,"DestinationFile %s\n",DestinationFile.c_str());
    bool rv = true;
    const std::size_t buf_sz = 4096;
    try
    {
        char buf[buf_sz];
        //ACE_HANDLE infile=0, outfile=0;
        ACE_HANDLE infile=ACE_INVALID_HANDLE , outfile=ACE_INVALID_HANDLE;
        ACE_stat from_stat = {0};
        if ( ACE_OS::lstat((const char*) SourceFile.c_str(), &from_stat ) == 0
             && (infile = ACE_OS::open(SourceFile.c_str(), O_RDONLY )) != ACE_INVALID_HANDLE
             && (outfile = ACE_OS::open( DestinationFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC )) != ACE_INVALID_HANDLE )
        {
            ssize_t sz, sz_read=1, sz_write;
            while ( sz_read > 0
                    && (sz_read = ACE_OS::read( infile, buf, buf_sz )) > 0 )
            {
                sz_write = 0;
                do
                {
                    if ( (sz = ACE_OS::write( outfile, buf + sz_write, sz_read - sz_write )) < 0 )
                    {
                        sz_read = sz; // cause read loop termination
                        break;        //  and error to be thrown after closes
                    }
                    sz_write += sz;
                } while ( sz_write < sz_read );
            }
            if ( ACE_OS::close( infile) < 0 )
                sz_read = -1;
            if ( ACE_OS::close( outfile) < 0 )
                sz_read = -1;
            if ( sz_read < 0 )
            {
                rv = false;
            }
        }
        else
        {
            rv = false;
            if ( outfile != ACE_INVALID_HANDLE)
                ACE_OS::close( outfile );
            if ( infile != ACE_INVALID_HANDLE)
                ACE_OS::close( infile );
        }
    }
    catch(...)
    {
        rv =false;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return rv;
}

bool removeChars( const std::string& stuff_to_trim, std::string& s )
{
    bool rv = true;	
	std::string::size_type index = s.find_first_of(stuff_to_trim);
	while( index != std::string::npos )
	{
		std::string leftStr, rightStr;
		leftStr = s.substr( 0, index );
		std::string::size_type index1 = s.find_first_not_of(stuff_to_trim, index);
		if( index1 != std::string::npos )
		{
			rightStr = s.substr(index1);
		}
		s = leftStr;
		s = s + rightStr;
		index = s.find_first_of(stuff_to_trim);
	}
	if( s.at( s.size()-1 ) == '\0' )
	{
		DebugPrintf(SV_LOG_DEBUG, "Removing End of Null Char \n");
		s = s.substr(0, s.size()-1 );
	}
    return rv;
}

std::string getOutputFileName(SV_ULONG policyId,SV_INT groupId,SV_ULONGLONG instanceId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppLocalConfigurator configurator ;
    std::string ouputFile;
	truncateApplicationPolicyLogs(); 
    std::string policyIdStr = boost::lexical_cast<std::string>(policyId);
    if(instanceId == 0)
    {
        instanceId = AppScheduler::getInstance()->getInstanceId(policyId);
    }
    ouputFile = configurator.getApplicationPolicyLogPath() ;
    if(groupId == 0)
    {
        ouputFile += "policy_" + policyIdStr + "_"+ boost::lexical_cast<std::string>(instanceId) + ".log";
    }
    else
    {
        std::string groupIdStr= boost::lexical_cast<std::string>(groupId);
        ouputFile += "policy_" + policyIdStr + "_groupId_" + groupIdStr + "_" + boost::lexical_cast<std::string>(instanceId) + ".log";
    }
    DebugPrintf(SV_LOG_DEBUG,"outputFileName = %s\n",ouputFile.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return ouputFile;
}

std::string getPolicyFileName(SV_ULONG policyId, SV_INT groupId, SV_ULONGLONG instanceId)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	AppLocalConfigurator configurator;
	std::string ouputFile;
	truncateApplicationPolicyLogs();
	std::string policyIdStr = boost::lexical_cast<std::string>(policyId);
	if (instanceId == 0)
	{
		instanceId = AppScheduler::getInstance()->getInstanceId(policyId);
	}
	if (groupId == 0)
	{
		ouputFile = "policy_" + policyIdStr + "_" + boost::lexical_cast<std::string>(instanceId)+".log";
	}
	else
	{
		std::string groupIdStr = boost::lexical_cast<std::string>(groupId);
		ouputFile = "policy_" + policyIdStr + "_groupId_" + groupIdStr + "_" + boost::lexical_cast<std::string>(instanceId)+".log";
	}
	DebugPrintf(SV_LOG_DEBUG, "outputFileName = %s\n", ouputFile.c_str());
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return ouputFile;
}


void getFileNameForUpLoad(SV_ULONG policyId,SV_ULONGLONG instanceId,std::map<SV_INT,std::string>& UpLoadFileName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    AppLocalConfigurator configurator ;
    std::string cachePath;
    std::string ouputFile;
    std::string policyIdStr = boost::lexical_cast<std::string>(policyId);
    cachePath = configurator.getApplicationPolicyLogPath() ;
	ouputFile = cachePath + "policy_" + policyIdStr + "_"+ boost::lexical_cast<std::string>(instanceId) + ".log";
    int statRetVal = -1;
    ACE_stat stat ;
    if(( statRetVal = sv_stat(getLongPathName(ouputFile.c_str()).c_str(), &stat)) == 0)
	{
		DebugPrintf(SV_LOG_WARNING, "Unable to stat %s file \n", ouputFile.c_str()) ; 
        UpLoadFileName.insert(std::make_pair(0,ouputFile));
        DebugPrintf(SV_LOG_DEBUG,"outputFileName = %s\n",ouputFile.c_str());

	}
	else
	{
        std::string groupIdStr;
        for(int i = 1; i<MAX_GROUP_ID;i++)
        {
            ouputFile = "";
            groupIdStr = boost::lexical_cast<std::string>(i);
            ouputFile = cachePath + "policy_" + policyIdStr + "_groupId_" + groupIdStr + "_" + boost::lexical_cast<std::string>(instanceId) + ".log";
            if(( statRetVal = sv_stat(getLongPathName(ouputFile.c_str()).c_str(), &stat)) != 0)
	        {
                DebugPrintf(SV_LOG_WARNING, "Unable to stat %s file \n", ouputFile.c_str()) ; 
		        ouputFile = "";
	        }
            else
            {
                DebugPrintf(SV_LOG_DEBUG,"outputFileName = %s\n",ouputFile.c_str());
                UpLoadFileName.insert(std::make_pair(i,ouputFile));
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool getSupportingApplicationNames(const std::string& serviceName, std::set<std::string>& selectedApplicationNameSet)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRetStatus = false;
    
    AppLocalConfigurator localConfig;
    std::string appNames = localConfig.getSupportedAppNames();
    // here we get the comma seperated application names.
    trimSpaces(appNames);
    std::string tempapNames = appNames;
    std::string::size_type index = std::string::npos;
    while(tempapNames.empty() == false)
    {
        trimSpaces(tempapNames);
        index = tempapNames.find_first_of(",");
        if(index != std::string::npos)
        {
            std::string leftString = tempapNames.substr(0, index);
            if(tempapNames.size() >= index+1)
            {
                tempapNames = tempapNames.substr(index+1);
            }
            else
            {
                break;
            }
            
            trimSpaces(leftString);
            if(isValidApplicationName(serviceName, leftString))
    {
                selectedApplicationNameSet.insert(leftString);   
                bRetStatus = true;
            }
    }
    else
    {
            trimSpaces(tempapNames);
            if(isValidApplicationName(serviceName, tempapNames))
            {
                selectedApplicationNameSet.insert(tempapNames);
                bRetStatus = true;
            }
            break;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;       
    return bRetStatus;
}

bool isDiscoveryApllicable(const std::set<std::string>& appNameSet)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bRet = true;
    if(appNameSet.size() == 1)
    {
        if(strcmpi(appNameSet.begin()->c_str(), "SYSTEM") == 0 || 
           strcmpi(appNameSet.begin()->c_str(), "BULK") == 0  )
        {
            bRet = false;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet;
}

bool isAvailableInTheSet(const std::string& appTypeinStr, const std::set<std::string>& selectedAppNameSet)
{
    bool bRet = true;
    std::set<std::string>::const_iterator selectedAppNameSetBegIter = selectedAppNameSet.begin();
    std::set<std::string>::const_iterator selectedAppNameSetEndIter = selectedAppNameSet.end();
    while(selectedAppNameSetBegIter != selectedAppNameSetEndIter)
    {
        if(strcmpi(selectedAppNameSetBegIter->c_str(), appTypeinStr.c_str()) == 0)
        {
           break; 
        }
        selectedAppNameSetBegIter++;
    }
    if(selectedAppNameSetBegIter == selectedAppNameSetEndIter)
    {
        bRet = false;
    }
    return bRet;
}

void truncateApplicationPolicyLogs ()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	AppLocalConfigurator configurator ;
	std::string applicationPolicyLogPath = configurator.getApplicationPolicyLogPath();
	ACE_DIR * ace_dir = NULL ;
	ACE_DIRENT * dirent = NULL ;
	ace_dir = sv_opendir( applicationPolicyLogPath.c_str()) ;
    if( ace_dir != NULL )
	{
		do
        {
            dirent = ACE_OS::readdir(ace_dir) ;
            if( dirent != NULL )
            {
                std::string fileName = ACE_TEXT_ALWAYS_CHAR(dirent->d_name);
                if( strcmp(fileName.c_str(), "." ) == 0 || strcmp(fileName.c_str(), ".." ) == 0 )
                {
                    continue ;
                }
				std::string totalFilePath = applicationPolicyLogPath.c_str();
				totalFilePath +=  ACE_DIRECTORY_SEPARATOR_CHAR ; 
				totalFilePath += fileName.c_str();
				ACE_HANDLE fileHandle =  ACE_OS::open( totalFilePath.c_str(), O_RDWR );
				if ( ACE_INVALID_HANDLE != fileHandle)
				{
					ACE_stat st;
					ACE_OS::fstat(fileHandle,&st);
					time_t currentTime = ACE_OS::time(); 

					if (currentTime - st.st_ctime > 7 * 24 * 60 * 60 )
					{
						if (ACE_OS::unlink(getLongPathName(totalFilePath.c_str()).c_str()) != 0)
						{
							DebugPrintf(SV_LOG_ERROR, "unlink failed, path = %s, errno = 0x%x\n", totalFilePath.c_str(), ACE_OS::last_error()); 
						}
					}
					ACE_OS::close(fileHandle);
				}
            }
        } while ( dirent != NULL  ) ;
		ACE_OS::closedir( ace_dir ) ;
	}
	DebugPrintf(SV_LOG_DEBUG , "EXITED %s\n", FUNCTION_NAME) ;
	return;
}
std::string generateLogFilePath(const std::string& cmd ) 
{
    std::string outFile ;
    AppLocalConfigurator configurator ;
    ACE_Time_Value time = ACE_OS::gettimeofday() ;
    outFile = configurator.getApplicationPolicyLogPath() ;
    outFile += "AppLog_" ;
    unsigned char computedHash[16];
    INM_MD5_CTX ctx;

    INM_MD5Init(&ctx);
    INM_MD5Update(&ctx, (unsigned char*)cmd.c_str(), cmd.size()) ;
    INM_MD5Final(computedHash, &ctx);
    char *md5chksum = new(std::nothrow)char[INM_MD5TEXTSIGLEN + 1] ;
	// Get the string representation of the checksum
    for (int i = 0; i < INM_MD5TEXTSIGLEN/2; i++) {
		inm_sprintf_s((md5chksum + i * 2), ((INM_MD5TEXTSIGLEN + 1) - (i*2)), "%02X", computedHash[i]);
    }

    outFile += boost::lexical_cast<std::string>(md5chksum);
    delete [] md5chksum;
    outFile += boost::lexical_cast<std::string>( time.sec() ) ;
    outFile += ".log" ;

    return outFile ;
}
#ifdef SV_WINDOWS

void SetupWinSockDLL()
{
  WSADATA InmWsaData;
  memset(&InmWsaData, 0, sizeof(InmWsaData));

  if( WSAStartup( MAKEWORD( 1, 0 ), &InmWsaData ) )
  {
	DebugPrintf( "FAILED Couldn't start Winsock, err %d\n", WSAGetLastError() );
  }
}

bool CheckVolumesAccess(std::list<std::string> &volList, SV_ULONG count )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::list<std::string> activeVollist;
	HANDLE volumeHandle = INVALID_HANDLE_VALUE;
    bool returnValue = true ;
    SV_ULONG currentIndex = 0 ; 

	std::list<std::string>::iterator iterList = volList.begin();
    count = count < volList.size() ? count :  volList.size() ;
    while(  returnValue == true && currentIndex++ < count )
	{
		std::string strVolName = *iterList;
		//Remove " from volume name if exist.
		removeChars("\"",strVolName);
		if(strVolName.length() >2)
		{
			sanitizeVolumePathName(strVolName);
		}
		
        FormatVolumeNameToGuid(strVolName);
		volumeHandle = SVCreateFile((strVolName).c_str(),GENERIC_READ | GENERIC_WRITE ,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
		if(volumeHandle == INVALID_HANDLE_VALUE)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open volume %s handle. Error Code: %d\n",(strVolName).c_str(),GetLastError());
            returnValue = false ;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Successfully opened volume: %s handle.\n",(strVolName).c_str());
			CloseHandle(volumeHandle);
		}    
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return returnValue;
}

bool isValidApplicationName(const std::string& serviceName, const std::string& appName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRetStatus = false; 
    if(strcmpi(serviceName.c_str(), "APP") == 0)
    {
        if( strcmpi(appName.c_str(), "EXCHANGE") == 0 ||
			strcmpi(appName.c_str(), "EXCHANGE2003") == 0 ||
			strcmpi(appName.c_str(), "EXCHANGE2007") == 0 ||
			strcmpi(appName.c_str(), "EXCHANGE2010") == 0 ||
            strcmpi(appName.c_str(), "SQL") == 0 ||
			strcmpi(appName.c_str(), "SQL2000") == 0 ||
			strcmpi(appName.c_str(), "SQL2005") == 0 || 
			strcmpi(appName.c_str(), "SQL2008") == 0 || 
			strcmpi(appName.c_str(), "SQL2012") == 0 || 
            strcmpi(appName.c_str(), "FILESERVER") == 0 ||
            strcmpi(appName.c_str(), "BULK") == 0 || 
            strcmpi(appName.c_str(), "SYSTEM") == 0 ||
            strcmpi(appName.c_str(), "DB2") == 0 ||
			strcmpi(appName.c_str(), "CLOUD") == 0)
        {
            bRetStatus = true;
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "Invaild Application Name: %s \n", appName.c_str());
        }
    }
    else if(strcmpi(serviceName.c_str(), "BACK-UP") == 0)
    {
        if(strcmpi(appName.c_str(), "BULK") == 0 || 
                strcmpi(appName.c_str(), "SYSTEM") == 0)
        {
            bRetStatus = true;
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "Invaild Application Name: %s \n", appName.c_str());
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRetStatus;
}
#else

bool CheckVolumesAccess(std::list<std::string> &activeVollist, SV_ULONG count )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::list<std::string>::iterator activeVollistBegIter = activeVollist.begin();
	std::list<std::string>::iterator activeVollistEndIter = activeVollist.end();
	bool retVal = false ;
	if( activeVollistBegIter != activeVollistEndIter)
	{
		if(IsValidDevfile(*activeVollistBegIter))
		{
			retVal = true ;
		}
		else
		{
			retVal = false ;
			DebugPrintf(SV_LOG_INFO, "This Volume is not accessible %s.", activeVollistBegIter->c_str()); 
		}
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return retVal;
}
std::string getIpAdrress(const std::string& recordName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;	
	std::string ipAddress;
	ipAddress = "";
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return ipAddress;
}

bool isValidApplicationName(const std::string& serviceName, const std::string& appName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRetStatus = false; 
    if(strcmpi(serviceName.c_str(), "APP") == 0)
    {
        if( strcmpi(appName.c_str(), "ARRAY") == 0 ||
            strcmpi(appName.c_str(), "ORACLE") == 0 || 
            strcmpi(appName.c_str(), "BULK") == 0 || 
            strcmpi(appName.c_str(), "SYSTEM") == 0 ||
            strcmpi(appName.c_str(), "DB2") == 0 ||
			strcmpi(appName.c_str(), "CLOUD") == 0)
        {
            bRetStatus = true;
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "Invaild Application Name: %s \n", appName.c_str());
        }
    }
    else if(strcmpi(serviceName.c_str(), "BACK-UP") == 0)
    {
        if(strcmpi(appName.c_str(), "BULK") == 0 || 
           strcmpi(appName.c_str(), "SYSTEM") == 0)
        {
            bRetStatus = true;
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING, "Invaild Application Name: %s \n", appName.c_str());
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;       
    return bRetStatus;
}
#endif

std::string getInmPatchHistory(std::string pszInstallPath)
{
    char line[2001];
    std::fstream fp;
    std::string patchfile, patch_history;
#ifdef SV_WINDOWS
    patchfile = pszInstallPath + PATCH_FILE;
#else
	patchfile = pszInstallPath + "/patch.log";
#endif
    fp.open (patchfile.c_str(), std::fstream::in);

    if(!fp.fail())
    {
		while (true)
		{
			fp.getline(line, sizeof(line));
			if (0 == fp.gcount())
				break;
            if(line[0] == '#' )			
                continue;
            if(patch_history.empty())
                patch_history  = std::string(line) ;
            else 
                patch_history += '|' + std::string(line) ;
        }
        fp.close();
        DebugPrintf(SV_LOG_DEBUG,"PatchHistory= %s\n", patch_history.c_str());		
    }
    else
        DebugPrintf(SV_LOG_DEBUG,"%s File cannot be opened \n", patchfile.c_str());

    return patch_history;
}
void GetLastNLinesFromBuffer(std::string const &strBuffer, int lastNLines, std::string &strFinalNLines)
{

	std::string::size_type idx = std::string::npos;
	std::string::size_type pos = idx;

	int count = 0;
	while (count < lastNLines){
		idx = strBuffer.rfind(SV_NEWLINE_CHAR, pos);
		if (std::string::npos == idx || 0 == idx)
			break;
		pos = idx - 1;

		count++;
	}

	if (std::string::npos == idx) {
		strFinalNLines = strBuffer;
	}
	else {
		strFinalNLines = strBuffer.substr(idx + 1);
	}
}
