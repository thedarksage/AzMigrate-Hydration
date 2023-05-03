
///
/// \file diskpartwrapper.cpp
///
/// \brief
///

#include <windows.h>
#include <string>

#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "diskpartwrapper.h"
#include "fiopipe.h"

namespace DISKPART {

    std::string const diskpartExit("exit\n");
    
#ifdef _WIN32
    struct Wow64Disabler {
        typedef BOOL (WINAPI *Wow64DisableWow64FsRedirection_t) (PVOID*);
        typedef BOOL (WINAPI *Wow64RevertWow64FsRedirection_t) (PVOID);
        Wow64Disabler()
            : m_wow64DisableWow64FsRedirection((Wow64DisableWow64FsRedirection_t)0),
              m_wow64RevertWow64FsRedirection((Wow64RevertWow64FsRedirection_t)0),
              m_dismissed(true),
              m_oldValue((void*)0)
            {
                HMODULE kernel32Dll = GetModuleHandle(TEXT("kernel32"));
                if (0 != kernel32Dll) {
                    m_wow64DisableWow64FsRedirection = (Wow64DisableWow64FsRedirection_t)GetProcAddress(kernel32Dll, "Wow64DisableWow64FsRedirection");
                    m_wow64RevertWow64FsRedirection = (Wow64RevertWow64FsRedirection_t)GetProcAddress(kernel32Dll, "Wow64RevertWow64FsRedirection");
                    if (0 != m_wow64DisableWow64FsRedirection && 0 != m_wow64RevertWow64FsRedirection) {
                        if (m_wow64DisableWow64FsRedirection(&m_oldValue)) {
                            m_dismissed = false;
                        } else {
                            m_oldValue = (void*)0;
                            m_dismissed = true;
                        }
                    }
                }
            }
        ~Wow64Disabler()
            {
                if (!m_dismissed) {
                    dismiss();
                }
            }
        void dismiss()
            {
                if (0 != m_wow64RevertWow64FsRedirection && !m_dismissed) {
                    if (m_wow64RevertWow64FsRedirection(&m_oldValue)) {
                        m_oldValue = (void*)0;
                        m_dismissed = true;
                    }
                }
            }

        Wow64DisableWow64FsRedirection_t m_wow64DisableWow64FsRedirection;
        Wow64RevertWow64FsRedirection_t m_wow64RevertWow64FsRedirection;

        bool m_dismissed;
        void* m_oldValue;
    };
#else
    struct Wow64Disabler {
        Wow64Disabler()
            {
            }
        ~Wow64Disabler()
            {
            }
        void dismiss()
            {
            }
    };
#endif

    char const* DISK_STATUS_ANY = "ANY";
    char const* DISK_STATUS_ONLINE = "Online";
    char const* DISK_STATUS_OFFLINE = "Offline";
    char const* DISK_STATUS_MISSING = "Missing";
    char const* DISK_STATUS_FOREIGN = "Foreign";

    diskPartDiskNumbers_t DiskPartWrapper::getDisks(char const* status, DISK_TYPE type, DISK_FORMAT_TYPE formatType)
    {
        m_errMsg.clear();
        diskPartDiskNumbers_t disks;
        std::string response;
        if (!executeCmd("list disk", response)) {
            return disks;
        }
        parseGetDiskResponse(response, disks, status, type, formatType);
        return disks;
    }

    bool DiskPartWrapper::onlineDisks(diskPartDiskNumbers_t const& disks)
    {
        m_errMsg.clear();
        size_t count = 0;
        diskPartDiskNumbers_t::const_iterator iter;
        diskPartDiskNumbers_t::const_iterator iterEnd;
        diskPartDiskNumbers_t allMatchingDisks;
        if (disks.empty()) {
            allMatchingDisks = getDisks(DISK_STATUS_OFFLINE);
            if (allMatchingDisks.empty()) {
                return m_errMsg.empty();
            }
            count = allMatchingDisks.size();
            iter = allMatchingDisks.begin();
            iterEnd = allMatchingDisks.end();
        } else {
            iter = disks.begin();
            iterEnd = disks.end();
            count = disks.size();
        }
        std::string cmd;
        for (/* empty */; iter != iterEnd; ++iter) {
            cmd += "select disk ";
            cmd += *iter;
            cmd += "\n";
            if (m_supportsAllRequests) {
                cmd += "attributes disk clear readonly noerr\n";
            }
            cmd += "online ";
            if (m_supportsAllRequests) {
                cmd += "disk ";
            }
            cmd += "noerr\n";
        }
        std::string response;
        if (!executeCmd(cmd, response)) {
            return false;
        }
        successText_t checkForText;
        checkForText.push_back(" successfully onlined");
        checkForText.push_back(" already online");
        return validateResponse(response, count, checkForText);
    }

