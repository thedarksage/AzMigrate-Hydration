#!/bin/bash
#Author:    InMage Systems Pvt. Ltd.
#Purpose:   Scout CX build and packaging script.
##################################################

#################
# Functions
#################


Usage()
{
    cat <<EOF

    Usage:  $0 [-d <top-src-code-dir] [-c <binary-copy-path>] [-m <build-mode>] [-p <partner>] [-P <build-phase>] [-j <Major Version>] [-n <Minor Version>] [-s <Patch Set Version> ] [-v Patch Version] [-q] [-h]

    -d	top source code directory having admin, build, host, server, thirdparty directories checked out under itself
	defaults to $TOP_DIR (pwd)

    -c	directory to where the built RPMs should be copied
	defaults to $BIN_DIR (pwd)

    -m	build mode - build RPM packages for process server (ps), configuration server (cs), common base (common),
	thirdparty (tp), packaging (pkg) or everything (all)

	defaults to $BUILD_MODE

    -p	partner
	defaults to $PARTNER

    -P	build phase
	defaults to $BUILD_PHASE

    -j  Major Version
    -n  Minor Version
    -s  Patch Set Version
    -v  Patch Version

    -h	print this help and exit

EOF

 exit 1

}

CleanUp()
{
    cd $THIS_SCRIPT_DIR

    rm -rf BUILD SPECS SOURCES RPMS ${PRODUCT}-${OS}* transband-setup
    mkdir  BUILD SPECS SOURCES RPMS transband-setup

    cd $THIS_DIR
}

BasicSetup()
{

    [[ ! -d ${BIN_DIR}/SYMBOLS ]] && mkdir -p ${BIN_DIR}/SYMBOLS || rm -rf ${BIN_DIR}/{SYMBOLS,*.rpm}
    
    if [ -f $TOP_DIR/build/scripts/general/OS_details.sh ] ; then
        . $TOP_DIR/build/scripts/general/OS_details.sh
        if (grep -q 'CentOS release 6.4 (Final)' /etc/redhat-release || grep -q 'Red Hat Enterprise Linux Server release 6.4 (Santiago)' /etc/redhat-release) && [ "$OS" = "RHEL6-64" ] && [ $(echo `uname -r`| grep -E '2.6.32-358|2.6.32-358.23.2') ]; then
            if uname -a | grep -q x86_64; then
                OS="RHEL6U4-64"
            fi
        fi
        [ -z "$OS" ] && echo "This platform is not supported for building CX server!" && exit 1
    else
        echo "$TOP_DIR/build/scripts/general/OS_details.sh is missing. Cannot proceed!" && exit 1
    fi
    
    PHP_INM_CONF="php_inmage.conf"

    # Source branding parameters
    if [ -f $TOP_DIR/build/branding/$PARTNER/branding_parameters.sh ]; then
        . $TOP_DIR/build/branding/$PARTNER/branding_parameters.sh
    else
        echo "$TOP_DIR/build/branding/$PARTNER/branding_parameters.sh is missing. Cannot proceed!" && exit 1
    fi
}

Symbol_Generation()
{
  objcopy --only-keep-debug $1 $1.dbg
  objcopy --strip-debug $1
  objcopy --add-gnu-debuglink=$1.dbg $1
  cp $1.dbg ${BIN_DIR}/SYMBOLS
}

