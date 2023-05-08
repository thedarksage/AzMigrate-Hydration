
///
/// \file cxpssync.h
///
/// \brief
///

#ifndef CXPSSYNC_H
#define CXPSSYNC_H

#include <string>
#include <map>
#include <set>

#include <boost/filesystem/path.hpp>
#include <boost/regex.hpp>
#include <boost/tokenizer.hpp>

#include "client.h"

/// \brief simple file/dir sync
class CxpsSync {
public:
    /// \brief type of copy modes supported
    enum RESULT_CODE {
        RESULT_CODE_SUCCESS = 0,            ///< Success
        RESULT_CODE_USAGE = 1,              ///< Syntax or usage error
        RESULT_CODE_UNUSED_2 = 2,           ///< Protocol incompatibility
        RESULT_CODE_FILES_DIRS = 3,         ///< Errors selecting input/output files, dirs
        RESULT_CODE_UNUSED_4 = 4,           ///< Requested  action  not supported
        RESULT_CODE_SERVER = 5,             ///< Error starting client-server protocol
        RESULT_CODE_UNUSED_6 = 6,           ///< Daemon unable to append to log-file
        RESULT_CODE_NETWORK_ERROR = 10,     ///< Error in socket I/O
        RESULT_CODE_FILE_IO = 11,           ///< Error in file I/O
        RESULT_CODE_UNUSED_12 = 12,         ///< Error in rsync protocol data stream
        RESULT_CODE_UNUSED_13 = 13,         ///< Errors with program diagnostics
        RESULT_CODE_UNUSED_14 = 14,         ///< Error in IPC code
        RESULT_CODE_UNUSED_20 = 20,         ///< Received SIGUSR1 or SIGINT
        RESULT_CODE_UNUSED_21 = 21,         ///< Some error returned by waitpid()
        RESULT_CODE_MEMORY_ERROR = 22,      ///< Error allocating core memory buffers
        RESULT_CODE_UNUSED_23 = 23,         ///< Partial transfer due to error
        RESULT_CODE_UNUSED_24 = 24,         ///< Partial transfer due to vanished source files
        RESULT_CODE_UNUSED_25 = 25,         ///< The --max-delete limit stopped deletions
        RESULT_CODE_TIMEOUT = 30,           ///< Timeout in data send/receive
        RESULT_CODE_CONNECT_TIMEOUT = 35 ,  ///< Timeout waiting for daemon connection
        RESULT_CODE_INTERNAL_ERROR = 999    ///< Timeout waiting for daemon connection
    };

    /// \brief constructor
    CxpsSync();

    /// \brief runs the file/dir sync request
    bool run(int argc,      ///< number of arguments in argv
             char* argv[]); ///< arguments for the request (typically command line arguments

    /// \brief gets errors
    ///
    /// \return std::string containing all errors
    std::string getErrors()
        {
            return m_errors;
        }

    void usage();

    int resultCode()
        {
            return (int)m_resultCode;
        }

protected:
    enum COPY_MODE {
        COPY_MODE_GET,    ///< get files from remote system
        COPY_MODE_PUT,    ///< put files to remote system
        COPY_MODE_LOCAL   ///< both src and dst are local
    };

    enum VERBOSE_LEVEL {
        VERBOSE_QUIET = 0,
        VERBOSE_VERY_LOW,
        VERBOSE_LOW,
        VERBOSE_MEDIUM,
        VERBOSE_HIGH,
        VERBOSE_VERY_HIGH
    };

    // these are the rsync return codes not all are used

    typedef std::map<std::string, std::string> options_t;                  ///< hold options found first: tag, second: value (if has one)
    typedef boost::filesystem::directory_iterator dirIter_t;               ///< used for interating over directories
    typedef std::vector<boost::regex> regex_t;                             ///< used for holding regular expressions
    typedef std::vector<char const*> longOptionTags_t;                     ///< used to hold long option tags
    typedef std::vector<char> shortOptionTags_t;                           ///< used to hold short option tags
    typedef std::vector<std::string> srcDirs_t;                            ///< used to hold source dirctoires to be copied
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;    ///< used for creating tokenizers
    typedef std::set<std::string> failedFiles_t;                           ///< used for hold list of files that failed sync

    /// \brief parses the source option
    ///
    /// source option has format [cxpsserver::]src
    /// if 'cxpsserver::' present that is stripped and placed into m_cxpsServer
    /// and m_copyMode is set to COPY_MODE_GET
    ///
    /// the src can be
    ///  * single source
    ///  * multiple sources with spaces between
    ///  * ingle source with spaces (requires spaces to be escaped)
    void parseSrc(char const* src); ///< source option to parse

