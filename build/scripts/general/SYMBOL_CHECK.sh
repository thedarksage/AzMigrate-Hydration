##################
# DEFAULT VALUES
##################

DATE=`date '+%d%h%Y'`
CALLING_SCRIPT="$1"
MAIL_ID="dl-build-engineering@inmage.net"
WRK_DIR=`pwd`

##############
# VALIDATIONS
##############

GDB=`which gdb`

if [ -z "$CALLING_SCRIPT" ] || [ -z "$OS" ] || [ -z "$GDB" ]; then
	echo
        echo "Either all the arguments are not defined or GDB is not installed in the setup ..."
	echo "Cannot proceed with the symbol checking ..."
	echo
        exit 1
fi

###########################
# Start the sendmail
###########################

case $OS in
	SLES*|OPENSUSE*)
		PS=`pgrep -f /usr/lib/postfix/master`
		if [ -z "$PS" ] ; then
			/etc/init.d/postfix start
		fi
	;;
	*)
		PS=`pgrep sendmail`
		if [ -z "$PS" ] ; then
			/etc/init.d/sendmail start
		fi
	;;
esac

#################################
# Find the machine Architecture
#################################

case `uname -m` in
        i586|i686)
                ARCH=Linux_i686
        ;;
         x86_64)
                ARCH=Linux_x86_64
	;;
esac

########################
# Cleaning the gdb log
########################

Clean_Up()
{
  BUILD_TYPE=$1

  if [ -f ${BIN_DIR}/GDB_${BUILD_TYPE}_${OS}.log ]; then
  	rm -f ${BIN_DIR}/GDB_${BUILD_TYPE}_${OS}.log
  fi
}

###############################
# Check if the symbol tar or 
# the build exists or not
###############################

Pre_Check()
{
  if [ -z "$SYMBOL_TAR_FILE" ] || [ -z "$BUILD_TAR_FILE" ]; then
	echo
	echo "Either the symbol file or the build tar is missing ..."
	echo "Cannot proceed with the symbol verification . Aborting ..."
	exit 1
  fi
}

########################################
# Execute the GDB here
# Check if the gdb is matching 
# with the corresponding binary or not 
########################################

FUNC_GDB_TEST()
{
  BINARY_FILE=$1
  GDB_FILE=$2
  BUILD_TYPE=$3
  NO_SYMS_FOUND=""
  if [ `uname -s` = "SunOS" ] ;then
	GREP='/usr/xpg4/bin/grep'
  else
	GREP=`which grep`
  fi

  gdb ${BINARY_FILE} --symbols=${GDB_FILE} >gdb.log<<DATA
q
DATA

  if $GREP -q 'no debugging symbols found' gdb.log  ; then
  	NO_SYMS_FOUND="$NO_SYMS_FOUND $file"
  fi

  echo "gdb ${BINARY_FILE} --symbols=${GDB_FILE}" >>${BIN_DIR}/GDB_${BUILD_TYPE}_${OS}.log
  cat gdb.log >>${BIN_DIR}/GDB_${BUILD_TYPE}_${OS}.log
  echo >>${BIN_DIR}/GDB_${BUILD_TYPE}_${OS}.log

  if [ -n "$NO_SYMS_FOUND" ] ; then
  	mail -s "SYMBOL VERIFICATION : Symbol Verification on $OS for ${BINARY_FILE} --- FAILED" $MAIL_ID <<EOF
EOF
  fi
}

###############################
# Verifying the symbols for FX
###############################

