#!/bin/sh -x

##
# Defining the Usage
##
USAGE="Usage:   ./fx_checkout.sh <CVS Username> [-t thirdparty path] [Top Bld Dir] [CVS Tag Name]. Default values for parameters in square brackets are to checkout thirdparty under source, use current directory, HEAD."
[ -z "$1" ] && echo $USAGE && exit 75

##
# Setting the required variables
##
CVS_USERNAME=$1
if [ "$2" = "-t" ]; then
	THIRDPARTY_PATH=$3

	TOP_BLD_DIR=${4:-`pwd`}
	CVS_TAG=${5:-HEAD}
else
	TOP_BLD_DIR=${2:-`pwd`}
	CVS_TAG=${3:-HEAD}
fi
CVS_CMD="cvs co -r $CVS_TAG"
CHKOUT_LOG="checkout_$CVS_TAG.log"

##
# set the common required shell variables
##

# CVSROOT: This is required for checking out source code from CVS
CVSROOT=:pserver:${CVS_USERNAME}@10.150.24.11:/src
CVSROOT_THIRDPARTY=:pserver:${CVS_USERNAME}@10.150.24.11:/thirdparty
export CVSROOT

# PATH: Adding CVS path to PATH variable
if [ -f /usr/bin/cvs ]; then
        PATH=$PATH:/usr/bin
else
        PATH=$PATH:/usr/local/bin
fi

export PATH

# First, change directories to the source folder where checkout will take place
if [ -d $TOP_BLD_DIR/source ]; then
  CUR_W_D=`pwd`
  cd $TOP_BLD_DIR/source
else
  echo "Directory $TOP_BLD_DIR/source not found!"
  exit 76
fi
: > $CHKOUT_LOG

# Guess the OS of the build machine by checking out OS_details.sh and running it
# Now the variable OS will hold the OS name and bit details
cvs co -r $CVS_TAG build/scripts/general/OS_details.sh 

chmod +x build/scripts/general/OS_details.sh
. build/scripts/general/OS_details.sh

