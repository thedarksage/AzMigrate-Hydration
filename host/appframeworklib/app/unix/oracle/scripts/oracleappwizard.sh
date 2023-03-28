#!/bin/bash
#=================================================================
# FILENAME
#    oracleappwizard.sh
#
# DESCRIPTION
# application wizard for oracle.
#    
# HISTORY
#     <29/10/2010>  Vishnu     Created.
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================

PATH=/bin:/usr/bin:/usr/sbin:/sbin:$PATH

ECHO=/bin/echo
SED=/bin/sed
CP=/bin/cp
MOVE=/bin/mv
VXDISK=vxdisk
DU=/usr/bin/du

NULL=/dev/null

PLATFORM=`uname | tr -d "\n"`
HOSTNAME=`hostname`

TMP_OUTPUT_FILE=/tmp/appwizard

Usage()
{
    $ECHO
    $ECHO "usage: $0 <query|update|display>"
    $ECHO "query - to initialize the application settings"
    $ECHO "update - to update the existing settings"
    $ECHO "display - to display the existing settings"
    $ECHO
    return
}

ParseArgs()
{
    if [ $# -ne 1 ]; then
        Usage
        exit 1
    fi

    OPTION=$1
    
    if [ $OPTION != "query" -a $OPTION != "update"  -a $OPTION != "display" ]; then
        Usage
        exit 1
    fi

    SetupLogs

    return
}

Output()
{
    $ECHO $* >> $TMP_OUTPUT_FILE
    return
}

SetupLogs()
{
    dirName=`dirname $0`
    orgWd=`pwd`

    cd $dirName
    SCRIPT_DIR=`pwd`
    cd $orgWd

    LOG_FILE=$SCRIPT_DIR/oradisc.log

    if [ -f $LOG_FILE ]; then
        size=`$DU -k $LOG_FILE | awk '{ print $1 }'`
        if [ $size -gt 5000 ]; then
            $CP -f $LOG_FILE $LOG_FILE.1
            > $LOG_FILE
        fi
    else
        > $LOG_FILE
        chmod a+w $LOG_FILE
    fi
}

log()
{
    $ECHO "(`date +'%m-%d-%Y %H:%M:%S'`):   DEBUG $$ $OPTION $*" >> $LOG_FILE
    return
}

Cleanup()
{
    $ECHO "Exiting application wizard."
    rm -rf $TMP_OUTPUT_FILE &> $NULL
}

CleanupError()
{
    rm -rf $TMP_OUTPUT_FILE &> $NULL
    exit 1
}

WriteToConfFile()
{
    $ECHO $* >> $APP_CONF_FILE 
    return
}

ReadAns()
{
    $ECHO $* "[Y/N]:"

    while :
    do
        read ANS
        if [ "$ANS" = "y" -o "$ANS" = "Y" -o "$ANS" = "n" -o "$ANS" = "N" ]; then
            break
        fi
        $ECHO "Please enter [Y/N]:"
    done

}

ReadOpt()
{
    options=$1
    $ECHO "Please select from $options:"

    while :
    do
        read OPT
        echo $options | grep -w $OPT > /dev/null 2>&1
        if [ $? -eq 0 ]; then
            break
        fi
        $ECHO "Please select from $options:"
    done
}

ReadOratab()
{
    $ECHO

    for line in `cat $ORATAB | grep -v "^#" | grep -v "^$" | grep -v "^*:" | grep -v "+ASM" | awk -F# '{print $1}'`
    do
        DB_NAME=`echo $line | cut -d: -f1`

        if [ ! -z "$DB_NAME" ]; then
            DB_NAMES="$DB_NAMES $DB_NAME"
        fi
    done

    if [ -z "$DB_NAMES" ]; then
        $ECHO "There are no Oracle database entries found in $ORATAB file."
        QueryOracleHomes 
        return
    fi

    $ECHO "Found the following database names in $ORATAB."
    $ECHO "$DB_NAMES"
    $ECHO

    ReadAns "Would you like to enable discovery for all the above databases?"

    if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then

        for line in `cat $ORATAB | grep -v "^#" | grep -v "^$" | grep -v "^*:" | grep -v "+ASM" | awk -F# '{print $1}'`
        do
            DB_NAME=`echo $line | cut -d: -f1`
            DB_LOC=`echo $line | cut -d: -f2`

            if [ ! -z "$DB_NAME" -a -d "$DB_LOC" ]; then
                WriteToConfFile "OracleDatabase=YES:$DB_NAME:$DB_LOC"
            fi
        done

    elif [ "$ANS" = "n" -o "$ANS" = "N" ]; then

        $ECHO "Please specify the databases you would like to enable for discovery."
        for line in `cat $ORATAB | grep -v "^#" | grep -v "^$" | grep -v "^*:" | grep -v "+ASM" | awk -F# '{print $1}'`
        do
            DB_NAME=`echo $line | cut -d: -f1`
            DB_LOC=`echo $line | cut -d: -f2`

            if [ ! -z "$DB_NAME" -a -d "$DB_LOC" ]; then
                $ECHO
                ReadAns "Would you like to enable discovery for database '$DB_NAME'?"

                if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then
                    WriteToConfFile "OracleDatabase=YES:$DB_NAME:$DB_LOC"
                fi

                if [ "$ANS" = "n" -o "$ANS" = "N" ]; then
                    WriteToConfFile "OracleDatabase=NO:$DB_NAME:$DB_LOC"
                fi
            fi
        done

    fi

    $ECHO
    $ECHO "Following ORACLE_HOME locations are found in $ORATAB"
    $ECHO

    cat $ORATAB | grep -v "^#" | grep -v "^*:" | grep -v "^$" | grep -v "+ASM" | awk -F# '{ print $1 }' | awk -F: '{print $2}' | sort | uniq

    $ECHO
    $ECHO "If this host `hostname` is intended to be used as a Secondary Server for Oracle database failover please specify at least one ORACLE_HOME to be used during failover."
    $ECHO
    $ECHO "In case more than one ORACLE_HOME is specified, during failover one of the compatible ORACLE_HOMEs will be used."
    $ECHO

    ReadAns "Would you like all of the above ORACLE_HOME locations to be used during failover?"

    if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then

        for DB_LOC in `cat $ORATAB | grep -v "^#" | grep -v "^*:" | grep -v "^$" | grep -v "+ASM" | awk -F# '{ print $1 }' | awk -F: '{print $2}' | sort | uniq`
        do
            if [ -d "$DB_LOC" ]; then
                ValidateOracleHome $DB_LOC
            fi
        done

    elif [ "$ANS" = "n" -o "$ANS" = "N" ]; then

        for DB_LOC in `cat $ORATAB | grep -v "^#" | grep -v "^*:" | grep -v "^$" | grep -v "+ASM" | awk -F# '{ print $1 }'| awk -F: '{print $2}' | sort | uniq`
        do
            if [ -d "$DB_LOC" ]; then

                ReadAns "Would you like '$DB_LOC' to be used as ORACLE_HOME during failover?"

                if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then
                    ValidateOracleHome $DB_LOC
                fi

                if [ "$ANS" = "n" -o "$ANS" = "N" ]; then
                    WriteToConfFile "OracleHome=NO:$DB_LOC"
                fi
            fi
        done
    fi
}

QueryOracleHomes()
{
    $ECHO
    $ECHO "If this host '`hostname`' is intended to be used as a for Oracle database failover please specify at least one ORACLE_HOME."
    $ECHO
    $ECHO "In case more than one ORACLE_HOME is specified, during failover one of the compatible ORACLE_HOMEs will be used."

    $ECHO
    ReadAns "Would you like to specify ORACLE_HOME location?"

    if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then
        $ECHO
        $ECHO "Please enter ORACLE_HOME directory:"
        read ANS

        if [ ! -d "$ANS" ]; then
            $ECHO "Directory $ANS not found."
        else
            ValidateOracleHome $ANS
        fi
    else
        return
    fi

    while :
    do
        $ECHO
        ReadAns "Would you like to specify additional ORACLE_HOME locations?"

        if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then
            $ECHO
            $ECHO "Please enter ORACLE_HOME directory:"
            read ANS

            if [ ! -d "$ANS" ]; then
                $ECHO "Directory $ANS not found."
            else
                ValidateOracleHome $ANS
            fi
        else
            break
        fi
    done

}

QueryOracleDiscovery()
{
    $ECHO
    ReadAns "Would you like to enable Oracle database application discovery on this host?"

    if [ "$ANS" = "n" -o "$ANS" = "N" ]; then
        WriteToConfFile "EnableOracleDatabaseDiscovery=NO"
        exit 0
    fi

    WriteToConfFile "EnableOracleDatabaseDiscovery=YES"

    if [ -f $ORATAB ]; then
        ReadOratab
    else
        $ECHO "Default Oratab file '$ORATAB' not found."
        while :
        do
            $ECHO
            ReadAns "Would you like to specify oratab file location?"

            if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then
                $ECHO "Please enter oratab file path:"
                read ANS

                if  [ ! -f "$ANS" ]; then
                    $ECHO "File $ANS not found."
                else
                    ORATAB=$ANS
                    ReadOratab
                    break
                fi
            else
                return 
            fi
        done
    fi

    $ECHO
    $ECHO "Probing for Oracle dba user..."
 
    oracleUser=`ps -ef | grep _pmon_ | grep -v grep | head -1 | awk '{ print $1 }'`
    if [ ! -z "$oracleUser" ]; then
        oracleGroup=`id $oracleUser | awk '{print $2}' | awk -F"(" '{ print $2}' | awk -F")" '{ print $1}'`
        $ECHO
        $ECHO "Found '$oracleUser'."

        $ECHO
        $ECHO "Oracle database discovery/failover will use user '$oracleUser' and group '$oracleGroup'."
        $ECHO
        WriteToConfFile "OracleUser=$oracleUser"
        WriteToConfFile "OracleGroup=$oracleGroup"
    elif [ ! -z "$ORACLE_HOME" ]; then
        if [ -f "$ORACLE_HOME/oraInst.loc" ]; then
            oracleUser=`ls -l $ORACLE_HOME/oraInst.loc | awk '{print $3}'`
            if [ ! -z "$oracleUser" ]; then
                oracleGroup=`id $oracleUser | awk '{print $2}' | awk -F"(" '{ print $2}' | awk -F")" '{ print $1}'`
                $ECHO
                $ECHO "Found '$oracleUser'."
                $ECHO
                $ECHO "Oracle database discovery/failover will use user '$oracleUser' and group '$oracleGroup'."
                $ECHO
                WriteToConfFile "OracleUser=$oracleUser"
                WriteToConfFile "OracleGroup=$oracleGroup"
            fi
        fi
    else

        $ECHO
        $ECHO "Oracle user could not be detected automatically."
        $ECHO
        $ECHO "Please enter oracle dba user name:"
        read ANS

        if  [ -z "$ANS" ]; then
            $ECHO "Oracle user is not valid."
            return
        else
            oracleUser=$ANS
            id $oracleUser > /dev/null
            if [ $? -ne 0 ]; then
                $ECHO "Oracle user $oracleUser not found."
                return
            fi
        fi

        oracleGroup=`id $oracleUser | awk '{print $2}' | awk -F"(" '{ print $2}' | awk -F")" '{ print $1}'`
        if [ -z "$oracleGroup" ]; then
            $ECHO "Oracle group not found."
            return
        fi

        $ECHO
        $ECHO "Application discovery will use user '$oracleUser' and group '$oracleGroup' for discovery."
        WriteToConfFile "OracleUser=$oracleUser"
        WriteToConfFile "OracleGroup=$oracleGroup"

    fi
}

ValidateOracleHome()
{
    ORACLE_HOME=$1
    export ORACLE_HOME

    if [ ! -f "$ORACLE_HOME/bin/oracle" ]; then
        $ECHO "Directory $ORACLE_HOME not a valid ORACLE_HOME."
        return
    fi

    TARGET_ORACLE_VERSION=`strings $ORACLE_HOME/bin/oracle | grep NLSRTL | awk '{print $3}'`
    if [ -z "$TARGET_ORACLE_VERSION" ]; then
        $ECHO "Error: Failed to find Oracle version for ORACLE_HOME $ORACLE_HOME."
        return
    fi
    $ECHO
    $ECHO "Found version '$TARGET_ORACLE_VERSION' for ORACLE_HOME '$ORACLE_HOME'."
    $ECHO

    WriteToConfFile "OracleHome=YES:$ORACLE_HOME"
}

UpdateConfig()
{
    if [ ! -f "$APP_CONF_FILE" ]; then
        $ECHO "$APP_CONF_FILE not found. Please initialize with 'query' option."
        return
    fi

    while :
    do
        clear
        $ECHO
        $ECHO
        $ECHO "1. Register new databases for discovery"
        $ECHO "2. Enable registered databases for discovery"
        $ECHO "3. Disable registered databases for discovery"
        $ECHO "4. Registered new ORACLE_HOMEs"
        $ECHO "5. Enable registered ORACLE_HOMEs to be used for failover"
        $ECHO "6. Disable registered ORACLE_HOMEs to be used for failover"
        $ECHO "7. Unregister databases"
        $ECHO "8. Unregister ORACLE_HOMEs"
        $ECHO "9. Exit"
        $ECHO

        ReadOpt "1 2 3 4 5 6 7 8 9"

        case $OPT in
        1)
            RegisterNewDb
            $ECHO "Please press enter to continue"
            read cr
            ;;
        2)
            oracleDbs=`grep OracleDatabase=NO $APP_CONF_FILE`
            if [ ! -z "$oracleDbs" ]; then
                for oracleDb in $oracleDbs
                do
                    dbName=`echo $oracleDb | awk -F: '{ print $2 }'`

                    ReadAns "Would you like to enable '$dbName' for discovery?"
                    if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then
                        EnableRegisteredDb $dbName
                    fi
                done
            else
                $ECHO "There are no disabled databases"
            fi
            $ECHO "Please press enter to continue"
            read cr
            ;;
        3)
            oracleDbs=`grep OracleDatabase=YES $APP_CONF_FILE`
            if [ ! -z "$oracleDbs" ]; then
                for oracleDb in $oracleDbs
                do
                    dbName=`echo $oracleDb | awk -F: '{ print $2 }'`

                    ReadAns "Would you like to disable '$dbName' from discovery?"
                    if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then
                        DisableRegisteredDb $dbName
                    fi
                done
            else
                $ECHO "There are no enabled databases"
            fi
            $ECHO "Please press enter to continue"
            read cr
            ;;
        4) 
            RegisterNewOraHome
            $ECHO "Please press enter to continue"
            read cr
            ;;
        5)
            oracleHomes=`grep OracleHome=NO $APP_CONF_FILE`
            if [ ! -z "$oracleHomes" ]; then
                for oracleHome in $oracleHomes
                do
                    oraHome=`echo $oracleHome | awk -F: '{ print $2 }'`

                    ReadAns "Would you like to enable '$oraHome' to be used for failover?"
                    if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then
                        EnableRegisteredOraHome $oraHome
                    fi
                done
            else
                $ECHO "There are no disabled ORACLE_HOMEs"
            fi
            $ECHO "Please press enter to continue"
            read cr
            ;;
        6)
            oracleHomes=`grep OracleHome=YES $APP_CONF_FILE`
            if [ ! -z "$oracleHomes" ]; then
                for oracleHome in $oracleHomes
                do
                    oraHome=`echo $oracleHome | awk -F: '{ print $2 }'`

                    ReadAns "Would you like to disable '$oraHome' to be used for failover?"
                    if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then
                        DisableRegisteredOraHome $oraHome
                    fi
                done
            else
                $ECHO "There are no enabled ORACLE_HOMEs"
            fi
            $ECHO "Please press enter to continue"
            read cr
            ;;
        7)
            oracleDbs=`grep OracleDatabase= $APP_CONF_FILE`
            if [ ! -z "$oracleDbs" ]; then
                for oracleDb in $oracleDbs
                do
                    dbName=`echo $oracleDb | awk -F: '{ print $2 }'`

                    ReadAns "Would you like to unregister database '$dbName'?"
                    if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then
                        UnregisterDb $dbName
                    fi
                done
            else
                $ECHO "There are no registered databases"
            fi
            $ECHO "Please press enter to continue"
            read cr
            ;;
        8)
            oracleHomes=`grep OracleHome= $APP_CONF_FILE`
            if [ ! -z "$oracleHomes" ]; then
                for oracleHome in $oracleHomes
                do
                    oraHome=`echo $oracleHome | awk -F: '{ print $2 }'`

                    ReadAns "Would you like to unregister ORACLE_HOME '$oraHome'?"
                    if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then
                        UnregisterOraHome $oraHome
                    fi
                done
            else
                $ECHO "There are no registered ORACLE_HOMEs"
            fi
            $ECHO "Please press enter to continue"
            read cr
            ;;
        9)
            return
            ;;
        esac
    done
}

