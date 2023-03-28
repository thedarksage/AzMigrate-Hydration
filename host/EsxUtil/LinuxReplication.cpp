#include "LinuxReplication.h"
#include "RollBackVM.h"
#include "svutils.h"
#include "curlwrapper.h"
#include "setpermissions.h"
#include "securityutils.h"
#include "genpassphrase.h"

#include <vector>
#include <algorithm>
#include <boost/lexical_cast.hpp>

using namespace std;

// false if the argument is whitespace, true otherwise
bool not_space(char c) { return !isspace(c); }
// true if the argument is whitespace, false otherwise
bool space(char c) { return isspace(c); }



bool isColon(char c) {return  (c == ':'?1:0);}
bool not_Colon(char c){return (c == ':'?0:1);}

//***************************************************************************************
// Finds the required Child node with given attribute match and returns its pointer
//	Input - Parent Pointer , Value of Child node, Name of Attribute and Value of Attribute
//	Output - Pointer to required Childe node (returns NULL if not found)
//***************************************************************************************
xmlNodePtr xGetChildNodeWithAttr(xmlNodePtr cur, const xmlChar* xmlEntry, const xmlChar* attrName, const xmlChar* attrValue)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	cur = cur->children;
	while(cur != NULL)
	{
		if (!xmlStrcasecmp(cur->name, xmlEntry))
		{
			//Found the required child node. Now check the attribute
			xmlChar *attr_value_temp;
			attr_value_temp = xmlGetProp(cur,attrName);
			if ((attr_value_temp != NULL) && (! xmlStrcasecmp(attr_value_temp,attrValue) ))
			{
				break;
			}
		}
		cur = cur->next;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return cur;
}


//***************************************************************************************
// Finds the required Value for the given property for that node.
//	Input - current node pointer , property name
//	Output - Output value
//***************************************************************************************
bool xGetValueForProp(xmlNodePtr xCur, const string xmlPropertyName, string& attrValue)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool rv = true;
	
	xmlNodePtr xChildNode = xGetChildNodeWithAttr(xCur,(const xmlChar*)"Parameter",(const xmlChar*)"Name", (const xmlChar*)xmlPropertyName.c_str());
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Parameter with required Name=%s> entry not found\n", xmlPropertyName.c_str());
		rv = false;		
	}
	else
	{
		xmlChar *attr_value_temp = xmlGetProp(xChildNode,(const xmlChar*)"Value");
		if (attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Parameter Value> entry not found\n");
			rv = false;
		}
		else
			attrValue = std::string((char *)attr_value_temp);
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return rv;
}


/* Finds the required Child node and returns its pointer
	Input - Parent Pointer , Value of Child node
	Output - Pointer to required Childe node (returns NULL if not found)
*/
xmlNodePtr LinuxReplication::xGetChildNode(xmlNodePtr cur, const xmlChar* xmlEntry)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	cur = cur->children;
	while(cur != NULL)
	{
		if (!xmlStrcmp(cur->name, xmlEntry))
		{
			//Found the required child node
			break;
		}
		cur = cur->next;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return cur;
}

std::string LinuxReplication::getVxInstallPath() const
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return std::string(m_obj.getInstallPath());
}

std::string LinuxReplication:: FetchCxIpAddress()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return GetCxIpAddress();
}

//Find Port number from the host
std::string LinuxReplication::FetchCxHttpPortNumber()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HTTP_CONNECTION_SETTINGS objHttpConSettings = m_obj.getHttp();
    std::ostringstream sin;
    sin<<objHttpConSettings.port;
    std::string cPortNumber = sin.str();    
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return cPortNumber; 
}

//Find Inmage host id
string LinuxReplication::getInMageHostId()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	LocalConfigurator obj;
	string hostid = obj.getHostId();
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hostid;
 
}

string LinuxReplication::GetAccessSignature(const string& strRqstMethod, const string& strAccessFile, const string& strFuncName, const string& strRqstId, const string& strVersion)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string strPassPhrase =  securitylib::getPassphrase();
	string strPassPhraseHash = securitylib::genSha256Mac(strPassPhrase.c_str(), strPassPhrase.size(), true);  //Gets SHA256 mac of passphrase

	string strFingerPrint = GetFingerPrint();
	string strFingerPrintHmac;
	if(!strFingerPrint.empty())
		strFingerPrintHmac = securitylib::genHmac(strPassPhraseHash.c_str(), strPassPhraseHash.size(), strFingerPrint);

	string strStringToSign = strRqstMethod + ":" + strAccessFile + ":" + strFuncName + ":" + strRqstId + ":" + strVersion;
	string strSignHmac = securitylib::genHmac(strPassPhraseHash.c_str(), strPassPhraseHash.size(), strStringToSign);

	string strAccessSignature = securitylib::genHmac(strPassPhraseHash.c_str(), strPassPhraseHash.size(), strFingerPrintHmac + ":" + strSignHmac);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return strAccessSignature;
}

string LinuxReplication::GetFingerPrint()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	//Read fingerprint from file
	string fingerprintFileName(securitylib::getFingerprintDir());
#ifdef WIN32
    fingerprintFileName = fingerprintFileName + "\\" + FetchCxIpAddress() + "_" + FetchCxHttpPortNumber() + ".fingerprint";
#else
	fingerprintFileName = fingerprintFileName + "/" + FetchCxIpAddress() + "_" + FetchCxHttpPortNumber() + ".fingerprint";
#endif
       
    std::string key;
    std::ifstream file(fingerprintFileName.c_str());
    if (file.good()) {
		file >> key;
        boost::algorithm::trim(key);
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return key;
}

string LinuxReplication::GetRequestId()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strRqstId;

	strRqstId = GeneratreUniqueTimeStampString();

	DWORD nRandomNum = rand() % 100000000 + 10000001;
	stringstream ss;
	ss << nRandomNum;
	strRqstId = strRqstId + "_" + ss.str();

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return strRqstId;
}

/*****************************************************************************************
This function is to download the mbrdata_<timestamp> file from CX.
If successfully downloads then returns true, else false.
1st Input: CX URL with path where the file exist.
2nd input: Local path where the file need to be downloaded.
*****************************************************************************************/
bool LinuxReplication::DownloadFileFromCx(string strCxPath, string strLocalPath)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;
	std::string strCxIp = FetchCxIpAddress();
	string CxCmd, strOutputFile;
#ifdef WIN32
	CxCmd = getVxInstallPath() + string("\\cxpsclient.exe -i " ) + strCxIp + string(" --cscreds --get \"") + strCxPath + string("\" -L \"") + strLocalPath + string("\" --secure -c \"") + getVxInstallPath() + string("\\transport\\client.pem\"");
	strOutputFile = m_strDataDirPath + "\\cxpsclient_output";
#else
	CxCmd = getVxInstallPath() + string("/bin/cxpsclient -i " ) + strCxIp + string(" --cscreds --get \"") + strCxPath + string("\" -L \"") + strLocalPath + string("\" --secure -c \"") + getVxInstallPath() + string("/transport/client.pem\"");
	strOutputFile = m_strDataDirPath + "/cxpsclient_output";
#endif

	DebugPrintf(SV_LOG_INFO,"\n");
    DebugPrintf(SV_LOG_INFO,"CX File Download command : %s\n", CxCmd.c_str());


	if(false == ExecuteCmdWithOutputToFileWithPermModes(CxCmd, strOutputFile, O_APPEND | O_RDWR | O_CREAT))
	{
		DebugPrintf(SV_LOG_ERROR,"File download command failed\n");
		bRetStatus = false;
	}

	//Assign Secure permission to the file
	if (bRetStatus)
		AssignSecureFilePermission(strLocalPath);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}


/*****************************************************************************************
This function is to download the mbrdata_<timestamp> file from CX.
If successfully downloads then returns true, else false.
1st Input: Local path from where the file need to upload to CX
2nd input: CX URL with path in which the file need to upload.
*****************************************************************************************/
bool LinuxReplication::UploadFileToCx(string strLocalPath, string strCxPath)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;
	std::string strCxIp = FetchCxIpAddress();
	string CxCmd;
#ifdef WIN32
	CxCmd = string("cxpsclient.exe -i " ) + strCxIp + string(" --cscreds --put \"") + strLocalPath + string("\" -d \"") + strCxPath + string("\" -C --secure -c \"") + string(getVxInstallPath()) + string("\\transport\\client.pem\"");
#else
	CxCmd = string("cxpsclient -i " ) + strCxIp + string(" --cscreds --put \"") + strLocalPath + string("\" -d \"") + strCxPath + string("\" -C --secure -c \"") + string(getVxInstallPath()) + string("/transport/client.pem\"");
#endif

	DebugPrintf(SV_LOG_INFO,"\n");
    DebugPrintf(SV_LOG_INFO,"CX File Upload command : %s\n", CxCmd.c_str());

	RollBackVM objRollBack;
    if(false == objRollBack.CdpcliOperations(CxCmd))
    {
        DebugPrintf(SV_LOG_ERROR,"File Upload command failed\n");
        bRetStatus = false;
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}


bool LinuxReplication::ProcessCxPath()
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	string strInfoFile = m_strCxPath + "/input.txt";
	string strFile, strLineRead, strDir;
	strFile = m_strDataDirPath + string("input.txt");

	do
	{
		if(false == DownloadFileFromCx(strInfoFile, strFile))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to download the %s file from CX", strInfoFile.c_str());
			rv = false;
			break;
		}
		ifstream readFile(strFile.c_str());
		if (!readFile.is_open())
		{
#ifdef WIN32
			DebugPrintf(SV_LOG_ERROR,"Failed to read the File : %s with Error : [%lu].\n",strFile.c_str(),GetLastError());
#else
			DebugPrintf(SV_LOG_ERROR,"Failed to read the File : %s\n",strFile.c_str());
#endif
			rv = false;
			break;
		}
		while (readFile >> strLineRead)
		{
			DebugPrintf(SV_LOG_INFO,"strLineRead =  %s\n",strLineRead.c_str());
			
			strFile = m_strDataDirPath + strLineRead;
			strInfoFile = m_strCxPath + "/" + strLineRead;
			if(false == DownloadFileFromCx(strInfoFile, strFile))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to download the %s file from CX", strInfoFile.c_str());
				rv = false;
				break;
			}
		}
		readFile.close();
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

//Prepares the xml file for CX API call to get the hostinfo for the specified host id
//We can make this Api generic, for this we need to pass all the values as required by storing it in a structure.
//currently all values are hard coded in it.
string LinuxReplication::prepareXmlForGetHostInfo(const string strHostUuid)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string strRqstId = GetRequestId();
	string xml_data = string("<Request Id=\"") + strRqstId + string("\" Version=\"1.0\">");
	xml_data = xml_data + string("<Header>");
	xml_data = xml_data + string("<Authentication>");
	xml_data = xml_data + string("<AuthMethod>ComponentAuth_V2015_01</AuthMethod>");
	xml_data = xml_data + string("<AccessKeyID>") + getInMageHostId() + string("</AccessKeyID>");
	xml_data = xml_data + string("<AccessSignature>") + GetAccessSignature("POST", "/ScoutAPI/CXAPI.php", "GetHostInfo", strRqstId, "1.0") + string("</AccessSignature>");
	xml_data = xml_data + string("</Authentication>");
	xml_data = xml_data + string("</Header>");
	xml_data = xml_data + string("<Body>");
	xml_data = xml_data + string("<FunctionRequest Name=\"GetHostInfo\" Include=\"Yes\">");
	xml_data = xml_data + string("<Parameter Name=\"HostGUID\" Value=\"") + strHostUuid + string("\" />");
	xml_data = xml_data + string("<Parameter Name=\"InformationType\" Value=\"All\" />");
	xml_data = xml_data + string("</FunctionRequest>");
	xml_data = xml_data + string("</Body>");
	xml_data = xml_data + string("</Request>");

	DebugPrintf(SV_LOG_DEBUG, "XML string for gethostinfo: %s\n", xml_data.c_str());
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return xml_data;
}


//Prepares the xml data for CX API call to update the task status
string LinuxReplication::prepareXmlData()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string strRqstId = GetRequestId();
    string xml_data = string("<Request Id=\"") + strRqstId + string("\" Version=\"1.0\">");
	xml_data = xml_data + string("<Header>");
	xml_data = xml_data + string("<Authentication>");
	xml_data = xml_data + string("<AuthMethod>ComponentAuth_V2015_01</AuthMethod>");
	xml_data = xml_data + string("<AccessKeyID>") + getInMageHostId() + string("</AccessKeyID>");
	xml_data = xml_data + string("<AccessSignature>") + GetAccessSignature("POST", "/ScoutAPI/CXAPI.php", m_TaskUpdateInfo.FunctionName, strRqstId, "1.0") + string("</AccessSignature>");
	xml_data = xml_data + string("</Authentication>");
	xml_data = xml_data + string("</Header>");
	xml_data = xml_data + string("<Body>");
	xml_data = xml_data + string("<FunctionRequest Name=\"")+ m_TaskUpdateInfo.FunctionName + string("\" Include=\"N\">");
	xml_data = xml_data + string("<Parameter Name=\"PlanId\" Value=\"") + m_TaskUpdateInfo.PlanId + string("\" />");
	xml_data = xml_data + string("<Parameter Name=\"HostGUID\" Value=\"") + m_TaskUpdateInfo.HostId + string("\" />");

	list<stepInfo_t>::iterator iterStep = m_TaskUpdateInfo.StepInfoList.begin();
	for(; iterStep != m_TaskUpdateInfo.StepInfoList.end(); iterStep++)
	{
		xml_data = xml_data + string("<ParameterGroup Id=\"") + iterStep->StepNum + string("\">");
		xml_data = xml_data + string("<Parameter Name=\"ExecutionStep\" Value=\"") + iterStep->ExecutionStep + string("\" />");
		xml_data = xml_data + string("<Parameter Name=\"Status\" Value=\"") + iterStep->StepStatus + string("\" />");
		xml_data = xml_data + string("<ParameterGroup Id=\"TaskList\">");

		list<taskInfo_t>::iterator iterTask = iterStep->TaskInfoList.begin();
		for(; iterTask != iterStep->TaskInfoList.end(); iterTask++)
		{
			xml_data = xml_data + string("<ParameterGroup Id=\"") + iterTask->TaskNum + string("\">");
			xml_data = xml_data + string("<Parameter Name=\"TaskName\" Value=\"") + iterTask->TaskName + string("\" />");
			xml_data = xml_data + string("<Parameter Name=\"Description\" Value=\"") + iterTask->TaskDesc + string("\" />");
			xml_data = xml_data + string("<Parameter Name=\"TaskStatus\" Value=\"") + iterTask->TaskStatus + string("\" />");
			xml_data = xml_data + string("<Parameter Name=\"ErrorMessage\" Value=\"") + iterTask->TaskErrMsg + string("\" />");
			xml_data = xml_data + string("<Parameter Name=\"FixSteps\" Value=\"") + iterTask->TaskFixSteps +  string("\" />");
			xml_data = xml_data + string("<Parameter Name=\"LogPath\" Value=\"") + iterTask->TaskLogPath +  string("\" />");
			xml_data = xml_data + string("</ParameterGroup>");
		}
		xml_data = xml_data + string("</ParameterGroup>");   // For TaskList
		xml_data = xml_data + string("</ParameterGroup>");   // For Stepid
	}

	xml_data = xml_data + string("</FunctionRequest>");
	xml_data = xml_data + string("</Body>");
	xml_data = xml_data + string("</Request>");

	DebugPrintf(SV_LOG_DEBUG,"Xml Data to Post in CX \n\n%s\n\n",xml_data.c_str());
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return xml_data;
}


/********************************************************************************************************
This function will process the input xml file using cx api and stroes the response in the ouput xml file
input -- chRequestXmlData - Prepared xml data for CX API in earlier call
output -- strOutPutXmlFile - Response xml file of CXAPI call
*********************************************************************************************************/
bool LinuxReplication::processXmlData(string strXmlData, string& strOutPutXmlFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;


	HTTP_CONNECTION_SETTINGS settings = m_obj.getHttp();
	ResponseData_t Data = {0};
	string responsedata;

	curl_global_init(CURL_GLOBAL_ALL);
	
	bool useSecure = (m_strCmdHttps.compare("https://") == 0);

	DebugPrintf(SV_LOG_DEBUG,"Server = %s\n",GetCxIpAddress().c_str());
	DebugPrintf(SV_LOG_DEBUG,"Port = %d\n",settings.port);
	DebugPrintf(SV_LOG_DEBUG,"Secure = %d\n",useSecure);
	DebugPrintf(SV_LOG_DEBUG,"PHP URL = %s\n","/ScoutAPI/CXAPI.php");

	CurlOptions options(settings.ipAddress,settings.port,"/ScoutAPI/CXAPI.php",useSecure);
	options.writeCallback(static_cast<void *>(&Data),WriteCallbackFunction);
    
	Data.chResponseData = NULL; 
	Data.length = 0; /* no data at this point */
	try {
		CurlWrapper cw;
		DebugPrintf(SV_LOG_DEBUG,"Posting the xml data\n");
		cw.post(options, strXmlData);
		DebugPrintf(SV_LOG_DEBUG,"Posted the xml data Successfully\n");
		if( Data.length > 0)
		{
			responsedata = string(Data.chResponseData);
			if(Data.chResponseData != NULL)
				free( Data.chResponseData ) ;
		}
	}  catch(ErrorException& exception )
	{
		DebugPrintf(SV_LOG_ERROR, "FAILED : %s with error %s\n",FUNCTION_NAME, exception.what());
		bResult = false;
	}	

	ofstream outXml(strOutPutXmlFile.c_str(),std::ios::out);
	if(!outXml.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s\n",strOutPutXmlFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	outXml << responsedata << endl;
	outXml.close();

	DebugPrintf(SV_LOG_DEBUG,"Response : %s\n", responsedata.c_str());

	//Assign Secure permission
	AssignSecureFilePermission(strOutPutXmlFile);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

//Modifys the task information for the running task and the task for whcih it will begin
void LinuxReplication::ModifyTaskInfoForCxApi(taskInfo_t currentTask, taskInfo_t nextTask, bool bUpdateProtection)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	list<stepInfo_t>::iterator iterStep = m_TaskUpdateInfo.StepInfoList.begin();
	for(; iterStep != m_TaskUpdateInfo.StepInfoList.end(); iterStep++)
	{
		list<taskInfo_t>::iterator iterTask = iterStep->TaskInfoList.begin();
		for(; iterTask != iterStep->TaskInfoList.end(); iterTask++)
		{
			if(iterTask->TaskNum.compare(currentTask.TaskNum) == 0)
			{
				iterTask->TaskStatus = currentTask.TaskStatus;
				if(!currentTask.TaskErrMsg.empty())
				{
					for(unsigned int i=0; i < currentTask.TaskErrMsg.size(); i++)
					{
						if(currentTask.TaskErrMsg.find("\"") != string::npos)
							currentTask.TaskErrMsg.replace(currentTask.TaskErrMsg.find("\""), 1, "'");
					}
					iterTask->TaskErrMsg = currentTask.TaskErrMsg;
				}
				if(!currentTask.TaskFixSteps.empty())
				{
					for(unsigned int i=0; i < currentTask.TaskFixSteps.size(); i++)
					{
						if(currentTask.TaskFixSteps.find("\"") != string::npos)
							currentTask.TaskFixSteps.replace(currentTask.TaskFixSteps.find("\""), 1, "'");
					}
					iterTask->TaskFixSteps = currentTask.TaskFixSteps;
				}
			}
			else if((iterTask->TaskNum.compare(nextTask.TaskNum) == 0) && !bUpdateProtection)
			{
				iterTask->TaskStatus = nextTask.TaskStatus;
			}
		}
		if(bUpdateProtection)
		{
			if(iterStep->ExecutionStep.compare(nextTask.TaskNum) == 0)
			{
				iterStep->StepStatus = nextTask.TaskStatus;
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

void LinuxReplication::UpdateTaskInfoToCX(bool bCheckResponse)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string xmlCxApiTaskInfo = prepareXmlData();

	string strCxApiResXmlFile;
	if(m_strDataDirPath.empty())
	{
#ifdef WIN32
		strCxApiResXmlFile = "c:\\OutPutCxXml.xml";
#else
		strCxApiResXmlFile = "/home/OutPutCxXml.xml";
#endif
	}
	else
	{
		strCxApiResXmlFile = m_strDataDirPath + "OutPutCxXml.xml";
	}

	if(false == processXmlData(xmlCxApiTaskInfo, strCxApiResXmlFile))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to process the request XML data of CX API call of host : %s\n", m_TaskUpdateInfo.HostId.c_str());
		DebugPrintf(SV_LOG_INFO,"Proceeding further without stopping the work flow for this operation\n");
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

bool LinuxReplication::ExecuteCmdWithOutputToFileWithPermModes(const std::string Command, const std::string OutputFile, int Modes)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    DebugPrintf(SV_LOG_INFO, "Command     = %s\n", Command.c_str());
    DebugPrintf(SV_LOG_INFO, "Output File = %s\n", OutputFile.c_str());

	boost::filesystem::path outputfile(OutputFile);
	boost::filesystem::path outputdir = outputfile.parent_path();

	string strDummyInputFile = outputdir.string() + ACE_DIRECTORY_SEPARATOR_STR_A + string("dummyinput.txt");

    ACE_Process_Options options;
    options.command_line("%s",Command.c_str());

	ACE_Time_Value timeout;
	timeout = ACE_Time_Value::max_time;

	ACE_HANDLE handle = ACE_OS::open(OutputFile.c_str(), Modes, securitylib::defaultFilePermissions(), securitylib::defaultSecurityAttributes());
	ACE_HANDLE handle1 = ACE_OS::open(strDummyInputFile.c_str(), Modes, securitylib::defaultFilePermissions(), securitylib::defaultSecurityAttributes());

	if (handle == ACE_INVALID_HANDLE)
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to create a handle to open the file - %s.\n", OutputFile.c_str());
		rv = false;
	}

	else if (handle1 == ACE_INVALID_HANDLE)
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to create a handle to open the dummy file - %s.\n", strDummyInputFile.c_str());
		rv = false;
	}

	else
	{
		if (options.set_handles(handle1, handle, handle) == -1)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to set handles for STDOUT and STDERR.\n");
			rv = false;
		}
		else
		{
			ACE_Process_Manager* pm = ACE_Process_Manager::instance();
			if (pm == NULL)
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to get process manager instance. Error - [%d - %s].\n", ACE_OS::last_error(), ACE_OS::strerror(ACE_OS::last_error()));
				rv = false;
			}
			else
			{
				pid_t pid = pm->spawn(options);
				if (pid == ACE_INVALID_PID)
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to spawn a process for executing the command. Error - [%d - %s].\n", ACE_OS::last_error(), ACE_OS::strerror(ACE_OS::last_error()));
					rv = false;
				}
				else
				{
					ACE_exitcode status = 0;
					pid_t rc = pm->wait(pid, timeout, &status); // wait for the process to finish
					if (ACE_INVALID_PID == rc)
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to wait for the process. PID: %d.\n", pid);
						DebugPrintf(SV_LOG_ERROR, "Command failed with exit status - %d\n", status);
						rv = false;
					}
					else if (0 == rc)
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to wait for the process. PID: %d. A timeout occurred.\n", pid);
						DebugPrintf(SV_LOG_ERROR, "Command failed to execute, terminating the process %d\n", pid);
						pm->terminate(pid);
						rv = false;
					}
					else if (rc > 0)
					{
						DebugPrintf(SV_LOG_INFO, "Command succeesfully executed having PID: %d.\n", rc);
						rv = true;
					}
				}
			}
			options.release_handles();
		}
	}

	if (handle != ACE_INVALID_HANDLE)
		ACE_OS::close(handle);

	if (handle1 != ACE_INVALID_HANDLE)
		ACE_OS::close(handle1);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

bool LinuxReplication::PauseAllReplicationPairs()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;
	string strStatus;

	//Find the Master target inmage host id
	string strMtHostID = getInMageHostId();
	if(strMtHostID.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to fetch InMage Hot id of Master target.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	map<string, string>::iterator iter = m_mapVmIdToVmName.begin();
	for(; iter != m_mapVmIdToVmName.end(); iter++)
	{
		CxApiInfo cxapi;
		cxapi.strFunction = "PauseReplicationPairs";
		cxapi.strValue = "Yes";
		cxapi.mapParameters.insert(make_pair("SourceHostGUID", iter->first));
		cxapi.mapParameters.insert(make_pair("TargetHostGUID", strMtHostID));
		cxapi.listParamGroup.clear();

		string xmlCxApiTaskInfo = PrepareCXAPIXmlData(cxapi);
		string strCxApiResXmlFile;
		if(m_strDataDirPath.empty())
		{
#ifdef WIN32
				strCxApiResXmlFile = "c:\\OutPutCxXml.xml";
#else
				strCxApiResXmlFile = "/home/OutPutCxXml.xml";
#endif
		}
		else
			strCxApiResXmlFile = m_strDataDirPath + "OutPutCxXml.xml";

		if(false == processXmlData(xmlCxApiTaskInfo, strCxApiResXmlFile))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to process the request XML data of CX API call of host : %s\n", iter->first.c_str());
			DebugPrintf(SV_LOG_INFO,"Proceeding further without stopping the work flow for this operation\n");
			continue;
		}

		string strRequestId;
		if(!ReadResponseXmlFile(strCxApiResXmlFile, "PauseReplicationPairs", strRequestId))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to read the response xml file for host : %s\n", iter->first.c_str());
			bRetStatus = false;
		}
		else
		{
			do
			{
				if(false == GetRequestStatus(strRequestId, strStatus))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed while processing \"GetRequestStatus\" for host : %s\n", iter->first.c_str());
					bRetStatus = false;
					break;
				}

				if(strStatus.compare("success") == 0)
					break;
				else if(strStatus.compare("failed") == 0)
				{
					bRetStatus = false;
					break;
				}
				else if(strStatus.compare("pending") == 0)
#ifdef WIN32
					Sleep(10000);
#else
					sleep(10);
#endif
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Unknown error occured while querying the status for pause the replication for host : %s\n", iter->first.c_str());
					bRetStatus = false;
					break;
				}
			}while(1);

			if(bRetStatus)
				DebugPrintf(SV_LOG_INFO,"successfully completed the pause replication for host : %s\n", iter->first.c_str());
			else
				DebugPrintf(SV_LOG_ERROR,"Failed to pause the replication for host : %s\n", iter->first.c_str());
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}


bool LinuxReplication::ResumeAllReplicationPairs()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;
	string strStatus;

	//Find the Master target inmage host id
	string strMtHostID = getInMageHostId();
	if(strMtHostID.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to fetch InMage Hot id of Master target.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	map<string, string>::iterator iter = m_mapVmIdToVmName.begin();
	for(; iter != m_mapVmIdToVmName.end(); iter++)
	{
		CxApiInfo cxapi;
		cxapi.strFunction = "ResumeReplicationPairs";
		cxapi.strValue = "Yes";
		cxapi.mapParameters.insert(make_pair("SourceHostGUID", iter->first));
		cxapi.mapParameters.insert(make_pair("TargetHostGUID", strMtHostID));
		cxapi.listParamGroup.clear();

		string xmlCxApiTaskInfo = PrepareCXAPIXmlData(cxapi);
		string strCxApiResXmlFile;
		if(m_strDataDirPath.empty())
		{
#ifdef WIN32
				strCxApiResXmlFile = "c:\\OutPutCxXml.xml";
#else
				strCxApiResXmlFile = "/home/OutPutCxXml.xml";
#endif
		}
		else
			strCxApiResXmlFile = m_strDataDirPath + "OutPutCxXml.xml";

		if(false == processXmlData(xmlCxApiTaskInfo, strCxApiResXmlFile))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to process the request XML data of CX API call of host : %s\n", iter->first.c_str());
			DebugPrintf(SV_LOG_INFO,"Proceeding further without stopping the work flow for this operation\n");
			continue;
		}

		if(!ReadResponseXmlFile(strCxApiResXmlFile, "ResumeReplicationPairs", strStatus))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to read the response xml file for host : %s\n", iter->first.c_str());
			bRetStatus = false;
		}
		else
		{
			if(strStatus.compare("success") == 0)
				DebugPrintf(SV_LOG_INFO,"successfully completed the resume replication for host : %s\n", iter->first.c_str());
			else
				DebugPrintf(SV_LOG_ERROR,"Failed to resume the replication for host : %s\n", iter->first.c_str());
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}

bool LinuxReplication::RestartResyncforAllPairs()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;
	string strStatus;

	//Find the Master target inmage host id
	string strMtHostID = getInMageHostId();
	if(strMtHostID.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to fetch InMage Hot id of Master target.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	map<string, string>::iterator iter = m_mapVmIdToVmName.begin();
	for(; iter != m_mapVmIdToVmName.end(); iter++)
	{
		CxApiInfo cxapi;
		cxapi.strFunction = "RestartResyncReplicationPairs";
		cxapi.strValue = "No";
		cxapi.mapParameters.insert(make_pair("ResyncRequired", "Yes"));
		cxapi.mapParameters.insert(make_pair("SourceHostGUID", iter->first));
		cxapi.mapParameters.insert(make_pair("TargetHostGUID", strMtHostID));
		cxapi.listParamGroup.clear();

		string xmlCxApiTaskInfo = PrepareCXAPIXmlData(cxapi);
		string strCxApiResXmlFile;
		if(m_strDataDirPath.empty())
		{
#ifdef WIN32
				strCxApiResXmlFile = "c:\\OutPutCxXml.xml";
#else
				strCxApiResXmlFile = "/home/OutPutCxXml.xml";
#endif
		}
		else
			strCxApiResXmlFile = m_strDataDirPath + "OutPutCxXml.xml";

		if(false == processXmlData(xmlCxApiTaskInfo, strCxApiResXmlFile))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to process the request XML data of CX API call of host : %s\n", iter->first.c_str());
			DebugPrintf(SV_LOG_INFO,"Proceeding further without stopping the work flow for this operation\n");
			continue;
		}

		if(!ReadResponseXmlFile(strCxApiResXmlFile, "RestartResyncReplicationPairs", strStatus))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to read the response xml file for host : %s\n", iter->first.c_str());
			bRetStatus = false;
		}
		else
		{
			if(strStatus.compare("success") == 0)
				DebugPrintf(SV_LOG_INFO,"successfully completed the restart resync for the replications of host : %s\n", iter->first.c_str());
			else
				DebugPrintf(SV_LOG_ERROR,"Failed to restart resync the replications for host : %s\n", iter->first.c_str());
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}


