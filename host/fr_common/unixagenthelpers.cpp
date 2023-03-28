#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <dirent.h>
#include <fnmatch.h>
#include <time.h>
#include <fstream>
#include <sys/socket.h>
#include <string>
#include <sstream>
#include <vector>
#include "inmsafecapis.h"
#include "inmageex.h"

#ifdef USE_STATFS
#define statvfs statfs
#define <sys/mount.h>
#else
#include <sys/statvfs.h>
#endif
#ifdef HAVE_NETINET_H 
#include <netinet/in.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef SV_SUNOS
#include <sys/systeminfo.h>
#endif
#include "hostagenthelpers_ported.h"
#include "../fr_config/svconfig.h"

#include "svutils.h"
#include "../fr_log/logger.h"
#include "version.h"
#include <libgen.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <compatibility.h>

#include <sys/utsname.h>

#ifdef LINUX_OS
#include <sys/ioctl.h>
#include<stdio.h>
#include <net/if.h>
#include <arpa/inet.h>
#include<string.h>
#endif

#include "inmsafeint.h"

#define FX_VERSION_FILE "/usr/local/.fx_version"
#define PATCH_FILE	"/patch.log"

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif
#ifndef OSINFOCMDDEF
#define OSINFOCMDDEF
const char OSINFOCMD[] =  "uname -rsv";
#define OSINFOLENGTH 500
#endif
#define UUID_STRING_LENGTH 36
#define BUFSIZE  512
#define	EOS				'\0'
#define	RANGE_MATCH		(1)
#define	RANGE_NOMATCH	(0)
#define	RANGE_ERROR		(-1)
char g_szEmptyString[] = "";
SSH_Configuration ssh_config;
int getFreePort_impl(int port);
bool bEnableEncryption ;
int RegisterHostIntervel = 0;
using namespace std;

#if (defined(SV_SUN_5DOT8) || defined(SV_SUN_5DOT9))
//This function neither takes a filter nor sorts the files. It just lists the files in the directory.
int get_dir_entries(char const* pszPathName,std::vector<std::string> &vFilesList);

int get_dir_entries(char const* pszPathName,std::vector<std::string> &vFilesList)
{
  DIR * dirp = NULL;
  size_t direntsize = offsetof(struct dirent, d_name) + pathconf(pszPathName, _PC_NAME_MAX) + 1;
  struct dirent * dentp, * dentresult;
  dentresult = NULL;
  dentp = (struct dirent *) calloc(direntsize, 1);
  if(!dentp)
  {
	DebugPrintf(SV_LOG_ERROR,"Cannot allocate memory to read and store directory entries.\n");
	return -1;
  }

  dirp = opendir(pszPathName);
  if(!dirp)
  {
	DebugPrintf(SV_LOG_ERROR,"\nCould not open the direcotry %s",pszPathName);
    return -1;
  }

 while( (0 == readdir_r(dirp,dentp, &dentresult)) && dentresult)
 {
     vFilesList.push_back(dentp->d_name);
 }
 free(dentp);
 closedir(dirp);

 return vFilesList.size();  
}
#endif

int DoesRegExpRangeMatch(const char *srchptrn, char verify, char **ptrcurr)
{
DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	char eachchar;
    char tmpchar;
    int fine;
    int reverse;

    if ((reverse = (*srchptrn == '!' || *srchptrn == '^')))
    {
        ++srchptrn;
    }
    fine = 0;
    eachchar = *srchptrn++;
    do {
        if (eachchar == '\\')
        {
            eachchar = *srchptrn++;
        }
        if (eachchar == '\0')
        {
            return -1;
        }
        if ((*srchptrn == '-') && ((tmpchar = *(srchptrn+1)) != '\0') && (tmpchar != ']')) 
        {
            srchptrn += 2;
            if (tmpchar == '\\' )
            {
                tmpchar = *srchptrn++;
            }
            if (tmpchar == '\0')
            {
                return -1;
            }
            if (eachchar <= verify && verify <= tmpchar)
            {
                fine = 1;
            }
        } 
        else if (eachchar == verify)
        {
            fine = 1;
        }
    } while ((eachchar = *srchptrn++) != ']');
    *ptrcurr = (char *)(srchptrn);
DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return ((fine == reverse)?0:1);
}



