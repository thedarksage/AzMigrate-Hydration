include top.mak

SRCS = \
	domain/protectedgroupvolumes.cpp \
	domain/$(X_MINOR_ARCH)/protectedgroupvolumesminor.cpp \
	domain/$(X_MAJOR_ARCH)/protectedgroupvolumesmajor.cpp \
	domain/$(X_MAJOR_ARCH)/stopvolumegroupsmajor.cpp \
	domain/volumemanager.cpp \
	domain/devmetrics.cpp \

X_CFLAGS =  \
	-Ivalidatesvdfile \
	-Inpwwn \
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
	-Is2/domain\
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
	-Icdpapplylib \
	-Idplibs \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3  \
	-Ibackups2lib/domain \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Iazureagentapi \
	-Iazureagentapi/$(X_MAJOR_ARCH) \
	-IAzureRcmLib \
	-ICommitTagLib \
	-IAzureRecoveryLib \
	-IAzureRecoveryLib/resthelper \
	-Igenericprofiler \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	$(ACE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(CURL_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(INM_MD5_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ESJSON_INCLUDES) \
	$(LIBXML2_INCLUDE) \

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)backups2lib
BINARY_EXT=$(X_LIBEXT)

include bottom.mak

