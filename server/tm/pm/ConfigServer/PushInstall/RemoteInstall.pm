package ConfigServer::PushInstall::RemoteInstall;

use warnings;
use strict;

use Cwd qw(abs_path);
use File::Path;
use Net::Ping;
use Net::OpenSSH;
use Expect;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");

######################################################################
# Set of allowed options to be passed to constructor
######################################################################

my %legalopts = ( 
	'ip_address' => 'mandatory',
	'user_name' => 'mandatory',
	'password' => 'mandatory',
	'cx_ip' => 'mandatory',
	'install_handle' => 'mandatory',
	'os_details_script_path' => 'optional',
	'bld_tar_path' => 'optional',
	'bld_config' => 'optional',
	'log_path' => 'optional',
	'installation_mode' => 'optional',
	'installation_dir' => 'optional',
	'cx_port' => 'optional',
	'start_agent' => 'optional',
	'debug' => 'optional',
	'component' => 'optional',
);

######################################################################
# Private lexical constants
######################################################################

$|++;

my $os_detection_script_basename = "OS_details.sh";
my $default_remote_copy_basedir = "/tmp";
my $this_session_dir = "remote_install_$$";
my $default_remote_copy_dir = "${default_remote_copy_basedir}/${this_session_dir}";
my $rebooted = 0;
my $remote_host_os;

my $pkg = __PACKAGE__;
$pkg =~ s|::|/|g;
$pkg = $pkg . ".pm";

my ($fh_out,$fh_err);
my ($bld_targz_name,$pwd);

( $pwd = abs_path($INC{$pkg}) ) =~ m|^(.+)/| and $pwd = $1;

######################################################################
# Class Methods/APIs - Public
#
# 'new' => constructor
# 'writeout' => Writes messages to STDOUT if used
# 'run' => Entry point for object (calls other functions as needed)
######################################################################

sub new
{
	my ($pkg,$opt) = @_;
	warn "Hash reference to options missing in call to 'new'" unless (defined $opt and ref($opt) eq "HASH");
	my $class = ref $pkg || $pkg;
	my $self = {};

	foreach my $legalkey (sort keys %legalopts) {
		$self->{$legalkey} = $opt->{$legalkey};
	}

	# Assign defaults to optional options if undefined

	$self->{bld_tar_path} = $self->{bld_tar_path} || $pwd;
	$self->{bld_config} = $self->{bld_config} || "release";
	$self->{log_path} = $self->{log_path} || $pwd;

	$self->{os_details_script_path} = $self->{bld_tar_path};

	# Add a sub-directory for the IP address

	$self->{log_path} = $self->{log_path} . "/" . $self->{ip_address};

	# Delete the trailing '/' if any , in the path variables

	$self->{os_details_script_path} =~ s|/$||;
	$self->{bld_tar_path} =~ s|/$||;
	$self->{log_path} =~ s|/$||;

	$self->{installation_mode} = $self->{installation_mode} || 'host';
	$self->{installation_dir} = $self->{installation_dir} || '/usr/local/InMage';
	$self->{start_agent} = $self->{start_agent} || 'Y';
	$self->{debug} = $self->{debug} || '0';
	$self->{component} = $self->{component} || 'UA';

	# Extra keys which are not passed by user but are useful for the object to maintain states, global data, etc

	$self->{cmd} = undef;
	$self->{known_host} = undef;
	$self->{remote_host_ssh_obj} = undef;
	$self->{job_id} = undef;

	bless $self, $class;
	return $self;
}

sub writeout
{
	print scalar localtime," :: ";
	print "@_\n";
}