DisableRegisteredDb()
{
    dbName=$1
    tempfile1=/tmp/appwizconf.$$

    cfgEntry=`grep OracleDatabase=YES:$dbName: $APP_CONF_FILE`
    
    if [ -z "$cfgEntry" ]; then
        $ECHO "Database $dbName is not found in config file $APP_CONF_FILE."
        return
    fi 

    grep -v $cfgEntry $APP_CONF_FILE > $tempfile1
    newCfgEntry=`echo $cfgEntry | sed 's/YES/NO/g'`
    echo $newCfgEntry >> $tempfile1
    mv -f $tempfile1 $APP_CONF_FILE

    return 
}

EnableRegisteredDb()
{
    dbName=$1
    tempfile1=/tmp/appwizconf.$$

    cfgEntry=`grep OracleDatabase=NO:$dbName: $APP_CONF_FILE`
    
    if [ -z "$cfgEntry" ]; then
        $ECHO "Database $dbName is not found in config file $APP_CONF_FILE."
        return
    fi 

    grep -v $cfgEntry $APP_CONF_FILE > $tempfile1
    newCfgEntry=`echo $cfgEntry | sed 's/NO/YES/g'`
    echo $newCfgEntry >> $tempfile1
    mv -f $tempfile1 $APP_CONF_FILE

    return 
}

