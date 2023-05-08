#ifndef EVTCOLLFORW_FORWARDERS_H
#define EVTCOLLFORW_FORWARDERS_H

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/filesystem.hpp>

#ifdef SV_WINDOWS
#include "azureagent.h"
#endif // SV_WINDOWS

#include "Monitoring.h"

#include "Collectors.h"
#include "EvtCollForwCommon.h"
#include "EventHubRestHelper.h"

namespace EvtCollForw
{
    template <class TForwardedData, class TCommitData>
    class ForwarderBase : boost::noncopyable
    {
    public:
        virtual ~ForwarderBase() {}

        virtual bool PutNext(const TForwardedData& data, const TCommitData& newState) = 0;
        // Move the final state to commit for this cycle further ahead; say converter failed on
        // or collector filtered out few data after the final PutNext().
        virtual void Complete(const TCommitData& newState) = 0;
        // This method would be called at the end of a not complete cycle. Ex: if a forwarding
        // throttle limit is reached.
        virtual void Complete() = 0;

        void Initialize(boost::function2<void, const TCommitData&, bool> commitCallback)
        {
            m_commitCallback = commitCallback;
        }

    protected:
        void Commit(const TCommitData& newState, bool hasMoreData)
        {
            if (m_commitCallback)
                m_commitCallback(newState, hasMoreData);
        }

    private:
        boost::function2<void, const TCommitData&, bool> m_commitCallback;
    };

    namespace FileStateIndicatorNames
    {
        static const char *CompletedIndicator = "_completed_";
        static const char *InProgressIndicator = "_current_";
    }

    template <class TCommitData>
    class AccumulatingForwarder : public ForwarderBase<std::string, TCommitData>
    {
    public:
        AccumulatingForwarder()
            : m_isLastAccumStateValid(false)
        {
        }

        virtual ~AccumulatingForwarder() {}

        virtual void Complete()
        {
            this->OnComplete();

            if (m_isLastAccumStateValid)
                this->Commit(m_lastAccumdState, false);
        }

        virtual void Complete(const TCommitData& newState)
        {
            // This is being done to force the commit for state provided by the collector. This method could
            // be called, when there have been no data upload by the forwarder. For ex, this could happen
            // when collector have gone through some events but have filtered them out. A commit is
            // needed by the collector and hence forced here.
            this->SetAccumulatedState(newState);
            this->Complete();
        }

    protected:
        virtual void OnComplete() = 0;

        void SetAccumulatedState(const TCommitData& lastAccumdState)
        {
            m_lastAccumdState = lastAccumdState;
            m_isLastAccumStateValid = true;
        }

        bool GetAccumulatedState(TCommitData& lastAccumdState) const
        {
            lastAccumdState = m_lastAccumdState;
            return m_isLastAccumStateValid;
        }

    private:
        TCommitData m_lastAccumdState;
        bool m_isLastAccumStateValid;
    };

    template <class TCommitData>
    class CxPsForwarder : public AccumulatingForwarder<TCommitData>
    {
    public:
        CxPsForwarder(boost::shared_ptr<CxTransport> cxTransport,
            const std::string &targetFilePrefix, const std::string &targetFileExt)
            : m_cxTransport(cxTransport),
            m_targetFilePrefix(targetFilePrefix),
            m_targetFileExt(targetFileExt),
            m_initialized(false),
            m_streamData(true)
        {
            BOOST_ASSERT(m_cxTransport);
        }

        CxPsForwarder(boost::shared_ptr<CxTransport> cxTransport,
            const std::string &targetFilePrefix, const std::string &targetFileExt,
            bool isStreamData)
            : m_cxTransport(cxTransport),
            m_targetFilePrefix(targetFilePrefix),
            m_targetFileExt(targetFileExt),
            m_initialized(false),
            m_streamData(isStreamData)
        {
            BOOST_ASSERT(m_cxTransport);
        }

        virtual ~CxPsForwarder() {}

