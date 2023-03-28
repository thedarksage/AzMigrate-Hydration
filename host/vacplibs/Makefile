include top.mak

SRCS = StreamEngine.cpp \
		VacpUtil.cpp

X_CFLAGS  = \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Iconversionlib \
	 -Icommon/$(X_MAJOR_ARCH) \
	$(BOOST_INCLUDE)

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)vacp
BINARY_EXT=$(X_LIBEXT)

include bottom.mak
