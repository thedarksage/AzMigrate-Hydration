
//
//  File: azureblobclient.h
//
//  Description:
//

#ifndef AZUREBLOBCLIENT_H
#define AZUREBLOBCLIENT_H

#include <string>
#include <boost/thread/mutex.hpp>

#include "client.h"
#include "CloudBlob.h"
#include "compressmode.h"
#include "transport_settings.h"

using namespace AzureStorageRest;

struct BlobMetadata
{
    BlobMetadata(std::string key, std::string value) : m_Key(key), m_Value(value) {}

    std::string m_Key;
    std::string m_Value;
};

class AzureBlobClient : public ClientAbc
{
public:
    static SV_LOG_LEVEL        s_logLevel;

    AzureBlobClient(const std::string& blobContainerSasUri,
        const std::size_t maxBufferSizeBytes,
        const uint32_t timeout = 300);

    virtual ~AzureBlobClient();
    
    virtual void SetHttpProxy(const std::string& address, const std::string& port, const std::string& bypasslist);
    
    virtual uint32_t GetLastError() { return m_lastError; }

    void putFile(std::string const& remoteName,
        std::size_t dataSize,
        char const * data,
        bool moreData,
        COMPRESS_MODE compressMode,
        mapHeaders_t const & headers,
        bool createDirs = false,
        long long offset = PROTOCOL_DO_NOT_SEND_OFFSET
    );

    void putFile(std::string const& remoteName,
        std::size_t dataSize,
        char const * data,
        bool moreData,
        COMPRESS_MODE compressMode,
        bool createDirs = false,
        long long offset = PROTOCOL_DO_NOT_SEND_OFFSET
    );

    virtual void putFile(std::string const& remoteName,
        std::string const& localName,
        COMPRESS_MODE compressMode,
        mapHeaders_t const & headers,
        bool createDirs = false
    );

    virtual void putFile(std::string const& remoteName,
        std::string const& localName,
        COMPRESS_MODE compressMode,
        bool createDirs = false
    );

    virtual ClientCode listFile(std::string const& fileSpec, std::string & files);

    virtual ClientCode renameFile(std::string const& oldName,
        std::string const& newName,
        COMPRESS_MODE compressMode,
        mapHeaders_t const& headers,
        std::string const& finalPaths = std::string()
    );

    virtual ClientCode renameFile(std::string const& oldName,
        std::string const& newName,
        COMPRESS_MODE compressMode,
        std::string const& finalPaths = std::string()
    );

    virtual ClientCode getFile(std::string const& name,
        std::size_t dataSize,
        char * data,
        std::size_t& bytesReturned);

    ClientCode getFile(std::string const& name,
        std::size_t offset,
        std::size_t dataSize,
        char * data,
        std::size_t& bytesReturned);

    virtual ClientCode getFile(std::string const& remoteName,  ///< remote file to get
        std::string const& localName);

    virtual ClientCode getFile(std::string const& remoteName,
        std::string const& localName,
        std::string& checksum);

    virtual ClientCode deleteFile(std::string const& names,
        int mode = FindDelete::FILES_ONLY);

    virtual ClientCode deleteFile(std::string const& names,
        std::string const& fileSpec,
        int mode = FindDelete::FILES_ONLY);

    virtual ClientCode heartbeat(bool forceSend = false) { return CLIENT_RESULT_OK; }

    virtual void abortRequest(bool disconnectOnly = false) {}

    virtual int timeoutSeconds() { return m_timeout; }

public:
    // Dummy implementations

    virtual std::string hostId() { return std::string(); }

    virtual std::string ipAddress() { return std::string(); }

    virtual std::string port() { return std::string(); }

    /// \brief connects to cs does not require fingerprint to match but insteads returns it
    virtual void csConnect(std::string& fingerprint, std::string& certificate) {}

    virtual bool sendCsRequest(std::string const& request, std::string& response) { return true; }

    virtual std::string password() { return std::string(); }

protected:
    virtual void syncConnect(boost::asio::ip::tcp::endpoint const& endpoint,
        int sendWindowSizeBytes, int receiveWindowSizeBytes) {}

    virtual void asyncConnect(boost::asio::ip::tcp::endpoint const& endpoint,
        int sendWindowSizeBytes, int receiveWindowSizeBytes) {}

private:
    void CreateCloudPageBlobObject(const std::string& blobSas);

    void CreateCloudPageBlobObjectForList(const std::string& blobSas);

    void CreateCloudPageBlobObjectForDelete(const std::string& blobSas);

    std::string GetRestUriFromSasUri(const std::string& sasUri, const std::string& suffix);

    void Write(const std::size_t offset, const char * buffer, const std::size_t length);

    void Write(const char * buffer, const std::size_t length);

    void Read(const std::size_t offset, char * buffer, const std::size_t length, std::size_t& bytesReturned);

    void ReadWithSize(std::size_t offset,
        char* buffer,
        const std::size_t length,
        std::size_t& bytesReturned,
        std::size_t& blobSize);

    ClientCode getFileAndChecksum(std::string const& remoteName,
        std::string const& localName,
        std::string& checksum,
        bool bNeedChecksum);

private:
    static uint32_t                 s_id;

    uint32_t                        m_id;

    std::string             m_blobContainerSasUri;
    std::string             m_continuationBlobName;

    std::size_t             m_writeDataSize;
    std::size_t             m_continuationWriteOffset;
    std::size_t             m_maxBufferSizeBytes; ///< max size to use for internal buffers
    std::size_t             m_bufferedWriteSize;

    boost::shared_ptr<char>         m_ptrWriteBuffer;

    std::size_t                     m_readFileSize;
    std::size_t                     m_continuationReadOffset;

    boost::recursive_mutex          m_contextLock;

    HttpProxy               m_proxy;
    bool                    m_use_proxy;
    uint32_t                m_timeout;
    uint32_t                m_lastError;

    boost::shared_ptr<CloudPageBlob>  m_PtrPageBlob;
    boost::shared_ptr<HttpClient>     m_PtrHttpClient;

    boost::shared_ptr<CloudPageBlob>  m_PtrPageBlobForList;
    boost::shared_ptr<HttpClient>     m_PtrHttpClientForList;

    boost::shared_ptr<CloudPageBlob>  m_PtrPageBlobForDelete;
    boost::shared_ptr<HttpClient>     m_PtrHttpClientForDelete;
};

#endif // AZUREBLOBCLIENT_H
