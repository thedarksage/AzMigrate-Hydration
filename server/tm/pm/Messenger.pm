# Package for sending email alerts to replication administrators
package Messenger;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use DBI;
use Net::SMTP;
use Utilities;
use tmanager;
use POSIX;
use TimeShotManager;
use HTTP::Date;
use Common::DB::Connection;
use Common::Log;
use Common::Constants;
use Data::Dumper;
use PHP::Serialization qw(serialize unserialize);

# Package version
our $VERSION                    = "0.1";

# Package constants
my $PACKAGE_NAME                = "Messenger";

my $EMAIL_SERVER                = "localhost";
my $EMAIL_SENDER                = "DPS\@inmage.net";
my $EMAIL_RECEIVER              = "admin\@inmage.net";
my $EMAIL_FULL_SENDER           = "DP Appliance Error Messenger <$EMAIL_SENDER>";
my $logging_obj = new Common::Log(); 


my $EMAIL_SUBJECT               = "Failure on Replication Pair: ";

my $SOURCE                      = "MESSAGE_SOURCE";

# Database tables and fields
my $HOSTS_TBL                   = "hosts";
my $HOSTS_ID                    = "id";
my $HOSTS_NAME                  = "name";

my $PAIR_TBL                    = "srcLogicalVolumeDestLogicalVolume";
my $PAIR_SRC_ID                 = "sourceHostId";
my $PAIR_DEST_ID                = "destinationHostId";
my $PAIR_SRC_DEV                = "sourceDeviceName";
my $PAIR_DEST_DEV               = "destinationDeviceName";
my $PAIR_PAIR                   = "ruleId";

my $USER_TBL                    = "users";
my $USER_ID                     = "PID";
my $USER_EMAIL                  = "email";

my $USER_R_TBL                  = "userErrorReceivers";
my $USER_R_ID                   = "PID";
my $USER_R_PAIR                 = "pairId";
my $USER_R_EMAIL                = "sendEmail";

# "As" names for queries
my $SRC_HOST                    = "srcHost";
my $SRC_DEV                     = "srcDev";
my $DEST_HOST                   = "destHost";
my $DEST_DEV                    = "destDev";

my $SRC_HOST_NAME               = "srcHostName";
my $SRC_DEV_NAME                = "srcDevName";
my $DEST_HOST_NAME              = "destHostName";
my $DEST_DEV_NAME               = "destDevName";

my $ADDRESS                     = "address";

# DB Queries
my $DEVICE_SELECT               = "SELECT a.$HOSTS_NAME as $SRC_HOST, p.$PAIR_SRC_DEV AS $SRC_DEV, b.$HOSTS_NAME as $DEST_HOST, p.$PAIR_DEST_DEV as $DEST_DEV";
my $DEVICE_FROM                 = "FROM $PAIR_TBL as p, $HOSTS_TBL as a, $HOSTS_TBL as b";
my $DEVICE_WHERE                = "WHERE p.$PAIR_SRC_ID=a.$HOSTS_ID AND p.$PAIR_DEST_ID=b.$HOSTS_ID AND p.$PAIR_PAIR="; #suppliment with pair ID

my $DEVICE_QUERY                = "$DEVICE_SELECT $DEVICE_FROM $DEVICE_WHERE"; # + "\"obj->{$MSGO_pairId}\";"

my $MAIL_SELECT                 = "SELECT u.$USER_EMAIL as $ADDRESS";
my $MAIL_FROM                   = "FROM $USER_TBL as u, $USER_R_TBL as r";
my $MAIL_WHERE                  = "WHERE u.$USER_ID=r.$USER_R_ID AND r.$USER_R_EMAIL=\"1\" AND r.$USER_R_PAIR=";

my $MAIL_QUERY                  = "$MAIL_SELECT $MAIL_FROM $MAIL_WHERE"; # + "\"obj->{$MSGO_pairId}\";"

##
# Messenger Object members
##
my $MSGO_dbh                    = "MSGO_dbh";
my $MSGO_host                   = "MSGO_host";
my $MSGO_queue                  = "MSGO_queue";
my $MSGO_conn                   = "MSGO_conn";
my $MSGO_sender                 = "MSGO_sender";
my $MSGO_receiver               = "MSGO_receiver";
my $MSGO_full_sender            = "MSGO_full_sender";
my $MSGO_bpm_sender             = "MSGO_bpm_sender";
my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
my $IN_APPLABEL = $AMETHYST_VARS->{IN_APPLABEL};

my $this_node_host_id = $AMETHYST_VARS->{HOST_GUID};
my $cx_ip = $AMETHYST_VARS->{CS_IP};
my $cs_installation_path = Utilities::get_cs_installation_path();
my $is_windows = Utilities::isWindows();

# Trap listeners
my $SNMP_TRAP 			= ($is_windows) ? "snmptrapagent" : "snmptrap";
my $TRAP_COMMUNITY      = "public";
my $ABHAI_MIB           = "Abhai-1-MIB-V1";
my $GUID_FILLER         = "";
my $MY_AMETHYST_GUID    = $AMETHYST_VARS->{HOST_GUID};
my $NULL_GUID           = $AMETHYST_VARS->{HOST_GUID};
my $TRAP_LISTENER_IPv4  = "trapListenerIPv4";

# TRAP: OBJECT-TYPE
my $TRAP_AMETHYSTGUID;
my $TRAP_HOSTGUID;
my $TRAP_VOLUMEGUID;
my $TRAP_AGENT_ERROR_MESSAGE;
my $TRAP_AGENT_LOGGED_ERROR_LEVEL;
my $TRAP_AGENT_LOGGED_ERROR_MESSAGE_TIME;
my $TRAP_FILEREP_CONFIG;
my $TRAP_CS_HOSTNAME;
my $TRAP_PS_HOSTNAME;
my $TRAP_SEVERITY;
my $TRAP_SOURCE_HOSTNAME;
my $TRAP_SOURCE_DEVICENAME;
my $TRAP_TARGET_HOSTNAME;
my $TRAP_TARGET_DEVICENAME;
my $TRAP_RETENTION_DEVICENAME;
my $TRAP_AFFTECTED_SYSTEM_HOSTNAME;

# TRAP: TRAP-TYPE
my $TRAP_VX_NODE_DOWN;
my $TRAP_FX_NODE_DOWN;
my $TRAP_PS_NODE_DOWN;
my $TRAP_APP_NODE_DOWN;
my $TRAP_SECONDARY_STORAGE_UTILIZATION_DANGER_LEVEL;
my $TRAP_VX_RPOSLA_THRESHOLD_EXCEEDED;
my $TRAP_FX_RPOSLA_THRESHOLD_EXCEEDED;
my $TRAP_PS_RPOSLA_THRESHOLD_EXCEEDED;
my $TRAP_FILEREP_PROGRESS_THRESHOLD_EXCEEDED;
my $TRAP_BITMAP_MODE_CHANGE;
my $TRAP_SENTINEL_RESYNC_REQUIRED;
my $TRAP_AGENT_LOGGED_ERROR_MESSAGE;
my $TRAP_DISK_SPACE_THRESHOLD_ON_SWITCH_BP_EXCEEDED;
my $TRAP_PROCESS_SERVER_UNINSTALLATION;
my $TRAP_BANDWIDTH_SHAPING;
my $TRAP_VERSION_MISMATCH;
my $TRAP_CX_NODE_FAILOVER;
my $TRAP_CSUP_AND_REUUNING;

