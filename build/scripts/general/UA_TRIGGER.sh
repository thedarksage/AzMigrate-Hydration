####################
# DEFAULT VARIABLES
####################

TOP_BLD_DIR=${1:?"Enter the path to the TOP BUILD directory!"}
PARTNER=${2:-inmage}
GIT_BRANCH=${3:-develop}
GIT_TAG=${4:-develop}
BUILD_QUALITY=${5:-RELEASE}
BUILD_PHASE=${6:-GA}
MAJOR_VERSION=${7:-9}
MINOR_VERSION=${8:-50}
PATCH_SET_VERSION=${9:-0}
shift
PATCH_VERSION=${9:-0}
shift
AGENT_TYPE=${9:-both}
shift
CONFIGURATION=${9:-both}
shift
FTP_BUILD=${9:-no}
shift
TIER_NUMBER=${9:-tier1}

BINARIES_PATH_FOR_UA=/BUILDS/${GIT_BRANCH}/BINARIES/UNIFIED

X_ARCH=`uname -ms | sed "s/ /_/g"`

if [ "$FTP_BUILD" = "yes" ]; then
    FTP_PATH="/DailyBuilds/Daily_Builds/${MAJOR_VERSION}.${MINOR_VERSION}"
fi

CWD=`pwd`
MAIL_ID="inmiet@microsoft.com"
date_suffix=`date +%e_%b_%Y | awk '{print $1}'`
SCP_SERVER="10.150.24.13"

Before_Build_Tasks()
{
	if [ ! -d ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source ]; then
		rm -rf ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build
		mkdir -p ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source
	fi

	cd ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source/

	# Update repo if it exists already, otherwise clone it.
	if [ -d InMage-Azure-SiteRecovery ]; then
                echo "InMage-Azure-SiteRecovery repo already exists."
		cd InMage-Azure-SiteRecovery
        	# Checkout code from appropriate branch, update code and remove any local files.
	        git checkout ${GIT_BRANCH} && git fetch origin && git reset --hard origin/${GIT_BRANCH} && git clean -xdf
       		if [ $? -eq 0 ]; then
			echo "Successfully checked out code from ${GIT_BRANCH} and removed local files."
        	else
			echo "Failed to check out code from ${GIT_BRANCH} and remove local files."
                	exit 1
        	fi
	else
                echo "InMage-Azure-SiteRecovery repo does not exist."
		git clone -b ${GIT_BRANCH} --single-branch msazure@vs-ssh.visualstudio.com:v3/msazure/One/InMage-Azure-SiteRecovery
        	if [ $? -eq 0 ]; then
                	echo "Successfully cloned InMage-Azure-SiteRecovery git repo and branch ${GIT_BRANCH}."
                	cd InMage-Azure-SiteRecovery
        	else
                	echo "Failed to clone InMage-Azure-SiteRecovery git repo and branch ${GIT_BRANCH}."
                	exit 1
        	fi
	fi

	. ./build/scripts/general/OS_details.sh

	if [ "$CONFIGURATION" = "both" ]; then
		CONFIGURATION='release debug'
	fi

	chmod -R +x build/scripts/unifiedagent
	cd build/scripts/unifiedagent

	CLEAN_REPO="no"
	./checkout.sh ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build $GIT_TAG $CLEAN_REPO
	if [ $? -ne 0 ]; then
		echo "Failed to setup InMage-Azure-SiteRecovery git repo."
		exit 1
	fi

	Build_Dir=`echo ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build | sed -e "s/\///g"`

	if [ -e build_status_${Build_Dir} ]; then
		rm -f build_status_${Build_Dir}
	fi
	touch build_status_${Build_Dir}
	echo "checkout" >> build_status_${Build_Dir}

	mkdir -p ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/
}

