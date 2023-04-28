//
// generate.h: encrypts licenses for CX server
//
#ifndef GENERATE__H
#define GENERATE__H

#include "keys/privatekey.h"

class CGenerate
{
    FILE* fp;
    long fsize, csize, msg_chunks;
    unsigned char* plain_text;
    unsigned char* cipher_text;
    unsigned char* msg_text;
    char* pukey; 
    char* prkey;
    RSA* rsa;

public:
    CGenerate()
    {
        fp = NULL;
        fsize = csize = msg_chunks = 0;
        plain_text = cipher_text = msg_text = NULL;
        prkey = pukey = NULL;
        rsa = RSA_new();
    }
    ~CGenerate()
    {
        FreeResources();
    }

    void ReadLicense(char* fname);
    void GenerateKeyPair(const char* fname, const char* prname, const char* puname);
    void InitializeKey();
    void EncryptLicense();
    void WriteLicense(char* fname);
    void FreeResources();
};

#endif // GENERATE__H
