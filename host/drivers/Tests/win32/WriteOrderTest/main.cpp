#include<windows.h>
#include<stdio.h>
#include <stdlib.h>
#include<string.h>
#include"Involflt.h"
#include"main.h"
#include "simulated_test.h"

MAIN_CMD arstMainCmdsList[MAX_CMDS];
int nMainCmds;
ULONG bSendShutdownNotify;
ULONG bSendProcessStartNotify;
int nargs_parsed;
int argc;
char **argv;

extern WRITE_ORDER_TEST_INFO stWriteOrderTestInfo;

void get_param_name(char *param_string,char *name) {
	char *temp = param_string;
	int len=0;

	temp++;
	while(*temp != '=') {
		name[len++] = *temp;
		temp++;
	}
	name[len] = '\0';
}

void get_value_string(char *param_string,char *name) {
	char *temp = param_string;
	int len=0;

	while(*temp != '=') temp++;
	temp++;
	while(*temp != '=') {
		name[len++] = *temp;
		temp++;
	}
	name[len] = '\0';
}

PARAM_PARSE_ERROR get_value_from_param_string(char *str,PARAM_TYPE type,void *value, const size_t valuelen = 0) {
	char *temp=str;
	char *val_str;
	int val=0;
	int len=0;
	int multiplier;
	int i;

	while(*temp++ != '=');
	val_str=temp;
	if(*val_str=='\0') {
		return VALUE_NOT_SPECIFIED;
	}

	switch(type) {
	case PARAM_TYPE_INT:
		temp = val_str;
		while(*temp!='\0') {
			if((*temp >= '0') || (*temp <= '9')) { temp++; continue; }
			return INVALID_VALUE_SPECIFIED;
		}
		val = 0;
		temp = val_str;
		while(*temp!='\0') {
			val = val*10 + (*temp-'0');
			temp++;
		}
		*((int *)value) = val;
		break;
	case PARAM_TYPE_STRING:
		temp = val_str;
		while(*temp++!='\0') {
			if((*temp >= '0') || (*temp <= '9')) {
				continue;
			}
			if((*temp >= 'a') || (*temp <= 'z')) {
				continue;
			}
			if((*temp >= 'A') || (*temp <= 'Z')) {
				continue;
			}
			return INVALID_VALUE_SPECIFIED;
		}
		strcpy_s((char *)value, valuelen, val_str);
		break;
	case PARAM_TYPE_DRIVE_LETTER:
		if(!IS_DRIVE_LETTER(val_str)) 
			return INVALID_VALUE_SPECIFIED;
		*((char *)value) = val_str[0];
		//if(*((char *)value) >= 'a') *((char *)value) += 'A'-'a';
		break;
	case PARAM_TYPE_MEMORY:
		temp = val_str;
		while(*temp != '\0') {
			if((*temp >= '0') && (*temp <= '9')) { temp++; continue; }
			if((*(temp+1) == '\0') && ((*temp == 'k') || (*temp == 'm'))) {temp++; continue; }
			return INVALID_VALUE_SPECIFIED;
		}
		temp = val_str;
		len = 0;
		while(*temp != '\0') {
			len++;
			temp++;
		}

		if(val_str[len-1] == 'm')
			multiplier=MEGA_BYTES;
		else if(val_str[len-1] == 'k')
			multiplier=KILO_BYTES;
		else multiplier = 1;
		if((val_str[len-1] == 'm') || (val_str[len-1] == 'k')) len--;
		val=0;
		for(i=0;i<len;i++) val = (val*10)+(val_str[i]-'0');
		val*=multiplier;
		*((int *)value) = val;
		break;
	default:
		return INVALID_VALUE_SPECIFIED;
	}
	return PARAM_CORRECT;
}

