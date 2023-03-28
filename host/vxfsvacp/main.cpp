#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <uuid/uuid.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <sys/ioctl.h>
#include <iostream>
#include <vector>
#include <set>
#include <string>

//#include "devicefilter.h"

#include "StreamEngine.h"
#include "VacpUtil.h"
#include "inmsafeint.h"
#include "inmageex.h"

using namespace std;

const unsigned short UUID_STR_LEN = 36;

// TODO: all these should come from devicefilter.h
// currently, we are having some multiple definitions in devicefilter.h
// and user space streamengine header file
// so, for time being making copy of this
// need to resolve this asap
//
enum {
    STOP_FILTER_CMD = 0,
    START_FILTER_CMD,
    START_NOTIFY_CMD,
    SHUTDOWN_NOTIFY_CMD,
    GET_DB_CMD,
    COMMIT_DB_CMD,
    SET_VOL_FLAGS_CMD,
    GET_VOL_FLAGS_CMD,
    WAIT_FOR_DB_CMD,
    CLEAR_DIFFS_CMD,
    GET_TIME_CMD,
    UNSTACK_ALL_CMD,
    SYS_SHUTDOWN_NOTIFY_CMD,
    TAG_CMD,	
    WAKEUP_THREADS_CMD,
    GET_DB_THRESHOLD_CMD,	
    VOLUME_STACKING_CMD,
    RESYNC_START_CMD,
    RESYNC_END_CMD,
    GET_DRIVER_VER_CMD,
    GET_SHELL_LOG_CMD,
	AT_LUN_CREATE_CMD,
	AT_LUN_DELETE_CMD,
	AT_LUN_LAST_WRITE_VI_CMD,
	AT_LUN_QUERY_CMD,
	GET_GLOBAL_STATS_CMD,
	GET_VOLUME_STATS_CMD,
	GET_PROTECTED_VOLUME_LIST_CMD,
	GET_SET_ATTR_CMD,
    BOOTTIME_STACKING_CMD,
    VOLUME_UNSTACKING_CMD,
    START_FILTER_CMD_V2,
    SYNC_TAG_CMD,
    SYNC_TAG_STATUS_CMD
};

#define INMAGE_FILTER_DEVICE_NAME    "/dev/involflt"
#define TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP 0x0001
#define TAG_FS_CONSISTENCY_REQUIRED                   0x0002
#define TAG_FS_FROZEN_IN_USERSPACE		      0x0004

#ifdef sun
	#define INM_FLT_IOCTL       (0xfe)
	#define INM_IOCTL_CODE(x)   (INM_FLT_IOCTL << 8 | (x))
	#define IOCTL_INMAGE_TAG_VOLUME                 INM_IOCTL_CODE(TAG_CMD)
    #define IOCTL_INMAGE_SYNC_TAG_VOLUME            INM_IOCTL_CODE(SYNC_TAG_CMD)
    #define IOCTL_INMAGE_GET_TAG_VOLUME_STATUS      INM_IOCTL_CODE(SYNC_TAG_STATUS_CMD)
#elif defined linux
	#define FLT_IOCTL 0xfe
	#define IOCTL_INMAGE_TAG_VOLUME                 _IOWR(FLT_IOCTL, TAG_CMD, unsigned long)
    #define IOCTL_INMAGE_SYNC_TAG_VOLUME            _IOWR(FLT_IOCTL, SYNC_TAG_CMD, unsigned long)
    #define IOCTL_INMAGE_GET_TAG_VOLUME_STATUS      _IOWR(FLT_IOCTL, SYNC_TAG_STATUS_CMD, unsigned long)
#else

    #define INM_FLT_IOCTL           (0xfe)
    #define INM_IOCTL_CODE(x)       (INM_FLT_IOCTL << 8 | (x))
    #define IOCTL_INMAGE_TAG_VOLUME                 INM_IOCTL_CODE(TAG_CMD)
    #define IOCTL_INMAGE_SYNC_TAG_VOLUME            INM_IOCTL_CODE(SYNC_TAG_CMD)
    #define IOCTL_INMAGE_GET_TAG_VOLUME_STATUS      INM_IOCTL_CODE(SYNC_TAG_STATUS_CMD)
#endif

enum TagStatus
{
   STATUS_PENDING = 1, /* Tag is pending */
   STATUS_COMMITED,    /* Tag is commited by s2 */
   STATUS_DELETED, /* Tag is deleted due to stop filtering or clear diffs */
   STATUS_DROPPED, /* Tag is dropped due to write in bitmap file */
   STATUS_FAILURE, /* Some error occured while adding tag */
   
};

// Command line options
//
#define OPTION_VOLUME					"v"
#define OPTION_TAGNAME					"t"
#define OPTION_EXCLUDEAPP				"x"
#define OPTION_HELP					"h"
#define OPTION_REMOTE					"remote"
#define OPTION_SERVER_IP				"serverip"
#define OPTION_SERVER_PORT				"serverport"
#define OPTION_SERVERDEVICE				"serverdevice"
#define DEFAULT_SERVER_PORT				20003
#define OPTION_SYNC_TAG					"sync"
#define OPTION_TAG_TIMEOUT              "tagtimeout"

