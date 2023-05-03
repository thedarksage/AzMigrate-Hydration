//
// license.h: decrypts licenses for CX server
//
#ifndef LICENSE__H
#define LICENSE__H
#include "svtypes.h"
#include <string>

class CLicense
{
public:
    CLicense()
    {
        rsa = RSA_new();
        fp = 0;
        csize = fsize = msg_chunks = 0;
        cipher_text = plain_text = sign_text = NULL;
    }
    ~CLicense()
    {
        FreeResources();
    }

    void ReadLicense(const char* fname);
    void InitializeKey();
    unsigned char* DecryptLicense();
    SVERROR ParseLicense(const char* options);
    SVERROR ParseLicense(const char* options, const char* id, const char* property);
    const char* GetParserResult();

private:
    void FreeResources();

    FILE* fp;
    long csize, fsize, msg_chunks;
    unsigned char* plain_text;
    unsigned char* cipher_text;
    unsigned char* sign_text;
    RSA* rsa;
    BIGNUM* modulus;
    BIGNUM* exponent;
    std::string parser_result;
};

#ifdef WIN32
#else
typedef struct
{
    unsigned char byte[6];
} mac_t;

typedef struct
{
    int sock;
    struct ifreq dev;
} net_info_t;

net_info_t * mc_net_info_new (const char *device);
mac_t * mc_net_info_get_mac (const net_info_t *);
void mc_mac_into_string (const mac_t *mac, char *s, size_t mac_string_size);
void get_mac_address (const char* device_name, char* mac_string, size_t mac_string_size);
#endif // ifdef WIN32

#endif // LICENSE__H
