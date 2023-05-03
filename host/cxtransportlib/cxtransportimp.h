
///
///  \file cxtransportimp.h
///
///  \brief cx transport implementation
///

#ifndef CXTRASNPORTIMP_H
#define CXTRASNPORTIMP_H

#include <string>
#include <vector>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/logic/tribool.hpp>

#include "volumegroupsettings.h"
#include "cxtransportdefines.h"
#include "compressmode.h"
#include "finddelete.h"
#include "ResponseData.h"
#include "Telemetry/TelemetrySharedParams.h"

/// \brief there is more data to be sent/retrieved
bool const CX_TRANSPORT_MORE_DATA      = true;

/// \brief there is no more data to be sent/retrieved
bool const CX_TRANSPORT_NO_MORE_DATA   = false;

/// \brief holds information about a file entry returned from a list file request
///
/// \note file names will only include the path if list file was called with
/// includePath = true
struct FileInfo {
    FileInfo(std::string name,  ///< name of file
             std::size_t size)  ///< size of file
        : m_name(name),
          m_size(size)
        {}

    std::string m_name;  ///< name of file
    std::size_t m_size;  ///< size of file

    bool operator<(const FileInfo& rhs) const
    {
        return (m_name.length() < rhs.m_name.length()) ? true :
            (m_name.compare(rhs.m_name) < 0);
    }
};

/// \brief for holding returned file entries from a list file request
typedef std::vector<FileInfo> FileInfos_t;

/// \brief shared pointer for FileInfos_t
typedef boost::shared_ptr<FileInfos_t> FileInfosPtr;

/// \brief abstract class used to send requests to cx process server
class CxTransportImp : private boost::noncopyable {
public:
    /// \brief shared pointer type for CxTransportImp
    typedef boost::shared_ptr<CxTransportImp> ptr;

    /// \brief constructor
    explicit CxTransportImp() {}

    /// \brief destructor
    virtual ~CxTransportImp() {}

    /// \brief abort an in progess request
    ///
    /// \param remoteName name of the remote file to abort
    /// (optional as it is only needed if aborting a putFile request)
    virtual bool abortRequest(std::string const& remoteName = std::string()) = 0;

    /// \brief issue put file request to put data to a remote file
    virtual bool putFile(std::string const& remoteName,                 ///< name of remote file to place data in
                         std::size_t dataSize,                          ///< size of data in buffer pointed to by data
                         char const* data,                              ///< points to buffer holding data to be sent
                         bool moreData,                                 ///< indicates if there is more data to be sent true: yes, false: no
                         COMPRESS_MODE compressMode,                   ///< compress mode (see config/compressmode.h)
                         bool createDirs = false,                      ///< indicates if missing dirs should be created (true: yes, false: no)
                         long long offset = PROTOCOL_DO_NOT_SEND_OFFSET ///< offset to write at (ignored by old (tal) transport)
                         ) = 0;

    /// \brief issue put file request to put data to a remote file
    virtual bool putFile(std::string const& remoteName,                 ///< name of remote file to place data in
        std::size_t dataSize,                          ///< size of data in buffer pointed to by data
        char const* data,                              ///< points to buffer holding data to be sent
        bool moreData,                                 ///< indicates if there is more data to be sent true: yes, false: no
        COMPRESS_MODE compressMode,                   ///< compress mode (see config/compressmode.h)
        std::map<std::string, std::string> const & headers, ///< additional headers to send in putfile request
        bool createDirs = false,                      ///< indicates if missing dirs should be created (true: yes, false: no)
        long long offset = PROTOCOL_DO_NOT_SEND_OFFSET ///< offset to write at (ignored by old (tal) transport)
    ) = 0;

    /// \brief issue put file request to put a local file to a remote file
    virtual bool putFile(std::string const& remoteName,  ///< name of remote file to place data in
                         std::string const& localName,   ///< name of local file to send
                         COMPRESS_MODE compressMode,     ///< compress mode (see config/compressmode.h)
                         bool createDirs = false        ///< indicates if missing dirs should be created (true: yes, false: no)
                         ) = 0;