void PrintUsage() {
	printf("One or more of the following options should be used:\n");
	printf("1.--writedata /drive= /pattern= /offset= /size=\n");
	printf("      All the above parameters are mandatory\n");
	printf("      An io will be filled with pattern for the entire size and will be written\n");
	printf("      onto the drive starting at the offset\n");
	printf("2.--writetag /drive= /tag=\n");
	printf("      Atleast one drive option should be specified, but one and only one tag option\n");
	printf("      should be specified\n");
	printf("      All the drives specified will be tagged with the given tag\n");
	printf("3.--processstartnotify\n");
	printf("      No Parameters should be specified for above command\n");
	printf("      This command directs the application to send processstartnotify ioctl\n");
	printf("4.--printnextdb /drive= /commit= /printdataios= /printpatterns= /printtagios=\n");
	printf("      All the parameters are optional\n");
	printf("      By default next db will be got from the driver and data io\n");
	printf("      info and tag io info will be printed but not committed\n");
	printf("5.--simulatedtest /nwritespertag= /datatest= -writestream /drive= /nios= /maxiosz= /miniosz=\n");
	printf("      writespertag option is optional, by default a tag will be issued randomly\n");
	printf("      per 8 ios\n");
	printf("      Any number of writestream sub-commands can be specified along with this option\n");
	printf("      and all parameters to writestream are mandatory\n");
	printf("      This simulates an automatic writeorder test with the parameters given\n");
}

int ParseWriteStreamSubCmd(PWRITE_STREAM_PARAMS pstWriteStreamParams) {
	char *cur_arg;
	int bNMaxIOs,bDriveLetter,bMinIOSz,bMaxIOSz;
	PARAM_PARSE_ERROR ret;

	bNMaxIOs = bDriveLetter = bMinIOSz = bMaxIOSz = 0;
	while(nargs_parsed < argc) {
		cur_arg = argv[nargs_parsed];
		if(IS_PARAM(cur_arg)) {
			if(!strncmp(cur_arg,"/drive=",strlen("/drive="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_DRIVE_LETTER,
												&pstWriteStreamParams->DriveLetter);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for drive in %s\n",cur_arg);
					return 0;
				}
				bDriveLetter = 1;
				nargs_parsed++;
			}
			else if(!strncmp(cur_arg,"/nios=",strlen("/nios="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_INT,
												&pstWriteStreamParams->ulNMaxIOs);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for nios in %s\n",cur_arg);
					return 0;
				}
				bNMaxIOs = 1;
				nargs_parsed++;
			}
			else if(!strncmp(cur_arg,"/miniosz=",strlen("/miniosz="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_MEMORY,
												&pstWriteStreamParams->ulMinIOSz);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for miniosz in %s\n",cur_arg);
					return 0;
				}
				bMinIOSz = 1;
				nargs_parsed++;
			}
			else if(!strncmp(cur_arg,"/maxiosz=",strlen("/maxiosz="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_MEMORY,
												&pstWriteStreamParams->ulMaxIOSz);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for maxiosz in %s\n",cur_arg);
					return 0;
				}
				bMaxIOSz = 1;
				nargs_parsed++;
			}
			else if(!strncmp(cur_arg,"/random",strlen("/random"))) {
				pstWriteStreamParams->Random = 1;
				nargs_parsed++;
			}
			else {
				break;	
			}
		}
		else if(IS_DRIVE_LETTER(cur_arg)){
			pstWriteStreamParams->DriveLetter = *cur_arg;
			bDriveLetter = 1;
			nargs_parsed++;
		}
		else
			break;
	}
	if(!bDriveLetter) {
		printf("Pls specify a DriveLetter to write to\n");
		return 0;
	}
	if(!bNMaxIOs) {
		printf("Pls specify the nios to write \n");
		return 0;
	}
	if(!bMinIOSz) {
		printf("Pls specify the miniosz of the IO to write\n");
		return 0;
	}
	if(!bMaxIOSz) {
		printf("Pls specify the maxiosz of the IO to write\n");
		return 0;
	}

	if(pstWriteStreamParams->ulNMaxIOs == 0) {
		printf("Please let the WriteStream Command do atleast a single IO\n");
		return 0;
	}

	if((pstWriteStreamParams->ulMinIOSz == 0) || (pstWriteStreamParams->ulMaxIOSz == 0) || 
	   (pstWriteStreamParams->ulMinIOSz%TEST_SECTOR_SIZE != 0) || 
	   (pstWriteStreamParams->ulMaxIOSz%TEST_SECTOR_SIZE != 0)) {
		printf("miniosz and maxiosz should be strictly positive and multiple of sector size which is 512\n");
		return 0;
	}

	if(pstWriteStreamParams->ulMinIOSz > pstWriteStreamParams->ulMaxIOSz) {
		printf("miniosz should be lesser than maxiosz\n");
		return 0;
	}

	if(pstWriteStreamParams->ulMaxIOSz > MAX_IO_SIZE) {
		printf("IOs greater than 10mb cant be executed..pls specify a lesser size\n");
		return 0;
	}
	
	return 1;
}

