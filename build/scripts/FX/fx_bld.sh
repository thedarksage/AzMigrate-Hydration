#!/bin/sh -x

##
# Step 1: USAGE
# This defines the usage and also captures different components to be built
# This also sets various compiler flags required for building components
##

# Define the usage
USAGE="Usage:   ./fx_bld.sh [top build dir] [inmsync,build,unregagent,alert,dspace,remotecli,all]. Parameters must be comma-separated. Default parameter values are current directory and 'all'."

# Capture the components to be built
TOP_BLD_DIR=${1:-`pwd`}
COMPONENTS=${2:-all}
CONFIG=${3:-debug}

CUR_W_D=`pwd`
COMPILE="g++"

if [ "$COMPONENTS" = "inmsync" ]; then
	BUILD_INMSYNC=1
else
	BUILD_INMSYNC=0
fi

if [ "$COMPONENTS" = "svfrd" ]; then
	BUILD_SVFRD=1
else
	BUILD_SVFRD=0
fi

if [ "$COMPONENTS" = "unregagent" ]; then
	BUILD_UNREG=1
else
	BUILD_UNREG=0
fi

if [ "$COMPONENTS" = "alert" ]; then
        BUILD_ALERT=1
else
        BUILD_ALERT=0
fi

if [ "$COMPONENTS" = "dspace" ]; then
        BUILD_DSPACE=1
else
        BUILD_DSPACE=0
fi

if [ "$COMPONENTS" = "remotecli" ]; then
        BUILD_REMOTECLI=1
else
        BUILD_REMOTECLI=0
fi

if [ "$COMPONENTS" = "vacp" ]; then
	BUILD_VACP=1
else
	BUILD_VACP=0
fi

[ $COMPONENTS = "all" ] && BUILD_INMSYNC=1 && BUILD_SVFRD=1 && BUILD_UNREG=1 && BUILD_ALERT=1 && BUILD_DSPACE=1 && BUILD_VACP=1 && BUILD_REMOTECLI=1

if [ $BUILD_INMSYNC -ne 1 -a $BUILD_SVFRD -ne 1 -a $BUILD_UNREG -ne 1 -a $BUILD_ALERT -ne 1 -a $BUILD_DSPACE -ne 1 -a $BUILD_VACP -ne 1 -a $BUILD_REMOTECLI -ne 1 ]
then
	echo $USAGE
	exit 95
fi

# Form OP_SYS variable based on platform
case `uname` in
	AIX)	OP_SYS="aix";;
	SunOS)	if uname -r | grep 5.8 > /dev/null 2>&1; then
                        OP_SYS="solaris58"
		elif uname -r | grep 5.9 > /dev/null 2>&1; then
			OP_SYS="solaris5-9"
                elif uname -r | grep 5.10 > /dev/null 2>&1; then
                        OP_SYS="solaris5-10"
                elif uname -r | grep 5.11 > /dev/null 2>&1; then
                        OP_SYS="solaris5-11"
                fi;;
	HP-UX)	if ! uname -a | grep -i ia64 > /dev/null 2>&1; then OP_SYS="hpux"; else OP_SYS="hpux_itanium"; fi;;
esac

. ${TOP_BLD_DIR}/source/build/scripts/general/OS_details.sh

# Set platform-based compiler flags

