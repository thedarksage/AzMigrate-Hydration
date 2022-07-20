#!/bin/sh -x

USAGE="Usage:   ./fx_versioning.sh <top build dir> [partner] [branch] [buildquality] [buildphase] [Major Version] [Minor Version] [Release Number]. Default value for arguments in square brackets are 'inmage', 'HEAD', 'DAILY' and 'DIT'."
[ -z "$1" ] && echo $USAGE && exit 85

##
# Setting the required variables
##

TOP_BLD_DIR=$1
PARTNER=${2:-inmage}
branchName=${3:-HEAD}
buildQuality=${4:-DAILY}
buildPhase=${5:-DIT}
verYear=`date +%Y`
verMonth=`date +%h`
verDate=`date +%d`
maj_ver=${6:-5}
min_ver=${7:-50}

patch_set_ver=00
patch_ver=00

rel_num=${8:-1}
bld_seq_num=1
relTagString="${buildQuality}_${maj_ver}-${min_ver}-${rel_num}_${buildPhase}"

case $branchName in
        TRUNK|TRUNC|HEAD|SCOUT_7-2_OSS_SEC_FIXES_BRANCH)
                refSecs=1104517800 ;;
esac

PATCH_VERSION=${maj_ver}.${min_ver}.${patch_set_ver}.${patch_ver}

##
# Calculate the number of seconds elapsed till now from UNIX epoch
# and thereby the tag name for this particular release
# There are 2 ways of calculating this figure - either by date +%s
# command or by compiling and storing in CVS a program called dcal
# which can return the figure. date +%s seems to be more like
# a GNU extension than anything else so don't expect all OSes to
# accommodate this command-argument.
##

. ${TOP_BLD_DIR}/source/build/scripts/general/OS_details.sh
DCAL_BIN="${TOP_BLD_DIR}/source/build/scripts/general/bin/dcal_${OS}"

if [ -f $DCAL_BIN ]; then
	no_of_secs_now=`$DCAL_BIN`
else
        no_of_secs_now=`date +%s`
fi

dlybldNum=`expr \`expr $no_of_secs_now - $refSecs\` / 86400`

case $PARTNER in
        inmage|InMage) PARTNER_VERSION_STRING="InMage" ;;
        xiotech|Xiotech) PARTNER_VERSION_STRING="Xiotech" ;;
        *) echo "$partner not handled yet in this script...aborting!" && exit 1 ;;
esac

tagName="${relTagString}_${dlybldNum}_${verMonth}_${verDate}_${verYear}_${PARTNER_VERSION_STRING}"

##################################
# Source the build param file here
# All env variables used to hold
# constants for this build are to
# be found in that file...
# This is an attempt to minimise
# any sort of hardcoding........
##################################

# Change the version depending on the version that is passed as an argument
# If the version is 500 change the version in the spec files and in the build_params_partner.conf file
sed -e "s/6.50.00.00/${maj_ver}.${min_ver}.00.00/g" build_params_${PARTNER}.conf >> build_params_${PARTNER}.conf.new
mv build_params_${PARTNER}.conf.new build_params_${PARTNER}.conf

# Find the version from the spec file.
version=`cat templates/${PARTNER}.spec | grep "^Version" | cut -d":" -f2 | tr -d " "`
release=`cat templates/${PARTNER}.spec | grep "^Release" | cut -d":" -f2 | tr -d " "`
sed -e "s/$version/${maj_ver}.${min_ver}/" templates/${PARTNER}.spec >> templates/${PARTNER}.spec.new
mv templates/${PARTNER}.spec.new templates/${PARTNER}.spec

sed -e "s/$release/${rel_num}/" templates/${PARTNER}.spec >> templates/${PARTNER}.spec.new
mv templates/${PARTNER}.spec.new templates/${PARTNER}.spec

. ./build_params_${PARTNER}.conf

##################################
# Create the version file here
##################################

if [ -d $TOP_BLD_DIR/binaries/FX ]; then
  echo "VERSION=$FX_VERSION" > $TOP_BLD_DIR/binaries/FX/${VERSION_FILE_NAME}
  echo "PROD_VERSION=$PATCH_VERSION" >> $TOP_BLD_DIR/binaries/FX/${VERSION_FILE_NAME}
  echo "PARTNER=$PARTNER" >> $TOP_BLD_DIR/binaries/FX/${VERSION_FILE_NAME}
  echo "INSTALLATION_DIR=RUNTIME_INSTALL_PATH" >> $TOP_BLD_DIR/binaries/FX/${VERSION_FILE_NAME}
  if [ `uname` = "Linux" ] && [ ! -e /etc/debian_version ] ; then
  	echo "RPM_PACKAGE=$BUILT_RPM_PACKAGE" >> $TOP_BLD_DIR/binaries/FX/${VERSION_FILE_NAME}
  	echo "RPM_NAME=$BUILT_RPM_FILE" >> $TOP_BLD_DIR/binaries/FX/${VERSION_FILE_NAME}
  fi
  echo "VERSION_FILE_PARENT_DIR=$METADATA_REF_DIR" >> $TOP_BLD_DIR/binaries/FX/${VERSION_FILE_NAME}
  echo "DEFAULT_UPGRADE_DIR=$DEFAULT_UPG_DIR" >> $TOP_BLD_DIR/binaries/FX/${VERSION_FILE_NAME}
  echo "BUILD_TAG=$tagName" >> $TOP_BLD_DIR/binaries/FX/${VERSION_FILE_NAME}
  echo "OS_BUILT_FOR=$OS" >> $TOP_BLD_DIR/binaries/FX/${VERSION_FILE_NAME}

