
///
/// \file diskpartwrapper.h
///
/// \brief
///

#ifndef DISKPARTWRAPPER_H
#define DISKPARTWRAPPER_H


#include <string>
#include <set>
#include <list>

namespace DISKPART {    
    typedef std::set<std::string> diskPartDiskNumbers_t;

    typedef std::set<unsigned int> partitionNumbers_t;

    struct DiskNumberAndPartitions {
        std::string m_diskNumber;
        partitionNumbers_t m_partitionNumbers;
    };

    typedef std::list<DiskNumberAndPartitions> diskPartDiskNumberAndPartitions_t;

    enum DISK_TYPE {
        DISK_TYPE_ANY,
        DISK_TYPE_BASIC,
        DISK_TYPE_DYNAMIC
    };

    enum DISK_ATTRIBUTE{
        DISK_READ_ONLY,
        DISK_BOOT,
        DISK_PAGEFILE,
        DISK_CLUSTER,
        DISK_CRASH_DUMP
    };

    enum DISK_FORMAT_TYPE {
        DISK_FORMAT_TYPE_ANY,
        DISK_FORMAT_TYPE_MBR,
        DISK_FORMAT_TYPE_GPT
    };

    extern char const* DISK_STATUS_ANY;
    extern char const* DISK_STATUS_ONLINE;
    extern char const* DISK_STATUS_OFFLINE;
    extern char const* DISK_STATUS_MISSING;
    extern char const* DISK_STATUS_FOREIGN;

    class DiskPartWrapper {
    public:
        DiskPartWrapper()
            {
                m_supportsAllRequests = supportsAllRequests();
            }

        ~DiskPartWrapper()
            {

            }

        diskPartDiskNumbers_t getDisks(char const* status = DISK_STATUS_ANY, DISK_TYPE type = DISK_TYPE_ANY, DISK_FORMAT_TYPE formatType = DISK_FORMAT_TYPE_ANY);

        diskPartDiskNumbers_t convertDiskNumberListToSet(std::list<std::string> const& diskPartDiskNumberList)
            {
                diskPartDiskNumbers_t diskNumbers;
                std::list<std::string>::const_iterator iter(diskPartDiskNumberList.begin());
                std::list<std::string>::const_iterator iterEnd(diskPartDiskNumberList.end());
                for (/* empty */; iter != iterEnd; ++iter) {
                    diskNumbers.insert(*iter);
                }
                return diskNumbers;
            }
        
        bool rescanDisks();
        bool onlineDisks(diskPartDiskNumbers_t const& disks = diskPartDiskNumbers_t());
        bool offlineDisks(diskPartDiskNumbers_t const& disks = diskPartDiskNumbers_t());
        bool deleteMissingDisks(diskPartDiskNumbers_t const& disks = diskPartDiskNumbers_t());
        bool importForeignDisks(diskPartDiskNumbers_t const& disks = diskPartDiskNumbers_t());
        bool recoverDisks(diskPartDiskNumbers_t const& disks = diskPartDiskNumbers_t());
        bool cleanDisks(diskPartDiskNumbers_t const& disks = diskPartDiskNumbers_t(), char const* status = DISK_STATUS_ANY, DISK_TYPE type = DISK_TYPE_ANY, DISK_FORMAT_TYPE formatType = DISK_FORMAT_TYPE_ANY);
        bool clearDisksAttribute(diskPartDiskNumbers_t const& disks = diskPartDiskNumbers_t(), DISK_ATTRIBUTE attribute = DISK_READ_ONLY);
        bool setPartitionId(diskPartDiskNumberAndPartitions_t const& diskNumbersAndPrtitions, char const* id);
        bool automount(bool enable);
        bool onlineVolumes();
        bool removeCdDvdDriveLetterMountPoint();
        bool convertDisks(diskPartDiskNumbers_t const& disks, DISK_TYPE type = DISK_TYPE_ANY, DISK_FORMAT_TYPE formattype = DISK_FORMAT_TYPE_ANY);

        std::string getError()
            {
                return m_errMsg;
            }

    protected:
        typedef std::list<std::string> successText_t;
        typedef std::list<std::string> diskPartDiskNumbersList_t;

        bool supportsAllRequests();

        bool executeCmd(std::string const& cmd, std::string& response);

        bool validateResponse(std::string const& response, size_t count, successText_t const& checkForText);

        bool parseGetDiskResponse(std::string const& response,
                                  diskPartDiskNumbers_t& disks,
                                  std::string status = DISK_STATUS_ANY,
                                  DISK_TYPE type = DISK_TYPE_ANY,
                                  DISK_FORMAT_TYPE formatType = DISK_FORMAT_TYPE_ANY);

    private:
        bool m_supportsAllRequests;

        std::string m_errMsg;     ///< additional error info when available. can be empty even if there are errors.

    };

} // namespace DISKPART

#endif // DISKPARTWRAPPER_H
