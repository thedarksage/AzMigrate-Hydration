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
E_VERSION_ARG_MISSING=85
E_VERSION_SCRIPT_NOT_FOR_CODE_BRANCH=86
E_VERSION_FAILURE_TO_GET_SECS_ELAPSED=88
E_VERSION_PARTNER_NOT_HANDLED=89
E_VERSION_BIN_DIR_NOT_FOUND=90
E_BUILD_VX_FAILURE=94
E_BUILD_DRIVER_BLD_FAILURE=95
E_BUILD_INMSHUTNOTIFY_FAILURE=99
E_BUILD_SVDCHECK_FAILURE=100
E_BUILD_INMSTKOPSBIN_FAILURE=101
E_BUILD_DRIVER_MACHINE_DOWN=102
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

		$E_OS_NOT_SUPPORTED)
			echo "THIS PLATFORM IS NOT SUPPORTED YET!"
			exit $E_OS_NOT_SUPPORTED ;;

		$E_SETUP_ARG_MISSING)
		   	echo "SOME OR ALL ARGUMENTS FOR vx_bld_env_setup.sh ARE MISSING!"
			exit $E_SETUP_ARG_MISSING ;;

		$E_SETUP_DIRCREATION_FAILURE)
			echo "DIRECTORY $TOP_BLD_DIR COULD NOT BE CREATED!"
			exit $E_SETUP_DIRCREATION_FAILURE ;;

		$E_SETUP_CHANGEDIR_FAILURE)
			echo "FAILED TO CHANGE DIRECTORY TO $TOP_BLD_DIR!"
			exit $E_SETUP_CHANGEDIR_FAILURE ;;

		$E_CHECKOUT_ARG_MISSING)
			echo "SOME OR ALL ARGUMENTS FOR vx_checkout.sh ARE MISSING!"
			exit $E_CHECKOUT_ARG_MISSING ;;

		$E_CHECKOUT_SOURCE_DIR_NOT_FOUND)
			echo "DIRECTORY source NOT FOUND UNDER TOP BUILD DIRECTORY!"
			exit $E_CHECKOUT_SOURCE_DIR_NOT_FOUND ;;

		$E_SCRIPT_NOT_FOR_CODE_BRANCH)
			echo "CODE BRANCH SUPPLIED AS ARGUMENT TO vx_versioning.sh IS NOT VALID FOR THIS SCRIPT!"
			exit $E_SCRIPT_NOT_FOR_CODE_BRANCH ;;

		$E_VERSION_PARTNER_NOT_HANDLED)
			echo "PARTNER SUPPLIED TO vx_versioning.sh NOT HANDLED IN THIS SCRIPT!"
			exit $E_VERSION_PARTNER_NOT_HANDLED ;;

		$E_PACKAGING_PARTNER_DIR_ABSENT)
			echo "DIRECTORY $TOP_BLD_DIR/source/build/$PARTNER NOT FOUND!"
			exit $E_PACKAGING_PARTNER_DIR_ABSENT ;;

	esac
}

usage()
{
	 echo "usage: ./complete.sh [-s <setup/checkout/build_package>] [-d <top build dir>] -T <thirdparty dirpath> [-b <branch>] [ -B <tagname-for-checkout> ] [-p <partner>] [-x <X_ARCH>] [-c <release|debug>] [-v] [-w <none|warn|error>] [-q <DAILY|RELEASE>] [-P <buildphase|DIT] [ -M <Major version> ] [ -N <Minor Version> ] [ -A <Agent Type> ] [-h] <cvs username>"
	 echo "  <cvs username> required: cvs login id used to check source"
	 echo "  -s <setup/checkout/build_package> --> Quit after the stage is finished"
	 echo "  -d <directory to build in> (optional): directory where you want the build done. default current directory"
	 echo "      $DEFAULT_TOP_BLD_DIR"
	 echo "  -b <branch> (optional): cvs branch to use for checking out source and computing build number. default $DEFAULT_CVS_BRANCH"
	 echo "  -B <tagname-for-checkout> (optional): cvs tag to use when checking out source. default $DEFAULT_GIT_TAG"
	 echo "  -p <partner> (optional): partner the build is for. default $DEFAULT_PARTNER"
	 echo "  -x <X_ARCH> (optional): X_ARCH to use for the build. default $DEFAULT_X_ARCH"
	 echo "  -c <release|debug> (optional): configuration to build. default $DEFAULT_CONFIG"
	 echo "  -q <DAILY|RELEASE> (optional): quality of the build (whether daily or release). default $DEFAULT_BLD_QUALITY"
	 echo "  -P <buildphase|DIT> (optional): phase of the build (whether DIT, BETA, etc). default $DEFAULT_BLD_PHASE"
	 echo "  -M < Major Version > default is 9"
	 echo "  -N < Minor Version > default is 0"
	 echo "  -S < Patch Set Version > default is 0"
	 echo "  -V < Patch Version > default is 0"
	 echo "  -A < Agent Type > If FX or VX or both"
	 echo "  -T <thirdparty path> required: path of thirdparty folder"
	 echo "  -v (optional): turn on verbose during build. default off"
	 echo "  -w <none|warn|error> (optional): compiler warning level to use. default $DEFAULT_WARN_LEVEL"
	 echo "               none: use compiler defaults"
	 echo "               warn: add additional warning checks to compiler"
	 echo "               error: add additional warning checks to compiler and treat warnings as errors"
	 echo "  -h (optional): display this help"
	 exit 1
}


