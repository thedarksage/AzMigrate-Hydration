include top.mak
SRCS = \
	cdpcli.cpp \
	cdpdatabase.cpp \
	cdpcoalesce.cpp \
	$(X_MAJOR_ARCH)/cdpmajor.cpp \
	cdplock.cpp \
	cdputil.cpp \
	svdrtd.cpp \
	cdpwin.cpp \
	differentialfile.cpp \
	../cdpapplylib/diffsortcriterion.cpp \
	cdpdatafilereader.cpp \
	inmemorydifffile.cpp \
	cdpdatabaseimpl.cpp \
	decompress.cpp \
	svmarker.cpp \
	cdpsnapshot.cpp \
	cdpvalidate.cpp \
	cdpfixdb.cpp \
	cdprealvolumeimpl.cpp \
	cdpemulatedvolumeimpl.cpp \
	cdpdiskimpl.cpp \
	cdpvolume.cpp \
	cdpvolumeutil.cpp \
	simplevolumeapplier.cpp \
        snapshotrequestprocessor.cpp \
	cdpredologfile.cpp \
	flushandholdwritetxn.cpp \
	cdp_resync_txn.cpp \
	$(X_MINOR_ARCH)/cdpminor.cpp \
	$(X_MAJOR_ARCH)/cdpsnapshotmajor.cpp \
	$(X_MAJOR_ARCH)/cdpclimajor.cpp \
	$(X_MAJOR_ARCH)/cdpvolumemajor.cpp \
	v1/cdpv1database.cpp \
	v1/cdpv1databaseimpl.cpp \
	v1/cdpv1datafilereader.cpp \
	v1/cdpv1fixdbimpl.cpp \
	v1/cdpv1validateimpl.cpp \
	v3/cdpv3bitmap.cpp \
	v3/cdpv3transaction.cpp \
	v3/cdpv3databaseimpl.cpp \
	v3/cdpv3metadatafile.cpp \
	v3/cdpv3validateimpl.cpp \
	v3/cdpv3fixdbimpl.cpp \
	v3/cdpv3metadatareadahead.cpp \
	$(X_MAJOR_ARCH)/registermtmajor.cpp

X_CFLAGS = \
	-DVSNAP_USER_MODE \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-I$(X_ARCH)/config \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Ilog \
	-IInmXmlParser \
	-Ital \
	-Iconversionlib \
	-Is2libs \
	-Is2libs/common \
	-Is2libs/foundation \
	-Is2libs/thread  \
	-Icdplibs -Icdplibs/$(X_MAJOR_ARCH) -Icdplibs/v1 -Icdplibs/v3  \
	-Ivacplibs \
	-Icdplibs/$(X_MINOR_ARCH) \
	-Ivsnaplib \
	-Ivirtualvolumelib/unix \
	-Idrivers/Include/linux \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/InVirVol/$(X_MINOR_ARCH) \
	-Idrivers/InVirVol/common \
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
	-Idisklayoutmgr \
	-Icdpapplylib \
     -Iinmsafecapis \
     -Iinmsafecapis/$(X_MAJOR_ARCH) \
     -Isecuritylib \
     -Isecuritylib/$(X_MAJOR_ARCH) \
     -Ierrorexception \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
   -Icurlwrapperlib \
   $(OPENSSL_LIBS) \
	$(ACE_INCLUDE) \
	$(CURL_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(OPENSSL_INCLUDE) \


X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)cdplibs
BINARY_EXT=$(X_LIBEXT)

include bottom.mak


