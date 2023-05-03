#include <exception>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <boost/lexical_cast.hpp>
#include <algorithm>
#include "configurator2.h"
#include "logger.h"
#include "portable.h" 
#include "configurecxagent.h"
#include "executecommand.h"
#include "devicehelper.h"
#include "fabricagent.h"
#include "volumegroupsettings.h"
#include "boost/lexical_cast.hpp"

#ifdef linux
#include "/usr/include/linux/kdev_t.h"
#endif
using namespace std;

/*************** CONSTANTS ******************************************************/

const char FC_HOST[] = "/sys/class/fc_host";
const char SCSI_HOST[] = "/sys/class/scsi_host";
const char FC_TARGET[] = "/sys/class/fc_remote_ports";
const char PORT_NAME_FILE[] = "port_name";
const char NODE_NAME_FILE[] = "node_name";
const char PORT_STATE_FILE[] = "state";
const char PATH_SEPERATOR[] = "/";
const char SCAN_FILE[] = "scan";
const char LIP_FILE[] = "issue_lip";
const char HOST_KEYWORD[] = "host";
const char TARGET_KEYWORD[] = "rport-";
const char PROC_SCSI_FILE[] = "/proc/scsi/scsi";
const char MODEL_NAME_FILE[] = "model_name";
const char MODEL_DESC_FILE[] = "model_desc";
const char SERIAL_NUMBER_FILE[] = "serial_num";
const char TARGET_MODE_FILE[] = "target_mode_enabled";
const char DEVICE_NAME_PREFIX[] = "/dev/mapper/";
const char SYS_BUS_SCSI[] = "/sys/bus/scsi/devices";
const char SYS_SCSI_DEV[] = "/sys/class/scsi_device/";

int const ADD_DEVICE_WAIT_INTERVAL = 1;//seconds
int const SCSI_SCAN_WAIT_INTERVAL = 2;//seconds
int const FC_LIP_WAIT_INTERVAL = 3; //seconds


DeviceHelper::DeviceHelper()
{
}

DeviceHelper::~DeviceHelper()
{
}

bool DeviceHelper::doDeviceOperation(DiscoverBindingDeviceSettingList discoverBindingDeviceSettingsList )
{
        bool bRet = true;
        // Code remove duplicate Lunid entries 
        for(DiscoverBindingDeviceSettingListIter compareIt = discoverBindingDeviceSettingsList.begin();compareIt != discoverBindingDeviceSettingsList.end();compareIt++)
        {
            DiscoverBindingDeviceSettingListIter subseqIt = compareIt;
            subseqIt++;
            while (subseqIt != discoverBindingDeviceSettingsList.end())
            {
                 if (compareIt->lunId == subseqIt->lunId)
                 {
                       for (BindingNexusList::iterator bindingit = subseqIt->bindingNexusList.begin(); bindingit != subseqIt->bindingNexusList.end(); bindingit++)
                       {
                            compareIt->bindingNexusList.push_back(*bindingit);
                       }
                       subseqIt = discoverBindingDeviceSettingsList.erase(subseqIt);
                 }
                 else
                    subseqIt++;
            }
        }

    try {
        //first iterate through all the entries and remove operation
        for(DiscoverBindingDeviceSettingListIter discoverIter = discoverBindingDeviceSettingsList.begin();discoverIter != discoverBindingDeviceSettingsList.end();discoverIter++)
        {
            if(discoverIter->state != DELETE_AT_LUN_BINDING_DEVICE_PENDING ) continue;
            RemoveResyncDevice(*discoverIter);
        }

        //then iterate through all the entries to discover:
        for(DiscoverBindingDeviceSettingListIter discoverIter = discoverBindingDeviceSettingsList.begin();discoverIter != discoverBindingDeviceSettingsList.end();discoverIter++)
        {
            if(discoverIter->state != DISCOVER_BINDING_DEVICE_PENDING) continue;

            procesDiscoverResyncDevice(*discoverIter);
        }
    }
    catch (ContextualException const & ie) {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::doDeviceOperation:%s", ie.what());             
    } catch (std::exception const & e) {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::doDeviceOperation:%s", e.what());            
    } catch (...) {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::doDeviceOperation: unknown exception\n");            
    }
    return bRet;
}

bool DeviceHelper::getFCHostId(const std::string &portName, std::vector<std::string>& hostIdVec, bool& hostIdFound)
{
    hostIdFound = false;
    std::string hostId;
    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::getFCHostId: ENTERED port WWN= %s\n",portName.c_str());
    DIR             *dip = 0;
    struct dirent   *dit = 0; //directory iterator

    if ((dip = opendir(FC_HOST)) == NULL)
    {         
        DebugPrintf(SV_LOG_ERROR, " DeviceHelper::getFCHostId: opendir on /sys/class/fc_host FAILED\n");
        return false;
    }
    while ((dit = readdir(dip)) != NULL)
    {            
        if (strncmp(dit->d_name, HOST_KEYWORD, strlen(HOST_KEYWORD)) == 0)
        {
            std::string currentPortName;
            std::string portNameFile = FC_HOST;
            portNameFile += PATH_SEPERATOR;
            portNameFile += dit->d_name;
            portNameFile += PATH_SEPERATOR;
            portNameFile += PORT_NAME_FILE;
            std::ifstream portNameFileStream(portNameFile.c_str(), std::ifstream::in);
            if (portNameFileStream)
            {
                portNameFileStream >> currentPortName;
                portNameFileStream.close();
                unsigned long long currentPortWWN,hexPortWWN;
                currentPortWWN = strtoll(currentPortName.c_str(),NULL,16);
                hexPortWWN = strtoll(getHexWWN(portName).c_str(),NULL,16);         
                if (currentPortWWN == hexPortWWN )
                {
                    hostId = dit->d_name;
                    hostId.erase(0,strlen(HOST_KEYWORD)); //remove "host"
                    hostIdFound = true;
                    break;
                }
            }
        }
    }
    if (closedir(dip) == -1)
    {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::getFCHostId:: closedir FAILED\n");
    }
    std::stringstream str;
    if (hostIdFound)
    {
        str << "DeviceHelper::getFCHostId:: hostId is: " << hostId << " found for portName=" << portName  << endl;
    }
    else
    {
        str << "DeviceHelper::getFCHostId:: No hostId found for portName=" << portName << endl;
    }
    DebugPrintf(SV_LOG_DEBUG, "%s", str.str().c_str());
    hostIdVec.push_back(hostId);//single host id pushed into the vector.
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return true;
}

