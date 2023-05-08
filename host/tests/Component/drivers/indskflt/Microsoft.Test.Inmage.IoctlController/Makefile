include top.mak

SRCS = \
	unix/InmageIoctlController.cpp \
	Logger.cpp \
	unix/LoggerMajor.cpp \
	unix/UnixAPIs.cpp

X_CFLAGS = \
	-Itests/common/Logger/$(X_MAJOR_ARCH) \
	-Itests/common/BlockDevice \
	-Itests/common/BlockDevice/$(X_MAJOR_ARCH) \
	-Itests/common/PlatformAPIs \
	-Itests/common/PlatformAPIs/$(X_MAJOR_ARCH)\
	-Itests/common/InmageTestException \
	-Itests/common/InmageTestException/$(X_MAJOR_ARCH) \
	-Itests/common \
	-Icommon \
	-Icommon/$(X_MAJOR_ARCH)\
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.ReplicationManager \
	-Itests/Component/drivers/indskflt/S2Agent \
	-Itests/Component/drivers/indskflt/DataProcessor \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.IoctlController/$(X_MAJOR_ARCH) \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.ExlcudeBlocks \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.ExlcudeBlocks \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.MD5ChecksumProvider \
	-Itests/Component/drivers/indskflt/inm_md5 \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.HoldIoChecksumConsistencyProvider \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	$(BOOST_INCLUDE) \
	$(ACE_INCLUDE)

X_LIBS =

X_THIRDPARTY_LIBS = \
	$(BOOST_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=tests_ioctl_controller
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