    bool DiskPartWrapper::offlineDisks(diskPartDiskNumbers_t const& disks)
    {
        m_errMsg.clear();
        size_t count = 0;
        if (!m_supportsAllRequests) {
            m_errMsg = "offline disk not supported";
            return false;
        }
        diskPartDiskNumbers_t::const_iterator iter;
        diskPartDiskNumbers_t::const_iterator iterEnd;
        diskPartDiskNumbers_t allMatchingDisks;
        if (disks.empty()) {
            allMatchingDisks = getDisks(DISK_STATUS_ONLINE);
            if (allMatchingDisks.empty()) {
                return m_errMsg.empty();
            }
            count = allMatchingDisks.size();
            iter = allMatchingDisks.begin();
            iterEnd = allMatchingDisks.end();
        } else {
            count = disks.size();
            iter = disks.begin();
            iterEnd = disks.end();
        }

        std::string cmd;
        for (/* empty */; iter != iterEnd; ++iter) {
            cmd += "select disk ";
            cmd += *iter;
            cmd += "\n";
            cmd += "offline disk noerr\n";
        }
        std::string response;
        if (!executeCmd(cmd, response)) {
            return false;
        }
        successText_t checkForText;
        checkForText.push_back(" successfully offlined");
        checkForText.push_back(" already offline");
        return validateResponse(response, count, checkForText);
    }

    //Convert disks to MBR\GPT, and Basic\Dynamic, disk has to be empty(should not have any volumes)
    bool DiskPartWrapper::convertDisks(diskPartDiskNumbers_t const& disks, DISK_TYPE type, DISK_FORMAT_TYPE formattype)
    {
        m_errMsg.clear();
        size_t count = 0;
        if (!m_supportsAllRequests) {
            m_errMsg = "offline disk not supported";
            return false;
        }
        diskPartDiskNumbers_t::const_iterator iter;
        diskPartDiskNumbers_t::const_iterator iterEnd;
        if (disks.empty()) {
            m_errMsg = "empty disk list found";
            return false;
        } else {
            count = disks.size();
            iter = disks.begin();
            iterEnd = disks.end();
        }

        std::string cmd;
        for (/* empty */; iter != iterEnd; ++iter) {
            cmd += "select disk ";
            cmd += *iter;
            cmd += "\n";
            if(DISK_TYPE_DYNAMIC == type) {
                if(DISK_FORMAT_TYPE_MBR == formattype)
                    cmd += "convert mbr\n";
                if(DISK_FORMAT_TYPE_GPT == formattype)
                    cmd += "convert gpt\n";
                cmd += "convert dynamic\n";
            }
            else if(DISK_TYPE_BASIC == type) {
                cmd += "convert basic\n";
                if(DISK_FORMAT_TYPE_MBR == formattype)
                    cmd += "convert mbr\n";
                if(DISK_FORMAT_TYPE_GPT == formattype)
                    cmd += "convert gpt\n";
            }
            else if( (DISK_TYPE_ANY == type) && (DISK_FORMAT_TYPE_ANY == formattype) ) {
                //Don't do anything
            }
            else {
                if(DISK_TYPE_ANY == type) {
                    if(DISK_FORMAT_TYPE_MBR == formattype)
                        cmd += "convert mbr\n";
                    if(DISK_FORMAT_TYPE_GPT == formattype)
                        cmd += "convert gpt\n";
                } else if (DISK_FORMAT_TYPE_ANY == formattype) {
                    if(DISK_TYPE_BASIC == type)
                        cmd += "convert basic\n";
                    if(DISK_TYPE_DYNAMIC == type)
                        cmd += "convert dynamic\n";
                }
            }
        }
        std::string response;
        if (!executeCmd(cmd, response)) {
            return false;
        }
        successText_t checkForText;
        checkForText.push_back(" successfully converted");
        return validateResponse(response, count, checkForText);
    }

