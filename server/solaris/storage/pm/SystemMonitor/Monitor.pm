#=================================================================
# FILENAME
#   Monitor.pm
#
# DESCRIPTION
#    This perl module is used by the health monitor thread
#
#=================================================================
package SystemMonitor::Monitor;

use lib qw( /home/svsystems/rrd/lib/perl/ /home/svsystems/pm);
use strict;
use Utilities;
use Common::Constants;
use Data::Dumper;
use Common::Log;
use PHP::Serialization qw(serialize unserialize);

my $logging_obj = new Common::Log();

our $all_disk_info;
our $all_errors;
our $smatctl_path = "/home/svsystems/bin/";
# 
# Function Name: new
#
# Description: Constructor for the Monitor class.
#
# Parameters: None 
#
# Return Value:None
#  	
sub new
{
	my ($class) = shift;    
	my $self = {};	
	bless ($self, $class);
}

# 
# Function Name: getCpuLoad
#
# Description: Used to get the CPU Usage Load
#
# Parameters: None 
#
# Return Value: Returns the cpu usage load of the system
#  

sub getCpuLoad
{
	my $self = shift;
	my $cpu_idle;
	my $cpu_load;
	my $cpu_load_all;
	my $avg_cpu_load;
	my $avg_cpu_load_per;
	#The sum of all the processes to idle processes gives the idle% of CPU 
	#and subtracting this value from 100 gives the CPU Usage Load.
	$cpu_idle = `iostat -c 1 6 |tail -5|awk \'{print \$4}'`;
	my @cpu_idle_output = split(/\n/,$cpu_idle);
	foreach my $cpu_idle(@cpu_idle_output)
	{
		$cpu_load = 100 - $cpu_idle; 
		$cpu_load_all = $cpu_load_all + $cpu_load;
	}
	$avg_cpu_load = $cpu_load_all / 5;
	$avg_cpu_load = sprintf("%.2f", $avg_cpu_load);
	return $avg_cpu_load;
}

# 
# Function Name: getMemUsage
#
# Description: Gives the physical memory usage.
#
# Parameters: None 
#
# Return Value: Returns the memory used.
#
sub getMemUsage
{
	my $self = shift;
	my ($memory_availble_and_use,$used_memory,$available_memory,$memory_usage,$total_memory);
	my (@mem_info,@det);
	my $i = 0;
	$memory_availble_and_use = `echo ::memstat | mdb -k |awk 'NR>8'`;
	
	@mem_info = split(/\n/,$memory_availble_and_use);
	foreach my $mem(@mem_info) 
	{
		if($i == 0)
		{
			@det = split(/\s+/,$mem);
			$available_memory = $det[3];
		}
		else
		{
			@det = split(/\s+/,$mem);
			$total_memory = $det[2];
			last;
		}
		$i++;
	}
	
	$used_memory = $total_memory - $available_memory;
	if($used_memory)
	{
		$memory_usage = ($used_memory / $total_memory) * 100;
		$memory_usage = sprintf("%.2f", $memory_usage);
	}
	my $available_memory = $available_memory."M";
	$total_memory = $total_memory."M";
	$available_memory = $self->getCapacityInBytes($available_memory);
	$total_memory = $self->getCapacityInBytes($total_memory);
	
	return ($memory_usage,$total_memory,$available_memory);
}

