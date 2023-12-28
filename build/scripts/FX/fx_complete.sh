#!/bin/sh

##
# Defining different exit codes. This would help track failures better
##

# Exit code table
E_SUCCESS=0
E_GENERIC_FAILURE=1
E_OS_NOT_SUPPORTED=25
E_SETUP_ARG_MISSING=65
E_SETUP_DIRCREATION_FAILURE=66
E_SETUP_CHANGEDIR_FAILURE=67
E_CHECKOUT_ARG_MISSING=75
E_CHECKOUT_SOURCE_DIR_NOT_FOUND=76
E_CHECKOUT_FAILURE=77
E_BUILD_INCORRECT_ARG=95
E_BUILD_INMSYNC_FAILURE=96
E_BUILD_INMSYNC_DIR_NOT_FOUND=97
E_BUILD_SVFRD_FAILURE=98
E_BUILD_SOURCE_DIR_NOT_FOUND=99
E_BUILD_UNREG_FAILURE=100
E_BUILD_ALERT_FAILURE=101
E_BUILD_DSPACE_FAILURE=102
E_BUILD_VACP_FAILURE=103
E_VERSION_ARG_MISSING=85
E_VERSION_BRANCH_MISMATCH=86
E_VERSION_BIN_DIR_NOT_FOUND=87
E_VERSION_DCAL_NOT_FOUND=88
E_PACKAGING_BINARIES_ABSENT=105
E_PACKAGING_PARTNER_DIR_ABSENT=106

##
# This function executes a command and logs the output
##
DO_AND_LOG()
{
	CMD=$1
	eval $CMD >> $FLOG
}

##
# This function verifies the exit status of a program and print messages accordingly
##
VERIFY()
{
	status=$1

	case $status in

		$E_SUCCESS) ;;

                $E_GENERIC_FAILURE)
                        echo "FAILURE WHILE BUILDING. PLEASE SEE ERROR MESSAGES ABOVE!"
                        exit $E_GENERIC_FAILURE ;;

                $E_OS_NOT_SUPPORTED)
                        echo "THIS PLATFORM IS NOT SUPPORTED YET!"
                        exit $E_OS_NOT_SUPPORTED ;;

		$E_SETUP_ARG_MISSING)
		   	echo "SOME OR ALL ARGUMENTS FOR fx_bld_env_setup.sh ARE MISSING!"
			exit $E_SETUP_ARG_MISSING ;;

		$E_SETUP_DIRCREATION_FAILURE)
			echo "DIRECTORY $TOP_BLD_DIR COULD NOT BE CREATED!"
			exit $E_SETUP_DIRCREATION_FAILURE ;;

		$E_SETUP_CHANGEDIR_FAILURE)
			echo "FAILED TO CHANGE DIRECTORY TO $TOP_BLD_DIR!"
			exit $E_SETUP_CHANGEDIR_FAILURE ;;

		$E_CHECKOUT_SOURCE_DIR_NOT_FOUND)
			echo "DIRECTORY source NOT FOUND UNDER TOP BUILD DIRECTORY!"
			exit $E_CHECKOUT_SOURCE_DIR_NOT_FOUND ;;

		$E_CHECKOUT_ARG_MISSING)
			echo "SOME OR ALL ARGUMENTS FOR fx_checkout.sh ARE MISSING!"
			exit $E_CHECKOUT_ARG_MISSING ;;

		$E_CHECKOUT_FAILURE)
			echo "CVS CHECKOUT FAILED FOR SOME FILES!"
			exit $E_CHECKOUT_FAILURE ;;

		$E_BUILD_INCORRECT_ARG)
			echo "INCORRECT ARGUMENTS FOR fx_bld.sh!"
			exit $E_BUILD_INCORRECT_ARG ;;

		$E_BUILD_INMSYNC_DIR_NOT_FOUND)
			echo "DIRECTORY host/inmsync NOT FOUND UNDER source DIRECTORY!"
			exit $E_BUILD_INMSYNC_DIR_NOT_FOUND ;;

		$E_BUILD_INMSYNC_FAILURE)
			echo "BUILD FAILURE FOR inmsync!"
			exit $E_BUILD_INMSYNC_FAILURE ;;

		$E_BUILD_SVFRD_FAILURE)
			echo "BUILD FAILURE FOR svfrd!"
			exit $E_BUILD_SVFRD_FAILURE ;;

		$E_BUILD_UNREG_FAILURE)
			echo "BUILD FAILURE FOR unregagent!"
			exit $E_BUILD_UNREG_FAILURE ;;

		$E_BUILD_ALERT_FAILURE)
			echo "BUILD FAILURE FOR alert!"
			exit $E_BUILD_ALERT_FAILURE ;;

		$E_BUILD_DSPACE_FAILURE)
			echo "BUILD FAILURE FOR dspace!"
			exit $E_BUILD_DSPACE_FAILURE ;;

		$E_BUILD_VACP_FAILURE)
			echo "BUILD FAILURE FOR vacp!"
			exit $E_BUILD_VACP_FAILURE ;;

		$E_VERSION_ARG_MISSING)
			echo "INCORRECT ARGUMENTS FOR fx_packaging.sh!"
			exit $E_VERSION_ARG_MISSING ;;

		$E_VERSION_BIN_DIR_NOT_FOUND)
			echo "DIRECTORY binaries/FX NOT FOUND UNDER ${TOP_BLD_DIR}!"
			exit $E_VERSION_BIN_DIR_NOT_FOUND ;;

		$E_VERSION_DCAL_NOT_FOUND)
			echo "FILE dcal NOT FOUND FOR THIS OS!"
			exit $E_VERSION_DCAL_NOT_FOUND ;;

		$E_PACKAGING_BINARIES_ABSENT)
			echo "ALL BINARIES REQUIRED FOR PACKAGING ARE NOT PRESENT!"
			exit $E_PACKAGING_BINARIES_ABSENT ;;

		$E_PACKAGING_PARTNER_DIR_ABSENT)
			echo "DIRECTORY $TOP_BLD_DIR/source/build/$PARTNER NOT FOUND!"
			exit $E_PACKAGING_PARTNER_DIR_ABSENT ;;

	esac
}

