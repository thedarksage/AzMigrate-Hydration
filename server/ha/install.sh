#!/bin/sh

# FUNCTION : Carries out a command and logs output into an install log
DO()
{
 eval "$1 2>> $INST_LOG | tee -a $INST_LOG"
}

# FUNCTION : Echoes a command into an install log
LOG()
{
 echo "$1" >> $INST_LOG
}

cur_wd=`pwd`
base_dir=/home/svsystems

SetFolderPermissions()
{
    # Giving folder permissions accordingly as suggested by CX team.
    chmod -R 755 /home/svsystems/pm/Fabric
    chmod -R 777 /home/svsystems/ha/{backup,restore}
    chmod -R 750 /home/svsystems/etc/amethyst.conf
    setfacl -R -m u:apache:rwx /home/svsystems/etc >> $INST_LOG 2>&1
    setfacl -R -m g:apache:rwx /home/svsystems/etc >> $INST_LOG 2>&1
}


#####################################
# Fresh Installation of the CX HA
#####################################

Fresh_Install()
{
  for RpmName in heartbeat-2.1 libnet-1.1 pils-2.1 stonith-2.1
  do
    if rpm -qa | grep -q $RpmName ; then
        LOG "$RpmName is available"
    else
        DO 'echo "$RpmName is missing."'
        ChkStatus=1        
    fi
  done
  [ "$ChkStatus" = "1" ] && DO 'echo "Install missing CX-HA pre-requisite rpm and re-try."' && exit 1
  
  # If both CS_IP and PS_CS_IP are same, Stop the FX/VX services if exists.
  CS_IP=`grep -w CS_IP /home/svsystems/etc/amethyst.conf | tr -d ' '| cut -d"=" -f2`
  PS_CS_IP=`grep -w PS_CS_IP /home/svsystems/etc/amethyst.conf | tr -d ' '| cut -d"=" -f2`
  if [ "${CS_IP}" = "${PS_CS_IP}" ]; then
      if [ -e /usr/local/.fx_version ]; then
          FX_INSTALLATION_DIR=`grep ^INSTALLATION_DIR /usr/local/.fx_version | awk -F= '{ print $2 }'`
          if [ -e ${FX_INSTALLATION_DIR}/stop -a -e ${FX_INSTALLATION_DIR}/config.ini ]; then
              DO 'echo "Stopping the FX service(this takes a while)..."'
              ${FX_INSTALLATION_DIR}/stop >> $INST_LOG 2>&1
              FX_STOP_EXIT_CODE=$?
              if [ "$FX_STOP_EXIT_CODE" -eq 0 ]; then
                  DO 'echo "FX service is stopped completely..."'
                  FX_STATUS=1
              elif [ "$FX_STOP_EXIT_CODE" -eq 2 ]; then
                  DO 'echo "Could not stop FX service. Could not proceed with installation. Aborting..."'
                  exit 1
              fi
          fi
      fi
      if [ -e /usr/local/.vx_version ]; then
          VX_INSTALLATION_DIR=`grep ^INSTALLATION_DIR /usr/local/.vx_version | awk -F= '{ print $2 }'`
          if [ -e ${VX_INSTALLATION_DIR}/bin/stop -a -e ${VX_INSTALLATION_DIR}/etc/drscout.conf ]; then
              DO 'echo "Stopping the VX service(this takes a while)..."'
              ${VX_INSTALLATION_DIR}/bin/stop >> $INST_LOG 2>&1
              VX_STOP_EXIT_CODE=$?
              if [ "$VX_STOP_EXIT_CODE" -eq 0 ]; then
                  DO 'echo "VX service is stopped completely..."'
                  VX_STATUS=1
              else
                  DO 'echo "Could not stop VX service. Could not proceed with installation. Aborting..."'
                  exit 1
              fi
          fi
      fi
  fi    
  
  # Installing the HA build.
  DO 'echo "Starting installation of HA server"'
  
  # Creating the directories if not exists
  mkdir -p ${base_dir}/ha/{backup,restore} ${base_dir}/pm/Fabric >/dev/null 2>&1
  
  # Copying the related files
  LOG "Copying the files db_sync_tgt_post_script.pl, db_sync_src_pre_script.pl, cxfailover.pl, node_fail_event_track to ${base_dir}/bin/"
  cp db_sync_tgt_post_script.pl db_sync_src_pre_script.pl cxfailover.pl node_fail_event_track ${base_dir}/bin/ >/dev/null 2>&1
  cp Constants.pm ${base_dir}/pm/Fabric >/dev/null 2>&1
  cp cxfailoverd /etc/init.d >/dev/null 2>&1 ; chmod 755 /etc/init.d/cxfailoverd >/dev/null 2>&1
  chmod 755 *.sh setresync >/dev/null 2>&1
  
  mkdir -p /home/svsystems/pm/ConfigServer/HighAvailability
  cp `pwd`/HighAvailability/*.pm /home/svsystems/pm/ConfigServer/HighAvailability
  
  cp uninstall.sh /home/svsystems/bin/uninstall_ha.sh >/dev/null 2>&1
  cp haresources /etc/ha.d/ >/dev/null 2>&1
  chmod 755 /home/svsystems/bin/uninstall_ha.sh >/dev/null 2>&1
  
  # Executing the configure script in the ha_module
  ./configure.sh
  
  # Calling SetFolderPermissions function
  SetFolderPermissions
  
  # Starting the FX/VX services
  if [ "$FX_STATUS" = "1" ]; then
      DO 'echo "Starting the FX service..."'
      ${FX_INSTALLATION_DIR}/start >> $INST_LOG 2>&1
      [ $? -eq 0 ] && DO 'echo "FX service started successfully"' || DO '"Failed to start FX service..."'
  fi
  if [ "$VX_STATUS" = "1" ]; then
      DO 'echo "Starting the VX service..."'
      ${VX_INSTALLATION_DIR}/bin/start >> $INST_LOG 2>&1
      [ $? -eq 0 ] && DO 'echo "VX service started successfully"' || DO '"Failed to start VX service..."'
  fi
}

###############################
# Upgradation of the CX HA
###############################

Upgrade()
{
  DO 'echo "Previous installation of HA Server detected ..."'
  DO 'echo "Upgrading HA Server ..."'

  for RpmName in heartbeat-2.1 libnet-1.1 pils-2.1 stonith-2.1
  do
    if rpm -qa | grep -q $RpmName ; then
        LOG "$RpmName is available"
    else
        DO 'echo "$RpmName is missing."'
        ChkStatus=1        
    fi
  done
  [ "$ChkStatus" = "1" ] && DO "Install missing CX-HA pre-requisite rpm and re-try." && exit 1
  
  for directory in ${base_dir}/pm/ConfigServer ${base_dir}/pm/ConfigServer/HighAvailability ${base_dir}/ha/restore ${base_dir}/ha/backup /etc/ha.d ${base_dir}/pm/Fabric
  do
	if [ ! -d $directory ]; then mkdir -p $directory ; fi
  done

  cp report_global_resync.php ${base_dir}/admin/web/
  chmod 755 ${base_dir}/admin/web/report_global_resync.php 
  cp -f db_sync_tgt_post_script.pl db_sync_src_pre_script.pl cxfailover.pl node_fail_event_track ${base_dir}/bin/
  cp Constants.pm ${base_dir}/pm/Fabric

  cp HighAvailability/*.pm ${base_dir}/pm/ConfigServer/HighAvailability

  # Taking the backup of the ha.cf and haresources file and copying the latest files
  DO 'echo "Taking the backup of the HA configuration file ..."'
  cp /etc/ha.d/ha.cf /etc/ha.d/ha.cf.orig.save
  mkdir -p /home/backup/ 2>/dev/null
  cp /etc/ha.d/haresources /home/backup/haresources.orig.save

  cp -f ha.cf /etc/ha.d/ha.cf
  cp node_ntw_fail ${base_dir}/bin/
  cp nodentwd /etc/init.d/
  cp -f authkeys /etc/ha.d/authkeys
  chmod 600 /etc/ha.d/authkeys
  chmod +x /etc/init.d/nodentwd

  # Comparing the old HA configuration file with the new HA configuration file
  # Adding the new entries that are added in the latest configuration file and retaining the old values

  OLD_CONF_FILE=/etc/ha.d/ha.cf.orig.save
  NEW_CONF_FILE=/etc/ha.d/ha.cf

  while read line
  do
        parameter=`echo $line | cut -d" " -f1`
        cat $OLD_CONF_FILE | grep -qw "$parameter"

        if [ $? -eq 0 ]; then
                :
        else
                echo "$line" >> $OLD_CONF_FILE
        fi
  done < $NEW_CONF_FILE
  mv $OLD_CONF_FILE $NEW_CONF_FILE

  # Copying the latest uninstall script of the HA and giving the execute permissions.
  cp uninstall.sh ${base_dir}/bin/uninstall_ha.sh
  chmod 755 ${base_dir}/bin/uninstall_ha.sh

  cp cxfailoverd /etc/init.d
  chmod 755 /etc/init.d/cxfailoverd

  rm -f /etc/init.d/setresync

  # Getting the hostname from /etc/ha.d/haresources and comparing with hostname
  hostname_hares=`cat /etc/ha.d/haresources | cut -d" " -f1`

  # Calling SetFolderPermissions function
  SetFolderPermissions

  # Restarting the tmanagerd services.
  DO 'echo "Restarting the tmanagerd services ..."'
  /etc/init.d/tmanagerd restart
}

# Check if CX is installed in the system or not before starting the HA Installation
CheckPreviousCXInstallation()
{
  CX_RPM=`rpm -qa | grep -i inmageCX`

  if [ -z "$CX_RPM" ]; then
  	echo "CX server should be installed for starting HA Installation ..."
  	echo "Please install CX server and try again..."
  	exit 1
  fi
}

# Check if CX-HA is installed or not.
CheckPreviousHAInstallation()
{
  if [ -e /etc/init.d/cxfailoverd -a -e /etc/init.d/nodentwd ]; then
	INST_LOG=/var/log/cx_upgrade.log
  	Upgrade
  else
	INST_LOG=/var/log/cx_install.log
  	Fresh_Install
  fi
}

###################################
# MAIN ENTRY POINT
###################################

CheckPreviousCXInstallation
CheckPreviousHAInstallation


