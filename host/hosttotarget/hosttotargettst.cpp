
///
/// \file hosttotargettst.cpp
///
/// \brief example showing how to use Host to Target library
///

#ifndef HOSTTOTARGETTST_CPP
#define HOSTTOTARGETTST_CPP

#include <iostream>
#include <string>

#include <boost/algorithm/string.hpp>

#include "targetupdater.h"

HostToTarget::OsTypes g_osType = HostToTarget::OS_TYPE_NOT_SUPPORTED;   ///< os type entered on the command line

std::string g_installDir;   ///< install directory entered on the command line
std::string g_systemVolume; ///< system volume entered on the command line

HostToTarget::TargetTypes g_targetType;  ///< the target type entered on the command line

/// \ brief hsost to target tester usage
void usage()
{
    std::cout << "usage: hosttotarget-tst -o osName -i installDir -t target -s systemVol [-h]\n"
              << "\n"
              << "  -o osType    : os type\n"
              << "                 can be one of\n"
              << "                    w2k3_32\n"
              << "                    w2k3_64\n"
              << "                    w2k8_32\n"
              << "                    w2k8_64\n"
              << "                    w2k8_r2\n"
              << "  -i installDir: directory where product is installed\n"
              << "  -t target    : target name (vmware, xen, hypervisor, kvm or physical).\n"
              << "                   Note only needs to be the unique part of the target name.\n"
              <<"                    (e.g. v, vm, vmw, etc. would all work for vmware)\n"
              << "  -s systemVol : volume that has the WINDOWS directory\n"
              << "\n"
              << "  -h: display this help\n"
              << std::endl;
}

/// \brief holds target name to target type mapping
struct TargetTypeNames {
    char* m_name;                       ///< target name
    HostToTarget::TargetTypes m_type;   ///< target type
};

/// \brief target name to target type lookup
TargetTypeNames g_targetTypeNames [] =
{
    { "vmware", HostToTarget::HOST_TO_VMWARE },
    { "xen", HostToTarget::HOST_TO_VMWARE },
    { "hypervisor",  HostToTarget::HOST_TO_VMWARE },
    { "kvm", HostToTarget::HOST_TO_VMWARE },
    { "physical", HostToTarget::HOST_TO_VMWARE },
    { 0, HostToTarget::HOST_TO_UNKNOWN }
};

/// \brief gets the target type for the given target name
HostToTarget::TargetTypes getTargetType(char const* targetName) ///< targetName to look up target type
{
    for (int i = 0; 0 != g_targetTypeNames[i].m_name; ++i) {
        if (boost::algorithm::istarts_with(g_targetTypeNames[i].m_name, targetName)) {
            return g_targetTypeNames[i].m_type;
        }
    }
    return HostToTarget::HOST_TO_UNKNOWN;
}

/// \brief parses the command line options
bool parseCmd(
    int argc,                     ///< number of command line arguments
    char* argv[])                 ///< command line arguments
{
    bool argsOk = true;
    bool printHelp = false;

    if (1 == argc) {
        usage();
        return false;
    }

    int i = 1;
    while (i < argc) {
        if ('-' != argv[i][0]) {
            std::cout << "*** invalid option: " << argv[i] << '\n';
            argsOk = false;
        } else if ('\0' == argv[i][1] || '\0' != argv[i][2]) {
            std::cout << "*** invalid option: " << argv[i] << '\n';
            argsOk = false;
        } else {
            switch (argv[i][1]) {
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
                    g_installDir = argv[i];
                    break;

                case 'o':
                    ++i;
                    if (i >= argc || '-' == argv[i][0]) {
                        --i;
                        std::cout << "*** missing value for -o\n";
                        argsOk = false;
                        break;
                    }
                    if (boost::algorithm::equals("w2k3_32", argv[i])) {
                        g_osType = HostToTarget::OS_TYPE_W2K3_32;
                    } else if (boost::algorithm::equals("w2k3_64", argv[i])) {
                        g_osType = HostToTarget::OS_TYPE_W2K3_64;
                    } else if (boost::algorithm::equals("w2k8_32", argv[i])) {
                        g_osType = HostToTarget::OS_TYPE_W2K8_32;
                    } else if (boost::algorithm::equals("w2k8_64", argv[i])) {
                        g_osType = HostToTarget::OS_TYPE_W2K8_64;
                    } else if (boost::algorithm::equals("w2k8_r2", argv[i])) {
                        g_osType = HostToTarget::OS_TYPE_W2K8_R2;
                    } else if (boost::algorithm::equals("win8", argv[i])) {
                        g_osType = HostToTarget::OS_TYPE_WIN8;
                    } else {
                        std::cout << "*** invalid os type: " << argv[i] << '\n';
                        argsOk = false;
                    }
                    break;

                case 's':
                    ++i;
                    if (i >= argc || '-' == argv[i][0]) {
                        --i;
                        std::cout << "*** missing value for -s\n";
                        argsOk = false;
                        break;
                    }
                    g_systemVolume = argv[i];
                    break;

                case 't':
                    ++i;
                    if (i >= argc || '-' == argv[i][0]) {
                        --i;
                        std::cout << "*** missing value for -t\n";
                        argsOk = false;
                        break;
                    }

                    g_targetType = getTargetType(argv[i]);
                    break;

                default:
                    std::cout << "*** invalid arg: " << argv[i] << "\n";
                    argsOk = false;
                    break;
            }
        }
        ++i;
    }

    if (printHelp) {
        usage();
        return false;
    }

    if (HostToTarget::OS_TYPE_NOT_SUPPORTED == g_osType) {
        std::cout << "*** missing or invalie os type\n";
        argsOk = false;
    }

    if (g_installDir.empty()) {
        std::cout << "*** missing -i <installDir>\n";
        argsOk = false;
    }

    if (-1 == g_targetType) {
        std::cout << "*** missing or invalid -v <targetName>\n";
        argsOk = false;
    }

    if (g_systemVolume.empty()) {
        std::cout << "*** missing -s <systemlDir>\n";
        argsOk = false;
    }

    if (!argsOk) {
        usage();
    }

    return argsOk;
}

/// \brief main
int main(int argc, char* argv[])
{
    if (!parseCmd(argc, argv)) {
        return -1;
    }
    try {
        // need the try block as TargetUpdater constructor will throw on error
        HostToTarget::HostToTargetInfo info(g_targetType, g_osType, g_installDir, g_systemVolume);
        HostToTarget::TargetUpdater targetUpdater(info);
        if (!targetUpdater.update(HostToTarget::UPDATE_OTPION_MANUAL_START_SERVICES | HostToTarget::UPDATE_OPTION_CLEAR_CONFIG_SETTINGS)) {
            std::cout  << targetUpdater.getErrorAsString() << '\n';
        }
        return (targetUpdater.ok() ? 0 : -1);
    } catch (ErrorException const& e) {
        std::cout << e.what() << '\n';
    }
    return -1;
}


#endif // HOSTTOTARGETTST_CPP