bool DeviceHelper::getISCSIHostId(const std::string &portName, const std::string &targetName, std::vector<std::string>& hostIdVec, bool& hostIdFound)
{
    hostIdFound = false;
    DebugPrintf(SV_LOG_DEBUG, "DeviceHelper::getISCSIHostId ENTERED. iqn= %s\n", portName.c_str());
    DIR             *dip = 0;
    struct dirent   *dit = 0;


    if(NULL == (dip = opendir("/sys/class/iscsi_session/")))
    {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::getISCSIHostId: opendir on /sys/class/iscsi_sesion FAILED\n");
        return false;
    }
    bool bSessionFound = false;
    while ((dit = readdir(dip)) != NULL)
    {
        std::string strInitiatorFile("/sys/class/iscsi_session/");
        std::string strTargetFile("/sys/class/iscsi_session/");
        if(strcmp(dit->d_name,".") == 0 || strcmp(dit->d_name,"..") == 0)
        {
            //dont process . and ..
            continue;
        }
        strInitiatorFile += dit->d_name;
        strTargetFile += dit->d_name;
        strInitiatorFile += PATH_SEPERATOR;
        strTargetFile += PATH_SEPERATOR;
        strInitiatorFile += "initiatorname";
        strTargetFile += "targetname";
        std::ifstream ifInitiatorFileStream(strInitiatorFile.c_str(), std::ifstream::in);
        std::ifstream ifTargetFileStream(strTargetFile.c_str(), std::ifstream::in);
        if(ifInitiatorFileStream && ifTargetFileStream)
        {
            std::string strInitiator, strTarget;
            ifInitiatorFileStream >> strInitiator;
            ifTargetFileStream >> strTarget;
            if(0 == stricmp(strInitiator.c_str(),portName.c_str()) && 0 == stricmp(strTarget.c_str(), targetName.c_str())) //case insensitive compare.
            {
                DIR             *deviceFolder_dip = 0;
                struct dirent   *deviceFolder_dit = 0;
                std::string strDeviceFolder("/sys/class/iscsi_session/");
                bSessionFound = true;
                strDeviceFolder += dit->d_name;
                strDeviceFolder += PATH_SEPERATOR;
                strDeviceFolder += "device";
                if(NULL == (deviceFolder_dip = opendir(strDeviceFolder.c_str())))
                {
                    DebugPrintf(SV_LOG_ERROR, "DeviceHelper::getISCSIHostId: opendir on %s FAILED. skipping the directory. errno=%d\n", strDeviceFolder.c_str(),errno);
                    continue;
                }
                DebugPrintf(SV_LOG_DEBUG, "DeviceHelper::getISCIHostID searching %d directory", strDeviceFolder.c_str());
                while((deviceFolder_dit = readdir(deviceFolder_dip)) != NULL)
                {
                    if(0 == strncmp(deviceFolder_dit->d_name, "target", 6))
                    {
                        std::string strTarget(deviceFolder_dit->d_name);
                        strTarget.erase(0, 6);//remove the 'target'
                        std::vector<std::string> vTargetChannels;
                        Tokenize(strTarget, vTargetChannels, ":");
                        hostIdVec.push_back(vTargetChannels[0]);
                        hostIdFound = true;
                        DebugPrintf(SV_LOG_DEBUG, "DeviceHelper::getISCSIHostId:: hostId is: %s for portName=%s\n", vTargetChannels[0].c_str(), portName.c_str());
                    }
                }
                if(NULL == deviceFolder_dit && 0 != errno)
                {
                    DebugPrintf(SV_LOG_ERROR, "DeviceHelper::getISCSIHostId Unable to readdir %s errno=%d.\n", strDeviceFolder.c_str(), errno);
                }
                if(-1 == closedir(deviceFolder_dip))
                {
                    DebugPrintf(SV_LOG_ERROR, "DeviceHelper::getISCSIHostId::closedir on %s FAILED\n", strDeviceFolder.c_str());
                }
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG,"initiatorname:\"%s\" targetname=\"%s\" portName=\"%s\" targetName=\"%s\"\n",strInitiator.c_str(), strTarget.c_str(), portName.c_str(), targetName.c_str());
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "DeviceHelper::getISCSIHostId::  Unable to open intiator/target files %s %s\n",strInitiatorFile.c_str(),strTargetFile.c_str());
        }
    }
    if(false == bSessionFound)
    {
        DebugPrintf(SV_LOG_ERROR, "iscsi session for the lun not found.\n");
    }
    if(NULL == dit && 0 != errno)
    {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::getISCSIHostId Unable to readdir /sys/class/iscsi_session folder. errno=%d\n", errno);
    }
    if( -1 == closedir(dip) )
    {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::getISCSIHostId:: closedir FAILED\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    if(hostIdVec.size()>0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool DeviceHelper::getHostId(const std::string &portName, const std::string& targetPortName, std::vector<std::string>& hostIdVec,bool& hostIdFound)
{
    if(portName.find("iqn") != std::string::npos && targetPortName.find("iqn") != std::string::npos) //this is iscsi iqn.
    {
        return getISCSIHostId(portName, targetPortName, hostIdVec, hostIdFound);
    }
    else
    {
        return getFCHostId(portName, hostIdVec, hostIdFound);
    }
}

bool DeviceHelper::issueScan(const std::string & hostId)
{
    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::issueScan: Entered: Host(HBA) name = %s\n",hostId.c_str());
    std::string scanInput = "- - -";

    std::string scanFile = SCSI_HOST;
    scanFile += PATH_SEPERATOR;
    scanFile += HOST_KEYWORD;
    scanFile +=  hostId;
    scanFile += PATH_SEPERATOR;
    scanFile += SCAN_FILE;

    std::ofstream scan(scanFile.c_str(),std::ofstream::out);
    if(!scan)
    {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::issueScan Failed: Unable to open directory %s for writingi\n",scanFile.c_str());
        return false;
    }
    scan << scanInput;
    scan.close();
    return true;
}

bool DeviceHelper::issueLip(const std::string & hostId)
{
    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::issueLip: Entered: Host(HBA) name = %s\n",hostId.c_str());
    std::string lipInput = "1";

    std::string lipFile = FC_HOST;
    lipFile += PATH_SEPERATOR;
    lipFile += HOST_KEYWORD; 
    lipFile += hostId;
    lipFile += PATH_SEPERATOR;
    lipFile += LIP_FILE;

    std::ofstream lip(lipFile.c_str(),std::ofstream::out);
    if(!lip)
    {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::issueLip Failed: Unable to open directory %s for writing\n",lipFile.c_str());
        return false;
    }
    lip << lipInput;
    lip.close();
    return true;
}

bool DeviceHelper::getISCSITargetChannel(const std::string& hostId, const std::string& targetPortName, std::vector<std::string>& channelIdVec, std::vector<std::string>& targetIdVec, bool& targetFound)
{
    targetFound = false;
    DebugPrintf(SV_LOG_DEBUG, "DeviceHelper::getISCSITargetChannel: ENTERED target port iqn = %s\n", targetPortName.c_str());
    DIR             *dip = 0;
    struct dirent   *dit = 0;

    std::string strIscsiSessionFolder("/sys/class/iscsi_session");
    if(NULL == (dip = opendir(strIscsiSessionFolder.c_str())))
    {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::getISCSITargetChannel: opendir %s FAILDED\n", strIscsiSessionFolder.c_str());
    }

    while((dit = readdir(dip)) != NULL)
    {
        std::string strTargetFile("/sys/class/iscsi_session/");
        strTargetFile += dit->d_name;
        strTargetFile += PATH_SEPERATOR;
        strTargetFile += "targetname";
        std::ifstream ifTargetFileStream(strTargetFile.c_str(), std::ifstream::in);
        if(ifTargetFileStream)
        {
            std::string strInitiator, strTarget;
            ifTargetFileStream >> strTarget;
            if(0 == stricmp(strTarget.c_str(), targetPortName.c_str())) //case insensitive compare.
            {
                DIR             *deviceFolder_dip = 0;
                struct dirent   *deviceFolder_dit = 0;
                std::string strDeviceFolder("/sys/class/iscsi_session/");
                strDeviceFolder += dit->d_name;
                strDeviceFolder += PATH_SEPERATOR;
                strDeviceFolder += "device";
                if(NULL == (deviceFolder_dip = opendir(strDeviceFolder.c_str())))
                {
                    DebugPrintf(SV_LOG_ERROR, "DebugPrintf::getISCSITargetChannel: opendir on %s FAILED. skipping the directory\n", strDeviceFolder.c_str());
                    continue;
                }
                while((deviceFolder_dit = readdir(deviceFolder_dip)) != NULL)
                {
                    if(0 == strncmp(deviceFolder_dit->d_name, "target", 6))
                    {
                        std::string strTarget(deviceFolder_dit->d_name);
                        strTarget.erase(0, 6);//remove the 'target'
                        std::vector<std::string> vTargetChannels;
                        Tokenize(strTarget, vTargetChannels, ":");
                        if(hostId.compare(vTargetChannels[0]) == 0)
                        {
                            channelIdVec.push_back(vTargetChannels[1]);
                            targetIdVec.push_back(vTargetChannels[2]);
                            targetFound = true;
                            DebugPrintf(SV_LOG_DEBUG, "DeviceHelper::getISCSITargetChannel:: HTC is: %s \n", strTarget.c_str());
                        }
                    }
                }
                if(-1 == closedir(deviceFolder_dip))
                {
                    DebugPrintf(SV_LOG_ERROR, "DeviceHelper::getISCSITargetChannel::closedir on %s FAILED\n", strDeviceFolder.c_str());
                }
            }
        }
    }
    if(-1 == closedir(dip))
    {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::getISCSITargetChannel:: closedir FAILED\n");
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return true;
}

bool DeviceHelper::getFCTargetChannel(const std::string& hostId, const std::string& targetPortName, std::vector<std::string>& channelIdvec, std::vector<std::string>& targetIdvec, bool& targetFound)
{
    targetFound = false;
    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::getFCTargetChannel: ENTERED target port WWN= %s\n",targetPortName.c_str());
    DIR             *dip = 0;
    struct dirent   *dit = 0; //directory iterator

    if ((dip = opendir(FC_TARGET)) == NULL)
    {         
        DebugPrintf(SV_LOG_ERROR, " DeviceHelper::getFCTargetChannel: opendir /sys/class/fc_transport FAILED\n");
        return false;
    }

    std::string searchString = TARGET_KEYWORD;
    searchString += hostId;

    while ((dit = readdir(dip)) != NULL)
    {            
        if (strncmp(dit->d_name, searchString.c_str(),searchString.size()) == 0)
        {
            std::string currentPortName;
            std::string portNameFile = FC_TARGET;
            portNameFile += PATH_SEPERATOR;
            portNameFile += dit->d_name;
            portNameFile += PATH_SEPERATOR;
            portNameFile += PORT_NAME_FILE;
            std::ifstream portNameFileStream(portNameFile.c_str(), std::ifstream::in);
            if (portNameFileStream)
            {
                portNameFileStream >> currentPortName;
                portNameFileStream.close();
                unsigned long long currentPortWWN,hexPortWWN;
                currentPortWWN = strtoll(currentPortName.c_str(),NULL,16);
                hexPortWWN = strtoll(getHexWWN(targetPortName).c_str(),NULL,16);
                if (currentPortWWN == hexPortWWN )
                {
                    std::string targetId;
                    std::string channelId;
            std::string dirName = dit->d_name;
                    std::string targetIdFile = FC_TARGET;
                    targetIdFile += PATH_SEPERATOR;
                    targetIdFile += dit->d_name;
                    targetIdFile += PATH_SEPERATOR;
                    targetIdFile += "scsi_target_id";
                    std::ifstream scsiIdFileStream(targetIdFile.c_str(),std::ifstream::in);
                    if(scsiIdFileStream)
                    {
                        scsiIdFileStream >> targetId;
                        targetIdvec.push_back(targetId);
                    }
                    dirName.erase(0,searchString.size());
                    size_t fpos = dirName.find_first_of(':');
                    size_t lpos = dirName.find_last_of('-'); 
                    if(fpos != std::string::npos && lpos != std::string::npos)
                    {
                        channelId = dirName.substr(fpos+1,lpos-1);
                        channelIdvec.push_back(channelId);
                    }
                    targetFound = true;
                    //break; //Loop continously for still any further matches found
                }
            }
        }
    }
    if (closedir(dip) == -1)
    {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::getHostName:: closedir FAILED\n");
    }
    std::stringstream str;
    if (targetFound)
    {
        for (int j=0; j < targetIdvec.size(); j++)
        str << "DeviceHelper::getHostName:: entry: " << j << ", channel id is: " << channelIdvec[j] << " and target id is =" << targetIdvec[j] << "for target port name"<< targetPortName  << endl;
    }
    else
    {
        str << "DeviceHelper::getHostName:: No target found for  target portName=" << targetPortName << endl;
    }
    DebugPrintf(SV_LOG_DEBUG, "%s", str.str().c_str());
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return true;
}

bool DeviceHelper::getTargetChannel(const std::string& hostId, const std::string& aiPortName, const std::string& targetPortName, std::vector<std::string>& channelIdvec, std::vector<std::string>& targetIdvec,bool& targetFound )
{
    if(aiPortName.find("iqn") != std::string::npos && targetPortName.find("iqn") != std::string::npos)
    {
        return getISCSITargetChannel(hostId, targetPortName, channelIdvec, targetIdvec, targetFound);
    }
    else
    {
        return getFCTargetChannel(hostId, targetPortName, channelIdvec, targetIdvec, targetFound);
    }
}

bool DeviceHelper::addDevice(const std::string& hostId, const std::string& channelId, const std::string& targetId,const std::string& lunNumber)
{
    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::addDevice: Entered: Host(HBA) name = %s,channelId = %s,targetId = %s, lunNumber = %s \n",hostId.c_str(),channelId.c_str(),targetId.c_str(),lunNumber.c_str());
    std::string addDeviceInput = "scsi add-single-device";
    addDeviceInput += " ";
    addDeviceInput += hostId;
    addDeviceInput += " ";
    addDeviceInput += channelId;
    addDeviceInput += " ";
    addDeviceInput += targetId;
    addDeviceInput += " ";
    addDeviceInput += lunNumber;

    std::ofstream scsiDevice(PROC_SCSI_FILE,std::ofstream::out);
    if(!scsiDevice)
    {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::addDevice Failed: Unable to open directory %s for writing\n",PROC_SCSI_FILE);
        return false;
    }
    scsiDevice << addDeviceInput;
    scsiDevice.close();
    return true;
}

bool DeviceHelper::removeDevice(const std::string& hostId, const std::string& channelId, const std::string& targetId,const std::string& lunNumber)
{
    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::removeDevice: Entered: Host(HBA) name = %s,channelId = %s,targetId = %s, lunNumber = %s \n",hostId.c_str(),channelId.c_str(),targetId.c_str(),lunNumber.c_str());
    std::string addDeviceInput = "scsi remove-single-device";
    addDeviceInput += " ";
    addDeviceInput += hostId;
    addDeviceInput += " ";
    addDeviceInput += channelId;
    addDeviceInput += " ";
    addDeviceInput += targetId;
    addDeviceInput += " ";
    addDeviceInput += lunNumber;

    std::ofstream scsiDevice(PROC_SCSI_FILE,std::ofstream::out);
    if(!scsiDevice)
    {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::removeDevice Failed: Unable to open directory %s for writing \n",PROC_SCSI_FILE);
        return false;
    }
    scsiDevice << addDeviceInput;
    scsiDevice.close();
    return true;
}

bool DeviceHelper::discoverMappedDevice(DiscoverBindingDeviceSetting& discoverBindingDeviceSetting)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n", FUNCTION_NAME);
    if(discoverBindingDeviceSetting.state == DISCOVER_BINDING_DEVICE_PENDING)
    {
        procesDiscoverResyncDevice(discoverBindingDeviceSetting); 
        //If the device is discovered succesfully, delete all partitions of the device.
        if (discoverBindingDeviceSetting.resultState > 0)
            removeMultipathPartitions(discoverBindingDeviceSetting.lunId);
    }
    if(discoverBindingDeviceSetting.state == DELETE_AT_LUN_BINDING_DEVICE_PENDING)
        RemoveResyncDevice (discoverBindingDeviceSetting); 
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n", FUNCTION_NAME);
    return discoverBindingDeviceSetting.resultState > 0? true: false;
}

bool DeviceHelper::isMPDeviceReadable(const std::string &device)
{
    bool rVal = false;
#ifdef linux
    int fd = open(device.c_str(), O_RDONLY|O_DIRECT);
#else
    int fd = open(device.c_str(), O_RDONLY);
#endif
    if (fd > 0)
    {
        char *tmpbuf = (char *)valloc(512);

        DebugPrintf(SV_LOG_DEBUG,"%s File: %s open success\n", FUNCTION_NAME, device.c_str());
        if (read(fd, tmpbuf,  512) != 512)
        {
           DebugPrintf(SV_LOG_ERROR,"%s: Error while reading first 512 bytes, multipathFile: %s\n", FUNCTION_NAME, device.c_str());
        }
        else
        {
             DebugPrintf(SV_LOG_DEBUG,"%s: Reading first 512 bytes, multipathFile: %s\n", FUNCTION_NAME, device.c_str());
             rVal = true; 
        }
        close(fd);
        free (tmpbuf);
    }
    return rVal;
}

void DeviceHelper::procesDiscoverResyncDevice(DiscoverBindingDeviceSetting& discoverBindingDeviceSetting)
{
    //add single device to all the AI's and targets
    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::procesDiscoverResyncDevice: Entered \n");
    bool deviceFound = false;
    bool isDeviceReadable = false;

    if(discoverResyncDevice(discoverBindingDeviceSetting.lunId,discoverBindingDeviceSetting.deviceName,deviceFound))
    {
        if(deviceFound)
        {
             std::string multipathFile = string("/dev/mapper/") + discoverBindingDeviceSetting.lunId;
            if(isMPDeviceReadable(multipathFile))
            {
                isDeviceReadable = true;
                discoverBindingDeviceSetting.resultState = DISCOVER_BINDING_DEVICE_DONE;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR,"DeviceHelper::procesDiscoverResyncDevice: Error while opening multipathFile: %s\n", multipathFile.c_str());
            }
        }
    }
    else
    {
        discoverBindingDeviceSetting.resultState = FA_ERROR_STATE(DISCOVER_BINDING_DEVICE_OTHER_FAILURE);
        return;
    }

    std::list<ScsiDevInfo> scsiDevInfoList;

    bool anyStaleDeviceExists = false;
    bool anyHostFound = false;
    for(BindingNexusList::iterator bindingNexusListIter = discoverBindingDeviceSetting.bindingNexusList.begin(); bindingNexusListIter != discoverBindingDeviceSetting.bindingNexusList.end(); bindingNexusListIter++)
    {
        std::vector<std::string> channelIdvec;
        std::vector<std::string> targetIdvec;
        bool targetFound = false;

        DebugPrintf("Add device ITL Details are : I=%s, T=%s, L=%d\n",bindingNexusListIter->bindingAiPortName.c_str(), bindingNexusListIter->bindingVtPortName.c_str(), bindingNexusListIter->lunNumber);

        std::vector<std::string> hostIdVec;
        bool hostIdFound = false;

        if(getHostId(bindingNexusListIter->bindingAiPortName, bindingNexusListIter->bindingVtPortName, hostIdVec, hostIdFound))
        {
            if(!hostIdFound)
            {
                DebugPrintf(SV_LOG_ERROR, "Host is not found for Initiator %s. Continuing with remaining ITLs\n",bindingNexusListIter->bindingAiPortName.c_str());
                continue;
            }
            anyHostFound = true;
        }
        else
        {
            discoverBindingDeviceSetting.resultState = FA_ERROR_STATE(DISCOVER_BINDING_DEVICE_OTHER_FAILURE);
            continue;
        }

        if(!anyHostFound)
        {
            DebugPrintf(SV_LOG_ERROR, "Host is not found for any Initiators.\n");
            discoverBindingDeviceSetting.resultState = FA_ERROR_STATE(DISCOVER_BINDING_DEVICE_OTHER_FAILURE);
            return;
        }

        for(int hostIdIndex = 0; hostIdIndex < hostIdVec.size(); ++hostIdIndex)
        {
            if(getTargetChannel(hostIdVec[hostIdIndex], bindingNexusListIter->bindingAiPortName, bindingNexusListIter->bindingVtPortName, channelIdvec, targetIdvec, targetFound))
            {
                if(targetFound)
                {
                    std::string lunNumber = boost::lexical_cast<std::string>(bindingNexusListIter->lunNumber);
                    for (int index = 0; index < targetIdvec.size(); ++index)
                    {
                        scsiDevInfoList.push_back(ScsiDevInfo(hostIdVec[hostIdIndex],channelIdvec[index],targetIdvec[index],lunNumber));
                        if(IsScsiDeviceExists(hostIdVec[hostIdIndex],channelIdvec[index],targetIdvec[index],lunNumber))// && !isDeviceReadable)
                        {
                            if (!isDeviceReadable)
                            {
                                DebugPrintf(SV_LOG_ERROR,"DeviceHelper::procesDiscoverResyncDevice: Found stale device [%s:%s:%s:%s].\n",hostIdVec[hostIdIndex].c_str(), channelIdvec[index].c_str(), targetIdvec[index].c_str(), lunNumber.c_str());
                                removeScsiDevice(hostIdVec[hostIdIndex],channelIdvec[index],targetIdvec[index],lunNumber);
                                anyStaleDeviceExists = true;
                            }
                        }
                        else
                        {
                            if (isDeviceReadable)
                            {
                                addDevice(hostIdVec[hostIdIndex],channelIdvec[index],targetIdvec[index],lunNumber);
                            }
                        }
                    }
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Target Channel is not found for Host id %s. Continuing with remaining ITLs\n", hostIdVec[hostIdIndex].c_str());
                continue;
            }
        }
    }

// Device is not readable and found few stale entries, so delete all the entries.
    if (anyStaleDeviceExists)
    {
        if (deviceFound)
            removeMultipathDevice(discoverBindingDeviceSetting.lunId);
        discoverBindingDeviceSetting.resultState = FA_ERROR_STATE(DISCOVER_BINDING_DEVICE_OTHER_FAILURE);
        sleep(10);
        return;
    }

//if Device in not radable and we found no device entries, add all entries.
    if (!isDeviceReadable)
    {
        DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::procesDiscoverResyncDevice: Adding scsi devices.\n");
        std::list<ScsiDevInfo>::const_iterator listIter = scsiDevInfoList.begin();
        for(; listIter != scsiDevInfoList.end(); ++listIter)
        {
            addDevice(listIter->h, listIter->c, listIter->t, listIter->l);
        }
    }

    int maxSleep = 45; // Max sleep for 30 secs before returning DISCOVER_BINDING_DEVICE_DEVICE_NOT_FOUND

// Device is added or we have found the device is readable.
    while (maxSleep > 0)
    {
        sleep(FC_LIP_WAIT_INTERVAL);
        maxSleep -= FC_LIP_WAIT_INTERVAL;

        if(discoverResyncDevice(discoverBindingDeviceSetting.lunId,discoverBindingDeviceSetting.deviceName,deviceFound))
        {
            if(deviceFound)
            {
                std::string multipathFile = string("/dev/mapper/") + discoverBindingDeviceSetting.lunId;
                if(isMPDeviceReadable(multipathFile))
                {
                    discoverBindingDeviceSetting.resultState = DISCOVER_BINDING_DEVICE_DONE;
                    break;
                }
                else
                {
                    discoverBindingDeviceSetting.resultState = FA_ERROR_STATE(DISCOVER_BINDING_DEVICE_DEVICE_NOT_FOUND);
                    DebugPrintf(SV_LOG_ERROR,"DeviceHelper::procesDiscoverResyncDevice: Error while opening multipathFile: %s after discover\n", multipathFile.c_str());
                   break; // dont want to continue with sleep if Failed for other reason.
                }
            }
            else
            {
               discoverBindingDeviceSetting.resultState = FA_ERROR_STATE(DISCOVER_BINDING_DEVICE_DEVICE_NOT_FOUND);
               DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::procesDiscoverResyncDevice: MP device for LunId %s not formed. Retrying after sleeing %d secs.\n", discoverBindingDeviceSetting.lunId.c_str(),FC_LIP_WAIT_INTERVAL);
            } 
        }
        else
        {
            discoverBindingDeviceSetting.resultState = FA_ERROR_STATE(DISCOVER_BINDING_DEVICE_OTHER_FAILURE);
            break; // dont want to continue with sleep if Failed for other reason.
        }
    }
    //mknod the device if it is not created by multipath.
    if (maxSleep <= 0 && discoverBindingDeviceSetting.resultState == FA_ERROR_STATE(DISCOVER_BINDING_DEVICE_DEVICE_NOT_FOUND))
    {
        int major = 0, minor = 0;
        bool rVal = getMinMojorForMultipath (discoverBindingDeviceSetting.lunId, major, minor);
        if(rVal)
        {
            std::string devMapper = "/dev/mapper/" + discoverBindingDeviceSetting.lunId;
            int status;
#ifdef linux
            dev_t dev = MKDEV (major, minor);
            status  = mknod(devMapper.c_str(), S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH, dev);
#endif
            {
                sleep (2);
                DebugPrintf(SV_LOG_ERROR,"DeviceHelper::procesDiscoverResyncDevice - Device %s creates Succesfully.\n", devMapper.c_str());
                if(isMPDeviceReadable(devMapper))
                {
                    discoverBindingDeviceSetting.resultState = DISCOVER_BINDING_DEVICE_DONE;
                }
                else
                {
                    discoverBindingDeviceSetting.resultState = FA_ERROR_STATE(DISCOVER_BINDING_DEVICE_DEVICE_NOT_FOUND);
                    DebugPrintf(SV_LOG_ERROR,"DeviceHelper::procesDiscoverResyncDevice: Error %d while reading from  multipathFile: %s after mknod\n", errno, devMapper.c_str());
                }
            }
        }
        else
        {
           discoverBindingDeviceSetting.resultState = FA_ERROR_STATE(DISCOVER_BINDING_DEVICE_DEVICE_NOT_FOUND);
        }
    }
}

void DeviceHelper::RemoveResyncDevice(DiscoverBindingDeviceSetting& discoverBindingDeviceSetting)
{
    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::RemoveResyncDevice: Entered \n");
    removeMultipathPartitions(discoverBindingDeviceSetting.lunId);
    int noOfRetries = 12;
    discoverBindingDeviceSetting.resultState = DELETE_AT_LUN_BINDING_DEVICE_DONE;
    for(int i = 0; i < noOfRetries; i++)
    if( removeMultipathDevice(discoverBindingDeviceSetting.lunId) == true) 
    {
        DebugPrintf(SV_LOG_DEBUG, "DeviceHelper::removeMultipathDevice removed multipath device sucessfully\n");
        discoverBindingDeviceSetting.resultState = DELETE_AT_LUN_BINDING_DEVICE_DONE;
        break;
    }
    else 
    {
        DebugPrintf(SV_LOG_WARNING,"DeviceHelper::removeMultipathDevice failed to remove multipath device \n");
        discoverBindingDeviceSetting.resultState = FA_ERROR_STATE(DELETE_AT_LUN_BINDING_DEVICE_FAILURE);
        sleep(10);
    }

    for(BindingNexusList::iterator bindingNexusListIter = discoverBindingDeviceSetting.bindingNexusList.begin();bindingNexusListIter != discoverBindingDeviceSetting.bindingNexusList.end();bindingNexusListIter++)
    {
        std::vector<std::string> hostIdVec;
        bool hostIdFound = false;
        DebugPrintf("Remove device ITL Details are : I=%s, T=%s, L=%d\n",bindingNexusListIter->bindingAiPortName.c_str(), bindingNexusListIter->bindingVtPortName.c_str(), bindingNexusListIter->lunNumber);

        if(getHostId(bindingNexusListIter->bindingAiPortName, bindingNexusListIter->bindingVtPortName, hostIdVec, hostIdFound))
        {
            if(!hostIdFound)
            {
                DebugPrintf(SV_LOG_ERROR, "Host is not found for Initiator %s. Continuing with remaining ITLs\n",bindingNexusListIter->bindingAiPortName.c_str());
                continue;
            }
        }
        else
        {
            discoverBindingDeviceSetting.resultState = FA_ERROR_STATE(DISCOVER_BINDING_DEVICE_OTHER_FAILURE);
            continue;
        }

        std::vector<std::string> channelIdvec;
        std::vector<std::string> targetIdvec;
        bool targetFound = false;

        for(int index=0; index<hostIdVec.size(); ++index)
        {
            if(getTargetChannel(hostIdVec[index],bindingNexusListIter->bindingAiPortName, bindingNexusListIter->bindingVtPortName,channelIdvec,targetIdvec,targetFound))
            {
                if(targetFound)
                {
                    std::string lunNumber = boost::lexical_cast<std::string>(bindingNexusListIter->lunNumber);
                    for (int targetIdindex = 0; targetIdindex <targetIdvec.size(); targetIdindex++)
                    {
                        removeDevice(hostIdVec[index],channelIdvec[targetIdindex],targetIdvec[targetIdindex],lunNumber);
                        //remove everything if it is last device 
                        if(discoverBindingDeviceSetting.deviceDelete)
                        removeAllDevice(hostIdVec[index],channelIdvec[targetIdindex],targetIdvec[targetIdindex]);
                    }
                    discoverBindingDeviceSetting.resultState = DELETE_AT_LUN_BINDING_DEVICE_DONE;
                }
            }
        }
    }
    //discoverBindingDeviceSetting.resultState = DELETE_AT_LUN_BINDING_DEVICE_DONE;
}

std::string DeviceHelper::getHexWWN(const std::string& wwn)
{
    std::string hexWWN = "0x";
    hexWWN += wwn;
    hexWWN.erase(std::remove(hexWWN.begin(), hexWWN.end(), ':'),hexWWN.end());
    std::transform(hexWWN.begin(), hexWWN.end(),hexWWN.begin(), ::tolower);
    return hexWWN;
}

std::string DeviceHelper::getColonWWN(const std::string& wwn)
{
    std::string colonWWN = wwn;
    std::string hex = "0x"; //we need to remove the "0x" if there is one

    size_t pos= colonWWN.find(hex,0);
    if(pos != std::string::npos)
        colonWWN.erase(pos,hex.size());

    // normalize the wwn length to 16 chars by prepending 0s
    while (colonWWN.size() < 16)
    {
        colonWWN.insert(0,1,'0');
    }

    for(size_t it = 2;it < colonWWN.size();it+=3)
    {
        colonWWN.insert(it,1,':');
    }
    return colonWWN;
}

bool DeviceHelper::getMinMojorForMultipath(const std::string& lunId, int &major, int &minor)
{
   DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::getMinMojorForMultipath : Entered\n");
   std::string cmd("/sbin/dmsetup ls  2>/dev/null | grep ");
   cmd += lunId;
   cmd += " | cut -d'(' -f2";
   std::stringstream results;

    if (!executePipe(cmd, results)) {
        return false;
    }

    if(!results.good())
    {
       return false;
    }

    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::getMinMojorForMultipath  = %s \n", results.str().c_str());
    
    size_t pos = results.str().find_first_of("0123456789");
    size_t lastPos = results.str().find_first_not_of("0123456789");
    try
    {
        if (pos != std::string::npos && lastPos != std::string::npos)
        {
            std::string maj(results.str().c_str() + pos, lastPos - pos);
            major = boost::lexical_cast<int>(maj);
        }
        else
            return false;
    }
    catch(boost::bad_lexical_cast &)
    {
        DebugPrintf(SV_LOG_ERROR,"DeviceHelper::getMinMojorForMultipath - Unable to lexical cast string %s  to major number for LUN %s.\n", results.str().c_str() + pos, lunId.c_str());
        return false;
    }

    pos = results.str().find_first_of("0123456789", lastPos);
    try
    {
        if (pos != std::string::npos)
        {
            lastPos = results.str().find_first_not_of("0123456789", pos);
            if (lastPos ==  std::string::npos)
                lastPos = results.str().size();//If no non-digit found after minor number,
            std::string min (results.str().c_str() + pos, lastPos - pos);
            minor = boost::lexical_cast<int>(min.c_str());
        }
        else
            return false;

    }
    catch(boost::bad_lexical_cast &)
    {
        DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::getMinMojorForMultipath - Unable to lexical cast string %s to minor number for the LUN %s.\n", results.str().c_str() + pos, lunId.c_str());
        return false;
    }

  return true;
 }

bool DeviceHelper::removeMultipathPartitions(const std::string& lunId)
{
    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::removeMultipathPartitions : Entered\n");
    std::string cmd("/sbin/dmsetup info --target multipath -c --noheadings 2>/dev/null | cut -d ':' -f1| grep ");
    cmd += lunId;
    cmd += "p"; //p for partition
    std::stringstream results;

    if (!executePipe(cmd, results)) 
    {
        return false;
    }

    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::removeMultipathPartitions: partitions = %s \n", results.str().c_str());
    std::string cmd1 ("/sbin/dmsetup remove -f /dev/mapper/");


    if(results.good())
    {
        std::string partition;
        while(std::getline(results,partition))
        {
            cmd1 += partition;
            std::stringstream results1;
            DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::removeMultipathPartitions: removing partition : %s \n ",cmd1.c_str());
            if (!executePipe(cmd1, results1))
            {
                DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::removeMultipathPartitions: failure");
                return false;
            }
        }
    }
    return true;
}


bool DeviceHelper::IsScsiDeviceExists(const string &h, const string &c, const string &t, const string &l) 
{
    struct stat statbuf;
    std::string deviceName = SYS_SCSI_DEV;
    deviceName += h + ":" + c + ":" + t + ":" + l;
    if( -1 == stat(deviceName.c_str(), &statbuf))
    {
         DebugPrintf(SV_LOG_DEBUG, "DeviceHelper::IsScsiDeviceExists devicename = %s, already deleted.\n", deviceName.c_str());
         return false;
    }
    DebugPrintf(SV_LOG_DEBUG, "DeviceHelper::IsScsiDeviceExists devicename = %s present.\n",deviceName.c_str());
    return true;
}
bool DeviceHelper::removeMultipathDevice(const std::string& lunId)
{
    struct stat statbuf;
    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::removeMultipathDevice : Entered\n");
    std::string deviceName = "/dev/mapper/";
    deviceName += lunId;
    DebugPrintf(SV_LOG_DEBUG, "DeviceHelper::removeMultipathDevice devicename = %s \n",deviceName.c_str());
    if( -1 == stat(deviceName.c_str(), &statbuf))
    {
         DebugPrintf(SV_LOG_DEBUG, "DeviceHelper::removeMultipathDevice devicename = %s, already deleted after checking with stat\n",
                deviceName.c_str());
         return true;
    }
    else
         DebugPrintf(SV_LOG_DEBUG, "DeviceHelper::removeMultipathDevice devicename = %s is present\n",deviceName.c_str());
    std::string cmd("/sbin/dmsetup remove -f ");
    cmd += deviceName;
    cmd += " 2>&1";
    std::stringstream results;

    if (!executePipe(cmd, results)) 
    {
        return false;
    }
     std::string error;
 
      while (!results.eof()) 
      {
        std::getline(results, error);
        if(error.empty())
        {
            return true;
        }
        else
        {
            DebugPrintf(SV_LOG_WARNING,"DeviceHelper::removeMultipathDevice failed with %s \n",error.c_str());
            return false;
        }
    }

    return true;
}



bool DeviceHelper::discoverResyncDevice(const std::string& lunId, std::string& deviceName,bool& deviceFound)
{
    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::discoverResyncDevice: Entered\n");
    deviceFound = false;
    std::string cmd("/sbin/dmsetup info --target multipath -c --noheadings 2>/dev/null | cut -d ':' -f1| grep ");
    cmd += lunId;
    std::stringstream results;

    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::discoverResyncDevice: Command to ExecutePipe is %s\n", cmd.c_str());

    if (!executePipe(cmd, results)) {
        return false;
    }

    if (results.str().empty() ) {
        return true;
    }
    else
    {
        deviceName = DEVICE_NAME_PREFIX;
        deviceName += lunId;
        deviceFound = true;
        DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::discoverResyncDevice: Lunid = %s, devicename + %s\n",lunId.c_str(),deviceName.c_str());
    }
    return true;
}


bool DeviceHelper::reportInitiators(std::vector<SanInitiatorSummary> &sanInitiatorList)
{
    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::reportInitiators: ENTERED \n" );
    bool ret = true;
    //std::vector<SanInitiatorSummary> sanInitiatorList;
    DIR             *dip = 0;
    struct dirent   *dit = 0; //directory iterator

    if ((dip = opendir(FC_HOST)) == NULL)
    {         
        DebugPrintf(SV_LOG_ERROR, " DeviceHelper::reportInitiators: opendir on %s failed\n",FC_HOST);
        return false;
    }
    while ((dit = readdir(dip)) != NULL)
    {            
        if (strncmp(dit->d_name, HOST_KEYWORD, strlen(HOST_KEYWORD)) == 0)
        {
            std::string currentPortName;
            std::string currentNodeName;
            std::string modelName;
            std::string modelDescription;
            std::string serialNumber;
            bool targetModeEnabled;

            std::string portNameFile = FC_HOST;
            portNameFile += PATH_SEPERATOR;
            portNameFile += dit->d_name;
            portNameFile += PATH_SEPERATOR;
            portNameFile += PORT_NAME_FILE;
            std::ifstream portNameFileStream(portNameFile.c_str(), std::ifstream::in);
            if (portNameFileStream)
            {
                portNameFileStream >> currentPortName;
                portNameFileStream.close();
            }
            else
            {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::reportInitiators::FAILED: %s open failure.\n",portNameFile.c_str());
                ret = false;
                break;
            }

            std::string nodeNameFile = FC_HOST;
            nodeNameFile += PATH_SEPERATOR;
            nodeNameFile += dit->d_name;
            nodeNameFile += PATH_SEPERATOR;
            nodeNameFile += NODE_NAME_FILE;
            std::ifstream nodeNameFileStream(nodeNameFile.c_str(), std::ifstream::in);
            if (nodeNameFileStream)
            {
                nodeNameFileStream >> currentNodeName;
                nodeNameFileStream.close();
            }
            else
            {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::reportInitiators::FAILED: %s open failure.\n",nodeNameFile.c_str());
                ret = false;
                break;
            }

            std::string modelNameFile = SCSI_HOST;
            modelNameFile += PATH_SEPERATOR;
            modelNameFile += dit->d_name;
            modelNameFile += PATH_SEPERATOR;
            modelNameFile += MODEL_NAME_FILE;
            std::ifstream modelNameFileStream(modelNameFile.c_str(), std::ifstream::in);
            if (modelNameFileStream)
            {
                modelNameFileStream >> modelName;
                modelNameFileStream.close();
            }
            else
            {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::reportInitiators::FAILED: %s open failure.\n",modelNameFile.c_str());
                ret = false;
                break;
            }

            std::string modelDescFile = SCSI_HOST;
            modelDescFile += PATH_SEPERATOR;
            modelDescFile += dit->d_name;
            modelDescFile += PATH_SEPERATOR;
            modelDescFile += MODEL_DESC_FILE;
            std::ifstream modelDescFileStream(modelDescFile.c_str(), std::ifstream::in);
            if (modelDescFileStream)
            {
                modelDescFileStream >> modelDescription;
                modelDescFileStream.close();
            }
            else
            {
            DebugPrintf(SV_LOG_ERROR, "DeviceHelper::reportInitiators::FAILED: %s open failure.\n",modelDescFile.c_str());
                ret = false;
                break;
            }
            std::string serialNumberFile = SCSI_HOST;
            serialNumberFile += PATH_SEPERATOR;
            serialNumberFile += dit->d_name;
            serialNumberFile += PATH_SEPERATOR;
            serialNumberFile += SERIAL_NUMBER_FILE;
            std::ifstream serialNumberFileStream(serialNumberFile.c_str(), std::ifstream::in);
            if (serialNumberFileStream)
            {
                serialNumberFileStream >> serialNumber;
                serialNumberFileStream.close();
            }
            else
            {
            DebugPrintf(SV_LOG_ERROR, "DeviceHelper::reportInitiators::FAILED: %s open failure.\n",serialNumberFile.c_str());
                ret = false;
                break;
            }

            std::string targetModeFile = SCSI_HOST;
            targetModeFile += PATH_SEPERATOR;
            targetModeFile += dit->d_name;
            targetModeFile += PATH_SEPERATOR;
            targetModeFile += TARGET_MODE_FILE;
            std::ifstream targetModeFileStream(targetModeFile.c_str(), std::ifstream::in);
            if (targetModeFileStream)
            {
                targetModeFileStream >> targetModeEnabled;
                targetModeFileStream.close();
            }
            else
            {
               targetModeEnabled = false;
            }

#if 0
            std::string nodeStateFile = SCSI_HOST;
            nodeStateFile += PATH_SEPERATOR;
            nodeStateFile += dit->d_name;
            nodeStateFile += PATH_SEPERATOR;
            nodeStateFile += PORT_STATE_FILE;

            std::string portStatus;
            std::ifstream stateNameFileStream(nodeStateFile.c_str(), std::ifstream::in);

            if (stateNameFileStream)
            {
                char state[32];    
                stateNameFileStream.getline(state, 32);
                state[31] = '\0';
                stateNameFileStream.close();
                portStatus = state;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "DeviceHelper::reportInitiators::FAILED: %s open failure.\n",nodeNameFile.c_str());
                ret = false;
                break;
            }
#endif

            SanInitiatorSummary sanInitiator;
            sanInitiator.PortWWN = getColonWWN(currentPortName);
            sanInitiator.NodeWWN = getColonWWN(currentNodeName);
            sanInitiator.ModelName = modelName;
            sanInitiator.ModelDesc = modelDescription;
            sanInitiator.SerialNum = serialNumber;
            sanInitiator.target_mode_enabled = targetModeEnabled;

            // std::transform(portStatus.begin(), portStatus.end(), portStatus.begin(), (int (*)(int))std::toupper);

            // sanInitiator.state = (portStatus.find ("LINK UP") != string::npos) ? 1 : 0;
            sanInitiator.state = true;

            DebugPrintf(SV_LOG_DEBUG, "scsi initiator : %s %s %s %s %d %d\n", sanInitiator.PortWWN.c_str(), sanInitiator.NodeWWN.c_str(), sanInitiator.ModelName.c_str(), sanInitiator.ModelDesc.c_str(), sanInitiator.SerialNum.c_str(), sanInitiator.target_mode_enabled, sanInitiator.state);     
     
            sanInitiatorList.push_back(sanInitiator); 
        }
    }    
    if (closedir(dip) == -1)
    {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::reportInitiators:: closedir FAILED\n");
    }
    if(ret)
    {
        //Collect initiator and target iqns for iscsi.
        //new command which will collect the iqn and ipaddress of the initiator.
        //for interface in `ls /var/lib/iscsi/ifaces`; do if [ -f /var/lib/iscsi/ifaces/$interface ]; then iqn=`cat /var/lib/iscsi/ifaces/$interface  | grep -v "^\s*\#" | sed "s/\#.*$//g" | grep "iface.initiatorname" | cut -d "=" -f2 | sed "s/\s*//g"`; ipAddress=`ifconfig $interface | grep "inet addr:" | sed "s/^\s*inet/ /g" | sed "s/  / /g" | cut -d " " -f 2| cut -d ":" -f 2`; echo "$iqn $ipAddress"; fi ; done
       //TODO: rewrite the command in readable english. 
        std::string cmd("iqn=:;ipAddress=:;");
        cmd += "for interface in `ls /var/lib/iscsi/ifaces`;";
        cmd += "do ";
        cmd += "if [ -f /var/lib/iscsi/ifaces/$interface ]; then ";
        cmd += "iqn=`cat /var/lib/iscsi/ifaces/$interface  | grep -v \"^\\s*\\#\" | sed \"s/\\#.*$//g\" | grep \"iface.initiatorname\" | cut -d \"=\" -f2 | sed \"s/\\s*//g\"`;";
        cmd += "ipAddress=`ifconfig $interface | grep \"inet addr:\" | sed \"s/^\\s*inet/ /g\" | sed \"s/  / /g\" | cut -d \" \" -f 2| cut -d \":\" -f 2`;";
        cmd += "if [ -z $ipAddress ]; then iqn=:;ipAddress=:;fi;";
        cmd += "echo \"$iqn $ipAddress\";";
        cmd += "fi;";
        cmd += "done";

        std::stringstream strInitiators;
        DebugPrintf(SV_LOG_DEBUG, "executing command to get iscsi initiator list: %s\n", cmd.c_str());
        if(executePipe(cmd, strInitiators))
        {
            if(strInitiators.good())
            {
                std::string strRecord;
                while(std::getline(strInitiators, strRecord))
                {
                    if(strRecord.find("iqn") != std::string::npos)
                    {
                        std::vector<std::string> vIqnIpPair;
                        Tokenize(strRecord, vIqnIpPair, " ");
                        DebugPrintf(SV_LOG_DEBUG, "initiator: %s IpAddress: %s\n", vIqnIpPair[0].c_str(), vIqnIpPair[1].c_str());
                        SanInitiatorSummary sanInitiator;
                        if(":" != vIqnIpPair[0])
                            sanInitiator.PortWWN = vIqnIpPair[0];
                        if(":" != vIqnIpPair[1])
                            sanInitiator.NodeWWN = vIqnIpPair[1];
                        sanInitiator.target_mode_enabled = false;
                        sanInitiator.state = true;
                        sanInitiatorList.push_back(sanInitiator);
                    }
                }
            }
        }
        //Get the IP address to which the scst is listening.
        /*
         * iqn=:;ipAddress=:;ipPort=:;arrCmd=(`cat /etc/init.d/iscsi-scst | grep iscsi-scstd`);for (( i = 0; i < ${#arrCmd[@]}; i++ )); do if [ "${arrCmd[$i]}" == "-a" ]; then ipAddress="${arrCmd[(( $i + 1 ))]}"; elif [ "${arrCmd[$i]}" == "-p" ]; then ipPort="${arrCmd[(( $i + 1 ))]}"; fi; done;iqn=`cat /etc/iscsi-scstd.conf | grep -v "^\s*\#" | grep "^\s*Target" | cut -d " " -f 2 | sed "s/\#.*$//g"`;if [ $ipAddress == ":" ]; then ipAddress=`ifconfig eth0 | grep "inet addr:" | sed "s/^\s*inet/ /g" | sed "s/  / /g" | cut -d " " -f 2| cut -d ":" -f 2`;fi;echo $iqn $ipAddress $ipPort;
        */
        cmd = "iqn=:;ipAddress=:;ipPort=:;";
        cmd += "arrCmd=(`cat /etc/init.d/iscsi-scst | grep iscsi-scstd`);";
        cmd += "for (( i = 0; i < ${#arrCmd[@]}; i++ ));";
        cmd += "do ";
        cmd += "if [ \"${arrCmd[$i]}\" == \"-a\" ]; then ipAddress=\"${arrCmd[(( $i + 1 ))]}\"; ";
        cmd += "elif [ \"${arrCmd[$i]}\" == \"-p\" ]; then ipPort=\"${arrCmd[(( $i + 1 ))]}\"; ";
        cmd += "fi;";
        cmd += "done;";
        cmd += "iqn=`cat /etc/iscsi-scstd.conf | grep -v \"^\\s*\\#\" | grep \"^\\s*Target\" | cut -d \" \" -f 2 | sed \"s/\\#.*$//g\"`;";
        cmd += "if [ $ipAddress == \":\" ]; then "; // return the ip of eth0 instead of : if no ip is found.
        cmd += "ipAddress=`ifconfig eth0 | grep \"inet addr:\" | sed \"s/^\\s*inet/ /g\" | sed \"s/  / /g\" | cut -d \" \" -f 2 | cut -d \":\" -f 2`;";
        cmd += "fi;";
        cmd += "echo $iqn $ipAddress $ipPort;";
        std::stringstream strTargets;
        DebugPrintf(SV_LOG_DEBUG, "executing command to get iscsi targets list: %s\n", cmd.c_str());
        if(executePipe(cmd, strTargets))
        {
            if(strTargets.good())
            {
                std::string strRecord;
                while(std::getline(strTargets, strRecord))
                {
                    if(strRecord.find("iqn") != std::string::npos)
                    {
                        DebugPrintf(SV_LOG_DEBUG, "target: %s\n", strRecord.c_str());
                        std::vector<std::string> vIqnIpPair;
                        Tokenize(strRecord, vIqnIpPair, " ");
                        if (vIqnIpPair.size() == 3)
                        {
                            SanInitiatorSummary sanInitiator;
                            if(":" != vIqnIpPair[0])
                                sanInitiator.PortWWN = vIqnIpPair[0];
                            if(":" != vIqnIpPair[1])
                                sanInitiator.NodeWWN = vIqnIpPair[1];
                            if(":" != vIqnIpPair[2])
                            {
                                sanInitiator.NodeWWN += ":";
                                sanInitiator.NodeWWN += vIqnIpPair[2];
                            }
                            sanInitiator.target_mode_enabled = true;
                            sanInitiator.state = true;
                            sanInitiatorList.push_back(sanInitiator);
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_DEBUG, "not reporting target as IP address is not configured: %s\n", strRecord.c_str());
                        }
                    }
                }
            }
        }
    }
    /*if(!ret)
    {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::reportInitiators::FAILED: file open failure?");
    }
    else
    {
        try {
            TheConfigurator->getVxAgent().SanRegisterInitiators(sanInitiatorList);
        }catch (ContextualException const & ie) {
            ret = false;
            DebugPrintf(SV_LOG_ERROR, "DeviceHelper::reportInitiators:%s", ie.what());             
        } catch (std::exception const & e) {
            ret = false;
            DebugPrintf(SV_LOG_ERROR, "DeviceHelper::reportInitiators:%s", e.what());            
        } catch (...) {
            ret = false;
            DebugPrintf(SV_LOG_ERROR, "DeviceHelper::reportInitiators: unknown exception\n");            
        }
    }*/
    return ret;
}


