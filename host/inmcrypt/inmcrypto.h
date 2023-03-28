
/// \file  inmcrypto.h
///
/// \brief for encrypting and decrypting sensitive information like user name and passwords
///
/// note: this class serves as replacement for RX admin/web/Cryptor.php.
///       the functionality is retained as is to maintain backward compatibility
///

#ifndef INM_CRYPTO_H
#define INM_CRYPTO_H

#include <openssl/evp.h>
#include <openssl/md5.h>

#include <time.h>

#include <string.h>
#include <string>
#include <vector>
#include <iostream>
#include <stdio.h>

#include "inmsafeint.h"
#include "inmageex.h"
#if 0
// boost/locale.hpp in available in boost_1_55_0
// enable this block when we upgrade boost to 1.55.0 version or higher
#include <boost/locale.hpp>

#endif

#include "inmcryptotraits.h"
#include "inmbase64.h"


template < typename CRYPTO_TRAITS >
class inmcrypto
{
public:
    inmcrypto() {}
    ~inmcrypto() {}

    std::string encrypt(const std::string & key, const std::string & plain_text)
    {
        if(plain_text.empty()) 
            return plain_text;


        EVP_CIPHER_CTX inmctx;
        int key_len  = 0;
        int block_size = 0;

        std::string utf8_key ;
        std::string iv;
        std::string utf8_plain_text;


        // convert into utf-8
        utf8_key = to_utf8(CRYPTO_TRAITS::source_charset(), key);
        utf8_plain_text = to_utf8(CRYPTO_TRAITS::source_charset(), plain_text);

        if(CRYPTO_TRAITS::usehash())
            utf8_key = hash(utf8_key);


        EVP_CIPHER_CTX_init(&inmctx);
        EVP_CipherInit_ex(&inmctx, CRYPTO_TRAITS::cipher(), NULL, NULL, NULL, true);
        key_len = EVP_CIPHER_CTX_key_length(&inmctx);
        block_size = EVP_CIPHER_CTX_block_size(&inmctx);


        // truncate or append to make input key length as cipher key length
        if(utf8_key.size() > key_len)
            utf8_key = utf8_key.substr(0,key_len);
        else
            utf8_key.append(utf8_key,0, key_len - utf8_key.size());

        // add pad bytes to make input as multiple of cipher block size
        int rem_len = utf8_plain_text.size()%block_size;
        for(int i = rem_len; i < block_size; i++) {
            utf8_plain_text += (block_size - rem_len);
        }

        // generate iv based on cipher
        generate_iv(inmctx, iv);

        int bytes_written = 0;
        int encrypted_len;
        INM_SAFE_ARITHMETIC(encrypted_len = InmSafeInt<size_t>::Type(utf8_plain_text.size()) + block_size - 1, INMAGE_EX(utf8_plain_text.size())(block_size))
        size_t buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<int>::Type(encrypted_len)+1, INMAGE_EX(encrypted_len))
        std::vector<unsigned char> buffer(buflen);


        EVP_CipherInit_ex(&inmctx, NULL, NULL, (const unsigned char*)utf8_key.c_str(),
            (const unsigned char*)iv.c_str(),true);

        EVP_CIPHER_CTX_set_padding(&inmctx, 0);
        EVP_CipherUpdate(&inmctx, &buffer[0], &bytes_written,
            (const unsigned char*) utf8_plain_text.c_str(),utf8_plain_text.size());

        encrypted_len = bytes_written;
        EVP_CipherFinal_ex(&inmctx, &buffer[0]+bytes_written,
            &bytes_written);
        INM_SAFE_ARITHMETIC(encrypted_len += InmSafeInt<int>::Type(bytes_written), INMAGE_EX(encrypted_len)(bytes_written))
        EVP_CIPHER_CTX_cleanup(&inmctx);
        buffer[encrypted_len] = '\0';

        debugprint_hexvalue(&buffer[0], encrypted_len);

