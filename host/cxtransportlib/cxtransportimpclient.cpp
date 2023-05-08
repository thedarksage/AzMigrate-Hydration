
///
///  \file cxtransportimpclient.cpp
///
///  \brief implements cx transport using cxpslib client
///

#include "cxtransportimpclient.h"
#include "localconfigurator.h"
#include "client.h"
#include "azureblobclient.h"
#include "errorexception.h"
#include "genpassphrase.h"
#include "defaultdirs.h"

const std::size_t g_maxDataPerGet(1024*1024*2); // 2MB

void CxTransportImpClient::resetClientImp()
{
    LocalConfigurator localConfigurator;

    switch (m_protocol) {
        case TRANSPORT_PROTOCOL_HTTP:
            if (m_secure) {
                if (m_useCfs) {
                    m_client.reset(new HttpsCfsClient_t(localConfigurator.getCfsLocalName(),
                                                        m_psId,
                                                        localConfigurator.getHostId(),
                                                        securitylib::getPassphrase(),
                                                        localConfigurator.getTransportMaxBufferSize(),
                                                        localConfigurator.getTransportConnectTimeoutSeconds(),
                                                        localConfigurator.getTransportResponseTimeoutSeconds(),
                                                        KEEP_ALIVE,
                                                        localConfigurator.getTcpSendWindowSize(),
                                                        localConfigurator.getTcpRecvWindowSize(),
                                                        std::string(),
                                                        localConfigurator.getTransportWriteMode()));
                } else {
                    if (boost::iequals(localConfigurator.getCSType(), CSTYPE_CSPRIME)) {
                        m_client.reset(new HttpsClient_t(m_settings.ipAddress,
                                                        m_settings.sslPort,
                                                        localConfigurator.getHostId(),
                                                        localConfigurator.getTransportMaxBufferSize(),
                                                        localConfigurator.getTransportConnectTimeoutSeconds(),
                                                        localConfigurator.getTransportResponseTimeoutSeconds(),
                                                        KEEP_ALIVE,
                                                        localConfigurator.getTcpSendWindowSize(),
                                                        localConfigurator.getTcpRecvWindowSize(),
                                                        securitylib::getSourceAgentClientCertPath(),
                                                        securitylib::getSourceAgentClientKeyPath(),
                                                        g_serverCertFingerprintMgr.getProcessServerCertFingerprint(),
                                                        localConfigurator.getTransportWriteMode()));
                    }
                    else {
                        m_client.reset(new HttpsClient_t(m_settings.ipAddress,
                                                        m_settings.sslPort,
                                                        localConfigurator.getHostId(),
                                                        localConfigurator.getTransportMaxBufferSize(),
                                                        localConfigurator.getTransportConnectTimeoutSeconds(),
                                                        localConfigurator.getTransportResponseTimeoutSeconds(),
                                                        KEEP_ALIVE,
                                                        localConfigurator.getTcpSendWindowSize(),
                                                        localConfigurator.getTcpRecvWindowSize(),
                                                        std::string(),
                                                        localConfigurator.getTransportWriteMode(),
                                                        securitylib::getPassphrase()));
                    }
                }
            } else {
                if (m_useCfs) {
                    m_client.reset(new HttpCfsClient_t(localConfigurator.getCfsLocalName(),
                                                       m_psId,
                                                       localConfigurator.getHostId(),
                                                       securitylib::getPassphrase(),
                                                       localConfigurator.getTransportMaxBufferSize(),
                                                       localConfigurator.getTransportConnectTimeoutSeconds(),
                                                       localConfigurator.getTransportResponseTimeoutSeconds(),
                                                       KEEP_ALIVE,
                                                       localConfigurator.getTcpSendWindowSize(),
                                                       localConfigurator.getTcpRecvWindowSize(),
                                                       localConfigurator.getTransportWriteMode()));
                } else {
                    m_client.reset(new HttpClient_t(m_settings.ipAddress,
                                                    m_settings.port,
                                                    localConfigurator.getHostId(),
                                                    localConfigurator.getTransportMaxBufferSize(),
                                                    localConfigurator.getTransportConnectTimeoutSeconds(),
                                                    localConfigurator.getTransportResponseTimeoutSeconds(),
                                                    KEEP_ALIVE,
                                                    localConfigurator.getTcpSendWindowSize(),
                                                    localConfigurator.getTcpRecvWindowSize(),
                                                    localConfigurator.getTransportWriteMode(),
                                                    securitylib::getPassphrase()));
                }
            }
            break;

        case TRANSPORT_PROTOCOL_FILE:
            m_client.reset(new FileClient_t(localConfigurator.getTransportWriteMode(), getRemapPrefixFromToForFileClient()));
            break;

        case TRANSPORT_PROTOCOL_BLOB:
            m_client.reset(new AzureBlobClient(m_settings.ipAddress, localConfigurator.getTransportMaxBufferSize()));
            break;

        default:
            throw ERROR_EXCEPTION << "unsupported protocol: " << m_protocol;
    }
}

