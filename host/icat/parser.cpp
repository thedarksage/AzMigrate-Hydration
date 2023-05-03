#include "parser.h"
#include "logger.h"
#include "defs.h"
#include "common.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include <sstream>

static const char SECTION_REMOTE_OFFICE[] = "remoteoffice";
static const char SECTION_ARCHIVE_REPOSITORY[] = "archiverepository";
static const char SECTION_CONTENT[] = "content";
static const char SECTION_CONTENT_SOURCE[] = "content.source";
static const char SECTION_TUNABLES[] = "tunables";
static const char SECTION_CONFIG[] = "config";
static const char SECTION_DELETE[] = "delete";
static const char SECTION_FILELIST[]= "filelist";

static const char KEY_SERVER_NAME[] = "servername";
static const char KEY_BRANCH_NAME[] = "branchname";
static const char KEY_SOURCE_VOLUME[] = "sourcevolume";

static const char KEY_TRANSPORT[] = "transport";
static const char KEY_NODES[] = "nodes";
static const char KEY_DNS_NAME[] = "dnsname";
static const char KEY_UID[] = "uid";
static const char KEY_GID[] = "gid";

static const char KEY_DIRECTORY_NAME[] = "directoryname";
static const char KEY_EXCLUDE_LIST[] = "excludelist";
static const char KEY_FILE_FILTER[] = "filefilter";
static const char KEY_INCLUDE[] = "include";

static const char KEY_RETRY_ON_FAILURE[] = "retryonfailure";
static const char KEY_RETRY_LIMIT[] = "retrylimit";
static const char KEY_RETRY_INTERVAL[] = "retryinterval";
static const char KEY_EXITONRETRYEXPIRY[] = "exitonretryexpiry";
static const char KEY_MAX_CONNECTS[] = "maxconnects";
static const char KEY_TCP_RECV_BUFFER[] = "tcprecvbuffer";
static const char KEY_TCP_SEND_BUFFER[] = "tcpsendbuffer";
static const char KEY_LOWSPEED_LIMT[] = "lowspeedlimit"; 
static const char KEY_LOWSPEED_TIME[] = "lowspeedtime";
static const char KEY_ROOT_DIR[] = "rootdir";
static const char KEY_CONNECTION_TIMEOUT[] = "connectiontimeout";

static const char KEY_RESUME[] = "resume";
static const char KEY_FROMLASTRUN[] = "fromlastrun";
static const char KEY_FORCERUN[] = "forcerun";
static const char KEY_OVERWRITE[] = "overwrite";
static const char KEY_AUTOGENDESTDIRECTORY[] = "autogendestdirectory";
static const char KEY_TARGET_DIRECTORY[] = "targetdirectory";
static const char KEY_LOG_FILEPATH[] = "logfilepath";
static const char KEY_LOGLEVEL[] = "loglevel";
static const char KEY_MAXFILELISTERS[] = "maxfilelisters";
static const char KEY_GENERATEFILELIST[] = "generatefilelist";
static const char KEY_FILELISTPATH[] = "filelistpath";
static const char KEY_MAXLIFETIME[] = "maxlifetime";
static const char KEY_MAXCOPIES[] = "maxcopies";

static const char KEY_FILELISTFROMSOURCE[] = "filelistfromsource";
static const char KEY_FILELISTTOTARGET[] = "filelisttotarget";




/*
* Function Name: Constuctor (Special member function)
* Arguements:  Parameterised(1st --> no of cmd line args, 2nd --> cmdline arg values.)
* Return Value: No return type.
* Description: 
* Called by: ConfigValueObj's Createinstance function..
* Calls: No calls.
* Issues Fixed:
*/

ConfigValueParser::ConfigValueParser(int count, ACE_TCHAR** cmdLine)    
{
    m_count = count;
    m_cmdLine = cmdLine;
}

/*
* Function Name: getInstance     
* Arguements: reference to actual ConfigValueObj 's singleton Object. 
* Return Value: 
* Description: Ths will decide whether this is config file input or cmd line input.
* Called by: ConfigValueObj's Createinstance function..
* Calls: approprite function based on input..
* Issues Fixed:
*/

bool  ConfigValueParser::parseInput( ConfigValueObject& obj )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    int i=1;
    ACE_Tokenizer token1(m_cmdLine[i]);
	if( m_count == 1 || ACE_OS::strcmp(token1.next(),ACE_TEXT("--help")) == 0)  
    {  
        icatUsage();
        throw icatException("--help command given \n");
    }
    i= 1;
    ACE_Tokenizer token2(m_cmdLine[i]);
    if( ACE_OS::strcmp(token2.next(),ACE_TEXT("--resume")) == 0 && m_count > 2 )
    {
        obj.setResume(true);
        i=2;
    }
    ACE_TCHAR tempCmd[1000] ;
    ACE_OS::strcpy(tempCmd, m_cmdLine[i]) ;
    ACE_Tokenizer token3(tempCmd);
    token3.delimiter_replace('=', 0); 
    if( ACE_OS::strcmp(token3.next(),ACE_TEXT("--config")) == 0)
    {
        ACE_TCHAR *temp = token3.next();
        if(temp == NULL)
        {
            DebugPrintf(SV_LOG_ERROR, "Invalid argument for --config \n") ;
            throw icatException("Invalid argument for --config \n"); 
        }
		m_szconfigPath = ACE_TEXT_ALWAYS_CHAR(temp);
		parseConfigFileInput(obj);
    } 
    else
    {
        parseCmdLineInput(obj);
    }
 DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
 return true; 
}

/*
* Function Name: parseCmdLineInput     
* Arguements:  reference to actual ConfigValueObj 's singleton Object.
* Return Value: 
* Description: Ths will perform the actual command line parsing and fills into obj.
* Called by: parseInput() of this component ..
* Calls: 
* Issues Fixed:
*/

