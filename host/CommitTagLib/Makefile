include top.mak
SRCS = \
	CommitTag.cpp \

X_CFLAGS = \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
    -IAzureRcmLib \
    -ICommitTagLib \
    -Isimplelogger \
    -Isimplelogger/$(X_MAJOR_ARCH) \
    -Icommon \
    -Icommon/$(X_MAJOR_ARCH) \
    -Icommon/$(X_MINOR_ARCH) \
    -Iconfig \
    -Iconfig/$(X_MAJOR_ARCH) \
    -Ilog \
    -Ivacplibs \
    -Is2libs/thread \
    -Is2libs/foundation/$(X_MAJOR_ARCH) \
    -Is2libs/foundation \
    -Is2libs/streams \
    -Is2libs/common \
    -Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
    -Idrivers/InVolFlt/$(X_MINOR_ARCH) \
    -Idrivers/lib/$(X_MINOR_ARCH) \
    -Iinmsafecapis \
    -Iinmsafecapis/$(X_MAJOR_ARCH) \
    -Ivolumeinfocollector \
    -Ivolumeinfocollector/$(X_MAJOR_ARCH) \
    -Ivolumeinfocollector/$(X_MINOR_ARCH) \
    -Ibackups2lib/domain \
    -Icdplibs -Icdplibs/v1 -Icdplibs/v3 \
    -Ivalidatesvdfile \
    -Icdpapplylib \
    -Iexecutecommand \
    -Iexecutecommand/$(X_MINOR_ARCH) \
    -Ivacplibs \
    $(SQLITE_INCLUDE) \
    $(CURL_INCLUDE) \
    $(BOOST_INCLUDE) \
    $(ACE_INCLUDE) \
    $(OPENSSL_INCLUDE) \
    $(SIGSLOT_INCLUDE) \
    $(ZLIB_INCLUDE) \
    $(ESJSON_INCLUDES) \
    $(INM_MD5_INCLUDE)

X_LIBS = \

X_THIRDPARTY_LIBS =  \

X_SYSTEM_LIBS = \

X_LDFLAGS = 

BINARY=$(X_LIBPRE)CommitTagLib
BINARY_EXT=$(X_LIBEXT)

include bottom.mak