    bool DiskPartWrapper::deleteMissingDisks(diskPartDiskNumbers_t const& disks)
    {
        m_errMsg.clear();
        size_t count = 0;
        diskPartDiskNumbers_t::const_iterator iter;
        diskPartDiskNumbers_t::const_iterator iterEnd;
        diskPartDiskNumbers_t allMatchingDisks;
        if (disks.empty()) {
            allMatchingDisks = getDisks(DISK_STATUS_MISSING, DISK_TYPE_DYNAMIC);
            if (allMatchingDisks.empty()) {
                return m_errMsg.empty();
            }
            count = allMatchingDisks.size();
            iter = allMatchingDisks.begin();
            iterEnd = allMatchingDisks.end();
        } else {
            count = disks.size();
            iter = disks.begin();
            iterEnd = disks.end();
        }
        std::string cmd;
        for (/* empty */; iter != iterEnd; ++iter) {
            cmd += "select disk ";
            cmd += *iter;
            cmd += "\n";
            cmd += "delete disk override\n";
        }
        std::string response;
        if (!executeCmd(cmd, response)) {
            return false;
        }
        successText_t checkForText;
        checkForText.push_back(" successfully");
        return validateResponse(response, count, checkForText);
    }

    bool DiskPartWrapper::importForeignDisks(diskPartDiskNumbers_t const& disks)
    {
        m_errMsg.clear();
        diskPartDiskNumbers_t::const_iterator iter;
        diskPartDiskNumbers_t::const_iterator iterEnd;
        diskPartDiskNumbers_t allMatchingDisks;
        if (disks.empty()) {
            allMatchingDisks = getDisks(DISK_STATUS_FOREIGN, DISK_TYPE_DYNAMIC);
            if (allMatchingDisks.empty()) {
                return m_errMsg.empty();
            }
            iter = allMatchingDisks.begin();
            iterEnd = allMatchingDisks.end();
        } else {
            iter = disks.begin();
            iterEnd = disks.end();
        }
        for (/* empty */; iter != iterEnd; ++iter) {
            // operates on dynamic disk pack (group) . DiskPartWrapper
            // currently only supports a single dynamic group so only
            // 1 disk needs to be selected but it is possible that a
            // disk is in state that does not succeed so try one at a
            // time until one succeeds or all fail
            std::string cmd;
            cmd += "select disk ";
            cmd += *iter;
            cmd += "\n";
            cmd += "import noerr\n";
            std::string response;
            if (!executeCmd(cmd, response)) {
                return false;
            }
            successText_t checkForText;
            checkForText.push_back(" successfully");
            if (validateResponse(response, 1, checkForText)) {
                return true;
            }
        }
        return false;
    }

    bool DiskPartWrapper::recoverDisks(diskPartDiskNumbers_t const& disks)
    {
        m_errMsg.clear();
        if (!m_supportsAllRequests) {
            m_errMsg = "recover not suppported";
            return false;
        }
        diskPartDiskNumbers_t::const_iterator iter;
        diskPartDiskNumbers_t::const_iterator iterEnd;
        diskPartDiskNumbers_t allMatchingDisks;
        if (disks.empty()) {
            allMatchingDisks = getDisks(DISK_STATUS_ANY, DISK_TYPE_DYNAMIC);
            if (allMatchingDisks.empty()) {
                return m_errMsg.empty();
            }
            iter = allMatchingDisks.begin();
            iterEnd = allMatchingDisks.end();
        } else {
            iter = disks.begin();
            iterEnd = disks.end();
        }
        for (/* empty */; iter != iterEnd; ++iter) {
            // operates on dynamic disk pack (group) . DiskPartWrapper
            // currently only supports a single dynamic group so only
            // 1 disk needs to be selected but it is possible that a
            // disk is in state that does not succeed so try one at a
            // time until one succeeds or all fail
            std::string cmd;
            cmd += "select disk ";
            cmd += *iter;
            cmd += "\n";
            cmd += "recover noerr\n";
            std::string response;
            if (!executeCmd(cmd, response)) {
                return false;
            }
            successText_t checkForText;
            checkForText.push_back(" successfully");
            if (validateResponse(response, 1, checkForText)) {
                return true;
            }
        }
        return false;
    }