#define COMMAND_DMSETUP					"dmsetup"
#define OPTION_SUSPEND					"suspend"
#define OPTION_RESUME					"resume"
#define SEPARATOR_COMMA					","

bool GetFSTagSupportedolumes(std::set<std::string> & supportedFsVolumes);
void FreezeDevices(const set<string > &);
void FreezeVxFsDevices(const set<string > &);
void ThawDevices(const set<string > &);
void ThawVxFsDevices(const set<string > &);

bool ParseVolumeList(set<string>& sVolumes ,char* tokens, char* seps);
bool ParseVolume(set<string> & volumes, char *token);
string uuid_gen(void);

//Configurator* cxConfigurator = NULL;
int32_t vacpServSocket = -1;
struct sockaddr_in vacpServer;
bool gbRemoteConnection = false;
bool gbTagSend = false;
bool gbRemoteSend = false;

typedef struct TagInfo
{
	char * tagData;
	unsigned short tagLen;
} TagInfo_t;

typedef struct _VACP_SERVER_RESPONSE_ 
{
	int iResult; // return value of server success or failure
	int val;	// Success or Failure code
	char str[1024];  //string related to val		
}VACP_SERVER_RESPONSE;

bool operator <(const TagInfo_t & t1, const TagInfo_t & t2)
{
	if((t1.tagLen == t2.tagLen) && ( 0 == memcmp(t1.tagData, t2.tagData,t1.tagLen)))
		return false;

	return true;
}


void usage(char * myname)
{
	cout << "This tool can be used to issue consistancy bookmarks to vxfs volumes also, not available with standard distribution.\n";
	cout << "Usage:\n";	
	printf("%s { [-remote -serverdevice <device1,device2,..> -t <tag1,..> -serverip <serve IPAddress>]} [-serverport <ServerPort>] | [-x] | [-h]\n\n", myname);

#if defined(sun) | defined (linux) |defined (SV_AIX)

	printf("%s { [-v <vol1,..> |all ] | [-remote -serverdevice <device1,device2,..>] } [-t <tag1,..>] [-serverip <serve IPAddress>] [-serverport <ServerPort>] [-x] [-h] [-sync [-tagouttime <time out>] \n\n", myname);

	printf(" -v <vol1,vol2,..>\n");		
	printf("	 Specify volumes on which tag has to be generated.\n");
	printf("	 Specify \"all\" to generate tags on all volumes in the system\n");
	printf("	 Eg: /dev/sda1;/dev/sda2 etc\n\n");
	printf(" all      Issue tags to all protected volumes\n\n");
	printf(" -sync Issue synchronous tags, wait for <wait time> duration before returning.\n\n");
    printf(" -tagtimeout <time_wait> wait for time_out seconds, if the tag is not submitted within <time_out>. 0 to wait for ever.\n\n");
#endif


	printf(" -t <tagName1,tagNam2,..>\n");
	printf("	 Specifies one or more bookmark tags.\n");	
	printf("	 Eg: \"WeeklyBackup\" \"AfterQuartelyResults\" etc.\n\n");
	printf("	 NOTE: Tags cannot be issued to dismounted Volumes\n");
	printf("	 unless -x option is specified.\n\n");

	printf(" -x \t To generate tags w/o any consistency mechanism (Supported in future)\n");
	printf("    \t This option must be specified along with -v and -t options.\n\n");

	printf(" -remote");
	printf(" This switch is required for host operation.\n\n");

	printf(" -serverdevice <device1,device2,..>\n");
	printf("    \t Specifies server device names separated by comma on which tag is to be applied.\n\n");
	printf("    \t For fabric, serverdevice is LUN IDs.\n\n");

	printf(" -serverip <server IP Address>\n");
	printf("    \t Specifies server IP address where VACP Server is running.\n");
	printf("    \t Example -serverip 10.0.10.24\n\n");

	printf(" -serverport <Server Port>\n");
	printf("    \t Specifies vacp server port number that to be used by VACP client to connect to\n");
	printf("    \t vacp server. This is an optional parameter. \n");
	printf("    \t If not specified it will use 20003 port number.\n\n");

	printf(" -h \t Print the usage message.\n");
	printf("    \t This option should not be combined with any other option.\n\n");

	printf("Example:\n");

#if defined(sun) | defined (linux) |defined (SV_AIX)
	printf("Example for normal setup:\n");
	printf("vacp -v /dev/sda1,/dev/sda2 -t TAG1\n\n");
	printf("Example for client-server setup:\n");
	printf("vacp.exe -v VolGroup00-LogVol01,VolGroup00-LogVol00 -remote -serverdevice VG_XenStorage--d3cad621--52f6--3565--5cba--a225c249e5b9-LV--a7340a24--2776--4c8e--9434--88d1103801bf,VG_XenStorage--d3cad621--52f6--3565--5cba--a225c249e5b9-LV--a7340a24--2776--4c8e--9434--88d1103801ae -t TAG1 -serverip 10.0.214.10 -serverport 20003\n");	
#else
	printf("Example for client-server setup:\n");
	printf("vacp.exe -remote -serverdevice VG_XenStorage--d3cad621--52f6--3565--5cba--a225c249e5b9-LV--a7340a24--2776--4c8e--9434--88d1103801bf,VG_XenStorage--d3cad621--52f6--3565--5cba--a225c249e5b9-LV--a7340a24--2776--4c8e--9434--88d1103801ae -t TAG1 -serverip 10.0.214.10 -serverport 20003\n");	
#endif


	printf("\n NOTE: If no other option is specified, -h is a default option.\n\n");

}


