#include <boost/make_shared.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/core/noncopyable.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "cdputil.h"
#include "common/Trace.h"
#include "configurator2.h"
#include "RcmConfigurator.h"
#include "localconfigurator.h"
#include "HandlerCurl.h"
#include "MonitoringMsgSettings.h"
#include "resthelper/HttpClient.h"
#include "resthelper/ServiceBusQueue.h"

#include "EvtCollForwCommon.h"
#include "RunnerFactories.h"
#include "TraceProcessor.h"
#include "cxps.h"
#include "pssettingsconfigurator.h"

extern boost::shared_ptr<LocalConfigurator> lc;
extern bool gbPrivateEndpointEnabled;

#define RETURN_IF_CANCELLATION_REQUESTED \
    if (cancelRequested && cancelRequested())  \
    { \
        return; \
    }

#ifdef SV_WINDOWS

// TODO-SanKumar-1806: Create the query strings "once" as part of environment
// intialization at the start of the process.

// TODO-SanKumar-1702: Bookmark isn't supported for Analytic or Debug. We should've time-based
// bookmarking for it. Until then, we shouldn't be using those channels, as it would cause failures.
static void GetSourceAdminLogsQuery(std::string &queryName, std::wstring &query)
{
    // CRITICAL NOTE: The query name shouldn't be modified across updates, as
    // this is used to create the bookmark file for this query.
    queryName = "InMageAdminLog";

    query =
        L"<QueryList> \
            <Query Id=\"0\"> \
                <Select Path=\"Application\"> \
                    *[System[Provider[ \
                        @Name='AzureSiteRecoveryVssProvider' or \
                        @Name='Chkdsk' or \
                        @Name='Microsoft-Windows-Defrag' or \
                        @Name='SQLWriter' \ or \
                        @Name='VSS' \
                    ]]] \
                </Select> \
                <Select Path=\"Microsoft-InMage-InDiskFlt/Admin\"> \
                    *[System[Provider[@Name='Microsoft-InMage-InDiskFlt']]] \
                </Select> \
                <Select Path=\"Microsoft-InMage-InDiskFlt/Operational\"> \
                    *[System[Provider[@Name='Microsoft-InMage-InDiskFlt']]] \
                </Select> \
                <Select Path=\"Microsoft-Windows-Kernel-Boot/Operational\"> \
                    *[System[Provider[@Name='Microsoft-Windows-Kernel-Boot']]] \
                </Select> \
                <Select Path=\"Microsoft-Windows-Kernel-Power/Thermal-Operational\"> \
                    *[System[Provider[@Name='Microsoft-Windows-Kernel-Power']]] \
                </Select> \
                <Select Path=\"Microsoft-Windows-Storage-Disk/Admin\"> \
                    *[System[Provider[@Name='Microsoft-Windows-Disk']]] \
                </Select> \
                <Select Path=\"Microsoft-Windows-Storage-Disk/Operational\"> \
                    *[System[Provider[@Name='Microsoft-Windows-Disk']]] \
                </Select> \
                <Select Path=\"Microsoft-Windows-BitLocker-DrivePreparationTool/Admin\"> \
                    *[System[Provider[@Name='Microsoft-Windows-BitLocker-DrivePreparationTool']]] \
                </Select> \
                <Select Path=\"Microsoft-Windows-BitLocker-DrivePreparationTool/Operational\"> \
                    *[System[Provider[@Name='Microsoft-Windows-BitLocker-DrivePreparationTool']]] \
                </Select> \
                <Select Path=\"Microsoft-Windows-BitLocker/BitLocker Management\"> \
                    *[System[Provider[@Name='Microsoft-Windows-BitLocker-API']]] \
                </Select> \
                <Select Path=\"System\"> \
                    ( \
                        *[System[Provider[ \
                            @Name='BugCheck' or \
                            @Name='InDskFlt' or \
                            @Name='eventlog' or \
                            @Name='User32' or \
                            @Name='Ntfs' or \
                            @Name='Microsoft-Windows-Wininit' or \
                            @Name='volmgr' or \
                            @Name='Microsoft-Windows-WER-SystemErrorReporting' or \
                            @Name='Microsoft-Windows-Kernel-Boot' or \
                            @Name='Microsoft-Windows-Kernel-Power' or \
                            @Name='vhdmp' or \
                            @Name='iScsiPrt' or \
                            @Name='partmgr' or \
                            @Name='storflt' or \
                            @Name='Volsnap' or \
                            @Name='Mpio' or \
                            @Name='Microsoft-Windows-BitLocker-Driver' \
                        ]]] \
                    ) \
                    or \
                    ( \
                        *[System[Provider[@Name='Service Control Manager']]] \
                        and \
                        *[EventData[Data[@Name = 'param1'] \
                            and \
                            ( \
                            Data='InMage Scout Application Service' or \
                            Data='InMage Scout FX Agent' or \
                            Data='InMage Scout VX Agent - Sentinel/Outpost' or \
                            Data='Azure Site Recovery VSS Provider' \
                            ) \
                        ]] \
                    ) \
                </Select> \
            </Query> \
          </QueryList>";

    // TODO-SanKumar-1708: InDiskFlt and other Operational logs should go into the corresponding query for Operational MDS events.
    // <!--<Select Path=\"Microsoft-InMage-InDiskFlt/Operational\">*[System[Provider[@Name='Microsoft-InMage-InDiskFlt']]]</Select>-->

    // <!--<Select Path = "Microsoft-Windows-Kernel-PnP/Configuration">*[System[Provider[@Name = 'Microsoft-Windows-Kernel-PnP']]]< / Select>-->
    // <!--<Select Path = "Microsoft-Windows-Kernel-PnPConfig/Configuration">*[System[Provider[@Name = 'Microsoft-Windows-Kernel-PnPConfig']]]< / Select>-->
    // L"<Select Path=\"System\">*[System[Provider[@Name = 'Microsoft-Windows-Kernel-General' or @Name = 'Microsoft-Windows-Kernel-PnP' or-->

    // Note-SanKumar-1708: Removed "or @Name='disk'" from System channel, as the source seemed to
    // be too spurious in A2A private preview VMs.
}

static void GetInDskFltTelemetryQuery(std::string &queryName, std::wstring &query)
{
    // The query name shouldn't be modified across updates, as this is used to
    // create the bookmark file for this query.
    queryName = "InDskFltTelemetry";

    query =
        L"<QueryList> \
            <Query Id=\"0\" Path=\"Microsoft-InMage-InDiskFlt/telemetryOperational\"> \
                <Select Path=\"Microsoft-InMage-InDiskFlt/telemetryOperational\">*</Select> \
            </Query> \
          </QueryList>";
}

static void GetPSAdminLogsQuery(std::string &queryName, std::wstring &query)
{
    // CRITICAL NOTE: The query name shouldn't be modified across updates, as
    // this is used to create the bookmark file for this query.
    queryName = "InMagePSAdminLog";

    query =
        L"<QueryList> \
            <Query Id=\"0\"> \
                <Select Path=\"Application\"> \
                    *[System[Provider[ \
                        @Name='Chkdsk' or \
                        @Name='Microsoft-Windows-Defrag' \
                    ]]] \
                </Select> \
                <Select Path=\"Microsoft-Windows-Kernel-Boot/Operational\"> \
                    *[System[Provider[@Name='Microsoft-Windows-Kernel-Boot']]] \
                </Select> \
                <Select Path=\"Microsoft-Windows-Kernel-Power/Thermal-Operational\"> \
                    *[System[Provider[@Name='Microsoft-Windows-Kernel-Power']]] \
                </Select> \
                <Select Path=\"Microsoft-Windows-Storage-Disk/Admin\"> \
                    *[System[Provider[@Name='Microsoft-Windows-Disk']]] \
                </Select> \
                <Select Path=\"Microsoft-Windows-Storage-Disk/Operational\"> \
                    *[System[Provider[@Name='Microsoft-Windows-Disk']]] \
                </Select> \
                <Select Path=\"Microsoft-Windows-BitLocker-DrivePreparationTool/Admin\"> \
                    *[System[Provider[@Name='Microsoft-Windows-BitLocker-DrivePreparationTool']]] \
                </Select> \
                <Select Path=\"Microsoft-Windows-BitLocker-DrivePreparationTool/Operational\"> \
                    *[System[Provider[@Name='Microsoft-Windows-BitLocker-DrivePreparationTool']]] \
                </Select> \
                <Select Path=\"Microsoft-Windows-BitLocker/BitLocker Management\"> \
                    *[System[Provider[@Name='Microsoft-Windows-BitLocker-API']]] \
                </Select> \
                <Select Path=\"System\"> \
                    ( \
                        *[System[Provider[ \
                            @Name='BugCheck' or \
                            @Name='eventlog' or \
                            @Name='User32' or \
                            @Name='Ntfs' or \
                            @Name='volmgr' or \
                            @Name='Microsoft-Windows-WER-SystemErrorReporting' or \
                            @Name='Microsoft-Windows-Kernel-Boot' or \
                            @Name='Microsoft-Windows-Kernel-Power' or \
                            @Name='iScsiPrt' or \
                            @Name='partmgr' or \
                            @Name='storflt' or \
                            @Name='Volsnap' or \
                            @Name='Mpio' or \
                            @Name='Microsoft-Windows-BitLocker-Driver' \
                        ]]] \
                    ) \
                    or \
                    ( \
                        *[System[Provider[@Name='Service Control Manager']]] \
                        and \
                        *[EventData[Data[@Name = 'param1'] \
                            and \
                            ( \
                            Data='InMage PushInstall' or \
                            Data='InMage Scout Application Service' or \
                            Data='InMage Scout FX Agent' or \
                            Data='InMage Scout VX Agent - Sentinel/Outpost' or \
                            Data='cxprocessserver' or \
                            Data='tmansvc' or \
                            Data='Microsoft Azure Recovery Services Agent' or \
                            Data='Microsoft Azure Recovery Services Management Agent' or \
                            Data='Microsoft Azure Site Recovery Service' or \
                            Data='Process Server' or \
                            Data='Process Server Monitor' \
                            ) \
                        ]] \
                    ) \
                </Select> \
            </Query> \
        </QueryList>";
}

#endif // SV_WINDOWS

static void A2ALogCallback(unsigned int logLevel, const char *msg)
{
    DebugPrintf((SV_LOG_LEVEL)logLevel, "%s", msg);
}

static boost::shared_ptr<boost::thread> StartFileXferLog(const EvtCollForw::CmdLineSettings &settings)
{
    BOOST_ASSERT(lc);

    // TODO-SanKumar-1702: Read these values from Config.
    const std::string EvtCollForwXferLogFileName = "evtcollforw.xfer.log";
    const int MaxSize = 10 * 1024 * 1024;
    const int LogRetainSize = 0;
    const int RotateCount = 0;
    // TODO-SanKumar-1702: Should we use lc.getLogFileXfer() for this enable?
    const bool Enabled = true;
    boost::filesystem::path evtCollForwXferLogPath;
    if(boost::iequals(settings.Environment, EvtCollForw_Env_V2ARcmPS))
    {
        evtCollForwXferLogPath = lc->getPSInstallPathOnCsPrimeApplianceToAzure();
        evtCollForwXferLogPath /= EvtCollForw::PSComponentPaths::HOME;
        evtCollForwXferLogPath /= EvtCollForw::PSComponentPaths::SVSYSTEMS;
        evtCollForwXferLogPath /= EvtCollForw::PSComponentPaths::VAR;
    }
    else
    {
        evtCollForwXferLogPath = lc->getLogPathname();
    }
    evtCollForwXferLogPath /= EvtCollForwXferLogFileName;

    DebugPrintf(EvCF_LOG_DEBUG, "Initializing and starting evtcollforw xfer log monitor.\n");
    cxTransportSetupXferLogging(evtCollForwXferLogPath, MaxSize, RotateCount, LogRetainSize, Enabled);
    return cxTransportStartXferLogMonitor();
}

