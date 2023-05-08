include top.mak

SRCS = \
	$(X_MAJOR_ARCH)/volumeinfocollectorinfo.cpp \
	$(X_MINOR_ARCH)/volumeinfocollectorinfominor.cpp \
	$(X_MAJOR_ARCH)/volumeinfocollectormajor.cpp \
	$(X_MINOR_ARCH)/volumeinfocollectorminor.cpp \
	$(X_MAJOR_ARCH)/swapvolumesmajor.cpp \
	$(X_MINOR_ARCH)/swapvolumesminor.cpp \
	$(X_MAJOR_ARCH)/systemmountpointsmajor.cpp \
	$(X_MINOR_ARCH)/systemmountpointsminor.cpp \
	$(X_MAJOR_ARCH)/utilfdsafeclose.cpp \
	$(X_MAJOR_ARCH)/devicetrackermajor.cpp \
	$(X_MINOR_ARCH)/devicetrackerminor.cpp \
	$(X_MAJOR_ARCH)/customdevices.cpp \
	$(X_MAJOR_ARCH)/dumpmsg.cpp \
	$(X_MAJOR_ARCH)/cachevolume.cpp \
	$(X_MAJOR_ARCH)/utilfunctionsmajor.cpp \
	$(X_MINOR_ARCH)/filesystemtracker.cpp \
	$(X_MINOR_ARCH)/dumpdevice.cpp \
	$(X_MINOR_ARCH)/tempdevice.cpp \
	$(X_MINOR_ARCH)/volumemanagerfunctionsminor.cpp \
    $(X_MAJOR_ARCH)/writecacheinformer.cpp \
    $(X_MAJOR_ARCH)/scsiwritecacheinformer.cpp \
    $(X_MAJOR_ARCH)/atawritecacheinformer.cpp \
    $(X_MINOR_ARCH)/atawritecacheinformerminor.cpp \
    $(X_MAJOR_ARCH)/atacommandissuer.cpp \
    $(X_MINOR_ARCH)/platformbasedatacommandissuer.cpp \
	$(X_MINOR_ARCH)/devdidtrackerminor.cpp \
	$(X_MINOR_ARCH)/devglbltrackerminor.cpp \
	$(X_MINOR_ARCH)/mpxiotrackerminor.cpp \
	$(X_MINOR_ARCH)/bootdiskminor.cpp \
	$(X_MAJOR_ARCH)/volumemanagerfunctions.cpp \
	$(X_MAJOR_ARCH)/logicalvolumes.cpp \
	$(X_MINOR_ARCH)/logicalvolumesminor.cpp \
	$(X_MAJOR_ARCH)/volumegroups.cpp \
	$(X_MAJOR_ARCH)/luncapacityinformer.cpp \
	$(X_MINOR_ARCH)/luncapacityinformerminor.cpp \
	$(X_MAJOR_ARCH)/scsiluncapacityinformer.cpp \
	$(X_MAJOR_ARCH)/persistentdevicenames.cpp \
	volumereporter.cpp \
	volumereporterimp.cpp \


X_CFLAGS = \
    -DVSNAP_USER_MODE \
	-Ivolumeinfocollector \
	-Ivolumeinfocollector/$(X_MAJOR_ARCH) \
	-Ivolumeinfocollector/$(X_MINOR_ARCH) \
	-Iappdevicelib \
	-Ideviceidinformer \
	-Iplatforminfo \
	-Iplatforminfo/$(X_MAJOR_ARCH) \
	-Iplatforminfo/$(X_MINOR_ARCH) \
	-Iexecutecommand \
	-Iexecutecommand/$(X_MINOR_ARCH) \
	-Iscsicommandissuer \
	-Iscsicommandissuer/$(X_MINOR_ARCH) \
	-Idirbasename \
	-Idirbasename/$(X_MAJOR_ARCH) \
	-Ivsnaplib \
	-Ilog \
	-Ilog/$(X_MAJOR_ARCH) \
	-Iconfig \
	-Icdplibs  \
	-Iconfig/$(X_MAJOR_ARCH) \
	-I$(X_ARCH)/config \
	-Icommon \
    -Iinmsafeint \
    -Iinmsafeint/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MAJOR_ARCH) \
	-Icommon/$(X_MINOR_ARCH) \
	-Is2libs/thread \
	-Is2libs/foundation \
	-Idrivers/InVirVol/$(X_MINOR_ARCH) \
	-Idrivers/InVirVol/common \
	-Idisklayoutmgr \
	-Iinmsafecapis \
	-Iinmsafecapis/$(X_MAJOR_ARCH) \
	$(ACE_INCLUDE) \
	$(SQLITE_INCLUDE) \
	$(SIGSLOT_INCLUDE) \
	$(ESJSON_INCLUDES) \
	$(BOOST_INCLUDE)

X_LIBS = 

X_THIRDPARTY_LIBS = 

X_SYSTEM_LIBS =

X_LDFLAGS =

BINARY=$(X_LIBPRE)volumeinfocollector
BINARY_EXT=$(X_LIBEXT)

include bottom.mak
