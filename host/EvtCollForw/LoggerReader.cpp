#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include "json_reader.h"
#include "json_writer.h"

#include "LogCutter.h"
#include "LoggerReader.h"

#ifdef SV_UNIX
#include <ace/OS_NS_sys_stat.h>
#endif // SV_UNIX

namespace EvtCollForw
{
    template <class TBookmark>
    LoggerReader<TBookmark>::LoggerReader(const std::string &logPath)
                        : LoggerReaderBase(logPath), m_bookmark()
    {
        BOOST_ASSERT(!m_logPath.empty());
    }

    template <class TBookmark>
    void LoggerReader<TBookmark>::Initialize(bool createBkmrkFileIfNotFound)
    {
        const std::string bookmarkFilePath = m_bookmark.GetBookmarkFilePath(m_logPath);

        boost::system::error_code errorCode;
        std::string bookmarkData;

        if (!boost::filesystem::exists(bookmarkFilePath, errorCode))
        {
            DebugPrintf(createBkmrkFileIfNotFound ? EvCF_LOG_INFO : EvCF_LOG_ERROR,
                "Unable to find the bookmark file : %s for log : %s.\n",
                bookmarkFilePath.c_str(), m_logPath.c_str());

            if (!createBkmrkFileIfNotFound)
                throw std::runtime_error("Failed to find the bookmark file " + bookmarkFilePath + " for " + m_logPath);
        }
        else if (!Utils::ReadFileContents(bookmarkFilePath, bookmarkData))
        {
            DebugPrintf(EvCF_LOG_ERROR,
                "Failed to read the contents of the bookmark file : %s for log : %s.\n",
                bookmarkFilePath.c_str(), m_logPath.c_str());

            // Try cleaning up the file, so the collection recovers from the next session.
            // TODO-SanKumar-1703: Workaround. We should figure out why the read fails for the file.
            Utils::TryDeleteFile(bookmarkFilePath);
            throw std::runtime_error("Failed to read the contents of the bookmark file : " + bookmarkFilePath);
        }

        if (bookmarkData.empty())
        {
            m_bookmark = TBookmark();
        }
        else
        {
            try
            {
                m_bookmark = JSON::consumer<TBookmark>::convert(bookmarkData);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                catch (...)
            {
                DebugPrintf(EvCF_LOG_ERROR,
                    "Failed to parse the contents of the bookmark file : %s for log : %s.\n",
                    bookmarkFilePath.c_str(), m_logPath.c_str());

                // Try cleaning up the file, so the collection recovers from the next session.
                // TODO-SanKumar-1703: Workaround. We should figure out why the parse fails for the file.
                Utils::TryDeleteFile(bookmarkFilePath);
                throw;
            }
        }

        m_lastCommittedBookmark = m_bookmark;

        if (m_ifstream.is_open())
        {
            BOOST_ASSERT(!m_ifstream.is_open());
            m_ifstream.close();
        }
    }

    // A very nice article used as reference :
    // https://gehrcke.de/2011/06/reading-files-in-c-using-ifstream-dealing-correctly-with-badbit-failbit-eofbit-and-perror/


    template <class TBookmark>
    bool LoggerReader<TBookmark>::GetNextMessage(std::string &message, TBookmark &currBookmark)
    {
        // If file stream is not open, get the next eligible file to be parsed.
        while (!m_ifstream.is_open())
        {
            uint64_t currSequenceNumber, currOffset;
            if (!GetNextFileForParsing(currSequenceNumber, currOffset))
            {
                DebugPrintf(EvCF_LOG_DEBUG, "Unable to find the eligible completed log file for %s.\n", m_logPath.c_str());
                currBookmark = m_bookmark;
                return false;
            }

            m_currCompletedFilePath = Logger::LogCutter::GetCompletedLogFilePath(m_logPath, currSequenceNumber).string();
            m_ifstream.open(m_currCompletedFilePath.c_str(), std::ios_base::in | std::ios_base::binary);
            if (!m_ifstream.is_open())
            {
                // TODO-SanKumar-1612: On repeated failure to load a file or error while reading
                // contents of the file, should we discard that file, after few retries?
                m_ifstream.clear();
                const char *message = "Failed to open the completed log file";
                DebugPrintf(EvCF_LOG_ERROR, "%s : %s.\n", message, m_currCompletedFilePath.c_str());
                currBookmark = m_bookmark;
                throw std::runtime_error(message);
            }

            if (currOffset != 0)
            {
                BOOST_ASSERT(0 == m_currCompletedFilePath.compare(
                    Logger::LogCutter::GetCompletedLogFilePath(m_logPath, m_bookmark.SequenceNumber).string()));

                m_ifstream.seekg(currOffset);

                // TODO-SanKumar-1705: The expectation was that the seekg() would set the fail bit
                // on moving beyond the EOF. But the offset seems to move to the EOF in that case,
                // both in Windows and Linux. So, we should modify the following check as
                // (newOffset < currOffset) to catch this scenario.
                // With the current code, instead of moving to the beginning of the file, the seek
                // moves the offset to EOF. This temp behavior is OK, since if it's the old file, we
                // already transfered the present data and if it's an overwritten file, we will drop it.
                if (m_ifstream.fail())
                {
                    DebugPrintf(EvCF_LOG_ERROR, "Failed to seek to the offset : %llu of the log file : %s.\n",
                        currOffset, m_currCompletedFilePath.c_str());

                    // It could be possible that the file is being truncated and/or overwritten by
                    // another process/driver. Finish the reading of logs and reset the bookmark
                    // to the start of the file, so that the new data is read from the file.
                    // Possibility of duplicate data, which could be tolerated, since the above
                    // mentioned case is the most likely scenario to hit this issue.
                    m_bookmark.SequenceNumber = currSequenceNumber;
                    m_bookmark.Offset = 0;
                    m_bookmark.Completed = false;

                    // Close the stream for this file.
                    if (m_ifstream.is_open())
                        m_ifstream.close();

                    m_ifstream.clear();

                    currBookmark = m_bookmark;
                    return false; // Force stop the reading loop.
                }
            }

            bool isFirstLogLineFound = false;
            uint64_t parsedOffset = m_ifstream.tellg();

            // After opening the file, drop all the lines in the log, until we find the Start indicator.
            while (std::getline(m_ifstream, m_currLine))
            {
                BOOST_ASSERT(!m_ifstream.bad());

                if (IsBeginningOfMessage(m_currLine))
                {
                    // Found the first line that's a beginning of the message.
                    isFirstLogLineFound = true;
                    break;
                }

                parsedOffset = m_ifstream.tellg();
            }

            if (isFirstLogLineFound)
            {
                m_bookmark.SequenceNumber = currSequenceNumber;
                m_bookmark.Offset = parsedOffset;
                m_bookmark.Completed = false;

                break; // Next file to parse has been found. Process the log file starting from this message.
            }

            if (m_ifstream.bad())
            {
                const char *message = "Failed to read the log file, while looking for the first log";
                DebugPrintf(EvCF_LOG_ERROR, "%s : %s.\n", message, m_currCompletedFilePath.c_str());
                m_bookmark.SequenceNumber = currSequenceNumber;
                m_bookmark.Offset = parsedOffset;
                m_bookmark.Completed = m_ifstream.eof();
                // Since an error occured during read, this shouldn't be true.
                BOOST_ASSERT(!m_bookmark.Completed);

                // Close the stream for this file, so it could be reopened on the next retry.
                if (m_ifstream.is_open())
                    m_ifstream.close();

                m_ifstream.clear();

                currBookmark = m_bookmark;
                throw std::runtime_error(message);
            }
            else
            {
                BOOST_ASSERT(m_ifstream.eof());

                DebugPrintf(EvCF_LOG_DEBUG, "Parsed the entire file : %s, but couldn't find the first proper message line. "
                    "Moving on to the next file.\n", m_currCompletedFilePath.c_str());

                m_bookmark.SequenceNumber = currSequenceNumber;
                m_bookmark.Offset = parsedOffset;
                m_bookmark.Completed = true;

                // Close the stream for this file and proceed to find parsing the next completed file.
                if (m_ifstream.is_open())
                    m_ifstream.close();

                // TODO-SanKumar-1612: Instead of trusting on the ifstream to be always closed properly,
                // to carry forward to the next file, should we set a flag and move on to the next file.
                // If there are any handles open because of an unclosed stream, they would be closed at
                // the process exit.

                m_ifstream.clear();
            }
        }

        // Parse the message from the next log file, which has at least one message.
        bool nextLogLineFound = false;
        uint64_t nextLogLineOffset = m_bookmark.Offset;
        message = m_currLine;

        // m_currLine contains the next log line to process, either from the first log line search above
        // or from the previous call to this method.
        while (std::getline(m_ifstream, m_currLine))
        {
            BOOST_ASSERT(!m_ifstream.bad());

            if (IsBeginningOfMessage(m_currLine))
            {
                // Found the next line that begins with the Start indicator.
                nextLogLineOffset = m_ifstream.tellg();
                nextLogLineFound = true;
                break;
            }

            message += "\n"; // getline() ignores the delimiter before returning.
            message += m_currLine;
        }

        if (nextLogLineFound)
        {
            m_bookmark.Offset = nextLogLineOffset;
            m_bookmark.Completed = false;

            currBookmark = m_bookmark;
            return true; // A valid message is returned.
        }

        if (m_ifstream.bad())
        {
            const char *message = "Failed to read the log file";
            DebugPrintf(EvCF_LOG_ERROR, "%s : %s.\n", message, m_currCompletedFilePath.c_str());

            // Close the stream for this file, so it could be reopened on the next retry.
            if (m_ifstream.is_open())
                m_ifstream.close();

            m_ifstream.clear();

            currBookmark = m_bookmark;
            throw std::runtime_error(message);
        }
        else
        {
            BOOST_ASSERT(m_ifstream.eof());

            DebugPrintf(EvCF_LOG_DEBUG, "Successfully parsed the entire file : %s. "
                "Moving on to the next file.\n", m_currCompletedFilePath.c_str());

            // Close the stream for this file, as it has been parsed completely.
            if (m_ifstream.is_open())
                m_ifstream.close();

            m_ifstream.clear();

            m_bookmark.Offset = nextLogLineOffset;
            m_bookmark.Completed = true;

            currBookmark = m_bookmark;
            return true; // A valid message is returned.
        }
    }

    template <class TBookmark>
    void LoggerReader<TBookmark>::GetCurrBookmark(TBookmark &currBookmark) const
    {
        currBookmark = m_bookmark;
    }

    template <class TBookmark>
    bool LoggerReader<TBookmark>::MarkCompletionAndCleanup(TBookmark &bookmark, bool shouldCleanup)
    {
        if (m_lastCommittedBookmark != bookmark)
        {
            std::string bookmarkData = JSON::producer<TBookmark>::convert(bookmark);
            std::string bookmarkFilePath = bookmark.GetBookmarkFilePath(m_logPath);

            if (!Utils::CommitState(bookmarkFilePath, bookmarkData))
            {
                DebugPrintf(EvCF_LOG_ERROR, "Failed to commit bookmark : %s.\n", bookmarkFilePath.c_str());
                return false;
            }

            m_lastCommittedBookmark = bookmark;
        }
        else
        {
            DebugPrintf(EvCF_LOG_DEBUG, "Avoiding a bookmark commit for %s as nothing has changed.\n", m_logPath.c_str());
        }

        Cleanup(bookmark, shouldCleanup);

        return true;
    }

    template <class TBookmark>
    bool AgentLoggerReader<TBookmark>::IsBeginningOfMessage(const std::string &currLine)
    {
        return boost::starts_with(currLine, Logger::LogCutter::LogStartIndicator);
    }

    template <class TBookmark>
    bool AgentLoggerReader<TBookmark>::GetNextFileForParsing(uint64_t &seqNumber, uint64_t &offset)
    {
        std::vector<uint64_t> completedLogSeqNums;
        Logger::LogCutter::ListCompletedLogFiles(LoggerReaderBase::GetLogPath(), completedLogSeqNums);

        size_t ind = 0;
        for (ind; ind < completedLogSeqNums.size() && completedLogSeqNums[ind] < LoggerReader<TBookmark>::m_bookmark.SequenceNumber; ind++);

        if (ind == completedLogSeqNums.size())
        {
            return false;
        }

        // Start from the beginning of the file, unless we are continuing to parse a file from
        // wherever left off.
        offset = 0;

        if (completedLogSeqNums[ind] == LoggerReader<TBookmark>::m_bookmark.SequenceNumber)
        {
            if (LoggerReader<TBookmark>::m_bookmark.Completed)
            {
                // Move to the next file, as the current file is completed.
                if (++ind == completedLogSeqNums.size())
                {
                    return false;
                }
            }
            else
            {
                offset = LoggerReader<TBookmark>::m_bookmark.Offset;
            }
        }
        // else, the least file sequence number after the bookmark sequence number would be returned.

        seqNumber = completedLogSeqNums[ind];

        return true;
    }

    template <class TBookmark>
    void AgentLoggerReader<TBookmark>::Cleanup(TBookmark &bookmark, bool shouldCleanup)
    {
        if (shouldCleanup)
        {
            try
            {
                std::vector<uint64_t> sequenceNumbers;
                std::string logPath = LoggerReaderBase::GetLogPath();

                Logger::LogCutter::ListCompletedLogFiles(logPath, sequenceNumbers);

                for (size_t ind = 0; ind < sequenceNumbers.size(); ind++)
                {
                    // Note: Not good practice to access template object member variable (used as of current design limitation of template based model).
                    // TODO-nichougu: can impletement OOP model for bookmark instead template model to overcome this.
                    if (sequenceNumbers[ind] > bookmark.SequenceNumber)
                        break;

                    // Cleanup the files that are lesser than bookmark sequence number or
                    // if it's equal, then cleanup if the complete file has been parsed.
                    if ((sequenceNumbers[ind] < bookmark.SequenceNumber) ||
                        (sequenceNumbers[ind] == bookmark.SequenceNumber && bookmark.Completed))
                    {
                        boost::filesystem::path currCompPath = Logger::LogCutter::GetCompletedLogFilePath(logPath, sequenceNumbers[ind]);
                        Utils::TryDeleteFile(currCompPath);
                    }
                }
            }
            // This is best effort. We have committed already, so we ignore this error as cleanup would
            // be attempted at the next mark completion as well.
            GENERIC_CATCH_LOG_IGNORE("Cleaning up completed agent log files")
        }
    }

#ifdef SV_UNIX
    template <class TBookmark>
    bool InVolFltLinuxTelemetryLoggerReader<TBookmark>::IsBeginningOfMessage(const std::string &currLine)
    {
        // Every line is the beginning of a new message for the linux driver.
        return true;
    }

    template <class TBookmark>
    bool InVolFltLinuxTelemetryLoggerReader<TBookmark>::GetNextFileForParsing(uint64_t &seqNumber, uint64_t &offset)
    {
        boost::mutex::scoped_lock guard(m_listMutex);

        offset = 0;

        // Perform one time listing of the eligible files.
        if (!m_isListingCompleted)
        {
            std::vector<uint64_t> completedLogSeqNums;
            std::string logPath = LoggerReaderBase::GetLogPath();
            Logger::LogCutter::ListCompletedLogFiles(logPath, completedLogSeqNums);

            if (!completedLogSeqNums.empty())
            {
                std::vector<time_t> mTimes;
                for (std::vector<uint64_t>::iterator itr = completedLogSeqNums.begin(); itr != completedLogSeqNums.end(); itr++)
                {
                    ACE_stat stat;
                    std::string currCompFile = Logger::LogCutter::GetCompletedLogFilePath(logPath, *itr).string();
                    int statErr = ACE_OS::stat(currCompFile.c_str(), &stat);
                    if (statErr != 0)
                    {
                        const char *message = "Failed to get stat for linux driver telemetry log file";
                        DebugPrintf(EvCF_LOG_ERROR, "%s : %s with error : %d.\n", message, currCompFile.c_str(), statErr);
                        throw std::runtime_error(message);
                    }

                    mTimes.push_back(stat.st_mtime);
                }

                size_t leastMtimeInd = std::distance(mTimes.begin(), std::min_element(mTimes.begin(), mTimes.end()));

                bool hitIgnoreTimeWindow = false;
                const time_t IGNORE_TIME_WINDOW = 3 * 60; // 3 minutes

                time_t currTime;
                time(&currTime);

                // Queue the files to be parsed starting from the least mtime in circular rotated fashion.
                // Stop whenever a file's mtime is very close to the current time, as that file could
                // still be written and not closed by the driver.

                for (size_t orderedInd = leastMtimeInd; orderedInd < completedLogSeqNums.size(); orderedInd++)
                {
                    if (mTimes[orderedInd] < currTime &&
                        mTimes[orderedInd] + IGNORE_TIME_WINDOW > currTime)
                    {
                        hitIgnoreTimeWindow = true;
                        break;
                    }

                    m_queuedFiles.push_back(completedLogSeqNums[orderedInd]);
                }

                if (!hitIgnoreTimeWindow)
                {
                    for (size_t orderedInd = 0; orderedInd < leastMtimeInd; orderedInd++)
                    {
                        if (mTimes[orderedInd] < currTime &&
                            mTimes[orderedInd] + IGNORE_TIME_WINDOW > currTime)
                        {
                            hitIgnoreTimeWindow = true;
                            break;
                        }

                        m_queuedFiles.push_back(completedLogSeqNums[orderedInd]);
                    }
                }

                // If the first file to be processed is the sequence number in the bookmark, it means
                // that the previous order persisted in the bookmark file is guaranteed and so only
                // parse from wherever left off.
                if (m_queuedFiles.front() == LoggerReader<TBookmark>::m_bookmark.SequenceNumber && !LoggerReader<TBookmark>::m_bookmark.Completed)
                    offset = LoggerReader<TBookmark>::m_bookmark.Offset;
            }

            m_isListingCompleted = true;
        }

        if (m_queuedFiles.empty())
            return false;

        seqNumber = m_queuedFiles.front();
        m_parsedFiles.push_back(seqNumber);
        m_queuedFiles.pop_front();
        return true;
    }

    template <class TBookmark>
    void InVolFltLinuxTelemetryLoggerReader<TBookmark>::Cleanup(TBookmark &bookmark, bool shouldCleanup)
    {
        try
        {
            // shouldCleanup is generally set at the end of the upload of all possible data. But, it's
            // not used here, as the files might be overwritten with new data already before the end of
            // all the upload. So, it would be better to cleanup the files as and when a corresponding
            // upload is done, in order to reduce the drop in telemetry data.

            std::vector<boost::filesystem::path> completedFilesToBeDeleted;

            {
                boost::mutex::scoped_lock guard(m_listMutex);

                if (std::find(m_parsedFiles.begin(), m_parsedFiles.end(), bookmark.SequenceNumber) == m_parsedFiles.end())
                {
                    // This scenario should only occur, when there's no more data and a valid bookmark
                    // from previous rounds or already purged file is committed again at the end.
                    BOOST_ASSERT(m_queuedFiles.empty() && m_parsedFiles.empty());
                    return;
                }

                std::string logPath = LoggerReaderBase::GetLogPath();
                while (m_parsedFiles.front() != bookmark.SequenceNumber || bookmark.Completed)
                {
                    boost::filesystem::path currCompPath = Logger::LogCutter::GetCompletedLogFilePath(logPath, m_parsedFiles.front());
                    completedFilesToBeDeleted.push_back(currCompPath);

                    bool quitLoop = (m_parsedFiles.front() == bookmark.SequenceNumber);
                    m_parsedFiles.pop_front();

                    if (quitLoop)
                        break;
                }
            }

            for (std::vector<boost::filesystem::path>::iterator itr = completedFilesToBeDeleted.begin(); itr != completedFilesToBeDeleted.end(); itr++)
            {
                Utils::TryDeleteFile(*itr);
                m_tryDeletedFilesCnt++;
            }
        }
        // This is best effort. We have committed already, so we ignore this error as the data would
        // be resent and cleanup would be attempted at the next iteration as well.
        GENERIC_CATCH_LOG_IGNORE("Cleaning up completed linux telemetry log files")
    }

#endif // SV_UNIX

}