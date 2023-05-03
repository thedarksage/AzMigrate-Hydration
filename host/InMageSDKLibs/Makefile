include top.mak

SRCS = \
	APIController.cpp \
	InmsdkGlobals.cpp \
	inmsdkutil.cpp \
	Handler.cpp \
	HandlerFactory.cpp \
	HostHandler.cpp \
	LicencingHandler.cpp \
	MonitorHandler.cpp \
	ProtectionHandler.cpp \
	RecoveryHandler.cpp \
	RepositoryHandler.cpp \
	StubHandler.cpp \
	VmAssociationHandler.cpp \
	inmsdkexception.cpp \
	VolumeHandler.cpp \
	unix/inmagesdkutil.cpp




X_CFLAGS = \
	-DVSNAP_USER_MODE \
	-DDP_PORTABLE \
	-IInmXmlParser \
	-Iappframeworklib \
	-IInMageSDKLibs \
	-Iinmapihandler \
	-Iappframeworklib/config \
	-Iinmapirhandler/confengine \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Iconfschema \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Ilog \
	-Ital \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVirVol/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3  \
	-Is2libs/common \
	-Is2libs \
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
	-Is2libs/foundation \
	-Is2libs/thread \
	-Ivacplibs \
	-Ivsnaplib \
	-Ivirtualvolumelib/$(X_MAJOR_ARCH) \
	-Iinmtransaction \
	-Idisklayoutmgr \
	$(ACE_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(CURL_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(INM_MD5_INCLUDE)


X_LIBS = 
  

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)InMageSDKLibs
BINARY_EXT=$(X_LIBEXT)

include bottom.mak

