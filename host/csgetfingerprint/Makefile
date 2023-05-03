
include top.mak

SRCS = \
	main.cpp

X_CFLAGS = \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	-Ierrorexception/$(X_MINOR_ARCH) \
	-Ifio \
	-Ifio/$(X_MAJOR_ARCH) \
	-Iscopeguard \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	-Izflate \
	-Iconfig \
	-Icommon \
	-Icommon/$(X_MAJOR_ARCH) \
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

BINARY=csgetfingerprint
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
