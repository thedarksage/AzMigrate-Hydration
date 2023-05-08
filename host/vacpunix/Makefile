include top.mak

SRCS =  \
	main.cpp \
	$(X_MINOR_ARCH)/mainminor.cpp \

X_CFLAGS = \
	-Ivacplibs \
	-Icxpslib \
	-Icommon \
    -Iinmsafeint \
    -Isecuritylib \
    -Isecuritylib/$(X_MAJOR_ARCH) \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
    -Icommon/$(X_MINOR_ARCH) \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
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
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
    -Ivolumeinfocollector \
    -Idevicefilter \
    -Idevicefilter/$(X_MAJOR_ARCH) \
    -Idrivers/InVolFlt/common \
    -I $(ACE_INCLUDE)

X_LIBS = \
	vacplibs \
	config \
	conversionlib \
        curlwrapperlib \
        securitylib \
    $(ACE_LIBS)


X_THIRDPARTY_LIBS =  \

X_SYSTEM_LIBS = -luuid

X_LDFLAGS = -lzstd 

include $(X_MODULE)/$(X_MINOR_ARCH)/Makefile
BINARY=vacp
BINARY_EXT=$(X_EXEEXT)

include bottom.mak