bool ConfigValueParser::parseCmdLineInput( ConfigValueObject& obj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERING %s\n", __FUNCTION__) ;
    if( obj.getResume())
    {
        m_cmdLine[1] =ACE_TEXT("");
        m_count--;
    }
    ACE_Get_Opt cmd_opts(m_count,m_cmdLine);
    bool retVal = true ;
    retVal = setLongOption(cmd_opts); 
    if( !retVal || m_count == 2 )
    {
        cmdLineUsage();    
        throw InvalidInput(" required arguments are not found \n");
    }

    int option;
    int set=0;
    ContentSource src;
    std::string global = "";
    std::string serial2 = "";
    ACE_TCHAR optionVal[ 100 ];
    char lastCommand ;
    ACE_TString lastString = ACE_TEXT("");
    std::list<std::string> lastStringList;
    int j=1;
    int first = 1;
    while ((option = cmd_opts ()) != EOF)
    {
        switch (option) 
        {		
        case 'a': 
            if(strstr(global.c_str(),"a")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());                
                obj.setNodes(ACE_TEXT_ALWAYS_CHAR(optionVal));
                lastString = ACE_TEXT("--nodes");
                ValidateArgument(optionVal,lastString);
                global.append("a");
                lastCommand = 'a';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --nodes is already given . Being Ignored. \n");               
            }
            break;
        case 'b':
            if(strstr(global.c_str(),"b")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());                
                obj.setDNSname(ACE_TEXT_ALWAYS_CHAR(optionVal));
                lastString = ACE_TEXT("--dnsname");
                ValidateArgument(optionVal,lastString);
                global.append("b");
                lastCommand = 'b';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --dnsname is already given . Being Ignored. \n");               
            }
            break;
        case 'c':
            if(strstr(global.c_str(),"c")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());                
                obj.setTransport(ACE_TEXT_ALWAYS_CHAR(optionVal));
                lastString = ACE_TEXT("--transport");
                ValidateArgument(optionVal,lastString);
                global.append("c");
                lastCommand = 'c';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --transport is already given . Being Ignored. \n");               
            }
            break;
        case 'd':
            if(strstr(global.c_str(),"d")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());                
                obj.setUID(ACE_TEXT_ALWAYS_CHAR(optionVal));
                lastString = ACE_TEXT("--uid");
                ValidateArgument(optionVal,lastString);
                global.append("d");
                lastCommand = 'd';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --uid is already given . Being Ignored. \n");               
            }
            break;
        case 'e':
            if(strstr(global.c_str(),"e")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());                
                obj.setGID(ACE_TEXT_ALWAYS_CHAR(optionVal));
                lastString = ACE_TEXT("--gid");
                ValidateArgument(optionVal,lastString);
                global.append("e");
                lastCommand = 'e';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --gid is already given . Being Ignored. \n");               
            }
            break;
        case 'f':
            if(strstr(global.c_str(),"f")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());                
                std::string t = ACE_TEXT_ALWAYS_CHAR(optionVal);                
                if( t.compare("false") == 0 || t.compare( "0" ) == 0 || t.compare("FALSE") == 0 )
                    obj.setAutoGenDestDirectory(false);
                else
                  obj.setAutoGenDestDirectory(true);
                lastString = ACE_TEXT("--autogendestdirectory");
                ValidateArgument(optionVal,lastString);
                global.append("f");
                lastCommand = 'f';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --autogendestdirectory is already given . Being Ignored. \n");               
            }
            break;
        case 'g':
            if(strstr(global.c_str(),"g")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());                
                obj.setServerName(ACE_TEXT_ALWAYS_CHAR(optionVal));
                lastString = ACE_TEXT("--servername");
                ValidateArgument(optionVal, lastString);
                global.append("g");
                lastCommand = 'g';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --servername is already given . Being Ignored. \n");               
            }
            break;
        case 'h':
            if(strstr(global.c_str(),"h")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());                
                obj.setBranchName(ACE_TEXT_ALWAYS_CHAR(optionVal));
                lastString = ACE_TEXT("--branchname");
                ValidateArgument(optionVal, lastString);
                global.append("h");
                lastCommand = 'h';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --branchname is already given . Being Ignored. \n");               
            }
            break;
        case 'i':
            if(strstr(global.c_str(),"i")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());                
                obj.setSourceVolume(ACE_TEXT_ALWAYS_CHAR(optionVal));
                lastString = ACE_TEXT("--sourcevolume");
                ValidateArgument(optionVal, lastString);
                global.append("i");
                lastCommand = 'i';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --sourcevolume is already given . Being Ignored. \n");               
            }
            break;
        case 'j':
            if(strstr(global.c_str(),"j")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());                
                obj.setTargetDirectory(ACE_TEXT_ALWAYS_CHAR(optionVal));
                lastString = ACE_TEXT("--targetdirectory");
                ValidateArgument(optionVal, lastString);
                global.append("j");
                lastCommand = 'j';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --targetdirectory is already given . Being Ignored. \n");               
            }
            break;
        case 'k':
            ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
            lastString = ACE_TEXT("--directory");
            ValidateArgument(optionVal, lastString);
            serial2 = "lBmEnop";
            if(set == 1)
            {
                obj.addContentSource(src); 
                src.m_Include = true;
                src.m_Size = 0;
                src.m_SizeOps = ">=";
                src.m_szDirectoryName = "";
                src.m_szExcludeList = "";
                src.m_szFilePattern = "*.*";
                src.m_szFilePatternOps = "=";
                src.m_szFilter1 = "and";
                src.m_szFilter2 = "and";
                src.m_szMtime = "";
                src.m_szMtimeOps = ">=";
                src.m_szfilterTemplate.clear();
            }
            lastCommand = 'k';
            src.m_szDirectoryName = ACE_TEXT_ALWAYS_CHAR(optionVal);
            set = 1;
            break;
        case 'l':
            if(strstr(serial2.c_str(),"l") != NULL && lastCommand == 'k')
            {
               ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
               lastString = ACE_TEXT("--excludelist");
               ValidateArgument(optionVal, lastString);
               src.m_szExcludeList = ACE_TEXT_ALWAYS_CHAR(optionVal);
               serial2.erase(serial2.find('l',0),1);
               set = 1;
               lastCommand = 'l';
            }
            else
            {
              throw ParseException("exclude list should be followed by --directory");
            }
            break;
        case 'B':
            if( strstr(serial2.c_str(),"B") != NULL && (lastCommand == 'l' || lastCommand == 'k') )                 
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg()); 
                lastString = ACE_TEXT("--include");
                ValidateArgument(optionVal, lastString);
                std::string t = ACE_TEXT_ALWAYS_CHAR(optionVal);
                if( t.compare("false") == 0 || t.compare( "0" ) == 0 || t.compare("FALSE") == 0 )
                      src.m_Include = false;
                else
                      src.m_Include = true;
                serial2.erase(serial2.find('B',0),1);
                set = 1;
                lastCommand = 'B';
            }
            else
            {
                throw ParseException("--include should be followed by either --exclude or --directory");
            }
            break;
        case 'm':
            if( strstr(serial2.c_str(),"m") != NULL && (lastCommand == 'k' || lastCommand == 'l' || lastCommand == 'B') || lastCommand == 'n')
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--namepattern");
                ValidateArgument(optionVal, lastString);
                src.m_szFilePattern = ACE_TEXT_ALWAYS_CHAR(optionVal);
                src.m_szfilterTemplate.push_back("pattern");
                serial2.erase(serial2.find('m',0),1);
                set = 1;
                lastCommand = 'm';
            }
            else
            {
               throw ParseException("\n un-orderd --namepattern \n");
            }
            break;
        case 'E':
            if(lastCommand == 'm' || lastCommand == 'o' || lastCommand == 'p')
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--comp");
                ValidateArgument(optionVal, lastString);
                if(lastCommand == 'm')
                {
                    src.m_szFilePatternOps = ACE_TEXT_ALWAYS_CHAR(optionVal);
                }
                else if(lastCommand == 'o')
                {
                    src.m_szMtimeOps= ACE_TEXT_ALWAYS_CHAR(optionVal);
                }
                else 
                {
                    src.m_SizeOps = ACE_TEXT_ALWAYS_CHAR(optionVal);
                    serial2.erase(serial2.find('E',0),1);
                }
                lastCommand = 'E';
               
            }
            else
            {
               throw ParseException("\n un-orderd --comp option \n");
            }
            break;
        case 'n':
            if(lastCommand == 'E' || lastCommand == 'm' || lastCommand == 'o' || lastCommand == 'p')
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--op");
                ValidateArgument(optionVal, lastString);
                if(j==1)
                {
                    src.m_szFilter1 = ACE_TEXT_ALWAYS_CHAR(optionVal);
                    j++;
                }
                else
                {
                    src.m_szFilter2 = ACE_TEXT_ALWAYS_CHAR(optionVal);
                    j=1;
                    serial2.erase(serial2.find('n',0),1);
                }
                src.m_szfilterTemplate.push_back(ACE_TEXT_ALWAYS_CHAR(optionVal));
                lastCommand = 'n';
            }
            else
            {
                throw ParseException("\n un-orderd --op \n");
            }
            break;
        case 'o':
           if( strstr(serial2.c_str(),"o") != NULL && (lastCommand == 'k' || lastCommand == 'l' || lastCommand == 'B' || lastCommand == 'n'))
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--date");
                ValidateArgument(optionVal, lastString);
                src.m_szMtime = ACE_TEXT_ALWAYS_CHAR(optionVal);
                src.m_szfilterTemplate.push_back("date");
                serial2.erase(serial2.find('o',0),1);
                set = 1;
                lastCommand = 'o';
            }
            else
            {
                throw ParseException("un-orderd --date");
            }
            break;
        case 'p':
           if( strstr(serial2.c_str(),"p") != NULL && (lastCommand == 'k' || lastCommand == 'l' || lastCommand == 'B' || lastCommand == 'n'))
            {
                 ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                 lastString = ACE_TEXT("--size");
                 ValidateArgument(optionVal, lastString);
                 src.m_Size = atoi(ACE_TEXT_ALWAYS_CHAR(optionVal));
                 src.m_szfilterTemplate.push_back("size");
                 serial2.erase(serial2.find('p',0),1);
            }
            else
            {
               throw ParseException("un-orderd --size");
            }
            lastCommand = 'p';
            break;
        case 'q':
            if(strstr(global.c_str(),"q")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--overwrite");
                ValidateArgument(optionVal, lastString);
                std::string t = ACE_TEXT_ALWAYS_CHAR(optionVal);
                if( t.compare("false") == 0 || t.compare( "0" ) == 0 || t.compare("FALSE") == 0 )
                    obj.setOverWrite( false );
                else
                    obj.setOverWrite( true );
                global.append("q");
                lastCommand = 'q';
                
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --overwrite is already given . Being Ignored. \n");               
            }
            break;
        case 'r':
            if(strstr(global.c_str(),"r")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--retrylimit");               
                ValidateArgument(optionVal, lastString);
                obj.setRetryLimit(atoi(ACE_TEXT_ALWAYS_CHAR(optionVal)));
                global.append("r");
                lastCommand = 'r';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --maxretries is already given . Being Ignored. \n");               
            }
            break;
        case 's':
            if(strstr(global.c_str(),"s")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--retryinterval");
                ValidateArgument(optionVal, lastString);
                obj.setRetryInterval(atoi(ACE_TEXT_ALWAYS_CHAR(optionVal)));
                global.append("s");
                lastCommand = 's';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --retryinterval is already given . Being Ignored. \n");               
            }
            break;
        case 't':
            if(strstr(global.c_str(),"t")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--exitonretryexpiry");
                ValidateArgument(optionVal, lastString);
                std::string t = ACE_TEXT_ALWAYS_CHAR(optionVal);
                if( t.compare("false") == 0 || t.compare( "0" ) == 0 || t.compare("FALSE") == 0 )
                {
                    obj.setExitOnRetryExpiry(false);
                }    
                else
                {
                    obj.setExitOnRetryExpiry(true);
                }    
                global.append("t");
                lastCommand = 't';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --extronretryexpiry is already given . Being Ignored. \n");               
            }
            break;
        case 'u':
            if(strstr(global.c_str(),"u")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--logfilepath");
                ValidateArgument(optionVal, lastString);
                obj.setLogFilePath(ACE_TEXT_ALWAYS_CHAR(optionVal));
                global.append("u");
                lastCommand = 'u';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " logfilepath is already given . Being Ignored. \n");               
            }
            break;
        case 'F':
            if(strstr(global.c_str(),"F")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--fromlastrun");
                ValidateArgument(optionVal, lastString);
                std::string t = ACE_TEXT_ALWAYS_CHAR(optionVal);
                if( t.compare("false") == 0 || t.compare( "0" ) == 0 || t.compare("FALSE") == 0 )
                    obj.setFromLastRun( false );
                else
                    obj.setFromLastRun( true );
                global.append("F");
                lastCommand = 'F'; 
                
            }  
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --fromlastrun is already given . Being Ignored. \n");               
            }
            break;
         case 'x':
            if(strstr(global.c_str(),"x")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--fourcerun"); 
                ValidateArgument(optionVal, lastString);
                std::string t = ACE_TEXT_ALWAYS_CHAR(optionVal);
                if( t.compare("false") == 0 || t.compare( "0" ) == 0 || t.compare("FALSE") == 0 )
                    obj.setForceRun( false );
                else
                    obj.setForceRun( true ) ;
                global.append("x");
                lastCommand = 'x';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --fourcerun is already given . Being Ignored. \n");               
            }
            break;
        case 'y':
            if(strstr(global.c_str(),"y")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--recvtcpbuffer"); 
                ValidateArgument(optionVal, lastString);
                obj.setTCPrecvbuffer(atoi(ACE_TEXT_ALWAYS_CHAR(optionVal)));
                global.append("y");
                lastCommand = 'y';
                
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --recvtcpbuffer is already given . Being Ignored. \n");               
            }
            break;
        case 'z':
            if(strstr(global.c_str(),"z")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--sendtcpbuffer");
                ValidateArgument(optionVal, lastString);
                obj.setTCPsendbuffer(atoi(ACE_TEXT_ALWAYS_CHAR(optionVal)));
                global.append("z");
                lastCommand = 'z';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --sendtcpbuffer is already given . Being Ignored. \n");               
            }
            break;
        case 'A':
            if(strstr(global.c_str(),"A")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--connects");
                ValidateArgument(optionVal, lastString);
                obj.setMaxConnects(atoi(ACE_TEXT_ALWAYS_CHAR(optionVal)));
                global.append("A");
                lastCommand = 'A';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --connects is already given . Being Ignored. \n");               
            }
            break;
        case 'L':
            if(strstr(global.c_str(),"L")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--lifetime");
                ValidateArgument(optionVal, lastString);
                obj.setMaxLifeTime(atoi(ACE_TEXT_ALWAYS_CHAR(optionVal)));
                global.append("L");
                lastCommand = 'L';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --lifetime is already given . Being Ignored. \n");               
            }
            break;
        case 'C':
            if(strstr(global.c_str(),"C")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--copies");
                ValidateArgument(optionVal, lastString);
                obj.setMaxCopies(atoi(ACE_TEXT_ALWAYS_CHAR(optionVal)));
                global.append("C");
                lastCommand = 'C';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --copies is already given . Being Ignored. \n");               
            }
            break;
        case 'D':
            if(strstr(global.c_str(),"D")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--rootdir");
                ValidateArgument(optionVal, lastString);
                obj.setArchiveRootDirectory(ACE_TEXT_ALWAYS_CHAR(optionVal));
                global.append("D");
                lastCommand = 'D';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --rootdir is already given . Being Ignored. \n");               
            }
            break;
        case 'G':
            if(strstr(global.c_str(),"G")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--retryonfailure");
                ValidateArgument(optionVal, lastString);
                std::string t = ACE_TEXT_ALWAYS_CHAR(optionVal);
                if( t.compare("false") == 0 || t.compare( "0" ) == 0 || t.compare("FALSE") == 0 )
                {
                    obj.setRetryOnFailure(false);
                }    
                else
                {
                    obj.setRetryOnFailure(true);
                }
                global.append("G");
                lastCommand = 'G';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --retryonfailure is already given . Being Ignored. \n");               
            }
            break;
        case 'H':
            if(strstr(global.c_str(),"H")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--loglevel");               
                ValidateArgument(optionVal, lastString);
                obj.setLogLevel(atoi(ACE_TEXT_ALWAYS_CHAR(optionVal)));
                global.append("H");
                lastCommand = 'H';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --loglevel is already given . Being Ignored. \n");               
            }
            break;
       case 'M':     
            if(strstr(global.c_str(),"M")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--lowspeedlimit");               
                ValidateArgument(optionVal, lastString);
                obj.setLowspeedLimt(atoi(ACE_TEXT_ALWAYS_CHAR(optionVal)));
                global.append("M");
                lastCommand = 'M';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --lowspeedlimit is already given . Being Ignored. \n");               
            }
            break;
        case 'N':    
            if(strstr(global.c_str(),"N")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--lowspeedtime");               
                ValidateArgument(optionVal, lastString);
                obj.setLowspeedTime(atoi(ACE_TEXT_ALWAYS_CHAR(optionVal)));
                global.append("N");
                lastCommand = 'N';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --lowspeedtime is already given . Being Ignored. \n");               
            }
            break;
        case 'J':  
            if(strstr(global.c_str(),"J")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());
                lastString = ACE_TEXT("--connectiontimeout");               
                ValidateArgument(optionVal, lastString);
                obj.setConnectionTimeout(atoi(ACE_TEXT_ALWAYS_CHAR(optionVal)));
                global.append("J");
                lastCommand = 'J';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --connectiontimeout is already given . Being Ignored. \n");               
            }
            break; 
		case 'S':
			if(strstr(global.c_str(),"S")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());                
                obj.setFilelistFromSource(ACE_TEXT_ALWAYS_CHAR(optionVal));
                lastString = ACE_TEXT("--filelistfromsource");
                ValidateArgument(optionVal, lastString);
                global.append("S");
                lastCommand = 'S';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --filelistfromsource  is already given . Being Ignored. \n");               
            }
            break;
		case 'T':
			if(strstr(global.c_str(),"T")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());                
                obj.setFilelistToTarget(ACE_TEXT_ALWAYS_CHAR(optionVal));
                lastString = ACE_TEXT("--filelisttotarget");
                ValidateArgument(optionVal, lastString);
                global.append("T");
                lastCommand = 'T';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --filelisttotarget is already given . Being Ignored. \n");               
            }
            break;
		case 'R':
			if(strstr(global.c_str(),"R")==NULL)
            {
                ACE_OS_String::strcpy (optionVal, cmd_opts.opt_arg());                
                obj.setmaxfilelisters(atoi(ACE_TEXT_ALWAYS_CHAR(optionVal)));
                lastString = ACE_TEXT("--maxfilelisters");
                ValidateArgument(optionVal, lastString);
                global.append("R");
                lastCommand = 'R';
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " --maxfilelisters is already given . Being Ignored. \n");               
            }
            break;
        case ':':
            throw InvalidInput("Option requires an argument");
            break;
        default:
            lastString = cmd_opts.last_option();
            lastStringList.push_back(ACE_TEXT_ALWAYS_CHAR(lastString.c_str()));
        }
    }
    for(std::list<std::string>::iterator it = lastStringList.begin(); it!=lastStringList.end(); )
    {
        DebugPrintf(SV_LOG_ERROR, "\n Either Option or argument --%s is InValid \n", (*it).c_str());
        it++;
        if(it == lastStringList.end() )
        {
            throw ParseException(" \n InValid Command Line options \n");
        }
    }
    if( set == 1 )
    {    
		char szBuffer[250];
        inm_strcpy_s( szBuffer,ARRAYSIZE(szBuffer), src.m_szDirectoryName.c_str()) ;
		for( char* pszToken = strtok(szBuffer, "," ); 
		   (NULL != pszToken);
		   (pszToken = strtok( NULL, "," ))) 
	       {
				src.m_szDirectoryName = pszToken;
                trimSpaces(src.m_szDirectoryName);
                obj.addContentSource(src);
           }
	}	
    ValidateInput(obj); 
    
    DebugPrintf(SV_LOG_DEBUG, "EXITING %s\n", __FUNCTION__) ;
    return true;
}

