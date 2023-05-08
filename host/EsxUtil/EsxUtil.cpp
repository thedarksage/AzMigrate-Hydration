// ESXUTIL.cpp : Defines the entry point for the console application.
//

#include <ace/Init_ACE.h>

#include "LinuxReplication.h"
#include "RollBackVM.h"
#include "ChangeVmConfig.h"
#include "rpcconfigurator.h"
#include <ace/OS_main.h> 
#include "logger.h"
#include "cdputil.h"
#include "inmageex.h"
#include "configwrapper.h"
#include "inmsafecapis.h"
#include "terminateonheapcorruption.h"

#include "version.h"

#ifdef WIN32
#define SECURITY_WIN32		1
#include "boostsectfix.h"
#include <Security.h> // for getusernameex api
#endif

class CopyrightNotice
{ 
public:
    CopyrightNotice()
    {
        std::cout <<"\n"<< INMAGE_COPY_RIGHT <<"\n\n";
    }
};

Configurator* TheConfigurator = 0;
bool bConfigStatus = false;
bool InitializeConfigurator();

#define OPTION_REPLICATION			"-replication"
#define OPTION_ROLLBACK				"-rollback"
#define OPTION_NW_SETUP             "-nwsetup"
#define OPTION_AT_BOOTING           "-atbooting"
#define OPTION_FILENAME	            "-file"
#define OPTION_BOOTSECTOR_FIX		"-bootsectorfix"
#define OPTION_BOOTSECTOR_INFO		"-bootsectorinfo"
#define OPTION_HELP					"-help"
#define OPTION_EXTRACHECKS          "-extrasafetychecks"
#define OPTION_FETCHDISKMAP         "-fetchdiskmap"
#define OPTION_VMWARETOOLS			"-installVmWareTools"
#define OPTION_NETWORK				"-nw"
#define OPTION_PHYSICAL_SNAPSHOT	"-physnapshot"
#define OPTION_DR_DRILL             "-drdrill"
#define OPTION_P2V					"-p2v"
#define OPTION_LOG_DIRECTOY		    "-d"
#define OPTION_CX_PATH				"-cxpath"
#define OPTION_CX_PLANID			"-planid"
#define OPTION_APPLY_TAG			"-applytag"
#define OPTION_TGTHOST_NAME         "-t"
#define OPTION_TAG_NAME             "-tag"
#define OPTION_CRASHCONSISTENCY     "-crashconsistency"
#define OPTION_IMPORT_DISK          "-import"
#define OPTION_OPERATION			"-op"
#define OPTION_DLMMBR				"-dlmmbr"
#define OPTION_DLM_PARTITION        "-dlmpartition"
#define OPTION_READ                 "-read"
#define OPTION_DLM_OPN				"-opn"
#define OPTION_DLM_REG_DELETE		"-deldlmreg"
#define OPTION_INSTALL_DRIVER		"-installw2k3driver"
#define OPTION_SYSTEM_VOLUME		"-sysvol"
#define OPTION_OS_TYPE				"-ostype"
#define OPTION_REMOVE_PAIR			"-removepair"
#define OPTION_CLOUD_ENV			"-cloudenv"
#define OPTION_CONFIG_FILE			"-configfile"
#define OPTION_RESULT_FILE			"-resultfile"
#define OPTION_SKIP_CDPCLI_SNAPSHOT "-skipcdpclisnapshot"
#define OPTION_SKIP_CDPCLI_ROLLBACK "-skipcdpclirollback"
#define OPTION_DOWNLOAD				"-download"
#define OPTION_UPDATE				"-update"
#define OPTION_DISK					"-disk"
#define OPTION_NUMOFBYTES			"-numofbytes"
#define OPTION_SKIP_BYTES			"-skip"
#define OPTION_LAST					"-last"
#define OPTION_DLM_FILE             "-dlmfile"
#define OPTION_HOST_GUID            "-host"
#define OPTION_GETBACK_MOUNTPOINT   "-restoremountpoints"
#define OPTION_ONLY_DISK_LAYOUT     "-onlydisklayout"
#define OPTION_GENERATE_SIGNATURE   "-generatesig"
#define OPTION_CXAPI_FUNC_NAME      "-cxapifuncname"
#define OPTION_REQUEST_METHOD       "-requestmethod"
#define OPTION_CXAPI_ACCESSFILE     "-cxapiaccessfile"
#define OPTION_REQUESTID            "-requestid"
#define OPTION_VERSION              "-version"
#define OPTION_RESETGUID			"-resetguid"
#define OPTION_NEW_HOST_ID			"-newhostid"
#define OPTION_TEST_FAILOVER		"-testfailover"
#define OPTION_ENABLE_RDP_FIREWALL	"-enablerdpfirewall"

bool isComma(char c) {return (c == ',' ? 1:0);}
bool not_comma(char c) { return (c == ','? 0:1);}

using namespace std;


bool InitializeConfigurator()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);


    bool rv = false;

    do
    {
        try 
        {
            if(!InitializeConfigurator(&TheConfigurator, USE_ONLY_CACHE_SETTINGS, PHPSerialize))
            {
                rv = false;
                break;
            }

			bConfigStatus = true;
            rv = true;
        }

        catch ( ContextualException& ce )
        {
            rv = false;
            //cout << " encountered exception. " << ce.what() << endl;
            DebugPrintf(SV_LOG_ERROR,"\n encountered exception. %s \n",ce.what());
        }
        catch( exception const& e )
        {
            rv = false;
           //cout << " encountered exception. " << e.what() << endl;
            DebugPrintf(SV_LOG_ERROR,"\n encountered exception. %s \n",e.what());
        }
        catch ( ... )
        {
            rv = false;
            //cout << FUNCTION_NAME << " encountered unknown exception." << endl;
			//cout << "encountered unknown exception." << endl;
            DebugPrintf(SV_LOG_ERROR,"\n encountered unknown exception. \n");
        }

    } while(0);

    if(!rv)
    {
       // cout << "[WARNING]: CX server is unreachable. \nPlease verify if CX server is running..." << endl;
        DebugPrintf(SV_LOG_INFO,"[WARNING]: Configurator initialization failed. Verify if Vx agent service is running...\n");
		bConfigStatus = false;
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);

    return rv;
}

#ifdef WIN32
// to get current process Id and command line arguments which we have passed and domain name\username  
void GetCmdLineargsandProcessId(void)
{
    ULONG ulProcessId = 0;
    // To print the current command & arguments     
    cout <<"\nCommand Line: "<< GetCommandLine() << endl;

    //The user's security access manager (SAM) name in the format "DOMAIN\USER".

    LPVOID szUserName;
    DWORD  dwNameLen;
    BOOLEAN bRetVal;

    dwNameLen = MAX_PATH;
    szUserName = LocalAlloc(LPTR, dwNameLen);

    bRetVal = true;

    bRetVal = GetUserNameEx(NameDnsDomain,(LPTSTR)szUserName,&dwNameLen);

    if( ERROR_MORE_DATA == bRetVal )
    {
        LocalFree(szUserName);
        szUserName = LocalAlloc(LPTR, dwNameLen);
        bRetVal = GetUserNameEx(NameDnsDomain, (LPTSTR)szUserName, &dwNameLen);		 
    }

    if( bRetVal )
    {
        cout <<"Running under the user: "<< (LPTSTR)szUserName << endl;
        LocalFree(szUserName);
    }
    else
    { 
        //cout << "[ERROR][" << GetLastError() << "] GetUserNameEx failed.\n";
        bRetVal = GetUserName((LPTSTR)szUserName, &dwNameLen);
        if( bRetVal )
        {
            cout <<"Running under the user: "<< (LPTSTR)szUserName << endl;
            LocalFree(szUserName);
        } 
        else
        {
            cout << "[ERROR][" << GetLastError() << "] GetUserName failed.\n";
        }

    }

    int result = 0;
    char MachineName[512];
    result = gethostname(MachineName,sizeof(MachineName));
    if ( result != 0)
    {
        cout << "ERROR: Could not get the host name of this machine. Error Code = " << result << std::endl;
        // return SVE_FAIL;
    }
    else
        cout<<"Local Machine Name is : "<<MachineName<<endl;


    //Get the current process ID    
    ulProcessId = GetCurrentProcessId();
    cout<< "Process ID: "<< ulProcessId <<endl<<endl;
}
#endif