bool CxTransportImpClient::abortRequest(std::string const& remoteName)
{
    try {
        m_status.clear();
        m_client->abortRequest();
    } catch (ErrorException const& ee) {
        m_status = ee.what();
    } catch (std::exception const& e) {
        m_status = e.what();
    } catch (...) {
        m_status = "unknown exception caught";
    }
    resetRequest();
    return m_status.empty();
}

void CxTransportImpClient::resetRequest()
{
    try {
        resetClientImp();
    } catch (...) {
    }
}

bool CxTransportImpClient::putFile(std::string const& remoteName,
                                   std::size_t dataSize,
                                   char const* data,
                                   bool moreData,
                                   COMPRESS_MODE compressMode,
                                   bool createDirs,
                                   long long offset)
{
    try {
        m_status.clear();
        m_responseData.reset();
        m_client->putFile(remoteName, dataSize, data, moreData, compressMode, createDirs, offset);
        return true;
    } catch (ErrorException const& ee) {
        m_status = ee.what();
    }
    catch (std::exception const& e) {
        m_status = e.what();
    } catch (...) {
        m_status = "unknown exception caught";
    }
    m_responseData = m_client->getResponseData();
    resetRequest();
    return false;
}

bool CxTransportImpClient::putFile(std::string const& remoteName,
    std::size_t dataSize,
    char const* data,
    bool moreData,
    COMPRESS_MODE compressMode,
    std::map<std::string, std::string> const & headers,
    bool createDirs,
    long long offset)
{
    try {
        m_status.clear();
        m_responseData.reset();
        m_client->putFile(remoteName, dataSize, data, moreData, compressMode, headers, createDirs, offset);
        return true;
    }
    catch (ErrorException const& ee) {
        m_status = ee.what();
    }
    catch (std::exception const& e) {
        m_status = e.what();
    }
    catch (...) {
        m_status = "unknown exception caught";
    }
    m_responseData = m_client->getResponseData();
    resetRequest();
    return false;
}

bool CxTransportImpClient::putFile(std::string const& remoteName,
                                   std::string const& localName,
                                   COMPRESS_MODE compressMode,
                                   bool createDirs)
{
    try {
        m_status.clear();
        m_responseData.reset();
        m_client->putFile(remoteName, localName, compressMode, createDirs);
        return true;
    } catch (ErrorException const& ee) {
        m_status = ee.what();
    } catch (std::exception const& e) {
        m_status = e.what();
    } catch (...) {
        m_status = "unknown exception caught";
    }
    m_responseData = m_client->getResponseData();
    resetRequest();
    return false;
}

bool CxTransportImpClient::putFile(std::string const& remoteName,
    std::string const& localName,
    COMPRESS_MODE compressMode,
    std::map<std::string, std::string> const & headers,
    bool createDirs)
{
    try {
        m_status.clear();
        m_responseData.reset();
        m_client->putFile(remoteName, localName, compressMode, headers, createDirs);
        return true;
    }
    catch (ErrorException const& ee) {
        m_status = ee.what();
    }
    catch (std::exception const& e) {
        m_status = e.what();
    }
    catch (...) {
        m_status = "unknown exception caught";
    }
    m_responseData = m_client->getResponseData();
    resetRequest();
    return false;
}

boost::tribool CxTransportImpClient::renameFile(std::string const& oldName,
                                                std::string const& newName,
                                                COMPRESS_MODE compressMode,
                                                mapHeaders_t const& headers,
                                                std::string const& finalPaths)
{
    try {
        m_status.clear();
        if (CLIENT_RESULT_NOT_FOUND == m_client->renameFile(oldName, newName, compressMode, headers, finalPaths)) {
            m_status = oldName + " not found";
            return boost::indeterminate;
        }
        return true;
    } catch (ErrorException const& ee) {
        m_status = ee.what();
    } catch (std::exception const& e) {
        m_status = e.what();
    } catch (...) {
        m_status = "unknown exception caught";
    }
    resetRequest();
    return false;
}

