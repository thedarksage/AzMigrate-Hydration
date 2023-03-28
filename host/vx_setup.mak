# ----------------------------------------------------------------------
# create VX setup_vx package
# ----------------------------------------------------------------------

include ../build/scripts/VX/build_params_$(X_PARTNER).mak
include ../build/scripts/VX/build_params_$(X_PARTNER)_linux.mak
ifndef X_ALREADY_INCLUDED

X_OP:=$(shell uname -i)
X_OS:=$(shell ../build/scripts/general/OS_details.sh 1)

$(shell echo $(X_OS) > OS_details_script)
$(shell sed -i -e "s/-/_/g" OS_details_script)
$(shell sed -i -e "s/\./_/g" OS_details_script)
OS_FOR_RPM := $(shell cat OS_details_script)

INMSHUT_NOTIFY := drivers/InVolFlt/linux/user_space/shutdownt
INM_DMIT := drivers/InVolFlt/linux/user_space/inmdmit

vx_setup: vx_build driver_build non_driver_build vx_packaging

driver_build: involflt

involflt_overall_list_redhat := 2.6.18-194.el5 \
				2.6.18-194.el5xen \
				2.6.32-71.el6.i686 \
				2.6.32-71.el6.x86_64 \
				3.10.0-123.el7.x86_64 \
				3.10.0-514.el7.x86_64 \
				3.10.0-693.el7.x86_64 \
				2.6.32-131.0.15.el6.i686 \
				2.6.32-131.0.15.el6.x86_64 \
				2.6.32-100.34.1.el6uek.i686 \
				2.6.32-200.16.1.el6uek.i686 \
				2.6.32-300.3.1.el6uek.i686 \
				2.6.32-400.26.1.el6uek.i686 \
				2.6.39-100.5.1.el6uek.i686 \
				2.6.39-200.24.1.el6uek.i686 \
				2.6.39-300.17.1.el6uek.i686 \
				2.6.39-400.17.1.el6uek.i686 \
				2.6.32-100.28.5.el6.x86_64 \
				2.6.32-100.34.1.el6uek.x86_64 \
				2.6.32-200.16.1.el6uek.x86_64 \
				2.6.32-300.3.1.el6uek.x86_64 \
				2.6.32-400.26.1.el6uek.x86_64 \
				2.6.39-100.5.1.el6uek.x86_64 \
				2.6.39-200.24.1.el6uek.x86_64 \
				2.6.39-300.17.1.el6uek.x86_64 \
				2.6.39-400.17.1.el6uek.x86_64 \
				3.8.13-16.2.1.el6uek.x86_64 \
				4.1.12-32.1.2.el6uek.x86_64 \
				3.8.13-35.3.1.el7uek.x86_64 \
				4.1.12-124.18.1.el7uek.x86_64 \
				4.14.35-1818.0.9.el7uek.x86_64 \
				4.14.35-1902.0.18.el7uek.x86_64 \
				5.4.17-2011.1.2.el7uek.x86_64 \
				4.18.0-80.el8.x86_64 \
				4.18.0-147.el8.x86_64 \
				4.18.0-305.el8.x86_64 \
				4.18.0-348.el8.x86_64 \
				4.18.0-425.3.1.el8.x86_64 \
				4.18.0-425.10.1.el8_7.x86_64 \
				5.14.0-70.13.1.0.3.el9_0.x86_64 \
				5.14.0-162.6.1.el9_1.x86_64


involflt_overall_list_sles := 2.6.5-7.191-bigsmp 2.6.5-7.191-default 2.6.5-7.191-smp 2.6.5-7.244-bigsmp 2.6.5-7.244-default 2.6.5-7.244-smp 2.6.16.21-0.8-bigsmp 2.6.16.21-0.8-default 2.6.16.21-0.8-smp 2.6.16.46-0.12-bigsmp 2.6.16.46-0.12-default 2.6.16.46-0.12-smp 2.6.16.60-0.21-bigsmp 2.6.16.60-0.21-default 2.6.16.60-0.21-smp 2.6.13-15-bigsmp 2.6.13-15-default 2.6.13-15-smp 2.6.27.19-5-default 2.6.27.19-5-pae 2.6.27.19-5-xen 2.6.16.60-0.54.5-default 2.6.16.60-0.54.5-smp 2.6.16.60-0.54.5-bigsmp 2.6.16.60-0.54.5-xen 2.6.32.12-0.7-default 2.6.32.12-0.7-xen 2.6.32.12-0.7-pae 2.6.16.60-0.85.1-default 2.6.16.60-0.85.1-smp 2.6.16.60-0.85.1-bigsmp 2.6.16.60-0.85.1-xen 3.0.13-0.27-default 3.0.13-0.27-pae 3.0.13-0.27-xen 3.0.76-0.11-default 3.0.76-0.11-pae 3.0.76-0.11-xen 3.0.101-63-default 3.0.101-63-xen

.PHONY: copy_filter_driver copy_initrd_generic_scripts copy_initrd_specific_scripts giving_recurssive_permissions copy_centosplus_64_bit_drivers

# RHEL5-64
ifeq ($(X_OS), RHEL5-64)
  involflt:: 2.6.18-194.el5 2.6.18-194.el5xen
endif

# RHEL6-64
ifeq ($(X_OS), RHEL6-64)
  involflt:: 2.6.32-71.el6.x86_64
endif

# RHEL7-64
ifeq ($(X_OS), RHEL7-64)
  involflt:: 3.10.0-123.el7.x86_64 3.10.0-514.el7.x86_64 3.10.0-693.el7.x86_64
endif

# RHEL8-64
ifeq ($(X_OS), RHEL8-64)
  involflt:: 4.18.0-80.el8.x86_64 4.18.0-147.el8.x86_64 4.18.0-305.el8.x86_64 4.18.0-348.el8.x86_64 4.18.0-425.3.1.el8.x86_64 4.18.0-425.10.1.el8_7.x86_64
endif

# RHEL9-64
ifeq ($(X_OS), RHEL9-64)
  involflt:: 5.14.0-70.13.1.0.3.el9_0.x86_64 5.14.0-162.6.1.el9_1.x86_64
endif

# OL6-64
ifeq ($(X_OS), OL6-64)
  involflt:: 2.6.32-131.0.15.el6.x86_64 \
			2.6.32-100.28.5.el6.x86_64 2.6.32-100.34.1.el6uek.x86_64 2.6.32-200.16.1.el6uek.x86_64 2.6.32-300.3.1.el6uek.x86_64 2.6.32-400.26.1.el6uek.x86_64 \
			2.6.39-100.5.1.el6uek.x86_64 2.6.39-200.24.1.el6uek.x86_64 2.6.39-300.17.1.el6uek.x86_64 2.6.39-400.17.1.el6uek.x86_64 3.8.13-16.2.1.el6uek.x86_64 \
			4.1.12-32.1.2.el6uek.x86_64
endif

