include top.mak

SRCS =  \
	sendquit.cpp \

#-DDP_SENDFILE_ON 
X_CFLAGS = \
	-Is2libs/thread \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Icommon \
	$(ACE_INCLUDE) \
	$(BOOST_INCLUDE) \

X_LIBS = 

X_THIRDPARTY_LIBS = \
	$(BOOST_LIBS) \
	$(ACE_LIBS)

X_SYSTEM_LIBS = \
	-lrt \
	-ldl

X_LDFLAGS =

BINARY=sendquit
BINARY_EXT=$(X_EXEEXT)

include bottom.mak
