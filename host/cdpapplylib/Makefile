include top.mak
SRCS = \
	cdpapply.cpp \
	cdpresyncapply.cpp \
	cdptargetapply.cpp \
	dataprotectionsync.cpp \
	dataprotectionutils.cpp \
	differentialsync.cpp \
	retentionvolumeapplier.cpp \
	diffinfo.cpp \
	groupinfo.cpp \
	programargs.cpp \
	volumeinfo.cpp \
	diffsortcriterion.cpp \
	cdpv2writer.cpp \
	svproactor.cpp \
	$(X_MAJOR_ARCH)/svproactormajor.cpp \
	$(X_MAJOR_ARCH)/dpdrivercomm.cpp \
    $(X_MINOR_ARCH)/dataprotectionsyncminor.cpp \
    $(X_MAJOR_ARCH)/dataprotectionsyncmajor.cpp \

X_CFLAGS = \
	-DDP_TRACKING_ON \
	-DDP_PORTABLE \
    -DBASIC_TEST \
    -DVSNAP_USER_MODE \
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
    -Isecuritylib \
    -Isecuritylib/$(X_MAJOR_ARCH) \
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
    -Is2libs/foundation/$(X_MINOR_ARCH) \
    -Is2libs/thread \
    -Is2libs/streams \
    -Idplibs \
    -Ivacplibs \
	-Icdpapplylib \
    -Ideviceidinformer \
    -Iplatforminfo \
    -Iplatforminfo/$(X_MAJOR_ARCH) \
    -Iplatforminfo/$(X_MINOR_ARCH) \
    -Iexecutecommand \
    -Iexecutecommand/$(X_MINOR_ARCH) \
    -Iscsicommandissuer \
    -Iscsicommandissuer/$(X_MINOR_ARCH) \
    -Iinmsafecapis \
    -Iinmsafecapis/$(X_MAJOR_ARCH) \
    -Idirbasename \
    -Idirbasename/$(X_MAJOR_ARCH) \
	-Ivolumeinfocollector \
	-Ivolumeinfocollector/$(X_MAJOR_ARCH) \
	-Ivolumeinfocollector/$(X_MINOR_ARCH) \
	-Iazureagentapi \
	-Iazureagentapi/$(X_MAJOR_ARCH) \
	-Igenericprofiler \
    $(SQLITE_INCLUDE) \
    $(BOOST_INCLUDE) \
    $(ACE_INCLUDE) \
    $(OPENSSL_INCLUDE) \
    $(ZLIB_INCLUDE) \
    $(CURL_INCLUDE) \
	$(INM_MD5_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ESJSON_INCLUDES) \

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)cdpapplylib
BINARY_EXT=$(X_LIBEXT)

include bottom.mak