##
# Step 1: COMMAND LINE VALIDATION
# This would capture various parameters passed to this script, validates them and
# sets them to required variables
##

USAGE="CORRECT USAGE: $0 <valid CVS username> [-t <thirdparty path>] [top build dir] [partner] [valid CVS branch-name] [valid CVS tag name] [build-phase] [bld-quality] [Major Version] [Minor Version] [Release Number] [CONFIG]"

[ -z "$1" ] && echo $USAGE && exit 1

##
# Setting the variables
##

CVS_USERNAME=$1
if [ "$2" = "-t" ]; then
	THIRDPARTY_PATH=$3
	PARTNER=${5:-inmage}
	TOP_BLD_DIR=${4:-`pwd`}
	CVS_BRANCH=${6:-HEAD}
	CVS_TAG=${7:-HEAD}
	BUILDPHASE=${8:-DIT}
	BUILDQUALITY=${9:-DAILY}
	shift
	MAJOR_VERSION=${9:-6}
	shift
	MINOR_VERSION=${9:-50}
	shift
	REL_NUM=${9:-1}
	shift
	CONFIG=${9:-debug}
else
	PARTNER=${3:-inmage}
	TOP_BLD_DIR=${2:-`pwd`}
	CVS_BRANCH=${4:-HEAD}
	CVS_TAG=${5:-HEAD}
	BUILDPHASE=${6:-DIT}
	BUILDQUALITY=${7:-DAILY}
	MAJOR_VERSION=${8:-6}
	MINOR_VERSION=${9:-50}
	shift
	REL_NUM=${9:-1}
        shift
        CONFIG=${9:-debug}
fi

if [ -n "$THIRDPARTY_PATH" ]; then
    THIRDPARTY_ARG="-t $THIRDPARTY_PATH"
fi

CVS_CMD="cvs co -r $CVS_BRANCH"
SUFFIX=`date +%d`_`date +%b`_`date +%H`_`date +%M`_`date +%S`
FLOG="`pwd`/$TOP_BLD_DIR/logs/FX_Build_$SUFFIX.log"

[ "$PARTNER" = "both" ] && PARTNER="inmage xiotech"
Build_Dir=`echo $TOP_BLD_DIR | sed -e "s/\///g"`

if [ ! -f build_status_${Build_Dir} ]; then
        touch build_status_${Build_Dir}
        SETUP=1 CHECKOUT=1 BRANDING=1 VERSIONING=1 BUILD=1 PACKAGING=1