if [ "$OP_SYS" = "aix" ]; then			

    	case $OS in
		AIX53)
			CURL_INCLUDE="-Ithirdparty/aix/lib53/curl-7.59.0/include"
			;;              
	esac

	unset LD_LIBRARY_PATH
        LD_FLAGS='-lstdc++ -lgcc_s -lcrypt -lpthread'
        LIBS='lib/libcurl.a  lib/libz.a  lib/libssl.a  lib/libcrypto.a  lib/libcares.a'
	GCC_OPTS_SVFRD="-Bstatic -Llib -Wno-deprecated ${CURL_INCLUDE} -I/usr/local/include -Ihost/fr_common \
		-Ihost/fr_common/unix -DSV_UNIX \
		-DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION -DHAVE_STRING_H -DHAVE_SETSID \
		-DHAVE_STRCASECMP -DHAVE_NETDB_H -DHAVE_NETINET_H -DSV_GENUUID \
		host/fr_log/logger.cpp \
		host/fr_config/svconfig.cpp \
		host/fr_common/svtransport.cpp \
		host/fr_common/unix/portableunix.cpp \
		host/fr_common/unixagenthelpers.cpp \
		host/fr_common/svtypes.cpp \
		host/fr_common/svutils.cpp \
		host/fragent/fragent.cpp \
		host/frdaemon/daemon.cpp -o svfrd -Llib $LIBS $LD_FLAGS"

	GCC_OPTS_UNREGAGENT="-Bstatic -Llib -Wno-deprecated -Wno-sign-compare -Wno-conversion ${CURL_INCLUDE} -I/usr/local/include \
		-Ihost/fr_common -Ihost/fr_common/unix -DSV_UNIX -DSTDC_HEADERS -DHAVE_UNISTD_H \
		-DHAVE_SIGACTION -DHAVE_STRING_H -DHAVE_SETSID -DHAVE_STRCASECMP \
		-DHAVE_NETDB_H -DHAVE_NETINET_H -DSV_GENUUID \
		host/fr_log/logger.cpp \
		host/fr_config/svconfig.cpp \
		host/fr_common/svtransport.cpp \
		host/fr_common/unix/portableunix.cpp \
		host/fr_common/unixagenthelpers.cpp \
		host/fr_common/svtypes.cpp \
		host/fr_common/svutils.cpp \
		host/fr_common/unregworker.cpp \
		host/fr_unregagent/unixunreg.cpp -o unregfragent -Llib $LIBS $LD_FLAGS"

	GCC_OPTS_ALERT="-Bstatic -Llib -Wno-deprecated ${CURL_INCLUDE} -I/usr/include \
		-I/usr/local/include -Ihost/fr_common -Ihost/fr_common/unix \
        	-Ihost/fr_config -Ihost/config -DSV_UNIX -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION -DHAVE_SETSID \
        	-DHAVE_STRCASECMP -DHAVE_NETDB_H -DHAVE_NETINET_H -DHAVE_STRING_H -DSV_GENUUID \
		host/fr_log/logger.cpp \
		host/fr_config/svconfig.cpp \
        	host/fr_common/svtransport.cpp \
		host/fr_common/unix/portableunix.cpp \
		host/fr_common/unixagenthelpers.cpp \
		host/fr_common/svtypes.cpp \
		host/fr_common/svutils.cpp \
		host/fragent/fragent.cpp \
        	host/fxalert/fxalert.cpp -o alert -Llib $LIBS $LD_FLAGS"

	GCC_OPTS_DSPACE="-Bstatic -Llib -Wno-deprecated ${CURL_INCLUDE} -I/usr/include \
		-I/usr/local/include  -Ihost/fr_common -Ihost/fr_common/unix \
        	-Ihost/fr_config -Ihost/config -DSV_UNIX -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION -DHAVE_SETSID \
        	-DHAVE_STRCASECMP -DHAVE_NETDB_H -DHAVE_NETINET_H -DHAVE_STRING_H -DSV_GENUUID \
		host/fr_log/logger.cpp \
		host/fr_config/svconfig.cpp \
        	host/fr_common/svtransport.cpp \
		host/fr_common/unix/portableunix.cpp \
		host/fr_common/unixagenthelpers.cpp \
		host/fr_common/svtypes.cpp \
		host/fr_common/svutils.cpp \
		host/fragent/fragent.cpp \
        	host/fxdspace/fxdspace.cpp -o dspace -Llib $LIBS $LD_FLAGS"

	GCC_OPTS_REMOTECLI="-Bstatic -Wno-deprecated ${CURL_INCLUDE} -I/usr/local/include \
		-Ihost/remotecli -Ihost/common -Ihost/cdplibs \
		-DSV_UNIX -DSV_GENUUID -DSV_SUNOS -DSTDC_HEADERS \
		-DHAVE_UNISTD_H -DHAVE_SIGACTION -DHAVE_SETSID -DHAVE_STRCASECMP \
		-DHAVE_STRING_H -DHAVE_NETINET_H -DHAVE_NETDB_H \
		host/remotecli/main.cpp \
		host/remotecli/remotecli.cpp \
		host/common/svutilities.cpp  -o remotecli -Llib $LIBS $LD_FLAGS"

