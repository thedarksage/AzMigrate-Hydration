
//
//  File: azureblobstream.h
//
//  Description:
//

#ifndef AZUREBLOBSTREAM_H
#define AZUREBLOBSTREAM_H

#include <string>

#include "transport_settings.h"
#include "basetransportstream.h"
#include "compressmode.h"
#include "CloudBlob.h"

using namespace AzureStorageRest;

class AzureBlobStream :
    public BaseTransportStream
{
protected:
    std::string     m_blobContainerSasUri;
    std::string     m_continuationBlobName;
    uint64_t        m_dataSize;
    
    uint64_t        m_blobCreateTime;

    bool            m_use_proxy;
    HttpProxy       m_proxy;
    uint32_t        m_timeout;

    uint8_t m_NumberOfParallelUploads;

    std::vector<boost::shared_ptr<HttpClient> >     m_vPtrHttpClient;

    uint32_t                    m_lastError;
    std::string                 m_responseData;

    // Blob content checksum variables
    const bool                      m_bEnableChecksums;
    boost::shared_ptr<SHA256_CTX>   m_pSha256CtxForChecksums;

protected:
    void Init();
    
    std::string GetRestUriFromSasUri(const std::string& sasUri, const std::string& suffix);

    bool GetEndTimeStampSeqNumForSorting(const std::string& filename, std::string& endTimeStampSeqNum);

    virtual int SetMetadata(const metadata_t& metadata) = 0;

public:
    AzureBlobStream(std::string& blobContainerSasUri, const uint32_t& timeout = 300, const bool bEnableChecksums = false);
    virtual ~AzureBlobStream();
    virtual int Write(const char* sData, unsigned long int uliDataLen, const std::string& sDestination, bool bMoreData, COMPRESS_MODE compressMode, bool createDirs = false,
        bool bIsFull = false) = 0;
    virtual int Write(const std::string& localFile, const std::string& targetFile, COMPRESS_MODE compressMode, bool createDirs = false);
    virtual int Write(const std::string& localFile, const std::string& targetFile, COMPRESS_MODE compressMode,
        const std::string& renameFile, std::string const& finalPaths = std::string(), bool createDirs = false);

    /// \brief write to azure page blob with page-aligned size (512-bytes)
    virtual int Write(const char* buffer, unsigned long int length, unsigned long int offset, const std::string &sasUri) = 0;

    virtual int Open(const STREAM_MODE);
    virtual int Close();
    virtual int Abort(char const* pData);
    virtual int DeleteFile(std::string const& names, int mode = FindDelete::FILES_ONLY);
    virtual int DeleteFile(std::string const& names, std::string const& fileSpec, int mode = FindDelete::FILES_ONLY);
    virtual int Rename(const std::string& sOldFileName, const std::string& sNewFileName, COMPRESS_MODE compressMode, std::string const& finalPaths = std::string());
    virtual int DeleteFiles(const ListString& DeleteFileList);
    virtual int Write(const void*, const unsigned long int);
    virtual bool NeedToDeletePreviousFile(void);
    virtual void SetNeedToDeletePreviousFile(bool bneedtodeleteprevfile);
    virtual int Read(void*, const unsigned long int);
    virtual void SetSizeOfStream(unsigned long int);
    virtual bool heartbeat(bool forceSend = false);
    virtual void SetHttpProxy(const std::string& address, const std::string& port, const std::string& bypasslist);

    virtual uint32_t GetLastError() { return m_lastError; }
    virtual bool UpdateTransportSettings(const std::string& sasUri);
    virtual void SetTransportTimeout(SV_ULONGLONG timeout);
    virtual std::string GetLastResponseData() { return m_responseData; }
};

#endif // AZUREBLOBSTREAM_H