bool LinuxReplication::RemoveReplicationPair(string strTgtHostId, list<string>& lstTgtDevices)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;
	string strStatus;

	CxApiInfo cxapi;
	cxapi.strFunction = "DeletePairs";
	cxapi.strValue = "No";
	cxapi.mapParameters.clear();

	CxApiParamGroup childPrmGrp;
	childPrmGrp.strId = "DeleteList_1";
	int i = 1;
	list<string>::iterator iter = lstTgtDevices.begin();
	for(;iter != lstTgtDevices.end(); iter++, i++)
	{
		string dev;
		stringstream s;
		s << i;
		s >> dev;

		dev = string("Device") + dev;
		childPrmGrp.mapParameters.insert(make_pair(dev, *iter));
	}
	childPrmGrp.mapParameters.insert(make_pair(string("TargetHostGUID"), strTgtHostId));
	childPrmGrp.lstCxApiParam.clear();

	cxapi.listParamGroup.push_back(childPrmGrp);

	string xmlCxApiTaskInfo = PrepareCXAPIXmlData(cxapi);
	string strCxApiResXmlFile;
	if(m_strDataDirPath.empty())
	{
#ifdef WIN32
			strCxApiResXmlFile = "c:\\OutPutCxXml.xml";
#else
			strCxApiResXmlFile = "/home/OutPutCxXml.xml";
#endif
	}
	else
		strCxApiResXmlFile = m_strDataDirPath + "OutPutCxXml.xml";

	if(false == processXmlData(xmlCxApiTaskInfo, strCxApiResXmlFile))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to process the request XML data of CX API call of host\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	if(!ReadResponseXmlFile(strCxApiResXmlFile, "DeletePairs", strStatus))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to read the response xml file\n");
		bRetStatus = false;
	}
	else
	{
		if(strStatus.compare("success") == 0)
			DebugPrintf(SV_LOG_INFO,"successfully completed the deletion of replication pairs\n");
		else
			DebugPrintf(SV_LOG_ERROR,"Failed to delete the replication pairs\n");
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}


bool LinuxReplication::GetRequestStatus(string strRequestId, string& strStatus)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;

	CxApiInfo cxapi;
	cxapi.strFunction = "GetRequestStatus";
	cxapi.strValue = "No";
	cxapi.mapParameters.clear();

	CxApiParamGroup paramgrp;
	paramgrp.strId = "RequestIdList";
	paramgrp.mapParameters.insert(make_pair("RequestId", strRequestId));
	paramgrp.lstCxApiParam.clear();

	cxapi.listParamGroup.push_back(paramgrp);

	string xmlCxApiTaskInfo = PrepareCXAPIXmlData(cxapi);
	string strCxApiResXmlFile;
	if(m_strDataDirPath.empty())
	{
#ifdef WIN32
		strCxApiResXmlFile = "c:\\OutPutCxXml.xml";
#else
		strCxApiResXmlFile = "/home/OutPutCxXml.xml";
#endif
	}
	else
		strCxApiResXmlFile = m_strDataDirPath + "OutPutCxXml.xml";

	if(false == processXmlData(xmlCxApiTaskInfo, strCxApiResXmlFile))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to process the request XML data of CX API \"GetRequestStatus\"\n");
		bRetStatus = false;
	}

	if(!ReadResponseXmlFile(strCxApiResXmlFile, "GetRequestStatus", strStatus))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to read the response xml file\n");
		bRetStatus = false;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"successfully read the response xml file\n");
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}

bool LinuxReplication::GetCommonRecoveryPoint(string strHostName, string& strValue)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetStatus = true;

	//Find the Master target inmage host id
	string strMtHostID = getInMageHostId();
	if(strMtHostID.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to fetch InMage Hot id of Master target.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	CxApiInfo cxapi;
	cxapi.strFunction = "GetCommonRecoveryPoint";
	cxapi.strValue = "No";
	cxapi.mapParameters.insert(make_pair("SourceHostID", strHostName));
	cxapi.mapParameters.insert(make_pair("TargetHostID", strMtHostID));
	cxapi.mapParameters.insert(make_pair("Option", strValue));
	//cxapi.mapParameters.insert(make_pair("Time", ""));   //Need to add logic for Latest_TIME
	cxapi.listParamGroup.clear();

	string xmlCxApiTaskInfo = PrepareCXAPIXmlData(cxapi);
	string strCxApiResXmlFile;
	if(m_strDataDirPath.empty())
	{
#ifdef WIN32
		strCxApiResXmlFile = "c:\\OutPutCxXml.xml";
#else
		strCxApiResXmlFile = "/home/OutPutCxXml.xml";
#endif
	}
	else
		strCxApiResXmlFile = m_strDataDirPath + "OutPutCxXml.xml";

	if(false == processXmlData(xmlCxApiTaskInfo, strCxApiResXmlFile))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to process the request XML data of CX API call of host : %s\n", strHostName.c_str());
		bRetStatus = false;
	}
	else
	{
		if(!ReadResponseXmlFile(strCxApiResXmlFile, "GetCommonRecoveryPoint", strValue))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to read the response xml file for host : %s\n", strHostName.c_str());
			bRetStatus = false;
		}
		else
		{
			if(!strValue.empty())
				DebugPrintf(SV_LOG_INFO,"successfully got the common recovery point from CX for host : %s and Tag : %s\n", strHostName.c_str(), strValue.c_str());
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the common recovery point from CX for host : %s\n", strHostName.c_str());
				bRetStatus = false;
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetStatus;
}


bool LinuxReplication::SendAlertsToCX(map<string, SendAlerts>& sendAlertsInfo)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bRetStatus = true;
	string strStatus;

	CxApiInfo cxapi;
	cxapi.strFunction = "SendAlerts";
	cxapi.strValue = "No";

	map<string, SendAlerts>::iterator iter;
	for (iter = sendAlertsInfo.begin(); iter != sendAlertsInfo.end(); iter++)
	{
		CxApiParamGroup paramgrp, prmgrp;
		paramgrp.strId = iter->first;
		paramgrp.mapParameters = iter->second.mapParameters;

		prmgrp.strId = iter->second.mapParamGrp.begin()->first;
		prmgrp.mapParameters = iter->second.mapParamGrp.begin()->second;

		paramgrp.lstCxApiParam.push_back(prmgrp);
		cxapi.listParamGroup.push_back(paramgrp);
	}

	string xmlCxApiTaskInfo = PrepareCXAPIXmlData(cxapi);
	string strCxApiResXmlFile;
	if (m_strDataDirPath.empty())
	{
#ifdef WIN32
		strCxApiResXmlFile = "c:\\OutPutCxXml.xml";
#else
		strCxApiResXmlFile = "/home/OutPutCxXml.xml";
#endif
	}
	else
		strCxApiResXmlFile = m_strDataDirPath + "OutPutCxXml.xml";

	if (false == processXmlData(xmlCxApiTaskInfo, strCxApiResXmlFile))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to process the request XML data for CX API call \"UpdateStatusToCX\" \n");
		DebugPrintf(SV_LOG_INFO, "Proceeding further without stopping the work flow for this operation\n");
		return false;
	}

	if (!ReadResponseXmlFile(strCxApiResXmlFile, "SendAlerts", strStatus))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to read the response xml file for CX API call \"UpdateStatusToCX\" \n");
		bRetStatus = false;
	}
	else
	{
		if (strStatus.compare("success") == 0)
			DebugPrintf(SV_LOG_INFO, "successfully completed the update status to CX.\n");
		else
			DebugPrintf(SV_LOG_ERROR, "Failed to update the status to CX.\n");
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bRetStatus;
}


void LinuxReplication::UpdateSendAlerts(taskInfo_t& taskInfo, string& strCxPath, const string strPlan, map<string, SendAlerts>& sendAlertsInfo)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	map<string, string> mapGrp;
	SendAlerts sendAlert;
	sendAlert.mapParameters["Category"] = "VCON_ERROR";  //Current default value.
	sendAlert.mapParameters["HostId"] = getInMageHostId();
	mapGrp["XmlFilePath"] = strCxPath;

	string strFailedEventCode, strSuccessEventCode, strSuccessMsg, strFailedMsg;
	if (strPlan.compare("recovery") == 0)
	{
		strFailedEventCode = "VE0704";
		strSuccessEventCode = "VA0602";
		strSuccessMsg = "Successfully completed the recovery operation";
		strFailedMsg = "Recovery failed";
	}
	else if (strPlan.compare("protection") == 0)
	{
		strFailedEventCode = "VE0703";
		strSuccessEventCode = "VA0601";
		strSuccessMsg = "Successfully completed the protection";
		strFailedMsg = "Protection failed";
	}

	if (taskInfo.TaskStatus.compare(INM_VCON_TASK_FAILED) == 0)
	{
		sendAlert.mapParameters["EventCode"] = strFailedEventCode;
		sendAlert.mapParameters["Message"] = strFailedMsg;
        sendAlert.mapParameters["Severity"] = "ERROR";
	}
	else
	{
		sendAlert.mapParameters["EventCode"] = strSuccessEventCode;
		sendAlert.mapParameters["Message"] = strSuccessMsg;
        sendAlert.mapParameters["Severity"] = "NOTICE";
	}
	sendAlert.mapParamGrp["PlaceHolders"] = mapGrp;
	sendAlertsInfo["1"] = sendAlert;

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}


void LinuxReplication::UpdateSendAlerts(map<string, SendAlerts>& mapSendAlertsInfo, const string strPlan)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	int nCounter = 1;
	string strFailedEventCode, strSuccessEventCode, strFailedMsg, strSuccessMsg;
	if (strPlan.compare("recovery") == 0)
	{
		strFailedEventCode = "VE0702";
		strSuccessEventCode = "VA0602";
		strFailedMsg = "Recovery failed for host : ";
		strSuccessMsg = "Successfully completed recovery for host : ";
	}
	else if (strPlan.compare("protection") == 0)
	{
		strFailedEventCode = "VE0701";
		strSuccessEventCode = "VA0601";
		strFailedMsg = "Protection failed for host : ";
		strSuccessMsg = "Successfully completed protection for host : ";
	}

	for (map<string, statusInfo_t>::iterator iter = m_mapHostIdToStatus.begin(); iter != m_mapHostIdToStatus.end(); iter++, nCounter++)
	{
		string strHostName;
		map<string, string>::iterator iterMap = m_mapVmIdToVmName.find(iter->first);
		if (iterMap->second.empty())
			strHostName = iter->first;
		else
			strHostName = iterMap->second;

		map<string, string> mapGrp;
		SendAlerts sendAlertsInfo;
		sendAlertsInfo.mapParameters["Category"] = "VCON_ERROR";  //Current default value.
		sendAlertsInfo.mapParameters["HostId"] = iter->first;
		mapGrp["HostName"] = strHostName;

		if (iter->second.Status.compare(INM_VCON_TASK_FAILED) == 0)
		{
			sendAlertsInfo.mapParameters["EventCode"] = strFailedEventCode;
			sendAlertsInfo.mapParameters["Message"] = strFailedMsg + strHostName;
            sendAlertsInfo.mapParameters["Severity"] = "ERROR";
		}
		else
		{
			sendAlertsInfo.mapParameters["EventCode"] = strSuccessEventCode;
			sendAlertsInfo.mapParameters["Message"] = strSuccessMsg + strHostName;
            sendAlertsInfo.mapParameters["Severity"] = "NOTICE";
		}
		sendAlertsInfo.mapParamGrp["PlaceHolders"] = mapGrp;

		stringstream ss;
		ss << nCounter;
		mapSendAlertsInfo[ss.str()] = sendAlertsInfo;
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}


bool LinuxReplication::ReadResponseXmlFile(string strCxApiResponseFile, string strCxApi, string& strStatus)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;

	string strTagType;
	if(strCxApi.compare("GetCommonRecoveryPoint") == 0)
	{
		strTagType = strStatus;
		strStatus.clear();
	}

	xmlDocPtr xDoc;	
	xmlNodePtr xCur;

	do
	{
		xDoc = xmlParseFile(strCxApiResponseFile.c_str());
		if (xDoc == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
			bResult = false;
			break;
		}

		//Found the doc. Now process it.
		xCur = xmlDocGetRootElement(xDoc);
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is empty. Cannot Proceed further.\n");
			bResult = false;
			break;
		}
		if (xmlStrcasecmp(xCur->name,(const xmlChar*)"Response")) 
		{
			//Response is not found
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Response> entry not found\n");
			bResult = false;
			break;
		}

		xmlChar *attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"Message");
		if(attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to find out the <Response Message=\"\"> from xml file\n");
			bResult = false;
			break;
		}
		strStatus = std::string((char *)attr_value_temp);
		DebugPrintf(SV_LOG_DEBUG,"Status at <Response> : %s\n", strStatus.c_str());

		//If you are here Response entry is found. Go for Body entry.
		xCur = xGetChildNode(xCur,(const xmlChar*)"Body");
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Body> entry not found\n");
			bResult = false;
			break;		
		}
		//If you are here Body entry is found. Go for Function entry with required Function name.
		xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"Function",(const xmlChar*)"Name", (const xmlChar*)(strCxApi.c_str()));
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Function with required Name> entry not found\n");
			bResult = false;
			break;		
		}
		attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"Message");
		if(attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to find out the <Response Message=\"\"> from xml file\n");
			bResult = false;
			break;
		}
		strStatus = std::string((char *)attr_value_temp);
		DebugPrintf(SV_LOG_DEBUG,"Status at <Function> : %s\n", strStatus.c_str());

		xCur = xGetChildNode(xCur,(const xmlChar*)"FunctionResponse");
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <FunctionResponse> entry not found\n");
			bResult = false;
			break;		
		}

		if(strCxApi.compare("PauseReplicationPairs") == 0)
		{
			if(false == xGetValueForProp(xCur, "RequestId", strStatus))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the value for Parameter Name \"RequestId\"\n");
				bResult = false;
				break;
			}
			DebugPrintf(SV_LOG_DEBUG,"RequestId: %s for pause request\n",strStatus.c_str());
		}
		else if(strCxApi.compare("GetCommonRecoveryPoint") == 0)
		{
			if(false == xGetValueForProp(xCur, "CommonTagExists", strStatus))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the value for Parameter Name \"CommonTagExists\"\n");
				bResult = false;
				break;
			}
			DebugPrintf(SV_LOG_DEBUG,"CommonTagExists : %s \n",strStatus.c_str());

			//Converting to lowercase
			for(size_t i = 0; i < strStatus.length(); i++)
				strStatus[i] = tolower(strStatus[i]);

			if(strStatus.compare("true") == 0)
			{
				//Checking for CommonTag
				if(strTagType.compare("LATEST_TAG") == 0)
				{
					if(false == xGetValueForProp(xCur, "TagName", strStatus))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to get the value for Parameter Name \"TagName\"\n");
						bResult = false;
						break;
					}
					DebugPrintf(SV_LOG_DEBUG,"TagName : %s \n",strStatus.c_str());
				}
				//Checking for CommonTime
				else if(strTagType.compare("LATEST_TIME") == 0)
				{
					if(false == xGetValueForProp(xCur, "BeforeTagName", strStatus))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to get the value for Parameter Name \"BeforeTagName\"\n");
						bResult = false;
						break;
					}
					DebugPrintf(SV_LOG_DEBUG,"BeforeTagName : %s \n",strStatus.c_str());
				}
			}
		}
		else
		{
			string strOption = "Status";
			if(strCxApi.compare("ResumeReplicationPairs") == 0)
			{
				strOption = "ResumeStatus";
			}
			else if(strCxApi.compare("RestartResyncReplicationPairs") == 0)
			{
				strOption = "ResyncStatus";
			}
			else if(strCxApi.compare("DeletePairs") == 0)
			{
				strOption = "deleteStatus";
			}
			if(false == xGetValueForProp(xCur, strOption, strStatus))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the value for Paramater Name \"Status\"\n");
				bResult = false;
				break;
			}
			DebugPrintf(SV_LOG_DEBUG,"status: %s \n",strStatus.c_str());
		}
	}while(0);

	if(!bResult)
	{
		if(strCxApi.compare("GetCommonRecoveryPoint") == 0)
			strStatus.clear();
		else
			strStatus = "failed";
		xmlFreeDoc(xDoc);
	}

	if(strCxApi.compare("GetCommonRecoveryPoint") != 0)
	{
		for(size_t i = 0; i < strStatus.length(); i++)
			strStatus[i] = tolower(strStatus[i]);
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


string LinuxReplication::PrepareCXAPIXmlData(CxApiInfo& cxapi)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string strRqstId = GetRequestId();
	string xml_data = string("<Request Id=\"") + strRqstId + string("\" Version=\"1.0\">");
	xml_data = xml_data + string("<Header>");
	xml_data = xml_data + string("<Authentication>");
	xml_data = xml_data + string("<AuthMethod>ComponentAuth_V2015_01</AuthMethod>");
	xml_data = xml_data + string("<AccessKeyID>") + getInMageHostId() + string("</AccessKeyID>");
	xml_data = xml_data + string("<AccessSignature>") + GetAccessSignature("POST", "/ScoutAPI/CXAPI.php", cxapi.strFunction, strRqstId, "1.0") + string("</AccessSignature>");
	xml_data = xml_data + string("</Authentication>");
	xml_data = xml_data + string("</Header>");
	
	xml_data = xml_data + string("<Body>");
	xml_data = xml_data + string("<FunctionRequest Name=\"")+ cxapi.strFunction + string("\" Include=\"") + cxapi.strValue + string("\">");
	map<string, string>::iterator iter = cxapi.mapParameters.begin();
	for(; iter != cxapi.mapParameters.end(); iter++)
	{
		xml_data = xml_data + string("<Parameter Name=\"") + iter->first + string("\" Value=\"") + iter->second + string("\" />");
	}

	if(!cxapi.listParamGroup.empty())
	{
		xml_data = xml_data + ProcessParamGroup(cxapi.listParamGroup);
	}

	xml_data = xml_data + string("</FunctionRequest>");
	xml_data = xml_data + string("</Body>");
	xml_data = xml_data + string("</Request>");

	DebugPrintf(SV_LOG_DEBUG,"Xml Data to Post in CX \n\n%s\n\n",xml_data.c_str());

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return xml_data;
}

string LinuxReplication::ProcessParamGroup(list<CxApiParamGroup>& listParamGroup)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string xml_data;
	
	list<CxApiParamGroup>::iterator iterList = listParamGroup.begin();
	
	for(; iterList != listParamGroup.end(); iterList++)
	{
		xml_data = xml_data + string("<ParameterGroup Id=\"") + iterList->strId + string("\">");
		map<string, string>::iterator iter = iterList->mapParameters.begin();
		for(;iter != iterList->mapParameters.end(); iter++)
		{
			xml_data = xml_data + string("<Parameter Name=\"") + iter->first + string("\" Value=\"") + iter->second + string("\" />");
		}
		if(!iterList->lstCxApiParam.empty())
		{
			xml_data = xml_data + ProcessParamGroup(iterList->lstCxApiParam);
		}
		xml_data = xml_data + string("</ParameterGroup>");
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return xml_data;
}

bool LinuxReplication::readScsiFile()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetvalue = true;
	m_mapVmIdToVmName.clear();
	string strFileName;
#ifdef WIN32
	if(m_strOperation.compare("volumeremove") == 0)
		strFileName = m_strDataDirPath + string("Inmage_volumepairs.txt");
	else
		strFileName = m_strDataDirPath + string("Inmage_scsi_unit_disks.txt");
#else
	strFileName = m_strDataDirPath + string("Inmage_scsi_unit_disks.txt");
#endif
    string strLineRead;
	size_t index;
	string token("!@!@!");
    list<string> listOfScsiIds;
	map<string, string> mapOfTgtScsiIdAndSrcDisk;
	map<string, string> mapOfSrcToTgtVolOrDiskPair;
    ifstream inFile(strFileName.c_str());
    do
    {
        if(!inFile.is_open())
        {
            DebugPrintf(SV_LOG_ERROR,"\nFailed to open File : %s",strFileName.c_str());
            bRetvalue = false;
            break;
        }
        while(inFile >> strLineRead)
        {
			string strVmName, strVmHostId;
			if(m_vConVersion >= 4.0)
			{
				size_t pos1 = strLineRead.find_first_of("(");
				size_t pos2 = strLineRead.find_last_of(")");
				if(pos1 != string::npos)
				{
					strVmHostId = strLineRead.substr(0, pos1);
					if(pos2 != string::npos)
						strVmName = strLineRead.substr(pos1+1, pos2-(pos1+1));
					else
						strVmName = strLineRead.substr(pos1+1);
				}
				else
					strVmHostId = strLineRead;
			}
			else
			{
				strVmName = strLineRead;
				strVmHostId = strVmName;
			}
				
			DebugPrintf(SV_LOG_INFO,"VM Name : %s  And Vm InMage ID: %s\n",strVmName.c_str(), strVmHostId.c_str());
			m_mapVmIdToVmName.insert(make_pair(strVmHostId, strVmName));
			
			if(!m_bVolResize)
			{
				if(inFile >> strLineRead)
				{
					stringstream sstream ;
					sstream << strLineRead;
					int iCount;
					sstream >> iCount;
					for(int i = 0; i < iCount; i++ )
					{
						if(inFile >> strLineRead)
						{
							if(m_vConVersion >= 4.0)
							{
								index = strLineRead.find_first_of(token);
								if(index != std::string::npos)
								{
									string strSrcDisk = strLineRead.substr(0,index);
									string strTgtScsiId = strLineRead.substr(index+(token.length()),strLineRead.length());
									if(m_strOperation.compare("volumeremove") == 0)
									{
										DebugPrintf(SV_LOG_INFO,"Source Volume =  %s  <==>  ",strSrcDisk.c_str());
										DebugPrintf(SV_LOG_INFO,"Target Volume =  %s\n",strTgtScsiId.c_str());
									}
									else
									{
										DebugPrintf(SV_LOG_INFO,"strSrcDisk =  %s  <==>  ",strSrcDisk.c_str());
										DebugPrintf(SV_LOG_INFO,"strTgtScsiId =  %s\n",strTgtScsiId.c_str());
									}
									if ((!strSrcDisk.empty()) && (!strTgtScsiId.empty()))
									{
										if((m_strOperation.compare("volumeremove") == 0) || (m_strOperation.compare("diskremove") == 0))
											mapOfSrcToTgtVolOrDiskPair.insert(make_pair(strSrcDisk, strTgtScsiId));
										else
											mapOfTgtScsiIdAndSrcDisk.insert(make_pair(strTgtScsiId, strSrcDisk));
										listOfScsiIds.push_back(strTgtScsiId);
									}
								}
								else
								{
									DebugPrintf(SV_LOG_ERROR,"Failed to find token in lineread in the file : %s\n",strFileName.c_str());
									bRetvalue = false;
									break;
								}
							}
							else
								listOfScsiIds.push_back(strLineRead);
						}
						else
						{
							inFile.close();
							DebugPrintf(SV_LOG_ERROR,"1. Cant read the disk mapping file\n");
							bRetvalue = false;
							break;
						}
					}
					if(bRetvalue == false)
						break;
				}
				else
				{
					inFile.close();
   					DebugPrintf(SV_LOG_ERROR,"2. Cant read the disk mapping file.\n");
					bRetvalue = false;
					break;
				}
				if(bRetvalue == false)
					break;
				else
				{
					/*BUG BUG BUG:::: after protecting Vm,if user has added say 2 more diska and he wants to protect them also,then this code 
					Will fail.Reason is we are assigning the disks the least alphabet to the fist disk encountered in scsi.txt.for example if 
					say we are protecting a vm having 2 disks say disk 1 and disk 2.then we'll assume that in source side they will have name
					like /dev/sda and /dev/sdb .We are assuming that the order in which scsi Ids are written in the scsi file,disks are in same 				      order in the Soure side. 
					*/
					//FIX: run some exe on source side vms and find their identity ,e.g dev/sda has scsi0:9 controller and unit number
            		//listOfScsiIds.sort();
					if(m_vConVersion >= 4.0)
					{
						if((m_strOperation.compare("volumeremove") == 0) || (m_strOperation.compare("diskremove") == 0))
							m_mapVmToSrcandTgtVolOrDiskPair.insert(make_pair(strVmHostId, mapOfSrcToTgtVolOrDiskPair));
						else
							m_mapVmToTgtScsiIdAndSrcDisk.insert(make_pair(strVmHostId, mapOfTgtScsiIdAndSrcDisk));
					}				
					m_mapOfVmAndScsiIds.insert(make_pair(strVmHostId,listOfScsiIds));
					listOfScsiIds.clear();
					mapOfTgtScsiIdAndSrcDisk.clear();
					mapOfSrcToTgtVolOrDiskPair.clear();
				}
			}
        }
    }while(0);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetvalue;	
}

//Assign permissions to file or directory
//Input - File name and flag for directory
void LinuxReplication::AssignSecureFilePermission(const string& strFile, bool bDir)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	DebugPrintf(SV_LOG_DEBUG, "Assign Permission to : %s\n", strFile.c_str());

	try
	{
		if (bDir)
		{
			if (checkIfDirExists(strFile))
				securitylib::setPermissions(strFile, securitylib::SET_PERMISSIONS_NAME_IS_DIR);
			else
				DebugPrintf(SV_LOG_ERROR, "Dir does not exist : %s\n", strFile.c_str());
		}
		else
		{
			if (checkIfFileExists(strFile))
				securitylib::setPermissions(strFile);
			else
				DebugPrintf(SV_LOG_ERROR, "File does not exist : %s\n", strFile.c_str());
		}
	}
	catch (...)
	{
		DebugPrintf(SV_LOG_ERROR, "[Exception] Failed to set permission : %s\n", strFile.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

bool LinuxReplication::GetHostInfoFromCx(const string& strHostName, string& strGetHostInfoXmlFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetvalue = true;

	string strXmlData = prepareXmlForGetHostInfo(strHostName);

#ifdef WIN32
	strGetHostInfoXmlFile = getVxInstallPath() +  "\\Failover\\Data\\" + strHostName + "_cx_api.xml";
#else
	strGetHostInfoXmlFile = getVxInstallPath() +  "failover_data/" + strHostName + "_cx_api.xml";
#endif
				
	if(!processXmlData(strXmlData, strGetHostInfoXmlFile))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to process the request XML data of CX API call for host : %s\n", strHostName.c_str());
		bRetvalue = false;
	}
	else
		DebugPrintf(SV_LOG_INFO,"Successfully generated GetHostInfo API xml file : %s\n", strGetHostInfoXmlFile.c_str());

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetvalue;
}

bool LinuxReplication::ReadDlmFileFromCxApiXml(const string& strXmlFile, string& strDlmFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetvalue = true;
	
	xmlDocPtr xDoc;	
	xmlNodePtr xCur;

	do
	{
		xDoc = xmlParseFile(strXmlFile.c_str());
		if (xDoc == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
			bRetvalue = false; break;
		}

		//Found the doc. Now process it.
		xCur = xmlDocGetRootElement(xDoc);
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is empty. Cannot Proceed further.\n");
			bRetvalue = false; break;
		}
		if (xmlStrcasecmp(xCur->name,(const xmlChar*)"Response")) 
		{
			//Response is not found
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Response> entry not found\n");
			bRetvalue = false; break;
		}
		//If you are here Response entry is found. Go for Body entry.
		xCur = xGetChildNode(xCur,(const xmlChar*)"Body");
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Body> entry not found\n");
			bRetvalue = false; break;		
		}
		//If you are here Body entry is found. Go for Function entry with required Function name.
		xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"Function",(const xmlChar*)"Name", (const xmlChar*)"GetHostInfo");
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Function with required Name> entry not found\n");
			bRetvalue = false; break;	
		}
		xCur = xGetChildNode(xCur,(const xmlChar*)"FunctionResponse");
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <FunctionResponse> entry not found\n");
			bRetvalue = false; break;		
		}
		
		if(false == xGetValueForProp(xCur, "HostGUID", strDlmFile))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to find out the Host Id from xml file\n");
			bRetvalue = false; break;
		}
		DebugPrintf(SV_LOG_INFO,"InMageHotId from XML : %s\n", strDlmFile.c_str());

		if(false == xGetValueForProp(xCur, "Caption", strDlmFile))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to find out OS Name from xml file\n");
			bRetvalue = false; break;
		}		
		DebugPrintf(SV_LOG_INFO,"OS : %s\n", strDlmFile.c_str());

		if(false == xGetValueForProp(xCur, "MBRPath", strDlmFile))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to find out Dlm file name from xml file\n");
			bRetvalue = false; break;
		}
		DebugPrintf(SV_LOG_INFO,"DLM file : %s\n", strDlmFile.c_str());
	}while(0);

	xmlFreeDoc(xDoc);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetvalue;
}


void LinuxReplication::UpdateSrcHostStaus()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	for (map<string, string>::iterator iter = m_mapVmIdToVmName.begin(); iter != m_mapVmIdToVmName.end(); iter++)
	{
		statusInfo_t statusInfo;
		statusInfo.HostId = iter->first;
		statusInfo.Status = "0";   // indicates initiated
		statusInfo.ErrorMsg = "";
		statusInfo.ExecutionStep = "";
		statusInfo.Resolution = "";

		m_mapHostIdToStatus[iter->first] = statusInfo;
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}

