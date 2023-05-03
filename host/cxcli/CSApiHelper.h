
#ifndef CSAPIHELPER__H
#define CSAPIHELPER__H

#include <string.h>
#include <iostream>
#include "inmsafecapis.h"
#include "curlwrapper.h"
#include "terminateonheapcorruption.h"
#include "hostagenthelpers_ported.h"

using namespace std;

#include <curl/curl.h>
#include <curl/easy.h>
#include "logger.h"
#include "setpermissions.h"
#include "securityutils.h"
#include "genpassphrase.h" 
#include "hostagenthelpers_ported.h"
#include "ace/Process.h"
#include <ace/Task.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include "ace/Process_Manager.h"
#include "transport_settings.h"
#include <libxml/parser.h>
#include <libxml/tree.h> 

#define CSPORT "443"
struct ResponseData
{
	char *chResponseData;
	size_t length;
};

typedef ResponseData ResponseData_t;

// Get fingerprint from file.
string GetFingerPrint(const string& csIP, const string& csPort)
{
	string fingerprintFileName(securitylib::getFingerprintDir());
#ifdef WIN32
	fingerprintFileName = fingerprintFileName + "\\" + csIP + "_" + csPort + ".fingerprint";
#else
	fingerprintFileName = fingerprintFileName + "/" + csIP + "_" + csPort + ".fingerprint";
#endif

	std::string key;
	std::ifstream file(fingerprintFileName.c_str());
	if (file.good()) {
		file >> key;
		boost::algorithm::trim(key);
	}

	return key;
}

// Get Access signature.
string GetAccessSignature(const string& strRqstMethod, const string& strAccessFile, const string& strFuncName, const string& strRqstId, const string& strVersion, const string& csIp)
{
	string strPassPhrase = securitylib::getPassphrase();
	string strPassPhraseHash = securitylib::genSha256Mac(strPassPhrase.c_str(), strPassPhrase.size(), true);  //Gets SHA256 mac of passphrase

	string strFingerPrint = GetFingerPrint(csIp, CSPORT);
	string strFingerPrintHmac;
	if (!strFingerPrint.empty())
		strFingerPrintHmac = securitylib::genHmac(strPassPhraseHash.c_str(), strPassPhraseHash.size(), strFingerPrint);

	string strStringToSign = strRqstMethod + ":" + strAccessFile + ":" + strFuncName + ":" + strRqstId + ":" + strVersion;
	string strSignHmac = securitylib::genHmac(strPassPhraseHash.c_str(), strPassPhraseHash.size(), strStringToSign);

	string strAccessSignature = securitylib::genHmac(strPassPhraseHash.c_str(), strPassPhraseHash.size(), strFingerPrintHmac + ":" + strSignHmac);

	return strAccessSignature;
}

#ifdef WIN32

// Check if Dir exists
bool checkIfDirExists(string strDirName)
{
	bool blnReturn = false;

	DWORD type = GetFileAttributes(strDirName.c_str());
	if (type != INVALID_FILE_ATTRIBUTES)
	{
		if (type & FILE_ATTRIBUTE_DIRECTORY)
			blnReturn = true;
	}

	return blnReturn;
}

// Check if File exists
bool checkIfFileExists(string strFileName)
{
	bool blnReturn = true;
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	hFind = FindFirstFile(strFileName.c_str(), &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		blnReturn = false;
	}
	
	return blnReturn;
}

// This func will generate unique time stamp string
std::string GeneratreUniqueTimeStampString()
{
	string strUniqueTimeStampString;
	SYSTEMTIME  lt;
	GetLocalTime(&lt);
	string strTemp;

	strTemp = boost::lexical_cast<string>(lt.wYear);
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	strTemp = boost::lexical_cast<std::string>(lt.wMonth);
	if (strTemp.size() == 1)
		strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	strTemp = boost::lexical_cast<std::string>(lt.wDay);
	if (strTemp.size() == 1)
		strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	strTemp = boost::lexical_cast<std::string>(lt.wHour);
	if (strTemp.size() == 1)
		strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	strTemp = boost::lexical_cast<std::string>(lt.wMinute);
	if (strTemp.size() == 1)
		strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	strTemp = boost::lexical_cast<std::string>(lt.wSecond);
	if (strTemp.size() == 1)
		strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	strTemp = boost::lexical_cast<std::string>(lt.wMilliseconds);
	if (strTemp.size() == 1)
		strTemp = string("00") + strTemp;
	else if (strTemp.size() == 2)
		strTemp = string("0") + strTemp;
	strUniqueTimeStampString += strTemp;
	strTemp.clear();

	return strUniqueTimeStampString;
}

