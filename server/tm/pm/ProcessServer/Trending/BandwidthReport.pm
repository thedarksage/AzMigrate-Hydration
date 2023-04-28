#=================================================================
# FILENAME
#   BandwidthTrends.pm
#
# DESCRIPTION
#    This perl module is responsible for creating/updating
#    bandwidth RRD file for each host downloading/uploading
#    data from a process server
#
# HISTORY
#     11 Dec 2008  hpatel    Created.
#
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================
package ProcessServer::Trending::BandwidthReport;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use tmanager;
use Utilities;
use Time::Local;
use Data::Dumper;
use POSIX qw(ceil);
use ConfigServer::Trending::HealthReport;

#
# This sub routine used for to initialized the varibles.
#
sub new
{
	my ($class, %args) = @_;
	my $self = {};

	while (my($key, $value) = each %args)
	{
	    $self->{$key} = $value;
	}
	if ($self->{'filename'} ne "")
	{
		open (FILEH, "> $self->{'filename'}") 
			|| die "Unable to write to $self->{'filename'}: $!\n";
	}
	bless ($self, $class);
}

#
# Get Date Range based upon user inputs
#
sub getDateRange
{
	my $self = shift;
	
	my $startDate;
	my $endDate;
			  
	if (($self->{'from'} ne "") && ($self->{'to'} ne ""))
	{
		$startDate = $self->{'from'};
		$endDate = $self->{'to'};
	}
	elsif ($self->{'daily'} == 1)
	{	
		#my ($second, $minute, $hour, $dayOfMonth, $month, $yearOffset, $dayOfWeek, $dayOfYear, $daylightSavings) = localtime();
		my ($day, $mon, $year) = (localtime(time))[3,4,5];
		$year += 1900;
		$mon += 1;

		$startDate = "$mon/$day/$year";
		$endDate = "$mon/$day/$year";
	}
	elsif ($self->{'weekly'} == 1)
	{
		($startDate, $endDate) = &getWeekRange();
	}
	elsif ($self->{'monthly'} == 1)
	{
		($startDate, $endDate) = &getMonthRange();
	}
	elsif ($self->{'yearly'} == 1)
	{
		my ($year) = (localtime(time))[5];
		$year += 1900;
		
		$startDate = "01/01/$year";
		$endDate = "12/31/$year";		
	}	
	
	return ($startDate, $endDate);
}

#
# Formats data for the report
#
sub humanReadable()
{
	my $self = shift;
	my $size = shift;

	my @names;
	@names = ('B', 'KB', 'MB', 'GB', 'TB');
	my $times = 0;

	while($size>1024)
	{
		$size = ceil(($size*100)/1024)/100;
		$times++;
	}
	
	$size = sprintf('%8.2f', $size);
	my $str = "$size " . $names[$times];
    
	return $str;
}

#
# Returns Start Date & End date of Current Month
#
sub getMonthRange()
{
    my $self = shift;

    my (@mon, $mon, $year);

    @mon = (31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);
	($mon, $year) = (localtime(time))[4,5];

    $mon++;
 
	$year += 1900 if $year < 2000;
	if ($mon == 0) 
	{
		$mon = 12;
		$year--;
	}
	if ($mon == 2) 
	{
		if ($year % 4 != 0) 
		{
			$mon[1] = 28;
		}
		elsif ($year % 400 == 0) 
		{
			$mon[1] = 29;
		}
		elsif ($year % 100 == 0) 
		{
			$mon[1] = 28;
		}
		else 
		{
			$mon[1] = 29;
		}
	}

	my $st_date = "$mon/01/$year";
	my $en_date = "$mon/$mon[$mon-1]/$year";

	return ($st_date, $en_date);
}