void Usage()
{    
#ifndef WIN32
    cout<<"\n\n To setup Linux Protection \n";
	cout<<"\n\t EsxUtil -replication linux [-d <log directory path> -cxpath <cx directory> -planid <CX Plan id> -op <operation>]\n";

	cout<<"\n\n To get the network file details \n";
	cout<<"\n\t EsxUtil -nw linux \n";

    cout<<"\n\n To perform Rollback \n";
	cout<<"\n\t EsxUtil -rollback [-d <Directory path> -cxpath <cx directory> -planid <CX Plan id> -op <operation> [-skipcdpclirollback]]\n";
	cout<<"\n\t     Note: skipcdpcli option when confident that rollback completed for all hosts.. Use it with help of dev team\n";

	cout<<"\n\n To get the required files for physical snapshot \n";
	cout<<"\n\t EsxUtil -drdrill [-d <Directory path> -cxpath <cx directory> -planid <CX Plan id> -op <operation>]\n";
	
	cout<<"\n\n To generate a consistency point on source machine \n";
	cout<<"\n\t EsxUtil -applytag -t <Source Corressponding MT Hostname> -tag <Tag Name> [-crashconsistency]\n";

	cout<<"\n\n To generate a consistency point on source physical machine \n";
	cout<<"\n\t EsxUtil -applytag -t <Source Corressponding MT Hostname> -tag <Tag Name> [-crashconsistency] -p2v\n";

#else
    cout<<"\n\n To perform Rollback \n";
	cout<<"\n\t EsxUtil -rollback [-d <Directory Path> -extrasafetychecks -fetchdiskmap -cxpath <cx directory> -planid <CX Plan id> -op <operation> [-skipcdpclirollback]]\n";
    cout<<"\n\t    Note: extrasafetychecks and fetchdiskmap are optional\n";
	cout<<"\n\t          skipcdpcli option when confident that rollback completed for all hosts.. Use it with help of dev team\n";

    cout<<"\n\n To configure settings when VM boots and performs netowrk related changes.\n";
	cout<<"\n\t EsxUtil -atbooting -file <File Path> -p2v\n";
    cout<<"\n\t    Note: This will be run before logon.\n";
	cout<<"\n\t\t	       [-p2v] option is incase of physcial machien recovery\n";

	cout<<"\n\n To configure settings when VM boots and performs changes related to cloud environment\n";
	cout<<"\n\t EsxUtil -atbooting -cloudenv <Cloud environment[azure/vcloud/amazoncloud/googlecloud]>\n";
    cout<<"\n\t    Note: This will be run before logon.\n";

	cout<<"\n\n To Fix \"Boot Parameter Block\" of the boot sector of a NTFS volume\n";
	cout<<"\n\t EsxUtil -bootsectorfix  <volume>\n";
	
    cout<<"\n\n To Display \"Boot Parameter Block\" of the boot sector of a NTFS volume\n";
	cout<<"\n\t EsxUtil -bootsectorinfo  <volume>\n";


	/********** DEBUGGING COMMANDS FOR INTERNAL USE ***********/

	//Sets network by taking inputs from file nw.txt
	cout<<"\n\n To configure Network settings on windows\n";
	cout<<"\n\t EsxUtil -nwsetup -file <File Path>\n";	

	//Installs Vmware tools
	cout<<"\n\n To install VMWare tools after recovery of VM in case of P2V\n";
	cout<<"\n\t EsxUtil -installVmWareTools\n";
    cout<<"\n\t    Note: The VM Ware tools has to be attached to the CD-Rom drive\n";

	//Imports if foreign disks found
	cout<<"\n\n To Import all the foreign disks in a windows system\n";
	cout<<"\n\t EsxUtil -import\n";

	//Creats DLM MBR file
	cout<<"\n\n To create DLM MBR file\n";
	cout<<"\n\t EsxUtil -dlmmbr -file <file name with complete path> -opn <dlm_normal/dlm_mbronly/dlm_all>\n";

	//Reads the specified DLM MBR file
	cout<<"\n\n To read DLM MBR file\n";
	cout<<"\n\t EsxUtil -dlmmbr -read -file <file name with complete path>\n";

	//Reads DLM parititon file
	cout<<"\n\n To read DLM Partition file\n";
	cout<<"\n\t EsxUtil -dlmpartition -read -file <file name with complete path>\n";

	//Delete DLM reg entries 
	cout<<"\n\n Delete DLM registry Entries\n";
	cout<<"\n\t EsxUtil -deldlmreg\n";

	//Install w2k3 drivers
	cout<<"\n\n Install W2K3 Driver(symmpi.sys)\n";
	cout<<"\n\t EsxUtil -installw2k3driver -sysvol <Systemvolume> -ostype <64/32>\n";

	//Download data from a disk
	cout<<"\n\n Download from a disk and store on a file : \n";
	cout<<"\n\t EsxUtil -download -disk <disk number> -numofbytes <number bytes to download> {[-file \"<filename with complete path>\"] [-skip <bytes to skip>]/[-last]}\n";
	cout<<"\t -download     --> Indicates to download data from disk\n";
	cout<<"\t -disk         --> Disk from which data need to download\n";
	cout<<"\t -numofbytes   --> Number of bytes to download from the disk\n";
	cout<<"\t -skip         --> [Optional] Skips certain location of the disk and downloads from there\n";
	cout<<"\t -last         --> [Optional] Downloads the specified amount of bytes from end of disk\n";
	cout<<"\t -file         --> [Optional] File name which is going to store the collected binary data. If not selected then by default stores at <Agent installation path>\\Failover\\Data\\ \n";

	//Update data on a disk
	cout<<"\n\n Update binary data of a file onto disk : \n";
	cout<<"\n\t EsxUtil -update -disk <disk number> -numofbytes <number bytes to update> -file \"<filename with complete path>\" {[-skip <bytes to skip>]/[-last]}\n";
	cout<<"\t -update       --> Indicates to update data onto a disk\n";
	cout<<"\t -disk         --> Disk number on which data will update.\n";
	cout<<"\t -numofbytes   --> Number of bytes to update on the disk\n";
	cout<<"\t -file         --> File name which contains the data which need to dump on the specified disk.\n";
	cout<<"\t -skip         --> [Optional] Skips certain location of the disk and updates from there\n";
	cout<<"\t -last         --> [Optional] Updates the specified number of bytes at end of disk\n";

	//Follwing command will prepare the target disks for protection by creating the volume under it
	cout<<"\n\n Following tasks will be done by the command :\n";
	cout<<"\t 1. Extracts the MBR file and prepares the target disks for updating disk layout information.\n";
	cout<<"\t 2. Dumps the extracted disk information on target disks and prepare the volumes for protection by assigning mount points\n";
	cout<<"\n\t EsxUtil -update -file \"<filename with complete path which has disk mappings>\" -dlmfile \"<Respective source DLM MBR file>\" \n";
	cout<<"\t -update       --> Prepare target disks for protection\n";
	cout<<"\t -file         --> File name which contains the source and target disk number mappings as below.\n";
	cout<<"\t                   <SrcDisk0>:<TgtDisk2>,<SrcDisk1>:<TgtDisk5>,<SrcDisk3>:<TgtDisk6>\n";
	cout<<"\t                   e.g. - 0:2,1:5,3:6,2:3 \n";
	cout<<"\t -dlmfile      --> Source DLM MBR file with full path from which MBR/GPT need to extract\n";

	//Follwing command will update the DLM information on target disks.
	cout<<"\n\n Following tasks will be done by the command :\n";
	cout<<"\t Extracts the MBR file and dumps DLM information on target disks\n";
	cout<<"\n\t EsxUtil -update -onlydisklayout -file \"<filename with complete path which has disk mappings>\" -dlmfile \"<Respective source DLM MBR file>\" \n";
	cout<<"\t -update          --> Prepare target disks for protection\n";
	cout<<"\t -onlydisklayout  --> This indicates that only dump the disk layout information on target disks as per the mapping.\n";
	cout<<"\t -file            --> File name which contains the source and target disk number mappings as below.\n";
	cout<<"\t                      <SrcDisk0>:<TgtDisk2>,<SrcDisk1>:<TgtDisk5>,<SrcDisk3>:<TgtDisk6>\n";
	cout<<"\t                      e.g. - 0:2,1:5,3:6,2:3 \n";
	cout<<"\t -dlmfile         --> Source DLM MBR file with full path from which MBR/GPT need to extract\n";

	//Command to dump MBR on target disks when the host protected using vCon
	//It will restore the target disks to its protection state.
	//It will download the MBR from CX if not found in MT and updates on the target disks from dinfo file mapping.
	cout<<"\n\n Command to dump MBR on target disks when the host protected using vCon\n";
	cout<<" Restores target disks to its protection state\n";
	cout<<" Downloads DLM file from CX if does not exist at MT and updates DLM info on the target disks as per dinfo file mapping for the host.\n";
	cout<<"\n\t EsxUtil -update -host <InMage Host GUID>\n";
	cout<<"\t -update       --> updates disk information of a particular host on the target disks which were protected using vCon.\n";
	cout<<"\t -host         --> InMage host GUID for which the task need to carry out.\n";

    cout<<"\n\n To get back the protected mount points\n";
	cout<<"\n\t EsxUtil -restoremountpoints -file <file name with complete path>\n";
	cout<<"\t\t -file <file name> --> file should contain the host inmage Id's for which mount points need to get back.\n";

	//Generating access signature by taking inputs from user.
	cout<<"\n\n Generating access signature by taking below inputs:\n";
	cout<<"\n\t EsxUtil -generatesig -cxapifuncname \"<CX API name>\" -file \"<File name under which access signature need to store>\" [-requestmethod \"<Request method>\"] [-cxapiaccessfile \"<CX Access file>\"] [-requestid <requestid>] [-version <version details>] \n";
	cout<<"\t -generatesig     --> Indicates to generate access signature.\n";
	cout<<"\t -cxapifuncname   --> Function Name - \"Name of the API\" (Mandatory)\n";
	cout<<"\t -file            --> File Name - Under which access signature information need to write (Mandatory).\n";
	cout<<"\t -requestmethod   --> Request Method (Optional).\n";
	cout<<"\t -cxapiaccessfile --> AccessFile (Optional)\n";
	cout<<"\t -requestid       --> RequestId details - required to generate the signature (Optional)\n";
	cout<<"\t -version         --> Version details (Optional)\n";

	/********** END OF DEBUGGING COMMANDS USAGE ***********/
	

#endif /* WIN32 */
	cout<<"\n\n To take Physical Snapshot \n";
	cout<<"\n\t EsxUtil -physnapshot [-d <Directory path> -cxpath <cx directory> -planid <CX Plan id> -op <operation> [-skipcdpclisnapshot]]\n";
	cout<<"\n\t     Note: skipcdpcli option when confident that snapshot completed for all hosts.. Use it with help of dev team\n";

	cout<<"\n\n To remove particular replication pairs for hosts...\n";
	cout<<"\n\t EsxUtil -removepair -d <Directory path> -cxpath <cx directory> -planid <CX Plan id> -op <diskremove/volumeremove> [-p2v]\n";

	cout<<"\n\n To perform Rollback when the target environment is a cloud\n";
	cout<<"\n\t EsxUtil -rollback -configfile <Configuration file path> -resultfile <return status of the job> -cloudenv <Cloud environment[azure/vcloud/amazoncloud/googlecloud] [-skipcdpclirollback]>\n";

	cout<<"\n\n To perform physical snapshot when the target environment is a cloud\n";
	cout<<"\n\t EsxUtil -physnapshot -configfile <Configuration file path> -resultfile <return status of the job> -cloudenv <Cloud environment[azure/vcloud/amazoncloud/googlecloud] [-skipcdpclisnapshot]>\n";

    cout<<"\n\n To display this info\n";
	cout<<"\n\t EsxUtil -help\n";
}

