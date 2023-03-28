#ifndef DEVICE_HELPER_H
#define DEVICE_HELPER_H

#include "atconfigmanagersettings.h"
#include <string>

extern Configurator* TheConfigurator; 

bool InitializeConfigurator();
class DeviceHelper
{  

    struct ScsiDevInfo
    {
        std::string h;
        std::string c;
        std::string t;
        std::string l;
        ScsiDevInfo(const std::string& hh, const std::string& cc, const std::string& tt, const std::string& ll)
                :h(hh),c(cc),t(tt),l(ll){}

    };
 protected:
    bool issueScan (const std::string & hostId);
    bool issueLip (const std::string & hostId);
    bool getTargetChannel (const std::string & hostId,
             const std::string & aiPortName,
             const std::string & targetPortName,
             std::vector<std::string> & channelIdvec,
             std::vector<std::string> & targetIdvec, bool & targetFound);
    bool addDevice (const std::string & hostId, const std::string & channelId,
             const std::string & targetId,
             const std::string & lunNumber);
    bool removeDevice (const std::string & hostId,
             const std::string & channelId,
             const std::string & targetId,
             const std::string & lunNumber);
    void procesDiscoverResyncDevice (DiscoverBindingDeviceSetting &
             discoverBindingDeviceSetting);
    void RemoveResyncDevice (DiscoverBindingDeviceSetting &
             discoverBindingDeviceSetting);
    std::string getHexWWN (const std::string & wwn);
    bool discoverResyncDevice (const std::string & lunId,
             std::string & deviceName, bool & deviceFound);
    bool removeScsiDevice(std::string& hostId,std::string& channelId,std::string& targetId, std::string &lunNo);
    bool removeMultipathDevice(const std::string& lunId);
    bool removeMultipathPartitions(const std::string& lunId);
    bool IsScsiDeviceExists(const std::string &h, const std::string &c, const std::string &t, const std::string &l); 
    bool isMPDeviceReadable(const std::string &device);
    bool getMinMojorForMultipath(const std::string& lunId, int &major, int &minor);


 public:
     DeviceHelper ();
     ~DeviceHelper ();

     bool reportInitiators(std::vector<SanInitiatorSummary> &sanInitiatorList);
    std::string getColonWWN (const std::string & wwn);
     bool doRebootOperation();
     bool discoverMappedDevice(DiscoverBindingDeviceSetting &);
     bool doDeviceOperation (DiscoverBindingDeviceSettingList
             discoverBindingDeviceSettingsList);
     bool getHostId (const std::string & portName, const std::string & targetPortName, std::vector<std::string>& hostIdVec, bool & hostIdFound);
     bool getFCHostId(const std::string &portName, std::vector<std::string>& hostIdVec, bool& hostIdFound);
     bool getFCTargetChannel(const std::string& hostId, const std::string& targetPortName, std::vector<std::string>& channelIdvec, std::vector<std::string>& targetIdvec, bool& targetFound);
     bool getISCSIHostId(const std::string &portName, const std::string &targetName, std::vector<std::string>& hostIdVec, bool& hostIdFound);
     bool getISCSITargetChannel(const std::string& hostId, const std::string& targetPortName, std::vector<std::string>& channelIdVec, std::vector<std::string>& targetIdVec, bool& targetFound);
     bool removeAllDevice(const std::string& hostId, const std::string& channelId="", const std::string &targetId="");

 private:

};

#endif /* DeviceHelper_H */