        void Initialize(
            size_t maxPerFileSize,
            size_t maxTotalFileSizeInParent,
            size_t maxFilesCntInParent,
            uint64_t incompleteCleanupTimeout,
            uint32_t maxNumOfAttempts)
        {
            if (m_initialized)
            {
                BOOST_ASSERT(false);
                throw std::logic_error("Incorrect usage of CxPsForwarder. Already initialized.");
            }

            // Set the limits
            m_maxPerFileSize = maxPerFileSize;
            m_maxTotalFileSizeInParent = maxTotalFileSizeInParent;
            m_maxFilesCntInParent = maxFilesCntInParent;
            m_incompleteCleanupTimeout = incompleteCleanupTimeout;
            m_maxNumOfAttempts = maxNumOfAttempts;

            BOOST_ASSERT(m_cxTransport);
            // The accumulating logic would only work, if the max size is non-zero.
            BOOST_ASSERT(m_maxPerFileSize > 0);

            // List all the siblings, so that the parent folder's size is calculated.
            boost::filesystem::path givenPrefixPath(m_targetFilePrefix);
            BOOST_ASSERT(givenPrefixPath.has_parent_path());
            boost::filesystem::path allSiblingsPath = givenPrefixPath.parent_path();
            allSiblingsPath /= "*" + m_targetFileExt;

            const bool includePath = true;
            FileInfos_t matchingFiles;

            for (uint32_t currAttemptCnt = 1; currAttemptCnt <= m_maxNumOfAttempts; currAttemptCnt++)
            {
                // TODO-SanKumar-1702: The list file call really goes for a toss, if there's a folder under
                // say logs\* or telemetry\*. Although these are our folders, we can't guarantee that
                // these folders won't get any sub folders. so fix the buildFileInfo() method to handle
                // dirs in the FileInfo list too. Currently it expects all files and so the lexical parsing
                // for file size of a sub folder fails.
                // !!!!!!! so the fix has been made above to query only the *.json files inside the logs folder.
                // This shouldn't be spitting any folders or sub folders but only the JSON files. This is all
                // good but if someother file is put in this folder, it's size won't be taken into account and
                // our promise of folder size (not the total telemetry / log files size) is invalid.
                boost::tribool queryResult = m_cxTransport->listFile(
                    allSiblingsPath.string(), matchingFiles, includePath);

                if (boost::indeterminate(queryResult) || queryResult)
                {
                    DebugPrintf(EvCF_LOG_DEBUG,
                        "%s - List file succeeded for pattern : %s. Attempt : %lu. NotFound : %d. Status : %s.\n",
                        FUNCTION_NAME, allSiblingsPath.string().c_str(), currAttemptCnt,
                        boost::indeterminate(queryResult), m_cxTransport->status());
                    break;
                }

                const char *errMsg = "Failed to list the files";
                DebugPrintf(EvCF_LOG_ERROR, "%s - %s with pattern : %s. Attempt : %lu. Error : %s.\n",
                    FUNCTION_NAME, errMsg, allSiblingsPath.string().c_str(),
                    currAttemptCnt, m_cxTransport->status());

                if (currAttemptCnt == m_maxNumOfAttempts)
                    throw INMAGE_EX(errMsg);
            }

            // Calculate available size at PS parent folder
            size_t totalSiblingFilesSize = 0;
            size_t totalSiblingFilesCnt = 0;

            //  Try cleaning incomplete files that have timed out.
            uint64_t currTime = GetTimeInSecSinceEpoch1601();
            const size_t InProgIndicSize = strlen(FileStateIndicatorNames::InProgressIndicator);
            std::string psFilesToDelete;
            for (FileInfos_t::const_iterator itr = matchingFiles.begin(); itr != matchingFiles.end(); itr++)
            {
                totalSiblingFilesSize += itr->m_size;
                totalSiblingFilesCnt++;

                boost::filesystem::path currFile(itr->m_name);
                BOOST_ASSERT(currFile.has_stem());
                std::string currStem = currFile.stem().string();

                size_t inCompLoc = currStem.rfind(FileStateIndicatorNames::InProgressIndicator);

                // TODO-SanKumar-1702: Validate if Delete actually works for C:\Prog...., which is how
                // it is passed by ListFile() call. so, would it work if the path is not the accepted
                // folder - i.e. doesn't start with /home/svsystems?
                if (inCompLoc != std::string::npos)
                {
                    try
                    {
                        // the file names are stamped will milli-secs to avoid overwriting by multiple components on appliance
                        // convert to seconds from the file names before check for deletion
                        std::string currFileEpochStr = currStem.substr(inCompLoc + InProgIndicSize);
                        uint64_t currFileEpochStamp = boost::lexical_cast<size_t>(currFileEpochStr.substr(0, 10));

                        DebugPrintf(EvCF_LOG_INFO, "%s: file %s, currTime %lld, currFileEpochStamp %lld.\n", FUNCTION_NAME,
                            itr->m_name.c_str(), currTime, currFileEpochStamp);

                        if (currTime > currFileEpochStamp + m_incompleteCleanupTimeout)
                        {
                            if (!psFilesToDelete.empty())
                                psFilesToDelete += ';';

                            // TODO-SanKumar-1702: Should we have max check on this appended string?
                            psFilesToDelete += itr->m_name;
                        }
                    }
                    CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                        GENERIC_CATCH_LOG_IGNORE("Finding if the incomplete file has expired");
                }
            }

            if (!psFilesToDelete.empty())
            {
                // Try deleting the PS files but note that we don't reduce the totalSiblingSize/Cnt in
                // this iteration. This reduces the file load in PS till the next iteration.

                for (uint32_t currAttemptCnt = 1; currAttemptCnt <= m_maxNumOfAttempts; currAttemptCnt++)
                {
                    boost::tribool delResult = m_cxTransport->deleteFile(psFilesToDelete);

                    if (!boost::indeterminate(delResult) && delResult)
                    {
                        DebugPrintf(EvCF_LOG_DEBUG,
                            "%s - Succeeded deleting files : %s in PS. Attempt : %lu. Status : %s.\n",
                            FUNCTION_NAME, psFilesToDelete.c_str(), currAttemptCnt, m_cxTransport->status());

                        break;
                    }

                    const char *errMsg = "Failed to delete the in-progress files";
                    DebugPrintf(EvCF_LOG_ERROR,
                        "%s - %s : %s. Attempt : %lu. FileNotFound : %d. Error : %s.\n",
                        FUNCTION_NAME, errMsg, psFilesToDelete.c_str(), currAttemptCnt,
                        boost::indeterminate(delResult), m_cxTransport->status());

                    // This is best-effort operation. No need to fail the initialization.
                }
            }

            m_availableSizeInParent = (totalSiblingFilesSize >= m_maxTotalFileSizeInParent) ?
                0 : (m_maxTotalFileSizeInParent - totalSiblingFilesSize);

            m_availableFilesCntInParent = (totalSiblingFilesCnt >= m_maxFilesCntInParent) ?
                0 : (m_maxFilesCntInParent - totalSiblingFilesCnt);

            m_initialized = true;
        }