elif [ "$OP_SYS" = "solaris58" -o "$OP_SYS" = "solaris5-9" -o "$OP_SYS" = "solaris5-10" -o "$OP_SYS" = "solaris5-11" ]; then
	unset LD_LIBRARY_PATH
	LD_FLAGS='-Llib -lc -lnsl -lsocket -lldap -ldl -lrt -lstdc++ -lgcc_s'
	LIBS='lib/libcurl.a  lib/libz.a  lib/libssl.a  lib/libcrypto.a  lib/libcares.a'

	if [ "$CONFIG" = "release" ]; then
		SYMBOL_FLAGS='-O3 -g3 -ggdb' 
	else
		SYMBOL_FLAGS='-O0 -gdwarf-2 -g3 -fno-optimize-sibling-calls -fno-omit-frame-pointer'
	fi

	if [ "$OP_SYS" = "solaris58" ]; then
           GCC_OPTS_SVFRD="$SYMBOL_FLAGS -m32 -Bstatic -Rlib -Wno-deprecated -I/usr/local/include -Ihost/fr_common -Ihost/fr_common/unix \
                -Ithirdparty/curl-7.59.0/include \
                -DSV_UNIX -DSV_GENUUID -DSV_SUN_5DOT8 -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION \
                -DHAVE_SETSID -DHAVE_STRCASECMP -DHAVE_STRING_H -DHAVE_NETINET_H -DHAVE_NETDB_H \
                host/fr_log/logger.cpp \
                host/fr_config/svconfig.cpp \
                host/fr_common/svtransport.cpp \
                host/fr_common/unix/portableunix.cpp \
                host/fr_common/unixagenthelpers.cpp \
                host/fr_common/svtypes.cpp \
                host/fr_common/svutils.cpp \
                host/fragent/fragent.cpp \
                host/frdaemon/daemon.cpp -o svfrd $LIBS $LD_FLAGS"
        elif [ "$OP_SYS" = "solaris5-9" ]; then
            GCC_OPTS_SVFRD="$SYMBOL_FLAGS -m32 -Bstatic -Rlib -Wno-deprecated -I/usr/local/include -Ihost/fr_common -Ihost/fr_common/unix \
                -Ithirdparty/curl-7.59.0/include \
                -DSV_UNIX -DSV_GENUUID -DSV_SUN_5DOT9 -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION \
                -DHAVE_SETSID -DHAVE_STRCASECMP -DHAVE_STRING_H -DHAVE_NETINET_H -DHAVE_NETDB_H \
                host/fr_log/logger.cpp \
                host/fr_config/svconfig.cpp \
                host/fr_common/svtransport.cpp \
                host/fr_common/unix/portableunix.cpp \
                host/fr_common/unixagenthelpers.cpp \
                host/fr_common/svtypes.cpp \
                host/fr_common/svutils.cpp \
                host/fragent/fragent.cpp \
                host/frdaemon/daemon.cpp -o svfrd $LIBS $LD_FLAGS"
        else
	    GCC_OPTS_SVFRD="$SYMBOL_FLAGS -m32 -Bstatic -Rlib -Wno-deprecated -I/usr/local/include -Ihost/fr_common -Ihost/fr_common/unix \
		-Ithirdparty/curl-7.59.0/include \
		-DSV_UNIX -DSV_GENUUID -DSV_SUNOS -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION \
		-DHAVE_SETSID -DHAVE_STRCASECMP -DHAVE_STRING_H -DHAVE_NETINET_H -DHAVE_NETDB_H \
		host/fr_log/logger.cpp \
		host/fr_config/svconfig.cpp \
		host/fr_common/svtransport.cpp \
		host/fr_common/unix/portableunix.cpp \
		host/fr_common/unixagenthelpers.cpp \
		host/fr_common/svtypes.cpp \
		host/fr_common/svutils.cpp \
		host/fragent/fragent.cpp \
		host/frdaemon/daemon.cpp -o svfrd $LIBS $LD_FLAGS"
	fi

	GCC_OPTS_UNREGAGENT="$SYMBOL_FLAGS -m32 -Bstatic -Rlib -Wno-deprecated -I/usr/local/include -Ihost/fr_common -Ihost/fr_common/unix \
		-Ithirdparty/curl-7.59.0/include \
		-DSV_UNIX -DSV_GENUUID -DSV_SUNOS -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION \
		-DHAVE_SETSID -DHAVE_STRCASECMP -DHAVE_STRING_H -DHAVE_NETINET_H -DHAVE_NETDB_H \
		host/fr_log/logger.cpp \
		host/fr_config/svconfig.cpp \
		host/fr_common/svtransport.cpp \
		host/fr_common/unregworker.cpp \
		host/fr_common/unixagenthelpers.cpp \
		host/fr_common/svtypes.cpp \
		host/fr_common/svutils.cpp \
		host/fr_common/unix/portableunix.cpp \
		host/fr_unregagent/unixunreg.cpp -o unregfragent $LIBS $LD_FLAGS"

	GCC_OPTS_ALERT="$SYMBOL_FLAGS -m32 -Bstatic -Rlib -Wno-deprecated -I/usr/include -I/usr/local/include -Ihost/fr_common -Ihost/fr_common/unix \
        	-Ihost/fr_config -Ihost/config -Ithirdparty/curl-7.59.0/include \
		-DSV_UNIX -DSV_SUNOS -DSV_GENUUID -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION -DHAVE_SETSID \
        	-DHAVE_STRCASECMP -DHAVE_STRING_H -DHAVE_NETDB_H -DHAVE_NETINET_H \
		host/fr_log/logger.cpp \
		host/fr_config/svconfig.cpp \
        	host/fr_common/svtransport.cpp \
		host/fr_common/unix/portableunix.cpp \
		host/fr_common/unixagenthelpers.cpp \
		host/fr_common/svtypes.cpp \
		host/fr_common/svutils.cpp \
		host/fragent/fragent.cpp \
        	host/fxalert/fxalert.cpp -o alert $LIBS $LD_FLAGS"

        GCC_OPTS_DSPACE="$SYMBOL_FLAGS -m32 -Bstatic -Rlib -Wno-deprecated -I/usr/include -I/usr/local/include -Ihost/fr_common -Ihost/fr_common/unix \
        	-Ihost/fr_config -Ihost/config -Ithirdparty/curl-7.59.0/include \
		-DSV_UNIX -DSV_SUNOS -DSV_GENUUID -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION -DHAVE_SETSID \
        	-DHAVE_STRCASECMP -DHAVE_STRING_H -DHAVE_NETDB_H -DHAVE_NETINET_H \
		host/fr_log/logger.cpp \
		host/fr_config/svconfig.cpp \
        	host/fr_common/svtransport.cpp \
		host/fr_common/unix/portableunix.cpp \
		host/fr_common/unixagenthelpers.cpp \
		host/fr_common/svtypes.cpp \
		host/fr_common/svutils.cpp \
		host/fragent/fragent.cpp \
	        host/fxdspace/fxdspace.cpp -o dspace $LIBS $LD_FLAGS"
	
	GCC_OPTS_REMOTECLI="$SYMBOL_FLAGS -m32 -Bstatic -Wno-deprecated -I/usr/local/include -Ihost/remotecli -Ihost/common -Ihost/cdplibs \
		-Ithirdparty/curl-7.59.0/include \
		-DSV_UNIX -DSV_GENUUID -DSV_SUNOS -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION -DHAVE_SETSID -DHAVE_STRCASECMP -DHAVE_STRING_H \
		-DHAVE_NETINET_H -DHAVE_NETDB_H host/remotecli/main.cpp host/remotecli/remotecli.cpp host/common/svutilities.cpp  -o remotecli \
		-Llib $LIBS $LD_FLAGS"