int ParseControlledTestSubCmd()
{
    stWriteOrderTestInfo.ControlledTest = TRUE;

    char *cur_arg;
    PARAM_PARSE_ERROR ret;

    while(nargs_parsed < argc) {
        cur_arg = argv[nargs_parsed];
		if(IS_PARAM(cur_arg)) {
            if(!strncmp(cur_arg,"/tuningstep=",strlen("/tuningstep="))) {
				ret = get_value_from_param_string(cur_arg, PARAM_TYPE_INT, &stWriteOrderTestInfo.DataRateTuningStep);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for drive in %s\n",cur_arg);
					return 0;
				}
                stWriteOrderTestInfo.DataRateTuningStep *= MEGA_BYTES;
				nargs_parsed++;
			}
            else if(!strncmp(cur_arg,"/datathreshold=",strlen("/datathreshold="))) {
				ret = get_value_from_param_string(cur_arg, PARAM_TYPE_INT, &stWriteOrderTestInfo.DataThresholdForVolume);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for drive in %s\n",cur_arg);
					return 0;
				}
                stWriteOrderTestInfo.DataThresholdForVolume *= MEGA_BYTES;
				nargs_parsed++;
			}
			else if(!strncmp(cur_arg,"/datarate=",strlen("/datarate="))) {
				ret = get_value_from_param_string(cur_arg, PARAM_TYPE_INT, &stWriteOrderTestInfo.InputDataRate);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for drive in %s\n",cur_arg);
					return 0;
				}
                stWriteOrderTestInfo.InputDataRate *= MEGA_BYTES;
				nargs_parsed++;
			}
			else
				break;
        }
    }
    return TRUE;
}

int ParseSimulatedTestCmd(PSIMULATED_TEST_CMD pstSimulatedTestCmd) {
	char *cur_arg;
	PARAM_PARSE_ERROR ret;
	
	pstSimulatedTestCmd->ulNWritesPerTag = DEFAULT_NWRITES_PER_TAG;
	pstSimulatedTestCmd->ulNWriteThreads = 0;
	pstSimulatedTestCmd->bDataTest = 0;

	while(nargs_parsed < argc) {
		cur_arg = argv[nargs_parsed];
		if(IS_PARAM(cur_arg)) {
			if(!strncmp(cur_arg,"/nwritespertag=",strlen("/nwritespertag="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_INT,
												&pstSimulatedTestCmd->ulNWritesPerTag);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for nwritespertag in %s\n",cur_arg);
					return 0;
				}
				nargs_parsed++;
			}
			else if(!strncmp(cur_arg,"/datatest=",strlen("/datatest="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_INT,
												&pstSimulatedTestCmd->bDataTest);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for nodatatest in %s\n",cur_arg);
					return 0;
				}
				nargs_parsed++;
			}
			else {
				break;	
			}
		}
		else if(IS_SUB_CMD(cur_arg)) {
			if(!strcmp(cur_arg,"-writestream")) {
				nargs_parsed++;
				if(!ParseWriteStreamSubCmd(&pstSimulatedTestCmd->arstWriteStreamParams[pstSimulatedTestCmd->ulNWriteThreads])) {
					return 0;
				}
				pstSimulatedTestCmd->ulNWriteThreads++;
			}
            else if(!strcmp(cur_arg,"-controlledtest")) {
				nargs_parsed++;
				if(!ParseControlledTestSubCmd()) {
					return 0;
				}
			}
			else {
				break;	
			}
		}
		else {
			break;
		}
	}

	if(pstSimulatedTestCmd->ulNWriteThreads == 0) {
		printf("Atleast one writestream need to be specified\n");
		return 0;
	}

	if(pstSimulatedTestCmd->ulNWritesPerTag <= 0) {
		printf("An invalid value of %d is specified for nWritesPerTag\n",pstSimulatedTestCmd->ulNWritesPerTag);
		return 0;
	}

	return 1;
}

int ParseRapidTestCmd(PRAPID_TEST_CMD pstRapidTestCmd) {
	char *cur_arg;
	ULONG bWriteStream=0;
	
	memset(pstRapidTestCmd,0,sizeof(PRAPID_TEST_CMD));

	while(nargs_parsed < argc) {
		cur_arg = argv[nargs_parsed];
		if(IS_SUB_CMD(cur_arg)) {
			if(!strcmp(cur_arg,"-writestream")) {
				nargs_parsed++;
				if(bWriteStream) {
					printf("Only one WriteStream can be specified for a RapidTest command\n");
					return 0;
				}
				if(!ParseWriteStreamSubCmd(&pstRapidTestCmd->stWriteStreamParams)) {
					return 0;
				}
				bWriteStream = 1;
			}
			else {
				break;	
			}
		}
		else {
			break;
		}
	}

	if(!bWriteStream) {
		printf("Atleast one writestream need to be specified for RapidTest Command\n");
		return 0;
	}

	return 1;
}

