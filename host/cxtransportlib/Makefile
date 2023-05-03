include top.mak

SRCS = \
	cxtransport.cpp  \
	cxtransportimpclient.cpp  \
	cxtransportimp.cpp  \
	cxtransportlogger.cpp \
	azureblobclient.cpp \

X_CFLAGS = \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Ifio \
	-Ifio/$(X_MINOR_ARCH) \
	-Ifio/$(X_MAJOR_ARCH) \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	-Izflate \
	-Iscopeguard \
	-Iconfig \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Ierrorexception \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-IAzureRecoveryLib/resthelper \
	$(ACE_INCLUDE) \
	$(CURL_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ZLIB_INCLUDE)

X_LIBS =

X_THIRDPARTY_LIBS =

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)cxtransportlib
BINARY_EXT=$(X_LIBEXT)

include bottom.mak