int main(int argc, char* argv[])
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
    CopyrightNotice displayCopyrightNotice;

	bool bLinuxReplication = false;
	bool bRollBack         = false;
    bool bNwSetup          = false;
    bool bAtBooting        = false;
	bool bTagType          = false;
	bool bResult           = true;
	bool bTagName          = false;
	bool bInfoBootSector   = false;  
    bool bExtraChecks      = false; 
    bool bFetchDiskNumMap  = false;
	bool bInstallVmWareTools = false;
	bool bLinNetwork       = false;
	bool bPhySnapShot	   = false;
	bool bDrDrill          = false;	
	bool bP2v              = false;
	bool bApplyTag         = false;
	bool bCrashConsistency = false;
	bool bImport           = false;
	bool bDlmMbr           = false;
	bool bDlmPartition     = false;
	bool bDlmRegDel        = false;
	bool bInstallDriver    = false;
	bool bRemovePair       = false;
	bool bCloudEnvMT       = false;
	bool bSkipCdpcli       = false;
	bool bRead             = false;
	bool bMountPoints      = false;
	bool bDownload         = false;
	bool bUpdate           = false;
	bool bEndOfDisk        = false;
	bool bOnlyDiskLayout   = false;
	bool bGenerateSig      = false;
	bool bResetGUID		   = false;
	bool bTestFailover     = false;
	bool bEnableRdpFileWall = false;
	
	string strTgtHostName       = "";
	string strOsName            = "";
	string strTagType           = "";
	string strTagName           = "";
	string strVmName            = "";
	string strFileName          = "";
	string strLogDir            = "";
	string strCxPath			= "";
	string strPlanId			= "1";
	string strOperation         = "";
	string strOption			= "dlm_normal";
	string strSysVol            = "";
	string strOsType            = "";
	string strRemovePair        = "";
	string strCloudEnv          = "";
	string strConfFile	        = "";
	string strResultFile        = "";
	string strDlmFile           = "";	
	string strDiskName          = "";
	string strNumOfBytes        = "";
	string strBytesToSkip       = "";
	string strCxApiFuncName     = "";
	string strRequestMethod     = "POST";
	string strCxApiAccessFile   = "/ScoutAPI/CXAPI.php";
	string strRequestId         = "";
	string strVersion           = "1.0";
	string newHostId;

	// PR#10815: Long Path support
	ACE::init();
	LocalConfigurator m_localConfigurator;

