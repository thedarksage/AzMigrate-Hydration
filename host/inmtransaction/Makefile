include top.mak

SRCS =  inmlogbasedtransaction.cpp \
        inmfilebackupmgr.cpp

X_CFLAGS = \
	-Ilog \
	-Iconfig \
	-Iconfig/unix \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Iinmtransaction \
	-Iinmtransaction/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	$(CURL_INCLUDE) \
	$(ACE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(SQLITE_INCLUDE) \

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)inmtransaction
BINARY_EXT=$(X_LIBEXT)

include bottom.mak