    /// \brief parses the destination option
    ///
    /// destination  option has format [cxpsserver::]dst
    /// if 'cxpsserver::' present that is stripped and placed into m_cxpsServer
    /// and m_copyMode is set to COPY_MODE_PUT
    ///
    /// dst will be
    /// * a single path
    /// * job_<id> where <id> is a number. job_<id> indicates the sync job
    void parseDst(char const* dst); ///< destination option to parse

    /// \brief resovces cxps server ti ip address if not already ip address
    void resolveCxpsServerIpAddress();

    /// \brief sets cxps info if present: ip adress, port, user, password
    void setCxpsInfo();

    /// \brief checks if str matches any of the regular expressions on the exclude from list
    ///
    /// \return bool true: matched, false: no match (this includes no entries on exclude from list)
    bool matchesExcludeFromRegex(std::string const& str); ///< str to check if it matches exclude from directives

    /// \brief builds the exclude from list by reading the entries in filename
    void buildExcludeFromRegex(char const* fileName); ///< name of file that conatins the exclude from directives

    /// \brief processes the list of sources to get them copied to destintion
    void copySrcsToDst(ClientAbc* client); ///< cxps client to be used for copying to cxps

    /// \brief processes a given source making sure its files/dirs are copied to destination
    void copySrcToDst(ClientAbc* client,                          ///< cxps client to be used for copying to cxps
                      boost::filesystem::path const& src,         ///< src file/dir to copy to cxps
                      boost::filesystem::path const& topSrcDir);  ///< the top level source directory needed for mapping source to destination names

    /// \brief copies fileToCopy to destintion
    void copyFile(ClientAbc* client,                           ///< cxps client to be used for copying to cxps
                  boost::filesystem::path const& fileToCopy,   ///< src file/dir to copy to cxps
                  boost::filesystem::path const& topSrcDir);   ///< the top level source directory needed for mapping source to destination names

    /// \brief gets files from cxps and places them in local destination
    void getFilesFromCxps(ClientAbc* client,                       ///< cxps client used to get file/dirs from cxps
                          std::string const& files,                ///< list of file/dirs to get from cxps
                          boost::filesystem::path const& prefix);  ///< cxps source prefix needed to map source to destination names

    /// \breif top level get files/dirs from cxps to local system
    void getFromCxps();

    /// \brief top level put local files/dirs to cxps
    void putToCxps();

    /// \brief top level copy files from local source to local destination
    void localCopy();

    /// \brief calls the correct function to process the given requests
    void processRequest();

    /// \brief parses the arguments passed into run
    ///
    /// \return bool true: succes, false: errors m_errors will hold error details
    bool parseArgs(int argc,      ///< number of agruments in argv
                   char* argv[]); ///< arguments

    /// \brief strips off the cxps server and place it in m_cxpsServer
    ///
    /// if presen, str will begin with 'cxpsserver::'
    /// cxpsserver can be name or ip address
    ///
    /// \return bool true: indicates string contained cxps server, false: no cxps server present (note this is not an error)
    bool stripCxpsServerIfExists(std::string& str); ///< string that may begin with the cxps server

    /// \brief strips the prefix from name
    ///
    /// prefix can be
    ///   \li full path: /full/path/prefix or c:\full\path\prefix
    ///   in this case strip off prefix starting from begining of name
    ///
    ///   \li prefix: /dir1/dir2
    ///   \li name: /dir1/dir2/file1
    ///
    ///   want to strip off /dir1/ so that the final name is dir2/file1
    ///
    ///   \li relative path: relative/path/prefix
    ///   this can happen when geting files from cxps as it will return full paths
    ///   for all file/dir names, but when the get from cxps was made it may have used
    ///   a relative path with the assumption the files are under the a specific dir
    ///   that cxps knows about.
    ///
    ///   So in this case need to skip over the initial parts
    ///   of the full path returned by cxps until you find the first part that matcehs
    ///   the first part of the prefix and then continue to strip from ther
    ///   e.g.
    ///
    ///   \li inmsync --port=9080 cxpsserver::src/data /tmp/dst
    ///
    ///   cxps would return file/dir names something lke this
    ///
    ///   \li /home/svsystems/transport/src/data
    ///
    ///   so when stripPrefix is called
    ///
    ///   \li name: /home/svsystems/transport/src/data/file.txt,
    ///   \li prefix: src/data
    ///
    ///   want to strip /home/svsystems/transport/src/ so the final name is
    ///   data/file.txt
    ///
    /// \return boost:filesystem::path that holds the name with prefix stripped off
    boost::filesystem::path stripPrefix(boost::filesystem::path const& prefix, ///< prefix to be stripped
                                        boost::filesystem::path const& name);  ///< name that gets prefix stripped from