UnregisterDb()
{
    dbName=$1
    tempfile1=/tmp/appwizconf.$$

    cfgEntry=`grep -w $dbName $APP_CONF_FILE | grep "OracleDatabase="`
    
    if [ -z "$cfgEntry" ]; then
        $ECHO "Database $dbName is not found in config file $APP_CONF_FILE."
        return
    fi 

    grep -v $cfgEntry $APP_CONF_FILE > $tempfile1
    mv -f $tempfile1 $APP_CONF_FILE

    return 
}


RegisterNewDb()
{
    for line in `cat $ORATAB | grep -v "^#" | grep -v "^$" | grep -v "^*:" | grep -v "+ASM" | awk -F# '{print $1}'`
    do
        DB_NAME=`echo $line | cut -d: -f1`

        if [ ! -z "$DB_NAME" ]; then

            grep OracleDatabase= $APP_CONF_FILE | grep -w $DB_NAME > /dev/null
            if [ $? -ne 0 ]; then
                NEW_DB_NAMES="$NEW_DB_NAMES $DB_NAME"
            fi

        fi
    done

    if [ -z "$NEW_DB_NAMES" ]; then
        $ECHO "There are no unconfigured Oracle database entries found in $ORATAB file."
        return
    fi

    $ECHO "Found the following unconfigured database names in $ORATAB."
    $ECHO "    $NEW_DB_NAMES"
    $ECHO

    ReadAns "Would you like to enable discovery for all the above databases?"

    if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then

        for DB_NAME in $NEW_DB_NAMES
        do
            DB_LOC=`cat $ORATAB | grep -v "^#" | grep -v "^*:" | grep -v "^$" | grep -w $DB_NAME | cut -d: -f2`

            if [ ! -z "$DB_NAME" -a -d "$DB_LOC" ]; then
                WriteToConfFile "OracleDatabase=YES:$DB_NAME:$DB_LOC"
            fi
        done

    elif [ "$ANS" = "n" -o "$ANS" = "N" ]; then

        $ECHO "Please specify the databases you would like to enable for discovery."
        for DB_NAME in $NEW_DB_NAMES
        do
            DB_LOC=`cat $ORATAB | grep -v "^#" | grep -v "^*:" | grep -w $DB_NAME | cut -d: -f2`

            if [ ! -z "$DB_NAME" -a -d "$DB_LOC" ]; then
                $ECHO
                ReadAns "Would you like to enable discovery for database '$DB_NAME'?"

                if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then
                    WriteToConfFile "OracleDatabase=YES:$DB_NAME:$DB_LOC"
                fi

                if [ "$ANS" = "n" -o "$ANS" = "N" ]; then
                    WriteToConfFile "OracleDatabase=NO:$DB_NAME:$DB_LOC"
                fi
            fi
        done

    fi
    return
}