sub run
{
	my $self = shift;
	my $job_id = shift;
	my $name = $self->whoami;

	# Add job_id to object
	$self->{job_id} = $job_id;

	# Make log directory with job id into consideration
	$self->{log_path} = $self->{log_path} . "/" . $self->{job_id};

	my $log_dir = $self->{log_path};
	mkpath( "$log_dir" , {verbose => 1, mode => 0755} );

	open $fh_out, ">","$log_dir/install.out" or die "Could not open file $log_dir/install.out for writing installation output\n";
	open $fh_err, ">","$log_dir/install.err" or die "Could not open file $log_dir/install.err for writing installation errors\n";

	$self->debug_print_to_log($fh_out,"In $name now\n");

	my $start_time = time();

	########################
	# Initialization
	########################

	eval { 
					$self->Log($fh_out,"Initializing");
					$self->initialize;
				};

	if ($@) {
		$self->Log($fh_err,"$@");
		$self->Log($fh_out,"Initialization failure!\n");
		return $self->set_results("initialization",-1,$start_time,time,"$log_dir/install.err","Failed with reason - $@","Initialization failure!");
	} else {
		$self->Log($fh_out,"Initialization successful\n");
		$self->{install_handle}->update_install_status
			(	$self->{ip_address},
				$self->{user_name},
				"initialization",
				0,
				$start_time,
				time,
				"$log_dir/install.out",
				"",
				2,
				$job_id,
				"Initialization successful"
			);
	}

	########################
	# Contacting remote IP
	########################

	eval {
				$self->Log($fh_out,"Checking if remote host $self->{ip_address} is reachable");
				$self->is_remote_host_alive;
				};

	if ($@) {
		$self->Log($fh_err,"$@");
		$self->Log($fh_out,"Remote host $self->{ip_address} ping failed!\n");
		return $self->set_results("check_remote_host_ssh_reachability",-1,$start_time,time,"$log_dir/install.err","Failed with reason - $@","Remote host $self->{ip_address} ping failed!");
	} else {
		$self->Log($fh_out,"Remote host $self->{ip_address} is reachable and its SSH service is running\n");
		$self->{install_handle}->update_install_status
			(	$self->{ip_address},
				$self->{user_name},
				"check_remote_host_ssh_reachability",
				0,
				$start_time,
				time,
				"$log_dir/install.out",
				"",
				2,
				$job_id,
				"Remote host $self->{ip_address} is reachable and its SSH service is running"
			);
	}

	########################
	# Verify if known host
	########################

	eval {
				$self->Log($fh_out,"Checking user authentication");
				$self->verify_remote_host_key_and_passwd;
				};

	if ($@) {
		$self->Log($fh_err,"$@");
		$self->Log($fh_out,"User authentication failed!\n");
		return $self->set_results("check_remote_user_credentials",-1,$start_time,time,"$log_dir/install.err","Failed with reason - $@","User authentication failed!");
	} else {
		$self->Log($fh_out,"User authentication successful\n");
		$self->{install_handle}->update_install_status
			(	$self->{ip_address},
				$self->{user_name},
				"check_remote_user_credentials",
				0,
				$start_time,
				time,
				"$log_dir/install.out",
				"",
				2,
				$job_id,
				"User authentication successful"
			);
	}

	# Create the remote temporary directory to which builds and other files are copied
	mkpath($default_remote_copy_dir, { mode => 0777 });

	########################
	# Check existing install
	########################

	eval {
				$self->Log($fh_out,"Performing pre-install checks");
				$self->preinstall_check;
				};

	if ($@) {
		$self->Log($fh_err,"$@");
		$self->Log($fh_out,"Pre-install checks failed!\n");
		return $self->set_results("preinstall_checks",-1,$start_time,time,"$log_dir/install.err","Failed with reason - $@","Pre-install checks failed!");
	} else {
		$self->Log($fh_out,"Pre-install checks are successful\n");
		$self->{install_handle}->update_install_status
			(	$self->{ip_address},
				$self->{user_name},
				"preinstall_checks",
				0,
				$start_time,
				time,
				"$log_dir/install.out",
				"",
				2,
				$job_id,
				"Pre-install checks are successful"
			);
	}

	########################
	# Get remote host OS
	########################

	eval {
				$self->Log($fh_out,"Determining the Operating System of remote host $self->{ip_address}");
				$remote_host_os = $self->get_remote_host_os;
				$self->Log($fh_out,"OS : $remote_host_os\n");
				};

	if ($@) {
		$self->Log($fh_err,"$@");
		$self->Log($fh_out,"Determining remote host OS failed!\n");
		return $self->set_results("determine_remote_host_os",-1,$start_time,time,"$log_dir/install.err","Failed with reason - $@");
	}

	########################
	# Check if build present
	########################

	eval {
				$self->Log($fh_out,"Checking if appropriate build for remote host is present in $self->{bld_tar_path}");
				$self->have_remote_host_os_build;
				};

	if ($@) {
		$self->Log($fh_err,"$@");
		$self->Log($fh_out,"Could not find a build for remote host in $self->{bld_tar_path}!\n");
		return $self->set_results("check_build_present",-1,$start_time,time,"$log_dir/install.err","Failed with reason - $@","Could not find a build for remote host in $self->{bld_tar_path}!");
	} else {
		$self->Log($fh_out,"Found a build $bld_targz_name in $self->{bld_tar_path}\n");
		$self->{install_handle}->update_install_status
			(	$self->{ip_address},
				$self->{user_name},
				"check_build_present",
				0,
				$start_time,
				time,
				"$log_dir/install.out",
				"",
				2,
				$job_id,
				"Found a build $bld_targz_name in $self->{bld_tar_path}"
			);
	}

	########################
	# Copy build
	########################

	eval {
				$self->Log($fh_out,"Copying build $bld_targz_name to remote host path $default_remote_copy_dir");
				$self->copy_build_to_remote_host;
				};

	if ($@) {
		$self->Log($fh_err,"$@");
		$self->Log($fh_out,"Could not copy build $bld_targz_name to remote host path ${default_remote_copy_dir}!\n");
		return $self->set_results("copy_build",-1,$start_time,time,"$log_dir/install.err","Failed with reason - $@","Could not copy build $bld_targz_name to remote host path ${default_remote_copy_dir}!");
	} else {
		$self->Log($fh_out,"Copied build to remote host path $default_remote_copy_dir successfully\n");
		$self->{install_handle}->update_install_status
			(	$self->{ip_address},
				$self->{user_name},
				"copy_build",
				0,
				$start_time,
				time,
				"$log_dir/install.out",
				"",
				2,
				$job_id,
				"Copied build to remote host path $default_remote_copy_dir successfully"
			);
	}

	########################
	# Install remotely
	########################

	eval {
				$self->Log($fh_out,"Installing $self->{component} remotely");
				$self->remote_install;
				};

	if ($@) {
		$self->Log($fh_err,"$@");
		$self->Log($fh_out,"Remote installation of $self->{component} failed!\n");
		return $self->set_results("remote_install",1,$start_time,time,"$log_dir/install.err","Failed with reason - $@","Remote installation of $self->{component} failed!");
	} else {
		$self->Log($fh_out,"Remote installation of $self->{component} successful\n");
		$self->{install_handle}->update_install_status
			(	$self->{ip_address},
				$self->{user_name},
				"remote_install",
				0,
				$start_time,
				time,
				"$log_dir/install.out",
				"",
				2,
				$job_id,
				"Remote installation of $self->{component} successful"
			);
	}

	########################
	# Clean up remote dir
	########################

	eval {
				$self->Log($fh_out,"Remote copy directory cleanup");
				$self->remote_cleanup;
				};

	if ($@) {
		$self->Log($fh_err,"$@");
		$self->Log($fh_out,"Remote copy directory cleanup failure!\n");
		return $self->set_results("remote_dir_cleanup",1,$start_time,time,"$log_dir/install.err","Failed with reason - $@","Remote copy directory cleanup failure!");
	} else {
		$self->Log($fh_out,"Remote copy directory cleanup successful\n");
		$self->{install_handle}->update_install_status
			(	$self->{ip_address},
				$self->{user_name},
				"remote_dir_cleanup",
				0,
				$start_time,
				time,
				"$log_dir/install.out",
				"",
				2,
				$job_id,
				"Remote copy directory cleanup successful"
			);
	}

	########################
	# Check service status
	########################

	my @services = $self->get_component_details("service");

	eval {
				$self->Log($fh_out,"Checking for service status");
				$self->check_service(@services);
				};

	if ($@) {
		$self->Log($fh_err,"$@");
		$self->Log($fh_out,"Service on remote host is not running!\n");
		return $self->set_results("check_service_on_remote_host",1,$start_time,time,"$log_dir/install.err","Failed with reason - $@","Service on remote host is not running!");
	} else {
		$self->Log($fh_out,"Service on remote host is running\n");
		$self->{install_handle}->update_install_status
			(	$self->{ip_address},
				$self->{user_name},
				"check_service_on_remote_host",
				0,
				$start_time,
				time,
				"$log_dir/install.out",
				"",
				2,
				$job_id,
				"Service on remote host is running"
			);
	}

	########################
	# Fetch agent GUID
	########################

	my $hostId;

	eval {
				$self->Log($fh_out,"Fetching agent GUID from remote host");
				$hostId = $self->get_agent_guid;
				};

	if ($@) {
		$self->Log($fh_err,"$@");
		$self->Log($fh_out,"Fetching agent GUID from remote host failed!\n");
		return $self->set_results("fetch_agent_guid",1,$start_time,time,"$log_dir/install.err","Failed with reason - $@","Fetching agent GUID from remote host failed!");
	} else {
		$self->Log($fh_out,"Successfully fetched agent GUID from remote host : ${hostId}\n");
		$self->Log($fh_out,"Installation of $self->{component} on remote host $self->{ip_address} is successful\n");

		# Update GUID in database
		$self->{install_handle}->update_host_id($hostId,$job_id);

		# Return final result set
		return $self->set_results("fetch_agent_guid",0,$start_time,time,"$log_dir/install.out","See log file $log_dir/install.out for details of the remote installation process.","Successfully fetched agent GUID from remote host : ${hostId}");
	}
}

