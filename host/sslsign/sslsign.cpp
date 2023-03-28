
///
/// \file sslsign.cpp
///
/// \brief
///


#include <iostream>
#include <iomanip>
#include <string>

#include "sslsign.h"
#include "securityutils.h"

void usage()
{
    std::cout << "usage: sslsign [-h]\n"
              << '\n'
              << "Reads private key file name and data to be signed from stdin\n"
              << "Use the following format (-k option must be first)\n"
              << "  -k <private-key-file-pem-format> <data-to-sign>\n"
              << '\n'
              << "Returns 0 on succes and writes signature to stdout, non 0 on failure and writes error msg to stdout\n"
              << '\n'
              << "  -h: displays this help\n";
}

bool parseInput(std::string const& input,
                std::string& keyFile,
                std::string& data)
{
    if (input.empty() || input.size() < 6) {
        return false;
    }

    data.clear();
    keyFile.clear();
    if (!('-' == input[0] && 'k' == input[1])) {
        std::cout << "*** Missing -k option\n";
        return false;
    }
    std::string::size_type idx = 2;
    if (' '  != input[idx]) {
        std::cout << "*** Missing private key file name\n";
        return false;
    }
    ++idx;
    std::string::size_type last = input.size();

    //skip any leading spaces
    while (idx < last && ' ' == input[idx]) {
        ++idx;
    }
    char delim = ' ';
    if ('"' == input[idx]) {
        delim = '"';
        ++idx;
    } else if ('\'' == input[idx]) {
        delim = '\'';
        ++idx;
    }
    while (idx < last && delim  != input[idx]) {
        keyFile.append(1, input[idx++]);
    }
    ++idx;
    //skip any leading spaces
    while (idx < last && ' ' == input[idx]) {
        ++idx;
    }
    if (idx == last) {
        std::cout << "*** Missing data to sign\n";
        return false;
    }
    data = input.substr(idx);
    return true;
}


int main(int argc, char* argv[])
{
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    // reads data from stdin
    // exepects -k <private-key-file-name-pem-format> <data to sign>
    std::string input;
    std::string keyFile;
    std::string data;
    std::getline(std::cin, input);
    if (!parseInput(input, keyFile, data)) {
        usage();
        return -1;
    }
    securitylib::signature_t signature;
    std::string msg;
    if (!securitylib::sslsign(msg, keyFile, data, signature)) {
        std::cout << msg << '\n';
        return -1;
    }
    std::cout << securitylib::base64Encode(&signature[0], signature.size());

    return 0;
}