#
# Returns Start Date & End date of Current week
#
sub getWeekRange
{
	my $self = shift;

	my @dt = localtime(time);
	
	my $first_day = 0 - $dt[6];
	my $last_day = 6 - $dt[6];
	
	my $nextTimeStamp = removeDays($first_day, @dt);
	my ($dayOfMonth, $month, $yearOffset) = (localtime($nextTimeStamp))[3,4,5];
	my $year = 1900 + $yearOffset;
	my $mon = $month+1;
	
	my $startDate = "$mon/$dayOfMonth/$year";

	my $nextTimeStamp = addDays($last_day, @dt);
	my ($dayOfMonth, $month, $yearOffset) = (localtime($nextTimeStamp))[3,4,5];
	my $year = 1900 + $yearOffset;
	my $mon = $month+1;
	
	my $endDate = "$mon/$dayOfMonth/$year";
	
	return ($startDate, $endDate);
}

sub addDays($@)
{
	my ($numDays,@dt) = @_;
	my $seconds = $dt[0];
	my $minutes = $dt[1];
	my $hours   = $dt[2];
	my $day     = $dt[3];
	my $month   = $dt[4];
	my $year    = $dt[5];
	
	my $resultTimeStamp; 
	if($numDays <= 0)
	{
	  $resultTimeStamp = timelocal( $seconds, $minutes, $hours, $day, $month, $year );
	}
	else
	{
	  #get Number of days for a month	
	   my $monthDays=getDaysInAMonth(@dt);
	   my $newDays = $dt[3]+$numDays;
	   if($newDays > $monthDays)
	   {
	   	 my $diff= $monthDays - $day+1;
	   	 if($month==11)
	   	 {
	   	   $year += 1;
	   	   $month=0;	
	   	 }
	   	 else
	   	 {
	   	   $month=$month+1;	
	   	 }
	   	 
	   	 my $nextTimeStamp = timelocal( $seconds, $minutes, $hours,1, $month, $year );
	   	 my @nextTimeStampAry = localtime($nextTimeStamp);
	   	 $numDays -= $diff;
	   	 if($numDays>0)
	   	 {
	   	  $resultTimeStamp=addDays($numDays,@nextTimeStampAry);
	   	 }
	   	 else
	   	 {
	   	   $resultTimeStamp=$nextTimeStamp; 	
	   	 }
	   } 
	   else
	   {
	   	$resultTimeStamp = timelocal( $seconds, $minutes, $hours, $newDays, $month, $year );
	   }	
	}
	
	return $resultTimeStamp;
}

sub removeDays($@)
{
	my ($numDays,@dt) = @_;
	my $seconds = $dt[0];
	my $minutes = $dt[1];
	my $hours   = $dt[2];
	my $day     = $dt[3];
	my $month   = $dt[4];
	my $year    = $dt[5];
	
	my $resultTimeStamp; 
	if($numDays >= 0)
	{
	  $resultTimeStamp = timelocal( $seconds, $minutes, $hours, $day, $month, $year );
	}
	else
	{
	  #get Number of days for a month	
	   #my $monthDays=getDaysInAMonth(@dt);
	   my $newDays = $day+$numDays; # can be negitive
	   if($newDays < 0)
	   {
	   	 if($month==0)
	   	 {
	   	   $year -= 1;
	   	   $month=11;	
	   	 }
	   	 else
	   	 {
	   	   $month=$month-1;	
	   	 }
	   	 
	   	 my $nextTimeStamp = timelocal( $seconds, $minutes, $hours,1, $month, $year );
	   	 my @nextTimeStampAry = localtime($nextTimeStamp);
	   	 my $monthDays=getDaysInAMonth(@nextTimeStampAry);
	   	 
	   	 my $diff = $day; #Positive
	   	 $numDays += $diff;
	   	 
	   	 if($numDays<0)
	   	 {
	   	  my $nextTimeStamp = timelocal( $seconds, $minutes, $hours,$monthDays, $month, $year );
	   	  my @nextTimeStampAry = localtime($nextTimeStamp);
	   	  $resultTimeStamp=removeDays($numDays,@nextTimeStampAry);
	   	 }
	   	 else
	   	 {
	   	   $resultTimeStamp=$nextTimeStamp; 	
	   	 }
	   } 
	   else
	   {
	   	$resultTimeStamp = timelocal( $seconds, $minutes, $hours, $newDays, $month, $year );
	   }	
	}
	
	return $resultTimeStamp;
}

