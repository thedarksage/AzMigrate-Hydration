//
// generate.cpp: implements license generation functions
//
#include <string.h>
#include "common.h"
#include "generate.h"
#include "inmsafecapis.h"
//
// read contents of plain text license file
//
void CGenerate::ReadLicense(char* fname)
{
    fp = fopen (fname, "rb");
    if (fp != NULL)
    {
        fseek (fp, 0, SEEK_END);
        fsize = ftell (fp);
        rewind (fp);
        fsize++;
        plain_text = (unsigned char*) malloc ((fsize) * sizeof(char));
        if (plain_text != NULL)
        {
            fread (plain_text, 1, fsize, fp);
            *(plain_text + fsize - 1) = '\0';
        }
        fclose(fp);
    }
}

//
// function to generate RSA key pair and save the modulus and exponent
//
void CGenerate::GenerateKeyPair(const char* fname, const char* prname, const char* puname)
{
    std::string strkey_header;

    rsa = RSA_generate_key(KEY_SIZE*8, RSA_F4, NULL, NULL);
    if(RSA_check_key(rsa))
    {
        fp = fopen(fname, "wb");
        if (fp != NULL)
        {
            RSA_print_fp(fp, rsa, 0);
            fclose(fp);
        }

        prkey = BN_bn2hex(rsa->d);
        fp = fopen (prname, "wb");
        if (fp != NULL)
        {
            strkey_header = "\n// Private exponent\n#define PRIVATE_KEY \"";
            strkey_header += std::string(prkey);
            strkey_header += "\"\n";
            fputs(strkey_header.c_str(), fp);
            fclose(fp);
        }

        pukey = BN_bn2hex(rsa->n);
        fp = fopen (puname, "wb");
        if (fp != NULL)
        {
            strkey_header = "\n// RSA modulus\n#define PUBLIC_KEY \"";
            strkey_header += std::string(pukey);
            strkey_header += "\"\n";
            fputs(strkey_header.c_str(), fp);
            fclose(fp);
        }
    }
    return;
}

//
// function to initialize private key
//
void CGenerate::InitializeKey()
{
    char hex_exponent[] = PRIVATE_KEY;
    char hex_modulus[] = PUBLIC_KEY;
    char* public_exponent = new char[ KEY_SIZE*2 + 1 ];

	inm_sprintf_s(public_exponent, (KEY_SIZE * 2 + 1), "%x", RSA_F4);
    BN_hex2bn(&rsa->d, hex_exponent);
    BN_hex2bn(&rsa->n, hex_modulus);
    BN_hex2bn(&rsa->e, public_exponent);
    rsa->p = NULL;
    rsa->q = NULL;
    rsa->dmp1 = NULL;
    rsa->dmq1 = NULL;
    rsa->iqmp = NULL;

    delete [] public_exponent;
}

//
// function to perform RSA encryption
//
void CGenerate::EncryptLicense()
{
    int i, j;
    csize = (fsize % MSG_SIZE == 0) ? (KEY_SIZE)*(fsize/MSG_SIZE) : (KEY_SIZE)*(int(fsize/MSG_SIZE + 1));
    msg_chunks = csize/KEY_SIZE;
    cipher_text = (unsigned char*) malloc (csize);
    msg_text = (unsigned char*) malloc (MSG_SIZE + 1);

    for (i = 0; i < msg_chunks; i++)
    {
        for (j = 0; j < MSG_SIZE; j++)
        {
            *(msg_text + j) = *(plain_text + (MSG_SIZE * i) + j);
            if(*(msg_text + j) == '\0') break;
        }
        *(msg_text + j) = '\0';
        RSA_private_encrypt(j, msg_text, (cipher_text + i*KEY_SIZE), rsa, RSA_PKCS1_PADDING);
    }
    return;
}

//
// this function writes the encrypted license to a file
//
void CGenerate::WriteLicense(char* fname)
{
    fp = fopen (fname, "wb");
    if (fp != NULL)
    {
        for (int i = 0; i < csize; i++)
        {
            fprintf(fp, "%c", *(cipher_text + i));
        }
        fclose (fp);
    }
}

//
// free allocated resources
//
void CGenerate::FreeResources()
{
    free (msg_text);
    free (plain_text);
    free (cipher_text);
    RSA_free (rsa);
}

//
// main function
//
int main(int argc, char* argv[])
{
    //calling init funtion of inmsafec library
	init_inm_safe_c_apis();

	CGenerate license;

    if(argc != 2)
    {
        printf("\nInmage license generator v0.1\n");
        printf("Usage: %s [option | filename]\n", argv[0]);
        printf("      --new generates a new key pair\n");
        printf("\n");
        exit(1);
    }

    if(!strcmp(argv[1], "--new"))
    {
        printf("Generating new key pair...");
        license.GenerateKeyPair("output/keypair.info", "output/privatekey.h", "output/publickey.h");
    }
    else
    {
        printf("Generating license file... ");
        license.ReadLicense(argv[1]);
        license.InitializeKey();
        license.EncryptLicense();
        license.WriteLicense("output/license.dat");
    }
    printf("done\n");

    return 0;
}