EnableRegisteredOraHome()
{
    oraHome=$1
    tempfile1=/tmp/appwizconf.$$

    cfgEntry=`grep -w $oraHome $APP_CONF_FILE | grep "OracleHome="`
    
    if [ -z "$cfgEntry" ]; then
        $ECHO "ORACLE_HOME $oraHome is not found in config file $APP_CONF_FILE."
        return
    fi 

    grep -v $cfgEntry $APP_CONF_FILE > $tempfile1
    newCfgEntry=`echo $cfgEntry | sed 's/NO/YES/g'`
    echo $newCfgEntry >> $tempfile1
    mv -f $tempfile1 $APP_CONF_FILE

    return 
}

DisableRegisteredOraHome()
{
    oraHome=$1
    tempfile1=/tmp/appwizconf.$$

    cfgEntry=`grep -w $oraHome $APP_CONF_FILE | grep "OracleHome="`
    
    if [ -z "$cfgEntry" ]; then
        $ECHO "ORACLE_HOME $oraHome is not found in config file $APP_CONF_FILE."
        return
    fi 

    grep -v $cfgEntry $APP_CONF_FILE > $tempfile1
    newCfgEntry=`echo $cfgEntry | sed 's/YES/NO/g'`
    echo $newCfgEntry >> $tempfile1
    mv -f $tempfile1 $APP_CONF_FILE

    return 
}

