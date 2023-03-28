
//
//  File: storagefailover.cpp
//
//  Description:
//


// note:
// MA?YBE: support all FC and iSCSI storage even when
// doing migration.
// 
// for now migration supports both iSCSI and fiber channel
// when migration option is used, it is up to the user to
// manual add lun access to the source side prior to starting
// migration and manual remove access to the luns on the
// target side after migration has completed, as such all
// iSCSI related calls will be skipped when doing migration

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <linux/fs.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <dirent.h>
#include <algorithm>
#include <vector>

#include<boost/tokenizer.hpp>
#include<boost/shared_ptr.hpp>
#include<boost/bind.hpp>

#include "../storagefailover.h"
#include "../prockill.h"
#include "cdpcli.h"

typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;

bool executePipe(std::string const & command, std::stringstream & results)
{
    FILE* pipe = popen(command.c_str(), "r");
    if (0 == pipe) {
        results << "popen failed: " << errno << '\n';
        return false;
    }

    char buffer[512];
    size_t bytesRead;
    do {
        bytesRead = fread(buffer, 1, sizeof(buffer), pipe);
        if (ferror(pipe)) {
            results.str(std::string());
            results << "fread pipe failed: " << errno << '\n';
            pclose(pipe);
            return false;
        }
        results.write(buffer, bytesRead);
    } while (!feof(pipe));

    pclose(pipe);

    // reset for user to read from begining
    results.clear();
    results.seekg(0, std::ios::beg);

    return true;
}

unsigned int StorageFailoverManager::getSectorSize(std::string const & deviceName)
{
    // TODO: should not default this
    unsigned int sectorSize = 512;

    int fd = open(deviceName.c_str(), O_RDONLY);
    if (-1 == fd) {
        return sectorSize;
    }

    ioctl(fd, BLKSSZGET, &sectorSize);

    close(fd);

    return sectorSize;
}

void StorageFailoverManager::addDmInfo(std::string uuid, DmInfo& dmInfo )
{
    // uuid looks something like one of the following
    // mpath-360060e80108291b004d41f5b00000003
    // part1-mpath-360060e80108291b004d41f5b00000003
    // LVM-GNIi2v5djeUwLouoQhW6EOcXDqOASgiJcCGCwyqBcv0nxnjX3Soef3AO7RylrrXj
    //
    // only want the mpath ones (i.e. the ones that have mpath in the name
    // TODO: may need to worry about non-mpath names

    boost::char_separator<char> dashSep("-");
    tokenizer_t tokens(uuid, dashSep);
    tokenizer_t::iterator dasIter(tokens.begin());
    tokenizer_t::iterator dasIterEnd(tokens.end());
    if (dasIter != dasIterEnd) {
        if (std::string::npos != (*dasIter).find("part")) {
            dmInfo.m_partition = *dasIter;
            ++dasIter; // skip over part
            ++dasIter; // skip over mpath
            dmInfo.m_scsiId = *dasIter;
        } else if (*dasIter == "mpath") {
            ++dasIter; // skip over mpath
            dmInfo.m_scsiId = *dasIter;
        }
    }

    if (!dmInfo.m_scsiId.empty()) {
        m_dmInfos.insert(std::make_pair(dmInfo.m_name, dmInfo));
    }
}

int StorageFailoverManager::getDmInfo()
{
    std::string cmd("/sbin/dmsetup info --noheadings -c -o name,uuid");
    std::stringstream results;

    if (!executePipe(cmd, results)) {
        std::cout << results.str() << '\n';
        return RESULT_ERROR;
    }

    // get info back
    // name:uuid
    // e.g.
    // mpath5p1:part1-mpath-360060e80108291b004d41f5b00000003
    // 360060e80108291b004d41f5b00000003:mpath-360060e80108291b004d41f5b00000003
    // VolGroup00-LogVol01:LVM-GNIi2v5djeUwLouoQhW6EOcXDqOASgiJcCGCwyqBcv0nxnjX3Soef3AO7RylrrXj

    std::string dmData(results.str());
    boost::char_separator<char> newlineSep("\n");
    tokenizer_t lines(dmData, newlineSep);
    tokenizer_t::iterator newLineIter(lines.begin());
    tokenizer_t::iterator newLineIterEnd(lines.end());
    if (newLineIter != newLineIterEnd) {
        DmInfo dmInfo;
        do {
            boost::char_separator<char> colonSep(":");
            tokenizer_t tokens(*newLineIter, colonSep);
            tokenizer_t::iterator colonIter(tokens.begin());
            tokenizer_t::iterator colonIterEnd(tokens.end());
            dmInfo.m_name = *colonIter++;
            addDmInfo(*colonIter, dmInfo);
            ++newLineIter;
        } while (newLineIter != newLineIterEnd);

    }

    return RESULT_SUCCESS;
}