void LinuxReplication::UpdateSrcHostStaus(taskInfo_t & taskInfo)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	list<stepInfo_t>::iterator iterStep = m_TaskUpdateInfo.StepInfoList.begin();
	for (; iterStep != m_TaskUpdateInfo.StepInfoList.end(); iterStep++)
	{
		list<taskInfo_t>::iterator iterTask = iterStep->TaskInfoList.begin();
		for (; iterTask != iterStep->TaskInfoList.end(); iterTask++)
		{
			if (iterTask->TaskNum.compare(taskInfo.TaskNum) == 0)
			{
				if (!taskInfo.TaskErrMsg.empty())
				{
					for (int i = 0; i < taskInfo.TaskErrMsg.size(); i++)
					{
						if (taskInfo.TaskErrMsg.find("\"") != string::npos)
							taskInfo.TaskErrMsg.replace(taskInfo.TaskErrMsg.find("\""), 1, "'");
					}
				}
				for (map<string, statusInfo_t>::iterator iter = m_mapHostIdToStatus.begin(); iter != m_mapHostIdToStatus.end(); iter++)
				{
					iter->second.ErrorMsg = iter->second.ErrorMsg + "-@!@!@-" + taskInfo.TaskErrMsg;
					iter->second.ExecutionStep = iterTask->TaskName;
					iter->second.Status = taskInfo.TaskStatus;
				}
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}


void LinuxReplication::UpdateSrcHostStaus(taskInfo_t & taskInfo,const string& strHostId)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	list<stepInfo_t>::iterator iterStep = m_TaskUpdateInfo.StepInfoList.begin();
	for (; iterStep != m_TaskUpdateInfo.StepInfoList.end(); iterStep++)
	{
		list<taskInfo_t>::iterator iterTask = iterStep->TaskInfoList.begin();
		for (; iterTask != iterStep->TaskInfoList.end(); iterTask++)
		{
			if (iterTask->TaskNum.compare(taskInfo.TaskNum) == 0)
			{
				if (!taskInfo.TaskErrMsg.empty())
				{
					for (int i = 0; i < taskInfo.TaskErrMsg.size(); i++)
					{
						if (taskInfo.TaskErrMsg.find("\"") != string::npos)
							taskInfo.TaskErrMsg.replace(taskInfo.TaskErrMsg.find("\""), 1, "'");
					}
				}
				map<string, statusInfo_t>::iterator iter = m_mapHostIdToStatus.find(strHostId);
				if (iter != m_mapHostIdToStatus.end())
				{
					iter->second.ErrorMsg = taskInfo.TaskErrMsg;
					iter->second.ExecutionStep = iterTask->TaskName;
					iter->second.Status = taskInfo.TaskStatus;
				}
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}

bool LinuxReplication::UpdateRecoveryStatusInFile(string strResultFile, statusInfo_t& statInfoOveral, bool bUpdateOveralStatus)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bReturn = true;

	VMsRecoveryUpdateInfo vmsRecoveryInfo;
	if (bUpdateOveralStatus)
	{
		vmsRecoveryInfo.m_updateProps["RecoveryStatus"] = statInfoOveral.Status;
		vmsRecoveryInfo.m_updateProps["ErrorMessage"] = statInfoOveral.ErrorMsg;
		vmsRecoveryInfo.m_updateProps["ErrorCode"] = statInfoOveral.ErrorCode;

		PlaceHolder placeHolder;
		placeHolder.m_Props = statInfoOveral.PlaceHolder.Properties;
		placeHolder.m_Props["ErrorCode"] = statInfoOveral.ErrorCode;
		vmsRecoveryInfo.m_PlaceHolderProps["PlaceHolder"] = placeHolder;
	}
	else
	{
		bool bUpdateFail = false;		
		map<string, statusInfo_t>::iterator iter = m_mapHostIdToStatus.begin();
		for (; iter != m_mapHostIdToStatus.end(); iter++)
		{
			RecoveredVMInfo vmRecoveryInfo;
			vmRecoveryInfo.m_Props["RecoveryStatus"] = iter->second.Status;
			vmRecoveryInfo.m_Props["ErrorMessage"] = iter->second.ErrorMsg;
			vmRecoveryInfo.m_Props["ErrorCode"] = iter->second.ErrorCode;
			vmRecoveryInfo.m_Props["Tag"] = iter->second.Tag;

			PlaceHolder placeHolder;
			placeHolder.m_Props = iter->second.PlaceHolder.Properties;
			placeHolder.m_Props["ErrorCode"] = iter->second.ErrorCode;
			vmRecoveryInfo.m_PlaceHolderProps["PlaceHolder"] = placeHolder;
			vmsRecoveryInfo.m_MapRecoveredVMs[iter->first] = vmRecoveryInfo;

			if (iter->second.Status.compare("2") == 0)
				bUpdateFail = true;
		}
		if (bUpdateFail)
			vmsRecoveryInfo.m_updateProps["ErrorMessage"] = "Failover failed.";
	}

	if (!strResultFile.empty())
	{
		ofstream out(strResultFile.c_str());
		if (out.good())
		{
			try
			{
				out << CxArgObj<VMsRecoveryUpdateInfo>(vmsRecoveryInfo);
			}
			catch (std::exception &e)
			{
				DebugPrintf(SV_LOG_ERROR, "Not able to serialize Vms recovery information. Exception: %s\n", e.what());
				bReturn = false;
			}
			catch (...)
			{
				DebugPrintf(SV_LOG_ERROR, "Not able to serialize Vms recovery information. Unknown Exception\n");
				bReturn = false;
			}
			out.close();
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to open file for reading. File : %s\n", strResultFile.c_str());
			bReturn = false;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "Result file path is missing.");
		bReturn = false;
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bReturn;
}

#ifdef WIN32
// This func will generate unique time stamp string
std::string LinuxReplication::GeneratreUniqueTimeStampString()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strUniqueTimeStampString;
	SYSTEMTIME  lt;
    GetLocalTime(&lt);
	string strTemp;

	strTemp = boost::lexical_cast<string>(lt.wYear);
	strUniqueTimeStampString += strTemp;
	strTemp.clear();
	
    strTemp = boost::lexical_cast<std::string>(lt.wMonth);
    if(strTemp.size() == 1)
        strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	strTemp = boost::lexical_cast<std::string>(lt.wDay);
    if(strTemp.size() == 1)
        strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	strTemp = boost::lexical_cast<std::string>(lt.wHour);
    if(strTemp.size() == 1)
        strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	strTemp = boost::lexical_cast<std::string>(lt.wMinute) ;
    if(strTemp.size() == 1)
        strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	strTemp = boost::lexical_cast<std::string>(lt.wSecond) ;
    if(strTemp.size() == 1)
        strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	strTemp = boost::lexical_cast<std::string>(lt.wMilliseconds);
    if(strTemp.size() == 1)
        strTemp = string("00") + strTemp;
    else if(strTemp.size() == 2)
        strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

    DebugPrintf(SV_LOG_DEBUG,"strUniqueTimeStampString = %s\n",strUniqueTimeStampString.c_str());
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return strUniqueTimeStampString;
}

//Check if File exists and return true
bool LinuxReplication::checkIfFileExists(string strFileName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool blnReturn = true;
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	hFind = FindFirstFile(strFileName.c_str(), &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE) 
	{
		DebugPrintf(SV_LOG_INFO,"Failed to find the file : %s\n",strFileName.c_str());
		DebugPrintf(SV_LOG_INFO,"Invalid File Handle.GetLastError reports [Error %lu ]\n",GetLastError());
		blnReturn = false;
	} 
	else 
	{
		DebugPrintf(SV_LOG_INFO,"Found the file : %s\n",strFileName.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return blnReturn;
}

//Check if Dir exists and return true
bool LinuxReplication::checkIfDirExists(string strDirName)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool blnReturn = false;

	DWORD type = GetFileAttributes(strDirName.c_str());
	if (type != INVALID_FILE_ATTRIBUTES)
	{
		if (type & FILE_ATTRIBUTE_DIRECTORY)
			blnReturn = true;
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return blnReturn;
}

//Hostname of local machine
string LinuxReplication::GetHostName()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
	string strHostName = "";
	char currentHost[MAX_PATH];
	//Find the current host 
	if(gethostname(currentHost, sizeof(currentHost )) == SOCKET_ERROR)
	{
		DebugPrintf(SV_LOG_ERROR,"GethostnameFailed Failed.Error Code %0X\n",WSAGetLastError());		
	}
	else
		strHostName = string(currentHost);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return strHostName;
}

#else

//CreateInitrdImgForOS : check if initrd image needs to be created for this OS passed.
bool LinuxReplication::CreateInitrdImgForOS( std::string strOsName )
{
	DebugPrintf( SV_LOG_DEBUG , "Entering function %s.\n" , __FUNCTION__ );
	bool bReturnValue	= false;
	DebugPrintf(SV_LOG_INFO, "OS Name = %s.\n" , strOsName.c_str());
	std::map<std::string,std::list<std::string> >::iterator iter_OsType = m_InitrdOsMap.begin();
	while( iter_OsType != m_InitrdOsMap.end() )
	{
		if ( bReturnValue )
		{
			break;
		}
		DebugPrintf(SV_LOG_INFO, "OS Type = %s.\n" , iter_OsType->first.c_str() );
		if ( string::npos != strOsName.find( iter_OsType->first ) )
		{
			//Now check the release under this.
			std::list<std::string>::iterator iter_OsRelease = iter_OsType->second.begin();
			while( iter_OsType->second.end() != iter_OsRelease )
			{
				DebugPrintf(SV_LOG_INFO, "Release = %s.\n" , std::string(iter_OsRelease->data()).c_str() );
				if ( string::npos != strOsName.find( iter_OsRelease->data() ) )
				{
					DebugPrintf(SV_LOG_INFO, "Generating initrd image for OS %s.\n", strOsName.c_str());
					bReturnValue= true;
					break;
				}
				iter_OsRelease++;
			}
		}
		iter_OsType++;
	}
	DebugPrintf( SV_LOG_DEBUG , "Exiting function %s.\n" , __FUNCTION__ );
	return bReturnValue;
}

//Parse the ESX.xml and fetch the value for Addition of disk from it.
//Output - Update flag for addition of disk. Default is "true".
//Only if the entry is found and if set to false, this flag will be false
// and "true" for all remaining cases.
//Sample xml -
//  <root CX_IP="10.0.79.50" AdditionOfDisk="true">
//  ..........
//  </root>
void LinuxReplication::UpdateAoDFlag()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    

    m_bIsAdditionOfDisk = true;//default true to avoid overwriting previous entries.
    
    do
    {
	    xmlDocPtr xDoc;	
	    xmlNodePtr xCur;	

	    xDoc = xmlParseFile(m_strXmlFile.c_str());
	    if (xDoc == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
            break;
	    }

	    //Found the doc. Now process it.
	    xCur = xmlDocGetRootElement(xDoc);
	    if (xCur == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"ESX.xml is empty. Cannot Proceed further.\n");
            break;
	    }

	    if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	    {
		    //root is not found
		    DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <root> entry not found\n");
            break;
	    }

	    //If you are here root entry is found. Go for AdditionOfDisk which is an attribute in root.
	    xmlChar *xAodFlag;
        xAodFlag = xmlGetProp(xCur,(const xmlChar*)"AdditionOfDisk");
        if(NULL == xAodFlag)
        {
		    DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <AdditionOfDisk> entry not found\n");
            break;
        }
        else
        {
            if (0 == xmlStrcasecmp(xAodFlag,(const xmlChar*)"false")) 
	        {
                m_bIsAdditionOfDisk = false;
                break;
            }            
        }        
    }while(0);

    DebugPrintf(SV_LOG_INFO,"Addition of Disk = %d.\n",m_bIsAdditionOfDisk);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

bool LinuxReplication::swapFile(std::string strCurFile, std::string strSwapFile, std::string strTempFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	int result;
	result = rename( strCurFile.c_str() , strTempFile.c_str() );
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strCurFile.c_str(), strTempFile.c_str());
		bResult = false;
	}
	result = rename( strSwapFile.c_str() , strCurFile.c_str());
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strSwapFile.c_str(), strCurFile.c_str());	
		bResult = false;
	}
	
	result = rename( strTempFile.c_str() , strSwapFile.c_str() );
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strTempFile.c_str(), strSwapFile.c_str());
		bResult = false;
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool LinuxReplication::restartLinuxService(std::string strService)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	if(false == stopLinuxService(strService))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to stop the Service: %s \n", strService.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	if(false == startLinuxService(strService))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to start the Service: %s \n", strService.c_str());
		bResult = false;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool LinuxReplication::startLinuxService(std::string strService)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	std::stringstream results;
	
	std::string startSvcCmd = "service " + strService + " start" + string(" 1>>/var/log/inmage_mount.log 2>>/var/log/inmage_mount.log");
	DebugPrintf(SV_LOG_INFO, "Start Service Command: %s \n", startSvcCmd.c_str());
	
	string cmd1 = string("echo running command : ") + startSvcCmd + string(" >>/var/log/inmage_mount.log");
	if (!executePipe(cmd1, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
	}
	
	results.clear();
	if(!executePipe(startSvcCmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to execute cmd %s \n", startSvcCmd.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO, "Successfully Started the Service: %s \n", strService.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool LinuxReplication::stopLinuxService(std::string strService)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	std::stringstream results;
	
	std::string stopSvcCmd = "service " + strService + " stop" + string(" 1>>/var/log/inmage_mount.log 2>>/var/log/inmage_mount.log");
	DebugPrintf(SV_LOG_DEBUG, "Stop Service Command: %s \n", stopSvcCmd.c_str());
	
	string cmd1 = string("echo running command : ") + stopSvcCmd + string(" >>/var/log/inmage_mount.log");
	if (!executePipe(cmd1, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
	}
	
	results.clear();
	if(!executePipe(stopSvcCmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to execute cmd %s \n", stopSvcCmd.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO, "Successfully Stoped the Service: %s \n", strService.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool LinuxReplication::restartVxAgent()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	RollBackVM objRollback;
	if(false ==  objRollback.stopVxServices("svagents"))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to stop vxagent.Exiting..");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	if(false == objRollback.startVxServices("svagents"))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to start vxagent.Exiting..");
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}
std::vector<std::string>LinuxReplication::parseCsvLine(const std::string &strLineRead)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    typedef string::const_iterator iter;
    vector<string> vectorOfStringTokens;
    iter i =  strLineRead.begin();
    while (i != strLineRead.end())
    {
        // ignore leading commas
        i = std::find_if(i, strLineRead.end(), not_space);
        // find end of next word
        iter j = std::find_if(i, strLineRead.end(),space);
        // copy the characters in [i, j)
        if (i != strLineRead.end()) 
            vectorOfStringTokens.push_back(string(i, j));
        i = j;
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return vectorOfStringTokens;
}


char LinuxReplication::posix_device_names[60][10] =  {"sda","sdb","sdc","sdd","sde","sdf","sdg","sdh","sdi","sdj","sdk","sdl","sdm","sdn"\
	,"sdo","sdp","sdq","sdr","sds","sdt","sdu","sdv","sdw","sdx","sdy","sdz","sdaa","sdab","sdac","sdad","sdae",\
	"sdaf","sdag","sdah","sdai","sdaj","sdak","sdal","sdam","sdan"\
	,"sdao","sdap","sdaq","sdar","sdas","sdat","sdau","sdav","sdaw","sdax","sday","sdaz","sdba",\
	"sdbb","sdbc","sdbd","sdbe","sdbf","sdbg","sdbh"};

std::map<std::string,std::string> LinuxReplication:: FetchProtectedSrcVmsAddress()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	std::map<std::string ,std::string> mapOfVMsAndTheirIpAddresses;
	std::string strfilename;

#ifdef WIN32
    strfilename = std::string(getVxInstallPath()) + std::string("\\Failover") + std::string("\\data\\") + std::string("SrcsVmAddresFile.txt");
#else
    strfilename = std::string(getVxInstallPath()) + std::string("/failover_data/") + std::string("SrcsVmAddresFile.txt");
#endif
	std::string strLineRead;
	std::string strProtectedVmName;
	std::string strIpAddress;
	ifstream inPutFile(strfilename.c_str());
	if(!inPutFile.is_open())
	{
		//std::cout<<"Failed to open "<<strfilename<<"for reading"<<std::endl;
		DebugPrintf(SV_LOG_ERROR,"Failed to open %s for reading\n",strfilename.c_str());
		mapOfVMsAndTheirIpAddresses.clear();
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return mapOfVMsAndTheirIpAddresses;
	}

	while(inPutFile>>strLineRead)
	{
		std::size_t index = strLineRead.find_last_of("=");
		if(index == std::string::npos)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to fetch the Ip address from %s \n",strfilename.c_str());
			mapOfVMsAndTheirIpAddresses.clear();
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
     		return mapOfVMsAndTheirIpAddresses;

		}
		strProtectedVmName = strLineRead.substr(0,index-1);
		strIpAddress = strLineRead.substr(index+1,strLineRead.length());
		mapOfVMsAndTheirIpAddresses.insert(make_pair(strProtectedVmName,strIpAddress));
		strProtectedVmName.clear();
		strIpAddress.clear();
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return mapOfVMsAndTheirIpAddresses;
}

void LinuxReplication::addAndRemoveScsiEntry()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	std::list<std::string> listOfPhysicaldeviceNames;
	std::map<std::string,std::list<std::string> >::iterator iter = m_mapOfVmAndScsiIds.begin();
	std::map<std::string,std::string>::iterator mapIter;
	mapIter = m_mapLsScsiID.begin();
	std::list<std::string>::iterator listIter;
	
	while(iter != m_mapOfVmAndScsiIds.end())
	{
		std::string strVmName;
		listIter = iter->second.begin();
		while(listIter != iter->second.end())
		{
			DebugPrintf(SV_LOG_INFO,"Trying to look for %s inside Map\n",(*listIter).c_str());
			//Getting this map from lscsi map scsiIds,physical device name
			mapIter = m_mapLsScsiID.find(*listIter);
			if(mapIter == m_mapLsScsiID.end())
			{
				DebugPrintf(SV_LOG_ERROR,"SCSI ID %s is not present in the SCSI IDs provided by Master target.\n",(*listIter).c_str());
				listIter++;
				continue;
			}
			removeLsscsiId(mapIter->second);
			addLsscsiId(mapIter->second);
			listIter++;
		}
		iter++;
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

bool LinuxReplication::removeLsscsiId(string strLsscsiID)
{
	bool bResult = true;	
	std::stringstream results;
	string strScript = string("echo \"scsi remove-single-device ") + strLsscsiID + string("\" > /proc/scsi/scsi");
	DebugPrintf(SV_LOG_DEBUG,"Lscsi ID removing command: %s\n", strScript.c_str());
	
	if(!executePipe(strScript, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to execute cmd %s \n", strScript.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Successfully removed scsi Id: %s \n", strLsscsiID.c_str());
		sleep(5);
	}
	return bResult;
}

bool LinuxReplication::addLsscsiId(string strLsscsiID)
{
	bool bRetValue = true;
	string strScript = string("echo \"scsi add-single-device ") + strLsscsiID + string("\" > /proc/scsi/scsi");
	DebugPrintf(SV_LOG_DEBUG,"Lscsi ID adding command: %s\n", strScript.c_str());
	std::stringstream results;
    if(!executePipe(strScript, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to execute cmd %s \n", strScript.c_str());
		bRetValue = false;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Successfully Added scsi Id: %s \n", strLsscsiID.c_str());
		sleep(5);
	}
	return  bRetValue;
}

bool LinuxReplication::mapSrcDisksInMt()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bFalse = true;
	std::list<std::string> listOfPhysicaldeviceNames;
	std::map<std::string,std::list<std::string> >::iterator iter = m_mapOfVmAndScsiIds.begin();
	std::map<std::string, std::map<std::string, std::string> >::iterator iterDisk = m_mapVmToTgtScsiIdAndSrcDisk.begin();
	std::map<std::string,std::string>::iterator mapIter;
	mapIter = m_mapOfScsiCont.begin();
	while(mapIter != m_mapOfScsiCont.end())
	{
		DebugPrintf(SV_LOG_INFO,"mapIter->first =%s mapIter->second =%s\n",mapIter->first.c_str(),mapIter->second.c_str());
		mapIter++;
	}
	std::list<std::string>::iterator listIter;
	//getting this map from txt file <vm_name and List<scsiids>
	while(iter != m_mapOfVmAndScsiIds.end())
	{
		std::string strVmName;
		listIter = iter->second.begin();
		while(listIter != iter->second.end())
		{
			DebugPrintf(SV_LOG_INFO,"Trying to look for %s inside Map: \n",(*listIter).c_str());
			//Getting this map from lscsi map scsiIds,physical device name
			mapIter = m_mapOfScsiCont.find(*listIter);
			if(mapIter == m_mapOfScsiCont.end())
			{
				DebugPrintf(SV_LOG_ERROR,"SCSI ID %s is not present in the SCSI IDs provided by Master target.\n",(*listIter).c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			DebugPrintf(SV_LOG_INFO,"Found index. It is : = %s\n",mapIter->second.c_str());
			//U found the scsi controller number .Now find the corresponding Physical device and push it.
			listOfPhysicaldeviceNames.push_back(mapIter->second);
			listIter++;
		}
		if(listOfPhysicaldeviceNames.empty())
		{
			DebugPrintf(SV_LOG_ERROR,"No physical device found corresponding to %s.\n",iter->first.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		if(true == checkIfFileExists("/etc/multipath.conf"))
		{
			std::list<std::string> listOfMultiPathNames;
			if(false == convertDeviceIntoMultiPath(listOfPhysicaldeviceNames, listOfMultiPathNames))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to Convert the Physical device names into multipath name \n");
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			if(false == editMultiPathFile())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to Edit the \"/etc/multipath.conf\" file. \n");
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			if(false == reloadMultipathDevice())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to reload multipath devices\n");
			}
			
			strVmName = iter->first;
			m_mapOfVmsAndPhysicalDevices.insert(make_pair(strVmName,listOfMultiPathNames));
			listOfMultiPathNames.clear();
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Unable to get the file \"/etc/multipath.conf\".\n"
							"\tSo Considering the device name instead of Device mapper path for pair creation.\n");
			strVmName = iter->first;
			m_mapOfVmsAndPhysicalDevices.insert(make_pair(strVmName,listOfPhysicaldeviceNames));			
		}
		listOfPhysicaldeviceNames.clear();
		iter++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bFalse;
}
bool LinuxReplication::createSrcTgtDeviceMapping()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	unsigned int iCount = 0;
	std::string strSrcPhysicaldevice;
	std::map<std::string,std::string> mapOfSrcTgtPhysicalDevices;
	std::list<std::string> listOfTgtPhysicalDevices;
	std::list<std::string>::iterator iterList;
	std::map<std::string,std::list<std::string> >::iterator iterMap = m_mapOfVmsAndPhysicalDevices.begin();
	while(iterMap != m_mapOfVmsAndPhysicalDevices.end())
	{
		listOfTgtPhysicalDevices = iterMap->second;
		iterList = listOfTgtPhysicalDevices.begin();
		while(iterList != listOfTgtPhysicalDevices.end())
		{
			strSrcPhysicaldevice = GetDiskNameForNumber(iCount);
			mapOfSrcTgtPhysicalDevices.insert(make_pair(*iterList,strSrcPhysicaldevice));
			iCount++;
			iterList++;
		}
		iCount = 0;
		if(mapOfSrcTgtPhysicalDevices.empty())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the mapping between source and target disks.\n ");
			bResult = false;
			break;
		}
		m_mapOfSrcTgtPhysicaldevices.insert(make_pair(iterMap->first,mapOfSrcTgtPhysicalDevices));
		mapOfSrcTgtPhysicalDevices.clear();
		iterMap++;
	}
	std::map<std::string,std::map<std::string,std::string> >::iterator it = m_mapOfSrcTgtPhysicaldevices.begin();
	while(it != m_mapOfSrcTgtPhysicaldevices.end())
	{
		DebugPrintf(SV_LOG_INFO,"%s\n ",it->first.c_str());
		std::map<std::string,std::string> mp =it->second;
		std::map<std::string,std::string>::iterator pop = mp.begin();
		while(pop != mp.end())
		{
			cout<<"tgt --- >"<<pop->first<<"     "<<pop->second<<endl;
            RescanTargetDisk(pop->first); //since first is target disk
			//DebugPrintf(SV_LOG_INFO,"\ntgt --- >%s    %s\n "pop->first.c_str(),pop->second.c_str());
			pop++;
		}
		it++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


/**********************************************************************************************
This Function will create the mapping of VM to Source and Target devices
This will take the mapping of target scsi-id to source device as one reference.
And it compares with the map m_mapOfScsiCont to get the map of Target to source device mapping
***********************************************************************************************/
bool LinuxReplication::cretaeMapOfVmToSrcAndTgtDevice()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	std::map<std::string, std::map<std::string, std::string> >::iterator iter = m_mapVmToTgtScsiIdAndSrcDisk.begin();
	std::map<std::string, std::string> mapSrcDiskToTgtDisk;
	std::map<std::string, std::string> mapTgtMultipathToSrcDisk;
	std::map<std::string,std::string>::iterator mapIter;
	mapIter = m_mapOfScsiCont.begin();
	while(mapIter != m_mapOfScsiCont.end())
	{
		DebugPrintf(SV_LOG_INFO,"mapIter->first =%s mapIter->second =%s\n",mapIter->first.c_str(),mapIter->second.c_str());
		mapIter++;
	}
	
	for(; iter != m_mapVmToTgtScsiIdAndSrcDisk.end(); iter++ )
	{
		std::map<std::string, std::string>::iterator iterDisk = iter->second.begin();
		for(;iterDisk != iter->second.end(); iterDisk++)
		{
			DebugPrintf(SV_LOG_INFO,"Trying to look for %s inside Map: \n",iterDisk->first.c_str());
			//Getting this map from lscsi map scsiIds,physical device name
			mapIter = m_mapOfScsiCont.find(iterDisk->first);
			if(mapIter == m_mapOfScsiCont.end())
			{
				DebugPrintf(SV_LOG_ERROR,"SCSI ID %s is not present in the SCSI IDs provided by Master target.\n",iterDisk->first.c_str());
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			DebugPrintf(SV_LOG_INFO,"Found index. It is : = %s\n",mapIter->second.c_str());
			mapSrcDiskToTgtDisk.insert(make_pair(iterDisk->second, mapIter->second));
		}
		if(false == createMapperPathMap(mapSrcDiskToTgtDisk, mapTgtMultipathToSrcDisk))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to Convert the Physical device names into multipath name \n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		m_mapOfSrcTgtPhysicaldevices.insert(make_pair(iter->first,mapTgtMultipathToSrcDisk));
		mapSrcDiskToTgtDisk.clear();
		mapTgtMultipathToSrcDisk.clear();
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

// Run the following on target disk to rescan it.
//      echo 1 > /sys/block/sdc/device/rescan
// TgtDisk will be of format /dev/sdc ... so first convert it to sdc and then run the above command.
void LinuxReplication::RescanTargetDisk(std::string TgtDisk)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    string Disk = TgtDisk.substr(5); // since /dev/ counts to 5
    string Cmd = string("echo 1 > /sys/block/") + Disk + string("/device/rescan");
    string CmdOpFile = "/tmp/tmpfile.txt";

    DebugPrintf(SV_LOG_INFO, "Command - %s\n",Cmd.c_str());
    if(false == ExecuteCmdWithOutputToFile(Cmd, CmdOpFile))
    {
        // Not erroring out for now. As this may not be necessary always.
        DebugPrintf(SV_LOG_ERROR, "Failed to rescan the disk - %s\n",Disk.c_str());
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

bool LinuxReplication::rescanScsiBus()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;
	string strScript;
 	struct dirent *dp;
	std::list<std::string> listHost;
    std::string strDirPath = string("/sys/class/scsi_host");
    DIR *dir = opendir(strDirPath.c_str());
    while ((dp=readdir(dir)) != NULL)
    {
        printf("%s\n",dp->d_name);
        string strDir = string(dp->d_name);
        if((strDir == ".") || (strDir == ".."))
            continue;
		DebugPrintf(SV_LOG_INFO, "%s\n", strDir.c_str());
        listHost.push_back(strDir);
    }
    closedir(dir);
    list<string>::iterator iter = listHost.begin();
    while(iter != listHost.end())
    {
		string cmd = string("chmod +x ") + strDirPath + string ("/") + *iter + string ("/") +  string("scan");
		if(false == executeScript(cmd))
        {
            DebugPrintf(SV_LOG_ERROR,"Unable to change the scan command mode to executable.\n");
        }
        strScript = string("echo \"- - -\" >") + strDirPath + string ("/") + *iter + string ("/") +  string("scan");
		DebugPrintf(SV_LOG_INFO,"Disk Scanning Script: %s\n", strScript.c_str());
        if(false == executeScript(strScript))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to rescan the disks.\n");
            bRetValue = false;
            break;
        }
        iter++;
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;

}
bool LinuxReplication::executeScript(const std::string& script)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    int ret = system(script.c_str());
    if (WIFSIGNALED(ret) )
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to execute the rescan command. Error code = %d\n",ret);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    else
    {
        DebugPrintf(SV_LOG_INFO,"Successfully executed command....\n ");
        sleep(10);
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

//It initialises the snapshot task update structure with default values for linux
void LinuxReplication::InitialiseSnapshotTaskUpdate()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	stepInfo_t objStepInfo;
	taskInfo_t objTaskInfo;
	list<taskInfo_t> taskInfoList;
	list<stepInfo_t> stepInfoList;

	objTaskInfo.TaskNum			= INM_VCON_TASK_1;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_1;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_1_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_INPROGRESS;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_2;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_2;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_2_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_3;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_3;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_3_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_4;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_5;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_5_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_5;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_6;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_6_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_6;
	objTaskInfo.TaskName		= INM_VCON_SNAPSHOT_TASK_7;
	objTaskInfo.TaskDesc		= INM_VCON_SNAPSHOT_TASK_7_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objStepInfo.StepNum			= "Step1";
	objStepInfo.ExecutionStep	= m_strOperation;
	objStepInfo.StepStatus		= INM_VCON_TASK_INPROGRESS;
	objStepInfo.TaskInfoList	= taskInfoList;
	stepInfoList.push_back(objStepInfo);

	m_TaskUpdateInfo.FunctionName	= "UpdateESXProtectionDetails";
	m_TaskUpdateInfo.HostId			= getInMageHostId();
	m_TaskUpdateInfo.PlanId			= m_strPlanId;
	m_TaskUpdateInfo.StepInfoList	= stepInfoList;

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

//It initialises the protection task update structure with default values
void LinuxReplication::InitialiseTaskUpdate()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	stepInfo_t objStepInfo;
	taskInfo_t objTaskInfo;
	list<taskInfo_t> taskInfoList;
	list<stepInfo_t> stepInfoList;

	objTaskInfo.TaskNum			= INM_VCON_TASK_1;
	objTaskInfo.TaskName		= INM_VCON_PROTECTION_TASK_1;
	objTaskInfo.TaskDesc		= INM_VCON_PROTECTION_TASK_1_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_INPROGRESS;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_2;
	objTaskInfo.TaskName		= INM_VCON_PROTECTION_TASK_2;
	objTaskInfo.TaskDesc		= INM_VCON_PROTECTION_TASK_2_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_3;
	objTaskInfo.TaskName		= INM_VCON_PROTECTION_TASK_3;
	objTaskInfo.TaskDesc		= INM_VCON_PROTECTION_TASK_3_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_4;
	objTaskInfo.TaskName		= INM_VCON_PROTECTION_TASK_4;
	objTaskInfo.TaskDesc		= INM_VCON_PROTECTION_TASK_4_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objStepInfo.StepNum			= "Step1";
	objStepInfo.ExecutionStep	= m_strOperation;
	objStepInfo.StepStatus		= INM_VCON_TASK_INPROGRESS;
	objStepInfo.TaskInfoList	= taskInfoList;
	stepInfoList.push_back(objStepInfo);

	m_TaskUpdateInfo.FunctionName	= "UpdateESXProtectionDetails";
	m_TaskUpdateInfo.HostId			= getInMageHostId();
	m_TaskUpdateInfo.PlanId			= m_strPlanId;
	m_TaskUpdateInfo.StepInfoList	= stepInfoList;

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

void LinuxReplication::InitialiseVolResizeTaskUpdate()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	stepInfo_t objStepInfo;
	taskInfo_t objTaskInfo;
	list<taskInfo_t> taskInfoList;
	list<stepInfo_t> stepInfoList;

	objTaskInfo.TaskNum			= INM_VCON_TASK_1;
	objTaskInfo.TaskName		= INM_VCON_VOL_RESIZE_TASK_1;
	objTaskInfo.TaskDesc		= INM_VCON_VOL_RESIZE_TASK_1_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_INPROGRESS;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_2;
	objTaskInfo.TaskName		= INM_VCON_VOL_RESIZE_TASK_2;
	objTaskInfo.TaskDesc		= INM_VCON_VOL_RESIZE_TASK_2_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_3;
	objTaskInfo.TaskName		= INM_VCON_VOL_RESIZE_TASK_3;
	objTaskInfo.TaskDesc		= INM_VCON_VOL_RESIZE_TASK_3_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_4;
	objTaskInfo.TaskName		= INM_VCON_VOL_RESIZE_TASK_4;
	objTaskInfo.TaskDesc		= INM_VCON_VOL_RESIZE_TASK_4_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objTaskInfo.TaskNum			= INM_VCON_TASK_5;
	objTaskInfo.TaskName		= INM_VCON_VOL_RESIZE_TASK_5;
	objTaskInfo.TaskDesc		= INM_VCON_VOL_RESIZE_TASK_5_DESC;
	objTaskInfo.TaskStatus		= INM_VCON_TASK_QUEUED;
	objTaskInfo.TaskErrMsg		= "";
	objTaskInfo.TaskFixSteps	= "";
	objTaskInfo.TaskLogPath     = m_strCxPath + "/EsxUtil.log";
	taskInfoList.push_back(objTaskInfo);

	objStepInfo.StepNum			= "Step1";
	objStepInfo.ExecutionStep	= m_strOperation;
	objStepInfo.StepStatus		= INM_VCON_TASK_INPROGRESS;
	objStepInfo.TaskInfoList	= taskInfoList;
	stepInfoList.push_back(objStepInfo);

	m_TaskUpdateInfo.FunctionName	= "UpdateESXProtectionDetails";
	m_TaskUpdateInfo.HostId			= getInMageHostId();
	m_TaskUpdateInfo.PlanId			= m_strPlanId;
	m_TaskUpdateInfo.StepInfoList	= stepInfoList;

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

bool LinuxReplication::createDir(string Dir)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;

	stringstream results;
	string cmd = "mkdir " + Dir;
	DebugPrintf(SV_LOG_INFO,"cmd = : %s\n", cmd.c_str());
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to create directory: %s\n",cmd.c_str());
		bResult = false;
	}
	else
	{
		string commandresult = results.str();
		trim(commandresult);
		if(!commandresult.empty())
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to create directory: %s got Error: %s\n", Dir.c_str(), commandresult.c_str());
			bResult = false;;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Successfully created directory: %s\n", Dir.c_str());
			bResult = true;
		}
		commandresult.clear();
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool LinuxReplication::initLinuxTarget()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	taskInfo_t currentTask, nextTask;
	m_bUpdateStep = false;
    bool bUpdateToCXbyMTid = false;

	if(m_strLogDir.empty())
		m_strDataDirPath = std::string(getVxInstallPath()) +  std::string("failover_data");
	else
		m_strDataDirPath = m_strLogDir;

	if(false == createDir(m_strDataDirPath))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to create Directory - %s\n", m_strDataDirPath.c_str());
	}
	AssignSecureFilePermission(m_strDataDirPath, true);
	m_strDataDirPath = m_strDataDirPath + "/";

	GetDiskUsage();
	GetMemoryInfo();

	if(m_strOperation.compare("volumeresize") == 0)
	{
		m_bVolResize = true;
	}
	
	if(m_bPhySnapShot)
		m_strXmlFile = m_strDataDirPath + std::string(SNAPSHOT_FILE_NAME);
	else if(m_bVolResize)
		m_strXmlFile = m_strDataDirPath + std::string(VOL_RESIZE_FILE_NAME);
	else
		m_strXmlFile = m_strDataDirPath + std::string(VX_JOB_FILE_NAME);

	if(m_bPhySnapShot)
	{
		string snapshotfile = m_strDataDirPath + std::string(SNAPSHOT_VM_FILE_NAME);
		if(true == checkIfFileExists(snapshotfile))
		{
			DebugPrintf(SV_LOG_INFO,"File %s already exists, existing from Dr-Drill opeartion\n", snapshotfile.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return true;
		}
	}

	//Initially Filling the CX update structure and updating the status as queued to CX.
	if(m_bPhySnapShot)
		InitialiseSnapshotTaskUpdate();
	else if(m_bVolResize)
		InitialiseVolResizeTaskUpdate();
	else
		InitialiseTaskUpdate();
	UpdateTaskInfoToCX();
    
    m_vecVxJobObj.clear(); //vector will store information of all Vx pairs to be set.

	//Updating Task1 as completed and Task2 as inprogress
	currentTask.TaskNum = INM_VCON_TASK_1;
	currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
	nextTask.TaskNum = INM_VCON_TASK_2;
	nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
	ModifyTaskInfoForCxApi(currentTask, nextTask);
	UpdateTaskInfoToCX();

	std::string strCxIpAddress;
	std::string strCxUrl;
	const int operation = 0;
	bool retValue = true;
	do
	{
		if(m_strCxPath.empty())
		{
			DebugPrintf(SV_LOG_INFO,"[WARNING]CX Path is not provided to download the protection related files.\n continue with normal way\n");
		}
		else
		{
			if(false == ProcessCxPath()) //This will download all the required files from the given CX Path.
			{
				currentTask.TaskNum = INM_VCON_TASK_2;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Failed to download the required files from the given CX path. For extended Error information download EsxUtil.log in vCon Wiz";
				currentTask.TaskFixSteps = "Check the existance CX path and the required files inside that one. Rerun the job again after specifying the correct details.";
				nextTask.TaskNum = INM_VCON_TASK_3;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
                bUpdateToCXbyMTid = true;

				DebugPrintf(SV_LOG_ERROR,"[ERROR]Failed to download the required files from the given CX path %s\n", m_strCxPath.c_str());
				DebugPrintf(SV_LOG_ERROR,"Can't Proceed further, Stopping the Protection/Snapshot operation.\n");
				retValue = false;
			    break;
			}
		}

		if(false == xCheckXmlFileExists())
		{
			currentTask.TaskNum = INM_VCON_TASK_2;
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			currentTask.TaskErrMsg = "Failed while checking for the xml file existance in MT. For extended Error information download EsxUtil.log in vCon Wiz";
			currentTask.TaskFixSteps = "Check the the files existance in MT, if not then place it in the required folder and Rerun the job ";
			nextTask.TaskNum = INM_VCON_TASK_3;
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
			bUpdateToCXbyMTid = true;

			DebugPrintf(SV_LOG_ERROR,"Failed to find the XML document with all the Virtual Machine info\n");
			retValue = false;
			break;
		}

		if(false == xGetvConVersion())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the vCon version from Esx.xml/Resize.xml file.\n");
			m_vConVersion = 1.2;
		}

		if(!m_bV2P)
		{
			if(false == readScsiFile())
			{
				currentTask.TaskNum = INM_VCON_TASK_2;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Failed while reading the file Inmage_scsi_unit_disks.txt. For extended Error information download EsxUtil.log in vCon Wiz";
				currentTask.TaskFixSteps = "Check the the file existance and access permission for the file and Rerun the job ";
				nextTask.TaskNum = INM_VCON_TASK_3;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
				bUpdateToCXbyMTid = true;

				DebugPrintf(SV_LOG_ERROR,"Cannot proceed further for linux protection\n");
				retValue = false;
				break;
			}
		}

		UpdateSrcHostStaus();
		
		if(false == reloadMultipathDevice())
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to reload the multipath devices\n");
		}
		
		if(false == rescanScsiBus())
		{
			currentTask.TaskNum = INM_VCON_TASK_2;
			currentTask.TaskStatus = INM_VCON_TASK_FAILED;
			currentTask.TaskErrMsg = "Failed to rescan Scsi bus for any new disks. For extended Error information download EsxUtil.log in vCon Wiz";
			currentTask.TaskFixSteps = "Check the the lsscsi scsi service running properly or not and Rerun the job ";
			nextTask.TaskNum = INM_VCON_TASK_3;
			nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

			DebugPrintf(SV_LOG_ERROR,"Failed to rescan Scsi bus for any new disks.\n");
			retValue = false;
			break;
		}

		currentTask.TaskNum = INM_VCON_TASK_2;
		currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
		nextTask.TaskNum = INM_VCON_TASK_3;
		nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
		ModifyTaskInfoForCxApi(currentTask, nextTask);
		UpdateTaskInfoToCX();
		
		if(!m_bV2P && !m_bVolResize)
		{
			m_mapOfScsiCont.clear();
			if(false == parseLsscsiOutput())
			{
				currentTask.TaskNum = INM_VCON_TASK_3;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Failed to parse the Lsscsi output. lsscsi rpm might not have installed in MT";
				currentTask.TaskFixSteps = "Check the lsscsi service is runnign or not and Rerun the job ";
				nextTask.TaskNum = INM_VCON_TASK_4;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

				DebugPrintf(SV_LOG_ERROR,"Failed to parse the Lsscsi output.\n");
				retValue = false;
				break;
			}
			
			//Remove and adding Lsscsi ID's
			addAndRemoveScsiEntry();
			
			m_mapOfScsiCont.clear();
			if(false == parseLsscsiOutput())
			{
				currentTask.TaskNum = INM_VCON_TASK_3;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Failed to parse the Lsscsi output. lsscsi rpm might not have installed in MT";
				currentTask.TaskFixSteps = "Check the lsscsi service is running or not and Rerun the job ";
				nextTask.TaskNum = INM_VCON_TASK_4;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

				DebugPrintf(SV_LOG_ERROR,"Failed to parse the Lsscsi output.\n");
				retValue = false;
				break;
			}

			if(false == reloadMultipathDevice())
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to reload multipath devices\n");
			}

			if(m_vConVersion >= 4.0)
			{
				if(false == cretaeMapOfVmToSrcAndTgtDevice())
				{
					currentTask.TaskNum = INM_VCON_TASK_3;
					currentTask.TaskStatus = INM_VCON_TASK_FAILED;
					currentTask.TaskErrMsg = "Failed to create mapping between source and target disks with respect to VM. For extended Error information download EsxUtil.log in vCon Wiz";
					currentTask.TaskFixSteps = string("Check the the given files parsed successfully or not and check the lsscsi ouput is proper or not and Rerun the job,") +
													string("if issue persists contact inmage customer support");
					nextTask.TaskNum = INM_VCON_TASK_4;
					nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

					DebugPrintf(SV_LOG_ERROR,"Failed to create mapping between source and target disks with respect to VM. \n");
					retValue = false;
					break;
				}
			}
			else
			{
				if(false == mapSrcDisksInMt())
				{
					currentTask.TaskNum = INM_VCON_TASK_3;
					currentTask.TaskStatus = INM_VCON_TASK_FAILED;
					currentTask.TaskErrMsg = "Failed to find mapping between source vm's scsi numbers and master target's physical devices. For extended Error information download EsxUtil.log in vCon Wiz";
					currentTask.TaskFixSteps = string("Check the the given files parsed successfully or not and check the lsscsi ouput is proper or not and Rerun the job,") +
													string("if issue persists contact inmage customer support");
					nextTask.TaskNum = INM_VCON_TASK_4;
					nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

					DebugPrintf(SV_LOG_ERROR,"Failed to find mapping between source vm's scsi numbers and master target's physical devices.\n");
					retValue = false;
					break;	
				}
				if(false == createSrcTgtDeviceMapping())
				{
					currentTask.TaskNum = INM_VCON_TASK_3;
					currentTask.TaskStatus = INM_VCON_TASK_FAILED;
					currentTask.TaskErrMsg = "Failed to create mapping between source and target disks. For extended Error information download EsxUtil.log in vCon Wiz";
					currentTask.TaskFixSteps = string("Check the the given files parsed successfully or not and check the lsscsi ouput is proper or not and Rerun the job,") +
													string("if issue persists contact inmage customer support");
					nextTask.TaskNum = INM_VCON_TASK_4;
					nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

					DebugPrintf(SV_LOG_ERROR,"Failed to create mapping between source and target disks. \n");
					retValue = false;
					break;
				}
			}
		}
		else if(m_bVolResize)
		{
			if(false == PauseAllReplicationPairs())
			{
				DebugPrintf(SV_LOG_ERROR,"PauseAllReplicationPairs() failed.\n");
				currentTask.TaskNum = INM_VCON_TASK_3;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Pausing the replication pairs for soem of the VM's Failed";
				currentTask.TaskFixSteps = "";
				nextTask.TaskNum = INM_VCON_TASK_4;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
				
				DebugPrintf(SV_LOG_ERROR,"Failed while pasuing the replcation pairs. Cannot proceed!!!\n");
				retValue = false;
				break;
			}

			currentTask.TaskNum = INM_VCON_TASK_3;
			currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
			nextTask.TaskNum = INM_VCON_TASK_4;
			nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
			ModifyTaskInfoForCxApi(currentTask, nextTask);
			UpdateTaskInfoToCX();

			do
			{
				m_mapOfScsiCont.clear();
				if(false == parseLsscsiOutput())
				{
					currentTask.TaskNum = INM_VCON_TASK_4;
					currentTask.TaskStatus = INM_VCON_TASK_FAILED;
					currentTask.TaskErrMsg = "Failed to parse the Lsscsi output. For extended Error information download EsxUtil.log in vCon Wiz";
					currentTask.TaskFixSteps = "Check the lsscsi service is runnign or not and Rerun the job ";
					nextTask.TaskNum = INM_VCON_TASK_5;
					nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

					DebugPrintf(SV_LOG_ERROR,"Failed to parse the Lsscsi output.\n");
					retValue = false;
					break;
				}

				map<string, string> mapOfMultipathToScsiId;
				if(false == getMapOfMultipathToScsiId(mapOfMultipathToScsiId))
				{
					currentTask.TaskNum = INM_VCON_TASK_4;
					currentTask.TaskStatus = INM_VCON_TASK_FAILED;
					currentTask.TaskErrMsg = "Failed to find the multipath device to scsi id mapping. For extended Error information download EsxUtil.log in vCon Wiz";
					currentTask.TaskFixSteps = "Check the lsscsi and multipath service is running or not and rerun the job ";
					nextTask.TaskNum = INM_VCON_TASK_5;
					nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

					DebugPrintf(SV_LOG_ERROR,"Failed to find the multipath device to scsi id mapping.\n");
					retValue = false;
					break;
				}
				if(false == reloadDevices(mapOfMultipathToScsiId))
				{
					currentTask.TaskNum = INM_VCON_TASK_4;
					currentTask.TaskStatus = INM_VCON_TASK_FAILED;
					currentTask.TaskErrMsg = "Failed to reload the target devices. For extended Error information download EsxUtil.log in vCon Wiz";
					currentTask.TaskFixSteps = "";
					nextTask.TaskNum = INM_VCON_TASK_5;
					nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

					DebugPrintf(SV_LOG_ERROR,"Failed to reload the target devices.\n");
					retValue = false;
					break;
				}

				currentTask.TaskNum = INM_VCON_TASK_4;
				currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
				nextTask.TaskNum = INM_VCON_TASK_5;
				nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
				ModifyTaskInfoForCxApi(currentTask, nextTask);
				UpdateTaskInfoToCX();
			}while(0);
			
			if(false == RegisterHostUsingCdpCli())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to Register the Master Target disks using cdpcdli\n");
			}
			if(false == ResumeAllReplicationPairs())
			{
				m_bUpdateStep = true;
				currentTask.TaskNum = INM_VCON_TASK_5;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Resuming the replication pairs for soem of the VM's Failed";
				currentTask.TaskFixSteps = "Resume all the pairs which are in paused state from CX UI";
				nextTask.TaskNum = m_strOperation;
				nextTask.TaskStatus = INM_VCON_TASK_FAILED;
				
				DebugPrintf(SV_LOG_ERROR,"Failed while resuming the replcation pairs. Cannot proceed!!!\n");
				retValue = false;
			}
			if(retValue)
			{
				if(false == RestartResyncforAllPairs())
				{
					m_bUpdateStep = true;
					currentTask.TaskNum = INM_VCON_TASK_5;
					currentTask.TaskStatus = INM_VCON_TASK_FAILED;
					currentTask.TaskErrMsg = "Restart resync failed for the resized disks";
					currentTask.TaskFixSteps = "Manually set the resync flag in CXUI for the resized disk pairs.";
					nextTask.TaskNum = m_strOperation;
					nextTask.TaskStatus = INM_VCON_TASK_FAILED;

					DebugPrintf(SV_LOG_ERROR,"Failed to restart resync the disk resized pairs!!!\n");
					retValue = false;
				}
				else
				{
					currentTask.TaskNum = INM_VCON_TASK_5;
					currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
					nextTask.TaskNum = m_strOperation;
					nextTask.TaskStatus = INM_VCON_TASK_COMPLETED;
					ModifyTaskInfoForCxApi(currentTask, nextTask, true);
					UpdateTaskInfoToCX();

					DebugPrintf(SV_LOG_INFO,"Successfully completed all the disk resize tasks.\n");
					retValue = true;
					
				}
			}
			break;
		}
		else
		{
			if(false == SrcTgtDiskMapFromScsiFileV2P())
			{
				currentTask.TaskNum = INM_VCON_TASK_3;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = "Failed while reading the file Inmage_scsi_unit_disks.txt. For extended Error information download EsxUtil.log in vCon Wiz";
				currentTask.TaskFixSteps = "Check the the file existance and access permission for the file and Rerun the job ";
				nextTask.TaskNum = INM_VCON_TASK_4;
				nextTask.TaskStatus = INM_VCON_TASK_QUEUED;

				DebugPrintf(SV_LOG_ERROR,"Cannot proceed with CxCli \n");
				retValue = false;
				break;
			}
		}

		currentTask.TaskNum = INM_VCON_TASK_3;
		currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
		nextTask.TaskNum = INM_VCON_TASK_4;
		nextTask.TaskStatus = INM_VCON_TASK_INPROGRESS;
		ModifyTaskInfoForCxApi(currentTask, nextTask);
		UpdateTaskInfoToCX();

		std::string strMasterTgtIpAddress;
		if(!m_bPhySnapShot)
		{
			if(m_vConVersion >= 4.0)
			{
				strMasterTgtIpAddress = FetchInMageHostId();
				if(strMasterTgtIpAddress.empty())
				{
					m_bUpdateStep = true;
					currentTask.TaskNum = INM_VCON_TASK_4;
					currentTask.TaskStatus = INM_VCON_TASK_FAILED;
					currentTask.TaskErrMsg = "Failed to fetch the Master Target InMage HostId. For extended Error information download EsxUtil.log in vCon Wiz";
					currentTask.TaskFixSteps = string("Check the vxagent registerd in CX or not, rrestart the vxagent and Rerun the job") +
													string("if issue persists contact inmage customer support");
					nextTask.TaskNum = m_strOperation;
					nextTask.TaskStatus = INM_VCON_TASK_FAILED;

					DebugPrintf(SV_LOG_ERROR,"Failed to fetch the Master Target InMage HostId\n");
					retValue = false;
					break;
				}
			}
			else
			{			
				strMasterTgtIpAddress = FetchLocalIpAddress();
				if(strMasterTgtIpAddress.empty())
				{
					m_bUpdateStep = true;
					currentTask.TaskNum = INM_VCON_TASK_4;
					currentTask.TaskStatus = INM_VCON_TASK_FAILED;
					currentTask.TaskErrMsg = "Failed to fetch the Master Target IP Address. For extended Error information download EsxUtil.log in vCon Wiz";
					currentTask.TaskFixSteps = string("Check the ipaddress in of MT, if everythign is fine then Rerun the job") +
													string("if issue persists contact inmage customer support");
					nextTask.TaskNum = m_strOperation;
					nextTask.TaskStatus = INM_VCON_TASK_FAILED;

					DebugPrintf(SV_LOG_ERROR,"Failed to fetch the Master Target IP Address.\n");
					retValue = false;
					break;
				}
			}
		}		
		
		if(true == checkIfFileExists("/etc/lvm/lvm.conf") && (!m_bV2P))
		{
			if(false == editLvmConfFile())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to modified the File \"/etc/lvm/lvm.conf\" \n");
			}
		}

		int iSrcDiskNumber = 0;
		std::map<std::string,std::string>::iterator tempIter;
		std::map<std::string,std::map<std::string,std::string> >::iterator iterVmPhysicalDisk;
		iterVmPhysicalDisk = m_mapOfSrcTgtPhysicaldevices.begin();
		
        //This outer Loop is for each Protected VM
		while(iterVmPhysicalDisk != m_mapOfSrcTgtPhysicaldevices.end())
		{			
            DebugPrintf(SV_LOG_INFO,"----------------------------------------------------------------------------------------------\n");
            DebugPrintf(SV_LOG_INFO,"Virtual Machine Name = %s\n",iterVmPhysicalDisk->first.c_str());
		    DebugPrintf(SV_LOG_INFO,"----------------------------------------------------------------------------------------------\n");

            VxJobXml VmObj;
			
			if(!m_bPhySnapShot)
			{
				if (false == xGetValuesFromXML(VmObj,m_strXmlFile,iterVmPhysicalDisk->first))
				{
					m_bUpdateStep = true;
					currentTask.TaskNum = INM_VCON_TASK_4;
					currentTask.TaskStatus = INM_VCON_TASK_FAILED;
					currentTask.TaskErrMsg = "Failed to fetch the Retention details and Process Server IP for this VM.. For extended Error information download EsxUtil.log in vCon Wiz";
					currentTask.TaskFixSteps = string("Check the xml file is in proper state or not, if teh values in that are fine then Rerun the job") +
													string("if issue persists contact inmage customer support");
					nextTask.TaskNum = m_strOperation;
					nextTask.TaskStatus = INM_VCON_TASK_FAILED;

					DebugPrintf(SV_LOG_ERROR,"Failed to fetch the Retention details and Process Server IP for this VM.\n");
					retValue = false;
					break;
				}
							
				bool bsrcnode = true;
				string strIPAddress;
				if(m_vConVersion < 4.0)
				{
					if(true == GetSrcIPUsingCxCli(iterVmPhysicalDisk->first, strIPAddress))
					{
						DebugPrintf(SV_LOG_INFO,"Source Vm Name :(%s) ,IP:[%s]\n",iterVmPhysicalDisk->first.c_str(),strIPAddress.c_str());
					}
					//reading the source vms natip or normal ip from Esx.xml file
					else if(readIpAddressfromEsxXmlfile(iterVmPhysicalDisk->first,strIPAddress,bsrcnode))
					{
						//For debug 
						DebugPrintf(SV_LOG_INFO,"Source Vm Name :(%s) ,IP:[%s]\n",iterVmPhysicalDisk->first.c_str(),strIPAddress.c_str());
					}
					else
					{
						m_bUpdateStep = true;
						currentTask.TaskNum = INM_VCON_TASK_4;
						currentTask.TaskStatus = INM_VCON_TASK_FAILED;
						currentTask.TaskErrMsg = string("Failed to find IP address for VM : ") + iterVmPhysicalDisk->first + string(". For extended Error information download EsxUtil.log in vCon Wiz");
						currentTask.TaskFixSteps = string("Rerun the job if issue persists contact inmage customer support");
						nextTask.TaskNum = m_strOperation;
						nextTask.TaskStatus = INM_VCON_TASK_FAILED;

						DebugPrintf(SV_LOG_ERROR,"Failed to find IP address for VM : %s\n",iterVmPhysicalDisk->first.c_str());
						retValue = false;
						break;
					}
				}
			
				if(m_vConVersion >= 4.0)
				{
					VmObj.SourceInMageHostId = iterVmPhysicalDisk->first;
					VmObj.TargetInMageHostId = strMasterTgtIpAddress;
				}
				else
				{
					VmObj.SourceMachineIP = strIPAddress;
					VmObj.TargetMachineIP = strMasterTgtIpAddress;
				}
				VmObj.RetentionLogVolumeName =  VmObj.RetentionLogPath;           
				VmObj.VmHostName = iterVmPhysicalDisk->first;
			}

			std::string strReplicatedVmsInfoFle;
			string strInfoFile;
			std::map<std::string, std::string>::iterator iterVm;
			if(m_bPhySnapShot)
			{
				strReplicatedVmsInfoFle =  std::string(getVxInstallPath()) +  std::string("failover_data/")+ std::string(iterVmPhysicalDisk->first) + string(".sInfo");
				DebugPrintf(SV_LOG_INFO,"The snapshot VM sInfo file = %s\n",strReplicatedVmsInfoFle.c_str());
				iterVm = m_mapVmIdToVmName.find(iterVmPhysicalDisk->first);
				if(iterVm->first.compare(iterVm->second) != 0)
					strInfoFile = std::string(getVxInstallPath()) +  std::string("failover_data/")+ std::string(iterVm->second) + string(".sInfo");
			}
			else
			{
				strReplicatedVmsInfoFle =  std::string(getVxInstallPath()) +  std::string("failover_data/")+ std::string(iterVmPhysicalDisk->first) + string(".pInfo");
				DebugPrintf(SV_LOG_INFO,"The replicated VM pInfo file = %s\n",strReplicatedVmsInfoFle.c_str());
				iterVm = m_mapVmIdToVmName.find(iterVmPhysicalDisk->first);
				if(iterVm->first.compare(iterVm->second) != 0)
					strInfoFile = std::string(getVxInstallPath()) +  std::string("failover_data/")+ std::string(iterVm->second) + string(".pInfo");
			}
			
			std::ofstream replicatedPairInfoFile;
			if(m_bIsAdditionOfDisk)
			{
				if(false == checkIfFileExists(strReplicatedVmsInfoFle))
				{
					DebugPrintf(SV_LOG_INFO, "Addition of Disk, File %s dose not exist, Upgrade case", strReplicatedVmsInfoFle.c_str());
					if(true == checkIfFileExists(strInfoFile))
					{
						char cmdline[256];
						inm_sprintf_s(cmdline,ARRAYSIZE(cmdline), "cp %s %s", strInfoFile.c_str(), strReplicatedVmsInfoFle.c_str());
						system(cmdline);
						DebugPrintf(SV_LOG_INFO, "Created file %s from old file %s", strReplicatedVmsInfoFle.c_str(), strInfoFile.c_str());
					}
				}
				DebugPrintf(SV_LOG_INFO, "Addition of Disk, Updating file %s", strReplicatedVmsInfoFle.c_str());
				replicatedPairInfoFile.open(strReplicatedVmsInfoFle.c_str(), std::ios::app);
			}
			else
			{
				DebugPrintf(SV_LOG_INFO, "Normal Protection/Snapshot, Creating new file %s", strReplicatedVmsInfoFle.c_str());
				replicatedPairInfoFile.open(strReplicatedVmsInfoFle.c_str(), std::ios::trunc);
			}
				
			if(!replicatedPairInfoFile.is_open())
			{				
				currentTask.TaskNum = INM_VCON_TASK_4;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				currentTask.TaskErrMsg = string("writing failed on file ") + strReplicatedVmsInfoFle + string(". For extended Error information download EsxUtil.log in vCon Wiz");
				currentTask.TaskFixSteps = string("check for permissions on the file and Rerun the job, if issue persists contact inmage customer support");
				if(m_bPhySnapShot)
				{
					nextTask.TaskNum = INM_VCON_TASK_5;
					nextTask.TaskStatus = INM_VCON_TASK_QUEUED;
				}
				else
				{
					m_bUpdateStep = true;
					nextTask.TaskNum = m_strOperation;
					nextTask.TaskStatus = INM_VCON_TASK_FAILED;
				}				

				DebugPrintf(SV_LOG_ERROR,"Failed to open %s for writing\n",strReplicatedVmsInfoFle.c_str());
				retValue = false;//bug:Should we continue,since this is used in breaking replicationPair
				break;
			}
			std::map<std::string,std::string>::iterator iterMap_tgt_srcDisks = iterVmPhysicalDisk->second.begin();
			while(iterMap_tgt_srcDisks != iterVmPhysicalDisk->second.end())
			{
				if(!m_bV2P)
				{
					VmObj.SourceVolumeName = iterMap_tgt_srcDisks->second; //this is source disk
					VmObj.TargetVolumeName = iterMap_tgt_srcDisks->first; //this is target disk
				}
				else
				{
					VmObj.SourceVolumeName = iterMap_tgt_srcDisks->first; //this is source disk
					VmObj.TargetVolumeName = iterMap_tgt_srcDisks->second; //this is target disk
				}

                //update pInfo file
                replicatedPairInfoFile<<VmObj.SourceVolumeName;
				replicatedPairInfoFile<<"!@!@!";
				replicatedPairInfoFile<<VmObj.TargetVolumeName<<endl;
                m_vecVxJobObj.push_back(VmObj);

				iterMap_tgt_srcDisks++;
			}
            replicatedPairInfoFile.close();

			AssignSecureFilePermission(strReplicatedVmsInfoFle);
			
			if(!strInfoFile.empty())
			{
				char cmdline[256];
				inm_sprintf_s(cmdline,ARRAYSIZE(cmdline), "cp %s %s", strReplicatedVmsInfoFle.c_str(), strInfoFile.c_str());
				system(cmdline);
				AssignSecureFilePermission(strInfoFile);
			}

			//At this point we have to Make sure that Pairs information file is created??
			//There is  no difference for Linux V2V and P2V protection.So this code is going to get executed for both.
			if ( false == WriteProtectedDiskInformationInOrder( std::string(iterVmPhysicalDisk->first) , m_bIsAdditionOfDisk ) )
			{
				//Shall we have to fail the status over here if it failed to make file?
				//We have to fail here.
				DebugPrintf(SV_LOG_ERROR , "Failed to write protected disk information in order for machine %s.\n", iterVmPhysicalDisk->first.c_str() );
			}


            iterVmPhysicalDisk++;
		}

		if(false == retValue)
			break;
		
		std::map<std::string, std::string>::iterator iterVm;
		string strInmHostNameToIdFile =  std::string(getVxInstallPath()) +  std::string("failover_data/")+ string("inmage_hostname_hostid");
		std::ofstream strInmfile(strInmHostNameToIdFile.c_str(), std::ios::app);
		if(!strInmfile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open %s for writing\n",strInmHostNameToIdFile.c_str());
		}
		else
		{
			iterVm = m_mapVmIdToVmName.begin();
			while(iterVm != m_mapVmIdToVmName.end())
			{
				//update inmage file
				strInmfile<<"VmName=\"";
				strInmfile<<iterVm->second<<"\"\t";
				strInmfile<<"VmInMageId=\"";
				strInmfile<<iterVm->first<<"\""<<endl;
				iterVm++;
			}
			strInmfile.close();
		}
		
		if(!m_bPhySnapShot)
		{
			for(iterVm = m_mapVmIdToVmName.begin(); iterVm != m_mapVmIdToVmName.end(); iterVm++)
			{
				string strCxApiResXmlFile;				
				if(false == GetHostInfoFromCx(iterVm->first, strCxApiResXmlFile))
				{
					DebugPrintf(SV_LOG_ERROR,"Need to configure Network manually for host : %s (or) Need to create the file %s manually.\n", iterVm->first.c_str(), strCxApiResXmlFile.c_str());
				}
			}

			string strErrorMsg = "";
			string strFixStep = "";
			if(false == SetVxJobs(strErrorMsg, strFixStep))
			{
				m_bUpdateStep = true;
				currentTask.TaskNum = INM_VCON_TASK_4;
				currentTask.TaskStatus = INM_VCON_TASK_FAILED;
				strErrorMsg = strErrorMsg;
				strFixStep = strFixStep;
				nextTask.TaskNum = m_strOperation;
				nextTask.TaskStatus = INM_VCON_TASK_FAILED;

				DebugPrintf(SV_LOG_ERROR,"SetVxJobs() failed.\n");
				retValue = false;
				break;
			}

			currentTask.TaskNum = INM_VCON_TASK_4;
			currentTask.TaskStatus = INM_VCON_TASK_COMPLETED;
			nextTask.TaskNum = m_strOperation;
			nextTask.TaskStatus = INM_VCON_TASK_COMPLETED;
			ModifyTaskInfoForCxApi(currentTask, nextTask, true);
			UpdateTaskInfoToCX();
		}

	}while(0);

	if(false == retValue)
	{
		ModifyTaskInfoForCxApi(currentTask, nextTask, m_bUpdateStep);
		UpdateTaskInfoToCX();
	}

	if (!m_bPhySnapShot)
	{
        map<string, SendAlerts> sendAlertsInfo;
		if (bUpdateToCXbyMTid)
			UpdateSendAlerts(currentTask, m_strCxPath, "protection", sendAlertsInfo);
		else
		{
			UpdateSrcHostStaus(currentTask);
			UpdateSendAlerts(sendAlertsInfo, "protection");
		}
		if (!SendAlertsToCX(sendAlertsInfo))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to update the Alerts to CX!!!\n");
		}
	}

	if(!m_strCxPath.empty())
	{
		//uploads the Log file to CX
		if(false == UploadFileToCx(m_strLogFile, m_strCxPath))
			DebugPrintf(SV_LOG_ERROR, "Failed to upload the %s file to CX", m_strLogFile.c_str());
		else
			DebugPrintf(SV_LOG_INFO, "Successfully upload file %s file to CX", m_strLogFile.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
	return retValue;
}

bool LinuxReplication::WriteProtectedDiskInformationInOrder( std::string str_hostId , bool b_isAdditionOfDisk )
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool b_returnValue = true;

	//first we need to write input file.
	std::string str_inputFile = m_strDataDirPath + str_hostId + std::string(".InputFile");
	DebugPrintf(SV_LOG_INFO, "Input File = %s.\n", str_inputFile.c_str() );
	std::ofstream WriteInputFile;
	WriteInputFile.open( str_inputFile.c_str() , std::ios::out | std::ios::trunc );
	if ( !WriteInputFile.is_open() )
	{				
		DebugPrintf(SV_LOG_ERROR,"Failed to open %s for writing\n",str_inputFile.c_str());
		b_returnValue = false;
	}

	//Write Input File.
	WriteInputFile<<"_MACHINE_UUID_ "<<str_hostId.c_str()<<endl;
	
	if ( b_isAdditionOfDisk )
		WriteInputFile<<"_TASK_ "<<"ADDPROTECT"<<endl;
	else
		WriteInputFile<<"_TASK_ "<<"PROTECT"<<endl;
	
	WriteInputFile<<"_PLAN_NAME_ "<<m_strDataDirPath.c_str()<<endl; //here we are sending complete path to Plan instead of PLAN NAME.
	WriteInputFile<<"_VX_INSTALL_PATH_ "<<std::string(getVxInstallPath())<<endl;
		
	//Now we have to close the open handle.
	WriteInputFile.close();

	AssignSecureFilePermission(str_inputFile);

	//We have to call our script for this to execute.
	//Get VX installation path and build path to the scripts
	std::string str_vxInstallPath	= std::string(getVxInstallPath());
	std::string str_scriptsPath		= str_vxInstallPath + std::string("scripts/vCon/");
	//append script file name to scripts path 
	std::string str_prepareTargetInformation = str_scriptsPath + std::string(LINUXP2V_PREPARETARGET_INFORMATION) + std::string(" ") + str_inputFile;
	std::string str_outputFile = str_scriptsPath + std::string("PrepareTarget.log");

	DebugPrintf( SV_LOG_INFO , "Command = %s .\n" , str_prepareTargetInformation.c_str() );
	//Now we have to invoke script such that it collects all information of source side.
	if(false == ExecuteCmdWithOutputToFileWithPermModes( str_prepareTargetInformation, str_outputFile, O_APPEND | O_RDWR | O_CREAT))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to write protected disk information for physical machine with host id %s.\n" , str_hostId.c_str() );
		b_returnValue	= false;
	}
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	return b_returnValue;
}

