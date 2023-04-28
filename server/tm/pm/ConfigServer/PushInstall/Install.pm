#$Header: /src/server/tm/pm/ConfigServer/PushInstall/Install.pm,v 1.2 2015/11/25 13:44:08 prgoyal Exp $
#=================================================================
# FILENAME
#   Install.pm
#
# DESCRIPTION
#    This perl module is responsible for remote installation of 
#    agents from the CX UI.
#
# HISTORY
#     05 Mar 2009  bknathan    Created.
#
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================
package ConfigServer::PushInstall::Install;

use DBI;
use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use Utilities;

#
# Constructor. Create a new object of the class and pass
# all required args from outside.
#
sub new 
{
   my ($class, %args) = @_;
   
   my $self = {};
   $self->{'dbh'} = $args{'dbh'};
   $self->{'dbh'}->{'mysql_auto_reconnect'} = 1;
   $self->{'install_pending'} = 1;
   $self->{'install_in_progress'} = 2;
   $self->{'install_completed'} = 3;
   $self->{'install_failed'} = -3;
   $self->{'amethyst_vars'} = Utilities::get_ametheyst_conf_parameters();

   bless ($self, $class);
}

#
# Check if there are any agent installers that
# need to be pushed to a remote machine
#
sub check_new_installers
{

   my $self = shift;

   #
   # Check if there is anything to install. Pick up everything
   # where state = 1 (install_pending)
   #
   my $sql = "SELECT 
		    a.ipaddress, 
		    a.agentInstallerLocation,
		    a.jobId
		FROM 
		    agentInstallerDetails a
		WHERE 
		    state = $self->{'install_pending'} 
		AND 
		    (select b.osType from agentInstallers b where b.ipaddress = a.ipaddress) = 2";
   my $sth = $self->{'dbh'}->prepare($sql) or die ("Unable to prepare $sql: $!\n");
   $sth->execute() or die ("Unable to execute $sql: $!\n");
   my $rows = $sth->rows;

   if ($rows != 0)
   {
       while (my ($ipaddress, $agent_installer_location, $job_id) = $sth->fetchrow_array())
       {
             $self->log("New installer available for $ipaddress\n");
	     $self->log(" -- location: $agent_installer_location\n");
   	     $self->log(" -- Job id : $job_id\n");
	     $self->log("-" x 20 ."\n");

	     my ($ipaddress, $username, $password) = $self->get_install_host_privs($ipaddress);

	     eval
             {
	       	   $self->push_install(
				       $ipaddress,
 				       $username, 
				       $password, 
	    			       $agent_installer_location,
				       $job_id);
             };
	     if ($@)
 	     {
	       	   $self->log("Error occurred during push installation: $@\n");
	     }
	     else
	     {
		   $self->log("Installation of $agent_installer_location on $username\@$ipaddress successfull\n");
	     }
       }
   }
}

#
# Get the privileges for the installer to push and 
# install the required binaries on the remote node.
#
sub get_install_host_privs
{
    my $self = shift;
    my $ipaddress = shift;

    my $sql  = "SELECT 
			ipaddress, 
			username, 
			password 
		FROM 
			agentInstallers 
		WHERE 
			ipaddress = \'$ipaddress\'
		AND 
			osType = 2";

    my $sth_get_privs  = $self->{'dbh'}->prepare($sql)
				|| die "Unable to prepare $sql: $!\n";
    $sth_get_privs->execute() || die "Unable to execute $sql\n";
    my $ref = $sth_get_privs->fetchrow_hashref();
 
    return ($ref->{'ipaddress'}, $ref->{'username'}, $ref->{'password'});   
}

#
# Update the installation status in DB. This will be visible
# to the user.
#
sub update_install_status
{
   my $self = shift;
   my ($ipaddress, $username, $command, $exit_status, $start_time, $end_time, $log_file_location, $log_details, $state,$job_id, $status_message) = @_;
   
   my $sql = "UPDATE 
		agentInstallerDetails 
	      SET  
		lastUpdatedTime = now(),
		state = $state,
		startTime = \'$start_time\', 
		endTime = \'$end_time\', 
		logFileLocation = \'$log_file_location\',
		logFile = \'$log_details\',
		statusMessage = \'$status_message\'
	      WHERE
		jobId = $job_id";
   print "SQL : $sql\n";
   my $sth_update = $self->{'dbh'}->prepare($sql) || die "Unable to execute $sql\n";
   $sth_update->execute() || die "Unable to execute $sql\n";
   my $rows = $sth_update->rows();

   if ($rows != 0)
   {
	$self->log("Updated $rows rows\n");
   }
}

