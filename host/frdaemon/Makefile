include top.mak

SRCS =  daemon.cpp

X_CFLAGS = \
	-DLINUX_OS \
	-Ifr_common \
	-Ifr_common/$(X_MAJOR_ARCH) \
	-Ifr_config \
	-Ifr_config/$(X_MAJOR_ARCH) \
	-Icommon \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
   -Ierrorexception \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
   -Icurlwrapperlib \
	$(CURL_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE) \

X_LIBS = \
   fr_log \
   fr_common \
   fr_config \
   fragent \
   curlwrapperlib \
   securitylib

X_SYSTEM_LIBS = 

X_OS := $(shell ../build/scripts/general/OS_details.sh 1)

ifneq ($(X_OS),RHEL3-32)
X_THIRDPARTY_LIBS = $(ARES_LIBS) $(CURL_LIBS) $(OPENSSL_LIBS) $(ZLIB_LIBS) $(BOOST_LIBS) 
endif

ifeq ($(X_OS),RHEL3-32)
X_LDFLAGS = -Llib -lcurl
else
X_LDFLAGS =
endif

BINARY=svfrd
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
