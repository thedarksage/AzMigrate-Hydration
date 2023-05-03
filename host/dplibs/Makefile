include top.mak
SRCS = \
	bitmap.cpp \
	svbitset.cpp \

X_CFLAGS = \
	-DVSNAP_USER_MODE \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Ifio \
	-Ifio/$(X_MINOR_ARCH) \
	-Ifio/$(X_MAJOR_ARCH) \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	-Izflate \
	-Iscopeguard \
	-Icxtransportlib \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Ilog \
	-Ital \
	-Is2libs \
	-Is2libs/common \
	-Is2libs/foundation \
	-Is2libs/thread  \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3  \
	-Ivacplibs \
	-Ivsnaplib \
	-Ivirtualvolumelib/unix \
	-Idrivers/Include/linux \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Idrivers/InVirVol/linux \
	-Icdpapplylib \
	-Idataprotection \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	$(ACE_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(INM_MD5_INCLUDE) \

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)dplibs
BINARY_EXT=$(X_LIBEXT)

include bottom.mak