bool RegExpMatch(const char *srchptrn, const char *input)
{

DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    char *ptr;
    char temp;
    const char *psource;
    char ch;

    for (psource = input;;)
    {
        ch = *srchptrn++;
	if(ch == '\0')
	{
		DebugPrintf(SV_LOG_DEBUG,"  returning with Pattern matchin with '\0' \n");
		return (*input == '\0');
	}
	else if(ch == '\\')
	{
		 DebugPrintf(SV_LOG_DEBUG,"  Pattern matching with '\\ so ignorning ' \n");
		 if ((ch = *srchptrn++) == '\0') 
                {
                    ch = '\\';
                    --srchptrn;
                }
		
	}
	else if(ch == '*')
	{
		DebugPrintf(SV_LOG_DEBUG,"  Pattern matching with * \n");
		ch = *srchptrn;
		while (ch == '*')
             	{
                	ch = *++srchptrn;
             	}
             	if ((*input == '.') && (input == psource))
             	{
                 	return false;
             	}
             	if (ch == '\0') 
             	{
                	return true;
             	}	
             	while ((temp = *input) != '\0') 
             	{
                    if (RegExpMatch(srchptrn, input))
                    {
			return true;
                    }
                    ++input;
             	}
                return false;
		
	}
	else if(ch == '?')
	{
		DebugPrintf(SV_LOG_DEBUG,"  Pattern matching with '?' \n");
		if (*input == '\0')
                {
                    return false;
                }
                if ((*input == '.') && (input == psource))
                {
                    return false;
                }
                ++input;
	}
	else if(ch == '[')
	{
		DebugPrintf(SV_LOG_DEBUG,"  Pattern matching with '[' \n");
		if (*input == '\0')
                {
                    return false;
                }
                if ((*input == '.') && (input == psource))
                {
                    return false;
                }
                switch (DoesRegExpRangeMatch(srchptrn, *input,&ptr)) 
                {
                    case -1:
                        goto regular;
                    case 1:
                        srchptrn = ptr;
                        break;
                    case 0:
                        return false;
                }
                ++input;
		
	}		
	else
	{
regular:		
	    if (ch != *input )
            {
                return false;
            }
            ++input;
		
		
	}	
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

//end
#if 1

SVERROR SetStanbyCxValuesInConfigFile(const char *pszServerName,int port)
{
       
        SVConfig* conf = SVConfig::GetInstance();
        SVERROR er =SVS_OK;
        char portno[10];
        string newhostname;
		string newport;
        memset(portno,0,10);
        string value;
        newhostname = pszServerName;
        er = conf->GetValue(KEY_SV_SERVER,value);
        if(!er.failed() )
        {
               
                DebugPrintf(SV_LOG_DEBUG," SetStanbyCxValuesInConfigFile server = %s\n",value.c_str());
        }
        string cursec;
        conf->UpdateConfigParameter(KEY_SV_SERVER,newhostname);
        DebugPrintf( SV_LOG_DEBUG,"[UnixAgentHelper::SetStanbyCxValuesInConfigFile()] Host Name = %s\n",newhostname.c_str());
        conf->Write();
        value.erase();
        er = conf->GetValue(KEY_SV_SERVER,value);
        if(!er.failed() )
        {
               
               DebugPrintf(SV_LOG_DEBUG," SetStanbyCxValuesInConfigFile server = %s\n",value.c_str());
        }
       
		inm_sprintf_s(portno,ARRAYSIZE(portno),"%d",port);
		newport +=portno; 
        conf->UpdateConfigParameter(KEY_SV_SERVER_PORT,newport);
		 conf->Write();
		 value.erase();
		 er = conf->GetValue(KEY_SV_SERVER_PORT,value);
		if(!er.failed() )
		{
			DebugPrintf(SV_LOG_DEBUG," SetStanbyCxValuesInConfigFile Port = %d\n",atoi(value.c_str()));
        }
         
        return (er);
}

#endif
		

string getInstallationDirectory()
{
	static string installationDirectory;
	
	if(!installationDirectory.empty())		
		return installationDirectory;
	
	SVERROR er;
	SVConfig* iconf;
	
	iconf = SVConfig::GetInstance();
	iconf->Parse(FX_VERSION_FILE);    
	er = iconf->GetValue("INSTALLATION_DIR", installationDirectory);
	
	if(er.failed())
		DebugPrintf(SV_LOG_ERROR,"[UnixAgentHelpers::RegisterHost():getInstallationDirectory()] INSTALLATION_DIR key not present in FX_VERSION_FILE");  
	
	return installationDirectory;    
}

string getLocalTime()
{
	char szLocalTime[90];
	tm *today;
	time_t ltime;
	
	time( &ltime );
	today = localtime(&ltime);

	inm_sprintf_s(szLocalTime, ARRAYSIZE(szLocalTime), "20%02d-%02d-%02d %02d:%02d:%02d",
			today->tm_year - 100,
			today->tm_mon + 1,
			today->tm_mday,
			today->tm_hour,
			today->tm_min,
			today->tm_sec		
	       );
	DebugPrintf(SV_LOG_DEBUG,"[UnixAgentHelpers::RegisterHost():getLocalTime()] AgentLocalTime = %s\n", szLocalTime);

	return string(szLocalTime);
}

string getTimeZone()
{
	static string zone;
	if(!zone.empty())
		return zone;
	int  zonediff;
	char zonesign;
	char zonebuf[10];
	time_t t, lt, gt; 

	t  = time(NULL);
	lt = mktime (localtime(&t));
	gt = mktime (gmtime(&t));
	zonediff = (int)difftime(lt, gt);

	zonesign = (zonediff >= 0)? '+' : '-';
	inm_sprintf_s(zonebuf, ARRAYSIZE(zonebuf), "%c%.2d%.2d", zonesign, abs(zonediff) / 3600, abs(zonediff) % 3600 / 60);
	zone = string(zonebuf);
	DebugPrintf(SV_LOG_DEBUG,"[UnixAgentHelpers::RegisterHost():getTimeZone()] AgentZone = %s\n", zonebuf);
	return zone;
}

// 3.5.2: Commented due to unavailability of "statvfs.h" in MAC OS
// Uncomment in 4.1 and also 3.5.2 
// Change build script for Mac Os ADD:USE_STATFS
void getInstallVolumeParameters(string installVolume, SV_ULONGLONG* insVolCapacity, SV_ULONGLONG* insVolFreeSpace)
{
	struct statvfs insVolBuf;
	if ( statvfs((char*)installVolume.c_str(),&insVolBuf) < 0) 
	{
		if(insVolCapacity)
			*insVolCapacity  = 0;
		if(insVolFreeSpace)
			*insVolFreeSpace = 0;		
		DebugPrintf(SV_LOG_ERROR,"FAILED: Unable to open install Volume: %s\n", installVolume.c_str());
	}
	else
	{
		if(insVolCapacity)
			*insVolCapacity = (SV_ULONGLONG)insVolBuf.f_blocks * (SV_ULONGLONG) insVolBuf.f_bsize; 
		if(insVolFreeSpace)
			*insVolFreeSpace= (SV_ULONGLONG)insVolBuf.f_bavail * (SV_ULONGLONG) insVolBuf.f_bsize;
	}
}

// Patch History
// Now read patch.log which is present in installation directory along with svagents.exe
// Prepare patch_history with fields seperated by ',' & records seperated by '|'
string getPatchHistory(string pszInstallPath)
{
	char line[2001];
	std::fstream fp;
	string patchfile, patch_history;
	
	patchfile = pszInstallPath + PATCH_FILE;
	fp.open (patchfile.c_str(), fstream::in);
	
	if(!fp.fail())
	{
		while(fp.getline(line, sizeof(line)) && !fp.fail())
		{			
			if(line[0] == '#' )			
				continue;
			if(patch_history.empty())
				patch_history  = string(line) ;
			else 
				patch_history += '|' + string(line) ;
		}
		fp.close();
		DebugPrintf(SV_LOG_DEBUG,"[UnixAgentHelpers::RegisterHost()]PatchHistory= %s\n", patch_history.c_str());		
	}
	else
		DebugPrintf(SV_LOG_DEBUG,"[UnixAgentHelper::RegisterHost()] %s File cannot be opened \n", patchfile.c_str());
	
	return patch_history;
}

/*
 //The system call dirname changes the input param, hence make a copy of the   
 //param pszFullName before calling this function.   
 SVERROR DirName( const char *pszFullName, char *pszDirName )   
 {   
         // place holder function
 }
*/
char* getHomeDir()
{
	return ssh_config.ssh_homedir_path;
}

SV_ULONG SVGetFileSize( char const* pszPathname, OPTIONAL SV_ULONG* pHighSize )
{
	// BUGBUG: should set pHighSize if not null
     struct stat file;
     if(!stat(pszPathname,&file))
     {
         return file.st_size;
     }
     else return SV_INVALID_FILE_SIZE;
}

bool SVIsFilePathExists( char const* pszPathname )
{
     struct stat file;
     if(!stat(pszPathname,&file))
         return true;
     else 
		 return false;
}

SVERROR DeleteOlderMatchingFiles( char const* pszPath, char const* pszPattern, time_t timestamp )
{
	int n = 0;
	int counter = 0;
	struct dirent **namelist;
    struct stat	statinfo;

#if (defined(SV_SUN_5DOT8) || defined(SV_SUN_5DOT9))
	
	std::vector<std::string>vDirectoryFiles;
	n = get_dir_entries(pszPath, vDirectoryFiles); 

#elif defined(SV_SUNOS)

	n = scandir(pszPath, &namelist, 0, 0); 

#elif defined(LINUX_OS)

    n = scandir(pszPath, &namelist, 0, alphasort);

#endif

	counter = n;
	int remCode = 0;
	if(n < 0)
	{
	  DebugPrintf(SV_LOG_ERROR,"FAILED Cannot get files from directory %s, error = %d\n",pszPath,errno);
	  return -1;
	}
	else
	{
	  while(n--) 
      {
		#if (defined(SV_SUN_5DOT8) || defined(SV_SUN_5DOT9))
          int mt= fnmatch(pszPattern,vDirectoryFiles[n].c_str(),FNM_PATHNAME|FNM_PERIOD );
		#else
		  int mt= fnmatch(pszPattern,namelist[n]->d_name,FNM_PATHNAME|FNM_PERIOD );
		#endif
          if(mt == 0)
          {
              char path[PATH_MAX];
              memset(path,0,PATH_MAX);
              inm_strcpy_s(path,ARRAYSIZE(path),pszPath);
              inm_strcat_s(path,ARRAYSIZE(path),"/");
			  #if (defined(SV_SUN_5DOT8) || defined(SV_SUN_5DOT9))
				inm_strcat_s(path,ARRAYSIZE(path),vDirectoryFiles[n].c_str());
			  #else
				inm_strcat_s(path,ARRAYSIZE(path),namelist[n]->d_name);
			  #endif
              if(!stat(path,&statinfo))
              {
                  time_t mtime = statinfo.st_mtime;
                  if(mtime < timestamp)
                  {
                      DebugPrintf(SV_LOG_DEBUG,"[DeleteOlderMatchingFiles] Going to delete file %s\n",path);
                      remCode = remove(path);
					  if(remCode == -1)
					  {
						DebugPrintf(SV_LOG_ERROR,"Failed to remove the file. Error = %d\n",errno);
					  }
                  }
              }
          }
      }
	}
#if (!(defined(SV_SUN_5DOT8) || defined(SV_SUN_5DOT9)))
	//free up the memory allocated for the dirent strucutres
	counter-=1;
	while(counter >= 0)
	{	  
	  free((namelist[counter]));
	  counter-=1;
	}
#endif
	return SVS_OK;
}

#ifdef LINUX_OS
const char *GetFirstActiveInetAddr()
{
  struct ifreq ifr={0};
  int fd = 0;
  static char netAddr[128];
  char interface[5];
  int interface_idx = 0;

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  for(; interface_idx < 8; interface_idx++) {
	  inm_sprintf_s(interface, ARRAYSIZE(interface), "eth%d", interface_idx);
          if (fd >= 0) {
                inm_strcpy_s(ifr.ifr_name,ARRAYSIZE(ifr.ifr_name), interface) ;
                ifr.ifr_addr.sa_family = AF_INET;
                if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
                  const char *addr = (const char *)inet_ntop(AF_INET,
                                (void *)(&((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr),
                                netAddr, sizeof(netAddr));
                  close(fd);
                  return addr;

                }
        }
        continue;
  }
  close(fd);
  string hostname;
  const char *addr;
  GetProcessOutput("hostname", hostname);

  struct hostent *hp = gethostbyname(hostname.c_str());
  if (hp == NULL) {
       DebugPrintf(SV_LOG_WARNING, "gethostbyname failed to determine the IP Address..\n");
       DebugPrintf(SV_LOG_WARNING, "Please check /etc/hosts file has proper entries..\n");
       return "127.0.0.1";
    } else {
       unsigned int i=0;
       while ( hp -> h_addr_list[i] != NULL) {
          addr = inet_ntoa( *( struct in_addr*)( hp -> h_addr_list[i]));
          i++;
       }
    }
    return addr;
}
#endif

/** Updates The actual Os_type according to our Build Os Name*/
void trimStringSpaces( std::string& s )
{
	std::string stuff_to_trim = " \n\b\t\a\r\xc" ; 
	s.erase( s.find_last_not_of(stuff_to_trim) + 1) ; //erase all trailing spaces
	s.erase(0 , s.find_first_not_of(stuff_to_trim) ) ; //erase all leading spaces
}
bool isLocalMachine32Bit( std::string machine)
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	std::string uname_machine = machine;
    DebugPrintf( SV_LOG_DEBUG, "Machine Vaue %s\n", machine.c_str() );
	trimStringSpaces( uname_machine );
	bool bis32BitMachine = false;
    int i = 0;
    for( ;i < 10;i++ )
    {
        std::stringstream sub;
        sub<< "i" << i << "86";
        if( uname_machine.find( sub.str() ) != std::string::npos )
        {
            bis32BitMachine = true;
			break;
        }
    }
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return bis32BitMachine;
}
/*#ifdef SV_SUNOS
bool isLocalMachine32Bit( )
{
    DebugPrintf( SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	bool bis32BitMachine = false;
    const int COUNT = 257;	
    char info[COUNT] = {'\0'};
    if( sysinfo( SI_ARCHITECTURE_64, info, COUNT ) == -1 )
    {
        bis32BitMachine = true;
		DebugPrintf( SV_LOG_DEBUG, "This is 32 bit machine \n" );
	} 
    else
	{
		std::string architech(info);
	    DebugPrintf( SV_LOG_DEBUG, "This is 64 bit machine %s\n", architech.c_str() );
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return bis32BitMachine;
}
#endif */
SVERROR findOsRelese( std::string filePath, std::string& osRelease )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	SVERROR status = SVS_OK;   
    do
	{
		int intStat; 
		struct stat stFileInfo; 
		intStat = stat( filePath.c_str(),&stFileInfo );
		if( intStat != 0 )
		{
			DebugPrintf(SV_LOG_WARNING, "The %s is either not existed or not readable.. \n", filePath.c_str() );
			status = SVS_FALSE;
			break;
		}	
		fstream filestr ( filePath.c_str(), fstream::in );
		char * buffer;
		int buffer_length = 0;    
		filestr.seekg (0, ios::end);
		buffer_length = filestr.tellg();
		filestr.seekg (0, ios::beg);
        size_t alloclen;
        INM_SAFE_ARITHMETIC(alloclen = InmSafeInt<int>::Type(buffer_length) + 1, INMAGE_EX(buffer_length))
		buffer = new char [alloclen];
		memset(buffer,'\0',	buffer_length + 1);	
		filestr.read (buffer,buffer_length ); 
		osRelease =	buffer;
		delete[] buffer;
		filestr.close();
	 }while( false );
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ ) ;
	return status ;
}
void getLinuxMachineOsType( std::string machineValue, std::string releaseValue, std::string& OS )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	OS="RHEL";
	bool bis32BitMachine;
	bis32BitMachine = isLocalMachine32Bit( machineValue );
	
	std::string strRHEL4BaseVerRegExp1 = "Red Hat Enterprise Linux ES release 4 (Nahant Update [4-9])";
	std::string strCentOS4BaseVerRegExp1 = "CentOS release 4.[4-9] (Final)";

	//Added Support for Red Hat Enterprise Linux [AE]S release 4 
	std::string strRHEL4AdvVerRegExp1 = "Red Hat Enterprise Linux AS release 4 (Nahant Update [4-9])";
	
	//Read the following two strings from /etc/enterprise-release
	std::string strOEL5BaseVerRegExp1 = "Enterprise Linux Enterprise Linux Server release 5 (Carthage)";
	std::string strOEL5BaseVerRegExp2 = "Enterprise Linux Enterprise Linux Server release 5.[1-9] (Carthage)";
	std::string strOEL5BaseVerRegExp3 = "Enterprise Linux Enterprise Linux Server release 5.[1-9][0-9] (Carthage)";
	
	std::string strRHEL5BaseVerRegExp1 = "Red Hat Enterprise Linux Server release 5 (Tikanga)";
	std::string strRHEL5BaseVerRegExp2 = "Red Hat Enterprise Linux Server release 5.[1-9] (Tikanga)";
	std::string strRHEL5BaseVerRegExp3 = "Red Hat Enterprise Linux Server release 5.[1-9][0-9] (Tikanga)";
	std::string strCentOS5BaseVerRegExp1 = "CentOS release 5 (Final)";
	std::string strCentOS5BaseVerRegExp2 = "CentOS release 5.[1-9] (Final)";
	std::string strCentOS5BaseVerRegExp3 = "'CentOS release 5* (Final)";
	std::string strCentOS5BaseVerRegExp4 = "CentOS release 5.[1-9][0-9] (Final)";

	std::string strOEL6BaseVerRegExp1 = "Oracle Linux Server release 6.[0-9]";///Read this string from etc/oracle-release
	
	std::string strRHEL6BaseVerRegExp1 = "Red Hat Enterprise Linux Server release 6.[0-9] (Santiago)";
	std::string strRHEL6BaseVerRegExp2 = "Red Hat Enterprise Linux Server release 6.[0-9]";
	std::string strCentOS6BaseVerRegExp = "CentOS release 6.[0-9] (Final)";
	std::string strLinuxCentOS6BaseVerRegExp = "CentOS Linux release 6.[0-9] (Final)";
	
	std::string redhat_release_Value, SuSE_release_Value, debian_version_Value, ubuntu_release_Value,enterprise_release_Value, oracle_release_Value; 
	
	if( findOsRelese( "/etc/enterprise-release", enterprise_release_Value) == SVS_OK)
	{
		if( findOsRelese( "/etc/redhat-release",redhat_release_Value) == SVS_OK)
		{
			if((RegExpMatch(strOEL5BaseVerRegExp1.c_str(),enterprise_release_Value.c_str())) ||
				(RegExpMatch(strOEL5BaseVerRegExp2.c_str(),enterprise_release_Value.c_str())) ||
				(RegExpMatch(strOEL5BaseVerRegExp3.c_str(),enterprise_release_Value.c_str()))
				)
				{
					if(bis32BitMachine)
					{
						OS="OL5-32";
					}
					else
					{
						OS="OL5-64";
					}
				}
		}
	}
	else if( findOsRelese( "/etc/oracle-release", oracle_release_Value) == SVS_OK)
	{
		if( findOsRelese( "/etc/redhat-release",redhat_release_Value) == SVS_OK)
		{
			if(RegExpMatch(strOEL6BaseVerRegExp1.c_str(),oracle_release_Value.c_str()))
			{
				if(bis32BitMachine)
				{
					OS="OL6-32";
				}
				else
				{
					OS="OL6-64";
				}
			}
		}
	}	
	else if( findOsRelese( "/etc/redhat-release", redhat_release_Value ) == SVS_OK ) //TODO ...
	{
	    trimStringSpaces( redhat_release_Value );
		DebugPrintf(SV_LOG_DEBUG, "Command:cat /etc/redhat-release; OutPut:%s\n", redhat_release_Value.c_str() ) ;
		if( redhat_release_Value.find("release 3") != std::string::npos )
		{
			OS="RHEL3-32";
		}
		else if( redhat_release_Value.find("release 4 (Nahant Update 3)") != std::string::npos || 
		         redhat_release_Value.find("CentOS release 4.3 (Final)") != std::string::npos )
		{
			if( bis32BitMachine )
				OS="RHEL4U3-32";
			else
				OS="RHEL4U3-64";	
		}
		//else if( redhat_release_Value.find("release 4 (Nahant Update 4)") != std::string::npos || 
		//         redhat_release_Value.find("CentOS release 4.4 (Final)") != std::string::npos )
		//{
		//	 if( bis32BitMachine )
		//	     OS="RHEL4U4-32";
		//	 else
		//		 OS="RHEL4U4-64";
		//}
		else if( redhat_release_Value.find("Enterprise Linux Enterprise Linux AS release 4 (October Update 4)") != std::string::npos ) 
		{
  			 if( bis32BitMachine )
				OS="ELORCL4U4-32";
		}
		//else if( redhat_release_Value.find("release 4 (Nahant Update 5)") != std::string::npos || 
		 //		 redhat_release_Value.find("CentOS release 4.5 (Final)") != std::string::npos )
		//{
		//	if( bis32BitMachine )
		//		OS="RHEL4U5-32";
		//	else
		//		OS="RHEL4U5-64";
		//}		
		else if ((RegExpMatch(strRHEL4BaseVerRegExp1.c_str(),redhat_release_Value.c_str())) ||									 
				 (RegExpMatch(strCentOS4BaseVerRegExp1.c_str(),redhat_release_Value.c_str())) ||
				 (RegExpMatch(strRHEL4AdvVerRegExp1.c_str(),redhat_release_Value.c_str()))
				)
			{
				if(bis32BitMachine)
				{
					OS="RHEL4-32";
				}
				else
				{
					OS="RHEL4-64";
				}
			}
		else if ((RegExpMatch(strRHEL6BaseVerRegExp1.c_str(),redhat_release_Value.c_str()) ||
				 (RegExpMatch(strRHEL6BaseVerRegExp2.c_str(),redhat_release_Value.c_str())) ||
				 (RegExpMatch(strCentOS6BaseVerRegExp.c_str(),redhat_release_Value.c_str())) ||
				 (RegExpMatch(strLinuxCentOS6BaseVerRegExp.c_str(),redhat_release_Value.c_str())))
				)
		{
			if ( bis32BitMachine)
				OS="RHEL6-32";
			else
				OS="RHEL6-64";
		}
		else if ((RegExpMatch(strRHEL5BaseVerRegExp1.c_str(),redhat_release_Value.c_str())) ||
                         (RegExpMatch(strRHEL5BaseVerRegExp2.c_str(),redhat_release_Value.c_str())) ||
                         (RegExpMatch(strCentOS5BaseVerRegExp1.c_str(),redhat_release_Value.c_str())) ||
                         (RegExpMatch(strCentOS5BaseVerRegExp2.c_str(),redhat_release_Value.c_str())) ||
                         (RegExpMatch(strCentOS5BaseVerRegExp3.c_str(),redhat_release_Value.c_str()))
                        )
                        {
                                if(bis32BitMachine)
                                {
                                        OS="RHEL5-32";
                                }
                                else
                                {
                                        OS="RHEL5-64";
                                }
                        }
		else if ((RegExpMatch(strRHEL5BaseVerRegExp3.c_str(),redhat_release_Value.c_str())) ||
                 (RegExpMatch(strCentOS5BaseVerRegExp4.c_str(),redhat_release_Value.c_str())) 
                 )
                        {
                                if(bis32BitMachine)
                                {
                                        OS="RHEL5-32";
                                }
                                else
                                {
                                        OS="RHEL5-64";
                                }
                        }

		else if( redhat_release_Value.find("CentOS release 5 (Final)") != std::string::npos ) 
		{
			DebugPrintf(SV_LOG_DEBUG, " st_uts.release Value: %s \n", releaseValue.c_str() ) ;
			////if( releaseValue.find("2.6.18-8") != std::string::npos )
			if( releaseValue.find("2.6.18") != std::string::npos )

			{
				if( bis32BitMachine )
					OS="RHEL5-32";
				else
					OS="RHEL5-64";
			}
			//else if( releaseValue.find("2.6.18-53") != std::string::npos )
			else if( releaseValue.find("2.6.18") != std::string::npos )
			{
				if( bis32BitMachine )
					OS="RHEL5-32";
				else
					OS="RHEL5-64";
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, " Un Supported Cent Os 5 's vesion \n" ) ;
			}	
		}
		else if( redhat_release_Value.find("XenServer release 4.1.0-7843p (xenenterprise)") != std::string::npos ) 
		{
			if( bis32BitMachine )
				OS="CITRIX-XEN-4.1.0-7843p-32";
			else
				OS="CITRIX-XEN-4.1.0-7843p-64";	
		}
		else if( redhat_release_Value.find("XenServer release 5.0.0-10918p (xenenterprise)") != std::string::npos ) 
		{
			if( bis32BitMachine )
				OS="CITRIX-XEN-5.0.0-10918p-32";
			else
				OS="CITRIX-XEN-5.0.0-10918p-64";
		}
		else if( redhat_release_Value.find("XenServer release 4.0.1-4249p (xenenterprise)") != std::string::npos ) 
		{
			if( bis32BitMachine )
				OS="CITRIX-XEN-4.0.1-4249p-32";
			else
				OS="CITRIX-XEN-4.0.1-4249p-64";
		}
		else if( redhat_release_Value.find("5.5.0-15119p (xenenterprise)") != std::string::npos ) 
		{
			if( bis32BitMachine )
				OS="CITRIX-XEN-5.5.0-15119p-32";
			else
				OS="CITRIX-XEN-5.5.0-15119p-64";
		}
		else if( redhat_release_Value.find("5.5.0-25727p (xenenterprise)") != std::string::npos ) 
		{
			if( bis32BitMachine )
				OS="CITRIX-XEN-5.5.0-25727p-32";
			else
				OS="CITRIX-XEN-5.5.0-25727p-64";
		}
		else if( redhat_release_Value.find("5.6.0-31188p (xenenterprise)") != std::string::npos ) 
		{
			if( bis32BitMachine )
				OS="CITRIX-XEN-5.6.0-31188p-32";
			else
				OS="CITRIX-XEN-5.6.0-31188p-64";
		}
		else if( redhat_release_Value.find("5.6.100-39265p (xenenterprise)") != std::string::npos ) //for new OS CitrixXen-5.6-FP1-32
		{
			if( bis32BitMachine )
				OS="CITRIX-XEN-5.6.100-39265p-32";					
		}
		else if( redhat_release_Value.find("5.6.100-47101p (xenenterprise)") != std::string::npos ) //for new OS CitrixXen-5.6-SP2-32
		{
			if( bis32BitMachine )
				OS="CITRIX-XEN-5.6.100-47101p-32";					
		}
		else if( redhat_release_Value.find("6.0.0-50762p (xenenterprise)") != std::string::npos ) //1)XenServer release 6.0.0-50762p (xenenterprise)  2) XenServer SDK release 6.0.0-50762p (xenenterprise)
		{
			if( bis32BitMachine )
				OS="CITRIX-XEN-6.0.0-50762p-32";					
		}
		else if( redhat_release_Value.find("6.1.0-59235p (xenenterprise)") != std::string::npos ) //1)XenServer release 6.1.0-59235p (xenenterprise)  2) XenServer SDK release 6.1.0-59235p (xenenterprise)
		{
			if( bis32BitMachine )
				OS="CITRIX-XEN-6.1.0-59235p-32";					
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, " Un Supported Cent Os 5 's vesion \n" ) ;
		}
	}	
	else if( findOsRelese( "/etc/SuSE-release", SuSE_release_Value ) == SVS_OK) //TODO....
	{
		OS="SLES";
		trimStringSpaces( SuSE_release_Value );
		DebugPrintf(SV_LOG_DEBUG, " Command:cat /etc/SuSE-release; OutPut:%s\n", SuSE_release_Value.c_str() ) ;
		if( SuSE_release_Value.find("VERSION = 10") != std::string::npos )
        {		
			if( SuSE_release_Value.find("PATCHLEVEL") == std::string::npos )
			{
				if( SuSE_release_Value.find("SUSE LINUX 10.0") != std::string::npos )
				{
					if( bis32BitMachine )
						OS="OPENSUSE10-32";
			  	    else
						OS="OPENSUSE10-64";
				}
				else if( SuSE_release_Value.find("SUSE Linux Enterprise Server 10") != std::string::npos )
				{
					if( bis32BitMachine )
						OS="SLES10-32";
			  	    else
						OS="SLES10-64";
				}
			}
			else if( SuSE_release_Value.find("SUSE Linux Enterprise Server 10") != std::string::npos  &&
			         SuSE_release_Value.find("PATCHLEVEL = 1") != std::string::npos )
			{
				if( bis32BitMachine )
					OS="SLES10-SP1-32";
				else
					OS="SLES10-SP1-64";
			}
		    else if( SuSE_release_Value.find("SUSE Linux Enterprise Server 10") != std::string::npos &&
			         SuSE_release_Value.find("PATCHLEVEL = 2") != std::string::npos )
		    {
				if( bis32BitMachine )
					OS="SLES10-SP2-32";
				else
					OS="SLES10-SP2-64";
			}
		    else if( SuSE_release_Value.find("SUSE Linux Enterprise Server 10") != std::string::npos &&
			         SuSE_release_Value.find("PATCHLEVEL = 3") != std::string::npos )
		    {
				if( bis32BitMachine )
					OS="SLES10-SP3-32";
				else
					OS="SLES10-SP3-64";
			}
			else if( SuSE_release_Value.find("SUSE Linux Enterprise Server 10") != std::string::npos &&
			         SuSE_release_Value.find("PATCHLEVEL = 4") != std::string::npos )
		    {
				if( bis32BitMachine )
					OS="SLES10-SP4-32";
				else
					OS="SLES10-SP4-64";
			}			
			else if( SuSE_release_Value.find("PATCHLEVEL = 1") != std::string::npos )
			{
				if( bis32BitMachine )
					OS="OPENSUSE10-SP1-32";
				else
					OS="OPENSUSE10-SP1-64";
		 	}
			else
			{
				DebugPrintf(SV_LOG_ERROR, " Un Supported SuSE10 version \n" ) ;
			}
		 }
		 else if( SuSE_release_Value.find("VERSION = 11") != std::string::npos && 
			      SuSE_release_Value.find("SUSE Linux Enterprise Server 11") != std::string::npos && 
                  SuSE_release_Value.find("PATCHLEVEL = 0") != std::string::npos )
         	{
			if (SuSE_release_Value.find("SUSE Linux Enterprise Server 11") != std::string::npos )
			{
				if( bis32BitMachine )
					OS="SLES11-32";
				else
					OS="SLES11-64";
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, " Un Supported SuSE11 version \n" ) ;
			}
		 }
		 else if( SuSE_release_Value.find("VERSION = 11") != std::string::npos && 
			      SuSE_release_Value.find("SUSE Linux Enterprise Server 11") != std::string::npos && 
                  SuSE_release_Value.find("PATCHLEVEL = 1") != std::string::npos )
         	{
			if (SuSE_release_Value.find("SUSE Linux Enterprise Server 11") != std::string::npos )
			{
				if( bis32BitMachine )
					OS="SLES11-SP1-32";
				else
					OS="SLES11-SP1-64";
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, " Un Supported SuSE11 version \n" ) ;
			}
		 }
 		else if( SuSE_release_Value.find("VERSION = 11") != std::string::npos &&
                              	SuSE_release_Value.find("SUSE Linux Enterprise Server 11") != std::string::npos &&
                  		SuSE_release_Value.find("PATCHLEVEL = 2") != std::string::npos )
         	{
                        if (SuSE_release_Value.find("SUSE Linux Enterprise Server 11") != std::string::npos )
                        {
                                if( bis32BitMachine )
                                        OS="SLES11-SP2-32";
                                else
                                        OS="SLES11-SP2-64";
                        }
                        else
                        {
                                DebugPrintf(SV_LOG_ERROR, " Un Supported SuSE11 version \n" ) ;
                        }
                 }
		else if( SuSE_release_Value.find("VERSION = 11") != std::string::npos &&
                                SuSE_release_Value.find("SUSE Linux Enterprise Server 11") != std::string::npos &&
                                SuSE_release_Value.find("PATCHLEVEL = 3") != std::string::npos )
                {
                        if (SuSE_release_Value.find("SUSE Linux Enterprise Server 11") != std::string::npos )
                        {
                                if( bis32BitMachine )
                                        OS="SLES11-SP3-32";
                                else
                                        OS="SLES11-SP3-64";
                        }
                        else
                        {
                                DebugPrintf(SV_LOG_ERROR, " Un Supported SuSE11 version \n" ) ;
                        }
                 }
		else if( SuSE_release_Value.find("VERSION = 11") != std::string::npos &&
                                SuSE_release_Value.find("SUSE Linux Enterprise Server 11") != std::string::npos &&
                                SuSE_release_Value.find("PATCHLEVEL = 4") != std::string::npos )
                {
                        if (SuSE_release_Value.find("SUSE Linux Enterprise Server 11") != std::string::npos )
                        {
                                if( bis32BitMachine )
                                        OS="SLES11-SP4-32";
                                else
                                        OS="SLES11-SP4-64";
                        }
                        else
                        {
                                DebugPrintf(SV_LOG_ERROR, " Un Supported SuSE11 version \n" ) ;
                        }
                 }
		 else if( SuSE_release_Value.find("VERSION = 9") != std::string::npos )
		 {
		
			if( SuSE_release_Value.find("PATCHLEVEL") == std::string::npos )
			{
				if( bis32BitMachine )
					OS="SLES9-32";
				else
					OS="SLES9-64";
			}
			else if( SuSE_release_Value.find("PATCHLEVEL = 2") != std::string::npos )
			{
				if( bis32BitMachine )
					OS="SLES9-SP2-32";
				else
					OS="SLES9-SP2-64";
			}
			else if( SuSE_release_Value.find("PATCHLEVEL = 3") != std::string::npos )
			{
				if( bis32BitMachine )
					OS="SLES9-SP3-32";
				else
					OS="SLES9-SP3-64";
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, " Un Supported SuSE9 version \n" ) ;
			}
		 }
		 else
		 {
			 DebugPrintf(SV_LOG_ERROR, " Un Supported SuSE-relese version \n" ) ;
		 }
	 }
	else if( findOsRelese("/etc/lsb-release", ubuntu_release_Value ) == SVS_OK )
	{
		trimStringSpaces( ubuntu_release_Value );
		DebugPrintf(SV_LOG_DEBUG, "Command:cat /etc/lsb-release; OutPut:%s\n", ubuntu_release_Value.c_str() ) ;
		if( ubuntu_release_Value.find("8.04") != std::string::npos )
		{
			if( bis32BitMachine )
				OS="UBUNTU8-32";
			else
				OS="UBUNTU8-64";
		}
		if( ubuntu_release_Value.find("10.04.1") != std::string::npos )
		{
			if( bis32BitMachine )
				OS="UBUNTU10-32";
			else
				OS="UBUNTU10-64";
		}
		if( ubuntu_release_Value.find("10.04.4 LTS") != std::string::npos )	
		{
			if( bis32BitMachine )
			{	
				OS=" ";
			}	
			else
				OS="UBUNTU-10.04.4-64";			
		}
		if( ubuntu_release_Value.find("12.04.4 LTS") != std::string::npos )	
		{
			if( bis32BitMachine )
			{	
				OS=" ";
			}	
			else
				OS="UBUNTU-12.04.4-64";			
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, " Un Supported Ubuntu release Flavour version.. \n" ) ;
		}
	}
	else if( findOsRelese( "/etc/debian_version", debian_version_Value ) == SVS_OK )
	{
		trimStringSpaces( debian_version_Value );
		DebugPrintf(SV_LOG_DEBUG, "Command:cat /etc/debian_version; OutPut:%s\n", debian_version_Value.c_str() ) ;
		if( debian_version_Value.find("4.0") != std::string::npos )
		{
			if( bis32BitMachine )
				OS="DEBIAN-ETCH-32";
			else
				OS="DEBIAN-ETCH-64";
		}
		if( debian_version_Value.find("6.0.1") != std::string::npos )
		{
			if( bis32BitMachine )
				OS="DEBIAN6U1-32";
			else
				OS="DEBIAN6U1-64";
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, " Un Supported debain_version \n" ) ;
		}		
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Could not cat the apporpriate files to know os type \n" ) ;
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ ) ;
}
void getHPUXMachineOsType( std::string releaseValue, std::string& OS )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	OS="B.11.23";
	if( releaseValue.find( "B.11.23" ) == 0 )
	{
		OS="HP-UX-Itanium-11iv2";
	}
	else if( releaseValue.find( "B.11.31" ) == 0 )
	{
		OS="HP-UX-Itanium-11iv3";
	}
	else if( releaseValue.find( "B.11.11" ) == 0 )
	{
		OS="HP-UX-PA-RISC";
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, " Un Supported HP-UX \n" ) ;
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ ) ;	
}
void getAIXMachineOsType( std::string versionValue, std::string releaseValue, std::string& OS )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	OS="AIX";
	if( versionValue.compare("5") == 0 )
	{
		if( releaseValue.compare("2") == 0 )
		{
			OS="AIX52";
		}
		else if( releaseValue.compare("3") == 0 )
		{
			OS="AIX53";
		}
	}
	else if( versionValue.compare("6") == 0 )
	{	
		if( releaseValue.compare("1") == 0 )
		{
			OS="AIX61";
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, " Un Supported AIX%s.%s \n",versionValue.c_str(),releaseValue.c_str() ) ;
		}
	}
	else if( versionValue.compare("7") == 0 )
	{	
		if( releaseValue.compare("1") == 0 )
		{
			OS="AIX71";
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, " Un Supported AIX%s.%s \n",versionValue.c_str(),releaseValue.c_str() ) ;
		}
	}

	else
	{
		DebugPrintf(SV_LOG_DEBUG, " Un Supported AIX \n" ) ;
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ ) ;
}
#ifdef SV_SUNOS
void getSunOSMachineOsType( std::string uts_releaseValue, std::string systemArchitecture, std::string& OS )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	std::string sunos_release_Value;
	bool bis32BitMachine;
	bis32BitMachine = false;
	OS="SunOS";
	if( uts_releaseValue.compare("5.8") == 0 && systemArchitecture.find("sparc") != std::string::npos )
	{
		if( bis32BitMachine )
			OS="Solaris-5-8-Sparc-32";
		else
			OS="Solaris-5-8-Sparc";
	}
	else if( uts_releaseValue.compare("5.9") == 0 && systemArchitecture.find("sparc") != std::string::npos )
	{
		if( bis32BitMachine )
			OS="Solaris-5-9-Sparc-32";
		else
			OS="Solaris-5-9-Sparc";
	}
	else if( uts_releaseValue.compare("5.10") == 0 && ( systemArchitecture.find("i386") != std::string::npos || systemArchitecture.find("x86") != std::string::npos ) )
	{
		if( bis32BitMachine )
			OS="Solaris-5-10-x86-32";
		else
			OS="Solaris-5-10-x86-64";
	}
	else if( uts_releaseValue.compare("5.10") == 0 && systemArchitecture.find("sparc") != std::string::npos )
	{
		if( bis32BitMachine )
			OS="Solaris-5-10-Sparc-32";
		else
			OS="Solaris-5-10-Sparc";
	}
	else if( uts_releaseValue.compare("5.11") == 0 ) 
	{
		if( findOsRelese( "/etc/release", sunos_release_Value ) == SVS_OK) 
		{
			trimStringSpaces( sunos_release_Value );
			DebugPrintf(SV_LOG_DEBUG, " Command:cat /etc/release; OutPut:%s\n", sunos_release_Value.c_str() ) ;

			if( sunos_release_Value.find("OpenSolaris") != std::string::npos )
			{		

				if ( systemArchitecture.find("i386") != std::string::npos || systemArchitecture.find("x86") != std::string::npos ) 
				{
					if( bis32BitMachine )
						OS="OpenSolaris-5-11-x86-32";
					else
						OS="OpenSolaris-5-11-x86-64";
				}
				else	 if(systemArchitecture.find("sparc") != std::string::npos )
				{
					if( bis32BitMachine )
						OS="OpenSolaris-5-11-Sparc-32";
					else
						OS="OpenSolaris-5-11-Sparc";
				}
			}
			else if(sunos_release_Value.find("Oracle Solaris") != std::string::npos)
			{
				if ( systemArchitecture.find("i386") != std::string::npos || systemArchitecture.find("x86") != std::string::npos ) 
				{
					if( bis32BitMachine )
						OS="Solaris-5-11-x86-32";
					else
						OS="Solaris-5-11-x86-64";
				}
				else	 if(systemArchitecture.find("sparc") != std::string::npos )
				{
					if( bis32BitMachine )
						OS="Solaris-5-11-Sparc-32";
					else
						OS="Solaris-5-11-Sparc";
				}

			}
			
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Could not cat the /etc/release for os type \n" ) ;
		}
	}	
	else
	{
		DebugPrintf( SV_LOG_ERROR, " Un Supported SunOs Flavour.. \n" );
	}
	DebugPrintf( SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__ ) ;
}
#endif

