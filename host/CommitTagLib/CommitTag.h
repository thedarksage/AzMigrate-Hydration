#ifndef COMMIT_TAG_H
#define COMMIT_TAG_H

#include "logger.h"
#include "filterdrvvolmajor.h"
#include "devicestream.h"
#include "CommitTagBookmark.h"

#include <boost/atomic.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <boost/algorithm/string.hpp>
#include <map>

using namespace RcmClientLib;

#define TEST_CXFLAG(_a, _f)       (((_a) & (_f)) == (_f))

typedef std::map<FilterDrvVol_t, std::string> FormattedDeviceIdDiskId_t;
typedef std::pair<FilterDrvVol_t, std::string> FormattedDeviceIdDiskIdPair_t;

class EqDiskId : public std::unary_function<FilterDrvVol_t, bool>
{
    FilterDrvVol_t m_diskId;
public:
    explicit EqDiskId(const FilterDrvVol_t &diskId) : m_diskId(diskId) {}
    bool operator()(const FormattedDeviceIdDiskIdPair_t &in) const
    {
        FilterDrvVol_t formattedDiskId = in.first;
        return boost::iequals(m_diskId, formattedDiskId);
    }
};

enum TagCommitNotifyState {
    TAG_COMMIT_NOTIFY_STATE_NOT_INPROGRESS = 0,
    TAG_COMMIT_NOTIFY_STATE_NOT_ISSUED,
    TAG_COMMIT_NOTIFY_STATE_WAITING_FOR_COMPLETION,
    TAG_COMMIT_NOTIFY_STATE_COMPLETED,
    TAG_COMMIT_NOTIFY_STATE_JOB_STATUS_UPDATED,
    TAG_COMMIT_NOTIFY_STATE_FAILED
};

class TagCommitNotifier {
public:

    TagCommitNotifier();
    ~TagCommitNotifier();

    void Init(DeviceStream::Ptr pDriverStream, bool blockDrain = false);

    TagCommitNotifyState GetTagCommitNotifierState();

    void SetTagCommitNotifierState(TagCommitNotifyState tagCommitNotifyState);

    bool CheckAnotherInstance(TagCommitNotifyState &expected);

    SVSTATUS PrepareFinalBookmarkCommand(
        const CommitTagBookmarkInput &commitTagBookmarkInput, const std::string &jobId,
        std::string &vacpArgs, std::string &appPolicyLogPath, std::string &preLogPath);

    SV_ULONG RunFinalBookmarkCommand(std::string &vacpArgs,
        std::string &appPolicyLogPath, std::string &preLogPath, SV_UINT uTimeOut, bool active);

    bool IsCommitTagNotifySupported();

    SVSTATUS IssueCommitTagNotify(CommitTagBookmarkInput &commitTagBookmarkInput,
        CommitTagBookmarkOutput &commitTagBookmarkOutput,
        const std::vector<std::string>& protectedDiskIds,
        std::stringstream &ssPossibleCauses);

    SVSTATUS UnblockDrain(const std::vector<std::string> &protectedDiskIds,
        std::stringstream &strErrMsg);

    SVSTATUS VerifyDrainStatus(const std::vector<std::string> &protectedDiskIds,
        uint64_t flag);

    SVSTATUS VerifyDrainBlock(const std::vector<std::string> &protectedDiskIds);

    SVSTATUS VerifyDrainUnblock(const std::vector<std::string> &protectedDiskIds);

    void CancelCommitTagNotify();

    void HandleCommitTagNotifyTimeout(const boost::system::error_code& error);

#ifdef SV_UNIX
    SVSTATUS GetPNameFromDriver(const std::string &devName, std::string &driverPName);

    SVSTATUS ModifyPersistentName(const std::string &devName,
        const std::string &curPName, const std::string &finalPName);

    SVSTATUS ModifyPName(const std::string &devName,
        const std::string &pName, std::stringstream &strErrMsg);
#endif

private:

    DeviceStream::Ptr m_pDriverStream;

    /// \brief indicates if a tag commitnotify is issued to driver and if it is completed
    boost::atomic<TagCommitNotifyState> m_tagCommitNotifyState;

    /// \brief indicates if drain is to be blocked post tag commitnotify is issued to driver
    bool m_blockDrain;

};

#endif
