include top.mak

SRCS = \
	main.cpp

X_CFLAGS = \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	$(OPENSSL_INCLUDE) \

X_LIBS = 

X_THIRDPARTY_LIBS = \
	$(OPENSSL_LIBS) \

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=inmcrypt
BINARY_EXT=$(X_EXEEXT)

include bottom.mak