elif [ "$OP_SYS" = "hpux" ]; then
	unset LD_LIBRARY_PATH
	LD_FLAGS='-lstdc++ -lgcc_s -lpthread'
        LIBS='lib/libcurl.a lib/libz.a lib/libssl.a lib/libcrypto.a lib/libcares.a lib/libidn.a'
	GCC_OPTS_SVFRD="-Bstatic -Wl,+blib -Wno-deprecated -fno-exceptions -Ithirdparty/curl-7.59.0/config_release/include -I/usr/local/include -Ihost/fr_common \
			-Ihost/fr_common/unix -DSV_UNIX -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION \
			-DHAVE_SETSID -DHAVE_STRCASECMP -DHAVE_NETDB_H -DHAVE_NETINET_H \
			host/fr_log/logger.cpp \
			host/fr_config/svconfig.cpp \
			host/fr_common/svtransport.cpp \
			host/fr_common/unix/portableunix.cpp \
			host/fr_common/unixagenthelpers.cpp \
			host/fr_common/svtypes.cpp \
			host/fr_common/svutils.cpp \
			host/fragent/fragent.cpp \
			host/frdaemon/daemon.cpp -o svfrd -Llib $LIBS $LD_FLAGS"

		GCC_OPTS_UNREGAGENT="-Bstatic -Wl,+blib -Wno-deprecated -Ithirdparty/curl-7.59.0/config_release/include -I/usr/local/include \
			-Ihost/fr_common -Ihost/fr_common/unix \
			-DSV_UNIX -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION -DHAVE_SETSID \
			-DHAVE_STRCASECMP -DHAVE_NETDB_H -DHAVE_NETINET_H \
			host/fr_log/logger.cpp \
			host/fr_config/svconfig.cpp \
			host/fr_common/svtransport.cpp \
			host/fr_common/unregworker.cpp \
			host/fr_common/unixagenthelpers.cpp \
			host/fr_common/svtypes.cpp \
			host/fr_common/svutils.cpp \
			host/fr_common/unix/portableunix.cpp \
			host/fr_unregagent/unixunreg.cpp -o unregfragent -Llib $LIBS $LD_FLAGS"
		
		GCC_OPTS_ALERT="-Bstatic -Wl,+blib -Wno-deprecated -Ithirdparty/curl-7.59.0/config_release/include -I/usr/include -I/usr/local/include \
			-Ihost/fr_common -Ihost/fr_common/unix \
        		-Ihost/fr_config -Ihost/config -DSV_UNIX -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION -DHAVE_SETSID \
        		-DHAVE_STRCASECMP -DHAVE_NETDB_H -DHAVE_NETINET_H \
			host/fr_log/logger.cpp \
			host/fr_config/svconfig.cpp \
        		host/fr_common/svtransport.cpp \
			host/fr_common/unix/portableunix.cpp \
			host/fr_common/unixagenthelpers.cpp \
			host/fr_common/svtypes.cpp \
			host/fr_common/svutils.cpp \
			host/fragent/fragent.cpp \
        		host/fxalert/fxalert.cpp -o alert -Llib $LIBS $LD_FLAGS"

		 GCC_OPTS_DSPACE="-Bstatic -Wl,+blib -Wno-deprecated -Ithirdparty/curl-7.59.0/config_release/include
			-I/usr/include -I/usr/local/include  \
			-Ihost/fr_common -Ihost/fr_common/unix \
        		-Ihost/fr_config \
			-Ihost/config -DSV_UNIX -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION -DHAVE_SETSID \
        		-DHAVE_STRCASECMP -DHAVE_NETDB_H -DHAVE_NETINET_H \
			host/fr_log/logger.cpp \
			host/fr_config/svconfig.cpp \
        		host/fr_common/svtransport.cpp \
			host/fr_common/unix/portableunix.cpp \
			host/fr_common/unixagenthelpers.cpp \
			host/fr_common/svtypes.cpp \
			host/fr_common/svutils.cpp \
			host/fragent/fragent.cpp \
		        host/fxdspace/fxdspace.cpp -o dspace -Llib $LIBS $LD_FLAGS"

		GCC_OPTS_REMOTECLI="-Bstatic -Wl,+blib -Wno-deprecated -D _REENTRANT -Ithirdparty/curl-7.59.0/config_release/include \
			-I/usr/local/include -Ihost/remotecli \
			-Ihost/common -Ihost/cdplibs \
			-DSV_UNIX -DSV_GENUUID -DSV_SUNOS -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION -DHAVE_SETSID -DHAVE_STRCASECMP \
			-DHAVE_STRING_H -DHAVE_NETINET_H -DHAVE_NETDB_H \
			host/remotecli/main.cpp host/remotecli/remotecli.cpp host/common/svutilities.cpp -o remotecli \
			-Llib $LIBS $LD_FLAGS"
		