SVERROR getLocalMachineOsType( std::string& OS )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	OS = "OS";
	SVERROR bRet = SVS_OK;
	std::string sysName;
	struct utsname st_uts;
	if ( uname( &st_uts ) == -1 )
	{
		DebugPrintf( SV_LOG_ERROR, "Failed to stat out uname \n" );
		bRet = SVS_FALSE;
		return bRet;
	}
	sysName = st_uts.sysname;	
	trimStringSpaces( sysName );
	DebugPrintf(SV_LOG_DEBUG, "Sysname is: %s\n", sysName.c_str() ) ;
	if( sysName.compare("Linux") == 0 )
	{
		OS = "Linux";	
        std::string releaseValue = st_uts.release;
        trimStringSpaces( releaseValue );
		std::string machineValue = st_uts.machine;
        trimStringSpaces( machineValue );
	    std::string Linux_OS_Type;
		getLinuxMachineOsType( machineValue, releaseValue, Linux_OS_Type );
		OS = Linux_OS_Type;
	}
	else if( sysName.compare("HP-UX") == 0 )
	{
		OS = "HP-UX";
		std::string HPUX_OS_Type;
		std::string releaseValue = st_uts.release;
        trimStringSpaces( releaseValue );
		DebugPrintf(SV_LOG_DEBUG, " st_uts.release Value is:%s\n", releaseValue.c_str() ) ;
		getHPUXMachineOsType( releaseValue, HPUX_OS_Type );
		OS = HPUX_OS_Type;
	}
	else if( sysName.compare("AIX") == 0 )
	{
		OS = "AIX";
		std::string versionValue, releaseValue;
		versionValue = st_uts.version;        
        trimStringSpaces( versionValue );
        releaseValue = st_uts.release;
        trimStringSpaces( releaseValue );
		DebugPrintf(SV_LOG_DEBUG, "st_uts.version:%s\n", versionValue.c_str() ) ;
		DebugPrintf(SV_LOG_DEBUG, "st_uts.release:%s\n", releaseValue.c_str() ) ;
		std::string AIX_OS_TYPE;
		getAIXMachineOsType( versionValue, releaseValue, AIX_OS_TYPE );
		OS = AIX_OS_TYPE;
	}