# 
# Function Name: getDiskActivity
#
# Description: Used to get the disk read and writes from or to IO on the system. 
#
# Parameters: None 
#
# Return Value: Returns disk_activity
#
sub getDiskActivity
{
	my ($self) = @_;
	my ($disk_usage,$disk_reads,$disk_writes,$disk_reads_all,$disk_writes_all,$disk_throughput,$disk_name);
	my (@disk_throughput_value,@all_disks);
	my $numiters = 6;
	my $numentries = `iostat -xn | grep \'c[0-9]t*\' | wc -l`;
	
	$logging_obj->log("EXCEPTION","request_xml ::getDiskActivity numentries - $numentries \n");
	
	my $numreqdlines = ($numiters - 1)*$numentries;
	my $disk_usage = `iostat -xn 1 6 | grep \'c[0-9]t*\'|tail -"$numreqdlines"|awk \'{print \$3,\$4,\$11}\'`;
	my @disk_through_output = split(/\n/,$disk_usage);
	
	foreach $disk_throughput(@disk_through_output)
	{
		@disk_throughput_value = split(/ /,$disk_throughput);
		$disk_name =	$disk_throughput_value[2];
		push(@all_disks,$disk_name);
		$disk_reads =	$disk_throughput_value[0].'K';
		$disk_reads = $self->getCapacityInBytes($disk_reads);
		$disk_writes  =	$disk_throughput_value[1].'K';
		$disk_writes = $self->getCapacityInBytes($disk_writes);
		
		$disk_reads_all  = $disk_reads_all + $disk_reads;
		$disk_writes_all = $disk_writes_all + $disk_writes;
		
		if((grep  $_ eq $disk_name,@all_disks ))
		{
			$disk_reads = $all_disk_info->{$disk_name}->{'disk_reads'} + $disk_reads;
			$disk_writes = $all_disk_info->{$disk_name}->{'disk_writes'} + $disk_writes;
		}
		
		$all_disk_info->{$disk_name}->{'disk_reads'} = $disk_reads;
		$all_disk_info->{$disk_name}->{'disk_writes'} = $disk_writes;
	}
	
	$logging_obj->log("EXCEPTION","getDiskActivity :: ".Dumper($all_disk_info)." \n");
	
	foreach my $disk (keys %{$all_disk_info})
	{
		if($all_disk_info->{$disk}->{'disk_reads'})
		{
			$all_disk_info->{$disk}->{'disk_reads'} = $all_disk_info->{$disk}->{'disk_reads'}/($numiters-1);
		}
		if($all_disk_info->{$disk}->{'disk_writes'})
		{
			$all_disk_info->{$disk}->{'disk_writes'} = $all_disk_info->{$disk}->{'disk_writes'}/($numiters-1);
		}
	}

	$logging_obj->log("EXCEPTION","getDiskActivity Final values:: ".Dumper($all_disk_info)." \n");
	$disk_reads = $disk_reads_all/($numiters - 1);
	$disk_writes = $disk_writes_all/($numiters - 1);
	$disk_throughput = $disk_reads + $disk_writes;
	$disk_throughput = sprintf("%.3f", $disk_throughput);
	$logging_obj->log("EXCEPTION","getDiskActivity disk_throughput:: ".$disk_throughput." \n");
	return $disk_throughput;
}

# 
# Function Name: getCacheUsage
#
# Description: Gives the cache usage.
#
# Parameters: None 
#
# Return Value: Returns the cache usage.
# 
sub getCacheUsage
{
	my $self = shift;
	
	my $total_cache_size = 0;
	my $allocated_size = 0;
	my $free_size = 0;
	my $cache_usage = 0; 
	my $cumulative_free_size = 0;
	my $cumulative_allocated_size = 0;
		
	my $zpool_list =	`zpool list | awk \'NR>1\' | awk \'{print \$1 }\'`;
	my @zpool_name = split(/\n/,$zpool_list);
	foreach my $zpool_name (@zpool_name)
	{
		my $is_cache = 0;
		my $cache_disk_list = `zpool status $zpool_name|awk \'{print \$1} \'`;
		my @cache_disk = split(/\n/,$cache_disk_list);
		foreach my $cache_disk_name (@cache_disk) 
		{
			if($is_cache)
			{
				if(($cache_disk_name =~ /^c[0-9]/))
				{
					my $zpool_cache_size = `zpool iostat -v $zpool_name | egrep $cache_disk_name | awk \'{print \$2,\$3}\'`;
					my @zpool_cache_size_arr = split(" ",$zpool_cache_size);
					my $allocated_size_str = $zpool_cache_size_arr[0];
					my $free_size_str = $zpool_cache_size_arr[1];
					chomp($free_size_str);
					
					$allocated_size = $self->getCapacityInBytes($allocated_size_str);
					$free_size = $self->getCapacityInBytes($free_size_str);
					
					$cumulative_allocated_size = $cumulative_allocated_size + $allocated_size;
					$cumulative_free_size = $cumulative_free_size + $free_size;
				}
				else
				{
				   $is_cache = 0;
				}
			}
			
			if($cache_disk_name eq 'cache')
			{
				$is_cache = 1;
			}
		}
	}
	$total_cache_size = $cumulative_allocated_size + $cumulative_free_size;
	if($total_cache_size != 0)
	{
		$cache_usage = ($cumulative_allocated_size/$total_cache_size)*100;
	}
	
	$cache_usage = sprintf("%.2f",$cache_usage);
	return ($cache_usage,$cumulative_free_size,$total_cache_size);
}