    bool DiskPartWrapper::cleanDisks(diskPartDiskNumbers_t const& disks, char const* status, DISK_TYPE type, DISK_FORMAT_TYPE formatType)
    {
        m_errMsg.clear();
        size_t count = 0;
        diskPartDiskNumbers_t::const_iterator iter;
        diskPartDiskNumbers_t::const_iterator iterEnd;
        diskPartDiskNumbers_t allMatchingDisks;
        if (disks.empty()) {
            allMatchingDisks = getDisks(DISK_STATUS_ONLINE);
            if (allMatchingDisks.empty()) {
                return m_errMsg.empty();
            }
            count = allMatchingDisks.size();
            iter = allMatchingDisks.begin();
            iterEnd = allMatchingDisks.end();
        } else {
            count = disks.size();
            iter = disks.begin();
            iterEnd = disks.end();
        }

        std::string cmd;
        for (/* empty */; iter != iterEnd; ++iter) {
            cmd += "select disk ";
            cmd += *iter;
            cmd += "\n";
            cmd += "clean\n";
        }
        std::string response;
        if (!executeCmd(cmd, response)) {
            return false;
        }
        successText_t checkForText;
        checkForText.push_back(" succeeded in cleaning");
        return validateResponse(response, count, checkForText);
    }

    bool DiskPartWrapper::clearDisksAttribute(diskPartDiskNumbers_t const& disks, DISK_ATTRIBUTE attribute)
    {
        m_errMsg.clear();
        size_t count = 0;
        diskPartDiskNumbers_t::const_iterator iter;
        diskPartDiskNumbers_t::const_iterator iterEnd;
        diskPartDiskNumbers_t allMatchingDisks;
        if (disks.empty()) {
            allMatchingDisks = getDisks(DISK_STATUS_ONLINE);
            if (allMatchingDisks.empty()) {
                return m_errMsg.empty();
            }
            count = allMatchingDisks.size();
            iter = allMatchingDisks.begin();
            iterEnd = allMatchingDisks.end();
        } else {
            count = disks.size();
            iter = disks.begin();
            iterEnd = disks.end();
        }

        std::string diskattr;

        if(DISK_READ_ONLY == attribute)
            diskattr = "readonly";

        std::string cmd;
        for (/* empty */; iter != iterEnd; ++iter) {
            cmd += "select disk ";
            cmd += *iter;
            cmd += "\n";
            cmd += "attribute disk clear ";
            cmd += diskattr;
            cmd += " noerr\n";
        }
        std::string response;
        if (!executeCmd(cmd, response)) {
            return false;
        }
        successText_t checkForText;
        checkForText.push_back(" cleared successfully");
        return validateResponse(response, count, checkForText);
    }

    bool DiskPartWrapper::setPartitionId(diskPartDiskNumberAndPartitions_t const& disks, char const* id)
    {
        m_errMsg.clear();
        size_t count = 0;
        if (disks.empty()) {
            return true;
        }
        std::string cmd;
        count = disks.size();
        diskPartDiskNumberAndPartitions_t::const_iterator iter(disks.begin());
        diskPartDiskNumberAndPartitions_t::const_iterator iterEnd(disks.end());;
        for (/* empty */; iter != iterEnd; ++iter) {
            cmd += "select disk ";
            cmd += (*iter).m_diskNumber;
            cmd += "\n";
            partitionNumbers_t::const_iterator partitionIter((*iter).m_partitionNumbers.begin());
            partitionNumbers_t::const_iterator partitionIterEnd((*iter).m_partitionNumbers.end());
            for (/* empty */; partitionIter != partitionIterEnd; ++partitionIter) {
                cmd += "select partition ";
                cmd += boost::lexical_cast<std::string>(*partitionIter);;
                cmd += "\nset id=";
                cmd += id;
                cmd += " noerr\n";
            }
        }
        std::string response;
        if (!executeCmd(cmd, response)) {
            return false;
        }
        successText_t checkForText;
        checkForText.push_back(" successfully set");
        return validateResponse(response, count, checkForText);
    }

