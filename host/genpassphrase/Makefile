
include top.mak

SRCS = \
	genpassphrase.cpp

X_CFLAGS = \
	-Ierrorexception \
	-Ierrorexception/$(X_MINOR_ARCH) \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Iscopeguard \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE)

X_LIBS = \

X_THIRDPARTY_LIBS = \
	$(BOOST_LIBS) \
	$(OPENSSL_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=genpassphrase
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
