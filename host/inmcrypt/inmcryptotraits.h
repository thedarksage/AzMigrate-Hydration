/// \file  inmcryptotraits.h
///
/// \brief various traits for the encryption/decryption class
///
///

#ifndef INM_CRYPTO_TRAITS_H
#define INM_CRYPTO_TRAITS_H

#include <string>
#include <openssl/evp.h>



class inmcrypto_default_traits
{
public:
    static const EVP_CIPHER * cipher() { return EVP_des_ede_ecb() ; }
    static std::string source_charset() { return "ISO-8859-1" ; }
    static bool usehash()        { return true; }
};

#endif
