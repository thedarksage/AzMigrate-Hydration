include top.mak

SRCS = \
	BlockDevice/DiskDevice.cpp \
	BlockDevice/$(X_MAJOR_ARCH)/DiskDeviceMajor.cpp \
	BlockDevice/$(X_MINOR_ARCH)/DiskDeviceMinor.cpp \
	BlockDevice/$(X_MAJOR_ARCH)/FileDevice.cpp \
	PlatformAPIs/$(X_MAJOR_ARCH)/UnixAPIs.cpp \
	InmageTestException/InmageTestException.cpp \
	Logger/Logger.cpp \
	Logger/$(X_MAJOR_ARCH)/LoggerMajor.cpp

X_CFLAGS = \
	-Icommon \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Itests/common/BlockDevice \
	-Itests/common/BlockDevice/$(X_MAJOR_ARCH) \
	-Itests/common/PlatformAPIs \
	-Itests/common/PlatformAPIs/$(X_MAJOR_ARCH)\
	-Itests/common/InmageTestException \
	-Itests/common/InmageTestException/$(X_MAJOR_ARCH) \
	-Itests/common \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Itests/common/Logger \
	-Itests/common/Logger/$(X_MAJOR_ARCH) \
	$(BOOST_INCLUDE) \
	$(ACE_INCLUDE)

X_LIBS =

X_THIRDPARTY_LIBS =

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)tests_common
BINARY_EXT=$(X_LIBEXT)

include bottom.mak
