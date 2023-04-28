#=================================================================
# FILENAME
#   Monitor.pm
#
# DESCRIPTION
#    This perl module is used by the health monitor thread
#
#=================================================================
package SystemMonitor::Monitor;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
#use lib qw( /home/svsystems/rrd/lib/perl/ /home/svsystems/pm );
use Utilities;
use Common::Constants;
use Data::Dumper;
use Common::Log;
use RRDs;

my $logging_obj = new Common::Log();
my $cs_installation_path = Utilities::get_cs_installation_path();
my $MDS_AND_TELEMETRY = Common::Constants::LOG_TO_MDS_AND_TELEMETRY;
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
	my $amethyst_vars = Utilities::get_ametheyst_conf_parameters();
	my $base_dir = $amethyst_vars->{"INSTALLATION_DIR"};
	$self->{'amethyst_vars'} = $amethyst_vars;
	$self->{'base_dir'} = $base_dir;
	$self->{'is_linux'} = Utilities::isLinux();	
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
	my ($self, $cpus) = @_;
	my $cpu_load = 0;
	if($self->{'is_linux'})
	{
		if($cpus =~ /^\d+/)
		{
			#The sum of all the processes to idle processes gives the idle% of CPU 
			#and subtracting this value from 100 gives the CPU Usage Load.
			my @cpu_nums = split(/\s+/,$cpus);
			my $cpu_idle = $cpu_nums[5];
			$cpu_load = 100 - $cpu_idle;
		}
	}
	else 
	{
		#For a Windows machine the wmic formatted output gives CPU utilization percentage. 
		my $raw_cpuload = 'wmic path Win32_PerfFormattedData_PerfOS_Processor where Name="_Total" get percentprocessortime';
		my $cpu_op = `$raw_cpuload`;
		my @cpu = split(/\s+/,$cpu_op);
		$cpu_load = $cpu[1];
	}
	if($cpu_load > 0) 
	{
		$cpu_load =~s/(^\d{1,}\.\d{2})(.*$)/$1/;
	}
	#print "returning $cpu_load\n";
	return $cpu_load;
}

# 
# Function Name: getSysLoad
#
# Description: Used to get the overall load of the system. 
#
# Parameters: None 
#
# Return Value: Returns the system load.
# 
sub getSysLoad
{
	my $self = shift;
	my $sys_load;
	if($self->{'is_linux'})
	{
		#System load on Linux system can be calculated from the /proc/loadavg file.
		$sys_load = `cat /proc/loadavg | cut -d" " -f1`;
	}
	else 
	{
		#For a Windows machine the following command gives system load percentage. 
		my $raw_sysload = 'wmic path Win32_PerfFormattedData_PerfOS_System get processorqueuelength';
		my $sys_op = `$raw_sysload`;
		my @sys = split(/\s+/,$sys_op);
		$sys_load = $sys[1];
	}
	$sys_load =~ s/\n//g;
	return $sys_load;
}