###########################################################################
# Class Methods/APIs - Private
#
# 'whoami' => Returns caller's function name (useful in logging)
# 'debug_print_to_log' => Writes to out/err log based on debug flag
# 'initialize' => Attribute value validation and other checks
# 'is_remote_host_alive' => Check if remote host is SSH-alive
# 'verify_remote_host_key_and_passwd' => Checks for credentials
# 'get_remote_host_ssh_obj' => Creates SSH master to remote host
# 'exec_remote_cmd' => Executes remote command, returns out/err/status
# 'get_remote_host_os' => Detects remote host operating system
# 'scp_put_file' => Sends file to remote host through scp
# 'have_remote_host_os_build' => Checks if build for remote host os exists
# 'preinstall_check' => Performs checks before remote install begins
# 'copy_build_to_remote_host' => Copies build to remote host copy path
# 'remote_install' => Performs remote installation
# 'set_results' => Returns a result hashref (useful in 'run')
# 'check_service' => Checks if a service is running on remote host or not
# 'get_component_details' => Get component install cmd or service name
# 'remote_cleanup' => Clean up remote copy directory
# 'get_agent_guid" => Gets the GUID created by agent on remote host
###########################################################################

sub whoami
{
	(caller(1))[3];
}

sub debug_print_to_log
{
	my $self = shift;
	my $fh = shift;

	print $fh scalar localtime, " :: ", @_, "\n" if $self->{debug};
}

