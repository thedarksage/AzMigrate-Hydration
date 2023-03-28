include top.mak

SRCS =  \
    RcmClient.cpp \
    Converter.cpp \
    AgentSettings.cpp \
    AgentConfigHelpers.cpp \
    Monitoring.cpp \
    RcmConfigurator.cpp \
    RcmClientProxyImpl.cpp \
    RcmClientSettings.cpp \
    RcmClientAzureImpl.cpp \
    RcmProxyDetails.cpp \
    TransportSettings.cpp \
    CertUtil.cpp \
    MigrationHelper.cpp \
	MonitoringMsgHandler.cpp \
	AgentMonitoringHandler.cpp \
	VirtualNodeMonitoringHandler.cpp \
	SharedDiskClusterMonitoringHandler.cpp

X_CFLAGS = \
	-IAzureRcmLib \
	-ICommitTagLib \
	-IAzureRecoveryLib \
	-IAzureRecoveryLib/resthelper \
	-IAzureRecoveryLib/common \
	-IAzureADAuthProviderLib \
	-Ilog \
	-Icommon \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Ierrorexception \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Iinmsafeint \
	-Is2libs \
	-Is2libs/foundation \
	-Is2libs/foundation/unix \
	-Is2libs/thread \
	-Is2libs/streams \
	-Ivolumeinfocollector \
	-Ivolumeinfocollector/$(X_MAJOR_ARCH) \
	-Ivolumeinfocollector/$(X_MINOR_ARCH) \
	-Iexecutecommand \
	-Idirbasename \
	-Ideviceidinformer \
	-Iscsicommandissuer \
	-Iscsicommandissuer/$(X_MINOR_ARCH) \
	-Idevicefilter \
	-Idevicefilter/$(X_MAJOR_ARCH) \
	-Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Iscopeguard \
	-Icxpslib \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Ifio \
	-Ifio/$(X_MAJOR_ARCH) \
	-Ifio/$(X_MINOR_ARCH) \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Ivacplibs \
	$(ACE_INCLUDE) \
	$(CURL_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ESJSON_INCLUDES) \
	$(LIBXML2_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(INM_MD5_INCLUDE) \

X_LIBS = \

X_THIRDPARTY_LIBS =  \

X_SYSTEM_LIBS = \

X_LDFLAGS = 

BINARY=$(X_LIBPRE)AzureRcmLib
BINARY_EXT=$(X_LIBEXT)

include bottom.mak
