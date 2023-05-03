include top.mak

SRCS =  \
        EventGenerator.cpp \
        ExecutionController.cpp \
        ExecutionEngine.cpp \
        ExecutionObject.cpp \
        FileRepManager.cpp \
        SnapshotManager.cpp \
        main.cpp \
        thread.cpp \
        Logger.cpp \
        util.cpp 
        

X_CFLAGS = \
        -Iconfig \
        -Icommon \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Isecuritylib \
	-Isecuritylib/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	-Iscopeguard \
	-Icurlwrapperlib \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
        -Icommon/$(X_MAJOR_ARCH) \
        -Icommon/$(X_MINOR_ARCH) \
    -Iinmsafecapis \
    -Iinmsafecapis/$(X_MAJOR_ARCH) \
     $(ACE_INCLUDE) \
     $(BOOST_INCLUDE) \
     $(CURL_INCLUDE) \
     $(OPENSSL_INCLUDE) \
     $(LIBXML2_INCLUDE)

X_LIBS = \
        config \
        curlwrapperlib \
        securitylib
X_THIRDPARTY_LIBS = \
        $(ARES_LIBS) \
        $(CURL_LIBS) \
        $(ACE_LIBS) \
        $(ZLIB_LIBS) \
        $(LIBSSH2_LIBS) \
        $(OPENSSL_LIBS) \
        $(LIBXML2_LIBS) \
	$(BOOST_LIBS)

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=scheduler
BINARY_EXT=$(X_EXEEXT)

include bottom.mak

