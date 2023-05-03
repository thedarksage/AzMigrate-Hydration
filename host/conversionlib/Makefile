include top.mak
SRCS =  \
    conversionexception.cpp \
    convertorfactory.cpp \
    standardtypeconvertor.cpp \
    emptyconvertor.cpp \
    hashconvertor.cpp \
    svdconvertor.cpp \
    $(X_MAJOR_ARCH)/svdconvertormajor.cpp \

X_CFLAGS = \
    -Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
    -Icommon/$(X_MAJOR_ARCH) \
    -Iconfig \
    -Iconfig/$(X_MAJOR_ARCH) \
    -I$(X_ARCH)/config \
    -Idplibs \
    -Idataprotection \
    -Iconversionlib \
    $(ACE_INCLUDE) \
    $(BOOST_INCLUDE) \
    $(ZLIB_INCLUDE) \

X_LIBS =

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)conversion
BINARY_EXT=$(X_LIBEXT)

include bottom.mak
