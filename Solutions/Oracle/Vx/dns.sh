#!/bin/bash
##
##
# This function verifies the exit status of a program and print messages accordingly
##

function usage()
{
	 echo " "
	 echo " Usage: ./dns.sh {-failover | -failback} "
	 echo "                 -s   <SOURCE_SERVER> "
	 echo "                [-sz  <SOURCE_ZONE>]"
	 echo "                {-t   <TARGET_SERVER> | -ip <SOURCE_SERVER_IP>}"
	 echo "                [-dns <DNS_SERVER>]"
	 echo " 		-kn  <KEY_NAME>"
	 echo "                 -kf  <KEY_FILE>"
	 echo " "
	 echo "  -failover		This option is for dns failover from <SOURCE_SERVER> to <TARGET_SERVER>"
	 echo "  -failback 		This option is for dns failback to <SOURCE_SERVER> with <SOURCE_IP> "
	 echo "  -h 			[optional]: Displays this help"
	 echo " "
	 echo "  <SOURCE_SERVER>	Source Server"
	 echo " "
	 echo "  <SOURCE_ZONE>		[optional] Zone of Source Server. If not specified then the script will"
	 echo "			attempt determine the correct zone to update based on the rest of the input"
	 echo " "
	 echo "  <TARGET_SERVER>	Target Server"
	 echo " "
	 echo "  <SOURCE_SERVER_IP>	The Source Server will be assigned this IP address at the DNS Server"
	 echo " "
	 echo "  <DNS_SERVER>		[Optional] DNS Server to be considered for failover/failback. If not specified"
	 echo "			the master server of the correct zone will be considered."
	 echo " " 
	 echo "  <KEY_NAME>		Name of the TSIG key used in /etc/named.conf . Ex: Ksollinux3.inmage.in. "
	 echo " " 
	 echo "  <KEY_FILE>		Name of the TSIG private key file. Ex: Ksollinux3.inmage.in.+157+32024.private "
	 echo " "
	 exit 1
}


#Check for -h option 
if test "$1" == "-h" ; then
	usage
fi

#Check if Command Line Options present or not
if [ -z $1 ]; then
	usage
fi

##
# Process the Command Line Options and Arguments
#
while [ $# -ne 0 ] 
do
	case $1 in
	-failover)
		if [ -z $FAILOVER ]; then
			FAILOVER="1"
		else
			echo "-failover option is already specified..."
			usage
		fi
		shift
		;;

	-failback)
		if [ -z $FAILBACK ]; then
			FAILBACK="1"
		else
			echo "-failback option is already specified..."
			usage
		fi
		shift
		;;

	-s)
		if [ -z $SOURCE ]; then
			SOURCE=$2
		else
			echo "-s option is already specified...."
			usage
		fi
		shift
		shift
		;;
		
	-sz)  
		if [ -z $SOURCE_ZONE ]; then
			SOURCE_ZONE=$2
		else
		        echo "-sz option is already specified...."
			usage
		fi					                						
		shift
		shift
		;;
		
	-t)
		if [ -z $TARGET ]; then
			TARGET=$2
		else
			echo "-t option is already specified...."
			usage
		fi
		shift
		shift
		;;

	-ip)
		if [ -z $IP ]; then
			IP=$2
                else
		        echo "-ip option is already specified...."
			usage
		fi							
		shift
		shift
		;;
	
	-dns)
		if [ -z $DNS_SERVER ]; then
			DNS_SERVER=$2
                else
		        echo "-dns option is already specified...."
			usage
		fi							
		shift
		shift
		;;

        -kn)
                if [ -z $KEY_NAME ]; then
                        KEY_NAME=$2
                else
                        echo "-kn option is already specified...."
                        usage
                fi
                shift
                shift
                ;;

        -kf)
                if [ -z $KEY_FILE ]; then
                        KEY_FILE=$2
                else
                        echo "-kf option is already specified...."
                        usage
                fi
                shift
                shift
                ;;

	*)
		echo "Invalid usage..."
		usage		
		;;
	esac
