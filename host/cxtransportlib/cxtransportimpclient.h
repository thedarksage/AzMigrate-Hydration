
///
///  \file cxtransportimpclient.h
///
///  \brief cx transport implementation using cxpslib client
///

#ifndef CXTRANSPORTIMPCLIENT_H
#define CXTRANSPORTIMPCLIENT_H

#include <string>
#include <vector>
#include <utility>

#include "volumegroupsettings.h"
#include "transport_settings.h"
#include "cxtransportimp.h"
#include "client.h"
#include "compressmode.h"

/// \brief cx transport when using the cxpslib client
class CxTransportImpClient : public CxTransportImp {
public:

    typedef std::pair<std::string, std::string> remapPrefixFromTo_t; ///< Remap prefix type for file tal operations

    /// \brief constructor
    explicit CxTransportImpClient(TRANSPORT_PROTOCOL transportProtocol,
                                  TRANSPORT_CONNECTION_SETTINGS const& settings,
                                  bool secure,
                                  bool useCfs = false,
                                  std::string const& psId = std::string())
        : m_protocol(transportProtocol),
          m_settings(settings),
          m_secure(secure),
          m_useCfs(useCfs),
          m_psId(psId)
        {
            resetClientImp();
        }

    /// \brief destructor
    virtual ~CxTransportImpClient() { }

    /// \brief abort an in progess request
    ///
    /// \param remoteName name of the remote file to abort
    /// (optional as it is only needed if aborting a putFile request)
    virtual bool abortRequest(std::string const& remoteName = std::string());

    /// \brief issue put file request to put data to a remote file
    virtual bool putFile(std::string const& remoteName,                  ///< name of remote file to place data in
                         std::size_t dataSize,                           ///< size of data in buffer pointed to by data
                         char const* data,                               ///< points to buffer holding data to be sent
                         bool moreData,                                  ///< indicates if there is more data to be sent true: yes, false: no
                         COMPRESS_MODE compressMode,                     ///< compress mode (see config/compressmode.h)
                         bool createDirs = false,                        ///< indicates if missing dirs should be created (true: yes, false: no)
                         long long offset = PROTOCOL_DO_NOT_SEND_OFFSET ///< offset to write at (ignored when if using old (tal) transport)
                         );

    /// \brief issue put file request to put data to a remote file
    virtual bool putFile(std::string const& remoteName,                  ///< name of remote file to place data in
        std::size_t dataSize,                           ///< size of data in buffer pointed to by data
        char const* data,                               ///< points to buffer holding data to be sent
        bool moreData,                                  ///< indicates if there is more data to be sent true: yes, false: no
        COMPRESS_MODE compressMode,                     ///< compress mode (see config/compressmode.h)
        std::map<std::string, std::string> const & headers,   ///< additional headers to send in putfile request
        bool createDirs = false,                        ///< indicates if missing dirs should be created (true: yes, false: no)
        long long offset = PROTOCOL_DO_NOT_SEND_OFFSET ///< offset to write at (ignored when if using old (tal) transport)
    );

    /// \brief issue put file request to put a local file to a remote file
    virtual bool putFile(std::string const& remoteName,  ///< name of remote file to place data in
                         std::string const& localName,   ///< name of local file to send
                         COMPRESS_MODE compressMode,     ///< compress mode (see config/compressmode.h)
                         bool createDirs = false        ///< indicates if missing dirs should be created (true: yes, false: no)
                         );

