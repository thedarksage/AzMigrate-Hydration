include top.mak

SRCS =  main.cpp 

X_CFLAGS = \
	$(CURL_INCLUDE) \
	$(OPENSSL_INCLUDE) \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
    -Ilog \
    -Iconfig \
    -Iconfig/$(X_MAJOR_ARCH) \
	-Icommon \
	-Icommon/$(X_MAJOR_ARCH) \
    -Iscopeguard \
    -Isecuritylib \
    -Isecuritylib/$(X_MAJOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
   -Ierrorexception \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
   -Icurlwrapperlib \
	$(ZLIB_INCLUDE) \
   $(BOOST_INCLUDE) \
   $(ACE_INCLUDE) \
	$(LIBXML2_INCLUDE) \
    $(SIGSLOT_INCLUDE) \
    $(INM_MD5_INCLUDE)


X_LIBS = \
         curlwrapperlib \
         securitylib

X_THIRDPARTY_LIBS = \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(LIBXML2_LIBS) \
	$(ZLIB_LIBS) \
    $(BOOST_LIBS)

ifeq ($(X_OS),$(filter $(X_OS), RHEL8-64 DEBIAN11-64 UBUNTU-22.04-64 RHEL9-64 OL8-64 SLES15-64 OL9-64))
        X_SYSTEM_LIBS = -llzma
else
        X_SYSTEM_LIBS =
endif

X_LDFLAGS = 

BINARY=cxcli
BINARY_EXT=$(X_EXEEXT)

include bottom.mak

