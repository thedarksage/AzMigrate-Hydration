# ----------------------------------------------------------------------
# create FX setup package
# ----------------------------------------------------------------------

include ../build/scripts/FX/build_params_$(X_PARTNER)_$(X_MINOR_ARCH).mak
ifndef X_ALREADY_INCLUDED
X_OS:=$(shell ../build/scripts/general/OS_details.sh 1)
X_KERVER:=$(shell uname -r)

verify:
	@echo $(X_OS)

ifeq ($(X_OS),$(filter $(X_OS), UBUNTU-14.04-64 UBUNTU-16.04-64 UBUNTU-18.04-64 UBUNTU-20.04-64 UBUNTU-22.04-64 DEBIAN7-64 DEBIAN8-64 DEBIAN9-64 DEBIAN10-64 DEBIAN11-64))
  NON_RPM=YES
endif

ifeq ($(NON_RPM), YES)
FX_TAR_NAME := $(COMPANY)_FX_$(X_VERSION_DOTTED)_$(X_OS)_$(X_VERSION_PHASE)_$(shell date "+%d%h%Y")_$(X_CONFIGURATION).tar.gz
else
FX_TAR_NAME := $(COMPANY)_FX_$(X_VERSION_DOTTED)_$(X_OS)_$(X_VERSION_PHASE)_$(shell date "+%d%h%Y")_$(X_CONFIGURATION).tar.gz
endif

fx_setup: $(X_ARCH)/setup/$(X_CONFIGURATION)/$(FX_TAR_NAME)

clean_fx_setup:
	$(VERBOSE)rm -rf $(X_ARCH)/setup/$(X_CONFIGURATION)

ifeq ($(NON_RPM), YES)
$(X_ARCH)/setup/$(X_CONFIGURATION)/$(FX_TAR_NAME):\
	$(X_ARCH)/setup/$(X_CONFIGURATION)/$(COMPANY)_FX_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_OS).tar \
	$(X_ARCH)/setup/$(X_CONFIGURATION)/install \
	$(X_ARCH)/setup/$(X_CONFIGURATION)/OS_details.sh \
	$(X_ARCH)/setup/$(X_CONFIGURATION)/$(VERSION_FILE_NAME) \
	$(X_ARCH)/setup/$(X_CONFIGURATION)/$(BUILD_MANIFEST_FILE_NAME) \
	$(X_ARCH)/setup/$(X_CONFIGURATION)/conf_file \

	$(VERBOSE)cd $(dir $<);tar cvzf $(notdir $@) $(notdir $^)
	$(RULE_SEPARATOR)

else	
$(X_ARCH)/setup/$(X_CONFIGURATION)/$(FX_TAR_NAME):\
	$(X_ARCH)/setup/$(X_CONFIGURATION)/$(COMPANY)Fx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm \
	$(X_ARCH)/setup/$(X_CONFIGURATION)/install \
	$(X_ARCH)/setup/$(X_CONFIGURATION)/conf_file \
	$(X_ARCH)/setup/$(X_CONFIGURATION)/$(VERSION_FILE_NAME) \
	$(X_ARCH)/setup/$(X_CONFIGURATION)/$(BUILD_MANIFEST_FILE_NAME) \
	$(X_ARCH)/setup/$(X_CONFIGURATION)/OS_details.sh \

	$(VERBOSE)cd $(dir $<);tar cvzf $(notdir $@) $(notdir $^)
	$(RULE_SEPARATOR)
endif

ifeq ($(NON_RPM), YES)
  FX_TOP_DIR=$(CURDIR)/$(X_ARCH)/setup/$(X_CONFIGURATION)
  FX_BUILD_PATH=$(FX_TOP_DIR)
else
  FX_TOP_DIR=$(CURDIR)/$(X_ARCH)/setup/$(X_CONFIGURATION)
  FX_BUILD_ROOT=$(FX_TOP_DIR)/$(COMPANY)Fx-buildroot
  FX_BUILD_PATH=$(FX_BUILD_ROOT)/$(COMPANY)/Fx
endif

