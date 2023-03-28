
///
/// \file cxpssync.cpp
///
/// \brief
///

#include <iostream>
#include <iomanip>
#include <limits>
#include <fstream>

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>

#include "cxpssync.h"
#include "globtoregex.h"
#include "createpaths.h"
#include "genpassphrase.h"

// NOTE: remember to add new ones to either m_longOptionTags or m_ignoredLongOptionTag lists
char const * const OPT_LONG_USER = "user";                           ///< cxps user
char const * const OPT_LONG_PASSWORD = "password";                   ///< cxps password
char const * const OPT_LONG_PORT = "port";                           ///< specify double-colon alternate port number
char const * const OPT_LONG_EXCLUDE_FROM = "exclude-from";           ///< read exclude patterns from FILE
char const * const OPT_LONG_DELETE_EXCLUDED = "delete-excluded";     ///< also delete excluded files from dest dirs
char const * const OPT_LONG_DELETE_AFTER = "delete-after";           ///< receiver deletes after transfer, not before
char const * const OPT_LONG_DELETE = "delete";                       ///< delete extraneous files from dest dirs
char const * const OPT_LONG_EXISTING = "existing";                   ///< skip creating new files on receiver
char const * const OPT_LONG_SIZE_ONLY = "size-only";                 ///< skip files that match in size
char const * const OPT_LONG_IGNORE_EXISTING = "ignore-existing";     ///< skip updating files that exist on receiver
char const * const OPT_LONG_SUFFIX = "suffix";                       ///< backup suffix (default ~ w/o --backup-dir)
char const * const OPT_LONG_BACKUP_DIR = "backup-dir";               ///< make backups into hierarchy based in DIR
char const * const OPT_LONG_TEMP_DIR = "temp-dir";                   ///< create temporary files in directory DIR
char const * const OPT_LONG_BLOCK_SIZE = "block-size";               ///< force a fixed checksum block-size
char const * const OPT_LONG_MODIFY_WINDOW = "modify-window";         ///< compare mod-times with reduced accuracy
char const * const OPT_LONG_BWLIMIT = "bwlimit";                     ///< limit I/O bandwidth; KBytes per second
char const * const OPT_LONG_TIMEOUT = "timeout";                     ///< set I/O timeout in seconds
char const * const OPT_LONG_LOG_FORMAT = "log-format";               ///< ??
char const * const OPT_LONG_IGNORE_ERRORS = "ignore-errors";         ///< delete even if there are I/O errors
char const * const OPT_LONG_FORCE = "force";                         ///< force deletion of dirs even if not empty
char const * const OPT_LONG_PARTIAL = "partial";                     ///< keep partially transferred files
char const * const OPT_LONG_COPY_UNSAFE_LINKS = "copy-unsafe-links"; ///< only "unsafe" symlinks are transformed
char const * const OPT_LONG_SAFE_LINKS = "safe-links";               ///< ignore symlinks that point outside the tree
char const * const OPT_LONG_BLOCKING_IO = "blocking-io";             ///< use blocking I/O for the remote shell
char const * const OPT_LONG_NO_BLOCKING_IO = "no-blocking-io";       ///< ??
char const * const OPT_LONG_STATS = "stats";                         ///< give some file-transfer stats
char const * const OPT_LONG_PROGRESS = "progress";                   ///< show progress during transfer
char const * const OPT_LONG_JOBID = "jobid";                         ///< job id
char const * const OPT_LONG_NOOP = "noop";                           ///< indicates running on destination which should do nothing but report success
char const * const OPT_LONG_CERT = "cert";                           ///< certificate file name to use for secure transfer

// NOTE: remember to add new ones to either m_shortOptionTags or m_ignoredShortOptionTags lists
char const OPT_SHORT_RECURSE_DIRS = 'r';                ///< recurse directories
char const OPT_SHORT_COMPRESS = 'z';                    ///< compress file data during transfer
char const OPT_SHORT_COPY_WHOLE = 'W';                  ///< copy files whole (w/o delta-xfer algorithm)
char const OPT_SHORT_BACKUP = 'b';                      ///< make backups (see --suffix & --backup-dir)
char const OPT_SHORT_SPARSE = 'S';                      ///< handle sparse files efficiently
char const OPT_SHORT_SKIP_NEWER = 'u';                  ///< skip files that are newer on the receiver
char const OPT_SHORT_NO_SKIP_SIZE_TIME = 'I';           ///< don't skip files that match size and time
char const OPT_SHORT_SKIP_CHECKSUM = 'c';               ///< skip based on checksum, not mod-time & size
char const OPT_SHORT_COPY_SYMLINKS = 'l';               ///< copy symlinks as symlinks
char const OPT_SHORT_TRANSFORM_SYMLINK = 'L';           ///< transform symlink into referent file/dir
char const OPT_SHORT_PRESERVE_HARD_LIN = 'H';           ///< preserve hard links
char const OPT_SHORT_PRESERVE_ACLS = 'A';               ///< preserve ACLs (implies -p)
char const OPT_SHORT_PRESERVE_PERMISSIONS = 'p';        ///< preserve permissions
char const OPT_SHORT_PRESERVE_OWNER = 'o';              ///< preserve owner (super-user only)
char const OPT_SHORT_PRESERVE_GROUP = 'g';              ///< preserve group
char const OPT_SHORT_PRESERVE_DEVICE_AND_SPECIAL= 'D';  ///< same as --devices --specials preserve device files (super-user only) preserve special files
char const OPT_SHORT_PRESERVE_MOD_TIMES = 't';          ///< preserve modification times
char const OPT_SHORT_NO_CROSS_FILESYSTEM = 'x';         ///< don't cross filesystem boundaries
char const OPT_SHORT_VERBOSE = 'v';                     ///< verbose option add multple to increase verosity