static void StopXferLog(boost::shared_ptr<boost::thread> xferMonitorThread)
{
    if (xferMonitorThread)
    {
        DebugPrintf(EvCF_LOG_DEBUG, "Waiting for evtcollforw xfer log monitor.\n");
        cxTransportStopXferLogMonitor();
        xferMonitorThread->join();
    }
}

namespace EvtCollForw
{
    boost::shared_ptr<IRunner> SerializedRunnerFactory::GetRunner(boost::function0<bool> cancelRequested)
    {
        std::vector<PICollForwPair> collForwPairs;
        GetCollForwPairs(collForwPairs, cancelRequested);

        if (cancelRequested && cancelRequested())
            return boost::shared_ptr<IRunner>();

        if (collForwPairs.empty())
        {
            BOOST_ASSERT(false);
            throw INMAGE_EX("Runner factory created no pairs!");
        }

        return boost::make_shared<SerializedRunner>(collForwPairs);
    }

    void V2ASourceRunnerFactoryBase::InitializeEnvironment(
        const CmdLineSettings &settings, std::vector < boost::shared_ptr < void > > &uninitializers)
    {
        {
            // Setup Xfer log

            boost::shared_ptr<boost::thread> xferMonitorThread = StartFileXferLog(settings);

            // TODO-SanKumar-1702: Create macro for uninit push back...
            uninitializers.insert(uninitializers.begin(), boost::shared_ptr<void>(
                static_cast<void*>(0), boost::bind(StopXferLog, xferMonitorThread)));
        }

        {
            tal::GlobalInitialize(); // for curl
            uninitializers.insert(uninitializers.begin(), boost::shared_ptr<void>(
                static_cast<void*>(0), boost::bind(tal::GlobalShutdown)));
        }
    }

    SerializedRunnerFactory::SerializedRunnerFactory(const CmdLineSettings &cmdLineSettings) :
        m_cmdLineSettings(cmdLineSettings)
    {
        BOOST_ASSERT(lc);
        m_hostId = lc->getHostId();

        Utils::TryGetFabricType(m_fabricType);

        Utils::TryGetBiosId(m_biosId);

        // TODO-SanKumar-1702: Should the CxPs forwarder per file size be some calculation like % of
        // max file size, etc.?
    }

    SerializedRunnerFactory::~SerializedRunnerFactory()
    {
    }

    void V2ASourceRunnerFactory::GetCollForwPairs(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested)
    {
        collForwPairs.clear();
        BOOST_ASSERT(lc);

        RETURN_IF_CANCELLATION_REQUESTED;

        // TODO-SanKumar-1703: Make the table names found immediately below and in few blocks below
        // as constants to avoid any mistakes.
        std::map<std::string, std::string> sourceAgentLogsMap;
        sourceAgentLogsMap["svagents.log"] = V2AMdsTableNames::AdminLogs;
        sourceAgentLogsMap["s2.log"] = V2AMdsTableNames::AdminLogs;
        sourceAgentLogsMap["appservice.log"] = V2AMdsTableNames::AdminLogs;
        sourceAgentLogsMap["srcTelemetry.log"] = V2AMdsTableNames::SourceTelemetry;
        sourceAgentLogsMap["tagTelemetry.log"] = V2AMdsTableNames::Vacp;
        sourceAgentLogsMap["schedulerTelemetry.log"] = V2AMdsTableNames::SchedulerTelemetry;

        // TODO-SanKumar-1702: Add these values to configuration.
        const int MaxPerFileSize = 1 * 1024 * 1024;
        const int MaxTotalTelemetryFolderSize = 10 * 1024 * 1024;
        const int MaxTotalLogsFolderSize = 20 * 1024 * 1024;
        const int MaxTotalFilesCntInParent = 8 * 3;
        const int IncompleteFileCleanupTimeout = 2 * 60 * 60; // 2 hours

        int MaxNumOfAttempts = lc->getEvtCollForwCxTransportMaxAttempts();

        // In V2A, MARS expects a JSON "with" the surrounding { Map : {Actual Data} }
        const bool RemoveMapKeyName = false;

        TRANSPORT_CONNECTION_SETTINGS transportSettings;

        // TODO-SanKumar-1702: Else, there's no protection enabled, so don't create the CxPs target pairs at all.
        // Although, returning empty set of pairs is perceived as fault by the runner as of today.
        // Educate that it's intentional, may be using a null pair.
        if (GetTransportConnectionSettings(transportSettings))
        {
            RETURN_IF_CANCELLATION_REQUESTED;

            const bool secure = true;
            boost::shared_ptr<CxTransport> cxTransport = boost::make_shared<CxTransport>(TRANSPORT_PROTOCOL_HTTP, transportSettings, secure);

            RETURN_IF_CANCELLATION_REQUESTED;

#ifdef SV_WINDOWS
            {
                PICollForwPair inDskfltEvtLog; // Will enqueue nullptr, if the below pair creation fails.
                try
                {
                    std::string queryName;
                    std::wstring queryStr;
                    GetSourceAdminLogsQuery(queryName, queryStr);

                    boost::shared_ptr<EventLogCollector> coll = boost::make_shared<EventLogCollector>(queryName, queryStr);

                    RETURN_IF_CANCELLATION_REQUESTED;

                    boost::shared_ptr<EventLogToMdsLogConverter> conv = boost::make_shared<EventLogToMdsLogConverter>();
                    conv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), ComponentId::SOURCE_AGENT, RemoveMapKeyName);

                    // TODO-SanKumar-1708: Move all such path buildings of source to init method.
                    boost::filesystem::path destFilePrefix(lc->getDestDir());
                    destFilePrefix /= GetHostId();
                    destFilePrefix /= V2ASourceFileAttributes::DestLogsFolder;
                    destFilePrefix /= V2AMdsTableNames::AdminLogs;

                    boost::shared_ptr<CxPsForwarder<boost::shared_ptr<EventLogRecord>>> forw =
                        boost::make_shared<CxPsForwarder<boost::shared_ptr<EventLogRecord>>>(
                            cxTransport, destFilePrefix.string(), V2ASourceFileAttributes::DestFileExt);

                    forw->Initialize(
                        MaxPerFileSize, MaxTotalLogsFolderSize, MaxTotalFilesCntInParent,
                        IncompleteFileCleanupTimeout, MaxNumOfAttempts);

                    inDskfltEvtLog = CollForwPair<boost::shared_ptr<EventLogRecord>, std::string, boost::shared_ptr<EventLogRecord>>::MakePair(coll, forw, conv);
                }
                CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                    GENERIC_CATCH_LOG_IGNORE("Creating indskflt driver eventlog - CxPs Uploader");

                collForwPairs.push_back(inDskfltEvtLog);
            }

            RETURN_IF_CANCELLATION_REQUESTED;

            {
                PICollForwPair inDskfltTelemetry; // Will enqueue nullptr, if the below pair creation fails.
                try
                {
                    std::string queryName;
                    std::wstring queryStr;
                    GetInDskFltTelemetryQuery(queryName, queryStr);

                    boost::shared_ptr<EventLogTelemetryCollector> coll =
                        boost::make_shared<EventLogTelemetryCollector>(queryName, queryStr);

                    RETURN_IF_CANCELLATION_REQUESTED;

                    boost::shared_ptr<EventLogTelemetryToMdsConverter> conv = boost::make_shared<EventLogTelemetryToMdsConverter>();
                    conv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), RemoveMapKeyName);

                    boost::filesystem::path destFilePrefix(lc->getDestDir());
                    destFilePrefix /= GetHostId();
                    destFilePrefix /= V2ASourceFileAttributes::DestTelemetryFolder;
                    destFilePrefix /= V2AMdsTableNames::DriverTelemetry;

                    boost::shared_ptr<CxPsForwarder<boost::shared_ptr<EventLogRecord>>> forw =
                        boost::make_shared<CxPsForwarder<boost::shared_ptr<EventLogRecord>>>(
                            cxTransport, destFilePrefix.string(), V2ASourceFileAttributes::DestFileExt);

                    forw->Initialize(
                        MaxPerFileSize, MaxTotalTelemetryFolderSize, MaxTotalFilesCntInParent,
                        IncompleteFileCleanupTimeout, MaxNumOfAttempts);

                    inDskfltTelemetry = CollForwPair<std::map<std::string, std::string>, std::string, boost::shared_ptr<EventLogRecord>>::MakePair(coll, forw, conv);
                }
                CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                    GENERIC_CATCH_LOG_IGNORE("Creating indskflt driver telemetry - CxPs Uploader");

                collForwPairs.push_back(inDskfltTelemetry);
            }
#endif // SV_WINDOWS

            RETURN_IF_CANCELLATION_REQUESTED;

#ifdef SV_UNIX
            {
                PICollForwPair inVolFltLinuxTelemetry; // Will enqueue nullptr, if the below pair creation fails.
                try
                {
                    // TODO-SanKumar-1704: Move the file name to Config.
                    const std::string inVolFltLinuxTelemetryPath =
                        (boost::filesystem::path(lc->getLogPathname()) / "involflt_telemetry.log").string();

                    boost::shared_ptr< InVolFltLinuxTelemetryLoggerReader < LoggerReaderBookmark > > inVolFltLinuxTelemetryReader =
                        boost::make_shared< InVolFltLinuxTelemetryLoggerReader < LoggerReaderBookmark > >(inVolFltLinuxTelemetryPath);

                    boost::shared_ptr< InMageLoggerCollector < LoggerReaderBookmark > > coll =
                        boost::make_shared< InMageLoggerCollector < LoggerReaderBookmark > >(inVolFltLinuxTelemetryReader);

                    RETURN_IF_CANCELLATION_REQUESTED;

                    boost::filesystem::path destFilePrefix(lc->getDestDir());
                    destFilePrefix /= GetHostId();
                    destFilePrefix /= V2ASourceFileAttributes::DestTelemetryFolder;
                    destFilePrefix /= V2AMdsTableNames::DriverTelemetry;

                    boost::shared_ptr<InVolFltLinuxTelemetryJsonMdsConverter> conv = boost::make_shared<InVolFltLinuxTelemetryJsonMdsConverter>();
                    conv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), RemoveMapKeyName);

                    boost::shared_ptr < CxPsForwarder < LoggerReaderBookmark > > forw =
                        boost::make_shared < CxPsForwarder < LoggerReaderBookmark > >(
                            cxTransport, destFilePrefix.string(), V2ASourceFileAttributes::DestFileExt);

                    forw->Initialize(
                        MaxPerFileSize, MaxTotalTelemetryFolderSize, MaxTotalFilesCntInParent,
                        IncompleteFileCleanupTimeout, MaxNumOfAttempts);

                    inVolFltLinuxTelemetry = CollForwPair<std::string, std::string, LoggerReaderBookmark>::MakePair(coll, forw, conv);
                }
                CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                    GENERIC_CATCH_LOG_IGNORE("Creating involflt linux driver telemetry - CxPs Uploader");

                collForwPairs.push_back(inVolFltLinuxTelemetry);
            }
