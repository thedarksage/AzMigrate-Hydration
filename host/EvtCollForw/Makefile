include top.mak

SRCS = 	Collectors.cpp \
	Converters.cpp \
	EvtCollForwCommon.cpp \
	Forwarders.cpp \
	main.cpp \
	RunnerFactories.cpp \
    TraceProcessor.cpp \
	EventHubRestHelper.cpp

X_CFLAGS =  \
	-IAzureRcmLib \
	-ICommitTagLib \
	-IAzureRecoveryLib \
	-Icommon \
	-Iinmsafeint \
	-Iinmsafeint/$(X_MAJOR_ARCH) \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Icxtransportlib \
	-Ierrorexception \
	-Ifio \
	-Ifio/$(X_MINOR_ARCH) \
	-Ifio/$(X_MAJOR_ARCH) \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Ilog \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3  \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-IAzureRecoveryLib \
	-IAzureRecoveryLib/resthelper \
	-Iscopeguard \
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	-Is2libs/thread \
	-Ital \
	-Ivolumeinfocollector \
	-Ivolumeinfocollector/$(X_MAJOR_ARCH) \
	-Ivolumeinfocollector/$(X_MINOR_ARCH) \
	-Izflate \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	$(ACE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(CURL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(ESJSON_INCLUDES) \
	$(INM_MD5_INCLUDE)

X_LIBS = \
	npwwn \
	validatesvdfile \
	common \
	s2libs \
	config \
	cxtransportlib \
	tal \
	log \
	cdplibs \
	vsnaplib \
	dplibs \
	vacplibs \
	volumeinfocollector \
	deviceidinformer \
	platforminfo \
	executecommand \
	scsicommandissuer \
	dirbasename \
	appdevicelib \
	conversionlib \
	virtualvolumelib \
	backups2lib \
	cdpapplylib \
	curlwrapperlib \
	securitylib \
	AzureRcmLib \
	AzureRecoveryLib \
	AzureADAuthProviderLib \
	cxpslib


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

BINARY=evtcollforw
BINARY_EXT=$(X_EXEEXT)

include bottom.mak