case `uname` in
        AIX)
	      if [ `uname -v` -eq 5 ] ; then 
                case `uname -r` in
                        3) AIX_LIB="thirdparty/aix/lib53/st_libs"; CURL_HEADERS="thirdparty/aix/lib53/curl-7.59.0/include/curl" ;;
                esac
	      elif [ `uname -v` -eq 6 ] ; then
		case `uname -r` in
			1)AIX_LIB="thirdparty/aix/lib61/st_libs" ;;
		esac
	      fi
                FILE_LIST="$AIX_LIB $CURL_HEADERS thirdparty/Notices/notices.txt"
                ln -s -f $AIX_LIB lib
                for FILE in $FILE_LIST
                do
			CVSROOT=$CVSROOT_THIRDPARTY
			export CVSROOT
                        cvs co $FILE >> $CHKOUT_LOG 2>&1
                        chmod +x $FILE
                        [ $? -ne 0 ] && echo "CVS Checkout failed for $FILE" >> $CHKOUT_LOG
                done
		chmod -R +x lib
		;;
        SunOS)
                case `uname -r` in
                        5.8)
				FILE_LIST="thirdparty/solaris/58/lib/st_libs thirdparty/Notices/notices.txt"
				ln -s -f thirdparty/solaris/58/lib/st_libs lib
                                ;;
			5.9)
				FILE_LIST="thirdparty/solaris/59/lib/st_libs thirdparty/Notices/notices.txt"
				ln -s -f thirdparty/solaris/59/lib/st_libs lib
				;;
                        5.10)
			    case `uname -p` in
			     sparc)
				FILE_LIST="thirdparty/solaris/510/lib/st_libs thirdparty/Notices/notices.txt"
				ln -s -f thirdparty/solaris/510/lib/st_libs lib ;;
			      i386|i86pc)
				FILE_LIST="thirdparty/solaris/510/lib_x86/st_libs thirdparty/Notices/notices.txt"
				ln -s -f thirdparty/solaris/510/lib_x86/st_libs lib ;;
			    esac
			    ;;
			5.11)
				case `uname -p` in
				    i386|i86pc)
					FILE_LIST="thirdparty/solaris/511/lib_x86/st_libs thirdparty/Notices/notices.txt"
					ln -s -f thirdparty/solaris/511/lib_x86/st_libs lib ;;
				esac
			;;	
                esac
                for FILE in $FILE_LIST
                do
			CVSROOT=$CVSROOT_THIRDPARTY
			export CVSROOT
                        cvs co $FILE >> $CHKOUT_LOG 2>&1
                        chmod +x $FILE
                        [ $? -ne 0 ] && echo "CVS Checkout failed for $FILE" >> $CHKOUT_LOG
                done
		chmod -R +x lib
		;;

        HP-UX)
                if ! uname -a | grep -iq ia64; then
                        # PA-RISC architecture
                        FILE_LIST="thirdparty/hpux/lib/st_libs thirdparty/Notices/notices.txt"

                        for FILE in $FILE_LIST
                        do
				CVSROOT=$CVSROOT_THIRDPARTY
				export CVSROOT
                                cvs co $FILE >> $CHKOUT_LOG 2>&1
                                chmod +x $FILE
                                [ $? -ne 0 ] && echo "CVS Checkout failed for $FILE" >> $CHKOUT_LOG
                        done

                        chmod -R +x thirdparty/hpux/lib/st_libs
                        ln -s -f thirdparty/hpux/lib/st_libs lib

		else
			case `uname -r` in
			B.11.23)
				# Itanium architecture 11i v2
				FILE_LIST="thirdparty/hpux_itanium/lib/st_libs thirdparty/Notices/notices.txt"
				ln -s -f thirdparty/hpux_itanium/lib/st_libs lib
				for FILE in $FILE_LIST
				do
					CVSROOT=$CVSROOT_THIRDPARTY
					export CVSROOT
					cvs co $FILE >> $CHKOUT_LOG 2>&1
					chmod +x $FILE
					[ $? -ne 0 ] && echo "CVS Checkout failed for $FILE" >> $CHKOUT_LOG
				done
				;;
			B.11.31)
				# Itanium architecture 11i v3
				FILE_LIST="thirdparty/hpux_itanium11iv3/lib/st_libs thirdparty/Notices/notices.txt"
				ln -s -f thirdparty/hpux_itanium11iv3/lib/st_libs lib
				for FILE in $FILE_LIST
				do
					CVSROOT=$CVSROOT_THIRDPARTY
					export CVSROOT
					cvs co $FILE >> $CHKOUT_LOG 2>&1
					chmod +x $FILE
					[ $? -ne 0 ] && echo "CVS Checkout failed for $FILE" >> $CHKOUT_LOG
				done
				;;
			esac
		fi
		chmod -R +x lib
		;;
esac

# This is the checkout part that's common to all the OSes that FX is built on
CVSROOT=:pserver:${CVS_USERNAME}@10.150.24.11:/src
export CVSROOT


