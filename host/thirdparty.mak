# this will configure and build the thirdparty packages as needed
# it will always try to do both debug and release no matter
# what value is in X_CONFIGURATION because some of the thirdparty
# packages don't allow to have both a debug and release built at
# the same time

# ----------------------------------------------------------------------
# thirdpart makefile name
# ----------------------------------------------------------------------
THIRDPARTY_MAK = thirdparty.mak

# ----------------------------------------------------------------------
# thirdparty root dirs
# add new thirdparty root dirs here or update the locations to switch
# to a different version
#
# NOTE: boost has its own section (see below)
# ----------------------------------------------------------------------
ACE_ROOT := ../thirdparty/ace-6.4.6/ACE_wrappers
CDK_ROOT := ../thirdparty/cdk-5.0.4
CURL_ROOT := ../thirdparty/curl-7.83.1
OPENSSL_ROOT := ../thirdparty/openssl-1.1.1n
SQLITE_ROOT := ../thirdparty/sqlite-3.36.0
SQLITE3X_ROOT := ../thirdparty/sqlite3x/sqlite3x
ZLIB_ROOT := ../thirdparty/zlib-1.2.12
LIBSSH2_ROOT := ../thirdparty/libssh2-1.10.0
LIB_ROOT := ../thirdparty/lib
BIN_ROOT := ../thirdparty/bin
LIBXML2_ROOT := ../thirdparty/libxml2/libxml2-2.9.13
INM_MD5_ROOT := ../thirdparty/inm_md5
SIGSLOT_ROOT := ../thirdparty/sigslot
XENDETECT_ROOT := ../thirdparty/xendetect
LIBXENSERVER_ROOT := ../thirdparty/libxenserver/include
ESJSON_ROOT := ../thirdparty/g40-esj-1.05
JQ_ROOT := ../thirdparty/jq-1.6

# ----------------------------------------------------------------------
# thirdparty includes
# add new thirdparty include dirs here or update the locations to switch
# to a different version
#
# NOTE: boost has its own section (see below)
# ----------------------------------------------------------------------
ACE_INCLUDE := -I$(ACE_ROOT)
CDK_INCLUDE := -I$(CDK_ROOT)/config_$(X_CONFIGURATION)/include -I$(CDK_ROOT)/include
CURL_INCLUDE := -I$(CURL_ROOT)/config_$(X_CONFIGURATION)/include/curl -I$(CURL_ROOT)/include
OPENSSL_INCLUDE := -I$(OPENSSL_ROOT)/include
SQLITE_INCLUDE := -I$(SQLITE3X_ROOT) -I$(SQLITE_ROOT)/config_$(X_CONFIGURATION)
ZLIB_INCLUDE := -I$(ZLIB_ROOT)
LIBSSH2_INCLUDE := -I$(LIBSSH2_ROOT)/include
LIBXML2_INCLUDE := -I$(LIBXML2_ROOT)/config_$(X_CONFIGURATION)/build/include/libxml2
INM_MD5_INCLUDE := -I$(INM_MD5_ROOT)
SIGSLOT_INCLUDE := -I$(SIGSLOT_ROOT)
XENDETECT_INCLUDE := -I$(XENDETECT_ROOT)
LIBXENSERVER_INCLUDE := -I$(LIBXENSERVER_ROOT)
ESJSON_INCLUDES := -I$(ESJSON_ROOT)

