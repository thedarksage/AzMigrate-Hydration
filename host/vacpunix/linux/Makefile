
SRCS =  \
	main.cpp \
	../vacp/Communicator.cpp \
	$(X_MINOR_ARCH)/mainminor.cpp \
	apps/ApplicationConsistencyController.cpp \
	apps/oracle/OracleConsistencyProvider.cpp \
	apps/db2/Db2ConsistencyProvider.cpp \
	apps/mysql/MysqlConsistencyProvider.cpp \
	apps/custom/CustomConsistencyProvider.cpp \
	../vacp/VacpLogger.cpp \
	../vacp/SecureTransport.cpp \

X_CFLAGS = \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
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
	-Ivacplibs \
	-Icommon \
	-Iinmsafeint \
	-Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Ilog \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3 \
	-Idrivers/InVolFlt \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
    -Ivacpunix \
    -Ivacpunix/apps \
    -Ivacpunix/apps/oracle \
    -Ivacpunix/apps/db2 \
    -Ivacpunix/apps/mysql \
    -Ivacpunix/apps/custom \
    -Ivacp \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Iscopeguard \
	-Ierrorexception \
	-Icxpslib/$(X_MINOR_ARCH) \
    -I/opt/VRTS/include \
    -Is2libs/thread \
    -Is2libs/foundation\
    -Icxpslib \
    -IAzureRecoveryLib \
    -IAzureRecoveryLib/resthelper \
    -IAzureRcmLib \
    -IAzureADAuthProviderLib \
	$(ACE_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(CURL_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ESJSON_INCLUDES) \
	$(ZLIB_INCLUDE)

X_LIBS = \
    cxtransportlib \
    npwwn \
	common \
	tal \
	log \
	config \
	s2libs \
	volumeinfocollector \
    deviceidinformer \
    platforminfo \
    executecommand \
    scsicommandissuer \
    dirbasename \
    appdevicelib \
	cdplibs \
	vsnaplib \
	vacplibs \
	conversionlib \
	curlwrapperlib \
	securitylib \
	AzureRecoveryLib \
	AzureADAuthProviderLib


X_THIRDPARTY_LIBS = \
	$(CURL_LIBS) \
	$(ACE_LIBS) \
	$(SQLITE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
	$(BOOST_LIBS) \
	$(INM_MD5_LIBS)

X_LDFLAGS = 

BINARY=vacp
BINARY_EXT=$(X_EXEEXT)


