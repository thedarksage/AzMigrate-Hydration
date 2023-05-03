#include <iostream>
#include <string>
#include <fstream>

#include <boost/filesystem/operations.hpp>
#include <boost/lexical_cast.hpp>

#include "securityutils.h"
#include "defaultdirs.h"
#include "createpaths.h"
#include "setpermissions.h"
#include "genpassphrase.h"
#include "terminateonheapcorruption.h"

#ifdef SV_WINDOWS
#include "svtypes.h"
#include <Windows.h>
#endif

bool parseCmdline(int argc,
    char* argv[],
    bool& backup,
    bool& overwrite,
    int& length,
    std::string& outDir,
    securitylib::RANGE& range,
    bool& displayOnly,
    std::string& passphraseToWrite,
    bool& viewCurrent,
    bool& genEncryptionKey,
    bool& keyReadable)
{
    keyReadable = false;
    genEncryptionKey = false;
    displayOnly = false;
    range = securitylib::RANGE_CHARS_NUMBERS;
    backup = false;
    overwrite = false;
    length = 0;
    bool showHelp = false;
    bool argsOk = true;
    int i = 1;
    while (i < argc) {
        if ('-' != argv[i][0]) {
            std::cout << "*** invalid option: " << argv[i] << '\n';
            argsOk = false;
        }
        else if ('\0' == argv[i][1]) {
            std::cout << "*** invalid option: " << argv[i] << '\n';
            argsOk = false;
        }
        else if ('-' == argv[i][1]) {
            // long options
            // no long options
            std::cout << "*** invalid option: " << argv[i] << '\n';
            argsOk = false;
        }
        else {
            // short option
            switch (argv[i][1]) {
            case 'b':
                backup = true;
                break;
            case 'd':
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for -d\n";
                    argsOk = false;
                }
                else {
                    outDir = argv[i];
                }
                break;
            case 'k':
                genEncryptionKey = true;
                break;
            case 'l':
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for -l\n";
                    argsOk = false;
                }
                else {
                    try {
                        length = boost::lexical_cast<long>(argv[i]);
                    }
                    catch (...) {
                        std::cout << "*** invalid value for -l: '" << argv[i] << "'. Must be a valid number\n";
                        argsOk = false;
                    }
                }
                break;
            case 'o':
                overwrite = true;
                break;
            case 'p':
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    std::cout << "*** missing value for -p\n";
                    argsOk = false;
                }
                else {
                    passphraseToWrite = argv[i];
                }
                break;
            case 's':
                range = securitylib::RANGE_CHARS_NUMBERS_SYMBOLS;
                break;
            case 'n':
                displayOnly = true;
                break;
            case 'r':
                keyReadable = true;
                break;
            case 'v':
                viewCurrent = true;
                break;
            case 'h':
                showHelp = true;
                break;
            default:
                std::cout << "*** Invalid command line option: " << argv[i] << '\n';
                argsOk = false;
            }
        }
        ++i;
    }

    if (showHelp) {
        return false;
    }

    if (0 == length) {
        if (genEncryptionKey) {
            length = securitylib::MIN_ENCRYPT_KEY_LENGTH;
        }
        else {
            length = securitylib::MIN_PASSPHRASE_LENGTH;
        }
    }

    if (length < securitylib::MIN_PASSPHRASE_LENGTH || (genEncryptionKey && length < securitylib::MIN_ENCRYPT_KEY_LENGTH)) {
        std::cout << "*** Invalid length " << length << " for -l option. Must be >= ";
        if (genEncryptionKey) {
            std::cout << securitylib::MIN_ENCRYPT_KEY_LENGTH << " when using -k \n";
        }
        else {
            std::cout << securitylib::MIN_PASSPHRASE_LENGTH << '\n';
        }
        argsOk = false;
    }

    if (genEncryptionKey) {
        range = securitylib::RANGE_FULL;
    }

    return argsOk;
}

void usage()
{
    std::cout << "usage: genpassphrase [options] | -h\n"
        << "\n"
        << "generates a random passphrase\n"
        << "\n"
        << "options (all are optional): \n"
        << "  -b              : backup existing passphrase and generate a new one. Default is error if passphrase file exists\n"
        << "  -o              : overwrite passphrase file if it exists. Default error if passphrase file exists\n"
        << "  -l <len>        : length of the passphrase to genreate. Must be >= " << securitylib::MIN_PASSPHRASE_LENGTH << ". Default is " << securitylib::MIN_PASSPHRASE_LENGTH << '\n'
        << "  -d <out dir>    : directory to write in. Default " << securitylib::getPrivateDir() << ". Optional\n"
        << "  -s              : include printable sysmbols . Default nonly letters and numbers. Optional\n"
        << "  -k              : generate an encryption key. Default length " << securitylib::MIN_ENCRYPT_KEY_LENGTH << " bytes (" << securitylib::MIN_ENCRYPT_KEY_LENGTH * 8 << " bits)\n"
        << "                    This will be binary bytes between 0 - 256. Optional\n"
        << "  -v              : view current passphrase. Use -k option to view current encryption key. Optional\n"
        << "  -r              : when ued with -k outputs key readable hex digits. Default is output binary\n"
        << "  -p <passphrase> : use <passphrase> instead of generating one. Optional\n"
        << "  -n              : display passphrase after generating but do not write to file. Optional\n"
        << "\n"
        << "  -h: display this help\n"
        << std::endl;
}