        virtual bool PutNext(const std::string& data, const TCommitData& newState)
        {
            if (!m_initialized)
            {
                BOOST_ASSERT(false);
                throw std::logic_error("Incorrect usage of CxPsForwarder. Not initialized.");
            }

            if (m_availableSizeInParent < data.size())
                return false;

            BOOST_ASSERT(m_cxTransport);
            BOOST_ASSERT(m_buffer.size() <= m_maxPerFileSize);

            // Check overflow and if the length is crossed upload the file to PS.
            if (m_buffer.size() + data.size() > m_maxPerFileSize)
            {
                if (m_buffer.empty())
                {
                    // If a single message can't be accomodated into the maximum size of the file,
                    // then the progress would be stuck here failing forever.
                    this->SetAccumulatedState(newState);
                    return true;
                    // TODO-SanKumar-1702: Add a debug log.
                }
                else
                {
                    PersistBufferToPs(m_buffer);
                    m_availableFilesCntInParent--;
                    BOOST_ASSERT(m_buffer.empty());

                    TCommitData lastAccumdState;
                    BOOST_VERIFY(this->GetAccumulatedState(lastAccumdState));
                    this->Commit(lastAccumdState, true);
                }
            }

            // Note: Explicitly differing till this point, as there's a persist in the above block,
            // which would reduce the number of available files.
            if (m_availableFilesCntInParent < 1)
                return false;

            BOOST_ASSERT(m_buffer.size() + data.size() <= m_maxPerFileSize);

            if (!m_streamData && !m_buffer.empty())
            {
                m_buffer += "\n";
                m_buffer += data;
                m_availableSizeInParent -= data.size() + 1;
            }
            else
            {
                m_buffer += data;
                m_availableSizeInParent -= data.size();
            }
            this->SetAccumulatedState(newState);
            return true;
        }

    protected:
        virtual void OnComplete()
        {
            if (!m_buffer.empty())
            {
                BOOST_ASSERT(m_buffer.size() <= m_maxPerFileSize);
                PersistBufferToPs(m_buffer);
                BOOST_ASSERT(m_buffer.empty());
            }
        }

        // TODO-SanKumar-1702: Use explicit constructors for all non-copyables.
        // TODO-SanKumar-1702: Name all the functions virtual for better coding style and proper
        // continuity in inheritance.

