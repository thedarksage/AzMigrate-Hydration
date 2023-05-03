include top.mak

SRCS = \
	testinmdistributor.cpp \
	echoworker.cpp \
	additiondistributor.cpp

X_CFLAGS = \
	-DDUMP_INFOS_TO_STDOUT \
	-Iinmdistributor \
	-Ilog \
	-Is2libs/foundation \
	-Is2libs/foundation/$(X_MAJOR_ARCH) \
	-Is2libs/common \
	-Is2libs/thread \
	-Is2libs/streams \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	$(ACE_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(BOOST_INCLUDE)

X_LIBS = \
	dplibs \
	cdpapplylib \
	inmdistributor \
	npwwn \
    cdplibs \
    config \
    common \
    s2libs \
    tal \
    vsnaplib \
    log \
    deviceidinformer \
    platforminfo \
    executecommand \
    scsicommandissuer \
    dirbasename \
    volumeinfocollector \
	appdevicelib \
    virtualvolumelib \
    cdplibs \
    vacplibs \
    conversionlib \
    cxtransportlib \
    npwwn \
    appdevicelib


X_THIRDPARTY_LIBS = \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
    $(SQLITE_LIBS) \
    $(BOOST_LIBS) \
    $(INM_MD5_LIBS)


X_SYSTEM_LIBS = 

X_LDFLAGS =

BINARY=testinmdistributor
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
