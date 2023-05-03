#!/bin/sh 
#=================================================================
# FILENAME
#    inm_zpool_create.sh
#
# DESCRIPTION
#     Performs the following actions
#        1. Root Pool Mirroring
#        2. zpool Creation
#        3. Change The Attributes Of zpool
#        4. Exit
#
# VERSION: $Revision: 1.19 $     
#
# HISTORY
#     <17/07/2013>  Anusha T     Created.
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================

NULL=/dev/null
ECHO=/bin/echo
DU=/usr/bin/du

NUM_HOT_SPARES=2
NUM_FRONT_ENCL_SLOTS=24
NUM_BACK_ENCL_SLOTS=12

LOG_SSD_VENDORS="001517 5CD2E4"
CACHE_SSD_VENDORS="00A075 002538"
MIN_CACHE_CAPACITY=400120971264

isDisplayCached=0

diskinfofound=0
if [ -f /usr/sbin/diskinfo ];then
    diskinfofound=1
fi

SetupLogs()
{
  dirName=`dirname $0`
  orgWd=`pwd`

  cd $dirName
  SCRIPT_DIR=`pwd`
  cd $orgWd

  LOG_FILE=$SCRIPT_DIR/inm_pool_create.log

  if [ -f $LOG_FILE ]; then
     size=`$DU -k $LOG_FILE | awk '{ print $1 }'`
     if [ $size -gt 5000 ]; then
        cp -f $LOG_FILE $LOG_FILE.1
        > $LOG_FILE
     fi
  else
     > $LOG_FILE
     chmod a+w $LOG_FILE
  fi

  OUTFILE=${SCRIPT_DIR}/inm_disk.dat

  DISKLOCATE=${SCRIPT_DIR}/inm_disk_locate.sh

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


ReadOpt()
{
    options=$1
    echo "Please select from $options:"

    while :
    do
        read OPT
        $ECHO $options | grep -w $OPT > /dev/null 2>&1
        if [ $? -eq 0 ]; then
            break
        fi
        $ECHO "Please select from $options:"
    done
}

ReadAns()
{
    echo $* "[Y/N]:"
    while :
    do
        read ANS
        if [ "$ANS" = "y" -o "$ANS" = "Y" -o "$ANS" = "n" -o "$ANS" = "N" ]; then
            break
        fi
        $ECHO "Please enter [Y/N]:"
    done
}

ReadDisk()
{
    TMPFILE=/tmp/diskdetails
    $ECHO "Available $2 Disks Are: "
    for eachDisk in `cat $OUTFILE|grep "$2"|grep "FREE"|awk '{print $2}'`
    do
        iostat -Eni $eachDisk > $TMPFILE
        DiskNum=`grep $eachDisk $OUTFILE | awk '{print $1}'`
        DiskName=`grep $eachDisk $OUTFILE | awk '{print $2}'`
        DiskSize=`grep $eachDisk $OUTFILE | awk '{print $6 $7}'|sed 's/\[//g' |sed 's/\]//g'`
        ProductId=`grep "Product" $TMPFILE |awk '{print $4}'`
        VendorId=`grep "Vendor" $TMPFILE | awk '{print $2}'`
        DeviceId=`grep "Device Id" $TMPFILE | awk '{print $NF}'`

        $ECHO "$DiskNum $DiskName $ProductId $DiskSize $VendorId $DeviceId"
        log "$DiskNum $DiskName $ProductId $DiskSize $VendorId $DeviceId"
    done
    
    $ECHO $1 ":"
    while :
    do
       read DiskNum
       if [ ! -z $DiskNum ];then
           sed -n "${DiskNum}{p;q;}" $OUTFILE |grep "FREE"|grep "$2" > $NULL
           if [ $? -eq 0 ];then
               SelectedDisk=`sed -n "${DiskNum}{p;q;}" $OUTFILE | awk '{print $2}'`
               break
           fi
           $ECHO "Please Enter The Disk Number Among The Availabale Disks"
       else
           break
       fi
    done

    rm -f $TMPFILE
}

OutPut()
{
   echo $* >> $OUTFILE
}

Run()
{
    log "$*"

    if [ $TEST -eq 1 ]; then
       return
    fi

    $*
}

GetDiskConfig()
{
    $ECHO
    $ECHO "Disk Scanning is in Progress. Please Wait ........."
    $ECHO

    if [ -f $OUTFILE ];then
        rm -f $OUTFILE
    fi 

    diskinfofound=0
    if [ -f /usr/sbin/diskinfo ];then
        diskinfofound=1
    fi

    zpool status |grep "c[0-9]t" |awk '{print $1}' > /tmp/zpools
   
    if [ -f /tmp/vendorinfo ];then
        rm -f /tmp/vendorinfo
    fi

    iostat -iEn > /tmp/iostat.out
    while read line
    do
       case $line in
           *c[0-9]t[0-9]*)
               Disk=`echo $line|awk '{print $1}'`
            ;;
           *Product*)
               Product=`echo $line|awk '{print $4}'`
               DeviceId=`echo $line|awk '{print $NF}'|sed 's/id1,sd@n//g'`
               Vendor=`echo $line|awk '{print $2}'`
               echo "$Disk $Product $DeviceId $Vendor" >> /tmp/vendorinfo
            ;;
       esac
    done</tmp/iostat.out


    Disks=`cat /tmp/iostat.out|grep "c[0-9]t[0-9]"|awk '{print $1}'`

    log "DiskNumber DiskName DiskType DiskCapacity Enclosure DiskSlot PhysicalBlockSize LogicalBlockSize DiskStatus"

    DiskNum=0
    for Disk in $Disks
    do
        vendor=`grep "$Disk" /tmp/vendorinfo | awk '{print $4}'`    
        if [ "$vendor" = "IPMI" ];then      
            log "vendor type : $vendor for Disk : $Disk"
            continue    
        fi

        DiskEnclosure=
        DiskSlot=

        $SMARTCTL -x /dev/rdsk/$Disk -d scsi >/tmp/deviceinfo  

        grep "No such device" /tmp/deviceinfo > /dev/null
        if [ $? -eq 0 ];then
            device=`echo $Disk s0|sed 's/ //g'`
            $SMARTCTL -i /dev/rdsk/$device -d scsi >/tmp/deviceinfo 
            grep "No such device" /tmp/deviceinfo > /dev/null
            if [ $? -eq 0 ];then
                log "Not able to get info for the disk through smartctl :$Disk"
                continue
            fi
        fi

        vendor=
        cat /tmp/deviceinfo | grep "Rotation Rate" | grep "Solid State Device" > $NULL
        if [ $? -eq 0 ];then
            DiskType="SSD"
        else
            DiskType="SAS"
            vendor=`grep "Vendor:" /tmp/deviceinfo | awk -F: '{print $2}'|sed 's/^[ \t]*//'`
            if [ -z "$vendor" ];then
                log "vendor type : $vendor for the SAS Disk : $Disk"
                continue
            fi
        fi

        #Get the physical & logical sector sizes
        PhySecSize=`cat /tmp/deviceinfo|grep "Physical block size"|awk '{print $(NF-1)}'`
        LogSecSize=`cat /tmp/deviceinfo|grep "Logical block size" |awk '{print $(NF-1)}'`

        if [ -z "$PhySecSize" ];then
            PhySecSize=0
        fi

        if [ -z "$LogSecSize" ];then
            LogSecSize=0
        fi
      
        DiskCapacity=`cat /tmp/deviceinfo|grep "User Capacity" |awk -F":" '{print $NF}'|sed 's/^[ \t]*//' |sed 's/,//g'`

        if [ -z "$DiskCapacity" ];then
            log " DiskCapacity : $DiskCapacity for the disk : $Disk"
            DiskCapacity="0 bytes [0 GB]"
            continue
        fi

        if [ $diskinfofound -eq 1 ];then
            DiskEnclosure=`diskinfo -h -o CRc|grep -i $Disk|awk '{print $1}'|tr '[a-z]' '[A-Z]'`
            DiskSlot=`diskinfo -h -o CRc|grep -i $Disk|awk '{print $2}'|awk -F_ '{print $NF}'`
        else
            DiskEnclosure=`cat /tmp/deviceinfo|grep "attached SAS address"|head -1|awk -F= '{print $2}'|sed 's/ //g'`
            DiskSlot=`cat /tmp/deviceinfo|grep "attached phy identifier" |head -1|awk -F= '{print $2}'|sed 's/ //g'`
        fi

        if [ -z "$DiskSlot" -o -z "$DiskEnclosure" ];then
            if [ "$DiskType" = "SSD" ];then
                DiskLocate=`$DISKLOCATE $Disk`
                DiskEnclosure=`echo $DiskLocate|awk '{print $1}'|awk -F: '{print $1}'|tr '[a-z]' '[A-Z]'`
                DiskSlot=`echo $DiskLocate|awk '{print $2}'`
            fi
        fi

        if [ -z "$DiskEnclosure" ];then
            DiskEnclosure="-"
        fi

        if [ -z "$DiskSlot" ];then
            DiskSlot="-"
        fi

        DiskNum=`expr $DiskNum + 1`
        #Excluding the disks which are part of other pools.
        grep $Disk /tmp/zpools > $NULL
        if [ $? -eq 0 ];then
            OutPut "$DiskNum $Disk $DiskType $DiskCapacity $DiskEnclosure $DiskSlot $PhySecSize $LogSecSize INUSE"
            log "$DiskNum $Disk $DiskType $DiskCapacity $DiskEnclosure $DiskSlot $PhySecSize $LogSecSize INUSE"
        else
            OutPut "$DiskNum $Disk $DiskType $DiskCapacity $DiskEnclosure $DiskSlot $PhySecSize $LogSecSize FREE"
            log "$DiskNum $Disk $DiskType $DiskCapacity $DiskEnclosure $DiskSlot $PhySecSize $LogSecSize FREE"
        fi
    done

    #This is to remove the empty lines from the Outfile
    sed '/^$/d' $OUTFILE > /tmp/out
    cp /tmp/out $OUTFILE
    rm -f /tmp/out

    rm -f /tmp/zpools
    rm -f /tmp/deviceinfo

    return
}