bool DeviceHelper::removeScsiDevice(std::string& hostId,std::string& channelId,std::string& targetId, std::string &lunNo)
{
    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::removeScsiDevice  ENTERED \n" );

    std::string searchString = hostId;
    searchString += ":";
    searchString += channelId;
    searchString += ":";
    searchString += targetId;
    searchString += ":";
    searchString += lunNo;

    std::string deleteFile = SYS_BUS_SCSI;
    deleteFile += "/";
    deleteFile += searchString;
    deleteFile += "/";
    deleteFile += "delete";

    std::ofstream deleteDevice(deleteFile.c_str(),std::ofstream::out);
    if(!deleteDevice)
    {
        DebugPrintf(SV_LOG_ERROR, "DeviceHelper::removeScsiDevice Failed: Unable to open file %s for writing \n",deleteFile.c_str());
        return false;
    }
    deleteDevice << "1";
    deleteDevice.close();

    return true;
}

bool DeviceHelper::removeAllDevice(const std::string& hostId, const std::string& channelId, const std::string& targetId)
{
    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::removeAllDevice  ENTERED \n" );
    bool ret = true;
    DIR             *dip = 0;
    struct dirent   *dit = 0; //directory iterator

    if ((dip = opendir(SYS_BUS_SCSI)) == NULL)
    {         
        DebugPrintf(SV_LOG_ERROR, " DeviceHelper::removeAllDevice: opendir on %s failed\n",SYS_BUS_SCSI);
        return false;
    }

    std::string searchString = hostId;
    searchString += ":";
    if (channelId.size())
    {
        searchString += channelId;
        searchString += ":";
    }
    if (targetId.size())
    {
        searchString += targetId;
    }
    while ((dit = readdir(dip)) != NULL)
    {            
        if (strncmp(dit->d_name, searchString.c_str(), searchString.size()) == 0)
        {
            std::string deleteFile = SYS_BUS_SCSI;
            deleteFile += "/";
            deleteFile += dit->d_name;
            deleteFile += "/";
            deleteFile += "delete";

            std::ofstream deleteDevice(deleteFile.c_str(),std::ofstream::out);
            if(!deleteDevice)
            {
                DebugPrintf(SV_LOG_ERROR, "DeviceHelper::removeAllDevice Failed: Unable to open file %s for writing \n",deleteFile.c_str());
                return false;
            }
            deleteDevice << "1";
            deleteDevice.close();
            sleep(1);
        }
    }
    closedir(dip);
    DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::removeAllDevice  EXITED \n" );
    return true;

}

bool DeviceHelper::doRebootOperation()
{
     DebugPrintf(SV_LOG_DEBUG,"DeviceHelper::doRebootOperation: issuing scan\n");
    DIR             *dip = 0;
    struct dirent   *dit = 0; //directory iterator

    if ((dip = opendir(FC_HOST)) == NULL)
    {
        DebugPrintf(SV_LOG_ERROR, " DeviceHelper::doRebootOperation: opendir on /sys/class/fc_host FAILED\n");
        return false;
    }
    while ((dit = readdir(dip)) != NULL)
    {
        if (strncmp(dit->d_name, HOST_KEYWORD, strlen(HOST_KEYWORD)) == 0)
        {
            std::string hostId = dit->d_name;
            hostId.erase(0,strlen(HOST_KEYWORD));
            issueScan(hostId);
        }
    }             
    closedir(dip);
return true;
}