/*
* Function Name: fillRemoteOfficeDetails     
* Arguements:  imported config heap reference, sectionKey and reference to actual ConfigValueObj 's singleton Object.
* Return Value: Nothing.
* Description: This will perform the config parsing and fills the RemoteOffice structure of ConfigValueObject's singleton obj.
* Called by: parseConfigFileInput() of this component ..
* Calls: Calls appropriate set() functions to set values of RemoteOffice structure.
* Issues Fixed:
*/

void ConfigValueParser::fillRemoteOfficeDetails(  ACE_Configuration_Heap& configHeap, 
                                                  ACE_Configuration_Section_Key& sectionKey,
                                                  ConfigValueObject& obj ) 
{
    int keyIndex = 0 ;
    ACE_TString sectionName;
    ACE_TString tStrKeyName;
    ACE_Configuration::VALUETYPE valueType = ACE_Configuration::STRING ;

    while(configHeap.enumerate_values(sectionKey, keyIndex, tStrKeyName, valueType) == 0)
    {
        ACE_TString tStrValue ;
        configHeap.get_string_value(sectionKey, tStrKeyName.c_str(), tStrValue) ;
        keyIndex++;

		std::string keyName = ACE_TEXT_ALWAYS_CHAR(tStrKeyName.c_str());
		std::string value = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());

        if( value.empty() == false )
        {
            if( keyName.compare(KEY_SERVER_NAME) == 0 )
            {
                obj.setServerName( value.c_str() ) ;
            }
            else if( keyName.compare( KEY_BRANCH_NAME ) == 0 )
            {
                obj.setBranchName( value.c_str() );
            }
            else if( keyName.compare( KEY_SOURCE_VOLUME ) == 0 )
            {
                obj.setSourceVolume( value.c_str() );
            } 
            else
            {
                DebugPrintf(SV_LOG_ERROR, " Invalid Key %s under section %s . Being Ignored. \n",keyName.c_str(),sectionName.c_str());               
            }
        }
    }
}

/*
* Function Name: fillHcapDetails     
* Arguements:  imported config heap reference, sectionKey and reference to actual ConfigValueObj 's singleton Object.
* Return Value: Nothing.
* Description: This will perform the config parsing and fills the hcap structure of ConfigValueObject's singleton obj.
* Called by: parseConfigFileInput() of this component ..
* Calls: Calls appropriate set() functions to set values of hcap structure.
* Issues Fixed:
*/

