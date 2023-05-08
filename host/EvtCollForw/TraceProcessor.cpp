#include <list>
#include <set>

#include "localconfigurator.h"
#include "EvtCollForwCommon.h"
#include "TraceProcessor.h"
#include "LoggerReader.h"
#include "Collectors.h"
#include "Forwarders.h"
#include "Converters.h"
#include "pssettingsconfigurator.h"

#include <boost/shared_ptr.hpp>
#include "boost/filesystem.hpp"
#include <boost/filesystem/path.hpp>

#define RETURN_IF_CANCELLATION_REQUESTED \
    if (cancelRequested && cancelRequested())  \
        { \
        return true; \
        }

extern boost::shared_ptr<LocalConfigurator> lc;
extern bool gbPrivateEndpointEnabled;

namespace EvtCollForw
{
    // TODO: Add these values to configuration.
    const int MaxPerFileSize = 1 * 1024 * 1024;
    const int MaxTotalLogsFolderSize = 20 * 1024 * 1024;
    const int MaxTotalFilesCntInParent = 8 * 3;
    const int IncompleteFileCleanupTimeout = 2 * 60 * 60; // 2 hours

    void IRTraceProcessor::UpdateFilesToProcessSet(const std::string& LOG_FORMAT)
    {
        std::map<std::string, std::string> logDirectories;
        if (boost::iequals(m_setting.Environment, EvtCollForw_Env_V2ARcmPS))
        {
            std::map<std::string, std::string> psTelemetrySettings;
            Utils::GetPSSettingsMap(psTelemetrySettings);

            for (std::map<std::string, std::string>::iterator itr = psTelemetrySettings.begin(); itr != psTelemetrySettings.end(); itr++)
            {
                logDirectories[Utils::GetRcmMTDPResyncLogFolderPath(itr->second)] =
                    Utils::GetRcmMTDPCompletedIRLogFolderPath(itr->second);
            }
        }
        else
        {
            logDirectories[Utils::GetDPResyncLogFolderPath()] = Utils::GetDPCompletedIRLogFolderPath();
        }

        for (std::map<std::string, std::string>::iterator itr = logDirectories.begin(); itr != logDirectories.end(); itr++)
        {
            boost::system::error_code errcode;
            const std::string resyncLogDir(itr->first);
            const std::string completedLogDir(itr->second);
            FileInfos_t fileInfos;
            std::string folderPattern(resyncLogDir + LOG_FORMAT);
            CxTransport fileops(TRANSPORT_PROTOCOL_FILE, TRANSPORT_CONNECTION_SETTINGS(), false);

            if (!fileops.listFile(folderPattern, fileInfos, true))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: listFile failed for %s with status: %s\n", FUNCTION_NAME, folderPattern.c_str(), fileops.status());
                return;
            }

            for (FileInfos_t::iterator iter(fileInfos.begin()); iter != fileInfos.end(); ++iter)
            {
                // Move 
                //  <Agent log path>\vxlogs\resync\...\dataprotection_completed_*.log
                // to
                //  <Agent log path>\vxlogs\completed\...\dataprotection_completed_*.log
                std::string resyncFileMetaData = iter->m_name.substr(resyncLogDir.length(), iter->m_name.length() - resyncLogDir.length());
                boost::filesystem::path completedLogFilePath(completedLogDir + resyncFileMetaData);
                try
                {
                    if (!boost::filesystem::exists(completedLogFilePath.parent_path()))
                    {
                        if (!boost::filesystem::create_directories(completedLogFilePath.parent_path(), errcode))
                        {
                            // Log error and continue
                            DebugPrintf(EvCF_LOG_ERROR, "%s: create_directories failed to create %s with error %d\n", FUNCTION_NAME,
                                completedLogFilePath.parent_path().string().c_str(), errcode.value());
                            continue;
                        }
                    }
                    if (boost::filesystem::exists(iter->m_name))
                    {
                        boost::filesystem::rename(iter->m_name, completedLogFilePath, errcode);
                        if (0 != errcode.value())
                        {
                            // Log error and continue
                            DebugPrintf(EvCF_LOG_ERROR, "%s: failed to move %s to %s with error %d\n", FUNCTION_NAME,
                                iter->m_name.c_str(), completedLogFilePath.string().c_str(), errcode.value());
                            continue;
                        }
                    }
                }
                catch (const boost::filesystem::filesystem_error &exc)
                {
                    // Log exception and continue
                    DebugPrintf(EvCF_LOG_ERROR, "%s: cuaght exception %s in moving %s to %s.\n", FUNCTION_NAME,
                        exc.what(), iter->m_name.c_str(), completedLogFilePath.string().c_str());
                    continue;
                }
                GENERIC_CATCH_LOG_ACTION("cuaght exception in moving IR log file to completed folder.", continue);
            }

            // List _completed*.log in completed folder to update m_filesToProcessSet
            // Note: We can fill m_filesToProcessSet while we move files in above loop.
            // But again we have to list files in completed folder as there can be previoue unprocessed log present - crash case
            folderPattern = completedLogDir + LOG_FORMAT;
            fileInfos.clear();
            if (!fileops.listFile(folderPattern, fileInfos, true))
            {
                DebugPrintf(SV_LOG_ERROR, "%s: listFile failed for %s with status: %s\n", FUNCTION_NAME, folderPattern.c_str(), fileops.status());
                return;
            }

            for (FileInfos_t::iterator iter(fileInfos.begin()); iter != fileInfos.end(); ++iter)
            {
                // Update set - an each item of m_filesToProcessSet represent a unique path per IR session
                m_filesToProcessSet.insert((boost::filesystem::path(iter->m_name).parent_path() / "dataprotection.log").string());
            }
        }
    }

    bool IRSourceTraceProcessor::GetCollForwPair(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested)
    {
        try
        {
            // Create CollForwPair per IR instance - per protected disk
            // List dataprotection_completed_*.log and create set of unique log files per protected source disk
            // known regex to list file
            //   <Agent log path>\vxlogs\resync\<device name>\<volume group ID>\C\Source Host ID\<device name>\dataprotection_completed_*.log
            // Windows:
            //   <Agent log path>\vxlogs\resync\{1619707344}\52\C\a9ce1755-6f28-44b9-9525-c044b2686f74\{1619707344}\dataprotection_completed_*.log
            // Linux:
            //   <Agent log path>/vxlogs/resync/dev/sda/44/C:\1106266f-b9e5-4b80-9dd9-c2f31a30161d\dev\sda/dataprotection_completed_*.log

            BOOST_ASSERT(m_pSerRunnFactory);

#ifdef SV_WINDOWS
            const std::string strIR_SRC_LOG_FORMAT = "*\\*\\*\\*-*-*-*-*\\*\\dataprotection_completed_*.log";
#else
            const std::string strIR_SRC_LOG_FORMAT = "*/*/*/*/dataprotection_completed_*.log";
#endif

            PICollForwPair sourceAgentLog; // Will enqueue nullptr, if the below pair creation fails.

            TRANSPORT_CONNECTION_SETTINGS transportSettings;

            if (m_pSerRunnFactory->GetTransportConnectionSettings(transportSettings))
            {
                boost::shared_ptr<CxTransport> cxTransport = boost::make_shared<CxTransport>(TRANSPORT_PROTOCOL_HTTP, transportSettings, true);

                // Prepare number of coll-forw pairs instances list - one per disk
                UpdateFilesToProcessSet(strIR_SRC_LOG_FORMAT);

                std::set<std::string>::const_iterator filesToProcessSetItr = m_filesToProcessSet.begin();
                for (filesToProcessSetItr; filesToProcessSetItr != m_filesToProcessSet.end(); filesToProcessSetItr++)
                {
                    try
                    {
                        RETURN_IF_CANCELLATION_REQUESTED;

                        boost::shared_ptr< AgentLoggerReader <IRSrcLoggerReaderBookmark> > reader = boost::make_shared< AgentLoggerReader <IRSrcLoggerReaderBookmark> >(*filesToProcessSetItr);
                        boost::shared_ptr<InMageLoggerCollector <IRSrcLoggerReaderBookmark> > coll = boost::make_shared<InMageLoggerCollector <IRSrcLoggerReaderBookmark> >(reader);

                        boost::shared_ptr < IConverter < std::string, std::string > > conv;
                        boost::shared_ptr< IRSourceLoggerToMdsLogConverter > logConv =
                            boost::make_shared< IRSourceLoggerToMdsLogConverter >(m_marsForwPutLogLevel, GetSubComponentName(*filesToProcessSetItr), *filesToProcessSetItr);

                        logConv->Initialize(m_pSerRunnFactory->GetFabricType(), m_pSerRunnFactory->GetBiosId(), m_pSerRunnFactory->GetHostId(), GetComponentId(), m_removeMapKeyName);
                        conv = logConv;

                        boost::shared_ptr < CxPsForwarder < IRSrcLoggerReaderBookmark > > forw =
                            boost::make_shared < CxPsForwarder < IRSrcLoggerReaderBookmark > >(cxTransport, m_destFilePrefix.string(), V2ASourceFileAttributes::DestFileExt);

                        const int MaxTotalParentFolderSize = MaxTotalLogsFolderSize;

                        forw->Initialize(
                            MaxPerFileSize, MaxTotalParentFolderSize, MaxTotalFilesCntInParent,
                            IncompleteFileCleanupTimeout, m_cxTransportMaxAttempts);

                        sourceAgentLog = CollForwPair<std::string, std::string, IRSrcLoggerReaderBookmark>::MakePair(coll, forw, conv);
                        collForwPairs.push_back(sourceAgentLog);
                    }
                    GENERIC_CATCH_LOG_ACTION("cuaght exception in creating IR source logger telemetry pairs.", continue);
                }
            }

            return true;
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_IGNORE("Creating IR source logger telemetry reader - CxPs Uploader");

        return false; // caught exception
    }

    bool IRRcmSourceTraceProcessor::GetCollForwPair(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested)
    {
        try
        {
            //
            // Create CollForwPair per IR RCM instance - per protected disk
            // List dataprotection_completed_*.log and create set of unique log files per protected source disk
            // known regex to list file
            //   <Agent log path>\vxlogs\resync\<device name>\dataprotection_completed_*.log
            // Windows:
            //   MBR disk: "<Agent log path>\vxlogs\completed\{1619707344}\dataprotection.log"
            //   GPT disk: "<Agent log path>\vxlogs\completed\{0BD692D5-D45B-4959-8D3F-6645F6CE5A8F}\dataprotection.log"
            // Linux:
            //             "<Agent log path>/vxlogs/completed/DataDisk0/dataprotection.log"
            //

            BOOST_ASSERT(m_pSerRunnFactory);

#ifdef SV_WINDOWS
            const std::string strIR_SRC_LOG_FORMAT = "*\\dataprotection_completed_*.log";
#else
            const std::string strIR_SRC_LOG_FORMAT = "*/dataprotection_completed_*.log";
#endif

            PICollForwPair sourceAgentLog; // Will enqueue nullptr, if the below pair creation fails.

            TRANSPORT_CONNECTION_SETTINGS transportSettings;

            if (m_pSerRunnFactory->GetTransportConnectionSettings(transportSettings))
            {
                boost::shared_ptr<CxTransport> cxTransport = boost::make_shared<CxTransport>(TRANSPORT_PROTOCOL_HTTP, transportSettings, true);

                // Prepare number of coll-forw pairs instances list - one per disk
                UpdateFilesToProcessSet(strIR_SRC_LOG_FORMAT);

                std::set<std::string>::const_iterator filesToProcessSetItr = m_filesToProcessSet.begin();
                for (filesToProcessSetItr; filesToProcessSetItr != m_filesToProcessSet.end(); filesToProcessSetItr++)
                {
                    try
                    {
                        RETURN_IF_CANCELLATION_REQUESTED;

                        boost::shared_ptr< AgentLoggerReader <IRRcmSrcLoggerReaderBookmark> > reader = boost::make_shared< AgentLoggerReader <IRRcmSrcLoggerReaderBookmark> >(*filesToProcessSetItr);
                        boost::shared_ptr<InMageLoggerCollector <IRRcmSrcLoggerReaderBookmark> > coll = boost::make_shared<InMageLoggerCollector <IRRcmSrcLoggerReaderBookmark> >(reader);

                        boost::shared_ptr < IConverter < std::string, std::string > > conv;
                        boost::shared_ptr< IRRcmSourceLoggerToMdsLogConverter > logConv =
                            boost::make_shared< IRRcmSourceLoggerToMdsLogConverter >(m_marsForwPutLogLevel, GetSubComponentName(*filesToProcessSetItr), *filesToProcessSetItr);

                        logConv->Initialize(m_pSerRunnFactory->GetFabricType(), m_pSerRunnFactory->GetBiosId(), m_pSerRunnFactory->GetHostId(), GetComponentId(), m_removeMapKeyName);
                        conv = logConv;

                        boost::shared_ptr < CxPsForwarder < IRRcmSrcLoggerReaderBookmark > > forw =
                            boost::make_shared < CxPsForwarder < IRRcmSrcLoggerReaderBookmark > >(cxTransport, m_destFilePrefix.string(), V2ASourceFileAttributes::DestFileExt, false);

                        const int MaxTotalParentFolderSize = MaxTotalLogsFolderSize;

                        forw->Initialize(
                            MaxPerFileSize, MaxTotalParentFolderSize, MaxTotalFilesCntInParent,
                            IncompleteFileCleanupTimeout, m_cxTransportMaxAttempts);

                        sourceAgentLog = CollForwPair<std::string, std::string, IRRcmSrcLoggerReaderBookmark>::MakePair(coll, forw, conv);
                        collForwPairs.push_back(sourceAgentLog);
                    }
                    GENERIC_CATCH_LOG_ACTION("cuaght exception in creating IR source logger telemetry pairs.", continue);
                }
            }

            return true;
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_IGNORE("Creating IR RCM source logger telemetry reader - CxPs Uploader");

        return false; // caught exception
    }

    bool IRRcmSourceOnAzureTraceProcessor::GetCollForwPair(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested)
    {
        try
        {
            //
            // Create CollForwPair per IR RCM instance - per protected disk
            // List dataprotection_completed_*.log and create set of unique log files per protected source disk
            // known regex to list file
            //   <Agent log path>\vxlogs\resync\<device name>\dataprotection_completed_*.log
            // Windows:
            //   MBR disk: "<Agent log path>\vxlogs\completed\{1619707344}\dataprotection.log"
            //   GPT disk: "<Agent log path>\vxlogs\completed\{0BD692D5-D45B-4959-8D3F-6645F6CE5A8F}\dataprotection.log"
            // Linux:
            //             "<Agent log path>/vxlogs/completed/DataDisk0/dataprotection.log"
            //

            BOOST_ASSERT(m_pSerRunnFactory);

            const unsigned int maxFileSize = lc->getLogMaxFileSize();
            const unsigned int maxFileCount = lc->getLogMaxCompletedFiles();
            boost::filesystem::path telemetryLogsPath(lc->getLogPathname());
            telemetryLogsPath /= "ArchivedLogs";

#ifdef SV_WINDOWS
            const std::string strIR_SRC_LOG_FORMAT = "*\\dataprotection_completed_*.log";
#else
            const std::string strIR_SRC_LOG_FORMAT = "*/dataprotection_completed_*.log";
#endif

            PICollForwPair sourceAgentLog; // Will enqueue nullptr, if the below pair creation fails.

            // Prepare number of coll-forw pairs instances list - one per disk
            UpdateFilesToProcessSet(strIR_SRC_LOG_FORMAT);

            std::set<std::string>::const_iterator filesToProcessSetItr = m_filesToProcessSet.begin();
            for (filesToProcessSetItr; filesToProcessSetItr != m_filesToProcessSet.end(); filesToProcessSetItr++)
            {
                try
                {
                    RETURN_IF_CANCELLATION_REQUESTED;

                    boost::shared_ptr< AgentLoggerReader <IRRcmSrcLoggerReaderBookmark> > reader = boost::make_shared< AgentLoggerReader <IRRcmSrcLoggerReaderBookmark> >(*filesToProcessSetItr);
                    boost::shared_ptr<InMageLoggerCollector <IRRcmSrcLoggerReaderBookmark> > coll = boost::make_shared<InMageLoggerCollector <IRRcmSrcLoggerReaderBookmark> >(reader);

                    boost::shared_ptr < IConverter < std::string, std::string > > conv;
                    boost::shared_ptr< IRRcmSourceLoggerToMdsLogConverter > logConv =
                        boost::make_shared< IRRcmSourceLoggerToMdsLogConverter >(m_marsForwPutLogLevel, GetSubComponentName(*filesToProcessSetItr), *filesToProcessSetItr);

                    logConv->Initialize(m_pSerRunnFactory->GetFabricType(), m_pSerRunnFactory->GetBiosId(), m_pSerRunnFactory->GetHostId(), GetComponentId(), m_removeMapKeyName);
                    conv = logConv;

                    boost::shared_ptr<ForwarderBase<std::string, IRRcmSrcLoggerReaderBookmark> > forw;
                    if (gbPrivateEndpointEnabled)
                        forw = boost::make_shared<PersistAndPruneForwarder<std::string, IRRcmSrcLoggerReaderBookmark> >(telemetryLogsPath.string(), A2ATableNames::AdminLogs, maxFileSize, maxFileCount);
                    else
                        forw = boost::make_shared<EventHubForwarderEx<IRRcmSrcLoggerReaderBookmark> >(V2AMdsTableNames::AdminLogs);

                    sourceAgentLog = CollForwPair<std::string, std::string, IRRcmSrcLoggerReaderBookmark>::MakePair(coll, forw, conv);
                    collForwPairs.push_back(sourceAgentLog);
                }
                GENERIC_CATCH_LOG_ACTION("cuaght exception in creating IR source logger telemetry pairs.", continue);
            }

            return true;
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_IGNORE("Creating IR RCM source logger telemetry reader - CxPs Uploader");

        return false; // caught exception
    }

    bool IRMTTraceProcessor::GetCollForwPair(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested)
    {
        try
        {
            // Create CollForwPair per IR MT instance
            // List dataprotection_completed_*.log and create set of unique log files per protected source disk
            //   <Agent log path>\vxlogs\resync\C\Source Host ID\<device name>\dataprotection_completed_*.log
            // Log path for Windows source:
            //   <Agent log path>\vxlogs\resync\C\a9ce1755-6f28-44b9-9525-c044b2686f74\{1619707344}\dataprotection_completed_*.log
            // Log path for Linux source:
            //   <Agent log path>\vxlogs\resync\C\1106266f-b9e5-4b80-9dd9-c2f31a30161d\dev\sda\dataprotection_completed_*.log

            BOOST_ASSERT(m_pSerRunnFactory);

            const std::string strIR_MT_LOG_FORMAT_Win = "*\\*-*-*-*-*\\*\\dataprotection_completed_*.log";
            const std::string strIR_MT_LOG_FORMAT_Linux = "*\\*-*-*-*-*\\*\\*\\dataprotection_completed_*.log";

            PICollForwPair sourceAgentLog; // Will enqueue nullptr, if the below pair creation fails.

            boost::shared_ptr<CxTransport> cxTransport = boost::make_shared<CxTransport>(TRANSPORT_PROTOCOL_FILE, TRANSPORT_CONNECTION_SETTINGS(), false);

            // Prepare number of coll-forw pairs instances list - one per disk
            UpdateFilesToProcessSet(strIR_MT_LOG_FORMAT_Win);
            UpdateFilesToProcessSet(strIR_MT_LOG_FORMAT_Linux);

            std::set<std::string>::const_iterator filesToProcessSetItr = m_filesToProcessSet.begin();
            for (filesToProcessSetItr; filesToProcessSetItr != m_filesToProcessSet.end(); filesToProcessSetItr++)
            {
                try
                {
                    RETURN_IF_CANCELLATION_REQUESTED;

                    boost::shared_ptr< AgentLoggerReader <IRMTLoggerReaderBookmark> > reader = boost::make_shared< AgentLoggerReader <IRMTLoggerReaderBookmark> >(*filesToProcessSetItr);
                    boost::shared_ptr<InMageLoggerCollector <IRMTLoggerReaderBookmark> > coll = boost::make_shared<InMageLoggerCollector <IRMTLoggerReaderBookmark> >(reader);

                    boost::shared_ptr < IConverter < std::string, std::string > > conv;
                    boost::shared_ptr< IRMTLoggerToMdsLogConverter > logConv =
                        boost::make_shared< IRMTLoggerToMdsLogConverter >(m_marsForwPutLogLevel, GetSubComponentName(*filesToProcessSetItr), *filesToProcessSetItr);

                    logConv->Initialize(m_pSerRunnFactory->GetFabricType(), m_pSerRunnFactory->GetBiosId(), m_pSerRunnFactory->GetHostId(), GetComponentId(), m_removeMapKeyName);
                    conv = logConv;

                    boost::shared_ptr < CxPsForwarder < IRMTLoggerReaderBookmark > > forw =
                        boost::make_shared < CxPsForwarder < IRMTLoggerReaderBookmark > >(cxTransport, m_destFilePrefix.string(), V2ASourceFileAttributes::DestFileExt);

                    const int MaxTotalParentFolderSize = MaxTotalLogsFolderSize;

                    forw->Initialize(
                        MaxPerFileSize, MaxTotalParentFolderSize, MaxTotalFilesCntInParent,
                        IncompleteFileCleanupTimeout, m_cxTransportMaxAttempts);

                    sourceAgentLog = CollForwPair<std::string, std::string, IRMTLoggerReaderBookmark>::MakePair(coll, forw, conv);
                    collForwPairs.push_back(sourceAgentLog);
                }
                GENERIC_CATCH_LOG_ACTION("cuaght exception in creating IR MT logger telemetry pairs.", continue);
            }

            return true;
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_IGNORE("Creating IR MT logger telemetry reader - CxPs Uploader");

        return false; // caught exception
    }

    bool IRRcmMTTraceProcessor::GetCollForwPair(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested)
    {
        try
        {
            //
            // Create CollForwPair per IR Rcm MT instance
            // List dataprotection_completed_*.log and create set of unique log files per protected source disk
            //   <Agent log path>\vxlogs\resync\Source Host ID\<device name>\dataprotection_completed_*.log
            // Log path for Windows source:
            //   <Agent log path>\vxlogs\resync\a9ce1755-6f28-44b9-9525-c044b2686f74\{1619707344}\dataprotection_completed_*.log
            // Log path for Linux source:
            //   <Agent log path>\vxlogs\resync\1106266f-b9e5-4b80-9dd9-c2f31a30161d\dev\sda\dataprotection_completed_*.log
            //

            BOOST_ASSERT(m_pSerRunnFactory);

            const std::string strIR_MT_LOG_FORMAT = "*-*-*-*-*\\*\\dataprotection_completed_*.log";

            PICollForwPair sourceAgentLog; // Will enqueue nullptr, if the below pair creation fails.

            boost::shared_ptr<CxTransport> cxTransport = boost::make_shared<CxTransport>(TRANSPORT_PROTOCOL_FILE, TRANSPORT_CONNECTION_SETTINGS(), false);

            // Prepare number of coll-forw pairs instances list - one per disk
            UpdateFilesToProcessSet(strIR_MT_LOG_FORMAT);

            std::set<std::string>::const_iterator filesToProcessSetItr = m_filesToProcessSet.begin();
            for (filesToProcessSetItr; filesToProcessSetItr != m_filesToProcessSet.end(); filesToProcessSetItr++)
            {
                try
                {
                    RETURN_IF_CANCELLATION_REQUESTED;

                    boost::shared_ptr< AgentLoggerReader <IRRcmMTLoggerReaderBookmark> > reader = boost::make_shared< AgentLoggerReader <IRRcmMTLoggerReaderBookmark> >(*filesToProcessSetItr);
                    boost::shared_ptr<InMageLoggerCollector <IRRcmMTLoggerReaderBookmark> > coll = boost::make_shared<InMageLoggerCollector <IRRcmMTLoggerReaderBookmark> >(reader);

                    boost::shared_ptr < IConverter < std::string, std::string > > conv;
                    boost::shared_ptr< IRRcmMTLoggerToMdsLogConverter > logConv =
                        boost::make_shared< IRRcmMTLoggerToMdsLogConverter >(m_marsForwPutLogLevel, GetSubComponentName(*filesToProcessSetItr), *filesToProcessSetItr);

                    logConv->Initialize(m_pSerRunnFactory->GetFabricType(), m_pSerRunnFactory->GetBiosId(), m_pSerRunnFactory->GetHostId(), GetComponentId(), m_removeMapKeyName);
                    conv = logConv;

                    boost::shared_ptr < CxPsForwarder < IRRcmMTLoggerReaderBookmark > > forw =
                        boost::make_shared < CxPsForwarder < IRRcmMTLoggerReaderBookmark > >(cxTransport, m_destFilePrefix.string(), V2ASourceFileAttributes::DestFileExt, false);

                    const int MaxTotalParentFolderSize = MaxTotalLogsFolderSize;

                    forw->Initialize(
                        MaxPerFileSize, MaxTotalParentFolderSize, MaxTotalFilesCntInParent,
                        IncompleteFileCleanupTimeout, m_cxTransportMaxAttempts);

                    sourceAgentLog = CollForwPair<std::string, std::string, IRRcmMTLoggerReaderBookmark>::MakePair(coll, forw, conv);
                    collForwPairs.push_back(sourceAgentLog);
                }
                GENERIC_CATCH_LOG_ACTION("cuaght exception in creating IR MT logger telemetry pairs.", continue);
            }

            return true;
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_IGNORE("Creating IR RCM target logger telemetry reader - CxPs Uploader");

        return false; // caught exception
    }

    void DRMTTraceProcessor::UpdateFilesToProcessSet(const std::string& LOG_FORMAT)
    {
        std::set<std::string> filesToProcessSet;
        std::string folderPattern(Utils::GetDPDiffsyncLogFolderPath() + LOG_FORMAT);
        FileInfos_t fileInfos;
        CxTransport fileops(TRANSPORT_PROTOCOL_FILE, TRANSPORT_CONNECTION_SETTINGS(), false);
        if (!fileops.listFile(folderPattern, fileInfos, true))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: GetFileList failed with status: %s\n", FUNCTION_NAME, fileops.status());
            return;
        }
        for (FileInfos_t::iterator iter(fileInfos.begin()); (iter != fileInfos.end()) && (1/*!QuitRequested(0)*/); ++iter)
        {
            m_filesToProcessSet.insert((boost::filesystem::path(iter->m_name).parent_path() / "dataprotection.log").string());
        }
    }

    bool DRMTTraceProcessor::GetCollForwPair(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested)
    {
        try
        {
            // Create CollForwPair per DR instance - per protected disk
            // List dataprotection_completed_*.log and create set of unique log files per protected source disk
            // known regex to list file
            //   <Agent log path>\vxlogs\diffsync\<volume group ID>\dataprotection_completed_*.log
            //   <Agent log path>\vxlogs\diffsync\48\dataprotection_completed_*.log

            BOOST_ASSERT(m_pSerRunnFactory);

            const std::string strDR_MT_LOG_FORMAT = "*\\dataprotection_completed_*.log";

            PICollForwPair sourceAgentLog; // Will enqueue nullptr, if the below pair creation fails.

            boost::shared_ptr<CxTransport> cxTransport = boost::make_shared<CxTransport>(TRANSPORT_PROTOCOL_FILE, TRANSPORT_CONNECTION_SETTINGS(), false);

            // Prepare number of coll-forw pairs instances list - one per disk
            UpdateFilesToProcessSet(strDR_MT_LOG_FORMAT);

            std::set<std::string>::const_iterator filesToProcessSetItr = m_filesToProcessSet.begin();
            for (filesToProcessSetItr; filesToProcessSetItr != m_filesToProcessSet.end(); filesToProcessSetItr++)
            {
                try
                {
                    RETURN_IF_CANCELLATION_REQUESTED;

                    boost::shared_ptr< AgentLoggerReader <DRMTLoggerReaderBookmark> > reader = boost::make_shared< AgentLoggerReader <DRMTLoggerReaderBookmark> >(*filesToProcessSetItr);
                    boost::shared_ptr<InMageLoggerCollector <DRMTLoggerReaderBookmark> > coll = boost::make_shared<InMageLoggerCollector <DRMTLoggerReaderBookmark> >(reader);

                    boost::shared_ptr < IConverter < std::string, std::string > > conv;
                    boost::shared_ptr< DRMTLoggerToMdsLogConverter > logConv =
                        boost::make_shared< DRMTLoggerToMdsLogConverter >(m_marsForwPutLogLevel, GetSubComponentName(*filesToProcessSetItr), *filesToProcessSetItr);

                    logConv->Initialize(m_pSerRunnFactory->GetFabricType(), m_pSerRunnFactory->GetBiosId(), m_pSerRunnFactory->GetHostId(), GetComponentId(), m_removeMapKeyName);
                    conv = logConv;

                    boost::shared_ptr < CxPsForwarder < DRMTLoggerReaderBookmark > > forw =
                        boost::make_shared < CxPsForwarder < DRMTLoggerReaderBookmark > >(cxTransport, m_destFilePrefix.string(), V2ASourceFileAttributes::DestFileExt);

                    const int MaxTotalParentFolderSize = MaxTotalLogsFolderSize;

                    forw->Initialize(
                        MaxPerFileSize, MaxTotalParentFolderSize, MaxTotalFilesCntInParent,
                        IncompleteFileCleanupTimeout, m_cxTransportMaxAttempts);

                    sourceAgentLog = CollForwPair<std::string, std::string, DRMTLoggerReaderBookmark>::MakePair(coll, forw, conv);
                    collForwPairs.push_back(sourceAgentLog);
                }
                GENERIC_CATCH_LOG_ACTION("cuaght exception in creating DR MT logger telemetry pairs.", continue);
            }

            return true;
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_IGNORE("Creating DR MT logger telemetry reader - CxPs Uploader");

        return false; // caught exception
    }

    void MTTraceProcessor::UpdateFilesToProcessSet(const std::string& LOG_FORMAT)
    {
        const boost::filesystem::path agentLogPath(lc->getLogPathname());

        m_filesToProcessSet.insert((agentLogPath / "svagents.log").string());
        m_filesToProcessSet.insert((agentLogPath / "CacheMgr.log").string());
        m_filesToProcessSet.insert((agentLogPath / "appservice.log").string());
        // m_filesToProcessSet.insert((agentLogPath / "CDPMgr.log").string()); // TODO: nichougu-1802
        // m_filesToProcessSet.insert((agentLogPath / "cdpcli.log").string()); // TODO: nichougu-1802
    }

    bool V2ATraceProcessor::GetCollForwPair(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested)
    {
        try
        {
            // Create pairs for each of the agent binary logs.

            BOOST_ASSERT(m_pSerRunnFactory);

            PICollForwPair sourceAgentLog; // Will enqueue nullptr, if the below pair creation fails.

            boost::shared_ptr<CxTransport> cxTransport = boost::make_shared<CxTransport>(TRANSPORT_PROTOCOL_FILE, TRANSPORT_CONNECTION_SETTINGS(), false);

            // Prepare number of coll-forw pairs instances list - one per disk
            UpdateFilesToProcessSet();

            std::set<std::string>::const_iterator filesToProcessSetItr = m_filesToProcessSet.begin();
            for (filesToProcessSetItr; filesToProcessSetItr != m_filesToProcessSet.end(); filesToProcessSetItr++)
            {
                try
                {
                    RETURN_IF_CANCELLATION_REQUESTED;

                    boost::shared_ptr< AgentLoggerReader <LoggerReaderBookmark> > reader = boost::make_shared< AgentLoggerReader <LoggerReaderBookmark> >(*filesToProcessSetItr);
                    boost::shared_ptr<InMageLoggerCollector <LoggerReaderBookmark> > coll = boost::make_shared<InMageLoggerCollector <LoggerReaderBookmark> >(reader);

                    boost::shared_ptr < IConverter < std::string, std::string > > conv;
                    boost::shared_ptr< InMageLoggerToMdsLogConverter > logConv =
                        boost::make_shared< InMageLoggerToMdsLogConverter >(m_marsForwPutLogLevel, GetSubComponentName(*filesToProcessSetItr));

                    logConv->Initialize(m_pSerRunnFactory->GetFabricType(), m_pSerRunnFactory->GetBiosId(), m_pSerRunnFactory->GetHostId(), GetComponentId(), m_removeMapKeyName);
                    conv = logConv;

                    boost::shared_ptr < CxPsForwarder < LoggerReaderBookmark > > forw =
                        boost::make_shared < CxPsForwarder < LoggerReaderBookmark > >(cxTransport, m_destFilePrefix.string(), V2ASourceFileAttributes::DestFileExt);

                    const int MaxTotalParentFolderSize = MaxTotalLogsFolderSize;

                    forw->Initialize(
                        MaxPerFileSize, MaxTotalParentFolderSize, MaxTotalFilesCntInParent,
                        IncompleteFileCleanupTimeout, m_cxTransportMaxAttempts);

                    sourceAgentLog = CollForwPair<std::string, std::string, LoggerReaderBookmark>::MakePair(coll, forw, conv);
                    collForwPairs.push_back(sourceAgentLog);
                }
                GENERIC_CATCH_LOG_ACTION("cuaght exception in creating logger telemetry pairs.", continue);
            }

            return true;
        }
        CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
            GENERIC_CATCH_LOG_IGNORE("Creating logger telemetry reader - CxPs Uploader");

        return false; // caught exception
    }

}