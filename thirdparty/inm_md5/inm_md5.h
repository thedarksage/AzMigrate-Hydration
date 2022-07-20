///
/// Define functions used to compute MD5 checksum
///
#ifndef INM_MD5_H
#define INM_MD5_H

#define DIGEST_LEN 16

#ifdef __alpha
typedef unsigned int uint32;
#else
typedef unsigned int uint32;
#endif

#define INM_MD5TEXTSIGLEN 32

typedef struct INM_MD5Context {
        uint32 buf[4];
        uint32 bits[2];
        unsigned char in[64];
} INM_MD5Context;

typedef struct INM_MD5Context INM_MD5_CTX;

#ifdef __cplusplus
extern "C" {
#endif

void byteReverse(unsigned char *buf, unsigned longs);
void INM_MD5Transform(uint32 buf[4], uint32 in[16]);
void INM_MD5Init(INM_MD5Context *ctx);
void INM_MD5Update(INM_MD5Context *ctx, unsigned char *buf, unsigned len);
void INM_MD5Final(unsigned char digest[16], struct INM_MD5Context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* !INM_MD5__H */