void ConfigValueParser::fillHcapDetails(ACE_Configuration_Heap& configHeap, 
                                        ACE_Configuration_Section_Key& sectionKey,
                                        ConfigValueObject& obj)
{
    int keyIndex = 0 ;
    ACE_TString sectionName;
    ACE_TString tStrKeyName;
    ACE_Configuration::VALUETYPE valueType = ACE_Configuration::STRING ;

    while(configHeap.enumerate_values(sectionKey, keyIndex, tStrKeyName, valueType) == 0)
    {
        ACE_TString tStrValue ;
        configHeap.get_string_value(sectionKey, tStrKeyName.c_str(), tStrValue) ;
        keyIndex++;

		std::string keyName = ACE_TEXT_ALWAYS_CHAR(tStrKeyName.c_str());
		std::string value = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());

        if( value.empty() == false )
        {
            if( keyName.compare( KEY_TRANSPORT )== 0 )
            {
                obj.setTransport(value.c_str()); 
            }
            else if( keyName.compare( KEY_NODES ) == 0 )
            {
                obj.setNodes(value.c_str());
            }
            else if( keyName.compare( KEY_DNS_NAME ) == 0 )
            {
                obj.setDNSname(value.c_str());
            }
            else if(keyName.compare(KEY_ROOT_DIR)==0)
            {
                obj.setArchiveRootDirectory(value.c_str());
            }
            else if( keyName.compare( KEY_UID ) == 0 )
            {
                obj.setUID(value.c_str());
            }
            else if( keyName.compare( KEY_GID ) == 0 )
            {
                obj.setGID(value.c_str());
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "Invalid Key %s under section %s . Being Ignored. \n",
                    keyName.c_str(),
                    sectionName.c_str());
            }
        }
    }
    //Values otherthan the following are not supported for time being
    obj.setUID("0"); 
    obj.setGID("0"); 
}

/*
* Function Name: parseFilePattern     
* Arguements:  tempString which contains the value of filefilter key of content.source section and content.source structure.
* Return Value: Nothing.
* Description: This will perform the config parsing and fills the content.source structure of ConfigValueObject's singleton obj.
* Called by: fillContenetSourceDetails() of this component ..
* Calls: 
* Issues Fixed:
*/

void ConfigValueParser::parseFilePattern( std::string tempValue, ContentSource& src )
{

    std::string tempArray[5] ;
    int count = 0 ;
    char szBuffer[250];
    inm_strcpy_s( szBuffer,ARRAYSIZE(szBuffer), tempValue.c_str() ) ;
    
    for( char* pszToken = strtok(szBuffer, " \t" ); 
		(NULL != pszToken);
		(pszToken = strtok( NULL, " \t" ))) 
	{
        tempArray[count++] = pszToken;
    }
    
    if(count > ICAT_FILEFILTER_SIZE || count%2 == 0)
    {
        throw InvalidInput(" Invalid no.of Filters provided. Correct it before retry...");  
    }
    
    std::string::size_type length = std::string::npos ;
    std::string tempSubString;
    
    for(int i =0; i<count; i++)
    {
        if( i%2 == 0 )
        {
           size_t pos1 = tempArray[i].find_first_of("!>=<"); 
           size_t pos2 = tempArray[i].find_last_of(">=<");
           if( pos1 == std::string::npos || 
               pos2 == std::string::npos )
           {
                throw InvalidInput("Invalid filepattern must have delimeter ");
           }
           tempSubString = tempArray[i].substr(0,pos1);
           trimSpaces(tempSubString);
           length = tempArray[i].size();
           
           pos2++ ;
           std::string filterComparator = tempArray[i].substr( pos1,( pos2 - pos1 ) );
           std::string filterCondition = tempArray[i].substr( pos2, length - pos2 ) ;
           trimSpaces( filterComparator ) ;
           trimSpaces( filterCondition ) ;

           if( filterCondition.empty() ) 
               continue ;

		   if( tempSubString.compare( "pattern" ) == 0 )
		   {
               src.m_szFilePatternOps = filterComparator ;
               src.m_szFilePattern = filterCondition ;              
           }
		   else if ( tempSubString.compare("date") == 0 )
		   {
               src.m_szMtimeOps = filterComparator ;
               src.m_szMtime = filterCondition ; 			   
           }
		   else if( tempSubString.compare("size") == 0 ) 
		   {
               src.m_SizeOps = filterComparator ;
               src.m_Size = convertStringtoULL(filterCondition) ;                  
		   }
		   else
		   {
				throw InvalidInput(" Invalid Options in filepattern ");
		   }

           src.m_szfilterTemplate.push_back(tempSubString);
        }
        else
        {
            if(!(tempArray[i].compare("and") == 0 || tempArray[i].compare("or") == 0) )
            {
                throw InvalidInput(" Invalid and/or operators in filepattern ");
            }
            src.m_szfilterTemplate.push_back(tempArray[i]); 
        }    
    }    
}

/*
* Function Name: fillContenetSourceDetails     
* Arguements:  imported config heap reference, sectionKey and reference to actual ConfigValueObj 's singleton Object.
* Return Value: Nothing.
* Description: This will perform the config parsing and fills the ContentSource structure of ConfigValueObject's singleton obj.
* Called by: parseConfigFileInput() of this component ..
* Calls: Calls parseFilePattern().
* Issues Fixed:
*/

void ConfigValueParser::fillContenetSourceDetails(ACE_Configuration_Heap& configHeap, 
                                                  ACE_Configuration_Section_Key& sectionKey, 
                                                  ContentSource& src)
{
    int keyIndex = 0 ;
    ACE_TString sectionName;
    ACE_TString tStrKeyName;
    ACE_Configuration::VALUETYPE valueType = ACE_Configuration::STRING ;

    while(configHeap.enumerate_values(sectionKey, keyIndex, tStrKeyName, valueType) == 0)
    {
        ACE_TString tStrValue ;
        configHeap.get_string_value(sectionKey, tStrKeyName.c_str(), tStrValue) ;
        keyIndex++;

		std::string keyName = ACE_TEXT_ALWAYS_CHAR(tStrKeyName.c_str());
		std::string value = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());

        if( value.empty() == false )
        {
            if( keyName.compare(KEY_DIRECTORY_NAME)==0)
            {
                src.m_szDirectoryName = value.c_str() ;
                trimSpaces(src.m_szDirectoryName);    
            }
            else if(keyName.compare(KEY_EXCLUDE_LIST)==0)
            {
                src.m_szExcludeList = value.c_str() ;
                trimSpaces(src.m_szExcludeList); 
            }
            else if(keyName.compare(KEY_FILE_FILTER)==0)
            {
                parseFilePattern( value.c_str(), src ) ;
            }
            else if(keyName.compare(KEY_INCLUDE)==0)
            {
                
                if( value.compare("false") == 0 || 
                    value.compare( "0" ) == 0 ||
                    value.compare("FALSE") == 0 )
                      src.m_Include = false;
                else
                      src.m_Include = true;
            }
            else
            {
                    DebugPrintf(SV_LOG_ERROR, 
                        "Invalid Key %s under section %s . Being Ignored. \n",
                        keyName.c_str(),sectionName.c_str());
            }            
        }            
    }
}

/*
* Function Name: fillTunablesDetails     
* Arguements:  imported config heap reference, sectionKey and reference to actual ConfigValueObj 's singleton Object.
* Return Value: Nothing.
* Description: This will perform the config parsing and fills the tunables structure of ConfigValueObject's singleton obj.
* Called by: parseConfigFileInput() of this component ..
* Calls: Calls appropriate set() functions to set values of hcap structure.
* Issues Fixed:
*/

void ConfigValueParser::fillTunablesDetails(ACE_Configuration_Heap& configHeap, 
                                            ACE_Configuration_Section_Key& sectionKey,
                                            ConfigValueObject& obj)

{
    int keyIndex = 0 ;
    ACE_TString sectionName;
    ACE_TString tStrKeyName;
    ACE_Configuration::VALUETYPE valueType = ACE_Configuration::STRING ;

    while(configHeap.enumerate_values(sectionKey, keyIndex, tStrKeyName, valueType) == 0)
    {
        ACE_TString tStrValue ;
        configHeap.get_string_value(sectionKey, tStrKeyName.c_str(), tStrValue) ;
        keyIndex++;

		std::string keyName = ACE_TEXT_ALWAYS_CHAR(tStrKeyName.c_str());
		std::string value = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());

        if( value.empty() == false )
        {
            if(keyName.compare(KEY_RETRY_ON_FAILURE) == 0)
            {
                if( value.compare("false") == 0 || value.compare( "0" ) == 0 || value.compare("FALSE") == 0 )
                {
                    obj.setRetryOnFailure(false);
                }    
                else
                {
                    obj.setRetryOnFailure(true);
                }        
            }
            else if(keyName.compare(KEY_RETRY_LIMIT) == 0)
            {
                obj.setRetryLimit(atoi(value.c_str()));
            }
            else if(keyName.compare(KEY_RETRY_INTERVAL)==0)
            {
                obj.setRetryInterval(atoi(value.c_str()));
            }
            else if(keyName.compare(KEY_EXITONRETRYEXPIRY) == 0)
            {
                if( value.compare("false") == 0 || value.compare( "0" ) == 0 || value.compare("FALSE") == 0 )
                {
                    obj.setExitOnRetryExpiry(false);
                }    
                else
                {
                    obj.setExitOnRetryExpiry(true);
                }        
            }
            else if(keyName.compare(KEY_MAX_CONNECTS)==0)
            {
                obj.setMaxConnects(atoi(value.c_str()));
            } 
            else if(keyName.compare(KEY_TCP_RECV_BUFFER) == 0)
            {
                obj.setTCPrecvbuffer(atoi(value.c_str()));
            }
            else if(keyName.compare(KEY_TCP_SEND_BUFFER)==0)
            {
                obj.setTCPsendbuffer(atoi(value.c_str()));
            }
            else if(keyName.compare(KEY_LOWSPEED_LIMT)==0)     
            {
                obj.setLowspeedLimt(atoi(value.c_str()));
            }
            else if(keyName.compare(KEY_LOWSPEED_TIME)==0)     
            {
                obj.setLowspeedTime(atoi(value.c_str()));
            }
            else if(keyName.compare(KEY_CONNECTION_TIMEOUT)==0)
            {
                obj.setConnectionTimeout(atoi(value.c_str()));
            }
			else if(keyName.compare(KEY_MAXFILELISTERS)==0)
            {
                obj.setmaxfilelisters(atoi(value.c_str()));
				
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " Illegal Key %s under section %s  Ignoring...\n",keyName.c_str(),sectionName.c_str());	
            }
        }
    }

}