void StorageFailoverManager::getDeviceNameForMpathName(DmInfo const& dmInfo, std::string& deviceName)
{
    std::string byIdName("/dev/disk/by-id/scsi-");

    byIdName += dmInfo.m_scsiId;

    if (!dmInfo.m_partition.empty()) {
        byIdName += "-";
        byIdName += dmInfo.m_partition;
    }
    
    char symlnk[256];
        
    int len = readlink(byIdName.c_str(), symlnk, sizeof(symlnk) - 1);

    if (-1 == len) {
        std::cout << "no link found: " << errno << '\n';
        return;
    }
    symlnk[len] = '\0';

    // skip over any .. parts
    char* name = symlnk;

    while ('\0' != *name) {
        if ('.' == *name) {
            ++name;
        } else if ('/' == *name) {
            ++name;
            if ('.' != *name) {                
                break;
            }
        } else {
            break;
        }
    }
    
    deviceName = name;        
}

unsigned long long StorageFailoverManager::getDiskOffset(std::string const& deviceName, unsigned int sectorSize)
{
    // TODO: should look up sys file dir in /proc/mounts
    std::string startOffsetFile("/sys/block/");

    std::string::size_type idx = deviceName.find_first_of("0123456789");
    if (std::string::npos == idx) {
        // not a pratition
        return 0;
    }
    
    startOffsetFile += deviceName.substr(0, idx);
    startOffsetFile += "/";
    startOffsetFile += deviceName;
    startOffsetFile += "/start";

    std::ifstream startFile(startOffsetFile.c_str());
    if (!startFile) {
        return 0;
    }

    unsigned long startOffset;

    startFile >> startOffset;
    
    return (startOffset * sectorSize);
    
}
void StorageFailoverManager::getDiskIdAndOffset(std::string const& destinationName, std::string& diskId, unsigned long long& offset)
{
    std::string name;
    std::string::size_type idx = destinationName.find_last_of("/");
    if (std::string::npos != idx) {
        name = destinationName.substr(idx + 1);
    } else {
        name = destinationName;
    }

    std::string deviceName;
    std::string partitionName;
    
    dmInfos_t::iterator iter(m_dmInfos.find(name));
    if (m_dmInfos.end() == iter) {
        // TODO: not an mpath name, is this good enogh for all cases?
        std::string idName("/block/");
        idName += deviceName;
        diskId = getDiskId(idName);

        
    } else {
        getDeviceNameForMpathName((*iter).second, deviceName);
        diskId = (*iter).second.m_scsiId;        
    }

    std::string tmpName("/dev/");
    tmpName += deviceName;
    unsigned int sectorSize = getSectorSize(tmpName);

    offset = getDiskOffset(deviceName, sectorSize);
}

void StorageFailoverManager::parsePortalInfo(std::string const& portalData, IScsiPortal& portal)
{
    std::string::size_type idx1 = portalData.find_first_of(":");
    if (std::string::npos != idx1) {
        portal.m_ipAddress = portalData.substr(0, idx1);

        std::string::size_type idx2 = portalData.find_first_of(",");
        if (std::string::npos != idx2) {
            portal.m_ipPortNumber = portalData.substr(idx1 + 1, idx2 - idx1 - 1);
        }
    }
}

void StorageFailoverManager::addIScsiDisk(std::string const& diskName, IScsiTargetPortal const& iScsiTargetPortal)
{
    std::string tmpName("/block/");
    tmpName += diskName;
    std::string diskId = getDiskId(tmpName);

    iScsiDisks_t::iterator findIter(m_iScsiDisks.find(diskId));
    if (m_iScsiDisks.end() != findIter) {
        (*findIter).second.m_targetPortals.insert(std::make_pair(iScsiTargetPortal.m_portal.m_ipAddress
                                                                 + iScsiTargetPortal.m_portal.m_ipPortNumber
                                                                 + iScsiTargetPortal.m_targetName,
                                                                 iScsiTargetPortal));
    } else {
        IScsiDisk iScsiDisk;
        iScsiDisk.m_name = diskName;
        iScsiDisk.m_targetPortals.insert(std::make_pair(iScsiTargetPortal.m_portal.m_ipAddress
                                                        + iScsiTargetPortal.m_portal.m_ipPortNumber
                                                        + iScsiTargetPortal.m_targetName,
                                                        iScsiTargetPortal));
        m_iScsiDisks.insert(std::make_pair(diskId, iScsiDisk));
    }
}
    
