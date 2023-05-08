include top.mak

SRCS =  \
        configmerge.cpp \
       	configmergetool.cpp

X_CFLAGS = \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Icommon \
	-Icommon/$(X_MAJOR_ARCH) \
	$(ACE_INCLUDE) \
        $(BOOST_INCLUDE) \

X_LIBS = 

X_THIRDPARTY_LIBS =   \
        $(ACE_LIBS) \
    $(BOOST_LIBS)


X_SYSTEM_LIBS =

X_LDFLAGS = 

BINARY=configmergetool
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