int ParseWriteDataCmd(PWRITE_DATA_CMD pstWriteDataCmd) {
	char *cur_arg;
	int bDriveLetter,bOffset,bSize;
	PARAM_PARSE_ERROR ret;

	bDriveLetter = bOffset = bSize = 0;
	pstWriteDataCmd->ulPattern = 1234;
	while(nargs_parsed < argc) {
		cur_arg = argv[nargs_parsed];
		if(IS_PARAM(cur_arg)) {
			if(!strncmp(cur_arg,"/offset=",strlen("/offset="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_MEMORY,
												&pstWriteDataCmd->ulOffset);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for offset in %s\n",cur_arg);
					return 0;
				}
				bOffset = 1;
				nargs_parsed++;
			}
			else if(!strncmp(cur_arg,"/size=",strlen("/size="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_MEMORY,
												&pstWriteDataCmd->ulSize);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for size in %s\n",cur_arg);
					return 0;
				}
				bSize = 1;
				nargs_parsed++;
			}
			else if(!strncmp(cur_arg,"/drive=",strlen("/drive="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_DRIVE_LETTER,
												&pstWriteDataCmd->DriveLetter);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for size in %s\n",cur_arg);
					return 0;
				}
				bDriveLetter = 1;
				nargs_parsed++;
			}
			else if(!strncmp(cur_arg,"/pattern=",strlen("/pattern="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_INT,
												&pstWriteDataCmd->ulPattern);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for pattern in %s\n",cur_arg);
					return 0;
				}
				nargs_parsed++;
			}
			else {
				break;	
			}
		}
		else if(IS_DRIVE_LETTER(cur_arg)){
			pstWriteDataCmd->DriveLetter = *cur_arg;
			bDriveLetter = 1;
			nargs_parsed++;
		}
		else {
			break;
		}
	}
	if(!bDriveLetter) {
		printf("Pls specify a DriveLetter to write to\n");
		return 0;
	}
	if(!bOffset) {
		printf("Pls specify a Offset to write at\n");
		return 0;
	}
	if(!bSize) {
		printf("Pls specify the Size of the IO to write to\n");
		return 0;
	}

	if((pstWriteDataCmd->ulSize == 0) || 
	   (pstWriteDataCmd->ulOffset%TEST_SECTOR_SIZE != 0) || 
	   (pstWriteDataCmd->ulSize%TEST_SECTOR_SIZE != 0)) {
		printf("WriteData:: offset should be non-negative but size should be strictly positive and both should be multiple of sector size which is 512\n");
		return 0;
	}

	if(pstWriteDataCmd->ulSize > MAX_IO_SIZE) {
		printf("IOs greater than 10mb cant be executed..pls specify a lesser size\n");
		return 0;
	}

	return 1;
}

int ParseWriteTagCmd(PWRITE_TAG_CMD pstWriteTagCmd) {
	char *cur_arg;
	int bDriveLetter,bTag;
	char c;
	PARAM_PARSE_ERROR ret;

	bDriveLetter = bTag = 0;
	while(nargs_parsed < argc) {
		cur_arg = argv[nargs_parsed];
		if(IS_PARAM(cur_arg)) {
			if(!strncmp(cur_arg,"/drive=",strlen("/drive="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_DRIVE_LETTER,&c);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for drive in %s\n",cur_arg);
					return 0;
				}
				pstWriteTagCmd->DrivesBitmap |= 1<<(c-'a');
				bDriveLetter = 1;
				nargs_parsed++;
			}
			else if(!strncmp(cur_arg,"/tag=",strlen("/tag="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_STRING,
												&pstWriteTagCmd->TagName,
                                                _countof(pstWriteTagCmd->TagName));
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for tag in %s\n",cur_arg);
					return 0;
				}
				bTag = 1;
				nargs_parsed++;
			}
			else {
				break;	
			}
		}
		else if(IS_DRIVE_LETTER(cur_arg)){
			pstWriteTagCmd->DrivesBitmap |= 1<<((*cur_arg)-'a');
			bDriveLetter = 1;
			nargs_parsed++;
		}
		else {
			break;
		}
	}
	if(!bDriveLetter) {
		printf("Pls specify a DriveLetter to write to\n");
		return 0;
	}
	if(!bTag) {
		printf("Pls specify the Tag\n");
		return 0;
	}
	return 1;
}