# ----------------------------------------------------------------------
# thirdparty_LIB_DIRS
# add new thirdparty lib dirs here or update the locations to switch to
# a different version
#
# NOTE: boost has its own section (see below)
# ----------------------------------------------------------------------
ACE_LIBS := $(ACE_ROOT)/lib/$(X_SPECIFIC)/$(X_CONFIGURATION)/libACE.a
CDK_LIBS := $(CDK_ROOT)/lib/$(X_SPECIFIC)/$(X_CONFIGURATION)/libcdk.a
ARES_LIBS := $(CURL_ROOT)/../c-ares-1.18.1/install_$(X_CONFIGURATION)/lib/libcares.a
CURL_LIBS := $(CURL_ROOT)/lib/$(X_SPECIFIC)/$(X_CONFIGURATION)/libcurl.a
OPENSSL_LIBS := $(OPENSSL_ROOT)/lib/$(X_SPECIFIC)/$(X_CONFIGURATION)/libssl.a $(OPENSSL_ROOT)/lib/$(X_SPECIFIC)/$(X_CONFIGURATION)/libcrypto.a
SQLITE_LIBS := $(SQLITE3X_ROOT)/lib/$(X_SPECIFIC)/$(X_CONFIGURATION)/libsqlite3x.a $(SQLITE_ROOT)/lib/$(X_SPECIFIC)/$(X_CONFIGURATION)/libsqlite3.a
ZLIB_LIBS := $(ZLIB_ROOT)/lib/$(X_SPECIFIC)/$(X_CONFIGURATION)/libz.a
LIBSSH2_LIBS := $(LIBSSH2_ROOT)/lib/$(X_SPECIFIC)/$(X_CONFIGURATION)/libssh2.a
LIBXML2_LIBS := $(LIBXML2_ROOT)/config_$(X_CONFIGURATION)/build/lib/libxml2.a
INM_MD5_LIBS := $(INM_MD5_ROOT)/lib/config_$(X_CONFIGURATION)/libinmmd5.a

# ----------------------------------------------------------------------
# thirdparty build scripts
# add new thirdparty lib dirs here or update the locations to switch to
# a different version
#
# NOTE: boost has its own section (see below)
# ----------------------------------------------------------------------
ACE_SCRIPT := $(ACE_ROOT)/inmage_config_build
CDK_SCRIPT := $(CDK_ROOT)/inmage_config_build
CURL_SCRIPT := $(CURL_ROOT)/inmage_config_build
OPENSSL_SCRIPT := $(OPENSSL_ROOT)/inmage_config_build
SQLITE_SCRIPT := $(SQLITE_ROOT)/inmage_config_build
SQLITE3X_SCRIPT := $(SQLITE3X_ROOT)/inmage_config_build
ZLIB_SCRIPT := $(ZLIB_ROOT)/inmage_config_build
LIBSSH2_SCRIPT := $(LIBSSH2_ROOT)/inmage_config_build
LIBXML2_SCRIPT := $(LIBXML2_ROOT)/inmage_config_build
INM_MD5_SCRIPT := $(INM_MD5_ROOT)/inmage_config_build

# ----------------------------------------------------------------------
# thirdparty dependency dirs
# this will find all dirs that have source that a thirdparty might need
# and add it to the dependency, that way if thirdparty source is
# modified by us, that thirdparty package's make will run to rebuild
# what is needed. at the same time this avoids running each thirparties
# build when nothing was changed.
#
# NOTE: boost has its own section (see below)
# ----------------------------------------------------------------------
# note for ace we only care about what is under the ace dir as that is all
# we actually build
ACE_DIR_DEPS := $(shell ./find-dir-deps $(ACE_ROOT)/ace; cat $(ACE_ROOT)/ace/dir_deps)
CDK_DIR_DEPS := $(shell ./find-dir-deps $(CDK_ROOT); cat $(CDK_ROOT)/dir_deps)
CURL_DIR_DEPS := $(shell ./find-dir-deps $(CURL_ROOT); cat $(CURL_ROOT)/dir_deps )
OPENSSL_DIR_DEPS := $(shell ./find-dir-deps $(OPENSSL_ROOT); cat $(OPENSSL_ROOT)/dir_deps)
SQLITE_DIR_DEPS := $(shell ./find-dir-deps $(SQLITE_ROOT) ; cat $(SQLITE_ROOT)/dir_deps)
SQLITE3X_DIR_DEPS := $(shell ./find-dir-deps $(SQLITE3X_ROOT); cat $(SQLITE3X_ROOT)/dir_deps)
ZLIB_DIR_DEPS := $(shell ./find-dir-deps $(ZLIB_ROOT); cat $(ZLIB_ROOT)/dir_deps)
LIBSSH2_DIR_DEPS := $(shell ./find-dir-deps $(LIBSSH2_ROOT); cat $(LIBSSH2_ROOT)/dir_deps)
LIBXML2_DIR_DEPS := $(shell ./find-dir-deps $(LIBXML2_ROOT); cat $(LIBXML2_ROOT)/dir_deps)
INM_MD5_DIR_DEPS := $(shell ./find-dir-deps $(INM_MD5_ROOT); cat $(INM_MD5_ROOT)/dir_deps)