# OL7-64
ifeq ($(X_OS), OL7-64)
  involflt:: 3.10.0-123.el7.x86_64  3.10.0-514.el7.x86_64  3.10.0-693.el7.x86_64  3.8.13-35.3.1.el7uek.x86_64  4.1.12-124.18.1.el7uek.x86_64  4.14.35-1818.0.9.el7uek.x86_64 4.14.35-1902.0.18.el7uek.x86_64 5.4.17-2011.1.2.el7uek.x86_64
endif

# OL8-64
ifeq ($(X_OS), OL8-64)
  involflt:: ol8_drv
endif

# OL9-64
ifeq ($(X_OS), OL9-64)
  involflt:: ol9_drv
endif

#For SLES11-SP3-64 platforms
ifeq ($(X_OS), SLES11-SP3-64)
 involflt:: 3.0.76-0.11-default 3.0.76-0.11-xen
endif

#For SLES11-SP4-64 platforms
ifeq ($(X_OS), SLES11-SP4-64)
 involflt:: 3.0.101-63-default 3.0.101-63-xen
endif

ifeq ($(X_OS),$(filter $(X_OS), UBUNTU-14.04-64 UBUNTU-16.04-64 UBUNTU-18.04-64 UBUNTU-20.04-64 UBUNTU-22.04-64 DEBIAN7-64 DEBIAN8-64 DEBIAN9-64 DEBIAN10-64 DEBIAN11-64))
 involflt:: ubuntu_drv
endif

ifeq ($(X_OS),$(filter $(X_OS), SLES12-64 SLES15-64))
 involflt:: sles_drv
endif

# The assumption is that the dir time stamp changes only when some checkins are done. Else there will not be any
# change in the timestamp.

$(involflt_overall_list_redhat): drivers/InVolFlt/linux
	@echo $@
	cd drivers/InVolFlt/linux && mkdir -p drivers_dbg && make -f involflt.mak KDIR=/lib/modules/$@/build clean && make -f involflt.mak KDIR=/lib/modules/$@/build debug=$(debug) && mv bld_involflt/involflt.ko drivers_dbg/involflt.ko.$@.dbg && strip -g drivers_dbg/involflt.ko.$@.dbg -o involflt.ko.$@

$(involflt_overall_list_sles): drivers/InVolFlt/linux
	@echo $@
	cd drivers/InVolFlt/linux && mkdir -p drivers_dbg && make -f involflt.mak KDIR=/lib/modules/$@/build clean && make -f involflt.mak KDIR=/lib/modules/$@/build debug=$(debug) && mv bld_involflt/involflt.ko drivers_dbg/involflt.ko.$@.dbg && strip -g drivers_dbg/involflt.ko.$@.dbg -o involflt.ko.$@

ol8_drv: drivers/InVolFlt/linux
	@echo "Building $(X_OS) Drivers"
	cd drivers/InVolFlt/linux && user_space/build/ol8.sh

ol9_drv: drivers/InVolFlt/linux
	@echo "Building $(X_OS) Drivers"
	cd drivers/InVolFlt/linux && user_space/build/ol9.sh

ubuntu_drv: drivers/InVolFlt/linux
	@echo "Building $(X_OS) Drivers"
	cd drivers/InVolFlt/linux && user_space/build/ubuntu.sh build

sles_drv: drivers/InVolFlt/linux
	@echo "Building $(X_OS) Drivers"
	cd drivers/InVolFlt/linux && user_space/build/sles.sh

non_driver_build: inmshutnotify_build inmstkops_build jq_build cvt_build

inmshutnotify_build: drivers/InVolFlt/linux/user_space/shutdownt
	cd $(INMSHUT_NOTIFY) && $(MAKE) clean && $(MAKE)

inmstkops_build: drivers/InVolFlt/linux/user_space/inmdmit
	cd $(INM_DMIT) && make -f mklinux.mk clean && make -f mklinux.mk

jq_build:
	cd $(JQ_ROOT) && autoreconf -fi && ./configure --disable-maintainer-mode && make clean && make LDFLAGS=-all-static

cvt_build:
	-$(VERBOSE)gmake tests_test_agent debug=$(debug) verbose=$(verbose) 2>&1

ifeq ($(NON_RPM), YES)
VX_TAR_NAME := $(COMPANY)_VX_$(X_VERSION_DOTTED)_$(X_OS)_$(X_VERSION_PHASE)_$(shell date "+%d%h%Y")_$(X_CONFIGURATION).tar.gz
else
VX_TAR_NAME := $(COMPANY)_VX_$(X_VERSION_DOTTED)_$(X_OS)_$(X_VERSION_PHASE)_$(shell date "+%d%h%Y")_$(X_CONFIGURATION).tar.gz
endif

vx_packaging: $(X_ARCH)/setup_vx/$(X_CONFIGURATION)/$(VX_TAR_NAME)

clean_vx_setup:
	$(VERBOSE)rm -rf $(X_ARCH)/setup_vx/$(X_CONFIGURATION)

ifeq ($(NON_RPM), YES)
$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/$(VX_TAR_NAME):\
	$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/$(COMPANY)_VX_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_OS).tar \
	$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/install \
	$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/conf_file \
	$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/$(VX_VERSION_FILE_NAME) \
	$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/OS_details.sh \

	$(VERBOSE)cd $(dir $<);tar cvzf $(notdir $@) $(notdir $^)
	$(RULE_SEPARATOR)
else
$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/$(VX_TAR_NAME): \
	$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/$(COMPANY)Vx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm \
	$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/install \
	$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/conf_file \
	$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/$(VX_VERSION_FILE_NAME) \
	$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/OS_details.sh \

	$(VERBOSE)cd $(dir $<);tar cvzf $(notdir $@) $(notdir $^)
	$(RULE_SEPARATOR)
endif

ifeq ($(NON_RPM), YES)
  VX_TOP_DIR=$(CURDIR)/$(X_ARCH)/setup_vx/$(X_CONFIGURATION)
  VX_BUILD_PATH=$(VX_TOP_DIR)
else
  VX_TOP_DIR=$(CURDIR)/$(X_ARCH)/setup_vx/$(X_CONFIGURATION)
  VX_BUILD_ROOT=$(VX_TOP_DIR)/$(COMPANY)Vx-buildroot
  VX_BUILD_PATH=$(VX_BUILD_ROOT)/$(COMPANY)/Vx
endif

$(VX_BUILD_PATH)/bin/stop: ../build/scripts/VX/templates/stop
$(VX_BUILD_PATH)/bin/TuneMaxRep.sh: ../build/scripts/VX/templates/TuneMaxRep.sh
$(VX_BUILD_PATH)/bin/start: ../build/scripts/VX/templates/start
$(VX_BUILD_PATH)/bin/status: ../build/scripts/VX/templates/status
$(VX_BUILD_PATH)/bin/uninstall: ../build/scripts/VX/templates/uninstall
$(VX_BUILD_PATH)/etc/drscout.conf: ../build/scripts/VX/templates/drscout.conf
$(VX_BUILD_PATH)/bin/unregister: ../build/scripts/VX/templates/unregister
$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/install: ../build/scripts/VX/templates/install
$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/conf_file: ../build/scripts/VX/templates/conf_file