CxpsSync::CxpsSync()
    : m_resultCode(RESULT_CODE_SUCCESS),
      m_ipRegex("^[0-9]+[.][0-9]+[.][0-9][.][0-9]+", boost::regex::perl),
      m_copyMode(COPY_MODE_LOCAL),
      m_maxBufferSizeBytes(1048576),
      m_connectTimeoutSeconds(10),
      m_timeoutSeconds(300),
      m_sendWindowSizeBytes(1048576),
      m_receiveWindowSizeBytes(1048576),
      m_writeMode(WRITE_MODE_NORMAL),
      m_compressMode(COMPRESS_NONE),
      m_keepAlive(true),
      m_verbose(VERBOSE_QUIET),
      m_daemon(false),
      m_totalBytesSent(0),
      m_totalFileSizes(0),
      m_literalSize(0),
      m_totalFileCount(0),
      m_totalFileTransferCount(0),
      m_totalTransferTimeSeconds(0),
      m_lastRunTime(0)
{
    // supported long options
    m_longOptionTags.push_back(OPT_LONG_USER);
    m_longOptionTags.push_back(OPT_LONG_PASSWORD);
    m_longOptionTags.push_back(OPT_LONG_PORT);
    m_longOptionTags.push_back(OPT_LONG_EXCLUDE_FROM);
    m_longOptionTags.push_back(OPT_LONG_JOBID);
    m_longOptionTags.push_back(OPT_LONG_NOOP);
    m_longOptionTags.push_back(OPT_LONG_CERT);

    // MAYBE: eventually support some of these
    m_ignoredLongOptionTags.push_back(OPT_LONG_DELETE);
    m_ignoredLongOptionTags.push_back(OPT_LONG_DELETE_EXCLUDED);
    m_ignoredLongOptionTags.push_back(OPT_LONG_DELETE_AFTER);
    m_ignoredLongOptionTags.push_back(OPT_LONG_EXISTING);
    m_ignoredLongOptionTags.push_back(OPT_LONG_SIZE_ONLY);
    m_ignoredLongOptionTags.push_back(OPT_LONG_IGNORE_EXISTING);
    m_ignoredLongOptionTags.push_back(OPT_LONG_SUFFIX);
    m_ignoredLongOptionTags.push_back(OPT_LONG_BACKUP_DIR);
    m_ignoredLongOptionTags.push_back(OPT_LONG_TEMP_DIR);
    m_ignoredLongOptionTags.push_back(OPT_LONG_BLOCK_SIZE);
    m_ignoredLongOptionTags.push_back(OPT_LONG_MODIFY_WINDOW);
    m_ignoredLongOptionTags.push_back(OPT_LONG_BWLIMIT);
    m_ignoredLongOptionTags.push_back(OPT_LONG_TIMEOUT);
    m_ignoredLongOptionTags.push_back(OPT_LONG_LOG_FORMAT);
    m_ignoredLongOptionTags.push_back(OPT_LONG_IGNORE_ERRORS);
    m_ignoredLongOptionTags.push_back(OPT_LONG_FORCE);
    m_ignoredLongOptionTags.push_back(OPT_LONG_PARTIAL);
    m_ignoredLongOptionTags.push_back(OPT_LONG_COPY_UNSAFE_LINKS);
    m_ignoredLongOptionTags.push_back(OPT_LONG_SAFE_LINKS);
    m_ignoredLongOptionTags.push_back(OPT_LONG_BLOCKING_IO);
    m_ignoredLongOptionTags.push_back(OPT_LONG_NO_BLOCKING_IO);
    m_ignoredLongOptionTags.push_back(OPT_LONG_STATS);
    m_ignoredLongOptionTags.push_back(OPT_LONG_PROGRESS);

    // supported short options
    m_shortOptionTags.push_back(OPT_SHORT_RECURSE_DIRS);
    m_shortOptionTags.push_back(OPT_SHORT_COMPRESS);
    m_shortOptionTags.push_back(OPT_SHORT_VERBOSE);

    // MAYBE: support some of these
    m_ignoredShortOptionTags.push_back(OPT_SHORT_COPY_WHOLE);
    m_ignoredShortOptionTags.push_back(OPT_SHORT_BACKUP);
    m_ignoredShortOptionTags.push_back(OPT_SHORT_SPARSE);
    m_ignoredShortOptionTags.push_back(OPT_SHORT_SKIP_NEWER);
    m_ignoredShortOptionTags.push_back(OPT_SHORT_NO_SKIP_SIZE_TIME);
    m_ignoredShortOptionTags.push_back(OPT_SHORT_SKIP_CHECKSUM);
    m_ignoredShortOptionTags.push_back(OPT_SHORT_COPY_SYMLINKS);
    m_ignoredShortOptionTags.push_back(OPT_SHORT_TRANSFORM_SYMLINK);
    m_ignoredShortOptionTags.push_back(OPT_SHORT_PRESERVE_HARD_LIN);
    m_ignoredShortOptionTags.push_back(OPT_SHORT_PRESERVE_ACLS);
    m_ignoredShortOptionTags.push_back(OPT_SHORT_PRESERVE_PERMISSIONS);
    m_ignoredShortOptionTags.push_back(OPT_SHORT_PRESERVE_OWNER);
    m_ignoredShortOptionTags.push_back(OPT_SHORT_PRESERVE_GROUP);
    m_ignoredShortOptionTags.push_back(OPT_SHORT_PRESERVE_DEVICE_AND_SPECIAL);
    m_ignoredShortOptionTags.push_back(OPT_SHORT_PRESERVE_MOD_TIMES);
    m_ignoredShortOptionTags.push_back(OPT_SHORT_NO_CROSS_FILESYSTEM);
}

