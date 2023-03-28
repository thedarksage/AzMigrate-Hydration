include top.mak

SRCS = \
	testvolumescopier.cpp \
	$(X_MAJOR_ARCH)/testvolumescopiermajor.cpp \
	simplevolumescopierapp.cpp \

X_CFLAGS = \
	-DDUMP_INFOS_TO_STDOUT \
	-Iinmpipeline \
	-Ivolumescopier \
	-Icdplibs \
    -Ideviceidinformer \
    -Iplatforminfo \
    -Iplatforminfo/$(X_MAJOR_ARCH) \
    -Iplatforminfo/$(X_MINOR_ARCH) \
    -Iexecutecommand \
    -Iexecutecommand/$(X_MINOR_ARCH) \
    -Iscsicommandissuer \
    -Iscsicommandissuer/$(X_MINOR_ARCH) \
    -Idirbasename \
    -Idirbasename/$(X_MAJOR_ARCH) \
	-Ivolumeinfocollector \
	-Ivolumeinfocollector/$(X_MAJOR_ARCH) \
	-Ivolumeinfocollector/$(X_MINOR_ARCH) \
	-Ilog \
	-Is2libs/foundation \
	-Is2libs/foundation/$(X_MAJOR_ARCH) \
	-Is2libs/common \
	-Is2libs/thread \
	-Is2libs/streams \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-IAzureRecoveryLib/resthelper \
	$(ACE_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(BOOST_INCLUDE)

X_LIBS = \
	dplibs \
	cdpapplylib \
	volumescopier \
	inmpipeline \
	npwwn \
    config \
    common \
    s2libs \
    tal \
    vsnaplib \
    log \
    platforminfo \
    executecommand \
    dirbasename \
    deviceidinformer \
    scsicommandissuer \
    volumeinfocollector \
    virtualvolumelib \
    cdplibs \
    vacplibs \
    conversionlib \
    cxtransportlib \
    appdevicelib \
    curlwrapperlib \
	AzureRecoveryLib \
    securitylib


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

BINARY=testvolumescopier
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