sub getDaysInAMonth(@)
{
  #initialize the variable
  my ( @dt) = @_;
  my $month   = $dt[4];
  my $year    = $dt[5];
  my $numdays;		
  if($month==0 || $month==2 || $month==4 ||$month==6 ||$month==7 || $month==9 || $month==11 )
  {
  	 $numdays=31;
  }
  elsif($month==3 || $month==5 || $month==8 ||$month==10) 
  {
  	 $numdays=30;
  }
  else
  {
  	if(uc(isLeapYear($month)) eq "TRUE")
  	{
  	  $numdays=29;
  	}
  	else
  	{
  	  $numdays=28;	
  	}
  }
}

sub isLeapYear($)
{
	#Initialize Variables
	my ( $year) = @_;
	my $result="false";
	
	#Check if a Given year is a leap year
	if((($year % 4 == 0) && ($year % 100 != 0)) || ($year % 400 == 0))
	{
	 $result="true";	
	}
	#Return Result
	return $result;
}

#
# Converts date to Time Stamp
#
sub getTimeStamp
{
	my $self = shift;
	my $start_date = shift;
	my $end_date = shift;

	my @arr_st = split(/\//, $start_date);
	my @arr_en = split(/\//, $end_date);

	$start_date = sprintf('%04d-%02d-%02d', $arr_st[2], $arr_st[0], $arr_st[1])." 00:00:00";
	$end_date = sprintf('%04d-%02d-%02d', $arr_en[2], $arr_en[0], $arr_en[1])." 23:58:59";

	#print("$start_date, $end_date\n");

	my $st_dt = Utilities::datetimeToUnixTimestamp($start_date);
	my $en_dt = Utilities::datetimeToUnixTimestamp($end_date);

	return ($st_dt, $en_dt);
}

#
# Generates the Report data and Print on CLI
#
sub displayReport 
{
	my $self = shift;
	my $start = shift;
	my $end = shift;

	my $file = $self->{'base_dir'}.'/'.$self->{'host_id'}.'/bandwidth.rrd';
	my $result_file = $self->{'base_dir'}.'/'.$self->{'host_id'}.'/bandwidth_cli.txt'; 
	
	unless(-e $file)
	{
		print "\n\t RRD file is not available ($file)\n\n";
		return;		
	}
	ConfigServer::Trending::HealthReport::fetchRrd($file,$result_file,$start,$end);
	#my $cmd = $self->{'rrd_command'}.' fetch "'.$file.'" AVERAGE --start '.$start.' --end '.$end;

	open(RRDFETCH, "$result_file") or die ("Could not open file $result_file \n$!");
	
	my $st_dt = Utilities::unixTimestampToDate($start);
	my $en_dt = Utilities::unixTimestampToDate($end);
	
	my $label;
	
	if($self->{'mode'} eq 'hourly')
	{
		$label = "DATE\t\tHOUR";
	}
	elsif($self->{'mode'} eq 'monthly')
	{
		$label = "MONTH\t";
	}		
	else
	{
		$label = "DATE\t";
	}	
	
	
	$self->generateOutput("\n----------------------------------------------------------------------------------------------\n");
	$self->generateOutput("\t\t\tBANDWIDTH REPORT [ $st_dt to $en_dt ]");
	$self->generateOutput("\n----------------------------------------------------------------------------------------------\n");
	$self->generateOutput("\t$label\t    IN\t\t    OUT\t\t    MAX\t\t    SUM ");
	$self->generateOutput("\n----------------------------------------------------------------------------------------------");
 	

	my ($mday_tmp, $cnt);
	my ($incoming_day, $outgoing_day, $max_day, $sum_day) = (0, 0, 0, 0);
	my ($incoming_tot, $outgoing_tot, $max_tot, $sum_tot) = (0, 0, 0, 0);
	
	my $dt;
	my $step = 300;
	while (<RRDFETCH>)
   	{
		chop;
		my @fields = split(/\s+/);
		
		my $start = shift @fields;
		my $incoming = shift @fields;
		my $outgoing = shift @fields;
		
		next unless ($start =~ /^\d+:$/);
		$start =~s/://g ;
		
		my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($start);

		unless ($incoming =~ /^\d/)
		{
			$incoming = 0;
		}
		
		unless ($outgoing =~ /^\d/)
		{
			$outgoing = 0;
		}		
		
		$mon += 1;
		$year += 1900;
		
		if($self->{'mode'} eq 'monthly')
		{
			$mon = sprintf('%02d', $mon);
			$dt = "$mon/$year    ";
		}
		elsif($self->{'mode'} eq 'hourly')
		{
			$mon = sprintf('%02d', $mon);
			$mday = sprintf('%02d', $mday);		
			$hour = sprintf('%02d', $hour);
			$dt = "$mon/$mday/$year\t$hour";
		}
		else
		{
			$mon = sprintf('%02d', $mon);
			$mday = sprintf('%02d', $mday);
			$dt = "$mon/$mday/$year";
		}
			
		if($mday_tmp eq $dt) 
		{
			
			$incoming_day += $incoming;
			$outgoing_day += $outgoing;
			
			$cnt++;
		}
		else
		{	
			if($mday_tmp ne "")
			{
				if($incoming_day > $outgoing_day) 
				{
					$max_day = $incoming_day; 
				} 
				else 
				{
					$max_day = $outgoing_day;
				}
								
				$sum_day = $incoming_day + $outgoing_day;
				
				$incoming_tot += $incoming_day;
				$outgoing_tot += $outgoing_day;				
				$max_tot += $max_day;
				$sum_tot += $sum_day;
			
				my $tmp_in = $self->humanReadable($incoming_day);
				my $tmp_out = $self->humanReadable($outgoing_day);
				my $tmp_max = $self->humanReadable($max_day);
				my $tmp_tot = $self->humanReadable($sum_day);
				
				$self->generateOutput("\n\t$mday_tmp\t$tmp_in\t$tmp_out\t$tmp_max\t$tmp_tot");
				
				$incoming_day = 0;
				$outgoing_day = 0;
				$cnt = 0;
			}
			$mday_tmp = $dt;
		}
	}
	close RRDFETCH;
	
	if($cnt > 0)#($self->{'custom'} == 1) && ($self->{'mode'} eq 'monthly') && $mday_tmp eq $dt) 
	{
		if($incoming_day > $outgoing_day) 
		{
			$max_day = $incoming_day; 
		} 
		else 
		{
			$max_day = $outgoing_day;
		}
		
		$sum_day = $incoming_day + $outgoing_day;
		
		$incoming_tot += $incoming_day;
		$outgoing_tot += $outgoing_day;				
		$max_tot += $max_day;
		$sum_tot += $sum_day;
	
		my $tmp_in = $self->humanReadable($incoming_day);
		my $tmp_out = $self->humanReadable($outgoing_day);
		my $tmp_max = $self->humanReadable($max_day);
		my $tmp_tot = $self->humanReadable($sum_day);
		
		$self->generateOutput("\n\t$mday_tmp\t$tmp_in\t$tmp_out\t$tmp_max\t$tmp_tot");
	}		
	
	$incoming_tot = $self->humanReadable($incoming_tot);
	$outgoing_tot = $self->humanReadable($outgoing_tot);
	$max_tot = $self->humanReadable($max_tot);
	$sum_tot = $self->humanReadable($sum_tot);
        
	$self->generateOutput("\n----------------------------------------------------------------------------------------------\n");
	if($self->{'mode'} eq 'hourly')
	{
		$self->generateOutput("\tTOTAL\t\t\t$incoming_tot\t$outgoing_tot\t$max_tot\t$sum_tot\n");
	}
	else
	{
		$self->generateOutput("\tTOTAL\t\t$incoming_tot\t$outgoing_tot\t$max_tot\t$sum_tot\n");
	}
	$self->generateOutput("----------------------------------------------------------------------------------------------\n");
}

sub generateOutput
{
	my $self = shift;
	my $string = shift;
	if ($self->{'filename'} ne "")
	{
		print FILEH "$string";
		print $string;
	}
	else
	{
		print $string;
	}
}
 
1;
