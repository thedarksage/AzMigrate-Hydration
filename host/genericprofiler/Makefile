include top.mak


SRCS = \
	genericprofiler.cpp \

X_CFLAGS = \
	-Igenericprofiler \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	-Ifio \
	-Ifio/$(X_MINOR_ARCH) \
	-Ifio/$(X_MAJOR_ARCH) \
	-Ilistfile \
	-Ilistfile/$(X_MAJOR_ARCH) \
	-Iscopeguard \
	-Izflate \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Icdplibs \
	-Isecuritylib \
	-Isecuritylib/$(X_MINOR_ARCH) \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Ilog \
	-Isimplelogger \
	-Isimplelogger/$(X_MAJOR_ARCH) \
	$(BOOST_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ACE_INCLUDE) \
	$(ESJSON_INCLUDES) \

X_LIBS =

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)genericprofiler
BINARY_EXT=$(X_LIBEXT)

include bottom.mak


