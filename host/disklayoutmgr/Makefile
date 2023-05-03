include top.mak

SRCS =  \
	dlcommon.cpp \
	dldiscover.cpp  \
	dlmapi.cpp  \
	dlrestore.cpp \
	dllinux.cpp \
	dlwindows.cpp \
	dlvdshelper.cpp \
	dlmdiskinfo.cpp

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
	-Idisklayoutmgr \
    -Iinmsafecapis \
    -Iinmsafecapis/$(X_MAJOR_ARCH) \
	$(CURL_INCLUDE) \
	$(ACE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(INM_MD5_INCLUDE) \
	$(SIGSLOT_INCLUDE) \

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)disklayoutmgr
BINARY_EXT=$(X_LIBEXT)

include bottom.mak