parse_cmd_line ()
{
	 if test "$1" = "" ; then
		  echo "cvs username required"
		  usage
	 else
		  while test "$1" != "" ; do
				case $1 in				 
					 -d)
						  TOP_BLD_DIR="$2"
						  shift
						  ;;
					 -s)
						  STAGE="$2"
						  shift
						  ;;
					 -b)
						  CVS_BRANCH="$2"
						  shift
						  ;;
					 -B)
						  GIT_TAG="$2"
						  shift
						  ;;
					 -p)
						  PARTNER="$2"
						  shift
						  ;;
					 -c)
						  CONFIG="$2"
						  shift
						  ;;
					 -q)
						  BUILD_QUALITY="$2"
						  shift
						  ;;
					 -v)
						  VERBOSE="-v"
						  ;;
					 -h)
						  usage
						  ;;
					 -w)
						  WARN_LEVEL="$2"
						  shift
						  ;;
					 -x)
						  X_ARCH="$2"
						  shift
						  ;;
					 -P)
						  BUILD_PHASE="$2"
						  shift
						  ;;

					 -M)
						 MAJOR_VERSION="$2"
						 shift
						 ;;
					-N)
                                                 MINOR_VERSION="$2"
                                                 shift
                                                 ;;
					-S)
                                                 PATCH_SET_VERSION="$2"
                                                 shift
                                                 ;;

                                        -V)
                                                 PATCH_VERSION="$2"
                                                 shift
                                                 ;;
                                        -A)
                                                 AGENT_TYPE="$2"
                                                 shift
                                                 ;;

					 -T)
						  THIRDPARTY_PATH="$2"
						  if test "$THIRDPARTY_PATH" = ""; then
							echo "Enter thirdparty path"
							usage
						  fi
						  shift
						  ;;
					 -*)
						  echo "invalid option $1" 
						  usage
						  ;;                  
					 *)
						  if test "$CVS_USERNAME" != "" ; then 
							echo "invalid option $1" 
							usage
						  fi
 						  CVS_USERNAME="$1"
						  ;;
				esac
				shift
		  done
	 fi
	 
	 if test "$CVS_USERNAME" = "" ; then
		  echo "cvs username required"
		  usage
	 fi
	 
}	 

##
# DEFAULTS
# sets up inital default values that can be over ridden by command line options
# these are used that usage can display the default values even if a command line
# option has already set the value
##
DEFAULT_TOP_BLD_DIR=`pwd`
DEFAULT_CVS_BRANCH="develop"
DEFAULT_GIT_TAG="develop"
DEFAULT_PARTNER="inmage"
DEFAULT_CONFIG="release"
DEFAULT_WARN_LEVEL="none"
DEFAULT_X_ARCH=`uname -ms | sed -e s"/ /_/g"`
DEFAULT_BLD_QUALITY="DAILY"
DEFAULT_BLD_PHASE="DIT"
DEFAULT_STAGE="";
DEFAULT_MAJOR_VERSION=9
DEFAULT_MINOR_VERSION=0
DEFAULT_AGENT_TYPE=both

