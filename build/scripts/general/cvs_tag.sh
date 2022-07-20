#!/bin/sh

BRANCHES="INT_BRANCH REL_BRANCH INTERIM_REL_BRANCH"
TODAY_DAY=`date +%b_%d_%Y`

# Tag host modules for all applicable branches
echo > ./host_status

for BRANCH_NAME in $BRANCHES
do
	MODULES="admin build host server Solutions"
	case $BRANCH_NAME in
        INT_BRANCH)
                BRANCH_CODE=INT_BRANCH
                QUALITY=DAILY
                PHASE=DIT
                ;;
        REL_BRANCH)
                BRANCH_CODE=REL_BRANCH
                QUALITY=RELEASE
                PHASE=GA
                ;;
        INTERIM_REL_BRANCH)
                BRANCH_CODE=INTERIM_REL_BRANCH
                QUALITY=RELEASE
                PHASE=GA
                ;;
	esac

	# Export the source CVSROOT
	export CVSROOT=:pserver:vegogine@10.150.24.11:/src
	TODAY_TAG=${QUALITY}_${BRANCH_CODE}_${PHASE}_${TODAY_DAY}
	echo "$BRANCH_NAME Today's Tag		: ${TODAY_TAG}"

	echo "Cmd: cvs rtag -F -r ${BRANCH_NAME} ${TODAY_TAG} ${MODULES}"
	cvs rtag -F -r ${BRANCH_NAME} ${TODAY_TAG} ${MODULES}
	if [ $? -eq 0 ]; then
		echo "Applied ${TODAY_TAG} successfully! ..."
                echo "Source Tagging     : ${BRANCH_NAME} - ${TODAY_TAG} - Successful" >> ./host_status
	else
		echo "Could not apply ${TODAY_TAG}. Tagging failed..."
		echo "Source Tagging     : ${BRANCH_NAME} - ${TODAY_TAG} - Failed" >> ./host_status
	fi
	echo
	echo

	echo >> ./host_status
	echo
	echo
done

grep -q "Failed" ./host_status 
if [ $? -eq 0 ]; then
	RESULT="FAILED"
else
	RESULT="SUCCESSFUL"
fi 

# Send mail to inmiet@microsoft.com about the host tagging status
mail -s "SOURCE TAGGING : ${RESULT}" inmiet@microsoft.com <<DATA
`cat -v ./host_status`
DATA


# Tag thirdparty modules for all applicable branches
echo > ./tp_status

BRANCHES="INT_BRANCH REL_BRANCH INTERIM_REL_BRANCH"
TODAY_DAY=`date +%b_%d_%Y`

for BRANCH_NAME in $BRANCHES
do
        case $BRANCH_NAME in
        INT_BRANCH)
                BRANCH_CODE=INT_BRANCH
                QUALITY=DAILY
                PHASE=DIT
                ;;
        REL_BRANCH)
                BRANCH_CODE=REL_BRANCH
                QUALITY=RELEASE
                PHASE=GA
                ;;
        INTERIM_REL_BRANCH)
                BRANCH_CODE=INTERIM_REL_BRANCH
                QUALITY=RELEASE
                PHASE=GA
                ;;
        esac

        # Export the Thirpdarty CVSROOT
        export CVSROOT=:pserver:vegogine@10.150.24.11:/thirdparty

	TODAY_TAG=${QUALITY}_${BRANCH_CODE}_${PHASE}_${TODAY_DAY}
	echo "$BRANCH_NAME Today's Tag		: ${TODAY_TAG}"

        echo "Cmd: cvs rtag -F -r ${BRANCH_NAME} ${TODAY_TAG} thirdparty"
        cvs rtag -F -r ${BRANCH_NAME} ${TODAY_TAG} thirdparty
        if [ $? -eq 0 ]; then
                echo "Applied ${TODAY_TAG} successfully! ..."
                echo "Thirdparty Tagging : ${BRANCH_NAME} - ${TODAY_TAG} - Successful" >> ./tp_status
        else
                echo "Could not apply ${TODAY_TAG}. Tagging failed..."
                echo "Thirdparty Tagging : ${BRANCH_NAME} - ${TODAY_TAG} - Failed" >> ./tp_status
        fi
	echo >> ./tp_status
        echo
        echo
done

grep -q "Failed" ./tp_status
if [ $? -eq 0 ]; then
        RESULT="FAILED"
else
        RESULT="SUCCESSFUL"
fi

# Send mail to inmiet@microsoft.com about the thirdparty status
mail -s "THIRDPARTY TAGGING : ${RESULT}" inmiet@microsoft.com <<DATA
`cat -v ./tp_status`
DATA