int StorageFailoverManager::getIScsiDisks()
{
    std::string cmd("/sbin/iscsiadm -m session -P 3 2>&1 | grep -e 'Target:' -e 'Current Portal:' -e 'Attached scsi disk' -e 'iscsiadm:'");
    std::stringstream results;

    if (!executePipe(cmd, results)) {
        std::cout << results.str() << '\n';
        return RESULT_ERROR;
    }

    // should have blocks of lines of the following (if there were any sessions)
    // Target: t2hd2
    //     Current Portal: 10.80.9.25:3260,1
    //         Attached scsi disk sdg   State: running
    //         Attached scsi disk sde   State: running


    std::string iScsiAdm(results.str());
    boost::char_separator<char> newlineSep("\n");
    tokenizer_t lines(iScsiAdm, newlineSep);
    tokenizer_t::iterator newLineIter(lines.begin());
    tokenizer_t::iterator newLineIterEnd(lines.end());

    if (newLineIter != newLineIterEnd) {
        IScsiTargetPortal iScsiTargetPortal;
        do {
            boost::char_separator<char> whiteSpaceSep(" \t");
            tokenizer_t tokens(*newLineIter, whiteSpaceSep);
            tokenizer_t::iterator whiteSpaceIter(tokens.begin());
            tokenizer_t::iterator whiteSpaceIterEnd(tokens.end());
            if (whiteSpaceIter != whiteSpaceIterEnd) {
                if (*whiteSpaceIter == "iscsiadm:") {
                    // when iscsiadm: shows up it is either an error
                    // or no sessions, just return all the info
                    // and check if error or no sessions
                    std::cout << results.str() << '\n';
                    if (std::string::npos != results.str().find("No active sessions")) {
                        return RESULT_NOT_FOUND;
                    }
                    return RESULT_ERROR;
                } else if (*whiteSpaceIter == "Target:") {
                    // have a line like
                    // Target: t2hd2
                    // want the target name (t2hd2 in this example)
                    ++whiteSpaceIter; // skip over Target:
                    iScsiTargetPortal.m_targetName = *whiteSpaceIter;
                } else if (*whiteSpaceIter == "Current") {
                    // have a line like
                    //     Current Portal: 10.80.9.25:3260,1
                    // want the ip and port (but not the ,1), howerver just take
                    // the the ip port and ,1 and let parsePortalInfo handle it
                    ++whiteSpaceIter; // skip over Current
                    ++whiteSpaceIter; // skip over Portal:
                    parsePortalInfo(*whiteSpaceIter, iScsiTargetPortal.m_portal);
                } else if (*whiteSpaceIter == "Attached") {
                    // have a line like
                    //   Attached scsi disk sdg   State: running
                    // want the device name (sdg in this example)
                    ++whiteSpaceIter; // skip over Attached
                    ++whiteSpaceIter; // skip over scsi
                    ++whiteSpaceIter; // skip over disk

                    // add the disk and its portal info
                    addIScsiDisk(*whiteSpaceIter, iScsiTargetPortal);
                }
            }
            ++newLineIter;
        } while (newLineIter != newLineIterEnd);
    }

    return RESULT_SUCCESS;
}

bool StorageFailoverManager::parseSqlResults(std::string const& results)
{
    // at this point results will either have
    // * some type of error which will have the word ERROR in the output
    // * all matching pairs for all cluster nodes with the following fields
    // (tab separated) ordered by sourceHostId.
    //
    // sourceHostId\tdestinationDeviceName\tdeviceName\tdeviceId\tclusterGroupName\tclusterResourceName

    std::string primaryHostId;
    std::string sourceDeviceName;
    std::string targetDeviceName;
    std::string sourceGroupName;
    std::string sourceResourceName;
    std::string sourceDeviceId;

    bool checkError = true;

    boost::char_separator<char> newlineSep("\n");
    tokenizer_t lines(results, newlineSep);
    tokenizer_t::iterator newLineIter(lines.begin());
    tokenizer_t::iterator newLineIterEnd(lines.end());

    for (/* empty */; newLineIter != newLineIterEnd; ++newLineIter) {
        boost::char_separator<char> tabSep("\t");
        tokenizer_t tokens(*newLineIter, tabSep);
        tokenizer_t::iterator tabIter(tokens.begin());
        tokenizer_t::iterator tabIterEnd(tokens.end());

        // the first sourceHostId returned will be the
        // primary cluster node that will drive the
        // storage failover. 
        if (primaryHostId.empty()) {
            primaryHostId = *tabIter;

            // only need to check for sql error first time through loop
            if (checkError && std::string::npos != primaryHostId.find("ERROR")) {
                return false;
            }
        }

        // now get the entires for the host that is making the request
        if (m_sourceHostId != (*tabIter)) {
            continue;
        }

        ++tabIter;

        targetDeviceName = *tabIter++;

        checkError = false;

        sourceDeviceName = *tabIter++;
        sourceDeviceId = *tabIter++;
        sourceGroupName = *tabIter++;
        sourceResourceName = *tabIter++;

        std::string diskId;
        unsigned long long offset = 0;
        if (!sourceDeviceName.empty() && !targetDeviceName.empty()) {
            getDiskIdAndOffset(targetDeviceName, diskId, offset);
            m_pairsByTargetDevice.insert(std::make_pair(targetDeviceName,
                                                        IScsiTargetPairInfo(sourceDeviceName,
                                                                            sourceDeviceId,
                                                                            sourceGroupName,
                                                                            sourceResourceName,
                                                                            diskId,
                                                                            offset)));
        }

        sourceDeviceName.clear();
        targetDeviceName.clear();
        sourceGroupName.clear();
        sourceResourceName.clear();
        sourceDeviceId.clear();
    }

    m_primaryNode = (primaryHostId == m_sourceHostId);
    return true;
}

