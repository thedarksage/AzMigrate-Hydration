///
/// Define functions used to compute MD5 checksum
///
#ifndef MD5__H
#define MD5__H

#ifdef __alpha
typedef unsigned int uint32;
#else
typedef unsigned long uint32;
#endif

#define MD5TEXTSIGLEN 32

typedef struct MD5Context {
        uint32 buf[4];
        uint32 bits[2];
        unsigned char in[64];
} MD5Context;

typedef struct MD5Context MD5_CTX;

#ifdef __cplusplus
extern "C" {
#endif

void byteReverse(unsigned char *buf, unsigned longs);
void MD5Transform(uint32 buf[4], uint32 in[16]);
void MD5Init(MD5Context *ctx);
void MD5Update(MD5Context *ctx, unsigned char *buf, unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* !MD5__H */