bool CxpsSync::run(int argc, char* argv[])
{
    try {
        m_currentRunTime = time(0);
        m_errors.clear();
        if (parseArgs(argc, argv)) {
            if (m_options.end() != m_options.find(OPT_LONG_NOOP)) {
                // either daemon mode or attempting to run client on destination
                // in either case should just return success and exit
                return true;
            }
            loadLastRun();
            processRequest();
            trackRun();
            printStats();
        }
    } catch (ErrorException const& e) {
        m_resultCode = RESULT_CODE_MEMORY_ERROR; // FIXME: is the the only type of except to be caught here?
        m_errors += e.what();
        m_errors += "\n";
    } catch (std::exception const& e) {
        m_resultCode = RESULT_CODE_FILES_DIRS; // FIXME: is the the only type of except to be caught here?
        m_errors += e.what();
        m_errors += "\n";
    } catch (...) {
        m_resultCode = RESULT_CODE_INTERNAL_ERROR;
        m_errors += "unknown exception";
    }
    return m_errors.empty();
}

/// \brief converts \ to / as well as removes any repeating \ or / in a single pass
void CxpsSync::normalizePathName(std::string& str)
{
    std::string::size_type curIdx = 0;
    std::string::size_type lstIdx = 0;
    std::string::size_type endIdx = str.size();
    while (curIdx < endIdx) {
        if (curIdx > lstIdx) {
            str[lstIdx] = str[curIdx];
        }
        if ('/' == str[curIdx] || '\\' == str[curIdx]) {
            if ('\\' == str[lstIdx]) {
                str[lstIdx] = '/';
            }
            ++lstIdx;
            do {
                ++curIdx;
            } while (curIdx < endIdx && ('\\' == str[curIdx] || '/' == str[curIdx]));
        } else {
            ++lstIdx;
            ++curIdx;
        }
    }
    if (lstIdx < endIdx) {
        str.erase(lstIdx);
    }
}

void CxpsSync::parseSrc(char const* src)
{
    std::string srcDir(src);
    if (stripCxpsServerIfExists(srcDir)) {
        m_copyMode = COPY_MODE_GET;
    }

    // source could be a
    //  * single source
    //  * multiple sources with spaces between
    //  * ingle source with spaces (requires spaces to be escaped)
    std::string dir;
    boost::char_separator<char> sep(" ");
    tokenizer_t tokens(srcDir, sep);
    tokenizer_t::iterator iter(tokens.begin());
    tokenizer_t::iterator iterEnd(tokens.end());
    for (/* empty*/; iter != iterEnd; ++iter) {
        dir += *iter;
        if ('\\' != dir[dir.size() - 1]) {
            normalizePathName(dir);
            m_src.push_back(dir);
            dir.clear();
        } else {
            dir[dir.size() - 1] = ' ';
        }
    }
}

