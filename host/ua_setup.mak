# ----------------------------------------------------------------------
# create UA setup_ua package
# ----------------------------------------------------------------------

include ../build/scripts/VX/build_params_$(X_PARTNER).mak
include ../build/scripts/FX/build_params_$(X_PARTNER)_$(X_MINOR_ARCH).mak
include ../build/scripts/VX/build_params_$(X_PARTNER)_linux.mak
ifndef X_ALREADY_INCLUDED

X_OS:=$(shell ../build/scripts/general/OS_details.sh 1)

ifeq ($(NON_RPM), YES)
UA_TAR_NAME := Microsoft-ASR_UA_$(X_VERSION_DOTTED)_$(X_OS)_$(X_VERSION_PHASE)_$(shell date "+%d%h%Y")_$(X_CONFIGURATION).tar.gz
ifeq ($(X_OS),$(filter $(X_OS), DEBIAN7-64 DEBIAN8-64 DEBIAN9-64 DEBIAN10-64 DEBIAN11-64))
	TAR_OPT := czvf
else
	TAR_OPT := --lzma -cvf
endif
else
UA_TAR_NAME := Microsoft-ASR_UA_$(X_VERSION_DOTTED)_$(X_OS)_$(X_VERSION_PHASE)_$(shell date "+%d%h%Y")_$(X_CONFIGURATION).tar.gz
endif

ua_setup: fx_setup vx_setup ua_packaging symbol_generation

ua_packaging: $(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(UA_TAR_NAME)

clean_ua_setup:
	      $(VERBOSE)rm -rf $(X_ARCH)/setup_ua/$(X_CONFIGURATION)
	      $(VERBOSE)rm -rf $(X_ARCH)/setup_vx/$(X_CONFIGURATION)
	      $(VERBOSE)rm -rf $(X_ARCH)/setup_fx/$(X_CONFIGURATION)

ifeq ($(NON_RPM), YES)
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(UA_TAR_NAME):\
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(COMPANY)_VX_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_OS).tar \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/install \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/uninstall.sh \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/install_vx \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/teleAPIs.sh \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/jq \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/conf_file \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/ApplyCustomChanges.sh \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(VX_VERSION_FILE_NAME) \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/OS_details.sh \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/Third-Party_Notices.txt \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/csgetfingerprint \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/gencert \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/genpassphrase \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/prereq_check_install.sh \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/prereq_check_installer.json \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/libcommon.sh \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/supported_kernels \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/inm_list_part \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/spv.json \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/AgentUpgrade.sh

	$(VERBOSE)cd $(dir $<);tar $(TAR_OPT) $(notdir $@) $(notdir $^)
	$(RULE_SEPARATOR)
else
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(UA_TAR_NAME):\
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(COMPANY)Vx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/install \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/uninstall.sh \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/install_vx \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/teleAPIs.sh \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/jq \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/conf_file \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/ApplyCustomChanges.sh \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(VX_VERSION_FILE_NAME) \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/OS_details.sh \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/Third-Party_Notices.txt \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/csgetfingerprint \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/gencert \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/genpassphrase \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/prereq_check_install.sh \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/prereq_check_installer.json \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/libcommon.sh \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/supported_kernels \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/inm_list_part \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/spv.json \
	$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/AgentUpgrade.sh

	$(VERBOSE)cd $(dir $<);tar cvzf $(notdir $@) $(notdir $^)
	$(RULE_SEPARATOR)
endif

UA_TOP_DIR=$(CURDIR)/$(X_ARCH)/setup_ua/$(X_CONFIGURATION)

$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(COMPANY)Fx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm: $(X_ARCH)/setup/$(X_CONFIGURATION)/$(COMPANY)Fx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm

$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(COMPANY)Vx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm: $(X_ARCH)/setup_vx/$(X_CONFIGURATION)/$(COMPANY)Vx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm

$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(COMPANY)_VX_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_OS).tar: $(X_ARCH)/setup_vx/$(X_CONFIGURATION)/$(COMPANY)_VX_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_OS).tar

$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(COMPANY)_FX_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_OS).tar: $(X_ARCH)/setup/$(X_CONFIGURATION)/$(COMPANY)_FX_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_OS).tar

$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/install: ../build/scripts/unifiedagent/templates/install
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/uninstall.sh: ../build/scripts/unifiedagent/templates/uninstall.sh
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/install_vx: $(X_ARCH)/setup_vx/$(X_CONFIGURATION)/install
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/teleAPIs.sh: ../build/scripts/unifiedagent/templates/teleAPIs.sh
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/jq: $(JQ_ROOT)/jq
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/conf_file: ../build/scripts/unifiedagent/templates/conf_file
ifeq ($(NON_RPM), YES)
    $(X_ARCH)/setup_ua/$(X_CONFIGURATION)/ApplyCustomChanges.sh: ../build/scripts/general/ApplyCustomChanges.sh