FUNC_SYM_FX()
{
  BUILD_TYPE=FX

  Clean_Up $BUILD_TYPE

  if [ "${OS}" = "RHEL6-64" ]; then
      OS="RHEL6U2-64"
  fi

  SYMBOL_TAR_FILE=`ls -1 $BIN_DIR/InMage_UA_${MAJOR_VERSION}.${MINOR_VERSION}*${OS}*symbols.tar.gz`

  BUILD_TAR_FILE=`ls -1 $BIN_DIR/InMage_FX_${MAJOR_VERSION}.${MINOR_VERSION}*release.tar.gz`

  Pre_Check

  tar --wildcards -xzvf ${BUILD_TAR_FILE} "InMageFx-*"
  FX_TAR_NAME=`ls -1 InMageFx-*`

  rpm2cpio "$FX_TAR_NAME" | cpio -ivd
  cd $SYM_DIR/InMage/Fx

  tar -xzvf ${SYMBOL_TAR_FILE}
  CUR_DIR=`pwd`

  for symfile in `find . -name "*.dbg"`
  do
	DBG_DIR=`dirname $symfile`
	cd $DBG_DIR
	binfile=`basename $symfile`
	binfile=`ls $binfile | awk -F'.' '{ print $1 }'`

        cp $SYM_DIR/InMage/Fx/$binfile .
        FUNC_GDB_TEST $binfile ${binfile}.dbg $BUILD_TYPE
        cd $CUR_DIR	
  done
}

##########################################
# Check for Solaris VX Symbols
# This includes Pushinstall client also  
##########################################

FUNC_SYM_VX()
{
  BUILD_TYPE=VX

  Clean_Up $BUILD_TYPE

  SYMBOL_TAR_FILE=`ls -1 $BIN_DIR/InMage_VX_*symbols.tar.gz`

  BUILD_TAR_FILE=`ls -1 $BIN_DIR/InMage_VX_*release.tar.gz`

  Pre_Check

  cp $SYMBOL_TAR_FILE $BUILD_TAR_FILE .
  gunzip *.tar.gz
 	
  SYMBOL_TAR_FILE=`ls -1 InMage_VX_*${OS}*symbols.tar`
  BUILD_TAR_FILE=`ls -1 InMage_VX_*release.tar`

  mkdir -p $SYM_DIR/InMage/Vx
  cd $SYM_DIR/InMage/Vx

  tar -xvf  ${SYM_DIR}/$BUILD_TAR_FILE 
  tar -xvf  InMage_VX_*.tar bin1
  cd bin1

  tar -xvf ${SYM_DIR}/${SYMBOL_TAR_FILE}
  CUR_DIR=`pwd`

  for symfile in `find $CUR_DIR -name "*.dbg"`
   do
	DBG_DIR=`dirname $symfile`
	binfile=`basename $symfile`
	binfile=`ls $binfile | awk -F'.' '{ print $1 }'`
	cd $DBG_DIR

	if [ "$binfile" = "pushinstallclient" ]; then
		cp /BUILDS/SCRIPTS/head_daily_builds/release/pushinstallclient .
		FUNC_GDB_TEST $binfile ${binfile}.dbg $BUILD_TYPE
		cd $CUR_DIR
		continue
	fi

	cp $SYM_DIR/InMage/Vx/bin1/$binfile .
	FUNC_GDB_TEST $binfile ${binfile}.dbg $BUILD_TYPE 
	cd $CUR_DIR 
	continue
  done
}


##########################################
# Check for Solaris FX Symbols only
##########################################

FUNC_Unix_SYM_FX()
{
  BUILD_TYPE=FX

  Clean_Up $BUILD_TYPE

  SYMBOL_TAR_FILE=`ls -1 $BIN_DIR/InMage_FX_*symbols.tar.gz`

  BUILD_TAR_FILE=`ls -1 $BIN_DIR/InMage_FX_*release.tar.gz`

  Pre_Check

  cp $SYMBOL_TAR_FILE $BUILD_TAR_FILE .
  gunzip *.tar.gz

  SYMBOL_TAR_FILE=`ls -1 InMage_FX_*${OS}*symbols.tar`
  BUILD_TAR_FILE=`ls -1 InMage_FX_*release.tar`

  mkdir -p $SYM_DIR/InMage/Fx
  cd $SYM_DIR/InMage/Fx

  tar -xvf  ${SYM_DIR}/$BUILD_TAR_FILE
  tar -xvf  InMage_FX_*.tar 

  tar -xvf ${SYM_DIR}/${SYMBOL_TAR_FILE}
  CUR_DIR=`pwd`

  for symfile in `find $CUR_DIR -name "*.dbg"`
   do
        DBG_DIR=`dirname $symfile`
        binfile=`basename $symfile`
        binfile=`ls $binfile | awk -F'.' '{ print $1 }'`
        cd $DBG_DIR

        cp $SYM_DIR/InMage/Fx/$binfile .
        FUNC_GDB_TEST $binfile ${binfile}.dbg $BUILD_TYPE
        cd $CUR_DIR
        continue
  done
}

