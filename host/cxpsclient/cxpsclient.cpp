
///
/// \file cxpsclient.cpp
///
///  \brief test client for cx process server and client code
///

#include <iostream>
#include <limits>
#include <fstream>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/random/linear_congruential.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include <ace/Init_ACE.h>
#include <ace/ACE.h>

#include "volume.h"
#include "client.h"
#include "cxtransport.h"
#include "transportstream.h"
#include "genericstream.h"
#include "transport_settings.h"
#include "transportprotocols.h"
#include "waitforquit.h"
#include "HandlerCurl.h"
#include "wallclocktimer.h"
#include "compressmode.h"
#include "genrandnonce.h"
#include "writemode.h"
#include "configurator2.h"
#include "programfullpath.h"
#include "initialsettings.h"
#include "localconfigurator.h"
#include "logger.h"
#include "inmsafecapis.h"
#include "genpassphrase.h"
#include "cfslocalname.h"
#include "terminateonheapcorruption.h"
#include "securityutils.h"
#include "azureblobclient.h"

// needed by cdpapplylib
Configurator* TheConfigurator;

typedef std::vector<boost::shared_ptr<boost::thread> > threads_t; ///< holding threads

typedef std::vector<char> chBuffer_t; ///< used for put/get buffers

chBuffer_t g_hostName(512);

COMPRESS_MODE g_compressMode = COMPRESS_NONE;

struct Protocols {
    char* name;
    TRANSPORT_PROTOCOL protocol;
};

Protocols g_supportedProtocols[] =
{
    { "http", TRANSPORT_PROTOCOL_HTTP },
    { "file", TRANSPORT_PROTOCOL_FILE },
    { "cfs", TRANSPORT_PROTOCOL_CFS },
    { "pageblob", TRANSPORT_PROTOCOL_BLOB},

    // must always be last
    { 0, TRANSPORT_PROTOCOL_UNKNOWN }
};

std::string g_ip;
std::string g_port;
std::string g_sslPort;
std::string g_password;
std::string g_clientSslFile;
std::string g_localName;
std::string g_remoteDir;
std::string g_outFileName;
std::string g_hostId;
std::string g_finalPaths;
std::string g_putFile;
std::string g_getFile;
std::string g_getToFile;
std::string g_proto;
std::string g_cfsName(CFS_LOCAL_NAME_DEFAULT);
std::string g_psId;
std::string g_containerSasUri;

std::string g_clientCertFile;
std::string g_clientKeyFile;
std::string g_serverThumbprint;

std::string const g_extendedPathName("abcdefghijklmnoabcdefghijklmnoabcdefghijklmnoabcdefghijklmnoabcdefghijklmnoabcdefghijklmnoabcdefghijklmnoabcdefghijklmnoabcdefghijklmnoabcdefghijklmnoabcdefghijklmnoabcdefghijklmnoabcdefghijklmnoabcdefghijklmnoabcdefghijklmnoabcdefghijklmnoabc1");

bool g_useS2 = false;
bool g_useDp = false;
bool g_zCompress = false;
bool g_useExtendedPathName = false;
bool g_injectErrors = false;
bool g_useZeroLengthPutfileEof = false;
bool g_validateData = false;
bool g_waitOnError = false;
bool g_logout = false;
bool g_prompt = false;
bool g_quit = false;
bool g_useBaseName = false;
bool g_createDirs = false;
bool g_csCreds = false;
bool g_secure = false;
bool g_generalXfer = false;
bool g_generateChecksum = false;
bool g_useCertAuth = false;

int g_writeMode = WRITE_MODE_NORMAL;
int g_finalPathsCount = 0;
TRANSPORT_PROTOCOL g_protocol = TRANSPORT_PROTOCOL_UNKNOWN;

std::size_t g_maxBytesToSend = 1048576;
std::size_t g_minBytesToSend = 1048576;
std::size_t g_bufferSize= 1048576;       ///< maximum buffer size to use for put/get data

unsigned long long g_numberOfRuns = 1ull;
unsigned long long g_logoutEveryNRuns = 1ull;

int g_verboseLevel = 0;
int g_receiveWindow = 0;
int g_sendWindow = 0;
int g_timeoutSeconds = 300;
int g_threadCnt = 1;
int g_putRepeat = 1;
int g_getRepeat = 1;
int g_delay = -1;
int g_listFileTst = 0;

int const VERBOSE_LEVEL_ERROR = -1;
int const VERBOSE_LEVEL_0 = 0;
int const VERBOSE_LEVEL_1 = 1;
int const VERBOSE_LEVEL_2 = 2;
int const VERBOSE_LEVEL_3 = 3;
int const FENCE_SIZE = 10;

char const FENCE_CHAR = 'X';

// random generators
typedef boost::minstd_rand base_generator_type; ///< boost random number generator to be used
base_generator_type g_generator(42u);
boost::uniform_int<> g_asciiPrintableUniDist(32, 126); // simple prinatable ASCII chars
boost::variate_generator<base_generator_type&, boost::uniform_int<> > g_asciiPrintableUni(g_generator, g_asciiPrintableUniDist);

base_generator_type g_generator2(42u);
boost::uniform_int<> g_delayUniDist(0, 6); // simple prinatable ASCII chars
boost::variate_generator<base_generator_type&, boost::uniform_int<> > g_delayUni(g_generator2, g_delayUniDist);

boost::mutex g_mutexLock; ///< for intra-process serialization access to the log

#define MY_SIMPLE_TRACE(vLevel, myX)                                    \
    do {                                                                \
        if (vLevel <= g_verboseLevel) {                                 \
            boost::mutex::scoped_lock guard(g_mutexLock);               \
            std::cout << boost::posix_time::second_clock::local_time()  \
                      << " threadId: " << boost::this_thread::get_id()  \
                      << ' ' << myX << std::endl;                       \
        }                                                               \
        if (VERBOSE_LEVEL_ERROR == vLevel) {                            \
            waitOnError();                                              \
        }                                                               \
    } while(false)


#define FREE_BUFFER(theBufferToFree)            \
    do {                                        \
        if (0 != theBufferToFree) {             \
            free(getBuffer);                    \
            getBuffer = 0;                      \
        }                                       \
    } while(false)

void waitOnError()
{
    if (g_waitOnError) {
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_0, "\nwaiting as requested. press enter to continue: ");
        char ch;
        std::cin.read(&ch, sizeof(ch));
    }
}

void delayIfRequested(unsigned long long& delaySeconds)
{
    if (-1 == g_delay) {
        return;
    }
    if (0 == g_delayUni()) {
        delaySeconds += g_delay;
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "+++++ Delaying for " << g_delay << " seconds");
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000 * g_delay));
    } else {
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "!!!!! Skipping delay this time");
    }
}

std::size_t bytesToSend(std::size_t min, std::size_t max)
{
    if (min == max) {
        return max;
    }
    boost::uniform_int<> uniDist(min, max);
    boost::variate_generator<base_generator_type&, boost::uniform_int<> > uni(g_generator, uniDist);
    return uni();
}