#ifdef WIN32
    //Display processID,Commandline and user
    GetCmdLineargsandProcessId();
#endif

	if(argc <= 1)
	{
		cout<<"Invalid number of  arguments detected.\n";
        Usage();
        return SVS_FALSE;
	}

	int iParsedOptions = 1;
	for (;iParsedOptions < argc; iParsedOptions++)
    {
		if(strcmpi(argv[iParsedOptions],OPTION_REPLICATION) == 0)
		{
			iParsedOptions++;
			bLinuxReplication = true;
			if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
            {                
                cout<<"Missing parameter for Replication.\n\n";
                Usage();
				return SVS_FALSE;
            }
            strOsName = argv[iParsedOptions];
			strOperation = "Protection";
			cout<< "choosen option: -replication "<< strOsName<<"\n";
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_NETWORK) == 0)
        {
            iParsedOptions++;
            bLinNetwork = true;
            if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
            {
                cout<<"Missing parameter for network option.\n\n";
                Usage();
                return SVS_FALSE;
            }
            strOsName = argv[iParsedOptions];
			cout<< "choosen option: -nw "<< strOsName <<"\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_ROLLBACK) == 0)
		{
			bRollBack = true;
			strOperation = "Recovery";
			cout<<"choosen option: -rollback\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_PHYSICAL_SNAPSHOT) == 0)
		{
			bPhySnapShot = true;
			strOperation = "Drdrill";
			cout<< "choosen option: -physnapshot\n";
        }
		else if(strcmpi(argv[iParsedOptions], OPTION_DR_DRILL) == 0)
		{
			bDrDrill = true;
			bLinuxReplication = true;
			strOperation = "Drdrill";
			cout<< "choosen option: -drdrill\n";
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_FILENAME) == 0)
		{
			iParsedOptions++;                
            if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
            {
                cout<<"\n Missing parameter for the option: "<<OPTION_FILENAME<<"\n\n";
                Usage();
				return SVS_FALSE;
        	}
			strFileName = argv[iParsedOptions];
			cout<<"choosen option: -file "<< strFileName<<"\n";
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_LOG_DIRECTOY) == 0)
		{
			iParsedOptions++;
			if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
			{				
				cout<<"Missing the Log directory path.\n\n";
				Usage();
				return EXIT_FAILURE;
			}
			strLogDir = argv[iParsedOptions];
			cout<<"choosen option: -d "<< strLogDir<<"\n";
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_CX_PATH) == 0)
		{
			iParsedOptions++;
			if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
			{				
				cout<<"Missing the CX path.\n\n";
				Usage();
				return EXIT_FAILURE;
			}
			strCxPath = argv[iParsedOptions];
			cout<< "Choosen Option: -cxpath "<< strCxPath<<"\n";
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_CX_PLANID) == 0)
		{
			iParsedOptions++;
			if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
			{				
				cout<<"Missing the Plan id.\n\n";
				Usage();
				return EXIT_FAILURE;
			}
			strPlanId = argv[iParsedOptions];
			cout<<"Choosen Option: -planid "<< strPlanId<<"\n";
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_OPERATION) == 0)
        {
            iParsedOptions++;
            if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
            {
                cout<<"Missing parameter for Operation option.\n\n";
                Usage();
                return SVS_FALSE;
            }
            strOperation = argv[iParsedOptions];
			cout<< "choosen option: -op "<< strOperation<<"\n";
        }
		else if(strcmpi(argv[iParsedOptions], OPTION_APPLY_TAG) == 0)
		{
			bApplyTag = true;
			cout<< "choosen option: -applytag\n";
		}
		else if(strcmpi(argv[iParsedOptions], OPTION_CRASHCONSISTENCY) == 0)
		{
			bCrashConsistency = true;
			cout<< "choosen option: -crashconsistency\n";
		}		
		else if(strcmpi(argv[iParsedOptions],OPTION_TGTHOST_NAME) == 0)
        {
            iParsedOptions++;
            if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
            {
                cout<<"Missing parameter for target host.\n\n";
                Usage();
                return SVS_FALSE;
            }
            strTgtHostName = argv[iParsedOptions];
			cout<< "choosen option: -t "<< strTgtHostName<<"\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_TAG_NAME) == 0)
        {
            iParsedOptions++;
            if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
            {
                cout<<"Missing parameter for network option.\n\n";
                Usage();
                return SVS_FALSE;
            }
            strTagName = argv[iParsedOptions];
			cout<< "choosen option: -tag "<< strTagName<<"\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_P2V) == 0)
		{
			bP2v = true;
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_REMOVE_PAIR) == 0)
		{
			bRemovePair = true;
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_CLOUD_ENV) ==0)
		{
			iParsedOptions++;
			bCloudEnvMT = true;
			if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
			{				
				cout<<"Missing the -cloudenv parameter value.\n\n";
				Usage();
				return EXIT_FAILURE;
			}
			strCloudEnv = argv[iParsedOptions];
			cout<< "Choosen Option: -cloudenv "<< strCloudEnv<<"\n";
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_CONFIG_FILE) ==0)
		{
			iParsedOptions++;
			if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
			{				
				cout<<"Missing the -configfile parameter value.\n\n";
				Usage();
				return EXIT_FAILURE;
			}
			strConfFile = argv[iParsedOptions];
			cout<< "Choosen Option: -configfile "<< strConfFile<<"\n";
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_RESULT_FILE) ==0)
		{
			iParsedOptions++;
			if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
			{				
				cout<<"Missing the -resultfile parameter value.\n\n";
				Usage();
				return EXIT_FAILURE;
			}
			strResultFile = argv[iParsedOptions];
			cout<< "Choosen Option: -resultfile "<<strResultFile<<"\n";
		}
		else if((strcmpi(argv[iParsedOptions], OPTION_SKIP_CDPCLI_SNAPSHOT) == 0) || (strcmpi(argv[iParsedOptions], OPTION_SKIP_CDPCLI_ROLLBACK) == 0))
		{
			bSkipCdpcli = true;
			if(strcmpi(argv[iParsedOptions], OPTION_SKIP_CDPCLI_ROLLBACK) == 0)
				cout<<"choosen option: -skipcdpclirollback\n";
			else
				cout<< "choosen option: -skipcdpclisnapshot\n";
		}
		
		//As per requirement its gonna to be a coomand line tool so not considering return value