# 
# Function Name: getPoolUsage
#
# Description: Used to get the Pool Usage
#
# Parameters: None 
#
# Return Value: Returns the pool throughput, readiops & writeiops for each pool
#
sub getPoolUsage
{
	my $self = shift;
	my($read_iops, $write_iops,$pool_reads_all,$pool_writes_all,$pool_usage_details,$pool_throughput,$status, $pool_reads,$pool_writes,$status_op,$pool_read_iops,$pool_write_iops);
	my $count = 0;
	
	my $zpool_list =`zpool list | awk \'NR>1\' | awk \'{print \$1 }\'`;
	my @zpool_name = split(/\n/,$zpool_list);
	
	my($all_pools_info,$all_iops_info) = $self->getAllPoolThroughput();	
	
	foreach my $zpool_name (@zpool_name) 
	{
		$status_op = `zpool status -T d $zpool_name | grep "state" | cut -d":" -f2`;
		my @status_details = split(/\n/,$status_op);
		$status = $status_details[0];
		$status =~ s/\s//g;
		
		$pool_usage_details->{$count}->{'status'} = $status;
		$pool_usage_details->{$count}->{'zpool_name'} = $zpool_name;
		
		if($zpool_name ne "rpool")
		{			
			$pool_read_iops = $all_iops_info->{$zpool_name}->{'read_iops'};
			$pool_write_iops = $all_iops_info->{$zpool_name}->{'write_iops'};
			
			$pool_reads = $all_pools_info->{$zpool_name}->{'pool_reads'};
			$pool_writes = $all_pools_info->{$zpool_name}->{'pool_writes'};			
			
			$pool_throughput = $pool_read_iops + $pool_write_iops;
			$pool_throughput = sprintf("%.3f", $pool_throughput);			
			
			$pool_usage_details->{$count}->{'read_iops'} = $pool_reads;
			$pool_usage_details->{$count}->{'write_iops'} = $pool_writes;
			$pool_usage_details->{$count}->{'pool_throughput'} = $pool_throughput;			
		}
		$count++;
	}
	
	if($pool_usage_details)
	{
		$pool_usage_details = serialize($pool_usage_details);
	}
	return $pool_usage_details;
}

# 
# Function Name: getNetworkUsage
#
# Description: Used to get the Network details
#
# Parameters: None 
#
# Return Value: Returns the readbytes & outbytes for each nic
#
sub getNetworkUsage
{
	my $self = shift;
	my($rbytes,$obytes,$network_info,$nic_op,$nic_state,$nic_name,$nic_mtu);
	my @disk_info;
	my $count = 0;
	
	my $network_result = `dladm show-link | awk \'NR>1\' | awk \'{ print \$1,\$3}\'`;
	my @network_details = split(/\n/,$network_result);
	
	my($nic_status, $nic_ro) = $self->getAllNicStatus();
	
	foreach my $nic_op (@network_details)
	{
		my @nic_details = split(/ /,$nic_op);
		$nic_name = $nic_details[0];
		$nic_name =~ s/^\s+|\s+$//g;
		$nic_mtu = $nic_details[1];
		$nic_mtu =~ s/^\s+|\s+$//g;
		
		$network_info->{$count}->{'nic_name'} = $nic_name;
		$network_info->{$count}->{'nic_mtu'} = $nic_mtu;
		$network_info->{$count}->{'rbytes'} = $nic_ro->{$nic_name}->{'rbytes'};
		$network_info->{$count}->{'obytes'} = $nic_ro->{$nic_name}->{'obytes'};
		$network_info->{$count}->{'status'} = $nic_status->{$nic_name};
		$count++;
	}
	if($network_info)
	{
		$network_info = serialize($network_info);
	}
	return $network_info;
}

