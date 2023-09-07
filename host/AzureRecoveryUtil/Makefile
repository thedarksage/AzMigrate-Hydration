include top.mak
X_OS:=$(shell ../build/scripts/general/OS_details.sh 1)

SRCS =  \
	main.cpp 

X_CFLAGS = \
	-IAzureRecoveryLib \
	$(BOOST_INCLUDE) \
	$(ACE_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(CURL_INCLUDE) \
	$(LIBXML2_INCLUDE) 

X_LIBS = \
    AzureRecoveryLib \
    securitylib

X_THIRDPARTY_LIBS = \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
	$(BOOST_LIBS) \
	$(LIBXML2_LIBS)

ifeq ($(X_OS),$(filter $(X_OS), RHEL8-64 RHEL7-64 RHEL9-64 SLES11-SP4-64 SLES15-64 DEBIAN11-64 UBUNTU-22.04-64 OL8-64 OL9-64))
        X_SYSTEM_LIBS = -llzma
else
        X_SYSTEM_LIBS =
endif

X_LDFLAGS =

BINARY=AzureRecoveryUtil
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
