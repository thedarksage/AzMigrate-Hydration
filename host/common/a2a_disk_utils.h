#ifndef A2A_DISK_UTILS_H
#define A2A_DISK_UTILS_H

#include "FabricDetails.h"
#include "imds.h"
#include <map>
#include <set>


const std::string LINUX_ROOT_DISK("RootDisk");
const std::string LINUX_DATA_DISK("DataDisk");

#define VACP_VM_OS_DISKID                           "OsDiskId"
#define VACP_VM_MAP_DISKID_TO_ARMID_FROM_SETTINGS   "MapDiskIdToArmIdFromSettings"
#define VACP_VM_A2A_MODE                            "IsA2AMode"


class VacpAgentReplicationSettings
{
public:

    std::map<std::string, std::string> DiskIdArmIdMapping;

    void serialize(JSON::Adapter& adapter)
    {
        JSON::Class root(adapter, "VacpAgentReplicationSettings", false);

        JSON_KV_T(adapter, "DiskIdArmIdMapping", DiskIdArmIdMapping);
    }

    void serialize(ptree& node)
    {
        JSON_KV_P(node, DiskIdArmIdMapping);
    }
};

namespace A2ADiskUtils {
    static void CheckDiskRestoreOrSwap(
        const std::map<std::string, std::string>& protectedDiskArmIds,
        const std::map<std::string, std::string>& diskArmIdsDiscovery,
        std::set<std::string>& swappedDisks,
        std::set<std::string>& restoredDisks)
    {
        DebugPrintf(SV_LOG_DEBUG, "Entered %s\n", FUNCTION_NAME);

        std::stringstream   ssRestoredDisks;
        std::stringstream   ssSwappedDisks;

        std::map<std::string, std::string>::const_iterator protectedDiskIdit = protectedDiskArmIds.begin();
        for (/*empty*/;
            protectedDiskIdit != protectedDiskArmIds.end();
            protectedDiskIdit++)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: DiskId %s ARMID: %s found in settings\n", FUNCTION_NAME,
                protectedDiskIdit->first.c_str(),
                protectedDiskIdit->second.c_str());

            std::map<std::string, std::string>::const_iterator armIdDiscoveryIt = diskArmIdsDiscovery.find(protectedDiskIdit->first);
            if (armIdDiscoveryIt == diskArmIdsDiscovery.end())
            {
                DebugPrintf(SV_LOG_ERROR, "%s: DiskId %s not found in discovery\n", FUNCTION_NAME, protectedDiskIdit->first.c_str());
                continue; // disk not found in discovery
            }

            DebugPrintf(SV_LOG_DEBUG, "%s: DiskId %s ARMID: %s found in discovery\n", FUNCTION_NAME, protectedDiskIdit->first.c_str(), protectedDiskIdit->second.c_str());
            if (boost::iequals(protectedDiskIdit->second, armIdDiscoveryIt->second))
            {
                // Armid Matched continue
                DebugPrintf(SV_LOG_INFO, "%s: DiskId %s ARMID: %s is not restored or swapped.\n", FUNCTION_NAME,
                    protectedDiskIdit->first.c_str(),
                    protectedDiskIdit->second.c_str());

                continue;
            }

            std::string     discoveryArmdId = armIdDiscoveryIt->second;

            // Search if this armid is present in existing settings

            std::map<std::string, std::string>::const_iterator protectedDiskArmIdsIt = protectedDiskArmIds.begin();
            for (; protectedDiskArmIds.end() != protectedDiskArmIdsIt; protectedDiskArmIdsIt++) {
                if (boost::iequals(protectedDiskArmIdsIt->second, discoveryArmdId)) {
#ifndef SV_WINDOWS
                    swappedDisks.insert(protectedDiskArmIdsIt->second);
#endif
                    DebugPrintf(SV_LOG_ERROR, "%s: DiskId %s ARMID %s is swapped with ARMID %s.\n", FUNCTION_NAME,
                        protectedDiskIdit->first.c_str(), protectedDiskIdit->second.c_str(),
                        discoveryArmdId.c_str());
                    break;
                }
            }

            if (protectedDiskArmIds.end() != protectedDiskArmIdsIt) {
                continue;
            }