boost::tribool CxTransportImpClient::renameFile(std::string const& oldName,
                                                std::string const& newName,
                                                COMPRESS_MODE compressMode,
                                                std::string const& finalPaths)
{
    try {
        m_status.clear();
        if (CLIENT_RESULT_NOT_FOUND == m_client->renameFile(oldName, newName, compressMode, finalPaths)) {
            m_status = oldName + " not found";
            return boost::indeterminate;
        }
        return true;
    } catch (ErrorException const& ee) {
        m_status = ee.what();
    } catch (std::exception const& e) {
        m_status = e.what();
    } catch (...) {
        m_status = "unknown exception caught";
    }
    resetRequest();
    return false;
}

boost::tribool CxTransportImpClient::deleteFile(std::string const& names,
                                                int mode)
{
    return deleteFile(names, std::string(), mode);
}

boost::tribool CxTransportImpClient::deleteFile(std::string const& names,
                                                std::string const& fileSpec,
                                                int mode)
{
    try {
        m_status.clear();
        if (CLIENT_RESULT_NOT_FOUND == m_client->deleteFile(names, fileSpec, mode)) {
            m_status = names + " not found";
            return boost::indeterminate;
        }
        return true;
    } catch (ErrorException const& ee) {
        m_status = ee.what();
    } catch (std::exception const& e) {
        m_status = e.what();
    } catch (...) {
        m_status = "unknown exception caught";
    }
    resetRequest();
    return false;

}

void CxTransportImpClient::buildFileInfos(std::string const& files,
                                          FileInfos_t& fileInfos,
                                          bool includePath)
{
    // assumes files has the following format
    //    "filename\tfilesize\n"
    // for each file returned in the list
    std::string::size_type fileIdx = 0;
    std::string::size_type sizeIdx = 0;
    std::string::size_type idx = 0;
    std::string::size_type size = files.size();
    fileInfos.clear();

    do {
        while (idx < size  && '\t' != files[idx]) {
            ++idx;
        }

        if (idx >= size) {
            throw ERROR_EXCEPTION << "expecting tab but reached end of file list";
        }

        sizeIdx = idx + 1; // points to file size

        while (idx < size && '\n' != files[idx]) {
            ++idx;
        }

        if (idx >= size) {
            throw ERROR_EXCEPTION << "expecting new-line but reached end of file list";
        }

        if (!includePath) {
            std::string::size_type tmpIdx = files.substr(fileIdx, sizeIdx - fileIdx - 1).find_last_of("/\\");
            if (std::string::npos != tmpIdx) {
                fileIdx += tmpIdx + 1;
            }
        }

        // the - 1 for the file name is there because sizeIdx has skipped over the \t
        // but the \t should not be copied with the file name.  a - 1 is not needed for
        // the file size as idx has not skipped over the \n yet
        fileInfos.push_back(FileInfo(files.substr(fileIdx, sizeIdx - fileIdx - 1),
                                     boost::lexical_cast<std::size_t>(files.substr(sizeIdx, idx - sizeIdx))));

        ++idx;
        fileIdx = idx; // points to next file

    } while (idx < size);

}

boost::tribool CxTransportImpClient::listFile(std::string const& fileSpec,
                                              FileInfos_t& fileInfos,
                                              bool includePath)
{
    try {
        m_status.clear();
        fileInfos.clear();
        std::string files;
        if (CLIENT_RESULT_NOT_FOUND == m_client->listFile(fileSpec, files)) {
            m_status = fileSpec + " no matches found";
            return boost::indeterminate;
        }
        buildFileInfos(files, fileInfos, includePath);
        return true;
    } catch (ErrorException const& ee) {
        m_status = ee.what();
    } catch (std::exception const& e) {
        m_status = e.what();
    } catch (...) {
        m_status = "unknown exception caught";
    }
    resetRequest();
    return false;
}

