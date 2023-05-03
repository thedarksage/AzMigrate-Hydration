#ifndef EVTCOLLFORW_TRACE_PROCESSORS_H
#define EVTCOLLFORW_TRACE_PROCESSORS_H

#include <iostream>
#include <string.h>

#include "EvtCollForwCommon.h"
#include "CollForwPair.h"
#include "RunnerFactories.h"
#include "cxps.h"
#include "RcmConfigurator.h"

extern boost::shared_ptr<LocalConfigurator> lc;

namespace EvtCollForw
{
    class TraceProcessor
    {
    public:
        //TODO-nichougu: Can implement with an approach of Get() return respective TraceProcessor,
        //  which may used to call Process() of each TraceProcessor instead calling CollectAndForward from SerializedRunner.
        //  Useful if design changed to use OOP approach instead of template based - cleaner, avoid code duplication approach.
        // virtual void Prepare() = 0;
        // virtual void Get() = 0;
        // virtual void Process() = 0;

        virtual bool GetCollForwPair(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested) = 0;

        virtual std::string GetSubComponentName(const std::string &logFilePath) const = 0;

        virtual std::string GetComponentId() = 0;

        virtual ~TraceProcessor() {}
    };

    class V2ATraceProcessor : public TraceProcessor // TODO: 1902 - make V2ALogTraceProcessor
    {
    public:
        V2ATraceProcessor(SerializedRunnerFactory* pSerRunnFactory, const CmdLineSettings & setting) :
            m_pSerRunnFactory(pSerRunnFactory),
            m_setting(setting)

        {

            BOOST_ASSERT(lc);

            try
            {
                m_destFilePrefix = lc->getDestDir();
                m_destFilePrefix /= m_pSerRunnFactory->GetHostId();
                m_destFilePrefix /= V2ASourceFileAttributes::DestLogsFolder;
                m_destFilePrefix /= V2AMdsTableNames::AdminLogs; // MDS Table name - file prefix.

                m_marsForwPutLogLevel = lc->getEvtCollForwAgentLogPostLevel();

                // In V2A, MARS expects a JSON "with" the surrounding { Map : {Actual Data} }
                // This is the default value. For Rcm stack components, this value must be changed to true explicitly in derived class.
                m_removeMapKeyName = false;

                m_cxTransportMaxAttempts = lc->getEvtCollForwCxTransportMaxAttempts();
            }
            catch (const std::exception &exc)
            {   
                DebugPrintf(EvCF_LOG_ERROR, "%s: Caught exception %s.\n", FUNCTION_NAME, exc.what());
                throw;
            }
            catch (...)
            {
                DebugPrintf(EvCF_LOG_ERROR, "%s: Caught an unknown exception.\n", FUNCTION_NAME);
                throw;
            }
        }

        virtual bool GetCollForwPair(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested);

        virtual void UpdateFilesToProcessSet(const std::string& LOG_FORMAT = std::string()) = 0;

        virtual std::string GetSubComponentName(const std::string &logFilePath) const
        {
            return boost::filesystem::path(logFilePath).stem().string();
        }

        virtual ~V2ATraceProcessor() {}

    protected:
        std::set<std::string> m_filesToProcessSet;
        SerializedRunnerFactory* m_pSerRunnFactory;
        boost::filesystem::path m_destFilePrefix;
        SV_LOG_LEVEL m_marsForwPutLogLevel;
        bool m_removeMapKeyName;
        int m_cxTransportMaxAttempts;
        const CmdLineSettings & m_setting;
    };

    class IRTraceProcessor : public V2ATraceProcessor // TODO: nichougu - make IRTraceProcessor standalone interface
    {
    public:
        IRTraceProcessor(SerializedRunnerFactory* pSerRunnFactory, const CmdLineSettings & setting)
            : V2ATraceProcessor(pSerRunnFactory, setting) {}

    protected:
        virtual void UpdateFilesToProcessSet(const std::string& LOG_FORMAT = std::string());
    };

    class IRSourceTraceProcessor : public IRTraceProcessor // TODO: nichougu - derive from SourceTraceProcessor and IRTraceProcessor
    {
    public:
        IRSourceTraceProcessor(SerializedRunnerFactory* pSerRunnFactory, const CmdLineSettings & setting)
            : IRTraceProcessor(pSerRunnFactory, setting) {}

        virtual bool GetCollForwPair(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested);

        virtual std::string GetSubComponentName(const std::string &logFilePath) const
        {
            return std::string("dataprotection\\IR_Source");
        }

        virtual std::string GetComponentId()
        {
            return ComponentId::SOURCE_AGENT;
        }

        virtual ~IRSourceTraceProcessor() {}
    };

