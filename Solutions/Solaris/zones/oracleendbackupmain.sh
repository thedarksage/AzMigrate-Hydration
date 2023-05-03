#!/bin/ksh

        INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`
        if [ -z "$INMAGE_DIR" ]; then
            echo "Vx agent installation not found."
            exit 1
        fi

        LOGDIR=$INMAGE_DIR/failover_data
        if [ ! -d "$LOGDIR" ]; then
            mkdir -p $LOGDIR
            if [ ! -d "$LOGDIR" ]; then
                echo "Error: $LOGDIR could not be created"
                exit 1
            fi
        fi

        TEMP_FILE="$LOGDIR/.files"
        SID_FILE="$LOGDIR/instnames"

        ps -ef | grep ora_pmon_ | grep -v grep > /dev/null
        if [ $? -ne 0 ]; then
            echo "No oracle database instance found"
            exit 0
        fi

        ps -ef | grep ora_pmon_ | grep -v grep | awk '{ print $NF}'| awk -F"_" '{ print $NF}' >$SID_FILE
        if [ ! -f $SID_FILE ]; then
            echo "Error: database instance could not be detected"
            exit 1
        fi

        ORA_USER=`ps -ef | grep ora_pmon_ | grep -v grep | head -1 | awk '{ print $1 }'`
        if [ -z "$ORA_USER" ]; then
            echo "Error: database user id not found"
            exit 1
        fi

        cat $SID_FILE | while read x
        do

            ORACLE_SID=$x
            insPid=`ps -ef | grep -w ora_pmon_${ORACLE_SID} | grep -v grep | awk '{print $2}'`
            pargs -e $insPid | grep ORACLE_HOME > /dev/null
            if [ $? -ne 0 ]; then
                echo "ERROR: Failed to find the ORACLE_HOME for $ORACLE_SID"
                continue
            fi

            ORACLE_HOME=`pargs -e $insPid | grep ORACLE_HOME | awk -F= '{print $2}'`
            PATH=$PATH:$ORACLE_HOME/bin
            export ORACLE_HOME ORACLE_SID PATH

            echo "Running end backup command for $ORACLE_SID"

            su - $ORA_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S ' /as sysdba' <<EOF >$TEMP_FILE
            alter database end backup;
EOF"
        done