// --------------------------------------------------------------------------
// Fetch the system drive (say C:\ - it will be in this format).
// --------------------------------------------------------------------------
bool GetSystemVolume(string & strSysVol)
{  
	char szSysPath[MAX_PATH];
	if (0 == GetSystemDirectory(szSysPath, MAX_PATH))
	{
		return false;
	}

	char szSysDrive[MAX_PATH];
	if (0 == GetVolumePathName(szSysPath,szSysDrive,MAX_PATH))
	{
		return false;
	}    
	strSysVol = string(szSysDrive);

	return true;
}

#else

// Check if a file exists or not.
bool checkIfFileExists(std::string strFileName)
{
	bool blnReturn = true;
	struct stat stFileInfo;
	int intStat;

	// Attempt to get the file attributes
	intStat = stat(strFileName.c_str(), &stFileInfo);
	if (intStat == 0) {
		// We were able to get the file attributes
		// so the file obviously exists.
		blnReturn = true;
	}
	else {
		// We were not able to get the file attributes.
		// This may mean that we don't have permission to
		// access the folder which contains this file. If you
		// need to do that level of checking, lookup the
		// return values of stat which will give you
		// more details on why stat failed.
		blnReturn = false;
	}

	return blnReturn;
}

//Check if a Dir exists or not.
bool checkIfDirExists(string strDirName)
{
	bool blnReturn = false;

	try
	{
		if (access(strDirName.c_str(), 0) == 0)
		{
			struct stat stFileInfo;
			stat(strDirName.c_str(), &stFileInfo);
			if (stFileInfo.st_mode & S_IFDIR)
				blnReturn = true;
		}
	}
	catch (...)
	{
		std::cerr << "Exception on accessing the path - " << strDirName << endl;
	}

	return blnReturn;
}

// This func will generate unique time stamp string
string GeneratreUniqueTimeStampString()
{
	string strUniqueTimeStampString;
	stringstream ss;

	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	ss << (tm.tm_year + 1900) << (tm.tm_mon + 1) << tm.tm_mday << tm.tm_hour << tm.tm_min << tm.tm_sec;
	strUniqueTimeStampString = ss.str();

	return strUniqueTimeStampString;
}
#endif

// Get request Id.
string GetRequestId()
{
	string strRqstId;

	strRqstId = GeneratreUniqueTimeStampString();

#ifdef win32
	DWORD nRandomNum = rand() % 100000000 + 10000001;
#else
	unsigned long nRandomNum = rand() % 100000000 + 10000001;
#endif

	stringstream ss;
	ss << nRandomNum;
	strRqstId = strRqstId + "_" + ss.str();

	return strRqstId;
}

// prepare XML file.
string prepareXmlFile(string& csIp)
{
	string strRqstId = GetRequestId();
	string xml_data = string("<Request Id=\"") + strRqstId + string("\" Version=\"1.0\">");
	xml_data = xml_data + string("<Header>");
	xml_data = xml_data + string("<Authentication>");
	xml_data = xml_data + string("<AuthMethod>ComponentAuth_V2015_01</AuthMethod>");
	xml_data = xml_data + string("<AccessKeyID>") + "5C1DAEF0-9386-44a5-B92A-31FE2C79236A" + string("</AccessKeyID>");
	xml_data = xml_data + string("<AccessSignature>") + GetAccessSignature("POST", "/ScoutAPI/CXAPI.php", "GetCxPsConfiguration", strRqstId, "1.0", csIp) + string("</AccessSignature>");
	xml_data = xml_data + string("</Authentication>");
	xml_data = xml_data + string("</Header>");
	xml_data = xml_data + string("<Body>");
	xml_data = xml_data + string("<FunctionRequest Name=\"GetCxPsConfiguration\" >");
	xml_data = xml_data + string("</FunctionRequest>");
	xml_data = xml_data + string("</Body>");
	xml_data = xml_data + string("</Request>");

	return xml_data;
}