# All the scripts parameterized by template values
$(FX_BUILD_PATH)/stop: ../build/scripts/FX/templates/stop
$(FX_BUILD_PATH)/start: ../build/scripts/FX/templates/start
$(FX_BUILD_PATH)/status: ../build/scripts/FX/templates/status
$(FX_BUILD_PATH)/uninstall: ../build/scripts/FX/templates/uninstall
$(FX_BUILD_PATH)/unregister: ../build/scripts/FX/templates/unregister
$(X_ARCH)/setup/$(X_CONFIGURATION)/install: ../build/scripts/FX/templates/install
$(X_ARCH)/setup/$(X_CONFIGURATION)/conf_file: ../build/scripts/FX/templates/conf_file

ifeq ($(NON_RPM), YES)
$(FX_BUILD_PATH)/stop \
$(FX_BUILD_PATH)/start \
$(FX_BUILD_PATH)/status \
$(FX_BUILD_PATH)/uninstall \
$(FX_BUILD_PATH)/unregister \
$(X_ARCH)/setup/$(X_CONFIGURATION)/install:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)sed   -e "s|PARTNER_OR_COMPANY|$(COMPANY)|g" \
		-e "s|CX|$(LABEL)|g" \
		-e "s|DEFAULT_PARTNER_INSTALL_DIR|$(DEFAULT_INSTALL_DIR)|g"\
		-e "s|BUILDTIME_CURRENT_VERSION|$(X_VERSION_DOTTED)|" \
		-e "s|BUILDTIME_INST_VERSION_FILE|$(VERSION_FILE_NAME)|" \
		-e "s|BUILDTIME_VALUE_INST_VERSION_FILE|$(VERSION_FILE_NAME)|" \
		-e "s|BUILDTIME_BUILD_MANIFEST_FILE|$(BUILD_MANIFEST_FILE_NAME)|" \
		-e "s|BUILDTIME_VALUE_BLD_MANIFEST_FILE_NAME|$(BUILD_MANIFEST_FILE_NAME)|" \
		-e "s|BUILDTIME_DEFAULT_UPGRADE_DIR|$(DEFAULT_UPG_DIR)|" \
		-e "s|BUILDTIME_PARTNER_SPECIFIC_STRING|$(PARTNER_SPECIFIC_STRING)|" \
		-e "s|BUILDTIME_REF_DIR|$(METADATA_REF_DIR)|" \
		-e "s|BUILDTIME_VALUE_REF_DIR|$(METADATA_REF_DIR)|" \
		$< > $@
	$(VERBOSE)chmod 755 $@
	$(RULE_SEPARATOR)
else
$(FX_BUILD_PATH)/stop \
$(FX_BUILD_PATH)/start \
$(FX_BUILD_PATH)/status \
$(FX_BUILD_PATH)/uninstall \
$(FX_BUILD_PATH)/unregister \
$(X_ARCH)/setup/$(X_CONFIGURATION)/install:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)sed   -e "s|PARTNER_OR_COMPANY|$(COMPANY)|g" \
		-e "s|CX|$(LABEL)|g" \
		-e "s|DEFAULT_PARTNER_INSTALL_DIR|$(DEFAULT_INSTALL_DIR)|g"\
		-e "s|BUILDTIME_CURRENT_VERSION|$(X_VERSION_DOTTED)|" \
		-e "s|BUILDTIME_INST_VERSION_FILE|$(VERSION_FILE_NAME)|" \
		-e "s|BUILDTIME_VALUE_INST_VERSION_FILE|$(VERSION_FILE_NAME)|" \
		-e "s|BUILDTIME_BUILD_MANIFEST_FILE|$(BUILD_MANIFEST_FILE_NAME)|" \
		-e "s|BUILDTIME_VALUE_BLD_MANIFEST_FILE_NAME|$(BUILD_MANIFEST_FILE_NAME)|" \
		-e "s|BUILDTIME_MAIN_RPM_FILE|$(BUILT_RPM_FILE)|" \
		-e "s|BUILDTIME_MAIN_RPM_PACKAGE|$(BUILT_RPM_PACKAGE)|" \
		-e "s|BUILDTIME_VALUE_RPM_PACKAGE|$(BUILT_RPM_PACKAGE)|" \
		-e "s|BUILDTIME_DEFAULT_UPGRADE_DIR|$(DEFAULT_UPG_DIR)|" \
		-e "s|BUILDTIME_PARTNER_SPECIFIC_STRING|$(PARTNER_SPECIFIC_STRING)|" \
		-e "s|BUILDTIME_REF_DIR|$(METADATA_REF_DIR)|" \
		-e "s|BUILDTIME_VALUE_REF_DIR|$(METADATA_REF_DIR)|" \
		$< > $@
	$(VERBOSE)chmod 755 $@
	$(RULE_SEPARATOR)
