#!/bin/sh
#
# Copyright (c) 2011 InMage
# This file contains proprietary and confidential information and
# remains the unpublished property of InMage. Use, disclosure,
# or reproduction is prohibited except as permitted by express
# written license aggreement with InMage.
#
# File       : collect_support_materials.sh
#
# Usage      : collect_support_materials.sh <directory-for-support-materials>
#
# Collect support materials for Unix based system
#
# Description:
# Script helps user to collect support materials on a given system based on Unix.
# It collects following information
# 1. Agent logs under /var/log
# 2. System logs user /var/
# 3. Get list of crash dumps if available
# 4. Get contents of persistent store
# 5. Collect release build information
# 6. drscout.conf
# 7. Get driver and target context stats 
# 8. Get Bitmap files on the system
# 9. Get dmesg output to a file
# 10. Get h/w , s/w configuration information
# 
# Author - Prashant Kharche (prkharch@inmage.com)


# Persisten tstore directory
LANG=en_US.UTF-8
export LANG
LC_NUMERIC=en_US.UTF-8
export LC_NUMERIC

pstore=/etc/vxagent/involflt
procstore=/proc/involflt
hostname="`hostname`"
filename="support-materials-$hostname-`date +'%b_%d_%H_%M_%S'`"
vx_dir=`grep INSTALLATION_DIR /usr/local/.vx_version | awk -F'=' '{print $2 }'`
inm_dmit=$vx_dir/bin/inm_dmit
inmage_logs="/var/log/s2* /var/log/svagent*  /var/log/cdpcli* /var/log/CDPMgr* /var/log/CacheMgr* /var/log/appservice* /var/log/involflt* /var/log/linvsnap* /var/log/InMage_Old_Logs $vx_dir/resync $vx_dir/AppliedInfo  /var/log/Scouttunning* /var/log/failovecommandutil* /var/log/cxpsclient* /var/log/vacps* /var/log/customdevicecli* 
/var/log/fabricagent* /var/log/oradisc* /var/log/db2out* /var/log/ua_uninstall* /var/log/InMage_drivers* /var/log/ua_install* /var/log/svagents* /var/log/AzureRcmCli* /var/log/syslog*"
get_scsid_cmd=/etc/vxagent/bin/inm_scsi_id
os=`uname -s`
script_path=/etc/init.d/
partition=""
inm_dbg_str=""
MirrorFilterDevType="5"
INVOLFLT_DRVNAME="involflt"
INVOLFLT_CTRLNODE="/dev/involflt"
src_drv_name=""
dst_drv_name=""
DF_COMMAND="df -hk"

# err logger for the script
inm_log() {
   inm_dbg_str="${inm_dbg_str} |"
   inm_date_str=`date +"%b %d %H:%M:%S"`
   inm_dbg_str="${inm_dbg_str} ${inm_date_str}"
   inm_dbg_str="${inm_dbg_str} $*"
}

# FUNCTION: Invokes inm_logger to write into kernel ring buffer
cleanup() {
   INM_LOGGER=/etc/init.d/inm_logger
   if [ -f $INM_LOGGER ] ; then
      $INM_LOGGER "inm_hotplug: " $inm_dbg_str
   else
      echo $inm_dbg_str
   fi
   inm_dbg_str=""
}

consistency_log()
{
    logPath=`grep -w LogPathname /usr/local/drscout.conf | cut -d= -f2`
    logDir="${logPath}/ApplicationPolicyLogs"
    if [ -d $logDir ]
    then
        
        # delete policy log files older than 12 hours
        find $logDir -mtime +0 -name "*olicy*.log" -delete

        echo
        echo "consistency_log START"
        find $logDir -mtime -0.01 -exec grep -l vacp {} \; | sort | xargs awk '
        BEGIN{
            IGNORECASE = 1
            fname="tmpfile"
        }
        function prtName() {
            if (fname!=FILENAME){
                "stat -c %y "FILENAME |getline fsize
                printf "\n\t%s, %s\n", FILENAME, fsize
                fname=FILENAME
            }
        }
        /Command Line/ || /Exiting/ || /failed/ || /Error/ {prtName(); printf "\t\t"; print}
        END{
        }'
        echo "consistency_log END"
    fi
}