/*
* Function Name: fillConfigDetails     
* Arguements:  imported config heap reference, sectionKey and reference to actual ConfigValueObj 's singleton Object.
* Return Value: Nothing.
* Description: This will perform the config parsing and fills the config structure of ConfigValueObject's singleton obj.
* Called by: parseConfigFileInput() of this component ..
* Calls: Calls appropriate set() functions to set values of config structure.
* Issues Fixed:
*/

void ConfigValueParser::fillConfigDetails(ACE_Configuration_Heap& configHeap, 
                                            ACE_Configuration_Section_Key& sectionKey,
                                            ConfigValueObject& obj)
{
    int keyIndex = 0 ;
    ACE_TString sectionName;
    ACE_TString tStrKeyName;
    ACE_Configuration::VALUETYPE valueType = ACE_Configuration::STRING ;

    while(configHeap.enumerate_values(sectionKey, keyIndex, tStrKeyName, valueType) == 0)
    {
        ACE_TString tStrValue ;
        configHeap.get_string_value(sectionKey, tStrKeyName.c_str(), tStrValue) ;
        keyIndex++;

		std::string keyName = ACE_TEXT_ALWAYS_CHAR(tStrKeyName.c_str());
		std::string value = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());

        if( value.empty() == false )
        {
           if( keyName.compare( KEY_FROMLASTRUN ) == 0)
            {
                if( value.compare("false") == 0 || value.compare( "0" ) == 0 || value.compare("FALSE") == 0 )
                    obj.setFromLastRun( false );
                else
                    obj.setFromLastRun( true );
            }
            else if(keyName.compare( KEY_FORCERUN )==0)
            {
                if( value.compare("false") == 0 || value.compare( "0" ) == 0 || value.compare("FALSE") == 0 )
                    obj.setForceRun( false );
                else
                    obj.setForceRun( true ) ;
            }
            else if(keyName.compare( KEY_OVERWRITE )==0)
            {
                if( value.compare("false") == 0 || value.compare( "0" ) == 0 || value.compare("FALSE") == 0 )
                    obj.setOverWrite( false );
                else
                    obj.setOverWrite( true );
            }
            else if(keyName.compare( KEY_AUTOGENDESTDIRECTORY )==0)
            {
                if( value.compare("false") == 0 || value.compare( "0" ) == 0 || value.compare("FALSE") == 0 )
                    obj.setAutoGenDestDirectory(false);
                else
                  obj.setAutoGenDestDirectory(true);
            }
            else if(keyName.compare( KEY_TARGET_DIRECTORY )==0)
            {
                obj.setTargetDirectory(value.c_str());
            }
            else if(keyName.compare( KEY_LOG_FILEPATH )==0)
            {
                obj.setLogFilePath(value.c_str());
            }
            else if(keyName.compare( KEY_LOGLEVEL )==0)
            {
                obj.setLogLevel(atoi(value.c_str()));
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " Illegal Key %s under section %s  Ignoring...\n",keyName.c_str(),sectionName.c_str());	
            }  	
        }
    }
}

/*
* Function Name: fillDeleteDetails     
* Arguements:  imported config heap reference, sectionKey and reference to actual ConfigValueObj 's singleton Object.
* Return Value: Nothing.
* Description: This will perform the config parsing and fills the delete structure of ConfigValueObject's singleton obj.
* Called by: parseConfigFileInput() of this component ..
* Calls: Calls appropriate set() functions to set values of delete structure.
* Issues Fixed:
*/

void ConfigValueParser::fillDeleteDetails(ACE_Configuration_Heap& configHeap, 
                                            ACE_Configuration_Section_Key& sectionKey,
                                            ConfigValueObject& obj)
{
    int keyIndex = 0 ;
    ACE_TString sectionName;
    ACE_TString tStrKeyName;
    ACE_Configuration::VALUETYPE valueType = ACE_Configuration::STRING ;

    while(configHeap.enumerate_values(sectionKey, keyIndex, tStrKeyName, valueType) == 0)
    {
        ACE_TString tStrValue ;
        configHeap.get_string_value(sectionKey, tStrKeyName.c_str(), tStrValue) ;
        keyIndex++;

		std::string keyName = ACE_TEXT_ALWAYS_CHAR(tStrKeyName.c_str());
		std::string value = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());

        if( value.empty() == false )
        {
            if(keyName.compare( KEY_MAXLIFETIME ) == 0)
            {
                obj.setMaxLifeTime(atoi(value.c_str()));
            } 
            else if( keyName.compare( KEY_MAXCOPIES ) == 0)
            {
                obj.setMaxCopies(atoi(value.c_str()));
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, " Illegal Key %s under section %s  Ignoring...\n",keyName.c_str(),sectionName.c_str());	
            }  	
        }
    }
}

void ConfigValueParser::fillFilelistDetails(ACE_Configuration_Heap& configHeap, 
                                        ACE_Configuration_Section_Key& sectionKey,
                                        ConfigValueObject& obj)
{
    int keyIndex = 0 ;
    ACE_TString sectionName;
    ACE_TString tStrKeyName;
    ACE_Configuration::VALUETYPE valueType = ACE_Configuration::STRING ;

    while(configHeap.enumerate_values(sectionKey, keyIndex, tStrKeyName, valueType) == 0)
    {
        ACE_TString tStrValue ;
        configHeap.get_string_value(sectionKey, tStrKeyName.c_str(), tStrValue) ;
        keyIndex++;

		std::string keyName = ACE_TEXT_ALWAYS_CHAR(tStrKeyName.c_str());
		std::string value = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());

        if( value.empty() == false )
        {
            if( keyName.compare( KEY_FILELISTFROMSOURCE )== 0 )
            {
                obj.setFilelistFromSource(value.c_str()); 
            }
            else if( keyName.compare( KEY_FILELISTTOTARGET ) == 0 )
            {
                obj.setFilelistToTarget(value.c_str());
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "Invalid Key %s under section %s . Being Ignored. \n",
                    keyName.c_str(),
                    sectionName.c_str());
            }
        }
    }
    //Values otherthan the following are not supported for time being
    obj.setUID("0"); 
    obj.setGID("0"); 
}


/*
* Function Name: parseConfigFileInput     
* Arguements:  reference to actual ConfigValueObj 's singleton Object.
* Return Value: 
* Description: This will perform the actual config parsing and fills the obj.
* Called by: parseInput() of this component ..
* Calls: approprite functions to fill respective structures of ConfigValueObject's singleton obj.
* Issues Fixed:
*/

bool ConfigValueParser::parseConfigFileInput( ConfigValueObject& obj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
   
    ACE_Configuration_Heap configHeap ;
    
    if( configHeap.open() >= 0 )
    {
        ACE_Ini_ImpExp iniImporter( configHeap );
        if(iniImporter.import_config(ACE_TEXT_CHAR_TO_TCHAR(m_szconfigPath.c_str())) >= 0)
        {
	        varifyConfigDuplication();     
            int sectionIndex = 0 ;
            ACE_Configuration_Section_Key sectionKey;
            ACE_TString tStrSectionName;
            Nodes tempnode ;

            while( configHeap.enumerate_sections(configHeap.root_section(), sectionIndex, tStrSectionName) == 0 )
            {
                configHeap.open_section(configHeap.root_section(), tStrSectionName.c_str(), 0, sectionKey) ;

				std::string sectionName = ACE_TEXT_ALWAYS_CHAR(tStrSectionName.c_str());

                if( sectionName.compare( SECTION_REMOTE_OFFICE ) == 0 )
                {
                    fillRemoteOfficeDetails( configHeap,sectionKey, obj ) ;
                }
                else if(sectionName.compare( SECTION_ARCHIVE_REPOSITORY ) == 0 )
                {
                    fillHcapDetails( configHeap,sectionKey, obj);
                }
                else if(strstr(sectionName.c_str(),SECTION_CONTENT_SOURCE) != NULL)
                {
                    ContentSource src ; 
                    fillContenetSourceDetails(configHeap,sectionKey, src);
                    char szBuffer[250];
                    inm_strcpy_s( szBuffer, ARRAYSIZE(szBuffer),src.m_szDirectoryName.c_str()) ;
    
                    for( char* pszToken = strtok(szBuffer, "," ); 
		                (NULL != pszToken);
		                (pszToken = strtok( NULL, "," ))) 
	                {
                        src.m_szDirectoryName = pszToken;
                        trimSpaces(src.m_szDirectoryName);
                        obj.addContentSource(src);
                    }
                }
                else if(sectionName.compare( SECTION_TUNABLES ) == 0)
                {
                    fillTunablesDetails(configHeap,sectionKey, obj);
                }
                else if(sectionName.compare( SECTION_CONFIG ) == 0)
                {
                    fillConfigDetails(configHeap,sectionKey, obj);
                }
                else if(sectionName.compare( SECTION_DELETE ) == 0)
                {
                    fillDeleteDetails(configHeap,sectionKey, obj);
                }
				else if(sectionName.compare( SECTION_FILELIST) == 0)
                {
                    fillFilelistDetails(configHeap,sectionKey, obj);
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, " Illegal Section %s . Ignoring...\n",sectionName.c_str());	            
                }
                sectionIndex++; 
            }
            ValidateInput(obj); 
        }
        else
        {
            throw ParseException("FAILED to Read Configuration File\n");
        }
    }
    else
    {
        throw ParseException("FAILED to Initialize Parser object.");
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return true ;
}

