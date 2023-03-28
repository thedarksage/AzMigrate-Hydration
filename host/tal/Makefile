include top.mak

SRCS = \
   HandlerCurl.cpp \

X_CFLAGS = \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
    -Ilog \
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	-Izflate \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	-Iscopeguard \
	-Ifio \
	-Ifio/$(X_MINOR_ARCH) \
	-Ifio/$(X_MAJOR_ARCH) \
	$(CURL_INCLUDE) \
	$(ACE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(OPENSSL_INCLUDE) \
    $(ZLIB_INCLUDE) \

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)tal
BINARY_EXT=$(X_LIBEXT)

include bottom.mak