check_openssl()
{
  # Check if OpenSSL is installed
  if ! command -v openssl > /dev/null 2>&1; then
    echo "Error: OpenSSL not found"
    return 1
  fi

  # Check OpenSSL version
  openssl_version=$(openssl version)
  if [ $? -ne 0 ]
  then
    echo "Error: Failed to detect OpenSSL version"
    return 1
  fi

  # Check if OpenSSL supports TLS 1.2
  openssl_supports_tls12=$(openssl ciphers -v | grep -q TLSv1.2 && echo "yes" || echo "no")

  # Print output
  echo "OpenSSL [ Version - $openssl_version, TLS 1.2 - $openssl_supports_tls12 ]"
}

mds_log()
{
    local _syslog_="/var/log/messages"
    local _disk_=""

    # get the run time in local and UTC
    date &&  date -u

    # Get Driver Stats
    $inm_dmit --get_drv_stat | cut -f 2- -d ":" | sed '/^$/d' | sed 's/^[ \t]*//'

    for _disk_ in `$inm_dmit --get_protected_volume_list`
    do
        echo "#### $_disk_ ####"
        $inm_dmit --get_volume_stat $_disk_ | cut -f 2- -d ":" | sed '/^$/d' | sed 's/^[ \t]*//'
    done

    # Extract all driver logs from syslog
    echo "#### Syslog ####"

    if [ ! -f "$_syslog_" ]
    then
        _syslog_="/var/log/syslog"
    fi

    echo "#### HEAD ####"
    grep "involflt" "$_syslog_" | head -50 > /var/log/involflt_head.cur
    cmp /var/log/involflt_head.cur /var/log/involflt_head.prev > /dev/null 2>&1 && {
        echo "Same as before"
    } || {
        cat /var/log/involflt_head.cur
        cp /var/log/involflt_head.cur /var/log/involflt_head.prev
    }
    echo "#### TAIL ####"
    grep "involflt" "$_syslog_" | tail -50 > /var/log/involflt_tail.cur
    cmp /var/log/involflt_tail.cur /var/log/involflt_tail.prev > /dev/null 2>&1 && {
        echo "Same as before"
    } || {
        cat /var/log/involflt_tail.cur
        cp /var/log/involflt_tail.cur /var/log/involflt_tail.prev
    }

    chmod 640 /var/log/involflt_head.cur
    chmod 640 /var/log/involflt_head.prev
    chmod 640 /var/log/involflt_tail.cur
    chmod 640 /var/log/involflt_tail.prev

    cat /var/log/InMage_drivers.log
    cat /var/log/InMage_drivers.log >> /var/log/InMage_drivers.old.log
    rm -f /var/log/InMage_drivers.log

    check_openssl

}

# Main entry point to the script
if [ "$1" = "_mds_" ]
then
    mds_log | tr '\n' '|'
    exit
fi

inm_log "Executing support material collection script "
cleanup
case "${os}" in
    SunOS)
        if /usr/sbin/modinfo | grep -i involflt > /dev/null 2>&1; then
            inm_log " ${INVOLFLT_DRVNAME} driver is loaded"
            cleanup
        else
            echo " ${INVOLFLT_DRVNAME} driver is not loaded"
            inm_log " ${INVOLFLT_DRVNAME} driver is not loaded"
            cleanup
            exit
        fi
    ;;
    Linux)
        if ! lsmod | grep -i involflt > /dev/null 2>&1; then
            inm_log " ${INVOLFLT_DRVNAME} driver is not loaded"
            cleanup
            echo " ${INVOLFLT_DRVNAME} driver is not loaded"
        fi
    ;;
    AIX)
        DF_COMMAND="df -P"
        if ! /usr/bin/genkex | grep -i involflt > /dev/null 2>&1; then
            inm_log " ${INVOLFLT_DRVNAME} driver is not loaded"
            cleanup
            echo " ${INVOLFLT_DRVNAME} driver is not loaded"
            exit
        fi
    ;;
    *)
        echo "OS is not supported"
        exit
    ;;
