include top.mak

SRCS = 	inmdistributor.cpp \
		inmqdistributor.cpp 

X_CFLAGS = \
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
	-Iinmdistributor \
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	-Izflate \
	-Is2libs/thread \
	-Is2libs/foundation \
	-Is2libs/foundation/$(X_MINOR_ARCH) \
	-Is2libs/common \
	-Is2libs/streams \
	-Ilog \
	-Inpwwn \
	-Iconfig \
	-Iconfig/unix \
	-Ivolumeinfocollector \
	-Ivolumeinfocollector/$(X_MAJOR_ARCH) \
	-Ivolumeinfocollector/$(X_MINOR_ARCH) \
	-Ital \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3 \
	-Icommon/$(X_MAJOR_ARCH) \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	$(CURL_INCLUDE) \
	$(ACE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE) \

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)inmdistributor
BINARY_EXT=$(X_LIBEXT)

include bottom.mak