CreateRpoolMirror()
{
    zpool status rpool > /tmp/rootpool

    grep "mirror" /tmp/rootpool > /dev/null
    if [ $? -eq 0 ];then
        $ECHO "root pool is already mirrorred"
        log "root pool is already mirrorred"
        #TODO check for other operations possible here.
        # 1. Want to recreate the root pool by destroying existing one
        # 2. Want to add another disk to the existing root pool
        return
    else
        isSlice=0

        RootDisk=`zpool status rpool|awk '{print $1}'|grep 'c[0-9]'|head -1`
        RootEnclId=`cat $OUTFILE|grep $RootDisk|awk '{print $8}'`

        log "Root Pool is created on $RootDisk"
        $ECHO $RootDisk |grep "s[0-9]" > /dev/null
        if [ $? -eq 0 ];then
            SliceNum=`echo $RootDisk |sed -e "s/^.*\(.\)$/\1/"`
            isSlice=1
            log "Root Pool is Created On Slice : $SliceNum"
            RootDevice=`echo $RootDisk |sed -e 's/s[0-9]//g'`
        else
            RootDevice=`echo $RootDisk`
        fi

        RootEnclId=`cat $OUTFILE|grep $RootDevice|awk '{print $8}'`
        RootDiskCapacity=`cat $OUTFILE|grep $RootDevice|awk '{print $4}'`
        RootDiskType=`cat $OUTFILE|grep $RootDevice|awk '{print $3}'`
        RootDiskSlot=`cat $OUTFILE|grep $RootDevice|awk '{print $9}'`

        RootDiskVendor=`cat /tmp/vendorinfo|grep $RootDevice|awk '{print $2}'`
          
        log "Root Disk Capacity : $RootDiskCapacity"
        log "Root Disk Enclosure : $RootEnclId"
        log "Root Disk Slot : $RootDiskSlot"

        PsblMirrorDisks=`cat $OUTFILE|sort -k4n|grep $RootDiskCapacity|grep $RootDiskType|grep $RootEnclId|egrep 'RESERVED|FREE'|awk '{print $2}'`
        nPsblMDsks=`cat $OUTFILE|sort -k4n|grep $RootDiskCapacity|grep $RootDiskType|grep $RootEnclId|egrep 'RESERVED|FREE'|awk '{print $2}'|wc -l`

        if [ $nPsblMDsks -eq 1 ];then
            RootMirrorDisk=`echo $PsblMirrorDisks`
            UpdateDevices $PsblMirrorDisks "INUSE"
        elif [ $nPsblMDsks -gt 1 ];then
              for Disk in $PsblMirrorDisks
              do
                 DiskVendor=`cat /tmp/vendorinfo|grep $Disk|awk '{print $2}'`
                 if [ "$RootDiskVendor" = "$DiskVendor" ];then
                     RootMirrorDisk=`echo $Disk`
                     UpdateDevices $Disk "INUSE"
                 fi
              done
        fi

        if [ -z "$RootMirrorDisk" ]; then
            for device in `cat $OUTFILE|sort -k4n|grep $RootDiskType|grep $RootEnclId|grep "FREE"|awk '{print $2}'`
            do
                log "Found Free Device : $device"
                capacity=`cat $OUTFILE|grep $device|awk '{print $4}'`
                log "Capacity of the Disk : $capacity"

                if [ $capacity -gt $RootDiskCapacity ]; then
                    if [ -z "$RootMirrorDisk" ]; then
                        RootMirrorDisk=$device
                        RootMirrorDiskCapacity=$capacity
                    else
                        if [ $capacity -lt $RootMirrorDiskCapacity ]; then
                            RootMirrorDisk=$device
                            RootMirrorDiskCapacity=$capacity
                        fi
                    fi
                fi
            done
        fi

        if [ ! -z "$RootMirrorDisk" ]; then
            log "Selected Root Pool Mirror : $RootMirrorDisk"
            $ECHO "Selected Root Pool Mirror : $RootMirrorDisk"

            DiskType=`grep "$RootMirrorDisk" $OUTFILE|awk '{print $3}'`
            DiskSize=`grep "$RootMirrorDisk" $OUTFILE|awk '{print $6 $7}'|sed 's/\[//g'|sed 's/\]//g'`
            DiskEnclosure=`grep "$RootMirrorDisk" $OUTFILE | awk '{print $8}'`
            DiskSlot=`grep "$RootMirrorDisk" $OUTFILE | awk '{print $9}'`
            VendorId=`grep "$RootMirrorDisk" /tmp/vendorinfo|awk '{print $4}'`
            ProductId=`grep "$RootMirrorDisk" /tmp/vendorinfo|awk '{print $2}'`

            $ECHO
            $ECHO "Attributed of Selected Rpool Mirror Disk "
            $ECHO "  DiskType : $DiskType"
            $ECHO "  DiskSize : $DiskSize"
            $ECHO "  DiskEnclosure : $DiskEnclosure"
            $ECHO "  DiskSlot : $DiskSlot"
            $ECHO "  VendorId : $VendorId"
            $ECHO "  Product : $ProductId"

            log "Attributed of Selected Rpool Mirror Disk "
            log "  DiskType : $DiskType"
            log "  DiskSize : $DiskSize"
            log "  DiskEnclosure : $DiskEnclosure"
            log "  DiskSlot : $DiskSlot"
            log "  VendorId : $VendorId"
            log "  Product : $ProductId"

            log "Command For Attaching Mirror To rpool"
            if [ $isSlice -eq 1 ];then
                Run "zpool attach -f rpool $RootDisk `echo $RootMirrorDisk s$SliceNum | sed 's/ //g'`"
            else
                Run "zpool attach -f rpool $RootDisk $RootMirrorDisk"
            fi

            $ECHO "Check the rpool status using the command 'zpool status -v rpool' for more details"
            
            UpdateDevices $RootMirrorDisk "INUSE"

            return
        else
            log "Unable to fetch the mirror disk for rpool"
            $ECHO "Unable to fetch the mirror disk for rpool"
            log "Error: Failed to add mirror to rpool"
            $ECHO "Error: Failed to add mirror to rpool"
        fi
    fi 
}