boost::tribool CxTransportImpClient::getFile(std::string const& remoteName,
                                             std::size_t dataSize,
                                             char** data,
                                             std::size_t& totalBytesReturned)
{
    try {
        m_status.clear();
        (*data) = (char*)malloc(dataSize);
        if (0 == (*data)) {
            m_status = "out of memory";
            return false;
        }

        std::size_t bytesReturned = 0;

        totalBytesReturned = 0;

        ClientCode result;

        do {
            bytesReturned = 0;
            result = m_client->getFile(remoteName, totalBytesReturned, (dataSize - totalBytesReturned > g_maxDataPerGet ? g_maxDataPerGet : dataSize - totalBytesReturned), *data + totalBytesReturned, bytesReturned);
            if (bytesReturned > 0) {
                totalBytesReturned += bytesReturned;
            }

            switch (result) {
                case CLIENT_RESULT_OK:
                {
                    if (typeid(*m_client) == typeid(AzureBlobClient))
                    {
                        if (totalBytesReturned == dataSize)
                        {
                            return true;
                        }
                        else if (totalBytesReturned > dataSize)
                        {
                            totalBytesReturned = dataSize;
                            m_status = "buffer too small";
                            resetRequest();
                            return false;
                        }
                        break;
                    }
                    else
                    {
                        return true;
                    }
                }

                case CLIENT_RESULT_MORE_DATA:
                {
                    // TODO: allow caller to continue in this case instead of aborting request
                    if (totalBytesReturned >= dataSize) {
                        totalBytesReturned = dataSize;
                        m_status = "buffer too small";
                        resetRequest();
                        return false;
                    }
                    break;
                }

                case CLIENT_RESULT_NOT_FOUND:
                    m_status = remoteName + " not found";
                    totalBytesReturned = 0;
                    return boost::indeterminate;

                default:
                    m_status = "unknown result returned";
                    totalBytesReturned = 0;
                    return false;
            }
        } while ((CLIENT_RESULT_MORE_DATA == result) ||
            ((typeid(*m_client) == typeid(AzureBlobClient)) && (totalBytesReturned < dataSize)));

    } catch (ErrorException const& ee) {
        m_status = ee.what();
    } catch (std::exception const& e) {
        m_status = e.what();
    } catch (...) {
        m_status = "unknown exception caught";
    }

    totalBytesReturned = 0;

    resetRequest();
    return false;
}

boost::tribool CxTransportImpClient::getFile(std::string const& remoteName,
                                             std::string const& localName)
{
    try {
        m_status.clear();
        if (CLIENT_RESULT_NOT_FOUND == m_client->getFile(remoteName, localName)) {
            m_status = remoteName + " not found";
            return boost::indeterminate;
        }
        return true;
    } catch (ErrorException const& ee) {
        m_status = ee.what();
    } catch (std::exception const& e) {
        m_status = e.what();
    } catch (...) {
        m_status = "unknown exception caught";
    }
    resetRequest();
    return false;
}

boost::tribool CxTransportImpClient::getFile(std::string const& remoteName,
    std::string const& localName,
    std::string& checksum)
{
    try {
        m_status.clear();
        if (CLIENT_RESULT_NOT_FOUND == m_client->getFile(remoteName, localName, checksum)) {
            m_status = remoteName + " not found";
            return boost::indeterminate;
        }
        return true;
    }
    catch (ErrorException const& ee) {
        m_status = ee.what();
    }
    catch (std::exception const& e) {
        m_status = e.what();
    }
    catch (...) {
        m_status = "unknown exception caught";
    }
    resetRequest();
    return false;
}

bool CxTransportImpClient::heartbeat(bool forceSend)
{
    try {
        m_status.clear();
        m_client->heartbeat(forceSend);
        return true;
    } catch (ErrorException const& ee) {
        m_status = ee.what();
    } catch (std::exception const& e) {
        m_status = e.what();
    } catch (...) {
        m_status = "unknown exception caught";
    }
    resetRequest();
    return false;
}

CxTransportImpClient::remapPrefixFromTo_t CxTransportImpClient::getRemapPrefixFromToForFileClient(void)
{
    LocalConfigurator lc;
    std::string cxpsConfPath;
    if (FILE_CONFIGURATOR_MODE_CS_PRIME_APPLAINCE_TO_AZURE == FileConfigurator::getInitMode()
        || FILE_CONFIGURATOR_MODE_CS_PRIME_APPLIANCE_EVTCOLLFORW == FileConfigurator::getInitMode())
    {
        const std::string cxpsConfFile("/home/svsystems/transport/cxps.conf");
        cxpsConfPath = lc.getPSInstallPathOnCsPrimeApplianceToAzure() + cxpsConfFile; // TODO: nichougu - PS owner to provide cxps.conf path.
    }
    else
    {
        cxpsConfPath = securitylib::getCxpsConfPath();
    }
    std::ifstream confFile(cxpsConfPath.c_str());
    
    // commenting out exception as we can have PS and MT on seperate box
    // we need to also support the scenario where cxps file tal is being
    // used for just local file operations on MT alone box without need
    // for remapping
    
    //if (!confFile.good())
    //  throw ERROR_EXCEPTION << "Failed to open file " << cxpsConfPath;
    
    remapPrefixFromTo_t r;
    while (confFile.good()) {
        std::string line;
        std::getline(confFile, line);
        if ('#' != line[0] && '[' != line[0]) {
            std::string data(line);
            std::string::size_type idx = data.find_first_of("=");
            if (std::string::npos != idx) {
                std::string tag(data.substr(0, idx));
                boost::algorithm::trim(tag);
                std::string value(data.substr(idx + 1));
                boost::algorithm::trim(value);
                if (tag == "remap_full_path_prefix") {
                    r = getRemapPrefixFromToFromConfValue(value);
                    break;
                }
            }
        }
    }

    return r;
}

