#$Header: 
#================================================================= 
# FILENAME
#   NetworkTrends.pm 
#
# DESCRIPTION
#    This perl module is responsible for detecting the list of 
#    network interfaces on a *UNIX* based system. 
#    
#    It handles cases where there are multiple network interfaces 
#    on a host. Some of these interfaces may be bonded and some of 
#    them may be seperate ones not enslaved to a particular bonded 
#    interface. The functions in this module identify the list of 
#    network interfaces and group them based on the master interfaces 
#    they are enslaved to. This is then used for generating the MRTG 
#    configuration file to display the list of active network 
#    interfaces on the host.
#
# HISTORY
#     09 Jan 2007  bknathan    Created.
#
#=================================================================
#                 Copyright (c) InMage Systems                    
#=================================================================
package ProcessServer::Trending::NetworkTrends;

use strict;
use Sys::Hostname;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
#use lib qw( /home/svsystems/rrd/lib/perl/ /home/svsystems/pm );
use TimeShotManager;
use Common::Log;
use Utilities;
use RRDs;
use Data::Dumper;
use RequestHTTP;
 
my $logging_obj = new Common::Log();

#
# Global hash to hold the names of each network
# interface(one for slaves and one for normal ones)
# and the master interfaces.
#
my (%slave_list, %master_list, %other_ntw_dev);

sub new 
{
    my ($class, %args) = @_; 
    my $self = {};

    #
    # Initialize all the required variables for 
    # printing in the global options
    #
    
    $self->{'net_trends_dir'} = $args{'net_trends_dir'};
    $self->{'rrdtool_lib_path'} = $args{'rrdtool_lib_path'};
    $self->{'rrdtool_bin_path'} = $args{'rrdtool_bin_path'};
    $self->{'master_list'} = "";
    $self->{'hostname'} = hostname;
    
    %slave_list = ();
    %master_list = ();
    %other_ntw_dev = ();

    #
    # Use the /sbin/ip command to get the list of network
    # interfaces on the system. This is more reliable than
    # reading from /proc/net/dev as this file does not get 
    # updated with the appropriate statistics sometimes.
    #
	if(Utilities::isLinux()) {
		#my $get_netstat_cmd = "/sbin/ip -o link show 2>&1";
		my $get_netstat_cmd = "/sbin/ip -o addr show| grep -w inet| egrep -v 'inet6|127.0.0.1' 2>&1";

		$self->{'ntw_command_output'} = &run_ntw_command($get_netstat_cmd);
	}

    bless ($self, $class);
}

#
# Run the /sbin/ip command to collect network stats
#
sub run_ntw_command 
{
    my $command = shift;
    
    #
    # Run the network statistics command and 
    # collect the output.
    #
    my @output = `$command`;
    #my @output = get_net_stats();
    if ($? != 0)
    {
       
	   $logging_obj->log("EXCEPTION","Error retrieving network interface statistics: $! : @output");
	   $logging_obj->log("EXCEPTION"," -- Command : $command ");
    }
    return (\@output);
}

