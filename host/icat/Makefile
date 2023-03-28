include top.mak

SRCS =  \
	archivecontroller.cpp \
	archivefactory.cpp \
	configvalueobj.cpp \
	common.cpp \
	$(X_MAJOR_ARCH)/commonmajor.cpp \
	filefilter.cpp \
	filelister.cpp \
	filesystemobj.cpp \
	hcaphttparchiveobj.cpp \
	hcaparcprocess.cpp \
	filelistprocess.cpp \
	hcapcifsnfsarchiveobj.cpp \
	httptransport.cpp \
	icatexception.cpp \
	icatqueue.cpp \
	main.cpp \
	parser.cpp \
	resumetracker.cpp \
	connection.cpp \

X_CFLAGS = \
	-Is2libs/thread \
	-Iconfig \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3 \
	-Iicat \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Ilog \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ACE_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(CURL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \

X_LIBS = \
    dplibs \
    cdpapplylib \
    npwwn \
	cdplibs \
	config \
	common \
	log \
	s2libs \
	vacplibs \
 	vsnaplib \
	volumeinfocollector \
    deviceidinformer \
    platforminfo \
    executecommand \
    scsicommandissuer \
    dirbasename \
    appdevicelib \
	conversionlib \
    cxtransportlib \
    virtualvolumelib \
    tal \
    curlwrapperlib \
    securitylib


X_THIRDPARTY_LIBS = \
	$(SQLITE_LIBS) \
	$(ACE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
    $(BOOST_LIBS) \
    $(INM_MD5_LIBS) \

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=icat
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
