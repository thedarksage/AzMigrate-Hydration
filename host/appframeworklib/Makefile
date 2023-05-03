include top.mak
SRCS = \
	app/$(X_MAJOR_ARCH)/appfactorymajor.cpp \
    app/$(X_MAJOR_ARCH)/oracle/Oracleapplication.cpp \
    app/$(X_MAJOR_ARCH)/oracle/Oraclecontroller.cpp \
    app/$(X_MAJOR_ARCH)/oracle/OracleDiscovery.cpp \
    app/$(X_MAJOR_ARCH)/oracle/Oraclediscoveryandvalidator.cpp \
    app/$(X_MAJOR_ARCH)/db2/Db2Application.cpp \
    app/$(X_MAJOR_ARCH)/db2/Db2Controller.cpp \
    app/$(X_MAJOR_ARCH)/db2/Db2Discovery.cpp \
    app/$(X_MAJOR_ARCH)/db2/Db2discoveryandvalidator.cpp \
	app/$(X_MAJOR_ARCH)/Array/arraydiscoveryandvalidator.cpp \
    app/$(X_MAJOR_ARCH)/Array/arraycontroller.cpp \
    app/$(X_MAJOR_ARCH)/Array/arrayHBController.cpp \
	app/appcontroller.cpp \
	util/common/commonmajor.cpp \
	util/common/util.cpp \
	util/volpack.cpp \
	Consistency/TagGenerator.cpp \
	Consistency/tagdiscovery.cpp \
	controller.cpp \
	discovery/discoverycontroller.cpp \
	discovery/$(X_MAJOR_ARCH)/discoveryfactory.cpp \
	ruleengine/validator.cpp \
	ruleengine/$(X_MAJOR_ARCH)/ruleengine.cpp \
	ruleengine/$(X_MAJOR_ARCH)/rulefactory.cpp \
    ruleengine/$(X_MAJOR_ARCH)/inmagevalidator.cpp \
    ruleengine/$(X_MAJOR_ARCH)/Oraclevalidators.cpp \
	ruleengine/$(X_MAJOR_ARCH)/Db2validators.cpp \
	discovery/$(X_MAJOR_ARCH)/discoveryandvalidator.cpp \
	config/appconfchangemediator.cpp \
	config/appconfigurator.cpp \
	config/applocalconfigurator.cpp \
	config/cxupdatecache.cpp \
	config/connection.cpp \
	config/appconfigvalueobj.cpp \
	config/appparser.cpp \
    ../fabricagent/devicehelper.cpp \
	util/$(X_MAJOR_ARCH)/$(X_MINOR_ARCH)/exportdeviceminor.cpp \
	util/common/bulkProtection.cpp \
    util/common/bulkprotectionandvalidator.cpp \
    util/common/cloudcontroller.cpp \
	util/$(X_MAJOR_ARCH)/utilmajor.cpp \
    storagerepository/$(X_MAJOR_ARCH)/storagerepositorymajor.cpp 

X_CFLAGS = \
	-DVSNAP_USER_MODE \
	-Ischeduler \
	-IInmXmlParser \
	-Icommon \
	-Iinmsafeint \
	-Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-I$(X_ARCH)/config \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Iconfig/$(X_MINOR_ARCH) \
	-Iconfig \
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
	-Ivolumeinfocollector/$(X_MAJOR_ARCH) \
	-Ifabricagent \
	-Iappframeworklib \
	-Iappframeworklib/discovery/$(X_MAJOR_ARCH) \
	-Iappframeworklib/util/$(X_MAJOR_ARCH) \
	-Iappframeworklib/util/common \
	-Iappframeworklib/ruleengine/$(X_MAJOR_ARCH) \
	-Iappframeworklib/$(X_MAJOR_ARCH) \
	-Iappframeworklib/app/$(X_MAJOR_ARCH) \
	-Iappframeworklib/app/$(X_MAJOR_ARCH)/oracle \
	-Iappframeworklib/app/$(X_MAJOR_ARCH)/db2 \
	-Iappframeworklib/storagerepository/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Iconfig/$(X_MINOR_ARCH) \
	-Ital \
	-Is2libs \
	-Is2libs/common \
	-Is2libs/foundation \
	-Is2libs/foundation/$(X_MAJOR_ARCH) \
	-Is2libs/thread  \
	-Is2libs/streams  \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v2  \
	-Ivacplibs \
	-Ivsnaplib \
	-Ischeduler \
	-Inpwwn \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Icurlwrapperlib \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Isecuritylib/$(X_MINOR_ARCH) \
	-Iscopeguard \
	-Ifio \
	-Ifio/$(X_MAJOR_ARCH) \
	-Ifio/$(X_MINOR_ARCH) \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-IAzureRcmLib \
	-ICommitTagLib \
	$(ACE_INCLUDE) \
	$(CURL_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(INM_MD5_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ESJSON_INCLUDES) \


X_LIBS =

X_THIRDPARTY_LIBS =

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)appframeworklib
BINARY_EXT=$(X_LIBEXT)

include bottom.mak


