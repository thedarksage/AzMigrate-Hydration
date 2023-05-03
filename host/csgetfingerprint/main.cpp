
#include <iostream>
#include <string>

#include "csgetfingerprint.h"
#include "terminateonheapcorruption.h"

void usage()
{
    std::cout << "\nusage: csgetfingerprint -i <ipaddress> -p <port> [-w <passphrase>] [-v] [-u <url>] [-r] | -h\n"
        << '\n'
        << "  Gets the CS fingerprint and writes it to default location " << securitylib::getFingerprintDir() << '\n'
        << "  this also serves as a passphrase verification. Use -v option if you only need to verify passphrase\n"
        << '\n'
        << "  -i <ipaddress> : CS server ip address\n"
        << "  -p <port>      : CS server port. Must be SSL port to get fingerprint.\n"
        << "  -w <passphrase>: connection passphrase. This will be verified as part of getting CS fingerprint.\n"
        << "                   Default uses locally stored passphrase. Optional\n"
        << "  -v             : verify passhrase only do not write fingerprint to local file. Optional\n"
        << "  -u <url>       : url to send the request to. Optional. Default CXAPI.php. Mainly used for developement\n"
        << "  -r             : Redirect the output to log\n"
        << "\n"
        << "  -h : show usage\n"
        << "\n";
}

bool parseCmdLine(int argc,
                  char* argv[],
                  std::string& ipAddress,
                  std::string& port,
                  std::string& passphrase,
                  std::string& url,
                  bool& verifyPhassphraseOnly,
                  bool& useSsl,
                  bool& overwrite,
                  bool& redirectToLog)
{
    overwrite = false;
    bool showHelp = false;
    bool argsOk = true;
    int i = 1;
    while (i < argc) {
        if ('-' != argv[i][0]) {
            std::cout << "*** invalid option: " << argv[i];
            argsOk = false;
        } else if ('\0' == argv[i][1]) {
            std::cout << "*** invalid option: " << argv[i];
            argsOk = false;
        } else if ('-' == argv[i][1]) {
            // long options
            // no long options
            std::cout << "*** Invalid command line option: " << argv[i];
            argsOk = false;
        } else {
            // short option
            switch (argv[i][1]) {
                case 'h':
                    showHelp = true;
                    break;
                case 'i':
                    ++i;
                    if (i >= argc || '-' == argv[i][0]) {
                        --i;
                        std::cout << "*** missing value for -i";
                        argsOk = false;
                    } else {
                        ipAddress = argv[i];
                    }
                    break;
                case 'n':
                    useSsl = false;
                    break;
                case 'o':
                    overwrite = true;
                    break;
                case 'p':
                    ++i;
                    if (i >= argc || '-' == argv[i][0]) {
                        --i;
                        std::cout << "*** missing value for -p";
                        argsOk = false;
                    } else {
                        port = argv[i];
                    }
                    break;
                case 'u':
                    ++i;
                    if (i >= argc || '-' == argv[i][0]) {
                        --i;
                        std::cout << "*** missing value for -u";
                        argsOk = false;
                    } else {
                        url = argv[i];
                    }
                    break;
                case 'v':
                    verifyPhassphraseOnly = true;
                    break;
                case 'w':
                    ++i;
                    if (i >= argc || '-' == argv[i][0]) {
                        --i;
                        std::cout << "*** missing value for -w";
                        argsOk = false;
                    } else {
                        passphrase = argv[i];
                    }
                    break;
                case 'r':
                    redirectToLog = true;
                    break;
                default:
                    std::cout << "*** Invalid command line option: " << argv[i];
                    argsOk = false;
            }
        }
        ++i;
    }

    if (showHelp) {
        usage();
    }
    if (ipAddress.empty()) {
        std::cout << "*** -i option required\n";
        argsOk = false;
    }

    if (port.empty()) {
        std::cout << "*** -p option required\n";
        argsOk = false;
    }

    if (passphrase.empty()) {
        passphrase = securitylib::getPassphrase();
        if (passphrase.empty()) {
            std::cout << "*** passphrase not found nor specifed on command line. Try adding -w option\n";
            argsOk = false;
        }
    }

    if (url.empty()) {
        url = "/ScoutAPI/CXAPI.php";
    } else {
        if ('/' != url[0]) {
            url.insert(0, "/");
        }
    }
    return argsOk;
}

int main(int argc, char* argv[])
{
    TerminateOnHeapCorruption();

    try {
        std::string hostId;
        std::string passphrase;
        std::string ipAddress;
        std::string port;
        std::string url;
        std::string fingerprint_output_file = "csgetfingerprint.log";
        bool verifyPassphraeOnly = false;
        bool useSsl = true;
        bool overwrite = false;
        bool redirectToLog = false;
        if (!parseCmdLine(argc, argv, ipAddress, port, passphrase, url, verifyPassphraeOnly, useSsl, overwrite, redirectToLog)) {
            usage();
            return -1;
        }
        std::string reply;
        bool rc = securitylib::csGetFingerprint(reply, hostId, passphrase, ipAddress, port, verifyPassphraeOnly, useSsl, overwrite, url);
        std::cout << reply << '\n';
        if (redirectToLog)
        {
            std::ofstream fingerprintstream(fingerprint_output_file.c_str());
            fingerprintstream << reply;
            fingerprintstream.flush();
            fingerprintstream.close();
        }
        return (rc ? 0 : -1);
    } catch (std::exception& e) {
        std::cout << AT_LOC << "Exception: " << e.what() << "\n";
    }
    return -1;
}