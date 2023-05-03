include top.mak

ARCH = $(shell uname)

SRCS =  \
	main.cpp \
	resyncRcmSettings.cpp \
	resyncStateManager.cpp \
	dataprotectionRcm.cpp \
	fastsyncRcm.cpp \
	../dataprotection/fastsync.cpp \
	../dataprotection/hashcomparedata.cpp \
	../dataprotection/fsclusterinfo.cpp \
	../dataprotection/clustergen.cpp \
	../dataprotection/processcluster.cpp \
	../dataprotection/hcdgen.cpp \
	../dataprotection/hcdsend.cpp \
	../dataprotection/clusterparams.cpp \
	../dataprotectionCommon/globalHealthCollator.cpp \
    

#-DDP_SENDFILE_ON 
X_CFLAGS = \
	-DDP_SYNC_RCM \
	-DDP_TRACKING_ON \
	-DDP_PORTABLE \
	-DBASIC_TEST \
	-DVSNAP_USER_MODE \
	-IAzureRcmLib \
	-ICommitTagLib \
	-IdataprotectionCommon \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
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
	-IAzureRecoveryLib/ \
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
	AzureRcmLib \
	AzureADAuthProviderLib \
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
	cdpapplylibRcm \
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

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=DataProtectionSyncRcm
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