sub Log
{
	my $self = shift;
	my $fh = shift;

	print $fh scalar localtime, " :: ", @_, "\n";
}

sub initialize
{
	my $self = shift;
	my $name = $self->whoami;

	$self->debug_print_to_log($fh_out,"In $name now\n");

	# Die if mandatory options are undefined

	foreach my $key (sort keys %legalopts) {
		die "Option $key MUST have an non-empty value\n" if ( ( not defined $self->{$key} or $self->{$key} !~ /[^\s]+/ ) and $legalopts{$key} eq 'mandatory' );
	}

	# Perform some necessary checks on values of options

	my $bld_tar_path = $self->{bld_tar_path};
	my $os_details_script_path = $self->{os_details_script_path};

	$self->debug_print_to_log($fh_out,"bld_tar_path is $bld_tar_path");
	$self->debug_print_to_log($fh_out,"os_details_script_path is $os_details_script_path");

	die "os_details_script_path: Path $os_details_script_path does not exist\n" unless -d $os_details_script_path;

	die "os_details_script_path: Path $os_details_script_path does not have $os_detection_script_basename\n" unless -f "$os_details_script_path/$os_detection_script_basename";

	die "bld_tar_path: Path $bld_tar_path does not exist\n" unless -d $bld_tar_path;

	opendir my $buildpath_dh, "$bld_tar_path" or die "Could not open directory $bld_tar_path for reading\n";
	my $num_tar_gz = scalar grep /tar\.gz/, readdir $buildpath_dh;
	closedir $buildpath_dh;

	$self->debug_print_to_log($fh_out,"num_tar_gz is $num_tar_gz");

	die "bld_tar_path: Path $bld_tar_path does not have any build tar.gz files\n" if $num_tar_gz == 0;

	# openssh* rpm packages should be installed

	foreach (qw(openssh openssh-clients)) {
		chomp(my $rpmname = qx/rpm -q $_/);
		$self->debug_print_to_log($fh_out,"rpmname for $_ is $rpmname");
		die "$_: this rpm package is not installed on this machine\n" unless $rpmname =~ /$_-\d/;
	}
}

