
#include <iostream>
#include <stdio.h>
#include <time.h>

#include "gencert.h"
#include "terminateonheapcorruption.h"

std::string g_name;
std::string g_extractPfx;
std::string g_pfxKey;
std::string g_country;
std::string g_state;
std::string g_locality;
std::string g_org;
std::string g_orgUnit;
std::string g_commonName;
std::string g_expiresInDays;
std::string g_outDir;

bool g_importPfx = false;
bool g_generateDh = false;
bool g_overwrite = false;

bool parseCmdline(int argc,
    char* argv[],
    securitylib::CertData& certData)
{
    certData.m_expiresInDays = 0;
    certData.m_prompt = false;
    bool showHelp = false;
    bool argsOk = true;
    int i = 1;
    while (i < argc) {
        if ('-' != argv[i][0]) {
            REPORT_ERROR("*** invalid option: " << argv[i]);
            argsOk = false;
        }
        else if ('\0' == argv[i][1]) {
            REPORT_ERROR("*** invalid option: " << argv[i]);
            argsOk = false;
        }
        else if ('-' == argv[i][1]) {
            // long options
            if (boost::algorithm::equals("--dh", argv[i])) {
                g_generateDh = true;
#ifdef CA_SIGN_ENABLED
            } else if (boost::algorithm::equals("--cacert", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    REPORT_ERROR("*** missing value for --cacert");
                    argsOk = false;
                } else {
                    certData.m_caCert = argv[i];
                }
            } else if (boost::algorithm::equals("--cakey", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    REPORT_ERROR("*** missing value for --cakey");
                    argsOk = false;
                } else {
                    certData.m_caKey = argv[i];
                }
#endif
            }
            else if (boost::algorithm::equals("--ST", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    REPORT_ERROR("*** missing value for --ST");
                    argsOk = false;
                }
                else {
                    certData.m_stateOrProvince = argv[i];
                }
            }
            else if (boost::algorithm::equals("--OU", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    REPORT_ERROR("*** missing value for --OU");
                    argsOk = false;
                }
                else {
                    certData.m_unit = argv[i];
                }
            }
            else if (boost::algorithm::equals("--CN", argv[i])) {
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    REPORT_ERROR("*** missing value for --CN");
                    argsOk = false;
                }
                else {
                    certData.m_commonName = argv[i];
                }
            }
            else {
                REPORT_ERROR("*** Invalid command line option: " << argv[i]);
                argsOk = false;
            }
        }
        else {
            // short option
            switch (argv[i][1]) {
            case 'C':
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    REPORT_ERROR("*** missing value for -C");
                    argsOk = false;
                }
                else {
                    certData.m_country = argv[i];
                }
                break;
            case 'E':
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    REPORT_ERROR("*** missing value for -E");
                    argsOk = false;
                }
                else {
                    try {
                        certData.m_expiresInDays = boost::lexical_cast<long>(argv[i]);
                    }
                    catch (...) {
                        REPORT_ERROR("*** invalid value for -E: '" << argv[i] << "'. Must be a valid number");
                        argsOk = false;
                    }
                }
                break;
            case 'L':
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    REPORT_ERROR("*** missing value for -L");
                    argsOk = false;
                }
                else {
                    certData.m_locality = argv[i];
                }
                break;
            case 'd':
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    REPORT_ERROR("*** missing value for -d");
                    argsOk = false;
                }
                else {
                    g_outDir = argv[i];
                }
                break;
            case 'O':
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    REPORT_ERROR("*** missing value for -O");
                    argsOk = false;
                }
                else {
                    certData.m_organization = argv[i];
                }
                break;
            case 'h':
                showHelp = true;
                break;
            case 'i':
                g_importPfx = true;
                break;
            case 'k':
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    REPORT_ERROR("*** missing value for -k");
                    argsOk = false;
                }
                else {
                    g_pfxKey = argv[i];
                }
                break;

            case 'o':
                g_overwrite = true;
                break;
            case 'n':
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    REPORT_ERROR("*** missing value for -o");
                    argsOk = false;
                }
                else {
                    g_name = argv[i];
                }
                break;
            case 'p':
                certData.m_prompt = true;
                break;
            case 'x':
                ++i;
                if (i >= argc || '-' == argv[i][0]) {
                    --i;
                    REPORT_ERROR("*** missing value for -x");
                    argsOk = false;
                }
                else {
                    g_extractPfx = argv[i];
                }
                break;
            default:
                REPORT_ERROR("*** Invalid command line option: " << argv[i]);
                argsOk = false;
            }
        }
        ++i;
    }

    if (showHelp) {
        return false;
    }
#ifdef CA_SIGN_ENABLED
    if (certData.m_caKey.empty() && !certData.m_caCert.empty()) {
        REPORT_ERROR("*** Invalid options: --cakey required with --cacert.\n"
            << "You must provide either both --cakey and --cacert or neither");
        argsOk = false;
    } else if (!certData.m_caKey.empty() && certData.m_caCert.empty()) {
        REPORT_ERROR("*** Invalid options: --cakcert required with --cackey.\n"
            << "You must provide either both --cakey and --cacert or neither");
        argsOk = false;
    }
#endif
    if (g_name.empty() && g_extractPfx.empty()) {
        if (!securitylib::getNodeName(g_name)) {
            REPORT_ERROR("*** Error getting node name. If this persists, use -n option to over ride nodename");
            argsOk = false;
        }
    }

    if (!g_name.empty() && !g_extractPfx.empty()) {
        REPORT_ERROR("*** Error can not specify both -n and -x");
        argsOk = false;
    }

    return argsOk;
}

