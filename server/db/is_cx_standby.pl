#!/usr/bin/perl
use strict;
use DBI;
use Config;
my $dsn;
my $DB_HOST;
my $DB_NAME;
my $username;
my $password;
my $dbh = 'undef';
eval
{
    my $amethyst_variables = &parse_conf_file();
    $DB_HOST = $amethyst_variables->{'DB_HOST'};
    $DB_NAME = $amethyst_variables->{'DB_NAME'};
    $dsn = "DBI:mysql:database=".$DB_NAME.";host=".$DB_HOST;
    $username = $amethyst_variables->{'DB_USER'};
    $password = $amethyst_variables->{'DB_PASSWD'};
    $dbh = DBI->connect($dsn,$username,$password);
};
if($@)
{
    &standby_log("Database Connection Failed".$@."\n");
}

if($dbh ne 'undef')
{
	eval
	{
		my $amethyst_vars = &parse_conf_file();	
		my $host_guid = $amethyst_vars->{'HOST_GUID'};		
		
		&standby_log("INFO :: host_guid : ".$host_guid);
		my $sql = "SELECT 
						role 
					FROM 
						cxPair 
					WHERE 
						hostId='$host_guid' 
					AND 
						(clusterIp = 'NULL' OR clusterIp='' OR clusterIp IS NULL)
					AND 
						role='PASSIVE'";
						
		&standby_log("INFO :: sql to get role : ".$sql);
		#print "sql::$sql";
		my $sth = $dbh->prepare($sql);
		$sth->execute();
		if($sth->rows())
		{
			&standby_log("INFO :: Setup is standby");
			print "1";
		}
		else
		{
			&standby_log("INFO :: Setup is not standby");
			print "0";
		}
	};
	if($@)
	{
		&standby_log("ERROR while getting standby details - ".$@);
		$dbh->rollback;
	}
}

sub getOSType()
{
	return $Config{osname};
}

sub isWindows() 
{
  my $os = getOSType();
  $os =~ m/win/ig ? return 1 : return 0;
}

sub standby_log 
{
	my ($content) = @_;
	my $log_file = "/home/svsystems/var/is_cx_standby.log";
	
	if(isWindows())
	{
		$log_file = "C:\\home\\svsystems\\var\\is_cx_standby.log";
	}

	open(LOGFILE1, ">>$log_file") || print "Could not open standby logfile:
			  standby_log.log , faulty installation?\n";

	my $display_timestamp   = time();
	my $display_time = scalar localtime($display_timestamp);
	my $entry   = $display_time . ": " . $content ; # No newline added at end
	print LOGFILE1 $entry;
	close(LOGFILE1);
}

sub parse_conf_file
{
	my $filename = "/home/svsystems/etc/amethyst.conf";
	open (FILEH, "<$filename") || die "Unable to locate $filename:$!\n";
	my @file_contents = <FILEH>;
	close (FILEH);
	my $amethyst_vars;
	#
	# Read all the config parameters in amethyst.conf and 
	# put them in a global hash.
	#
	foreach my $line (@file_contents)
	{
		chomp $line;
		if (($line ne "") && ($line !~ /^#/))
		{
			my @arr = split (/=/, $line);
			$arr[0] =~ s/\s+$//g;
			$arr[1] =~ s/^\s+//g;
			$arr[1] =~ s/\"//g;
			$arr[1] =~ s/\s+$//g;
			$amethyst_vars->{$arr[0]} = $arr[1];
		}
	}
	return $amethyst_vars;
}