##########################################
# Do not keep quotes in DEBIAN or UBUNTU
# While extracting the FX or VX tars
##########################################

FUNC_SYM_UA()
{
  BUILD_TYPE=UA

  Clean_Up $BUILD_TYPE

  SYMBOL_TAR_FILE=`ls -1 $BIN_DIR/InMage_UA_${MAJOR_VERSION}.${MINOR_VERSION}*${OS}*symbols.tar.gz`

  BUILD_TAR_FILE=`ls -1 $BIN_DIR/InMage_UA_${MAJOR_VERSION}.${MINOR_VERSION}*release.tar.gz`

  Pre_Check

  case $OS in
        DEBIAN*|UBUNTU*)
                mkdir -p InMage/Fx InMage/Vx

                cd $SYM_DIR/InMage/Fx

                tar --wildcards -xzvf $BUILD_TAR_FILE "InMage_FX_*.tar"
                tar --wildcards -xvf InMage_FX_*.tar

                cd $SYM_DIR/InMage/Vx

                tar --wildcards -xzvf $BUILD_TAR_FILE "InMage_VX_*.tar"
                tar --wildcards -xvf InMage_VX_*.tar

                tar -xzvf ${SYMBOL_TAR_FILE}
                LIST=`find -name *.dbg | egrep  '(storagefailover|unlockVolumes|sendquit|pushinstalld)'`

                rm $LIST

        ;;
              *)
                tar --wildcards -xzvf ${BUILD_TAR_FILE} 'InMageVx-*'

                VX_TAR_NAME=`ls -1 InMageVx-*`
                rpm2cpio "$VX_TAR_NAME" | cpio -ivd

                tar --wildcards -xzvf ${BUILD_TAR_FILE} 'InMageFx-*'

                FX_TAR_NAME=`ls -1 InMageFx-*`
                rpm2cpio "$FX_TAR_NAME" | cpio -ivd

                cd $SYM_DIR/InMage/Vx/bin

                tar -xzvf ${SYMBOL_TAR_FILE}
        ;;
  esac
  CUR_DIR=`pwd`

  for symfile in `find . -name *.dbg`
  do
        DBG_DIR=`dirname $symfile`
        cd $DBG_DIR
        binfile=`ls | awk -F'.' '{ print $1 }'`

	if [ "$binfile" = "pushinstallclient" ]; then
		cd $CUR_DIR
		continue
	fi

	if [ "$binfile" = "fabricagent" ] ; then
	 	case $OS in
	  		RHEL5U1-64)
				cp $SYM_DIR/InMage/Vx/bin/$binfile .
				FUNC_GDB_TEST $binfile ${binfile}.dbg $BUILD_TYPE 
                		cd $CUR_DIR 
                		continue
	   		;;
			*)
				cd $CUR_DIR 
                		continue
	 	esac
	fi

	if [ -f $SYM_DIR/InMage/Vx/bin/$binfile ] ; then
	        cp $SYM_DIR/InMage/Vx/bin/$binfile .
	elif [ -f $SYM_DIR/InMage/Fx/$binfile ] ; then
		cp $SYM_DIR/InMage/Fx/$binfile .
	elif [ -f $SYM_DIR/InMage/Fx/transport/$binfile ] ; then
		cp $SYM_DIR/InMage/Fx/transport/$binfile .
	else
	 	mail -s "The corresponding binary $binfile is not found for $OS." $MAIL_ID <<EOF
