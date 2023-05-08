include top.mak
SRCS = \
	hostdiscoveryhandler.cpp \
	settingshandler.cpp \
	generalupdate.cpp \
	appsettingshandler.cpp \
	generalupdate.cpp \
	hostdiscoveryhandler.cpp \
	paircreationhandler.cpp \
	policyhandler.cpp \
	resynchandler.cpp \
	settingshandler.cpp \
	sourcehandler.cpp \
	targethandler.cpp \
	snapshotshandler.cpp \
	util.cpp \
	confengine/alertconfig.cpp \
	confengine/confschemareaderwriter.cpp \
	confengine/policyconfig.cpp \
	confengine/protectionpairconfig.cpp \
	confengine/scenarioconfig.cpp \
	confengine/volumeconfig.cpp \
	confengine/licenseconfig.cpp \
	confengine/agentconfig.cpp \
	confengine/auditconfig.cpp \
	confengine/eventqueueconfig.cpp


X_CFLAGS = \
	-DVSNAP_USER_MODE \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Iconfschema \
	-Iappframeworklib \
	-Ifio \
	-Ifio/$(X_MINOR_ARCH) \
	-Ifio/$(X_MAJOR_ARCH) \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-IInmXmlParser \
	-Iinmapihandler \
	-Iconfschema \
	-IInMageSDKLibs \
	-Ierrorexception \
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	-Izflate \
	-Iscopeguard \
	-Icxtransportlib \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Ilog \
	-Ital \
	-Is2libs \
	-Is2libs/common \
	-Is2libs/foundation \
	-Is2libs/thread  \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3  \
	-Ivacplibs \
	-Ivsnaplib \
	-Ivirtualvolumelib/unix \
	-Idrivers/Include/linux \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVirVol/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Idrivers/InVirVol/linux \
	-Idataprotection \
	-Iinmtransaction \
	-Ivacp/StreamEngine \
	-Idisklayoutmgr \
	-Ivirtualvolumelib/$(X_MAJOR_ARCH) \
	$(ACE_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(SIGSLOT_INCLUDE) \

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)inmapihandler
BINARY_EXT=$(X_LIBEXT)

include bottom.mak


