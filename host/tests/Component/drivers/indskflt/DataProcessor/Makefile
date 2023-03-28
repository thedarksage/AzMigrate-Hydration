include top.mak

SRCS = \
	DataProcessor.cpp 

X_CFLAGS = \
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
	-Itests/Component/drivers/indskflt/DataProcessor \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.IoctlController/unix \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.ExlcudeBlocks \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.ExlcudeBlocks \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.MD5ChecksumProvider \
	-Itests/Component/drivers/indskflt/inm_md5 \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.HoldIoChecksumConsistencyProvider \
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

BINARY=$(X_LIBPRE)DataProcessor
BINARY_EXT=$(X_LIBEXT)

include bottom.mak
