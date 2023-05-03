include top.mak
SRCS =  \
    validatesvdfile.cpp \

X_CFLAGS = \
    -Ivalidatesvdfile \
    -Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
    -Icommon/$(X_MAJOR_ARCH) \
    -Iconfig \
    -Iconfig/$(X_MAJOR_ARCH) \
    -I$(X_ARCH)/config \
    -Idplibs \
    -Iconversionlib \
    -Idataprotection \
    -Iinmsafecapis \
    -Iinmsafecapis/$(X_MAJOR_ARCH) \
    $(ACE_INCLUDE) \
    $(ZLIB_INCLUDE) \
    $(BOOST_INCLUDE) \
	$(INM_MD5_INCLUDE) \

X_LIBS =

X_THIRDPARTY_LIBS =  \
    $(ZLIB_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)validatesvdfile
BINARY_EXT=$(X_LIBEXT)

include bottom.mak