##
# VARIABLES
# the variables that will be used 
# set them to their defaults. note: some maybe over ridden by command line options
##
TOP_BLD_DIR="$DEFAULT_TOP_BLD_DIR"
CVS_BRANCH="$DEFAULT_CVS_BRANCH"
GIT_TAG="$DEFAULT_GIT_TAG"
PARTNER="$DEFAULT_PARTNER"
CONFIG="$DEFAULT_CONFIG"
VERBOSE=yes
WARN_LEVEL="$DEFAULT_WARN_LEVEL"
X_ARCH="$DEFAULT_X_ARCH"
SUFFIX=`date +%d`_`date +%b`_`date +%H`_`date +%M`_`date +%S`
FLOG="$TOP_BLD_DIR/logs/VX_Build_$SUFFIX.log"
CVS_USERNAME=
THIRDPARTY_PATH=
BUILD_QUALITY="$DEFAULT_BLD_QUALITY"
BUILD_PHASE="$DEFAULT_BLD_PHASE"
STAGE=$DEFAULT_STAGE
MAJOR_VERSION="$DEFAULT_MAJOR_VERSION"
MINOR_VERSION="$DEFAULT_MINOR_VERSION"
AGENT_TYPE="$DEFAULT_AGENT_TYPE"

##
# Step 1: COMMAND LINE VALIDATION
# This would capture various parameters passed to this script, validates them and
# sets them to required variables
##
parse_cmd_line $@

##
# Step 1.1: Find out how much needs to be done
# If build_status file is lying around, read it and figure out
# If file does not exist, do everything
# A value of 1 for a step's flag means the step's to be done; a value of 0 means leave that step
# Truncate all slashes in TOP_BLD_DIR so that touch doesn't fail
##

Build_Dir=`echo $TOP_BLD_DIR | sed -e "s/\///g"`

if [ ! -e build_status_${Build_Dir} ]; then
	touch build_status_${Build_Dir}
	SETUP=1 CHECKOUT=1 BUILD_PACKAGE=1
else
	if grep -iq "setup_${AGENT_TYPE}" build_status_${Build_Dir} ; then
		SETUP=0
	else
		SETUP=1
	fi

	if grep -iq "checkout" build_status_${Build_Dir} ; then
		CHECKOUT=0
	else
		CHECKOUT=1
	fi

	if grep -iq "build_package_${PARTNER}_${CONFIG}_${AGENT_TYPE}" build_status_${Build_Dir} ; then
		BUILD_PACKAGE=0
	else
		BUILD_PACKAGE=1
	fi

fi

if [ "$AGENT_TYPE" = "both" ]; then
        argument_for_make=ua_setup
elif [ "$AGENT_TYPE" = "FX" ]; then
        argument_for_make=fx_setup
elif [ "$AGENT_TYPE" = "VX" ]; then
        argument_for_make=vx_setup
fi

##
# Step 2: BUILD ENVIRONMENT SETUP
# Making top dir for build and export common required shell variables
##

if [ $SETUP -eq 1 ]; then
	echo
	echo "Setting up required directory structure..."
	echo
	./bld_env_setup.sh $TOP_BLD_DIR $AGENT_TYPE
	SETUP_EXIT_STATUS=$?
	VERIFY $SETUP_EXIT_STATUS
	echo "setup_${AGENT_TYPE}" >> build_status_${Build_Dir}
else
	echo "Build directory structure setup already done for $TOP_BLD_DIR!"
fi

if [ z"$STAGE" = z"setup" ]; then exit; fi

##
# Step 3: CHECKOUT
# Set variables and checkout files as required on the chosen OS
##

if [ $CHECKOUT -eq 1 ] ; then
	echo
	echo "Checking out source code now..."
	echo
	./checkout.sh $TOP_BLD_DIR $GIT_TAG
	CHECKOUT_EXIT_STATUS=$?
	VERIFY $CHECKOUT_EXIT_STATUS
	echo "checkout" >> build_status_${Build_Dir}
else
	echo "Source code checkout already done for $TOP_BLD_DIR!"
fi

if [ z"$STAGE" = z"checkout" ]; then exit; fi

##
# Step 6: BUILD
##

