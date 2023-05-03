include top.mak

SRCS =  \
    AADAuthProvider.cpp \

X_CFLAGS = \
    -IAzureRcmLib \
    -IAzureRecoveryLib \
	-IAzureRecoveryLib/resthelper \
	-Ilog \
	-Icommon \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Iinmsafeint \
	-Is2libs \
	-Is2libs/foundation \
	-Is2libs/thread \
	-Ivolumeinfocollector \
	-Ivolumeinfocollector/$(X_MAJOR_ARCH) \
	-Ivolumeinfocollector/$(X_MINOR_ARCH) \
	-Idevicefilter \
	-Idevicefilter/$(X_MAJOR_ARCH) \
	-Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Iscopeguard \
	-Icxpslib \
	$(ACE_INCLUDE) \
	$(CURL_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ESJSON_INCLUDES) \
	$(SIGSLOT_INCLUDE) \

X_LIBS = \

X_THIRDPARTY_LIBS =  \

X_SYSTEM_LIBS = \

X_LDFLAGS = 

BINARY=$(X_LIBPRE)AzureADAuthProviderLib
BINARY_EXT=$(X_LIBEXT)

include bottom.mak
