#include"EventHubRestHelper.h"
#include "pssettingsconfigurator.h"

namespace EvtCollForw
{
    EventHubRestHelper::EventHubRestHelper(const std::string& environment)
        :m_httpClientPtr(new HttpClient())
    {
        GetEventHubSasFromSettings(environment);
        if (!ParseSasUri())
            throw std::runtime_error("Event hub sas uri is not in expected format");
        InitTablesAndIngestionMappings();
    }

    void EventHubRestHelper::GetEventHubSasFromSettings(const std::string& environment)
    {
        if (environment == EvtCollForw_Env_V2ARcmSourceOnAzure)
        {
            GetEventHubSasFromSettingsOnSource();
        }
        else if (environment == EvtCollForw_Env_V2ARcmPS)
        {
            GetEventHubSasFromSettingsOnPS();
        }
        else
        {
            DebugPrintf(EvCF_LOG_ERROR, "%s: unsupported environment %s\n", FUNCTION_NAME, environment.c_str());
            throw std::runtime_error("Unsupported environment");
        }
    }

    void EventHubRestHelper::GetEventHubSasFromSettingsOnPS()
    {
        PSSettings::PSSettingsPtr pssettingsptr = PSSettings::PSSettingsConfigurator::GetInstance().GetPSSettings();
        if (pssettingsptr.get() == NULL)
        {
            throw std::runtime_error("Failed to fetch PS settings, or the settings are null.");
        }
        BOOST_ASSERT(!pssettingsptr->TelemetryChannelUri.empty());
        m_TelemetryChannelUri = pssettingsptr->TelemetryChannelUri;
        if (m_TelemetryChannelUri.empty())
        {
            throw std::runtime_error("PS Settings doesn't contain sas uri.");
        }
    }

    void EventHubRestHelper::GetEventHubSasFromSettingsOnSource()
    {
        m_TelemetryChannelUri = RcmClientLib::RcmConfigurator::getInstance()->GetTelemetrySasUriForAzureToOnPrem();
        if (m_TelemetryChannelUri.empty())
        {
            throw std::runtime_error("Source agent telemetry settings doesn't contain sas uri.");
        }
    }