static size_t WriteCallbackFunction(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t realsize;
	INM_SAFE_ARITHMETIC(realsize = InmSafeInt<size_t>::Type(size) * nmemb, INMAGE_EX(size)(nmemb))
		ResponseData_t *ResData = (ResponseData_t *)data;

	size_t resplen;
	INM_SAFE_ARITHMETIC(resplen = InmSafeInt<size_t>::Type(ResData->length) + realsize + 1, INMAGE_EX(ResData->length)(realsize))
		ResData->chResponseData = (char *)realloc(ResData->chResponseData, resplen);
	if (ResData->chResponseData)
	{
		inm_memcpy_s(&(ResData->chResponseData[ResData->length]), resplen, ptr, realsize);
		ResData->length += realsize;
		ResData->chResponseData[ResData->length] = 0;
	}
	return realsize;
}


//Assign permissions to file or directory
//Input - File name and flag for directory
void AssignSecureFilePermission(const string& strFile, bool bDir = false)
{
	try
	{
		if (bDir)
		{
			if (checkIfDirExists(strFile))
				securitylib::setPermissions(strFile, securitylib::SET_PERMISSIONS_NAME_IS_DIR);
			else
				std::cerr << "Dir does not exist - " << strFile << endl;
		}
		else
		{
			if (checkIfFileExists(strFile))
				securitylib::setPermissions(strFile);
			else
			    std::cerr << "File does not exist - " << strFile << endl;
		}
	}
	catch (...)
	{
		std::cerr << "Failed to set permission - " << strFile << endl;
	}
}

// Process the XML data.
bool processXmlData(string strXmlData, string& csIp, string& strOutPutXmlFile)
{
	bool bResult = true;

	HTTP_CONNECTION_SETTINGS s;
    inm_strcpy_s(s.ipAddress, sizeof(s.ipAddress) - 1, csIp.c_str());
	s.port = 443;
	ResponseData_t Data = { 0 };
	string responsedata;

	curl_global_init(CURL_GLOBAL_ALL);

	CurlOptions options(s.ipAddress, s.port, "/ScoutAPI/CXAPI.php", true);
	options.writeCallback(static_cast<void *>(&Data), WriteCallbackFunction);

	Data.chResponseData = NULL;
	Data.length = 0; /* no data at this point */
	try {
		CurlWrapper cw;
		cw.post(options, strXmlData);
		if (Data.length > 0)
		{
			responsedata = string(Data.chResponseData);
			if (Data.chResponseData != NULL)
				free(Data.chResponseData);
		}
	}
	catch (ErrorException& exception)
	{
		std::cerr << "Exception - " << exception.what() << endl;
		bResult = false;
	}

	ofstream outXml(strOutPutXmlFile.c_str(), std::ios::out);
	if (!outXml.is_open())
	{
		return false;
	}
	
	outXml << responsedata << endl;
	outXml.close();

	//Assign Secure permission
	AssignSecureFilePermission(strOutPutXmlFile);

	return bResult;
}

/* Finds the required Child node and returns its pointer
Input - Parent Pointer , Value of Child node
Output - Pointer to required Childe node (returns NULL if not found)
*/
xmlNodePtr xGetChildNode(xmlNodePtr cur, const xmlChar* xmlEntry)
{
	cur = cur->children;
	while (cur != NULL)
	{
		if (!xmlStrcmp(cur->name, xmlEntry))
		{
			//Found the required child node
			break;
		}
		cur = cur->next;
	}

	return cur;
}