sub is_remote_host_alive
{
	my $self = shift;
	my $name = $self->whoami;
	my $host = $self->{ip_address};

	$self->debug_print_to_log($fh_out,"In $name now\n");
	$self->debug_print_to_log($fh_out,"Remote Host ping is: $host");

	my $ping_obj = Net::Ping->new();
	$ping_obj->port_number(22);
	my $success_flag = $ping_obj->ping($host);

	$self->debug_print_to_log($fh_out,"Remote Host ping status: $success_flag");

	die "SSH service on remote host $host is not reachable\n" unless $success_flag;
}

# Verify if remote host is a known host and also if password supplied is correct

sub verify_remote_host_key_and_passwd
{
	my $self = shift;
	my $name = $self->whoami;

	$self->debug_print_to_log($fh_out,"In $name now\n");

	return if $self->{known_host};

	my ($host,$user,$passwd) = @$self{qw(ip_address user_name password)};

	my $exp_addkey = new Expect;
	$exp_addkey->raw_pty(1);

	$exp_addkey = Expect->spawn("ssh $user\@$host");

	my $prompt = '[\]\$\>\#]\s*$';

	$exp_addkey->log_stdout(0);

	my $ret = $exp_addkey->expect(
		120,
		[ qr/\(yes\/no\)\?\s*$/ => sub { $exp_addkey->send_slow(0.10,"yes\n"); exp_continue_timeout; } ],
		[ qr/assword:\s*$/ => sub { $exp_addkey->send_slow(0.10,"$passwd\n"); exp_continue_timeout; } ],
		[ qr/$prompt/ => sub {} ],
		[ qr/Permission denied.*$/ => sub {} ]
	);

	if ($ret==1)
	{
		print "Host key verification for remote host $host successful\n";
	}
	elsif ($ret==2)
	{
		print "Host key verification for remote host $host already done\n";
	}
	elsif ($ret==3)
	{
		print "\n";
	}
	elsif ($ret==4)
	{
		die "Incorrect password for user $user on remote host $host\n";
	}

	$self->{known_host} = 1;
	$exp_addkey->hard_close();
}