if ($is_windows)
{
	# OBJECT-TYPE
	$TRAP_AMETHYSTGUID                     = "1.3.6.1.4.1.17282.1.14.1"; 
	$TRAP_HOSTGUID                         = "1.3.6.1.4.1.17282.1.14.5";
	$TRAP_VOLUMEGUID                       = "1.3.6.1.4.1.17282.1.14.10";
	$TRAP_AGENT_ERROR_MESSAGE              = "1.3.6.1.4.1.17282.1.14.50";
	$TRAP_AGENT_LOGGED_ERROR_LEVEL	       = "1.3.6.1.4.1.17282.1.14.55";
	$TRAP_AGENT_LOGGED_ERROR_MESSAGE_TIME  = "1.3.6.1.4.1.17282.1.14.60";
	$TRAP_FILEREP_CONFIG                   = "1.3.6.1.4.1.17282.1.14.65";
	$TRAP_CS_HOSTNAME                      = "1.3.6.1.4.1.17282.1.14.70";
	$TRAP_PS_HOSTNAME                      = "1.3.6.1.4.1.17282.1.14.75";
	$TRAP_SEVERITY                         = "1.3.6.1.4.1.17282.1.14.80";
	$TRAP_SOURCE_HOSTNAME                  = "1.3.6.1.4.1.17282.1.14.85";
	$TRAP_SOURCE_DEVICENAME                = "1.3.6.1.4.1.17282.1.14.90";
	$TRAP_TARGET_HOSTNAME                  = "1.3.6.1.4.1.17282.1.14.95";
	$TRAP_TARGET_DEVICENAME                = "1.3.6.1.4.1.17282.1.14.100";
	$TRAP_RETENTION_DEVICENAME             = "1.3.6.1.4.1.17282.1.14.105";
	$TRAP_AFFTECTED_SYSTEM_HOSTNAME        = "1.3.6.1.4.1.17282.1.14.110";
		
	# TRAP-TYPE
	$TRAP_VX_NODE_DOWN = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.240";
	$TRAP_FX_NODE_DOWN = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.235";
	$TRAP_PS_NODE_DOWN = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.245";
	$TRAP_APP_NODE_DOWN = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.230";
	$TRAP_SECONDARY_STORAGE_UTILIZATION_DANGER_LEVEL = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.260";
	$TRAP_VX_RPOSLA_THRESHOLD_EXCEEDED = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.280";
	$TRAP_FX_RPOSLA_THRESHOLD_EXCEEDED = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.285";
	$TRAP_PS_RPOSLA_THRESHOLD_EXCEEDED = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.290";
	$TRAP_FILEREP_PROGRESS_THRESHOLD_EXCEEDED = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.385";
	$TRAP_BITMAP_MODE_CHANGE = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.435";
	$TRAP_SENTINEL_RESYNC_REQUIRED = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.215";
	$TRAP_AGENT_LOGGED_ERROR_MESSAGE = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.340";
	$TRAP_DISK_SPACE_THRESHOLD_ON_SWITCH_BP_EXCEEDED = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.460";
	$TRAP_PROCESS_SERVER_UNINSTALLATION = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.380";
	$TRAP_BANDWIDTH_SHAPING = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.375";
	$TRAP_VERSION_MISMATCH	= "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.440";
	$TRAP_CX_NODE_FAILOVER	= "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.370";
	$TRAP_CSUP_AND_REUUNING = "1.3.6.1.6.3.1.1.4.1.0::o::1.3.6.1.4.1.17282.1.14.0.450";
}
else
{
	# OBJECT-TYPE
	$TRAP_AMETHYSTGUID                     = "trapAmethystGuid";
	$TRAP_HOSTGUID                         = "trapHostGuid";
	$TRAP_VOLUMEGUID                       = "trapVolumeGuid";
	$TRAP_AGENT_ERROR_MESSAGE              = "trapAgentErrorMessage";
	$TRAP_AGENT_LOGGED_ERROR_LEVEL	       = "trapAgentLoggedErrorLevel";
	$TRAP_AGENT_LOGGED_ERROR_MESSAGE_TIME  = "trapAgentLoggedErrorMessageTime";	
	$TRAP_FILEREP_CONFIG                   = "trapFilerepConfig";
	$TRAP_CS_HOSTNAME                      = "trapCSHostname";
	$TRAP_PS_HOSTNAME                      = "trapPSHostname";
	$TRAP_SEVERITY                         = "trapSeverity";
	$TRAP_SOURCE_HOSTNAME                  = "trapSourceHostName";
	$TRAP_SOURCE_DEVICENAME                = "trapSourceDeviceName";
	$TRAP_TARGET_HOSTNAME                  = "trapTargetHostName";
	$TRAP_TARGET_DEVICENAME                = "trapTargetDeviceName";
	$TRAP_RETENTION_DEVICENAME             = "trapRetentionDeviceName";
	$TRAP_AFFTECTED_SYSTEM_HOSTNAME        = "trapAffectedSystemHostName";
	
	# TRAP-TYPE
	$TRAP_VX_NODE_DOWN                               = "trapVxAgentDown";
	$TRAP_FX_NODE_DOWN                               = "trapFxAgentDown";
	$TRAP_PS_NODE_DOWN                               = "trapPSNodeDown";
	$TRAP_APP_NODE_DOWN                              = "trapAppAgentDown";
	$TRAP_SECONDARY_STORAGE_UTILIZATION_DANGER_LEVEL = "trapSecondaryStorageUtilizationDangerLevel";
	$TRAP_VX_RPOSLA_THRESHOLD_EXCEEDED               = "trapVXRPOSLAThresholdExceeded";
	$TRAP_FX_RPOSLA_THRESHOLD_EXCEEDED               = "trapFXRPOSLAThresholdExceeded";
	$TRAP_PS_RPOSLA_THRESHOLD_EXCEEDED               = "trapPSRPOSLAThresholdExceeded";
	$TRAP_FILEREP_PROGRESS_THRESHOLD_EXCEEDED        = "trapFXJobError";
	$TRAP_BITMAP_MODE_CHANGE                         = "trapbitmapmodechange";
	$TRAP_SENTINEL_RESYNC_REQUIRED                   = "trapSentinelResyncRequired";
	$TRAP_AGENT_LOGGED_ERROR_MESSAGE                 = "trapAgentLoggedErrorMessage";
	$TRAP_DISK_SPACE_THRESHOLD_ON_SWITCH_BP_EXCEEDED = "trapDiskSpaceThresholdOnSwitchBPExceeded";
	$TRAP_PROCESS_SERVER_UNINSTALLATION 			 = "ProcessServerUnInstallation";
	$TRAP_BANDWIDTH_SHAPING                          = "BandwidthShaping";
	$TRAP_VERSION_MISMATCH							 = "trapVersionMismatch";
	$TRAP_CX_NODE_FAILOVER							 = "CXNodeFailover";
	$TRAP_CSUP_AND_REUUNING							 = "trapCSUpAndRunning";
}



##
#
# Function   : new
#
# Purpose    : Returns a new Messenger object. The object manages
#              communication of error messages from system devices
#              to system administrators
#
# Parameters : $host - Hostname of the transband box
#
##
sub new ($)
{

  my ($host) = @_;
  my $messenger_object = {};
   # Bless this object that it may serve us well
  bless ($messenger_object, $PACKAGE_NAME);

  # Amen
  return $messenger_object;
  
}
  
 

