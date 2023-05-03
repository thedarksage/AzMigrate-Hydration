
///
///  \file  cxtransport.cpp
///
///  \brief implements the cx transport factory for creating the correct
///  transport object to use
///

#include <sstream>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include "cxtransport.h"
#include "extendedlengthpath.h"

/// \brief helper to get the size of a file
///
/// \return
/// \li true: if the file exists
/// \li false: if the file does not exist
bool getFileSize(std::string const& name, ///< name of file to get size
                 unsigned long long& size ///< receives the size of the file
                 )
{
    extendedLengthPath_t extName(ExtendedLengthPath::name(name));
    try {
        if (!boost::filesystem::is_regular_file(extName)) {
            return false;
        }
        size = boost::filesystem::file_size(extName);
    } catch (...) {
        size = 0;
    }
    return true;
}

CxTransport::CxTransport(TRANSPORT_PROTOCOL transportProtocol,
                         TRANSPORT_CONNECTION_SETTINGS const& transport,
                         bool secure,
                         bool useCfs,
                         std::string const& psId)
    : m_protocol(transportProtocol),
      m_imp(CxTransportImpFactory(transportProtocol, transport, secure, useCfs, psId))
{
}

bool CxTransport::abortRequest(std::string const& remoteName)
{
    return m_imp->abortRequest(remoteName);
}

bool CxTransport::putFile(std::string const& remoteName,
                          std::size_t dataSize,
                          char const* data,
                          bool moreData,
                          COMPRESS_MODE compressMode,
                          bool createDirs,
                          long long offset)
{
    bool b = m_imp->putFile(remoteName, dataSize, data, moreData, compressMode, createDirs, offset);
    if (TRANSPORT_PROTOCOL_FILE == m_protocol) {
        CX_TRANSPORT_LOG_XFER("putfile" << '\t'
                              << remoteName << '\t'
                              << dataSize << '\t'
                              << moreData  << '\t'
                              << (b ? "success" : "failed")
                              );
    }
    return b;

}

bool CxTransport::putFile(std::string const& remoteName,
    std::size_t dataSize,
    char const* data,
    bool moreData,
    COMPRESS_MODE compressMode,
    std::map<std::string, std::string> const & headers,
    bool createDirs,
    long long offset)
{
    bool b = m_imp->putFile(remoteName, dataSize, data, moreData, compressMode, headers, createDirs, offset);
    if (TRANSPORT_PROTOCOL_FILE == m_protocol) {
        CX_TRANSPORT_LOG_XFER("putfile" << '\t'
            << remoteName << '\t'
            << dataSize << '\t'
            << moreData << '\t'
            << (b ? "success" : "failed")
        );
    }
    return b;

}

bool CxTransport::putFile(std::string const& remoteName,
                          std::string const& localName,
                          COMPRESS_MODE compressMode,
                          bool createDirs)
{
    bool b = m_imp->putFile(remoteName, localName, compressMode, createDirs);
    if (TRANSPORT_PROTOCOL_FILE == m_protocol) {
        unsigned long long size;
        if (getFileSize(localName, size)) {
            CX_TRANSPORT_LOG_XFER("putfile" << '\t'
                                  << remoteName << '\t'
                                  << size << '\t'
                                  << (b ? "success" : "failed")
                                  );
        }
    }
    return b;
}

bool CxTransport::putFile(std::string const& remoteName,
    std::string const& localName,
    COMPRESS_MODE compressMode,
    std::map<std::string, std::string> const & headers,
    bool createDirs)
{
    bool b = m_imp->putFile(remoteName, localName, compressMode, headers, createDirs);
    if (TRANSPORT_PROTOCOL_FILE == m_protocol) {
        unsigned long long size;
        if (getFileSize(localName, size)) {
            CX_TRANSPORT_LOG_XFER("putfile" << '\t'
                << remoteName << '\t'
                << size << '\t'
                << (b ? "success" : "failed")
            );
        }
    }
    return b;
}

boost::tribool CxTransport::renameFile(std::string const& oldName,
                                       std::string const& newName,
                                       COMPRESS_MODE compressMode,
                                       std::map <std::string, std::string> const& headers,
                                       std::string const& finalPaths)
{
    return m_imp->renameFile(oldName, newName, compressMode, headers, finalPaths);
}

boost::tribool CxTransport::renameFile(std::string const& oldName,
    std::string const& newName,
    COMPRESS_MODE compressMode,
    std::string const& finalPaths)
{
    return m_imp->renameFile(oldName, newName, compressMode, finalPaths);
}

boost::tribool CxTransport::deleteFile(std::string const& names, int mode)
{
    return deleteFile(names, std::string(), mode);
}

boost::tribool CxTransport::deleteFile(std::string const& names, std::string const& fileSpec, int mode)
{
    boost::tribool tb = m_imp->deleteFile(names, fileSpec, mode);
    if (TRANSPORT_PROTOCOL_FILE == m_protocol) {
        CX_TRANSPORT_LOG_XFER("deletefile" << '\t'
                              << names << '\t'
                              << (!tb ? "failed" : "success")
                              );
    }
    return tb;
}


boost::tribool CxTransport::listFile(std::string const& fileSpec,
                                     FileInfos_t& fileInfos,
                                     bool includePath)
{
    return m_imp->listFile(fileSpec, fileInfos, includePath);
}


boost::tribool CxTransport::getFile(std::string const& remoteName,
                                    std::size_t dataSize,
                                    char** data,
                                    std::size_t& bytesReturned)
{
    boost::tribool tb =  m_imp->getFile(remoteName, dataSize, data, bytesReturned);
    if (TRANSPORT_PROTOCOL_FILE == m_protocol) {
        CX_TRANSPORT_LOG_XFER("getfile" << '\t'
                              << remoteName << '\t'
                              << dataSize << '\t'
                              << (tb ? "success" : "failed")
                              );
    }
    return tb;
}

boost::tribool CxTransport::getFile(std::string const& remoteName,
                                    std::string const& localName)
{
    boost::tribool tb = m_imp->getFile(remoteName, localName);
    if (TRANSPORT_PROTOCOL_FILE == m_protocol) {
        unsigned long long size;
        if (getFileSize(localName, size)) {
            CX_TRANSPORT_LOG_XFER("getfile" << '\t'
                                  << remoteName << '\t'
                                  << size << '\t'
                                  << (tb ? "success" : "failed")
                                  );
        }
    }
    return tb;

}

boost::tribool CxTransport::getFile(std::string const& remoteName,
    std::string const& localName,
    std::string& checksum)
{
    boost::tribool tb = m_imp->getFile(remoteName, localName, checksum);
    if (TRANSPORT_PROTOCOL_FILE == m_protocol) {
        unsigned long long size;
        if (getFileSize(localName, size)) {
            CX_TRANSPORT_LOG_XFER("getfile" << '\t'
                << remoteName << '\t'
                << size << '\t'
                << checksum << '\t'
                << (tb ? "success" : "failed")
            );
        }
    }
    return tb;

}

bool CxTransport::heartbeat(bool forceSend)
{
    return m_imp->heartbeat(forceSend);
}

const char* CxTransport::status()
{
    return m_imp->status();
}

ResponseData CxTransport::getResponseData()
{
    return m_imp->getResponseData();
}
