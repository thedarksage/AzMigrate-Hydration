#!/usr/bin/perl

# FILENAME
#   pillarArrayManager.pm
#
# DESCRIPTION
#    This perl module is specific for the pillar disk array
#     information. It has two methods
#     a)getpillarArrayDiscoveryInfo - will return the registeration
#        information of the pillar disk array
#     b)getPillarLunInfo - will return the lun info for the specific
#        pillar disk array
#
#
# HISTORY
#     17 November 2010  SriKeerthy  Created.
#
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================
#
##################################################################

package pillarArrayManager;

use lib qw(/usr/local/InMage/Vx/scripts/common);
use strict;
use Data::Dumper;
use PHP::Serialization qw(serialize unserialize);
use Log;

use Exporter;
our @ISA = 'Exporter';
our @EXPORT = qw($exit_code);

# Logging object for log messages
my $logging_obj = new Log("pcli.log", "PillarArrayManager");

# Declaring some global variables for logging and path definition
my $INSTALL_PATH = $logging_obj->getInstallPath();
#my $LOG_DIR = $CONF_PARAM{"LogPathname"}."array_pcli.log";

# Making a static IQN string to which array serial number will be appended
our $exit_code;
our $IQN_STATIC = "iqn.2002-03.com.pillardata:axiom.ssn.";
our $PCLI_PATH = $INSTALL_PATH."lib/pcli";
our $PCLI_BIN_PATH = $INSTALL_PATH."bin/pcli";
our $ARRAY_SERIAL_NO;

# Differentiate between source and target for Mapping
my $source = "53";
my $restart_resync = "57";
my $delete_mirror = "55";
our $delete_target = "24";
my $delete_target_lun_map = "56";
my $target = "300";

sub parseAX800SlammerInfo
{
    my $slammer = shift;
    my (%array_details) = @_;
    if($slammer =~ /ControllerHba\[\d+\]/)
    {
        my @ControllerHbas = split(/ControllerHba\[\d+\]/, $slammer);
        foreach my $ControllerHba (@ControllerHbas)
        {
            if($ControllerHba =~ /Port\[\d\]/)
            {
                my $portType="";
                if($ControllerHba =~ /FcPortInformation/)
                {
                    $portType = "fcPorts";
                }
                elsif($ControllerHba =~ /IscsiPortInformation/)
                {
                    $portType = "iscsiPorts";
                }
                my @SlammerPorts = split(/Port\[\d\]/, $ControllerHba);
                foreach my $SlammerPort (@SlammerPorts)
                { 
                    my ($wwn, $port_status, $ipAddress) = 0;
                    my @lines = split(/\n/, $SlammerPort);
                    foreach my $line (@lines)
                    {
                        $line =~ s/^\s+//g; #remove the empty spaces at the beginning.
                        if($line =~ /PortName: (.*)$/)
                        {
                            $wwn = $1;
                            $wwn =~ s/\s+//g;
                            $wwn =~ tr/[A-Z]/[a-z]/;
                            $wwn =~ s/\w{2}/$&:/g;
                            chop($wwn);
                        }
                        if($line =~ /PortSfpStatus: (.*)$/)
                        {
                            $port_status = $1;
                        }
                        if($line =~ /IpAddress: (.*)$/)
                        {
                            $ipAddress = $1;
                        }
                        if($port_status and $wwn and $portType eq "fcPorts")
                        {
                            $array_details{$portType}{$wwn}{"portWwn"} = $wwn;
                            $array_details{$portType}{$wwn}{"portStatus"} = $port_status;
                            $array_details{$portType}{$wwn}{"role"} = "IT";
                        }
                        elsif($port_status and $ipAddress and $portType eq "iscsiPorts")
                        {
                            my $iqn = $IQN_STATIC.$array_details{slNo};
                            $iqn =~ tr/[A-Z]/[a-z]/;
                            $array_details{$portType}{$ipAddress}{"ip"} = $ipAddress;
                            $array_details{$portType}{$ipAddress}{"portStatus"} = $port_status;
                            $array_details{$portType}{$ipAddress}{"iqn"} = $iqn;
                            $array_details{$portType}{$ipAddress}{"role"} = "IT";
                        }
                        if($line =~ /TcpPort: (.*)$/)
                        {
                            $array_details{$portType}{$ipAddress}{"port"} = $1;
                        }
                    }
                }#foreach $SlammerPort
            }
        }#foreach $ControllerHba
    }#if($slammer =~ /ControllerHba\[\d+\]/)
    return %array_details;
}

# This sub-routine is called for the argument getArrayInfo
# Returns the array details in a serialized string format
sub getpillarArrayDiscoveryInfo {
    my ($arrayType,%array_details) = @_;
    my ($other_info,$slammer_command);

    # Following command is framed to get the array information
    #usr/local/Inmage/Vx/bin/pcli-RHEL-4-x86 submit -H 10.0.0.39 -u administrator -p pillar GetAxiomSystem
    my $command = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    
    if($arrayType == 1)
	{
		$command .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetAxiomSystem 2>&1';
		
		# Slammer Info
		# Framing the slammer command
		$slammer_command = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';'.$PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetSlammer 2>&1';
	}
	elsif($arrayType == 2)
	{
		$command .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetOracleFsSystem 2>&1';
		# Slammer Info
		# Framing the slammer command
		$slammer_command = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';'.$PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetController 2>&1';
	}
	
	$logging_obj->log("DEBUG","getpillarArrayDiscoveryInfo : array info command is $command\n");
    my ($cmd_op,$return_value) = &ExecutePcliCommand($command);
    $logging_obj->log("DEBUG","getpillarArrayDiscoveryInfo : Command returned $cmd_op\n");

    my ($slammer_op,$slammer_return_value) = &ExecutePcliCommand($slammer_command);
    $exit_code = $slammer_return_value;
    $logging_obj->log("DEBUG","getpillarArrayDiscoveryInfo : command is $slammer_command\n");
    $logging_obj->log("DEBUG","getpillarArrayDiscoveryInfo : Command returned value $slammer_op\n");

    # If the any one of the commands has not executed succesfully assigning the error to command output
    if($cmd_op =~ /errorcode/i || $slammer_op =~ /errorcode/i || $return_value != 0 ||  $slammer_return_value != 0)
    {
        # Assigning status as fail
        $array_details{status} = "fail";
        my $error_message;
        if($cmd_op)
        {
            chomp($cmd_op);
            $error_message = $cmd_op;
        }
        elsif($slammer_op)
        {
            chomp($slammer_op);
            $error_message = $slammer_op;
        }
        if($error_message)
        {
            $error_message =~ s/\s+$//g;
            $array_details{errorMessage} = $error_message;
        }
        # If an error code was contained in the response parsing for it
        if($cmd_op =~ /ErrorCode:\s+\w+/mg or $slammer_op =~ /ErrorCode:\s+\w+/mg)
        {
            my @error = split(":",$&);
            $error[1] =~ s/\s+//g;
            $array_details{errorMessage} = $error[1];
            $logging_obj->log("EXCEPTION","getpillarArrayDiscoveryInfo : failed with error : output : $cmd_op $slammer_op");
        }
        # Serializing the hash
        #my $serialized_array = PHP::Serialization::serialize(\%array_details);
        #return $serialized_array;
        return %array_details;
    }
    # When both srray info and slammer commands are executed successfully without errors
    else
    {
        my @lines = split("\n",$cmd_op);
        # Check if data exists
        my $data_exists = $#lines + 1;
        if($data_exists > 1)
        {
            foreach my $line (@lines)
            {
                # All the slammer information we require has a colon, hence parsing for it
                if($line =~ /:/)
                {
                    # These lines are not required as part of slammer info
                    if($line !~ /CorrelationI|TaskGuid|TaskFqn/)
                    {
                        $line =~ s/^\s+//;
                        $other_info .= $line."\n";
                    }
                    $line =~ s/^\s+//;
                    my @reg_status = split(":",$line);
                    $reg_status[1] =~ s/^\s+//;

                    # Retrieving Name, Model, SerialNumber and assigning the status and command output
                    if($reg_status[0] eq "Model")
                    {
                        $array_details{modelNo} = $reg_status[1];
                    }
                    elsif($reg_status[0] eq  "SerialNumber")
                    {
                        $array_details{slNo} = $reg_status[1];
                        $ARRAY_SERIAL_NO = $reg_status[1];
                        $ARRAY_SERIAL_NO =~ tr/[A-Z]/[a-z]/;
                    }
                    elsif($reg_status[0] eq  "Name")
                    {
                        $array_details{arrayName} = $reg_status[1];
                    }
                    #$array_details{otherInfo} = $cmd_op;
                }
            }
            $array_details{status} = "success";
        }
        # Creating the IQN. This is static for a array
        my $iqn = $IQN_STATIC.$ARRAY_SERIAL_NO;
        $iqn =~ tr/[A-Z]/[a-z]/;
        my @slammers = split("Message",$slammer_op);
        my($slammer_name,$serial_num,$model_slam,$slam_status);
        foreach my $slammer (@slammers)
        {
            if($slammer =~ /SlammerName:\s+(.*)/)
            {
                $other_info .= $&."\n";
            }
            if($slammer =~ /SerialNumber:\s+(.*)/)
            {
                $other_info .= $&."\n";
            }
            if($slammer =~ /SlammerModelType:\s+(.*)/)
            {
                $other_info .= $&."\n";
            }
            if($slammer =~ /OverallSlammerStatus:\s+(.*)/)
            {
                $other_info .= $&."\n";
            }
            #fork a different parsing fn for AX800
            if($array_details{modelNo} eq "AX800" or $array_details{"modelNo"} eq "FS1_2")
            {
                (%array_details) = parseAX800SlammerInfo($slammer, %array_details);
            }
            else
            {
                my @control_units;
                if($slammer =~ /ControlUnit\[\d+\]/)
                {
                    @control_units = split("ControlUnit\[\d+\]",$slammer);
                }
                foreach my $unit (@control_units)
                {
                    my @net_modules;
                    if($unit =~ /NetworkInterfaceModule/)
                    {
                        @net_modules = split("NetworkInterfaceModule",$unit);
                    }
                    foreach my $module (@net_modules)
                    {
                        my @fcs;
                        if($module =~ /FibreChannelPort\[\d+\]/)
                        {
                            @fcs = split("FibreChannelPort\[\d+\]",$module);
                        }
                        foreach my $fc (@fcs)
                        {
                            # For FC Ports we need the information of WWN and the port status
                            my ($wwn, $port_status) = 0;
                            # portWwn=><dataa>;portType=><FC/ISCSI>;role=><I/T/IT>|
                            my @props = split("\n",$fc);
                            foreach my $prop (@props)
                            {
                                if($prop =~ /:/ && $prop =~ /WWN/)
                                {
                                    $prop =~ s/^\s+//;
                                    my @wwn_info = split(":",$prop);
                                    $wwn_info[1] =~ s/\s+//g;
                                    $wwn_info[1] =~ tr/[A-Z]/[a-z]/;
                                    $wwn_info[1] =~ s/\w{2}/$&:/g;
                                    chop($wwn_info[1]);
                                    $wwn = $wwn_info[1];
                                }
                                elsif($prop =~ /:/ && $prop =~ /PortStatus/)
                                {
                                    $prop =~ s/^\s+//;
                                    my @ports_status = split(":",$prop);
                                    $ports_status[1] =~ s/\s+//g;
                                    $port_status = $ports_status[1];
                                }
                                if($wwn && $port_status)
                                {
                                    $array_details{fcPorts}{$wwn}{portWwn} = $wwn;
                                    $array_details{fcPorts}{$wwn}{role} = "IT";
                                    $array_details{fcPorts}{$wwn}{portStatus} = $port_status;
                                }
                            }
                        }
                        my @iscsis;
                        if($module =~ /IScsiPort\[\d+\]/)
                        {
                            @iscsis = split("IScsiPort\[\d+\]",$module);
                            #print "$iscsis[0]\nNEXT\n$iscsis[1]\nNEXT\n$iscsis[2]\nNEXT\n$iscsis[3]\n";
                        }
                        foreach my $iscsi (@iscsis)
                        {
                            # For ISCSI Ports we need the information of IQN,IP,Port and the port status
                            my ($ip,$port,$port_status) = 0;
                            # portWwn=><dataa>;portType=><FC/ISCSI>;role=><I/T/IT>|
                            my @props = split("\n",$iscsi);
                            foreach my $prop (@props)
                            {
                                if($prop =~ /:/ && $prop =~ /IpAddress:/)
                                {
                                    $prop =~ s/^\s+//;
                                    my @ips = split(":",$prop);
                                    $ips[1] =~ s/\s+//g;
                                    $ip = $ips[1];
                                }
                                elsif($prop =~ /:/ && $prop =~ /PortStatus/)
                                {
                                    $prop =~ s/^\s+//;
                                    my @ports_status = split(":",$prop);
                                    $ports_status[1] =~ s/\s+//g;
                                    $port_status = $ports_status[1];
                                }
                                elsif($prop =~ /:/ && $prop =~ /TcpPort/)
                                {
                                    $prop =~ s/^\s+//;
                                    my @ports_name = split(":",$prop);
                                    $ports_name[1] =~ s/\s+//g;
                                    $port = $ports_name[1];
                                }
                                #if($port)
                                if($ip && $port_status && $port)
                                {
                                    $array_details{iscsiPorts}{$ip}{portStatus} = $port_status;
                                    $array_details{iscsiPorts}{$ip}{iqn} = $iqn;
                                    $array_details{iscsiPorts}{$ip}{port} = $port;
                                    $array_details{iscsiPorts}{$ip}{ip} = $ip;
                                    $array_details{iscsiPorts}{$ip}{role} = "IT";
                                }
                            }
                        }
                    }
                }
            } #else ($array_details{modelNo} eq "AX800")
        }
    }
    # Assigning the parsed Array info output and desired slammer info are assigned
    $array_details{otherInfo} = $other_info;
    ##my $serialized_array = PHP::Serialization::serialize(\%array_details);
    #return $serialized_array;
    return %array_details;
}