#
# Since we dont have multiple network devices, on
# every machine, i'm using this content below 
# just to simulate use case of a machine with
# 6 network cards out of which there are two bonded 
# interfaces slaving 2 NIC's each and 2 more are
# just seperate interfaces not enslaved to any master.
#
#  -- BKNATHAN
#
sub get_net_stats
{
    my $stats_file_contents = qq~
1: lo: <LOOPBACK,UP> mtu 16436 qdisc noqueue
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
2: bond0: <BROADCAST,MULTICAST,MASTER,UP> mtu 1500 qdisc noqueue
    link/ether 00:07:e9:09:ec:56 brd ff:ff:ff:ff:ff:ff
3: eth0: <BROADCAST,MULTICAST,SLAVE,UP> mtu 1500 qdisc pfifo_fast master bond0 qlen 1000
    link/ether 00:07:e9:09:ec:56 brd ff:ff:ff:ff:ff:ff
4: eth1: <BROADCAST,MULTICAST,SLAVE,UP> mtu 1500 qdisc pfifo_fast master bond0 qlen 1000
    link/ether 00:07:e9:09:ec:56 brd ff:ff:ff:ff:ff:ff
5: sit0: <NOARP> mtu 1480 qdisc noop
    link/sit 0.0.0.0 brd 0.0.0.0
6: bond1: <BROADCAST,MULTICAST,MASTER,UP> mtu 1500 qdisc noqueue
    link/ether 00:07:e9:09:ec:56 brd ff:ff:ff:ff:ff:ff
7: eth2: <BROADCAST,MULTICAST,SLAVE,UP> mtu 1500 qdisc pfifo_fast master bond1 qlen 1000
    link/ether 00:07:e9:09:ec:56 brd ff:ff:ff:ff:ff:ff
8: eth3: <BROADCAST,MULTICAST,SLAVE,UP> mtu 1500 qdisc pfifo_fast master bond1 qlen 1000
    link/ether 00:07:e9:09:ec:56 brd ff:ff:ff:ff:ff:ff
6: bond2: <BROADCAST,MULTICAST,MASTER,UP> mtu 1500 qdisc noqueue
    link/ether 00:07:e9:09:ec:56 brd ff:ff:ff:ff:ff:ff
7: eth4: <BROADCAST,MULTICAST,SLAVE,UP> mtu 1500 qdisc pfifo_fast master bond2 qlen 1000
    link/ether 00:07:e9:09:ec:56 brd ff:ff:ff:ff:ff:ff
8: eth5: <BROADCAST,MULTICAST,SLAVE,UP> mtu 1500 qdisc pfifo_fast master bond2 qlen 1000
    link/ether 00:07:e9:09:ec:56 brd ff:ff:ff:ff:ff:ff
9: eth6: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast qlen 1000
    link/ether 00:0c:29:83:d9:f4 brd ff:ff:ff:ff:ff:ff
~;

    my @output = split(/\n/,$stats_file_contents);

}