# 
# Function Name: getSystemUsage
#
# Description: Used to fetch all the system related info
#
# Parameters: None 
#
# Return Value: Returns the cpu load, cache usage, memory usage & disk activity for the storage node
#
sub getSystemUsage
{
	my $self = shift;
	my($cpu_usage,$flash_cache_usage,$mem_usage,$disk_activity,$system_usage_info);	
	$cpu_usage = $self->getCpuLoad();
	my($flash_cache_usage,$available_arc_size,$total_arc_size) = $self->getCacheUsage();	
	my($mem_usage,$total_memory,$available_memory) = $self->getMemUsage();
	$disk_activity = $self->getDiskActivity();
	
	#my $service_op = `svcs -a | grep -i scoutsms | awk \'{print \$1}\'`;	
	#$service_op =~ s/^\s+|\s+$//g;
	
	$system_usage_info->{'cpu_load'} = $cpu_usage;
	$system_usage_info->{'cache_usage'} = $flash_cache_usage;
	$system_usage_info->{'available_cache_size'} = $available_arc_size;
	$system_usage_info->{'total_cache_size'} = $total_arc_size;
	$system_usage_info->{'memory_usage'} = $mem_usage;
	$system_usage_info->{'available_memory'} = $available_memory;
	$system_usage_info->{'total_memory'} = $total_memory;
	$system_usage_info->{'disk_activity'} = $disk_activity;
	#$system_usage_info->{'service_status'} = $service_op;
	$system_usage_info = serialize($system_usage_info);
	return $system_usage_info;
}

# 
# Function Name: getLunStatus
#
# Description: Used to get the status of all luns
#
# Parameters: None 
#
# Return Value: Returns the health status of all luns.
#
sub getLUNDetails
{
	my $self = shift;
	my ($lun_details,$lun_status);
	my $count = 0;
	
	my $luns = `stmfadm list-lu | cut -d":" -f2`;
	my @storage_luns = split(/\n/,$luns);
	foreach my $lun_name (@storage_luns)
	{
		$lun_name =~ s/^\s+|\s+$//g;
		
		$lun_status = `stmfadm list-lu -v $lun_name | grep 'Operational Status' | cut -d":" -f2`;
		$lun_status =~ s/^\s+|\s+$//g;
		$lun_details->{$count}->{'status'} = $lun_status;
		$lun_details->{$count}->{'lun_name'} = $lun_name;
		$count++;
	}
	if($lun_details)
	{
		$lun_details = serialize($lun_details);
	}
	return $lun_details;
	
}	

