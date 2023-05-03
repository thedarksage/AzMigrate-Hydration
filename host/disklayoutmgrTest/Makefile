include top.mak

SRCS =  \
		disklayoutmgrTest.cpp

X_CFLAGS = \
		-DVSNAP_USER_MODE \
        -Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
        -Icommon/$(X_MAJOR_ARCH) \
        -Icommon/$(X_MINOR_ARCH) \
        -I$(X_ARCH)/config \
        -Iconfig \
        -Iconfig/$(X_MAJOR_ARCH) \
        -Ilog \
        -Ital \
        -Iconversionlib \
        -Is2libs \
        -Is2libs/common \
        -Is2libs/foundation \
        -Is2libs/thread  \
        -Icdplibs -Icdplibs/v1 -Icdplibs/v3  \
        -Ivacplibs \
        -Icdplibs/$(X_MINOR_ARCH) \
        -Ivsnaplib \
        -Ivirtualvolumelib/unix \
        -Idrivers/Include/linux \
        -Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
        -Idrivers/InVolFlt/$(X_MINOR_ARCH) \
        -Idrivers/lib/$(X_MINOR_ARCH) \
        -Idrivers/InVolFlt/$(X_MINOR_ARCH) \
        -Idrivers/InVirVol/$(X_MINOR_ARCH) \
        -Idrivers/InVirVol/common \
		-Idplibs \
        -Idataprotection \
		-IRetention \
		$(ACE_INCLUDE) \
        $(SQLITE_INCLUDE) \
        $(BOOST_INCLUDE) \
        $(CURL_INCLUDE) \
        $(OPENSSL_INCLUDE) \
        $(ZLIB_INCLUDE) \
		$(LIBXML2_INCLUDE) \
		-Idisklayoutmgr \
		$(SIGSLOT_INCLUDE) \

X_LIBS = \
    npwwn \
	common \
	tal \
	log \
	config \
	s2libs \
	virtualvolumelib \
	volumeinfocollector \
    appdevicelib \
	cdplibs \
	vsnaplib \
	vacplibs \
	conversionlib \
	dplibs \
	disklayoutmgr \
        curlwrapperlib \
        securitylib

X_THIRDPARTY_LIBS = \
	$(CURL_LIBS) \
	$(ACE_LIBS) \
	$(SQLITE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
	$(LIBXML2_LIBS) \
	$(BOOST_LIBS) \
	$(INM_MD5_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS = 

BINARY=disklayoutmgrTest
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
