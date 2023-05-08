include top.mak
SRCS =  \
	svdcheck.cpp \

X_CFLAGS = \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
        -Ivalidatesvdfile \
	-Ilog \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	$(ACE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(CURL_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(SIGSLOT_INCLUDE) \

X_LIBS = \
    cdpapplylib \
    dplibs \
    validatesvdfile \
	npwwn \
	cdplibs \
	config \
	common \
	s2libs \
	tal \
	vsnaplib \
	log \
	volumeinfocollector \
    deviceidinformer \
    platforminfo \
    executecommand \
    scsicommandissuer \
    dirbasename \
    appdevicelib \
	virtualvolumelib \
	vacplibs \
	conversionlib \
	cxtransportlib \
        curlwrapperlib \
        securitylib

X_THIRDPARTY_LIBS =  \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
    $(BOOST_LIBS) \
	$(SQLITE_LIBS) \
	$(INM_MD5_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS = 

BINARY=svdcheck
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