esac

# Description:
# Script helps user to collection support materials on a given system based on Unix.
# It collects following information
# 1. Agent logs under /var/log
# 2. System logs user /var/
# 3. Get list of crash dumps if available
# 4. Get contents of persistent store
# 5. Collect release build information
# 6. drscout.conf
# 7. Get driver and target context stats 
# 8. Get Bitmap files on the system
# 9. Get dmesg output to a file
# 

supportpath_input=$1
# Store support materials in directory provided by user
if [ -d "${supportpath_input}" ]
then
    #echo "User input $supportpath_input"
    supportstore=$supportpath_input/$filename
else
    if [ "${supportpath_input}" ]
    then
        echo "Path $supportpath_input for support materials is not valid"
        exit;
    fi
    supportstore="/root/$filename"
fi

# Scout protection model - currently used only for MaxRep
if [ "$2" != "MaxRep" ]
then
    supportmodel_input=
    echo "Evaluating free space in chosen support store location $supportstore"
    $DF_COMMAND $supportpath_input /root
    echo
    echo "Please press (Y/y) if you think chosen support store location has"
    echo "sufficient free space for generating logs: "
    read input

    if [ "$input" != "y" ] && [ "$input" != "Y" ]; then
        echo "Free up some space on desired location for support materials.."
        exit 1
    fi

else
    supportmodel_input=$2
fi

if [ "$supportmodel_input" ]; then
    echo "MaxRep configuration..: $supportmodel_input"
fi

supportstore=`echo $supportstore | sed -e "s: :_:g"`;
filename=`echo $filename | sed -e "s: :_:g"`;
mkdir $supportstore
echo "Support materials will be stored in temporary directory $supportstore";

if [ "${os}" = "SunOS" ]; then
     ID=/usr/xpg4/bin/id
else
     ID=/usr/bin/id
fi

user=`$ID -un`

echo "Collecting agent logs, system logs & dmesg output"
# 1. Agent logs under /var/log
# 2. System logs user /var/
# 9. Get dmesg output to a file
mkdir $supportstore/system_logs
mkdir $supportstore/config
case "${os}" in
    SunOS)
        dmesg > $supportstore/dmesg.log
        cp /var/adm/message* $inmage_logs $supportstore/system_logs/ > /dev/null 2>&1
        cp /var/svc/log/InMage* $supportstore/system_logs/ > /dev/null 2>&1
        acctadm > $supportstore/system_logs/acctadm.log  > /dev/null 2>&1
        cp /var/log/syslog* $supportstore/system_logs/  > /dev/null 2>&1
        if [ -f /var/log/vormetric/vmd.log ]; then
            cp /var/log/vormetric/vmd.log $supportstore/system_logs/  > /dev/null 2>&1
        fi
        if [ -f /var/log/vormetric/secfs.log ]; then
            cp /var/log/vormetric/secfs.log $supportstore/system_logs/  > /dev/null 2>&1
        fi
    ;;
    Linux)
        dmesg > $supportstore/dmesg.log
        if [ "$supportmodel_input" ]; then
            echo "MaxRep configuration..: $supportmodel_input"
            cp $vx_dir/resync $vx_dir/AppliedInfo $supportstore/system_logs/ > /dev/null 2>&1
        else
            cp /var/log/message* /var/log/syslog* $inmage_logs $vx_dir/resync $vx_dir/AppliedInfo $supportstore/system_logs/ > /dev/null 2>&1
        fi
        cp /var/log/syslog* $supportstore/system_logs/  > /dev/null 2>&1
        #tar -cvf $supportstore/system_logs.tar /var/log/message* $inmage_logs $vx_dir/resync $vx_dir/AppliedInfo > /dev/null 2>&1
        cat /proc/cpuinfo > $supportstore/cpuinfo.log
        dmidecode > $supportstore/dmidecode.log
    ;;
    AIX)
        $inm_dmit --op=dmesg  > $supportstore/dmesg.log
        /usr/bin/cp /var/log/involflt_driver.log* $inmage_logs $supportstore/system_logs/ > /dev/null 2>&1
        /usr/bin/cp /var/log/.inmboot_log $supportstore/config/ > /dev/null 2>&1
    ;;
    *)
        echo "OS is not supported"
        exit
    ;;
