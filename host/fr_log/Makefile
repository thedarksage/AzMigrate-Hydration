include top.mak

SRCS =  logger.cpp \

X_CFLAGS = \
	-DLINUX_OS \
	-Ifr_common \
	-Ifr_common/$(X_MAJOR_ARCH) \
	-Ifr_config \
	-Ifr_config/$(X_MAJOR_ARCH) \
	-Icommon \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	$(ACE_INCLUDE) \
	$(CURL_INCLUDE) \
	$(BOOST_INCLUDE) \

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)fr_log
BINARY_EXT=$(X_LIBEXT)

include bottom.mak

