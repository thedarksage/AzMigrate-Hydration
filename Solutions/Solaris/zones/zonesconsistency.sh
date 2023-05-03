#!/bin/ksh 

    ZONE_CONSISTENCY=0
    APP_CONSISTENCY=0
    TEMP_FILE="$LOGDIR/.files.tmp"
    TEMP_FILE1="$LOGDIR/.files1"
    Resources="$LOGDIR/.resources"
    ResourcesTmp="$LOGDIR/.resourcestmp"

    Usage()
    {
        echo "Usage: $0 -a applications -c config -s scope -z zones [-e execution]"
        echo " "
        echo " where applications can be comma separated list of following keywords"
        echo "      oradb               -- To issue bookmarks on oracle databases inside the non-global zone"
        echo "      none                -- To not include any applications while issuing bookmarks"
        echo " "
        echo " where config can be"
        echo "      yes                 -- To issue bookmarks on zone config files"
        echo "      no                  -- To not issue bookmarks on zone config files"
        echo " "
        echo " where scope can be the following keyworkds"
        echo "      global              -- To issue bookmarks in global zone"
        echo "      local               -- To issue bookmarks in non-global zones"
        echo " "
        echo " where zones can be comma separated list of non-global zone names"
        echo "      all                 -- To issue bookmarks on all non-zones"
        echo "      [zones]             -- To issue bookmarks on listed non-global zones"
        echo " "
        echo " where execution can be the following keyworkds"
        echo "      pre                 -- Pre execution script to be executed before issuing bookmarks"
        echo "      post                -- Post execution script to be executed after issuing bookmarks"
        echo " "
        exit 1
    }

    GlobalConsistency()
    {
        zoneadm list -cpi|grep -v "global"|awk -F":" '{ print $2 "  " $3 "   " $4 }' >$TEMP_FILE
        if [ -f $Resources ]
        then
            rm -f $Resources
        fi
        
        ####Collecting hard disk resources for all the zones
        
        cat $TEMP_FILE|while read x
        do

            ZoneName=`echo $x|awk '{ print $1 }'`
            Status=`echo $x|awk '{ print $2 }'`
            ZonesDirectory=`echo $x |awk '{ print $3 }'`

            INCLUDE=0
            if [ $ALL_ZONES -eq 0 ]; then
                for zone in `echo $ZONES | sed 's/,/ /g'`
                do
                    if [ "$ZoneName" = "$zone" ]; then
                        INCLUDE=1
                    fi
                done
            else
                INCLUDE=1
            fi

            if [ $INCLUDE -eq 1 ]; then
                zonecfg -z $ZoneName export -f $TEMP_FILE1
                
                egrep "special|match" $TEMP_FILE1|grep -v "usr"| grep -v "/dev/rdsk" | grep -v "involflt" | awk -F"=" '{ print $2 }' >>$Resources

                if [ $CONFIG = "yes" ]; then
                    cd $ZonesDirectory
                    df -k . |tail -1 |awk '{ print $1 }' | grep "/dev/" >>$Resources
                fi
            fi
        done

        grep "/dev/" $Resources >$ResourcesTmp

        cat $Resources|grep -v "/dev/"|while read x
        do
            cd  $x
            df -k . |tail -1 |awk '{ print $1 }' | grep "/dev/" >> $ResourcesTmp
        done

        mv $ResourcesTmp $Resources

        if [ "$EXEC" = "pre" ];then
            for x in `cat $TEMP_FILE|awk '{ print $1 }'`
            do
                ZoneName=$x
                INCLUDE=0
                if [ $ALL_ZONES -eq 0 ]; then
                   for zone in `echo $ZONES | sed 's/,/ /g'`
                   do
                      if [ "$ZoneName" = "$zone" ]; then
                          INCLUDE=1
                      fi
                   done
                else
                    INCLUDE=1
                fi

                if [ $INCLUDE -eq 1 ]; then
                    if [ $ORACLE -eq 1 ]; then
                        echo "Checking for oracle databases in $ZoneName"

                        zlogin $ZoneName ps -ef | grep ora_pmon_ | grep -v grep >>/dev/null
                        if [ $? -eq 0 ]
                        then
                             echo "Oracle database(s) running in $ZoneName"
                             echo "Running database freeze commands"
                             zlogin  $ZoneName $INMAGE_DIR/scripts/zones/oraclebeginbackupmain.sh
                        else
                             echo "No running database found. Skipping database freeze"
                        fi
                    fi
                fi
            done
        elif [ "$EXEC" = "post" ];then
             for x in `cat $TEMP_FILE|awk '{ print $1 }'`
             do
                ZoneName=$x
                INCLUDE=0
                if [ $ALL_ZONES -eq 0 ]; then
                    for zone in `echo $ZONES | sed 's/,/ /g'`
                    do
                       if [ "$ZoneName" = "$zone" ]; then
                           INCLUDE=1
                       fi
                    done
                else
                    INCLUDE=1
                fi
                if [ $INCLUDE -eq 1 ]; then
                    if [ $ORACLE -eq 1 ]; then
                        echo "Checking oracle databases in $ZoneName"
                        zlogin $ZoneName ps -ef | grep ora_pmon_ | grep -v grep >>/dev/null
                        if [ $? -eq 0 ]
                        then
                             echo "Oracle database(s) running in $ZoneName"
                             echo "Running database unfreeze commands"
                             zlogin  $ZoneName $INMAGE_DIR/scripts/zones/oracleendbackupmain.sh
                        else
                             echo "No running database found. Skipping database unfreeze"
                        fi
                    fi
                fi
             done
        else
            for x in `cat $TEMP_FILE|awk '{ print $1 }'`
            do
               ZoneName=$x
               INCLUDE=0
               if [ $ALL_ZONES -eq 0 ]; then
                   for zone in `echo $ZONES | sed 's/,/ /g'`
                   do
                      if [ "$ZoneName" = "$zone" ]; then
                          INCLUDE=1
                      fi
                   done
               else
                   INCLUDE=1
               fi

               if [ $INCLUDE -eq 1 ]; then
                   if [ $ORACLE -eq 1 ]; then
                       echo "Checking for oracle databases in $ZoneName"
 
                       zlogin $ZoneName ps -ef | grep ora_pmon_ | grep -v grep >>/dev/null
                       if [ $? -eq 0 ]
                       then
                           echo "Oracle database(s) running in $ZoneName"
                           echo "Running database freeze commands"
                           zlogin  $ZoneName $INMAGE_DIR/scripts/zones/oraclebeginbackupmain.sh
                       else
                           echo "No running database found. Skipping database freeze"
                       fi
                   fi
               fi 
            done


            ###Issuing vacp tag on all the resources####
            #sed 's/$/,/g' $Resources >$TEMP_FILE
            Volumes=`cat $Resources | sort | uniq`
            VACPCOMMANDOPTION=`echo $Volumes|/usr/xpg4/bin/awk '{ for(i=1;i<=NF;i++) printf("%s,",$i)}'`
            $INMAGE_DIR/bin/vacp -v $VACPCOMMANDOPTION
 
 
            ####Keeping database in end backup mode#########
 
      
            for x in `cat $TEMP_FILE|awk '{ print $1 }'`
            do
               ZoneName=$x
               INCLUDE=0
               if [ $ALL_ZONES -eq 0 ]; then
                   for zone in `echo $ZONES | sed 's/,/ /g'`
                   do
                      if [ "$ZoneName" = "$zone" ]; then
                          INCLUDE=1
                      fi
                   done
               else
                   INCLUDE=1
               fi

               if [ $INCLUDE -eq 1 ]; then
                   if [ $ORACLE -eq 1 ]; then
                       echo "Checking oracle databases in $ZoneName"
 
                       zlogin $ZoneName ps -ef | grep ora_pmon_ | grep -v grep >>/dev/null
                       if [ $? -eq 0 ]
                       then
                           echo "Oracle database(s) running in $ZoneName"
                           echo "Running database unfreeze commands"	
                           zlogin  $ZoneName $INMAGE_DIR/scripts/zones/oracleendbackupmain.sh
                       else
                           echo "No running database found. Skipping database unfreeze"
                       fi
                   fi
               fi
           done
        fi
    }

    LocalConsistency()
    {
        zoneadm list -cpi|grep -v "global"|awk -F":" '{ print $2 "  " $3 "   " $4 }' >$TEMP_FILE

        if [ -f $Resources ]
        then
            rm -f $Resources
        fi
        
        cat $TEMP_FILE|while read x
        do
            ZoneName=`echo $x|awk '{ print $1 }'`
            INCLUDE=0
            if [ $ALL_ZONES -eq 0 ]; then
                for zone in `echo $ZONES | sed 's/,/ /g'`
                do
                    if [ "$ZoneName" = "$zone" ]; then
                        INCLUDE=1
                    fi
                done
            else
                INCLUDE=1
            fi

            if [ $INCLUDE -eq 1 ]; then
                zonecfg -z $ZoneName export -f $TEMP_FILE1
                egrep "special|match" $TEMP_FILE1|grep -v "usr"| grep -v "/dev/rdsk"  | grep -v "involflt"| awk -F"=" '{ print $2 }' >>$Resources

                grep "/dev/" $Resources >$ResourcesTmp

                cat $Resources|grep -v "/dev/"|while read dir
                do
                    zlogin $ZoneName df -k $dir |tail -1 |awk '{ print $1 }' | grep "/dev/" >> $ResourcesTmp
                done

                mv $ResourcesTmp $Resources

                if [ "$EXEC" = "pre" ];then
                    if [ $ORACLE -eq 1 ]; then
                        echo "Checking for oracle databases in $ZoneName for freeze"

                        zlogin $ZoneName ps -ef | grep ora_pmon_ | grep -v grep >>/dev/null
                        if [ $? -eq 0 ]
                        then
                            echo "Oracle database(s) running in $ZoneName"
                            echo "Running database freeze commands"	
                            zlogin  $ZoneName $INMAGE_DIR/scripts/zones/oraclebeginbackupmain.sh
                        else
                            echo "No running database found. Skipping database freeze"
                        fi
                    fi
                elif [ "$EXEC" = "post" ];then
                      if [ $ORACLE -eq 1 ]; then
                          echo "Checking oracle databases in $ZoneName for unfreeze"
 
                          zlogin $ZoneName ps -ef | grep ora_pmon_ | grep -v grep >>/dev/null
                          if [ $? -eq 0 ]
                          then
                              echo "Oracle database(s) running in $ZoneName"
                              echo "Running database unfreeze commands"	
                              zlogin  $ZoneName $INMAGE_DIR/scripts/zones/oracleendbackupmain.sh
                          else
                              echo "No running database found. Skipping database unfreeze"
                          fi
                      fi
                else
                     if [ $ORACLE -eq 1 ]; then
                         echo "Checking for oracle databases in $ZoneName for freeze"

                         zlogin $ZoneName ps -ef | grep ora_pmon_ | grep -v grep >>/dev/null
                         if [ $? -eq 0 ]
                         then
                             echo "Oracle database(s) running in $ZoneName"
                             echo "Running database freeze commands"	
                             zlogin  $ZoneName $INMAGE_DIR/scripts/zones/oraclebeginbackupmain.sh
                         else
                             echo "No running database found. Skipping database freeze"
                         fi
                     fi


                     ###Issuing vacp tag inside the zone####
                     #sed 's/$/,/g' $Resources >$TEMP_FILE

                     Volumes=`cat $Resources | sort | uniq`
                     VACPCOMMANDOPTION=`echo $Volumes|/usr/xpg4/bin/awk '{ for(i=1;i<=NF;i++) printf("%s,",$i)}'`
                     zlogin $ZoneName $INMAGE_DIR/bin/vacp -v $VACPCOMMANDOPTION

                     ####Keeping database in end backup mode#########
 
            
                     if [ $ORACLE -eq 1 ]; then
                         echo "Checking oracle databases in $ZoneName for unfreeze"

                         zlogin $ZoneName ps -ef | grep ora_pmon_ | grep -v grep >>/dev/null
                         if [ $? -eq 0 ]
                         then
                             echo "Oracle database(s) running in $ZoneName"
                             echo "Running database unfreeze commands"	
                             zlogin  $ZoneName $INMAGE_DIR/scripts/zones/oracleendbackupmain.sh
                         else
                             echo "No running database found. Skipping database unfreeze"
                         fi
                     fi
                fi
            fi
        done
    }

    ############################ Main #################################
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


    if [ $# -ne 8 ]
    then
       if [ $# -ne 10 ]
       then
          Usage
       fi
    fi

    while getopts "a:c:s:z:e:" opt
    do
        case $opt in
        a )
            APPS=$OPTARG
            if [ $APPS != "oradb" -a $APPS != "none" ]
            then
                Usage
            fi
            ;;
        c )
            CONFIG=$OPTARG
            if [ $CONFIG != "yes" -a $CONFIG != "no" ]
            then
                Usage
            fi
            ;;
        s )
            SCOPE=$OPTARG
            if [ $SCOPE != "global" -a $SCOPE != "local" ]
            then
                Usage
            fi
            ;;
        z ) 
            ZONES=$OPTARG
            if [ $ZONES != "all" ]
            then
                if [ -z "$ZONES" ]
                then
                    Usage
                fi
            fi
            ;;
        e )
            EXEC=$OPTARG
            if [ $EXEC != "pre" -a $EXEC != "post" ]
            then
                Usage
            fi
            ;;
        \?) Usage ;;
        esac
    done

    if [ -z "$APPS" -o -z "$CONFIG" -o -z "$SCOPE" -o -z "$ZONES" ];then
         Usage
    fi

    ORACLE=0
    for app in `echo $APPS | sed 's/,/ /g'`
    do
        if [ "$app" = "oradb" ]; then
            ORACLE=1
        fi
    done

    ALL_ZONES=0
    if [ "$ZONES" = "all" ]; then
        ALL_ZONES=1
    fi

    if [ "$SCOPE" = "global" ]; then
        GlobalConsistency
    elif [ "$SCOPE" = "local" ]; then
        LocalConsistency
    fi
    
