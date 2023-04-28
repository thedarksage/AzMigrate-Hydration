#!/bin/sh

PATH=/bin:/usr/bin:/usr/sbin:/sbin:$PATH

ECHO=/bin/echo
SED=/bin/sed
AWK=/usr/bin/awk
NULL=/dev/null
DU=/usr/bin/du

FailedConditions=0

SetupLogs()
{
   dirName=`dirname $0`
   orgWd=`pwd`

   cd $dirName
   SCRIPT_PATH=`pwd`
   cd $orgWd

   LOG_FILE=${SCRIPT_PATH}/inm_mondopod_validator.log

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

   CONFIG_FILE=${SCRIPT_PATH}/config.ini
   if [ ! -f $CONFIG_FILE ];then
       echo "ERROR: Config file $CONFIG_FILE not found"
       exit 1
   fi

   INSTALLATION_DIR=`grep INSTALLATION_DIR /usr/local/.sms_version| cut -d'=' -f2`

   if [ -f ${INSTALLATION_DIR}/bin/smartctl ];then
       SMARTCTL=${INSTALLATION_DIR}/bin/smartctl
   else
       $ECHO "ERROR: smartctl is not found under ${INSTALLATION_DIR}/bin"
       exit 1
   fi

}

log()
{
   $ECHO "(`date +'%m-%d-%Y %H:%M:%S'`):   DEBUG $$ $*" >> $LOG_FILE
   return
}

Output()
{
    $ECHO $*
    $ECHO 

    log $*
}

Validate()
{
    Value=`echo $1|tr '[a-z]' '[A-Z]'`
    Param=$2

    grep "$Param" $CONFIG_FILE |grep -v "^#" > $NULL
    if [ $? -eq 0 ];then

        CfgValue=`grep "$Param" $CONFIG_FILE|awk -F= '{print $2}'|tr '[a-z]' '[A-Z]'|sed 's/[ \t]*$//'`

        if [ ! -z "$CfgValue" ];then
            if [ "$Value" = "$CfgValue" ];then
                Output "Validation of $Param ............... PASSED "
            else
                Output "Validation of $Param ............... FAILED "
                Output "Configured Value : $Value for Parameter : $Param"
                FailedConditions=`expr $FailedConditions + 1`
            fi
        else
            Output "Null value is configured for $Param....."
            FailedConditions=`expr $FailedConditions + 1`
        fi
    else
        Output "$Param Not Present in the $CONFIG_FILE file"
        Output "Skipping the Validation of $Param"
        return
    fi

}

VersionValidator()
{
   ChkOSVer=`grep "CheckOSVersion" $CONFIG_FILE|awk -F= '{print $2}'`

   if [ $ChkOSVer -eq 1 ];then
       $ECHO
       uname -a
       $ECHO
       cat /etc/*release*
       $ECHO
   fi

   SysBiosVer=`prtdiag|grep "BIOS"|awk -F: '{print $2}'|awk '{print $(NF-1)}'|sed 's/[ \t]*$//'`

   Validate "$SysBiosVer" "SysBIOSVersion"

   FWVersion=`sasinfo hba -v|grep "Firmware"|uniq|awk -F: '{print $2}'|sed 's/^[ \t]*//g;s/[ \t]*$//'`

   Validate "$FWVersion" "FirmwareVersion"

   BmcVersion=`ipmitool mc info | grep "Firmware Revision"|awk -F: '{print $2}'|sed 's/^[ \t]*//g'`

   Validate "$BmcVersion" "BMCVersion"

   Disks=`iostat -iEn|grep "c[0-9]t"|awk '{print $1}'`

   CheckHDDFWVer=`grep "CheckHDDFWVersion" $CONFIG_FILE|awk -F= '{print $2}'`

   if [ $CheckHDDFWVer -eq 1 ];then
       Output "Validating FWVersions of the SSDs ............"
       tempfile=/tmp/devinfo
       for Disk in $Disks
       do
          $SMARTCTL -x /dev/rdsk/$Disk -d scsi|grep "Solid State Device" > $NULL
          if [ $? -eq 0 ];then
              $SMARTCTL -i /dev/rdsk/$Disk -d sat > $tempfile
              DiskFWVer=`grep "Firmware Version" $tempfile | awk -F: '{print $2}'|sed 's/^[ \t]*//g'`
              DevModel=`grep "Device Model" $tempfile | awk -F: '{print $2}'|sed 's/^[ \t]*//g'`
              Output " Disk : $Disk \t DeviceModel: $DevModel \t FirmwareVersion: $DiskFWVer"
          fi
       done
   fi
   $ECHO
}