    bool DiskPartWrapper::rescanDisks()
    {
        m_errMsg.clear();
        std::string response;
        if (!executeCmd("rescan", response)) {
            return false;
        }
        successText_t checkForText;
        checkForText.push_back(" finished scanning");
        return validateResponse(response, 1, checkForText);
    }

    bool DiskPartWrapper::automount(bool enable)
    {
        m_errMsg.clear();
        successText_t checkForText;
        std::string response;
        std::string cmd("automount ");
        if (enable) {
            cmd += "enable";
            checkForText.push_back(" enabled");
        } else {
            cmd += "disable";
            checkForText.push_back(" disabled");
        }
        cmd += " noerr";
        if (!executeCmd(cmd, response)) {
            return false;
        }
        return validateResponse(response, 1, checkForText);
    }

    bool DiskPartWrapper::onlineVolumes()
    {
        m_errMsg.clear();
        std::string response;
        if (!executeCmd("list volume", response)) {
            return false;
        }
        size_t count = 0;
        std::string cmd;
        typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;
        boost::char_separator<char> sep("\n");
        tokenizer_t lines(response, sep);
        tokenizer_t::iterator lineIter(lines.begin());
        tokenizer_t::iterator lineIterEnd(lines.end());        
        for (/* empty*/; lineIter != lineIterEnd; ++lineIter) {
            if (boost::algorithm::icontains(*lineIter, "Volume")
                && boost::algorithm::icontains(*lineIter, "Offline")) {
                boost::char_separator<char> sep2(" \t\n");
                tokenizer_t tokens(*lineIter, sep2);
                tokenizer_t::iterator tokenIter(tokens.begin());
                tokenizer_t::iterator tokenIterEnd(tokens.end());
                if (tokenIter != tokens.end()) {
                    if (boost::algorithm::iequals("Volume", *tokenIter)) {
                        ++tokenIter; // at volume number
                        if (tokenIterEnd != tokenIter) {
                            cmd += "select volume ";
                            cmd += *tokenIter;
                            cmd += "\nonline volume noerr\n";
                            ++count;
                        }
                    }
                }
            }
        }
        if (!cmd.empty()) {
            if (!executeCmd(cmd, response)) {
                return false;
            }
        }
        successText_t checkForText;
        checkForText.push_back(" successfully");
        return validateResponse(response, count, checkForText);
    }

    bool DiskPartWrapper::removeCdDvdDriveLetterMountPoint()
    {
        m_errMsg.clear();
        std::string response;
        if (!executeCmd("list volume", response)) {
            return false;
        }
        size_t count = 0;
        std::string cmd;
        typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;
        boost::char_separator<char> sep("\n");
        tokenizer_t lines(response, sep);
        tokenizer_t::iterator lineIter(lines.begin());
        tokenizer_t::iterator lineIterEnd(lines.end());
        for (/* empty*/; lineIter != lineIterEnd; ++lineIter) {
            if (boost::algorithm::icontains(*lineIter, "Volume")
                && boost::algorithm::icontains(*lineIter, "DVD_ROM")) {
                boost::char_separator<char> sep2(" \t\n");
                tokenizer_t tokens(*lineIter, sep2);
                tokenizer_t::iterator tokenIter(tokens.begin());
                tokenizer_t::iterator tokenIterEnd(tokens.end());
                if (tokenIter != tokens.end()) {
                    if (boost::algorithm::iequals("Volume", *tokenIter)) {
                        ++tokenIter; // at volume number
                        if (tokenIterEnd != tokenIter) {
                            cmd += "select volume ";
                            cmd += *tokenIter;
                            cmd += "\nremove all noerr\n";
                            ++count;
                        }
                    }
                }
            }
        }
        if (!cmd.empty()) {
            if (!executeCmd(cmd, response)) {
                return false;
            }
        }
        successText_t checkForText;
        checkForText.push_back(" successfully");
        return validateResponse(response, count, checkForText);
    }