elif [ "$OP_SYS" = "hpux_itanium" ]; then
		unset LD_LIBRARY_PATH
                LD_FLAGS='-lstdc++ -lgcc_s -lssl -lpthread'
                LIBS='lib/libcurl.a  lib/libz.a  lib/libssl.a  lib/libcrypto.a  lib/libcares.a lib/libidn.a'
		GCC_OPTS_SVFRD="-Bstatic -Wl,+blib -Wno-deprecated -fno-exceptions -Ithirdparty/curl-7.59.0/config_release/include \
			-I/usr/local/include -Ihost/fr_common \
			-Ihost/fr_common/unix -DSV_UNIX -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION \
			-DHAVE_SETSID -DHAVE_STRCASECMP -DHAVE_NETDB_H -DHAVE_NETINET_H \
			host/fr_log/logger.cpp \
			host/fr_config/svconfig.cpp \
			host/fr_common/svtransport.cpp \
			host/fr_common/unix/portableunix.cpp \
			host/fr_common/unixagenthelpers.cpp \
			host/fr_common/svtypes.cpp \
			host/fr_common/svutils.cpp \
			host/fragent/fragent.cpp \
			host/frdaemon/daemon.cpp -o svfrd \
			-Llib $LIBS $LD_FLAGS"

		GCC_OPTS_UNREGAGENT="-Bstatic -Wl,+blib -Wno-deprecated -Ithirdparty/curl-7.59.0/config_release/include -I/usr/local/include -Ihost/fr_common \
			-Ihost/fr_common/unix -Llib \
			-DSV_UNIX -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION -DHAVE_SETSID \
			-DHAVE_STRCASECMP -DHAVE_NETDB_H -DHAVE_NETINET_H \
			host/fr_log/logger.cpp \
			host/fr_config/svconfig.cpp \
			host/fr_common/svtransport.cpp \
			host/fr_common/unregworker.cpp \
			host/fr_common/unixagenthelpers.cpp \
			host/fr_common/svtypes.cpp \
			host/fr_common/svutils.cpp \
			host/fr_common/unix/portableunix.cpp \
			host/fr_unregagent/unixunreg.cpp -o unregfragent \
			-Llib $LIBS $LD_FLAGS"

		GCC_OPTS_ALERT="-Bstatic -Wl,+blib -Wno-deprecated -Ithirdparty/curl-7.59.0/config_release/include -I/usr/include \
			-I/usr/local/include -Ihost/fr_common -Ihost/fr_common/unix \
        		-Ihost/fr_config -Ihost/config -DSV_UNIX -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION -DHAVE_SETSID \
		        -DHAVE_STRCASECMP -DHAVE_NETDB_H -DHAVE_NETINET_H \
			host/fr_log/logger.cpp \
			host/fr_config/svconfig.cpp \
        		host/fr_common/svtransport.cpp \
			host/fr_common/unix/portableunix.cpp \
			host/fr_common/unixagenthelpers.cpp \
			host/fr_common/svtypes.cpp \
			host/fr_common/svutils.cpp \
			host/fragent/fragent.cpp \
		        host/fxalert/fxalert.cpp -o alert -Llib $LIBS $LD_FLAGS"

                 GCC_OPTS_DSPACE="-Bstatic -Wl,+blib -Wno-deprecated -Ithirdparty/curl-7.59.0/config_release/include -I/usr/include -I/usr/local/include \
			-Ihost/fr_common -Ihost/fr_common/unix \
		        -Ihost/fr_config -Ihost/config -DSV_UNIX -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION -DHAVE_SETSID \
		        -DHAVE_STRCASECMP -DHAVE_NETDB_H -DHAVE_NETINET_H \
			host/fr_log/logger.cpp \
			host/fr_config/svconfig.cpp \
		        host/fr_common/svtransport.cpp \
			host/fr_common/unix/portableunix.cpp \
			host/fr_common/unixagenthelpers.cpp \
			host/fr_common/svtypes.cpp \
			host/fr_common/svutils.cpp \
			host/fragent/fragent.cpp \
		        host/fxdspace/fxdspace.cpp -o dspace -Llib $LIBS $LD_FLAGS"

		GCC_OPTS_REMOTECLI="-Bstatic -Wl,+blib -Wno-deprecated -D _REENTRANT -Ithirdparty/curl-7.59.0/config_release/include -I/usr/local/include \
			-Ihost/remotecli -Ihost/common \
			-Ihost/cdplibs -DSV_UNIX -DSV_GENUUID -DSV_SUNOS -DSTDC_HEADERS -DHAVE_UNISTD_H -DHAVE_SIGACTION -DHAVE_SETSID \
			-DHAVE_STRCASECMP -DHAVE_STRING_H -DHAVE_NETINET_H -DHAVE_NETDB_H host/remotecli/main.cpp \
			host/remotecli/remotecli.cpp host/common/svutilities.cpp -o remotecli \
			-Llib $LIBS $LD_FLAGS"
		