sub get_remote_host_ssh_obj
{
	my $self = shift;
	my $name = $self->whoami;

	$self->debug_print_to_log($fh_out,"In $name now\n");

	return $self->{remote_host_ssh_obj} if defined $self->{remote_host_ssh_obj};

	my ($host,$user,$passwd) = @$self{qw(ip_address user_name password)};
	my $ssh = Net::OpenSSH->new( $host, ( user => $user, passwd => $passwd) );
	$ssh->error and die "Could not establish SSH connection to remote host $host due to error - ". $ssh->error . "\n";
	$self->{remote_host_ssh_obj} = $ssh;
}

sub exec_remote_cmd
{
	my $self = shift;
	my $cmd = shift;
	my $name = $self->whoami;

	$self->debug_print_to_log($fh_out,"In $name now\n");
	$self->debug_print_to_log($fh_out,"Command to be executed on remote host is - $cmd");

	my $ssh = $self->get_remote_host_ssh_obj;
	my ($stdout,$stderr) = $ssh->capture2( { timeout => 300 } , ($cmd) );
	my $status = $? >> 8;

	if ($ssh->error) {
		$self->debug_print_to_log($fh_out,"Command ==> $cmd had some problems");
		# warn "Execution not successful due to - " . $ssh->error if $self->{debug};
	}

	chomp($stdout,$stderr);
	return ($stdout,$stderr,$status);	
}

sub get_remote_host_os
{
	my $self = shift;
	my $name = $self->whoami;

	$self->debug_print_to_log($fh_out,"In $name now\n");

	$self->scp_put_file("$self->{os_details_script_path}/$os_detection_script_basename",$default_remote_copy_dir);
	my ($remote_host_os,undef,$status) = $self->exec_remote_cmd("sh $default_remote_copy_dir/$os_detection_script_basename 1");
	$self->debug_print_to_log($fh_out,"Status of OS_details.sh execution is $status and remote host os is $remote_host_os\n");
	die "Could not detect remote host OS\n" if $status != 0;

	$self->debug_print_to_log($fh_out,"Remote host OS is : $remote_host_os");
	return $remote_host_os;
}

sub scp_put_file
{
	my $self = shift;
	my $name = $self->whoami;

	$self->debug_print_to_log($fh_out,"In $name now\n");

	my ($src_file,$dest_file_or_dir) = @_;
	my ($host,$user,$passwd) = @$self{qw(ip_address user_name password)};
	chomp(my $scp_path = qx/which scp/);

	$self->verify_remote_host_key_and_passwd;

	my $exp_scp = new Expect;
	$exp_scp->raw_pty(1);

	$exp_scp = Expect->spawn("$scp_path $src_file $user\@$host:$dest_file_or_dir");
	$exp_scp->log_stdout(0);
	$exp_scp->debug(2);
	$exp_scp->log_file("expect_match.log","w");

	my $PROMPT = '[\]\$\>\#]\s$';
	my $ret = $exp_scp->expect(
		120,
		[ qr/assword:\s*$/ => sub { $exp_scp->send_slow(0.10,"$passwd\n"); exp_continue_timeout; } ],
		[ qr/$PROMPT/ => sub {} ]
	);

	$exp_scp->log_file(undef);

	# Sometimes due to SSH key exchange, Expect's patterns will not match
	# In that case, expect_match.log will have '100%' after a successful scp transfer

	my $expect_match_text = do { local( @ARGV, $/ ) = "/expect_match.log" ; <> } ;

	die "Problem while scp-ing $src_file to $dest_file_or_dir on remote $host\n" unless (defined $exp_scp->match_number() or $expect_match_text =~ /100%/);
}