ifeq ($(NON_RPM), YES)
$(VX_BUILD_PATH)/bin/stop \
$(VX_BUILD_PATH)/bin/TuneMaxRep.sh \
$(VX_BUILD_PATH)/bin/start \
$(VX_BUILD_PATH)/bin/status \
$(VX_BUILD_PATH)/bin/uninstall \
$(VX_BUILD_PATH)/bin/unregister \
$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/install:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)sed   -e "s|PARTNER_OR_COMPANY|$(COMPANY)|g" \
		-e "s|CX|$(LABEL)|g" \
		-e "s|DEFAULT_PARTNER_INSTALL_DIR|$(DEFAULT_INSTALL_DIR)|g"\
		-e "s|BUILDTIME_CURRENT_VERSION|$(X_VERSION_DOTTED)|" \
		-e "s|BUILDTIME_INST_VERSION_FILE|$(VX_VERSION_FILE_NAME)|" \
		-e "s|BUILDTIME_VALUE_INST_VERSION_FILE|$(VX_VERSION_FILE_NAME)|" \
		-e "s|BUILDTIME_BUILD_MANIFEST_FILE|$(VX_BUILD_MANIFEST_FILE_NAME)|" \
		-e "s|BUILDTIME_VALUE_BLD_MANIFEST_FILE_NAME|$(VX_BUILD_MANIFEST_FILE_NAME)|" \
		-e "s|BUILDTIME_DEFAULT_UPGRADE_DIR|$(VX_DEFAULT_UPG_DIR)|" \
		-e "s|BUILDTIME_PARTNER_SPECIFIC_STRING|$(PARTNER_SPECIFIC_STRING)|" \
		-e "s|BUILDTIME_REF_DIR|$(METADATA_REF_DIR)|" \
		-e "s|BUILDTIME_VALUE_REF_DIR|$(METADATA_REF_DIR)|" \
		$< > $@
	$(VERBOSE)chmod 755 $@
	$(RULE_SEPARATOR)
else
$(VX_BUILD_PATH)/bin/stop \
$(VX_BUILD_PATH)/bin/TuneMaxRep.sh \
$(VX_BUILD_PATH)/bin/start \
$(VX_BUILD_PATH)/bin/status \
$(VX_BUILD_PATH)/bin/uninstall \
$(VX_BUILD_PATH)/bin/unregister \
$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/install:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)sed   -e "s|PARTNER_OR_COMPANY|$(COMPANY)|g" \
                -e "s|CX|$(LABEL)|g" \
                -e "s|DEFAULT_PARTNER_INSTALL_DIR|$(DEFAULT_INSTALL_DIR)|g"\
                -e "s|BUILDTIME_CURRENT_VERSION|$(X_VERSION_DOTTED)|" \
                -e "s|BUILDTIME_INST_VERSION_FILE|$(VX_VERSION_FILE_NAME)|" \
                -e "s|BUILDTIME_VALUE_INST_VERSION_FILE|$(VX_VERSION_FILE_NAME)|" \
                -e "s|BUILDTIME_BUILD_MANIFEST_FILE|$(VX_BUILD_MANIFEST_FILE_NAME)|" \
                -e "s|BUILDTIME_VALUE_BLD_MANIFEST_FILE_NAME|$(VX_BUILD_MANIFEST_FILE_NAME)|" \
                -e "s|BUILDTIME_MAIN_RPM_FILE|$(VX_BUILT_RPM_FILE)|" \
                -e "s|BUILDTIME_MAIN_RPM_PACKAGE|$(VX_BUILT_RPM_PACKAGE)|" \
                -e "s|BUILDTIME_VALUE_RPM_PACKAGE|$(VX_BUILT_RPM_PACKAGE)|" \
                -e "s|BUILDTIME_DEFAULT_UPGRADE_DIR|$(VX_DEFAULT_UPG_DIR)|" \
                -e "s|BUILDTIME_PARTNER_SPECIFIC_STRING|$(PARTNER_SPECIFIC_STRING)|" \
                -e "s|BUILDTIME_REF_DIR|$(METADATA_REF_DIR)|" \
                -e "s|BUILDTIME_VALUE_REF_DIR|$(METADATA_REF_DIR)|" \
                $< > $@
	$(VERBOSE)chmod 755 $@
	$(RULE_SEPARATOR)
endif

ifeq ($(debug), yes)
$(VX_BUILD_PATH)/etc/drscout.conf:
	$(VERBOSE)mkdir -p $(dir $@)
	$(shell ./drscout.conf_changes)
	cp ../build/scripts/VX/templates/drscout.conf.new.new $(VX_BUILD_PATH)/etc/drscout.conf
	$(VERBOSE)chmod 755 $@
	rm -f ../build/scripts/VX/templates/drscout.conf.new.new ../build/scripts/VX/templates/drscout.conf.new
	$(RULE_SEPARATOR)
else
$(VX_BUILD_PATH)/etc/drscout.conf:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)sed   -e "s|LogLevel = 7|LogLevel = 3|g" $< > $@
	$(VERBOSE)chmod 755 $@
	$(RULE_SEPARATOR) 
endif

ifeq ($(NON_RPM), YES)
$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/$(VX_VERSION_FILE_NAME): $(X_ARCH)/branding/platform-* $(X_ARCH)/branding/version-*
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)echo VERSION=$(X_VERSION_DOTTED) > $@
	$(VERBOSE)echo PROD_VERSION=$(PROD_VERSION) >> $@
	$(VERBOSE)echo PARTNER=$(X_PARTNER) >> $@
	$(VERBOSE)echo INSTALLATION_DIR=RUNTIME_INSTALL_PATH >> $@
	$(VERBOSE)echo AGENT_MODE=RUNTIME_AGENT_MODE >> $@
	$(VERBOSE)echo VERSION_FILE_PARENT_DIR=$(METADATA_REF_DIR) >> $@
	$(VERBOSE)echo DEFAULT_UPGRADE_DIR=$(DEFAULT_UPG_DIR) >> $@
	$(VERBOSE)echo BUILD_TAG=$(X_VERSION_QUALITY)_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_VERSION_PHASE)_$(X_VERSION_BUILD_NUM)_$(shell date "+%h_%d_%Y")_$(shell echo $(X_PARTNER) | tr 'a-z' 'A-Z') >> $@
	$(VERBOSE)echo OS_BUILT_FOR=$(subst platform-,,$(notdir $<)) >> $@
	$(VERBOSE)echo CONFIG=$(X_CONFIGURATION) >> $@
	$(VERBOSE)echo TAR_FILE_NAME=$(COMPANY)_VX_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_OS).tar >> $@
	$(RULE_SEPARATOR)