UnregisterOraHome()
{
    oraHome=$1
    tempfile1=/tmp/appwizconf.$$

    cfgEntry=`grep -w $oraHome $APP_CONF_FILE | grep "OracleHome="`
    
    if [ -z "$cfgEntry" ]; then
        $ECHO "ORACLE_HOME $oraHome is not found in config file $APP_CONF_FILE."
        return
    fi 

    grep -v $cfgEntry $APP_CONF_FILE > $tempfile1
    mv -f $tempfile1 $APP_CONF_FILE

    return 
}

RegisterNewOraHome()
{
    for oraHome in `cat $ORATAB | grep -v "^#" | grep -v "^$" | grep -v "^*:" | grep -v "+ASM" | awk -F: '{print $2}' | sort | uniq`
    do
        if [ ! -z "$oraHome" ]; then

            grep OracleHome= $APP_CONF_FILE | grep $oraHome > /dev/null
            if [ $? -ne 0 ]; then
                NEW_ORA_HOMES="$NEW_ORA_HOMES $oraHome"
            fi

        fi
    done

    if [ -z "$NEW_ORA_HOMES" ]; then
        $ECHO "There are no unconfigured Oracle homes entries found in $ORATAB file."
        QueryOracleHomes 
        return
    fi

    uniqOraHomes=`cat $ORATAB | grep -v "^#" | grep -v "^$" | grep -v "^*:" | grep -v "+ASM" | awk -F: '{print $2}' | sort | uniq`

    for oraHome in $uniqOraHomes
    do
        if [ ! -z "$oraHome" ]; then

            grep OracleHome= $APP_CONF_FILE | grep $oraHome > /dev/null
            if [ $? -ne 0 ]; then

                ReadAns "Would you like '$oraHome' to be used as ORACLE_HOME during failover?"

                if [ "$ANS" = "y" -o "$ANS" = "Y" ]; then
                    ValidateOracleHome $oraHome
                fi

                if [ "$ANS" = "n" -o "$ANS" = "N" ]; then
                    WriteToConfFile "OracleHome=NO:$oraHome"
                fi
            fi
        fi
    done

    return
}

