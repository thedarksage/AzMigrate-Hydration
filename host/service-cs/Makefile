include top.mak
SRCS = \
	$(X_MAJOR_ARCH)/volumemonitormajor.cpp \
	$(X_MINOR_ARCH)/volumemonitorminor.cpp \
	cservice.cpp \
	$(X_MAJOR_ARCH)/servicemajor.cpp \
	AgentProcess.cpp \
	vxservice.cpp \
	registerhostthread.cpp \
	monitorhostthread.cpp \
	../service/monitorhostcommon.cpp \
	../serviceCommon/consistencythread.cpp \
    consistencythread-cs.cpp \

X_CFLAGS = \
	-DVSNAP_USER_MODE \
	-Iiscsiinterface \
    -IserviceCommon \
	-Iservice-cs \
	-Iservice-cs/$(X_MAJOR_ARCH) \
	-Icommon \
	-Iinmuuid/$(X_MAJOR_ARCH) \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3 \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Ilog \
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
	-Idrivers/InVirVol/common \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Ibackups2lib-cs/domain \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Is2libs/thread \
	-Is2libs/foundation/$(X_MAJOR_ARCH) \
	-Is2libs/foundation \
	-Is2libs/streams \
	-Is2libs/common \
	-Is2/domain \
	-Ivsnaplib \
	-Ivirtualvolumelib/unix \
	-Ibranding \
	-Ixslibs/$(X_MAJOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Iscopeguard \
	-Ierrorexception \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
   -Icurlwrapperlib \
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	-Izflate \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Icxtransportlib \
	-Ivacplibs \
	-Ifio \
	-Ifio/$(X_MAJOR_ARCH) \
	-Ifio/$(X_MINOR_ARCH) \
	-IAzureRecoveryLib/resthelper \
	$(SQLITE_INCLUDE) \
	$(CURL_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ACE_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(ESJSON_INCLUDES) \

X_LIBS = \
	validatesvdfile \
	backups2lib \
	dplibs \
	cdpapplylib \
	npwwn \
	iscsiinterface \
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
    cdplibs \
	vacplibs \
	conversionlib \
	xslibs \
    cxtransportlib \
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

X_SYSTEM_LIBS = -luuid

X_LDFLAGS =

BINARY=svagents
BINARY_EXT=$(X_EXEEXT)

include bottom.mak