    /// \brief issue put file request to put a local file to a remote file
    virtual bool putFile(std::string const& remoteName,  ///< name of remote file to place data in
        std::string const& localName,   ///< name of local file to send
        COMPRESS_MODE compressMode,     ///< compress mode (see config/compressmode.h)
        std::map<std::string, std::string> const &headers,  ///< additional headers to send in putfile request
        bool createDirs = false        ///< indicates if missing dirs should be created (true: yes, false: no)
    ) = 0;

    /// \brief issue rename file request
    ///
    ///  \note
    ///  \li \c finalPaths should be used to tell transport it should perform the "final" rename instead of time shot manager
    ///  it should be a semi-conlon (';') seperated list of the final paths that the file should get a "copy" of the new file.
    ///  hard links are used to create the "copies" unless transport server was built with RENAME_COPY_ON_FAILED_LINK defined.
    ///  in that case transport will attempt to copy the file if the hard link fails.
    ///
    /// \return
    /// \li \c true : on success
    /// \li \c false: on failure
    /// \li \c boost::indeterminate: on oldName not found.
    /// use CxTransportNotFound to test for not found\n
    /// \sa BOOST_TRIBOOL_THIRD_STATE(CxTransportNotFound)
    virtual boost::tribool renameFile(std::string const& oldName,                    ///< name of the file to be renamed
                                      std::string const& newName,                    ///< new name to give the file
                                      COMPRESS_MODE compressMode,                    ///< compress mode in affect (see config/compressmode.h)
                                      std::map<std::string, std::string> const& headers,    ///< additional headers to be sent in renamefile request
                                      std::string const& finalPaths = std::string()  ///< semi-colon (';') seperated  list of all paths that should get a "copy" of the renamed file
                                      ) = 0;

    /// \brief issue rename file request
    ///
    ///  \note
    ///  \li \c finalPaths should be used to tell transport it should perform the "final" rename instead of time shot manager
    ///  it should be a semi-conlon (';') seperated list of the final paths that the file should get a "copy" of the new file.
    ///  hard links are used to create the "copies" unless transport server was built with RENAME_COPY_ON_FAILED_LINK defined.
    ///  in that case transport will attempt to copy the file if the hard link fails.
    ///
    /// \return
    /// \li \c true : on success
    /// \li \c false: on failure
    /// \li \c boost::indeterminate: on oldName not found.
    /// use CxTransportNotFound to test for not found\n
    /// \sa BOOST_TRIBOOL_THIRD_STATE(CxTransportNotFound)
    virtual boost::tribool renameFile(std::string const& oldName,                    ///< name of the file to be renamed
                                      std::string const& newName,                    ///< new name to give the file
                                      COMPRESS_MODE compressMode,                    ///< compress mode in affect (see config/compressmode.h)
                                      std::string const& finalPaths = std::string()  ///< semi-colon (';') seperated  list of all paths that should get a "copy" of the renamed file
                                      ) = 0;

    /// \brief issue delete file request
    ///
    /// see client.h Clinet::deleteFile for complete details
    ///
    /// \return
    /// \li \c true : on success
    /// \li \c false: on failure
    /// \li \c boost::indeterminate: on remoteName not found.
    /// use CxTransportNotFound to test for not found\n
    /// \sa BOOST_TRIBOOL_THIRD_STATE(CxTransportNotFound)
    virtual boost::tribool deleteFile(std::string const& names,          ///< semi-colon (';') separated list of names
                                      int mode = FindDelete::FILES_ONLY  ///< delete mode to use
                                      ) = 0;

    /// \brief issue delete file request
    ///
    /// see client.h Clinet::deleteFile for complete details
    ///
    /// \return
    /// \li \c true : on success
    /// \li \c false: on failure
    /// \li \c boost::indeterminate: on remoteName not found.
    /// use CxTransportNotFound to test for not found\n
    /// \sa BOOST_TRIBOOL_THIRD_STATE(CxTransportNotFound)
    virtual boost::tribool deleteFile(std::string const& names,         ///< semi-colon (';') separated list of names
                                      std::string const& fileSpec,      ///< semi-colon (';') separated list of file specs
                                      int mode = FindDelete::FILES_ONLY ///< delete mode to use
                                      ) = 0;