sub have_remote_host_os_build
{
	my $self = shift;
	my $name = $self->whoami;
	my $remote_host_os = $self->get_remote_host_os;

	$self->debug_print_to_log($fh_out,"In $name now\n");

	my $bld_tar_path = $self->{bld_tar_path};

	my $build_string;
	if ($self->{component} eq "VX" or $self->{component} eq "FX")
	{
		$build_string = $self->{component};
	} else
	{
		$build_string = "UnifiedAgent";
	}

	opendir my $buildpath_dh, "$bld_tar_path" or die "Could not open directory $bld_tar_path for reading\n";

	if ($self->{component} eq "VX" or $self->{component} eq "UA")
	{
		($bld_targz_name) = grep /$build_string/, grep /$self->{bld_config}/, grep /$remote_host_os/, readdir $buildpath_dh;
	} elsif ($self->{component} eq "FX")
	{
		# There is no bld_config defined for FX agent
		($bld_targz_name) = grep /$build_string/, grep /$remote_host_os/, readdir $buildpath_dh;
	}

	closedir $buildpath_dh;

	# If UA build is not found, it should look next for an FX build
	unless ($build_string eq "UnifiedAgent" and defined $bld_targz_name)
	{
		rewinddir $buildpath_dh;
		$self->debug_print_to_log($fh_out,"Since Unified Agent build is not present in build source directory, a search will be made for FX agent build...\n");
		$self->{component} = "FX";
		$build_string = $self->{component};

		opendir my $buildpath_dh, "$bld_tar_path" or die "Could not open directory $bld_tar_path for reading\n";
		($bld_targz_name) = grep /$build_string/, grep /$remote_host_os/, readdir $buildpath_dh;
		closedir $buildpath_dh;
	}

	$self->debug_print_to_log($fh_out,"For component $self->{component}, bld_targz_name is $bld_targz_name");

	die "Path $bld_tar_path does not have the required build for component $self->{component}\n" unless defined $bld_targz_name;
}

sub preinstall_check
{
	my $self = shift;
	my $name = $self->whoami;

	$self->debug_print_to_log($fh_out,"In $name now\n");

	my $host = $self->{ip_address};

	if ( $self->{component} ne 'UA' )
	{
		my (undef,undef,$status) = $self->exec_remote_cmd("rpm -qa | grep -i InMage" . $self->{component});
		die "Remote host $host has $self->{component} already installed\n" if $status == 0;
	} else
	{
		my (undef,undef,$status_fx_installed) = $self->exec_remote_cmd("rpm -qa | grep -i InMageFx");
		my (undef,undef,$status_vx_installed) = $self->exec_remote_cmd("rpm -qa | grep -i InMageVx");
		die "Remote host $host has $self->{component} already installed\n" if $status_fx_installed == 0 or $status_vx_installed == 0;
	}
}

sub copy_build_to_remote_host
{
	my $self = shift;
	my $name = $self->whoami;

	$self->debug_print_to_log($fh_out,"In $name now\n");
	$self->scp_put_file("$self->{bld_tar_path}/$bld_targz_name",$default_remote_copy_dir);
}

sub remote_install
{
	my $self = shift;
	my $name = $self->whoami;

	$self->debug_print_to_log($fh_out,"In $name now\n");
	$self->debug_print_to_log($fh_out,"bld_targz is $bld_targz_name");

	my $host = $self->{ip_address};

	$self->{cmd} = $self->get_component_details("cmd");

	my ($stdout,$stderr,$status) = $self->exec_remote_cmd($self->{cmd});

	$self->debug_print_to_log($fh_out,"\n","\n");
	$self->debug_print_to_log($fh_out,"\n",$stdout,"\n");
	$self->debug_print_to_log($fh_err,"\n",$stderr,"\n") if $stderr =~ /\w+/;

	if ($status) {
		&writeout("Remote component installation failed on host $host");
		die "Remote component installation failed on host $host\n";
	}
}