UpdateDevices()
{
    Device=$1
    Status=$2
    tempfile1=/tmp/tempfile1

    grep -v $Device $OUTFILE  > $tempfile1
    newEntry=`grep -w $Device $OUTFILE | sed 's/FREE/'"$Status"'/g'`
    $ECHO $newEntry >> $tempfile1
    sort -n $tempfile1 > $OUTFILE

    rm -f $tempfile1
}

GetLogAndCacheDevs()
{
    NumSSD=`cat $OUTFILE|grep "SSD"|grep "FREE"|wc -l`

    if [ $NumSSD -ne 0 ];then
       
        ReadAns "Would You Like To Specify Log And Cache Devices Among Available SSDs?"
    
        if [ $ANS = "y" -o $ANS = "Y" ]; then
        
            ReadDisk "Please Enter The DiskNumber You Want To Choose For LOG or Press Enter to skip Log and its mirror selection" "SSD"

            if [ ! -z "$SelectedDisk" ];then

                if [ -z "$LogDevices" ];then
                    LogDevices=`echo log $SelectedDisk`
                    log "LogDevices : $LogDevices"
                    UpdateDevices $SelectedDisk "INUSE"
                else
                    LogDevices=`echo $LogDevices $SelectedDisk|sed 's/log/log mirror/g'`
                    log "LogDevices : $LogDevices"
                    UpdateDevices $SelectedDisk "INUSE"
                fi
                AvailableSSD=`cat $OUTFILE|grep "SSD"|grep "FREE"|awk '{print $2}'`

                if [ ! -z "$AvailableSSD" ];then

                    if [ ! -z "$LogDevices" ];then
                                          
                        ReadAns "Do You Want To Mirror The Log Device?"

                        if [ $ANS = "y" -o $ANS = "Y" ]; then
               
                            ReadDisk "Please Enter The DiskNumber You Want To Choose For LOG Mirror" "SSD"
                            if [ -z "$LogDevices" ];then
                                LogDevices=`echo log $SelectedDisk`
                                log "LogDevices : $LogDevices"
                                UpdateDevices $SelectedDisk "INUSE"
                            else
                                LogDevices=`echo $LogDevices $SelectedDisk|sed 's/log/log mirror/g'`
                                log "LogDevices : $LogDevices"
                                UpdateDevices $SelectedDisk "INUSE"
                            fi                      
                        fi
                    fi
                fi
            fi

            AvailableSSD=`cat $OUTFILE|grep "SSD"|grep "FREE"|awk '{print $2}'`

            if [ ! -z "$AvailableSSD" ];then
               
                TMPFILE=/tmp/diskdetails
                $ECHO "Available SSD Disks Are: "
                for eachDisk in `cat $OUTFILE|grep "SSD"|grep "FREE"|awk '{print $2}'`
                do
                   iostat -Eni $eachDisk > $TMPFILE
                   DiskNum=`grep $eachDisk $OUTFILE | awk '{print $1}'`
                   DiskName=`grep $eachDisk $OUTFILE | awk '{print $2}'`
                   DiskSize=`grep $eachDisk $OUTFILE | awk '{print $6 $7}'|sed 's/\[//g' |sed 's/\]//g'`
                   ProductId=`grep "Product" $TMPFILE |awk '{print $4}'`
                   VendorId=`grep "Vendor" $TMPFILE | awk '{print $2}'`
                   DeviceId=`grep "Device Id" $TMPFILE | awk '{print $NF}'`

                   $ECHO "$DiskNum $DiskName $ProductId $DiskSize $VendorId $DeviceId"
                   log "$DiskNum $DiskName $ProductId $DiskSize $VendorId $DeviceId"
                   
                done

                rm -f $TMPFILE
   
                $ECHO "Please Enter the DiskNumbers You Want To Choose For Cache ( As Space Separated ) or Press Enter to skip Cache Selection"
                read DiskNumbers

                for DiskNum in $DiskNumbers
                do
                   sed -n "${DiskNum}{p;q;}" $OUTFILE|grep -w "SSD" |grep -w "FREE" > /dev/null
                   if [ $? -eq 0 ];then
                       SelectedDisk=`sed -n "${DiskNum}{p;q;}" $OUTFILE | awk '{print $2}'`
                       if [ -z "$CacheDevices" ];then
                           CacheDevices=`echo cache $SelectedDisk`
                           log "CacheDevices : $CacheDevices"
                           UpdateDevices $SelectedDisk "INUSE"
                       else
                           CacheDevices=`echo $CacheDevices $SelectedDisk`
                           log "CacheDevices : $CacheDevices"
                           UpdateDevices $SelectedDisk "INUSE"
                       fi
                   else
                       $ECHO "DiskNumber $DiskNum is not available for selection"
                   fi
                done
            fi
        else
            NumSSD=`cat $OUTFILE|grep "SSD"|grep "FREE"|sort -k4n |wc -l`
 
            log "Number of SSD Devices Found Are : $NumSSD"

            if [ $NumSSD -lt 2 ];then
                log "ERROR: Number of SSD Devices Found Are Less Than 2. Cannot Configure Log And Cache"
                $ECHO "ERROR: Number of SSD Devices Found Are Less Than 2. Cannot Configure Log And Cache"
                return
            fi

            SSDForLog=
            SSDForCache=
            
            NumSSDForLog=0
            NumSSDForCache=0

            DiskSizes=` cat inm_disk.dat |grep SSD|grep FREE|sort -k4n -u|awk '{print $4}'`
            for size in $DiskSizes
            do
                NumDisk=`cat $OUTFILE|grep "SSD"|grep "FREE"|grep $size|wc -l`
             
                SSDDevices=`cat $OUTFILE|grep "SSD"|grep "FREE"|sort -k4n |grep $size|awk '{print $2}'`

                #Get the vendors of the SSDs
                for eachssd in $SSDDevices
                do
                    if [ $NumSSDForLog -ne 2 ];then
                        for logvenid in $LOG_SSD_VENDORS
                        do
                           echo $eachssd|grep "$logvenid" > $NULL
                           if [ $? -eq 0 ];then
                               if [ -z "$SSDForLog" ];then
                                   SSDForLog=`echo $eachssd`
                                   NumSSDForLog=`expr $NumSSDForLog + 1`
                               else
                                   SSDForLog=`echo $SSDForLog $eachssd`
                                   NumSSDForLog=`expr $NumSSDForLog + 1`
                               fi
                           fi
                        done
                    fi

                    if [ $NumSSDForCache -ne 2 ];then
                        for cachevenid in $CACHE_SSD_VENDORS
                        do
                           echo $eachssd|grep "$cachevenid" > $NULL
                           if [ $? -eq 0 ];then
                               if [ -z "$SSDForCache" ];then
                                   SSDForCache=`echo $eachssd`
                                   NumSSDForCache=`expr $NumSSDForCache + 1`
                               else
                                   SSDForCache=`echo $SSDForCache $eachssd`
                                   NumSSDForCache=`expr $NumSSDForCache + 1`
                               fi
                           fi
                        done
                    fi
                done
            done

            log "Number of SSDs Found for Log : $NumSSDForLog"
            log "Number of SSDs Found for Cache : $NumSSDForCache"
 
            if [ $NumSSDForLog -eq 0 ];then
                log "ERROR: No Log devices of supporting vendor are not found"
                $ECHO "ERROR: No Log devices of supporting vendor are not found"

                #Need to group the remaining disks according to the size

                for SSDisk in $SSDForCache
                do
                   cat $OUTFILE|grep $SSDisk | grep "FREE" > /dev/null
                   if [ $? -eq 0 ];then
                       size=`grep $SSDisk $OUTFILE|awk '{print $4}'|bc`
                       if [ $size -gt $MIN_CACHE_CAPACITY ];then
                           if [ -z "$CacheDevices" ];then
                               CacheDevices=`echo cache $SSDisk`
                               UpdateDevices $SSDisk "INUSE"
                           else
                               CacheDevices=`echo $CacheDevices $SSDisk`
                               UpdateDevices $SSDisk "INUSE"
                           fi
                       else
                           if [ -z "$LogDevices" ];then
                               LogDevices=`echo log $SSDisk`
                               UpdateDevices $SSDisk "INUSE"
                           else
                               LogDevices=`echo $LogDevices $SSDisk|sed 's/log/log mirror/g'`
                               UpdateDevices $SSDisk "INUSE"
                           fi
                       fi
                   fi
                done
            fi
            
            if [ $NumSSDForCache -eq 0 ];then
                log "ERROR: No Cache devices of supporting vendor are not found"
                $ECHO "ERROR: No Cache devices of supporting vendor are not found"

                #Need to group the remaining disks according to the size
                
                for SSDisk in $SSDForLog
                do
                   cat $OUTFILE|grep $SSDisk | grep "FREE" > /dev/null
                   if [ $? -eq 0 ];then
                       size=`grep $SSDisk $OUTFILE|awk '{print $4}'|bc`
                       if [ $size -gt $MIN_CACHE_CAPACITY ];then
                           if [ -z "$CacheDevices" ];then
                               CacheDevices=`echo cache $SSDisk`
                               UpdateDevices $SSDisk "INUSE"
                           else
                               CacheDevices=`echo $CacheDevices $SSDisk`
                               UpdateDevices $SSDisk "INUSE"
                           fi
                       else
                           if [ -z "$LogDevices" ];then
                               LogDevices=`echo log $SSDisk`
                               UpdateDevices $SSDisk "INUSE"
                           else
                               LogDevices=`echo $LogDevices $SSDisk|sed 's/log/log mirror/g'`
                               UpdateDevices $SSDisk "INUSE"
                           fi
                       fi
                   fi
                done
            fi

            if [ $NumSSDForLog -ne 0 -a $NumSSDForCache -ne 0 ];then

                for SSDisk in $SSDForLog
                do
                   if [ -z "$LogDevices" ];then
                       LogDevices=`echo log $SSDisk`
                       UpdateDevices $SSDisk "INUSE"
                   else
                       LogDevices=`echo $LogDevices $SSDisk|sed 's/log/log mirror/g'`
                       UpdateDevices $SSDisk "INUSE"
                   fi
                done

                for SSDisk in $SSDForCache
                do
                   if [ -z "$CacheDevices" ];then
                       CacheDevices=`echo cache $SSDisk`
                       UpdateDevices $SSDisk "INUSE"
                   else
                       CacheDevices=`echo $CacheDevices $SSDisk`
                       UpdateDevices $SSDisk "INUSE"
                   fi
                done
            fi
       fi
    else
        NumSAS=`cat $OUTFILE|grep SAS|grep FREE|wc -l`
        if [ $NumSAS -eq 0 ];then
            $ECHO "No FREE SAS Devices Are Available"
            return 
        fi  
 
        $ECHO "No SSDs Are Available On The Machine"
        ReadAns "Do You Want To Use The Available SAS Disks For LOG And CACHE?"

        if [ $ANS = "y" -o $ANS = "Y" ]; then

            ReadDisk "Please Enter The DiskNumber You Want To Choose For LOG" "SAS"
            
            if [ -z "$LogDevices" ];then
                LogDevices=`echo log $SelectedDisk`
                log "LogDevices : $LogDevices"
                UpdateDevices $SelectedDisk "INUSE"
            else
                LogDevices=`echo $LogDevices $SelectedDisk|sed 's/log/log mirror/g'`
                log "LogDevices : $LogDevices"
                UpdateDevices $SelectedDisk "INUSE"
            fi    
            
            AvailableSAS=`cat $OUTFILE|grep "SAS"|grep "FREE"|awk '{print $2}'`

            if [ ! -z "$AvailableSAS" ];then

                ReadAns "Do You Want To Mirror The Log Device?"

                if [ $ANS = "y" -o $ANS = "Y" ]; then

                    ReadDisk "Please Enter The DiskNumber You Want To Choose For LOG Mirror" "SAS"
                
                    if [ -z "$LogDevices" ];then
                        LogDevices=`echo log $SelectedDisk`
                        log "LogDevices : $LogDevices"
                        UpdateDevices $SelectedDisk "INUSE"
                    else
                        LogDevices=`echo $LogDevices $SelectedDisk|sed 's/log/log mirror/g'`
                        log "LogDevices : $LogDevices"
                        UpdateDevices $SelectedDisk "INUSE"
                    fi                             
                fi
           fi

           AvailableSAS=`cat $OUTFILE|grep "SAS"|grep "FREE"|awk '{print $2}'`

           if [ ! -z "$AvailableSAS" ];then

                TMPFILE=/tmp/diskdetails
                $ECHO "Available SAS disks are : "
                for eachDisk in `cat $OUTFILE|grep "SAS"|grep "FREE"|awk '{print $2}'`
                do
                    iostat -Eni $eachDisk > $TMPFILE
                    DiskNum=`grep $eachDisk $OUTFILE | awk '{print $1}'`
                    DiskName=`grep $eachDisk $OUTFILE | awk '{print $2}'`
                    DiskSize=`grep $eachDisk $OUTFILE | awk '{print $6 $7}'|sed 's/\[//g' |sed 's/\]//g'`
                    ProductId=`grep "Product" $TMPFILE |awk '{print $4}'`
                    VendorId=`grep "Vendor" $TMPFILE | awk '{print $2}'`
                    DeviceId=`grep "Device Id" $TMPFILE | awk '{print $NF}'`

                    $ECHO "$DiskNum $DiskName $ProductId $DiskSize $VendorId $DeviceId"
                    log "$DiskNum $DiskName $ProductId $DiskSize $VendorId $DeviceId"

                done

                rm -f $TMPFILE

                $ECHO "Please Enter the DiskNumbers You Want To Choose For Cache ( As Space Separated ) or Press Enter to skip Cache Selection"
                read DiskNumbers

                for DiskNum in $DiskNumbers
                do
                   sed -n "${DiskNum}{p;q;}" $OUTFILE|grep -w "SAS" |grep -w "FREE" > /dev/null
                   if [ $? -eq 0 ];then
                       SelectedDisk=`sed -n "${DiskNum}{p;q;}" $OUTFILE | awk '{print $2}'`
                       if [ -z "$CacheDevices" ];then
                           CacheDevices=`echo cache $SelectedDisk`
                           log "CacheDevices : $CacheDevices"
                           UpdateDevices $SelectedDisk "INUSE"
                       else
                           CacheDevices=`echo $CacheDevices $SelectedDisk`
                           log "CacheDevices : $CacheDevices"
                           UpdateDevices $SelectedDisk "INUSE"
                       fi
                   else
                       $ECHO "DiskNumber $DiskNum is not available for selection"
                   fi
               done
           fi
       fi
    fi
}