void CxpsSync::parseDst(char const* dst)
{
    m_dst = dst;
    if (stripCxpsServerIfExists(m_dst)) {
        m_copyMode = COPY_MODE_PUT;
    }
    normalizePathName(m_dst);
    if (':' == m_dst[m_dst.size() -1]) {
        m_dst += "/";
    }
}

void CxpsSync::resolveCxpsServerIpAddress()
{
    if (boost::regex_match(m_cxpsServer, m_ipRegex)) {
        // no need to resolve already have ip address
        return;
    }
    boost::asio::io_service ioService;
    boost::asio::ip::tcp::resolver resolver(ioService);
    boost::asio::ip::tcp::resolver::query query(m_cxpsServer, m_cxpsPort);
    boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
    m_cxpsServer = endpoint.address().to_string();
}

void CxpsSync::setCxpsInfo()
{
    options_t::iterator findIter;
    if (!m_cxpsServer.empty()) {
        findIter = m_options.find(OPT_LONG_PORT);
        if (m_options.end() != findIter) {
            m_cxpsPort = (*findIter).second;
        }
        resolveCxpsServerIpAddress();
    }
    findIter = m_options.find(OPT_LONG_USER);
    if (m_options.end() != findIter) {
        m_user = (*findIter).second;
        findIter = m_options.find(OPT_LONG_PASSWORD);
        if (m_options.end() != findIter) {
            m_password = (*findIter).second;
        }
    }
    findIter = m_options.find(OPT_LONG_CERT);
    if (m_options.end() != findIter) {
        m_cxpsCert = (*findIter).second;
    }
}

bool CxpsSync::matchesExcludeFromRegex(std::string const& str)
{
    regex_t::const_iterator iterEnd(m_excludeFileSpecRegex.end());
    for (regex_t::const_iterator iter = m_excludeFileSpecRegex.begin(); iter != iterEnd; ++iter) {
        if (boost::regex_match(str, *iter)) {
            return true;
        }
    }
    return false;
}


void CxpsSync::buildExcludeFromRegex(char const* fileName)
{
    std::ifstream file(fileName);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            std::string regx;
            // FIXME: currently only support exclude patterns
            if ('+' != line[0]) {
                if (boost::algorithm::starts_with(line, "- ")) {
                    line.erase(0, 2);
                }
                Glob::globToRegex(line, regx);
                m_excludeFileSpecRegex.push_back(boost::regex(regx, boost::regex::perl));
            }
            line.clear();
        }
    }
}

void CxpsSync::copySrcsToDst(ClientAbc* client)
{
    srcDirs_t::iterator iterEnd(m_src.end());
    for (srcDirs_t::iterator iter = m_src.begin(); iter != iterEnd; ++iter) {
        if (!boost::filesystem::exists(*iter)) {
            m_errors += "source does not exist: ";
            m_errors += *iter;
            m_errors += "\n";
            m_resultCode = RESULT_CODE_FILES_DIRS;
        } else if (COPY_MODE_LOCAL != m_copyMode || !pathsMatch(*iter, m_dst)) {
            // OK src and dst our not the same local path
            if (boost::filesystem::is_directory(boost::filesystem::status(*iter))) {
                copySrcToDst(client, *iter, *iter);
            } else {
                copyFile(client, *iter, std::string());
            }
        }
    }
}

void CxpsSync::copySrcToDst(ClientAbc* client,
                            boost::filesystem::path const& src,
                            boost::filesystem::path const& topSrcDir)
{
    if (boost::filesystem::is_directory(boost::filesystem::status(src))) {
        boost::filesystem::directory_iterator dirEnd;
        boost::filesystem::directory_iterator dirIter(src);
        for (/* empty */; dirIter != dirEnd; ++dirIter) {
            if (m_excludeFileSpecRegex.empty() || !matchesExcludeFromRegex(dirIter->path().filename().string())) {
                if (boost::filesystem::is_directory(dirIter->status())) {
                    copySrcToDst(client, dirIter->path(), topSrcDir);
                } else {
                    copyFile(client, dirIter->path(), topSrcDir);
                }
            } else {
                // FIXME: check if need to delete exlude and if so
                // and exclude after add to exlude list else delete now
                // ? is delete for src, dst, or both
            }
        }
    }
}