DisplayConfig()
{
    if [ ! -f "$APP_CONF_FILE" ]; then
        $ECHO
        $ECHO "Config file $APP_CONF_FILE not found"
        $ECHO
        return
    fi

    enabledDbNames=`grep OracleDatabase=YES $APP_CONF_FILE |  awk -F: '{print $2}'`
    disabledDbNames=`grep OracleDatabase=NO $APP_CONF_FILE |  awk -F: '{print $2}'`
    enabledOraHomes=`grep OracleHome=YES $APP_CONF_FILE |  awk -F: '{print $2}'`
    disabledOraHomes=`grep OracleHome=NO $APP_CONF_FILE |  awk -F: '{print $2}'`

    if [ -z "$enabledDbNames" -a -z "$disabledDbNames" ]; then
        $ECHO
        $ECHO "The are no Oracle databases currently registered"
        $ECHO
    else
        if [ ! -z "$enabledDbNames" ]; then
            $ECHO
            $ECHO "The following Oracle databases are currently registered and enabled for discovery"
            $ECHO
            $ECHO "$enabledDbNames"
            $ECHO
        fi

        if [ ! -z "$disabledDbNames" ]; then
            $ECHO
            $ECHO "The following Oracle databases are currently registered and disabled for discovery"
            $ECHO
            $ECHO "$disabledDbNames"
            $ECHO
        fi 
    fi

    if [ -z "$enabledOraHomes" -a -z "$disabledOraHomes" ]; then
        $ECHO
        $ECHO "The are no ORACLE_HOMEs currently registered"
        $ECHO
    else

        if [ ! -z "$enabledOraHomes" ]; then
            $ECHO
            $ECHO "The following ORACLE_HOMEs are currently registered and enabled"
            $ECHO
            $ECHO "$enabledOraHomes"
            $ECHO
        fi

        if [ ! -z "$disabledOraHomes" ]; then
            $ECHO
            $ECHO "The following ORACLE_HOMEs are currently registered and disabled"
            $ECHO
            $ECHO "$disabledOraHomes"
            $ECHO
        fi
    fi

}

