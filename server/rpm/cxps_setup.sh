#!/bin/sh

# script to turn cxps on/off
#
# usage: cxps_setup.sh on | off
#
# note:
#  after turring the cxps on you must run
#
#       cxps_db_setup.sh on 
#
#  on the configuration server to make sure
#  existing and new pairs switch to the cxps
#
# before turning cxps off, you must run
#
#       cxps_db_setup.sh off
#
#  on the configuration server to make sure
#  existing and new pairs switch back to using FTP
#  as the cxps will not be available
#

CXPSCTL="/etc/init.d/cxpsctl"

AMETHYST_CONF=`dirname $0`/../etc/amethyst.conf

if [ "on" = "$1" ] ; then 
    # set up links
	 for i in {0,1,2,3,4,5,6}
	 do
		  [ ! -h /etc/rc.d/rc${i}.d/S84cxpsctl ] && ln -s $CXPSCTL /etc/rc.d/rc${i}.d/S84cxpsctl
		  [ ! -h /etc/rc.d/rc${i}.d/K84cxpsctl ] && ln -s $CXPSCTL /etc/rc.d/rc${i}.d/K84cxpsctl
	 done

    # set to start on reboot
	 chkconfig cxpsctl on

    # start
	 $CXPSCTL start

	 # update amethyst.conf to use the cxps xfer log file
	 XFER_LOG=/home/svsystems/transport/log/cxps.xfer.log
	 CXPS_CONF=`grep "CXPS_CONF=" /etc/init.d/cxpsctl  | cut -d '=' -f2`
	 if [ -f $CXPS_CONF ] ; then
		  dos2unix $CXPS_CONF
		  CXPS_INSTALL=`grep "^install_dir[ ]*=" $CXPS_CONF | cut -d '=' -f2`
		  TMP_XFER_LOG=`grep "^xfer_log[ ]*=" $CXPS_CONF | cut -d '=' -f2`
		  if [ ! "" = "$TMP_XFER_LOG" ] ; then
				FULL_PATH=`echo $TMP_XFER_LOG | grep "^/"`
				if [ "" = "$FULL_PATH" ] ; then
					 XFER_LOG=$CXPS_INSTALL/$TMP_XFER_LOG
				else
					 XFER_LOG=$CXPS_INSTALL/log/cxps.xfer.log
				fi		  
		  else 
				XFER_LOG=$CXPS_INSTALL/log/cxps.xfer.log
		  fi
	 fi	 
	 cat ${AMETHYST_CONF} | sed -e 's/^CXPS_XFER.*$/CXPS_XFER = 1/g' | sed -e "s|XFER_LOG_FILE[ ]*=.*$|XFER_LOG_FILE =$XFER_LOG|g" > ${AMETHYST_CONF}.new
	 cp -f ${AMETHYST_CONF} ${AMETHYST_CONF}.bak
	 cp -f ${AMETHYST_CONF}.new ${AMETHYST_CONF}

	 echo ""
	 echo "make sure to run the cxps_db_setup on the configuration server."
	 echo "If you have HA enabled, run it on the current active configuration server."
	 echo ""
else 
	 if [ "off" = "$1" ] ; then
		  
        # make sure they switched the db back to ftp
		  while true ; do
				echo ""
				read -p "You must run \"cxps_db_setup off\" before turning off cxps. Continue [y/n]: " ok
				case $ok in
					 y|Y)
						  break
						  ;;
					 n|N)
						  exit 0
						  ;;
				esac
		  done
		  
		  # update amethyst.conf to use xfer log
		  cat ${AMETHYST_CONF} | sed -e 's/^CXPS_XFER.*$/CXPS_XFER = 0/g' | sed -e "s|XFER_LOG_FILE[ ]*=.*$|XFER_LOG_FILE = /var/log/xferlog|g" > ${AMETHYST_CONF}.new		  
		  cp -f ${AMETHYST_CONF} ${AMETHYST_CONF}.bak
		  cp -f ${AMETHYST_CONF}.new ${AMETHYST_CONF}

        # do not  start on reboot
		  chkconfig cxpsctl off		  
		  for i in {0,1,2,3,4,5,6}
		  do
				if [ -h /etc/rc.d/rc${i}.d/S84cxpsctl ] ; then
					 rm /etc/rc.d/rc${i}.d/S84cxpsctl
				fi
				
				if [ -h /etc/rc.d/rc${i}.d/K84cxpsctl ] ; then
					 rm /etc/rc.d/rc${i}.d/K84cxpsctl
				fi
		  done

        # should cxps be stopped
		  ok=n
		  while true ; do
				echo ""
				echo "Stop the cxps now (stopping cxps may cause host agents to report transfer errors"
				read -p "you should only do this if it is causing system issues) [n]: " ok
				case $ok in
					 y|Y)
                    # stop
						  $CXPSCTL stop
						  break
						  ;;
					 n|N)
						  break
						  ;;
				esac
		  done
	 else
		  echo "usage: cxps_setup on | off"
	 fi
fi

echo " "
