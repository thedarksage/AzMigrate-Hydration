include top.mak

SRCS = DITestUtil.cpp \
	$(X_MAJOR_ARCH)/DITestUtilMajor.cpp \
	

X_CFLAGS = \
   -Wcast-align \
	-Itests/common/Logger \
	-Itests/common/Logger/$(X_MAJOR_ARCH) \
	-Itests/common/ILogger \
	-Itests/common/Machine \
	-Itests/common/Storage \
	-Itests/common/BlockDevice \
	-Itests/common/BlockDevice/$(X_MAJOR_ARCH) \
	-Itests/common/PlatformAPIs \
	-Itests/common/PlatformAPIs/$(X_MAJOR_ARCH)\
	-Itests/common/ConsistencyPoint \
	-Itests/common/ProcessProvider \
	-Itests/common/VacpDefines \
	-Itests/common/VacpDefines/$(X_MAJOR_ARCH)\
	-Itests/common/Snapshot \
	-Ierrorexception \
	-Itests/common \
	-Icommon \
	-Icommon/$(X_MAJOR_ARCH)\
	-Itests/common/InmageTestException \
	-Itests/common/ConsistencyProvider \
	-Itests/common/SnapshotProvider \
	-Itests/common/ChecksumProvider \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH)\
	$(BOOST_INCLUDE) \
	$(ACE_INCLUDE) \
	$(INM_MD5_INCLUDE) \

X_LIBS = \
	tests_common

X_THIRDPARTY_LIBS = \
	$(BOOST_LIBS) \
	$(ACE_LIBS) \
	$(INM_MD5_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=DITestUtil
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