#
# Run the remote installer from here
#
sub push_install
{
   my $self = shift;
   
   my ($ipaddress, $username, $password, $agent_installer, $job_id)  = @_;

   my $log_path = $self->{'amethyst_vars'}->{'INSTALLATION_DIR'}."/var/push_install_logs/";

   #
   # Call the agent installer push module from here
   #
   my $push_install_args =  {
			       'ip_address' => $ipaddress, 
			       'user_name' => $username, 
			       'password' => $password, 
			       'bld_tar_path' => $agent_installer,
			       'log_path' => $log_path,
			       'debug' => 1,
			       'cx_ip' => $self->{'amethyst_vars'}->{'CS_IP'},
			       'cx_port' => $self->{'amethyst_vars'}->{'CS_PORT'},
			       'install_handle' => $self
			    };


   #
   # Declare all vars used for updating install status
   # in DB.
   #
   my ($exit_status, $username, $start_time, $end_time, $state);
   my ($log_file_location, $log_details, $ip_address, $command,$status_message); 
   
   $start_time = Utilities::unixTimestampToDatetime(time());

   #
   # Mark the install status as in progress. This is not 
   # the right way to set an install status. Ideally, the 
   # run command should fork off a process and the install
   # status should be poll based.
   # 			
   $self->update_install_status(    $ipaddress,
                                    $username,
                                    "",
                                    "",
                                    $start_time,
                                    "",
                                    "",
				    "", 
                                    $self->{'install_in_progress'},
				    $job_id,
				    "Starting"); 

   #
   # Run the push installer now.
   # 
   if ($self->{'amethyst_vars'}->{'PUSH_INSTALL_ENABLED'} == 1)
   {
	use if $self->{'amethyst_vars'}->{'PUSH_INSTALL_ENABLED'}, "ConfigServer::PushInstall::RemoteInstall";
	my $push_install_obj = new ConfigServer::PushInstall::RemoteInstall($push_install_args);
        my $command_status = $push_install_obj->run($job_id);

        $exit_status = $command_status->{'exit_status'};
        $username = $command_status->{'user_name'};
        $start_time = Utilities::unixTimestampToDatetime($command_status->{'start_time'});
        $end_time = Utilities::unixTimestampToDatetime($command_status->{'end_time'});
	$log_file_location = $command_status->{'log_file_location'};
        $log_details = $command_status->{'log_details'};
        $ip_address = $command_status->{'ip_address'};
        $command = $command_status->{'cmd'};
        $job_id  = $command_status->{'job_id'};
        $status_message = $command_status->{'status_message'};
   } 
   else
   {
 	$self->update_install_status(    $ipaddress,
                                    $username,
                                    "",
                                    "",
                                    $start_time,
                                    "",
                                    "",
                                    "",
                                    $self->{'install_failed'},
                                    $job_id,
                                    "Push Install not enabled on this host");
	
   }

   if ($exit_status == 0)
   {
	$state = $self->{'install_completed'};
        $self->log("Executed command: $command successfully\n");
   }
   else
   {
        $state = $self->{'install_failed'};
	$self->log("Error occurred while running command: $command\n");
   }

   #
   # Update the installation status in DB 
   #
   $self->update_install_status(  $ipaddress, 
			          $username, 
                                  $command, 
                                  $exit_status, 
				  $start_time, 
				  $end_time, 
				  $log_file_location, 
				  $log_details,
				  $state,
				  $job_id,
				  $status_message);
}

#
# Update the host guid of the installed 
# host
# 
sub update_host_id
{
   my ($self) = shift;
   my ($host_id, $job_id) = @_;

   my $sql = "UPDATE 
		  agentInstallerDetails
              SET 
	     	 hostId = \'$host_id\'
	      WHERE 
		 jobId = \'$job_id\'";
   my $sth = $self->{'dbh'}->prepare($sql) 
		|| die "Unable to prepare $sql: $!\n";
   $sth->execute() || die "Unable to execute $sql: $!\n";
   my $rows = $sth->rows;

   if ($rows > 0)
   {
	$self->log("Updated $host_id for $job_id in DB successfully\n");
   }
}

#
# Log everything to push_install.log
#
sub log
{
     my $self = shift;
     my $message = shift;

     open (FILEH, ">> $self->{'INSTALLATION_DIR'}/var/push_install.log") 
		|| die "Error opening push install log for writing: $!\n";
     print FILEH $message;
     close (FILEH);
}

1;
