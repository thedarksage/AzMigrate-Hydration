include top.mak

SRCS = \
	$(X_MAJOR_ARCH)/hosttotargetmajor.cpp \
	targetupdaterimp.cpp \
	$(X_MAJOR_ARCH)/targetupdaterimpmajor.cpp 

X_CFLAGS = \
	-Ihosttotarget \
	-Ihosttotarget/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	-Iscopeguard \
	$(BOOST_INCLUDE) \

X_LIBS =

X_THIRDPARTY_LIBS =

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)hosttotargetlib
BINARY_EXT=$(X_LIBEXT)

include bottom.mak