#ifdef SV_WINDOWS
bool executePreAndPostScript(std::string &prePostScriptName, std::string &cmdArg, DWORD &exitCode)
{
    if (!boost::filesystem::exists(prePostScriptName))
    {
        std::cout << prePostScriptName << " file not present" << std::endl;
        return false;
    }
    STARTUPINFO StartupInfo;
    PROCESS_INFORMATION ProcessInformation;
    DWORD timeout = 900000;
    bool bExitState = false;
    memset(&StartupInfo, 0x00, sizeof(STARTUPINFO));
    StartupInfo.cb = sizeof(STARTUPINFO);
    memset(&ProcessInformation, 0x00, sizeof(PROCESS_INFORMATION));
    std::string powershellExe = "powershell.exe";
    std::string prepostscriptCmd = powershellExe + " -file " + prePostScriptName + " " + cmdArg;

    if (!::CreateProcess(NULL, const_cast<char*>(prepostscriptCmd.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &StartupInfo, &ProcessInformation))
    {
        std::cout << "CreateProcess failed with Error: " << GetLastError() << std::endl;
        // Get the exit code.
        bExitState = GetExitCodeProcess(ProcessInformation.hProcess, &exitCode);
        std::cout << "Exit Code: " << exitCode << std::endl;
        return false;
    }

    // Successfully created the process.  Wait for it to finish.
    DWORD retValue = WaitForSingleObject(ProcessInformation.hProcess, timeout);
    if (retValue == WAIT_FAILED || retValue == WAIT_TIMEOUT)
    {
        std::cout << "WaitForSingleObject has failed. Either Wait failed or Wait timeout exceeded. Exiting " << __FUNCTION__ << std::endl;
        std::cout << "WaitForSingleObject retValue: " << retValue << std::endl;
        ::CloseHandle(ProcessInformation.hProcess);
        ::CloseHandle(ProcessInformation.hThread);
        return false;
    }

    Sleep(100);

    int count = 0;
    do
    {
        // Get the exit code.
        bExitState = GetExitCodeProcess(ProcessInformation.hProcess, &exitCode);
        count++;
    } while (exitCode == STILL_ACTIVE && count < 2);

    ::CloseHandle(ProcessInformation.hProcess);
    ::CloseHandle(ProcessInformation.hThread);

    return bExitState;
}
#endif

int main(int argc, char* argv[])
{
    TerminateOnHeapCorruption();

    if (!securitylib::isRootAdmin()) {
        std::cout << "Must be " << securitylib::ROOT_ADMIN_NAME << " to run this program\n";
        return -1;
    }
    bool ok = false;
    try {
        bool genEncryptionKey = false;
        bool viewCurrent = false;
        securitylib::RANGE range = securitylib::RANGE_CHARS_NUMBERS;
        bool displayOnly = false;
        bool backup = false;
        bool overwrite = false;
        bool backedup = true;
        std::string passphrase;
        int length = 12;
        std::string outDir;
        bool keyReadable = false;
        if (!parseCmdline(argc, argv, backup, overwrite, length, outDir, range, displayOnly, passphrase, viewCurrent, genEncryptionKey, keyReadable)) {
            usage();
            return -1;
        }
        std::string scriptFileName;
        scriptFileName = "PassphraseUpdateHealth.ps1";
        std::string preScriptFailureServicesRestart = " 1) Microsoft Azure Site Recovery Service (DRA)\n 2) World Wide Web Publishing Service (IIS)\n 3) InMage Scout VX Agent - Sentinel/Outpost\n 4) InMage Scout Application Service\n 5) InMage PushInstall\n 6) cxprocessserver\n 7) tmansvc\n 8) processservermonitor\n 9) processserver";
        std::string postScriptFailureServicesRestart = " 1) Microsoft Azure Site Recovery Service (DRA)\n 2) World Wide Web Publishing Service (IIS)";

        std::string fileName(outDir.empty() ? securitylib::getPrivateDir() : outDir);
        if (genEncryptionKey) {
            fileName += securitylib::ENCRYPTION_KEY_FILENAME;
        }
        else {
            fileName += securitylib::CONNECTION_PASSPHRASE_FILENAME;
        }
        if (viewCurrent) {
            if (genEncryptionKey) {
                securitylib::encryptKey_t key = securitylib::getEncryptionKey(fileName);
                securitylib::printEncryptKey(key, keyReadable);
            }
            else {
                std::cout << securitylib::getPassphrase(fileName) << std::endl;
            }
            return 0;
        }
        if (passphrase.empty()) {
            if (genEncryptionKey) {
                securitylib::encryptKey_t key = securitylib::genEncryptKey(length);
                passphrase.assign(&key[0], key.size());
            }
            else {
                passphrase = securitylib::genPassphrase(length, range);
            }
        }

        if (displayOnly) {
            if (genEncryptionKey) {
                securitylib::encryptKey_t key = securitylib::getEncryptionKey(fileName);
                securitylib::printEncryptKey(key, keyReadable);
            }
            else {
                std::cout << securitylib::getPassphrase(fileName) << std::endl;
            }
        }
        else {
            if (!(backup || overwrite) && boost::filesystem::exists(fileName)) {
                std::cout << fileName << " exists. To create a new passphrase use -b or -o option\n";
                return -1;
            }
            else if ((backup || overwrite) && boost::filesystem::exists(fileName))
            {
#ifdef SV_WINDOWS
                char chr;
                std::cout << '\n'
                    << "This operation will disconnect all Site Recovery Infrastructure and source machines from Configuration Server. You will need to re-configure each machine manually to continue replication. Are you sure you want to change the passphrase [y/n]? ";
                std::cin >> chr;
                if (chr != 'y')
                {
                    return -1;
                }

                std::cout << "\nThe passphrase change is in progress and may take up to 10 minutes." << std::endl;
                //Stopping the services before generating a new passphrase
                std::string preCmdArg;
                preCmdArg = "-argsparameter pre";// "pre";
                DWORD preScriptRetValue = 0;

                if (!executePreAndPostScript(scriptFileName, preCmdArg, preScriptRetValue))
                {
                    std::cout << scriptFileName << " execution is failed" << std::endl;
                    std::cout << "Please re-try the operation" << std::endl;
                    return -1;
                }

                if (preScriptRetValue != 1)
                {
                    std::cout << "\nFailed to stop the services." << std::endl;
                    std::cout << "\nPlease re-try the operation. If you do not want to re-try the operation, please check the following services state and start them manually if they are in stopped state." << std::endl;
                    std::cout << preScriptFailureServicesRestart << std::endl;
                    return -1;
                }
#endif
            }

            CreatePaths::createPathsAsNeeded(fileName);
            if (backup) {
                if (!securitylib::backup(fileName)) {
                    char ch;
                    do {
                        std::cout << '\n'
                            << "unable to backup " << fileName << "\nDo you want to overwrite wtih new passphrase [y/n]? ";
                        std::cin >> ch;
                    } while (!('y' == ch || 'n' == ch));
                    if ('y' != ch) {
                        return -1;
                    }
                }
            }

            std::ofstream out;
            if (genEncryptionKey) {
                out.open(fileName.c_str(), std::ofstream::out | std::ofstream::binary);
                out.write(passphrase.data(), passphrase.size());
            }
            else {
                out.open(fileName.c_str());
                out << passphrase;
            }
            if (out.good()) {
                std::cout << "\npassphrase written to " << fileName << std::endl;
                ok = true;
            }
            else {
                std::cout << "\nerror writing passphrase to " << fileName << ": " << errno << std::endl;
            }
            out.close();
#ifdef SV_WINDOWS
            //Starting the services as the passphrase generation completed
            if ((backup || overwrite) && ok)
            {
                std::string postCmdArg;
                postCmdArg = "-argsparameter post";
                DWORD postScriptExitCode = 0;

                if (!executePreAndPostScript(scriptFileName, postCmdArg, postScriptExitCode))
                {
                    std::cout << scriptFileName << " execution failed" << std::endl;
                }

                if (postScriptExitCode != 1)
                {
                    if (postScriptExitCode == 2)
                    {
                        std::cout << "\nFailed to start the services." << std::endl;
                        std::cout << "\nPlease re-try the operation. If you do not want to re-try the operation, please restart the following services manually." << std::endl;
                        std::cout << postScriptFailureServicesRestart << std::endl;
                    }
                    else if (postScriptExitCode == 3)
                    {
                        std::cout << "\nDatabase operation failed while updating passphrase health events." << std::endl;
                        std::cout << "\nPlease re-try the operation." << std::endl;
                    }
                    else
                    {
                        std::cout << "\nPost script exited with unknown value: " << postScriptExitCode << std::endl;
                        std::cout << "\nPlease re-try the operation. If you do not want to re-try the operation, please restart the following services manually." << std::endl;
                        std::cout << postScriptFailureServicesRestart << std::endl;
                    }
                }
            }
            else if (backup || overwrite)
            {
                std::cout << "\nPlease restart the following services manually." << std::endl;
                std::cout << preScriptFailureServicesRestart << std::endl;
            }
#endif
            // FIXME: enable once apache is figured out
            //securitylib::setPermissions(fileName, securitylib::SET_PERMISSION_BOTH);
        }
        securitylib::secureClearPassphrase(passphrase);
    }
    catch (std::exception const& e) {
        std::cout << "Failed: " << e.what() << std::endl;
    }
    return (ok ? 0 : -1);
}