#ifdef WIN32        
        else if(strcmpi(argv[iParsedOptions],OPTION_NW_SETUP) == 0)
		{
			bNwSetup = true;
			cout<<"choosen option: -nwsetup\n";
        }
        else if(strcmpi(argv[iParsedOptions],OPTION_AT_BOOTING) == 0)
		{
			bAtBooting = true;
			cout<<"choosen option: -atbooting\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_IMPORT_DISK) == 0)
		{
			bImport = true;
			cout<< "choosen option: -import\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_DLM_REG_DELETE) == 0)
		{
			bDlmRegDel = true;
			cout<< "choosen option: -deldlmreg\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_BOOTSECTOR_FIX) == 0)
		{
			cout<< "choosen option: -bootsectorfix\n";
			bInfoBootSector = true;

			iParsedOptions++;                
    		if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
            {
  				cout<<"\n Missing parameter for the option: -bootsectorfix\n\n";
             	Usage();
				return SVS_FALSE;
            }
			char* ar[3] = {0};
			ar[0] = "-v";
			ar[1] = "-f";
			ar[2] = argv[iParsedOptions];
			bootSectFix objbootSectFix(true);
			if(0 != objbootSectFix.fixBootSector(3,ar))
					return SVS_FALSE;
			return SVS_OK;
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_BOOTSECTOR_INFO) == 0)
		{
			cout<< "choosen option: -bootsectorinfo\n";
			bInfoBootSector = true;

			iParsedOptions++;                
    		if((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
            {
  				cout<<"\n Missing parameter for the option: -bootsectorinfo\n\n";
             	Usage();
				return SVS_FALSE;
            }
			char* ar[2] = {0};
			ar[0] = "-v";
			ar[1] = argv[iParsedOptions];
			bootSectFix objbootSectFix(false);
			if(0 != objbootSectFix.fixBootSector(2,ar))
					return SVS_FALSE;
			return SVS_OK;
		}
        else if(strcmpi(argv[iParsedOptions],OPTION_EXTRACHECKS) == 0)
		{
            bExtraChecks = true;
			cout<< "choosen option: -extrasafetychecks\n";
        }
        else if(strcmpi(argv[iParsedOptions],OPTION_FETCHDISKMAP) == 0)
        {
            bFetchDiskNumMap = true;
			cout<<"choosen option: -fetchdiskmap\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_VMWARETOOLS) == 0)
		{
			bInstallVmWareTools = true;
			cout<<"choosen option: -installVmWareTools\n";
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_DLMMBR) == 0)
		{
			bDlmMbr = true;
			cout<<"choosen option: -dlmmbr\n";
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_DLM_PARTITION) == 0)
		{
			bDlmPartition = true;
			cout<<"choosen option: -dlmpartition\n";
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_READ) == 0)
		{
			bRead = true;
			cout<<"choosen option: -read\n";
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_DLM_OPN) == 0)
        {
            iParsedOptions++;
            if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
            {
                cout<<"Missing parameter for dlm option.\n\n";
                Usage();
                return SVS_FALSE;
            }
            strOption = argv[iParsedOptions];
			cout<<"choosen option: -opn "<<strOption<<"\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_INSTALL_DRIVER) == 0)
		{
			bInstallDriver = true;
			cout<<"choosen option: -installw2k3driver\n";
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_SYSTEM_VOLUME) == 0)
        {
            iParsedOptions++;
            if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
            {
                cout<<"Missing parameter for dlm option.\n\n";
                Usage();
                return SVS_FALSE;
            }
            strSysVol = argv[iParsedOptions];
			cout<<"choosen option: -sysvol "<< strSysVol<<"\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_OS_TYPE) == 0)
        {
            iParsedOptions++;
            if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
            {
                cout<<"Missing parameter for dlm option.\n\n";
                Usage();
                return SVS_FALSE;
            }
            strOsType = argv[iParsedOptions];
			cout<<"choosen option: -ostype "<<strOsType<<"\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_DOWNLOAD) == 0)
		{
			bDownload = true;
			cout<< "choosen option: -download\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_UPDATE) == 0)
		{
			bUpdate = true;
			cout<< "choosen option: -update\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_DISK) == 0)
        {
            iParsedOptions++;
            if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
            {
                cout<<"Missing parameter for -disk option.\n\n";
                Usage();
                return SVS_FALSE;
            }
            strDiskName = argv[iParsedOptions];
			cout<<"choosen option: -disk "<< strDiskName<<"\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_NUMOFBYTES) == 0)
        {
            iParsedOptions++;
            if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
            {
                cout<<"Missing parameter for -numofbytes option.\n\n";
                Usage();
                return SVS_FALSE;
            }
            strNumOfBytes = argv[iParsedOptions];
			cout<<"choosen option: -numofbytes "<<strNumOfBytes<<"\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_SKIP_BYTES) == 0)
        {
            iParsedOptions++;
            if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
            {
                cout<<"Missing parameter for -skip option.\n\n";
                Usage();
                return SVS_FALSE;
            }
            strBytesToSkip = argv[iParsedOptions];
			cout<<"choosen option: -skip "<<strBytesToSkip<<"\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_LAST) == 0)
		{
			bEndOfDisk = true;
			cout<<"choosen option: -last\n";
        }
		else if(strcmpi(argv[iParsedOptions], OPTION_DLM_FILE) == 0)
		{
			iParsedOptions++;
            if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
            {
                cout<<"Missing parameter for -dlmfile option.\n\n";
                Usage();
                return SVS_FALSE;
            }
            strDlmFile = argv[iParsedOptions];
			cout<<"choosen option: -dlmfile "<<strDlmFile<<"\n";
		}
		else if(strcmpi(argv[iParsedOptions], OPTION_HOST_GUID) == 0)
		{
			iParsedOptions++;
            if( (iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-') )
            {
                cout<<"Missing parameter for -host option.\n\n";
                Usage();
                return SVS_FALSE;
            }
            strVmName = argv[iParsedOptions];
			cout<<"choosen option: -host "<< strVmName<<"\n";
		}
        else if(strcmpi(argv[iParsedOptions],OPTION_GETBACK_MOUNTPOINT) == 0)
		{			
			bMountPoints = true;
			cout<<"choosen option: -restoreMountPoints\n";
        }
		else if(strcmpi(argv[iParsedOptions],OPTION_ONLY_DISK_LAYOUT) == 0)
		{
			bOnlyDiskLayout = true;
			cout<<"choosen option: -onlydisklayout\n";
        }
		else if (strcmpi(argv[iParsedOptions], OPTION_RESETGUID) == 0)
		{
			bResetGUID = true;
			cout << "choosen option: -resetguid";
		}
		else if (strcmpi(argv[iParsedOptions], OPTION_NEW_HOST_ID) == 0)
		{
			iParsedOptions++;
			if ((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			{
				cout << "Missing parameter for -newhostid option.\n\n";
				Usage();
				return SVS_FALSE;
			}
			newHostId = argv[iParsedOptions];
			cout << "choosen option: -newhostid " << newHostId << "\n";
		}
		else if (strcmpi(argv[iParsedOptions], OPTION_TEST_FAILOVER) == 0)
		{
			bTestFailover = true;
			cout << "choosen option: -testfailover";
		}
		else if (strcmpi(argv[iParsedOptions], OPTION_ENABLE_RDP_FIREWALL) == 0)
		{
			bEnableRdpFileWall = true;
			cout << "choosen option: -enablerdpfirewall";
		}
#endif
		else if (strcmpi(argv[iParsedOptions], OPTION_GENERATE_SIGNATURE) == 0)
		{
			bGenerateSig = true;
			cout << "choosen option: -generatesig\n";
		}
		else if (strcmpi(argv[iParsedOptions], OPTION_CXAPI_FUNC_NAME) == 0)
		{
			iParsedOptions++;
			if ((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			{
				cout << "Missing parameter for -cxapifuncname option.\n\n";
				Usage();
				return SVS_FALSE;
			}
			strCxApiFuncName = argv[iParsedOptions];
			cout << "choosen option: -cxapifuncname " << strCxApiFuncName << "\n";
		}
		else if (strcmpi(argv[iParsedOptions], OPTION_REQUEST_METHOD) == 0)
		{
			iParsedOptions++;
			if ((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			{
				cout <<  "Missing parameter for -requestmethod option.\n\n";
				Usage();
				return SVS_FALSE;
			}
			strRequestMethod = argv[iParsedOptions];
			cout << "choosen option: -requestmethod " << strRequestMethod << "\n";
		}
		else if (strcmpi(argv[iParsedOptions], OPTION_CXAPI_ACCESSFILE) == 0)
		{
			iParsedOptions++;
			if ((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			{
				cout << "Missing parameter for -cxapiaccessfile option.\n\n";
				Usage();
				return SVS_FALSE;
			}
			strCxApiAccessFile = argv[iParsedOptions];
			cout << "choosen option: -cxapiaccessfile " << strCxApiAccessFile << "\n";
		}
		else if (strcmpi(argv[iParsedOptions], OPTION_REQUESTID) == 0)
		{
			iParsedOptions++;
			if ((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			{
				cout << "Missing parameter for -requestid option.\n\n";
				Usage();
				return SVS_FALSE;
			}
			strRequestId = argv[iParsedOptions];
			cout << "choosen option: -requestid " << strRequestId << "\n";
		}
		else if (strcmpi(argv[iParsedOptions], OPTION_VERSION) == 0)
		{
			iParsedOptions++;
			if ((iParsedOptions >= argc) || (argv[iParsedOptions][0] == '-'))
			{
				cout << "Missing parameter for -version option.\n\n";
				Usage();
				return SVS_FALSE;
			}
			strVersion = argv[iParsedOptions];
			cout << "choosen option: -verison " << strVersion << "\n";
		}
		else if(strcmpi(argv[iParsedOptions],OPTION_HELP) == 0)
		{
			Usage();
			return SVS_OK;
		}
        else
        {
            cout<<"\n Detected Invalid option : "<<argv[iParsedOptions]<<"\n\n";
            Usage();
            return SVS_FALSE;            
        }
	}

	if( (bLinuxReplication && bRollBack)    || 
        (bLinuxReplication && bNwSetup)     ||
        (bRollBack         && bNwSetup)     ||
		(bPhySnapShot      && bRollBack)    ||
		(bLinuxReplication && bPhySnapShot) ||
		(bPhySnapShot      && bNwSetup))
	{
        cout<<"\n Two options cannot be combined.\n";
        Usage();
        return SVS_FALSE;
	}	
	
	if(bLinuxReplication && !bDrDrill)
	{
		if(strOsName.empty())
		{
			cout<<"\n EsxUtil -replication linux \n";	
			return SVS_FALSE;
		}
	}
	else if(bLinNetwork)
	{
		if(strOsName.empty())
		{
            cout<<"\n EsxUtil -nw linux \n";
            return SVS_FALSE;
        }
	}
	else if(bRollBack)
	{
        // keep this else if... else it will return Invalid arguments because of last else.
	}
	else if(bPhySnapShot)
	{
		// keep this else if... else it will return Invalid arguments because of last else.
	}
	else if(bDrDrill)
	{
		// keep this else if... else it will return Invalid arguments because of last else.
	}
	else if(bApplyTag)
	{
		if(strTgtHostName.empty())
		{
			cout<<"\n Detected empty hostname. Please specify file name with network details.\n";
			Usage();
			return SVS_FALSE;
		}
	}
#ifdef WIN32
    else if(bNwSetup)
    {
        if(strFileName.empty())
		{
            cout<<"\n Detected empty file name. Please specify file name with network details.\n";
			Usage();
			return SVS_FALSE;
		}
    }
    else if(bAtBooting)
    {
		if(strFileName.empty() && strCloudEnv.empty())
		{
            cout<<"\n Detected empty file name or no cloud environment. Please specify file name with network details for booting option or the cloud enviroment details.\n";
			Usage();
			return SVS_FALSE;
		}
    }
	else if(bInstallVmWareTools)
	{
		cout<<"\n Installing VMWare tools Option is selected.\n";
	}
	else if(bImport)
	{
		cout<<"\n Import Foreign Disks selected \n\n";
	}
	else if(bDlmRegDel)
	{
		cout<<"\n Delete DLM registry selected \n\n";
	}
	else if(bDlmMbr)
	{
		if(strFileName.empty())
		{
            cout<<"\n Detected empty file name. Please specify file name for collecting\reading MBR information of all disks\n";
			Usage();
			return SVS_FALSE;
		}
	}
	else if(bDlmPartition)
	{
		if(strFileName.empty())
		{
            cout<<"\n Detected empty file name. Please specify partition file name for reading information of all disks\n";
			Usage();
			return SVS_FALSE;
		}
	}
	else if(bInstallDriver)
	{
		if(strOsType.empty() || strSysVol.empty())
		{
			cout<<"\n Detected empty system volume or system type. \n";
			Usage();
			return SVS_FALSE;
		}
	}
	else if(bDownload)
	{
		if(strDiskName.empty() || strNumOfBytes.empty())
		{
			cout<<"\n Disk number (or) Number of bytes to download is not specified.\n";
			Usage();
			return SVS_FALSE;
		}
	}
	else if(bUpdate)
	{
		if(!strFileName.empty() && !strDlmFile.empty())
		{
			cout<<"\n Multiple disk update mechanism has been chossen.\n";
		}
		else if(!strDiskName.empty() && !strNumOfBytes.empty() && !strFileName.empty())
		{
			cout<<"\n Single disk update mechanism has been choosen.\n";
		}
		else if(!strVmName.empty())
		{
			cout<<"\n Multiple disk update mechanism has been chossen from given host uuid.\n";
		}
		else
		{
			cout<<"\n some of the parameters might be missing with \"-update\"...\n";
			Usage();
			return SVS_FALSE;
		}
	}
    else if(bMountPoints)
    {		
        if(strFileName.empty())
		{
            cout<<"\n Detected empty file name. Please specify file name with network details for booting option.\n";
			Usage();
			return SVS_FALSE;
		}
    }
#endif /* WIN32 */
	else if(bRemovePair)
    {
		if(strLogDir.empty() || strCxPath.empty() || strPlanId.empty() || strOperation.empty())
		{
            cout<<"\nDirectory path\\cx directory\\CX Plan id\\Operation is missing. Check the usage for remove pair...\n";
			Usage();
			return SVS_FALSE;
		}
    }
	else if (bGenerateSig)
	{
		if (strFileName.empty() || strCxApiFuncName.empty())
		{
			cout << "\nFile name or cxapi function name is empty. Check usage for generate signature...\n";
			Usage();
			return SVS_FALSE;
		}
	}
    else
    {
        cout<<"\n Invalid arguments.\n";
        Usage();
        return SVS_FALSE;
    }

	ChangeVmConfig obj;
	LinuxReplication objLinRep;
#ifdef WIN32
	if(strLogDir.empty())
		strLogDir = m_localConfigurator.getInstallPath() + std::string("\\Failover") + std::string("\\data");

	if(FALSE == CreateDirectory(strLogDir.c_str(),NULL))
	{
		if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
		{
			cout<<"Directory already exists. Considering it not as an Error.\n";            				
		}
		else
		{
			cout<<"Failed to create Directory - "<<strLogDir<<". ErrorCode : ["<<GetLastError()<<"]\n";
		}
	}

	obj.m_VxInstallPath = m_localConfigurator.getInstallPath();
	strLogDir = strLogDir + "\\";
#else
	if(strLogDir.empty())
		strLogDir = m_localConfigurator.getInstallPath() + std::string("failover_data");
	if(false == obj.createDirectory(strLogDir))
	{
		cout<<"Failed to create Directory - "<<strLogDir<<"\n";
	}
	strLogDir = strLogDir + "/";
#endif
	objLinRep.AssignSecureFilePermission(strLogDir, true);
	std::string sLogFileName;
	try 
    {
		if(strLogDir.empty())
			sLogFileName = m_localConfigurator.getLogPathname()+ "EsxUtil.log";
		else
		{
#ifdef WIN32
			sLogFileName = strLogDir + "\\EsxUtil.log";
#else
			sLogFileName = strLogDir + "/EsxUtil.log";
#endif
		}
        SetLogFileName(sLogFileName.c_str()) ;
	    SetLogLevel( 7 ) ;
        SetLogHttpIpAddress(GetCxIpAddress().c_str());
        SetLogHttpPort( m_localConfigurator.getHttp().port );
        SetLogHostId( m_localConfigurator.getHostId().c_str() );
		SetLogRemoteLogLevel( m_localConfigurator.getRemoteLogLevel() );
		SetSerializeType( m_localConfigurator.getSerializerType() ) ;
        SetLogHttpsOption(m_localConfigurator.IsHttps());
		SetLogInitSize(26214400);
    }
    catch (...) 
    {
        cout<<"\n Local Logging initialization failed.\n";
    }

	//Assign Secure permission to the log file
	objLinRep.AssignSecureFilePermission(sLogFileName);

	//setting the string to use http (or) https
	string strHttps = string("http://");
	if(m_localConfigurator.IsHttps())
		strHttps = string("https://");

	if (!bNwSetup && !bAtBooting && !bInstallVmWareTools && !bImport)
	{
		//initilizing cdp init quit event
		bool callerCli = true;
		CDPUtil::InitQuitEvent(callerCli);
		if (!InitializeConfigurator())
		{
			//if configurator initialization fails then making bCxdown bool variable to true
			DebugPrintf(SV_LOG_ERROR, "\n [ERROR]: Configurator initialization failed...\n");
			DebugPrintf(SV_LOG_ERROR, "\n Job Failed. Retry the operation after sometime.\n");
			bResult = false;
			goto cleanup;
		}
	}	

	if(bRollBack || bPhySnapShot)
	{
		RollBackVM objRoll(bExtraChecks, bFetchDiskNumMap, bPhySnapShot, strLogDir, strCxPath, strPlanId, strOperation);
		objRoll.m_strLogFile = sLogFileName;
		objRoll.m_bSkipCdpcli = bSkipCdpcli;
		objRoll.m_objLin.m_strCmdHttps = strHttps;
		if(bCloudEnvMT)
		{
			objRoll.SetCloudEnv(bCloudEnvMT, strCloudEnv, strConfFile, strResultFile);
			if(false == objRoll.startRollBackCloud())
			{
				bResult = false;
				goto cleanup;
			}
		}
		else
		{
			if(false == objRoll.startRollBack())
			{
				bResult = false;
				goto cleanup;
			}
		}
	}
	else if(bRemovePair)
	{
		RollBackVM objRoll(bExtraChecks, bFetchDiskNumMap, bPhySnapShot, strLogDir, strCxPath, strPlanId, strOperation);
		objRoll.m_strLogFile = sLogFileName;
		objRoll.m_bP2V = bP2v;
		objRoll.m_bSkipCdpcli = bSkipCdpcli;
		objRoll.m_objLin.m_strCmdHttps = strHttps;
		if(false == objRoll.RemoveReplicationPairs())
		{
			bResult = false;
			goto cleanup;
		}
	}
#ifndef WIN32
	else if(bLinNetwork)
	{
		try
		{
			bResult = objLinRep.getLinSrcNetworkInfo();
		}
		catch(exception &ex)
		{
			DebugPrintf(SV_LOG_INFO,"\n Exception Caught: ");
			DebugPrintf(SV_LOG_INFO,"%s\n",ex.what());
			bResult = false;
		}
		catch(...)
		{
			DebugPrintf(SV_LOG_INFO,"\n Exception Caught");
			bResult = false;
		}

	}
	else if(bLinuxReplication)
	{
		try
		{
			string strHostID = m_localConfigurator.getHostId();
			LinuxReplication objLinuxReplication(bDrDrill, strHostID, strLogDir, strCxPath, strPlanId, strOperation);
			objLinuxReplication.m_strLogFile = sLogFileName;
			objLinuxReplication.m_strCmdHttps = strHttps;
			bResult = objLinuxReplication.initLinuxTarget();
		}
		catch(exception &ex)
		{
			DebugPrintf(SV_LOG_INFO,"\n Exception Caught: ");
            DebugPrintf(SV_LOG_INFO,"%s\n",ex.what());
			bResult = false;
		}
		catch(...)
		{
            DebugPrintf(SV_LOG_INFO,"\n Exception Caught");
			bResult = false;
		}
	}
	else if(bApplyTag)
	{
		DebugPrintf(SV_LOG_INFO,"Applying Consistency bookmark.\n");
		if ( bP2v )
		{
			DebugPrintf(SV_LOG_INFO,"Machine Type = Physical Machine.Collecting information which will be used to prepare target.\n");
			//here is where we have to change.
			//we have to call the SCRIPT, which does all this stuff
			//1. Create INITRD IMAGE FILES.
			//2. Create a file which has all the information of Source.
			bResult = objLinRep.MakeInformation( TheConfigurator );
			if ( true != bResult )
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to collect information of physical machine.\n");
				//Consistency Job has to be failed.
			}
			else
				DebugPrintf(SV_LOG_INFO, "Successfully collected information.\n");
		}

        bResult = objLinRep.GenerateTagForSrcVol(strTgtHostName,strTagName,bCrashConsistency,TheConfigurator);
		goto cleanup;
	}
#endif /* Not WIN32 */

#ifdef WIN32
    else if(bNwSetup)
    {
		//ChangeVmConfig obj;
		if(false == obj.checkIfFileExists(strFileName))
		{
            DebugPrintf(SV_LOG_ERROR,"\n File %s does not exist in the specified path.\n", strFileName.c_str());
			bResult = false;
			goto cleanup;
		}		
		if(false == obj.ConfigureNWSettings(strFileName))
		{
			bResult = false;
			goto cleanup;
		}
    }
	else if(bImport)
    {
		if(false == obj.ImportForeignDisks())
		{
			bResult = false;
			goto cleanup;
		}
    }
    else if(bAtBooting)
    {
		DebugPrintf(SV_LOG_INFO, "\nReset HostId: %s\nNew HostId: %s\nIs TestFailover: %s\nEnvironment: %s\n",
			bResetGUID ? "Yes" : "No",
			newHostId.c_str(),
			bTestFailover ? "Yes" : "No",
			strCloudEnv.c_str());

		//TFS ID : 1802061.
		obj.m_strDatafolderPath = strLogDir;
		obj.m_newHostId = newHostId;
		obj.m_bTestFailover = bTestFailover;
		obj.m_bEnableRdpFireWall = bEnableRdpFileWall;
        if(false == obj.ConfigureSettingsAtBooting(strFileName, bP2v, strCloudEnv, bResetGUID))
		{
			bResult = false;
			goto cleanup;
		}
    }
	else if(bInstallVmWareTools)
	{
		if(false == obj.InstallVmWareTools())	//Install the VmWare tools in case of P2V only
		{
			bResult = false;
			goto cleanup;
		}
	}
	else if(bDlmMbr)
	{
		if(bRead)
		{
			if(strFileName.find("mbrfilelist") != string::npos)
			{
				list<string> listFiles;
				string strLineRead = "";
				ifstream inFile(strFileName.c_str());
				if(!inFile.is_open())
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to read the file - %s\n",strFileName.c_str());
					bResult = false;
				}
				else
				{
					while(getline(inFile,strLineRead))
					{
						listFiles.push_back(strLineRead);
					};
				}

				for(list<string>::iterator iterList = listFiles.begin(); iterList != listFiles.end(); iterList++)
				{
					if(1 == obj.GetDiskAndMbrDetails(*iterList))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to read the DLM Mbr file %s\n", iterList->c_str());
						bResult = false;
					}
				}
			}
			else
			{
				//Reading the MBR file
				if(1 == obj.GetDiskAndMbrDetails(strFileName))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to read the DLM Mbr file %s\n", strFileName.c_str());
					bResult = false;
				}
			}
		}
		else
		{
			//Creating the MBR file and partition file(if dlm_all option is selected)
			std::list<std::string> erraticVolumeGuids;
			if(S_FALSE == obj.InitCOM())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to initialize COM.\n");
				bResult = false;
			}
			else
			{
				std::list<std::string> UploadFiles;
				DLM_ERROR_CODE RetVal = StoreDisksInfo(strFileName.c_str(), UploadFiles, erraticVolumeGuids, strOption);
				if(!erraticVolumeGuids.empty())
				{
					DebugPrintf(SV_LOG_INFO, "Some Failed state Volumes are : \n");
					std::list<std::string>::iterator iter = erraticVolumeGuids.begin();
					for(; iter != erraticVolumeGuids.end(); iter++)
					{
						DebugPrintf(SV_LOG_INFO, "\t%s\n", iter->c_str());
					}
				}
				if((DLM_FILE_CREATED == RetVal) || (DLM_FILE_NOT_CREATED == RetVal))
				{
					std::list<std::string>::iterator iter = UploadFiles.begin();
					DebugPrintf(SV_LOG_INFO, "File need to upload in CS are : \n");
					for(; iter != UploadFiles.end(); iter++)
					{
						DebugPrintf(SV_LOG_INFO, "\t%s\n", iter->c_str());
					}
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to collect the DLM MBR file %s. Error Code - %u\n", strFileName.c_str(), RetVal);
					bResult = false;
				}
			}
			CoUninitialize();
		}
	}

	else if(bDlmPartition)
	{
		if(bRead)
		{
			//Reading partition file information
			if(1 == obj.GetDiskAndPartitionDetails(strFileName))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to read the DLM Partition file %s\n", strFileName.c_str());
				bResult = false;
			}
		}
		else
		{
			//Code need to write to ceate only the partition file
			//Currently we can generate the partition file by using -opn dlm_all along with -dlmmbr
		}
	}

	else if(bDlmRegDel)
	{
		DLM_ERROR_CODE RetVal = DeleteDlmRegitryEntries();
		if(DLM_ERR_SUCCESS != RetVal)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to cleanup the DLM related registry entries : %u\n", RetVal);
			bResult = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully cleaned up the DLM related registry entries\n");
		}
	}
	else if(bInstallDriver)
	{
		if(false == obj.ExtractW2k3Driver(strSysVol, strOsType)) //Gets the driver file in plan name folder
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to extarct the driver file to the target path\n");
			bResult = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully extarcted the driver file to the target path\n");
		}
	}
	else if(bDownload)
	{
		strDiskName = string("\\\\.\\PHYSICALDRIVE") + strDiskName;
		if(false == obj.DownloadDataFromDisk(strDiskName, strNumOfBytes, bEndOfDisk, strBytesToSkip, strFileName))
		{
			DebugPrintf(SV_LOG_ERROR, "[ERROR] Failed to download %s bytes of data from Disk %s.\n", strNumOfBytes.c_str(), strDiskName.c_str());
			DebugPrintf(SV_LOG_ERROR, "Check the Disk is in proper state or not...");
			bResult = false;
		}
	}
	else if(bUpdate)
	{
		if(S_FALSE == obj.InitCOM())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to initialize COM.\n");
			bResult = false;
		}
		else
		{
			if(!strFileName.empty() && !strDlmFile.empty())
			{
				if(bOnlyDiskLayout)
				{
					//It only dumps the disk layout information on the target disks
					if(false == obj.UpdateDiskLayoutInfo(strFileName, strDlmFile))
					{
						DebugPrintf(SV_LOG_ERROR, "[ERROR] Failed to update the disk layout information on target disks.\n");
						bResult = false;
					}
				}
				else
				{
					//Also it creates the mount points
					if(false == obj.PrepareTargetDiskForProtection(strFileName, strDlmFile))
					{
						DebugPrintf(SV_LOG_ERROR, "[ERROR] Failed to prepare the target side disks for protection.\n");
						bResult = false;
					}
				}
			}
			else if(!strDiskName.empty() && !strNumOfBytes.empty() && !strFileName.empty())
			{
				strDiskName = string("\\\\.\\PHYSICALDRIVE") + strDiskName;
				if(false == obj.UpdateDataOnDisk(strDiskName, strNumOfBytes, bEndOfDisk, strBytesToSkip, strFileName))
				{
					DebugPrintf(SV_LOG_ERROR, "[ERROR] Failed to update %s bytes of data on Disk %s.\n", strNumOfBytes.c_str(), strDiskName.c_str());
					DebugPrintf(SV_LOG_ERROR, "Check the Disk is in proper state or not...");
					bResult = false;
				}
			}
			else if(!strVmName.empty())
			{
				RollBackVM objRoll;
				if(!objRoll.RestoreTargetDisks(strVmName))
				{
					DebugPrintf(SV_LOG_ERROR, "[ERROR] Failed to restore the target disks to its original protection state for host %s.\n", strVmName.c_str());
					bResult = false;
				}
			}
		}
	}
    else if(bMountPoints)
    {
		if(S_FALSE == obj.InitCOM())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to initialize COM.\n");
			bResult = false;
		}
		//ChangeVmConfig obj;
		if(false == obj.checkIfFileExists(strFileName))
		{
            DebugPrintf(SV_LOG_ERROR,"\n File %s does not exist in the specified path.\n", strFileName.c_str());
			bResult = false;
			goto cleanup;
		}		
		RollBackVM objRoll;
		if(false == objRoll.ConfigureMountPoints(strFileName))
		{
			bResult = false;
			goto cleanup;
		}
    }
#endif /* WIN32 */
	else if (bGenerateSig)
	{
		if (obj.checkIfFileExists(strFileName))
		{
#ifdef WIN32
			DeleteFile(strFileName.c_str());
#else
			remove(strFileName.c_str());
#endif
		}
		if (strRequestId.empty())
			strRequestId = objLinRep.GetRequestId();
		string strAccessSignature = objLinRep.GetAccessSignature(strRequestMethod, strCxApiAccessFile, strCxApiFuncName, strRequestId, strVersion);

		ofstream result(strFileName.c_str());
		if (result.good())
		{
			result << strAccessSignature;
			result.close();
			objLinRep.AssignSecureFilePermission(strFileName);
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to write access signature into file - %s\n", strFileName.c_str());
			bResult = false;
			goto cleanup;
		}
	}

cleanup:
	//invoking unint quit event
	CDPUtil::UnInitQuitEvent();
	//need to add stop local logging 
	CloseDebug();

	//stoppinfg the configurator
	if(bConfigStatus)
	{
		TheConfigurator = NULL;
	}

	//No need to convert boolean value to correcponding integer ..
	if(bResult == true)
        return SVS_OK;
	else
		return SVS_FALSE;
}