done

if test "$FAILOVER" == "$FAILBACK" ; then
	echo "Please specify either -failover or -failback option..."
	usage
fi

# Check if Source Server name is entered or not
if [ -z $SOURCE ]; then
	echo "Source Server is mandatory...."
	usage
fi

# Check if Target Server name or IP is entered or not
if [ -z $FAILOVER ]; then
	if [ -z $IP ]; then
		echo "-ip option is mandatory for failback operation...."
		usage
	fi
else
	if [ -z $TARGET ]; then
		echo "-t option is mandatory for failover operation...."
		usage
	fi
fi

# Check if Name of the key file is entered or not
if [ -z $KEY_FILE ]; then
	echo "Key file is mandatory for authentication with DNS Server...."
	usage
fi

# Check if Name of the key file is entered or not
if [ -z $KEY_NAME ]; then
        echo "Key Name is mandatory for authentication with DNS Server...."
        usage
fi


if [ -e inmage_dnsupdate_file ]; then
        rm -f inmage_dnsupdate_file
fi

if test -n "$DNS_SERVER" ; then
	printf "server $DNS_SERVER\n" >> inmage_dnsupdate_file
fi

if test -n "$SOURCE_ZONE" ; then
	printf "zone $SOURCE_ZONE\n" >> inmage_dnsupdate_file
fi

if test -n "$KEY_NAME" ; then
	if test -n "$KEY_FILE" ; then
		KEY_VALUE=`grep Key $KEY_FILE | awk '{ print $2 }'`
		printf "key $KEY_NAME $KEY_VALUE\n" >> inmage_dnsupdate_file
	fi
fi
		


##
# Delete RR for Source Server 

printf "update delete $SOURCE \n">> inmage_dnsupdate_file

printf "send\n" >> inmage_dnsupdate_file

#cat inmage_dnsupdate_file

# Send the update to DNS Server
nsupdate -v inmage_dnsupdate_file

#if [ $? -ne "0" ]; then
#        if test -n "$FAILOVER" ; then
#                echo "ERROR: ".$?."DNS Failover is Unsuccessful..."
#        else
#                echo "ERROR: ".$?."DNS Failback is Unsuccessful..."
#        fi
#	exit 1
#else
#        if test -n "$FAILOVER" ; then
#                echo "DNS Failover is Successful..."
#        else
#                echo "DNS Failback is Successful..."
#        fi
#fi

##
# Add RR for Source Server with Target IP

if [ -e inmage_dnsupdate_file ]; then
        rm -f inmage_dnsupdate_file
fi

if test -n "$DNS_SERVER" ; then
        printf "server $DNS_SERVER\n" >> inmage_dnsupdate_file
fi

if test -n "$SOURCE_ZONE" ; then
        printf "zone $SOURCE_ZONE\n" >> inmage_dnsupdate_file
fi

if test -n "$KEY_NAME" ; then
        if test -n "$KEY_FILE" ; then
                KEY_VALUE=`grep Key $KEY_FILE | awk '{ print $2 }'`
                printf "key $KEY_NAME $KEY_VALUE\n" >> inmage_dnsupdate_file
        fi
fi


if test -n "$FAILOVER" ; then
	if test -n "$TARGET" ; then
		TARGET_IP=`host $TARGET | awk '{ print  $4 }'`
	fi
	printf "update add $SOURCE 86400 IN A $TARGET_IP\n" >> inmage_dnsupdate_file
else
	printf "update add $SOURCE 86400 IN A $IP\n" >> inmage_dnsupdate_file
fi

printf "send\n" >> inmage_dnsupdate_file

#echo "now 2nd setting record"

#cat inmage_dnsupdate_file

# Send the update to DNS Server
nsupdate -v inmage_dnsupdate_file

if [ -e inmage_dnsupdate_file ]; then
        rm -f inmage_dnsupdate_file
fi