        size_t out_len = 0;
        return inmbase64<inm_base64traits>::to_base64(&buffer[0], encrypted_len, out_len).get();
    }

    std::string decrypt(const std::string & key, const std::string & encrypted_text)
    {

        if(encrypted_text.empty()) 
            return encrypted_text;


        EVP_CIPHER_CTX inmctx;
        int key_len  = 0;
        int block_size = 0;

        std::string utf8_key ;
        std::string iv;

        // convert into utf-8
        utf8_key = to_utf8(CRYPTO_TRAITS::source_charset(), key);

        if(CRYPTO_TRAITS::usehash())
            utf8_key = hash(utf8_key);

        // encrypted text is base64 encoded
        // convert to binary encoding
        size_t encrypted_len_binary = 0;
        std::auto_ptr<unsigned char> binary_text = 
            inmbase64<inm_base64traits>::to_binary(encrypted_text.c_str(), encrypted_text.size(), encrypted_len_binary);
        debugprint_hexvalue(binary_text.get(), encrypted_len_binary);

        EVP_CIPHER_CTX_init(&inmctx);
        EVP_CipherInit_ex(&inmctx, CRYPTO_TRAITS::cipher(), NULL, NULL, NULL, false);
        key_len = EVP_CIPHER_CTX_key_length(&inmctx);
        block_size = EVP_CIPHER_CTX_block_size(&inmctx);

        // truncate or append to make input key length as cipher key length
        if(utf8_key.size() > key_len)
            utf8_key = utf8_key.substr(0,key_len);
        else
            utf8_key.append(utf8_key,0, key_len - utf8_key.size());


        // generate iv based on cipher
        generate_iv(inmctx, iv);

        int bytes_written = 0;
        int decrypted_len = encrypted_len_binary + block_size - 1; 
        std::vector<char> buffer(decrypted_len+1);

        EVP_CipherInit_ex(&inmctx, NULL, NULL, (const unsigned char*)utf8_key.c_str(),
            (const unsigned char*)iv.c_str(),false);

        EVP_CIPHER_CTX_set_padding(&inmctx, 0);
        EVP_CipherUpdate(&inmctx, (unsigned char*)&buffer[0], &bytes_written,
            binary_text.get(),encrypted_len_binary);

        decrypted_len = bytes_written;
        EVP_CipherFinal_ex(&inmctx, (unsigned char*)&buffer[0]+bytes_written,
            &bytes_written);
        decrypted_len += bytes_written;
        buffer[decrypted_len] = '\0';

        std::string utf8_plain_text = strip(inmctx, &buffer[0]);
        EVP_CIPHER_CTX_cleanup(&inmctx);
        debugprint_hexvalue((const unsigned char*)utf8_plain_text.c_str(), utf8_plain_text.size()-1);
        return from_utf8(CRYPTO_TRAITS::source_charset(), utf8_plain_text);
    }

protected:

private:

    std::string to_utf8(const std::string & from_charset, const std::string & input)
    {
        // enable this block when we upgrade boost library to 1_55_0 or higher version
        //return boost::locale::to_utf<char>(input,from_charset); 
        return input;
    }

    std::string from_utf8(const std::string & to_charset, const std::string & input)
    {
        // enable this block when we upgrade boost library to 1_55_0 or higher version
        //return boost::locale::from_utf<char>(input,to_charset); 
        return input;
    }

    std::string hash(const std::string & input)
    {
        unsigned char inmmd5buf[MD5_DIGEST_LENGTH];
        MD5((const unsigned char*)input.c_str(), input.size(),inmmd5buf);
        return (const char*)inmmd5buf;
    }

    void generate_iv(EVP_CIPHER_CTX & inmctx, std::string & iv)
    {
        unsigned int inmseed = time(0);
        srand(inmseed);
        for(int i =0 ; i < EVP_CIPHER_CTX_iv_length(&inmctx); i++) {
            iv += rand();
        }
    }

    std::string strip(EVP_CIPHER_CTX & inmctx, const std::string & input)
    {
        int block_size = EVP_CIPHER_CTX_block_size(&inmctx);
        int input_len = input.size();
        int pad_byte = input[input_len -1];
        if(pad_byte < block_size) {
            for(int i = input_len - 1; i >= input_len - pad_byte; i--) {
                if(input[i] != pad_byte) {
                    pad_byte = 0;
                }
            }
        }

        return input.substr(0, input_len -pad_byte);
    }

    void debugprint_hexvalue(const unsigned char *input, size_t input_len)
    {
#ifdef DEBUG
        for(int i = 0; i < input_len; i++) 
            printf("%02x", input[i]);	
        std::cout << std::endl;
#endif		
    }



};


#endif