#
# Get the device statistics for each network interface on 
# the host. This is done by running the /sbin/ip command.
#
sub get_device_statistics
{
    my $self = shift;

    my @output = @{$self->{'ntw_command_output'}};

    #
    # Traverse through each line and find out if you 
    # get master or slave interfaces on the host.
    #   
    foreach my $line(@output)
    {
        chomp $line;

        if ($line =~ /(.*?):(.*?):(.*?slave.*)$/i)
        {
            my $ntw_device_name = $2;
            my $ntw_device_stats = $3;
            $ntw_device_name =~ s/\s+//g;
            $slave_list{$ntw_device_name} = $ntw_device_stats;
        }
        elsif ($line =~ /(.*?):(.*?):(.*?master.*)$/i)
        {
            my $ntw_device_name = $2;
            my $ntw_device_stats = $3;
            $ntw_device_name =~ s/\s+//g;
            $master_list{$ntw_device_name} = $ntw_device_stats;
        }
        elsif (($line !~ /loopback/i) 
        && ($line !~ /slave/i)
        && ($line =~ /inet/gi))
        {
          my  @data = split(" ",$line);
          my $ntw_device_name = $data[$#data];
	   #my $ntw_device_name = $2;
           # my $ntw_device_stats = $3;
            $ntw_device_name =~ s/\s+//g;
            $other_ntw_dev{$ntw_device_name} = $ntw_device_name;
        }
    }
  
    #
    # Group the network interface statistics 
    # for each master and slave interfaces on 
    # the host
    # 
    %master_list = &group_network_interfaces();
	
	#
	# To get the list of devices that are down
	#
	%master_list = &report_all_network_interfaces();
	
    $self->{'master_list'} = \%master_list;
}


#
# Get the ip address for each of the network interfaces
# connected to the host
#
sub get_ip_address
{
   my ($self) = shift;
   my $value;
   my $nic_ip = {};

   my @nics = keys %{$self->{'master_list'}};
   foreach my $nic (@nics)
   {
       if ($self->{'master_list'}->{$nic})
       {
	    push (@nics, @{$self->{'master_list'}->{$nic}});
       }
   }
  
   foreach my $nic (@nics) 
   {

	  my $english = `ifconfig | grep \"inet addr:\" 2>/dev/null`;
	  my $german  = `ifconfig  | grep \"inet Adresse:\" 2>/dev/null`;
      my $french  = `ifconfig | grep \"inet Adr:\" 2>/dev/null`;
		
	  if( $english ne "")
	  {
		 $value = "inet addr";
	  }
	  elsif($german ne "")
	  {
		 $value = "inet Adresse";
	  }
	  else
	  {
		 $value = "inet adr";
	  }

      my $ipconfig_cmd = "ifconfig -a $nic  | grep \"$value\"  | grep cast | awk -F\" \" \'{print \$2}\' | awk -F\":\" \'{print \$2}\'";
	  $logging_obj->log("DEBUG","get_ip_address  ipconfig_cmd: $ipconfig_cmd ");	  
      my $ip = `$ipconfig_cmd 2>&1`;	 
	  if($? == 0)
	  {
		$nic_ip->{$nic} = $ip;
      }
   } 
      return $nic_ip;
}

#
# Find out the slaves for each master (i.e if there are more
# than one bonded interfaces on the machine) and group them
# in the global hash.
#
sub group_network_interfaces
{
    #
    # Get the list of master interfaces from the
    # global list
    #
    my @master_nics = keys %master_list;

    #
    # Traverse through each of the interfaces in 
    # the master list and check which slave interfaces
    # are enslaved to it.
    #
    foreach my $master (@master_nics)
    {
        my @enslaved_list; 
        while (my ($interface, $stats) = each %slave_list)
        {
            if ($stats =~ /master $master/)
            {
                push @enslaved_list, $interface;
            }
            else
            { 
                if (exists $other_ntw_dev{$interface})
                {
                    $master_list{$interface} = "";
                }
            }
        }
   
        #
        # We now know the list of master interfaces and
        # the corresponding child interfaces enslaved to 
        # it.
        #
        $master_list{$master} = \@enslaved_list;
    }
    
    my @other_ntw_devs = keys %other_ntw_dev;

    #
    # Now that we have grouped all master interfaces
    # and the corresponding slaves, make each independent
    # interface a master NIC (as they are not enslaved to 
    # any other bonded interface)
    #
    foreach my $other_ntw_devices (@other_ntw_devs)
    {
        $master_list{$other_ntw_devices} = "";
    }
    
    return %master_list;
}

sub get_network_traffic {
  my $self = shift;
  my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
  my $ps_guid = $AMETHYST_VARS->{'HOST_GUID'};
  my ($bytes_in,$bytes_out,$send_to_cs,$nic_name);
  if (Utilities::isWindows()) {
    my $all_ips_details = `wmic path Win32_PerfRawData_Tcpip_NetworkInterface get BytesReceivedPerSec,BytesSentPerSec,name`;
    $all_ips_details = &clean_nic_name($all_ips_details);
    $all_ips_details =~ s/(\s{2})+/:/g;
    my @ips_details = split(/:/,$all_ips_details);
    for(my $j=0;$j<($#ips_details + 1);$j++) {
	  $ips_details[$j] =~ s/^\s+//;
      $ips_details[$j] =~ s/\s+$//;
	  if($ips_details[$j] =~ /^\d+$/) {
	    #print "matched $ips_details[$j] so nic is assigned $ips_details[$j+2] and bytes out = $ips_details[$j+1]\n";
	    $nic_name = $ips_details[$j+2];
        $bytes_in = $ips_details[$j];
        $bytes_out = $ips_details[$j+1];
		$j = $j + 2;
	  }
	  if($nic_name) {
		$bytes_in =~ s/\s//g;
		$bytes_out =~ s/\s//g;
        $send_to_cs .= $nic_name.":".$bytes_in.":".$bytes_out.";";
	  }
      #print "For $nic_name \nBytes In = $bytes_in\nBytes Out = $bytes_out\n";
    }
    #print "Sending CS $send_to_cs\n";
  }
  if (Utilities::isLinux()) {
    my $dev_input = `cat /proc/net/dev`;
    my @net_input = split(/\n/,$dev_input);
    foreach my $line (@net_input) {
      if($line =~ m/^\s*(.*):\s*(.*)$/) {
        my @get_int = split(/:/,$line);
        $get_int[0] =~ s/\s//g;
        my $interface = $get_int[0];
        if($interface ne "lo" && $interface ne "sit0") {
          $get_int[1] =~ s/\s+/ /g;
          $get_int[1] =~ s/^\s//g;
          my @values = split(/\s/,$get_int[1]);
          $bytes_in = $values[0];
          $bytes_out = $values[8];
		  $bytes_in =~ s/\s//g;
		  $bytes_out =~ s/\s//g;
          $send_to_cs .= $interface.":".$bytes_in.":".$bytes_out.";";
        }
        #print "For $interface , bytes In = $bytes_in, bytes_out = $bytes_out\n";
      }
    }
    #print "Sending CS $send_to_cs\n";
  }
  $send_to_cs = $ps_guid."|".$send_to_cs;
  #print "$send_to_cs\n";
  return $send_to_cs;
}
sub clean_nic_name() {
  my ($name) = @_;
  $name =~ s/{//g;
  $name =~ s/}//g;
  $name =~ s/,//g;
  $name =~ s/"//g;
  $name =~ s/\n//g;
  $name =~ s/\(//g;
  $name =~ s/\)//g;
  $name =~ s/\///g;
  $name =~ s/\\//g;
  $name =~ s/_//g;
  $name =~ s/\[//g;
  $name =~ s/\]//g;
  $name =~ s/\{//g;
  $name =~ s/\}//g;
  return $name;
}

sub getPsNetworkStats() 
{ 
   	eval
	{
		my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
		my $net_stats = &get_network_traffic();
		
		my $response;
		my $http_method = "POST";
		my $https_mode = ($AMETHYST_VARS->{'PS_CS_HTTP'} eq 'https') ? 1 : 0;
		my %args = ('HTTPS_MODE' => $https_mode, 'HOST' => $AMETHYST_VARS->{'PS_CS_IP'}, 'PORT' => $AMETHYST_VARS->{'PS_CS_PORT'});
		my $param = {'access_method_name' => "update_ps_network_traffic", 'access_file' => "/update_ps_network_traffic.php", 'access_key' => $AMETHYST_VARS->{'HOST_GUID'}};
		my %perl_args = ('network_stats' => $net_stats);
		my $content = {'content' => \%perl_args};
	
		my $request_http_obj = new RequestHTTP(%args);
		$response = $request_http_obj->call($http_method, $param, $content);
		
		$logging_obj->log("DEBUG","Response::".Dumper($response));
				
		if ($response->{'status'}) 
		{
		  $logging_obj->log("DEBUG","network_stats  : ".$net_stats."SUCCESS\n");
		}
		else 
		{
		  $logging_obj->log("EXCEPTION","Post failed:$net_stats\n");
		}
	};
	
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error : " . $@);
	}
}

sub updateNetworkRrds() {
  my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
  my $rrd_path = "$AMETHYST_VARS->{'NET_TRENDS_DIR'}/";
  if($^O =~ /Win/i ) {
    $rrd_path =~ s/\//\\/g;
    $rrd_path =~ s/^\\/C:\\/;
  }
  $logging_obj->log("DEBUG","Checking for the RRD existence at $rrd_path\n");
  my $cs_guid = $AMETHYST_VARS->{'HOST_GUID'};
  
  my $FILE_LOC = "$AMETHYST_VARS->{'INSTALLATION_DIR'}/var/PS_GUIDS";
  open(PS,"$FILE_LOC");
  while(<PS>) { 
    chomp($_);
    my $ps_guid = $_;
	 my ($serv_input);
    my $serv_file = "$AMETHYST_VARS->{'NET_TRENDS_DIR'}/".$ps_guid."-network_traffic.txt";
    if($^O =~ /Win/i ) {
      $AMETHYST_VARS->{'NET_TRENDS_DIR'} =~ s/\//\\/g;
      $serv_file = "C:".$AMETHYST_VARS->{'NET_TRENDS_DIR'}."\\".$ps_guid."-network_traffic.txt";
    }
    #$logging_obj->log("EXCEPTION","Looking for the file $serv_file to parse the contents for PS ID $ps_guid\n");
    #$logging_obj->log("EXCEPTION","opening file $serv_file\n");
    open(SERV,$serv_file);
    while(<SERV>) {
      $serv_input = $_;
      #print "$serv_input\n";
    }
    close(SERV);
    #print "Removing the file $serv_file after parsing the contents as $serv_input\n";
    #$logging_obj->log("EXCEPTION","Removing the file $serv_file after parsing the contents as $serv_input\n");
    unlink($serv_file);
    if($serv_input ne "") {
      $serv_input =~ s/\n//;
      $logging_obj->log("DEBUG","INPUT is ****$serv_input*********\n");
      my @nics_info = split(/;/,$serv_input);
      foreach my $nic (@nics_info) {
        my @update;
		my @data = split(/:/,$nic);
        $data[0] =~ s/\s//g;
        my $rrd_name = $rrd_path . $ps_guid . "-".$data[0].".rrd";
		my $bkup_file = "$AMETHYST_VARS->{'NET_TRENDS_DIR'}/".$ps_guid."-".$data[0].".txt";
	    if($^O =~ /Win/i ) {
	      $bkup_file = "C:".$AMETHYST_VARS->{'NET_TRENDS_DIR'}."\\".$ps_guid."-".$data[0].".txt";
    	}
		if(-e $bkup_file)
		{
			my ($bkup_input);
			open(BKUP,$bkup_file);
			while(<BKUP>) {
		      $bkup_input = $_;
		      #print "$serv_input\n";
    		}
		    close(BKUP);
			my @bkup = split(/:/,$bkup_input);
			if((($data[1] - $bkup[1]) > 0) && (($data[2] - $bkup[2]) > 0))
			{
				my $in = ($data[1] - $bkup[1])/300;
				push(@update,$in);
				my $out = ($data[2] - $bkup[2])/300;
				push(@update,$out);
				my $update_data = join(":",@update);
				$logging_obj->log("DEBUG","updateNetworRrds :: RRD : $rrd_name DATA : $update_data");
				&createNetworkRrd($rrd_name,$update_data);
			}
		}
        my $send = join(":",@data);
		open(BKUP_W,">$bkup_file");
		print BKUP_W $send; 
		close(BKUP_W);
	  }
	}
  }
}

sub createNetworkRrd($$) {
  my ($rrd,$data,$type,$time) = @_;
  $time = time unless ( $time );
  my @rrdcmd = ();
  my $flag = 0;
  if(! -e $rrd) {
    push @rrdcmd, $rrd;
    push @rrdcmd, "--step=300";
    push @rrdcmd, "--start=N";
    push @rrdcmd, "DS:in:GAUGE:86400:U:U";
    push @rrdcmd, "DS:out:GAUGE:86400:U:U";
    push @rrdcmd, "RRA:AVERAGE:0.5:1:800";
    push @rrdcmd, "RRA:AVERAGE:0.5:6:800";
    push @rrdcmd, "RRA:AVERAGE:0.5:24:800";
    push @rrdcmd, "RRA:AVERAGE:0.5:288:800";
    RRDs::create (@rrdcmd);
    my $ERROR = RRDs::error;
    die $logging_obj->log("EXCEPTION","$0: unable to create $rrd: $ERROR\n") if $ERROR;
    $flag = 1;
  }
  if($flag == 1) {
    @rrdcmd = ();
    $flag = 0;
    $time = time + 1;
  }
  push @rrdcmd, $rrd;
  push @rrdcmd, "$time:$data";
  $logging_obj->log("DEBUG","Updating $rrd with $time:$data\n");
  RRDs::update ( @rrdcmd );
  my $ERROR = RRDs::error;
  die $logging_obj->log("EXCEPTION","$0: unable to update $rrd: $ERROR\n") if $ERROR;
  return $rrd;
}

sub report_all_network_interfaces
{
	my $get_all_interface = `/bin/ls /sys/class/net | grep eth | sort`;
	my @all_interfaces = split(/\s+/,$get_all_interface);
	
	foreach my $interface (@all_interfaces)
	{
		if(! exists($master_list{$interface}))
		{
			$master_list{$interface} = "";
		}
	}
	return %master_list;
}

1;
