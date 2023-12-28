X_VERSION_MAJOR ?= 9
X_VERSION_MINOR ?= 58
X_PATCH_SET_VERSION ?=0
X_PATCH_VERSION ?=0
X_VERSION_DATE = $(shell date "+%h_%d_%Y")
X_VERSION_QUALITY ?= DAILY
X_VERSION_PHASE ?= DIT

X_VERSION_DOTTED = $(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)

X_OP:=$(shell uname)
X_OS:=$(shell ../build/scripts/general/OS_details.sh 1)
X_KERVER:=$(shell uname -r)

# ----------------------------------------------------------------------
# Platform independent way to get version build num with out the need
# for dcal_$(X_OS) binary on sun
# note in the dc calculation below:
# * 2005 is the year of the first build (i.e. build 0), 
# * handles leap years sort of. does not correctly handle centry years
#   that are not leap years (it will treat them as a leap year), the
#   first one is 2100. We can worry about it around december 2099 ;-)
# ----------------------------------------------------------------------
X_YEAR = $(shell date "+%Y")
X_JULIAN_DAY = $(shell date "+%j")
X_VERSION_BUILD_NUM := \
	$(shell echo "$(X_YEAR) 2005 - 365 * $(X_JULIAN_DAY) + $(X_YEAR) 2005 - 4 / + p q" | dc) 

# Removing the extra space at the end while calculating the daily build number
X_VERSION_BUILD_NUM := $(shell echo $(X_VERSION_BUILD_NUM) | awk '{ print $1 }')

# Reduce build number by 1 so that it will be in sync with build number on Windows platform.
X_VERSION_BUILD_NUM := $(shell echo $(X_VERSION_BUILD_NUM) 1 - p q | dc)

PROD_VERSION=$(X_VERSION_MAJOR).$(X_VERSION_MINOR).$(X_PATCH_SET_VERSION).$(X_PATCH_VERSION)

ifeq ($(wildcard common/.version),)
version_h:
	$(VERBOSE)echo "#define" INMAGE_PRODUCT_VERSION_STR \"$(X_VERSION_QUALITY)_$(PROD_VERSION)_${X_VERSION_PHASE}_$(X_VERSION_BUILD_NUM)_$(X_VERSION_DATE)\" > common/version.h
	$(VERBOSE)echo "#define" INMAGE_PRODUCT_VERSION $(X_VERSION_MAJOR),$(X_VERSION_MINOR),$(X_VERSION_BUILD_NUM),1 >> common/version.h
	$(VERBOSE)echo "#define" INMAGE_PRODUCT_VERSION_MAJOR $(X_VERSION_MAJOR) >> common/version.h
	$(VERBOSE)echo "#define" INMAGE_PRODUCT_VERSION_MINOR $(X_VERSION_MINOR) >> common/version.h
	$(VERBOSE)echo "#define" INMAGE_PRODUCT_VERSION_BUILDNUM $(X_VERSION_BUILD_NUM) >> common/version.h
	$(VERBOSE)echo "#define" INMAGE_PRODUCT_VERSION_PRIVATE 1 >> common/version.h
	$(VERBOSE)echo "#define" INMAGE_HOST_AGENT_CONFIG_CAPTION \"$(X_VERSION_QUALITY)_$(PROD_VERSION)_${X_VERSION_PHASE}_$(X_VERSION_BUILD_NUM)_$(X_VERSION_DATE)\" >> common/version.h
	$(VERBOSE)echo "#define" PROD_VERSION \"$(PROD_VERSION)\" >> common/version.h
	$(VERBOSE)echo "#define" INMAGE_COPY_RIGHT \"\(C\) $(X_YEAR) Microsoft Corp. All rights reserved.\" >> common/version.h
	$(VERBOSE)echo "#define" INMAGE_PRODUCT_NAME \"Microsoft Azure Site Recovery\" >> common/version.h
	touch common/.version
else
version_h:
	echo "Version already generated"
endif