else
        if grep -iq "setup" build_status_${Build_Dir} ; then
                SETUP=0
        else
                SETUP=1
        fi

        if grep -iq "checkout" build_status_${Build_Dir} ; then
                CHECKOUT=0
        else
                CHECKOUT=1
        fi

        if grep -iq "branding_${PARTNER}" build_status_${Build_Dir} ; then
                BRANDING=0
        else
                BRANDING=1
        fi

        if grep -iq "versioning_${PARTNER}" build_status_${Build_Dir} ; then
                VERSIONING=0
        else
                VERSIONING=1
        fi

        if grep -iq "build_${PARTNER}" build_status_${Build_Dir} ; then
                BUILD=0
        else
                BUILD=1
        fi

        if grep -iq "packaging_${PARTNER}" build_status_${Build_Dir} ; then
                PACKAGING=0
        else
                PACKAGING=1
        fi
fi

##
# Step 2: BUILD ENVIRONMENT SETUP
# Making top dir for build
##

if [ $SETUP -eq 1 ]; then
    echo
    echo "Setting up required directory structure..."
    echo
    ./fx_bld_env_setup.sh $TOP_BLD_DIR $CVS_USERNAME
    SETUP_EXIT_STATUS=$?
    VERIFY $SETUP_EXIT_STATUS
    echo "setup" >> build_status_${Build_Dir}
else
    echo "Build directory structure setup already done for $TOP_BLD_DIR!"
fi

##
# Step 3: CHECKOUT
# Set variables and checkout files as required on the chosen OS
##

if [ $CHECKOUT -eq 1 ] ; then
    echo
    echo "Checking out source code now..."
    echo
    ./fx_checkout.sh $CVS_USERNAME $THIRDPARTY_ARG $TOP_BLD_DIR $CVS_TAG 
    CHECKOUT_EXIT_STATUS=$?
    VERIFY $CHECKOUT_EXIT_STATUS
    echo "checkout" >> build_status_${Build_Dir}
else
    echo "Source code checkout already done for $TOP_BLD_DIR!"
fi

. ${TOP_BLD_DIR}/source/build/scripts/general/OS_details.sh

##
# Step 4: BRANDING
##

if [ $BRANDING -eq 1 ] ; then
        echo
        echo "Branding the source code and build environment variables now..."
        echo

        # Source the branding parameters script here. Working directory is source which contains the build folder
        . $TOP_BLD_DIR/source/build/branding/$PARTNER/branding_parameters.sh

        # Copy the branding_parameters.h file to host/branding
        cp $TOP_BLD_DIR/source/build/branding/$PARTNER/branding_parameters.h $TOP_BLD_DIR/source/host/branding

        echo "branding_${PARTNER}" >> build_status_${Build_Dir}
else
        echo "Source code and build environment branding already done for $TOP_BLD_DIR!"
fi

##
# Step 5: VERSIONING
# Create a version file which shall also be packaged along with the binaries
# Also update the build numbers, partner name etc, in the version.h file in <top-dir>/host/common
# This will help any future installs or upgrades
##

if [ $VERSIONING -eq 1 ] ; then
        echo
        echo "Changing version.h, generating .fx_version and .fx_build_manifest now..."
        echo

        ./fx_versioning.sh $TOP_BLD_DIR $PARTNER $CVS_BRANCH $BUILDQUALITY $BUILDPHASE $MAJOR_VERSION $MINOR_VERSION $REL_NUM
        VERSIONING_EXIT_STATUS=$?
        VERIFY $VERSIONING_EXIT_STATUS
        echo "versioning_${PARTNER}" >> build_status_${Build_Dir}
else
        echo "Generation of .fx_version and .fx_build_manifest, and change of version.h already done for $TOP_BLD_DIR!"
fi

