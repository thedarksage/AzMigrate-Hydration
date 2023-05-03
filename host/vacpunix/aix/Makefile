

SRCS =  \
	main.cpp \
	$(X_MINOR_ARCH)/mainminor.cpp \
	apps/ApplicationConsistencyController.cpp \
	apps/oracle/OracleConsistencyProvider.cpp \
	apps/db2/Db2ConsistencyProvider.cpp \
	apps/mysql/MysqlConsistencyProvider.cpp

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
    -Ivacpunix/apps \
    -Ivacpunix/apps/oracle \
    -Ivacpunix/apps/db2 \
    -Ivacpunix/apps/mysql \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3 \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
    -I/opt/VRTS/include \
	$(ACE_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(CURL_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ZLIB_INCLUDE)

X_LIBS = \
    cdpapplylib \
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
	curlwrapperlib \
	securitylib

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



BINARY=vacp
BINARY_EXT=$(X_EXEEXT)