#endif // SV_UNIX

            // Create pairs for each of the agent binary logs and telemetry tables.
            for (std::map<std::string, std::string>::const_iterator itr = sourceAgentLogsMap.begin(); itr != sourceAgentLogsMap.end(); itr++)
            {
                RETURN_IF_CANCELLATION_REQUESTED;

                // TODO-SanKumar-1702: Read CSV from configuration and initialize one pair per each table.
                PICollForwPair sourceAgentLog; // Will enqueue nullptr, if the below pair creation fails.
                try
                {
                    // TODO-SanKumar-1702: We qualify if the table name contains Telemetry as telemetry
                    // table. The dictionary should be extended to contain a detailed object that would
                    // say if it's a telemetry table or not and other factors right inside it.
                    bool isTelemetryTable = boost::icontains(itr->second, "Telemetry");

                    const std::string sourceLogPath = (boost::filesystem::path(lc->getLogPathname()) / itr->first).string();
                    boost::filesystem::path destFilePrefix(lc->getDestDir());
                    destFilePrefix /= GetHostId();
                    destFilePrefix /= isTelemetryTable ? V2ASourceFileAttributes::DestTelemetryFolder : V2ASourceFileAttributes::DestLogsFolder;
                    destFilePrefix /= itr->second; // MDS Table name - file prefix.

                    boost::shared_ptr< AgentLoggerReader < LoggerReaderBookmark > > reader = boost::make_shared< AgentLoggerReader <LoggerReaderBookmark> >(sourceLogPath);
                    boost::shared_ptr< InMageLoggerCollector < LoggerReaderBookmark > > coll = boost::make_shared<InMageLoggerCollector <LoggerReaderBookmark> >(reader);

                    RETURN_IF_CANCELLATION_REQUESTED;

                    const SV_LOG_LEVEL MarsForwPutLogLevel = isTelemetryTable ? SV_LOG_ALWAYS : lc->getEvtCollForwAgentLogPostLevel();
                    std::string subComponentName = boost::filesystem::path(itr->first).stem().string();

                    boost::shared_ptr < IConverter < std::string, std::string > > conv;
                    if (isTelemetryTable)
                    {
                        boost::shared_ptr< InMageLoggerTelemetryToMdsConverter > teleConv =
                            boost::make_shared< InMageLoggerTelemetryToMdsConverter >(MarsForwPutLogLevel);

                        teleConv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), RemoveMapKeyName);
                        conv = teleConv;
                    }
                    else
                    {
                        boost::shared_ptr< InMageLoggerToMdsLogConverter > logConv =
                            boost::make_shared< InMageLoggerToMdsLogConverter >(MarsForwPutLogLevel, subComponentName);

                        logConv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), ComponentId::SOURCE_AGENT, RemoveMapKeyName);
                        conv = logConv;
                    }

                    boost::shared_ptr < CxPsForwarder < LoggerReaderBookmark > > forw =
                        boost::make_shared < CxPsForwarder < LoggerReaderBookmark > >(cxTransport, destFilePrefix.string(), V2ASourceFileAttributes::DestFileExt);

                    const int MaxTotalParentFolderSize = isTelemetryTable ? MaxTotalTelemetryFolderSize : MaxTotalLogsFolderSize;

                    forw->Initialize(
                        MaxPerFileSize, MaxTotalParentFolderSize, MaxTotalFilesCntInParent,
                        IncompleteFileCleanupTimeout, MaxNumOfAttempts);

                    sourceAgentLog = CollForwPair<std::string, std::string, LoggerReaderBookmark>::MakePair(coll, forw, conv);
                }
                CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                    GENERIC_CATCH_LOG_IGNORE("Creating source logger telemetry reader - CxPs Uploader");

                collForwPairs.push_back(sourceAgentLog);
            }

            RETURN_IF_CANCELLATION_REQUESTED;

            {
                // Creating IR source logger telemetry reader - CxPs Uploader
                IRSourceTraceProcessor dpSrcTraceProcessor(this, m_cmdLineSettings);
                std::vector<PICollForwPair> dpSrcCollForwPairs;
                dpSrcTraceProcessor.GetCollForwPair(dpSrcCollForwPairs, cancelRequested);
                collForwPairs.insert(collForwPairs.end(), dpSrcCollForwPairs.begin(), dpSrcCollForwPairs.end());
            }
        }
    }

    void V2ARcmSourceRunnerFactory::GetCollForwPairs(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested)
    {
        collForwPairs.clear();
        BOOST_ASSERT(lc);

        RETURN_IF_CANCELLATION_REQUESTED;

        // TODO-SanKumar-1703: Make the table names found immediately below and in few blocks below
        // as constants to avoid any mistakes.
        std::map<std::string, std::string> sourceAgentLogsMap;
        sourceAgentLogsMap["svagents.log"] = V2AMdsTableNames::AdminLogs;
        sourceAgentLogsMap["s2.log"] = V2AMdsTableNames::AdminLogs;
        sourceAgentLogsMap["appservice.log"] = V2AMdsTableNames::AdminLogs;
        sourceAgentLogsMap["srcTelemetry.log"] = V2AMdsTableNames::SourceTelemetry;
        sourceAgentLogsMap["tagTelemetry.log"] = V2AMdsTableNames::Vacp;
        sourceAgentLogsMap["schedulerTelemetry.log"] = V2AMdsTableNames::SchedulerTelemetry;

        // TODO-SanKumar-1702: Add these values to configuration.
        const int MaxPerFileSize = 1 * 1024 * 1024;
        const int MaxTotalTelemetryFolderSize = 10 * 1024 * 1024;
        const int MaxTotalLogsFolderSize = 20 * 1024 * 1024;
        const int MaxTotalFilesCntInParent = 8 * 3;
        const int IncompleteFileCleanupTimeout = 2 * 60 * 60; // 2 hours

        int MaxNumOfAttempts = lc->getEvtCollForwCxTransportMaxAttempts();

        const bool RemoveMapKeyName = true;
        const bool isStreamData = false;

        TRANSPORT_CONNECTION_SETTINGS transportSettings;

        // TODO-SanKumar-1702: Else, there's no protection enabled, so don't create the CxPs target pairs at all.
        // Although, returning empty set of pairs is perceived as fault by the runner as of today.
        // Educate that it's intentional, may be using a null pair.
        if (GetTransportConnectionSettings(transportSettings))
        {
            RETURN_IF_CANCELLATION_REQUESTED;

            const bool secure = true;
            boost::shared_ptr<CxTransport> cxTransport = boost::make_shared<CxTransport>(TRANSPORT_PROTOCOL_HTTP, transportSettings, secure);

            RETURN_IF_CANCELLATION_REQUESTED;

#ifdef SV_WINDOWS
            {
                PICollForwPair inDskfltEvtLog; // Will enqueue nullptr, if the below pair creation fails.
                try
                {
                    std::string queryName;
                    std::wstring queryStr;
                    GetSourceAdminLogsQuery(queryName, queryStr);

                    boost::shared_ptr<EventLogCollector> coll = boost::make_shared<EventLogCollector>(queryName, queryStr);

                    RETURN_IF_CANCELLATION_REQUESTED;

                    boost::shared_ptr<EventLogToMdsLogConverter> conv = boost::make_shared<EventLogToMdsLogConverter>();
                    conv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), ComponentId::SOURCE_AGENT, RemoveMapKeyName);

                    // TODO-SanKumar-1708: Move all such path buildings of source to init method.
                    boost::filesystem::path destFilePrefix(RcmClientLib::RcmConfigurator::getInstance()->GetTelemetryFolderPathForOnPremToAzure());
                    destFilePrefix /= V2ASourceFileAttributes::DestLogsFolder;
                    destFilePrefix /= V2AMdsTableNames::AdminLogs;

                    boost::shared_ptr<CxPsForwarder<boost::shared_ptr<EventLogRecord>>> forw =
                        boost::make_shared<CxPsForwarder<boost::shared_ptr<EventLogRecord>>>(
                            cxTransport, destFilePrefix.string(), V2ASourceFileAttributes::DestFileExt, isStreamData);

                    forw->Initialize(
                        MaxPerFileSize, MaxTotalLogsFolderSize, MaxTotalFilesCntInParent,
                        IncompleteFileCleanupTimeout, MaxNumOfAttempts);

                    inDskfltEvtLog = CollForwPair<boost::shared_ptr<EventLogRecord>, std::string, boost::shared_ptr<EventLogRecord>>::MakePair(coll, forw, conv);
                }
                CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                    GENERIC_CATCH_LOG_IGNORE("Creating indskflt driver eventlog - CxPs Uploader");

                collForwPairs.push_back(inDskfltEvtLog);
            }

            RETURN_IF_CANCELLATION_REQUESTED;

            {
                PICollForwPair inDskfltTelemetry; // Will enqueue nullptr, if the below pair creation fails.
                try
                {
                    std::string queryName;
                    std::wstring queryStr;
                    GetInDskFltTelemetryQuery(queryName, queryStr);

                    boost::shared_ptr<EventLogTelemetryCollector> coll =
                        boost::make_shared<EventLogTelemetryCollector>(queryName, queryStr);

                    RETURN_IF_CANCELLATION_REQUESTED;

                    boost::shared_ptr<EventLogTelemetryToMdsConverter> conv = boost::make_shared<EventLogTelemetryToMdsConverter>();
                    conv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), RemoveMapKeyName);

                    boost::filesystem::path destFilePrefix(RcmClientLib::RcmConfigurator::getInstance()->GetTelemetryFolderPathForOnPremToAzure());
                    destFilePrefix /= V2ASourceFileAttributes::DestTelemetryFolder;
                    destFilePrefix /= V2AMdsTableNames::DriverTelemetry;

                    boost::shared_ptr<CxPsForwarder<boost::shared_ptr<EventLogRecord>>> forw =
                        boost::make_shared<CxPsForwarder<boost::shared_ptr<EventLogRecord>>>(
                            cxTransport, destFilePrefix.string(), V2ASourceFileAttributes::DestFileExt, isStreamData);

                    forw->Initialize(
                        MaxPerFileSize, MaxTotalTelemetryFolderSize, MaxTotalFilesCntInParent,
                        IncompleteFileCleanupTimeout, MaxNumOfAttempts);

                    inDskfltTelemetry = CollForwPair<std::map<std::string, std::string>, std::string, boost::shared_ptr<EventLogRecord>>::MakePair(coll, forw, conv);
                }
                CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                    GENERIC_CATCH_LOG_IGNORE("Creating indskflt driver telemetry - CxPs Uploader");

                collForwPairs.push_back(inDskfltTelemetry);
            }
#endif // SV_WINDOWS

            RETURN_IF_CANCELLATION_REQUESTED;

#ifdef SV_UNIX
            {
                PICollForwPair inVolFltLinuxTelemetry; // Will enqueue nullptr, if the below pair creation fails.
                try
                {
                    // TODO-SanKumar-1704: Move the file name to Config.
                    const std::string inVolFltLinuxTelemetryPath =
                        (boost::filesystem::path(lc->getLogPathname()) / "involflt_telemetry.log").string();

                    boost::shared_ptr< InVolFltLinuxTelemetryLoggerReader < LoggerReaderBookmark > > inVolFltLinuxTelemetryReader =
                        boost::make_shared< InVolFltLinuxTelemetryLoggerReader < LoggerReaderBookmark > >(inVolFltLinuxTelemetryPath);

                    boost::shared_ptr< InMageLoggerCollector < LoggerReaderBookmark > > coll =
                        boost::make_shared< InMageLoggerCollector < LoggerReaderBookmark > >(inVolFltLinuxTelemetryReader);

                    RETURN_IF_CANCELLATION_REQUESTED;

                    boost::filesystem::path destFilePrefix(RcmClientLib::RcmConfigurator::getInstance()->GetTelemetryFolderPathForOnPremToAzure());
                    destFilePrefix /= V2ASourceFileAttributes::DestTelemetryFolder;
                    destFilePrefix /= V2AMdsTableNames::DriverTelemetry;

                    boost::shared_ptr<InVolFltLinuxTelemetryJsonMdsConverter> conv = boost::make_shared<InVolFltLinuxTelemetryJsonMdsConverter>();
                    conv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), RemoveMapKeyName);

                    boost::shared_ptr < CxPsForwarder < LoggerReaderBookmark > > forw =
                        boost::make_shared < CxPsForwarder < LoggerReaderBookmark > >(
                            cxTransport, destFilePrefix.string(), V2ASourceFileAttributes::DestFileExt, isStreamData);

                    forw->Initialize(
                        MaxPerFileSize, MaxTotalTelemetryFolderSize, MaxTotalFilesCntInParent,
                        IncompleteFileCleanupTimeout, MaxNumOfAttempts);

                    inVolFltLinuxTelemetry = CollForwPair<std::string, std::string, LoggerReaderBookmark>::MakePair(coll, forw, conv);
                }
                CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                    GENERIC_CATCH_LOG_IGNORE("Creating involflt linux driver telemetry - CxPs Uploader");

                collForwPairs.push_back(inVolFltLinuxTelemetry);
            }