# 
# Function Name: getFreeSpacePhp
#
# Description: Gives the total free space available on the system
#
# Parameters: None 
#
# Return Value: Returns the available free space.
#
sub getFreeSpacePhp
{
	my $self = shift;
	my $output;
	my ($vol_used,$vol_tot,$vol_free) = 0;
	my ($per_vol_use,$for_vol_tot,$for_vol_use,$cnt) = 0;
	if($self->{'is_linux'}) 
	{
		my ($hash,@vol_names);
		#df -lP is used to get the free space of all the volumes on the system.
		my @volume_usage = `df -lP`;
		foreach my $line (@volume_usage) 
		{
			if($line =~ /^\//) 
			{
				my @fields = split(/\s+/,$line);
				$hash->{$fields[0]}->{percent} = $fields[4];
				$hash->{$fields[0]}->{used} = $fields[2];
				$hash->{$fields[0]}->{total} = $fields[1];
				push(@vol_names,$fields[0]);
			}
			#print "$fields[0]\t$hash->{$fields[0]}->{used}\t$hash->{$fields[0]}->{free}\t$hash->{$fields[0]}->{total}\n"; #FOR DEBUG
		}
		foreach my $volume (@vol_names)
		{
			$vol_used += $hash->{$volume}->{used};
			$vol_tot += $hash->{$volume}->{total};
		}
		$vol_free = $vol_tot - $vol_used;	
		$per_vol_use = ( $vol_free / $vol_tot ) * 100;
		$per_vol_use =~s/(^\d{1,}\.\d{2})(.*$)/$1/;
		$for_vol_use = ($vol_free * 1024);
		$for_vol_tot = ($vol_tot * 1024);
	}
	else 
	{
		# In case of windows considering only the freespace for $cs_installation_path\home volume
		my $path = '"'.$cs_installation_path."\\home".'"';
		my @volume_usage = `df -a $path`;
		foreach my $line (@volume_usage) 
		{
			if($cnt > 0) 
			{
				my @fields = split(/\s+/,$line);
				$vol_tot = ($fields[1] * 1024);
				$vol_free = ($fields[3] * 1024);
			}
			$cnt++;			
		}
		$per_vol_use = ( $vol_free / $vol_tot ) * 100;
		$per_vol_use =~s/(^\d{1,}\.\d{2})(.*$)/$1/;
		$for_vol_use = $vol_free;
		$for_vol_tot = $vol_tot;
	}	
	#print "MEM USED = $for_vol_use\nTOT MEM = $for_vol_tot\nPERCENT_VOLUME = $per_vol_use\n"; # FOR DEBUG
	$output = $per_vol_use.":".$for_vol_tot.":".$for_vol_use;
	return $output;

}		

# 
# Function Name: getTotalMem
#
# Description: Gives the physical memory usage.
#
# Parameters: None 
#
# Return Value: 1,0
#	
sub getTotalMem
{
	my $self = shift;
	my ($tot_num,$free_num,$used_phy_mem) = 0;
	#We can get the Physical memory usage as total memory to free memory + buffers + cache. 
	if($self->{'is_linux'}) 
	{
		my $phy_tot = `cat /proc/meminfo | grep -i "memtotal" | cut -d":" -f2`;
		$phy_tot =~ /\d+/; 
		$tot_num = $&; #	Contains the string matched by the last pattern match
		my $raw_cache = `cat /proc/meminfo | grep -i "^cached" | cut -d":" -f2`;
		$raw_cache =~ /\d+/;
		my $cache = $&; 
		my $raw_buffer = `cat /proc/meminfo | grep -i "buffers" | cut -d":" -f2`;
		$raw_buffer =~ /\d+/;
		my $buffer = $&;
		my $phy_on_free = `cat /proc/meminfo | grep -i "memfree" | cut -d":" -f2`;
		$phy_on_free =~ /\d+/;
		my $free_on_num = $&;
		$free_num = $free_on_num + $buffer + $cache;
	}
	else 
	{
		my $raw_phymem = 'wmic path Win32_OperatingSystem get FreePhysicalMemory,TotalVisibleMemorySize';
		my $phy_mem = `$raw_phymem`;
		my @mem = split(/\s+/,$phy_mem);
		$free_num = $mem[2];
		$tot_num = $mem[3];
	}
	my $memory = $tot_num * 1024;
	my $min_mem = Common::Constants::CS_MIN_MEM;
	if($memory < $min_mem) 
	{ 	
		return 0;
	}
	elsif($memory >= $min_mem) 
	{
		return 1;
	}
}

# 
# Function Name: getMemUsagePhp
#
# Description: Gives the physical memory usage.
#
# Parameters: None 
#
# Return Value: Returns the memory used.
#
sub getMemUsagePhp
{
	my $self = shift;
	my($used_phy_mem,$tot_num,$free_num);
	if($self->{'is_linux'}) 
	{
		my $phy_tot = `cat /proc/meminfo | grep -i "memtotal" | cut -d":" -f2`;
		$phy_tot =~ /\d+/;
		$tot_num = $&;
		my $raw_cache = `cat /proc/meminfo | grep -i "^cached" | cut -d":" -f2`;
		$raw_cache =~ /\d+/;
		my $cache = $&;
		my $raw_buffer = `cat /proc/meminfo | grep -i "buffers" | cut -d":" -f2`;
		$raw_buffer =~ /\d+/;
		my $buffer = $&;
		my $phy_on_free = `cat /proc/meminfo | grep -i "memfree" | cut -d":" -f2`;
		$phy_on_free =~ /\d+/;
		my $free_on_num = $&;
		$free_num = $free_on_num + $buffer + $cache;
	}
	else 
	{
		my $raw_phymem = 'wmic path Win32_OperatingSystem get FreePhysicalMemory,TotalVisibleMemorySize';
		my $phy_mem = `$raw_phymem`;
		my @mem = split(/\s+/,$phy_mem);
		$free_num = $mem[2];
		$tot_num = $mem[3]; 
	}
	$used_phy_mem = $tot_num - $free_num;
	my $per_usage = ( $used_phy_mem / $tot_num ) * 100;
	$per_usage =~s/(^\d{1,}\.\d{2})(.*$)/$1/;
	my $output = $per_usage.":".($tot_num * 1024).":".($used_phy_mem * 1024);
	#print "output::$output\n";
	return $output;
}

# 
# Function Name: getApache
#
# Description: Configuration Policy Server is calculated with respect to 
#			   Total Accesses, Bytes/Sec and Request/Sec. 	
#
# Parameters: None 
#
# Return Value: Returns $apa_ret
#
sub getApache
{
	my $self = shift;
	my $apa_ret;
	if($self->{'is_linux'}) 
	{
		my $file = $self->{'base_dir'}."/var/apache.txt";
		my (@apa_out,$apache_det);
		my ($prev_req_per_sec);
		if(-e $file) 
		{
			open(PREV_AP,$file);
			while(<PREV_AP>) 
			{
				push(@apa_out,$_);
			}
			close(PREV_AP);
			foreach my $apa (@apa_out) 
			{
				my @params = split(/:/,$apa);
				#if($apa =~ /([aA-zZ]+)(\s?)([aA-zZ]*)(\s?):\s?((\d*)(\.?)(\d*))/) {
				if($params[0] eq "Total Accesses") 
				{
					$params[0] =~ s/\s//;
					$params[1] =~ s/\s//;
					$params[1] =~ s/\n//;
					if($params[1] =~ /^\./) 
					{
						$params[1] =~ s/(\.\d{2})(.*$)/0$1/;
					}
					else 
					{
						$params[1] =~ s/(^\d{1,}\.\d{2})(.*$)/$1/;
					}
					$prev_req_per_sec = $params[1];
				}
			}
		}
		my $cs_port = $self->{'amethyst_vars'}->{'CS_PORT'};
		my $csHTTP = $self->{'amethyst_vars'}->{'CS_HTTP'};
		
		my $option = "";
		if($csHTTP eq "https")
		{
			$option = "--no-check-certificate";
		}
		$logging_obj->log("INFO"," getApache cs_http :: $csHTTP , cs_port :: $cs_port \n");
		`wget $option $csHTTP://localhost:$cs_port/server-status?auto -qO $file`;
		open(AP,$file);
		while(<AP>) 
		{
			push(@apa_out,$_);
		}
		close(AP);
		foreach my $apa (@apa_out) 
		{
			my @params = split(/:/,$apa);
			#if($apa =~ /([aA-zZ]+)(\s?)([aA-zZ]*)(\s?):\s?((\d*)(\.?)(\d*))/) {
			if($params[0] eq "Total Accesses" or $params[0] eq "ReqPerSec" or $params[0] eq "BytesPerSec") 
			{
				$params[0] =~ s/\s//;
				$params[1] =~ s/\s//;
				$params[1] =~ s/\n//;
				if($params[1] =~ /^\./) 
				{
					$params[1] =~ s/(\.\d{2})(.*$)/0$1/;
				}
				else 
				{
					$params[1] =~ s/(^\d{1,}\.\d{2})(.*$)/$1/;
				}
				$apache_det->{$params[0]} = $params[1];
			}
		}	
		if($prev_req_per_sec) 
		{
			my $present_reqs = $apache_det->{'TotalAccesses'};
			my $value = ($present_reqs - $prev_req_per_sec)/60;
			if($value > 0) 
			{
				$apache_det->{'ReqPerSec'} = $value;
			}
		}
			$apa_ret = "$apache_det->{TotalAccesses}:$apache_det->{ReqPerSec}:$apache_det->{BytesPerSec}";
	}
	else
	{
		my $raw_apache = `wmic path Win32_PerfFormattedData_W3SVC_WebService where Name="_Total" get BytesTotalPersec, GetRequestsPersec, PostRequestsPersec, TotalGetRequests, TotalPostRequests`;
		if($raw_apache !~ /ERROR/i) 
		{
			my @apache_params = split(/\s+/,$raw_apache);
			my $bytes_per_sec = $apache_params[5];
			my $requests_per_sec = $apache_params[6] + $apache_params[7];
			my $total_hits = $apache_params[8] + $apache_params[9];
			$apa_ret = "$total_hits:$bytes_per_sec:$requests_per_sec";
		}
		else 
		{
			$apa_ret = "0:0:0";
		}
	}
	#print "APA RETURN ****$apa_ret****";
	return $apa_ret;
}

# 
# Function Name: getApachePhp
#
# Description:
#	 Gets the value of per apache load  	
#
# Parameters: None 
#
# Return Value: Returns per apache load
#
sub getApachePhp
{
	my $self = shift;
	my ($per_apache_load) = 0;
	if($self->{'is_linux'}) 
	{
		my $cs_port = $self->{'amethyst_vars'}->{'CS_PORT'};
		my $csHTTP = $self->{'amethyst_vars'}->{'CS_HTTP'};
		
		my (@apa_out,$apache_load) = 0;
		my $option = "";
		if($csHTTP eq "https")
		{
			$option = "--no-check-certificate";
		}
		$logging_obj->log("INFO"," getApachePhp cs_http :: $csHTTP , cs_port :: $cs_port  \n");		
		
		`wget $option $csHTTP://localhost:$cs_port/server-status?auto -qO apache.txt`;		
		open(AP,"apache.txt");
		while(<AP>) 
		{
			push(@apa_out,$_);
		}
		foreach my $apa (@apa_out) 
		{
			my @params = split(/:/,$apa);
			#if($apa =~ /([aA-zZ]+)(\s?)([aA-zZ]*)(\s?):\s?((\d*)(\.?)(\d*))/) {
			$params[0] =~ s/\s//;
			if($params[0] eq "CPULoad") 
			{
				$params[1] =~ s/\s//;
				$params[1] =~ s/\n//;
				if($params[1] =~ /^\./) 
				{
					$params[1] =~ s/(\.\d{2})(.*$)/0$1/;
				}
				else 
				{	
					$params[1] =~ s/(^\d{1,}\.\d{2})(.*$)/$1/;
				}
				$apache_load = $params[1];
				$per_apache_load = $apache_load;
			}
		}
	
	}
	else 
	{
		my $one_more_apache = `tasklist | findstr /I "w3sp"`;
		if($one_more_apache !~ /ERROR/i) 
		{
		my @apache_vals = split(/\s+/,$one_more_apache);
		$per_apache_load = $apache_vals[2];
		}			
		$one_more_apache = `tasklist | findstr /I "php-cgi"`;
		if($one_more_apache !~ /ERROR/i) 
		{
			my $apa_load;
			my @apache_vals = split(/\s+/,$one_more_apache);
			foreach my $php (@apache_vals) 
			{
				my @php_loads = split(/s+/,$php);
				$apa_load += $php_loads[2];
			}
				$per_apache_load += $apa_load;
		}
	}
	if($per_apache_load > 0) 
	{
		$per_apache_load =~ s/(^\d{1,}\.\d{2})(.*$)/$1/;
	}
	return $per_apache_load;
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
	my ($self,$disk_buf) = @_;
	$disk_buf =~ s/\s/:/g;
	my $disk_activity = 0;
	my $cpu_load_disk = 0;
	if($self->{'is_linux'}) 
	{
		my @disk = split(/::/,$disk_buf);
		foreach my $per_disk (@disk) 
		{
			my @disk_io = split(/:/,$per_disk);
			if($per_disk =~ /^[s,h]d/) 
			{
			$disk_activity += $disk_io[2] + $disk_io[3];	   
			}
			else
			{
				$cpu_load_disk = $disk_io[$#disk_io];				
			}
		}
	}
	else 
	{
		my $raw_diskio = `wmic path Win32_PerfFormattedData_PerfDisk_LogicalDisk where Name="_Total" get AvgDiskQueueLength`;
		my @io = split(/\s+/,$raw_diskio);
		$disk_activity = $io[1];
	}
	if($disk_activity > 0) 
	{
		$disk_activity =~s/(^\d{1,}\.\d{2})(.*$)/$1/;
	}
	#print "Disk activity - $disk_activity\n";
	return $disk_activity;
}	

# 
# Function Name: getMySql
#
# Description: Used to get mysql load 
#
# Parameters: None 
#
# Return Value: Returns mysql load
#
sub getMySql
{
	my $self = shift;
	my (@sql_status,$mysql_store);
	
    my $db_user = $self->{'amethyst_vars'}->{'DB_USER'};
    my $db_passwd = $self->{'amethyst_vars'}->{'DB_PASSWD'};
	if($self->{'is_linux'}) 
	{
		#The output of “mysqladmin extended-status” is captured to calculate MySql load
        #@sql_status = `mysqladmin extended-status`;
        my $cmd = "mysqladmin -u $db_user -p$db_passwd extended-status";
        @sql_status = `$cmd`;
	}
	else
	{
		my $cmd = $self->{'amethyst_vars'}->{'MYSQL_PATH'}.'\\bin\\mysqladmin.exe';
		#@sql_status = `"C:\\Program Files (x86)\\MySQL\\MySQL Server 5.1\\bin\\mysqladmin.exe" -u $db_user -p$db_passwd extended-status`;
		@sql_status = `"$cmd" -u $db_user -p$db_passwd extended-status`;
		#print "executed and got @sql_status\n";
	}
	#MySql load is calculated based on the parameters present in sql_status array.
	foreach my $status_par (@sql_status) 
	{ 
		my @par = split(/\|/,$status_par);
		$par[1] =~ s/\s+//g;
		if($par[1] eq "Questions" or 
			$par[1] eq "Bytes_sent" or 
			$par[1] eq "Bytes_received" or 
			$par[1] eq "Threads_connected" or 
			$par[1] eq "Threads_running") 
		{
				$par[2] =~ s/\s+//g;
				$mysql_store->{$par[1]} = $par[2];
		}
	}		 
	my $mysql_ret = "$mysql_store->{Questions}:$mysql_store->{Bytes_sent}:$mysql_store->{Bytes_received}:$mysql_store->{Threads_connected}:$mysql_store->{Threads_running}";
	return $mysql_ret;
}

# 
# Function Name: getMySqlPhp
#
# Description: Used to get per sql load 
#
# Parameters: None 
#
# Return Value: Returns per sql load
#	
sub getMySqlPhp
{	
	my $self = shift;
	my $sql = `cat /etc/my.cnf | grep -i "max_connections"`;
	my @sql_op = split(/=/,$sql);
	my $max_conn = $sql_op[1];
	my $used_conn;
	my $db_user = $self->{'amethyst_vars'}->{'DB_USER'};
    my $db_passwd = $self->{'amethyst_vars'}->{'DB_PASSWD'};
	my @sql_status = `mysqladmin -u $db_user -p$db_passwd extended-status`;
	foreach my $status_par (@sql_status) 
	{
		my @par = split(/\|/,$status_par);
		$par[1] =~ s/\s+//g;
		if($par[1] eq "Threads_connected") 
		{
			$par[2] =~ s/\s+//g;
			$used_conn = $par[2];
		}
	}
	my $per_sql_load = ($used_conn / $max_conn) * 100;
	$per_sql_load =~ s/(^\d{1,}\.\d{2})(.*$)/$1/;
	return $per_sql_load;
}

# 
# Function Name: getAll
#
# Description: Used to get perf parameters
#
# Parameters: None 
#
# Return Value: Returns perf params
#
sub getAll
{
	my $self = shift;
	my($guid,$sys_load,$load,$free_space,$mem_usage,$disk_io,$apache,$mysql,$output) = 0;
	if($self->{'is_linux'}) 
	{
		my $result = `/home/svsystems/bin/iostatavg.sh`;
		$load = $self->getCpuLoad($result);
		$disk_io = $self->getDiskActivity($result);
	}
	else
	{
		$load = $self->getCpuLoad();
		$disk_io = $self->getDiskActivity();
	}
	$sys_load = $self->getSysLoad();	
	$free_space = $self->getFreeSpacePhp();
	$mem_usage = $self->getMemUsagePhp();	
	$apache = $self->getApache();
	$mysql = $self->getMySql();
	$guid = $self->getGuid();
	$output = "$guid:$sys_load:$load:$free_space:$mem_usage:$disk_io:$apache:$mysql";
	#print "Output is $output\n";
	return $output;
}

sub getPerfAll
{
	my $self = shift;
	my($guid,$apache,$mysql,$output) = 0;
	$guid = $self->getGuid();
	$apache = $self->getApachePhp();
	if($self->{'is_linux'}) 
	{
		$mysql = $self->getMySqlPhp();
	}
	else 
	{
		# my $innodb_file_path = $self->{'amethyst_vars'}->{'MYSQL_PATH'}.'\\my-innodb-heavy-4G.ini';
		# my $sql = `cat "$innodb_file_path" | findstr /I "max_connections"`;
		# my @sql_op = split(/=/,$sql);
		# my $max_conn = $sql_op[1];
		##
		# Hard coding for now as my.ini file path is not known
		##
		my $max_conn = 800;
		$mysql = $self->getMySql();
		my @mysql_params = split(":",$mysql);
		$mysql = ($mysql_params[3] / $max_conn) * 100;
	}
	$output = "$apache:$mysql";
	#print "OUT IS $output\n";
	return $output;
}

sub getPerfPs
{
	my $self = shift;
	my($guid,$sys_load,$load,$free_space,$mem_usage,$disk_io,$apache,$mysql,$output,$cpu_load_disk) = 0;
	if($self->{'is_linux'}) 
	{
		my $result = `/home/svsystems/bin/iostatavg.sh`;
		$load = $self->getCpuLoad($result);
		$disk_io = $self->getDiskActivity($result);
	}
	else
	{
		$load = $self->getCpuLoad();
		$disk_io = $self->getDiskActivity();
	}
	$sys_load = $self->getSysLoad();	
	$free_space = $self->getFreeSpacePhp();
	$mem_usage = $self->getMemUsagePhp();	
	$guid = $self->getGuid();
	$output = "$guid:$sys_load:$load:$free_space:$mem_usage:$disk_io:0:0:0:0:0:0:0:0";
	#print "Output is $output\n";
	return $output;
}

# 
# Function Name: getCpuInfo
#
# Description: Used to get cpu information
#
# Parameters: None 
#
# Return Value: Returns cpu_info
#	
sub getCpuInfo
{	
	my $self = shift;
	my($cpu_cores) = 0;
	if($self->{'is_linux'}) 
	{
		$cpu_cores = `cat /proc/stat | grep -E 'cpu[0-9]{1,3}' | wc -l`;
		$cpu_cores =~ s/\s+//g;
	}
	if(Utilities::isWindows()) 
	{
		my $cpu_cores_op = `wmic path Win32_Processor get NumberOfCores`;
		my @cores = split(/\s+/,$cpu_cores_op);
		my $core_count = $#cores + 1;
		for(my $i=1;$i<$core_count;$i++) 
		{
			$cpu_cores += $cores[$i];
		}
		$cpu_cores =~ s/\s+//g;
	}
	my $cpu_info = $cpu_cores;
	return $cpu_info;
}

# 
# Function Name: getServiceStatus
#
# Description: Used to get the status of the services
#
# Parameters: None 
#
# Return Value: Returns status
#
sub getServiceStatus
{
	my $self = shift;
	my $services;
	my $return_status = "";
	my $VOLSYNC_MAX = Common::Constants::VOLSYNC_MAX;
	my $guid = $self->getGuid();
	if($self->{'is_linux'}) 
	{
		$services = Common::Constants::SERVICES;
		$return_status = $self->linuxServices($services,$VOLSYNC_MAX,$guid);
	}
	else
	{
		$services = Common::Constants::WIN_SERVICES;
		$return_status = $self->windowsServices($services,$VOLSYNC_MAX,$guid);
	}
	return $return_status;
}

# 
# Function Name: linuxServices
#
# Description: Used to get the status of the services
#
# Parameters: None 
#
# Return Value: Returns status
#
sub linuxServices
{
	my $self = shift;
	my ($services,$VOLSYNC_MAX,$guid) = @_;
	my $return_status = "";
	my $service_monitor_buffer_time = Common::Constants::SEVICE_MONITOR_BUFFER_TIME;
	
	foreach my $service (@$services) 
	{
		my $status = 0;
		my $cmd;
				
		if($service eq "volsync") 
		{
			$cmd = "ps aux | grep -i 'volsync_child' | grep -v 'grep' | wc -l";
			my $result = `$cmd`;
			$result =~ s/\n//;
			$status = $result;
			if($status > $VOLSYNC_MAX) 
			{
				$status = $VOLSYNC_MAX;
			}
		}
		elsif($service eq "cxps")
		{
			$cmd = 'ps aux | grep -P "cxps"';
		}
		elsif($service =~ /^monitor/ || $service eq "gentrends" || $service eq "array" || $service eq "prism" || $service eq "fabric")
		{
			my $service_file = $self->{'amethyst_vars'}->{'INSTALLATION_DIR'}."/var/services/".$service;
			if(-f($service_file))
			{
				my $run_events_pid_file = $self->{'amethyst_vars'}->{'INSTALLATION_DIR'}."/var/eventmanager.pid";
				open(PID_FILE_CONTENTS,$run_events_pid_file);
				my $run_events_pid = <PID_FILE_CONTENTS>;
				close(PID_FILE_CONTENTS);
				chomp($run_events_pid);
				$logging_obj->log("DEBUG","Run events PID : $run_events_pid");
				$logging_obj->log("DEBUG","PID check command : ps aux | grep $run_events_pid | grep -v grep");
				my $pid_running = `ps aux | grep " $run_events_pid " | grep -v grep`;				
				if($pid_running)
				{
					my $current_timestamp = time;
					open(SERVICE_TIME,$service_file);
					my $service_timestamp = <SERVICE_TIME>;
					close(SERVICE_TIME);
					my $get_service_lowest_timestamp = &Utilities::get_service_lowest_interval($service);
					my $threshold_time = $service_timestamp + $get_service_lowest_timestamp + $service_monitor_buffer_time;
					if(($current_timestamp - $service_timestamp) < $threshold_time) 
					{
						$status = 1;
					}
				}
			}
		}
		else
		{
			$cmd = 'ps aux | grep -P '.$service;
		}
		if($service !~ /^monitor/ && $service ne "gentrends" && $service ne "volsync") 
		{
			my $result = `$cmd`;
			my @lines = split(/\n/, $result);
			foreach(@lines) 
			{
				#ignore results for the grep command and this script
				unless ($_ =~ m/grep/ ||  $_ =~ m/$0/)
				{
					$status = 1;
				}	
			}
		}
		$return_status .= ":" . $status;
	}
	$return_status = $guid . $return_status . ":0";
	return $return_status;
}

# 
# Function Name: windowsServices
#
# Description: Used to get the status of the services
#
# Parameters: None 
#
# Return Value: Returns status
#
sub windowsServices
{
	my $self = shift;
	my ($services,$VOLSYNC_MAX,$guid) = @_;
	my $return_status = "";
	my %log_data;
	my $service_status;
	
	my %telemetry_metadata = ('RecordType' => 'WindowsServiceMonitor');
	$logging_obj->set_telemetry_metadata(%telemetry_metadata);
	my $service_monitor_buffer_time = Common::Constants::SEVICE_MONITOR_BUFFER_TIME;
	my $max_attempts = 60;
	my $sleep_time = 5;
	
	foreach my $service (@$services) 
	{
		my $count = 0;
		my $status = 0;
		my ($cmd,$service_name);

		if($service eq "volsync") 
		{
			$cmd = 'wmic process get commandline | findstr /I "volsync_child"';
			my $result = `$cmd`;
			my @lines = split(/\n/, $result);
			foreach(@lines) 
			{
				unless ($_ =~ m/findstr/ || $_ eq "")
				{
					$count++;
				}
			}
			$status = $count;		
			if($status > $VOLSYNC_MAX) 
			{
				$status = $VOLSYNC_MAX;
			}
		}
		elsif($service =~ /^monitor/ || $service eq "gentrends")
		{
			my $service_file = $self->{'amethyst_vars'}->{'INSTALLATION_DIR'}."/var/services/".$service;
			if(-f($service_file))
			{
				$status = 1;
			}
		}		
		else 
		{
			$cmd = "wmic process get commandline | findstr /I " . $service;
			
			# Above wmic command is giving output for both ProcessServer and ProcessServermonitor services as it is not doing exact service string match. Appending .exe to ProcessServer to fix it.
			if($service eq "ProcessServer")
			{
				$cmd = $cmd . ".exe";
			}
		}
		if($service !~ /^monitor/ && $service ne "gentrends" && $service ne "volsync") 
		{
			#print "Command is $cmd\n";
			my $result = `$cmd`;
			#print "RES $result\n";
			my @lines = split(/\n/, $result);
			foreach(@lines) 
			{
				#ignore results for the grep command and this script
				unless ($_ =~ m/findstr/ ||  $_ =~ m/$0/)
				{
					$status = 1;
				}
			}
			
			if($service eq "pushinstall") 
			{
				if(!$status)  #pushinstall service is down
				{
					$log_data{"Command"} = $cmd;
					$log_data{"Result"} = $result;
					$logging_obj->log("INFO",\%log_data,$MDS_AND_TELEMETRY);
					
					$service_name = "InMage PushInstall";
					$service_status  = Utilities::start_windows_service($service_name,$max_attempts,$sleep_time);
					if($service_status)
					{
						$status = 1; # reseting push install service status as it started now.
					}
				}
			}
		}
		#print "$status of $service\n";
		$return_status .= ":" . $status;	  
	}
	$return_status = $guid . $return_status . ":0:0:0";
	return $return_status;
}

# 
# Function Name: createServiceRrdDb
#
# Description: 
#	 Updates the existing rrd or creates a new rrd 
#
# Parameters:
#    Param 1 [IN]:$rrd
#	  	   - This holds the rrd name
#    Param 2 [IN]:$data
#	  	   - This holds data present in rrds
#    Param 3 [IN]:$type
#	  	   - This holds type of rrd
#    Param 4 [IN]:$time
#	  	   - This holds time
#
# Return Value: Returns rrd
#
sub createServiceRrdDb ($$) {
	my $self = shift;
	my ($rrd,$data,$type,$time) = @_;
	$time = time unless ( $time );
	my @rrdcmd = ();
	if(Utilities::isWindows()) {
		$rrd =~ s/\//\\/g;
	#	$rrd = "c:" . $rrd;
	#	#print "changed rrd to $rrd\n";
	}
    
	#Each of the service is monitored and the values 
	#are collected and stored to a RRD
	if(! -e $rrd) {
		push @rrdcmd, $rrd;
		push @rrdcmd, "--step=60";
		push @rrdcmd, "--start=N-60";
		push @rrdcmd, "DS:monitor:GAUGE:86400:0:1";
		push @rrdcmd, "DS:monitor_ps:GAUGE:86400:0:1";
		push @rrdcmd, "DS:monitor_disks:GAUGE:86400:0:1";
		push @rrdcmd, "DS:volsync:GAUGE:86400:U:U";
		push @rrdcmd, "DS:scheduler:GAUGE:86400:0:1";
		push @rrdcmd, "DS:httpd:GAUGE:86400:0:1";
		push @rrdcmd, "DS:pushinstalld:GAUGE:86400:0:1";
		push @rrdcmd, "DS:heartbeat:GAUGE:86400:0:1";
		push @rrdcmd, "DS:gentrends:GAUGE:86400:0:1";
		push @rrdcmd, "DS:mysqld:GAUGE:86400:0:1";
		push @rrdcmd, "DS:bpm:GAUGE:86400:0:1";
		push @rrdcmd, "DS:cxps:GAUGE:86400:0:1";
		push @rrdcmd, "DS:wintc:GAUGE:86400:0:1";
	
		push @rrdcmd, "DS:ex-service1:GAUGE:86400:0:1";
		push @rrdcmd, "DS:ex-service2:GAUGE:86400:0:1";
		push @rrdcmd, "DS:ex-service3:GAUGE:86400:0:1";
		push @rrdcmd, "RRA:AVERAGE:0.5:1:129600";

		RRDs::create (@rrdcmd);
		my $err = RRDs::error;
        if($err)
        {
            $logging_obj->log("EXCEPTION","unable to create $rrd: $err\n");
        }
		@rrdcmd = ();
	}
    # Update the RRD
	push @rrdcmd, $rrd;
	push @rrdcmd, "$time:$data";
	RRDs::update ( @rrdcmd );
	my $err = RRDs::error;
    if($err)
    {
        $logging_obj->log("EXCEPTION","unable to update $rrd: $err\n");
    }
	return $rrd;
}

# 
# Function Name: getGuid
#
# Description:  Used to get the host guid
#
# Parameters: None 
#
# Return Value: Returns guid
#
sub getGuid 
{
	my $self = shift;
	my $guid = $self->{'amethyst_vars'}->{'HOST_GUID'};
	$guid =~ s/\n//;
	return $guid;
}

sub maintainPsIps($) 
{
	my $self = shift;
	my($ps_guid) = @_;
	if($ps_guid ne "") {
		my $exist_flag = 0;
		my $FILE_LOC   = $self->{'base_dir'}."/var/PS_GUIDS";
		if(-e "$FILE_LOC") {
			open(PS_GUID,"<$FILE_LOC");
			while(<PS_GUID>) {
				if($_ =~ /[aA0-zZ9]+/) {
					chomp($_);
					if($_ eq $ps_guid) {
						$exist_flag = 1;
						last;
					}
				}
			}
			if($exist_flag == 0) {
				open(PS_GUID,">>$FILE_LOC");
				print PS_GUID $ps_guid;
				print PS_GUID "\n";
				close PS_GUID;
			}
		}
		else {
			if($ps_guid =~ /[aA0-zZ9]+/) {
				open(PS_GUID,">>$FILE_LOC");
				print PS_GUID $ps_guid;
				print PS_GUID "\n";
				close PS_GUID;
			}
		}
	}
	return;
}

# 
# Function Name: createPerfRrdDb
#
# Description:  
#	 Updates the existing rrd or creates a new rrd with perf parameters
# Parameters:
#    Param 1 [IN]:$rrd
#	  	   - This holds the rrd name
#    Param 2 [IN]:$data
#	  	   - This holds data present in rrds
#    Param 3 [IN]:$type
#	  	   - This holds type of rrd
#    Param 4 [IN]:$time
#	  	   - It holds time
#
# Return Value: Returns rrd
#
sub createPerfRrdDb($$) 
{
	my $self = shift;
	my ($rrd,$data,$itype,$time) = @_;
	$time = time unless ($time);
	my @rrdcmd = ();
	if(Utilities::isWindows()) {
		$rrd =~ s/\//\\/g;
	#	$rrd = "c:" . $rrd;
		#print "changed rrd to $rrd\n";
	}
    
	#Each of the parameter is monitored for an interval 
	#of one minute and the values are stored to a RRD 
	if(! -e $rrd) { 
		push @rrdcmd, $rrd;
		push @rrdcmd, "--step=60";
		push @rrdcmd, "--start=N-60";
		push @rrdcmd, "DS:sysload:GAUGE:86400:U:U";
		push @rrdcmd, "DS:cpuload:GAUGE:86400:U:U";
		push @rrdcmd, "DS:freespace:GAUGE:86400:U:U";
		push @rrdcmd, "DS:tot_freespace:GAUGE:86400:U:U";
		push @rrdcmd, "DS:avail_freespace:GAUGE:86400:U:U";
		push @rrdcmd, "DS:memusage:GAUGE:86400:U:U";
		push @rrdcmd, "DS:tot_memusage:GAUGE:86400:U:U";
		push @rrdcmd, "DS:free_memusage:GAUGE:86400:U:U";
		push @rrdcmd, "DS:diskactivity:GAUGE:86400:U:U";	
		push @rrdcmd, "DS:hits:GAUGE:86400:U:U";
		push @rrdcmd, "DS:rps:GAUGE:86400:U:U";
		push @rrdcmd, "DS:bps:GAUGE:86400:U:U";
		push @rrdcmd, "DS:ques:GAUGE:86400:U:U";
		push @rrdcmd, "DS:byse:GAUGE:86400:U:U";
		push @rrdcmd, "DS:byre:GAUGE:86400:U:U";
		push @rrdcmd, "DS:thrcon:GAUGE:86400:U:U";
		push @rrdcmd, "DS:thrun:GAUGE:86400:U:U";
		push @rrdcmd, "DS:nexten1:GAUGE:86400:U:U";
		push @rrdcmd, "DS:nexten2:GAUGE:86400:U:U";
		push @rrdcmd, "DS:nexten3:GAUGE:86400:U:U";
		push @rrdcmd, "RRA:AVERAGE:0.5:1:129600";
		RRDs::create (@rrdcmd);
		my $err = RRDs::error;
        if ($err)
        {
            $logging_obj->log("EXCEPTION","unable to create $rrd: $err\n");
        }
		@rrdcmd = ();
	}
    # Update the RRD
	push @rrdcmd, $rrd;
	push @rrdcmd, "$time:$data";
	RRDs::update ( @rrdcmd );
	my $err = RRDs::error;
	$logging_obj->log("DEBUG","rrd::$rrd");
    if($err)
    {
        $logging_obj->log("EXCEPTION","unable to update $rrd: $err\n");
    }
	return $rrd;
}
	
1;