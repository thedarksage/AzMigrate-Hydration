
include top.mak

SRCS = \
	viewcert.cpp

X_CFLAGS = \
	-Iviewcert \
	-Ierrorexception \
	-Ierrorexception/$(X_MINOR_ARCH) \
	-Iscopeguard \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE)

X_LIBS = 

X_THIRDPARTY_LIBS = \
	$(BOOST_LIBS) \
	$(OPENSSL_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=viewcert
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
