
//
//  File: storagefailover.h
//
//  Description:
//

#ifndef STORAGEFAILOVER_H
#define STORAGEFAILOVER_H

#include <string>
#include <list>
#include <map>
#include <set>

class StorageFailoverManager {
public:
    static int const RESULT_NOT_FOUND = 254;
    static int const RESULT_ERROR = -1;
    static int const RESULT_SUCCESS = 0;

    explicit StorageFailoverManager(std::string const& hostId, bool failOver, bool migration)
        : m_sourceHostId(hostId),
          m_failOver(failOver),
          m_primaryNode(false),
          m_migration(migration)
        { }

    int doFailover();

    int getIScsiInitiatorNodeName(std::string& nodeName);

protected:
    typedef std::set<std::string> iScsiTargets_t;

    struct DmInfo {
        std::string m_name;
        std::string m_scsiId;
        std::string m_partition;
    };

    typedef std::map<std::string, DmInfo> dmInfos_t;

    struct IScsiPortal  {
        std::string m_ipAddress;
        std::string m_ipPortNumber;
    };
    
    struct IScsiTargetPortal {
        IScsiPortal m_portal;
        std::string m_targetName;
    };

    typedef std::map<std::string, IScsiTargetPortal> iScsiTargetPortals_t;
    
    struct IScsiDisk {
        std::string m_name;
        iScsiTargetPortals_t m_targetPortals;
    };

    typedef std::map<std::string, IScsiDisk> iScsiDisks_t;

    struct IScsiTargetPairInfo {
        explicit IScsiTargetPairInfo()
            : m_destinationOffset(0)
            {}

        explicit IScsiTargetPairInfo(std::string const& sourceDeviceName,
                                     std::string const& sourceDeviceId,
                                     std::string const& sourceGroupName,
                                     std::string const& sourceResourceName,
                                     std::string id,
                                     unsigned long long offset)
            : m_sourceDeviceName(sourceDeviceName),
              m_sourceDeviceId(sourceDeviceId),
              m_sourceGroupName(sourceGroupName),
              m_sourceResourceName(sourceResourceName),
              m_destinationDiskId(id),
              m_destinationOffset(offset)
            {}

        std::string m_sourceDeviceName;
        std::string m_sourceDeviceId;
        std::string m_sourceGroupName;
        std::string m_sourceResourceName;
        std::string m_destinationDiskId;

        unsigned long long m_destinationOffset;
    };

    typedef std::map<std::string, IScsiTargetPairInfo> pairsByTargetDevice_t;

    unsigned int getSectorSize(std::string const & deviceName);
    std::string getDiskId(std::string const& diskName);    
    unsigned long long getDiskOffset(std::string const& deviceName, unsigned int sectorSize);

    int getIScsiDisks();
    int getPairs();
    int stopAgent();
    int startAgent();
    int getDmInfo();

    bool validateIscsiadm();
    bool parseSqlResults(std::string const& results);
    bool rollbackPairs();

    void getDiskIdAndOffset(std::string const& deviceName, std::string& id, unsigned long long& offset);
    void logoutAllTargets();
    void reportMappings();
    void parsePortalInfo(std::string const& portalInfo, IScsiPortal& portal);
    void addDmInfo(std::string uuid, DmInfo& dmInfo);
    void getDeviceNameForMpathName(DmInfo const& dmInfo, std::string& deviceName);
    void addIScsiDisk(std::string const& diskName, IScsiTargetPortal const& iScsiTargetPortal);

private:
    std::string m_sourceHostId;

    bool m_failOver;    
    bool m_primaryNode;
    bool m_migration;

    iScsiDisks_t m_iScsiDisks;

    pairsByTargetDevice_t m_pairsByTargetDevice;

    iScsiTargets_t m_iScsiTargets;

    dmInfos_t m_dmInfos;
};

#endif // STORAGEFAILOVER_H
