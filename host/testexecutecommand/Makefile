include top.mak

SRCS = \
	testexecutecommand.cpp

X_CFLAGS = \
	-DDUMP_INFOS_TO_STDOUT \
    -Iexecutecommand \
    -Iexecutecommand/$(X_MINOR_ARCH) \
	-Ilog \
	-Ilog/$(X_MAJOR_ARCH) \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \

X_LIBS = \
    log \
    executecommand \
    curlwrapperlib \
    securitylib

X_THIRDPARTY_LIBS = \
    $(ACE_LIBS) \
    $(ARES_LIBS) \
    $(CURL_LIBS) \
    $(OPENSSL_LIBS) \
    $(ZLIB_LIBS) \
    $(BOOST_LIBS) \

X_SYSTEM_LIBS = 

X_LDFLAGS =

BINARY=testexecutecommand
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