GetSpareDevices()
{
    Size=$1

    NumSAS=`cat $OUTFILE | grep $Size | grep "SAS"| grep "FREE" | wc -l`
    numSpares=0

    if [ $NumSAS -ne 0 ];then
                 
        ReadAns "Would You Like To Specify Spare Disks Among Available SAS Disks?"
                     
        if [ $ANS = "y" -o $ANS = "Y" ]; then

            rem=`expr $NumSAS % 2`
            if [ $rem -eq 0 ];then
                $ECHO "Please Select Even Number of Spare Disks"
            else
                $ECHO "Please Select Odd Number of Spare Disks"
            fi

            cat $OUTFILE | grep $Size | grep "SAS"| grep "FREE"
            $ECHO "Please Enter the DiskNumbers You Want To Choose For Spares ( As Space Separated ) or Press Enter to skip the Selection"
            read DiskNumbers

            for DiskNum in $DiskNumbers
            do
                sed -n "${DiskNum}{p;q;}" $OUTFILE|grep $Size | grep -w "SAS" |grep -w "FREE" > /dev/null
                if [ $? -eq 0 ];then
                    SelectedDisk=`sed -n "${DiskNum}{p;q;}" $OUTFILE | awk '{print $2}'`
                    if [ -z $SpareDevices ];then
                        SpareDevices=`echo $SelectedDisk`
                    else
                        SpareDevices=`echo $SpareDevices $SelectedDisk`
                    fi
                    numSpares=`expr $numSpares + 1`
                else
                    log "ERROR: DiskNumber $DiskNum is not available for selection"
                    $ECHO "ERROR: DiskNumber $DiskNum is not available for selection"
                fi
            done
        fi
    else
       $ECHO "No FREE SAS Devices Are Available"
    fi

    if [ $numSpares -ne 0 ];then
        RemSASDisks=`expr $NumSAS - $numSpares`
        log "Remaining Disks after Choosing Spare Disks are : $RemSASDisks"

        if [ `expr $RemSASDisks % 2` -eq 0 ];then
            log "There Are Sufficient Number of Disks for mirror pairs"
            prevDiskSlot=0
            for eachspare in $SpareDevices
            do
                DiskSlot=`grep $eachspare $OUTFILE | awk '{print $9}'`
                if [ $prevDiskSlot -eq 0 ];then
                    prevDiskSlot=`echo $DiskSlot`
                else
                    if [ `expr $prevDiskSlot % 2` -eq 1 ];then
                        if [ `expr $prevDiskSlot + 1` -eq $DiskSlot ];then
                            log "ERROR: Disks in the same Bay are selected for Spare Disks"
                            $ECHO "ERROR: Disks in the same Bay are selected for Spare Disks"
                            log "Spare Disks Seleted : $SpareDevices"
                            $ECHO "Spare Disks Seleted : $SpareDevices"
                            exit 1
                        fi
                    fi
                fi

                UpdateDevices $eachspare "INUSE"
            done   
            SpareDevices=`echo spare $SpareDevices`
        else
            log "ERROR: There Are Odd Number of Disks Left After Spare Disks Reservation,which May cause zpool creation error"
            $ECHO "ERROR: There Are Odd Number of Disks Left After Spare Disks Reservation,which May cause zpool creation error"
            exit 1
        fi
    fi
}