else
  exit 87
fi

######################################
# Create the build manifest file here
######################################

if [ -d $TOP_BLD_DIR/binaries/FX ]; then
  # 2. List of files
  # Since CVS/Entries contains entries for files checked out, it's easy to copy its contents to this file without much ado
  echo >> $TOP_BLD_DIR/binaries/FX/${BUILD_MANIFEST_FILE_NAME}
  echo >> $TOP_BLD_DIR/binaries/FX/${BUILD_MANIFEST_FILE_NAME}
  echo "Following is the list of files used in this build:" >> $TOP_BLD_DIR/binaries/FX/${BUILD_MANIFEST_FILE_NAME}
  echo >> $TOP_BLD_DIR/binaries/FX/${BUILD_MANIFEST_FILE_NAME}
  echo >> $TOP_BLD_DIR/binaries/FX/${BUILD_MANIFEST_FILE_NAME}
  find $TOP_BLD_DIR/source -type f -name "Entries" -exec cat {} \; | grep -v CVS | grep -v "^D" \
	>> $TOP_BLD_DIR/binaries/FX/${BUILD_MANIFEST_FILE_NAME}
else
  exit 87
fi


######################################################################################
# Version file should be changed to reflect correct release versions and partner names
######################################################################################

if [ -f $TOP_BLD_DIR/source/host/fr_common/version.h ]; then
	sed -n "s/INMAGE_PRODUCT_VERSION_STR[ ]\{1,\}\".*\"/INMAGE_PRODUCT_VERSION_STR \"$tagName\"/p" \
		$TOP_BLD_DIR/source/host/fr_common/version.h > $TOP_BLD_DIR/source/host/fr_common/version.h.new
	sed -n "s/INMAGE_PRODUCT_VERSION[ ]\{1,\}[0-9,]*/INMAGE_PRODUCT_VERSION $maj_ver,${min_ver}${rel_num},\
${dlybldNum},${bld_seq_num}/p" $TOP_BLD_DIR/source/host/fr_common/version.h \
			>> $TOP_BLD_DIR/source/host/fr_common/version.h.new
	sed -n "s/INMAGE_PRODUCT_VERSION_MAJOR[ ]\{1,\}[0-9]*/INMAGE_PRODUCT_VERSION_MAJOR $maj_ver/p" \
		$TOP_BLD_DIR/source/host/fr_common/version.h >> $TOP_BLD_DIR/source/host/fr_common/version.h.new
	sed -n "s/INMAGE_PRODUCT_VERSION_MINOR[ ]\{1,\}[0-9]*/INMAGE_PRODUCT_VERSION_MINOR ${min_ver}${rel_num}/p" \
		$TOP_BLD_DIR/source/host/fr_common/version.h >> $TOP_BLD_DIR/source/host/fr_common/version.h.new
	sed -n "s/INMAGE_PRODUCT_VERSION_BUILDNUM[ ]\{1,\}[0-9]*/INMAGE_PRODUCT_VERSION_BUILDNUM $dlybldNum/p" \
		$TOP_BLD_DIR/source/host/fr_common/version.h >> $TOP_BLD_DIR/source/host/fr_common/version.h.new
	sed -n "s/INMAGE_PRODUCT_VERSION_PRIVATE[ ]\{1,\}[0-9]*/INMAGE_PRODUCT_VERSION_PRIVATE $bld_seq_num/p" \
		$TOP_BLD_DIR/source/host/fr_common/version.h >> $TOP_BLD_DIR/source/host/fr_common/version.h.new
	sed -n "s/INMAGE_HOST_AGENT_CONFIG_CAPTION[ ]\{1,\}\".*\"/INMAGE_HOST_AGENT_CONFIG_CAPTION \"$tagName\"/p" \
		$TOP_BLD_DIR/source/host/fr_common/version.h >> $TOP_BLD_DIR/source/host/fr_common/version.h.new
	sed -n "s/PROD_VERSION[ ]\{1,\}\".*\"/PROD_VERSION \"$PATCH_VERSION\"/p" \
		$TOP_BLD_DIR/source/host/fr_common/version.h >> $TOP_BLD_DIR/source/host/fr_common/version.h.new

       	mv $TOP_BLD_DIR/source/host/fr_common/version.h.new $TOP_BLD_DIR/source/host/fr_common/version.h
fi

######################################################################################
# End of correcting version file
######################################################################################		