#ifdef SV_SUNOS
	else if( sysName.compare("SunOS") == 0 )
	{
		OS = "SunOS";
		SVERROR bRet = SVS_OK;
		const int COUNT = 257;
		char sytem_info[COUNT] = {'\0'};
		if( sysinfo( SI_ARCHITECTURE, sytem_info, COUNT ) == -1 )
		{
			DebugPrintf( SV_LOG_ERROR, "Failed to stat out sysinfo \n" );	
			bRet = SVS_FALSE;
			return bRet;
		}
		std::string uts_releaseValue;
		uts_releaseValue = st_uts.release;
		trimStringSpaces( uts_releaseValue );
		std::string systemArchitecture( sytem_info );
		trimStringSpaces( systemArchitecture );
		DebugPrintf(SV_LOG_DEBUG, "uts_release Value: %s\n", uts_releaseValue.c_str() ) ;
		DebugPrintf(SV_LOG_DEBUG, "systemArchitecture %s\n", systemArchitecture.c_str() ) ;
		std::string Sun_OS_TYPE;
		getSunOSMachineOsType( uts_releaseValue, systemArchitecture, Sun_OS_TYPE );
		OS = Sun_OS_TYPE;
	 }
#endif	 
	else
	{	
		DebugPrintf(SV_LOG_DEBUG, " UnSupported SysName: %s \n", sysName.c_str() ) ;
		bRet = SVS_FALSE;
	}	
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	trimStringSpaces( OS );
	if( OS.empty() == true )
	{
		OS = "OS";
	}
	return bRet;
}