# ----------------------------------------------------------------------
# boost 
# 
# building boost works slightly differently then other thirdparty
# packages. 
# 
# most of the time, you should only need to change the version number 
# when upgrading boost. 
#
# a possible issue when upgrading maybe changes to library naming
# conventions. starting with 1_40_0 boost added a --layout option that 
# allows one to slightly customize output library names.
#
# ----------------------------------------------------------------------
REQUIRED_GCC_VERSION = "4.6"
GCC_VERSION := "`gcc -dumpversion`"
IS_GCC_SUPPORTED := $(shell expr "$(GCC_VERSION)" ">=" "$(REQUIRED_GCC_VERSION)")
ifeq "$(IS_GCC_SUPPORTED)" "1"
BOOST_VERSION := 1_78_0
else
BOOST_VERSION := 1_70_0
endif

BOOST_ROOT := ../thirdparty/boost/boost_$(BOOST_VERSION)
BOOST_LIB_DIR := $(BOOST_ROOT)/stage/lib
BOOST_INCLUDE := -I$(BOOST_ROOT)
BOOST_BUILD_DIR := boost_$(BOOST_VERSION)

# unix/linux uses --layout=tagged option when building boost that results
# in names like the following
#   release: libboost_serialization-mt.a
#   debug  : libboost_serialization-mt-d.a
# the -d in the name is part of what boost calls the ABI. 
# we only need to worry if the -d should be pressent or not as we do not
# use any other parts of the ABI (see boost docs for details)
ifeq ($(X_CONFIGURATION),debug)
BOOST_ABI := -d
BOOST_CONFIGURATION :=
else
BOOST_ABI := 
BOOST_CONFIGURATION := -r
endif

# if new libraries are added to the boost\inmage_config_build script
# they will need to be added here too
BOOST_LIBS := $(BOOST_LIB_DIR)/libboost_date_time-mt-x64$(BOOST_ABI).a \
	$(BOOST_LIB_DIR)/libboost_filesystem-mt-x64$(BOOST_ABI).a \
	$(BOOST_LIB_DIR)/libboost_program_options-mt-x64$(BOOST_ABI).a \
	$(BOOST_LIB_DIR)/libboost_regex-mt-x64$(BOOST_ABI).a \
	$(BOOST_LIB_DIR)/libboost_serialization-mt-x64$(BOOST_ABI).a \
	$(BOOST_LIB_DIR)/libboost_system-mt-x64$(BOOST_ABI).a \
	$(BOOST_LIB_DIR)/libboost_thread-mt-x64$(BOOST_ABI).a \
	$(BOOST_LIB_DIR)/libboost_random-mt-x64$(BOOST_ABI).a \
	$(BOOST_LIB_DIR)/libboost_chrono-mt-x64$(BOOST_ABI).a

BOOST_SCRIPT := ../thirdparty/boost/inmage_config_build
BOOST_DIR_DEPS := $(shell ./find-dir-deps $(BOOST_ROOT)/boost; cat $(BOOST_ROOT)/boost/dir_deps)

# ----------------------------------------------------------------------
# make sure all thirdparty packages are built
# ----------------------------------------------------------------------
thirdparty_build: $(THIRDPARTY_MAK) $(ZLIB_ROOT)/build_zlib $(OPENSSL_ROOT)/build_openssl $(BOOST_ROOT)/build_boost_$(X_CONFIGURATION) $(CDK_ROOT)/build_cdk $(CURL_ROOT)/build_curl $(ACE_ROOT)/build_ace $(LIBXML2_ROOT)/build_libxml2 $(INM_MD5_ROOT)/build_inm_md5 thirdparty_links
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