endif

$(X_ARCH)/setup/$(X_CONFIGURATION)/conf_file:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)sed   -e "s|DEFAULT_PARTNER_INSTALL_DIR|$(DEFAULT_INSTALL_DIR)|g" \
                $< > $@
	$(VERBOSE)chmod 755 $@
	$(RULE_SEPARATOR)

ifeq ($(NON_RPM), YES)
$(X_ARCH)/setup/$(X_CONFIGURATION)/$(VERSION_FILE_NAME): $(X_ARCH)/branding/platform-* $(X_ARCH)/branding/version-*
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)echo VERSION=$(X_VERSION_DOTTED) > $@
	$(VERBOSE)echo PROD_VERSION=$(PROD_VERSION) >> $@
	$(VERBOSE)echo PARTNER=$(X_PARTNER) >> $@
	$(VERBOSE)echo INSTALLATION_DIR=RUNTIME_INSTALL_PATH >> $@
	$(VERBOSE)echo VERSION_FILE_PARENT_DIR=$(METADATA_REF_DIR) >> $@
	$(VERBOSE)echo DEFAULT_UPGRADE_DIR=$(DEFAULT_UPG_DIR) >> $@
	$(VERBOSE)echo BUILD_TAG=$(X_VERSION_QUALITY)_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_VERSION_PHASE)_$(X_VERSION_BUILD_NUM)_$(shell date "+%h_%d_%Y")_$(shell echo $(X_PARTNER) | tr 'a-z' 'A-Z') >> $@
	$(VERBOSE)echo OS_BUILT_FOR=$(subst platform-,,$(notdir $<)) >> $@
	$(VERBOSE)echo TAR_FILE_NAME=$(COMPANY)_FX_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_OS).tar >> $@
	$(RULE_SEPARATOR)
else
$(X_ARCH)/setup/$(X_CONFIGURATION)/$(VERSION_FILE_NAME): $(X_ARCH)/branding/platform-* $(X_ARCH)/branding/version-*
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)echo VERSION=$(X_VERSION_DOTTED) > $@
	$(VERBOSE)echo PROD_VERSION=$(PROD_VERSION) >> $@
	$(VERBOSE)echo PARTNER=$(X_PARTNER) >> $@
	$(VERBOSE)echo INSTALLATION_DIR=RUNTIME_INSTALL_PATH >> $@
	$(VERBOSE)echo RPM_PACKAGE=$(BUILT_RPM_PACKAGE) >> $@
	$(VERBOSE)echo RPM_NAME=$(BUILT_RPM_FILE) >> $@
	$(VERBOSE)echo VERSION_FILE_PARENT_DIR=$(METADATA_REF_DIR) >> $@
	$(VERBOSE)echo DEFAULT_UPGRADE_DIR=$(DEFAULT_UPG_DIR) >> $@
	$(VERBOSE)echo BUILD_TAG=$(X_VERSION_QUALITY)_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_VERSION_PHASE)_$(X_VERSION_BUILD_NUM)_$(shell date "+%h_%d_%Y")_$(shell echo $(X_PARTNER) | tr 'a-z' 'A-Z') >> $@
	$(VERBOSE)echo OS_BUILT_FOR=$(subst platform-,,$(notdir $<)) >> $@
	$(RULE_SEPARATOR)
endif

