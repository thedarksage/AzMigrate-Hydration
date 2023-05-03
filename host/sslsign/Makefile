
include top.mak

SRCS = \
	sslsign.cpp

X_CFLAGS = \
	-Isslsign \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Iscopeguard \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE)

X_LIBS = \

X_THIRDPARTY_LIBS = \
	$(BOOST_LIBS) \
	$(OPENSSL_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=sslsign
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