void usage()
{

#ifdef CA_SIGN_ENABLED
    std::cout << "\nusage: gencert [-n <name>] [--dh] [-i] [-d <out dir>] [-o] [--cakey <cakeyfile> --cacert <cacertfile>] [cert values] [-p]\n"
        << "         gencert -x <pfx> [-k <passphrase>]\n"
        << "         gencert -h\n"
#else
    std::cout << "\nusage: gencert [-n <name>] [--dh] [-i] [-d <out dir>] [-o] [cert values] [-p]\n"
        << "       gencert -x <pfx> [-k <passphrase>]\n"
        << "       gencert -h\n"
#endif
        << "\n"
        << "The first command line generates the requested files.\n"
        << "  The names of the files are (note if you use the -d option that directory will be used instead):\n"
        << "    private key file          : " << securitylib::getPrivateDir() << "/<name>" << securitylib::EXTENSION_KEY << '\n'
        << "    certificate file          : " << securitylib::getCertDir() << "/<name>" << securitylib::EXTENSION_CRT << '\n'
        << "    Diffie-Hellman file       : " << securitylib::getPrivateDir() << "/<name>" << securitylib::EXTENSION_DH << '\n'
        << "    fingerprint file          : " << securitylib::getFingerprintDir() << "/<name>" << securitylib::EXTENSION_FINGERPRINT << '\n'
        << "    key and cert in pfx format: " << securitylib::getPrivateDir() << "/<name>" << securitylib::EXTENSION_KEYCERT_PFX << '\n'
        << "\n\n"
        << "   -n  <name>         : name used for all file names. This is also used to create a friendly name if importing into local cert store. Defaults to hostname. Optional\n"
        << "  --dh                : generate Diffle-Hellman key pair. Optional\n"
        << "   -i                 : import key and cert into local certificate store (only valid on windows). Optional\n"
        << "   -d  <out dir>      : directory to write files in. Default (see file names above). Optional\n"
        << "   -o                 : overwrite file if it exists. Default create backup of old file. Optional\n"
#ifdef CA_SIGN_ENABLED
        << "  --cakey <cakeyfile> : name of file that contains the CA private\n"
        << "                        key to use when signing the certificate. Optional\n"
        << "  --cacet <cacertfile>: name of file that contains the CA certificate\n"
        << "                        to use when signing the certificate. Optional\n"
        << "\n"
        << "  NOTE: both --cakey and --cacert are optional, but if one is supplied the other is required\n"
#endif
        << "\n"
        << "  The following are options for the certificate values. These are all optional. \n"
        << "  If you use -p, you will be prompted for any values not provided:\n"
        << "    -C  <country/region code>          : 2 letter country/region code Default US\n"
        << "   --ST <state/province>        : State or Province name> Default Washingtion\n"
        << "    -L  <locality name>         : locality (city) name. Default Redmond\n"
        << "    -O  <organization>          : organization (company) name. Default Microsoft\n"
        << "   --OU <organizationl unit name: organizational unit name. Default InMage\n"
        << "   --CN <common name>           : your name or FQDN that certificate will be issued to. \n"
        << "                                  (ip, domain name, or any name). Default Scout\n"
        << "    -E  <expires in days>       : number of days until the certificate will expire. Default 1095 (3 years)\n"
        << "\n"
        << "   -p                           : prompt for certificate values not provided on command line instead of using defaults\n"
        << "\n\n"
        << "The second command line extracts private key from a pfx file and writes it to stdout in PEM format\n"
        << "  -x <pfx>       : Extract private key from <pfx> file and write to stdout in PEM format\n"
        << "  -k <passphrase>: passphrase (key) needed if pfx is passphrase protected. Optional\n"
        << "\n\n"
        << "The last command line shows this help message\n"
        << "  -h : show usage\n"
        << "\n";
}

int main(int argc, char ** argv)
{
    TerminateOnHeapCorruption();

    try
    {
        OpenSSL_add_all_algorithms();
        ERR_load_crypto_strings();

        // MAYBE: if any platfrom gets "RNG not seeded" error message,
        // then platform does not support automatic seeding and it will
        // require code to seed it. basically will need to know how to
        // get good random data for the platform that is getting the error

        securitylib::CertData certData;
        if (!parseCmdline(argc, argv, certData)) {
            usage();
            return -1;
        }
        certData.updateCertData();

        securitylib::GenCert genCert(g_name,
            certData,
            true,
            g_generateDh,
            g_importPfx,
            g_extractPfx,
            g_pfxKey,
            g_overwrite,
            g_outDir);
        if (!g_extractPfx.empty()) {
            if (g_pfxKey.empty()) {
                g_pfxKey = securitylib::PFX_PASSPHRASE;
            }
            genCert.extractPfxPrivateKey(g_extractPfx, g_pfxKey);
            return 0;
        }

#ifdef CA_SIGN_ENABLED
        if (!certData.m_caKey.empty()) {
            caSignReq();
        }
        else {
#endif
            genCert.selfSignReq();
            if (g_generateDh) {
                genCert.generateDiffieHellman(argv[0]);
            }
#ifdef CA_SIGN_ENABLED
        }
#endif
        if (g_importPfx) {
            securitylib::importPfxCert(genCert.getPfxName(), securitylib::PFX_PASSPHRASE);
        }
    }
    catch (std::exception &e)
    {
        std::cerr << "Error: " << e.what();
        return -1;
    }
    return 0;
}

