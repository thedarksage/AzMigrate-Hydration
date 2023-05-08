#!/usr/bin/perl

#FILENAME
#   fabricagent.pm
#
#DESCRIPTION
#   This perl module implements the tasks related to fabric environments.
#
#HISTORY
#   06 November 2012 Raghu rmadanala@inmage.com Created.
#
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================
#
##################################################################

package fabriccontroller;

use lib qw(/usr/local/InMage/Vx/scripts/Array /home/svsystems/pm);
use lib qw(/home/svsystems/pm);
use strict;
use PHP::Serialization qw(serialize unserialize);
use Data::Dumper;
use Log;

# Logging object for log messages
my $logging_obj = new Log();
#my $LOG_DIR = $CONF_PARAM{"LogPathname"}."fabriccontroller.log";

sub reportHBAs(%)
{
    my (%hba_info) = @_;
    $hba_info{"Version"} = "2.0";
    #collect the info of each hba;
    my @hbaPorts = `/bin/ls /sys/class/fc_host`;
    foreach my $hbaPort (@hbaPorts)
    {
        my %hba_port_info;
        chomp($hbaPort);
        $hba_port_info{"PWWN"}          = `cat /sys/class/fc_host/$hbaPort/port_name`;
        $hba_port_info{"NWWN"}          = `cat /sys/class/fc_host/$hbaPort/node_name`;
        $hba_port_info{"PortType"}      = `cat /sys/class/fc_host/$hbaPort/port_type`;
        $hba_port_info{"Speed"}         = `cat /sys/class/fc_host/$hbaPort/speed`;
        $hba_port_info{"SymbolicName"}  = `cat /sys/class/fc_host/$hbaPort/symbolic_name`;
        $hba_port_info{"PortState"}     = `cat /sys/class/fc_host/$hbaPort/port_state`;
        $hba_port_info{"PortType"}      = `cat /sys/class/fc_host/$hbaPort/port_type`;
        $hba_port_info{"SupportedClass"}= `cat /sys/class/fc_host/$hbaPort/supported_classes`;
        $hba_port_info{"PortId"}        = `cat /sys/class/fc_host/$hbaPort/port_id`;

        foreach (values %hba_port_info) { s/\n//g; };

        my %vpd = parseVPD("/sys/class/fc_host/$hbaPort/device/vpd");
        $hba_port_info{"VPD"} = \%vpd;
        $hba_info{$hba_port_info{"PWWN"}} = \%hba_port_info;
    }
    return %hba_info;
}

sub parseVPD($)
{
    my ($vpdFile) = @_;
    open my $VPD, "<", $vpdFile;
    binmode($VPD, ":bytes");
    my %vpdData;
    my $buffer;
    while (3 == read ($VPD, $buffer, 3))
    {
        my @header = unpack("C3", $buffer);
        my $length = $header[2];
        my $value;
        if      ($header[0] == 0x82) #product name
        {
            $length = $header[1];
            read ($VPD, $value, $length);
            $vpdData{"Product Name"} = unpack ("a*", $value);
        }
        elsif   ($header[0] == unpack("C",'P')) #PN or PG
        {
            read ($VPD, $value, $length);
            if      ($header[1] == unpack("C", 'N')) {$vpdData{"Part Number"} = unpack("a*", $value);}
            elsif   ($header[1] == unpack("C", 'G')) {$vpdData{"PCI Geography"} = unpack("a*",$value);}
        }
        elsif   ($header[0] == unpack("C", 'E')) #EC
        {
            read ($VPD, $value, $length);
            $vpdData{"EC Level"} = unpack("a*", $value);
        }
        elsif   ($header[0] == unpack("C", 'F')) #FG
        {
            read ($VPD, $value, $length);
            $vpdData{"Fabric Geography"} = unpack("a*", $value);
        }
        elsif   ($header[0] == unpack("C", 'L')) #LC
        {
            read ($VPD, $value, $length);
            $vpdData{"Location"} = unpack("a*", $value);
        }
        elsif   ($header[0] == unpack("C", 'M')) #MN
        {
            read ($VPD, $value, $length);
            $vpdData{"Manufacture Number"} = unpack("a*", $value);
        }
        elsif   ($header[0] == unpack("C", 'S')) #SN
        {
            read ($VPD, $value, $length);
            $vpdData{"Serial Number"} = unpack("a*", $value);
        }
        elsif   ($header[0] == unpack("C", 'V')) #Vx
        {
            read ($VPD, $value, $length);
            my @keyChars = unpack ("aaa", $buffer);
            my $key = "VendorSpecific".$keyChars[1];
            $vpdData{$key} = unpack("a*", $value);
        }
        elsif   ($header[0] == unpack ("C", 'C')) #CP
        {
            read ($VPD, $value, $length);
            $vpdData{"Extended Capability"} = unpack("a*", $value);
        }
        #elsif   ($header[0] == 'R') #RV or RW #dont need these tags.
        elsif   ($header[0] == unpack("C", 'Y')) #Yx or YA
        {
            read ($VPD, $value, $length);
            if      ($header[1] == unpack("C", 'A')) {$vpdData{"Asset Tag"} = unpack("a*", $value);}
            else
            {
                my @keyChars = unpack("aaa", $buffer);
                my $key = "System Specific".$keyChars[1];
                $vpdData{$key} = unpack("a*", $value);
            }
        }
    }
    close($VPD);
    return %vpdData;
}

1;