void CxpsSync::copyFile(ClientAbc* client,
                        boost::filesystem::path const& fileToCopy,
                        boost::filesystem::path const& topSrcDir)
{
    ++m_totalFileCount;
    unsigned long long fileSize = boost::filesystem::file_size(fileToCopy);
    m_totalFileSizes += fileSize;
    if (m_lastRunTime < boost::filesystem::last_write_time(fileToCopy)
        || m_failedFilesLastRun.end() != m_failedFilesLastRun.find(fileToCopy.string())) {
        boost::filesystem::path finalDst(m_dst);
        if (!topSrcDir.empty()) {
            finalDst /= stripPrefix(topSrcDir, fileToCopy);
        } else {
            finalDst /= fileToCopy.filename();
        }
        try {
            if (m_verbose >= VERBOSE_MEDIUM) {
                std::cout <<  "copy " << fileToCopy.string() << " to " << finalDst.string();
                if (!m_cxpsServer.empty()) {
                    std::cout << " on server " << m_cxpsServer;
                }
                std::cout << '\n';
            }
            ++m_totalFileTransferCount;
            m_totalBytesSent += fileSize;
            m_literalSize += fileSize; // not using hash compare so non matched will always equal file size
            time_t startTime = time(0);
            client->putFile(finalDst.string(), fileToCopy.string(), m_compressMode, true);
            m_totalTransferTimeSeconds += time(0) - startTime;
        } catch (ErrorException const& e) {
            m_resultCode = RESULT_CODE_NETWORK_ERROR;
            m_failedFilesCurrentRun.insert(fileToCopy.string());
            m_errors += e.what();
            m_errors += '\n';
        }
    }
}

void CxpsSync::getFilesFromCxps(ClientAbc* client,
                                std::string const& files,
                                boost::filesystem::path const& prefix)
{
    // files will be a list of file/dir names with following format
    //
    //   name\tsize\nname\tsize\n
    //
    // size will not be present for dirs i.e. dir\t\n
    // files will always have size value
    //
    // first break up files by \n
    // then for each entry break up by \t
    // if size present and > 0 then have file to get otherwise skip transfer
    // but may still need to create empty dir and 0 size file
    boost::char_separator<char> newlineSep("\n");
    tokenizer_t tokens(files, newlineSep);
    tokenizer_t::iterator newlineIter(tokens.begin());
    tokenizer_t::iterator newlineIterEnd(tokens.end());
    for (/* empty*/; newlineIter != newlineIterEnd; ++newlineIter) {
        boost::char_separator<char> tabSep("\t");
        tokenizer_t fileTokens(*newlineIter, tabSep);
        tokenizer_t::iterator tabIter(fileTokens.begin());
        tokenizer_t::iterator tabIterEnd(fileTokens.end());
        boost::filesystem::path remoteFile(*tabIter);
        ++tabIter;
        if (tabIterEnd != tabIter) {
            if (m_excludeFileSpecRegex.empty() || !matchesExcludeFromRegex(remoteFile.filename().string())) {
                try {
                    boost::filesystem::path finalDst(m_dst);
                    finalDst /= stripPrefix(prefix, remoteFile);
                    CreatePaths::createPathsAsNeeded(finalDst);
                    if (boost::lexical_cast<unsigned long long>(*tabIter) > 0) {
                        // have file with size > 0 get it
                        try {
                            if (m_verbose >= VERBOSE_MEDIUM) {
                                std::cout <<  "copy " << remoteFile.string() << " to " << finalDst.string() << '\n';
                            }
                            // FIXME: need to check if actually modified
                            client->getFile(remoteFile.string(), finalDst.string());
                        } catch (ErrorException const& e) {
                            m_resultCode = RESULT_CODE_NETWORK_ERROR;
                            m_errors += e.what();
                            m_errors += "\n";
                        }
                    } else {
                        // MAYBE: do not create empty file?
                        std::ofstream outFile(finalDst.string().c_str());
                    }
                } catch (...) {
                    // ignore for now
                }
            } else {
                // FIXME: check if need to delete exclude and if so
                // and exclude after add to exlude list else delete now
                // ? is delete for src, dst, or both
            }
        } else {
            // FIXME: if exclude check if need to delete and if so
            // and exclude after add to exlude list else delete now
            // ? is delete for src, dst, or both.
            // If not exclude create empty dirs?
        }
    }
}