##
#
# Function   : send_message
#
# Purpose    : Sends a message to all administrators set to
#              receive traps from the replication pair linked
#              to this object.
#
# Parameters : $obj         - This object
#              $source      - String describing the source of the message
#              $error_level - The error level of the message
#              $subject     - Subject for the message
#              $message     - The message to be sent
#
# Notes      : Will be expanded to automatically send messages
#              to SMS phones and pagers later.
#
##
sub send_message ($$$$$)
{
  my ($obj, $source, $error_level, $subject, $message) = @_;
  
  #print "Received error:\n";
  #print "src: $source\n";
  #print "lvl: $error_level\n";
  #print "msg: $message\n";
  
  if (!exists $obj->{$MSGO_queue}{$source})
  {	
    $obj->{$MSGO_queue}{$source} = +{};
  }
  if (!exists $obj->{$MSGO_queue}{$source}{$subject})
  {	
    $obj->{$MSGO_queue}{$source}{$subject} = +{};
  }
  if (!exists $obj->{$MSGO_queue}{$source}{$subject}{$error_level})
  {	
    $obj->{$MSGO_queue}{$source}{$subject}{$error_level} = [];
  }
  push (@{$obj->{$MSGO_queue}{$source}{$subject}{$error_level}}, $message) 
  if($#{$obj->{$MSGO_queue}{$source}{$subject}{$error_level}} == -1); 
}

##
#
# Function   : send_all_messages
#
# Purpose    : Sends all queued messages
#
# Parameters : $obj - This object
#
##
sub send_all_messages ($)
{
    my ($obj) = @_;		
	my $ENCRYPTION_KEY= Utilities::get_cs_encryption_key();
    eval
    {
        my $conn = new Common::DB::Connection();
        #Check whether cx is in maintenance mode
        my $cx_maintenance_mode = TimeShotManager::cx_maintenance_mode($conn);
		my @encryption_key_arr = ($ENCRYPTION_KEY);
		my $encryption_key_ref = \@encryption_key_arr;

		if(! $ENCRYPTION_KEY)
		 {     
		  $logging_obj->log("EXCEPTION","Encryption key not found");
		  die "Encryption key not found";
		 }
		
        if($cx_maintenance_mode ne '1')
        {
            my $body_delimiter = "-------------------------------------------".
                                "-------------------------------------------<br><br>";

				my $query = "SELECT 
								UID, 
								if (ISNULL(email) OR email = '', email, AES_DECRYPT(UNHEX(email), ?)) as email,
								dispatch_interval,
								last_dispatch_time,
								emailSubject,
								testEmail,
								uname
							FROM 
								users";
			   
				my $sth = $conn->sql_query($query, $encryption_key_ref);
				my $uid;
				my $email_address;
				my $dispatch_interval;
				my $last_dispatch_time;
				my $mailSubject;
				my $test_email;
				my $user_name;
		
			my @param_array = (\$uid, \$email_address, \$dispatch_interval, \$last_dispatch_time, \$mailSubject, \$test_email, \$user_name);
			$conn->sql_bind_columns($sth,@param_array);
            while ($conn->sql_fetch($sth))
            {
				if($mailSubject eq "")
                {
                    $mailSubject = $IN_APPLABEL." Alert";
                    $mailSubject = TimeShotManager::customerBrandingNames($mailSubject);
                }
				
				if($test_email eq '1')
				{
					my $test_mail_body = "User name     : ".$user_name."<br/>";
					my $test_mail_subject = "Test Mail:: This is an automated message sent by";
					
					if($obj->send_email_message($email_address,$test_mail_subject,$test_mail_body))
					{
						my $users_query = "UPDATE 
										users 
									SET 
										testEmail = '0' 
									WHERE 
										UID='$uid'";
					
						my $users_sth = $conn->sql_query($users_query);
					 
						$conn->sql_finish($users_sth);
					}
				}

                my $current_time = time();

                if (($current_time > $last_dispatch_time + ($dispatch_interval*60))	|| !$last_dispatch_time)
                {
                    my $email_body = "";
                    my $send_email = 0;
                    my $sql2 = "SELECT 
                                    error_template_id 
                                FROM 
                                    error_policy 
                                WHERE 
                                    user_id='$uid' AND 
                                    send_mail='1'";
                   
            		my $result2 = $conn->sql_exec($sql2);
                    foreach my $ref2(@{$result2})
                    {
                        my $template_id = $ref2->{'error_template_id'};
                        my $sql3 = "SELECT 
                                        summary, 
                                        message, 
                                        message_count,
                                        first_update_time, 
                                        last_update_time 
                                    FROM 
                                        error_message 
                                    WHERE 
                                        user_id='$uid' AND 
                                        error_template_id='$template_id'";
                       
                		my $result3 = $conn->sql_exec($sql3);
                        foreach my $ref3(@{$result3})
                        {
                            my $summary = $ref3->{'summary'};
                            my $message = $ref3->{'message'};
                            my $count = $ref3->{'message_count'};
                            my $first_update_time = $ref3->{'first_update_time'};
                            my $last_update_time = $ref3->{'last_update_time'};
                            if ($message ne "")
                            {
                                if ($summary ne "") 
                                {
                                    $email_body .= "<b>".$summary.":</b><br><br>";
                                }		
                                $email_body .= $message."<br>";
                                $email_body .= "Number of occurrences: ".$count." time(s) between ".$first_update_time." and ".$last_update_time."<br>";
                                $email_body .= $body_delimiter;       
                                $send_email = 1;
                            }
                        }
                    }
                 
                    if ($send_email)
                    {												
                        if($obj->send_email_message($email_address,$mailSubject,$email_body))
                        {
                            my $sql6 = "DELETE FROM
                                            error_message 
                                        WHERE 
                                            user_id='$uid'";
                           
                         
                            my $sth6 = $conn->sql_query($sql6);
                            
                            $conn->sql_finish($sth6);
                        }
                        else
                        {
                            #Mail server down
                        }

                        my $sql5 = "UPDATE 
                                        users 
                                    SET 
                                        last_dispatch_time='$current_time' 
                                    WHERE 
                                        UID='$uid'";
                    
                        my $sth5 = $conn->sql_query($sql5);
                     
                        $conn->sql_finish($sth5);
                    }
                }
            }
       
            #
            # For Protection Report Alert
            #
            my $sql1 = "SELECT 
                            a.UID, 
                            if (ISNULL(a.email) OR a.email = '', a.email, AES_DECRYPT(UNHEX(a.email), ?)) as email,
                            b.dispatch_interval, 
                            b.last_dispatch_time,
                            UNIX_TIMESTAMP(CURDATE()) as cur_date_ts,
                            CURDATE() as cur_date,
                            b.schecule_time
                        FROM 
                            users a, 
                            error_policy b
                        WHERE 
                            a.UID = b.user_id AND 
                            b.error_template_id = 'PROTECT_REP' AND 
                            b.send_mail = '1'";
            
            my $sth1 = $conn->sql_query($sql1, $encryption_key_ref);
			my $uid;
			my $email_address;
			my $dispatch_interval;
			my $last_dispatch_time;
			my $cur_date_ts;			
			my $current_date;
			my $schecule_time;
			
			my @params_array = (\$uid, \$email_address, \$dispatch_interval, \$last_dispatch_time, \$cur_date_ts, \$current_date, \$schecule_time);
			$conn->sql_bind_columns($sth1,@params_array);
			while ($conn->sql_fetch($sth1))           
            {
				if(!$schecule_time)
                {
                    $schecule_time = "00:00";
                }			
                my $cur_schecule_time = "'$current_date"." 00:00:00' , '$schecule_time'";			
                
                my $get_time_sql = "SELECT UNIX_TIMESTAMP(NOW()) as cur_time,UNIX_TIMESTAMP(ADDTIME($cur_schecule_time)) as cur_schecule_time";
                my $get_time_sql_rs = $conn->sql_exec($get_time_sql);
            	
				my @value = @$get_time_sql_rs;

				my $current_time = $value[0]{'cur_time'};
				my $latest_dispatch_time = $value[0]{'cur_schecule_time'};
				
				my $generate_rep = 1;
				my $email_body = '';
				
				if (($current_time >= $last_dispatch_time + 86400) || !$last_dispatch_time)
				{
					#
					# Generate Protection Report through PHP
					#
					if($generate_rep == 1)
					{
						my $no_of_days = $dispatch_interval ;
						my $sql2 = "SELECT 
										sourceHostId, 
										sourceDeviceName 
									FROM 
										srcLogicalVolumeDestLogicalVolume 
									GROUP BY 
										sourceHostId, 
										sourceDeviceName";
						
						my $rs = $conn->sql_exec($sql2);
						if(!defined $rs)
						{
							return;
						}

						my $flag;
						my $report_filename;
						if (Utilities::isWindows())
						{
							my $report_generator = '"'.$cs_installation_path."\\home\\svsystems\\admin\\web\\ui\\".
							"generate_protection_report.php\"";
							$flag = `$AMETHYST_VARS->{'PHP_PATH'} $report_generator $no_of_days`;
							$report_filename = "$cs_installation_path\\home\\svsystems\\admin\\web\\trends\\rep\\".
							"protection_rep_mail.htm";
						}
						else
						{
							my $report_generator = $AMETHYST_VARS->{WEB_ROOT}.
							"/ui/generate_protection_report.php";

							if (-e "/usr/bin/php")
							{
								$flag = `php $report_generator $no_of_days`;
							}
							elsif(-e "/usr/bin/php5")
							{
								$flag = `php5 $report_generator $no_of_days`;
							}
							
							$report_filename = "/home/svsystems/admin/web/trends/rep/".
							"protection_rep_mail.htm";
						}

						$report_filename = Utilities::makePath("$report_filename");
						open (FILEH, "<$report_filename");
						my @file_contents = <FILEH>;
						close (FILEH);
						
						foreach my $line (@file_contents)
						{
							chomp $line;
							$email_body .= $line;
						}
						$email_body =~ s/</\n</g;
						$email_body =~ s/{/\n{/g;
						
						$generate_rep = 0;
					}
					 
					$obj->send_email_message($email_address,"Health Report",$email_body);
					
					my $sql5 = "UPDATE 
									error_policy 
								SET 
									last_dispatch_time='$latest_dispatch_time' 
								WHERE 
									user_id='$uid' and 
									error_template_id='PROTECT_REP'";
					
					my $sth5 = $conn->sql_query($sql5);
					
					$conn->sql_finish($sth5);
				}
	
            }
        }
    };
    if ($@)
    {
       
        $logging_obj->log("EXCEPTION"," EXCEPTION HAS BEEN OCCURED ! ".$@);
    }
}

sub send_email_message($$$$)
{
    my ($obj, $email_address, $email_subject, $email_body) = @_;
	my $cx_name = $obj->get_cx_server_name ();
	$cx_name = TimeShotManager::customerBrandingNames($cx_name);
	$email_subject .= " - [".$cx_ip." ( ".$cx_name." ) ]";
    my $ret=1;
	
	eval
	{
		my $conn = new Common::DB::Connection();
		
		my $mail_server = $conn->sql_get_value('transbandSettings', 'ValueData',"ValueName='EMAIL_SERVER'");
		my $host = $conn->sql_get_value('transbandSettings', 'ValueData',"ValueName='EMAIL_SERVER_DOMAIN'");
		my $from_email = $conn->sql_get_value('transbandSettings', 'ValueData',"ValueName='FROM_EMAIL'");
		my $server_name = $conn->sql_get_value('transbandSettings', 'ValueData',"ValueName='CX_SERVER_NAME'");
		
		if ($email_subject =~ /Test Mail/)
		{
			my $user_info = $email_body;
			$email_body = "This is an automated message sent by ".$server_name.'@'.$cx_ip." to ".$email_address."<br>";
			$email_body.= "Your mail settings are as below:"."<br>";
			$email_body.= $user_info;
			$email_body.= "Server Name   : ".$server_name."<br>";
			$email_body.= "IP Address    : ".$cx_ip."<br>";
			$email_body.= "Mail Host     : ".$mail_server."<br>";
			$email_body.= "Mail Domain   : ".$host."<br>";
		}
        
		my $IN_APPLABEL_LC = lc($IN_APPLABEL);
		my $MSGO_host = $host;
		my $MSGO_sender      = "<$IN_APPLABEL_LC-messenger\@$host>";
		my $MSGO_receiver    ="admins\@$host";
		my $MSGO_full_sender = "- $IN_APPLABEL Messenger <$IN_APPLABEL_LC-messenger\@$host>";
		my $MSGO_bpm_sender = "-$IN_APPLABEL Bandwidth Policy Manager <$IN_APPLABEL_LC-messenger\@$host>";

		$MSGO_sender = TimeShotManager::customerBrandingNames($MSGO_sender);
		$MSGO_full_sender = TimeShotManager::customerBrandingNames($MSGO_full_sender);
		$MSGO_bpm_sender = TimeShotManager::customerBrandingNames($MSGO_bpm_sender);
		
		require Mail::Sender;
		my $smtp = Net::SMTP->new($mail_server);
		if($smtp != undef)
		{
			if (is_valid_email_delivery())
			{
				my $cx_server_name = $obj->get_cx_server_name ();
				$cx_server_name = TimeShotManager::customerBrandingNames($cx_server_name);
				my $MSGO_sender_temp = $cx_server_name." ".$MSGO_sender;

				my $MSGO_full_sender_temp = ($from_email) ? $from_email : $cx_server_name." ".$MSGO_full_sender;
				 my $sender = new Mail::Sender
				 {smtp => "$mail_server", from => "$MSGO_full_sender_temp"};
				 $sender->MailMsg({to => "$email_address",
				  subject => "$email_subject",
				  ctype => "text/html",
				  msg => "$email_body"});
			}
			else
			{
				$ret=0;
			}
		}
		else
		{
			$ret = 0;
		}
	};
	if ($@)
	{
        $logging_obj->log("EXCEPTION"," EXCEPTION HAS BEEN OCCURED ! ".$@);
    }
    return $ret;
}

sub is_valid_email_delivery
{
	my $mail_delivery = 1;
	my $conn = new Common::DB::Connection();
	my $host_guid = $AMETHYST_VARS->{HOST_GUID};
	my $role = Common::Constants::ACTIVE_ROLE;
	##
	#This is the block to identify to deliver the mails for active HA node and sets flag to mail delivery.
	#The emails should be delivered from active node, not from the passive node.
	##	
	my $query = "SELECT
				appliacneClusterIdOrHostId,
				nodeRole,
				role
			FROM
				standByDetails a,
				applianceNodeMap b
			WHERE 
				b.nodeId = '$host_guid' AND
				((a.appliacneClusterIdOrHostId = b.applianceId) OR (a.appliacneClusterIdOrHostId = b.nodeId) ) AND 
				((a.role = 'PRIMARY') OR (b.nodeRole = '$role'))";
	
	
	my $cxpair_record = $conn->sql_exec($query);

	# This is to identify whether it is active/passive for HA or remote stand by CX
	if (defined $cxpair_record) 
	{
		my @cxpair_record_hash = @$cxpair_record;

		my $cluster_ip = $cxpair_record_hash[0]{'appliacneClusterIdOrHostId'};
		my $node_role = $cxpair_record_hash[0]{'nodeRole'};
			
		$logging_obj->log("DEBUG","is_valid_email_delivery: CURRENT ROLE::$node_role appliacneClusterIdOrHostId::$cluster_ip \n");
		if($node_role eq Common::Constants::PASSIVE_ROLE) 
		{
		$mail_delivery = 0;								
		}		
	}
	return $mail_delivery;
}
sub get_cx_server_name
{
	my ($obj) = @_;
	my $cx_server_name;

	
	eval
	{
		
		my $conn = new Common::DB::Connection();
        $cx_server_name = $conn->sql_get_value('transbandSettings', 'ValueData',"ValueName='CX_SERVER_NAME'");
	};
	if ($@)
	{
       
		$logging_obj->log("EXCEPTION"," EXCEPTION HAS BEEN OCCURED ! ".$@);
    }
	return $cx_server_name;
}

##
#  
#Function Name: add_error_message  
#Description  : Adds an alert to the DB for each user
#Parameters   : $conn         - Connection object
#               $error_id     - error id of the message
#               $error_template_id  - error template
#               $summary      - summary for the message
#               $message      - The message to be sent
#Return Value : None
#
##
sub add_error_message
{
    my ($conn, $error_id, $error_template_id, $summary, $message, $host_id, $error_code, $error_placeholders) = @_;
    
	$summary = &TimeShotManager::customerBrandingNames($summary);
	$message = &TimeShotManager::customerBrandingNames($message);
	   
	$summary=~s/\\/\\\\/g;
	$message=~s/\\/\\\\/g;
    if($error_placeholders)
    {
        $error_placeholders=~s/\\/\\\\/g;
    }
	
	#updates the entries in dashboardAlerts table
	&updateAlertsDb($conn, $error_id, $error_template_id, $summary, $message, $host_id, $error_code, $error_placeholders);
	#Check whether cx is in maintenance mode
	my $cx_maintenance_mode = TimeShotManager::cx_maintenance_mode($conn);
	if($cx_maintenance_mode ne '1')
	{
        my $error_count = 1;
        my $err_start_time = '';
        my $err_update_time = '';
        my $count;
        my $first_update_time;
        my $last_update_time;
        
        my $sql_q = "SELECT errCount,errTime,errStartTime FROM dashboardAlerts WHERE hostId = '$host_id' AND errSummary = '$summary' LIMIT 0,1";

        my $record_hash = $conn->sql_exec($sql_q);
		
		my @value = @$record_hash;
		$error_count = $value[0]{'errCount'};
		$err_start_time = $value[0]{'errStartTime'};
		$err_update_time = $value[0]{'errTime'};
		
        my $sql = "SELECT UID from users";

        my $result = $conn->sql_exec($sql);
        foreach my $ref(@{$result})
        {
            my $uid = $ref->{'UID'};
            my $send_mail = 0;
            $send_mail = $conn->sql_get_value('error_policy', 'send_mail',"user_id='$uid' AND error_template_id='$error_template_id'");
            if ($send_mail)
            {
                my $message_count = 0;
				$message_count = $conn->sql_get_value('error_message', 'message_count',"error_id='$error_id' AND error_template_id='$error_template_id' AND summary='$summary' AND user_id='$uid'");
                
                if ($message_count)

                {
                    if($error_count ne 0)
                    {
                        $count = $error_count;
                    }
                    else
                    {
                        $count = $message_count + 1;
                    }
                    my $sql3 = "UPDATE error_message SET last_update_time=now(), message_count='$count', 
                               message='$message' WHERE error_id='$error_id' AND summary='$summary' 
                               AND error_template_id='$error_template_id' AND user_id='$uid'";
                    
                    my $sth3 = $conn->sql_query($sql3);
          
                    $conn->sql_finish($sth3);
                }
                else
                {
                    if($error_count ne 0)
                    {
                        $count = $error_count;
                        $first_update_time = "'".$err_start_time."'";
                        $last_update_time = "'".$err_update_time."'";
                    }
                    else
                    {
                        $count = $message_count + 1;
                        $first_update_time = "now()";
                        $last_update_time = "now()";
                    }
                    my $sql4 = "INSERT INTO error_message (error_id,
                               error_template_id,summary,message,first_update_time,last_update_time,message_count,user_id)
                               VALUES ('$error_id', '$error_template_id', 
                               '$summary', '$message', $first_update_time, $last_update_time,'$count', '$uid')";
                    
                    my $sth4 = $conn->sql_query($sql4);
                    $conn->sql_finish($sth4);
                }
            }
        }
    }
}
 
##
#  
#Function Name: updateAlertsDb  
#Description  : updates the entries in dashboardAlerts table
#Parameters   : $conn         - Connection object
#               $error_id     - error id of the message
#               $error_template_id  - error template
#               $summary      - summary for the message
#               $message      - The message to be sent
#Return Value : None
#
##   
sub updateAlertsDb
{
	my ($conn, $error_id, $error_template_id, $summary, $message, $host_id, $error_code, $error_placeholders) = @_;

	my ($errLevel, $errCategory, $errSummary, $errMessage, $sth, $sql, $data_hash_ref);

	$sql = "SELECT level,mail_subject FROM error_template WHERE error_template_id='$error_template_id'";
	my $result = $conn->sql_exec($sql);
	my @values = @$result;
	
	$errLevel = $values[0]{'level'};
	$errCategory = $values[0]{'mail_subject'};

	$errCategory = &TimeShotManager::customerBrandingNames($errCategory);
	my $cond = "";
	my $is_event = 0;
	if($error_code)
	{
		$is_event = $conn->sql_get_value("eventCodes", "isEvent", "errCode = '$error_code'");
	    $cond = "AND errCode = '$error_code'";
	}
	
	$errSummary = $summary;
	$errMessage = $message;		
	chomp $errSummary;
	
	#to get the error count 
	if (! $is_event)
	{
		$sql = "SELECT
					alertId,
					errCount
				FROM 
					dashboardAlerts
				WHERE
					errTemplateId='$error_template_id' AND
					errSummary='$errSummary' AND
					hostId='$host_id' $cond";
		$sth = $conn->sql_query($sql);
		$data_hash_ref = $conn->sql_fetchrow_hash_ref($sth)
	}
	
	#Fix for 15021, 2987844
	#update the entries in dashboardAlerts if we get the same alert again
	if (! $is_event and $data_hash_ref)
	{
		my $count = $data_hash_ref->{'errCount'} + 1;
		
		$sql = "UPDATE 
					dashboardAlerts
				SET
					errCount='$count',
					errTime=now(),
					errMessage='$errMessage',
					errTemplateId='$error_template_id',
					errCode='$error_code',
					errPlaceholders='$error_placeholders'
				WHERE
					alertId='" . $data_hash_ref->{'alertId'} . "'";
	}
	else
	{
		#insert a new entry into dashboardAlerts table
		$sql = "INSERT 
				INTO 
					dashboardAlerts(
					errTime,
					errLevel,
					errCategory,
					errSummary,
					errMessage,
					errTemplateId,
					hostId,
					errCount,
					errStartTime,
					errCode,
					errPlaceholders
					)
				VALUES (
					now(),
					'$errLevel',
					'$errCategory',
					'$errSummary',
					'$errMessage',
					'$error_template_id',
					'$host_id',
					'1',
					now(),
                    '$error_code',
                    '$error_placeholders'
					)";
	}
	
	$sth = $conn->sql_query($sql);
	$conn->sql_finish($sth);
} 

##
#
#Function Name: send_traps  
#Description  : Contains trap definitions and is used to send traps
#Parameters   : $conn         - Connection object
#               $error_template_id  - error template
#               $summary      - summary for the message
#Return Value : None
#
##  
sub send_traps
{
	my ($conn, $error_template_id, $err_summary, $params)=@_;
	#Check whether cx is in maintenance mode
	my $cx_maintenance_mode = TimeShotManager::cx_maintenance_mode($conn);
	my $trap_cmdline = &getTrapCommand($cx_maintenance_mode, $error_template_id, $err_summary, $params);
    $logging_obj->log("DEBUG","send_traps: trap_cmdline::$trap_cmdline");
    &insertTrapCommandInfo($conn,$trap_cmdline,$error_template_id);
}

##
#
#Function Name: getTrapListeners  
#Description  : Adds trap listeners
#Parameters   : $conn         - Connection object
#               $error_template_id  - error template
#Return Value : None
#
## 
sub getTrapListeners
{
  my ($conn, $error_template) = @_;
  my $errorTemplate = &get_error_template($error_template);
  
  #get the ipaddress of the trap listener 

  my $sql = "SELECT 
				tl.trapListenerIPv4,
				tl.userId
			FROM
				trapListeners tl, 
				error_policy ep
			WHERE 
				ep.user_id = tl.userId 
				AND ep.error_template_id = '$errorTemplate'
				AND ep.send_trap = '1'";
			
  my $res = $conn->sql_exec($sql);
  # Set current list to 0
  my %trap_data = ();
  # Put the listeners in an array
  foreach my $ref (@{$res}) {
	my $uid = $ref->{'userId'};
	push(@{$trap_data{$uid}}, $ref->{$TRAP_LISTENER_IPv4});
  }
  
  return %trap_data;
} 

#
# Function name : insertTrapCommandInfo
# Description   : It will insert all the trap commands into the trapInfo table.
# Parameters    : $conn , $trap_command , $error_template_id
# Return Value  : None
#
sub insertTrapCommandInfo
{
    my ($conn,$trap_cmd,$error_template_id) = @_;
	my @trap_id_arr;
	my %trap_data = &getTrapListeners($conn,$error_template_id);
	while ( my ($uid, $trap_ips) = each (%trap_data)) 
	{
		my @trap_listener_ips = @{$trap_ips};
		foreach my $trapListenerIp (@trap_listener_ips)
		{
			my $trap_command;
			if($is_windows)
			{
				my @param_array = ($trapListenerIp, $uid);
				my $param_array_ref = \@param_array;
				my $sql_trap_port = $conn->sql_query("SELECT DISTINCT trapListenerPort FROM trapListeners WHERE trapListenerIPv4 = ?  AND userId = ?",$param_array_ref);
				my $ref_trap_port = $conn->sql_fetchrow_hash_ref($sql_trap_port);
				my $trap_listener_port = $ref_trap_port->{'trapListenerPort'};
	
				$trap_command = $SNMP_TRAP." -v 2c -c ".$TRAP_COMMUNITY." -ip ".$trapListenerIp." -port ".$trap_listener_port." ".$trap_cmd;
			}
			else
			{
				$trap_command = "$SNMP_TRAP -v 2c -c $TRAP_COMMUNITY $trapListenerIp \"\" ".$trap_cmd;
			}
			$logging_obj->log("DEBUG","insertTrapCommandInfo: For UID : $uid  trap_command :: $trap_command ");
			my @param_array = ($trap_command, $uid);
			my $param_array_ref = \@param_array;
			my $sql_stmnt = $conn->sql_query("SELECT id FROM trapInfo WHERE trapCommand = ? and userId = ?",$param_array_ref);
			if($conn->sql_num_rows($sql_stmnt) == 0)
			{
				my @param_array = ($trap_command, $uid);
				my $param_array_ref = \@param_array;
				my $trap_cmd_sth = $conn->sql_query("INSERT INTO trapInfo (trapCommand, userId) VALUES (?,?)",$param_array_ref);
			}
			else
			{
				my $trap_id;
				my @id_array = (\$trap_id);
				$conn->sql_bind_columns($sql_stmnt,@id_array);
				# Put the trap ids in an array
				while ($conn->sql_fetch($sql_stmnt))
				{
					push(@trap_id_arr,$trap_id);
				}
				my $num_of_trap_ids = @trap_id_arr;
				if($num_of_trap_ids > 0)
				{
					my $trap_id_str = join("','",@trap_id_arr);
					my $sql_update = "UPDATE trapInfo SET createdTime=now() WHERE id IN ('$trap_id_str')";
					my $sth_update = $conn->sql_query($sql_update);
				}
			}
		}	
	}
}

#
# Function name : sendTrapInfo
# Description   : It will fetch all the trap commands from the db and executes at the command line.
#                 It will delete the trap commands from the db, after successful execution.
# Parameters    : None
# Return Value  : None
#
sub sendTrapInfo()
{
    my $conn = new Common::DB::Connection();
	#Check whether cx is in maintenance mode
	my $cx_maintenance_mode = TimeShotManager::cx_maintenance_mode($conn);
	if($cx_maintenance_mode ne '1')
	{	
		my @trap_id_arr;
		my $query = "SELECT 
                        UID, 
                        trap_dispatch_interval,
						last_trap_dispatch_time
                    FROM 
                        users";
       
		my $result = $conn->sql_exec($query);
        foreach my $ref1 (@{$result})
        {
			my $uid = $ref1->{'UID'};
            my $trap_dispatch_interval = $ref1->{'trap_dispatch_interval'};
            my $last_trap_dispatch_time = $ref1->{'last_trap_dispatch_time'};
			my $current_time = time();
			$logging_obj->log("DEBUG","For uid :: $uid : current_time :: $current_time : last_trap_dispatch_time :: $last_trap_dispatch_time : trap_dispatch_interval :: $trap_dispatch_interval");
			if (($current_time >= $last_trap_dispatch_time + ($trap_dispatch_interval*60)) || 
			!$last_trap_dispatch_time)
			{
				my $sql = "SELECT
							id,
							trapCommand	
						FROM 
							trapInfo 
						WHERE 
							userId = '$uid'
						ORDER BY id";

				my $result1 = $conn->sql_exec($sql);
				foreach my $ref (@{$result1})
				{	
					my $trap_id = $ref->{'id'};
					my $trap_cmd = $ref->{'trapCommand'};
					$trap_cmd =~ s/\\"/\\\\"/g;
					$logging_obj->log("DEBUG","sending trap for uid : $uid\nTrap :: $trap_cmd\n");
					#execute the trap command
					my $snmptrapout = `$trap_cmd`;
					if ($? != 0) 
					{
						$logging_obj->log("EXCEPTION","Error sending trap: $snmptrapout");
					}  
					else
					{
						# Put the trap ids in an array
						push(@trap_id_arr,$trap_id);
					}
				}
				my $num_of_trap_ids = @trap_id_arr;
				if($num_of_trap_ids > 0)
				{
					my $trap_id_str = join("','",@trap_id_arr);
					my $sql_delete = "DELETE FROM trapInfo WHERE id IN ('$trap_id_str')";
					my $sth_delete = $conn->sql_query($sql_delete);
					
					my $sql = "UPDATE 
									users 
								SET 
									last_trap_dispatch_time='$current_time' 
								WHERE 
									UID='$uid'";
				
					my $sth = $conn->sql_query($sql);
					$conn->sql_finish($sth);
				}
			}
		}	
	}
}

##
#
#Function Name: csUpAndRunningAlert  
#Description  : Send trap alerts as long as cs is up and running.
#Parameters   : None
#Return Value : None
#
##
sub csUpAndRunningAlert
{
    my ($conn) = @_;
    my $CSHostName = &TimeShotManager::getCSHostName($conn);

    my $sql = "SELECT 
                tl.trapListenerIPv4, tl.userId
            FROM
                trapListeners tl, 
                error_policy ep 
            WHERE 
                ep.user_id = tl.userId 
                AND ep.error_template_id='CX_HEARTBEAT' 
                AND ep.send_trap='1' 
                AND UNIX_TIMESTAMP() >= (ep.last_dispatch_time + (ep.dispatch_interval * 60))";
	$logging_obj->log("DEBUG","csUpAndRunningAlert:sql::$sql");

    my $result = $conn->sql_exec($sql);
    # Set current list to 0
    my @TRAP_LISTENERS = ();
    my @USER_IDS = ();

    # Put the listeners in an array
    foreach my $ref (@{$result}) {
        push (@TRAP_LISTENERS, $ref->{$TRAP_LISTENER_IPv4});        
        $logging_obj->log("DEBUG","Adding listener: $ref->{$TRAP_LISTENER_IPv4}");
        
        push (@USER_IDS, $ref->{'userId'});
    }
    
    my $trap_cmdline;
    my $current_time;
    my $snmptrapout;
    
    foreach my $trapmanager (@TRAP_LISTENERS) {    
        $current_time = strftime("%m-%d-%Y %I:%M:%S %p", localtime);
		if($is_windows)
		{
			my @param_array = ($trapmanager);
			my $param_array_ref = \@param_array;
			my $sql_trap_port = $conn->sql_query("SELECT DISTINCT trapListenerPort FROM trapListeners WHERE trapListenerIPv4 = ?",$param_array_ref);
			my $ref_trap_port = $conn->sql_fetchrow_hash_ref($sql_trap_port);
			my $trap_listener_port = $ref_trap_port->{'trapListenerPort'};
				
			$trap_cmdline = $SNMP_TRAP." -v 2c -c ".$TRAP_COMMUNITY." -ip ".$trapmanager." -port ".$trap_listener_port;
			$trap_cmdline   .= " -traptype ".$TRAP_CSUP_AND_REUUNING;
			$trap_cmdline   .= " -trapvar ".$TRAP_AMETHYSTGUID."\:\:s\:\:".$MY_AMETHYST_GUID.$GUID_FILLER;
			$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"CS server is up and running as on ".$current_time."\"";
			$trap_cmdline   .= " -trapvar ".$TRAP_CS_HOSTNAME."\:\:s\:\:\"".$CSHostName."\"";    
			$trap_cmdline   .= " -trapvar ".$TRAP_SEVERITY."\:\:s\:\:Notice";
        }
		else
		{		
			$trap_cmdline = "$SNMP_TRAP -v 2c -c $TRAP_COMMUNITY $trapmanager \"\" ";
			$trap_cmdline   .= "$ABHAI_MIB\:\:trapCSUpAndRunning ";
			$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AMETHYSTGUID ";
			$trap_cmdline   .= "s $MY_AMETHYST_GUID" . $GUID_FILLER . " ";
			$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
			$trap_cmdline   .= "s \"CS server is up and running as on $current_time\" ";
			$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_CS_HOSTNAME ";
			$trap_cmdline   .= "s \"$CSHostName\" ";        
			$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SEVERITY ";
			$trap_cmdline   .= "s Notice ";
		}
        
        $trap_cmdline =~ s/\\"/\\\\"/g;
        $snmptrapout = `$trap_cmdline`;
        if ($? != 0) 
        {
            $logging_obj->log("EXCEPTION","Error sending trap: $snmptrapout");
        }
    }

    foreach my $user_id (@USER_IDS) {
        my $sql1 = "UPDATE 
                        error_policy 
                    SET 
                        last_dispatch_time=UNIX_TIMESTAMP() 
                    WHERE 
                        user_id='$user_id' and 
                        error_template_id='CX_HEARTBEAT'";
		$logging_obj->log("DEBUG","csUpAndRunningAlert:sql1::$sql1");
        
        my $sth1 = $conn->sql_query($sql1);
        
        $conn->sql_finish($sth1);
    }
}

sub getTrapCommand
{
	my ($cx_maintenance_mode, $error_template_id, $err_summary, $params, $data_hash) = @_;
	
	if($cx_maintenance_mode ne '1')
    {
        my($cs_host_name,$ps_host_name,$src_host_name,$id,$dest_host_name,$src_vol,$dest_vol,$dest_id,$expiration_date,$errtime,$errlvl);
        if($params)
        {
            $logging_obj->log("DEBUG","send_traps: params::".Dumper ($params));
            my %args = %{$params};
            if(%args != '')
            {
                $cs_host_name = $args{"cs_host_name"};
                $ps_host_name = $args{"ps_host_name"};
                $src_host_name = $args{"src_host_name"};
                $id = $args{"id"};
                $dest_host_name = $args{"dest_host_name"};
                $src_vol = $args{"src_vol"};
                $dest_vol = $args{"dest_vol"};
                $dest_id = $args{"dest_id"};
                $expiration_date = $args{"expiration_date"};
                $errtime = $args{"errtime"};
                $errlvl = $args{"errlvl"};
            }
        }
        
        #Remove newline and <br> from the error message and replace with a single white space
        $err_summary =~ s/<br>/ /g;
        $err_summary =~ s/\n/ /g;
        my $trap_cmdline;
        
        if($error_template_id eq "SWITCH_USAGE")
        { 
            if ($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_DISK_SPACE_THRESHOLD_ON_SWITCH_BP_EXCEEDED;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
			}
			else
			{
				$trap_cmdline   .= "$ABHAI_MIB\:\:trapDiskSpaceThresholdOnSwitchBPExceeded ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary\" ";
			}
        }	
        elsif($error_template_id eq "PS_UNINSTALL")
        {
            if($cs_host_name eq '' || $ps_host_name eq '')
            {
                $logging_obj->log("EXCEPTION","send_traps:Expected parameter is not received");
            }
			if ($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_PROCESS_SERVER_UNINSTALLATION;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
				$trap_cmdline 	.= " -trapvar ".$TRAP_SEVERITY."\:\:s\:\:Critical ";
				$trap_cmdline 	.= " -trapvar ".$TRAP_CS_HOSTNAME."\:\:s\:\:\"".$cs_host_name."\"";
				$trap_cmdline 	.= " -trapvar ".$TRAP_PS_HOSTNAME."\:\:s\:\:\"".$ps_host_name."\"";
			}
			else
			{
				$trap_cmdline   .= "$ABHAI_MIB\:\:ProcessServerUnInstallation ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary\" ";
				$trap_cmdline 	.= "$ABHAI_MIB\:\:$TRAP_SEVERITY s Critical ";
				$trap_cmdline 	.= "$ABHAI_MIB\:\:$TRAP_CS_HOSTNAME s \"$cs_host_name\" ";
				$trap_cmdline 	.= "$ABHAI_MIB\:\:$TRAP_PS_HOSTNAME s \"$ps_host_name\" ";
			}
            
        }
        elsif($error_template_id eq "CXBPM_ALERT")
        {
			if ($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_BANDWIDTH_SHAPING;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
			}
			else
			{
				$trap_cmdline   .= "$ABHAI_MIB\:\:BandwidthShaping ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary\" ";
			}
        }
        elsif($error_template_id eq "CXSTOR_WARN")
        {
             if($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_SECONDARY_STORAGE_UTILIZATION_DANGER_LEVEL;
				$trap_cmdline   .= " -trapvar ".$TRAP_AMETHYSTGUID."\:\:s\:\:".$MY_AMETHYST_GUID.$GUID_FILLER;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
			}
			else
			{
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SECONDARY_STORAGE_UTILIZATION_DANGER_LEVEL ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AMETHYSTGUID ";
				$trap_cmdline   .= "s $MY_AMETHYST_GUID" . $GUID_FILLER . " ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary\" ";
			}
		}
        elsif($error_template_id eq "RPOSLA_EXCD_VX")
        {	
            if($id eq '' || $cs_host_name eq '' || $ps_host_name eq '' || $src_host_name eq '' || $src_vol eq '' || $dest_host_name eq '' || $dest_vol eq '')
            {
                $logging_obj->log("EXCEPTION","send_traps:Expected parameter is not received");
            }
            my $src_hostguid =  $id . $GUID_FILLER;
			if($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_VX_RPOSLA_THRESHOLD_EXCEEDED;
				$trap_cmdline   .= " -trapvar ".$TRAP_AMETHYSTGUID."\:\:s\:\:".$MY_AMETHYST_GUID.$GUID_FILLER;
				$trap_cmdline   .= " -trapvar ".$TRAP_HOSTGUID."\:\:s\:\:".$src_hostguid;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SEVERITY."\:\:s\:\:Major";
				$trap_cmdline   .= " -trapvar ".$TRAP_CS_HOSTNAME."\:\:s\:\:\"".$cs_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_PS_HOSTNAME."\:\:s\:\:\"".$ps_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SOURCE_HOSTNAME."\:\:s\:\:\"".$src_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SOURCE_DEVICENAME."\:\:s\:\:\"".$src_vol."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_TARGET_HOSTNAME."\:\:s\:\:\"".$dest_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_TARGET_DEVICENAME."\:\:s\:\:\"".$dest_vol."\"";
			}
			else
			{			
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_VX_RPOSLA_THRESHOLD_EXCEEDED ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AMETHYSTGUID ";
				$trap_cmdline   .= "s $MY_AMETHYST_GUID" . $GUID_FILLER . " ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_HOSTGUID ";
				$trap_cmdline   .= "s $src_hostguid ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SEVERITY ";
				$trap_cmdline   .= "s Major ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_CS_HOSTNAME ";
				$trap_cmdline   .= "s \"$cs_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_PS_HOSTNAME ";
				$trap_cmdline   .= "s \"$ps_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SOURCE_HOSTNAME ";
				$trap_cmdline   .= "s \"$src_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SOURCE_DEVICENAME ";
				$trap_cmdline   .= "s \"$src_vol\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_TARGET_HOSTNAME ";
				$trap_cmdline   .= "s \"$dest_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_TARGET_DEVICENAME ";
				$trap_cmdline   .= "s \"$dest_vol\" ";
			}
        }
        elsif($error_template_id eq "RPOSLA_EXCD_FX")
        {
            if($id eq '' || $cs_host_name eq '' || $src_host_name eq '' || $src_vol eq '' || $dest_host_name eq '' || $dest_vol eq '')
            {
                $logging_obj->log("EXCEPTION","send_traps:Expected parameter is not received");
            }
			if($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_FX_RPOSLA_THRESHOLD_EXCEEDED;
				$trap_cmdline   .= " -trapvar ".$TRAP_AMETHYSTGUID."\:\:s\:\:".$MY_AMETHYST_GUID.$GUID_FILLER;
				$trap_cmdline   .= " -trapvar ".$TRAP_FILEREP_CONFIG."\:\:s\:\:".$id;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SEVERITY."\:\:s\:\:Major";
				$trap_cmdline   .= " -trapvar ".$TRAP_CS_HOSTNAME."\:\:s\:\:\"".$cs_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SOURCE_HOSTNAME."\:\:s\:\:\"".$src_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SOURCE_DEVICENAME."\:\:s\:\:\"".$src_vol."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_TARGET_HOSTNAME."\:\:s\:\:\"".$dest_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_TARGET_DEVICENAME."\:\:s\:\:\"".$dest_vol."\"";
			}
			else
			{
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_FX_RPOSLA_THRESHOLD_EXCEEDED "; 	 
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AMETHYSTGUID "; 	 
				$trap_cmdline   .= "s $MY_AMETHYST_GUID" . $GUID_FILLER . " "; 	 
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_FILEREP_CONFIG "; 	 
				$trap_cmdline   .= "s $id "; 
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SEVERITY ";
				$trap_cmdline   .= "s Major ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_CS_HOSTNAME ";
				$trap_cmdline   .= "s \"$cs_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SOURCE_HOSTNAME ";
				$trap_cmdline   .= "s \"$src_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SOURCE_DEVICENAME ";
				$trap_cmdline   .= "s \"$src_vol\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_TARGET_HOSTNAME ";
				$trap_cmdline   .= "s \"$dest_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_TARGET_DEVICENAME ";
				$trap_cmdline   .= "s \"$dest_vol\" ";
			}
        }
        elsif($error_template_id eq "RPOSLA_EXCD_PS")
        {
            if($id eq '' || $cs_host_name eq '' || $src_host_name eq '')
            {
                $logging_obj->log("EXCEPTION","send_traps:Expected parameter is not received");
            }
			if($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_PS_RPOSLA_THRESHOLD_EXCEEDED;
				$trap_cmdline   .= " -trapvar ".$TRAP_AMETHYSTGUID."\:\:s\:\:".$MY_AMETHYST_GUID.$GUID_FILLER;
				$trap_cmdline   .= " -trapvar ".$TRAP_HOSTGUID."\:\:s\:\:".$id;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SEVERITY."\:\:s\:\:Major";
				$trap_cmdline   .= " -trapvar ".$TRAP_CS_HOSTNAME."\:\:s\:\:\"".$cs_host_name."\"";
				#ps host name is same as cs host name as is mentioned in file_monitor subroutine in tmanager.pm
				$trap_cmdline   .= " -trapvar ".$TRAP_PS_HOSTNAME."\:\:s\:\:\"".$cs_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SOURCE_HOSTNAME."\:\:s\:\:\"".$src_host_name."\"";
			}
			else
			{
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_PS_RPOSLA_THRESHOLD_EXCEEDED ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AMETHYSTGUID ";
				$trap_cmdline   .= "s $MY_AMETHYST_GUID" . $GUID_FILLER . " ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_HOSTGUID ";
				$trap_cmdline   .= "s $id ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary \" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SEVERITY ";
				$trap_cmdline   .= "s Major ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_CS_HOSTNAME ";
				$trap_cmdline   .= "s \"$cs_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_PS_HOSTNAME ";
				#ps host name is same as cs host name as is mentioned in file_monitor subroutine in tmanager.pm
				$trap_cmdline   .= "s \"$cs_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SOURCE_HOSTNAME ";
				$trap_cmdline   .= "s \"$src_host_name\" ";
			}
        }
        elsif($error_template_id eq "FXJOB_ERROR")
        {	
            if($id eq '' || $cs_host_name eq '' || $src_host_name eq '' || $src_vol eq '' || $dest_host_name eq ''  || $dest_vol eq '')
            {
                $logging_obj->log("EXCEPTION","send_traps:Expected parameter is not received");
            }
			if($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_FILEREP_PROGRESS_THRESHOLD_EXCEEDED;
				$trap_cmdline   .= " -trapvar ".$TRAP_AMETHYSTGUID."\:\:s\:\:".$MY_AMETHYST_GUID.$GUID_FILLER;
				$trap_cmdline   .= " -trapvar ".$TRAP_FILEREP_CONFIG."\:\:s\:\:".$id;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SEVERITY."\:\:s\:\:Warning";
				$trap_cmdline   .= " -trapvar ".$TRAP_CS_HOSTNAME."\:\:s\:\:\"".$cs_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SOURCE_HOSTNAME."\:\:s\:\:\"".$src_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SOURCE_DEVICENAME."\:\:s\:\:\"".$src_vol."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_TARGET_HOSTNAME."\:\:s\:\:\"".$dest_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_TARGET_DEVICENAME."\:\:s\:\:\"".$dest_vol."\"";
			}
			else
			{
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_FILEREP_PROGRESS_THRESHOLD_EXCEEDED ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AMETHYSTGUID ";
				$trap_cmdline   .= "s $MY_AMETHYST_GUID" . $GUID_FILLER . " ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_FILEREP_CONFIG ";
				$trap_cmdline   .= "s $id ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SEVERITY ";
				$trap_cmdline   .= "s Warning ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_CS_HOSTNAME ";
				$trap_cmdline   .= "s \"$cs_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SOURCE_HOSTNAME ";
				$trap_cmdline   .= "s \"$src_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SOURCE_DEVICENAME ";
				$trap_cmdline   .= "s \"$src_vol\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_TARGET_HOSTNAME ";
				$trap_cmdline   .= "s \"$dest_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_TARGET_DEVICENAME ";
				$trap_cmdline   .= "s \"$dest_vol\" ";
			}
        }
        elsif($error_template_id eq "RESYNC_REQD")
        {
            if($id eq '' || $cs_host_name eq '' || $ps_host_name eq '' || $src_host_name eq '' || $src_vol eq '' || $dest_host_name eq '' || $dest_vol eq '')
            {
                $logging_obj->log("EXCEPTION","send_traps:Expected parameter is not received");
            }
            my $src_hostguid =  $id .  $GUID_FILLER;
            my $dst_hostguid =  $dest_id .  $GUID_FILLER;
            my $src_hostvol  =  $src_vol    .  $GUID_FILLER;
            my $dst_hostvol  =  $dest_vol    .  $GUID_FILLER;
			if($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_SENTINEL_RESYNC_REQUIRED;
				$trap_cmdline   .= " -trapvar ".$TRAP_AMETHYSTGUID."\:\:s\:\:".$MY_AMETHYST_GUID.$GUID_FILLER;
				$trap_cmdline   .= " -trapvar ".$TRAP_HOSTGUID."\:\:s\:\:".$src_hostguid;
				$trap_cmdline   .= " -trapvar ".$TRAP_HOSTGUID."\:\:s\:\:".$dst_hostguid;
				$trap_cmdline   .= " -trapvar ".$TRAP_VOLUMEGUID."\:\:s\:\:".$src_hostvol;
				$trap_cmdline   .= " -trapvar ".$TRAP_VOLUMEGUID."\:\:s\:\:".$dst_hostvol;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SEVERITY."\:\:s\:\:Critical";
				$trap_cmdline   .= " -trapvar ".$TRAP_CS_HOSTNAME."\:\:s\:\:\"".$cs_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SOURCE_HOSTNAME."\:\:s\:\:\"".$src_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SOURCE_DEVICENAME."\:\:s\:\:\"".$src_vol."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_TARGET_HOSTNAME."\:\:s\:\:\"".$dest_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_TARGET_DEVICENAME."\:\:s\:\:\"".$dest_vol."\"";
			}
			else
			{
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SENTINEL_RESYNC_REQUIRED ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AMETHYSTGUID ";
				$trap_cmdline   .= "s $MY_AMETHYST_GUID" . $GUID_FILLER . " ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_HOSTGUID ";
				$trap_cmdline   .= "s $src_hostguid ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_HOSTGUID ";
				$trap_cmdline   .= "s $dst_hostguid ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_VOLUMEGUID ";
				$trap_cmdline   .= "s $src_hostvol ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_VOLUMEGUID ";
				$trap_cmdline   .= "s $dst_hostvol ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SEVERITY ";
				$trap_cmdline   .= "s Critical ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_CS_HOSTNAME ";
				$trap_cmdline   .= "s \"$cs_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_PS_HOSTNAME ";
				$trap_cmdline   .= "s \"$ps_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SOURCE_HOSTNAME ";
				$trap_cmdline   .= "s \"$src_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SOURCE_DEVICENAME ";
				$trap_cmdline   .= "s \"$src_vol\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_TARGET_HOSTNAME ";
				$trap_cmdline   .= "s \"$dest_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_TARGET_DEVICENAME ";
				$trap_cmdline   .= "s \"$dest_vol\" ";
			}
        }
        elsif($error_template_id eq "NODE_DOWN_PS")
        {
            if($id eq '' || $cs_host_name eq '' || $ps_host_name eq '' || $src_host_name eq '')
            {
                $logging_obj->log("EXCEPTION","send_traps:Expected parameter is not received");
            }
            my $src_hostguid =  $id . $GUID_FILLER;
			if($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_PS_NODE_DOWN;
				$trap_cmdline   .= " -trapvar ".$TRAP_AMETHYSTGUID."\:\:s\:\:".$MY_AMETHYST_GUID.$GUID_FILLER;
				$trap_cmdline   .= " -trapvar ".$TRAP_HOSTGUID."\:\:s\:\:".$src_hostguid;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SEVERITY."\:\:s\:\:Critical";
				$trap_cmdline   .= " -trapvar ".$TRAP_CS_HOSTNAME."\:\:s\:\:\"".$cs_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_PS_HOSTNAME."\:\:s\:\:\"".$ps_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_AFFTECTED_SYSTEM_HOSTNAME."\:\:s\:\:\"".$src_host_name."\"";
			}
			else
			{
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_PS_NODE_DOWN ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AMETHYSTGUID ";
				$trap_cmdline   .= "s $MY_AMETHYST_GUID" . $GUID_FILLER . " ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_HOSTGUID ";
				$trap_cmdline   .= "s $src_hostguid ";	
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SEVERITY ";
				$trap_cmdline   .= "s Critical ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_CS_HOSTNAME ";
				$trap_cmdline   .= "s \"$cs_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_PS_HOSTNAME ";
				$trap_cmdline   .= "s \"$ps_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AFFTECTED_SYSTEM_HOSTNAME ";
				$trap_cmdline   .= "s \"$src_host_name\" ";
			}
        }
        elsif($error_template_id eq "NODE_DOWN_VX")
        {
            if($id eq '' || $cs_host_name eq '' || $src_host_name eq '')
            {
                $logging_obj->log("EXCEPTION","send_traps:Expected parameter is not received");
            }
            my $src_hostguid =  $id . $GUID_FILLER;
			if($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_VX_NODE_DOWN;
				$trap_cmdline   .= " -trapvar ".$TRAP_AMETHYSTGUID."\:\:s\:\:".$MY_AMETHYST_GUID.$GUID_FILLER;
				$trap_cmdline   .= " -trapvar ".$TRAP_HOSTGUID."\:\:s\:\:".$src_hostguid;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SEVERITY."\:\:s\:\:Critical";
				$trap_cmdline   .= " -trapvar ".$TRAP_CS_HOSTNAME."\:\:s\:\:\"".$cs_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_AFFTECTED_SYSTEM_HOSTNAME."\:\:s\:\:\"".$src_host_name."\"";
			}
			else
			{
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_VX_NODE_DOWN ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AMETHYSTGUID ";
				$trap_cmdline   .= "s $MY_AMETHYST_GUID" . $GUID_FILLER . " ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_HOSTGUID ";
				$trap_cmdline   .= "s $src_hostguid ";	
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SEVERITY ";
				$trap_cmdline   .= "s Critical ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_CS_HOSTNAME ";
				$trap_cmdline   .= "s \"$cs_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AFFTECTED_SYSTEM_HOSTNAME ";
				$trap_cmdline   .= "s \"$src_host_name\" ";
			}
        }
        elsif($error_template_id eq "NODE_DOWN_FX")
        {
            if($id eq '' || $cs_host_name eq '' || $src_host_name eq '')
            {
                $logging_obj->log("EXCEPTION","send_traps:Expected parameter is not received");
            }
            my $src_hostguid =  $id . $GUID_FILLER;
			if($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_FX_NODE_DOWN;
				$trap_cmdline   .= " -trapvar ".$TRAP_AMETHYSTGUID."\:\:s\:\:".$MY_AMETHYST_GUID.$GUID_FILLER;
				$trap_cmdline   .= " -trapvar ".$TRAP_HOSTGUID."\:\:s\:\:".$src_hostguid;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SEVERITY."\:\:s\:\:Critical";
				$trap_cmdline   .= " -trapvar ".$TRAP_CS_HOSTNAME."\:\:s\:\:\"".$cs_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_AFFTECTED_SYSTEM_HOSTNAME."\:\:s\:\:\"".$src_host_name."\"";
			}
			else
			{
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_FX_NODE_DOWN ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AMETHYSTGUID ";
				$trap_cmdline   .= "s $MY_AMETHYST_GUID" . $GUID_FILLER . " ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_HOSTGUID ";
				$trap_cmdline   .= "s $src_hostguid ";	
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SEVERITY ";
				$trap_cmdline   .= "s Critical ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_CS_HOSTNAME ";
				$trap_cmdline   .= "s \"$cs_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AFFTECTED_SYSTEM_HOSTNAME ";
				$trap_cmdline   .= "s \"$src_host_name\" ";
			}
        }
        elsif($error_template_id eq "NODE_DOWN_APP")
        {
            if($id eq '' || $cs_host_name eq '' || $src_host_name eq '')
            {
                $logging_obj->log("EXCEPTION","send_traps:Expected parameter is not received");
            }
            my $src_hostguid =  $id . $GUID_FILLER;
			if($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_APP_NODE_DOWN;
				$trap_cmdline   .= " -trapvar ".$TRAP_AMETHYSTGUID."\:\:s\:\:".$MY_AMETHYST_GUID.$GUID_FILLER;
				$trap_cmdline   .= " -trapvar ".$TRAP_HOSTGUID."\:\:s\:\:".$src_hostguid;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_SEVERITY."\:\:s\:\:Critical";
				$trap_cmdline   .= " -trapvar ".$TRAP_CS_HOSTNAME."\:\:s\:\:\"".$cs_host_name."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_AFFTECTED_SYSTEM_HOSTNAME."\:\:s\:\:\"".$src_host_name."\"";
			}
			else
			{
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_APP_NODE_DOWN ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AMETHYSTGUID ";
				$trap_cmdline   .= "s $MY_AMETHYST_GUID" . $GUID_FILLER . " ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_HOSTGUID ";
				$trap_cmdline   .= "s $src_hostguid ";	
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_SEVERITY ";
				$trap_cmdline   .= "s Critical ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_CS_HOSTNAME ";
				$trap_cmdline   .= "s \"$cs_host_name\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AFFTECTED_SYSTEM_HOSTNAME ";
				$trap_cmdline   .= "s \"$src_host_name\" ";
			}
        }
        elsif($error_template_id eq "PROTECTION_ALERT")
        {
            if($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_VERSION_MISMATCH;
				$trap_cmdline   .= " -trapvar ".$TRAP_AMETHYSTGUID."\:\:s\:\:".$MY_AMETHYST_GUID.$GUID_FILLER;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
			}
			else
			{
				$trap_cmdline   .= "$ABHAI_MIB\:\:trapVersionMismatch ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AMETHYSTGUID ";
				$trap_cmdline   .= "s $MY_AMETHYST_GUID" . $GUID_FILLER . " ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary\" ";
			}
        }	
        elsif($error_template_id eq "PROTECTION_ALERT_APP")
        {
            if($id eq '')
            {
                $logging_obj->log("EXCEPTION","send_traps:Expected parameter 'id' is not received");
            }
            my $src_hostguid =  $id . $GUID_FILLER;
			if($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_BITMAP_MODE_CHANGE;
				$trap_cmdline   .= " -trapvar ".$TRAP_AMETHYSTGUID."\:\:s\:\:".$MY_AMETHYST_GUID.$GUID_FILLER;
				$trap_cmdline   .= " -trapvar ".$TRAP_HOSTGUID."\:\:s\:\:".$src_hostguid;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
			}
			else
			{
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_BITMAP_MODE_CHANGE ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AMETHYSTGUID ";
				$trap_cmdline   .= "s $MY_AMETHYST_GUID" . $GUID_FILLER . " ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_HOSTGUID ";
				$trap_cmdline   .= "s $src_hostguid ";	
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary\" ";
			}
        }
        elsif($error_template_id eq "AGENT_ERROR")
        {
            if($id eq '' || $errtime eq '' || $errlvl eq '')
            {
                $logging_obj->log("EXCEPTION","send_traps:Expected parameter is not received");
            }
			if($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_AGENT_LOGGED_ERROR_MESSAGE;
				$trap_cmdline   .= " -trapvar ".$TRAP_AMETHYSTGUID."\:\:s\:\:".$MY_AMETHYST_GUID.$GUID_FILLER;
				$trap_cmdline   .= " -trapvar ".$TRAP_HOSTGUID."\:\:s\:\:".$id;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_LOGGED_ERROR_MESSAGE_TIME."\:\:s\:\:\"".$errtime."\"";
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_LOGGED_ERROR_LEVEL."\:\:s\:\:".$errlvl;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
			}
			else
			{
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_LOGGED_ERROR_MESSAGE ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AMETHYSTGUID ";
				$trap_cmdline   .= "s $MY_AMETHYST_GUID" . $GUID_FILLER . " ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_HOSTGUID ";
				$trap_cmdline   .= "s $id ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_LOGGED_ERROR_MESSAGE_TIME ";
				$trap_cmdline   .= "s \"$errtime\" ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_LOGGED_ERROR_LEVEL ";
				$trap_cmdline   .= "s $errlvl ";
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary\" ";
			}
        }
        elsif($error_template_id eq "CS_FAILOVER")
        {
			if($is_windows)
			{
				$trap_cmdline   .= " -traptype ".$TRAP_CX_NODE_FAILOVER;
				$trap_cmdline   .= " -trapvar ".$TRAP_AGENT_ERROR_MESSAGE."\:\:s\:\:\"".$err_summary."\"";
			}
			else
			{
				$trap_cmdline   .= "$ABHAI_MIB\:\:CXNodeFailover ";	
				$trap_cmdline   .= "$ABHAI_MIB\:\:$TRAP_AGENT_ERROR_MESSAGE ";
				$trap_cmdline   .= "s \"$err_summary\" ";
			}
        }	

		return $trap_cmdline;	
	}
}

sub get_error_template
{
	my ($errorTemplate) = @_;

	if($errorTemplate =~ /RPOSLA_EXCD_*/)	
	{
	$errorTemplate = "RPOSLA_EXCD";
	}
	elsif($errorTemplate =~ /PROTECTION_*/)	
	{
	$errorTemplate = "PROTECTION_ALERT";
	}
	elsif($errorTemplate =~ /NODE_DOWN_*/)	
	{
	$errorTemplate = "VXAGENT_DWN";
	}
	else
	{
	$errorTemplate = $errorTemplate;
	}
	return $errorTemplate;
}

1;