//
// Function: GetAffectedVolumes
//   Given the list of input volumes, input applications, and FileSystem Consistency
//   generate the list of affected volumes.
//   Currently, we can issue filesystem consistency tag only if the
//   volume is mounted and the filesystem is a journaled filesystem
//
bool GetAffectedVolumes(set<string> & inputVolumes, /* set<string> & inputAppNames *,*/
						set<string> & affectedVolumes, bool FsTag)
{

	bool rv = true;
	char * token = NULL;
	string unsupportedVolumes;

	do 
	{
		if (!FsTag)
		{
			affectedVolumes.clear();
			affectedVolumes.insert(inputVolumes.begin(), inputVolumes.end());
			rv = true;
			break;
		}

		set<string> supportedFsVolumes;
		if(!GetFSTagSupportedolumes(supportedFsVolumes))
		{
			rv = false;
			break;
		}

		set<string>::const_iterator viter(inputVolumes.begin());
		set<string>::const_iterator vend(inputVolumes.end());
		while(viter != vend)
		{
			string devname = *viter;
			if(supportedFsVolumes.find(devname) == supportedFsVolumes.end())
			{                       
				unsupportedVolumes += devname;
				unsupportedVolumes += "\n";
				// driver will not ensure fs consistency for unmounted volumes
				affectedVolumes.insert(devname); 
			}
			else
			{
				affectedVolumes.insert(devname);
			}

			viter++;
		}

		if(!unsupportedVolumes.empty())
		{
			cout << "\nOnly Mounted Volumes are supported for FileSystem Consistency\n";
			cout << "Following volumes will be  excluded from file system consistency:\n";
			cout << unsupportedVolumes;
		}

	} while (0);

	return rv;
}

//
// Function: ParseTag
//  fill in the list of input tags
//

bool ParseTag(set<string> & tags, char *token)
{
	bool rv = true;
	tags.insert(string(token));
	return rv;
}

bool BuildAndAddStreamBasedTag(set<TagInfo_t> & finaltags, unsigned short tagId, char * inputTag, const std::string &sTagGuid)
{
	bool rv = true;

	do
	{
		unsigned long TotalTagLen = 0;
		unsigned long TagLen = 0;
		unsigned long TagHeaderLen = 0;
		TagLen = (unsigned long)strlen(inputTag) + 1;
		
		if (TagLen <= ((unsigned long)0xFF - sizeof(STREAM_REC_HDR_4B)))
		{
			TagHeaderLen = sizeof(STREAM_REC_HDR_4B);				
		}
		else
		{
			ASSERT(TagLen <= ((unsigned long)(-1) - sizeof(STREAM_REC_HDR_8B)));
			TagHeaderLen = sizeof(STREAM_REC_HDR_8B);
		}
		
		unsigned long TagGuidLen;
        //Append with 2 \0s to avoid alignment issues on Solaris sparc
        INM_SAFE_ARITHMETIC(TagGuidLen = InmSafeInt<unsigned long>::Type((unsigned long)sTagGuid.size()) + 2, INMAGE_EX(sTagGuid.size()))
        char strTagGuid[TagGuidLen];
        inm_memcpy_s(strTagGuid, sizeof(strTagGuid), sTagGuid.c_str(), sTagGuid.size() + 1); //Copy including '\0'
		unsigned long ulDataLength;
        INM_SAFE_ARITHMETIC(ulDataLength = InmSafeInt<unsigned long>::Type(TagGuidLen) + (unsigned long)strlen(inputTag) + 1, INMAGE_EX(TagGuidLen)(strlen(inputTag)))
		
		StreamEngine en(eStreamRoleGenerator);
		//StreamEngine *en = new StreamEngine(eStreamRoleGenerator);
		rv = en.GetDataBufferAllocationLength(ulDataLength,&TotalTagLen);		
		if(!rv)
		{
			("\nFILE = %s, LINE = %d, GetDataBufferAllocationLength() failed for Tag GUID\n",__FILE__,__LINE__);
			break;
		}
		//since there are two headers, we need to add header size again to the total length.
		//To add the header length again(for the second header), call this function GetDataBufferAllocationLength() again
        unsigned long dataandtagheaderlen;
        INM_SAFE_ARITHMETIC(dataandtagheaderlen = InmSafeInt<unsigned long>::Type(ulDataLength) + TagHeaderLen, INMAGE_EX(ulDataLength)(TagHeaderLen))
		rv = en.GetDataBufferAllocationLength(dataandtagheaderlen, &TotalTagLen);
		if (!rv)
		{
			printf("FILE = %s, LINE = %d, GetDataBufferAllocationLength() failed\n", __FILE__, __LINE__);
			break;
		}

		char *buffer = new char[TotalTagLen];		
		rv = en.RegisterDataSourceBuffer(buffer, TotalTagLen);		
		if (!rv)
		{
			printf("FILE = %s, LINE = %d, GetDataBufferAllocationLength() failed\n", __FILE__, __LINE__);
			break;
		}
		//Generate a Header to store the Tag Guid			
		printf("\nGenerating Header for TagGuid: %s \n",strTagGuid);
					
		rv = en.BuildStreamRecordHeader(STREAM_REC_TYPE_TAGGUID_TAG,TagGuidLen);

		if(!rv)
		{
			printf("\nFILE = %s, LINE = %d, BuildStreamRecordHeader() for TagGuid failed.\n",__FILE__,__LINE__);
			break;
		}			


		//Fill in the Tag Guid in the Data Part of the Stream
		rv = en.BuildStreamRecordData((void*)strTagGuid,TagGuidLen);		
		if(!rv)
		{
			printf("\nFILE = %s, LINE = %d, BuildStreamRecordData() for TagGuid failed,\n", __FILE__,__LINE__);
			break;
		}
		//Again set the stream state as waiting for Header as the actual Tag Data header should be filled now
		en.SetStreamState(eStreamWaitingForHeader);

		ulDataLength = (unsigned long)strlen(inputTag) + 1;

		if(!en.BuildStreamRecordHeader(tagId, ulDataLength))
		{
			printf( "Function %s @LINE %d in FILE %s : in BuildStreamRecordHeader\n", 
				FUNCTION_NAME, LINE_NO, FILE_NAME);
			rv = false;
			break;
		}

		if(!en.BuildStreamRecordData((void *)inputTag, ulDataLength))
		{
			printf( "Function %s @LINE %d in FILE %s : in BuildStreamRecordData\n", 
				FUNCTION_NAME, LINE_NO, FILE_NAME);
			rv = false;
			break;
		}

		TagInfo_t t;
		t.tagData = buffer;
		t.tagLen = (unsigned short) TotalTagLen;
		finaltags.insert(t);
		rv = true;

	} while (0);

	return rv;
}