case $OS in
   RHEL*|SLES*|OPENSUSE*|CITRIX*)
    if [ $BUILD -eq 1 ] ; then
        echo
        echo "Building and packaging the FX agent now..."
        echo
	cur_wd=`pwd`
	cd $TOP_BLD_DIR/source/host 
	# If the version is 500 change the version in the spec files and in the build_params_partner.conf file
	sed -e "s/5.50.1/${MAJOR_VERSION}.${MINOR_VERSION}.${REL_NUM}/g" ../build/scripts/FX/build_params_${PARTNER}.conf >> ../build/scripts/FX/build_params_${PARTNER}.conf.new
	mv ../build/scripts/FX/build_params_${PARTNER}.conf.new ../build/scripts/FX/build_params_${PARTNER}.conf

	# Find the version from the spec file.
	version=`cat ../build/scripts/FX/templates/${PARTNER}.spec | grep "^Version" | cut -d":" -f2 | tr -d " "`
	release=`cat ../build/scripts/FX/templates/${PARTNER}.spec | grep "^Release" | cut -d":" -f2 | tr -d " "`
	sed -e "s/$version/${MAJOR_VERSION}.${MINOR_VERSION}/" ../build/scripts/FX/templates/${PARTNER}.spec >> ../build/scripts/FX/templates/${PARTNER}.spec.new
	mv ../build/scripts/FX/templates/${PARTNER}.spec.new ../build/scripts/FX/templates/${PARTNER}.spec
	
	sed -e "s/$release/${REL_NUM}/" ../build/scripts/FX/templates/${PARTNER}.spec >> ../build/scripts/FX/templates/${PARTNER}.spec.new
	mv ../build/scripts/FX/templates/${PARTNER}.spec.new ../build/scripts/FX/templates/${PARTNER}.spec

	make clean		
	make X_VERSION_MAJOR=$MAJOR_VERSION X_VERSION_MINOR=$MINOR_VERSION X_VERSION_PHASE=$REL_NUM X_VERSION_QUALITY=$BUILDQUALITY X_VERSION_PHASE=$BUILDPHASE partner=$PARTNER debug=no verbose=yes fx_setup 
	VERIFY $?
	cd ../../binaries/FX 
	cp ../../source/host/`uname -ms | tr ' ' '_'`/setup/release/*.gz .
	VERIFY $?
	cd $cur_wd
	echo "build_${PARTNER}" >> build_status_${Build_Dir}
	echo "packaging_${PARTNER}" >> build_status_${Build_Dir}

        # Generating the symbols for FX

        if [ `uname` = "Linux" ]; then
                DATE_TIME_SUFFIX=`date +%d%b%Y`
                FX_VERSION=${MAJOR_VERSION}.${MINOR_VERSION}.${REL_NUM}

                for file in `find  $TOP_BLD_DIR/source/host -name "*.dbg"`
                do
                        BaseDir=`dirname $file`
                        BaseName=`basename $file`
                        tar --append --file=${COMPANY}_FX_${FX_VERSION}_${OS}_${BUILDPHASE}_${DATE_TIME_SUFFIX}_symbols.tar -C $BaseDir $BaseName
                done

                gzip ${COMPANY}_FX_${FX_VERSION}_${OS}_${BUILDPHASE}_${DATE_TIME_SUFFIX}_symbols.tar
                [ $? -eq 0 ] && echo "FX SYMBOL TAR FILE GENERATION IS SUCCESSFUL FOR $PARTNER on ${OS}..."

                mv ${COMPANY}_FX_${FX_VERSION}_${OS}_${BUILDPHASE}_${DATE_TIME_SUFFIX}_symbols.tar.gz $TOP_BLD_DIR/binaries/FX
                [ $? -eq 0 ] && echo "Copied symbol tar to $TOP_BLD_DIR/binaries/FX..."

        fi

    else
        echo "Build and Packaging already done for $TOP_BLD_DIR!"
    fi

	;;

*)

##
# Step 6: BUILD
# Default argument is 'all' (i.e. svfrd, unregagent). If you want 
# selective build, pass comma-separated values 
# e.g. ./build_common.sh svfrd,unregagent
##

if [ $BUILD -eq 1 ] ; then
    echo
    echo "Building the FX agent now..."
    echo
    ./fx_bld.sh $TOP_BLD_DIR all $CONFIG
    BUILD_EXIT_STATUS=$?	
    VERIFY $BUILD_EXIT_STATUS
    echo "build_${PARTNER}" >> build_status_${Build_Dir}
else
    echo "Build already done for $TOP_BLD_DIR!"
fi

##
# Step 7: PACKAGING
# The packaging script replaces a lot of placeholder variables in the templates with some values at packaging time
# and leaves some others to be replaced at install time. It then copies all the required files into a temporary dir
# and creates an RPM or a TAR based on whether the operating system is Linux or otherwise.
##

if [ $PACKAGING -eq 1 ] ; then
    echo
    echo "Packaging the FX agent now..."
    echo
    ./fx_packaging.sh $PARTNER $TOP_BLD_DIR/binaries/FX $BUILDPHASE $CONFIG
    PACKAGING_EXIT_STATUS=$?
    VERIFY $PACKAGING_EXIT_STATUS
    echo "packaging_${PARTNER}" >> build_status_${Build_Dir}
else
    echo "Packaging already done for $TOP_BLD_DIR!"
fi
	;;
esac
