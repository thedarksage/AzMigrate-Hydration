include top.mak
SRCS = \
	$(X_MAJOR_ARCH)/remoteconnectionmajor.cpp \
	$(X_MAJOR_ARCH)/unixremoteconnectionmajor.cpp \
	unixremoteconnection.cpp \
	remoteapi.cpp

X_CFLAGS = \
   -Iremoteapilib -Iremoteapilib/$(X_MAJOR_ARCH) -Iremoteapilib/$(X_MINOR_ARCH) \
   -Ierrorexception \
   -Ierrorexception/$(X_MINOR_ARCH) \
   -Ierrorexception/$(X_MAJOR_ARCH) \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
   -Icurlwrapperlib \
   -Icxpsclient/$(X_MAJOR_ARCH) \
   -Iscopeguard \
   -Icommon -Icommon/$(X_MAJOR_ARCH) -Iconfig \
   -Ilog \
	$(ACE_INCLUDE) \
	$(CURL_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(BOOST_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ZLIB_INCLUDE) \
	$(OPENSSL_INCLUDE) \
	$(LIBSSH2_INCLUDE)


X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)remoteapi
BINARY_EXT=$(X_LIBEXT)

include bottom.mak


