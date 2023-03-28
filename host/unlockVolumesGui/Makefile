include top.mak
SRCS =  \

BINARY=
BINARY_EXT=$(X_EXEEXT)
DEPS=
X_INDEPENDENTMAKE=1
UNLOCK_VOLUMES_GUI :
	set fnord $(MAKEFLAGS); amf=$$2; \
	dot_seen=no; \
	(/bin/cp unlockVolumesGui/*.* $(X_ARCH)/unlockVolumesGui && cd $(X_ARCH)/unlockVolumesGui && /usr/bin/qmake -o Makefile unlockvolumes.pro -cache qmake.cache && gmake -f Makefile X_ARCH=$(X_ARCH) ) \
	 || (case "$$amf" in *=*) exit 1;; *k*) fail=yes;; *) exit 1;; esac;) \
	&& test -z "$$fail"


all:: UNLOCK_VOLUMES_GUI

include bottom.mak

