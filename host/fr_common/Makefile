include top.mak

SRCS = \
	unixagenthelpers.cpp \
	$(X_MAJOR_ARCH)/portableunix.cpp \
	svutils.cpp \
	svtransport.cpp \
	svtypes.cpp \
	unregworker.cpp \

X_CFLAGS = \
	-DLINUX_OS \
	-Ifr_common \
	-Ifr_common/$(X_MAJOR_ARCH) \
	-Ifr_config \
	-Ifr_config/$(X_MAJOR_ARCH) \
	-Icommon \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Iinmsafeint \
   -Iinmsafeint/$(X_MAJOR_ARCH) \
   -Ierrorexception \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
   -Icurlwrapperlib \
	$(ACE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(CURL_INCLUDE) \
	$(OPENSSL_INCLUDE) \

X_LIBS = 


X_THIRDPARTY_LIBS = \
 $(ACE_LIBS) \
 $(ARES_LIBS) \
 $(CURL_LIBS) \
 $(OPENSSL_LIBS) \
 $(ZLIB_LIBS) \

X_LDFLAGS = -Llib -lcurl

X_SYSTEM_LIBS =


BINARY=$(X_LIBPRE)fr_common
BINARY_EXT=$(X_LIBEXT)

include bottom.mak