    bool DiskPartWrapper::supportsAllRequests()
    {
        m_errMsg.clear();
        // for now just need to check if offline is supported
        // if so then everything DispartWrapper suppports is supported
        std::string response;
        if (!executeCmd("help offline", response)) {
            return false;
        }
        successText_t checkForText;
        checkForText.push_back("offline a");
        return validateResponse(response, 1, checkForText);
    }

    bool DiskPartWrapper::validateResponse(std::string const& response, size_t count, successText_t const& checkForText)
    {
        // first see if a simple check is good enough
        if (1 == checkForText.size()) {
            if (1 == count) {
                if (!boost::algorithm::icontains(response, *(checkForText.begin()))) {
                    m_errMsg = response;
                    return false;
                }
                return true;
            }
        }
        size_t foundCount = 0;
        successText_t::const_iterator iter(checkForText.begin());
        successText_t::const_iterator iterEnd(checkForText.end());
        for (/* empty */; iter != iterEnd; ++iter) {
            std::vector<std::string> found;
            boost::algorithm::ifind_all(found, response, *iter);
            foundCount += found.size();
        }
        if (foundCount != count) {
            m_errMsg = response;
            return false;
        }
        return true;
    }

    bool DiskPartWrapper::parseGetDiskResponse(std::string const& response,
                                               diskPartDiskNumbers_t& disks,
                                               std::string status,
                                               DISK_TYPE type,
                                               DISK_FORMAT_TYPE formatType)
    {
        bool ok = false;
        disks.clear();
        // for get disk requests the reponse should look something like this
        // NOTE the first 3 lines are shown as ignored line as the actual
        // text output by diskpart causes issues with Fossology
        // 
        // ignored line 1
        // ignored line 2
        // ignored line 3
        //
        //   Disk ###  Status      Size     Free     Dyn  Gpt
        //   --------  ----------  -------  -------  ---  ---
        //   Disk 0    Online       186 GB      0 B
        //   Disk 1    Online       149 GB  8033 KB   *
        //   Disk 2    Online       149 GB  8033 KB        *
        //   Disk 3    Online       149 GB  8033 KB   *    *
        //
        // Leaving DiskPart...
        //
        // want just the list of disks based on the status and type
        std::string::size_type dynIdx = std::string::npos;
        typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;
        boost::char_separator<char> sep("\n");
        tokenizer_t lines(response, sep);
        tokenizer_t::iterator lineIter(lines.begin());
        tokenizer_t::iterator lineIterEnd(lines.end());
        for (/* empty*/; lineIter != lineIterEnd; ++lineIter) {
            boost::char_separator<char> sep2(" \t\n");
            tokenizer_t tokens(*lineIter, sep2);
            tokenizer_t::iterator tokenIter(tokens.begin());
            tokenizer_t::iterator tokenIterEnd(tokens.end());
            if (tokenIter != tokens.end()) {
                if (!boost::algorithm::iequals("Disk", *tokenIter)) {
                    continue;
                }
                ++tokenIter; // at disk number
                if (tokenIterEnd == tokenIter) {
                    continue;
                }
                std::string disk;
                try {
                    disk = *tokenIter;
                } catch (...) {
                }
                ++tokenIter; // at status
                if (tokenIterEnd == tokenIter) {
                    continue;
                }
                std::string diskStatus(*tokenIter);
                if (boost::algorithm::iequals("Status", diskStatus)) {
                    // need to get the idx of Dyn for use later
                    // first get the actual dyn string that was used
                    for (/* empty */; tokenIter != tokenIterEnd; ++tokenIter) {
                        if (boost::algorithm::iequals("dyn", *tokenIter)) {
                            dynIdx = (*lineIter).find(*tokenIter);
                            break;
                        }
                    }
                    if (std::string::npos == dynIdx) {
                        m_errMsg = "unrecognized format returned by diskpart";
                        return false;
                    }
                    continue; // ok move on to next line
                }
                if (std::string::npos == dynIdx) {
                    continue; // ok move on to next line
                }
                ok = true; // if we get this far then have a least 1 valid disk line
                // now need to see if either dyn or gpt have a '*'
                // cannot use tokenIter as it can not distiguish between
                // Dyn   Gpt
                // ---   ---
                //  *
                //        *
                //
                // both will look like dyn has the '*'. So instead need to see if they
                // exist and the positon they are in.
                bool dyn = false;
                bool gpt = false;
                std::string::size_type idxFirst((*lineIter).find_first_of("*"));
                if (std::string::npos != idxFirst) {
                    if (idxFirst >= dynIdx && idxFirst <= dynIdx + 3) {
                        dyn = true;
                        std::string::size_type idxLast((*lineIter).find_last_of("*"));
                        if (std::string::npos != idxLast) {
                            if (idxFirst != idxLast) {
                                gpt = true;
                            }
                        }
                    } else {
                        gpt = true;
                    }
                }
                if (!disk.empty()
                    && (boost::algorithm::iequals(DISK_STATUS_ANY, status) || boost::algorithm::iequals(diskStatus, status))
                    && (DISK_TYPE_ANY == type || (dyn && (DISK_TYPE_DYNAMIC == type)) || (!dyn && (DISK_TYPE_BASIC == type)))
                    && (DISK_FORMAT_TYPE_ANY == formatType || (gpt && (DISK_FORMAT_TYPE_GPT == formatType)) || (!gpt && (DISK_FORMAT_TYPE_MBR == formatType)))) {
                    disks.insert(disk);
                }
            }
        }
        if (!ok) {
            m_errMsg = "unrecognized format returned by diskpart";
        }
        return ok;
    }