#endif // SV_UNIX

            // Create pairs for each of the agent binary logs and telemetry tables.
            for (std::map<std::string, std::string>::const_iterator itr = sourceAgentLogsMap.begin(); itr != sourceAgentLogsMap.end(); itr++)
            {
                RETURN_IF_CANCELLATION_REQUESTED;

                // TODO-SanKumar-1702: Read CSV from configuration and initialize one pair per each table.
                PICollForwPair sourceAgentLog; // Will enqueue nullptr, if the below pair creation fails.
                try
                {
                    // TODO-SanKumar-1702: We qualify if the table name contains Telemetry as telemetry
                    // table. The dictionary should be extended to contain a detailed object that would
                    // say if it's a telemetry table or not and other factors right inside it.
                    bool isTelemetryTable = boost::icontains(itr->second, "Telemetry");

                    const std::string sourceLogPath = (boost::filesystem::path(lc->getLogPathname()) / itr->first).string();
                    boost::filesystem::path destFilePrefix(RcmClientLib::RcmConfigurator::getInstance()->GetTelemetryFolderPathForOnPremToAzure());
                    destFilePrefix /= isTelemetryTable ? V2ASourceFileAttributes::DestTelemetryFolder : V2ASourceFileAttributes::DestLogsFolder;
                    destFilePrefix /= itr->second; // MDS Table name - file prefix.

                    boost::shared_ptr< AgentLoggerReader < LoggerReaderBookmark > > reader = boost::make_shared< AgentLoggerReader <LoggerReaderBookmark> >(sourceLogPath);
                    boost::shared_ptr< InMageLoggerCollector < LoggerReaderBookmark > > coll = boost::make_shared<InMageLoggerCollector <LoggerReaderBookmark> >(reader);

                    RETURN_IF_CANCELLATION_REQUESTED;

                    const SV_LOG_LEVEL MarsForwPutLogLevel = isTelemetryTable ? SV_LOG_ALWAYS : lc->getEvtCollForwAgentLogPostLevel();
                    std::string subComponentName = boost::filesystem::path(itr->first).stem().string();

                    boost::shared_ptr < IConverter < std::string, std::string > > conv;
                    if (isTelemetryTable)
                    {
                        boost::shared_ptr< InMageLoggerTelemetryToMdsConverter > teleConv =
                            boost::make_shared< InMageLoggerTelemetryToMdsConverter >(MarsForwPutLogLevel);

                        teleConv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), RemoveMapKeyName);
                        conv = teleConv;
                    }
                    else
                    {
                        boost::shared_ptr< InMageLoggerToMdsLogConverter > logConv =
                            boost::make_shared< InMageLoggerToMdsLogConverter >(MarsForwPutLogLevel, subComponentName);

                        logConv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), ComponentId::SOURCE_AGENT, RemoveMapKeyName);
                        conv = logConv;
                    }

                    boost::shared_ptr < CxPsForwarder < LoggerReaderBookmark > > forw =
                        boost::make_shared < CxPsForwarder < LoggerReaderBookmark > >(cxTransport, destFilePrefix.string(), V2ASourceFileAttributes::DestFileExt, isStreamData);

                    const int MaxTotalParentFolderSize = isTelemetryTable ? MaxTotalTelemetryFolderSize : MaxTotalLogsFolderSize;

                    forw->Initialize(
                        MaxPerFileSize, MaxTotalParentFolderSize, MaxTotalFilesCntInParent,
                        IncompleteFileCleanupTimeout, MaxNumOfAttempts);

                    sourceAgentLog = CollForwPair<std::string, std::string, LoggerReaderBookmark>::MakePair(coll, forw, conv);
                }
                CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                    GENERIC_CATCH_LOG_IGNORE("Creating source logger telemetry reader - CxPs Uploader");

                collForwPairs.push_back(sourceAgentLog);
            }

            RETURN_IF_CANCELLATION_REQUESTED;

            {
                // Creating IR RCM source logger telemetry reader - CxPs Uploader
                IRRcmSourceTraceProcessor dpSyncRcmSrcTraceProcessor(this, m_cmdLineSettings);
                std::vector<PICollForwPair> dpSyncRcmSrcCollForwPairs;
                dpSyncRcmSrcTraceProcessor.GetCollForwPair(dpSyncRcmSrcCollForwPairs, cancelRequested);
                collForwPairs.insert(collForwPairs.end(), dpSyncRcmSrcCollForwPairs.begin(), dpSyncRcmSrcCollForwPairs.end());
            }
        }
    }

    void V2ARcmSourceOnAzureRunnerFactory::InitializeEnvironment(
        const CmdLineSettings &settings, std::vector < boost::shared_ptr < void > > &uninitializers)
    {
        BOOST_ASSERT(lc);

        {
            Trace::Init("",
                (LogLevel)lc->getLogLevel(),
                boost::bind(&A2ALogCallback, _1, _2));
        }

        {
            AzureStorageRest::HttpClient::GlobalInitialize();
            AzureStorageRest::HttpClient::SetRetryPolicy(0);

            uninitializers.insert(uninitializers.begin(), boost::shared_ptr<void>(
                static_cast<void*>(0), boost::bind(AzureStorageRest::HttpClient::GlobalUnInitialize)));
        }
    }

    void V2ARcmSourceOnAzureRunnerFactory::GetCollForwPairs(std::vector<PICollForwPair>& collForwPairs, boost::function0<bool> cancelRequested)
    {
        collForwPairs.clear();
        BOOST_ASSERT(lc);

        RETURN_IF_CANCELLATION_REQUESTED;

        // TODO-SanKumar-1703: Make the table names found immediately below and in few blocks below
        // as constants to avoid any mistakes.
        std::map<std::string, std::string> sourceAgentLogsMap;
        sourceAgentLogsMap["svagents.log"] = V2AMdsTableNames::AdminLogs;
        sourceAgentLogsMap["s2.log"] = V2AMdsTableNames::AdminLogs;
        sourceAgentLogsMap["appservice.log"] = V2AMdsTableNames::AdminLogs;
        sourceAgentLogsMap["srcTelemetry.log"] = V2AMdsTableNames::SourceTelemetry;
        sourceAgentLogsMap["tagTelemetry.log"] = V2AMdsTableNames::Vacp;
        sourceAgentLogsMap["schedulerTelemetry.log"] = V2AMdsTableNames::SchedulerTelemetry;

        const bool RemoveMapKeyName = true;
        const unsigned int maxFileSize = lc->getLogMaxFileSize();
        const unsigned int maxFileCount = lc->getLogMaxCompletedFiles();
        boost::filesystem::path telemetryLogsPath(lc->getLogPathname());
        telemetryLogsPath /= "ArchivedLogs";

        RETURN_IF_CANCELLATION_REQUESTED;

        if (!gbPrivateEndpointEnabled)
        {
            std::string eventHubSasUri = RcmClientLib::RcmConfigurator::getInstance()->GetTelemetrySasUriForAzureToOnPrem();
            if (eventHubSasUri.empty())
            {
                // no telemetry settings exist
                return;
            }
        }

#ifdef SV_WINDOWS
        {
            PICollForwPair inDskfltEvtLog; // Will enqueue nullptr, if the below pair creation fails.
            try
            {
                std::string queryName;
                std::wstring queryStr;
                GetSourceAdminLogsQuery(queryName, queryStr);

                boost::shared_ptr<EventLogCollector> coll = boost::make_shared<EventLogCollector>(queryName, queryStr);

                RETURN_IF_CANCELLATION_REQUESTED;

                boost::shared_ptr<EventLogToMdsLogConverter> conv = boost::make_shared<EventLogToMdsLogConverter>();
                conv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), ComponentId::SOURCE_AGENT, RemoveMapKeyName);

                boost::shared_ptr<ForwarderBase<std::string, boost::shared_ptr<EventLogRecord>>> forw;
                if (gbPrivateEndpointEnabled)
                {
                    forw = boost::make_shared<PersistAndPruneForwarder<std::string, boost::shared_ptr<EventLogRecord>>>(telemetryLogsPath.string(), A2ATableNames::AdminLogs, maxFileSize, maxFileCount);
                }
                else
                {
                    forw = boost::make_shared<EventHubForwarderEx<boost::shared_ptr<EventLogRecord>>>(V2AMdsTableNames::AdminLogs);
                }
                inDskfltEvtLog = CollForwPair<boost::shared_ptr<EventLogRecord>, std::string, boost::shared_ptr<EventLogRecord>>::MakePair(coll, forw, conv);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Creating indskflt driver eventlog - EventHub Uploader");

            collForwPairs.push_back(inDskfltEvtLog);
        }

        RETURN_IF_CANCELLATION_REQUESTED;

        {
            PICollForwPair inDskfltTelemetry; // Will enqueue nullptr, if the below pair creation fails.
            try
            {
                std::string queryName;
                std::wstring queryStr;
                GetInDskFltTelemetryQuery(queryName, queryStr);

                boost::shared_ptr<EventLogTelemetryCollector> coll =
                    boost::make_shared<EventLogTelemetryCollector>(queryName, queryStr);

                RETURN_IF_CANCELLATION_REQUESTED;

                boost::shared_ptr<EventLogTelemetryToMdsConverter> conv = boost::make_shared<EventLogTelemetryToMdsConverter>();
                conv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), RemoveMapKeyName);

                boost::shared_ptr<ForwarderBase<std::string, boost::shared_ptr<EventLogRecord>>> forw;
                if (gbPrivateEndpointEnabled)
                {
                    forw = boost::make_shared<PersistAndPruneForwarder<std::string, boost::shared_ptr<EventLogRecord>>>(telemetryLogsPath.string(), A2ATableNames::DriverTelemetry, maxFileSize, maxFileCount);
                }
                else
                {
                    forw = boost::make_shared<EventHubForwarderEx<boost::shared_ptr<EventLogRecord>>>(V2AMdsTableNames::DriverTelemetry);
                }
                inDskfltTelemetry = CollForwPair<std::map<std::string, std::string>, std::string, boost::shared_ptr<EventLogRecord>>::MakePair(coll, forw, conv);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Creating indskflt driver telemetry - EventHub Uploader");

            collForwPairs.push_back(inDskfltTelemetry);
        }
#endif // SV_WINDOWS

        RETURN_IF_CANCELLATION_REQUESTED;