/*
* Function Name: setLongOption     
* Arguements:  reference to ACE_Get_Opt 's cmd_opts.
* Return Value : bool , returns true if successfully set short option to long option, otherewise false.
* Description: This will set appropriate short options to long options.
* Called by: parseCmdLineInput() of this component ..
* Calls: long_option functions to set long option.
* Issues Fixed:
*/

bool ConfigValueParser::setLongOption( ACE_Get_Opt& cmd_opts)
{
    if( cmd_opts.long_option(ACE_TEXT("nodes"),'a',ACE_Get_Opt::ARG_REQUIRED) == -1)
        {    
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("dnsname"),'b',ACE_Get_Opt::ARG_REQUIRED) == -1)
        {
            return false ;
        }     
    if( cmd_opts.long_option(ACE_TEXT("transport"),'c',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("uid"),'d',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("gid"),'e',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }    
    if( cmd_opts.long_option(ACE_TEXT("autogendestdirectory"),'f',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("servername"),'g',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("branchname"),'h',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("sourcevolume"),'i',ACE_Get_Opt::ARG_REQUIRED) == -1)
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("targetdirectory"),'j',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("directory"),'k',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    
    if( cmd_opts.long_option(ACE_TEXT("excludelist"),'l',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("include"),'B',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("namepattern"),'m',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("op"),'n',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("date"),'o',ACE_Get_Opt::ARG_REQUIRED)==-1)
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("size"),'p',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("overwrite"),'q',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("maxretries"),'r',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("retryinterval"),'s',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("exitonretryexpiry"),'t',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("logpath"),'u',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("fromlastrun"),'F',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("forcerun="),'x',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("recvtcpbuffer"),'y',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("sendtcpbuffer"),'z',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("connects"),'A',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("lifetime"),'L',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("copies"),'C',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("comp"),'E',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("rootdir"),'D',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("retryonfailure"),'G',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }           
    if( cmd_opts.long_option(ACE_TEXT("loglevel"),'H',ACE_Get_Opt::ARG_REQUIRED) == -1 )
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("lowspeedlimit"),'M',ACE_Get_Opt::ARG_REQUIRED) == -1 )  
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("lowspeedtime"),'N',ACE_Get_Opt::ARG_REQUIRED) == -1 )  
        {
            return false ;
        }
    if( cmd_opts.long_option(ACE_TEXT("connectiontimeout"),'J',ACE_Get_Opt::ARG_REQUIRED) == -1 ) 
        {
            return false ;
        } 
	if( cmd_opts.long_option(ACE_TEXT("filelistfromsource"),'S',ACE_Get_Opt::ARG_REQUIRED) == -1 ) 
		{
			return false ;
		}
	if( cmd_opts.long_option(ACE_TEXT("filelisttotarget"),'T',ACE_Get_Opt::ARG_REQUIRED) == -1 ) 
		{
			return false ;
		}
	if( cmd_opts.long_option(ACE_TEXT("maxfilelisters"),'R',ACE_Get_Opt::ARG_REQUIRED) == -1 ) 
		{
			return false ;
		}	
		
    return true;

}

/*
* Function Name: ValidateInput     
* Arguements: reference to actual ConfigValueObj 's singleton Object.
* Return Value : Nothing..
* Description: This will validate input as per the CVO 's bussiness rules and valid input.
* Called by: either parseConfigFileInput() or parseCmdLineInput() at last to validate input.
* Calls: 
* Issues Fixed:
*/

void ConfigValueParser::ValidateInput( ConfigValueObject& obj)
{
    if( obj.getResume() == true )
        obj.setOverWrite( true );
    
    std::string tempString;
    
    if(obj.m_config.m_AutoGenDestDirectory == 1)
    {
        if(obj.m_remoteOffice.m_szServerName.empty() || 
            obj.m_remoteOffice.m_szBranchName.empty() || 
            obj.m_remoteOffice.m_szSourceVolume.empty())
        {
            throw InvalidInput("AutoGenDestDirectory is set to 1 so should provide remote office details ");              
        }
    }
    else
    {
		if(obj.m_config.m_szTargetDirectory.empty())
        {
            throw InvalidInput("AutoGenDestDirectory is set to 0 so should provide target directory details ");              
        }
        DebugPrintf(SV_LOG_WARNING, "AutoGenDestDirectory is set to 0,maxcopies option ignored\n");
        std::cout<<"AutoGenDestDirectory is set to 0,maxcopies option ignored only when target directory name is same for every run"<<endl;
    }
    
    list<Nodes>& nodeList = obj.getNodeList();
    list<Nodes>::iterator it=nodeList.begin();
	
    if( strcmp( obj.getTransport().c_str(),"nfs" ) != 0 && obj.m_hcap.m_szDNSname.empty() && it == nodeList.end())
    {
        throw InvalidInput("should provide Ip or DNSname any one of the details");
    }
	else if ( (strcmp( obj.getTransport().c_str(),"nfs" ) == 0 ||  strcmp( obj.getTransport().c_str(),"cifs" ) == 0) && (nodeList.size() > 1))
    {
        throw InvalidInput("Should provide only one node for CIFS/NFS");
    }
    else
    {        
        while(it!=nodeList.end()) 
	    {
            if((*it).m_szIp.empty())
            {
               throw InvalidInput("should provide Ip or DNSname any one of the details ");
            }
            if((*it).m_Port < 1 || (*it).m_Port > 65535 )
            {
                throw InvalidInput("port number should be in between 0 and 65536 ");
            }
           
            it++;
        } 
    } 
   
   //content source list validation ...   
	
	std::string FilelistFromSrc= obj.getFilelistFromSource();
    if(FilelistFromSrc.empty() == true) 
    {

		list<ContentSource>& contentList =  obj.getContentSrcList();
		list<ContentSource>::iterator it1 = contentList.begin() ;
	    if(it1==contentList.end())
    	{
        	throw InvalidInput("sholuld provide atleast one source details");  
    	}
	    for(; it1!=contentList.end(); it1++)
		{
        	if((*it1).m_szDirectoryName.empty() == true)
        	{
           	throw InvalidInput(" should provide a source directory name ");
        	}
        
	        std::string myString = (*it1).m_szDirectoryName;
    	    size_t found;
        	found=myString.find_last_of("/\\");
	        size_t size = myString.size();

#ifdef WIN32

    	    if( found == std::string::npos )
        	{ 
            	if( myString.size() == 1 )
            	{
                	myString += ":\\" ;
            	}
	            else if( myString.size() == 2 )
    	        {
        	        myString += "\\";
           	 }
            	else
            	{	
                	throw InvalidInput(" Invalid source directory name ");
	            }
        	}
    	    else if( found == 0 || found == 1 || found == 3 )
        	{
                throw InvalidInput(" Invalid source directory name ");
	       	 }
    	    else if( found > 3 && found == size - 1 )
        	{
                myString.erase( found, 1 );
    	   	}
	
#else
       		if( found == std::string::npos || found == 1 )
       		{
            	throw InvalidInput(" Invalid source directory name ");
       		}
    		else if( found > 1 && found == size - 1 )
	      	{
            	myString.erase( found, 1 );
       		}
#endif
	        (*it1).m_szDirectoryName = myString;

	        if( (*it1).m_szFilter1.compare("and") !=0 &&  (*it1).m_szFilter1.compare("or") !=0 )
    	    {
        	    (*it1).m_szFilter1 = "and"; 
        	}
	        if(isValidDate((*it1).m_szMtime) == false)
    	    {
        	    throw InvalidInput(" Invalid time entry in filepattern ");
        	}
	        if((*it1).m_szFilter2.compare("and") !=0 && (*it1).m_szFilter2.compare("or") !=0)
    	    {
        	   (*it1).m_szFilter2 = "and"; 
       		}
	        if((*it1).m_szFilePatternOps.compare("!=") != 0 && (*it1).m_szFilePatternOps.compare("=") != 0)
    	    {
        	    throw InvalidInput("Invalid Operators for pattern in filefilter, it should be either != or = ");
      		}
	        if( ( (*it1).m_szMtimeOps.compare(">=") != 0 ) && 
    	        ( (*it1).m_szMtimeOps.compare("<=") != 0 ) &&
        	    ( (*it1).m_szMtimeOps.compare(">") != 0 ) && 
            	( (*it1).m_szMtimeOps.compare("<") != 0 ) &&
	            ( (*it1).m_szMtimeOps.compare("=") != 0 ) ) 

    	   {
        	    throw InvalidInput("Invalid Operators for date in filefilter, it should be either > , < , >= or <= ");
       	   }
	        if( ( (*it1).m_SizeOps.compare(">=") != 0 ) && 
    	        ( (*it1).m_SizeOps.compare("<=") != 0 ) &&
        	    ( (*it1).m_SizeOps.compare(">") != 0 ) && 
            	( (*it1).m_SizeOps.compare("<") != 0 ) &&  
            	( (*it1).m_SizeOps.compare("=") != 0 ) )

        	{
            	throw InvalidInput("Invalid Operators for size in filefilter, it should be either > , < , >= or <= ");
        	}
    	}
	}
    // logfile path validation...

    std::string logpath = obj.getLogFilePath();
    std::string installPath = logpath;
    if(installPath.empty() == true)
    {
        installPath = getLogFilePath();
    }
    else
    {
        size_t found;
        found=installPath.find_last_of("/\\");
        size_t size = installPath.size();
        if(found == size-1)
        {
            installPath.erase(found,1);
        }
        if(SVMakeSureDirectoryPathExists(installPath.c_str()).failed())
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to create logpath directory %s.\n", installPath.c_str());
            installPath = getLogFilePath() ; 
        }
    }
    
    std::stringstream str ;
	std::string SrcVolume;
	SrcVolume = obj.m_remoteOffice.m_szSourceVolume;
    str << installPath << getPathSeparator() ;
    if(!obj.m_remoteOffice.m_szServerName.empty() && 
            !obj.m_remoteOffice.m_szBranchName.empty() && 
            !obj.m_remoteOffice.m_szSourceVolume.empty())
    {
		formatingSrcVolume(SrcVolume);
		sliceAndReplace(SrcVolume,"/\\","_");
	    str << obj.m_remoteOffice.m_szBranchName << "_" 
            << obj.m_remoteOffice.m_szServerName << "_" << SrcVolume  ;
       
    }
    else
    {
        std::string target = obj.m_config.m_szTargetDirectory ;
        sliceAndReplace(target,"/\\","_");
        str << target; 
        
    }
    time_t start_time = obj.getStartTime();
    std::string timeStr;
    getTimeToString("%Y-%m-%d-%H-%M-%S", timeStr, &start_time) ;
    str <<"_" << timeStr;
    str << "_icat.log" ;
    obj.setLogFilePath( str.str() ); 
    
    if(obj.getArchiveRootDirectory().empty() == true)
    {
         throw InvalidInput("\n archive root directory details should be given");  
    }
      
    //  log level validation..
    if(obj.getLogLevel() < 0 || obj.getLogLevel() > 7)
    {
       obj.setLogLevel(3); 
    } 