# 
# Function Name: getSensorDetails
#
# Description: Used to get all the sensors(fan, temperature, voltage, power_supply) information
#
# Parameters: None 
#
# Return Value: Returns the all the sensors related information available on the storage node.
#
sub getSensorDetails
{
	my $self = shift;
	my ($fan_details,$voltage_details,$temp_details,$power_details,$sensor_details);
	
	my $fan_op = `ipmitool sdr | grep FAN | tr -s " "`;
	my @fan_info = split(/\n/,$fan_op);
	my $count =0;
	
	foreach my $fan_key (@fan_info)
	{
		my @fan_data = split(/\|/,$fan_key);
		$fan_data[0] =~ s/^\s+|\s+$//g;
		$fan_data[1] =~ s/^\s+|\s+$//g;
		$fan_data[2] =~ s/^\s+|\s+$//g;
		$fan_details->{$count}->{'name'} = $fan_data[0];
		$fan_details->{$count}->{'unit'} = $fan_data[1];
		$fan_details->{$count}->{'status'} = $fan_data[2];
		$count++;
	}	
	$sensor_details->{'fan'} = $fan_details;
		
	my $voltage_op = `ipmitool sdr | grep -i Volts | tr -s " "`;	
	my @voltage_details = split(/\n/,$voltage_op);
	my $count =0;
	foreach my $voltage_key (@voltage_details)
	{
		my @voltage_data = split(/\|/,$voltage_key);
		$voltage_data[0] =~ s/^\s+|\s+$//g;
		$voltage_data[1] =~ s/^\s+|\s+$//g;
		$voltage_data[2] =~ s/^\s+|\s+$//g;
		$voltage_details->{$count}->{'name'} = $voltage_data[0];
		$voltage_details->{$count}->{'unit'} = $voltage_data[1];
		$voltage_details->{$count}->{'status'} = $voltage_data[2];
		$count++;
	}
	$sensor_details->{'voltage'} = $voltage_details;

	my $temp_op = `ipmitool sdr | grep -i TEMP | tr -s " "`;
	my @temp_details = split(/\n/,$temp_op);
	my $count =0;
	foreach my $temp_key (@temp_details)
	{
		my @temp_data = split(/\|/,$temp_key);
		$temp_data[0] =~ s/^\s+|\s+$//g;
		$temp_data[1] =~ s/^\s+|\s+$//g;
		$temp_data[2] =~ s/^\s+|\s+$//g;
		$temp_details->{$count}->{'name'} = $temp_data[0];
		$temp_details->{$count}->{'unit'} = $temp_data[1];
		$temp_details->{$count}->{'status'} = $temp_data[2];
		$count++;
	}
	$sensor_details->{'temperature'} = $temp_details;
	
	my $power_op = `ipmitool sensor | /usr/xpg4/bin/grep -iE 'PS[0-9]+' | tr -s " " | awk \'{print \$1,\$2,\$3,\$4}\'`;
	
	my @power_details = split(/\n/,$power_op);
	my $count =0;
	foreach my $power_key (@power_details)
	{
		my $power_supply_status;
		my @power_data = split(/\|/,$power_key);
		$power_data[0] =~ s/^\s+|\s+$//g;
		$power_data[1] =~ s/^\s+|\s+$//g;
		
		if($power_data[1] eq "0x1")
		{
			$power_supply_status = "nc";
		}
		elsif($power_data[1] eq "0xb")
		{
			$power_supply_status ="cr";
		}
		else
		{
			$power_supply_status = "ok";
		}
		
		$power_details->{$count}->{'name'} =$power_data[0];
		$power_details->{$count}->{'unit'} = $power_data[1];
		$power_details->{$count}->{'status'} = $power_supply_status;
		$count++;		
	}
	$sensor_details->{'power_supply'} = $power_details;

	if($sensor_details)
	{
		$sensor_details = serialize($sensor_details);
	}
	return $sensor_details;
}

# 
# Function Name: getCapacityInBytes
#
# Description: Used to convert the given value into bytes
#
# Parameters: None 
#
# Return Value: Returns the given value in bytes.
#
sub getCapacityInBytes
{
	my($self,$capacity) = @_;
	
	if(substr($capacity, length($capacity)-1, 1) eq 'T') 
	{ 
		$capacity = substr($capacity,0, -1);
		$capacity = ($capacity*1024*1024*1024*1024); 
	}
	elsif(substr($capacity, length($capacity)-1, 1) eq 'G') 
	{ 
		$capacity = substr($capacity,0, -1);
		$capacity = ($capacity*1024*1024*1024); 
	}
	elsif(substr($capacity, length($capacity)-1, 1) eq 'M') 
	{ 
		$capacity = substr($capacity,0, -1);
		$capacity = ($capacity*1024*1024); 
	}
	elsif(substr($capacity, length($capacity)-1, 1) eq 'K') 
	{ 
		$capacity = substr($capacity,0, -1);
		$capacity = ($capacity*1024); 
	}
	return $capacity;
}

