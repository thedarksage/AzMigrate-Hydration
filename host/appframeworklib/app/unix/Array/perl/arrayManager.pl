#!/usr/bin/perl

use lib qw(/usr/local/InMage/Vx/scripts/common /usr/local/InMage/Vx/scripts/Array/);
use strict;
use Data::Dumper;
use PHP::Serialization qw(serialize unserialize);
use pillarArrayManager;
use Log;
#use Common::DB::Connection;
#my $conn = new Common::DB::Connection();

my $logging_obj = new Log("pcli.log");
my $ARGC = @ARGV;

if($ARGC == 2)
{
    my $sanHostName = `hostname`;
    $sanHostName =~ s/\n//g; 
    $sanHostName = uc $sanHostName;
    my $cmd_type   = $ARGV[0];
    my $array_info = $ARGV[1];
    $logging_obj->log("DEBUG","arrayManager.pl : Data input is $array_info\n");
    my (%array_details,$data_return,$details);
    my ($arrayType,$array_bin_path);
    if ($array_info) 
    {

        chomp($array_info);
        $details = PHP::Serialization::unserialize($array_info);
#         $logging_obj->log("EXCEPTION","arrayManager.pl : details1".Dumper(\$details));
#         my $passwd =  $details->{pwd};
#         my $sql = "SELECT 
#                        DES_DECRYPT('$passwd') as passwd";
#    
#         my $sth = $conn->sql_query($sql);
#         my $ref = $conn->sql_fetchrow_hash_ref($sth);
#         $details->{pwd} = $ref->{passwd};
#         $logging_obj->log("EXCEPTION","arrayManager.pl : details2".Dumper(\$details));

        %array_details = %$details;

# Checking if all the required information is available. Else throwing error
        if(!$array_details{cmd} and (!$array_details{ip} or !$array_details{uname} or !$array_details{pwd}))
        {
            $exit_code = 1;
            $array_details{status} = "fail";
            $array_details{errorMessage} = "INFO NOT AVAILABLE CHECK LOG FOR DETAILS";
            $logging_obj->log("EXCEPTION","arrayManager.pl : Values for the keys ip or uname or pwd are empty");
            $data_return = PHP::Serialization::serialize(\%array_details);
            print $data_return;
            exit (1);
        }
        if (-e "/etc/ha.d/haresources")
        {
            `ifconfig |grep ":0 " | grep -w "Ethernet"`;
            my $isActive = `echo $?`;
            if ($isActive != 0)
            {
                $exit_code = 1;
                $logging_obj->log("DEBUG","arrayManager.pl : This is a passive node. HB will not ne reported to Storage Array.\n");
                $array_details{status} = "fail";
                $array_details{errorMessage} = "It is passive node and no requests will be processed on this node.";
                $data_return = PHP::Serialization::serialize(\%array_details);
                print $data_return;
                exit (1);

            }
        }

    }
    else
    {
        print "Please enter proper argument\n";
        $logging_obj->log("EXCEPTION","arrayManager.pl : Second argument empty\n");
        exit;
    }
    my %hash_return;

    if(!$array_details{cmd} and (!$array_details{type} or $array_details{type} ne "Axiom"))
    {
        print "Array type not specified as expected\n";
        $logging_obj->log("DEBUG","arrayManager.pl : Type of the Array is not valid.\n");
        exit;
    }
    else
    {

        my $install_path = $logging_obj->getInstallPath();
        #arrayType, axiom500=1, axiom600=1, axiom800=2
        if($array_details{modelNo} eq "")
        {
            $arrayType = &pillarArrayManager::getArrayType(%array_details);
        }
        elsif($array_details{modelNo} eq "AX500" or $array_details{modelNo} eq "AX600")
        {
            $arrayType = 1;
        }
        elsif($array_details{modelNo} eq "AX800" or $array_details{"modelNo"} eq "FS1_2")
        {
            $arrayType = 2;
        }

        $logging_obj->log("DEBUG","arrayManager.pl : got the array type as $arrayType\n");
        $array_bin_path = $install_path."bin/".$array_details{ip}."/AXIOMPCLI/pcli";
        $pillarArrayManager::PCLI_BIN_PATH = $install_path."bin/".$array_details{ip}."/AXIOMPCLI/pcli";
        if($arrayType == 2)
        {
            $array_bin_path = $install_path."bin/".$array_details{ip}."/FSPCLI/pcli";
            $pillarArrayManager::PCLI_BIN_PATH = $install_path."bin/".$array_details{ip}."/FSPCLI/pcli";
        }

        $logging_obj->log("DEBUG","arrayManager.pl : array_bin_path::$array_bin_path\n");
        if(!$array_details{cmd} and !(-e $array_bin_path))
        {
            if((&pillarArrayManager::upgradePcli($arrayType,%array_details)) == 0)
            {
                $array_details{status} = "fail";
                $array_details{errorMessage} = "UNKNOWN AXIOM";
                $data_return = PHP::Serialization::serialize(\%array_details);
                print $data_return."from the start\n";
                $logging_obj->log("DEBUG","arrayManager.pl : Return output is $data_return\n");
                exit 1;
            }
        }

        if($cmd_type eq "getArrayInfo")
        {
            %hash_return = pillarArrayManager::getpillarArrayDiscoveryInfo($arrayType,%array_details);
            $data_return = PHP::Serialization::serialize(\%hash_return);
        }    
        elsif($cmd_type eq "getLunInfo")
        {
            %hash_return = pillarArrayManager::getPillarArrayLunInfo(%array_details);
            $data_return = PHP::Serialization::serialize(\%hash_return);
        }
        elsif($cmd_type eq "regCxWithArray")
        {
            %hash_return = pillarArrayManager::registerCxWithArray(%array_details);
            $data_return = PHP::Serialization::serialize(\%hash_return);
        }
        elsif($cmd_type eq "unregCxWithArray")
        {
            %hash_return = pillarArrayManager::deregisterCxWithArray(%array_details);
            $data_return = PHP::Serialization::serialize(\%hash_return);
        }
        elsif($cmd_type eq "creatLunMap")
        {
#writing here because it should also support mount lun;
            my (%array_details_npiv) = %{PHP::Serialization::unserialize(PHP::Serialization::serialize(\%array_details))}; #do a deep copy. shallow copy will wreck havok.

            my $adSanHost = $array_details{"sanHostName"};
            my $mappedSanHost;
            if(ref($array_details_npiv{"fcPorts"}{"ai"}) eq "HASH") #do this only if the ai list is hashes.
            {
                my @hash_return;
                my @ports = sort(keys %{$array_details_npiv{"fcPorts"}{"ai"}});
#foreach my $port_info (sort(keys %{$array_details_npiv{"fcPorts"}{"ai"}}))
                my @port_info;
                my $i = 0;
                my $j = 1;
                if ($array_details{"sanHostName"}  ne "_AI_FOR_SOURCE")
                {
                    $port_info[0]=$array_details{"sanHostName"};

                    for (; $i < $#ports + 1; $i++)
                    {
                        if ($ports[$i] ne $port_info[0])
                        {
                            $port_info[$j] = $ports[$i];	
                            $j++;
                        }
                    }
                }
                else
                {
                    @port_info = @ports;
                }
                $i=0;
                for (; $i < $#port_info + 1; $i++)
                {
                    delete $array_details{"fcPorts"}{"ai"}; #remove the entry first;
                    $array_details{"sanHostName"} = $sanHostName.$port_info[$i];
                    $array_details{"fcPorts"}{"ai"} = PHP::Serialization::unserialize(PHP::Serialization::serialize(\@{$array_details_npiv{"fcPorts"}{"ai"}{$port_info[$i]}}));
                    if($array_details{"sanHostName"} !~ /_AI_FOR_.*1/) #not *_AI_FOR_SOURCE1 or *_AI_FOR_TARGET1
                    {
                        delete $array_details{"iscsiPorts"}{"ai"}; #remove the iscsiAIs if the sanhost is > 1
                    }
                    if ( $array_details{state} eq ${pillarArrayManager::delete_target})
                    {
                        pillarArrayManager::MountLun(%array_details);
                    }
                    @hash_return = pillarArrayManager::createHostLunMap($arrayType,%array_details);
                    $array_details{"sanHostName"} = $port_info[$i];
                    my %return_code = @hash_return;
                    if ($return_code{"errorMessage"} eq "LUN_NUMBER_NOT_AVAILABLE" or $return_code{"errorMessage"} eq "DUPLICATE_NAME" or "UNABLE_TO_ACQUIRE_LOCK_ERROR" eq $return_code{"errorMessage"})
                    {
                        $logging_obj->log("DEBUG", "LUN is not mapped as either the available LUN number is used to map by other thread or the sanhost trying to create is exist. Retrying LUN map again.\n");
                        $i--;
                        next;
                    }
                    if ($return_code{"errorMessage"} eq "NO LUN NUMBERS AVAILABLE" or $hash_return[0] eq "NoMaps" )
                    {
                        next;
                    }
                    else
                    {
                        last;
                    }
                }
                $mappedSanHost =  $array_details{"sanHostName"};
                $data_return = '|'.PHP::Serialization::serialize(\@hash_return).'|'.PHP::Serialization::serialize(\%array_details);
                $logging_obj->log("DEBUG", "final ouput is :".$data_return."\n");
#Delete the LUN Maps of other appliance San Hosts if the same LUN is mapped. If the previous m
                if ($adSanHost ne $mappedSanHost)
                {
                    $i++; #increment valu of i as last will not increment.
                    for ( ;$i < $#port_info + 1; $i++)
                    {
                        delete $array_details{"fcPorts"}{"ai"}; #remove the entry first;
                        $array_details{"sanHostName"} = $sanHostName.$port_info[$i];
                        $array_details{"fcPorts"}{"ai"} = PHP::Serialization::unserialize(PHP::Serialization::serialize(\@{$array_details_npiv{"fcPorts"}{"ai"}{$port_info[$i]}}));
                        if($array_details{"sanHostName"} !~ /_AI_FOR_.*1/) #not *_AI_FOR_SOURCE1 or *_AI_FOR_TARGET1
                        {
                            delete $array_details{"iscsiPorts"}{"ai"}; #remove the iscsiAIs if the sanhost is > 1
                        }
                        $array_details{"sanHostName"} = $sanHostName.$port_info[$i];
                        pillarArrayManager::deleteLunMapsToSanHost(%array_details);
                    }
                }
            }
            else
            {
#Store the arraySanHost received from CX to some temp and return it after successful map/unmap.
                my $arraySanHost = $array_details{"sanHostName"};
                $mappedSanHost =   $array_details{"sanHostName"};
                $array_details{"sanHostName"} = $sanHostName.$array_details{"sanHostName"};
                if ( $array_details{state} eq ${pillarArrayManager::delete_target})
                {
                    pillarArrayManager::MountLun(%array_details);
                }
                my @hash_return = pillarArrayManager::createHostLunMap($arrayType,%array_details);
#return the stored sanhost, withour hostname prepended to CX
                $array_details{"sanHostName"}=$arraySanHost;
                $data_return = '|'.PHP::Serialization::serialize(\@hash_return).'|'.PHP::Serialization::serialize(\%array_details);
            }
#if the request is not to unmap, delete all multipath scsi paths if the sanhost mapped is different from earlier. We add again by Appservice 
#this will help to remove the stale entried of scsi devices.
            if ( $array_details{state} ne ${pillarArrayManager::delete_target} and $adSanHost ne $mappedSanHost)
            {
                $logging_obj->log("DEBUG", " Deleting the stale multipath entri ");
                pillarArrayManager::deleteScsiDevices($array_details{"sanLunId"});
            }

		if ($exit_code == 0 && $arrayType == 2)
		{
		    pillarArrayManager::modifyRefTag(%array_details);
		}        
        }
        elsif($cmd_type eq "creatMirror")
        {
            my $arraySanHost = $array_details{"sanHostName"};
            $array_details{"sanHostName"} = $sanHostName.$arraySanHost;
            %hash_return = pillarArrayManager::lunMirrorInfo(%array_details);
            $hash_return{"sanHostName"} = $arraySanHost;
            $data_return = PHP::Serialization::serialize(\%hash_return);
        }
        elsif($cmd_type eq "executeCommand")
        {
            my $arraySanHost = $array_details{"sanHostName"};
            $array_details{"sanHostName"} = $sanHostName.$arraySanHost;
            %hash_return = pillarArrayManager::executeCommand(%array_details);
            $hash_return{"sanHostName"} = $arraySanHost;
            $data_return = PHP::Serialization::serialize(\%hash_return);
        }
        elsif($cmd_type eq "deleteLunMap")
        {
            my $arraySanHost = $array_details{"sanHostName"};
            $array_details{"sanHostName"} = $sanHostName.$arraySanHost;
            my @hash_return = pillarArrayManager::deleteLunMap(%array_details);
            $data_return = PHP::Serialization::serialize(\@hash_return);
        }
        elsif($cmd_type eq "getMirrorLunStatus")
        {
            %hash_return = pillarArrayManager::getMirrorLunStatus(%array_details);
            $data_return = PHP::Serialization::serialize(\%hash_return);
        }
        elsif($cmd_type eq "forceDeleteWriteSplit")
        {
            %hash_return = pillarArrayManager::forceDeleteMirror(%array_details);
            $data_return = PHP::Serialization::serialize(\%hash_return);
        }
        elsif($cmd_type eq "getSanHostLunMaps")
        {
            %hash_return = pillarArrayManager::getSanHostLunMaps(%array_details);
            $data_return = PHP::Serialization::serialize(\%hash_return);
        }
        elsif($cmd_type eq "mountLun")
        {
            %hash_return = pillarArrayManager::MountLun(%array_details);
            $data_return = PHP::Serialization::serialize(\%hash_return);
        }
        elsif($cmd_type eq "ReplicationApplianceHeartbeat")
        {
            %hash_return = pillarArrayManager::ReplicationApplianceHeartbeat($arrayType,%array_details);
            $data_return = PHP::Serialization::serialize(\%hash_return);
        }
        if($hash_return{errorMessage} && $hash_return{errorMessage} =~ /ELEMENT_VERSION_MISMATCH/)
        {
#$data_return = PHP::Serialization::serialize(\%hash_return);
            if(pillarArrayManager::upgradePcli($arrayType,%array_details))
            {
                delete($hash_return{errorMessage});
                delete($hash_return{status});
#my $call_me_cmd = "/usr/bin/perl ".$install_path."scripts/Array/arrayManager.pl ".$cmd_type." '$data_return'";
                $data_return = PHP::Serialization::serialize(\%hash_return);
                my $call_me_cmd = "/usr/bin/perl ".$0." ".$cmd_type." '$data_return'";
                $data_return = `$call_me_cmd`; 
            }
            else
            {
                $array_details{status} = "fail";
                $array_details{errorMessage} = "UNKNOWN AXIOM";
                $data_return = PHP::Serialization::serialize(\%array_details);
            }
        }
        $logging_obj->log("DEBUG","arrayManager.pl : Return output is $data_return\n");
        print $data_return;
        if ($exit_code != 0)
        {
            exit (1);
        }        
    }
}
else
{
    print "Please pass valid arguments\nUsage:\n\t\tperl arrayManager.pl <command> <PHP Serialized String>\n";
    $logging_obj->log("EXCEPTION","arrayManager.pl : Invalid Arguments\n");
    exit;
}