CreateStoragePool()
{
    LogDevices=
    CacheDevices=
    PoolDevices=
    SpareDevices=

    #Removing empty lines from $OUTFILE
    sed '/^$/d' $OUTFILE > /tmp/diskdata
    mv -f /tmp/diskdata $OUTFILE

    RootDisk=`zpool status rpool|awk '{print $1}'|grep 'c[0-9]'|head -1`
    RootEnclId=`cat $OUTFILE|grep $RootDisk|awk '{print $8}'`

    zpool status rpool | grep "mirror" > $NULL
    if [ $? -ne 0 ];then
        log "WARNING: Root Pool Is Not Mirrored"
        $ECHO "WARNING: Root Pool Is Not Mirrored"

        RootDiskCapacity=`cat $OUTFILE|grep $RootDisk|awk '{print $4}'`
        RootDiskType=`cat $OUTFILE|grep $RootDisk|awk '{print $3}'`
        RootDiskSlot=`cat $OUTFILE|grep $RootDisk|awk '{print $9}'`

        RootDiskVendor=`cat /tmp/vendorinfo|grep $RootDisk|awk '{print $2}'`

        isMirrorReserved=0

        PsblMirrorDisks=`cat $OUTFILE|sort -k4n|grep $RootDiskCapacity|grep $RootDiskType|grep $RootEnclId|grep "FREE"|awk '{print $2}'`
        nPsblMDsks=`cat $OUTFILE|sort -k4n|grep $RootDiskCapacity|grep $RootDiskType|grep $RootEnclId|grep "FREE"|awk '{print $2}'|wc -l`

        if [ -z "$PsblMirrorDisks" ];then
            PsblMirrorDisks=`cat $OUTFILE|sort -k4n|grep $RootDiskCapacity|grep $RootDiskType|grep "FREE"|awk '{print $2}'`
            nPsblMDsks=`cat $OUTFILE|sort -k4n|grep $RootDiskCapacity|grep $RootDiskType|grep "FREE"|awk '{print $2}'|wc -l`
        fi

        for Disk in $PsblMirrorDisks
        do
           if [ $nPsblMDsks -eq 1 ];then
               log "Reserving the Disk $PsblMirrorDisks for root pool mirrorring"
               UpdateDevices $PsblMirrorDisks "RESERVED"
               isMirrorReserved=1
           elif [ $nPsblMDsks -gt 1 ];then
                DiskVendor=`cat /tmp/vendorinfo|grep $Disk|awk '{print $2}'`
                if [ "$RootDiskVendor" = "$DiskVendor" ];then
                   log "Reserving the Disk $Disk for root pool mirrorring"
                   UpdateDevices $Disk "RESERVED"
                   isMirrorReserved=1
                   break
               fi
           else
               log "Warning : No Disk is reserved for rpool mirrorring"
           fi
        done
    fi

    Enclosures=`cat $OUTFILE|sort -k8,8 -u|awk '{print $8}'`
    NumDisksRootEncl=`cat $OUTFILE|grep $RootEnclId|wc -l`

    PoolDevices=
    numExtraEvenDisk=0
    numExtraOddDisk=0
  
    DiskSizes=`cat $OUTFILE |grep "SAS"|grep "FREE"|sort -k4,4 -u|awk '{print $4}'`

    if [ -z "$DiskSizes" ];then
        $ECHO "Sufficient HDDs are not available for pool creation"
        return
    fi

    maxSizedDisks=0
    for Size in $DiskSizes
    do
       numDisks=`cat $OUTFILE |grep "SAS"|grep "FREE"|grep $Size|wc -l`
       if [ $numDisks -gt $maxSizedDisks ];then
           maxSizedDisks=`echo $numDisks`
           MaxSize=`echo $Size`
       fi
    done

    s=0
    for Size in $DiskSizes
    do
       numDisks=`cat $OUTFILE |grep "SAS"|grep "FREE"|grep $Size|wc -l`
       if [ $numDisks -lt 2 ];then
           log " Number of Disks of size $Size are : $numDisks"
           log " Cannot create Pool with disks of size $Size"
           continue
       fi

       sz=`cat $OUTFILE | grep $Size | awk '{print $6 " " $7}' |sed 's/\[//g' |sed 's/\]//g'|uniq`

       ReadAns "Would You Like To Have Log And Cache For The Pool With Disks Of Size : $sz ?" 
        
       if [ $ANS = "y" -o $ANS = "Y" ]; then
           GetLogAndCacheDevs

           $ECHO "LogDevices: $LogDevices"
           $ECHO "CacheDevices: $CacheDevices"
       fi

       isSpareRequired=0
       if [ $numDisks -gt 2 ];then
           isSpareRequired=1 
       fi

       numExtraEvenDisks=0
       numExtraOddDisks=0

       TOTALODDDISKS=/tmp/totalodddisks
       TOTALEVENDISKS=/tmp/totalevendisks
       EXTRAODDDISKS=/tmp/extraodddisks
       EXTRAEVENDISKS=/tmp/extraevendisks
       MIRRORPAIRS=/tmp/mirrorpairs
       
       >$TOTALODDDISKS
       >$TOTALEVENDISKS
       >$EXTRAODDDISKS
       >$EXTRAEVENDISKS
       >$MIRRORPAIRS

       if [ $isSpareRequired -eq 1 ];then
           GetSpareDevices $Size 

           if [ ! -z $SpareDevices ];then
               log "SpareDevices : $SpareDevices"
               isSpareRequired=0
           fi
       fi

       for Encl in $Enclosures
       do
          NumOddDisks=`cat $OUTFILE|grep "FREE"|grep $Encl|grep "SAS"|grep $Size|sort -k9n|awk '{if($9%2 != 0)print $2;}'|wc -l`
          if [ `expr $NumOddDisks % 2` -eq 1 ];then
              ExtraOddDisk=`cat $OUTFILE|grep "FREE"|grep $Encl|grep "SAS"|grep $Size|sort -k9n|awk '{if($9%2 != 0)print $2;}'|tail -1`
              log " ExtraOddDisk : $ExtraOddDisk"
              $ECHO "$ExtraOddDisk" >> $EXTRAODDDISKS

              numExtraOddDisks=`expr $numExtraOddDisks + 1`
              OddDisks=`cat $OUTFILE|grep "FREE"|grep $Encl|grep "SAS"|grep $Size|sort -k9n|grep -v $ExtraOddDisk|awk '{if($9%2 != 0)print $2;}'`
              $ECHO "$OddDisks" >> $TOTALODDDISKS
              $ECHO "$OddDisks" >> $MIRRORPAIRS 
          else
              OddDisks=`cat $OUTFILE|grep "FREE"|grep $Encl|grep "SAS"|grep $Size|sort -k9n|awk '{if($9%2 != 0)print $2;}'`
              $ECHO "$OddDisks" >> $TOTALODDDISKS
              $ECHO "$OddDisks" >> $MIRRORPAIRS 
          fi

          NumEvenDisks=`cat $OUTFILE|grep "FREE"|grep $Encl|grep "SAS"|grep $Size|sort -k9n|awk '{if($9%2 == 0)print $2;}'|wc -l`
          if [ `expr $NumEvenDisks % 2` -eq 1 ];then
              ExtraEvenDisk=`cat $OUTFILE|grep "FREE"|grep $Encl|grep "SAS"|grep $Size|sort -k9n|awk '{if($9%2 == 0)print $2;}'|tail -1`
              log " ExtraEvenDisk : $ExtraEvenDisk"
              $ECHO "$ExtraEvenDisk" >> $EXTRAEVENDISKS

              numExtraEvenDisks=`expr $numExtraEvenDisks + 1`
              EvenDisks=`cat $OUTFILE|grep "FREE"|grep $Encl|grep "SAS"|grep $Size|sort -k9n|grep -v $ExtraEvenDisk|awk '{if($9%2 == 0)print $2;}'`
              $ECHO "$EvenDisks" >> $TOTALEVENDISKS 
              $ECHO "$EvenDisks" >> $MIRRORPAIRS 
          else
              EvenDisks=`cat $OUTFILE|grep "FREE"|grep $Encl|grep "SAS"|grep $Size|sort -k9n|awk '{if($9%2 == 0)print $2;}'`
              $ECHO "$EvenDisks" >> $TOTALEVENDISKS 
              $ECHO "$EvenDisks" >> $MIRRORPAIRS 
          fi

       done
      
       numSpares=0

       TMPFILE2=/tmp/tempfile2
       TMPFILE3=/tmp/tempfile3
       if [ $isSpareRequired -eq 1 ];then

           if [ $numExtraOddDisks -eq 0 ];then

               if [ $numExtraEvenDisks -eq 0 ];then
                   if [ `expr $numDisks % 2 ` -eq 0 ];then
                       SpareDevices=`cat $TOTALEVENDISKS | tail -2`
                       numSpares=`expr $numSpares + 2`
                       for eachspare in $SpareDevices
                       do
                          grep -v $eachspare $TOTALEVENDISKS > $TMPFILE2
                          mv -f $TMPFILE2 $TOTALEVENDISKS

                          grep -v $eachspare $MIRRORPAIRS > $TMPFILE3
                          mv -f $TMPFILE3 $MIRRORPAIRS
                       done
                   else
                       SpareDevices=`cat $TOTALEVENDISKS | tail -1`
                       numSpares=`expr $numSpares + 1`
                       grep -v $SpareDevices $TOTALEVENDISKS > $TMPFILE2
                       mv -f $TMPFILE2 $TOTALEVENDISKS

                       grep -v $SpareDevices $MIRRORPAIRS > $TMPFILE3
                       mv -f $TMPFILE3 $MIRRORPAIRS
                   fi
               else
                   if [ `expr $numExtraEvenDisks % 2` -eq 0 ];then
                       SpareDevices=`cat $EXTRAEVENDISKS | tail -2`
                       numSpares=`expr $numSpares + 2`
                       for eachspare in $SpareDevices
                       do
                          grep -v $eachspare $EXTRAEVENDISKS > $TMPFILE2
                          mv -f $TMPFILE2 $EXTRAEVENDISKS
                       done
                   else
                       SpareDevices=`cat $EXTRAEVENDISKS | tail -1`
                       numSpares=`expr $numSpares + 1`
                       grep -v $SpareDevices $EXTRAEVENDISKS > $TMPFILE2
                       mv -f $TMPFILE2 $EXTRAEVENDISKS
                   fi
               fi
           else
               if [ $numExtraEvenDisks -eq 0 ];then
                   if [ `expr $numExtraOddDisks % 2` -eq 0 ];then
                       SpareDevices=`cat $EXTRAODDDISKS | tail -2`
                       numSpares=`expr $numSpares + 2`
                       for eachspare in $SpareDevices
                       do
                          grep -v $eachspare $EXTRAODDDISKS > $TMPFILE2
                          mv -f $TMPFILE2 $EXTRAODDDISKS
                       done
                   else
                       SpareDevices=`cat $EXTRAODDDISKS | tail -1`
                       numSpares=`expr $numSpares + 1`
                       grep -v $SpareDevices $EXTRAODDDISKS > $TMPFILE2
                       mv -f $TMPFILE2 $EXTRAODDDISKS
                   fi
               else
                   if [ `expr $numExtraOddDisks % 2` -eq 1 ];then
                       SpareDeviceOdd=`cat $EXTRAODDDISKS | tail -1`
                       numSpares=`expr $numSpares + 1`
                       grep -v $SpareDeviceOdd $EXTRAODDDISKS > $TMPFILE2
                       mv -f $TMPFILE2 $EXTRAODDDISKS
                   fi
                   if [ `expr $numExtraEvenDisks % 2` -eq 1 ];then
                       SpareDeviceEven=`cat $EXTRAEVENDISKS | tail -1`
                       numSpares=`expr $numSpares + 1`
                       grep -v $SpareDeviceEven $EXTRAEVENDISKS > $TMPFILE2
                       mv -f $TMPFILE2 $EXTRAEVENDISKS 
                   fi

                   if [ $numSpares -eq 0 ];then
                       SpareDevices=`cat $EXTRAEVENDISKS | tail -2`
                       numSpares=`expr $numSpares + 2`
                       for eachspare in $SpareDevices
                       do
                          grep -v $eachspare $EXTRAEVENDISKS > $TMPFILE2
                          mv -f $TMPFILE2 $EXTRAEVENDISKS
                       done
                   else
                       SpareDevices=`echo $SpareDeviceOdd $SpareDeviceEven`
                   fi
               fi
           fi

           if [ $numSpares -ne 0 ];then
               log "SpareDevices : $SpareDevices"
               for eachspare in $SpareDevices
               do
                  UpdateDevices $eachspare "INUSE"
               done   
               SpareDevices=`echo spare $SpareDevices`
           fi
       fi

       cp -f $MIRRORPAIRS $TMPFILE3
       sed '/^$/d' $TMPFILE3 > $MIRRORPAIRS

       numDisksForPairs=`cat $MIRRORPAIRS|wc -l`
       if [ `expr $numDisksForPairs % 2` -eq 1 ];then
           ExtraDisk=`cat $MIRRORPAIRS|tail -1`
           grep -v $ExtraDisk $MIRRORPAIRS > $TMPFILE2
           mv -f $TMPFILE2 $MIRRORPAIRS
       fi

       i=0

       for Disk in `cat $MIRRORPAIRS`
       do
           i=`expr $i + 1`
           if [ `expr $i % 2` -eq 1 ];then
               PairDisk1=`echo $Disk`
               #PoolDevices=`echo $PoolDevices mirror $Disk`
               #UpdateDevices $Disk "INUSE"
           else
               #PoolDevices=`echo $PoolDevices $Disk`
               #UpdateDevices $Disk "INUSE"
               if [ ! -z "$Disk" ];then
                   PairDisk2=`echo $Disk`
                   MirrorPair=`echo mirror $PairDisk1 $PairDisk2`
                                                          
                   PoolDevices=`echo $PoolDevices $MirrorPair`
                   UpdateDevices $PairDisk1 "INUSE"
                   UpdateDevices $PairDisk2 "INUSE"
               fi
           fi
       done

       i=0

       for Disk in `cat $EXTRAODDDISKS`
       do
           i=`expr $i + 1`
           if [ `expr $i % 2` -eq 1 ];then
               PairDisk1=`echo $Disk`
               #PoolDevices=`echo $PoolDevices mirror $Disk`
               #UpdateDevices $Disk "INUSE"
           else
               #PoolDevices=`echo $PoolDevices $Disk`
               #UpdateDevices $Disk "INUSE"
               if [ ! -z "$Disk" ];then
                   PairDisk2=`echo $Disk`
                   MirrorPair=`echo mirror $PairDisk1 $PairDisk2`

                   PoolDevices=`echo $PoolDevices $MirrorPair`
                   UpdateDevices $PairDisk1 "INUSE"
                   UpdateDevices $PairDisk2 "INUSE"
               fi
           fi
       done

       i=0

       for Disk in `cat $EXTRAEVENDISKS`
       do
           i=`expr $i + 1`
           if [ `expr $i % 2` -eq 1 ];then
               PairDisk1=`echo $Disk`
               #PoolDevices=`echo $PoolDevices mirror $Disk`
               #UpdateDevices $Disk "INUSE"
           else
               #PoolDevices=`echo $PoolDevices $Disk`
               #UpdateDevices $Disk "INUSE"
               if [ ! -z "$Disk" ];then
                   PairDisk2=`echo $Disk`
                   MirrorPair=`echo mirror $PairDisk1 $PairDisk2`

                   PoolDevices=`echo $PoolDevices $MirrorPair`
                   UpdateDevices $PairDisk1 "INUSE"
                   UpdateDevices $PairDisk2 "INUSE"
               fi
           fi
       done

       numDisks=`cat $OUTFILE |grep "SAS"|grep "FREE"|grep $Size|wc -l`
       if [ $numDisks -ne 0 ];then
           log "Number of Disks not selected for Mirror Pool devices : $numDisks"
           if [ $numDisks -eq 2 ];then
               Disk1=`cat $OUTFILE |grep "SAS"|grep "FREE"|grep $Size|sort -k9n|awk '{print $2}'|head -1`
               EnclDisk1=`cat $OUTFILE |grep "$Disk1" |awk '{print $8}'`
               SlotDisk1=`cat $OUTFILE |grep "$Disk1" |awk '{print $9}'`
               MirrorDisk1=`cat $OUTFILE |grep "SAS"|grep "FREE"|grep $Size|sort -k9n|awk '{print $2}'|tail -1`
               EnclMirrorDisk1=`cat $OUTFILE |grep "$MirrorDisk1" | awk '{print $8}'`
               SlotMirrorDisk1=`cat $OUTFILE |grep "$MirrorDisk1" |awk '{print $9}'`

               if [ `expr $SlotDisk1 + 1` -eq $SlotMirrorDisk1 ];then
                   if [ $EnclDisk1 = $EnclMirrorDisk1 ];then
                        $ECHO "Disks Remaining reside on the same bay."
                        exit 1
                   fi
               fi

               MirrorPair=`echo mirror $Disk1 $MirrorDisk1`
               PoolDevices=`echo $PoolDevices $MirrorPair`
               UpdateDevices $Disk1 "INUSE"
               UpdateDevices $MirrorDisk1 "INUSE"
           else
               $ECHO "ERROR: Few disks are not selected for creating the pool "
               $ECHO "Number of such disks are : $numDisks"
               exit 1
           fi
       fi

       log "Command For zpool Creation: "

       if [ ! -z "$PoolDevices" ];then
           PoolName=`echo "storage $s"|sed 's/ //g'`
           while true
           do
              zpool list | grep -w $PoolName > $NULL
              if [ $? -eq 0 ];then
                  s=`expr $s + 1`
                  PoolName=`echo "storage $s"|sed 's/ //g'`
              else
                  break
              fi
           done

           Run "zpool create -f $PoolName $PoolDevices $SpareDevices $LogDevices $CacheDevices"

           if [ $TEST -eq 0 ];then
               status=`zpool status $PoolName | grep state | awk -F":" '{print $2}' |sed 's/ //g'`
               if [[ "$status" = "ONLINE" ]];then
                    echo "zpool $PoolName is created successfully"
                    log "zpool $PoolName is created successfully"
                    Run "zfs set compression=gzip $PoolName"
               else
                    error=`zpool status $PoolName | grep "errors:" | awk -F":" '{print $2}'`
                    echo "Errors while creating zpool : $error"
               fi  
           else
               $ECHO "zpool create -f $PoolName $PoolDevices $SpareDevices $LogDevices $CacheDevices"
           fi

           LogDevices=
           CacheDevices=
           PoolDevices=
           SpareDevices=
           ExtraEvenDisk=
           ExtraOddDisk=
           ExtraDisk=

           s=`expr $s + 1`
       fi

       rm -f $TOTALODDDISKS
       rm -f $TOTALEVENDISKS
       rm -f $EXTRAODDDISKS
       rm -f $EXTRAEVENDISKS
       rm -f $MIRRORPAIRS

    done
}

