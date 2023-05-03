include top.mak

SRCS =  \
	main.cpp \
	$(X_MINOR_ARCH)/mainminor.cpp \

X_CFLAGS = \
	-Ivacplibs \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
    -Icommon/$(X_MINOR_ARCH) \
    -Ivolumeinfocollector \
    -Idrivers/InVolFlt/common

X_LIBS = \
	vacplibs \
	conversionlib 

X_THIRDPARTY_LIBS =  \

X_SYSTEM_LIBS = -luuid

X_LDFLAGS = 

include $(X_MODULE)/$(X_MINOR_ARCH)/Makefile
BINARY=vacp
BINARY_EXT=$(X_EXEEXT)

include bottom.mak

