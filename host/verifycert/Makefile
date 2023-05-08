
include top.mak

SRCS = \
	verifycert.cpp

X_CFLAGS = \
	-Icommon \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Ierrorexception \
	-Ierrorexception/$(X_MINOR_ARCH) \
	-Iscopeguard \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE)

X_LIBS = \
	securitylib

X_THIRDPARTY_LIBS = \
	$(BOOST_LIBS) \
	$(OPENSSL_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=verifycert
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