//
// Function: GenerateTagNames
//  given the list of input user tags,applications and FS tag, generate
//  stream based tags
//
bool GenerateTagNames(set<TagInfo_t> & finaltags, set<string> & inputtags, 
					  /*set<string & inputApps, */  bool FSTag, std::string uuid)
{

	bool rv = true;
	printf( "Generating tag names ...\n");

	// generate a unique sequence based on time
	time_t tm;
	char timeStamp[15];
	(void)time(&tm);
	inm_sprintf_s(timeStamp, ARRAYSIZE(timeStamp), "%08x", (unsigned long)tm);

	do
	{
		if(FSTag)
		{
			string appName = "FS";
			const char * fsTagData = "FileSystem";

			unsigned short tagId;
			VacpUtil::AppNameToTagType(appName,tagId);

			char TagString[1024];
			inm_sprintf_s(TagString, ARRAYSIZE(TagString), "%s%s", fsTagData, timeStamp);
			printf( "Tag: %s\n", TagString);
			if(!BuildAndAddStreamBasedTag(finaltags,tagId,TagString, uuid))
			{
				rv = false;
				break;
			}
		}

		set<string>::const_iterator tagiter = inputtags.begin();
		set<string>::const_iterator tagend = inputtags.end();

		while(tagiter != tagend )
		{
			string appName = "USERDEFINED";
			string inputtag = *tagiter;

			unsigned short tagId;
			VacpUtil::AppNameToTagType(appName, tagId);

			char TagString[1024];
			inm_sprintf_s(TagString, ARRAYSIZE(TagString), "%s", inputtag.c_str());
			printf( "Tag: %s\n", TagString);
			if(!BuildAndAddStreamBasedTag(finaltags,tagId,TagString, uuid))
			{
				rv = false;
				break;
			}	
			++tagiter;
		}

		if(tagiter != tagend)
		{
			rv = false;
			break;
		}

	} while (0);

	return rv;
}


bool ConnToVacpServer (u_short port, const char* vacpServerIP)
{
	bool rv = true;

	// Create a socket for connecting to server
	vacpServSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (vacpServSocket < 0) 
	{
		printf("Error creating socket(): %d\n", errno);
		return false;
	}

	// The sockaddr_in structure specifies the address family,
	// IP address, and port of the server to be connected to.
	vacpServer.sin_family = AF_INET;
	vacpServer.sin_addr.s_addr = inet_addr( vacpServerIP );
	vacpServer.sin_port = htons(port);

	printf( "\nENTERED: ConnToVacpServer\n");

	do 
	{
		if( connect( vacpServSocket, (struct sockaddr *) &vacpServer, sizeof(vacpServer) ) < 0) 
		{
			printf("Failed to connect: %d\n", errno);						
			printf( "FAILED Couldn't able to connect to remote server: %s, port: %d\n", vacpServerIP, port );
			rv = false;
			break;
		}
		else 
		{
			gbRemoteConnection = true;
			printf( "Connected to %s at port %d\n", vacpServerIP, port);
		}
	}while(0);

	printf( "EXITED: ConnToVacpServer\n");

	return rv;
}

