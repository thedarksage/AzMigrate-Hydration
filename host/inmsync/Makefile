include top.mak

SRCS = \
	inmsync.cpp \
	cxpssync.cpp

X_CFLAGS = \
	-Iinmsync \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Ilistfile \
	-Ilistfile/unix \
	-Isimplelogger \
	-Isimplelogger/unix \
	-Izflate \
	-Iscopeguard \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Ifio \
	-Ifio/$(X_MINOR_ARCH) \
	-Ifio/unix \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDES)

X_LIBS = \
	securitylib

X_THIRDPARTY_LIBS = \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
	$(BOOST_LIBS) \
	$(INM_MD5_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=inmsync
BINARY_EXT=$(X_EXEEXT)

include bottom.mak