SVERROR RegisterHost( const char *pszRegKey,
                      const char *pszSVServerName,
                      SV_INT HttpPort,
                      const char *pszURL,
                      const char* pszInstallPath,
                      char *pszHostID,
                      SV_ULONG *pHostIDSize,bool https )
{
    /* place holder function */
	return SVS_OK;
}

SVERROR RegisterFX(const char *pszRegKey,
                                const char *pszSVServerName,
                                SV_INT HttpPort,
                                const char *pszURL,
                                const char* pszInstallPath,
                                char *pszHostID,
                                SV_ULONG *pHostIDSize,bool https)
{
       return RegisterHost(pszRegKey, pszSVServerName, HttpPort, pszURL, pszInstallPath, pszHostID, pHostIDSize,https);
}

SVERROR ReadAgentConfiguration( SV_HOST_AGENT_PARAMS *pConfiguration,
                                const char* pFileName,
                                SV_HOST_AGENT_TYPE AgentType )
{
    /* place holder function */
    return SVS_OK;
}

 

///
/// Removes all trailing backslashes
///
char* PathRemoveAllBackslashesA( char* pszPath )
{
    char* psz = NULL;
    assert( NULL != pszPath );

    int cch = strlen( pszPath );
    for( psz = pszPath + cch - 1; psz >= pszPath && '\\' == *psz;
         psz-- )
    {
        *psz = 0;
    }

    return( max( psz, pszPath ) );
}

