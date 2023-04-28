//
// common.h: includes headers and macros common to 'generate' and 'license'
//
#ifndef COMMON__H
#define COMMON__H

#ifdef WIN32
#include "..\..\thirdparty\openssl-1.0.2p\inc32\openssl\rsa.h"
#include "..\..\thirdparty\openssl-1.0.2p\inc32\openssl\bn.h"
#include "..\..\thirdparty\openssl-1.0.2p\inc32\openssl\err.h"
#else
#include "openssl/rsa.h"
#include "openssl/err.h"
#include "openssl/bn.h"
#endif // ifdef WIN32

#define KEY_SIZE 64
#define MSG_MODIFIER 11
#define MSG_SIZE (KEY_SIZE - MSG_MODIFIER)

#include "keys/publickey.h"

#endif // COMMON__H