    bool DiskPartWrapper::executeCmd(std::string const& cmd, std::string& response)
    {
        try {
            response.clear();
            m_errMsg.clear();
            FIO::cmdArgs_t vargs;
            vargs.push_back("diskpart");
            vargs.push_back((char*)0);
            Wow64Disabler wow64Disabler;
            FIO::FioPipe fioPipe(vargs, true);
            wow64Disabler.dismiss();
            std::string request(cmd + "\n");//exit\n");
            int bytes = fioPipe.write((char*)request.c_str(), (long)request.size());
            if (0 == bytes) {
                m_errMsg = fioPipe.errorAsString();
                return false;
            }
            char buffer[1024];
            bytes = fioPipe.read(buffer, sizeof(buffer));
            if (bytes <= 0) {
                m_errMsg = fioPipe.errorAsString();
                return false;
            }
            response.append(buffer, bytes);
            do {
                while ((bytes = fioPipe.read(buffer, sizeof(buffer), false)) > 0) {
                    response.append(buffer, bytes);
                }
                if (!(FIO::FIO_SUCCESS == fioPipe.error() || FIO::FIO_WOULD_BLOCK == fioPipe.error() || FIO::FIO_EOF == fioPipe.error())) {
                    m_errMsg = fioPipe.errorAsString();
                    return false;
                }
                Sleep(0); // avoids tight loop that delays reading diskpart output
                if (FIO::FIO_WOULD_BLOCK == fioPipe.error()) {
                    // tell diskpart to exit
                    fioPipe.write(diskpartExit.c_str(), diskpartExit.size());
                }
                // success or eof indicates either diskpart exited or closed pipe being read
                // either way nothing more can be read so just exit loop and parsing
                // results will determine if there as was an error or not
            } while (!(FIO::FIO_EOF == fioPipe.error() || FIO::FIO_SUCCESS == fioPipe.error()));
            while ((bytes = fioPipe.read(buffer, sizeof(buffer), false)) > 0) {
                response.append(buffer, bytes);
            }
            return true;
        } catch (ErrorException const& e) {
            m_errMsg = e.what();
        }
        return false;
    }

} //namespace DISKPART