    private:
        void PersistBufferToPs(std::string &string)
        {
            if (string.empty())
            {
                BOOST_ASSERT(false);
                return;
            }

            const bool moreData = false;
            const bool createDirs = true;
            const bool offset = 0;

            std::string epochStamp = Utils::GetUniqueTime();
            std::string inProgFileName = m_targetFilePrefix + FileStateIndicatorNames::InProgressIndicator + epochStamp + m_targetFileExt;

            BOOST_ASSERT(m_cxTransport);

            for(uint32_t currAttemptCnt = 1; currAttemptCnt <= m_maxNumOfAttempts; currAttemptCnt++)
            {
                if (m_cxTransport->putFile(
                    inProgFileName, string.size(), string.c_str(), moreData, COMPRESS_NONE, createDirs, offset))
                {
                    DebugPrintf(EvCF_LOG_DEBUG,
                        "%s - Put file succeded for file : %s. Attempt : %lu. Status : %s.\n",
                        FUNCTION_NAME, inProgFileName.c_str(), currAttemptCnt, m_cxTransport->status());
                    break;
                }

                const char *errMsg = "Failed to put the requested file";
                DebugPrintf(EvCF_LOG_ERROR,
                    "%s - %s : %s. Attempt : %lu. Error : %s.\n",
                    FUNCTION_NAME, errMsg, inProgFileName.c_str(), currAttemptCnt, m_cxTransport->status());

                if (currAttemptCnt == m_maxNumOfAttempts)
                    throw INMAGE_EX(errMsg);
            }

            std::string completedFileName = m_targetFilePrefix + FileStateIndicatorNames::CompletedIndicator + epochStamp + m_targetFileExt;

            for (uint32_t currAttemptCnt = 1; currAttemptCnt <= m_maxNumOfAttempts; currAttemptCnt++)
            {
                boost::tribool renResult = m_cxTransport->renameFile(inProgFileName, completedFileName, COMPRESS_NONE);

                if (!boost::indeterminate(renResult) && renResult)
                {
                    DebugPrintf(EvCF_LOG_DEBUG,
                        "%s - Rename file succeeded from : %s to : %s. Attempt : %lu. Status : %s.\n",
                        FUNCTION_NAME, inProgFileName.c_str(), completedFileName.c_str(),
                        currAttemptCnt, m_cxTransport->status());
                    break;
                }

                const char *errMsg = "Failed to rename the completed file";
                DebugPrintf(EvCF_LOG_ERROR,
                    "%s - %s from : %s to : %s. Attempt : %lu. OldFileMissing : %d. Error : %s.\n",
                    FUNCTION_NAME, errMsg, inProgFileName.c_str(), completedFileName.c_str(),
                    currAttemptCnt, boost::indeterminate(renResult), m_cxTransport->status());

                if (currAttemptCnt == m_maxNumOfAttempts)
                    throw INMAGE_EX(errMsg);
            }

            string.clear();
        }

        boost::shared_ptr<CxTransport> m_cxTransport;
        std::string m_targetFilePrefix;
        std::string m_targetFileExt;

        bool m_initialized;
        bool m_streamData;
        uint32_t m_maxNumOfAttempts;
        size_t m_maxPerFileSize;
        size_t m_maxTotalFileSizeInParent;
        size_t m_maxFilesCntInParent;
        uint64_t m_incompleteCleanupTimeout;
        size_t m_availableSizeInParent;
        size_t m_availableFilesCntInParent;

        std::string m_buffer;
    };

    template <class TCommitData>
    class RcmServiceBusForwarder : public AccumulatingForwarder<TCommitData>
    {
    public:
        RcmServiceBusForwarder(size_t maxServiceBusMsgSize, const std::string& messageType, const std::string& messageSource)
            : m_maxServiceBusMsgSize(maxServiceBusMsgSize),
            m_messageType(messageType),
            m_messageSource(messageSource)
        {
            BOOST_ASSERT(m_maxServiceBusMsgSize > 2);
        }

        virtual ~RcmServiceBusForwarder() {}

        virtual bool PutNext(const std::string& data, const TCommitData& newState)
        {
            // We construct the JSON array in this method by appending one by one of its elements.
            // There should be space for 2 more characters always free in the buffer, as one char is
            // used for prefix - [ for first element or , for next elements and one char is used in
            // the future for finishing the array with ].
            // So on checking if there's an overflow, 2 chars for JSON array tokens is also considered
            // along with the actual data size.
            BOOST_ASSERT(m_buffer.size() + 2 <= m_maxServiceBusMsgSize);

            bool isFirstElement = m_buffer.empty();
            size_t requiredSizeForElement = data.size();

            if (m_buffer.size() + requiredSizeForElement + 2 > m_maxServiceBusMsgSize)
            {
                if (isFirstElement)
                {
                    // If the current element is the first element and it has overflown, we have no other
                    // option other than marking this as success after dropping the message. Otherwise, we
                    // would keep on failing for this element, while posting and never move forward.
                    this->SetAccumulatedState(newState);
                    BOOST_ASSERT(m_buffer.empty());

                    // TODO-SanKumar-1704: Should we log (and/or post) a warning along with the discarded element?
                    return true;
                }
                else
                {
                    // Flush the buffer and commit the last accumd state.
                    SendMsgToSvcBus();
                    BOOST_ASSERT(m_buffer.empty());

                    TCommitData lastAccumdState;
                    BOOST_VERIFY(this->GetAccumulatedState(lastAccumdState));
                    this->Commit(lastAccumdState, true); // MoreData = true, as this is overflow.
                }
            }

            // If this is the first element, it's a start of the JSON array ([) and if it's not,
            // then it's a start of next element in JSON array (,).
            m_buffer += isFirstElement ? '[' : ',';

            m_buffer += data;
            this->SetAccumulatedState(newState);
            return true;
        }