# ----------------------------------------------------------------------
# individual thirdparty package builds
# ----------------------------------------------------------------------
$(ACE_ROOT)/config_ace: $(ACE_SCRIPT)
	$(VERBOSE)$(ACE_SCRIPT) --clean
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

$(ACE_ROOT)/build_ace: $(ACE_ROOT)/config_ace $(ACE_ROOT) $(ACE_DIR_DEPS) $(THIRDPARTY_MAK)
	$(VERBOSE)$(ACE_SCRIPT)
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

$(BOOST_ROOT)/config_boost: $(BOOST_SCRIPT)
	$(VERBOSE)$(BOOST_SCRIPT) -d $(BOOST_BUILD_DIR) --clean
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

# boost supports building just debug or release 
$(BOOST_ROOT)/build_boost_$(X_CONFIGURATION): $(BOOST_ROOT)/config_boost $(BOOST_ROOT) $(BOOST_DIR_DEPS) $(THIRDPARTY_MAK)
	$(VERBOSE)$(BOOST_SCRIPT) -d $(BOOST_BUILD_DIR) $(BOOST_CONFIGURATION)
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

$(CDK_ROOT)/config_cdk: $(CDK_SCRIPT)
	$(VERBOSE)$(CDK_SCRIPT) --clean
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

$(CDK_ROOT)/build_cdk: $(CDK_ROOT)/config_cdk $(CDK_ROOT) $(THIRDPARTY_MAK)
	$(VERBOSE)$(CDK_SCRIPT)
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

$(CURL_ROOT)/config_curl: $(CURL_SCRIPT)
	$(VERBOSE)$(CURL_SCRIPT) --clean
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

$(CURL_ROOT)/build_curl: $(CURL_ROOT)/config_curl $(CURL_ROOT) $(CURL_DIR_DEPS) $(THIRDPARTY_MAK)
	$(VERBOSE)$(CURL_SCRIPT)
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

#$(LIBSSH2_ROOT)/config_libssh2: $(LIBSSH2_SCRIPT)
#	$(VERBOSE)$(LIBSSH2_SCRIPT) --clean
#	$(VERBOSE)touch $@
#	$(RULE_SEPARATOR)

#$(LIBSSH2_ROOT)/build_libssh2: $(LIBSSH2_ROOT)/config_libssh2 $(LIBSSH2_ROOT) $(LIBSSH2_DIR_DEPS) $(THIRDPARTY_MAK)
#	$(VERBOSE)$(LIBSSH2_SCRIPT)
#	$(VERBOSE)touch $@
#	$(RULE_SEPARATOR)

$(LIBXML2_ROOT)/config_libxml2: $(LIBXML2_SCRIPT)
	$(VERBOSE)$(LIBXML2_SCRIPT) --clean
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

$(LIBXML2_ROOT)/build_libxml2: $(LIBXML2_ROOT)/config_libxml2 $(LIBXML2_ROOT) $(LIBXML2_DIR_DEPS) $(THIRDPARTY_MAK)
	$(VERBOSE)$(LIBXML2_SCRIPT)
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

$(OPENSSL_ROOT)/config_openssl: $(OPENSSL_SCRIPT)
	$(VERBOSE)$(OPENSSL_SCRIPT) --clean
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

$(OPENSSL_ROOT)/build_openssl: $(OPENSSL_ROOT)/config_openssl $(OPENSSL_ROOT) $(OPENSSL_DIR_DEPS) $(THIRDPARTY_MAK)
	$(VERBOSE)$(OPENSSL_SCRIPT)
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

#$(SQLITE_ROOT)/config_sqlite: $(SQLITE_SCRIPT)
#	$(VERBOSE) chmod +x $(SQLITE_SCRIPT)
#	$(VERBOSE)$(SQLITE_SCRIPT)  --clean
#	$(VERBOSE)touch $@
#	$(RULE_SEPARATOR)