    /// \brief tracks last run
    void trackRun();

    /// \brief get last run tracking
    void loadLastRun();

    /// \brief prints on stats based on verbose setting
    void printStats();

    /// \brief check if paths match (ignore trailing slash (backslash)
    bool pathsMatch(std::string const& path1, std::string const& path2);

    /// \brief convert all \ to / and remove repeated \  or /
    void normalizePathName(std::string& str);
    
private:
    RESULT_CODE m_resultCode; ///< holds the result code of the run

    boost::regex m_ipRegex;   ///< regular expression that matches an ip address dot notation

    COPY_MODE m_copyMode; ///< copy mode, can be get from cxps, put to cxps, or local

    int m_maxBufferSizeBytes;     ///< buffers size to be used by cxps client when talking to cxps. Deafult 1MB
    int m_connectTimeoutSeconds;  ///< time in seconds that cxps client will wait on connect to complete before giving up
    int m_timeoutSeconds;         ///< time in seconds that cxps client will wait for interacting with cxps before giving up
    int m_sendWindowSizeBytes;    ///< size in bytes to set the cxps client send tcp window size. Default 1MB
    int m_receiveWindowSizeBytes; ///< size in bytes to set the cxps client receive tcp window size. Default !MB
    int m_writeMode;              ///< write file mode to be used. Default WRITE_MODE_NORMAL (other option is WRITE_MODE_FLUSH)

    COMPRESS_MODE m_compressMode; ///< indicates the compress mode to use. Default is COMPRESS_NONE (could be COMPRESS_SOURCESIDE if -z option used)

    bool m_keepAlive; ///< indicates if the connection should be kept alive. Default true

    int m_verbose; ///< level of out put

    bool m_daemon;
    
    unsigned long long m_totalBytesSent;            ///< holds total bytes sent
    unsigned long long m_totalFileSizes;           ///< holsts total size of all files processed (sent or not)
    unsigned long long m_literalSize;              ///< literal size, size of all modified files
    unsigned long long m_totalFileCount;           ///< total number of files processed
    unsigned long long m_totalFileTransferCount;   ///< total number of files transfered

    time_t m_totalTransferTimeSeconds;             ///< total time in seconds to transfer data

    regex_t m_excludeFileSpecRegex; ///< holds list of exclude from directives (these are globs converted to regular expressions)

    longOptionTags_t m_longOptionTags;         ///< holds long options supported
    longOptionTags_t m_ignoredLongOptionTags;  ///< holds long opotions that are knoen but currently ignored

    shortOptionTags_t m_shortOptionTags;         ///< holds short options supported
    shortOptionTags_t m_ignoredShortOptionTags;  ///< holds short options that are known but currently ignored

    options_t m_options; ///< holds the actual supported short/long options that were specified

    srcDirs_t m_src; ///< holds all the source directories/files to be copied

    std::string m_dst;           ///< holds the destination directory where files/dirs are to be copied
    std::string m_cxpsServer;    ///< holds cxps ip address when taling to cxps
    std::string m_cxpsPort;      ///< holds cxps port when talking to cxps (if --cert used, this will be ssl port otherwise non ssl port)
    std::string m_cxpsCert;      ///< holds cxps cert file name to use for secure mode (this being set indicates secure mode)
    std::string m_user;          ///< holds cxps user when talking to cxps
    std::string m_password;      ///< holds cxps password when talking to cxps
    std::string m_errors;        ///< holds any errors detected during procssing. Empty indicates no errors
    std::string m_trackingFile;  ///< holds name of file to hold tracking info about last successful run
    std::string m_jobId;         ///< holds job id being run
    
    time_t m_currentRunTime; ///< time current run started
    time_t m_lastRunTime;    ///< time of last run

    failedFiles_t m_failedFilesLastRun;    ///< holds list of files that failed sync last run so they can be reprocessed if still exist.
    failedFiles_t m_failedFilesCurrentRun;    ///< holds list of files that failed sync last run so they can be reprocessed if still exist.

};

#endif // CXPSSYNC_H