    class IRRcmSourceTraceProcessor : public IRTraceProcessor
    {
    public:
        IRRcmSourceTraceProcessor(SerializedRunnerFactory* pSerRunnFactory, const CmdLineSettings & setting)
            : IRTraceProcessor(pSerRunnFactory, setting) 
        {
            m_destFilePrefix.clear();
            m_destFilePrefix = RcmClientLib::RcmConfigurator::getInstance()->GetTelemetryFolderPathForOnPremToAzure();
            m_destFilePrefix /= V2ASourceFileAttributes::DestLogsFolder;
            m_destFilePrefix /= V2AMdsTableNames::AdminLogs; // MDS Table name - file prefix.
            m_removeMapKeyName = true;
        }

        virtual bool GetCollForwPair(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested);

        virtual std::string GetSubComponentName(const std::string &logFilePath) const
        {
            return std::string("dataprotectionSyncRcm\\IR_Source");
        }

        virtual std::string GetComponentId()
        {
            return ComponentId::SOURCE_AGENT;
        }

        virtual ~IRRcmSourceTraceProcessor() {}
    };

    class IRRcmSourceOnAzureTraceProcessor : public IRTraceProcessor
    {
    public:
        IRRcmSourceOnAzureTraceProcessor(SerializedRunnerFactory* pSerRunnFactory, const CmdLineSettings & setting)
            : IRTraceProcessor(pSerRunnFactory, setting)
        {
            m_destFilePrefix.clear();
            m_destFilePrefix = V2AMdsTableNames::AdminLogs; // MDS Table name - file prefix.
            m_removeMapKeyName = true;
        }

        virtual bool GetCollForwPair(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested);

        virtual std::string GetSubComponentName(const std::string &logFilePath) const
        {
            return std::string("dataprotectionSyncRcm\\IR_Source");
        }

        virtual std::string GetComponentId()
        {
            return ComponentId::SOURCE_AGENT;
        }

        virtual ~IRRcmSourceOnAzureTraceProcessor() {}
    };

    class IRMTTraceProcessor : public IRTraceProcessor // TODO: nichougu - derive from MTTraceProcessor and IRTraceProcessor
    {
    public:
        IRMTTraceProcessor(SerializedRunnerFactory* pSerRunnFactory, const CmdLineSettings & setting)
            : IRTraceProcessor(pSerRunnFactory, setting) {}

        virtual bool GetCollForwPair(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested);

        virtual std::string GetSubComponentName(const std::string &logFilePath) const
        {
            return std::string("dataprotection\\IR_MT");
        }

        virtual std::string GetComponentId()
        {
            return ComponentId::MASTER_TARGET;
        }

        virtual ~IRMTTraceProcessor() {}
    };

    class IRRcmMTTraceProcessor : public IRTraceProcessor // TODO: nichougu - derive from MTTraceProcessor and IRTraceProcessor
    {
    public:
        IRRcmMTTraceProcessor(SerializedRunnerFactory* pSerRunnFactory, const CmdLineSettings & setting)
            : IRTraceProcessor(pSerRunnFactory, setting) 
        {
            m_removeMapKeyName = true;
        }

        virtual bool GetCollForwPair(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested);

        virtual std::string GetSubComponentName(const std::string &logFilePath) const
        {
            return std::string("dataprotectionSyncRcm\\IR_MT");
        }

        virtual std::string GetComponentId()
        {
            return ComponentId::MASTER_TARGET;
        }

        virtual ~IRRcmMTTraceProcessor() {}
    };

    class MTTraceProcessor : public V2ATraceProcessor
    {
    public:
        MTTraceProcessor(SerializedRunnerFactory* pSerRunnFactory, const CmdLineSettings & setting)
            : V2ATraceProcessor(pSerRunnFactory, setting) {}

        virtual void UpdateFilesToProcessSet(const std::string& LOG_FORMAT = std::string());

        virtual std::string GetComponentId()
        {
            return ComponentId::MASTER_TARGET;
        }

        virtual ~MTTraceProcessor() {}
    };

    class DRMTTraceProcessor : public MTTraceProcessor
    {
    public:
        DRMTTraceProcessor(SerializedRunnerFactory* pSerRunnFactory, const CmdLineSettings & setting)
            : MTTraceProcessor(pSerRunnFactory, setting) {}

        virtual bool GetCollForwPair(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested);

        virtual void UpdateFilesToProcessSet(const std::string& LOG_FORMAT = std::string());

        virtual std::string GetSubComponentName(const std::string &logFilePath) const
        {
            return std::string("dataprotection\\DR_MT");
        }

        virtual ~DRMTTraceProcessor() {}
    };

}

#endif // !EVTCOLLFORW_TRACE_PROCESSORS_H