bool prepare(u_short vacpServPort, const string vacpServerIP)
{
	bool rv = true;


	//Check if connection established already
	if(!gbRemoteConnection)
	{		
		if( ConnToVacpServer( vacpServPort, vacpServerIP.c_str()) )
		{
			printf("Connected to Server...\n");			
		}
		else
		{
			printf("\nCannot continue...\n");
			rv = false;
		}
	}
	else
	{
		printf("\nConnection to Vacp Server already exists\n");
	}
	return rv;	
}
bool ParseVolumeList(set<string>& sVolumes ,char* tokens, char* seps)
{
	char* token;

	token = strtok(tokens,seps);
	while( token != NULL )
	{
		if( !ParseVolume(sVolumes, token) )
		{
			cout << "Invalid volume: " << token << endl;
			return false;
		}
		token = strtok( NULL, seps );
	}   
    return true;
}
bool ParseTagList(set<string>& sTags ,char* tokens, const char* seps)
{
	char* token;

	token = strtok(tokens,seps);
	while( token != NULL )
	{
		ParseTag(sTags, token);
		token = strtok( NULL, seps );
	}
}
bool ParseServerDeviceList(set<string>& sDevices, char* tokens, const char* seps)
{
	char* token;

	token = strtok(tokens,seps);
	while( token != NULL )
	{
		sDevices.insert(string(token));
		token = strtok( NULL, seps );
	}
}