sub getAllPoolThroughput
{
	my $self = shift;
	my($pool_usage, $pool_name, $pool_reads, $pool_writes, $all_pools_info,$read_iops, $write_iops,$pool_iops_info);
	my(@pool_through_iops_value ,@all_pools, @pool_iops_value);
	
	my $numentries = `zpool iostat | awk \'NR>3\' | grep -v \'-\' | grep -v \'rpool\' | wc -l`;
	
	$logging_obj->log("EXCEPTION","getAllPoolThroughput numentries - $numentries \n");
	my $numiters = 6;
	my $numreqdlines = ($numiters - 1)*$numentries;
	
	$pool_usage = `zpool iostat -T d 1 6 | awk \'NR>4\' | grep -v \'rpool\' | grep -v \'-\' |/usr/xpg4/bin/grep -vE "[0-9]{2}{3}:" | tail -"$numreqdlines"| awk '{print \$1,\$4,\$5,\$6,\$7}\'`;
	my @pool_through_output = split(/\n/,$pool_usage);
	
	foreach my $pool_throughput(@pool_through_output)
	{
		@pool_through_iops_value = split(/ /,$pool_throughput);
		$pool_name = $pool_through_iops_value[0];
		push(@all_pools,$pool_name);
		$pool_reads = $pool_through_iops_value[1];
		$pool_reads = $self->getCapacityInBytes($pool_reads);
		$pool_writes  =	$pool_through_iops_value[2];
		chomp($pool_writes);
		$pool_writes = $self->getCapacityInBytes($pool_writes);
		$read_iops = $pool_through_iops_value[3];
		$read_iops = $self->getCapacityInBytes($read_iops);
		$write_iops = $pool_through_iops_value[4];
		$write_iops = $self->getCapacityInBytes($write_iops);
		
		$logging_obj->log("EXCEPTION","getAllPoolThroughput Final values:: pool_reads - $pool_reads, pool_writes - $pool_writes, read_iops - $read_iops, write_iops - $write_iops \n");
		
		if((grep  $_ eq $pool_name,@all_pools ))
		{
			$pool_reads = $all_pools_info->{$pool_name}->{'pool_reads'} + $pool_reads;
			$pool_writes = $all_pools_info->{$pool_name}->{'pool_writes'} + $pool_writes;
			
			$read_iops = $pool_iops_info->{$pool_name}->{'read_iops'} + $read_iops;
			$write_iops = $pool_iops_info->{$pool_name}->{'write_iops'} + $write_iops;
		}
		
		$all_pools_info->{$pool_name}->{'pool_reads'} = $pool_reads;
		$all_pools_info->{$pool_name}->{'pool_writes'} = $pool_writes;
		$pool_iops_info->{$pool_name}->{'read_iops'} = $read_iops;
		$pool_iops_info->{$pool_name}->{'write_iops'} = $write_iops;
	}
	$logging_obj->log("EXCEPTION","getAllPoolThroughput values:: ".Dumper($all_pools_info)." \n");
	$logging_obj->log("EXCEPTION","getAllPoolThroughput values:: ".Dumper($pool_iops_info)." \n");
	
	foreach my $pool (keys %{$all_pools_info})
	{
		if($all_pools_info->{$pool}->{'pool_reads'})
		{
			$all_pools_info->{$pool}->{'pool_reads'} = $all_pools_info->{$pool}->{'pool_reads'}/($numiters-1);
		}
		if($all_pools_info->{$pool}->{'pool_writes'})
		{
			$all_pools_info->{$pool}->{'pool_writes'} = $all_pools_info->{$pool}->{'pool_writes'}/($numiters-1);
		}
	}
	
	foreach my $pool (keys %{$pool_iops_info})
	{
		if($pool_iops_info->{$pool}->{'read_iops'})
		{
			$pool_iops_info->{$pool}->{'read_iops'} = $pool_iops_info->{$pool}->{'read_iops'}/($numiters-1);
		}
		if($pool_iops_info->{$pool}->{'write_iops'})
		{
			$pool_iops_info->{$pool}->{'write_iops'} = $pool_iops_info->{$pool}->{'write_iops'}/($numiters-1);
		}
	}
	return($all_pools_info,$pool_iops_info);
}