ChangeStoragePoolAttr()
{
   $ECHO "Please Enter The Storage Pool Name For Which You Wanted To Change The Attribute: "
   read Ans
   PoolName=$Ans

   zpool list | grep -w "$PoolName" > $NULL
   if [ $? -ne 0 ];then
       $ECHO "Pool '$PoolName' is not valid. Please Enter valid Pool Name"
       log "Pool '$PoolName' is not valid. Please Enter valid Pool Name"
       return
   fi

   zfs get all $PoolName 

   POOLATTR=/tmp/poolattr
   zfs get all $PoolName |awk '{print $2}' > $POOLATTR 

   $ECHO "Please Enter The Attribute You Want To Change : "
   while : 
   do
      read Ans
      Attribute=$Ans
      if [ ! -z $Attribute ];then
          grep -w $Attribute $POOLATTR > $NULL
          if [ $? -ne 0 ];then
             $ECHO "Attribute '$Attribute' is not valid. Please Enter Valid Attribute"
             log "Attribute '$Attribute' is not valid. Please Enter Valid Attribute"
          else
             break
          fi
      else
          rm -f $POOLATTR
          return
      fi
   done

   $ECHO "Please Enter The Value Of The Attribute To Which You Change: "
   read Ans
   Value=$Ans

   Run "zfs set $Attribute=$Value $PoolName"     

   rm -f $POOLATTR

   Val=`zfs get all $PoolName | grep -w "$Attribute" | awk '{print $3}'`
   if [ "$Val" = "$Value" ];then
       $ECHO "Command Executed Successfully."
       log "Command Executed Successfully."
   else
       $ECHO "Command did not execute Successfully."
       log "Command did not execute Successfully."
       $ECHO "`zfs get all $PoolName | grep -w "$Attribute"`"
       log "`zfs get all $PoolName | grep -w "$Attribute"`"
       exit 1
   fi
}

