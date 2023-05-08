#!/bin/bash
#=================================================================
# FILENAME
#    oraclediscovery.sh
#
# DESCRIPTION
#	Discover the Oracle database info
#
# HISTORY
#     <06/09/2011>  Vishnu     Created.
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================

PATH=/bin:/usr/bin:/usr/sbin:/sbin:$PATH
ECHO=/bin/echo
SED=/bin/sed
OSTYPE=`uname -s`

Usage()
{
    $ECHO "usage: $0 <ORACLE_USER> <ORACLE_SID> <ORACLE_HOME> <OUTFILE1> <OUTFILE2>"
    return
}

ParseArgs()
{
    if [ $# -ne 5 ]; then
        Usage
        exit 1
    fi

    dirName=`dirname $0`
    orgWd=`pwd`

    cd $dirName
    SCRIPT_DIR=`pwd`
    cd $orgWd

    LOG_FILE=/var/log/oradisc.log

    ORACLE_USER=$1
    ORACLE_DB_INS=$2
    ORACLE_DB_INS_HOME=$3
    OUTPUT_FILE=$4
    CONF_FILE=$5

    > $OUTPUT_FILE
    > $CONF_FILE

    if [ "$OSTYPE" = "SunOS" ]; then
        ID=/usr/xpg4/bin/id
    else
        ID=/usr/bin/id
    fi


    id $ORACLE_USER > /dev/null
    if [ $? -ne 0 ]; then
        $ECHO "Error: Oracle user $ORACLE_USER not found"
        log "Error: Oracle user $ORACLE_USER not found"
        exit 1
    fi

    ps -ef | grep -v grep | grep -w ora_pmon_${ORACLE_DB_INS} > /dev/null
    if [ $? -ne 0 ]; then
        $ECHO "Error: Oracle SID $ORACLE_DB_INS not found"
        log "Error: Oracle SID $ORACLE_DB_INS not found"
        exit 1
    fi 

    if [ ! -d $ORACLE_DB_INS_HOME/bin ]; then
        $ECHO "Error: Oracle home $ORACLE_DB_INS_HOME not found"
        log "Error: Oracle home $ORACLE_DB_INS_HOME not found"
        exit 1
    fi

    if [ ! -f "$LOG_FILE" ]; then
        > $LOG_FILE
        chmod 640 $LOG_FILE
    fi
}

log()
{
    $ECHO "(`date +'%m-%d-%Y %H:%M:%S'`):   DEBUG $$ oraclediscovery $*" >> $LOG_FILE
    return
}


GetDbVersion()
{

    tempfile=/tmp/verfile.$$

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' << _EOF > $tempfile
         select version from v\\\$instance;
_EOF"

    grep "VERSION" $tempfile > /dev/null
    if [ $? -eq 0 ]; then
        DB_VERSION=`grep -v "^$" $tempfile | grep -v "VERSION" | grep -v -- "--"`
        log "Database version $DB_VERSION"
    else
        log "Database version not found for SID $ORACLE_SID"
        $ECHO "Database version not found for SID $ORACLE_SID"
        rm -f $tempfile
        exit 1
    fi

    rm -f $tempfile
}

GetIsCluster()
{
    tempfile=/tmp/clufile.$$

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' << _EOF > $tempfile
     select parallel from v\\\$instance;
_EOF"

    grep "PAR" $tempfile > /dev/null
    if [ $? -eq 0 ]; then
        IS_CLUSTER=`grep -v "^$" $tempfile | grep -v "PAR" | grep -v -- "--"`
    else
        log "IS_CLUSTER not found for SID $ORACLE_SID"
        $ECHO "IS_CLUSTER not found for SID $ORACLE_SID"
        rm -f $tempfile
        exit 1
    fi

    rm -f $tempfile
}

GetDbNames()
{
    tempfile=/tmp/instfile.$$

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' << _EOF > $tempfile
         select instance_name from v\\\$instance;
_EOF"

    grep "INSTANCE_NAME" $tempfile > /dev/null
    if [ $? -eq 0 ]; then
        DB_INST_NAME=`grep -v "^$" $tempfile | grep -v "INSTANCE_NAME" | grep -v -- "--"`
        log "Database instance name : $DB_INST_NAME"
    else
        log "Database instance not found for SID $ORACLE_SID"
        $ECHO "Database instance not found for SID $ORACLE_SID"
        rm -f $tempfile
        exit 1
    fi

    echo $DB_VERSION | awk -F"." '{print $1}' | grep 9 > /dev/null
    if [ $? -eq 0 ]; then

        su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' << _EOF > $tempfile
             select value from v\\\$parameter where name='db_name';
_EOF"

        grep "VALUE" $tempfile > /dev/null
        if [ $? -eq 0 ]; then
            DB_NAME=`grep -v "^$" $tempfile | grep -v "VALUE" | grep -v -- "--"`
            log "Database name : $DB_NAME"
            return
        else
            log "Database name not found for SID $ORACLE_SID"
            $ECHO "Database name not found for SID $ORACLE_SID"
            rm -f $tempfile
            exit 1
        fi
    fi 

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' << _EOF > $tempfile
     select db_unique_name from v\\\$database;
_EOF"

    grep "DB_UNIQUE_NAME" $tempfile > /dev/null
    if [ $? -eq 0 ]; then
        DB_NAME=`grep -v "^$" $tempfile | grep -v "DB_UNIQUE_NAME" | grep -v -- "--"`
    else
        log "Database unique name not found for SID $ORACLE_SID"
        $ECHO "Database unique name not found for SID $ORACLE_SID"
        rm -f $tempfile
        exit 1
    fi

    rm -f $tempfile

    return
}


GetNumClusterDbInstances()
{
    tempfile=/tmp/numcludbinstfile.$$

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' << _EOF > $tempfile
        select value from v\\\$parameter where name='cluster_database_instances';
_EOF"

    grep "VALUE" $tempfile > /dev/null
    if [ $? -eq 0 ]; then
        NUM_CLUSTER_DB_INST=`grep -v "^$" $tempfile | grep -v "VALUE" | grep -v -- "--"`
    else
        log "NUM_CLUSTER_DB_INST not found for SID $ORACLE_SID"
        $ECHO "NUM_CLUSTER_DB_INST not found for SID $ORACLE_SID"
        rm -f $tempfile
        exit 1
    fi

    rm -f $tempfile
}

GetPfileLocation()
{
    isPfileFound=1
    pfileHasSpfile=1
    
    PFILE_LOC="$ORACLE_HOME/dbs/init${ORACLE_SID}.ora"
    if [ ! -f $PFILE_LOC ] ; then 
        log "pfile $ORACLE_HOME/dbs/init${ORACLE_SID}.ora not found"
        $ECHO "pfile $ORACLE_HOME/dbs/init${ORACLE_SID}.ora not found"
        isPfileFound=0
        pfileHasSpfile=0
        return
    fi

    grep -i "spfile=" $PFILE_LOC > /dev/null
    if [ $? -ne 0 ]; then
        log "pfile $ORACLE_HOME/dbs/init${ORACLE_SID}.ora doesn't have spfile location"
        $ECHO "pfile $ORACLE_HOME/dbs/init${ORACLE_SID}.ora doesn't have spfile location"
        pfileHasSpfile=0
        return
    fi

    SPFILE_LOC_IN_PFILE=`grep spfile= $PFILE_LOC | awk -F"=" '{ print $2 }' | sed -e "s/'//g"`
    if [ -z "$SPFILE_LOC_IN_PFILE" ]; then
        log "SPFILE_LOC_IN_PFILE not found for SID $ORACLE_SID"
        $ECHO "SPFILE_LOC_IN_PFILE not found for SID $ORACLE_SID"
        pfileHasSpfile=0
    fi
}

GetSpfileLocation()
{
    tempfile=/tmp/spfileloc.$$
    isSpfileFound=1

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' << _EOF > $tempfile
	select value from v\\\$parameter where name='spfile';
_EOF"

    grep "VALUE" $tempfile > /dev/null
    if [ $? -eq 0 ]; then
        SPFILE_LOC=`grep -v "^$" $tempfile | grep -v "VALUE" | grep -v -- "--"`
        if [ -z "$SPFILE_LOC" ]; then
            log "SPFILE_LOC not found for SID $ORACLE_SID"
            $ECHO "SPFILE_LOC not found for SID $ORACLE_SID"
            isSpfileFound=0
        fi
    else
        log "SPFILE_LOC not found for SID $ORACLE_SID"
        $ECHO "SPFILE_LOC not found for SID $ORACLE_SID"
        isSpfileFound=0
    fi

    rm -f $tempfile
}

CreatePfileFromSpfile()
{
    tempfile=/tmp/crpfile.$$

    su - $ORACLE_USER -c "SPFILE_LOC=$SPFILE_LOC;ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export SPFILE_LOC ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' << _EOF > $tempfile
       
	CREATE PFILE='/tmp/oracledb_pfile.ora' FROM SPFILE='$SPFILE_LOC';
_EOF"

    if [ ! -f $CreatedPfile ]; then
        log "Failed to creat pfile=$CreatedPfile from spfile=$SPFILE_LOC"
        $ECHO "Failed to creat pfile=$CreatedPfile from spfile=$SPFILE_LOC"
    fi

    rm -f $tempfile
}

GetPfile()
{
    GetPfileLocation
    GetSpfileLocation

    CreatedPfile="/tmp/oracledb_pfile.ora"
    rm -f $CreatedPfile

    if [ $isSpfileFound -eq 1 ]; then
        log "creating pfile from spfile=$SPFILE_LOC."
        $ECHO "creating pfile from spfile=$SPFILE_LOC."

        CreatePfileFromSpfile
    fi

    if [ ! -f $CreatedPfile ]; then

        if [ $pfileHasSpfile -eq 1 -a "$SPFILE_LOC" != "$SPFILE_LOC_IN_PFILE" ]; then
            log "SPFILE_LOC=$SPFILE_LOC and SPFILE_LOC_IN_PFILE=$SPFILE_LOC_IN_PFILE does not match."

            SPFILE_LOC=$SPFILE_LOC_IN_PFILE
            log "creating pfile from spfile=$SPFILE_LOC."
            $ECHO "creating pfile from spfile=$SPFILE_LOC."

            CreatePfileFromSpfile
        fi
    fi

    if [ ! -f $CreatedPfile ]; then
        if [ $isPfileFound -eq 1 -a $pfileHasSpfile -eq 0 ]; then
            cp -f $PFILE_LOC $CreatedPfile
        fi
    fi
}

GetLogMode()
{
    tempfile=/tmp/logmode.$$

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' << _EOF > $tempfile
        SELECT LOG_MODE FROM V\\\$DATABASE;
_EOF"

    grep "LOG_MODE" $tempfile > /dev/null
    if [ $? -eq 0 ]; then
        LOG_MODE=`grep -v "^$" $tempfile | grep -v "LOG_MODE" | grep -v -- "--"`
    else
        log "LOG_MODE not found for SID $ORACLE_SID"
        $ECHO "LOG_MODE not found for SID $ORACLE_SID"
        rm -f $tempfile
        exit 1
    fi

    rm -f $tempfile
}

GetDbFileLocations()
{
    tempfile=/tmp/dbop.$$
    tempfile1=/tmp/dbdirs.$$

    su - $ORACLE_USER -c "ORACLE_SID=$ORACLE_SID;ORACLE_HOME=$ORACLE_HOME;export ORACLE_HOME ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<_EOF >$tempfile
        SELECT NAME FROM V\\\$DATAFILE UNION SELECT MEMBER FROM V\\\$LOGFILE UNION SELECT NAME FROM V\\\$CONTROLFILE;
_EOF"

    grep "NAME" $tempfile > /dev/null
    if [ $? -eq 0 ]; then
        >$tempfile1
        for i in `grep -v "^$" $tempfile | grep -v "NAME" | grep -v "MEMBER" | grep -v "rows selected" | grep -v -- "--"`
        do
           dirname $i >>$tempfile1
        done
        UNIQ_DIRS=`cat $tempfile1 | sort | uniq`
    else
        log "DB File Location not found for SID $ORACLE_SID"
        $ECHO  "DB File Location not found for SID $ORACLE_SID"
        rm -f $tempfile
        exit 1
    fi

    rm -f $tempfile
    rm -f $tempfile1
}


WriteToConfigFile()
{
    echo "DBVersion=$DB_VERSION" > $OUTPUT_FILE
    echo "DBVersion=$DB_VERSION" > $CONF_FILE
    echo "isCluster=$IS_CLUSTER" >> $OUTPUT_FILE
    echo "isCluster=$IS_CLUSTER" >> $CONF_FILE
    echo "TotalDbNodes=$NUM_CLUSTER_DB_INST" >> $CONF_FILE
    echo "SPFile=$SPFILE_LOC" >> $CONF_FILE
    echo "PFile=$PFILE_LOC" >> $CONF_FILE
    echo "RecoveryLogEnabled=$LOG_MODE" >> $OUTPUT_FILE
    echo "InstallPath=$ORACLE_HOME" >> $OUTPUT_FILE
    echo "usingAsmDg=FALSE" >> $OUTPUT_FILE
    echo "dbFileLoc=$UNIQ_DIRS" >> $OUTPUT_FILE
    echo "isASM=NO" >>$CONF_FILE
    echo "DBName=$DB_NAME" >> $OUTPUT_FILE
    echo "OracleDBName=$DB_NAME" >> $CONF_FILE
}

GetDbDetails()
{
    GetDbVersion
    GetIsCluster
    GetDbNames
    GetNumClusterDbInstances
    GetLogMode
    GetPfile
    GetDbFileLocations
    WriteToConfigFile
}

#########################################################################
#      Main
#########################################################################
ParseArgs $*

log "ORACLE_USER: $ORACLE_USER"
log "ORACLE_DB_INS: $ORACLE_DB_INS"
log "ORACLE_DB_INS_HOME: $ORACLE_DB_INS_HOME"

if [ "${ORACLE_DB_INS_HOME}" != "" ];then
    ORACLE_HOME="${ORACLE_DB_INS_HOME}"
    export ORACLE_HOME
else
    $ECHO "The Oracle home is not available."
    log "The Oracle home is not available."
    exit 1
fi

if [ "${ORACLE_DB_INS}" != "" ];then
    ORACLE_SID="${ORACLE_DB_INS}"
    export ORACLE_SID
else
    $ECHO "The Oracle instance is not available."
    log "The Oracle instance is not available."
    exit 1
fi

#echo "ORACLE_SID:$ORACLE_SID"
#echo "ORACLE_HOME:$ORACLE_HOME"
#echo "PATH:$PATH"

GetDbDetails

exit 0
