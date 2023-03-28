include top.mak

SRCS = 	sentinel.cpp \
	$(X_MAJOR_ARCH)/sentinelmajor.cpp \
	domain/prismvolume.cpp \
	domain/$(X_MAJOR_ARCH)/prismvolumemajor.cpp \

X_CFLAGS =  \
	-Wcast-align \
	-Ivalidatesvdfile \
	-Inpwwn \
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
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	-Izflate \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	-Iscopeguard \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Ifio \
	-Ifio/$(X_MINOR_ARCH) \
	-Ifio/$(X_MAJOR_ARCH) \
	-Icxtransportlib \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Is2-cs \
	-Is2-cs/domain\
	-Ibackups2lib-cs/domain \
	-Is2libs/foundation \
	-Is2libs/foundation/$(X_MAJOR_ARCH) \
	-Is2libs/foundation/$(X_MINOR_ARCH) \
	-Is2libs/common \
	-Is2libs/thread \
	-Is2libs/streams \
	-Ital \
	-Ilog \
	-Iconversionlib \
	-Is2libs/common \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Ivacplibs \
	-Ibackups2lib-cs \
	-Icdpapplylib \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3  \
	-Idplibs \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Iazureagentapi \
	-Iazureagentapi/$(X_MAJOR_ARCH) \
	-Igenericprofiler \
	-IAzureRecoveryLib/resthelper \
	$(ACE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(CURL_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(INM_MD5_INCLUDE) \
	$(ESJSON_INCLUDES) \

X_LIBS = \
    npwwn \
	validatesvdfile \
	common \
	s2libs \
    config \
	cxtransportlib \
	tal \
	config \
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
	backups2lib-cs \
	AzureRecoveryLib \
	cdpapplylib \
        curlwrapperlib \
        securitylib \
		simplelogger \
		genericprofiler

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

BINARY=s2
BINARY_EXT=$(X_EXEEXT)

include bottom.mak