EOF
	fi

	FUNC_GDB_TEST $binfile ${binfile}.dbg $BUILD_TYPE
	cd $CUR_DIR
done
}

###############################
# Verifying the symbols for CX
###############################

FUNC_SYM_CX()
{
  BUILD_TYPE=CX

  Clean_Up $BUILD_TYPE

  if [ "${OS}" = "RHEL6-64" ]; then
      OS="RHEL6U2-64"
  fi

  SYMBOL_TAR_FILE=`ls -1tr $BIN_DIR/SYMBOLS/InMage_Scout_CX_${MAJOR_VERSION}.${MINOR_VERSION}*${OS}_*_${verDate}${verMonth}${verYear}_release_symbol.tar.gz`

  BUILD_TAR_FILE=`ls -1tr $BIN_DIR/InMage_Scout_CX_${MAJOR_VERSION}.${MINOR_VERSION}*_${OS}_*_${verDate}${verMonth}${verYear}.tar.gz`

  Pre_Check

  tar --wildcards -xzvf ${BUILD_TAR_FILE} Scout-CX-*/inmage*.rpm Scout-CX-*/pushinstalld Scout-CX-*/storagefailover
  cd Scout-CX-*
  tar -xzvf ${SYMBOL_TAR_FILE} 

  rpm2cpio inmagePS-* | cpio -ivd ./home/svsystems/bin/bwm ./home/svsystems/bin/diffdatasort ./home/svsystems/bin/inmtouch ./home/svsystems/bin/svdcheck
  rpm2cpio inmageCS-* | cpio -ivd ./home/svsystems/bin/inmlicense ./home/svsystems/bin/scheduler

  mv ./home/svsystems/bin/bwm ./home/svsystems/bin/diffdatasort ./home/svsystems/bin/inmtouch  ./home/svsystems/bin/svdcheck ./home/svsystems/bin/inmlicense ./home/svsystems/bin/scheduler .

  for symfile in `ls -1 *.dbg`
  do
 	binfile=`ls $symfile | awk -F'.' '{ print $1 }'`
	mkdir ${binfile}_dir
	mv $binfile ${binfile}.dbg ${binfile}_dir
	cd ${binfile}_dir
        FUNC_GDB_TEST $binfile ${binfile}.dbg $BUILD_TYPE
	cd -
  done
}

################################################
# Verifying the symbols for Push Install Client
# This will not be verified as part of the UA
################################################

FUNC_SYM_PUSH()
{
  BUILD_TYPE=PUSH

  Clean_Up $BUILD_TYPE

  SYMBOL_TAR_FILE=`ls -1 $BIN_DIR/SYMBOLS/InMage_pushinstallclient_${MAJOR_VERSION}.${MINOR_VERSION}*${OS}_*release_symbol.tar.gz`

  BUILD_TAR_FILE=`ls -1 $BIN_DIR/${OS}_pushinstallclient.tar.gz`

  Pre_Check

  tar -xzvf $BUILD_TAR_FILE
  tar -xzvf $SYMBOL_TAR_FILE

  for symfile in `ls -1 *.dbg`
  do
        binfile=`ls $symfile | awk -F'.' '{ print $1 }'`
        FUNC_GDB_TEST $binfile ${binfile}.dbg $BUILD_TYPE
  done
}

###################
# MAIN ENTRY POINT
###################

##################################
# Find the Calling script and 
# Execute the gdb test accordingly
#################################

