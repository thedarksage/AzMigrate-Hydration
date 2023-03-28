include top.mak
SRCS = \
	agentconfigobject.cpp \
	apppolicyobject.cpp \
	appscenarioobject.cpp \
	autoresyncobj.cpp \
	backuprepositoryobj.cpp \
	confschema.cpp \
	confschemamgr.cpp \
	confschemaparser.cpp \
	consistencypolicyobj.cpp \
	pairinfoobj.cpp \
	pairsobj.cpp \
	planobj.cpp \
	policyinstanceobj.cpp \
	protectionpairobject.cpp \
	replicationoptionsobj.cpp \
	retevtpolicyobject.cpp \
	retlogpolicyobject.cpp \
	retwindowobject.cpp \
	repositoriesobject.cpp \
	scenariondetailsobj.cpp \
	scenarioretentionpolicyobj.cpp \
	scenarioretentionwindowobj.cpp \
	volumesobject.cpp \
	alertobject.cpp \
	snapshotsobject.cpp \
	auditobject.cpp \
	snapshotinstancesobject.cpp \
	eventqueueobj.cpp


X_CFLAGS = \
	-DVSNAP_USER_MODE \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
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
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Idrivers/InVirVol/linux \
	-Idataprotection \
	$(ACE_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(SIGSLOT_INCLUDE) \

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)confschema
BINARY_EXT=$(X_LIBEXT)

include bottom.mak