    /// \brief issue list file request
    ///
    /// \return
    /// \li \c true : on success
    /// \li \c false: on failure
    /// \li \c boost::indeterminate: on no matches found
    /// use CxTransportNotFound to test for not found\n
    /// \sa BOOST_TRIBOOL_THIRD_STATE(CxTransportNotFound)
    virtual boost::tribool listFile(std::string const& fileSpec, ///< file specification to use to match against use glob syntax
                                    FileInfos_t& files,          ///< holds FileInfos for each match found
                                    bool includePath = false     ///< indicates if path should be added true: yes, false: no
                                    ) = 0;


    /// \brief issue get file request to get a remote file to buffer
    ///
    /// \return
    /// \li \c true : on success
    /// \li \c false: on failure
    /// \li \c boost::indeterminate: on remoteName not found.
    /// use CxTransportNotFound to test for not found
    /// \sa BOOST_TRIBOOL_THIRD_STATE(CxTransportNotFound)
    /// \note
    /// \li \c the data buffer is allocated using malloc and it is the
    /// responsibility of the caller to use free to release the memory
    /// when it is no longer needed
    virtual boost::tribool getFile(std::string const& remoteName, ///< name of remote file to get
                                   std::size_t dataSize,          ///< size of buffer pointed to by data
                                   char** data,                   ///< pointer to a pointer to buffer to place data into
                                   std::size_t& bytesReturned     ///< number of bytes placed in data
                                   ) = 0;

    /// \brief issue get file request to get a remote file to a local file
    ///
    /// \return
    /// \li \c true : on success
    /// \li \c false: on failure
    /// \li \c boost::indeterminate: on remoteName not found.
    /// use CxTransportNotFound to test for not found\n
    /// \sa BOOST_TRIBOOL_THIRD_STATE(CxTransportNotFound)
    virtual boost::tribool getFile(std::string const& remoteName, ///< name of remote file to get
                                   std::string const& localName   ///< name of local file to place data into
                                   ) = 0;

    /// \brief issue get file request to get a remote file to a local file with SHA256 checksum
    ///
    /// \return
    /// \li \c true : on success
    /// \li \c false: on failure
    /// \li \c boost::indeterminate: on remoteName not found.
    /// use CxTransportNotFound to test for not found\n
    /// \sa BOOST_TRIBOOL_THIRD_STATE(CxTransportNotFound)
    virtual boost::tribool getFile(std::string const& remoteName, ///< name of remote file to get
                                   std::string const& localName,   ///< name of local file to place data into
                                   std::string& checksum   ///< buffer to get checksum
                                   ) = 0;

    /// \brief issue heartbeat request to keep connction alive
    ///
    /// \return
    /// \li \c true : on success
    /// \li \c false: on failure
    virtual bool heartbeat(bool forceSend = false) = 0;

    /// \brief returns the status of the last request
    virtual const char* status() = 0;

    /// \brief returns response data from cxps response
    virtual ResponseData getResponseData() = 0;

protected:

private:

};

/// \brief get a CxTransportImp object to use
CxTransportImp::ptr CxTransportImpFactory(TRANSPORT_PROTOCOL transportProtocol,                     ///< protocol to be used HTTP, FTP, file
                                          TRANSPORT_CONNECTION_SETTINGS const& transportSettings,   ///< transport settings to use
                                          bool secure,                                              ///< indicates if ssl should be used true: yes, false: no
                                          bool useCfs = false,                                      ///< indicate if cfs protocal should be used ture: yes false: no
                                          std::string const& psId = std::string()                   ///< process server id to use when useCfs is true
                                          );


#endif // CXTRASNPORTIMP_H
