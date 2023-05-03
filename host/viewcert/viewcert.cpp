
///
/// \file viewcert.cpp
///
/// \brief
///

#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>

#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/bio.h>

#include <boost/date_time/posix_time/posix_time.hpp>

#include "fingerprintmgr.h"

#define REPORT_ERROR(xStrToWrite)                   \
    do {                                            \
        std::cerr << xStrToWrite;                   \
        ERR_print_errors_fp(stderr);                \
        std::cerr << std::endl;                     \
    } while (false)

securitylib::FingerprintMgr g_fingerprintMgs(false);

int main(int argc, char* argv[])
{
    if (2 != argc) {
        std::cout << "usage: viewcert <cert in pem format>\n\n";
        return -1;
    }

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    BIO* bio = BIO_new_file(argv[1], "r");
    if (0 == bio) {
        REPORT_ERROR("Error opening " << argv[1]);
        return -1;
    }

    X509* x509;
    x509 = PEM_read_bio_X509(bio, NULL, 0, NULL);
    if (0 == x509) {
        REPORT_ERROR("Error reading " << argv[1]);
        return -1;
    }

    std::cout << "\nFingerprints:\n"
              << "  SHA1: " << g_fingerprintMgs.getFingerprint(x509, EVP_sha1(), ' ') << '\n'
              << "   MD5: " << g_fingerprintMgs.getFingerprint(x509, EVP_md5(), ' ') << '\n'
              << '\n';
    X509_print_fp(stdout, x509);
    if (X509_cmp_time(X509_get_notAfter(x509), 0) < 0) {
        std::cout << "\n***** Certificate is expired *****\n";
    }
    std::cout << "\n";
    return 0;
}