int StorageFailoverManager::getPairs()
{
    bool firstTime = true;

    std::string cmd("mysql --user=root --password= --batch -N --execute=\""
                    "select distinct "
                    "  sd2.sourceHostId, sd2.destinationDeviceName, "
                    "  sd2.sourceDeviceName, c.deviceId, c.clusterGroupName, "
                    "  c.clusterResourceName "
                    "from "
                    "  srcLogicalVolumeDestLogicalVolume sd1, "
                    "  srcLogicalVolumeDestLogicalVolume sd2 "
                    "  left join  clusters c "
                    "  on (sd2.sourceHostId = c.hostId and sd2.sourceDeviceName = c.deviceName) "
                    "where "
                    "  sd1.sourceHostId = '");
    cmd += m_sourceHostId;
    cmd += "'"
        "  and sd1.destinationHostId = sd2.destinationHostId "
        "order by  "
        "  sd2.sourceHostId, sd2.sourceDeviceName \" svsdb1 2>&1";

    std::stringstream results;

    if (!executePipe(cmd, results)) {
        std::cout << results.str() << '\n';
        return RESULT_ERROR;
    }

    if (results.str().empty()) {
        std::cout << "no pairs found for host id " << m_sourceHostId << '\n';
        return RESULT_NOT_FOUND;
    }

    if (!parseSqlResults(results.str())) {
        std::cout << results.str() << '\n';
        return RESULT_ERROR;
    }

    return RESULT_SUCCESS;
}

bool StorageFailoverManager::rollbackPairs()
{

    std::string rollBackPairs("--rollbackpairs=");

    std::string::size_type idx;

    pairsByTargetDevice_t::iterator iter(m_pairsByTargetDevice.begin());
    pairsByTargetDevice_t::iterator iterEnd(m_pairsByTargetDevice.end());
    for (/* empty */; iter != iterEnd; ++iter) {
        rollBackPairs += (*iter).first;
        rollBackPairs += ';';
    }

    char const* cdpcliArgs[6];

    cdpcliArgs[0] = "cdpcli";
    cdpcliArgs[1] = "--rollback";
    cdpcliArgs[2] = "--recentcrashconsistentpoint";
    cdpcliArgs[3] = "--deleteretentionlog=yes"; // MAYBE: do not delete retention logs
    cdpcliArgs[4] = rollBackPairs.c_str();
    cdpcliArgs[5] = "--force=yes";

    int argCount = 6;

    CDPCli cli(argCount, (char**)cdpcliArgs);

    return cli.run();
}

void StorageFailoverManager::logoutAllTargets()
{
    // TODO: should make sure there are not mount points
    // associated with any of the luns for these targets
    // it can cause reboot issues.
    //
    // at the moment none of these should be mounted as
    // this is linux but the luns will be NTFS
    
    std::string cmd;

    std::stringstream results;

    iScsiTargets_t::iterator iter(m_iScsiTargets.begin());
    iScsiTargets_t::iterator iterEnd(m_iScsiTargets.end());
    for (/* empty */; iter != iterEnd; ++iter) {
        // logout target
        cmd = "/sbin/iscsiadm -m node -u -T ";
        cmd += (*iter);
        executePipe(cmd, results);

        // delete target
        cmd = "/sbin/iscsiadm -m node -o delete -T ";
        cmd += (*iter);
        executePipe(cmd, results);
    }
}

