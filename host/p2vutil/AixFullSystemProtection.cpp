/*
* Copyright (c) 2005 InMage.
*       This file contains proprietary and confidential information and
*       remains the unpublished property of InMage. Use,disclosure, or 
*       reproduction is prohibited except as permitted by express written 
*       license agreement with InMage.

* File       : AIXFullSystemProtection.cpp

* Description:
*/

#include <iostream>
#include <string.h>
#include <ostream>
#include <fstream>
#include <sstream>
#include <curl/curl.h>
#include <stdlib.h>
#include <unistd.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include <map>
#include <stdarg.h>
#include <list>
#include <stdlib.h>
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "curlwrapper.h"


#define NUM_RETRIES 5

using namespace std;

enum SV_LOG_LEVEL { SV_LOG_DISABLE,
                    SV_LOG_FATAL,
                    SV_LOG_SEVERE,
                    SV_LOG_ERROR,
                    SV_LOG_WARNING,
                    SV_LOG_INFO,
                    SV_LOG_DEBUG,
                    SV_LOG_ALWAYS };

#define NUM_RETRIES 5

std::string g_inmLogFile;
std::string installLocation;
std::map<std::string,std::string> g_SrcDiskTgtLvMap;
bool isRecovery=0;
bool g_isResume=0;
int g_inmLogLevel;

typedef struct HostData 
{
   std::string IPAddress;
   std::string HostID;
   std::string TemplateLPAR;
};

typedef struct ProtectionSettings 
{
   std::string HMCIpAddress;
   std::string HMCUser;
   std::string VGNameOnVIOS;
   std::string ProcessServerIP;
   std::string PlanName;
   std::string BatchResyncRequired;
   std::string RetentionPath;

   std::list<HostData> HostList;
};

ProtectionSettings g_ProtectionSettings;

typedef struct LparData
{
    std::string lparName;
    std::string lparIp;
};

typedef struct VirScsiAdapter
{
    std::string vAdapter;
    std::string phyLoc;
};

std::map<std::string,std::list<LparData> > g_MSLparMap;

typedef struct DiskProperties
{
    std::string strVgName;
    std::string strSize;
    std::string strScsiId;
    void reset()
    {
        strVgName.clear();
        strSize.clear();
        strScsiId.clear();
    }
    ~DiskProperties()
    {
        reset();
    }
};

typedef struct StorageInfo
{
    std::string cpuCount;
    std::map<std::string,DiskProperties> listDiskNames;
    //listLVNames contains mapping of lv name and size
    std::map<std::string,std::string> listLVNames;
    void reset()
    {
        cpuCount.clear();
        listDiskNames.clear();
        listLVNames.clear();
    }
    ~StorageInfo()
    {
       reset();
    }
};

std::map<std::string,StorageInfo> g_StorageInfoMap;

typedef struct ResponseData
{
   char *chResponseData;
   size_t length;
};

void BinUsage( char *binname )
{
   std::stringstream out;
   out << "usage: " << binname << " --protection <FileName> \n"
       << "       " << binname << " --recovery --Tag <TAGNAME> <FileName> \n"
       << "       " << binname << " --resume <FileName> \n";
   std::cout << out.str().c_str();
}

void setLogFile(const char* file_name)
{
   g_inmLogFile = file_name;
}

void log(SV_LOG_LEVEL nLogLevel, const char* format, ... )
{
    va_list a;

    if( 0 == strlen( format ) )
    {
        return;
    }
    if ( nLogLevel <= g_inmLogLevel )
    {
        FILE *p_inmfile;

        time_t ltime;
        struct tm *today;

        time( &ltime );
        today= localtime( &ltime );
        char present[30];
        memset(present,0,30);
 
		inm_sprintf_s(present, ARRAYSIZE(present), "(%02d-%02d-20%02d %02d:%02d:%02d): ",
                today->tm_mon + 1,
                today->tm_mday,
                today->tm_year - 100,
                today->tm_hour,
                today->tm_min,
                today->tm_sec
         );
         p_inmfile = fopen(g_inmLogFile.c_str(), "a");
         if ( p_inmfile != NULL )
         {
            fprintf(p_inmfile,present);
            va_start( a, format );
            vfprintf(p_inmfile, format, a);
            va_end( a );
            fclose(p_inmfile);
         }
         else
         {
            cout<<"Failed to open "<<g_inmLogFile.c_str()<<endl;
            exit(1);
         }
    }
}

std::list<std::string> tokenizeString(std::string str,const char* delimiter)
{
    std::list<std::string> tokenList;
    char* keys;

    size_t cstrlen;
    INM_SAFE_ARITHMETIC(cstrlen = InmSafeInt<size_t>::Type(str.length()) + 1, INMAGE_EX(str.length()))
    char *cstr = new char[cstrlen];
    inm_strcpy_s(cstr,(str.length() + 1), str.c_str());

    keys = strtok(cstr,delimiter);
    while( NULL != keys)
    {
       tokenList.push_back(string(keys));
       keys = strtok(NULL,delimiter);
    }

    delete []cstr;

    return tokenList;
}