CheckLinkStatus()
{
    ChkLinkStatus=`grep "CheckLinkStatus" $CONFIG_FILE|awk -F= '{print $2}'`

    if [ $ChkLinkStatus -eq 1 ];then

        Output "Checking Link Status.............."

        $ECHO "LINK \t\t MTU \t\t STATE \t\t MEDIA \t\t SPEED" 

        dladm|grep -v LINK > /tmp/dlm.out
        dladm show-phys > /tmp/dlmphy.out

        while read line
        do
            link=`echo $line | awk '{print $1}'`
            mtu=`echo $line | awk '{print $3}'`
            state=`echo $line | awk '{print $4}'`
            speed=`grep $link /tmp/dlmphy.out|awk '{print $4}'`
            media=`grep $link /tmp/dlmphy.out|awk '{print $2}'`
            len=${#state}
            if [ $len -gt 6 ];then
                $ECHO "$link \t\t $mtu \t\t $state \t $media \t\t $speed"
            else
                $ECHO "$link \t\t $mtu \t\t $state \t\t $media \t\t $speed"
            fi
        done</tmp/dlm.out

        rm -f /tmp/dlm.out
        rm -f /tmp/dlmphy.out
    fi
    $ECHO
}

TuneParamValidator()
{
   Output "Validating TuneParameters........."

   zfsarcmax=`grep -v "^\*" /etc/system | grep zfs_arc_max|awk -F= '{print $2}'`

   Validate "$zfsarcmax" "ZfsArcMax"

   l2arcwriteboost=`grep -v "^\*" /etc/system | grep l2arc_write_boost|awk -F= '{print $2}'`

   Validate "$l2arcwriteboost" "L2ARCWriteBoost"

   zfsunmapeignoresize=`grep -v "^\*" /etc/system | grep zfs_unmap_ignore_size|awk -F= '{print $2}'`

   Validate "$zfsunmapeignoresize" "ZFSUnmapIgnoreSize"

   ChkCache=`grep "CheckCache" $CONFIG_FILE|awk -F= '{print $2}'`

   if [ $ChkCache -eq 1 ];then
       Output "Validating Cache Settings in /kernel/drv/sd.conf"
       for ssd in `iostat -E | egrep -i 'intel|samsung'| awk '{print $4 " " $5}' | uniq`
       do
           grep $ssd /kernel/drv/sd.conf > /dev/null
           if [ $? -ne 0 ];then
               Output "NonVolatile Cache Settings Check  FAILED"
               Output "SSD $ssd is not configured in /kernel/drv/sd.conf"
               exit 1
           fi
       done
       for ssd in `iostat -E|grep -i crucial | awk '{print $4}' | uniq`
       do
           grep $ssd /kernel/drv/sd.conf > /dev/null
           if [ $? -ne 0 ];then
               Output "NonVolatile Cache Settings Check  FAILED"
               Output "SSD $ssd is not configured in /kernel/drv/sd.conf"
               exit 1
           fi
       done
       Output "NonVolatile Cache Settings check PASSED"
       $ECHO
       
   fi
}

LunParamValidator()
{
   Output "Validating LUN Parameters.................."

   for eachlun in `stmfadm list-lu|awk -F: '{print $2}'|sed 's/ //g'`
   do
       Output "Validating LUN $eachlun"
       
       sbdadm list-lu|grep -i $eachlun|awk '{print $3}'|grep -w rdsk > /dev/null
       if [ $? -ne 0 ];then
           $ECHO "ERROR: LUN is not created on rdsk"
           $ECHO
       else
           $ECHO "LUN is created on rdsk"
           $ECHO
       fi

       BlockSize=`stmfadm list-lu -v $eachlun |grep "Block Size"|awk -F: '{print $2}'|sed 's/ //g'`
       Validate "$BlockSize" "LUNBlockSize"

       WriteCacheMode=`stmfadm list-lu -v $eachlun |grep "Write Cache Mode Select"|awk -F: '{print $2}'|sed 's/ //g'`
       Validate "$WriteCacheMode" "LUNWriteCacheMode"

       $ECHO
   done

   Output "Validation of LUN Parameters is Done."
   $ECHO
}

ZfsParamValidator()
{
   Output "Validating Zfs Parameters............."

   for eachlun in `stmfadm list-lu|awk -F: '{print $2}'|sed 's/ //g'`
   do
       zfsvol=`sbdadm list-lu|grep -i $eachlun| sed 's/dev//g' | sed 's/zvol//g' | sed 's/rdsk//g' |sed 's/\// /g'| awk '{print $3 "/" $4}'`

       Output "Validating Parameters for the Zfs Vol : $zfsvol"
       
       Compression=`zfs get all $zfsvol |grep "compression" |awk '{print $3}'`
       Validate "$Compression" "ZfsCompression"

       Dedup=`zfs get all $zfsvol |grep "dedup" |awk '{print $3}'`
       Validate "$Dedup" "ZfsDedup"

       VolBlockSize=`zfs get all $zfsvol |grep "volblocksize" |awk '{print $3}'`
       Validate "$VolBlockSize" "ZfsVolBlockSize"

       $ECHO
   done

   Output "Validating Zfs Parameters is Done"
   $ECHO
}

ZpoolParamValidator()
{
    Output "Validating zpool Parameters............"

    Pools=`zpool list|grep -v NAME|grep -v rpool|awk '{print $1}'`

    for Pool in $Pools
    do
        RecordSize=`zfs get all $Pool|grep "recordsize"|awk '{print $3}'`
        Validate "$RecordSize" "ZPoolRecordSize"
    done

    Output "Validating zpool Parameters is Done"
    $ECHO
}

DiskParamValidator()
{
    ChkDiskErrors=`grep "CheckDiskErrors" $CONFIG_FILE|awk -F= '{print $2}'`

    if [ $ChkDiskErrors -eq 1 ];then

        Output "Checking for Disk Errors................."

        iostat -En > /tmp/iostat.out

        while read line
        do
           case $line in
              *Errors*)
                   Disk=`echo $line|awk '{print $1}'`
                   SoftErrors=`echo $line|awk -F: '{print $2}'|awk '{print $1}'`
                   HardErrors=`echo $line|awk -F: '{print $3}'|awk '{print $1}'`
                   TransportErrors=`echo $line|awk -F: '{print $4}'|awk '{print $1}'`
                   if [ $SoftErrors -ne 0 -o $HardErrors -ne 0 -o $TransportErrors -ne 0 ];then
                       Output "Validation of Disk $Disk FAILED"
                       Output "Disk $Disk has SoftErrors : $SoftErrors HardErrors : $HardErrors TransportErrors : $TransportErrors"
                       FailedConditions=`expr $FailedConditions + 1`
                   fi
              ;;
           esac
        done</tmp/iostat.out

        rm -f  /tmp/iostat.out
    fi
    $ECHO
}

###################### Main #########################

SetupLogs

VersionValidator

CheckLinkStatus

TuneParamValidator

LunParamValidator

ZfsParamValidator

ZpoolParamValidator

DiskParamValidator

if [ $FailedConditions -ne 0 ];then
    Output " Number of Failed Conditions : $FailedConditions"
    Output " Validation of few Parameters got FAILED "
    exit 1
else
    Output " Validation of all Parameters got PASSED"
fi