std::string LinuxReplication::GetDiskNameForNumber(int disk_number) const
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	std::string strDiskName("/dev/" + std::string(posix_device_names[disk_number]));
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return strDiskName;
}


std::string LinuxReplication::FetchInMageHostId()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strHostInMageId = getInMageHostId();
	if(strHostInMageId.empty())
	{
		string strHostName = GetHostName();
		bool bsrc = false;
		//reading the master target inmage host id from Esx.xml file
		if(readInMageHostIdfromEsxXmlfile(strHostName,strHostInMageId,bsrc))
		{
			//For debug 
			DebugPrintf(SV_LOG_INFO,"Master Target Name :(%s) ,InMage HostId: [%s] ",strHostName.c_str(),strHostInMageId.c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Faield to get the InMage Host-id for MT %s",strHostName.c_str());
		}
	}
	DebugPrintf(SV_LOG_INFO,"Master Target InMage HostId: [%s] ", strHostInMageId.c_str());

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return strHostInMageId;
}


const std::string LinuxReplication:: FetchLocalIpAddress()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	char szTemp[MAXHOSTNAMELEN];
	memset((void*)szTemp, 0, MAXHOSTNAMELEN);
    std::string sHostName = GetHostName();
	std::string sIPAddress = "";
    bool bsrc = false;
	if(true == GetSrcIPUsingCxCli(sHostName, sIPAddress))
	{
		DebugPrintf(SV_LOG_INFO,"MT Host Name :(%s) ,IP:[%s]\n",sHostName.c_str(),sIPAddress.c_str());
	}
    else if(readIpAddressfromEsxXmlfile(sHostName,sIPAddress,bsrc))
	{
		//For debug 
		DebugPrintf(SV_LOG_INFO,"Master Target Name : %s , IP : %s\n",sHostName.c_str(),sIPAddress.c_str());
	}
    else
    {
	    if ( !sHostName.empty() )
	    {
		    memset(szTemp, 0, sizeof(szTemp));
		    hostent* pHostEntry = NULL;
            if( NULL != ( pHostEntry = ACE_OS::gethostbyname( sHostName.c_str() ) ) )
		    {
                assert( 4 == pHostEntry->h_length );
				inm_sprintf_s( szTemp,ARRAYSIZE(szTemp), "%u.%u.%u.%u", 
                    *((unsigned char*) pHostEntry->h_addr_list[0]+0),
                    *((unsigned char*) pHostEntry->h_addr_list[0]+1),
                    *((unsigned char*) pHostEntry->h_addr_list[0]+2),
                    *((unsigned char*) pHostEntry->h_addr_list[0]+3) );
			    sIPAddress = szTemp;
		    }
		    else
            {
			    DebugPrintf(SV_LOG_ERROR,"FAILED : Couldn't get local IP address for %s\n",sHostName.c_str());
			    DebugPrintf(SV_LOG_ERROR,"Error Code : %d\n",ACE_OS::last_error());
            }
	    }
        else
        {
		    DebugPrintf(SV_LOG_ERROR,"FAILED : Couldn't get local IP address for %s\n",sHostName.c_str());
		    DebugPrintf(SV_LOG_ERROR,"Error Code : %d\n",ACE_OS::last_error());
        }
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return sIPAddress;

}
std::string LinuxReplication:: GetHostName() 
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	std::string sHostName = "";
	char szTemp[MAXHOSTNAMELEN];
	memset((void*)szTemp, 0, MAXHOSTNAMELEN);
    if ( 0 == ACE_OS::hostname(szTemp,sizeof(szTemp) ) )
	{
		sHostName = szTemp;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Could not determine HostName. Error Code: %d\n",ACE_OS::last_error());		
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return sHostName;
}
bool LinuxReplication::convertTokenIntoProperScsiFormat(string &strLineRead)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;
	m_strLsscsiID.clear();
	
	strLineRead = strLineRead.substr(1,strLineRead.length());
    strLineRead = strLineRead.substr(0,strLineRead.length()-1);
	
	typedef string::iterator iter;
    vector<string> vectorOfStringTokens;
    iter i =  strLineRead.begin();
    while (i != strLineRead.end())
    {
        // ignore leading commas
        i = std::find_if(i, strLineRead.end(), not_Colon);
        // find end of next word
        iter j = std::find_if(i, strLineRead.end(),isColon);
        // copy the characters in [i, j)
        if (i != strLineRead.end()) vectorOfStringTokens.push_back(string(i, j));
            i = j;
    }
    if(vectorOfStringTokens.size() != 4)
    {
	bRetValue = false;	
	DebugPrintf(SV_LOG_ERROR,"Invalid no of tokens found for scsi Ids.Cannot continue.\n");
	strLineRead = "";
    }
    else
	{
	       strLineRead.clear();
	       strLineRead = vectorOfStringTokens[0] + string(":") + vectorOfStringTokens[2];
		   m_strLsscsiID = vectorOfStringTokens[0] + string(":") + vectorOfStringTokens[1] + string(":") + vectorOfStringTokens[2] + string(":") + vectorOfStringTokens[3];
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}