CxTransportImpClient::remapPrefixFromTo_t CxTransportImpClient::getRemapPrefixFromToFromConfValue(const std::string &value)
{
    std::string fromTo = value;
    if (fromTo.empty()) {
        return remapPrefixFromTo_t(std::make_pair(std::string(), std::string()));
    }

    // have a remap setting
    std::string::size_type len(fromTo.size());

    char stopChar;

    // get <from prefix> start
    // skip over leading white space
    std::string::size_type fromStartIdx(0);
    while (fromStartIdx < len && std::isspace(fromTo[fromStartIdx])) {
        ++fromStartIdx;
    }

    // check if <from prefix> enclosed in double quotes
    if ('"' == fromTo[fromStartIdx]) {
        ++fromStartIdx;
        // skip over any leading white space after a double quote
        while (fromStartIdx < len && std::isspace(fromTo[fromStartIdx])) {
            ++fromStartIdx;
        }
        stopChar = '"';
    }
    else {
        stopChar = ' ';
    }

    // find <from prefix> end
    std::string::size_type fromEndIdx(fromStartIdx);
    while (fromEndIdx < len && (('"' == stopChar && '"' != fromTo[fromEndIdx]) || (' ' == stopChar && !std::isspace(fromTo[fromEndIdx])))) {
        if ('\\' == fromTo[fromEndIdx]) {
            fromTo[fromEndIdx] = '/'; // just to be safe convert backslash to slash
        }
        ++fromEndIdx;
    }

    // this is where to start looking for the <to prefix>
    std::string::size_type toStartIdx(fromEndIdx + 1);

    // if the <from prefix> was enclosed in double quotes trim
    // any trailing white space between <from prefix> and its
    // closing double quote
    if ('"' == stopChar && std::isspace(fromTo[fromEndIdx - 1])) {
        do {
            --fromEndIdx;
        } while (std::isspace(fromTo[fromEndIdx]));
        ++fromEndIdx;
    }

    // skip all white space between <from prefix> and <to prefix>
    while (toStartIdx < len && std::isspace(fromTo[toStartIdx])) {
        ++toStartIdx;
    }

    // check if <to prefix> enclosed in double quotes
    if ('"' == fromTo[toStartIdx]) {
        ++toStartIdx;
        // skip over any leading white space after a double quote
        while (toStartIdx < len && std::isspace(fromTo[toStartIdx])) {
            ++toStartIdx;
        }
        stopChar = '"';
    }
    else {
        stopChar = ' ';
    }

    // find <to prefix> end
    std::string::size_type toEndIdx(toStartIdx);
    while (toEndIdx < len && (('"' == stopChar && '"' != fromTo[toEndIdx]) || (' ' == stopChar && !std::isspace(fromTo[toEndIdx])))) {
        if ('\\' == fromTo[toEndIdx]) {
            fromTo[toEndIdx] = '/'; // just to be safe convert backslash to slash
        }
        ++toEndIdx;
    }

    // if the <to prefix> was enclosed in double quotes trim
    // any trailing white space between <to prefix> and its
    // closing double quote
    if ('"' == stopChar && std::isspace(fromTo[toEndIdx - 1])) {
        do {
            --toEndIdx;
        } while (std::isspace(fromTo[toEndIdx]));
        ++toEndIdx;
    }

    return remapPrefixFromTo_t(make_pair(std::string(fromTo.substr(fromStartIdx, fromEndIdx - fromStartIdx)),
        std::string(fromTo.substr(toStartIdx, toEndIdx - toStartIdx))));
}

ResponseData CxTransportImpClient::getResponseData()
{
    return m_responseData;
}