InitializeDisk()
{
   Disks=`cat $OUTFILE | awk '{print $2}'`
        
   DISKOUT=/tmp/diskout
   for Disk in $Disks
   do
      echo | format | grep $Disk > $DISKOUT
      egrep -e "cyl|alt|hd" $DISKOUT > $NULL
      if [ $? -eq 0 ];then
          Run "format -e $Disk"
          #Run "printf "label\r1\ry\rq" | format -e $Disk"
      fi
   done
}

PromptUser()
{
    while :
    do
        clear
        $ECHO
        $ECHO "Please enter the action want to perform"
        $ECHO "1. Root Pool Mirroring"
        $ECHO "2. zpool Creation"
        $ECHO "3. Change The Attributes Of zpool"
        $ECHO "4. Initialize Disks"
        $ECHO "5. Exit"
        $ECHO
        ReadOpt "1 2 3 4 5"

        case $OPT in
        1)
           CreateRpoolMirror
           $ECHO "Please press enter to continue"
           read cr
        ;;
        2)
           CreateStoragePool
           $ECHO "Please press enter to continue"
           read cr
        ;;
        3)
           ChangeStoragePoolAttr
           $ECHO "Please press enter to continue"
           read cr
        ;;
        4) 
           InitializeDisk
           $ECHO "Please press enter to continue"
           read cr
           ;;
        5) 
           return
        ;;
       esac
   done
}