/// \brief display cxpsclient usage
void usage()
{
    std::cout << "usage: cxpsclient <options>\n"
              << "\n"
              << "  cxpsclient can be used for testing or general file transfer with cxps (CX process server).\n"
              << "\n"
              << "  conditional required options\n"
              << "    -i ipaddress: required for HTTP proto.\n"
              << "    -p port     : non-ssl port to use when connecting. Typically cxps listens on 9080.\n"
              << "                  Required for HTTP protool cif not using --cscreds nor -l\n"
              << "    -l sslPort  : ssl port. Typcially cxps listens on 9443\n"
              << "                  general file transfer: use either non-ssl (-p) or ssl (-l and -c) options, but not both\n"
              << "                                         if both are used, non-ssl will be used for file transfer\n"
              << "                  testing              : ssl and non-ssl can both be specifed at the same time\n"
              << "                                         if both specifed ssl test will run after non-ssl test\n"
              << "                                         if -p and -n 0 are specified, then ssl test will not be run\n"
              << "                 required if using HTTP protocal and not specifying --creds nor -p\n"
              << "      --secure  : use secure mode. Use with --proto cfs to use secure connection\n"
              << "\n\n"
              << "    these can be used with testing or general transfer\n"
              << "      -v 1 | 2 | 3            : enable verbose. 1 = minimal ouput, 2 = minimal plus putfile and getfile times, 3 = full output\n"
              << "      -C                      : create dirs if needed\n"
              << "  additional options\n"
              << "    -h                      : print usage\n"
              << "\n"
              << "    general transfer only options\n"
              << "      cxpsclient -i ipaddress -p port --put file -d remoteDir\n"
              << "      cxpsclient -i ipaddress -p port --get file [-L localFile] [-d remoteDir]\n"
              << "\n"
              << "      --put file      : put file to server. Note requires -d option\n"
              << "                        file is the local file to send and the filename portion of local file will be appended to the remoter dir\n"
              << "                        specified by the -d option\n"
              << "      --get file      : get file from server.\n"
              << "                        if -d omitted, then remoteFile should include full path (or cxps will assume its default request dir)\n"
              << "                        if -d used, then file should be filename with out path\n"
              << "                        if -L omitted, then the filename portion of file will be used as the local file name\n"
              << "                        and it will be written to current directory\n"
              << "      -d remoteDir    : remote directory to put/get file to/from\n"
              << "      -L file         : local file name to receive get file. Optional Defaults to the <current dir>/<filename portion>\n"
              << "\n\n"
              << "    testing only options\n"
              << "      when testing the following requests are always done: \n"
              << "          putfile, listfile, renamefile, listfile, getfile, deletefile\n\n"
              << "      the exepction is when using -m s2 as that will only do\n"
              << "          putfile, renamefile, deletefile\n\n"
              << "\n"
              << "      -c client pem file      : client pem file. Optional for secure HTTP\n. Defaults to well known location"
              << "      -m <s2 | dp>            : use either s2 or data protection (dp) mode. Requires svagents to be installed\n"
              << "      -F                      : use file protocal\n"
              << "      --proto <http|file|cfs|pageblob> : protocol to use http, file, cfs or pageblob; Default http\n"
              << "      --ps <id>               : process erver id to connect to. required when proto is cfs\n"
              << "      --cfsname <name>        : cfs local name to use. Optional with --proto cfs.\n"
              << "                                it must be the same name as cfs_local_name set in cfs cxps.conf.\n"
              << "                                on windows always set to 127.0.0.1 on non-windows defaults to /usr/local/InMage/transport/cfs.ud\n"
              << "      --containersasuri <uri> : requried with protocol pageblob.\n"
              << "      -b bytes[:bytes]        : number of bytes to send. Default 1048576. Ignored if -f present\n"
              << "                                when -b bytes:bytes it specfies a range of bytes to send.\n"
              << "                                each put request will send a random number of bytes between that range\n"
              << "      -f send file            : test file name to send and then get back\n"
              << "      -d remote dir           : remote directory to be used\n"
              << "      -r receive window size  : receive window size in bytes. Default system setting\n"
              << "      -s send window size     : send window size in bytes. Default system setting\n"
              << "      -n number of runs       : number times to repeat the test (per thread). 0 is infinite until stopped. Default 1 \n"
              << "      -k thread count         : number of threads to use. Default 1 \n"
              << "      -q                      : prompt to continue after each request to cx\n"
              << "      -o [every n runs]       : logout between runs. Optional logout after every n runs\n"
              << "      -z                      : test enable_upload_diff_compression (cxps.conf used by cxps must have thist option set to yes)\n"
              << "      -Z                      : test inline compression using compression option COMPRESS_CXSIDE (default COMPRESS_NONE)\n"
              << "      -x                      : use extended-path name. Ignored if -f option used\n"
              << "      -y timeout seconds      : timeout interval in seconds. default 300 (5 minutes)\n"
              << "                                ingnored if -m option used. Update drscout.conf TransportResponseTimeoutSeconds\n"
              << "                                and TransportConnectTimeoutSeconds to change timeout durations\n"
              << "      -0                      : use a 0 length with more data = false putfile to indicate the end of file\n"
              << "      -I bytes                : internal buffer size to use for put/get. Default 1048576\n"
              << "      -V                      : validate getfile data against send data. if -W specified will also wait if validation fails\n"
              << "                                (currently does not validate when compression enabled)\n"
              << "      -P putfile repeat count : number of times to repeat the putfile per test run. Default 1\n"
              << "                                when combined with -b can simulate sending a large volume using multiple files\n"
              << "      -G getfile repeat count : number of times to repeat the getfile per test run. Default 1\n"
              << "                                when combined with -b can simulate getting a large volume using multiple files\n"
              << "      -M [count]              : use finalPaths on rename to tell cxps to do final rename.\n"
              << "                                count is optional and is the number of finalPaths to use (default 1).\n"
              << "      -B                      : use only the base file name on rename when using finalPaths (only valid with -M)\n"
              << "      -W                      : wait on errors to allow investigation before proceeding\n"
              << "      -D [seconds]            : randomly delay seconds between calls. If seconds is larger then timeout or enough delays when sending larger amount of data\n"
              << "                                can result in get/put file requets timeing out on the server side\n"
              << "                                Note ignored when doing file to file put/get or if using -t option. Default 305 seconds\n"
              << "      -O                      : write mode to be used 0; use normal mode, 1 use flush mode. default 0\n"
              << "      -H                      : generate checksums on getfile when local file is specified \n"
              << "      --certauth              : use client certificate based authentication (instead of passphrase based auth).\n"
              << "                                Requires options --clientcertfile, --clientkeyfile and --serverthumbprint.\n"
              << "      --clientcertfile <path> : client certificate file to use \n"
              << "      --clientkeyfile <path>  : client private key file to use \n"
              << "      --serverthumbprint <value> : server cert thumbprint to use for server validation \n"
              << "\n"
              << "Example of most common uses\n"
              << "\n"
              << "  HTTP non secure\n"
              << "    cxpsclient -i <ip> -p <non ssl port> -v 2\n"
              << "\n"
              << "  HTTP secure\n"
              << "    cxpsclient -i <ip> -l <ssl port> -v 2\n"
              << "\n"
              << "  CFS non secure (must run on system with cxps running in cfs mode and ps created cfslogin)\n"
              << "    cxpsclient --proto cfs --ps <ps id> -v 2\n"
              << "\n"
              << "  CFS secure (must run on system with cxps running in cfs mode and ps created cfslogin)\n"
              << "    cxpsclient --proto cfs --ps <ps id> --secure -v 2\n"
              << "\n"
              << "\n"
              << std::endl;
}

bool isCompressableDiffFile(boost::filesystem::path const& name)
{
    return ((std::string::npos != name.string().find("diff_") || std::string::npos != name.string().find("sync_"))
            && name.extension() == ".dat"
            && std::string::npos == name.string().find("missing"));
}

bool compressFile(boost::filesystem::path const& name)
{
    return (g_zCompress && isCompressableDiffFile(name));
}

