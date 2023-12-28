# ----------------------------------------------------------------------
# InMage global Makefile. Requires GNU Make 3.80 or better
# See http://safe/twiki/pub/Intranet/Documentation/CrossPlatformBuilds.pdf
# ----------------------------------------------------------------------

# ----------------------------------------------------------------------
# get the arch if not set when invoked
# ----------------------------------------------------------------------
ifeq (AIX,$(shell uname))
	X_ARCH ?= $(shell uname)
else
	X_ARCH ?= $(shell uname -ms | sed -e s"/ /_/g")
endif 
X_SUNOS ?= $(shell uname)
X_XENOS ?= $(shell cat /etc/redhat-release 2>/dev/null | cut -d ' ' -f1)
X_OS ?= $(shell uname)
# ----------------------------------------------------------------------
# needed by all X_ARCHs
# ----------------------------------------------------------------------
REQUIRED_BY_ALL_MODULES := config log tal

$(shell ln -sf tests/common tests_common)
$(shell ln -sf tests/DITests/DITestUtil tests_ditests_ditestutil)
$(shell ln -sf tests/Component/drivers/indskflt/ tests_component_drivers)
$(shell ln -sf tests/Component/drivers/indskflt/Microsoft.Test.Inmage.IoctlController tests_ioctl_controller)
$(shell ln -sf tests/Component/drivers/indskflt/Microsoft.Test.Inmage.TestAgent tests_test_agent)

# ----------------------------------------------------------------------
# X_ARCH specific modules
# NOTE:
# order is somewhat important in that you need to list all modules that 
# generate libs before any of the modules that need those libs
# if you get the wrong order you will see an error message telling you 
# the order is in correct
# ----------------------------------------------------------------------
MODULES := $(REQUIRED_BY_ALL_MODULES) remoteapilib scheduler common s2libs inmpipeline conversionlib cdplibs vacplibs vsnaplib virtualvolumelib appdevicelib dplibs deviceidinformer  platforminfo dirbasename executecommand scsicommandissuer volumeinfocollector npwwn xslibs backups2lib backups2lib-cs cdpapplylib cdpapplylibRcm localdirectsynclib volumescopier securitylib simplelogger genericprofiler curlwrapperlib cxpslib cxtransportlib cxps cxpscli AzureRecoveryLib cxpsclient iscsiinterface validatesvdfile disklayoutmgr AzureADAuthProviderLib IMDSLib AzureRcmLib AzureGatewayLib AzureRcmCli CommitTagLib service service-cs dataprotection DataProtectionSyncRcm cdpmgr s2 s2-cs cachemgr cdpcli unregagent hostconfig customdevicecli svdcheck vacpunix unlockVolumes testexecutecommand gfdisk testvolumeinfocollector testvolumescopier fr_log fr_common fr_config fragent frdaemon runundervalgrind tools/inmlimit fr_unregagent fabricagent cxcli pushInstallerCli storagefailover EsxUtil appframeworklib applicationagent  FailoverCommadUtil inm_scsi_id configmergetool inmexec ScoutTuning inmsync csgetfingerprint gencert viewcert genpassphrase CSScheduler sslsign AzureRecoveryUtil tests_common tests_ditests_ditestutil AzureRcmTest EvtCollForw tests_component_drivers tests_test_agent tests_ioctl_controller

ifeq (SunOS,$(X_SUNOS))
MODULES := $(REQUIRED_BY_ALL_MODULES) remoteapilib scheduler common s2libs inmpipeline conversionlib cdplibs vacplibs vsnaplib virtualvolumelib appdevicelib dplibs deviceidinformer  platforminfo dirbasename executecommand scsicommandissuer volumeinfocollector npwwn xslibs backups2lib cdpapplylib cdpapplylibRcm localdirectsynclib volumescopier securitylib simplelogger genericprofiler curlwrapperlib cxpslib cxtransportlib cxps cxpscli cxpsclient iscsiinterface validatesvdfile service dataprotection DataProtectionSyncRcm cdpmgr s2 cachemgr cdpcli unregagent hostconfig customdevicecli svdcheck vacpunix testexecutecommand testvolumeinfocollector testvolumescopier cxcli pushInstallerCli inmzpool appframeworklib applicationagent FailoverCommadUtil inm_scsi_id configmergetool inmexec inmsync csgetfingerprint gencert viewcert genpassphrase sslsign
endif

ifeq (AIX,$(X_OS))
MODULES := $(REQUIRED_BY_ALL_MODULES) remoteapilib common s2libs inmpipeline conversionlib cdplibs vacplibs vsnaplib virtualvolumelib appdevicelib scheduler dplibs deviceidinformer  platforminfo dirbasename executecommand scsicommandissuer volumeinfocollector npwwn xslibs backups2lib cdpapplylib cdpapplylibRcm localdirectsynclib volumescopier cxpslib cxtransportlib cxps cxpscli cxpsclient iscsiinterface validatesvdfile service dataprotection DataProtectionSyncRcm cdpmgr s2 cachemgr cdpcli unregagent customdevicecli svdcheck vacpunix testexecutecommand testvolumeinfocollector testvolumescopier fr_log fr_common fr_config fragent frdaemon fr_unregagent cxcli pushInstallerCli appframeworklib applicationagent  FailoverCommadUtil inm_scsi_id configmergetool inmexec inmsync p2vutil
endif

ifeq (yes,$(FX_VACP))
MODULES := conversionlib vacplibs vacpunix
X_ARCH=fx_vacp
endif

include once.mak