#ifdef SV_WINDOWS
    if( strcmp( obj.getTransport().c_str(),"nfs" )== 0 )
    {
        std::stringstream temp;
            temp << "\n\n  ICAT does not support \"nfs\" transport in Windows."
                <<"  \n \n ICAT exiting ...";
            throw ParseException(temp.str());
    }

#else
    if( strcmp( obj.getTransport().c_str(),"cifs" )== 0 )
    {
    
        std::stringstream temp;
            temp << "\n\n ICAT does not support \"cifs\" transport in Non-Windows."
                 << " \n \n ICAT exiting ...";
            throw ParseException(temp.str());
    }
#endif
	if(obj.getMaxConnects() < obj.getmaxfilelisters())
    {
        std::stringstream temp;
            temp << "\n\n maxconnects should greater than maxfilelisters.";
            
		throw ParseException(temp.str());
    } 
	//filelist section validations
	
	std::string FilelistFromSource = obj.getFilelistFromSource();
    if(FilelistFromSource.empty() == false)
    {
       obj.setmaxfilelisters(1);
	   obj.setMaxConnects(1);
    }
	std::string FilelistToTarget = obj.getFilelistToTarget();
	if(FilelistToTarget.empty() == false)
	{
		obj.setMaxConnects(1);
		obj.setmaxfilelisters(1);
	}
	if(FilelistFromSource.empty() == false && FilelistToTarget.empty() == false)
	{
		throw InvalidInput("\n Configure either filelistfromsource or filelisttotarget");
	}	
    return ;
}

/*
* Function Name: ValidateArgument     
* Arguements: Option name and argument are input to this function.
* Return Value : Nothing..
* Description: This will check whether the argument contains -- or not.if the argument contains -- that mean no argument provided for option.
* Called by: parseCmdLineInput() after reading each argument of every option.
* Calls: 
* Issues Fixed:
*/

void ConfigValueParser::ValidateArgument(ACE_TCHAR argument[], ACE_TString option)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERING %s\n", __FUNCTION__);
    ACE_TString temp = argument;
    if(argument[0] == ACE_TEXT('-') && argument[1] == ACE_TEXT('-') || temp.empty() == true)
    {
        char tempString[1000];
		inm_sprintf_s(tempString, ARRAYSIZE(tempString), "\n InValid command line argument for the option %s", ACE_TEXT_ALWAYS_CHAR(option.c_str()));
        throw InvalidInput(tempString);
    }    
    DebugPrintf(SV_LOG_DEBUG, "EXITING %s\n", __FUNCTION__) ;
}

/*
* Function Name: varifyConfigDuplication     
* Arguements: No args
* Return Value : Nothing..
* Description: This will check any duplicate section name occure or not
* Called by: parseConfigFileInput() before performing the actual parsing..
* Calls: 
* Issues Fixed:
*/

void ConfigValueParser::varifyConfigDuplication()
{
   std::string sectionList[100];
   int sectionList_legnth = 0;
   fstream filestr (m_szconfigPath.c_str(), fstream::in );
   
   char * buffer;
   int buffer_length = 0;    
   filestr.seekg (0, ios::end);
   buffer_length = filestr.tellg();
   filestr.seekg (0, ios::beg);
 
   size_t buflen;
   INM_SAFE_ARITHMETIC(buflen = InmSafeInt<int>::Type(buffer_length) + 1, INMAGE_EX(buffer_length))
   buffer = new char [buflen];
   memset(buffer,'\0',	buffer_length + 1);	
  
   filestr.read (buffer,buffer_length );
 
   std::string configBuffer = buffer;
   
   std::string::size_type index1 = 0 ;
   std::string::size_type index2 = std::string::npos ;
   while( index2 != configBuffer.length() )
   {
        index1 = configBuffer.find('[', index1 );
        if(index1 == std::string::npos)
        {
            index2 = configBuffer.length() ;
            continue;
        }
        index2 = configBuffer.find(']', index1+1 );
        if( index2 == std::string::npos )
        {
            index2 = configBuffer.length() ;
            continue;
        }
        index1++;
        sectionList[sectionList_legnth] = configBuffer.substr(index1, index2 - index1 );
        trimSpaces( sectionList[sectionList_legnth++] ) ;
        index1 = index2 + 1 ;
    }
   
   //check duplicate strings in sectionList array
   for(int i = 0; i <= sectionList_legnth-1; i++)
   {
      for(int j = i+1; j <= sectionList_legnth-1; j++)
      {
          if(strcmp(sectionList[i].c_str(),sectionList[j].c_str()) == 0)
        {
            std::stringstream dup;
            dup << "\n Duplcate section   "
                <<"["<< sectionList[j]<<"]"
                <<"  found \n \n ICAT exiting ...";
            throw ParseException(dup.str());
        }
      }
   } 
   delete[] buffer;
   filestr.close();  
}

/*
* Function Name: icatUsage     
* Arguements: No args
* Return Value : Nothing..
* Description: This will call cmdLineUsage and configFileUsage
* Called by: parseInput() .
* Calls: 
* Issues Fixed:
*/

void ConfigValueParser::icatUsage()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERING %s\n", __FUNCTION__);
    std::cerr<<"\nICAT  version 1.0 "<<std::endl;
    std::cerr<<"\nCopyright (C) 2009 by InMage Systems."<<std::endl;
    std::cerr<<"\nInMage Content Archival Tool or ICAT for short provides seamless backup and archival solution by archiving content from multiple remote" 
    <<"offices simultaneously. "<<std::endl;
    std::cerr<<std::endl<<"GENERAL USAGE"<<std::endl;
    std::cerr<<"-------------"<<std::endl ;
    std::cerr<<std::endl<<"   ICAT runs in two modes. The normal mode and the resume mode. The \"--resume\" option is used to continue an older operation"                  <<"stopped."<<std::endl;
    configFileUsage();
    cmdLineUsage();
}
/*
* Function Name: cmdLineUsage     
* Arguements: No args
* Return Value : Nothing..
* Description: This will print the commandline usage of icat .
* Called by: icatUsage() .
* Calls: 
* Issues Fixed:
*/

