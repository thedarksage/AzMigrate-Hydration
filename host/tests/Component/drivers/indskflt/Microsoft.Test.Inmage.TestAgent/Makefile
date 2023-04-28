include top.mak

ARCH = $(shell uname)

SRCS = \
	main.cpp


X_CFLAGS = \
	-Icommon \
	-Icommon/$(X_MAJOR_ARCH) \
	-Itests/common \
	-Itests/common/$(X_MAJOR_ARCH) \
	-Idrivers/lib/$(X_MINOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Idrivers/InvolFlt \
	-Idrivers/InVolFlt/common \
	-Itests/common/BlockDevice \
	-Itests/common/BlockDevice/$(X_MAJOR_ARCH) \
	-Itests/common/PlatformAPIs \
	-Itests/common/PlatformAPIs/$(X_MAJOR_ARCH) \
	-Itests/common/InmageTestException \
	-Itests/common/InmageTestException/$(X_MAJOR_ARCH) \
	-Itests/common/Logger \
	-Itests/common/Logger/$(X_MAJOR_ARCH) \
	-Itests/Component/drivers/indskflt/S2Agent \
	-Itests/Component/drivers/indskflt/S2Agent/$(X_MAJOR_ARCH) \
	-Itests/Component/drivers/indskflt/DataProcessor \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.ReplicationManager \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.IoctlController/$(X_MAJOR_ARCH) \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.MD5ChecksumProvider \
	-Itests/Component/drivers/indskflt/Microsoft.Test.Inmage.ExcludeBlocks \
	-Idrivers/InVolFlt/common \
	-Idrivers/InVolFlt/$(X_MINOR_ARCH) \
	-Is2libs/thread \
	$(BOOST_INCLUDE)  \
	$(ACE_INCLUDE) \
	$(INM_MD5_INCLUDE) \

X_LIBS = \
	common \
	tests_common \
	tests_component_drivers

X_THIRDPARTY_LIBS = \
	$(ACE_LIBS) \
	$(BOOST_LIBS) \
	$(INM_MD5_LIBS)

X_SYSTEM_LOBS =

X_LDFLAGS =

BINARY=indskflt_ct
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