# This sub-routine outputs the lun information associted with the array
sub getPillarArrayLunInfo(%) {
    my (%array_details) = @_;

    # Storing a file name to be created in /tmp with the pid
    # Creating this file when the command is executed to redirect the standard error
    # Reason is when a shell command is executed with a piped output, the return status will be non-zero
    # though command execution has succeeded
    my $error_file = '/tmp/'.$$;

    # Following command is framed
    # usr/local/InMage/Vx/bin/pcli-RHEL-4-x86 submit -T 30  -H 10.0.0.39 -T 300 -u administrator -p pillar GetLun 2>/tmp/<pid> | grep  -P "ErrorCode|LunInformation|^(\s+)MaximumCapacityInBytes|Luid|Name|Status|VolumeGroupIdentity(\n)(\s+)Id|(\s+)Identity(\n)(\s+)Id|FibreChannelAccessEnabled|IScsiAccessEnabled"
    my $command = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    $command .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetLun 2>'.$error_file.'|egrep "ErrorCode|LunInformation|ExposedCapacityInBytes|MaximumCapacityInBytes|Luid|Name|Status|VolumeGroupIdentity|Id|Fqn:|Identity|FibreChannelAccessEnabled|IScsiAccessEnabled|GlobalLunNumber"';

    $logging_obj->log("DEBUG","getPillarArrayLunInfo : getLunInfo command is $command\n");
    my ($cmd_op,$cmd_return) = &ExecutePcliCommand($command);
    $logging_obj->log("DEBUG","getPillarArrayLunInfo : getLunInfo returned $cmd_op\n");
    $exit_code = $cmd_return;

    # Retrieves the sanHost details to get the map status of the Lun
    my %details = &getSanHostInfo(%array_details);
    #print Dumper(%details);
    #exit;
    if($details{errorMessage})
    {
        return %details;
    }
    # Retrieve the private data for all the mirrored luns
    my %mirror_details = &getLunPrivateData($array_details{ip}, $array_details{uname}, $array_details{pwd});
    # Retrieve all the map details
    my %map_details = &getMapInfo(%array_details);

    # If the command has not executed succesfully assigning the error to command output
    if($cmd_op =~ /errorcode/i || $cmd_return != 0)
    {
        # Assigning the status as fail
        $exit_code = 1;
        $array_details{status} = "fail";
        # If an errorcode is returned by the command
        if($cmd_op =~ /errorcode/i)
        {
            my @error = split(":",$cmd_op);
            $error[1] =~ s/\s+//g;
            $array_details{errorMessage} = $error[1];
        }
        # If command returned non-zero then reading the standard error from the /tmp/<pid> file created
        else
        {
            my $cat_cmd = 'cat '.$error_file;
            my $cat_error_op = `$cat_cmd`;
            $array_details{errorMessage} = $cat_error_op;
        }
        $logging_obj->log("EXCEPTION","getPillarArrayLunInfo : failed with error : output : $cmd_op");
        #my $serialized_array = PHP::Serialization::serialize(\%array_details);
        #return $serialized_array;
        unlink($error_file);
        return %array_details;
    }
    # If command executed successfully
    elsif($cmd_op =~ /LunInformation/)
    {
        my (%lun_hash,$lun_info,@ind_lun_info);
        my %map_hash;
        my %lun_id_map;
        $lun_info = $cmd_op;

        # Status is success
        $array_details{status} = "success";

        # Split with the word LunInformation
        my @messages = split(/LunInformation/,$lun_info);
        foreach my $message (@messages)
        {
            my ($lun_id,$capacity,$device_name,$diskGroup,$status,$lunIdentity,$fc_access,$iscsi_access,$global_lun_number) = "";
            @ind_lun_info = split("\n",$message);
            my $info_length = $#ind_lun_info + 1;
            my (@lun_values);
            for(my $i=0;$i<$info_length;$i++)
            {
                # Parsing and getting only the information required
                if($ind_lun_info[$i] =~ /Luid/)
                {
                    my @lun_values = split(":",$ind_lun_info[$i]);
                    $lun_values[1] =~ s/^\s+//;
                    if($array_details{"modelNo"} eq "AX800" or $array_details{"modelNo"} eq "FS1_2")
                    {
                        $lun_id = "3".$lun_values[1];
                    }
                    elsif($array_details{"modelNo"} eq "AX500" or $array_details{"modelNo"} eq "AX600")
                    {
                        $lun_id = "2".$lun_values[1];
                    }
                    $lun_id =~ tr/[A-Z]/[a-z]/;
                }
                elsif($ind_lun_info[$i] =~ /Name/)
                {
                    my @lun_values = split(":",$ind_lun_info[$i]);
                    $lun_values[1] =~ s/^\s+//;
                    $device_name = $lun_values[1];
                }
                #elsif($ind_lun_info[$i] =~ /MaximumCapacityInBytes/)
                elsif($ind_lun_info[$i] =~ /ExposedCapacityInBytes/)
                {
                    my @lun_values = split(":",$ind_lun_info[$i]);
                    $lun_values[1] =~ s/^\s+//;
                    $capacity = $lun_values[1];
                    #$capacity = $capacity - 131072;
                }
                elsif($ind_lun_info[$i] =~ /^(\s+)Identity/)
                {
                    my @lun_values = split(":",$ind_lun_info[$i+2]);
                    $lun_values[1] =~ s/^\s+//;
                    $lunIdentity = $lun_values[1];
                }
                elsif($ind_lun_info[$i] =~ /Status/)
                {
                    my @lun_values = split(":",$ind_lun_info[$i]);
                    $lun_values[1] =~ s/^\s+//;
                    $status = $lun_values[1];
                }
                elsif($ind_lun_info[$i] =~ /VolumeGroupIdentity/)
                {
                    # If the FQN is not found then constructing here
                    if(!$lunIdentity)
                    {
                        my @lun_values = split(":",$ind_lun_info[$i+2]);
                        $lun_values[1] =~ s/^\s+//;
                        if ($device_name =~ m/^\//)
                        {	
                            $lunIdentity = $lun_values[1].$device_name;
                        }
                        else 
                        {
                            $lunIdentity = $lun_values[1]."/".$device_name;
                        }
                        $logging_obj->log("DEBUG","getPillarArrayLunInfo : The FQN information for SCSI ID - $lun_id was NOT FOUND.Hence vol grp fqn(".$lun_values[1].") + devicename(".$device_name.") is updated as FQN\n");
                    }
                }
                elsif($ind_lun_info[$i] =~ /FibreChannelAccessEnabled/)
                {
                    my @lun_values = split(":",$ind_lun_info[$i]);
                    $lun_values[1] =~ s/^\s+//;
                    $fc_access = $lun_values[1];
                }
                elsif($ind_lun_info[$i] =~ /IScsiAccessEnabled/)
                {
                    my @lun_values = split(":",$ind_lun_info[$i]);
                    $lun_values[1] =~ s/^\s+//;
                    $iscsi_access = $lun_values[1];
                }
                elsif($ind_lun_info[$i] =~ /GlobalLunNumber/)
                {
                    my @lun_values = split(":",$ind_lun_info[$i]);
                    $lun_values[1] =~ s/^\s+//;
                    $global_lun_number = $lun_values[1];
                }
                # Not checking for all as some times the capacity could be empty or fc_access could be zero
                if($lun_id && $lunIdentity) #&& $capacity && $device_name && $capacity && $lunIdentity && $status && $fc_access && $iscsi_access)
                {
                    $array_details{luns}{$lun_id}->{scsiId}      = $lun_id;
                    $array_details{luns}{$lun_id}->{deviceName}  = $device_name;
                    $array_details{luns}{$lun_id}->{capacity}    = $capacity;
                    $array_details{luns}{$lun_id}->{diskGroup}   = $lunIdentity;
                    $array_details{luns}{$lun_id}->{lunStatus}   = $status;
                    $array_details{luns}{$lun_id}->{fcAccess}    = $fc_access;
                    $array_details{luns}{$lun_id}->{iscsiAccess} = $iscsi_access;
                    # Creating a hash with the lun array id as key and scsi id as value for reference
                    $lun_id_map{$lunIdentity} = $lun_id;
                    $array_details{luns}{$lun_id}->{privateData} = $mirror_details{$lunIdentity}{privateData};
                    $array_details{luns}{$lun_id}->{applianceGuid} = $mirror_details{$lunIdentity}{applianceGuid};
                    # If the LUN is a GloballyMappedLun
                    if($global_lun_number)
                    {
                        $array_details{globalLuns}{$lun_id} = $global_lun_number;
                    }
                }
            }
        }
        # For each sanHost
        foreach my $host (keys (%details))
        {
            my $initr_details = $details{$host};
            # For each initiator
            foreach my $initr (keys (%$initr_details))
            {
                my $host_key;
                if($details{$host}{$initr}{wwn})
                {
                    $host_key = $details{$host}{$initr}{wwn};
                }
                if($details{$host}{$initr}{ip})
                {
                    $host_key = $details{$host}{$initr}{ip};
                }
                if($map_details{$host}{$initr})
                {
                    my $maps = $map_details{$host}{$initr};
                    foreach my $lun_id (keys (%$maps))
                    {
                        $array_details{itlMaps}{$host_key}{$lun_id}{initiatorId} = $host_key;
                        $array_details{itlMaps}{$host_key}{$lun_id}{targetId}    = "";
                        $array_details{itlMaps}{$host_key}{$lun_id}{lunNumber}   = $maps->{$lun_id};
                        $array_details{itlMaps}{$host_key}{$lun_id}{lunId}       = $lun_id_map{$lun_id};
                        $array_details{itlMaps}{$host_key}{$lun_id}{hostName}    = $details{$host}{$initr}{name};
                    }
                }
            }
            #$array_details{hostInfo}{$details{$host}{name}
        }
    }
    # If no Lun information is returned
    else
    {
        $array_details{status} = "success";
        $array_details{errorMessage} = "No Luns Associated with Array";
    }
    $array_details{hostInfo} = \%details;
    # The file created for capturing standard error is unlinked here
    unlink($error_file);
    #my $serialized_array = PHP::Serialization::serialize(\%array_details);
    #return $serialized_array;
    return %array_details;
}
#This sub-routine returns the LunInfos information for all the luns in the array.
sub getSanLuns(%)
{
    my (%array_details) = @_;
    my (%LunInfoRecords);
    my $error_file = "/tmp".$$;
    #command for getting info of all the luns in the array.
    my $command = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
#$command .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetLun 2>'.$error_file.'|grep "ErrorCode|LunInformation|^(\s+)MaximumCapacityInBytes|Luid|Name|Status|(\s+)Identity(\n)(\s+)Id:\s\w+\s+Fqn:|FibreChannelAccessEnabled|IScsiAccessEnabled"';
    $command .= $PCLI_BIN_PATH.' submit -H '. $array_details{ip} .' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetLun 2>'.$error_file.'|egrep "ErrorCode|LunInformation|ExposedCapacityInBytes|Luid|Name|Status|Identity|Id|Fqn|FibreChannelAccessEnabled|IScsiAccessEnabled|GlobalLunNumber|VolumeGroupIdentity"';
    $logging_obj->log("DEBUG","getSanLuns : GetLun command is $command\n");
    my ($cmd_op,$cmd_return) = &ExecutePcliCommand($command);
    $logging_obj->log("DEBUG","getSanLuns : getLunInfo returned $cmd_op\n");

    # If the command has not executed succesfully assigning the error to command output
    if($cmd_op =~ /errorcode/i || $cmd_return != 0)
    {
        $exit_code = 1;
        # Assigning the status as fail
        $array_details{status} = "fail";
        # If an errorcode is returned by the command
        if($cmd_op =~ /errorcode/i)
        {
            my @error = split(":",$cmd_op);
            $error[1] =~ s/\s+//g;
            $array_details{errorMessage} = $error[1];
        }
        # If command returned non-zero then reading the standard error from the /tmp/<pid> file created
        else
        {
            my $cat_cmd = 'cat '.$error_file;
            my $cat_error_op = `$cat_cmd`;
            $array_details{errorMessage} = $cat_error_op;
        }
        #my $serialized_array = PHP::Serialization::serialize(\%array_details);
        #return $serialized_array;
        $logging_obj->log("EXCEPTION","getSanLuns : failed with error : output : $cmd_op");
        unlink($error_file);
        return %array_details;
    }
    elsif($cmd_op =~ /LunInformation/)  #The command executed successfully. now start parsing the output.
    {
        $array_details{status} = "success";
        $cmd_op =~ s/\n^(\s+)Id:/:/mg;
        $cmd_op =~ s/\n^(\s+)Fqn:/:/mg;
        my @messageStrings = split(/LunInformation/,$cmd_op);
        foreach my $messageString (@messageStrings)
        {
            my(%lunInfoRecord);
            my $volumeGroupFqn;
            my @lines = split("\n", $messageString);
            foreach my $line (@lines)
            {
                if($line =~ /^(\s+)Identity/)
                {
                    my @values = split(": ", $line);
                    $lunInfoRecord{'Id'} = $values[1];
                    $lunInfoRecord{'Fqn'} = $values[2];
                }
                elsif($line =~ /VolumeGroupIdentity/)
                {
                    my @values = split(": ", $line);
                    $volumeGroupFqn = $values[2];
                }
                elsif($line =~ /:/) #parse only if there is : in the line. omits LunInformation and Identity lines from the output.
                {
                    #$line =~ s/\s+//g; #remove all the spaces.#dont remove spaces before splitting as Fqns may contain ':'
                    my @values = split(": ", $line);
                    $values[0] =~ s/\s+//g;
                    $values[1] =~ s/\s+//g;
                    $lunInfoRecord{$values[0]} = $values[1];
                }
            }

            #convert the selected members into lowercase.
            #$lunInfoRecord{"Id"} =~ tr/[A-Z]/[a-z]/;
            $lunInfoRecord{'Luid'} =~ tr/[A-Z]/[a-z]/;
            $lunInfoRecord{'Luid'} = "2".$lunInfoRecord{'Luid'}; #Add two at the beginning.

            my $lunInfoRecordKey = $lunInfoRecord{'Fqn'};
            if($lunInfoRecordKey eq '')
            {
                $lunInfoRecordKey = $volumeGroupFqn.$lunInfoRecord{'Name'};
            }
            $LunInfoRecords{$lunInfoRecordKey} = \%lunInfoRecord;
        }
        $array_details{LunInfoRecords} = \%LunInfoRecords;
    }
    else
    {
        #There is something really bad as we should never be in this section.
        $exit_code = 1;
        $array_details{status} = "fail";
        $array_details{errorMessage} = "Not sure exactly!";
        $logging_obj->log("EXCEPTION","getSanLuns : failed with error : output : Not Sure exactly");
    }
    return %array_details;
}
#This sub-routine returns the GetSanHostLunMap information for all the luns in the array
sub getSanHostLunMaps(%)
{
    #my (%array_details) = @_;
    my (%array_details) = &fillLunPrivateData(@_);
    my $command = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    my $error_file = "/tmp".$$;
#returns sanhost lun map info for all the sanhosts.
    $command .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetSanHostLunMap 2>'.$error_file.' | egrep "Id|Fqn|LunNumber|ErrorCode|Message"';
    $logging_obj->log("DEBUG", "getSanHostLunMaps : Executing GetSanHostLunMap command: $command");
    my ($cmd_op,$cmd_return) = &ExecutePcliCommand($command);
    $logging_obj->log("DEBUG","getSanHostLunMaps : GetSanHostLunMap command output is $cmd_op\n");
    # If an error is returned
    if($cmd_op =~ /ErrorCode/ || $cmd_return !=0)
    {
        if($cmd_op =~ /ErrorCode/)
        {
            my @error = split(":",$cmd_op);
            $error[1] =~ s/\s+//g;
            $array_details{status} = "fail";
            $array_details{errorMessage} = $error[1];
        }
        else
        {
            my $cat_cmd = 'cat '.$error_file;
            my $cat_error_op = `$cat_cmd`;
            $array_details{errorMessage} = $cat_error_op;
        }
        $logging_obj->log("EXCEPTION","getSanHostLunMaps : failed with error : output : $cmd_op");
        $exit_code = 1;
        unlink($error_file);
        return %array_details;
    }
    else
    {
        $array_details{status} = "success";
        #some cleanup
        $cmd_op =~ s/\n^(\s+)Id:/:/mg;
        $cmd_op =~ s/\n^(\s+)Fqn:/:/mg;
        $cmd_op =~ s/\n^(\s+)InitiatorIdentity:/:/mg;
        $cmd_op =~ s/\n^(\s+)LunNumber:/:/mg;
        my @messageStrings = split(/Message/, $cmd_op);
        foreach my $messageString (@messageStrings)
        {
            my @lines = split("\n", $messageString);
            my $iLineIndex = 0;
            my %lunMapDetails;
            my $sanHostIdentity;
            my $lunIdentity;
            foreach my $line (@lines)
            {
                $line =~ s/^(\s+)//;
                if ($line =~ /SanHostIdentity/)
                {
                    my @values = split(": ", $line);
                    $sanHostIdentity = @values[1];
                    $values[2] =~ s/\///; #remove the leading / from the fqn
                    $lunMapDetails{$sanHostIdentity}{'SanHostFqn'} = $values[2];
                }
                elsif ($line =~ /LunIdentity/)
                {
                    my @values = split(": ", $line);
                    $lunIdentity = $values[2];
                    #$lunMapDetails{'LunFqn'} = $values[2];
                }
                elsif ($line =~ /MapIdentity/)
                {
                    my @values = split(": ", $line);
                    $values[4] =~ s/\///; #remove / from pwwn
                    $values[4] =~ tr/[A-Z]/[a-z]/; #convert to lowercase.
                    $values[5] =~ s/(\s+)//;
                    $lunMapDetails{$sanHostIdentity}{$values[4]} = $values[5];
                }
            }
            foreach my $lunMapKey (%lunMapDetails)
            {
                if($lunMapDetails{$lunMapKey})
                {
                    $array_details{LunInfoRecords}{$lunIdentity}{maps}{$lunMapKey} = $lunMapDetails{$lunMapKey};
                }
            }
        }
        #print Dumper(\%array_details);
    }
    return %array_details;
}

# This sub-rotuine is called as part of getLunInfo to get the sanHosts information
sub getSanHostInfo(%) {
    my (%array_details) = @_;
    my %details;

    # Command framed to get the SanHost info
    # ./pcli-RHEL-4-x86 submit -H 10.0.0.39 -u administrator -p pillar GetSanHost
    my $command = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    $command .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetSanHost';
    if($array_details{sanHostName})
    {
        $command .=' Identity[1].Fqn=/'.$array_details{sanHostName};
    }
    $logging_obj->log("DEBUG","getSanHostInfo : getSanHost command is $command\n");
    my ($cmd_op, $getsanhost_cmd_ret) = &ExecutePcliCommand($command);
    $exit_code = $getsanhost_cmd_ret;
    $logging_obj->log("DEBUG","getSanHostInfo : getSanHost returned $cmd_op\n");

    # If the command has not executed succesfully assigning the error to command output
    if($cmd_op =~ /errorcode/i || $getsanhost_cmd_ret != 0)
    {
        $exit_code = 1;
        $array_details{status} = "fail";
        $array_details{errorMessage} = $cmd_op;
        if($cmd_op =~ /ErrorCode:\s+\w+/mg)
        {
            my @error = split(":",$&);
            $error[1] =~ s/\s+//g;
            $array_details{status} = "fail";
            $array_details{errorMessage} = $error[1];
        }
        $logging_obj->log("EXCEPTION","getSanHostInfo : failed with error : output : $cmd_op");
        return %array_details;
    }
    else
    {
        my @messages = split("Message",$cmd_op);
        foreach my $message (@messages)
        {
            my($host_name,$wwn,$port_id,$ip,$host,$iqn);
            if($message =~ /SanHostInformation/)
            {
                if($message =~ /Fqn:\s+(.*)/)
                {
                    my @hosts = split(":",$&);
                    $hosts[1] =~ s/^\s+//;
                    $hosts[1] =~ s/\///;
                    $host = $hosts[1];
                }
                if($message =~ /IpAddress:\s+(.*)/)
                {
                    my @ip_data = split(/\s+/,$&);
                    $ip_data[1] =~ s/\s+//g;
                    $ip = $ip_data[1];
                    #print "IP - $ip\n";
                }
                # We need the FC and ISCSI port information
                if($message =~ /FcInitiatorPortList/)
                {
                    my @fcports = split("FcInitiatorPort",$message);
                    foreach my $fcport (@fcports)
                    {
#                        print "FC port info is  $fcport\n";
                        if($fcport =~ /\[\d+\]\n\s+Identity\n\s+Id:\s+\w+/)
                        {
                            my $port_matches = $&;
#                            print "MAtched port is $port_matches\n";
                            if($port_matches =~ /Id:\s+\w+/)
                            {
                                my @port_data = split(":",$&);
                                $port_data[1] =~ s/\s+//g;
                                $port_id = $port_data[1];
#                                print "Port Id is $port_id\n";
                            }
                        }
                        if($fcport =~ /WWN:\s+\w+/)
                        {
                            my @wwns = split(":",$&);
                            $wwns[1] =~ s/\s+//g;
                            $wwn = $wwns[1];
#                            print "PWWN is $wwn\n";
                        }
                        if($port_id)
                        {
                            if($message =~ /SanHostInformation\n\s+Identity\n\s+Id:\s+\w+\n\s+Fqn:\s+(.*)\n\s+Name:\s+(.*)/)
                            {
                                my $name_matches = $&;
                                if($name_matches =~ /Name:\s+(.*)/)
                                {
                                    my @names = split(/\s+/,$&);
                                    $names[1] =~ s/\s+//g;
                                    $host_name = $names[1];
                                }
                            }
                            $details{$host}{$port_id}{name} = $host_name;
                            if($wwn)
                            {
                                $details{$host}{$port_id}{wwn} = $wwn;
                            }
                        }
                    }
                }
                if($message =~ /IScsiInitiatorPortList/)
                {
                    my @iscsiports = split("IScsiInitiatorPort",$message);
                    foreach my $iscsi (@iscsiports)
                    {
                        my $flag = 0;
                        if($iscsi =~ /\[\d+\]\n\s+Identity\n\s+Id:\s+\w+\n\s+Fqn:\s+(.*)/)
                        {
                            my $port_matches = $&;
                            #print "Matched port is $iscsi\n";
                            if($port_matches =~ /Id:\s+\w+/)
                            {
                                my @port_data = split(":",$&);
                                $port_data[1] =~ s/\s+//g;
                                $port_id = $port_data[1];
                                #print "port id is $port_id\n";
                            }
                            if($port_matches =~ /Fqn:\s+(.*)/)
                            {
                                my @port_data = split(": ",$&);
                                $port_data[1] =~ s/\s+//g;
                                $port_data[1] =~ s/^\///;
                                $iqn = $port_data[1];
                                #print "port id is $port_id\n";
                            }
                        }
                        if($port_id)
                        {
                            if($message =~ /SanHostInformation\n\s+Identity\n\s+Id:\s+\w+\n\s+Fqn:\s+(.*)\n\s+Name:\s+(.*)/)
                            {
                                my $name_matches = $&;
                                if($name_matches =~ /Name:\s+(.*)/)
                                {
                                    my @names = split(/\s+/,$&);
                                    $names[1] =~ s/\s+//g;
                                    $host_name = $names[1];
                                }
                            }
                            $details{$host}{$port_id}{name} = $host_name;
                            if($ip && !$details{$host}{$port_id}{wwn} && $iqn)
                            {
                                $details{$host}{$port_id}{ip} = $ip;
                                $details{$host}{$port_id}{iqn} = $iqn;
                            }
                        }
                    }
                }
            }
        }
    }
    $logging_obj->log("DEBUG","getSanHostInfo : San Host details are : ".Dumper(%details)."\n");
    return %details;
}
#this subroutine collects the private data from GetMirrorLun pcli call.
sub fillLunPrivateData(%)
{
    my (%array_details) = &getSanLuns(@_);
    my $command = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    my $error_file = "/tmp".$$;
#returns GetMirrorLun info for all the luns.
    $command .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetMirrorLun 2>'.$error_file.' | egrep "Fqn:|PrivateData:|Message"';
    $logging_obj->log("DEBUG", "fillLunPrivateData : Executing GetMirrorLun command: $command\n");
    my ($cmd_op,$cmd_return) = &ExecutePcliCommand($command);
    $logging_obj->log("DEBUG","fillLunPrivateData : GetMirrorLun command output is $cmd_op\n");
    # If an error is returned
    if($cmd_op =~ /ErrorCode/ || $cmd_return !=0)
    {
        if($cmd_op =~ /ErrorCode/)
        {
            my @error = split(":",$cmd_op);
            $error[1] =~ s/\s+//g;
            $array_details{status} = "fail";
            $array_details{errorMessage} = $error[1];
        }
        else
        {
            my $cat_cmd = 'cat '.$error_file;
            my $cat_error_op = `$cat_cmd`;
            $array_details{errorMessage} = $cat_error_op;
        }
        $logging_obj->log("EXCEPTION","fillLunPrivateData : failed with error : output : $cmd_op");
        $exit_code = 1;
        unlink($error_file);
        return %array_details;
    }
    else
    {
        $array_details{status} = "success";
        my @messageStrings = split(/Message/, $cmd_op);
        foreach my $messageString (@messageStrings)
        {
            my $mirrorLunId = "";
            my $mirrorPrivateData = "";
            my @lines = split("\n", $messageString);
            foreach my $line (@lines)
            {
                if($line =~ /:/)
                {
                    $line =~ s/\s+//g;
                    my @values = split(":", $line);
                    if ($values[0] =~ /^Fqn/)
                    {
                        $mirrorLunId = $values[1];
                    }
                    if($values[0] =~ /PrivateData/)
                    {
                        $mirrorPrivateData = $values[1];
                    }
                }
            }
            if($mirrorLunId ne "" && $mirrorPrivateData ne "")
            {
                $array_details{LunInfoRecords}{$mirrorLunId}{'PrivateData'} = $mirrorPrivateData;
            }
        }
    }
    return %array_details;
}
sub getLunPrivateData($) {
    my ($ip,$uname, $pwd) = @_;

    my $error_file = "/tmp".$$;
    my %p_data;
    # Command to be framed to get the private data of the lun
    # /usr/local/INMage/Vx/bin/pcli-RHEL-4-x86 submit -H 10.0.0.39 -u administrator -p pillar GetLunPrivateData
    my $command = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    $command .= $PCLI_BIN_PATH.' submit -H '.$ip.' -T 300 -u '.$uname.' -p '.$pwd.' GetMirrorLun | egrep "Message|\sFqn|PrivateData|ApplianceGuid|ErrorCode" 2>'.$error_file;
    $logging_obj->log("DEBUG","getLunPrivateData : privatedata command is $command\n");
    my ($cmd_op,$priv_data_cmd_ret) = &ExecutePcliCommand($command);
    #$logging_obj->log("DEBUG","getLunPrivateData : privatedata command op is $cmd_op\n");
    $exit_code = $priv_data_cmd_ret;
    $logging_obj->log("DEBUG","getLunPrivateData : privatedata returned $priv_data_cmd_ret\n");

    # If the command has not executed succesfully assigning the error to command output
    if($cmd_op =~ /ErrorCode/)
    {
        print "$cmd_op\n";
        # If command returned an error
        if($cmd_op =~ /ErrorCode:\s+\w+/mg)
        {
            my @error = split(":",$&);
            $error[1] =~ s/\s+//g;
            $p_data{errorMessage} = "Error:".$error[1];
        }
        else
        {
            my $cat_cmd = "cat ".$error_file;
            my $cat_cmd_op = `$cat_cmd`;
            $p_data{errorMessage} = "Error: ".$cat_cmd_op;
        }
        $exit_code = 1;
        $logging_obj->log("EXCEPTION","getLunPrivateData : failed with error : output : $cmd_op");
        exit;
    }
    else
    {
        my @mirrors = split("Message",$cmd_op);
        foreach my $message (@mirrors)
        {
            my ($lun_id,$pvt_data,$app_guid);
            if($message =~ /:/)
            {
                if($message =~ /Fqn:\s+(.*)/)
                {
                    my @data = split(":",$&);
                    $data[1] =~ s/^\s+//;
                    $lun_id = $data[1];
                }
                if($message =~ /PrivateData:\s+(.*)/)
                {
                    my @data = split(": ",$&);
                    $data[1] =~ s/^\s+//;
                    $pvt_data = $data[1];
                }
                if($message =~ /ApplianceGuid:\s+(.*)/)
                {
                    my @data = split(": ",$&);
                    $data[1] =~ s/^\s+//;
                    $data[1] =~ tr/[A-Z]/[a-z]/;
                    $app_guid = $data[1];
                }
            }
            if($lun_id)
            {
                $p_data{$lun_id}{privateData} = $pvt_data;
                $p_data{$lun_id}{applianceGuid} = $app_guid;
            }
        }
    }
    unlink($error_file);
    #my $serialized_array = PHP::Serialization::serialize(\%array_details);
    #$logging_obj->log("DEBUG","getLunPrivateData : MirroDetails are ".Dumper(%p_data)."\n");
    return %p_data;
}

# This sub-routine registers the CX with the Array
sub registerCxWithArray(%)
{
    my(%array_details) = @_;
    my $error_file = "/tmp".$$;
    my $p_data;
    my $command = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    $command .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' RegisterReplicationApplianceV2 ApplianceGuid='.$array_details{SharedApplianceGuid}.' ApplianceName='.$array_details{ApplianceName}.' ApplianceIpAddress='.$array_details{ApplianceIpAddress}.' ApplianceVersion='.$array_details{ApplianceVersion}.' ManagementUrl ='.$array_details{ManagementUrl}.' LogCollectionUrl="'.$array_details{LogCollectionUrl}.'" 2>'.$error_file;
    $logging_obj->log("DEBUG", "registerCxWithArray command:\n$command\n");
    #my $cmd_op = `$command`;
    my ($cmd_op,$cmd_return) = &ExecutePcliCommand($command);
    $logging_obj->log("DEBUG","registerCxWithArray : privatedata command op is $cmd_op\n");
    my $priv_data_cmd_ret = `echo $?`;
    $exit_code = $priv_data_cmd_ret;
    $logging_obj->log("DEBUG","registerCxWithArray : privatedata returned exit code: $exit_code and output is: $cmd_op\n");

    # If the command has not executed succesfully assigning the error to command output
    # If an error is returned
    if($cmd_op =~ /ErrorCode:\s+\w+/ || $cmd_return !=0)
    {
        $array_details{status} = "fail";
        my @errors = split(":",$&);
        $errors[1] =~ s/\s+//g;
        $array_details{errorMessage} = $errors[1];
        $exit_code = $cmd_return;
        $logging_obj->log("EXCEPTION","registerCxWithArray : failed with error : output : $cmd_op");
    }
    # If the command execution is successful
    else
    {
        $array_details{status} = "success";
    }
    unlink($error_file);
    return %array_details;
}

# This sub-routine deregisters the CX with the Array
sub deregisterCxWithArray(%)
{
    my(%array_details) = @_;
    my $error_file = "/tmp".$$;
    my $p_data;

    # Checking if all the required information is available. Else throwing error
    if(!$array_details{SharedApplianceGuid})
    {
        $exit_code = 1;
        $array_details{status} = "fail";
        $array_details{errorMessage} = "INFO NOT AVAILABLE CHECK LOG FOR DETAILS";
        $logging_obj->log("EXCEPTION","deregisterCxWithArray : Values for the keys ApplianceGuid are empty");
        return %array_details;
    }

    ## Delete the SanHost based on the hostname
=notneeded
    my $delete_sanhost_command = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    $delete_sanhost_command .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' DeleteSanHost Identity[1].Fqn="/'.$array_details{Hostname}.'_AI_FOR_TARGET" Identity[1].Fqn="/'.$array_details{Hostname}.'_AI_FOR_SOURCE"'." 2>&1 >/dev/null";
    $logging_obj->log("DEBUG", "deregisterCxWithArray : Delete SanHostCommand : \n$delete_sanhost_command\n");
    my ($delete_sanhost_op,$delete_sanhost_ret) = &ExecutePcliCommand($delete_sanhost_command);
    $exit_code = $delete_sanhost_ret;
    $logging_obj->log("DEBUG","deregisterCxWithArray : Delete SanHost Returned : $delete_sanhost_op : status : $delete_sanhost_ret\n");
=cut
    my $command = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    $command .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' DeregisterReplicationAppliance ApplianceGuid='.$array_details{SharedApplianceGuid}.' 2>'.$error_file;
    $logging_obj->log("DEBUG", "deregisterCxWithArray command:\n$command\n");
    my ($cmd_op,$cmd_return) = &ExecutePcliCommand($command);
    $logging_obj->log("DEBUG","deregisterCxWithArray : privatedata command op is $cmd_op\n");
    $logging_obj->log("DEBUG","deregisterCxWithArray : privatedata returned exit code: $exit_code and output is: $cmd_op\n");
    # If the command has not executed succesfully assigning the error to command output
    # If an error is returned
    if($cmd_op =~ /ErrorCode:\s+\w+/ || $cmd_return !=0)
    {
        $array_details{status} = "fail";
        my @errors = split(":",$&);
        $errors[1] =~ s/\s+//g;

        $exit_code = $cmd_return;
        $logging_obj->log("EXCEPTION","deregisterCxWithArray : failed with error : output : $cmd_op");
    }
    # If the command execution is successful
    else
    {
        $array_details{status} = "success";
    }
    unlink($error_file);
    return %array_details;
}

# This sub-routine creates the sanHostLunMap
# Following functionalities to be server by the sub-routine
# 1. Check if the san host already exists
# 2. create a san host with name given either fc or iscsi
# 3. get available lun numbers for the given id
# 4. create a lun map using the above details

sub createHostLunMap
{
    my($arrayType,%array_details) = @_;

    # Checking if all the required information is available. Else throwing error
    if(!$array_details{sanHostName} or !$array_details{lunId} or !$array_details{state})
    {
        $exit_code = 1;
        $array_details{status} = "fail";
        $array_details{errorMessage} = "INFO NOT AVAILABLE CHECK LOG FOR DETAILS";
        $logging_obj->log("EXCEPTION","createHostLunMap : Values for the keys sanHostName or lunId or state are empty".$array_details{sanHostName}. " ".$array_details{lunId}." " .$array_details{state});
        return %array_details;
    }

    # LunNumber for the map will be populated to this variable
    my $lun_number;
    my $error_file = '/tmp/'.$$;
    my $map_creation_flag = 1;
    my $delete_flag = 0;

    my %host_details = &getSanHostInfo(%array_details);
    if($host_details{errorMessage} && $host_details{errorMessage} =~ /ELEMENT_VERSION_MISMATCH/)
    {
        if(!(&upgradePcli($arrayType,%array_details)))
        {
            $array_details{status} = "fail";
            $array_details{errorMessage} = "UNKNOWN AXIOM";
            my $data_return = PHP::Serialization::serialize(\%array_details);
            $logging_obj->log("DEBUG","arrayManager.pl : Return output is $data_return\n");
            print $data_return ."\n";
            exit 1;
        }
        %host_details = &getSanHostInfo(%array_details);
    }
    my %mod_details;
    if($host_details{$array_details{sanHostName}})
    {
        %mod_details = &modifySanHost(%array_details);
        if($mod_details{status} eq "fail")
        {
            $array_details{errorMessage} = $mod_details{errorMessage};
            unlink($error_file);
            return %array_details;
        }
    }
    else
    {
        my $create_san_host_cmd = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
        $create_san_host_cmd .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' CreateSanHost Name='.$array_details{sanHostName};


        # Get the fcPorts and iscsiPorts in to arrays
        my (@fc_port_details,@iscsi_port_details);
        if($array_details{"fcPorts"}{"ai"})
        {
            @fc_port_details = @{$array_details{"fcPorts"}{"ai"}};
        }
        if($array_details{"iscsiPorts"}{"ai"})
        {
            @iscsi_port_details = @{$array_details{"iscsiPorts"}{"ai"}};
        }

        # Counter for FCPORTS
        my $fc_count = 1;
        # Counter for ISCSIPORTS
        my $iscsi_count = 1;
        foreach my $port_detail (@fc_port_details)
        {
            $port_detail =~ s/://g;
            $create_san_host_cmd .= ' FcInitiatorPortList.FcInitiatorPort['.$fc_count.'].WWN='.$port_detail.' ';
            $fc_count += 1;
        }
        foreach my $port_detail (@iscsi_port_details)
        {
            $create_san_host_cmd .= ' IScsiInitiatorPortList.IScsiInitiatorPort['.$iscsi_count.'].Name='.$port_detail.' ';
            $iscsi_count += 1;
        }
        if($array_details{state} == $source || $array_details{state} == $restart_resync)
        {
            $create_san_host_cmd .= ' RemoteReplicationAppliance=1 ';
        }
        $create_san_host_cmd .= ' 2>'.$error_file;
        $logging_obj->log("DEBUG","createHostLunMap : Create San Host CMD $create_san_host_cmd\n");
        #print "$create_san_host_cmd\n";
        # Command for sanHost creation Done

        # Execution of the command
        my ($create_san_host_cmd_op,$create_san_host_cmd_ret) = &ExecutePcliCommand($create_san_host_cmd);
        $exit_code = $create_san_host_cmd_ret;
        my $sanHostName;
        if($create_san_host_cmd_op !~ /ErrorCode/)
        {
            #$array_details{createSanHostCmdStatus} = "success";
            my $host_id;
            if($create_san_host_cmd_op =~ /Id:\s\w+/)
            {
                my @errors = split(":",$&);
                $errors[1] =~ s/^\s+//;
                $sanHostName = $errors[1];
            }
        }
        # If an error is returned by the command
        else
        {
            $map_creation_flag = 0;
            $array_details{status} = "fail";
            if($create_san_host_cmd_op =~ /ErrorCode:\s+\w+/)
            {
                my @errors = split(":",$&);
                $errors[1] =~ s/\s+//;
                $array_details{errorMessage} = $errors[1];
            }
            $exit_code = 1;
            $logging_obj->log("EXCEPTION","createHostLunMap : failed with error : output : $create_san_host_cmd_op");
            unlink($error_file);
            return %array_details;
        }

         #Update host_details with latest sanhost info from Axiom after creating the sanhost.
         %host_details = &getSanHostInfo(%array_details);
    }
    # Verifying if the lun is already mapped
    my %map_details = &getLunMapInfo(%array_details);
    my @maps;
    if($map_details{$array_details{lunId}}{mapFqn})
    {
        my @maps = @{$map_details{$array_details{lunId}}{mapFqn}};
        $logging_obj->log("DEBUG","createHostLunMap : The Lun ".$array_details{lunId}." is already mapped and maps are @maps\n");
        foreach my $map_fqn (@maps)
        {
            $array_details{mapId} = $map_fqn;
            $logging_obj->log("DEBUG","createHostLunMap : Deleting the map ".$map_fqn."  no_add_flag=$map_details{no_add_flag}\n");
            $array_details{deleteFlag} = 1;
            if($array_details{isGlobalLun} ne "1" && !($map_details{no_add_flag})) #($array_details{state} == $restart_resync))
            {
                # Deleting the maps if they already exist;
                %array_details = &deleteLunMap(%array_details);
                if($array_details{isGlobalLun} eq "1")
                {
                    last;
                }
                if($array_details{status} eq "fail")
                {
                    unlink($error_file);
                    return %array_details;
                }
            }
        }
        if($array_details{state} == $delete_mirror or $array_details{state} == $delete_target)
        {
            #if($array_details{deletionFlag} == 1)
            #{
            #	$array_details{status} = "success";
            #	delete($array_details{deletionFlag});
            #	return %array_details;
            #}
            my @pts;
            if($array_details{fcPorts}{pt})
            {
                push @pts,@{$array_details{fcPorts}{pt}};
            }
            if($array_details{iscsiPorts}{pt})
            {
                push @pts,@{$array_details{iscsiPorts}{pt}};
            }
            my @ais = @{$map_details{$array_details{lunId}}{initiatorId}};
            my $lun_no = $map_details{$array_details{lunId}}{lunNumber};
            my @return_array;
            $return_array[0] = 0;
            $return_array[1] = $array_details{sanLunId};
            $return_array[3] = "/dev/mapper/".$array_details{sanLunId};
            $return_array[4] = 141;  # Always fill 141.
            $return_array[5] = 1;  # Always 1

            my $arr_cnt = 0;
            foreach my $pt(@pts)
            {
                my $iscsi_check = 1;
                my $ok_insert = 0;
                foreach my $ai (@ais)
                {
                    my $ai_edit;
                    if($host_details{$array_details{sanHostName}}{$ai}{wwn})
                    {
                        $ai_edit = $host_details{$array_details{sanHostName}}{$ai}{wwn};
                        if($ai_edit !~ /iqn/g)
                        {
                            $ai_edit =~ tr/[A-Z]/[a-z]/;
                            $ai_edit =~ s/\w{2}/$&:/g;
                            chop($ai_edit);
                            $iscsi_check = 0;
                        }
                    }
                    elsif($host_details{$array_details{sanHostName}}{$ai}{iqn})
                    {
                        $ai_edit = $host_details{$array_details{sanHostName}}{$ai}{iqn};
                    }
                    if($iscsi_check)
                    {
                        if($pt =~ /iqn/)
                        {
                            $ok_insert = 1;
                        }
                        else
                        {
                            $ok_insert = 0;
                        }
                    }
                    else
                    {
                        if($pt !~ /^iqn/)
                        {
                            $ok_insert = 1;
                        }
                        else
                        {
                            $ok_insert = 0;
                        }
                    }
                    if($ok_insert)
                    {
                        $return_array[2][$arr_cnt][0] = 0;
                        $return_array[2][$arr_cnt][1] = $ai_edit;
                        my @arrPt = split("##",$pt);
                        $return_array[2][$arr_cnt][2] = $arrPt[0];
                        $return_array[2][$arr_cnt][3] = $lun_no;
                        $arr_cnt += 1;
                    }
                }
            }
            return @return_array;
        }
    }
    elsif($array_details{state} == $delete_mirror or $array_details{state} == $delete_target)
    {
        my @return_array;
        $return_array[0] = "NoMaps";
        return @return_array;
    }

    if($array_details{errorMessage})
    {
        $exit_code = 1;
        $logging_obj->log("EXCEPTION","createHostLunMap : failed with error : output : ".$array_details{errorMessage});
        unlink($error_file);
        return %array_details;
    }

    #my $source = "53";$restart_resync = "57"; $delete_mirror = "55"; $delete_target_lun_map = "56"; $target = "300";
    my @return_array;
    if($array_details{state} == $source || $array_details{state} == $restart_resync || $array_details{state} == $target)
    {
        # Preparing the array in the format APPAGENT expects (2 reserved for the maps)
        $return_array[0] = 0;
        $return_array[1] = $array_details{sanLunId};
        $return_array[3] = "/dev/mapper/".$array_details{sanLunId};
        $return_array[4] = 9;  # Always fill 9.
        $return_array[5] = 1;  # Always 1

        #Get Avaialble Lun numbers for the SanHost

        # Following command is framed for getting available Lun Number for the sanHost
        # /usr/local/InMage/Vx/bin/pcli-RHEL-4-x86 submit -H 10.0.0.24 -u administrator  -p pillar GetAvailableLunNumbers Identity.Fqn=<name> | grep "ErrorCode|^\s+LunNumber\[0\]"
if (!($map_details{no_add_flag}))
{
        my $get_av_lun_no_cmd = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
        $get_av_lun_no_cmd .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetAvailableLunNumbers Identity.Fqn=/'.$array_details{sanHostName}.' 2>'.$error_file.' | egrep "ErrorCode|^\s+LunNumber\[0\]|^\s+LunNumber\[1\]"';

        $logging_obj->log("DEBUG","createHostLunMap : Get Available Lun Numbers Command: $get_av_lun_no_cmd\n");

        # Executed the command
        my ($get_av_lun_no_op,$get_av_lun_no_ret) = &ExecutePcliCommand($get_av_lun_no_cmd);

        # Getting the shell status of the command
        $exit_code = $get_av_lun_no_ret;
        $logging_obj->log("DEBUG","createHostLunMap : Get Available Lun Numbers OP: $get_av_lun_no_op\n");

        # If the command failed
        if($get_av_lun_no_op =~ /ErrorCode/ || $get_av_lun_no_ret != 0)
        {
            $exit_code = 1;
            if($get_av_lun_no_op =~ /ErrorCode/)
            {
                $array_details{status} = "fail";
                #$array_details{getAvailableLunNumbersStatus} = "fail";
                if($get_av_lun_no_op =~ /ErrorCode:\s/)
                {
                    my @error = split(":",$&);
                    $error[1] =~ s/\s+//g;
                    $array_details{errorMessage} = $error[1];
                }
                $logging_obj->log("EXCEPTION","createHostLunMap : failed with error : output : $get_av_lun_no_op");
            }
            else
            {
                my $cat_cmd = 'cat '.$error_file;
                my $cat_error_op = `$cat_cmd`;
                $array_details{errorMessage} = $cat_error_op;
                $map_creation_flag = 0;
            }
            unlink($error_file);
            return %array_details;
        }
        else
        {
            if($get_av_lun_no_op =~ /LunNumber\[0\]:\s+\d+/)
            {
                my @luns_nums = split(":",$&);
                $luns_nums[1] =~ s/\s+//g;
                $lun_number = $luns_nums[1];
            }
            if($lun_number == 0 || $lun_number eq "0")
            {
                if($get_av_lun_no_op =~ /LunNumber\[1\]:\s+\d+/)
                {
                    my @luns_nums = split(":",$&);
                    $luns_nums[1] =~ s/\s+//g;
                    $lun_number = $luns_nums[1];
                }
                else
                {
                    $logging_obj->log("EXCEPTION","createHostLunMap : Exhausted with the available lun numbers\n");
                    $exit_code = 1;
                    $array_details{status} = "fail";
                    $array_details{errorMessage} = "NO LUN NUMBERS AVAILABLE";
                    return %array_details;
                }
            }
        }
}
        if($array_details{lunId} && $map_creation_flag)
        {
            my $error_message;
            #print "Creating the Map\n";

            #Create the HostToLunMapping

            # Following command is framed for CreateSanHostLunMap
            # /usr/local/InMage/Vx/bin/pcli-RHEL-4-x86 submit -T 30  -H 10.0.0.24 -u administrator -p pillar CreateSanHostLunMap SanHostIdentity.Fqn=<HOST_FQN> LunIdentity.Id=<LUN_ID> LunNumber=<Lun Number>
            my $create_map_cmd = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
            if(($map_details{no_add_flag}) || ($array_details{isGlobalLun} eq "1" && ($array_details{state} == $source)))
            {
                $create_map_cmd .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetSanHostLunMap SanHostIdentity.Fqn=/'.$array_details{sanHostName}.' LunIdentity.Fqn="'.$array_details{lunId}.'" | tee '.$error_file.' |egrep "LunNumber|MapIdentity|Id|Fqn:"';
                $lun_number = $map_details{$array_details{lunId}}{lunNumber};
            }
            else
            {
                $create_map_cmd .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' CreateSanHostLunMap SanHostIdentity.Fqn=/'.$array_details{sanHostName}.' LunIdentity.Fqn="'.$array_details{lunId}.'" LunNumber='.$lun_number.' | tee '.$error_file;
            }

            #$create_map_cmd .= $PCLI_BIN_PATH.'-RHEL-4-x86 submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' CreateSanHostLunMap SanHostIdentity.Fqn=/'.$array_details{protectedProcessServerId}.' LunIdentity.Fqn=/'.$array_details{lunId}.' LunNumber='.$lun_number.' 2>/dev/null | grep "Id:\s+\w+|ErrorCode"';

            $logging_obj->log("DEBUG","createHostLunMap : Creat Map Command $create_map_cmd\n");

            my ($create_map_op,$create_map_return) = &ExecutePcliCommand($create_map_cmd);
            $exit_code = $create_map_return;
            # The command execution was successful
            #if(($create_map_op eq "" || $create_map_op !~ /Fqn/mig) && $create_map_return == 0)
            #{
            my $get_map_cmd = 'cat '.$error_file.' | egrep "LunNumber|MapIdentity|Id:|Fqn:"';
            unless ( open(FH , "<$error_file") )
            {
                $logging_obj->log("EXCEPTION","createHostLunMap : Couldn't read the file $error_file\n");
            }

            local $/ = undef;
            # Reading contents of source file
            $create_map_op = <FH>;
            #$create_map_op = `$get_map_cmd`;
            close(FH);
            #}
            if($lun_number eq "" && $create_map_op =~ /LunNumber/)
            {
                if($create_map_op =~ /LunNumber:\s+\d+/)
                {
                    my @lun_nums = split(":",$&);
                    $lun_nums[1] =~ s/^\s+//;
                    $lun_number = $lun_nums[1];
                }
            }

            $logging_obj->log("DEBUG","createHostLunMap : Creat Map Command returned $create_map_op\n");


            if($create_map_op !~ /ErrorCode/ && $create_map_return == 0)
            {
                #$array_details{createLunMapCmdStatus} = "success";
                $array_details{status} = "success";
                my (@pts);

                # Creating a common array for pts
                if($array_details{fcPorts}{pt})
                {
                    push @pts,@{$array_details{fcPorts}{pt}};
                }
                if($array_details{iscsiPorts}{pt})
                {
                    push @pts,@{$array_details{iscsiPorts}{pt}};
                }

                # Split with Message Response
                my @messages = split(/Message\s+Response/,$create_map_op);

                # A map each for the AI will be created
                # Hence for each map get the id and ai
                my $arr_cnt = 0;
                foreach my $message (@messages)
                {
                    my @lines = split(/\n/,$message);
                    my ($map_id,@ais);
                    foreach my $line (@lines)
                    {
                        #if($line =~ /Id:\s+/)
                        #{
                        #    my @maps_det = split(":",$line);
                        #    $maps_det[1] =~ s/\s+//g;
                        #    $map_id = $maps_det[1];
                        #}
                        if($line =~ /\s+Fqn:\s+\/(.*)\/(.*)/)
                        {
                            my @maps_det = split(":",$line);
                            $maps_det[1] =~ s/\s+//g;
                            $map_id = $maps_det[1];
                            my @maps_dets = split(/\//,$line);
                            push(@ais,$maps_dets[$#maps_dets]);
                        }
                    }
                    if($map_id && ($lun_number ne ""))
                    {
                        foreach my $ai (@ais)
                        {
                            my $iscsi_check = 1;
                            my $ok_insert = 0;
                            if($ai !~ /iqn/)
                            {
                                $ai =~ tr/[A-Z]/[a-z]/;
                                $ai =~ s/\w{2}/$&:/g;
                                chop($ai);
                                $iscsi_check = 0;
                            }
                            $array_details{maps}{$map_id}{ai} = $ai;
                            @{$array_details{maps}{$map_id}{pt}} = @pts;

                            #Added by patnana, In the context that to convert int lun number to string
                            #$lun_number = $lun_number.'0';

                            $array_details{maps}{$map_id}{lunNumber} = $lun_number;
                            foreach my $pt (@pts)
                            {
                                if($iscsi_check)
                                {
                                    if($pt =~ /iqn/)
                                    {
                                        $ok_insert = 1;
                                    }
                                    else
                                    {
                                        $ok_insert = 0;
                                    }
                                }
                                else
                                {
                                    if($pt !~ /^iqn/)
                                    {
                                        $ok_insert = 1;
                                    }
                                    else
                                    {
                                        $ok_insert = 0;
                                    }
                                }
                                if($pt !~ /N\/A/ && $ok_insert)
                                {
                                    $return_array[2][$arr_cnt][0] = 0;
                                    $return_array[2][$arr_cnt][1] = $ai;
                                    my @arrPt = split("##",$pt);
                                    $return_array[2][$arr_cnt][2] = $arrPt[0];
                                    $return_array[2][$arr_cnt][3] = $lun_number;
                                    $arr_cnt += 1;
                                }
                            }
                        }
                    }
                }

                # Deleting the following keys from the input as they are not required to be part of output
                delete($array_details{fcPorts});
                delete($array_details{iscsiPorts});
                delete($array_details{lunId});
            }
            # Creation of lun Map failed
            else
            {
                $exit_code = 1;
                $array_details{status} = "fail";
                if($create_map_op =~ /ErrorCode:\s+\w+/)
                {
                    my @error = split(":",$&);
                    $error[1] =~ s/\s+//g;
                    $error_message = $error[1];
                }
                $array_details{errorMessage} = $error_message;
                $logging_obj->log("EXCEPTION","createHostLunMap : failed with error : output : $create_map_op");
                unlink($error_file);
                return %array_details;
            }
        }
        else
        {
            $exit_code = 1;
        }
    }
    # Deleting the error file created from the command using the pid
    unlink($error_file);
    #print Dumper(@return_array);
    #return %array_details;
    return @return_array;
}
sub lunMirrorInfo(%)
{
    my(%array_details) = @_;
    my $error_file = "/tmp/".$$;
    my $atlun_base = 5002383000000000;

    if(!$array_details{atLunNumber} or !$array_details{lunId} or !$array_details{state} or !$array_details{atLunId} or !$array_details{privateData} )
    {
        $exit_code = 1;
        $array_details{status} = "fail";
        $array_details{errorMessage} = "INFO NOT AVAILABLE CHECK LOG FOR DETAILS";
        $logging_obj->log("EXCEPTION","createHostLunMap : Values for the keys lunId or atLunId or atLunNumber or privateData or state are empty");
        return %array_details;
    }

    my @scsids = split("",$array_details{sanLunId});
    shift(@scsids);
    my $scsiId = join("",@scsids);
    # Following command is framed for Mirror Creation
    # ./pcli submit -H 10.80.240.246 -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' SetWriteSplit SourceLunIdentity.Fqn=/InmLun9 ApplianceGuid='aa2a79f1-43ad-4bdf-b55e-1b80e33cd4b3' MirrorLuid=000b080003002750 MirrorLunNumber=98 IoTimeoutValue=50 FcPort[1]=2101001B32AF3023 PrivateData="aa2a79f1-43ad-4bdf-b55e-1b80e33cd4b3"

    #my $source = "53";$restart_resync = "57"; $delete_mirror = "55"; $delete_target_lun_map = "56"; $target = "300";

    if($array_details{state} == $source || $array_details{state} == $restart_resync || $array_details{state} == $target)
    {
        $array_details{atLunId} =~ s/[a-z]/0/g;
        $array_details{atLunId} = $atlun_base + $array_details{atLunId};
        my $mirror_create_cmd = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
        $mirror_create_cmd .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' SetWriteSplit SourceLunIdentity.Fqn="'.$array_details{lunId}.'" ApplianceGuid='.$array_details{applianceGuid}.' MirrorLuid='.$array_details{atLunId}.' MirrorLunNumber='.$array_details{atLunNumber}.' IoTimeoutValue=30';
        my (@fc_port_details,@iscsi_port_details);
        if($array_details{"fcPorts"}{"at"})
        {
            @fc_port_details = @{$array_details{"fcPorts"}{"at"}};
        }
        if($array_details{"iscsiPorts"}{"at"})
        {
            @iscsi_port_details = @{$array_details{"iscsiPorts"}{"at"}};
        }

        # Counter for FCPORTS
        my $fc_count = 1;
        foreach my $port_detail (@fc_port_details)
        {
            $port_detail =~ s/://g;
            $mirror_create_cmd .= ' FcPort['.$fc_count.']='.$port_detail.' ';
            $fc_count += 1;
        }
        my $iscsi_count = 1;
        foreach my $port_detail (@iscsi_port_details)
        {
            my @iscsi_detail = split("##",$port_detail);
            $logging_obj->log("DEBUG","lunMirrorInfo : @iscsi_detail");
            $mirror_create_cmd .= ' IscsiPort['.$iscsi_count.'].IpAddress='.$iscsi_detail[1].' IscsiPort['.$iscsi_count.'].TcpPort='.$iscsi_detail[2].' IscsiTargetName='.$iscsi_detail[0].' ';
            $iscsi_count += 1;
        }
        $mirror_create_cmd .= ' PrivateData="'.$array_details{privateData}.'"';

        $logging_obj->log("DEBUG","lunMirrorInfo : Mirror create command is $mirror_create_cmd\n");
        my $mirror_create_return;
        my ($mirror_create_op, $mirror_create_return) = &ExecutePcliCommand($mirror_create_cmd);
        $logging_obj->log("DEBUG","lunMirrorInfo : Mirror create command OP is $mirror_create_op\n");
        $exit_code = $mirror_create_return;

        if($mirror_create_op=~ /ErrorCode:\s+\w+/ || $mirror_create_return != 0)
        {
            $array_details{status} = "fail";
            my @errors = split(":",$&);
            $errors[1] =~ s/\s+//g;
            $array_details{errorMessage} = $errors[1];
            $logging_obj->log("EXCEPTION","lunMirrorInfo : failed with error : output : $mirror_create_op");
        }
        else
        {
            $array_details{status} = "success";
            #$array_details{status} = "fail";
        }
    }
    #my $source = "53";$restart_resync = "57"; $delete_mirror = "55"; $delete_target_lun_map = "56"; $target = "300";
    $logging_obj->log("DEBUG","lunMirrorInfo : CreateMirror state = ".$array_details{state}." checking with  = $delete_mirror\n");

    if($array_details{state} == $delete_target_lun_map || $array_details{state} == $delete_mirror)
    {
        %array_details = &deleteMirror(%array_details);
    }
    &setMemory($array_details{state});
    unlink($error_file);
    return %array_details;
}
sub deleteLunMap(%)
{
    my(%array_details) = @_;
    my $error_file = "/tmp/".$$;

    # Checking if all the required information is available. Else throwing error
    if(!$array_details{lunId} and !$array_details{mapId})
    {
        $exit_code = 1;
        $array_details{status} = "fail";
        $array_details{errorMessage} = "INFO NOT AVAILABLE CHECK LOG FOR DETAILS";
        $logging_obj->log("EXCEPTION","deleteLunMap : Values for the keys mapId or lunId are empty");
        return %array_details;
    }

    if(!$array_details{deleteFlag})
    {
        $array_details{deletionFlag} = 1;
        $array_details{state} = $delete_mirror;
        my @array_return = &createHostLunMap(%array_details);
        return @array_return;
    }
    # Following command is framed for DeletSanHostLunMap
    #./pcli submit -H 10.80.240.246 -u administrator -p pillar DeleteSanHostLunMap Identity.Fqn=/AxiomScout/InmLun4/2101001B32AF3023
    my $delete_map_cmd = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    if($array_details{isGlobalLun} eq "1")
    {
        $delete_map_cmd .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' DeleteGlobalLunMap LunIdentity.Fqn="'.$array_details{lunId}.'" 2>'.$error_file;
    }
    else
    {
        $delete_map_cmd .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' DeleteSanHostLunMap Identity.Fqn="'.$array_details{mapId}.'" 2>'.$error_file;
    }
    $logging_obj->log("DEBUG","deleteLunMap : DeleteSanHostLunMap  command is $delete_map_cmd\n");
    my ($delete_map_op,$delete_map_return) = &ExecutePcliCommand($delete_map_cmd);
    $logging_obj->log("DEBUG","deleteLunMap : DeleteSanHostLunMap  command returned  $delete_map_op\n");

    $exit_code = $delete_map_return;

    if($delete_map_op =~ /ErrorCode:\s+\w+/ || $delete_map_return != 0)
    {
        $array_details{status} = "fail";
        if($delete_map_op =~ /ErrorCode:\s+\w+/)
        {
            my @errors = split(":",$&);
            $errors[1] =~ s/^\s+//;
            $array_details{errorMessage} = "Delete Lun Map Failed ".$errors[1];
            if($delete_map_op =~ /ErrorCode:\s+CANNOT_DELETE_HOST_MAPPED_LUN_BY_LUN_ID/)
            {
                $array_details{status} = "success";
                $array_details{isGlobalLun} = 0;
                %array_details = &deleteLunMap(%array_details);
                return %array_details;
            }
        }
        else
        {
            my $cat_cmd = 'cat '.$error_file;
            $array_details{errorMessage} = `$cat_cmd`;
        }
        $logging_obj->log("EXCEPTION","deleteLunMap : failed with error : output : $delete_map_op");
    }
    else
    {
        $array_details{status} = "success";
    }
    unlink($error_file);
    return %array_details;
}

sub modifyRefTag(%)
{
    my(%array_details) = @_;
    my $error_file = "/tmp/".$$;
   
 
   if ($array_details{lunType} ne "TARGET" )
   {
    	$logging_obj->log("DEBUG","modifyRefTag : RefTag will not be modified for Lun type ".$array_details{lunType}."\n");
	return;
   }
    my $modify_reftag_cmd = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    $modify_reftag_cmd .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' ModifyLun Identity.Fqn='.$array_details{lunId};

    if($array_details{state} == $delete_target )
    {
	$modify_reftag_cmd .=  ' DisableRefTagChecking=0';
    }
    else
    {
	$modify_reftag_cmd .=  ' DisableRefTagChecking=1';
    }
    $modify_reftag_cmd .= ' 2>'.$error_file;
    #print "$modify_san_host_cmd\n";
    $logging_obj->log("DEBUG","modifyRefTag : Modify RefTag CMD $modify_reftag_cmd\n");
    # Command for sanHost creation Done

    # Execution of the command
    my ($modify_reftag_cmd_op,$modify_reftag_cmd_ret) = &ExecutePcliCommand($modify_reftag_cmd);
    $logging_obj->log("DEBUG","modifySanHost : Modify San Host CMD returned $modify_reftag_cmd_op\n");
    $exit_code = $modify_reftag_cmd_ret;
    if($modify_reftag_cmd_op !~ /ErrorCode/)
    {
        $exit_code = 0;
        $array_details{status} = "success";
        $logging_obj->log("DEBUG","modifyRefTag : Modify RefTag CMD succeeded\n");
        my $host_id;
        if($modify_reftag_cmd_op =~ /Id:\s\w+/)
        {
            my @errors = split(":",$&);
            $errors[1] =~ s/^\s+//;
        }
    }
    # If an error is returned by the command
    else
    {
        $array_details{status} = "fail";
        if($modify_reftag_cmd_op =~ /ErrorCode:\s+\w+/)
        {
            my @errors = split(":",$&);
            $errors[1] =~ s/\s+//;
            $array_details{errorMessage} = $errors[1];
        }
        $logging_obj->log("EXCEPTION","modifyRefTag : failed with error : output : $modify_reftag_cmd_op");
    }
    unlink($error_file);
    #$logging_obj->log("DEBUG","modifySanHost : Modify San Host CMD returning".Dumper(%array_details)."\n");
}
sub modifySanHost(%)
{
    my(%array_details) = @_;
    my $error_file = "/tmp/".$$;

    my $modify_san_host_cmd = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    $modify_san_host_cmd .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' ModifySanHost Identity.Fqn=/'.$array_details{sanHostName}.' ReconcileMappings=1';
    # Get the fcPorts and iscsiPorts in to arrays
    my (@fc_port_details,@iscsi_port_details);
    if($array_details{"fcPorts"}{"ai"})
    {
        @fc_port_details = @{$array_details{"fcPorts"}{"ai"}};
    }
    if($array_details{"iscsiPorts"}{"ai"})
    {
        @iscsi_port_details = @{$array_details{"iscsiPorts"}{"ai"}};
    }

    # Counter for FCPORTS
    my $fc_count = 1;
    # Counter for ISCSIPORTS
    my $iscsi_count = 1;
    foreach my $port_detail (@fc_port_details)
    {
        $port_detail =~ s/://g;
        $modify_san_host_cmd .= ' FcInitiatorPortList.FcInitiatorPort['.$fc_count.'].WWN='.$port_detail.' ';
        $fc_count += 1;
    }
    foreach my $port_detail (@iscsi_port_details)
    {
        $modify_san_host_cmd .= ' IScsiInitiatorPortList.IScsiInitiatorPort['.$iscsi_count.'].Name='.$port_detail.' ';
        $iscsi_count += 1;
    }
    if($array_details{state} == $source || $array_details{state} == $restart_resync)
    {
        $modify_san_host_cmd .= ' RemoteReplicationAppliance=1 ';
    }
    $modify_san_host_cmd .= ' 2>'.$error_file;
    #print "$modify_san_host_cmd\n";
    $logging_obj->log("DEBUG","modifySanHost : Modify San Host CMD $modify_san_host_cmd\n");
    # Command for sanHost creation Done

    # Execution of the command
    my ($modify_san_host_cmd_op,$modify_san_host_cmd_ret) = &ExecutePcliCommand($modify_san_host_cmd);
    $logging_obj->log("DEBUG","modifySanHost : Modify San Host CMD returned $modify_san_host_cmd_op\n");
    $exit_code = $modify_san_host_cmd_ret;
    my $sanHostName;
    if($modify_san_host_cmd_op !~ /ErrorCode/)
    {
        $exit_code = 1;
        $array_details{status} = "success";
        $logging_obj->log("DEBUG","modifySanHost : Modify San Host CMD succeeded\n");
        my $host_id;
        if($modify_san_host_cmd_op =~ /Id:\s\w+/)
        {
            my @errors = split(":",$&);
            $errors[1] =~ s/^\s+//;
            $sanHostName = $errors[1];
        }
    }
    # If an error is returned by the command
    else
    {
        $array_details{status} = "fail";
        if($modify_san_host_cmd_op =~ /ErrorCode:\s+\w+/)
        {
            my @errors = split(":",$&);
            $errors[1] =~ s/\s+//;
            $array_details{errorMessage} = $errors[1];
        }
        $logging_obj->log("EXCEPTION","modifySanHost : failed with error : output : $modify_san_host_cmd_op");
    }
    unlink($error_file);
    #$logging_obj->log("DEBUG","modifySanHost : Modify San Host CMD returning".Dumper(%array_details)."\n");
    return %array_details;
}
sub getLunMapInfo(%)
{
    my(%array_details) = @_;
    my $error_file = "/tmp/".$$;

    # Getting Map information

    # Framing the command
    # /usr/local/InMage/Vx/bin/pcli-RHEL-4-x86 submit -H 10.0.0.24 -u administrator -p pillar GetSanHostLunMap LunIdentity[1..n].Id=<value> | grep "Id|LunNumber|ErrorCode
    my $map_command = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    ##$map_command .= '/usr/local/InMage/Vx/bin/pcli submit -H '.$array_details{ip}.' -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetSanHostLunMap SanHostIdentity.Id='.$array_details{hostId}.' LunIdentity.Fqn='.$array_details{lunId}.' | grep "LunIdentity|MapIdentity|Id|Fqn|LunNumber|ErrorCode" 2>'.$error_file;
    if($array_details{lunId})
    {
        $map_command .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetSanHostLunMap LunIdentity.Fqn="'.$array_details{lunId}.'"';
    }
    else
    {
        $map_command .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetSanHostLunMap';
    }
#   print "State is ".$array_details{state}."\n";
    #UnMap the lun from appliance only, if the states are source, or restart resync or mirror delete.
    if($array_details{state} == $source or $array_details{state} == $restart_resync or $array_details{state} == $delete_mirror or $array_details{state} == $delete_target or $array_details{lunType} eq "TARGET")
    {
        $map_command .= ' SanHostIdentity.Fqn=/'.$array_details{sanHostName};
    }
    $map_command .= ' | egrep "LunIdentity|MapIdentity|Id|Fqn|LunNumber|ErrorCode" 2>'.$error_file;
#    print "Get map :::: $map_command\n";
    $logging_obj->log("DEBUG","getLunMapInfo : GetSanHostLunMap  command is $map_command\n");
    my ($map_cmd_op,$map_cmd_return) = &ExecutePcliCommand($map_command);
    $logging_obj->log("DEBUG","getLunMapInfo : GetSanHostLunMap  command returned $map_cmd_op");

    $exit_code = $map_cmd_return;
    # If an error is returned
    if($map_cmd_op =~ /ErrorCode/ || $map_cmd_return !=0)
    {
        $exit_code = 1;
        if($map_cmd_op =~ /ErrorCode/)
        {
            my @error = split(":",$map_cmd_op);
            $error[1] =~ s/\s+//g;
            $array_details{status} = "fail";
            $array_details{errorMessage} = $error[1];
        }
        else
        {
            my $cat_cmd = 'cat '.$error_file;
            my $cat_error_op = `$cat_cmd`;
            $array_details{errorMessage} = $cat_error_op;
        }
        $logging_obj->log("EXCEPTION","getLunMapInfo : failed with error : output : $map_cmd_op");
        #my $serialized_array = PHP::Serialization::serialize(\%array_details);
        #return $serialized_array;
        $exit_code = $map_cmd_return;
        unlink($error_file);
        return %array_details;
    }
    # If the command execution is successful
    else
    {
        $array_details{status} = "success";
        my @map_details;
        @map_details = split("\n",$map_cmd_op);
        my $map_length = $#map_details + 1;
        my($lun_id,$lun_number);
        my (@map_ids,@map_fqns,@host_ids);
        my $no_add_flag = 0;
        for(my $i=0;$i<$map_length;$i++)
        {
           # if(($array_details{state} ne $source) and ($array_details{state} ne $restart_resync) and ($array_details{state} ne $delete_mirror) and ($array_details{state} ne $delete_target))
            if((($array_details{state} ne $delete_mirror) and ($array_details{state} ne $delete_target)) or (($array_details{lunType} eq "TARGET") && ($array_details{state} ne $delete_target) ))
            {
                if($map_details[$i] =~ /SanHostIdentity/)
                {
                    my @hosts_id = split(":",$map_details[$i+2]);
                    $hosts_id[1] =~ s/^\s+//;
                    $hosts_id[1] =~ s/^\///;
                    if($hosts_id[1] eq $array_details{sanHostName})
                    {
			$logging_obj->log("DEBUG","getLunMapInfo no_add_flag set to 1. San Host found ". $array_details{sanHostName});
                        $no_add_flag = 1;
                        $array_details{no_add_flag} = 1;
                    }
                    else
                    {
                        $no_add_flag = 0;
                    }
                }
            }
            if($no_add_flag == 0)
            {
                if($map_details[$i] =~ /LunNumber/)
                {
                    my @lun_nums = split(":",$map_details[$i]);
                    $lun_nums[1] =~ s/^\s+//;
                    $lun_number = $lun_nums[1];
                }
                elsif($map_details[$i] =~ /InitiatorIdentity/)
                {
                    my @hosts_id = split(":",$map_details[$i+1]);
                    $hosts_id[1] =~ s/^\s+//;
                    if(!(grep $_ eq $hosts_id[1], @host_ids))
                    {
                        push(@host_ids, $hosts_id[1]);
                    }
                }
                elsif($map_details[$i] =~ /MapIdentity/)
                {
                    my @hosts_id = split(":",$map_details[$i+1]);
                    $hosts_id[1] =~ s/^\s+//;
                    push(@map_ids, $hosts_id[1]);
                    @hosts_id = split(": ",$map_details[$i+2]);
                    $hosts_id[1] =~ s/^\s+//;
                    push(@map_fqns , $hosts_id[1]);
                }
                elsif($map_details[$i] =~ /LunIdentity/)
                {
                    my @lun_values = split(":",$map_details[$i+2]);
                    $lun_values[1] =~ s/^\s+//;
                    $lun_id = $lun_values[1];
                }
                if($lun_id && ($lun_number ne "") && @host_ids && @map_fqns && @map_ids)
                {
                    @{$array_details{$lun_id}{initiatorId}} = @host_ids;
                    $array_details{$lun_id}{lunNumber}   = $lun_number;
                    @{$array_details{$lun_id}{mapId}}   = @map_ids;
                    @{$array_details{$lun_id}{mapFqn}}   = @map_fqns;
                }
                if($array_details{lunId} && (!$array_details{$lun_id}{mapFqn}))
                {
                    $array_details{errorMessage} = "NOMAPS";
                }
            }
        }
    }
    # The file created for capturing standard error is unlinked here
    unlink($error_file);
    #my $serialized_array = PHP::Serialization::serialize(\%array_details);
    #return $serialized_array;
    #$logging_obj->log("DEBUG","getLunMapInfo returned ".Dumper(%array_details));
    return %array_details;
}
sub getMapInfo(%)
{
    my(%array_details) = @_;
    my $error_file = "/tmp/".$$;

    # Getting Map information

    # Framing the command
    # /usr/local/InMage/Vx/bin/pcli-RHEL-4-x86 submit -H 10.0.0.24 -u administrator -p pillar GetSanHostLunMap LunIdentity[1..n].Id=<value> | grep "Id|LunNumber|ErrorCode
    my $map_command = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    ##$map_command .= '/usr/local/InMage/Vx/bin/pcli submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetSanHostLunMap SanHostIdentity.Id='.$array_details{hostId}.' LunIdentity.Fqn='.$array_details{lunId}.' | grep "LunIdentity|MapIdentity|Id|Fqn|LunNumber|ErrorCode" 2>'.$error_file;
    $map_command .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetSanHostLunMap';
#   print "State is ".$array_details{state}."\n";
    $map_command .= ' | egrep "LunIdentity|MapIdentity|Id|Fqn|LunNumber|ErrorCode" 2>'.$error_file;
#    print "Get map :::: $map_command\n";
    $logging_obj->log("DEBUG","getMapInfo : GetSanHostLunMap  command is $map_command\n");
    my $map_cmd_return;
    my ($map_cmd_op,$map_cmd_return) = &ExecutePcliCommand($map_command);
    #$logging_obj->log("DEBUG","getMapInfo : GetSanHostLunMap returned $map_cmd_op\n");
    $exit_code = $map_cmd_return;
    # If an error is returned
    if($map_cmd_op =~ /ErrorCode/ || $map_cmd_return !=0)
    {
        $exit_code = 1;
        if($map_cmd_op =~ /ErrorCode/)
        {
            my @error = split(":",$map_cmd_op);
            $error[1] =~ s/\s+//g;
            $array_details{status} = "fail";
            $array_details{errorMessage} = $error[1];
        }
        else
        {
            my $cat_cmd = 'cat '.$error_file;
            my $cat_error_op = `$cat_cmd`;
            $array_details{errorMessage} = $cat_error_op;
        }
        $logging_obj->log("EXCEPTION","getMapInfo : failed with error : output : $map_cmd_op");
        #my $serialized_array = PHP::Serialization::serialize(\%array_details);
        #return $serialized_array;
        unlink($error_file);
        return %array_details;
    }
    # If the command execution is successful
    else
    {
        $array_details{status} = "success";
        my @map_details;
        #print "$map_cmd_op\n";
        @map_details = split("\n",$map_cmd_op);
        my $map_length = $#map_details + 1;
        my($host_id,$lun_number,$host);
        my (@map_ids,@map_fqns,$lun_id);
        for(my $i=0;$i<$map_length;$i++)
        {
            if($map_details[$i] =~ /SanHostIdentity/)
            {
                $host_id = $lun_number = $host = $lun_id = "";
                my @lun_values = split(":",$map_details[$i+2]);
                $lun_values[1] =~ s/^\s+//;
                $lun_values[1] =~ s/^\///;
                $host = $lun_values[1];
            }
            if($map_details[$i] =~ /LunNumber/)
            {
                $lun_number = "";
                my @lun_nums = split(":",$map_details[$i]);
                $lun_nums[1] =~ s/^\s+//;
                $lun_number = $lun_nums[1];
            }
            elsif($map_details[$i] =~ /InitiatorIdentity/)
            {
                $host_id = "";
                my @hosts_id = split(":",$map_details[$i+1]);
                $hosts_id[1] =~ s/^\s+//;
                $host_id = $hosts_id[1];
            }
            elsif($map_details[$i] =~ /LunIdentity/)
            {
                $lun_id = "";
                my @lun_values = split(":",$map_details[$i+2]);
                $lun_values[1] =~ s/^\s+//;
                $lun_id = $lun_values[1];
            }
            if($host_id && ($lun_number ne "") && $lun_id && $host)
            {
                $array_details{$host}{$host_id}{$lun_id} = $lun_number;
                $host_id = $lun_number =  "";
            }
        }
    }
    # The file created for capturing standard error is unlinked here
    unlink($error_file);
    #my $serialized_array = PHP::Serialization::serialize(\%array_details);
    #return $serialized_array;
    #$logging_obj->log("DEBUG","MapDetails are".Dumper(%array_details)."\n");
    return %array_details;
}
sub forceDeleteMirror(%)
{
    my %array_details = @_;
    my $error_file = "/tmp/".$$;

    # Checking if all the required information is available. Else throwing error
    if(!$array_details{applianceGuid} or !$array_details{lunId})
    {
        $exit_code = 1;
        $array_details{status} = "fail";
        $array_details{errorMessage} = "INFO NOT AVAILABLE CHECK LOG FOR DETAILS";
        $logging_obj->log("EXCEPTION","forceDeleteMirror : Values for the keys hostGuid or lunId are empty");
        return %array_details;
    }

    my @scsids = split("",$array_details{sanLunId});
    shift(@scsids);
    my $scsiId = join("",@scsids);
    # Following command is framed for Mirror Deletion
    # ./pcli submit -H 10.80.240.246 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' ClearWriteSplit SourceLunIdentity.Fqn=/InmLun9 ApplianceGuid='aa2a79f1-43ad-4bdf-b55e-1b80e33cd4b3'

    my $mirror_del_cmd = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    $mirror_del_cmd .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' ClearWriteSplit SourceLunIdentity.Fqn="'.$array_details{lunId}.'" ApplianceGuid='.$array_details{applianceGuid}.' ApplianceGuidOverride=1';

    $logging_obj->log("DEBUG","forceDeleteMirror : Mirror force delete command is $mirror_del_cmd\n");
    my ($mirror_del_op,$mirror_del_return) = &ExecutePcliCommand($mirror_del_cmd);
    $logging_obj->log("DEBUG","forceDeleteMirror : Mirror force delete command OP : $mirror_del_op\n");
    $exit_code = $mirror_del_return;

    if($mirror_del_op=~ /ErrorCode:\s+\w+/ || $mirror_del_return != 0)
    {
        $array_details{status} = "fail";
        my @errors = split(":",$&);
        $errors[1] =~ s/\s+//g;
        $array_details{errorMessage} = $errors[1];
        $logging_obj->log("EXCEPTION","forceDeleteMirror : failed with error : output : $mirror_del_op");
    }
    else
    {
        $array_details{status} = "success";
        #$array_details{status} = "fail";
    }
    return %array_details;
}
sub deleteMirror(%)
{
    my %array_details = @_;
    my $error_file = "/tmp/".$$;

    # Checking if all the required information is available. Else throwing error
    if(!$array_details{privateData} or !$array_details{lunId})
    {
        $exit_code = 1;
        $array_details{status} = "fail";
        $array_details{errorMessage} = "INFO NOT AVAILABLE CHECK LOG FOR DETAILS";
        $logging_obj->log("EXCEPTION","deleteMirror : Values for the keys privateData or lunId or state are empty");
        return %array_details;
    }

    my @scsids = split("",$array_details{sanLunId});
    shift(@scsids);
    my $scsiId = join("",@scsids);
    # Following command is framed for Mirror Deletion
    # ./pcli submit -H 10.80.240.246 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' ClearWriteSplit SourceLunIdentity.Fqn=/InmLun9 ApplianceGuid='aa2a79f1-43ad-4bdf-b55e-1b80e33cd4b3'

    my $mirror_del_cmd = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    $mirror_del_cmd .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' ClearWriteSplit SourceLunIdentity.Fqn="'.$array_details{lunId}.'" ApplianceGuid='.$array_details{applianceGuid};

    $logging_obj->log("DEBUG","deleteMirror : Mirror delete command is $mirror_del_cmd\n");
    my ($mirror_del_op, $mirror_del_return) = &ExecutePcliCommand($mirror_del_cmd);
    $logging_obj->log("DEBUG","deleteMirror : Mirror delete command OP : $mirror_del_op\n");
    $exit_code = $mirror_del_return;

    if($mirror_del_op=~ /ErrorCode:\s+\w+/ || $mirror_del_return != 0)
    {
        $array_details{status} = "fail";
        my @errors = split(":",$&);
        $errors[1] =~ s/\s+//g;
        $array_details{errorMessage} = $errors[1];
        $logging_obj->log("EXCEPTION","deleteMirror : failed with error : output : $mirror_del_op");
    }
    else
    {
        $array_details{status} = "success";
        #$array_details{status} = "fail";
    }
    return %array_details;
}
sub getMirrorLunStatus(%)
{
    my(%array_details) = @_;
    my $error_file = "/tmp/".$$;

    # Checking if all the required information is available. Else throwing error
    if(!$array_details{lunId})
    {
        $exit_code = 1;
        $array_details{status} = "fail";
        $array_details{errorMessage} = "INFO NOT AVAILABLE CHECK LOG FOR DETAILS";
        $logging_obj->log("EXCEPTION","getMirrorLunStatus : Values for the keys lunId is empty");
        return %array_details;
    }

    my @scsids = split("",$array_details{sanLunId});
    shift(@scsids);
    my $scsiId = join("",@scsids);
    # Following command is framed for Mirror Creation
    # ./pcli submit -H 10.80.240.246 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetMirrorLunStatus Identity.Fqn=/InmLun9 | grep "MirrorStatus|MirrorFailedReason|MirrorSetupTime|IoTimeoutValue|PathsConfigured|PathsActive"
    my $mirror_create_cmd = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    $mirror_create_cmd .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetMirrorLunStatus Identity.Fqn="'.$array_details{lunId}.'" 2>'.$error_file;
    #$mirror_create_cmd .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetMirrorLunStatus Identity.Fqn='.$array_details{lunId}.' | grep "MirrorStatus|MirrorFailedReason|MirrorSetupTime|IoTimeoutValue|PathsConfigured|PathsActive|ErrorCode"';

    $logging_obj->log("DEBUG","getMirrorLunStatus : GetMirrorLunStatus command is $mirror_create_cmd\n");
    my ($mirror_create_op,$mirror_create_return) = &ExecutePcliCommand($mirror_create_cmd);
    #$logging_obj->log("DEBUG","getMirrorLunStatus : GetMirrorLunStatus command returned $mirror_create_op\n");
    $exit_code = $mirror_create_return;

    if($mirror_create_op=~ /NO_WRITE_SPLIT_CONFIGURED/g)
    {
        $array_details{$array_details{sanLunId}}{mirroStatus} = "FAILED";
        $array_details{$array_details{sanLunId}}{mirrorFailedReason} = "NO_WRITE_SPLIT_CONFIGURED";
        $array_details{$array_details{sanLunId}}{mirrorSetupTime} = "";
        $array_details{$array_details{sanLunId}}{ioTimeoutValue} = "";
        $array_details{$array_details{sanLunId}}{pathsConfigured} = "";
        $array_details{$array_details{sanLunId}}{pathsActive} = "";
        $exit_code = 0;
        $logging_obj->log("EXCEPTION","getMirrorLunStatus : GetMirrorLunStatus command is $mirror_create_cmd\nFailed with Output $mirror_create_op");
    }
    elsif($mirror_create_op=~ /ErrorCode:\s+\w+/ || $mirror_create_return != 0)
    {
        $exit_code = 1;
        if($mirror_create_op)
        {
            $array_details{status} = "fail";
            my @errors = split(":",$&);
            $errors[1] =~ s/\s+//g;
            $array_details{errorMessage} = $errors[1];
        }
        else
        {
            my $cat_cmd = 'cat '.$error_file;
            my $cat_error_op = `$cat_cmd`;
            chomp ($cat_error_op);
            $array_details{errorMessage} = $cat_error_op;
        }
        $logging_obj->log("EXCEPTION","getMirrorLunStatus : failed with error : output : $mirror_create_op - $array_details{errorMessage}");
    }
    else
    {
        if($mirror_create_op =~ /MirrorStatus:\s+\w+/)
        {
            my @data = split(":",$&);
            $data[1] =~ s/^\s+//;
            $array_details{$array_details{sanLunId}}{mirroStatus} = $data[1];
        }
        if($mirror_create_op =~ /MirrorFailedReason:\s+\w+/)
        {
            my @data = split(":",$&);
            $data[1] =~ s/^\s+//;
            $array_details{$array_details{sanLunId}}{mirrorFailedReason} = $data[1];
        }
        if($mirror_create_op =~ /MirrorSetupTime:\s+(.*):(.*):(.*)/)
        {
            my @data = split(":",$&);
            $data[1] =~ s/^\s+//;
            $array_details{$array_details{sanLunId}}{mirrorSetupTime} = $data[1].":".$data[2].":".$data[3];
        }
        if($mirror_create_op =~ /IoTimeoutValue:\s+\w+/)
        {
            my @data = split(":",$&);
            $data[1] =~ s/^\s+//;
            $array_details{$array_details{sanLunId}}{ioTimeoutValue} = $data[1];
        }
        if($mirror_create_op =~ /PathsConfigured:\s+\w+/)
        {
            my @data = split(":",$&);
            $data[1] =~ s/^\s+//;
            $array_details{$array_details{sanLunId}}{pathsConfigured} = $data[1];
        }
        if($mirror_create_op =~ /PathsActive:\s+\w+/)
        {
            my @data = split(":",$&);
            $data[1] =~ s/^\s+//;
            $array_details{$array_details{sanLunId}}{pathsActive} = $data[1];
        }
    }
    unlink($error_file);
    return %array_details;
}

sub SCSI3_Reserve(%)
{
    my(%array_details) = @_;
    #multipath -l 2000b08005a002749|grep "\\_"|grep sd | sed 's/\s\+/ /g'| cut -d " " -f 4
    # Checking if all the required information is available. Else throwing error
    if(!$array_details{sanLunId})
    {
        $exit_code = 1;
        $array_details{status} = "fail";
        $array_details{errorMessage} = "sanLunId is missing in the policy info";
        $logging_obj->log("EXCEPTION","SCSI3_Reserve : sanLunId is missing in the info");
        return %array_details;
    }
    if(!$array_details{pgrKey})
    {
        $exit_code = 1;
        $array_details{status} = "fail";
        $array_details{errorMessage} = "pgrKey is missing in the policy info.";
        $logging_obj->log("EXCEPTION", "SCSI3_Reserve : pgrKey is missing in the info");
        return %array_details;
    }
    my $cmd = 'multipath -l '.$array_details{sanLunId}.' |grep "\\_" | grep sd | sed "s/\s\+/ /g" | cut -d " " -f 4';
    my $cmd_op = `$cmd`;
    $exit_code = `echo $?`;
    if($exit_code!=0)
    {
        $array_details{status} = "fail";
        $array_details{errorMessage} = "multipath command returned an error.";
        $logging_obj->log("EXCEPTION", "SCSI3_Reserve : multipath command returned an error with output : $cmd_op");
        return %array_details;
    }

    #now register all the paths and reserve one path.
    my @lines = split("\n",$cmd_op);
    foreach my $line (@lines)
    {
        $line =~ s/\s+//g; #trim.
        $cmd = 'sg_persist -n --out --register-ignore  --param-sark='.$array_details{pgrKey}.' /dev/'.$line;
        $cmd_op = `$cmd`;
        $exit_code = `echo $?`;
        if($exit_code != 0)
        {
            logging_obj->log("EXCEPTION", "scsi register command failed for /dev/".$line." with exit code $exit_code");
            #we are not breaking the loop here.
        }
    }
    #now clear the reserve for first device.
    $cmd = 'sg_persist --out --clear --param-rk='.$array_details{pgrKey}.' /dev/'.$lines[0];
    $cmd_op = `$cmd`;
    $exit_code = `echo $?`;
    if($exit_code != 0)
    {
        logging_obj->log("EXCEPTION", "scsi clear command failed for /dev/".$lines[0]." with exit code $exit_code");
        $array_details{status} = "fail";
        $array_details{errorMessage} = "scsi clear command failed for /dev/".$lines[0]." with exit code $exit_code";
        return %array_details;
    }
    #now resgister again for reserving for exclusive acces.
    foreach my $line (@lines)
    {
        $line =~ s/\s+//g; #trim
        $cmd = 'sg_persist -n --out --register-ignore  --param-sark='.$array_details{pgrKey}.' /dev/'.$line;
        $cmd_op = `$cmd`;
        $exit_code = `echo $?`;
        if($exit_code != 0)
        {
            logging_obj->log("EXCEPTION", "scsi register command failed for /dev/".$line." with exit code $exit_code");
            #now exit the loop because there is no point in continuing.
            $array_details{status} = "fail";
            $array_details{errorMessage} = "scsi register command failed for /dev/".$line." with exit code $exit_code";
            return %array_details;
        }
    }
    #now reserve with exclusive access to registrants only param.
    $cmd = 'sg_persist -n --out --reserve --prout-type=6 --param-rk='.$array_details{pgrKey}.' /dev/'.$lines[0];
    $cmd_op = `$cmd`;
    $exit_code = `echo $?`;
    if($exit_code != 0)
    {
        logging_obj->log("EXCEPTION", "scsi reserve command failed for /dev/".$lines[0]." with exit code $exit_code");
        $array_details{status} = "fail";
        $array_details{errorMessage} = "scsi reserve command failed for /dev/".$lines[0]." with exit code $exit_code";
        return %array_details;
    }
    else
    {
        $array_details{status} = "success";
        $array_details{errorMessage} = "successfully reserved exclusive access with key ".$array_details{pgrKey};
        return %array_details;
    }
}
sub SCSI3_Unreserve(%)
{
    my(%array_details) = @_;
    # Checking if all the required information is available. Else throwing error
    if(!$array_details{sanLunId})
    {
        $exit_code = 1;
        $array_details{status} = "fail";
        $array_details{errorMessage} = "sanLunId is missing in the policy info";
        $logging_obj->log("EXCEPTION","SCSI3_Unreserve : sanLunId is missing in the info");
        return %array_details;
    }
    if(!$array_details{pgrKey})
    {
        $exit_code = 1;
        $array_details{status} = "fail";
        $array_details{errorMessage} = "pgrKey is missing in the policy info.";
        $logging_obj->log("EXCEPTION", "SCSI3_Unreserve : pgrKey is missing in the info");
        return %array_details;
    }
    my $cmd = 'multipath -l '.$array_details{sanLunId}.' |grep "\\_" | grep sd | sed "s/\s\+/ /g" | cut -d " " -f 4';
    my $cmd_op = `$cmd`;
    $exit_code = `echo $?`;
    if($exit_code!=0)
    {
        $array_details{status} = "fail";
        $array_details{errorMessage} = "multipath command returned an error.";
        $logging_obj->log("EXCEPTION", "SCSI3_Reserve : multipath command returned an error with output : $cmd_op");
        return %array_details;
    }
    my @lines = split("\n",$cmd_op);
    #now clear the reserve for first device.
    $cmd = 'sg_persist --out --clear --param-rk='.$array_details{pgrKey}.' /dev/'.$lines[0];
    $cmd_op = `$cmd`;
    $exit_code = `echo $?`;
    if($exit_code != 0)
    {
        logging_obj->log("EXCEPTION", "scsi clear command failed for /dev/".$lines[0]." with exit code $exit_code");
        $array_details{status} = "fail";
        $array_details{errorMessage} = "scsi clear command failed for /dev/".$lines[0]." with exit code $exit_code";
        return %array_details;
    }
    else
    {
        $array_details{status} = "success";
        $array_details{errorMessage} = "scsi reservations successfully removed for ".$array_details{sanLunId};
        return %array_details;
    }
}

sub executeCommand(%)
{
    my(%array_details) = @_;

    if ($array_details{cmd} eq "detectResize")
    {
        %array_details = &detectResize(%array_details);
    }
    elsif($array_details{cmd} eq "iscsiLogin")
    {
        $logging_obj->log("EXCEPTION","Calling iscsiLogin function");
        %array_details = &iscsiPortLogin(%array_details);
    }
    return %array_details;
}

sub MountLun(%)
{
    my(%array_details) = @_;

    # Checking if all the required information is available. Else throwing error
    if(!$array_details{sanLunId})
    {
        $exit_code = 1;
        $array_details{status} = "fail";
        $array_details{errorMessage} = "sanLunId is missing in the policy info";
        $logging_obj->log("EXCEPTION","MountLun : sanLunId is missing in the info");
        return %array_details;
    }
    # Checking if all the required information is available. Else throwing error
    if(!$array_details{lunType} || ($array_details{lunType} ne "RETENTION" && $array_details{lunType} ne "GENERAL" && $array_details{lunType} ne  "SYSTEM"))
    {
        $exit_code = 0;
        $array_details{status} = "success";
        return %array_details;
    }
    #make fs if it required
    if ($array_details{isFormatRequired})
    {
        #unmount the retention volume.
        my $cmd = 'umount /dev/mapper/'.$array_details{sanLunId}.' >/dev/null 2>&1';
        `$cmd`;
        $cmd = 'mkfs -t '.$array_details{fsType}.' /dev/mapper/'.$array_details{sanLunId}.' >/dev/null 2>&1';
        my $cmd_op = `$cmd`;
        $exit_code = `echo $?`;
        if ($exit_code != 0)
        {
            $array_details{errorMessage} = "Unable to make the File System ".$array_details{fsType}." on the volume ".' /dev/mapper/'.$array_details{sanLunId};
            $array_details{status} = "fail";
            return %array_details;
        }
        $array_details{status} = "success";
    }
    if ($array_details{mountPoint})
    {
        if($array_details{state} ne $delete_target)
        {
            #check if already mounted.
            my $cmd = 'mount | grep '.$array_details{sanLunId}.' | cut -d " " -f 1';
            my $cmd_op = `$cmd`;
            $exit_code = `echo $?`;
            if($cmd_op !~ /mapper/)
            {
                #create mount point
                $cmd = 'mkdir -p '.$array_details{mountPoint}." 2>&1 >&1";
                $cmd_op=`$cmd`;
                $exit_code = `echo $?`;
                chop($cmd_op);
                if ($exit_code != 0)
                {
                    $array_details{errorMessage} = "Unable to mount the volume - mount returned ".$cmd_op.".";
                    $array_details{status} = "fail";
                    return %array_details;
                }
                #mount the retention volume.
                $cmd = 'mount -t '.$array_details{fsType}.' /dev/mapper/'.$array_details{sanLunId}.' '.$array_details{mountPoint}.' >/dev/null 2>&1';
                $cmd_op = `$cmd`;
                $exit_code = `echo $?`;
                if ($exit_code != 0)
                {
                    $array_details{errorMessage} = "Unable to mount the volume - mount returned $cmd_op.";
                    $array_details{status} = "fail";
                    return %array_details;
                }
            }
            $cmd = "dd if=/dev/zero of=".$array_details{mountPoint}."/writetest oflag=direct count=1 >/dev/null 2>&1";
            $cmd_op = `$cmd`;
            $exit_code = `echo $?`;
            if ($exit_code != 0)
            {
                $array_details{errorMessage} = "unable to write on to the volume - Error returned is $cmd_op.";
                $array_details{status} = "fail";
                return %array_details;
            }
            else
            {
                $cmd = "rm -f ".$array_details{mountPoint}."/writetest >/dev/null 2>&1";
                $cmd_op = `$cmd`;
                $array_details{errorMessage} = "Success";
                $array_details{status} = "success";
                return %array_details;
            }
        }
    }
    elsif( $array_details{state} eq $delete_target)
    {
        my $cmd = 'umount /dev/mapper/'.$array_details{sanLunId}." 2>&1 >/dev/null";
        `$cmd`;
        $array_details{errorMessage} = "Unmap and umount succeed for volume.";
        $array_details{status} = "success";
        return %array_details;
    }
    #If you are here means all went well.
    $array_details{errorMessage} = "Success";
    $array_details{status} = "success";
    return %array_details;
}
sub upgradePcli
{
    my($arrayType,%array_details) = @_;
    my ($wget_cmd,$tar_cmd,$cmd_ret);
    $logging_obj->log("EXCEPTION","upgrdePcli : Got the version mismatch error so upgrading the pcli,$arrayType");
	my $tar_file_path = $INSTALL_PATH."bin/".$array_details{ip}."/AxiomCli-RHEL-5-x86.tgz";
	
	#retrying with the new install path.
	$wget_cmd = "mkdir -p ".$INSTALL_PATH."bin/".$array_details{ip}.";wget --tries=1 --timeout=10 http://".$array_details{ip}."/installers/AxiomCli-RHEL-5-x86.tgz -qO ".$INSTALL_PATH."bin/".$array_details{ip}."/AxiomCli-RHEL-5-x86.tgz";
	
	$tar_cmd = "mkdir -p ".$INSTALL_PATH."bin/".$array_details{ip}."/AXIOMPCLI;tar -zxvf ".$tar_file_path." -C ".$INSTALL_PATH."bin/".$array_details{ip}."/AXIOMPCLI -x ./pcli";
        
	$PCLI_BIN_PATH = $INSTALL_PATH."bin/".$array_details{ip}."/AXIOMPCLI/pcli";	
	
	if($arrayType == 2)
	{
		$tar_file_path = $INSTALL_PATH."bin/".$array_details{ip}."/FSCli-RHEL-5-x86.tgz";
		
		$wget_cmd = "mkdir -p ".$INSTALL_PATH."bin/".$array_details{ip}.";wget --tries=1 --timeout=10 http://".$array_details{ip}."/installers/FSCli-RHEL-5-x86.tgz -qO ".$INSTALL_PATH."bin/".$array_details{ip}."/FSCli-RHEL-5-x86.tgz";
		
		$tar_cmd = "mkdir -p ".$INSTALL_PATH."bin/".$array_details{ip}."/FSPCLI;tar -zxvf ".$tar_file_path." -C ".$INSTALL_PATH."bin/".$array_details{ip}."/FSPCLI -x ./pcli";
        
	$PCLI_BIN_PATH = $INSTALL_PATH."bin/".$array_details{ip}."/FSPCLI/pcli";
		
    }
	if(-e "$tar_file_path")
    {
        my $dir_rm = $INSTALL_PATH.'bin/'.$array_details{ip}.'/';
        my $del_op = `rm -rf $dir_rm`;
        my $del_status = `echo $?`;
    }

    `$wget_cmd`;
    my $wget_return = `echo $?`;
    $logging_obj->log("EXCEPTION","upgrdePcli:wget_return::$wget_return");
	if($wget_return != 0)
    {
        # Creating the wget command to get the latest PCLI bin from the array
        #[Bug 18872] 6.2=>Pillar-Defect # 70595 -- The wget is trying default 20 times before giving up. Adding timeout and tries to try only once.
		#$logging_obj->log("EXCEPTION","upgrdePcli:wget_return::$wget_cmd");
        `$wget_cmd`;
        $wget_return = `echo $?`;
		$logging_obj->log("EXCEPTION","upgrdePcli:wget_return>>::$wget_cmd,$wget_return");
    }
    if($wget_return == 0)
    {
        #$logging_obj->log("EXCEPTION","upgrdePcli:tar_cmd>>::$tar_cmd");
		`$tar_cmd`;
	}
    else
    {
      #$logging_obj->log("EXCEPTION", "upgrdePcli : wget command failed.");
      $exit_code = 1;
      return 0;
    }
    
    if(-e $PCLI_BIN_PATH)
    {
        $logging_obj->log("DEBUG","upgrdePcli : Successfully upgraded the pcli");
        return 1;
    }
    else
    {
        $logging_obj->log("EXCEPTION","upgrdePcli : Failed to upgrade pcli");
        $exit_code = 1;
        return 0;
    }
}

sub detectResize
{
    my (%array_details) = @_;
    if(!$array_details{sanLunId})
    {
        $exit_code = 1;
        $array_details{status} = "fail";
        $array_details{errorMessage} = "sanLunId is missing in the policy info";
        $logging_obj->log("EXCEPTION","ExecuteCommand : sanLunId is missing in the info");
        return %array_details;
    }
    #make fs if it required

    my $cmd = 'multipath -ll '.$array_details{sanLunId};
    my $cmd_op = `$cmd`;
    $exit_code = `echo $?`;
    if ($exit_code != 0)
    {
        $array_details{errorMessage} = "Unable to list the multipath device".' /dev/mapper/'.$array_details{sanLunId};
        $array_details{status} = "fail";
        return %array_details;
    }
    if ($cmd_op =~ m/failed/g)
    {
        $exit_code = 1;
        $array_details{errorMessage} = "Unable to resize the multipath device".' /dev/mapper/'.$array_details{sanLunId}.". Few paths are offline. Make sure that all the paths are online before detecting the resize.";
        $array_details{status} = "fail";
        return %array_details;
    }
    while($cmd_op =~ m/(\d+:\d+:\d+:\d+)/g)
    {
        $cmd = 'echo 1> /sys/class/scsi_device/'.$1.'/device/rescan';
        my $rescan_op = `$cmd`;
        $exit_code = `echo $?`;
        if ($exit_code != 0)
        {
            $array_details{errorMessage} = "Unable to rescan the multipath device".' /dev/mapper/'.$array_details{sanLunId}." for path ".$1;
            $array_details{status} = "fail";
            return %array_details;
        }
    }
    $cmd = '/sbin/multipathd resize map '.$array_details{sanLunId}.' >/dev/null 2>&1';
    $cmd_op = `$cmd`;
    $exit_code = `echo $?`;
    if ($exit_code != 0)
    {
        $array_details{errorMessage} = "Unable to Resize the multipath device".' /dev/mapper/'.$array_details{sanLunId};
        $array_details{status} = "fail";
        return %array_details;
    }
    if(($array_details{mountPoint} ne "") && ($array_details{lunType} eq "RETENTION" || $array_details{lunType} eq "GENERAL" || $array_details{lunType} eq "SYSTEM"))
    {
        $cmd = '/sbin/resize2fs '."/dev/mapper/".$array_details{sanLunId}.' 2>&1';
        $cmd_op = `$cmd`;
        $exit_code = `echo $?`;
        if ($exit_code != 0)
        {
            $array_details{errorMessage} = "Multipath detected the resize but file system resize failed. resize2fs returned - ".$cmd_op.".";
            $array_details{status} = "success";
            return %array_details;
        }
    }
    #If you are here means all went well.
    $array_details{errorMessage} = "Success";
    $array_details{status} = "success";
}

sub iscsiPortLogin
{
    my(%array_details) = @_;
    my $status;
    my $final_status;
    my $error;
    foreach my $details (@{$array_details{"ports"}})
    {
        $logging_obj->log("DEBUG","To register ".Dumper($details));
        if($array_details{"action"} eq "register")
        {
            my $reg_cmd = "iscsiadm -m discovery -p ".$details->{ip};
            if($details->{portNo} ne "")
            {
                $reg_cmd .= ":".$details->{portNo};
            }
            $reg_cmd .= " -t sendtargets";
            $logging_obj->log("DEBUG","Register ISCSI command = $reg_cmd\n");
            my $reg_op = `$reg_cmd`;
            $status = `echo $?`;
            if($status == 0)
            {
                #send targets succeed so now we have to login to the nodes and the rescan to refresh the iscsi sessions.
                $logging_obj->log("DEBUG","Register ISCSI command succeeded with output $reg_op\n");
                my $login_cmd = "iscsiadm -m node -p ".$details->{ip};
                if($details->{portNo} ne "")
                {
                    $reg_cmd .= ":".$details->{portNo};
                }
                $login_cmd .= " --login";
                $logging_obj->log("DEBUG","Login ISCSI command = $login_cmd\n");
                my $login_op = `$login_cmd`;
                $status = `echo $?`;
                if($status == 0 || $status == 65280)
                {
                    $logging_obj->log("DEBUG","Login ISCSI command succeeded with output $login_op\n");

                    #rescan the nodes for new luns to refresh the sessions.
                    $login_cmd = "iscsiadm -m node -p ".$details->{ip};
                    if($details->{portNo} ne "")
                    {
                        $login_cmd .= ":".$details->{portNo};
                    }

                    $login_cmd .= " --rescan";
                    $login_op = `$login_cmd`;
                    $status = `echo $?`;
                    if(0 == $status)
                    {
                        $final_status += 1;
                        $logging_obj->log("DEBUG", "ISCSI node rescan succeeded with output $login_op\n");
                    }
                    else
                    {
                        $error = "ISCSI node rescan failed.";
                        $logging_obj->log("EXCEPTION", "ISCSI node rescan command $login_cmd failed with output $login_op\n");
                    }
                }
                else
                {
                    $error = "ISCSI ports login failed";
                    $logging_obj->log("EXCEPTION","Login ISCSI command $login_cmd failed with output $login_op\n");
                }
            }
            else
            {
                $error = "ISCSI ports registeration failed";
                $logging_obj->log("EXCEPTION","Register ISCSI command $reg_cmd failed with output $reg_op\n");
            }
        }
        else
        {
            my $unreg_cmd = "iscsiadm -m node -p ".$details->{ip};
            if ($details->{portNo} ne "")
            {
                $unreg_cmd .= ":".$details->{portNo};
            }
            $unreg_cmd .= " -u";
            $logging_obj->log("EXCEPTION","ISCSI node logout command = $unreg_cmd\n");
            my $unreg_op = `$unreg_cmd`;
            $status = `echo $?`;
            if($status == 0)
            {
                $final_status += 1;
                $logging_obj->log("DEBUG","ISCSI node logout command succeeded with output $unreg_op\n");
                $unreg_cmd = "iscsiadm -m discovery -p ".$details->{ip};
                if($details->{portNo} ne "")
                {
                    $unreg_cmd .= ":".$details->{portNo};
                }
                $unreg_cmd .= "-o delete";

                $logging_obj->log("EXCEPTION", "ISCSI discovery delete command = $unreg_cmd\n");
                $unreg_op = `$unreg_cmd`;
                $status = `echo $?`;
                if(0 == $status)
                {
                    $final_status +=1;
                    $logging_obj->log("DEBUG", "ISCSI discovery delete command succeeded with output $unreg_op\n");
                }
                else
                {
                    $error = "ISCSI discovery delete failed.";
                    $logging_obj->log("EXCEPTION", "ISCSI discovery delete command failed with output $unreg_op\n");
                }
            }
            else
            {
                $error = "ISCSI node logout failed";
                $logging_obj->log("EXCEPTION","ISCSI node logout command $unreg_cmd failed with output $unreg_op\n");
            }
        }
    }
    if($final_status <= 0)
    {
        $array_details{status} = "fail";
        $array_details{errorMessage} = $error;
        $exit_code = 1;
    }
    else
    {
        $array_details{status} = "success";
    }
    return %array_details;
}

sub ReplicationApplianceHeartbeat
{
    #request = ReplicationApplianceHeartbeat
    #ApplianceGuid =
    #AgentStatus =
    #ServiceStatus =
    my ($arrayType,%array_details) = @_;
    if (-e "/etc/ha.d/haresources")
    {
        `ifconfig |grep ":0 " | grep -w "Ethernet"`;
        my $isActive = `echo $?`;
        if ($isActive != 0)
        {
            $logging_obj->log("DEBUG","arrayManager.pl : This is a passive node. HB will not ne reported to Storage Array.\n");
            #Dont send HB from passive node.
            $array_details{status} = "success";
            return %array_details;
        }
    }
    my $error_file = "/tmp/".$$;
    $logging_obj->log("DEBUG", "Agent Heartbeat SharedApplianceGuid = ".$array_details{SharedApplianceGuid}."ApplianceGuid=".$array_details{ApplianceGuid}." AgentStatus=".$array_details{AgentStatus}." ServiceStatus=".$array_details{ServiceStatus}."\n");
    my $heartbeatCmd = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    $heartbeatCmd .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' ReplicationApplianceHeartbeat ApplianceGuid='.$array_details{SharedApplianceGuid}.' AgentStatus='.$array_details{AgentStatus}.' ServiceStatus='.$array_details{ServiceStatus}.' 2>'.$error_file;
    $logging_obj->log("DEBUG", "Agentheartbeat command is $heartbeatCmd\n");
    my ($heartbeatOp,$heartbeat_return) = &ExecutePcliCommand($heartbeatCmd);
    chomp($heartbeatOp);
    if($heartbeat_return != 0 || $heartbeatOp =~ /error/i || $heartbeatOp =~ /Unknown request/g)
    {
        my $cat_cmd = 'cat '.$error_file;
        my $cat_error_op = `$cat_cmd`;
        if($heartbeatOp =~ /Unknown request/g || $cat_error_op =~ /Unknown request/g)
        {
            if(!(&upgradePcli($arrayType,%array_details)))
            {
                $array_details{status} = "fail";
                $array_details{errorMessage} = "UNSUPPORTED CALL - ReplicationApplianceHeartbeat";
                my $data_return = PHP::Serialization::serialize(\%array_details);
                $logging_obj->log("DEBUG","arrayManager.pl : Return output is $data_return\n");
                print $data_return ."\n";
                exit 1;
            }
        }
        $array_details{status} = "fail";
        $array_details{errorMessage} = $heartbeatOp;
        $exit_code = 1;
    }
    else
    {
        $array_details{status} = "success";
    }
    unlink($error_file);
    return %array_details;
}

sub setMemory
{
    my ($state) = @_;
    $logging_obj->log("DEBUG","Entered setMemory\n");

    if ($state != $source && $state != $delete_mirror)
    {
        #nothing to do if it is not delete or create mirror.
        return;
    }
    my $protectedLuns = `/usr/local/InMage/Vx/bin/inm_dmit --get_protected_volume_list |wc -l`;
    if ($state == $delete_mirror)
    {
        #Let us reserve memory only required for total protected - (dummy lun + lun we are going to stop protection)
        $protectedLuns = $protectedLuns - 2;
    }

    my $cmd="cat /proc/meminfo |grep MemTotal";
    my $cmd_op=`$cmd`;
    my $totalMem =0;

    while($cmd_op =~ m/(\d+)/g)
    {
        $logging_obj->log("DEBUG","Total = $1\n");
        $totalMem=$1;
    }

    $logging_obj->log("DEBUG","Total memory = $totalMem KB\n");

    my $maxDataPoolPerc = `/usr/local/InMage/Vx/bin/inm_dmit --get_attr MaxDataPoolSize |tr '%' ' '`;
    my $variableSize = 2048;
    if ($protectedLuns == 1)
    {
        $variableSize = 10240;
    }
    elsif($protectedLuns == 2)
    {
        $variableSize = 10240 + 4096;
    }
    elsif($protectedLuns > 2)
    {
        $variableSize = 10240 + 4096 + (($protectedLuns - 2) * 2048);
    }

    my $maxDataPool = int($maxDataPoolPerc * $totalMem / 100/1024);
    $logging_obj->log("DEBUG","MaxDataPool = $maxDataPool, $maxDataPoolPerc\n");

    my $dataPoolSize = $variableSize;
    $logging_obj->log("DEBUG","dataPoolSize=$dataPoolSize, $maxDataPool\n");
    $logging_obj->log("DEBUG","$dataPoolSize , $maxDataPool\n");
    if ($dataPoolSize > $maxDataPool)
    {
        $dataPoolSize = $maxDataPool;
        $logging_obj->log("DEBUG"," Setting $dataPoolSize  to  $maxDataPool\n");
    }
    my $usedDataPool=`/usr/local/InMage/Vx/bin/inm_dmit  --get_attr DataPoolSize |tr 'MB' ' '`;
    $logging_obj->log("DEBUG","usedDataPool=$usedDataPool\n");
    if($dataPoolSize != $usedDataPool)
    {
        my $cmd="/usr/local/InMage/Vx/bin/inm_dmit  --set_attr DataPoolSize  $dataPoolSize";
        $logging_obj->log("DEBUG",$cmd."\n");
        `$cmd`;
    }
}

#Takes a pcli command, executes it and then returns the output in a string.
sub ExecutePcliCommand
{
    use Expect;
    my $cmd = shift;
    my $ret = "";
    my $ret_code ;
    #extracting the password from the string.
    $cmd =~ m/(-p\s+)(\S+)/;
    my ($password) = $2;
    $cmd =~ s/(-p\s+)(\S+)//;

    my $greps ;
    if($cmd =~ m/\|(.*)$/)
    {
        $greps = $1;
        $cmd =~ s/\|(.*)//;
    }
    $logging_obj->log("DEBUG", "Executing : $cmd");
    my $key = substr($password, 2, hex(substr($password , 0, 2)));
    my $pwd = substr($password, 2+hex(substr($password, 0, 2)));
    $password = `echo $pwd | openssl enc -aes-128-cbc -base64 -k $key -d`;
    $password =~ s/\n//g;

    my $exp = new Expect;
    $exp->log_stdout(0);
    $exp->raw_pty(1);
    $exp->spawn($cmd);
    $exp->expect(undef,"Password:");
    $exp->send($password."\n\n");
    #check the return code, if the command is malformed or complete, it will be non empty string.
    $ret_code = $exp->exitstatus();
    my @output = $exp->expect(1);
    while ($ret_code eq "" && $output[1] =~ /TIMEOUT/)
    {
        if($output[3] =~ /Password:\s*$password/)
        {
            $output[3] =~ s/Password:\s*$password//g;
        }
        $ret .= $exp->clear_accum();
        @output = $exp->expect(1);
    }
    $ret .= $output[3];
    $exp->soft_close();
    $ret_code = $exp->exitstatus();

    $ret =~ s/^\s+$//g;
    $ret =~ s///g;
    $ret =~ s/.*//g;
    if (defined $greps && $greps ne '')
    {
        open(PCLIFH, ">/tmp/$$.pcli");
        print PCLIFH $ret;
        close (PCLIFH);
        $ret=`cat /tmp/$$.pcli | $greps`;
        unlink "/tmp/$$.pcli";
    }

    return ($ret, $ret_code);
}

sub createSanHost
{
    my(%array_details) = @_;
    my $create_san_host_cmd = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    $create_san_host_cmd .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' CreateSanHost Name='.$array_details{sanHostName};


# Get the fcPorts and iscsiPorts in to arrays
    my (@fc_port_details,@iscsi_port_details);
    if($array_details{"fcPorts"}{"ai"})
    {
        @fc_port_details = @{$array_details{"fcPorts"}{"ai"}};
    }
    if($array_details{"iscsiPorts"}{"ai"})
    {
        @iscsi_port_details = @{$array_details{"iscsiPorts"}{"ai"}};
    }

# Counter for FCPORTS
    my $fc_count = 1;
# Counter for ISCSIPORTS
    my $iscsi_count = 1;
    foreach my $port_detail (@fc_port_details)
    {
        $port_detail =~ s/://g;
        $create_san_host_cmd .= ' FcInitiatorPortList.FcInitiatorPort['.$fc_count.'].WWN='.$port_detail.' ';
        $fc_count += 1;
    }
    foreach my $port_detail (@iscsi_port_details)
    {
        $create_san_host_cmd .= ' IScsiInitiatorPortList.IScsiInitiatorPort['.$iscsi_count.'].Name='.$port_detail.' ';
        $iscsi_count += 1;
    }
    if($array_details{state} == $source || $array_details{state} == $restart_resync)
    {
        $create_san_host_cmd .= ' RemoteReplicationAppliance=1 ';
    }
#	$create_san_host_cmd .= ' 2>'.$error_file;
    $logging_obj->log("DEBUG","createSanHost : Create San Host CMD $create_san_host_cmd\n");
#print "$create_san_host_cmd\n";
# Command for sanHost creation Done

# Execution of the command
    my ($create_san_host_cmd_op,$create_san_host_cmd_ret) = &ExecutePcliCommand($create_san_host_cmd);
}
sub getLunMapsToSanHost
{
    my(%array_details) = @_;
    my $error_file = "/tmp/".$$;

    # Getting Map information

    # Framing the command
    my $map_command = 'export LD_LIBRARY_PATH='.$PCLI_PATH.';';
    if($array_details{lunId})
    {
        $map_command .= $PCLI_BIN_PATH.' submit -H '.$array_details{ip}.' -T 300 -u '.$array_details{uname}.' -p '.$array_details{pwd}.' GetSanHostLunMap LunIdentity.Fqn="'.$array_details{lunId}.'"';
        $map_command .= ' SanHostIdentity.Fqn=/'.$array_details{sanHostName};
        $map_command .= ' | egrep "LunIdentity|MapIdentity|Id|Fqn|LunNumber|ErrorCode" 2>'.$error_file;
    }
    createSanHost(%array_details);
    modifySanHost(%array_details);
    #UnMap the lun from supplied sanhost only.
#    print "Get map :::: $map_command\n";
    $logging_obj->log("DEBUG","getLunMapsToSanHost : GetSanHostLunMap  command is $map_command\n");
    my ($map_cmd_op,$map_cmd_return) = &ExecutePcliCommand($map_command);
    $logging_obj->log("DEBUG","getLunMapsToSanHost : GetSanHostLunMap  command returned $map_cmd_op");

    #do not modify the exit code in this function.
    #$exit_code = $map_cmd_return;
    # If an error is returned
    if($map_cmd_op =~ /ErrorCode/ || $map_cmd_return !=0)
    {
        #$exit_code = 1;
        if($map_cmd_op =~ /ErrorCode/)
        {
            my @error = split(":",$map_cmd_op);
            $error[1] =~ s/\s+//g;
            $array_details{status} = "fail";
            $array_details{errorMessage} = $error[1];
        }
        else
        {
            my $cat_cmd = 'cat '.$error_file;
            my $cat_error_op = `$cat_cmd`;
            $array_details{errorMessage} = $cat_error_op;
        }
        $logging_obj->log("EXCEPTION","getLunMapsToSanHost : failed with error : output : $map_cmd_op");
        #my $serialized_array = PHP::Serialization::serialize(\%array_details);
        #return $serialized_array;
        #$exit_code = $map_cmd_return;
        unlink($error_file);
        return %array_details;
    }
    # If the command execution is successful
    else
    {
        $array_details{status} = "success";
        my @map_details;
        @map_details = split("\n",$map_cmd_op);
        my $map_length = $#map_details + 1;
        my($lun_id,$lun_number);
        my (@map_ids,@map_fqns,@host_ids);
        my $no_add_flag = 0;
        for(my $i=0;$i<$map_length;$i++)
        {
            if($map_details[$i] =~ /SanHostIdentity/)
            {
                my @hosts_id = split(":",$map_details[$i+2]);
                $hosts_id[1] =~ s/^\s+//;
                $hosts_id[1] =~ s/^\///;
                if($hosts_id[1] eq $array_details{sanHostName})
                {
                    $no_add_flag = 1;
                }
                else
                {
                    $no_add_flag = 0;
                }
            }
            if($no_add_flag == 1)
            {
                if($map_details[$i] =~ /LunNumber/)
                {
                    my @lun_nums = split(":",$map_details[$i]);
                    $lun_nums[1] =~ s/^\s+//;
                    $lun_number = $lun_nums[1];
                }
                elsif($map_details[$i] =~ /InitiatorIdentity/)
                {
                    my @hosts_id = split(":",$map_details[$i+1]);
                    $hosts_id[1] =~ s/^\s+//;
                    if(!(grep $_ eq $hosts_id[1], @host_ids))
                    {
                        push(@host_ids, $hosts_id[1]);
                    }
                }
                elsif($map_details[$i] =~ /MapIdentity/)
                {
                    my @hosts_id = split(":",$map_details[$i+1]);
                    $hosts_id[1] =~ s/^\s+//;
                    push(@map_ids, $hosts_id[1]);
                    @hosts_id = split(": ",$map_details[$i+2]);
                    $hosts_id[1] =~ s/^\s+//;
                    push(@map_fqns , $hosts_id[1]);
                }
                elsif($map_details[$i] =~ /LunIdentity/)
                {
                    my @lun_values = split(":",$map_details[$i+2]);
                    $lun_values[1] =~ s/^\s+//;
                    $lun_id = $lun_values[1];
                }
                if($lun_id && ($lun_number ne "") && @host_ids && @map_fqns && @map_ids)
                {
                    @{$array_details{$lun_id}{initiatorId}} = @host_ids;
                    $array_details{$lun_id}{lunNumber}   = $lun_number;
                    @{$array_details{$lun_id}{mapId}}   = @map_ids;
                    @{$array_details{$lun_id}{mapFqn}}   = @map_fqns;
                }
                if($array_details{lunId} && (!$array_details{$lun_id}{mapFqn}))
                {
                    $array_details{errorMessage} = "NOMAPS";
                }
            }
        }
    }
    # The file created for capturing standard error is unlinked here
    unlink($error_file);
    #my $serialized_array = PHP::Serialization::serialize(\%array_details);
    #return $serialized_array;
    #$logging_obj->log("DEBUG","getLunMapInfo returned ".Dumper(%array_details));
    return %array_details;
}
sub deleteLunMapsToSanHost
{
    # Verifying if the lun is already mapped
    my (%array_details) = @_;
    if($array_details{lunType} )
    {
        if ( $array_details{lunType} eq "RETENTION" || $array_details{lunType} eq "GENERAL" || $array_details{lunType} eq "SYSTEM")
        {
            return; #For retention and general luns we always unmap from all sanhosts before mapping to available sanhost.
        }
    }

    my %map_details = &getLunMapsToSanHost(%array_details);
    my @maps;
    if($map_details{$array_details{lunId}}{mapFqn})
    {
        my @maps = @{$map_details{$array_details{lunId}}{mapFqn}};
        $logging_obj->log("DEBUG","deleteLunMaps : The Lun ".$array_details{lunId}." is already mapped and maps are @maps\n");
        foreach my $map_fqn (@maps)
        {
            $array_details{mapId} = $map_fqn;
            $logging_obj->log("DEBUG","deleteLunMaps : Deleting the map ".$map_fqn."\n");
            $array_details{deleteFlag} = 1;
            if($array_details{isGlobalLun} ne "1")# && ($array_details{state} == $source || $array_details{state} == $restart_resync))
            {
                # Deleting the maps if they already exist;
                %array_details = &deleteLunMap(%array_details);
                if($array_details{isGlobalLun} eq "1")
                {
                    last;
                }
                if($array_details{status} eq "fail")
                {
                    return ;
                }
            }
        }
    }
$exit_code = 0;
}
sub deleteScsiDevices
{
   my ($lunId) = @_;
   my $cmd =  " for d in `multipath -l  $lunId | grep ' sd' | cut -d'-' -f2 |cut -d' ' -f2`;do dd=`echo \$d |sed -e \"s\/:\/ \/g\"`; echo \"scsi remove-single-device \$dd\" >/proc/scsi/scsi; done";
   `$cmd`;
    $logging_obj->log("DEBUG","Deleted scsi devices for LUN with Id $lunId; cmd is $cmd and exit status is $?");
   `/sbin/dmsetup remove -f /dev/mapper/$lunId 2&>1 >/dev/null`;
}
sub getArrayType
{
	my (%array_details) = @_;
	my ($wget_cmd,$type);
    #arrayType, axiom500=1, axiom800=2
	my $type = 2;
	if($array_details{modelNo}  eq "")
    {
		$wget_cmd = "wget -O /dev/null -q http://".$array_details{ip}."/installers/AxiomCli-RHEL-5-x86.tgz";
		`$wget_cmd`;
		my $wget_return = `echo $?`;
		
		if($wget_return == 0)
		{
			$type = 1;
		}
	}
	return $type;
}


1;
