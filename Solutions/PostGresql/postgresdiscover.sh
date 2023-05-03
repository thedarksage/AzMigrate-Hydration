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

	[ ! -d $INMAGE_DIR/failover_data/pgres ] && mkdir $INMAGE_DIR/failover_data/pgres

	###checking whether service is running or not####
	/etc/init.d/postgresql status|grep -v "grep"|grep -i "running" >>/dev/null
	if [ $? -ne 0 ]
	then

		echo "Postgresql in not running..."
		echo "Please start it again and run the discovery"
		echo "Exiting..."
		exit 1
	else

		if [ -f /etc/redhat-release ]
		then
			PGDATA="/var/lib/pgsql"
			PGDATATEMP=`grep -v "#" /etc/init.d/postgresql|grep "PGDATA="|head -1|awk -F"=" '{ print $2 }'`
		elif [ -f /etc/SuSE-release ]
		then
			PGDATA="~postgres/data"
			PGDATATEMP=`grep -v "#" /etc/sysconfig/postgresql|grep "POSTGRES_DATADIR="|head -1|awk -F"=" '{ print $2 }'|sed 's/"//g'`

		fi
		if [ "$PGDATA" != "$PGDATATEMP" ]
		then

			PGDATA=$PGDATATEMP
		fi
		
		##PGDATA is location of postgresql files####
		echo $PGDATA
		cd $PGDATA
		rm -f $LOGDIR/df_koutput.txt
                i=1
                for p in `df -k .|grep -v "Mounted on"`
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

		PGDATAFS=`awk '{ print $NF}' $LOGDIR/df_koutput.txt`
		rm -f $LOGDIR/df_koutput.txt
		echo "POSTGRES DATA DIRECTORY = $PGDATA"
		echo "POSTGRES FILESYSTEM = $PGDATAFS"
		if [ "$PGDATAFS" = "/" ]
		then


			echo "Please move postgres out of / volume."
			echo "Protecting postgres using Scout with Postgres data directory as '"/"'is not supported"
			exit 1
		fi
		echo "Collecting required information..."
		sleep 2
		cp -f postgresql.conf pg_hba.conf pg_ident.conf $PGINMAGEDATA/
		echo "done.."
		echo "PGDATA=$PGDATA" >$PGINMAGEDATA/pg_config
		echo "PGDATAFS=$PGDATAFS" >>$PGINMAGEDATA/pg_config

	fi