# Package Azure Recovery Utils
Package_AzureRecoveryTools()
{
    case $OS in
        UBUNTU-16.04-64)
            if [ "${CONFIGURATION}" = "release" ]; then            
                CUR_DIR=`pwd`

                rm -rf ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/AzureRecoveryTools

                if [ ! -d ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/AzureRecoveryTools ] || [ ! -d ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/AzureRecoveryTools/SYMBOLS ]; then 
                    mkdir -p ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/AzureRecoveryTools ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/AzureRecoveryTools/SYMBOLS
                fi                

                if  [ -e ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source/InMage-Azure-SiteRecovery/host/${X_ARCH}/AzureRecoveryUtil/release/AzureRecoveryUtil ]; then
                    mv ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source/InMage-Azure-SiteRecovery/host/${X_ARCH}/AzureRecoveryUtil/release/AzureRecoveryUtil ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/AzureRecoveryTools
                    cp ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source/InMage-Azure-SiteRecovery/host/AzureRecoveryUtil/Scripts/linux/{*.sh,network,CentOS-Base.repo} ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/AzureRecoveryTools
					cp ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source/InMage-Azure-SiteRecovery/thirdparty/setuptools-31.1.1/{*.py,*.zip} ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/AzureRecoveryTools
                    mv ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source/InMage-Azure-SiteRecovery/host/${X_ARCH}/AzureRecoveryUtil/release/AzureRecoveryUtil.dbg ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/AzureRecoveryTools/SYMBOLS
                    
                    cd ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/AzureRecoveryTools/SYMBOLS	
                    tar cvfz InMage_AzureRecoveryUtil_${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_SET_VERSION}.${PATCH_VERSION}_${OS}_${BUILD_PHASE}_`date +%d``date +%b``date +%Y`_release_symbol.tar.gz AzureRecoveryUtil.dbg
                    cd ${CUR_DIR}
               else
                    echo "AzureRecoveryUtil binary is not available."
                    cd ${CUR_DIR}
                    return 1                
               fi
          fi
     ;;
     esac
}


Package_Push_Client()
{
    if [ "${CONFIGURATION}" = "release" ]; then
        CUR_DIR=`pwd`

        rm -rf ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/PUSH

        if [ ! -d ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/PUSH ] || [ ! -d ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/PUSH/SYMBOLS ]; then
            mkdir -p ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/PUSH ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/PUSH/SYMBOLS
        fi

        if [ -e ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source/InMage-Azure-SiteRecovery/host/${X_ARCH}/pushInstallerCli/release/pushinstallclient ]; then
            mv ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source/InMage-Azure-SiteRecovery/host/${X_ARCH}/pushInstallerCli/release/pushinstallclient ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/PUSH
            mv ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source/InMage-Azure-SiteRecovery/host/${X_ARCH}/pushInstallerCli/release/pushinstallclient.dbg ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/PUSH/SYMBOLS

            cd ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/PUSH
            tar cvfz ${OS}_pushinstallclient.tar.gz pushinstallclient
            cp ${OS}_pushinstallclient.tar.gz ${OS}_pushinstallclient_${MAJOR_VERSION}.${MINOR_VERSION}.tar.gz
            cd ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/PUSH/SYMBOLS	
            tar cvfz InMage_pushinstallclient_${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_SET_VERSION}.${PATCH_VERSION}_${OS}_${BUILD_PHASE}_`date +%d``date +%b``date +%Y`_release_symbol.tar.gz pushinstallclient.dbg
            cd ${CUR_DIR}
        fi
    fi
}

UA_Build()
{
    for CONFIGURATION in $CONFIGURATION
    do
        sh -x ./makefile_complete.sh -d ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build -T /BUILDS/${GIT_BRANCH}/SOURCE/thirdparty -b ${GIT_BRANCH} -B ${GIT_TAG} -p ${PARTNER} -c ${CONFIGURATION} -q ${BUILD_QUALITY} -P ${BUILD_PHASE} -M ${MAJOR_VERSION} -N ${MINOR_VERSION} -S ${PATCH_SET_VERSION} -V ${PATCH_VERSION} -A ${AGENT_TYPE} inmbuild > ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/UA_${OS}_${CONFIGURATION}.log 2>&1
			
        if [ "${CONFIGURATION}" = "release" ]; then
            Package_Push_Client
            Package_AzureRecoveryTools
            After_Build_Tasks
            Symbol_Check
            if [ "$FTP_BUILD" = "yes" ]; then
                Scp_Release 
            fi
        fi

        if [ "${CONFIGURATION}" = "debug" ]; then
            After_Build_Tasks
            if [ "$FTP_BUILD" = "yes" ]; then
                Scp_Debug
            fi
        fi
    done
}

#######################################################
# FUNCTION : After_Build_Tasks
# Copy the required binaries to 
# BINARIES path ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES
#######################################################

