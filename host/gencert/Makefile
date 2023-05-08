
include top.mak

SRCS = \
	gencert.cpp

X_CFLAGS = \
	-Igencert \
	-Igencert/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	-Ierrorexception/$(X_MINOR_ARCH) \
	-Iscopeguard \
	-Ilog \
	-Icommon \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Icxpslib \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE)

X_LIBS = \

X_THIRDPARTY_LIBS = \
	$(BOOST_LIBS) \
	$(OPENSSL_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=gencert
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