#$(SQLITE_ROOT)/build_sqlite: $(SQLITE_ROOT)/config_sqlite $(SQLITE_ROOT) $(SQLITE_DIR_DEPS) #$(THIRDPARTY_MAK)
#	$(VERBOSE) chmod +x $(SQLITE_SCRIPT)
#	$(VERBOSE)$(SQLITE_SCRIPT)
#	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

#$(SQLITE3X_ROOT)/config_sqlite3x: $(SQLITE3X_SCRIPT)
#	$(VERBOSE)$(SQLITE3X_SCRIPT)  --clean
#	$(VERBOSE)touch $@
#	$(RULE_SEPARATOR)

#$(SQLITE3X_ROOT)/build_sqlite3x: $(SQLITE3X_ROOT)/config_sqlite3x $(SQLITE3X_ROOT) $(SQLITE3X_DIR_DEPS) #$(THIRDPARTY_MAK)
#	$(VERBOSE)$(SQLITE3X_SCRIPT)
#	$(VERBOSE)touch $@
#	$(RULE_SEPARATOR)

$(ZLIB_ROOT)/config_zlib: $(ZLIB_SCRIPT)
	$(VERBOSE)$(ZLIB_SCRIPT) --clean
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

$(ZLIB_ROOT)/build_zlib: $(ZLIB_ROOT)/config_zlib $(ZLIB_ROOT) $(ZLIB_DIR_DEPS) $(THIRDPARTY_MAK)
	$(VERBOSE)$(ZLIB_SCRIPT)
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

$(INM_MD5_ROOT)/config_inm_md5: $(INM_MD5_SCRIPT)
	$(VERBOSE) chmod +x $(INM_MD5_SCRIPT)
	$(VERBOSE)$(INM_MD5_SCRIPT)  --clean
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

$(INM_MD5_ROOT)/build_inm_md5: $(INM_MD5_ROOT)/config_inm_md5 $(INM_MD5_ROOT) $(INM_MD5_DIR_DEPS) $(THIRDPARTY_MAK)
	$(VERBOSE) chmod +x $(INM_MD5_SCRIPT)
	$(VERBOSE)$(INM_MD5_SCRIPT)
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

thirdparty_links: thirdparty_links.sh
	$(VERBOSE)./$<
	$(VERBOSE)touch $@
	$(RULE_SEPARATOR)

# ----------------------------------------------------------------------
# clean all thirdparty packages
# ----------------------------------------------------------------------
clean_thirdparty: clean_ace clean_boost clean_cdk clean_curl clean_openssl clean_zlib clean_libxml2 clean_inm_md5 clean_thirdparty_links
	$(VERBOSE)rm -f $@
	$(RULE_SEPARATOR)

# ----------------------------------------------------------------------
# clean individual thridparty packages
# ----------------------------------------------------------------------
.PHONY: clean_ace
clean_ace:
	$(VERBOSE)$(ACE_SCRIPT) --clean
	$(VERBOSE)rm -f $(ACE_ROOT)/ace/dep_dirs
	$(VERBOSE)rm -f $(ACE_ROOT)/build_ace
	$(VERBOSE)rm -f $(ACE_ROOT)/config_ace
	$(RULE_SEPARATOR)

.PHONY: clean_boost
clean_boost:
	$(VERBOSE)$(BOOST_SCRIPT) -d $(BOOST_BUILD_DIR) --clean
	$(VERBOSE)rm -f $(BOOST_ROOT)/boost/dep_dirs
	$(VERBOSE)rm -f $(BOOST_ROOT)/build_boost
	$(VERBOSE)rm -f $(BOOST_ROOT)/config_boost
	$(RULE_SEPARATOR)

.PHONY: clean_cdk
clean_cdk:
	$(VERBOSE)$(CDK_SCRIPT) --clean
	$(VERBOSE)rm -f $(CDK_ROOT)/dep_dirs
	$(VERBOSE)rm -f $(CDK_ROOT)/build_cdk
	$(VERBOSE)rm -f $(CDK_ROOT)/config_cdk
	$(RULE_SEPARATOR)

