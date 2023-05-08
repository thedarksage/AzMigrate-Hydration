include top.mak

ARCH = $(shell uname)

SRCS =  \
	dataprotection.cpp \
	dataprotector.cpp \
	fastsync.cpp \
	hashcomparedata.cpp \
	main.cpp \
	fsclusterinfo.cpp \
	clustergen.cpp \
	processcluster.cpp \
	hcdgen.cpp \
	hcdsend.cpp \
	clusterparams.cpp \
	../dataprotectionCommon/globalHealthCollator.cpp \


#-DDP_SENDFILE_ON 
X_CFLAGS = \
	-DDP_TRACKING_ON \
	-DDP_PORTABLE \
	-DBASIC_TEST \
	-DVSNAP_USER_MODE \
	-IdataprotectionCommon \
	-IAzureRcmLib \
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	-Igenericprofiler \
	-Izflate \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	-Iscopeguard \
	-Ivolumescopier \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Ifio \
	-Ifio/$(X_MINOR_ARCH) \
	-Ifio/$(X_MAJOR_ARCH) \
	-Icxtransportlib \
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
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Ilog \
	-Iinmpipeline \
	-Iconversionlib \
	-Ital \
	-ISnapShotMgr/include \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3 \
	-IRetention \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Is2libs \
	-Is2libs/common \
	-Is2libs/foundation \
	-Is2libs/foundation/$(X_MAJOR_ARCH) \
	-Is2libs/thread \
	-Is2libs/streams \
	-Idplibs \
	-Ivacplibs \
	-Icdpapplylib \
	-Idataprotection \
	-Ilocaldirectsynclib \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
   -Ierrorexception \
   -Icurlwrapperlib \
	-Iazureagentapi \
	-Iazureagentapi/$(X_MAJOR_ARCH) \
	-IAzureRecoveryLib/resthelper \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ACE_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(CURL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(INM_MD5_INCLUDE) \
	$(ESJSON_INCLUDES) \

X_LIBS = \
	volumescopier \
    inmpipeline \
    npwwn \
	common \
	cxtransportlib \
	tal \
	config \
	s2libs \
	log \
	cdplibs \
	virtualvolumelib \
	vsnaplib \
	vacplibs \
	volumeinfocollector \
    deviceidinformer \
    platforminfo \
    executecommand \
    scsicommandissuer \
    dirbasename \
    appdevicelib \
	dplibs \
	conversionlib \
	cdpapplylib \
	AzureRecoveryLib \
	localdirectsynclib \
        curlwrapperlib \
        securitylib \
		simplelogger \
		genericprofiler

X_THIRDPARTY_LIBS = \
	$(SQLITE_LIBS) \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
	$(BOOST_LIBS) \
	$(INM_MD5_LIBS)

ifeq ($(X_OS),$(filter $(X_OS), OL8-64 OL9-64))
        X_SYSTEM_LIBS = -llzma
else
        X_SYSTEM_LIBS =
endif

X_LDFLAGS =

BINARY=dataprotection
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
