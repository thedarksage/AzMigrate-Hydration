include top.mak

SRCS = \
	DataProcessor/DataProcessor.cpp \
	Microsoft.Test.Inmage.IoctlController/$(X_MAJOR_ARCH)/InmageIoctlController.cpp \
	Microsoft.Test.Inmage.ReplicationManager/ReplicationManager.cpp \
	Microsoft.Test.Inmage.MD5ChecksumProvider/ChecksumProvider.cpp \
	Microsoft.Test.Inmage.ReplicationManager/ReplicationWorker.cpp \
	Microsoft.Test.Inmage.ReplicationManager/SegStream.cpp \
	Microsoft.Test.Inmage.ReplicationManager/SVDStream.cpp \
	Microsoft.Test.Inmage.ReplicationManager/Validations.cpp \
	S2Agent/S2Agent.cpp 

X_CFLAGS = \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Itests/common/BlockDevice \
	-Itests/common/BlockDevice/$(X_MAJOR_ARCH) \
	-Itests/common/PlatformAPIs \
	-Itests/common/PlatformAPIs/$(X_MAJOR_ARCH)\
	-Itests/common/InmageTestException \
	-Itests/common/InmageTestException/$(X_MAJOR_ARCH) \
	-Itests/common \
	-Icommon \
	-Icommon/$(X_MAJOR_ARCH)\
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.ReplicationManager \
	-Itests/Component/drivers/indskflt/S2Agent \
	-Itests/Component/drivers/indskflt/S2Agent/$(X_MAJOR_ARCH) \
	-Itests/Component/drivers/indskflt/DataProcessor \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.IoctlController/$(X_MAJOR_ARCH) \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.ExlcudeBlocks \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.ExlcudeBlocks \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.MD5ChecksumProvider \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.HoldIoChecksumConsistencyProvider \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Itests/common/Logger \
	-Itests/common/Logger/$(X_MAJOR_ARCH) \
	-Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	$(BOOST_INCLUDE) \
	$(INM_MD5_INCLUDE) \
	$(ACE_INCLUDE)

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)tests_component_drivers
BINARY_EXT=$(X_LIBEXT)

include bottom.mak
