#=================================================================
# FILENAME
#   Monitor.pm
#
# DESCRIPTION
#    This perl module is used by the storage monitor thread
#
#=================================================================
package StorageMonitor;

use lib ("/var/apache2/2.2/htdocs");
use lib ("/home/svsystems/pm");
use strict;
use Utilities;
use Common::Constants;
use Data::Dumper;
use PHP::Serialization qw(serialize unserialize);
use Common::Log;
#use LWP::UserAgent;
use SystemMonitor::Monitor;

my $logging_obj = new Common::Log();
my $STORAGE_CONF_VARS = Utilities::get_ametheyst_conf_parameters();

our $INMAGE_VERSION_FILE              = "etc/version";
our $PATCH_VERSION_FILE               = "patch.log";
our $BASE_DIR 					  = $STORAGE_CONF_VARS->{'INSTALLATION_DIR'};
our $host_guid                        = $STORAGE_CONF_VARS->{'HOST_GUID'};
our $smatctl_path					  = "/home/svsystems/bin/";
# 
# Function Name: new
#
# Description: Constructor for the Update class.
#
# Parameters: None 
#
# Return Value:None
#  	
sub new
{
	my $class = shift;
	my $self = {};
	$self->{'mon_obj'} = new SystemMonitor::Monitor();
    bless ($self, $class);
}
 
# 
# Function Name: nicDetails
#
# Description: Used to get the nic details
#
# Parameters: None 
#
# Return Value: Returns the nic properties per each nic.
#
sub nicDetails
{
	my $self = shift;
	my $nic_list;
	my @nic_array;
	my $nic_name;
	$nic_list = `dladm show-link | awk \'NR>1\' | awk \'{ print \$1 }\'`;
	my @nic_names = split(/\n/,$nic_list);
	foreach $nic_name(@nic_names) 
	{
		my %nic_details = $self->getNicProperties($nic_name);
		push(@nic_array,\%nic_details);
	}
	return @nic_array;
}