std::string StorageFailoverManager::getDiskId(std::string const& diskName)
{    
    std::string cmd("/sbin/scsi_id -p 0x83 -g -s ");
    cmd += diskName;   
                    
    std::stringstream results;

    if (!executePipe(cmd, results)) {
        std::cout << results.str() << '\n';
        return std::string();
    }

    std::string diskId;

    results >> diskId;
    
    return diskId;
}

void StorageFailoverManager::reportMappings()
{   
    std::string ip;
    std::string port;

    if (!m_pairsByTargetDevice.empty()) {
        if (m_primaryNode) {
            std::cout << "role:\tPRIMARY\n";
        } else {
            std::cout << "role:\tSECONDARY\n";
        }
    }

    if (m_failOver) {
        // only need to report status and role in fail over case
        return;
    }

    
    iScsiDisks_t::iterator iScsiDiskIterEnd(m_iScsiDisks.end());

    pairsByTargetDevice_t::iterator iter(m_pairsByTargetDevice.begin());
    pairsByTargetDevice_t::iterator iterEnd(m_pairsByTargetDevice.end());
    for (/* empty */; iter != iterEnd; ++iter) {
        // report this pair if it is an iSCSI disk or doing a migration
        iScsiDisks_t::iterator iScsiDiskFindIter(m_iScsiDisks.find((*iter).second.m_destinationDiskId));
        if (iScsiDiskIterEnd != iScsiDiskFindIter || m_migration) {
            std::cout << "disk:\t"
                      << std::hex << (*iter).second.m_destinationDiskId << std::dec << '\t'
                      << std::hex << std::setw(16) << std::setfill('0') << (*iter).second.m_destinationOffset << std::dec << '\t'
                      << (*iter).second.m_sourceDeviceName << '\t'
                      << (*iter).second.m_sourceDeviceId << '\t'
                      << (*iter).second.m_sourceGroupName << '\t'
                      << (*iter).second.m_sourceResourceName << '\t';
            
            if (!m_migration) {
                iScsiTargetPortals_t::iterator tpIter((*iScsiDiskFindIter).second.m_targetPortals.begin());
                iScsiTargetPortals_t::iterator tpIterEnd((*iScsiDiskFindIter).second.m_targetPortals.end());
                for (/* empty **/; tpIter != tpIterEnd; ++ tpIter) {
                    m_iScsiTargets.insert((*tpIter).second.m_targetName);
                    std::cout << (*tpIter).second.m_targetName << '\t'
                              << (*tpIter).second.m_portal.m_ipAddress << '\t'
                              << (*tpIter).second.m_portal.m_ipPortNumber << '\t';
                }
            }
            std::cout << '\n';
        }
    }
}

int StorageFailoverManager::stopAgent()
{
    ProcKill::stopProc("svagents", SIGKILL, true, 1, true);
}

int StorageFailoverManager::startAgent()
{
    std::string cmd("/etc/init.d/vxagent start 2>&1");

    std::stringstream results;

    if (!executePipe(cmd, results)) {
        std::cout << results.str() << '\n';
        return RESULT_ERROR;
    }
}

bool StorageFailoverManager::validateIscsiadm()
{
    struct stat statInfo;

    return (0 == stat("/sbin/iscsiadm", &statInfo));
}


int StorageFailoverManager::getIScsiInitiatorNodeName(std::string& nodeName)
{
    std::ifstream initiatorNameFile("/etc/iscsi/initiatorname.iscsi");
    if (!initiatorNameFile) {
        return errno;
    }

    std::string tag;

    std::getline(initiatorNameFile, tag, '=');
    std::getline(initiatorNameFile, nodeName, '\n');

    return 0;
}

int StorageFailoverManager::doFailover()
{
    int rc;

    if (!validateIscsiadm()) {
        std::cout << "iscsiadm not found\n";
        return RESULT_NOT_FOUND;
    }

    rc = getDmInfo();
    if (RESULT_SUCCESS != rc) {
        return rc;
    }

    if (!m_migration) {
        rc = getIScsiDisks();
        if (RESULT_SUCCESS != rc) {
            return rc;
        }
    }

    rc = getPairs();    
    if (RESULT_SUCCESS != rc) {
        return rc;
    }

    if (m_primaryNode && m_failOver) {
        rc = stopAgent();
        if (RESULT_SUCCESS != rc) {
            return rc;
        }
        if (!rollbackPairs()) {
            return RESULT_ERROR;
        }
    }

    reportMappings();

    if (m_primaryNode && m_failOver) {
        if (!m_migration) {
            logoutAllTargets();
        }
        startAgent();
    }
    
    return RESULT_SUCCESS;
}
