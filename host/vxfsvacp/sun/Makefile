

SRCS =  \
	main.cpp \
	$(X_MINOR_ARCH)/mainminor.cpp \

X_CFLAGS = \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Ivolumeinfocollector \
	-Ivolumeinfocollector/$(X_MAJOR_ARCH) \
	-Ivolumeinfocollector/$(X_MINOR_ARCH) \
	-Ivacplibs \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Ilog \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v3 \
	-Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	$(ACE_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(CURL_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(ZLIB_INCLUDE)

X_LIBS = \
    npwwn \
	common \
	tal \
	log \
	config \
	s2libs \
	volumeinfocollector \
    appdevicelib \
	cdplibs \
	vsnaplib \
	vacplibs \
	conversionlib \

X_THIRDPARTY_LIBS = \
	$(CURL_LIBS) \
	$(ACE_LIBS) \
	$(SQLITE_LIBS) \
	$(ARES_LIBS) \
	$(CURL_LIBS) \
	$(OPENSSL_LIBS) \
	$(ZLIB_LIBS) \
	$(BOOST_LIBS)

X_LDFLAGS = 

BINARY=vacp
BINARY_EXT=$(X_EXEEXT)