# Checkout only required files if the OS is non-linux.
		for FILE in \
			build/ \
			host/branding/branding_parameters.h \
			host/fr_common/unix/svtypes.h \
			host/fr_common/unix/portableunix.h \
			host/fr_common/unix/portableunix.cpp \
			host/fr_common/defines.h \
			host/fr_common/svtransport.cpp \
			host/fr_common/svutils.h \
			host/fr_common/portable.h \
			host/fr_common/svmacros.cpp \
			host/fr_common/svtypes.cpp \
			host/fr_common/svstatus.h \
			host/fr_common/hostagenthelpers_ported.h \
			host/fr_common/svtransport.h \
			host/fr_common/unixagenthelpers.cpp \
			host/fr_common/svmacros.h \
			host/fr_common/svutils.cpp \
			host/fr_common/svuuid.h \
			host/fr_common/svuuid.cpp \
			host/fr_common/unregworker.h \
			host/fr_common/compatibility.h \
			host/fr_common/unregworker.cpp \
			host/fr_common/version.h \
			host/fr_config/svconfig.cpp \
			host/fr_config/svconfig.h \
			host/fragent/fragent.h \
			host/fragent/fragent.cpp \
			host/frdaemon/daemon.cpp \
			host/fr_log/logger.cpp \
			host/fr_log/logger.h \
			host/fr_unregagent/unixunreg.cpp \
			host/fr_unregagent/Makefile \
			host/inmsync/ \
			host/conversionlib \
			host/dataprotection \
			build/inmage \
			build/xiotech \
			host/vacp/ACM_SCRIPTS/linux/linuxshare_pre.sh \
			host/vacp/ACM_SCRIPTS/linux/linuxshare_post.sh \
			host/fxdspace/fxdspace.cpp \
			host/fxdspace/Makefile \
			host/fxalert/fxalert.cpp \
			host/fxalert/Makefile \
			host/gcc-depend \
			host/get-generic-version-info \
			host/get-specific-version-info \
			host/set-make-env \
			host/validate-modules-set \
			host/set-make-os-env \
			host/is-generated-header \
			host/depend.sh \
			host/thirdparty_build.sh \
			host/thirdparty_links.sh \
			host/top.mak \
			host/bottom.mak \
			host/once.mak  \
			host/find-dir-deps \
			host/rules.mak \
			host/thirdparty.mak \
			host/Makefile \
			host/fr_log/Makefile \
			host/fr_common/Makefile \
			host/fr_config/Makefile \
			host/fragent/Makefile \
			host/frdaemon/Makefile \
			host/config/transport_settings.h \
			host/common \
			host/remotecli \
			host/cdplibs \
			host/vacplibs \
			host/vacpunix \
			host/fx_vacp-thirdparty.mak \
			host/fx_vacp.mak \
			host/fx_vacp-rules.mak \
			host/AIX_00C14AB24C00-rules.mak \
			host/AIX_00C14AB24C00-thirdparty.mak \
			host/AIX_00C14AB24C00.mak \
			host/AIX_00C4E42C4C00-rules.mak \
			host/AIX_00C4E42C4C00-thirdparty.mak \
			host/AIX_00C4E42C4C00.mak \
			host/AIX_00C4E42C4C00.mak \
			host/HP-UX_ia64-thirdparty.mak \
			host/HP-UX_ia64.mak \
			host/linux-rules.mak \
			host/once.mak \
			server/linux/cx_backup_withfx.sh \
			Solutions/Fabric/Oracle/FX \
			Solutions/Oracle/Fx/cx/logrm_applylist.sh \
			Solutions/Oracle/Fx/cx/logrm_mv_cx.sh \
			Solutions/Oracle/Fx/source/getDBsize.sh \
			Solutions/Oracle/Fx/source/prepdump.sh \
			Solutions/Oracle/Fx/target/preprestore.sh \
			Solutions/Oracle/Fx/target/recovermain.sh \
			Solutions/Oracle/Fx/target/recover.sh
		do
			$CVS_CMD $FILE >> $CHKOUT_LOG 2>&1
			[ $? -ne 0 ] && echo "CVS Checkout failed for $FILE" >> $CHKOUT_LOG
		done

# Sometimes dependency checking seems to loop forever seemingly because 
# checked out files are timestamped in the future after checkout
touch host/*

# Build uses some of the header files of thirdparty boost.
# Create a softlink to thirdparty boost.

mkdir -p ${TOP_BLD_DIR}/source/thirdparty/boost
cd ${TOP_BLD_DIR}/source/thirdparty/boost
ln -s ${THIRDPARTY_PATH}/boost/boost_1_78_0 boost_1_78_0

if grep "CVS Checkout failed for" $CHKOUT_LOG 2>/dev/null ; then
	echo "CVS checkout failed."
	exit 77
else
	rm -f $CHKOUT_LOG > /dev/null 2>&1
fi

cd $CUR_W_D
