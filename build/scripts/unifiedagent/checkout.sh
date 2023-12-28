#!/bin/sh

# Set the required variables.
TOP_BLD_DIR=${1:-`pwd`}
GIT_TAG=${2:-develop}
CLEAN_REPO=${3:-yes}
CHKOUT_LOG="checkout_$GIT_TAG.log"
X_ARCH=`uname -ms | sed -e s"/ /_/g"`
date_suffix=`date +%e_%b_%Y | awk '{print $1}'`

# Change directories to the source folder where checkout will take place.
if [ -d $TOP_BLD_DIR/source ]; then
  CUR_W_D=`pwd`
  cd $TOP_BLD_DIR/source
else
  echo "Directory $TOP_BLD_DIR/source not found."
  exit 1
fi

if [ $CLEAN_REPO == "yes" ]; then
	rm -rf $TOP_BLD_DIR/source/InMage-Azure-SiteRecovery
	if [ $? -eq 0 ]; then
		echo "Successfully deleted $TOP_BLD_DIR/source/InMage-Azure-SiteRecovery directory."
	else
		echo "Failed to delete $TOP_BLD_DIR/source/InMage-Azure-SiteRecovery directory."
		exit 1
	fi
fi

# checkout workarea if it doesn't exist already.
if [ ! -d $TOP_BLD_DIR/source/InMage-Azure-SiteRecovery ]; then
	: > $CHKOUT_LOG

	git clone -b ${GIT_TAG} --single-branch msazure@vs-ssh.visualstudio.com:v3/msazure/One/InMage-Azure-SiteRecovery >> $CHKOUT_LOG 2>&1
	if [ $? -eq 0 ]; then
		echo "Successfully cloned InMage-Azure-SiteRecovery git repo from branch ${GIT_TAG}." >> $CHKOUT_LOG 2>&1
		cd InMage-Azure-SiteRecovery
	else
		echo "Failed to clone InMage-Azure-SiteRecovery git repo and branch ${GIT_TAG}." >> $CHKOUT_LOG 2>&1
		exit 1
	fi
else
		cd InMage-Azure-SiteRecovery
		git checkout ${GIT_TAG}
		if [ $? -eq 0 ]; then
			echo "Successfully checked out code from ${GIT_TAG}." >> $CHKOUT_LOG 2>&1
		else
			echo "Failed to check out code from ${GIT_TAG}." >> $CHKOUT_LOG 2>&1
		exit 1
		fi
fi

# Replacing build switch with all in ubuntu.sh script. Build switch is used in cloud build and all switch is used in on-prem builds.
sed -i -e "s|user_space/build/ubuntu.sh\ build|user_space/build/ubuntu.sh\ all|g" host/vx_setup.mak
if [ $? -eq 0 ]; then
	echo "Replaced build switch with all in ubuntu.sh script." >> $CHKOUT_LOG 2>&1
else
	echo "Failed to replace build switch with all in ubuntu.sh script." >> $CHKOUT_LOG 2>&1
	exit 1
fi

# Provide execute permissions to all files.
find admin/ build/ host/ server/ Solutions/ -type f -exec chmod 755 {} \; >> $CHKOUT_LOG 2>&1
if [ $? -eq 0 ]; then
	echo "Gave execute permission to all files successfully." >> $CHKOUT_LOG 2>&1
else
	echo "Failed to give execute permissions to all files." >> $CHKOUT_LOG 2>&1
	exit 1
fi

# Convert to Unix line endings.
find admin/ build/ host/ server/ Solutions/ -type f -exec dos2unix {} \; >> $CHKOUT_LOG 2>&1
if [ $? -eq 0 ]; then
    echo "Converted to Unix line endings successfully." >> $CHKOUT_LOG 2>&1
else
    echo "Failed to convert to Unix line endings." >> $CHKOUT_LOG 2>&1
    exit 1
fi

# Temporary workaround till depend.sh is fixed.
find admin/ build/ host/ server/ Solutions/ -type f -exec touch {} \; >> $CHKOUT_LOG 2>&1
if [ $? -eq 0 ]; then
    echo "Touched all the files successfully." >> $CHKOUT_LOG 2>&1
else
    echo "Failed to touch files." >> $CHKOUT_LOG 2>&1
    exit 1
fi

# Removing the exiting symbolic links so that the make will create the respective symbolic links.
rm -f ${THIRDPARTY_PATH}/lib/lib ${THIRDPARTY_PATH}/bin/bin

cd $CUR_W_D