/// \brief parse command line options
bool parseCmd(
    int argc,                     ///< number of command line arguments
    char* argv[])                 ///< command line arguments
{
    bool argsOk = true;
    bool printHelp = false;
    bool yOptionSet = false;

    if (1 == argc) {
        usage();
        return false;
    }

    int i = 1;
    while (i < argc) {
        if ('-' != argv[i][0]) {
            std::cout << "*** invalid option: " << argv[i] << '\n';
            argsOk = false;
        } else if ('\0' == argv[i][1]) {
            std::cout << "*** invalid option: " << argv[i] << '\n';
            argsOk = false;
        } else if ('-' == argv[i][1]) {
            // long option - these are general transfer
            if (0 == strcmp("--get", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --get\n";
                    argsOk = false;
                } else {
                    g_generalXfer = true;
                    g_getFile = argv[i];
                }
            } else if (0 == strcmp("--put", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --put\n";
                    argsOk = false;
                } else {
                    g_generalXfer = true;
                    g_putFile = argv[i];
                }
            } else if (0 == strcmp("--cscreds", argv[i])) {
                g_csCreds = true;
            } else if (0 == strcmp("--secure", argv[i])) {
                g_secure = true;
            } else if (0 == strcmp("--proto", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --proto\n";
                    argsOk = false;
                } else {
                    g_proto = argv[i];
                }
            } else if (0 == strcmp("--ps", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --psid\n";
                    argsOk = false;
                } else {
                    g_psId = argv[i];
                }
            } else if (0 == strcmp("--cfsname", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --cfsname\n";
                    argsOk = false;
                } else {
                    g_cfsName = argv[i];
                }
            }
            else if (0 == strcmp("--listfiletst", argv[i])) {
                ++i;
                if (i < argc && '-' != argv[i][0]) {
                    try {
                        g_listFileTst = boost::lexical_cast<int>(argv[i]);
                    }
                    catch (...) {
                        g_listFileTst = 30;
                    }
                }
                else {
                    --i;
                    g_listFileTst = 30;
                }
            }
            else if (0 == strcmp("--containersasuri", argv[i])) {
                i++;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --containersasuri\n";
                    argsOk = false;
                }
                else {
                    g_containerSasUri = argv[i];
                }
            }
            else if (0 == strcmp("--certauth", argv[i])) {
                g_useCertAuth = true;
            }
            else if (0 == strcmp("--clientcertfile", argv[i])) {
                i++;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --clientcertfile\n";
                    argsOk = false;
                }
                else {
                    g_clientCertFile = argv[i];
                }
            }
            else if (0 == strcmp("--clientkeyfile", argv[i])) {
                i++;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --clientkeyfile\n";
                    argsOk = false;
                }
                else {
                    g_clientKeyFile = argv[i];
                }
            }
            else if (0 == strcmp("--serverthumbprint", argv[i])) {
                i++;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for --serverthumbprint\n";
                    argsOk = false;
                }
                else {
                    g_serverThumbprint = argv[i];
                }
            } else {
                std::cout << "*** invalid option: " << argv[i] << '\n';
                argsOk = false;
            }
        } else {
            if ('\0' != argv[i][2]) {
                std::cout << "*** invalid option: " << argv[i] << '\n';
                argsOk = false;
            } else {
                // short option
                switch (argv[i][1]) {
                    case '0':
                        g_useZeroLengthPutfileEof = true;
                        break;

                    case 'b':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -b\n";
                            argsOk = false;
                            break;
                        }
                        {
                            std::string bytes(argv[i]);
                            std::string::size_type idx = bytes.find_first_of(":");
                            if (std::string::npos != idx) {
                                g_minBytesToSend = boost::lexical_cast<std::size_t>(bytes.substr(0, idx));
                                g_maxBytesToSend = boost::lexical_cast<std::size_t>(bytes.substr(idx + 1));
                                if (g_minBytesToSend > g_maxBytesToSend) {
                                    std::size_t bytes = g_maxBytesToSend;
                                    g_maxBytesToSend = g_minBytesToSend;
                                    g_minBytesToSend = bytes;
                                }
                            } else {
                                g_maxBytesToSend = boost::lexical_cast<std::size_t>(bytes.c_str());
                                g_minBytesToSend = g_maxBytesToSend;
                            }
                        }

                        break;

                    case 'c':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -c\n";
                            argsOk = false;
                            break;
                        }

                        //g_clientSslFile = argv[i];
                        break;

                    case 'd':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -d\n";
                            argsOk = false;
                            break;
                        }

                        g_remoteDir = argv[i];
                        break;

                    case 'f':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -f\n";
                            argsOk = false;
                            break;
                        }

                        g_localName = argv[i];
                        if (!boost::filesystem::exists(g_localName)) {
                            std::cout << "*** local file '" << g_localName << "' does not exist\n";
                            argsOk = false;
                        }
                        break;

                    case 'h':
                        printHelp = true;
                        break;

                    case 'i':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -i\n";
                            argsOk = false;
                            break;
                        }

                        g_ip = argv[i];
                        break;

                    case 'k':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -n\n";
                            argsOk = false;
                            break;
                        }

                        g_threadCnt = boost::lexical_cast<int>(argv[i]);
                        if (g_threadCnt < 1) {
                            std::cout << "*** thread count must be > 0, defaulting to 1\n";
                            g_threadCnt = 1;
                        }
                        break;

                    case 'l':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -l\n";
                            argsOk = false;
                            break;
                        }

                        g_sslPort = argv[i];
                        boost::lexical_cast<unsigned short>(g_sslPort);
                        break;

                    case 'm':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -m\n";
                            argsOk = false;
                            break;
                        }

                        if (0 == strcmp("s2", argv[i])) {
                            g_useS2 = true;
                        } else if (0 == strcmp("dp", argv[i])) {
                            g_useDp = true;
                        } else {
                            argsOk = false;
                            std::cout << "*** invalid value '" << argv[i] << "' for option -m. tyr s2 or dp\n";
                        }
                        break;

                    case 'n':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -n\n";
                            argsOk = false;
                            break;
                        }

                        {
                            int runs  = boost::lexical_cast<int>(argv[i]);
                            if (runs > 0) {
                                g_numberOfRuns = runs;
                            } else if (0 == runs) {
                                // need to wrap the numeric_limits::max in parentheses to prevent conflict with max macro
                                g_numberOfRuns = (std::numeric_limits<unsigned long long>::max)();
                            }
                        }

                        break;

                    case 'o':
                        g_logout = true;
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            // logout every run
                            --i;
                            break;
                        }

                        {
                            int runs  = boost::lexical_cast<int>(argv[i]);
                            if (runs > 0) {
                                g_logoutEveryNRuns = runs;
                            }
                        }

                        break;

                    case 'p':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -p\n";
                            argsOk = false;
                            break;
                        }

                        g_port = argv[i];
                        boost::lexical_cast<unsigned short>(g_port);
                        break;

                    case 'q':
                        g_prompt = true;
                        break;

                    case 'r':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -r\n";
                            argsOk = false;
                            break;
                        }

                        g_receiveWindow = boost::lexical_cast<int>(argv[i]);
                        break;

                    case 's':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -s\n";
                            argsOk = false;
                            break;
                        }

                        g_sendWindow = boost::lexical_cast<int>(argv[i]);
                        break;

                    case 'u':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -u\n";
                            argsOk = false;
                            break;
                        }

                        break;

                    case 'v':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -v\n";
                            argsOk = false;
                            break;
                        }

                        g_verboseLevel = boost::lexical_cast<int>(argv[i]);
                        if (g_verboseLevel < 0) {
                            g_verboseLevel = 0;
                        }
                        break;

                    case 'w':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -w\n";
                            argsOk = false;
                            break;
                        }

                        //g_password = argv[i];
                        break;

                    case 'x':
                        g_useExtendedPathName = true;
                        break;

                    case 'y':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -y\n";
                            argsOk = false;
                            break;
                        }
                        yOptionSet = true;
                        g_timeoutSeconds = boost::lexical_cast<int>(argv[i]);
                        break;

                    case 'z':
                        g_zCompress= true;
                        break;

                    case 'B':
                        g_useBaseName = true;
                        break;

                    case 'C':
                        g_createDirs = true;
                        break;

                    case 'D':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            g_delay = 305;
                            break;
                        }
                        g_delay = boost::lexical_cast<int>(argv[i]);
                        break;

                    case 'G':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -G\n";
                            argsOk = false;
                            break;
                        }

                        g_getRepeat = boost::lexical_cast<int>(argv[i]);
                        break;

                    case 'H':
                        g_generateChecksum = true;
                        break;

                    case 'I':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -I\n";
                            argsOk = false;
                            break;
                        }

                        g_bufferSize = boost::lexical_cast<int>(argv[i]);
                        break;

                    case 'L':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -L\n";
                            argsOk = false;
                            break;
                        }

                        g_getToFile = argv[i];
                        break;

                    case 'M':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            g_finalPathsCount = 1;
                            break;
                        }

                        g_finalPathsCount = boost::lexical_cast<int>(argv[i]);
                        break;

                    case 'O':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -O\n";
                            argsOk = false;
                            break;
                        }
                        try {
                            g_writeMode = boost::lexical_cast<int>(argv[i]);
                        } catch(...) {
                            g_writeMode = WRITE_MODE_NORMAL;
                        }
                        if (g_writeMode < WRITE_MODE_NORMAL || g_writeMode > WRITE_MODE_FLUSH) {
                            g_writeMode = WRITE_MODE_NORMAL;
                        }
                        break;

                    case 'P':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "*** missing value for -P\n";
                            argsOk = false;
                            break;
                        }

                        g_putRepeat = boost::lexical_cast<int>(argv[i]);
                        break;

                    case 'V':
                        g_validateData = true;
                        break;

                    case 'W':
                        g_waitOnError = true;
                        break;

                    case 'Z':
                        g_compressMode = COMPRESS_CXSIDE;
                        break;

                    default:
                        std::cout << "*** invalid arg: " << argv[i] << "\n";
                        argsOk = false;
                        break;
                }
            }
        }
        ++i;
    }

    if (printHelp) {
        usage();
        return false;
    }

    if (!g_proto.empty()) {
        int i = 0;
        for (/* empty */; 0 != g_supportedProtocols[i].name; ++i) {
            if (boost::algorithm::iequals(g_proto, g_supportedProtocols[i].name)) {
                g_protocol = g_supportedProtocols[i].protocol;
                break;
            }
        }
        if (TRANSPORT_PROTOCOL_UNKNOWN == g_protocol ) {
            g_protocol = TRANSPORT_PROTOCOL_HTTP;
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_0, "*** --proto value " << g_proto << " unknonw. Defaulting to http");
        }
    } else {
        g_protocol = TRANSPORT_PROTOCOL_HTTP;
    }

    g_password = securitylib::getPassphrase();

    if (g_ip.empty() && TRANSPORT_PROTOCOL_HTTP == g_protocol) {
        std::cout << "*** missing -i ip\n";
        argsOk = false;
    }

    if (g_port.empty() && g_sslPort.empty() && TRANSPORT_PROTOCOL_HTTP == g_protocol && !g_csCreds) {
        std::cout << "*** missing port. Either -p or -l (or both options) or --cscreds required\n";
        argsOk = false;
    }

    if (g_containerSasUri.empty() && TRANSPORT_PROTOCOL_BLOB == g_protocol) {
        std::cout << "*** missing container SAS URI. --containersasuri required\n";
        argsOk = false;
    }

    if (g_useDp && g_useS2) {
        std::cout << "*** s2 and dp modes are mutually exclusive. Specify only one -m option\n";
        argsOk = false;
    }

    if (!g_putFile.empty() && g_remoteDir.empty()) {
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_0, "*** --put option requires -d option\n");
        argsOk = false;
    }

    if (yOptionSet && (g_useDp || g_useS2)) {
        std::cout << "*** warning -y option ignored when -m option used\n"
                  << "    update drscout.conf TransportResponseTimeoutSeconds and TransportConnectTimeoutSeconds  to change timeouts\n";
    }

    if ((COMPRESS_CXSIDE == g_compressMode || g_zCompress) && g_validateData ) {
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_0, "*** note validation will be skipped as compression (-z or -Z option) enabled");
    }

    if (g_useBaseName && 0 == g_finalPathsCount ) {
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_0, "*** note -B options will be ignored as -M option not specified\n");
    }

    if (TRANSPORT_PROTOCOL_CFS == g_protocol) {
        if (g_cfsName.empty()) {
            std::cout << "*** missing --cfsname, required for --proto cfs\n";
            argsOk = false;
        }
        if (g_psId.empty()) {
            std::cout << "*** missing --ps <id>, required for --proto cfs\n";
            argsOk = false;
        }
    }

    if (!argsOk) {
        std::cout << "\n";
        usage();
    }

    // becuase of new cxps restrictions need to use a subdir and create it
    // when perfomring testing and no remote dir is specified as the cxps dir
    // maybe on the exclude_dir list
    if (g_remoteDir.empty() && !g_generalXfer) {
        g_remoteDir = "tstdata/";
        g_remoteDir += g_hostId;
        g_createDirs = true;
    }
    return argsOk;
}