else
$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/$(VX_VERSION_FILE_NAME): $(X_ARCH)/branding/platform-* $(X_ARCH)/branding/version-*
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)echo VERSION=$(X_VERSION_DOTTED) > $@
	$(VERBOSE)echo PROD_VERSION=$(PROD_VERSION) >> $@
	$(VERBOSE)echo PARTNER=$(X_PARTNER) >> $@
	$(VERBOSE)echo INSTALLATION_DIR=RUNTIME_INSTALL_PATH >> $@
	$(VERBOSE)echo RPM_PACKAGE=$(VX_BUILT_RPM_PACKAGE) >> $@
	$(VERBOSE)echo RPM_NAME=$(VX_BUILT_RPM_FILE) >> $@
	$(VERBOSE)echo AGENT_MODE=RUNTIME_AGENT_MODE >> $@
	$(VERBOSE)echo VERSION_FILE_PARENT_DIR=$(METADATA_REF_DIR) >> $@
	$(VERBOSE)echo DEFAULT_UPGRADE_DIR=$(DEFAULT_UPG_DIR) >> $@
	$(VERBOSE)echo BUILD_TAG=$(X_VERSION_QUALITY)_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_VERSION_PHASE)_$(X_VERSION_BUILD_NUM)_$(shell date "+%h_%d_%Y")_$(shell echo $(X_PARTNER) | tr 'a-z' 'A-Z') >> $@
	$(VERBOSE)echo OS_BUILT_FOR=$(subst platform-,,$(notdir $<)) >> $@
	$(VERBOSE)echo CONFIG=$(X_CONFIGURATION) >> $@
	$(RULE_SEPARATOR)
endif

$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/conf_file:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)sed   -e "s|DEFAULT_PARTNER_INSTALL_DIR|$(DEFAULT_INSTALL_DIR)|g" \
		$< > $@
	$(VERBOSE)chmod 755 $@
	$(RULE_SEPARATOR)

$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/OS_details.sh: ../build/scripts/general/OS_details.sh

$(VX_BUILD_PATH)/bin/killchildren: ../build/scripts/VX/templates/killchildren
$(VX_BUILD_PATH)/bin/vxagent: ../build/scripts/VX/templates/vxagent
$(VX_BUILD_PATH)/bin/vxagent.service.systemd: ../build/scripts/VX/templates/vxagent.service.systemd
$(VX_BUILD_PATH)/bin/uarespawnd: ../build/scripts/VX/templates/uarespawnd
$(VX_BUILD_PATH)/bin/uarespawnd.service.systemd: ../build/scripts/VX/templates/uarespawnd.service.systemd
$(VX_BUILD_PATH)/bin/uarespawndagent: ../build/scripts/VX/templates/uarespawndagent
$(VX_BUILD_PATH)/bin/check_drivers.sh: ../build/scripts/VX/templates/check_drivers.sh
$(VX_BUILD_PATH)/bin/UnifiedAgentConfigurator.sh: ../build/scripts/VX/templates/UnifiedAgentConfigurator.sh
$(VX_BUILD_PATH)/bin/teleAPIs.sh: ../build/scripts/unifiedagent/templates/teleAPIs.sh
$(VX_BUILD_PATH)/bin/jq: $(JQ_ROOT)/jq
$(VX_BUILD_PATH)/bin/vsnap_export_unexport.pl: ../build/scripts/VX/templates/vsnap_export_unexport.pl
$(VX_BUILD_PATH)/etc/notallowed_for_mountpoints.dat: common/unix/notallowed_for_mountpoints.dat
$(VX_BUILD_PATH)/etc/inmexec_jobspec.txt: ./inmexec/inmexec_jobspec.txt
$(VX_BUILD_PATH)/etc/RCMInfo.conf: ../build/scripts/VX/templates/RCMInfo.conf
$(VX_BUILD_PATH)/bin/vol_pack.sh: cdpcli/vol_pack.sh
$(VX_BUILD_PATH)/bin/collect_support_materials.sh: ../build/scripts/VX/templates/collect_support_materials.sh
$(VX_BUILD_PATH)/bin/halt.local.rh: drivers/InVolFlt/linux/user_space/shutdown_script/halt.local.rh
$(VX_BUILD_PATH)/bin/inmaged.modules: drivers/InVolFlt/linux/user_space/hotplug/inmaged.modules
$(VX_BUILD_PATH)/bin/boot.inmage: drivers/InVolFlt/linux/user_space/hotplug/boot.inmage
$(VX_BUILD_PATH)/bin/boot.inmage.sles11: drivers/InVolFlt/linux/user_space/shutdown_script/boot.inmage.sles11
$(VX_BUILD_PATH)/bin/boottimemirroring.sh: drivers/InVolFlt/common/user_space/boottimemirroring/boottimemirroring.sh
$(VX_BUILD_PATH)/bin/scsi_id.sh: drivers/InVolFlt/common/user_space/scsi_id/scsi_id.sh
$(VX_BUILD_PATH)/bin/boottimemirroring_phase1: drivers/InVolFlt/linux/user_space/boottimemirroring/boottimemirroring_phase1
$(VX_BUILD_PATH)/bin/boottimemirroring_phase2: drivers/InVolFlt/linux/user_space/boottimemirroring/boottimemirroring_phase2
$(VX_BUILD_PATH)/bin/at_lun_masking_udev.sh: drivers/InVolFlt/linux/user_space/at_lun_masking/at_lun_masking_udev.sh
$(VX_BUILD_PATH)/bin/at_lun_masking.sh: drivers/InVolFlt/linux/user_space/at_lun_masking/at_lun_masking.sh
$(VX_BUILD_PATH)/bin/09-inm_udev.rules: drivers/InVolFlt/linux/user_space/at_lun_masking/09-inm_udev.rules
$(VX_BUILD_PATH)/bin/generate_device_name: generateDeviceNames/generate_device_name
$(VX_BUILD_PATH)/bin/boot-involflt.sh: drivers/InVolFlt/linux/user_space/hotplug/boot-involflt.sh
$(VX_BUILD_PATH)/bin/involflt.sh: drivers/InVolFlt/linux/user_space/scripts/involflt.sh
$(VX_BUILD_PATH)/bin/get_protected_dev_details.sh: drivers/InVolFlt/linux/user_space/scripts/get_protected_dev_details.sh
$(VX_BUILD_PATH)/bin/involflt_start.service.systemd: drivers/InVolFlt/linux/user_space/hotplug/involflt_start.service.systemd
$(VX_BUILD_PATH)/bin/involflt_stop.service.systemd: drivers/InVolFlt/linux/user_space/shutdown_script/involflt_stop.service.systemd
$(VX_BUILD_PATH)/bin/libcommon.sh: ../build/scripts/unifiedagent/templates/libcommon.sh