sub set_results
{
	my $self = shift;
	my $name = $self->whoami;

	$self->debug_print_to_log($fh_out,"In $name now\n");

	my %results = ();
	$results{ip_address} = $self->{ip_address};
	$results{user_name} = $self->{user_name};
	$results{cmd} = shift;
	$results{exit_status} = shift;
	$results{start_time} = shift;
	$results{end_time} = shift;
	$results{log_file_location} = shift;
	$results{log_details} = shift;
	$results{status_message} = shift;
	$results{job_id} = $self->{job_id};
	return \%results;
}

sub check_service
{
	my $self = shift;
	my $name = $self->whoami;
	my @services = @_;
	my $cmd;

	$self->debug_print_to_log($fh_out,"In $name now\n");

	foreach (@services) {
		$self->debug_print_to_log($fh_out,"Checking whether service $_ is running");

		if ($remote_host_os =~ /HP-UX/)
		{
			$cmd = "/sbin/init.d/$_ status";
			$ENV{PATH} = $ENV{PATH} . ":/usr/contrib/bin";
		} else
		{
			$cmd = "/etc/init.d/$_ status";
		}

		my (undef,undef,$status) = $self->exec_remote_cmd($cmd);
		if ($status) {
			$self->debug_print_to_log($fh_out,"service $_ is not running");
			die "service $_ is not running\n";
		} else {
			$self->debug_print_to_log($fh_out,"service $_ running");
		}
	}
}

sub get_component_details
{
	my $self = shift;
	my $mode = shift;
	my $name = $self->whoami;
	my $component = $self->{component};

	my ($bld_tar) = ($bld_targz_name =~ /(.+)[.]gz/);

	my $cmd_common_part = "cd $default_remote_copy_dir ; gunzip $bld_targz_name ; tar xvf $bld_tar ; sh install -d $self->{installation_dir} -i $self->{cx_ip} -p $self->{cx_port} -s $self->{start_agent}";

	die "Inappropriate argument passed to ${name}\n" if ( $mode ne "cmd" and $mode ne "service");
	die "Don't know how to handle component ${component}\n" if ( $component ne "FX" and $component ne "VX" and $component ne "UA");

	$self->debug_print_to_log($fh_out,"In $name now\n");
	$self->debug_print_to_log($fh_out,"Getting details on install command or service name for $component");

	if ($mode eq "cmd") {
		if ($component eq "VX") {
			return "$cmd_common_part -a $self->{installation_mode}";
		} elsif ($component eq "UA") {
			return "$cmd_common_part -a $self->{installation_mode} -t both";
		} elsif ($component eq "FX") {
			return "$cmd_common_part";
		}
	} elsif ($mode eq "service") {
		if ($component eq "VX") {
			return ("vxagent");
		} elsif ($component eq "FX") {
			return ("svagent");
		} elsif ($component eq "UA") {
			return ("vxagent","svagent");
		}
	}
}

sub remote_cleanup
{
	my $self = shift;
	my $name = $self->whoami;

	my $cmd = "cd $default_remote_copy_basedir ; rm -rf $this_session_dir ";
	my (undef,undef,$status) = $self->exec_remote_cmd($cmd);
	die "Remote cleanup failed for path ${default_remote_copy_dir}\n" if $status != 0;
}

sub get_agent_guid
{
	my $self = shift;
	my $name = $self->whoami;
	my $component = $self->{component};
	my $cmd;

	if ($component eq "VX" or $component eq "UA") {
		$cmd = "grep 'HostId' $self->{installation_dir}/Vx/etc/drscout.conf | awk '{print \$3}'";
	} else {
		$cmd = "grep 'HostId' $self->{installation_dir}/Fx/config.ini | awk '{print \$3}'";
	}

	my ($hostId,undef,$status) = $self->exec_remote_cmd($cmd);
	die "Could not detect the host Id of agent on remote host\n" if $status;
	return $hostId;
}

1;