int main(int argc, char ** argv)
{
	/*
	1. parse
	2. validate each volume
	3. generate tag identifiers
	4. issue tag
	5. print success/failure.

	*/

	bool rv = true;
	string vacpServer;
	u_short vacpServerPort = DEFAULT_SERVER_PORT;
	set<string> inputVolumes;
	set<string> inputTags;
	set<string> inputDevices;
	// set<string> inputAppNames;

	set<string> affectedVolumes;
	set<TagInfo_t> finalTags;
	bool FSTag = true;
	bool issue_syns_tags = false;

    int wait_time = 0;
	bool terminate = false;
	string volumes;

	// check usage
	if(argc == 1)
	{
		usage(argv[0]);
		return 1;
	}

	if(argc > 1 && argv[1] == "-h")
	{
		usage(argv[0]);
		return 0;
	}
	for (int i = 1; i < argc; i++)
	{
		if(argv[i][0] == '-') 
		{
#if defined(sun) | defined (linux) |defined (SV_AIX)
			if(strcasecmp((argv[i]+1),OPTION_VOLUME) == 0)
			{
				i++;
				if((i >= argc)||(argv[i][0] == '-'))
				{
					terminate = true;
					break;
				}
				volumes = argv[i];
				// No validation in case of remote tag generation
				// ParseServerDeviceList(inputVolumes, argv[i], SEPARATOR_COMMA);		
			}
			else
#endif	
		    if(strcasecmp((argv[i]+1),OPTION_TAGNAME) == 0)
			{
				i++;
				if((i >= argc)||(argv[i][0] == '-'))
				{
					terminate = true;
					break;
				}
				ParseTagList(inputTags, argv[i], SEPARATOR_COMMA);
			}
			else if(strcasecmp((argv[i]+1),OPTION_SERVERDEVICE) == 0)
			{
				i++;
				if((i >= argc)||(argv[i][0] == '-'))
				{
					terminate = true;
					break;
				}
				ParseServerDeviceList(inputDevices, argv[i], SEPARATOR_COMMA);
			}
			else if(strcasecmp((argv[i]+1),OPTION_REMOTE) == 0)
			{			
				gbRemoteSend = true;
			}			
			else if(strcasecmp((argv[i]+1),OPTION_SERVER_IP) == 0)
			{
				i++;
				if((i >= argc)||(argv[i][0] == '-'))
				{
					terminate = true;
					break;
				}			
				if(strlen(argv[i]) > 15)
				{	
					printf("Invalid Server IP passed");
					printf("Error in input");
				}
				vacpServer = argv[i];				
			}
			else if(strcasecmp((argv[i]+1),OPTION_SERVER_PORT) == 0)
			{
				i++;
				if((i >= argc)||(argv[i][0] == '-'))
				{
					terminate = true;
					break;
				}
				vacpServerPort = atoi(argv[i]);
			}
			else if(strcasecmp((argv[i]+1),OPTION_SYNC_TAG) == 0)
			{
                issue_syns_tags = true;
            }
            else if(strcasecmp((argv[i]+1), OPTION_TAG_TIMEOUT) == 0)
            {
				if(((i+1) < argc) && (argv[i+1][0] != '-'))
                {
				    wait_time = atoi(argv[i+1]);
                    i++;
                }
			}
			else if(strcasecmp((argv[i]+1),OPTION_EXCLUDEAPP) == 0)
			{
				FSTag = false;
			}
			else if(strcasecmp((argv[i]+1),OPTION_HELP) == 0)
			{
				if (argc != 2)
					printf("Error in input");
				else
				{
					terminate = true;
					break;
				}
			}
			else
			{
				cout << "Invalid option: " << argv[i]+1 << endl;
				printf("Error in input\n");
				terminate = true;
				break;
			}
		}
		else
		{
			terminate = true;
			break;
		}
	}

	if(terminate)
	{
		usage(argv[0]);
		rv = false;
	}

	//Valid Input...
	if(rv)
	{
		do {
#if defined(sun) | defined (linux) |defined (SV_AIX)
			if(!(rv = (ParseVolumeList(inputVolumes, (char*)volumes.c_str(), SEPARATOR_COMMA))))
			     break;
			// 0. Get the list of effected volumes
#endif
			if( !gbRemoteSend )
			{
				rv = GetAffectedVolumes(inputVolumes, /*inputAppNames, */ 
					affectedVolumes, FSTag);
				if(!rv)
					break;
			}
			else
			{				
				affectedVolumes = inputDevices;
			}
			if(!gbRemoteSend && affectedVolumes.size() < 1)
			{
				cout << "Please specify atleast one volume/device \n\n";
				rv = false;
				break;
			}			

            std::string uuid=uuid_gen();
			// 1. Go through each tag and generate a stream based tag
			//    If FStag is required, generate a stream based fstag as well
			rv = GenerateTagNames(finalTags, inputTags, /*inputAppNames, */ FSTag, uuid);
			if(!rv)
				break;

			if(finalTags.size() < 1)
			{
				cout << "Please specify atleast one tag\n\n";
				rv = false;
				break;
			}

			// 3. create ioctl buffer
			// in following form:
			//	Flags + TotalNumberOfVolumes + 
			// first DeviceName Length + First Device Name 
			// second DeviceName Length + Second Device Name
			// ...
			// + TotalNumberOfTags 
			// + TagLenOfFirstTag + FirstTagData 
			// + TagLenofSecondTag + SecondTagData ...

			unsigned int Flags = 0;		
			unsigned short numTags = finalTags.size();
			unsigned short numVolumes;

			numVolumes = affectedVolumes.size();
			// by default, we want to issue tags atomically across volumes
			Flags |= TAG_VOLUME_INPUT_FLAGS_ATOMIC_TO_VOLUME_GROUP;

			//For sun we are freezing FS from userspace
#ifdef sun
			Flags |= TAG_FS_FROZEN_IN_USERSPACE;
#endif

			if(FSTag)
				Flags |= TAG_FS_CONSISTENCY_REQUIRED;

			uint32_t current_buf_size = 0; // size of buffer currently filled
            uint32_t  status_offset = 0; //offset of status field in buffer, used if tags issues in sync mode
			uint32_t total_buffer_size = sizeof(Flags) + 
                                        sizeof(numVolumes) +
                                        ( numVolumes * sizeof(unsigned short))
                                        /* + size of each volume */ 
                                        + sizeof(numTags)
                                        + ( numTags * sizeof(unsigned short))
                                        /* + size of each tag */;


			set<string>::const_iterator viter = affectedVolumes.begin();
			set<string>::const_iterator vend = affectedVolumes.end();

			while(viter != vend)
			{
				string devname = *viter;
				total_buffer_size += strlen(devname.c_str());
				++viter;
			}

			set<TagInfo_t>::const_iterator titer = finalTags.begin();
			set<TagInfo_t>::const_iterator tend = finalTags.end();

			while(titer != tend)
			{
				total_buffer_size += (*titer).tagLen;
				++titer;
			}

            if (issue_syns_tags)
            {
			    total_buffer_size += sizeof (unsigned short) + 
                                        UUID_STR_LEN ; // To add uuid
                status_offset  = total_buffer_size;
                cout<<"Status offset index is "<<status_offset <<"\n";
                total_buffer_size += (sizeof (int) * numVolumes) /*return status for each volume*/;
            }

			char * buffer = static_cast<char*>(calloc(total_buffer_size,1));
			if(!buffer)
			{
				printf( "Function %s @LINE %d in FILE %s :insufficient memory\n",
					FUNCTION_NAME, LINE_NO, FILE_NAME);
				rv = false;
				break;
			}

			printf( "\nSending Following Tag Request ...\n");

            if (issue_syns_tags)
            {
                inm_memcpy_s(buffer + current_buf_size, total_buffer_size - current_buf_size, &UUID_STR_LEN, sizeof (UUID_STR_LEN));
                current_buf_size += sizeof(UUID_STR_LEN);

                uuid = uuid_gen();

                inm_memcpy_s(buffer + current_buf_size, total_buffer_size - current_buf_size, uuid.c_str(), UUID_STR_LEN);
                current_buf_size += UUID_STR_LEN;
            }

			inm_memcpy_s(buffer + current_buf_size, total_buffer_size - current_buf_size, &Flags, sizeof (Flags));
			current_buf_size += sizeof(Flags);
			printf( "Flags = %d\n", Flags);

			inm_memcpy_s(buffer + current_buf_size , total_buffer_size - current_buf_size, &numVolumes, sizeof (numVolumes));
			current_buf_size += sizeof(numVolumes);
			printf( "Num. Volumes = %d\n", numVolumes);

			unsigned short curvol = 1;
			viter = affectedVolumes.begin();
			while(viter != vend)
			{
				string devname = *viter;
				unsigned short len = strlen(devname.c_str());
				inm_memcpy_s(buffer + current_buf_size ,total_buffer_size - current_buf_size, &len, sizeof(unsigned short)); ;
				current_buf_size += sizeof(len);
				inm_memcpy_s(buffer + current_buf_size, total_buffer_size - current_buf_size, devname.c_str(), len);
				current_buf_size += len;
				printf( "Volume: %d  Name: %s Length:%d\n", curvol, devname.c_str(),
					strlen(devname.c_str()));

				++viter;
				++curvol;
			}

			inm_memcpy_s(buffer + current_buf_size, total_buffer_size - current_buf_size, &numTags, sizeof(unsigned short));;
			current_buf_size += sizeof(numTags);
			printf( "Num. Tags = %d\n", numTags);

			unsigned short curtag = 1;
			titer = finalTags.begin();
			while(titer != tend)
			{
				inm_memcpy_s(buffer + current_buf_size, total_buffer_size - current_buf_size, &(*titer).tagLen, sizeof(unsigned short)) ;
				current_buf_size += sizeof(unsigned short);
				inm_memcpy_s(buffer + current_buf_size, total_buffer_size - current_buf_size, (*titer).tagData, (*titer).tagLen);
				current_buf_size += (*titer).tagLen;
				printf( "Tag: %d Length:%d\n", curtag, (*titer).tagLen);

				++titer;
				++curtag;
			}

			//For linux we do not want to freeze the volume if the tag is issued for locally.
			// We need it if we issue tags to remote server for linux
#if !defined (linux)
			FreezeDevices(inputVolumes);
#endif
			if( gbRemoteSend )
			{
              #if defined (sun)
			    Flags |= TAG_FS_FROZEN_IN_USERSPACE; // in case remote tag, volumes were always frozen from userspace
              #endif
				// create an ACE process to run the generated script
				int result = 0;
				set<string>::const_iterator deviceIter;
				//Connect to server
				std::cout<<"Connecting to Server.\n";
				if( !prepare(vacpServerPort, vacpServer) )
				{
					//Exit
					rv = false;
					break;
				}

				//Issue packets to remote server
				bool bResult = false;
				uint32_t uiReturn = 0;
				VACP_SERVER_RESPONSE tVacpServerResponse;

				//Send tags to remote vacp server
				printf("Sending tags to the remote server ...\n");
				printf("Bytes passed to RemoteServer = %d \n", current_buf_size);

				uint32_t tmplongdata = htonl(total_buffer_size);
				do
				{
					unsigned short us = 1;
					char arch = *((char *)&us);//will yeild 1 on little endians and 0 on big endians.

					const char *svdarch = (arch)?"SVDL":"SVDB";
					printf ("Arch is %s\n", svdarch);

					//Send Arch  to server
					if(send(vacpServSocket, svdarch, 4 , 0) < 0)
					{
						printf ( "FAILED Couldn't send total arch data to remote server");
						rv = false;
						break;
					}				

				// We need it if we issue tags to remote server for linux
#if defined (linux)
					FreezeDevices(inputVolumes);
#endif
					//Send request buffer to server
					if(send(vacpServSocket, (char *)&tmplongdata, sizeof tmplongdata, 0) < 0)
					{
						printf ( "FAILED Couldn't send total Tag data length to remote server");
						rv = false;
						break;
					}				
					else if(send(vacpServSocket, (char*)buffer, current_buf_size, 0) < 0)
					{
						printf ( "FAILED Couldn't send Tag data to remote server");
						rv = false;
						break;
					}
					//Receive response buffer from server
					else if(recv(vacpServSocket,(char*)&tVacpServerResponse, sizeof tVacpServerResponse,0) <= 0)
					{
						printf ( "FAILED Couldn't get acknowledgement from remote server");
						rv = false;
						break;
					}
#if defined (linux)
					ThawDevices(inputVolumes);
#endif

					//Validate response
					tVacpServerResponse.iResult  = ntohl(tVacpServerResponse.iResult);
					if(tVacpServerResponse.iResult > 0)
					{
						bResult = true;
					}
					if(buffer)
					{
						free(buffer);
					}					

					tVacpServerResponse.val = ntohl(tVacpServerResponse.val);
					tVacpServerResponse.str[tVacpServerResponse.val] = '\0';

					printf ("INFO: VacpServerResponse:[%d] %s\n", tVacpServerResponse.iResult, tVacpServerResponse.str);

				} while(0);

				if(bResult)
				{
					gbTagSend = true;
					//printf( "Successfully sent tags to following volumes,\n");					
					printf( "Successfully sent tags to the Remote Server ...\n");
					rv = true;
				}
				else
				{
					printf( "FAILED to Send Tag to the Remote Server for following Server devices\n");
					printf("Error code:%d", errno);					
					printf("\n");
					rv = false;
				}


			}
			else
			{
				int m_Driver = open(INMAGE_FILTER_DEVICE_NAME, O_RDWR );

				if (-1 == m_Driver) 
                {
					printf( "Function %s @LINE %d in FILE %s :unable to open %s\n",
						FUNCTION_NAME, LINE_NO, FILE_NAME, INMAGE_FILTER_DEVICE_NAME);
					rv = false;
					break;
				}
				#if defined (linux)
					FreezeVxFsDevices(inputVolumes);
				#endif    
                if (issue_syns_tags)
                {
                    int retVal;
                    if((retVal = ioctl(m_Driver, IOCTL_INMAGE_SYNC_TAG_VOLUME, buffer)) < 0)
                    {
                        if (retVal == EINVAL)
                        {
                            printf( "ioctl failed: synchronous tags might not be supported. Look for supporting driver.\n");
                            rv = false;
                            close(m_Driver);
							#if defined (linux)
								ThawVxFsDevices(inputVolumes);
							#endif
                            break;
                        }

                        printf( "ioctl failed: tags could not be issued. errno %d\n", errno);
                        rv = false;
                        close(m_Driver);
						#if defined (linux)
							ThawVxFsDevices(inputVolumes);
						#endif
                        break;
                    }
                    else
                    {
                        cout << "\nTags successfully issued.\n";

                        /*check for tag status for all volumes */
                        bool anyTagsPending = false;
                        cout<<"Status offset index is "<<status_offset <<"\n";

                        int *status = (int *) (buffer + status_offset);

                        for (int i = 0; i < numVolumes; i++)
                        {
                            cout<<"Tag status is " << status[i] <<"\n";
                            if ( status[i] == STATUS_PENDING)
                            {
                                anyTagsPending = true;
                            }   
                         }

                        if (!anyTagsPending)
                        {
                            close (m_Driver);
                            cout<<"All tags have been processed succesfully.\n";
							#if defined (linux)
								ThawVxFsDevices(inputVolumes);
							#endif
                            break;
                        }

                        if (wait_time)
                            cout << "\n Checking the status of tags after waiting for "<<wait_time << " seconds ...\n";

                        unsigned short total_status_buffer_size =  sizeof (unsigned short) // length of the guid
                                                                    + UUID_STR_LEN // guid
                                                                    + sizeof (unsigned short); // wait time
                        status_offset = total_status_buffer_size; //remember where status starts

                        total_status_buffer_size += sizeof (int) * numVolumes; //room to store status

                        char * stat_buffer = static_cast<char*>(calloc(total_status_buffer_size,1));
                        if(!stat_buffer)
                        {
                            printf( "Function %s @LINE %d in FILE %s :insufficient memory\n",
                                FUNCTION_NAME, LINE_NO, FILE_NAME);
                            rv = false;
                            close(m_Driver);
							#if defined (linux)
								ThawVxFsDevices(inputVolumes);
							#endif
                            break;
                        }

                        current_buf_size = 0;

                        inm_memcpy_s(stat_buffer + current_buf_size, total_status_buffer_size - current_buf_size, &UUID_STR_LEN, sizeof (UUID_STR_LEN));
                        current_buf_size += sizeof(UUID_STR_LEN);

                        inm_memcpy_s(stat_buffer + current_buf_size, total_status_buffer_size - current_buf_size, uuid.c_str(), uuid.size());
                        current_buf_size += UUID_STR_LEN;

                        inm_memcpy_s(stat_buffer + current_buf_size, total_status_buffer_size - current_buf_size, &wait_time, sizeof (wait_time));
                        current_buf_size += sizeof(wait_time);

                        status = (int *) (stat_buffer + status_offset); //reset status to start of status in buffer
                        do
                        {

                            if(ioctl(m_Driver, IOCTL_INMAGE_GET_TAG_VOLUME_STATUS, stat_buffer) < 0)
                            {
                                printf( "Function %s @LINE %d in FILE %s :IOCTL_INMAGE_GET_TAG_VOLUME_STATUS  returned error\n",
                                        FUNCTION_NAME, LINE_NO, FILE_NAME);
                                rv = false;
                                close(m_Driver);
								#if defined (linux)
									ThawVxFsDevices(inputVolumes);
								#endif
                                break;
                            }

                            /*check for tag status for all volumes */
                            anyTagsPending = false;
                            for (int i = 0; i < numVolumes; i++)
                            {
                                cout<<"Tag status is " << status[i] <<"\n";
                                if (status[i] == STATUS_PENDING)
                                {
                                    anyTagsPending = true;
                                    set<string>::iterator it = affectedVolumes.begin() ;
                                    for(int j=i;j>0;--j)
                                        ++it;
                                    printf ("Function %s : Tag for %s is still pending\n", FUNCTION_NAME, (*it).c_str());
                                }
                            }

                            if(!anyTagsPending)
                                printf ("All Tags processed.\n");
                            else
                                printf ("Warning : Tags were not processed within the time out period.\n");

                            close(m_Driver);
							#if defined (linux)
								ThawVxFsDevices(inputVolumes);
							#endif
                            break;
                        }while (0);//end of while loop for synch tag status processing
                    }
                }
                else
                {
                    if(ioctl(m_Driver, IOCTL_INMAGE_TAG_VOLUME, buffer) < 0)
                    {
                        printf( "ioctl failed: tags could not be issued. errno %d\n", errno);
                        rv = false;
                        close(m_Driver);
						#if defined (linux)
							ThawVxFsDevices(inputVolumes);
						#endif
                        break;
                    }
                    else
                    {
                        cout << "\nTags successfully issued.\n";
                        rv = true;
                        close(m_Driver);
						#if defined (linux)
							ThawVxFsDevices(inputVolumes);
						#endif
						break;
                    }
                }
            }
        } while (0);
#if !defined (linux)
        ThawDevices(inputVolumes);
#endif
	}
	if(rv) 
		return 0;
	else
		return 1;
}

string uuid_gen ()
{
    uuid_t id;
    char key[UUID_STR_LEN + 1] = "";
    uuid_generate(id);
    uuid_unparse(id, key);
    return key;
}
