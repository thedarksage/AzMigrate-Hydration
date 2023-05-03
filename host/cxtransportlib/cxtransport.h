
///
///  \file cxtransport.h
///
///  \brief cx transport client to use when interacting with cx
///

#ifndef CXTRANSPORT_H
#define CXTRANSPORT_H

#include <string>

#include <boost/noncopyable.hpp>
#include <boost/logic/tribool.hpp>

#include "cxtransportimp.h"
#include "cxtransportdefines.h"
#include "cxtransportlogger.h"
#include "compressmode.h"
#include "finddelete.h"
#include "ResponseData.h"
#include "Telemetry/TelemetrySharedParams.h"

/// \brief CxTransportNotFound is an alternate name for boost::indeterminate
///
/// use to explicitly test for not found\n
/// \n
/// e.g.\n
/// \code
///   boost::tribool tb = getFile(remoteName, localName);
///   if (tb) {
///       // file found process it
///   } else if (CxTransportNotFound(tb)) {
///       // file not found
///   } else {
///      // error
///   }
/// \endcode
/// \n
/// see boost.tribool documentation for more details about tribool
BOOST_TRIBOOL_THIRD_STATE(CxTransportNotFound);

/// \brief used to send requests to cx process server
class CxTransport : private boost::noncopyable {
public:
    /// \brief shared pointer type for CxTransport
    typedef boost::shared_ptr<CxTransport> ptr;

    /// \brief constructor
    explicit CxTransport(TRANSPORT_PROTOCOL transportProtocol,                     ///< protocol to use FTP, HTTP, or FILE
                         TRANSPORT_CONNECTION_SETTINGS const& transportSettings,   ///< transport settings to use
                         bool secure,                                              ///< indicates if ssl should be used true: yes, false: no
                         bool useCfs = false,                                      ///< indicate if CFS protocol should be used true: yes false: no
                         std::string const& psId = std::string()                   ///< process server id to connect to when useCfs is true
                         );

    /// \brief destructor
    ~CxTransport() {}

    /// \brief abort an in progess request
    ///
    /// \param remoteName name of the remote file to abort
    /// (optional as it is only needed if aborting a putFile request)
    bool abortRequest(std::string const& remoteName = std::string());

    /// \brief issue put file request to put data to a remote file
    ///
    /// \return
    /// \li \c true : on success
    /// \li \c false: on error
    bool putFile(std::string const& remoteName,                 ///< name of remote file to place data in
                 std::size_t dataSize,                          ///< size of data in buffer pointed to by data
                 char const* data,                              ///< points to buffer holding data to be sent
                 bool moreData,                                 ///< indicates if there is more data to be sent true: yes, false: no
                 COMPRESS_MODE compressMode,                    ///< compress mode (see config/compressmode.h)
                 bool createDirs = false,                       ///< indicates if missing dirs should be created (true: yes, false: no)
                 long long offset = PROTOCOL_DO_NOT_SEND_OFFSET ///< offset to write at (ignored when if using old (tal) transport)
                 );

    bool putFile(std::string const& remoteName,                 ///< name of remote file to place data in
        std::size_t dataSize,                          ///< size of data in buffer pointed to by data
        char const* data,                              ///< points to buffer holding data to be sent
        bool moreData,                                 ///< indicates if there is more data to be sent true: yes, false: no
        COMPRESS_MODE compressMode,                    ///< compress mode (see config/compressmode.h)
        std::map<std::string, std::string> const & headers,         ///< additional headers to send in putfile request
        bool createDirs = false,                       ///< indicates if missing dirs should be created (true: yes, false: no)
        long long offset = PROTOCOL_DO_NOT_SEND_OFFSET ///< offset to write at (ignored when if using old (tal) transport)
    );

    /// \brief issue put file request to put a local file to a remote file
    ///
    /// \return
    /// \li \c true : on success
    /// \li \c false: on error
    bool putFile(std::string const& remoteName,  ///< name of remote file to place data in
                 std::string const& localName,   ///< name of local file to send
                 COMPRESS_MODE compressMode,     ///< compress mode (see config/compressmode.h)
                 bool createDirs = false          ///< indicates if missing dirs should be created (true: yes, false: no)
                 );