bool LinuxReplication::parseLsscsiOutput()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    //TO DO: do proper logging
    int ret = system("lsscsi" ">/tmp/test.txt");
    if (WIFSIGNALED(ret) )
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to execute the 'lsscsi'. Check if 'lsscsi' package is installed and working.\n ");
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    else
    {
		DebugPrintf(SV_LOG_INFO,"Persisted the lsscsi output.\n ");
    }
    string strFileNmae;
    strFileNmae = "/tmp/test.txt";
    unsigned int uVecSize;
    ifstream inFile(strFileNmae.c_str());
    if(!inFile.is_open())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open file %s\n ",strFileNmae.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    string strLineRead;
    vector<string> strVector;
    while(std::getline(inFile,strLineRead))
    {
        //Ignore the new Line characetrs
        if(strLineRead.length() <= 1)
            continue;
        
        strVector = parseCsvLine(strLineRead);
        if(strVector.empty())
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to parse the output.\n ");
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return false;
        }
        else
        {
            uVecSize = strVector.size();
            string strScsiId = strVector[0];
            if(false == convertTokenIntoProperScsiFormat(strScsiId))
            {
                DebugPrintf(SV_LOG_ERROR,"Convert Token Into Proper SSCI Format failed!\n");
                inFile.close();
                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                return false;
            }
            m_mapOfScsiCont.insert(make_pair(strScsiId,strVector[uVecSize-1]));
			m_mapLsScsiID.insert(make_pair(strScsiId, m_strLsscsiID));
        }
        uVecSize = 0;
        strVector.clear();
    }
    inFile.close();
    remove(strFileNmae.c_str());
	std::map<std::string,std::string>::iterator mapIter;
	mapIter = m_mapOfScsiCont.begin();
	while(mapIter != m_mapOfScsiCont.end())
	{
		DebugPrintf(SV_LOG_INFO,"mapIter->first =%s mapIter->second =%s\n",mapIter->first.c_str(),mapIter->second.c_str());
		mapIter++;
	}
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}


bool LinuxReplication::SrcTgtDiskMapFromScsiFileV2P()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetvalue = true;
	m_mapVmIdToVmName.clear();
	string strFileName = m_strDataDirPath + string("Inmage_scsi_unit_disks.txt");
    string strLineRead;
	size_t index;
	string token("!@!@!");
	map<string, string> mapOfSrcToTgtDisk;
    ifstream inFile(strFileName.c_str());
    do
    {
        if(!inFile.is_open())
        {
            DebugPrintf(SV_LOG_ERROR,"\nFailed to open File : %s",strFileName.c_str());
            bRetvalue = false;
            break;
        }
        while(inFile >> strLineRead)
        {
			string strVmName, strVmHostId;
			if(m_vConVersion >= 4.0)
			{
				size_t pos1 = strLineRead.find_first_of("(");
				size_t pos2 = strLineRead.find_last_of(")");
				if(pos1 != string::npos)
				{
					strVmHostId = strLineRead.substr(0, pos1);
					if(pos2 != string::npos)
						strVmName = strLineRead.substr(pos1+1, pos2-(pos1+1));
					else
						strVmName = strLineRead.substr(pos1+1);
				}
				else
					strVmHostId = strLineRead;
			}
			else
			{
				strVmName = strLineRead;
				strVmHostId = strVmName;
			}
				
			DebugPrintf(SV_LOG_INFO,"VM Name : %s  And Vm InMage ID: %s\n",strVmName.c_str(), strVmHostId.c_str());
			m_mapVmIdToVmName.insert(make_pair(strVmHostId, strVmName));
			
            if(inFile >> strLineRead)
            {
                stringstream sstream ;
                sstream << strLineRead;
                int iCount;
                sstream >> iCount;
                for(int i = 0; i < iCount; i++ )
                {
                    if(inFile >> strLineRead)
                    {
						index = strLineRead.find_first_of(token);
						if(index != std::string::npos)
						{
							string strSrcDisk = strLineRead.substr(0,index);
							string strTgtDisk = strLineRead.substr(index+(token.length()),strLineRead.length());
							DebugPrintf(SV_LOG_INFO,"strSrcDisk =  %s  <==>  ",strSrcDisk.c_str());
							DebugPrintf(SV_LOG_INFO,"strTgtDisk =  %s\n",strTgtDisk.c_str());
							if ((!strSrcDisk.empty()) && (!strTgtDisk.empty()))
							{
								mapOfSrcToTgtDisk.insert(make_pair(strSrcDisk, strTgtDisk));
							}
						}
						else
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to find token in lineread in the file : %s\n",strFileName.c_str());
							bRetvalue = false;
							break;
						}
                    }
                    else
					{
		                inFile.close();
						DebugPrintf(SV_LOG_ERROR,"1. Cant read the disk mapping file\n");
                        bRetvalue = false;
                        break;
                    }
                }
                if(bRetvalue == false)
                    break;
            }
            else
            {
                inFile.close();
   				DebugPrintf(SV_LOG_ERROR,"2. Cant read the disk mapping file.\n");
                bRetvalue = false;
                break;
            }
            if(bRetvalue == false)
                break;
            else
            {
				m_mapOfSrcTgtPhysicaldevices.insert(make_pair(strVmHostId, mapOfSrcToTgtDisk));
				mapOfSrcToTgtDisk.clear();
            }
        }
    }while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetvalue;	
}

std::map<std::string,std::string>LinuxReplication::readProtectedVmsIpAddresses()const
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	std::map<std::string,std::string> VM_IpAddressMap;
	std::string strVirtualMachineName;
	std::string strVmIp;
	std::string strVxInstallPath = getVxInstallPath();
	std::string strProtectedVmsIpAddressFile ;
	
	strProtectedVmsIpAddressFile = strVxInstallPath + std::string("/failover_data/") + std::string("ProtectedVmsIpAddress.txt");
	ifstream in (strProtectedVmsIpAddressFile.c_str());
	if (in.is_open() )
	{
        DebugPrintf(SV_LOG_INFO,"File opened Success fully.\n");
	}
	else
	{
			DebugPrintf(SV_LOG_ERROR,"Failed to open %s\n",strProtectedVmsIpAddressFile.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return VM_IpAddressMap;
	}
	std::string line;
	while (getline(in, line))
	{
        if(line.length() <= 1)
            continue;
        if(line[line.size()-1] == '\r') // this will come when a file created in windows is read in linux
            line.resize(line.size()-1);
		std::string::size_type index = line.find_first_of("=");
		if( index != std::string::npos)
		{
			strVirtualMachineName = line.substr(0,index);
			strVmIp = line.substr(index+1,line.length());
			DebugPrintf(SV_LOG_INFO,"strVirtualMachineName = %s\n",strVirtualMachineName.c_str());
			DebugPrintf(SV_LOG_INFO,"strVmIp =  %s\n",strVmIp.c_str());
			VM_IpAddressMap[strVirtualMachineName] = strVmIp;
		}
	}
	in.close();
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return VM_IpAddressMap;
}


/************************************************************
Get the vcon Version from Esx.xml file
************************************************************/
bool LinuxReplication::xGetvConVersion()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	m_bIsAdditionOfDisk = true;
	m_bV2P = false;

    DebugPrintf(SV_LOG_INFO,"XML File - %s\n", m_strXmlFile.c_str());

	xmlDocPtr xDoc;	
	xmlNodePtr xCur;	

	xDoc = xmlParseFile(m_strXmlFile.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	//Found the doc. Now process it.
	xCur = xmlDocGetRootElement(xDoc);
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"%s is empty. Cannot Proceed further.\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <root> entry not found\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	xmlChar *attr_value_temp;
	if(!m_bPhySnapShot)
	{
		attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"AdditionOfDisk");
		if(NULL == attr_value_temp)
		{
			DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <AdditionOfDisk> entry not found\n");
			m_bIsAdditionOfDisk = false;
		}
		else
		{
			if (0 == xmlStrcasecmp(attr_value_temp,(const xmlChar*)"false")) 
			{
				DebugPrintf(SV_LOG_INFO, "Not addition of Disk\n");
				m_bIsAdditionOfDisk = false;
			}            
		}

		attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"V2P");
		if(NULL == attr_value_temp)
		{
			DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <V2P> entry not found\n");
		}
		else
		{
			if (xmlStrcasecmp(attr_value_temp,(const xmlChar*)"false")) 
			{
				m_bV2P = true;
			}           
		}
	}
	else
		m_bIsAdditionOfDisk = false;

	DebugPrintf(SV_LOG_INFO,"Addition of Disk = %d.\n",m_bIsAdditionOfDisk);
	DebugPrintf(SV_LOG_INFO,"Linux V2P = %d.\n",m_bV2P);

	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"ConfigFileVersion");
	if (attr_value_temp != NULL)
	{
		string strVConVersion = string((char *)attr_value_temp);
		stringstream temp;
		temp << strVConVersion;
		temp >> m_vConVersion;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"The %s file is not in expected format : ConfigFileVersion entry not found\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}


/* Fetch retention details and process server IP from ESX.xml provided by UI.

	Sample Hierarchy of XML file
	<root CX_IP="10.0.15.153">
		<SRC_ESX>
			<host ....>					
				<process_server IP="10.0.0.109" />
				<retention retain_change_MB="0" retain_change_days="7"  log_data_dir="c:\logs" />
				.
				.
			</host>
		</SRC_ESX>
		.
		.
	</root>

	We need to fetch the process_server IP and retention details.
	Output - Vx Job Object with retention details and Process Server IP
	Return Value - true on success and false on failure
*/
bool LinuxReplication::xGetValuesFromXML(VxJobXml &VmObj, std::string FileName, std::string HostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	xmlDocPtr xDoc;	
	xmlNodePtr xCur;	

	xDoc = xmlParseFile(FileName.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	//Found the doc. Now process it.
	xCur = xmlDocGetRootElement(xDoc);
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"%s is empty. Cannot Proceed further.\n", FileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <root> entry not found\n", FileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	//If you are here root entry is found. Go for SRC_ESX entry.
	xCur = xGetChildNode(xCur,(const xmlChar*)"SRC_ESX");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <SRC_ESX> entry not found\n", FileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;			
	}
	
	//If you are here SRC_ESX entry is found. Go for host entry with required HostName.
	xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"inmage_hostid",(const xmlChar*)HostName.c_str());
	if(xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with required inmage_hostid> entry not found\n", FileName.c_str());
		xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"hostname",(const xmlChar*)HostName.c_str());
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with required hostname> entry not found\n", FileName.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;			
		}
	}
	
	xmlChar *attr_value_temp;	
	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"isSourceNatted");
	if (attr_value_temp != NULL)
	{
		if (0 == xmlStrcasecmp(attr_value_temp,(const xmlChar*)"True"))
		{
			VmObj.IsSourceNatted = string("yes");
		}
		else
		{
			VmObj.IsSourceNatted = string("no");
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <isSourceNatted> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"isTargetNatted");
	if (attr_value_temp != NULL)
	{
		if (0 == xmlStrcasecmp(attr_value_temp,(const xmlChar*)"True"))
		{
			VmObj.IsTargetNatted = string("yes");
		}
		else
		{
			VmObj.IsTargetNatted = string("no");
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <isTargetNatted> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"secure_src_to_ps");
	if (attr_value_temp != NULL)
	{
		if (0 == xmlStrcasecmp(attr_value_temp,(const xmlChar*)"True"))
		{
			VmObj.SecureSrcToPs = string("yes");
		}
		else
		{
			VmObj.SecureSrcToPs = string("no");
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <secure_src_to_ps> entry not found\n");
		VmObj.SecureSrcToPs = "no";
	}
	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"secure_ps_to_tgt");
	if (attr_value_temp != NULL)
	{
		if (0 == xmlStrcasecmp(attr_value_temp,(const xmlChar*)"True"))
		{
			VmObj.SecurePsToTgt = string("yes");
		}
		else
		{
			VmObj.SecurePsToTgt = string("no");
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <secure_ps_to_tgt> entry not found\n");
		VmObj.SecurePsToTgt = "no";
	    
	}
	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"compression");
	if (attr_value_temp != NULL)
	{
		VmObj.Compression = string((char *)attr_value_temp);
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <compression> entry not found\n");
		VmObj.Compression = "cx_base";
	}
	
	//If you are here host entry is found. Go for Process server and retention details.
	//Get PS IP
	xmlNodePtr xPsPtr = xGetChildNode(xCur,(const xmlChar*)"process_server");
	if (xPsPtr == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <process_server> entry not found\n", FileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;			
	}
	
	attr_value_temp = xmlGetProp(xPsPtr,(const xmlChar*)"IP");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <process_server IP> entry not found\n", FileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	VmObj.ProcessServerIp = string((char *)attr_value_temp);

	//Get Retention details -
	//	<retention retain_change_MB="0" retain_change_days="7"  log_data_dir="c:\logs" />
	xmlNodePtr xRetentionPtr = xGetChildNode(xCur,(const xmlChar*)"retention");
	if (xRetentionPtr == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <retention> entry not found\n", FileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;			
	}
	
	attr_value_temp = xmlGetProp(xRetentionPtr,(const xmlChar*)"retain_change_MB");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <retention retain_change_MB> entry not found\n", FileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	VmObj.RetainChangeMB = string((char *)attr_value_temp);

	attr_value_temp = xmlGetProp(xRetentionPtr,(const xmlChar*)"retain_change_days");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <retention retain_change_days> entry not found\n", FileName.c_str());
	    VmObj.RetainChangeDays = "";
	}
	else
		VmObj.RetainChangeDays = string((char *)attr_value_temp);

	if(VmObj.RetainChangeDays.empty())
	{
		attr_value_temp = xmlGetProp(xRetentionPtr,(const xmlChar*)"retain_change_hours");
		if (attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <retention retain_change_hours> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		VmObj.RetainChangeHours = string((char *)attr_value_temp);
	}

	attr_value_temp = xmlGetProp(xRetentionPtr,(const xmlChar*)"log_data_dir");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <retention log_data_dir> entry not found\n", FileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	VmObj.RetentionLogPath = string((char *)attr_value_temp);
	
	VmObj.RetTagType = "File System";
	
	xmlNodePtr xTimePtr = xGetChildNode(xCur,(const xmlChar*)"time");
	if (xTimePtr == NULL)
	{
		DebugPrintf(SV_LOG_INFO,"The ESX.xml document is not having the \"time\" tag. Sparse retention is not configurable\n");			
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Sparse Retention is selected.\n");	
		for(xmlNodePtr xChild = xTimePtr->children; xChild != NULL; xChild = xChild->next)
		{
			SparseRet_t objSparse;
			objSparse.strTimeWindow = string((char *)xChild->name);
			if(objSparse.strTimeWindow.find("time_interval_window") != string::npos)
			{
				attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"unit");
				if (attr_value_temp == NULL)
				{
					DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <%s unit> entry not found\n", objSparse.strTimeWindow.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return false;
				}
				objSparse.strUnit = string((char *)attr_value_temp);

				attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"value");
				if (attr_value_temp == NULL)
				{
					DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <%s value> entry not found\n", objSparse.strTimeWindow.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return false;
				}
				objSparse.strValue = string((char *)attr_value_temp);

				attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"bookmark_count");
				if (attr_value_temp == NULL)
				{
					DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <%s bookmark_count> entry not found\n", objSparse.strTimeWindow.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return false;
				}
				objSparse.strBookMark = string((char *)attr_value_temp);

				attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"range");
				if (attr_value_temp == NULL)
				{
					DebugPrintf(SV_LOG_INFO,"The %s does not conatains the \"range\" tag\n", objSparse.strTimeWindow.c_str());
					objSparse.strTimeRange = "";
				}
				else					
					objSparse.strTimeRange = string((char *)attr_value_temp);
				VmObj.sparseTime.push_back(objSparse);
			}
			else
				DebugPrintf(SV_LOG_INFO,"Got the child node for \"time\" as %s\n", objSparse.strTimeWindow.c_str());
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

VxJobXml::VxJobXml()
{	
    VmHostName = string("");
    SourceMachineIP = string("");
    TargetMachineIP = string("");
    SourceVolumeName = string("");
    TargetVolumeName = string("");
	ProcessServerIp = string("");
    RetentionLogVolumeName = string("");
	RetentionLogPath = string("");
	RetainChangeMB = string("");
	RetainChangeDays = string("");
	IsSourceNatted = string("");
	IsTargetNatted = string("");
	RetTagType = string("");
	SecureSrcToPs = string("");
	SecurePsToTgt = string("");
	Compression = string("");
	sparseTime.empty();
}