After_Build_Tasks()
{
    mv ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/binaries/*.tar.gz ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/
    mv ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/binaries/FX/*.tar.gz ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/
    mv ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/binaries/VX/*.tar.gz ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/

    ls -1 ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/*UA*.tar.gz
    if [ $? -ne 0 ]; then
        HOSTNAME=`hostname`
        mail -s "UA BUILD FAILURE ON $OS WITH HOSTNAME $HOSTNAME FOR ${GIT_BRANCH} FOR ${CONFIGURATION} CONFIGURATION . PLEASE CHECK THE LOGS FOR COMPLETE RESULTS" $MAIL_ID <<DATA
DATA
    fi

    rm -f ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/binaries/*.gz
    rm -f ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/binaries/FX/*
    rm -f ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/binaries/VX/*
    rm -f ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/logs/FX/*
    rm -f ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/logs/VX/*

    # Generate OS specific agent version file.
    AGENT_VERSION=`grep -w INMAGE_PRODUCT_VERSION ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source/InMage-Azure-SiteRecovery/host/common/version.h | tr ',' '.' | cut -d" " -f3 | tr -d "\n"`
    echo -n ${AGENT_VERSION} > ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/AgentVersion_${AGENT_VERSION}_${OS}.txt

    # Copy supported_kernels file if it has some content.
    if [ -s ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source/InMage-Azure-SiteRecovery/host/drivers/InVolFlt/linux/supported_kernels ] ; then
        cp -f ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source/InMage-Azure-SiteRecovery/host/drivers/InVolFlt/linux/supported_kernels /BUILDS/${GIT_TAG}/BINARIES/UNIFIED/${OS}_supported_kernels.txt
    elif [ -s ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source/InMage-Azure-SiteRecovery/host/drivers/InVolFlt/linux/sles_drivers/supported_kernels ] ; then 
        cp -f ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source/InMage-Azure-SiteRecovery/host/drivers/InVolFlt/linux/sles_drivers/supported_kernels /BUILDS/${GIT_TAG}/BINARIES/UNIFIED/${OS}_supported_kernels.txt
    fi

    # Copy AgentVersion.txt and copy OS_details.sh which will be used by A2A extenstion. Also, copy supported_kernels file.
    if [ "$OS" = "SLES12-64" ]; then
        echo -n ${AGENT_VERSION} > ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/AgentVersion.txt
        cp -f ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source/InMage-Azure-SiteRecovery/build/scripts/general/OS_details.sh ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/
        cp -f ${TOP_BLD_DIR}/${GIT_BRANCH}/daily_unified_build/source/InMage-Azure-SiteRecovery/host/drivers/InVolFlt/linux/sles_drivers/supported_kernels /BUILDS/${GIT_TAG}/BINARIES/UNIFIED/${OS}_supported_kernels.txt
    fi

}

#############################################
# FUNCTIONS : Scp_Release Scp_Debug
# Used for copying the UA builds to 10.150.24.13
#############################################
Scp_Debug()
{
    # debug builds are not used in ASR
    return 0
}

MakeDir_Remote()
{
    remotedirpath=$1
    # retry the remote command for 5 mins
    starttime=`date +%s`
    while :
    do
        ssh $SCP_SERVER "mkdir -p $remotedirpath"
        if [ $? -eq 0 ]; then
            break
        fi


        currtime=`date +%s`
        elapsed=`expr $currtime - $starttime`
        if [ $elapsed -gt 300 ]; then
            echo "Retries of mkdir failed"
            return 1
        fi

        sleep 5
        echo "retrying mkdir command"
    done
}

Copy_To_Remote()
{
    localfilespath=$1
    remotedirpath=$2

    for localfilepath in `ls $localfilespath`
    do
        # retry the remote command for 5 mins
        starttime=`date +%s`
        while :
        do
            scp $localfilepath $SCP_SERVER:$remotedirpath
            if [ $? -eq 0 ]; then
                break
            fi

            currtime=`date +%s`
            elapsed=`expr $currtime - $starttime`
            if [ $elapsed -gt 300 ]; then
                echo "Retries of scp failed"
                return 1
            fi

            sleep 5
            echo "retrying scp command"
        done
    done
}



Scp_Release()
{
    DATE_FOLDER=`date +%d_%b_%Y | awk '{print $1}'`

    MakeDir_Remote "${FTP_PATH}/HOST/${DATE_FOLDER}/UnifiedAgent_Builds/release/symbol_tars"
    MakeDir_Remote "${FTP_PATH}/HOST/${DATE_FOLDER}/UnifiedAgent_Builds/release/logs"
    MakeDir_Remote "${FTP_PATH}/HOST/${DATE_FOLDER}/UnifiedAgent_Builds/release/supported_kernels"
    MakeDir_Remote "${FTP_PATH}/HOST/${DATE_FOLDER}/UnifiedAgent_Builds/release/agent_versions"
    MakeDir_Remote "${FTP_PATH}/HOST/${DATE_FOLDER}/PushInstallClients/symbol_tars"
    MakeDir_Remote "${FTP_PATH}/HOST/${DATE_FOLDER}/PushInstallClientsRcm"
    MakeDir_Remote "${FTP_PATH}/HOST/${DATE_FOLDER}/release/AzureRecoveryTools/Symbols"

    Copy_To_Remote ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/*UA*release.tar.gz ${FTP_PATH}/HOST/${DATE_FOLDER}/UnifiedAgent_Builds/release/
    Copy_To_Remote ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/*UA*release_symbols.tar.gz ${FTP_PATH}/HOST/${DATE_FOLDER}/UnifiedAgent_Builds/release/symbol_tars/

    Copy_To_Remote ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/OS_details.sh ${FTP_PATH}/HOST/${DATE_FOLDER}/UnifiedAgent_Builds/release/
    Copy_To_Remote ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/AgentVersion.txt ${FTP_PATH}/HOST/${DATE_FOLDER}/UnifiedAgent_Builds/release/

    #Copy_To_Remote ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/GDB*.log ${FTP_PATH}/HOST/${DATE_FOLDER}/UnifiedAgent_Builds/release/logs/
    Copy_To_Remote ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/UA*release.log ${FTP_PATH}/HOST/${DATE_FOLDER}/UnifiedAgent_Builds/release/logs/
    Copy_To_Remote ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/AgentVersion_*.txt ${FTP_PATH}/HOST/${DATE_FOLDER}/UnifiedAgent_Builds/release/agent_versions/
    Copy_To_Remote ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/*supported_kernels.txt ${FTP_PATH}/HOST/${DATE_FOLDER}/UnifiedAgent_Builds/release/supported_kernels/

    Copy_To_Remote ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/PUSH/*pushinstallclient.tar.gz ${FTP_PATH}/HOST/${DATE_FOLDER}/PushInstallClients/
    Copy_To_Remote ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/PUSH/SYMBOLS/*.tar.gz ${FTP_PATH}/HOST/${DATE_FOLDER}/PushInstallClients/symbol_tars/
    Copy_To_Remote ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/PUSH/*pushinstallclient_${MAJOR_VERSION}.${MINOR_VERSION}.tar.gz ${FTP_PATH}/HOST/${DATE_FOLDER}/PushInstallClientsRcm/

    if [ "$OS" = "UBUNTU-16.04-64" ]; then
        Copy_To_Remote "${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/AzureRecoveryTools/*.sh" ${FTP_PATH}/HOST/${DATE_FOLDER}/release/AzureRecoveryTools/
        Copy_To_Remote "${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/AzureRecoveryTools/*.py" ${FTP_PATH}/HOST/${DATE_FOLDER}/release/AzureRecoveryTools/
        Copy_To_Remote ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/AzureRecoveryTools/CentOS-Base.repo ${FTP_PATH}/HOST/${DATE_FOLDER}/release/AzureRecoveryTools/
        Copy_To_Remote ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/AzureRecoveryTools/network ${FTP_PATH}/HOST/${DATE_FOLDER}/release/AzureRecoveryTools/
        Copy_To_Remote ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/AzureRecoveryTools/AzureRecoveryUtil ${FTP_PATH}/HOST/${DATE_FOLDER}/release/AzureRecoveryTools/
        Copy_To_Remote ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/AzureRecoveryTools/SYMBOLS/*.tar.gz  ${FTP_PATH}/HOST/${DATE_FOLDER}/release/AzureRecoveryTools/Symbols/
    fi

    Copy_To_Remote ${TOP_BLD_DIR}/${GIT_BRANCH}/BINARIES/UNIFIED/UA_WRAPPER_log  ${FTP_PATH}/HOST/${DATE_FOLDER}/UnifiedAgent_Builds/release/logs/UA_${OS}_WRAPPER.log
}

#####################################
# FUNCTION : Symbol_Check
# Executing the Symbol check for all
# the linux platforms
#####################################

Symbol_Check()
{
  cd $CWD
  # cvs co -r $GIT_TAG build/scripts/general/SYMBOL_CHECK.sh
  export TOP_BLD_DIR PARTNER GIT_BRANCH GIT_TAG BUILD_QUALITY BUILD_PHASE MAJOR_VERSION MINOR_VERSION AGENT_TYPE OS
  BRANCH=$GIT_TAG
  export BRANCH
  if [ -d InMage-Azure-SiteRecovery ]; then
      sh -x InMage-Azure-SiteRecovery/build/scripts/general/SYMBOL_CHECK.sh "$0"
  else
      sh -x build/scripts/general/SYMBOL_CHECK.sh "$0"
  fi
}

#####################
# MAIN ENTRY POINT
#####################

rm -f ${BINARIES_PATH_FOR_UA}/*.tar.gz ${BINARIES_PATH_FOR_UA}/*.log ${BINARIES_PATH_FOR_UA}/*.txt

Before_Build_Tasks

UA_Build
