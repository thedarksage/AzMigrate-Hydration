#!/usr/bin/perl -w
use strict;

sub ReadConfigFile
{

	my $configFile = $_[0];
	my $configName, my $configVal;

	my %configHash;

	open( CONFIG_FILE, "< $configFile" ) or  die "Can't read config file\n";

	while( <CONFIG_FILE> )
	{
		chomp;
		
		if ( !  ($_ =~ /\:/ ) )
		{
			die  "$_ : Please check format in config file\n";

		}

		($configName, $configVal) = split(':', $_, 2);
		

		 $configName =~ s/^ *//;
		 $configName =~ s/ *$//;

		 $configVal =~ s/^ *//;
		 $configVal =~ s/ *$//;

		$configHash{$configName} = $configVal;

	}

	return %configHash;

}
###############################################################################
#Name:		PrintConfigFile
#Description:	Prints the config file to console
#Input:		Reference to the config hash
#Returns	None
#Affects:
###############################################################################
sub PrintConfigFile
{
	my $configHash = $_[0];

	for my $key ( keys %$configHash )
	{
		print "$key ==> $$configHash{$key} \n";
	}

}

###############################################################################
#Name:		ValidateConfigFile
#Description:	MAke sure that minimum required fields are set
#Input:		Reference to the config hash
#Returns	1=true or o=false
#Affects:
###############################################################################
sub ValidateConfigFile
{
	my $configHash = $_[0];


	( $$configHash{"SourceIP"}) or die "No sourceIP\n";
	( $$configHash{"TargetIP"}) or die "No TargetIP\n";
	( $$configHash{"CXIP"}) or die "No CXIP\n";
	( $$configHash{"App"}) or die "No APP\n";


}

###############################################################################
#Name:		DisocerOracle
#Description:	Finds all volumes of oracle
#Input:		ConfigHash
#Returns	array of volume names
#Affects:
###############################################################################
sub DiscoverOracle
{
	my $configHash = $_[0];

	my @filearray;

	@filearray = GetOracleFileList($configHash);

	if( $$configHash{"Platform"} =~ /unix/)
	{

		my @mountPoints = GetUnixFileSystems( \@filearray );
	
		foreach my $mp ( @mountPoints)
		{
			print "Mount point = $mp\n";
		}

	}
	elsif ( $$configHash{"Platform"} =~ /windows/ )
	{
		GetWindowsDrives(@filearray);
	}
	else
	{
		die "Wrong platform";
	}

	
}

sub GetUnixFileSystems
{
	my $filearray = $_[0];
	my %temphash;
	my @mount_points;

	foreach my $file ( @$filearray)
	{
		if( length($file) > 0 )
		{
			my ( $first, $second, $junk) = split(/\//, $file);
			$second = "/$second";
			$temphash{$second} = "dummy";
		}
	}

	@mount_points = keys %temphash;

	return @mount_points;


}

sub GetOracleFileList
{
	my $configHash = $_[0];
	my ( $ORACLE_USER, $ORACLE_HOME, $ORACLE_SID );

	( $$configHash{"ORACLE_HOME"}) or die "No ORACLE_HOME\n";
	( $$configHash{"ORACLE_SID"}) or die "No ORACLE_SID\n";
	( $$configHash{"ORACLE_USER"}) or die "No ORACLE_USER\n";

	$ORACLE_HOME = $$configHash{"ORACLE_HOME"};
	$ORACLE_SID =  $$configHash{"ORACLE_SID"};
	$ORACLE_USER = $$configHash{"ORACLE_USER"};


	my $logfile='\\\\\$logfile';
	my $controlfile='\\\\\$controlfile';

 	my $files_line=` su - $ORACLE_USER -c "export ORACLE_SID=$ORACLE_SID;$ORACLE_HOME/bin/sqlplus -S '/as sysdba' <<END 
                                set head off;
				set feedback off;
                                select member from v$logfile
				union
                                select name from v$controlfile
				union
                                select file_name from dba_data_files;
END"`;
	#print "$files_line\n";

	my @files = split(/\n/, $files_line);

	return @files;
}


BEGIN{

	my %configHash;

	%configHash = ReadConfigFile("/tmp/test");
	ValidateConfigFile(\%configHash);

	if ( (uc $configHash{"App"}) =~ /ORACLE/ )
	{
		DiscoverOracle(\%configHash);
	}
	elsif( ( uc $configHash{"App"}) =~ /MYSQL/ )
	{ 
#		DiscoverMysql;
	}
	else
	{
		die " $configHash{\"App\"} is not supported. Please check spelling \n";
	}


}