/// \brief prompt user to continue to next request
void promptUser(char const* msg)
{
    if (g_prompt) {
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_0, "about to issue " << msg << "\nEnter to continue (s to skip prompts): ");
        std::string prompt;
        std::getline(std::cin, prompt);
        if (!prompt.empty() && 's' == prompt[0]) {
            g_prompt = false;
        }
    }
}

/// \brief sets up full remote names that should be used for requests
void setRemoteNames(boost::filesystem::path const &localName,  ///< local name being used
                    boost::filesystem::path const & remoteDir, ///< remote dir to be used
                    std::string& remoteFileName,               ///< set to the put remote file name to be used
                    std::string& newRemoteFileName,            ///< set to the new remote file name to be used
                    std::string& fileSpec,                     ///< set to file spec to be used to list the original remote file
                    std::string& newFileSpec,                  ///< set to file spec to be used to list the new remote file
                    unsigned long long count)                  ///< current run count
{
    std::stringstream id;
    id <<  boost::this_thread::get_id() << '-' << count << '-' << securitylib::genRandNonce(16);

    if (remoteDir.empty()) {
        if (TRANSPORT_PROTOCOL_FILE == g_protocol) {
            remoteFileName = "remote-";
            newRemoteFileName = "remote-";
        }

        if (localName.empty()) {
            remoteFileName += "start_filename_";
            remoteFileName += &g_hostName[0];
            remoteFileName += "_start";
            newRemoteFileName += "end_filename_";
            newRemoteFileName += &g_hostName[0];
            newRemoteFileName += "_end";
            if (g_useExtendedPathName) {
                remoteFileName += g_extendedPathName.substr(0, g_extendedPathName.size() - remoteFileName.size());
                newRemoteFileName += g_extendedPathName.substr(0, g_extendedPathName.size() - remoteFileName.size());
            }
        } else {
            remoteFileName += "start_";
            remoteFileName += localName.filename().string();
            newRemoteFileName += "end_";
            newRemoteFileName += localName.filename().string();
        }

        if (TRANSPORT_PROTOCOL_FILE == g_protocol) {
            fileSpec = "remote-";
            newFileSpec = "remote-";
        }

        fileSpec += "start_*";
        newFileSpec += "end_*";

    } else {
        std::string fileName("/start_");
        if (localName.empty()) {
            fileName += "filename_start";
            if (g_useExtendedPathName) {
                fileName += g_extendedPathName.substr(0, g_extendedPathName.size() - fileName.size());
            }
        } else {
            fileName += localName.filename().string();
        }

        remoteFileName = remoteDir.string() + fileName;

        fileName = "/end_";
        if (localName.empty()) {
            fileName += "filename_end";
            if (g_useExtendedPathName) {
                newRemoteFileName += g_extendedPathName.substr(0, g_extendedPathName.size() - fileName.size());
            }
        } else {
            fileName += localName.filename().string();
        }

        newRemoteFileName = remoteDir.string() + fileName;

        fileSpec = remoteDir.string() + "/start_*";
        newFileSpec = remoteDir.string() + "/end_*";
    }

    remoteFileName += ".";
    remoteFileName += id.str();
    newRemoteFileName += ".";
    newRemoteFileName += id.str();

    if (g_useBaseName && g_finalPathsCount > 0) {
        boost::filesystem::path tmpName(newRemoteFileName);
        newRemoteFileName = tmpName.filename().string();
    }

    if (!compressFile(localName) && g_zCompress) {
        remoteFileName += "diff_.dat";
        newRemoteFileName += "diff_.dat";
    }

    if (TRANSPORT_PROTOCOL_BLOB == g_protocol) {
        newFileSpec = fileSpec;
        newRemoteFileName = remoteFileName;
    }

    for (int i = 0; i < g_finalPathsCount; ++i) {
        if (!g_remoteDir.empty()) {
            g_finalPaths += g_remoteDir;
            g_finalPaths += '/';
        }
        g_finalPaths += "final";
        g_finalPaths += boost::lexical_cast<std::string>(i);
        g_finalPaths += ';';
    }
}

/// \brief fills buffer with random data
void fillBuffer(chBuffer_t& buffer) ///< buffer to be filled
{
    // use well known markers for being and end fencing
    std::size_t size = buffer.size();
    if (size < FENCE_SIZE * 2) {
        memset(&buffer[0], FENCE_CHAR, buffer.size());
    } else {
        memset(&buffer[0], FENCE_CHAR, FENCE_SIZE);
        for (std::size_t idx = FENCE_SIZE; idx < size - FENCE_SIZE; ++idx) {
            buffer[idx] = g_asciiPrintableUni();
        }
        memset(&buffer[size - FENCE_SIZE], FENCE_CHAR, FENCE_SIZE);
    }
}

/// \brief validates the returned data against the data sent
void validateData(chBuffer_t& matchBuffer,      ///< buffer containing original data sent
                  std::size_t& matchIdx,        ///< current index into the match buffer to start comparing against
                  char const* validateBuffer,   ///< buffer containing the data to be validated
                  std::size_t bytesToValidate) ///< total number of bytes in validateBuffer to be validated
{
    if (!g_validateData) {
        return;
    }

    if (g_zCompress || COMPRESS_CXSIDE == g_compressMode) {
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_0, "skipping validation as compression enabled (-z or -Z)");
        return;
    }

    bool ok = true;

    for (std::size_t i = 0; i < bytesToValidate; ++i) {
        if (0 != matchIdx && 0 == matchIdx % matchBuffer.size()) {
            matchIdx = 0;
        }
        if (matchBuffer[matchIdx] != validateBuffer[i]) {
            ok = false;
        }
        ++matchIdx;
    }

    if (ok) {
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_0, "data passed validation");
    } else {
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** data failed validation");
    }
}

/// \brief validates the orginal put file with the returned get file
void validateFile(std::string const& putName,  ///< original put file name
                  std::string const& gotName)  ///< retrieved get file name
{
    MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "validating file");

    if (g_zCompress) {
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_0, "*** -z option enabled, skipping file validation");
        return;
    }

    std::ifstream putFile(putName.c_str(), std::ios::binary);
    if (!putFile.good()) {
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_0, "*** error opening file " << putName << ": " << errno);
        return;
    }

    std::ifstream gotFile(gotName.c_str(), std::ios::binary);
    if (!gotFile.good()) {
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** error opening file " << gotName << ": " << errno);
        return;
    }

    bool ok = true;
    chBuffer_t putFileBuffer(g_bufferSize);
    chBuffer_t gotFileBuffer(g_bufferSize);
    while (!putFile.eof() && !gotFile.eof()) {
        putFile.read(&putFileBuffer[0], putFileBuffer.size());
        gotFile.read(&gotFileBuffer[0], gotFileBuffer.size());
        if (putFile.gcount() != gotFile.gcount() || 0 != memcmp(&putFileBuffer[0], &gotFileBuffer[0], putFile.gcount())) {
            ok = false;
        }
    }
    if (!ok || !putFile.eof() || !gotFile.eof()) {
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** files failed validation");
    } else {
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, " files passed validation");
    }
}

void tstListFile(ClientAbc::ptr client)
{
    std::string fileSpec("notfound*");

    std::string files;
    for (unsigned long long i = 0; i < g_numberOfRuns; ++i) {
        if (g_quit) {
            break;
        }
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_1, ">>>>> run: " << (i +1) << " of " << g_numberOfRuns << " <<<<<");
        try {

            if (CLIENT_RESULT_NOT_FOUND == client->listFile(fileSpec, files)) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** no files found for " << fileSpec);
            } else {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "found files: " << files);
            }
            if (i + 1 < g_numberOfRuns) {
                boost::this_thread::sleep(boost::posix_time::milliseconds(1000 * g_listFileTst));
            }
        } catch (std::exception& e) {
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** run " << (i + 1) << ": " << e.what());
            try {
                client->abortRequest(true);
            } catch(...) {
            }
        }
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_1,  ">>>>> end run: " << (i + 1) << " of " << g_numberOfRuns << " <<<<<");
    }
}

static void tstClient_DeleteFile(ClientAbc::ptr client,
    const std::string &fileName,
    const char *operation)
{
    WallClockTimer wcTimer;

    promptUser(operation);

    // delete the new named file
    MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling " << operation << "(" << fileName << ")");
    wcTimer.start();

    client->deleteFile(fileName);

    wcTimer.stop();
    MY_SIMPLE_TRACE(VERBOSE_LEVEL_2, operation << " time seconds: " << wcTimer.elapsedSeconds());
}