//Checks whether Esx.xml document exists and 
//		returns true  if exists 
//				false if does not exist.
bool LinuxReplication::xCheckXmlFileExists()
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
    if (checkIfFileExists(m_strXmlFile))
    {
        if (NULL == xmlParseFile(m_strXmlFile.c_str()))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to parse the file - %s.\n",m_strXmlFile.c_str());
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return false;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"File - %s is not found.\n",m_strXmlFile.c_str());
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//Check whether a file exists or not.
bool LinuxReplication::checkIfFileExists(std::string strFileName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool blnReturn = true;
    struct stat stFileInfo;
    int intStat;
    
    // Attempt to get the file attributes
    intStat = stat(strFileName.c_str(),&stFileInfo);
    if(intStat == 0) {
        // We were able to get the file attributes
        // so the file obviously exists.
        blnReturn = true;
    } else {
        // We were not able to get the file attributes.
        // This may mean that we don't have permission to
        // access the folder which contains this file. If you
        // need to do that level of checking, lookup the
        // return values of stat which will give you
        // more details on why stat failed.
        blnReturn = false;
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return blnReturn;
}

//Check if Dir exists and return true
bool LinuxReplication::checkIfDirExists(string strDirName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool blnReturn = false;

	try
	{
		if (access(strDirName.c_str(), 0 ) == 0)
		{
			struct stat stFileInfo;
			stat(strDirName.c_str(), &stFileInfo);
			if(stFileInfo.st_mode & S_IFDIR)
				blnReturn = true;
		}
	}
	catch(...)
	{
		DebugPrintf(SV_LOG_ERROR,"Exception on accessing the path : %s\n", strDirName.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return blnReturn;
}

//Process all the Vx Pairs,
//Restart the Vx service(svagents)
//Separate registered and unregistered volumes with Cx.
//Set the Vx Jobs for registered volumes automatically.
//Update the user about the unregistered volumes and ask him to set them manually later
bool LinuxReplication::SetVxJobs(string &strErrorMsg, string &strFixStep)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__); 
	if(false == RegisterHostUsingCdpCli())
	{
        DebugPrintf(SV_LOG_ERROR,"Failed to Register the Master Target disks using cdpcdli\n");
	}

    //Prepare CX for posting.
    std::string strCxIpAddress = FetchCxIpAddress();
    DebugPrintf(SV_LOG_INFO,"Cx IP address = %s\n",strCxIpAddress.c_str());
    if(strCxIpAddress.empty())
    {
		strErrorMsg = "Failed to get the CX IP Address. For extended Error information check EsxUtil.log in MT";
		strFixStep = string("Check the Vxagent is pointed to CX or not and availability of CX ") + 
						string("Restart the vxagent and rerun the job again");

	    DebugPrintf(SV_LOG_ERROR,"Failed to fetch Cx IP address.\n");
 	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	    return false;
    }
    std::string strCXHttpPortNumber = FetchCxHttpPortNumber();
    DebugPrintf(SV_LOG_INFO,"Cx HTTP Port Number = %s\n",strCXHttpPortNumber.c_str());
	if (strCXHttpPortNumber.empty())
	{
		strErrorMsg = "Failed to get the CX HTTP Port Number. For extended Error information check EsxUtil.log in MT";
		strFixStep = string("Check the Vxagnet is pointed to CX or not and availability of CX ") + 
						string("Restart the vxagent and rerun the job again");

		DebugPrintf(SV_LOG_ERROR,"Failed to get the CX HTTP Port Number.\n");
 	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	// Appending HTTP Port number to CX IP.
	strCxIpAddress += string(":") + strCXHttpPortNumber;
    
    std::string strCxCliPath = std::string(getVxInstallPath()) + std::string("bin/cxcli");
    std::string strCxUrl = m_strCmdHttps + strCxIpAddress + string("/cli/cx_cli_no_upload.php");

    // Generate XML by parsing all the disks map and post to Cx.
    std::string strDisksXMLFile = m_strDataDirPath + VOLUMES_CXCLI_FILE;

    std::string strCommand = strCxCliPath + std::string(" ") + strCxUrl + std::string(" ") + 
                             strDisksXMLFile + std::string(" ") + std::string("42");

    std::string strResultFile = m_strDataDirPath + RESULT_CXCLI_FILE;

    if(!m_vecVxJobObj.empty())
    {
		//This is to check the target Volume is registered to CX or not.
		vector<VxJobXml>::iterator iterVxJob = m_vecVxJobObj.begin();
		for( ; iterVxJob != m_vecVxJobObj.end(); iterVxJob++)
		{
			string strOutPut;
			if(false == ProcessCxCliCall(strCxCliPath, strCxUrl, iterVxJob->TargetVolumeName, true, strOutPut))
			{
				DebugPrintf(SV_LOG_ERROR,"Volume : %s is not registered.\n",iterVxJob->TargetVolumeName.c_str());
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Volume : %s is registered.\n", iterVxJob->TargetVolumeName.c_str());
			}
		}
		
		//Creats the xml file for setting up the replication pair
        if(false == MakeXmlForCxcli(m_vecVxJobObj, strDisksXMLFile))
        {
			strErrorMsg = "Failed to make the XML for registered volumes in CX. For extended Error information check EsxUtil.log in MT";
			strFixStep = "Rerun the job, if issue persists contact inmage customer support";

            DebugPrintf(SV_LOG_ERROR,"Failed to make the XML for registered volumes.\n");
 	        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return false;
        }
        
		for(int retry = 0; retry < 3; retry++)
		{
			// Post the XML to Cx.        
			if(false == ExecuteCmdWithOutputToFile(strCommand, strResultFile))
			{
				strErrorMsg = "Failed to post the XML to Cx using Cxcli. For extended Error information check EsxUtil.log in MT";
				strFixStep = string("Check the CX availablity, Ping from MT, Restart the vxagent and rerun the job again");

				if(retry < 2)
				{
					DebugPrintf(SV_LOG_INFO,"Once again will retry Creation of pairs after 1 min.\n");
					sleep(60);
					continue;
				}

				DebugPrintf(SV_LOG_ERROR,"Failed to post the XML to Cx using Cxcli.\n");
 				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}

			if(false == ProcessCxcliOutputForVxPairs(strResultFile))
			{
				DebugPrintf(SV_LOG_INFO,"ProcessCxcliOutputForVxPairs failed.\n");
				if(retry < 2)
				{
					DebugPrintf(SV_LOG_INFO,"Once again will retry Creation of pairs after 1 min.\n");
					sleep(60);
					continue;
				}
				//Display manual steps to be done by user if any volume pairs are not set                                
				DebugPrintf(SV_LOG_ERROR,"\n******************************** Important Information *********************************\n\n");
				DebugPrintf(SV_LOG_ERROR," Failed to set replication pairs for some of the Vx pairs.\n");
				DebugPrintf(SV_LOG_ERROR," Set the replication pairs manually for the failed Vx pairs.\n");
				DebugPrintf(SV_LOG_ERROR,"                              Or              \n");
				DebugPrintf(SV_LOG_ERROR," Run the following command on master target : \n");
				DebugPrintf(SV_LOG_ERROR," %s\n\n",strCommand.c_str());
				DebugPrintf(SV_LOG_ERROR,"****************************************************************************************\n\n");

				strErrorMsg = "Failed to set replication pairs for some of the Vx pairs. For extended Error information check EsxUtil.log in MT";
				strFixStep = string("Set the replication pairs manually for the failed Vx pairs (or) Run the following command on master target :\n") + strCommand + string("\n");

				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;            
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Successfully set the Vx pairs on CX.\n\n");
				break;
			}
		}
    }
    else
    {
		strErrorMsg = "List of disks to be replicated is empty. For extended Error information check EsxUtil.log in MT";
		strFixStep = string("Check the selected disks are proper in the inmage_scsi_unit_dissk.txt file, correct it and rerun the job");

        DebugPrintf(SV_LOG_ERROR,"List of disks to be replicated is empty.\n");
 	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

 	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//Create the XML in new format for 5.5 to support Batch Resync feature
//Input - Vector of VxJob Attributes 
//Input - File Name for the XML to be created
//Returns true if "All is Well"
bool LinuxReplication::MakeXmlForCxcli(std::vector<VxJobXml> vecVxVolInfo, std::string strVolFileName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    
	//Fetching the plan name from ESX.xml
    std::string strPlanName;
    std::string strBatchResyncCount;
    string strUseNatIpForSrc = string("no");
    string strUseNatIpForTgt = string("no");
    if(false == FetchVxCommonDetails(strPlanName,strBatchResyncCount,strUseNatIpForSrc,strUseNatIpForTgt))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to fetch the Plan Name and Batch Resync value required to set the volume pairs.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
	
    ofstream outXml(strVolFileName.c_str(),std::ios::out);
    if(!outXml.is_open())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s\n",strVolFileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    std::string vx_xml_data_start1 = string("<plan><header><parameters><param name='name'>");
    std::string vx_xml_data_start2 = string("</param><param name='type'>DR Protection</param><param name='version'>1.1</param></parameters></header><application_options name=\"APM\"><batch_resync>");
    std::string vx_xml_data_start3 = string("</batch_resync></application_options><failover_protection><replication>");    
    std::string vx_xml_data_start  = vx_xml_data_start1 + strPlanName + vx_xml_data_start2 + strBatchResyncCount + vx_xml_data_start3;
    outXml<<vx_xml_data_start<<endl; //Write the start part to XML file

	string vx_xml_data_job1, vx_xml_data_job2, vx_xml_data_job3;
	if(m_vConVersion >= 4.0)
	{
		vx_xml_data_job1 = string("<job type=\"vx\"><server_hostid type=\"source\">"); //Add source inmage hostid after this
		vx_xml_data_job2 = string("</server_hostid><server_hostid type=\"target\">");//Add Target inmage host id
		vx_xml_data_job3 = string("</server_hostid><source_device>");//Add source volume
	}
	else
	{
		vx_xml_data_job1 = string("<job type=\"vx\"><server_ip type=\"source\">"); //Add source IP after this
		vx_xml_data_job2 = string("</server_ip><server_ip type=\"target\">");//Add Target IP
		vx_xml_data_job3 = string("</server_ip><source_device>");//Add source volume
	}
    string vx_xml_data_job4 = string("</source_device><target_device>");//Add target volume
    string vx_xml_data_job5 = string("</target_device><process_server_ip use_nat_ip_for_src=\"");//Add strUseNatIpForSrc
    string vx_xml_data_job51 = string("\" use_nat_ip_for_trg=\"");//Add strUseNatIpForTgt
    string vx_xml_data_job52 = string("\">");//Add PS IP
    string vx_xml_data_job6 = string("</process_server_ip><options rawsize_required=\"Yes\" resync_required=\"Yes\" secure_src_to_ps=\"");
	string vx_xml_data_job61 = string("\" secure_ps_to_tgt=\"");
	string vx_xml_data_job62 = string("\" sync_option=\"fast\" compression=\"");
	string vx_xml_data_job63 = string("\"><retention retain_change_writes=\"all\" disk_space_alert_threshold=\"80\" insufficient_disk_space=\"purge_old_logs\" log_data_vol=\""); //Add log data dir volume name
    string vx_xml_data_job7 = string("\" log_data_path=\"");//Add retention log directory path
    string vx_xml_data_job8 = string("\"><space_in_mb>");//Add space in MB
    string vx_xml_data_job9 = string("</space_in_mb><retention_policy><retain_change_time unit=\"days\" value=\"");//Add retention time in days
	string vx_xml_data_job91 = string("</space_in_mb><retention_policy><retain_change_time unit=\"hours\" value=\"");//Add retention time in hours
    string vx_xml_data_job10 = string("\"></retain_change_time>");
	string vx_xml_data_job101 = string("<sparse_enabled>yes</sparse_enabled><time>");
	string vx_xml_data_job11 = string("</time></retention_policy></retention></options></job>");//End of Job
    
    //For each pair, add the job into XML.
    vector<VxJobXml>::iterator iterVxJob = vecVxVolInfo.begin();
    for( ; iterVxJob != vecVxVolInfo.end(); iterVxJob++)
    {
        //Removing the trailing characters from volumes (Ex: C:\ to C)
        std::string strTempSrcVol = iterVxJob->SourceVolumeName;
        std::string strTempTgtVol = iterVxJob->TargetVolumeName;

		string vx_xml_data_job;
		if(m_vConVersion >= 4.0)
			vx_xml_data_job = vx_xml_data_job1 + iterVxJob->SourceInMageHostId + vx_xml_data_job2 + iterVxJob->TargetInMageHostId;
		else
			vx_xml_data_job = vx_xml_data_job1 + iterVxJob->SourceMachineIP + vx_xml_data_job2 + iterVxJob->TargetMachineIP;

		vx_xml_data_job = vx_xml_data_job +
                          vx_xml_data_job3 + strTempSrcVol +
                          vx_xml_data_job4 + strTempTgtVol +
                          vx_xml_data_job5 + iterVxJob->IsSourceNatted +
						  vx_xml_data_job51 + iterVxJob->IsTargetNatted +
                          vx_xml_data_job52 + iterVxJob->ProcessServerIp +
						  vx_xml_data_job6 + iterVxJob->SecureSrcToPs +
						  vx_xml_data_job61 + iterVxJob->SecurePsToTgt +
						  vx_xml_data_job62 + iterVxJob->Compression +
                          vx_xml_data_job63 + iterVxJob->RetentionLogVolumeName +
                          vx_xml_data_job7 + iterVxJob->RetentionLogPath +
                          vx_xml_data_job8 + iterVxJob->RetainChangeMB;

		if(iterVxJob->RetainChangeDays.empty())
		{
			if(iterVxJob->RetainChangeHours.empty())
				vx_xml_data_job = vx_xml_data_job + vx_xml_data_job9 + string("3") + vx_xml_data_job10;
			else
				vx_xml_data_job = vx_xml_data_job + vx_xml_data_job91 + iterVxJob->RetainChangeHours + vx_xml_data_job10;
		}
		else
			vx_xml_data_job = vx_xml_data_job + vx_xml_data_job9 + iterVxJob->RetainChangeDays + vx_xml_data_job10;

		vx_xml_data_job = vx_xml_data_job + vx_xml_data_job101;
		
		list<SparseRet>::iterator sparseiter = iterVxJob->sparseTime.begin();
		for(; sparseiter != iterVxJob->sparseTime.end(); sparseiter++)
		{
			string vx_xml_data_job12 = string("<") + sparseiter->strTimeWindow + string(" unit=\"") + sparseiter->strUnit + 
										string("\" value=\"") + sparseiter->strValue + 
										string("\" bookmark_count=\"") + sparseiter->strBookMark;
			if(!sparseiter->strTimeRange.empty())
				vx_xml_data_job12 = vx_xml_data_job12 +	string("\" range=\"") + sparseiter->strTimeRange;
			vx_xml_data_job12 = vx_xml_data_job12 + string("\"></") + sparseiter->strTimeWindow + string(">");
			vx_xml_data_job = vx_xml_data_job + vx_xml_data_job12;
		}
		vx_xml_data_job = vx_xml_data_job + string("<application_name customtag=\"\">") + iterVxJob->RetTagType + 
							string("</application_name>") + vx_xml_data_job11;

        outXml<<vx_xml_data_job<<endl;
    }
    
    
    std::string vx_xml_data_end = string("</replication></failover_protection></plan>");
    outXml<<vx_xml_data_end<<endl;//Write the end part to XML file

    outXml.close();

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

bool LinuxReplication::ExecuteCmdWithOutputToFile(const std::string Command, const std::string OutputFile)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    
    bool rv = true;

    DebugPrintf(SV_LOG_INFO, "Command     = %s\n", Command.c_str());
    DebugPrintf(SV_LOG_INFO, "Output File = %s\n", OutputFile.c_str());
	
	std::string strDummyInputFile = std::string(getVxInstallPath()) + std::string("/failover_data/dummyinput.txt");

    ACE_Process_Options options;
    options.command_line("%s",Command.c_str());

	ACE_HANDLE handle = ACE_OS::open(OutputFile.c_str(), O_TRUNC | O_RDWR | O_CREAT);
	ACE_HANDLE handle1 = ACE_OS::open(strDummyInputFile.c_str(), O_TRUNC | O_RDWR | O_CREAT);

	if(handle == ACE_INVALID_HANDLE)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to create a handle to open the file - %s.\n",OutputFile.c_str());
		rv = false;
	}
	else if(handle1 == ACE_INVALID_HANDLE)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to create a handle to open dummy input file - %s.\n",strDummyInputFile.c_str());
		rv = false;
	}
	else
	{
		if(options.set_handles(handle1, handle, handle) == -1)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to set handles for STDOUT and STDERR.\n");
			rv = false; 
		}
		else
		{
			ACE_Process_Manager* pm = ACE_Process_Manager::instance();
			if (pm == NULL)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get process manager instance. Error - [%d - %s].\n", ACE_OS::last_error(), ACE_OS::strerror(ACE_OS::last_error())); 
				rv = false;
			}
			else
			{
				pid_t pid = pm->spawn(options);
				if (pid == ACE_INVALID_PID)
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to spawn a process for executing the command. Error - [%d - %s].\n", ACE_OS::last_error(), ACE_OS::strerror(ACE_OS::last_error())); 
					rv = false;
				}
				else
				{
					ACE_exitcode status = 0;
					pid_t rc = pm->wait(pid, &status); // wait for the process to finish
					if (0 != status)
					{
						DebugPrintf(SV_LOG_ERROR,"Command failed with exit status - %d\n", status);
						rv = false;
					}
				}
			}
			options.release_handles();
		}
	}

	if(handle != ACE_INVALID_HANDLE)
		ACE_OS::close(handle);

	if(handle1 != ACE_INVALID_HANDLE)
		ACE_OS::close(handle1);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return rv;
}

// Function to check if any Vx Pair is not set 
// As suggested in Bug 13726, checking for following string in cxcli ouput.
//      "Failed to Create Volume Replication"
// But we need to change to correct way later.
// Return true only if all pairs are set and 
// Return false in all other cases.
bool LinuxReplication::ProcessCxcliOutputForVxPairs(std::string strCxCliOutputXml)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    

    string strResultString; //fetch from xml

    //Parse the xml and get the result string.
    xmlDocPtr xDoc;	
	xmlNodePtr xCur;

    if(false == checkIfFileExists(strCxCliOutputXml))
    {
        DebugPrintf(SV_LOG_ERROR,"The file - %s does not exist.\n",strCxCliOutputXml.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
    }

	xDoc = xmlParseFile(strCxCliOutputXml.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to parse the XML file - %s\n",strCxCliOutputXml.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	//Found the doc. Now process it.
	xCur = xmlDocGetRootElement(xDoc);
	if (xCur == NULL)
	{
        DebugPrintf(SV_LOG_ERROR,"%s is empty. Cannot Proceed further.\n",strCxCliOutputXml.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"plan")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The %s is not in expected format : <plan> entry not found\n",strCxCliOutputXml.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

    xCur = xGetChildNode(xCur,(const xmlChar*)"failover_protection");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s is not in expected format : <failover_protection> entry not found\n",strCxCliOutputXml.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;			
	}

    xCur = xGetChildNode(xCur,(const xmlChar*)"vx");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s is not in expected format : <vx> entry not found\n",strCxCliOutputXml.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;			
	}

    xmlChar *key;
    key = xmlNodeListGetString(xDoc,xCur->children,1);
    DebugPrintf(SV_LOG_INFO,"The CxCli Output string - \n\n");
    DebugPrintf(SV_LOG_INFO,"%s\n\n",key);
    strResultString = string((char *)key);
    if(string::npos != strResultString.find("Failed to Create Volume Replication") || string::npos != strResultString.find("Failed to create Volume Replication"))
    {
        DebugPrintf(SV_LOG_ERROR,"Some of the Volume pairs are not set.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
    }
	else if(string::npos != strResultString.find("job creation failed"))
	{
		DebugPrintf(SV_LOG_ERROR,"Some of the Volume pairs are not set.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//Parse the Esx.xml and fetch the plan name and Batch resync from it.
//Output - Plan Name and Batch resync value
//Returns true if 'All is Well'
//Sample xml -
//  <root CX_IP="10.0.79.50" plan="PlanND" batchresync="3" isSourceNatted="False" isTargetNatted="True">
//  ..........
//  </root>
bool LinuxReplication::FetchVxCommonDetails(std::string &strPlanName, 
                                            std::string &strBatchResyncCount,
                                            std::string &strUseNatIpForSrc,
                                            std::string &strUseNatIpForTgt)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool bRetValue = true;
    do
    {
	    xmlDocPtr xDoc;	
	    xmlNodePtr xCur;	

	    xDoc = xmlParseFile(m_strXmlFile.c_str());
	    if (xDoc == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"Esx.XML document is not Parsed successfully.\n");
	        bRetValue = false;
            break;
	    }

	    //Found the doc. Now process it.
	    xCur = xmlDocGetRootElement(xDoc);
	    if (xCur == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"ESX.xml is empty. Cannot Proceed further.\n");
	        bRetValue = false;
            break;
	    }

	    if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	    {
		    //root is not found
		    DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <root> entry not found\n");
	        bRetValue = false;
            break;
	    }

	    //If you are here root entry is found. Go for plan which is an attribute in root.
	    xmlChar *xPlan_value;
        xPlan_value = xmlGetProp(xCur,(const xmlChar*)"plan");
        if(NULL == xPlan_value)
        {
		    DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <root plan> entry not found\n");
            bRetValue = false;
            break;
        }
        else
        {
            strPlanName = std::string((char *)xPlan_value);
            if(strPlanName.empty())
            {
                DebugPrintf(SV_LOG_ERROR,"Found an empty Plan name in ESX.xml document.\n");
                bRetValue = false;
                break;
            }
        }

        //Go for batchresync which is an attribute in root.
	    xmlChar *xBR_value;
        xBR_value = xmlGetProp(xCur,(const xmlChar*)"batchresync");
        if(NULL == xBR_value)
        {
		    DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <root batchresync> entry not found\n");
            bRetValue = false;
            break;
        }
        else
        {
            strBatchResyncCount = std::string((char *)xBR_value);
            if(strBatchResyncCount.empty())
            {
                DebugPrintf(SV_LOG_ERROR,"Found an empty Batch Resync value in ESX.xml document.\n");
                bRetValue = false;
                break;
            }
        }
        
        //Go for isSourceNatted which is an attribute in root.
	    xmlChar *xSN_value;
        xSN_value = xmlGetProp(xCur,(const xmlChar*)"isSourceNatted");
        if(NULL != xSN_value)
        {
		    if (0 == xmlStrcasecmp(xSN_value,(const xmlChar*)"True"))
                strUseNatIpForSrc = string("yes");
        }

        //Go for isTargetNatted which is an attribute in root.
	    xmlChar *xTN_value;
        xTN_value = xmlGetProp(xCur,(const xmlChar*)"isTargetNatted");
        if(NULL != xTN_value)
        {
		    if (0 == xmlStrcasecmp(xTN_value,(const xmlChar*)"True"))
                strUseNatIpForTgt = string("yes");            
        }
    }while(0);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bRetValue;
}


bool LinuxReplication::readInMageHostIdfromEsxXmlfile(string HostName, string &strHostInMageId,bool bsrc)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	xmlDocPtr xDoc;	
	xmlNodePtr xCur;	
	
	xDoc = xmlParseFile(m_strXmlFile.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

		//Found the doc. Now process it.
	xCur = xmlDocGetRootElement(xDoc);
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"%s is empty. Cannot Proceed further.\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <root> entry not found\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	//If you are here root entry is found. Go for SRC_ESX entry.
	string xmlhandlenode;
	if(bsrc)
	{
		xmlhandlenode = "SRC_ESX";
	}
	else
	{
		xmlhandlenode = "TARGET_ESX";
	}
	xCur = xGetChildNode(xCur,(const xmlChar*)xmlhandlenode.c_str());
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format: %s entry not found\n",m_strXmlFile.c_str(),xmlhandlenode.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;			
	}
	
	xmlNodePtr xPsPtr;

	xPsPtr = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"hostname",(const xmlChar*)HostName.c_str());
	if (xPsPtr == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with hostname> entry not found\n", m_strXmlFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;	
	}

	xmlChar *attr_value_temp;
	attr_value_temp = xmlGetProp(xPsPtr,(const xmlChar*)"inmage_hostid");
	if( (attr_value_temp == NULL) || (xmlStrlen(attr_value_temp) == 0) )
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with inmage_hostid attribute> entry not found or empty\n", m_strXmlFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	strHostInMageId = string((char *)attr_value_temp);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}


//reading the source vms and master target host natip or normal ip from Esx.xml file
bool LinuxReplication::readIpAddressfromEsxXmlfile(std::string HostName, std::string & HostIp, bool bsrc)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	xmlDocPtr xDoc;	
	xmlNodePtr xCur;	
	
	xDoc = xmlParseFile(m_strXmlFile.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

		//Found the doc. Now process it.
	xCur = xmlDocGetRootElement(xDoc);
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"%s is empty. Cannot Proceed further.\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <root> entry not found\n", m_strXmlFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	//If you are here root entry is found. Go for SRC_ESX entry.
	string xmlhandlenode;
	if(bsrc)
	{
		xmlhandlenode = "SRC_ESX";
	}
	else
	{
		xmlhandlenode = "TARGET_ESX";
	}
	xCur = xGetChildNode(xCur,(const xmlChar*)xmlhandlenode.c_str());
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format: %s entry not found\n",m_strXmlFile.c_str(),xmlhandlenode.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;			
	}
	
	xmlNodePtr xPsPtr = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"inmage_hostid",(const xmlChar*)HostName.c_str());
	if (xPsPtr == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with inmage_hostid>entry not found\n", m_strXmlFile.c_str());
		xPsPtr = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"hostname",(const xmlChar*)HostName.c_str());
		if (xPsPtr == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with hostname >entry not found\n", m_strXmlFile.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;	
		}
	}

	xmlChar *attr_value_temp;
	attr_value_temp = xmlGetProp(xPsPtr,(const xmlChar*)"natIPAddress");
	if( (attr_value_temp == NULL) || (xmlStrlen(attr_value_temp) == 0) )
	{
		DebugPrintf(SV_LOG_INFO,"We are going to collect the ip_address as the NAT IP address value is NULL or empty.\n");
		attr_value_temp = xmlGetProp(xPsPtr,(const xmlChar*)"ip_address");
		if( (attr_value_temp == NULL) || (xmlStrlen(attr_value_temp) == 0) )
		{
			DebugPrintf(SV_LOG_ERROR,"The %s document is not in expected format : <host with ip_address attribute> entry not found or empty\n", m_strXmlFile.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
	}

	HostIp = string((char *)attr_value_temp);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);

	return true;
}


bool LinuxReplication::createMapperPathMap(std::map<std::string, std::string> mapSrcDiskToTgtDisk, std::map<std::string, std::string>& mapTgtMultipathToSrcDisk)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult  = true;
	m_listScsiId.clear();
	
	std::map<std::string, std::string>::iterator iter = mapSrcDiskToTgtDisk.begin();
	
	//Doing this for rewriting the partitions. If we won't do it then the sizes of the mapper device won't update.
	for(; iter != mapSrcDiskToTgtDisk.end(); iter++)
	{
		RollBackVM obj;
		if(false == obj.rescanDisk(iter->second))
		{
			DebugPrintf(SV_LOG_ERROR,"Unable to rewtrite the partition table for the device: %s\n", iter->second.c_str());
		}
	}
	
	//After updating the partition table of each protected device, we need to get the mapper path of that device.
	//For this purpose we need to reload the multipath devices
	if(false == reloadMultipathDevice())
	{
		DebugPrintf(SV_LOG_ERROR,"Unable to reload multipath devices\n");
	}
	sleep(3);
	
	iter = mapSrcDiskToTgtDisk.begin();
	for(; iter != mapSrcDiskToTgtDisk.end(); iter++)
	{
		std::string strDeviceToMultiPath;
		if(false == convertToMultiPath(iter->second, strDeviceToMultiPath)) //This will find out the mapper device for each standard device
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to convert the physical device %s into multipath\n", iter->second.c_str());
			mapTgtMultipathToSrcDisk.insert(make_pair(iter->second, iter->first));
		}
		else
		{
			mapTgtMultipathToSrcDisk.insert(make_pair(strDeviceToMultiPath, iter->first));
		}
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


