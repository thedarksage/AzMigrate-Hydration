include top.mak

SRCS = unixunreg.cpp

X_CFLAGS = \
        -Ifr_common \
        -Ifr_common/$(X_MAJOR_ARCH) \
        -Ifr_config \
        -Ifr_config/$(X_MAJOR_ARCH) \
	-Icommon \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \

X_LIBS = \
        fr_log \
        fr_common \
        fr_config \
        curlwrapperlib \
        securitylib

X_SYSTEM_LIBS = \
        -lrt \
        -ldl \

X_THIRDPARTY_LIBS = \
 $(ACE_LIBS) \
 $(ARES_LIBS) \
 $(CURL_LIBS) \
 $(OPENSSL_LIBS) \
 $(ZLIB_LIBS) \
 $(BOOST_LIBS)

X_LDFLAGS = -Llib


BINARY=unregfragent
BINARY_EXT=$(X_EXEEXT)
DEPS=

include bottom.mak