#ifdef SV_UNIX
        {
            PICollForwPair inVolFltLinuxTelemetry; // Will enqueue nullptr, if the below pair creation fails.
            try
            {
                // TODO-SanKumar-1704: Move the file name to Config.
                const std::string inVolFltLinuxTelemetryPath =
                    (boost::filesystem::path(lc->getLogPathname()) / "involflt_telemetry.log").string();

                boost::shared_ptr< InVolFltLinuxTelemetryLoggerReader < LoggerReaderBookmark > > inVolFltLinuxTelemetryReader =
                    boost::make_shared< InVolFltLinuxTelemetryLoggerReader < LoggerReaderBookmark > >(inVolFltLinuxTelemetryPath);

                boost::shared_ptr< InMageLoggerCollector < LoggerReaderBookmark > > coll =
                    boost::make_shared< InMageLoggerCollector < LoggerReaderBookmark > >(inVolFltLinuxTelemetryReader);

                RETURN_IF_CANCELLATION_REQUESTED;

                boost::shared_ptr<InVolFltLinuxTelemetryJsonMdsConverter> conv = boost::make_shared<InVolFltLinuxTelemetryJsonMdsConverter>();
                conv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), RemoveMapKeyName);

                boost::shared_ptr<ForwarderBase<std::string, LoggerReaderBookmark> > forw;
                if (gbPrivateEndpointEnabled)
                {
                    forw = boost::make_shared<PersistAndPruneForwarder<std::string, LoggerReaderBookmark> >(telemetryLogsPath.string(), A2ATableNames::DriverTelemetry, maxFileSize, maxFileCount);
                }
                else
                {
                    forw = boost::make_shared<EventHubForwarderEx<LoggerReaderBookmark> >(V2AMdsTableNames::DriverTelemetry);
                }

                inVolFltLinuxTelemetry = CollForwPair<std::string, std::string, LoggerReaderBookmark>::MakePair(coll, forw, conv);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Creating involflt linux driver telemetry - EventHub Uploader");

            collForwPairs.push_back(inVolFltLinuxTelemetry);
        }
