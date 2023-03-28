include top.mak

SRCS = \
	cxpsclient.cpp \
	$(X_MAJOR_ARCH)/waitforquit.cpp

X_CFLAGS = \
	-Icxpsclient \
	-Icxpsclient/$(X_MAJOR_ARCH) \
	-Icxtransportlib \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Ifio \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Ifio/$(X_MINOR_ARCH) \
	-Ifio/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Iscopeguard \
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	-Izflate \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Icommon \
   -Iinmsafeint \
   -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Iconversionlib \
	-Ilog \
	-Is2libs \
	-Is2libs/common \
	-Is2libs/foundation \
	-Is2libs/foundation/$(X_MAJOR_ARCH) \
	-Is2libs/thread \
	-Is2libs/streams \
	-Ital \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Isecuritylib \
	-Isecuritylib/$(X_MINOR_ARCH) \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-IAzureRecoveryLib/resthelper \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ACE_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(CURL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \

X_LIBS = \
   dplibs \
   npwwn \
	common \
	cxtransportlib \
	tal \
	config \
	s2libs \
	log \
	conversionlib \
	cdplibs \
	volumeinfocollector \
   deviceidinformer \
   platforminfo \
   executecommand \
   scsicommandissuer \
   dirbasename \
   appdevicelib \
	vsnaplib \
	vacplibs \
   cdpapplylib \
   virtualvolumelib \
	securitylib \
	AzureRecoveryLib \
   curlwrapperlib

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

BINARY=cxpsclient
BINARY_EXT=$(X_EXEEXT)

include bottom.mak


