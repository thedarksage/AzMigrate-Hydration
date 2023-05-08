include top.mak

SRCS =  hostconfigcurses.cpp \

X_CFLAGS = \
	-DHAVE_CONFIG_H \
	-D_GNU_SOURCE \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	-Ierrorexception/$(X_MINOR_ARCH) \
	-Ifio \
	-Ifio/$(X_MAJOR_ARCH) \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Iscopeguard \
	-Ilog \
	-Ibranding \
	-Is2libs/thread \
	-I$(X_ARCH)/config \
	-IAzureRecoveryLib/resthelper \
	$(ACE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(CDK_INCLUDE) \
	$(CURL_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
    $(INM_MD5_INCLUDE) \
	$(ZLIB_INCLUDE)

X_LIBS = \
	dplibs \
	cdpapplylib \
	common \
	config \
	s2libs \
	vsnaplib \
	vacplibs \
	log \
	volumeinfocollector \
    deviceidinformer \
    platforminfo \
    executecommand \
    scsicommandissuer \
    dirbasename \
    appdevicelib \
	cdplibs \
	conversionlib \
	tal \
	cxtransportlib \
	npwwn \
	virtualvolumelib \
        curlwrapperlib \
	AzureRecoveryLib \
        securitylib

X_THIRDPARTY_LIBS = \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(CDK_LIBS) \
	$(BOOST_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
	$(SQLITE_LIBS) \
	$(INM_MD5_LIBS)

X_SYSTEM_LIBS = \
	$(LIBCURSES) \
	$(LIBTERMCAP)

X_LDGLAGS = 

BINARY=hostconfigcli
BINARY_EXT=$(X_EXEEXT)

include bottom.mak