    protected:
        virtual void OnComplete()
        {
            if (!m_buffer.empty())
            {
                SendMsgToSvcBus();
                BOOST_ASSERT(m_buffer.empty());
            }
        }

    private:
        void SendMsgToSvcBus()
        {
            if (m_buffer.empty())
            {
                BOOST_ASSERT(false);
                return;
            }

            // Complete the JSON array.
            m_buffer += ']';

            BOOST_ASSERT(m_buffer.size() <= m_maxServiceBusMsgSize);

            // TODO-SanKumar-1704: The network consumption of the service bus upload should be throttled
            // per hour/min and also we should govern the interval between two posts using the config.
            if (!RcmClientLib::MonitoringLib::PostAgentLogsToRcm(m_messageType, m_messageSource, m_buffer))
            {
                const char *errMsg = "Failed to post the accumulated message to RCM";
                DebugPrintf(EvCF_LOG_ERROR, "%s\n", errMsg);
                // The message hasn't been accumulated and sent to the bus. Retryable error.
                throw INMAGE_EX(errMsg);
            }

            m_buffer.clear();
        }

        std::string m_buffer;

        size_t m_maxServiceBusMsgSize;
        std::string m_messageType;
        std::string m_messageSource;
    };

    class AgentSetupLogForwarder : public ForwarderBase <std::vector<std::string>, std::string>
    {
    public:
        AgentSetupLogForwarder(const std::string & targetFilePrefix,
            const std::string & targetFileExt,
            bool streamData,
            int maxMsgSize)
            :m_targetFilePrefix(targetFilePrefix),
            m_targetFileExt(targetFileExt),
            m_streamData(streamData),
            m_maxMsgSize(maxMsgSize)
        {}

        virtual ~AgentSetupLogForwarder() {}

        virtual bool PutNext(const std::vector<std::string>& data, const std::string& newState)
        {
            for (std::vector<std::string>::const_iterator itr = data.begin(); itr != data.end(); itr++)
            {
                std::string currRow = *itr;
                if (m_buffer.size() + currRow.size() > m_maxMsgSize)
                {
                    // if the buffer is empty and a single line of data
                    // is greater than max_size. then drop that line.
                    if (!m_buffer.empty())
                    {
                        persist();
                    }
                }
                else
                {
                    if (m_buffer.empty())
                    {
                        m_buffer = currRow + "\n";
                    }
                    else
                    {
                        m_buffer += currRow + "\n";
                    }
                }
            }
            if (!m_buffer.empty())
                persist();
            Commit(newState, false);

            return true;
        }

        virtual void Complete(const std::string& newState)
        {
            Commit(newState, false);
        }

        virtual void Complete()
        {}

    private:
        void persist()
        {
            boost::system::error_code ec;
            std::string epochStamp = Utils::GetUniqueTime();
            std::string inProgFileName = m_targetFilePrefix + FileStateIndicatorNames::InProgressIndicator + epochStamp + m_targetFileExt;

            boost::filesystem::path ppath = boost::filesystem::path(inProgFileName).parent_path();
            if (!boost::filesystem::exists(ppath))
            {
                boost::filesystem::create_directories(ppath);
            }

            std::ofstream ofs(inProgFileName.c_str());
            if (!ofs.good())
            {
                std::string errMsg = "Unable to open the file " + inProgFileName;
                throw std::runtime_error(errMsg.c_str());
            }

            ofs << m_buffer;
            ofs.flush();
            ofs.close();
            m_buffer.clear();

            std::string completedFileName = m_targetFilePrefix + FileStateIndicatorNames::CompletedIndicator + epochStamp + m_targetFileExt;
            boost::filesystem::rename(inProgFileName, completedFileName, ec);
            if (ec != boost::system::errc::success)
            {
                std::string errMsg = "Failed to rename file " + inProgFileName + " to " + completedFileName;
                throw std::runtime_error(errMsg.c_str());
            }
        }

