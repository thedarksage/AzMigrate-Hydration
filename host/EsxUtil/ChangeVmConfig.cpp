#include "ChangeVmConfig.h"
#include "inm_md5.h"
#include "svutils.h"
#include "setpermissions.h"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#ifdef WIN32

#include "scopeguard.h"
#include "disk.h"

#include <boost/assert.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#endif

/* Fetch NW details from XML provided by UI.

	Sample Hierarchy of XML file
	<root CX_IP="10.0.15.153">
		<SRC_ESX>
			<host hostname="WIN2k8-64" ....>					
                <nic network_label="Network adapter 2" network_name="VM Network" nic_mac="00:0c:29:10:b2:00" ip="10.0.0.60" dhcp="0" mask="" gateway="" dnsip="" new_ip="10.0.0.60" new_mask="255.255.0.0" new_dnsip="10.0.64.64" new_gateway="10.0.0.1" new_network_name="vmnic2network" />
                <nic network_label="Network adapter 1" network_name="VM Network" nic_mac="00:0c:29:10:b2:f6" ip="10.0.61.2" dhcp="1" mask="" gateway="" dnsip="" new_ip="10.0.45.23" new_mask="255.255.0.0" new_dnsip="10.0.64.64" new_gateway="10.0.0.1" new_network_name="vmnic2network" />
                .
				.
			</host>
            <host ...>
                .
                .
            </host>
		</SRC_ESX>
		.
		.
	</root>

	Output - NetworkInfoMap_t
	Return Value - true on success and false on failure
*/
bool ChangeVmConfig::xGetNWInfoFromXML(NetworkInfoMap_t & NwMap, std::string HostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	std::string XmlFileName;
	unsigned int count = 1;

	if(m_bDrDrill)
		XmlFileName = m_strDatafolderPath + std::string(SNAPSHOT_FILE_NAME);
	else
		XmlFileName = m_strDatafolderPath + std::string(RECOVERY_FILE_NAME);

    DebugPrintf(SV_LOG_INFO,"XML File - %s\n", XmlFileName.c_str());

    xmlDocPtr xDoc;	
	xmlNodePtr xCur;	

	xDoc = xmlParseFile(XmlFileName.c_str());
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

	if (xmlStrcasecmp(xCur->name,(const xmlChar*)"root")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <root> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}

	//If you are here root entry is found. Go for SRC_ESX entry.
	xCur = xGetChildNode(xCur,(const xmlChar*)"SRC_ESX");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <SRC_ESX> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;			
	}

	//If you are here SRC_ESX entry is found. Go for host entry with required HostName.
	xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"inmage_hostid",(const xmlChar*)HostName.c_str());
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host with required inmage_hostid> entry not found\n");
		xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"hostname",(const xmlChar*)HostName.c_str());
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host with required hostname> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			xmlFreeDoc(xDoc); return false;			
		}		
	}
	
    //Parse through child nodes and if they are "nic" , fetch n/w details.     
    for(xmlNodePtr xChild = xCur->children; xChild != NULL; xChild = xChild->next)
	{
        if (!xmlStrcasecmp(xChild->name, (const xmlChar*)"nic"))
		{
			std::string strCount;
			std::stringstream ss;
			ss<<count;
			ss>>strCount;
			count++;

            NetworkInfo nw;
            std::string InterfaceOrLabelName;

            xmlChar *attr_value_temp;
			
			attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"nic_mac");
            if (attr_value_temp == NULL)
            {
                DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <nic nic_mac> entry not found\n");
	            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		        xmlFreeDoc(xDoc); return false;
            }
            InterfaceOrLabelName = std::string((char *)attr_value_temp);
			for(size_t i = 0; i < InterfaceOrLabelName.length(); i++)
				InterfaceOrLabelName[i] = tolower(InterfaceOrLabelName[i]);
			DebugPrintf(SV_LOG_INFO,"Mac Address from xml file : %s \n", InterfaceOrLabelName.c_str());

	        attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"dhcp");
            if (attr_value_temp != NULL) 
            {
                if (0 == xmlStrcasecmp(attr_value_temp,(const xmlChar*)"1"))
                {
                    nw.EnableDHCP = 1;
					attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"new_macaddress");
					if (attr_value_temp != NULL)
						nw.MacAddress = std::string((char *)attr_value_temp);
					DebugPrintf(SV_LOG_INFO,"New Mac Address from xml file : %s \n", nw.MacAddress.c_str());
#ifdef WIN32
					NwMap.insert(make_pair(strCount, nw));
#else
					attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"ip");
					if (attr_value_temp != NULL)
						nw.OldIPAddress = std::string((char *)attr_value_temp);
					DebugPrintf(SV_LOG_INFO,"Old IP Address from xml file : %s \n", nw.OldIPAddress.c_str());

					NwMap.insert(make_pair(InterfaceOrLabelName, nw));
#endif
                    continue;
                }
            }
            nw.EnableDHCP = 0;
		
			attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"ip");
	        if (attr_value_temp != NULL)
				nw.OldIPAddress = std::string((char *)attr_value_temp);

            attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"new_ip");
	        if (attr_value_temp != NULL)
				nw.IPAddress = std::string((char *)attr_value_temp);

            attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"new_mask");
	        if (attr_value_temp != NULL)
				nw.SubnetMask = std::string((char *)attr_value_temp);

            attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"new_gateway");
	        if (attr_value_temp != NULL)
                nw.DefaultGateway = std::string((char *)attr_value_temp);

            attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"new_dnsip");
	        if (attr_value_temp != NULL)
                nw.NameServer = std::string((char *)attr_value_temp);
				
			attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"new_macaddress");
	        if (attr_value_temp != NULL)
				nw.MacAddress = std::string((char *)attr_value_temp);

			std::map<std::string, std::string>::iterator iterClus = m_mapOfVmToClusOpn.find(HostName);
			if(iterClus != m_mapOfVmToClusOpn.end())
			{
				if(iterClus->second.compare("yes") == 0)
				{
					attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"virtual_ips");
					if (attr_value_temp != NULL)
						nw.VirtualIps = std::string((char *)attr_value_temp);
				}
			}
			
			DebugPrintf(SV_LOG_INFO,"Old IP Address from xml file : %s \n", nw.OldIPAddress.c_str());
			DebugPrintf(SV_LOG_INFO,"New Mac Address from xml file : %s \n", nw.MacAddress.c_str());
			DebugPrintf(SV_LOG_INFO,"New IP Address from xml file : %s \n", nw.IPAddress.c_str());
			DebugPrintf(SV_LOG_INFO,"New SubnetMask from xml file : %s \n", nw.SubnetMask.c_str());
			DebugPrintf(SV_LOG_INFO,"New Default Gateway from xml file : %s \n", nw.DefaultGateway.c_str());
			DebugPrintf(SV_LOG_INFO,"New Dns Server IP from xml file : %s \n", nw.NameServer.c_str());
			DebugPrintf(SV_LOG_INFO,"Virtual IPs from xml file : %s \n", nw.VirtualIps.c_str());

#ifdef WIN32
			NwMap.insert(make_pair(strCount, nw));
#else
			NwMap.insert(make_pair(InterfaceOrLabelName, nw));
#endif
        }
    }

    xmlFreeDoc(xDoc);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

//***************************************************************************************
// Finds the required Child node and returns its pointer
//	Input - Parent Pointer , Value of Child node
//	Output - Pointer to required Childe node (returns NULL if not found)
//***************************************************************************************
xmlNodePtr ChangeVmConfig::xGetChildNode(xmlNodePtr cur, const xmlChar* xmlEntry)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	cur = cur->children;
	while(cur != NULL)
	{
		if (!xmlStrcasecmp(cur->name, xmlEntry))
		{
			//Found the required child node
			break;
		}
		cur = cur->next;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return cur;
}

//***************************************************************************************
// Finds the required next Child node and returns its pointer
//	Input - Parent Pointer , Value of Child node
//	Output - Pointer to required Childe node (returns NULL if not found)
//***************************************************************************************
xmlNodePtr ChangeVmConfig::xGetNextChildNode(xmlNodePtr cur, const xmlChar* xmlEntry)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	cur = cur->next;
	while(cur != NULL)
	{
		if (!xmlStrcasecmp(cur->name, xmlEntry))
		{
			//Found the required child node
			break;
		}
		cur = cur->next;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return cur;
}


/*************************************************************************
Get the list of services for the respective host which need to set manual
Input - Hostname
Output - list of service
**************************************************************************/
bool ChangeVmConfig::xGetListOfSrvToSetManual(string& HostName, vector<string>& lstSrvManual)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string FileName;
	if(m_bDrDrill)
		FileName = m_strDatafolderPath + std::string(SNAPSHOT_FILE_NAME);
	else
		FileName = m_strDatafolderPath + std::string(RECOVERY_FILE_NAME);

    DebugPrintf(SV_LOG_INFO,"XML File - %s\n", FileName.c_str());

	xmlDocPtr xDoc;	
	xmlNodePtr xCur;	

	DebugPrintf(SV_LOG_INFO,"XML File - %s\n", FileName.c_str());

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
		DebugPrintf(SV_LOG_ERROR,"The XML is empty. Cannot Proceed further.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <root> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}

	//If you are here root entry is found. Go for SRC_ESX entry.
	xCur = xGetChildNode(xCur,(const xmlChar*)"SRC_ESX");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <SRC_ESX> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
	
	xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"inmage_hostid",(const xmlChar*)HostName.c_str());
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host with required inmage_hostid> entry not found\n");
		xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"hostname",(const xmlChar*)HostName.c_str());
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host with required hostname> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			xmlFreeDoc(xDoc); return false;
		}
	}

	xmlChar *attr_value_temp;
	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"servicestomanual");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host operatingsystem> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
    string strSrvManual = string((char *)attr_value_temp);

	std::string Separator = std::string(":");
    size_t pos = 0;
    size_t found = strSrvManual.find(Separator, pos);
    while(found != std::string::npos)
    {
	    DebugPrintf(SV_LOG_INFO,"Service - [%s]\n",strSrvManual.substr(pos,found-pos).c_str());
        lstSrvManual.push_back(strSrvManual.substr(pos,found-pos));
        pos = found + 1; 
        found = strSrvManual.find(Separator, pos);            
    }
	DebugPrintf(SV_LOG_INFO,"Service - [%s]\n",strSrvManual.substr(pos,strSrvManual.length()).c_str());
	lstSrvManual.push_back(strSrvManual.substr(pos, strSrvManual.length()));

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}


/************************************************************
Get the vcon Version from Esx.xml file
************************************************************/
bool ChangeVmConfig::xGetvConVersion()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string FileName;
	m_bV2P = false;
	m_nMaxCdpJobs = 5;

	if(m_bDrDrill)
		FileName = m_strDatafolderPath + std::string(SNAPSHOT_FILE_NAME);
	else
		FileName = m_strDatafolderPath + std::string(RECOVERY_FILE_NAME);

    DebugPrintf(SV_LOG_INFO,"XML File - %s\n", FileName.c_str());

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
	xmlChar *attr_value_temp;

	if(!m_bDrDrill)
	{
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

	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"ParallelCdpcli");
	if(NULL == attr_value_temp)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <ParallelCdpcli> entry not found\n");
	}
	else
	{
		string strCdpCliJobs = string((char *)attr_value_temp);
		stringstream temp;
		temp << strCdpCliJobs;
		temp >> m_nMaxCdpJobs;           
	}

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
		DebugPrintf(SV_LOG_ERROR,"The %s file is not in expected format : ConfigFileVersion entry not found\n", FileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

//***************************************************************************************
// Fetch OsType, MachineType from XML provided by UI.
/*
	Sample Hierarchy of XML file
	<root CX_IP="10.0.15.153">
		<SRC_ESX>
			<host hostname="WIN2k8-64"  operatingsystem="Microsoft Windows Server 2008 (64-bit)" machinetype="PhysicalMachine"...>					
                .
				.
			</host>
            <host hostname="CWIN2k8-32"  operatingsystem="Microsoft Windows Server 2008 (32-bit)" machinetype="VirtualMachine"...>
                .
                .
            </host>
		</SRC_ESX>
		.
		.
	</root>
*/
//	Output - OsType, MachineType
//	Return Value - true on success and false on failure
//***************************************************************************************
bool ChangeVmConfig::xGetMachineAndOstypeAndV2P(std::string HostName, std::string & OsType, std::string & MachineType, std::string& Failback)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	xmlDocPtr xDoc;	
	xmlNodePtr xCur;	

	std::string XmlFileName;
	if(m_bDrDrill)
		XmlFileName = m_strDatafolderPath + std::string(SNAPSHOT_FILE_NAME);
	else
		XmlFileName = m_strDatafolderPath + std::string(RECOVERY_FILE_NAME);

    DebugPrintf(SV_LOG_INFO,"XML File - %s\n", XmlFileName.c_str());

	xDoc = xmlParseFile(XmlFileName.c_str());
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
		DebugPrintf(SV_LOG_ERROR,"The XML is empty. Cannot Proceed further.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <root> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}

	//If you are here root entry is found. Go for SRC_ESX entry.
	xCur = xGetChildNode(xCur,(const xmlChar*)"SRC_ESX");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <SRC_ESX> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
	
	xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"inmage_hostid",(const xmlChar*)HostName.c_str());
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host with required inmage_hostid> entry not found\n");
		xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"hostname",(const xmlChar*)HostName.c_str());
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host with required hostname> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			xmlFreeDoc(xDoc); return false;
		}
	}

	xmlChar *attr_value_temp;
	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"operatingsystem");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host operatingsystem> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
    OsType = string((char *)attr_value_temp);

    attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"machinetype");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host machinetype> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
    MachineType = string((char *)attr_value_temp);

	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"failback");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host operatingsystem> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
    Failback = string((char *)attr_value_temp);
    
    xmlFreeDoc(xDoc); 

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

bool ChangeVmConfig::ExecuteCmdWithOutputToFileWithPermModes(const std::string Command, const std::string OutputFile, int Modes)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    DebugPrintf(SV_LOG_DEBUG, "Command     = %s\n", Command.c_str());
    DebugPrintf(SV_LOG_DEBUG, "Output File = %s\n", OutputFile.c_str());

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
						DebugPrintf(SV_LOG_DEBUG, "Command succeesfully executed having PID: %d.\n", rc);
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


bool ChangeVmConfig::UpdateArraySnapshotFlag()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    
    bool bArraySnapshot = false;

	string FileName = m_strDatafolderPath + std::string(SNAPSHOT_FILE_NAME);

	do
    {
	    xmlDocPtr xDoc;	
	    xmlNodePtr xCur;	

	    xDoc = xmlParseFile(FileName.c_str());
	    if (xDoc == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
            break;
	    }

	    //Found the doc. Now process it.
	    xCur = xmlDocGetRootElement(xDoc);
	    if (xCur == NULL)
	    {
		    DebugPrintf(SV_LOG_ERROR,"Snapshot.xml is empty. Cannot Proceed further.\n");
            break;
	    }

	    if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	    {
		    //root is not found
		    DebugPrintf(SV_LOG_ERROR,"The Snapshot.xml document is not in expected format : <root> entry not found\n");
            break;
	    }

	    //If you are here root entry is found. Go for AdditionOfDisk which is an attribute in root.
	    xmlChar *xAodFlag;
        xAodFlag = xmlGetProp(xCur,(const xmlChar*)"ArrayBasedDrDrill");
        if(NULL == xAodFlag)
        {
		    DebugPrintf(SV_LOG_ERROR,"The ESX.xml document is not in expected format : <ArrayBasedDrDrill> entry not found\n");
            break;
        }
        else
        {
            if (xmlStrcasecmp(xAodFlag,(const xmlChar*)"false")) 
			{
				bArraySnapshot = true;
				break;
			}				
        }        
    }while(0);

    DebugPrintf(SV_LOG_INFO,"Array based snapahot = %d.\n",bArraySnapshot);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bArraySnapshot;
}


std::map<std::string,std::string> ChangeVmConfig::getProtectedOrSnapshotDrivesMap(std::string strVmName, bool bTakePinfo)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	std::map<std::string,std::string> replicatedDrivesMap;
    std::string strReplicatedVmsInfoFile;
	std::string strLineRead;
	std::string token("!@!@!");

#ifdef WIN32
	if(m_bDrDrill)
	{
		if(bTakePinfo)
			strReplicatedVmsInfoFile = m_VxInstallPath + std::string("\\Failover") + std::string("\\data\\") + strVmName + PAIRINFO_EXTENSION;
		else
			strReplicatedVmsInfoFile = m_VxInstallPath + std::string("\\Failover") + std::string("\\data\\") + strVmName + SNAPSHOTINFO_EXTENSION;
	}
	else
		strReplicatedVmsInfoFile = m_VxInstallPath + std::string("\\Failover") + std::string("\\data\\") + strVmName + PAIRINFO_EXTENSION;
#else
	if(m_bDrDrill)
	{
		if(bTakePinfo)
			strReplicatedVmsInfoFile = m_VxInstallPath +  std::string("failover_data/")+ strVmName + PAIRINFO_EXTENSION;
		else
			strReplicatedVmsInfoFile = m_VxInstallPath +  std::string("failover_data/")+ strVmName + SNAPSHOTINFO_EXTENSION;
	}
	else
		strReplicatedVmsInfoFile = m_VxInstallPath +  std::string("failover_data/")+ strVmName + PAIRINFO_EXTENSION;
#endif
	
	if(false == checkIfFileExists(strReplicatedVmsInfoFile))
	{
        DebugPrintf(SV_LOG_ERROR, "File not found - %s\n", strReplicatedVmsInfoFile.c_str());
		map<string, string>::iterator iter = m_mapOfVmNameToId.find(strVmName);
		if(iter == m_mapOfVmNameToId.end())
		{
			replicatedDrivesMap.clear();
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return replicatedDrivesMap;
		}
		strVmName = iter->second;
#ifdef WIN32
		if(m_bDrDrill)
		{
			if(bTakePinfo)
				strReplicatedVmsInfoFile = m_VxInstallPath + std::string("\\Failover") + std::string("\\data\\") + strVmName + PAIRINFO_EXTENSION;
			else
				strReplicatedVmsInfoFile = m_VxInstallPath + std::string("\\Failover") + std::string("\\data\\") + strVmName + SNAPSHOTINFO_EXTENSION;
		}
		else
			strReplicatedVmsInfoFile = m_VxInstallPath + std::string("\\Failover") + std::string("\\data\\") + strVmName + PAIRINFO_EXTENSION;
#else
		if(m_bDrDrill)
		{
			if(bTakePinfo)
				strReplicatedVmsInfoFile = m_VxInstallPath +  std::string("failover_data/")+ strVmName + PAIRINFO_EXTENSION;
			else
				strReplicatedVmsInfoFile = m_VxInstallPath +  std::string("failover_data/")+ strVmName + SNAPSHOTINFO_EXTENSION;
		}
		else
			strReplicatedVmsInfoFile = m_VxInstallPath +  std::string("failover_data/")+ strVmName + PAIRINFO_EXTENSION;
#endif
	}
	
	std::ifstream inFile(strReplicatedVmsInfoFile.c_str());
	if(!inFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to read the file - %s\n",strReplicatedVmsInfoFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return replicatedDrivesMap;
	}
	while(getline(inFile,strLineRead))
	{
		std::string strSrcVolName;
		std::string strTgtVolName;
		std::size_t index;		
        DebugPrintf(SV_LOG_DEBUG,"Line read =  %s \n",strLineRead.c_str());
		index = strLineRead.find_first_of(token);
		if(index != std::string::npos)
		{
			strSrcVolName = strLineRead.substr(0,index);
			strTgtVolName = strLineRead.substr(index+(token.length()),strLineRead.length());			
            DebugPrintf(SV_LOG_INFO,"SrcVolName =  %s  :  Target volume =  %s\n",strSrcVolName.c_str(),strTgtVolName.c_str());            
			if((!strSrcVolName.empty()) && (!strTgtVolName.empty()))
			{
				replicatedDrivesMap.insert(make_pair(strSrcVolName,strTgtVolName));
			}
		}
		else
		{
            DebugPrintf(SV_LOG_ERROR,"Line read =  %s\n",strLineRead.c_str());
			DebugPrintf(SV_LOG_ERROR,"Failed to find token in line read. Data Format is incorrect in file - %s.\n",strReplicatedVmsInfoFile.c_str());            
			inFile.close();
			replicatedDrivesMap.clear();
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return replicatedDrivesMap;
		}
	}
	inFile.close();
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
	return replicatedDrivesMap;
}


#ifndef WIN32

//Check whether a file exists or not.
bool ChangeVmConfig::checkIfFileExists(std::string strFileName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool blnReturn = true;
    struct stat stFileInfo;
    int intStat;
	
	DebugPrintf(SV_LOG_DEBUG,"Checking File: %s\n", strFileName.c_str());
    
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

bool ChangeVmConfig::MakeInMageSVCManual(std::string HostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::PostRollbackCloudVirtualConfig(std::string& strHostName)
{
	DebugPrintf(SV_LOG_DEBUG , "Entering function %s.\n", __FUNCTION__);
	bool bReturnValue = true;
	LinuxReplication LinObj1;

	do
	{
		StorageInfo_t storageInfo_root;
		m_storageInfo.reset();
		if(false == getHostInfoUsingCXAPI(strHostName , ROOT_MOUNT_POINT , false ))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed while processing CX API call\n");
			DebugPrintf(SV_LOG_ERROR, "Cannot Proceed Further for Linux P2V configuration...\n");
			bReturnValue = false;
			break;
		}

		storageInfo_root = m_storageInfo;
		if ( ! storageInfo_root.strRootVolType.empty() )
		{
			string strTgtPartitionOrLV;
			if("LVM" == storageInfo_root.strRootVolType)
			{
				strTgtPartitionOrLV = storageInfo_root.strLvName;
				if(false == activateVolGrp(storageInfo_root.strVgName))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed To Activate the LV : %s\n", strTgtPartitionOrLV.c_str());
					bReturnValue = false;
					break;		
				}
			}
			else
			{
				string strSrcPartition = storageInfo_root.strPartition;
				if(false == processPartition(strHostName, strSrcPartition, strTgtPartitionOrLV))
				{
					DebugPrintf(SV_LOG_ERROR, "Cannot Proceed Further for Linux P2V configuration...\n");
					bReturnValue = false;
					break;		
				}
			}
			
			string strMntDirectory;
			bool bMount = false;
			
			for( int i=0 ; i <= 3 ; i++ ) 
			{
				string strTimeStamp = LinObj1.GeneratreUniqueTimeStampString();
				DebugPrintf(SV_LOG_DEBUG, "time Stamp: %s\n", strTimeStamp.c_str());
				strMntDirectory = string("/mnt/inmage_mnt_") + strTimeStamp;

				if( true != createDirectory(strMntDirectory))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to create the directory. Trying again...\n");
					sleep(4);
					continue;
				}
					
				if( true != mountPartition(strMntDirectory, strTgtPartitionOrLV, 
                                           strMntDirectory))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to Mount the Parttion: %s Trying again after 4 secs...\n", strTgtPartitionOrLV.c_str());
					sleep(4);
					continue;
				}
				bMount = true;
				break;
			}
			if(!bMount)
				break;

			std::set<std::string> mountedVGs;
			std::vector<std::string> nonRootSysPartitions;
			nonRootSysPartitions.push_back(BOOT_MOUNT_POINT);
			nonRootSysPartitions.push_back(ETC_MOUNT_POINT);
			nonRootSysPartitions.push_back(USR_MOUNT_POINT);
			nonRootSysPartitions.push_back(VAR_MOUNT_POINT);
			nonRootSysPartitions.push_back(OPT_MOUNT_POINT);
			nonRootSysPartitions.push_back(USR_LOCAL_MOUNT_POINT);
			nonRootSysPartitions.push_back(HOME_MOUNT_POINT);

			map<string, StorageInfo_t> mapMountedPartitonInfo;
			std::vector<std::string>::const_iterator iterSysPartition = nonRootSysPartitions.begin();
			for(; iterSysPartition != nonRootSysPartitions.end(); iterSysPartition++)
			{
				string strMountDir = strMntDirectory + *iterSysPartition;
				bool bMounted = false;
				DebugPrintf(SV_LOG_DEBUG, "Mount Directory : %s\n.", strMountDir.c_str());
				bReturnValue = MountSourceLinuxPartition(strMntDirectory, strHostName, *iterSysPartition, strMountDir, mapMountedPartitonInfo);

				if(!bReturnValue)
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to prepare the source partition: %s\n", iterSysPartition->c_str());
					break;
				}
			}				

			if(bReturnValue)
			{
				if(m_strCloudEnv.compare("azure") == 0)
				{
					if(false == CreateProtectedDisksFile(strHostName))
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to write the disks information in protected list file\n.");
						bReturnValue = false;
					}
					else if(false == ModifyConfigForAzure(strHostName, "all", strMntDirectory))  //both are part of root partition
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to modify configuration files under /.");
						bReturnValue = false;
					}
					else
					{
						//write code for the script to execute
						std::string str_scriptsPath	= m_VxInstallPath + std::string(AZURE_SCRIPT_PATH);
						//append script file name to scripts path 
						std::string str_prepareTargetInformation = str_scriptsPath + std::string(LINUXP2V_PREPARETARGET_AZURE) + string(" ") + strMntDirectory;
						std::string str_outputFile = str_scriptsPath + std::string("PrepareTarget.log");

						DebugPrintf( SV_LOG_DEBUG, "Command = %s .\n" , str_prepareTargetInformation.c_str() );
						if(false == ExecuteCmdWithOutputToFileWithPermModes( str_prepareTargetInformation, str_outputFile, O_APPEND | O_RDWR | O_CREAT))
						{
							DebugPrintf(SV_LOG_ERROR, "Failed to run the script for physical machine with host id %s.\n" , strHostName.c_str() );
							bReturnValue	= false;
						}
					}
				}
				else if(m_strCloudEnv.compare("aws") == 0)
				{
					m_storageInfo.strIsETCseperateFS = "no";
					
					//Changes the vxagent and svagent (Inmage services) names in /etc/initr.d file to _old.
					MakeInMageSVCStop(strMntDirectory);
					
					//Removes all the invloflt related files
					if(RemoveInVolFltFiles(strMntDirectory))
					{
						DebugPrintf(SV_LOG_DEBUG, "Removed all the \"/etc/vxagent/involflt/_*\" files for host : %s\n", strHostName.c_str());
					}
					
					if(NetrworkConfigForAWS(strMntDirectory))
					{
						DebugPrintf(SV_LOG_INFO, "Successfully modified the recquired netowrk config files for host : %s\n", strHostName.c_str());
					}
					
					if(FsTabConfigForAWS(strMntDirectory))
					{
						DebugPrintf(SV_LOG_INFO, "Successfully modified the recquired fstab entries for host : %s\n", strHostName.c_str());
					}
					
					std::string str_scriptsPath	= m_VxInstallPath + std::string(AWS_SCRIPT_PATH);
					//append script file name to scripts path 
					std::string str_prepareTargetInformation = str_scriptsPath + std::string(LINUXP2V_PREPARETARGET_AWS) + string(" ") + strMntDirectory;
					std::string str_outputFile = str_scriptsPath + std::string("PrepareTarget.log");

					DebugPrintf( SV_LOG_DEBUG, "Command = %s .\n" , str_prepareTargetInformation.c_str() );
					if(false == ExecuteCmdWithOutputToFileWithPermModes( str_prepareTargetInformation, str_outputFile, O_APPEND | O_RDWR | O_CREAT))
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to run the script for host id %s.\n" , strHostName.c_str() );
						bReturnValue	= false;
					}
				}
				else if (boost::algorithm::iequals(m_strCloudEnv, MT_ENV_VMWARE))
				{
					if(false == CreateProtectedDisksFile(strHostName))
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to write the disks information in protected list file\n.");
						bReturnValue = false;
					}
					else if(false == ModifyConfigForVMWare(strHostName, "all", strMntDirectory))  //both are part of root partition
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to modify configuration files under /.");
						bReturnValue = false;
					}

					//
					// Network changes
					//
					if( bReturnValue && !ModifyNetworkConfigForVMWare(strMntDirectory) )
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to modify network configuration for the host: %s\n.",strHostName.c_str());
						bReturnValue = false;
					}
				}
				else
				{
					DebugPrintf(SV_LOG_DEBUG, "Cloud Environment \"%s\" is not supported currently.\n" , m_strCloudEnv.c_str() );
				}

				if(false == listFilesInDir(strMntDirectory, ""))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to List out all the files in directory : %s\n", strMntDirectory.c_str());
				}
			}

			map<string, StorageInfo_t>::const_reverse_iterator iterMnt = mapMountedPartitonInfo.rbegin();
			for(; iterMnt != mapMountedPartitonInfo.rend(); iterMnt++)
			{
				if(iterMnt->second.bMounted)
				{
					string strMntPartitionDir = strMntDirectory + iterMnt->first;
					if(!unmountPartition(strMntPartitionDir))
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to UnMount the Partition: %s\n",iterMnt->first.c_str());
						bReturnValue = false;
					}
					else if(iterMnt->second.bVgActive)
					{
						mountedVGs.insert(iterMnt->second.strVgName);
					}
				}
				else if(iterMnt->second.bVgActive)
				{
					mountedVGs.insert(iterMnt->second.strVgName);
				}
			}
			
			if( true != unmountPartition(strMntDirectory))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to UnMount the Partition: %s\n", strTgtPartitionOrLV.c_str());
				bReturnValue = false;
				break;
			}
			if("LVM" == storageInfo_root.strRootVolType)
			{
				mountedVGs.insert(storageInfo_root.strVgName);
			}
			if(!mountedVGs.empty())
			{
				std::set<std::string>::iterator vgs;
				for(vgs = mountedVGs.begin(); vgs != mountedVGs.end(); vgs++)
				{
					if(false == deActivateVolGrp(*vgs))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed To deactivate the VG : %s\n", vgs->c_str());
					}
				}
			}
			if( true != deleteDirectory(strMntDirectory))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to delete the directory\n");
			}
		}

	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bReturnValue;
}


bool ChangeVmConfig::GetDiskNameForDiskGuid(const std::string& strDiskGuid, std::string& strDiskName)
{
	DebugPrintf(SV_LOG_DEBUG , "Entering function %s.\n", __FUNCTION__);

    // Get the real device name here for a disk guid, i.e. persistent disk name
    std::map<std::string, std::string>::iterator guidIter = m_mapOfDiskGuidToDiskName.find(strDiskGuid);
    if ( guidIter == m_mapOfDiskGuidToDiskName.end())
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s for source disk guid %s could not find the disk name in guid map.\n", 
            FUNCTION_NAME,
            strDiskGuid.c_str());
        return false;
    }
    else
    {
        strDiskName = guidIter->second;
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

bool ChangeVmConfig::CreateProtectedDisksFile(string strHostId)
{
	DebugPrintf(SV_LOG_DEBUG , "Entering function %s.\n", __FUNCTION__);
	bool bReturnValue = true;

	do
	{
		string strProtectedDiskFileName = m_VxInstallPath + string("/failover_data/") + strHostId + string(".PROTECTED_DISKS_LIST");
		ofstream pairInfoFile;
		pairInfoFile.open(strProtectedDiskFileName.c_str(), std::ios::trunc);

		if(!pairInfoFile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed open create file : %s\n", strProtectedDiskFileName.c_str());
			bReturnValue = false;
			break;
		}

		map<string, string>::iterator iterMap;
		map<string, string> mapSrcToTgtDisks;
		map<string, map<string, string> >::iterator iterHost = m_mapOfVmToSrcAndTgtDisks.find(strHostId);
		if(iterHost == m_mapOfVmToSrcAndTgtDisks.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to find host [%s] in vm to disk mapping list.\n", strHostId.c_str());
			bReturnValue = false;
			pairInfoFile.close();
			break;
		}
		mapSrcToTgtDisks.insert(iterHost->second.begin(), iterHost->second.end());
		
        // the below call is to populate the m_mapOfDiskGuidToDiskName
        // also ignore any failures.
        getHostInfoUsingCXAPI(strHostId, std::string(), false, false);

		//Finding out source boot devices.
		m_storageInfo.reset();
		if(false == getHostInfoUsingCXAPI(strHostId, "/boot", false, true))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed while processing CX API xml file\n");
			bReturnValue = false;
			pairInfoFile.close();
			break;
		}

		set<string> setDisks;  //Contains all the required boot disks.
		list<string>::iterator iterList = m_storageInfo.listDiskName.begin();
		for(; iterList != m_storageInfo.listDiskName.end(); iterList++)
		{
			setDisks.insert(*iterList);
		}

		//First writing all the collected boot devices into the file which are part of protection.
		set<string>::iterator iterSet;
		for(iterSet = setDisks.begin(); iterSet != setDisks.end(); iterSet++)
		{
			iterMap = mapSrcToTgtDisks.find(*iterSet);
			if(iterMap != mapSrcToTgtDisks.end())
			{
                std::string strDiskName;
                if (!(bReturnValue = GetDiskNameForDiskGuid(iterMap->first, strDiskName)))
                    break;

                pairInfoFile<<strDiskName;
				pairInfoFile<<"!@!@!";
				pairInfoFile<<iterMap->second<<endl;
				mapSrcToTgtDisks.erase(iterMap);
			}			
		}

        // if diskguid mapping fails, bailout with error
        if (!bReturnValue)
        {
			pairInfoFile.close();
			break;
        }

		//Finding out source root devices.
		m_storageInfo.reset();
		if(false == getHostInfoUsingCXAPI(strHostId, "/", false, true))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed while processing CX API xml file\n");
			bReturnValue = false;
			pairInfoFile.close();
			break;
		}

		setDisks.clear();  //will contain all the required root disks.
		for(iterList = m_storageInfo.listDiskName.begin(); iterList != m_storageInfo.listDiskName.end(); iterList++)
		{
			setDisks.insert(*iterList);
		}

		//First writing all the collected root devices into the file which are part of protection.
		for(iterSet = setDisks.begin(); iterSet != setDisks.end(); iterSet++)
		{
			iterMap = mapSrcToTgtDisks.find(*iterSet);
			if(iterMap != mapSrcToTgtDisks.end())
			{
                std::string strDiskName;
                if (!(bReturnValue = GetDiskNameForDiskGuid(iterMap->first, strDiskName)))
                    break;

                pairInfoFile<<strDiskName;
				pairInfoFile<<"!@!@!";
				pairInfoFile<<iterMap->second<<endl;
				mapSrcToTgtDisks.erase(iterMap);
			}			
		}

        // if diskguid mapping fails, bailout with error
        if (!bReturnValue)
        {
			pairInfoFile.close();
			break;
        }

		m_storageInfo.reset(); //clearing the member variable

		//writing the remaining protected devices into the file
		for(iterMap = mapSrcToTgtDisks.begin(); iterMap != mapSrcToTgtDisks.end(); iterMap++)
		{
            std::string strDiskName;
            if (!(bReturnValue = GetDiskNameForDiskGuid(iterMap->first, strDiskName)))
                break;

            pairInfoFile<<strDiskName;
			pairInfoFile<<"!@!@!";
			pairInfoFile<<iterMap->second<<endl;
		}
		pairInfoFile.close();
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bReturnValue;
}


bool ChangeVmConfig::PrepareTargetVirtualConfiguration( std::string strHostName )
{
	DebugPrintf(SV_LOG_DEBUG , "Entering function %s.\n", __FUNCTION__);
	bool bReturnValue			= true;
	do
	{
		std::string OsType;
		std::string MachineType; // This will be either VirtualMachine or PhysicalMachine
		std::string strFailback;
	    
		if(false == xGetMachineAndOstypeAndV2P(strHostName, OsType, MachineType, strFailback))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to find Machine Type and OsType for %s\n", strHostName.c_str());
			bReturnValue = false;
			break;
		}

		if( MachineType.compare("PhysicalMachine") != 0 )
		{
			DebugPrintf(SV_LOG_INFO , "Host Id %s belongs to a virtual machine.", strHostName.c_str() );
			break;
		}

		//check for /etc
		m_storageInfo.reset();
		std::string str_modify = "";
		if(false == getHostInfoUsingCXAPI(strHostName , "/etc" , false ))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed while processing CX API call\n");
			DebugPrintf(SV_LOG_ERROR, "Cannot Proceed Further for Linux P2V configuration...\n");
			bReturnValue = false;
			break;
		}
		if ( ! m_storageInfo.strRootVolType.empty() )
		{
			DebugPrintf(SV_LOG_INFO, "Found seperate partition for etc.");
			str_modify = "etc";
			if ( false == ModifyConfiguration( strHostName , str_modify ) )
			{
				DebugPrintf( SV_LOG_ERROR , "Failed to modify configuration files under /etc.");
				bReturnValue = false;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Didn't find seperate partition for etc.");
			m_storageInfo.reset();
			if(false == getHostInfoUsingCXAPI(strHostName , "/" , false ))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed while processing CX API call\n");
				DebugPrintf(SV_LOG_ERROR, "Cannot Proceed Further for Linux P2V configuration...\n");
				bReturnValue = false;
				break;
			}

			if ( ! m_storageInfo.strRootVolType.empty() )
			{
				DebugPrintf(SV_LOG_INFO, "Found /etc on /.");
				str_modify = "etc";
				if ( false == ModifyConfiguration( strHostName , str_modify ) )
				{
					DebugPrintf( SV_LOG_ERROR , "Failed to modify configuration files under /etc.");
					bReturnValue = false;
				}
			}
		}

		m_storageInfo.reset();
		if(false == getHostInfoUsingCXAPI(strHostName , "/boot" , false ))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed while processing CX API call\n");
			DebugPrintf(SV_LOG_ERROR, "Cannot Proceed Further for Linux P2V configuration...\n");
			bReturnValue = false;
			break;
		}

		if ( ! m_storageInfo.strRootVolType.empty() )
		{
			//Found seperate Partition for "/boot".
			DebugPrintf(SV_LOG_INFO, "Found seperate partition for boot.");
			str_modify = "boot";
			if ( false == ModifyConfiguration( strHostName , str_modify ) )
			{
				DebugPrintf( SV_LOG_ERROR , "Failed to modify configuration files under /boot.");
				bReturnValue = false;
			}

			if ( !m_bV2P )
			{
				if ( false == InstallGRUB( strHostName , str_modify ) )
				{
					DebugPrintf( SV_LOG_ERROR , "Failed to install GRUB for host with HostId %s.", strHostName.c_str());
					bReturnValue	= false;
				}
			}
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Didn't find seperate partition for boot.");
			m_storageInfo.reset();
			if(false == getHostInfoUsingCXAPI(strHostName , "/" , false ))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed while processing CX API call\n");
				DebugPrintf(SV_LOG_ERROR, "Cannot Proceed Further for Linux P2V configuration...\n");
				bReturnValue = false;
				break;
			}
			str_modify = "all";
			if ( ! m_storageInfo.strRootVolType.empty() )
			{
				if ( false == ModifyConfiguration( strHostName , str_modify ) )
				{
					DebugPrintf( SV_LOG_ERROR , "Failed to modify configuration files under /boot.");
					bReturnValue = false;
					break;
				}

				if ( !m_bV2P )
				{
					if ( false == InstallGRUB( strHostName , str_modify ) )
					{
						DebugPrintf( SV_LOG_ERROR , "Failed to install GRUB for host with HostId %s.", strHostName.c_str());
						bReturnValue	= false;
					}
				}
			}
		}

		//Check if the modified is either for boot or all.
		//We have to make sure that bReturnValue is not equal to false.
		if ( ! bReturnValue )
		{
			DebugPrintf( SV_LOG_INFO, "Skipping GURB install checks.");
			break;
		}
	}while(0);
		
	DebugPrintf(SV_LOG_DEBUG , "Exiting function %s.\n", __FUNCTION__);
	return bReturnValue;
}

bool ChangeVmConfig::InstallGRUB( std::string str_hostId , std::string strModifyFor )
{
	DebugPrintf(SV_LOG_DEBUG , "Entering function %s.\n", __FUNCTION__);
	bool bReturnValue			= true;

	//First we have to check whether file exists or not. IT might not be DL 380 series of HP?
	std::string str_fileToCheck	= m_VxInstallPath + std::string("/failover_data/") + str_hostId + std::string("_Configuration.info");
	if ( ! checkIfFileExists( str_fileToCheck ) )
	{
		DebugPrintf( SV_LOG_INFO , "File %s does not exists for host with host id %s." , str_fileToCheck.c_str() , str_hostId.c_str() );
		DebugPrintf( SV_LOG_INFO , "No need to install GRUB for above machine." );
		return bReturnValue;
	}

	//check if the partiton is an LVM ? if LVM skip it.
	do
	{
		if("LVM" == m_storageInfo.strRootVolType)
		{
			DebugPrintf( SV_LOG_INFO , "As partition for /boot is an LVM, skipping the GRUB installation part.");
			break;
		}

		string strSrcPartition = m_storageInfo.strPartition;
		std::string strTgtPartitionOrLV;
		if(false == processPartition(str_hostId, strSrcPartition, strTgtPartitionOrLV))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to install GRUB for host with host id %s.\n" , str_hostId.c_str() );
			bReturnValue = false;
			break;		
		}

		//We have target partition.
		std::string str_inputFile = m_strDatafolderPath + str_hostId + std::string(".PrepareTargetFile");
		DebugPrintf(SV_LOG_INFO, "Input File = %s.\n", str_inputFile.c_str() );
		std::ofstream WriteInputFile;
		WriteInputFile.open( str_inputFile.c_str() , std::ios::out | std::ios::trunc );
		if ( !WriteInputFile.is_open() )
		{				
			DebugPrintf(SV_LOG_ERROR,"Failed to open %s for writing\n",str_inputFile.c_str());
			bReturnValue = false;
			break;
		}

		//Write Input File.
		WriteInputFile<<"_MACHINE_UUID_ "<<str_hostId.c_str()<<endl;
		WriteInputFile<<"_TASK_ "<<"INSTALLGRUB"<<endl;
		WriteInputFile<<"_PLAN_NAME_ "<<m_strDatafolderPath.c_str()<<endl; //here we are sending complete path to Plan instead of PLAN NAME.
		WriteInputFile<<"_VX_INSTALL_PATH_ "<<m_VxInstallPath.c_str()<<endl;
		WriteInputFile<<"_MPARTITION_ "<<strTgtPartitionOrLV.c_str()<<endl;
		WriteInputFile<<"_MOUNT_PATH_FOR_ "<<strModifyFor.c_str()<<endl;
				
		//Now we have to close the open handle.
		WriteInputFile.close();
        securitylib::setPermissions(str_inputFile);

		std::string str_scriptsPath		= m_VxInstallPath + std::string("scripts/vCon/");
		//append script file name to scripts path 
		std::string str_prepareTargetInformation = str_scriptsPath + std::string(LINUXP2V_PREPARETARGET_INFORMATION) + std::string(" ") + str_inputFile;
		std::string str_outputFile = str_scriptsPath + std::string("PrepareTarget.log");

		DebugPrintf( SV_LOG_INFO , "Command = %s .\n" , str_prepareTargetInformation.c_str() );
		if(false == ExecuteCmdWithOutputToFileWithPermModes( str_prepareTargetInformation, str_outputFile, O_APPEND | O_RDWR | O_CREAT))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to install GRUB on physical machine with host id %s.\n" , str_hostId.c_str() );
			bReturnValue	= false;
		}
	}while(0);

	DebugPrintf(SV_LOG_DEBUG , "Exiting function %s.\n", __FUNCTION__);
	return bReturnValue;
}

bool ChangeVmConfig::ModifyNetworkConfigForVMWare(const string& rootMntPath)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering function %s\n", __FUNCTION__);
	bool bReturnValue = true;

	// TODO-V2: The logic will configure all failback vm nics to DHCP
	//         
	//
	std::stringstream cmdConfigureNetwork;
	cmdConfigureNetwork << m_VxInstallPath << "scripts/vCon/"
		<< VMWARE_CONFIGURE_NETWORK_SCRIPT << " "
		<< rootMntPath;
	
	std::string outputFileName = m_VxInstallPath + "scripts/vCon/";
	outputFileName += "vmware-configure-network.log";

	if(false == ExecuteCmdWithOutputToFileWithPermModes( cmdConfigureNetwork.str(), outputFileName, O_APPEND | O_RDWR | O_CREAT))
	{
		DebugPrintf(SV_LOG_ERROR, "Network configuration has failed.\n");
		bReturnValue	= false;
	}

	DebugPrintf(SV_LOG_DEBUG , "Exiting function %s.\n", __FUNCTION__);
	return bReturnValue;
}

bool ChangeVmConfig::ModifyConfigForVMWare(string strHostId, string strModifyFor, string strMountPath)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering function %s\n", __FUNCTION__);
	bool bReturnValue = true;

	do
	{
		std::string strInputFile = m_strDatafolderPath + strHostId + std::string(".PrepareTargetFile");
		DebugPrintf(SV_LOG_DEBUG, "Input File = %s.\n", strInputFile.c_str() );
		ofstream WriteInputFile;
		WriteInputFile.open( strInputFile.c_str() , std::ios::out | std::ios::trunc );
		if ( !WriteInputFile.is_open() )
		{				
			DebugPrintf(SV_LOG_ERROR,"Failed to open %s for writing\n",strInputFile.c_str());
			bReturnValue = false;
			break;
		}

		//Write Input File.
		WriteInputFile << "_MACHINE_UUID_ " << strHostId << endl;
		WriteInputFile << "_TASK_ " << "PREPARETARGET" << endl;
		WriteInputFile << "_PLAN_NAME_ " << m_strDatafolderPath << endl; //here we are sending complete path to Plan instead of PLAN NAME.
		WriteInputFile << "_VX_INSTALL_PATH_ " << m_VxInstallPath << endl;
		WriteInputFile << "_MOUNT_PATH_ " << strMountPath << endl;
		WriteInputFile << "_MOUNT_PATH_FOR_ " << strModifyFor << endl;
		WriteInputFile << "_INSTALL_VMWARE_TOOLS_ 1" << endl;
		WriteInputFile << "_UNINSTALL_VMWARE_TOOLS_ 0" << endl;

		std::string strNewHostId = GetUuid();
		DebugPrintf( SV_LOG_DEBUG, "New Host ID Value = %s.\n", strNewHostId.c_str() );

		WriteInputFile << "_NEW_GUID_ "<<strNewHostId<< endl;

		//Now we have to close the open handle.
		WriteInputFile.close();
		securitylib::setPermissions(strInputFile);

		std::string strScriptsPath	= m_VxInstallPath + string("scripts/vCon/");
		//append script file name to scripts path 
		std::string strPrepareTargetInformation = strScriptsPath + string(LINUXP2V_PREPARETARGET_INFORMATION) + string(" ") + strInputFile;
		std::string strOutputFile = strScriptsPath + string("PrepareTarget.log");

		DebugPrintf(SV_LOG_DEBUG, "Command = %s\n" , strPrepareTargetInformation.c_str() );
		if(false == ExecuteCmdWithOutputToFileWithPermModes(strPrepareTargetInformation, strOutputFile, O_APPEND | O_RDWR | O_CREAT))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to modify the recovering vm configuration for %s with host id %s.\n" , strModifyFor.c_str(), strHostId.c_str() );
			bReturnValue	= false;
		}

	}while(0);
	DebugPrintf(SV_LOG_DEBUG, "Exiting function %s\n", __FUNCTION__);
	return bReturnValue;
}

bool ChangeVmConfig::ModifyConfigForAzure(string strHostId, string strModifyFor, string strMountPath)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering function %s\n", __FUNCTION__);
	bool bReturnValue = true;

	do
	{
		std::string strInputFile = m_strDatafolderPath + strHostId + std::string(".PrepareTargetFile");
		DebugPrintf(SV_LOG_DEBUG, "Input File = %s.\n", strInputFile.c_str() );
		ofstream WriteInputFile;
		WriteInputFile.open( strInputFile.c_str() , std::ios::out | std::ios::trunc );
		if ( !WriteInputFile.is_open() )
		{				
			DebugPrintf(SV_LOG_ERROR,"Failed to open %s for writing\n",strInputFile.c_str());
			bReturnValue = false;
			break;
		}

		//Write Input File.
		WriteInputFile << "_MACHINE_UUID_ " << strHostId << endl;
		WriteInputFile << "_TASK_ " << "PREPARETARGET" << endl;
		WriteInputFile << "_PLAN_NAME_ " << m_strDatafolderPath << endl; //here we are sending complete path to Plan instead of PLAN NAME.
		WriteInputFile << "_VX_INSTALL_PATH_ " << m_VxInstallPath << endl;
		WriteInputFile << "_MOUNT_PATH_ " << strMountPath << endl;
		WriteInputFile << "_MOUNT_PATH_FOR_ " << strModifyFor << endl;
		WriteInputFile << "_INSTALL_VMWARE_TOOLS_ 0" << endl;
		WriteInputFile << "_UNINSTALL_VMWARE_TOOLS_ 1" << endl;

		//TFS ID : 1269459.
		std::string strNewHostId = GetUuid();
		DebugPrintf( SV_LOG_DEBUG, "New Host ID Value = %s.\n", strNewHostId.c_str() );

		WriteInputFile << "_NEW_GUID_ "<<strNewHostId<< endl;
			
		//Now we have to close the open handle.
		WriteInputFile.close();
        securitylib::setPermissions(strInputFile);

		std::string strScriptsPath	= m_VxInstallPath + string("scripts/vCon/");
		//append script file name to scripts path 
		std::string strPrepareTargetInformation = strScriptsPath + string(LINUXP2V_PREPARETARGET_INFORMATION) + string(" ") + strInputFile;
		std::string strOutputFile = strScriptsPath + string("PrepareTarget.log");

		DebugPrintf(SV_LOG_DEBUG, "Command = %s\n" , strPrepareTargetInformation.c_str() );
		if(false == ExecuteCmdWithOutputToFileWithPermModes(strPrepareTargetInformation, strOutputFile, O_APPEND | O_RDWR | O_CREAT))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to write protected disk information for physical machine with host id %s.\n" , strHostId.c_str() );
			bReturnValue	= false;
		}
	}while(0);

	DebugPrintf(SV_LOG_DEBUG, "Exiting function %s\n", __FUNCTION__);
	return bReturnValue;
}

bool ChangeVmConfig::ModifyConfiguration( std::string str_hostId , std::string strModifyFor )
{
	DebugPrintf(SV_LOG_DEBUG , "Entering function %s.\n", __FUNCTION__);
	bool bReturnValue			= true;
	LinuxReplication LinObj1;
	do
	{
		string strTgtPartitionOrLV;
		if("LVM" == m_storageInfo.strRootVolType)
		{
			strTgtPartitionOrLV = m_storageInfo.strLvName;
			if(false == activateVolGrp(m_storageInfo.strVgName))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed To Activate the LV : %s\n", strTgtPartitionOrLV.c_str());
				bReturnValue = false;
				break;		
			}
		}
		else
		{
			string strSrcPartition = m_storageInfo.strPartition;
			if(false == processPartition(str_hostId, strSrcPartition, strTgtPartitionOrLV))
			{
				DebugPrintf(SV_LOG_ERROR, "Cannot Proceed Further for Linux P2V configuration...\n");
				bReturnValue = false;
				break;		
			}
		}

		for( int i=0 ; i <= 3 ; i++ ) 
		{
			if( 3 == i )
			{
				DebugPrintf(SV_LOG_ERROR , "Could not modify configuration. Tried 3 times.\n");
				bReturnValue	= false;
				break;
			}
			string strTimeStamp = LinObj1.GeneratreUniqueTimeStampString();
			DebugPrintf(SV_LOG_INFO, "time Stamp: %s\n", strTimeStamp.c_str());
			string strMntDirectory = string("/mnt/inmage_mnt_") + strTimeStamp;

			if( true != createDirectory(strMntDirectory))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to create the directory. Trying again...\n");
				sleep(4);
				continue;
			}
				
			if( true != mountPartition(strMntDirectory, strTgtPartitionOrLV, 
                                       strMntDirectory))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to Mount the Parttion: %s Trying again after 4 secs...\n", strTgtPartitionOrLV.c_str());
				sleep(4);
				continue;
			}

			//Successfully Mounted partition.Now we have to write Input file and pass it.
			std::string str_inputFile = m_strDatafolderPath + str_hostId + std::string(".PrepareTargetFile");
			DebugPrintf(SV_LOG_INFO, "Input File = %s.\n", str_inputFile.c_str() );
			std::ofstream WriteInputFile;
			WriteInputFile.open( str_inputFile.c_str() , std::ios::out | std::ios::trunc );
			if ( !WriteInputFile.is_open() )
			{				
				DebugPrintf(SV_LOG_ERROR,"Failed to open %s for writing\n",str_inputFile.c_str());
				bReturnValue = false;
				break;
			}

			//Write Input File.
			WriteInputFile<<"_MACHINE_UUID_ "<<str_hostId.c_str()<<endl;
			
			if ( m_bV2P )
				WriteInputFile<<"_TASK_ "<<"PREPARETARGET_V2P"<<endl;
			else
				WriteInputFile<<"_TASK_ "<<"PREPARETARGET"<<endl;

			WriteInputFile<<"_PLAN_NAME_ "<<m_strDatafolderPath.c_str()<<endl; //here we are sending complete path to Plan instead of PLAN NAME.
			WriteInputFile<<"_VX_INSTALL_PATH_ "<<m_VxInstallPath.c_str()<<endl;
			WriteInputFile<<"_MOUNT_PATH_ "<<strMntDirectory.c_str()<<endl;
			WriteInputFile<<"_MOUNT_PATH_FOR_ "<<strModifyFor.c_str()<<endl;

			//TFS ID : 1269459.
			std::string strNewHostId = GetUuid();
			DebugPrintf( SV_LOG_INFO, "New Host ID Value = %s.\n", strNewHostId.c_str() );

			WriteInputFile << "_NEW_GUID_ "<<strNewHostId<< endl;
				
			//Now we have to close the open handle.
			WriteInputFile.close();
            securitylib::setPermissions(str_inputFile);

			std::string str_scriptsPath		= m_VxInstallPath + std::string("scripts/vCon/");
			//append script file name to scripts path 
			std::string str_prepareTargetInformation = str_scriptsPath + std::string(LINUXP2V_PREPARETARGET_INFORMATION) + std::string(" ") + str_inputFile;
			std::string str_outputFile = str_scriptsPath + std::string("PrepareTarget.log");

			DebugPrintf( SV_LOG_INFO , "Command = %s .\n" , str_prepareTargetInformation.c_str() );
			if(false == ExecuteCmdWithOutputToFileWithPermModes( str_prepareTargetInformation, str_outputFile, O_APPEND | O_RDWR | O_CREAT))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to write protected disk information for physical machine with host id %s.\n" , str_hostId.c_str() );
				bReturnValue	= false;
			}

			if( true != unmountPartition(strMntDirectory))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to UnMount the Partition: %s Trying again by killing reference PID\n", strTgtPartitionOrLV.c_str());
				bReturnValue = false;
				break;
			}
				
			if( true != deleteDirectory(strMntDirectory))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to delete the directory\n");
			}
			break;
		}
			
		if("LVM" == m_storageInfo.strRootVolType)
		{
			if(false == deActivateVolGrp(m_storageInfo.strVgName))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed To deactivate the LV : %s\n", m_storageInfo.strLvName.c_str());
				bReturnValue = false;
			}
		}
	}while(0);
	DebugPrintf(SV_LOG_DEBUG , "Exiting function %s.\n", __FUNCTION__);
	return bReturnValue;
}

bool ChangeVmConfig::GetFileNames( std::string strFileSearchPattern , std::string strSearchInPath , std::list<std::string>& list_FileNames )
{
	DebugPrintf(SV_LOG_DEBUG , "Entering function %s.\n", __FUNCTION__);
	bool bReturnValue	= true;

	do
	{
		std::string strcmd = std::string("find ") +  strSearchInPath + std::string (" -name \"") + strFileSearchPattern + std::string("\"");
		DebugPrintf(SV_LOG_INFO, "command constructed to find file = %s.\n", strcmd.c_str());
		std::string strOutFileName = strSearchInPath + std::string("InMageOutput.log_inmage");

		LinuxReplication obj1;
		if ( !obj1.ExecuteCmdWithOutputToFile( strcmd , strOutFileName ) )
		{
			bReturnValue = false;
			break;
		}

		//Now we have to read  the File.
		std::string line;
		std::ifstream readfile(strOutFileName.c_str());
		if (!readfile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR , "Failed to read file %s.\n", strOutFileName.c_str());
			bReturnValue = false;
			break;
		}
		while ( getline (readfile,line) )
		{
			list_FileNames.push_back(line);
		}
		readfile.close();
		//here we need to delete the file.
	}while(0);

	DebugPrintf(SV_LOG_DEBUG , "Exiting function %s.\n", __FUNCTION__);
	return ( bReturnValue );
}

bool ChangeVmConfig::RenameFile( std::string strOldFileName , std::string strNewFileName )
{
	DebugPrintf(SV_LOG_DEBUG , "Entering function %s.\n", __FUNCTION__);
	bool bReturnValue	= true;
	DebugPrintf(SV_LOG_INFO, "Renaming file %s to %s.\n", strOldFileName.c_str() , strNewFileName.c_str() );
	int iResult = rename( strOldFileName.c_str() , strNewFileName.c_str() );
	if( 0 != iResult )
	{
		DebugPrintf( SV_LOG_ERROR , "Failed to rename file %s to %s", strOldFileName.c_str() , strNewFileName.c_str() );
		bReturnValue = false;
	}

	DebugPrintf(SV_LOG_DEBUG , "Exiting function %s.\n", __FUNCTION__);
	return bReturnValue;
}

bool ChangeVmConfig::MakeChangesPostRollback(std::string HostName, bool bP2VChanges, std::string& OsType )
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	m_strHostName = HostName;
	m_NetworkInfo.clear();
	m_storageInfo.reset();

	if ( true == bP2VChanges )
	{
		bResult		= PrepareTargetVirtualConfiguration( HostName/*But this is Host ID*/ );
		if ( true != bResult )
		{
			//suppose if network changes are not done, are we de-listing the machine from recovered machines list? My Answer : NO. 
			DebugPrintf( SV_LOG_ERROR , "Failed to rename initrd image file.\n");
			DebugPrintf( SV_LOG_ERROR , "1. After recovery, mount /boot path to master target and rename Initrd image file." );
			bResult	= false;
		}
		else
			DebugPrintf( SV_LOG_ERROR , "Successfully renamed the initrd image file.\n");

		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return bResult;
	}
	
	bool bProcessOld = false;
	if(false == getHostInfoUsingCXAPI(HostName, "/etc"))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed while processing CX API call\n");
		bProcessOld = true;
	}
	
	if ( m_storageInfo.strRootVolType.empty() )
	{
		bProcessOld = false;
		if(false == getHostInfoUsingCXAPI(HostName))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed while processing CX API call\n");
			bProcessOld = true;			
		}
	}
	
	if(bProcessOld)
	{
		if(false == getNwInfoFromXmlFile(HostName))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed while getting network information from XML file.\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
	}
	
	string strTgtPartitionOrLV;
	if("LVM" == m_storageInfo.strRootVolType)
	{
		strTgtPartitionOrLV = m_storageInfo.strLvName;
		if(false == activateVolGrp(m_storageInfo.strVgName))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed To Activate the LV : %s\n", strTgtPartitionOrLV.c_str());
			bResult = false;
		}
	}
	else
	{
		string strSrcPartition = m_storageInfo.strPartition;
		if(false == processPartition(HostName, strSrcPartition, strTgtPartitionOrLV))
		{
			DebugPrintf(SV_LOG_ERROR, "Cannot Proceed Further for Linux Network configuration...\n");
			bResult = false;
		}
	}
	if(bResult)
	{
		//This checks the FileSytem to get the consistence data. 
		//Sometimes we are facing inconsistency in filesytem after recovery of that device.
		if(false == checkFSofDevice(strTgtPartitionOrLV))
		{
			DebugPrintf(SV_LOG_ERROR, "[Warning] Unable to check the FileSystem of device: %s\n", strTgtPartitionOrLV.c_str());
		}
		int i=0;
		for(;i < 3 ; i++)
		{
			LinuxReplication LinObj1;
			string strTimeStamp = LinObj1.GeneratreUniqueTimeStampString();
			DebugPrintf(SV_LOG_INFO, "time Stamp: %s\n", strTimeStamp.c_str());
			string strMntDirectory = string("/mnt/inmage_mnt_") + strTimeStamp;
			if(false == createDirectory(strMntDirectory))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to create the directory. Trying again...\n");
				sleep(4);
				continue;
			}
			else
			{
				if(false == mountPartition(strMntDirectory, strTgtPartitionOrLV, 
                                           strMntDirectory))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to Mount the Parttion: %s Trying again after 4 secs...\n", strTgtPartitionOrLV.c_str());
					sleep(4);
					continue;
				}
				
				else
				{
					if(m_bDrDrill)
					{
						//Stopping the inmage services in the dr-drilled machine, 
						MakeInMageSVCStop(strMntDirectory); 
					}
					if(false == setNetworkConfig(HostName, strMntDirectory))
					{
						DebugPrintf(SV_LOG_ERROR, "Failed during network configuration. Trying Again after 4 secs...\n");
						unmountPartition(strTgtPartitionOrLV);
						sleep(4);
						continue;
					}
				
					if(false == unmountPartition(strMntDirectory))
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to UnMount the Parttion: %s Trying Again after killing reference PID\n", strTgtPartitionOrLV.c_str());
					}
				}
				if(false == deleteDirectory(strMntDirectory))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to delete the directory\n");
				}
			}
			if(bResult)
			{
				break;
			}
		}
		if(i == 3)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed set the Network configuration in all attempts...\n\n");
			bResult = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Successfulluy configured the network Settings.\n");
		}
		if("LVM" == m_storageInfo.strRootVolType)
		{
			if(false == deActivateVolGrp(m_storageInfo.strVgName))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed To deactivate the LV : %s\n", m_storageInfo.strLvName.c_str());
				bResult = false;
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

/*******************************************************************************
This function will prepare the xml file for CX API and Executes the CX API call.
In resopnse of that call it gets a xml file as response, 
output = strCxXmlFile
*********************************************************************************/
bool ChangeVmConfig::getHostInfoUsingCXAPI(const string strHostUuid , std::string strMountPoint , bool fetchNetworkInfo , bool bGetDisks)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	string strCxXmlFile = m_VxInstallPath + string("failover_data/") + strHostUuid + string("_cx_api.xml");	
	DebugPrintf(SV_LOG_DEBUG,"Xml File uploaded from CX is : %s\n", strCxXmlFile.c_str());
	
	if(false == parseCxApiXmlFile(strCxXmlFile, strHostUuid , strMountPoint , fetchNetworkInfo , bGetDisks))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to parse the xml file of Cx API call\n");
		bResult = false;
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


/**************************************************************************
This function will process the responsed xml file of CxAPI call.
It will get the network as well as storage information from the xml file.
Input -- Xml File 
**************************************************************************/
bool ChangeVmConfig::parseCxApiXmlFile(string strCxXmlFile, const string strHostUuid , const std::string strMountPoint , bool fetchNetworkInfo , bool bGetDisks)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	xmlDocPtr xDoc;	
	xmlNodePtr xCur;
	m_strOsType.clear();

	xDoc = xmlParseFile(strCxXmlFile.c_str());
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
	string xmlHostId;
	if(false == xGetValueForProp(xCur, "HostGUID", xmlHostId))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to find out the Host Id from xml file\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
	if(false == xGetValueForProp(xCur, "Caption", m_strOsType))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to find out OS Name from xml file\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
	DebugPrintf(SV_LOG_DEBUG,"InMageHotId from XML : %s\n", xmlHostId.c_str());
	DebugPrintf(SV_LOG_DEBUG,"OS : %s\n", m_strOsType.c_str());

	if(strHostUuid.compare(xmlHostId) == 0)
	{
		xChildNode = xGetChildNodeWithAttr(xCur,(const xmlChar*)"ParameterGroup",(const xmlChar*)"Id", (const xmlChar*)"DiskList");
		if (xChildNode == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <ParameterGroup with required Id=DiskList> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			xmlFreeDoc(xDoc); return false;			
		}
		else
		{
			if(false == xGetStorageInfo(xChildNode , strMountPoint , bGetDisks))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the Storage information from xml file\n");
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				xmlFreeDoc(xDoc); return false;
			}
		}
		
		if ( fetchNetworkInfo )
		{
			xChildNode = xGetChildNodeWithAttr(xCur,(const xmlChar*)"ParameterGroup",(const xmlChar*)"Id", (const xmlChar*)"MacAddressList");
			if (xChildNode == NULL)
			{
				DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <ParameterGroup with required Id=MacAddressList> entry not found\n");
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				xmlFreeDoc(xDoc); return false;			
			}
			else
			{
				if(false == xGetNetworkInfo(xChildNode))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to get the Network information from xml file\n");
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					xmlFreeDoc(xDoc); return false;
				}
			}
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Required hostinformation does not found in the CxPAI response xml file for host %s\n",strHostUuid.c_str());
		bResult = false;
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

//This Function will collect all the required storage information from the CxAPI response xml file
//This will fill the m_storageInfo structure.
bool ChangeVmConfig::xGetStorageInfo(xmlNodePtr xCur , std::string strMountPoint , bool bGetDisks )
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	m_storageInfo.reset();
	
	for(xmlNodePtr xChild = xCur->children; xChild != NULL; xChild = xChild->next)
	{
		if (!xmlStrcasecmp(xChild->name, (const xmlChar*)"ParameterGroup"))
		{
			xmlChar* attr_value = xmlGetProp(xChild,(const xmlChar*)"Id");
            if (attr_value == NULL)
            {
                DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Disk ParameterGroup Id> entry not found\n");
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	            return false;
            }
            string disk = std::string((char *)attr_value);
			string diskName;
			if(false == xGetValueForProp(xChild, "DiskName", diskName))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the value for DiskName\n");
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}

			DebugPrintf(SV_LOG_DEBUG,"Selected disk from XML : %s And Disk Name is : %s\n",disk.c_str(), diskName.c_str());

#ifndef WIN32
			std::string diskGuid;
			if(false == xGetValueForProp(xChild, "DiskGuid", diskGuid))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the value for DiskGuid\n");
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}

			DebugPrintf(SV_LOG_DEBUG,"Selected disk from XML : %s And Disk Name is : %s and Disk Guid is %s\n",disk.c_str(), diskName.c_str(), diskGuid.c_str());
            m_mapOfDiskGuidToDiskName.insert(std::make_pair(diskGuid, diskName));
#endif
			
			string DiskVg;
			if(false == xGetValueForProp(xChild, "DiskGroup", DiskVg))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the value for DiskGroup at Disk Level\n");
			}
			
			for(xmlNodePtr xChild1 = xChild->children; xChild1 != NULL; xChild1 = xChild1->next)
			{
				if (!xmlStrcasecmp(xChild1->name, (const xmlChar*)"ParameterGroup"))
				{
					attr_value = xmlGetProp(xChild1,(const xmlChar*)"Id");
					if (attr_value == NULL)
					{
						DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Partition ParameterGroup Id> entry not found\n");
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						return false;
					}
					string partition = std::string((char *)attr_value);
					string partitionName;
					if(false == xGetValueForProp(xChild1, "Name", partitionName))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to get the value for Partiton Name\n");
						partitionName = diskName;
						/*DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						return false;*/
					}
					DebugPrintf(SV_LOG_DEBUG,"Selected partition from XML : %s And partition Name is : %s\n",partition.c_str(), partitionName.c_str());
					
					string PartVg;
					if(false == xGetValueForProp(xChild, "DiskGroup", PartVg))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to get the value for DiskGroup at Partition Level\n");
					}

					bool bVgExist = false;
					
					for(xmlNodePtr xChild2 = xChild1->children; xChild2 != NULL; xChild2 = xChild2->next)
					{
						if (!xmlStrcasecmp(xChild2->name, (const xmlChar*)"ParameterGroup"))
						{
							xmlChar* attr_value = xmlGetProp(xChild2,(const xmlChar*)"Id");
							if (attr_value == NULL)
							{
								DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Logical Volume ParameterGroup Id> entry not found\n");
								DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
								return false;
							}
							string lv = std::string((char *)attr_value);
							string lvName, systemVolume, mountPoint;
							if(false == xGetValueForProp(xChild2, "Name", lvName))
							{
								DebugPrintf(SV_LOG_ERROR,"Failed to get the value for Logical Volume Name\n");
								DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
								return false;
							}
							DebugPrintf(SV_LOG_DEBUG,"Selected Logical Volume from XML : %s And Logical Volume Name is : %s\n",lv.c_str(), lvName.c_str());
							m_lvs.insert(lvName);
							
							bVgExist = true;		//This is to inform that there is volume group exist in this partition.
							
							string vg;
							if(false == xGetValueForProp(xChild2, "DiskGroup", vg))
							{
								DebugPrintf(SV_LOG_ERROR,"Failed to get the value for DiskGroup\n");
							}

							if(vg.empty())
							{
								if(PartVg.empty())
								{
									if(DiskVg.empty())
									{
										DebugPrintf(SV_LOG_ERROR,"VG name for the particular LV does not found in gethostinfo\n");
									}
									else
									{
										std::string Separator = std::string(",");
										size_t pos = 0;
										size_t found;
										found = DiskVg.find(Separator, pos);
										if(found == std::string::npos)
										{
											DebugPrintf(SV_LOG_DEBUG,"Only single VG found.\n");
											vg = DiskVg;
										}
										else
										{
											vg = DiskVg.substr(pos,found-pos);
										}
									}
								}
								else
									vg = PartVg;
							}
							DebugPrintf(SV_LOG_DEBUG,"VG - [%s]\n",vg.c_str());
							
							if(false == xGetValueForProp(xChild2, "IsSystemVolume", systemVolume))
							{
								DebugPrintf(SV_LOG_ERROR,"Failed to get the value for IsSystemVolume\n");
								DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
								return false;
							}
							if(false == xGetValueForProp(xChild2, "MountPoint", mountPoint))
							{
								DebugPrintf(SV_LOG_ERROR,"Failed to get the value for mountPoint\n");
								DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
								return false;
							}
							if(mountPoint.compare(strMountPoint) != 0)
								continue;
							else
							{
								if ( strMountPoint.compare("/") == 0 )
									m_storageInfo.strIsETCseperateFS = "no";
								else
									m_storageInfo.strIsETCseperateFS = "yes";
								m_storageInfo.strRootVolType = "LVM";
								m_storageInfo.strVgName = vg;
								m_storageInfo.strLvName = lvName;
								m_storageInfo.strPartition = partitionName;
								m_storageInfo.strMountPoint = strMountPoint;
								if(false == xGetValueForProp(xChild2, "FileSystemType", m_storageInfo.strFSType))
								{
									DebugPrintf(SV_LOG_ERROR,"Failed to get the value for mountPoint\n");
									m_storageInfo.strFSType = "ext3";
								}
									
								if(bGetDisks)
								{
									DebugPrintf(SV_LOG_DEBUG,"Got disk: %s \n", diskName.c_str());
									m_storageInfo.listDiskName.push_back(diskName);
								}
								else
								{
									DebugPrintf(SV_LOG_DEBUG,"Partition of %s Volume is : %s \n", strMountPoint.c_str(), m_storageInfo.strPartition.c_str());
									DebugPrintf(SV_LOG_DEBUG,"Type of %s Volume is : %s \n", strMountPoint.c_str(), m_storageInfo.strRootVolType.c_str());
									DebugPrintf(SV_LOG_DEBUG,"VG of %s Volume is : %s \n", strMountPoint.c_str(), m_storageInfo.strVgName.c_str());
									DebugPrintf(SV_LOG_DEBUG,"LV of %s Volume is : %s \n", strMountPoint.c_str(), m_storageInfo.strLvName.c_str());
									DebugPrintf(SV_LOG_DEBUG,"FileSystem of %s Volume is : %s \n", strMountPoint.c_str(), m_storageInfo.strFSType.c_str());
									DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);										
									return true;
								}
							}
						}
					}
					if(!bVgExist)
					{
						string systemVolume, mountPoint;
						if(false == xGetValueForProp(xChild1, "IsSystemVolume", systemVolume))
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to get the value for IsSystemVolume\n");
							DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
							return false;
						}
						if(false == xGetValueForProp(xChild1, "MountPoint", mountPoint))
						{
							DebugPrintf(SV_LOG_ERROR,"Failed to get the value for mountPoint\n");
							DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
							return false;
						}
						if(mountPoint.compare(strMountPoint) != 0)
							continue;
						else
						{
							if ( strMountPoint.compare("/") == 0 )
								m_storageInfo.strIsETCseperateFS = "no";
							else
								m_storageInfo.strIsETCseperateFS = "yes";
							m_storageInfo.strRootVolType = "FS";
							m_storageInfo.strPartition = partitionName;
							m_storageInfo.strMountPoint = strMountPoint;
							if(false == xGetValueForProp(xChild1, "FileSystemType", m_storageInfo.strFSType))
							{
								DebugPrintf(SV_LOG_ERROR,"Failed to get the value for mountPoint\n");
								m_storageInfo.strFSType = "ext3";
							}

							if(bGetDisks)
							{
								DebugPrintf(SV_LOG_DEBUG,"Got disk: %s \n", diskName.c_str());
								m_storageInfo.listDiskName.push_back(diskName);
							}
							else
							{
								DebugPrintf(SV_LOG_DEBUG,"Partition of %s Volume is : %s \n", strMountPoint.c_str(), m_storageInfo.strPartition.c_str());
								DebugPrintf(SV_LOG_DEBUG,"Type of %s Volume is : %s \n", strMountPoint.c_str(), m_storageInfo.strRootVolType.c_str());
								DebugPrintf(SV_LOG_DEBUG,"FileSystem of %s Volume is : %s \n", strMountPoint.c_str(), m_storageInfo.strFSType.c_str());
								DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);										
								return true;
							}
						}
					}
				}
			}
		}
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}


//This Function will collect all the required Network information from the CxAPI response xml file
//This will fill the m_NetworkInfo structure.
bool ChangeVmConfig::xGetNetworkInfo(xmlNodePtr xCur)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	m_NetworkInfo.clear();
	
	for(xmlNodePtr xChild = xCur->children; xChild != NULL; xChild = xChild->next)
	{
		if (!xmlStrcasecmp(xChild->name, (const xmlChar*)"ParameterGroup"))
		{
			NetworkInfo_t nwInfo;
			xmlChar* attr_value = xmlGetProp(xChild,(const xmlChar*)"Id");
            if (attr_value == NULL)
            {
                DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <MacAddress ParameterGroup Id> entry not found\n");
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	            return false;
            }
            string mac = std::string((char *)attr_value);
			if(false == xGetValueForProp(xChild, "MacAddress", nwInfo.strMacAddress))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the value for MacAddress\n");
				DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return false;
			}
			
			for(size_t i = 0; i < nwInfo.strMacAddress.length(); i++)
				nwInfo.strMacAddress[i] = tolower(nwInfo.strMacAddress[i]);
				
			DebugPrintf(SV_LOG_ERROR,"Selected Mac from XML : %s And MacAddress is : %s\n",mac.c_str(), nwInfo.strMacAddress.c_str());
			for(xmlNodePtr xChild1 = xChild->children; xChild1 != NULL; xChild1 = xChild1->next)
			{
				if (!xmlStrcasecmp(xChild1->name, (const xmlChar*)"ParameterGroup"))
				{
					attr_value = xmlGetProp(xChild1,(const xmlChar*)"Id");
					if (attr_value == NULL)
					{
						DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <IpAddress ParameterGroup Id> entry not found\n");
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						return false;
					}
					string ip = std::string((char *)attr_value);
					if(false == xGetValueForProp(xChild1, "Ip", nwInfo.strIpAddress))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to get the value for Ip\n");
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						return false;
					}
					if(false == xGetValueForProp(xChild1, "SubnetMask", nwInfo.strNetMask))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to get the value for SubnetMask\n");
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						return false;
					}
					if(false == xGetValueForProp(xChild1, "DeviceName", nwInfo.strNicName))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to get the value for DeviceName\n");
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						return false;
					}
					if(false == xGetValueForProp(xChild1, "GateWay", nwInfo.strGateWay))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to get the value for GateWay\n");
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						return false;
					}
					if(false == xGetValueForProp(xChild1, "DnsIp", nwInfo.strDnsServer))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to get the value for DnsIp\n");
						DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
						return false;
					}
					DebugPrintf(SV_LOG_ERROR,"IP from XML=%s  EthName=%s  IpAddress=%s  NetMask=%s\n",ip.c_str(),nwInfo.strNicName.c_str(), nwInfo.strIpAddress.c_str(), nwInfo.strNetMask.c_str());
					DebugPrintf(SV_LOG_ERROR,"GateWay=%s  DnsServerIP=%s\n", nwInfo.strGateWay.c_str(), nwInfo.strDnsServer.c_str());
					m_NetworkInfo.push_back(nwInfo);
				}				
			}
		}
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}


bool ChangeVmConfig::getNwInfoFromXmlFile(std::string HostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	std::string xmlFileName = m_VxInstallPath + "failover_data/" + HostName + NETWORK_DETAILS_FILE;
	DebugPrintf(SV_LOG_INFO,"XML File - %s\n", xmlFileName.c_str());
	
	if(false == checkIfFileExists(xmlFileName))
	{
		DebugPrintf(SV_LOG_INFO,"XML File %s not Found, Configure the Network Manually\n", xmlFileName.c_str());
		std::map<std::string, std::string>::iterator iter = m_mapOfVmNameToId.find(HostName);
		if(iter == m_mapOfVmNameToId.end())
		{
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		HostName = iter->second;
		xmlFileName = m_VxInstallPath + "failover_data/" + HostName + NETWORK_DETAILS_FILE;
		DebugPrintf(SV_LOG_INFO,"XML File - %s\n", xmlFileName.c_str());
	}

	if(false == parseStorageInfo(xmlFileName))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to parse the Storage information from file: %s\n", xmlFileName.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	if(false == parseNetworkInfo(xmlFileName))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to parse the Network information from file: %s\n", xmlFileName.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

bool ChangeVmConfig::parseStorageInfo(string xmlFileName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	xmlDocPtr xDoc = xmlParseFile(xmlFileName.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	//Found the doc. Now process it.
	xmlNodePtr xRoot = xmlDocGetRootElement(xDoc);
	if (xRoot == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML is empty. Cannot Proceed further.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
	if (xmlStrcmp(xRoot->name,(const xmlChar*)"Host")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Host> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
	
	xmlChar *attr_value_temp;
	attr_value_temp = xmlGetProp(xRoot,(const xmlChar*)"Os");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Host Os> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	m_strOsType = string((char *)attr_value_temp);
	
	attr_value_temp = xmlGetProp(xRoot,(const xmlChar*)"OsType");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Host OsType> entry not found\n");
		
		m_strOsFlavor = "RedHat";
	    //DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		//return false;
	}
	else
		m_strOsFlavor = string((char *)attr_value_temp);
	
	xmlNodePtr xChild = xGetChildNode(xRoot,(const xmlChar*)"Storage");
	if (xChild == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Storage> entry not found\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	
	attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"RootVolumeType");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Storage RootVolumeType> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
    m_storageInfo.strRootVolType = string((char *)attr_value_temp);
	
	attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"IsETCseperateFS");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Storage IsETCseperateFS> entry not found\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	m_storageInfo.strIsETCseperateFS = string((char *)attr_value_temp);
	
	attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"FileSystem");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Storage FileSystem> entry not found\n");
		m_storageInfo.strFSType = "ext3";
		//DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		//return false;
	}
	else
		m_storageInfo.strFSType = string((char *)attr_value_temp);
	
	if(m_storageInfo.strRootVolType == "LVM")
	{
		attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"VgName");
		if (attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Storage VgName> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		m_storageInfo.strVgName = string((char *)attr_value_temp);
		
		attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"LvName");
		if (attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Storage LvName> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		m_storageInfo.strLvName = string((char *)attr_value_temp);
		
		xChild = xGetChildNode(xChild,(const xmlChar*)"VG");
		if (xChild == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <VG> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		
		attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"Disks");
		if (attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <VG Disks> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		std::string strAllDisks = string((char *)attr_value_temp);
		tokeniseString(strAllDisks,m_storageInfo.listDiskName);
	}
	else
	{
		attr_value_temp = xmlGetProp(xChild,(const xmlChar*)"Partition");
		if (attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Storage Partition> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		m_storageInfo.strPartition = string((char *)attr_value_temp);
	}
	
	xmlFreeDoc(xDoc);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

bool ChangeVmConfig::parseNetworkInfo(string xmlFileName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	xmlDocPtr xDoc = xmlParseFile(xmlFileName.c_str());
	if (xDoc == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"XML document is not Parsed successfully.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

	//Found the doc. Now process it.
	xmlNodePtr xRoot = xmlDocGetRootElement(xDoc);
	if (xRoot == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML is empty. Cannot Proceed further.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
	if (xmlStrcmp(xRoot->name,(const xmlChar*)"Host")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}

	xmlNodePtr xChild = xGetChildNode(xRoot,(const xmlChar*)"Network");
	if (xChild == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Network> entry not found\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	xmlNodePtr xChild1 = xGetChildNode(xChild,(const xmlChar*)"Nic");
	if (xChild1 == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Nic> entry not found\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
		
	while(xChild1 != NULL)
	{
		NetworkInfo_t nwInfo;
		xmlChar *attr_value_temp;
		DebugPrintf(SV_LOG_INFO,"Got the Child node : <Nic> entry\n");
		
		attr_value_temp = xmlGetProp(xChild1,(const xmlChar*)"Name");
		if (attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Nic Name> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		nwInfo.strNicName = string((char *)attr_value_temp);
		
		attr_value_temp = xmlGetProp(xChild1,(const xmlChar*)"Macaddress");
		if (attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Nic Gateway> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		nwInfo.strMacAddress = string((char *)attr_value_temp);
		
		attr_value_temp = xmlGetProp(xChild1,(const xmlChar*)"Ipaddress");
		if (attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Nic Ipaddress> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		nwInfo.strIpAddress = string((char *)attr_value_temp);
		
		attr_value_temp = xmlGetProp(xChild1,(const xmlChar*)"Netmask");
		if (attr_value_temp == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <Nic Netmask> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		nwInfo.strNetMask = string((char *)attr_value_temp);
		
		m_NetworkInfo.push_back(nwInfo);
		xChild1 = xGetNextChildNode(xChild1, (const xmlChar*)"Nic");
	}
	xmlFreeDoc(xDoc);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

void ChangeVmConfig::tokeniseString(std::string strAllDisks, std::list<std::string>& listDisks, std::string strdel)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	char *pToken = strtok((char*)strAllDisks.c_str(), strdel.c_str());
	while(pToken)
	{
		listDisks.push_back(string(pToken));
		pToken = strtok(NULL, strdel.c_str());
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

bool ChangeVmConfig::processPartition(std::string hostName, std::string strPartition, std::string& strTargetDisk)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	map<string, map<string,string> >::iterator iterFind = m_mapOfVmToSrcAndTgtDisks.find(hostName);
	set<string> setPartitions;
	if(iterFind != m_mapOfVmToSrcAndTgtDisks.end()) //will always find VM as vecRecoveredVMs is subset of m_mapOfVmToSrcAndTgtDisks
	{
		map<string, string> mapSrcToTgtDisks = iterFind->second;
		map<string, string>::iterator iter = mapSrcToTgtDisks.begin();
		for(; iter != mapSrcToTgtDisks.end(); iter++)
		{
            std::string strSrcDiskName(iter->first);
			string strPartitionNum;
#ifndef WIN32
            // in case of Linux, the src disk is a persistent name which is retrieved from CS as DiskGuid. 
            // Get the real device name here
            std::map<std::string, std::string>::iterator guidIter = m_mapOfDiskGuidToDiskName.find(strSrcDiskName);
            if ( guidIter == m_mapOfDiskGuidToDiskName.end())
            {
				DebugPrintf(SV_LOG_ERROR,"for source disk could not find the disk name in guid map : %s\n", strSrcDiskName.c_str());
				continue;
            }
            strSrcDiskName = guidIter->second;
#endif
			try
			{
				DebugPrintf(SV_LOG_DEBUG,"Processing disk : %s for Partition : %s\n", strSrcDiskName.c_str(), strPartition.c_str());
                // BUGBUG: if strSrcDiskName is sda and strPartition is sdaa1 ...
				if(strPartition.find(strSrcDiskName) != string::npos)
				{
					strPartitionNum = strPartition.substr(strSrcDiskName.length(), strPartition.length());
                    // BUGBUG: if strPartition is sdap1 though 'sdap' is the disk name ...
					if(strPartitionNum.find("p") != string::npos)
					{
						strPartitionNum = strPartitionNum.substr(strPartitionNum.find("p")+1);
					}
				}
				else
				{
					DebugPrintf(SV_LOG_DEBUG,"Partition : %s is not availble in Disk: %s\n", strPartition.c_str(), strSrcDiskName.c_str());
				}
			}
			catch(const std::out_of_range& oor)
			{
				DebugPrintf(SV_LOG_ERROR,"Caught exception while proceesing the strings for partition : %s\n", oor.what());
				break;
			}
			catch(...)
			{
				DebugPrintf(SV_LOG_ERROR,"Caught exception while proceesing the strings for partition\n");
				break;
			}
			if(strPartition.find(strSrcDiskName) != string::npos)
			{
				setPartitions.clear();
				FindAllPartitions(iter->second, setPartitions);
				string strTargetDiskWithNum = iter->second + strPartitionNum;
				string strTargetDiskwithPartP = iter->second + string("p") + strPartitionNum;
				string strTargetDiskwithPartNum = iter->second + string("-part") + strPartitionNum;
				if(setPartitions.find(strTargetDiskWithNum) != setPartitions.end())
				{
					strTargetDisk = strTargetDiskWithNum;
					DebugPrintf(SV_LOG_DEBUG,"Found source respective target parititon : %s\n", strTargetDisk.c_str());
				}
				else if(setPartitions.find(strTargetDiskwithPartP) != setPartitions.end())
				{
					strTargetDisk = strTargetDiskwithPartP;
					DebugPrintf(SV_LOG_DEBUG,"Found source respective target parititon : %s\n", strTargetDisk.c_str());
				}
				else if(setPartitions.find(strTargetDiskwithPartNum) != setPartitions.end())
				{
					strTargetDisk = strTargetDiskwithPartNum;
					DebugPrintf(SV_LOG_DEBUG,"Found source respective target parititon : %s\n", strTargetDisk.c_str());
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Source respective root volume partition not found in the Rollbacked VM.\n",__FUNCTION__);
					DebugPrintf(SV_LOG_ERROR,"NetWork Configuration got Failed...Configure manually\n",__FUNCTION__);
					bResult = false;
				}
				break;
			}
		}
	}
	if(true == strTargetDisk.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Specified Parttion %s not found in MT for the VM: %s\n",strPartition.c_str(), hostName.c_str());
		bResult = false;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


bool ChangeVmConfig::FindAllPartitions(string strDevMapper, set<string>& setPartitions, bool bDevMapper)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;

	if((!m_bV2P || bDevMapper) && (m_strCloudEnv.compare("aws") != 0))
	{
		std::string strMapper = "/dev/mapper/";
		size_t pos = strDevMapper.find(strMapper);
		if(string::npos != pos)
			strDevMapper = strDevMapper.substr(pos+strMapper.length());
		
		stringstream results;
		string cmd = string("dmsetup ls | grep ") + strDevMapper + string(" | awk \'{ print $1 }\'");
		DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
		if (!executePipe(cmd, results))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to run the command for finding the partitions in : %s\n",strDevMapper.c_str());
			bResult = false;
		}
		else
		{
			while(!results.eof())
			{
				string strPartition;
				results >> strPartition;
				trim(strPartition);
				if(!strPartition.empty())
				{
					DebugPrintf(SV_LOG_DEBUG, "Got Partition: %s\n",strPartition.c_str());
					strPartition = string("/dev/mapper/") + strPartition;
					setPartitions.insert(strPartition);
				}
			}
		}
	}
	else
	{		
		stringstream results;
		string cmd = string("fdisk -l ") + strDevMapper + string(" | grep \"^") + strDevMapper + string("\" | awk \'{ print $1 }\'");
		DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
		if (!executePipe(cmd, results))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to run the command for finding the partitions in : %s\n",strDevMapper.c_str());
			bResult = false;
		}
		else
		{
			while(!results.eof())
			{
				string strPartition;
				results >> strPartition;
				trim(strPartition);
				if(!strPartition.empty())
				{
					DebugPrintf(SV_LOG_DEBUG, "Got Partition: %s\n",strPartition.c_str());
					setPartitions.insert(strPartition);
				}
			}
		}
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::activateVolGrp(string strVgName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	stringstream results;
	string cmd = string("vgchange -ay ") + strVgName + string(" 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
	DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
		
	string cmd1 = string("echo running command : \"") + cmd + string(" >>") + m_InMageCmdLogFile;
	if (!executePipe(cmd1, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
	}
		
	results.clear();
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
		bResult = false;
	}
	else
	{
		string result = results.str();
		trim(result);
		DebugPrintf(SV_LOG_DEBUG, "Successfully ran the cmd: %s Output: %s\n", cmd.c_str(), result.c_str());
		bResult = true;
	}
		
	cmd = string("vgmknodes ") + strVgName + string(" 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
	DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
		
	cmd1 = string("echo running command : \"") + cmd + string(" >>") + m_InMageCmdLogFile;
	if (!executePipe(cmd1, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
	}
	
	results.clear();
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
	}
	else
	{
		string result = results.str();
		trim(result);
		DebugPrintf(SV_LOG_DEBUG, "Successfully ran the cmd: %s Output: %s\n", cmd.c_str(), result.c_str());
	}
	sleep(2);
		
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::deActivateVolGrp(string strVgName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	stringstream results;
	string cmd = string("vgchange -an ") + strVgName + string(" 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
	DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
		
	string cmd1 = string("echo running command : \"") + cmd + string(" >>") + m_InMageCmdLogFile;
	if (!executePipe(cmd1, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
	}
		
	results.clear();
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
		bResult = false;
	}
	else
	{
		string result = results.str();
		trim(result);
		DebugPrintf(SV_LOG_DEBUG, "Successfully ran the cmd: %s, OutPut from the cmd: %s\n", cmd.c_str(), result.c_str());
		bResult = true;
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::activateLV(string& strLvName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	stringstream results;
	string cmd = string("lvchange -ay ") + strLvName + string(" 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
	DebugPrintf(SV_LOG_INFO,"cmd = : %s\n", cmd.c_str());
		
	string cmd1 = string("echo running command : \"") + cmd + string(" >>") + m_InMageCmdLogFile;
	if (!executePipe(cmd1, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
	}
		
	results.clear();
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
		bResult = false;
	}
	else
	{
		string result = results.str();
		trim(result);
		DebugPrintf(SV_LOG_INFO, "Successfully ran the cmd: %s Output: %s\n", cmd.c_str(), result.c_str());
		bResult = true;
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::deActivateLV(string& strLvName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	stringstream results;
	string cmd = string("lvchange -an ") + strLvName + string(" 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
	DebugPrintf(SV_LOG_INFO,"cmd = : %s\n", cmd.c_str());
		
	string cmd1 = string("echo running command : \"") + cmd + string(" >>") + m_InMageCmdLogFile;
	if (!executePipe(cmd1, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
	}
		
	results.clear();
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command: %s\n",cmd.c_str());
		bResult = false;
	}
	else
	{
		string result = results.str();
		trim(result);
		DebugPrintf(SV_LOG_INFO, "Successfully ran the cmd: %s Output: %s\n", cmd.c_str(), result.c_str());
		bResult = true;
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::createDirectory(string mnt)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;

	stringstream results;
	string cmd = "mkdir -p " + mnt;
	DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
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
			DebugPrintf(SV_LOG_ERROR, "Failed to create directory: %s got Error: %s\n", mnt.c_str(), commandresult.c_str());
			bResult = false;;
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Successfully created directory: %s\n", mnt.c_str());
			bResult = true;
		}
		commandresult.clear();
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::deleteDirectory(string dir)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;

	stringstream results;
	string cmd = "rmdir " + dir;
	DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to delete the directory : %s\n", dir.c_str());
        bResult = false;
    }
	else
	{
		string commandresult = results.str();
		trim(commandresult);
		if(!commandresult.empty())
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to delete the directory: %s got Error: %s\n", dir.c_str(), commandresult.c_str());
			bResult = false;
        }
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Successfully deleted the directory: %s\n", dir.c_str());
			bResult = true;
        }
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::copyFileToTarget(string srcFile, string destFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;

	stringstream results;
	string cmd = "cp -p " + srcFile + " " + destFile;
	DebugPrintf(SV_LOG_INFO,"cmd = : %s\n", cmd.c_str());
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to copy the file %s to the directory: %s\n", srcFile.c_str(), destFile.c_str());
        bResult = false;
    }
	else
	{
		string commandresult = results.str();
		trim(commandresult);
		if(!commandresult.empty())
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to copy the File %s to %s, Got Error: %s\n",srcFile.c_str(), destFile.c_str(), commandresult.c_str());
			bResult = false;
        }
		else
		{
			DebugPrintf(SV_LOG_INFO, "Successfully copied the File %s to %s\n",srcFile.c_str(), destFile.c_str());
			bResult = true;
        }
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::moveFileToDir(string file, string dir)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;

	stringstream results;
	string cmd = "mv " + file + " " + dir;
	DebugPrintf(SV_LOG_INFO,"cmd = : %s\n", cmd.c_str());
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to move the file %s to the directory: %s\n", file.c_str(), dir.c_str());
        bResult = false;
    }
	else
	{
		string commandresult = results.str();
		trim(commandresult);
		if(!commandresult.empty())
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to move the File %s to the directory: %s got Error: %s\n",file.c_str(), dir.c_str(), commandresult.c_str());
			bResult = false;
        }
		else
		{
			DebugPrintf(SV_LOG_INFO, "Successfully moved the File %s to the directory: %s\n",file.c_str(), dir.c_str());
			bResult = true;
        }
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::checkFSofDevice(string strTgtPartitionOrLV)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	string cmd = string("fsck -y -t ") + m_storageInfo.strFSType + string(" ") + strTgtPartitionOrLV + string(" 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
	DebugPrintf(SV_LOG_INFO,"The Command to check filesystem is: %s\n",cmd.c_str());
	stringstream results;
	
	string cmd1 = string("echo running command : \"") + cmd + string("\" on host : ") + m_strHostName + string(" >>") + m_InMageCmdLogFile;
	if (!executePipe(cmd1, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display the command on file\n");
	}
	
	results.clear();
	if (!executePipe(cmd, results))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to check the filesystem: %s\n", cmd.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO, "Successfully checked the filesystem: %s\n", strTgtPartitionOrLV.c_str());
        bResult = true;
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::mountFstab(string strRootMnt, string strTgtPartition)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
    bool bResult = true;
	stringstream results;
	
	std::string cmd = m_VxInstallPath + std::string("scripts/vCon/") 
		    + std::string("MountFS.sh ") + strRootMnt + std::string(" ") 
                    + strTgtPartition + string(" 1>>") + m_InMageCmdLogFile 
                    + string(" 2>>") + m_InMageCmdLogFile;
	
    DebugPrintf(SV_LOG_INFO,"The Command to mount fstab: %s\n",cmd.c_str());
	
    if (!executePipe(cmd, results))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to mount fstab: %s\n",
                    strTgtPartition.c_str());
        bResult = false;
    }
    else
    {
        DebugPrintf(SV_LOG_INFO, "Successfully mount fstab: %s\n", 
                    strTgtPartition.c_str());
        bResult = true;
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bResult;
}

bool ChangeVmConfig::mountPartition(string strRootMnt, string strTgtPartition, string mnt)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	stringstream results;

    /* Mount all non btrfs fs and only root btrfs fs */
    if (m_storageInfo.strFSType.compare("btrfs") != 0 || // Non brfs
        strRootMnt.compare(mnt) == 0) {                 // btrfs based src root
    	string cmd = string("mount -t ") + m_storageInfo.strFSType + string(" ") + strTgtPartition + string(" ") + mnt + string(" 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
	    string cmd1 = string("echo running command : \"") + cmd + string("\" on host : ") + m_strHostName + string(" >>") + m_InMageCmdLogFile;	
	
    	if (!executePipe(cmd1, results))
	    {
		    DebugPrintf(SV_LOG_ERROR, "Failed to run the command to display hostname while mounting the Partition: %s\n", strTgtPartition.c_str());
    	}
	
	    results.clear();
    	DebugPrintf(SV_LOG_DEBUG,"The Command to Mount is: %s\n",cmd.c_str());
	    if (!executePipe(cmd, results))
    	{
	    	DebugPrintf(SV_LOG_ERROR, "Failed to run the command to mount the Partition: %s\n", cmd.c_str());
		    bResult = false;
    	}
	    else
    	{
	    	string commandresult = results.str();
		    if(!commandresult.empty())
    		{
	    		DebugPrintf(SV_LOG_ERROR, "Failed to mount the Partition: %s got Error: %s\n", strTgtPartition.c_str(), commandresult.c_str());
		    	bResult = false;
    		}
	    	else
		    {
                DebugPrintf(SV_LOG_DEBUG, "Successfully mounted the Partition: %s\n", strTgtPartition.c_str());
                bResult = true;
		    }
	    }
    }

    /* For btrfs, mount the subvolumes using fstab file */ 
    if (m_storageInfo.strFSType.compare("btrfs") == 0) 
        bResult = mountFstab(strRootMnt, strTgtPartition);
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::unmountPartition(string strTgtPartition)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;

	DebugPrintf(SV_LOG_INFO,"Performing unmount operation on %s ...\n", strTgtPartition.c_str());

    std::string error;	
	std::string command = SV_UMOUNT + std::string(" -R ");
	InmCommand unmount(command + strTgtPartition);
	std::string pathvar = "PATH=";
	pathvar += GetCommandPath(command);
	unmount.PutEnv(pathvar);
    int iTry = 0;

    do
    {
        bResult = true;
	    InmCommand::statusType status = unmount.Run();

	    if (status != InmCommand::completed) 
		{
	        DebugPrintf(SV_LOG_ERROR,"Failed to unmount - %s\n", strTgtPartition.c_str());
		    error = "Failed to unmount ";
		    error += strTgtPartition;
	        if(!unmount.StdErr().empty())
		    {
        	    error += unmount.StdErr();
	        }
        	bResult=false;
	    }

       	if(unmount.ExitCode())
        {
	        error = "Failed to unmount ";
	        error += strTgtPartition;
        	std::ostringstream msg;
	        msg << " Exit Code = " << unmount.ExitCode()<< std::endl;
	        if(!unmount.StdOut().empty())
	        {
	            msg << "Output = " << unmount.StdOut() << std::endl;
	        }
	        if(!unmount.StdErr().empty())
	        {
        	    msg << "Error = " << unmount.StdErr() << std::endl;
	        }
        	error += msg.str();

	        DebugPrintf(SV_LOG_ERROR,"Failed to unmount - %s\n : %s\n", strTgtPartition.c_str(), error.c_str());
	        bResult=false;
	    }
            
        if(bResult)
        {
            DebugPrintf(SV_LOG_DEBUG, "Successfully unmounted : %s\n", strTgtPartition.c_str());
            break;
        }
            
        iTry++;

        //This block is temporary
        //--------------------------------------------
        set<string> stPids;
        ListPIDsHoldsMntPath(strTgtPartition, stPids);
        //--------------------------------------------

        sleep(10);

    }while(iTry < 3);

    if(3 == iTry)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to unmount : %s in all 3 tries.\n", strTgtPartition.c_str());
        bResult = false;

        //This block is temporary
        //--------------------------------------------
        set<string> stPids;
        ListAllPIDs(stPids);
        //--------------------------------------------
    }

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bResult;
}


bool ChangeVmConfig::FsTabConfigForAWS(string strMntDirectory)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult;
	
	string strFsTabFile, strFsTabFile_old;
	
	strFsTabFile = strMntDirectory + "/etc/fstab";
	strFsTabFile_old = strMntDirectory + "/etc/fstab_old";
	
	string strFsTabFileTemp = strFsTabFile + ".temp";
	ifstream inFile(strFsTabFile.c_str());
	ofstream outFile(strFsTabFileTemp.c_str());
	if(!inFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open file %s\n ",strFsTabFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	string strLineRead;
	string strDevSearch = "/dev/sd";
	DebugPrintf(SV_LOG_DEBUG,"Replacing device name: %s --> /dev/xvd\n ", strDevSearch.c_str());
	while(std::getline(inFile,strLineRead))
	{
		bool bLine = true;
		size_t pos;
		pos = strLineRead.find(strDevSearch);
		while(pos != string::npos)
		{
			if(bLine)
				DebugPrintf(SV_LOG_DEBUG,"Replacing in line %s\n ",strLineRead.c_str());
			bLine = false;
			strLineRead.replace(pos, strDevSearch.length(), "/dev/xvd");
			pos = strLineRead.find(strDevSearch, pos+strDevSearch.length());
		}
		outFile << strLineRead << endl;
	}
	
	inFile.close();
	outFile.close();
	
	int result = rename(strFsTabFile.c_str() , strFsTabFile_old.c_str());
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strFsTabFile.c_str(), strFsTabFile_old.c_str());	
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,"Successfully renamed the file %s into %s \n ",strFsTabFile.c_str(), strFsTabFile_old.c_str());
	}
	
	result = rename(strFsTabFileTemp.c_str() , strFsTabFile.c_str());
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strFsTabFileTemp.c_str(), strFsTabFile.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,"Successfully renamed the file %s into %s \n ",strFsTabFileTemp.c_str(), strFsTabFile.c_str());
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


bool ChangeVmConfig::NetrworkConfigForAWS(string strMntDirectory)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult;
	
	string strConfigPath, strConfigPath_old;
	
	if(m_strOsFlavor == "RedHat" || m_strOsFlavor == "Ubuntu")
	{
		strConfigPath = strMntDirectory + "/etc/sysconfig/network-scripts/ifcfg-eth0";
		strConfigPath_old = strMntDirectory + "/etc/sysconfig/network-scripts/ifcfg-eth0.old";
	}
	else if(m_strOsFlavor == "Suse")
	{
		strConfigPath = strMntDirectory + "/etc/sysconfig/network/ifcfg-eth0";
		strConfigPath_old = strMntDirectory + "/etc/sysconfig/network/ifcfg-eth0.old";
	}	
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Required OS is not found to configure the Network Changes\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	
	int result = rename(strConfigPath.c_str() , strConfigPath_old.c_str());
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strConfigPath.c_str(), strConfigPath_old.c_str());	
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,"Successfully renamed the file %s into %s \n ",strConfigPath.c_str(), strConfigPath_old.c_str());
	}
	
	ofstream outFile(strConfigPath.c_str());
	
	if(m_strOsFlavor == "RedHat" || m_strOsFlavor == "Ubuntu")
	{
		outFile << "DEVICE=eth0" << endl;
		outFile << "BOOTPROTO=dhcp" << endl;
		outFile << "ONBOOT=yes" << endl;
	}
	else if(m_strOsFlavor == "Suse")
	{
		outFile << "BOOTPROTO=\'dhcp\'" << endl;
		outFile << "STARTMODE=\'auto\'" << endl;
	}
	outFile.close();
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


bool ChangeVmConfig::RemoveInVolFltFiles(string strMntDirectory)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult;
	
	stringstream results;
	string cmd = "rm -rf " + strMntDirectory + "/etc/vxagent/involflt/_*";
	cmd += " 2> /dev/null ";
	DebugPrintf(SV_LOG_DEBUG,"Command: %s\n",cmd.c_str());
	if (!executePipe(cmd, results))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to run the command to list out the files: %s\n", cmd.c_str());
        bResult = false;
    }
	else
    {
		DebugPrintf(SV_LOG_DEBUG, "Files Got removed successfully...\n");
        string commandresult;
		while(!results.eof())
		{
			results >> commandresult;
			trim(commandresult);
			if(!commandresult.empty())
			{
				DebugPrintf(SV_LOG_DEBUG, "%s\n", commandresult.c_str());
			}
		}
        bResult = true;
    }
			
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


//In Case of Dr-Drill we need to stop the InMage services in the recovered snapshot VM.
//If we won't stop the services the duplicate entries will get create in the CX databse with the hostname.
void ChangeVmConfig::MakeInMageSVCStop(string strMntDirectory)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	string strSvcPath = strMntDirectory + string("/");
	string strSvcVxPath;
	string strSvcFxPath;
	if("yes" == m_storageInfo.strIsETCseperateFS)
	{
		strSvcVxPath = strSvcPath + "init.d/vxagent";
		strSvcFxPath = strSvcPath + "init.d/svagent";
	}
	else
	{
		strSvcVxPath = strSvcPath + "etc/init.d/vxagent";
		strSvcFxPath = strSvcPath + "etc/init.d/svagent";
	}
	
	string strTempFile;
	int result;
	
	//Renaming the vxagent file name, So that during boot up time the original vxagent file won't found to start the vxagent service
	if(checkIfFileExists(strSvcVxPath))
	{
		strTempFile = strSvcVxPath + "_old";
		result = rename( strSvcVxPath.c_str() , strTempFile.c_str());
		if(result != 0)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strSvcVxPath.c_str(), strTempFile.c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Successfully Renamed the file %s into %s\n", strSvcVxPath.c_str(), strTempFile.c_str());
		}
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,"File %s not found for renaming it to different name\n", strSvcVxPath.c_str());
	}
	
	//Renaming the svagent file name, So that during boot up time the original svagent file won't found to start the fxagent service
	if(checkIfFileExists(strSvcFxPath))
	{
		strTempFile = strSvcFxPath + "_old";
		result = rename( strSvcFxPath.c_str() , strTempFile.c_str());
		if(result != 0)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strSvcFxPath.c_str(), strTempFile.c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Successfully Renamed the file %s into %s\n", strSvcFxPath.c_str(), strTempFile.c_str());
		}
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG,"File %s not found for renaming it to different name\n", strSvcFxPath.c_str());
	}	
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

//Will call the respective function depends on the Linux flavor to set the network configuration
bool ChangeVmConfig::setNetworkConfig(string hostName, string strMntDirectory)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	
	if(m_strOsFlavor == "RedHat" || m_strOsFlavor == "Ubuntu")
	{
		if(false == setNetworkConfigRedHat(hostName, strMntDirectory))
		{
			DebugPrintf(SV_LOG_INFO,"Failed to set the network configuration for the RedHat machine\n");
			bResult = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully set the network configuration for %s\n", hostName.c_str());
		}
	}
	else if(m_strOsFlavor == "Suse")
	{
		if(false == setNetworkConfigSuse(hostName, strMntDirectory))
		{
			DebugPrintf(SV_LOG_INFO,"Failed to set the network configuration for the Suse machine\n");
			bResult = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully set the network configuration for %s\n", hostName.c_str());
		}
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Required OS is not found to configure the Network Changes\n");
		bResult = false;
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::setNetworkConfigRedHat(string hostName, string strMntDirectory)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	bool bRhel6 = false;
	bool bOel = false;
	bool bUbuntu = false;
	m_mapOfOldAndNewMacAdd.clear();
	set<string> setDnsServer;
	NetworkInfoMap_t NwMapFromXml;
	string strHostIp;
	
	if((m_strOsType.find("Red Hat Enterprise Linux Server release 6") != string::npos) 
		|| (m_strOsType.find("CentOS release 6") != string::npos) 
		|| (m_strOsType.find("CentOS Linux release 6") != string::npos)
		|| (m_strOsType.find("Enterprise Linux Enterprise Linux Server release 6") != string::npos)
		|| (m_strOsType.find("Oracle Linux Server release 6") != string::npos))
	{
		bRhel6 = true;
	}

	if((m_strOsType.find("Enterprise Linux Enterprise Linux Server release") != string::npos)
		|| ((m_strOsType.find("Oracle Linux Server release") != string::npos)))
	{
		bOel = true;
	}

	if(m_strOsFlavor == "Ubuntu")
	{
		bUbuntu = true;
	}
	
	if(false == xGetNWInfoFromXML(NwMapFromXml, hostName))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get Network settings from Recovery XML file for VM - %s\n", hostName.c_str());
		bResult = false;
    }
	string strNwDirectory = strMntDirectory + string("/");
	string strMacAddDir;
	string strDnsFilePath;
	string strNmFile;
	if("yes" == m_storageInfo.strIsETCseperateFS)
	{
		strDnsFilePath = strNwDirectory;
		strMacAddDir = strNwDirectory + "udev/rules.d/";
		if(!bUbuntu)
		{
			strNmFile = strNwDirectory + "vxagent/vcon/";
			strNwDirectory = strNwDirectory + "sysconfig/network-scripts/";
		}
	}
	else
	{
		strDnsFilePath = strNwDirectory + "etc/";
		strMacAddDir = strNwDirectory + "etc/udev/rules.d/";
		if(!bUbuntu)
		{
			strNmFile = strNwDirectory + "etc/vxagent/vcon/";
			strNwDirectory = strNwDirectory + "etc/sysconfig/network-scripts/";
		}
	}
	
	string nwDir = strNwDirectory + "inmage_nw";
	if(bUbuntu)
	{
		nwDir = strDnsFilePath + string("/network/inmage_nw");
	}
	if(false == createDirectory(nwDir))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to create the directory %s.\n", nwDir.c_str());
	}
	
	string strNwConfigFile;
	std::list<NetworkInfo_t>::iterator iter;
	
	//m_NetworkInfo == filled from the source cx api xml file
	//NwMapFromXml == filled from the recovery\snapshot.xml file
	//In this  for loop we will refill the m_NetworkInfo with new ip values which we got in NwMapFromXml
	for(iter = m_NetworkInfo.begin(); iter != m_NetworkInfo.end(); iter++)
	{
		bool bOldIpExist = false;
		DebugPrintf(SV_LOG_INFO,"Looking for old Mac address %s of nic %s from xml file\n",iter->strMacAddress.c_str(), iter->strNicName.c_str());
		NetworkInfoMapIter_t nwIter;
		nwIter = NwMapFromXml.find(iter->strMacAddress);
		if(nwIter == NwMapFromXml.end())
		{
			DebugPrintf(SV_LOG_INFO,"Mac address %s of nic %s not found from xml file\n",iter->strMacAddress.c_str(), iter->strNicName.c_str());
			continue;
		}
		
		map<string, IpInfo> mapOfOldIpToIpInfo;
		map<string, IpInfo>::iterator iterIP;
		
		if(!iter->strIpAddress.empty())
			bOldIpExist = true;
		
		if(bOldIpExist)
		{
			if(nwIter->second.EnableDHCP != 1)
			{
				if (false == createMapOfOldIPToIPInfo(nwIter->second.OldIPAddress, nwIter->second.IPAddress, nwIter->second.SubnetMask, 
														nwIter->second.DefaultGateway, nwIter->second.NameServer, mapOfOldIpToIpInfo))
				{
					DebugPrintf(SV_LOG_INFO,"Given IP addresses are not in proper order for old macaddress\n",nwIter->second.MacAddress.c_str());
					continue;
				}
				
				iterIP = mapOfOldIpToIpInfo.find(iter->strIpAddress);
				
				if(iterIP == mapOfOldIpToIpInfo.end())
				{
					DebugPrintf(SV_LOG_INFO,"Old IP address (or) the new ip list didn't find for old macaddress %s\n",nwIter->second.MacAddress.c_str());
					continue;
				}
			}
		}
		if(nwIter->second.EnableDHCP == 1)
		{
			DebugPrintf(SV_LOG_INFO,"DHCP seleted\n");
			iter->strIsDhcp = "dhcp";
			if(!nwIter->second.MacAddress.empty())
			{
				m_mapOfOldAndNewMacAdd.insert(make_pair(iter->strMacAddress, nwIter->second.MacAddress));
				iter->strMacAddress = nwIter->second.MacAddress;
			}
			else
				m_mapOfOldAndNewMacAdd.insert(make_pair(iter->strMacAddress, iter->strMacAddress));
			iter->strIpAddress = "";
			iter->strNetMask = "";
			iter->strGateWay = "";
			iter->strDnsServer = "";
		}
		else
		{
			if(!nwIter->second.MacAddress.empty())
			{
				m_mapOfOldAndNewMacAdd.insert(make_pair(iter->strMacAddress, nwIter->second.MacAddress));
				iter->strMacAddress = nwIter->second.MacAddress;
			}
			else
				m_mapOfOldAndNewMacAdd.insert(make_pair(iter->strMacAddress, iter->strMacAddress));
			if(!nwIter->second.IPAddress.empty())
			{
				if(bOldIpExist)
				{
					iter->strIpAddress = iterIP->second.IPAddress;
					strHostIp = iterIP->second.IPAddress;
				}
				else
				{
					iter->strIpAddress = nwIter->second.IPAddress;
					strHostIp = nwIter->second.IPAddress;
				}
			}
			if(!nwIter->second.SubnetMask.empty())
			{
				if(bOldIpExist)
					iter->strNetMask = iterIP->second.SubnetMask;
				else
					iter->strNetMask = nwIter->second.SubnetMask;
			}
			if(!nwIter->second.DefaultGateway.empty())
			{
				if(bOldIpExist)
					iter->strGateWay = iterIP->second.DefaultGateway;
				else
					iter->strGateWay = nwIter->second.DefaultGateway;
			}
			if(!nwIter->second.NameServer.empty())
			{
				if(bOldIpExist)
					iter->strDnsServer = iterIP->second.DnsServer;
				else
					iter->strDnsServer = nwIter->second.NameServer;

				std::string Separator = std::string(",");
				size_t pos = 0;

				size_t found = nwIter->second.NameServer.find(Separator, pos);
				if(found == std::string::npos)
				{
					DebugPrintf(SV_LOG_DEBUG,"Only single DNS ip provided for this Nic.\n");
					setDnsServer.insert(nwIter->second.NameServer);
				}
				else
				{
					while(found != std::string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",nwIter->second.NameServer.substr(pos,found-pos).c_str());
						setDnsServer.insert(nwIter->second.NameServer.substr(pos,found-pos));
						pos = found + 1; 
						found = nwIter->second.NameServer.find(Separator, pos);            
					}
					setDnsServer.insert(nwIter->second.NameServer.substr(pos));
				}
			}
			iter->strIsDhcp = "static";
		}
	}
	
	if(!bUbuntu)
	{
		//In this for loop we are processing the newly filled m_NetworkInfo map to update the new ips.
		for(iter = m_NetworkInfo.begin(); iter != m_NetworkInfo.end(); iter++)
		{
			string strBroadCastAdd;
			string strNetworkAdd;
			if(false == getBroadCastandNWAddr(iter->strIpAddress, iter->strNetMask, strBroadCastAdd, strNetworkAdd))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed while calculating the Broadcast and Network address for IP: %s\n",iter->strIpAddress.c_str());
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Successfully got the Broadcast address: %s and Network address: %s\n",strBroadCastAdd.c_str(), strNetworkAdd.c_str());
			}
			strNwConfigFile = strNwDirectory + "ifcfg-" + iter->strNicName;
			if(false == checkIfFileExists(strNwConfigFile))
			{
				DebugPrintf(SV_LOG_ERROR,"File Not Found %s\n",strNwConfigFile.c_str());
				strNwConfigFile = strNwDirectory + "ifcfg-lo";
				if(false == checkIfFileExists(strNwConfigFile))
				{
					DebugPrintf(SV_LOG_ERROR,"Unble to Configure Network for device %s of machine %s\n",iter->strNicName.c_str(), hostName.c_str());
					bResult  = false;
				}
				else
				{
					if(iter->strNicName.find(":") != string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"Ethernet file does not exist for NIC : [%s], Could be a Virtual IP so ignoring network file creation for it.\n",iter->strNicName.c_str());
					}
					else
					{
						strNwConfigFile = strNwDirectory + "ifcfg-" + iter->strNicName;
						ofstream outFile(strNwConfigFile.c_str());
						string strLineRead = "#InMage Modified for host : ";
						outFile << strLineRead << hostName << endl;
						DebugPrintf(SV_LOG_INFO,"Device : %s\n", iter->strNicName.c_str());
						outFile << "DEVICE=" << iter->strNicName << endl;
						DebugPrintf(SV_LOG_INFO,"BootProto : %s\n", iter->strIsDhcp.c_str());
						outFile << "BOOTPROTO=" << iter->strIsDhcp << endl;
						DebugPrintf(SV_LOG_INFO,"OnBoot : %s\n", iter->strIsDhcp.c_str());
						outFile << "ONBOOT=yes" << endl;
						DebugPrintf(SV_LOG_INFO,"IpAddress : %s\n", iter->strIpAddress.c_str());
						outFile << "IPADDR=" << iter->strIpAddress << endl;
						DebugPrintf(SV_LOG_INFO,"NetMask : %s\n", iter->strNetMask.c_str());
						outFile << "NETMASK=" << iter->strNetMask << endl;
						if(false == iter->strGateWay.empty())
						{
							DebugPrintf(SV_LOG_INFO,"GateWay : %s\n", iter->strGateWay.c_str());
							outFile << "GATEWAY=" << iter->strGateWay << endl;
						}
						if(bRhel6)
						{
							DebugPrintf(SV_LOG_INFO,"DNS Server : %s\n", iter->strDnsServer.c_str());
							outFile << "DNS1=" << iter->strDnsServer << endl;
						}
						if(bRhel6 || bOel)
						{					
							DebugPrintf(SV_LOG_INFO,"MacAddress : %s\n", iter->strMacAddress.c_str());
							outFile << "HWADDR=" << iter->strMacAddress << endl;
						}
						DebugPrintf(SV_LOG_INFO,"Broadcast Address : %s\n", strBroadCastAdd.c_str());
						outFile << "BROADCAST=" << strBroadCastAdd << endl;
						DebugPrintf(SV_LOG_INFO,"Network Address : %s\n", strNetworkAdd.c_str());
						outFile << "NETWORK=" << strNetworkAdd << endl;
						DebugPrintf(SV_LOG_INFO,"Peer Dns : no\n");
						outFile << "PEERDNS=no" << endl;
						outFile.close();

						AssignFilePermission( strNwDirectory + string("ifcfg-lo"), strNwConfigFile);
					}
				}
			}
			else
			{
				std::string strNwConfigFileTemp = strNwConfigFile + ".temp";
				ifstream inFile(strNwConfigFile.c_str());
				ofstream outFile(strNwConfigFileTemp.c_str());
				if(!inFile.is_open())
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to open file %s\n ",strNwConfigFile.c_str());
					continue;
				}
				string strLineRead = "#InMage Modified for host : ";
				outFile << strLineRead << hostName << endl;
				strLineRead.clear();
				bool bDevice = false;
				bool bBoot = false;
				bool bIpAdd = false;
				bool bNetMask = false;
				bool bGateWay = false;
				bool bHwAddress = false;
				bool bDns = false;
				bool bOnBoot = false;
				while(std::getline(inFile,strLineRead))
				{
					if(strLineRead.find("DEVICE") != string::npos)
					{
						bDevice = true;
						DebugPrintf(SV_LOG_INFO,"Device : %s\n", iter->strNicName.c_str());
						outFile << "DEVICE=" << iter->strNicName << endl; 
					}
					else if(strLineRead.find("BOOTPROTO") != string::npos)
					{
						bBoot = true;
						DebugPrintf(SV_LOG_INFO,"BootProto : %s\n", iter->strIsDhcp.c_str());
						outFile << "BOOTPROTO=" << iter->strIsDhcp << endl;
					}
					else if(strLineRead.find("IPADDR") != string::npos)
					{
						bIpAdd = true;
						DebugPrintf(SV_LOG_INFO,"IpAddress : %s\n", iter->strIpAddress.c_str());
						outFile << "IPADDR=" << iter->strIpAddress << endl;
					}
					else if(strLineRead.find("NETMASK") != string::npos)
					{
						bNetMask = true;
						DebugPrintf(SV_LOG_INFO,"NetMask : %s\n", iter->strNetMask.c_str());
						outFile << "NETMASK=" << iter->strNetMask << endl;
					}
					else if(strLineRead.find("GATEWAY") != string::npos)
					{
						bGateWay = true;
						if(false == iter->strGateWay.empty())
						{
							DebugPrintf(SV_LOG_INFO,"GateWay : %s\n", iter->strGateWay.c_str());
							outFile << "GATEWAY=" << iter->strGateWay << endl;
						}
						else
						{
							outFile << strLineRead << endl;
						}
					}
					else if(strLineRead.find("HWADDR") != string::npos)
					{
						bHwAddress = true;
						if(bRhel6 || bOel)
						{
							DebugPrintf(SV_LOG_INFO,"MacAddress : %s\n", iter->strMacAddress.c_str());
							outFile << "HWADDR=" << iter->strMacAddress << endl;
						}
						else
						{
							DebugPrintf(SV_LOG_INFO,"Found MacAddress, skipping the updation of HWADDRESS\n");
						}
					}
					else if(strLineRead.find("DNS") != string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"Found DNS Server, skipping it.\n");
					}
					else if(strLineRead.find("ONBOOT") != string::npos)
					{
						bOnBoot = true;
						DebugPrintf(SV_LOG_INFO,"ONBOOT : yes\n");
						outFile << "ONBOOT=yes" << endl; 
					}
					else if(strLineRead.find("PEERDNS") != string::npos)
					{
						bDns = true;
						DebugPrintf(SV_LOG_INFO,"PEERDNS : no\n");
						outFile << "PEERDNS=no" << endl; 
					}
					else if(strLineRead.find("BROADCAST") != string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"Broadcast Address : %s\n", strBroadCastAdd.c_str());
						outFile << "BROADCAST=" << strBroadCastAdd << endl;
					}
					else if(strLineRead.find("NETWORK") != string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"Network Address : %s\n", strNetworkAdd.c_str());
						outFile << "NETWORK=" << strNetworkAdd << endl;
					}
					else if(strLineRead.find("NM_CONTROLLED") != string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"Found NM_CONTROLLED, commenting it.\n");
						outFile << "#" << strLineRead << endl;
					}
					else if(strLineRead.find("UUID") != string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"Found UUID, commenting it.\n");
						outFile << "#" << strLineRead << endl;
					}
					else if(strLineRead.find("PREFIX") != string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"Found PREFIX, commenting it.\n");
						outFile << "#" << strLineRead << endl;
					}
					else
					{
						outFile << strLineRead << endl;
					}
				}
				if(!bDevice)
				{
					DebugPrintf(SV_LOG_INFO,"Device = %s\n", iter->strNicName.c_str());
					outFile << "DEVICE=" << iter->strNicName << endl;
				}
				if(bRhel6)
				{
					DebugPrintf(SV_LOG_INFO,"Dns Server = %s\n", iter->strDnsServer.c_str());
					outFile << "DNS1=" << iter->strDnsServer << endl;
				}
				if(!bHwAddress)
				{
					if(bRhel6 || bOel)
					{
						DebugPrintf(SV_LOG_INFO,"MacAddress = %s\n", iter->strMacAddress.c_str());
						outFile << "HWADDR=" << iter->strMacAddress << endl;
					}
					else
					{
						DebugPrintf(SV_LOG_INFO,"Found MacAddress, skipping the updation of HWADDRESS\n");
					}
				}
				if(!bBoot)
				{
					DebugPrintf(SV_LOG_INFO,"BootProto = %s\n", iter->strIsDhcp.c_str());
					outFile << "BOOTPROTO=" << iter->strIsDhcp << endl;
				}
				if(!bIpAdd)
				{
					DebugPrintf(SV_LOG_INFO,"IpAddress = %s\n", iter->strIpAddress.c_str());
					outFile << "IPADDR=" << iter->strIpAddress << endl;
				}
				if(!bNetMask)
				{
					DebugPrintf(SV_LOG_INFO,"NetMask = %s\n", iter->strNetMask.c_str());
					outFile << "NETMASK=" << iter->strNetMask << endl;
				}	
				if(!bGateWay)
				{
					DebugPrintf(SV_LOG_INFO,"GateWay = %s\n", iter->strGateWay.c_str());
					outFile << "GATEWAY=" << iter->strGateWay << endl;
				}
				if(!bOnBoot)
				{
					DebugPrintf(SV_LOG_INFO,"ONBOOT = yes\n");
					outFile << "ONBOOT=yes" << endl;
				}
				if(!bDns)
				{
					DebugPrintf(SV_LOG_INFO,"PEERDNS = no\n");
					outFile << "PEERDNS=no" << endl;
				}
				inFile.close();
				outFile.close();
				int result;
				string strNwConfigFileOld = strNwConfigFile + ".old";
				result = rename( strNwConfigFile.c_str() , strNwConfigFileOld.c_str() );
				if(result != 0)
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strNwConfigFile.c_str(), strNwConfigFileOld.c_str());	
				}
				else
				{
					DebugPrintf(SV_LOG_INFO,"Successfully renamed the file %s into %s \n ",strNwConfigFile.c_str(), strNwConfigFileOld.c_str());
				}
				
				result = rename( strNwConfigFileTemp.c_str() , strNwConfigFile.c_str());
				if(result != 0)
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strNwConfigFileTemp.c_str(), strNwConfigFile.c_str());
				}
				else
				{
					DebugPrintf(SV_LOG_INFO,"Successfully renamed the file %s into %s \n ",strNwConfigFileTemp.c_str(), strNwConfigFile.c_str());
					DebugPrintf(SV_LOG_INFO,"Successfully edited the file: %s \n", strNwConfigFile.c_str());
				}

				AssignFilePermission(strNwConfigFileOld, strNwConfigFile);
				moveFileToDir(strNwConfigFileOld, nwDir);
				
				if(false == listFilesInDir(strNwDirectory, "ifcfg"))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to List out all the \"ifcfg\" files in directory \"/etc/sysconfig/network-scripts/\" \n ");
				}
			}
		}
	}
	else
	{
		strNwConfigFile = strDnsFilePath + string("/network/interfaces");
		if(false == checkIfFileExists(strNwConfigFile))
		{
			moveFileToDir(strNwConfigFile, nwDir);
		}
		ofstream outFile(strNwConfigFile.c_str());
		string strLineRead = "# InMage Created for host : ";
		outFile << strLineRead << hostName << endl;
		outFile << "# This file describes the network interfaces available on your system" << endl;
		outFile << "# and how to activate them. For more information, see interfaces(5)." << endl << endl;
		outFile << "# The loopback network interface" << endl;
		outFile << "auto lo" << endl;
		outFile << "iface lo inet loopback" << endl << endl;
		outFile << "# The primary network interface" << endl;

		//In this for loop we are processing the newly filled m_NetworkInfo map to update the new ips.
		for(iter = m_NetworkInfo.begin(); iter != m_NetworkInfo.end(); iter++)
		{
			DebugPrintf(SV_LOG_INFO,"Updating Network config for ethernet = %s\n", iter->strNicName.c_str());
			outFile << "auto " << iter->strNicName << endl;
			if(iter->strIsDhcp.compare("dhcp") == 0)
			{
				DebugPrintf(SV_LOG_INFO,"DHCP selected for ethernet = %s\n", iter->strNicName.c_str());
				outFile << "iface " << iter->strNicName << " inet dhcp" << endl << endl;
			}
			else
			{
				outFile << "iface " << iter->strNicName << " inet static" << endl;
				DebugPrintf(SV_LOG_INFO,"IpAddress = %s\n", iter->strIpAddress.c_str());
				outFile << "address " << iter->strIpAddress << endl;
				DebugPrintf(SV_LOG_INFO,"NetMask = %s\n", iter->strNetMask.c_str());
				outFile << "netmask " << iter->strNetMask << endl;
				size_t found = iter->strGateWay.find(",");
				if(found != std::string::npos)
				{
					DebugPrintf(SV_LOG_INFO,"GateWay = %s\n", iter->strGateWay.substr(0, found).c_str());
					outFile << "gateway " << iter->strGateWay.substr(0, found) << endl << endl;
				}
				else
				{
					DebugPrintf(SV_LOG_INFO,"GateWay = %s\n", iter->strGateWay.c_str());
					outFile << "gateway " << iter->strGateWay << endl << endl;
				}
			}
		}

		AssignFilePermission( nwDir+string("/interfaces"), strNwConfigFile);
	}
	
	if(bRhel6 || bUbuntu)
	{
		//This is required only in case of Rhel6 version onwards.
		if(false == GetMacAddressFile(strMacAddDir))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to find out the Mac Address containing file in  directory \"/etc/udev/rules.d/\" \n ");
			DebugPrintf(SV_LOG_ERROR,"Cannot update the new Mac Addresses for the machine %s\n", hostName.c_str());
			//bResult = false;
		}
		else
		{
			if(false == UpdateMacAddress(strMacAddDir, nwDir))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to update the new Mac Addresses for the machine %s\n", hostName.c_str());
				//bResult = false;
			}
		}
	}
	
	bool bDnsUpdate = false;
	if(!setDnsServer.empty())
	{
		if(false == UpdateDnsServerIP(hostName, strDnsFilePath, setDnsServer, nwDir))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to update the DNS Server IP for host : %s\n", hostName.c_str());
			//bResult  = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully updated the DNS Server IP for host : %s\n", hostName.c_str());
			bDnsUpdate = true;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"No DNS Server IP provided to update for host : %s\n", hostName.c_str());
	}
	
	if(bDnsUpdate)
	{
		if(false == UpdateNsSwitchFile(strDnsFilePath, nwDir))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to update the nsswitch.conf file for host : %s\n", hostName.c_str());
			//bResult  = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully updated the nsswitch.conf file for host : %s\n", hostName.c_str());
		}
	}
	
	if(!strHostIp.empty())
	{
		if(false == UpdateHostsFile(strDnsFilePath, nwDir, hostName, strHostIp))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to update the nsswitch.conf file for host : %s\n", hostName.c_str());
			//bResult  = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully updated the nsswitch.conf file for host : %s\n", hostName.c_str());
		}
	}

	if(!bUbuntu)
	{
		DebugPrintf(SV_LOG_INFO,"Creating Directory: %s\n", strNmFile.c_str());
		createDirectory(strNmFile);

		if(bRhel6)
		{		
			string strRhelFile = strNmFile + string("restartnetworkrhel");
			DebugPrintf(SV_LOG_INFO,"Creating file : %s\n", strRhelFile.c_str());
			ofstream outFile(strRhelFile.c_str());
			outFile << "NM_CONTROLLED=yes" << endl;
			outFile.close();
			DebugPrintf(SV_LOG_INFO,"Successfully created the file : %s\n", strRhelFile.c_str());
		}
		if(bOel)
		{
			string strOelFile = strNmFile + string("restartnetworkoel");
			DebugPrintf(SV_LOG_INFO,"Creating file : %s\n", strOelFile.c_str());
			ofstream outFile(strOelFile.c_str());
			outFile << "#InMage Created" << endl;
			outFile.close();
			DebugPrintf(SV_LOG_INFO,"Successfully created the file : %s\n", strOelFile.c_str());
		}
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


bool ChangeVmConfig::setNetworkConfigSuse(string hostName, string strMntDirectory)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	m_mapOfOldAndNewMacAdd.clear();
	set<string> setDnsServer;
	NetworkInfoMap_t NwMapFromXml;
	string strHostIp;
	
	if(false == xGetNWInfoFromXML(NwMapFromXml, hostName))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get Network settings from Recovery XML file for VM - %s\n", hostName.c_str());
		bResult = false;
    }
	string strNwDirectory = strMntDirectory + string("/");
	string strMacAddDir;
	string strDnsFilePath;
	
	if("yes" == m_storageInfo.strIsETCseperateFS)
	{
		strDnsFilePath = strNwDirectory;
		strMacAddDir = strNwDirectory + "udev/rules.d/";
		strNwDirectory = strNwDirectory + "sysconfig/network/";
	}
	else
	{
		strDnsFilePath = strNwDirectory + "etc/";
		strMacAddDir = strNwDirectory + "etc/udev/rules.d/";
		strNwDirectory = strNwDirectory + "etc/sysconfig/network/";
	}
	
	bool bSuse11 = false;
	if((m_strOsType.find("SUSE Linux Enterprise Server 11") != string::npos))
	{
		bSuse11 = true;
	}
	
	string nwDir = strNwDirectory + "inmage_nw";
	if(false == createDirectory(nwDir))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to create the directory %s.\n", nwDir.c_str());
	}
	
	string strNwConfigFile;
	std::list<NetworkInfo_t>::iterator iter;
	
	//m_NetworkInfo == filled from the source cx api xml file
	//NwMapFromXml == filled from the recovery\snapshot.xml file
	//In this  for loop we will refill the m_NetworkInfo with new ip values which we got in NwMapFromXml
	for(iter = m_NetworkInfo.begin(); iter != m_NetworkInfo.end(); iter++)
	{
		bool bOldIpExist = false;
		DebugPrintf(SV_LOG_INFO,"Looking for old Mac address %s of nic %s from xml file\n",iter->strMacAddress.c_str(), iter->strNicName.c_str());
		NetworkInfoMapIter_t nwIter;
		nwIter = NwMapFromXml.find(iter->strMacAddress);
		if(nwIter == NwMapFromXml.end())
		{
			DebugPrintf(SV_LOG_INFO,"Mac address %s of nic %s not found from xml file\n",iter->strMacAddress.c_str(), iter->strNicName.c_str());
			continue;
		}
		
		map<string, IpInfo> mapOfOldIpToIpInfo;
		map<string, IpInfo>::iterator iterIP;
		
		if(!iter->strIpAddress.empty())
			bOldIpExist = true;
		
		if(bOldIpExist)
		{
			if (false == createMapOfOldIPToIPInfo(nwIter->second.OldIPAddress, nwIter->second.IPAddress, nwIter->second.SubnetMask, 
													nwIter->second.DefaultGateway, nwIter->second.NameServer, mapOfOldIpToIpInfo))
			{
				DebugPrintf(SV_LOG_INFO,"Given IP addresses are not in proper order for old macaddress\n",nwIter->second.MacAddress.c_str());
				continue;
			}
			
			iterIP = mapOfOldIpToIpInfo.find(iter->strIpAddress);
			
			if(iterIP == mapOfOldIpToIpInfo.end())
			{
				DebugPrintf(SV_LOG_INFO,"Old IP address didn't find in the new ip list for old macaddress\n",nwIter->second.MacAddress.c_str());
				continue;
			}
		}
		
		if(nwIter->second.EnableDHCP == 1)
		{
			iter->strIsDhcp = "dhcp";
			if(!nwIter->second.MacAddress.empty())
				m_mapOfOldAndNewMacAdd.insert(make_pair(iter->strMacAddress, nwIter->second.MacAddress));
			else
				m_mapOfOldAndNewMacAdd.insert(make_pair(iter->strMacAddress, iter->strMacAddress));
			iter->strIpAddress = "";
			iter->strNetMask = "";
		}
		else
		{
			if(!nwIter->second.MacAddress.empty())
				m_mapOfOldAndNewMacAdd.insert(make_pair(iter->strMacAddress, nwIter->second.MacAddress));
			else
				m_mapOfOldAndNewMacAdd.insert(make_pair(iter->strMacAddress, iter->strMacAddress));
			if(!nwIter->second.IPAddress.empty())
			{
				if(bOldIpExist)
				{
					iter->strIpAddress = iterIP->second.IPAddress;
					strHostIp = iterIP->second.IPAddress;
				}
				else
				{
					iter->strIpAddress = nwIter->second.IPAddress;
					strHostIp = nwIter->second.IPAddress;
				}
			}
			if(!nwIter->second.SubnetMask.empty())
			{
				if(bOldIpExist)
					iter->strNetMask = iterIP->second.SubnetMask;
				else
					iter->strNetMask = nwIter->second.SubnetMask;
			}
			if(!nwIter->second.DefaultGateway.empty())
			{
				if(bOldIpExist)
					m_strGateWay = iterIP->second.DefaultGateway;
				else
					m_strGateWay = nwIter->second.DefaultGateway;
			}
			if(!nwIter->second.NameServer.empty())
			{
				iter->strDnsServer = nwIter->second.NameServer;
				std::string Separator = std::string(",");
				size_t pos = 0;

				size_t found = nwIter->second.NameServer.find(Separator, pos);
				if(found == std::string::npos)
				{
					DebugPrintf(SV_LOG_DEBUG,"Only single DNS ip provided for this Nic.\n");
					setDnsServer.insert(nwIter->second.NameServer);
				}
				else
				{
					while(found != std::string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",nwIter->second.NameServer.substr(pos,found-pos).c_str());
						setDnsServer.insert(nwIter->second.NameServer.substr(pos,found-pos));
						pos = found + 1; 
						found = nwIter->second.NameServer.find(Separator, pos);            
					}
					setDnsServer.insert(nwIter->second.NameServer.substr(pos));
				}
			}
			iter->strIsDhcp = "static";
		}
	}
	
	//In this for loop we are processing the newly filled m_NetworkInfo map to update the new ips.
	for(iter = m_NetworkInfo.begin(); iter != m_NetworkInfo.end(); iter++)
	{
		string strBroadCastAdd;
		string strNetworkAdd;
		if(false == getBroadCastandNWAddr(iter->strIpAddress, iter->strNetMask, strBroadCastAdd, strNetworkAdd))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed while calculating the Broadcast and Network address for IP: %s\n",iter->strIpAddress.c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully got the Broadcast address: %s and Network address: %s\n",strBroadCastAdd.c_str(), strNetworkAdd.c_str());
		}
		
		bool bMacFile = false;
		string strNicName, strNicLabel;
		bool bNicLabel = false;
		size_t strPos = iter->strNicName.find_first_of(":");
		if(strPos != string::npos)
		{
			strNicName = iter->strNicName.substr(0, strPos);
			strNicLabel = iter->strNicName.substr(strPos+1);
			bNicLabel = true;
		}
		else
			strNicName = iter->strNicName;
			
		strNwConfigFile = strNwDirectory + "ifcfg-" + strNicName;
		bool bFileExist = true;
		if(false == checkIfFileExists(strNwConfigFile))
		{
			DebugPrintf(SV_LOG_ERROR,"File Not Found %s...Checking the network file with mac address suffix\n",strNwConfigFile.c_str());
			strNwConfigFile = strNwDirectory + "ifcfg-eth-id-" + iter->strMacAddress;
			if(bNicLabel)
			{
				if(m_mapOfOldAndNewMacAdd.find(iter->strMacAddress) != m_mapOfOldAndNewMacAdd.end())
				{
					strNwConfigFile = strNwDirectory + "ifcfg-eth-id-" + m_mapOfOldAndNewMacAdd.find(iter->strMacAddress)->second;
				}
			}
			if(false == checkIfFileExists(strNwConfigFile))
			{
				bFileExist = false;
				DebugPrintf(SV_LOG_ERROR,"File Not Found %s\n",strNwConfigFile.c_str());				
				strNwConfigFile = strNwDirectory + "ifcfg-lo";
				if(false == checkIfFileExists(strNwConfigFile))
				{
					DebugPrintf(SV_LOG_ERROR,"Unble to Configure Network for device %s of machine %s\n",iter->strNicName.c_str(), hostName.c_str());
					bResult  = false;
				}
				else
				{
					if(bSuse11)
						strNwConfigFile = strNwDirectory + "ifcfg-" + strNicName;
					else
					{
						string strNewMacAdd = iter->strMacAddress;
						if(m_mapOfOldAndNewMacAdd.find(iter->strMacAddress) != m_mapOfOldAndNewMacAdd.end())
						{
							strNewMacAdd = m_mapOfOldAndNewMacAdd.find(iter->strMacAddress)->second;
						}
						strNwConfigFile = strNwDirectory + "ifcfg-eth-id-" + strNewMacAdd;
					}
					ofstream outFile(strNwConfigFile.c_str());
					string strLineRead = "#InMage Modified for host : ";
					outFile << strLineRead << hostName << endl;
					DebugPrintf(SV_LOG_INFO,"BootProto : %s\n", iter->strIsDhcp.c_str());
					outFile << "BOOTPROTO='" << iter->strIsDhcp << "'" << endl;
					if(!bNicLabel)
					{
						DebugPrintf(SV_LOG_INFO,"IpAddress : %s\n", iter->strIpAddress.c_str());
						outFile << "IPADDR='" << iter->strIpAddress << "'" << endl;
						DebugPrintf(SV_LOG_INFO,"NetMask : %s\n", iter->strNetMask.c_str());
						outFile << "NETMASK='" << iter->strNetMask << "'" << endl;
						DebugPrintf(SV_LOG_INFO,"Broadcast Address : %s\n", strBroadCastAdd.c_str());
						outFile << "BROADCAST='" << strBroadCastAdd << "'" << endl;
						DebugPrintf(SV_LOG_INFO,"Network Address : %s\n", strNetworkAdd.c_str());
						outFile << "NETWORK='" << strNetworkAdd << "'" << endl;
					}
					DebugPrintf(SV_LOG_INFO,"Start Mode : auto\n");
					outFile << "STARTMODE='auto'" << endl;
					DebugPrintf(SV_LOG_INFO,"Peer DNS : no\n");
					outFile << "PEERDNS='no'" << endl;
					outFile.close();

					AssignFilePermission(strNwDirectory + string("ifcfg-lo"), strNwConfigFile);
				}
			}
			else
			{
				bMacFile = true;
			}
		}
		if(true == bFileExist)
		{
			bool bTempFile = false;
			std::string strNwConfigFileTemp;
			if(!bMacFile)
			{
				strNwConfigFileTemp = strNwConfigFile + ".temp";
				bTempFile = true;
			}
			else
			{
				if(!bNicLabel)
				{
					if(m_mapOfOldAndNewMacAdd.find(iter->strMacAddress) != m_mapOfOldAndNewMacAdd.end())
					{
						string strNewMacAdd = m_mapOfOldAndNewMacAdd.find(iter->strMacAddress)->second;
						if(strNewMacAdd.compare(iter->strMacAddress) != 0)
						{
							strNwConfigFileTemp = strNwDirectory + "ifcfg-eth-id-" + strNewMacAdd;
						}
						else
						{
							strNwConfigFileTemp = strNwConfigFile + ".temp";
							bTempFile = true;
							bMacFile = false;
						}
					}
					else
					{
						strNwConfigFileTemp = strNwConfigFile + ".temp";
						bTempFile = true;
						bMacFile = false;
					}
				}
				else
				{
					bTempFile = true;
					strNwConfigFileTemp = strNwConfigFile + ".temp";
				}
			}
			ifstream inFile(strNwConfigFile.c_str());
			ofstream outFile(strNwConfigFileTemp.c_str());
			if(!inFile.is_open())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to open file %s\n ",strNwConfigFile.c_str());
				continue;
			}
			string strLineRead = "#InMage Modified for host : ";
			outFile << strLineRead << hostName << endl;
			strLineRead.clear();
			bool bBoot = false;
			bool bIpAdd = false;
			bool bNetMask = false;
			bool bDns = false;
			bool bIpLabel = false;
			bool bLabel = false;
			bool bNetMaskLabel = false;
			if(bNicLabel)
			{
				string strNicLabelExist = string("\'") + strNicLabel + string("\'");
				while(std::getline(inFile,strLineRead))
				{
					string strIpLabel = string("IPADDR_") + strNicLabel;
					string strLabel = string("LABEL_") + strNicLabel;
					string strNetMaskLabel = string("NETMASK_") + strNicLabel;
					if(strLineRead.find(strIpLabel) != string::npos)
					{
						bIpLabel = true;
						DebugPrintf(SV_LOG_INFO,"IP Address Label: %s\n", iter->strIpAddress.c_str());
						outFile << "IPADDR_" << strNicLabel << "='" << iter->strIpAddress << "'" << endl;
					}
					else if(strLineRead.find(strLabel) != string::npos)
					{
						bLabel = true;
						DebugPrintf(SV_LOG_INFO,"Label Name: %s\n", strNicLabel.c_str());
						outFile << "LABEL_" << strNicLabel << "='" << strNicLabel << "'" << endl;
					}
					else if(strLineRead.find(strNetMaskLabel) != string::npos)
					{
						bNetMaskLabel = true;
						DebugPrintf(SV_LOG_INFO,"NetMask Label : %s\n", iter->strNetMask.c_str());
						outFile << "NETMASK_" << strNicLabel << "='" << iter->strNetMask << "'" << endl;
					}
					else if(strLineRead.find(strNicLabelExist) != string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"Found string with Label %s. Skipping it\n", strLineRead.c_str());
					}
					else
						outFile << strLineRead << endl;
				}
			}
			else
			{
				while(std::getline(inFile,strLineRead))
				{
					if(strLineRead.find("BOOTPROTO") != string::npos)
					{
						bBoot = true;
						DebugPrintf(SV_LOG_INFO,"BootProto : %s\n", iter->strIsDhcp.c_str());
						outFile << "BOOTPROTO='" << iter->strIsDhcp << "'" << endl;
					}
					else if(strLineRead.compare(0,7,"IPADDR=") == 0)
					{
						bIpAdd = true;
						DebugPrintf(SV_LOG_INFO,"IpAddress : %s\n", iter->strIpAddress.c_str());
						outFile << "IPADDR='" << iter->strIpAddress << "'" << endl;
					}
					else if(strLineRead.find("NETMASK") != string::npos)
					{
						bNetMask = true;
						DebugPrintf(SV_LOG_INFO,"NetMask : %s\n", iter->strNetMask.c_str());
						outFile << "NETMASK='" << iter->strNetMask << "'" << endl;
					}
					else if(strLineRead.find("PEERDNS") != string::npos)
					{
						bDns = true;
						DebugPrintf(SV_LOG_INFO,"PEERDNS : no\n");
						outFile << "PEERDNS='no'" << endl; 
					}
					else if(strLineRead.find("BROADCAST") != string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"Broadcast Address : %s\n", strBroadCastAdd.c_str());
						outFile << "BROADCAST='" << strBroadCastAdd << "'" << endl;
					}
					else if(strLineRead.find("NETWORK") != string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"Network Address : %s\n", strNetworkAdd.c_str());
						outFile << "NETWORK='" << strNetworkAdd << "'" << endl;
					}
					else if(strLineRead.find("PREFIX") != string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"Found PREFIX, commenting it.\n");
						outFile << "#" << strLineRead << endl;
					}
					else
					{
						outFile << strLineRead << endl;
					}
				}
				if(!bBoot)
				{
					DebugPrintf(SV_LOG_INFO,"BootProto = %s\n", iter->strIsDhcp.c_str());
					outFile << "BOOTPROTO='" << iter->strIsDhcp << "'" << endl;
				}
				if(!bIpAdd)
				{
					DebugPrintf(SV_LOG_INFO,"IpAddress = %s\n", iter->strIpAddress.c_str());
					outFile << "IPADDR='" << iter->strIpAddress << "'" << endl;
				}
				if(!bNetMask)
				{
					DebugPrintf(SV_LOG_INFO,"NetMask = %s\n", iter->strNetMask.c_str());
					outFile << "NETMASK='" << iter->strNetMask << "'" << endl;
				}
				if(!bDns)
				{
					DebugPrintf(SV_LOG_INFO,"PEERDNS = no\n");
					outFile << "PEERDNS='no'" << endl; 
				}
			}
			inFile.close();
			outFile.close();

			AssignFilePermission(strNwConfigFile, strNwConfigFileTemp);

			int result;
			string strNwConfigFileOld = strNwConfigFile + ".old";
			result = rename( strNwConfigFile.c_str() , strNwConfigFileOld.c_str() );
			if(result != 0)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strNwConfigFile.c_str(), strNwConfigFileOld.c_str());	
			}
			else
			{
 				DebugPrintf(SV_LOG_INFO,"Successfully renamed the file %s into %s \n ",strNwConfigFile.c_str(), strNwConfigFileOld.c_str());
			}				
			moveFileToDir(strNwConfigFileOld, nwDir);

			if(bTempFile)
			{
				result = rename( strNwConfigFileTemp.c_str() , strNwConfigFile.c_str() );
				if(result != 0)
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strNwConfigFileTemp.c_str(), strNwConfigFile.c_str());	
				}
				else
				{
 					DebugPrintf(SV_LOG_INFO,"Successfully renamed the file %s into %s \n ",strNwConfigFileTemp.c_str(), strNwConfigFile.c_str());
				}
			}			
			if(false == listFilesInDir(strNwDirectory, "ifcfg-eth"))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to List out all the \"ifcfg\" files in directory \"/etc/sysconfig/network/\" \n ");
			}
		}
	}
	
	//string strMacAddFile;
	if(false == GetMacAddressFile(strMacAddDir))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to find out the Mac Address containing file in  directory \"/etc/udev/rules.d/\" \n ");
		DebugPrintf(SV_LOG_ERROR,"Cannot update the new Mac Addresses for the machine %s\n", hostName.c_str());
		//bResult = false;
	}
	else
	{
		if(false == UpdateMacAddress(strMacAddDir, nwDir))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to update the new Mac Addresses for the machine %s\n", hostName.c_str());
			//bResult = false;
		}
	}
	
	if(false == UpdateGateWay(strNwDirectory, nwDir))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to update the new GateWay for the machine %s\n", hostName.c_str());
	}
	
	bool bDnsUpdate = false;
	if(!setDnsServer.empty())
	{
		if(false == UpdateDnsServerIP(hostName, strDnsFilePath, setDnsServer, nwDir))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to update the DNS Server IP for host : %s\n", hostName.c_str());
			//bResult  = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully updated the DNS Server IP for host : %s\n", hostName.c_str());
			bDnsUpdate = true;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"No DNS Server IP provided to update for host : %s\n", hostName.c_str());
	}
	
	if(bDnsUpdate)
	{
		if(false == UpdateNsSwitchFile(strDnsFilePath, nwDir))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to update the nsswitch.conf file for host : %s\n", hostName.c_str());
			//bResult  = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully updated the nsswitch.conf file for host : %s\n", hostName.c_str());
		}
	}
	
	if(!strHostIp.empty())
	{
		if(false == UpdateHostsFile(strDnsFilePath, nwDir, hostName, strHostIp))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to update the nsswitch.conf file for host : %s\n", hostName.c_str());
			//bResult  = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully updated the nsswitch.conf file for host : %s\n", hostName.c_str());
		}
	}
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

/*
	This function will devide the comma separated ip's, netmask and gateway, whcih we got from the xml file
	This will create the map between the old ip to the new ip details.
*/
bool ChangeVmConfig::createMapOfOldIPToIPInfo(string OldIPAddress, string strIpAddress, string strSubNet, 
												string strGateWay, string strDnsServer, map<string, IpInfo>& mapOfOldIpToIpAndSubnet)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	DebugPrintf(SV_LOG_INFO,"Old IP Addresses : %s\n",OldIPAddress.c_str());
	DebugPrintf(SV_LOG_INFO,"NEW IP Addresses : %s\n",strIpAddress.c_str());
	DebugPrintf(SV_LOG_INFO,"SUBNET Addresses : %s\n",strSubNet.c_str());
	DebugPrintf(SV_LOG_INFO,"GATEWAY Addresses : %s\n",strGateWay.c_str());
	DebugPrintf(SV_LOG_INFO,"DNS Server Addresses : %s\n",strDnsServer.c_str());

	std::list<std::string> listOldIp;
    std::list<std::string> listIp;
	std::list<std::string> listSubNet;
	std::list<std::string> listGateWay;
	std::list<std::string> listDnsServer;
	IpInfo ipDetails;
    std::string Separator = std::string(",");

    size_t pos = 0;
	size_t found = OldIPAddress.find(Separator, pos);
	if(found == std::string::npos)
	{
		DebugPrintf(SV_LOG_DEBUG,"Only single IP found at Source for this Nic.\n");
		ipDetails.IPAddress = strIpAddress;
		ipDetails.SubnetMask = strSubNet;
		ipDetails.DefaultGateway = strGateWay;
		ipDetails.DnsServer = strDnsServer;
		mapOfOldIpToIpAndSubnet.insert(make_pair(OldIPAddress, ipDetails));
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return true;
	}
	else
	{
		while(found != std::string::npos)
		{
			DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",OldIPAddress.substr(pos,found-pos).c_str());
			listOldIp.push_back(OldIPAddress.substr(pos,found-pos));
			pos = found + 1; 
			found = OldIPAddress.find(Separator, pos);            
		}
		DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",OldIPAddress.substr(pos).c_str());
		listOldIp.push_back(OldIPAddress.substr(pos));
	}
	
	pos = 0;
	found = strIpAddress.find(Separator, pos);
	if(found == std::string::npos)
	{
		DebugPrintf(SV_LOG_ERROR,"There is something wrong in collecting New IPAddress info From source for this Nic \n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	else
	{
		while(found != std::string::npos)
		{
			DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",strIpAddress.substr(pos,found-pos).c_str());
			listIp.push_back(strIpAddress.substr(pos,found-pos));
			pos = found + 1; 
			found = strIpAddress.find(Separator, pos);            
		}
		DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",strIpAddress.substr(pos).c_str());
		listIp.push_back(strIpAddress.substr(pos));
	}

	pos = 0;
	found = strSubNet.find(Separator, pos);
	if(found == std::string::npos)
	{
		DebugPrintf(SV_LOG_ERROR,"There is something wrong in collecting NetMask info From source for this Nic \n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	else
	{
		while(found != std::string::npos)
		{
			DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",strSubNet.substr(pos,found-pos).c_str());
			listSubNet.push_back(strSubNet.substr(pos,found-pos));
			pos = found + 1; 
			found = strSubNet.find(Separator, pos);            
		}
		DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",strSubNet.substr(pos).c_str());
		listSubNet.push_back(strSubNet.substr(pos));
	}
	
	pos = 0;
	found = strGateWay.find(Separator, pos);
	if(found == std::string::npos)
	{
		DebugPrintf(SV_LOG_INFO,"Only Single gateway is provided\n");
		listGateWay.push_back(strGateWay);
	}
	else
	{
		while(found != std::string::npos)
		{
			DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",strGateWay.substr(pos,found-pos).c_str());
			listGateWay.push_back(strGateWay.substr(pos,found-pos));
			pos = found + 1; 
			found = strGateWay.find(Separator, pos);            
		}
		DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",strGateWay.substr(pos).c_str());
		listGateWay.push_back(strGateWay.substr(pos));
	}

	pos = 0;
	found = strDnsServer.find(Separator, pos);
	if(found == std::string::npos)
	{
		DebugPrintf(SV_LOG_INFO,"Only Single DnsServer is provided\n");
		listDnsServer.push_back(strDnsServer);
	}
	else
	{
		while(found != std::string::npos)
		{
			DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",strDnsServer.substr(pos,found-pos).c_str());
			listDnsServer.push_back(strDnsServer.substr(pos,found-pos));
			pos = found + 1; 
			found = strDnsServer.find(Separator, pos);            
		}
		DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",strDnsServer.substr(pos).c_str());
		listDnsServer.push_back(strDnsServer.substr(pos));
	}

	std::list<std::string>::iterator iterOldIp = listOldIp.begin();
	std::list<std::string>::iterator iterIp = listIp.begin();
	std::list<std::string>::iterator iterNet = listSubNet.begin();
	std::list<std::string>::iterator iterGw = listGateWay.begin();
	std::list<std::string>::iterator iterDns = listDnsServer.begin();

	for(; iterOldIp != listOldIp.end(); iterOldIp++)
	{
		if(iterIp == listIp.end())
		{
			DebugPrintf(SV_LOG_INFO,"No more new IP's available for the Old IP: %s\n", iterOldIp->c_str());
			continue;
		}
		ipDetails.IPAddress = (*iterIp);

		string subNet;
		if(iterNet == listSubNet.end())
			subNet = "255.0.0.0";
		else
		{
			subNet = (*iterNet);
		}
		ipDetails.SubnetMask = subNet;

		ipDetails.DefaultGateway = "";
		if(iterGw != listGateWay.end())
		{
			if(iterGw->compare("notselected") != 0)
				ipDetails.DefaultGateway = (*iterGw);
		}

		ipDetails.DnsServer = "";
		if(iterDns != listDnsServer.end())
		{
			if(iterDns->compare("notselected") != 0)
				ipDetails.DnsServer = (*iterDns);
		}
		DebugPrintf(SV_LOG_INFO,"Old IP: %s --> IP: %s  --> SubNet: %s  --> GateWay: %s  --> DnsServer: %s\n",
								(*iterOldIp).c_str(), ipDetails.IPAddress.c_str(), ipDetails.SubnetMask.c_str(), ipDetails.DefaultGateway.c_str(), ipDetails.DnsServer.c_str());
		mapOfOldIpToIpAndSubnet.insert(make_pair((*iterOldIp), ipDetails));
		iterIp++;
		iterNet++;
		iterGw++;
		iterDns++;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}



bool ChangeVmConfig::listFilesInDir(std::string strDirectory, std::string strFile)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	DebugPrintf(SV_LOG_DEBUG,"List out Files on name %s in directory %s\n", strFile.c_str(), strDirectory.c_str());
	stringstream results;
	string cmd; 
	if(strFile.empty())
		cmd = "ls " + strDirectory;
	else
		cmd = "ls " + strDirectory + strFile + "*";
	cmd += " 2> /dev/null ";
	DebugPrintf(SV_LOG_DEBUG,"Command: %s\n",cmd.c_str());
	if (!executePipe(cmd, results))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to run the command to list out the files: %s\n", cmd.c_str());
        bResult = false;
    }
	else
    {
		DebugPrintf(SV_LOG_DEBUG, "List of files: \n");
        string commandresult;
		while(!results.eof())
		{
			results >> commandresult;
			trim(commandresult);
			if(!commandresult.empty())
			{
				DebugPrintf(SV_LOG_DEBUG, "%s\n", commandresult.c_str());
			}
		}
        bResult = true;
    }
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

//This will assign the file permission of one file to the other file
//Parameter 1 - input file name whose permission need to store on other
//Parameter 2 - input file name on which param1 file permission need to restore.
bool ChangeVmConfig::AssignFilePermission(string strFirstFile, string& strSecondFile)
{
	bool bResult = false;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	if(false == checkIfFileExists(strFirstFile))
	{
		DebugPrintf(SV_LOG_ERROR, "File [%s] does not exist. Cannot assign the permission.\n", strFirstFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return bResult;
	}
	if(false == checkIfFileExists(strSecondFile))
	{
		DebugPrintf(SV_LOG_ERROR, "File [%s] does not exist. Cannot assign the permission.\n", strSecondFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return bResult;
	}
	
	stringstream results;
	string cmd = "chmod --reference " + strFirstFile + " " + strSecondFile;
	cmd += " 2> /dev/null ";
	DebugPrintf(SV_LOG_INFO,"Command: %s\n",cmd.c_str());
	if (!executePipe(cmd, results))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to run the command to assign the permisssions: %s\n", cmd.c_str());
    }
	else
    {
		DebugPrintf(SV_LOG_INFO, "Successfully assigned the permisssions to file [%s] of file [%s]\n", strSecondFile.c_str(), strFirstFile.c_str());
        bResult = true;
    }
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


bool ChangeVmConfig::GetMacAddressFile(string &strMacAddDir)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	stringstream results;
	string cmd = "ls " + strMacAddDir + " | grep persistent | grep net" ;
	cmd += " 2> /dev/null ";
	DebugPrintf(SV_LOG_INFO,"Command: %s\n",cmd.c_str());
	if (!executePipe(cmd, results))
    {
        DebugPrintf(SV_LOG_INFO, "Failed to run the command to list out the files: %s\n", cmd.c_str());
        bResult = false;
    }
	else
    {
		DebugPrintf(SV_LOG_INFO, "Got the file: ");
		string strMacAddFile;
		results >> strMacAddFile;
		trim(strMacAddFile);
		if(!strMacAddFile.empty())
		{
			strMacAddDir = strMacAddDir + strMacAddFile;
			DebugPrintf(SV_LOG_INFO, " %s\n", strMacAddDir.c_str());			
		}
        bResult = true;
    }
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::UpdateMacAddress(string strMacAddFile, string strNwDir)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	if(false == checkIfFileExists(strMacAddFile))
	{
		DebugPrintf(SV_LOG_INFO, " File %s not found...Unable to update the Mac Adresses\n", strMacAddFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	
	string strMacAddFileTemp = strMacAddFile + ".temp";
	ifstream inFile(strMacAddFile.c_str());
	ofstream outFile(strMacAddFileTemp.c_str());
	if(!inFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open file %s\n ",strMacAddFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	string strLineRead;
	
	while(std::getline(inFile,strLineRead))
	{
		if(strLineRead.find("SUBSYSTEM") != string::npos)
		{
			size_t pos;
			std::map<string, string>::iterator iter = m_mapOfOldAndNewMacAdd.begin();
			for(; iter != m_mapOfOldAndNewMacAdd.end(); iter++)
			{
				pos = strLineRead.find(iter->first);
				if(pos != string::npos)
				{
					DebugPrintf(SV_LOG_INFO,"Updateing Old Mac Address: %s to New MacAddress: %s\n ",iter->first.c_str(), iter->second.c_str());
					string strMacAdd = iter->second;
					for(size_t i = 0; i < strMacAdd.length(); i++)
						strMacAdd[i] = tolower(strMacAdd[i]);
					strLineRead.replace(pos, iter->first.length(), strMacAdd);
				}
			}
		}
		outFile << strLineRead << endl;
	}
	
	inFile.close();
	outFile.close();
	int result;
	
	string strMacAddFileOld = strMacAddFile + ".old";
	result = rename( strMacAddFile.c_str() , strMacAddFileOld.c_str() );
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strMacAddFile.c_str(), strMacAddFileOld.c_str());	
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Successfully renamed the file %s into %s \n ",strMacAddFile.c_str(), strMacAddFileOld.c_str());
	}
		
	result = rename( strMacAddFileTemp.c_str() , strMacAddFile.c_str());
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strMacAddFileTemp.c_str(), strMacAddFile.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Successfully renamed the file %s into %s \n ",strMacAddFileTemp.c_str(), strMacAddFile.c_str());
		DebugPrintf(SV_LOG_INFO, "Successfully edited the File: %s \n", strMacAddFile.c_str());
	}

	AssignFilePermission(strMacAddFileOld, strMacAddFile);

	moveFileToDir(strMacAddFileOld, strNwDir);
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::UpdateGateWay(string strNwDirectory, string strNwDir)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	string strGateWayFile = strNwDirectory + "routes";
	if(false == checkIfFileExists(strGateWayFile))
	{
		DebugPrintf(SV_LOG_INFO, " File %s not found...Unable to update the GateWay\n", strGateWayFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	
	string strGateWayFileTemp = strGateWayFile + ".temp";
	ifstream inFile(strGateWayFile.c_str());
	ofstream outFile(strGateWayFileTemp.c_str());
	if(!inFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open file %s\n ",strGateWayFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	
	string strLineRead;	
	while(std::getline(inFile,strLineRead))
	{
		if(strLineRead.find("default") != string::npos)
		{
			if(!m_strGateWay.empty())
			{
				DebugPrintf(SV_LOG_INFO,"Updateing the new default GateWay: %s\n ",m_strGateWay.c_str());
				strLineRead = "default " + m_strGateWay + " - -";
			}
		}
		outFile << strLineRead << endl;
	}
	
	inFile.close();
	outFile.close();
	int result;
	
	string strGateWayFileOld = strGateWayFile + ".old";
	result = rename( strGateWayFile.c_str() , strGateWayFileOld.c_str() );
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strGateWayFile.c_str(), strGateWayFileOld.c_str());	
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Successfully renamed the file %s into %s \n ",strGateWayFile.c_str(), strGateWayFileOld.c_str());
	}	
		
	result = rename( strGateWayFileTemp.c_str() , strGateWayFile.c_str());
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strGateWayFileTemp.c_str(), strGateWayFile.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Successfully renamed the file %s into %s \n ",strGateWayFileTemp.c_str(), strGateWayFile.c_str());
		DebugPrintf(SV_LOG_INFO, "Successfully edited the File: %s \n", strGateWayFile.c_str());
	}

	AssignFilePermission(strGateWayFileOld, strGateWayFile);
	moveFileToDir(strGateWayFileOld, strNwDir);
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

//Updates the DNSServer IP's which are present in the set in the file "resolv.conf"
//Updation will happen on the top of the file by reading one by one entry from the set.
bool ChangeVmConfig::UpdateDnsServerIP(string hostName, string strDnsFilePath, set<string> setDnsServer, string strNwDir)
{

	bool bResult = true;
	bool bfileExist = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	set<string>::iterator iter;
	string strDnsResolvFileOld, strLineRead, cmd;	
	string strDnsResolvFile = strDnsFilePath + "resolv.conf";
	if(false == checkIfFileExists(strDnsResolvFile))
	{
		DebugPrintf(SV_LOG_ERROR,"File %s Not Found, Unble to Configure the DNS\n",strDnsResolvFile.c_str());
		bfileExist = false;
	}
	else
	{
		strDnsResolvFileOld = strDnsResolvFile + ".old";
		if(!copyFileToTarget(strDnsResolvFile, strDnsResolvFileOld))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to copy file %s to %s \n ",strDnsResolvFile.c_str(), strDnsResolvFileOld.c_str());	
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully copied the file %s to %s \n ",strDnsResolvFile.c_str(), strDnsResolvFileOld.c_str());
		}
	}

	ofstream outFile;
	outFile.open(strDnsResolvFile.c_str(),std::ofstream::trunc);
	if(outFile.is_open())
	{
		strLineRead = string("#InMage Modified for host : ") + hostName;
		DebugPrintf(SV_LOG_INFO,"Resolve config file : %s\n ",strLineRead.c_str());
		outFile << strLineRead << endl;
		outFile.flush();
		strLineRead.clear();
		iter = setDnsServer.begin();
		for(; iter != setDnsServer.end(); iter++)
		{
			strLineRead = string("nameserver ") + (*iter);
			DebugPrintf(SV_LOG_INFO,"Updating DNS ip in resolv.conf file: %s\n ",strLineRead.c_str());
			outFile << strLineRead << endl;
			outFile.flush();
		}
		outFile.close();	
	
		//Adding Domain search part of in resolv.conf file. Igonring the fail case because it should not effect the network change worflow
		stringstream results;
		cmd = string("grep \"^search\" ") + strDnsResolvFileOld + ">>" + strDnsResolvFile;
		DebugPrintf(SV_LOG_INFO,"cmd = : %s\n", cmd.c_str());
		if (!executePipe(cmd, results))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to run the command to place the domain search entries in file : %s\n", strDnsResolvFile.c_str());
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to update the file %s \n ",strDnsResolvFile.c_str());	
		bResult = false;
	}

	AssignFilePermission(strDnsResolvFileOld, strDnsResolvFile);

	strDnsResolvFile = strDnsFilePath + ".resolv.conf";
	ofstream resolvFile(strDnsResolvFile.c_str());

	strLineRead = string("#InMage Modified for host : ") + hostName;
	DebugPrintf(SV_LOG_INFO,"in hidden file : %s\n ",strLineRead.c_str());
	resolvFile << strLineRead << endl;
	resolvFile.flush();
	strLineRead.clear();
	iter = setDnsServer.begin();
	for(; iter != setDnsServer.end(); iter++)
	{
		strLineRead = string("nameserver ") + (*iter);
		DebugPrintf(SV_LOG_INFO,"Updating DNS ip in hidden file: %s\n ",strLineRead.c_str());
		resolvFile << strLineRead << endl;
		resolvFile.flush();
	}
	resolvFile.close();
	
	//Adding Domain search part of in hidden resolv.conf file.
	cmd = string("grep \"^search\" ") + strDnsResolvFileOld + ">>" + strDnsResolvFile;
	DebugPrintf(SV_LOG_INFO,"cmd = : %s\n", cmd.c_str());
	stringstream resultbuf;
	if (!executePipe(cmd, resultbuf))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run the command to place the domain search entries in hidden file : %s\n", strDnsResolvFile.c_str());
    }
	
	AssignFilePermission(strDnsResolvFileOld, strDnsResolvFile);

	//Moving the original resolv.conf file to inmage debug directory
	if(bfileExist)
		moveFileToDir(strDnsResolvFileOld, strNwDir);
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

//Updates the "nsswitch.conf" file and keeps teh old file in the "strNwDir"
bool ChangeVmConfig::UpdateNsSwitchFile(string strNwSwitchFilePath, string strNwDir)
{

	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	string strNsSwitchFile = strNwSwitchFilePath + "nsswitch.conf";
	if(false == checkIfFileExists(strNsSwitchFile))
	{
		DebugPrintf(SV_LOG_ERROR,"File %s Not Found, Unble to Configure the DNS\n",strNsSwitchFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return bResult;
	}
	
	string strNsSwitchFileTemp = strNsSwitchFile + ".temp";
	ifstream inFile(strNsSwitchFile.c_str());
	ofstream outFile(strNsSwitchFileTemp.c_str());
	if(!inFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open file %s\n ",strNsSwitchFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	string strLineRead;	
	strLineRead.clear();
	
	while(std::getline(inFile,strLineRead))
	{
		if(strLineRead.find("hosts:") != string::npos)
		{
			string strDnshost = strLineRead.substr(0,6);
			if(strDnshost == "hosts:")
			{
				if(strLineRead.find("files") == string::npos)
				{
					strLineRead = strLineRead + " files";
				}
				if(strLineRead.find("dns") == string::npos)
				{
					strLineRead = strLineRead + " dns";
				}
			}
			DebugPrintf(SV_LOG_INFO,"Updating Entry: %s\n",strLineRead.c_str());
			outFile << strLineRead << endl;
		}
		else
		{
			outFile << strLineRead << endl;
		}
	}
	
	inFile.close();
	outFile.close();
	int result;
	
	string strNsSwitchFileOld = strNsSwitchFile + ".old";
	result = rename( strNsSwitchFile.c_str() , strNsSwitchFileOld.c_str() );
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strNsSwitchFile.c_str(), strNsSwitchFileOld.c_str());	
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Successfully renamed the file %s into %s \n ",strNsSwitchFile.c_str(), strNsSwitchFileOld.c_str());
	}
		
	result = rename( strNsSwitchFileTemp.c_str() , strNsSwitchFile.c_str());
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strNsSwitchFileTemp.c_str(), strNsSwitchFile.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Successfully renamed the file %s into %s \n ",strNsSwitchFileTemp.c_str(), strNsSwitchFile.c_str());
		DebugPrintf(SV_LOG_INFO, "Successfully edited the File: %s \n", strNsSwitchFile.c_str());
	}

	AssignFilePermission(strNsSwitchFileOld, strNsSwitchFile);
	moveFileToDir(strNsSwitchFileOld, strNwDir);
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

//Updates the newIP and corresponding hostname in the file "/etc/hosts"
bool ChangeVmConfig::UpdateHostsFile(string strHostFilePath, string strNwDir, string strHostName, string strHostIp)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	string strHostsFile = strHostFilePath + "hosts";
	if(false == checkIfFileExists(strHostsFile))
	{
		DebugPrintf(SV_LOG_ERROR,"File %s Not Found, Unble to Configure the DNS\n",strHostsFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return bResult;
	}

	std::map<std::string, std::string>::iterator iter = m_mapOfVmNameToId.find(strHostName);
	if(iter != m_mapOfVmNameToId.end())
	{
		strHostName = iter->second;
	}
	
	string strHostsFileTemp = strHostsFile + ".temp";
	ifstream inFile(strHostsFile.c_str());
	ofstream outFile(strHostsFileTemp.c_str());
	if(!inFile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open file %s\n ",strHostsFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	string strLineRead;	
	strLineRead.clear();
	
	while(std::getline(inFile,strLineRead))
	{
		if(strLineRead.find(strHostName) == string::npos)
		{
			outFile << strLineRead << endl;
		}
	}
	
	strLineRead = strHostIp + string("\t") + strHostName;
	DebugPrintf(SV_LOG_INFO,"Updating Entry: %s\n",strLineRead.c_str());
	outFile << strLineRead << endl;
	
	inFile.close();
	outFile.close();
	int result;
	
	string strHostsFileOld = strHostsFile + ".old";
	result = rename( strHostsFile.c_str() , strHostsFileOld.c_str() );
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strHostsFile.c_str(), strHostsFileOld.c_str());	
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Successfully renamed the file %s into %s \n ",strHostsFile.c_str(), strHostsFileOld.c_str());
	}
		
	result = rename( strHostsFileTemp.c_str() , strHostsFile.c_str());
	if(result != 0)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to rename file %s into %s \n ",strHostsFileTemp.c_str(), strHostsFile.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Successfully renamed the file %s into %s \n ",strHostsFileTemp.c_str(), strHostsFile.c_str());
		DebugPrintf(SV_LOG_INFO, "Successfully edited the File: %s \n", strHostsFile.c_str());
	}

	AssignFilePermission(strHostsFileOld, strHostsFile);
	moveFileToDir(strHostsFileOld, strNwDir);
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


//Gets the BroadCast and Network address by taking input as ipaddress and netmak.
//To get the network address perform a bitwise AND on the IP address and the netmask:
//network = ip & netmask
//To get the broadcast address perform a bitwise OR on the network address and the complement of the netmask:
//broadcast = network | ~netmask
bool ChangeVmConfig::getBroadCastandNWAddr(string strIpAddress, string strNetMask, string &strBroadCastAdd, string &strNetworkAdd)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	
	struct in_addr ip;
    struct in_addr netmask;
    struct in_addr network;
    struct in_addr broadcast;

    inet_aton(strIpAddress.c_str(), &ip);
    inet_aton(strNetMask.c_str(), &netmask);

    // bitwise AND of ip and netmask gives the network
    network.s_addr = ip.s_addr & netmask.s_addr;
	strNetworkAdd = inet_ntoa(network);

    // bitwise OR of network and complement of netmask gives the broadcast
    broadcast.s_addr = network.s_addr | ~netmask.s_addr;
	strBroadCastAdd = inet_ntoa(broadcast);
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::ListAllPIDs(set<string>& stPids)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	stringstream results;

	string cmd = string("ps -aef 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
	DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
	
	string cmd1 = string("echo running command : \"") + cmd + string("\" >>") + m_InMageCmdLogFile;
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
		string commandresult;
		while(!results1.eof())
		{
			results1 >> commandresult;
			if(!commandresult.empty())
			{
				DebugPrintf(SV_LOG_DEBUG, "%s\n", commandresult.c_str());
				stPids.insert(commandresult);
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::ListPIDsHoldsMntPath(const string& strPath, set<string>& stPids)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	stringstream results;
	
	string cmd = string("lsof ") + strPath + string("|awk '{print $2}' 1>>") + m_InMageCmdLogFile + string(" 2>>") + m_InMageCmdLogFile;
	DebugPrintf(SV_LOG_DEBUG,"cmd = : %s\n", cmd.c_str());
	
	string cmd1 = string("echo running command : \"") + cmd + string("\" >>") + m_InMageCmdLogFile;
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
		string commandresult;
		while(!results1.eof())
		{
			results1 >> commandresult;
			trim(commandresult);
			if(!commandresult.empty())
			{
				DebugPrintf(SV_LOG_INFO, "%s\n", commandresult.c_str());
				if(commandresult.compare("PID") != 0)
					stPids.insert(commandresult);
			}
		}
	}
	return bResult;
}


bool ChangeVmConfig::MountSourceLinuxPartition(string strRootMnt, string strHostName, string strPartitionName, string strMountDir, map<string, StorageInfo_t>& mapMountedInfo)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bReturnValue = true;
	m_storageInfo.reset();
		
	if (false == getHostInfoUsingCXAPI(strHostName, strPartitionName, false))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed while processing CX API call\n");
		DebugPrintf(SV_LOG_ERROR, "Cannot Proceed Further for Linux P2V configuration...\n");
		bReturnValue = false;
	}
	else if (m_storageInfo.strIsETCseperateFS.compare("yes") == 0)
	{
		do
		{	
			DebugPrintf(SV_LOG_DEBUG, "Found seperate partition for : %s\n", strPartitionName.c_str());

			string strTgtPartitionType;
			if ("LVM" == m_storageInfo.strRootVolType)
			{
				strTgtPartitionType = m_storageInfo.strLvName;
				if (false == activateVolGrp(m_storageInfo.strVgName))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed To Activate the LV : %s\n", strTgtPartitionType.c_str());
					bReturnValue = false;
					break;
				}
				m_storageInfo.bVgActive = true;
			}
			else
			{
				string strSrcPartition = m_storageInfo.strPartition;
				if (false == processPartition(strHostName, strSrcPartition, strTgtPartitionType))
				{
					DebugPrintf(SV_LOG_ERROR, "Cannot Proceed Further for Linux P2V configuration...\n");
					bReturnValue = false;
					break;
				}
			}

			if (true != mountPartition(strRootMnt, strTgtPartitionType, strMountDir))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to Mount the Partition: %s\n", strPartitionName.c_str());
				bReturnValue = false;
				break;
			}
			else
				m_storageInfo.bMounted = true;
		}while(0);
		mapMountedInfo.insert(make_pair(strPartitionName, m_storageInfo));
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Didn't find seperate partition for %s.\n", strPartitionName.c_str());
	}	

	DebugPrintf(SV_LOG_DEBUG,"Existing %s\n",__FUNCTION__);
	return bReturnValue;
}


#endif /*Linux*/

/*
ALGO: we need to read the network class name from the files created on the source side and use
that class name to mount directly,then change IP configuration directly..No need to read from software 
registry key.Directly read from system key and chaneg the IP from controlset001 and controlset002
This way we are assured that IP configured properly
*/

#ifdef WIN32

// callback I/O functions needed for FDI routines
FNALLOC(memAlloc)
{
    return malloc(cb);
}
FNFREE(memFree)
{
    free(pv);
}
FNOPEN(fileOpen)
{
    return _open(pszFile, oflag, pmode);
}
FNREAD(fileRead)
{
    return _read(hf, pv, cb);
}
FNWRITE(fileWrite)
{
    return _write(hf, pv, cb);
}
FNCLOSE(fileClose)
{
    return _close(hf);
}
FNSEEK(fileSeek)
{
    return _lseek(hf, dist, seektype);
}

int extractCabFile(CabExtractInfo* cabInfo, char const* fileName)
{
    if (0 == cabInfo->m_extractFiles.erase(fileName)) {
        return 0; // do not extract
    }
    std::string out(cabInfo->m_destination + fileName);
    return fileOpen((LPSTR)out.c_str(), _O_BINARY | _O_CREAT | _O_WRONLY | _O_SEQUENTIAL, _S_IREAD | _S_IWRITE);
}

FNFDINOTIFY(fdiNotifyCallback)
{
    switch (fdint)
	{
		case fdintCOPY_FILE: // file to be copied
			return extractCabFile((CabExtractInfo*)pfdin->pv, pfdin->psz1);
        case fdintCLOSE_FILE_INFO: // close the file, set relevant info
            _close(pfdin->hf);
            return TRUE;
        default:
            break;
    }
    return FALSE;
}

void fdiCleanup(HFDI hfdi)
{
    if (NULL != hfdi) {
        FDIDestroy(hfdi);
    }
}

DWORD closeRegKey(HKEY& hKey)
{
	DWORD lRv;
	if(hKey != NULL)
	{
		lRv = RegCloseKey(hKey);
		hKey = NULL;
	}
	return lRv;
}

bootSectFix objbootSectFix(false);
bool ChangeVmConfig:: printBootSectorInfo(std::string& mntptName) 
{
	//P2VMount point filled in changeVmIpAdd
	char* argv[2];
	argv[0] = "-v";
	argv[1] = const_cast<char*>(mntptName.c_str());

	if(0 != objbootSectFix.fixBootSector(2,argv))
	{
		return false;
	}
	return true;
}

//***************************************************************************************
// Prepare to read/modify registry changes
//   Set required Privileges to true
//   Load system and software registry hives.
//***************************************************************************************
bool ChangeVmConfig::PreRegistryChanges(std::string TgtMntPtForSrcSystemVolume,
                                        std::string SystemHiveName, std::string SoftwareHiveName,
                                        bool & bSystemHiveLoaded, bool & bSoftwareHiveLoaded)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
    bool rv = true;
       
    do
    {
        if(false == SetPrivileges(SE_BACKUP_NAME, true))
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to set SE_BACKUP_NAME privileges.\n");
            rv = false; break;
        }
        if(false == SetPrivileges(SE_RESTORE_NAME, true))
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to set SE_RESTORE_NAME privileges.\n");
            rv = false; break;
        }

        long lReturnValue;
        std::string SystemHivePath = TgtMntPtForSrcSystemVolume + std::string(VM_SYSTEM_HIVE_PATH);    
        std::string SoftwareHivePath = TgtMntPtForSrcSystemVolume + std::string(VM_SOFTWARE_HIVE_PATH);        

		if(false == checkIfFileExists(SystemHivePath))
		{
			SystemHivePath = TgtMntPtForSrcSystemVolume + string("\\WINNT\\system32\\config\\system");
			SoftwareHivePath = TgtMntPtForSrcSystemVolume + string("\\WINNT\\system32\\config\\software");
			m_bWinNT = true;
		}

		DebugPrintf(SV_LOG_DEBUG,"SystemHivePath   - %s\n",SystemHivePath.c_str());
        DebugPrintf(SV_LOG_DEBUG,"SoftwareHivePath - %s\n",SoftwareHivePath.c_str());

		string strRegPathToOpen = SystemHiveName;
		HKEY hKey;
		long lRV = RegOpenKeyEx( HKEY_LOCAL_MACHINE, strRegPathToOpen.c_str(), 0, KEY_ALL_ACCESS , &hKey);

		if(lRV == ERROR_SUCCESS)
		{
			closeRegKey(hKey);
			lReturnValue = RegUnLoadKey(HKEY_LOCAL_MACHINE, SystemHiveName.c_str());
            if(ERROR_SUCCESS != lReturnValue)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to unload the \"%s\" at HKLM. Return value for RegUnLoadKey() - [%lu].\n", SystemHivePath.c_str(), lReturnValue);
            }
		}
		if(hKey != NULL)
		{
			closeRegKey(hKey);
		}
		
		DebugPrintf(SV_LOG_DEBUG,"Loading the Hive %s in registry\n", SystemHiveName.c_str());
				
		// Load System Hive
		lReturnValue = RegLoadKey(HKEY_LOCAL_MACHINE, SystemHiveName.c_str(), SystemHivePath.c_str());    
		if(ERROR_SUCCESS != lReturnValue)
		{            
			DebugPrintf(SV_LOG_ERROR, "Failed to load \"%s\" at HKLM. Return value for RegLoadKey() = [%lu].\n", SystemHivePath.c_str(), lReturnValue);
			rv = false; break;
		}
		bSystemHiveLoaded = true;

		strRegPathToOpen = SoftwareHiveName;
		lRV = RegOpenKeyEx( HKEY_LOCAL_MACHINE, strRegPathToOpen.c_str(), 0, KEY_ALL_ACCESS , &hKey);

		if(lRV == ERROR_SUCCESS)
		{
			closeRegKey(hKey);
			lReturnValue = RegUnLoadKey(HKEY_LOCAL_MACHINE, SoftwareHiveName.c_str());
            if(ERROR_SUCCESS != lReturnValue)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to unload the \"%s\" at HKLM. Return value for RegUnLoadKey() - [%lu].\n", SoftwareHivePath.c_str(), lReturnValue);
            }
		}
		if(hKey != NULL)
		{
			closeRegKey(hKey);
		}
		
		DebugPrintf(SV_LOG_DEBUG,"Loading the Hive %s in registry\n", SoftwareHiveName.c_str());
        // Load Software Hive    
        lReturnValue = RegLoadKey(HKEY_LOCAL_MACHINE, SoftwareHiveName.c_str(), SoftwareHivePath.c_str());    
        if(ERROR_SUCCESS != lReturnValue)
        {	    
            DebugPrintf(SV_LOG_ERROR, "Failed to load \"%s\" at HKLM. Return value for RegLoadKey() = [%lu].\n", SoftwareHivePath.c_str(), lReturnValue);
	        rv = false; break;
        }
        bSoftwareHiveLoaded = true;
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
    return rv;
}

//***************************************************************************************
// Restore the changes done to read/modify registry changes in PreRegistryChanges()
//   Set required Privileges to false
//   Unload system and software registry hives.
//***************************************************************************************
bool ChangeVmConfig::PostRegistryChanges(std::string TgtMntPtForSrcSystemVolume,
                                         std::string SystemHiveName, std::string SoftwareHiveName,
                                         bool bSystemHiveLoaded, bool bSoftwareHiveLoaded)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
    bool rv = true;

    do
    {
        long lReturnValue;
        std::string SystemHivePath = TgtMntPtForSrcSystemVolume + std::string(VM_SYSTEM_HIVE_PATH);    
        std::string SoftwareHivePath = TgtMntPtForSrcSystemVolume + std::string(VM_SOFTWARE_HIVE_PATH);    

        // Unload sytem hive
        if(bSystemHiveLoaded)
        {
            lReturnValue = RegUnLoadKey(HKEY_LOCAL_MACHINE, SystemHiveName.c_str());
            if(ERROR_SUCCESS != lReturnValue)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to unload the \"%s\" at HKLM. Return value for RegUnLoadKey() - [%lu].\n", SystemHivePath.c_str(), lReturnValue);
		        rv = false;
            }
        }

        // Unload software hive
        if(bSoftwareHiveLoaded)
        {
            lReturnValue = RegUnLoadKey(HKEY_LOCAL_MACHINE, SoftwareHiveName.c_str());
            if(ERROR_SUCCESS != lReturnValue)
            {
                DebugPrintf(SV_LOG_ERROR, "Failed to unload the \"%s\" at HKLM. Return value for RegUnLoadKey() - [%lu].\n", SoftwareHivePath.c_str(), lReturnValue);
		        rv = false;
            }
        }

        SetPrivileges(SE_BACKUP_NAME, false);
        SetPrivileges(SE_RESTORE_NAME, false);
    }while(0);
    
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
    return rv;
}

/**************************************************************************************
// Make the Registry Changes required for converting the Inmage services start up type.
//  Input - HostName
//  Return true if successfully changes.
***************************************************************************************/
bool ChangeVmConfig::MakeInMageSVCManual(std::string strHostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
    bool rv = true;

	bool bSystemHiveLoaded = false;
    bool bSoftwareHiveLoaded = false;
    
    std::string SystemHiveName = std::string(VM_SYSTEM_HIVE_NAME);    
    std::string SoftwareHiveName = std::string(VM_SOFTWARE_HIVE_NAME);

    std::string SrcSystemVolume, TgtMntPtForSrcSystemVolume;

	do
	{

		if(false == GetSrcNTgtSystemVolumes(strHostName, SrcSystemVolume, TgtMntPtForSrcSystemVolume))
        {
            DebugPrintf(SV_LOG_ERROR,"GetSrcNTgtSystemVolumes() failed for the source machine - %s\n", strHostName.c_str());
			rv = false; break;
        }
        DebugPrintf(SV_LOG_INFO,"System Volumes for %s : %s <==> %s", strHostName.c_str(), SrcSystemVolume.c_str(), TgtMntPtForSrcSystemVolume.c_str());

		//===========================================================================
		//Copy file to source VM's system registry data to Installation location of Master Target
		//This Part of Code is added for some debugging purpose
		std::string tgtSystemHivePath = TgtMntPtForSrcSystemVolume + std::string(VM_SYSTEM_HIVE_PATH); 
		std::string TargetPath = m_VxInstallPath + string("\\Failover\\Data\\RegistryFiles");

		DebugPrintf(SV_LOG_INFO,"Directory to Create - %s\n", TargetPath.c_str());
		if(FALSE == CreateDirectory(TargetPath.c_str(),NULL))
		{
			if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
			{
				DebugPrintf(SV_LOG_INFO,"Directory already exists. Considering it not as an Error.\n");            				
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to create Directory - %s. ErrorCode : [%lu].\n",TargetPath.c_str(),GetLastError());
			}
		}

		TargetPath = TargetPath + string("\\") + strHostName + string("_systemhive");
        DebugPrintf(SV_LOG_INFO,"Destination file - %s\n", TargetPath.c_str());
        if(0 == CopyFile(tgtSystemHivePath.c_str(), TargetPath.c_str(), FALSE))
        {            
            DebugPrintf(SV_LOG_ERROR,"Failed to copy the system hive file %s. GetLastError - %lu\n", strHostName.c_str(), GetLastError());
        }
        else
            DebugPrintf(SV_LOG_INFO, "Successfully Copied the system hive file.\n");

		//============================================================================

        if(false == PreRegistryChanges(TgtMntPtForSrcSystemVolume, SystemHiveName, SoftwareHiveName, bSystemHiveLoaded, bSoftwareHiveLoaded))
        {
            DebugPrintf(SV_LOG_ERROR,"PreRegistryChanges() failed for the source machine - %s\n", strHostName.c_str());
			rv = false; break;
        }

		if (false == MakeChangesForInmSvc(SystemHiveName, SoftwareHiveName))
        {
	        DebugPrintf(SV_LOG_ERROR,"Failed to modify changes in registry to convert the inmage services type to manual\n");
            DebugPrintf(SV_LOG_ERROR,"MakeChangesForInmSvc() failed for source machine - %s\n\n", strHostName.c_str());
	        rv = false; break;
        }       
        DebugPrintf(SV_LOG_INFO,"****** Made Changes for converting the inmage services type to manual for machine  - %s\n\n", strHostName.c_str());

	}while(0);

	if(false == PostRegistryChanges(TgtMntPtForSrcSystemVolume, SystemHiveName, SoftwareHiveName, bSystemHiveLoaded, bSoftwareHiveLoaded))
    {
        DebugPrintf(SV_LOG_ERROR,"PostRegistryChanges() Failed.\n");
        rv = false;
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
    return rv;
}

/*
Method: RemoveRegistryPathFromSystemHiveControlSets
	Method to remove the registry key and the sub folders underneath of it from control sets of system hive.

Inputs:
    SystemHiveName - System hive as input.
	RegPathToDelete - Registry path to delete from control sets of system hive.

Retrun: Returns true if the remove key operation completes successfully.
*/
bool ChangeVmConfig::RemoveRegistryPathFromSystemHiveControlSets(std::string SystemHiveName, std::string RegPathToDelete)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bReturnValue = true;
	HKEY hKey;
	long lRV;

	do
	{
		if (m_sysHiveControlSets.empty())
		{
			DebugPrintf(SV_LOG_ERROR, "Registry control sets information is empty for system hive [%s]. Discover the control sets by using method \"GetControlSets\".\n", SystemHiveName.c_str());
			bReturnValue = false;
			break;
		}
		std::vector<std::string>::iterator iterCS = m_sysHiveControlSets.begin();
		for (; iterCS != m_sysHiveControlSets.end(); iterCS++)
		{
			RegPathToDelete = SystemHiveName + string("\\") + (*iterCS) + string("\\") + RegPathToDelete;

			DebugPrintf(SV_LOG_DEBUG, "Removing registry keys from path [%s] in control set [%s].\n", RegPathToDelete.c_str(), (*iterCS).c_str());

			if (DeleteRegistryTree(RegPathToDelete))
			{
				DebugPrintf(SV_LOG_DEBUG, "Successfully removed Hyper-V IC software shadow copy provider registry keys in control set [%s].\n", (*iterCS).c_str());
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to remove Hyper-V IC software shadow copy provider registry keys in control set [%s].\n", (*iterCS).c_str());
				bReturnValue = false;
			}
		}
	} while (0);

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bReturnValue;
}


bool ChangeVmConfig::ResetInvolfltParameters( std::string hostName, std::string SystemHiveName, std::string SoftwareHiveName )
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
	bool bReturnValue = true;
	HKEY hKey;
	do
	{
		//Thinking that registry is already loaded.
		//Now we shall check if it is 32 bit or 64 bit machine.
		//If 64 bit machine we make our modifications.
		std::string RegPathToOpen = SoftwareHiveName + std::string("\\SV Systems\\VxAgent");
        long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_READ, &hKey);
        if( lRV == ERROR_SUCCESS )
        {
			//Mean to say this is 32 bit system.No need to change this.
			DebugPrintf( SV_LOG_DEBUG, "No need to tune involflt parameters for host %s." , hostName.c_str() );
			closeRegKey(hKey);
			bReturnValue = true;
			break;
		}
		else if ( lRV != ERROR_SUCCESS )
		{
            DebugPrintf( SV_LOG_DEBUG,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu]. Is it a 64bit system? Checking under Wow6432Node.\n", RegPathToOpen.c_str(), lRV);
            RegPathToOpen = SoftwareHiveName + std::string("\\Wow6432Node\\SV Systems\\VxAgent");
            lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_READ, &hKey);
            if( lRV != ERROR_SUCCESS )
            {
                DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu].\n", RegPathToOpen.c_str(), lRV);
                bReturnValue = false; break;
            }
			closeRegKey(hKey);

			//It is a 64 bit machine.Open SYSTEM HIVE. Modify below entries.
			//HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\involflt\Parameters\DataPoolSize
			//HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\involflt\Parameters\VolumeDataSizeLimit
			DebugPrintf( SV_LOG_DEBUG, "Host %s is a 64 bit system, tuning involflt parameters." , hostName.c_str() );
			std::vector<std::string> vecControlSets;
			if(false == GetControlSets(vecControlSets))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to get Control Sets vector for machine %s.\n" , hostName.c_str() );
				bReturnValue = false; break;
			}

			std::vector<std::string>::iterator iterCS = vecControlSets.begin();
			for(; iterCS != vecControlSets.end(); iterCS++)
			{
				DebugPrintf(SV_LOG_DEBUG, "Performing involflt tuining in control set [%s].\n", (*iterCS).c_str() );

				// In caspian v2, disk filter is introduced.
				// so when we failback to vmware, we need to change disk filter parameters.
				if (!m_strCloudEnv.empty() && (0 == strcmpi(m_strCloudEnv.c_str(), "vmware")))
				{
					RegPathToOpen = SystemHiveName + string("\\") + (*iterCS) + string("\\") + std::string("Services\\InDskFlt\\Parameters");
				}
				else {
					RegPathToOpen = SystemHiveName + string("\\") + (*iterCS) + string("\\") + std::string("Services\\involflt\\Parameters");
				}

				DebugPrintf(SV_LOG_DEBUG, "Registry path to open: %s \n", RegPathToOpen.c_str());
				lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_ALL_ACCESS, &hKey);
				if( lRV != ERROR_SUCCESS )
				{
					DebugPrintf(SV_LOG_ERROR, "RegOpenKeyEx failed for Path - [%s] and return code - [%lu].\n", RegPathToOpen.c_str(), lRV);
					DebugPrintf(SV_LOG_ERROR , "Failed to tune involflt parameters in control set [%s].\n", (*iterCS).c_str() );
					continue;
				}
				DWORD dwValue = 0;
				if(false == ChangeDWord( hKey, "DataPoolSize", dwValue ) )
				{
					closeRegKey(hKey);
					continue;
				}

				if(false == ChangeDWord( hKey, "VolumeDataSizeLimit", dwValue ) )
				{
					closeRegKey(hKey);
					continue;
				}
				closeRegKey(hKey);
				DebugPrintf( SV_LOG_DEBUG, "Successfully tuned involflt parameters in control set [%s].\n", (*iterCS).c_str() );
			}
        }
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
	return bReturnValue;
}

/*
This part of code is used to make changes post rollback incase of migration to cloud.
Supported Cloud/Cloud using this piece of code = Azure.(Add cloud providers as and they are using below code.)
*/
bool ChangeVmConfig::MakeChangesPostRollbackCloud(std::string HostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
    bool rv = true;

	m_bWinNT = false;
	bool bSystemHiveLoaded = false;
    bool bSoftwareHiveLoaded = false;
    
    std::string SystemHiveName = std::string(VM_SYSTEM_HIVE_NAME);    
    std::string SoftwareHiveName = std::string(VM_SOFTWARE_HIVE_NAME);

	std::string OsType, TgtMntPtForSrcSystemVolume, SrcSystemVolume;

	map<string, string>::iterator iter = m_mapOfVmInfoForPostRollback.find("SrcRespTgtSystemVolume");

	if(iter != m_mapOfVmInfoForPostRollback.end())
	{
		TgtMntPtForSrcSystemVolume = iter->second;
		DebugPrintf(SV_LOG_DEBUG,"Target System Volume : %s\n",TgtMntPtForSrcSystemVolume.c_str());
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Post rollback cloud changes not possible as source respective target system volume  not found for host : %s\n",HostName.c_str());	
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
		return false;
	}

	iter = m_mapOfVmInfoForPostRollback.find("SrcSystemVolume");
	if(iter != m_mapOfVmInfoForPostRollback.end())
	{
		SrcSystemVolume = iter->second;
		DebugPrintf(SV_LOG_DEBUG,"Source System Volume : %s\n",SrcSystemVolume.c_str());
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Post rollback cloud changes not possible as system volume  not found for host : %s\n",HostName.c_str());	
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
		return false;
	}

	iter = m_mapOfVmInfoForPostRollback.find("OperatingSystem");
	if(iter != m_mapOfVmInfoForPostRollback.end())
	{
		OsType = iter->second;
		DebugPrintf(SV_LOG_DEBUG,"OS Type : %s\n",OsType.c_str());
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Post rollback cloud changes not possible as Operating system not found for host : %s\n",HostName.c_str());	
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
		return false;
	}

	bool bW2K8 = false;
	bool bW2012 = false;
    bool bW2K8R2 = false;

    if((OsType.find("2008") != std::string::npos) ||  (OsType.find("7") != std::string::npos) || (OsType.find("Vista") != std::string::npos))        
    {
        DebugPrintf(SV_LOG_DEBUG,"OsType - %s \n",OsType.c_str());
        bW2K8 = true;     
        if((OsType.find("R2") != std::string::npos) || (OsType.find("7") != std::string::npos))
        {
            bW2K8R2 = true;        
        }
    }
	else if((OsType.find("2012") != std::string::npos) ||(OsType.find("8") != std::string::npos))
	{
		bW2012 = true;
	}

	do
	{
		//===========================================================================
		//Copy file to source VM's system registry data to Installation location of Master Target
		//This Part of Code is added for some debugging purpose
		std::string tgtSystemHivePath = TgtMntPtForSrcSystemVolume + std::string(VM_SYSTEM_HIVE_PATH); 
		std::string TargetPath = m_VxInstallPath + string("\\Failover\\Data\\RegistryFiles");

		DebugPrintf(SV_LOG_DEBUG,"Directory to Create - %s\n", TargetPath.c_str());
		if(FALSE == CreateDirectory(TargetPath.c_str(),NULL))
		{
			if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
			{
				DebugPrintf(SV_LOG_DEBUG,"Directory already exists. Considering it not as an Error.\n");            				
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to create Directory - %s. ErrorCode : [%lu].\n",TargetPath.c_str(),GetLastError());
			}
		}

		TargetPath = TargetPath + string("\\") + HostName + string("_systemhive");
        DebugPrintf(SV_LOG_INFO,"Destination file - %s\n", TargetPath.c_str());
        if(0 == CopyFile(tgtSystemHivePath.c_str(), TargetPath.c_str(), FALSE))
        {            
            DebugPrintf(SV_LOG_ERROR,"Failed to copy the system hive file %s. GetLastError - %lu\n", HostName.c_str(), GetLastError());
        }
        else
            DebugPrintf(SV_LOG_DEBUG, "Successfully Copied the system hive file.\n");

		//============================================================================

		//Write inmage_drive.txt file, this will be used to re-assign driver letters in case needed.
		//TFS ID : 1051609
		if (false == MakeChangesForDrive(HostName, SystemHiveName, SoftwareHiveName, SrcSystemVolume, TgtMntPtForSrcSystemVolume))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to create the drives map file by using registry mount device info.\n");
			DebugPrintf(SV_LOG_ERROR, "MakeChangesForDrive() failed for source machine - %s\n\n", HostName.c_str());
		}

		if(false == PreRegistryChanges(TgtMntPtForSrcSystemVolume, SystemHiveName, SoftwareHiveName, bSystemHiveLoaded, bSoftwareHiveLoaded))
		{
			DebugPrintf(SV_LOG_ERROR,"PreRegistryChanges() failed for the source machine - %s\n", HostName.c_str());
			rv = false; break;
		}

		//Converts the intelide related boot services start type to 0
		//This is added as part of BUG - 1313149
		if (false == MakeChangesForBootDrivers(TgtMntPtForSrcSystemVolume, SystemHiveName, SoftwareHiveName))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to modify changes in registry for the startup drivers\n");
			DebugPrintf(SV_LOG_ERROR, "MakeChangesForBootDrivers() failed for source machine - %s\n\n", HostName.c_str());
			rv = false;
		}
		else
			DebugPrintf(SV_LOG_INFO, "****** Successfully made changes for startup drivers during machine bootup for host  - %s\n\n", HostName.c_str());

		//Converts the vxagnet and fx agnet service status to manual in the source registry.
		if (false == MakeChangesForInmSvc(SystemHiveName, SoftwareHiveName))
        {
	        DebugPrintf(SV_LOG_ERROR,"Failed to modify changes in registry to convert the inmage services type to manual\n");
            DebugPrintf(SV_LOG_ERROR,"MakeChangesForInmSvc() failed for source machine - %s\n\n", HostName.c_str());
	        rv = false;
        }
		else
			DebugPrintf(SV_LOG_INFO,"****** Made Changes for converting the inmage services type to manual for machine  - %s\n\n", HostName.c_str());


		// Update the registry of source machine to run the script on next boot
		if(false == RegAddStartupExeOnBoot(SoftwareHiveName, SrcSystemVolume, bW2K8||bW2012, bW2K8R2||bW2012))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to add registry entries for running an executable on VM boot for VM - %s\n", HostName.c_str());
			rv = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"****** Made script Changes to run on VM boot for host - %s\n\n", HostName.c_str());
		}

		if ( false == ResetInvolfltParameters( HostName , SystemHiveName, SoftwareHiveName ) )
		{
			//Load registry and try this.Hope that after unload it is removed.
			DebugPrintf( SV_LOG_ERROR , "Failed to tune involflt parameters for host %s.", HostName.c_str() );
			//NO NEED TO RETURN ERROR IN THIS CASE.SHALL WE TRIGGER WARNING FOR THIS INCASE ??
		}

		//AWS Specific changes
		if( stricmp(m_strCloudEnv.c_str(),"aws") == 0 &&
			bW2012)
		{
			std::string strRegKeyXenTools = SoftwareHiveName + XEN_TOOLS_REG_KEY;
			HKEY hKeyXenTools = NULL;
			LONG lRegApiRet  = 0;
			DWORD dwMajor    = 0,
				  dwMinor    = 0;
			lRegApiRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE,strRegKeyXenTools.c_str(),0,KEY_READ,&hKeyXenTools);
			if(lRegApiRet != ERROR_SUCCESS)
			{
				DebugPrintf(SV_LOG_ERROR,"Error[%d] opening xen tools reg key\n",lRegApiRet);
				DebugPrintf(SV_LOG_ERROR,"6.0 Xen-Tools are not found for the host[%s]. Recovered Instance may not boot on AWS Environment\n",HostName.c_str());
				rv = false;
			}
			else if ( RegGetValueOfTypeDword(hKeyXenTools,"MajorVersion",dwMajor) && 
				      RegGetValueOfTypeDword(hKeyXenTools,"MinorVersion",dwMinor))
			{
				if( 6 == dwMajor && 0 == dwMinor)
				{
					if(false == MakeRegChangesForW2012AWS(SystemHiveName))
					{
						DebugPrintf( SV_LOG_ERROR , "Failed to make Windows 2012 Server registry changes for host [%s].", HostName.c_str() );
						rv = false;
					}
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"%d.%d Xen tools installed on the host [%s]. This version is not supported.\n",dwMajor,dwMinor,HostName.c_str());
					DebugPrintf(SV_LOG_DEBUG,"Supported Xen tools version : 6.0\n");
					rv = false;
				}
				RegCloseKey(hKeyXenTools);
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Faile to get the xen tools version for the host [%s]\n",HostName.c_str());
				rv = false;
			}
		}

	}while(0);

	if(false == PostRegistryChanges(TgtMntPtForSrcSystemVolume, SystemHiveName, SoftwareHiveName, bSystemHiveLoaded, bSoftwareHiveLoaded))
    {
        DebugPrintf(SV_LOG_ERROR,"PostRegistryChanges() Failed.\n");
        rv = false;
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
	return rv;
}


//***************************************************************************************
// Make the Registry Changes required for P2V or NW based on input bP2VChanges=true/false
// First get the machine type and ostype
//  Input - HostName
//  Return true if successfully changes.
//***************************************************************************************
bool ChangeVmConfig::MakeChangesPostRollback(std::string HostName, bool bP2VChanges, std::string& OsType)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
    bool rv = true;

    std::string MachineType; // This will be either VirtualMachine or PhysicalMachine
	std::string strFailback;
    
    if(false == xGetMachineAndOstypeAndV2P(HostName, OsType, MachineType, strFailback))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to Machine Type and OsType for %s\n", HostName.c_str());
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
        return false;
    }
    DebugPrintf(SV_LOG_INFO,"OsType - %s  ,  MachineType - %s\n", OsType.c_str(), MachineType.c_str());

	if(MachineType.compare("PhysicalMachine") == 0)
	{
		if(strFailback.compare("yes") == 0)
			m_p2v = false;
		else
			m_p2v = true;
	}
	else
	{
		m_p2v = false;
	}
    if((bP2VChanges) && (MachineType.compare("PhysicalMachine") != 0)) 
    {
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
        return rv; // If not PhysicalMachine, return true i.e. skip P2V changes.
    }

	// Physical machine and P2V= true, considering this as V2P scenario
	if(bP2VChanges && (MachineType.compare("PhysicalMachine") == 0) && (strFailback.compare("yes") == 0))
	{
		//Currently igonring the failure case
		if(false == RevertSrvStatusToOriginal(HostName))
			DebugPrintf(SV_LOG_ERROR,"Failed  to revert some of the service status to orginal which were modified during P2V\n");
		else
			DebugPrintf(SV_LOG_INFO,"Successfully reverted the service status to orginal which were modified during P2V\n");	

		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
        return rv;
	}
    
    m_bWinNT = false;   
    bool bSystemHiveLoaded = false;
    bool bSoftwareHiveLoaded = false;
    
    std::string SystemHiveName = std::string(VM_SYSTEM_HIVE_NAME);    
    std::string SoftwareHiveName = std::string(VM_SOFTWARE_HIVE_NAME);

    std::string SrcSystemVolume, TgtMntPtForSrcSystemVolume;

    do
    {
		if(false == GetSrcNTgtSystemVolumes(HostName, SrcSystemVolume, TgtMntPtForSrcSystemVolume))
		{
			DebugPrintf(SV_LOG_ERROR,"GetSrcNTgtSystemVolumes() failed for the source machine - %s\n", HostName.c_str());
			return false;
		}
		DebugPrintf(SV_LOG_INFO,"System Volumes for %s : %s <==> %s", HostName.c_str(), SrcSystemVolume.c_str(), TgtMntPtForSrcSystemVolume.c_str());

		if(false == PreRegistryChanges(TgtMntPtForSrcSystemVolume, SystemHiveName, SoftwareHiveName, bSystemHiveLoaded, bSoftwareHiveLoaded))
        {
            DebugPrintf(SV_LOG_ERROR,"PreRegistryChanges() failed for the source machine - %s\n", HostName.c_str());
			rv = false; break;
        }

        if(bP2VChanges)
        {
            if(false == MakeChangesForP2V(HostName, OsType, SystemHiveName, SrcSystemVolume, TgtMntPtForSrcSystemVolume, SoftwareHiveName))
	        {
		        DebugPrintf(SV_LOG_ERROR,"Failed to modify changes in registry to convert Physical Machine to Virtual Machine.\n");
                DebugPrintf(SV_LOG_ERROR,"MakeChangesForP2V() failed for source machine - %s\n\n", HostName.c_str());
		        rv = false; break;
	        }       
            DebugPrintf(SV_LOG_INFO,"****** Made Changes required to convert the Physical Machine to Virtual Machine for - %s\n\n", HostName.c_str());

			//TFS ID : 1547582.
			if (!m_bDrDrill)
			{
				if (false == MakeChangesForInmSvc(SystemHiveName, SoftwareHiveName))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to modify changes in registry to convert the inmage services type to manual\n");
					DebugPrintf(SV_LOG_ERROR, "MakeChangesForInmSvc() failed for source machine - %s\n\n", HostName.c_str());
					rv = false; break;
				}
			}

			if(false == SetServicesToManual(HostName, SystemHiveName, SoftwareHiveName))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to set the services to manual in registry for host : %s.\n", HostName.c_str());
			}
			
			if(false == MakeChangesForDrive(HostName, SystemHiveName, SoftwareHiveName, SrcSystemVolume, TgtMntPtForSrcSystemVolume))
	        {
		        DebugPrintf(SV_LOG_ERROR,"Failed to create the drives map file by using registry mount device info.\n");
                DebugPrintf(SV_LOG_ERROR,"MakeChangesForDrive() failed for source machine - %s\n\n", HostName.c_str());
	        }

			DWORD startValue	= 2;
			if ( false == SetVMwareToolsStartType( SystemHiveName , startValue ) )
			{
				//Not Considering its return value for now.
			}

        }
        else
        {
            if(false == MakeChangesForNW(HostName, OsType, SystemHiveName, SoftwareHiveName, SrcSystemVolume, TgtMntPtForSrcSystemVolume))
	        {
		        DebugPrintf(SV_LOG_ERROR,"Failed to modify changes in registry to configure Network settings when recovered VM boots.\n");
                DebugPrintf(SV_LOG_ERROR,"MakeChangesForNW() failed for source machine - %s\n\n", HostName.c_str());
		        rv = false;
	        }
			else
			{
				DebugPrintf(SV_LOG_INFO,"****** Made Changes required to configure Network settings on VM boot for the VM - %s\n\n", HostName.c_str());
			}

			if ( false == ResetInvolfltParameters( HostName , SystemHiveName, SoftwareHiveName ) )
			{
				//Load registry and try this.Hope that after unload it is removed.
				DebugPrintf( SV_LOG_ERROR , "Failed to tune involflt parameters for host %s.", HostName.c_str() );
				//NO NEED TO RETURN ERROR IN THIS CASE.SHALL WE TRIGGER WARNING FOR THIS INCASE ??
			}
        }

    }while(0);

    if(false == PostRegistryChanges(TgtMntPtForSrcSystemVolume, SystemHiveName, SoftwareHiveName, bSystemHiveLoaded, bSoftwareHiveLoaded))
    {
        DebugPrintf(SV_LOG_ERROR,"PostRegistryChanges() Failed.\n");
        rv = false;
    }    

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
    return rv;
}

//***************************************************************************************
// For the given source HostName, get its system volume's drive letter(SrcSysVol) 
// and its corresponding target mount point on MT (TgtMntPtForSrcSystemVolume).
//***************************************************************************************
bool ChangeVmConfig::GetSrcNTgtSystemVolumes(std::string HostName, std::string & SrcSystemVolume, std::string & TgtMntPtForSrcSystemVolume)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
    bool rv = true;

    do
    {
        // Get the map of all pairs which we set for vx replication		
        std::map<std::string, std::string> mapOfSrcToTgtVolumes;
		if(false == GetMapOfSrcToTgtVolumes(HostName, mapOfSrcToTgtVolumes))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to get map of source and target volumes for the source machine - %s\n", HostName.c_str());
			rv = false; break;
		}

        // Get the source vm's volume drive letter which contains "system32" path
		if(xGetSrcSystemVolume(HostName, SrcSystemVolume))
		{
			DebugPrintf(SV_LOG_INFO,"Got the system volume from XML file is : %s\n", SrcSystemVolume.c_str());	
		}
		else if(GetSrcSystemVolume(HostName, SrcSystemVolume))
		{
			DebugPrintf(SV_LOG_INFO,"Got the system volume from sysvolume file of source is : %s\n", SrcSystemVolume.c_str());				
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to get the System Volume of the source machine - %s\n", HostName.c_str());
			rv = false; break;	
		}

		//Get the corresponding target volume Mount Point for system drive.
        std::map<std::string, std::string>::iterator iter = mapOfSrcToTgtVolumes.find(SrcSystemVolume);
		if(iter == mapOfSrcToTgtVolumes.end())
		{
            DebugPrintf(SV_LOG_ERROR,"System volume is not found in protected pairs for the source machine - %s\n", HostName.c_str());
		    rv = false; break;
		}
		else
		{                				
			TgtMntPtForSrcSystemVolume = iter->second;				
		}		
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
    return rv;
}


bool ChangeVmConfig::GetMapVolumeGuidAndChecksum(std::string SystemHiveName, map<string, string>& mapChecksumAndVolGuid)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	DWORD retCode = 0;
	HKEY hKey;

	std::string regMountDevice = SystemHiveName + string("\\MountedDevices");
	retCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE,regMountDevice.c_str(),0,KEY_ALL_ACCESS,&hKey);

	if(ERROR_SUCCESS == retCode)
	{
		DebugPrintf(SV_LOG_DEBUG, "Successfully opened the key SYSTEM\\MountedDevices. \n");

		DWORD    cValues;              // number of values for key 
		DWORD    cchMaxValue;          // longest value name  
		DWORD    i;
	 
		TCHAR achValue[MAX_VALUE_NAME]; 
		DWORD cchValue = MAX_VALUE_NAME;

		// Get the value count. 
		retCode = RegQueryInfoKey(
			hKey,           // key handle 
			NULL,           // buffer for class name 
			NULL,           // size of class string 
			NULL,           // reserved 
			NULL,           // number of subkeys 
			NULL,           // longest subkey size 
			NULL,           // longest class string 
			&cValues,       // number of values for this key 
			&cchMaxValue,   // longest value name 
			NULL,			// longest value data 
			NULL,			// security descriptor 
			NULL);			// last write time 

		// Enumerate the key values.
		DWORD lpType = REG_BINARY;
		if (cValues) 
		{
			for (i=0, retCode=ERROR_SUCCESS; i<cValues; i++) 
			{ 
				//allocate enough memory for the data variable.
				
				cchValue = MAX_VALUE_NAME; 
				achValue[0] = '\0'; 
				retCode = RegEnumValue(hKey, i, 
										achValue, 
										&cchValue,
										NULL,
										&lpType,
										NULL,
										NULL);
	 
				if (retCode == ERROR_SUCCESS ) 
				{
					/*wstring wstr(achValue);
					string str(wstr.begin(), wstr.end());*/
					string KeyToFetch = string(achValue);
					if(KeyToFetch.find("Volume") != string::npos)
					{
						string strChecksum;
						DWORD dwSize;											
						retCode = RegQueryValueEx(hKey, achValue, NULL, &lpType, NULL, &dwSize);
						if(retCode != ERROR_SUCCESS)
						{
							DebugPrintf(SV_LOG_ERROR,"RegQueryValueEx failed to fetch the type and size for Key - [%s] and return code - [%lu].\n", KeyToFetch.c_str(), retCode);
							continue;
						}

						char *cValue = new char [dwSize/sizeof(char)];
						retCode = RegQueryValueEx(hKey, achValue, NULL, &lpType, (PBYTE)cValue, &dwSize);
						if( retCode == ERROR_SUCCESS )
						{
							//Need to find out the checksum of the bianry value.
							unsigned char CheckSum[16];
							INM_MD5_CTX md5ctx;
							INM_MD5Init(&md5ctx);
							INM_MD5Update(&md5ctx, (unsigned char *)cValue, dwSize);
							INM_MD5Final(CheckSum, &md5ctx);
							strChecksum = GetBytesInArrayAsHexString(CheckSum, 16);
							DebugPrintf(SV_LOG_DEBUG,"Volume - %s, Checksum - %s.\n",KeyToFetch.c_str(), strChecksum.c_str());
							delete [] cValue;
						}
						else
						{
							delete [] cValue;
							DebugPrintf(SV_LOG_ERROR,"RegQueryValueEx failed at Path - [%s], Key - [%s] and return code - [%lu].\n", regMountDevice.c_str(), KeyToFetch.c_str(), retCode);
							continue;
						}

						unsigned int found = KeyToFetch.find("Volume");
						KeyToFetch = KeyToFetch.substr(found);
						KeyToFetch = string("\\\\?\\") + KeyToFetch;
						mapChecksumAndVolGuid.insert(make_pair(strChecksum, KeyToFetch));
					}
				} 
				else
				{
					DebugPrintf(SV_LOG_ERROR,"RegEnumValue failed at Path - [%s], return code - [%lu].\n", regMountDevice.c_str(), retCode);
					rv = false;
				}
				lpType = REG_BINARY;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG,"No keys found in path : [%s] and return code : [%lu]\n", regMountDevice.c_str(), retCode);
		}		
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s], return code - [%lu].\n", regMountDevice.c_str(), retCode);
		rv = false;
	}
	if(hKey != NULL)
	{
		closeRegKey(hKey);
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
    return rv;
}


bool ChangeVmConfig::GetMapofVolGuidToVolName(std::string SystemHiveName, map<string, string> mapChecksumAndVolGUID, 
							  map<string, string> mapOfSrcTgtVol, map<string, string>& mapOfVolGuidAndName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	HKEY hKey;
	DWORD retCode = 0;
	std::string regMountDevice = SystemHiveName + string("\\MountedDevices");
	retCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE,regMountDevice.c_str(),0,KEY_ALL_ACCESS,&hKey);

	if(ERROR_SUCCESS == retCode)
	{
		map<string, string>::iterator iterMapVol = mapOfSrcTgtVol.begin();
		map<string, string>::iterator iterChecksum;
		for(;iterMapVol != mapOfSrcTgtVol.end(); iterMapVol++)
		{
			string strSrcVol;
			if(iterMapVol->first.length() <= 3)
			{
				strSrcVol = iterMapVol->first.substr(0, iterMapVol->first.length()-1);
				strSrcVol = string("\\DosDevices\\") + strSrcVol;

				string strChecksum;
				DWORD dwSize, retCode = 0;
				DWORD lpType = REG_BINARY;

				retCode = RegQueryValueEx(hKey, strSrcVol.c_str(), NULL, &lpType, NULL, &dwSize);
				if(retCode != ERROR_SUCCESS)
				{
					DebugPrintf(SV_LOG_ERROR,"RegQueryValueEx failed to fetch the type and size for Key - [%s] and return code - [%lu].\n", strSrcVol.c_str(), retCode);
					continue;
				}

				char *cValue = new char [dwSize/sizeof(char)];
				retCode = RegQueryValueEx(hKey, strSrcVol.c_str(), NULL, &lpType, (PBYTE)cValue, &dwSize);
				if( retCode == ERROR_SUCCESS )
				{
					//Need to find out the checksum of the bianry value.
					unsigned char CheckSum[16];
					INM_MD5_CTX md5ctx;
					INM_MD5Init(&md5ctx);
					INM_MD5Update(&md5ctx, (unsigned char *)cValue, dwSize);
					INM_MD5Final(CheckSum, &md5ctx);
					strChecksum = GetBytesInArrayAsHexString(CheckSum, 16);
					DebugPrintf(SV_LOG_DEBUG,"Volume - %s, Checksum - %s.\n",strSrcVol.c_str(), strChecksum.c_str()); 

					iterChecksum = mapChecksumAndVolGUID.find(strChecksum);
					if(iterChecksum != mapChecksumAndVolGUID.end())
					{
						mapOfVolGuidAndName.insert(make_pair(iterChecksum->second, iterMapVol->first));
					}
					else
					{
						DebugPrintf(SV_LOG_WARNING,"Volume - %s not found a matched volume GUID with Checksum - %s.\n",strSrcVol.c_str(), strChecksum.c_str());
					}
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"RegQueryValueEx failed at Path - [%s], Key - [%s] and return code - [%lu].\n", regMountDevice.c_str(), strSrcVol.c_str(), retCode);
				}
				delete [] cValue;
			}
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s], return code - [%lu].\n", regMountDevice.c_str(), retCode);
		rv = false;
	}
	if(hKey != NULL)
	{
		closeRegKey(hKey);
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
    return rv;
}


/*
    Write drive info in the file in following format for each drive (Separator - !@!@!)
    VolumeGuid!@!@!VolumeName!@!@!
*/
bool ChangeVmConfig::WriteDriveInfoMapToFile(map<string, string> mapOfVolGuidToName, std::string localDriveFile, std::string recDriveFile)
{
    bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	FILE *fPtr;
	fPtr = fopen(recDriveFile.c_str(), "w");

	if(fPtr == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s with error %d\n",recDriveFile.c_str(), GetLastError());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
	}
	else
	{
		std::stringstream data;
		int fd = fileno(fPtr);

		DebugPrintf(SV_LOG_INFO,"Data to Enter in file %s are:\n", recDriveFile.c_str());
		for(map<string, string>::iterator i = mapOfVolGuidToName.begin(); i != mapOfVolGuidToName.end(); i++)
		{
			DebugPrintf(SV_LOG_INFO,"%s!@!@!%s!@!@!\n", i->first.c_str(), i->second.c_str());

			data<<i->first<<"!@!@!"
              <<i->second<<"!@!@!"<<endl;
		}
		fwrite(data.str().c_str(),data.str().size(),1,fPtr);
		fflush(fPtr);

		if(fd>0)
		{
			if(_commit(fd) < 0)
			{
				DebugPrintf(SV_LOG_ERROR,"Encountered error while trying to flush the filesystem for the file %s. Failed with error: %d\n",recDriveFile.c_str(),errno);
				rv = false;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Encountered error while trying to open the file %s. Failed with error: %d\n",recDriveFile.c_str(),errno);
			rv = false;
		}

		fclose(fPtr);
	}	

	
	ofstream FOUT(localDriveFile.c_str(), std::ios::out | std::ios::app);
    if(! FOUT.is_open())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s with Error %d\n",localDriveFile.c_str(), GetLastError());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
	for(map<string, string>::iterator i = mapOfVolGuidToName.begin(); i != mapOfVolGuidToName.end(); i++)
	{
		FOUT<<i->first<<"!@!@!"
			<<i->second<<"!@!@!"<<endl;
	}
    FOUT.close();

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


bool ChangeVmConfig::GetMapofVolGuidToVolNameFromMbrFile(std::string HostName, map<string, string>& mapOfSrcTgtVol, map<string, string>& mapOfVolGuidAndName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	mapOfVolGuidAndName.clear();

	string strMbrFile = m_VxInstallPath + string("\\Failover") + string("\\data\\") + HostName + string("_mbrdata");

	if((true == checkIfFileExists(strMbrFile)) && (m_vConVersion >= 4.0))
	{
		DisksInfoMap_t srcMapOfDiskNamesToDiskInfo;
		DLM_ERROR_CODE RetVal = ReadDiskInfoMapFromFile(strMbrFile.c_str(), srcMapOfDiskNamesToDiskInfo);
		if(DLM_ERR_SUCCESS != RetVal)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to collect the Source disks info map from the BLOB file(%s). Error Code - %u\n", strMbrFile.c_str(), RetVal);
			rv = false;
		}
		else
		{
			DisksInfoMapIter_t iter = srcMapOfDiskNamesToDiskInfo.begin();
			for(; iter != srcMapOfDiskNamesToDiskInfo.end(); iter++)
			{
				VolumesInfoVecIter_t iterVol = iter->second.VolumesInfo.begin();
				for(; iterVol != iter->second.VolumesInfo.end(); iterVol++)
				{
					string strVolGuid = string(iterVol->VolumeGuid);
					string strVolName = string(iterVol->VolumeName);
					if(mapOfVolGuidAndName.find(strVolGuid) == mapOfVolGuidAndName.end())
					{
						DebugPrintf(SV_LOG_DEBUG,"Volume GUID : %s  ==>  Volume name : %s.\n", strVolGuid.c_str(), strVolName.c_str());
						mapOfVolGuidAndName.insert(make_pair(strVolGuid, strVolName));
					}
				}
			}
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"File(%s) does not found, trying alternate way to create the volumes map\n", strMbrFile.c_str());
		rv = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
    return rv;
}

//Reverts the service status to its original state which were changed during P2V
bool ChangeVmConfig::RevertSrvStatusToOriginal(string& HostName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	m_bWinNT = false;   
    bool bSystemHiveLoaded = false;
    bool bSoftwareHiveLoaded = false;
    
    std::string SystemHiveName = std::string(VM_SYSTEM_HIVE_NAME);    
    std::string SoftwareHiveName = std::string(VM_SOFTWARE_HIVE_NAME);

    std::string SrcSystemVolume, TgtMntPtForSrcSystemVolume;

    do
    {
		if(false == GetSrcNTgtSystemVolumes(HostName, SrcSystemVolume, TgtMntPtForSrcSystemVolume))
		{
			DebugPrintf(SV_LOG_ERROR,"GetSrcNTgtSystemVolumes() failed for the source machine - %s\n", HostName.c_str());
			return false;
		}
		DebugPrintf(SV_LOG_INFO,"System Volumes for %s : %s <==> %s", HostName.c_str(), SrcSystemVolume.c_str(), TgtMntPtForSrcSystemVolume.c_str());

		if(false == PreRegistryChanges(TgtMntPtForSrcSystemVolume, SystemHiveName, SoftwareHiveName, bSystemHiveLoaded, bSoftwareHiveLoaded))
        {
            DebugPrintf(SV_LOG_ERROR,"PreRegistryChanges() failed for the source machine - %s\n", HostName.c_str());
			rv = false; break;
        }

		if(false == SetServicesToOriginalType(HostName, SystemHiveName, SoftwareHiveName))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to set some of the services to manual in registry for host : %s.\n", HostName.c_str());
		}

	}while(0);

    if(false == PostRegistryChanges(TgtMntPtForSrcSystemVolume, SystemHiveName, SoftwareHiveName, bSystemHiveLoaded, bSoftwareHiveLoaded))
    {
        DebugPrintf(SV_LOG_ERROR,"PostRegistryChanges() Failed.\n");
        rv = false;
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
    return rv;
}


bool ChangeVmConfig::SetServicesToOriginalType(string& HostName, string& SystemHiveName, string& SoftwareHiveName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	string strListManualSrv;
	string strRegvConSrv = "vConManualSrv";
	vector<string> vecControlSets, lstSrvManual;

	do
	{
		if(false == GetControlSets(vecControlSets))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to get Control Sets vector for this VM.\n");
			rv = false; break;
		}
		if (false == RegGetKeyFromInstallDir(SoftwareHiveName, strRegvConSrv, strListManualSrv))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to get the services list which are marked manual in registry during P2V for host : [%s]\n", HostName.c_str());
			rv = false; break;
		}

		//Separate the services list with "!@!@!"
		string Separator = "!@!@!";
		size_t pos = 0;
		size_t found = strListManualSrv.find(Separator, pos);
		while(found != string::npos)
		{
			DebugPrintf(SV_LOG_INFO,"Service with status - [%s]\n",strListManualSrv.substr(pos,found-pos).c_str());
			lstSrvManual.push_back(strListManualSrv.substr(pos,found-pos));
			pos = found + 5; 
			found = strListManualSrv.find(Separator, pos);            
		}
		DebugPrintf(SV_LOG_INFO,"Service with status- [%s]\n",strListManualSrv.substr(pos, strListManualSrv.length()).c_str());
		lstSrvManual.push_back(strListManualSrv.substr(pos, strListManualSrv.length()));

		vector<string>::iterator iterCtrl, iterSrv;
		for(iterSrv = lstSrvManual.begin(); iterSrv != lstSrvManual.end(); iterSrv++)
		{
			string strSrv, strOrigType;
			DWORD dOrigType = 2;
			Separator = ":";
			pos = iterSrv->find_first_of(Separator);
			if(pos != string::npos)
			{
				strSrv = iterSrv->substr(0,pos);
				strOrigType = iterSrv->substr(pos+(Separator.length()),iterSrv->length());
				stringstream ss;
				ss << strOrigType;
				ss >> dOrigType;
			}
			DebugPrintf(SV_LOG_INFO,"Service - [%s], Original Status : %lu. \n",strSrv.c_str(), dOrigType);

			for(iterCtrl = vecControlSets.begin(); iterCtrl != vecControlSets.end(); iterCtrl++)
			{
				//Making services manual
				if (false == ChangeServiceStartType(SystemHiveName, strSrv, dOrigType, *iterCtrl))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to change the start type for service : [%s].\n", strSrv.c_str());
					rv = false;
				}
			}
		}
		if(rv)
		{
			HKEY hKey;
			std::string regDelPath = SoftwareHiveName + std::string("\\SV Systems\\VxAgent");
			long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, regDelPath.c_str(), 0, KEY_ALL_ACCESS, &hKey);
			if( lRV != ERROR_SUCCESS )
			{
				DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu]. Trying alternate Path.\n", regDelPath.c_str(), lRV);
				regDelPath = SoftwareHiveName + std::string("\\Wow6432Node\\SV Systems\\VxAgent");
				lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, regDelPath.c_str(), 0, KEY_ALL_ACCESS, &hKey);
				if( lRV != ERROR_SUCCESS )
				{
					DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu].\n", regDelPath.c_str(), lRV);
					break;
				}
			}

			//Delete the Key "vConManualSrv"
			lRV = RegDeleteValue(hKey, strRegvConSrv.c_str());
			if( lRV != ERROR_SUCCESS )
			{
				DebugPrintf(SV_LOG_ERROR,"RegDeleteValues failed for path - %s and for value - vConManualSrv, return code - [%lu].\n", regDelPath.c_str(), lRV);
			}
			closeRegKey(hKey);
		}
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
    return rv;
}


//Sets the servioces to manual which were reported in xml file
//Also updates the services which were marked to manual in the regitry
bool ChangeVmConfig::SetServicesToManual(string& HostName, string& SystemHiveName, string& SoftwareHiveName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	string strFinalSrvManual;
	std::vector<std::string> vecControlSets, lstSrvManual;
    if(false == GetControlSets(vecControlSets))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get Control Sets vector for this VM.\n");
        DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
        return false;
    }

	if (false == xGetListOfSrvToSetManual(HostName, lstSrvManual))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to get the list of services to set manual for host : %s.\n", HostName.c_str());
        DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
        return false;
	}

	std::vector<std::string>::iterator iterCS, iterSrv;
    for(iterSrv = lstSrvManual.begin(); iterSrv != lstSrvManual.end(); iterSrv++)
    {
		DebugPrintf(SV_LOG_INFO , "Service to set manual [%s].\n", (*iterSrv).c_str() );
		for(iterCS = vecControlSets.begin(); iterCS != vecControlSets.end(); iterCS++)
		{
			DebugPrintf(SV_LOG_INFO , "Performing registry in control set [%s].\n", (*iterCS).c_str() );

			string RegPathToOpen = SystemHiveName + string("\\") + *iterCS + string("\\Services\\") + *iterSrv;
			DebugPrintf(SV_LOG_INFO, "Registry path to open to get service status: %s \n", RegPathToOpen.c_str());
			
			HKEY hKey;
			DWORD dStartType = 3;
			long lRV = RegOpenKeyEx( HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_ALL_ACCESS , &hKey);
			if( ERROR_FILE_NOT_FOUND == lRV )
			{
				if(hKey != NULL)
				{
					closeRegKey(hKey);
				}
				DebugPrintf(SV_LOG_INFO,"Registry Path %s doesn't exist in control set [%s]...Trying to modify in next control set.\n", RegPathToOpen.c_str(), (*iterCS).c_str() );
			}
			else if( lRV != ERROR_SUCCESS )
			{
				if(hKey != NULL)
				{
					closeRegKey(hKey);
				}
				DebugPrintf(SV_LOG_INFO,"Failed to open registry path [%s] with error code %ld...Trying to modify in next control set.\n", RegPathToOpen.c_str(), lRV );
			}
			else
			{
				if( false == RegGetValueOfTypeDword(hKey, "start", dStartType) )
				{
					DebugPrintf(SV_LOG_WARNING,"[WARNING]RegGetValueOfTypeDword failed : Path - [%s].\n", RegPathToOpen.c_str());
				}
				closeRegKey(hKey);
			}

			if(2 == dStartType || 1 == dStartType)
			{
				//Making services manual
				if (false == ChangeServiceStartType(SystemHiveName, *iterSrv, 3, *iterCS))
				{
					DebugPrintf(SV_LOG_WARNING, "Failed to change the start type for service : [%s].\n", iterSrv->c_str());
					rv = false;
				}
				else
				{
					stringstream ss;
					ss << dStartType;
					string strService = *iterSrv + ":" + ss.str();

					if(strFinalSrvManual.empty())
						strFinalSrvManual = strService;
					else
					{
						if(strFinalSrvManual.find(*iterSrv) == string::npos)
							strFinalSrvManual = strFinalSrvManual + "!@!@!" + strService;
					}
				}
			}
		}
	}

	if(!strFinalSrvManual.empty())
	{
		if (false == RegSetKeyAtInstallDir(SoftwareHiveName, "vConManualSrv", strFinalSrvManual))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to update the services list which are marked manual in registry for host : [%s]\n", HostName.c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Successfully updated the services list which are marked manual in registry for host : [%s]\n", HostName.c_str());
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
    return rv;
}


//Sets the services start type which are assigned in map.
//Also updates the services earlier status in their registry. It can be useful for future to revert back the status
//Input : System hive where the service type need to update
//        software hive where old details need update (creates a key "SrvOldStartType" under SvAgents folder. Value will be under format <srv name1>:<srv1 old start type>!@!@!<srv name2>:<srv2 old start type>)
//        Map of service name and their new start type.
//Output : return  true if everything is fine.
bool ChangeVmConfig::SetServicesStartType(string& SystemHiveName, string& SoftwareHiveName, map<string, DWORD> SrvWithNewStartType)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool rv = true;

	string strFinalSrvManual;
	std::vector<string> vecControlSets;
	if (false == GetControlSets(vecControlSets))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to get Control Sets vector for this VM.\n");
		DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
		return false;
	}

	std::vector<string>::iterator iterCS;
	map<string, DWORD>::iterator iterSrv;
	for (iterSrv = SrvWithNewStartType.begin(); iterSrv != SrvWithNewStartType.end(); iterSrv++)
	{
		DebugPrintf(SV_LOG_DEBUG, "Changing start type for Service [%s].\n", iterSrv->first.c_str());
		for (iterCS = vecControlSets.begin(); iterCS != vecControlSets.end(); iterCS++)
		{
			DebugPrintf(SV_LOG_DEBUG, "Performing registry in control set [%s].\n", (*iterCS).c_str());

			string RegPathToOpen = SystemHiveName + string("\\") + *iterCS + string("\\Services\\") + iterSrv->first;
			DebugPrintf(SV_LOG_DEBUG, "Registry path to open to get service status: %s \n", RegPathToOpen.c_str());

			HKEY hKey;
			DWORD dStartType;
			long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_ALL_ACCESS, &hKey);
			if (ERROR_FILE_NOT_FOUND == lRV)
			{
				if (hKey != NULL)
				{
					closeRegKey(hKey);
				}
				DebugPrintf(SV_LOG_ERROR, "Registry Path %s doesn't exist in control set [%s]...Trying to modify in next control set.\n", RegPathToOpen.c_str(), (*iterCS).c_str());
				rv = false;
			}
			else if (lRV != ERROR_SUCCESS)
			{
				if (hKey != NULL)
				{
					closeRegKey(hKey);
				}
				DebugPrintf(SV_LOG_ERROR, "Failed to open registry path [%s] with error code %ld...Trying to modify in next control set.\n", RegPathToOpen.c_str(), lRV);
				rv = false;
			}
			else
			{
				if (false == RegGetValueOfTypeDword(hKey, "start", dStartType))
				{
					DebugPrintf(SV_LOG_WARNING, "[WARNING]RegGetValueOfTypeDword failed : Path - [%s].\n", RegPathToOpen.c_str());
				}
				closeRegKey(hKey);
			}
			
			//Changing service start type
			if (false == ChangeDriverServicesStartType(SystemHiveName, iterSrv->first, iterSrv->second, *iterCS))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to change the start type for service : [%s].\n", iterSrv->first.c_str());
				rv = false;
			}
			else
			{
				stringstream ss;
				ss << dStartType;
				string strService = iterSrv->first + ":" + ss.str();
				
				if (strFinalSrvManual.empty())
					strFinalSrvManual = strService;
				else
				{
					if (strFinalSrvManual.find(iterSrv->first) == string::npos)
						strFinalSrvManual = strFinalSrvManual + "!@!@!" + strService;
				}
			}
		}
	}

	if (!strFinalSrvManual.empty())
	{
		if (false == RegSetKeyAtInstallDir(SoftwareHiveName, "SrvOldStartType", strFinalSrvManual))
		{
			DebugPrintf(SV_LOG_WARNING, "Failed to update the start type for all the services in registry\n");
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Successfully updated the start type for all the services in registry\n");
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return rv;
}


bool ChangeVmConfig::MakeChangesForDrive(std::string HostName, std::string SystemHiveName, std::string SoftwareHiveName, std::string SrcSystemVolume, std::string TgtMntPtForSrcSystemVolume)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	do
	{
		//Get protected Volume Details for the host from pinfo file.
		//TFS ID : 1051609
		map<string, string> mapOfSrcTgtVol;
		if (m_bCloudMT)
		{
			map<string, map<string, string> >::iterator iterHost = m_RecVmsInfo.m_volumeMapping.find(HostName);
			if (iterHost != m_RecVmsInfo.m_volumeMapping.end())
			{
				map<string, string>::iterator iterVolPair = iterHost->second.begin();
				for (; iterVolPair != iterHost->second.end(); iterVolPair++)
				{
					mapOfSrcTgtVol.insert(make_pair(iterVolPair->first, iterVolPair->second));
				}
			}
			if (mapOfSrcTgtVol.empty())
			{
				DebugPrintf(SV_LOG_ERROR, "Protected or Snapshot Pairs is empty for the VM - %s\n", HostName.c_str());
				rv = false; break;
			}
		}
		else
		{
			mapOfSrcTgtVol = getProtectedOrSnapshotDrivesMap(HostName);
			if (mapOfSrcTgtVol.empty())
			{
				DebugPrintf(SV_LOG_ERROR, "Map of source and target volume path is empty. \n");
				rv = false;
				break;
			}
		}

		//Get Map of Volume GUID to volume name from mbr file of that host.
		map<string, string> mapOfVolGuidAndName;
		rv = GetMapofVolGuidToVolNameFromMbrFile(HostName, mapOfSrcTgtVol, mapOfVolGuidAndName);
		if(!rv)
		{
			//Get map of volume GUID data's checksum from registry and Volume Registry
			map<string, string> mapChecksumAndVolGUID;
			rv = GetMapVolumeGuidAndChecksum(SystemHiveName, mapChecksumAndVolGUID);
			if(!rv)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the map of volume guid and their data's checksum from registry. \n");
				break;
			}

			if(mapChecksumAndVolGUID.empty())
			{
				DebugPrintf(SV_LOG_ERROR,"Map of volume guid and their data's checksum from registry is empty. \n");
				rv = false;
				break;
			}

			DebugPrintf(SV_LOG_DEBUG,"Successfully got the map of volume guid and their data's checksum from registry. \n");

			//Get Map of Volume GUID to volume name
			mapOfVolGuidAndName.clear();
			rv = GetMapofVolGuidToVolName(SystemHiveName, mapChecksumAndVolGUID, mapOfSrcTgtVol, mapOfVolGuidAndName);
			if(!rv)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to get the map of volume guid and respective volume path using registry. \n");
				break;
			}
		}

		if(mapOfVolGuidAndName.empty())
		{
			DebugPrintf(SV_LOG_ERROR,"Map of volume guid and respective volume path is empty. \n");
			rv = false;
			break;
		}
		DebugPrintf(SV_LOG_DEBUG,"Successfully got the map of volume guid and respective volume path. \n");

		string strLocalFile, strRecoveryDriveFile;
		strLocalFile = m_VxInstallPath + std::string("\\Failover\\Data\\") + std::string(DRIVE_FILE_NAME);

		/*std::string AgentInstallDir;
        if(false == RegGetInstallDir(SoftwareHiveName, AgentInstallDir))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get Install directory from registry for VM\n");
		    rv = false; break;
        }
		AgentInstallDir.replace(AgentInstallDir.find(SrcSystemVolume), SrcSystemVolume.length(), TgtMntPtForSrcSystemVolume);
		strRecoveryDriveFile = AgentInstallDir + std::string("\\Failover\\Data\\") + std::string(DRIVE_FILE_NAME);*/ 

		if(!m_bWinNT)
			strRecoveryDriveFile = TgtMntPtForSrcSystemVolume + std::string("\\Windows\\Temp\\") + std::string(DRIVE_FILE_NAME);
		else
			strRecoveryDriveFile = TgtMntPtForSrcSystemVolume + std::string("\\WINNT\\Temp\\") + std::string(DRIVE_FILE_NAME);

		rv = WriteDriveInfoMapToFile(mapOfVolGuidAndName, strLocalFile, strRecoveryDriveFile);
		if(!rv)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to write the volume guid and respective volume path in file %s. \n", strRecoveryDriveFile.c_str());
			break;
		}
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);	
    return rv;
}


//***************************************************************************************
// Make the required changes for P2V
//  Return true if all is well.
//***************************************************************************************
bool ChangeVmConfig::MakeChangesForP2V(std::string HostName, std::string PhysicalMachineOsType, std::string SystemHiveName,  
                                       std::string SrcSystemVolume, std::string TgtMntPtForSrcSystemVolume, std::string SoftwareHiveName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	std::string strSrcPath;
	std::string strTgtPath;
	std::string strRegistryName;
	PVOID OldValue = NULL;	
	bool bW2K8 = false;
	bool bW2K8_R2 = false;
	bool bW2K12 = false;
	bool bRegDelete = true;
	bool bW2K3 = false;
	bool bx64 = false;
	bool bXp32 = false;

	if( false == printBootSectorInfo(TgtMntPtForSrcSystemVolume))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to modify the VM's boot sector's configuration\n");
	}

	do
	{		
		if(PhysicalMachineOsType.compare("Windows_2003_32") == 0)
		{
			bW2K3 = true;
			bx64 = false;
		}
		else if(PhysicalMachineOsType.compare("Windows_2003_64") == 0)
		{
			bW2K3 = true;
			bx64 = true;
		}
		else if(PhysicalMachineOsType.compare("Windows_XP_32") == 0) 
		{
			strRegistryName = "symmpi";
			bXp32 = true;
		}
		else if(PhysicalMachineOsType.compare("Windows_XP_64") == 0) 
		{
			bXp32 = false;
			bW2K3 = true;
			bx64 = true;
		}
		else if((PhysicalMachineOsType.compare("Windows_2008_32") == 0)||(PhysicalMachineOsType.compare("Windows_2008_64") == 0) || 
			(PhysicalMachineOsType.compare("Windows_Vista_32") == 0) || (PhysicalMachineOsType.compare("Windows_Vista_64") == 0))
		{
			bW2K8 = true;
			strRegistryName = "LSI_SCSI";			
		}
		else if((PhysicalMachineOsType.compare("Windows_2008_R2")== 0) || (PhysicalMachineOsType.compare("Windows_7_64") == 0) || (PhysicalMachineOsType.compare("Windows_7_32") == 0))
		{
			bW2K8 = true;
			strRegistryName = "LSI_SCSI";
			//pass this variable to the ChangeRegistry()this is for the DriverPackageId and the tag value which are different from the windows 2008 32 and 64.
			bW2K8_R2 = true;
		}
		else if((PhysicalMachineOsType.compare("Windows_2012")== 0) || (PhysicalMachineOsType.compare("Windows_8") == 0))
		{
			strRegistryName = "LSI_SAS";
			bW2K12 = true;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Currently Os type: %s is not supported in case of P2V\n", PhysicalMachineOsType.c_str());
			DebugPrintf(SV_LOG_ERROR,"Note: As of now, only Windows XP/Windows Vista/Windows 7/Windows 8/Windows 2003/Windows 2008(R2)/Windows 2012 are supported.\n");
			rv = false;
			break;
		}
		// no need to copy the LSI_SCSI.sys file from the external place...because the LSI_SCSI driver file was already there.
		
		if(bW2K3)
		{
			if(!m_bWinNT)
				strTgtPath = TgtMntPtForSrcSystemVolume + std::string("\\windows\\system32\\drivers\\symmpi.sys");
			else
				strTgtPath = TgtMntPtForSrcSystemVolume + std::string("\\WINNT\\system32\\drivers\\symmpi.sys");

			strRegistryName = "symmpi";

            DebugPrintf(SV_LOG_INFO,"Destination Path = %s\n",strTgtPath.c_str());

			if(false == checkIfFileExists(strTgtPath))
			{
				if(false == GetDriverFile(TgtMntPtForSrcSystemVolume, SoftwareHiveName, bx64, strSrcPath)) //Gets the driver file in plan name folder
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to extarct the driver file to the target path %s\n", strSrcPath.c_str());
					rv = false;
					break;
				}
				else
				{
					DebugPrintf(SV_LOG_INFO,"Successfully extarcted the driver file to the target path %s\n", strSrcPath.c_str());
				}			
			
				DWORD dwFileAttrs = GetFileAttributes(strTgtPath.c_str());

				if(dwFileAttrs != INVALID_FILE_ATTRIBUTES)
				{
					if(dwFileAttrs & FILE_ATTRIBUTE_READONLY)
					{
						DebugPrintf(SV_LOG_INFO,"Driver File %s found as read-only, clearing the flag.\n", strTgtPath.c_str());
						if(0 == SetFileAttributes(strTgtPath.c_str(), dwFileAttrs ^ FILE_ATTRIBUTE_READONLY))
							DebugPrintf(SV_LOG_WARNING,"Failed to clear the read-only flag for the driver File. Error - %lu\n",GetLastError());
					}
					else
						DebugPrintf(SV_LOG_INFO,"Driver File %s is not in read-only state.\n", strTgtPath.c_str());
					if(dwFileAttrs & FILE_ATTRIBUTE_HIDDEN)
					{
						DebugPrintf(SV_LOG_INFO,"Driver File %s found as hidden, clearing the flag.\n", strTgtPath.c_str());
						if(0 == SetFileAttributes(strTgtPath.c_str(), dwFileAttrs ^ FILE_ATTRIBUTE_HIDDEN))
							DebugPrintf(SV_LOG_WARNING,"Failed to clear the hidden flag for the driver File. Error - %lu\n",GetLastError());
					}
					else
						DebugPrintf(SV_LOG_INFO,"Driver File %s is not in hidden state.\n", strTgtPath.c_str());
				}
				else
				{
					DebugPrintf(SV_LOG_WARNING,"Failed to get the Driver File attributes, proceeding further. Error - %lu\n",GetLastError());
				}

				Sleep(10000); //Sleeping for 10 secs, issue came during WMI reocvery (BSOD- Don't remove thispart)

				strSrcPath = m_strDatafolderPath + "\\symmpi.sys"; //Copying to a temporary location
				if(checkIfFileExists(strSrcPath))
				{
					if(0 == DeleteFile(strSrcPath.c_str()))
					{
						DebugPrintf(SV_LOG_WARNING,"Failed to delete file : %s, Error - %lu\n",strSrcPath.c_str(), GetLastError());
					}
					else
					{
						DebugPrintf(SV_LOG_INFO,"Successfully deleted the file : %s\n",strSrcPath.c_str(), GetLastError());
					}
				}
				if(0 == CopyFile(strTgtPath.c_str(),strSrcPath.c_str(),FALSE))
					DebugPrintf(SV_LOG_ERROR,"Driver File copy failed to temp location with error - %lu \n",GetLastError());
				else
					DebugPrintf(SV_LOG_ERROR,"Successfully copied the driver file to - %s \n",strSrcPath.c_str()); 
				
				char VolGuid[MAX_PATH];
				memset(VolGuid,0,MAX_PATH);
				if(0 == GetVolumeNameForVolumeMountPoint(TgtMntPtForSrcSystemVolume.c_str(), VolGuid, MAX_PATH))
				{            
					DebugPrintf(SV_LOG_ERROR,"Failed to get volume GUID for the volume[%s] with Errorcode : [%lu].\n", TgtMntPtForSrcSystemVolume.c_str(), GetLastError());                 
				}
				else
				{
					DebugPrintf(SV_LOG_INFO,"Volume : %s ==> %s\n", TgtMntPtForSrcSystemVolume.c_str(), VolGuid); 
					string strDrive = string(VolGuid);        
					std::string::size_type pos = strDrive.find_last_of("\\");
					if(pos != std::string::npos)
					{
						strDrive.erase(pos);
					}

					FlushFileUsingWindowsAPI(strDrive); // Flushing the target reocverd system volume
				}
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Driver File %s already exist.Skipping the driver file extraction.\n", strTgtPath.c_str());
			}
		}
		if(bXp32)
		{
			strSrcPath = m_VxInstallPath + std::string("\\Failover\\Data\\Windows_XP_32\\symmpi.sys");
			strTgtPath = TgtMntPtForSrcSystemVolume + std::string("\\windows\\system32\\drivers\\symmpi.sys");
			DebugPrintf(SV_LOG_INFO,"Source file Path = %s\n",strSrcPath.c_str());
            DebugPrintf(SV_LOG_INFO,"Destination Path = %s\n",strTgtPath.c_str());
			if(false == checkIfFileExists(strTgtPath))
			{
				if(0 == CopyFile(strSrcPath.c_str(),strTgtPath.c_str(),FALSE))
				{
					DebugPrintf(SV_LOG_ERROR,"Driver File copy failed with error - %lu\n",GetLastError()); 
					rv = false;
					break;
				}
				else
					DebugPrintf(SV_LOG_INFO,"Driver File %s copied successfully.\n", strTgtPath.c_str());
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Driver File %s already exist.\n", strTgtPath.c_str());
				DWORD dwFileAttrs = GetFileAttributes(strTgtPath.c_str());

				if(dwFileAttrs != INVALID_FILE_ATTRIBUTES)
				{
					if(dwFileAttrs & FILE_ATTRIBUTE_READONLY)
					{
						DebugPrintf(SV_LOG_INFO,"Driver File %s found as read-only, clearing the flag.\n", strTgtPath.c_str());
						if(0 == SetFileAttributes(strTgtPath.c_str(), dwFileAttrs ^ FILE_ATTRIBUTE_READONLY))
							DebugPrintf(SV_LOG_WARNING,"Failed to clear the read-only flag for the driver File. Error - %lu\n",GetLastError());
					}
					if(dwFileAttrs & FILE_ATTRIBUTE_HIDDEN)
					{
						DebugPrintf(SV_LOG_INFO,"Driver File %s found as hidden, clearing the flag.\n", strTgtPath.c_str());
						if(0 == SetFileAttributes(strTgtPath.c_str(), dwFileAttrs ^ FILE_ATTRIBUTE_HIDDEN))
							DebugPrintf(SV_LOG_WARNING,"Failed to clear the hidden flag for the driver File. Error - %lu\n",GetLastError());
					}
				}
				else
				{
					DebugPrintf(SV_LOG_WARNING,"Failed to get the Driver File attributes, proceeding further. Error - %lu\n",GetLastError());
				}
				if(0 == CopyFile(strSrcPath.c_str(),strTgtPath.c_str(),FALSE))
				{
					DebugPrintf(SV_LOG_ERROR,"Driver File copy failed with error - %lu, proceeding further with P2V changes\n",GetLastError());
				}
				else
					DebugPrintf(SV_LOG_INFO,"Driver File %s copied successfully.\n", strTgtPath.c_str());
			}
		}

		//Change the required registry entries.
		if(false == ChangeRegistry(SystemHiveName, strRegistryName, bRegDelete, PhysicalMachineOsType))
		{
            DebugPrintf(SV_LOG_ERROR,"ERROR in changing the registry entries for the Physical machine - %s\n",HostName.c_str());
			rv = false;
			break;
		}
		else
        {
			DebugPrintf(SV_LOG_INFO,"Successfuly changed the registry entries for the Physical machine - %s\n",HostName.c_str());
        }

		bRegDelete = false;
		//To update the registry changes for LSI_SAS for w2K8 and W2K8 R2
		if(bW2K8)
		{
			strRegistryName = "LSI_SAS";
			//Change the required registry entries.
			if(false == ChangeRegistry(SystemHiveName, strRegistryName, bRegDelete, PhysicalMachineOsType))
			{
				DebugPrintf(SV_LOG_ERROR,"ERROR in changing the registry entries for the Physical machine - %s\n",HostName.c_str());
				rv = false;
				break;
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Successfuly changed the registry entries for the Physical machine - %s\n",HostName.c_str());
			}
		}
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

bool ChangeVmConfig::ChangeServiceStartType(string& SystemHiveName, const string strSrvName, DWORD dSrvType, string& strCtrlSet)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	string RegPathToOpen = SystemHiveName + string("\\") + strCtrlSet + string("\\Services\\") + strSrvName;
	DebugPrintf(SV_LOG_DEBUG, "Registry path to open: %s \n", RegPathToOpen.c_str());
	
	HKEY hKey;
	long lRV = RegOpenKeyEx( HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_ALL_ACCESS , &hKey);

	if( ERROR_FILE_NOT_FOUND == lRV )
	{
		if(hKey != NULL)
		{
			closeRegKey(hKey);
		}
		DebugPrintf(SV_LOG_DEBUG,"Registry Path %s doesn't exist in control set [%s]...Trying to modify in next control set.\n", RegPathToOpen.c_str(), strCtrlSet.c_str() );
	}
	else if( lRV != ERROR_SUCCESS )
	{
		if(hKey != NULL)
		{
			closeRegKey(hKey);
		}
		DebugPrintf(SV_LOG_DEBUG,"Failed to open registry path [%s] with error code %ld...Trying to modify in next control set.\n", RegPathToOpen.c_str(), lRV );
	}
	else
	{
		if(false == ChangeDWord(hKey,"Start",dSrvType))
		{
			DebugPrintf(SV_LOG_WARNING, "Failed to update the Key \"Start\" for Registry path %s...Please stop the service manually.\n", RegPathToOpen.c_str());
			rv = false;
		}
		closeRegKey(hKey);
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


//Currently its a duplicate API of "ChangeServiceStartType" .
//BUG ID - 1313149
bool ChangeVmConfig::ChangeDriverServicesStartType(string& SystemHiveName, const string strSrvName, DWORD dSrvType, string& strCtrlSet)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool rv = true;

	string RegPathToOpen = SystemHiveName + string("\\") + strCtrlSet + string("\\Services\\") + strSrvName;
	DebugPrintf(SV_LOG_DEBUG, "Registry path to open: %s \n", RegPathToOpen.c_str());

	HKEY hKey;
	long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_ALL_ACCESS, &hKey);

	if (ERROR_FILE_NOT_FOUND == lRV)
	{
		if (hKey != NULL)
		{
			closeRegKey(hKey);
		}
		DebugPrintf(SV_LOG_ERROR, "Registry Path %s doesn't exist in control set [%s]...Trying to modify in next control set.\n", RegPathToOpen.c_str(), strCtrlSet.c_str());
		rv = false;
	}
	else if (lRV != ERROR_SUCCESS)
	{
		if (hKey != NULL)
		{
			closeRegKey(hKey);
		}
		DebugPrintf(SV_LOG_ERROR, "Failed to open registry path [%s] with error code %ld...Trying to modify in next control set.\n", RegPathToOpen.c_str(), lRV);
		rv = false;
	}
	else
	{
		if (false == ChangeDWord(hKey, "Start", dSrvType))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to update the Key \"Start\" for Registry path %s...Please stop the service manually.\n", RegPathToOpen.c_str());
			rv = false;
		}
		closeRegKey(hKey);
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return rv;
}

void ChangeVmConfig::RestoreSanPolicy()
{
	TRACE_FUNC_BEGIN;


	std::stringstream errorStream;
	std::string SystemHiveName = VM_SYSTEM_HIVE_NAME;
	std::string SoftwareHiveName = VM_SOFTWARE_HIVE_NAME;

	if (false == RestoreSanPolicy(SystemHiveName,SoftwareHiveName))
	{
		errorStream << m_sourceVMName << ":Could not restore SanPolicy.\n";
		throw std::exception(errorStream.str().c_str());
	}

	TRACE_FUNC_END;
}


/***************************************************************************************
  Look for Backup.SanPolicy value under "SV Systems" key. If found then restore that 
  value to all control sets. If not found then no need to restore as SanPolicy might 
  not modified as part of failover. Also make sure that the Backup.SanPolicy value
  should not be 0, if it is 0 the skip resotring SanPolicy. 0 indicases SAN_POLICY_UNKOWN
****************************************************************************************/
bool ChangeVmConfig::RestoreSanPolicy(const std::string& SystemHiveName, const std::string& SoftwareHiveName)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool rv = true;
	DWORD dwValue = 0;
	
	CRegKey svSystemsKey;
	std::string svSystemsKeyPath = SoftwareHiveName + "\\SV Systems";
	LONG lRetStatus = svSystemsKey.Open(HKEY_LOCAL_MACHINE, svSystemsKeyPath.c_str());
	if (ERROR_FILE_NOT_FOUND == lRetStatus)
	{
		svSystemsKeyPath = SoftwareHiveName + "\\Wow6432Node\\SV Systems";
		lRetStatus = svSystemsKey.Open(HKEY_LOCAL_MACHINE, svSystemsKeyPath.c_str());
	}

	if (ERROR_SUCCESS != lRetStatus)
	{
		DebugPrintf(SV_LOG_ERROR, "Could not restore SAN Policy. Failed to open the %s for VM %s. Error %ld\n", 
			svSystemsKeyPath.c_str(),
			m_sourceVMName.c_str(),
			lRetStatus);

		DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
		return false;
	}

	lRetStatus = svSystemsKey.QueryDWORDValue( REG_VALUE_BACKUP_SANPOLICY, dwValue);
	if (ERROR_FILE_NOT_FOUND == lRetStatus)
	{
		DebugPrintf(SV_LOG_INFO, "Backup.SanPolicy is not found under %s. Nothing to restore.\n", 
			svSystemsKeyPath.c_str());

		return true;
	}
	else if (ERROR_SUCCESS != lRetStatus)
	{
		DebugPrintf(SV_LOG_ERROR, "Could not restore SAN Policy. Failed to query Backup.SanPolicy under %s for VM %s. Error %ld\n",
			svSystemsKeyPath.c_str(),
			m_sourceVMName.c_str(),
			lRetStatus);

		DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
		return false;
	}
	else if (dwValue < 1 || dwValue > 4 )
	{
		DebugPrintf(SV_LOG_WARNING, "SanPolicy should be withing the range {1,4}. Current value is %lu Its a miss configuration. Skipping SanPolicy restore.\n", dwValue);
		return true;
	}

	std::vector<std::string> vecControlSets;
	if (false == GetControlSets(vecControlSets))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to get Control Sets for the VM %s.\n", m_sourceVMName.c_str());
		DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
		return false;
	}

	std::vector<std::string>::iterator iterCS = vecControlSets.begin();
	for (; iterCS != vecControlSets.end(); iterCS++)
	{
		DebugPrintf(SV_LOG_DEBUG, "Performing SanPolicy changes in control set [%s].\n", (*iterCS).c_str());

		string partMgrSvcParamsKey = SystemHiveName + 
			"\\" + *iterCS 
			+ "\\Services\\partmgr\\Parameters";

		CRegKey paramKey;
		LONG lRetStatus = paramKey.Open(HKEY_LOCAL_MACHINE, partMgrSvcParamsKey.c_str());
		if (ERROR_FILE_NOT_FOUND == lRetStatus)
		{
			DebugPrintf(SV_LOG_INFO, "Registry key %s not found in this control set.\n",
				partMgrSvcParamsKey.c_str());

			continue;
		}
		else if (ERROR_SUCCESS != lRetStatus)
		{
			DebugPrintf(SV_LOG_ERROR, "Error opening registry key %s. Error %ld.\n", 
				partMgrSvcParamsKey.c_str(), 
				lRetStatus);

			rv = false;
			break;
		}
		
		// 
		// Restore the SanPolicy value.
		//
		lRetStatus = paramKey.SetDWORDValue("SanPolicy", dwValue);
		if (ERROR_SUCCESS != lRetStatus)
		{
			DebugPrintf(SV_LOG_ERROR, "Could not restore SanPolicy for this control set. Error %ld\n", lRetStatus);

			rv = false;
			break;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Successfully restored the SanPolicy to %lu.\n", dwValue);
		}
	}

	//
	// Remove the backup sanpolicy key.
	//
	if (rv &&
		ERROR_SUCCESS != svSystemsKey.DeleteValue(REG_VALUE_BACKUP_SANPOLICY)
		)
	{
		DebugPrintf(SV_LOG_ERROR, "Could not delete SanPolicy backup key.\n");
		rv = false;
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return rv;
}

/***************************************************************************************
Change the Inmage Services Start up Property from Automatic to Manual.
1. Getting the all the control sets from the system hive.
2. For each control set searching for the "Services\\frsvc" registry path.
3. In that path changing the "Start" key value to "3", which means Manual.
4. In case of Azure, not changing vx,fx and appagent status to Manual.
****************************************************************************************/
bool ChangeVmConfig::MakeChangesForInmSvc(std::string SystemHiveName, std::string SoftwareHiveName, bool bDoNotSkipInmServices)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;
	DWORD dwValue;

	std::vector<std::string> vecControlSets;
    if(false == GetControlSets(vecControlSets))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get Control Sets vector for this VM.\n");
        DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
        return false;
    }

	std::vector<std::string>::iterator iterCS = vecControlSets.begin();
    for(; iterCS != vecControlSets.end(); iterCS++)
    {
		DebugPrintf(SV_LOG_DEBUG, "Performing registry changes in control set [%s].\n", (*iterCS).c_str() );

        //Making Vxagent service manual
		if (bDoNotSkipInmServices &&
			false == ChangeServiceStartType(SystemHiveName, "svagents", 3, *iterCS))
		{
			DebugPrintf(SV_LOG_WARNING, "Failed to change the start type for service : \"svagents\".\n");
			rv = false;
		}

		//Making FXagent service manual
		if (bDoNotSkipInmServices &&
			false == ChangeServiceStartType(SystemHiveName, "frsvc", 3, *iterCS))
		{
			DebugPrintf(SV_LOG_WARNING, "Failed to change the start type for service : \"svagents\".\n");
			rv = false;
		}

		//Making Appagent service manual
	    if (bDoNotSkipInmServices &&
			false == ChangeServiceStartType(SystemHiveName, "InMage Scout Application Service", 3, *iterCS))
		{
			DebugPrintf(SV_LOG_WARNING, "Failed to change the start type for service : \"svagents\".\n");
			rv = false;
		}
		
		//Disabling VMware tools in case of Azure or Aws recovery\migration
		if((m_strCloudEnv.compare("azure") == 0) || (m_strCloudEnv.compare("aws") == 0))
		{
			dwValue = 4; //SERVICE_DISABLED//0x00000004
		}
		else
		{
			//This applies for VMware environment.
			dwValue = 2;  //Enables the service//SERVICE_AUTO_START//0x00000002
		}

		if (false == ChangeServiceStartType(SystemHiveName, "VMTools", dwValue, *iterCS))
        {
		    DebugPrintf(SV_LOG_WARNING, "Failed to change the start type for service : \"VMTools\".\n");
			rv = false;
		}

        if (m_strCloudEnv.compare("vmware") == 0)
        {
            list<string> listOfAzureAgentSvcs;
            listOfAzureAgentSvcs.push_back(WINDOWS_AZURE_GUEST_AGENT);
            listOfAzureAgentSvcs.push_back(WINDOWS_AZURE_TELEMETRY_SVC);
            listOfAzureAgentSvcs.push_back(WINDOWS_AZURE_RDAGENT_SVC);

            list<string>::iterator iterSvc = listOfAzureAgentSvcs.begin();
            for (; iterSvc != listOfAzureAgentSvcs.end(); iterSvc++)
            {
                if (false == ChangeServiceStartType(SystemHiveName, *iterSvc, SERVICE_DISABLED, *iterCS))
                {
                    DebugPrintf(SV_LOG_ERROR, "Failed to change the start type for service : %s.\n", iterSvc->c_str());
                    rv = false;
                }
            }
        }
	}
	if ((m_strCloudEnv.compare("azure") == 0) || (m_strCloudEnv.compare("vmware") ==0))
	{
		//Enabling dhcp service in Azure recovery VM.
		map<string, DWORD> SrvWithNewStartType;
		SrvWithNewStartType["Dhcp"] = 2;      //Enables the service//SERVICE_AUTO_START//0x00000002

		if (false == SetServicesStartType(SystemHiveName, SoftwareHiveName, SrvWithNewStartType))
		{
			DebugPrintf(SV_LOG_WARNING, "Failed to change the start type for dhcp services.\n");
			rv = false;
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


//This will update the start type of driver to automatic.
//This also verifies the required system files exist or not.
bool ChangeVmConfig::MakeChangesForBootDrivers(string TgtMntPtForSrcSystemVolume, string SystemHiveName, string SoftwareHiveName)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool rv = true;

	string strSytemPath = "\\Windows";
	if (m_bWinNT)
		strSytemPath = "\\WINNT";

	list<string> lstDrivers;
	lstDrivers.push_back(TgtMntPtForSrcSystemVolume + strSytemPath + "\\System32\\drivers\\Intelide.sys");
	lstDrivers.push_back(TgtMntPtForSrcSystemVolume + strSytemPath + "\\System32\\drivers\\pciidex.sys");
	lstDrivers.push_back(TgtMntPtForSrcSystemVolume + strSytemPath + "\\System32\\drivers\\atapi.sys");
	lstDrivers.push_back(TgtMntPtForSrcSystemVolume + strSytemPath + "\\System32\\drivers\\ataport.sys");
	lstDrivers.push_back(TgtMntPtForSrcSystemVolume + strSytemPath + "\\System32\\drivers\\vmstorfl.sys");
	lstDrivers.push_back(TgtMntPtForSrcSystemVolume + strSytemPath + "\\System32\\drivers\\storvsc.sys");
	lstDrivers.push_back(TgtMntPtForSrcSystemVolume + strSytemPath + "\\System32\\drivers\\vmbus.sys");
	
	//Checks for driver files existence as listed above
	if (!CheckDriverFilesExistance(lstDrivers))
	{
		DebugPrintf(SV_LOG_ERROR, "Some of the driver files are missing.\n");
		rv = false;
	}

	//Enabling intelide driver related services in Azure recovery VM.
	map<string, DWORD> SrvWithNewStartType;
	SrvWithNewStartType["Intelide"] = 0;      //Enables the service//SYTEM_SERVICE_AUTO_START//0x00000000
	SrvWithNewStartType["atapi"] = 0;
	SrvWithNewStartType["storflt"] = 0;
	SrvWithNewStartType["storvsc"] = 0;
	SrvWithNewStartType["vmbus"] = 0;

	//Updating starttype for the driver related services
	if (!SetServicesStartType(SystemHiveName, SoftwareHiveName, SrvWithNewStartType))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to change the start type for some of the driver related services. This will cause booting issues for recovered server.\n");
		rv = false;
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return rv;
}


//*************************************************************
//----------------SetVMwareToolsStartType---------------------------
//1. Checks If VMware Tools are installed by looking at registry entry for it.
//2. If VMware Tools are available then checks
//		1. if Start State is equal to value passed through argument.
//		2. if not, sets the value which is equal to argument received.
//3. If tools are not available then 
//		1. It returns 'true', may be a true physical machine.
//		2. In case of Physical machine tools will be installed through iso in vContinuum work flow.
//*************************************************************
bool ChangeVmConfig::SetVMwareToolsStartType( std::string SystemHiveName , DWORD startValue )
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	std::vector<std::string> vecControlSets;
    if(false == GetControlSets(vecControlSets))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get Control Sets vector for this VM.\n");
        DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
        return false;
    }

	std::vector<std::string>::iterator iterCS = vecControlSets.begin();
    for(; iterCS != vecControlSets.end(); iterCS++)
    {
		DebugPrintf(SV_LOG_INFO , "checking for VMware tools in control set [%s].\n", (*iterCS).c_str() );
		
		long lRV;
		HKEY hKey = NULL;
		string RegPathToOpen="";
		
		RegPathToOpen = SystemHiveName + string("\\") + (*iterCS) + string("\\") + std::string("Services\\VMTools");
		DebugPrintf(SV_LOG_INFO, "Registry path to open: %s \n", RegPathToOpen.c_str());

		lRV = RegOpenKeyEx( HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_ALL_ACCESS , &hKey);

		if( ERROR_FILE_NOT_FOUND == lRV )
		{
			if(hKey != NULL)
			{
				closeRegKey(hKey);
			}
			DebugPrintf(SV_LOG_INFO,"Registry Path %s doesn't exist in control set [%s]...continuing to next control set.\n", RegPathToOpen.c_str(), (*iterCS).c_str() );
		}
		else if( lRV != ERROR_SUCCESS )
		{
			if(hKey != NULL)
			{
				closeRegKey(hKey);
			}
			DebugPrintf(SV_LOG_INFO,"Failed to open registry path [%s] with error code %ld...continuing to next control set.\n", RegPathToOpen.c_str(), lRV );
		}
		else
		{
			//Disable or Enable Service to start during system start-up time.
			if(false == ChangeDWord( hKey, "Start", startValue ))
			{
				DebugPrintf(SV_LOG_WARNING, "Failed to update the Key \"Start\" for Registry path %s...Please set VMTools service start type manually.\n", RegPathToOpen.c_str());
				rv = false;
			}
			closeRegKey(hKey);
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//***************************************************************************************
// Make the required changes for NW
//  1. Fetch NW details from Registry
//  2. Fetch NW details from XML
//  3. Update NW details by overwriting XML ones with Reg ones 
//  4. Write updated NW details to file
//  5. Copy the file to VM's directory (say C:\windows\temp on src i.e. C:\ESX\HOSTNAME_C\Windows\temp on MT)
//  6. Prepare/Make changes to recovered VM's registry such that an exe starts on next boot and configures nw settings
// Return true if "All is Well"
//***************************************************************************************
bool ChangeVmConfig::MakeChangesForNW(std::string HostName, std::string OsType, 
                                      std::string SystemHiveName, std::string SoftwareHiveName,                                      
                                      std::string SrcSystemVolume, std::string TgtMntPtForSrcSystemVolume)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    do
    {
        bool bW2K8 = false;
        bool bW2K8R2 = false;

        if((OsType.find("2008") != std::string::npos) || (OsType.find("2012") != std::string::npos) || (OsType.find("7") != std::string::npos) || (OsType.find("8") != std::string::npos) || (OsType.find("Vista") != std::string::npos))        
        {
            DebugPrintf(SV_LOG_INFO,"OsType - %s \n",OsType.c_str());
            bW2K8 = true;     
            if((OsType.find("R2") != std::string::npos) || (OsType.find("2012") != std::string::npos) || (OsType.find("7") != std::string::npos))
            {
                bW2K8R2 = true;        
            }
			else if((OsType.find("Windows 8") != std::string::npos)|| (OsType.find("Windows Server 8") != std::string::npos) || (OsType.compare("Windows_8") == 0))
			{
				bW2K8R2 = true;
			}
        }

        NetworkInfoMap_t NwMapFromXml;
        if(false == xGetNWInfoFromXML(NwMapFromXml, HostName))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get Network settings from Recovery XML file for VM - %s\n", HostName.c_str());
            rv = false;
        }

        std::string NwFileName = m_VxInstallPath + std::string("\\") + std::string(NETWORK_FILE_NAME);

		std::string TargetPath, ClusterNwPath;
		if(!m_bWinNT)
		{
			TargetPath = TgtMntPtForSrcSystemVolume + std::string("\\Windows") + std::string(NETWORK_FILE_PATH);
			ClusterNwPath = TgtMntPtForSrcSystemVolume + std::string("\\Windows\\Temp\\clusternw.txt");
		}
		else
		{
			TargetPath = TgtMntPtForSrcSystemVolume + std::string("\\WINNT\\Temp\\nw.txt");
			ClusterNwPath = TgtMntPtForSrcSystemVolume + std::string("\\WINNT\\Temp\\clusternw.txt");
		}

		DebugPrintf(SV_LOG_INFO,"Source Network File Path - %s\n", NwFileName.c_str());
        DebugPrintf(SV_LOG_INFO,"Recovered VM network File Path - %s\n", TargetPath.c_str());
		DebugPrintf(SV_LOG_INFO,"Recovered VM cluster network File Path - %s\n", ClusterNwPath.c_str());
        if(false == WriteNWInfoMapToFile(NwMapFromXml, NwFileName, TargetPath, ClusterNwPath))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to write Network settings for VM - %s to file - %s \n", HostName.c_str(), NwFileName.c_str());
            rv = false;
        }

        // Update the registry of VM to run the script on next boot
        if(false == RegAddStartupExeOnBoot(SoftwareHiveName, SrcSystemVolume, bW2K8, bW2K8R2))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to add registry entries for running an executable on VM boot for VM - %s\n", HostName.c_str());
		    rv = false; break;
        }
    }while(0);


    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

/***************************************************************************************
Change the xenvbd 6.0 driver related registry enties of Win2012 to work in AWS. 
****************************************************************************************/

bool ChangeVmConfig::MakeRegChangesForW2012AWS(std::string SystemHiveName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool rv = false;
	string xenvbdPath = SystemHiveName + XENVBD_6_0_REG_KEY;
	HKEY hXenvbdKey = NULL;
	do
	{
		LRESULT lResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE,xenvbdPath.c_str(),0,KEY_ALL_ACCESS,&hXenvbdKey);
		if(ERROR_SUCCESS != lResult)
		{
			DebugPrintf(SV_LOG_ERROR,"RegOpenKey failed for %s. Error : %d\n",xenvbdPath.c_str(),lResult);
			hXenvbdKey = NULL;
			break;
		}
		string xenvbd_InstKey = "Configurations\\xenvbd_Inst";
		string pnpInterfaceKey = xenvbd_InstKey + "\\Services\\xenvbd\\Parameters\\PnpInterface";
		DWORD dwDispos;
		HKEY pnpiKey = NULL;
		lResult = RegCreateKeyEx(hXenvbdKey,pnpInterfaceKey.c_str(),0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&pnpiKey,&dwDispos);
		if(ERROR_SUCCESS != lResult)
		{
			DebugPrintf(SV_LOG_ERROR,"RegCreateKeyEx failed for following key. Error %d\n%s\n",lResult,pnpInterfaceKey.c_str());
			break;
		}
		else
		{
			bool bSuccess = SetValueREG_DWORD(pnpiKey,"5",1);
			RegCloseKey(pnpiKey);
			if(!bSuccess)
			{
				DebugPrintf(SV_LOG_ERROR,"Faile to update values for the key %s",pnpInterfaceKey.c_str());
				break;
			}

			HKEY hXenvbd_Inst = NULL;
			lResult = RegOpenKeyEx(hXenvbdKey,xenvbd_InstKey.c_str(),0,KEY_ALL_ACCESS,&hXenvbd_Inst);
			if(ERROR_SUCCESS != lResult)
			{
				DebugPrintf(SV_LOG_ERROR,"RegOpenKey failed for %s. Error : %d\n",xenvbd_InstKey.c_str(),lResult);
				break;
			}
			bSuccess = SetValueREG_DWORD(hXenvbd_Inst,"ConfigFlags",0) &&
				       SetValueREG_SZ(hXenvbd_Inst,"Service","xenvbd");
			RegCloseKey(hXenvbd_Inst);
			if(!bSuccess)
			{
				DebugPrintf(SV_LOG_ERROR,"Faile to update values for the key %s",xenvbd_InstKey.c_str());
				break;
			}
		}
		
		vector<string> pciKeyList;
		pciKeyList.push_back("Descriptors\\PCI\\VEN_5853&DEV_0001");
		pciKeyList.push_back("Descriptors\\PCI\\VEN_5853&DEV_0001&SUBSYS_00015853");
		pciKeyList.push_back("Descriptors\\PCI\\VEN_fffd&DEV_0101");
		vector<string>::const_iterator iterPCI = pciKeyList.begin();
		bool bPCIUpdateSucces = true;
		for(;iterPCI != pciKeyList.end() && bPCIUpdateSucces; iterPCI++)
		{
			HKEY pciKey = NULL;
			lResult = RegCreateKeyEx(hXenvbdKey,iterPCI->c_str(),0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&pciKey,&dwDispos);
			if(ERROR_SUCCESS != lResult)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to create the following key. Error %d\n %s\n",lResult,iterPCI->c_str());
				bPCIUpdateSucces = false;
				break;
			}

			if( !SetValueREG_SZ(pciKey,"Configuration","xenvbd_Inst") ||
				!SetValueREG_SZ(pciKey,"Manufacturer","%citrix%") ||
				!SetValueREG_SZ(pciKey,"Description","%xenvbddesc%"))
			{
				DebugPrintf(SV_LOG_ERROR,"Faile to update values for the key %s\n",iterPCI->c_str());
				bPCIUpdateSucces = false;
			}
			RegCloseKey(pciKey);
		}

		if(!bPCIUpdateSucces)
			break;

		string xenvbdStringskey = "Strings";
		HKEY stringsKey = NULL;
		lResult = RegCreateKeyEx(hXenvbdKey,xenvbdStringskey.c_str(),0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&stringsKey,&dwDispos);
		if(ERROR_SUCCESS != lResult)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to create the following key. Error %d\n %s\n",lResult,iterPCI->c_str());
			break;
		}
		else
		{
			if( !SetValueREG_SZ(stringsKey,"xenvbddesc","Citrix PV SCSI Host Adapter") ||
				!SetValueREG_SZ(stringsKey,"citrix","Citrix Systems, Inc."))
			{
				DebugPrintf(SV_LOG_ERROR,"Faile to update values for the key %s\n",xenvbdStringskey.c_str());
				RegCloseKey(stringsKey);
				break;
			}
			RegCloseKey(stringsKey);
		}

		rv = true;//Successfuly updated all the registry entries
	}while(false);
	
	if(hXenvbdKey)
		RegCloseKey(hXenvbdKey);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//***************************************************************************************
// Steps :
//    -> Load system and software hives in advance (instead of loading for every registry operation)
//    -> Get Current Control set number 
//    -> Get Adapters (Interfaces) from registry
//    -> Get IP values of these NIC cards from the same control set.    
//***************************************************************************************
bool ChangeVmConfig::RegGetNWInfo(std::string SystemHiveName, std::string SoftwareHiveName,
                                  std::string SystemDrivePathOnVm, std::string SystemDrivePathOnMT, 
                                  NetworkInfoMap_t & NetworkInfoMap)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    do
    {           
        // Get current control set number
        DWORD dwCCS;
        if(false == GetCurrentControlSetValue(SystemHiveName, dwCCS))
        {
            DebugPrintf(SV_LOG_ERROR, "GetCurrentControlSetValue() failed.\n");
		    rv = false; break;
        }
        DebugPrintf(SV_LOG_INFO,"dwCCS - %lu\n",dwCCS);

        string CCS;
        FormatCCS(dwCCS, CCS);       
        DebugPrintf(SV_LOG_INFO,"CCS - %s\n",CCS.c_str());

        // Get Adapters (Interfaces)
        InterfacesVec_t Interfaces;
        if(false == RegGetInterfaces(SystemHiveName, CCS, Interfaces))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get the NIC Interfaces transport names.\n");
		    rv = false; break;
        }

        // Fetch IP values of these NIC cards from the above control set.            
        if(false == RegGetNwMap(SystemHiveName, CCS, Interfaces, NetworkInfoMap))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get the Network Info of all NIC Interfaces.\n");
		    rv = false; break;
        }       
    }while(0);
    
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);    
    return rv;
}

/*
    Function to get current control set number
    Read the value in Reg Path - 
        HKEY_LOCAL_MACHINE\SYSTEM\Select --> Current = ___    
*/
bool ChangeVmConfig::GetCurrentControlSetValue(std::string SystemHiveName, DWORD & dwCCS)
{
    bool rv = true;   

    do
    {
        string RegPathToOpen = SystemHiveName + std::string("\\Select");    
        
        HKEY hKey;
        long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_READ, &hKey);
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu].\n", RegPathToOpen.c_str(), lRV);
            rv = false; break;
        }

        std::string KeyToFetch = std::string("Current");        
        DWORD dwType = REG_DWORD;
        DWORD dwSize = sizeof(dwCCS);

        lRV = RegQueryValueEx(hKey, KeyToFetch.c_str(), NULL, &dwType, (PBYTE)&dwCCS, &dwSize);            
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_ERROR,"RegQueryValueEx failed at Path - [%s], Key - [%s] and return code - [%lu].\n", RegPathToOpen.c_str(), KeyToFetch.c_str(), lRV);
            closeRegKey(hKey);
            rv = false; break;
        }
        DebugPrintf(SV_LOG_INFO,"Control Set Value = %lu\n", dwCCS);
        closeRegKey(hKey);
    }while(0);
    
    return rv;
}

/*
    Get Adapters from location (HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\services\Tcpip\Parameters\Adapters)	
        They are subfolders in this location
*/
bool ChangeVmConfig::RegGetInterfaces(std::string SystemHiveName, string CCS, InterfacesVec_t & Interfaces)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;   

    do
    {
        string RegPathToOpen = SystemHiveName + std::string("\\") + CCS + std::string(ADAPTER_REG_PATH);                    
        DebugPrintf(SV_LOG_INFO,"RegPathToOpen = %s\n", RegPathToOpen.c_str());

        InterfacesVec_t vecInterface;
        if(false == RegGetSubFolders(RegPathToOpen, vecInterface))
        {
            DebugPrintf(SV_LOG_ERROR, "RegGetSubFolders failed.\n");            
            rv = false; break;
        }

        // Parse through Interfaces vector and check for "NdisWanIp"
        // and remove this (issue in W2K3)
        InterfacesVecIter_t iter = vecInterface.begin();
        for(; iter != vecInterface.end(); iter++)
        {
            if(iter->compare("NdisWanIp") != 0)
				Interfaces.push_back(*iter);                
        }

    }while(0);
    
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

/*
    Get the value for the given key. This function deals types DWORD
    Output is returned as string.
*/
bool ChangeVmConfig::RegGetValueOfTypeDword(HKEY hKey, std::string KeyToFetch, DWORD & Value)
{
    bool rv = true;

    do
    {
        DWORD dwValue;

        DWORD dwType, dwSize;
        long lRV;

        lRV = RegQueryValueEx(hKey, KeyToFetch.c_str(), NULL, &dwType, (PBYTE)&dwValue, &dwSize);            
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_ERROR,"RegQueryValueEx failed to fetch the value for Key - [%s] and return code - [%lu].\n", KeyToFetch.c_str(), lRV);
            closeRegKey(hKey);
            rv = false; break;
        }
        DebugPrintf(SV_LOG_INFO,"dwValue = %lu\n",dwValue);

        Value = dwValue;
    }while(0);

    return rv;
}

/*
    Get the value for the given key. This function deals types SZ and MULTI_SZ 
    Output is returned as string.
*/
bool ChangeVmConfig::RegGetValueOfTypeSzAndMultiSz(HKEY hKey, std::string KeyToFetch, std::string & Value)
{
    bool rv = true;   
    
    do
    {
        char * cValue;

        DWORD dwType, dwSize;
        long lRV;

        lRV = RegQueryValueEx(hKey, KeyToFetch.c_str(), NULL, &dwType, 0, &dwSize);            
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_ERROR,"RegQueryValueEx failed to fetch the type and size for Key - [%s] and return code - [%lu].\n", KeyToFetch.c_str(), lRV);
            closeRegKey(hKey);
            rv = false; break;
        }
        
        cValue = new char [dwSize/sizeof(char)];

        lRV = RegQueryValueEx(hKey, KeyToFetch.c_str(), NULL, &dwType, (PBYTE)cValue, &dwSize);            
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_ERROR,"RegQueryValueEx failed to fetch the value for Key - [%s] and return code - [%lu].\n", KeyToFetch.c_str(), lRV);
            closeRegKey(hKey);
            rv = false; break;
        }
        DebugPrintf(SV_LOG_DEBUG,"cValue = %s\n",cValue);

        Value = std::string(cValue);
    }while(0);    

    return rv;
}

/*
    For each interface get Network Info struct from the following location
    HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\services\Tcpip\Parameters\Interfaces\{28887561-3D6E-4322-BC7A-D8B89159A2DB}

    Made a change for Bug 15714 
        Earlier - it is failing when one particular entry is not found in registry.
        Now - it will continue and read next entry , hence removed breaks.
*/
bool ChangeVmConfig::RegGetNwMap(std::string SystemHiveName, std::string CCS, InterfacesVec_t Interfaces, NetworkInfoMap_t & NetworkInfoMap)
{
    bool rv = true;

    for(size_t i = 0; i < Interfaces.size(); i++)
    {
        std::string RegPathToOpen = SystemHiveName + std::string("\\") + CCS + std::string(INTERFACE_REG_PATH) + std::string("\\") + Interfaces[i];
        DebugPrintf(SV_LOG_INFO,"RegPathToOpen = %s\n", RegPathToOpen.c_str());

        NetworkInfo nw;

        HKEY hKey;
        long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_READ, &hKey);
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed : Path - [%s] and return code - [%lu].\n", RegPathToOpen.c_str(), lRV);
            //rv = false; break;
            rv = false; continue;
        }
        
        if( false == RegGetValueOfTypeDword(hKey, ENABLE_DHCP_KEY, nw.EnableDHCP) )
        {
            DebugPrintf(SV_LOG_WARNING,"[WARNING]RegGetValueOfTypeDword failed : Path - [%s].\n", RegPathToOpen.c_str());
            //closeRegKey(hKey);
            //rv = false; break;
            //rv = false;
        }

        if(0 == nw.EnableDHCP)
        {
            if(false == RegGetValueOfTypeSzAndMultiSz(hKey, IP_ADDRESS_KEY, nw.IPAddress))
            {
                DebugPrintf(SV_LOG_WARNING,"[WARNING]IP Address not found, RegGetValueOfTypeSzAndMultiSz : Path - [%s].\n", RegPathToOpen.c_str());
				closeRegKey(hKey);
				lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_READ, &hKey);
                //closeRegKey(hKey);
                //rv = false; break;
                //rv = false;
            }

            if(false == RegGetValueOfTypeSzAndMultiSz(hKey, SUBNET_MASK_KEY, nw.SubnetMask))
            {
                DebugPrintf(SV_LOG_WARNING,"[WARNING]SubNet Mask not found, RegGetValueOfTypeSzAndMultiSz : Path - [%s].\n", RegPathToOpen.c_str());
				closeRegKey(hKey);
				lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_READ, &hKey);
                //closeRegKey(hKey);
                //rv = false; break;
                //rv = false;
            }

            if(false == RegGetValueOfTypeSzAndMultiSz(hKey, DEFAULT_GATEWAY_KEY, nw.DefaultGateway))
            {
                DebugPrintf(SV_LOG_WARNING,"[WARNING]GateWay not found, RegGetValueOfTypeSzAndMultiSz : Path - [%s].\n", RegPathToOpen.c_str());
				closeRegKey(hKey);
				lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_READ, &hKey);
                //closeRegKey(hKey);
                //rv = false; break;
                //rv = false;
            }

            if(false == RegGetValueOfTypeSzAndMultiSz(hKey, NAME_SERVER_KEY, nw.NameServer))
            {
				DebugPrintf(SV_LOG_WARNING,"[WARNING]Name Server not found, RegGetValueOfTypeSzAndMultiSz : Path - [%s].\n", RegPathToOpen.c_str());
                //closeRegKey(hKey);
                //rv = false; break;
                //rv = false;
            }
        }

        NetworkInfoMap.insert(make_pair(Interfaces[i], nw));

        closeRegKey(hKey);
    }

    return rv;
}

/*
    Helper function for RegAddStartupExeOnBoot()
    Add Reg entries required based on OS for running a exe at boot time
*/
bool ChangeVmConfig::RegAddRegEntriesForStartupExe(std::string SystemDrivePathOnVm, 
                                                   std::string RegPathToOpen, 
                                                   std::string AgentInstallDir,
                                                   bool bW2K8R2)
{
    bool rv = true;
    
    long lRV;
    HKEY hKey;
    
    std::string Key, Value;

    do
    {
        DebugPrintf(SV_LOG_DEBUG,"Key to Open = %s\n", RegPathToOpen.c_str());
        DWORD dwDisposition;
        
        DebugPrintf(SV_LOG_DEBUG,"RegPathToCreate - %s\n", RegPathToOpen.c_str());
        lRV = RegCreateKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,&hKey, &dwDisposition);
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_ERROR,"RegCreateKeyEx failed for Path - [%s] and return code - [%lu].\n", RegPathToOpen.c_str(), lRV);            
            rv = false; break;            
        }        
        
        if(false == SetValueREG_SZ(hKey, "GPO-ID", "LocalGPO"))
        {
            closeRegKey(hKey); rv = false; break;
        }

        if(false == SetValueREG_SZ(hKey, "SOM-ID", "Local"))
        {
            closeRegKey(hKey); rv = false; break;
        }

        std::string FileSysPathValue = SystemDrivePathOnVm + std::string("\\Windows\\System32\\GroupPolicy\\Machine");
        if(false == SetValueREG_SZ(hKey, "FileSysPath", FileSysPathValue))
        {
            closeRegKey(hKey); rv = false; break;
        }

        if(false == SetValueREG_SZ(hKey, "DisplayName", "Local Group Policy"))
        {
            closeRegKey(hKey); rv = false; break;
        }

        if(false == SetValueREG_SZ(hKey, "GPOName", "Local Group Policy"))
        {
            closeRegKey(hKey); rv = false; break;
        }

        if(bW2K8R2)
        {
            if(false == SetValueREG_DWORD(hKey, "PSScriptOrder", 1))
            {
                closeRegKey(hKey); rv = false; break;
            }
        }

        closeRegKey(hKey);

        // SubPath i.e. Path + \\0
        RegPathToOpen = RegPathToOpen + std::string("\\0");
        DebugPrintf(SV_LOG_DEBUG,"RegPathToCreate - %s\n", RegPathToOpen.c_str());
        DebugPrintf(SV_LOG_DEBUG,"Key to Open = %s\n", RegPathToOpen.c_str());
        lRV = RegCreateKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,&hKey, &dwDisposition);
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_ERROR,"RegCreateKeyEx failed for Path - [%s] and return code - [%lu].\n", RegPathToOpen.c_str(), lRV);
            rv = false; break;            
        }

        std::string ScriptPath = AgentInstallDir + std::string(STARTUP_EXE_PATH);
        if(false == SetValueREG_SZ(hKey, "Script", ScriptPath))
        {
            closeRegKey(hKey); rv = false; break;
        }

        std::string ParametersForExe;

		if (!m_strCloudEnv.empty())
		{
			ParametersForExe = std::string("-atbooting -resetguid -cloudenv ") + m_strCloudEnv;

			// In caspian v2, 
			//   drive letter mapping is created in <vx install path>/failover/data/inmage_drive.txt when consistency policy is run on source
			// so for failback to vmware
			//   pass p2v to install vmware tools
			//   pass the appropriate file location for nw and inmage_drive.txt 
			//   
			if (0 == strcmpi(m_strCloudEnv.c_str(), "vmware")){
				ParametersForExe += " -p2v";
				ParametersForExe = ParametersForExe + std::string(" -file ") + "\"" + AgentInstallDir + std::string("\\Failover\\Data\\nw.txt\"");
			}
			else{
				//TFS ID 1051609.
				if (!m_bWinNT)
					ParametersForExe = ParametersForExe + std::string(" -file ") + SystemDrivePathOnVm + std::string(":\\Windows") + std::string(NETWORK_FILE_PATH);
				else
					ParametersForExe = ParametersForExe + std::string(" -file ") + SystemDrivePathOnVm + std::string(":\\WINNT") + std::string(NETWORK_FILE_PATH);
			}
		}
		else
		{
			if(!m_bWinNT)
				ParametersForExe = std::string("-atbooting -file ") + SystemDrivePathOnVm + std::string("\\Windows") + std::string(NETWORK_FILE_PATH);
			else
				ParametersForExe = std::string("-atbooting -file ") + SystemDrivePathOnVm + std::string("\\WINNT") + std::string(NETWORK_FILE_PATH);
			if(true == m_p2v)
				ParametersForExe = ParametersForExe + " -p2v";
			if ( (true == m_p2v) && (false == m_bDrDrill) )
				ParametersForExe = ParametersForExe + " -resetguid";
		}
        DebugPrintf(SV_LOG_DEBUG,"ParametersExe - %s\n",ParametersForExe.c_str());
        if(false == SetValueREG_SZ(hKey, "Parameters", ParametersForExe))
        {
            closeRegKey(hKey); rv = false; break;
        }  

        /*if(false == SetValueREG_QWORD(hKey, "ExecTime", 0))
        {
            closeRegKey(hKey); rv = false; break;
        }*/

        if(bW2K8R2)
        {
            if(false == SetValueREG_DWORD(hKey, "IsPowershell", 0))
            {
                closeRegKey(hKey); rv = false; break;
            }
        }

        closeRegKey(hKey);
    }while(0);

    return rv;
}

/*
 Steps :    
    Update the following for each VM - 
A
    W2K8 - [HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Group Policy\Scripts\Startup\0]
    W2K3 - [HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Microsoft\Windows\System\Scripts\Startup\0]
    "GPO-ID"="LocalGPO"
    "SOM-ID"="Local"
    "FileSysPath"="C:\\Windows\\System32\\GroupPolicy\\Machine"
    "DisplayName"="Local Group Policy"
    "GPOName"="Local Group Policy"
    "PSScriptOrder"=dword:00000001      { only for W2K8 R2 }
B
    W2K8 - [HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Group Policy\Scripts\Startup\0\0]
    W2K3 - [HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Microsoft\Windows\System\Scripts\Startup\0\0]
    "Script"="C:\\Program Files\\InMage Systems\\EsxUtil.exe"
    "Parameters"="-file C:\\Windows\\temp\\nw.txt"
    "ExecTime"=hex(b):00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00
    "IsPowershell"=dword:00000000      { only for W2K8 R2 }
C
    [HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Group Policy\State\Machine\Scripts\Startup\0]
    "GPO-ID"="LocalGPO"
    "SOM-ID"="Local"
    "FileSysPath"="C:\\Windows\\System32\\GroupPolicy\\Machine"
    "DisplayName"="Local Group Policy"
    "GPOName"="Local Group Policy"
    "PSScriptOrder"=dword:00000001      { only for W2K8 R2 }
D
    [HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Group Policy\State\Machine\Scripts\Startup\0\0]
    "Script"="C:\\Program Files\\InMage Systems\\EsxUtil.exe"
    "Parameters"="-file C:\\Windows\\temp\\nw.txt"
    "ExecTime"=hex(b):00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00
    "IsPowershell"=dword:00000000      { only for W2K8 R2 }

    Since A,B and C,D entries are same except path, 
    write a function to set the keys and values and call it twice.
*/
bool ChangeVmConfig::RegAddStartupExeOnBoot(std::string SoftwareHiveName, std::string SystemDrivePathOnVm, bool bW2K8, bool bW2K8R2)
{
    bool rv = true;

    do
    {
        std::string RegPath1, RegPath2;

        if(!bW2K8) //i.e. for W2K3
        {
            RegPath1 = SoftwareHiveName + std::string("\\Policies\\Microsoft\\Windows\\System\\Scripts\\Startup\\0");
        }
        else 
        {
            RegPath1 = SoftwareHiveName + std::string("\\Microsoft\\Windows\\CurrentVersion\\Group Policy\\Scripts\\Startup\\0");
        }
        
        RegPath2 = SoftwareHiveName + std::string("\\Microsoft\\Windows\\CurrentVersion\\Group Policy\\State\\Machine\\Scripts\\Startup\\0");

        // Get Agent Install directory for value of Script in registry entries.
        std::string AgentInstallDir;
        if(false == RegGetInstallDir(SoftwareHiveName, AgentInstallDir))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get Install directory from registry for VM\n");
		    rv = false; break;
        }
        DebugPrintf(SV_LOG_DEBUG,"Install Directory - %s\n", AgentInstallDir.c_str());
                
        if(false == RegAddRegEntriesForStartupExe(SystemDrivePathOnVm, RegPath1, AgentInstallDir, bW2K8R2))
        {
            rv = false; break;
        }

        if(false == RegAddRegEntriesForStartupExe(SystemDrivePathOnVm, RegPath2, AgentInstallDir, bW2K8R2))
        {
            rv = false; true;
        }

    }while(0);

    return rv;
}

/*
    Write NW info in the file in following format for each nic (Separator - !@!@!)
        InterfaceCount!@!@!EnableDHCP!@!@!IPAddress!@!@!SubnetMask!@!@!DefaultGateway!@!@!NameServer!@!@!MacAddress!@!@!
*/
bool ChangeVmConfig::WriteNWInfoMapToFile(NetworkInfoMap_t NwMap, std::string FileName, std::string NwFileName, std::string ClusterNwFile)
{
    bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	NetworkInfoMap_t NwClusInfo;
	//preparing nw.txt in the mounted system volume of Recovered VM. (Windows\Temp Directory)

	FILE *fPtr;
	fPtr = fopen(NwFileName.c_str(), "w");

	if(fPtr == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s with error %d\n",NwFileName.c_str(), GetLastError());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
	}
	else
	{
		std::stringstream data;
		int fd = fileno(fPtr);

		DebugPrintf(SV_LOG_INFO,"Data to Enter in file %s are:\n", NwFileName.c_str());
		for(NetworkInfoMapIter_t i = NwMap.begin(); i != NwMap.end(); i++)
		{
			if(i->second.VirtualIps.empty())
			{
				DebugPrintf(SV_LOG_INFO,"%s!@!@!%lu!@!@!%s!@!@!%s!@!@!%s!@!@!%s!@!@!%s!@!@!\n", i->first.c_str(), i->second.EnableDHCP, i->second.IPAddress.c_str(), i->second.SubnetMask.c_str(), i->second.DefaultGateway.c_str(),i->second.NameServer.c_str(), i->second.MacAddress.c_str());
				data<<i->first<<"!@!@!"
				  <<i->second.EnableDHCP<<"!@!@!"
				  <<i->second.IPAddress<<"!@!@!"
				  <<i->second.SubnetMask<<"!@!@!"
				  <<i->second.DefaultGateway<<"!@!@!"
				  <<i->second.NameServer<<"!@!@!"
				  <<i->second.MacAddress<<"!@!@!"<<endl;
			}
			else
			{
				NetworkInfo PhysicalNw, VirtualNw;
				DiffPhysicalAndVirtualNw(i->second, PhysicalNw, VirtualNw);

				DebugPrintf(SV_LOG_INFO,"%s!@!@!%lu!@!@!%s!@!@!%s!@!@!%s!@!@!%s!@!@!%s!@!@!\n", i->first.c_str(), PhysicalNw.EnableDHCP, PhysicalNw.IPAddress.c_str(), PhysicalNw.SubnetMask.c_str(), PhysicalNw.DefaultGateway.c_str(),PhysicalNw.NameServer.c_str(), PhysicalNw.MacAddress.c_str());
				data<<i->first<<"!@!@!"
				  <<PhysicalNw.EnableDHCP<<"!@!@!"
				  <<PhysicalNw.IPAddress<<"!@!@!"
				  <<PhysicalNw.SubnetMask<<"!@!@!"
				  <<PhysicalNw.DefaultGateway<<"!@!@!"
				  <<PhysicalNw.NameServer<<"!@!@!"
				  <<PhysicalNw.MacAddress<<"!@!@!"<<endl;

				NwClusInfo.insert(make_pair(i->first,VirtualNw));
			}
		}
		fwrite(data.str().c_str(),data.str().size(),1,fPtr);
		fflush(fPtr);

		if(fd>0)
		{
			if(_commit(fd) < 0)
			{
				DebugPrintf(SV_LOG_ERROR,"Encountered error while trying to flush the filesystem for the file %s. Failed with error: %d\n",NwFileName.c_str(),errno);
				rv = false;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Encountered error while trying to open the file %s. Failed with error: %d\n",NwFileName.c_str(),errno);
			rv = false;
		}

		fclose(fPtr);
	}	

	if(!NwClusInfo.empty())
	{
		fPtr = fopen(ClusterNwFile.c_str(), "w");

		if(fPtr == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s with error %d\n",ClusterNwFile.c_str(), GetLastError());
			//Need to take action depends on the priority of this failure fornow ignoring
		}
		else
		{
			std::stringstream data;
			int fd = fileno(fPtr);

			DebugPrintf(SV_LOG_INFO,"Data to Enter in file %s are:\n", ClusterNwFile.c_str());
			for(NetworkInfoMapIter_t i = NwClusInfo.begin(); i != NwClusInfo.end(); i++)
			{
				DebugPrintf(SV_LOG_INFO,"%s!@!@!%lu!@!@!%s!@!@!%s!@!@!%s!@!@!%s!@!@!%s!@!@!\n", i->first.c_str(), i->second.EnableDHCP, i->second.IPAddress.c_str(), i->second.SubnetMask.c_str(), i->second.DefaultGateway.c_str(),i->second.NameServer.c_str(), i->second.MacAddress.c_str());
				data<<i->first<<"!@!@!"
				  <<i->second.EnableDHCP<<"!@!@!"
				  <<i->second.IPAddress<<"!@!@!"
				  <<i->second.SubnetMask<<"!@!@!"
				  <<i->second.DefaultGateway<<"!@!@!"
				  <<i->second.NameServer<<"!@!@!"
				  <<i->second.MacAddress<<"!@!@!"<<endl;
			}
			fwrite(data.str().c_str(),data.str().size(),1,fPtr);
			fflush(fPtr);

			if(fd>0)
			{
				if(_commit(fd) < 0)
				{
					DebugPrintf(SV_LOG_ERROR,"Encountered error while trying to flush the filesystem for the file %s. Failed with error: %d\n",NwFileName.c_str(),errno);
					rv = false;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Encountered error while trying to open the file %s. Failed with error: %d\n",NwFileName.c_str(),errno);
				rv = false;
			}

			fclose(fPtr);
		}
	}
	
	//Writing the same content of nw.txt[ of recovered vm] in MT's nw.txt, this file will be always in append mode.and located in VX_INSTALLATION_DIRECTORY
	
	ofstream FOUT(FileName.c_str(), std::ios::out | std::ios::app);
    if(! FOUT.is_open())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s with Error %d\n",FileName.c_str(), GetLastError());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
	for( NetworkInfoMapIter_t i = NwMap.begin(); i != NwMap.end(); i++)
	{
		FOUT<<i->first<<"!@!@!"
            <<i->second.EnableDHCP<<"!@!@!"
            <<i->second.IPAddress<<"!@!@!"
            <<i->second.SubnetMask<<"!@!@!"
            <<i->second.DefaultGateway<<"!@!@!"
            <<i->second.NameServer<<"!@!@!"
			<<i->second.MacAddress<<"!@!@!"
			<<i->second.VirtualIps<<"!@!@!"<<endl;
	}
    FOUT.close();

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//This function divides the physical to virtual(cluster) ips
//input -- combiantion of all the ips (NwInfo)
//output -- Physical ip details only (PhysicalNw)
//output -- Virtual ip details only (VirtualNw)
bool ChangeVmConfig::DiffPhysicalAndVirtualNw(NetworkInfo & NwInfo, NetworkInfo & PhysicalNw, NetworkInfo & VirtualNw)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	DebugPrintf(SV_LOG_INFO,"Old IP Addresses : %s\n",NwInfo.OldIPAddress.c_str());
	DebugPrintf(SV_LOG_INFO,"NEW IP Addresses : %s\n",NwInfo.IPAddress.c_str());
	DebugPrintf(SV_LOG_INFO,"SUBNET Addresses : %s\n",NwInfo.SubnetMask.c_str());
	DebugPrintf(SV_LOG_INFO,"GATEWAY Addresses : %s\n",NwInfo.DefaultGateway.c_str());
	DebugPrintf(SV_LOG_INFO,"DNS Addresses : %s\n",NwInfo.NameServer.c_str());
	DebugPrintf(SV_LOG_INFO,"Virtual IP Addresses : %s\n",NwInfo.VirtualIps.c_str());

	std::list<std::string> listOldIp;
    std::list<std::string> listIp;
	std::list<std::string> listSubNet;

    std::string Separator = std::string(",");

    size_t pos = 0;
	size_t found = NwInfo.OldIPAddress.find(Separator, pos);
	if(found == std::string::npos)
	{
		if(NwInfo.VirtualIps.find(NwInfo.OldIPAddress) == std::string::npos)
		{
			DebugPrintf(SV_LOG_DEBUG,"Only single Physical IP found in xml for this Nic.\n");
			PhysicalNw.EnableDHCP = NwInfo.EnableDHCP;
			PhysicalNw.IPAddress = NwInfo.IPAddress;
			PhysicalNw.MacAddress = NwInfo.MacAddress;
			PhysicalNw.NameServer = NwInfo.NameServer;
			PhysicalNw.SubnetMask = NwInfo.SubnetMask;
			PhysicalNw.DefaultGateway = NwInfo.DefaultGateway;
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG,"Only single virtual IP found in xml for this Nic.\n");
			VirtualNw.EnableDHCP = NwInfo.EnableDHCP;
			VirtualNw.IPAddress = NwInfo.IPAddress;
			VirtualNw.MacAddress = NwInfo.MacAddress;
			VirtualNw.NameServer = NwInfo.NameServer;
			VirtualNw.SubnetMask = NwInfo.SubnetMask;
			VirtualNw.DefaultGateway = NwInfo.DefaultGateway;
		}

		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return true;
	}
	else
	{
		while(found != std::string::npos)
		{
			DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",NwInfo.OldIPAddress.substr(pos,found-pos).c_str());
			listOldIp.push_back(NwInfo.OldIPAddress.substr(pos,found-pos));
			pos = found + 1; 
			found = NwInfo.OldIPAddress.find(Separator, pos);            
		}
		DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",NwInfo.OldIPAddress.substr(pos).c_str());
		listOldIp.push_back(NwInfo.OldIPAddress.substr(pos));
	}
	
	pos = 0;
	found = NwInfo.IPAddress.find(Separator, pos);
	if(found == std::string::npos)
	{
		DebugPrintf(SV_LOG_ERROR,"There is something wrong in collecting New IPAddress info From xml for this Nic \n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	else
	{
		while(found != std::string::npos)
		{
			DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",NwInfo.IPAddress.substr(pos,found-pos).c_str());
			listIp.push_back(NwInfo.IPAddress.substr(pos,found-pos));
			pos = found + 1; 
			found = NwInfo.IPAddress.find(Separator, pos);            
		}
		DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",NwInfo.IPAddress.substr(pos).c_str());
		listIp.push_back(NwInfo.IPAddress.substr(pos));
	}

	pos = 0;
	found = NwInfo.SubnetMask.find(Separator, pos);
	if(found == std::string::npos)
	{
		DebugPrintf(SV_LOG_ERROR,"There is something wrong in collecting NetMask info From xml for this Nic \n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	else
	{
		while(found != std::string::npos)
		{
			DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",NwInfo.SubnetMask.substr(pos,found-pos).c_str());
			listSubNet.push_back(NwInfo.SubnetMask.substr(pos,found-pos));
			pos = found + 1; 
			found = NwInfo.SubnetMask.find(Separator, pos);            
		}
		DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",NwInfo.SubnetMask.substr(pos).c_str());
		listSubNet.push_back(NwInfo.SubnetMask.substr(pos));
	}

	std::list<std::string>::iterator iterOldIp = listOldIp.begin();
	std::list<std::string>::iterator iterIp = listIp.begin();
	std::list<std::string>::iterator iterNet = listSubNet.begin();

	bool bPhyIp = false;
	bool bVirIP = false;
	for(; iterOldIp != listOldIp.end(); iterOldIp++)
	{
		if(NwInfo.VirtualIps.find(*iterOldIp) == std::string::npos)
		{
			if(bPhyIp)
			{
				PhysicalNw.IPAddress = PhysicalNw.IPAddress + std::string(",");
				PhysicalNw.SubnetMask = PhysicalNw.SubnetMask + std::string(",");
			}
			else
			{
				PhysicalNw.EnableDHCP = NwInfo.EnableDHCP;
				PhysicalNw.MacAddress = NwInfo.MacAddress;
				PhysicalNw.NameServer = NwInfo.NameServer;
				PhysicalNw.DefaultGateway = NwInfo.DefaultGateway;
				bPhyIp = true;
			}
			
			PhysicalNw.IPAddress = PhysicalNw.IPAddress + (*iterIp);
			PhysicalNw.SubnetMask = PhysicalNw.SubnetMask + (*iterNet);		
		}
		else
		{
			if(bVirIP)
			{
				VirtualNw.IPAddress = VirtualNw.IPAddress + std::string(",");
				VirtualNw.SubnetMask = VirtualNw.SubnetMask + std::string(",");
			}
			else
			{
				VirtualNw.EnableDHCP = NwInfo.EnableDHCP;
				VirtualNw.MacAddress = NwInfo.MacAddress;
				bVirIP = true;
			}
			
			VirtualNw.IPAddress = VirtualNw.IPAddress + (*iterIp);
			VirtualNw.SubnetMask = VirtualNw.SubnetMask + (*iterNet);		
		}
		iterIp++;
		iterNet++;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}



/*
    Get the Install directory of our agent from registry.
    Paths - 
    32 bit -
	    HKEY_LOCAL_MACHINE\SOFTWARE\SV Systems\VxAgent	
    64 bit - 
        HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\SV Systems\VxAgent
*/
bool ChangeVmConfig::RegGetInstallDir(std::string SoftwareHiveName, std::string & InstallDir)
{
    bool rv = true;

    HKEY hKey;

    do
    {
        std::string RegPathToOpen = SoftwareHiveName + std::string("\\SV Systems\\VxAgent");

        long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_READ, &hKey);
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu]. Trying alternate Path.\n", RegPathToOpen.c_str(), lRV);
            RegPathToOpen = SoftwareHiveName + std::string("\\Wow6432Node\\SV Systems\\VxAgent");
            lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_READ, &hKey);
            if( lRV != ERROR_SUCCESS )
            {
                DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu].\n", RegPathToOpen.c_str(), lRV);
                rv = false; break;
            }
        }

        if(false == RegGetValueOfTypeSzAndMultiSz(hKey, "InstallDirectory", InstallDir))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get value for InstallDirectory at Path - [%s].\n", RegPathToOpen.c_str());
            closeRegKey(hKey);
            rv = false; break;
        }

        closeRegKey(hKey);
    }while(0);

    return rv;
}


/*
    Get the Install directory of our agent from registry.
    Paths - 
    32 bit -
	    HKEY_LOCAL_MACHINE\SOFTWARE\SV Systems\VxAgent	
    64 bit - 
        HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\SV Systems\VxAgent
	After that get the value in registry for a particular key
*/
bool ChangeVmConfig::RegGetKeyFromInstallDir(string& SoftwareHiveName, const string strKeyName, string& strKeyValue)
{
    bool rv = true;

    HKEY hKey;

    do
    {
        std::string RegPathToOpen = SoftwareHiveName + std::string("\\SV Systems\\VxAgent");

        long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_READ, &hKey);
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu]. Trying alternate Path.\n", RegPathToOpen.c_str(), lRV);
            RegPathToOpen = SoftwareHiveName + std::string("\\Wow6432Node\\SV Systems\\VxAgent");
            lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_READ, &hKey);
            if( lRV != ERROR_SUCCESS )
            {
                DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu].\n", RegPathToOpen.c_str(), lRV);
                rv = false; break;
            }
        }

        if(false == RegGetValueOfTypeSzAndMultiSz(hKey, strKeyName, strKeyValue))
        {
			DebugPrintf(SV_LOG_ERROR,"Failed to get value fro key [%s] from InstallDirectory Path - [%s].\n", strKeyName.c_str(), RegPathToOpen.c_str());
			rv = false;
        }
        closeRegKey(hKey);
    }while(0);

    return rv;
}


/*
    Get the Install directory of our agent from registry.
    Paths - 
    32 bit -
	    HKEY_LOCAL_MACHINE\SOFTWARE\SV Systems\VxAgent	
    64 bit - 
        HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\SV Systems\VxAgent
	After that set the value in registry
*/
bool ChangeVmConfig::RegSetKeyAtInstallDir(string& SoftwareHiveName, const string strKeyName, string& strKeyValue)
{
    bool rv = true;

    HKEY hKey;

    do
    {
        std::string RegPathToOpen = SoftwareHiveName + std::string("\\SV Systems\\VxAgent");

        long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_ALL_ACCESS, &hKey);
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu]. Trying alternate Path.\n", RegPathToOpen.c_str(), lRV);
            RegPathToOpen = SoftwareHiveName + std::string("\\Wow6432Node\\SV Systems\\VxAgent");
            lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_ALL_ACCESS, &hKey);
            if( lRV != ERROR_SUCCESS )
            {
                DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu].\n", RegPathToOpen.c_str(), lRV);
                rv = false; break;
            }
        }

		string strOldKeyValue;
		if (false == RegGetValueOfTypeSzAndMultiSz(hKey, strKeyName, strOldKeyValue))   //Gets if any old value updated earlier
		{
			//hKey - will get close in case of failed so need to reopen it again
			lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_ALL_ACCESS, &hKey);
			if (lRV != ERROR_SUCCESS)
			{
				DebugPrintf(SV_LOG_ERROR, "RegOpenKeyEx failed for Path - [%s] and return code - [%lu].\n", RegPathToOpen.c_str(), lRV);
				rv = false; break;
			}
		}

		if (!strOldKeyValue.empty())
			strKeyValue = strOldKeyValue + "!@!@!" + strKeyValue;

        if(false == ChangeREG_SZ(hKey, strKeyName, strKeyValue))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to set value for InstallDirectory at Path - [%s].\n", RegPathToOpen.c_str());
			rv = false;
        }
        closeRegKey(hKey);
    }while(0);

    return rv;
}

/*
    Function to get sub folder names in given registry path
*/
bool ChangeVmConfig::RegGetSubFolders(std::string RegPathToOpen, std::vector<std::string> & SubFolders)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);        
    bool rv = true;

    SubFolders.clear();

    const unsigned int max_key_len = 255;
    char     achKey[max_key_len];   // buffer for subkey name    
    DWORD    cbName;                   // size of name string 
    TCHAR    achClass[MAX_PATH] = TEXT("");  // buffer for class name 
    DWORD    cchClassName = MAX_PATH;  // size of class string 
    DWORD    cSubKeys=0;               // number of subkeys 
    DWORD    cbMaxSubKey;              // longest subkey size 
    DWORD    cchMaxClass;              // longest class string 
    DWORD    cValues;              // number of values for key 
    DWORD    cchMaxValue;          // longest value name 
    DWORD    cbMaxValueData;       // longest value data 
    DWORD    cbSecurityDescriptor; // size of security descriptor 
    FILETIME ftLastWriteTime;      // last write time 
 
    DWORD i, retCode; 

    HKEY hKey;
    long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_READ, &hKey);
    if( lRV != ERROR_SUCCESS )
    {
        DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu].\n", RegPathToOpen.c_str(), lRV);
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);;
        return false;
    }
 
    // Get the class name and the value count. 
    retCode = RegQueryInfoKey(  hKey,                    // key handle 
                                achClass,                // buffer for class name 
                                &cchClassName,           // size of class string 
                                NULL,                    // reserved 
                                &cSubKeys,               // number of subkeys 
                                &cbMaxSubKey,            // longest subkey size 
                                &cchMaxClass,            // longest class string 
                                &cValues,                // number of values for this key 
                                &cchMaxValue,            // longest value name 
                                &cbMaxValueData,         // longest value data 
                                &cbSecurityDescriptor,   // security descriptor 
                                &ftLastWriteTime);       // last write time 
 
    // Enumerate the subkeys, until RegEnumKeyEx fails.    
    if (cSubKeys)
    {       
        for (i=0; i<cSubKeys; i++) 
        { 
            cbName = max_key_len;
            retCode = RegEnumKeyEx(  hKey, i,
                                     achKey, 
                                     &cbName, 
                                     NULL, 
                                     NULL, 
                                     NULL, 
                                     &ftLastWriteTime); 
            if (retCode == ERROR_SUCCESS) 
            {
                DebugPrintf(SV_LOG_INFO,"(%d) %s\n", i+1, achKey);
                SubFolders.push_back(std::string(achKey));
            }
        }
    } 
    else
    {
        DebugPrintf(SV_LOG_ERROR,"No Sub Folders are found in path - %s.\n", RegPathToOpen.c_str());
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);        
        closeRegKey(hKey);
        return false;
    }

    closeRegKey(hKey);

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n",__FUNCTION__);        
    return rv;
}

/*
    Convert the DWORD to string and make the string count to 3
    Ex: if DWORD is 1, return 001 / if 11, return 011
*/
void ChangeVmConfig::FormatCCS(DWORD dwCCS, std::string & CCS)
{
    std::stringstream ss;
    ss<<dwCCS;
    ss>>CCS;
    
    switch(CCS.size())
    {
    case 0:
        // Error condition. This should not come. Will be handled prior to this call.
        DebugPrintf(SV_LOG_ERROR, "Current Control set value is empty.\n");
        break;
    case 1:
        CCS = std::string(CONTROL_SET_PREFIX) + std::string("00") + CCS;
        break;
    case 2:
        CCS = std::string(CONTROL_SET_PREFIX) + std::string("0") + CCS;
        break;
    default:
        CCS = std::string(CONTROL_SET_PREFIX) + CCS;
        break;
    }
}

/*
    Make final NwInfo map using NwMapFromReg(from reg) and NwMapFromXml(user defined)
    Logic - 
    NwMapFromXml has got more priority as it has come from user to update.
    If map count is equal
        if any value exist(in NwMapFromXml) for corresponding value(in NwMapFromReg), 
            write value from NwMapFromXml in NwMapFinal
        else
            write the value from NwMapFromReg in NwMapFinal.
    If map count is not equal in both, then add those entries(from either maps) directly to final map.
*/
void ChangeVmConfig::UpdateNwInfoMapWithNewValues(NetworkInfoMap_t NwMapFromReg, NetworkInfoMap_t NwMapFromXml, NetworkInfoMap_t & NwMapFinal)
{
    NetworkInfoMapIter_t iterReg = NwMapFromReg.begin();
    NetworkInfoMapIter_t iterXml = NwMapFromXml.begin();

    unsigned int count = 1;
    while(1)
    {
        std::string strCount;
        std::stringstream ss;
        ss<<count;
        ss>>strCount;
        count++;

        if ( (iterReg == NwMapFromReg.end()) && (iterXml == NwMapFromXml.end()) )
        {
            break;
        }
        else if (iterReg == NwMapFromReg.end())
        {
            NwMapFinal.insert(make_pair(strCount,iterXml->second));
            iterXml++;
        }
        else if (iterXml == NwMapFromXml.end())
        {
            NwMapFinal.insert(make_pair(strCount,iterReg->second));
            iterReg++;
        }
        else
        {
            NetworkInfo n;
            
            //if (false == iterXml->second.IPAddress.empty()) //since new ip is given, dhcp is 0
            //    n.EnableDHCP = 0;
            //else
            //    n.EnableDHCP = iterReg->second.EnableDHCP;
            n.EnableDHCP = iterXml->second.EnableDHCP;

            if (0 == n.EnableDHCP)
            {
                if (false == iterXml->second.IPAddress.empty())
                    n.IPAddress = iterXml->second.IPAddress;
                else if (false == iterReg->second.IPAddress.empty())
                    n.IPAddress = iterReg->second.IPAddress;

                if (false == iterXml->second.SubnetMask.empty())
                    n.SubnetMask = iterXml->second.SubnetMask;
                else if (false == iterReg->second.SubnetMask.empty())
                    n.SubnetMask = iterReg->second.SubnetMask;

                if (false == iterXml->second.DefaultGateway.empty())
                    n.DefaultGateway = iterXml->second.DefaultGateway;
                else if (false == iterReg->second.DefaultGateway.empty())
                    n.DefaultGateway = iterReg->second.DefaultGateway;

                if (false == iterXml->second.NameServer.empty())
                    n.NameServer = iterXml->second.NameServer;
                else if (false == iterReg->second.NameServer.empty())
                    n.NameServer = iterReg->second.NameServer;
            }

            NwMapFinal.insert(make_pair(strCount,n));
            iterReg++; iterXml++;
        }
    }
}

/*
    Description : Changes the DWORD values in the registry.
    Input       : The hkey to the registry, Key name and value for the key
    Output      : true if successfully sets the value
*/
bool ChangeVmConfig::SetValueREG_DWORD(HKEY hKey, std::string Key, DWORD Value)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool rv = true;

    do
    {
        long lRV = RegSetValueEx(hKey, Key.c_str(), 0, REG_DWORD, reinterpret_cast<BYTE *>(&Value), sizeof(Value));
	    if(lRV == ERROR_SUCCESS)
	    {            
            DebugPrintf(SV_LOG_DEBUG, " Successfully set  \"%s\"=%lu\n", Key.c_str(), Value);		
	    }
	    else
	    {
		    DebugPrintf(SV_LOG_ERROR," Failed to set \"%s\"=\"%s\". RegSetValueEx Return Value - %lu\n", Key.c_str(), Value, lRV);
		    rv = false; break;
	    }
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return rv;
}

/*
    Description : Changes the REG_SZ values in the registry.
    Input       : The hkey to the registry, Key name and value for the key
    Output      : true if successfully sets the value
*/
bool ChangeVmConfig::SetValueREG_SZ(HKEY hKey, std::string Key, std::string Value)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool rv = true;

    do
    {
        long lRV = RegSetValueEx(hKey, Key.c_str() , NULL, REG_SZ,(LPBYTE)Value.c_str(), Value.size()+1);
	    if(lRV == ERROR_SUCCESS)
	    {
            DebugPrintf(SV_LOG_DEBUG," Successfully set  \"%s\"=\"%s\"\n", Key.c_str(), Value.c_str());
	    }
	    else
	    {
            DebugPrintf(SV_LOG_ERROR," Failed to set \"%s\"=\"%s\". RegSetValueEx Return Value - %lu\n", Key.c_str(), Value.c_str(), lRV);
		    rv = false; break;
	    }
    }while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return rv;
}

//***************************************************************************************
// Fetch the Source Machines drive letter which contains system32 folder.
// Read from xml file.
// Input  - Hostname 
// Output - Source System Drive (Format - C:\ )
//***************************************************************************************
bool ChangeVmConfig::xGetSrcSystemVolume(std::string HostName, std::string & SrcSystemVolume)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	xmlDocPtr xDoc;	
	xmlNodePtr xCur;	

	std::string XmlFileName;
	if(m_bDrDrill)
		XmlFileName = m_strDatafolderPath + std::string(SNAPSHOT_FILE_NAME);
	else
		XmlFileName = m_strDatafolderPath + std::string(RECOVERY_FILE_NAME);

    DebugPrintf(SV_LOG_INFO,"XML File - %s\n", XmlFileName.c_str());

	xDoc = xmlParseFile(XmlFileName.c_str());
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
		DebugPrintf(SV_LOG_ERROR,"The XML is empty. Cannot Proceed further.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}

	if (xmlStrcmp(xCur->name,(const xmlChar*)"root")) 
	{
		//root is not found
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <root> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}

	//If you are here root entry is found. Go for SRC_ESX entry.
	xCur = xGetChildNode(xCur,(const xmlChar*)"SRC_ESX");
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <SRC_ESX> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}

	xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"inmage_hostid",(const xmlChar*)HostName.c_str());
	if (xCur == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host with required inmage_hostid> entry not found\n");
		//If you are here SRC_ESX entry is found. Go for host entry with required HostName.
		xCur = xGetChildNodeWithAttr(xCur,(const xmlChar*)"host",(const xmlChar*)"hostname",(const xmlChar*)HostName.c_str());
		if (xCur == NULL)
		{
			DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host with required hostname> entry not found\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			xmlFreeDoc(xDoc); return false;
		}
	}

	xmlChar *attr_value_temp;
	attr_value_temp = xmlGetProp(xCur,(const xmlChar*)"system_volume");
	if (attr_value_temp == NULL)
	{
		DebugPrintf(SV_LOG_ERROR,"The XML document is not in expected format : <host system_volume> entry not found\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
    SrcSystemVolume = string((char *)attr_value_temp);
	if(!SrcSystemVolume.empty())
		SrcSystemVolume = SrcSystemVolume + string(":\\");
	else
	{
		DebugPrintf(SV_LOG_INFO,"Found empty system volume from XML file\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		xmlFreeDoc(xDoc); return false;
	}
    
    xmlFreeDoc(xDoc); 

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}



/*
description: Changes the Dword values in the registry. when ever you want to change the DWord value in registry just call this function
Input: the key to the registry,the value name,the value.
Output : Successful set of the value in the given key.
*/
bool ChangeVmConfig::ChangeDWord(HKEY hKey,std::string strName,DWORD dwValue)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bFlag = true;
	if(RegSetValueEx(hKey,strName.c_str(), 0, REG_DWORD,reinterpret_cast<BYTE *>(&dwValue), sizeof(dwValue)) == ERROR_SUCCESS)
	{
        DebugPrintf(SV_LOG_DEBUG, " Successfully set  \"%s\"=%lu\n",strName.c_str(), dwValue);		
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR," Failed to set value for \"%s\"\n",strName.c_str());
		bFlag = false;
		return bFlag;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bFlag;
}
/*
description: Changes the REG_SZ type values in the registry. when ever you want to change the REG_SZ value in registry just call this function
Input: the key to the registry,the value name,the value.
Output : Successful set of the value in the given key.
*/
bool ChangeVmConfig::ChangeREG_SZ(HKEY hKey,std::string strName,std::string strTempRegValue)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bFlag = true;
	if(RegSetValueEx(hKey, strName.c_str() , NULL, REG_SZ,(LPBYTE) strTempRegValue.c_str(), strTempRegValue.size()+1)==ERROR_SUCCESS)
	{
        DebugPrintf(SV_LOG_INFO, " Successfully set  \"%s\"=\"%s\"\n",strName.c_str(), strTempRegValue.c_str());
	}
	else
	{
        DebugPrintf(SV_LOG_ERROR," Failed to set value for \"%s\"\n",strName.c_str());
		bFlag = false;
		return bFlag;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bFlag;
}
/*
description: Changes the REG_EXPAND_SZ type values in the registry. when ever you want to change the REG_EXPAND_SZ value in registry just call this function
Input: the key to the registry,the value name,the value.
Output : Successful set of the value in the given key.
*/
bool ChangeVmConfig::ChangeREG_EXPAND_SZ(HKEY hKey,std::string strName,std::string strTempRegValue)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bFlag = true;
	if(RegSetValueEx(hKey, strName.c_str() , NULL, REG_EXPAND_SZ,(LPBYTE) strTempRegValue.c_str(), strTempRegValue.size()+1)==ERROR_SUCCESS)
	{
        DebugPrintf(SV_LOG_INFO, " Successfully set  \"%s\"=\"%s\"\n",strName.c_str(), strTempRegValue.c_str());
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR," Failed to set value for \"%s\"\n",strName.c_str());
		bFlag = false;
		return bFlag;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bFlag;
}

/*
description: Changes the REG_MULTI_SZ type values in the registry. when ever you want to change the REG_MULTI_SZ value in registry just call this function
Input: the key to the registry,the value name,the value.
Output : Successful set of the value in the given key.
*/
bool ChangeVmConfig::ChangeREG_MULTI_SZ(HKEY hKey,std::string strName,std::string strTempRegValue)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bFlag = true;
	if(RegSetValueEx(hKey, strName.c_str() , NULL, REG_MULTI_SZ,(LPBYTE) strTempRegValue.c_str(), strTempRegValue.size()+1)==ERROR_SUCCESS)
	{
        DebugPrintf(SV_LOG_INFO, " Successfully set  \"%s\"=\"%s\"\n",strName.c_str(), strTempRegValue.c_str());
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR," Failed to set value for \"%s\"\n",strName.c_str());
		bFlag = false;
		return bFlag;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bFlag;
}

/*
Checks whether Registry Path passed as argument exists or not under HKEY_LOCAL_MACHINE. As most of the time we are importing hive to HKLM this has been hardcoded.
***** NEED TO TAKE WHICH REGISTRY TO BE CHECKED AS WELL AS PARAMETER *****
Logic: Call RegOpenKeyEx() on Path as PATH TO BE CHEDKED,
		if 2 == ReturnValue, then path does not exist,return FALSE
		else if ERROR_SUCCESS != ReturnValue , then Failed to check, return FALSE
		else, path exists , return TRUE		
*/
bool ChangeVmConfig::DoesRegistryPathExists( std::string checkRegPath )
{
	bool ReturnValue = false;

	HKEY hKey;
	long lRV = RegOpenKeyEx( HKEY_LOCAL_MACHINE, checkRegPath.c_str(), 0, KEY_ALL_ACCESS , &hKey);
	if( ERROR_FILE_NOT_FOUND == lRV )
	{
		DebugPrintf(SV_LOG_INFO,"Registry Path [%s] does not exist.\n", checkRegPath.c_str());
		return ReturnValue;
	}
    else if( lRV != ERROR_SUCCESS )
    {
		DebugPrintf(SV_LOG_INFO,"Failed to open registry path [%s] with error code %ld.\n", checkRegPath.c_str(), lRV );
		return ReturnValue;
	}
	
	DebugPrintf(SV_LOG_INFO,"Registry Path [%s] exists.\n", checkRegPath.c_str());
	ReturnValue	= true;
	closeRegKey(hKey);
	return ReturnValue;
}

/*
Deletes Registry path specified by deleting it's subkeys recursively.
***** NEED TO TAKE WHICH REGISTRY TO BE CHECKED AS WELL AS PARAMETER *****
Algo: Open Path to registry using RegOpenKeyEx(),
		QueryInfo on opened registry path to know how many subKeys exists under that path.
		if ( subKey > 0 )
		{
			Get SubKeyName using RegEnumKeyEx().
			probe SubkeyPath with function DeleteRegistryTree().This is recursive call.
		}
		else
		{
			Delete Path.
		}
*/
bool ChangeVmConfig::DeleteRegistryTree( std::string regPath )
{
	DebugPrintf(SV_LOG_INFO , "Probing registry path [%s].\n", regPath.c_str());
	bool ReturnValue = false;
	HKEY hKey;
	long lRV = RegOpenKeyEx( HKEY_LOCAL_MACHINE ,regPath.c_str(), 0, KEY_ALL_ACCESS , &hKey );
    if( lRV != ERROR_SUCCESS )
    {
		DebugPrintf(SV_LOG_ERROR,"Failed to open registry path [%s] with error code %ld.\n", regPath.c_str(), lRV);
		return ReturnValue;
	}
	
	DWORD lpcSubKeys;
	DWORD lpcMaxSubKeyLen;
	lRV = RegQueryInfoKey( hKey , NULL , NULL , NULL, &lpcSubKeys , &lpcMaxSubKeyLen , NULL , NULL , NULL , NULL , NULL , NULL );
	if( ERROR_SUCCESS != lRV )
	{
		closeRegKey(hKey);
		DebugPrintf(SV_LOG_ERROR,"Failed to query registry path [%s] with error code %ld.\n", regPath.c_str(), lRV);
		return ReturnValue;
	}
	
	DebugPrintf(SV_LOG_INFO , "Number of subkeys under registry path [%s] = %ld.\n" , regPath.c_str() , lpcSubKeys );

	if( lpcSubKeys )
	{
		TCHAR    achKey[MAX_KEY_LENGTH];   // buffer for subkey name
		DWORD    cbName;

		for (DWORD index = lpcSubKeys; index > 0; index--)
		{			
			cbName	= MAX_KEY_LENGTH;
			lRV	= RegEnumKeyEx( hKey , index-1 , achKey , &cbName , NULL , NULL , NULL , NULL );
			if ( ERROR_SUCCESS != lRV )
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to enumerate key under registry path [%s], with error code %ld.\n" , regPath.c_str() , lRV);
				break;
			}
			else
			{
				string newRegPath = regPath + string("\\") + achKey;
				if ( false == DeleteRegistryTree( newRegPath ) )
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to delete registry path [%s].\n", newRegPath.c_str());
					break;
				}
			}
		}
		if (RegDelGivenKey(regPath))
		{
			ReturnValue = true;
		}
	}
	else
	{
		ReturnValue	= RegDelGivenKey( regPath );
	}
	
	closeRegKey(hKey);
	return ReturnValue;		
}

/*
Description : it changes all the registry values for the driver we need(symmpi or LSI_SCSI)
Input : RegistryName(symmpi or LSI_SCSI),boolean value to state whether it is W2K8_R2 or not.
Output : Successful setting of the registry entries.
*/
bool ChangeVmConfig::ChangeRegistry(std::string SystemHiveName, std::string strRegName, bool bRegDelete, string strPhyOsType)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

    std::vector<std::string> vecControlSets;
    if(false == GetControlSets(vecControlSets))
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to get Control Sets vector for this VM.\n");
        DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
        return false;
    }

    bool bFinalRetValue = true;

    std::vector<std::string>::iterator iterCS = vecControlSets.begin();
    for(; iterCS != vecControlSets.end(); iterCS++)
    {
		DebugPrintf(SV_LOG_INFO , "Performing registry in control set [%s].\n", (*iterCS).c_str() );
		//We need to delete "rhelscsi" interface registry entry and it's corresponding PCI#vendevice

		if(bRegDelete)
		{
			do
			{
				//find if PCI#Ven exists or not
				std::string regPath = std::string(VM_SYSTEM_HIVE_NAME) + string("\\") + (*iterCS) + string("\\") + std::string("Control\\CriticalDeviceDatabase\\PCI#VEN_5853&DEV_0001");
				if( DoesRegistryPathExists( regPath ) )
				{
					if( false == RegDelGivenKey(regPath) )
					{
						DebugPrintf(SV_LOG_ERROR , "Failed to delete reistry path [%s].\n",regPath.c_str());
						//Continuing furthur registry change operations with out erroring.
					}
				}

				regPath	= std::string(VM_SYSTEM_HIVE_NAME) + string("\\") + (*iterCS) + string("\\") + std::string("services\\rhelscsi");
				if( DoesRegistryPathExists( regPath ) )
				{
					if( false == DeleteRegistryTree( regPath ) )
					{
						DebugPrintf(SV_LOG_ERROR , "Failed to delete reistry path [%s].\n",regPath.c_str());
						//Continuing furthur registry change operations with out erroring.
					}
				}
			}while(0);
		}

        bool rv = true;
        do
        {
	        DWORD lRv;
	        HKEY hKey;
	        DWORD dwDisposition;
	        string strInterfaceKey;
	        std::string strTempRegValue;

			if((strPhyOsType.compare("Windows_2012") != 0) && (strPhyOsType.compare("Windows_8") != 0))
			{
				if(strRegName.compare("LSI_SAS") == 0)
					strInterfaceKey = SystemHiveName + string("\\") + *iterCS + string("\\Control\\CriticalDeviceDatabase\\PCI#VEN_1000&DEV_0054");
				else
					strInterfaceKey = SystemHiveName + string("\\") + *iterCS + string("\\Control\\CriticalDeviceDatabase\\pci#ven_1000&dev_0030");

				DebugPrintf(SV_LOG_INFO, "Registry Path = [%s]\n",strInterfaceKey.c_str());
				lRv = RegCreateKeyEx(HKEY_LOCAL_MACHINE,strInterfaceKey.c_str(),0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,&hKey, &dwDisposition);
				if(lRv != ERROR_SUCCESS)
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to create the key for [%s] with error code - %lu\n", strInterfaceKey.c_str(), lRv); 
					rv = false;
					break;
				}	       	
				if(strRegName.compare("LSI_SCSI") == 0)
					 strTempRegValue = "LSI_SCSI";
				else if(strRegName.compare("LSI_SAS") == 0)
					strTempRegValue = "LSI_SAS";
				else
					strTempRegValue = "symmpi";        	
				if(false == ChangeREG_SZ(hKey,"Service",strTempRegValue))
				{                
					rv = false;
					closeRegKey(hKey);
					break;
				}        	
				strTempRegValue = "{4D36E97B-E325-11CE-BFC1-08002BE10318}";
				if(false == ChangeREG_SZ(hKey,"ClassGUID",strTempRegValue))
				{                
					rv = false;
					closeRegKey(hKey);
					break;
				}
				//set this if at all the machine was 2008R2
				if((strPhyOsType.compare("Windows_2008_R2") == 0) || (strPhyOsType.compare("Windows_7_64") == 0) || (strPhyOsType.compare("Windows_7_32") == 0))
				{
					//set the DriverPackageId.
					if(strRegName.compare("LSI_SCSI") == 0)
						strTempRegValue = "lsi_scsi.inf_amd64_neutral_cfbbf0b0b66ba280";
					else if(strRegName.compare("LSI_SAS") == 0)
						strTempRegValue = "lsi_sas.inf_amd64_neutral_a4d6780f72cbd5b4";

					if(false == ChangeREG_SZ(hKey,"DriverPackageId",strTempRegValue))
					{                
						rv = false;
						closeRegKey(hKey);
						break;
					}
				}        	
				closeRegKey(hKey); //close the registry key for criticaldatabasedevice
			}
        	
	        //this is the key for LSI_SCSI or symmpi
	        //strInterfaceKey = std::string("Inmage_Loaded_System_Hive\\ControlSet001\\Services\\") + strRegName;
            strInterfaceKey = SystemHiveName + string("\\") + *iterCS + string("\\Services\\") + strRegName;
            DebugPrintf(SV_LOG_INFO, "Registry Path = [%s]\n",strInterfaceKey.c_str());
	        lRv = RegCreateKeyEx(HKEY_LOCAL_MACHINE,strInterfaceKey.c_str(),0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,&hKey, &dwDisposition);
	        if(lRv != ERROR_SUCCESS)
	        {
                DebugPrintf(SV_LOG_ERROR, "Failed to create the key for [%s] with error code - %lu\n", strInterfaceKey.c_str(), lRv); 
                rv = false;
                break;
	        }
	        //Group
	        strTempRegValue = "SCSI miniport";
	        if(false == ChangeREG_SZ(hKey,"GROUP",strTempRegValue))
            {                
	            rv = false;
                closeRegKey(hKey);
                break;
            }
	        //Image Path
	        if(strRegName.compare("LSI_SCSI") == 0)
		        strTempRegValue = "system32\\DRIVERS\\lsi_scsi.sys";
			else if(strRegName.compare("LSI_SAS") == 0)
				strTempRegValue = "System32\\DRIVERS\\lsi_sas.sys";
	        else
		        strTempRegValue = "system32\\DRIVERS\\symmpi.sys";

	        if(false == ChangeREG_EXPAND_SZ(hKey,"ImagePath",strTempRegValue))
		    {                
	            rv = false;
                closeRegKey(hKey);
                break;
            }
	        //Error Control
            DWORD dwValue = 1;
	        if(false == ChangeDWord(hKey,"ErrorControl",dwValue))
	        {                
	            rv = false;
                closeRegKey(hKey);
                break;
            }
	        //Start
	        dwValue = 0;        	
	        if(false == ChangeDWord(hKey,"Start",dwValue))
            {                
	            rv = false;
                closeRegKey(hKey);
                break;
            }
	        //Type
	        dwValue = 1;
	        if(false == ChangeDWord(hKey,"Type",dwValue))
		    {                
	            rv = false;
                closeRegKey(hKey);
                break;
            }        	
	        //Tag
	        if(strRegName.compare("LSI_SCSI") == 0)
	        {
		        dwValue = 34;
		        if((strPhyOsType.compare("Windows_2008_R2") == 0) || (strPhyOsType.compare("Windows_7_64") == 0) || (strPhyOsType.compare("Windows_7_32") == 0))
		        {
			        dwValue = 64;//in case of W2K8_R2
		        }
	        }
			else if(strRegName.compare("LSI_SAS") == 0)
	        {
		        dwValue = 40;
		        if((strPhyOsType.compare("Windows_2008_R2") == 0) || (strPhyOsType.compare("Windows_7_64") == 0) || (strPhyOsType.compare("Windows_7_32") == 0))
		        {
			        dwValue = 22;//in case of W2K8_R2
		        }
				else if((strPhyOsType.compare("Windows_2012") == 0) || (strPhyOsType.compare("Windows_8") == 0))
				{
					dwValue = 68;
				}
	        }
	        else
		        dwValue = 33;        	
	        if(false == ChangeDWord(hKey,"Tag",dwValue))
		    {                
	            rv = false;
                closeRegKey(hKey);
                break;
            }
	        //set this if at all the machine was 2008R2
	        if((strPhyOsType.compare("Windows_2008_R2") == 0) || (strPhyOsType.compare("Windows_7_64") == 0) || (strPhyOsType.compare("Windows_7_32") == 0))
	        {
		        //set the DriverPackageId.
				if(strRegName.compare("LSI_SCSI") == 0)
					strTempRegValue = "lsi_scsi.inf_amd64_neutral_cfbbf0b0b66ba280";
				else if(strRegName.compare("LSI_SAS") == 0)
					strTempRegValue = "lsi_sas.inf_amd64_neutral_a4d6780f72cbd5b4";

		        if(false == ChangeREG_SZ(hKey,"DriverPackageId",strTempRegValue))
		        {                
	                rv = false;
                    closeRegKey(hKey);
                    break;
                }  
	        }
			if((strPhyOsType.compare("Windows_2012") == 0) || (strPhyOsType.compare("Windows_8") == 0))
			{
				//set the DriverPackageId.
				if(strRegName.compare("LSI_SAS") == 0)
					strTempRegValue = "lsi_sas.inf";
		        if(false == ChangeREG_MULTI_SZ(hKey,"Owners",strTempRegValue))
		        {                
	                rv = false;
                    closeRegKey(hKey);
                    break;
                }
			}
	        lRv = closeRegKey(hKey); //close the registry key of LSI_SCSI or the symmpi...

	        //registry key name = Parameters
	        //strInterfaceKey = std::string("Inmage_Loaded_System_Hive\\ControlSet001\\Services\\") + strRegName +std::string("\\Parameters");
	        strInterfaceKey = SystemHiveName + string("\\") + *iterCS + string("\\Services\\") + strRegName +std::string("\\Parameters");
            DebugPrintf(SV_LOG_INFO, "Registry Path = [%s]\n",strInterfaceKey.c_str());
	        lRv = RegCreateKeyEx(HKEY_LOCAL_MACHINE,strInterfaceKey.c_str(),0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,&hKey, &dwDisposition);
	        if(lRv != ERROR_SUCCESS)
	        {
                DebugPrintf(SV_LOG_ERROR, "Failed to create the key for [%s] with error code - %lu\n", strInterfaceKey.c_str(), lRv); 
                rv = false;
                break;
	        }
	        //Bus Type	
	        if(strRegName.compare("LSI_SCSI") == 0)
		        dwValue = 1;
			else if(strRegName.compare("LSI_SAS") == 0)
				dwValue = 10;
	        else
		        dwValue = 6;
	        if(false == ChangeDWord(hKey,"BusType",dwValue))
		    {                
	            rv = false;
                closeRegKey(hKey);
                break;
            }        	
	        lRv = closeRegKey(hKey);  //close the handle of parameter's
        	
	        //open handle for the key name = PnpInterface
	        //strInterfaceKey = std::string("Inmage_Loaded_System_Hive\\ControlSet001\\Services\\") + strRegName + std::string("\\Parameters\\PnpInterface");
	        strInterfaceKey = SystemHiveName + string("\\") + *iterCS + string("\\Services\\") + strRegName + std::string("\\Parameters\\PnpInterface");
            DebugPrintf(SV_LOG_INFO, "Registry Path = [%s]\n",strInterfaceKey.c_str());
	        lRv = RegCreateKeyEx(HKEY_LOCAL_MACHINE,strInterfaceKey.c_str(),0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,&hKey, &dwDisposition);
	        if(lRv != ERROR_SUCCESS)
	        {
                DebugPrintf(SV_LOG_ERROR, "Failed to create the key for [%s] with error code - %lu\n", strInterfaceKey.c_str(), lRv); 
                rv = false;
                break;
	        }
	        //"5"
	        dwValue = 1;
	        if(false == ChangeDWord(hKey,"5",dwValue))
		    {                
	            rv = false;
                closeRegKey(hKey);
                break;
            }        	
	        lRv = closeRegKey(hKey); //close the Key PnpInterface

	        //open the handle for the Enum
	        //strInterfaceKey = std::string("Inmage_Loaded_System_Hive\\ControlSet001\\Services\\") + strRegName +std::string("\\Enum");
	        strInterfaceKey = SystemHiveName + string("\\") + *iterCS + string("\\Services\\") + strRegName +std::string("\\Enum");
            DebugPrintf(SV_LOG_INFO, "Registry Path = [%s]\n",strInterfaceKey.c_str());
	        lRv = RegCreateKeyEx(HKEY_LOCAL_MACHINE,strInterfaceKey.c_str(),0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,&hKey, &dwDisposition);
	        if(lRv != ERROR_SUCCESS)
	        {
                DebugPrintf(SV_LOG_ERROR, "Failed to create the key for [%s] with error code - %lu\n", strInterfaceKey.c_str(), lRv); 
                rv = false;
                break;
	        }
	        //Count		
	        dwValue = 1;
	        if(false == ChangeDWord(hKey,"Count",dwValue))
		    {                
	            rv = false;
                closeRegKey(hKey);
                break;
            }
	        //Next Instance
	        dwValue = 1;
	        if(false == ChangeDWord(hKey,"NextInstance",dwValue))
		    {                
	            rv = false;
                closeRegKey(hKey);
                break;
            }        	
	        //"0"
	        if(strRegName.compare("LSI_SCSI") == 0)
		        strTempRegValue = "PCI\\VEN_1000&DEV_0030&SUBSYS_197615AD&REV_01\\4&b70f118&0&1088";
			else if(strRegName.compare("LSI_SAS") == 0)
				strTempRegValue = "PCI\\VEN_1000&DEV_0054&SUBSYS_197615AD&REV_01\\4&2732702b&1&00A8";
	        else
		        strTempRegValue = "PCI\\VEN_1000&DEV_0030&SUBSYS_00000000&REV_01\\3&61aaa01&0&80";
	        if(false == ChangeREG_SZ(hKey,"0",strTempRegValue))
		    {                
	            rv = false;
                closeRegKey(hKey);
                break;
            }        	
	        lRv=closeRegKey(hKey);

			if((strPhyOsType.compare("Windows_2012") == 0) || (strPhyOsType.compare("Windows_8") == 0))
			{
				//open the handle for the Device
				strInterfaceKey = SystemHiveName + string("\\") + *iterCS + string("\\Services\\") + strRegName +std::string("\\Parameters\\Device");
				DebugPrintf(SV_LOG_INFO, "Registry Path = [%s]\n",strInterfaceKey.c_str());
				lRv = RegCreateKeyEx(HKEY_LOCAL_MACHINE,strInterfaceKey.c_str(),0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,&hKey, &dwDisposition);
				if(lRv != ERROR_SUCCESS)
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to create the key for [%s] with error code - %lu\n", strInterfaceKey.c_str(), lRv); 
					rv = false;
					break;
				}
				strTempRegValue = "PlaceHolder=0;";
				if(false == ChangeREG_SZ(hKey,"DriverParameter",strTempRegValue))
				{                
					rv = false;
					closeRegKey(hKey);
					break;
				}
				dwValue = 1;
				if(false == ChangeDWord(hKey,"EnableQueryAccessAlignment",dwValue))
				{                
					rv = false;
					closeRegKey(hKey);
					break;
				}
				lRv=closeRegKey(hKey);
			}
        }while(0);

        if(false == rv)
        {
            bFinalRetValue = false;
        }
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bFinalRetValue;
}

/**************************************************************************************************
***************************************************************************************************/
bool ChangeVmConfig::ExportRegistryFile(string regPath, const string regFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	string strRegistryExportCommand = REGISTRY_EXPORT_CMD + string("\"") + regFile +string("\"") + string(" ")+ regPath;
	DebugPrintf(SV_LOG_INFO,"Command to Execute Registry Export: %s\n", strRegistryExportCommand.c_str());

	STARTUPINFO StartupInfo;
	PROCESS_INFORMATION ProcessInformation;
	memset(&StartupInfo, 0x00, sizeof(STARTUPINFO));
	StartupInfo.cb = sizeof(STARTUPINFO);
	memset(&ProcessInformation, 0x00, sizeof(PROCESS_INFORMATION));
	if(!::CreateProcess(NULL,const_cast<char*>(strRegistryExportCommand.c_str()),NULL,NULL,FALSE,NULL/*CREATE_DEFAULT_ERROR_MODE|CREATE_SUSPENDED*/,NULL, NULL, &StartupInfo, &ProcessInformation))
	{
		DebugPrintf(SV_LOG_ERROR,"Registry path %s, Export process failed...with Error: %d\n", regPath.c_str(), GetLastError());
		return false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Exported Registry file %s successfully.\n", regFile.c_str());
		::ResumeThread(ProcessInformation.hThread);
	}
	DWORD retValue = WaitForSingleObject( ProcessInformation.hProcess, INFINITE );
	if(retValue == WAIT_FAILED)
	{
		DebugPrintf(SV_LOG_INFO,"WaitForSingleObject has failed\n");
		rv = false;
	}
	::CloseHandle(ProcessInformation.hProcess);
	::CloseHandle(ProcessInformation.hThread);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

bool ChangeVmConfig::SetPrivileges(LPCSTR privilege, bool set)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	TOKEN_PRIVILEGES tp;
    HANDLE hToken;
    LUID luid;
    bool bhTokenOpen = false;

    do
    {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
	    {
            DebugPrintf(SV_LOG_ERROR, "OpenProcessToken failed - %lu\n", GetLastError());
		    rv = false; break;
	    }
        bhTokenOpen = true;

        if(!LookupPrivilegeValue(NULL, privilege, &luid))
        {            
		    DebugPrintf(SV_LOG_ERROR, "LookupPrivilegeValue failed - %lu\n", GetLastError());
            rv = false; break;
        }

        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        
        if (set)
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        else
            tp.Privileges[0].Attributes = 0;

        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
        if (GetLastError() != ERROR_SUCCESS)
        {            
		    DebugPrintf(SV_LOG_ERROR, "AdjustTokenPrivileges failed - %lu\n", GetLastError());
            rv = false; break;
        }
    }while(0);

    if(bhTokenOpen)
        CloseHandle(hToken);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//Check if File exists and return true
bool ChangeVmConfig::checkIfFileExists(string strFileName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool blnReturn = true;
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;
    hFind = FindFirstFile(strFileName.c_str(), &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE) 
    {
        DebugPrintf(SV_LOG_DEBUG,"Failed to find the file : %s\n",strFileName.c_str());
        DebugPrintf(SV_LOG_DEBUG,"Invalid File Handle.GetLastError reports [Error %lu ]\n",GetLastError());
        blnReturn = false;
    } 
    else 
    {
        DebugPrintf(SV_LOG_DEBUG,"Found the file : %s\n",strFileName.c_str());
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return blnReturn;
}

bool ChangeVmConfig::CheckDriverFilesExistance(list<string>& listDriverFiles)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bReturn = true;

	list<string>::iterator iter;

	for (iter = listDriverFiles.begin(); iter != listDriverFiles.end(); iter++)
	{
		if (!checkIfFileExists(*iter))
		{
			bReturn = false;
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bReturn;
}

bool ChangeVmConfig::GetControlSets(std::vector<std::string> & vecControlSets)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
    const unsigned int max_key_len = 255;
    char     achKey[max_key_len];   // buffer for subkey name    
    DWORD    cbName;                   // size of name string 
    TCHAR    achClass[MAX_PATH] = TEXT("");  // buffer for class name 
    DWORD    cchClassName = MAX_PATH;  // size of class string 
    DWORD    cSubKeys=0;               // number of subkeys 
    DWORD    cbMaxSubKey;              // longest subkey size 
    DWORD    cchMaxClass;              // longest class string 
    DWORD    cValues;              // number of values for key 
    DWORD    cchMaxValue;          // longest value name 
    DWORD    cbMaxValueData;       // longest value data 
    DWORD    cbSecurityDescriptor; // size of security descriptor 
    FILETIME ftLastWriteTime;      // last write time 
 
    DWORD i, retCode; 

    HKEY hKey;
    if( RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT(VM_SYSTEM_HIVE_NAME), 0, KEY_READ, &hKey) != ERROR_SUCCESS )
    {
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);;
        return false;
    }
 
    // Get the class name and the value count. 
    retCode = RegQueryInfoKey(  hKey,                    // key handle 
                                achClass,                // buffer for class name 
                                &cchClassName,           // size of class string 
                                NULL,                    // reserved 
                                &cSubKeys,               // number of subkeys 
                                &cbMaxSubKey,            // longest subkey size 
                                &cchMaxClass,            // longest class string 
                                &cValues,                // number of values for this key 
                                &cchMaxValue,            // longest value name 
                                &cbMaxValueData,         // longest value data 
                                &cbSecurityDescriptor,   // security descriptor 
                                &ftLastWriteTime);       // last write time 
 
    // Enumerate the subkeys, until RegEnumKeyEx fails.    
    if (cSubKeys)
    {       
        for (i=0; i<cSubKeys; i++) 
        { 
            cbName = max_key_len;
            retCode = RegEnumKeyEx(  hKey, i,
                                     achKey, 
                                     &cbName, 
                                     NULL, 
                                     NULL, 
                                     NULL, 
                                     &ftLastWriteTime); 
            if (retCode == ERROR_SUCCESS) 
            {
                DebugPrintf(SV_LOG_DEBUG,"(%d) %s\n", i+1, achKey);                
                std::string s = std::string(achKey);
                if( 0 == s.find(std::string(CONTROL_SET_PREFIX),0) )     
                {
                    DebugPrintf(SV_LOG_DEBUG,"Adding to vector - %s\n",s.c_str());
                    vecControlSets.push_back(s);                
                }
            }
        }
    } 
    else
    {
        DebugPrintf(SV_LOG_ERROR, "No subkeys are found for this VM. Hence no control sets are found.\n");
        DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n",__FUNCTION__);        
        closeRegKey(hKey);
        return false;
    }

    closeRegKey(hKey);
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

DWORD ChangeVmConfig::GetControlSets(std::string strSystemHive)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	m_sysHiveControlSets.clear();
	const unsigned int max_key_len = 255;
	char     achKey[max_key_len];   // buffer for subkey name    
	DWORD    cbName;                   // size of name string 
	TCHAR    achClass[MAX_PATH] = TEXT("");  // buffer for class name 
	DWORD    cchClassName = MAX_PATH;  // size of class string 
	DWORD    cSubKeys = 0;               // number of subkeys 
	DWORD    cbMaxSubKey;              // longest subkey size 
	DWORD    cchMaxClass;              // longest class string 
	DWORD    cValues;              // number of values for key 
	DWORD    cchMaxValue;          // longest value name 
	DWORD    cbMaxValueData;       // longest value data 
	DWORD    cbSecurityDescriptor; // size of security descriptor 
	FILETIME ftLastWriteTime;      // last write time 

	DWORD i, retCode;

	HKEY hKey;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, strSystemHive.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
	{
		retCode = GetLastError();
		DebugPrintf(SV_LOG_ERROR, "RegOpenKeyEx failed with error : %lu\n", retCode);
		DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
		return retCode;
	}

	// Get the class name and the value count. 
	retCode = RegQueryInfoKey(hKey,                    // key handle 
		achClass,                // buffer for class name 
		&cchClassName,           // size of class string 
		NULL,                    // reserved 
		&cSubKeys,               // number of subkeys 
		&cbMaxSubKey,            // longest subkey size 
		&cchMaxClass,            // longest class string 
		&cValues,                // number of values for this key 
		&cchMaxValue,            // longest value name 
		&cbMaxValueData,         // longest value data 
		&cbSecurityDescriptor,   // security descriptor 
		&ftLastWriteTime);       // last write time 

	if (retCode != ERROR_SUCCESS)
	{
		DebugPrintf(SV_LOG_ERROR, "RegQueryInfoKey failed with error : %lu\n", retCode);
		DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
		closeRegKey(hKey);
		return retCode;
	}

	// Enumerate the subkeys, until RegEnumKeyEx fails.    
	if (cSubKeys)
	{
		for (i = 0; i<cSubKeys; i++)
		{
			cbName = max_key_len;
			retCode = RegEnumKeyEx(hKey, i,
				achKey,
				&cbName,
				NULL,
				NULL,
				NULL,
				&ftLastWriteTime);
			if (retCode == ERROR_SUCCESS)
			{
				DebugPrintf(SV_LOG_DEBUG, "(%d) %s\n", i + 1, achKey);
				std::string s = std::string(achKey);
				/*if (0 == s.find(std::string(CONTROL_SET_PREFIX), 0))
				{
					DebugPrintf(SV_LOG_DEBUG, "Adding to vector - %s\n", s.c_str());
					m_sysHiveControlSets.push_back(s);
				}*/
				if (s.compare(0, 10, CONTROL_SET_PREFIX) == 0)
				{
					DebugPrintf(SV_LOG_DEBUG, "Adding to vector - %s\n", s.c_str());
					m_sysHiveControlSets.push_back(s);
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "RegEnumKeyEx failed with error : %lu\n", retCode);
				DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
				closeRegKey(hKey);
				return retCode;
			}
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "No subkeys are found for this VM. Hence no control sets are found.\n");
		DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
		closeRegKey(hKey);
		return ERROR_BAD_CONFIGURATION;
	}

	closeRegKey(hKey);
	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return ERROR_SUCCESS;
}

//*************************************************************************
//
//UpdateHostID()
//
//Purpose		: Updates HostID in DrScout.conf(VX) and Registry(FX) for Windows.
//					HKLM\Software\SV Systems ValueName: HostId
//					{code:Getdirvx}\Application Data\etc\drscout.conf; Section: vxagent; Key: HostId;
//					In case of linux we will handle this to linux service? Even on Linux machine,drScout.conf and Config.ini are to be upadeted.
//					On any OS it has to update for both VX and FX.
//
//Parameters	: string -> New UUID.
//
//Returns		: Returns TRUE on SUCCESS, FALSE on FAILURE.
//
//*************************************************************************
bool ChangeVmConfig::UpdateHostID(const string& strHostId)
{
	DebugPrintf( SV_LOG_INFO, "Entering %s\n", __FUNCTION__ );
	bool bRetVal = true;

	do
	{
		//Updating HostId for Vx Agent.
		LocalConfigurator objFileConfig;
		objFileConfig.setHostId(strHostId);

		//Updating HostId for FX
		LONG lRV;
		HKEY hKey;
		std::string strKeyName, strCurrentHostId;

		std::string strRegPathToOpen = std::string("SOFTWARE\\SV Systems");
		lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strRegPathToOpen.c_str(), 0, KEY_ALL_ACCESS, &hKey);
		if (lRV != ERROR_SUCCESS)
		{
			//May be a 64 bit system need to access path \\Wow6432Node\\SV Systems
			DebugPrintf( SV_LOG_INFO, "SV Systems could not be loacted under HKLM\\Software.Looks like a 64-bit system. Trying to access Wow6432Node.\n" );
			strRegPathToOpen = std::string("SOFTWARE\\Wow6432Node\\SV Systems");
			lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strRegPathToOpen.c_str(), 0, KEY_ALL_ACCESS, &hKey);
			if (lRV != ERROR_SUCCESS)
			{
				DebugPrintf( SV_LOG_ERROR, "Failed to access registry path [%s], return code -[%lu].\n", strRegPathToOpen.c_str(), lRV );
				DebugPrintf( SV_LOG_ERROR, "Failed to identify Unified Agent installation. Failed to read registry related to Unified Agent.\n" );
				DebugPrintf( SV_LOG_ERROR, "Failed to update HostID value in registry.\n");
				bRetVal = false;
				break;
			}	
		}

		strKeyName = "HostId";
		strCurrentHostId.clear();
		if (false == RegGetValueOfTypeSzAndMultiSz(hKey, strKeyName, strCurrentHostId))
		{
			DebugPrintf(SV_LOG_INFO, "Failed to fetch HostId using key [%s].\n", strKeyName.c_str());
			closeRegKey(hKey);
			bRetVal = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Current HostId is [%s].\n", strCurrentHostId.c_str());
			DebugPrintf(SV_LOG_INFO, "Updating HostId parameter with [%s].\n", strHostId.c_str());
			if (true != SetValueREG_SZ(hKey, strKeyName, strHostId))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to update HostId at key location [%s].\n", strKeyName.c_str());
				closeRegKey(hKey);
				bRetVal = false;
			}
		}
		closeRegKey(hKey);
	} while (0);

	DebugPrintf(SV_LOG_INFO, "Exiting %s\n", __FUNCTION__);
	return bRetVal;
}

//****************************************************************************
//
//UpdateServicesStartType()
//
//Purpose		: Sets/Updates service start type to value defined by user.
//					*Make this more generic in furture by receiving 
//					Hash Map with Key as Service Name and StartType as Value.
// 
//Parameters	: Vector containing Service Names and a START TYPE value for 
//					all
//
//Returns		: Returns true upon successful else false. Successful mean to 
//					say all services are updated.
//
//****************************************************************************
bool ChangeVmConfig::UpdateServicesStartupType( std::vector<std::string>& vecServicesToBeUpdated , DWORD dStartType )
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bRetVal = true;

	if (vecServicesToBeUpdated.empty())
	{
		DebugPrintf(SV_LOG_WARNING, "Received an empty list. No service would be updated/modified with a different StartType.\n");
		//bRetVal = false; Not considering this as an error. Only when this function failed to update a service then considering it as an error.
		return bRetVal;
	}

	do
	{
		if (m_sysHiveControlSets.empty())
		{
			DebugPrintf(SV_LOG_ERROR, "Registry control sets information is empty for system hive. Discover the control sets by using method \"GetControlSets\".\n");
			bRetVal = false;
			break;
		}
		std::vector<std::string>::iterator iterCS = m_sysHiveControlSets.begin();
		for (; iterCS != m_sysHiveControlSets.end(); iterCS++)
		{
			std::vector<std::string>::iterator iter = vecServicesToBeUpdated.begin();
			for (; iter != vecServicesToBeUpdated.end(); ++iter)
			{
				DebugPrintf(SV_LOG_INFO, "Service name received for updating start-type : %s\n", (*iter).c_str());
				if (false == ChangeServiceStartType( std::string("SYSTEM"), (*iter).c_str(), dStartType, *iterCS ))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to change the start type for service : [%s].\n", (*iter).c_str());
					bRetVal = false;
				}
			}
		}
	} while (0);

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bRetVal;
}

//****************************************************************************
//
//RestartServices()
//
//Purpose		: Restart list of services sent. Attempts to restart all services
//					specified even though there is a failure to restart 
//					one of the service.
//
//Parameters	: vector containing Service Names which have to be restarted.
//
//Retruns		: Returns true upon successful else false. Successful mean to 
//					say all the services restarted without failure.
//
//****************************************************************************
bool ChangeVmConfig::RestartServices(std::vector<std::string>& vecServicesToBeRestarted)
{
	DebugPrintf( SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__ );
	bool bRetVal = true;

	if (vecServicesToBeRestarted.empty())
	{
		DebugPrintf( SV_LOG_WARNING, "Received an empty list. No service would be restarted.\n" );
		//bRetVal = false; Not considering this as an error. Only when this function failed to restart a service then considering it as an error.
		return bRetVal;
	}

	std::vector<std::string>::iterator iter = vecServicesToBeRestarted.begin();
	for (;iter != vecServicesToBeRestarted.end(); ++iter )
	{
		DebugPrintf( SV_LOG_INFO, "Service name received to restart : %s\n", (*iter).c_str() );
		if (S_FALSE == restartService( *iter ))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to re-start service : %s.\n", (*iter).c_str());
			bRetVal = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Successfully re-started service : %s.\n", (*iter).c_str());
		}
	}

	DebugPrintf( SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__ );
	return bRetVal;
}

//****************************************************************************
//TFS ID 1269459.
//ResetGUID()
//
//Parameters	: Reset's host GUID. If it failed to generate a NEW GUID, 
//					the services are going to be in manual state.
//
//Input			: NONE.
//
//Output		: Returns TRUE upon success else FALSE.
//
//*****************************************************************************
bool ChangeVmConfig::ResetGUID()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bRetVal = true;

	std::string strNewHostId;
	try{
		strNewHostId = m_newHostId.empty() && m_strCloudEnv.empty() ? GetUuid() : m_newHostId;
	}
	catch (ContextualException &e)
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to generate Host UUID with exception: %s\n", e.what());
		bRetVal = false;
	}

	if (!strNewHostId.empty())
	{
		DebugPrintf(SV_LOG_INFO, "New HostId = %s.\n", strNewHostId.c_str());
		if (false == UpdateHostID(strNewHostId))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to update host ID.\n");
			bRetVal = false;
		}
		else
		{
			//delete checksum of MBR file.
			DLM_ERROR_CODE RetVal = DeleteDlmRegitryEntries();
			if (DLM_ERR_SUCCESS != RetVal)
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to cleanup the DLM related registry entries : %u\n", RetVal);
				bRetVal = false;
			}
			else
				DebugPrintf(SV_LOG_INFO, "Successfully cleaned up the DLM related registry entries\n");

			if (IsBackwardCompatibleVcon())
			{
				std::vector<std::string> vecServices;
				vecServices.push_back("svagents"); //VX agent service Name.
				vecServices.push_back("frsvc"); // fx Agent service Name.
				vecServices.push_back("InMage Scout Application Service");//Appllication agent Service Name.

				//FIX-ME: Instead of Passing vector as argument if we could pass a Hash Map with service as Key and Startup type as value it would make the function generic.
				//TFS ID : 1547582.Set Services back to Automatic. Passing Service Startup type as 2 which indicates AUTO START.//SERVICE_AUTO_START//0x00000002
				if (false == UpdateServicesStartupType(vecServices, 2))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to update startup type for some or all services.\n");
					bRetVal = false;
				}

				if (false == RestartServices(vecServices))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to restart some of the services or all.\n");
					bRetVal = false;
				}
			}
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "New HostId should not be empty\n");
		bRetVal = false;
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bRetVal;
}


//*************************************************************************************
// Resets the replication state on recovered VM by making following changes:
//         1. Reset the filter driver state
//         2. Clear the bitmap files if exist
//         3. Clear the cache settings
//
// Note: Stop VX-Agent before calling this function, because vx-agent holds the lock on
//       config.dat file
//*************************************************************************************
bool ChangeVmConfig::ResetReplicationState()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool bRetVal = true;

	try
	{
		LocalConfigurator lConfig;

		// Call drvutil to stop filtering & delete bitmaps
		std::string strDrvUtilCmd = lConfig.getInstallPath();
		BOOST_ASSERT(!strDrvUtilCmd.empty());
		boost::trim(strDrvUtilCmd);
		if (!boost::ends_with(strDrvUtilCmd, ACE_DIRECTORY_SEPARATOR_STR_A))
			strDrvUtilCmd += ACE_DIRECTORY_SEPARATOR_STR_A;

		strDrvUtilCmd += "drvutil.exe --stopf -stopall -deletebitmap";
		
		DebugPrintf(SV_LOG_INFO, "Running the command: %s\n", strDrvUtilCmd.c_str());

		int exitCode = 0;
		std::string strCmdOut, strCmdError;
		bRetVal = RunInmCommand(strDrvUtilCmd, strCmdOut, strCmdError, exitCode);
		if (!bRetVal)
		{
			DebugPrintf(SV_LOG_ERROR, "Command execution error.\n");
			DebugPrintf(SV_LOG_ERROR, "Exit code: %d\n", exitCode);
			DebugPrintf(SV_LOG_ERROR, "-----stderror----\n%s\n", strCmdError.c_str());
		}		
		DebugPrintf(SV_LOG_INFO, "-----stdout----\n%s\n", strCmdOut.c_str());
		
		// Clear chache settigns files.
		std::list<std::string> lstFilesToRemove;
		
		std::string configFilesPath;
		if (LocalConfigurator::getConfigDirname(configFilesPath))
		{
			BOOST_ASSERT(!configFilesPath.empty());
			boost::trim(configFilesPath);
			if (!boost::ends_with(configFilesPath, ACE_DIRECTORY_SEPARATOR_STR_A))
				configFilesPath += ACE_DIRECTORY_SEPARATOR_STR_A;

			lstFilesToRemove.push_back(configFilesPath + "config.dat");
			lstFilesToRemove.push_back(configFilesPath + "config.dat.lck");
			lstFilesToRemove.push_back(configFilesPath + "AppAgentCache.dat");
			lstFilesToRemove.push_back(configFilesPath + "AppAgentCache.dat.lck");
			lstFilesToRemove.push_back(configFilesPath + "SchedulerCache.dat");
			lstFilesToRemove.push_back(configFilesPath + "SchedulerCache.dat.lck");
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Could not get the config directory path. Cache files cleanup won't happen.\n");
			bRetVal = false;
		}

		std::list<std::string>::const_iterator iterFile = lstFilesToRemove.begin();
		for (; iterFile != lstFilesToRemove.end(); iterFile++)
		{
			boost::filesystem::path cache_file(*iterFile);
			if (boost::filesystem::exists(cache_file))
			{
				DebugPrintf(SV_LOG_INFO, "Removing the file %s\n", iterFile->c_str());
				boost::filesystem::remove(cache_file);
			}
			else
			{
				DebugPrintf(SV_LOG_INFO, "File %s does not exist.\n", iterFile->c_str());
			}
		}

	}
	catch (const std::exception& exp)
	{
		bRetVal = false;
		DebugPrintf(SV_LOG_ERROR, "Error:  exception while resetting replication settings state.\n");
		DebugPrintf(SV_LOG_ERROR, "Exception: %s\n", exp.what());
	}
	catch (...)
	{
		bRetVal = false;
		DebugPrintf(SV_LOG_ERROR, "Error: Unkown exception while resetting replication settings state.\n");
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return bRetVal;
}

//***************************************************************************************
// Configure the Settings when the VM is bootinup. 
// Pre Step - First remove the entry from registry so that on next boot this will not run.
//  1. Network Settings
//	2. Installs VM Ware Tools in Case of P2V Recovery.
//  3. Bring the disks online and reboot if needed
//***************************************************************************************
bool ChangeVmConfig::ConfigureSettingsAtBooting(std::string NwInfoFile, bool bP2v, std::string strCloudEnv , bool bResetGUID )
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
    bool rv = true;
	m_p2v = bP2v;
	m_strCloudEnv = strCloudEnv;

    do
    {
        bool bW2K3 = true;
        if(true == IsItW2k8Machine())
            bW2K3 = false;

        bool bRegEntryDeleted = true;
        bool bNwFileMoved = false;

		DWORD retCode = GetControlSets(RUNNING_VM_SYSTEM_HIVE);
		if (ERROR_SUCCESS != retCode)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to get Control Sets. Error code: [%lu] and error message: [%s].\n", retCode, ConvertGetLastErrorToMessage(retCode).c_str());
			rv = false;
			break;
		}

		DWORD dwStartupScriptOrder = 0;
		// Remove disk online and driveletter assignment for V2A failover & failback scenarios
		// as the driver will take care of them.
		if (IsBackwardCompatibleVcon())
		{
			//Remove the reg entry so that on next boot this exe will not run.
			if (false == RegDelStartupExePathEntries(bW2K3, dwStartupScriptOrder))
			{
				DebugPrintf(SV_LOG_ERROR, "RegDelStartupExePathEntries failed.\n");
				DebugPrintf(SV_LOG_ERROR, "Manually remove the registry entry for starting an executable on next boot.\n");
				bRegEntryDeleted = false;
			}


			rescanIoBuses();
			//Online the Disks if any offline disks found
			if (false == OnlineAllOfflineDisks())
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to online some of the offline disks\n");
				DebugPrintf(SV_LOG_ERROR, "Manually online the offline disks and reboot the machine to function the applications properly\n");
			}

			Sleep(30000);
			rescanIoBuses();
			//Import the disks if any foreign disks found
			if (false == ImportForeignDisks())
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to import the dynamic disks\n");
				DebugPrintf(SV_LOG_ERROR, "Manually import the dynamic disks and reboot the machine to function the applications properly\n");
				rv = false;
			}

			DebugPrintf(SV_LOG_INFO, "Sleeping for 60 secs for the volumes to come up properly in this time...\n");
			Sleep(60000);  //sleeping for 60 secs for the volumes to come up properly.

			//Assign proper drive letters as of source and reboot if required (added for drive swap issue)
			//TFS ID : 1051609
			if (false == AssignDriveLetters(NwInfoFile))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to assign proper drive letters to the volumes of recovered VM\n");
				DebugPrintf(SV_LOG_ERROR, "Manually assign the drive letters and reboot the machine to function the applications properly\n");
				rv = false;
			}
		}

		if (m_bEnableRdpFireWall &&
			boost::iequals(strCloudEnv, "azure")) // Want to enable RDP only on failover to azure
		{
			DebugPrintf(SV_LOG_INFO, "Enabling RDP Public Firewall Rule\n");
			//Enabling public profile for "Remote Desktop" rule group
			if (!EnabledRdpPublicFirewallRule())
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to update firewall rules for public profile on recovered VM\n");
				rv = false;
			}
			else
			{   // Reset RDP flag in registry.
				try
				{
					DebugPrintf(SV_LOG_INFO, "Reset RDP flag in registry\n");

					ResetEnableRdpFlagInReg();
				}
				catch (const std::exception& exp)
				{
					DebugPrintf(SV_LOG_ERROR, "Error resetting EnableRDP flag in registry. Exception: %s\n", exp.what());
					rv = false;
				}
				catch (...)
				{
					DebugPrintf(SV_LOG_ERROR, "Error resetting EnableRDP flag in registry. Unkown exception.\n");
					rv = false;
				}
			}
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Skipped enabling firewall rules for RDP public profile on recovered VM\n");
		}

		if (strCloudEnv.empty() || (0 == strcmpi(strCloudEnv.c_str(), "vmware")))
		{
			if (m_p2v)
			{
				// Removing Hyper-V vssadmin providers from all control sets of system hive. These are added automatically when the machine moved to Azure.
				// Currently ignoring the failures, so that other boot up tasks won't get effected.
				string strRegPathToDelete = "Services\\VSS\\Providers\\{74600e39-7dc5-4567-a03b-f091d6c7b092}";
				if (RemoveRegistryPathFromSystemHiveControlSets(RUNNING_VM_SYSTEM_HIVE, strRegPathToDelete))
				{
					// Restarting the VSS service, so that new providers will get populated after the registry cleanup.
					string strVssService = "VSS";
					DebugPrintf(SV_LOG_INFO, "Successfully removed the Hyper-V IC Software Shadow Copy Provider registry keys. Restarting service : %s.\n", strVssService.c_str());

					if (S_FALSE == restartService(strVssService))
					{
						DebugPrintf(SV_LOG_ERROR, "Failed to re-start service : %s.\n", strVssService.c_str());
					}
					else
					{
						DebugPrintf(SV_LOG_INFO, "Successfully restarted the service : %s.\n", strVssService.c_str());
					}
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to remove Hyper-V IC Software Shadow Copy Provider registry keys.\n");
				}

				//Install the VmWare tools in case of P2V only
				if (false == InstallVmWareTools())
				{
					DebugPrintf(SV_LOG_ERROR, "installVmWareTools failed.\n");
					DebugPrintf(SV_LOG_ERROR, "Manually install the vmware tools in the recovered VM.\n");
					rv = false;
				}
			}
		}

		if (!NwInfoFile.empty() && strCloudEnv.empty())
		{

			//Updates the new network configuration
			if (false == ConfigureNWSettings(NwInfoFile))
			{
				DebugPrintf(SV_LOG_ERROR, "ConfigureNWSettings failed.\n");
				DebugPrintf(SV_LOG_ERROR, "Manually configure the network settings.\n");
				rv = false;
			}

			// Apart from removing registry entry, move the nw file so that the startup exe 
			// will fail on next boot in case where reg entry is not removed and 
			// prevent the infinte reboots as machine is rebooted after bringing disks online
			std::string NewPathForNwInfoFile = NwInfoFile + std::string(".old");
			if (0 == MoveFile(NwInfoFile.c_str(), NewPathForNwInfoFile.c_str()))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to move file from %s to %s with error - %lu\n", NwInfoFile.c_str(), NewPathForNwInfoFile.c_str(), GetLastError());
			}
			else
			{
				bNwFileMoved = true;
			}
		}

		// Remove disk online and driveletter assignment for V2A failover & failback scenarios
		// as the driver will take care of them.
		if (IsBackwardCompatibleVcon())
		{
			DebugPrintf(SV_LOG_INFO, "Retrying drive letter assignment again for the recovered VM.\n");
			//Assign proper drive letters as of source and reboot if required (added for drive swap issue)
			if (false == AssignDriveLetters(NwInfoFile))
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to assign proper drive letters to the volumes of recovered VM\n");
				DebugPrintf(SV_LOG_ERROR, "Manually assign the drive letters and reboot the machine to function the applications properly\n");
			}
		}

		// Stop filtering, clear bitmap files & remove config settings file.
		// Note: Not stoping vx-agent here, because at this stage the vx-agent
		//       start-type is manual & it will be in stopped state. Or it will
		//       be in halt state for recovery to complete.
		if (!ResetReplicationState())
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to reset the replication state on recovered vm.\n");
			rv = false;
		}

		if (bResetGUID)
		{
			if (false == ResetGUID())
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to reset host GUID.\n");
				rv = false;
			}
			else
			{
				DebugPrintf(SV_LOG_INFO, "New HostId is updated successfully\n");
				// Set failover complete.
				try 
				{
					SetRecoveryCompleted();
				} catch (const std::exception& exp)	
				{
					DebugPrintf(SV_LOG_ERROR, "Error updating recovery complete. Exception: %s\n", exp.what());
					rv = false;
				}
				catch (...)
				{
					DebugPrintf(SV_LOG_ERROR, "Error updating recovery complete. Unkown exception.\n");
					rv = false;
				}				
			}
		}

		//reboots if required
		if(m_bReboot && (bRegEntryDeleted == true || bNwFileMoved == true))
        {
			DebugPrintf(SV_LOG_INFO,"Triggering Reboot, As Some of storage changes has been by the boot up script...\n");
			DebugPrintf(SV_LOG_INFO,"Command To Reboot: \"shutdown /r /f /t 5\"\n");
            system("shutdown /r /f /t 5"); // Reboot Machine.
        }

    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

LONG ChangeVmConfig::RegGetSVSystemDWORDValue(const std::string& valueName,
	DWORD& valueData,
	const std::string& hiveRoot,
	const std::string& relativeKeyPath)
{
	TRACE_FUNC_BEGIN;
	LONG lRetStatus = ERROR_SUCCESS;

	std::stringstream errorStream;

	do
	{
		CRegKey svSystemsKey;

		std::string svSystemsPath = hiveRoot
			+ std::string(REG_KEY_SV_SYSTEMS_X32)
			+ (relativeKeyPath.empty() ? "" : relativeKeyPath);

		lRetStatus = svSystemsKey.Open(HKEY_LOCAL_MACHINE, svSystemsPath.c_str());
		if (ERROR_FILE_NOT_FOUND == lRetStatus)
		{
			svSystemsPath = hiveRoot
				+ std::string(REG_KEY_SV_SYSTEMS_X64)
				+ (relativeKeyPath.empty() ? "" : relativeKeyPath);

			lRetStatus = svSystemsKey.Open(HKEY_LOCAL_MACHINE, svSystemsPath.c_str());
		}

		if (ERROR_SUCCESS != lRetStatus)
		{
			errorStream << "Could not open " << svSystemsPath
				<< ". Error " << lRetStatus;

			break;
		}

		lRetStatus = svSystemsKey.QueryDWORDValue(valueName.c_str(), valueData);
		if (ERROR_SUCCESS != lRetStatus)
		{
			errorStream << "Could not get "
				<< valueName
				<< " value under the key " << svSystemsPath
				<< ". Error " << lRetStatus;
		}

	} while (false);

	if (ERROR_SUCCESS != lRetStatus)
	{
		TRACE_ERROR("%s\n", errorStream.str().c_str());
	}

	TRACE_FUNC_END;
	return lRetStatus;
}


//***************************************************************************************
// Delete the registry entries added in RegAddStartupExeOnBoot.
//***************************************************************************************
bool ChangeVmConfig::RegDelStartupExePathEntries(bool bW2K3, DWORD dwStartupScriptOrder)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
    bool rv = true;

    do
    {
        std::string RegPath1, RegPath2;

        if(bW2K3)
        {
            RegPath1 = std::string("SOFTWARE\\Policies\\Microsoft\\Windows\\System\\Scripts\\Startup");
        }
        else 
        {
            RegPath1 = std::string("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Group Policy\\Scripts\\Startup");
        }
        
        RegPath2 = std::string("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Group Policy\\State\\Machine\\Scripts\\Startup");

		if (false == RegDelRegEntriesForStartupExe(RegPath1, dwStartupScriptOrder))
        {
            rv = false; break;
        }

		if (false == RegDelRegEntriesForStartupExe(RegPath2, dwStartupScriptOrder))
        {
            rv = false; true;
        }
    }while(0);

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//***************************************************************************************
// Delete the registry entry for given path
// First delete subkey by adding \\0 and then remove the given path 
// Ex: HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Microsoft\Windows\System\Scripts\Startup\0
//      First remove it subkey i.e HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Microsoft\Windows\System\Scripts\Startup\0\0
//      Then remove the given one as RegDeleteKey will not delete a key if it has subkey.
//***************************************************************************************
bool ChangeVmConfig::RegDelRegEntriesForStartupExe(std::string RegPath, DWORD scriptOrderNumber)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool rv = true;

	do
	{
		std::stringstream startupKey, startupSubKey; 
		startupKey << RegPath << "\\" << scriptOrderNumber;
		startupSubKey << RegPath << "\\" << scriptOrderNumber << "\\0";

		if (false == RegDelGivenKey(startupSubKey.str()))
		{
			rv = false; break;
		}

		if (false == RegDelGivenKey(startupKey.str()))
		{
			rv = false; break;
		}
	} while (0);

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return rv;
}

//********************************************************
// Delete the given Value under a key. 
//********************************************************
bool ChangeVmConfig::RegDelGivenValue(string RegPath, string strRegValue)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
	DebugPrintf(SV_LOG_INFO, "Deleting registry path [%s].\n", RegPath.c_str());
	bool rv = true;

    do
    {
        HKEY hKey;
		long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPath.c_str(), 0, KEY_ALL_ACCESS, &hKey);
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed and return code - [%lu].\n", lRV);
            rv = false; break;
        }

		lRV = RegDeleteValue(hKey,strRegValue.c_str());
        if( lRV != ERROR_SUCCESS )
        {
			DebugPrintf(SV_LOG_ERROR,"RegDeleteValues failed for path - %s and for value - %s, return code - [%lu].\n", RegPath.c_str(), strRegValue.c_str(), lRV);
            rv = false;
        }
        closeRegKey(hKey);
    }while(0);
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


//***************************************************************************************
// Delete the given key.. This will delete the keys only if it does not have any sub keys
//***************************************************************************************
bool ChangeVmConfig::RegDelGivenKey(std::string RegPath)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
	DebugPrintf(SV_LOG_INFO, "Deleting registry path [%s].\n", RegPath.c_str());
	bool rv = true;

    do
    {
        HKEY hKey;
        long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, NULL, 0, KEY_ALL_ACCESS, &hKey);
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed and return code - [%lu].\n", lRV);
            rv = false; break;
        }

        lRV = RegDeleteKey(hKey, RegPath.c_str());
        if( lRV != ERROR_SUCCESS )
        {
            DebugPrintf(SV_LOG_ERROR,"RegDeleteKey failed for path - %s and return code - [%lu].\n", RegPath.c_str(), lRV);
            closeRegKey(hKey);
            rv = false; break;
        }
        closeRegKey(hKey);
    }while(0);
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//***************************************************************************************
// Configure the NW Settings. 
// Read the NW info from file and set the IPs.
//***************************************************************************************
bool ChangeVmConfig::ConfigureNWSettings(std::string NwInfoFile)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
    bool rv = true;

    do
    {
		string sysDrive;
		/*if(false == GetSystemVolume(sysDrive))
		{
			sysDrive = "c:";
		}*/

		char strVol[MAX_PATH]={0};
		if(GetWindowsDirectory((LPTSTR)strVol,MAX_PATH))
		{
			sysDrive = string(strVol);
		}
		string strFileName = sysDrive + NETWORK_LOGFILE_PATH;
		if(checkIfFileExists(strFileName))
		{
			if(-1 == remove(strFileName.c_str()))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to delete the file - %s\n",strFileName.c_str());
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"SuccessFully deleted stale file - %s\n",strFileName.c_str());			
			}
		}

		strFileName.clear();
		strFileName = sysDrive + NETWORK_FILE_PATH + string(".old");
		if(checkIfFileExists(strFileName))
		{
			if(-1 == remove(strFileName.c_str()))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to delete the file - %s\n",strFileName.c_str());
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"SuccessFully deleted stale file - %s\n",strFileName.c_str());			
			}
		}

        NetworkInfoMap_t NwMap;
		int nNumNicsToUpdate = 0;

        if(false == ReadNWInfoMapFromFile(NwMap, NwInfoFile, nNumNicsToUpdate))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to read the Network Info from file - %s\n", NwInfoFile.c_str());
            DebugPrintf(SV_LOG_ERROR,"Please configure the IP address manually.\n");
            rv = false; break;
        }

        if(false == SetNwUsingNetsh(NwMap, nNumNicsToUpdate))
        {
            DebugPrintf(SV_LOG_ERROR,"SetNwUsingNetsh() failed.\n");
            DebugPrintf(SV_LOG_ERROR,"Please configure the IP address manually.\n");
            rv = false; break;
        }

    }while(0);        

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//***************************************************************************************
// Read the nw file info and make the map
// Input File Format (6 elements) (Separator - !@!@!)
//  InterfaceCount!@!@!EnableDHCP!@!@!IPAddress!@!@!SubnetMask!@!@!DefaultGateway!@!@!NameServer
//***************************************************************************************
bool ChangeVmConfig::ReadNWInfoMapFromFile(NetworkInfoMap_t & NwMap, std::string FileName, int& nNumNics)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
    bool rv = true;

	DebugPrintf(SV_LOG_INFO,"Network Information File %s.\n",FileName.c_str());	
    ifstream FIN(FileName.c_str(), std::ios::in);
    if(! FIN.is_open())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s\n",FileName.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    std::string strLineRead;
	//Faced issue when more NICs are available, bianry is failed to discover all the NICS as OS is taking time to detect those NICS.
	//So Per NIC waiting for 15 seconds for disocvery by OS. So toatl time for waiting becomes multiple of no.of NIC entries present in the nw.txt file 
    while(getline(FIN, strLineRead))
    {
		nNumNics++;
        if(strLineRead.empty())
            continue;

        DebugPrintf(SV_LOG_INFO,"%s\n",strLineRead.c_str());

        std::vector<std::string> v;
        std::string Separator = std::string("!@!@!");
        size_t pos = 0;

        size_t found = strLineRead.find(Separator, pos);
        while(found != std::string::npos)
        {
            DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",strLineRead.substr(pos,found-pos).c_str());
            v.push_back(strLineRead.substr(pos,found-pos));
            pos = found + 5; 
            found = strLineRead.find(Separator, pos);            
        }

        if(v.size() < 6)
        {
            DebugPrintf(SV_LOG_INFO,"Number of elements in this line - %u\n",v.size());
            rv = false; break;
        }

        NetworkInfo n;

        std::stringstream ss;
        ss<<v[1];
        ss>>n.EnableDHCP;

        n.IPAddress  = v[2];
        n.SubnetMask = v[3];
        n.DefaultGateway = v[4];
        n.NameServer = v[5];
		n.MacAddress = v[6];

        NwMap.insert(make_pair(v[0],n));
    }

    FIN.close();
	
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//***************************************************************************************
//  Set the IP Address using NwMap
//***************************************************************************************
bool ChangeVmConfig::SetNwUsingNetsh(NetworkInfoMap_t NwMap, int nNumNicsToUpdate)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	
    bool rv = true;

    // Get InterfaceNames
	//InterfacesVec_t InterfaceNames;
	//InterfacesVecIter_t iter;
	int nTime = 0;
	map<InterfaceName_t, unsigned int> mapOfInterfaceStatus;
	Sleep(30000);

	std::string strResultFile = m_VxInstallPath + string("\\Failover\\data\\") + std::string(NETWORK_LOGFILE_PATH);
	DebugPrintf(SV_LOG_INFO, "Netwrok Log File : %s\n", strResultFile.c_str());

	do
	{
		if(S_FALSE == InitCOM())
		{
			DebugPrintf(SV_LOG_ERROR,"InitCOM Failed\n");
		}
		else
		{
			if(S_FALSE == WMINwInterfaceQueryWithMAC( NwMap , mapOfInterfaceStatus ))
			{
				DebugPrintf(SV_LOG_ERROR,"WMINetworkQuery Failed\n");
				mapOfInterfaceStatus.clear();
			}
			else
			{
				if(!mapOfInterfaceStatus.empty())
				{
					DebugPrintf( SV_LOG_ALWAYS , "Number of NIC's in nw.txt = %d , Number of NIC's discovered = %d.\n", nNumNicsToUpdate , mapOfInterfaceStatus.size() );
					if(nNumNicsToUpdate == mapOfInterfaceStatus.size())
					{
						DebugPrintf(SV_LOG_INFO,"All the Network Adapters got discovered.\n");
						break;
					}
				}			
			}
		}
		DebugPrintf(SV_LOG_INFO,"All the Network Adapters are not discovered. Sleeping for 15secs, Will try again.\n");
		nTime = nTime + 15;
		if(nTime > 1800)
		{
			DebugPrintf(SV_LOG_INFO,"Maximum time(30 mins) reached to Discover all the network adapters, coming out of the loop\n");
			break;
		}
		Sleep(15000);
	}while(1);

	map<InterfaceName_t, unsigned int>::iterator iterMap = mapOfInterfaceStatus.begin();
	for(;iterMap != mapOfInterfaceStatus.end(); iterMap++)
	{
		DebugPrintf(SV_LOG_INFO,"Discovered Network Adapter : %s\n",iterMap->first.c_str());
		if((iterMap->second == 0) || (iterMap->second == 4) || (iterMap->second == 5))
		{
			if (false == EnableConnection(iterMap->first, true, strResultFile))
			{
				DebugPrintf(SV_LOG_INFO,"Failed to Enable the network connection for adapter : %s\n",iterMap->first.c_str());
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Successfully Enabled the network connection for adapter : %s\n",iterMap->first.c_str());
			}
		}
	}

	list<string> listUpdatedNic;
	NetworkInfoMap_t UnUpdatedNw;

	for(NetworkInfoMapIter_t iter = NwMap.begin(); iter != NwMap.end(); iter++)
    {
		string strMacAddress = iter->second.MacAddress;
		if(strMacAddress.empty())
		{
			DebugPrintf(SV_LOG_INFO,"Null NIC MAC address found from file\n");
			UnUpdatedNw.insert(make_pair(iter->first, iter->second));
			continue;
		}

		DebugPrintf(SV_LOG_INFO,"Going to update the network details for NIC MAC : %s\n", strMacAddress.c_str());

		string strNetworkName;
		
		for(int i=0; i < 5 ; i++)
		{
			if(S_FALSE == InitCOM())
			{
				DebugPrintf(SV_LOG_ERROR,"InitCOM Failed\n");
				CoUninitialize();
				continue;
			}
			if(S_FALSE == WMINicQueryForMac(strMacAddress, strNetworkName))
			{
				DebugPrintf(SV_LOG_ERROR,"WMINetworkQuery Failed\n");
				strNetworkName.clear();
			}
			else
			{
				if(!strNetworkName.empty())
                    break;
			}
			if(i != 5)
            {
                DebugPrintf(SV_LOG_ERROR,"WMI query did not return NICs present in the system for Mac Address %s\n\n", strMacAddress.c_str());
                Sleep(15000);
            }
            else
            {
				DebugPrintf(SV_LOG_ERROR,"\nWMI query did not return NICs present in the system in all attempts for Mac address %s\n", strMacAddress.c_str());
            }
		}
		if(!strNetworkName.empty())
		{
			DebugPrintf(SV_LOG_INFO,"Network interface %s found for Mac address: %s\n", strNetworkName.c_str(), strMacAddress.c_str());
			if(false == UpdateNetworkInfo(strNetworkName, iter->second, strResultFile))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to update for Network interface %s having Mac address: %s\n", strNetworkName.c_str(), strMacAddress.c_str());
				rv = false;
			}
			else
			{
				listUpdatedNic.push_back(strNetworkName);
			}
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Empty Network interface found for Mac address: %s\n", strMacAddress.c_str());
			UnUpdatedNw.insert(make_pair(iter->first, iter->second));
		}
	}

	if(UnUpdatedNw.empty())
	{
		DebugPrintf(SV_LOG_INFO,"All the Network interfaces got updated properly\n");
	}
	else
	{
		////Old method of network updation, serially updates for each discovered nics.Discarding this.
		
		//list<string>::iterator iterList;
		//map<InterfaceName_t, unsigned int>::iterator iter;
		//iterList = listUpdatedNic.begin();
		//for(;iterList != listUpdatedNic.end(); iterList++)
		//{
		//	iter = mapOfInterfaceStatus.find(*iterList);
		//	if(iter != mapOfInterfaceStatus.end())
		//	{
		//		DebugPrintf(SV_LOG_INFO,"Network interfaces %s got updated, Erasing from map.\n", iter->first.c_str());
		//		mapOfInterfaceStatus.erase(iter);
		//	}
		//}

		//iter = mapOfInterfaceStatus.begin();
		////Old method of network updation, serially updates for each discovered nics.
		//for(NetworkInfoMapIter_t i = UnUpdatedNw.begin(); 
		//(i != UnUpdatedNw.end() && iter != mapOfInterfaceStatus.end());
		//i++, iter++)
		//{
		//	DebugPrintf(SV_LOG_INFO,"Going to update Network interface for NIC : %s \n", iter->first.c_str());
		//	if(false == UpdateNetworkInfo(iter->first, i->second, strResultFile))
		//	{
		//		rv = false;
		//	}
		//}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

bool ChangeVmConfig::UpdateNetworkInfo(string strNetworkName, NetworkInfo nwInfo, string strResultFile)
{
	bool rv = true;

	std::string NetshCmdForIP = std::string("netsh interface ip set address");
	std::string NetshCmdForDns;
	NetshCmdForIP = NetshCmdForIP + std::string(" ") + std::string("\"") + strNetworkName + std::string("\"");	

	if(1 == nwInfo.EnableDHCP)
	{
		NetshCmdForIP = NetshCmdForIP + std::string(" ") + std::string("dhcp");

		NetshCmdForDns = std::string("netsh interface ip set dns");
		NetshCmdForDns = NetshCmdForDns + std::string(" ") + std::string("\"") + strNetworkName + std::string("\"");
		NetshCmdForDns = NetshCmdForDns + std::string(" ") + std::string("dhcp");

		if( false == ExecuteNetshCmd(NetshCmdForIP, strResultFile))
		{
			rv = false;
		}

		ExecuteNetshCmd(NetshCmdForDns, strResultFile);
	}
	else
	{
		if(nwInfo.IPAddress.empty())
		{
			DebugPrintf(SV_LOG_INFO,"New IP details are not provided for the Interface : %s \n", strNetworkName.c_str());
			DebugPrintf(SV_LOG_INFO,"Skipped Network updation for this Interface : %s\n", strNetworkName.c_str());
			return rv;
		}
		std::map<std::string, std::string> mapIpAndNetmask;

		if(false == CreateMapOfIPandSubNet(nwInfo.IPAddress, nwInfo.SubnetMask, mapIpAndNetmask))
		{
			DebugPrintf(SV_LOG_INFO,"Failed to get Map of IP Address and SubNet Mask - %s\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}


		std::map<std::string, std::string>::iterator iter = mapIpAndNetmask.begin();
		
		for(;iter !=  mapIpAndNetmask.end(); iter++)
		{
			if(iter == mapIpAndNetmask.begin())
			{
				NetshCmdForIP = std::string("netsh interface ip set address");
				NetshCmdForIP = NetshCmdForIP + std::string(" ") + std::string("\"") + strNetworkName + std::string("\"");
				NetshCmdForIP = NetshCmdForIP + std::string(" ") + std::string("static"); 
			}
			else
			{
				NetshCmdForIP = std::string("netsh interface ip add address");
				NetshCmdForIP = NetshCmdForIP + std::string(" ") + std::string("\"") + strNetworkName + std::string("\"");
			}			

			NetshCmdForIP = NetshCmdForIP + std::string(" ") + iter->first;
			NetshCmdForIP = NetshCmdForIP + std::string(" ") + iter->second;

			if(false == ExecuteNetshCmd(NetshCmdForIP, strResultFile))
			{
				rv = false;
			}
		}
		std::string Separator = std::string(",");
		size_t pos = 0;
		if(!nwInfo.DefaultGateway.empty())
		{
			string NetshCmdForGateWay = std::string("netsh interface ip add address");
			NetshCmdForGateWay = NetshCmdForGateWay + std::string(" ") + std::string("\"") + strNetworkName + std::string("\"");
			set<string> listGateways;

			size_t found = nwInfo.DefaultGateway.find(Separator, pos);
			if(found == std::string::npos)
			{
				DebugPrintf(SV_LOG_DEBUG,"Only single Default Gateway found for this Nic.\n");
				if(nwInfo.DefaultGateway.compare("notselected") != 0)
				{
					listGateways.insert(nwInfo.DefaultGateway);
				}
			}
			else
			{
				string notGateWay;
				while(found != std::string::npos)
				{
					DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",nwInfo.DefaultGateway.substr(pos,found-pos).c_str());
					notGateWay = nwInfo.DefaultGateway.substr(pos,found-pos);
					if(notGateWay.compare("notselected") != 0)
					{
						listGateways.insert(notGateWay);
					}
					pos = found + 1; 
					found = nwInfo.DefaultGateway.find(Separator, pos);
				}
				notGateWay = nwInfo.DefaultGateway.substr(pos);
				if(notGateWay.compare("notselected") != 0)
				{
					listGateways.insert(notGateWay);
				}
			}
			set<string>::iterator iter = listGateways.begin();
			for(;iter != listGateways.end(); iter++)
			{
				string NetshCmd = NetshCmdForGateWay + " gateway=" + (*iter) + " gw=1";
				ExecuteNetshCmd(NetshCmd, strResultFile);
			}
		}

		pos =0;
		if(!nwInfo.NameServer.empty())
		{
			list<string> listDnsServers;
			size_t found = nwInfo.NameServer.find(Separator, pos);
			if(found == std::string::npos)
			{
				DebugPrintf(SV_LOG_DEBUG,"Only single DNS server found for this Nic.\n");
				if(nwInfo.NameServer.compare("notselected") != 0)
				{
					listDnsServers.push_back(nwInfo.NameServer);
				}
			}
			else
			{
				string notDnsServer;
				while(found != std::string::npos)
				{
					DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",nwInfo.NameServer.substr(pos,found-pos).c_str());
					notDnsServer = nwInfo.NameServer.substr(pos,found-pos);
					if(notDnsServer.compare("notselected") != 0)
					{
						listDnsServers.push_back(notDnsServer);
					}
					pos = found + 1; 
					found = nwInfo.NameServer.find(Separator, pos);
				}
				notDnsServer = nwInfo.NameServer.substr(pos);
				if(notDnsServer.compare("notselected") != 0)
				{
					listDnsServers.push_back(notDnsServer);
				}
			}
			if(listDnsServers.empty())
			{
				NetshCmdForDns = std::string("netsh interface ip set dns");
				NetshCmdForDns = NetshCmdForDns + std::string(" ") + std::string("\"") + strNetworkName + std::string("\"");
				NetshCmdForDns = NetshCmdForDns + std::string(" ") + std::string("dhcp");
				ExecuteNetshCmd(NetshCmdForDns, strResultFile);
			}
			else
			{
				list<string>::iterator iterDns = listDnsServers.begin();
				for(; iterDns != listDnsServers.end(); iterDns++)
				{
					if(iterDns == listDnsServers.begin())
					{
						NetshCmdForDns = std::string("netsh interface ip set dns");
						NetshCmdForDns = NetshCmdForDns + std::string(" ") + std::string("\"") + strNetworkName + std::string("\"");
						NetshCmdForDns = NetshCmdForDns + std::string(" ") + std::string("static");
					}
					else
					{
						NetshCmdForDns = std::string("netsh interface ip add dns");
						NetshCmdForDns = NetshCmdForDns + std::string(" ") + std::string("\"") + strNetworkName + std::string("\"");
					}
					string NetshCmd = NetshCmdForDns + std::string(" ") + (*iterDns);
					ExecuteNetshCmd(NetshCmd, strResultFile);
				}
			}
		}
		else
		{
			NetshCmdForDns = std::string("netsh interface ip set dns");
			NetshCmdForDns = NetshCmdForDns + std::string(" ") + std::string("\"") + strNetworkName + std::string("\"");
			NetshCmdForDns = NetshCmdForDns + std::string(" ") + std::string("dhcp");
			ExecuteNetshCmd(NetshCmdForDns, strResultFile);
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


bool ChangeVmConfig::ExecuteNetshCmd(string strNetshCmd, string strResultFile)
{
	bool bResult = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	DebugPrintf(SV_LOG_INFO,"Command - %s\n", strNetshCmd.c_str());

	ofstream FOUT(strResultFile.c_str(), std::ios::out | std::ios::app);
	if(! FOUT.is_open())
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s with Error %d\n",strResultFile.c_str(), GetLastError());
	}
	else
	{
		FOUT << strNetshCmd << endl << endl;
	}

	string strTempFile = strResultFile + string(".temp");

	if(false == ExecuteCmdWithOutputToFileWithPermModes(strNetshCmd, strTempFile, O_RDWR | O_CREAT))
	{
		DebugPrintf(SV_LOG_INFO,"Failed to execute the command - %s\n", strNetshCmd.c_str());
		bResult = false;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO,"Successfully executed the command - %s\n", strNetshCmd.c_str());
		string strLineRead;
		ifstream inFile(strTempFile.c_str());
		do
		{
			if(!inFile.is_open())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to read the File : %s with Error : [%lu].\n",strTempFile.c_str(),GetLastError());
				break;
			}
			while(getline(inFile,strLineRead))
			{
				FOUT << strLineRead <<  endl << endl;
				if(strLineRead.find("object already") != string::npos)
				{
					FOUT << "Retrying again as it failed with error \"The object Already Exists\"." << endl << endl;
					FOUT << strNetshCmd << endl << endl;

					Sleep(10000);

					if(false == ExecuteCmdWithOutputToFileWithPermModes(strNetshCmd, strResultFile, O_APPEND | O_RDWR | O_CREAT))
					{
						DebugPrintf(SV_LOG_INFO,"Failed to execute the command - %s\n", strNetshCmd.c_str());
						bResult = false;
					}
					else
					{
						DebugPrintf(SV_LOG_INFO,"Successfully executed the command - %s\n", strNetshCmd.c_str());
					}
					break;
				}
			}
			inFile.close();
		}while(0);
	}
	FOUT.close();
    securitylib::setPermissions(strResultFile);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::CreateMapOfIPandSubNet(string strIpAddress, string strSubNet, map<string, string>& mapOfIpAndSubnet)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	DebugPrintf(SV_LOG_INFO,"IP Addresses : %s\n",strIpAddress.c_str());
	DebugPrintf(SV_LOG_INFO,"SUBNET Addresses : %s\n",strSubNet.c_str());

    std::list<std::string> listIp;
	std::list<std::string> listSubNet;
    std::string Separator = std::string(",");
    size_t pos = 0;

	size_t found = strIpAddress.find(Separator, pos);
	if(found == std::string::npos)
	{
		DebugPrintf(SV_LOG_DEBUG,"Only single IP found at Source for this Nic.\n");
		mapOfIpAndSubnet.insert(make_pair(strIpAddress, strSubNet));
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return true;
	}
	else
	{
		while(found != std::string::npos)
		{
			DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",strIpAddress.substr(pos,found-pos).c_str());
			listIp.push_back(strIpAddress.substr(pos,found-pos));
			pos = found + 1; 
			found = strIpAddress.find(Separator, pos);            
		}
		listIp.push_back(strIpAddress.substr(pos));
	}

	pos = 0;
	found = strSubNet.find(Separator, pos);
	if(found == std::string::npos)
	{
		DebugPrintf(SV_LOG_ERROR,"There is something wrong in collecting NetMask info From source for this Nic \n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	else
	{
		while(found != std::string::npos)
		{
			DebugPrintf(SV_LOG_INFO,"Element - [%s]\n",strSubNet.substr(pos,found-pos).c_str());
			listSubNet.push_back(strSubNet.substr(pos,found-pos));
			pos = found + 1; 
			found = strSubNet.find(Separator, pos);            
		}
		 listSubNet.push_back(strSubNet.substr(pos));
	}

	std::list<std::string>::iterator iterIp = listIp.begin();
	std::list<std::string>::iterator iterNet = listSubNet.begin();

	for(; iterIp != listIp.end(); iterIp++)
	{
		string subNet;
		if(iterNet == listSubNet.end())
			subNet = "255.0.0.0";
		else
		{
			subNet = (*iterNet);
			iterNet++;
		}
		DebugPrintf(SV_LOG_INFO,"IP: %s  --> SubNet: %s\n",(*iterIp).c_str(), subNet.c_str());
		mapOfIpAndSubnet.insert(make_pair((*iterIp), subNet));
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

//***************************************************************************************
// Check if machine is W2K8 or above
//***************************************************************************************
bool ChangeVmConfig::IsItW2k8Machine()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = false;
	//Add code to bring all the earlier cluster disks to online from offline.
	OSVERSIONINFO osVerInfo;
	ZeroMemory(&osVerInfo,sizeof(osVerInfo));
	osVerInfo.dwOSVersionInfoSize = sizeof(osVerInfo);
	BOOL bVer = ::GetVersionEx(&osVerInfo);
	if(bVer)
	{
		if(osVerInfo.dwMajorVersion >= WIN2K8_SERVER_VER)
		{
			bResult = true;
		}
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}

bool ChangeVmConfig::BringAllDisksOnline()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

    int iResult = 0;

    do
    {
        DebugPrintf(SV_LOG_INFO,"Bringing all the offline disks to online.\n");
        //Now bring the disks to online mode        
        //Try 3 times in loop if few disks are failed to come online.     
        //First try for initialized disks.
        for(int iCount = 1; iCount<=3; iCount++)
        {
            Sleep(15000);
            if(SUCCEEDED(BringDisksOnline()))
            {                    
                break;
            }
			CoUninitialize();
            DebugPrintf(SV_LOG_INFO,"%d.Unable to Bring the Initialized Offline Disks to Online Mode.\n",iCount);
            if (3==iCount) iResult = 1;
        }
		CoUninitialize();
        //Second try for uninitialized disks.
        for(int iCount = 1; iCount<=3; iCount++)
        {
            Sleep(15000);
            if(SUCCEEDED(BringUnInitializedDisksOnline()))
            {                                        
                break;
            }
			CoUninitialize();
            DebugPrintf(SV_LOG_INFO,"%d.Unable to Bring the Uninitialized Offline Disks to Online Mode.\n",iCount);                
            if (3==iCount) iResult = 1;
        }
		CoUninitialize();
		m_bInit = false;
        if(1 == iResult)
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to bring the Offline Disks to Online Mode in all 3 attempts.\n");            
            rv = false;
            break;
        }  
        else
            DebugPrintf(SV_LOG_INFO,"Successfully brought all disks from Offline to Online Mode.\n");  
    }while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

bool ChangeVmConfig::RecoverDynDisks(list<string>& listDiskNum)
{
	bool bRet = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);	

	list<string>::iterator iter = listDiskNum.begin();

	for(; iter != listDiskNum.end(); iter++)
	{
		DebugPrintf(SV_LOG_INFO, "Got dynamic disk %s to recover\n", iter->c_str());

		string strDiskPartFile;
		/*string strSysVol;
		if(false == GetSystemVolume(strSysVol))
		{
			DebugPrintf(SV_LOG_INFO,"GetSystemVolume failed.\n");
			strDiskPartFile = string("c:") + RECOVER_FILE_PATH;
		}
		else*/
		strDiskPartFile = m_VxInstallPath + string("\\Failover\\data\\") + RECOVER_FILE_PATH;

		DebugPrintf(SV_LOG_DEBUG,"File Name - %s.\n",strDiskPartFile.c_str());

		ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
		if(! outfile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
			continue;
		}

		string strDiskNumber = string("select disk ") + (*iter); 
		outfile << strDiskNumber << endl;
		outfile << "recover";
		outfile.close();

        securitylib::setPermissions(strDiskPartFile);

		string strExeName = string("diskpart.exe");
		string strParameterToDiskPartExe = string (" /s ") + string("\"")+ strDiskPartFile + string("\"");

		DebugPrintf(SV_LOG_INFO,"Command to recover disks (Dynamic disks) : %s%s\n",strExeName.c_str(),strParameterToDiskPartExe.c_str());
		strExeName = strExeName + strParameterToDiskPartExe;
		string strOurPutFile = m_VxInstallPath + string("\\Failover\\data\\recover_output.txt");
		if(FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to recover the foreign disks %d\n", iter->c_str());
			bRet = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully Recovered the foreign disks\n");
			Sleep(5000);//This is required for updating the disk managemnt
			string strLineRead;
			ifstream inFile(strOurPutFile.c_str());
			
			if(!inFile.is_open())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to read the File : %s with Error : [%lu].\n",strOurPutFile.c_str(),GetLastError());
				bRet = false;
			}
			else
			{
				while(getline(inFile,strLineRead))
				{
					DebugPrintf(SV_LOG_INFO,"strLineRead : %s\n", strLineRead.c_str());
					if(strLineRead.find("successfully") != string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"All the Disks were imported successfully\n");
					}
				}
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}

bool ChangeVmConfig::ImportForeignDisks()
{
	bool rv = true;
	bool bImport = false;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	list<string> listDiskNum;  //this will conatain the list of foreign disks

	CVdsHelper objVds;
	if(!objVds.InitilizeVDS())
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to initialise the vds in all attempts\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		objVds.unInitialize();
		return false;
	}
	objVds.CollectForeignDisks(listDiskNum);

	list<string>::iterator iter = listDiskNum.begin();
	for(; iter != listDiskNum.end(); iter++)
	{
		DebugPrintf(SV_LOG_INFO, "Got dynamic disk %s to import\n", iter->c_str());
		m_bReboot = true;

		string strDiskPartFile = m_VxInstallPath + string("\\Failover\\data\\") + IMPORT_FILE_PATH;

		DebugPrintf(SV_LOG_DEBUG,"File Name - %s.\n",strDiskPartFile.c_str());

		ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
		if(! outfile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
			continue;
		}

		string strDiskNumber = string("select disk ") + (*iter); 
		outfile << strDiskNumber << endl;
		outfile << "import";
		outfile.close();

        securitylib::setPermissions(strDiskPartFile);

		string strExeName = string("diskpart.exe");
		string strParameterToDiskPartExe = string (" /s ") + string("\"")+ strDiskPartFile + string("\"");

		DebugPrintf(SV_LOG_INFO,"Command to import the foreign disks (Dynamic disks) : %s%s\n",strExeName.c_str(),strParameterToDiskPartExe.c_str());
		strExeName = strExeName + strParameterToDiskPartExe;
		string strOurPutFile = m_VxInstallPath + string("\\Failover\\data\\import_output.txt");
		if(FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to import the foreign disks %d\n", iter->c_str());
			bImport = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully imported the foreign disks\n");
			Sleep(5000);//This is required for updating the disk managemnt
			string strLineRead;
			ifstream inFile(strOurPutFile.c_str());
			
			if(!inFile.is_open())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to read the File : %s with Error : [%lu].\n",strOurPutFile.c_str(),GetLastError());
				bImport = false;
			}
			else
			{
				while(getline(inFile,strLineRead))
				{
					DebugPrintf(SV_LOG_INFO,"strLineRead : %s\n", strLineRead.c_str());
					if(strLineRead.find("successfully") != string::npos)
					{
						DebugPrintf(SV_LOG_INFO,"All the Disks were imported successfully\n");
						bImport = true;
					}
				}
			}
		}
		if(bImport)
			break;
	}

	listDiskNum.clear();
	objVds.CollectDynamicDisks(listDiskNum);

	if(bImport)
	{
		bool bAvail = true;
		bAvail = objVds.CheckUnKnowndisks();		
		if(bAvail)
		{
			if(false == RestartVdsService())
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to restart Vds Service, proceeding further\n");
			}
		}
		rescanIoBuses();

		DebugPrintf(SV_LOG_INFO,"Removing the missing disks\n");
		objVds.RemoveMissingDisks();
		DebugPrintf(SV_LOG_INFO,"Clearing the Hidden/Read-only/Shadowcopy flags for volumes if exists\n");
		objVds.ClearFlagsOfVolume();
		if(false == RecoverDynDisks(listDiskNum))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to Recover the dynamic disks having mirrored and Raid-5 volumes\n");
            DebugPrintf(SV_LOG_ERROR,"Manually do the recover on selecting the disks having Raid-5 and mirrored volumes by using diskpart\n");
		}
	}
	else
	{
		if(false == RecoverDynDisks(listDiskNum))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to Recover the dynamic disks having mirrored and Raid-5 volumes\n");
            DebugPrintf(SV_LOG_ERROR,"Manually do the recover on selecting the disks having Raid-5 and mirrored volumes by using diskpart\n");
		}
	}
	objVds.unInitialize();

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


bool ChangeVmConfig::GetMapOfVolGuidToNameFromDriveFile(map<string, string>& mapVolGuidToNameFromFile, string strDriveFile)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	if(strDriveFile.find(string(NETWORK_FILE_NAME)) == string::npos)
	{
		DebugPrintf(SV_LOG_ERROR,"[Error] Drive Information File not found\n");
		return false;
	}

	strDriveFile.replace(strDriveFile.find(string(NETWORK_FILE_NAME)), string(NETWORK_FILE_NAME).length(), string(DRIVE_FILE_NAME));
	//string strDriveFile = m_VxInstallPath + string("\\Failover\\Data\\") + string(DRIVE_FILE_NAME);
	DebugPrintf(SV_LOG_INFO,"Drive Information File %s.\n",strDriveFile.c_str());

    ifstream FIN(strDriveFile.c_str(), std::ios::in);
    if(! FIN.is_open())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to open the file - %s\n",strDriveFile.c_str());
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }

    std::string strLineRead;
	while(getline(FIN, strLineRead))
    {
		try 
		{ 
			boost::trim(strLineRead); 
		} 
		catch (const std::exception& exp) 
		{
			DebugPrintf(SV_LOG_ERROR, "trim exception: %s\n", exp.what());
			DebugPrintf(SV_LOG_ERROR, "Can not parse line %s\n", strLineRead.c_str());
			continue;
		}

        if(strLineRead.empty())
            continue;

        DebugPrintf(SV_LOG_INFO,"%s\n",strLineRead.c_str());

        std::vector<std::string> v;
        std::string Separator = std::string("!@!@!");
        size_t pos = 0;

        size_t found = strLineRead.find(Separator, pos);
        while(found != std::string::npos)
        {
            v.push_back(strLineRead.substr(pos,found-pos));
            pos = found + 5; 
            found = strLineRead.find(Separator, pos);            
        }

		if (v.size() == 2)
		{
			DebugPrintf(SV_LOG_INFO, "VolumeGUID : %s ==> VolumeName : %s\n", v[0].c_str(), v[1].c_str());
			mapVolGuidToNameFromFile.insert(make_pair(v[0], v[1]));
		}
		else
		{
			DebugPrintf(SV_LOG_WARNING, "Line is in unexpected format\n");
		}
    }
    FIN.close();

	if(mapVolGuidToNameFromFile.empty())
	{
		DebugPrintf(SV_LOG_ERROR,"Drive information is empty in the file %s.\n",strDriveFile.c_str());
		rv = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


bool ChangeVmConfig::GetAllVolumesMapOfAllType(map<string, string>& mapVolGuidToNameFromSystem)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	TCHAR buf[BUFSIZE];      // buffer for unique volume identifiers
	HANDLE volumeHandle;     // handle for the volume scan
	BOOL bFlag = 0;              // generic results flag

	volumeHandle = FindFirstVolume (buf, BUFSIZE );

	if (volumeHandle == INVALID_HANDLE_VALUE)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to dicover the Volumes from system.\n");
		rv = false;
	}

	else
	{
		while ( true ) 
		{
			// We have a volume,now process it.
			bFlag = FetchAllTypeVolumes(volumeHandle, buf, BUFSIZE, mapVolGuidToNameFromSystem);
			if( bFlag == TRUE )
			{
				bFlag = FindNextVolume(volumeHandle, buf, BUFSIZE);       

				if( bFlag == FALSE )
				{
					SV_ULONG dwErrorCode = GetLastError();
					if( dwErrorCode == ERROR_NO_MORE_FILES)
					{
						bFlag = TRUE;
					}
					else
					{
						DebugPrintf(SV_LOG_ERROR,"FindNextVolume failed with Error Code: %lu\n",dwErrorCode);
						rv = false;
					}
					break;
				}
			}
			else
			{
				rv = false;
				break;
			}
		}
		// Close out the volume scan.
		FindVolumeClose(volumeHandle);
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


BOOL ChangeVmConfig::FetchAllTypeVolumes(HANDLE volumeHandle, TCHAR *szBuffer, int iBufSize, map<string, string>& mapVolGuidToNameFromSystem)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	BOOL bFlag = TRUE;           // generic results flag for return

	DWORD driveType = GetDriveType(szBuffer);
	if((DRIVE_UNKNOWN == driveType) || (DRIVE_NO_ROOT_DIR == driveType))
	{
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return (bFlag);
	}
	char szVolumePath[MAX_PATH+1] = {0};
	DWORD dwVolPathSize = 0;
	string strVolumeGuid = string(szBuffer);
	char szVolumeLabel[MAX_PATH+1] = {0};
	char szVolumeFileSystem[MAX_PATH+1] = {0};
	char *pszVolumePath = NULL;

	if(GetVolumePathNamesForVolumeName(strVolumeGuid.c_str(), szVolumePath, MAX_PATH, &dwVolPathSize))
	{
		DebugPrintf(SV_LOG_INFO,"VolumeGUID : %s ==> Volume Path : %s \n",strVolumeGuid.c_str(), szVolumePath);
		mapVolGuidToNameFromSystem[strVolumeGuid] = string(szVolumePath);
	}
	else
	{				
		if(GetLastError() == ERROR_MORE_DATA)
		{
			pszVolumePath = new char[dwVolPathSize];
			if(!pszVolumePath)
			{
				DebugPrintf(SV_LOG_ERROR,"\n Failed to allocate required memory to get the list of Mount Point Paths.");
			}
			else
			{
				if(GetVolumePathNamesForVolumeName(strVolumeGuid.c_str(), pszVolumePath, dwVolPathSize, &dwVolPathSize))
				{
					DebugPrintf(SV_LOG_INFO,"VolumeGUID : %s ==> Volume Path : %s \n",strVolumeGuid.c_str(), szVolumePath);
					mapVolGuidToNameFromSystem[strVolumeGuid] = string(szVolumePath);
				}						
				else
				{
					DebugPrintf(SV_LOG_ERROR,"GetVolumePathNamesForVolumeName failed for %s, %s with Error Code: %lu\n",strVolumeGuid.c_str(), szVolumePath, GetLastError());
				}
				delete [] pszVolumePath ;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"GetVolumePathNamesForVolumeName failed for %s, %s with Error Code: %lu\n",strVolumeGuid.c_str(), szVolumePath, GetLastError());
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return (bFlag); 
}


bool ChangeVmConfig::CompareAndAssignVolGuidToPath(map<string, string>& mapVolGuidToNameFromFile, map<string, string>& mapVolGuidToNameFromSystem)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	list<string> listDeleteVols;
	multimap<string, string> mapVolName;
	map<string, string>::iterator iterVolSys;
	map<string, string>::iterator iter = mapVolGuidToNameFromFile.begin();
	for(; iter != mapVolGuidToNameFromFile.end(); iter++)
	{
		iterVolSys = mapVolGuidToNameFromSystem.find(iter->first);
		if(iterVolSys != mapVolGuidToNameFromSystem.end())
		{
			if(iter->second.compare(iterVolSys->second) != 0)
			{
				if(!iterVolSys->second.empty())
					listDeleteVols.push_back(iterVolSys->second);
				mapVolName.insert(make_pair(iter->second, iter->first));
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Volume name %s is equal from source and recoverd VM for Volume GUID %s\n",iterVolSys->second.c_str(), iterVolSys->first.c_str());
			}
		}
		else
		{
			//Need to write logic to take proper action in this case.
			DebugPrintf(SV_LOG_INFO,"Volume GUID %s does not found in system discovery\n",iter->first.c_str());
		}
	}

	list<string>::iterator iterList;
	if(!listDeleteVols.empty())
	{
		listDeleteVols.sort();
		listDeleteVols.reverse();
		for(iterList = listDeleteVols.begin(); iterList != listDeleteVols.end(); iterList++)
		{
			if(0 == DeleteVolumeMountPoint(iterList->c_str()))
			{
				DebugPrintf(SV_LOG_ERROR,"DeleteVolumeMountPoint failed for volume [%s] with ErrorCode : [%lu].\n",iterList->c_str(), GetLastError());
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Successfully deleted the volume [%s].\n",iterList->c_str());
			}
		}
	}
	listDeleteVols.clear();

	multimap<string, string> mapOfVolumeNameToGuid;
	multimap<string, string>::iterator itermap;
	if(!mapVolName.empty())
	{
		m_bReboot = true;
		for(itermap = mapVolName.begin(); itermap != mapVolName.end(); itermap++)
		{
			if(0 == SetVolumeMountPoint(itermap->first.c_str(), itermap->second.c_str()))
			{
				DWORD dError = GetLastError();
				if(ERROR_DIR_NOT_EMPTY == dError)
				{
					mapOfVolumeNameToGuid.insert(make_pair(itermap->first, itermap->second));
					DebugPrintf(SV_LOG_INFO,"volume [%s] is assigned to someother Volume GUID.\n", itermap->first.c_str());
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to set mount point [%s] to volguid [%s] with Error Code : [%lu].\n", itermap->first.c_str(), itermap->second.c_str(), dError);
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Successfully set mount point [%s] to volguid [%s].\n", itermap->first.c_str(), itermap->second.c_str());
			}
		}
	}

	for(iter = mapVolGuidToNameFromSystem.begin(); iter != mapVolGuidToNameFromSystem.end(); iter++)
	{
		for(itermap = mapOfVolumeNameToGuid.begin(); itermap != mapOfVolumeNameToGuid.end(); itermap++)
		{
			if(iter->second.compare(itermap->first) == 0)
			{
				if(0 == DeleteVolumeMountPoint(itermap->first.c_str()))
				{
					DebugPrintf(SV_LOG_ERROR,"DeleteVolumeMountPoint failed for volume [%s] with ErrorCode : [%lu].\n",itermap->first.c_str(), GetLastError());
				}
				else
				{
					listDeleteVols.push_back(iter->first);
					DebugPrintf(SV_LOG_INFO,"Successfully deleted the volume [%s].\n",itermap->first.c_str());
				}
			}
		}
	}

	for(itermap = mapOfVolumeNameToGuid.begin(); itermap != mapOfVolumeNameToGuid.end(); itermap++)
	{
		if(0 == SetVolumeMountPoint(itermap->first.c_str(), itermap->second.c_str()))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to set mount point [%s] to volguid [%s] with Error Code : [%lu].\n", itermap->first.c_str(), itermap->second.c_str(), GetLastError());
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully set mount point [%s] to volguid [%s].\n", itermap->first.c_str(), itermap->second.c_str());
		}
	}

	for(iterList = listDeleteVols.begin(); iterList != listDeleteVols.end(); iterList++)
	{
		string strVolumeName;
		if(false == MountVolume(*iterList, strVolumeName))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to set mount point to volguid [%s].\n", iterList->c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully set mount point [%s] to volguid [%s].\n", iterList->c_str(), strVolumeName.c_str());
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


bool ChangeVmConfig::AssignDriveLetters(string strDriveInfoFile)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	do
	{
		map<string, string> mapVolGuidToNameFromFile;
		if(false == GetMapOfVolGuidToNameFromDriveFile(mapVolGuidToNameFromFile, strDriveInfoFile))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the map of Volumeguid to volume name from drive info file.\n");
			rv = false;
			break;
		}

		map<string, string> mapVolGuidToNameFromSystem;
		if(false == GetAllVolumesMapOfAllType(mapVolGuidToNameFromSystem))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the map of Volumeguid to volume name from system volume discovery.\n");
			rv = false;
			break;
		}

		if(false == CompareAndAssignVolGuidToPath(mapVolGuidToNameFromFile, mapVolGuidToNameFromSystem))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to assign proper volume name to their respective Volumeguid.\n");
			rv = false;
			break;
		}
		DebugPrintf(SV_LOG_INFO,"Successfully completed the drive letter assignment to their respective volumes.\n");
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//*************************************************************
//
//ClearReadonlyFlagOnDisk()
//
//purpose		: Clear readonly flag on a Disk.
//
//parameters	: Disk Number which has to be made online.
//
//returns		: TRUE if success else FALSE.
//
//*************************************************************
bool ChangeVmConfig::ClearReadonlyFlagOnDisk(const std::string & strPhysicalDriveNumber)
{
	DebugPrintf(SV_LOG_INFO, "Entering %s\n", __FUNCTION__);
	bool bRetVal = false;

	std::string strPhyscialDrive = "\\\\.\\PHYSICALDRIVE";
	string strDiskPartFile = m_strDatafolderPath + CLEAR_RO_DISK;
	DebugPrintf(SV_LOG_DEBUG, "File Name - %s.\n", strDiskPartFile.c_str());

	ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
	if (!outfile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR, "\n Failed to open %s \n", strDiskPartFile.c_str());
		DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
		return false;
	}

	DebugPrintf(SV_LOG_INFO, "Clear Readonly flag for disk : %s\n", strPhysicalDriveNumber.c_str());
	outfile << "select disk " << strPhysicalDriveNumber << endl;
	outfile << "attribute disk clear readonly" << endl;
	outfile.flush();
	outfile.close();

	string strExeName = string(DISKPART_EXE);
	string strParameterToDiskPartExe = string(" /s ") + string("\"") + strDiskPartFile + string("\"");
	DebugPrintf(SV_LOG_INFO, "Command to Clear Readonly flag for the disks: %s%s\n", strExeName.c_str(), strParameterToDiskPartExe.c_str());

	strExeName = strExeName + strParameterToDiskPartExe;
	string strOurPutFile = m_strDatafolderPath + string("clear_ro_flag_output.txt");
	if (FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to Clear Readonly flag for the disk.\n");
	}
	else
	{
		bRetVal = true;
		Sleep(5000);
	}

	DebugPrintf(SV_LOG_INFO, "Exiting %s\n", __FUNCTION__);
	return bRetVal;
}

//*************************************************************
//
//OnlineDisk()
//
//purpose		: Online a Disk which is offline.
//
//parameters	: Disk Number which has to be made online.
//
//returns		: TRUE if success else FALSE.
//
//*************************************************************
bool ChangeVmConfig::OnlineDisk( const std::string & strPhysicalDriveNumber )
{
	DebugPrintf( SV_LOG_INFO , "Entering %s\n", __FUNCTION__ );
	bool bRetVal = false;

	std::string strPhyscialDrive = "\\\\.\\PHYSICALDRIVE";
	string strDiskPartFile = m_strDatafolderPath + ONLINE_DISK;
	DebugPrintf(SV_LOG_DEBUG, "File Name - %s.\n", strDiskPartFile.c_str());

	ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
	if (!outfile.is_open())
	{
		DebugPrintf(SV_LOG_ERROR, "\n Failed to open %s \n", strDiskPartFile.c_str());
		DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
		return false;
	}

	DebugPrintf(SV_LOG_INFO, "Online disk : %s\n", strPhysicalDriveNumber.c_str());
	outfile << "select disk " << strPhysicalDriveNumber << endl;
	outfile << "attribute disk clear readonly" << endl;
	outfile << "online disk" << endl;
	outfile.flush();
	outfile.close();

	string strExeName = string(DISKPART_EXE);
	string strParameterToDiskPartExe = string(" /s ") + string("\"") + strDiskPartFile + string("\"");
	DebugPrintf(SV_LOG_INFO, "Command to online the disks: %s%s\n", strExeName.c_str(), strParameterToDiskPartExe.c_str());

	strExeName = strExeName + strParameterToDiskPartExe;
	string strOurPutFile = m_strDatafolderPath + string("online_output.txt");
	if (FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to online the disks for the respective host\n");
	}
	else
	{
		bRetVal = true;
		Sleep(5000);
	}

	DebugPrintf( SV_LOG_INFO , "Exiting %s\n", __FUNCTION__);
	return bRetVal;
}

bool ChangeVmConfig::OnlineAllOfflineDisks()
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	list<string> listDiskNum;  //this will conatain the list of foreign disks

	CVdsHelper objVds;
	if(!objVds.InitilizeVDS())
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to initialise the vds in all attempts\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		objVds.unInitialize();
		return false;
	}
	objVds.CollectOfflineDisks(listDiskNum);

	objVds.unInitialize();

	if(!listDiskNum.empty())
	{
		// Online the disks
		list<string>::iterator iter = listDiskNum.begin();
		for(; iter != listDiskNum.end(); iter++)
		{
			string DiskName = string("\\\\.\\PHYSICALDRIVE") + (*iter);
			DebugPrintf(SV_LOG_INFO, "Got Offline disk %s to online\n", DiskName.c_str());
			//OnlineOrOfflineDisk(DiskName, true);
			//int iPhysicalDriverNumber = std::atoi((*iter).c_str());
			//TFS ID 1248383
			DebugPrintf(SV_LOG_INFO, "Going to online physical driver number %s.\n", (*iter).c_str());
			OnlineDisk((*iter).c_str());
		}
		m_bReboot = true;
	}
	else
	{
		DebugPrintf(SV_LOG_INFO, "No Offline disks found to make them online.\n");
		m_bReboot = false;
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

void ChangeVmConfig::Reboot()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strExeName = string("shutdown.exe");
	string strParameterToDiskPartExe = string (" /r /t 5");
	
	if(FALSE == ExecuteProcess(strExeName,strParameterToDiskPartExe))
	{
		 DebugPrintf(SV_LOG_ERROR,"Failed to Reboot the machine\n");
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

BOOL ChangeVmConfig::rescanIoBuses()
{	
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	BOOL bResult = TRUE;
	string strParameterToDiskPartExe;
	string strExeName;
	string strDiskPartScriptFilePath;
	/*string strSysVol;
	if(false == GetSystemVolume(strSysVol))
	{
		DebugPrintf(SV_LOG_INFO,"GetSystemVolume failed.\n");
		strDiskPartScriptFilePath = string("c:") + RESCAN_DISKS_FILE;
	}
	else*/
	strDiskPartScriptFilePath = m_VxInstallPath + string("\\Failover\\data\\")  + RESCAN_DISKS_FILE;
	ofstream outfile(strDiskPartScriptFilePath.c_str(), ios_base::out);
	bool bNeedToCallRevert = false;
	bNeedToCallRevert = enableDiableFileSystemRedirection() ; 
	

	do
	{
		if(outfile.fail())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s. Cannot continue further.\n",strDiskPartScriptFilePath.c_str());
			bResult = FALSE;
			break;
		}
		outfile << "rescan";
		outfile << endl;
		outfile.flush();
		outfile.close();
		strExeName = string("diskpart.exe");
		strParameterToDiskPartExe = string (" /s ") +  string("\"") + strDiskPartScriptFilePath + string("\"");
		
		if(FALSE == ExecuteProcess(strExeName,strParameterToDiskPartExe))
		{
			 bResult = false;
			 DebugPrintf(SV_LOG_ERROR,"Rescanning of IO buses failed. Please run rescan from diskmgmt.msc manually and then try again.\n");
		}
		if(bNeedToCallRevert)
		{
			enableDiableFileSystemRedirection( false ) ;
		}
	}while(0);

    DeleteFile(strDiskPartScriptFilePath.c_str());
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return bResult;
}

bool ChangeVmConfig::RestartVdsService()
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	string svcName = "vds";
	if(S_FALSE == restartService(svcName))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to re-start the \"vds\" service \n");
		rv = false;
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"Successfully re-started the \"vds\" service \n");
		rv = true;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}


HRESULT ChangeVmConfig::restartService(const string& serviceName)
{
	 DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	 SC_HANDLE schService;        
     SERVICE_STATUS_PROCESS  serviceStatusProcess;
	 SERVICE_STATUS serviceStatus;
	 DWORD actualSize = 0;
	 ULONG dwRet = 0;
	 LPVOID lpMsgBuf;
	 SC_HANDLE schSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	 if (NULL == schSCManager) 
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to Get handle to Windows Services Manager in OpenSCMManager.\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return S_FALSE;
    }

	// stop requested service
	// PR#10815: Long Path support
	schService = ::SVOpenService(schSCManager, serviceName.c_str(), SERVICE_ALL_ACCESS); 
    if(NULL != schService) 
	{ 
		if (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_RUNNING == serviceStatusProcess.dwCurrentState) 
		{
            DebugPrintf(SV_LOG_INFO,"Stopping %s service.\n",serviceName.c_str());
            // stop service
			if (::ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus)) 
			{
                // wait for it to actually stop
				int retry = 1;
				do 
				{
					Sleep(1000); 
				} while (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_STOPPED != serviceStatusProcess.dwCurrentState && retry++ <= 600/*180 */);//some times service was taking too much time to stop..So increased this value to 10 mins as suggested by Jayesh
				if (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_STOPPED != serviceStatusProcess.dwCurrentState) 
				{
					dwRet = GetLastError();     
					DebugPrintf(SV_LOG_ERROR,"Failed to stop the service: %s with Error Code : [%lu].\n",serviceName.c_str(),GetLastError());
					::CloseServiceHandle(schService); 
					goto Error_return_fail;    
				}
            }
        }
		else
		{
			DebugPrintf(SV_LOG_INFO,"The service  %s is currently not running.\n",serviceName.c_str());
		}

		// start the service
        DebugPrintf(SV_LOG_INFO,"Starting %s service.\n",serviceName.c_str());
		if (::StartService(schService, 0, NULL) == 0) 
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to start the service: %s with Error Code : [%lu].\n",serviceName.c_str(),GetLastError());
			dwRet = GetLastError();
			::CloseServiceHandle(schService); 
			goto Error_return_fail;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully started the service  %s \n",serviceName.c_str());
		}
		::CloseServiceHandle(schService); 
	}
	else
	{
		dwRet = GetLastError();
		goto Error_return_fail;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return S_OK;
	Error_return_fail: 
	::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_FROM_SYSTEM,
                    NULL,
                    dwRet,
                    0,
                    (LPTSTR)&lpMsgBuf,
                    0,
                    NULL);
	if(dwRet)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to restart %s service Error Code : [%lu].\n",serviceName.c_str(), dwRet);
        DebugPrintf(SV_LOG_ERROR,"Error %lu = %s",dwRet,(LPCTSTR)lpMsgBuf);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	}  

	// Free the buffer
	LocalFree(lpMsgBuf);
	return S_FALSE;
}


bool ChangeVmConfig::ExecuteProcess(const std::string &FullPathToExe, const std::string &Parameters)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	string strFullCommand = FullPathToExe + Parameters;
    STARTUPINFO StartupInfo;
	PROCESS_INFORMATION ProcessInformation;
	memset(&StartupInfo, 0x00, sizeof(STARTUPINFO));
	StartupInfo.cb = sizeof(STARTUPINFO);
	memset(&ProcessInformation, 0x00, sizeof(PROCESS_INFORMATION));
	if(!::CreateProcess(NULL,const_cast<char*>(strFullCommand.c_str()),NULL,NULL,FALSE,NULL,NULL, NULL, &StartupInfo, &ProcessInformation))
	{
        DebugPrintf(SV_LOG_ERROR,"CreateProcess failed with Error [%lu]\n",GetLastError());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	DWORD retValue = WaitForSingleObject( ProcessInformation.hProcess, INFINITE );
	if(retValue == WAIT_FAILED)
	{
		DebugPrintf(SV_LOG_ERROR,"WaitForSingleObject has failed.\n");
		::CloseHandle(ProcessInformation.hProcess);
		::CloseHandle(ProcessInformation.hThread);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}
	::CloseHandle(ProcessInformation.hProcess);
	::CloseHandle(ProcessInformation.hThread);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}


//******************************************************************************
//This function grants Debug privileges to the program.
//******************************************************************************
void ChangeVmConfig::EnableDebugPriv( void )
{
 DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
 HANDLE hToken;
 LUID sedebugnameValue;
 TOKEN_PRIVILEGES tkp;

 // enable the SetDebugPrivilege
 if ( ! OpenProcessToken( GetCurrentProcess(),
  TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken ) )
 {
  wprintf( L"OpenProcessToken() failed, Error = %d SeDebugPrivilege is not available.\n", GetLastError() );
  return;
 }

 if ( ! LookupPrivilegeValue( NULL, SE_DEBUG_NAME, &sedebugnameValue ) )
 {
  wprintf( L"LookupPrivilegeValue() failed, Error = %d SeDebugPrivilege is not available.\n", GetLastError() );
  CloseHandle( hToken );
  return;
 }

 tkp.PrivilegeCount = 1;
 tkp.Privileges[0].Luid = sedebugnameValue;
 tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

 if ( ! AdjustTokenPrivileges( hToken, FALSE, &tkp, sizeof tkp, NULL, NULL ) )
  wprintf( L"AdjustTokenPrivileges() failed, Error = %d SeDebugPrivilege is not available.\n", GetLastError() );
  
 CloseHandle( hToken );
 DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

//*****************************************************************************
//This function cleans up the DiskProperties structure of VDS.
//*****************************************************************************
void ChangeVmConfig::CleanUpDiskProps(VDS_DISK_PROP &vdsDiskProperties)
{
  DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
  //Free the strings in the properties structure.
  if (NULL != vdsDiskProperties.pwszDiskAddress)
  {
    CoTaskMemFree(vdsDiskProperties.pwszDiskAddress);
	vdsDiskProperties.pwszDiskAddress = NULL;
  }
  if(NULL != vdsDiskProperties.pwszName)
  {
    CoTaskMemFree(vdsDiskProperties.pwszName);
	vdsDiskProperties.pwszName = NULL;
  }
  if(NULL != vdsDiskProperties.pwszFriendlyName)
  {
    CoTaskMemFree(vdsDiskProperties.pwszFriendlyName);
	vdsDiskProperties.pwszFriendlyName = NULL;
  }
  if(NULL != vdsDiskProperties.pwszAdaptorName)
  {
    CoTaskMemFree(vdsDiskProperties.pwszAdaptorName);
	vdsDiskProperties.pwszAdaptorName = NULL;
  }
  if(NULL != vdsDiskProperties.pwszDevicePath)
  {
    CoTaskMemFree(vdsDiskProperties.pwszDevicePath);
	vdsDiskProperties.pwszDevicePath = NULL;
  }
  ZeroMemory(&vdsDiskProperties, sizeof(VDS_DISK_PROP));
  DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}
//*****************************************************************************
//This function enumerates the VDS Disks available in the list of available
//VDS packs.It then brings the required disks which are Offline to Online.
//*****************************************************************************
HRESULT ChangeVmConfig::ProcessVdsPacks(IN IEnumVdsObject **pEnumIVdsPack)
{
   DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
   HRESULT rv = 0;

   USES_CONVERSION;
   VDS_DISK_PROP       vdsDiskProperties;
   IEnumVdsObject      *pEnumVdsDisk     = NULL;
   IVdsDisk            *pDisk            = NULL;
   IVdsPack            *pIVdsPack        = NULL;
   IVdsDiskOnline      *pVdsDiskOnline   = NULL;
   IUnknown            *pIUnknown        = NULL;
   HRESULT             hResult           = S_OK;
   ULONG               ulFetched         = 0;

   ZeroMemory(&vdsDiskProperties, sizeof(VDS_DISK_PROP));//initialize the structure members.

  assert(NULL != pEnumIVdsPack);
  while(((*pEnumIVdsPack)->Next(1,&pIUnknown,&ulFetched))== 0x00)
  {
    if(!pIUnknown)
    {
      DebugPrintf(SV_LOG_ERROR,"Unable to get the Pack from the VDS Software Provider.\n");
      return -1;
    }
    hResult = pIUnknown->QueryInterface(IID_IVdsPack,(void**)&pIVdsPack);
    if(!SUCCEEDED(hResult)|| (!pIVdsPack))
    {
      DebugPrintf(SV_LOG_ERROR,"Unable to get the Vds Pack object.\n");
      _SafeRelease(pIUnknown);
      return -1;
    }
    _SafeRelease(pIUnknown);

    hResult = pIVdsPack->QueryDisks(&pEnumVdsDisk);
    if(!SUCCEEDED(hResult))
    {
      DebugPrintf(SV_LOG_ERROR,"Failed to get the Disks from the Pack.\n");
      _SafeRelease(pIVdsPack);
      return -1;//S_FALSE;
    }

    IUnknown *pUn = NULL;
    while(((pEnumVdsDisk->Next(1,&pUn,&ulFetched)) == 0x00))
    {
      DebugPrintf(SV_LOG_INFO,"In Loop (for debugging purpose).\n");
      assert(1 == ulFetched);
      if(pUn)
      {
        hResult = pUn->QueryInterface(IID_IVdsDisk,(void**)&pDisk);
        if(!SUCCEEDED(hResult) || (!pDisk))
        {
          DebugPrintf(SV_LOG_ERROR,"Unable to get the Disk pointer.\n");
          _SafeRelease(pUn);
          return -1;
        }
        _SafeRelease(pUn);

        hResult = pDisk->GetProperties( &vdsDiskProperties);
        if(SUCCEEDED(hResult))
        {
          DebugPrintf(SV_LOG_INFO,"Disk Name - %s <==> State - %d <==> Flag - %lu\n",W2A(vdsDiskProperties.pwszName),vdsDiskProperties.status,vdsDiskProperties.ulFlags);          
          if(VDS_DS_OFFLINE == vdsDiskProperties.status)
          {
            hResult = pDisk->QueryInterface(IID_IVdsDiskOnline,(void**)&pVdsDiskOnline);
            if(SUCCEEDED(hResult) && (NULL != pVdsDiskOnline))
            {
              hResult = pVdsDiskOnline->Online();
              if(!SUCCEEDED(hResult))
              {
                DebugPrintf(SV_LOG_ERROR,"Error bringing the disk %s to online.\n",W2A(vdsDiskProperties.pwszName));//MCHECK
                rv = -1;
                /*_SafeRelease(pVdsDiskOnline);
                return -1;*/
              }
              else
              {				
                //Clear the ReadOnly Flags if any
                hResult = pDisk->ClearFlags(VDS_DF_READ_ONLY);                
                if(!SUCCEEDED(hResult))
                {
                  DebugPrintf(SV_LOG_ERROR,"Could not clear the Read-Only flag of the disk %s.\n",W2A(vdsDiskProperties.pwszName));
                  rv = -1;
                  /*_SafeRelease(pVdsDiskOnline);
                  _SafeRelease(pDisk);                  
                  return hResult;*/
                }
                else
                {
                    m_NumberOfDisksMadeOnline++;
                    DebugPrintf(SV_LOG_INFO,"Made the disk %s Online and Cleared the Read-Only flag.\n",W2A(vdsDiskProperties.pwszName));//MCHECK
                }
                Sleep(15000);
              }
              _SafeRelease(pVdsDiskOnline);
            }
          }
          else if(VDS_DF_READ_ONLY & vdsDiskProperties.ulFlags)//since flag use & (13222 bug)
          {
              hResult = pDisk->ClearFlags(VDS_DF_READ_ONLY);
              if(!SUCCEEDED(hResult))
              {
                  DebugPrintf(SV_LOG_ERROR,"Could not clear the Read-Only flag of the disk %s.\n",W2A(vdsDiskProperties.pwszName));
                  rv = -1;
                  /*_SafeRelease(pVdsDiskOnline);
                  _SafeRelease(pDisk);                  
                  return hResult;*/
              }
              else
              {
                  m_NumberOfDisksMadeOnline++;
                  DebugPrintf(SV_LOG_INFO,"Cleared the Read-Only flag of the disk %s.\n",W2A(vdsDiskProperties.pwszName));
              }
              Sleep(15000);
          }
        }
        else
        {
          _SafeRelease(pDisk);
          return -1;
        }
        _SafeRelease(pDisk);
      }
      else
      {
        hResult = E_UNEXPECTED; // Errant provider
        DebugPrintf(SV_LOG_ERROR,"Failed: Pointer to IUnknown Interface could not be obtained.\n");
        _SafeRelease(pEnumVdsDisk);
        return -1;
      }
    }//end of while
    _SafeRelease(pEnumVdsDisk);
    _SafeRelease(pIVdsPack);    
  }//end of Outer while
  CleanUpDiskProps(vdsDiskProperties);
  DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
  return rv;
}
//*****************************************************************************
//This function first enumerates all the disks then brings the required disks
//to Online mode.
//*****************************************************************************
HRESULT ChangeVmConfig::BringDisksOnline()
{
  DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
  HRESULT rv = S_OK;
  HRESULT           hResult;
  IVdsService       *pService         = NULL;
  IVdsServiceLoader *pLoader          = NULL;
  IVdsDisk          *pDisk            = NULL;
  IUnknown          *pIUnknown        = NULL;
  IVdsSwProvider    *pIVdsSwProvider  = NULL;
  IEnumVdsObject    *pEnumIVdsPack    = NULL;
  unsigned long     ulFetched         = 0;

  EnableDebugPriv();

  // For first get a pointer to the VDS Loader
  hResult = CoInitialize(NULL);

  if (SUCCEEDED(hResult))
  {
    hResult = CoCreateInstance( CLSID_VdsLoader,
                                NULL,
                                CLSCTX_LOCAL_SERVER,
                                IID_IVdsServiceLoader,
                                (void **) &pLoader);

    //Launch the VDS service.;and then load VDS on the local computer.
    if (SUCCEEDED(hResult))
    {
        hResult = pLoader->LoadService(NULL, &pService);
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR,"LoadService failed.\n");
    }
    if (SUCCEEDED(hResult))
    {
        hResult = pService->WaitForServiceReady();
        if (SUCCEEDED(hResult))
        {
          //printf("VDS Service Loaded");
          IEnumVdsObject *pProviderEnum;
          hResult = pService->QueryProviders(VDS_QUERY_SOFTWARE_PROVIDERS | VDS_QUERY_HARDWARE_PROVIDERS,&pProviderEnum);
          if(!SUCCEEDED(hResult))
          {
            DebugPrintf(SV_LOG_ERROR,"Error:Could not get the Software Providers of VDS.\n");
            _SafeRelease(pService);
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
            return -1;
          }
          _SafeRelease(pService);
          
          while(pProviderEnum && (pProviderEnum->Next(1,(IUnknown**)&pIUnknown,&ulFetched) == 0x00))
          {
            if(!pIUnknown)
            {
              DebugPrintf(SV_LOG_ERROR,"Failed:Pointer to pIUnknown interface could not be obtained for the provider.\n");
              _SafeRelease(pProviderEnum);
              DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
              return -1;
            }
            assert(1 == ulFetched);
            hResult = pIUnknown->QueryInterface(IID_IVdsSwProvider,(void**)&pIVdsSwProvider);
            if(SUCCEEDED(hResult) && (NULL != pIVdsSwProvider))
            {
              hResult = pIVdsSwProvider->QueryPacks(&pEnumIVdsPack);
              if(!SUCCEEDED(hResult))
              {
               DebugPrintf(SV_LOG_ERROR,"Error: Unable to get the Pack from the Provider.\n");
                _SafeRelease(pIVdsSwProvider);
                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                return -1;
              }
              _SafeRelease(pIVdsSwProvider);

              hResult = ProcessVdsPacks(&pEnumIVdsPack);
              if(!SUCCEEDED(hResult))
              {
                DebugPrintf(SV_LOG_ERROR,"Unable to bring some or all of the disks Online. Result = %x\n",hResult);
                rv = -1;
              }
              _SafeRelease(pEnumIVdsPack);
            }
            else 
            {
              DebugPrintf(SV_LOG_ERROR,"IVdsSubSystem failed. Result = %x\n",hResult);
              _SafeRelease(pIUnknown);
              DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
              return -1;
            }
             _SafeRelease(pIUnknown);
          }//end of while
          _SafeRelease(pProviderEnum);
        }//end of checking the condition for WaitForServiceReady
      }
      else
      {
          DebugPrintf(SV_LOG_ERROR,"VDS Service failed hr=%x\n",hResult);
      }
    // You're done with the Loader interface at this point.
    _SafeRelease(pLoader); 
    //printf("\n VDS Service is unloaded.\n");
  }//end of CoCreateInstance condition check
  DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
  return rv;
}

HRESULT ChangeVmConfig::BringUnInitializedDisksOnline() 
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    HRESULT rv = 0;

	USES_CONVERSION;
    HRESULT 			hResult			    = S_OK;
    ULONG 				ulFetched 		    = 0;
    IVdsServiceLoader 	*pLoader 		    = NULL;
    IVdsService 		*pService 		    = NULL;    
    IEnumVdsObject 		*pUnallocatedDisks 	= NULL;
    IVdsDisk 			*pDisk 			    = NULL;
    IUnknown 			*pUnknown 		    = NULL;
	IVdsDiskOnline 		*pVdsDiskOnline	    = NULL;
    VDS_DISK_PROP 		vdsDiskProperties;
    ZeroMemory(&vdsDiskProperties, sizeof(VDS_DISK_PROP));//initialize the structure members.    

    EnableDebugPriv();

    // Initialize COM
    hResult = CoInitialize(NULL);
    if (!SUCCEEDED(hResult)) 	
	{
		DebugPrintf(SV_LOG_ERROR,"Unable to Initialize COM. Error = %x\n",hResult);
		return hResult;
	}

	// For this, get a pointer to the VDS Loader
	hResult = CoCreateInstance(CLSID_VdsLoader,
								NULL,
								CLSCTX_LOCAL_SERVER,
								IID_IVdsServiceLoader,
								(void **) &pLoader);
	if (!SUCCEEDED(hResult)) 
	{
		DebugPrintf(SV_LOG_ERROR,"Unable to Create Instance i.e pointer to VDS Loader. Error = %x\n",hResult);
		return hResult;
	}
	
	// Launch the VDS service. 
	hResult = pLoader->LoadService(NULL, &pService);	
	if (!SUCCEEDED(hResult))
	{
		DebugPrintf(SV_LOG_ERROR,"Unable to load the VDS service. Error = %x\n",hResult);
		_SafeRelease(pLoader); 
		return hResult;
	}
	_SafeRelease(pLoader);  // We're done with the Loader interface at this point.
	
	// Wait for service to be ready
	hResult = pService->WaitForServiceReady();
	if (!SUCCEEDED(hResult))
	{
		DebugPrintf(SV_LOG_ERROR,"Unable to wait for the VDS service to be ready. Error = %x\n",hResult);
		_SafeRelease(pService);
		return hResult;		
	}	
	// Query for unallocated disks	
	hResult = pService->QueryUnallocatedDisks(&pUnallocatedDisks);
	if (!SUCCEEDED(hResult)) 
	{
		DebugPrintf(SV_LOG_ERROR,"Unable to Query the Unallocated disks from VDS service. Error = %x\n",hResult);
		_SafeRelease(pService);
		return hResult;
	}
	_SafeRelease(pService);

	// Iterate over unallocated disks
	while(0x00 == (pUnallocatedDisks->Next(1, &pUnknown, &ulFetched)))
	{
        DebugPrintf(SV_LOG_INFO,"In Loop (for debugging purpose).\n");
		if(!pUnknown)
		{		
			DebugPrintf(SV_LOG_ERROR,"Unable to get the next Unallocated disk from the VDS Provider.\n");
			_SafeRelease(pUnallocatedDisks);
			return hResult;
		}
		// Cast the current value to a  disk pointer
		hResult = pUnknown->QueryInterface(IID_IVdsDisk, (void **) &pDisk); 
		if (!SUCCEEDED(hResult)) 
		{
			DebugPrintf(SV_LOG_ERROR,"Unable to get the VDS disk object. Error = %x\n",hResult);
			_SafeRelease(pUnknown);
			_SafeRelease(pUnallocatedDisks);
			return hResult;
		}
		_SafeRelease(pUnknown);
		
		hResult = pDisk->GetProperties( &vdsDiskProperties);
		if(SUCCEEDED(hResult))
		{
            DebugPrintf(SV_LOG_INFO,"Disk Name - %s <==> State - %d <==> Flag - %lu\n",W2A(vdsDiskProperties.pwszName),vdsDiskProperties.status,vdsDiskProperties.ulFlags);            
			if(VDS_DS_OFFLINE == vdsDiskProperties.status)
			{
				
				hResult = pDisk->QueryInterface(IID_IVdsDiskOnline,(void**)&pVdsDiskOnline);
				if(SUCCEEDED(hResult) && (NULL != pVdsDiskOnline))
				{
					hResult = pVdsDiskOnline->Online();
					if(!SUCCEEDED(hResult))
					{
						DebugPrintf(SV_LOG_ERROR,"Error bringing the disk %s to online.\n",W2A(vdsDiskProperties.pwszName));//MCHECK
                        rv = -1;
					}
					else
					{
						hResult = pDisk->ClearFlags(VDS_DF_READ_ONLY);
						if(!SUCCEEDED(hResult))
						{
							DebugPrintf(SV_LOG_ERROR,"Could not clear the Read-Only flag of the disk %s.\n",W2A(vdsDiskProperties.pwszName));
                            rv = -1;
                            /*_SafeRelease(pVdsDiskOnline);
                            _SafeRelease(pDisk);
                            _SafeRelease(pUnallocatedDisks);
                            return hResult;*/
						}
                        else
                        {
                            m_NumberOfDisksMadeOnline++;
                            DebugPrintf(SV_LOG_INFO,"Made the disk %s Online and Cleared the Read-Only flag.\n",W2A(vdsDiskProperties.pwszName));//MCHECK                    
                        }
						Sleep(15000);
					}
					_SafeRelease(pVdsDiskOnline);
				}
			}
            else if(VDS_DF_READ_ONLY & vdsDiskProperties.ulFlags) //since flag use & (13222 bug)
            {
                hResult = pDisk->ClearFlags(VDS_DF_READ_ONLY);
				if(!SUCCEEDED(hResult))
				{
					DebugPrintf(SV_LOG_ERROR,"Could not clear the Read-Only flag of the disk %s.\n",W2A(vdsDiskProperties.pwszName));
                    rv = -1;
                    /*_SafeRelease(pVdsDiskOnline);
                    _SafeRelease(pDisk);
                    _SafeRelease(pUnallocatedDisks);
                    return hResult;*/
				}
                else
                {
                    m_NumberOfDisksMadeOnline++;
                    DebugPrintf(SV_LOG_INFO,"Cleared the Read-Only flag of the disk %s.\n",W2A(vdsDiskProperties.pwszName));
                }
				Sleep(15000);
            }
		}             
		_SafeRelease(pDisk);		
	}	
	_SafeRelease(pUnallocatedDisks);	
    CleanUpDiskProps(vdsDiskProperties);
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//***************************************************************************************
// Fetch the Source Machines drive letter which contains system32 folder.
// Read HostName_SysVolInfo.txt in failover data folder.
// Input  - Hostname 
// Output - Source System Drive (Format - C:\ )
//***************************************************************************************
bool ChangeVmConfig::GetSrcSystemVolume(std::string HostName, std::string & SrcSystemVolume)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;
	std::string strSystemVolPath;
	std::string strSysVolInfoFile;
	std::string strLine;
	strSysVolInfoFile = m_VxInstallPath + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("failover") + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("data") + ACE_DIRECTORY_SEPARATOR_CHAR_A + HostName + std::string("_SysVolInfo.txt");
	
	if(false == checkIfFileExists(strSysVolInfoFile))
	{
        DebugPrintf(SV_LOG_ERROR,"File - %s does not exist.\n",strSysVolInfoFile.c_str());
		map<string, string>::iterator iter = m_mapOfVmNameToId.find(HostName);
		if(iter != m_mapOfVmNameToId.end())
		{
			HostName = iter->second;
			strSysVolInfoFile = m_VxInstallPath + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("failover") + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("data") + ACE_DIRECTORY_SEPARATOR_CHAR_A + HostName + std::string("_SysVolInfo.txt");
		}
	}
	
	ifstream inFile(strSysVolInfoFile.c_str());

	size_t index = 0;
	do
	{
		if(!inFile.is_open())
		{
            DebugPrintf(SV_LOG_ERROR, "Failed to open file - %s for reading.\n",strSysVolInfoFile.c_str());
			bRetValue = false;
			break;
		}
		if(inFile>>strLine)
		{
			strSystemVolPath = strLine;

			/**************** We are assuming Vm will never be an Dual boot able VM *****************/
			index = strSystemVolPath.find_first_of(":\\");

			if(index != std::string::npos)
			{				
				//We got the base volume 
				SrcSystemVolume = strSystemVolPath.substr(0,index+2);
    		}
			else
			{
                DebugPrintf(SV_LOG_ERROR, "Failed to get token from file - %s\n",strSysVolInfoFile.c_str());
				bRetValue = false;
				break;
			}
		}
        if(SrcSystemVolume.empty())
        {
            DebugPrintf(SV_LOG_ERROR, "System Volume is empty for the source machine - %s\n", HostName.c_str());
			bRetValue = false;
			break;
        }
        else
        {
            DebugPrintf(SV_LOG_INFO,"SrcSystemVolume - %s\n",SrcSystemVolume.c_str());
        }
	}while(0);
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}

//***************************************************************************************
// Get the map of source to target volumes for given HostName from the .pInfo file
//   if "mapOfSrcToTgtVolumes" is empty, function will return false
//***************************************************************************************
bool ChangeVmConfig::GetMapOfSrcToTgtVolumes(std::string HostName, std::map<std::string,std::string> & mapOfSrcToTgtVolumes)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool bRetValue = true;
	std::string strReplicationPairInfoFile;
	std::string strTemp;
	std::string strSourceDeviceName;
	std::string strLine;
	std::string strTargetDeviceName;
	size_t index;
	std::string strToken("!@!@!");
	if(m_bDrDrill)
		strReplicationPairInfoFile = m_VxInstallPath + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("failover") + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("data") + ACE_DIRECTORY_SEPARATOR_CHAR_A + HostName + std::string(".sInfo");
	else
		strReplicationPairInfoFile = m_VxInstallPath + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("failover") + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("data") + ACE_DIRECTORY_SEPARATOR_CHAR_A + HostName + std::string(".pInfo");
	
    if(false == checkIfFileExists(strReplicationPairInfoFile))
	{
        DebugPrintf(SV_LOG_ERROR,"File %s does not exist.\n",strReplicationPairInfoFile.c_str());
		map<string, string>::iterator iter = m_mapOfVmNameToId.find(HostName);
		if(iter != m_mapOfVmNameToId.end())
		{
			HostName = iter->second;
			if(m_bDrDrill)
				strReplicationPairInfoFile = m_VxInstallPath + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("failover") + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("data") + ACE_DIRECTORY_SEPARATOR_CHAR_A + HostName + std::string(".sInfo");
			else
				strReplicationPairInfoFile = m_VxInstallPath + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("failover") + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("data") + ACE_DIRECTORY_SEPARATOR_CHAR_A + HostName + std::string(".pInfo");
		}
	}
	
    ifstream inFile(strReplicationPairInfoFile.c_str());	
    do
	{
		if(!inFile.is_open())
		{
            DebugPrintf(SV_LOG_ERROR, "Failed to open %s for reading.\n",strReplicationPairInfoFile.c_str());
			bRetValue = false;
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return bRetValue;
		}
		while(getline(inFile,strLine))
		{
            DebugPrintf(SV_LOG_INFO,"Line read =  %s\n",strLine.c_str());
			strTemp = strLine;
			index = strTemp.find_first_of(strToken);
			if(index != std::string::npos)
			{
				strSourceDeviceName = strTemp.substr(0,index);
				strTargetDeviceName = strTemp.substr(index+strToken.length(),strTemp.length());
				mapOfSrcToTgtVolumes.insert(make_pair(strSourceDeviceName,strTargetDeviceName));
			}
			else
			{
				bRetValue = false;
				mapOfSrcToTgtVolumes.clear();
				break;
			}
		}
	}while(0);
	inFile.close();

	if(mapOfSrcToTgtVolumes.empty())
	{
        DebugPrintf(SV_LOG_ERROR,"Failed to fetch the Replication Pair info. It seems file %s is empty.\n",strReplicationPairInfoFile.c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return false;
	}

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return true;
}

HRESULT ChangeVmConfig::WMINwInterfaceQuery(map<InterfaceName_t, unsigned int>& mapOfInterfaceStatus)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME) ;
	USES_CONVERSION;

	string strNetworkName;
	unsigned int nNwStatus = 2;

	BSTR language = SysAllocString(L"WQL");
	BSTR query = SysAllocString(L"SELECT * FROM Win32_NetworkAdapter where NetConnectionID != NULL");
    // Create querying object
    IWbemClassObject *pObj = NULL;
    IWbemServices *pServ = NULL;
    IEnumWbemClassObject *pEnum = NULL;
    HRESULT hRet;

    if(FAILED(GetWbemService(&pServ))) 
	{
	    DebugPrintf(SV_LOG_ERROR,"Failed to initialise GetWbemService.\n");
		CoUninitialize();
		m_bInit = false;
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n", FUNCTION_NAME);
		return S_FALSE; 
	}
    // Issue the query.
    hRet = pServ->ExecQuery(
			language,
			query,
			WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,         // Flags
			0,                              // Context
			&pEnum
			);

    if (FAILED(hRet))
    {
        DebugPrintf(SV_LOG_ERROR,"Query for NIC's Device ID failed.Error Code %0X\n",hex);
        pServ->Release();
        CoUninitialize();
		m_bInit = false;
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n", FUNCTION_NAME) ;
        return S_FALSE;                // Program has failed.
    }
    SysFreeString(language);
    SysFreeString(query);
        
    // Get the disk drive information
    ULONG uTotal = 0;

    // Retrieve the objects in the result set.
    while (pEnum)
    {
        ULONG uReturned = 0;
        hRet = pEnum->Next
				(
					WBEM_INFINITE,       // Time out
					1,                  // One object
					&pObj,
					&uReturned
				);
		if (uReturned == 0)
			break;
		VARIANT val;
		uTotal += uReturned;
		
		hRet = pObj->Get(L"NetConnectionID", 0, &val, 0, 0); // Get the value of the Name property
		strNetworkName = string(W2A(val.bstrVal));
		VariantClear(&val);

		hRet = pObj->Get(L"NetConnectionStatus", 0, &val, 0, 0);        
		nNwStatus = val.uintVal;
        VariantClear(&val);

		mapOfInterfaceStatus.insert(make_pair(strNetworkName, nNwStatus));

		pObj->Release();    // Release objects not owned.		
    }
    // All done. Free the Resources
    pEnum->Release();
    pServ->Release();
	CoUninitialize();
	m_bInit = false;
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n", FUNCTION_NAME) ;
	return S_OK; 
}

HRESULT ChangeVmConfig::WMINwInterfaceQueryWithMAC( NetworkInfoMap_t NwMap , map<InterfaceName_t, unsigned int>& mapOfInterfaceStatus)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME) ;
	USES_CONVERSION;

	string strNetworkName;
	unsigned int nNwStatus = 2;

	BSTR language = SysAllocString(L"WQL");

	//Get All MACAddresses in to a List. Other wise we don't know whether next MAC Address is empty or not.
	list <string> list_MACAddresses;
	for(NetworkInfoMapIter_t iter = NwMap.begin(); iter != NwMap.end(); iter++)
    {
		string strMacAddress = iter->second.MacAddress;
		if( strMacAddress.empty() )
		{
			continue;
		}
		list_MACAddresses.push_back(strMacAddress);
	}

	//Edit this string to form query string on basis of MAC address.
	string str_query = string("SELECT * FROM Win32_NetworkAdapter");
	if( !list_MACAddresses.empty() )
	{
		str_query = str_query + string(" WHERE");
		std::list<std::string>::iterator iter = list_MACAddresses.begin();
		while ( list_MACAddresses.end() != iter )
		{
			if ( list_MACAddresses.begin() == iter )
			{
				str_query = str_query + string(" MACAddress = '") + *iter + string("'"); 
			}
			else
			{
				str_query = str_query + string(" OR MACAddress = '") + *iter + string("'");
			}
			iter++;
		}
	}
	
	DebugPrintf( SV_LOG_ALWAYS , "Query String = %s.\n" , str_query.c_str() );
	wstring str_Query_W(str_query.length(), L' ');
	copy(str_query.begin(), str_query.end(), str_Query_W.begin());
	BSTR query = SysAllocString(str_Query_W.c_str());
    // Create querying object
    IWbemClassObject *pObj = NULL;
    IWbemServices *pServ = NULL;
    IEnumWbemClassObject *pEnum = NULL;
    HRESULT hRet;

    if(FAILED(GetWbemService(&pServ))) 
	{
	    DebugPrintf(SV_LOG_ERROR,"Failed to initialise GetWbemService.\n");
		CoUninitialize();
		m_bInit = false;
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n", FUNCTION_NAME);
		return S_FALSE; 
	}
    // Issue the query.
    hRet = pServ->ExecQuery(
			language,
			query,
			WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,         // Flags
			0,                              // Context
			&pEnum
			);

    if (FAILED(hRet))
    {
        DebugPrintf(SV_LOG_ERROR,"Query for NIC's Device ID failed.Error Code %0X\n",hex);
        pServ->Release();
        CoUninitialize();
		m_bInit = false;
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n", FUNCTION_NAME) ;
        return S_FALSE;                // Program has failed.
    }
    SysFreeString(language);
    SysFreeString(query);
        
    // Get the disk drive information
    ULONG uTotal = 0;

    // Retrieve the objects in the result set.
    while (pEnum)
    {
        ULONG uReturned = 0;
        hRet = pEnum->Next
				(
					WBEM_INFINITE,       // Time out
					1,                  // One object
					&pObj,
					&uReturned
				);
		if (uReturned == 0)
			break;
		VARIANT val;
		uTotal += uReturned;
		
		hRet = pObj->Get(L"NetConnectionID", 0, &val, 0, 0); // Get the value of the Name property
		strNetworkName = string(W2A(val.bstrVal));
		VariantClear(&val);

		hRet = pObj->Get(L"NetConnectionStatus", 0, &val, 0, 0);        
		nNwStatus = val.uintVal;
        VariantClear(&val);

		mapOfInterfaceStatus.insert(make_pair(strNetworkName, nNwStatus));

		pObj->Release();    // Release objects not owned.		
    }
    // All done. Free the Resources
    pEnum->Release();
    pServ->Release();
	CoUninitialize();
	m_bInit = false;
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n", FUNCTION_NAME) ;
	return S_OK; 
}

HRESULT ChangeVmConfig::WMIDiskPartitionQuery(map<string, string>& mapOfDiskNumberToType)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME) ;

	USES_CONVERSION;

	BSTR language = SysAllocString(L"WQL");
	BSTR query = SysAllocString(L"SELECT * FROM Win32_DiskPartition where SIZE>0");
    // Create querying object
    IWbemClassObject *pObj = NULL;
    IWbemServices *pServ = NULL;
    IEnumWbemClassObject *pEnum = NULL;
    HRESULT hRet;

    if(FAILED(GetWbemService(&pServ))) 
	{
	    DebugPrintf(SV_LOG_ERROR,"Failed to initialise GetWbemService.\n");
		CoUninitialize();
		m_bInit = false;
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n", FUNCTION_NAME);
		return S_FALSE; 
	}
    // Issue the query.
    hRet = pServ->ExecQuery(
			language,
			query,
			WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,         // Flags
			0,                              // Context
			&pEnum
			);

    if (FAILED(hRet))
    {
        DebugPrintf(SV_LOG_ERROR,"Query for Win32_DiskPartition failed.Error Code %0X\n",hex);
        pServ->Release();
        CoUninitialize();
		m_bInit = false;
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n", FUNCTION_NAME) ;
        return S_FALSE;                // Program has failed.
    }
    SysFreeString(language);
    SysFreeString(query);
        
    // Get the disk drive information
    ULONG uTotal = 0;

    // Retrieve the objects in the result set.
    while (pEnum)
    {
        ULONG uReturned = 0;
        hRet = pEnum->Next
				(
					WBEM_INFINITE,       // Time out
					1,                  // One object
					&pObj,
					&uReturned
				);
		if (uReturned == 0)
			break;
		VARIANT val;
		uTotal += uReturned;
		
		hRet = pObj->Get(L"DiskIndex", 0, &val, 0, 0); // Get the value of the disk number
		string strDiskNum = string(W2A(val.bstrVal));
		VariantClear(&val);

		hRet = pObj->Get(L"Type", 0, &val, 0, 0);        
		string strPartitionStyle = string(W2A(val.bstrVal));
        VariantClear(&val);

		mapOfDiskNumberToType.insert(make_pair(strDiskNum, strPartitionStyle));

		pObj->Release();    // Release objects not owned.		
    }
    // All done. Free the Resources
    pEnum->Release();
    pServ->Release();
	CoUninitialize();
	m_bInit = false;
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n", FUNCTION_NAME) ;
	return S_OK;
}


HRESULT ChangeVmConfig::WMINicQueryForMac(string strMacAddress, string& strNetworkName)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME) ;
	USES_CONVERSION;

	BSTR language = SysAllocString(L"WQL");

	string sqlQuery = string("SELECT * FROM Win32_NetworkAdapter WHERE MACAddress = '") + strMacAddress + string("'");

	DebugPrintf(SV_LOG_INFO,"Query to execute : %s\n", sqlQuery.c_str());

	wstring sqlQuery_W(sqlQuery.length(), L' ');
	copy(sqlQuery.begin(), sqlQuery.end(), sqlQuery_W.begin());

	BSTR query = SysAllocString(sqlQuery_W.c_str());
    // Create querying object
    IWbemClassObject *pObj = NULL;
    IWbemServices *pServ = NULL;
    IEnumWbemClassObject *pEnum = NULL;
    HRESULT hRet;

    if(FAILED(GetWbemService(&pServ))) 
	{
	    DebugPrintf(SV_LOG_ERROR,"Failed to initialise GetWbemService.\n");
		CoUninitialize();
		m_bInit = false;
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n", FUNCTION_NAME);
		return S_FALSE; 
	}
    // Issue the query.
    hRet = pServ->ExecQuery(
			language,
			query,
			WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,         // Flags
			0,                              // Context
			&pEnum
			);

    if (FAILED(hRet))
    {
        DebugPrintf(SV_LOG_ERROR,"Query for NIC's Device ID failed.Error Code %0X\n",hex);
        pServ->Release();
        CoUninitialize();
		m_bInit = false;
        DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n", FUNCTION_NAME) ;
        return S_FALSE;                // Program has failed.
    }
    SysFreeString(language);
    SysFreeString(query);
        
    // Get the disk drive information
    ULONG uTotal = 0;

    // Retrieve the objects in the result set.
    while (pEnum)
    {
        ULONG uReturned = 0;
        hRet = pEnum->Next
				(
					WBEM_INFINITE,       // Time out
					1,                  // One object
					&pObj,
					&uReturned
				);
		if (uReturned == 0)
			break;
		VARIANT val;
		uTotal += uReturned;
		
		hRet = pObj->Get(L"NetConnectionID", 0, &val, 0, 0); // Get the value of the Name property
		strNetworkName = string(W2A(val.bstrVal));

		VariantClear(&val);        	
		pObj->Release();    // Release objects not owned.

		DebugPrintf(SV_LOG_INFO,"\nNIC Name is - %s\n", strNetworkName.c_str());
    }
    // All done. Free the Resources
    pEnum->Release();
    pServ->Release();
	CoUninitialize();
	m_bInit = false;
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n", FUNCTION_NAME) ;
	return S_OK;      
}

HRESULT ChangeVmConfig::InitCOM()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hres;
	if(!m_bInit)
	{
		hres = CoInitializeEx(0, COINIT_MULTITHREADED); 
		if (FAILED(hres))
		{
		   DebugPrintf(SV_LOG_ERROR,"Failed to initialize COM library. Error Code %0X\n",hex);
		   return S_FALSE;                  // Program has failed.
		}
		m_bInit = true;
	}
	hres = CoInitializeSecurity(
							NULL, 
							-1,                          // COM authentication
							NULL,                        // Authentication services
							NULL,                        // Reserved
							RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
							RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
							NULL,                        // Authentication info
							EOAC_NONE,                   // Additional capabilities 
							NULL                         // Reserved
							);

	// Check if the securiy has already been initialized by someone in the same process.
	if (hres == RPC_E_TOO_LATE)
	{
		hres = S_OK;
	}

    if (FAILED(hres))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to initialize security  library. Error Code %0X\n",hres);
        CoUninitialize();
		m_bInit = false;
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return S_FALSE;                    // Program has failed.
    }
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return S_OK;
}

HRESULT ChangeVmConfig::GetWbemService(IWbemServices **pWbemService)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    IWbemServices *pWbemServ = NULL;
    IWbemLocator *pLoc = 0;
    HRESULT hr;

    hr = CoCreateInstance(CLSID_WbemLocator, 0,CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) &pLoc);

    if (FAILED(hr))
    {
	    DebugPrintf(SV_LOG_ERROR,"Failed to create IWbemLocator object. Error Code %0X\n",hr);
        CoUninitialize();
		m_bInit = false;
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return hr;     // Program has failed.
    }

    // Connect to the root\default namespace with the current user.
    hr = pLoc->ConnectServer(
            BSTR(L"ROOT\\cimv2"), 
            NULL, NULL, 0, NULL, 0, 0, pWbemService);

    if (FAILED(hr))
    {
		DebugPrintf(SV_LOG_ERROR,"Could not connect to server. Error Code %0X\n",hr);
        pLoc->Release();
        CoUninitialize();
		m_bInit = false;
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return hr;      // Program has failed.
    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return S_OK;    
}


//********************************************************************************
//  Get Interface names 
//  First try using WMI call
//  Then try using the API call GetAdaptersAddresses
//********************************************************************************
bool ChangeVmConfig::GetInterfaceFriendlyNames(InterfacesVec_t & InterfaceNames)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    
    bool rv = true;
 
    DebugPrintf(SV_LOG_ERROR,"Trying to get InterfaceNames :\n");
    for(int i = 0; i < 20; i++)
    {
        if(i != 0)
        {
            DebugPrintf(SV_LOG_ERROR,"Trying again.\n");
            Sleep(30000);             
        }

        InterfaceNames.clear();
        if(false == GetInterfaceFriendlyNamesUsingApi(InterfaceNames))
        {
            DebugPrintf(SV_LOG_ERROR,"GetInterfaceFriendlyNamesUsingApi failed.\n");
        }
        else if(! InterfaceNames.empty())
        {
            rv = true;
            break;
        }
        
        DebugPrintf(SV_LOG_ERROR,"%d. InterfaceNames vector is empty.\n", i);
        rv = false;
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}

//********************************************************************************
//  Get Interface names using the API call GetAdaptersAddresses
//********************************************************************************
bool ChangeVmConfig::GetInterfaceFriendlyNamesUsingApi(InterfacesVec_t & InterfaceNames)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);    

    USES_CONVERSION;

    /* Declare and initialize variables */
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    unsigned int i = 0;

    // Set the flags to pass to GetAdaptersAddresses
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;

    //ULONG family = AF_UNSPEC; // default to unspecified address family (both)
    ULONG family = AF_INET; // keeping ipv4 address family

    LPVOID lpMsgBuf = NULL;

    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    ULONG outBufLen = 0;

    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;

    outBufLen = sizeof (IP_ADAPTER_ADDRESSES);
    pAddresses = (IP_ADAPTER_ADDRESSES *) MALLOC(outBufLen);

    // Make an initial call to GetAdaptersAddresses to get the 
    // size needed into the outBufLen variable
    if (ERROR_BUFFER_OVERFLOW == GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen))
    {
        FREE(pAddresses);
        pAddresses = (IP_ADAPTER_ADDRESSES *) MALLOC(outBufLen);
    }

    if (pAddresses == NULL) {
        DebugPrintf(SV_LOG_ERROR,"Memory allocation failed for IP_ADAPTER_ADDRESSES struct\n");
	    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
        return false;
    }
    // Make a second call to GetAdapters Addresses to get the
    // actual data we want
    DebugPrintf(SV_LOG_ERROR,"Memory allocated for GetAdapterAddresses = %d bytes\n", outBufLen);
    DebugPrintf(SV_LOG_ERROR,"Calling GetAdaptersAddresses function with family = AF_INET\n");
    
    dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

    if (dwRetVal == NO_ERROR)
    {
        // If successful, output some information from the data we received
        pCurrAddresses = pAddresses;
        while (pCurrAddresses) 
        {
            DebugPrintf(SV_LOG_ERROR,"\tIfType: %ld\n", pCurrAddresses->IfType);
            DebugPrintf(SV_LOG_ERROR,"\tFriendly name: %wS\n", pCurrAddresses->FriendlyName);
            if (IF_TYPE_ETHERNET_CSMACD == pCurrAddresses->IfType)
            {
                DebugPrintf(SV_LOG_ERROR,"\tAdding this to vector(after conversion W2A) : %s\n", W2A(pCurrAddresses->FriendlyName));
                InterfaceNames.push_back(std::string(W2A(pCurrAddresses->FriendlyName)));
            }
            pCurrAddresses = pCurrAddresses->Next;
        }
    } 
    else
    {
        DebugPrintf(SV_LOG_ERROR,"Call to GetAdaptersAddresses failed with error: %d\n", dwRetVal);
        if (dwRetVal == ERROR_NO_DATA)
            DebugPrintf(SV_LOG_ERROR,"\tNo addresses were found for the requested parameters\n");
        else 
        {
            if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dwRetVal, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),   // Default language
                              (LPTSTR) & lpMsgBuf, 0, NULL)) 
            {
                DebugPrintf(SV_LOG_ERROR,"\tError: %s", lpMsgBuf);
                LocalFree(lpMsgBuf);
                FREE(pAddresses);
	            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
                return false;
            }
        }
    }
    FREE(pAddresses);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return true;
}

// --------------------------------------------------------------------------
// Fetch the system drive (say C:\ - it will be in this format).
// --------------------------------------------------------------------------
bool ChangeVmConfig::GetSystemVolume(string & strSysVol)
{  
    char szSysPath[MAX_PATH];
    if (0 == GetSystemDirectory(szSysPath, MAX_PATH))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to fetch the System Directory. Error - [%lu]\n",GetLastError());
		return false;
	}

    char szSysDrive[MAX_PATH];
    if (0 == GetVolumePathName(szSysPath,szSysDrive,MAX_PATH))
    {
        DebugPrintf(SV_LOG_ERROR,"System Directory - %s\n",szSysPath);
        DebugPrintf(SV_LOG_ERROR,"Failed to fetch the System Volume from the System Directory. Error - [%lu]\n",GetLastError());
		return false;
    }    
    strSysVol = string(szSysDrive);
    DebugPrintf(SV_LOG_DEBUG,"System Volume - %s\n",strSysVol.c_str());

    return true;
}

//Intsalls VmWare tools in Recovered virtual machines during booting (installs only in case of P2V)
//Algorithm:
//		Gets the CDRom drive information which contains the "VM Ware Tools".
//		After getting the CDRom Drive processes the below command to install the VM Ware tools:
//		"<CD-Rom Drive>" setup.exe /S /v /qn REBOOT=R
//On Succeesful installtion returns true else false.
bool ChangeVmConfig::InstallVmWareTools()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = false;
	string strCdRomDrive = "";   //Will contain the VmWare Tool  CD-ROM drive path
	if(true == CheckServiceExist("VMTools"))
	{
		DebugPrintf(SV_LOG_INFO,"VM Ware Tools Service already exist, so skipping installation of vmwaretools.\n");
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return true;
	}
	
	if(false == CdRomVolumeEnumeration(strCdRomDrive))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get the Cd-Rom Drive in all tries. \n");
	}
	else
	{
		if(!strCdRomDrive.empty())
		{
			string strVmWareInstallCmd = strCdRomDrive + string("setup.exe");
			string strParameters = string(" /S /v /qn REBOOT=R") ;
			string strInstallResultFile = m_VxInstallPath + string("\\Failover") + string("\\data\\") + string("VmWareInstallOutPut.txt");
			DebugPrintf(SV_LOG_INFO,"Command to install VmWare Tools is : %s%s\n", strVmWareInstallCmd.c_str(), strParameters.c_str());

			int i = 0;
			for(; i < 3 ; i++)
			{
				//if(false == ExecuteCmdWithOutputToFileWithPermModes(strVmWareInstallCmd, strInstallResultFile, O_APPEND | O_RDWR | O_CREAT))
				try
				{
					if(false == ExecuteProcess(strVmWareInstallCmd, strParameters))
					{
						DebugPrintf(SV_LOG_ERROR,"Failed to execute the command - %s%s\n", strVmWareInstallCmd.c_str(), strParameters.c_str());
						DebugPrintf(SV_LOG_ERROR,"Retring Installtion of VmWare Tools Again...\n");
						continue;
					}
					else
					{
						if(i < 3)
						{
							if(false == CheckServiceExist("VMTools"))
							{
								DebugPrintf(SV_LOG_ERROR,"Failed to find the service \"VMTools\"...Will Check the service again after 1 min.\n");
								Sleep(60000); //sleeping one minute as the vmware tool installation will take some time.
								if(false == CheckServiceExist("VMTools"))
								{
									DebugPrintf(SV_LOG_ERROR,"Failed to find the service \"VMTools\"...Will retry once again VmWare Tools installation\n");
									continue;
								}
							}
							bResult = true;
							break;
						}
					}
				}
				catch(...)
				{
					DebugPrintf(SV_LOG_ERROR,"Exception : Failed to execute the command - %s%s\n", strVmWareInstallCmd.c_str(), strParameters.c_str());
					DebugPrintf(SV_LOG_ERROR,"Retring Installtion of VmWare Tools Again...\n");
					continue;
				}
			}
			if(i == 3)
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to install the VmWare tools in all tries...\n" );
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the VmWare Tools CD Rom Drive...\n" );
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;	
}

//Wiil get the VmWare tool contained drive from CDROM and wiil use this for installing the VM Ware tools
//Algorithm: Enumerate all the volumes of that system and find out the CDRom drive in those volumes.
//		 If CDRom Drive is not found in the first iteration then sleeps for 15 sec. and will retry once again enumerating all the volumes.
// output: strCdRomDrive --> will contain the the CD Rom drive letter after successful completion of the Process.
// On Successful Enumerating the CD-Rom Drive returns True else False.
bool ChangeVmConfig::CdRomVolumeEnumeration(string& strCdRomDrive)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	strCdRomDrive.clear();
	bool bResult = true;
	TCHAR buf[BUFSIZE];      // buffer for unique volume identifiers
	HANDLE volumeHandle;     // handle for the volume scan
	BOOL bFlag;              // generic results flag

	int counter = 1;
	do
	{
		// Open a scan for volumes.
		volumeHandle = FindFirstVolume(buf, BUFSIZE);

		if (volumeHandle == INVALID_HANDLE_VALUE)
		{
			DebugPrintf(SV_LOG_ERROR,"No volume found in System.\n");
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}

		// We have a volume,now process it.
		bFlag = FetchAllCdRomVolumes(volumeHandle, buf, BUFSIZE, strCdRomDrive);

		// Do while we have volumes to process.
		while (bFlag) 
		{
			bFlag = FetchAllCdRomVolumes(volumeHandle, buf, BUFSIZE, strCdRomDrive);
		}
		if(strCdRomDrive.empty())
		{
			DebugPrintf(SV_LOG_INFO,"Count: %d --> Did not get the VmWare Tools Drive, will retry after 15 secs \n", counter);
			Sleep(15000);	//If didn't get the VMWARE Tools CD RomDrive then will try once again after 15 secs.
			counter++; 
		}
		else
		{
			break;
		}
	}while(counter <= 20);

	// Close out the volume scan.
	bFlag = FindVolumeClose(volumeHandle);  // handle to be closed

	return bResult;
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

//Feteched the CD Rom Drive and process it to get the Drive letter from the CR Rom Volume GUID.
//Algorithm:
//		volumeHandle --> input current volume handle to process.
//		szBuffer --> input contains the Volume GUID.
//      iBufSize --> input size of output buffer
//		strCdRomDrive --> output CD Rom drive letter.
//	By taking the input Volume GUId, finds out the Drive type. If the drive type is CD Rom then proceed further.
//	if its not CD Rom drive then getting the handle for next Volume and exiting.
//	if got the CD Rom drive then processing it to get the Drive letter.
//	On Successfully getting the Volume returns True else returns False.
BOOL ChangeVmConfig::FetchAllCdRomVolumes(HANDLE volumeHandle, TCHAR *szBuffer, int iBufSize, string& strCdRomDrive)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	BOOL bFlag;           // generic results flag for return

	if(DRIVE_CDROM != GetDriveType(szBuffer))
	{
		bFlag = FindNextVolume(
			volumeHandle,   // handle to scan being conducted
			szBuffer,       // pointer to output
			iBufSize        // size of output buffer
			);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return (bFlag);
	}
	char szVolumePath[MAX_PATH+1] = {0};
	DWORD dwVolPathSize = 0;
	string strVolumeGuid = string(szBuffer);
	char szVolumeLabel[MAX_PATH+1] = {0};
	char szVolumeFileSystem[MAX_PATH+1] = {0};

	if(GetVolumePathNamesForVolumeName (strVolumeGuid.c_str(),szVolumePath,MAX_PATH, &dwVolPathSize))
	{
		if(GetVolumeInformation( szVolumePath, szVolumeLabel, MAX_PATH+1, NULL, NULL, NULL, szVolumeFileSystem, MAX_PATH+1))
		{
			DebugPrintf(SV_LOG_INFO, "Volume Label: %s\n", szVolumeLabel );
		}
		else
		{
			cout << "Failed to get Volume Name Label with ErrorCode : [" << GetLastError() << "].\n";
		}
		if(strcmp("VMware Tools", szVolumeLabel) == 0)  //Checking the VMWare tools drive only.(this check is for more then one CDROM drives exist)
		{
			strCdRomDrive = string(szVolumePath);
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"GetVolumePathNamesForVolumeName Failed with ErrorCode : [%lu].\n",GetLastError());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return FALSE;
	}

	bFlag = FindNextVolume(
		volumeHandle,    // handle to scan being conducted
		szBuffer,     // pointer to output
		iBufSize // size of output buffer
		);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return (bFlag); 
}

bool ChangeVmConfig::CheckServiceExist(const std::string& svcName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    SC_HANDLE schService;        
    bool bResult = true;

    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (schSCManager) 
    {
        schService = OpenService(schSCManager, svcName.c_str(), SERVICE_ALL_ACCESS); 
        if(NULL == schService) 
        {
			if((ERROR_SERVICE_DOES_NOT_EXIST == GetLastError()) || (ERROR_INVALID_NAME == GetLastError()))
			{
				DebugPrintf(SV_LOG_ERROR,"VM Ware Tools Service is not Exist in the system. Will try to install vmware tools...\n");
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to check the VMWare Tools Service existance...Error %d\n", GetLastError());
			}			
			bResult = false;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"VM Ware Tools Service Exist in the system\n");			
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME) ;
	return bResult;
}


/****************************************************************************************
// wszName is the name of the connection as appears in Network Connections folder
// set bEnable to true to enable and to false to disable
****************************************************************************************/
bool ChangeVmConfig::EnableConnection(const std::string& strNwAdpName, bool bEnable, const std::string& strResultFile)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", FUNCTION_NAME) ;
    bool result = true;

	std::string NetshCmd = std::string("netsh interface set interface ") + strNwAdpName;
	if (bEnable)
		NetshCmd = NetshCmd + " enable";
	else
		NetshCmd = NetshCmd + " disable";

	if (false == ExecuteNetshCmd(NetshCmd, strResultFile))
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to run command : [%s]\n", NetshCmd.c_str());
		result = false;
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", FUNCTION_NAME) ;
	return result; 
} 

BOOL ChangeVmConfig::EnumerateAllVolumes(std::set<std::string>& setVolume)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	TCHAR buf[BUFSIZE];      // buffer for unique volume identifiers
	HANDLE volumeHandle;     // handle for the volume scan
	BOOL bFlag = 0;              // generic results flag

	int i = 0;
	for(; i < 3; i++)
	{
		volumeHandle = FindFirstVolume (buf, BUFSIZE );

		if (volumeHandle == INVALID_HANDLE_VALUE)
		{
			DebugPrintf(SV_LOG_ERROR,"No volume found in System...Retrying again(%d)\n", i);
			continue;
		}

		// We have a volume,now process it.
		bFlag = FetchAllVolumes(volumeHandle, buf, BUFSIZE, setVolume);

		// Do while we have volumes to process.
		while (bFlag) 
		{
			bFlag = FetchAllVolumes(volumeHandle, buf, BUFSIZE, setVolume);
		}

		// Close out the volume scan.
		bFlag = FindVolumeClose(
				volumeHandle  // handle to be closed
			   );
	}
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	return bFlag;
}


BOOL ChangeVmConfig::FetchAllVolumes(HANDLE volumeHandle, TCHAR *szBuffer, int iBufSize, std::set<std::string>& stVolume)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	BOOL bFlag;           // generic results flag for return

	if(DRIVE_FIXED != GetDriveType(szBuffer))
	{
		bFlag = FindNextVolume(
					volumeHandle,    // handle to scan being conducted
					szBuffer,     // pointer to output
					iBufSize // size of output buffer
		);
		return (bFlag);
	}
	stVolume.insert(string(szBuffer));

	bFlag = FindNextVolume(
				 volumeHandle,    // handle to scan being conducted
				 szBuffer,     // pointer to output
				 iBufSize // size of output buffer
			   );
   
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return (bFlag); 
}

bool ChangeVmConfig::GetSizeOfVolume(const string& strVolGuid, SV_ULONGLONG& volSize)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	HANDLE	hVolume ;
	
	// Remove the trailing "\" from volume GUID
    std::string strTemp = strVolGuid;        
    std::string::size_type pos = strTemp.find_last_of("\\");
    if(pos != std::string::npos)
    {
        strTemp.erase(pos);
    }

    hVolume = INVALID_HANDLE_VALUE ;
	hVolume = CreateFile(strTemp.c_str(),GENERIC_READ, 
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT  , NULL 
                         );
	if( hVolume == INVALID_HANDLE_VALUE ) 
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open handle for volume-%s with Error Code - %d \n", strTemp.c_str(), GetLastError());
		bResult = false;
	}
	else
	{
		ULONG	bytesWritten;
		UCHAR	DiskExtentsBuffer[0x400];
		DWORD   returnBufferSize = sizeof(DiskExtentsBuffer);
		PVOLUME_DISK_EXTENTS DiskExtents = (PVOLUME_DISK_EXTENTS)DiskExtentsBuffer;
		if( DeviceIoControl(hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
							NULL, 0, DiskExtents, sizeof(DiskExtentsBuffer),
							&bytesWritten, NULL) ) 
		{            
			for( DWORD extent = 0; extent < DiskExtents->NumberOfDiskExtents; extent++ ) 
			{
				volSize    = (SV_ULONGLONG)DiskExtents->Extents[extent].ExtentLength.QuadPart;				
				DebugPrintf(SV_LOG_INFO,"VolumeLength    = %lld\n",volSize);
			}
		}
		else
		{
			DWORD error = GetLastError();
			DebugPrintf(SV_LOG_ERROR,"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed for volume-%s with Error code - %lu.\n", strTemp.c_str(), error);
			if(error == ERROR_MORE_DATA)
			{
				returnBufferSize = sizeof(VOLUME_DISK_EXTENTS) + ((DiskExtents->NumberOfDiskExtents - 1) * sizeof(DISK_EXTENT));
				DiskExtents = (PVOLUME_DISK_EXTENTS) new UCHAR[returnBufferSize];

				if( DeviceIoControl(hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
							NULL, 0, DiskExtents, returnBufferSize,
							&bytesWritten, NULL) ) 
				{
	    			
					for( DWORD extent = 0; extent < DiskExtents->NumberOfDiskExtents; extent++ ) 
					{
						volSize    = (SV_ULONGLONG)DiskExtents->Extents[extent].ExtentLength.QuadPart;				
						DebugPrintf(SV_LOG_INFO,"VolumeLength    = %lld\n",volSize);
					}
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS ioctl failed for Volume: %s with Error code : [%lu].\n",strVolGuid.c_str(), GetLastError());
					bResult = false;
					
				}
				delete [] DiskExtents ;
			}
			else
			{
				DebugPrintf(SV_LOG_INFO,"Failed to get the volume size\n");
				bResult = false;
			}
		}
		CloseHandle(hVolume);
		hVolume = INVALID_HANDLE_VALUE ;
	}

    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


BOOL ChangeVmConfig::MountVolume(const string& strVolGuid, string& strMountPoint)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	int MntCount = 1;
	
	//If volume is not mounted, then create a mount directory and mount it
	if (IsVolumeNotMounted(strVolGuid, strMountPoint)) 
	{	
		DebugPrintf(SV_LOG_DEBUG,"Found the volume as unmounted : %s \n",strVolGuid.c_str());
		string strSysVol;

		//Fetch the System Drive(first get sys dir and then extract drive letter from it)
		if(false == GetSystemVolume(strSysVol))
		{
			DebugPrintf(SV_LOG_DEBUG,"GetSystemVolume failed.\n");
			strSysVol = string("c:");
		}

		//Create the Parent Mount directory
		string MountPath = strSysVol + "\\InMageEsx\\"; //end "\" is must
		if (FALSE == CreateDirectory(MountPath.c_str(),NULL))
		{
			if(GetLastError() != ERROR_ALREADY_EXISTS)// To Support Rerun scenario
			{
				DebugPrintf(SV_LOG_ERROR,"CreateDirectory Operation Failed with ErrorCode : %d \n",GetLastError());
                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return FALSE;
			}
		}
		
		//Create the Child mount directory 
		//  Say mount1 then check if it has reparse points then create mount2
		//  check again and continue this until a directory which got no reparse points
		char cMntCount[15];
		itoa(MntCount++,cMntCount,10); //last argument 10 since MntCount is in decimal form
		strMountPoint = MountPath + "mount" + string(cMntCount) + string("\\");
		while(FALSE == CreateDirectory(strMountPoint.c_str(),NULL))
		{
			if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
			{
				DebugPrintf(SV_LOG_ERROR,"Child Directory already exists. ");
				//Check whether the folder doesnot have any reparse points
				DWORD File_Attribute;
				File_Attribute = GetFileAttributes(strMountPoint.c_str());
				if (File_Attribute & FILE_ATTRIBUTE_REPARSE_POINT)
				{
					DebugPrintf(SV_LOG_ERROR,"Its in use. So creating another directory.\n");
					itoa(MntCount++,cMntCount,10);
					strMountPoint = MountPath + "mount" + string(cMntCount) + string("\\");
					continue;
				}
				else
				{
					DebugPrintf(SV_LOG_ERROR,"Considering it not as Error\n");
					break;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"CreateDirectory Operation Failed - ErrorCode : %d \n",GetLastError());
                DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				return FALSE;
			}
			
		}

		//Mount the Volume 
		if (FALSE == SetVolumeMountPoint(strMountPoint.c_str(),strVolGuid.c_str()))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to mount the Volume: %d \n",GetLastError());
            DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return FALSE;
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG,"Successfully mounted this Volume to : %s\n",strMountPoint.c_str());
		}			
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return TRUE;
}

void ChangeVmConfig::RemoveAndDeleteMountPoints(const string & strEfiMountPoint)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	if(false == DeleteVolumeMountPoint(strEfiMountPoint.c_str()))
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to unmount the volume with above mount point with Error : [%lu].\n",GetLastError());
        DebugPrintf(SV_LOG_INFO,"Please unmount the volume and delete the mount point manually.\n");
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"Unmounted the volume. Deleting the mount point now.\n");
        if(false == RemoveDirectory(strEfiMountPoint.c_str()))
            DebugPrintf(SV_LOG_ERROR,"Failed to delete the mount point with Error : [%lu].\n",GetLastError());
        else
            DebugPrintf(SV_LOG_DEBUG,"Deleted the mount point Successfully.\n");

    }
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


//Check whether the volume is mounted or not
BOOL ChangeVmConfig::IsVolumeNotMounted(const string& strVolumeGuid , string & strMountPoint)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	BOOL ret = TRUE;
	char szVolumePath[MAX_PATH+1] = {0};
	DWORD dwVolPathSize = 0;

	if (GetVolumePathNamesForVolumeName (strVolumeGuid.c_str(),szVolumePath,MAX_PATH, &dwVolPathSize))
	{
		if (string(szVolumePath).empty()) //if volume path name is empty, then it is not mounted.
		{			
			strMountPoint = string(szVolumePath);
			DebugPrintf(SV_LOG_DEBUG,"Found mount point : %s\n",strMountPoint.c_str());
		}
		else
		{            
			ret = FALSE;
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"GetVolumePathNamesForVolumeName Failed with ErrorCode : %d \n",GetLastError());
		ret = FALSE;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return ret;
}


bool ChangeVmConfig::GetDriverFile(std::string& SystemVol, std::string& SoftwareHiveName, bool& bx64, std::string& strExtractPath)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool ret = false;

	int w2k3SpVersion = 0;

	if(false == GetW2k3SpVesrion(SoftwareHiveName, w2k3SpVersion))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed on getting the service pack version from registry \n");
	}

	while(w2k3SpVersion > 0) 
	{
		std::string w2k3SpFile = string("sp") + boost::lexical_cast<std::string>(w2k3SpVersion) + string(".cab");
		ret = ExtractDriverFile(SystemVol, w2k3SpFile, bx64, "symmpi.sys", strExtractPath);
		if(ret && (true == checkIfFileExists(strExtractPath + string("\\symmpi.sys"))))
		{
			DebugPrintf(SV_LOG_INFO,"Successfully extracted the driver file from the cabinet file\n");
			break;
		}
		--w2k3SpVersion;
	}

	if(!ret)
	{
		ret = ExtractDriverFile(SystemVol, "driver.cab", bx64, "symmpi.sys", strExtractPath);
		if(true == checkIfFileExists(strExtractPath + string("\\symmpi.sys")))
		{
			DebugPrintf(SV_LOG_INFO,"Successfully extracted the driver file from the driver.cab file\n");
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to extract the driver file from the driver.cab file\n");
			ret = false;
		}
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return ret;
}

bool ChangeVmConfig::GetW2k3SpVesrion(std::string& SoftwareHiveName, int& w2k3SpVersion)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool ret = true;

	string strSpVer;
	do
    {
		HKEY hKey;
        std::string RegPathToOpen = SoftwareHiveName + std::string("\\Microsoft\\Windows NT\\CurrentVersion");

        long lRV = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegPathToOpen.c_str(), 0, KEY_READ, &hKey);
        if( lRV != ERROR_SUCCESS )
        {
			DebugPrintf(SV_LOG_ERROR,"RegOpenKeyEx failed for Path - [%s] and return code - [%lu].\n", RegPathToOpen.c_str(), lRV);
            ret = false; break;
        }

        if(false == RegGetValueOfTypeSzAndMultiSz(hKey, "CSDVersion", strSpVer))
        {
            DebugPrintf(SV_LOG_ERROR,"Failed to get value for CSDVersion at Path - [%s].\n", RegPathToOpen.c_str());
			ret = false; 
            w2k3SpVersion = 0;
        }
		else
			DebugPrintf(SV_LOG_ERROR,"Successfully got value for CSDVersion [%s] from registry path [%s]\n", strSpVer.c_str(), RegPathToOpen.c_str());

        closeRegKey(hKey);
    }while(0);

	for(size_t i = 0; i < strSpVer.length(); i++)
		strSpVer[i] = tolower(strSpVer[i]);
	if(strSpVer.find("service pack") != string::npos)
	{
		size_t idx = strSpVer.size() - 1;
        while(' ' == strSpVer[idx])
		{
			--idx;
        }
        while(' ' != strSpVer[idx])
		{
			--idx;
		} 
        ++idx;
        try 
		{
            w2k3SpVersion = boost::lexical_cast<int>(strSpVer.substr(idx));
        }
		catch (...) 
		{
            ret = false;
        }
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return ret;
}


bool ChangeVmConfig::ExtractDriverFile(std::string& strSystemVol, std::string strCabFile, bool& bx64, std::string strDriverFile, std::string& strExtractPath)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool ret = true;

	do
	{
		CabExtractInfo cabInfo;
        cabInfo.m_name = strCabFile;
		ret = GetCabFilePath(strSystemVol, strCabFile, bx64, cabInfo.m_path);
		if(!ret)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get the cabinet file : %s from the source windows directory \n", cabInfo.m_name.c_str());
			break;
		}
		DebugPrintf(SV_LOG_INFO,"Successfully to got the cabinet file : %s%s \n", cabInfo.m_path.c_str(),  cabInfo.m_name.c_str());

		/*if(m_strDatafolderPath.empty())
		{*/
		if(!m_bWinNT)
			cabInfo.m_destination = strSystemVol + "\\Windows\\system32\\drivers\\";
		else
			cabInfo.m_destination = strSystemVol + "\\WINNT\\system32\\drivers\\";
		/*}
		else
			cabInfo.m_destination = m_strDatafolderPath + "\\driverextract\\";*/

		if(FALSE == CreateDirectory(cabInfo.m_destination.c_str(),NULL))
		{
			if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
			{
				DebugPrintf(SV_LOG_INFO,"Directory already exists. Considering it not as an Error.\n");            				
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to create Directory - %s. ErrorCode : [%lu]. proceeding with %s\n",cabInfo.m_destination.c_str(),GetLastError(), m_strDatafolderPath.c_str());
				cabInfo.m_destination = m_strDatafolderPath;
			}
		}
		
		strExtractPath = cabInfo.m_destination;
        cabInfo.m_extractFiles.insert(strDriverFile);
		ret = ExtractCabFile(&cabInfo);
		if(!ret)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to extract the cabinet file : %s%s \n", cabInfo.m_path.c_str(), cabInfo.m_name.c_str());
			break;
		}
		DebugPrintf(SV_LOG_INFO,"Successfully to extracted the cabinet file : %s%s \n", cabInfo.m_path.c_str(), cabInfo.m_name.c_str());
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return ret;
}


bool ChangeVmConfig::GetCabFilePath(std::string& strSystemVol, std::string& strCabFile, bool& bx64, std::string& strCabFilePath)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool ret = true;

	do
	{
		std::string strCabDir, strCabFullPath;
		if(!m_bWinNT)
			strCabDir = strSystemVol + string("\\windows\\driver cache\\");
		else
			strCabDir = strSystemVol + string("\\WINNT\\driver cache\\");
		
		if (bx64)
		{
			strCabFilePath = strCabDir + string("amd64\\");
			strCabFullPath = strCabFilePath + strCabFile;
			if(true == checkIfFileExists(strCabFullPath))
			{
				DebugPrintf(SV_LOG_INFO,"Found the file : %s \n",strCabFullPath.c_str());
				break;
			}
			else
			{
				DebugPrintf(SV_LOG_WARNING,"Did not find the file : %s\n",strCabFullPath.c_str());
			}
		} 
		else
		{
			strCabFilePath = strCabDir + string("i386\\");
			strCabFullPath = strCabFilePath + strCabFile;
			if(true == checkIfFileExists(strCabFullPath))
			{
				DebugPrintf(SV_LOG_INFO,"Found the file : %s\n",strCabFullPath.c_str());
				break;
			}
			else
			{
				DebugPrintf(SV_LOG_WARNING,"Did not find the file : %s\n",strCabFullPath.c_str());
			}
		}
		
		strCabFilePath = strCabDir;
		strCabFullPath = strCabFilePath + strCabFile;
		if(true == checkIfFileExists(strCabFullPath))
		{
			DebugPrintf(SV_LOG_INFO,"Found the file : %s \n",strCabFullPath.c_str());
			break;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"Did not find the file : %s\n",strCabFullPath.c_str());
			ret = false;
		}
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return ret;
}

bool ChangeVmConfig::ExtractCabFile(CabExtractInfo* cabInfo)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool ret = true;
	HFDI   hfdi;
	ERF    erf;

	do
	{
		// set up context
		hfdi = FDICreate(memAlloc, memFree, fileOpen, fileRead, fileWrite, fileClose, fileSeek, cpuUNKNOWN, &erf);
		if (NULL == hfdi)
		{
			DebugPrintf(SV_LOG_ERROR, "FDICreate failed with error code %d\n", erf.erfOper);
			ret = false;
			break;
		}

		// extract files
		if (!FDICopy(hfdi, (LPSTR)(cabInfo->m_name.c_str()), (LPSTR)(cabInfo->m_path.c_str()), 0, fdiNotifyCallback, NULL, cabInfo))
		{
			DebugPrintf(SV_LOG_ERROR, "FDICopy failed with error code %d\n", erf.erfOper);
			ret = false;
		}
	}while(0);

	//Destory the FDI context
    if ( hfdi != NULL )
    {
        if ( FDIDestroy(hfdi) != TRUE )
        {
            DebugPrintf(SV_LOG_ERROR, "FDIDestroy failed with error code %d\n", erf.erfOper);
        }
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return ret;
}


bool ChangeVmConfig::ExtractW2k3Driver(string& strSysVol, string& strOsType)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool ret = true;

	bool bx64 = false;
	bool bSystemHiveLoaded = false;
	bool bSoftwareHiveLoaded = false;
	std::string SystemHiveName = std::string(VM_SYSTEM_HIVE_NAME);    
    std::string SoftwareHiveName = std::string(VM_SOFTWARE_HIVE_NAME);
	string strSrcPath, strTgtPath;

	if(strOsType.compare("64") == 0)
		bx64 = true;

	do
	{
		m_bWinNT = false;
		if(false == PreRegistryChanges(strSysVol, SystemHiveName, SoftwareHiveName, bSystemHiveLoaded, bSoftwareHiveLoaded))
		{
			DebugPrintf(SV_LOG_ERROR,"PreRegistryChanges() failed for the machine \n");
			ret = false;break;
		}

		if(!m_bWinNT)
			strTgtPath = strSysVol + std::string("\\windows\\Temp\\symmpi.sys");
		else
			strTgtPath = strSysVol + std::string("\\WINNT\\Temp\\symmpi.sys");

		if(false == GetDriverFile(strSysVol, SoftwareHiveName, bx64, strSrcPath)) //Gets the driver file in plan name folder
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to extarct the driver file to the target path %s\n", m_strDatafolderPath.c_str());
			ret = false;break;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"Successfully extarcted the driver file to the target path %s\n", m_strDatafolderPath.c_str());
		}			

		strSrcPath = strSrcPath + string("symmpi.sys");
		DebugPrintf(SV_LOG_INFO,"Source file Path = %s\n",strSrcPath.c_str());
		DebugPrintf(SV_LOG_INFO,"Destination Path = %s\n",strTgtPath.c_str());

		if(0 == CopyFile(strSrcPath.c_str(),strTgtPath.c_str(), FALSE))
		//if(!CopyFileUsingCmd(strSrcPath, strTgtPath))
		{
			DebugPrintf(SV_LOG_ERROR,"Driver File copy failed with error - %lu\n",GetLastError()); 
			ret = false;
		}
		else
			DebugPrintf(SV_LOG_INFO,"Driver File %s copied successfully.\n", strTgtPath.c_str());
	}while(0);

	if(false == PostRegistryChanges(strSysVol, SystemHiveName, SoftwareHiveName, bSystemHiveLoaded, bSoftwareHiveLoaded))
    {
        DebugPrintf(SV_LOG_ERROR,"PostRegistryChanges() Failed.\n");
        ret = false;
    } 

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return ret;
}


bool ChangeVmConfig::CopyFileUsingCmd(string& strSrcPath, string& strTgtPath)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool ret = true;

	string strCmd = string("copy \"") + strSrcPath + string("\" \"") + strTgtPath + ("\" /Y");
	string strBatchFile = m_VxInstallPath + string("\\Failover") + string("\\data\\") + string("copysymmpi.bat");
	ofstream outFile(strBatchFile.c_str());
	outFile << strCmd << endl;
	outFile.close();
	string strResultFile = m_VxInstallPath + string("\\Failover") + string("\\data\\") + string("copyresultfile.txt");

	if(checkIfFileExists(strResultFile))
	{
		if(-1 == remove(strResultFile.c_str()))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to delete the file - %s\n",strResultFile.c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_INFO,"SuccessFully deleted stale file - %s\n",strResultFile.c_str());			
		}
	}

	if(false == ExecuteCmdWithOutputToFileWithPermModes(strBatchFile, strResultFile, O_APPEND | O_RDWR | O_CREAT))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to execute the command - %s\n", strCmd.c_str());
		DebugPrintf(SV_LOG_ERROR,"Failed to copy the file from %s to %s\n",strSrcPath.c_str(), strTgtPath.c_str());
		ret = false;
	}
	else
		DebugPrintf(SV_LOG_ERROR,"Successfully copied the file from %s to %s\n",strSrcPath.c_str(), strTgtPath.c_str());

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return ret;
}


bool ChangeVmConfig::FlushFileUsingWindowsAPI(string& strFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bResult = true;
	HANDLE	hHandle;
	BOOL bRet;

	DebugPrintf(SV_LOG_INFO,"File to Flush : %s\n",strFile.c_str());

	hHandle = CreateFile(strFile.c_str(),GENERIC_READ | GENERIC_WRITE, 
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
	if( hHandle == INVALID_HANDLE_VALUE ) 
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to open handle for %s with Error Code - %d \n", strFile.c_str(), GetLastError());
		bResult = false;
	}
	else
	{
		bRet = FlushFileBuffers(hHandle);
		CloseHandle(hHandle);

		if(bRet)
			DebugPrintf(SV_LOG_INFO,"Successfully flushed file - %s\n", strFile.c_str());
		else
		{
			DebugPrintf(SV_LOG_WARNING,"Failed to flush the file - %s with Error Code - %d \n", strFile.c_str(), GetLastError());
			bResult = false;
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bResult;
}


/******************************************************************************************
This function will read the specified MBR file and display the disk and Volume information
Also stroes the MBR/GPT/LDM data in files with respect to disks.
*******************************************************************************************/
int ChangeVmConfig::GetDiskAndMbrDetails(const string& strMbrFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	int iRetValue = 0;

	DisksInfoMap_t srcMapOfDiskNamesToDiskInfo;
	DLM_ERROR_CODE RetVal = ReadDiskInfoMapFromFile(strMbrFile.c_str(), srcMapOfDiskNamesToDiskInfo);
	if(DLM_ERR_SUCCESS != RetVal)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to collect the Source disks info map from the BLOB file(%s). Error Code - %u\n", strMbrFile.c_str(), RetVal);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return 1;
	}

	DisksInfoMapIter_t diskIter;

	for(diskIter = srcMapOfDiskNamesToDiskInfo.begin(); diskIter != srcMapOfDiskNamesToDiskInfo.end(); diskIter++)
	{
		DISK_INFO diskInfo = diskIter->second;
		DebugPrintf(SV_LOG_INFO, "Disk Name = %s\n", diskIter->first.c_str());

		DebugPrintf(SV_LOG_INFO, "Name            = %s\n", diskInfo.DiskInfoSub.Name);
        DebugPrintf(SV_LOG_INFO, "BytesPerSector  = %llu\n", diskInfo.DiskInfoSub.BytesPerSector);
        DebugPrintf(SV_LOG_INFO, "EbrCount        = %llu\n", diskInfo.DiskInfoSub.EbrCount);
        DebugPrintf(SV_LOG_INFO, "Flag            = %llu\n", diskInfo.DiskInfoSub.Flag);
        DebugPrintf(SV_LOG_INFO, "FormatType      = %u\n", diskInfo.DiskInfoSub.FormatType);
        DebugPrintf(SV_LOG_INFO, "ScsiId          = %u:%u:%u:%u\n", diskInfo.DiskInfoSub.ScsiId.Host, diskInfo.DiskInfoSub.ScsiId.Channel, diskInfo.DiskInfoSub.ScsiId.Target, diskInfo.DiskInfoSub.ScsiId.Lun);
        DebugPrintf(SV_LOG_INFO, "Size            = %lld\n", diskInfo.DiskInfoSub.Size);
        DebugPrintf(SV_LOG_INFO, "Type            = %u\n", diskInfo.DiskInfoSub.Type);
        DebugPrintf(SV_LOG_INFO, "VolumeCount     = %llu\n", diskInfo.DiskInfoSub.VolumeCount);
		DebugPrintf(SV_LOG_INFO, "Disk Device Id  = %s\n", diskInfo.DiskDeviceId);
		if(diskInfo.DiskInfoSub.FormatType == MBR)
		{
			DebugPrintf(SV_LOG_INFO, "Disk Signature  = %s (Signature is in decimal format, convert to hexadecimal to get the signature)\n", diskInfo.DiskSignature);
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Disk Signature  = %s\n", diskInfo.DiskSignature);
		}

		if(!diskInfo.vecPartitionFilePath.empty())
		{
			std::vector<PARTITION_FILE>::iterator iterVecP = diskInfo.vecPartitionFilePath.begin();
			for(; iterVecP != diskInfo.vecPartitionFilePath.end(); iterVecP++)
			{
				DebugPrintf(SV_LOG_INFO, "PartitionFileName     = %s\n", iterVecP->Name);
			}
		}

		if(RAWDISK != diskInfo.DiskInfoSub.FormatType)
		{
            //read vol info of all  volumes
			VolumesInfoVecIter_t volIter= diskInfo.VolumesInfo.begin();
			for(; volIter != diskInfo.VolumesInfo.end(); volIter++)
			{                   
				DebugPrintf(SV_LOG_INFO, "\tVolumeName      = %s\n",volIter->VolumeName);
				DebugPrintf(SV_LOG_INFO, "\tVolumeGuid      = %s\n",volIter->VolumeGuid);
				DebugPrintf(SV_LOG_INFO, "\tVolumeLength    = %lld\n",volIter->VolumeLength);
				DebugPrintf(SV_LOG_INFO, "\tStartingOffset  = %lld\n",volIter->StartingOffset);
				DebugPrintf(SV_LOG_INFO, "\tEndingOffset    = %lld\n",volIter->EndingOffset);
			}
		}
		if(MBR == diskInfo.DiskInfoSub.FormatType && BASIC == diskInfo.DiskInfoSub.Type)
		{
			string strMbrData = string("C:\\") + diskIter->first + string("_mbr_basic");
			FILE *fp;
			fp = fopen(strMbrData.c_str(),"wb");
			if(!fp)
			{
				 DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s. unable to write the MBR for basic disk %s\n",strMbrData.c_str(), diskIter->first.c_str());
			}
			else
			{
				fwrite(diskInfo.MbrSector, sizeof(SV_UCHAR)*MBR_BOOT_SECTOR_LENGTH, 1, fp);
				fclose(fp);
			}
			if(!diskInfo.EbrSectors.empty())
			{
				vector<EBR_SECTOR>::iterator iterVec = diskInfo.EbrSectors.begin();
				for(int i = 0; iterVec != diskInfo.EbrSectors.end(); iterVec++, i++)
				{
					stringstream ss;
					ss << i;
					strMbrData = string("C:\\") + diskIter->first + "_" + ss.str() + "_ebr";
					fp = fopen(strMbrData.c_str(),"wb");
					if(!fp)
					{
						 DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s. unable to write the MBR for basic disk %s\n",strMbrData.c_str(), diskIter->first.c_str());
					}
					else
					{
						fwrite(iterVec->EbrSector, sizeof(SV_UCHAR)*MBR_BOOT_SECTOR_LENGTH, 1, fp);
						fclose(fp);
					}
				}
			}
		}
		else if(MBR == diskInfo.DiskInfoSub.FormatType && DYNAMIC == diskInfo.DiskInfoSub.Type)
		{
			string strMbrData = string("C:\\") + diskIter->first + string("_mbr_dynamic");
			FILE *fp;
			fp = fopen(strMbrData.c_str(),"wb");
			if(!fp)
			{
				 DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s.\n",strMbrData.c_str());
				 DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				 return false;
			}
			else
			{
				fwrite(diskInfo.MbrDynamicSector, sizeof(SV_UCHAR)*MBR_DYNAMIC_BOOT_SECTOR_LENGTH, 1, fp);
				fclose(fp);
			}

			strMbrData = string("C:\\") + diskIter->first + string("_ldm_mbr_dynamic");
			fp = fopen(strMbrData.c_str(),"wb");
			if(!fp)
			{
				 DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s.\n",strMbrData.c_str());
				 DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				 return false;
			}
			else
			{
				fwrite(diskInfo.LdmDynamicSector, sizeof(SV_UCHAR)*LDM_DYNAMIC_BOOT_SECTOR_LENGTH, 1, fp);
				fclose(fp);
			}
		}
		else if(GPT == diskInfo.DiskInfoSub.FormatType && BASIC == diskInfo.DiskInfoSub.Type)
		{
			string strMbrData = string("C:\\") + diskIter->first + string("_gpt_basic");
			FILE *fp;
			fp = fopen(strMbrData.c_str(),"wb");
			if(!fp)
			{
				 DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s.\n",strMbrData.c_str());
				 DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				 return false;
			}
			else
			{
				fwrite(diskInfo.GptSector, sizeof(SV_UCHAR)*GPT_BOOT_SECTOR_LENGTH, 1, fp);
				fclose(fp);
			}
		}
		else if(GPT == diskInfo.DiskInfoSub.FormatType && DYNAMIC == diskInfo.DiskInfoSub.Type)
		{
			string strMbrData = string("C:\\") + diskIter->first + string("_gpt_dynamic");
			FILE *fp;
			fp = fopen(strMbrData.c_str(),"wb");
			if(!fp)
			{
				 DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s.\n",strMbrData.c_str());
				 DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				 return false;
			}
			else
			{
				fwrite(diskInfo.GptDynamicSector, sizeof(SV_UCHAR)*GPT_DYNAMIC_BOOT_SECTOR_LENGTH, 1, fp);
				fclose(fp);
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return iRetValue;
}


int ChangeVmConfig::GetDiskAndPartitionDetails(const string& strPartitionFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	int iRetValue = 0;

	DlmPartitionInfoMap_t srcMapOfDiskToPartitions;
	DLM_ERROR_CODE RetVal = ReadPartitionInfoFromFile(strPartitionFile.c_str(), srcMapOfDiskToPartitions);
	if(DLM_ERR_SUCCESS != RetVal)
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to collect the Source disks info map from the BLOB file(%s). Error Code - %u\n", strPartitionFile.c_str(), RetVal);
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return 1;
	}

	DlmPartitionInfoIterMap_t diskIter;

	for(diskIter = srcMapOfDiskToPartitions.begin(); diskIter != srcMapOfDiskToPartitions.end(); diskIter++)
	{
		DebugPrintf(SV_LOG_INFO, "Disk Name = %s\n", diskIter->first.c_str());

		DlmPartitionInfoIterVec_t iterVec = diskIter->second.begin();
		for(; iterVec != diskIter->second.end(); iterVec++)
		{
			DebugPrintf(SV_LOG_INFO, "Partition Name            = %s\n", iterVec->PartitionInfoSub.PartitionName);
			DebugPrintf(SV_LOG_INFO, "Partition Type            = %s\n", iterVec->PartitionInfoSub.PartitionType);
			DebugPrintf(SV_LOG_INFO, "Partition Number          = %d\n", iterVec->PartitionInfoSub.PartitionNum);
			DebugPrintf(SV_LOG_INFO, "Partition Length          = %lld\n", iterVec->PartitionInfoSub.PartitionLength);
			DebugPrintf(SV_LOG_INFO, "Partition Starting Offset = %lld\n", iterVec->PartitionInfoSub.StartingOffset);

			stringstream s;
			s << iterVec->PartitionInfoSub.PartitionNum;
		
			string strPartitionData = string("C:\\") + diskIter->first + string("_partition_") + s.str();
			FILE *fp;
			fp = fopen(strPartitionData.c_str(),"wb");
			if(!fp)
			{
				 DebugPrintf(SV_LOG_ERROR,"Failed to open file : %s.\n",strPartitionData.c_str());
				 DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
				 return 1;
			}
			else
			{
				fwrite(iterVec->PartitionSector, sizeof(SV_UCHAR)*(iterVec->PartitionInfoSub.PartitionLength), 1, fp);
				DebugPrintf(SV_LOG_INFO,"Successfully created the file : %s.\n",strPartitionData.c_str());
				fclose(fp);
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return iRetValue;
}


bool ChangeVmConfig::DownloadDataFromDisk(string strDiskName, string strNumOfBytes, bool bEndOfDisk, string strBytesToSkip, string& FileName)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;

	do
	{
		RollBackVM obj;
		LONGLONG ulDiskSize;
		if(false == obj.GetDiskSize(strDiskName.c_str(), ulDiskSize))
		{
			DebugPrintf(SV_LOG_ERROR, "[ERROR] Failed to get the disk size for disk : %s", strDiskName.c_str());
	    	if(bEndOfDisk)
			{
			    bRetValue = false; break;				
			}
	    }

		size_t nNumOfBytes;
		stringstream s(strNumOfBytes);
		s >> nNumOfBytes;

		LONGLONG BytesToSkip = 0;
		if(!strBytesToSkip.empty())
		{
			stringstream ss(strBytesToSkip);
			ss >> BytesToSkip;
		}
		if(bEndOfDisk)
			BytesToSkip = ulDiskSize - nNumOfBytes;

		unsigned char * DiskData = new unsigned char[nNumOfBytes];

		if(NULL == DiskData)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to allocate memory to download data from disk %s.\n", strDiskName.c_str());
			bRetValue = false; break;
		}

		if(false == DownloadFromDisk(strDiskName.c_str(), BytesToSkip, nNumOfBytes, DiskData))
		{
			delete[] DiskData;
			DebugPrintf(SV_LOG_ERROR, "[ERROR] Failed to read the disk data for disk : %s.\n", strDiskName.c_str());
			bRetValue = false; break;
		}

		std::string strDiskNumber = strDiskName;
		std::string strDisk = "\\\\.\\PHYSICALDRIVE";
		size_t pos = strDiskName.find(strDisk);
		if(pos != std::string::npos)
		{
			strDiskNumber = strDiskName.substr(pos+strDisk.length());
		}

		if(FileName.empty())
		{
			FileName = m_VxInstallPath + "\\Failover\\data\\Disk_" + strDiskNumber;
			if(bEndOfDisk)
				FileName += string("_end");
			else
				FileName += string("_begin");
		}

		FILE *pFileWrite = fopen(FileName.c_str(),"wb");
		if(!pFileWrite)
		{
			delete[] DiskData;
			DebugPrintf(SV_LOG_ERROR, "Failed to open the file - %s.\n", FileName.c_str());
			bRetValue = false; break;
		}

		if(! fwrite(DiskData, nNumOfBytes*sizeof(unsigned char), 1, pFileWrite))
		{
			fclose(pFileWrite);
			delete[] DiskData;
			DebugPrintf(SV_LOG_ERROR, "Failed to write data of disk-%s to binary file-%s\n", strDiskName.c_str(), FileName.c_str());
			bRetValue = false; break;
		}
		fflush(pFileWrite);
		fclose(pFileWrite);
		delete[] DiskData;

		DebugPrintf(SV_LOG_INFO, "Successfully downloaded the required data [%s] and stored in file %s.\n", strNumOfBytes.c_str(), FileName.c_str());
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}

bool ChangeVmConfig::UpdateDataOnDisk(string strDiskName, string strNumOfBytes, bool bEndOfDisk, string strBytesToSkip, const string& strBinFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;

	do
	{
		RollBackVM obj;
		LONGLONG ulDiskSize;
		if(false == obj.GetDiskSize(strDiskName.c_str(), ulDiskSize))
		{
			DebugPrintf(SV_LOG_ERROR, "[ERROR] Failed to get the disk size for disk : %s", strDiskName.c_str());
	    	if(bEndOfDisk)
			{
			    bRetValue = false; break;				
			}
	    }

		DebugPrintf(SV_LOG_INFO, "Disk Size = %d", ulDiskSize);

		size_t nNumOfBytes;
		stringstream s(strNumOfBytes);
		s >> nNumOfBytes;

		LONGLONG BytesToSkip = 0;
		if(!strBytesToSkip.empty())
		{
			stringstream ss(strBytesToSkip);
			ss >> BytesToSkip;
		}
		if(bEndOfDisk)
			BytesToSkip = ulDiskSize - nNumOfBytes;

		unsigned char * DiskData = new unsigned char[nNumOfBytes];

		if(NULL == DiskData)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to allocate memory for update data on disk %s.\n", strDiskName.c_str());
			bRetValue = false; break;
		}

		//open the file in binary mode, read mode
		FILE *pFileRead = fopen(strBinFile.c_str(),"rb");
		if(!pFileRead)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to open the file - %s.\n", strBinFile.c_str());
			delete [] DiskData;
			bRetValue = false; break;
		}
		if(! fread(DiskData, sizeof(unsigned char)*nNumOfBytes, 1, pFileRead))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to read data from binary file-%s\n", strBinFile.c_str());
			delete [] DiskData; fclose(pFileRead);
			bRetValue = false; break;
		}
		fclose(pFileRead);

		if(false == UpdateOnDisk(strDiskName.c_str(), BytesToSkip, nNumOfBytes, DiskData))
		{
			delete[] DiskData;
			DebugPrintf(SV_LOG_ERROR, "[ERROR] Failed to read the disk data for disk : %s.\n", strDiskName.c_str());
			bRetValue = false; break;
		}
		delete[] DiskData;
		DebugPrintf(SV_LOG_INFO, "Successfully updated the required amount of data from file %s on disk %s.\n", strBinFile.c_str(), strDiskName.c_str());
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}


bool ChangeVmConfig::UpdateDiskLayoutInfo(const string& strDiskMapFile, const string& strDlmFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;

	do
	{
		map<string, string> mapSrcAndTgtDiskNum;
		if (!GetDiskMappingFromFile(strDiskMapFile, mapSrcAndTgtDiskNum))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get disk map information from file : %s.\n", strDiskMapFile.c_str());
			bRetValue = false; break;
		}

		DisksInfoMap_t srcDisksInfo;
		if(DLM_ERR_SUCCESS != ReadDiskInfoMapFromFile(strDlmFile.c_str(), srcDisksInfo))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get source disks information from file : %s.\n", strDlmFile.c_str());
			bRetValue = false; break;
		}

		if(!UpdateDiskLayout(mapSrcAndTgtDiskNum, srcDisksInfo))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to dump the source disks information on target disks.\n");
			bRetValue = false; break;
		}
		rescanIoBuses();
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}


bool ChangeVmConfig::PrepareTargetDiskForProtection(const string& strDiskMapFile, const string& strDlmFile)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;

	do
	{
		map<string, string> mapSrcAndTgtDiskNum;
		if (!GetDiskMappingFromFile(strDiskMapFile, mapSrcAndTgtDiskNum))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get disk map information from file : %s.\n", strDiskMapFile.c_str());
			bRetValue = false; break;
		}

		DisksInfoMap_t srcDisksInfo;
		if(DLM_ERR_SUCCESS != ReadDiskInfoMapFromFile(strDlmFile.c_str(), srcDisksInfo))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to get source disks information from file : %s.\n", strDlmFile.c_str());
			bRetValue = false; break;
		}

		if(!PrepareDisksForConfiguration(mapSrcAndTgtDiskNum, srcDisksInfo))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to prepare the target disks...\n");
			bRetValue = false; break;
		}

		map<string, string> mapDiskSignatureToNum;
		map<string, string> mapDiskNumToSignature;
		CVdsHelper objVds;
		try
		{
			if(objVds.InitilizeVDS())
				objVds.GetDisksSignature(mapDiskSignatureToNum);
			objVds.unInitialize();
		}
		catch(...)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed while trying to get disk signatures.\n\n");
		}

		map<string, string>::iterator iterDisk;
		for(iterDisk = mapDiskSignatureToNum.begin(); iterDisk != mapDiskSignatureToNum.end(); iterDisk++)
		{
			mapDiskNumToSignature.insert(make_pair(iterDisk->second, iterDisk->first));
		}

		if(!PrepareDynamicGptDisks(mapSrcAndTgtDiskNum, srcDisksInfo))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to prepare the target dynamic gpt disks...\n");
			bRetValue = false; break;
		}

		if(!ConfigureTargetDisks(mapSrcAndTgtDiskNum, srcDisksInfo, mapDiskNumToSignature))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to Configure the Target disks same as source.\n");
			bRetValue = false; break;
		}

		Sleep(10000);
		rescanIoBuses();  //Scans the disk management

		try
		{
			if(objVds.InitilizeVDS())
				objVds.ClearFlagsOfVolume();
			objVds.unInitialize();
		}
		catch(...)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed while trying to clear the hidden flags of volumes.\n\n");
		}

		map<string, string> mapSrcVolToTgtVol;
		if(false == PrepareVolumesMapping(srcDisksInfo, mapSrcVolToTgtVol))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to Create the volumes mapping for protection.\n");
			bRetValue = false; break;
		}

		map<string, string> mapSrcToTgtDiskName;
		for(iterDisk = mapSrcAndTgtDiskNum.begin(); iterDisk != mapSrcAndTgtDiskNum.end(); iterDisk++)
		{
			mapSrcToTgtDiskName.insert(make_pair("\\\\.\\PHYSICALDRIVE" + iterDisk->first, "\\\\.\\PHYSICALDRIVE" + iterDisk->second));
		}

		//Getting Current system (MT) disk information
		DisksInfoMap_t tgtDiskInfo;
		if(DLM_ERR_SUCCESS != CollectDisksInfoFromSystem(tgtDiskInfo))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to collect the disks info from the system.\n");
			bRetValue = false; break;
		}

		//Restoring the mount points.
		if (DLM_ERR_INCOMPLETE == RestoreVConVolumeMountPoints(srcDisksInfo, tgtDiskInfo, mapSrcToTgtDiskName, mapSrcVolToTgtVol)) 
		{
			DebugPrintf(SV_LOG_ERROR,"Failed while restoring the mount points..Retry again after validating the disks\n");
			bRetValue = false; break;
		}
		else
			DebugPrintf(SV_LOG_INFO,"Successfully restored the mount points...\n");

		//Register-Host to update the newly created volumes to CX.
		RegisterHostUsingCdpCli();

		DebugPrintf(SV_LOG_INFO,"\n***** Volume Pairs *****\n\n");
		map<string, string>::iterator iterVolume = mapSrcVolToTgtVol.begin();
		for(; iterVolume != mapSrcVolToTgtVol.end(); iterVolume++)
		{
			DebugPrintf(SV_LOG_INFO,"Src Vol = %s \t Tgt Vol = %s.\n", iterVolume->first.c_str(), iterVolume->second.c_str());
		}
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}


bool ChangeVmConfig::PrepareVolumesMapping(DisksInfoMap_t& srcDisksInfo, map<string, string>& mapSrcVolToTgtVol)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;

	CHAR szSysPath[MAX_PATH];
	CHAR szRootVolumeName[MAX_PATH];

	//System Directory Path
	if(0 == GetSystemDirectory(szSysPath, MAX_PATH))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get System Directory Path. ErrorCode : [%lu].\n",GetLastError());
		bRetValue = false;
	}

    if (0 == SVGetVolumePathName(szSysPath, szRootVolumeName, ARRAYSIZE(szRootVolumeName)))
	{
		DebugPrintf(SV_LOG_ERROR,"Failed to get the Root volume Name of the System Directory. ErrorCode : [%lu].\n",GetLastError());
		bRetValue = false;
	}

	string strInmageDirectory = string(szRootVolumeName) + "INM";
	if(FALSE == CreateDirectory(strInmageDirectory.c_str(),NULL))
	{
		if(GetLastError() == ERROR_ALREADY_EXISTS)// To Support Rerun scenario
		{
			DebugPrintf(SV_LOG_INFO,"'INM' Directory already exists. Considering it not as an Error.\n");
		}
		else
		{
            DebugPrintf(SV_LOG_ERROR,"Failed to create the Root Directory - %s. ErrorCode : [%lu].\n",strInmageDirectory.c_str(),GetLastError());
			bRetValue = false;
		}
	}
		

	set<string> listVol;
	DisksInfoMapIter_t iterDiskInfo = srcDisksInfo.begin();
	for(; iterDiskInfo != srcDisksInfo.end(); iterDiskInfo++)
	{
		VolumesInfoVecIter_t iterVol = iterDiskInfo->second.VolumesInfo.begin();
		for(; iterVol != iterDiskInfo->second.VolumesInfo.end(); iterVol++)
		{
			listVol.insert(string(iterVol->VolumeName));
		}
	}

	set<string>::iterator iterVol = listVol.begin();
	for(; iterVol != listVol.end(); iterVol++)
	{
		string strSrcVolName = (*iterVol);
		RemoveTrailingCharactersFromVolume(strSrcVolName);

		//Removing unwanted characters from Source Volume name
		for(size_t indx = 0;indx<strSrcVolName.length();indx++)
		{
            DebugPrintf(SV_LOG_INFO,"In the loop for removing unwanted characters from source volume name.\n");
			if((strSrcVolName[indx] == '\\') || (strSrcVolName[indx] == '/') || (strSrcVolName[indx] == ':') || (strSrcVolName[indx] == '?'))
			{
				strSrcVolName[indx] = '_';
			}
		}
		string strTgtMountPoint = strInmageDirectory + "\\Mount_" + strSrcVolName + "\\";

		DebugPrintf(SV_LOG_INFO,"Src Vol = %s \t Tgt Vol = %s.\n", iterVol->c_str(), strTgtMountPoint.c_str());
		mapSrcVolToTgtVol.insert(make_pair(*iterVol, strTgtMountPoint));
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}


bool ChangeVmConfig::GetDiskMappingFromFile(const string& strDiskMapFile, map<string, string>& mapSrcAndTgtDiskNum)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;
	string token(",");
	size_t found, pos = 0;
	
    ifstream inFile(strDiskMapFile.c_str());
    if(!inFile.is_open())
    {
        DebugPrintf(SV_LOG_ERROR,"Failed to read the File : %s with Error : [%lu].\n",strDiskMapFile.c_str(),GetLastError());
        bRetValue = false;
    }
	else
	{
		vector<string> v;
		string strLineRead;
		while(getline(inFile,strLineRead))
        {			
			if(strLineRead.empty())
			{
				DebugPrintf(SV_LOG_INFO, "End of file reached...\n" );
				break;
			}
			DebugPrintf(SV_LOG_INFO,"%s\n",strLineRead.c_str());

			found = strLineRead.find(token, pos);
			while(found != std::string::npos)
			{
				string strDiskMap = strLineRead.substr(pos,found-pos);
				DebugPrintf(SV_LOG_INFO,"Element - [%s]\n", strDiskMap.c_str());
				v.push_back(strDiskMap);
				pos = found + 1; 
				found = strLineRead.find(token, pos);            
			}
			DebugPrintf(SV_LOG_INFO,"Element - [%s]\n", strLineRead.substr(pos).c_str());
			v.push_back(strLineRead.substr(pos));
		}
		inFile.close();
			
		token = ":";
		vector<string>::iterator iter = v.begin();
		for(; iter != v.end(); iter++)
		{
			pos = iter->find_first_of(token);
			if(pos != std::string::npos)
			{
				string strSrcDiskNum = iter->substr(0,pos);
				string strTgtDiskNum = iter->substr(pos+1);
				DebugPrintf(SV_LOG_INFO,"Source disk number = %s  <==>  ", strSrcDiskNum.c_str());
				DebugPrintf(SV_LOG_INFO,"Target disk number =  %s\n", strTgtDiskNum.c_str());
				if ((!strSrcDiskNum.empty()) && (!strTgtDiskNum.empty()))
					mapSrcAndTgtDiskNum.insert(make_pair(strSrcDiskNum,strTgtDiskNum));
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to find \":\" in lineread in the file : %s for diskmap : %s\n", strDiskMapFile.c_str(), iter->c_str());
				bRetValue = false;
				break;
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}


bool ChangeVmConfig::PrepareDisksForConfiguration(map<string, string>& mapSrcAndTgtDiskNum, DisksInfoMap_t& srcDisksInfo)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;
	map<string, string>::iterator iter;

	do
	{
		//Stop this process to avoid the format popups.
        if(false == StopService("ShellHWDetection"))
        {
            DebugPrintf(SV_LOG_WARNING,"Failed to stop the service - ShellHWDetection.\n");
            DebugPrintf(SV_LOG_WARNING,"Please cancel all the popups of format volumes if observed.\n");
        }

		//disables the automount for newly created volumes.
		if(DLM_ERR_SUCCESS != AutoMount(false))
		{
			DebugPrintf(SV_LOG_WARNING,"Error: Failed to disable the automount for newly created volumes.\n");
			DebugPrintf(SV_LOG_INFO,"Info  :Manually delete the volume names at the Master Target if not required.\n");
		}

		//Cleanup any missing disks if exists.
		CVdsHelper objVds;
		try
		{
			if(objVds.InitilizeVDS())
				objVds.RemoveMissingDisks();  //Removes if any missing disks available in MT.
			objVds.unInitialize();
		}
		catch(...)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed while trying to remove the missing disks\n");
		}

		for(iter = mapSrcAndTgtDiskNum.begin(); iter != mapSrcAndTgtDiskNum.end(); iter++)
		{
			if(DLM_ERR_SUCCESS != ClearReadOnlyAttrOfDisk(iter->second))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to clear read-only attribute for disk : %s\n", iter->second.c_str());
				bRetValue = false; break;
			}
			if(DLM_ERR_SUCCESS != OnlineOrOfflineDisk(iter->second, true))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to online the disk : %s\n", iter->second.c_str());
				bRetValue = false; break;
			}
			if(DLM_ERR_SUCCESS != CleanDisk(iter->second))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to clean disk : %s\n", iter->second.c_str());
				bRetValue = false; break;
			}
			Sleep(2000);  //All the disks need to update properly. so sleeping for 10 secs.

			string srcDisk = "\\\\.\\PHYSICALDRIVE" + iter->first;
			DisksInfoMapIter_t iterDiskInfo = srcDisksInfo.find(srcDisk);
			if(iterDiskInfo == srcDisksInfo.end())
			{
				DebugPrintf(SV_LOG_ERROR,"Source disk information doesnot found for disk %s.\n", srcDisk.c_str());
				bRetValue = false; break;
			}

			if(DLM_ERR_SUCCESS != InitializeDisk(iter->second, iterDiskInfo->second.DiskInfoSub.FormatType, iterDiskInfo->second.DiskInfoSub.Type))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to Initialise the disk : %s\n", iter->second.c_str());
				bRetValue = false; break;
			}
		}
		rescanIoBuses();  //Scans the disk management
		Sleep(10000); 
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}

bool ChangeVmConfig::PrepareDynamicGptDisks(map<string, string>& mapSrcAndTgtDiskNum, DisksInfoMap_t& srcDisksInfo)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;

	map<string, string>::iterator iter;
	for(iter = mapSrcAndTgtDiskNum.begin(); iter != mapSrcAndTgtDiskNum.end(); iter++)
	{
		string srcDisk = "\\\\.\\PHYSICALDRIVE" + iter->first;
		DisksInfoMapIter_t iterDiskInfo = srcDisksInfo.find(srcDisk);
		if(iterDiskInfo == srcDisksInfo.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Source disk information doesnot found for disk %s.\n", srcDisk.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		if(GPT == iterDiskInfo->second.DiskInfoSub.FormatType && DYNAMIC == iterDiskInfo->second.DiskInfoSub.Type)
		{
			//Collecting 2 KB data in case fo GPT dynamic disk
			if(false == CollectGPTData(iter->second))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to collect the 2KB GPT initial data for disk = %s.\n", iter->second.c_str());
				bRetValue = false;
			}

			if(DLM_ERR_SUCCESS != CleanDisk(iter->second))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to clean the GPT disk = %s.\n", iter->second.c_str());
				bRetValue = false;
			}
			Sleep(20000);
		}
	}
	rescanIoBuses();  //Scans the disk management
	Sleep(10000);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}


bool ChangeVmConfig::CollectGPTData(string strDiskNmbr)
{
	bool bRetValue = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string strGptFileName;
	strGptFileName = m_VxInstallPath + "\\Failover\\Data\\" + "Disk_" + strDiskNmbr + GPT_FILE_NAME;
					
	DebugPrintf(SV_LOG_INFO,"GPT file name is = %s\n",strGptFileName.c_str());

	string strDiskName = string("\\\\.\\PHYSICALDRIVE") + strDiskNmbr;

	do
	{
		unsigned char * DiskData = new unsigned char[2048];
		if(NULL == DiskData)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to allocate memory to download data from disk %s.\n", strDiskName.c_str());
			bRetValue = false; break;
		}

		if(false == DownloadFromDisk(strDiskName.c_str(), 0, 2048, DiskData))
		{
			delete[] DiskData;
			DebugPrintf(SV_LOG_ERROR, "[ERROR] Failed to read the disk data for disk : %s.\n", strDiskName.c_str());
			bRetValue = false; break;
		}

		FILE *pFileWrite = fopen(strGptFileName.c_str(),"wb");
		if(!pFileWrite)
		{
			delete[] DiskData;
			DebugPrintf(SV_LOG_ERROR, "Failed to open the file - %s.\n", strGptFileName.c_str());
			bRetValue = false; break;
		}

		if(! fwrite(DiskData, 2048*sizeof(unsigned char), 1, pFileWrite))
		{
			fclose(pFileWrite); delete[] DiskData;
			DebugPrintf(SV_LOG_ERROR, "Failed to write data of disk-%s to binary file-%s\n", strDiskName.c_str(), strGptFileName.c_str());
			bRetValue = false; break;
		}
		fflush(pFileWrite);
		fclose(pFileWrite);
		delete[] DiskData;
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}


bool ChangeVmConfig::UpdateGPTDataOnDisk(string strDiskNmbr)
{
	bool bRetValue = true;
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);

	string strGptFileName;
	strGptFileName = m_VxInstallPath + "\\Failover\\Data\\" + "Disk_" + strDiskNmbr + GPT_FILE_NAME;
					
	DebugPrintf(SV_LOG_INFO,"GPT file name is = %s\n",strGptFileName.c_str());

	string strDiskName = string("\\\\.\\PHYSICALDRIVE") + strDiskNmbr;

	do
	{
		unsigned char * DiskData = new unsigned char[2048];
		if(NULL == DiskData)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to allocate memory to download data from disk %s.\n", strDiskName.c_str());
			bRetValue = false; break;
		}

		//open the file in binary mode, read mode
		FILE *pFileRead = fopen(strGptFileName.c_str(),"rb");
		if(!pFileRead)
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to open the file - %s.\n", strGptFileName.c_str());
			delete [] DiskData;
			bRetValue = false; break;
		}
		if(! fread(DiskData, sizeof(unsigned char)*2048, 1, pFileRead))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to read data from binary file-%s\n", strGptFileName.c_str());
			delete [] DiskData; fclose(pFileRead);
			bRetValue = false; break;
		}
		fclose(pFileRead);

		if(false == UpdateOnDisk(strDiskName.c_str(), 0, 2048, DiskData))
		{
			delete[] DiskData;
			DebugPrintf(SV_LOG_ERROR, "[ERROR] Failed to read the disk data for disk : %s.\n", strDiskName.c_str());
			bRetValue = false; break;
		}
		delete[] DiskData;
	}while(0);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}

bool ChangeVmConfig::UpdateDiskLayout(map<string, string>& mapSrcAndTgtDiskNum, DisksInfoMap_t& srcDisksInfo)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;

	map<string, string>::iterator iterDisk = mapSrcAndTgtDiskNum.begin();
	for(; iterDisk != mapSrcAndTgtDiskNum.end(); iterDisk++)
	{
		string strSrcDisk = "\\\\.\\PHYSICALDRIVE" + iterDisk->first;
		string strTgtDisk = "\\\\.\\PHYSICALDRIVE" + iterDisk->second;

		DisksInfoMapIter_t iterDiskInfo = srcDisksInfo.find(strSrcDisk);
		if(iterDiskInfo == srcDisksInfo.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Source disk information doesnot found for disk %s from the disks information\n", strSrcDisk.c_str());
			bRetValue = false; break;
		}

		if(DYNAMIC == iterDiskInfo->second.DiskInfoSub.Type)
		{
			if(DLM_ERR_SUCCESS != OnlineOrOfflineDisk(iterDisk->second, false))
				DebugPrintf(SV_LOG_ERROR,"Failed to offline the disk : %s\n", iterDisk->second.c_str());

			if(DLM_ERR_SUCCESS != ClearReadOnlyAttrOfDisk(iterDisk->second))
				DebugPrintf(SV_LOG_ERROR,"Failed to clear read-only attribute for disk : %s\n", iterDisk->second.c_str());
		}

		RollBackVM obj;
		SV_LONGLONG tgtDiskSize;
		if(true == obj.GetDiskSize(strTgtDisk.c_str(), tgtDiskSize))
		{
			DebugPrintf(SV_LOG_INFO,"Source disk : %s --> size : %lld\n", strSrcDisk.c_str(), iterDiskInfo->second.DiskInfoSub.Size);
			DebugPrintf(SV_LOG_INFO,"Target disk : %s --> size : %lld\n", strTgtDisk.c_str(), tgtDiskSize);
			if(tgtDiskSize < iterDiskInfo->second.DiskInfoSub.Size)
			{
				DebugPrintf(SV_LOG_ERROR,"Source disk size is more then target disk size. Disk Metadata update won't restore the mount points\n");
				bRetValue = false; break;
			}
		}

		if(DLM_ERR_SUCCESS != RestoreDisk(strTgtDisk.c_str(), strSrcDisk.c_str(), iterDiskInfo->second))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to Update MBR (or) GPT for disk %s on disk %s.\n", strSrcDisk, strTgtDisk.c_str());
			bRetValue = false; break;
		}
		Sleep(2000);
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}


bool ChangeVmConfig::ConfigureTargetDisks(map<string, string>& mapSrcAndTgtDiskNum, DisksInfoMap_t& srcDisksInfo, map<string, string>& mapDiskNameToSignature)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;

	set<string> listDynDisks;
	map<string, string>::iterator iterDisk = mapSrcAndTgtDiskNum.begin();
	for(; iterDisk != mapSrcAndTgtDiskNum.end(); iterDisk++)
	{
		string strSrcDisk = "\\\\.\\PHYSICALDRIVE" + iterDisk->first;
		string strTgtDisk = "\\\\.\\PHYSICALDRIVE" + iterDisk->second;

		DisksInfoMapIter_t iterDiskInfo = srcDisksInfo.find(strSrcDisk);
		if(iterDiskInfo == srcDisksInfo.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Source disk information doesnot found for disk %s from the disks information\n", strSrcDisk.c_str());
			bRetValue = false; break;
		}

		RollBackVM obj;
		SV_LONGLONG tgtDiskSize;
		if(true == obj.GetDiskSize(strTgtDisk.c_str(), tgtDiskSize))
		{
			DebugPrintf(SV_LOG_INFO,"Source disk : %s --> size : %lld\n", strSrcDisk.c_str(), iterDiskInfo->second.DiskInfoSub.Size);
			DebugPrintf(SV_LOG_INFO,"Target disk : %s --> size : %lld\n", strTgtDisk.c_str(), tgtDiskSize);
			if(tgtDiskSize < iterDiskInfo->second.DiskInfoSub.Size)
			{
				DebugPrintf(SV_LOG_ERROR,"Source disk size is more then target disk size. Disk Metadata update won't restore the mount points\n");
				bRetValue = false; break;
			}
		}

		DLM_ERROR_CODE RetVal = RestoreDisk(strTgtDisk.c_str(), strSrcDisk.c_str(), iterDiskInfo->second);
		if(DLM_ERR_SUCCESS != RetVal)
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to Update MBR (or) GPT for disk %s on disk %s. Error Code - %u\n", strSrcDisk, strTgtDisk.c_str(), RetVal);
			bRetValue = false; break;
		}
		if(GPT == iterDiskInfo->second.DiskInfoSub.FormatType && DYNAMIC == iterDiskInfo->second.DiskInfoSub.Type)
		{
			//Update Collected 2 KB data in case fo GPT dynamic disk
			if(false == UpdateGPTDataOnDisk(iterDisk->second))
			{
				DebugPrintf(SV_LOG_ERROR,"Failed to update the 2KB GPT initial data for disk = %d\n",iterDisk->second);
				bRetValue = false; break;
			}
		}

		if(DYNAMIC == iterDiskInfo->second.DiskInfoSub.Type)
			listDynDisks.insert(iterDisk->second);
	}

	rescanIoBuses();  //Rescans the disks once bcz some of the disk onlined but diskmanagement is not updated with latest data

	if(bRetValue)
	{
		if(!UpdateSignatureOfDisk(mapSrcAndTgtDiskNum, srcDisksInfo, mapDiskNameToSignature))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to update the disk signature for the disks\n");
			bRetValue = false;
		}
		else
		{
			//online and offline disks. This is required because volumes of basic will be inaccessible state.
			for(iterDisk = mapSrcAndTgtDiskNum.begin(); iterDisk != mapSrcAndTgtDiskNum.end(); iterDisk++)
			{
				string strSrcDisk = "\\\\.\\PHYSICALDRIVE" + iterDisk->first;
				DisksInfoMapIter_t iterDiskInfo = srcDisksInfo.find(strSrcDisk);
				if(iterDiskInfo == srcDisksInfo.end())
				{
					DebugPrintf(SV_LOG_ERROR,"Source disk information doesnot found for disk %s from the disks information\n", strSrcDisk.c_str());
					bRetValue = false; break;
				}

				if(BASIC == iterDiskInfo->second.DiskInfoSub.Type)
				{
					if(DLM_ERR_SUCCESS != OnlineOrOfflineDisk(iterDisk->second, false))
						DebugPrintf(SV_LOG_ERROR,"Failed to offline the disk : %s\n", iterDisk->second.c_str());

					if(DLM_ERR_SUCCESS != OnlineOrOfflineDisk(iterDisk->second, true))
						DebugPrintf(SV_LOG_ERROR,"Failed to online the disk : %s\n", iterDisk->second.c_str());
				}
			}
			Sleep(10000); 

			if(!listDynDisks.empty())
			{
				if(DLM_ERR_FAILURE == ImportDisks(listDynDisks))
				{
					DebugPrintf(SV_LOG_ERROR,"Failed to Import dynamic disks.\n");
					bRetValue = false;
				}
				else
					Sleep(10000);
			}
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}

bool ChangeVmConfig::UpdateSignatureOfDisk(map<string, string>& mapSrcAndTgtDiskNum, DisksInfoMap_t& srcDisksInfo, map<string, string>& mapDiskNameToSignature)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRetValue = true;

	map<string, string>::iterator iterDisk = mapSrcAndTgtDiskNum.begin();
	for(; iterDisk != mapSrcAndTgtDiskNum.end(); iterDisk++)
	{
		string strSrcDisk = "\\\\.\\PHYSICALDRIVE" + iterDisk->first;
		DisksInfoMapIter_t iterDiskInfo = srcDisksInfo.find(strSrcDisk);
		if(iterDiskInfo == srcDisksInfo.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Source disk information doesnot found for disk %s.\n", strSrcDisk.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}
		/*string strTgtDisk = "\\\\?\\PhysicalDrive" + iterDisk->second;*/
		DebugPrintf(SV_LOG_INFO,"Changing DiskSignature/DiskIdentifier for disk %s.\n", iterDisk->second.c_str());
		map<string, string>::iterator iterSig = mapDiskNameToSignature.find(iterDisk->second);
		if(iterSig == mapDiskNameToSignature.end())
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to find DiskSignature/DiskIdentifier for disk %s\n", iterDisk->second.c_str());
			continue;
		}

		//now we have to find the Layout of Disk, is it MBR or GPT?
		string diskId = iterSig->second;
		if(MBR == iterDiskInfo->second.DiskInfoSub.FormatType)
		{
			//in this case we need to truncate the String.
			diskId = iterSig->second.substr(0,iterSig->second.find_first_of('-'));
			diskId.erase(0, 1);
		}
		else if(GPT == iterDiskInfo->second.DiskInfoSub.FormatType)
		{
			diskId = iterSig->second.erase(0, 1);
			int length = diskId.size();
			diskId.erase(length-1, 1);
		}
		DebugPrintf(SV_LOG_INFO,"DiskSignature/DiskIdentifier for disk %s to update : %s\n", iterDisk->second.c_str(), diskId.c_str());
		
		string strDiskPartFile = m_VxInstallPath + "\\Failover\\Data\\" + "disksignature";
		DebugPrintf(SV_LOG_INFO,"File Name - %s.\n",strDiskPartFile.c_str());

		ofstream outfile(strDiskPartFile.c_str(), ios_base::out);
		if(! outfile.is_open())
		{
			DebugPrintf(SV_LOG_ERROR,"\n Failed to open %s \n",strDiskPartFile.c_str());
			DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
			return false;
		}

		string strDiskNumber = string("select disk ") + iterDisk->second; 
		outfile << strDiskNumber << endl;
		outfile << "uniqueid disk id="<<diskId<< endl;
		outfile.flush();
		outfile.close();

		DebugPrintf(SV_LOG_INFO,"uniqueid disk id=%s\n",diskId.c_str());			

		string strExeName = "diskpart.exe";
		string strParameterToDiskPartExe = string (" /s ") + string("\"")+ strDiskPartFile + string("\"");
		DebugPrintf(SV_LOG_INFO,"Command to convert signature of disk : %s%s\n",strExeName.c_str(),strParameterToDiskPartExe.c_str());

		strExeName = strExeName + strParameterToDiskPartExe;
		string strOurPutFile = m_VxInstallPath + "\\Failover\\Data\\" + string("ChangeDiskSignatureOutput.txt");
		if(FALSE == ExecuteCmdWithOutputToFileWithPermModes(strExeName, strOurPutFile, O_APPEND | O_RDWR | O_CREAT))
		{
			DebugPrintf(SV_LOG_ERROR,"Failed to change disk signature of disks %d and respective source disk %s\n", iterDisk->second, strSrcDisk.c_str());
			bRetValue = false;
		}
		Sleep(10000);//This is recommended between successive Diskpart call to change disk signature.
	}

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRetValue;
}


//Stop the given service.
//Return true - if service is stopped or does not exist.
//Return false - if service is faile to stop.
bool ChangeVmConfig::StopService(const string serviceName)
{
    DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = false;

	SC_HANDLE schService;        
    SERVICE_STATUS_PROCESS  serviceStatusProcess;
    SERVICE_STATUS serviceStatus;
    DWORD actualSize = 0;
	ULONG dwRet = 0;
	LPVOID lpMsgBuf;

    do
    {

	    SC_HANDLE schSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == schSCManager) 
        {
		    DebugPrintf(SV_LOG_ERROR,"Failed to Get handle to Windows Services Manager in OpenSCMManager.\n");    
            rv = false;
            break;
        }
	    
	    schService = OpenService(schSCManager, serviceName.c_str(), SERVICE_ALL_ACCESS); 
        if (NULL != schService)
        { 
		    if (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_RUNNING == serviceStatusProcess.dwCurrentState) 
            {
                // stop service
			    if (::ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus))
                {
                    // wait for it to actually stop
	                DebugPrintf(SV_LOG_INFO,"Waiting for the service \"%s\" to stop.\n", serviceName.c_str() );
				    int retry = 1;
                    do
                    {
                        Sleep(1000); 
				    } while(::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_STOPPED != serviceStatusProcess.dwCurrentState && retry++ <= 600);

				    if (::QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(serviceStatusProcess), &actualSize) && SERVICE_STOPPED == serviceStatusProcess.dwCurrentState) 
                    {
	                    DebugPrintf(SV_LOG_INFO,"Successfully stopped the service : \"%s\"\n",serviceName.c_str());
					    ::CloseServiceHandle(schService); 	    
					    rv = true;
                        break;
				    }
                }
		    }
		    else
		    {
	            DebugPrintf(SV_LOG_INFO,"The service \"%s\" is stopped already.\n",serviceName.c_str() );			    
			    ::CloseServiceHandle(schService); //close the service handle
			    rv = true;
                break;
		    }
		    // Collect the error ahead of other statement ::CloseServiceHandle()
		    dwRet = GetLastError();		    
		    ::CloseServiceHandle(schService); //close the service handle
	    }
    	
	    if( !dwRet )
		    dwRet = GetLastError();       		
    		
	    if(dwRet == ERROR_SERVICE_DOES_NOT_EXIST)
	    {         
	    	DebugPrintf(SV_LOG_INFO,"The service \"%s\" does not exist as an installed service.\n",serviceName.c_str());
		    rv = true;
            break;
	    }

        ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwRet, 0, (LPTSTR)&lpMsgBuf, 0, NULL);	
        if(dwRet)
	    {
		    DebugPrintf(SV_LOG_ERROR,"Failed to stop the service \"%s\" with Error:[%d - %s].\n",serviceName.c_str(),dwRet,(LPCTSTR)lpMsgBuf);
	    }
	    LocalFree(lpMsgBuf); // Free the buffer
    }while(0);
    
    DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;	
}


bool ChangeVmConfig::RegisterHostUsingCdpCli()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
    bool rv = true;

	string CdpcliCmd = string("cdpcli.exe --registerhost --loglevel=") + boost::lexical_cast<std::string>(SV_LOG_ALWAYS);

	DebugPrintf(SV_LOG_INFO,"\n");
    DebugPrintf(SV_LOG_INFO,"CDPCLI command : %s\n", CdpcliCmd.c_str());

	RollBackVM obj;
    if(false == obj.CdpcliOperations(CdpcliCmd))
    {
        DebugPrintf(SV_LOG_ERROR,"CDPCLI register host operation got failed\n");
        rv = false;
    }

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
    return rv;
}


void ChangeVmConfig::RemoveTrailingCharactersFromVolume(std::string& volumeName)
{  
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	// we need to strip off any leading \, ., ? if they exists
	std::string::size_type idx = volumeName.find_first_not_of("\\.?");
	if (std::string::npos != idx) {
		volumeName.erase(0, idx);
	}

	// strip off trailing :\ or : if they exist
	std::string::size_type len = volumeName.length();
	idx = len;
	if ('\\' == volumeName[len - 1] || (':' == volumeName[len - 1])) 
	{
		--idx;
	} 

	if (':' == volumeName[len - 2]) 
	{
		--idx;
	} 

	if (idx < len) {
		volumeName.erase(idx);
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


/******************************************************

CLASS - CVdsHelper

*******************************************************/

CVdsHelper::CVdsHelper(void)
:m_pIVdsService(NULL)
{
}

CVdsHelper::~CVdsHelper(void)
{
	INM_SAFE_RELEASE(m_pIVdsService);
}

bool CVdsHelper::InitVDS()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bInitSuccess = false;
	HRESULT hResult;
	IVdsServiceLoader *pVdsSrvLoader = NULL;

	hResult = CoInitializeEx(NULL,COINIT_MULTITHREADED);
	if(SUCCEEDED(hResult))
	{
		hResult = CoCreateInstance(CLSID_VdsLoader,
			NULL,
			CLSCTX_LOCAL_SERVER,
			IID_IVdsServiceLoader,
			(LPVOID*)&pVdsSrvLoader);

		if(SUCCEEDED(hResult))
		{
			//Release if we have already loaded the VDS Service
			INM_SAFE_RELEASE(m_pIVdsService);
			hResult = pVdsSrvLoader->LoadService(NULL,&m_pIVdsService);
		}

		INM_SAFE_RELEASE(pVdsSrvLoader);

		if(SUCCEEDED(hResult))
		{
			hResult = m_pIVdsService->WaitForServiceReady();
			if(SUCCEEDED(hResult))
			{
				DebugPrintf(SV_LOG_INFO, "VDS Initializaton successful\n");
				bInitSuccess = true;
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR,"VDS Service ready failure %ld\n", hResult);
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR,"VDS Initialization failure %ld\n", hResult);
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR,"COM initialization failed %ld \n", hResult);
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bInitSuccess;
}

HRESULT CVdsHelper::ProcessProviders()
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hr;
	IEnumVdsObject *pEnumSwProvider = NULL;
	IUnknown *pIUnkown = NULL;
	IVdsSwProvider *pVdsSwProvider = NULL;
	ULONG ulFetched = 0;

	hr = m_pIVdsService->QueryProviders(
						VDS_QUERY_SOFTWARE_PROVIDERS,
						&pEnumSwProvider);
	if(SUCCEEDED(hr)&&(!pEnumSwProvider))
	{
		hr = E_UNEXPECTED;
	}
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			hr = pEnumSwProvider->Next(1,&pIUnkown,&ulFetched);
			if(0 == ulFetched)
			{
				break;
			}
			if(SUCCEEDED(hr) && (!pIUnkown))
			{
				hr = E_UNEXPECTED;
			}
			if(SUCCEEDED(hr))
			{
				//Get software provider interface pointer for pack details.
				hr = pIUnkown->QueryInterface(IID_IVdsSwProvider,
					(void**)&pVdsSwProvider);
			}
			if(SUCCEEDED(hr) && (!pVdsSwProvider))
			{
				hr = E_UNEXPECTED;
			}
			if(SUCCEEDED(hr))
			{
				ProcessPacks(pVdsSwProvider);
			}
			INM_SAFE_RELEASE(pVdsSwProvider);
			INM_SAFE_RELEASE(pIUnkown);
		}
	}
	INM_SAFE_RELEASE(pEnumSwProvider);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}

HRESULT CVdsHelper::ProcessPacks(IVdsSwProvider *pSwProvider)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	USES_CONVERSION;
	HRESULT hr;
	IEnumVdsObject  *pEnumPacks = NULL;
	IUnknown		*pIUnkown = NULL;
	IVdsPack		*pPack = NULL;
	ULONG ulFetched = 0;

	hr = pSwProvider->QueryPacks(&pEnumPacks);
	if(SUCCEEDED(hr)&&(!pEnumPacks))
	{
		hr = E_UNEXPECTED;
	}
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			hr = pEnumPacks->Next(1,&pIUnkown,&ulFetched);
			if(0 == ulFetched)
			{
				break;
			}
			if(SUCCEEDED(hr)&&(!pIUnkown))
			{
				hr = E_UNEXPECTED;
			}
			if(SUCCEEDED(hr))
			{
				hr = pIUnkown->QueryInterface(IID_IVdsPack,
					(void**)&pPack);
			}
			if(SUCCEEDED(hr)&&(!pPack))
			{
				hr = E_UNEXPECTED;
			}
			if(SUCCEEDED(hr))
			{
				switch(m_objType)
				{
				case INM_OBJ_UNKNOWN:
					CheckUnKnownDisks(pPack);
					break;
				case INM_OBJ_MISSING_DISK:
					RemoveMissingDisks(pPack);
					break;
				case INM_OBJ_FAILED_VOL:
					DeleteFailedVolumes(pPack);
					break;
				case INM_OBJ_DISK_SCSI_ID:
					CollectDiskScsiIDs(pPack);
					break;
				case INM_OBJ_DISK_SIGNATURE:
					CollectDiskSignatures(pPack);
					break;
				case INM_OBJ_FOREIGN_DISK:
					CollectForeignDisks(pPack);
					break;
				case INM_OBJ_HIDDEN_VOL:
					ClearFlagsOfVolume(pPack);
					break;
				case INM_OBJ_DYNAMIC_DISK:
					CollectDynamicDisks(pPack);
					break;
				case INM_OBJ_OFFLINE_DISK:
					CollectOfflineDisks(pPack);
					break;
				default:
					DebugPrintf(SV_LOG_ERROR,"Error: Object type not set for vds helper.\n");
					hr = E_UNEXPECTED;
					INM_SAFE_RELEASE(pPack);
					INM_SAFE_RELEASE(pIUnkown);
					INM_SAFE_RELEASE(pEnumPacks);
					DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
					return hr;
				}
			}
			INM_SAFE_RELEASE(pPack);
			INM_SAFE_RELEASE(pIUnkown);
		}
	}
	INM_SAFE_RELEASE(pEnumPacks);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}


HRESULT CVdsHelper::CheckUnKnownDisks(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hr;
	IEnumVdsObject *pIEnumDisks = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk = NULL;
	ULONG ulFetched = 0;

	bool isDynProvider = false;
	IVdsProvider *pProvider = NULL;
	hr = pPack->GetProvider(&pProvider);
	if(SUCCEEDED(hr) && (!pProvider))
		hr = E_UNEXPECTED;
	if(SUCCEEDED(hr))
	{
		VDS_PROVIDER_PROP ProviderProps;
		ZeroMemory(&ProviderProps,sizeof(ProviderProps));
		hr = pProvider->GetProperties(&ProviderProps);
		if(SUCCEEDED(hr))
		{
			if(ProviderProps.ulFlags & VDS_PF_DYNAMIC)
				isDynProvider = true;
		}
		CoTaskMemFree(ProviderProps.pwszName);
		CoTaskMemFree(ProviderProps.pwszVersion);
	}
	INM_SAFE_RELEASE(pProvider);

	if(!isDynProvider)	//missing disks related to only dynamic providers
		return hr;

	hr = pPack->QueryDisks(&pIEnumDisks);
	if(SUCCEEDED(hr) && (!pIEnumDisks))
	{
		hr = E_UNEXPECTED;
	}
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumDisks->Next(1,&pIUnknown,&ulFetched);
			if(0 == ulFetched)
				break;
			if(SUCCEEDED(hr)&&(!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsDisk,
					(void**)&pDisk);
			}
			if(SUCCEEDED(hr) && (!pDisk))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				//Disk properties
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{					
					if(VDS_DS_UNKNOWN == diskProps.status)
					{
						DebugPrintf(SV_LOG_INFO,"Unknown disk found : %s\n", GUIDString(diskProps.id).c_str());
						m_bAvailable = true;
					}
					
					CoTaskMemFree(diskProps.pwszDiskAddress);
					CoTaskMemFree(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDevicePath);
					CoTaskMemFree(diskProps.pwszFriendlyName);
					CoTaskMemFree(diskProps.pwszAdaptorName);
				}
			}
			INM_SAFE_RELEASE(pDisk);
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumDisks);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}


HRESULT CVdsHelper::ClearFlagsOfVolume(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hr = S_OK;
USES_CONVERSION;
	IEnumVdsObject *pIEnumVoluems = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsVolume *pIVolume = NULL;
	ULONG ulFetched = 0;

	std::list<VDS_OBJECT_ID> lstFailedVols;

	hr = pPack->QueryVolumes(&pIEnumVoluems);
	if(SUCCEEDED(hr) && (!pIEnumVoluems))
		hr = E_UNEXPECTED;
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumVoluems->Next(1,&pIUnknown,&ulFetched);
			if(0 == ulFetched || hr == S_FALSE)
				break;
			if(SUCCEEDED(hr) && (!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsVolume,
					(void**)&pIVolume);
				if(SUCCEEDED(hr) && (!pIVolume))
					hr = E_UNEXPECTED;
				if(SUCCEEDED(hr))
				{
					VDS_VOLUME_PROP volProps;
					ZeroMemory(&volProps,sizeof(volProps));
					hr = pIVolume->GetProperties(&volProps);
					if(SUCCEEDED(hr))
					{
						if(volProps.ulFlags & VDS_VF_HIDDEN)
						{
							DebugPrintf(SV_LOG_INFO, "Found Hidden volume, Name = %s\n", W2A(volProps.pwszName));
							pIVolume->ClearFlags(VDS_VF_HIDDEN);
						}
						
						if(volProps.ulFlags & VDS_VF_READONLY)
						{
							DebugPrintf(SV_LOG_INFO, "Found Read-Only volume, Name = %s\n", W2A(volProps.pwszName));
							pIVolume->ClearFlags(VDS_VF_READONLY);
						}

						if(volProps.ulFlags & VDS_VF_SHADOW_COPY)
						{
							DebugPrintf(SV_LOG_INFO, "Found shadow copy flag enbaled volume, Name = %s\n", W2A(volProps.pwszName));
							pIVolume->ClearFlags(VDS_VF_SHADOW_COPY);
						}
					}
					CoTaskMemFree(volProps.pwszName);
				}
				INM_SAFE_RELEASE(pIVolume);
			}
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumVoluems);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}


HRESULT CVdsHelper::CollectDynamicDisks(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	USES_CONVERSION;
	HRESULT hr;
	IEnumVdsObject *pIEnumDisks = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk = NULL;
	ULONG ulFetched = 0;

	list<VDS_OBJECT_ID> listDynDisks;

	bool isDynProvider = false;
	IVdsProvider *pProvider = NULL;
	hr = pPack->GetProvider(&pProvider);
	if(SUCCEEDED(hr) && (!pProvider))
		hr = E_UNEXPECTED;
	if(SUCCEEDED(hr))
	{
		VDS_PROVIDER_PROP ProviderProps;
		ZeroMemory(&ProviderProps,sizeof(ProviderProps));
		hr = pProvider->GetProperties(&ProviderProps);
		if(SUCCEEDED(hr))
		{
			if(ProviderProps.ulFlags & VDS_PF_DYNAMIC)
				isDynProvider = true;
		}
		CoTaskMemFree(ProviderProps.pwszName);
		CoTaskMemFree(ProviderProps.pwszVersion);
	}
	INM_SAFE_RELEASE(pProvider);

	if(!isDynProvider)	//missing disks related to only dynamic providers
		return hr;

	hr = pPack->QueryDisks(&pIEnumDisks);
	if(SUCCEEDED(hr) && (!pIEnumDisks))
	{
		hr = E_UNEXPECTED;
	}
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumDisks->Next(1,&pIUnknown,&ulFetched);
			if(0 == ulFetched)
				break;
			if(SUCCEEDED(hr)&&(!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsDisk,
					(void**)&pDisk);
			}
			if(SUCCEEDED(hr) && (!pDisk))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				//Disk properties
				string strDisk;
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{					
					if(diskProps.pwszName)
					{
						strDisk = W2A(diskProps.pwszName);
						m_listDisk.push_back(strDisk);
					}
					DebugPrintf(SV_LOG_INFO,"Dynamic Disk found, Disk Name : %s  And Disk Id : %s\n", strDisk.c_str() ,GUIDString(diskProps.id).c_str());
					
					CoTaskMemFree(diskProps.pwszDiskAddress);
					CoTaskMemFree(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDevicePath);
					CoTaskMemFree(diskProps.pwszFriendlyName);
					CoTaskMemFree(diskProps.pwszAdaptorName);
				}
			}
			INM_SAFE_RELEASE(pDisk);
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumDisks);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}


HRESULT CVdsHelper::CollectForeignDisks(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	USES_CONVERSION;
	HRESULT hr;
	IEnumVdsObject *pIEnumDisks = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk = NULL;
	ULONG ulFetched = 0;

	bool isForeignPack = false;
	//VDS Pack properties
	VDS_PACK_PROP packProps;
	hr = pPack->GetProperties(&packProps);

	if(SUCCEEDED(hr))
	{
		
		if(packProps.ulFlags & VDS_PKF_FOREIGN)
		{
			DebugPrintf(SV_LOG_INFO, "Got Foreign disk pack\n");
			isForeignPack = true;
		}
	}

	if(!isForeignPack)
		return hr;

	hr = pPack->QueryDisks(&pIEnumDisks);
	if(SUCCEEDED(hr) && (!pIEnumDisks))
	{
		hr = E_UNEXPECTED;
	}
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumDisks->Next(1,&pIUnknown,&ulFetched);
			if(0 == ulFetched)
				break;
			if(SUCCEEDED(hr)&&(!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsDisk,
					(void**)&pDisk);
			}
			if(SUCCEEDED(hr) && (!pDisk))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				//Disk properties
				string strDisk;
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{
					if(diskProps.pwszName)
					{
						strDisk = W2A(diskProps.pwszName);
						m_listDisk.push_back(strDisk);
					}
					DebugPrintf(SV_LOG_INFO, "Foriegn Disk found, Disk Name : %s  And Disk Id : %s\n", strDisk.c_str() ,GUIDString(diskProps.id).c_str());

					CoTaskMemFree(diskProps.pwszDiskAddress);
					CoTaskMemFree(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDevicePath);
					CoTaskMemFree(diskProps.pwszFriendlyName);
					CoTaskMemFree(diskProps.pwszAdaptorName);
				}
			}
			INM_SAFE_RELEASE(pDisk);
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumDisks);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}


HRESULT CVdsHelper::CollectOfflineDisks(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	USES_CONVERSION;
	HRESULT hr;
	IEnumVdsObject *pIEnumDisks = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk = NULL;
	ULONG ulFetched = 0;

	hr = pPack->QueryDisks(&pIEnumDisks);
	if(SUCCEEDED(hr) && (!pIEnumDisks))
	{
		hr = E_UNEXPECTED;
	}
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumDisks->Next(1,&pIUnknown,&ulFetched);
			if(0 == ulFetched)
				break;
			if(SUCCEEDED(hr)&&(!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsDisk,
					(void**)&pDisk);
			}
			if(SUCCEEDED(hr) && (!pDisk))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				//Disk properties
				string strDisk;
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{					
					if(VDS_DS_OFFLINE == diskProps.status)
					{
						strDisk = W2A(diskProps.pwszName);
						m_listDisk.push_back(strDisk);
					}
					
					CoTaskMemFree(diskProps.pwszDiskAddress);
					CoTaskMemFree(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDevicePath);
					CoTaskMemFree(diskProps.pwszFriendlyName);
					CoTaskMemFree(diskProps.pwszAdaptorName);
				}
			}
			INM_SAFE_RELEASE(pDisk);
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumDisks);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}



HRESULT CVdsHelper::RemoveMissingDisks(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hr;
	IEnumVdsObject *pIEnumDisks = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk = NULL;
	ULONG ulFetched = 0;

	list<VDS_OBJECT_ID> lstMissingDisks;

	bool isDynProvider = false;
	IVdsProvider *pProvider = NULL;
	hr = pPack->GetProvider(&pProvider);
	if(SUCCEEDED(hr) && (!pProvider))
		hr = E_UNEXPECTED;
	if(SUCCEEDED(hr))
	{
		VDS_PROVIDER_PROP ProviderProps;
		ZeroMemory(&ProviderProps,sizeof(ProviderProps));
		hr = pProvider->GetProperties(&ProviderProps);
		if(SUCCEEDED(hr))
		{
			if(ProviderProps.ulFlags & VDS_PF_DYNAMIC)
				isDynProvider = true;
		}
		CoTaskMemFree(ProviderProps.pwszName);
		CoTaskMemFree(ProviderProps.pwszVersion);
	}
	INM_SAFE_RELEASE(pProvider);

	if(!isDynProvider)	//missing disks related to only dynamic providers
		return hr;

	hr = pPack->QueryDisks(&pIEnumDisks);
	if(SUCCEEDED(hr) && (!pIEnumDisks))
	{
		hr = E_UNEXPECTED;
	}
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumDisks->Next(1,&pIUnknown,&ulFetched);
			if(0 == ulFetched)
				break;
			if(SUCCEEDED(hr)&&(!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsDisk,
					(void**)&pDisk);
			}
			if(SUCCEEDED(hr) && (!pDisk))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				//Disk properties
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{
					
					if(VDS_DS_MISSING == diskProps.status)
					{
						lstMissingDisks.push_back(diskProps.id);
					}
					
					CoTaskMemFree(diskProps.pwszDiskAddress);
					CoTaskMemFree(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDevicePath);
					CoTaskMemFree(diskProps.pwszFriendlyName);
					CoTaskMemFree(diskProps.pwszAdaptorName);
				}
			}
			INM_SAFE_RELEASE(pDisk);
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumDisks);

	//Remove missing disks
	list<VDS_OBJECT_ID>::const_iterator iterDiskId = lstMissingDisks.begin();
	while(lstMissingDisks.end() != iterDiskId)
	{
		hr = pPack->RemoveMissingDisk(*iterDiskId);
		if(S_OK == hr)
		{
			DebugPrintf(SV_LOG_INFO, "\t%s Success\n", GUIDString(*iterDiskId).c_str());
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "\t%s Failed, ERROR: %ld\n", GUIDString(*iterDiskId).c_str(), hr);
		}
		iterDiskId++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}

HRESULT CVdsHelper::DeleteVolume(const VDS_OBJECT_ID & objId)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hr;
	IUnknown *pIUnknown = NULL;
	IVdsVolume *pIVdsVol = NULL;
	hr = m_pIVdsService->GetObject(objId,VDS_OT_VOLUME,&pIUnknown);
	if(hr == VDS_E_OBJECT_NOT_FOUND)
	{
		DebugPrintf(SV_LOG_INFO, "\t%s  Object not found. Can not remove volume.\n", GUIDString(objId).c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return hr;
	}
	hr = pIUnknown->QueryInterface(IID_IVdsVolume,(void**)&pIVdsVol);
	if(SUCCEEDED(hr) && (!pIVdsVol))
		hr = E_UNEXPECTED;
	if(SUCCEEDED(hr))
	{
		hr = pIVdsVol->Delete(TRUE);
		if(hr == S_OK || hr == VDS_S_ACCESS_PATH_NOT_DELETED)
			DebugPrintf(SV_LOG_INFO, "\t%s Success\n", GUIDString(objId).c_str());
		else
			DebugPrintf(SV_LOG_ERROR,"\t%s Failed, ERROR %ld\n", GUIDString(objId).c_str(), hr);
	}
	INM_SAFE_RELEASE(pIVdsVol);
	INM_SAFE_RELEASE(pIUnknown);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}

bool CVdsHelper::IsDiskMissing(const VDS_OBJECT_ID & diskId)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = false;
	HRESULT hr;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk		= NULL;
	hr = m_pIVdsService->GetObject(diskId,VDS_OT_DISK,&pIUnknown);
	if(hr == VDS_E_OBJECT_NOT_FOUND)
	{
		DebugPrintf(SV_LOG_INFO,"\t%s Object not found. Can not get disk porps\n", GUIDString(diskId).c_str());
		DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
		return bRet;
	}
	hr = pIUnknown->QueryInterface(IID_IVdsDisk,(void**)&pDisk);
	if(SUCCEEDED(hr) && (!pDisk))
		hr = E_UNEXPECTED;
	if(SUCCEEDED(hr))
	{
		VDS_DISK_PROP diskProps;
		ZeroMemory(&diskProps,sizeof(diskProps));
		hr = pDisk->GetProperties(&diskProps);
		if(SUCCEEDED(hr))
		{
			
			if(VDS_DS_MISSING == diskProps.status)
			{
				bRet = true;
			}
			
			CoTaskMemFree(diskProps.pwszDiskAddress);
			CoTaskMemFree(diskProps.pwszName);
			CoTaskMemFree(diskProps.pwszDevicePath);
			CoTaskMemFree(diskProps.pwszFriendlyName);
			CoTaskMemFree(diskProps.pwszAdaptorName);
		}
	}
	INM_SAFE_RELEASE(pDisk);
	INM_SAFE_RELEASE(pIUnknown);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}

bool CVdsHelper::AllDisksMissig(std::list<VDS_DISK_EXTENT> & lstExtents)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	bool bRet = true;
	std::list<VDS_DISK_EXTENT>::const_iterator iterExtent = lstExtents.begin();
	while(iterExtent != lstExtents.end())
	{
		if(!IsDiskMissing(iterExtent->diskId))
		{
			bRet = false;
			break;
		}
		iterExtent++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return bRet;
}

HRESULT CVdsHelper::ProcessPlexes(IVdsVolume *pIVolume, std::list<VDS_DISK_EXTENT> & lstExtents)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hr;
	IEnumVdsObject *pIEnumPlex = NULL;
	IVdsVolumePlex *pPlex = NULL;
	IUnknown *pIUnknown = NULL;
	ULONG ulFetched = 0;
	hr = pIVolume->QueryPlexes(&pIEnumPlex);
	if(SUCCEEDED(hr) && (!pIEnumPlex))
		hr = E_UNEXPECTED;
	if(SUCCEEDED(hr))
	{
		lstExtents.clear();
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumPlex->Next(1,&pIUnknown,&ulFetched);
			if(0==ulFetched || S_FALSE == hr)
				break;
			if(SUCCEEDED(hr) && (!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsVolumePlex, (void**)&pPlex);
				if(SUCCEEDED(hr) && (!pPlex))
					hr = E_UNEXPECTED;
				if(SUCCEEDED(hr))
				{
					VDS_DISK_EXTENT *pDiskExtnts;
					LONG lNumExtents = 0;
					hr = pPlex->QueryExtents(&pDiskExtnts,&lNumExtents);
					if(SUCCEEDED(hr))
					{
						for(LONG l=0;l<lNumExtents;l++)
						{
							lstExtents.push_back(pDiskExtnts[l]);
						}
					}
					CoTaskMemFree(pDiskExtnts);
				}
				INM_SAFE_RELEASE(pPlex);
			}
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumPlex);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}

HRESULT CVdsHelper::DeleteFailedVolumes(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	HRESULT hr = S_OK;
	IEnumVdsObject *pIEnumVoluems = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsVolume *pIVolume = NULL;
	ULONG ulFetched = 0;

	std::list<VDS_OBJECT_ID> lstFailedVols;

	hr = pPack->QueryVolumes(&pIEnumVoluems);
	if(SUCCEEDED(hr) && (!pIEnumVoluems))
		hr = E_UNEXPECTED;
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumVoluems->Next(1,&pIUnknown,&ulFetched);
			if(0 == ulFetched || hr == S_FALSE)
				break;
			if(SUCCEEDED(hr) && (!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsVolume,
					(void**)&pIVolume);
				if(SUCCEEDED(hr) && (!pIVolume))
					hr = E_UNEXPECTED;
				if(SUCCEEDED(hr))
				{
					VDS_VOLUME_PROP volProps;
					ZeroMemory(&volProps,sizeof(volProps));
					hr = pIVolume->GetProperties(&volProps);
					if(SUCCEEDED(hr))
					{
						//Extents
						std::list<VDS_DISK_EXTENT> lstExtents;
						ProcessPlexes(pIVolume,lstExtents);
						if(AllDisksMissig(lstExtents))
							lstFailedVols.push_back(volProps.id);
					}
					CoTaskMemFree(volProps.pwszName);
				}
				INM_SAFE_RELEASE(pIVolume);
			}
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumVoluems);

	//Delete filed volumes
	DeleteFailedVolumes(lstFailedVols);

	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
	return hr;
}

void CVdsHelper::DeleteFailedVolumes(std::list<VDS_OBJECT_ID> & lstFailedVols)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	std::list<VDS_OBJECT_ID>::const_iterator iterVol = lstFailedVols.begin();
	while(iterVol != lstFailedVols.end())
	{
		DeleteVolume(*iterVol);
		iterVol++;
	}
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


void CVdsHelper::CollectDiskScsiIDs(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	USES_CONVERSION;
	HRESULT hr;
	IEnumVdsObject *pIEnumDisks = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk = NULL;
	ULONG ulFetched = 0;

	hr = pPack->QueryDisks(&pIEnumDisks);
	if(SUCCEEDED(hr) && (!pIEnumDisks))
	{
		hr = E_UNEXPECTED;
	}
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumDisks->Next(1,&pIUnknown,&ulFetched);
			if(0 == ulFetched)
				break;
			if(SUCCEEDED(hr)&&(!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsDisk,
					(void**)&pDisk);
			}
			if(SUCCEEDED(hr) && (!pDisk))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				string strDisk , strScsiID;
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{
					if(diskProps.pwszName)
						strDisk = W2A(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDiskAddress);
					CoTaskMemFree(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDevicePath);
					CoTaskMemFree(diskProps.pwszFriendlyName);
					CoTaskMemFree(diskProps.pwszAdaptorName);

					VDS_LUN_INFORMATION diskLunInfo;
					ZeroMemory(&diskLunInfo,sizeof(diskLunInfo));
					hr = pDisk->GetIdentificationData(&diskLunInfo);
					if(SUCCEEDED(hr))
					{
						if(diskLunInfo.m_szSerialNumber)
							strScsiID = diskLunInfo.m_szSerialNumber;
						
						CoTaskMemFree(diskLunInfo.m_szProductId);
						CoTaskMemFree(diskLunInfo.m_szProductRevision);
						CoTaskMemFree(diskLunInfo.m_szSerialNumber);
						CoTaskMemFree(diskLunInfo.m_szVendorId);
						for(ULONG n=0; n<diskLunInfo.m_cInterconnects; n++)
						{
							CoTaskMemFree(diskLunInfo.m_rgInterconnects[n].m_pbAddress);
							CoTaskMemFree(diskLunInfo.m_rgInterconnects[n].m_pbPort);
						}
						if(!strDisk.empty() && !strScsiID.empty())
							m_helperMap[strDisk] = strScsiID;
					}
				}
			}
			INM_SAFE_RELEASE(pDisk);
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumDisks);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}


void CVdsHelper::CollectDiskSignatures(IVdsPack *pPack)
{
	DebugPrintf(SV_LOG_DEBUG,"Entering %s\n",__FUNCTION__);
	USES_CONVERSION;
	HRESULT hr;
	IEnumVdsObject *pIEnumDisks = NULL;
	IUnknown *pIUnknown = NULL;
	IVdsDisk *pDisk = NULL;
	ULONG ulFetched = 0;

	hr = pPack->QueryDisks(&pIEnumDisks);
	if(SUCCEEDED(hr) && (!pIEnumDisks))
	{
		hr = E_UNEXPECTED;
	}
	if(SUCCEEDED(hr))
	{
		while(true)
		{
			ulFetched = 0;
			hr = pIEnumDisks->Next(1,&pIUnknown,&ulFetched);
			if(0 == ulFetched)
				break;
			if(SUCCEEDED(hr)&&(!pIUnknown))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				hr = pIUnknown->QueryInterface(IID_IVdsDisk,
					(void**)&pDisk);
			}
			if(SUCCEEDED(hr) && (!pDisk))
				hr = E_UNEXPECTED;
			if(SUCCEEDED(hr))
			{
				string strDisk;
				VDS_DISK_PROP diskProps;
				ZeroMemory(&diskProps,sizeof(diskProps));
				hr = pDisk->GetProperties(&diskProps);
				if(SUCCEEDED(hr))
				{
					if(diskProps.pwszName)
						strDisk = W2A(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDiskAddress);
					CoTaskMemFree(diskProps.pwszName);
					CoTaskMemFree(diskProps.pwszDevicePath);
					CoTaskMemFree(diskProps.pwszFriendlyName);
					CoTaskMemFree(diskProps.pwszAdaptorName);

					VDS_LUN_INFORMATION diskLunInfo;
					ZeroMemory(&diskLunInfo,sizeof(diskLunInfo));
					hr = pDisk->GetIdentificationData(&diskLunInfo);
					if(SUCCEEDED(hr))
					{
						if((FILE_DEVICE_DISK == diskProps.dwDeviceType) && !strDisk.empty())
						{
							wchar_t strGUID[128] = {0};
							StringFromGUID2(diskLunInfo.m_diskSignature,strGUID,128);
							m_helperMap[strDisk] = string(W2A(strGUID));
						}
						
						CoTaskMemFree(diskLunInfo.m_szProductId);
						CoTaskMemFree(diskLunInfo.m_szProductRevision);
						CoTaskMemFree(diskLunInfo.m_szSerialNumber);
						CoTaskMemFree(diskLunInfo.m_szVendorId);
						for(ULONG n=0; n<diskLunInfo.m_cInterconnects; n++)
						{
							CoTaskMemFree(diskLunInfo.m_rgInterconnects[n].m_pbAddress);
							CoTaskMemFree(diskLunInfo.m_rgInterconnects[n].m_pbPort);
						}
					}
				}
			}
			INM_SAFE_RELEASE(pDisk);
			INM_SAFE_RELEASE(pIUnknown);
		}
	}
	INM_SAFE_RELEASE(pIEnumDisks);
	DebugPrintf(SV_LOG_DEBUG,"Exiting %s\n",__FUNCTION__);
}

#endif /* WIN32 */


// ===========================
// A2E support
//
// ===========================

#ifndef WIN32

bool ChangeVmConfig::MakeChangesPostRollbackVmWare(const std::string & srcVmName)
{
	bool rv = false;
	return rv;
}

#else

bool ChangeVmConfig::MakeChangesPostRollbackVmWare(const std::string & srcVmName)
{
	TRACE_FUNC_BEGIN;

	bool rv = false;


	try
	{

		// step 1: Determine the boot disk
		// step 2: Online the boot disk
		// step 3: Find the volume guid of the boot partition within boot disk and mount it
		// step 4: carry out the post rollback changes on the mounted partition

		InitFailbackProperties(srcVmName);
		ParseBootDiskDetails();
		ParseOsTypeInformation();
		CollectAllDiskInformation();
		FindSrcBootDisk();
		FetchOsDiskMBR();
		OnlineRecoveredVMBootDisk();
		Sleep(10000);
		//PrepareSourceOSDriveWithRetries();
		//Sleep(10000);
		PrepareSourceRegistryHivesWithRetries();

		// todo: revert the drivers to their original state
		// currently we are not storing the original state during forward protection
		// once we have that we should be able to revert

		// SetServicesToOriginalType();

		ResetInvolfltParameters();
		RestoreSanPolicy();

		if (IsRecoveredVmV2GA())
		{
			MakeChangesForInMageServices(true);
			SetBootupScript();
		}
		else
		{
			// No need to make any changes to inmage services here, the agent services are 
			// now aware of recovery and pauses its state untle bootup script command completes.
			MakeChangesForInMageServices(false);
			SetBootupScriptOptions();
		}

		rv = true;
	}
	catch (const std::exception &e)
	{
		DebugPrintf(SV_LOG_ERROR, e.what());
	}

	if (!PerformCleanUpActions())
		rv = false;

	TRACE_FUNC_END;

	return rv;
}


void ChangeVmConfig::InitFailbackProperties(const std::string & srcVmName)
{
	TRACE_FUNC_BEGIN;

	m_sourceVMName = srcVmName;
	m_bWinNT = false;

	m_disksdetails.clear();
	m_srcOsVolDiskExtent.disk_id.clear();
	m_srcOsVolDiskExtent.offset = 0;
	m_srcOsVolDiskExtent.length = 0;

	m_OsType.clear();
	m_srcOSVolGuid.clear();
	m_srcOSVolumeName.clear();
	m_osVolMountPath.clear();

	m_srcBootDisk.displayName.clear();
	m_srcBootDisk.scsiId.clear();
	m_srcBootDisk.capacity = 0;

	m_bOsDiskOnlined = false;
	m_bOsVolMounted = false;
	m_bSystemHiveLoaded = false;
	m_bSoftwareHiveLoaded = false;

	m_osDiskMBRFilePath = m_strDatafolderPath + m_sourceVMName + std::string(OSDISK_MBR_FILE_PATH);
	m_osMBR.Reset(MAX_MBR_LENGTH, 4096);

	TRACE_FUNC_END;
}


void ChangeVmConfig::ParseBootDiskDetails()
{
	TRACE_FUNC_BEGIN;
	map<string, string>::iterator iter = m_mapOfVmInfoForPostRollback.find("boot_disk_drive_extent");

	if (iter != m_mapOfVmInfoForPostRollback.end())
	{
		std::string extentInfo = iter->second;
		svector_t  tokens = CDPUtil::split(extentInfo, ":",3);
		if (tokens.size() != 3) {
			std::stringstream e;
			e << "VM:" << m_sourceVMName << "invalid input " << extentInfo << " for boot_disk_drive_extent property.\n";
			throw std::exception(e.str().c_str());
		}
		m_srcOsVolDiskExtent.disk_id = tokens[0];
		m_srcOsVolDiskExtent.offset = boost::lexical_cast<SV_LONGLONG>(tokens[1]);
		m_srcOsVolDiskExtent.length = boost::lexical_cast<SV_LONGLONG>(tokens[2]);
	}
	else {

		std::stringstream e;
		e << "VM:" << m_sourceVMName << " boot_disk_drive_extent property is not recieved from CS.\n";
		throw std::exception(e.str().c_str());
	}

	iter = m_mapOfVmInfoForPostRollback.find("SystemVolume");
	if (iter != m_mapOfVmInfoForPostRollback.end())
	{
		m_srcOSVolumeName = iter->second;
	}else
	{
		std::stringstream e;
		e << "VM:" << m_sourceVMName << " SystemVolume property is not recieved from CS.\n";
		throw std::exception(e.str().c_str());
	}
	
	TRACE_FUNC_END;
}


void ChangeVmConfig::ParseOsTypeInformation()
{
	TRACE_FUNC_BEGIN;

	map<string, string>::iterator iter = m_mapOfVmInfoForPostRollback.find("OperatingSystem");
	if (iter != m_mapOfVmInfoForPostRollback.end())
	{
		m_OsType = iter->second;
		DebugPrintf(SV_LOG_DEBUG, "OS Type : %s\n", m_OsType.c_str());
	}
	else
	{
		std::stringstream e;
		e << "VM:" << m_sourceVMName << " OperatingSystem property is not recieved from CS.\n";
		throw std::exception(e.str().c_str());
	}

	TRACE_FUNC_END;
}


void ChangeVmConfig::CollectAllDiskInformation()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
	bool rv = true;


	if (S_FALSE == InitCOM()) {
		throw std::exception("InitCom() failed.\n");
	}

	m_disksdetails.clear();
	WmiDiskRecordProcessor p(&m_disksdetails);
	GenericWMI gwmi(&p);
	SVERROR sv = gwmi.init();
	if (sv != SVS_OK)
	{
		throw std::exception("initializating generic wmi failed.\n");
	}
	else
	{
		gwmi.GetData("Win32_DiskDrive");
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}


void ChangeVmConfig::FindSrcBootDisk()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	std::string bootDiskScsiId = m_srcOsVolDiskExtent.disk_id;

	if (!FindMatchingDisk(bootDiskScsiId, m_srcBootDisk)){

		std::stringstream e;
		e << "VM:" << m_sourceVMName << " unable to find boot disk with id:" << bootDiskScsiId << ".\n";
		throw std::exception(e.str().c_str());
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}



bool ChangeVmConfig::FindMatchingDisk(const std::string &scsiId, Diskinfo_t & diskinfo)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	bool rv = false;
	std::vector<Diskinfo_t>::const_iterator dIter = m_disksdetails.cbegin();
	for (; dIter != m_disksdetails.cend(); ++dIter)
	{
		if (scsiId == dIter->scsiId)
		{
			rv = true;
			diskinfo = *dIter;
			break;
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return rv;
}

void ChangeVmConfig::FetchOsDiskMBR()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	std::stringstream e;
	Disk d(m_srcBootDisk.scsiId, m_srcBootDisk.displayName, VolumeSummary::SCSIID);

	BasicIo::BioOpenMode openMode = BasicIo::BioReadExisting |
		BasicIo::BioShareAll |
		BasicIo::BioBinary |
		BasicIo::BioNoBuffer |
		BasicIo::BioWriteThrough;

	if (SV_SUCCESS != d.Open(openMode))
	{
		e << "VM:" << m_sourceVMName << " unable to open boot disk with id:" << m_srcBootDisk.scsiId
			<< " , device name:" << m_srcBootDisk.displayName << ".\n";

		throw std::exception(e.str().c_str());
	}

	if (VOLUME_HIDDEN == d.GetDeviceState())
	{
		if (MAX_MBR_LENGTH != d.Read(m_osMBR.Get(), MAX_MBR_LENGTH))
		{
			e << "VM:" << m_sourceVMName << " unable to read boot disk MBR.\n";
			throw std::exception(e.str().c_str());
		}

		PersistOsDiskMBR();
	}
	else
	{
		FetchPreviouslySavedMBR();
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}

void ChangeVmConfig::ResetOsDiskMBR()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	std::stringstream e;
	Disk d(m_srcBootDisk.scsiId, m_srcBootDisk.displayName, VolumeSummary::SCSIID);

	BasicIo::BioOpenMode openMode = BasicIo::BioRWExisitng |
		BasicIo::BioShareAll |
		BasicIo::BioBinary |
		BasicIo::BioNoBuffer |
		BasicIo::BioWriteThrough;

	if (SV_SUCCESS != d.Open(openMode))
	{
		e << "VM:" << m_sourceVMName << " unable to open boot disk with id:" << m_srcBootDisk.scsiId
			<< " , device name:" << m_srcBootDisk.displayName << ".\n";

		throw std::exception(e.str().c_str());
	}

	if (MAX_MBR_LENGTH != d.Write(m_osMBR.Get(), MAX_MBR_LENGTH))
	{
		e << "VM:" << m_sourceVMName << " unable to reset boot disk MBR.\n";
		throw std::exception(e.str().c_str());
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}


void ChangeVmConfig::FetchPreviouslySavedMBR()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	std::stringstream e;
	BasicIo bIo;

	BasicIo::BioOpenMode openMode = BasicIo::BioReadExisting |
		BasicIo::BioShareAll |
		BasicIo::BioBinary |
		BasicIo::BioNoBuffer |
		BasicIo::BioWriteThrough;

	bIo.Open(m_osDiskMBRFilePath, openMode);

	if (!bIo.Good())
	{
		e << "VM:" << m_sourceVMName
			<< " open " << m_osDiskMBRFilePath
			<< " failed with error: " << ACE_OS::last_error();

		throw std::exception(e.str().c_str());
	}

	if (MAX_MBR_LENGTH != bIo.Read(m_osMBR.Get(), MAX_MBR_LENGTH))
	{
		e << "VM:" << m_sourceVMName << " unable to save boot disk MBR to file"
			<< m_osDiskMBRFilePath << "\n";
		throw std::exception(e.str().c_str());
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}

void ChangeVmConfig::PersistOsDiskMBR()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	std::stringstream e;
	BasicIo bIo;

	BasicIo::BioOpenMode openMode = BasicIo::BioOverwrite |
		BasicIo::BioRW |
		BasicIo::BioShareAll |
		BasicIo::BioBinary |
		BasicIo::BioNoBuffer |
		BasicIo::BioWriteThrough;

	bIo.Open(m_osDiskMBRFilePath, openMode);

	if (!bIo.Good())
	{
		e << "VM:" << m_sourceVMName
			<< " open " << m_osDiskMBRFilePath
			<< " failed with error: " << ACE_OS::last_error();

		throw std::exception(e.str().c_str());
	}

	if (MAX_MBR_LENGTH != bIo.Write(m_osMBR.Get(), MAX_MBR_LENGTH))
	{
		e << "VM:" << m_sourceVMName << " unable to save boot disk MBR to file"
			<< m_osDiskMBRFilePath << "\n";
		throw std::exception(e.str().c_str());
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}

void ChangeVmConfig::OnlineRecoveredVMBootDisk()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	if (!OnlineDisk(m_srcBootDisk))
	{
	
		std::stringstream e;
		e << "VM:" << m_sourceVMName << " unable to online boot disk with id:" << m_srcBootDisk.scsiId 
			<< " , device name:" << m_srcBootDisk.displayName << ".\n";

		throw std::exception(e.str().c_str());
	}

	m_bOsDiskOnlined = true;
	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}

bool ChangeVmConfig::OnlineDisk(const Diskinfo_t & diskinfo)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	Disk d(diskinfo.scsiId, diskinfo.displayName, VolumeSummary::SCSIID);

	BasicIo::BioOpenMode openMode = BasicIo::BioRWExisitng |
		BasicIo::BioShareAll |
		BasicIo::BioBinary |
		BasicIo::BioNoBuffer |
		BasicIo::BioWriteThrough;

	if (SV_SUCCESS != d.Open(openMode))
		return false;

	if (!d.OnlineRW())
		return false;

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return true;
}


void ChangeVmConfig::OfflineRecoveredVMBootDisk()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	if (!OfflineDisk(m_srcBootDisk)){

		std::stringstream e;
		e << "VM:" << m_sourceVMName << " unable to offline boot disk with id:" << m_srcBootDisk.scsiId
			<< " , device name:" << m_srcBootDisk.capacity << ".\n";

		throw std::exception(e.str().c_str());
	}

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}

bool ChangeVmConfig::OfflineDisk(const Diskinfo_t & diskinfo)
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	Disk d(diskinfo.scsiId, diskinfo.displayName, VolumeSummary::SCSIID);

	BasicIo::BioOpenMode openMode = BasicIo::BioRWExisitng |
		BasicIo::BioShareAll |
		BasicIo::BioBinary |
		BasicIo::BioNoBuffer |
		BasicIo::BioWriteThrough;

	if (SV_SUCCESS != d.Open(openMode))
		return false;

	if (!d.OfflineRW())
		return false;

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
	return true;
}

void ChangeVmConfig::OfflineOnlineRecoveredVMBootDisk()
{
	DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

	rescanIoBuses();
	OfflineRecoveredVMBootDisk();
	OnlineRecoveredVMBootDisk();
	Sleep(30000);
	rescanIoBuses();

	DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}

/*
Method      : PrepareSourceOSDriveWithRetries

Description : Calls PrepareSourceOSDrive till it suceeds or all exhausted upto max attempts

On success - m_osVolMountPath: Filled with source system volume mountpoint/drive-letter

Return      : none

*/
void ChangeVmConfig::PrepareSourceOSDriveWithRetries()
{
	TRACE_FUNC_BEGIN;

	bool bSuceeded = false;

	const int MaxAttempts = 10;
	const int RetryIntervalInMs = 10000;
	int attempt = 0;

	while (!bSuceeded)
	{
		
		try
		{
			++attempt;
			PrepareSourceOSDrive();
			bSuceeded = true;
		}
		catch (const std::exception &e)
		{
			if (attempt == MaxAttempts)
				throw;

			DebugPrintf(SV_LOG_ERROR, "Attempt: %d %s.\n", attempt, e.what());
			Sleep(RetryIntervalInMs);
		}
		
	}

	TRACE_FUNC_END;

}


/*
Method      : PrepareSourceOSDrive

Description : Identifies the source system partition of recovered vm using source system
volume drive extents information. And also find the mountpoint or drive-letter
if available for the system partion, if not assigns a unique mount point for the
partition.

On success - m_osVolMountPath: Filled with source system volume mountpoint/drive-letter

Return      : true on success, otherwise false.

*/
void ChangeVmConfig::PrepareSourceOSDrive()
{
	TRACE_FUNC_BEGIN;

	std::stringstream errMsg;


	if (ERROR_SUCCESS != GetSourceOSVolumeGuid(m_srcOsVolDiskExtent, m_srcOSVolGuid))
	{
		errMsg << "VM: " << m_sourceVMName << "Source OS volume not found.\n";
		throw std::exception(errMsg.str().c_str());
	}

	std::list<std::string> mountPoints;
	if (ERROR_SUCCESS != GetVolumeMountPoints(m_srcOSVolGuid, mountPoints))
	{
		errMsg << "VM: " << m_sourceVMName << "Could not get mount points for OS volume.\n";
		throw std::exception(errMsg.str().c_str());
	}

	if (mountPoints.empty())
	{
		//
		// No mount-point or drive letter is assigned for os volume.
		// Assing a mount point.
		//
		if (ERROR_SUCCESS != AutoSetMountPoint(m_srcOSVolGuid, m_osVolMountPath))
		{
			errMsg << "VM: " << m_sourceVMName << "Could not assign mount point for OS volume.\n";
			throw std::exception(errMsg.str().c_str());

		}

		Sleep(10000);

		if (ERROR_SUCCESS != GetVolumeMountPoints(m_srcOSVolGuid, mountPoints))
		{
			errMsg << "VM: " << m_sourceVMName << "Could not get mount points for OS volume.\n";
			throw std::exception(errMsg.str().c_str());
		}

		if (mountPoints.empty())
		{
			errMsg << "VM: " << m_sourceVMName << "No mount point assigned for OS volume.\n";
			throw std::exception(errMsg.str().c_str());
		}
	}
	
	//
	// Get the first mount point from the list
	//
	m_osVolMountPath = *mountPoints.begin();
	m_bOsVolMounted = true;

	TRACE_FUNC_END;

}

/*
Method      : GetSourceOSVolumeGuid

Description : Identifies the source os volume on recovered vm using source os volume disk extent.

Parameters  : [in] osVolExtent: disk extent of source volume ( this information should be collected on source)
[out] volumeGuid : volume guid path of identified source os volume on hydration-vm.

Return      : ERROR_SUCCESS -> on success
win32 error   -> on failure.

*/
DWORD ChangeVmConfig::GetSourceOSVolumeGuid(const disk_extent & osVolExtent, std::string& volumeGuid)
{
	TRACE_FUNC_BEGIN;
	DWORD dwRet = ERROR_SUCCESS;
	HANDLE hVolume = INVALID_HANDLE_VALUE;
	TCHAR volumeName[MAX_PATH] = { 0 };
	bool bFoundVolume = false;

	hVolume = FindFirstVolume(volumeName, ARRAYSIZE(volumeName));
	if (INVALID_HANDLE_VALUE == hVolume)
	{
		dwRet = GetLastError();
		TRACE_ERROR("Could not get volume handle. FindFirstVolume failed with error %lu\n", dwRet);
		return dwRet;
	}

	do
	{
		TRACE_INFO("Verifying disk extents for the volume %s\n", volumeName);

		//Get volume disk extents and compare
		disk_extents_t volExtents;
		if (ERROR_SUCCESS == GetVolumeDiskExtents(volumeName, volExtents))
		{
			if (volExtents.size() == 1)
			{

				std::string strDiskID = osVolExtent.disk_id;

				// 
				// Compare the disk scsi id and OS volume start offset.
				// The partition length is not considering as it may change if the volume extend.
				//

				if (0 == lstrcmpi(volExtents[0].disk_id.c_str(),
					strDiskID.c_str())
					&&
					volExtents[0].offset == osVolExtent.offset)
				{
					//
					// If the OS volume found then stop volume enumeration and return the verified volume name.
					//
					bFoundVolume = true;
					TRACE_INFO("Volume %s is matching with Source OS disk extents\n", volumeName);
					volumeGuid = volumeName;
					break;
				}

			}
		}
		else
		{
			TRACE_ERROR("Could not get the disk extents for the volume %s\n", volumeName);
			//
			// Ignoring GetVolumeDiskExtents() failure and proceeding for next volume. 
			// If the required volume does not found then the function will return failure at end.
			//
		}

		//Get Next Volume
		SecureZeroMemory(volumeName, ARRAYSIZE(volumeName));
		if (!FindNextVolume(hVolume, volumeName, ARRAYSIZE(volumeName)))
		{
			dwRet = GetLastError();
			if (dwRet != ERROR_NO_MORE_FILES)
			{
				TRACE_ERROR("Could not enumerate all volumes. FindNextVolume failed with error %lu\n", dwRet);
			}
			else
			{
				TRACE_ERROR("Reached end of volumes enumeration. The Source OS volume was not found\n");
			}

			break;
		}

	} while (true);

	FindVolumeClose(hVolume);

	if (!bFoundVolume && dwRet == ERROR_SUCCESS)
		dwRet = ERROR_FILE_NOT_FOUND;

	TRACE_FUNC_END;
	return dwRet;
}

/*
Method      : GetVolumeDiskExtents

Description : Retrieves the volume disk extents of a given volume (volumeGuid), and the disk extent will have
disk signature/guid, starting offset and extent lenght.

Parameters  : [in] volumeGuid: volume guid paht of a volume
[out] diskExtents: filled with disk extents of a volume on success.

Return      : ERROR_SUCCESS -> on success
win32 error   -> on failure.

*/
DWORD ChangeVmConfig::GetVolumeDiskExtents(const std::string& volumeGuid, disk_extents_t& diskExtents)
{
	TRACE_FUNC_BEGIN;
	DWORD dwRet = ERROR_SUCCESS;
	HANDLE hVolume = INVALID_HANDLE_VALUE;
	DWORD dwBytesReturned = 0;
	DWORD cbInBuff = 0;
	PVOLUME_DISK_EXTENTS pVolDiskExt = NULL;
	std::string volumeName = volumeGuid;

	if (volumeName.empty())
	{
		TRACE_ERROR("%s:Line %d: ERROR: Volume Name can not be empty.\n", __FUNCTION__, __LINE__);
		return ERROR_INVALID_PARAMETER;
	}

	do {

		//Remove trailing "\" if exist.
		if (volumeName[volumeName.length() - 1] == '\\')
			volumeName.erase(volumeName.length() - 1);

		//Get the volume handle
		HANDLE hVolume = INVALID_HANDLE_VALUE;
		hVolume = CreateFile(
			volumeName.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
			NULL
			);

		if (hVolume == INVALID_HANDLE_VALUE)
		{
			dwRet = GetLastError();
			TRACE_ERROR("%s:Line %d: Could not open the volume- %s. CreateFile failed with Error Code - %lu \n",
				__FUNCTION__, __LINE__,
				volumeName.c_str(),
				dwRet);
			break;
		}

		//  Allocate default buffer sizes. 
		//  If the volume is created on basic disk then there will be only one disk extent for the volume, 
		//     and this default buffer will be enough to accomodate extent info.
		cbInBuff = sizeof(VOLUME_DISK_EXTENTS);
		pVolDiskExt = (PVOLUME_DISK_EXTENTS)malloc(cbInBuff);
		if (NULL == pVolDiskExt)
		{
			dwRet = ERROR_OUTOFMEMORY;
			TRACE_ERROR("%s:Line %d: Out of memory\n", __FUNCTION__, __LINE__);
			break;
		}

		if (!DeviceIoControl(
			hVolume,
			IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
			NULL,
			0,
			pVolDiskExt,
			cbInBuff,
			&dwBytesReturned,
			NULL
			))
		{
			dwRet = GetLastError();

			if (dwRet == ERROR_MORE_DATA)
			{
				//  If the volume is created on dynamic disk then there will be 
				//  posibility that more than one extent exist for the volume.
				//  Calculate the size required to accomodate all extents.
				cbInBuff = FIELD_OFFSET(VOLUME_DISK_EXTENTS, Extents) +
					pVolDiskExt->NumberOfDiskExtents * sizeof(DISK_EXTENT);

				//  Re-allocate the memory to new size
				pVolDiskExt = (PVOLUME_DISK_EXTENTS)realloc(pVolDiskExt, cbInBuff);
				if (NULL == pVolDiskExt)
				{
					dwRet = ERROR_OUTOFMEMORY;
					TRACE_ERROR("%s:Line %d: Out of memory\n", __FUNCTION__, __LINE__);
					break;
				}

				if (!DeviceIoControl(
					hVolume,
					IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
					NULL,
					0,
					pVolDiskExt,
					cbInBuff,
					&dwBytesReturned,
					NULL
					))
				{
					dwRet = GetLastError();
					TRACE_ERROR("%s:Line %d: Could not get the volume disk extents. DeviceIoControl failed with Error %lu\n",
						__FUNCTION__,
						__LINE__,
						dwRet);
					break;
				}
				else
				{
					dwRet = ERROR_SUCCESS;
				}
			}
			else
			{
				TRACE_ERROR("%s:Line %d: Could not get the volume disk extents. DeviceIoControl failed with Error %lu\n",
					__FUNCTION__,
					__LINE__,
					dwRet);
				break;
			}
		}

		//Fill disk_extents_t structure with retrieved disk extents
		for (DWORD i_extent = 0; i_extent < pVolDiskExt->NumberOfDiskExtents; i_extent++)
		{
			//Here disk_id will be a signature if disk is MBR type, a GUID if GPT type.
			std::string disk_id;
			GetDiskId(pVolDiskExt->Extents[i_extent].DiskNumber, disk_id);

			disk_extent extent(
				disk_id,
				pVolDiskExt->Extents[i_extent].StartingOffset.QuadPart,
				pVolDiskExt->Extents[i_extent].ExtentLength.QuadPart
				);

			std::stringstream extentOut;
			extentOut << "Disk-Id: " << extent.disk_id
				<< ", Offset: " << extent.offset
				<< ", Length: " << extent.length;

			TRACE_INFO("Extent Info: %s\n", extentOut.str().c_str());

			diskExtents.push_back(extent);
		}

	} while (false);

	if (NULL != pVolDiskExt)
		free(pVolDiskExt);

	if (INVALID_HANDLE_VALUE != hVolume)
		CloseHandle(hVolume);

	TRACE_FUNC_END;
	return dwRet;
}

/*
Method      : GetDiskId

Description : Retrieves the scsi id of a give disk name.

Parameters  : [in] dwDiskNum : Disk serial number
[out] diskID   : filled with scsi id of the disk.

Return      : none

*/
void ChangeVmConfig::GetDiskId(DWORD dwDiskNum, std::string& diskID)
{
	std::stringstream diskName;
	diskName << "\\\\.\\PhysicalDrive" << dwDiskNum;

	DeviceIDInformer deviceIDInformer;
	diskID = deviceIDInformer.GetID(diskName.str().c_str());
}



/*
Method      : GetVolumeMountPoints

Description : Retrieves the list of mountpoints/driveleters of a give volume (volumeGuid)

Parameters  : [in] volumeGuid: Volume Guid path of a volume
[out] mountPoints: List of mount-points/drive-letters of the volume.

Return      : ERROR_SUCCESS -> on success
win32 error   -> on failure.

*/
DWORD ChangeVmConfig::GetVolumeMountPoints(const std::string& volumeGuid, std::list<std::string>& mountPoints)
{
	TRACE_FUNC_BEGIN;
	BOOST_ASSERT(!volumeGuid.empty());

	DWORD dwRet = ERROR_SUCCESS;
	BOOL bSuccess = FALSE;
	DWORD ccBuffer = MAX_PATH + 1;
	TCHAR *buffer = NULL;
	do
	{
		buffer = (TCHAR*)malloc(ccBuffer * sizeof(TCHAR));
		if (NULL == buffer)
		{
			dwRet = ERROR_OUTOFMEMORY;
			TRACE_ERROR("Out of memory\n");
			break;
		}
		RtlSecureZeroMemory(buffer, ccBuffer * sizeof(TCHAR));

		bSuccess = GetVolumePathNamesForVolumeName(volumeGuid.c_str(), buffer, ccBuffer, &ccBuffer);
		if (!bSuccess)
		{
			dwRet = GetLastError();
			if (ERROR_MORE_DATA == dwRet)
			{
				free(buffer); buffer = NULL;
				dwRet = ERROR_SUCCESS;
				continue;
			}

			TRACE_ERROR("GetVolumePathNamesForVolumeName failed with error %lu for the volume %s\n",
				dwRet,
				volumeGuid.c_str());
			break;
		}

		for (TCHAR* path = buffer; path[0] != '\0'; path += lstrlen(path) + 1)
			mountPoints.push_back(path);

		break;

	} while (true);

	if (buffer) free(buffer);

	TRACE_FUNC_END;
	return dwRet;
}

/*
Method      : AutoSetMountPoint

Description : Sets a mount-point for a given volume (volumeGuid) and returns the mountpoint

Parameters  : [in] volumeGuid: Volume guid path of a volume to which the mount point should set.
[out] mountPoint: filled with auto generated mountpoint for the volume, if the
volume mounting was successful.

Return      : ERROR_SUCCESS -> on success
win32 error   -> on failure.

*/
DWORD ChangeVmConfig::AutoSetMountPoint(const std::string& volumeGuid, std::string& mountPoint)
{
	TRACE_FUNC_BEGIN;
	BOOST_ASSERT(!volumeGuid.empty());
	DWORD dwRet = ERROR_SUCCESS;

	dwRet = GenerateUniqueMntDirectory(mountPoint);
	if (ERROR_SUCCESS != dwRet)
	{
		TRACE_ERROR("Can not create directory\n");
		return dwRet;
	}

	BOOST_ASSERT(!mountPoint.empty());

	//
	// Append '\' if not exist. SetVolumeMountPoint API expects '\' at end of mount path.
	//
	if (!mountPoint.empty() &&
		mountPoint[mountPoint.length() - 1] != '\\')
		mountPoint += "\\";

	if (!SetVolumeMountPoint(mountPoint.c_str(), volumeGuid.c_str()))
	{
		dwRet = GetLastError();

		TRACE_ERROR("Could not set the mount point [%s] to the volume [%s]\n \
								        SetVolumeMountPoint API failed with error %lu",
										mountPoint.c_str(),
										volumeGuid.c_str(),
										dwRet
										);

		mountPoint.clear();
	}

	TRACE_FUNC_END;
	return dwRet;
}

/*
Method      : GenerateUniqueMntDirectory

Description : Generates a unique directory name under system root path and then creates the directory.

Parameters  : [out] mntDir: generated uniuque directory path.

Return      : 0 -> success
1 -> Failure generating unique mount directory
2 -> Unique directory name was generated but could not create that directory.

*/
DWORD ChangeVmConfig::GenerateUniqueMntDirectory(std::string& mntDir)
{
	TRACE_FUNC_BEGIN;
	namespace fs = boost::filesystem;
	DWORD dwRet = 0;
	size_t max_retry = 100;

	const char SOURCE_OS_MOUNT_POINT_ROOT[] = "C:\\";
	const char MOUNT_PATH_PREFIX[] = "azure_rcvr_";

	try
	{
		for (; --max_retry > 0;)
		{
			std::string strUuid;
			if (!GenerateUuid(strUuid))
			{
				TRACE_ERROR("Failed to generate uuid for mount point.");
				dwRet = 1;
				break;
			}

			boost::filesystem::path mntPath(
				std::string(SOURCE_OS_MOUNT_POINT_ROOT)
				+ MOUNT_PATH_PREFIX
				+ strUuid
				);

			if (!fs::exists(mntPath))
			{
				TRACE_INFO("Creating mount path is: %s\n", mntPath.string().c_str());

				if (fs::create_directory(mntPath))
				{
					TRACE_INFO("Unique mount path is: %s\n", mntPath.string().c_str());
					mntDir = mntPath.string();
				}
				else
				{
					TRACE_ERROR("Mount directory creation failed\n");
					dwRet = 2;
				}

				break;
			}
		}
	}
	catch (const std::exception& e)
	{
		TRACE_ERROR("Could not generate unique mount path. Exception: %s\n", e.what());
		dwRet = 1;
	}
	catch (...)
	{
		TRACE_ERROR("Could not generate unique mount path. Unkown exception\n");
		dwRet = 1;
	}

	TRACE_FUNC_END;
	return dwRet;
}

/*
Method      : GenerateUuid

Description : Generates a random uuid

Parameters  : [out] uuid: filled with generated uuid in string format, otherwise empty

Return      : returns true on success, otherwise false.

*/
bool ChangeVmConfig::GenerateUuid(std::string& uuid)
{
	TRACE_FUNC_BEGIN;

	bool bGenerated = true;

	try
	{
		boost::uuids::random_generator uuid_gen;

		boost::uuids::uuid new_uuid = uuid_gen();

		std::stringstream suuid;
		suuid << new_uuid;

		uuid = suuid.str();
	}
	catch (const std::exception& e)
	{
		TRACE_ERROR("Could not generate UUID. Exception: %s\n", e.what());
		bGenerated = false;
	}
	catch (...)
	{
		TRACE_ERROR("Could not generate UUID. Unkown exception\n");
		bGenerated = false;
	}

	TRACE_FUNC_END;
	return bGenerated;
}

/*
Method      : PrepareSourceRegistryHivesWithRetries

Description : Calls PrepareSourceRegistryHives till it suceeds or all exhausted upto max attempts

Return      : none

*/
void ChangeVmConfig::PrepareSourceRegistryHivesWithRetries()
{
	TRACE_FUNC_BEGIN;

	bool bSuceeded = false;

	const int MaxAttempts = 20;
	const int RetryIntervalInMs = 60000;
	int attempt = 0;

	while (!bSuceeded)
	{

		try
		{
			++attempt;
			
			if (attempt > 1) {
				OfflineOnlineRecoveredVMBootDisk();
			}

			PrepareSourceOSDrive();
			LogDirtyBitValueForVolume(m_srcOSVolGuid);
			HideUnhideSourceVolFS();
			PrepareSourceRegistryHives();
			bSuceeded = true;
		}
		catch (const std::exception &e)
		{
			if (attempt == MaxAttempts)
				throw;

			DebugPrintf(SV_LOG_ERROR, "Attempt: %d %s.\n", attempt, e.what());
			Sleep(RetryIntervalInMs);
		}

	}

	TRACE_FUNC_END;

}

/*
Method      : PrepareSourceRegistryHives

Description : Loads the source registry hives (system & software)



*/
void ChangeVmConfig::PrepareSourceRegistryHives()
{
	TRACE_FUNC_BEGIN;

	std::stringstream errStream;
	std::string SystemHiveName = VM_SYSTEM_HIVE_NAME;
	std::string SoftwareHiveName = VM_SOFTWARE_HIVE_NAME;

	if (!PreRegistryChanges(m_osVolMountPath, SystemHiveName, SoftwareHiveName, m_bSystemHiveLoaded, m_bSoftwareHiveLoaded))
	{
		errStream << "VM:" << m_sourceVMName << "Could not prepare the source registry.\n";
		throw std::exception(errStream.str().c_str());
	}

	TRACE_FUNC_END;
}

void ChangeVmConfig::SetServicesToOriginalType()
{
	TRACE_FUNC_BEGIN;

	std::stringstream errStream;
	std::string SystemHiveName = VM_SYSTEM_HIVE_NAME;
	std::string SoftwareHiveName = VM_SOFTWARE_HIVE_NAME;

	if (false == SetServicesToOriginalType(m_sourceVMName, SystemHiveName, SoftwareHiveName))
	{
		errStream << "VM:" << m_sourceVMName << "Could not revert back system service to original state.\n";
		std::exception(errStream.str().c_str());
	}
	
	TRACE_FUNC_END;
}

/*
Method      : MakeChangesForInMageServices

Description : Sets the unwanted service start types to disable/manual-start on source
registry hives so that when recovering vm boots those service will not
run on azure environment.


*/
void ChangeVmConfig::MakeChangesForInMageServices(bool bDoNotSkipInmServices)
{
	TRACE_FUNC_BEGIN;
	

	std::stringstream errorStream;
	std::string SystemHiveName = VM_SYSTEM_HIVE_NAME;
	std::string SoftwareHiveName = VM_SOFTWARE_HIVE_NAME;

	if (false == MakeChangesForInmSvc(SystemHiveName, SoftwareHiveName, bDoNotSkipInmServices))
	{
		errorStream << m_sourceVMName << ":Could not change the service start type for one of the services.\n";
		throw std::exception(errorStream.str().c_str());
	}

	TRACE_FUNC_END;
}


/*
Method      : ResetInvolfltParameters

Description : Resets the involflt filter driver registry settings on source registry hives


*/
void ChangeVmConfig::ResetInvolfltParameters()
{
	TRACE_FUNC_BEGIN;

	std::stringstream errorStream;
	std::string SystemHiveName = VM_SYSTEM_HIVE_NAME;
	std::string SoftwareHiveName = VM_SOFTWARE_HIVE_NAME;

	if (false == ResetInvolfltParameters(m_sourceVMName, SystemHiveName, SoftwareHiveName))
	{
		errorStream << m_sourceVMName << ":Could not reset involflt keys in source registry.\n";
		throw std::exception(errorStream.str().c_str());
	}
	
	TRACE_FUNC_END;
}

/*
Method      : SetBootupScript

Description : Sets the bootup scripts for recovering machine by editing its registry entries.
The script will be executed on booting recovered machine .
The script will validate assigned drive letters, online disks etc.

*/
void ChangeVmConfig::SetBootupScript()
{
	TRACE_FUNC_BEGIN;

	bool bW2K8 = false;
	bool bW2012 = false;
	bool bW2K8R2 = false;

	std::stringstream errorStream;
	std::string SystemHiveName = VM_SYSTEM_HIVE_NAME;
	std::string SoftwareHiveName = VM_SOFTWARE_HIVE_NAME;

	if ((m_OsType.find("2008") != std::string::npos) || (m_OsType.find("7") != std::string::npos) || (m_OsType.find("Vista") != std::string::npos))
	{
		DebugPrintf(SV_LOG_DEBUG, "OsType - %s \n", m_OsType.c_str());
		bW2K8 = true;
		if ((m_OsType.find("R2") != std::string::npos) || (m_OsType.find("7") != std::string::npos))
		{
			bW2K8R2 = true;
		}
	}
	else if ((m_OsType.find("2012") != std::string::npos) || (m_OsType.find("8") != std::string::npos))
	{
		bW2012 = true;
	}

	// Update the registry of source machine to run the script on next boot
	if (false == RegAddStartupExeOnBoot(SoftwareHiveName, m_srcOSVolumeName, bW2K8 || bW2012, bW2K8R2 || bW2012))
	{
		errorStream << "VM:" << m_sourceVMName << " failed to add registry entries for running an executable on VM boot.\n";
		throw std::exception(errorStream.str().c_str());
	}
	else
	{
		DebugPrintf(SV_LOG_INFO, "****** Made script Changes to run on VM boot for host - %s\n\n", m_sourceVMName.c_str());
	}

	TRACE_FUNC_END;
}

void ChangeVmConfig::SetBootupScriptOptions()
{
	TRACE_FUNC_BEGIN;

	std::string newHostId;
	if (!GenerateUuid(newHostId) || newHostId.empty())
		throw std::exception("New HostId should not be empty");

	DebugPrintf(SV_LOG_INFO, "New HostId for the recoveringVM: %s\n", newHostId.c_str());

	SetRecoveryInProgress(VM_SOFTWARE_HIVE_NAME);
	SetRecoveredEnv(m_strCloudEnv, VM_SOFTWARE_HIVE_NAME);
	SetNewHostIdInReg(newHostId, VM_SOFTWARE_HIVE_NAME);

	TRACE_FUNC_END;
}

bool ChangeVmConfig::IsRecoveredVmV2GA()
{
	TRACE_FUNC_BEGIN;
	bool bRet = false;

	std::string prod_ver;
	GetAgentVersionFromReg(prod_ver, VM_SOFTWARE_HIVE_NAME);

	DebugPrintf("Recovering VM VxAgent Prod Version: %s\n", prod_ver.c_str());

	boost::trim(prod_ver);
	if (prod_ver.empty())
		throw std::exception("PROD_VERSION should not be empty");

	bRet = boost::equals(prod_ver, CASPIAN_V2_GA_PROD_VERSION);

	TRACE_FUNC_END;
	return bRet;
}

bool ChangeVmConfig::PerformCleanUpActions()
{
	TRACE_FUNC_BEGIN;
	bool rv = true;

	try
	{
		ReleaseSourceRegistryHives();
	}
	catch (const std::exception &e)
	{
		DebugPrintf(SV_LOG_ERROR, e.what());
		rv = false;
	}

	try{

		if (m_bOsVolMounted)
		{
			if (!FlushFilesystemBuffers(m_srcOSVolGuid))
			{
				// Ignoring the error, as this is not blocking the failback workflow.
				// If the above API fails, then chkdsk will run on the volume during machine boot-up.
			}

			RemoveAndDeleteMountPoints(m_osVolMountPath);
			m_bOsVolMounted = false;
		}
	} catch (const std::exception &e)
	{
		DebugPrintf(SV_LOG_ERROR, e.what());
		rv = false;
	}

	try{

		if (m_bOsDiskOnlined)
		{
			OfflineRecoveredVMBootDisk();
			Sleep(30000);
			ResetOsDiskMBR();
			m_bOsDiskOnlined = false;
		}
	}
	catch (const std::exception &e)
	{
		DebugPrintf(SV_LOG_ERROR, e.what());
		rv = false;
	}

	TRACE_FUNC_END;
	return rv;
}

void ChangeVmConfig::ReleaseSourceRegistryHives()
{
	TRACE_FUNC_BEGIN;

	std::stringstream errStream;
	std::string SystemHiveName = VM_SYSTEM_HIVE_NAME;
	std::string SoftwareHiveName = VM_SOFTWARE_HIVE_NAME;

	if (!PostRegistryChanges(m_osVolMountPath, SystemHiveName, SoftwareHiveName, m_bSystemHiveLoaded, m_bSoftwareHiveLoaded))
	{
		errStream << "VM:" << m_sourceVMName << "Could not unmount the source registry.\n";
		throw std::exception(errStream.str().c_str());
	}

	m_bSystemHiveLoaded = false;
	m_bSoftwareHiveLoaded = false;
	
	TRACE_FUNC_END;
}


/*
Method      : HideUnhideSourceVolFS

Description : Hides the source system volume and unhides it.

Parameters  : 
*/
void ChangeVmConfig::HideUnhideSourceVolFS()
{
	TRACE_FUNC_BEGIN;

	TRACE_INFO("Hide and Unhide Source Volume GUID : %s\n", m_srcOSVolGuid.c_str());

	std::stringstream errStream;
	if (HideDrive(m_srcOSVolGuid.c_str(), m_srcOSVolGuid.c_str()) != SVS_OK)
	{
		errStream << "Hide for volume guid:" << m_srcOSVolGuid << " failed.\n";
		throw std::exception(errStream.str().c_str());
	}
	if (UnhideDrive_RW(m_srcOSVolGuid.c_str(), m_srcOSVolGuid.c_str()) != SVS_OK)
	{
		errStream << "Unhide for volume guid:" << m_srcOSVolGuid << " failed.\n";
		throw std::exception(errStream.str().c_str());
	}

	TRACE_FUNC_END;
}

/*
Method      : LogDirtyBitValueForVolume

Description : Dirty bit query for the specified volume.

Parameters  : volume guid
*/
void ChangeVmConfig::LogDirtyBitValueForVolume(const string & volumeGuid)
{
	TRACE_FUNC_BEGIN;

	TRACE_INFO("Dirty bit query for Volume guid : %s\n", volumeGuid.c_str());

	string osVolMountPath = volumeGuid.substr(0, volumeGuid.length() - 1);

	HANDLE handle = INVALID_HANDLE_VALUE;
	DWORD BytesReturned;
	DWORD VolumeStatus;

	do
	{
		handle = SVCreateFile(osVolMountPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (INVALID_HANDLE_VALUE == handle)
		{
			TRACE_ERROR("Failed to get handle on Volume guid : %s to query the dirty flag. Error: %lu\n", osVolMountPath.c_str(), GetLastError());
			break;
		}

		if(!DeviceIoControl(
			handle,
			FSCTL_IS_VOLUME_DIRTY,
			NULL,
			0,
			(LPVOID)&VolumeStatus,
			sizeof(VolumeStatus),
			&BytesReturned,
			(LPOVERLAPPED)NULL
			))
		{
			TRACE_ERROR("Failed to query the dirty flag for Volume guid : %s with error: %lu.\n", osVolMountPath.c_str(), GetLastError());
			break;
		}

		if (VolumeStatus & VOLUME_IS_DIRTY)
			TRACE_INFO("Dirty bit has been set for volume guid : %s. chkdsk is expected on the volume during machine reboot.\n", osVolMountPath.c_str());
		else
			TRACE_INFO("Dirty bit has not been set for Volume guid : %s.\n", osVolMountPath.c_str());
	} while (0);

	if (handle != INVALID_HANDLE_VALUE)
	{
		if (CloseHandle(handle) == FALSE)
			TRACE_ERROR("Failed while closing handle with error : %lu\n", GetLastError());
	}

	TRACE_FUNC_END;
}

/*
Method      : FlushFilesystemBuffers

Description : Flush filesystem buffers for the specified volume.

Parameters  : volume guid
*/
bool ChangeVmConfig::FlushFilesystemBuffers(const string & volumeGuid)
{
	TRACE_FUNC_BEGIN;
	TRACE_INFO("FlushFilesystemBuffers on volume guid : %s.\n", volumeGuid.c_str());

	ACE_HANDLE h;
	//guid is a output param for the OpenVolumeExclusive API.
	std::string sguid;

    SVERROR sve = OpenVolumeExclusive(&h, sguid, volumeGuid.c_str(), volumeGuid.c_str(), true, false);
	if (sve.failed())
	{
        TRACE_ERROR("OpenVolumeExclusive failed on Volume guid : %s with Error : %s.\n", volumeGuid.c_str(), sve.toString());
		return false;
	}

    sve = CloseVolumeExclusive(h, sguid.c_str(), volumeGuid.c_str(), volumeGuid.c_str());
	if (sve.failed())
	{
        TRACE_ERROR("CloseVolumeExclusive failed on Volume guid : %s with Error : %s.\n", volumeGuid.c_str(), sve.toString());
		return false;
	}

	TRACE_FUNC_END;
	return true;
}


// Instantiate INetFwPolicy2
HRESULT ChangeVmConfig::WFCOMInitialize(INetFwPolicy2** ppNetFwPolicy2)
{
	HRESULT hResult = S_OK;

	hResult = CoCreateInstance(__uuidof(NetFwPolicy2), NULL, CLSCTX_INPROC_SERVER, __uuidof(INetFwPolicy2),	(void**)ppNetFwPolicy2);

	if (FAILED(hResult))
	{
		DebugPrintf(SV_LOG_ERROR, "CoCreateInstance for INetFwPolicy2 failed. Error = %x\n", hResult);
	}
	return hResult;
}


// Release INetFwPolicy2
void ChangeVmConfig::WFCOMCleanup(INetFwPolicy2* pNetFwPolicy2)
{
	// Release the INetFwPolicy2 object (Vista+)
	if (pNetFwPolicy2 != NULL)
	{
		pNetFwPolicy2->Release();
	}
}


// Gets the firewall status for the profiles
// Input - Initialised Firewall policy
// Input - Profile details
// Input - Size of array of profiles
// Output - profile name with firewall status
HRESULT ChangeVmConfig::GetFirewallStatus(__in INetFwPolicy2 *pNetFwPolicy2, FwProfileElement_t ProfileElements[], int size, map<string, bool>& fwStatus)
{
	HRESULT hResult = S_FALSE;
	TRACE_FUNC_BEGIN;

	long    CurrentProfilesBitMask = 0;
	VARIANT_BOOL bActualFirewallEnabled = VARIANT_FALSE;

	do
	{
		hResult = pNetFwPolicy2->get_CurrentProfileTypes(&CurrentProfilesBitMask);
		if (FAILED(hResult))
		{
			DebugPrintf(SV_LOG_ERROR, "Failed to get CurrentProfileTypes. Error: %x.\n", hResult);
			break;
		}

		// The returned 'CurrentProfiles' bitmask can have more than 1 bit set if multiple profiles 
		// are active or current at the same time
		for (int count = 0; count < size; count++)
		{
			if (CurrentProfilesBitMask & ProfileElements[count].Id)
			{
				assert(ProfileElements[count].Name != nullptr);
				wstring wsProfile(ProfileElements[count].Name);
				string sProfile(wsProfile.begin(), wsProfile.end());

				/*Is Firewall Enabled?*/
				hResult = pNetFwPolicy2->get_FirewallEnabled(ProfileElements[count].Id, &bActualFirewallEnabled);
				if (FAILED(hResult))
				{
					DebugPrintf(SV_LOG_ERROR, "Failed to get FirewallEnabled settings for \"%s\" profile. Error: %x.\n", sProfile.c_str(), hResult);
					break;
				}
				DebugPrintf(SV_LOG_INFO, "Firewall state for \"%s\" profile is \"%s\"\n", sProfile.c_str(), bActualFirewallEnabled ? "ON" : "OFF");
				fwStatus[sProfile] = bActualFirewallEnabled ? true : false;
			}
		}
	} while (false);

	TRACE_FUNC_END;
	return hResult;
}


// For the specified firewall profiles display whether the rule group is enabled or not
//If diabled enable it.
HRESULT ChangeVmConfig::EnableRuleGroup(__in INetFwPolicy2 *pNetFwPolicy2, FwProfileElement_t ProfileElements[], int size, wstring wstrGroupName)
{
	HRESULT hResult = S_OK;
	TRACE_FUNC_BEGIN;
	VARIANT_BOOL bActualEnabled = VARIANT_FALSE;

	string strGroupName(wstrGroupName.begin(), wstrGroupName.end());
	BSTR GroupName = SysAllocString(wstrGroupName.c_str());

	for (int count = 0; count < size; count++)
	{
		assert(ProfileElements[count].Name != nullptr);
		wstring wsProfile(ProfileElements[count].Name);
		string sProfile(wsProfile.begin(), wsProfile.end());

		hResult = pNetFwPolicy2->IsRuleGroupEnabled(ProfileElements[count].Id, GroupName, &bActualEnabled);

		if (SUCCEEDED(hResult))
		{
			if (VARIANT_TRUE == bActualEnabled && S_OK == hResult)
			{
				DebugPrintf(SV_LOG_INFO, "Rule Group \"%s\" currently enabled on \"%s\" profile\n", strGroupName.c_str(), sProfile.c_str());
			}
			else if (VARIANT_FALSE == bActualEnabled)
			{
				DebugPrintf(SV_LOG_INFO, "Rule Group \"%s\" currently disabled on \"%s\" profile, enabling it.\n", strGroupName.c_str(), sProfile.c_str());

				// Group is not enabled for the profile - need to enable it
				hResult = pNetFwPolicy2->EnableRuleGroup(ProfileElements[count].Id, GroupName, TRUE);
				if (FAILED(hResult))
				{
					DebugPrintf(SV_LOG_ERROR, "EnableRuleGroup failed on profile \"%s\" with Error: %x\n", sProfile.c_str(), hResult);
					break;
				}
				DebugPrintf(SV_LOG_INFO, "Successfully Enabled the Rule Group \"%s\" on \"%s\" profile.\n", strGroupName.c_str(), sProfile.c_str());
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "Failed calling API IsRuleGroupCurrentlyEnabled. Error: %x.\n", hResult);
			break;
		}
	}

	SysFreeString(GroupName);
	TRACE_FUNC_END;
	return hResult;
}


bool ChangeVmConfig::EnabledRdpPublicFirewallRule()
{
	bool bResult = false;
	TRACE_FUNC_BEGIN;

	HRESULT hrComInit = S_OK;
	INetFwPolicy2 *pNetFwPolicy2 = NULL;

	do
	{
		// Initialize COM.
		hrComInit = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
		if (hrComInit != RPC_E_CHANGED_MODE)
		{
			if (FAILED(hrComInit))
			{
				DebugPrintf(SV_LOG_ERROR, "CoInitializeEx failed. Error = %x\n", hrComInit);
				break;
			}
		}

		// Retrieve INetFwPolicy2
		if (FAILED(WFCOMInitialize(&pNetFwPolicy2)))
			break;

		FwProfileElement_t ProfileElements[1];
		ProfileElements[0].Id = NET_FW_PROFILE2_PUBLIC;
		ProfileElements[0].Name = L"Public";

		map<string, bool> mapFwStatus;
		map<string, bool>::iterator fwIter;

		if (FAILED(GetFirewallStatus(pNetFwPolicy2, ProfileElements, 1, mapFwStatus)))
			break;

		fwIter = mapFwStatus.find("Public");
		if (fwIter == mapFwStatus.end())
		{
			DebugPrintf(SV_LOG_ERROR, "Public profile not found. Cloud not enable Rdp Firewall Rule.\n");
		}
		// If Firewall state is OFF then no modifications needed.
		else if( !fwIter->second )
		{
			bResult = true;
			DebugPrintf(SV_LOG_INFO, "Public profile is OFF. No need to enable Rdp Firewall Rule.\n");
		}
		//Firewall state is ON for Public profile. Enabling the rule if it is disabled for "Remote Desktop" group
		else if (FAILED(EnableRuleGroup(pNetFwPolicy2, ProfileElements, 1, L"Remote Desktop")))
		{
			DebugPrintf(SV_LOG_ERROR, "Error enabling Rdp Firewall Rule on Public Profile.\n");
		}
		else
		{
			//Remote Desktop group update went fine
			bResult = true;
		}

	} while (false);

	// Release INetFwPolicy2
	WFCOMCleanup(pNetFwPolicy2);

	// Uninitialize COM.
	if (SUCCEEDED(hrComInit))
	{
		CoUninitialize();
	}

	TRACE_FUNC_END;
	return bResult;
}

// Method converts the windows error code to message format.
// Input: Error code to convert to message format.
// Returns the converted string of the error code.
std::string ChangeVmConfig::ConvertGetLastErrorToMessage(DWORD error)
{
	if (error)
	{
		LPVOID lpMsgBuf;
		DWORD bufLen = FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf,
			0, NULL);
		if (bufLen)
		{
			LPCSTR lpMsgStr = (LPCSTR)lpMsgBuf;
			std::string result(lpMsgStr, lpMsgStr + bufLen);

			LocalFree(lpMsgBuf);

			return result;
		}
	}
	return std::string();
}

WmiDiskRecordProcessor::WmiDiskRecordProcessor(std::vector<Diskinfo_t> *pdiskinfos)
: m_pdiskinfos(pdiskinfos)
{

}


bool WmiDiskRecordProcessor::Process(IWbemClassObject *precordobj)
{
	if (0 == precordobj)
	{
		m_ErrMsg = "Record object is NULL";
		return false;
	}

	bool bprocessed = true;
	const char NAMECOLUMNNAME[] = "Name";
	const char SCSITARGETIDCOLUMNNAME[] = "SCSITargetId";

	std::string diskname;
	USES_CONVERSION;
	VARIANT vtProp;
	HRESULT hrCol;

	VariantInit(&vtProp);
	hrCol = precordobj->Get(A2W(NAMECOLUMNNAME), 0, &vtProp, 0, 0);
	if (!FAILED(hrCol))
	{
		if (VT_BSTR == V_VT(&vtProp))
		{
			diskname = W2A(V_BSTR(&vtProp));
		}
		VariantClear(&vtProp);
	}
	else
	{
		bprocessed = false;
		m_ErrMsg = "Could not find column: ";
		m_ErrMsg += NAMECOLUMNNAME;
		return bprocessed;
	}

	HANDLE h = SVCreateFile(diskname.c_str(),
		GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == h)
	{
		DWORD err = GetLastError();
		std::stringstream sserr;
		sserr << err;
		bprocessed = false;
		m_ErrMsg = "Failed to open disk ";
		m_ErrMsg += diskname;
		m_ErrMsg += " with error ";
		m_ErrMsg += sserr.str().c_str();
		return bprocessed;
	}
	ON_BLOCK_EXIT(&CloseHandle, h);

	std::vector<Diskinfo_t>::iterator vit = m_pdiskinfos->insert(m_pdiskinfos->end(), Diskinfo_t());
	Diskinfo_t &vi = *vit;

	vi.displayName = diskname;

	vi.scsiId = m_DeviceIDInformer.GetID(diskname);
	if (vi.scsiId.empty()){
		DebugPrintf(SV_LOG_ERROR, "For disk device name %s, could not find SCSI Id. Hence not reporting this.\n", diskname.c_str());
		m_pdiskinfos->erase(vit);
		return bprocessed;
	}


	std::string errMsg;
	vi.capacity = GetDiskSize(h, errMsg);
	if (0 == vi.capacity) {
		m_ErrMsg = "Failed to find size of disk with error: ";
		m_ErrMsg += errMsg;
		m_ErrMsg += ". Not collecting this disk.";
		bprocessed = false;
		m_pdiskinfos->erase(vit);
	}

	return bprocessed;
}

#endif
