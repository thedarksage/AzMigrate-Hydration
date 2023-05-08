include top.mak

SRCS =  \
	main.cpp \
	cachemgr.cpp \
	../cdpapplylib/diffsortcriterion.cpp

#-DDP_SENDFILE_ON
X_CFLAGS = \
	-DDP_TRACKING_ON \
	-DDP_PORTABLE \
	-DBASIC_TEST \
	-DVSNAP_USER_MODE \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Ifio \
	-Ifio/$(X_MINOR_ARCH) \
	-Ifio/$(X_MAJOR_ARCH) \
	-Icxtransportlib \
	-Ierrorexception \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Iscopeguard \
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	-Izflate \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Ilog \
	-Ital \
	-ISnapShotMgr/include \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3 \
	-IRetention \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Is2libs \
	-Is2libs/common \
	-Is2libs/foundation \
	-Is2libs/foundation/$(X_MAJOR_ARCH) \
	-Is2libs/thread \
	-Is2libs/streams \
	-Icdpapplylib \
	-Idataprotection \
	-Ivalidatesvdfile \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-IAzureRecoveryLib/resthelper \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ACE_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(CURL_INCLUDE) \

X_LIBS = \
    dplibs \
    cdpapplylib \
    npwwn \
	validatesvdfile \
	common \
	cxtransportlib \
	tal \
	config \
	s2libs \
	log \
	cdplibs \
	vsnaplib \
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
	AzureRecoveryLib \
        securitylib

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

BINARY=cachemgr
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