        boost::function2<void, const std::string&, bool> m_commitCallback;

        std::string m_targetFilePrefix;
        std::string m_targetFileExt;
        bool m_streamData;
        int m_maxMsgSize;
        std::string m_buffer;
    };

#ifdef SV_WINDOWS
    class MarsComForwarder : public ForwarderBase<std::vector<std::string>, std::vector<std::string>>
    {
    public:
        MarsComForwarder(unsigned int maxNumOfFiles);
        virtual ~MarsComForwarder() {}

        bool PutNext(const std::vector<std::string> &filesToUpload, const std::vector<std::string> &newState);
        void Complete();
        void Complete(const std::vector<std::string> &newState);

    private:
        // TODO-SanKumar-1702: Unique ptr?
        boost::shared_ptr<AzureAgentInterface::AzureAgent> m_azureagent_ptr;

        unsigned int m_maxNumOfFiles;
    };

    //Todo:sadewang: Fix build on linux
    class EventHubForwarder : public ForwarderBase < std::vector<std::string >, std::vector <std::string> >
    {
    public:
        explicit EventHubForwarder();
        virtual ~EventHubForwarder() {}

        bool PutNext(const std::vector<std::string> &filesToUpload, const std::vector<std::string> &newState);
        void Complete();
        void Complete(const std::vector<std::string> &newState);
    private:
        bool Upload_File(const std::string& filepath);
        EventHubRestHelper m_eventHubRestHelper;
    };
#endif // SV_WINDOWS

    template <class TCommitData>
    class EventHubForwarderEx : public AccumulatingForwarder<TCommitData>
    {
    public:
        explicit EventHubForwarderEx(const std::string& tableName)
            :m_eventHubRestHelper(EvtCollForw_Env_V2ARcmSourceOnAzure),
            m_tableName(tableName)
        {
        }

        virtual ~EventHubForwarderEx() {}

        bool PutNext(const std::string& data, const TCommitData &newState)
        {
            // Check overflow and if the length is crossed upload the file to PS.
            if (m_buffer.size() + data.size() > EventHubMaxFileSize)
            {
                if (m_buffer.empty())
                {
                    // If a single message can't be accomodated into the maximum size of the file,
                    // then the progress would be stuck here failing forever.
                    DebugPrintf(EvCF_LOG_ERROR,
                        "%s: message size %d is more than max supported %d\n",
                        FUNCTION_NAME,
                        data.size(),
                        EventHubMaxFileSize);

                    this->SetAccumulatedState(newState);
                    return true;
                }
                else
                {
                    UploadData();
                    BOOST_ASSERT(m_buffer.empty());
                    TCommitData lastAccumdState;
                    BOOST_VERIFY(this->GetAccumulatedState(lastAccumdState));
                    this->Commit(lastAccumdState, true);
                }
            }

            m_buffer += "\n";
            m_buffer += data;

            this->SetAccumulatedState(newState);
            return true;
        }
    protected:
        virtual void OnComplete()
        {
            if (!m_buffer.empty())
            {
                BOOST_ASSERT(m_buffer.size() <= EventHubMaxFileSize);
                UploadData();
                BOOST_ASSERT(m_buffer.empty());
            }
        }
    private:
        void UploadData()
        {
            m_eventHubRestHelper.UploadData(m_buffer, m_tableName);
            m_buffer.clear();
        }

        EventHubRestHelper      m_eventHubRestHelper;
        const std::string       m_tableName;
        std::string             m_buffer;
    };

    template <class TForwardedData, class TCommitData>
    class PersistAndPruneForwarder : public ForwarderBase<TForwardedData, TCommitData>
    {
    public:
        explicit PersistAndPruneForwarder(const std::string& targetDirPath,
            const std::string& targetFileName,
            const unsigned int maxFileSize,
            const unsigned int maxFileCount)
            :m_targetDirPath(targetDirPath),
            m_targetFileName(targetFileName),
            m_maxFileCount(maxFileCount),
            m_maxFileSize(maxFileSize)
        {
            DebugPrintf(EvCF_LOG_DEBUG,
                "PersistAndPruneForwarder: targetDir %s targetFileName %s maxFileSize %d maxFileCount %d\n",
                targetDirPath.c_str(),
                targetFileName.c_str(),
                maxFileSize,
                maxFileCount);

            InitializeTargetFileNames(targetFileName);
            GetTargetFileCurrentSize();
        }

