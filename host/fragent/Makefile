include top.mak

SRCS = \
	fragent.cpp \
	$(X_MAJOR_ARCH)/fragentmajor.cpp

X_CFLAGS = \
	-Ifragent \
	-Ifragent/$(X_MAJOR_ARCH) \
	-DLINUX_OS \
	-Ifr_common \
	-Ifr_common/$(X_MAJOR_ARCH) \
	-Ifr_config \
	-Ifr_log \
	-Icommon \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	$(CURL_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE) \

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)fragent
BINARY_EXT=$(X_LIBEXT)

include bottom.mak