    bool putFile(std::string const& remoteName,  ///< name of remote file to place data in
        std::string const& localName,   ///< name of local file to send
        COMPRESS_MODE compressMode,     ///< compress mode (see config/compressmode.h)
        std::map<std::string, std::string> const & headers,           ///< additional headers to send in putfile request
        bool createDirs = false          ///< indicates if missing dirs should be created (true: yes, false: no)
    );

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
    boost::tribool renameFile(std::string const& oldName,                    ///< name of the file to be renamed
                              std::string const& newName,                    ///< new name to give the file
                              COMPRESS_MODE compressMode,                    ///< compress mode in affect (see config/compressmode.h)
                              std::map<std::string, std::string> const& headers,  ///< additional headers to be sent in renamefile request
                              std::string const& finalPaths = std::string()  ///< semi-colon (';') seperated  list of all paths that should get a "copy" of the renamed file
                              );

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
    boost::tribool renameFile(std::string const& oldName,                    ///< name of the file to be renamed
                              std::string const& newName,                    ///< new name to give the file
                              COMPRESS_MODE compressMode,                    ///< compress mode in affect (see config/compressmode.h)
                              std::string const& finalPaths = std::string()  ///< semi-colon (';') seperated  list of all paths that should get a "copy" of the renamed file
                              );

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
    boost::tribool deleteFile(std::string const& names,          ///< semi-colon (';') separated list of names
                              int mode = FindDelete::FILES_ONLY  ///< delete mode to use
                              );

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
    boost::tribool deleteFile(std::string const& names,         ///< semi-colon (';') separated list of names
                              std::string const& fileSpec,      ///< semi-colon (';') separated list of file specs
                              int mode = FindDelete::FILES_ONLY ///< delete mode to use
                              );

    /// \brief issue list file request
    ///
    /// \return
    /// \li \c true : on success
    /// \li \c false: on failure
    /// \li \c boost::indeterminate: on no matches found
    /// use CxTransportNotFound to test for not found\n
    /// \sa BOOST_TRIBOOL_THIRD_STATE(CxTransportNotFound)
    boost::tribool listFile(std::string const& fileSpec, ///< file specification to match against use glob syntax
                            FileInfos_t& files,          ///< holds FileInfos for each match found
                            bool includePath = false     ///< indicates if path should be added: yes, false: no
                            );


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
    boost::tribool getFile(std::string const& remoteName, ///< name of remote file to get
                           std::size_t dataSize,          ///< size of buffer pointed to by data
                           char** data,                   ///< pointer to a pointer to buffer to place data into
                           std::size_t& bytesReturned     ///< number of bytes placed in data
                           );

    /// \brief issue get file request to get a remote file to a local file
    ///
    /// \return
    /// \li \c true : on success
    /// \li \c false: on failure
    /// \li \c boost::indeterminate: on remoteName not found.
    /// use CxTransportNotFound to test for not found\n
    /// \sa BOOST_TRIBOOL_THIRD_STATE(CxTransportNotFound)
    boost::tribool getFile(std::string const& remoteName, ///< name of remote file to get
                           std::string const& localName   ///< name of local file to place data into
                           );

    /// \brief issue get file request to get a remote file to a local file
    ///
    /// \return
    /// \li \c true : on success
    /// \li \c false: on failure
    /// \li \c boost::indeterminate: on remoteName not found.
    /// use CxTransportNotFound to test for not found\n
    /// \sa BOOST_TRIBOOL_THIRD_STATE(CxTransportNotFound)
    boost::tribool getFile(std::string const& remoteName, ///< name of remote file to get
                           std::string const& localName,   ///< name of local file to place data into
                           std::string& checksum   ///< buffer to place the checksum
                           );

    /// \brief issue heartbeat request to keep connction alive
    ///
    /// \return
    /// \li \c true : on success
    /// \li \c false: on failure
    bool heartbeat(bool forceSend = false); ///< should heartbeat be sent even if duration not expired, true: yes, false: no

    /// \brief returns the status of the last request
    const char* status();

    /// \brief gets the response data from cxps response
    ResponseData getResponseData();

protected:

private:
    TRANSPORT_PROTOCOL m_protocol; ///< transport protocol being used by m_imp

    CxTransportImp::ptr m_imp; ///< points to the actual cx transport implementation to be used
};

#endif // CXTRANSPORT_H
