#!/bin/sh -x

##
# Step 1: USAGE
# This would define the usage and also checks whether the build is successful
# or not for starting the packaging
##

# Define the usage
USAGE="Usage:   ./fx_packaging.sh [partner] [path of binaries]. Defaults are 'inmage' and current directory."

# Set the required variables from the parameters passed as arguments
PARTNER=${1:-inmage}
BINARY_PATH=${2:-`pwd`}
BUILD_PHASE=${3:-DIT}
CONFIG=${4:-debug}

DATE_TIME_SUFFIX=`date +%d%b%Y`

case $PARTNER in
		inmage)
				PARTNER_SPECIFIC_STRING="InMage";;
		xiotech)
				PARTNER_SPECIFIC_STRING="Xiotech";;
esac

# Check to see if build succeeded or not!
if [ -f $BINARY_PATH/svfrd ] && [ -f $BINARY_PATH/unregfragent ] && [ -f $BINARY_PATH/alert ] && [ -f $BINARY_PATH/dspace ] &&  [ -f $BINARY_PATH/remotecli ] && [ -f $BINARY_PATH/vacp ]  
then
	echo "All required binaries ( svfrd, unregfragent, alert, dspace, remotecli and vacp ) are present. Starting the packaging process now..."
else
	echo "Since all or one of the required binaries ( svfrd,alert, dspace, unregfragent, remotecli and vacp) are not present, aborting packaging!"
	exit 105
fi


##
# Step 2: 
# This would define the usage and also checks whether the build is successful
# or not for starting the packaging
##
# Now, if the control reached this point, it means that build was successful, so we can now go on with the packaging stuff
# Current directory is TOP_DIR/source
##

CUR_W_D=`pwd`

# Source the build parameters script here
. ./build_params_${PARTNER}.conf

cd $BINARY_PATH

# Guess the OS
. $BINARY_PATH/../../source/build/scripts/general/OS_details.sh


# Source the branding parameters script here
. ../../source/build/branding/$PARTNER/branding_parameters.sh

# Replace some placeholders in some of the template files during BUILD time
# Whatever placeholders remain will be replaced at INSTALL time

# 1. stop
sed -e "s|PARTNER_OR_COMPANY|$COMPANY|g" ../../source/build/scripts/FX/templates/stop > stop

# 2. start
sed -e "s|PARTNER_OR_COMPANY|$COMPANY|g" ../../source/build/scripts/FX/templates/start > start

# 3. status
sed -e "s|PARTNER_OR_COMPANY|$COMPANY|g" ../../source/build/scripts/FX/templates/status > status

# 4. uninstall
    sed -e "s|PARTNER_OR_COMPANY|$COMPANY|g" ../../source/build/scripts/FX/templates/uninstall | \
    sed -e "s|BUILDTIME_VALUE_RPM_PACKAGE|$BUILT_RPM_PACKAGE|" | \
    sed -e "s|BUILDTIME_VALUE_REF_DIR|$METADATA_REF_DIR|" | \
    sed -e "s|BUILDTIME_VALUE_BLD_MANIFEST_FILE_NAME|$BUILD_MANIFEST_FILE_NAME|" | \
    sed -e "s|BUILDTIME_VALUE_INST_VERSION_FILE|$VERSION_FILE_NAME|" > uninstall

# 5. unregister
sed -e "s|PARTNER_OR_COMPANY|$COMPANY|g" ../../source/build/scripts/FX/templates/unregister | \
sed -e "s|BUILDTIME_VALUE_REF_DIR|$METADATA_REF_DIR|" | \
sed -e "s|BUILDTIME_VALUE_INST_VERSION_FILE|$VERSION_FILE_NAME|" | \
sed -e "s|CX|$LABEL|g" > unregister

# 6. install
    sed -e "s|PARTNER_OR_COMPANY|$COMPANY|g" ../../source/build/scripts/FX/templates/install | \
    sed -e "s|CX|$LABEL|g" | sed -e "s|DEFAULT_PARTNER_INSTALL_DIR|$DEFAULT_INSTALL_DIR|g" | \
    sed -e "s|BUILDTIME_CURRENT_VERSION|$FX_VERSION|" | \
    sed -e "s|BUILDTIME_INST_VERSION_FILE|$VERSION_FILE_NAME|" | \
    sed -e "s|BUILDTIME_BUILD_MANIFEST_FILE|$BUILD_MANIFEST_FILE_NAME|" | \
    sed -e "s|BUILDTIME_MAIN_RPM_FILE|$BUILT_RPM_FILE|" | \
    sed -e "s|BUILDTIME_MAIN_RPM_PACKAGE|$BUILT_RPM_PACKAGE|" | \
    sed -e "s|BUILDTIME_DEFAULT_UPGRADE_DIR|$DEFAULT_UPG_DIR|" | \
    sed -e "s|BUILDTIME_PARTNER_SPECIFIC_STRING|$PARTNER_SPECIFIC_STRING|" | \
    sed -e "s|BUILDTIME_REF_DIR|$METADATA_REF_DIR|" > install

cp ../../source/build/scripts/FX/templates/svagent .
cp ../../source/build/scripts/FX/templates/config.ini .   

# conf_file
sed -e "s|DEFAULT_PARTNER_INSTALL_DIR|$DEFAULT_INSTALL_DIR|g" ../../source/build/scripts/FX/templates/conf_file > conf_file

# Copy unchanged files from templates folder to the curr dir