case "$CALLING_SCRIPT" in

	*UA*.sh)

		case $OS in
                        SLES9-32|SLES9-64|RHEL3-32)
                                BIN_DIR=/BUILDS/$BRANCH/BINARIES/UNIFIED

				if [ -d "$BIN_DIR/SYM_CHK" ] ; then
                                        rm -rf "$BIN_DIR/SYM_CHK"
                                fi

				mkdir -p $BIN_DIR/SYM_CHK
                                cd $BIN_DIR/SYM_CHK
                                SYM_DIR=`pwd`
                                FUNC_SYM_FX

				#Clean up for Linux FX builds after use
				if [ -d "$BIN_DIR/SYM_CHK" ] ; then
					cd $WRK_DIR
					rm -rf "$BIN_DIR/SYM_CHK"
				fi

                                BIN_DIR="/BUILDS/$BRANCH/BINARIES/PUSH"

                                if [ -d "$BIN_DIR/SYM_CHK" ] ; then
                                                 rm -rf "$BIN_DIR/SYM_CHK"
                                fi

                                mkdir -p $BIN_DIR/SYM_CHK
                                cd $BIN_DIR/SYM_CHK
                                FUNC_SYM_PUSH

				#Clean up for Linux push builds after use
				if [ -d "$BIN_DIR/SYM_CHK" ] ; then
					cd $WRK_DIR
					rm -rf "$BIN_DIR/SYM_CHK"
				fi
				
			;;
			*Solaris*)
				# For VX and Push
				BIN_DIR="/BUILDS/$BRANCH/BINARIES/UNIFIED"

				if [ -d "$BIN_DIR/SYM_CHK" ] ; then
					rm -rf "$BIN_DIR/SYM_CHK"
				fi
				mkdir -p $BIN_DIR/SYM_CHK

				cd $BIN_DIR/SYM_CHK
				SYM_DIR=`pwd`
				FUNC_Unix_SYM_FX
				cd $BIN_DIR/SYM_CHK
				FUNC_SYM_VX
				#Clean up for Solaris platforms after use
				if [ -d "$BIN_DIR/SYM_CHK" ] ; then
					cd $WRK_DIR
                                	rm -rf "$BIN_DIR/SYM_CHK"
				fi
			;;
			*)
				BIN_DIR="/BUILDS/$BRANCH/BINARIES/UNIFIED"

				if [ -d "$BIN_DIR/SYM_CHK" ] ; then
					rm -rf "$BIN_DIR/SYM_CHK"
				fi

				mkdir -p $BIN_DIR/SYM_CHK
				cd $BIN_DIR/SYM_CHK
				SYM_DIR=`pwd`
                                FUNC_SYM_UA

				#Clean up for Linux UA builds after us
				if [ -d "$BIN_DIR/SYM_CHK" ] ; then
					cd $WRK_DIR
					rm -rf "$BIN_DIR/SYM_CHK"
				fi

				BIN_DIR="/BUILDS/$BRANCH/BINARIES/PUSH"

                                if [ -d "$BIN_DIR/SYM_CHK" ] ; then
					 cd $WRK_DIR
                                         rm -rf "$BIN_DIR/SYM_CHK"
                                fi

                                mkdir -p $BIN_DIR/SYM_CHK
                                cd $BIN_DIR/SYM_CHK
				FUNC_SYM_PUSH

				#Clean up for Linux push builds after use
				if [ -d "$BIN_DIR/SYM_CHK" ] ; then
					cd $WRK_DIR
					rm -rf "$BIN_DIR/SYM_CHK"
				fi

                esac
	;;
	*CX*.sh)
		if [ -d "$BIN_DIR/SYM_CHK" ] ; then
                        rm -rf "$BIN_DIR/SYM_CHK"
                fi

		mkdir -p $BIN_DIR/SYM_CHK
		cd $BIN_DIR/SYM_CHK
		FUNC_SYM_CX			

		#Clean up for Linux CX builds after use
		if [ -d "$BIN_DIR/SYM_CHK" ] ; then
			cd $WRK_DIR
			rm -rf "$BIN_DIR/SYM_CHK"
		fi
	;;

	*)
		echo
		echo "Either the build type is unknown or there is no symbol support for this type of build"
		echo
		exit 1
	;;	
esac