BuildCX()
{
    CleanUp

    cd $TOP_DIR/host && gmake clean
    cd $THIS_DIR 

    # Build diffdatasort binary
    cd $TOP_DIR/host/diffdatasort && rm -f diffdatasort diffdatasort.dbg
    gmake -f cxbuild.mak clean
    if gmake -f cxbuild.mak ; then
        Symbol_Generation diffdatasort
        cp diffdatasort ${THIS_SCRIPT_DIR}/BUILD/ && cd $THIS_DIR
    else
        echo "diffdatasort failed to build. Cannot proceed!"
        cd $THIS_DIR && exit 1
    fi

    # Build inmtouch binary
    cd $TOP_DIR/server/inmtouch && rm -f inmtouch inmtouch.dbg
    if g++ -I../../host/common -I../../host/inmsafecapis -I../../host/inmsafecapis/unix -I../../host/inmsafeint -I../../host/inmsafeint/unix -o inmtouch inmtouch.cpp -g3 ; then
        Symbol_Generation inmtouch
        cp inmtouch ${THIS_SCRIPT_DIR}/BUILD/ && cd $THIS_DIR
    else
        echo "inmtouch failed to build. Cannot proceed!"
        cd $THIS_DIR && exit 1
    fi

    # Build svdcheck cxps cxpscli cxpsclient gencert csgetfingerprint genpassphrase viewcert CSScheduler sslsign binaries
    cd $TOP_DIR/host
    for BinName in `echo svdcheck cxps cxpscli cxpsclient gencert csgetfingerprint genpassphrase viewcert CSScheduler sslsign`
    do
        gmake ${BinName} debug=no verbose=yes
        [[ $? -ne 0 ]] && echo "Error while building ${BinName} . Can not proceed!" && cd $THIS_DIR && exit 1
    done
    
    mkdir -p ${THIS_SCRIPT_DIR}/BUILD/{transport,Cloud}
    mkdir -p ${THIS_SCRIPT_DIR}/BUILD/tm/pm/Cloud/{PHP,vCloud/PHP}    
    cp $X_ARCH/svdcheck/release/svdcheck $X_ARCH/cxpsclient/release/cxpsclient $X_ARCH/csgetfingerprint/release/csgetfingerprint $X_ARCH/genpassphrase/release/genpassphrase $X_ARCH/viewcert/release/viewcert $X_ARCH/gencert/release/gencert $X_ARCH/CSScheduler/release/scheduler  $X_ARCH/sslsign/release/sslsign ${THIS_SCRIPT_DIR}/BUILD/
    cp $X_ARCH/svdcheck/release/svdcheck.dbg $X_ARCH/cxpsclient/release/cxpsclient.dbg $X_ARCH/csgetfingerprint/release/csgetfingerprint.dbg $X_ARCH/genpassphrase/release/genpassphrase.dbg $X_ARCH/CSScheduler/release/scheduler.dbg $X_ARCH/sslsign/release/sslsign.dbg ${BIN_DIR}/SYMBOLS
    cp $X_ARCH/cxps/release/cxps $X_ARCH/cxpscli/release/cxpscli ${THIS_SCRIPT_DIR}/BUILD/transport
    cp $X_ARCH/cxps/release/cxps.dbg $X_ARCH/cxpscli/release/cxpscli.dbg $X_ARCH/gencert/release/gencert.dbg $X_ARCH/viewcert/release/viewcert.dbg ${BIN_DIR}/SYMBOLS
    cp cxpslib/pem/{servercert.pem,serverkey.pem,dh.pem} ${THIS_SCRIPT_DIR}/BUILD/transport
    cp cxpslib/cxps.conf cxps/unix/cxpsctl ${THIS_SCRIPT_DIR}/BUILD/transport

    # Copy other required stuff to BUILD
    cp -rf $TOP_DIR/server/{linux,tm,ha} $TOP_DIR/admin ${THIS_SCRIPT_DIR}/BUILD/
    cp -rf $TOP_DIR/thirdparty/server/yui ${THIS_SCRIPT_DIR}/BUILD/admin/web/ui/
    cp -rf $TOP_DIR/thirdparty/server/yui/custom/cx/* ${THIS_SCRIPT_DIR}/BUILD/admin/web/ui/yui/
    rm -rf ${THIS_SCRIPT_DIR}/BUILD/admin/web/ui/yui/custom/
    cp -rf $TOP_DIR/Solutions/SolutionsWizard/Scripts/p2v_esx/* ${THIS_SCRIPT_DIR}/BUILD/tm/pm/Cloud/
    cp -f $TOP_DIR/thirdparty/server/cpan/PHP/* ${THIS_SCRIPT_DIR}/BUILD/tm/pm/Cloud/PHP
    cp -f $TOP_DIR/thirdparty/server/cpan/PHP/* ${THIS_SCRIPT_DIR}/BUILD/tm/pm/Cloud/vCloud/PHP
    cp -rf $TOP_DIR/thirdparty/server/cpan/* ${THIS_SCRIPT_DIR}/BUILD/tm/pm/
    cp -rf $TOP_DIR/thirdparty/server/XML-RPC/perl/* ${THIS_SCRIPT_DIR}/BUILD/tm/pm/XML/
    cp -rf $TOP_DIR/thirdparty/server/XML-RPC/php/* ${THIS_SCRIPT_DIR}/BUILD/admin/web/xml_rpc/
    cp -rf $TOP_DIR/thirdparty/server/XML-SWF/* ${THIS_SCRIPT_DIR}/BUILD/admin/web/ui/
    cp -f $TOP_DIR/thirdparty/server/PHP/PEAR.php ${THIS_SCRIPT_DIR}/BUILD/admin/web/ui
    cp -rf $TOP_DIR/thirdparty/server/jquery ${THIS_SCRIPT_DIR}/BUILD/admin/web/ui
    cp -f $TOP_DIR/server/rpm/PostDeployConfigCX.sh ${THIS_SCRIPT_DIR}/BUILD/Cloud/

    # calculate tag string
    DLY_BLD_NUM=`expr \`expr \\\`date +%s\\\` - $REF_SECS\` / 86400`
    TAG_NAME="${REL_TAG_STRING}_${DLY_BLD_NUM}_${VER_MONTH}_${VER_DATE}_${VER_YEAR}_$PARTNER"
    # Perform some necessary replacements
    sed -i -e "s/InMageProfiler/$IN_PROFILER/g; s/InMage/$COMPANY/g; s/Scout-CX/$PRODUCT/g" ${THIS_SCRIPT_DIR}/BUILD/tm/etc/amethyst.conf
    [[ "$PARTNER" = "xiotech" ]] && sed -i -e "s/^IN_APPLABEL[ \t ]*=[ \t ]\"CX\"/IN_APPLABEL     = \"TS\"/g" ${THIS_SCRIPT_DIR}/BUILD/tm/etc/amethyst.conf
    sed -e "s/Scout CX Dashboard/$COMPANY $LABEL Dashboard/g; s/InMage CX/$COMPANY $LABEL/g" $TOP_DIR/admin/web/ui/index.html > ${THIS_SCRIPT_DIR}/BUILD/admin/web/ui/index.html
    [[ "$PARTNER" = "xiotech" ]] && sed "s/InMage Scout CX/Xiotech TimeScale TS/" $TOP_DIR/admin/web/trends/logs/TrapLog.txt > ${THIS_SCRIPT_DIR}/BUILD/admin/web/trends/logs/TrapLog.txt
    [[ "$PARTNER" = "xiotech" ]] && cp ${THIS_DIR}/xiotech/images/favicon.ico ${THIS_SCRIPT_DIR}/BUILD/admin/web/favicon.ico
    cp ${THIS_DIR}/inmagecx.spec ${THIS_SCRIPT_DIR}/SPECS/

    # Build inmlicense binary
    cd $TOP_DIR/server/license && rm -f output/{inmlicense,inmlicense.dbg}

    if ./build.sh ; then
        cur_wd=`pwd`
        cd output && Symbol_Generation inmlicense
        cd $cur_wd
        cp output/inmlicense $THIS_SCRIPT_DIR/BUILD/
        cd $THIS_SCRIPT_DIR
    else
        echo "inmlicense failed to build. Cannot proceed!"
        cd $THIS_DIR && exit 1
    fi

    # Remove CVS directories
    find ${THIS_SCRIPT_DIR}/BUILD -type d -name CVS | xargs rm -rf >/dev/null 2>&1

    # Generate inmagecx rpm
    rpmbuild -bb --define="OSNAME $OS" --define="_topdir `pwd`" --define="cliname $PARTNER" --define="clirelease 1" --define="cliversion ${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_SET_VERSION}.${PATCH_VERSION}" --define="_use_internal_dependency_generator 1" SPECS/inmagecx.spec

    [[ $? -ne 0 ]] && echo "inmageCX RPM failed to build. Cannot proceed!" && cd $THIS_DIR && exit 1
	echo "CX RPM build successful!" && mv RPMS/$RPM_ARCH/${PARTNER}*.rpm ${BIN_DIR} && cd $THIS_DIR
}

DoPackaging()
{
    cd $THIS_SCRIPT_DIR && CleanUp
    # Build PushInstallService binary
    cd $TOP_DIR/host
    if gmake PushInstallService storagefailover debug=no verbose=yes ; then
        cp $X_ARCH/PushInstallService/release/pushinstalld PushInstallService/pushinstaller.conf $X_ARCH/storagefailover/release/storagefailover ${THIS_SCRIPT_DIR}/
        cp $X_ARCH/PushInstallService/release/pushinstalld.dbg $X_ARCH/storagefailover/release/storagefailover.dbg ${BIN_DIR}/SYMBOLS/
        cd $THIS_DIR
    else
        echo "PushInstallService or storagefailover failed to build. Cannot proceed!"
        cd $THIS_DIR && exit 1
    fi
 
    cd $THIS_SCRIPT_DIR

    mkdir -p transband-setup/php-5.1 transband-setup/php-5.2 transband-setup/Additional_Checks
    cp -f $TOP_DIR/server/tm/bin/{readonlycheckd.pl,GetNICDetails.pl} transband-setup    
    cp -f $TOP_DIR/server/tm/readonlychecksd transband-setup    
    cp -f $TOP_DIR/server/tm/etc/version_pillar transband-setup    
    cp -f $TOP_DIR/server/rpm/{check.sh,web_data_split.sh,csps_merge_split.sh,csps_upgrade.sh,httpd_changes.sh,http_sys_monitor.sh,cxps_setup.sh,cxps_db_setup.sh} transband-setup
    cp -Rf $TOP_DIR/server/linux/log_rotate transband-setup
    cp -f $TOP_DIR/build/scripts/general/OS_details.sh transband-setup
    cp -f $TOP_DIR/build/scripts/general/CreateLiveCD.sh transband-setup
    cp -f $TOP_DIR/server/rpm/virtual_host_template transband-setup/virtual_host_template
    cp -f $TOP_DIR/server/rpm/$PHP_INM_CONF transband-setup
    cp -f $TOP_DIR/server/rpm/cx_security.conf transband-setup
    cp -f $TOP_DIR/server/rpm/mysql/conf/my.cnf.template transband-setup
    cp -f pushinstalld storagefailover pushinstaller.conf  transband-setup

    if [ "${BUILD_FOR}" = "pillar" ]; then
        cp -rf $TOP_DIR/server/rpm/pillar transband-setup
    else
        cp -rf $TOP_DIR/onlinehelp/help_output/cx/Webhelp transband-setup
        cp -rf $TOP_DIR/server/rpm/cloud transband-setup
    fi
    
    # db-related customisation
    ls -1 $TOP_DIR/server/db | \
	while read filename
	do
	    [ -d $TOP_DIR/server/db/$filename ] && continue
	    sed -e "s/CX/$LABEL/g; s/InMageProfiler/$IN_PROFILER/g" $TOP_DIR/server/db/$filename > transband-setup/$filename
	    chmod +x transband-setup/$filename
	done

    # Packaging ha_modules
    mkdir -p transband-setup/ha_module/HighAvailability
    cp -rf $TOP_DIR/server/ha/* $TOP_DIR/server/tm/pm/Fabric/Constants.pm  transband-setup/ha_module/
    cp -f $TOP_DIR/server/tm/pm/ConfigServer/HighAvailability/*.pm transband-setup/ha_module/HighAvailability
    chmod -R 755 transband-setup/ha_module/

    # Remove any CVS directories
    find transband-setup -type d -name CVS -exec rm -rf {} \; >/dev/null 2>&1
    
    # Create support_ui.tar.gz package.
    gtar -zcvf transband-setup/ha_module.tar.gz transband-setup/ha_module ; rm -rf transband-setup/ha_module/
    
    # Customise upgrade, install & uninstall scripts
    for filename in csps_install.sh csps_upgrade.sh
    do
        sed -e "s/BUILDTIME_PRODUCT/$PRODUCT/g; s/[[:space:]]CX[[:space:]]/ $LABEL /g; s/BUILDTIME_SRCVERSION/$OS/g" $TOP_DIR/server/rpm/$filename > transband-setup/$filename
        chmod +x transband-setup/$filename
        [ "$filename" = "csps_install.sh" ] && mv transband-setup/csps_install.sh transband-setup/install.sh
    done

    sed "s/[[:space:]]CX[[:space:]]/ $LABEL /g" $TOP_DIR/server/rpm/uninstall_template.sh > transband-setup/uninstall_template.sh
    chmod +x transband-setup/uninstall_template.sh

    # changing the version in the configuration files
    VERSION_PHASE_RELEASE="${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_SET_VERSION}.${PATCH_VERSION}_${BUILD_PHASE}"
    sed "s/4.1.0_RC0/${VERSION_PHASE_RELEASE}/g" $TOP_DIR/server/rpm/install.cfg > transband-setup/install.cfg
    sed "s/4.1.0_RC0/${VERSION_PHASE_RELEASE}/g" $TOP_DIR/server/rpm/upgrade.cfg > transband-setup/upgrade.cfg

    # remove any CVS directories
    find transband-setup -type d -name CVS -exec rm -rf {} \; >/dev/null 2>&1
    
    # copy rpm packages
    cp $BIN_DIR/*.rpm $TOP_DIR/thirdparty/Notices/notices.txt transband-setup/

    if [ "${OS}" = "RHEL6-64" ]; then
	if grep -q 'CentOS release 6.2 (Final)' /etc/redhat-release 2>/dev/null || \
	   grep -q 'Red Hat Enterprise Linux Server release 6.2 (Santiago)' /etc/redhat-release 2>/dev/null; then
                OS="RHEL6U2-64"
        fi
    fi

    # Remove any CVS directories
    find transband-setup -type d -name CVS -exec rm -rf {} \; >/dev/null 2>&1

    # Rename transband-setup
    mv transband-setup $PRODUCT-$OS

    # Partner string
    case $PARTNER in
        inmage) PARTNER_STRING="InMage" ;;
    esac

    # Package now
    if tar czvf ${BIN_DIR}/${PARTNER_STRING}_Scout_${LABEL}_${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_SET_VERSION}.${PATCH_VERSION}_${OS}_${BUILD_PHASE}_${VER_DATE}${VER_MONTH}${VER_YEAR}.tar.gz $PRODUCT-$OS ; then
        echo "CX packaging successful!"
    else
        echo "CX packaging failure!"
    fi

    rm -rf BUILD SPECS SOURCES RPMS

    # Generate symbols tar.gz.
    cur_wd=`pwd` && cd ${BIN_DIR}/SYMBOLS
    tar cvfz ${PARTNER_STRING}_${LABEL}_${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_SET_VERSION}.${PATCH_VERSION}_${OS}_${BUILD_PHASE}_${VER_DATE}${VER_MONTH}${VER_YEAR}_release_symbol.tar.gz *.dbg
    cd $cur_wd
}

###### Variables #######

THIS_DIR="`pwd`"
THIS_SCRIPT_DIR="`dirname $0`"
[ "$THIS_SCRIPT_DIR" = "." ] && THIS_SCRIPT_DIR="`pwd`"
[ "$THIS_SCRIPT_DIR" = ".." ] && THIS_SCRIPT_DIR="`pwd`/.."
RPM_ARCH=`rpm -q --qf "%{ARCH}" rpm`
X_ARCH=`uname -ms | sed "s/ /_/g"`
VER_YEAR=`date +%Y`
VER_MONTH=`date +%b`
VER_DATE=`date +%d`

######## Default values #######

TOP_DIR="`pwd`"
BIN_DIR="`pwd`"
BUILD_MODE="all"
BUILD_FOR="prism"
PARTNER="inmage"
BUILD_PHASE="DIT"
BUILD_QUALITY="DAILY"
MAJOR_VERSION="8"
MINOR_VERSION="3"
PATCH_SET_VERSION="0"
PATCH_VERSION="0"

REF_SECS="1104517800"
REL_TAG_STRING="${BUILD_QUALITY}_${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_SET_VERSION}.${PATCH_VERSION}_${BUILD_PHASE}"

########## Parse CLI #######

[ -n "$1" ] && [ "`echo \"echo $1\" | awk '{print $2}' | cut -c1`" != "-" ] && Usage

while getopts d:c:m:p:f:P:j:n:s:v:h opt
do
    case $opt in
	d) TOP_DIR="$OPTARG" ;;
	c) BIN_DIR="$OPTARG" ;;
	m) BUILD_MODE="$OPTARG" ;;
	f) BUILD_FOR="$OPTARG" ;;
	p) PARTNER="$OPTARG" ;;
	P) BUILD_PHASE="$OPTARG" ;;
	j) MAJOR_VERSION="$OPTARG" ;;
	n) MINOR_VERSION="$OPTARG" ;;
	s) PATCH_SET_VERSION="$OPTARG" ;;
	v) PATCH_VERSION="$OPTARG" ;;
	h|*) Usage ;;	
    esac
done

###################
# CLI validations
###################

[ "$BUILD_MODE" != "all" -a "$BUILD_MODE" != "tp" -a "$BUILD_MODE" != "cx" -a "$BUILD_MODE" != "pkg" ] && { echo "Argument can only be \"all\", \"tp\", \"cx\" or \"pkg\"!"; exit 1; }

for Dir_item in "$TOP_DIR" "$BIN_DIR"
do
    if [ "`echo \"$Dir_item\" | cut -c1`" != "/" ]; then
	echo "$Dir_item --- specify an absolute path. Cannot proceed!"
	exit 1
    fi
done

############################
# Decide what's to be built
############################

if [ "$BUILD_MODE" = "all" ] ; then
    BLD_TP=1 BLD_CX=1 MAKE_PKG=1
elif [ "$BUILD_MODE" = "tp" ] ; then
    BLD_TP=1 BLD_CX=0 MAKE_PKG=0
elif [ "$BUILD_MODE" = "cx" ] ; then
    BLD_TP=0 BLD_CX=1 MAKE_PKG=0
elif [ "$BUILD_MODE" = "pkg" ] ; then
    BLD_TP=0 BLD_CX=0 MAKE_PKG=1
fi

############################
# Call all build functions
############################

BasicSetup

if [ $BLD_CX -eq 1 ]; then
	BuildCX
fi

if [ $BLD_CX -eq 1 ]; then
	DoPackaging
fi
