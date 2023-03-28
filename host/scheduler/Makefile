include top.mak
SRCS = \
	appscheduler.cpp \
	taskexecution.cpp \
	util.cpp


X_CFLAGS = \
	-DVSNAP_USER_MODE \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Iconfig \
	-Iconfig/$(X_MAJOR_ARCH) \
	-Ilog \
	-Ital \
	-Is2libs \
	-Is2libs/common \
	-Is2libs/foundation \
	-Is2libs/thread  \
	-Icdplibs -Icdplibs/v1 -Icdplibs/v2  \
	-Ivacplibs \
	-Ivsnaplib \
	-Ischeduler \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	$(ACE_INCLUDE) \
	$(CURL_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ESJSON_INCLUDES) \

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)scheduler
BINARY_EXT=$(X_LIBEXT)

include bottom.mak