int ParsePrintNextDBCmd(PPRINT_NEXT_DB_CMD pstPrintNextDBCmd) {
	char *cur_arg;
	int bDriveLetter;
	PARAM_PARSE_ERROR ret;

	bDriveLetter = 0;
	pstPrintNextDBCmd->bCommit = 0;
	pstPrintNextDBCmd->bPrintDataIOs = 1;
	pstPrintNextDBCmd->bPrintTagIOs = 1;
	pstPrintNextDBCmd->bPrintPatterns = 0;
	while(nargs_parsed < argc) {
		cur_arg = argv[nargs_parsed];
		if(IS_PARAM(cur_arg)) {
			if(!strncmp(cur_arg,"/drive=",strlen("/drive="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_DRIVE_LETTER,
												&pstPrintNextDBCmd->DriveLetter);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for drive in %s\n",cur_arg);
					return 0;
				}
				bDriveLetter = 1;
				nargs_parsed++;
			}
			else if(!strncmp(cur_arg,"/commit=",strlen("/commit="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_INT,
												&pstPrintNextDBCmd->bCommit);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for commit in %s\n",cur_arg);
					return 0;
				}
				nargs_parsed++;
			}
			else if(!strncmp(cur_arg,"/printtagios=",strlen("/printtagios="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_INT,
												&pstPrintNextDBCmd->bPrintTagIOs);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for printtagios in %s\n",cur_arg);
					return 0;
				}
				nargs_parsed++;
			}
			else if(!strncmp(cur_arg,"/printdataios=",strlen("/printdataios="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_INT,
												&pstPrintNextDBCmd->bPrintDataIOs);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for pattern in %s\n",cur_arg);
					return 0;
				}
				nargs_parsed++;
			}
			else if(!strncmp(cur_arg,"/printpatterns=",strlen("/printpatterns="))) {
				ret = get_value_from_param_string(cur_arg,PARAM_TYPE_INT,
												&pstPrintNextDBCmd->bPrintPatterns);
				if(ret != PARAM_CORRECT) {
					printf("Invalid value for pattern in %s\n",cur_arg);
					return 0;
				}
				nargs_parsed++;
			}
			else {
				break;	
			}
		}
		else if(IS_DRIVE_LETTER(cur_arg)){
			pstPrintNextDBCmd->DriveLetter = *cur_arg;
			bDriveLetter = 1;
			nargs_parsed++;
		}
		else {
			break;
		}
	}
	if(!bDriveLetter) {
		printf("Pls specify a DriveLetter to write to\n");
		return 0;
	}
	return 1;
}