    bool EventHubRestHelper::ParseSasUri()
    {
        const std::string keyEndpoint("Endpoint=sb");
        const std::string keySas("SharedAccessSignature=");
        const std::string keyEntityPath("EntityPath=");
        const std::string slash("/");
        const std::string keyhttps("https://");
        const std::string keyPublishers("Publisher=");

        boost::char_separator<char> delm(";");
        boost::tokenizer < boost::char_separator<char> > strtokens(m_TelemetryChannelUri, delm);
        std::string errMsg;

        std::vector<std::string> tokens;
        BOOST_FOREACH(std::string t, strtokens)
        {
            tokens.push_back(t);
            DebugPrintf(EvCF_LOG_DEBUG, "%s: %s\n", FUNCTION_NAME, t.c_str());
        }

        if (tokens.size() != 4)
        {
            errMsg += " tokens size mismatch expected 4 but found ";
            errMsg += boost::lexical_cast<std::string>(tokens.size());
            DebugPrintf(EvCF_LOG_ERROR, "%s: %s.\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }

        std::string endPointUri(tokens[0]);
        size_t keyEndpointPos = endPointUri.find(keyEndpoint);
        if (keyEndpointPos == std::string::npos)
        {
            errMsg += "  endpoint not found.";
            DebugPrintf(EvCF_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }
        BOOST_ASSERT(keyEndpointPos == 0);
        if (keyEndpointPos != 0)
        {
            errMsg += "Endpoint is not in expected format.";
            DebugPrintf(EvCF_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }
        endPointUri.replace(keyEndpointPos, keyEndpoint.length(), HttpsPrefix);
        DebugPrintf(EvCF_LOG_DEBUG, "%s: Endpoint %s\n", FUNCTION_NAME, endPointUri.c_str());

        std::string hostNamespace(endPointUri);
        size_t keyHttpsPos = hostNamespace.find(keyhttps);
        if (keyHttpsPos == std::string::npos)
        {
            errMsg += " https key not found.";
            DebugPrintf(EvCF_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }
        BOOST_ASSERT(keyHttpsPos == 0);
        if (keyHttpsPos != 0)
        {
            errMsg += "Endpoint does not contain https.";
            DebugPrintf(EvCF_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }
        hostNamespace.erase(keyHttpsPos, keyhttps.length());
        boost::trim_right_if(hostNamespace, boost::is_any_of("/"));
        DebugPrintf(EvCF_LOG_DEBUG, "%s: Hostnamespace:  %s\n", FUNCTION_NAME, hostNamespace.c_str());

        std::string sasToken(tokens[1]);
        size_t keySasPos = sasToken.find(keySas);
        if (keySasPos == std::string::npos)
        {
            errMsg += " SAS key not found.";
            DebugPrintf(EvCF_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }
        BOOST_ASSERT(keySasPos == 0);
        if (keySasPos != 0)
        {
            errMsg += "SAS Uri is not in expected format.";
            DebugPrintf(EvCF_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }
        sasToken.erase(keySasPos, keySas.length());
        DebugPrintf(EvCF_LOG_DEBUG, "%s: SAS %s\n", FUNCTION_NAME, sasToken.c_str());

        std::string entityName(tokens[2]);
        size_t keyEntityPos = entityName.find(keyEntityPath);
        if (keyEntityPos == std::string::npos)
        {
            errMsg += " Entity name not found.";
            DebugPrintf(EvCF_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }
        BOOST_ASSERT(keyEntityPos == 0);
        if (keyEntityPos != 0)
        {
            errMsg += "EntityPath is not in expected format.";
            DebugPrintf(EvCF_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }
        entityName.erase(keyEntityPos, keyEntityPath.length());
        DebugPrintf(EvCF_LOG_DEBUG, "%s: Event hub name %s\n", FUNCTION_NAME, entityName.c_str());

        std::string publisher(tokens[3]);
        size_t keyPublisherPos = publisher.find(keyPublishers);
        if (keyPublisherPos == std::string::npos)
        {
            errMsg += " Publisher not found.";
            DebugPrintf(EvCF_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }
        BOOST_ASSERT(keyPublisherPos == 0);
        if (keyPublisherPos != 0)
        {
            errMsg += " Publisher is not in expected format.";
            DebugPrintf(EvCF_LOG_ERROR, "%s: %s\n", FUNCTION_NAME, errMsg.c_str());
            return false;
        }
        publisher.erase(keyPublisherPos, keyPublishers.length());
        DebugPrintf(EvCF_LOG_DEBUG, "%s: Publisher %s\n", FUNCTION_NAME, publisher.c_str());

        m_HostNamespace = hostNamespace;
        m_EventHubSasUri = sasToken;
        m_EventHubName = entityName;
        m_PublisherId = publisher;
        m_EventHubUri = endPointUri + m_EventHubName + slash + Publisher + slash + m_PublisherId + slash + Messages;
        DebugPrintf(EvCF_LOG_DEBUG, "%s: Event hub uri: %s\n", FUNCTION_NAME, m_EventHubUri.c_str());
        return true;
    }

    bool EventHubRestHelper::ParseFilename(const std::string &filepath)
    {
        boost::filesystem::path path(filepath);
        std::string filename = path.stem().string();
        std::string tableName = filename.substr(0, filename.find("_"));
        // remap table names from V2A tables to A2A tables/ CSPrime tables
        m_TableName = Utils::GetA2ATableNameFromV2ATableName(tableName);
        if (m_TableName.empty())
        {
            DebugPrintf(EvCF_LOG_ERROR, "No table found for the table name %s\n", tableName.c_str());
            return false;
        }

        if (TableIngestionMapping.find(tableName) != TableIngestionMapping.end())
        {
            // get ingestion mappings
            m_IngestionMapping = TableIngestionMapping[tableName];
        }
        else
        {
            DebugPrintf(EvCF_LOG_ERROR, "No mapping found for the table name %s\n", tableName.c_str());
            return false;
        }
        DebugPrintf(EvCF_LOG_DEBUG, "Table name and ingestion mapping for the file %s is %s and %s\n", filepath.c_str(), m_TableName.c_str(), m_IngestionMapping.c_str());
        return true;
    }

    bool EventHubRestHelper::UploadFileData(const std::string &filepath)
    {
        std::ifstream ifs;
        ifs.exceptions(ifstream::badbit);
        try
        {
            ifs.open(filepath.c_str(), ios::in);
            std::string currData = std::string();
            unsigned long currDataSize = 0;
            std::string currDataLine;
            while (!ifs.eof() && std::getline(ifs, currDataLine))
            {
                if (currDataLine.size() > EventHubMaxFileSize)
                {
                    // if this case happens then drop that log line
                    BOOST_ASSERT(false);
                    currDataLine.clear();
                    continue;
                }

                if (currDataSize + currDataLine.size() > EventHubMaxFileSize)
                {
                    if (!UploadBlocks(currData))
                    {
                        DebugPrintf(EvCF_LOG_ERROR, "%s: Failed to upload data from the file %s\n", FUNCTION_NAME, filepath.c_str());
                        return false;
                    }
                    currDataSize = 0;
                    currData.clear();
                }

                currData += currDataLine + "\n";
                currDataSize += currDataLine.size() + 1;        //adding 1 charaqcter size for \n
                currDataLine.clear();
            }
            if (!currData.empty())
            {
                if (!UploadBlocks(currData))
                {
                    DebugPrintf(EvCF_LOG_ERROR, "%s: Failed to upload data from the file %s\n", FUNCTION_NAME, filepath.c_str());
                    return false;
                }
            }
            return true;
        }
        GENERIC_CATCH_LOG_ACTION("Caught exception while uploading data from file.", return false);
    }

    bool EventHubRestHelper::UploadFile(const std::string &filepath)
    {
        if (!ParseFilename(filepath))
        {
            DebugPrintf(EvCF_LOG_ERROR, "%s: Failed to parse filepath %s. Deleting the file.\n", FUNCTION_NAME, filepath.c_str());
            Utils::TryDeleteFile(filepath);
            return false;
        }
        if (!UploadFileData(filepath))
        {
            DebugPrintf(EvCF_LOG_ERROR, "Failed to upload file %s\n", filepath.c_str());
            return false;
        }
        return true;
    }

    bool EventHubRestHelper::UploadData(const std::string& data, const std::string& tableName)
    {
        if (data.size() > EventHubMaxFileSize)
        {
            // if this case happens then drop that log line
            BOOST_ASSERT(false);
            return false;
        }

        // remap table names from V2A tables to A2A tables/ CSPrime tables
        m_TableName = Utils::GetA2ATableNameFromV2ATableName(tableName);
        if (m_TableName.empty())
        {
            DebugPrintf(EvCF_LOG_ERROR, "No table found for the table name %s\n", tableName.c_str());
            return false;
        }

        if (TableIngestionMapping.find(tableName) != TableIngestionMapping.end())
        {
            // get ingestion mappings
            m_IngestionMapping = TableIngestionMapping[tableName];
        }
        else
        {
            DebugPrintf(EvCF_LOG_ERROR, "No kusto ingestion mapping found for the table name %s\n", tableName.c_str());
            return false;
        }

        DebugPrintf(EvCF_LOG_DEBUG, "Table name and ingestion mapping is %s and %s\n", m_TableName.c_str(), m_IngestionMapping.c_str());

        if (!data.empty())
        {
            if (!UploadBlocks(data))
            {
                DebugPrintf(EvCF_LOG_ERROR, "%s: Failed to upload data of size %d\n", FUNCTION_NAME, data.size());
                return false;
            }
        }
    }

    bool EventHubRestHelper::UploadBlocks(const std::string &datablock)
    {
        assert(m_httpClientPtr);

        AzureStorageRest::HttpRequest request(m_EventHubUri);
        request.AddHeader(Authorization, m_EventHubSasUri);
        request.AddHeader(Content_Type, Form_encoded);
        request.AddHeader(Host, m_HostNamespace);
        request.AddHeader(Table, m_TableName);
        request.AddHeader(IngestionMappingReference, m_IngestionMapping);
        request.AddHeader(Format, Json);
        request.SetHttpMethod(AzureStorageRest::HTTP_POST);

        request.SetRequestBody((unsigned char *)&datablock[0], datablock.size());

        AzureStorageRest::HttpResponse response;
        int strikes = 0;
        while (m_httpClientPtr->GetResponse(request, response) && strikes < 3)
        {
            strikes++;
            long responseCode = response.GetResponseCode();
            if (responseCode != AzureStorageRest::HttpErrorCode::CREATED)
            {
                DebugPrintf(EvCF_LOG_ERROR, "Post failed with response code %ld and response data %s\n", responseCode, response.GetResponseData().c_str());
            }
            else
            {
                DebugPrintf(EvCF_LOG_DEBUG, "Post succeeded for size %d\n", datablock.size());
                return true;
            }
        }
        return false;
    }

    void EventHubRestHelper::InitTablesAndIngestionMappings()
    {
        // Initialize mapping from table to ingestion mapping
        TableIngestionMapping[V2ATableNames::AdminLogs] = A2AMdsTableMapping::AdminLogs;
        TableIngestionMapping[V2ATableNames::DriverTelemetry] = A2AMdsTableMapping::DriverTelemetry;
        TableIngestionMapping[V2ATableNames::SourceTelemetry] = A2AMdsTableMapping::SourceTelemetry;
        TableIngestionMapping[V2ATableNames::Vacp] = A2AMdsTableMapping::Vacp;
        TableIngestionMapping[V2ATableNames::PSV2] = A2AMdsTableMapping::PSV2;
        TableIngestionMapping[V2ATableNames::PSTransport] = A2AMdsTableMapping::PSTransport;
        TableIngestionMapping[V2ATableNames::AgentSetupLogs] = A2AMdsTableMapping::AgentSetupLogs;
    }
}