sub getAllNicStatus
{
	my $self = shift;
	my($nic_name,$nic_state,$nic_status_info,$nic_rbytes,$nic_obytes,$nic_ro_data);	
	
	my $nic_state = `dladm show-link | awk \'NR>1\' | awk \'{print \$1,\$4}\'`;
	my @nic_info = split(/\n/,$nic_state);
	
	foreach my $nics (@nic_info)
	{
		my @nic_details = split(/ /,$nics);
		$nic_name = $nic_details[0];
		$nic_state = $nic_details[1];
		chomp($nic_state);
		
		$nic_status_info->{$nic_name} = $nic_state;		
	}
	
	#my $nic_op = `dlstat | awk \'NR>1\'| awk \'{ print \$1,\$4,\$5 }\'`;
	#my @nic_ro = split(/\n/,$nic_op);
	
	my $dlstat_opfile = "/home/svsystems/var/dlstat_output.log";
	my $nic_op = `dlstat -i 5 | awk \'NR>1 { print \$1,\$3,\$5 }\' > $dlstat_opfile &`;
	sleep(7);
	my $ouput = `pkill -9 -x dlstat`;
	open(PH,$dlstat_opfile);
	my @nic_ro = <PH>;
	close($dlstat_opfile);
	unlink($dlstat_opfile);
	my $arr_count = scalar @nic_ro;
	for(my $i = 0; $i < ($arr_count / 2); $i++)
	{
		shift(@nic_ro);
	}
	
	foreach my $nics (@nic_ro)
	{
		my @nic_bytes = split(/ /,$nics);
		$nic_name = $nic_bytes[0];
		$nic_rbytes = $nic_bytes[1];
		chomp($nic_rbytes);
		if($nic_rbytes != 0)
		{
			$nic_rbytes = $self->getCapacityInBytes($nic_rbytes);
		}	
		$nic_obytes = $nic_bytes[2];
		chomp($nic_obytes);
		if($nic_obytes != 0)
		{
			$nic_obytes = $self->getCapacityInBytes($nic_obytes);
		}	
		
		$nic_ro_data->{$nic_name}->{'rbytes'} = $nic_rbytes/5;
		$nic_ro_data->{$nic_name}->{'obytes'} = $nic_obytes/5;
	}
	return($nic_status_info,$nic_ro_data);
}

sub getDiskIOPS
{	
	my $self = shift;
	my ($all_disk_info,$disk_usage,$read_iops,$write_iops,$read_iops_all,$write_iops_all,$disk_throughput,$disk_name);
	my (@disk_iops_value,@all_disks);
	my $numiters = 6;
	my $numentries = `iostat -xn | grep \'c[0-9]t*\' | wc -l`;
	
	$logging_obj->log("EXCEPTION","request_xml ::getDiskActivity numentries - $numentries \n");
	
	my $numreqdlines = ($numiters - 1)*$numentries;
	my $disk_usage = `iostat -xn 1 6 | grep \'c[0-9]t*\'|tail -"$numreqdlines"|awk \'{print \$1,\$2,\$11}\'`;
	my @disk_iops_output = split(/\n/,$disk_usage);
	
	foreach $disk_throughput(@disk_iops_output)
	{
		@disk_iops_value = split(/ /,$disk_throughput);
		$disk_name =	$disk_iops_value[2];
		push(@all_disks,$disk_name);
		$read_iops =	$disk_iops_value[0];
		$write_iops  =	$disk_iops_value[1];
		
		#$logging_obj->log("EXCEPTION","getDiskActivity :: disk_name - $disk_name, read_iops - $read_iops, write_iops - $write_iops \n");
		
		if((grep  $_ eq $disk_name,@all_disks ))
		{
			$read_iops = $all_disk_info->{$disk_name}->{'read_iops'} + $read_iops;
			$write_iops = $all_disk_info->{$disk_name}->{'write_iops'} + $write_iops;
		}
		
		$all_disk_info->{$disk_name}->{'read_iops'} = $read_iops;
		$all_disk_info->{$disk_name}->{'write_iops'} = $write_iops;
	}
	
	$logging_obj->log("EXCEPTION","getDiskIOPS :: ".Dumper($all_disk_info)." \n");
	
	foreach my $disk (keys %{$all_disk_info})
	{
		if($all_disk_info->{$disk}->{'read_iops'})
		{
			$all_disk_info->{$disk}->{'read_iops'} = $all_disk_info->{$disk}->{'read_iops'}/($numiters-1);
		}
		if($all_disk_info->{$disk}->{'write_iops'})
		{
			$all_disk_info->{$disk}->{'write_iops'} = $all_disk_info->{$disk}->{'write_iops'}/($numiters-1);
		}
	}

	$logging_obj->log("EXCEPTION","getDiskIOPS Final values:: ".Dumper($all_disk_info)." \n");
	
	
	return $all_disk_info;
}