else
    $(X_ARCH)/setup_ua/$(X_CONFIGURATION)/ApplyCustomChanges.sh: ../build/scripts/general/ApplyCustomChanges.sh.rhel7
endif
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/Third-Party_Notices.txt: ../thirdparty/Notices/Third-Party_Notices.txt
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(VX_VERSION_FILE_NAME): $(X_ARCH)/setup_vx/$(X_CONFIGURATION)/$(VX_VERSION_FILE_NAME)
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/OS_details.sh :$(X_ARCH)/setup_vx/$(X_CONFIGURATION)/OS_details.sh
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/csgetfingerprint: $(X_ARCH)/csgetfingerprint/$(X_CONFIGURATION)/csgetfingerprint
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/gencert: $(X_ARCH)/gencert/$(X_CONFIGURATION)/gencert
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/genpassphrase: $(X_ARCH)/genpassphrase/$(X_CONFIGURATION)/genpassphrase
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/prereq_check_install.sh: ../build/scripts/unifiedagent/templates/prereq_check_install.sh
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/prereq_check_installer.json: ../build/scripts/unifiedagent/templates/prereq_check_installer.json
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/libcommon.sh: ../build/scripts/unifiedagent/templates/libcommon.sh
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/AgentUpgrade.sh: ../build/scripts/unifiedagent/templates/AgentUpgrade.sh
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/spv.json:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)cat ../build/scripts/general/spv.json | $(JQ_ROOT)/jq '."$(X_OS)"' > $@
	$(RULE_SEPARATOR)

ifeq ($(NON_RPM), YES)
    $(X_ARCH)/setup_ua/$(X_CONFIGURATION)/supported_kernels: ./drivers/InVolFlt/linux/supported_kernels
else
    ifeq ($(X_OS),$(filter $(X_OS), SLES12-64 SLES15-64))
        $(X_ARCH)/setup_ua/$(X_CONFIGURATION)/supported_kernels: ./drivers/InVolFlt/linux/sles_drivers/supported_kernels
    else
        $(X_ARCH)/setup_ua/$(X_CONFIGURATION)/supported_kernels: ./drivers/InVolFlt/linux/supported_kernels
    endif
endif
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/inm_list_part: $(X_ARCH)/gfdisk/$(X_CONFIGURATION)/inm_list_part

$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/conf_file:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)sed   -e "s|DEFAULT_PARTNER_INSTALL_DIR|$(DEFAULT_INSTALL_DIR)|g" $< > $@
	$(VERBOSE)chmod 755 $@
	$(RULE_SEPARATOR)

$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/install:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)sed   -e "s|PARTNER_OR_COMPANY|$(COMPANY)|g" \
                -e "s|BUILDTIME_CURRENT_VERSION|$(X_VERSION_DOTTED)|" \
                -e "s|BUILDTIME_DEFAULT_UPGRADE_DIR|$(DEFAULT_UPG_DIR)|" \
                -e "s|BUILDTIME_REF_DIR|$(METADATA_REF_DIR)|" \
                $< > $@
	$(VERBOSE)chmod 755 $@
	$(RULE_SEPARATOR)
		
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/install_vx \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/teleAPIs.sh \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/jq \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/uninstall.sh \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(VX_VERSION_FILE_NAME) \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/ApplyCustomChanges.sh \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/Third-Party_Notices.txt \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/csgetfingerprint \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/gencert \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/genpassphrase \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/prereq_check_install.sh \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/prereq_check_installer.json \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/libcommon.sh \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/AgentUpgrade.sh \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/supported_kernels \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/inm_list_part \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/OS_details.sh:
	$(VERBOSE)mkdir -p $(UA_TOP_DIR)
	$(VERBOSE)cp -r $< $@
	$(RULE_SEPARATOR)

ifeq ($(NON_RPM), YES)
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(COMPANY)_VX_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_OS).tar \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(COMPANY)_FX_$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)_$(X_OS).tar:
	$(VERBOSE)mkdir -p $(UA_TOP_DIR)
	$(VERBOSE)cp -r $< $@
	$(RULE_SEPARATOR)
else
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(COMPANY)Vx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm \
$(X_ARCH)/setup_ua/$(X_CONFIGURATION)/$(COMPANY)Fx-$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)-1.$(RPM_ARCH_LEVEL).rpm:
	$(VERBOSE)mkdir -p $(UA_TOP_DIR)
	$(VERBOSE)cp -r $< $@
	$(RULE_SEPARATOR)
endif	

ifeq ($(X_CONFIGURATION), release)
symbol_generation:
	$(shell cd ../../../)
	$(shell) ./symbol_generation.sh $(COMPANY) $(X_VERSION_DOTTED) $(X_VERSION_PHASE) $(X_CONFIGURATION)
else
symbol_generation:
	echo "We are not building symbols for debug configuration "
endif

endif