/// \brief tests using new client APIs directly
void tstClient(ClientAbc::ptr client,               ///< pointer to the client to use for the test
               boost::filesystem::path localName,   ///< local file to be sent
               boost::filesystem::path remoteDir,   ///< remote directory to be used for the test
               boost::filesystem::path outFileName) ///< local file to receive data when using file to file test
{
    WallClockTimer wcTimer;

    chBuffer_t buffer(g_bufferSize);
    fillBuffer(buffer);

    std::size_t totalBytesToSend;
    std::size_t bytesSent = 0;

    unsigned long long logoutEveryNRuns = g_logoutEveryNRuns;
    for (unsigned long long i = 0; i < g_numberOfRuns; ++i) {
        if (g_quit) {
            break;
        }

        totalBytesToSend = bytesToSend(g_minBytesToSend, g_maxBytesToSend);

        bool oldRemoteFilePendingCleanup = false;
        bool newRemoteFilePendingCleanup = false;
        std::string remoteFileName;
        std::string newRemoteFileName;

        MY_SIMPLE_TRACE(VERBOSE_LEVEL_1, ">>>>> run: " << (i +1) << " of " << g_numberOfRuns << " <<<<<");
        try {
            std::string fileSpec;
            std::string newFileSpec;
            std::string files;

            setRemoteNames(localName, remoteDir, remoteFileName, newRemoteFileName, fileSpec, newFileSpec, i);

            promptUser("putFile");
            oldRemoteFilePendingCleanup = true;
            wcTimer.start();

            unsigned long long delaySeconds = 0;
            for (int putRepeat = 0; putRepeat < g_putRepeat && !g_quit; ++putRepeat) {
                // send file or buffer
                if (!localName.empty()) {
                    MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling putFile(" << remoteFileName << ", " << localName.string() << ")");
                    client->putFile(remoteFileName, localName.string(), g_compressMode, g_createDirs);
                } else {
                    bytesSent = 0;
                    std::size_t sendBytes;

                    do {
                        sendBytes = (buffer.size() < (totalBytesToSend - bytesSent) ? buffer.size() : (totalBytesToSend - bytesSent));
                        MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling putFile("
                                        << sendBytes
                                        << ", &buffer[0], "
                                        << (g_useZeroLengthPutfileEof ? "true)" : ((bytesSent + sendBytes) < totalBytesToSend) ? "true)" : "false)"));
                        client->putFile(remoteFileName,
                                        sendBytes, &buffer[0],
                                        (g_useZeroLengthPutfileEof ? true : ((bytesSent + sendBytes) < totalBytesToSend)),
                                        g_compressMode,
                                        g_createDirs);
                        bytesSent += sendBytes;
                        delayIfRequested(delaySeconds);
                    } while (bytesSent < totalBytesToSend);
                    if (g_useZeroLengthPutfileEof) {
                        MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling putFile(0, &buffer[0], false)");
                        client->putFile(remoteFileName, 0, &buffer[0], false, g_compressMode, g_createDirs);
                    }
                    MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "total bytes sent: " << bytesSent);
                }
            }
            wcTimer.stop();
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_2, "putfile time seconds: " << wcTimer.elapsedSeconds() << ", average: " << (wcTimer.elapsedSeconds() / g_putRepeat) << " total delay seconds: " << delaySeconds );
            bytesSent = -1;
            delaySeconds = 0;

            promptUser("listFile");

            // see if the file is there
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling listFile(" << fileSpec << ", files)");
            wcTimer.start();;
            if (CLIENT_RESULT_NOT_FOUND == client->listFile(fileSpec, files)) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** no files found for " << fileSpec);
            } else {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "found files: " << files);
            }
            wcTimer.stop();
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_2, "listfile time seconds: " << wcTimer.elapsedSeconds());

            promptUser("renameFile");

            // rename the sent file/data to a new name
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling renameFile(" << remoteFileName << ", " << newRemoteFileName << ")");
            newRemoteFilePendingCleanup = true;
            wcTimer.start();
            client->renameFile(remoteFileName, newRemoteFileName, g_compressMode, g_finalPaths);
            wcTimer.stop();
            oldRemoteFilePendingCleanup = false;
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_2, "renamefile time seconds: " << wcTimer.elapsedSeconds());

            files.clear();

            promptUser("listFile");

            if (COMPRESS_CXSIDE == g_compressMode || compressFile(newRemoteFileName)) {
                newRemoteFileName += ".gz";
            }

            // see if the new named file is there
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling listFile(" << newFileSpec << ", files)");
            wcTimer.start();;
            if (CLIENT_RESULT_NOT_FOUND == client->listFile(newFileSpec, files)) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** no files found for " << newFileSpec);
            } else {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "files found: " << files);
            }
            wcTimer.stop();
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_2, "listfile time seconds: " << wcTimer.elapsedSeconds());

            std::size_t bytesReturned = 0;
            ClientCode rc = CLIENT_RESULT_OK;

            promptUser("getFile");
            wcTimer.start();;
            for (int getRepeat = 0; getRepeat < g_getRepeat && !g_quit; ++getRepeat) {

                // get the new named file
                if (!localName.empty()) {
                    MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling getFile(" << newRemoteFileName << ", " << outFileName.string());
                    if (g_generateChecksum) {
                        std::string checksum;
                        rc = client->getFile(newRemoteFileName, outFileName.string(), checksum);
                        MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "recevied file checksum: " << checksum);
                    } else {
                        rc = client->getFile(newRemoteFileName, outFileName.string());
                    }
                    validateFile(localName.string(), outFileName.string());
                } else {
                    chBuffer_t getBuffer(g_bufferSize);
                    std::size_t matchIdx = 0;
                    std::size_t totalBytes = 0;
                    do {
                        MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling getFile(" << newRemoteFileName << ", " << getBuffer.size() << ", bytesReturned)");
                        rc = client->getFile(newRemoteFileName, getBuffer.size(), &getBuffer[0], bytesReturned);
                        MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "recevied bytes: " <<  bytesReturned);
                        if (bytesReturned > 0) {
                            totalBytes += bytesReturned;
                            // do something with the data
                            validateData(buffer, matchIdx, &getBuffer[0], bytesReturned);
                        }
                        delayIfRequested(delaySeconds);
                    } while (CLIENT_RESULT_MORE_DATA == rc);

                    if (totalBytes != totalBytesToSend) {
                        if (g_zCompress || COMPRESS_CXSIDE == g_compressMode) {
                            MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "compression enabled (-z or -Z) so bytes received ("
                                            << totalBytes
                                            << ") may not match bytes sent ("
                                            <<  totalBytesToSend
                                            <<  ")");
                        } else {
                            MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** bytes recevied (" << totalBytes << ") != bytes sent(" << totalBytesToSend << ")");
                        }
                    }
                }
            }
            wcTimer.stop();
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_2, "getfile time seconds: " << wcTimer.elapsedSeconds() << ", average: " << (wcTimer.elapsedSeconds() / g_getRepeat) << " total delay seconds: " << delaySeconds);
            delaySeconds = 0;
            
            if (CLIENT_RESULT_NOT_FOUND == rc) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** not found");
            }

            bytesReturned = 0;

            // delete the new named file
            if (g_finalPathsCount > 0) {
                tstClient_DeleteFile(client, g_finalPaths, "cleanDirs");
            } else {
                tstClient_DeleteFile(client, newRemoteFileName, "deleteFile");
            }

            newRemoteFilePendingCleanup = false;

            promptUser("calling heartbeat");
            client->heartbeat(false);
            client->heartbeat(true);

            --logoutEveryNRuns;
            if (g_logout && 0 == logoutEveryNRuns) {
                logoutEveryNRuns = g_logoutEveryNRuns;
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling logout(" << newRemoteFileName << ")");
                promptUser("calling logout");
                wcTimer.start();;
                client->abortRequest();
                wcTimer.stop();
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_2, "abort (logout) time seconds: " << wcTimer.elapsedSeconds());
            }

            promptUser("done with this run");
        } catch (std::exception& e) {
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** run " << (i + 1) << ": " << e.what());

            if (-1 != bytesSent) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_0, "bytesSent: " << bytesSent << '\n');
            }

            if (oldRemoteFilePendingCleanup) {
                try {
                    tstClient_DeleteFile(client, remoteFileName, "old remote file cleanup");

                    oldRemoteFilePendingCleanup = false;
                } catch (std::exception& e) {
                    MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** run " << (i + 1) << ": old remote file cleanup (" << remoteFileName << ") failed : " << e.what());
                } catch (...) {
                    MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** run " << (i + 1) << ": old remote file cleanup (" << remoteFileName << ") failed.");
                }
            }

            bool useFinalPaths = (g_finalPathsCount > 0);

            if (newRemoteFilePendingCleanup) {
                try {
                    // cleanup the new named file
                    if (useFinalPaths) {
                        tstClient_DeleteFile(client, g_finalPaths, "new remote final paths cleanup");
                    }
                    else {
                        tstClient_DeleteFile(client, newRemoteFileName, "new remote file cleanup");
                    }

                    newRemoteFilePendingCleanup = false;
                } catch (std::exception& e) {
                    MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR,
                        "*** run " << (i + 1) << ": new remote file cleanup (" << (useFinalPaths ? g_finalPaths : newRemoteFileName)
                        << ") failed - final paths (" << useFinalPaths << ") : " << e.what());
                } catch (...) {
                    MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR,
                        "*** run " << (i + 1) << ": new remote file cleanup (" << (useFinalPaths ? g_finalPaths : newRemoteFileName)
                        << ") failed - final paths (" << useFinalPaths << ")");
                }
            }

            try {
                client->abortRequest(true);
            } catch(...) {
            }
        }
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_1,  ">>>>> end run: " << (i + 1) << " of " << g_numberOfRuns << " <<<<<");
    }
}