# TODO: manifest omits the list of files that used to exist.
$(X_ARCH)/setup/$(X_CONFIGURATION)/$(BUILD_MANIFEST_FILE_NAME):
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)if [ -f fr_common/CVS/Tag ]; then \
				echo CODE_BRANCH=`cat fr_common/CVS/Tag | cut -d'T' -f2-`>$@;\
			  else \
				echo CODE_BRANCH=HEAD > $@; \
			  fi
	$(VERBOSE)echo >> $@
	$(RULE_SEPARATOR)

# All the files simply copied from one place to another
$(X_ARCH)/setup/$(X_CONFIGURATION)/OS_details.sh: ../build/scripts/general/OS_details.sh

$(FX_BUILD_PATH)/svfrd: $(X_ARCH)/frdaemon/$(X_CONFIGURATION)/svfrd
$(FX_BUILD_PATH)/cxcli: $(X_ARCH)/cxcli/$(X_CONFIGURATION)/cxcli
$(FX_BUILD_PATH)/unregfragent: $(X_ARCH)/fr_unregagent/$(X_CONFIGURATION)/unregfragent
$(FX_BUILD_PATH)/vacp: $(X_ARCH)/vacpunix/$(X_CONFIGURATION)/vacp
$(FX_BUILD_PATH)/csgetfingerprint: $(X_ARCH)/csgetfingerprint/$(X_CONFIGURATION)/csgetfingerprint
$(FX_BUILD_PATH)/gencert: $(X_ARCH)/gencert/$(X_CONFIGURATION)/gencert
$(FX_BUILD_PATH)/genpassphrase: $(X_ARCH)/genpassphrase/$(X_CONFIGURATION)/genpassphrase
$(FX_BUILD_PATH)/cx_backup_withfx.sh: ../server/linux/cx_backup_withfx.sh
$(FX_BUILD_PATH)/linuxshare_pre.sh: vacp/ACM_SCRIPTS/linux/linuxshare_pre.sh
$(FX_BUILD_PATH)/linuxshare_post.sh: vacp/ACM_SCRIPTS/linux/linuxshare_post.sh
$(FX_BUILD_PATH)/killchildren: ../build/scripts/FX/templates/killchildren
$(FX_BUILD_PATH)/config.ini: ../build/scripts/FX/templates/config.ini
$(FX_BUILD_PATH)/svagent: ../build/scripts/FX/templates/svagent
$(FX_BUILD_PATH)/inmsync: $(X_ARCH)/inmsync/$(X_CONFIGURATION)/inmsync
$(FX_BUILD_PATH)/transport/cxps: $(X_ARCH)/cxps/$(X_CONFIGURATION)/cxps
$(FX_BUILD_PATH)/transport/cxps.conf: ../host/cxpslib/cxps.conf
$(FX_BUILD_PATH)/transport/pem/servercert.pem: ../host/cxpslib/pem/servercert.pem
$(FX_BUILD_PATH)/transport/pem/dh.pem: ../host/cxpslib/pem/dh.pem

ifeq ($(NON_RPM), YES)
COMMON_LIST := $(FX_BUILD_PATH)/svfrd $(FX_BUILD_PATH)/unregfragent $(FX_BUILD_PATH)/vacp $(FX_BUILD_PATH)/cx_backup_withfx.sh $(FX_BUILD_PATH)/linuxshare_pre.sh $(FX_BUILD_PATH)/linuxshare_post.sh $(FX_BUILD_PATH)/killchildren $(FX_BUILD_PATH)/config.ini $(FX_BUILD_PATH)/svagent $(FX_BUILD_PATH)/inmsync $(FX_BUILD_PATH)/cxcli $(FX_BUILD_PATH)/csgetfingerprint $(FX_BUILD_PATH)/gencert $(FX_BUILD_PATH)/genpassphrase
else
COMMON_LIST := $(FX_BUILD_PATH)/svfrd $(FX_BUILD_PATH)/unregfragent $(FX_BUILD_PATH)/vacp $(FX_BUILD_PATH)/cx_backup_withfx.sh $(FX_BUILD_PATH)/linuxshare_pre.sh $(FX_BUILD_PATH)/linuxshare_post.sh $(FX_BUILD_PATH)/killchildren $(FX_BUILD_PATH)/config.ini $(FX_BUILD_PATH)/svagent $(FX_BUILD_PATH)/inmsync $(FX_BUILD_PATH)/cxcli $(FX_BUILD_PATH)/transport/cxps $(FX_BUILD_PATH)/transport/cxps.conf $(FX_BUILD_PATH)/transport/pem/servercert.pem $(FX_BUILD_PATH)/transport/pem/dh.pem $(FX_BUILD_PATH)/csgetfingerprint $(FX_BUILD_PATH)/gencert $(FX_BUILD_PATH)/genpassphrase
endif