        explicit PersistAndPruneForwarder(const std::string& targetDirPath,
            const unsigned int maxFileSize,
            const unsigned int maxFileCount)
            :m_targetDirPath(targetDirPath),
            m_maxFileCount(maxFileCount),
            m_maxFileSize(maxFileSize)
        {
            DebugPrintf(EvCF_LOG_DEBUG,
                "PersistAndPruneForwarder: targetDir %s maxFileSize %d maxFileCount %d\n",
                targetDirPath.c_str(),
                maxFileSize,
                maxFileCount);

            InitializeTargetFileNames(std::string());
        }

        virtual ~PersistAndPruneForwarder() {}

        bool PutNext(const std::string& data, const TCommitData& newState)
        {
            Persist(data);
            this->Commit(newState, false);
            return true;
        }

        bool PutNext(const std::vector<std::string>& filesToPersist, const TCommitData& newState)
        {
            Persist(filesToPersist);
            this->Commit(newState, false);
            return true;
        }

        virtual void Complete(const TCommitData& newState)
        {
            this->Commit(newState, false);
        }

        virtual void Complete()
        {}

    private:

        void InitializeTargetFileNames(const std::string& targetFileName)
        {
            m_allowedTargetFileNames.insert(A2ATableNames::AdminLogs);
            m_allowedTargetFileNames.insert(A2ATableNames::AgentSetupLogs);
            m_allowedTargetFileNames.insert(A2ATableNames::DriverTelemetry);
            m_allowedTargetFileNames.insert(A2ATableNames::PSTransport);
            m_allowedTargetFileNames.insert(A2ATableNames::PSV2);
            m_allowedTargetFileNames.insert(A2ATableNames::SourceTelemetry);
            m_allowedTargetFileNames.insert(A2ATableNames::Vacp);

            if (!targetFileName.empty() &&
               (m_allowedTargetFileNames.find(targetFileName) == m_allowedTargetFileNames.end()))
            {
                std::string errMsg = targetFileName + " is not an allowed target file name.";
                throw std::runtime_error(errMsg.c_str());
            }
        }

        void GetTargetFileCurrentSize()
        {
            boost::filesystem::path filePath(m_targetDirPath);
            filePath /= m_targetFileName;
            boost::system::error_code ec;
            if (!boost::filesystem::exists(filePath, ec))
                m_targetFileSize = 0;
            else
            {
                m_targetFileSize = boost::filesystem::file_size(filePath, ec);
                if (ec)
                {
                    DebugPrintf(EvCF_LOG_ERROR,
                        "GetTargetFileCurrentSize: Failed to get size for %s with error %s\n",
                        filePath.string().c_str(),
                        ec.message().c_str());

                    std::string errMsg = "Failed to get size of file " + filePath.string() + " with error " + ec.message();
                    throw std::runtime_error(errMsg.c_str());
                }
            }

            DebugPrintf(EvCF_LOG_DEBUG,
                "GetTargetFileCurrentSize: Target File %s, size %d\n",
                filePath.string().c_str(),
                m_targetFileSize);
        }

        void Persist(const std::string& data)
        {
            boost::system::error_code ec;
            std::string epochStamp = Utils::GetUniqueTime();

            boost::filesystem::path filePath(m_targetDirPath);
            filePath /= m_targetFileName;

            GetTargetFileCurrentSize();

            // Check overflow and if the length is crossed, rotate the file
            if (m_targetFileSize + data.size() > m_maxFileSize)
            {
                if (!m_targetFileSize)
                {
                    // If a single message can't be accomodated into the maximum size of the file,
                    // then the progress would be stuck here failing forever.
                    DebugPrintf(EvCF_LOG_ERROR,
                        "%s: message size %d is more than max supported %d\n",
                        FUNCTION_NAME,
                        data.size(),
                        m_maxFileSize);

                    return;
                }

                // rotate the file
                std::string completedFileName = filePath.string() + "_" + epochStamp;

                DebugPrintf(EvCF_LOG_DEBUG,
                    "Rotating file %s of size %d to %s\n",
                    filePath.string().c_str(),
                    m_targetFileSize,
                    completedFileName.c_str());

                boost::filesystem::rename(filePath.string(), completedFileName, ec);
                if (ec != boost::system::errc::success)
                {
                    std::string errMsg = "Failed to rename file " + filePath.string() + " to " + completedFileName + " with error " + ec.message();
                    throw std::runtime_error(errMsg.c_str());
                }

                // zip the file
                Utils::CompressFile(completedFileName);

                // delete any older files
                Utils::DeleteOlderFiles(filePath.string(), m_maxFileCount);
            }

            if (!boost::filesystem::exists(m_targetDirPath))
            {
                boost::filesystem::create_directories(m_targetDirPath);
            }

            bool targetFileExists = boost::filesystem::exists(filePath);
            std::ofstream ofs(filePath.string().c_str(), targetFileExists ? std::ios::app : std::ios::out);
            if (!ofs.good())
            {
                std::string errMsg = "Unable to open the file " + filePath.string();
                throw std::runtime_error(errMsg.c_str());
            }

            ofs << data << "\n";
            ofs.flush();
            ofs.close();
        }

