#!/bin/bash

PLATFORM=`uname | tr -d "\n"`
NULL=/dev/null

Usage()
{
    echo ""
    echo "usage: $0 <action> <Oracle DB Names> <TagName> <Volumes>"
    echo ""
    echo "where action can be one of the follwing:"
    echo "issuetags             - To issue consistency tag on DB volumes from command line"
    echo "                        The volumes having the filesystem should be supplied in command line."
    echo "consistency           - To issue consistency tag on DB volumes"
    echo "                        Note: only to be used with appservice"
    echo "failover              - To issue tag and shutdown databases for failover"
    echo "                        Note: only to be used with appservice"
    echo "validate              - To validate if the tag can be issued on the database"
    echo ""
    echo "freeze                - To freeze oracle database"
    echo ""
    echo "unfreeze              - To unfreeze oracle database"
    echo ""
    return

    return
}

ParseArgs()
{
    if [ $# -ne 4 ]; then
        Usage
        exit 1
    fi

    if [ $1 != "consistency"  -a  $1 != "failover" -a $1 != "issuetags"  -a $1 != "validate" -a $1 != "freeze" -a $1 != "unfreeze"  ]; then
        Usage
        exit 1
    fi

    ACTION=$1
    DB_NAMES=$2
    TAG=$3

    if [ $1 = "validate" ]; then
        SRC_CONF_FILE=$4
    else
        VOLUMES=$4
    fi

    TEMPFILE1='/tmp/tempfile1'

    INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`

    if [ -z "$INMAGE_DIR" ]; then
        exit 1
    fi

    dirName=`dirname $0`
    orgWd=`pwd`

    cd $dirName
    SCRIPT_DIR=`pwd`
    cd $orgWd

    LOG_FILE=/var/log/oradisc.log
    touch $LOG_FILE
    chmod 640 $LOG_FILE

    isDbFound=0

    ORACLE_USER=`grep -i OracleUser $INMAGE_DIR/etc/drscout.conf | awk -F"=" '{print $2}' | sed "s/ //g"`
    if [ ! -z "$ORACLE_USER" ];then
        SQLARGS="-S '/'"
    else
        SQLARGS="-S '/ as sysdba'"
    fi

    return
}

log()
{
    echo "(`date +'%m-%d-%Y %H:%M:%S'`):   DEBUG $$ oracle_consistency:$ACTION $*" >> $LOG_FILE
    return
}


Cleanup()
{
    rm -f $tempfile > $NULL
    rm -f $tempfile1 > $NULL
}

CleanupError()
{
    rm -f $tempfile > $NULL
    rm -f $tempfile1 > $NULL
    exit 1
}

IsCShell()
{
    ORACLE_USER=$1
          
    ORA_USR_SHELL=`grep ^$ORACLE_USER /etc/passwd | grep -w $ORACLE_USER | awk -F":" '{print $NF}'`
    echo $ORA_USR_SHELL | grep csh > $NULL
    if [ $? -eq 0 ];then
        ORAENV="setenv ORACLE_SID $ORACLE_SID;setenv ORACLE_HOME $ORACLE_HOME;$ORACLE_HOME/bin/sqlplus $SQLARGS"
    else
        ORAENV="ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_SID ORACLE_HOME;$ORACLE_HOME/bin/sqlplus $SQLARGS"
    fi
}

GetDatabaseName()
{
    ORACLE_SID=$1
    ORACLE_HOME=$2
    ORACLE_USER=$3
    tempfile=/tmp/dbstate.$$
    ORACLE_DB_NAME=

    IsCShell $ORACLE_USER

    su - $ORACLE_USER -c "$ORAENV << _EOF >$tempfile
        select status from v\\\$instance;
_EOF"

    grep "OPEN" $tempfile>> /dev/null
    if [ $? -eq 0 ]
    then
        log "Database started and opened"
    else
        log "Database not started"
        rm -f $tempfile
        return
    fi

    rm -f $tempfile

    gverfile=/tmp/dbname.$$

    su - $ORACLE_USER -c "$ORAENV << _EOF > $gverfile
                   select version from v\\\$instance;
_EOF"

    grep "VERSION" $gverfile > /dev/null
    if [ $? -eq 0 ]; then
        DB_VER=`grep -v "^$" $gverfile | grep -v "VERSION" | grep -v -- "--"`
    else
        log "Database version not found for SID $ORACLE_SID"
        $ECHO "Database version not found for SID $ORACLE_SID"
        rm -f $gverfile
        return
    fi


    echo $DB_VER | awk -F"." '{print $1}' | grep 9 > /dev/null
    if [ $? -eq 0 ]; then
        ORACLE_DB_NAME=$ORACLE_SID
        return
    fi


    gdbnfile=/tmp/dbname.$$

    su - $ORACLE_USER -c "$ORAENV << _EOF > $gdbnfile
                   select db_unique_name from v\\\$database;
_EOF"

    grep "DB_UNIQUE_NAME" $gdbnfile > /dev/null
    if [ $? -eq 0 ]; then
        ORACLE_DB_NAME=`grep -v "^$" $gdbnfile | grep -v "DB_UNIQUE_NAME" | grep -v -- "--"`
    else
        log "Database name not found for SID $ORACLE_SID"
        $ECHO "Database name not found for SID $ORACLE_SID"
        rm -f $gdbnfile
        return
    fi


    if [ -z "$ORACLE_DB_NAME" ]; then
        log "Database name not found for SID $ORACLE_SID"
        $ECHO "Database name not found for SID $ORACLE_SID"
        rm -f $gdbnfile
        return
    fi

    rm -f $gdbnfile
    return
}

GetDbInfo()
{
    DB_CONF_FILES=
    ORACLE_DB_NAMES=
    dbIns=`ps -ef | grep ora_pmon | grep -v grep | awk '{ print $NF}' |  sed -e 's/ora\_pmon\_//'`
    for DB_INS in $dbIns
    do
        insPid=`ps -ef | grep -w ora_pmon_${DB_INS} | grep -v grep | awk '{print $2}'`

        if [ "$PLATFORM" = "Linux" ]; then
            ORACLE_HOME=`cat /proc/${insPid}/environ | tr '\0' '\n' | grep ORACLE_HOME | awk -F'=' '{print $2}'`
        elif [ "$PLATFORM" = "SunOS" ];then
            ORACLE_HOME=`pargs -e $insPid | grep ORACLE_HOME | awk -F= '{print $2}'`
        elif [ "$PLATFORM" = "AIX" ];then
            ps ewww $insPid | grep -v PID | grep ORACLE_HOME > /dev/null
            if [ $? -ne 0 ]; then
                log "ERROR: Failed to find the ORACLE_HOME for $DB_INS"
                echo "ERROR: Failed to find the ORACLE_HOME for $DB_INS"
                return
            fi
            ORACLE_HOME=`ps ewww $insPid | grep -v PID | awk '{for(i=2;i<=NF;i++) print $i}' | grep ORACLE_HOME | awk -F= '{print $2}'`
        fi

        if [ -z "$ORACLE_USER" ];then
            log "ORACLE_USER is not defined in drscout.conf"
            ORACLE_USER=`ps -ef | grep ora_pmon | grep $DB_INS | grep -v grep |awk '{ print $1 }'`
        fi

        GetDatabaseName $DB_INS $ORACLE_HOME $ORACLE_USER
        

        DB_CONF_FILE="/tmp/$ORACLE_DB_NAME.conf"
        if [ "$DB_NAMES" = "ALL" ]; then
            echo "OracleUser=$ORACLE_USER" > $DB_CONF_FILE
            echo "OracleHome=$ORACLE_HOME" >> $DB_CONF_FILE
            echo "OracleSID=$DB_INS" >> $DB_CONF_FILE
            echo "DBVersion=$DB_VER" >> $DB_CONF_FILE
            echo "OracleDBName=$ORACLE_DB_NAME" >> $DB_CONF_FILE
        fi

        if [ -z "$ORACLE_DB_NAMES" ]; then
            ORACLE_DB_NAMES=$ORACLE_DB_NAME
        else
            ORACLE_DB_NAMES="$ORACLE_DB_NAMES:$ORACLE_DB_NAME"
        fi 

    done

    TMP_ORACLE_DB_NAMES=$ORACLE_DB_NAMES
    ORACLE_DB_NAMES=
    for ORACLE_DB_NAME in `echo $TMP_ORACLE_DB_NAMES | sed 's/:/ /g' | awk '{for(i=1;i<=NF;i++) print $i}' | sort | uniq` 
    do
        if [ -z "$ORACLE_DB_NAMES" ]; then
            ORACLE_DB_NAMES=$ORACLE_DB_NAME
        else
            ORACLE_DB_NAMES=$ORACLE_DB_NAMES:$ORACLE_DB_NAME
        fi
    done

    if [ "$DB_NAMES" = "ALL" ]; then
        DB_NAMES=$ORACLE_DB_NAMES
    fi 

    echo $DB_NAMES | grep : > /dev/null
    if [ $? -eq 0 ]; then
        DB_NAME=`echo $DB_NAMES | awk -F: '{print $1}'`
    else
        DB_NAME=$DB_NAMES
    fi
            
    for ORACLE_DB_NAME in `echo $ORACLE_DB_NAMES | sed 's/:/ /g'` 
    do
        if [ "$DB_NAME" = "$ORACLE_DB_NAME" ]; then
            isDbFound=1
        fi
    done

    TMP_DB_NAMES=$DB_NAMES
    DB_NAMES=
    for DB_NAME in `echo $TMP_DB_NAMES | sed 's/:/ /g' | awk '{for(i=1;i<=NF;i++) print $i}' | sort | uniq`
    do
        if [ -z "$DB_NAMES" ]; then
            DB_NAMES=$DB_NAME
        else
            DB_NAMES=$DB_NAMES:$DB_NAME
        fi
    done


    for DB_NAME in `echo $DB_NAMES | sed 's/:/ /g'`
    do
        for ORACLE_DB_NAME in `echo $ORACLE_DB_NAMES | sed 's/:/ /g'` 
        do
            if [ "$DB_NAME" = "$ORACLE_DB_NAME" ]; then
                DB_CONF_FILE="/tmp/$DB_NAME.conf"
                if [ -f "$INMAGE_DIR/etc/$DB_NAME.conf" ]; then
                    cp -f  "$INMAGE_DIR/etc/$DB_NAME.conf" $DB_CONF_FILE
                else
                    if [ ! -f "$DB_CONF_FILE" ]; then
                        log "Error: Config file $INMAGE_DIR/etc/$DB_NAME.conf or $DB_CONF_FILE not found for database $DB_NAME"
                        echo "Error: Config file $INMAGE_DIR/etc/$DB_NAME.conf or $DB_CONF_FILE not found for database $DB_NAME"
                        exit 1
                    fi
                fi

                if [ -z "$DB_CONF_FILES" ]; then
                    DB_CONF_FILES=$DB_CONF_FILE
                else
                    DB_CONF_FILES=$DB_CONF_FILES:$DB_CONF_FILE
                fi
            fi
        done
    done

    return
}

StartBackupMode()
{
    tempfile=/tmp/tables.$$

    su - $ORACLE_USER -c "$ORAENV << _EOF > $tempfile
            select 'alter tablespace ' || tablespace_name || ' begin backup;' from dba_tablespaces where contents <> 'TEMPORARY';
_EOF"

    tempfile1=/tmp/beginbkp.$$.sql
    grep "TABLESPACE_NAME" $tempfile
    if [ $? -eq 0 ]; then
        grep -v "^$" $tempfile | grep -v "TABLESPACE_NAME" | grep -v "rows selected" | grep -v -- "--" > $tempfile1
    else
        rm -f $tempfile
        return
    fi

    rm -f $tempfile
    tempfile2=/tmp/bbkplog.$$

    su - $ORACLE_USER -c "$ORAENV << _EOF > $tempfile2
            @$tempfile1;
_EOF"
    #cat $tempfile2

    rm -f $tempfile1
    rm -f $tempfile2

    return
}

EndBackupMode()
{

    tempfile=/tmp/tables.$$

     su - $ORACLE_USER -c "$ORAENV << _EOF > $tempfile
            select 'alter tablespace ' || tablespace_name || ' end backup;' from dba_tablespaces where contents <> 'TEMPORARY';
_EOF"

    tempfile1=/tmp/endbkp.$$.sql

    grep "TABLESPACE_NAME" $tempfile
    if [ $? -eq 0 ]; then
         grep -v "^$" $tempfile | grep -v "TABLESPACE_NAME" | grep -v "rows selected" | grep -v -- "--" > $tempfile1
    else
        rm -f $tempfile;
        return
    fi

    rm -f $tempfile;

    tempfile2=/tmp/ebkplog.$$

    su - $ORACLE_USER -c "$ORAENV << _EOF > $tempfile2
            @$tempfile1;
_EOF"

   #cat $tempfile2
   rm -f $tempfile1
   rm -f $tempfile2

   return
}


OracleFreeze()
{
    if [ -z "$DB_CONF_FILES" ]; then
        echo "No active Oracle databases found to start backup mode."
        if [ "$ACTION" = "freeze" ]; then
            exit 2
        else
            return 
        fi
    fi

    freezeStatus=0
    frozenDbs=
    for DB_CONF_FILE in `echo $DB_CONF_FILES | sed 's/:/ /g'`
    do
        ORACLE_SID=`grep OracleSID $DB_CONF_FILE | awk -F= '{ print $2 }'`
        ORACLE_HOME=`grep OracleHome $DB_CONF_FILE | awk -F= '{ print $2 }'`
        if [ -z "$ORACLE_USER" ];then
            ORACLE_USER=`grep OracleUser $DB_CONF_FILE | awk -F= '{ print $2 }'`
        fi
        DB_VER=`grep DBVersion $DB_CONF_FILE | awk -F= '{ print $2 }'`
        DB_NAME=`grep OracleDBName $DB_CONF_FILE | awk -F"=" '{ print $2 }'`

        IsCShell $ORACLE_USER
        
        tempfile2=/tmp/rmanstatus.$$
   
        su - $ORACLE_USER -c "$ORAENV << _EOF > $tempfile2
                  select sofar, totalwork, round(sofar/totalwork*100,2) \"%_complete\" from v\\\$session_longops where opname like 'RMAN%' and opname not like '%aggregate%' and totalwork != 0 and sofar <> totalwork;
_EOF"
     
        grep -e "no rows selected" -e "0 rows selected" $tempfile2 > $NULL
        if [ $? -ne 0 ]; then
            echo "RMAN backup is running."
            log "RMAN backup is running."
            rm -f $tempfile2
            freezeStatus=1
            break
        fi

        rm -f $tempfile2

        tempfile2=/tmp/bkpstatus.$$
   
        su - $ORACLE_USER -c "$ORAENV << _EOF > $tempfile2
                   SELECT * FROM V\\\$BACKUP WHERE STATUS = 'ACTIVE';
_EOF"
     
        grep -e "no rows selected" -e "0 rows selected" $tempfile2 > $NULL
        if [ $? -ne 0 ]; then
            echo "database files are already in backup mode."
            log  "database files are already in backup mode."
            rm -f $tempfile2
            freezeStatus=1
            break
        fi

        rm -f $tempfile2

        GenerateArchiveLog

        if [ $? -ne 0 ]; then
            freezeStatus=1
            break
        fi

        if [ "$ACTION" != "validate" ]; then
            echo $DB_VER | awk -F"." '{print $1}' | grep 9 > /dev/null
            if [ $? -eq 0 ]; then
                StartBackupMode
            else

                echo "Putting database $DB_NAME in begin backup mode...."
                tempfile1=/tmp/orafreeze.$$

                su - $ORACLE_USER -c "$ORAENV << _EOF > $tempfile1
                                alter database begin backup;
_EOF"
                log "Result from OracleFreeze: `cat $tempfile1`"
                grep "Database altered" $tempfile1 > $NULL
                if [ $? -ne 0 ]; then
                    echo "putting database in backup mode failed."
                    log  "putting database in backup mode failed."
                    rm -f $tempfile1
                    freezeStatus=1
                    break
                fi
                rm -f $tempfile1
            fi
        fi

        if [ -z "$frozenDbs" ]; then
            frozenDbs="${DB_CONF_FILE}"
        else
            frozenDbs="${frozenDbs}:${DB_CONF_FILE}"
        fi

    done

    if [ $freezeStatus -eq 1 ]; then
        DB_CONF_FILES="${frozenDbs}"
        OracleUnFreeze
        exit 1
    fi

    return
}

OracleUnFreeze()
{
    if [ -z "$DB_CONF_FILES" ]; then
        echo "No Oracle databases found to end backup mode."
        return
    fi

    for DB_CONF_FILE in `echo $DB_CONF_FILES | sed 's/:/ /g'`
    do
        ORACLE_SID=`grep OracleSID $DB_CONF_FILE | awk -F= '{ print $2 }'`
        ORACLE_HOME=`grep OracleHome $DB_CONF_FILE | awk -F= '{ print $2 }'`
        if [ -z "$ORACLE_USER" ];then
           ORACLE_USER=`grep OracleUser $DB_CONF_FILE | awk -F= '{ print $2 }'`
        fi
        DB_VER=`grep DBVersion $DB_CONF_FILE | awk -F= '{ print $2 }'`
        DB_NAME=`grep OracleDBName $DB_CONF_FILE | awk -F"=" '{ print $2 }'`

        IsCShell $ORACLE_USER

        echo $DB_VER | awk -F"." '{print $1}' | grep 9 > /dev/null
        if [ $? -eq 0 ]; then
            EndBackupMode
        else
            tempfile1=/tmp/oraunfreeze.$$

            su - $ORACLE_USER -c "$ORAENV << _EOF > $tempfile1
                		alter database end backup;
_EOF"
            log "Result from OracleUnFreeze: `cat $tempfile1`"
            grep "Database altered" $tempfile1 > $NULL
            if [ $? -ne 0 ]; then
                grep "ORA-01142: cannot end online backup - none of the files are in backup" $tempfile1 > $NULL
                if [ $? -ne 0 ]; then
                    rm -f $tempfile1
                    exit 1                     
                fi
            fi
            rm -f $tempfile1

            log "Putting database $DB_NAME in end backup mode is done."
            echo "Putting database $DB_NAME in end backup mode is done."

        fi
    done

    return
}

GenerateTag()
{
    tempfile1=/tmp/vacpout.$$
    $INMAGE_DIR/bin/vacp -t $TAG -v $VOLUMES >$tempfile1 2>&1
    if [ $? -ne 0 ]
    then
        result=`cat $tempfile1`
        log "Result from GenerateTag: $result"
        echo "$result"
        echo "Vacp failed to issue tags.."
        OracleUnFreeze
        exit 1
    fi
    result=`cat $tempfile1`
    log "Result from GenerateTag: $result"
    echo "$result"

    rm -f $tempfile1
}

OracleVacpTagGenerate()
{
    volumes=
    DB_TAG=
    for DB_CONF_FILE in `echo $DB_CONF_FILES | sed 's/:/ /g'`
    do
        fsType=`grep MountPoint= $DB_CONF_FILE| awk -F"=" '{ print $NF }' | awk -F":" '{ print $1}'`

        if [[ "$fsType" != "zfs" ]];then 
        for volume in `grep MountPoint= $DB_CONF_FILE| awk -F"=" '{ print $NF }' | awk -F":" '{ print $2 }'`	
        do
            if [ -z "$volumes" ]; then
                volumes="$volume"
            else
                volumes="$volume,$volumes"
            fi
        done
        fi

        DB_NAME=`grep OracleDBName $DB_CONF_FILE | awk -F"=" '{ print $2 }'`
        if [ -z "$DB_TAG" ]; then
            DB_TAG="$DB_NAME"
        else
            DB_TAG="${DB_NAME}_${DB_TAG}"
        fi
	
        grep "isVxfs=YES" $DB_CONF_FILE >/dev/null
        if [ $? -eq 0 ]
        then
            $INMAGE_DIR/bin/vacp | grep -w "vxfs" >/dev/null
            if [ $? -ne 0 ]
            then
                echo "vacp doesn't support vxfs filesystems"
                echo "Cannot Freeze Filesystem.."
                log "Its not a vxfs vacp.. Cannot Freeze Filesystem.."
                OracleUnFreeze
                exit 1
            fi
        fi
    done


    TAG_NAME=$TAG

    if [ "$ACTION" != "failover" ]; then
        if [ ! -z "$DB_TAG" ]; then
            TAG_NAME="${TAG}_${DB_TAG}"
        fi
    fi

    tempfile1=/tmp/vacpout.$$

    if [ "$ACTION" != "failover" -a -z "$DB_TAG" ]; then
        $INMAGE_DIR/bin/vacp -v "${volumes},${VOLUMES}" >$tempfile1 2>&1
    else
        $INMAGE_DIR/bin/vacp -t $TAG_NAME -v "${volumes},${VOLUMES}" >$tempfile1 2>&1
    fi
    if [ $? -ne 0 ]
    then
        result=`cat $tempfile1`
        log "Result from OracleVacpTagGenerate: $result"
        echo "$result"
        echo "Vacp failed to issue tags.."
        OracleUnFreeze
        exit 1
    fi

    result=`cat $tempfile1`
    log "Result from OracleVacpTagGenerate: $result"
    echo "$result"

    rm -f $tempfile1
}

GenerateArchiveLog()
{
    echo "Generating Archive log for database $DB_NAME "

    result=`su - $ORACLE_USER -c "$ORAENV << _EOF
                  alter system archive log current;
_EOF"`

        log "Result from GenerateArchiveLog: $result"
        echo $result|grep -w "System altered" >>/dev/null
        if [ $? -ne 0 ]
        then
            log "Archive log generation failed.."
            echo "Archive log generation failed.."
            return 1
        else
            echo "Succesfully generated archive log"
        fi
        return
}

ValidateVacp()
{
	grep "isVxfs=YES" $SRC_CONF_FILE >/dev/null
	if [ $? -eq 0 ]
	then
        $INMAGE_DIR/bin/vacp | grep -w "vxfs" >/dev/null
        if [ $? -ne 0 ]
        then
            echo "ERROR: $INMAGE_DIR/bin/vacp cannot freeze vxfs filesystem to issue tags."
            exit 1
	   fi
	fi
}

################   MAIN   #############

trap Cleanup 0

# trap CleanupError SIGHUP SIGINT SIGQUIT SIGTERM -  SLES doesn't know the definitions
if [ "$PLATFORM" = "Linux" -o "$PLATFORM" = "SunOS" -o "$PLATFORM" = "AIX" ]; then
    trap CleanupError 1 2 3 15
else
    exit 0
fi

date
echo "Script $0 $* spawned on source host `hostname`"

ParseArgs $*

GetDbInfo

if [ "$ACTION" != "unfreeze" ]; then
    OracleFreeze
fi

if [ "$ACTION" = "validate" ]; then
    # no need to issue a tag
    ValidateVacp
else
    if [ "$ACTION" != "freeze" -a "$ACTION" != "unfreeze" ]; then
        OracleVacpTagGenerate
    fi
    if [ "$ACTION" != "freeze" ]; then
        OracleUnFreeze
    fi
fi


if [ $isDbFound -eq 0 -a "$ACTION" = "consistency" ]; then
    echo "protected database not online" 
    exit 1
fi

date
exit 0