void CxpsSync::getFromCxps()
{
    std::string id("cxpssync-");
    id += boost::lexical_cast<std::string>(time(0));
    boost::shared_ptr<ClientAbc> client;
    if (m_cxpsCert.empty()) {
        client.reset(new HttpClient_t(m_cxpsServer,
                                      m_cxpsPort,
                                      id,
                                      m_maxBufferSizeBytes,
                                      m_connectTimeoutSeconds,
                                      m_timeoutSeconds,
                                      m_keepAlive,
                                      m_sendWindowSizeBytes,
                                      m_receiveWindowSizeBytes,
                                      m_writeMode,
                                      securitylib::getPassphrase(),
                                      HEARTBEAT_INTERVAL_SECONDS,
                                      true));
    } else {
        client.reset(new HttpsClient_t(m_cxpsServer,
                                       m_cxpsPort,
                                       id,
                                       m_maxBufferSizeBytes,
                                       m_connectTimeoutSeconds,
                                       m_timeoutSeconds,
                                       m_keepAlive,
                                       m_sendWindowSizeBytes,
                                       m_receiveWindowSizeBytes,
                                       std::string(),
                                       m_writeMode,
                                       securitylib::getPassphrase(),
                                       HEARTBEAT_INTERVAL_SECONDS,
                                       true));
    }
    srcDirs_t::iterator iterEnd(m_src.end());
    for (srcDirs_t::iterator iter = m_src.begin(); iter != iterEnd; ++iter) {
        std::string files;
        std::string fileSpec(*iter);
        fileSpec += "/*";
        if (CLIENT_RESULT_NOT_FOUND == client->listFile(fileSpec, files)) {
            m_resultCode = RESULT_CODE_FILES_DIRS;
            m_errors = "no files found for ";
            m_errors += fileSpec;
            m_errors += "\n";
            return;
        }
        // NOTE: just use raw client ponter shared pinter is just used to make sure it gets deleted
        getFilesFromCxps(client.get(), files, *iter);
    }
}

void CxpsSync::putToCxps()
{
    std::string id("cxpssync-");
    id += boost::lexical_cast<std::string>(time(0));
    boost::shared_ptr<ClientAbc> client;
    if (m_cxpsCert.empty()) {
        client.reset(new HttpClient_t(m_cxpsServer,
                                      m_cxpsPort,
                                      id,
                                      m_maxBufferSizeBytes,
                                      m_connectTimeoutSeconds,
                                      m_timeoutSeconds,
                                      m_keepAlive,
                                      m_sendWindowSizeBytes,
                                      m_receiveWindowSizeBytes,
                                      m_writeMode,
                                      securitylib::getPassphrase(),
                                      HEARTBEAT_INTERVAL_SECONDS,
                                      true));
    } else {
        client.reset(new HttpsClient_t(m_cxpsServer,
                                       m_cxpsPort,
                                       id,
                                       m_maxBufferSizeBytes,
                                       m_connectTimeoutSeconds,
                                       m_timeoutSeconds,
                                       m_keepAlive,
                                       m_sendWindowSizeBytes,
                                       m_receiveWindowSizeBytes,
                                       std::string(),
                                       m_writeMode,
                                       securitylib::getPassphrase(),
                                       HEARTBEAT_INTERVAL_SECONDS,
                                       true));
    }
    // NOTE: just use raw client ponter shared pinter is just used to make sure it gets deleted
    copySrcsToDst(client.get());
}

void CxpsSync::localCopy()
{
    FileClient_t client(WRITE_MODE_NORMAL);
    copySrcsToDst(&client);
}

void CxpsSync::processRequest()
{
    if (COPY_MODE_GET == m_copyMode) {
        getFromCxps();
    } else if (COPY_MODE_PUT == m_copyMode) {
        putToCxps();
    } else {
        localCopy();
    }
}

void CxpsSync::usage()
{
    std::cout << "usage: inmsync [options] [cxpserver::]src [cxpsserver::]dst\n"
              << " "
              << "  [options]: all are optional\n"
              << "    --port=<port>        : <port> is the cxps port.\n"
              << "    --exclude-from=<file>: <file> file name contains exclude directives\n"
              << "    --user=<user>        : <user> is cxps user\n"
              << "    --password=<passwrd> : <password> is cxps password\n"
              << "    --verbose=<level>    : set verbose level (1, 2, 3)\n"
              << "\n"
              << "  [destserver::] : destination server name or ip address. Note can only specify for src or dst, not both\n"
              << "  src            : one or more file/dirs to be copied. Note for multiple sources must place in single quotes. 'src1 src2'\n"
              << "                   if source file/dir has spaces, must escape them e.g. 'src\\ /with\\ /spaces' \n"
              << "  dst            : dir to place the files/dirs being copied\n"
              << "\n";
}