esac

echo "Collecting persistent store configuration files"
# 4. Get contents of persistent store
# copy whatever is stored in persistent store /etc/vxagent/involflt
mkdir $supportstore/persistentstore
cp -R /etc/vxagent/involflt $supportstore/persistentstore
#tar -cvf $supportstore/persistentstore.tar /etc/vxagent/involflt > /dev/null 2>&1

echo "Collecting Scout configuration files"
# 5. Collect release build information
# 6. drscout.conf
# Get configuration logs under config directory
cp /usr/local/drscout.conf $supportstore/config/
cp /usr/local/.vx_version $supportstore/config/vx_version
cp /usr/local/.fx_version $supportstore/config/fx_version
cp /.vx* $supportstore/config/ > /dev/null 2>&1
cp /.boottimemirroring* $supportstore/config/ > /dev/null 2>&1
#tar -cvf $supportstore/config/other.tar /.vx* /.boottimemirroring* > /dev/null 2>&1
case "${os}" in
    SunOS)
        cp /etc/system $supportstore/config/ 
        cp /kernel/drv/involflt.conf /kernel/drv/linvsnap.conf $supportstore/config/
        ls -l /var/crash/* > $supportstore/crash_dump-list
    ;;
    Linux)
        ls -l /var/crash/* > $supportstore/crash_dump-list
        /usr/bin/lsscsi > $supportstore/config/lsscsi.txt
        if [ "$supportmodel_input" ]; then
            cp /proc/scsi_tgt/sessions $supportstore/config/
            cp /proc/scsi_tgt/scsi_tgt $supportstore/config/
            cp /proc/scsi_tgt/sgv $supportstore/config/
            ls -l /proc/scsi_tgt/groups/ > $supportstore/config/groups.list
        fi
    ;;
    AIX)
        ls -l /var/adm/ras/ > $supportstore/crash_dump-list
    ;;
    *)
        echo "OS is not supported"
        exit
    ;;
esac


# 7. Get driver and target context stats 
mkdir $supportstore/stats 2>&1

echo "Collecting driver related statistics";
case "${os}" in
      SunOS)
      $inm_dmit --get_drv_stat > $supportstore/stats/driver-stats
      $inm_dmit --get_protected_volume_list > $supportstore/stats/pvlist-driver.log
      for volume in `cat $supportstore/stats/pvlist-driver.log | grep -v "IOCTL_INMAGE_GET_PROTECTED_VOLUME_LIST" | awk '{print $1}'`
      do
         tmp_fname=`echo $volume | sed -e "s:/:_:g"`;
         $inm_dmit --get_volume_stat $volume > $supportstore/stats/$tmp_fname;
         $inm_dmit --op=additional_stats --src_vol=$volume > $supportstore/stats/${tmp_fname}_bitmap_changes 2>&1 > /dev/null;
         $inm_dmit --op=latency_stats --src_vol=$volume > $supportstore/stats/${tmp_fname}_latency_stats 2>&1 > /dev/null;
      done

       ;;
      Linux)
      if [ -d /proc/involflt ]
      then
         ls -l $procstore/* > $supportstore/stats/pvlist-driver.log
         for file in $procstore/*
         do
            echo "Stats for entry $file"
            tmpfilename=`echo $file | sed -e "s:/:_:g"`
            cat $file > $supportstore/stats/$tmpfilename
         done
      else
         $inm_dmit --get_drv_stat > $supportstore/stats/driver-stats
         $inm_dmit --get_protected_volume_list > $supportstore/stats/pvlist-driver.log
         for volume in `cat $supportstore/stats/pvlist-driver.log | awk '{print $1}'`
         do
            tmp_fname=`echo $volume | sed -e "s:/:_:g"`;
            $inm_dmit --get_volume_stat $volume > $supportstore/stats/$tmp_fname;
            if [ "$volume" != "DUMMY_LUN_ZERO" ]; then
               $inm_dmit --op=additional_stats --src_vol=$volume > $supportstore/stats/${tmp_fname}_bitmap_changes 2>&1 > /dev/null;
               $inm_dmit --op=latency_stats --src_vol=$volume > $supportstore/stats/${tmp_fname}_latency_stats 2>&1 > /dev/null;
            fi
         done
      fi
        # Add vCon logs if any
        mkdir $supportstore/vCon
        if [ -d /etc/vxagent/vcon ]; then
            cp -R /etc/vxagent/vcon $supportstore/vCon/ 2>&1 > /dev/null
        fi
        if [ -d /etc/vxagent/logs ]; then
            cp -R /etc/vxagent/logs $supportstore/vCon/ 2>&1 > /dev/null
        fi
		if [ -d $vx_dir/etc ]; then
            cp -R $vx_dir/etc $supportstore/vCon/ 2>&1 > /dev/null
		fi
		
		if [ -f $vx_dir/scripts/azure/PrepareTarget.log ]; then
            	cp  $vx_dir/scripts/azure/PrepareTarget.log $supportstore/vCon/ 2>&1 > /dev/null
		fi
        if [ -d $vx_dir/failover_data ]; then
            cp -R $vx_dir/failover_data/ $supportstore/vCon/ 2>&1 > /dev/null
            if [ -f $vx_dir/scripts/vCon/PrepareTarget.log ]; then
            	cp  $vx_dir/scripts/vCon/PrepareTarget.log $supportstore/vCon/ 2>&1 > /dev/null
            	echo "Role: MT" > $supportstore/vCon/Machine_Role.log 2>&1 > /dev/null
            fi
            if [ -f $vx_dir/scripts/vCon/LinuxP2V.log ]; then
            	cp  $vx_dir/scripts/vCon/LinuxP2V.log $supportstore/vCon/ 2>&1 > /dev/null
            	echo "Role: Source" > $supportstore/vCon/Machine_Role.log 2>&1 > /dev/null
            fi
        fi
		#Data protection logs 
		if [ -d /var/log/vxlogs ]; then 
			mkdir $supportstore/dataProtection
			cp -R /var/log/vxlogs $supportstore/dataProtection/ 2>&1 > /dev/null
		fi				
		
        mkdir $supportstore/vCon/network
        if [ -d /etc/sysconfig/network-scripts/ ]
        then
            cp -R /etc/sysconfig/network-scripts/*eth* $supportstore/vCon/network 2>&1 > /dev/null
        fi
        # Applicable only for vCon protections 
        if [ -f /boot/Configuration.info ]; then
            mkdir $supportstore/vCon/boot
            cp /boot/Configuration.info $supportstore/vCon/boot/ 2>&1 > /dev/null
            md5sum /boot/init*.* 2>&1 > $supportstore/vCon/boot/initrd_md5sum.log
            stat /boot/init*.* 2>&1 > $supportstore/vCon/boot/init_stat.log
            if [ -d /boot/init*INMAGE* ]; then
                cp /boot/init*INMAGE* $supportstore/vCon/boot/ 2>&1 > /dev/null
            fi
            if [ -d /boot/init*InMage* ]; then
                cp /boot/init*InMage* $supportstore/vCon/boot/ 2>&1 > /dev/null
            fi
        fi

        if [ -f /etc/vxagent/logs ]; then
            cp /var/log/inmage_mount.log $supportstore/vCon/ 2>&1 > /dev/null
        fi
		if [ -d $fx_dir ]; then
		    mkdir -p $supportstore/fx_logs/ 2>&1 > /dev/null
            cp $fx_dir/inmage_job*.log $fx_dir/sv*.log $supportstore/fx_logs/ 2>&1 > /dev/null
			
			cp -R $fx_dir/transport $supportstore/fx_logs/ 2>&1 > /dev/null
		fi
		
		if [ -d /home/svsystems/var/ApplicationPolicyLogs ]; then 
			cp -R /home/svsystems/var/ApplicationPolicyLogs  $supportstore/ 2>&1 > /dev/null
		fi	
		
        if [ -d /var/log/ArchivedLogs ]; then
           cp -R /var/log/ArchivedLogs $supportstore/ 2>&1 > /dev/null
        fi
		
       ;;
   AIX)
      $inm_dmit --get_drv_stat > $supportstore/stats/driver-stats
      $inm_dmit --get_protected_volume_list > $supportstore/stats/pvlist-driver.log
      for volume in `cat $supportstore/stats/pvlist-driver.log | awk '{print $1}'`
      do
         tmp_fname=`echo $volume | sed -e "s:/:_:g"`;
         $inm_dmit --get_volume_stat $volume > $supportstore/stats/$tmp_fname;
         $inm_dmit --op=additional_stats --src_vol=$volume > $supportstore/stats/${tmp_fname}_bitmap_changes;
         $inm_dmit --op=latency_stats --src_vol=$volume > $supportstore/stats/${tmp_fname}_latency_stats;
      done
       ;;
      *)
         echo "OS is not supported"
         return 1
         ;;
esac

# 8. Get Bitmap files on the system
mkdir ${supportstore}/bitmap_files
cp /root/InMage*.VolumeLog ${supportstore}/bitmap_files/  > /dev/null 2>&1

#10. Get sysyem information..
mkdir ${supportstore}/sysinfo/
echo "Collecting system software/hardware configuration information.. "
cp /etc/*release ${supportstore}/sysinfo/
case "${os}" in
      SunOS)
          cp /etc/vfstab ${supportstore}/sysinfo/
          /usr/platform/`uname -i`/sbin/prtdiag -v > ${supportstore}/sysinfo/prtdiag.txt
          /usr/sbin/modinfo > ${supportstore}/sysinfo/modinfo.log
          /usr/sbin/prtconf -v > ${supportstore}/sysinfo/prtconf.txt
          /usr/bin/pkginfo -l > ${supportstore}/sysinfo/pkginfo.log
          /usr/sbin/patchadd -p > ${supportstore}/sysinfo/patchadd-p.txt
          ps -eo zone,uid,pid,ppid,time,comm,vsz,osz,pmem,rss > ${supportstore}/sysinfo/ps_detailed.txt
          /usr/sbin/cryptoadm list -v > ${supportstore}/sysinfo/cryptoadm.txt
          if [ "$user" = "root" ]
          then
              echo ::nm | /usr/bin/mdb -k > ${supportstore}/sysinfo/mdb-nm.log
          fi
          iostat -xdm -e > ${supportstore}/sysinfo/iostat_xdm.txt
          echo "Executing sar command. It takes about 15 seconds"
          sar -aAbcdgkmpqruvwy 5 3 > ${supportstore}/sysinfo/sar_A_5_3.log
          uptime > $supportstore/uptime.txt
          ipcs -a > ${supportstore}/sysinfo/ipcs_a.log
          fmadm faulty -a -v> ${supportstore}/sysinfo/fmadm_faulty.log
          svcs > ${supportstore}/sysinfo/svcs.log
          svcs -xv > ${supportstore}/sysinfo/svcs_xv.log
          vmstat -p > ${supportstore}/sysinfo/vmstat_p.log
          vmstat -s > ${supportstore}/sysinfo/vmstat_s.log
          mpstat > ${supportstore}/sysinfo/mpstat.log
      ;;
      Linux)
          ls -l /boot > ${supportstore}/sysinfo/boot.txt
          for i in /boot/init*
          do
              echo $i > ${supportstore}/sysinfo/initrd.txt
              echo "===================" >> ${supportstore}/sysinfo/initrd.txt
              which lsinitramfs > /dev/null 2>&1
              if [ $? -eq 0 ]; then
                  lsinitramfs $i >> ${supportstore}/sysinfo/initrd.txt
              else
                  which lsinitrd > /dev/null 2>&1
                  if [ $? -eq 0 ]; then
                      lsinitrd $i >>  ${supportstore}/sysinfo/initrd.txt
                  else
                      zcat $i | cpio -itv >> ${supportstore}/sysinfo/initrd.txt
                  fi
              fi
              echo "===================" >> ${supportstore}/sysinfo/initrd.txt
          done
          /sbin/sfdisk -luS 2> /dev/null | grep -e '/dev/' | grep -v Empty > ${supportstore}/sysinfo/sfdisk.txt
          dmsetup table > ${supportstore}/sysinfo/dmsetup.txt 2>&1
          vgdisplay -v > ${supportstore}/sysinfo/vgdisplay_v.txt 2>&1
          /bin/cp /etc/fstab ${supportstore}/sysinfo/
          /bin/cp /etc/mtab ${supportstore}/sysinfo/
          /sbin/lspci  -v > ${supportstore}/sysinfo/lspci.txt
          /sbin/lsmod > ${supportstore}/sysinfo/lsmod.txt
          cat /proc/meminfo  > ${supportstore}/sysinfo/meminfo.txt
          cat /proc/cpuinfo  > ${supportstore}/sysinfo/cpuinfo.txt
          cat /proc/mounts > ${supportstore}/sysinfo/proc_mounts.txt
          rpm -qa  > ${supportstore}/sysinfo/rpmlist.txt
          ps -aef > ${supportstore}/sysinfo/ps.txt
          ps -eo zone,uid,pid,ppid,time,comm,vsz,osz,pmem,rss > ${supportstore}/sysinfo/ps_detailed.txt
          mount > ${supportstore}/sysinfo/mount.log
          last > ${supportstore}/sysinfo/last.log
          uname -a > ${supportstore}/sysinfo/uname-a.log
          cp /etc/sysctl.conf  ${supportstore}/sysinfo/
          sysctl -p  > ${supportstore}/sysinfo/sysctl_p.log
          ipcs -a > ${supportstore}/sysinfo/ipcs_a.log
          iostat -xdm > ${supportstore}/sysinfo/iostat_xdm.txt
          > $supportstore/disk-stats
          for i in `ls /sys/block/`
          do
            echo "Stats for $i" >> $supportstore/disk-stats
            cat /sys/block/$i/stat >> $supportstore/disk-stats
          done
          echo "Executing sar command and it takes about 15 seconds"
          sar -A 5 3 2>&1 > ${supportstore}/sysinfo/sar_A_5_3.log
          uptime > $supportstore/uptime.txt
          top -b -d 1 -n 3 > $supportstore/sysinfo/top_b_d_1_n_3.log
          chkconfig --list > ${supportstore}/sysinfo/chkconfig_list.log
          vmstat -m > ${supportstore}/sysinfo/vmstat_m.log
          vmstat -s > ${supportstore}/sysinfo/vmstat_s.log
          vmstat -d > ${supportstore}/sysinfo/vmstat_d.log
          cat /proc/mdstat > ${supportstore}/sysinfo/mdstat
		  service --status-all > ${supportstore}/RunningServices_list.log
		  lsmod | sort > ${supportstore}/LoadedModules_list.log
		  find /lib/modules -type f -name linvsnap.ko > ${supportstore}/LinuxUA-linvsnapDriver_list.log
		  find /lib/modules -type f -name involflt.ko > ${supportstore}/LinuxUA-involfltDriver_list.log
		  which rpm && {
            rpm -qa | sort > ${supportstore}/InstalledRPMS_list.log
          } || {
            dpkg -l | sort > ${supportstore}/InstalledRPMS_list.log
          }
		  chkconfig --list > ${supportstore}/AllServices_list.log
      ;;
      AIX)
          /usr/bin/cp /etc/filesystems ${supportstore}/sysinfo/
          /usr/bin/lslpp -L > ${supportstore}/sysinfo/lslpp.lst
          /usr/bin/genkex > ${supportstore}/sysinfo/genkex.lst
          /usr/sbin/lspv > $supportstore/lspv.output
          /usr/sbin/lsdev > $supportstore/lsdev.output
          /usr/sbin/lsconf > $supportstore/sysinfo/lsconf.output
          /usr/bin/mkdir $supportstore/odm_info
          /usr/bin/odmget Config_Rules > $supportstore/odm_info/Config_Rules.lst
          /usr/bin/odmget CuVPD > $supportstore/odm_info/CuVPD.lst
          /usr/bin/odmget CuDv > $supportstore/odm_info/CuDv.lst
          /usr/bin/odmget CuAt > $supportstore/odm_info/CuAt.lst
          /usr/bin/odmget CuDvDr > $supportstore/odm_info/CuDvDr.lst
          /usr/bin/odmget PdDv > $supportstore/odm_info/PdDv.lst
          /usr/bin/odmget PdAt > $supportstore/odm_info/PdAt.lst
          /usr/bin/ps -aef > ${supportstore}/sysinfo/ps_aef.txt
          /usr/bin/ps auxw > ${supportstore}/sysinfo/ps_auxw.txt
          uptime > $supportstore/uptime.txt
          sar -A 5 3 > ${supportstore}/sysinfo/sar_A_5_3.log
          errpt -A > ${supportstore}/sysinfo/errpt_A.log
          ipcs -a > ${supportstore}/sysinfo/ipcs_a.log
          vmstat -v > ${supportstore}/sysinfo/vmstat_v.log
          vmstat -l > ${supportstore}/sysinfo/vmstat_l.log
          vmstat -s > ${supportstore}/sysinfo/vmstat_s.log
          svmon -G > ${supportstore}/sysinfo/svmon_G.log
          svmon -P > ${supportstore}/sysinfo/svmon_P.log
          mpstat > ${supportstore}/sysinfo/mpstat.log
          vmo -a > ${supportstore}/sysinfo/vmo_a.log
          pagesize -af > ${supportstore}/sysinfo/pagesizes.log
          iostat -alDRT 1 1 > ${supportstore}/sysinfo/iostat_alDRT_1_1.log
       ;;
       *)
         echo "OS is not supported"
         return 1
         ;;
esac

mkdir $supportstore/target
$vx_dir/bin/cdpcli  --listtargetvolumes 2>&1 > $supportstore/target/cdpcli-listtargetvolumes
$vx_dir/bin/cdpcli  --showreplicationpairs 2>&1 > $supportstore/target/cdpcli-showreplicationpairs
$vx_dir/bin/cdpcli  --showsnapshotrequests 2>&1 > $supportstore/target/cdpcli-showsnapshotrequests

$vx_dir/bin/cdpcli  --showreplicationpairs |grep Catalogue > $supportstore/target/retentionpath.log 2>&1 

install_path=`echo $vx_dir | sed -e "s/\/[^\/]*$//"`;
cp $install_path/patch.log $supportstore/

if [ -d $install_path/ApplicationData/etc ]; then
    cp  $install_path/ApplicationData/etc/*.log $supportstore/ 2>&1 > /dev/null
fi
filename="$supportstore/target/retentionpath.log"
while read -r line
do
    retname=$line
	retdbname=`echo $retname | awk -F ':' '{print $2}'`
	retpath=`echo $retdbname | awk -F 'cdp' '{print $1}'`
	mkdir -p $supportstore/$retpath;
	cp $retdbname $supportstore/$retpath
done < "$filename"

# Call stack_of_all_process.sh to capture the stack of all the processes, capturing its exit code
echo "Calling stack_of_all_process.sh with arg : $supportstore"
./stack_of_all_process.sh "${supportstore}"
exit_code=$?

# Check the exit code of stack_of_all_process.sh
if [ $exit_code -ne 0 ]; then
    echo "stack_of_all_process.sh failed with exit code $exit_code"
else
    echo "stack_of_all_process.sh executed successfully"
fi

journalctl > "$supportstore/journalctl.log"

#tar -cvf ${supportstore}/bitmap_files.tar /root/InMage*.VolumeLog  > /dev/null 2>&1
tar -cvf $supportstore.tar $supportstore > /dev/null 2>&1
gzip $supportstore.tar > /dev/null 2>&1
rm -rf $supportstore 
echo "Compressed support materials file $supportstore.tar.gz"
exit;