void ReadConfig(std::string filename)
{
    log(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    std::ifstream file( filename.c_str() );
    std::stringstream buffer;

    if ( file )
    {
       buffer << file.rdbuf();
       file.close();
    }
    else
    {
       log(SV_LOG_ERROR,"File %s does not exist\n",filename.c_str());
       exit(1);
    }

    std::string filecontents = buffer.str();
    std::string key,value;

       std::list<std::string> contentList = tokenizeString(filecontents,"\n");
       std::list<std::string>::iterator itrMap = contentList.begin();

       HostData l_hostData;
       while(itrMap != contentList.end())
       {
         log(SV_LOG_DEBUG,"Line from File :%s \n",(*itrMap).c_str());

         if(*itrMap != string("[SourceHost]"))
         {
            std::list<std::string> contentMap = tokenizeString((*itrMap),"=");

            key=contentMap.front();
            value=contentMap.back();

            if(key == "HMCIpAddress")
                 g_ProtectionSettings.HMCIpAddress = value;
            if(key == "HMCUser" )
                 g_ProtectionSettings.HMCUser = value;
            if(key == "VGNameOnVIOS" )
                 g_ProtectionSettings.VGNameOnVIOS = value;
            if(key == "ProcessServerIP")
                 g_ProtectionSettings.ProcessServerIP = value;
            if(key == "IPAddress")
                 l_hostData.IPAddress = value;
            if(key == "HostID" )
                 l_hostData.HostID = value;
            if(key == "PlanName" )
                 g_ProtectionSettings.PlanName = value;
            if(key == "BatchResyncRequired" )
                 g_ProtectionSettings.BatchResyncRequired = value;
            if(key == "RetentionPath" )
                 g_ProtectionSettings.RetentionPath = value;
            if(key == "TemplateLPAR" )
            {
                 l_hostData.TemplateLPAR = value;
                 g_ProtectionSettings.HostList.push_back(l_hostData);
            }
         }
        itrMap++;
      }

    log(SV_LOG_DEBUG,"HMCIpAddress : %s\n",g_ProtectionSettings.HMCIpAddress.c_str());
    log(SV_LOG_DEBUG,"HMCUser : %s\n",g_ProtectionSettings.HMCUser.c_str());
    log(SV_LOG_DEBUG,"VGNameOnVIOS : %s\n",g_ProtectionSettings.VGNameOnVIOS.c_str());
    log(SV_LOG_DEBUG,"ProcessServerIP : %s\n",g_ProtectionSettings.ProcessServerIP.c_str());
    log(SV_LOG_DEBUG,"PlanName : %s\n",g_ProtectionSettings.PlanName.c_str());
    log(SV_LOG_DEBUG,"BatchResyncRequired : %s\n",g_ProtectionSettings.BatchResyncRequired.c_str());
    log(SV_LOG_DEBUG,"RetentionPath : %s\n",g_ProtectionSettings.RetentionPath.c_str());

    std::list<HostData>::iterator itr;

    for(itr = g_ProtectionSettings.HostList.begin(); itr != g_ProtectionSettings.HostList.end(); itr++)
    {
       log(SV_LOG_DEBUG,"IPAddress : %s\n",itr->IPAddress.c_str());
       log(SV_LOG_DEBUG,"HostID  : %s\n",itr->HostID.c_str());
       log(SV_LOG_DEBUG,"TemplateLPAR : %s\n",itr->TemplateLPAR.c_str());
    }

    log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

xmlNodePtr xGetChildNodeWithAttr(xmlNodePtr cur, const xmlChar* xmlEntry, const xmlChar* attrName, const xmlChar* attrValue)
{
    log(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    cur = cur->children;
    while(cur != NULL)
    {
      if (!xmlStrcmp(cur->name, xmlEntry))
      {
         xmlChar *attr_value_temp;
         attr_value_temp = xmlGetProp(cur,attrName);
         if ((attr_value_temp != NULL) && (! xmlStrcmp(attr_value_temp,attrValue) ))
         {
             break;
         }
      }
      cur = cur->next;
    }
    log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return cur;
}

bool xGetValueForProp(xmlNodePtr xCur, const string xmlPropertyName, string& attrValue)
{
    log(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    log(SV_LOG_DEBUG,"Property Name : %s\n",xmlPropertyName.c_str());
    
    xmlNodePtr xChildNode = xGetChildNodeWithAttr(xCur,(const xmlChar*)"Parameter",(const xmlChar*)"Name", (const xmlChar*)xmlPropertyName.c_str());
    if (xCur == NULL)
    {
        log(SV_LOG_ERROR,"The XML document is not in expected format : <Parameter with required Name=%s> entry not found\n", xmlPropertyName.c_str());
        rv = false;
    }
    else
    {
        xmlChar *attr_value_temp = xmlGetProp(xChildNode,(const xmlChar*)"Value");
        if (attr_value_temp == NULL)
        {
           log(SV_LOG_ERROR,"The XML document is not in expected format : <Parameter Value> entry not found\n");
           rv = false;
        }
        else
           attrValue = std::string((char *)attr_value_temp);
    }
    log(SV_LOG_DEBUG,"Attribute Value : %s\n",attrValue.c_str());
    
    log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

bool ReadFile( std::string InputFile, std::map<std::string,std::string>& ConfigParamMap)
{
   std::ifstream file( InputFile.c_str() );
   std::stringstream buffer;

   if ( file )
   {
       buffer << file.rdbuf();
       file.close();
   }
   else
   {
       log(SV_LOG_ERROR,"File %s does not exist\n",InputFile.c_str());
       return false;
   }

   std::string filecontents = buffer.str();

   std::list<std::string> InputFileTokensList = tokenizeString(filecontents, "\n");

   std::list<std::string> keyValueTokens;

   std::list<std::string>::iterator outputTokenIter = InputFileTokensList.begin();

   std::string key,value;
   while(outputTokenIter != InputFileTokensList.end())
   {
      keyValueTokens = tokenizeString((*outputTokenIter), "=");

      key=keyValueTokens.front();
      value=keyValueTokens.back();

      log(SV_LOG_DEBUG,"Key - %s Value %s\n",key.c_str(),value.c_str());

      if(0 == strcmp(key.c_str(),value.c_str()))
         value=string("");

      ConfigParamMap.insert(std::make_pair<std::string,std::string>(key,value));
      
      key.clear();
      value.clear();

      outputTokenIter++;
   }
   return true;
}

bool ReadFromConfigMap(std::string filename,std::string Key, std::string& Value)
{
    std::map<std::string,std::string> l_InputParamMap;
    if(false == ReadFile(filename,l_InputParamMap))
    {
        log(SV_LOG_ERROR,"Unable to read the contents of %s\n",filename.c_str());
        return false;
    }

    std::map<std::string,std::string>::iterator itr=l_InputParamMap.begin();

    do
    {
       log(SV_LOG_DEBUG,"Key - %s - Value - %s\n",itr->first.c_str(),itr->second.c_str());
       if(itr->first == Key)
       {
          Value=itr->second;
          log(SV_LOG_DEBUG,"%s - %s\n",Key.c_str(),Value.c_str());
          if(!Value.empty())
             return true;
          else
             return false; 
       }
       itr++;
    }while(itr != l_InputParamMap.end());
}

void PrintDiscData(std::string hostId)
{
   log(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

   std::map<std::string,StorageInfo> ::iterator itrStorage;
   StorageInfo l_storageInfo;
   for( itrStorage = g_StorageInfoMap.begin(); itrStorage != g_StorageInfoMap.end(); itrStorage++)
   {
      if(itrStorage->first == hostId )
      {
        l_storageInfo = itrStorage->second;
        log(SV_LOG_DEBUG,"CPUCount : %s\n",l_storageInfo.cpuCount.c_str());
        log(SV_LOG_DEBUG,"Disk - VGName - DiskSize\n");

        for(std::map<std::string,DiskProperties>::iterator itr=l_storageInfo.listDiskNames.begin();itr != l_storageInfo.listDiskNames.end();itr++)
        {
          log(SV_LOG_DEBUG,"%s - %s - %s \n",itr->first.c_str(),itr->second.strVgName.c_str(),itr->second.strSize.c_str());
        }
        log(SV_LOG_DEBUG,"LVName - LVSize\n");

        for(std::map<std::string,std::string>::iterator iter=l_storageInfo.listLVNames.begin();iter != l_storageInfo.listLVNames.end();iter++)
        {
          log(SV_LOG_DEBUG,"%s - %s \n",iter->first.c_str(),iter->second.c_str());
        }
      }
   }

   log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

std::string prepareXmlForGetHostInfo(const std::string hostId)
{
   string xml_data = string("<Request Id=\"0001\" Version=\"1.0\">");
   xml_data = xml_data + string("<Header>");
   xml_data = xml_data + string("<Authentication>");
   xml_data = xml_data + string("<AccessKeyID>DDC9525FF275C104EFA1DFFD528BD0145F903CB1</AccessKeyID>");
   xml_data = xml_data + string("<AccessSignature></AccessSignature>");  //Currently The access key here is NULL, need to write the code for it.
   xml_data = xml_data + string("</Authentication>");
   xml_data = xml_data + string("</Header>");
   xml_data = xml_data + string("<Body>");
   xml_data = xml_data + string("<FunctionRequest Name=\"GetHostInfo\" Include=\"Yes\">");
   xml_data = xml_data + string("<Parameter Name=\"HostGUID\" Value=\"") + hostId + string("\" />");
   xml_data = xml_data + string("<Parameter Name=\"InformationType\" Value=\"All\" />");
   xml_data = xml_data + string("</FunctionRequest>");
   xml_data = xml_data + string("</Body>");
   xml_data = xml_data + string("</Request>");

   return xml_data;
}

static size_t WriteCallbackFunction(void *ptr, size_t size, size_t nmemb, void *data)
{
   size_t realsize;
   INM_SAFE_ARITHMETIC(realsize = InmSafeInt<size_t>::Type(size) * nmemb, INMAGE_EX(size)(nmemb))
   ResponseData *ResData = (ResponseData *)data;
   
   size_t resplen;
   INM_SAFE_ARITHMETIC(resplen = InmSafeInt<size_t>::Type(ResData->length) + realsize + 1, INMAGE_EX(ResData->length)(realsize))
   ResData->chResponseData = (char *)realloc(ResData->chResponseData, resplen);
   if(ResData->chResponseData)
   {
      inm_memcpy_s(&(ResData->chResponseData[ResData->length]),(realsize + 1), ptr, realsize);
      ResData->length += realsize;
      ResData->chResponseData[ResData->length] = 0;
   }
   return realsize;
}

bool processXmlDataForGetHostInfo(std::string strXmlData, std::string CXIpAddress, std::string CXPort, int l_isHttps, std::string& strRespFile)
{
	std::string phpUrl = string("/ScoutAPI/CXAPI.php");

    log(SV_LOG_INFO,"PHP URL : %s\n", phpUrl.c_str());
	log(SV_LOG_INFO,"CXIpAddress : %s\n",CXIpAddress.c_str());
	log(SV_LOG_INFO,"CXPort : %s\n",CXPort.c_str());
	log(SV_LOG_INFO,"IsHttps : %d\n",l_isHttps);

    ResponseData Data = {0};
    std::string responsedata;
   
	curl_global_init(CURL_GLOBAL_ALL);

	CurlOptions options(CXIpAddress, atoi(CXPort.c_str()), phpUrl, l_isHttps);
	options.writeCallback( static_cast<void *>( &Data ),WriteCallbackFunction);


	Data.chResponseData = NULL;
	Data.length = 0;

	try {
		CurlWrapper cw;
		cw.post(options, strXmlData);
		if( Data.length > 0)
		{
			responsedata = string(Data.chResponseData);
			if(Data.chResponseData != NULL)
				free( Data.chResponseData ) ;
		}
	} catch(ErrorException& exception )
	{
		DebugPrintf(SV_LOG_ERROR, "FAILED : %s with error %s\n",__FUNCTION__, exception.what());
	}


	std::ofstream outXml(strRespFile.c_str(),std::ios::out);
    if(!outXml.is_open())
    {
       log(SV_LOG_ERROR,"Failed to open file %s\n",strRespFile.c_str());
       return false;
    }
    outXml << responsedata << endl;
    outXml.close();

    log(SV_LOG_DEBUG,"Response : %s\n", responsedata.c_str());
    return true;
}

xmlNodePtr xGetChildNode(xmlNodePtr cur, const xmlChar* xmlEntry)
{
   log(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
   cur = cur->children;
   while(cur != NULL)
   {
     if (!xmlStrcasecmp(cur->name, xmlEntry))
     {
        break;
     }
     cur = cur->next;
   }
   log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
   return cur;
}

bool xGetStorageInfo(xmlNodePtr xCur , StorageInfo& l_storageInfo)
{
    log(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    for(xmlNodePtr xChild = xCur->children; xChild != NULL; xChild = xChild->next)
    {
       if (!xmlStrcasecmp(xChild->name, (const xmlChar*)"ParameterGroup"))
       {
          xmlChar* attr_value = xmlGetProp(xChild,(const xmlChar*)"Id");
          if (attr_value == NULL)
          {
              log(SV_LOG_ERROR,"The XML document is not in expected format : <Disk ParameterGroup Id> entry not found\n");
              log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
              return false;
          }
          string disk = std::string((char *)attr_value);
          string diskName;
          if(false == xGetValueForProp(xChild, "DiskName", diskName))
          {
             log(SV_LOG_ERROR,"Failed to get the value for DiskName\n");
             log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
             return false;
          }
          log(SV_LOG_INFO,"Selected disk from XML : %s And Disk Name is : %s\n",disk.c_str(), diskName.c_str());

          DiskProperties diskProp;         
          string vg;
          if(false == xGetValueForProp(xChild, "DiskGroup", vg))
          {
             log(SV_LOG_ERROR,"Failed to get the value for DiskGroup\n");
          }

          diskProp.strVgName = vg;

          string size;
          if(false == xGetValueForProp(xChild, "Size", size))
          {
              log(SV_LOG_ERROR,"Failed to get the value for Disk Size\n");
          }

          diskProp.strSize = size;

          string scsiId;
          if(false == xGetValueForProp(xChild, "ScsiId", scsiId))
          {
              log(SV_LOG_ERROR,"Failed to get the value for ScsiId\n");
          }

          diskProp.strScsiId = scsiId; 

          l_storageInfo.listDiskNames.insert(std::make_pair<string,DiskProperties>(diskName,diskProp) );
          diskProp.reset();

          for(xmlNodePtr xChild1 = xChild->children; xChild1 != NULL; xChild1 = xChild1->next)
          {
            if (!xmlStrcasecmp(xChild1->name, (const xmlChar*)"ParameterGroup"))
            {
              attr_value = xmlGetProp(xChild1,(const xmlChar*)"Id");
              if (attr_value == NULL)
              {
                 log(SV_LOG_ERROR,"The XML document is not in expected format : <Partition ParameterGroup Id> entry not found\n");
                 log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                 return false;
              }
              string partition = std::string((char *)attr_value);
              string partitionName;
              if(false == xGetValueForProp(xChild1, "Name", partitionName))
              {
                 log(SV_LOG_ERROR,"Failed to get the value for Partiton Name\n");
                 partitionName = diskName;
              }
              log(SV_LOG_INFO,"Selected partition from XML : %s And partition Name is : %s\n",partition.c_str(), partitionName.c_str());
              
              for(xmlNodePtr xChild2 = xChild1->children; xChild2 != NULL; xChild2 = xChild2->next)
              {
                 if (!xmlStrcasecmp(xChild2->name, (const xmlChar*)"ParameterGroup"))
                 {
                    xmlChar* attr_value = xmlGetProp(xChild2,(const xmlChar*)"Id");
                    if (attr_value == NULL)
                    {
                       log(SV_LOG_ERROR,"The XML document is not in expected format : <Logical Volume ParameterGroup Id> entry not found\n");
                       log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                       return false;
                    }
                    string lv = std::string((char *)attr_value);
                    string lvName, systemVolume, mountPoint;
                    if(false == xGetValueForProp(xChild2, "Name", lvName))
                    {
                       log(SV_LOG_ERROR,"Failed to get the value for Logical Volume Name\n");
                       log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                       return false;
                    }

                    size = "";
                    if(false == xGetValueForProp(xChild2, "Size", size))
                    {
                       log(SV_LOG_ERROR,"Failed to get the value for Logical Volume Size\n");
                    }
                    
                    log(SV_LOG_INFO,"Selected Logical Volume from XML : %s And Logical Volume Name is : %s\n",lv.c_str(), lvName.c_str());
                    l_storageInfo.listLVNames.insert(std::make_pair<string,string>(lvName,size));
                }
              }
            }
          }
       }
    }
    log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

bool parseCxApiXmlFile(std::string strCxApiResXmlFile, std::string HostId)
{
    log(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    xmlDocPtr xDoc;
    xmlNodePtr xCur;
    StorageInfo l_storageInfo;

    l_storageInfo.reset();

    xDoc = xmlParseFile(strCxApiResXmlFile.c_str());
    if (xDoc == NULL)
    {
        log(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
        log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    xCur = xmlDocGetRootElement(xDoc);
    if (xCur == NULL)
    {
        log(SV_LOG_ERROR,"The XML document is empty. Cannot Proceed further.\n");
        log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        xmlFreeDoc(xDoc); 
        return false;
    }

    if (xmlStrcasecmp(xCur->name,(const xmlChar*)"Response"))
    {
        log(SV_LOG_ERROR,"The XML document is not in expected format : <Response> entry not found\n");
        log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        xmlFreeDoc(xDoc); 
        return false;
    }

    xCur = xGetChildNode(xCur,(const xmlChar*)"Body");
    if (xCur == NULL)
    {
       log(SV_LOG_ERROR,"The XML document is not in expected format : <Body> entry not found\n");
       log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
       xmlFreeDoc(xDoc); 
       return false;
    }

    xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"Function",(const xmlChar*)"Name", (const xmlChar*)"GetHostInfo");
    if (xCur == NULL)
    {
       log(SV_LOG_ERROR,"The XML document is not in expected format : <Function with required Name> entry not found\n");
       log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
       xmlFreeDoc(xDoc); 
       return false;
    }

    xCur = xGetChildNode(xCur,(const xmlChar*)"FunctionResponse");
    if (xCur == NULL)
    {
       log(SV_LOG_ERROR,"The XML document is not in expected format : <FunctionResponse> entry not found\n");
       log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
       xmlFreeDoc(xDoc); 
       return false;
    }

    xmlNodePtr xChildNode;
    string xmlHostId;
    if(false == xGetValueForProp(xCur, "HostGUID", xmlHostId))
    {
       log(SV_LOG_ERROR,"Failed to find out the Host Id from xml file\n");
       log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
       xmlFreeDoc(xDoc); 
       return false;
    }

    string cpuCount;
    if(false == xGetValueForProp(xCur, "CPUCount" , cpuCount))
    {
       log(SV_LOG_ERROR,"Failed to find out the Host Id from xml file\n");
       log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
       xmlFreeDoc(xDoc); 
       return false;
    }

    log(SV_LOG_DEBUG,"CPU Count : %s\n",cpuCount.c_str());
    l_storageInfo.cpuCount=cpuCount;

    if(HostId.compare(xmlHostId) == 0)
    {
       xChildNode = xGetChildNodeWithAttr(xCur,(const xmlChar*)"ParameterGroup",(const xmlChar*)"Id", (const xmlChar*)"DiskList");
       if (xChildNode == NULL)
       {
          log(SV_LOG_ERROR,"The XML document is not in expected format : <ParameterGroup with required Id=DiskList> entry not found\n");
          log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
          xmlFreeDoc(xDoc); 
          return false;
       }
       else
       {
           if(false == xGetStorageInfo(xChildNode,l_storageInfo ))
           {
              log(SV_LOG_ERROR,"Failed to get the Storage information from xml file\n");
              log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
              xmlFreeDoc(xDoc); 
              return false;
           }
       }
     }

    g_StorageInfoMap.insert(make_pair<std::string,StorageInfo>(HostId,l_storageInfo));
    return true;    
}

int ExecuteCommand(const string command,std::string& l_output, int retries)
{
    log(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    log(SV_LOG_DEBUG,"Command to Execute - %s\n",command.c_str());

    int i=0;
    int ret;

    while(i < retries)
    {
       std::stringstream OutputString;

       FILE *fp;
       char inm_data[1035];
       l_output.clear();

       fp = popen(command.c_str(), "r");
       if (fp == NULL) {
          printf("Failed to run command %s\n",command.c_str());
          return 1;
       }

       while (!feof(fp))
       {
          while (fgets(inm_data, sizeof(inm_data), fp) != NULL) {
              log(SV_LOG_INFO,"%s\n", inm_data);
              OutputString << inm_data;
          }
       }

       ret=pclose(fp)/256;

       l_output=OutputString.str();

       log(SV_LOG_DEBUG,"Command Output - %s\n",l_output.c_str());
       log(SV_LOG_DEBUG,"Command Returned - %d\n",ret);

       if(ret == 0)
           break;

       i++;
       sleep(5);
    }

    if(ret !=0 && retries != 1)
    {
       log(SV_LOG_DEBUG,"Command Execution Failed Even after [%d] tries \n",retries);
    }

    log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return ret;
}

bool CleanupDisks()
{
    log(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    std::string HMCUser = g_ProtectionSettings.HMCUser;
    std::string HMCIp = g_ProtectionSettings.HMCIpAddress;
    std::string ManagedSystem,VIOServerName;

    std::list<HostData> :: iterator itrhostList;
    std::string SrcHostId;

    for(itrhostList = g_ProtectionSettings.HostList.begin(); itrhostList != g_ProtectionSettings.HostList.end(); itrhostList++ )
    {
       SrcHostId = itrhostList->HostID;

       std::string serversFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/Servers.dat");
       if(false == ReadFromConfigMap(serversFile,"ManagesSystem",ManagedSystem))
       {
          log(SV_LOG_ERROR,"Unable to get the ManagesSystem from %s\n",serversFile.c_str());
          return false;
       }
       if(false == ReadFromConfigMap(serversFile,"VIOServerName",VIOServerName))
       {
          log(SV_LOG_ERROR,"Unable to get the VIOServerName from %s\n",serversFile.c_str());
          return false;
       }

       std::string DiskMapFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/SrcTgtDiskMap.dat");
       std::string DiskVtdMapFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/SrcTgtVtdMap.dat");
       std::string DiskLvFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/SrcTgtLvMap.dat");

       std::string strCommnadToExecute ;
       std::string l_CmdOutput;

       std::map<std::string,std::string> SrcTgtDiskMap;
       if(false == ReadFile(DiskMapFile,SrcTgtDiskMap))
       {
          log(SV_LOG_ERROR,"Unable to read the contents of %s\n",DiskMapFile.c_str());
          return false;
       }

       std::map<std::string,std::string>::iterator itrDisk;
       std::string tgtDisk,targetPv;
       int found;
  
       for(itrDisk=SrcTgtDiskMap.begin();itrDisk!=SrcTgtDiskMap.end();itrDisk++)
       {
          tgtDisk=itrDisk->second;
          log(SV_LOG_DEBUG,"Deleting the Disk : %s\n",tgtDisk.c_str());

          found=tgtDisk.find_last_of("//");
          targetPv=tgtDisk.substr(found+1);
 
          strCommnadToExecute = string("rmdev -dl ");
          strCommnadToExecute += targetPv;

          if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,NUM_RETRIES))
          {
             log(SV_LOG_ERROR,"Failed to execute the command %s\n",strCommnadToExecute.c_str());
             return false;
          }
       }

       strCommnadToExecute.clear();
       l_CmdOutput.clear();
    
       std::map<std::string,std::string> SrcTgtVtdMap;
       if(false == ReadFile(DiskVtdMapFile,SrcTgtVtdMap))
       {
          log(SV_LOG_ERROR,"Unable to read the contents of %s\n",DiskVtdMapFile.c_str());
          return false;
       }

       std::map<std::string,std::string>::iterator itrVtd;
       std::string tgtVtd;
 
       for(itrVtd = SrcTgtVtdMap.begin(); itrVtd != SrcTgtVtdMap.end(); itrVtd++)
       {
          tgtVtd=itrVtd->second;
          log(SV_LOG_DEBUG,"Deleting the VTD : %s\n",tgtVtd.c_str());
         
          strCommnadToExecute = string("ssh ");
          strCommnadToExecute += HMCUser;
          strCommnadToExecute += string("@");
          strCommnadToExecute += HMCIp;
          strCommnadToExecute += string(" ");
          strCommnadToExecute += string("\'");
          strCommnadToExecute += string("viosvrcmd -m ");
          strCommnadToExecute += ManagedSystem;
          strCommnadToExecute += string(" -p ");
          strCommnadToExecute += VIOServerName;
          strCommnadToExecute += string(" -c ");
          strCommnadToExecute += string("\"");
          strCommnadToExecute += string("rmvdev -vtd ");
          strCommnadToExecute += tgtVtd;
          strCommnadToExecute += string("\"");
          strCommnadToExecute += string("\'");
 
          if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,NUM_RETRIES))
          {
             log(SV_LOG_ERROR,"Failed to execute the command %s\n",strCommnadToExecute.c_str());
             return false;
          }  
       }

       strCommnadToExecute.clear();
       l_CmdOutput.clear();
    
       std::map<std::string,std::string> SrcTgtLvMap;
       if(false == ReadFile(DiskLvFile,SrcTgtLvMap))
       {
          log(SV_LOG_ERROR,"Unable to read the contents of %s\n",DiskLvFile.c_str());
          return false;
       }
 
       /*std::map<std::string,std::string>::iterator itrLv;
       std::string tgtLv;

       for(itrLv = SrcTgtLvMap.begin(); itrLv != SrcTgtLvMap.end(); itrLv++)
       {
          tgtLv=itrLv->second;

          log(SV_LOG_DEBUG,"Deleting the LV : %s\n",tgtLv.c_str());
         
          strCommnadToExecute = string("ssh ");
          strCommnadToExecute += HMCUser;
          strCommnadToExecute += string("@");
          strCommnadToExecute += HMCIp;
          strCommnadToExecute += string(" ");
          strCommnadToExecute += string("\'");
          strCommnadToExecute += string("viosvrcmd -m ");
          strCommnadToExecute += ManagedSystem;
          strCommnadToExecute += string(" -p ");
          strCommnadToExecute += VIOServerName;
          strCommnadToExecute += string(" -c ");
          strCommnadToExecute += string("\"");
          strCommnadToExecute += string("rmlv ");
          strCommnadToExecute += string(" -f ");
          strCommnadToExecute += tgtLv;
          strCommnadToExecute += string("\"");
          strCommnadToExecute += string("\'");

          if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,NUM_RETRIES))
          {
             log(SV_LOG_ERROR,"Failed to execute the command %s\n",strCommnadToExecute.c_str());
             return false;
          }
       }*/
    }

   log(SV_LOG_DEBUG,"Exting %s\n",__FUNCTION__);
   return true;
}

bool Discovery(std::string HostId, std::string CXIpAddress, std::string CXPort, int isHttps)
{
    log(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    
    std::string strCxApiResXmlFile = installLocation + string("/failover_data/AIXP2V/") + HostId + string("/") + std::string("dicovery.xml");
    log(SV_LOG_DEBUG,"CXAPIResponseFile : %s\n",strCxApiResXmlFile.c_str());

    std::string strXmlData = prepareXmlForGetHostInfo(HostId);
    if ( false == processXmlDataForGetHostInfo(strXmlData,CXIpAddress,CXPort,isHttps,strCxApiResXmlFile))
    {
        log(SV_LOG_ERROR,"Failed to process the request XML data of CX API call for host : %s\n",HostId.c_str());
        return false;
    }

    if ( false == parseCxApiXmlFile(strCxApiResXmlFile,HostId))
    {
        log(SV_LOG_ERROR,"Failed to parse [%s] for host : %s\n",strCxApiResXmlFile.c_str(),HostId.c_str());
        return false;
    }

    log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

bool RegisterHostUsingCdpCli()
{
   log(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
   bool rv = true;

   std::string strCommnadToExecute ;
   std::string l_CmdOutput;
   strCommnadToExecute += installLocation;
   strCommnadToExecute += string("/bin/");
   strCommnadToExecute += string("cdpcli --registerhost");

   log(SV_LOG_INFO,"CDPCLI command : %s\n", strCommnadToExecute.c_str());

   if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,NUM_RETRIES))
   {
      log(SV_LOG_ERROR,"CDPCLI register host operation got failed\n");
      rv = false;
   }
   sleep(15);

   log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
   return rv;

}

bool GetVioServer(std::string HMCIp, std::string HMCUser, std::string MTIpAddress, std::string& VIOServerName,std::string& VIOServerIp, std::string& ManagedSystem, std::string& LparName,int& PartitionIdofMT)
{
    log(SV_LOG_DEBUG,"ENTERING %s\n",__FUNCTION__);
    
    std::string strCommnadToExecute ;
    std::string l_CmdOutput;

    strCommnadToExecute = string("ssh ");
    strCommnadToExecute += HMCUser;
    strCommnadToExecute += string("@");
    strCommnadToExecute += HMCIp;
    strCommnadToExecute += string(" ");

    strCommnadToExecute += string("lssyscfg -r sys -F name");
    if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
    {
       log(SV_LOG_ERROR,"Unable run command %s on HMC \n",strCommnadToExecute.c_str());
       return false;
    }

    std::list<std::string> ManagedServerNameList = tokenizeString(l_CmdOutput, "\n");

    strCommnadToExecute.clear();
    l_CmdOutput.clear();

    std::list<std::string>::iterator itrMsList;
    for(itrMsList=ManagedServerNameList.begin();itrMsList!=ManagedServerNameList.end();++itrMsList)
    {
       strCommnadToExecute = string("ssh ");
       strCommnadToExecute += HMCUser;
       strCommnadToExecute += string("@");
       strCommnadToExecute += HMCIp;
       strCommnadToExecute += string(" ");

       strCommnadToExecute += string("lssyscfg -r lpar -m ");
       strCommnadToExecute += *itrMsList;
       strCommnadToExecute += string(" -F name,lpar_id,rmc_ipaddr");

       if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
       {
          log(SV_LOG_ERROR,"Unable run command %s on HMC \n",strCommnadToExecute.c_str());
       }

       std::list<std::string> LParNameAndIpList = tokenizeString(l_CmdOutput, "\n"); 
       std::list<std::string> keyValueTokens;
       LparData lparData;
       std::string lparName;
       std::string lparIp;
       std::list<LparData> lparDataList;

       std::list<std::string>::iterator itrLparList;
       for(itrLparList=LParNameAndIpList.begin();itrLparList!=LParNameAndIpList.end();itrLparList++)
       {
           keyValueTokens = tokenizeString((*itrLparList), ",");
           lparName=keyValueTokens.front();
           lparIp=keyValueTokens.back();

           lparData.lparName=lparName;
           lparData.lparIp=lparIp;

           if ( MTIpAddress == lparIp )
           {
               ManagedSystem=*itrMsList;
               LparName=lparName;
           }
           lparDataList.push_back(lparData);

           g_MSLparMap.insert(std::make_pair<std::string,std::list<LparData> >(*itrMsList,lparDataList));
           lparDataList.clear();
       }

       strCommnadToExecute.clear();
       l_CmdOutput.clear();
     }

     if(ManagedSystem.empty())
     {
        log(SV_LOG_ERROR,"Not able to get Managed System [%s]\n",ManagedSystem.c_str());
        return false;
     }


     strCommnadToExecute = string("ssh ");
     strCommnadToExecute += HMCUser;
     strCommnadToExecute += string("@");
     strCommnadToExecute += HMCIp;
     strCommnadToExecute += string(" ");

     strCommnadToExecute += string("lssyscfg -r prof -m ");
     strCommnadToExecute += ManagedSystem;
     strCommnadToExecute += string(" --filter");
     strCommnadToExecute += string(" lpar_names=");
     strCommnadToExecute += LparName;
     strCommnadToExecute += string(" -F virtual_scsi_adapters");

     if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
     {
        log(SV_LOG_ERROR,"Unable run command %s on HMC \n",strCommnadToExecute.c_str());
        return false;
     }

     std::list<std::string> PartitionIdList = tokenizeString(l_CmdOutput, "/");
     std::list<std::string>::iterator itrParList = PartitionIdList.begin();

     strCommnadToExecute.clear();
     l_CmdOutput.clear();

     //Advancing through 4th element to get the VIOS server.
     std::advance(itrParList,3);

     VIOServerName=*itrParList;

     strCommnadToExecute = string("ssh ");
     strCommnadToExecute += HMCUser;
     strCommnadToExecute += string("@");
     strCommnadToExecute += HMCIp;
     strCommnadToExecute += string(" ");

     strCommnadToExecute += string("lssyscfg -r prof -m ");
     strCommnadToExecute += ManagedSystem;
     strCommnadToExecute += string(" --filter");
     strCommnadToExecute += string(" lpar_names=");
     strCommnadToExecute += LparName;
     strCommnadToExecute += string(" -F lpar_id");

     if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
     {
        log(SV_LOG_ERROR,"Unable run command %s on HMC \n",strCommnadToExecute.c_str());
        return false;
     }

     PartitionIdofMT=atoi(l_CmdOutput.c_str());
     log(SV_LOG_DEBUG,"Partition of MT %d\n",PartitionIdofMT);

     strCommnadToExecute.clear();
     l_CmdOutput.clear();

     strCommnadToExecute = string("ssh ");
     strCommnadToExecute += HMCUser;
     strCommnadToExecute += string("@");
     strCommnadToExecute += HMCIp;
     strCommnadToExecute += string(" ");

     strCommnadToExecute += string("lssyscfg -r lpar -m ");
     strCommnadToExecute += ManagedSystem;
     strCommnadToExecute += string(" -F name,rmc_ipaddr");

     if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
     {
        log(SV_LOG_ERROR,"Unable run command %s on HMC \n",strCommnadToExecute.c_str());
        return false;
     }

     std::list<std::string> LParNameIpList = tokenizeString(l_CmdOutput, "\n");
     std::list<std::string> NameIpTokens;

     std::string viosIp,viosName;
     std::list<std::string>::iterator itrLpar;
     for(itrLpar=LParNameIpList.begin();itrLpar!=LParNameIpList.end();itrLpar++)
     {
        NameIpTokens = tokenizeString((*itrLpar), ",");

        viosName=NameIpTokens.front();
        viosIp=NameIpTokens.back();

        if(VIOServerName == viosName)
        {
            VIOServerIp=viosIp;
        }
     }

     log(SV_LOG_DEBUG,"Managed System ID : %d\n",ManagedSystem.c_str());
     log(SV_LOG_DEBUG,"LparName : %s\n",LparName.c_str());
     log(SV_LOG_DEBUG,"VIOServer Name : %s\n",VIOServerName.c_str());
     log(SV_LOG_DEBUG,"VIOServer Ip : %s\n",VIOServerIp.c_str());
     log(SV_LOG_DEBUG,"Partition Id of MT LPAR : %d\n",PartitionIdofMT);

     return true;
}

bool FindIfLvExists(std::list<std::string> ExistingLvs,std::string tgtDisk)
{
    log(SV_LOG_DEBUG,"ENTERING %s\n",__FUNCTION__);

    std::list<std::string>::iterator itrlv;

    for(itrlv=ExistingLvs.begin();itrlv!=ExistingLvs.end();itrlv++)
    {
       if( *itrlv == tgtDisk)
       {
           log(SV_LOG_ERROR,"Lv %s is already exists on the VIOS\n",tgtDisk.c_str());
           return true;
       }
    }
    log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return false;
}

void printSrcTgtMap(std::map<std::string,std::string> SrcTgtDiskMap)
{
   log(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

   log(SV_LOG_DEBUG,"SourceDisk  - TargetDisk \n");

   for(std::map<std::string,std::string>::iterator itr=SrcTgtDiskMap.begin();itr != SrcTgtDiskMap.end();itr++)
   {
      log(SV_LOG_DEBUG,"%s - %s \n",itr->first.c_str(),itr->second.c_str());
   }

   log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

bool GetVirtualScsiAdapter(std::string HMCUser,std::string HMCIp,std::string ManagedSystem,std::string VIOServerName,std::string VIOServerIp,int PartitionIdofMT,std::map<int,std::list<VirScsiAdapter> >& virScsiAdapter,std::string LparName)
{
   log(SV_LOG_DEBUG,"ENTERING %s\n",__FUNCTION__);

   std::string strCommnadToExecute ;
   std::string l_CmdOutput;

   strCommnadToExecute = string("ssh ");
   strCommnadToExecute += HMCUser;
   strCommnadToExecute += string("@");
   strCommnadToExecute += HMCIp;
   strCommnadToExecute += string(" ");
   strCommnadToExecute += string("\'");
   strCommnadToExecute += string("viosvrcmd -m ");
   strCommnadToExecute += ManagedSystem;
   strCommnadToExecute += string(" -p ");
   strCommnadToExecute += VIOServerName;
   strCommnadToExecute += string(" -c ");
   strCommnadToExecute += string("\"");
   strCommnadToExecute += string("lsmap -all");
   strCommnadToExecute += string("\"");
   strCommnadToExecute += string(" | grep vhost");
   strCommnadToExecute += string("\'");

   if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
   {
       log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
       return false;
   }
   std::list<std::string> scsiAdapterList=tokenizeString(l_CmdOutput,"\n");

   std::list<std::string> scsiAdapterTokens;
   std::list<std::string>::iterator itrScsiAdap=scsiAdapterList.begin();
   std::string vhostname,phyLoc,parId;
   int partitionId;
   std::stringstream tempstream;
   VirScsiAdapter virScsiAdap;
   std::list<VirScsiAdapter> listVirScsiAdap;

   log(SV_LOG_DEBUG,"vhostname - PhyLoc - PartitionId \n");
   while(itrScsiAdap != scsiAdapterList.end())
   {
      scsiAdapterTokens = tokenizeString((*itrScsiAdap)," ");

      tempstream.clear();
      vhostname=scsiAdapterTokens.front();
      scsiAdapterTokens.pop_front();
      phyLoc=scsiAdapterTokens.front();
      parId=scsiAdapterTokens.back();

      tempstream << parId;
      tempstream >> std::hex >> partitionId;

      log(SV_LOG_DEBUG,"%s - %s - %d\n",vhostname.c_str(),phyLoc.c_str(),partitionId);
      
      if(partitionId == PartitionIdofMT)
      {
          virScsiAdap.vAdapter=vhostname;
          virScsiAdap.phyLoc=phyLoc;

          listVirScsiAdap.push_back(virScsiAdap);
      }
      itrScsiAdap++;
   }

   if (listVirScsiAdap.empty())
   {
     strCommnadToExecute.clear();
     l_CmdOutput.clear();

     strCommnadToExecute = string("ssh ");
     strCommnadToExecute += HMCUser;
     strCommnadToExecute += string("@");
     strCommnadToExecute += HMCIp;
     strCommnadToExecute += string(" ");

     strCommnadToExecute += string("lssyscfg -r prof -m ");
     strCommnadToExecute += ManagedSystem;
     strCommnadToExecute += string(" --filter lpar_names=");
     strCommnadToExecute += LparName;
     strCommnadToExecute += string(" -F virtual_scsi_adapters");
     strCommnadToExecute += string("| sed 's/\"//g''");

     if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
     {
        log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
        return false;
     }
     std::list<std::string> scsiAdaptersList=tokenizeString(l_CmdOutput,"\n");

     std::list<std::string> scsiAdapters=tokenizeString(scsiAdaptersList.front(),",");
     std::list<std::string> scsiAdapterSlots;

     for(std::list<std::string>::iterator itr=scsiAdapters.begin();itr != scsiAdapters.end();itr++)
     {
         strCommnadToExecute.clear();
         l_CmdOutput.clear();

         strCommnadToExecute = string("echo ");
         strCommnadToExecute += *itr;
         strCommnadToExecute += string("|awk -F/ '{print $(NF-1)}'");
         if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
         {
            log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
            return false;
         }
         std::list<std::string> slots=tokenizeString(l_CmdOutput,"\n");
         std::string slot=slots.front();
         scsiAdapterSlots.push_back(slot);
     }

     strCommnadToExecute.clear();
     l_CmdOutput.clear();

     strCommnadToExecute = string("ssh ");
     strCommnadToExecute += HMCUser;
     strCommnadToExecute += string("@");
     strCommnadToExecute += HMCIp;
     strCommnadToExecute += string(" ");
     strCommnadToExecute += string("\'");
     strCommnadToExecute += string("viosvrcmd -m ");
     strCommnadToExecute += ManagedSystem;
     strCommnadToExecute += string(" -p ");
     strCommnadToExecute += VIOServerName;
     strCommnadToExecute += string(" -c ");
     strCommnadToExecute += string("\"");
     strCommnadToExecute += string("lsmap -all");
     strCommnadToExecute += string("\"");
     strCommnadToExecute += string(" | grep vhost");
     strCommnadToExecute += string("\'");
  

     if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
     {
         log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
         return false;
     }
     std::list<std::string> vhostSlots=tokenizeString(l_CmdOutput,"\n");
     std::map<std::string,std::string> l_hostSlotMap;
     for(std::list<std::string>::iterator itrvhosts=vhostSlots.begin();itrvhosts!=vhostSlots.end();itrvhosts++)
     {
         strCommnadToExecute.clear();
         l_CmdOutput.clear();

         strCommnadToExecute = string("echo ");
         strCommnadToExecute += *itrvhosts;
         strCommnadToExecute += string("| awk '{print $2}' | awk -F- '{print $NF}'|sed 's/C//g'");

         if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
         {
            log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
            return false;
         }
         std::list<std::string> ServerSlots=tokenizeString(l_CmdOutput,"\n");
         std::string ServerSlot=ServerSlots.front();

         strCommnadToExecute.clear();
         l_CmdOutput.clear();

         strCommnadToExecute = string("echo ");
         strCommnadToExecute += *itrvhosts;
         strCommnadToExecute += string("|awk '{print $1}'");

         if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
         {
            log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
            return false;
         }
         std::list<std::string> vhosts=tokenizeString(l_CmdOutput,"\n");
         std::string vhost=vhosts.front();
         l_hostSlotMap.insert(make_pair<std::string,std::string>(ServerSlot,vhost));

     }

     std::string vscsihost;
     std::map<std::string,std::string>::iterator itrmap;

     for(std::list<std::string>::iterator itrSlots=scsiAdapterSlots.begin();itrSlots!=scsiAdapterSlots.end();itrSlots++)
     {
        if(l_hostSlotMap.find(*itrSlots) != l_hostSlotMap.end())
        {
           vscsihost=l_hostSlotMap.find(*itrSlots)->second;
           virScsiAdap.vAdapter=vscsihost;

           strCommnadToExecute.clear();
           l_CmdOutput.clear();

           strCommnadToExecute = string("ssh ");
           strCommnadToExecute += HMCUser;
           strCommnadToExecute += string("@");
           strCommnadToExecute += HMCIp;
           strCommnadToExecute += string(" ");
           strCommnadToExecute += string("\'");
           strCommnadToExecute += string("viosvrcmd -m ");
           strCommnadToExecute += ManagedSystem;
           strCommnadToExecute += string(" -p ");
           strCommnadToExecute += VIOServerName;
           strCommnadToExecute += string(" -c ");
           strCommnadToExecute += string("\"");
           strCommnadToExecute += string("lsmap -all");
           strCommnadToExecute += string("\"");
           strCommnadToExecute += string(" | grep -w ");
           strCommnadToExecute += vscsihost;
           strCommnadToExecute += string("\'");
           strCommnadToExecute += string("| awk '{print $2}' ");
   
           if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
           {
              log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
              return false;
           }
           std::list<std::string> LocList=tokenizeString(l_CmdOutput,"\n");
           std::string phyLoc=LocList.front();

           virScsiAdap.phyLoc=phyLoc;

           listVirScsiAdap.push_back(virScsiAdap);
        }
     }

   }

   if (listVirScsiAdap.empty())
   {
       log(SV_LOG_ERROR,"Virtual Scsi Adapter is not found for the partition Id : %d\n",PartitionIdofMT);
       return false;
   }

   virScsiAdapter.insert(std::make_pair<int,std::list<VirScsiAdapter> >(PartitionIdofMT,listVirScsiAdap));

   log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
   return true;
}

bool AttachTgtDevicesToMt(std::string HMCUser,std::string HMCIp,std::string ManagedSystem,std::string VIOServerName,std::map<std::string,std::string> SrcDiskTgtLvMap,std::string VIOServerIp,std::list<VirScsiAdapter> listVirScsiAdapter,std::string SrcHostId, std::map<std::string,std::string>& l_SrcTgtDiskMap)
{
   log(SV_LOG_DEBUG,"ENTERING %s\n",__FUNCTION__);

   std::string strCommnadToExecute ;
   std::string l_CmdOutput;
   std::string tgtLv,srcDisk,tgtVtd,tgtVtdLun;
   std::list<VirScsiAdapter> tempvirScsiAdapter;
   VirScsiAdapter vScsidapter;
   std::string adapter,phyLoc,tgtDisk;

   std::string DiskMapFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/SrcTgtDiskMap.dat");
   std::string DiskVtdMapFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/SrcTgtVtdMap.dat");

   if(isRecovery == 1  || g_isResume == 1)
   {
      std::ofstream diskvtdmap(DiskVtdMapFile.c_str(),std::ios::trunc);
      diskvtdmap.close();

      std::ofstream diskmap(DiskMapFile.c_str(),std::ios::trunc);
      diskmap.close();
   }

   std::map<std::string,std::string> l_SrcDiskTgtVTDMap;
   std::map<std::string,std::string> l_SrcDiskTgtLunMap;


   std::map<std::string,std::string>::iterator itrDiskMap;
   for(itrDiskMap=SrcDiskTgtLvMap.begin();itrDiskMap!=SrcDiskTgtLvMap.end();itrDiskMap++)
   {
      if(!tempvirScsiAdapter.empty())
      {
        vScsidapter=tempvirScsiAdapter.front();
        adapter=vScsidapter.vAdapter;
        phyLoc=vScsidapter.phyLoc;
        tempvirScsiAdapter.pop_front();
      }
      else
      {
          tempvirScsiAdapter=listVirScsiAdapter;
          vScsidapter=tempvirScsiAdapter.front();
          adapter=vScsidapter.vAdapter;
          phyLoc=vScsidapter.phyLoc;
      }
      srcDisk=itrDiskMap->first;
      tgtLv=itrDiskMap->second;

      strCommnadToExecute = string("ssh ");
      strCommnadToExecute += HMCUser;
      strCommnadToExecute += string("@");
      strCommnadToExecute += HMCIp;
      strCommnadToExecute += string(" ");
      strCommnadToExecute += string("\'");
      strCommnadToExecute += string("viosvrcmd -m ");
      strCommnadToExecute += ManagedSystem;
      strCommnadToExecute += string(" -p ");
      strCommnadToExecute += VIOServerName;
      strCommnadToExecute += string(" -c ");
      strCommnadToExecute += string("\"");
      strCommnadToExecute += string("mkvdev -vdev ");
      strCommnadToExecute += tgtLv;
      strCommnadToExecute += string(" -vadapter ");
      strCommnadToExecute += adapter;
      strCommnadToExecute += string("\"");
      strCommnadToExecute += string("\'");

      if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
      {
         log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
         printf("Failed to attach the LV [%s] to the MT \n",tgtLv.c_str());

         log(SV_LOG_INFO,"Cleaning up the disks whatever created \n");
         printf("Cleaning up the disks whatever created .......\n\n");

         if(false == CleanupDisks())
         {
             log(SV_LOG_INFO,"Cleaning of Disks is not Done Properly, Manual Cleanup is Required\n");
             printf("Cleaning of Disks is not Done Properly, Manual Cleanup is Required\n");
         }
         return false;
      }
      std::list<std::string> OutputStringList=tokenizeString(l_CmdOutput," ");
      tgtVtd = OutputStringList.front();

      l_SrcDiskTgtVTDMap.insert(std::make_pair<std::string,std::string>(srcDisk,tgtVtd));

      std::ofstream SrcTgtVtdMapFile(DiskVtdMapFile.c_str(),std::ios::out|std::ios::app);
      if(!SrcTgtVtdMapFile.is_open())
      {
          log(SV_LOG_ERROR,"Failed to open file %s\n",DiskVtdMapFile.c_str());
          return false;
      }

      std::string srctgtvtd = srcDisk + string("=") + tgtVtd;

      SrcTgtVtdMapFile << srctgtvtd;
      SrcTgtVtdMapFile << string("\n");
      SrcTgtVtdMapFile.close();

      strCommnadToExecute.clear();
      l_CmdOutput.clear();

      if(isRecovery != 1 )
      {
         strCommnadToExecute = string("cfgmgr ");

         if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
         {
            log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
         }

         strCommnadToExecute.clear();
         l_CmdOutput.clear();

         strCommnadToExecute = string("ssh ");
         strCommnadToExecute += HMCUser;
         strCommnadToExecute += string("@");
         strCommnadToExecute += HMCIp;
         strCommnadToExecute += string(" ");
         strCommnadToExecute += string("\'");
         strCommnadToExecute += string("viosvrcmd -m ");
         strCommnadToExecute += ManagedSystem;
         strCommnadToExecute += string(" -p ");
         strCommnadToExecute += VIOServerName;
         strCommnadToExecute += string(" -c ");
         strCommnadToExecute += string("\"");
         strCommnadToExecute += string("lsmap -vadapter ");
         strCommnadToExecute += adapter;
         strCommnadToExecute += string(" -field vtd lun ");
         strCommnadToExecute += string("\"");
         strCommnadToExecute += string(" | grep -v ^$ ");
         strCommnadToExecute += string("\'");
         strCommnadToExecute += string(" | awk -F\" \" '{print $2}'");

         if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
         {
            log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
            return false;
         }

         std::list<std::string> VtdLunList = tokenizeString(l_CmdOutput, "\n");

         std::string element,lunId;
         element="";

         std::list<std::string>::iterator itrVtdLun = VtdLunList.begin();
         while(itrVtdLun != VtdLunList.end())
         {
            element=(*itrVtdLun);
 
            if (element == tgtVtd)
            {
               itrVtdLun++;
               lunId=(*itrVtdLun);
               break;
            }
           itrVtdLun++;
         }

         log(SV_LOG_DEBUG,"LUN Id of the tgtVtd %s is %s \n",tgtVtd.c_str(),lunId.c_str());

         l_SrcDiskTgtLunMap.insert(std::make_pair<std::string,std::string>(srcDisk,lunId));

         std::string lunNum;

         strCommnadToExecute.clear();
         l_CmdOutput.clear();

         strCommnadToExecute = string("echo ");
         strCommnadToExecute += lunId;
         strCommnadToExecute += string(" | sed 's/0x//g' ");

         if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
         {
            log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
            return false;
         }

         std::list<std::string> LunList = tokenizeString(l_CmdOutput, "\n");

         lunNum=LunList.front();
         log(SV_LOG_DEBUG,"LUN Number %s\n",lunNum.c_str());

         strCommnadToExecute.clear();
         l_CmdOutput.clear();

         strCommnadToExecute = string("echo ");
         strCommnadToExecute += phyLoc;
         strCommnadToExecute += string(" | awk -F\"-\" '{print $1}'");

         if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
         {
            log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
            return false;
         }

         std::list<std::string> LocList = tokenizeString(l_CmdOutput, "\n");

         std::string vAdapterLoc = LocList.front();
         log(SV_LOG_DEBUG,"PhyLoc of Virtual Scsi Adapter %s is %s\n",adapter.c_str(),vAdapterLoc.c_str());

         strCommnadToExecute.clear();
         l_CmdOutput.clear();

         std::list<std::string> tgtDiskList;

         int i=1;
         while ( i < NUM_RETRIES)
         {
            if( tgtDiskList.empty())
            {
               strCommnadToExecute = string("lscfg | grep \"");
               strCommnadToExecute += vAdapterLoc;
               strCommnadToExecute += string("\" | grep \"");
               strCommnadToExecute += lunNum;
               strCommnadToExecute += string("\" | awk -F\" \" '{print $2}' ");

               if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
               {
                  log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
                  return false;
              }
              tgtDiskList = tokenizeString(l_CmdOutput, "\n");
           }
           else
           { 
               break;
           }
           i++;
        }

        if( tgtDiskList.empty())
        {
         strCommnadToExecute.clear();
         l_CmdOutput.clear();

         strCommnadToExecute = string("cfgmgr ");

         if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
         {
            log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
         }

         strCommnadToExecute.clear();
         l_CmdOutput.clear();

         strCommnadToExecute = string("lscfg | grep \"");
         strCommnadToExecute += vAdapterLoc;
         strCommnadToExecute += string("\" | grep \"");
         strCommnadToExecute += lunNum;
         strCommnadToExecute += string("\" | awk -F\" \" '{print $2}' ");

         if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
         {
            log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
            return false;
          }
            tgtDiskList = tokenizeString(l_CmdOutput, "\n");
        }
       
        if( tgtDiskList.empty())
        {
            log(SV_LOG_ERROR,"Disk attached is not visible on MT \n");
            return false;
        }

         if(! tgtDiskList.empty())
             tgtDisk=tgtDiskList.front();
 
         std::string targetDisk = string("/dev/") + tgtDisk;

         l_SrcTgtDiskMap.insert(std::make_pair<std::string,std::string>(srcDisk,targetDisk));

         std::ofstream SrcTgtDiskMapFile(DiskMapFile.c_str(),std::ios::out|std::ios::app);
         if(!SrcTgtDiskMapFile.is_open())
         {
            log(SV_LOG_ERROR,"Failed to open file %s\n",DiskMapFile.c_str());
            return false;
         }
         std::string srctgtdisk = srcDisk + string("=") + targetDisk;
         SrcTgtDiskMapFile << srctgtdisk; 
         SrcTgtDiskMapFile << string("\n");
         SrcTgtDiskMapFile.close();

         strCommnadToExecute.clear();
         l_CmdOutput.clear();
       }
   }  

   log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
   return true;
}

bool CheckIfAnyLvAvailable(std::string& tgtLv,std::string SrcHostId,long RequiredPPs,std::string ManagedSystem,std::string VIOServerName)
{
    log(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    std::string HMCUser = g_ProtectionSettings.HMCUser;
    std::string HMCIp = g_ProtectionSettings.HMCIpAddress;
    std::string VgOnVios = g_ProtectionSettings.VGNameOnVIOS;

    std::string strCommnadToExecute ;
    std::string l_CmdOutput;
    
    strCommnadToExecute = string("ssh ");
    strCommnadToExecute += HMCUser;
    strCommnadToExecute += string("@");
    strCommnadToExecute += HMCIp;
    strCommnadToExecute += string(" ");
    strCommnadToExecute += string("\'");
    strCommnadToExecute += string("viosvrcmd -m ");
    strCommnadToExecute += ManagedSystem;
    strCommnadToExecute += string(" -p ");
    strCommnadToExecute += VIOServerName;
    strCommnadToExecute += string(" -c ");
    strCommnadToExecute += string("\"");
    strCommnadToExecute += string("lsvg -lv ");
    strCommnadToExecute += VgOnVios;
    strCommnadToExecute += string("\"");
    strCommnadToExecute += string("\'");
    strCommnadToExecute += string(" | grep -v ");
    strCommnadToExecute += VgOnVios;
    strCommnadToExecute += string(" | grep -v \"LV NAME\" | awk -F\" \" '{print $1 \" \" $4 \" \" $6}' ");
    strCommnadToExecute += string ("|sort -k2n");

    if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
    {
       log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
       return false;
    }

    std::list<std::string> LvList = tokenizeString(l_CmdOutput, "\n");
    std::list<std::string>::iterator itrLvs;

    std::list<std::string> lvs;
    std::string lvname, ppsize, status;
    long size;

    log(SV_LOG_DEBUG,"Followong are the LVs and their size and status \n");

    for(itrLvs = LvList.begin();itrLvs != LvList.end(); itrLvs++)
    {
      lvs = tokenizeString((*itrLvs)," ");

      lvname = lvs.front();
      lvs.pop_front();
      ppsize = lvs.front();
      status = lvs.back();

      size = atol(ppsize.c_str());

      log(SV_LOG_INFO," %s - %s - %s \n",lvname.c_str(),ppsize.c_str(),status.c_str());

      if(status == string("closed/syncd") && size >= RequiredPPs)
      {
          tgtLv = lvname;
          log(SV_LOG_DEBUG,"Selecting existing Lv %s\n",tgtLv.c_str());
          break;
      }
    }

    log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);

    if(!tgtLv.empty())
        return true;
    else
        return false;
}

bool PrepareTarget()
{
    log(SV_LOG_DEBUG,"ENTERING %s\n",__FUNCTION__);

    std::string MTIpAddress,VIOServerIp,VIOServerName,ManagedSystem;
    std::string LparName;
    int PartitionIdofMT;

    std::string HMCIp = g_ProtectionSettings.HMCIpAddress;
    std::string HMCUser = g_ProtectionSettings.HMCUser;

    std::string strCommnadToExecute ;
    std::string l_CmdOutput;

    strCommnadToExecute = string("grep `hostname` /etc/hosts ");
    strCommnadToExecute += string("| grep -v \"^#\"");
    strCommnadToExecute += string("| awk '{print $1}'");

    if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
    {
        log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
        return false;
    }
    std::list<std::string> hostNameList = tokenizeString(l_CmdOutput, "\n");
    MTIpAddress = hostNameList.front();

    log(SV_LOG_DEBUG,"MTIpAddress : %s\n",MTIpAddress.c_str());

    strCommnadToExecute.clear();
    l_CmdOutput.clear();

    if(false == GetVioServer(HMCIp,HMCUser,MTIpAddress,VIOServerName,VIOServerIp,ManagedSystem,LparName,PartitionIdofMT))
    {
        log(SV_LOG_ERROR,"Unable to get VIOServer\n");
        return false;
    }

    std::map<int,std::list<VirScsiAdapter> > virScsiAdapter;
    
    if ( false == GetVirtualScsiAdapter(HMCUser,HMCIp,ManagedSystem,VIOServerName,VIOServerIp,PartitionIdofMT,virScsiAdapter,LparName))
    {
        log(SV_LOG_ERROR,"Unable to get virtual scsi adapter\n");
        return false;
    }

    std::list<VirScsiAdapter> listVirScsiAdap;
    std::map<int,std::list<VirScsiAdapter> >::iterator itrVirScsiAdap;
    itrVirScsiAdap=virScsiAdapter.find(PartitionIdofMT);

    if(itrVirScsiAdap != virScsiAdapter.end())
    {
       listVirScsiAdap=itrVirScsiAdap->second;
    }

    std::list<HostData> :: iterator itrhostList;
    std::string SrcHostId;
    StorageInfo l_storageInfo;

    for(itrhostList = g_ProtectionSettings.HostList.begin(); itrhostList != g_ProtectionSettings.HostList.end(); itrhostList++ )
    {
       SrcHostId = itrhostList->HostID;

       std::map<std::string,StorageInfo> ::iterator itrStorage;
       itrStorage = g_StorageInfoMap.find(SrcHostId);

       if(itrStorage != g_StorageInfoMap.end())
       {
           l_storageInfo = itrStorage->second;
       }
       else
       {
          log(SV_LOG_ERROR,"Failed to get the Storage details of Host :%s\n",SrcHostId.c_str());
          return false;
       }

       std::string DiskLvFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/SrcTgtLvMap.dat");
       std::ofstream disklv(DiskLvFile.c_str(),std::ios::trunc);
       disklv.close();

       std::string CredFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/Servers.dat");
       std::ofstream creds(CredFile.c_str(),std::ios::trunc);
       creds.close();

       std::string DiskMapFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/SrcTgtDiskMap.dat");
       std::ofstream diskmap(DiskMapFile.c_str(),std::ios::trunc);
       diskmap.close();

       std::string DiskVtdMapFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/SrcTgtVtdMap.dat");
       std::ofstream diskvtdmap(DiskVtdMapFile.c_str(),std::ios::trunc);
       diskvtdmap.close();

       std::string ipaddresses;
       ipaddresses = string("MTAddress=")+MTIpAddress+string("\n");
       ipaddresses += string("VIOServerName=")+VIOServerName+string("\n");
       ipaddresses += string("VIOServerIp=")+VIOServerIp+string("\n");
       ipaddresses += string("ManagesSystem=")+ManagedSystem+string("\n");
       ipaddresses += string("LPARName=")+LparName+string("\n");

       std::ofstream credentials(CredFile.c_str(),std::ios::out | std::ios::app);
       if(!credentials.is_open())
       {
          log(SV_LOG_ERROR,"Failed to open file %s\n",CredFile.c_str());
          return false;
       }

       credentials << ipaddresses;
       credentials.close();

       std::string VgOnVios = g_ProtectionSettings.VGNameOnVIOS;

       log(SV_LOG_DEBUG,"VgOnVios : %s\n",VgOnVios.c_str());

       long long ViosVgPPSize;

       strCommnadToExecute = string("ssh ");
       strCommnadToExecute += HMCUser;
       strCommnadToExecute += string("@");
       strCommnadToExecute += HMCIp;
       strCommnadToExecute += string(" ");
       strCommnadToExecute += string("\'");
       strCommnadToExecute += string("viosvrcmd -m ");
       strCommnadToExecute += ManagedSystem;
       strCommnadToExecute += string(" -p ");
       strCommnadToExecute += VIOServerName;
       strCommnadToExecute += string(" -c ");
       strCommnadToExecute += string("\"");
       strCommnadToExecute += string("lsvg ");
       strCommnadToExecute += VgOnVios;
       strCommnadToExecute += string("\"");
       strCommnadToExecute += string("\'");
       strCommnadToExecute += string(" | grep \"PP SIZE:\" | awk -F\" \" \'{print $(NF-1)}\'");

       if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
       {
          log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
          return false;
       }

       std::list<std::string> PPSizeList = tokenizeString(l_CmdOutput,"\n");

       ViosVgPPSize = atol(PPSizeList.front().c_str())*1024*1024;
       log(SV_LOG_DEBUG,"PP Size of VG %s on VIOS server is %ld\n",VgOnVios.c_str(),ViosVgPPSize);

       strCommnadToExecute.clear();
       l_CmdOutput.clear();
    
       std::map<std::string,DiskProperties>::iterator itrDisks;

       int i=0;
       std::string srcDisk;
       std::string tgtLv;
       long FreePPs;
       long long srcDiskSize;
       DiskProperties diskProp;
       long RequiredPPs;

       std::string diskNum;
       char strNum[100];
       char ppNum[50];

       printf("Creating LVs for the source Host :%s\n\n",SrcHostId.c_str());
       std::map<std::string,std::string> l_SrcTgtDiskMap;

       for( itrDisks=l_storageInfo.listDiskNames.begin();itrDisks!=l_storageInfo.listDiskNames.end();itrDisks++)
       {
          srcDisk=itrDisks->first;
          diskProp=itrDisks->second;

          log(SV_LOG_DEBUG,"Creating Disk for Source Disk [%s] \n",srcDisk.c_str());

          strCommnadToExecute = string("ssh ");
          strCommnadToExecute += HMCUser;
          strCommnadToExecute += string("@");
          strCommnadToExecute += HMCIp;
          strCommnadToExecute += string(" ");
          strCommnadToExecute += string("\'");
          strCommnadToExecute += string("viosvrcmd -m ");
          strCommnadToExecute += ManagedSystem;
          strCommnadToExecute += string(" -p ");
          strCommnadToExecute += VIOServerName;
          strCommnadToExecute += string(" -c ");
          strCommnadToExecute += string("\"");
          strCommnadToExecute += string("lsvg ");
          strCommnadToExecute += VgOnVios;
          strCommnadToExecute += string("\"");
          strCommnadToExecute += string("\'");
          strCommnadToExecute += string(" | grep \"FREE PPs\" | awk -F\" \" \'{print $(NF-2)}\'");

          if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
          {
             log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
             return false;
          }

          FreePPs=atol(l_CmdOutput.c_str());

          strCommnadToExecute.clear();
          l_CmdOutput.clear();

          srcDiskSize=atol(diskProp.strSize.c_str());
          int rem=srcDiskSize%ViosVgPPSize;
          RequiredPPs = srcDiskSize/ViosVgPPSize;

          if ( rem != 0 )
             RequiredPPs++;

          log(SV_LOG_DEBUG,"Source Disk Size %ld\n",srcDiskSize);
          log(SV_LOG_DEBUG,"Required PPs %ld\n",RequiredPPs);

          std::string pps;

          if(false == CheckIfAnyLvAvailable(tgtLv,SrcHostId,RequiredPPs,ManagedSystem,VIOServerName))
          {
             if(RequiredPPs <= FreePPs)
             {
				 inm_sprintf_s(ppNum, ARRAYSIZE(ppNum), "%d", RequiredPPs);
                pps=string(ppNum);

                strCommnadToExecute = string("ssh ");
                strCommnadToExecute += HMCUser;
                strCommnadToExecute += string("@");
                strCommnadToExecute += HMCIp;
                strCommnadToExecute += string(" ");
 
                strCommnadToExecute += string("\'");
                strCommnadToExecute += string("viosvrcmd -m ");
                strCommnadToExecute += ManagedSystem;
                strCommnadToExecute += string(" -p ");
                strCommnadToExecute += VIOServerName;
                strCommnadToExecute += string(" -c ");
                strCommnadToExecute += string("\"");
                strCommnadToExecute += string("mklv");
                strCommnadToExecute += string(" ");
                strCommnadToExecute += VgOnVios;
                strCommnadToExecute += string(" ");
                strCommnadToExecute += pps;
                strCommnadToExecute += string("\"");
                strCommnadToExecute += string("\'");

                if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
                {
                   log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
                   printf("Failed to Create LVs for the source Host :%s\n",SrcHostId.c_str());
 
                   log(SV_LOG_INFO,"Cleaning up the disks whatever created \n");
                   printf("Cleaning up the disks whatever created .......\n\n");
 
                   if(false == CleanupDisks())
                   {
                     log(SV_LOG_INFO,"Cleaning of Disks is not Done Properly, Manual Cleanup is Required\n");
                     printf("Cleaning of Disks is not Done Properly, Manual Cleanup is Required\n");
                   }
                   return false;
                }

                std::list<std::string> tgtLvList = tokenizeString(l_CmdOutput,"\n");
                tgtLv = tgtLvList.front();

                log(SV_LOG_INFO,"Created LV : %s\n",tgtLv.c_str());
             }
             else
             {
                log(SV_LOG_ERROR,"Sufficient Space is not available on VG to create the disk \n");
                log(SV_LOG_ERROR,"RequiredPPs - %d, Free PPs - %d\n",RequiredPPs,FreePPs);
                printf("Sufficient space is not available on VG %s to create Disks :%s\n",VgOnVios.c_str());

                log(SV_LOG_INFO,"Cleaning up the disks whatever created \n");
                printf("Cleaning up the disks whatever created .......\n\n");
 
                if(false == CleanupDisks())
                {
                   log(SV_LOG_INFO,"Cleaning of Disks is not Done Properly, Manual Cleanup is Required\n");
                   printf("Cleaning of Disks is not Done Properly, Manual Cleanup is Required\n");
                }
                return false;
             }
           }
 
           g_SrcDiskTgtLvMap.insert(std::make_pair<std::string,std::string>(srcDisk,tgtLv));

           std::ofstream SrcTgtLvMapFile(DiskLvFile.c_str(),std::ios::out | std::ios::app);
           if(!SrcTgtLvMapFile.is_open())
           {
               log(SV_LOG_ERROR,"Failed to open file %s\n",DiskLvFile.c_str());
               return false;
           }

           std::string srctgtlv;
           srctgtlv=srcDisk + string("=") + tgtLv;

           SrcTgtLvMapFile << srctgtlv;
           SrcTgtLvMapFile << string("\n");
           SrcTgtLvMapFile.close();
 
           strCommnadToExecute.clear();
           l_CmdOutput.clear();

           diskProp.reset();

           printSrcTgtMap(g_SrcDiskTgtLvMap);
           tgtLv.clear();

           std::map<std::string,std::string> l_SrcDiskTgtLunMap;

           if( false == AttachTgtDevicesToMt(HMCUser,HMCIp,ManagedSystem,VIOServerName,g_SrcDiskTgtLvMap,VIOServerIp,listVirScsiAdap,SrcHostId,l_SrcTgtDiskMap))
           {
              log(SV_LOG_ERROR,"Unable to AttachTgtDevices to MT \n");
              printf("Failed to attach the Disks to MT \n");

              log(SV_LOG_INFO,"Cleaning up the disks whatever created \n");
              printf("Cleaning up the disks whatever created .......\n\n");

              if(false == CleanupDisks())
              {
                 log(SV_LOG_INFO,"Cleaning of Disks is not Done Properly, Manual Cleanup is Required\n");
                 printf("Cleaning of Disks is not Done Properly, Manual Cleanup is Required\n");
              }
              return false;
           }

           g_SrcDiskTgtLvMap.clear();
        }
        printf("LVs are created for the source Host :%s and attached\n\n",SrcHostId.c_str());

        strCommnadToExecute.clear();
        l_CmdOutput.clear();

        std::map<std::string,std::string>::iterator itrSrcTgtDisk;

        std::string tgtPv,targetPv;
        int found;
        for(itrSrcTgtDisk = l_SrcTgtDiskMap.begin();itrSrcTgtDisk != l_SrcTgtDiskMap.end();itrSrcTgtDisk++)
        {
           targetPv = itrSrcTgtDisk->second;
           found = targetPv.find_last_of("//");
           tgtPv = targetPv.substr(found+1);

           log(SV_LOG_DEBUG,"Assigning PVID to disk %s\n",tgtPv.c_str());
 
           strCommnadToExecute = string("chdev -l ");
           strCommnadToExecute += tgtPv;
           strCommnadToExecute += string(" -a pv=yes");
    
           if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
           {
               log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
               return false;
           }
        }

        l_SrcTgtDiskMap.clear();
        printf("Registering the host [%s] with CX \n\n",SrcHostId.c_str());

        if(false == RegisterHostUsingCdpCli())
        {
           log(SV_LOG_ERROR,"Failed to Register the Host \n");
           printf("Failed to register the host [%s] with CX \n\n",SrcHostId.c_str());

           log(SV_LOG_INFO,"Cleaning up the disks whatever created \n");
           printf("Cleaning up the disks whatever created .......\n\n");

           if(false == CleanupDisks())
           {
              log(SV_LOG_INFO,"Cleaning of Disks is not Done Properly, Manual Cleanup is Required\n");
              printf("Cleaning of Disks is not Done Properly, Manual Cleanup is Required \n\n");
           }
           return false;
        }

    }

    return true;
}

bool ConstructXmlForCxcli(std::string TgtHostId,std::string protectxml)
{
    log(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    std::ofstream protdata(protectxml.c_str(),std::ios::trunc);
    protdata.close();

    std::string planName = g_ProtectionSettings.PlanName;
    std::string batchReync = g_ProtectionSettings.BatchResyncRequired;
    std::string srcIp,tgtIp;
    std::string psIp = g_ProtectionSettings.ProcessServerIP;

    if(planName.empty())
    {
       int z;
       char buf[32];
       z=gethostname(buf, sizeof buf);
       planName=string(buf);

       log(SV_LOG_DEBUG,"Assigning default PlanName %s\n",planName.c_str());
    }

    if(batchReync.empty())
    {
       batchReync="0";
       log(SV_LOG_DEBUG,"Assigning default batchReync %s\n",batchReync.c_str());
    }

    std::ofstream data(protectxml.c_str(),std::ios::out|std::ios::app);
    if(!data.is_open())
    {
       log(SV_LOG_ERROR,"Failed to open file %s\n",protectxml.c_str());
       return false;
    }

    std::list<HostData> :: iterator itrhostList;

    std::string xml_data,xml_data1,xml_data2;
    xml_data = string("<plan><header><parameters><param name='name'>");
    xml_data += planName;
    xml_data1 = string("</param><param name='type'>DR Protection</param><param name='version'>1.1</param></parameters></header><application_options name=\"VIOS\"><batch_resync>");
    xml_data2 = batchReync;
    xml_data2 += string("</batch_resync></application_options><failover_protection><replication>");

    xml_data += xml_data1 + xml_data2;
    data << xml_data;
    data.close();

    std::string SrcHostId;
    for(itrhostList = g_ProtectionSettings.HostList.begin(); itrhostList != g_ProtectionSettings.HostList.end(); itrhostList++ )
    {
       SrcHostId = itrhostList->HostID;
       srcIp = itrhostList->IPAddress;

       std::string serversFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/Servers.dat");

       std::string srcTgtDiskFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/SrcTgtDiskMap.dat"); 

       if(false == ReadFromConfigMap(serversFile,"MTAddress",tgtIp))
       {
          log(SV_LOG_ERROR,"Unable to get the MTIpAddress from %s\n",serversFile.c_str());
          return false;
       }

       std::map<std::string,std::string> SrcTgtDiskMap;

       if(false == ReadFile(srcTgtDiskFile,SrcTgtDiskMap))
       {
          log(SV_LOG_ERROR,"Unable to read the contents of %s\n",srcTgtDiskFile.c_str());
          return false;
       }

       std::map<std::string,std::string>::iterator itrSrcTgtMap;
       std::string srcDisk,tgtDisk,sourceDisk;
       int found;

       std::string retpath = g_ProtectionSettings.RetentionPath;
       log(SV_LOG_DEBUG,"RetentionPath : %s\n",retpath.c_str());

       std::string xmljob_data,xmljob_data1,xmljob_data2,xmljob_data3,xmljob_data4,xmljob_data5,xmljob_data6,xmljob_data7;
       std::string xmljob_options,xmljob_options1,xmljob_options2,xmljob_options3,xmljob_options4,xmljob_options5,xmljob_options6;
       for(itrSrcTgtMap=SrcTgtDiskMap.begin();itrSrcTgtMap!=SrcTgtDiskMap.end();itrSrcTgtMap++)
       {
          sourceDisk = itrSrcTgtMap->first;
          found = sourceDisk.find_last_of("//");
          srcDisk = string("/dev/") + sourceDisk.substr(found+1); 
          tgtDisk = itrSrcTgtMap->second;

          xmljob_data = string("<job type=\"vx\"><server_ip type=\"source\">");
          xmljob_data += srcIp; 
          xmljob_data1 = string("</server_ip><server_ip type=\"target\">");
          xmljob_data1 += tgtIp;
          xmljob_data2 = string("</server_ip><server_hostid type=\"source\">");
          xmljob_data2 += SrcHostId;
          xmljob_data3 = string("</server_hostid><server_hostid type=\"target\">");
          xmljob_data3 += TgtHostId; 
          xmljob_data4 = string("</server_hostid><source_device>");
          xmljob_data4 += srcDisk; 
          xmljob_data5 = string("</source_device>");
          xmljob_data5 += string("<target_device>");
          xmljob_data5 += tgtDisk;
          xmljob_data6 = string("</target_device>");
          xmljob_data6 += string("<process_server_ip use_nat_ip_for_src=\"No\" use_nat_ip_for_trg=\"No\">");
          xmljob_data7 = psIp;
          xmljob_data7 += string("</process_server_ip>");

          xmljob_data += xmljob_data1 + xmljob_data2 + xmljob_data3 + xmljob_data4 + xmljob_data5 + xmljob_data6 + xmljob_data7;

          std::ofstream data1(protectxml.c_str(),std::ios::out|std::ios::app);
          if(!data1.is_open())
          {
             log(SV_LOG_ERROR,"Failed to open file %s\n",protectxml.c_str());
             return false;
          }
          data1 << xmljob_data;
          data1.close();

          xmljob_options = string("<options rawsize_required=\"Yes\" resync_required=\"Yes/no\"");
          xmljob_options += string(" secure_src_to_ps=\"yes/no\" secure_ps_to_tgt=\"yes/no\"");
          xmljob_options1 = string(" sync_option=\"fast\" compression=\"cx_base/source_base\"");
          xmljob_options1 += string(" resync_threshold_mb=\"\" differential_threshold_mb=\"\" rpo_threshold_minutes=\"\">");
          xmljob_options2 = string("<retention retain_change_writes=\"all\" disk_space_alert_threshold=\"80\"");
          xmljob_options2 += string(" insufficient_disk_space=\"purge_old_logs\" log_data_vol=\"");
          xmljob_options2 += retpath;
          xmljob_options3 = string("\" log_data_path=\"");
          xmljob_options3 += retpath;
          xmljob_options3 += string("\"><space_in_mb>256</space_in_mb><retention_policy>");
          xmljob_options3 += string("<retain_change_time unit=\"hours\" value=\"3\"></retain_change_time><time>");
          xmljob_options4 = string("<time_interval_window1 unit=\"hours\" value=\"2\" bookmark_count=\"2\" range=\"3\" ></time_interval_window1>");
          xmljob_options4 += string("<application_name customtag=\"\">File System</application_name>");
          xmljob_options5 = string("</time></retention_policy></retention>");
          xmljob_options5 += string("<auto_resync_time wait_interval=\"10\" start_time=\"10:30\" end_time=\"20:40\">Yes</auto_resync_time>");
          xmljob_options6 += string("</options></job>");

          xmljob_options += xmljob_options1 + xmljob_options2 + xmljob_options3 + xmljob_options4 + xmljob_options5 + xmljob_options6;

          std::ofstream data2(protectxml.c_str(),std::ios::out|std::ios::app);
          if(!data2.is_open())
          {
            log(SV_LOG_ERROR,"Failed to open file %s\n",protectxml.c_str());
            return false;
          }
          data2 << xmljob_options;
          data2.close();

          xmljob_data.clear();
          xmljob_data1.clear();
          xmljob_data2.clear();
          xmljob_data3.clear();
          xmljob_data4.clear();
          xmljob_data5.clear();
          xmljob_data6.clear();
          xmljob_data7.clear();

          xmljob_options.clear();
          xmljob_options1.clear();
          xmljob_options2.clear();
          xmljob_options3.clear();
          xmljob_options4.clear();
          xmljob_options5.clear();
          xmljob_options6.clear();

          srcDisk.clear();
          tgtDisk.clear();
       }
       SrcTgtDiskMap.clear();
    }

    xml_data = string("</replication></failover_protection></plan>");

    log(SV_LOG_DEBUG,"xml_data %s\n",xml_data.c_str());

    std::ofstream data3(protectxml.c_str(),std::ios::out|std::ios::app);
    if(!data3.is_open())
    {
       log(SV_LOG_ERROR,"Failed to open file %s\n",protectxml.c_str());
       return false;
    }
    data3 << xml_data;
    data3.close();

    log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

bool DeleteProtection(std::list<std::string> tgtDevList,std::string TgtHostId,std::string SrcHostId,std::string CXIpAddress,std::string CXPort, int l_isHttps)
{
    log(SV_LOG_DEBUG,"ENTERING %s\n",__FUNCTION__);

    
    std::string phpUrl = string("/ScoutAPI/CXAPI.php ");

    std::string FileForDeletion = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/PairsToDelete.dat");

    string xml_data = string("<Request Id=\"0001\" Version=\"1.0\">");
    xml_data += string("<Header>");
    xml_data += string("<Authentication>");
    xml_data += string("<AccessKeyID>DDC9525FF275C104EFA1DFFD528BD0145F903CB1</AccessKeyID>");
    xml_data += string("<AccessSignature></AccessSignature>");
    xml_data += string("</Authentication>");
    xml_data += string("</Header>");
    xml_data += string("<Body>");
    xml_data += string("<FunctionRequest Name=\"DeletePairs\" Include=\"No\">");
    xml_data += string("<ParameterGroup Id=\"DeleteList_1\">");
    xml_data += string("<Parameter Name=\"TargetHostGUID\" Value=\"");
    xml_data += TgtHostId;
    xml_data += string("\" />");

    int i=1;
    char DiskNum[100];
    std::list<std::string>::iterator itrTgtList;
    for(itrTgtList = tgtDevList.begin(); itrTgtList != tgtDevList.end(); itrTgtList++)
    {
		inm_sprintf_s(DiskNum, ARRAYSIZE(DiskNum), "%d", i);
       std::string device=string("Device")+string(DiskNum);

       xml_data += string("<Parameter Name=\"");
       xml_data += device;
       xml_data += string("\" Value=\"");
       xml_data += *itrTgtList;
       xml_data += string("\" />");

       i++;
    }

    xml_data += string("</ParameterGroup>");
    xml_data += string("</FunctionRequest>");
    xml_data += string("</Body>");
    xml_data += string("</Request>");

    std::ofstream deletePairs(FileForDeletion.c_str(),std::ios::trunc |std::ios::out );
    if(!deletePairs.is_open())
    {
       log(SV_LOG_ERROR,"Failed to open file %s\n",FileForDeletion.c_str());
       return false;
    }

    deletePairs << xml_data;
    deletePairs.close();

	log(SV_LOG_INFO,"PHP URL : %s\n", phpUrl.c_str());

	log(SV_LOG_INFO,"CXIpAddress : %s\n",CXIpAddress.c_str());
	log(SV_LOG_INFO,"CXPort : %s\n",CXPort.c_str());
	log(SV_LOG_INFO,"IsHttps : %d\n",l_isHttps);

    ResponseData Data = {0};
    std::string responsedata;
                
	curl_global_init(CURL_GLOBAL_ALL);


	CurlOptions options(CXIpAddress, atoi(CXPort.c_str()), phpUrl,l_isHttps);
	options.writeCallback( static_cast<void *>( &Data ),WriteCallbackFunction);


	Data.chResponseData = NULL;
	Data.length = 0;

	try {
		CurlWrapper cw;
		cw.post(options, xml_data);
		if( Data.length > 0)
		{
			responsedata = string(Data.chResponseData);
			if(Data.chResponseData != NULL)
				free( Data.chResponseData ) ;
		}
	} catch(ErrorException& exception )
	{
		DebugPrintf(SV_LOG_ERROR, "FAILED : %s with error %s\n",__FUNCTION__, exception.what());
	}
      
    log(SV_LOG_DEBUG,"Response Data of DeletePairs : %s\n",responsedata.c_str());

    log(SV_LOG_DEBUG,"EXTING %s\n",__FUNCTION__);
    return true;
}

bool Protection(std::string TgtHostId,std::string CXIpAddress,std::string CXPort, int l_isHttps)
{
    log(SV_LOG_DEBUG,"ENTERING %s\n",__FUNCTION__);

    if ( g_isResume != 1 )
    {
       if(false == PrepareTarget())
       {
         log(SV_LOG_DEBUG,"Prepare Target Failed \n");
         return false;
       }
    }
    
    std::string hostId = g_ProtectionSettings.HostList.front().HostID;

    std::string protectxml=installLocation + string("/failover_data/AIXP2V/") + hostId + string("/Protecion.xml");
    log(SV_LOG_DEBUG,"ProtectXml file : %s\n",protectxml.c_str());

    if(false == ConstructXmlForCxcli(TgtHostId,protectxml))
    {
       log(SV_LOG_ERROR,"Failed to Construct Xml input for create Protection \n");

       log(SV_LOG_INFO,"Cleaning up the disks whatever created \n");
       printf("Cleaning up the disks whatever created .......\n\n");

       if(false == CleanupDisks())
       {
           log(SV_LOG_INFO,"Cleaning of Disks is not Done Properly, Manual Cleanup is Required\n");
           printf("Cleaning of Disks is not Done Properly, Manual Cleanup is Required\n");
       }
       return false;
    }

    std::string reponsexml = installLocation + string("/failover_data/AIXP2V/") + hostId + string("/Response.xml");

    std::string strCxIpAddress = CXIpAddress;
    strCxIpAddress += string(":") + CXPort;
    std::string httpsStr;
    
    if ( l_isHttps == 1 )
    {
        httpsStr=string("https://");
    }
    else
    {
        httpsStr=string("http://");
    }
        
    std::string strCxUrl = httpsStr + strCxIpAddress + string("/cli/cx_cli_no_upload.php ");

    std::string strCommnadToExecute ;
    std::string l_CmdOutput;

    strCommnadToExecute = installLocation;
    strCommnadToExecute += string("/bin/cxcli ") ;
    strCommnadToExecute += strCxUrl;
    strCommnadToExecute += protectxml;
    strCommnadToExecute += string(" 42 ");

    int ret = ExecuteCommand(strCommnadToExecute,l_CmdOutput,1);

    std::ofstream outXml(reponsexml.c_str(),std::ios::out);
    if(!outXml.is_open())
    {
       log(SV_LOG_ERROR,"Failed to open file %s\n",reponsexml.c_str());
       return false;
    }

    outXml << l_CmdOutput;
    
    outXml.close();

    strCommnadToExecute.clear();
    l_CmdOutput.clear();

    strCommnadToExecute = string("grep -i \"Failed to Create Volume Replication..\" ");
    strCommnadToExecute += reponsexml;
    
    if(0 == ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
    {
       log(SV_LOG_ERROR,"Failed to Create Protection Plan \n");
       ret = 1;
    }

    strCommnadToExecute.clear();
    l_CmdOutput.clear();

    strCommnadToExecute = string("grep \"Volume Replication Pair is Created with \" ");
    strCommnadToExecute += reponsexml;
    strCommnadToExecute += string(" | awk '{print $NF}' | sed 's/(//g' | sed 's/)//g'");

    if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
    {
       log(SV_LOG_ERROR,"Unable run command %s on HMC \n",strCommnadToExecute.c_str());
       return false;
    }

    std::list<std::string> tgtDevList = tokenizeString(l_CmdOutput, "\n");

    if(ret == 1) 
    {
       log(SV_LOG_ERROR,"Failed to Create the Protection Plan \n");
       printf("Failed to Create the Protection Plan \n\n");

       if(!tgtDevList.empty())
       {
          if(false == DeleteProtection(tgtDevList,TgtHostId,hostId,CXIpAddress,CXPort,l_isHttps))
          {
            log(SV_LOG_ERROR,"Pair Deletion is Failed. Manual Deletion is required \n");
          }
          else
          {
            log(SV_LOG_DEBUG,"Pair got Deleted \n");
          }
       }

       log(SV_LOG_INFO,"Cleaning up the disks whatever created \n");
       printf("Cleaning up the disks whatever created .......\n\n");

       if(false == CleanupDisks())
       {
          log(SV_LOG_INFO,"Cleaning of Disks is not Done Properly, Manual Cleanup is Required\n");
          printf("Cleaning of Disks is not Done Properly, Manual Cleanup is Required \n\n");
       }
       return false;
    }
    else
    {
       printf("Pair Created Successfully \n\n");
    }

    log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

bool GetMapFromFile(std::string filename,std::map<std::string,std::string>& keyValueMap)
{
    log(SV_LOG_DEBUG,"ENTERING %s\n",__FUNCTION__);

    std::ifstream file( filename.c_str() );
    std::stringstream buffer;

    if ( file )
    {
        buffer << file.rdbuf();
        file.close();
    }

    std::string inm_filecontents = buffer.str();

    std::list<std::string> contentList;
    std::list<std::string> contentMap;
    std::string key,value;

    std::list<std::string>::iterator itrMap;
    if(!inm_filecontents.empty())
    {
       contentList=tokenizeString(inm_filecontents,"\n");
       itrMap=contentList.begin();

       while(itrMap != contentList.end())
       {
           contentMap = tokenizeString((*itrMap),"=");
           key=contentMap.front();
           value=contentMap.back();
           
           keyValueMap.insert(std::make_pair<std::string,std::string>(key,value));
           itrMap++;
       }
    }
    else
        return false;

    std::map<std::string,std::string>::iterator itr=keyValueMap.begin();
    while(itr != keyValueMap.end())
    {
       log(SV_LOG_DEBUG,"Key - %s, Value - %s\n",itr->first.c_str(),itr->second.c_str());
       itr++;
    }

    log(SV_LOG_DEBUG,"EXITING %s\n",__FUNCTION__);
    return true;
}

bool Rollback(std::string Tag)
{
    log(SV_LOG_DEBUG,"ENTERING %s\n",__FUNCTION__);

    std::string strCommnadToExecute ;
    std::string l_CmdOutput;

    std::list<HostData> :: iterator itrHosts;
    std::string SrcHostId;
    std::string volList;

    for(itrHosts = g_ProtectionSettings.HostList.begin(); itrHosts != g_ProtectionSettings.HostList.end(); itrHosts++)
    {
        SrcHostId = itrHosts->HostID;
        std::string DiskMapFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/SrcTgtDiskMap.dat");

        log(SV_LOG_DEBUG,"Rolling back the pairs for the source : %s \n",SrcHostId.c_str());

        std::map<std::string,std::string> SrcTgtDiskMap;
        if(false == ReadFile(DiskMapFile,SrcTgtDiskMap))
        {
           log(SV_LOG_ERROR,"Unable to read the contents of %s\n",DiskMapFile.c_str());
           return false;
        }

        std::map<std::string,std::string>::iterator itrDisk;
        std::string tgtDisk;

        printf("Rolling Back the Pairs to the Tag : %s for the source %s \n",Tag.c_str(),SrcHostId.c_str());

        for(itrDisk=SrcTgtDiskMap.begin();itrDisk!=SrcTgtDiskMap.end();itrDisk++)
        {
           tgtDisk=itrDisk->second;

           if(volList.empty())
               volList = tgtDisk;
           else
               volList += string(";") + tgtDisk;

        }
        log(SV_LOG_DEBUG,"VolList : %s \n",volList.c_str());

        if(Tag == "LCT" )
        {
            printf("Rolling Back the Pairs to LATESTTIME\n");
            strCommnadToExecute = installLocation + string("/bin/cdpcli ");
            strCommnadToExecute += string("--rollback --rollbackpairs=\"");
            strCommnadToExecute += volList;
            strCommnadToExecute += string("\" --recentcrashconsistentpoint --deleteretentionlog=yes");
        }
        else
        {
            strCommnadToExecute = installLocation + string("/bin/cdpcli ");
            strCommnadToExecute += string("--rollback --rollbackpairs=\"");
            strCommnadToExecute += volList;
            strCommnadToExecute += string("\" --event=");
            strCommnadToExecute += Tag;
            strCommnadToExecute += string(" --picklatest --deleteretentionlog=yes");
        }

        SrcTgtDiskMap.clear();

        if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,NUM_RETRIES))
        {
            log(SV_LOG_ERROR,"Unable run command %s on MT \n",strCommnadToExecute.c_str());
            printf("Failed to Roll Back the pairs to the Tag [%s] for the source %s \n",Tag.c_str(),SrcHostId.c_str());
            return false;
        }
    }

    log(SV_LOG_DEBUG,"EXTING %s\n",__FUNCTION__);
    return true;
}

bool DeleteDiskFromFile(std::string filename,std::string srcDisk,std::string tgtDisk)
{
    log(SV_LOG_DEBUG,"ENTERING %s\n",__FUNCTION__);

    std::string strToDelete = srcDisk + string("=") + tgtDisk;
    log(SV_LOG_DEBUG,"String To Delete : %s\n",strToDelete.c_str());

    std::string line;
    int found=0;
    std::ifstream in(filename.c_str());
    found=filename.find_last_of("//");

    std::string dirLoc = filename.substr(0,found+1);
    std::string outfile = dirLoc + string("outfile.txt"); 

    if( !in.is_open())
    {
        log(SV_LOG_ERROR,"Failed to open file %s\n",filename.c_str());
        return false;
    }
    std::ofstream out(outfile.c_str());
    while( getline(in,line) )
    {
      if(line != strToDelete)
          out << line << "\n";
    }

    in.close();
    out.close();

    remove(outfile.c_str());
    rename(outfile.c_str(),filename.c_str());

    log(SV_LOG_DEBUG,"EXTING %s\n",__FUNCTION__);
    return true;
}

bool Recovery(std::string Tag)
{
    log(SV_LOG_DEBUG,"ENTERING %s\n",__FUNCTION__);

    std::string strCommnadToExecute ;
    std::string l_CmdOutput;
    int PartitionIdofMT;

    if(false == Rollback(Tag))
    {
        log(SV_LOG_ERROR,"Failed to Perform rollback  \n");
        return false;
    }

    std::string DiskMapFile,DiskVtdMapFile,CredFile,DiskLvFile;

    std::list<HostData> :: iterator itrhostList;
    std::string SrcHostId,LparName;

    for(itrhostList = g_ProtectionSettings.HostList.begin(); itrhostList != g_ProtectionSettings.HostList.end(); itrhostList++ )
    {
       SrcHostId = itrhostList->HostID;
       LparName = itrhostList->TemplateLPAR;

       log(SV_LOG_DEBUG," SrcHostId  - %s\n",SrcHostId.c_str());
       log(SV_LOG_DEBUG," LparName - %s\n",LparName.c_str());

       DiskMapFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/SrcTgtDiskMap.dat");
       DiskVtdMapFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/SrcTgtVtdMap.dat");
       CredFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/Servers.dat");
       DiskLvFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/SrcTgtLvMap.dat");

       std::map<std::string,std::string> SrcTgtDiskMap,SrcTgtVtdMap,DiskLvMap;
       if(false == GetMapFromFile(DiskMapFile,SrcTgtDiskMap))
       {
          log(SV_LOG_ERROR,"Unable to get Src Tgt Disk Map\n");
          return false;
       }

       if(false == GetMapFromFile(DiskVtdMapFile,SrcTgtVtdMap))
       {
          log(SV_LOG_ERROR,"Unable to get Src Tgt vtd Disk Map\n");
          return false;
       }

       if(false == GetMapFromFile(DiskLvFile,DiskLvMap))
       {
          log(SV_LOG_ERROR,"Unable to get Src Tgt Lv Map\n");
          return false;
       }

       std::string VIOServerIp;
       if(false == ReadFromConfigMap(CredFile,"VIOServerIp",VIOServerIp))
       {
          log(SV_LOG_ERROR,"Unable to Read VIOServerIp from %s\n",CredFile.c_str());
          return false;
       }
       log(SV_LOG_DEBUG,"VIOServer IP Address - %s\n",VIOServerIp.c_str());

       std::string ManagedSystem;
       if(false == ReadFromConfigMap(CredFile,"ManagesSystem",ManagedSystem))
       {
          log(SV_LOG_ERROR,"Unable to Read ManagesSystem from %s\n",CredFile.c_str());
          return false;
       }
       log(SV_LOG_DEBUG,"ManagesSystem - %s\n",ManagedSystem.c_str());

       std::string HMCUser = g_ProtectionSettings.HMCUser;
       log(SV_LOG_DEBUG,"HMCUser - %s\n",HMCUser.c_str());

       std::string HMCIp = g_ProtectionSettings.HMCIpAddress;
       log(SV_LOG_DEBUG," HMCIpAddress - %s\n",HMCIp.c_str());

       std::string VIOServerName;
       if(false == ReadFromConfigMap(CredFile,"VIOServerName",VIOServerName))
       {
          log(SV_LOG_ERROR,"Unable to Read VIOServerName from %s\n",CredFile.c_str());
          return false;
       }

       log(SV_LOG_DEBUG," VIOServerName - %s\n",VIOServerName.c_str());
     
       std::map<std::string,std::string>::iterator itrSrcTgtMap;
       int found;
       std::string tgtDisk,tgtPv,srcDisk;
 
       printf("Detaching the disks from the MT\n");

       for(itrSrcTgtMap=SrcTgtDiskMap.begin();itrSrcTgtMap!=SrcTgtDiskMap.end();itrSrcTgtMap++)
       {
          srcDisk=itrSrcTgtMap->first;
          tgtDisk=itrSrcTgtMap->second;
          found=tgtDisk.find_last_of("//");
          tgtPv=tgtDisk.substr(found+1);

          strCommnadToExecute = string("rmdev -dl ");
          strCommnadToExecute += tgtPv;

          if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,NUM_RETRIES))
          {
             log(SV_LOG_ERROR,"Unable run command %s on MT \n",strCommnadToExecute.c_str());
             return false;
          }

          //DeleteDiskFromFile(DiskMapFile,srcDisk,tgtDisk);
       }

       strCommnadToExecute = string("ssh ");
       strCommnadToExecute += HMCUser;
       strCommnadToExecute += string("@");
       strCommnadToExecute += HMCIp;
       strCommnadToExecute += string(" ");
 
       strCommnadToExecute += string("lssyscfg -r prof -m ");
       strCommnadToExecute += ManagedSystem;
       strCommnadToExecute += string(" --filter");
       strCommnadToExecute += string(" lpar_names=");
       strCommnadToExecute += LparName;
       strCommnadToExecute += string(" -F lpar_id");

       if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
       {
          log(SV_LOG_ERROR,"Unable run command %s on HMC \n",strCommnadToExecute.c_str());
          return false;
       }

       PartitionIdofMT=atoi(l_CmdOutput.c_str());
       log(SV_LOG_DEBUG,"Partition of MT %d\n",PartitionIdofMT);

       if(SrcTgtVtdMap.empty())
       {
          log(SV_LOG_ERROR,"Unable to get the SrcTgtVtdMap, failed to detach the vtds from MT LPAR \n");
          return false;
       }

       srcDisk.clear();
       std::map<std::string,std::string>::iterator itrSrcTgtVtd;
       std::string tgtVtd;
       for(itrSrcTgtVtd=SrcTgtVtdMap.begin();itrSrcTgtVtd!=SrcTgtVtdMap.end();itrSrcTgtVtd++)
       { 
         srcDisk=itrSrcTgtVtd->first;
         tgtVtd=itrSrcTgtVtd->second;
         strCommnadToExecute = string("ssh ");
         strCommnadToExecute += HMCUser;
         strCommnadToExecute += string("@");
         strCommnadToExecute += HMCIp;
         strCommnadToExecute += string(" ");
         strCommnadToExecute += string("\'");
         strCommnadToExecute += string("viosvrcmd -m ");
         strCommnadToExecute += ManagedSystem;
         strCommnadToExecute += string(" -p ");
         strCommnadToExecute += VIOServerName;
         strCommnadToExecute += string(" -c ");
         strCommnadToExecute += string("\"");
         strCommnadToExecute += string("rmvdev -vtd ");
         strCommnadToExecute += tgtVtd;
         strCommnadToExecute += string("\"");
         strCommnadToExecute += string("\'");

         if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,NUM_RETRIES))
         {
            log(SV_LOG_ERROR,"Unable run command %s on VIOS \n",strCommnadToExecute.c_str());
            printf("Failed to detach the disks from the MT\n");
            return false;
         }

         //DeleteDiskFromFile(DiskVtdMapFile,srcDisk,tgtVtd);
       }  

       std::map<int,std::list<VirScsiAdapter> > virScsiAdapter;
       if ( false == GetVirtualScsiAdapter(HMCUser,HMCIp,ManagedSystem,VIOServerName,VIOServerIp,PartitionIdofMT,virScsiAdapter,LparName))
       {
          log(SV_LOG_ERROR,"Failed to get the Virual Scsi Adapter \n");
          return false;
       }

       std::list<VirScsiAdapter> listVirScsiAdap;
       std::map<int,std::list<VirScsiAdapter> >::iterator itrVirScsiAdap;
       itrVirScsiAdap=virScsiAdapter.find(PartitionIdofMT);

       if(itrVirScsiAdap != virScsiAdapter.end())
       {
          listVirScsiAdap=itrVirScsiAdap->second;
       }

       std::map<std::string,std::string> l_SrcTgtDiskMap;
       printf("Attaching the disks to template LPAR \n");
       if(false == AttachTgtDevicesToMt(HMCUser,HMCIp,ManagedSystem,VIOServerName,DiskLvMap,VIOServerIp,listVirScsiAdap,SrcHostId,l_SrcTgtDiskMap))
       {
          log(SV_LOG_ERROR,"Unable to Attach Devices to template LPAR \n");
          printf("Failed to attach the disks to template LPAR \n");
          return false;
       }
       printf("Successfuly attached the disks to template LPAR \n");
    }

    log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

bool ResumeDisks(std::string SrcHostId)
{
     log(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
     
     std::string VIOServerIp,VIOServerName,ManagedSystem;
     std::string LparName;
     int PartitionIdofMT;
      
     std::string strCommnadToExecute ;
     std::string l_CmdOutput;
       
     std::string DiskLvFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/SrcTgtLvMap.dat");
     std::map<std::string,std::string> DiskLvMap;
     if(false == GetMapFromFile(DiskLvFile,DiskLvMap))
     {
         log(SV_LOG_ERROR,"Unable to get Src Tgt Lv Map\n");
         return false;
     }

     std::string HMCIp = g_ProtectionSettings.HMCIpAddress;
     std::string HMCUser = g_ProtectionSettings.HMCUser;
     
     strCommnadToExecute = string("grep `hostname` /etc/hosts ");
     strCommnadToExecute += string("| grep -v \"^#\"");
     strCommnadToExecute += string("| awk '{print $1}'");
   
     if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
     {
         log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
         return false;
     }
     std::list<std::string> hostNameList = tokenizeString(l_CmdOutput, "\n");
     std::string MTIpAddress = hostNameList.front();
         
     log(SV_LOG_DEBUG,"MTIpAddress : %s\n",MTIpAddress.c_str());
    
     strCommnadToExecute.clear();
     l_CmdOutput.clear();
  
     if(false == GetVioServer(HMCIp,HMCUser,MTIpAddress,VIOServerName,VIOServerIp,ManagedSystem,LparName,PartitionIdofMT))
     {
         log(SV_LOG_ERROR,"Unable to get VIOServer\n");
         return false;
     }
    
     std::map<int,std::list<VirScsiAdapter> > virScsiAdapter;

     if ( false == GetVirtualScsiAdapter(HMCUser,HMCIp,ManagedSystem,VIOServerName,VIOServerIp,PartitionIdofMT,virScsiAdapter,LparName))
     {
         log(SV_LOG_ERROR,"Failed to get the Virual Scsi Adapter \n");
         return false;
     }
     
     std::list<VirScsiAdapter> listVirScsiAdap;
     std::map<int,std::list<VirScsiAdapter> >::iterator itrVirScsiAdap;
     itrVirScsiAdap=virScsiAdapter.find(PartitionIdofMT);
   
     if(itrVirScsiAdap != virScsiAdapter.end())
     {
        listVirScsiAdap=itrVirScsiAdap->second;
     }
 
     std::map<std::string,std::string> l_SrcTgtDiskMap;
     
     printf("Attaching the disks to MT LPAR \n");
     if(false == AttachTgtDevicesToMt(HMCUser,HMCIp,ManagedSystem,VIOServerName,DiskLvMap,VIOServerIp,listVirScsiAdap,SrcHostId,l_SrcTgtDiskMap))
     {
         log(SV_LOG_ERROR,"Unable to Attach Devices to template LPAR \n");
         printf("Failed to attach the disks to template LPAR \n");
         return false;
     }
     printf("Successfuly attached the disks to MT LPAR \n");
        
     std::map<std::string,std::string>::iterator itrSrcTgtDisk;
   
     std::string tgtPv,targetPv;
     int inm_found;
     for(itrSrcTgtDisk = l_SrcTgtDiskMap.begin();itrSrcTgtDisk != l_SrcTgtDiskMap.end();itrSrcTgtDisk++)
     {
        targetPv = itrSrcTgtDisk->second;
        inm_found = targetPv.find_last_of("//");
        tgtPv = targetPv.substr(inm_found+1);

        log(SV_LOG_DEBUG,"Assigning PVID to disk %s\n",tgtPv.c_str());
   
        strCommnadToExecute = string("chdev -l ");
        strCommnadToExecute += tgtPv;
        strCommnadToExecute += string(" -a pv=yes");

        if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
        {
           log(SV_LOG_ERROR,"Failed to execute the Command %s\n",strCommnadToExecute.c_str());
           return false;
        }
      }
         
      l_SrcTgtDiskMap.clear();
      printf("Registering the host with CX \n\n");
    
      if(false == RegisterHostUsingCdpCli())
      {
         log(SV_LOG_ERROR,"Failed to Register the Host \n");
         printf("Failed to register the host with CX \n\n");
     
         log(SV_LOG_INFO,"Cleaning up the disks whatever created \n");
         printf("Cleaning up the disks whatever created .......\n\n");
      
         if(false == CleanupDisks())
         {
            log(SV_LOG_INFO,"Cleaning of Disks is not Done Properly, Manual Cleanup is Required\n");
            printf("Cleaning of Disks is not Done Properly, Manual Cleanup is Required \n\n");
   
         }
         return false;
       }
    
       log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}
         
bool Resume(std::string TgtHostId,std::string CXIpAddress,std::string CXPort,int isHttps)
{
    log(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    std::list<HostData> :: iterator itrhostList;
    std::string SrcHostId,LparName,ManagedSystem,VIOServerName;

    std::string HMCUser = g_ProtectionSettings.HMCUser;
    std::string HMCIp = g_ProtectionSettings.HMCIpAddress;

    std::string strCommnadToExecute ;
    std::string l_CmdOutput;

    for(itrhostList = g_ProtectionSettings.HostList.begin(); itrhostList != g_ProtectionSettings.HostList.end(); itrhostList++ )
    {
        SrcHostId = itrhostList->HostID;
        LparName = itrhostList->TemplateLPAR;
        std::string Status;

        std::string serversFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/Servers.dat");
        if(false == ReadFromConfigMap(serversFile,"ManagesSystem",ManagedSystem))
        {
            log(SV_LOG_ERROR,"Unable to get the ManagesSystem from %s\n",serversFile.c_str());
            return false;
        }
        if(false == ReadFromConfigMap(serversFile,"VIOServerName",VIOServerName))
        {
            log(SV_LOG_ERROR,"Unable to Read VIOServerName from %s\n",serversFile.c_str());
            return false;
        }


        std::string DiskVtdMapFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/SrcTgtVtdMap.dat");
        std::string DiskLvFile = installLocation + string("/failover_data/AIXP2V/") + SrcHostId + string("/SrcTgtLvMap.dat");
        std::map<std::string,std::string> SrcTgtVtdMap,DiskLvMap;

        printf("Determining the LPAR [ %s ] status \n",LparName.c_str());

        strCommnadToExecute = string("ssh ");
        strCommnadToExecute += HMCUser;
        strCommnadToExecute += string("@");
        strCommnadToExecute += HMCIp;
        strCommnadToExecute += string(" ");

        strCommnadToExecute += string("lssyscfg -r lpar -m ");
        strCommnadToExecute += ManagedSystem;
        strCommnadToExecute += string(" --filter lpar_names=");
        strCommnadToExecute += LparName;
        strCommnadToExecute += string(" -F state");

        if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
        {
           log(SV_LOG_ERROR,"Unable run command %s on HMC \n",strCommnadToExecute.c_str());
           printf("Failed to get the state of the LPAR : %s\n",LparName.c_str());
           return false;
        }

        std::list<std::string> StatusList = tokenizeString(l_CmdOutput, "\n");

        Status=StatusList.front();
        log(SV_LOG_DEBUG,"LPAR Status %s\n",Status.c_str());
        printf("LPAR is in [%s] state \n",Status.c_str());

        if(Status == "Running" )
        {
            printf("Shutting Down the LPAR : %s\n",LparName.c_str());

            strCommnadToExecute = string("ssh ");
            strCommnadToExecute += HMCUser;
            strCommnadToExecute += string("@");
            strCommnadToExecute += HMCIp;
            strCommnadToExecute += string(" ");

            strCommnadToExecute += string("chsysstate -r lpar -m ");
            strCommnadToExecute += ManagedSystem;
            strCommnadToExecute += string(" -o shutdown --immed -n ");
            strCommnadToExecute += LparName;

            if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
            {
               log(SV_LOG_ERROR,"Unable run command %s on HMC \n",strCommnadToExecute.c_str());
               printf("Failed to shutdown the LPAR : %s\n",LparName.c_str());
               return false;
            }
            printf("Successfully ShutDown the LPAR : %s\n",LparName.c_str());
        }

        if(false == GetMapFromFile(DiskVtdMapFile,SrcTgtVtdMap))
        {
           log(SV_LOG_ERROR,"Unable to get Src Tgt vtd Disk Map\n");
           return false;
        }

        if(false == GetMapFromFile(DiskLvFile,DiskLvMap))
        {
           log(SV_LOG_ERROR,"Unable to get Src Tgt Lv Map\n");
           return false;
        }

        std::map<std::string,std::string>::iterator itrSrcTgtVtd;
        std::string tgtVtd;
        for(itrSrcTgtVtd=SrcTgtVtdMap.begin();itrSrcTgtVtd!=SrcTgtVtdMap.end();itrSrcTgtVtd++)
        {
           tgtVtd=itrSrcTgtVtd->second;
           strCommnadToExecute = string("ssh ");
           strCommnadToExecute += HMCUser;
           strCommnadToExecute += string("@");
           strCommnadToExecute += HMCIp;
           strCommnadToExecute += string(" ");
           strCommnadToExecute += string("\'");
           strCommnadToExecute += string("viosvrcmd -m ");
           strCommnadToExecute += ManagedSystem;
           strCommnadToExecute += string(" -p ");
           strCommnadToExecute += VIOServerName;
           strCommnadToExecute += string(" -c ");
           strCommnadToExecute += string("\"");
           strCommnadToExecute += string("rmvdev -vtd ");
           strCommnadToExecute += tgtVtd;
           strCommnadToExecute += string("\"");
           strCommnadToExecute += string("\'");

           if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,NUM_RETRIES))
           {
              log(SV_LOG_ERROR,"Unable run command %s on VIOS \n",strCommnadToExecute.c_str());
              printf("Failed to detach the disk [%s] from the LAR :%s\n",tgtVtd.c_str(),LparName.c_str());
              return false;
           }

        }

      if(false == ResumeDisks(SrcHostId))
      {
          log(SV_LOG_DEBUG,"Failed to resume the protection \n");
          printf("Failed to resume the protection \n");
          return false;
      }

      printf("Resume Completed Successfully for the Host :%s \n",SrcHostId.c_str());

    }

    if(false == Protection(TgtHostId,CXIpAddress,CXPort,isHttps))
    {
       log(SV_LOG_ERROR,"Failed to Protect Full System\n");
       printf("Failed to Protect Full System\n");
       exit(1);
    }

    log(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

int main( int argc, char* argv[] )
{
    //calling init funtion of inmsafec library
	init_inm_safe_c_apis();
	std::string inm_logfile = string("/var/log/AixProtection.log");
    setLogFile(inm_logfile.c_str());

    std::string strCommnadToExecute;
    std::string l_CmdOutput;

    strCommnadToExecute = string("grep INSTALLATION_DIR ");
    strCommnadToExecute += string("/usr/local/.vx_version | cut -d'=' -f2");

    if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
    {
        log(SV_LOG_ERROR,"Failed to execute the command %s\n",strCommnadToExecute.c_str());
        exit(1);
    }

    std::list<std::string> dirList = tokenizeString(l_CmdOutput, "\n");

    installLocation = dirList.front();
    log(SV_LOG_DEBUG,"Installation Directory : %s\n", installLocation.c_str());
    
    strCommnadToExecute.clear();
    l_CmdOutput.clear();

    strCommnadToExecute = string("mkdir -p ");
    strCommnadToExecute += installLocation;
    strCommnadToExecute += string("/failover_data/");
    strCommnadToExecute += string("AIXP2V");

    if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
    {
        log(SV_LOG_ERROR,"Failed to execute the command %s\n",strCommnadToExecute.c_str());
        exit(1);
    }

    strCommnadToExecute.clear();
    l_CmdOutput.clear();

    strCommnadToExecute = string("grep -w LogLevel ");
    strCommnadToExecute += installLocation;
    strCommnadToExecute += string("/etc/drscout.conf");
    strCommnadToExecute += string("| awk -F= '{print $2}'");

    if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
    {
        log(SV_LOG_ERROR,"Failed to execute the command %s\n",strCommnadToExecute.c_str());
        exit(1);
    }
    std::list<std::string> LogList = tokenizeString(l_CmdOutput, "\n");
    std::string strLog = LogList.front();

    g_inmLogLevel = atoi(strLog.c_str());

    std::string InputXmlFile; 

    if (argc < 3)
    {
       BinUsage(argv[0]);
    }
    else
    {
      try
      {
        if((0 == strcmp( argv[1], "--h")) || (0 == strcmp(argv[1], "--help")))
        {
          BinUsage( argv[0] );
        }
        else if(0 == strcmp(argv[1], "--protection" ))
        {
            if (argc < 3)
            {
               BinUsage(argv[0]);
            }

            InputXmlFile = string(argv[2]);

            log(SV_LOG_DEBUG,"Received Input File : %s\n",InputXmlFile.c_str());

            ReadConfig(InputXmlFile);

            std::string CXIpAddress,CXPort,TgtHostId;
            int l_isHttps;

            strCommnadToExecute = string("grep -w Hostname ");
            strCommnadToExecute += installLocation;
            strCommnadToExecute += string("/etc/drscout.conf");
            strCommnadToExecute += string("| awk -F= '{print $2}'");

            if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
            {
               log(SV_LOG_ERROR,"Failed to execute the command %s\n",strCommnadToExecute.c_str());
               exit(1);
            }
            std::list<std::string> cxIpList = tokenizeString(l_CmdOutput, "\n");

            CXIpAddress = cxIpList.front();

            log(SV_LOG_DEBUG,"CXIpAddress : %s\n",CXIpAddress.c_str());

            strCommnadToExecute.clear();
            l_CmdOutput.clear();

            strCommnadToExecute = string("grep -w Port ");
            strCommnadToExecute += installLocation;
            strCommnadToExecute += string("/etc/drscout.conf");
            strCommnadToExecute += string("| awk -F= '{print $2}'");

            if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
            {
               log(SV_LOG_ERROR,"Failed to execute the command %s\n",strCommnadToExecute.c_str());
               exit(1);
            }

            std::list<std::string> cxPortList = tokenizeString(l_CmdOutput, "\n");

            CXPort = cxPortList.front();
            log(SV_LOG_DEBUG,"CXPort : %s\n",CXPort.c_str());

            strCommnadToExecute.clear();
            l_CmdOutput.clear();

            strCommnadToExecute = string("grep -w Https ");
            strCommnadToExecute += installLocation;
            strCommnadToExecute += string("/etc/drscout.conf");
            strCommnadToExecute += string("| awk -F= '{print $2}'");

            if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
            {
               log(SV_LOG_ERROR,"Failed to execute the command %s\n",strCommnadToExecute.c_str());
               exit(1);
            }

            std::list<std::string> httpsList = tokenizeString(l_CmdOutput, "\n");

            std::string HttpsVal = httpsList.front();
            l_isHttps = atoi(HttpsVal.c_str());

            log(SV_LOG_DEBUG,"IsHttps : %d\n",l_isHttps);

            strCommnadToExecute.clear();
            l_CmdOutput.clear();

            strCommnadToExecute = string("grep -w HostId ");
            strCommnadToExecute += installLocation;
            strCommnadToExecute += string("/etc/drscout.conf");
            strCommnadToExecute += string(" | awk -F= '{print $2}'");

            if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
            {
               log(SV_LOG_ERROR,"Failed to execute the command %s\n",strCommnadToExecute.c_str());
               exit(1);
            }

            std::list<std::string> TgtHostList = tokenizeString(l_CmdOutput, "\n");

            TgtHostId = TgtHostList.front();
            log(SV_LOG_DEBUG,"MT HostId : %s\n",TgtHostId.c_str());

            std::list<HostData> :: iterator itrHosts;
            std::string SrcHostId;

            for(itrHosts = g_ProtectionSettings.HostList.begin(); itrHosts != g_ProtectionSettings.HostList.end(); itrHosts++)
            {
               SrcHostId = itrHosts->HostID;
               strCommnadToExecute = string("mkdir -p ");
               strCommnadToExecute += installLocation;
               strCommnadToExecute += string("/failover_data/AIXP2V/");
               strCommnadToExecute += SrcHostId;

               if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
               {
                  log(SV_LOG_ERROR,"Failed to execute the command %s\n",strCommnadToExecute.c_str());
                  exit(1);
               }

               strCommnadToExecute.clear();
               l_CmdOutput.clear();

               if ( !SrcHostId.empty() && !CXIpAddress.empty() && !CXPort.empty() ) 
               {
                  log(SV_LOG_INFO,"SrcHostId : %s\n", SrcHostId.c_str());
                  log(SV_LOG_INFO,"CXIpAddress : %s\n", CXIpAddress.c_str());
                  log(SV_LOG_INFO,"CXPort : %s\n", CXPort.c_str());

                  printf("Discovering the data of Source : %s\n\n",SrcHostId.c_str());
                  printf("......................\n\n");

                  if(false == Discovery(SrcHostId, CXIpAddress, CXPort, l_isHttps))
                  {
                      log(SV_LOG_ERROR,"Failed Discover disks of Source  %s\n",SrcHostId.c_str());
                      printf("Failed Discover disks of Source  %s\n",SrcHostId.c_str());
                      exit(1);
                  }
                  else
                      printf("Discovery Completed for the host : %s\n\n",SrcHostId.c_str());
              
                  PrintDiscData(SrcHostId);
               }
            }

            if(false == Protection(TgtHostId,CXIpAddress,CXPort,l_isHttps))
            {
               log(SV_LOG_ERROR,"Failed to Protect Full System\n");
               printf("Failed to Protect Full System\n");
               exit(1); 
            }
        }
        else if(0 == strcmp(argv[1], "--recovery" ))
        {
            if (argc < 5)
            {
               BinUsage(argv[0]);
            }

            isRecovery=1;
            std::string Tag,LparName;

            InputXmlFile=string(argv[4]);

            log(SV_LOG_DEBUG,"Received Input File : %s\n",InputXmlFile.c_str());

            ReadConfig(InputXmlFile);

            if(0 == strcmp( argv[2], "--Tag"))
            {
                Tag = argv[3];
                log(SV_LOG_DEBUG,"Recovery Tag - %s\n",Tag.c_str());
            }

            if(Tag.empty())
            {
                log(SV_LOG_ERROR,"Unable to read the Tag - %s\n",Tag.c_str());
                exit(1);
            }

            if(false == Recovery(Tag))
            {
               log(SV_LOG_ERROR,"Failed to Recover to the tag - %s\n",Tag.c_str());
               printf("Failed to Recover to the tag - %s\n",Tag.c_str());
               exit(1);
            }
        }
        else if (0 == strcmp(argv[1], "--resume" ))
        {
            if (argc < 3)
            {
               BinUsage(argv[0]);
            }

            g_isResume=1;
            InputXmlFile = string(argv[2]);

            log(SV_LOG_DEBUG,"Received Input File : %s\n",InputXmlFile.c_str());

            ReadConfig(InputXmlFile);

            std::string CXIpAddress,CXPort,TgtHostId;
            int l_isHttps;

            strCommnadToExecute = string("grep -w Hostname ");
            strCommnadToExecute += installLocation;
            strCommnadToExecute += string("/etc/drscout.conf");
            strCommnadToExecute += string("| awk -F= '{print $2}'");

            if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
            {
               log(SV_LOG_ERROR,"Failed to execute the command %s\n",strCommnadToExecute.c_str());
               exit(1);
            }
            std::list<std::string> cxIpList = tokenizeString(l_CmdOutput, "\n");

            CXIpAddress = cxIpList.front();

            log(SV_LOG_DEBUG,"CXIpAddress : %s\n",CXIpAddress.c_str());

            strCommnadToExecute.clear();
            l_CmdOutput.clear();

            strCommnadToExecute = string("grep -w Port ");
            strCommnadToExecute += installLocation;
            strCommnadToExecute += string("/etc/drscout.conf");
            strCommnadToExecute += string("| awk -F= '{print $2}'");

            if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
            {
                log(SV_LOG_ERROR,"Failed to execute the command %s\n",strCommnadToExecute.c_str());
                exit(1);
            }

            std::list<std::string> cxPortList = tokenizeString(l_CmdOutput, "\n");

            CXPort = cxPortList.front();
            log(SV_LOG_DEBUG,"CXPort : %s\n",CXPort.c_str());

            strCommnadToExecute.clear();
            l_CmdOutput.clear();

            strCommnadToExecute = string("grep -w HostId ");
            strCommnadToExecute += installLocation;
            strCommnadToExecute += string("/etc/drscout.conf");
            strCommnadToExecute += string(" | awk -F= '{print $2}'");

            if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
            {
                log(SV_LOG_ERROR,"Failed to execute the command %s\n",strCommnadToExecute.c_str());
                exit(1);
            }

            std::list<std::string> TgtHostList = tokenizeString(l_CmdOutput, "\n");

            TgtHostId = TgtHostList.front();
                                                                                                                                                                                    log(SV_LOG_DEBUG,"MT HostId : %s\n",TgtHostId.c_str());

            strCommnadToExecute.clear();
            l_CmdOutput.clear();

            strCommnadToExecute = string("grep -w Https ");
            strCommnadToExecute += installLocation;
            strCommnadToExecute += string("/etc/drscout.conf");
            strCommnadToExecute += string("| awk -F= '{print $2}'");

            if(0 != ExecuteCommand(strCommnadToExecute,l_CmdOutput,1))
            {
               log(SV_LOG_ERROR,"Failed to execute the command %s\n",strCommnadToExecute.c_str());
               exit(1);
            }

            std::list<std::string> httpsList = tokenizeString(l_CmdOutput, "\n");

            std::string HttpsVal = httpsList.front();
            l_isHttps = atoi(HttpsVal.c_str());

            log(SV_LOG_DEBUG,"IsHttps : %d\n",l_isHttps);

            if(false == Resume(TgtHostId,CXIpAddress,CXPort,l_isHttps))
            {
               log(SV_LOG_ERROR,"Failed to Perform Resume \n");
               printf("Failed to Resume \n");
               exit(1);
            }
            printf("Resume Completed Successfully\n");
        }
        else
        {
          log(SV_LOG_ERROR,"Invalid Option Selected \n");
          BinUsage(argv[0]);
        }
      }
      catch( exception& e)
      {
         log(SV_LOG_ERROR,"Caught Exception %s\n",e.what());
      }
   }
}