sub getFmadmAlerts
{
	my $self = shift;
	my $message;
	my $fmadm_op = `fmadm faulty -s | awk \'NR>3\'  | awk \'!/SENSOR-/\' | awk \'{print \$4}\'`;

	my @fmadm_data = split(/\n/,$fmadm_op);
	my $count =0;
	
	foreach my $uid (@fmadm_data)
	{
		my $fm_op = `fmdump -v -u $uid | /usr/xpg4/bin/grep -e Location -e FRU: | awk \'{print \$2,\$3}\'`;
		my @details = split(/\n/,$fm_op);
		$message->{$count}->{'message'} = "The faulty FRU ".$details[1]." requires attention :".$details[0];
		$message->{$count}->{'type'} = $details[1];
		$count++;
	}
	if($message)
	{
		$message = serialize($message);
	}
	return $message;
}

# 
# Function Name: getDiskUsage
#
# Description: Used to get the Disk Usage
#
# Parameters: None 
#
# Return Value: Returns the disk throughput, readiops & writeiops for each disk
# 
sub getDiskUsage
{
	my $self = shift;
	my($disk_properties,$disk_usage_details,$read_iops,$write_iops,$disk_throughput,$disk_status,$all_disk_iops);
	my(@disk_through_output,@disk_throughput_value,@disk_info,@disk_status_list);
	my $count = 0;
	
	my $disks = `iostat -En|grep 'c[0-9]t*' | awk \'{print \$1}\'`;
	my @disk_names_list = split(/\n/,$disks);
	$all_disk_iops = $self->getDiskIOPS();
	
	foreach my $disk_name (@disk_names_list)
	{
		my($disk_reads_all, $disk_writes_all);		
		
		$read_iops = $all_disk_iops->{$disk_name}->{'read_iops'};
		$write_iops = $all_disk_iops->{$disk_name}->{'write_iops'};
		$disk_reads_all = $all_disk_info->{$disk_name}->{'disk_reads'};	
		$disk_writes_all = $all_disk_info->{$disk_name}->{'disk_writes'};		
		$disk_throughput = $disk_reads_all + $disk_writes_all;
		$disk_throughput = sprintf("%.3f", $disk_throughput);
		
		$disk_usage_details->{$count}->{'disk_name'} =  $disk_name;
		$disk_usage_details->{$count}->{'read_iops'} =  $read_iops;
		$disk_usage_details->{$count}->{'write_iops'} =  $write_iops;
		$disk_usage_details->{$count}->{'disk_throughput'} =  $disk_throughput;		
		$count++;
	}
	$logging_obj->log("EXCEPTION","getDiskUsage :: all_disk_info - ".Dumper($all_disk_info)." \n");
	if($disk_usage_details)
	{
		$disk_usage_details = serialize($disk_usage_details);
	}
	return $disk_usage_details;
}

# 
# Function Name: getSNService
#
# Description: Used to fetch sms service related info
#
# Parameters: None 
#
# Return Value: Returns the sms service status for the storage node
#
sub getSNService
{
	my $self = shift;
	
	my $service_op = `svcs -a | grep -i scoutsms | awk \'{print \$1}\'`;
	$service_op =~ s/^\s+|\s+$//g;
	my $service_status;
	$service_status->{'service_status'} = $service_op;
	$service_status = serialize($service_status);
	return $service_status;
}

1;