.PHONY: clean_curl
clean_curl:
	$(VERBOSE)$(CURL_SCRIPT) --clean
	$(VERBOSE)rm -f $(CURL_ROOT)/dep_dirs
	$(VERBOSE)rm -f $(CURL_ROOT)/build_curl
	$(VERBOSE)rm -f $(CURL_ROOT)/config_curl
	$(VERBOSE)rm -f $(CURL_ROOT)/../c-ares-1.18.1/debug/ran_config
	$(VERBOSE)rm -f $(CURL_ROOT)/../c-ares-1.18.1/release/ran_config
	$(RULE_SEPARATOR)

#.PHONY: clean_libssh2
#clean_libssh2:
#	$(VERBOSE)$(LIBSSH2_SCRIPT) --clean
#	$(VERBOSE)rm -f $(LIBSSH2_ROOT)/dep_dirs
#	$(VERBOSE)rm -f $(LIBSSH2_ROOT)/build_libssh2
#	$(VERBOSE)rm -f $(LIBSSH2_ROOT)/config_libssh2
#	$(RULE_SEPARATOR)

.PHONY: clean_libxml2
clean_libxml2:
	$(VERBOSE)$(LIBXML2_SCRIPT) --clean
	$(VERBOSE)rm -f $(LIBXML2_ROOT)/dep_dirs
	$(VERBOSE)rm -f $(LIBXML2_ROOT)/build_libxml2
	$(VERBOSE)rm -f $(LIBXML2_ROOT)/config_libxml2
	$(RULE_SEPARATOR)

.PHONY: clean_openssl
clean_openssl:
	$(VERBOSE)$(OPENSSL_SCRIPT) --clean
	$(VERBOSE)rm -f $(OPENSSL_ROOT)/dep_dirs
	$(VERBOSE)rm -f $(OPENSSL_ROOT)/build_openssl
	$(VERBOSE)rm -f $(OPENSSL_ROOT)/config_openssl
	$(RULE_SEPARATOR)

#.PHONY: clean_sqlite
#clean_sqlite:
#	$(VERBOSE)$(SQLITE_SCRIPT) --clean
#	$(VERBOSE)rm -f $(SQLITE_ROOT)/dep_dirs
#	$(VERBOSE)rm -f $(SQLITE_ROOT)/build_sqlite
#	$(VERBOSE)rm -f $(SQLITE_ROOT)/config_sqlite
#	$(RULE_SEPARATOR)

#.PHONY: clean_sqlite3x
#clean_sqlite3x:
#	$(VERBOSE)$(SQLITE3X_SCRIPT) --clean
#	$(VERBOSE)rm -f $(SQLITE3X_ROOT)/dep_dirs
#	$(VERBOSE)rm -f $(SQLITE3X_ROOT)/build_sqlite3x
#	$(VERBOSE)rm -f $(SQLITE3X_ROOT)/config_sqlite3x
#	$(RULE_SEPARATOR)

.PHONY: clean_zlib
clean_zlib:
	$(VERBOSE)$(ZLIB_SCRIPT) --clean
	$(VERBOSE)rm -f $(ZLIB_ROOT)/dep_dirs
	$(VERBOSE)rm -f $(ZLIB_ROOT)/build_zlib
	$(VERBOSE)rm -f $(ZLIB_ROOT)/config_zlib
	$(RULE_SEPARATOR)

.PHONY: clean_inm_md5
clean_inm_md5:
	$(VERBOSE)$(INM_MD5_SCRIPT) --clean
	$(VERBOSE)rm -f $(INM_MD5_ROOT)/dep_dirs
	$(VERBOSE)rm -f $(INM_MD5_ROOT)/build_inm_md5
	$(VERBOSE)rm -f $(INM_MD5_ROOT)/config_inm_md5
	$(RULE_SEPARATOR)

.PHONY: clean_thirdparty_links
clean_thirdparty_links:
	$(VERBOSE)rm -rf thirdparty_links
	$(RULE_SEPARATOR)