void ConfigValueParser::cmdLineUsage()
{

    DebugPrintf(SV_LOG_DEBUG, "ENTERING %s\n", __FUNCTION__);
    std::cerr<<std::endl<<"COMMANDLINE USAGE \t"<<std::endl;
    std::cerr<<"-----------------"<<std::endl ;
    
    std::cerr<<"\nCommand Syntax"<<std::endl ;
    std::cerr<<"--------------\n\n"<<std::endl ;
    std::cerr<<"\ticat.exe {--nodes [ip:port]+ | --dnsname } [--transport] [--uid] [--gid] {--autogendestdirectory=1 --branchname --servername --sourcevolume | --autogendestdirectory=0 --targetdirectory | --targetdirectory | --branchname --servername --sourcevolume} { { --directory [--excludelist] [--include] --namepattern --comp --op=and/or --date --comp --op=and/or --size] | --filelisttotarget} | --filelistfromsource} + [--overwrite=1][--retryonfailure][--maxretries] [--maxfilelisters] [--retryinterval] [--exitonretryexpiry=1] [--logpath][--fromlastrun][--forcerun] [--sendtcpbuffer] [--recvtcpbuffer][--connectiontimeout][--lowspeedlimit][--lowspeedtime][--connects][--lifetime][--copies][--rootdir][--loglevel]" << std::endl ; 
        
    std::cerr<<"\n\nOptions"<<std::endl ;
    std::cerr<<"-------\n"<<std::endl ;
    std::cerr<<"\n archiverepository"<<std::endl;
    std::cerr<<"-------------------"<<std::endl ;
    std::cerr<<"\t--nodes=ip:port   \t Storage nodes information in ip:port format. Each node is separated by comma. Default Value for port is 80."<<std::endl;
    std::cerr<<"\t--dnsname \t\t DNS name for a group of storage nodes "<<std::endl;
    std::cerr<<"\t--rootdir \t\t root dir name to archive."<<std::endl;
    std::cerr<<"\n\t--transport \t\t Transport Protocol. Defaults to http"<<std::endl;
    std::cerr<<"\t--uid \t\t\t User Id with which files would be archived. Defaults to 0"<<std::endl;
    std::cerr<<"\t--gid \t\t\t Group Id with which files would be archived.Defaults to 0"<<std::endl;
    
    std::cerr<<"\n remoteoffice"<<std::endl;
    std::cerr<<"--------------"<<std::endl ;
    std::cerr<<"\t--autogendestdirectory \t When enabled, icat would generate target directory using branch name, server name, source volume name, date and time to archive."
             <<"\n\t\t\t\t When disabled and no target directory is specified, icat would archive to the files relative path to the root directory of archival storage."
             <<"\n\t\t\t\t When disabled and target directory is specified, icat uses target directory to archive the entities.In this case maxcopies option ignored,"
			 <<"\n\t\t\t\t Only when target directory name is same for every run.Defaults to  1 "<<std::endl;
    std::cerr<<"\t--branchname \t\t Remote office name  "<<std::endl;
    std::cerr<<"\t--servername \t\t Server name in the remote office"<<std::endl;
    std::cerr<<"\t--sourcevolume \t\t Source volume, which is protected at Remote Office"<<std::endl;
    std::cerr<<"\t--targetdirectory \t Indicates target directory in the archival repository."<<std::endl << std::endl;
    
    std::cerr<<"\n Source Details"<<std::endl;
    std::cerr<<"----------------"<<std::endl ;
    std::cerr<<"\t--directory \t\t Source Directory from which files would be archived"<<std::endl;
    std::cerr<<"\t--excludelist \t\t Comma separated list of directories in the source directory that can be excluded from archival. Although optional, this is mandatory with the --directory Option "<<std::endl;
    std::cerr<<"\t--namepattern \t\t comma separated list of pattrens to be matched. Matching operators supported are: =, !="<<std::endl;
    std::cerr<<"\t--comp \t\t\t supported comparison operators  "<<std::endl;
    std::cerr<<"\t--op \t\t\t logical operators. Values can be and/or, defaults to and"<<std::endl;
    std::cerr<<"\t--date \t\t\t modified time of the files would be used. should be in the format YYYY-MM-DD and =, >, <, >=, <= are allowed as comparison operators"<<std::endl;
    std::cerr<<"\t--size \t\t\t Size of the file. Should be in bytes and it allows =, >, <, >=, <= as comparison operators"<<std::endl;
    std::cerr<<"\t--include \t\t Include/exclude files that match above search criteria. Values can be true/false. Defaults to true"<<std::endl << std::endl;
    
    std::cerr<<"\n config Details"<<std::endl;
    std::cerr<<"----------------"<<std::endl ;
    std::cerr<<"\t--overwrite \t\t The value \"true\" overwrites existing files (by default) and the value \"false\" does not overwrite existing files."<<std::endl;
    std::cerr<<"\t--logpath \t\t ICAT log file path. Defaults to installation directory of VX or FX. If neither them available it would be stored in current running directory In case of windows and for other platforms the logs are stored under \"/var/log/\"."<<std::endl;
    std::cerr<<"\t--loglevel \t\t loglevel values allowed 0 to 7, Defaults to 3"<<std::endl;
    std::cerr<<"\t--forcerun \t\t Resume archival process even though resume information is not found. Defaults to 1."<<std::endl << std::endl ;
    
    std::cerr<<"\n tunables Details"<<std::endl;
    std::cerr<<"------------------"<<std::endl ;
    std::cerr<<"\t--retryonfailure \t retries the archival process of the entity or not, Defaults to true "<<std::endl;
    std::cerr<<"\t--maxretries \t\t  max number of retires to archive in failure case, Defaults to 1"<<std::endl;
    std::cerr<<"\t--retryinterval \t Indicates time to idle before next retry, Defaults to 0"<<std::endl;
    std::cerr<<"\t--exitonretryexpiry \t Indicates if we need icat to exit in case of archiving failed after maxretries.Defaults to 1"<<std::endl << std::endl ;
    std::cerr<<"\t--tcprecvbuffer \t TCP receive window size for optimized bandwidth utilization, defaults is 0"<<std::endl;
    std::cerr<<"\t--tcpsendbuffer \t TCP send window for optimized bandwidth utilization, Defaults to 0"<<std::endl;
    std::cerr<<"\t--maxconnects \t\t Maximum number of connections to Archival Repository. Defaults to 10"<<std::endl << std::endl;
    std::cerr<<"\t--connectiontimeout \t Connection time out in seconds. Defaults to 180  "<<std::endl; 
    std::cerr<<"\t--lowspeedlimit \t lowspeed limit. Defaults to 180  "<<std::endl;
    std::cerr<<"\t--lowspeedtime  \t lowspeed time. Defaults to 1  "<<std::endl;
    std::cerr<<"\t--maxfilelisters \t Maximum number of filelisters,should be less than or equals to maxconnects,Defaults 10 "<<std::endl; 
    
    std::cerr<<"\n Deletion Details"<<std::endl;
    std::cerr<<"------------------"<<std::endl ;
    std::cerr<<"\t--maxlifetime \t\t maximum life time of an object in the archival repository. Once the time expired, ICAT would remove all the entries.Defaults to 30."<<std::endl;
    std::cerr<<"\t--copies \t\t maximum copies of an object in the archival repository. Once the max copies reached, ICAT would remove all the entries.Defaults to 5"<<std::endl;
    std::cerr<<"\n filelist Details"<<std::endl;
    std::cerr<<"--------------------"<<std::endl ;
    std::cerr<<"\t--filelistfromsource \tThe filelist path from which files would be archived"<<std::endl;
    std::cerr<<"\t--filelisttotarget  \tThe filelist path to which files would be listed"<<std::endl;
        
    DebugPrintf(SV_LOG_DEBUG, "EXITING %s\n", __FUNCTION__) ;
}

/*
* Function Name: configFileUsage     
* Arguements: No args
* Return Value : Nothing..
* Description: This will print the commandline usage of icat .
* Called by: icatUsage() .
* Calls: 
* Issues Fixed:
*/

void ConfigValueParser::configFileUsage()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERING %s\n", __FUNCTION__);
    std::cerr<<std::endl<<"CONFIGURATION FILE USAGE"<<std::endl;
    std::cerr<<"------------------------"<<std::endl ;
    std::cerr<<"   You may pass on a config file to the icat executable containing remote office details, target directory path, IP address and DNS name.\n";
    std::cerr<<std::endl<<"   Options"<<std::endl;
    std::cerr<<"\n      --config=configuration file location"<<std::endl<<std::endl;
    std::cerr<<"The configuration file should contain"<<std::endl;
    std::cerr<<"\t1)complete remoteoffice details or targetdirectory path details or both "<<std::endl;
    std::cerr<<"\t2)either IP address details or DNS name"<<std::endl;
    std::cerr<<"\n\t\tMultiple IP's should be specified by comma delimeter"<<std::endl;
	std::cerr<<"\n\tICAT can archive files from content.sources or from filelist"<<std::endl;
    std::cerr<<"\t3) For giving multiple content.sources, section name should be content.source1 and content.source2 etc... "<<std::endl;
    std::cerr<<"\nTypical filefilter "<<std::endl;
    std::cerr<<"\n\tfilefilter=pattern=<file patterns>  and/or date=<file last modified date> and/or size=<size of the file> "<<std::endl;
    std::cerr<<"\t\tfor mutilple patterns values should be separeted by comma delimiter and, it allows = and != operators"<<std::endl;
    std::cerr<<"\t\tdate format should be YYYY-MM-DD and it allows =, >, <, >=, <= operators"<<std::endl;
    std::cerr<<"\t\tsize should be given in terms bytes and it allows =, >, <, >=, <= operators"<<std::endl;
    std::cerr<<""<<std::endl;
    std::cerr<<"NOTE: The pattern, date and size fields are optional, they may be in any order and should be seperated by a space or tab.."<<std::endl;
	std::cerr<<"\n\tFor giving filelist path configure filelistfromsource"<<std::endl;
	std::cerr<<"\n\tTo listout files configure filelisttotarget"<<std::endl; 
	std::cerr<<""<<std::endl;
    
    DebugPrintf(SV_LOG_DEBUG, "EXITING %s\n", __FUNCTION__) ;
}