        void Persist(const std::vector<std::string>& filesToPersist)
        {
            std::vector<std::string>::const_iterator fileIter = filesToPersist.begin();
            for (/*empty*/; fileIter != filesToPersist.end(); fileIter++)
            {
                m_targetFileName = GetTargetFileName(*fileIter);
                PersistFile(*fileIter);
            }
        }

        std::string GetTargetFileName(const std::string& srcFilepath)
        {
            boost::filesystem::path filePath(srcFilepath);
            std::string filename = filePath.stem().string();
            std::string targetFileName = filename.substr(0, filename.find("_"));
            std::string remappedTargetFileName = Utils::GetA2ATableNameFromV2ATableName(targetFileName);

            DebugPrintf(EvCF_LOG_DEBUG,
                "src File %s, target file name %s\n",
                srcFilepath.c_str(),
                remappedTargetFileName.c_str());

            if (remappedTargetFileName.empty() ||
                (m_allowedTargetFileNames.find(remappedTargetFileName) == m_allowedTargetFileNames.end()))
            {
                DebugPrintf(EvCF_LOG_ERROR,
                    "remapped file name %s is not in allowed target file names.\n",
                    remappedTargetFileName.c_str());

                std::string errMsg = remappedTargetFileName + " is not an allowed target file name.";
                throw std::runtime_error(errMsg.c_str());
            }

            return remappedTargetFileName;
        }

        void PersistFile(const std::string& srcFile)
        {
            boost::filesystem::path srcFilepath(srcFile);

            GetTargetFileCurrentSize();
            boost::system::error_code ec;
            unsigned int srcFileSize = boost::filesystem::file_size(srcFilepath, ec);
            if (ec)
            {
                DebugPrintf(EvCF_LOG_ERROR,
                    "Failed to get size for %s with error %s\n",
                    srcFilepath.string().c_str(),
                    ec.message().c_str());

                std::string errMsg = "Failed to get size of file " + srcFilepath.string() + " with error " + ec.message();
                throw std::runtime_error(errMsg.c_str());
            }

            DebugPrintf(EvCF_LOG_DEBUG,
                "PersistFile: source File %s size %d\n",
                srcFilepath.string().c_str(),
                srcFileSize);

            boost::filesystem::path targetFilePath(m_targetDirPath);
            targetFilePath /= m_targetFileName;

            if (!boost::filesystem::exists(m_targetDirPath))
            {
                boost::filesystem::create_directories(m_targetDirPath);
            }

            bool targetFileExists = boost::filesystem::exists(targetFilePath);

            std::ifstream ifile(srcFilepath.string().c_str());
            if ((srcFileSize + m_targetFileSize) <= m_maxFileSize)
            {
                DebugPrintf(EvCF_LOG_DEBUG, "writing entire file\n");

                std::ofstream ofile(targetFilePath.string().c_str(), targetFileExists ? std::ios::app : std::ios::out);
                ofile << ifile.rdbuf() << "\n";
                ofile.flush();
                ofile.close();
            }
            else
            {
                DebugPrintf(EvCF_LOG_DEBUG, "writing partial file\n");

                std::string data, line;
                while (!ifile.eof() && std::getline(ifile, line))
                {
                    if (m_targetFileSize + data.size() + line.size() < m_maxFileSize)
                    {
                        DebugPrintf(EvCF_LOG_DEBUG, "caching partial file\n");

                        data += line;
                        data += '\n';
                    }
                    else
                    {
                        DebugPrintf(EvCF_LOG_DEBUG, "persisting partial file\n");

                        Persist(data);
                        data.clear();
                        data += line;
                        data += '\n';
                    }
                }

                if (!data.empty())
                    Persist(data);
            }

            ifile.close();

            Utils::TryDeleteFile(srcFilepath);
        }

        std::set<std::string>   m_allowedTargetFileNames;

        const unsigned int      m_maxFileSize;
        const unsigned int      m_maxFileCount;
        const std::string       m_targetDirPath;

        std::string             m_targetFileName;
        unsigned int            m_targetFileSize;
    };
}
#endif // !EVTCOLLFORW_FORWARDERS_H
