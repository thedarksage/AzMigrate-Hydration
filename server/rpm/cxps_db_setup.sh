#!/bin/sh

# script to turn cxps on/off in the db
#
# usage: cxps_db_setup.sh on | off
#
# note:
#  after turning the cxps off in the db, you should run
#
#       cxps_setup.sh off 
#
#  all all process servers. it is not needed if the cxps 
#  is not causing any issues
#
# before turning cxps on in the db you must run
#
#       cxps_setup.sh on
#
#  on all process server to make sure the cxps is available
#

DEFAULT_PROTOCOL=`mysql --user=root --password= --batch -N --execute="select psd.remoteTransportProtocolId from processServerDefaultTransportProtocol psd where psd.os = 'linux'" svsdb1`

if [ "on" == "$1" ] ; then
	 NEW_PROTOCOL="1"
	 OLD_PROTOCOL="0"
	 CHECK_NULL=
	 MODE="new transport"
else 
	 if [ "off" == "$1" ] ; then
		  NEW_PROTOCOL="0"
		  OLD_PROTOCOL="1"
		  CHECK_NULL="not"
		  MODE="old transport"
	 else 
		  echo " "
		  echo "usage: cxps_db_setup.sh on | off"
		  echo " "
		  exit 0
	 fi
fi

PS_QUERY="select distinct name from hosts h, processServer ps, processServerDefaultTransportProtocol dp where h.id = ps.processServerId and (ps.remoteTransportProtocolId = $OLD_PROTOCOL or (ps.remoteTransportProtocolId is null and dp.os = 'linux' and not dp.remoteTransportProtocolId = $NEW_PROTOCOL))"

# get current process servers using ftp
PS=`mysql --user=root --password= --batch -N --execute="$PS_QUERY" svsdb1`

# show user process servers that can be switched to new transport
echo ""
echo "Available Process Servers:"
for i in $PS ; do
	 echo "  $i"
done

# ask user which process server(s) to switch currently can either do all or one
while true ; do
	 echo ""
	 read -p "Enter Process Server to switch to the $MODE [default all]: " UPDATE_PS
	 if [ "" = "$UPDATE_PS" ] || [ "all" = "$UPDATE_PS" ] ; then
		  UPDATE_PS=all
		  break
	 else
		  for i in $PS ; do
				if [ "$i" = "$UPDATE_PS" ] ; then
					 FOUND="true"
					 break
				fi
		  done
		  if [ "" = "$FOUND" ] ; then 
				echo " "
				echo $UPDATE_PS is not a valid Process Server
		  else
				break
		  fi
	 fi
done


if [ "on" == "$1" ] ; then
	 while true ; do
		  echo ""
		  echo "You must have run \"cxps_setup.sh on\" on $UPDATE_PS process server(s) "
		  read -p "before configuring the DB. Continue [y/n]: " OK
		  case $OK in
				y|Y)
					 break
					 ;;
				n|N)
					 exit 0
					 ;;
		  esac
	 done
else 
	 echo "You must run \"cxps_setup.sh off\" on $UPDATE_PS process server(s)."
fi

# get process server ids
if [ "all" = "$UPDATE_PS" ] ; then
	 LV_PS_ID_EQU=""
	 PS_ID_EQU=""
else
	 PS_ID=`mysql --user=root --password= --batch -N --execute="select id from hosts where name = '$UPDATE_PS'" svsdb1`
	 LV_PS_ID_EQU=" and sd.processServerId='$PS_ID'"
	 PS_ID_EQU=" where processServerId='$PS_ID'"
fi

# set process server(s) to use new transport so new pairs will be set to use new transport
mysql --user=root --password= --batch -N --execute="update processServer set remoteTransportProtocolId = $NEW_PROTOCOL $PS_ID_EQU" svsdb1

# query to update locgial volumes
SQL="update logicalVolumes lv1, (select hostId, deviceName from logicalVolumes lv, srcLogicalVolumeDestLogicalVolume sd where (lv.hostId = sd.sourceHostId and lv.deviceName = sd.sourceDeviceName and lv.transProtocol = $OLD_PROTOCOL $LV_PS_ID_EQU) or (lv.hostId = sd.destinationHostId and lv.deviceName = sd.destinationDeviceName and lv.transProtocol = $OLD_PROTOCOL $LV_PS_ID_EQU)) lv2 set lv1.transProtocol = $NEW_PROTOCOL where lv1.hostId = lv2.hostId and lv1.deviceName = lv2.deviceName"

# set logical volumes that are currently being protected to use new transport
mysql --user=root --password= --batch -N --execute="$SQL" svsdb1

echo " "