/// \brief runs test using same "client" APIs used by dataprotection/cache manager
void tstDp(TRANSPORT_CONNECTION_SETTINGS const& transportSettings, ///< transport settings to be used
           bool secure,                                            ///< determines secure mode true: yes false: no
           boost::filesystem::path localName,                      ///< local file to send
           boost::filesystem::path remoteDir,                      ///< remote directory to use
           boost::filesystem::path outFileName)                    ///< local file to receive data when using file to tile
{
    WallClockTimer wcTimer;

    CxTransport::ptr client;

    std::size_t totalBytesToSend;
    std::size_t bytesSent = 0;

    chBuffer_t buffer(g_bufferSize);
    fillBuffer(buffer);

    char* getBuffer = 0;

    unsigned long long logoutEveryNRuns = g_logoutEveryNRuns;

    for (unsigned long long i = 0; i < g_numberOfRuns; ++i) {
        if (g_quit) {
            break;
        }

        totalBytesToSend = bytesToSend(g_minBytesToSend, g_maxBytesToSend);

        MY_SIMPLE_TRACE(VERBOSE_LEVEL_1, ">>>>> run: " << (i +1) << " of " << g_numberOfRuns << " <<<<<");
        try {

            if (0 == i || (g_logout && 0 == logoutEveryNRuns)) {
                client.reset(new CxTransport(g_protocol, transportSettings, secure, g_protocol == TRANSPORT_PROTOCOL_CFS, g_psId));
                logoutEveryNRuns = g_logoutEveryNRuns;
            }
            --logoutEveryNRuns;
            
            std::string remoteFileName;
            std::string newRemoteFileName;
            std::string fileSpec;
            std::string newFileSpec;

            setRemoteNames(localName, remoteDir, remoteFileName, newRemoteFileName, fileSpec, newFileSpec, i);

            FileInfos_t files;

            boost::tribool tb;

            // send file or buffer
            promptUser("putFile");

            wcTimer.start();

            unsigned long long delaySeconds = 0;
            for (int putRepeat = 0; putRepeat < g_putRepeat && !g_quit; ++putRepeat) {
                if (!localName.empty()) {
                    MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling putFile(" << remoteFileName << ", " << localName.string() << ")");
                    if (!client->putFile(remoteFileName, localName.string(), g_compressMode, g_createDirs)) {
                        MY_SIMPLE_TRACE(VERBOSE_LEVEL_0, "*** FAILED DataProtectionSync::SendFile for " << localName.string() << " err: " << client->status());
                        client->abortRequest(remoteFileName);
                        return;
                    }
                } else {
                    bytesSent = 0;
                    std::size_t sendBytes;
                    do {
                        sendBytes = (buffer.size() < (totalBytesToSend - bytesSent) ? buffer.size() : (totalBytesToSend - bytesSent));
                        MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling putFile("
                                        << buffer.size()
                                        << ", &buffer[0], "
                                        << (g_useZeroLengthPutfileEof ? "true)" : ((bytesSent + sendBytes) < totalBytesToSend) ? "true)" : "false)"));
                        if (!client->putFile(remoteFileName, sendBytes, &buffer[0], (g_useZeroLengthPutfileEof ? true : ((bytesSent + sendBytes) < totalBytesToSend)), g_compressMode, g_createDirs)) {
                            MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** FAILED " << (i + 1)
                                            << " DataProtectionSync::SendFile  put count: " << putRepeat
                                            << " total bytes sent: " << bytesSent << " err: " << client->status());
                            client->abortRequest(remoteFileName);
                            return;
                        }
                        bytesSent += sendBytes;
                        delayIfRequested(delaySeconds);
                    } while (bytesSent < totalBytesToSend);
                    if (g_useZeroLengthPutfileEof) {
                        MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling putFile(0, &buffer[0], false)");
                        client->putFile(remoteFileName, 0, &buffer[0], false, g_compressMode, g_createDirs);
                    }
                    MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "total bytes sent: " << bytesSent);
                }
            }
            wcTimer.stop();
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_2, "putfile time seconds: " << wcTimer.elapsedSeconds() << ", average: " << (wcTimer.elapsedSeconds() / g_putRepeat) << " total delay seoncds : " << delaySeconds);
            delaySeconds = 0;
            bytesSent = -1;
            promptUser("listFile");

            // see if the file is there
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling listFile(" << fileSpec << ", files)");
            wcTimer.start();;
            tb = client->listFile(fileSpec, files);
            wcTimer.stop();
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_2, "listfile time seconds: " << wcTimer.elapsedSeconds());

            if (tb) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "found files:");
            } else if (!tb) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** listfile error: " << client->status());
            } else {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** no files found for " << fileSpec);
            }

            promptUser("renameFile");

            // rename the sent file/data to a new name
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling renameFile(" << remoteFileName << ", " << newRemoteFileName << ")");
            wcTimer.start();;
            tb = client->renameFile(remoteFileName, newRemoteFileName, g_compressMode);
            wcTimer.stop();
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_2, "renamefile time seconds: " << wcTimer.elapsedSeconds());
            if (tb) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "file renamed");
            } else if (!tb) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** renameFile error: " << client->status());
            } else {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** renameFile file not found: " << remoteFileName);
            }

            files.clear();

            promptUser("listFile");

            if (COMPRESS_CXSIDE == g_compressMode || compressFile(newRemoteFileName)) {
                newRemoteFileName += ".gz";
            }

            // see if the new named file is there
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling listFile(" << newFileSpec << ", files)");
            wcTimer.start();;
            tb == client->listFile(newFileSpec, files);
            wcTimer.stop();
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_2, "listfile time seconds: " << wcTimer.elapsedSeconds());
            if (tb) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "found files");
            } else if (!tb) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** listfile error: " << client->status());
            } else {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** no files found for " << newFileSpec);
            }

            std::size_t bytesReturned = 0;

            promptUser("getFile");

            wcTimer.start();

            for (int getRepeat = 0; getRepeat < g_getRepeat && !g_quit; ++getRepeat) {
                // get the new named file
                if (!localName.empty()) {
                    MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling getFile(" << newRemoteFileName << ", " << outFileName.string());
                    if (g_generateChecksum) {
                        std::string checksum;
                        tb = client->getFile(newRemoteFileName, outFileName.string(), checksum);
                        MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "recevied file checksum: " << checksum);
                    } else {
                        tb = client->getFile(newRemoteFileName, outFileName.string());
                    }
                } else {
                    std::size_t getBufferSize = totalBytesToSend;
                    std::size_t matchIdx = 0;
                    std::size_t totalBytes = 0;
                    // no need to loop as older style requires buffer to be large enough or it is an error
                    MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling getFile(" << newRemoteFileName << ", " << getBufferSize << ", bytesReturned)");
                    tb = client->getFile(newRemoteFileName, getBufferSize, &getBuffer, bytesReturned);
                    if (tb) {
                        if (bytesReturned != totalBytesToSend) {
                            MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "client returned OK - status: " << client->status());
                            MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "bytes recevied (" << bytesReturned << ") != bytes sent(" << totalBytesToSend << ")");
                        }
                        MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "recevied bytes: " <<  bytesReturned);
                        if (bytesReturned > 0) {
                            // do something with the data
                            validateData(buffer, matchIdx, getBuffer, bytesReturned);
                        }
                    } else if (!tb) {
                        MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** getFile to buffer failed: " << client->status());
                    }
                    delayIfRequested(delaySeconds);
                }
                FREE_BUFFER(getBuffer);
            }
            wcTimer.stop();
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_2, "getfile time seconds: " << wcTimer.elapsedSeconds() << ", average: " << (wcTimer.elapsedSeconds() / g_getRepeat) << " total delay seconds: " << delaySeconds);
            delaySeconds = 0;
            bytesReturned = 0;

            if (boost::indeterminate == tb) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** " << newRemoteFileName << " not found");
            }

            promptUser("deleteFile");

            // delete the new named file
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling deleteFile(" << newRemoteFileName << ")");
            wcTimer.start();;
            tb = client->deleteFile(newRemoteFileName);
            wcTimer.stop();
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_2, "deletefile time seconds: " << wcTimer.elapsedSeconds());
            if (tb) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "deleteFile ok");
            } else if (!tb) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** deleteFile error: " << client->status());
            } else {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** deletFile file not found for " << newRemoteFileName);
            }

            promptUser("calling heartbeat");
            client->heartbeat(false);
            client->heartbeat(true);

            promptUser("done with this run");
        } catch (std::exception& e) {
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_0, "*** run " << (i + 1) << ": " << e.what());
            try {
                client.reset(new CxTransport(g_protocol == TRANSPORT_PROTOCOL_FILE ? TRANSPORT_PROTOCOL_FILE : TRANSPORT_PROTOCOL_HTTP, transportSettings, secure, g_protocol == TRANSPORT_PROTOCOL_CFS, g_psId));
            } catch (std::exception& e) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_0, "*** run " << (i + 1) << " unable to instantiate CxTransport: " << e.what());
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** remember you need the agent installed to use dp mode\n");
                break;
            }
        }
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_1, ">>>>> end run: " << (i +1) << " of " << g_numberOfRuns << " <<<<<");
    }
}