///
/// Removes all trailing backslashes, NOT IMPLEMENTED
///
SV_WCHAR* PathRemoveAllBackslashesW( SV_WCHAR* pszPath )
{
    /*
    assert( NULL != pszPath );

    SV_WCHAR* psz;
    do
    {
        psz = PathRemoveBackslashW( pszPath ) - 1;
    }
    while( 0 != pszPath[0] && L'/' == *psz );

    return( psz );
    */
    return NULL;
}


//
// Converts in-place pathnames from e:\program files\blah to /cygdrive/e/program files/blah
//
char* CygwinizePathname( char* pszPathname )
{
    /* place holder function */
    return( pszPathname );
}

char* CygwinizePathname( char* pszCygwinPathname, const char* pszPathname )
{
    /* place holder function */
    return( pszCygwinPathname );
}


SVERROR GetProcesOutput(const string& cmd, string& output)
{
    FILE *in;
    char buff[512];

    /* popen creates a pipe so we can read the output
     of the program we are invoking */
    if (!(in = popen(cmd.c_str(), "r")))
    {
        return SVS_FALSE;
    }

    /* read the output of command , one line at a time */
    while (fgets(buff, sizeof(buff), in) != NULL )
    {
        output += buff;
    }

    /* close the pipe */
    pclose(in);
    return SVS_OK;
}