cp ../../source/build/scripts/FX/templates/killchildren .
cp ../../source/host/vacp/ACM_SCRIPTS/linux/linuxshare_pre.sh .
cp ../../source/host/vacp/ACM_SCRIPTS/linux/linuxshare_post.sh .
cp ../../source/build/scripts/general/OS_details.sh .

# Get the OS variable
. ../../source/build/scripts/general/OS_details.sh


# Setting the option EnableDeleteOptions = 3255 instead of 0 so that it is enabled by default. 

sed -e "s/EnableDeleteOptions = 0/EnableDeleteOptions = 3255/" config.ini > config.ini.new
mv config.ini.new config.ini
chmod +x config.ini

# Copy the fabric/oracle/fx scripts to curr dir
cp ../../source/Solutions/Fabric/Oracle/FX/*.sh .

# Copy the oracle/fx scripts to curr dir
cp ../../source/Solutions/Oracle/Fx/cx/logrm_applylist.sh .
cp ../../source/Solutions/Oracle/Fx/cx/logrm_mv_cx.sh .
cp ../../source/Solutions/Oracle/Fx/source/getDBsize.sh .
cp ../../source/Solutions/Oracle/Fx/source/prepdump.sh .
cp ../../source/Solutions/Oracle/Fx/target/preprestore.sh .
cp ../../source/Solutions/Oracle/Fx/target/recovermain.sh .
cp ../../source/Solutions/Oracle/Fx/target/recover.sh .

# Copy notices.txt here only if OS is not DEBIAN. For DEBIAN we are copying notices.txt in checkout step.
find $BINARY_PATH/../../source -name notices.txt -exec cp {} . \;

if [ -f $BINARY_PATH/notices.txt ]; then
    :
else
    echo "notices.txt file is missing ..."
    exit 1
fi
    
# Grant execute permissions to the scripts above
chmod -R 755 *.*

# This is specific to HPUX
# Giving the chatr enabled for all the binaries svfrd,unregfragent,alert,dspace,remotecli,vacp and inmsync
[ `uname` = "HP-UX" ] && chatr +s enable svfrd unregfragent alert dspace remotecli vacp inmsync

# Now, depending on the target operating system, differentiate the packaging procedure
# If it is RHEL4 or SuSE9 or SuSE10, use RPM. Otherwise, use a simple tar.gz and build the installing
# logic into the install script itself.

if [ `uname` = "SunOS" -a "$CONFIG" = "release" ]; then
	CONF="_release"
elif [ `uname` = "SunOS" -a "$CONFIG" = "debug" ]; then
	CONF="_debug"
else
	CONF=""
fi

# Create temporary directories for building the TAR
mkdir -p TAR_BUILD/lib TAR_BUILD/scripts TAR_BUILD/fabric/scripts
# Copy required files to this directory

		cp -f start stop status uninstall svagent svfrd inmsync unregister alert dspace \
		remotecli unregfragent vacp config.ini notices.txt killchildren TAR_BUILD/
	
		cp -f ../../source/lib/libgcc_s*  ../../source/lib/libstdc++* TAR_BUILD/lib/

		# Copy the oracle/fx scripts to TAR
		cp -f logrm_applylist.sh logrm_mv_cx.sh getDBsize.sh prepdump.sh preprestore.sh recovermain.sh recover.sh TAR_BUILD/scripts
		cp -f application.sh oracle.sh TAR_BUILD/fabric/scripts
		# Enter tar file name into version file as install script will use it
		echo "TAR_FILE_NAME=${COMPANY}_FX_${FX_VERSION}_${OS}.tar" >> $VERSION_FILE_NAME

		# Create the TAR now
		cd TAR_BUILD/
		tar -cvf ../${COMPANY}_FX_${FX_VERSION}_${OS}.tar .
		# If tar created successfully, create the final tar.gz
		if [ $? -eq 0 ]; then
			cd ..
			rm -rf TAR_BUILD/
			chmod +x install
			tar -cvf ${COMPANY}_FX_${FX_VERSION}_${OS}_${BUILD_PHASE}_${DATE_TIME_SUFFIX}${CONF}.tar \
			${COMPANY}_FX_${FX_VERSION}_${OS}.tar install OS_details.sh $VERSION_FILE_NAME \
			$BUILD_MANIFEST_FILE_NAME conf_file
			if [ $? -eq 0 ]; then
				gzip ${COMPANY}_FX_${FX_VERSION}_${OS}_${BUILD_PHASE}_${DATE_TIME_SUFFIX}${CONF}.tar 
				[ $? -eq 0 ] && echo "FX BUILD AND PACKAGING IS SUCCESSFUL FOR $PARTNER on ${OS}..."
			fi
		fi

# Package the symbol files (.dbg) also

if [ `uname` = "SunOS" -a "$CONFIG" = "release" ]; then
    cd $BINARY_PATH
    tar -cvf ${COMPANY}_FX_${FX_VERSION}_${OS}_${BUILD_PHASE}_${DATE_TIME_SUFFIX}_${CONFIG}_symbols.tar *.dbg
    gzip ${COMPANY}_FX_${FX_VERSION}_${OS}_${BUILD_PHASE}_${DATE_TIME_SUFFIX}_${CONFIG}_symbols.tar
    [ $? -eq 0 ] && echo "FX SYMBOL TAR FILE GENERATION IS SUCCESSFUL FOR $PARTNER on ${OS}..."
fi

# Change directories back to the original working directory at the start of this script
cd $CUR_W_D
