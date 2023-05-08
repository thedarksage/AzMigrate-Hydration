include top.mak
SRCS = \
        AixFullSystemProtection.cpp

X_CFLAGS = \
        $(CURL_INCLUDE) \
        $(ZLIB_INCLUDE) \
        $(LIBXML2_INCLUDE) \
	-Icommon \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	-Icxpslib \
	-Icxpslib/$(X_MINOR_ARCH) \
	-Icxpslib/$(X_MAJOR_ARCH) \
	-Icurlwrapperlib \
	$(OPENSSL_INCLUDE) \

X_THIRDPARTY_LIBS = \
        $(ARES_LIBS) \
        $(CURL_LIBS) \
        $(OPENSSL_LIBS) \
        $(ZLIB_LIBS) \
        $(LIBXML2_LIBS) \
		$(BOOST_LIBS)

BINARY=p2vutil
BINARY_EXT=$(X_EXEEXT)

X_LDFLAGS := -Wl,-bstatic 
X_SYSTEM_LIBS := -Wl,-bdynamic

include bottom.mak