# 
# Function Name: getNicProperties
#
# Description: Used to get the nic properties
#
# Parameters: None 
#
# Return Value: Returns the nic name ,ip address,nic spped,mtu,packet loss,IPKTS,RBYTES,OPKTS,OBYTES
#
sub getNicProperties
{
	my ($self,$nic_name) = @_;
	chomp($nic_name);
	my ($nic_speed,$ip_address);
	my (@io_output,@bytes_output);
	my %nic_details;
	
	$nic_speed = `dladm show-phys $nic_name | awk \'NR>1\' | awk \'{print \$4}\'`;
	chomp($nic_speed);
	$ip_address = '';
	my @ip_val =  `ipadm show-addr | grep $nic_name | awk \'{print \$2,\$4}\'`;
	
	foreach my $nic_det(@ip_val)
	{
		my @nics_op = split(/ /,$nic_det);

		chomp($nics_op[0]);
		if($nics_op[0] eq 'static')
		{
			if($ip_address ne '')
			{
					$ip_address .= ',';
			}
			$ip_address .= $nics_op[1];
		}
	}
	my @ip_address_list = split(/\//,$ip_address);
	$ip_address = $ip_address_list[0];
	$ip_address =~ s/\n/,/g;
	%nic_details = ('nic-name'    => $nic_name,
					'ip-address'  => $ip_address,
					'nic-speed'   => $nic_speed
					);
	return %nic_details;		
}

# 
# Function Name: spareDiskDetail
#
# Description: Used to get the spare disk details
#
# Parameters: None 
#
# Return Value: Returns the spare disk name,disk type and disk size.
#

sub spareDiskDetail
{
	my ($self,$pool_name) = @_;	
	my $spare_disk_list = `zpool status |awk \'/spares/{x=NR+2;next}(NR<=x){print}\'|awk \'{print \$1}\'`;
	my @spare_disk = split(/\n/,$spare_disk_list);
	return @spare_disk;
}

# 
# Function Name: zilDiskDetail
#
# Description: Used to get the cache disk details
#
# Parameters: None 
#
# Return Value: Returns the zil disk name,disk type and disk size.
#

sub logDiskDetail
{
	my $self = shift;
	my @zil_list;
	my @zpool_name = $self->fetchZpoolList();
	foreach my $zpool_name(@zpool_name)
	{
		my $log_disk_list = `zpool status $zpool_name|awk \'{print \$1} \'`;

		my @log_disk = split(/\n/,$log_disk_list);
		my $a = 0;
		foreach my $log_disk_name (@log_disk) 
		{
			if(($a))
			{
				if(($log_disk_name =~ /^c[0-9]/))
				{
					push(@zil_list,$log_disk_name);
				}
				else
				{
					if($log_disk_name !~ /^mirror/)
					{
						$a = 0;
					}
				}
			}
			
			if($log_disk_name eq 'logs')
			{
				$a = 1;
			}
		}
	}
	return @zil_list;
}

# 
# Function Name: cacheDiskDetail
#
# Description: Used to get the cache disk details
#
# Parameters: None 
#
# Return Value: Returns the cache disk name,disk type and disk size.
#

sub cacheDiskDetail
{
	my $self = shift;
	my @cache_list;
	my @zpool_name = $self->fetchZpoolList();
	foreach my $zpool_name(@zpool_name)
	{
		my $cache_disk_list = `zpool status $zpool_name|awk \'{print \$1} \'`;

		my @cache_disk = split(/\n/,$cache_disk_list);
		my $a = 0;
		foreach my $cache_disk_name (@cache_disk) 
		{
			if(($a))
			{
				if(($cache_disk_name =~ /^c[0-9]/))
				{
					push(@cache_list,$cache_disk_name);
				}
				else
				{
				   $a = 0;
				}
			}
			
			if($cache_disk_name eq 'cache')
			{
				$a = 1;
			}
		}
	}
	return @cache_list;
}

# 
# Function Name: zpoolDetail
#
# Description: Used to get the pool details
#
# Parameters: None 
#
# Return Value: Returns the pool information
#

sub fetchZpoolList
{
	my $self = shift;
	my $zpool_list;	
	$zpool_list =	`zpool list | awk \'NR>1\' | awk \'{print \$1 }\'`;
	my @zpool_name = split(/\n/,$zpool_list);
	return @zpool_name;
}

# 
# Function Name: getVersion
#
# Description: Used to get the storage node build version
#
# Parameters: None 
#
# Return Value: Returns the storage node build version.
#
sub getVersion
{
	my $self = shift;	
	my $storage_node_version;
	
	eval 
    {
    	open(F, "<$BASE_DIR/$INMAGE_VERSION_FILE");
    };	
    if ($@)
    {
		$logging_obj->log("EXCEPTION","Couldn't open $INMAGE_VERSION_FILE: $@");
		return;
    }
	
	foreach my $line (<F>) 
    {
		chomp $line;
		my @data = split(/=/,$line);
		if($data[0] eq 'BUILD_TAG')
		{
			$storage_node_version = $data[1];
			last;
		}	
	}
	close( F );
    return $storage_node_version;
}

# 
# Function Name: get_storage_node_patch_detals
#
# Description: Used to fetch applied patch details
#
# Parameters: None 
#
# Return Value: Returns the array of patch version and install time for all the applied patches.
#
sub get_storage_node_patch_detals 
{  
	my @storage_patch_details;
	my @storage_patch_details_array;
	eval 
    {
    	open(F, "<$BASE_DIR/$PATCH_VERSION_FILE");
    };	
    if ($@)
    {
		$logging_obj->log("EXCEPTION","Couldn't open $PATCH_VERSION_FILE: $@");
		return;
    }	
	foreach my $line (<F>) 
    {
		my @data = split(/,/,$line);
		my $storage_patch_version = $data[0];
		my $storage_patch_install_time = $data[1];
		
		@storage_patch_details = {
								'storage_patch_version' => $line, 
								'storage_patch_install_time' => $storage_patch_install_time
						  };
		push(@storage_patch_details_array, @storage_patch_details);
	}
	close( F );
	
    return @storage_patch_details_array;
}

# 
# Function Name: getDiskDetails
#
# Description: Used to get all the realted disk properties
#
# Parameters: None 
#
# Return Value: Returns the all the disks related information available on the storage node.
#
sub getDiskDetails
{
	my $self = shift;
	my ($disk_details,$disk_capacity,$smart_ctl_op,$smart_key,$smart_value,$disk_type, $firm_ware_version,$product,$wear_out_indicator,$slot_status,$slot_number,$zpool_info,$enclosure_name);
		
	my @zpool_name = $self->fetchZpoolList();
	my $disks = `iostat -En|grep 'c[0-9]t*' | awk \'{print \$1\}\'`;
	my @disk_names = split(/\n/,$disks);
	my $count = 0;
	my $disk_manufacturer;
	my $zpool_info = $self->getAllDisksOfZpool();
	my @rpool_disks = $self->getAllDisksOfRpool();
	my @cache_disks = $self->cacheDiskDetail();
	my @spare_disks = $self->spareDiskDetail();
	my @log_disks = $self->logDiskDetail();
	
	foreach my $disk_name (@disk_names)
	{		
		my $disk_data;
		my $is_zpool_disk = 0;
		my $usb_op = '';
		$firm_ware_version = '';
		$usb_op = `iostat -Enxi $disk_name | grep Product: | awk \'{print \$4}\'`;
		$usb_op =~ s/^\s+|\s+$//g;
		
		if($usb_op ne 'Memory')
		{
			$disk_details->{$count}->{'zpoolName'} = '';	
			foreach my $zpool_name(@zpool_name)
			{
				my $all_disks = $zpool_info->{$zpool_name};
				my @all_disks = @$all_disks;
				
				if(grep  $_ eq $disk_name, @all_disks)
				{
					$disk_details->{$count}->{'zpoolName'} = $zpool_name;
					$is_zpool_disk = 1;
					last;
				}
			}
			
			if(grep  $_ eq $disk_name,@cache_disks )
			{
				$disk_details->{$count}->{'diskUsage'} = 'cache';				
			}
			elsif(grep  $_ eq $disk_name,@spare_disks )
			{
				$disk_details->{$count}->{'diskUsage'} = 'spare';		
			}
			elsif(grep  $_ eq $disk_name,@log_disks )
			{
				$disk_details->{$count}->{'diskUsage'} = 'log';
			}
			elsif(grep  $_ eq $disk_name,@rpool_disks )
			{
				$disk_details->{$count}->{'diskUsage'} = 'boot';
			}
			else
			{
				if($is_zpool_disk)
				{
					$disk_details->{$count}->{'diskUsage'} = 'data';
				}
				else
				{
					$disk_details->{$count}->{'diskUsage'} = 'disk';
				}
			}
			$disk_data->{'Product'} = $usb_op;
			$disk_data->{'RPM'} = "";
			$disk_data->{'Serial No'} = "";
			$disk_data->{'Manufacturer'} = "";
			
			$smart_ctl_op = `$smatctl_path/smartctl -i /dev/rdsk/$disk_name -d scsi | /usr/xpg4/bin/grep -e \"^User Capacity\" -e \"^Rotation Rate\" -e \"^Serial number\" -e \"^Vendor\"`;
			my @smart_op = split(/\n/,$smart_ctl_op);
			foreach my $smart_ip (@smart_op)
			{
				my @smart_details = split(/:/, $smart_ip);
				$smart_key = $smart_details[0];
				$smart_key =~ s/^\s+|\s+$//g;
				$smart_value = $smart_details[1];
				$smart_value =~ s/^\s+|\s+$//g;
				
				if($smart_key eq "User Capacity")
				{
					my @capacity = split(/\[/, $smart_value);
					$disk_details->{$count}->{'capacity'} = $capacity[0];
				}
				if($smart_key eq "Rotation Rate")
				{
					$disk_data->{'RPM'} = $smart_value;				
				}
				if($smart_key eq "Serial number")
				{
					$disk_data->{'Serial No'} = $smart_value;
				}
				if($smart_key eq "Vendor")
				{
					$disk_data->{'Manufacturer'} = $smart_value;
				}
			}
			
			$disk_type = `$smatctl_path/smartctl -i /dev/rdsk/$disk_name -d scsi | grep -i "Solid State Device" | cut -d":" -f2`;
			if($disk_type)
			{
				$firm_ware_version =`$smatctl_path/smartctl -i /dev/rdsk/$disk_name -d sat | grep -i "^Firmware Version" | cut -d":" -f2`;
				$firm_ware_version =~ s/\s//g;
				if($disk_details->{$count}->{'diskUsage'} eq "log")
				{
					$wear_out_indicator = `$smatctl_path/smartctl -a -d sat /dev/rdsk/$disk_name | grep -i "Media_Wearout_Indicator" | awk \'{print \$4\}\'`;
				}	
				$disk_type = "SSD";
			
				my $manufacturer_list = `$smatctl_path/smartctl -i /dev/rdsk/$disk_name -d sat| grep -i "Device Model" | awk \'{print \$3,\$4}\'`;

				my @ssd_disk = split(/\s/,$manufacturer_list);
				
				if ($ssd_disk[0] eq 'INTEL')
				{
					$disk_manufacturer = $ssd_disk[0];
					$disk_data->{'Product'} = $ssd_disk[1];
				}
				else
				{
					my @manufacturer_type = split(/_/,$ssd_disk[0]);
					if ($manufacturer_type[0] eq 'Crucial')
					{
						$disk_manufacturer = "MICRON";
					}
					$disk_data->{'Product'} = $ssd_disk[0];
				}
				$disk_data->{'Manufacturer'} = $disk_manufacturer;
			}
			else
			{
				$firm_ware_version = `diskinfo -o cf | grep $disk_name | awk \'{print \$2}\'`;
				$firm_ware_version =~ s/\s//g;
				$wear_out_indicator="";			
				$disk_type = "SAS";
			}			
			
			$slot_number = "";
			$enclosure_name = "";
			my $rdsk_flag = 0;
			my $smartctl_sas_rdsk_check = `$smatctl_path/smartctl -i /dev/rdsk/$disk_name -d scsi`;
			if($? == 0)
			{
				$rdsk_flag = 1;
			}
			else
			{
				my $smartctl_ssd_rdsk_check = `$smatctl_path/smartctl -i /dev/rdsk/$disk_name -d sat`;
				if($? == 0)
				{
					$rdsk_flag = 1;
				}
			}
			
			if($rdsk_flag)
			{
				if($disk_type eq "SAS")
				{
					my $enclosure_info = `diskinfo -o cRD | grep $disk_name | awk \'{print \$2,\$3}\'`;
					my @enclosure_data = split(/ /,$enclosure_info);		
					$slot_number = $enclosure_data[0];
					$slot_number =~ s/^\s+|\s+$//g;
					
					my $diskdev_chasis_path = $enclosure_data[1];
					chomp($diskdev_chasis_path);
					my @diskdev_chasis_arr = split(/\./,$diskdev_chasis_path);
					my @diskdev_enclosure_arr = split(/\//,$diskdev_chasis_arr[1]);
					$enclosure_name = $diskdev_enclosure_arr[0];
				}
				else
				{				
					my $disk_location_info = `/home/svsystems/scripts/inm_disk_locate.sh $disk_name`;
					my @disk_location_arr = split(/ /,$disk_location_info);
					$enclosure_name = $disk_location_arr[0];		
					my $slot_no = $disk_location_arr[1];
					# $disk_location_arr[2] is disk name
					
					if(int($slot_no/10) == 0)
					{
						$slot_number = "Slot_0".$slot_no;
					}
					else
					{
						$slot_number = "Slot_".$slot_no;
					}
				}
			}
			else
			{
				if($disk_type eq "SAS")
				{
					my $enclosure_info = `diskinfo -o cRD | grep $disk_name | awk \'{print \$2,\$3}\'`;
					my @enclosure_data = split(/ /,$enclosure_info);		
					$slot_number = $enclosure_data[0];
					$slot_number =~ s/^\s+|\s+$//g;
					
					my $diskdev_chasis_path = $enclosure_data[1];
					chomp($diskdev_chasis_path);
					my @diskdev_chasis_arr = split(/\./,$diskdev_chasis_path);
					my @diskdev_enclosure_arr = split(/\//,$diskdev_chasis_arr[1]);
					$enclosure_name = $diskdev_enclosure_arr[0];
				}			
			}
			
			$disk_details->{$count}->{'diskName'} = $disk_name;
			$disk_details->{$count}->{'slotNo'} = $slot_number;
			$disk_details->{$count}->{'enclosureName'} = $enclosure_name;
			$disk_data->{'Firmware version'} = $firm_ware_version;
			$disk_data->{'Wearout Indicator'} = $wear_out_indicator;
			$disk_details->{$count}->{'diskType'} = $disk_type;
			$disk_details->{$count}->{'diskProperties'} = serialize($disk_data);										
			$count++;
		}	
	}
	if($disk_details)
	{
		$disk_details = serialize($disk_details);
	}	
	return $disk_details;
}

sub getAllDisksOfZpool
{
	my $self = shift;
	my($zpool_name, $all_disks);
	my @disk_array;
	
	my @zpool_names = $self->fetchZpoolList();
	
	foreach my $zpool_name (@zpool_names)
	{
		my $disk_op = `zpool status $zpool_name | awk \'NR>6\' | awk \'!/mirror|spare|log|cache|errors|--|\^\$/\' | awk \'{print \$1}\'`;
		my @disks = split(/\n/,$disk_op);
		
		foreach my $disk_ip (@disks)
		{
			if(grep  $_ eq $disk_ip,@zpool_names)
			{
				$zpool_name = $disk_ip;
			}
			else
			{
				push(@disk_array,$disk_ip);
				$all_disks->{$zpool_name} = [@disk_array];
			}	
		}
	}
	return $all_disks;
}
# 
# Function Name: getZpoolDetails
#
# Description: Used to get all the related pool information
#
# Parameters: None 
#
# Return Value: Returns the all the pools related information available on the storage node.
#
sub getZpoolDetails
{
	my $self = shift;
	my($pool_result,$used_capacity,$free_capacity,$read_iops,$write_iops,$read_bw,$write_bw,$total_capacity,$zpool_details,$compression_ratio,$status,$dedup_enabled,$encryption,$ded_up_effeciency);
	my $count = 0;
	
	my @zpool_name = $self->fetchZpoolList();	
	my $zpool_list = $self->getZpoolProperties();
	my $zpool_mem_data = $self->getZpoolList();
	
	foreach my $zpool_name (@zpool_name) 
	{
		$zpool_details->{$count}->{'poolName'}= $zpool_name;
		$used_capacity = $zpool_mem_data->{$zpool_name}->{'allocated'};
		$free_capacity = $zpool_mem_data->{$zpool_name}->{'available'};
				
		$zpool_details->{$count}->{'allocated'}= $used_capacity;
		$zpool_details->{$count}->{'available'}= $free_capacity;		
		
		$dedup_enabled = $zpool_list->{$zpool_name}->{'dedup'};
		if($dedup_enabled eq "on")
		{
			$zpool_details->{$count}->{'dedupEnabled'} = "enabled";
			$ded_up_effeciency = `zpool list $zpool_name | awk 'NR>1' | awk \'{print \$6}\'`;
			$ded_up_effeciency =~ s/^\s+|\s+$//g;
			$zpool_details->{$count}->{'dedupEfficiency'}= $ded_up_effeciency;
		}
		else
		{
			$zpool_details->{$count}->{'dedupEnabled'} = "disabled";
			$zpool_details->{$count}->{'dedupEfficiency'}= $ded_up_effeciency;
		}
		
		$encryption = $zpool_list->{$zpool_name}->{'encryption'};
		if($encryption eq "on")
		{
			$zpool_details->{$count}->{'encryption'} = "enabled";
		}
		else
		{
			$zpool_details->{$count}->{'encryption'} = "disabled";
		}
		
		$compression_ratio = $zpool_list->{$zpool_name}->{'compressratio'};
		$compression_ratio =~ s/^\s+|\s+$//g;
		$zpool_details->{$count}->{'compressionRatio'} = $compression_ratio;
		$count++;
	}
	if($zpool_details)
	{
		$zpool_details = serialize($zpool_details);
	}
	
	return $zpool_details;
}

sub getZpoolList
{
	my $self = shift;
	my($zpool_name,$zpool_mem_op,$all_zpool_mem_data,$zpool_allocated_mem,$zpool_available_mem);	

	$zpool_mem_op = `zfs list | awk \'NR>1\'| awk \'{print \$1, \$2, \$3}\'`;
	my @zpool_mem_list = split(/\n/,$zpool_mem_op);	
	
	foreach my $zpool_mem_data (@zpool_mem_list) 
	{
		my @zpool_mem_data = split(/ /,$zpool_mem_data);
		
		$zpool_name = $zpool_mem_data[0];
		$zpool_allocated_mem = $zpool_mem_data[1];
		$zpool_allocated_mem = $self->getCapacityInBytes($zpool_allocated_mem);
		$zpool_available_mem = $zpool_mem_data[2];
		$zpool_available_mem = $self->getCapacityInBytes($zpool_available_mem);
		chomp($zpool_available_mem);
		
		$all_zpool_mem_data->{$zpool_name}->{'allocated'} = $zpool_allocated_mem;
		$all_zpool_mem_data->{$zpool_name}->{'available'} = $zpool_available_mem;
	}	
	return $all_zpool_mem_data;
}

sub getZpoolProperties
{
	my $self = shift;
	my($zpool_name,$zpool_option,$zpool_value,$all_zpool_data);
	
	my $zpool_list_op = `zfs get all | /usr/xpg4/bin/grep -e volsize -e checksum -e compressratio -e dedup -e encryption | awk \'{print \$1,\$2,\$3}\'`;
	my @zpool_list = split(/\n/,$zpool_list_op);
	
	foreach my $zpool_op (@zpool_list) 
	{
		my @zpool_list_data = split(/ /,$zpool_op);
		
		$zpool_name = $zpool_list_data[0];
		$zpool_option = $zpool_list_data[1];
		$zpool_value = $zpool_list_data[2];
		$zpool_value =~ s/^\s+|\s+$//g;
		
		$all_zpool_data->{$zpool_name}->{$zpool_option} = $zpool_value;
	}
	return $all_zpool_data;
}

# 
# Function Name: getVolumeDetails
#
# Description: Used to get all the related volumes information
#
# Parameters: None 
#
# Return Value: Returns the all the volumes related information available on the storage node.
#
sub getVolumeDetails
{
	my $self = shift;	
	my($volume_details,$zpool_name,$vol_capacity,$checksum,$compression_ratio,$dedup_enabled,$encryption,$free_space,$ded_up_effeciency,$vol_name);
	my $count =0;
	my @zpool_name = $self->fetchZpoolList();
	
	my $vols = `zfs list | awk \'NR>1\'| awk \'{print \$1, \$3}\'`;
	my @vol_names = split(/\n/,$vols);
	
	my $vol_properties = $self->getZpoolProperties();

	foreach my $vols_input (@vol_names)
	{		
		my @vols_list = split(/ /,$vols_input);
		$vol_name = $vols_list[0];
		$vol_name =~ s/^\s+|\s+$//g;
		
		if((grep  $_ eq $vol_name,@zpool_name ))
		{
			$zpool_name = $vol_name;			
		}
		elsif($zpool_name ne "rpool")
		{
			$volume_details->{$count}->{'poolName'}	= $zpool_name;
			$volume_details->{$count}->{'VolumeName'} = $vol_name;
			
			$free_space = $vols_list[1];
			$free_space =~ s/^\s+|\s+$//g;
			$free_space = $self->getCapacityInBytes($free_space);
			if($free_space == '-')
			{
				$free_space = 0;
			}
			else
			{
				$volume_details->{$count}->{'freeSpace'}= $free_space;
			}
		
			$vol_capacity = $vol_properties->{$vol_name}->{'volsize'};
			if($vol_capacity == '-')
			{
				$vol_capacity = 0;
			}
			else
			{
				$vol_capacity = $self->getCapacityInBytes($vol_capacity);
			}	
			
			$volume_details->{$count}->{'capacity'}= $vol_capacity;			
			$volume_details->{$count}->{'Checksum'} = $vol_properties->{$vol_name}->{'checksum'};			
			$volume_details->{$count}->{'compressionRatio'} =  $vol_properties->{$vol_name}->{'compressratio'};
			
			$dedup_enabled = $vol_properties->{$vol_name}->{'dedup'};
			if($dedup_enabled eq 'on')
			{
				$volume_details->{$count}->{'dedupEnabled'}= 'enabled';
				$ded_up_effeciency = `zpool list $zpool_name | awk 'NR>1' | awk \'{print \$6}\'`;
				$ded_up_effeciency =~ s/^\s+|\s+$//g;
				$volume_details->{$count}->{'dedupEfficiency'}= $ded_up_effeciency;
			}
			else			
			{
				$volume_details->{$count}->{'dedupEnabled'}= 'disabled';
				$volume_details->{$count}->{'dedupEfficiency'}= '';
			}
			
			$encryption = $vol_properties->{$vol_name}->{'encryption'};
			if($encryption eq 'on')
			{
				$volume_details->{$count}->{'encryption'}= 'enabled';				
			}
			else			
			{
				$volume_details->{$count}->{'encryption'}= 'disabled';
			}
			$count++;
		}		
	}
	if($volume_details)
	{
		$volume_details = serialize($volume_details);
	}	
	return $volume_details;
}

# 
# Function Name: getLUNDetails
#
# Description: Used to get all the related Luns information
#
# Parameters: None 
#
# Return Value: Returns the all the LUNs related information available on the storage node.
#
sub getLUNDetails
{
	my $self = shift;
	my ($lun_details,$lun_capacity,$lun_status,$lun_write_cache,$lun_block_size,$lun_data_file,$volume_name,$lun_read_only,$export_data_set);
	my $count = 0;
	
	my $luns = `stmfadm list-lu | cut -d":" -f2`;
	my @storage_luns = split(/\n/,$luns);
	
	my $lun_properties = $self->getLunProperties();
	
	foreach my $lun_name (@storage_luns)
	{
		$lun_name =~ s/^\s+|\s+$//g;
		
		$volume_name = `sbdadm list-lu | grep -i $lun_name | awk \'{print \$3}\'`;
		$volume_name =~ s/^\s+|\s+$//g;
		
		$lun_details->{$count}->{'volName'}	= $volume_name;		
		$lun_details->{$count}->{'lunName'}	= $lun_name;		
		$lun_details->{$count}->{'capacity'} = $lun_properties->{$lun_name}->{'Size'};
		$lun_details->{$count}->{'writeCache'} = $lun_properties->{$lun_name}->{'Write Cache Mode Select'};
		$lun_details->{$count}->{'blockSize'} = $lun_properties->{$lun_name}->{'Block Size'};
		$lun_details->{$count}->{'dataFile'} = $lun_properties->{$lun_name}->{'Data File'};		
		$lun_details->{$count}->{'readOnly'} = $lun_properties->{$lun_name}->{'Writeback Cache'};
		
		$export_data_set = `stmfadm list-view -l $lun_name`;
		$lun_details->{$count}->{'exportType'} = 'false';
		if($export_data_set)
		{
			$lun_details->{$count}->{'exportType'} = 'true';
		}
		
		$count++;
	}
	if($lun_details)
	{
		$lun_details = serialize($lun_details);	
	}
	
	return $lun_details;	
}

sub getLunProperties
{
	my $self = shift;
	my($lun_info,$input_key,$input_value,$lun_name);
	
	my $data = `stmfadm list-lu -v | /usr/xpg4/bin/grep -e Size -e \"Block Size\" -e \"Write Cache Mode Select\" -e \"Data File\" -e \"Writeback Cache\" -e LU`;
	my @data_list = split(/\n/,$data);
	
	foreach my $input (@data_list)
	{
		my @input = split(/:/,$input);
		$input_key =$input[0];
		$input_key =~s/^\s+|\s+$//g;
		$input_value = $input[1];
		$input_value =~s/^\s+|\s+$//g;
		if($input_key =~ m/LU Name/)
		{
			$lun_name = $input_value;
		}
		else
		{
			$lun_info->{$lun_name}->{$input_key} = $input_value;
		}
	}
	return $lun_info;
}

# 
# Function Name: getHostInfo
#
# Description: Used to get all the host related information
#
# Parameters: None 
#
# Return Value: Returns the host related information(hostname,build version, patch version, patch install time).
#
sub getHostInfo
{
	my $self = shift;
	my($storage_patch_version, $storage_patch_install_time,$host_data,$model_no);
	
	my $host = `hostname`;
	chomp $host;
	my $hostname =  $host;
		
	my $ip_address = $STORAGE_CONF_VARS->{"STORAGE_IP"};
	my $storage_node_version = $self->getVersion();
	my @storage_node_patch_version = $self->get_storage_node_patch_detals();	
	
	for(my $i = 0 ; $i < scalar(@storage_node_patch_version) ; $i++ ) 
	{
		if($i== 0)
		{
			$storage_patch_version = $storage_node_patch_version[$i]{'storage_patch_version'};
			$storage_patch_install_time = $storage_node_patch_version[$i]{'storage_patch_install_time'};
		}
		else
		{
			$storage_patch_version = $storage_patch_version."|".$storage_node_patch_version[$i]{'storage_patch_version'};
			$storage_patch_install_time = $storage_patch_install_time."|".$storage_node_patch_version[$i]{'storage_patch_install_time'};
		}
	}
	
	$model_no = `diskinfo -v | grep chassis-name | awk \'{print \$2}\'`;
	$model_no =~ s/^\s+|\s+$//g;
	$host_data->{'ipAddress'} = $ip_address;
	$host_data->{'hostName'} = $hostname;
	$host_data->{'modelNo'} = $model_no;
	$host_data->{'version'} = $storage_node_version;
	$host_data->{'patchVersion'} = $storage_patch_version;
	$host_data->{'patchInstallTime'} = $storage_patch_install_time;
	$host_data->{'hostId'} = $host_guid;
	$host_data = serialize($host_data);
	
	return $host_data;				
}

# 
# Function Name: getHostInfo
#
# Description: Used to get all the nic related information
#
# Parameters: None 
#
# Return Value: Returns the nic related information.
#
sub getNicDetails
{
	my $self = shift;	
	my ($nic_speed, $ip_address,$nic_name, $nic_data,$gateway,$dns,$mac_address);
	my $count = 0;
	
	my @nic_details = $self->nicDetails();
	$gateway = `netstat -rn | grep default | awk \'{print \$2}\'`;		
	$dns = `cat /etc/resolv.conf  | grep nameserver | awk \'{print \$2}\'`;
	$dns =~ s/\n/,/g;
	
	foreach my $data (@nic_details)
	{
		$nic_data->{$count}->{'nicSpeed'} = $data->{'nic-speed'};
		$nic_data->{$count}->{'ipAddress'} = $data->{'ip-address'};
		$nic_data->{$count}->{'nicName'} = $data->{'nic-name'};
		$nic_name = $data->{'nic-name'};
		$gateway =~ s/^\s+|\s+$//g; 
		$nic_data->{$count}->{'gateway'} = $gateway;
		chomp($dns);
		$nic_data->{$count}->{'dns'} = $dns;
		$mac_address = `ifconfig $nic_name | grep ether | awk \'{print \$2}\'`;
		$mac_address =~ s/^\s+|\s+$//g; 
		$nic_data->{$count}->{'mac_address'} = $mac_address;
		my $mask_details = `ifconfig $nic_name | grep netmask | awk \'{print \$4,\$6}\'`;
		my @mask_info = split(/ /,$mask_details);
		$mask_info[1] =~ s/^\s+|\s+$//g; 
		$nic_data->{$count}->{'broadcast'} = $mask_info[1];
		$nic_data->{$count}->{'netmask'} = $mask_info[0];
		$count++;				
	}
	if($nic_data)
	{
		$nic_data = serialize($nic_data);	
	}	
	return $nic_data;
}

sub getSlots
{
	my $self = shift;
	my($slot_number, $disk_name, $all_slot_info,$previous_slot_number);
	my $count = 0;
	my $enclosure = 1;
	my $disk_slot_op = `diskinfo -o cRD | awk \'NR>2\' | awk \'{print \$1,\$2,\$3}\'`;
	my @disk_slots = split(/\n/,$disk_slot_op);
	
	foreach my $slot_key (@disk_slots)
	{
		my @slots_data = split(/ /,$slot_key);		
		$disk_name = $slots_data[0];
		$slot_number = $slots_data[1];		
		$slot_number =~ s/^\s+|\s+$//g;
		
		my $dev_chasis_path = $slots_data[2];
		chomp($dev_chasis_path);
		my @dev_chasis_arr = split(/\./,$dev_chasis_path);
		my @dev_enclosure_arr = split(/\//,$dev_chasis_arr[1]);
        my $enclosure_name = $dev_enclosure_arr[0];
		
		if(($previous_slot_number eq "Slot_24") || ($previous_slot_number eq "Slot_12" && $slot_number eq "Slot_01"))
		{
			$enclosure++;
		}
		$all_slot_info->{$count}->{'slot_number'} = $slot_number;
		$all_slot_info->{$count}->{'enclosure'} = $enclosure;
		$all_slot_info->{$count}->{'enclosureName'} = $enclosure_name;
		$previous_slot_number = $slot_number;
		$count++;
	}	
	if($all_slot_info)
	{
		$all_slot_info = serialize($all_slot_info);
	}	
	return $all_slot_info;
}

# 
# Function Name: registerStorageStaticInfo
#
# Description: Used to communicate with cs to post the disk, volumes, luns, pool & host related information to insert into DB
#
# Parameters: None 
#
# Return Value: Nothing.
#
sub registerStorageStaticInfo
{
	my $self = shift;
	eval
	{
		my $cs_url = "http://".$STORAGE_CONF_VARS->{'CS_IP'}.":".$STORAGE_CONF_VARS->{'CS_PORT'}."/ScoutAPI/CXAPI.php";		
		my ($host_info, $network_info, $disk_info,$zpool_info,$volume_info,$LUN_info);
		
		$host_info = $self->getHostInfo();
		$network_info = $self->getNicDetails();
		$disk_info = $self->getDiskDetails();
		$zpool_info = $self->getZpoolDetails();
		$volume_info = $self->getVolumeDetails();
		$LUN_info = $self->getLUNDetails();
		my $all_slot_info = $self->getSlots();
		
		my $request_xml = "<Request Id='0001' Version='1.0'>
					<Header>
						<Authentication> 
							<AccessKeyID>DDC9525FF275C104EFA1DFFD528BD0145F903CB1</AccessKeyID>
							<AccessSignature></AccessSignature>
						</Authentication> 
					</Header>
					<Body>
						<FunctionRequest Name='RegisterStorageStaticInfo'>
							<ParameterGroup Id='RegisterStorageStaticInfo'>
								<Parameter Name='host_info' Value='".$host_info."'/>
								<Parameter Name='network_info' Value='".$network_info."'/>
								<Parameter Name='disk_info' Value='".$disk_info."'/>
								<Parameter Name='zpool_info' Value='".$zpool_info."'/>
								<Parameter Name='volume_info' Value='".$volume_info."'/>
								<Parameter Name='lun_info' Value='".$LUN_info."'/>
								<Parameter Name='slot_info' Value='".$all_slot_info."'/>
							</ParameterGroup>
						</FunctionRequest>
					</Body> 
				</Request>"."\n";					
			
		$logging_obj->log("EXCEPTION","request_xml ::$request_xml \n");
		my $response;
		
		my $content_type = 'text/xml';
		$request_xml =~ s/\"/\\"/g;
		my $post_data = "Content_Type##".$content_type."!##!"."Content##".$request_xml;
		$response = `php /home/svsystems/web/curl_dispatch_http.php "$cs_url" "$post_data"`;
		
		$logging_obj->log("EXCEPTION","Post the content for registerStorageStaticInfo request_xml - $request_xml");
		
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error updating the registerStorageStaticInfo details to the CS DB. Error : ".$@);
	}		
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

sub getAllDisksOfRpool
{
	my $self = shift;
	
	my $disk_op = `zpool status rpool | awk \'NR>6\' | awk \'!/mirror|spare|log|cache|errors|scan:|--|\^\$/\' | awk \'{print \$1}\'`;
	my @disks = split(/\n/,$disk_op);
		
	return @disks;
}


# 
# Function Name: updateNodeStatus
#
# Description: Used to communicate with cs to post the disk related information to insert into DB
#
# Parameters: None 
#
# Return Value: Nothing.
#
sub updateDiskStatus
{
	my $self = shift;
	
	eval
	{
		my $sn_details;
		$sn_details->{'disk_status'} = $self->getDiskStatus();
		&updateSNDetails($self,$sn_details);
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error updating the updateDiskStatus details to the CS DB. Error : ".$@);
	}
}

# 
# Function Name: getDiskStatus
#
# Description: Used to get the Disk Usage
#
# Parameters: None 
#
# Return Value: Returns the disk throughput, readiops & writeiops for each disk
# 
sub getDiskStatus
{
	
	my($disk_properties,$disk_usage_details,$read_iops,$write_iops,$disk_throughput,$refresh_disks,$disk_status,$all_disk_iops);
	my(@disk_through_output,@disk_throughput_value,@disk_info,@disk_status_list);
	my $count = 0;
	
	my $disks = `iostat -En|grep 'c[0-9]t*' | awk \'{print \$1}\'`;
	my @disk_names_list = split(/\n/,$disks);
	
	$refresh_disks = `devfsadm`;
	$disk_status = `echo | format | awk \'{print \$2}\'`;
	@disk_status_list = split(/\n/,$disk_status);
	
	
	foreach my $disk_name (@disk_names_list)
	{
		my($disk_reads_all, $disk_writes_all);
		
		my $rdsk_flag = 0;
		my $smartctl_sas_rdsk_check = `$smatctl_path/smartctl -i /dev/rdsk/$disk_name -d scsi`;
		if($? == 0)
		{
			$rdsk_flag = 1;
		}
		else
		{
			my $smartctl_ssd_rdsk_check = `$smatctl_path/smartctl -i /dev/rdsk/$disk_name -d sat`;
			if($? == 0)
			{
				$rdsk_flag = 1;
			}
		}
		
		if($rdsk_flag == 1)
		{
			if((grep  $_ eq $disk_name,@disk_status_list ))
			{
				$disk_usage_details->{$count}->{'status'} = 'online';
			}
			else
			{
				$disk_usage_details->{$count}->{'status'} = 'offline';
			}
		}
		else
		{
			$disk_usage_details->{$count}->{'status'} = 'offline';
		}
		
		$disk_usage_details->{$count}->{'disk_name'} =  $disk_name;		
		$count++;
	}
	
	if($disk_usage_details)
	{
		$disk_usage_details = serialize($disk_usage_details);
	}
	return $disk_usage_details;
}


# 
# Function Name: updateSNDetails
#
# Description: Used to communicate with cs to post the disk related information to insert into DB
#
# Parameters: None 
#
# Return Value: Nothing.
#
sub updateSNDetails
{
	my($self,$sn_details) = @_;
#print "updateSNDetails :: sn_details - ".Dumper($sn_details);	
	eval
	{
	
		my $cs_url = "http://".$STORAGE_CONF_VARS->{'CS_IP'}.":".$STORAGE_CONF_VARS->{'CS_PORT'}."/ScoutAPI/CXAPI.php";
		
		#my $disk_status = $self->getDiskStatus();
		my $request_xml = "<Request Id='0001' Version='1.0'>
					<Header>
						<Authentication> 
							<AccessKeyID>DDC9525FF275C104EFA1DFFD528BD0145F903CB1</AccessKeyID>
							<AccessSignature></AccessSignature>
						</Authentication> 
					</Header>
					<Body>
						<FunctionRequest Name='updateSNDetails'>
							<ParameterGroup Id='updateSNDetails'>
								<Parameter Name='hostId' Value='".$host_guid."'/>";
			foreach my $key (keys %$sn_details)
			{
				$request_xml .=	"<Parameter Name='".$key."' Value='".$sn_details->{$key}."'/>";
			}
			$request_xml .=	"</ParameterGroup>
						</FunctionRequest>
					</Body> 
				</Request>"."\n";					
			
		$logging_obj->log("EXCEPTION","updateDiskStatus - request_xml ::$request_xml \n");
		my $response;
		
		$logging_obj->log("EXCEPTION","updateSNDetails - cs_url- $cs_url , request_xml -$request_xml");		
		
		my $content_type = 'text/xml';
		$request_xml =~ s/\"/\\"/g;
		my $post_data = "Content_Type##".$content_type."!##!"."Content##".$request_xml;
		$response = `php /home/svsystems/web/curl_dispatch_http.php "$cs_url" "$post_data"`;
		
		$logging_obj->log("EXCEPTION","Post the content for updateDiskStatus response - ".$response);

	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error updating the updateDiskStatus details to the CS DB. Error : ".$@);
	}		
}


# 
# Function Name: updatePoolUsage
#
# Description: Used to communicate with cs to post the pool related information to insert into DB
#
# Parameters: None 
#
# Return Value: Nothing.
#
sub updatePoolUsage
{
	my $self = shift;
	my $mon_obj = new SystemMonitor::Monitor();	
	eval
	{
		my $sn_details;
		my $pool_usage = $mon_obj->getPoolUsage();
		$sn_details->{'pool_usage'} = $pool_usage;
		&updateSNDetails($self,$sn_details);
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error updating the updatePoolUsage details to the CS DB. Error : ".$@);
	}
}

# 
# Function Name: updatePoolUsage
#
# Description: Used to communicate with cs to post the nic related information 
#
# Parameters: None 
#
# Return Value: Nothing.
#
sub updateNetworkUsage
{
	my $self = shift;
	my $mon_obj = new SystemMonitor::Monitor();	
	eval
	{
		my $sn_details;
		my $network_usage = $mon_obj->getNetworkUsage();
		$sn_details->{'network_usage'} = $network_usage;
		&updateSNDetails($self,$sn_details);
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error updating the updateNetworkUsage details to the CS DB. Error : ".$@);
	}
}

# 
# Function Name: updateLUNDetails
#
# Description: Used to communicate with cs to post the LUN related information 
#
# Parameters: None 
#
# Return Value: Nothing.
#
sub updateLUNDetails
{
	my $self = shift;
	my $mon_obj = new SystemMonitor::Monitor();	
	eval
	{
		my $sn_details;
		my $lun_details;
		$lun_details = $mon_obj->getLUNDetails();
		$sn_details->{'lun_status'} = $lun_details;
		&updateSNDetails($self,$sn_details);
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error updating the updateLUNDetails details to the CS DB. Error : ".$@);
	}
}

# 
# Function Name: updateSensorDetails
#
# Description: Used to communicate with cs to post the LUN related information 
#
# Parameters: None 
#
# Return Value: Nothing.
#
sub updateSensorDetails
{
	my $self = shift;
my $mon_obj = new SystemMonitor::Monitor();
#print "updateSensorDetails :: ".Dumper($mon_obj);	
	eval
	{
		my $sn_details;
		my $sensor_info = $mon_obj->getSensorDetails();	
		$sn_details->{'sensor_info'} = $sensor_info;

		my $fmadm_alerts = $mon_obj->getFmadmAlerts();		
		$sn_details->{'fmadm_alerts'} = $fmadm_alerts;
		#print "updateSensorDetails :: sn_details - ".Dumper($sn_details);
		&updateSNDetails($self,$sn_details);
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error updating the updateSensorDetails details to the CS DB. Error : ".$@);
	}
}

# 
# Function Name: updateSNUsageDiskIOPS
#
# Description: Used to communicate with cs to post the LUN related information 
#
# Parameters: None 
#
# Return Value: Nothing.
#
sub updateSNUsageDiskIOPS
{
	my $self = shift;
my $mon_obj = new SystemMonitor::Monitor();
#print "updateSNUsageDiskIOPS :: ".Dumper($mon_obj);	
	eval
	{
		my $sn_details;
		#print "\ndebug1";
		my $system_usage = $mon_obj->getSystemUsage();
		#print "\ndebug2";
		$sn_details->{'system_usage'} = $system_usage;
		
		my $disk_usage;
		$disk_usage = $mon_obj->getDiskUsage();
		#print "\ndebug3";
		$sn_details->{'disk_usage'} = $disk_usage;
		
		#print "updateSNUsageDiskIOPS :: sn_details - ".Dumper($sn_details);
		&updateSNDetails($self,$sn_details);
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Error updating the updateSNUsageDiskIOPS details to the CS DB. Error : ".$@);
	}
}


1;	
