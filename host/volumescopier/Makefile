include top.mak
SRCS = \
	volumescopier.cpp \
	volumescopierreader.cpp \
	volumescopierwriter.cpp \
	volumescopiertask.cpp \

X_CFLAGS = \
	-DDP_TRACKING_ON \
	-DDP_PORTABLE \
    -DBASIC_TEST \
    -DVSNAP_USER_MODE \
    -Isimplelogger \
    -Isimplelogger/$(X_MAJOR_ARCH) \
    -Icdpapplylib \
    -Iinmpipeline \
    -Izflate \
    -Ilistfile \
    -Ilistfile/$(X_MAJOR_ARCH) \
    -Ierrorexception \
    -Iscopeguard \
    -Ivolumescopier \
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
    -Ilog \
    -Iconversionlib \
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
    -Idplibs \
    -Ivacplibs \
    -Iinmsafecapis \
    -Iinmsafecapis/$(X_MAJOR_ARCH) \
    $(SQLITE_INCLUDE) \
    $(BOOST_INCLUDE) \
    $(ACE_INCLUDE) \
    $(OPENSSL_INCLUDE) \
    $(ZLIB_INCLUDE) \
    $(CURL_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(INM_MD5_INCLUDE) \

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)volumescopier
BINARY_EXT=$(X_LIBEXT)

include bottom.mak


