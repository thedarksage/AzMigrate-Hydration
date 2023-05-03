#!/bin/bash
#=================================================================
# FILENAME
#    oracleappdiscovery.sh
#
# DESCRIPTION
#    setup the perl environment for oracleappdiscovery.pl
#
# HISTORY
#     <28/06/2010>  Vishnu     Created.
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

    CURRENT_USER=`$ID -un`

    id $ORACLE_USER > /dev/null
    if [ $? -ne 0 ]; then
        $ECHO "Error: Oracle user $ORACLE_USER not found"
        log "Error: Oracle user $ORACLE_USER not found"
        exit 1
    fi

    if [ "$CURRENT_USER" != "$ORACLE_USER" ]; then
        $ECHO "Error: not running as user $ORACLE_USER"
        log "Error: not running as user $ORACLE_USER"
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

PATH="${PATH}:${ORACLE_HOME}/bin"
export PATH

#echo "ORACLE_SID:$ORACLE_SID"
#echo "ORACLE_HOME:$ORACLE_HOME"
#echo "PATH:$PATH"

if [ "$OSTYPE" = "AIX" ]; then
    if [ -d ${ORACLE_HOME}/lib32 ]; then
        LIBPATH="${ORACLE_HOME}/lib32:${LIBPATH}"
    fi
    LIBPATH="${ORACLE_HOME}/lib:${LIBPATH}"
    export LIBPATH
    log "LIBPATH : $LIBPATH"
else
    if [ -d ${ORACLE_HOME}/lib32 ]; then
        LD_LIBRARY_PATH="${ORACLE_HOME}/lib32:${LD_LIBRARY_PATH}"
    fi
    LD_LIBRARY_PATH="${ORACLE_HOME}/lib:${LD_LIBRARY_PATH}"
    export LD_LIBRARY_PATH
    #echo "LD_LIBRARY_PATH : $LD_LIBRARY_PATH"
fi

PERLBIN="${ORACLE_HOME}/perl/bin/perl"

PERLV=`${PERLBIN} -e 'print $]'`
PERLV=`${ECHO} ${PERLV} | awk -F"." '{ v=$1; M=$2/1000; m=$2%1000; printf "%d.%d.%d",v,M,m;}'`

# export PERL5LIB
PERL5LIB="${ORACLE_HOME}/perl/lib/${PERLV}"
PERL5LIB="${PERL5LIB}:${ORACLE_HOME}/perl/lib/site_perl/${PERLV}"
PERL5LIB="${PERL5LIB}:${ORACLE_HOME}/bin"

export PERL5LIB

#echo "PERLBIN:: $PERLBIN";
#echo "PERL5LIB:: $PERL5LIB";

RUNTHIS="${PERLBIN} -w ${SCRIPT_DIR}/oracleappdiscovery.pl"

log "${RUNTHIS} $ORACLE_SID $ORACLE_HOME $OUTPUT_FILE $CONF_FILE"

exec ${RUNTHIS} $ORACLE_SID $ORACLE_HOME $OUTPUT_FILE $CONF_FILE

exit 1