/// \brief tests using the same "client" APIs used by s2
void tstS2(TRANSPORT_CONNECTION_SETTINGS const& transportSettings, ///< transport settings to be used
           bool secure,                                            ///< determines secure mode true: yes false: no
           boost::filesystem::path localName,                      ///< local file to send
           boost::filesystem::path remoteDir,                      ///< remote directory to use
           boost::filesystem::path outFileName)                    ///< local file to receive data when using file to tile
{
    WallClockTimer wcTimer;

    TransportStream* client = 0;

    std::size_t totalBytesToSend;

    chBuffer_t buffer(g_bufferSize);
    fillBuffer(buffer);

    unsigned long long logoutEveryNRuns = g_logoutEveryNRuns;

    for (unsigned long long i = 0; i < g_numberOfRuns; ++i) {
        if (g_quit) {
            break;
        }

        totalBytesToSend = bytesToSend(g_minBytesToSend, g_maxBytesToSend);

        MY_SIMPLE_TRACE(VERBOSE_LEVEL_1, ">>>>> run: " << (i +1) << " of " << g_numberOfRuns << " <<<<<");
        try {

            if (0 == i || (g_logout && 0 == logoutEveryNRuns)) {
                delete client;
                client =  new TransportStream(g_protocol, transportSettings);
                logoutEveryNRuns = g_logoutEveryNRuns;
            }
            --logoutEveryNRuns;
                
            STREAM_MODE mode = GenericStream::Mode_Open | GenericStream::Mode_RW;

            if (secure) {
                mode |= GenericStream::Mode_Secure;
            }

            std::string remoteFileName;
            std::string newRemoteFileName;
            std::string fileSpec;
            std::string newFileSpec;

            setRemoteNames(localName, remoteDir, remoteFileName, newRemoteFileName, fileSpec, newFileSpec, i);

            std::string newName;

            if (SV_FAILURE == client->Open(mode)) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** FAILED client->Open()");
                return;
            }

            // send file or buffer
            promptUser("putFile");

            wcTimer.start();

            unsigned long long delaySeconds = 0;
            for (int getRepeat = 0; getRepeat < g_getRepeat && !g_quit; ++getRepeat) {
                if (!localName.empty()) {
                    MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling putFile(" << remoteFileName << ", " << localName.string() << ")");
                    if (SV_FAILURE == client->Write(localName.string(), remoteFileName, g_compressMode, newName)) {
                        MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** FAILED DataProtectionSync::SendFile for " << localName.string());
                        client->Abort(remoteFileName.c_str());
                        return;
                    }
                } else {
                    std::size_t bytesSent = 0;
                    std::size_t sendBytes;
                    do {
                        sendBytes = (buffer.size() < (totalBytesToSend - bytesSent) ? buffer.size() : (totalBytesToSend - bytesSent));
                        MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling putFile("
                                        << buffer.size()
                                        << ", &buffer[0], "
                                        << (g_useZeroLengthPutfileEof ? "true)" : ((bytesSent + sendBytes) < totalBytesToSend) ? "true)" : "false)"));

                        if (SV_FAILURE ==  client->Write(&buffer[0], sendBytes, remoteFileName, (g_useZeroLengthPutfileEof ? true : ((bytesSent + sendBytes) < totalBytesToSend)), g_compressMode)) {
                            MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** FAILED DataProtectionSync::SendFile buffer");
                            client->Abort(remoteFileName.c_str());
                            return;
                        }
                        bytesSent += sendBytes;
                        delayIfRequested(delaySeconds);
                    } while (bytesSent < totalBytesToSend);
                    if (g_useZeroLengthPutfileEof) {
                        MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "calling putFile(0, &buffer[0], false)");
                        client->Write(&buffer[0], 0, remoteFileName, false, g_compressMode);
                    }
                    MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "total bytes sent: " << bytesSent);
                }
            }
            wcTimer.stop();
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_2, "putfile time seconds: " << wcTimer.elapsedSeconds() << ", average: " << (wcTimer.elapsedSeconds() / g_getRepeat) << " total delay seconds: " << delaySeconds);
            delaySeconds = 0;
            promptUser("renameFile");

            if (newName.empty() || localName.empty()) {
                // rename
                wcTimer.start();;
                if (SV_FAILURE == client->Rename(remoteFileName, newRemoteFileName, g_compressMode)) {
                    MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** FAILED rename");
                    if (SV_FAILURE == client->Delete(remoteFileName.c_str())) {
                        MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** FAILED to delete " << remoteFileName);
                    }
                }
                wcTimer.stop();
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_2, "renamefile time seconds: " << wcTimer.elapsedSeconds());
            }

            promptUser("deleteFile");

            // delete
            if (COMPRESS_CXSIDE == g_compressMode || compressFile(newRemoteFileName)) {
                newRemoteFileName += ".gz";
            }

            wcTimer.start();;
            if (SV_FAILURE == client->DeleteFile(newRemoteFileName.c_str())) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** FAILED to delete " << newRemoteFileName);
            }
            wcTimer.stop();
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_2, "deletefile time seconds: " << wcTimer.elapsedSeconds());

            promptUser("make sure needs delete flag is working correctly");

            // just make sure no bugs in the implemenation of these function
            client->SetNeedToDeletePreviousFile(true);
            if (client->NeedToDeletePreviousFile()) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_3, "need to delete previous file");
                client->SetNeedToDeletePreviousFile(false);
                if (client->NeedToDeletePreviousFile()) {
                    MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** ERROR - clearing need to delete previous file failed");
                }
            } else {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** ERROR - setting need to delete previous file failed");
            }

            promptUser("done with this run");
        } catch (std::exception& e) {
            MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** run " << (i + 1) << ": " << e.what());
            if (0 != client) {
                delete client;
            }
            try {
                client =  new TransportStream(g_protocol, transportSettings);
            } catch (std::exception& e) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_0, "*** run " << (i + 1) << " unable to instantiate CxTransport: " << e.what());
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_ERROR, "*** remember you need the agent installed to use s2 mode\n");
                break;
            }
        }
        MY_SIMPLE_TRACE(VERBOSE_LEVEL_1, ">>>>> end run: " << (i +1) << " of " << g_numberOfRuns << " <<<<<");
    }

    if (0 != client) {
        delete client;
    }
}

/// \bried simple class for testing Client and cxps
class TstRunner {
public:
    TstRunner()
        {}

    ~TstRunner()
        {}

    void start()
        {
            // creat theard
            threads_t tstThreads;

            for (int i = 0; i < g_threadCnt; ++i) {
                tstThreads.push_back(boost::shared_ptr<boost::thread>(new boost::thread(
                                                                          boost::bind(&TstRunner::run,
                                                                                      this))));
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_0, "thread " << i << " started\n");
            }

            if ((std::numeric_limits<unsigned long long>::max)() == g_numberOfRuns) {
                waitForQuit();
                g_quit = true;
            }

            // Wait for all threads in the pool to exit.
            threads_t::iterator threadIter(tstThreads.begin());
            threads_t::iterator threadIterEnd(tstThreads.end());
            for (/* empty */; threadIter != threadIterEnd; ++threadIter) {
                (*threadIter)->join();
            }

        }