bool CxpsSync::parseArgs(
    int argc,
    char* argv[])
{
    // at minimum need 3 args
    // inmsync src dst
    if (argc < 3) {
        m_errors += "missing args: inmsync [options] src dst\n";
        m_resultCode = RESULT_CODE_USAGE;
        return false;
    }

    int curArgIdx = 1;
    while (curArgIdx < argc) {
        if ('-' != argv[curArgIdx][0]) {
            // at this point will have only src dst left
            parseSrc(argv[curArgIdx]);
            ++curArgIdx;
            if (curArgIdx < argc) {
                parseDst(argv[curArgIdx]);
            } else {
                m_errors += "missing destination\n";
            }
            break;
        } else if ('\0' == argv[curArgIdx][1]) {
            // dash by itself
            m_errors += "invalid option: ";
            m_errors += argv[curArgIdx];
            m_errors += "\n";
        } else if ('-' == argv[curArgIdx][1]) {
            char* equalSignPos = strchr(argv[curArgIdx], '=');
            if (0 != equalSignPos) {
                *equalSignPos = '\0';
            }
            longOptionTags_t::iterator optIterEnd(m_longOptionTags.end());
            for (longOptionTags_t::iterator optIter = m_longOptionTags.begin(); optIter != optIterEnd; ++optIter) {
                if (boost::algorithm::iequals(&argv[curArgIdx][2], *optIter)) {
                    if (0 == equalSignPos) {
                        m_options.insert(std::make_pair(*optIter, std::string()));
                    } else {
                        m_options.insert(std::make_pair(*optIter, ++equalSignPos));
                    }
                    break;
                }
            }
        } else {
            // short option can have multiple options per - e.g. -av is same as -a -v
            for (int argIdx = 1; 0 != argv[curArgIdx][argIdx]; ++argIdx) {
                shortOptionTags_t::iterator optIterEnd(m_shortOptionTags.end());
                for (shortOptionTags_t::iterator optIter = m_shortOptionTags.begin(); optIter != optIterEnd; ++optIter) {
                    if (argv[curArgIdx][argIdx] == *optIter) {
                        // we have to special case verbose as you provide more then 1 to increase verosity
                        if (OPT_SHORT_VERBOSE == argv[curArgIdx][argIdx]) {
                            ++m_verbose;
                        } else {
                            m_options.insert(std::make_pair(boost::lexical_cast<std::string>(*optIter), std::string()));
                        }
                        break;
                    }
                }
            }
        }
        ++curArgIdx;
    }
    if (m_src.empty()) {
        m_errors += "missing source path\n";
    }
    if (m_dst.empty()) {
        m_errors += "missing destination path\n";
    }

    options_t::iterator findIter = m_options.find(OPT_LONG_JOBID);
    boost::filesystem::path trackingName = argv[0];
    trackingName.remove_filename();
    trackingName /= "frtracking";
    try {
        boost::filesystem::create_directory(trackingName);
    } catch (std::exception const& e) {
        m_errors += "failed to create tracking directory ";
        m_errors += trackingName.string();
        m_errors += ": ";
        m_errors += e.what();
    }
    if (m_options.end() == findIter) {
        m_errors += "missing job id\n";
    } else {
        std::string name("job_");
        name += (*findIter).second;
        trackingName /= name;
    }
    m_trackingFile = trackingName.string();

    if (m_errors.empty()) {
        options_t::iterator findIter = m_options.find(OPT_LONG_EXCLUDE_FROM);
        if (m_options.end() != findIter) {
            buildExcludeFromRegex((*findIter).second.c_str());
        }
        setCxpsInfo();
    }
    if (!m_errors.empty()) {
        m_resultCode = RESULT_CODE_USAGE;
    }
    return m_errors.empty();
}

bool CxpsSync::stripCxpsServerIfExists(std::string& str)
{
    std::string::size_type idx = str.find("::");
    if (std::string::npos != idx) {
        m_cxpsServer = str.substr(0, idx);
        str = str.substr(idx + 2);
        return true;
    }
    return false;
}