//***************************************************************************************
// Finds the required Child node with given attribute match and returns its pointer
//	Input - Parent Pointer , Value of Child node, Name of Attribute and Value of Attribute
//	Output - Pointer to required Childe node (returns NULL if not found)
//***************************************************************************************
xmlNodePtr xGetChildNodeWithAttr(xmlNodePtr cur, const xmlChar* xmlEntry, const xmlChar* attrName, const xmlChar* attrValue)
{
	cur = cur->children;
	while (cur != NULL)
	{
		if (!xmlStrcasecmp(cur->name, xmlEntry))
		{
			//Found the required child node. Now check the attribute
			xmlChar *attr_value_temp;
			attr_value_temp = xmlGetProp(cur, attrName);
			if ((attr_value_temp != NULL) && (!xmlStrcasecmp(attr_value_temp, attrValue)))
			{
				break;
			}
		}
		cur = cur->next;
	}

	return cur;
}

//***************************************************************************************
// Finds the required Value for the given property for that node.
//	Input - current node pointer , property name
//	Output - Output value
//***************************************************************************************
bool xGetValueForProp(xmlNodePtr xCur, const string xmlPropertyName, string& attrValue)
{
	bool rv = true;

	xmlNodePtr xChildNode = xGetChildNodeWithAttr(xCur, (const xmlChar*)"Parameter", (const xmlChar*)"Name", (const xmlChar*)xmlPropertyName.c_str());
	if (xCur == NULL)
	{
		rv = false;
	}
	else
	{
		xmlChar *attr_value_temp = xmlGetProp(xChildNode, (const xmlChar*)"Value");
		if (attr_value_temp == NULL)
		{
			rv = false;
		}
		else
			attrValue = std::string((char *)attr_value_temp);
	}

	return rv;
}

// Read the response XML file.
bool ReadResponseXmlFile(string strCxApiResponseFile, string strCxApi, string& strStatus)
{
	bool bResult = true;

	string strTagType;
	xmlDocPtr xDoc;
	xmlNodePtr xCur;

	do
	{
		xDoc = xmlParseFile(strCxApiResponseFile.c_str());
		if (xDoc == NULL)
		{
			bResult = false;
			break;
		}

		//Found the doc. Now process it.
		xCur = xmlDocGetRootElement(xDoc);
		if (xCur == NULL)
		{
			bResult = false;
			break;
		}
		if (xmlStrcasecmp(xCur->name, (const xmlChar*)"Response"))
		{
			//Response is not found
			bResult = false;
			break;
		}

		xmlChar *attr_value_temp = xmlGetProp(xCur, (const xmlChar*)"Message");
		if (attr_value_temp == NULL)
		{
			bResult = false;
			break;
		}
		strStatus = std::string((char *)attr_value_temp);

		//Converting to lowercase
		for (size_t i = 0; i < strStatus.length(); i++)
			strStatus[i] = tolower(strStatus[i]);
		if (strStatus.compare("success") != 0)
		{
			bResult = false;
			break;
		}

		//If you are here Response entry is found. Go for Body entry.
		xCur = xGetChildNode(xCur, (const xmlChar*)"Body");
		if (xCur == NULL)
		{
			bResult = false;
			break;
		}
		//If you are here Body entry is found. Go for Function entry with required Function name.
		xCur = xGetChildNodeWithAttr(xCur, (const xmlChar*)"Function", (const xmlChar*)"Name", (const xmlChar*)(strCxApi.c_str()));
		if (xCur == NULL)
		{
			bResult = false;
			break;
		}
		attr_value_temp = xmlGetProp(xCur, (const xmlChar*)"Message");
		if (attr_value_temp == NULL)
		{
			bResult = false;
			break;
		}
		strStatus = std::string((char *)attr_value_temp);

		//Converting to lowercase
		for (size_t i = 0; i < strStatus.length(); i++)
			strStatus[i] = tolower(strStatus[i]);
		if (strStatus.compare("success") != 0)
		{
			bResult = false;
			break;
		}

		xCur = xGetChildNode(xCur, (const xmlChar*)"FunctionResponse");
		if (xCur == NULL)
		{
			bResult = false;
			break;
		}

		if (strCxApi.compare("GetCxPsConfiguration") == 0)
		{
			if (false == xGetValueForProp(xCur, "CxPsSslPort", strStatus))
			{
				bResult = false;
				break;
			}
		}
	} while (0);

	xmlFreeDoc(xDoc);

	return bResult;
}

#endif // CSAPIHELPER__H