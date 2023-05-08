
//
//  File: transportstream.h
//
//  Description:
//

#ifndef TRANSPORTSTREAM_H
#define TRANSPORTSTREAM_H

#ifndef TRANSPORT_STREAM__H
#define TRANSPORT_STREAM__H

#include <string>
#include <stdexcept>

#include <boost/chrono.hpp>

#include "localconfigurator.h"
#include "transport_settings.h"
#include "basetransportstream.h"
#include "cxtransport.h"
#include "cxpsheaders.h"

using namespace boost::chrono;

class TransportStream :
    public BaseTransportStream
{
public:
    explicit TransportStream(TRANSPORT_PROTOCOL protocol,
                             TRANSPORT_CONNECTION_SETTINGS const& transportSettings)
        : m_bNeedToDeletePrevFile(false),
          m_protocol(protocol),
          m_transportSettings(transportSettings),
          m_secure(false),
          m_bAppendSystemTimeUtcOnRename(false),
          m_transportErrorLogInterval(0)
        { }

    virtual ~TransportStream() {
        Close();
    }

    virtual int Write(const char*, unsigned long int, const std::string&, bool bMoreData, COMPRESS_MODE compressMode, bool createDirs = false, 
                      bool bIsFull = false);
    virtual int Write(const std ::string& localFile, const std ::string& targetFile, COMPRESS_MODE compressMode, bool createDirs = false);
    virtual int Write(const std ::string& localFile, const std ::string& targetFile, COMPRESS_MODE compressMode, const std::string& renameFile, std::string const& finalPaths = std::string(), bool createDirs = false);

    virtual int Open(const STREAM_MODE);

    virtual int Close();
    virtual int Abort(char const* pData);

    // see client.h Client::deletFile for complete details on deleteFile
    virtual int DeleteFile(std::string const& names, int mode = FindDelete::FILES_ONLY); 
    virtual int DeleteFile(std::string const& names, std::string const& fileSpec, int mode = FindDelete::FILES_ONLY);
        
    virtual int Rename(const std::string& sOldFileName, const std::string& sNewFileName, COMPRESS_MODE compressMode, std::string const& finalPaths = std::string());

    virtual int DeleteFiles(const ListString& DeleteFileList);
    bool NeedToDeletePreviousFile(void);
    void SetNeedToDeletePreviousFile(bool bneedtodeleteprevfile);

    virtual int Write(const void*,const unsigned long int) {
        throw std::runtime_error("not implemented");
    }

    virtual int Read(void*,const unsigned long int) {
        throw std::runtime_error("not implemented");
    }
    virtual void SetSizeOfStream(unsigned long int size)
    {
    }
    
    virtual bool heartbeat(bool forceSend=false);

    bool LogTransportFailures(int iStatus);

    virtual void SetDiskId(const std::string & diskId);

    virtual void SetFileType(int filetype);

    virtual void SetAppendSystemTimeUtcOnRename(bool appendTimeOn) { m_bAppendSystemTimeUtcOnRename = appendTimeOn; };

    virtual ResponseData GetResponseData() const;

protected:
    bool m_bNeedToDeletePrevFile;

private:

    // TODO - these should be aggregated at protocol level
    static steady_clock::time_point   s_LastTransportErrLogTime;
    static uint64_t     s_TransportFailCnt;
    static uint64_t     s_TransportSuccCnt;

    TRANSPORT_PROTOCOL m_protocol;

    TRANSPORT_CONNECTION_SETTINGS m_transportSettings;

    CxTransport::ptr m_cxTransport;

    LocalConfigurator m_localConfigurator;

    bool m_secure;

    std::map<std::string, std::string> m_headers;

    bool m_bAppendSystemTimeUtcOnRename;

    uint64_t    m_transportErrorLogInterval;
};

#endif



#endif // TRANSPORTSTREAM_H