$(COMMON_LIST):
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)cp -r $< $@
	$(RULE_SEPARATOR)

ifeq ($(X_OS),RHEL3-32)
$(FX_BUILD_PATH)/scripts/ESX/get_source_esx_info.pl: ../Solutions/ESX_FX/get_source_esx_info.pl
$(FX_BUILD_PATH)/scripts/ESX/make_target_esx_vms.pl: ../Solutions/ESX_FX/make_target_esx_vms.pl
$(FX_BUILD_PATH)/lib/libgcc_s.so.1: ../thirdparty/linux/RHEL3-32/lib/libgcc_s.so.1
$(FX_BUILD_PATH)/lib/libstdc++.so.6: ../thirdparty/linux/RHEL3-32/lib/libstdc++.so.6
endif

ifneq ($(NON_RPM), YES)
$(X_ARCH)/setup/$(X_CONFIGURATION)/$(COMPANY)Fx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm: \
$(FX_TOP_DIR)/RPMS/$(RPM_ARCH_LEVEL)/$(COMPANY)Fx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm
endif

$(FX_BUILD_PATH)/scripts: ../Solutions/Oracle/Fx
	$(VERBOSE)mkdir -p $(FX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../Solutions/Oracle/Fx/Cx/*.sh $(FX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../Solutions/Oracle/Fx/Source/*.sh $(FX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../Solutions/Oracle/Fx/Target/*.sh $(FX_BUILD_PATH)/scripts
	$(VERBOSE)cp -r ../Solutions/Oracle/Fx/Pillar/*.sh $(FX_BUILD_PATH)/scripts
	$(RULE_SEPARATOR)

$(FX_BUILD_PATH)/fabric:../Solutions/Fabric/Oracle/FX
	 $(VERBOSE)mkdir -p $(FX_BUILD_PATH)/fabric/scripts
	$(VERBOSE)cp -r $</*.sh $(FX_BUILD_PATH)/fabric/scripts
	$(RULE_SEPARATOR)

$(FX_BUILD_PATH)/transport: $(X_ARCH)/cxps/$(X_CONFIGURATION)/cxps
	$(VERBOSE)mkdir -p $(FX_BUILD_PATH)/transport/pem
	$(VERBOSE)cp -r ./$(X_ARCH)/cxps/$(X_CONFIGURATION)/cxps $(FX_BUILD_PATH)/transport/cxps
	$(VERBOSE)cp -r ./cxpslib/cxps.conf $(FX_BUILD_PATH)/transport/cxps.conf 
	$(VERBOSE)cp -r ./cxpslib/pem/servercert.pem $(FX_BUILD_PATH)/transport/pem/servercert.pem
	$(VERBOSE)cp -r ./cxpslib/pem/dh.pem $(FX_BUILD_PATH)/transport/pem/dh.pem
	$(RULE_SEPARATOR)

ifeq ($(NON_RPM), YES)
$(X_ARCH)/setup/$(X_CONFIGURATION)/OS_details.sh:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)cp -r $< $@
	$(RULE_SEPARATOR)
else
$(X_ARCH)/setup/$(X_CONFIGURATION)/OS_details.sh \
$(X_ARCH)/setup/$(X_CONFIGURATION)/$(COMPANY)Fx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)cp -r $< $@
	$(RULE_SEPARATOR)
endif

ifeq ($(X_OS),RHEL3-32)
EXTRA_FILES := $(FX_BUILD_PATH)/scripts/ESX/get_source_esx_info.pl $(FX_BUILD_PATH)/scripts/ESX/make_target_esx_vms.pl $(FX_BUILD_PATH)/lib/libgcc_s.so.1 $(FX_BUILD_PATH)/lib/libstdc++.so.6
$(EXTRA_FILES):
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)cp -r $< $@
	$(RULE_SEPARATOR)
endif

ifneq ($(NON_RPM), YES)
RPM_DIR_LEAF = BUILD RPMS SOURCES SPECS SRPMS
RPM_DIRS = $(addprefix $(X_ARCH)/setup/$(X_CONFIGURATION)/,$(RPM_DIR_LEAF))
endif

ifeq ($(NON_RPM), YES)
$(X_ARCH)/setup/$(X_CONFIGURATION)/$(COMPANY)_FX_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_OS).tar: \
	$(FX_BUILD_PATH)/stop \
	$(FX_BUILD_PATH)/start \
	$(FX_BUILD_PATH)/status \
	$(FX_BUILD_PATH)/uninstall \
	$(FX_BUILD_PATH)/unregister \
	$(FX_BUILD_PATH)/fabric \
	$(FX_BUILD_PATH)/scripts \
	$(FX_BUILD_PATH)/transport \
	$(COMMON_LIST)

	$(VERBOSE)cd $(dir $<);tar cvf $(notdir $@) $(notdir $^)	
	$(RULE_SEPARATOR)
else
$(FX_TOP_DIR)/RPMS/$(RPM_ARCH_LEVEL)/$(COMPANY)Fx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm: \
../build/scripts/FX/templates/$(X_PARTNER).spec \
	$(FX_BUILD_PATH)/stop \
	$(FX_BUILD_PATH)/start \
	$(FX_BUILD_PATH)/status \
	$(FX_BUILD_PATH)/uninstall \
	$(FX_BUILD_PATH)/unregister \
	$(FX_BUILD_PATH)/fabric \
	$(FX_BUILD_PATH)/scripts \
	$(COMMON_LIST) \
	$(EXTRA_FILES) \

	$(VERBOSE)mkdir -p $(RPM_DIRS)
	$(VERBOSE)rpmbuild -bb --define="_topdir $(FX_TOP_DIR)" --define="_version $(X_VERSION_DOTTED)" $<
	$(RULE_SEPARATOR)

endif

fx_build:	$(FX_BUILD_PATH)/svfrd \
		$(FX_BUILD_PATH)/unregfragent \
		$(FX_BUILD_PATH)/cxcli \
		$(FX_BUILD_PATH)/vacp \
		$(FX_BUILD_PATH)/inmsync \
		$(FX_BUILD_PATH)/transport/cxps \
		$(FX_BUILD_PATH)/csgetfingerprint \
		$(FX_BUILD_PATH)/gencert \
		$(FX_BUILD_PATH)/genpassphrase
	$(RULE_SEPARATOR)

clean_fx_build:
	$(VERBOSE)rm -f $(FX_BUILD_PATH)/svfrd $(X_ARCH)/frdaemon/$(X_CONFIGURATION)/svfrd \
		$(FX_BUILD_PATH)/inmsync $(X_ARCH)/inmsync/$(X_CONFIGURATION)/inmsync \
		$(FX_BUILD_PATH)/unregfragent $(X_ARCH)/fr_unregagent/$(X_CONFIGURATION)/unregfragent \
		$(FX_BUILD_PATH)/vacp $(X_ARCH)/vacpunix/$(X_CONFIGURATION)/vacp \
		$(FX_BUILD_PATH)/transport/cxps $(X_ARCH)/cxps/$(X_CONFIGURATION)/cxps \
		$(FX_BUILD_PATH)/csgetfingerprint $(X_ARCH)/csgetfingerprint/$(X_CONFIGURATION)/csgetfingerprint \
		$(FX_BUILD_PATH)/gencert $(X_ARCH)/gencert/$(X_CONFIGURATION)/gencert \
		$(FX_BUILD_PATH)/genpassphrase $(X_ARCH)/genpassphrase/$(X_CONFIGURATION)/genpassphrase
	$(RULE_SEPARATOR)

endif