    /// \brief issue put file request to put a local file to a remote file
    virtual bool putFile(std::string const& remoteName,  ///< name of remote file to place data in
        std::string const& localName,   ///< name of local file to send
        COMPRESS_MODE compressMode,     ///< compress mode (see config/compressmode.h)
        std::map<std::string, std::string> const &headers,  ///< additional headers to send in putfile request
        bool createDirs = false        ///< indicates if missing dirs should be created (true: yes, false: no)
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
    virtual boost::tribool renameFile(std::string const& oldName,                    ///< name of the file to be renamed
                                      std::string const& newName,                    ///< new name to give the file
                                      COMPRESS_MODE compressMode,                    ///< compress mode in affect (see config/compressmode.h)
                                      mapHeaders_t const& headers,                   ///< additional headers to be sent in renamefile request
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
    virtual boost::tribool renameFile(std::string const& oldName,                    ///< name of the file to be renamed
                                      std::string const& newName,                    ///< new name to give the file
                                      COMPRESS_MODE compressMode,                    ///< compress mode in affect (see config/compressmode.h)
                                      std::string const& finalPaths = std::string()  ///< semi-colon (';') seperated  list of all paths that should get a "copy" of the renamed file
                                      );

    /// \brief issue delete file request
    ///
    /// see client.h Clinet::deleteFile for complete details
    /// \return
    /// \li \c true : on success
    /// \li \c false: on failure
    /// \li \c boost::indeterminate: on remoteName not found.
    /// use CxTransportNotFound to test for not found\n
    /// \sa BOOST_TRIBOOL_THIRD_STATE(CxTransportNotFound)
    virtual boost::tribool deleteFile(std::string const& names,          ///< semi-colon (';') separated list of names
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
    virtual boost::tribool deleteFile(std::string const& names,         ///< semi-colon (';') separated list of names
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
    virtual boost::tribool listFile(std::string const& fileSpec, ///< file specification to use to match against use glob syntax
                                    FileInfos_t& files,          ///< holds FileInfos for each match found
                                    bool includePath = false     ///< indicates if complete paths should be used true: yes, false: no
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
    /// \li \c currently if the remote file is larger then dataSize, data
    /// will be filled up to dataSize, true will be returned and  status
    /// will be set to "buffer to small" and the request will be aborted.
    /// It is up to the caller to determine if it is OK to continue in
    /// this case or retry either using getFile request that gets the
    /// remote file to a local file or with a larger buffer
    virtual boost::tribool getFile(std::string const& remoteName,   ///< name of remote file to get
                                   std::size_t dataSize,            ///< size of buffer pointed to by data
                                   char** data,                     ///< pointer to a pointer to buffer to place data into
                                   std::size_t& totalBytesReturned  ///< number of bytes placed in data
                                   );

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
                                   );

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
                                   );

    /// \brief issue heartbeat request to keep connction alive
    ///
    /// \return
    /// \li \c true : on success
    /// \li \c false: on failure
    virtual bool heartbeat(bool forceSend = false);

    /// \brief returns the status of the last request
    virtual const char* status()
    {
        m_statusExtended = m_status;
        if (!m_statusExtended.empty()) {
            m_statusExtended += " cxps " + m_settings.ipAddress;
        }
        return m_statusExtended.c_str();
    }

    /// \brief returns the response data from cxps response
    virtual ResponseData getResponseData();

protected:
    /// \brief instanties the Client class that should be used
    void resetClientImp();

    void resetRequest();

    /// \brief puts the results of listFile into a FileInfos_t
    void buildFileInfos(std::string const& files,  ///< list file data returned
                        FileInfos_t& fileInfos,    ///< receives the list file data
                        bool includePath           ///< indicates if paths should be include true: yes, false: no
                        );


    /// \brief gets the remap prefix for file operations for file client
    remapPrefixFromTo_t getRemapPrefixFromToForFileClient(void);

    /// \brief gets the remap prefix for file operations from conf value
    remapPrefixFromTo_t getRemapPrefixFromToFromConfValue(const std::string &fromTo);

private:

    TRANSPORT_CONNECTION_SETTINGS m_settings;

    TRANSPORT_PROTOCOL m_protocol;

    bool m_secure;

    bool m_useCfs;

    std::string m_psId;

    ClientAbc::ptr m_client; ///< holds the client object to be used for issuing requests

    std::string m_status; ///< holds status of last request (empty indicates success)
    std::string m_statusExtended; ///< holds status of last request + ps ip, can be extended
    ResponseData m_responseData; ///< holds the response for the last request
};


#endif // CXTRANSPORTIMPCLIENT_H