bool LinuxReplication::convertDeviceIntoMultiPath(std::list<std::string> listOfPhysicaldeviceNames, std::list<std::string>& listOfMultiPathNames)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult  = true;
	m_listScsiId.clear();
	std::list<std::string>::iterator iter = listOfPhysicaldeviceNames.begin();
	
	//Doing this for rewriting the partitions. If we won't do it then the sizes of the mapper device won't update.
	for(; iter != listOfPhysicaldeviceNames.end(); iter++)
	{
		RollBackVM obj;
		if(false == obj.rescanDisk(*iter))
		{
			DebugPrintf(SV_LOG_ERROR,"Unable to rewtrite the partition table for the device: %s\n", iter->c_str());
		}
	}
	
	//After updating the partition table of each protected device, we need to get the mapper path of that device.
	//For this purpose we need to reload the multipath devices.
	if(false == reloadMultipathDevice())
	{
		DebugPrintf(SV_LOG_ERROR,"Unable to reload multipath devices\n");
	}
	sleep(3);
	
	iter = listOfPhysicaldeviceNames.begin();
	for(; iter != listOfPhysicaldeviceNames.end(); iter++)
	{
		std::string strDeviceToMultiPath;
		if(false == convertToMultiPath((*iter), strDeviceToMultiPath)) //This will find out the mapper device for each standard device
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to convert the physical device %s into multipath\n", iter->c_str());
			listOfMultiPathNames.push_back(*iter);
		}
		else
		{
			listOfMultiPathNames.push_back(strDeviceToMultiPath);
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool LinuxReplication::convertToMultiPath(std::string strPhysicalDeviceName, std::string& strMultiPathName)
{
	bool bResult = true;
	std::string strMultiPathCmd = string(getVxInstallPath()) + "/bin/inm_scsi_id ";
	strMultiPathCmd = strMultiPathCmd + strPhysicalDeviceName;	
	std::stringstream results;

	if(!executePipe(strMultiPathCmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to execute cmd %s \n", strMultiPathCmd.c_str());
		bResult = false;
	}
	else
	{
		std::string strResult = results.str();
		trim(strResult);
		m_listScsiId.push_back(strResult);
		strMultiPathName = "/dev/mapper/" + strResult;
		DebugPrintf(SV_LOG_INFO, "Physical Device Name --> %s  Corressponding Multipath Name --> %s \n", strPhysicalDeviceName.c_str(), strMultiPathName.c_str());
	}
	return bResult;
}

bool LinuxReplication::editMultiPathFile()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	std::string strMultiPathFile = "/etc/multipath.conf";
	std::string strMultiPathFileTemp = "/etc/multipath.conf.temp";
	ifstream inFile(strMultiPathFile.c_str());
	ofstream outFile(strMultiPathFileTemp.c_str());
	if(!inFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open file %s\n ",strMultiPathFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
    string strLineRead;
	string strHash;
	std::getline(inFile,strLineRead);
	bool bGotBlocklist = false;
	if(strLineRead.compare("#InMage Edited Conf File") == 0)
	{
		outFile << strLineRead << endl;
		while(std::getline(inFile,strLineRead))
		{
			if(strLineRead.compare("blacklist {") == 0)
			{
				bGotBlocklist = true;
				outFile << strLineRead << endl;
				std::getline(inFile,strLineRead);
				if(strLineRead.compare("devnode \"sda$\"") == 0)
				{
					inFile.close();
					outFile.close();
					DebugPrintf(SV_LOG_INFO,"File %s already edited in previous protection.\n ",strMultiPathFile.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return true;
				}
				else
				{
					outFile << "devnode \"sda$\"" << endl;
				}
				outFile << strLineRead << endl;
			}
			else
			{
				outFile << strLineRead << endl;
			}
		}
		if(!bGotBlocklist)
		{
			outFile << "\n\n" << endl;
			outFile << "blacklist {" << endl;
			outFile << "devnode \"sda$\"" << endl;
			outFile << "}" << endl;
		}
	}
	else
	{
		outFile << "#InMage Edited Conf File" << endl;
		do
		{
			if(!strLineRead.empty())
			{
				strHash = strLineRead.at(0);
				if(strHash != "#")
				{
					strLineRead = "#" + strLineRead;
				}
			}
			outFile << strLineRead << endl;
		}while(std::getline(inFile,strLineRead));
		
		outFile << "\n\n" << endl;
		outFile << "blacklist {" << endl;
		outFile << "devnode \"sda$\"" << endl;
		outFile << "}" << endl;
	}
	
	inFile.close();
	outFile.close();
	int result;
	result = rename( strMultiPathFile.c_str() , "/etc/multipath.conf.inmage" );
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into \"/etc/multipath.conf.inmage\" \n ",strMultiPathFile.c_str());	
	}
	result = rename( strMultiPathFileTemp.c_str() , strMultiPathFile.c_str());
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strMultiPathFileTemp.c_str(), strMultiPathFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO, "Successfully edited the File: %s \n", strMultiPathFile.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

bool LinuxReplication::editLvmConfFile()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	std::string strLvmConfFile = "/etc/lvm/lvm.conf";
	std::string strLvmConfFileTemp = "/etc/lvm/lvm.conf.temp";
	ifstream inFile(strLvmConfFile.c_str());
	ofstream outFile(strLvmConfFileTemp.c_str());
	if(!inFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open file %s\n ",strLvmConfFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
    string strLineRead;
	std::getline(inFile,strLineRead);
	if(strLineRead.compare("#InMage Edited Conf File") == 0)
	{
		inFile.close();
		outFile.close();
		DebugPrintf(SV_LOG_INFO,"File %s already modified\n ",strLvmConfFile.c_str());
		return true;
	}
	outFile << "#InMage Edited Conf File" << endl;
	outFile << strLineRead << endl;
	while(std::getline(inFile,strLineRead))
	{
		outFile << strLineRead << endl;
		if((strLineRead == "devices {") || (strLineRead == "devices"))
		{
			if(strLineRead == "devices")
			{
				std::getline(inFile,strLineRead);
				outFile << strLineRead << endl;
				if(strLineRead != "{")
				{
					continue;
				}
			}
			while(std::getline(inFile,strLineRead))
			{
				if(strLineRead.find("/dev/fake") != string::npos)
				{
					outFile << "scan = [ \"/dev\" ]" << endl;
				}
				else
				{
					outFile << strLineRead << endl;
				}
				if(strLineRead.find("# By default we accept every block device:") != string::npos)
				{
					std::getline(inFile,strLineRead);
					string strIncludeList = "    filter = [ \"a|^/dev/sda$|\", \"r/.*/\" ]" ;
					outFile << strIncludeList << endl;
				}
			}
		}
	}
	
	inFile.close();
	outFile.close();
	int result;
	result = rename( strLvmConfFile.c_str() , "/etc/lvm/lvm.conf.inmage" );
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into \"/etc/lvm/lvm.conf.inmage\" \n ",strLvmConfFile.c_str());	
	}
	result = rename( strLvmConfFileTemp.c_str() , strLvmConfFile.c_str());
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strLvmConfFileTemp.c_str(), strLvmConfFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO, "Successfully edited the File: %s \n", strLvmConfFile.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

std::string LinuxReplication::filterScsiId(std::string strScsiDiskId)
{
	std::string strScsiId;
	size_t first = strScsiDiskId.find_first_of("\"");
	size_t last = strScsiDiskId.find_last_of("\"");
	if(last != string::npos)
		strScsiId = strScsiDiskId.substr(first+1, last-(first+1));
	return strScsiId;
}

bool LinuxReplication::getLinSrcNetworkInfo()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	std::string strHostName = GetHostName();
	std::string strNwXmlFile = std::string(getVxInstallPath()) + std::string("failover_data/") + strHostName + NETWORK_DETAILS_FILE;
	std::string strEtcMntPath;
	m_bRedHat = checkIfFileExists(REDHAT_RELEASE_FILE);
	m_bSuse = checkIfFileExists(SUSE_RELEASE_FILE);
	if(m_bRedHat || m_bSuse)
	{
		if(m_bRedHat)
		{
			m_strOsFlavor = "RedHat";
			if(false == getOsType(REDHAT_RELEASE_FILE))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the OS type... \n");
			}
		}
		else if(m_bSuse)
		{
			m_strOsFlavor = "Suse";
			if(false == getOsType(SUSE_RELEASE_FILE))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the OS type... \n");
			}
		}
		bool bEtcMounted = false;
		if(false == getEtcMntDevicePath(strEtcMntPath, bEtcMounted))
		{
			DebugPrintf(SV_LOG_ERROR,"Unable to Proceed further to get Network information \n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		bool bPartition = isLvOrPartition(strEtcMntPath);
		StorageInfo_t stgInfo;
		stgInfo.strFSType = m_strMntFS;
		if(bEtcMounted)
		{
			stgInfo.strIsETCseperateFS = "yes";
		}
		else
		{
			stgInfo.strIsETCseperateFS = "no";
		}
		if(bPartition)
		{
			stgInfo.strRootVolType = "FS";
			stgInfo.strPartition = strEtcMntPath;
		}
		else
		{
			stgInfo.strRootVolType = "LVM";
			std::string strLvName;
			std::string strVgName;
			
			getVgLvName(strEtcMntPath, strLvName, strVgName); //getting the LV and VG from the mapper path
			
			stgInfo.strLvName = strLvName;
			stgInfo.strVgName = strVgName;
			std::list<std::string> listDiskName;
			if(false == getPvOfVg(strVgName, listDiskName))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the Physical Volumes of the VG: %s\n", strVgName.c_str());
			}
			else
			{
				std::list<std::string>::iterator iter;
				DebugPrintf(SV_LOG_INFO, "Physical Volumes of VG %s are: ", strVgName.c_str());
				for(iter = listDiskName.begin(); iter != listDiskName.end(); iter++)
				{
					DebugPrintf(SV_LOG_INFO,"%s ", iter->c_str());
				}
				DebugPrintf(SV_LOG_INFO,"\n");
			}
			stgInfo.listDiskName = listDiskName;
		}
		if(false == getNetworkDetails())
		{
			DebugPrintf(SV_LOG_ERROR,"Unable to Proceed further to get Network information \n");
			bResult = false;
		}		
		else
		{
			if(false == writeNwInfoIntoXmlFile(strNwXmlFile, stgInfo))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed while writing the network information into file: %s\n", strNwXmlFile.c_str());
				bResult = false;
			}
		}
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Currently network change is not supported for this Linux flavor.\n");
		bResult = true;
	}
	
	DebugPrintf(SV_LOG_INFO,"Re-Registering the Volume information in CX.\n");
	
	if(false == RegisterHostUsingCdpCli())
	{
        DebugPrintf(SV_LOG_ERROR,"Failed to Register the Master Target disks using cdpcdli\n");
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool LinuxReplication::getOsType(string strOsType)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	std::string strLineRead;
	
	ifstream inFile;
	inFile.open(strOsType.c_str());
	if (inFile.is_open()) 
	{
		std::getline(inFile,strLineRead);
		m_strOsType = strLineRead;
		inFile.close();
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Failed to open the file \"/etc/redhat-release\"\n");
		bResult = false;
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool LinuxReplication::getEtcMntDevicePath(std::string& strEtcMntPath, bool& bEtcMounted)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	std::string strMntFile = "/etc/mtab";
	if(true == checkIfFileExists(strMntFile))
	{
		struct mntent *mnt;
		FILE *fptr;
		fptr = setmntent(strMntFile.c_str(), "r");
		
		while ((mnt = getmntent(fptr)))
		{
			if(strcmp(mnt->mnt_dir, "/etc") == 0)
			{
				strEtcMntPath = mnt->mnt_fsname;
				m_strMntFS = mnt->mnt_type;
				DebugPrintf(SV_LOG_INFO,"Etc folder mounted on device %s\n",strEtcMntPath.c_str());
				bEtcMounted = true;
				break;
			}
			else if(strcmp(mnt->mnt_dir, "/") == 0)
			{
				strEtcMntPath = mnt->mnt_fsname;
				m_strMntFS = mnt->mnt_type;
				DebugPrintf(SV_LOG_INFO,"Etc folder mounted on device %s\n",strEtcMntPath.c_str());
				bEtcMounted = false;
			}
		}
		endmntent(fptr);
		bResult = true;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get the File: %s ",strMntFile.c_str());
		bResult = false;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool LinuxReplication::isLvOrPartition(std::string strEtcMntPath)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bPartition;
	
	if(strEtcMntPath.find("/dev/mapper/") == std::string::npos)
	{
		bPartition = true;
	}
	else
	{
		bPartition = false;
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bPartition;
}

void LinuxReplication::getVgLvName(std::string strEtcMntPath, std::string& strLvName, std::string& strVgName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	size_t pos;
	std::string strSingHypen = "-";
	std::string strDoubHypen = "//";
	std::string strTemp = strEtcMntPath;
	std::string strMapper = "/dev/mapper/";
	if((pos = strEtcMntPath.find(strMapper)) != std::string::npos)
	{
		strTemp = strEtcMntPath.substr(pos+strMapper.length());
	}
	
	//Replacing the '-' by '/'
    while(((pos = strTemp.find(strSingHypen,pos)) != std::string::npos ) && pos < strTemp.length() )
    {
        strTemp.replace(pos,strSingHypen.length(),"/");
    }
    //Replacing the '//' by '-'
    while(((pos = strTemp.find(strDoubHypen,pos)) != std::string::npos ) && pos < strTemp.length())
    {
        strTemp.replace(pos,strDoubHypen.length(),strSingHypen);
    }
    strVgName = strTemp.substr(0, strTemp.find_first_of("/"));
    strLvName = "/dev/" + strVgName + "/" + strTemp.substr(strTemp.find_first_of("/")+1);
	
	DebugPrintf(SV_LOG_INFO,"VG Name: %s \tLV Name: %s\n",strVgName.c_str(), strLvName.c_str());

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

bool LinuxReplication::getPvOfVg(std::string strVgName, std::list<std::string>& listDiskName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	std::string cmd = "pvs | grep -w " + strVgName + "| awk \'{print $1}\'";
    stringstream results;
    if (!executePipe(cmd, results))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to execute the command: %s\n", cmd.c_str());
        return false;
    }
    while(!results.eof())
    {
        string disk;
        results >> disk;
        trim(disk);
        if(!disk.empty())
        {
			listDiskName.push_back(disk);
        }
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

bool LinuxReplication::getNetworkDetails()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult  = true;
	
	DebugPrintf(SV_LOG_INFO,"Collecting the Network details: \n");
	
    struct ifaddrs * ifAddrStruct=NULL;	//"ifaddrs" structure defined in header file "ifaddrs.h"
    struct ifaddrs * ifa=NULL;
	struct ifreq buffer;		//"ifreq" structure defined in header file "net/if.h"
    void * tmpAddrPtr=NULL;

    getifaddrs(&ifAddrStruct);
	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
    {
		// check it is IP4
		// is a valid IP4 Address
        if (ifa ->ifa_addr->sa_family==AF_INET)
        {
			if(strcasecmp("lo", ifa->ifa_name) != 0)
			{
				NetworkInfo_t nwInfo;
				char addressBuffer[INET_ADDRSTRLEN];
				
				DebugPrintf(SV_LOG_INFO,"Device: %s\n", ifa->ifa_name);
				nwInfo.strNicName = ifa->ifa_name;
				
				tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;				
				inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
				DebugPrintf(SV_LOG_INFO,"\tIP Address: %s\n", addressBuffer);
				nwInfo.strIpAddress = addressBuffer;
				
				tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr;
				inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
				DebugPrintf(SV_LOG_INFO,"\tNETMASK: %s\n", addressBuffer);
				nwInfo.strNetMask = addressBuffer;
				
				//collecting the mac address using ioctl.
				int s;
				s = socket(PF_INET, SOCK_DGRAM, 0);
				memset(&buffer, 0x00, sizeof(buffer));
				inm_strcpy_s(buffer.ifr_name, ARRAYSIZE(buffer.ifr_name), ifa->ifa_name);
				ioctl(s, SIOCGIFHWADDR, &buffer);

				string strHwAddr;
				char strTemp[2];
				for( s = 0; s < 6; s++ )
				{
					inm_sprintf_s(strTemp, ARRAYSIZE(strTemp),"%.2X",(unsigned char)buffer.ifr_hwaddr.sa_data[s]);
					if(s != 5)
					{
						strHwAddr = strHwAddr.append(strTemp);
						strHwAddr = strHwAddr + string(":");
					}
					else
					{
						strHwAddr = strHwAddr.append(strTemp);
					}
				}
				
				for(size_t i = 0; i < strHwAddr.length(); i++)
					strHwAddr[i] = tolower(strHwAddr[i]);
				DebugPrintf(SV_LOG_INFO,"\tMAC Address: %s\n", strHwAddr.c_str());
				nwInfo.strMacAddress = strHwAddr;
				DebugPrintf(SV_LOG_INFO,"\n");
				m_NetworkInfo.push_back(nwInfo);
				m_mapMacToEth.insert(make_pair(strHwAddr, nwInfo.strNicName));
			}
        }
	}	
	if(false == getNetworkDevices())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get the Network device details...Cannot proceed further\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}	
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool LinuxReplication::getNetworkDevices()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	std::string cmd  = "ifconfig -a | grep eth | awk {'print$1\"!@!@!\"$5'}";
	stringstream results;
    if (!executePipe(cmd, results))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to execute the command: %s\n", cmd.c_str());
        bResult = false;
    }
	else
	{
		while(!results.eof())
		{
			string nwDevice;
			results >> nwDevice;
			trim(nwDevice);
			if(!nwDevice.empty())
			{
				std::string Separator = std::string("!@!@!");
				size_t pos = 0;
				size_t found = nwDevice.find(Separator, pos);
				string strEth = nwDevice.substr(pos,found-pos);
				pos = found + 5;
				string strMac = nwDevice.substr(pos);
				for(size_t i = 0; i < strMac.length(); i++)
					strMac[i] = tolower(strMac[i]);
				DebugPrintf(SV_LOG_INFO,"Ethernet : [%s] --> MacAddress: [%s]\n",strEth.c_str(), strMac.c_str());
				if(m_mapMacToEth.find(strMac) == m_mapMacToEth.end())
				{
					NetworkInfo_t nwInfo;
					nwInfo.strNicName = strEth;
					nwInfo.strMacAddress = strMac;
					m_NetworkInfo.push_back(nwInfo);
				}
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}



bool LinuxReplication::writeNwInfoIntoXmlFile(std::string strNwXmlFile, StorageInfo_t stgInfo)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	FILE* pFilexml = NULL;
	pFilexml = fopen(strNwXmlFile.c_str(), "w");
	if(pFilexml == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"Error opening file: %s",strNwXmlFile.c_str());
	}
	
	xmlInitParser();
	xmlKeepBlanksDefault(0);
	xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
	xmlSetDocCompressMode (doc, 0);
	xmlNodePtr host = xmlNewNode(NULL, BAD_CAST "Host");
	xmlDocSetRootElement(doc, host);
	xmlNodePtr childnode = NULL, childnode1 = NULL;
	
	xmlNewProp(host, BAD_CAST "Os", BAD_CAST m_strOsType.c_str());
	xmlNewProp(host, BAD_CAST "Osversion", BAD_CAST "");
	xmlNewProp(host, BAD_CAST "OsType", BAD_CAST m_strOsFlavor.c_str());
	childnode = xmlNewChild(host, NULL, BAD_CAST "Storage", NULL);
	xmlNewProp(childnode, BAD_CAST "RootVolumeType", BAD_CAST stgInfo.strRootVolType.c_str());
    xmlNewProp(childnode, BAD_CAST "VgName", BAD_CAST stgInfo.strVgName.c_str());
	xmlNewProp(childnode, BAD_CAST "LvName", BAD_CAST stgInfo.strLvName.c_str());
	xmlNewProp(childnode, BAD_CAST "IsETCseperateFS", BAD_CAST stgInfo.strIsETCseperateFS.c_str());
	xmlNewProp(childnode, BAD_CAST "Partition", BAD_CAST stgInfo.strPartition.c_str());
	xmlNewProp(childnode, BAD_CAST "FileSystem", BAD_CAST stgInfo.strFSType.c_str());
	childnode1 = xmlNewChild(childnode, NULL, BAD_CAST "VG", NULL);
	xmlNewProp(childnode1, BAD_CAST "VgName", BAD_CAST stgInfo.strVgName.c_str());
	
	std::string strDisks;
	std::list<std::string>::iterator listiter = stgInfo.listDiskName.begin();
	while(listiter != stgInfo.listDiskName.end())
	{
		strDisks = strDisks + (*listiter);
		listiter++;
		if(listiter != stgInfo.listDiskName.end())
			strDisks = strDisks + ",";
	}
	
	xmlNewProp(childnode1, BAD_CAST "Disks", BAD_CAST strDisks.c_str());
	childnode = xmlNewChild(host, NULL, BAD_CAST "Network", NULL);
	std::list<NetworkInfo_t>::iterator listNwIter = m_NetworkInfo.begin();
	for(; listNwIter != m_NetworkInfo.end(); listNwIter++)
	{
		childnode1 = xmlNewChild(childnode, NULL, BAD_CAST "Nic", NULL);
		xmlNewProp(childnode1, BAD_CAST "Name", BAD_CAST listNwIter->strNicName.c_str());
		xmlNewProp(childnode1, BAD_CAST "Macaddress", BAD_CAST listNwIter->strMacAddress.c_str());
		xmlNewProp(childnode1, BAD_CAST "Ipaddress", BAD_CAST listNwIter->strIpAddress.c_str());
		xmlNewProp(childnode1, BAD_CAST "Netmask", BAD_CAST listNwIter->strNetMask.c_str());
	}
	
	xmlSaveFormatFile(strNwXmlFile.c_str(), doc,1);
	fclose(pFilexml);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

//Registers all the volumes to CX which are cyrrently available in the respective system 
bool LinuxReplication::RegisterHostUsingCdpCli()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	string CdpcliCmd = string("cdpcli --registerhost --loglevel=") + boost::lexical_cast<std::string>(SV_LOG_ALWAYS);

	DebugPrintf(SV_LOG_INFO,"\n");
    DebugPrintf(SV_LOG_INFO,"CDPCLI command : %s\n", CdpcliCmd.c_str());

	RollBackVM objRollBack;
    if(false == objRollBack.CdpcliOperations(CdpcliCmd))
    {
        DebugPrintf(SV_LOG_ERROR,"CDPCLI register host operation got failed\n");
        rv = false;
    }
	sleep(15);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

/******************************************************************************************************
This will process the CX CLI calls and Stores the output in a file Result_CxCli_<call number>.xml
Here currently we are processing two Cx CLI calls:
3--> to get the host info, from theer we will fetch the latest CX ip.
8--> to check the volume is registered or not.
******************************************************************************************************/
bool LinuxReplication::ProcessCxCliCall(const std::string strCxCliPath, const string strCxUrl, string strInput, bool isTargetVol, string &strOutput)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	std::string strOperation;
	static int iWaitCount = 1;
	if(isTargetVol)
	{		
		DebugPrintf(SV_LOG_INFO,"Volume to be searched in CX database = %s\n",strInput.c_str());
		strOperation = "8";
	}
	else
	{
		strOperation = "3";
	}
LABEL:
	std::string strXmlFile = std::string(getVxInstallPath()) + FAILOVER_DATA_FILE_NAME + ACE_DIRECTORY_SEPARATOR_CHAR_A +std::string("temp_for_CxCli.Xml");
	std::string strResultXmlFile;
	if(isTargetVol)
		strResultXmlFile = std::string(getVxInstallPath()) + FAILOVER_DATA_FILE_NAME + ACE_DIRECTORY_SEPARATOR_CHAR_A + string("Result_CxCli_8.Xml");
	else
		strResultXmlFile = std::string(getVxInstallPath()) + FAILOVER_DATA_FILE_NAME + ACE_DIRECTORY_SEPARATOR_CHAR_A + string("Result_CxCli_3.Xml");
	ofstream outfile;
	int i =0;
	for(; i < 2; i++)
	{
		outfile.open(strXmlFile.c_str(),std::ios::out | std::ios::trunc);
		if(outfile.fail())
		{
			DebugPrintf(SV_LOG_ERROR,"Unable to open file %s for writing...Retrying to open the file again.\n",strXmlFile.c_str());
			sleep(2);
		}
		else
			break;
	}
	if(i == 2)
	{
		DebugPrintf(SV_LOG_ERROR,"Unable to open file %s for writing.\n",strXmlFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return true;
	}

	outfile<<"Inmage_Temp_file_for_cxcli"<<endl;
	outfile.close();
	
	std::string strCommand = strCxCliPath + std::string(" ") + strCxUrl + std::string(" ") + 
                             strXmlFile + std::string(" ") + strOperation;
	DebugPrintf(SV_LOG_INFO,"Command to run = %s\n", strCommand.c_str());
	
	// Post the XML to Cx.        
    if(false == ExecuteCmdWithOutputToFile(strCommand, strResultXmlFile))
    {
		DebugPrintf(SV_LOG_ERROR,"Failed to post the XML to Cx using Cxcli.\n");
 	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

	bool bMatchFound;
	if(isTargetVol)
	{
		bMatchFound = bSearchVolume(strResultXmlFile,strInput);
		DebugPrintf(SV_LOG_INFO,"bMatchFound = %d\n",bMatchFound);	
		if(bMatchFound )
		{
			DebugPrintf(SV_LOG_INFO,"Volume : %s found.\n",strInput.c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Volume : %s is not registered with CX till now. Going to sleep for 60 seconds and check after that.\n",strInput.c_str());		
			if(iWaitCount >= 10) //10 * 60 sec = 10 mins.
			{
				DebugPrintf(SV_LOG_ERROR,"Reached the limit of wait time(10 minutes) overall. So no more retries.\n");
				bResult = false;
			}
			else
			{
				sleep(60);//60 seconds.
				iWaitCount++;
				remove(strResultXmlFile.c_str());
				goto LABEL;
			}
		}
	}
	else
	{
		bMatchFound = GetHostIpFromXml(strResultXmlFile, strInput, strOutput);
		DebugPrintf(SV_LOG_INFO,"bMatchFound = %d\n",bMatchFound);	
		if(bMatchFound )
		{
			DebugPrintf(SV_LOG_INFO,"Got the IP %s For host %s using CXCLI call 3\n",strOutput.c_str(), strInput.c_str());
		}
		else
		{
			bResult = false;
		}
	}
	
	remove(strResultXmlFile.c_str());
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


/* Searches for volume in the given filename which is expected in following format.
<xml>
<host id='052AD1D7-C93E-1845-B0FDF3AF57F14BB7'>
....
<logical_volumes>
<volume>
  <primary_volume>C:\ESX\w2k3-std01_C</primary_volume>
  ....
</volume>
<volume>
....
</volume>
</logical_volumes>
</host>
</xml>
    Returns true if volume exists , else false.
*/
bool LinuxReplication::bSearchVolume(string FileName, string VolumeName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    
	bool bResult = false;

	DebugPrintf(SV_LOG_INFO,"Volume to be searched - [%s].\n",VolumeName.c_str());

    do
    {
	    xmlDocPtr xDoc;	
	    xmlNodePtr xCur;

	    xDoc = xmlParseFile(FileName.c_str());
	    if (xDoc == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"XML document(%s) is not Parsed successfully.\n",FileName.c_str());
		    bResult = false;
            break;
	    }

	    //Found the doc. Now process it.
	    xCur = xmlDocGetRootElement(xDoc);
	    if (xCur == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"XML document(%s) is empty. Cannot Proceed further.\n",FileName.c_str());
		    bResult = false;
            break;
	    }

	    if (xmlStrcmp(xCur->name,(const xmlChar*)"xml")) 
	    {
		    //root is not found
		    DebugPrintf(SV_LOG_ERROR,"The XML document(%s) is not in expected format : <xml> entry not found.\n",FileName.c_str());
		    bResult = false;
            break;
	    }

	    //If you are here root entry is found. Go for host entry it id=Host ID.
	    xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"id",(const xmlChar*)m_strHostID.c_str());
	    if (xCur == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"The XML document(%s) is not in expected format : <host with required id> entry not found.\n",FileName.c_str());
		    bResult = false;
            break;			
	    }

	    //go to logical_volumes child.
	    xmlNodePtr xLogVolsPtr = xGetChildNode(xCur,(const xmlChar*)"logical_volumes");
	    if (xLogVolsPtr == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"The XML document(%s) is not in expected format : <logical_volumes> entry not found.\n",FileName.c_str());
		    bResult = false;
            break;
	    }

	    //We are at required volumes stage, so read all volumes until we find the required one.
	    //we need primary_volume child under volume.
	    xmlNodePtr xVolPtr = xLogVolsPtr->children;
	    while(xVolPtr != NULL)
	    {
		    if (!xmlStrcmp(xVolPtr->name, (const xmlChar*)"volume"))
		    {
			    xmlNodePtr xCurPtr = xVolPtr->children;
			    while(xCurPtr != NULL)
			    {
				    if (!xmlStrcmp(xCurPtr->name, (const xmlChar*)"primary_volume"))
				    {
					    xmlChar *key;
					    key = xmlNodeListGetString(xDoc,xCurPtr->children,1);
					    DebugPrintf(SV_LOG_INFO,"key = [%s]\n",key);
					    if(!xmlStrcasecmp(key,(const xmlChar*)VolumeName.c_str()))
					    {
						    DebugPrintf(SV_LOG_INFO,"Found the volume.\n");
						    bResult = true;
					    }
					    xmlFree(key);                    
					    break;
				    }				
				    xCurPtr = xCurPtr->next;				
			    }
			    if(bResult)
				    break;
		    }
		    xVolPtr = xVolPtr->next;
	    }
    }while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


/*************************************************************************************************
Searches for HostName and its IP in the given filename which is expected in following format.
<hosts>
<host id='2a90357d-48d2-453c-86b2-fc0e46f32a3b'>
  <name>CENTOS-CS</name>
  <ip_address>10.0.24.2</ip_address>
  <ip_addresses>10.0.24.2</ip_addresses>
  <os_type>LINUX/UNIX</os_type>
  <vx_agent_path>/usr/local/InMage/Vx/</vx_agent_path>
  <fx_agent_path>/usr/local/InMage/Fx</fx_agent_path>
  <vx_patch_history></vx_patch_history>
  <fx_patch_history></fx_patch_history>
  <agent_time_zone>+0530</agent_time_zone>
</host>
................................
................................
</hosts>
*************************************************************************************************/
bool LinuxReplication::GetHostIpFromXml(string FileName, string strHost, string &strHostIP)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	DebugPrintf(SV_LOG_INFO,"Host IP to be searched for Host - [%s].\n",strHost.c_str());

    do
    {
	    xmlDocPtr xDoc;	
	    xmlNodePtr xCur;

	    xDoc = xmlParseFile(FileName.c_str());
	    if (xDoc == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"XML document(%s) is not Parsed successfully.\n",FileName.c_str());
		    bResult = false;
            break;
	    }

	    //Found the doc. Now process it.
	    xCur = xmlDocGetRootElement(xDoc);
	    if (xCur == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"XML document(%s) is empty. Cannot Proceed further.\n",FileName.c_str());
		    bResult = false;
            break;
	    }

	    if (xmlStrcmp(xCur->name,(const xmlChar*)"hosts")) 
	    {
		    //root is not found
		    DebugPrintf(SV_LOG_ERROR,"The XML document(%s) is not in expected format : <hosts> entry not found.\n",FileName.c_str());
		    bResult = false;
            break;
	    }

	    //we need hostname and host ip from host entry
	    xmlNodePtr xHostPtr = xCur->children;
	    while(xHostPtr != NULL)
	    {
		    if (!xmlStrcmp(xHostPtr->name, (const xmlChar*)"host"))
		    {
				string strHostName;
			    //go to logical_volumes child.
				xmlChar *attr_value_temp;
				xmlNodePtr xChildNode = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"id",(const xmlChar*)strHost.c_str());
				if (xChildNode == NULL)
				{
					DebugPrintf(SV_LOG_INFO,"The XML document(%s) is not in expected format : <id> entry not found.\n",FileName.c_str());
					xChildNode = xGetChildNode(xHostPtr,(const xmlChar*)"name");
					if (xChildNode == NULL)
					{
						DebugPrintf(SV_LOG_ERROR,"The XML document(%s) is not in expected format : <name> entry not found.\n",FileName.c_str());
						bResult = false;
						break;
					}
				}
				attr_value_temp = xmlNodeListGetString(xDoc,xChildNode->children,1);
				strHostName = string((char *)attr_value_temp);
				
				if(0 == strcasecmp(strHost.c_str(), strHostName.c_str()))
				{
					xChildNode = xGetChildNode(xHostPtr,(const xmlChar*)"ip_address");
					if (xChildNode == NULL)
					{
						DebugPrintf(SV_LOG_ERROR,"The XML document(%s) is not in expected format : <ip_address> entry not found.\n",FileName.c_str());
						bResult = false;
						break;
					}
					attr_value_temp = xmlNodeListGetString(xDoc,xChildNode->children,1);
					strHostIP = string((char *)attr_value_temp);
					if(strHostIP.empty())
					{
						DebugPrintf(SV_LOG_ERROR,"Empty IP value found for host : %s", strHostName.c_str());
						bResult = false;
						break;
					}
					else
					{
						DebugPrintf(SV_LOG_INFO,"Got the IP %s for host : %s\n", strHostIP.c_str(), strHostName.c_str());
						break;
					}
				}
		    }
		    xHostPtr = xHostPtr->next;
	    }
    }while(0);

	if(strHostIP.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"CXCLI call had not return the IP value for host : %s \n", strHost.c_str());
		bResult = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