SVERROR GetConfigFileName(char* szConfFileName)
{
    /* place holder */
    return SVS_OK;
}

///
/// Does a recursive mkdir(). Argument is an absolute, not relative path.
///
SVERROR SVMakeSureDirectoryPathExists( const char* pszDestPathname )
{
    /* place holder function */

    return SVS_OK;
}

void StringToSVLongLong( SV_LONGLONG& num, char const* psz )
{
    assert( NULL != psz );
    sscanf( psz, "%lld", &num );
}

//
// NOTE: assumes psz has enough space to hold string
//
char* SVLongLongToString( char* psz, SV_LONGLONG num, size_t size )
{
    assert( NULL != psz );
    inm_sprintf_s( psz, size, "%lld", num );
    return( psz );
}

SVERROR SVGetFileSize( char const* pszPathname, SV_LONGLONG* pFileSize )
{
    SVERROR rc;
    struct stat file;

    assert( NULL != pszPathname && NULL != pFileSize );

    do
    {
        if( NULL == pszPathname || NULL == pFileSize )
        {
            rc = SVE_INVALIDARG;
            break;
        }
        *pFileSize = 0;

        //
        // BUGBUG: support files > 2GB
        //
        if( stat( pszPathname,&file ) )
        {
            rc = errno;
            break;
        }
        else
        {
            *pFileSize = file.st_size;
        }
    } while( false );

    return( rc );
}

void GetOperatingSystemInfo(char* const psInfo, int length)
{
    /* place holder function */
}


SVERROR convert_OpenSSH_SecSH(const char* ssh_key_gen_path,							
							const char* pubkey_file,
							struct SSHOPTIONS &sshOptions) {
    /* place holder function */
	return SVS_OK;
}

SVERROR Update_identity_file(const char* priv_key_path, const char* identity_file) 
{
	/* place holder function */
	return SVS_OK;
		
}
SVERROR generate_key_pair_impl (SV_ULONG JobId,
							SV_ULONG JobConfigId, 
							struct SSHOPTIONS &sshOptions, 
							struct SSH_Configuration& ssh_config, 
							const char* szCacheDirectory) 
{
	/* place holder function */
	return SVS_OK;
}

SVERROR fetch_keys(SV_ULONG JobConfigId, 
							struct SSHOPTIONS &sshOptions, 
							struct SSH_Configuration& ssh_config, 
							const char* szCacheDirectory) 
{
	/* place holder function */
	return SVS_OK;	

}

SVERROR update_authorization_file_impl(SV_ULONG JobConfigId,
									const char* szAuthorization_file,
									const struct SSHOPTIONS& sshOptions,
									const char* szCacheDirectory) 
{
	/* place holder function */
	return SVS_OK;
}

SVERROR update_authorized_keys_impl(const char* szAuthKeys_file, 
									const struct SSHOPTIONS & sshOptions) 
{
    /* place holder */
	return SVS_OK;

}
SVERROR convert_SecSH_OpenSSH(char *OpenSSHKey, 
							char* SecSHKey, 
							const char *szCacheDirectory, 
							SV_ULONG JobConfigId,
							SV_ULONG JobId) 
{
		/* place holder function */
		return SVS_OK;
}

SVERROR remove_pubkey_auth_keys(const char* authkeys_file, SV_ULONG JobConfigId) 
{
	CProcess *pProcess = NULL;
	char lInmszPubKeyComment[BUFSIZ];
	char lInmszSedCmd[BUFSIZ];
	SVERROR hr = SVS_OK;
	inm_sprintf_s(lInmszPubKeyComment, ARRAYSIZE(lInmszPubKeyComment), "The_PUBLIC_KEY_FOR_JOB_CONFIG_ID_%d", JobConfigId);
	inm_sprintf_s(lInmszSedCmd, ARRAYSIZE(lInmszSedCmd), "sed /%s/d %s/%s > /tmp/out; cat /tmp/out > %s/%s;rm -rf /tmp/out",
					lInmszPubKeyComment, getHomeDir(),authkeys_file, getHomeDir(), authkeys_file);
	DebugPrintf("The sez command is %s\n", lInmszSedCmd);
	string str = "";
	GetProcessOutput(lInmszSedCmd, str);
	DebugPrintf("The output of the above command while removing the keys for Config Id %d is %s\n", JobConfigId, str.c_str());
	return hr;
}
SVERROR remove_pubkey_authorization(const char* authorization_file, const char* szCacheDirectory, SV_ULONG JobConfigId)
{
	/* place holder function */
	return SVS_OK;
}
SVERROR remove_identity(const char* identification_file, const char* szCacheDirectory, SV_ULONG JobConfigId) 
{
	/* place holder function */
	return SVS_OK;

}
SVERROR cleanup_server_keys_impl(SSH_Configuration ssh_config, const char* szCacheDirectory, SV_ULONG JobId, SV_ULONG JobConfigId) 
	{
	SVERROR hr = SVS_OK;
	if(strstr(ssh_config.sshd_prod_version, "OpenSSH")) {
		hr = remove_pubkey_auth_keys(ssh_config.auth_keys_file, JobConfigId);
	} 
	else {
		hr = remove_pubkey_authorization(ssh_config.authorization_file, szCacheDirectory, JobId);
	}
	return hr;
}

SVERROR create_ssh_tunnel_impl(const char* szTunnel, CProcess **pProcess, const char* szLogFileName, SV_ULONG* exitCode) 
	{
	SVERROR hr = SVS_OK;
	hr = CProcess::Create(szTunnel, pProcess, szLogFileName);			
	if(hr.failed())
	{	
		(*pProcess)->terminate();
		(*pProcess)->getExitCode(exitCode);
		DebugPrintf(SV_LOG_ERROR, "Failed to create the tunnel with command %s\n", szTunnel);
		return SVS_FALSE;
	}	
	sleep(20);
	return SVS_OK;
}
SV_ULONG getFreePort(SV_ULONG startPort,SV_ULONG endPort) 
{
        int i = 0;
	SV_ULONG localPort;
        for(i = startPort; i<endPort; i++)
        {
                if(getFreePort_impl(i) == 0) {
                        localPort = i;
                        break;
                }
                continue;
        }
        if(localPort == -255 )
        {
                DebugPrintf("Unable to get the free localport\n");
        }
        else
        {
                DebugPrintf("Local port got for encrypting the traffic is %d\n", localPort);
        }
        return localPort;

}
SVERROR cleanup_client_keys_impl(SSH_Configuration ssh_config, const char* szCacheDirectory, SV_ULONG configId) {
	SVERROR hr = SVS_OK;
	if(strstr(ssh_config.sshd_prod_version, "OpenSSH") == 0) 
	{
		remove_identity(ssh_config.identity_file, szCacheDirectory, configId);
	}
	return hr;
}

