#pragma once
#include "HttpClient.h"
#include "EvtCollForwCommon.h"

using namespace AzureStorageRest;
namespace EvtCollForw
{
    //Event hub constants
    const std::string HttpsPrefix = "https";
    const std::string Messages = "messages";
    const std::string Authorization = "Authorization";
    const std::string Content_Type = "Content-Type";
    const std::string Host = "Host";
    const std::string Table = "Table";
    const std::string IngestionMappingReference = "IngestionMappingReference";
    const std::string Format = "Format";
    const std::string Form_encoded = "application/x-www-form-urlencoded";
    const std::string Json = "json";
    const std::string Publisher = "publishers";
    const long EventHubMaxFileSize = 200 * 1024;

    class EventHubRestHelper
    {
    public:
        explicit EventHubRestHelper(const std::string& environment);
        bool UploadFile(const std::string & filepath);
        bool UploadData(const std::string & date, const std::string& tablename);
    private:
        void GetEventHubSasFromSettings(const std::string& environment);
        void GetEventHubSasFromSettingsOnPS();
        void GetEventHubSasFromSettingsOnSource();
        bool ParseSasUri();
        bool ParseFilename(const std::string & filepath);
        bool UploadFileData(const std::string & filepath);
        bool UploadBlocks(const std::string &block);
        void InitTablesAndIngestionMappings();
        std::string m_TelemetryChannelUri;
        std::string m_EventHubSasUri;
        std::string m_HostNamespace;
        std::string m_ContentType;
        std::string m_TableName;
        std::string m_IngestionMapping;
        std::string m_DataFormat;
        std::string m_EventHubName;
        std::string m_EventHubUri;
        std::string m_PublisherId;
        std::map<std::string, std::string> TableIngestionMapping;
        boost::shared_ptr<HttpClient> m_httpClientPtr;
    };
}