#endif // SV_UNIX

        // Create pairs for each of the agent binary logs and telemetry tables.
        for (std::map<std::string, std::string>::const_iterator itr = sourceAgentLogsMap.begin(); itr != sourceAgentLogsMap.end(); itr++)
        {
            RETURN_IF_CANCELLATION_REQUESTED;

            // TODO-SanKumar-1702: Read CSV from configuration and initialize one pair per each table.
            PICollForwPair sourceAgentLog; // Will enqueue nullptr, if the below pair creation fails.
            try
            {
                // TODO-SanKumar-1702: We qualify if the table name contains Telemetry as telemetry
                // table. The dictionary should be extended to contain a detailed object that would
                // say if it's a telemetry table or not and other factors right inside it.
                bool isTelemetryTable = boost::icontains(itr->second, "Telemetry");

                const std::string sourceLogPath = (boost::filesystem::path(lc->getLogPathname()) / itr->first).string();

                boost::shared_ptr< AgentLoggerReader < LoggerReaderBookmark > > reader = boost::make_shared< AgentLoggerReader <LoggerReaderBookmark> >(sourceLogPath);
                boost::shared_ptr< InMageLoggerCollector < LoggerReaderBookmark > > coll = boost::make_shared<InMageLoggerCollector <LoggerReaderBookmark> >(reader);

                RETURN_IF_CANCELLATION_REQUESTED;

                const SV_LOG_LEVEL ForwPutLogLevel = isTelemetryTable ? SV_LOG_ALWAYS : lc->getEvtCollForwAgentLogPostLevel();
                std::string subComponentName = boost::filesystem::path(itr->first).stem().string();

                boost::shared_ptr < IConverter < std::string, std::string > > conv;
                if (isTelemetryTable)
                {
                    boost::shared_ptr< InMageLoggerTelemetryToMdsConverter > teleConv =
                        boost::make_shared< InMageLoggerTelemetryToMdsConverter >(ForwPutLogLevel);

                    teleConv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), RemoveMapKeyName);
                    conv = teleConv;
                }
                else
                {
                    boost::shared_ptr< InMageLoggerToMdsLogConverter > logConv =
                        boost::make_shared< InMageLoggerToMdsLogConverter >(ForwPutLogLevel, subComponentName);

                    logConv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), ComponentId::SOURCE_AGENT, RemoveMapKeyName);
                    conv = logConv;
                }

                boost::shared_ptr<ForwarderBase<std::string, LoggerReaderBookmark> > forw;
                if (gbPrivateEndpointEnabled)
                {
                    std::string remappedTableName = Utils::GetA2ATableNameFromV2ATableName(itr->second);
                    forw = boost::make_shared<PersistAndPruneForwarder<std::string, LoggerReaderBookmark> >(telemetryLogsPath.string(), remappedTableName, maxFileSize, maxFileCount);
                }
                else
                {
                    forw = boost::make_shared<EventHubForwarderEx<LoggerReaderBookmark> >(itr->second);
                }

                sourceAgentLog = CollForwPair<std::string, std::string, LoggerReaderBookmark>::MakePair(coll, forw, conv);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Creating source logger telemetry reader - EventHub Uploader");

            collForwPairs.push_back(sourceAgentLog);
        }

        RETURN_IF_CANCELLATION_REQUESTED;

        {
            // Creating IR RCM source logger telemetry reader - EventHub Uploader
            IRRcmSourceOnAzureTraceProcessor dpSyncRcmSrcTraceProcessor(this, m_cmdLineSettings);
            std::vector<PICollForwPair> dpSyncRcmSrcCollForwPairs;
            dpSyncRcmSrcTraceProcessor.GetCollForwPair(dpSyncRcmSrcCollForwPairs, cancelRequested);
            collForwPairs.insert(collForwPairs.end(), dpSyncRcmSrcCollForwPairs.begin(), dpSyncRcmSrcCollForwPairs.end());
        }
    }

    bool SerializedRunnerFactory::GetTransportConnectionSettings(TRANSPORT_CONNECTION_SETTINGS& transportSettings)
    {
        // Based on m_cmdLineSettings prepare TRANSPORT_CONNECTION_SETTINGS for V2A CS or V2A RCM
        if (boost::iequals(m_cmdLineSettings.Environment, EvtCollForw_Env_V2ASource))
        {
            Configurator* configurator;
            boost::shared_ptr<Configurator> pConfigurator;
            try {
                if (!::InitializeConfigurator(&configurator, USE_ONLY_CACHE_SETTINGS, PHPSerialize))
                {
                    pConfigurator.reset(configurator);
                    throw INMAGE_EX("Unable to instantiate configurator");
                }
                pConfigurator.reset(configurator);
            }
            GENERIC_CATCH_LOG_ACTION("SerializedRunnerFactory::GetTransportConnectionSettings: InitializeConfigurator", throw);


            // If there's no volume group, there's no protection enabled. So, we can't get a PS transport
            // setting in this case.
            const HOST_VOLUME_GROUP_SETTINGS &hostVolumeGroupSettings = configurator->getHostVolumeGroupSettings();
            if (hostVolumeGroupSettings.volumeGroups.empty())
                return false;

            // TODO-SanKumar-1702: In V2A source, all the host volume groups are supposed to have the
            // same transport to PS. Just as future facing, pick a random index rather than always picking 1st.
            transportSettings = hostVolumeGroupSettings.volumeGroups.front().transportSettings;
        }

        if (boost::iequals(m_cmdLineSettings.Environment, EvtCollForw_Env_V2ARcmSource))
        {
            std::string dataTransportType, dataPathTransportSettings;
            RcmClientLib::ProcessServerTransportSettings processServerDetails;

            RcmClientLib::RcmConfigurator::getInstance()->GetDataPathTransportSettings(dataTransportType, dataPathTransportSettings);

            // Parse ProcessServerDetails based on data transport type
            if (boost::iequals(dataTransportType, RcmClientLib::TRANSPORT_TYPE_PROCESS_SERVER))
            {
                processServerDetails = JSON::consumer<RcmClientLib::ProcessServerTransportSettings>::convert(dataPathTransportSettings, true);
            }
            else if (boost::iequals(dataTransportType, RcmClientLib::TRANSPORT_TYPE_LOCAL_FILE) ||
                boost::iequals(dataTransportType, "null"))
            {
                // For <see cref="RcmEnum.DataPathTransport.AzureBlobStorageBasedTransport"/> and
                // <see cref="RcmEnum.DataPathTransport.LocalFileTransport"/>, dataPathTransportSettings will be empty string.
                // So m_ProcessServerDetails remain uninitialized for RcmEnum.DataPathTransport.LocalFileTransport.
            }
            else
            {
                // Unsupported transport type received - throw
                std::stringstream sserr;
                sserr << "Unsupported transport type received " << dataTransportType << '.';
                DebugPrintf(EvCF_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, sserr.str().c_str());
                throw INMAGE_EX(sserr.str().c_str());
            }

            transportSettings.ipAddress = processServerDetails.GetIPAddress();
            transportSettings.sslPort = boost::lexical_cast<std::string>(processServerDetails.Port);
        }

        return true;
    }

    namespace V2AProcessServerFileAttributes
    {
        // TODO-SanKumar-1708: Should these be from the config?
        static const char *LogTelemetryRoot = "var";
        static const char *PSEventLogFolder = "ps_event_logs";
        static const char *PSTelemetryFolder = "telemetry_logs";
        static const char *CxpsTelemetryFolder = "cxps_telemetry";
        static const char *PSLogsFolder = "processserver_logs";
        static const char *PSMonitorLogsFolder = "processservermonitor_logs";
        static const char *RcmPSConfLogsFolder = "rcmpsconfigurator_logs";
        static const char *DestFileExt = ".json";
        static const char *LogFileExt = ".log";
    }

    boost::filesystem::path  V2AProcessServerRunnerFactoryBase::s_psEvtLogFolderPath;
    boost::filesystem::path V2AProcessServerRunnerFactoryBase::s_psTelFolderPath;
    boost::filesystem::path V2AProcessServerRunnerFactoryBase::s_cxpsTelFolderPath;
    std::set<boost::filesystem::path> V2AProcessServerRunnerFactoryBase::s_hostTelFilesPattern;
    std::set<boost::filesystem::path> V2AProcessServerRunnerFactoryBase::s_hostLogFilesPattern;
    boost::filesystem::path V2AProcessServerRunnerFactoryBase::s_psLogFilesPattern;
    boost::filesystem::path V2AProcessServerRunnerFactoryBase::s_psMonitorLogFilesPattern;

    boost::shared_ptr<CxTransport> V2AProcessServerRunnerFactoryBase::s_fileCxTransport;

    void V2AProcessServerRunnerFactory::InitializeEnvironment(
        const CmdLineSettings &settings, std::vector < boost::shared_ptr < void > > &uninitializers)
    {
        {
            // Setup Xfer log

            boost::shared_ptr<boost::thread> xferMonitorThread = StartFileXferLog(settings);

            // TODO-SanKumar-1702: Create macro for uninit push back...
            uninitializers.insert(uninitializers.begin(), boost::shared_ptr<void>(
                static_cast<void*>(0), boost::bind(StopXferLog, xferMonitorThread)));

            // TODO-SanKumar-1708: Xfer logs aren't seen in both Source and PS but the file is created.
        }

        {
            // Create the file cxtransport object.

            // One time construction of a stateless object, which saves reading the remapping details
            // from the conf file in every iteration.

            s_fileCxTransport = boost::make_shared<CxTransport>(
                TRANSPORT_PROTOCOL_FILE, TRANSPORT_CONNECTION_SETTINGS(), true);
        }

        {
            const char *GuidMatchingWildCard = "*-*-*-*";

            // /home/svsystems/var
            boost::filesystem::path baseFolder =
                boost::filesystem::path(lc->getDestDir())
                / V2AProcessServerFileAttributes::LogTelemetryRoot;

            // /home/svsystems/var/ps_event_logs
            s_psEvtLogFolderPath = baseFolder / V2AProcessServerFileAttributes::PSEventLogFolder;

            // /home/svsystems/var/telemetry_logs
            s_psTelFolderPath = baseFolder / V2AProcessServerFileAttributes::PSTelemetryFolder;

            // /home/svsystems/var/cxps_telemetry
            s_cxpsTelFolderPath = baseFolder / V2AProcessServerFileAttributes::CxpsTelemetryFolder;

            // /home/svsystems/*-*-*-*
            boost::filesystem::path baseFolderPerSource =
                boost::filesystem::path(lc->getDestDir())
                / GuidMatchingWildCard;

            // /home/svsystems/*-*-*-*/telemetry
            s_hostTelFilesPattern.insert(baseFolderPerSource / V2ASourceFileAttributes::DestTelemetryFolder);

            // /home/svsystems/*-*-*-*/logs
            s_hostLogFilesPattern.insert(baseFolderPerSource / V2ASourceFileAttributes::DestLogsFolder);

            // /home/svsystems/var/processserver_logs/*
            s_psLogFilesPattern = baseFolder / V2AProcessServerFileAttributes::PSLogsFolder / "*";

            // /home/svsystems/var/processservermonitor_logs/*
            s_psMonitorLogFilesPattern = baseFolder / V2AProcessServerFileAttributes::PSMonitorLogsFolder / "*";
        }
    }

    boost::filesystem::path V2ARcmProcessServerRunnerFactory::s_rcmPSConfLogFilesPattern;
    boost::filesystem::path V2ARcmProcessServerRunnerFactory::s_rcmPSAgentSetupLogFilesDestPattern;
    std::set<boost::filesystem::path> V2ARcmProcessServerRunnerFactory::s_rcmPSAgentSetupLogFilesPattern;

    void V2ARcmProcessServerRunnerFactory::InitializeEnvironment(
        const CmdLineSettings &settings, std::vector < boost::shared_ptr < void > > &uninitializers)
    {
        {
            Trace::Init("",
                (LogLevel)lc->getLogLevel(),
                boost::bind(&A2ALogCallback, _1, _2));
        }

        {
            // Setup Xfer log

            boost::shared_ptr<boost::thread> xferMonitorThread = StartFileXferLog(settings);

            // TODO-SanKumar-1702: Create macro for uninit push back...
            uninitializers.insert(uninitializers.begin(), boost::shared_ptr<void>(
                static_cast<void*>(0), boost::bind(StopXferLog, xferMonitorThread)));

            // TODO-SanKumar-1708: Xfer logs aren't seen in both Source and PS but the file is created.
        }

        {
            // Create the file cxtransport object.

            // One time construction of a stateless object, which saves reading the remapping details
            // from the conf file in every iteration.

            s_fileCxTransport = boost::make_shared<CxTransport>(
                TRANSPORT_PROTOCOL_FILE, TRANSPORT_CONNECTION_SETTINGS(), true);
        }

        {
            const char *GuidMatchingWildCard = "*-*-*-*";


            boost::filesystem::path baseFolder = boost::filesystem::path(lc->getInstallPath()) / PSComponentPaths::HOME / PSComponentPaths::SVSYSTEMS;
            baseFolder /= V2AProcessServerFileAttributes::LogTelemetryRoot;

            // /home/svsystems/var/ps_event_logs
            s_psEvtLogFolderPath = baseFolder / V2AProcessServerFileAttributes::PSEventLogFolder;

            // /home/svsystems/var/telemetry_logs
            s_psTelFolderPath = baseFolder / V2AProcessServerFileAttributes::PSTelemetryFolder;

            // /home/svsystems/var/cxps_telemetry
            s_cxpsTelFolderPath = baseFolder / V2AProcessServerFileAttributes::CxpsTelemetryFolder;

            // /home/svsystems/var/processserver_logs/*
            s_psLogFilesPattern = baseFolder / V2AProcessServerFileAttributes::PSLogsFolder / "*";

            // /home/svsystems/var/processservermonitor_logs/*
            s_psMonitorLogFilesPattern = baseFolder / V2AProcessServerFileAttributes::PSMonitorLogsFolder / "*";

            // /home/svsystems/var/rcmpsconfigurator_logs/*
            s_rcmPSConfLogFilesPattern = baseFolder / V2AProcessServerFileAttributes::RcmPSConfLogsFolder / "*";
        }
    }

    bool V2AProcessServerRunnerFactory::ValidateHostSourceFolder(const std::string &fullPath)
    {
        // Logs are put in the folder whose name is a guid without braces, that is equal to host id.
        // Host Id for source:
        //      128d4441-f809-4b99-af05-c0650a2205a8
        // Host Id for MT:
        //      B0BF6C1A-86B0-E047-ADA01B311CFAD3EA
        // Note: MT host ID has missing a fourth hyphen (-)
        const char *hostIdRegexStr = "([A-F]|[a-f]|[0-9]){8}-([A-F]|[a-f]|[0-9]){4}-([A-F]|[a-f]|[0-9]){4}-([A-F]|[a-f]|[0-9]){4}-?([A-F]|[a-f]|[0-9]){12}";
        //                                                                              MT Host ID has a hyphen (-) missing here ^

        boost::filesystem::path path(fullPath);
        if (!path.has_parent_path())
            return false;

        path = path.parent_path();
        if (!path.has_parent_path())
            return false;

        path = path.parent_path();
        if (!path.has_filename())
            return false;

        std::string hostId = path.filename().string();

        boost::regex regex(hostIdRegexStr);
        boost::sregex_iterator itr(hostId.begin(), hostId.end(), regex), matchEnd;

        if (itr == matchEnd || (++itr) != matchEnd)
            return false;

        return true;
    }

    bool V2ARcmProcessServerRunnerFactory::ValidateHostSourceFolder(const std::string &fullPath)
    {
        return true;
    }

    void V2ARcmProcessServerRunnerFactory::GetCollForwPairs(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested)
    {
#ifdef SV_WINDOWS
        bool RemoveMapKeyName = true, isStreamData = false;
        const unsigned int maxFileSize = lc->getLogMaxFileSize();
        const unsigned int maxFileCount = lc->getLogMaxCompletedFiles();
        boost::function1<bool, const std::string&> funptr = ValidateHostSourceFolder;
        boost::filesystem::path telemetryLogsPath(GetRcmPSInstallationInfo().m_telemetryFolderPath);
        telemetryLogsPath /= "ArchivedLogs";

        boost::shared_ptr<ForwarderBase<std::vector<std::string>, std::vector<std::string>>> comForw;
        if (gbPrivateEndpointEnabled)
        {
            comForw = boost::make_shared<PersistAndPruneForwarder<std::vector<std::string>, std::vector <std::string>>>(telemetryLogsPath.string(), maxFileSize, maxFileCount);
        }
        else
        {
            comForw = boost::make_shared<EventHubForwarder>();
        }

        // Since the ps settings can change in runtime, we need to fetch fresh settings for each iteration
        // Otherwise mew pairs will not be taken into account.
        // This is in contrast to legacy stack, where we use wildcards in paths, which will take care of all pairs
        std::map<std::string, std::string> psTelemetrySettings;
        Utils::GetPSSettingsMap(psTelemetrySettings);

        s_hostTelFilesPattern.clear();
        s_hostLogFilesPattern.clear();
        s_rcmPSAgentSetupLogFilesPattern.clear();

        for (std::map<std::string, std::string>::iterator itr = psTelemetrySettings.begin(); itr != psTelemetrySettings.end(); itr++)
        {
            const char *GuidMatchingWildCard = "*-*-*-*";
            boost::filesystem::path hostTelemetryPath(itr->second);
            s_hostTelFilesPattern.insert(hostTelemetryPath / V2ASourceFileAttributes::DestTelemetryFolder);
            s_hostLogFilesPattern.insert(hostTelemetryPath / V2ASourceFileAttributes::DestLogsFolder);
            s_rcmPSAgentSetupLogFilesPattern.insert(hostTelemetryPath / GuidMatchingWildCard);
        }

        // Adding pairs for MT DP logs
        boost::filesystem::path destFilePrefix(GetRcmPSInstallationInfo().m_telemetryFolderPath);
        destFilePrefix /= RcmClientLib::RcmConfigurator::getInstance()->getHostId();
        s_hostTelFilesPattern.insert(destFilePrefix / V2ASourceFileAttributes::DestTelemetryFolder);
        s_hostLogFilesPattern.insert(destFilePrefix / V2ASourceFileAttributes::DestLogsFolder);
        s_rcmPSAgentSetupLogFilesDestPattern = boost::filesystem::path(GetRcmPSInstallationInfo().m_telemetryFolderPath)
            / "AgentSetupLogs" / V2ASourceFileAttributes::DestLogsFolder / V2AMdsTableNames::AgentSetupLogs;

        GetCommonCollForwPairs(collForwPairs, cancelRequested, RemoveMapKeyName, isStreamData,
            comForw, funptr);

        RETURN_IF_CANCELLATION_REQUESTED;

        {
            PICollForwPair rcmPSConfLogsToComPair; // Will enqueue nullptr, if the below pair creation fails.
            try
            {
                typedef std::vector<std::string> strVect;

                PFilePollCollector coll = boost::make_shared<FilePollCollector>(
                    s_fileCxTransport, s_rcmPSConfLogFilesPattern, V2AProcessServerFileAttributes::DestFileExt,
                    boost::function1<bool, const std::string&>());

                rcmPSConfLogsToComPair = CollForwPair<strVect, strVect, strVect>::MakePair(coll, comForw);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Creating Rcm PS Configurator log file poller - Eventhub uploader pair");
            collForwPairs.push_back(rcmPSConfLogsToComPair);
        }

        RETURN_IF_CANCELLATION_REQUESTED;

        {
            // Creating IR RCM MT logger reader - CxPs Uploader
            IRRcmMTTraceProcessor dpIRRcmMTTraceProcessor(this, m_cmdLineSettings);
            std::vector<PICollForwPair> dpMTRcmCollForwPairs;
            dpIRRcmMTTraceProcessor.GetCollForwPair(dpMTRcmCollForwPairs, cancelRequested);
            collForwPairs.insert(collForwPairs.end(), dpMTRcmCollForwPairs.begin(), dpMTRcmCollForwPairs.end());
        }

        RETURN_IF_CANCELLATION_REQUESTED;

        {
            for (std::set<boost::filesystem::path>::iterator itr = s_rcmPSAgentSetupLogFilesPattern.begin();
                itr != s_rcmPSAgentSetupLogFilesPattern.end(); itr++)
            {
                PICollForwPair agentLogPair;

                try
                {
                    boost::shared_ptr<AgentSetupLogsCollector> agentSetupLogsCollectorPtr = boost::make_shared<AgentSetupLogsCollector>(
                        s_fileCxTransport, itr->string(), V2AProcessServerFileAttributes::LogFileExt);

                    const int MaxFileSize = 200 * 1024; // 200 KB

                    boost::shared_ptr<AgentSetupLogConvertor> agentSetupLogConvertorPtr = boost::make_shared<AgentSetupLogConvertor>();
                    agentSetupLogConvertorPtr->Initialize(GetFabricType(), GetBiosId(), GetHostId(), ComponentId::PROCESS_SERVER, RemoveMapKeyName);

                    boost::shared_ptr<AgentSetupLogForwarder> agentSetupLogForwarderPtr = boost::make_shared<AgentSetupLogForwarder>(
                        s_rcmPSAgentSetupLogFilesDestPattern.string(), V2AProcessServerFileAttributes::DestFileExt, isStreamData, MaxFileSize);

                    agentLogPair = CollForwPair<std::string, std::vector<std::string>, std::string>::MakePair(agentSetupLogsCollectorPtr,
                        agentSetupLogForwarderPtr, agentSetupLogConvertorPtr);

                    collForwPairs.push_back(agentLogPair);
                }
                CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                    GENERIC_CATCH_LOG_IGNORE("Failed to create coll-forw pair for agent setup logs");
            }
        }

        RETURN_IF_CANCELLATION_REQUESTED;

        {
            PICollForwPair agentLogFileUploadPair; // Will enqueue nullptr, if the below pair creation fails.
            try
            {
                typedef std::vector<std::string> strVect;

                PFilePollCollector coll = boost::make_shared<FilePollCollector>(
                    s_fileCxTransport, s_rcmPSAgentSetupLogFilesDestPattern.parent_path(), V2AProcessServerFileAttributes::DestFileExt,
                    boost::function1<bool, const std::string&>());

                agentLogFileUploadPair = CollForwPair<strVect, strVect, strVect>::MakePair(coll, comForw);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Creating Rcm PS Configurator log file poller - Eventhub uploader pair");
            collForwPairs.push_back(agentLogFileUploadPair);
        }

#endif
    }

    void V2AProcessServerRunnerFactory::GetCollForwPairs(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested)
    {
#ifdef SV_WINDOWS
        // In V2A, MARS expects a JSON "with" the surrounding { Map : {Actual Data} }
        bool RemoveMapKeyName = false, isStreamData = true;

        boost::function1<bool, const std::string&> funptr = ValidateHostSourceFolder;

        boost::shared_ptr<ForwarderBase< std::vector<std::string >, std::vector <std::string> >> comForw = boost::make_shared<MarsComForwarder>(lc->getEvtCollForwMaxMarsUploadFilesCnt());

        GetCommonCollForwPairs(collForwPairs, cancelRequested, RemoveMapKeyName, isStreamData,
            comForw, funptr);

        {
            // Creating MT logger telemetry reader - CxPs Uploader
            MTTraceProcessor mtTraceProcessor(this, m_cmdLineSettings);
            std::vector<PICollForwPair> mtCollForwPairs;
            mtTraceProcessor.GetCollForwPair(mtCollForwPairs, cancelRequested);
            collForwPairs.insert(collForwPairs.end(), mtCollForwPairs.begin(), mtCollForwPairs.end());
        }

        RETURN_IF_CANCELLATION_REQUESTED;

        {
            // Creating IR MT logger telemetry reader - CxPs Uploader
            IRMTTraceProcessor dpIRMTTraceProcessor(this, m_cmdLineSettings);
            std::vector<PICollForwPair> dpMTCollForwPairs;
            dpIRMTTraceProcessor.GetCollForwPair(dpMTCollForwPairs, cancelRequested);
            collForwPairs.insert(collForwPairs.end(), dpMTCollForwPairs.begin(), dpMTCollForwPairs.end());
        }

        RETURN_IF_CANCELLATION_REQUESTED;

        {
            // Creating DR MT logger telemetry reader - CxPs Uploader
            DRMTTraceProcessor dpDRMTTraceProcessor(this, m_cmdLineSettings);
            std::vector<PICollForwPair> dpDRMTCollForwPairs;
            dpDRMTTraceProcessor.GetCollForwPair(dpDRMTCollForwPairs, cancelRequested);
            collForwPairs.insert(collForwPairs.end(), dpDRMTCollForwPairs.begin(), dpDRMTCollForwPairs.end());
        }

        RETURN_IF_CANCELLATION_REQUESTED;
#endif
    }

    void V2AProcessServerRunnerFactoryBase::GetCommonCollForwPairs(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested,
        bool RemoveMapKeyName, bool isStreamData, boost::shared_ptr<ForwarderBase< std::vector<std::string >, std::vector <std::string> > > &comForw,
        boost::function1<bool, const std::string&> &funptr)
    {
        BOOST_ASSERT(lc);

        RETURN_IF_CANCELLATION_REQUESTED;

        collForwPairs.clear();

#ifdef SV_WINDOWS

        RETURN_IF_CANCELLATION_REQUESTED;

        BOOST_ASSERT(s_fileCxTransport);

        {
            // Writes the event logs in PS to files.

            PICollForwPair psEvtLogToFilePair; // Will enqueue nullptr, if the below pair creation fails.
            try
            {
                std::string queryName;
                std::wstring queryStr;
                GetPSAdminLogsQuery(queryName, queryStr);

                boost::shared_ptr<EventLogCollector> coll = boost::make_shared<EventLogCollector>(queryName, queryStr);

                RETURN_IF_CANCELLATION_REQUESTED;

                boost::shared_ptr<EventLogToMdsLogConverter> conv = boost::make_shared<EventLogToMdsLogConverter>();
                conv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), ComponentId::PROCESS_SERVER, RemoveMapKeyName);

                boost::filesystem::path destFilePrefix = s_psEvtLogFolderPath / V2AMdsTableNames::AdminLogs;

                boost::shared_ptr<CxPsForwarder<boost::shared_ptr<EventLogRecord>>> forw =
                    boost::make_shared<CxPsForwarder<boost::shared_ptr<EventLogRecord>>>(
                        s_fileCxTransport, destFilePrefix.string(), V2AProcessServerFileAttributes::DestFileExt, isStreamData);

                // TODO-SanKumar-1708: Add these values to configuration.
                const int MaxPerFileSize = 1 * 1024 * 1024; // 1 MB
                const int MaxTotalLogsFolderSize = 20 * 1024 * 1024; // 20 MB
                const int MaxTotalFilesCntInParent = 100;
                const int IncompleteFileCleanupTimeout = 2 * 60 * 60; // 2 hours
                const int MaxNumOfAttempts = 1; // Local file transport, so no retries.

                forw->Initialize(
                    MaxPerFileSize, MaxTotalLogsFolderSize, MaxTotalFilesCntInParent,
                    IncompleteFileCleanupTimeout, MaxNumOfAttempts);

                psEvtLogToFilePair = CollForwPair<boost::shared_ptr<EventLogRecord>, std::string, boost::shared_ptr<EventLogRecord>>::MakePair(coll, forw, conv);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Creating PS event log - File writer pair");
            collForwPairs.push_back(psEvtLogToFilePair);
        }

        RETURN_IF_CANCELLATION_REQUESTED;

        {
            // Uploads the event logs written in files to MDS via MARS.

            PICollForwPair psEvtLogFilesToComPair; // Will enqueue nullptr, if the below pair creation fails.
            try
            {
                typedef std::vector<std::string> strVect;

                PFilePollCollector coll = boost::make_shared<FilePollCollector>(
                    s_fileCxTransport, s_psEvtLogFolderPath, V2AProcessServerFileAttributes::DestFileExt,
                    boost::function1<bool, const std::string&>());

                psEvtLogFilesToComPair = CollForwPair<strVect, strVect, strVect>::MakePair(coll, comForw);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Creating PS event log file poller - MARS uploader pair");
            collForwPairs.push_back(psEvtLogFilesToComPair);
        }

        RETURN_IF_CANCELLATION_REQUESTED;

        // TODO-SanKumar-1708: Add upload of agent logs - svagents, appservice, etc.
        // Also, don't initialize agent telemetry (log cutter) for the user-mode binaries in PS.

        {
            PICollForwPair psTelToComPair; // Will enqueue nullptr, if the below pair creation fails.
            try
            {
                typedef std::vector<std::string> strVect;

                PFilePollCollector coll = boost::make_shared<FilePollCollector>(
                    s_fileCxTransport, s_psTelFolderPath, V2AProcessServerFileAttributes::DestFileExt,
                    boost::function1<bool, const std::string&>());

                psTelToComPair = CollForwPair<strVect, strVect, strVect>::MakePair(coll, comForw);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Creating PS telemetry file poller - MARS uploader pair");
            collForwPairs.push_back(psTelToComPair);
        }

        RETURN_IF_CANCELLATION_REQUESTED;

        {
            PICollForwPair cxpsTelToComPair; // Will enqueue nullptr, if the below pair creation fails.
            try
            {
                typedef std::vector<std::string> strVect;

                PFilePollCollector coll = boost::make_shared<FilePollCollector>(
                    s_fileCxTransport, s_cxpsTelFolderPath, V2AProcessServerFileAttributes::DestFileExt,
                    boost::function1<bool, const std::string&>());

                cxpsTelToComPair = CollForwPair<strVect, strVect, strVect>::MakePair(coll, comForw);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Creating cxps telemetry file poller - MARS uploader pair");
            collForwPairs.push_back(cxpsTelToComPair);
        }

        RETURN_IF_CANCELLATION_REQUESTED;

        {
            for (std::set<boost::filesystem::path>::iterator itr = s_hostTelFilesPattern.begin(); itr != s_hostTelFilesPattern.end(); itr++)
            {
                PICollForwPair hostTelToComPair; // Will enqueue nullptr, if the below pair creation fails.
                try
                {
                    typedef std::vector<std::string> strVect;

                    PFilePollCollector coll = boost::make_shared<FilePollCollector>(
                        s_fileCxTransport, *itr, V2ASourceFileAttributes::DestFileExt,
                        funptr);

                    hostTelToComPair = CollForwPair<strVect, strVect, strVect>::MakePair(coll, comForw);
                }
                CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                    GENERIC_CATCH_LOG_IGNORE("Creating Agent telemetry file poller - MARS uploader pair");
                collForwPairs.push_back(hostTelToComPair);
            }
        }

        RETURN_IF_CANCELLATION_REQUESTED;

        {
            for (std::set<boost::filesystem::path>::iterator itr = s_hostLogFilesPattern.begin(); itr != s_hostLogFilesPattern.end(); itr++)
            {
                PICollForwPair hostAgentLogToComPair; // Will enqueue nullptr, if the below pair creation fails.
                try
                {
                    typedef std::vector<std::string> strVect;

                    PFilePollCollector coll = boost::make_shared<FilePollCollector>(
                        s_fileCxTransport, *itr, V2ASourceFileAttributes::DestFileExt,
                        funptr);

                    hostAgentLogToComPair = CollForwPair<strVect, strVect, strVect>::MakePair(coll, comForw);
                }
                CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                    GENERIC_CATCH_LOG_IGNORE("Creating Agent log file poller - MARS uploader pair");
                collForwPairs.push_back(hostAgentLogToComPair);
            }
        }

        RETURN_IF_CANCELLATION_REQUESTED;

        {
            PICollForwPair psLogsToComPair; // Will enqueue nullptr, if the below pair creation fails.
            try
            {
                typedef std::vector<std::string> strVect;

                PFilePollCollector coll = boost::make_shared<FilePollCollector>(
                    s_fileCxTransport, s_psLogFilesPattern, V2AProcessServerFileAttributes::DestFileExt,
                    boost::function1<bool, const std::string&>());

                psLogsToComPair = CollForwPair<strVect, strVect, strVect>::MakePair(coll, comForw);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Creating ProcessServer log file poller - MARS uploader pair");
            collForwPairs.push_back(psLogsToComPair);
        }

        RETURN_IF_CANCELLATION_REQUESTED;

        {
            PICollForwPair psMonitorLogsToComPair; // Will enqueue nullptr, if the below pair creation fails.
            try
            {
                typedef std::vector<std::string> strVect;

                PFilePollCollector coll = boost::make_shared<FilePollCollector>(
                    s_fileCxTransport, s_psMonitorLogFilesPattern, V2AProcessServerFileAttributes::DestFileExt,
                    boost::function1<bool, const std::string&>());

                psMonitorLogsToComPair = CollForwPair<strVect, strVect, strVect>::MakePair(coll, comForw);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Creating ProcessServerMonitor log file poller - MARS uploader pair");
            collForwPairs.push_back(psMonitorLogsToComPair);
        }

        RETURN_IF_CANCELLATION_REQUESTED;


#endif // SV_WINDOWS
    }

    static void A2ALogCallback(unsigned int logLevel, const char *msg)
    {
        SV_LOG_LEVEL evcfLogLevel;

        switch (logLevel)
        {
        case SV_LOG_ALWAYS:
            evcfLogLevel = SV_LOG_ALWAYS;
            break;
        case SV_LOG_FATAL:
        case SV_LOG_SEVERE:
        case SV_LOG_ERROR:
            evcfLogLevel = EvCF_LOG_ERROR;
            break;
        case SV_LOG_WARNING:
            evcfLogLevel = EvCF_LOG_WARNING;
            break;
        case SV_LOG_INFO:
            evcfLogLevel = EvCF_LOG_INFO;
            break;
        case SV_LOG_DEBUG:
        default:
            evcfLogLevel = EvCF_LOG_DEBUG;
            break;
        }

        DebugPrintf(evcfLogLevel, msg);
    }

    void A2ASourceRunnerFactory::InitializeEnvironment(
        const CmdLineSettings &settings, std::vector < boost::shared_ptr < void > > &uninitializers)
    {
        BOOST_ASSERT(lc);

        {
            Trace::Init("",
                (LogLevel)lc->getLogLevel(),
                boost::bind(&A2ALogCallback, _1, _2));
        }

        {
            AzureStorageRest::HttpClient::GlobalInitialize();
            AzureStorageRest::HttpClient::SetRetryPolicy(0);

            uninitializers.insert(uninitializers.begin(), boost::shared_ptr<void>(
                static_cast<void*>(0), boost::bind(AzureStorageRest::HttpClient::GlobalUnInitialize)));
        }
    }

    void A2ASourceRunnerFactory::GetCollForwPairs(
        std::vector<PICollForwPair> &collForwPairs,
        boost::function0<bool> cancelRequested
    )
    {
        using namespace RcmClientLib;

        collForwPairs.clear();
        BOOST_ASSERT(lc);

        RETURN_IF_CANCELLATION_REQUESTED;

        std::map<std::string, std::string> sourceAgentLogsMap;
        sourceAgentLogsMap["svagents.log"] = MonitoringMessageType::AGENT_LOG_ADMIN_EVENT;
        sourceAgentLogsMap["s2.log"] = MonitoringMessageType::AGENT_LOG_ADMIN_EVENT;
        sourceAgentLogsMap["appservice.log"] = MonitoringMessageType::AGENT_LOG_ADMIN_EVENT;
        sourceAgentLogsMap["srcTelemetry.log"] = MonitoringMessageType::AGENT_TELEMETRY_SOURCE;
        sourceAgentLogsMap["tagTelemetry.log"] = MonitoringMessageType::AGENT_TELEMETRY_VACP;
        sourceAgentLogsMap["schedulerTelemetry.log"] = MonitoringMessageType::AGENT_TELEMETRY_SCHEDULER;

        // The string will be converted into Base64 encoded string before posting in Service Bus queue.
        // TODO-SanKumar-1704: Probably the RcmClientImpl should have a getter or const defined for this,
        // instead of us assuming the implementation.
        const std::string::size_type MaxServiceBusMessageSize =
            HttpRestUtil::ServiceBusConstants::StandardSBMessageInBase64MaxSize;

        const std::string &MessageSource = RcmClientLib::MonitoringMessageSource::SOURCE_AGENT;
        const std::string &ComponentId = RcmClientLib::RcmComponentId::SOURCE_AGENT;

        // In A2A, RCM expects a JSON "without" the surrounding { Map : {Actual Data} }
        const bool RemoveMapKeyName = true;

#ifdef SV_WINDOWS
        {
            PICollForwPair inDskfltEvtLog; // Will enqueue nullptr, if the below pair creation fails.
            try
            {
                std::string queryName;
                std::wstring queryStr;
                GetSourceAdminLogsQuery(queryName, queryStr);

                boost::shared_ptr<EventLogCollector> coll = boost::make_shared<EventLogCollector>(queryName, queryStr);

                RETURN_IF_CANCELLATION_REQUESTED;

                boost::shared_ptr<EventLogToMdsLogConverter> conv = boost::make_shared<EventLogToMdsLogConverter>();
                conv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), ComponentId, RemoveMapKeyName);

                boost::shared_ptr<RcmServiceBusForwarder<boost::shared_ptr<EventLogRecord>>> forw =
                    boost::make_shared<RcmServiceBusForwarder<boost::shared_ptr<EventLogRecord>>>(
                        MaxServiceBusMessageSize, MonitoringMessageType::AGENT_LOG_ADMIN_EVENT, MessageSource);

                inDskfltEvtLog = CollForwPair<boost::shared_ptr<EventLogRecord>, std::string, boost::shared_ptr<EventLogRecord>>::MakePair(coll, forw, conv);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Creating indskflt driver eventlog - RCM SB Uploader");

            collForwPairs.push_back(inDskfltEvtLog);
        }

        RETURN_IF_CANCELLATION_REQUESTED;

        {
            PICollForwPair inDskfltTelemetry; // Will enqueue nullptr, if the below pair creation fails.
            try
            {
                std::string queryName;
                std::wstring queryStr;
                GetInDskFltTelemetryQuery(queryName, queryStr);

                boost::shared_ptr<EventLogTelemetryCollector> coll =
                    boost::make_shared<EventLogTelemetryCollector>(queryName, queryStr);

                RETURN_IF_CANCELLATION_REQUESTED;

                boost::shared_ptr<EventLogTelemetryToMdsConverter> conv = boost::make_shared<EventLogTelemetryToMdsConverter>();
                conv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), RemoveMapKeyName);

                boost::shared_ptr<RcmServiceBusForwarder<boost::shared_ptr<EventLogRecord>>> forw =
                    boost::make_shared<RcmServiceBusForwarder<boost::shared_ptr<EventLogRecord>>>(
                        MaxServiceBusMessageSize, MonitoringMessageType::AGENT_TELEMETRY_INDSKFLT, MessageSource);

                inDskfltTelemetry = CollForwPair<std::map<std::string, std::string>, std::string, boost::shared_ptr<EventLogRecord>>::MakePair(coll, forw, conv);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Creating indskflt driver telemetry - RCM SB Uploader");

            collForwPairs.push_back(inDskfltTelemetry);
        }
#endif // SV_WINDOWS

        RETURN_IF_CANCELLATION_REQUESTED;

#ifdef SV_UNIX
        {
            PICollForwPair inVolFltLinuxTelemetry; // Will enqueue nullptr, if the below pair creation fails.
            try
            {
                // TODO-SanKumar-1704: Move the file name to Config.
                const std::string inVolFltLinuxTelemetryPath =
                    (boost::filesystem::path(lc->getLogPathname()) / "involflt_telemetry.log").string();

                boost::shared_ptr< InVolFltLinuxTelemetryLoggerReader < LoggerReaderBookmark > > inVolFltLinuxTelemetryReader =
                    boost::make_shared< InVolFltLinuxTelemetryLoggerReader < LoggerReaderBookmark > >(inVolFltLinuxTelemetryPath);

                boost::shared_ptr< InMageLoggerCollector < LoggerReaderBookmark > > coll =
                    boost::make_shared< InMageLoggerCollector < LoggerReaderBookmark > >(inVolFltLinuxTelemetryReader);

                RETURN_IF_CANCELLATION_REQUESTED;

                boost::shared_ptr<InVolFltLinuxTelemetryJsonMdsConverter> conv = boost::make_shared<InVolFltLinuxTelemetryJsonMdsConverter>();
                conv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), RemoveMapKeyName);

                boost::shared_ptr < RcmServiceBusForwarder < LoggerReaderBookmark > > forw =
                    boost::make_shared < RcmServiceBusForwarder < LoggerReaderBookmark > >(
                        MaxServiceBusMessageSize, MonitoringMessageType::AGENT_TELEMETRY_INDSKFLT, MessageSource);

                inVolFltLinuxTelemetry = CollForwPair<std::string, std::string, LoggerReaderBookmark>::MakePair(coll, forw, conv);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Creating involflt linux driver telemetry - RCM SB Uploader");

            collForwPairs.push_back(inVolFltLinuxTelemetry);
        }
#endif // SV_UNIX

        // Create pairs for each of the agent binary logs and telemetry tables.
        for (std::map<std::string, std::string>::const_iterator itr = sourceAgentLogsMap.begin(); itr != sourceAgentLogsMap.end(); itr++)
        {
            RETURN_IF_CANCELLATION_REQUESTED;

            // TODO-SanKumar-1702: Read CSV from configuration and initialize one pair per each table.
            PICollForwPair sourceAgentLog; // Will enqueue nullptr, if the below pair creation fails.
            try
            {
                // TODO-SanKumar-1704: We qualify if the table name contains Telemetry as telemetry
                // table. The dictionary should be extended to contain a detailed object that would
                // say if it's a telemetry table or not and other factors right inside it.
                const std::string &messageType = itr->second;
                bool isTelemetryTable = boost::icontains(messageType, "Telemetry");
                const std::string sourceLogPath = (boost::filesystem::path(lc->getLogPathname()) / itr->first).string();

                boost::shared_ptr< AgentLoggerReader <LoggerReaderBookmark> > reader = boost::make_shared< AgentLoggerReader <LoggerReaderBookmark> >(sourceLogPath);
                boost::shared_ptr< InMageLoggerCollector <LoggerReaderBookmark> > coll = boost::make_shared<InMageLoggerCollector <LoggerReaderBookmark> >(reader);

                RETURN_IF_CANCELLATION_REQUESTED;

                const SV_LOG_LEVEL MarsForwPutLogLevel = isTelemetryTable ? SV_LOG_ALWAYS : lc->getEvtCollForwAgentLogPostLevel();

                boost::shared_ptr < IConverter < std::string, std::string > > conv;
                if (isTelemetryTable)
                {
                    boost::shared_ptr< InMageLoggerTelemetryToMdsConverter > teleConv =
                        boost::make_shared< InMageLoggerTelemetryToMdsConverter >(MarsForwPutLogLevel);

                    teleConv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), RemoveMapKeyName);
                    conv = teleConv;
                }
                else
                {
                    std::string subComponentName = boost::filesystem::path(itr->first).stem().string();

                    boost::shared_ptr< InMageLoggerToMdsLogConverter > logConv =
                        boost::make_shared< InMageLoggerToMdsLogConverter >(MarsForwPutLogLevel, subComponentName);

                    logConv->Initialize(GetFabricType(), GetBiosId(), GetHostId(), ComponentId::SOURCE_AGENT, RemoveMapKeyName);
                    conv = logConv;
                }

                boost::shared_ptr < RcmServiceBusForwarder < LoggerReaderBookmark > > forw =
                    boost::make_shared < RcmServiceBusForwarder < LoggerReaderBookmark > >(
                        MaxServiceBusMessageSize, messageType, MessageSource);

                sourceAgentLog = CollForwPair<std::string, std::string, LoggerReaderBookmark>::MakePair(coll, forw, conv);
            }
            CATCH_LOG_AND_RETHROW_ON_NO_MEMORY()
                GENERIC_CATCH_LOG_IGNORE("Creating source logger telemetry reader - RCM SB Uploader");

            collForwPairs.push_back(sourceAgentLog);
        }
    }
}