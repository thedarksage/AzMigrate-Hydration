
include top.mak

SRCS = \
	curlwrapper.cpp \
	$(X_MAJOR_ARCH)/curlwrappermajor.cpp

X_CFLAGS = \
	-Icurlwrapperlib \
	-Icurlwrapperlib/$(X_MAJOR_ARCH) \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Iscopeguard \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(CURL_INCLUDE)

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

ifeq ($(X_OS),$(filter $(X_OS), RHEL9-64))
        X_LDFLAGS = -lzstd
else
        X_LDFLAGS =
endif

BINARY=$(X_LIBPRE)curlwrapperlib
BINARY_EXT=$(X_LIBEXT)

include bottom.mak