fi

case `uname` in
        AIX)
             if [ -n "$LIBPATH" ]; then
               	unset LIBPATH
             fi
	;;

        SunOS)
              # Setting the LD_LIBRARY_PATH
	      if [ -n "$LD_LIBRARY_PATH" ]; then
                 unset LD_LIBRARY_PATH
	      fi
	;;
        HP-UX)
		if [ -n "$SHLIB_PATH" ]; then
		  unset SHLIB_PATH
		fi 
	;;
esac

##
# Step 2: BUILD
# Component: inmsync
##

which gcc | grep "no gcc" > /dev/null 2>&1 || GCC_PATH=`dirname \`which gcc\``
PATH=$PATH:/usr/local/bin:$GCC_PATH
export PATH

if [ $BUILD_INMSYNC -eq 1 ]; then
	if [ -d $TOP_BLD_DIR/source/host/inmsync ]
	then
		cd $TOP_BLD_DIR/source/host/inmsync

		# Clean up existing binary before attempting to build again
		rm -f inmsync $TOP_BLD_DIR/binaries/FX/inmsync > /dev/null

		gmake -f Makefile

		if [ -s "inmsync" ]; then
			echo "inmsync: BUILD SUCCESSFUL!"
			mv inmsync ../../../binaries/FX/
			cd $CUR_W_D
		else
			echo "inmsync: BUILD FAILED!"
			cd $CUR_W_D
			exit 96
		fi
	else
		echo "Directory $TOP_BLD_DIR/source/host/inmsync not found!"
		exit 97
	fi
