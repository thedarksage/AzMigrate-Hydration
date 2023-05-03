include top.mak

SRCS = appdevicereporter.cpp

X_CFLAGS = \
	-Ilog \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
    -Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
    -Icommon/$(X_MAJOR_ARCH) \
    -Icommon/$(X_MINOR_ARCH) \
    -Iexecutecommand \
    -Iexecutecommand/$(X_MINOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	$(BOOST_INCLUDE) \
	$(ACE_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \

X_LIBS = \
	tal \
	config \
    common \
	log

X_THIRDPARTY_LIBS = \
	$(ACE_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)appdevicelib
BINARY_EXT=.a

include bottom.mak





