#ifndef EVTCOLLFORW_RUNNER_FACTORIES_H
#define EVTCOLLFORW_RUNNER_FACTORIES_H

#include "configurator2.h"
#include "synchronize.h"

#include "Runners.h"

namespace EvtCollForw
{
    namespace ComponentId
    {
        const std::string SOURCE_AGENT = "SourceAgent";
        const std::string GATEWAY_SERVICE = "GatewayService";
        const std::string PROCESS_SERVER = "ProcessServer";
        const std::string MASTER_TARGET = "MasterTarget";
    }

    namespace V2ASourceFileAttributes // TODO: nichougu - make this V2AFileAttributes
    {
        static const char *DestTelemetryFolder = "telemetry";
        static const char *DestLogsFolder = "logs";
        static const char *DestFileExt = ".json";
    }

    

    class IRunnerFactory : boost::noncopyable
    {
    public:
        virtual ~IRunnerFactory() {}

        virtual boost::shared_ptr<IRunner> GetRunner(boost::function0<bool> cancelRequested) = 0;
    };

    class SerializedRunnerFactory : public IRunnerFactory, public sigslot::has_slots<>
    {
    public:
        SerializedRunnerFactory(const CmdLineSettings &cmdLineSettings);

        virtual ~SerializedRunnerFactory();

        boost::shared_ptr<IRunner> GetRunner(boost::function0<bool> cancelRequested);

        bool GetTransportConnectionSettings(TRANSPORT_CONNECTION_SETTINGS& transportSettings);

        std::string GetFabricType() const { return m_fabricType; }
        std::string GetBiosId() const { return m_biosId; }
        std::string GetHostId() const { return m_hostId; }

    protected:
        // The pairs of events that have to be forwarded first must be prioritized
        // (occur before low prior pair) in the list.
        virtual void GetCollForwPairs(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested) = 0;

	protected:
		const CmdLineSettings &m_cmdLineSettings;

    private:
        std::string m_fabricType, m_biosId, m_hostId;
    };

    class V2ASourceRunnerFactoryBase : public SerializedRunnerFactory
    {
    public:
        V2ASourceRunnerFactoryBase(const CmdLineSettings &cmdLineSettings) : SerializedRunnerFactory(cmdLineSettings){};
        virtual ~V2ASourceRunnerFactoryBase() {};

        // TODO-nichougu: code optimization - move InitializeEnvironment to SerializedRunnerFactory
        static void InitializeEnvironment(
            const CmdLineSettings &settings, std::vector < boost::shared_ptr <void > > &uninitializers);

    protected:
        virtual void GetCollForwPairs(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested) = 0;
    };

    class V2ASourceRunnerFactory : public V2ASourceRunnerFactoryBase
    {
    public:
        V2ASourceRunnerFactory(const CmdLineSettings &cmdLineSettings) : V2ASourceRunnerFactoryBase(cmdLineSettings) {};
        virtual ~V2ASourceRunnerFactory() {};

    protected:
        void GetCollForwPairs(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested);
    };

    class V2ARcmSourceRunnerFactory : public V2ASourceRunnerFactoryBase
    {
    public:
        V2ARcmSourceRunnerFactory(const CmdLineSettings &cmdLineSettings) : V2ASourceRunnerFactoryBase(cmdLineSettings) {};
        virtual ~V2ARcmSourceRunnerFactory() {};

    protected:
        void GetCollForwPairs(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested);
    };

    class V2ARcmSourceOnAzureRunnerFactory : public SerializedRunnerFactory
    {
    public:
        V2ARcmSourceOnAzureRunnerFactory(const CmdLineSettings &cmdLineSettings) : SerializedRunnerFactory(cmdLineSettings) {};
        virtual ~V2ARcmSourceOnAzureRunnerFactory() {};

        static void InitializeEnvironment(
            const CmdLineSettings &settings, std::vector < boost::shared_ptr < void > > &uninitializers);

    protected:
        void GetCollForwPairs(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested);
    };

    class V2AProcessServerRunnerFactoryBase : public SerializedRunnerFactory
    {
    public:
        V2AProcessServerRunnerFactoryBase(const CmdLineSettings &cmdLineSettings) : SerializedRunnerFactory(cmdLineSettings){};
        virtual ~V2AProcessServerRunnerFactoryBase() {}

        static void InitializeEnvironment(
            const CmdLineSettings &settings, std::vector < boost::shared_ptr < void > > &uninitializers);

    protected:
        void GetCollForwPairs(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested) = 0;
        void GetCommonCollForwPairs(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested,
            bool RemoveMapKeyName, bool isStreamData, boost::shared_ptr<ForwarderBase< std::vector<std::string >, std::vector <std::string> > > &comForw,
            boost::function1<bool, const std::string&> &funptr);

        static boost::filesystem::path  s_psEvtLogFolderPath;
        static boost::filesystem::path s_psTelFolderPath;
        static boost::filesystem::path s_cxpsTelFolderPath;
        static std::set<boost::filesystem::path> s_hostTelFilesPattern;
        static std::set<boost::filesystem::path> s_hostLogFilesPattern;
        static boost::filesystem::path s_psLogFilesPattern;
        static boost::filesystem::path s_psMonitorLogFilesPattern;

        static boost::shared_ptr<CxTransport> s_fileCxTransport;

        static bool ValidateHostSourceFolder(const std::string &fullPath);
    };

    class V2AProcessServerRunnerFactory : public V2AProcessServerRunnerFactoryBase
    {
    public:
        V2AProcessServerRunnerFactory(const CmdLineSettings &cmdLineSettings) : V2AProcessServerRunnerFactoryBase(cmdLineSettings) {};
        virtual ~V2AProcessServerRunnerFactory() {}

        static void InitializeEnvironment(
            const CmdLineSettings &settings, std::vector < boost::shared_ptr < void > > &uninitializers);

    protected:
        void GetCollForwPairs(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested);

        static bool ValidateHostSourceFolder(const std::string &fullPath);
    };

    class V2ARcmProcessServerRunnerFactory : public V2AProcessServerRunnerFactoryBase
    {
    public:
        V2ARcmProcessServerRunnerFactory(const CmdLineSettings &cmdLineSettings) : V2AProcessServerRunnerFactoryBase(cmdLineSettings) {};
        virtual ~V2ARcmProcessServerRunnerFactory() {}

        static void InitializeEnvironment(
            const CmdLineSettings &settings, std::vector < boost::shared_ptr < void > > &uninitializers);

    protected:
        void GetCollForwPairs(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested);

        static bool ValidateHostSourceFolder(const std::string &fullPath);

    private:
        static boost::filesystem::path s_rcmPSConfLogFilesPattern;
        static boost::filesystem::path s_rcmPSAgentSetupLogFilesDestPattern;
        static std::set<boost::filesystem::path> s_rcmPSAgentSetupLogFilesPattern;
    };

    class A2ASourceRunnerFactory : public SerializedRunnerFactory
    {
    public:
        A2ASourceRunnerFactory(const CmdLineSettings &cmdLineSettings) : SerializedRunnerFactory(cmdLineSettings) {};
        virtual ~A2ASourceRunnerFactory() {}

        static void InitializeEnvironment(
            const CmdLineSettings &settings, std::vector < boost::shared_ptr < void > > &uninitializers);

    protected:
        void GetCollForwPairs(std::vector<PICollForwPair> &collForwPairs, boost::function0<bool> cancelRequested);
    };
}

#endif // !EVTCOLLFORW_RUNNER_FACTORIES_H