fi

##
# Step 3: BUILD
# Component: SVFRD
##

if [ $BUILD_SVFRD -eq 1 ]; then 
	if [ -d $TOP_BLD_DIR/source ]; then
		# Change directories to the 'source' directory since compiler flags are set as host/...
		cd $TOP_BLD_DIR/source

		# Clean up existing binary before attempting to build again
		rm -f  ../binaries/FX/svfrd > /dev/null

		# Compilation of svfrd
		$COMPILE $GCC_OPTS_SVFRD

		if [ -s "svfrd" ]; then
			echo "svfrd: BUILD SUCCESSFUL!"
			mv svfrd ../binaries/FX/
			cd $CUR_W_D
		else
			echo "svfrd: BUILD FAILURE!"
			cd $CUR_W_D
			exit 98
		fi
	else
		echo "Directory $TOP_BLD_DIR/source not found!"
		exit 99
	fi
fi


##
# Step 4: BUILD
# Component: UNREGAGAENT
##

if [ $BUILD_UNREG -eq 1 ]; then
        if [ -d $TOP_BLD_DIR/source ]; then
		# Change directories to the 'source' directory since compiler flags are set as host/...
		cd $TOP_BLD_DIR/source

                # Clean up existing binary before attempting to build again
                rm -f  ../binaries/FX/unregfragent > /dev/null

		# Compilation of unregfragent
		$COMPILE $GCC_OPTS_UNREGAGENT

		if [ -s "unregfragent" ]
		then
			echo "unregfragent: BUILD SUCCESSFUL!"
			mv unregfragent ../binaries/FX/
			cd $CUR_W_D
		else
   			echo "unregfragent: BUILD FAILURE!"
			cd $CUR_W_D
   			exit 100
		fi
	else
		echo "Directory $TOP_BLD_DIR/source not found!"
		exit 99
	fi
fi

