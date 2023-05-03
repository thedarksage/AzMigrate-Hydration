#!/bin/ksh

        INMAGE_DIR=`grep INSTALLATION_DIR /usr/local/.vx_version | cut -d'=' -f2`
        SCRIPTS_DIR=$INMAGE_DIR/scripts
        LOGDIR=$INMAGE_DIR/failover_data
        TEMP_FILE="$LOGDIR/.files"
        MOUNT_POINTS="$LOGDIR/.mountpoint"
        ERRORFILE=$LOGDIR/errorfile.txt
        ALERT=$INMAGE_DIR/bin/alert
        programname=$0
        WITHOUT_RETENTION=0

        ###POSTGRES Related Entries###
        PGINMAGEDATA=$INMAGE_DIR/failover_data/pgres
	
	
	if [ ! -f $PGINMAGEDATA/pg_config ]
	then

		echo "Please run Discovery first..then run consistency"
		echo "Aborting..."
		exit 1
	fi


	PGDATA=`head -1 $PGINMAGEDATA/pg_config|awk -F"=" '{print $2}'`
	echo "PostGres using following filesystem(s)"
	PGDATAFS=`grep PGDATAFS $PGINMAGEDATA/pg_config|awk -F"=" '{ print $2 }'`
	echo $PGDATAFS
	echo "Checking whether WAL is enabled or not"
	sleep 2
	grep -v "^#" $PGDATA/postgresql.conf|grep -v grep|grep "archive_command" >>/dev/null
	if [ $? -ne 0 ]
	then
		echo "creating WAL archive directory"
		mkdir -p /backup/InMage/WAL
		chown postgres /backup/InMage/WAL
		echo "archive_command = 'cp -i %p /backup/InMage/WAL/%f < /dev/null'" >>$PGDATA/postgresql.conf
	else
		echo "Checking for WAL archive log directory"
		[ ! -d /backup/InMage/WAL ] && mkdir /backup/InMage/WAL
	fi
	tag1=Tag_`date +%Y%m%d%H%M%S`
	
	echo "Changing database in backup mode.."
	su - postgres -c "psql -f $INMAGE_DIR/scripts/postgresbegin.sql"
	if [ $? -ne 0 ]
	then

		echo "database start backup failed"
		exit 1
	fi
	rm -f $LOGDIR/df_koutput.txt
	i=1
        for p in `df -k |grep -v "Mounted on"`
        do
             echo -n "$p " >> $LOGDIR/df_koutput.txt
             i=`expr $i + 1`
             x=`expr $i % 7` >>/dev/null
             if [ $x  -eq 0 ]
             then
                     echo "" >> $LOGDIR/df_koutput.txt
                     i=1
              fi
        done
	FS=`grep $PGDATAFS $LOGDIR/df_koutput.txt|awk '{print $1 }'`
	echo "FS is == $FS"
	rm -f /tmp/vacpoutput /tmp/vacperror
	$INMAGE_DIR/bin/vacp -v $FS -t $tag1 1>>/tmp/vacpoutput 2>>/tmp/vacperror
	if [ $? -ne 0 ]
	then
		cat /tmp/vacpoutput
		echo "vacp error..."
	else
		cat /tmp/vacpoutput
		grep -v "Tag" $PGINMAGEDATA/pg_config >/tmp/inmagefile
		mv /tmp/inmagefile $PGINMAGEDATA/pg_config
		echo "Tag=$tag1" >>$PGINMAGEDATA/pg_config
	fi
	echo "changing database to normal mode.."
	su - postgres -c "psql -f $INMAGE_DIR/scripts/postgresend.sql"
	if [ $? -ne 0 ]
	then

		echo "database end backup failed"
		exit 1
	fi
	##deleting data from WAL archives####
	rm -f /backup/InMage/WAL/*
	exit 0