SVERROR delete_encryption_keys_impl(const char *szCacheDirectory, SV_ULONG configId) {
	/* place holder function */
	return SVS_OK;
}
int getFreePort_impl(int port)
{
  struct sockaddr_in tcp_addr; 

  struct hostent *hp;        

  int addr_len, tcp_sockfd;             

  if ((hp = gethostbyname("localhost")) == NULL)
  {
    return -1;
  }

  if ((tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    return -1;

  }
  memset((char *) &tcp_addr, 0, sizeof(tcp_addr));
  tcp_addr.sin_family      = AF_INET;
  inm_memcpy_s((char *)&tcp_addr.sin_addr, sizeof(tcp_addr.sin_addr), (char *)hp->h_addr, hp->h_length);
  tcp_addr.sin_port        = htons(port);

  if (bind(tcp_sockfd, (struct sockaddr *) &tcp_addr, sizeof(tcp_addr)) < 0)
  {
    return -1;
  }
  addr_len = sizeof(tcp_addr);
/*  getsockname(tcp_sockfd, (struct sockaddr *) &tcp_addr, (socklen_t *)&addr_len);*/
  close(tcp_sockfd);
  return 0;
}

SVERROR waitTillTunnelEstablishes(int localPort)
{
        int sockfd;
        SVERROR ret = SVS_OK;
        struct sockaddr_in dest_addr; // will hold the destination addr
        int lInmtime_out = 120;
        int lInmwait_sec = 5;
        int lInmretries = 0;
        sockfd = socket(AF_INET, SOCK_STREAM, 0); // do some error checking!
        if(sockfd == -1 )
        {
            DebugPrintf(SV_LOG_ERROR, "socket failed.. errno %d\n", errno);
        }
        dest_addr.sin_family = AF_INET; // host byte order
        dest_addr.sin_port = htons(localPort); // short, network byte order
        dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        DebugPrintf(SV_LOG_INFO, "local port is %d\n", localPort);
        do
        {
                if (connect(sockfd, (struct sockaddr *)&dest_addr, sizeof dest_addr) != 0)
                {
                        lInmretries ++;
                        if(lInmwait_sec * lInmretries  >  lInmtime_out)
                        {
                                DebugPrintf(SV_LOG_ERROR, "Local port failed to enter into listening state even after %d seconds  with error %d\n", lInmtime_out, errno);
                                ret = SVS_FALSE;
                                break;
                        }
                        sleep(lInmwait_sec);
                }
                else
                {
                        DebugPrintf(SV_LOG_INFO, "Local port went to listening mode after %d seconds..\n", lInmretries * lInmwait_sec);
                        ret = SVS_OK;
                        break;
                }
        } while(1);
	close(sockfd);
return ret;
}
void PostFailOverJobStatusToCX(char* cxip, SV_ULONG httpPort, SV_ULONG processId, SV_ULONG jobId,bool https)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	char path[ BUFSIZ ];
	string failOverJobStatusFileName ;
	static string installationDirectory;
	SVERROR er;
	SVConfig* iconf;

	iconf = SVConfig::GetInstance();
	iconf->Parse("/usr/local/.vx_version");
	iconf->GetValue("INSTALLATION_DIR", installationDirectory);

	failOverJobStatusFileName = installationDirectory;
	failOverJobStatusFileName += '/';
	failOverJobStatusFileName += "ApplicationData" ;
	failOverJobStatusFileName += '/';
	failOverJobStatusFileName += "Failover_" ;
	char str[10];
	inm_sprintf_s(str, ARRAYSIZE(str), "%d", processId);
	failOverJobStatusFileName += str;
	DebugPrintf( SV_LOG_INFO, "Fail Over script's o/p path name %s\n", failOverJobStatusFileName.c_str());
	FILE * fp = NULL ;
	fp = fopen( failOverJobStatusFileName.c_str(), "r");
	if( fp !=  NULL )
	{
		char inBuffer[ 512 ] ;
		int size = fread(inBuffer, 1, 2048, fp);
		if( size == 0 )
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to read the file %s with error %d\n", failOverJobStatusFileName.c_str(), errno);
			return ;
		}
		else if( size > 0 )
		{
			inBuffer[ size ] = '\0';
			DebugPrintf(SV_LOG_DEBUG, "file %s and its contents %s\n", failOverJobStatusFileName.c_str(), inBuffer );
		}
		fclose( fp ) ;
		fp = NULL ;
		remove(failOverJobStatusFileName.c_str());
		SVERROR hr = SVS_OK;
		char szUrl[SV_INTERNET_MAX_URL_LENGTH];
		inm_sprintf_s(szUrl, ARRAYSIZE(szUrl), "/failover_status.php?JobId=%ld", jobId);
		char postBuffer[ BUFSIZ ];
		memset( postBuffer, '\0', BUFSIZ );
		inm_sprintf_s(postBuffer, ARRAYSIZE(postBuffer), "&JobType=%s&ScripExitStatus=%s", "FailOver", inBuffer);
		DebugPrintf("Update Failover script's status url is details url %s\n", szUrl);
		hr = postToCx(cxip, httpPort, szUrl, postBuffer,NULL,https );
		if ( hr.failed() )
		{
			DebugPrintf(SV_LOG_ERROR, "PostToServer Failed. Unable to update failover script status for job id %ld\n", jobId);
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Successfully updated the fail over job status to CX for job id %ld\n", jobId);
		}
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "fail over script file %s not present \n", failOverJobStatusFileName.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
}

SVERROR getSocketOptions(char* pszValue, int cch)
{
    /* place holder function */
	return SVS_OK;
}	


void GetCurrentOSCXCode(char *osVal, size_t len)
{
    /* place holder function */
}

SVERROR GetConfigParam(std::string szConfFile, std::string szKey, std::string & strValue)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	SVERROR hr = SVE_FAIL;
	if(szConfFile.empty() || szKey.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Config file or key should not be empty.\n");
		DebugPrintf(SV_LOG_DEBUG,"Config file: %s \nKey : %s\n",
		szConfFile.c_str(),
		szKey.c_str());
		return hr;
	}
    SVConfig* conf = SVConfig::GetInstance();
    hr = conf->Parse(szConfFile);
	if (hr.failed())
	{
           DebugPrintf(SV_LOG_ERROR,"PARSING  %s FILE FAILED \n",szConfFile.c_str());
           DebugPrintf(SV_LOG_ERROR,"CHECK config.ini FOR ANY JUNK DATA OR SYNTAX ERRORS\n");
		   DebugPrintf(SV_LOG_ERROR,"VERIFY config.ini AND RESTART FX SERVICE\n");
           return hr;
    }
    DebugPrintf(SV_LOG_DEBUG,"Config file at %s is successfully parsed\n",szConfFile.c_str());
    DebugPrintf(SV_LOG_DEBUG,"Total number of keys parsed = %d\n",
                                       conf->GetTotalNumberOfKeys());
	hr = conf->GetValue(szKey,strValue);
	if(hr.failed())
	{
		DebugPrintf(SV_LOG_WARNING," key %s is not available in the config file\n",szKey.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return hr;
}

SVERROR SetConfigParam(std::string szConfFile, std::string szKey, std::string strValue)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
	SVERROR hr = SVE_FAIL;
	if(szConfFile.empty() || szKey.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Config file or key or value should not be empty.\n");
		DebugPrintf(SV_LOG_DEBUG,"Config file: %s \nKey : %s\n",
		szConfFile.c_str(),
		szKey.c_str());
		return hr;
	}
    SVConfig* conf = SVConfig::GetInstance();
    hr = conf->Parse(szConfFile);
	if (hr.failed())
	{
           DebugPrintf(SV_LOG_ERROR,"PARSING  %s FILE FAILED \n",szConfFile.c_str());
           DebugPrintf(SV_LOG_ERROR,"CHECK config.ini FOR ANY JUNK DATA OR SYNTAX ERRORS\n");
		   DebugPrintf(SV_LOG_ERROR,"VERIFY config.ini AND RESTART FX SERVICE\n");
           return hr;
    }
    DebugPrintf(SV_LOG_DEBUG,"Config file at %s is successfully parsed\n",szConfFile.c_str());
    DebugPrintf(SV_LOG_DEBUG,"Total number of keys parsed = %d\n",
                                       conf->GetTotalNumberOfKeys());
	//Trim the special charaters to avoid the config file curruption
	trim(strValue);
	hr = conf->UpdateConfigParameter(szKey,strValue);
	if(hr.failed())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to update the key %s with value %s\n",szKey.c_str(),strValue.c_str());
	}
	hr = conf->Write();
	if(hr.failed())
	{
		DebugPrintf(SV_LOG_ERROR,"SVConfig::Write() failed. Failed to update the key %s with value %s\n",szKey.c_str(),strValue.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
	return hr;
}
