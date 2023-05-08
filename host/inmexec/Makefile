include top.mak

SRCS =  inmcmd.cpp inmspecparser.cpp inmexecutioncontroller.cpp inmlogger.cpp main.cpp

X_CFLAGS = \
	$(ACE_INCLUDE) \
	$(BOOST_INCLUDE) \
	-Icommon \
	-Icommon/$(X_MAJOR_ARCH) \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	-Ierrorexception \
	


X_THIRDPARTY_LIBS = \
	$(ACE_LIBS) \
    $(BOOST_LIBS)
X_SYSTEM_LIBS =

X_LDFLAGS = 

BINARY=inmexec
BINARY_EXT=$(X_EXEEXT)

include bottom.mak