protected:
    void run()
        {

            std::cout << "---------- S T A R T I N G R U N----------" << std::endl;

            TRANSPORT_CONNECTION_SETTINGS transportSettings;
            transportSettings.connectTimeout = g_timeoutSeconds;
            transportSettings.ipAddress = g_ip;
            transportSettings.port = g_port;
            transportSettings.responseTimeout = g_timeoutSeconds;
            transportSettings.sslPort = g_sslPort;
            transportSettings.ftpPort = (g_port.empty() ? g_sslPort : g_port);
            transportSettings.ipAddress = g_containerSasUri;

            if ((!g_port.empty() || g_protocol == TRANSPORT_PROTOCOL_CFS) && !g_secure) {
                MY_SIMPLE_TRACE(VERBOSE_LEVEL_1, "===== start Client =====");
                g_outFileName = g_localName + ".out";
                if (g_useDp) {
                    tstDp(transportSettings, false, g_localName, g_remoteDir, g_outFileName);
                } else if (g_useS2) {
                    tstS2(transportSettings, false, g_localName, g_remoteDir, g_outFileName);
                } else {
                    ClientAbc::ptr client;
                    switch (g_protocol) {
                        case TRANSPORT_PROTOCOL_FILE:
                            client.reset(new FileClient_t(g_writeMode));
                            break;
                        case TRANSPORT_PROTOCOL_CFS:
                            client.reset(new HttpCfsClient_t(g_cfsName, g_psId, g_hostId, g_password, g_bufferSize, 0, g_timeoutSeconds, true, g_sendWindow, g_receiveWindow, g_writeMode));
                            break;
                        case TRANSPORT_PROTOCOL_HTTP:
                            client.reset(new HttpClient_t(g_ip, g_port, g_hostId, g_bufferSize, g_timeoutSeconds, g_timeoutSeconds, true, g_sendWindow, g_receiveWindow, g_writeMode, g_password));
                            break;
                        case TRANSPORT_PROTOCOL_BLOB:
                            std::cout << "note pageblob protocol does not support non secure, defaulting to secure\n";
                            client.reset(new AzureBlobClient(g_containerSasUri, g_bufferSize, g_timeoutSeconds));
                            break;
                        default:
                            MY_SIMPLE_TRACE(VERBOSE_LEVEL_1, "unknonw protocol: " << g_protocol);
                            return;
                    }
                    if (g_listFileTst > 0) {
                        tstListFile(client);
                    } else {
                        tstClient(client, g_localName, g_remoteDir, g_outFileName);
                    }
                }

                if (boost::filesystem::exists(g_outFileName)) {
                    boost::filesystem::remove(g_outFileName);
                }

                MY_SIMPLE_TRACE(VERBOSE_LEVEL_1, "===== end Client =====");
            }

            if (!g_sslPort.empty() || (g_protocol == TRANSPORT_PROTOCOL_CFS && g_secure) || g_secure) {

                MY_SIMPLE_TRACE(VERBOSE_LEVEL_1, "===== start SSL Client =====");

                g_outFileName = g_localName + ".sslout";
                if (g_useDp) {
                    tstDp(transportSettings, true, g_localName, g_remoteDir, g_outFileName);
                } else if (g_useS2) {
                    tstS2(transportSettings, true, g_localName, g_remoteDir, g_outFileName);
                } else {
                    ClientAbc::ptr client;
                    switch (g_protocol) {
                        case TRANSPORT_PROTOCOL_FILE:
                            std::cout << "note file protocol does not support secure, defaulting to non secure\n";
                            client.reset(new FileClient_t(g_writeMode));
                            break;
                        case TRANSPORT_PROTOCOL_CFS:
                            client.reset(new HttpsCfsClient_t(g_cfsName, g_psId, g_hostId, g_password, g_bufferSize, 0, g_timeoutSeconds, true, g_sendWindow, g_receiveWindow, g_clientSslFile, g_writeMode));
                            break;
                        case TRANSPORT_PROTOCOL_HTTP:
                            if (g_useCertAuth)
                                client.reset(new HttpsClient_t(g_ip, g_sslPort, g_hostId, g_bufferSize, 0, g_timeoutSeconds, true, g_sendWindow, g_receiveWindow, g_clientCertFile, g_clientKeyFile, g_serverThumbprint, g_writeMode));
                            else
                                client.reset(new HttpsClient_t(g_ip, g_sslPort, g_hostId, g_bufferSize, 0, g_timeoutSeconds, true, g_sendWindow, g_receiveWindow, g_clientSslFile, g_writeMode, g_password));

                            break;
                        case TRANSPORT_PROTOCOL_BLOB:
                            client.reset(new AzureBlobClient(g_containerSasUri, g_bufferSize, g_timeoutSeconds));
                            break;
                        default:
                            MY_SIMPLE_TRACE(VERBOSE_LEVEL_1, "unknonw protocol: " << g_protocol);
                            return;
                    }

                    if (g_listFileTst > 0) {
                        tstListFile(client);
                    } else {
                        tstClient(client, g_localName, g_remoteDir, g_outFileName);
                    }
                }

                if (boost::filesystem::exists(g_outFileName)) {
                    boost::filesystem::remove(g_outFileName);
                }

                MY_SIMPLE_TRACE(VERBOSE_LEVEL_1, "===== SSL Client =====");
            }
        }

private:

};

void printmap(std::map<std::string, std::string> mapobject)
{
    for (std::map<std::string, std::string>::iterator it = mapobject.begin(); it != mapobject.end(); it++)
    {
        std::cout << it->first << "\t" << it->second << std::endl;
    }
}

void printResponseData(ResponseData const& responsedata)
{
    std::cout << "Client code: " << responsedata.responseCode << std::endl <<std::endl;
    std::cout << "Headers:\n";
    printmap(responsedata.headers);
    std::cout << "Parameters:\n";
    printmap(responsedata.uriParams);
    std::cout << "Data: " << responsedata.data << std::endl;
}

int generalXfer()
{
    int rc = -1;

    ClientAbc* client = 0;
    try {
        if (!(g_secure || g_port.empty())) {
            if (g_protocol == TRANSPORT_PROTOCOL_CFS) {
                client = new HttpCfsClient_t(g_cfsName, g_psId, g_hostId, g_password, g_bufferSize, 0, g_timeoutSeconds, true, g_sendWindow, g_receiveWindow, g_writeMode);
            } else {
                client = new HttpClient_t(g_ip, g_port, g_hostId, g_bufferSize, 0, g_timeoutSeconds, true, g_sendWindow, g_writeMode, g_receiveWindow, g_password);
            }
        } else  if (!g_sslPort.empty()) {
            if (g_protocol == TRANSPORT_PROTOCOL_CFS) {
                client =  new HttpsCfsClient_t(g_cfsName, g_psId, g_hostId, g_password, g_bufferSize, 0, g_timeoutSeconds, true, g_sendWindow, g_receiveWindow, g_clientSslFile, g_writeMode);
            } else {
                if (g_useCertAuth)
                    client = new HttpsClient_t(g_ip, g_sslPort, g_hostId, g_bufferSize, 0, g_timeoutSeconds, true, g_sendWindow, g_receiveWindow, g_clientCertFile, g_clientKeyFile, g_serverThumbprint, g_writeMode);
                else
                    client = new HttpsClient_t(g_ip, g_sslPort, g_hostId, g_bufferSize, 0, g_timeoutSeconds, true, g_sendWindow, g_receiveWindow, std::string(), g_writeMode, g_password);
            }
        } else {
            std::cout << "*** missing port could not put file " << g_putFile <<'\n';
            return rc;
        }

        if (!g_putFile.empty()) {
            boost::filesystem::path localName(g_putFile);
            boost::filesystem::path remoteName(g_remoteDir);
            remoteName /= localName.filename();
            client->putFile(remoteName.string(), localName.string(), COMPRESS_NONE, g_createDirs);
            rc = 0;
        } else {
            boost::filesystem::path remoteName(g_remoteDir);
            remoteName /= g_getFile;
            boost::filesystem::path localName;
            if (g_getToFile.empty()) {
                localName = ".";
                localName /= remoteName.filename();
            } else {
                localName = g_getToFile;
            }
            ClientCode cc = CLIENT_RESULT_OK;
            if (g_generateChecksum) {
                std::string checksum;
                cc = client->getFile(remoteName.string(), localName.string(), checksum);
            } else {
                cc = client->getFile(remoteName.string(), localName.string());
            }
            if (CLIENT_RESULT_NOT_FOUND == cc) {
                std::cout << g_getFile << " not found\n";
                rc = 1;
            } else {
                rc = 0;
            }
        }
    } catch (ErrorException const& ee) {
        std::cout << ee.what() << '\n';;
    }
    catch (ThrottlingException const& te) {
        std::cout << te.what() << "\n";
        printResponseData(client->getResponseData());
    }
    catch (std::exception const& e) {
        std::cout << e.what() << '\n';
    } catch (...) {
        std::cout << "unknown exception caught\n";
    }
    delete client;
    return rc;
}

int getInitialSettings()
{
    Configurator* configurator;
    int retry = 3;
    do {
        if (InitializeConfigurator(&configurator, USE_CACHE_SETTINGS_IF_CX_NOT_AVAILABLE, PHPSerialize)) {
            break;
        }
    } while (--retry > 0);
    if (retry > 0) {
        configurator->GetCurrentSettings();
        InitialSettings settings = configurator->getInitialSettings();
        g_port = settings.transportSettings.port;
        g_sslPort = settings.transportSettings.sslPort;
        if (g_port.empty() || g_sslPort.empty()) {
            std::cout << " failed to get credentilas from cs\n";
            return -1;
        }
        return 0;
    }
    std::cout << "unable to connect to CS\n";
    return -1;
}

int setupLogging(std::string const& argv0)
{
    std::string programName;
    if (!getProgramFullPath(argv0, programName)) {
        return -1;
    }
    boost::filesystem::path programPathName(programName);
    programPathName.replace_extension("log");
    LocalConfigurator localconfigurator;
    int logSize = localconfigurator.getLocalLogSize();
    std::string logName = localconfigurator.getLogPathname() + programPathName.filename().string();
    SetLogFileName(logName.c_str());
    int defaultLevel = localconfigurator.getLogLevel();
    SetLogLevel(defaultLevel);
    SetLogHostId(localconfigurator.getHostId().c_str());
    SetLogInitSize(logSize);
    return 0;
}

/// \brief main entry point
int main(int argc, char* argv[])
{
    TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
    int rc = -1;
    try
    {
        LocalConfigurator localconfigurator;

        ACE::init();
        tal::GlobalInitialize();
        MaskRequiredSignals();

        gethostname(&g_hostName[0], g_hostName.size());
        g_hostId = localconfigurator.getHostId();

        if (!parseCmd(argc, argv)) {
            return -1;
        }

        boost::filesystem::path logPath;
        logPath = localconfigurator.getLogPathname();
        logPath /= "cxpsclient.log";

        SetLogFileName(logPath.string().c_str());
        SetLogLevel(SV_LOG_ALWAYS);
        SetLogRemoteLogLevel(SV_LOG_DISABLE);

        if (g_protocol == TRANSPORT_PROTOCOL_BLOB)
        {
            AzureBlobClient::s_logLevel = SV_LOG_ALWAYS;
        }

        if (g_csCreds) {
            rc = getInitialSettings();
            if (-1 == rc) {
                return rc;
            }
        }
        if (!g_putFile.empty() || !g_getFile.empty()) {
            rc = generalXfer();
        } else {
            std::cout << "---------- S T A R T I N G ----------" << std::endl;
            TstRunner tstRunner;
            tstRunner.start();
            std::cout << "----------- E X I T I N G -----------\n" << std::endl;
            rc = 0;
        }
    } catch (std::exception const& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        std::cout << "----- E R R O R   E X I T I N G -----\n" << std::endl;
    }

    tal::GlobalShutdown();
    return rc;
}