########### Main ##########

trap Cleanup 0

if [ "$PLATFORM" = "Linux" -o "$PLATFORM" = "SunOS" -o "$PLATFORM" = "AIX" ]; then
    trap CleanupError 1 2 3 15
else
    $ECHO "Error: Unsupported Operating System $PLATFORM."
    exit 1
fi

if [ ! -f /usr/local/.vx_version ]; then
    $ECHO "Error: Agent installation not found."
    exit 1
fi

INSTALLPATH=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`
if [ -z "$INSTALLPATH" ]; then
    $ECHO "Error: Agent installation not found."
    exit 1
fi

ParseArgs $*

APP_CONF_FILE=$INSTALLPATH/etc/appwizard.conf

while [ -f $TMP_OUTPUT_FILE ]
do
    log "Error: appwizard is already running."
    sleep 2
done

> $TMP_OUTPUT_FILE

if [ "$PLATFORM" = "Linux" -o "$PLATFORM" = "AIX" ]; then
    ORATAB=/etc/oratab
elif [ "$PLATFORM" = "SunOS" ]; then
    ORATAB=/var/opt/oracle/oratab
fi

$ECHO
$ECHO
$ECHO "Welcome to application wizard!"
$ECHO

if [ $OPTION = "query" ]; then

    if [ -f $APP_CONF_FILE ]; then
        $ECHO "WARNING: Reinitializing the configuration file will erase the existing settings."
        ReadAns "Do you want to continue?"
        if [ $ANS = "n" -o $ANS = "N" ]; then
            exit 0
        fi
    fi

    > $APP_CONF_FILE
    QueryOracleDiscovery

elif [ $OPTION = "update" ]; then

    UpdateConfig

elif [ $OPTION = "display" ]; then

    DisplayConfig

fi

rm -f $TMP_OUTPUT_FILE &> $NULL

exit 0
