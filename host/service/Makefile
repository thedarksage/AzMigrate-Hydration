include top.mak
X_OS:=$(shell ../build/scripts/general/OS_details.sh 1)

SRCS = \
	$(X_MAJOR_ARCH)/volumemonitormajor.cpp \
	$(X_MINOR_ARCH)/volumemonitorminor.cpp \
	cservice.cpp \
	$(X_MAJOR_ARCH)/servicemajor.cpp \
	AgentProcess.cpp \
	vxservice.cpp \
	partition_types.cpp \
	registerhostthread.cpp \
	monitorhostthread.cpp \
	monitorhostcommon.cpp \
	rcmworkerthread.cpp \
	../serviceCommon/consistencythread.cpp \
    consistencythread-rcm.cpp \

X_CFLAGS = \
	-DVSNAP_USER_MODE \
    -IserviceCommon \
	-Iiscsiinterface \
	-Iservice \
	-I../thirdparty/service \
	-Iservice/$(X_MAJOR_ARCH) \
	-Icommon \
	-Iinmuuid/$(X_MAJOR_ARCH) \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Idplibs \
	-Icdpapplylib \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3 \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Ilog \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
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
	-Ibackups2lib/domain \
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
	-Ierrorexception \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Icurlwrapperlib \
	-IAzureRcmLib \
	-ICommitTagLib \
	-IAzureRecoveryLib \
	-IAzureRecoveryLib/resthelper \
	-Ivalidatesvdfile \
	-Icxtransportlib \
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	-Izflate \
	-Iscopeguard \
	-Ifio \
	-Ifio/$(X_MAJOR_ARCH) \
	-Ifio/$(X_MINOR_ARCH) \
	-Iconversionlib \
	-Iazureagentapi \
	-Igenericprofiler \
	-Ivacplibs \
	$(SQLITE_INCLUDE) \
	$(CURL_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ACE_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(ESJSON_INCLUDES) \
	$(INM_MD5_INCLUDE) \
	$(LIBXML2_INCLUDE) \

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
    securitylib \
	AzureRecoveryLib \
	AzureRcmLib \
	CommitTagLib \
	AzureADAuthProviderLib \
	cxtransportlib

X_THIRDPARTY_LIBS = \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
	$(SQLITE_LIBS) \
    $(BOOST_LIBS) \
    $(INM_MD5_LIBS) \
	$(LIBXML2_LIBS) \

ifeq ($(X_OS),$(filter $(X_OS), RHEL8-64 DEBIAN11-64 UBUNTU-22.04-64 RHEL9-64 OL8-64 OL9-64 SLES15-64))
    X_SYSTEM_LIBS = -luuid -llzma
else
    X_SYSTEM_LIBS = -luuid
endif

X_LDFLAGS =

BINARY=svagents
BINARY_EXT=$(X_EXEEXT)

include bottom.mak