if [ $BUILD_PACKAGE -eq 1 ] ; then
	echo
	echo "Building the agent now..."
	echo
	cur_wd=`pwd`
	cd $TOP_BLD_DIR/source/InMage-Azure-SiteRecovery/host
	make clean
        
        if [ "$CONFIG" = "debug" ];then
		BUILD_CONFIG=yes
	elif [ "$CONFIG" = "release" ];then
		BUILD_CONFIG=no
	fi
	gmake $argument_for_make X_VERSION_MAJOR=$MAJOR_VERSION X_VERSION_MINOR=${MINOR_VERSION} X_PATCH_SET_VERSION=${PATCH_SET_VERSION} X_PATCH_VERSION=${PATCH_VERSION} X_VERSION_QUALITY=$BUILD_QUALITY X_VERSION_PHASE=$BUILD_PHASE debug=$BUILD_CONFIG warnlevel=$WARN_LEVEL verbose=$VERBOSE partner=$PARTNER 
        		
	BUILD_EXIT_STATUS=$?
	VERIFY $BUILD_EXIT_STATUS
	cd $cur_wd
	echo "build_package_${PARTNER}_${CONFIG}_${AGENT_TYPE}" >> build_status_${Build_Dir}

	# Copy the tar files to binaries directory under TOP_BLD_DIR
	if [ "$AGENT_TYPE" = "both" ]; then
		mv $TOP_BLD_DIR/source/InMage-Azure-SiteRecovery/host/${X_ARCH}/setup/${CONFIG}/*.tar.gz $TOP_BLD_DIR/binaries/FX
		mv $TOP_BLD_DIR/source/InMage-Azure-SiteRecovery/host/${X_ARCH}/setup_vx/${CONFIG}/*.tar.gz $TOP_BLD_DIR/binaries/VX
		mv $TOP_BLD_DIR/source/InMage-Azure-SiteRecovery/host/${X_ARCH}/setup_ua/${CONFIG}/*.tar.gz $TOP_BLD_DIR/binaries
		mv $TOP_BLD_DIR/source/InMage-Azure-SiteRecovery/host/*.tar.gz $TOP_BLD_DIR/binaries

	elif [ "$AGENT_TYPE" = "FX" ]; then
		if [ "$CONFIG" = "release" ];then
			cd $TOP_BLD_DIR/source/InMage-Azure-SiteRecovery/host
			gmake symbol_generation X_VERSION_MAJOR=$MAJOR_VERSION X_VERSION_MINOR=${MINOR_VERSION} X_VERSION_QUALITY=$BUILD_QUALITY X_VERSION_PHASE=$BUILD_PHASE debug=$BUILD_CONFIG warnlevel=$WARN_LEVEL verbose=$VERBOSE partner=$PARTNER
			cd $cur_wd
			mv $TOP_BLD_DIR/source/InMage-Azure-SiteRecovery/host/*.tar.gz $TOP_BLD_DIR/binaries
		fi
		mv $TOP_BLD_DIR/source/InMage-Azure-SiteRecovery/host/${X_ARCH}/setup/${CONFIG}/*.tar.gz $TOP_BLD_DIR/binaries/FX

	elif [ "$AGENT_TYPE" = "VX" ]; then
		if [ "$CONFIG" = "release" ];then
			cd $TOP_BLD_DIR/source/InMage-Azure-SiteRecovery/host
			gmake symbol_generation X_VERSION_MAJOR=$MAJOR_VERSION X_VERSION_MINOR=${MINOR_VERSION} X_VERSION_QUALITY=$BUILD_QUALITY X_VERSION_PHASE=$BUILD_PHASE debug=$BUILD_CONFIG warnlevel=$WARN_LEVEL verbose=$VERBOSE partner=$PARTNER
			cd $cur_wd
			mv $TOP_BLD_DIR/source/InMage-Azure-SiteRecovery/host/*.tar.gz $TOP_BLD_DIR/binaries
		fi
		mv $TOP_BLD_DIR/source/InMage-Azure-SiteRecovery/host/${X_ARCH}/setup_vx/${CONFIG}/*.tar.gz $TOP_BLD_DIR/binaries/VX
	fi
else
	echo "Build and packaging are already done for $TOP_BLD_DIR!"
fi

if [ z"$STAGE" = z"build_package" ]; then exit; fi