boost::filesystem::path CxpsSync::stripPrefix(boost::filesystem::path const& prefix,
                                              boost::filesystem::path const& name)
{
    // while this comment is in the header put it here too since this is the actual implementation
    //
    // prefix can be
    //   * full path: /full/path/prefix or c:\full\path\prefix
    //   in this case strip off prefix starting from begining of name up but not the last path in prefix
    //   e.g.
    //      prefix: /dir1/dir2
    //      name:   /dir1/dir2/file1
    //
    //      final name: dir2/file1
    //
    //   if the prefix ends in a slash (/) then want to strip off the last path as well
    //   e.g.
    //      prefix: /dir1/dir2/
    //      name:   /dir1/dir2/file1
    //
    //      final name: file1
    //
    //   * relative path: relative/path/prefix
    //   this can happen when geting files from cxps as it will return full paths
    //   for all file/dir names, but when the get from cxps was made it may have used
    //   a relative path with the assumption the files are under the a specific dir
    //   that cxps knows about.
    //
    //   So in this case need to skip over the inital parts
    //   of the full path returned by cxps until you find the first part that matcehs
    //   the first part of the prefix and then continue to strip from ther
    //   e.g.
    //
    //     inmsync --port=9080 cxpsserver::src/data /tmp/dst
    //
    //   cxps would return file/dir names something lke this
    //
    //     /home/svsystems/transport/src/data
    //
    //   so when stripPrefix is called
    //
    //     name: /home/svsystems/transport/src/data/file.txt,
    //     prefix: src/data
    //
    //   want to strip /home/svsystems/transport/src/ so
    //   final name is data/file.txt
    //
    //  if prefix ends in a slash '/' then want to strip the entire path
    //
    //     name: /home/svsystems/transport/src/data/file.txt,
    //     prefix: src/data
    //
    //     final name: file.txt
    //
    boost::filesystem::path finalName;
    if (!prefix.empty()) {
        boost::filesystem::path::iterator nameIter = name.begin();
        boost::filesystem::path::iterator nameIterEnd = name.end();
        boost::filesystem::path::iterator prefixIter = prefix.begin();
        boost::filesystem::path::iterator prefixIterEnd = prefix.end();
        if (!prefix.has_root_name() && !prefix.has_root_directory()) {
            // skip name until you find the first match with the prefix
            for(/* empty */; nameIter != nameIterEnd; ++nameIter) {
                if (*nameIter == *prefixIter) {
                    break;
                }
            }
        }

        for(/* empty */; nameIter != nameIterEnd && prefixIter != prefixIterEnd; ++nameIter, ++prefixIter) {
            if (*nameIter != *prefixIter) {
                break;
            }
        }

        std::string prefixStr(prefix.string());
        if (!('/' == prefixStr[prefixStr.size() - 1] || '\\' == prefixStr[prefixStr.size() - 1])) {
            --nameIter; // want to include the very last path of the prefix
        }
        for(/* empty */; nameIter != nameIterEnd; ++nameIter) {
            finalName /= *nameIter;
        }
    }
    return finalName;
}

void CxpsSync::trackRun()
{
    if (!m_trackingFile.empty()) {
        std::ofstream trackFile(m_trackingFile.c_str());
        trackFile << m_currentRunTime << '\n';
        failedFiles_t::iterator iterEnd(m_failedFilesCurrentRun.end());
        for (failedFiles_t::iterator iter = m_failedFilesCurrentRun.end(); iter != iterEnd; ++iter) {
            trackFile << *iter << '\n';
        }
    }
}

void CxpsSync::loadLastRun()
{
    if (!m_trackingFile.empty()) {
        std::ifstream trackFile(m_trackingFile.c_str());
        // first line has time_t value of last run time
        trackFile >> m_lastRunTime;

        std::string name;
        // remaing lines have fullnames of files that fialed resync last run so that they can be retried
        while (trackFile.good()) {
            trackFile >> name;
            if (!name.empty()) {
                m_failedFilesLastRun.insert(name);
            }
        }
    }
}

void CxpsSync::printStats()
{
    std::cout << "Total bytes sent: " << m_totalBytesSent << '\n'
              << "Literal data: " << m_literalSize << '\n'
              << "Total file size: " << m_totalFileSizes << '\n';

    if (m_verbose >= VERBOSE_HIGH) {
        std::cout << "Number of files: " << m_totalFileCount << '\n'
                  << "Number of files transferred: " << m_totalFileTransferCount << '\n'
                  << "sent " << m_totalBytesSent << " bytes  received 0 bytes  "
                  << std::fixed << std::setprecision(2) << (m_totalTransferTimeSeconds > 0 ? m_totalBytesSent / (long double)m_totalTransferTimeSeconds : 0) << " bytes/sec\n";
    }
}

bool CxpsSync::pathsMatch(std::string const& path1, std::string const& path2)
{
    std::string::size_type path1Size = path1.size();
    std::string::size_type path2Size = path2.size();
    int lenDiff = path1Size - path2Size;
    if (lenDiff < -1 || lenDiff > 1) {
        return false;
    }
    std::string::size_type path1Idx = 0; // need this outside loop
    std::string::size_type path2Idx = 0; // need this outside loop
    for (/* empty */; path1Idx < path1Size && path2Idx < path2Size && path1[path1Idx] == path2[path2Idx]; ++path1Idx, ++path2Idx); // no body all work done in loop
    return ((path1Idx == path1Size && path2Idx == path2Size)                                      // exactly the same
            || (path1Idx == path1Size && ('/' == path2[path2Idx] || '\\' == path2[path2Idx]))     // path2 has trailing slash (backslash) - treat as match
            || (path2Idx == path2Size && ('/' == path1[path1Idx] || '\\' == path1[path1Idx])));   // path1 has trailing slash (backslash) - treat as match
}