            restoredDisks.insert(protectedDiskIdit->second);
            DebugPrintf(SV_LOG_ERROR, "%s: DiskId %s ARMID %s is replaced with %s.\n", FUNCTION_NAME,
                protectedDiskIdit->first.c_str(),
                protectedDiskIdit->second.c_str(),
                armIdDiscoveryIt->second.c_str());
        }
        DebugPrintf(SV_LOG_DEBUG, "Exit %s\n", FUNCTION_NAME);
        return;
    }

    /// <summary>
        /// Detects whether any protected disk is rstored from backup
        /// </summary>
        /// <In param name="mapDiskIdToArmIdFromSettings">map of DiskId to ArmId from settings</param>
        /// <In param name="osDiskId">os DiskId</param>
        /// <In param name="mapDataDiskIdToLunFromDiscovery">map of data DiskId to Lun from discovery</param>
        /// <Out param name="errormsg">any error during check</param>
        /// <returns>true on check successful fail otherwise</returns>
        static bool CheckDiskRestoreOrSwap(
            const std::map<std::string, std::string>& mapDiskIdToArmIdFromSettings,
            const std::string& osDiskId,
            const std::map<std::string, std::string>& mapDataDiskIdToLunFromDiscovery,
            std::string& errormsg)
        {
            /*
                Note that the diskID in Linux is in for /dev/sda, /dev/sdb which can shuffle on VM reboot. For this reason, agent derive disk ID for Linux from disk LUN ID and the derived disk ID along
                with other disk properties is registered to RCM which RCM send as part of replication settings(Ref.https://learn.microsoft.com/en-us/azure/virtual-machines/linux/azure-to-guest-disk-mapping).
                Note that disk swap does not guarantee the same LUN number for disk. So, challenge here is to ensure disk removal should not be considered as disk restore as the original diskID mapping
                to disk ARM ID change in case of disk swap, For example, original disk mapping is,
                    LUN 0 = /dev/sda = DataDisk0 <=> diskARMID1
                    LUN 1 = /dev/sdb = DataDisk1 <=> diskARMID2
                    After swapping it will look like,
                    LUN 0 = /dev/sdb = DataDisk0 <=> diskARMID2
                    LUN 1 = /dev/sda = DataDisk1 <=> diskARMID1
                Note that LUN ID mapping is also swapped with diskARMID so this should not be identified as a restore
                After detaching both disk and only diskARMID2 is attached back then again this is a swap like,
                    LUN 0 = /dev/sdb = DataDisk0 <=> diskARMID2
                Note that LUN 0 is mapped to diskARMID2 and not to original mapping to diskARMID1 which is now removed so is not present in disk ARM ID list fetched from IMDS, so this is not considered as
                a disk restore. This need to be correctly identified.
                Prepare map of Protected And Locally Discovered Lun To ArmId.
                Ignore disk which is protected but missing in local discovery as this is considered as temporary detached disk.
                Match each LUN in set of Discovered Lun with corresponding LUN in map of DiskId To ArmId From Settings and prepare map of Protected And Locally Discovered Lun To ArmId
                Prepare map of Disk ArmId To Lun From Imds
                Ensure each disk ARM ID in map of Protected And Locally Discovered Lun To ArmId is also present in map of Disk ArmId To Lun From Imds. If not the this is restored disk.
                Ensure each LUN in Protected And Locally Discovered Lun To ArmId match with corresponding LUN in map of Disk ArmId To Lun From Imds.If not the this is swapped disk.
                */
            errormsg.clear();
            std::stringstream ss;

            AzureInstanceMetadata::StorageProfile storProfile;
            if (!ImdsLibNamespace::ImdsLib::GetInstance()->GetVMStorageProfile(false, storProfile)) {
                errormsg = "Failed to get the VM storage profile from IMDS.";
                return false;
            }

            if (osDiskId.empty()) {
                DebugPrintf("%s: input param OS Disk is empty\n", FUNCTION_NAME);
                return false;

            }

            if (mapDiskIdToArmIdFromSettings.empty()) {
                DebugPrintf("%s Error: disk id to armid map is empty\n", FUNCTION_NAME);
                return false;
            }

            if (mapDataDiskIdToLunFromDiscovery.empty()) {
                DebugPrintf("%s Error: disk id to lun map is empty\n", FUNCTION_NAME);
                return false;
            }

            // Check RootDisk ARMID in settings match with osDisk ARMID in IMDS
            std::map<std::string, std::string>::const_iterator citOsDisk = mapDiskIdToArmIdFromSettings.find(osDiskId);
            if (citOsDisk == mapDiskIdToArmIdFromSettings.end()) {
                // OS Disk is not present in settings
                ss << "OS disk ID " << osDiskId << " not present in mapDiskIdToArmIdFromSettings";
                DebugPrintf("%s: %s\n", FUNCTION_NAME, ss.str().c_str());
                return false;
            }

            if (!boost::iequals(citOsDisk->second, boost::algorithm::to_lower_copy(storProfile.osDisk.managedDisk.id))) {
                // OS disk ARMID is different than the armid in settings
                DebugPrintf("Entered %s DiskId: %s ARMID in settings: %s Current ArmId: %s\n", FUNCTION_NAME, citOsDisk->first.c_str(),
                    citOsDisk->second.c_str(),
                    boost::algorithm::to_lower_copy(storProfile.osDisk.managedDisk.id).c_str());
                return true;
            }

            // Prepare mapDiskArmIdToLunFromImds
            std::map<std::string, std::string> mapDiskArmIdToLunFromImds;
            for (std::vector<AzureInstanceMetadata::Disk>::const_iterator it = storProfile.dataDisks.begin();
                it != storProfile.dataDisks.end(); it++) {
                const std::string& armid = boost::algorithm::to_lower_copy(it->managedDisk.id);
                const std::string& lun = boost::algorithm::to_lower_copy(it->lun);

                mapDiskArmIdToLunFromImds[lun] = armid;
                DebugPrintf("%s IMDS Info: Lun: %s ARMID: %s\n", FUNCTION_NAME, lun.c_str(), armid.c_str());
            }

            // Check for data disk restore/swap
            std::map<std::string, std::string>::const_iterator citDataDisk = mapDiskIdToArmIdFromSettings.begin();
            for (; mapDiskIdToArmIdFromSettings.end() != citDataDisk; citDataDisk++) {
                std::string armidInSettings = citDataDisk->second;
                std::string deviceId = citDataDisk->first;

                if (boost::iequals(deviceId, osDiskId)) {
                    continue;
                }
                //DebugPrintf(SV_LOG_DEBUG, "%s Info: Validation from settings DiskId: %s ArmId: %s\n", FUNCTION_NAME, deviceId.c_str(), armidInSettings.c_str());

                // Step1: Get Lun number from local discovery
                //        Not found. can be case of disk removal
                if (mapDataDiskIdToLunFromDiscovery.end() == mapDataDiskIdToLunFromDiscovery.find(deviceId)) {
                    DebugPrintf("%s Error: DiskId: %s not found in local discovery\n", FUNCTION_NAME, deviceId.c_str());
                    continue;
                }

                // Get Lun number from local discovery
                std::string     lunNumber = mapDataDiskIdToLunFromDiscovery.find(deviceId)->second;
                DebugPrintf("%s Discovery Info: DiskId: %s Lun: %s\n", FUNCTION_NAME, deviceId.c_str(), lunNumber.c_str());

                // For this lun number figure ARMID from IMDS query
                if (mapDiskArmIdToLunFromImds.end() == mapDiskArmIdToLunFromImds.find(lunNumber)) {
                    // This lun number is not found
                    // This is error condition..
                    // From inside and outside lun number has to be same
                    DebugPrintf("%s Error: DiskId: %s Lun: %s not found in IMDS discovery\n", FUNCTION_NAME, citDataDisk->first.c_str(), lunNumber.c_str());
                    continue;
                }

                std::string armidInImds = mapDiskArmIdToLunFromImds.find(lunNumber)->second;
                DebugPrintf("%s Imds Info: Lun: %s ArmId: %s\n", FUNCTION_NAME, lunNumber.c_str(), armidInImds.c_str());

                if (boost::iequals(armidInImds, armidInSettings)) {
                    DebugPrintf("%s Matched: armidInImds: %s armidInSettings: %s\n", FUNCTION_NAME, armidInImds.c_str(), armidInSettings.c_str());
                    continue;
                }

                DebugPrintf("%s Error: DiskId: %s Lun: %s ArmId in settings: %s IMDS armId: %s\n",
                    FUNCTION_NAME,
                    deviceId.c_str(),
                    lunNumber.c_str(),
                    armidInSettings.c_str(),
                    armidInImds.c_str()
                );
                return true;
            }

            DebugPrintf("%s All Armid matched with DiskId\n", FUNCTION_NAME);
            return false;
        }
}
#endif