int main(int _argc,char *_argv[]) {
	char *cur_arg = NULL;
	int i;
	int ret;
	int error = 0;

	argc = _argc;
	argv = _argv;
	
	if(argc == 1) {
		printf("Please specify one or more commands\n");
		PrintUsage();
		return 0;
	}

	// make all the parameters lower case, as we follow case-insensitivity
	for(i=1;i<argc;i++) {
		cur_arg=argv[i];
		while(*cur_arg!='\0') {
			if((*cur_arg >= 'A') && (*cur_arg <= 'Z')) *cur_arg = (*cur_arg)+'a'-'A';
			cur_arg++;
		}
	}

	nargs_parsed=1;

	while(nargs_parsed<argc) {
		cur_arg = argv[nargs_parsed];
		if(IS_MAIN_CMD(cur_arg)) {
			if(!strcmp(cur_arg,"--sendshutdownnotify")) {
				bSendShutdownNotify = 1;
				nargs_parsed++;
				continue;
			}
			else if(!strcmp(cur_arg,"--sendprocessstartnotify")) {
				bSendProcessStartNotify = 1;
				nargs_parsed++;
				continue;
			}
			else if(!strcmp(cur_arg,"--simulatedtest")) {
				nargs_parsed++;
				arstMainCmdsList[nMainCmds].type = MAIN_CMD_SIMULATED_TEST;
				if(!ParseSimulatedTestCmd(&arstMainCmdsList[nMainCmds].subCmd.stSimulatedTestCmd)) {
					error = 1;
				}
				nMainCmds++;
			}
			else if(!strcmp(cur_arg,"--rapidtest")) {
				nargs_parsed++;
				arstMainCmdsList[nMainCmds].type = MAIN_CMD_RAPID_TEST;
				if(!ParseRapidTestCmd(&arstMainCmdsList[nMainCmds].subCmd.stRapidTestCmd)) {
					error = 1;
				}
				nMainCmds++;
			}
			else if(!strcmp(cur_arg,"--writedata")) {
				nargs_parsed++;
				arstMainCmdsList[nMainCmds].type = MAIN_CMD_WRITE_DATA;
				if(!ParseWriteDataCmd(&arstMainCmdsList[nMainCmds].subCmd.stWriteDataCmd)) {
					error = 1;
				}
				nMainCmds++;
			}
			else if(!strcmp(cur_arg,"--writetag")) {
				nargs_parsed++;
				arstMainCmdsList[nMainCmds].type = MAIN_CMD_WRITE_TAG;
				if(!ParseWriteTagCmd(&arstMainCmdsList[nMainCmds].subCmd.stWriteTagCmd)) {
					error = 1;
				}
				nMainCmds++;
			}
			else if(!strcmp(cur_arg,"--printnextdb")) {
				nargs_parsed++;
				arstMainCmdsList[nMainCmds].type = MAIN_CMD_PRINT_NEXT_DB;
				if(!ParsePrintNextDBCmd(&arstMainCmdsList[nMainCmds].subCmd.stPrintNextDBCmd)) {
					error = 1;
				}
				nMainCmds++;
			}
			else {
				printf("unrecognized option %s\n",cur_arg);
				PrintUsage();
				error = 1;
			}
		}
		else {
			printf("unrecognized option %s\n",cur_arg);
			error = 1;
		}
		if(error)
			break;
	}
	if(error) {
		PrintUsage();
		return 0;
	}
	printf("Commands Parsed succesfully.\n");
	if(nMainCmds > MAX_CMDS) {
		printf("Too many commands specified\n");
		return 0;
	}

	HANDLE hDriver;
	hDriver = CreateFile (INMAGE_FILTER_DOS_DEVICE_NAME,
						  GENERIC_READ | GENERIC_WRITE,
						  FILE_SHARE_READ | FILE_SHARE_WRITE,
						  NULL,
						  OPEN_EXISTING,
						  FILE_FLAG_OVERLAPPED,
						  NULL);
	if( INVALID_HANDLE_VALUE == hDriver) {
            _tprintf(_T("CreateFile %s Failed with Error = %#x\n"), INMAGE_FILTER_DOS_DEVICE_NAME, GetLastError());
            return 0;
    }

	if(bSendShutdownNotify) {
		OVERLAPPED ovlpsn;
		ovlpsn.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
		if(!SendShutdownNotify(hDriver,&ovlpsn)) {
			goto done;
		}
	}

	if(bSendProcessStartNotify) {
		OVERLAPPED ovlpsn;
		ovlpsn.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
		if(!SendProcessStartNotify(hDriver,&ovlpsn)) {
			goto done;
		}
	}

	for(i=0;i<nMainCmds;i++) {
		switch(arstMainCmdsList[i].type) {
			case MAIN_CMD_SIMULATED_TEST:
				ret = SimulatedTest(hDriver,&arstMainCmdsList[i].subCmd.stSimulatedTestCmd);
				break;
			case MAIN_CMD_RAPID_TEST:
				ret = RapidTest(hDriver,&arstMainCmdsList[i].subCmd.stRapidTestCmd);
				break;
			case MAIN_CMD_WRITE_DATA:
				ret = WriteData(hDriver,&arstMainCmdsList[i].subCmd.stWriteDataCmd);
				break;
			case MAIN_CMD_WRITE_TAG:
				ret = WriteTag(hDriver,&arstMainCmdsList[i].subCmd.stWriteTagCmd);
				break;
			case MAIN_CMD_PRINT_NEXT_DB:
				ret = PrintNextDB(hDriver,&arstMainCmdsList[i].subCmd.stPrintNextDBCmd);
				break;
			default:
				printf("Invalid Command Type specified\n");
				break;
		}
		if(ret != COMMAND_SUCCESS) {
			break;
		}
	}
	if(i < nMainCmds) {
		printf("Executing command %i failed due to %d\n",ret,i);
		goto done;
	}

done:
	CancelIo(hDriver);
	CloseHandle(hDriver);
	return 0;
}
