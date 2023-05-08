include top.mak

SRCS = \
	cxpscli.cpp

X_CFLAGS = \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	-Ifio \
	-Ifio/$(X_MINOR_ARCH) \
	-Ifio/$(X_MAJOR_ARCH) \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Iscopeguard \
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	-Izflate \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ESJSON_INCLUDES) \
	$(INM_MD5_INCLUDE) \
	-Isecuritylib \
	-Isecuritylib/$(X_MINOR_ARCH) \
	-Isecuritylib/$(X_MAJOR_ARCH)

X_LIBS = \
	cxpslib \
	securitylib

X_THIRDPARTY_LIBS = \
	$(OPENSSL_LIBS) \
	$(BOOST_LIBS) \
	$(ZLIB_LIBS) \
	$(INM_MD5_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=cxpscli
BINARY_EXT=$(X_EXEEXT)

include bottom.mak


