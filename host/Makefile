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
$(shell ln -sf tests/DITests/DITestUtil tests_DITests_DITestUtil)

# ----------------------------------------------------------------------
# X_ARCH specific modules
# NOTE:
# order is somewhat important in that you need to list all modules that 
# generate libs before any of the modules that need those libs
# if you get the wrong order you will see an error message telling you 
# the order is in correct
# ----------------------------------------------------------------------
MODULES := $(REQUIRED_BY_ALL_MODULES) securitylib curlwrapperlib cxpslib AzureRecoveryLib AzureRecoveryUtil

ifeq (yes,$(FX_VACP))
MODULES := conversionlib vacplibs vacpunix
X_ARCH=fx_vacp
endif

include once.mak