##################### Main ###################

SetupLogs

gotFile=0
TEST=0

if [ "$1" = "-t" ]; then
    TEST=1
elif [ ! -z "$1" ];then
    if [ -f ${SCRIPT_DIR}/$1 ];then
        OUTFILE=${SCRIPT_DIR}/$1
        gotFile=1
    else
        $ECHO "File $OUTFILE does not exist under ${SCRIPT_DIR}"
        exit 1
    fi
fi

if [ $# -eq 2 ];then
    if [ -f ${SCRIPT_DIR}/$2 ];then
        OUTFILE=${SCRIPT_DIR}/$2
        gotFile=1
    else
        $ECHO "File $OUTFILE does not exist under ${SCRIPT_DIR}"
        exit 1
    fi
fi

if [ $gotFile -ne 1 ];then
    GetDiskConfig
else
    if [ -f /tmp/vendorinfo ];then
        rm -f /tmp/vendorinfo
    fi

    iostat -iEn > /tmp/iostat.out
    while read line
    do
       case $line in
           *c[0-9]t*)
               Disk=`echo $line|awk '{print $1}'`
            ;;
           *Product*)
               Product=`echo $line|awk '{print $4}'`
               DeviceId=`echo $line|awk '{print $NF}'|sed 's/id1,sd@n//g'`
               Vendor=`echo $line|awk '{print $2}'`
               echo "$Disk $Product $DeviceId $Vendor" >> /tmp/vendorinfo
            ;;
       esac
    done</tmp/iostat.out
fi

PromptUser

if [ -f /tmp/vendorinfo ];then
    rm -f /tmp/vendorinfo
fi

rm -f /tmp/iostat.out

exit 0