##
# Component: ALERT
##
if [ $BUILD_ALERT -eq 1 ]; then
        if [ -d $TOP_BLD_DIR/source ]; then
                # Change directories to the 'source' directory since compiler flags are set as host/...
                cd $TOP_BLD_DIR/source

                # Clean up existing binary before attempting to build again
                rm -f  ../binaries/FX/alert > /dev/null

                # Compilation of alert
                $COMPILE $GCC_OPTS_ALERT

                if [ -s "alert" ]
                then
                        echo "alert: BUILD SUCCESSFUL!"
                        mv alert ../binaries/FX/
                        cd $CUR_W_D
                else
                        echo "alert: BUILD FAILURE!"
                        cd $CUR_W_D
                        exit 101
                fi
        else
                echo "Directory $TOP_BLD_DIR/source not found!"
                exit 99
        fi
fi
##
# Component: DSPACE
##

if [ $BUILD_DSPACE -eq 1 ]; then
        if [ -d $TOP_BLD_DIR/source ]; then
                # Change directories to the 'source' directory since compiler flags are set as host/...
                cd $TOP_BLD_DIR/source

                # Clean up existing binary before attempting to build again
                rm -f  ../binaries/FX/dspace > /dev/null

                # Compilation of dspace
                $COMPILE $GCC_OPTS_DSPACE

                if [ -s "dspace" ]
                then
                        echo "dspace: BUILD SUCCESSFUL!"
                        mv dspace ../binaries/FX/
                        cd $CUR_W_D
                else
                        echo "dspace: BUILD FAILURE!"
                        cd $CUR_W_D
                        exit 102
                fi
        else
                echo "Directory $TOP_BLD_DIR/source not found!"
                exit 99
        fi
fi
##
# Component: REMOTECLI
##

if [ $BUILD_REMOTECLI -eq 1 ]; then
        if [ -d $TOP_BLD_DIR/source ]; then
                # Change directories to the 'source' directory since compiler flags are set as host/...
                cd $TOP_BLD_DIR/source

                # Clean up existing binary before attempting to build again
                rm -f  ../binaries/FX/remotecli > /dev/null

                # Compilation of dspace
                $COMPILE $GCC_OPTS_REMOTECLI

                if [ -s "remotecli" ]; then
                        echo "remotecli: BUILD SUCCESSFUL!"
                        mv remotecli ../binaries/FX/
                        cd $CUR_W_D
                else
                        echo "remotecli: BUILD FAILURE!"
                        cd $CUR_W_D
                        exit 102
                fi
        else
                echo "Directory $TOP_BLD_DIR/source not found!"
                exit 99
        fi
fi

##
# Component: FX_VACP for UNIX
##

if [ `uname` = "SunOS" -a "$CONFIG" = "debug" ]; then
	DEBUG_OPT="debug=yes"
	CONF="debug"
else
	DEBUG_OPT="debug=no"
	CONF="release"
fi

if [ $BUILD_VACP -eq 1 ]; then
        if [ -d $TOP_BLD_DIR/source/host ]; then
                # Change directories to the 'source/host' directory for compiling vacp using make
                cd $TOP_BLD_DIR/source/host

                # Clean up existing binary before attempting to build again
                rm -f ../binaries/FX/vacp > /dev/null

                 # Compilation of vacp
                 gmake vacpunix verbose=yes FX_VACP=yes $DEBUG_OPT

                if [ -s "fx_vacp/vacpunix/${CONF}/vacp" ]; then
                        echo "vacp: BUILD SUCCESSFUL!"
                        cp fx_vacp/vacpunix/${CONF}/vacp ../../binaries/FX/
                        cd $CUR_W_D
                else
                        echo "vacp: BUILD FAILURE!"
                        cd $CUR_W_D
                        exit 103
                fi
        else
                echo "Directory $TOP_BLD_DIR/source not found!"
                exit 99
        fi
fi

if [ `uname` = "SunOS" -a "$CONFIG" = "release" ]; then
	cd $TOP_BLD_DIR/binaries/FX/
	for binary in inmsync alert dspace svfrd unregfragent remotecli vacp	
	do
		gobjcopy --only-keep-debug ${binary} ${binary}.dbg
		gobjcopy --strip-debug ${binary}
		gobjcopy --add-gnu-debuglink=${binary}.dbg ${binary}
	done
	cd $CUR_W_D
fi