/********************************************************************************************
It will take Source hostname as input and return the source Latest IP from CX.
For this it will do a CX CLI call(number 3). This will give some information for the source.
e.g. 
<host id='2a90357d-48d2-453c-86b2-fc0e46f32a3b'>
  <name>CENTOS-CS</name>
  <ip_address>10.0.24.2</ip_address>
  <ip_addresses>10.0.24.2</ip_addresses>
  <os_type>LINUX/UNIX</os_type>
  <vx_agent_path>/usr/local/InMage/Vx/</vx_agent_path>
  <fx_agent_path>/usr/local/InMage/Fx</fx_agent_path>
  <vx_patch_history></vx_patch_history>
  <fx_patch_history></fx_patch_history>
  <agent_time_zone>+0530</agent_time_zone>
</host>

We will filter out the ip for that host from this information.
**********************************************************************************************/
bool LinuxReplication::GetSrcIPUsingCxCli(string strGuestOsName, string &strIPAddress)
{
	bool bRet = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	//Prepare CX for posting.
    std::string strCxIpAddress = FetchCxIpAddress();
    DebugPrintf(SV_LOG_INFO,"Cx IP address = %s\n",strCxIpAddress.c_str());
    if(strCxIpAddress.empty())
    {        
	    DebugPrintf(SV_LOG_ERROR,"Failed to fetch Cx IP address.\n");
 	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	    return false;
    }
    std::string strCXHttpPortNumber = FetchCxHttpPortNumber();
    DebugPrintf(SV_LOG_INFO,"Cx HTTP Port Number = %s\n",strCXHttpPortNumber.c_str());
	if (strCXHttpPortNumber.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get the CX HTTP Port Number.\n");
 	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	// Appending HTTP Port number to CX IP.
	strCxIpAddress += string(":") + strCXHttpPortNumber;
    
    std::string strCxCliPath = std::string(getVxInstallPath()) + std::string("bin/cxcli");
    std::string strCxUrl = m_strCmdHttps + strCxIpAddress + string("/cli/cx_cli_no_upload.php");

	if(false == ProcessCxCliCall(strCxCliPath, strCxUrl,strGuestOsName, false, strIPAddress))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get the IP address using CX CLI for host %s\n",strGuestOsName.c_str());
		bRet = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}

//MakeInformation 
//Called as part of consistency.
//It makes up new Initrd image files if required. **Will generation of new initrd/initramfs file trigger tag point to be not exact?? NEED TO CHECK THIS??
//IT also collect information of source machine which is used during DR-DRILL/RECOVERY.
bool LinuxReplication::MakeInformation( Configurator* Configurator )
{
	DebugPrintf( SV_LOG_DEBUG,"Entering %s\n", __FUNCTION__ );
	bool bReturnValue = true;
	

	//Get VX installation path and build path to the scripts
	std::string str_vxInstallPath	= std::string(getVxInstallPath());
	//scripts for collecting information will be under path InMage_VX_INSTALL_PATH/scripts/vCon.
	std::string str_scriptsPath		= str_vxInstallPath + std::string("scripts/vCon/");
	//append script file name to scripts path 
	std::string str_makeInformation = str_scriptsPath + std::string(LINUXP2V_MAKE_INFORMATION) + std::string(" ") + str_scriptsPath;
	std::string str_outputFile	= str_scriptsPath + std::string("LinuxP2V.log");
	
	//Now we have to invoke script such that it collects all information of source side.
	DebugPrintf(SV_LOG_INFO, "Command = %s.\n", str_makeInformation.c_str() );
	//Is this the Right function to use? or we have make duplicate of it?
	//Need to Take care of this.
	if(false == ExecuteCmdWithOutputToFileWithPermModes( str_makeInformation, str_outputFile, O_APPEND | O_RDWR | O_CREAT))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to collect information of physical machine.\n" );
		bReturnValue	= false;
	}

	return bReturnValue;
}



//Generates initrd image on physical linux machines.
//Algo
//Determine OS type of physical machine, and check if it is listed under OS types for which initrd image has to be generated.
//If initrd image has to be generated, check if it is already generated by previous instance and is older by 4 days.If not do not generate.
//Name of the initrd image has to be .img-InMage-0001. This is Fixed.
//command to generate initrd image = mkinitrd  --with=mptspi --with=e1000 --builtin=involflt -f /boot/initrd-`uname -r`.img-InMage-0001 `uname -r`
//right now OS'es for which initrd image has to be generated are RHEL 4 and 5.No Need to generate for RHEL 6 and SLES 11.
//bool LinuxReplication::GenerateInitrdImg( Configurator* Configurator )
//{
//	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
//	bool bReturnVal = true;
//
//	do
//	{
//		//Determine OS type.
//		std::string strHostId		= Configurator->getHostId();
//		DebugPrintf(SV_LOG_INFO,"Host Id = %s.\n", strHostId.c_str());
//		
//		std::string strFunctionName = "GetHostInfo";
//		std::string strInfoType		= "Basic";
//		std::string strXml			= ConstructXmlForCxApi( strHostId , strFunctionName , strInfoType );
//		if ( strXml.empty() )
//		{
//			DebugPrintf(SV_LOG_ERROR, "Failed to construct input xml for CX api.\n");
//			bReturnVal				= false;
//			break;
//		}
//
//		std::string strOutputFile	= std::string(getVxInstallPath()) +  std::string("failover_data/") + strHostId + string("_Output.xml"); 
//		bool bIsCxApiExecuted		= processXmlData( strXml , strOutputFile );
//		if ( true != bIsCxApiExecuted )
//		{
//			DebugPrintf(SV_LOG_INFO,"Failed to query CX for required information.\n");
//			bReturnVal				= false;
//			break;
//		}
//
//		//we have output saved into a file.Read the OS information from it.We have to pass container where to store information read from CX.
//		string strOsName			= "";
//		bool bIsFunctionReqSuccess	= xmlGetOsName( strOutputFile , strOsName ); 
//		if ( true != bIsFunctionReqSuccess )
//		{
//			DebugPrintf(SV_LOG_INFO , "Either function request is failed or failed to read output from CX.\n");
//			bReturnVal				= false;
//			break;
//		}
//
//		//Now we have OS type from CX.Check if initrd image has to be generated for this OS type.
//		DebugPrintf(SV_LOG_INFO,"OS Name = %s.\n", strOsName.c_str());
//		bool bGenerateInitrdImg		= CreateInitrdImgForOS( strOsName );
//		if ( true != bGenerateInitrdImg )
//		{
//			DebugPrintf(SV_LOG_INFO , "No need to generate inited image for OS: %s.\n", strOsName.c_str());
//			break;
//		}
//
//		//here we have to construct the file Name which we need to create.
//		std::string strInitrdImgName= ""; 
//		std::string strMkinitrdCmd	= "";
//		bool bIsNameCmdConstructed	= ConstructInitrdImgNameAndCmd( strOsName , strInitrdImgName , strMkinitrdCmd );
//		if ( true != bIsNameCmdConstructed )
//		{
//			DebugPrintf( SV_LOG_ERROR , "Failed to construct initrd image name for OS \"%s\".\n" , strOsName.c_str() );
//			bReturnVal				= false;
//			break;
//		}
//		
//		if ( ( strInitrdImgName.empty() ) || ( strMkinitrdCmd.empty() ) )
//		{
//			DebugPrintf( SV_LOG_ERROR , "Failed to construct initrd image name for OS \"%s\".\n" , strOsName.c_str() );
//			bReturnVal				= false;
//			break;
//		}
//
//		//initrd image has to be generated for this OS type.
//		DebugPrintf(SV_LOG_INFO , "custom initrd image file name = %s.\n" , strInitrdImgName.c_str() );
//		bool bValidateExistingImg	= ValidateExistingInitrdImg( strInitrdImgName.c_str() );
//		if ( true == bValidateExistingImg )
//		{
//			//Already a healthy intird image exists.
//			DebugPrintf(SV_LOG_INFO , "initrd image already exists.\n");
//			break;
//		}
//
//		//Pre steps to be done before creating the Initrd image. Depends on OS Name.
//		bool bIsPreStepsPerformed	= PreCreateInitrdImg( strOsName , strInitrdImgName );
//		if ( true != bIsPreStepsPerformed )
//		{
//			DebugPrintf( SV_LOG_INFO , "Failed to perform pre-steps of creating initrd img." );
//			bReturnVal				= false;
//			break;
//		}
//		
//		bool bCreateInitrdImg		= CreateInitrdImg( strMkinitrdCmd );
//		if ( true != bCreateInitrdImg )
//		{
//			DebugPrintf(SV_LOG_INFO , "Failed to create initrd image.\n");
//			bReturnVal				= false;
//		}
//
//		//post Conditions to be done after creating the initrd img file.
//		bool bIsPostStepsPerformed	= PostCreateInitrdImg( strOsName , strInitrdImgName );
//		if ( true != bIsPostStepsPerformed )
//		{
//			DebugPrintf( SV_LOG_INFO , "Failed to perform post-steps of creating initrd img." );
//			bReturnVal				= false;
//			break;
//		}
//		
//		if ( true == bReturnVal ) 
//		{
//			DebugPrintf( SV_LOG_INFO , "Successfully Created initrd image. Return code = %d.\n", bReturnVal );
//			sleep(10);
//		}
//	}while(0);
//	DebugPrintf(SV_LOG_DEBUG , "Exiting %s.\n",__FUNCTION__);
//	return bReturnVal;
//}

bool LinuxReplication::PreCreateInitrdImg( std::string strOsName , std::string strInitrdImgName )
{
	DebugPrintf( SV_LOG_DEBUG , "Entering function %s.\n" , __FUNCTION__ );
	bool bReturnVal		= true;

	if ( std::string::npos != strOsName.find("SUSE") )
	{
		std::string strOldFileName = strInitrdImgName.substr( 0 , strInitrdImgName.find_last_of(".") );
		std::string strNewFileName = strInitrdImgName.substr( 0 , strInitrdImgName.find_last_of(".") ) + std::string(".backup_inmage");
		DebugPrintf(SV_LOG_INFO, "Renaming file %s to %s.\n", strOldFileName.c_str() , strNewFileName.c_str() );
		int iResult = rename( strOldFileName.c_str() , strNewFileName.c_str() );
		if( 0 != iResult )
		{
			DebugPrintf( SV_LOG_ERROR , "Failed to rename file %s to %s", strOldFileName.c_str() , strNewFileName.c_str() );
			bReturnVal = false;
		}
	}

	DebugPrintf( SV_LOG_DEBUG , "Exiting function %s.\n" , __FUNCTION__ );
	return bReturnVal;
}

bool LinuxReplication::PostCreateInitrdImg( std::string strOsName , std::string strInitrdImgName )
{
	DebugPrintf( SV_LOG_DEBUG , "Entering function %s.\n" , __FUNCTION__ );
	bool bReturnVal		= true;

	if ( std::string::npos != strOsName.find("SUSE") )
	{
		std::string strOldFileName = strInitrdImgName.substr( 0 , strInitrdImgName.find_last_of(".") );
		std::string strNewFileName = strInitrdImgName;
		DebugPrintf(SV_LOG_INFO, "Renaming file %s to %s.", strOldFileName.c_str() , strNewFileName.c_str() );
		int iResult = rename( strOldFileName.c_str() , strNewFileName.c_str() );
		if( 0 != iResult )
		{
			DebugPrintf( SV_LOG_ERROR , "Failed to rename file %s to %s", strOldFileName.c_str() , strNewFileName.c_str() );
			bReturnVal = false;
		}

		strOldFileName = strInitrdImgName.substr( 0 , strInitrdImgName.find_last_of(".") ) + std::string(".backup_inmage");
		strNewFileName = strInitrdImgName.substr( 0 , strInitrdImgName.find_last_of(".") );
		DebugPrintf(SV_LOG_INFO, "Renaming file %s to %s.", strOldFileName.c_str() , strNewFileName.c_str() );
		iResult = rename( strOldFileName.c_str() , strNewFileName.c_str() );
		if( 0 != iResult )
		{
			DebugPrintf( SV_LOG_ERROR , "Failed to rename file %s to %s", strOldFileName.c_str() , strNewFileName.c_str() );
			bReturnVal = false;
		}
	}

	DebugPrintf( SV_LOG_DEBUG , "Exiting function %s.\n" , __FUNCTION__ );
	return bReturnVal;
}

bool LinuxReplication::ConstructInitrdImgNameAndCmd( std::string strOsName , std::string& strInitrdImgName , std::string& strMkinitrdCmd )
{
	DebugPrintf( SV_LOG_DEBUG , "Entering function %s.\n" , __FUNCTION__ );
	bool bReturnVal				= true;
	do
	{
		string strKernelVersion	= GetKernelVersion();
		if ( strKernelVersion.empty() )
		{
			DebugPrintf(SV_LOG_ERROR,"Unable to find kernerl version of OS \"%s\".\n", strOsName.c_str() );
			bReturnVal			= false;
			break;
		}

		if ( string::npos != strOsName.find( std::string("SUSE") ) )
		{
			strInitrdImgName	= std::string("/boot/initrd-") + strKernelVersion + std::string(".InMage-0001");
			strMkinitrdCmd		= std::string("mkinitrd  -m \"mptspi e1000\"");
		}
		else
		{
			strInitrdImgName	= std::string("/boot/initrd-") + strKernelVersion + std::string(".img.InMage-0001");
			strMkinitrdCmd		= std::string("mkinitrd  --with=mptspi --with=e1000 -f ") + strInitrdImgName + std::string(" ") + strKernelVersion;
		}
	}while(0);

	DebugPrintf( SV_LOG_DEBUG , "Exiting function %s.\n" , __FUNCTION__ );
	return ( bReturnVal );
}

//xmlGetOsName -- Get's Os Name from CX output.
bool LinuxReplication::xmlGetOsName( std::string strInFileName , std::string& strOsName )
{
	DebugPrintf( SV_LOG_DEBUG , "Entering function %s.\n" , __FUNCTION__ );
	bool bReturnValue = true;

	xmlDocPtr xDoc;	
	xmlNodePtr xCur;
	m_strOsType.clear();

	xDoc = xmlParseFile(strInFileName.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	//Found the doc. Now process it.
	xCur = xmlDocGetRootElement(xDoc);
	if (xCur == NULL)
	{
        DebugPrintf(SV_LOG_ERROR,"The XML document is empty. Cannot Proceed further.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        xmlFreeDoc(xDoc); return false;
	}
	if (xmlStrcasecmp(xCur->name,(const xmlChar*)"Response")) 
	{
		//Response is not found
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Response> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
	//If you are here Response entry is found. Go for Body entry.
	xCur = xGetChildNode(xCur,(const xmlChar*)"Body");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Body> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;			
	}
	//If you are here Body entry is found. Go for Function entry with required Function name.
	xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"Function",(const xmlChar*)"Name", (const xmlChar*)"GetHostInfo");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Function with required Name> entry not found\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;			
	}
	xCur = xGetChildNode(xCur,(const xmlChar*)"FunctionResponse");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <FunctionResponse> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;			
	}
	
	xmlNodePtr xChildNode;
	if(false == xGetValueForProp(xCur, "Caption", strOsName))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to find out OS name from xml file\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
	 
	DebugPrintf( SV_LOG_DEBUG , "Exiting function %s.\n" , __FUNCTION__ );
	return bReturnValue;
}

//CreateInitrdImg -- it Creates our own initrd image.
bool LinuxReplication::CreateInitrdImg( std::string strMkinitrdCmd )
{
	DebugPrintf( SV_LOG_DEBUG , "Entering function %s.\n" , __FUNCTION__ );
	bool bReturnValue	= true;

	DebugPrintf(SV_LOG_INFO, "Command = %s.\n", strMkinitrdCmd.c_str() );
	if ( false == executeScript( strMkinitrdCmd ) )
	{
		bReturnValue	= false;
	}

	DebugPrintf( SV_LOG_DEBUG , "Exiting function %s.\n" , __FUNCTION__  );
	return bReturnValue;
}

//GetKernelVersion -- Finds Kernel version of the machine where it is running.
string LinuxReplication::GetKernelVersion()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering function %s.\n" , __FUNCTION__ );
	string strKernelVersion	= "";
	struct utsname buffer;
	int iReturnValue		= uname( &buffer );
	if ( 0 == iReturnValue )
	{
		DebugPrintf( SV_LOG_INFO , "Kernel version = %s.\n" , buffer.release );
		strKernelVersion	= buffer.release;
	}
	else
		DebugPrintf( SV_LOG_ERROR , "ERROR : %s.\n" , strerror(errno) );
	DebugPrintf(SV_LOG_DEBUG, "Exiting function %s.\n" , __FUNCTION__ );
	return strKernelVersion;
}

//ValidateExistingInitrdImg -- Validates initrd image created by InMage if any.
//If exists, it checks whether it is older by 4 days? if older, it will wipe off older and creates a new one.
//If not,it continues as it is with out creating and triggers consistency tag.
bool LinuxReplication::ValidateExistingInitrdImg( const char* fileName )
{
	DebugPrintf(SV_LOG_DEBUG,"Entering function %s.\n",__FUNCTION__);
	bool bReturnVal		= false;
	do
	{
		struct stat buffer;
		int result = stat( fileName , &buffer);
		if (result != 0)
		{
			DebugPrintf( SV_LOG_INFO, "Does file exist? : %s.\n", strerror(errno) );
			break;
		}
		
		time_t now  = time(0);
		double ul_diffTime = difftime( now , buffer.st_mtime ); 
		DebugPrintf( SV_LOG_INFO , "Difference of time in seconds from last modified time to current time = %f.\n" , ul_diffTime );
		if ( 4 < ( ul_diffTime / ( 60 * 60 * 24 ) ) )
		{
			DebugPrintf(SV_LOG_INFO ,  "File is older than 4 days.Creating a new one.\n" );
			break;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO ,  "No need to create new initrd image.\n" );
			bReturnVal = true;
			break;
		}
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting function %s.\n",__FUNCTION__);
	return ( bReturnVal );
}

//ConstructXmlForCxApi : Constructs input XML message for CX API.
std::string LinuxReplication::ConstructXmlForCxApi( std::string strHostId , std::string strFunctionName , std::string strInfoType )
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    string xml_data = string("<Request Id=\"0001\" Version=\"1.0\">");
	xml_data = xml_data + string("<Header>");
	xml_data = xml_data + string("<Authentication>");
	xml_data = xml_data + string("<AccessKeyID>DDC9525FF275C104EFA1DFFD528BD0145F903CB1</AccessKeyID>");
	xml_data = xml_data + string("<AccessSignature></AccessSignature>");  //Currently The access key here is NULL, need to write the code for it.
	xml_data = xml_data + string("</Authentication>");
	xml_data = xml_data + string("</Header>");
	xml_data = xml_data + string("<Body>");
	xml_data = xml_data + string("<FunctionRequest Name=\"") + strFunctionName + string("\" Include=\"Yes\">");
	xml_data = xml_data + string("<Parameter Name=\"HostGUID\" Value=\"") + strHostId + string("\" />");
	xml_data = xml_data + string("<Parameter Name=\"InformationType\" Value=\"") + strInfoType + string("\" />");
	xml_data = xml_data + string("</FunctionRequest>");
	xml_data = xml_data + string("</Body>");
	xml_data = xml_data + string("</Request>");
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return xml_data;
}

//Generate tags for source replicated volumes
// 1. Fetch the source replicated volumes.
// 2. Execute the consistency script to generate tags.
//Input - Target Hostname, Tag Name, bCrashConsistency
bool LinuxReplication::GenerateTagForSrcVol(std::string strTarget, std::string strTagName, bool bCrashConsistency, Configurator* TheConfigurator)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    
    string strVxInstallPath = getVxInstallPath();
	
    std::string ts = GeneratreUniqueTimeStampString();
    std::string FinalTagName = strTagName + ts;
    //This will have list of replicated source volumes in the following format.
    //Ex -  "C:;E:;C:\Mount Points\MP1;"
    /*string strSrcRepVol;
    if(false == GetSrcReplicatedVolumes(strTarget,strSrcRepVol,TheConfigurator))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to fetch the source replicated volumes.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }*/
	
	string strVacpCommand;
	if(!bCrashConsistency)
		strVacpCommand = strVxInstallPath + string("bin/vacp -systemlevel -t ") + FinalTagName;
	else
		strVacpCommand = strVxInstallPath + string("bin/vacp -x -v all -t ") + FinalTagName;
		
	string OutputFile = strVxInstallPath + string("failover_data/Vacp_consistency.txt");
	
	DebugPrintf(SV_LOG_INFO,"Command to run = %s\n", strVacpCommand.c_str());
	DebugPrintf(SV_LOG_INFO,"OutpPut file = %s\n", OutputFile.c_str());
	
	if(false == ExecuteCmdWithOutputToFile(strVacpCommand, OutputFile))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed while executing the Consistency Script.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
	}

	ifstream FIN(OutputFile.c_str(), std::ios::in);
    if(! FIN.is_open())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s\n",OutputFile.c_str());
    }
	else
	{
		std::string strLineRead;
		while(getline(FIN, strLineRead))
		{
			if(strLineRead.empty())
				continue;
			DebugPrintf(SV_LOG_INFO,"%s\n",strLineRead.c_str());
		}
		FIN.close();
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

// This func will generate unique time stamp string
string LinuxReplication::GeneratreUniqueTimeStampString()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strUniqueTimeStampString;
	stringstream ss;
	
	time_t t = time(NULL);
    struct tm tm = *localtime(&t);

	ss << (tm.tm_year + 1900) << (tm.tm_mon + 1) << tm.tm_mday << tm.tm_hour << tm.tm_min << tm.tm_sec;
	strUniqueTimeStampString = ss.str();

    DebugPrintf(SV_LOG_INFO,"strUniqueTimeStampString = %s\n",strUniqueTimeStampString.c_str());
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return strUniqueTimeStampString;
}

//Get Replicated Volumes on the source
//Input - strTarget (target hostname)
//Output - List of Replicated Volumes on Source with the given target in required format("C:;E:;C:\Mount Points\MP1;").
bool LinuxReplication::GetSrcReplicatedVolumes(string strTarget, string &strSrcRepVol, Configurator* TheConfigurator)
{ 
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    DebugPrintf(SV_LOG_INFO,"Fetching the volumes replicated from this machine to target machine : %s.\n",strTarget.c_str());
    //Fetch the replication pairs from Configurator object
    map<string,string> mapVxPairs = TheConfigurator->getSourceVolumeDeviceNames(strTarget);    
    
    strSrcRepVol.clear();
    map<string,string>::iterator iter = mapVxPairs.begin();    
    for(;iter != mapVxPairs.end();iter++)
    {
        DebugPrintf(SV_LOG_INFO,"Source Vol : %s <==> Target Vol : %s \n",iter->second.c_str(),iter->first.c_str());
        //if the volume is drive letter add : (colon) to it.
        strSrcRepVol += string(iter->second) + string(",");
    }

    if(strSrcRepVol.empty())
    {
        DebugPrintf(SV_LOG_ERROR,"No Volumes are replicated on this source with the target machine : %s.\n",strTarget.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    else
        strSrcRepVol = "\"" + strSrcRepVol + "\"";

    DebugPrintf(SV_LOG_INFO,"Source volumes under replication = %s\n",strSrcRepVol.c_str());
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

bool LinuxReplication::getMapOfMultipathToScsiId(map<string, string>& mapOfMultipathToScsiId)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	map<string, string>::iterator iterScsi;
	std::map<std::string,std::string>::iterator iter = m_mapOfScsiCont.begin();
	for(; iter != m_mapOfScsiCont.end(); iter++)
	{
		std::string strDeviceToMultiPath;
		iterScsi = m_mapLsScsiID.find(iter->first);
		if(iterScsi != m_mapLsScsiID.end())
		{
			if(false == convertToMultiPath(iter->second, strDeviceToMultiPath)) //This will find out the mapper device for each standard device
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to convert the physical device %s into multipath\n", iter->second.c_str());
				mapOfMultipathToScsiId.insert(make_pair(iter->second, iterScsi->second));
			}
			else
			{
				mapOfMultipathToScsiId.insert(make_pair(strDeviceToMultiPath, iterScsi->second));
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

bool LinuxReplication::reloadDevices(map<string, string>& mapOfMultipathToScsiId)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;

	list<string> listTargetDevice;
	map<string, string>::iterator iterMap;

	std::map<std::string, std::string>::iterator iter = m_mapVmIdToVmName.begin();
	for(; iter != m_mapVmIdToVmName.end(); iter++)
	{
		listTargetDevice.clear();
		if(false == GetListOfTargetDevice(iter->first, listTargetDevice))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the list of target devices from pinfo file for host %s\n ",iter->first.c_str());
			bResult = false;
		}
		if(!listTargetDevice.empty())
		{
			list<string>::iterator iter = listTargetDevice.begin();
			for(; iter != listTargetDevice.end(); iter++)
			{
				iterMap = mapOfMultipathToScsiId.find((*iter));
				if(iterMap != mapOfMultipathToScsiId.end())
				{
					removeLsscsiId(iterMap->second);
					addLsscsiId(iterMap->second);
				}
			}
		}
	}

	if(false == reloadMultipathDevice())
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to reload multipath devices\n");
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}


//Will get it from pinfo(if rollback) or sinfo(is snapshot) file
bool LinuxReplication::GetListOfTargetDevice(string hostName, list<string>& listTargetDevice)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;
	
	string strPinfoFile = std::string(getVxInstallPath()) + std::string("failover_data/") + hostName + PAIRINFO_EXTENSION;
	DebugPrintf(SV_LOG_INFO,"Reading the Pinfo File: %s\n",strPinfoFile.c_str());
	
	if(false == checkIfFileExists(strPinfoFile))
	{
		DebugPrintf(SV_LOG_ERROR,"File Not Found: %s\n",strPinfoFile.c_str());
		std::map<std::string, std::string>::iterator iter = m_mapVmIdToVmName.find(hostName);
		if(iter != m_mapVmIdToVmName.end())
			hostName = iter->second;
		strPinfoFile = std::string(getVxInstallPath()) + std::string("failover_data/") + hostName + PAIRINFO_EXTENSION;
	}
	else
	{
		ifstream inFile(strPinfoFile.c_str());
		if(!inFile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open file %s\n ",strPinfoFile.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		string strLineRead;
		string token = "!@!@!";
		while(getline(inFile,strLineRead))
		{
			std::string strSrcVolName;
			std::string strTgtVolName;
			std::size_t index;		
			DebugPrintf(SV_LOG_INFO,"Line read =  %s \n",strLineRead.c_str());
			index = strLineRead.find_first_of(token);
			if(index != std::string::npos)
			{
				strSrcVolName = strLineRead.substr(0,index);
				strTgtVolName = strLineRead.substr(index+(token.length()),strLineRead.length());
				DebugPrintf(SV_LOG_INFO,"Source Disk =  %s  :  Target disk =  %s\n",strSrcVolName.c_str(),strTgtVolName.c_str());            
				if((!strSrcVolName.empty()) && (!strTgtVolName.empty()))
				{
					listTargetDevice.push_back(strTgtVolName);
				}
			}
		}		
		inFile.close();
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

bool LinuxReplication::reloadMultipathDevice()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;
	stringstream results;

	string cmd = string("multipath -r");
	DebugPrintf(SV_LOG_INFO,"cmd = : %s\n", cmd.c_str());
	results.clear();
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
		rv = false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO, "Successfully ran the cmd: %s\n", cmd.c_str());
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

bool LinuxReplication::GetDiskUsage()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	stringstream results;
	string strCmdLogFile = m_strDataDirPath + string("/inmage_cmd_output.log");
	if(m_strDataDirPath.empty())
		strCmdLogFile = "/home/inmage_cmd_output.log";
	
	string cmd = string("df -k") + string(" 1>>") + strCmdLogFile + string(" 2>>") + strCmdLogFile;
	DebugPrintf(SV_LOG_INFO,"cmd = : %s\n", cmd.c_str());
	
	string cmd1 = string("echo running command : \"") + cmd + string("\" >>") + strCmdLogFile;
	if (!executePipe(cmd1, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
	}	
	results.clear();

	stringstream results1;
	if (!executePipe(cmd, results1))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO, "Successfully ran the command: %s\n",cmd.c_str());
	}
	return bResult;
}

bool LinuxReplication::GetMemoryInfo()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	stringstream results;
	string strCmdLogFile = m_strDataDirPath + string("/inmage_cmd_output.log");
	if(m_strDataDirPath.empty())
		strCmdLogFile = "/home/inmage_cmd_output.log";
	
	string cmd = string("cat /proc/meminfo") + string(" 1>>") + strCmdLogFile + string(" 2>>") + strCmdLogFile;
	DebugPrintf(SV_LOG_INFO,"cmd = : %s\n", cmd.c_str());
	
	string cmd1 = string("echo running command : \"") + cmd + string("\" >>") + strCmdLogFile;
	if (!executePipe(cmd1, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
	}	
	results.clear();

	stringstream results1;
	if (!executePipe(cmd, results1))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO, "Successfully ran the command: %s\n",cmd.c_str());
	}
	return bResult;
}

#endif
