#################
# DEFAULT VALUES
#################

COMPONENT_TO_BE_BUILT=${1:-UA}
GIT_BRANCH=${2:-develop}
MAJOR_VERSION=${3:-9}
MINOR_VERSION=${4:-55}
PATCH_SET_VERSION=${5:-0}
PATCH_VERSION=${6:-0}
BUILD_QUALITY=${7:-RELEASE}
BUILD_PHASE=${8:-GA}
FTP_BUILD=${9:-no}
shift
CVS_UPDATE=${9:-yes}
shift
GIT_TAG=${9:-develop}
shift
TOP_DIR_FOR_UA=${9:-/BUILDS}
shift
AGENT_TYPE_FOR_UA=${9:-both}
shift
CONFIGURATION_FOR_UA=${9:-both}
shift
BINARIES_PATH_FOR_UA=${9:-/BUILDS/develop/BINARIES/UNIFIED}
shift
TIER_NUMBER=${9:-tier1}
SCRIPTS_PATH_FOR_UA=/BUILDS/SCRIPTS/develop/UNIFIED

case $COMPONENT_TO_BE_BUILT in

	UA)
		for ip in `cat build/scripts/general/iplist`
		do
		    IP=`echo ${ip} | cut -d":" -f1`
		    ssh -f ${IP} "mkdir -p ${BINARIES_PATH_FOR_UA} ${SCRIPTS_PATH_FOR_UA}"
		    sleep 10
            chmod +x build/scripts/general/${COMPONENT_TO_BE_BUILT}_TRIGGER.sh
		    scp build/scripts/general/${COMPONENT_TO_BE_BUILT}_TRIGGER.sh ${IP}:${SCRIPTS_PATH_FOR_UA}
		    ssh -f $IP "cd ${SCRIPTS_PATH_FOR_UA}; ./${COMPONENT_TO_BE_BUILT}_TRIGGER.sh ${TOP_DIR_FOR_UA} inmage ${GIT_BRANCH} ${GIT_TAG} ${BUILD_QUALITY} ${BUILD_PHASE} ${MAJOR_VERSION} ${MINOR_VERSION} ${PATCH_SET_VERSION} ${PATCH_VERSION} ${AGENT_TYPE_FOR_UA} ${CONFIGURATION_FOR_UA} ${FTP_BUILD} ${TIER_NUMBER} > ${BINARIES_PATH_FOR_UA}/UA_WRAPPER_log 2>&1 "
		done
	;;	
esac
