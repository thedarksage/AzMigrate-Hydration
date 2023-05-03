include top.mak

SRCS = \
	$(X_MAJOR_ARCH)/service.cpp

X_CFLAGS = \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Ierrorexception \
	-Ierrorexception/$(X_MINOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon \
   -Iinmsafeint \
   -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Ifio \
	-Ifio/$(X_MINOR_ARCH) \
	-Ifio/$(X_MAJOR_ARCH) \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Iscopeguard \
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	-Izflate \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Isecuritylib \
	-Isecuritylib/$(X_MINOR_ARCH) \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	$(OPENSSL_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(ESJSON_INCLUDES) \
	$(INM_MD5_INCLUDE)

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

BINARY=cxps
BINARY_EXT=$(X_EXEEXT)

include bottom.mak