$(VX_BUILD_PATH)/scripts: ../Solutions
	$(VERBOSE)mkdir -p $(VX_BUILD_PATH)/scripts
	$(VERBOSE)mkdir -p $(VX_BUILD_PATH)/scripts/Cloud/AWS
	$(VERBOSE)mkdir -p $(VX_BUILD_PATH)/scripts/Array
	$(VERBOSE)mkdir -p $(VX_BUILD_PATH)/scripts/samplefiles
	$(VERBOSE)mkdir -p $(VX_BUILD_PATH)/scripts/vCon
	$(VERBOSE)mkdir -p $(VX_BUILD_PATH)/scripts/azure
	$(VERBOSE)cp -r ../Solutions/Oracle/Vx/application.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../Solutions/Oracle/Vx/dns.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../Solutions/Oracle/Vx/mysql.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../Solutions/Oracle/Vx/mysqltargetdiscovery.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../Solutions/Oracle/Vx/oracle.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../Solutions/Oracle/Vx/oracletargetdiscovery.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../Solutions/PostGresql/*.sql $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../Solutions/PostGresql/*.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../Solutions/Oracle/samplefiles/sample_discovery.xml $(VX_BUILD_PATH)/scripts/samplefiles
	$(VERBOSE)cp -r ../host/appframeworklib/app/unix/oracle/scripts/oracleappagent.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../host/appframeworklib/app/unix/oracle/scripts/oracleappdiscovery.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../host/appframeworklib/app/unix/oracle/scripts/oracleappdiscovery.pl $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../host/appframeworklib/app/unix/oracle/scripts/oracle_consistency.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../host/appframeworklib/app/unix/oracle/scripts/oraclefailover.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../host/appframeworklib/app/unix/oracle/scripts/oracleappwizard.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../host/appframeworklib/app/unix/oracle/scripts/oraclediscovery.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../host/appframeworklib/app/unix/db2/scripts/db2_consistency.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../host/appframeworklib/app/unix/db2/scripts/db2discovery.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../host/appframeworklib/app/unix/db2/scripts/db2appagent.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../host/appframeworklib/app/unix/db2/scripts/db2failover.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../host/appframeworklib/ruleengine/unix/scripts/inmvalidator.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../host/appframeworklib/app/unix/Array/perl/arrayManager.pl $(VX_BUILD_PATH)/scripts/Array
	$(VERBOSE)cp -r ../host/appframeworklib/app/unix/Array/perl/Log.pm $(VX_BUILD_PATH)/scripts/Array
	$(VERBOSE)cp -r ../host/appframeworklib/app/unix/Array/perl/pillarArrayManager.pm $(VX_BUILD_PATH)/scripts/Array
	$(VERBOSE)cp -r ../host/appframeworklib/app/unix/Array/perl/clearwritesplits.pl $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../host/appframeworklib/app/unix/inm_device_refresh.sh $(VX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../Solutions/vconlinuxscripts/*.sh $(VX_BUILD_PATH)/scripts/vCon
	$(VERBOSE)cp -r ../Solutions/SolutionsWizard/Scripts/p2v_esx/*.sh $(VX_BUILD_PATH)/scripts/vCon
	$(VERBOSE)cp -r ../Solutions/SolutionsWizard/Scripts/p2v_esx/vConService_linux $(VX_BUILD_PATH)/scripts/vCon
	$(VERBOSE)cp -r ../Solutions/SolutionsWizard/Scripts/p2v_esx/vCon.service.systemd $(VX_BUILD_PATH)/scripts/vCon
	$(VERBOSE)cp -r ../build/scripts/general/OS_details.sh $(VX_BUILD_PATH)/scripts/vCon
	$(VERBOSE)cp -r ../host/applicationagent/unix/aws_recover.sh $(VX_BUILD_PATH)/scripts/Cloud/AWS
	$(VERBOSE)cp -r ../host/drivers/InVolFlt/linux/user_space/azure/ifcfg-eth0 $(VX_BUILD_PATH)/scripts/azure
	$(VERBOSE)cp -r ../host/drivers/InVolFlt/linux/user_space/azure/network $(VX_BUILD_PATH)/scripts/azure
	$(VERBOSE)cp -r ../host/drivers/InVolFlt/linux/user_space/azure/OS_details_target.sh $(VX_BUILD_PATH)/scripts/azure
	$(VERBOSE)cp -r ../host/drivers/InVolFlt/linux/user_space/azure/post-rec-script.sh $(VX_BUILD_PATH)/scripts/azure
	$(VERBOSE)cp -r ../host/drivers/InVolFlt/linux/user_space/azure/suse_ifcfg_eth0 $(VX_BUILD_PATH)/scripts/azure
	$(VERBOSE)cp -r ../host/drivers/InVolFlt/linux/user_space/azure/setazureip.sh $(VX_BUILD_PATH)/scripts/azure
	$(VERBOSE)cp -r ../host/drivers/InVolFlt/linux/user_space/azure/prepareforazure.sh $(VX_BUILD_PATH)/scripts/azure
	$(RULE_SEPARATOR)

generateDeviceNames/generate_device_name:
	cd generateDeviceNames; gmake clean; gmake; cd -

$(VX_BUILD_PATH)/bin/inmshutnotify: drivers/InVolFlt/linux/user_space/shutdownt/inmshutnotify
$(VX_BUILD_PATH)/bin/inm_dmit: drivers/InVolFlt/linux/user_space/inmdmit/inm_dmit
$(VX_BUILD_PATH)/bin/svagentsA2A: $(X_ARCH)/service/$(X_CONFIGURATION)/svagents
$(VX_BUILD_PATH)/bin/svagentsV2A: $(X_ARCH)/service-cs/$(X_CONFIGURATION)/svagents
$(VX_BUILD_PATH)/bin/appservice: $(X_ARCH)/applicationagent/$(X_CONFIGURATION)/appservice
$(VX_BUILD_PATH)/bin/FailoverCommandUtil: $(X_ARCH)/FailoverCommadUtil/$(X_CONFIGURATION)/FailoverCommandUtil
$(VX_BUILD_PATH)/bin/s2A2A: $(X_ARCH)/s2/$(X_CONFIGURATION)/s2
$(VX_BUILD_PATH)/bin/s2V2A: $(X_ARCH)/s2-cs/$(X_CONFIGURATION)/s2
$(VX_BUILD_PATH)/bin/cdpcli: $(X_ARCH)/cdpcli/$(X_CONFIGURATION)/cdpcli
$(VX_BUILD_PATH)/bin/dataprotection: $(X_ARCH)/dataprotection/$(X_CONFIGURATION)/dataprotection
$(VX_BUILD_PATH)/bin/DataProtectionSyncRcm: $(X_ARCH)/DataProtectionSyncRcm/$(X_CONFIGURATION)/DataProtectionSyncRcm
$(VX_BUILD_PATH)/bin/unregagent: $(X_ARCH)/unregagent/$(X_CONFIGURATION)/unregagent
$(VX_BUILD_PATH)/bin/svdcheck: $(X_ARCH)/svdcheck/$(X_CONFIGURATION)/svdcheck
$(VX_BUILD_PATH)/bin/testvolumeinfocollector: $(X_ARCH)/testvolumeinfocollector/$(X_CONFIGURATION)/testvolumeinfocollector
$(VX_BUILD_PATH)/bin/hostconfigcli: $(X_ARCH)/hostconfig/$(X_CONFIGURATION)/hostconfigcli
$(VX_BUILD_PATH)/bin/customdevicecli: $(X_ARCH)/customdevicecli/$(X_CONFIGURATION)/customdevicecli
$(VX_BUILD_PATH)/bin/vacp: $(X_ARCH)/vacpunix/$(X_CONFIGURATION)/vacp
$(VX_BUILD_PATH)/bin/cachemgr: $(X_ARCH)/cachemgr/$(X_CONFIGURATION)/cachemgr
$(VX_BUILD_PATH)/bin/cdpmgr: $(X_ARCH)/cdpmgr/$(X_CONFIGURATION)/cdpmgr
$(VX_BUILD_PATH)/bin/cxcli: $(X_ARCH)/cxcli/$(X_CONFIGURATION)/cxcli
$(VX_BUILD_PATH)/bin/fabricagent: $(X_ARCH)/fabricagent/$(X_CONFIGURATION)/fabricagent
$(VX_BUILD_PATH)/bin/pushinstallclient: $(X_ARCH)/pushInstallerCli/$(X_CONFIGURATION)/pushinstallclient
$(VX_BUILD_PATH)/bin/EsxUtil: $(X_ARCH)/EsxUtil/$(X_CONFIGURATION)/EsxUtil
$(VX_BUILD_PATH)/transport/newcert.pem: tal/newcert.pem
$(VX_BUILD_PATH)/transport/client.pem: cxpslib/pem/client.pem
$(VX_BUILD_PATH)/transport/cxps: $(X_ARCH)/cxps/$(X_CONFIGURATION)/cxps
$(VX_BUILD_PATH)/transport/cxpscli: $(X_ARCH)/cxpscli/$(X_CONFIGURATION)/cxpscli
$(VX_BUILD_PATH)/transport/cxps.conf: cxpslib/cxps.conf
$(VX_BUILD_PATH)/transport/cxpsctl: cxps/unix/cxpsctl
$(VX_BUILD_PATH)/bin/csgetfingerprint: $(X_ARCH)/csgetfingerprint/$(X_CONFIGURATION)/csgetfingerprint
$(VX_BUILD_PATH)/bin/gencert: $(X_ARCH)/gencert/$(X_CONFIGURATION)/gencert
$(VX_BUILD_PATH)/bin/genpassphrase: $(X_ARCH)/genpassphrase/$(X_CONFIGURATION)/genpassphrase
$(VX_BUILD_PATH)/bin/cxpsclient: $(X_ARCH)/cxpsclient/$(X_CONFIGURATION)/cxpsclient
$(VX_BUILD_PATH)/bin/inm_scsi_id: $(X_ARCH)/inm_scsi_id/$(X_CONFIGURATION)/inm_scsi_id
$(VX_BUILD_PATH)/bin/inmexec: $(X_ARCH)/inmexec/$(X_CONFIGURATION)/inmexec
$(VX_BUILD_PATH)/bin/ScoutTuning: $(X_ARCH)/ScoutTuning/$(X_CONFIGURATION)/ScoutTuning
$(VX_BUILD_PATH)/bin/AzureRecoveryUtil: $(X_ARCH)/AzureRecoveryUtil/$(X_CONFIGURATION)/AzureRecoveryUtil
$(VX_BUILD_PATH)/bin/AzureRcmCli: $(X_ARCH)/AzureRcmCli/$(X_CONFIGURATION)/AzureRcmCli
$(VX_BUILD_PATH)/bin/evtcollforw: $(X_ARCH)/EvtCollForw/$(X_CONFIGURATION)/evtcollforw
$(VX_BUILD_PATH)/bin/inm_list_part: $(X_ARCH)/gfdisk/$(X_CONFIGURATION)/inm_list_part



$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/$(COMPANY)Vx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm: \
$(VX_TOP_DIR)/RPMS/$(RPM_ARCH_LEVEL)/$(COMPANY)Vx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm

# Copying all the required files and binaries so that we can execute the spec file using the command rpmbuild.

COMMON_LIST :=  $(VX_BUILD_PATH)/bin/svagentsA2A \
		$(VX_BUILD_PATH)/bin/svagentsV2A \
		$(VX_BUILD_PATH)/bin/AzureRcmCli \
		$(VX_BUILD_PATH)/bin/AzureRecoveryUtil \
		$(VX_BUILD_PATH)/bin/s2A2A \
		$(VX_BUILD_PATH)/bin/s2V2A \
		$(VX_BUILD_PATH)/bin/cdpcli \
		$(VX_BUILD_PATH)/bin/dataprotection \
		$(VX_BUILD_PATH)/bin/DataProtectionSyncRcm \
		$(VX_BUILD_PATH)/bin/unregagent \
		$(VX_BUILD_PATH)/bin/hostconfigcli \
		$(VX_BUILD_PATH)/bin/vacp \
		$(VX_BUILD_PATH)/bin/cxcli \
		$(VX_BUILD_PATH)/bin/EsxUtil \
		$(VX_BUILD_PATH)/bin/killchildren \
		$(VX_BUILD_PATH)/bin/vxagent \
		$(VX_BUILD_PATH)/bin/vxagent.service.systemd \
		$(VX_BUILD_PATH)/bin/uarespawnd \
		$(VX_BUILD_PATH)/bin/uarespawnd.service.systemd \
		$(VX_BUILD_PATH)/bin/uarespawndagent \
		$(VX_BUILD_PATH)/bin/halt.local.rh \
		$(VX_BUILD_PATH)/bin/inmaged.modules \
		$(VX_BUILD_PATH)/bin/boot.inmage \
		$(VX_BUILD_PATH)/bin/boot.inmage.sles11 \
		$(VX_BUILD_PATH)/bin/inmshutnotify \
		$(VX_BUILD_PATH)/bin/inm_dmit \
		$(VX_BUILD_PATH)/bin/cxpsclient \
		$(VX_BUILD_PATH)/bin/appservice \
		$(VX_BUILD_PATH)/bin/FailoverCommandUtil \
		$(VX_BUILD_PATH)/bin/inm_scsi_id \
		$(VX_BUILD_PATH)/bin/inmexec \
		$(VX_BUILD_PATH)/bin/ScoutTuning \
		$(VX_BUILD_PATH)/bin/vsnap_export_unexport.pl \
		$(VX_BUILD_PATH)/bin/scsi_id.sh \
		$(VX_BUILD_PATH)/bin/check_drivers.sh \
		$(VX_BUILD_PATH)/bin/UnifiedAgentConfigurator.sh \
		$(VX_BUILD_PATH)/bin/teleAPIs.sh \
		$(VX_BUILD_PATH)/bin/jq \
		$(VX_BUILD_PATH)/bin/vol_pack.sh \
		$(VX_BUILD_PATH)/bin/collect_support_materials.sh \
		$(VX_BUILD_PATH)/bin/boottimemirroring.sh \
		$(VX_BUILD_PATH)/bin/boottimemirroring_phase1 \
		$(VX_BUILD_PATH)/bin/boottimemirroring_phase2 \
		$(VX_BUILD_PATH)/bin/at_lun_masking_udev.sh \
		$(VX_BUILD_PATH)/bin/at_lun_masking.sh \
		$(VX_BUILD_PATH)/bin/09-inm_udev.rules \
		$(VX_BUILD_PATH)/bin/boot-involflt.sh \
		$(VX_BUILD_PATH)/bin/involflt.sh \
		$(VX_BUILD_PATH)/bin/get_protected_dev_details.sh \
		$(VX_BUILD_PATH)/bin/involflt_start.service.systemd \
		$(VX_BUILD_PATH)/bin/involflt_stop.service.systemd \
		$(VX_BUILD_PATH)/bin/generate_device_name \
		$(VX_BUILD_PATH)/etc/notallowed_for_mountpoints.dat \
		$(VX_BUILD_PATH)/etc/inmexec_jobspec.txt \
		$(VX_BUILD_PATH)/etc/RCMInfo.conf \
		$(VX_BUILD_PATH)/transport/newcert.pem \
		$(VX_BUILD_PATH)/transport/client.pem \
		$(VX_BUILD_PATH)/transport/cxpsctl \
		$(VX_BUILD_PATH)/transport/cxps.conf \
		$(VX_BUILD_PATH)/transport/cxps \
		$(VX_BUILD_PATH)/transport/cxpscli \
		$(VX_BUILD_PATH)/bin/csgetfingerprint \
		$(VX_BUILD_PATH)/bin/gencert \
		$(VX_BUILD_PATH)/bin/genpassphrase \
		$(VX_BUILD_PATH)/bin/evtcollforw \
		$(VX_BUILD_PATH)/bin/inm_list_part \
		$(VX_BUILD_PATH)/bin/libcommon.sh

# Add MT specific binaries to common list only for MT supported platforms.
ifeq ($(X_OS),$(filter $(X_OS), RHEL6-64 RHEL7-64 UBUNTU-16.04-64 UBUNTU-20.04-64))
COMMON_LIST +=  $(VX_BUILD_PATH)/bin/cachemgr \
                $(VX_BUILD_PATH)/bin/cdpmgr
endif

$(COMMON_LIST):
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)cp -r $< $@
	$(RULE_SEPARATOR)

ifeq ($(NON_RPM), YES)
$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/OS_details.sh:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)cp -r $< $@
	$(RULE_SEPARATOR)
else
$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/OS_details.sh \
$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/$(COMPANY)Vx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)cp -r $< $@
	$(RULE_SEPARATOR)
endif

ifeq ($(X_OS),$(filter $(X_OS), UBUNTU-14.04-64 UBUNTU-16.04-64 UBUNTU-18.04-64 UBUNTU-20.04-64 UBUNTU-22.04-64 DEBIAN7-64 DEBIAN8-64 DEBIAN9-64 DEBIAN10-64 DEBIAN11-64))
copy_filter_driver: drivers/InVolFlt/linux/ubuntu_drivers
	$(VERBOSE)mkdir -p $(VX_BUILD_PATH)/bin/drivers
	$(VERBOSE)cp $</involflt.ko* $(VX_BUILD_PATH)/bin/drivers
	$(RULE_SEPARATOR)
else
    ifeq ($(X_OS),$(filter $(X_OS), SLES12-64 SLES15-64))
        copy_filter_driver: drivers/InVolFlt/linux/sles_drivers
	    $(VERBOSE)mkdir -p $(VX_BUILD_PATH)/bin/drivers
	    $(VERBOSE)cp $</involflt.ko* $</supported_kernels $(VX_BUILD_PATH)/bin/drivers
	    $(RULE_SEPARATOR)
else
    ifeq ($(X_OS),$(filter $(X_OS), OL7-64 OL8-64  OL9-64)) 
# For new distros, ship in drivers directory
        copy_filter_driver: drivers/InVolFlt/linux
	    $(VERBOSE)mkdir -p $(VX_BUILD_PATH)/bin/drivers
	    $(VERBOSE)cp $</involflt.ko* $(VX_BUILD_PATH)/bin/drivers
	    $(RULE_SEPARATOR)
    else
# For old distros, ship in drivers directory
        copy_filter_driver: drivers/InVolFlt/linux
	    $(VERBOSE)cp $</involflt.ko* $(VX_BUILD_PATH)/bin
	    $(RULE_SEPARATOR)
    endif
endif
endif

ifeq ($(X_OS), RHEL6-64)
copy_centosplus_64_bit_drivers: drivers/InVolFlt/linux
	$(VERBOSE)cp drivers/InVolFlt/linux/involflt.ko* $(VX_BUILD_PATH)/bin/involflt.ko.2.6.32-71.el6.centos.plus.x86_64
	$(RULE_SEPARATOR)
endif

copy_initrd_generic_scripts: drivers/InVolFlt/linux/user_space/initrd
	$(VERBOSE)mkdir -p $(VX_BUILD_PATH)/scripts/initrd
	$(VERBOSE)cp $</*.sh $(VX_BUILD_PATH)/scripts/initrd
	$(RULE_SEPARATOR)

ifeq ($(X_OS),$(filter $(X_OS), OL6-64 RHEL6-64 RHEL7-64 RHEL8-64 RHEL9-64 SLES12-64 SLES15-64 OL7-64 OL8-64  OL9-64))
copy_initrd_specific_scripts: drivers/InVolFlt/linux/user_space/initrd/dracut
	$(VERBOSE)cp -r $</* $(VX_BUILD_PATH)/scripts/initrd
	$(VERBOSE)find $(VX_BUILD_PATH)/scripts/initrd -type d -name "CVS" | xargs rm -rf
	$(RULE_SEPARATOR)
endif

ifeq ($(X_OS),$(filter $(X_OS), SLES11-SP3-64 SLES11-SP4-64))
copy_initrd_specific_scripts: drivers/InVolFlt/linux/user_space/initrd/sles11
	$(VERBOSE)cp -r $</* $(VX_BUILD_PATH)/scripts/initrd
	$(VERBOSE)find $(VX_BUILD_PATH)/scripts/initrd -type d -name "CVS" | xargs rm -rf
	$(RULE_SEPARATOR)
endif

ifeq ($(X_OS),$(filter $(X_OS), UBUNTU-14.04-64 UBUNTU-16.04-64 UBUNTU-18.04-64 UBUNTU-20.04-64 UBUNTU-22.04-64 DEBIAN7-64 DEBIAN8-64 DEBIAN9-64 DEBIAN10-64 DEBIAN11-64))
copy_initrd_specific_scripts: drivers/InVolFlt/linux/user_space/initrd/initramfs-tools
	$(VERBOSE)cp -r $</* $(VX_BUILD_PATH)/scripts/initrd
	$(VERBOSE)find $(VX_BUILD_PATH)/scripts/initrd -type d -name "CVS" | xargs rm -rf
	$(RULE_SEPARATOR)
endif

giving_recurssive_permissions:
	$(VERBOSE)cd $(VX_BUILD_PATH)
	$(VERBOSE)chmod -R 755 $(VX_BUILD_PATH)/bin/* $(VX_BUILD_PATH)/scripts/* $(VX_BUILD_PATH)/etc/*
	$(RULE_SEPARATOR)

# Copying involflt.conf to /etc/init for UBUNTU-14.04-64/UBUNTU-16.04-64/UBUNTU-18.04-64/UBUNTU-20.04-64/UBUNTU-22.04-64/DEBIAN7-64/DEBIAN8-64/DEBIAN9-64/DEBIAN10-64/DEBIAN11-64
ifeq ($(X_OS),$(filter $(X_OS), UBUNTU-14.04-64 UBUNTU-16.04-64 UBUNTU-18.04-64 UBUNTU-20.04-64 UBUNTU-22.04-64 DEBIAN7-64 DEBIAN8-64 DEBIAN9-64 DEBIAN10-64 DEBIAN11-64))
involflt_conf:
	$(VERBOSE)cp ../host/drivers/InVolFlt/linux/user_space/hotplug/involflt.conf $(VX_BUILD_PATH)/etc/involflt.conf
involflt.ubuntu:
	$(VERBOSE)cp ../host/drivers/InVolFlt/linux/user_space/shutdown_script/involflt.ubuntu $(VX_BUILD_PATH)/etc/involflt.ubuntu
vxagent_ubuntu1604:
	$(VERBOSE)cp ../build/scripts/VX/templates/vxagent_ubuntu1604 $(VX_BUILD_PATH)/bin/vxagent_ubuntu1604
uarespawnd_ubuntu1604:
	$(VERBOSE)cp ../build/scripts/VX/templates/uarespawnd_ubuntu1604 $(VX_BUILD_PATH)/bin/uarespawnd_ubuntu1604
involflt_start.sysvinit:
	$(VERBOSE)cp ../host/drivers/InVolFlt/linux/user_space/hotplug/involflt_start.sysvinit	$(VX_BUILD_PATH)/etc/involflt_start.sysvinit
endif

RPM_DIR_LEAF = BUILD RPMS SOURCES SPECS SRPMS
RPM_DIRS_VX = $(addprefix $(X_ARCH)/setup_vx/$(X_CONFIGURATION)/,$(RPM_DIR_LEAF))

ifeq ($(NON_RPM), YES)
$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/$(COMPANY)_VX_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_OS).tar: \
	$(COMMON_LIST) \
	$(VX_BUILD_PATH)/etc/drscout.conf \
	$(VX_BUILD_PATH)/bin/start \
	$(VX_BUILD_PATH)/bin/status \
	$(VX_BUILD_PATH)/bin/stop \
	$(VX_BUILD_PATH)/bin/TuneMaxRep.sh \
	$(VX_BUILD_PATH)/bin/unregister \
	$(VX_BUILD_PATH)/bin/uninstall \
	$(VX_BUILD_PATH)/scripts \
	copy_filter_driver \
	copy_initrd_generic_scripts \
        copy_initrd_specific_scripts \
	involflt_conf \
	involflt.ubuntu \
	vxagent_ubuntu1604 \
	uarespawnd_ubuntu1604 \
	involflt_start.sysvinit \
	giving_recurssive_permissions \

	$(VERBOSE)cd $(X_ARCH)/setup_vx/$(X_CONFIGURATION) ;tar cvf $(COMPANY)_VX_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_OS).tar bin etc scripts transport
	$(RULE_SEPARATOR)

else
$(VX_TOP_DIR)/RPMS/$(RPM_ARCH_LEVEL)/$(COMPANY)Vx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm: \
../build/scripts/VX/templates/vx$(X_PARTNER).spec \
	$(COMMON_LIST) \
	$(VX_BUILD_PATH)/etc/drscout.conf \
	$(VX_BUILD_PATH)/bin/start \
	$(VX_BUILD_PATH)/bin/status \
	$(VX_BUILD_PATH)/bin/stop \
	$(VX_BUILD_PATH)/bin/TuneMaxRep.sh \
	$(VX_BUILD_PATH)/bin/vxagent \
	$(VX_BUILD_PATH)/bin/unregister \
	$(VX_BUILD_PATH)/bin/uninstall \
	$(VX_BUILD_PATH)/scripts \
	copy_filter_driver \
        copy_initrd_generic_scripts \
        copy_initrd_specific_scripts \
	copy_centosplus_64_bit_drivers \
	giving_recurssive_permissions \

	$(VERBOSE)mkdir -p $(RPM_DIRS_VX)
	$(VERBOSE)rpmbuild -bb --define="_topdir $(VX_TOP_DIR)" --define="_version $(X_VERSION_DOTTED)" $< --with $(OS_FOR_RPM)
	$(RULE_SEPARATOR)
endif

COMMON_BINARIES = $(VX_BUILD_PATH)/bin/svagentsA2A $(VX_BUILD_PATH)/bin/svagentsV2A $(VX_BUILD_PATH)/bin/s2A2A $(VX_BUILD_PATH)/bin/s2V2A $(VX_BUILD_PATH)/bin/cdpcli $(VX_BUILD_PATH)/bin/dataprotection $(VX_BUILD_PATH)/bin/DataProtectionSyncRcm $(VX_BUILD_PATH)/bin/unregagent $(VX_BUILD_PATH)/bin/svdcheck $(VX_BUILD_PATH)/bin/testvolumeinfocollector $(VX_BUILD_PATH)/bin/hostconfigcli $(VX_BUILD_PATH)/bin/customdevicecli $(VX_BUILD_PATH)/bin/vacp $(VX_BUILD_PATH)/bin/cachemgr $(VX_BUILD_PATH)/bin/cxcli $(VX_BUILD_PATH)/bin/EsxUtil $(VX_BUILD_PATH)/bin/pushinstallclient $(VX_BUILD_PATH)/bin/appservice $(VX_BUILD_PATH)/bin/FailoverCommandUtil $(VX_BUILD_PATH)/bin/fabricagent $(VX_BUILD_PATH)/bin/cxpsclient $(VX_BUILD_PATH)/bin/inm_scsi_id $(VX_BUILD_PATH)/bin/inmexec $(VX_BUILD_PATH)/bin/ScoutTuning $(VX_BUILD_PATH)/bin/csgetfingerprint $(VX_BUILD_PATH)/bin/gencert $(VX_BUILD_PATH)/bin/genpassphrase $(VX_BUILD_PATH)/bin/AzureRecoveryUtil $(VX_BUILD_PATH)/bin/AzureRcmCli $(VX_BUILD_PATH)/bin/evtcollforw

vx_build:  $(COMMON_BINARIES)

endif
