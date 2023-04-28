#!/usr/bin/perl
#=================================================================
# FILENAME
#    BandwidthProvision.pm
#
# DESCRIPTION
#    This file is used for bpm operations.
# HISTORY
#     <24/02/2010>  skondamuri Created.
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================

package BandwidthProvision;
use strict;
use DBI();
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use Time::Local;
use Sys::Hostname;
use tmanager;
use Utilities;
use TimeShotManager;
use Common::DB::Connection;
use Config;
use Data::Dumper;
use PHP::Serialization qw(serialize unserialize);
use POSIX;
require Messenger;
#Global Variables

my $AMETHYST_VARS   = Utilities::get_ametheyst_conf_parameters();
my $BASE_DIR_TMP    = $AMETHYST_VARS->{"INSTALLATION_DIR"}; 

my $BPM_LNK_SPEED   = $AMETHYST_VARS->{"BPM_LNK_SPEED"};
my $HOST_GUID       = $AMETHYST_VARS->{"HOST_GUID"};
my $SVSYSTEM_BIN    = $AMETHYST_VARS->{"SVSYSTEM_BIN"};
my $BPM_OPTION    	= $AMETHYST_VARS->{"BPM_OPTION"};
my $is_linux 		= Utilities::isLinux();
my $bpm_rule_file 	= ($is_linux) ? "bpm_rules.sh" : "bpm_rules.ps1";

our $newupdateDate;
our $updateRequired;
our $message;

my $BASE_DIR = Utilities::makePath($BASE_DIR_TMP);
my $todayTime        = time();    
my @today            = localtime($todayTime);    
my $type             = "WAN";
my $host = hostname();

sub new
{
	my ($class) = shift;
	my $self = {};
	bless ($self, $class);
}

##
#function: This function checks, if the current time is between a time period
#Date: May 17,2005
#The Hours and Seconds are padded with leading zero to be consitant for comparision
##
sub isCurrentTimeBetweenScheduleTime
{
    #Initialize variables
    my ( $fromTime, $toTime ) = @_;
    my $result      = "false";
    my @aryFromTime = split( "[:]", $fromTime );
    my @aryToTime   = split( "[:]", $toTime );
    #If HH or MM are single digit pad them with an leading zero(For from and To Time)
    if ( length( $aryFromTime[0] ) == 1 )
    {
        $aryFromTime[0] = "0" . $aryFromTime[0];
    }
    if ( length( $aryFromTime[1] ) == 1 )
    {
        $aryFromTime[1] = "0" . $aryFromTime[1];
    }
    if ( length( $aryToTime[0] ) == 1 )
    {
        $aryToTime[0] = "0" . $aryToTime[0];
    }
    if ( length( $aryToTime[1] ) == 1 )
    {
        $aryToTime[1] = "0" . $aryToTime[1];
    }
    #Construct a Number Format for From and To Time
    my  $intFromTime = $aryFromTime[0] . $aryFromTime[1];
    my $intToTime   = $aryToTime[0] . $aryToTime[1];
    
    #If HH or MM are single digit pad them with an leading zero(For Current Date)
    if ( length( $today[2] ) == 1 )
    {
        $today[2] = "0" . $today[2];
    }
    if ( length( $today[1] ) == 1 )
    {
        $today[1] = "0" . $today[1];
    }
    
    #Construct a for today
    my $curHours   = $today[2];
    my $curMinutes = $today[1]; 
    my $curTime    = $curHours . $curMinutes;
    
    #Do the Comparision
    if($intToTime<$intFromTime)
    {
        if($curTime <= $intToTime || $curTime >= $intFromTime)
        {
            $result="true";    
        }
    }
    else
    {
        if ( $curTime >= $intFromTime && $curTime <= $intToTime )
        {
            $result = "true";
        }
    } 
    
    #Return the Result
    return $result;
}

##
#function: This function checks, If the current BW policy is applicable as on now, The most critical part
#Date: May 17,2005
##
sub isPolicyApplicable
{
     
	my ( $type , $schedule , $lastRunDate ) = @_;
    #Initialize the local state variables
    my $result = "false";
    my @arySchedule = split("[,]",$schedule);
    my $arySize = @arySchedule;

    #Reset global variables
    $updateRequired="false";

    #Parse the String and get int value
    my $lastTimeStamp = getIntValueForDate($lastRunDate);
    my @lastTimeStampAry = localtime($lastTimeStamp);

    # print "In isPolicyApplicable: TYPE: $type  LASTTIMESTAMP: $lastTimeStamp\n";

    if($type==0) #Default Policy, Selected, when no policy is applicable
    {
        $result = "true";
    }
    elsif($type==1) # Policy Selection Logic for Daily
    {
        #Get Difference(in days) between lastRunDate(in Database) and Current Date
        my $diff = dateDiff("d", $todayTime, $lastTimeStamp);
        if (uc($arySchedule[0]) eq "PERIODIC") #If the Policy is based on periodic intervals.
        {
            if ($diff < 0)
            {
                #Select, If the lastRunDay is less than today, It would also
                #call for updation of the lastRunDay.

                #Get the Duration
                my $dur = $arySchedule[1];
                #verify, if the date after bumping up by dur can occur today.    
                my %results = verifyIfDateCanOccurToday($lastRunDate,$dur);
                my $dateOk = $results{"result"};     
                #If yes, then set policy as selected
                if(uc($dateOk) eq "TRUE")
                {
					$result = "true";
                }

                #set updation required flags
                $updateRequired="true";
                $newupdateDate=$results{"nearestDateFormatted"}; 
            }
            elsif($diff==0)
            {
                #if lastRunDate is same as today,then select the policy.
                $result = "true";
            }

        } 
        else #if, the Policy can be run every day 
        {
            #If the lastRunDate is less than currentDay, then policy is selected and also lastRunDate is
            #updated
            if ($diff < 0)
            {
                $result = "true";    
                $updateRequired="true";
                $newupdateDate=getFormatedToday(); 
            }
            elsif($diff==0)
            {
                #If lastRunDate and Current Day are the same, then policy is selected
                $result = "true";    
            }
        }
    }
    elsif($type==2) #Policy Selection Logic for Weekly 
    {
        # If, for some reason, no weekdays are specified, the policy will not be process any further
        if($arySize>=2)
        {
            #Get the duration and difference between lastRunDate and CurrentDate is weeks.
            my $dur = $arySchedule[0];    
            my $wdiff = dateDiff("d",$lastTimeStamp,$todayTime)/7;

            #Get, if today's weekday is present in the policy weekdays
            my $isPresent = checkIfTodayIsList(@arySchedule);

            #Select the Policy, if only if, the below criteria is met
            if( ($dur>=1  || $wdiff >= $dur) && (uc($isPresent) eq "TRUE") )
            {
                $result="true";
            }

            #Updation of the lastRunDate shall be done, only if it meets the below criteria
            if($wdiff >= $dur)
            {
                $updateRequired="true";
                my $nextWeekTimeStamp = dateAdd("w",$dur,@lastTimeStampAry);
                $newupdateDate=getFormatedDate($nextWeekTimeStamp);
            }    
        }
    }
    elsif($type==3) #Policy Selection Logic for Monthly 
    {
        my $dur;
        my $tday = $today[3];
        my $dsame ="false";

        if (uc($arySchedule[0]) eq "PERIODIC") #If the Policy is based on periodic intervals.
        {
            #Get the duration and date specified in databse
            $dur = $arySchedule[2];
            my $sday = $arySchedule[1];
            #Checking, if the day is the same
            if($sday eq $tday)
            {
                $dsame="true";
            } 
        }
        elsif (uc($arySchedule[0]) eq "SPECIFIC")
        {
            #Get the duration,occurance and wkday type
            $dur = $arySchedule[3];
            my $specifiedOccurance = $arySchedule[1];
            my $specifiedWkdayType = $arySchedule[2];
            #Verify, the criteria has been met.
            $dsame=isTodayAsPerSpecifiedCriteria($specifiedOccurance,$specifiedWkdayType);
        }    

        #Projecting next execution day after adding duration
        my @nextDate = localtime(dateAdd("m",$dur,@lastTimeStampAry));

        #Checking,if the projected date's and today's MM & YYYY are the same
        my $projectedSame="false";
        if($nextDate[4]==$today[4] && $nextDate[5]==$today[5])
        {
            $projectedSame="true";
        }
        #Select, the policy, if the following criteria is met
        if( ($dur>=1 || uc($projectedSame) eq "TRUE")    && $dsame )
        {
            $result="true";
        }
        #Updating record
        my $mdiff = dateDiff("m",$lastTimeStamp,$todayTime);
        #Update Last Run date, if the following criteria is met
        if($mdiff>=$dur)
        {
            $updateRequired="true";
            my $nextWeekTimeStamp = dateAdd("m",$dur,@lastTimeStampAry);
            $newupdateDate=getFormatedDate($nextWeekTimeStamp);
        }    

    }
    elsif($type==4) #Policy Selection Logic for Yearly 
    {
        my $dur;
        my $tmonth = $today[4];
        my $tday =$today[3];
        my $ysame ="false";

        if (uc($arySchedule[0]) eq "PERIODIC") #If the Policy is based on periodic intervals.
        {
            my $pmonth = getMonthNum($arySchedule[1]);    
            my $pday  = $arySchedule[2];
            #IF current Month and Date are the same, Then selected the policy
            if($tmonth == $pmonth && $tday == $pday) {$ysame="true";}
        }
        elsif (uc($arySchedule[0]) eq "SPECIFIC")
        {
            my $smonth=getMonthNum($arySchedule[3]);
			my $tmonth;
            if($tmonth==$smonth)
            {
                my $wkCriteria = uc(isTodayASpecifiedWeekday($arySchedule[1],$arySchedule[2]));    
                if($wkCriteria eq "TRUE") {$ysame="true"};    
            }
        }
        if(uc($ysame) eq "TRUE")
        {
            $result="true";
            my $diff = dateDiff("d", $todayTime, $lastTimeStamp);
            #update, if lastRunDate is less than today
            if($diff < 0)
            {
                $updateRequired="true";
                $newupdateDate=getFormatedToday(); 
            }
        }
    }
    return $result;
}

##
#function: This function sets the currently active BW policy in transbandSettings table
#Date: June 21,2005
##
sub unsetActivePolicy
{
    my ($nwdevice,$id) = @_;
	my $input;
	$input->{0}->{"queryType"} = "UPDATE";
	$input->{0}->{"tableName"} = "BPMPolicies";
	$input->{0}->{"fieldNames"} = "isActivePolicy = 0";
	$input->{0}->{"condition"} = "sourceHostId='$HOST_GUID' AND networkDeviceName = '$nwdevice' AND id = '$id'";
	
	return $input;
}


##
# function: Enable Bandwidth Provisioning Process
##
sub enableBandwidthProvisioningProcess
{
    my ($nwdevice, $id, $networkDeviceName , $share , $cumulativeBandwidth , $policyName , $sourceIP, $data_hash) = @_;      
    #Initialize local state variables
    my $currentPolicyID = $id;
    my @batch;
	my $result = &generate_tc_rules($id, $networkDeviceName, $share, $cumulativeBandwidth, $policyName, $sourceIP, $data_hash);
	if($result == 1)
	{
		my $input_data =  &setActivePolicy($currentPolicyID,$nwdevice);
		my @input = @$input_data;
		push(@batch, @input);
		
		## Check if Updation to the Database is required
		if(uc($updateRequired) eq "TRUE")
		{
			my $input;
			$input->{0}->{"queryType"} = "UPDATE";
			$input->{0}->{"tableName"} = "BPMPolicies";
			$input->{0}->{"fieldNames"} = "lastRunDate='$newupdateDate'";
			$input->{0}->{"condition"} = " Id = $currentPolicyID";
			push(@batch, $input);
		}
	}
	return \@batch;	
}

##
#function: This function sets the currently active BW policy in transbandSettings table
#Date: June 21,2005
##
sub setActivePolicy
{
    my ($policyid, $nwdev) = @_; 
    my $input;
	my @batch;
	my $i=0;
    #
    # De-activate othet policies as at the same time
    # only one pair can be active
    #
	$input->{$i}->{"queryType"} = "UPDATE";
	$input->{$i}->{"tableName"} = "BPMPolicies";
	$input->{$i}->{"fieldNames"} = "isActivePolicy = 0";
	$input->{$i}->{"condition"} = "Id != '$policyid' AND sourceHostId='$HOST_GUID' AND networkDeviceName = '$nwdev'";
	push(@batch, $input);
    #
    # Activate the current policy
    #
	$i++;
	$input->{$i}->{"queryType"} = "UPDATE";
	$input->{$i}->{"tableName"} = "BPMPolicies";
	$input->{$i}->{"fieldNames"} = "isActivePolicy = 1";
	$input->{$i}->{"condition"} = "Id = '$policyid'";
	push(@batch, $input);
	
	return \@batch;
}



sub getIntValueForDate
{
    my ( $dt ) = @_;    
    my @parseDt = split("[-]",$dt);
    my $timestamp = timelocal( 0, 0, 0, $parseDt[2], ($parseDt[1]-1), ($parseDt[0]-1900) );    
    return $timestamp;    
}

##
#function: Gets Date from time stamp in YYYY-mm-dd format
#Date: May 21,2005
##

sub getFormatedDate
{
    #initialize the variables
    my ( $timestamp ) = @_;        
    my @dt = localtime($timestamp);
    my $year=  $dt[5]+1900; 
    my $month=  $dt[4]+1;
    my $day=  $dt[3];
    
    #Pad with leading zero,if the month is a single digit number
    if(length($month)!=2)
    {
      $month = "0".$month;    
    }
    if(length($day)!=2)
    {
      $day = "0".$day;    
    }
    #Format the Date as per YYYY-mm-dd format
    my $dtFormated =$year . "-" .$month. "-" . $day; 
    return $dtFormated;
}

#
# Function name : getFormatedDateTime
# Description : 
# Function used to get the time in YYYY-MM-DD hh:mm:ss format.
# Input  : None
# Output : Formated time 
#
sub getFormatedDateTime
{
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();
    my $formatted_time = sprintf "%4d-%02d-%02d %02d:%02d:%02d",$year+1900,$mon+1,$mday,$hour,$min,$sec;
    return $formatted_time;
}

##
# function: Gets today's day in YYYY-mm-dd format
# Date: May 21,2005
##
sub getFormatedToday
{
    return getFormatedDate($todayTime);
}

##
# This function verifies if the supplied date can ever be today for periodic duration d
# Date: May 21,2005
##
sub verifyIfDateCanOccurToday
{
    #Initialize the variables
    my ( $originalDate,$dur) = @_;        
    my $originalTimeStamp = getIntValueForDate($originalDate);
    my $prevDiff;
    my $prevDateTimestamp=$originalTimeStamp;
    my $result ="false";
    my %results;
    
    #Bump the Original Date, to verify, if the bumped date is same as the today
    while("true")
    {
        my $diff= dateDiff("d",$todayTime,$prevDateTimestamp);
         if($diff == 0)
         {
           $result="true";
           $prevDiff=0;
           $prevDateTimestamp=$todayTime;
           goto exitprocess;    
         }
         elsif($diff <0 )
         {
            $prevDiff=$diff;    
            $prevDateTimestamp=dateAdd("d",$dur,localtime($prevDateTimestamp));
         }
         elsif($diff>0)
         {
           goto exitprocess;    
         }        
    }
    
  exitprocess:    
  #Construct the result hash list and pass it to the caller 
 
  $results{"result"} = $result;
  $results{"nearestDiff"} = $prevDiff;
  $results{"nearestDateFormatted"} = getFormatedDate($prevDateTimestamp);
  $results{"nearestDateTimeStamp"} = $prevDateTimestamp;
  #return the result
  return %results;
}

##
#function: Returns number of intervals between two days
#Date: May 17,2005
#Interval Formt:m-months,w-weeks,d-days,h-hours,n-minutes,s-seconds
##
sub dateDiff
{
    my ( $interval, $date1UTC, $date2UTC ) = @_;

    # get the number of seconds between the two dates
    my $timedifference = $date2UTC - $date1UTC;
    my $retval;
    
    if($interval eq "w") { $retval = $timedifference / 604800; }
    elsif($interval eq "d") { $retval = $timedifference / 86400 ;}
    elsif($interval eq "h") { $retval = $timedifference / 3600 ;}
    elsif($interval eq "n") { $retval = $timedifference / 60 ;}
    elsif($interval eq "s") { $retval = $timedifference ;}
    elsif($interval eq "m") 
        {
          my @date2 = localtime($date2UTC);    
          my @date1 = localtime($date1UTC);        
          
          $retval=$date2[4]-$date1[4];
          #Precision need be adjusted based on date of the months
          if($retval!=0)
          {
            if($date2[3]<$date1[3]) {$retval--;}
          }    
        }
    #Return the results
    return roundDates($retval);
}


##
#function: This function adds interval to given date
#Date: May 17,2005
#Interval Formt:y - years, m-months, d-days,w-weeks 
##
sub dateAdd
{
    my ( $interval, $number, @dt ) = @_;
    my $seconds = $dt[0];
    my $minutes = $dt[1];
    my $hours   = $dt[2];
    my $day     = $dt[3];
    my $month   = $dt[4];
    my $year    = $dt[5];
    
    my $timestamp;
    
    if($interval eq 'y') { $year    += $number; }
    elsif($interval eq 'm')
    {
            if($number>=0)
            {
               $timestamp=addMonths($number,@dt);
            }
            else
            {
               $timestamp=removeMonths($number,@dt);    
            }
    }
    elsif($interval eq 'd')
    {
           if($number>=0)
            {
               $timestamp=addDays($number,@dt);
        }
        else
        {
           $timestamp=removeDays($number,@dt);    
        }
    } 
    elsif($interval eq 'w')
    { 
           my $tdays=($number * 7 );
           if($number>=0)
            {
                $timestamp=addDays($tdays,@dt);
            }
            else
            {
                $timestamp=removeDays($tdays,@dt);    
            }     
    }
    if($timestamp eq 'null')
    {
       $timestamp = timelocal( $seconds, $minutes, $hours, $day, $month, $year );
    }  
    return $timestamp;
}

##
#function: Add Months a date, it is a recursive function
#Date: March 23th,2005
##
sub addMonths
{
    #Initialize variables
    my ($number, @dt)=@_;    
      my $seconds = $dt[0];
    my $minutes = $dt[1];
    my $hours   = $dt[2];
    my $day     = $dt[3];
    my $month   = $dt[4];
    my $year    = $dt[5];
    my $timestamp;
    
    #Main Processing Logic - Tricky 
    
    if($month+$number>11)
    {
         my $adYears = int($number/12);
         my $diff = ($number)-($adYears * 12) ;
         if($month + $diff > 11)
         {
            $year+=1;
            $month=($month + $diff)-12;
         }
         else
         {
             $month+=$diff;
         }
         $year +=$adYears;
    }
    else
    {
      $month+=$number;    
    }

    #Date Rollover Logic
    my $temp = timelocal( $seconds, $minutes, $hours, 1, $month, $year );
     my $nextMonthDays = getDaysInAMonth(localtime($temp));
     if($day>$nextMonthDays)
     {
       $day=$nextMonthDays;
    }

    # Construct TimeStamp value for the next date
    $timestamp = timelocal( $seconds, $minutes, $hours, $day, $month, $year );             
    # Return Value
    return $timestamp
}
##
#function: Add Months a date, it is a recursive function
#Date: March 23th,2005
##
sub removeMonths
{
 #Initialize variables
    my ($number, @dt)=@_;    
      my $seconds = $dt[0];
    my $minutes = $dt[1];
    my $hours   = $dt[2];
    my $day     = $dt[3];
    my $month   = $dt[4];
    my $year    = $dt[5];
    my $timestamp;
    
    #Main Processing Logic - Tricky 
    
    if($month+$number<0)
    {
        my $adYears = int($number/12) * -1;   
        my $diff = (-1*$number)-($adYears * 12) ;
        if($month - $diff < 0)
         {
           $year-=1;
           $month=12-($diff-$month);
         }
         else
         {
             $month-=$diff;
         }
         $year -= $adYears;
        
    }
    else
    {
      $month+=$number;    
    }
        
     
    # Construct TimeStamp value for the next date
    $timestamp = timelocal( $seconds, $minutes, $hours, $day, $month, $year );             
    # Return Value
    return $timestamp

}

##
#function: Add days a date, it is a recursive function
#Date: March 23th,2005
##
sub addDays
{
    my ($numDays,@dt) = @_;
    my $seconds = $dt[0];
    my $minutes = $dt[1];
    my $hours   = $dt[2];
    my $day     = $dt[3];
    my $month   = $dt[4];
    my $year    = $dt[5];
    my $newDays;
    my $resultTimeStamp; 
    if($numDays <= 0)
    {
      my $resultTimeStamp = timelocal( $seconds, $minutes, $hours, $newDays, $month, $year );
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


##
#function: removes days from a date, it is a recursive function
#Date: March 23th,2005
##
sub removeDays
{
    my ($numDays,@dt) = @_;
    my $seconds = $dt[0];
    my $minutes = $dt[1];
    my $hours   = $dt[2];
    my $day     = $dt[3];
    my $month   = $dt[4];
    my $year    = $dt[5];
    my $newDays;
    my $resultTimeStamp; 
    if($numDays >= 0)
    {
      $resultTimeStamp = timelocal( $seconds, $minutes, $hours, $newDays, $month, $year );
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


##
#function: Check if,The current weekday is in the array of thesupplied days
#          Expected format is "Sunday","Monday", etc...
#Date: May 21,2005  
##
sub checkIfTodayIsList
{
     my (@dts) = @_;
    my $wday   = getWeekdayName($today[6]);
    my $result = "false";
	my $aday;
    foreach $aday (@dts)
    {
       if ( $wday eq uc($aday))
        {
            $result = "true";
            goto exitprocess;
        }
    }
    exitprocess:
    return $result;
}

###
#function:Get Weekday Number
#Note:The number is consistant with PERL and GNU scheme 
#Date: May 21,2005
###
sub getWeekdayNum
{
    my ( $strWeekdayName) = @_;
    my $result;
     
     if ( uc($strWeekdayName) eq "SUNDAY" )             {$result=0;}
     elsif ( uc($strWeekdayName) eq "MONDAY" )       {$result=1;}
     elsif ( uc($strWeekdayName) eq "TUESDAY" )     {$result=2;}
     elsif ( uc($strWeekdayName) eq "WEDNESDAY" )     {$result=3;}
     elsif ( uc($strWeekdayName) eq "THURSDAY" )     {$result=4;}
     elsif ( uc($strWeekdayName) eq "FRIDAY" )       {$result=5;}
     elsif ( uc($strWeekdayName) eq "SATURDAY" )     {$result=6;}
     else {$result=-1}
    
    return $result;    
}

###
#function: Get Month Number for a given month name
#Date:23 May,2005
###
sub getMonthNum
{
      #Initialize variables
      my ($monthName)=@_;    
      my $result=0;
    
    if ( uc($monthName) eq "JANUARY")     {$result=0;}
     elsif ( uc($monthName) eq "FEBRUARY")   {$result=1;}
     elsif ( uc($monthName) eq "MARCH")     {$result=2;}
     elsif ( uc($monthName) eq "APRIL")     {$result=3;}
     elsif ( uc($monthName) eq "MAY" )     {$result=4;}
     elsif ( uc($monthName) eq "JUNE")       {$result=5;}
     elsif ( uc($monthName) eq "JULY")     {$result=6;}
     elsif ( uc($monthName) eq "AUGUST")     {$result=7;}
     elsif ( uc($monthName) eq "SEPTEMBER")     {$result=8;}
     elsif ( uc($monthName) eq "OCTOBER")    {$result=9;}
     elsif ( uc($monthName) eq "NOVEMBER")      {$result=10;}
     elsif ( uc($monthName) eq "DECEMBER")    {$result=11;}
     
     else {$result=-1}
    
    return $result;    
}

###
#function:Get weekday name corresponding the number
#Note:Sunday starts with 0, which is consistant with Perl localtime and GNU scheme 
#Date: May 21,2005
###
sub getWeekdayName
{
    my ( $num) = @_;
    my $result;
     
     if ($num==0)         {$result="SUNDAY" ;}
     elsif ($num==1)     {$result="MONDAY";}
     elsif ($num==2)     {$result="TUESDAY";}
     elsif ($num==3)     {$result="WEDNESDAY";}
     elsif ($num==4)     {$result="THURSDAY";}
     elsif ($num==5)       {$result="FRIDAY";}
     elsif ($num==6)     {$result="SATURDAY";}
     else {$result=-1}
    
    return $result;    
}

##
#function: This function verifies, if today has met specified criteria
#          i.e. first weekend or fourth saturday  
#
##
sub isTodayAsPerSpecifiedCriteria
{
    #Initilize the variables
     my ($specifiedOccurance , $specifiedWday) = @_;
     my $specifiedOccuranceNum=1;
     
     #Initialize today's info
     my %twkdayInfo=getTodayWKdayInfo();
     my $todayOccuranceType; 
     
     #Intialize Result parameters
     my $result="false";
     
     #Do, the comparision
     if(uc($specifiedWday) eq "DAY")
     {
         my $mynum;
         if(uc($specifiedOccurance) eq "LAST") 
         {
             #If Last, Check if today is last day of the month
             $mynum=getDaysInAMonth(@today);
         }
         else
         {
               #Check, if today's day corresponds on specified day
               $mynum=getWordRepresentationOfANumber($specifiedOccurance);
         }
         if($today[3]==$mynum){$result="true";}
     } 
    else
    {
         #If last, copy the the status,if the occurance is last
          if(uc($specifiedOccurance) eq "LAST") {
              $todayOccuranceType=$twkdayInfo{"isOccuranceLast"};
          }else
          {
              $todayOccuranceType=$twkdayInfo{"occurance"};
          }    
     
          #Check if, Occurance and SpecifiedWeekday criteria matches 
         if((uc($todayOccuranceType) eq uc($specifiedOccurance)) && isTodayASpecifiedWeekday($specifiedWday) )
         {
             $result="true";
         }
    }
}


##
#function: Verifies, if the day is a type of day i.e. Weekday,Weekend,Sunday,etc.... 
#Date:May 22,2005
##
sub isTodayASpecifiedWeekday
{
     #Initilize the variables
     my ( $specifiedWday) = @_;
     my $specifiedWdayNumber;
     my $result="false";
 
     #Get todays number
     my $twdayNum= $today[6];
 
     #Get Number for the specified day
     if(uc($specifiedWday) eq "WEEKEND")
     {
           if($twdayNum == 6 || $twdayNum == 0) {$result="true";}
     }
     elsif(uc($specifiedWday) eq "WEEKDAY")
     {
         if($twdayNum > 0 && $twdayNum < 6) {$result="true";}
     }
     else
     {
        if(getWeekdayNum($specifiedWday)==$twdayNum){$result="true";}
     }
     
     #return results
     return $result;
}


##
#function:Returns more weekday info of today
#Date: May 22,2005 
##
sub getTodayWKdayInfo
{
    #initialize the variable
    my $twdayNum= $today[6];
    my $occuranceNum;
    my $occurance;
    my $isOccuranceLast="";
    my %wkdayInfo=();
    
    #Get this weeks Occurance
    $occuranceNum=countWkdays(@today);
    my $daysInMonth=getDaysInAMonth(@today);
    if(($daysInMonth - $occuranceNum) < 7) {$isOccuranceLast="LAST";}
    
    #Word Representation of OccuranceNum
    if($occuranceNum==1) {$occurance="FIRST";}
    if($occuranceNum==2) {$occurance="SECOND";}
    if($occuranceNum==3) {$occurance="THIRD";}
    if($occuranceNum==4) {$occurance="FOURTH";}
    if($occuranceNum==5) {$occurance="FIFTH";}  #Possible
 
    #Populate the Results
    $wkdayInfo{"occuranceNum"}=$occuranceNum;
    $wkdayInfo{"occurance"}=$occurance;
    $wkdayInfo{"isOccuranceLast"}=$isOccuranceLast;
    $wkdayInfo{"wkdayNum"}=$twdayNum;
    $wkdayInfo{"wkday"}=getWeekdayName($twdayNum);
    
    #Return result
    return %wkdayInfo;
}

##
#function:Get A Number Repsentation of Number in Words
#Date: May 22,2005 
##
sub getWordRepresentationOfANumber
{
    #initialize the variable
    my ( $numInWords) = @_;    
    my $num;
    if(uc($numInWords) eq "ZERO" ) {$num=0;}
    elsif(uc($numInWords) eq "FIRST"   || uc($numInWords) eq "ONE")   {$num=1;}
    elsif(uc($numInWords) eq "SECOND"  || uc($numInWords) eq "TWO")   {$num=2;}
    elsif(uc($numInWords) eq "THIRD"   || uc($numInWords) eq "THREE") {$num=3;}
    elsif(uc($numInWords) eq "FOURTH"  || uc($numInWords) eq "FOUR")  {$num=4;}
    elsif(uc($numInWords) eq "FIFTH"   || uc($numInWords) eq "FIVE")  {$num=5;}
    elsif(uc($numInWords) eq "SIXITH"  || uc($numInWords) eq "SIX")   {$num=6;}
    elsif(uc($numInWords) eq "SEVENTH" || uc($numInWords) eq "SEVEN") {$num=7;}
    elsif(uc($numInWords) eq "EIGHTH"  || uc($numInWords) eq "EIGHT") {$num=8;}
    elsif(uc($numInWords) eq "NINETH"  || uc($numInWords) eq "NINE")  {$num=9;}
    elsif(uc($numInWords) eq "TENTH"   || uc($numInWords) eq "TEN")   {$num=10;}
    
    #return result
    return $num;
}


##
#function:Returns a week number with corresponding to a month
#Date: May 21,2005 
##
sub countWkdays
{
    #initialize the variable
     my ( @dt) = @_;
    my $weekNo=1;
    #loop in a reverse order to count weeks by substracting 7 from the current day.
    while("true")
     {
          if($dt[3]<=7)
          {
              goto outer;
           } 
   
           my $val = dateAdd("w",-1,@dt);    
           my @prevWeek = localtime($val);
           $weekNo++;
           @dt=@prevWeek; 
   }
outer: 
return $weekNo;    
}

##
#function: Days in a Month
#Date: May 22,2005
##
sub getDaysInAMonth
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
    
    
##
#function:This function checks, if the year supplied is a leap year
#Date: May 22,2005    
##
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

##
# function: Specific to Rounding of dates
# Date: May 21,2005
##
sub roundDates ($) 
{
    my($number) = @_;
    return int($number); 
}

sub getHostIPAddress
{
    my ($addr) = (gethostbyname($host))[4];    
    my $ipaddress = join(".",unpack("C4", $addr));
	my @lines;
    if($ipaddress eq "127.0.0.1")
    {
        my $interface="eth0";
        my $ifConfig="/sbin/ifconfig";
        eval 
        {
            @lines = qx|$ifConfig $interface|;
        };    
        if ($@)
        {
            mylog("Cant get info from ifconfig: $@\n");    
            return;
        }        
        foreach(@lines)
        {
          if(/inet addr:([\d.]+)/) 
          {
              my $ipaddress=$1;
          }
        }
    }

    return $ipaddress;    
}

## 
# function: disableBandwidthProvisioningProcess
##
sub disableBandwidthProvisioningProcess
{
	mylog("X--------------------X--------------------X");
	mylog("Event: Disable Bandwidth Shaping");
	
	my ($nwdevice , $bpm_disable, $policyName, $data_hash) = @_;	
	my $is_linux = Utilities::isLinux();
	my $unsetNetworkDevice = $nwdevice;
	my $mail_data = "Event: Disable Bandwidth Shaping\n";
	$mail_data .= "Details: Bandwidth policy [".$policyName."] Disabled on ".$host;
	
	my $command;
	my $disable_file_path = $SVSYSTEM_BIN."/".$bpm_rule_file;
	if(-e $disable_file_path)
	{
		my $status = unlink $disable_file_path;
	}
	if($is_linux)
	{
		$command = sprintf("tc qdisc del dev %s root\n",($unsetNetworkDevice));
		open(BPM_CONF, ">> ".$disable_file_path);
		print BPM_CONF $command;
		close(BPM_CONF);
		my $permMode = 0755;   
		chdir($SVSYSTEM_BIN);	
		chmod $permMode, $bpm_rule_file;
		system("/bin/sh", $disable_file_path);
	}
	else
	{
		open(BPM_CONF, ">> ".$disable_file_path);
		print BPM_CONF "Remove-NetQosPolicy -AsJob | Wait-Job \n";
		close(BPM_CONF);
		
		my $powershell_path = `where powershell.exe`;
		system($powershell_path." -ExecutionPolicy ByPass -File ".$disable_file_path);
	}
	
	if($? == 0)
	{
		mylog("Disable policy executed successfully");
	}
	else
	{
		mylog("Failed to execute disable script (".$disable_file_path.") with Error: ".$!);
	}
	
	my @input_parameters;
	my $query_input = unsetAllactivepolicies($nwdevice);
	push(@input_parameters,$query_input);
	
	#Reset the unset device to active device
	$unsetNetworkDevice = $nwdevice;

	mylog("Bandwidth Shapping Disabled");
	mylog("X--------------------X--------------------X");

	# Alert mechanism for bandwidth management.
	my $err_summary = "Bandwidth Shaping";
	my $err_message = $mail_data;
	$err_message =~ s/\n/ /g;
	my $err_id = "DISABLE_BANDWIDTH_".$host;
	
	my $args;
	$args->{"error_id"} = $err_id;
	$args->{"summary"} = $err_summary;
	$args->{"err_temp_id"} = "CXBPM_ALERT";
	$args->{"err_msg"} = $err_message;
	my $alert_info = serialize($args);
	
	&tmanager::add_error_message($alert_info);
	return \@input_parameters;
}

sub unsetAllactivepolicies
{
    my ( $nwdevice ) = @_;
	my $input;	
	$input->{0}->{"queryType"} = "UPDATE";
	$input->{0}->{"tableName"} = "BPMPolicies";
    $input->{0}->{"fieldNames"} = "isActivePolicy = 0" ;
    $input->{0}->{"condition"} = "sourceHostId='$HOST_GUID' AND networkDeviceName = '$nwdevice'";
	
	return $input;
}

sub processBPM
{
	my ($self_obj, $data_hash) = @_;
	$todayTime = time();    
	@today     = localtime($todayTime);
	my (@batch, $input);
	my $i = 0;
	eval
    {
		my $nwdevice;
		my $priority_flag = "false";
		my $applicable = "false";		
		my $bpm_data = $data_hash->{'bpm_policy_data'};
		foreach my $id (sort(keys %{$bpm_data}))
		{
			my $BPMPoliciyId = $bpm_data->{$id}->{"BPMPoliciyId"};
			my $nwdevice = $bpm_data->{$id}->{"networkDeviceName"};
			my $bpm_db_type = $bpm_data->{$id}->{"Type"};
			my $bpm_host_id = $bpm_data->{$id}->{"sourceHostId"};
			if($bpm_db_type eq $type && $bpm_host_id eq $HOST_GUID)
			{
				my $isActivePolicy = $bpm_data->{$id}->{"isActivePolicy"};
				my $isBPMEnabled = $bpm_data->{$id}->{"isBPMEnabled"};
				my $fromTime  = $bpm_data->{$id}->{"ScheduleFromTime"};
				my $toTime    = $bpm_data->{$id}->{"ScheduleTOTime"};
				my $schedule = $bpm_data->{$id}->{"Schedule"};
				my $lastRunDate = $bpm_data->{$id}->{"lastRunDate"};
				my $scheduleType = $bpm_data->{$id}->{"ScheduleType"};				
				
				my $networkDeviceName = $bpm_data->{$id}->{"networkDeviceName"};
				my $share = $bpm_data->{$id}->{"Share"};
				my $cumulativeBandwidth = $bpm_data->{$id}->{"CumulativeBandwidth"};
				my $policyName = $bpm_data->{$id}->{"PolicyName"};
				my $sourceIP = $bpm_data->{$id}->{"SourceIP"};
				
				my $inBetween = isCurrentTimeBetweenScheduleTime($fromTime, $toTime);
				my $isPolicyApplicable = isPolicyApplicable($scheduleType, $schedule, $lastRunDate);
				
				if($isBPMEnabled == 0)
				{
					# Case: Bandwidth policy disabled by the user which is in active state 
					if($isActivePolicy == 1)
					{
						my $input_params = disableBandwidthProvisioningProcess($nwdevice,1,$policyName, $data_hash);
						my @input_batch = @$input_params;
						if(scalar(@input_batch) > 0)
						{
							push(@batch, @input_batch);
						}
						$i++;
						$isActivePolicy = 0;
						last;
					}
				}
				elsif($isBPMEnabled == 1)
				{
					my $applicable = "false";
					if($inBetween eq "true" and $isPolicyApplicable eq "true")
					{
						$applicable = "true";
					}
					
					if($isActivePolicy == 1)
					{
						# Case: Bandwidth policy enabled by the user but policy scheduling options are not in range
						if($applicable eq "false")
						{
							my $input_params = disableBandwidthProvisioningProcess($nwdevice,1, $policyName, $data_hash);
							my @input_batch = @$input_params;
							if(scalar(@input_batch) > 0)
							{
								push(@batch, @input_batch);
							}
							
							my $input = unsetActivePolicy($nwdevice,$BPMPoliciyId);
							push(@batch, $input);
							$isActivePolicy = 0;
						}
					}
					else
					{
						# Case: Bandwidth policy enabled by the user and policy scheduling options are within range
						if(($applicable eq "true") and ($priority_flag eq "false"))
						{
							my $input_params = enableBandwidthProvisioningProcess($nwdevice , $BPMPoliciyId , $networkDeviceName , $share , $cumulativeBandwidth , $policyName , $sourceIP, $data_hash);
							push(@batch, @$input_params);
							$priority_flag = "true";
						}
					}
				}
				else
				{
					# Remove the BPM policy	on vx un-registration which involves in replication, NIC removal etc..				
					my $input_params = disableBandwidthProvisioningProcess($nwdevice,1,$policyName, $data_hash);
					my @input_batch = @$input_params;
					if(scalar(@input_batch) > 0)
					{
						push(@batch, @input_batch);
					}
					sleep(10);
					
					$input->{$i}->{"queryType"} = "DELETE";
					$input->{$i}->{"tableName"} =  "BPMAllocations";
					$input->{$i}->{"fieldNames"} = "";
					$input->{$i}->{"condition"} = "sourceHostId = '$bpm_host_id'";					
					push(@batch, $input);					
					$i++;
					
					$input->{$i}->{"queryType"} = "DELETE";
					$input->{$i}->{"tableName"} =  "BPMPolicies";
					$input->{$i}->{"fieldNames"} = "";
					$input->{$i}->{"condition"} = "sourceHostId = '$bpm_host_id'";
					$i++;
					push(@batch, $input);
					last;					
				}
				if($isActivePolicy == 1)
				{
					$priority_flag = "true";
				}
			}
		}
		my $final_input;
		my $i = 0;
		foreach my $keys (@batch)
		{
			foreach my $key (keys %{$keys})
			{
				$final_input->{$i} = $keys->{$key};				
				$i++;
			}
		}
		if($final_input ne '') {		
			&tmanager::update_cs_db("processBPM",$final_input);		
		}
		
	};
	if ($@)
    {
		mylog("Exception in processBPM".$@);
        return ;
    }
}

sub generate_tc_rules
{
	my ($id , $networkDeviceName , $share , $cumulativeBandwidth , $policyName , $sourceIP, $data_hash) = @_;
	my $result = 1;
	#Initialize local state variables
	my $command;
    if($is_linux)
    {
        my $ARCHNAME = $Config{archname};
        #
        # Check for 64 Bit machine
        #
        if ($ARCHNAME =~ /86_64/)
        {
            my $executeCommand = "/sbin/ethtool -K ".$networkDeviceName." tso off";
            print "Its a 64 bit machine...executing the command: $executeCommand to turn off tcp segmentation offload \n";
            system($executeCommand);
        }
		
		my $getMcTypeCmd = "uname -a | grep -q i[0-9]86";
		my $getMcTypeReturn = system($getMcTypeCmd);
		$command = sprintf("tc qdisc del dev %s root\n",($networkDeviceName));
		if($getMcTypeReturn)
		{
			$command .= "\n/sbin/ethtool -K ".$networkDeviceName." tso off\n\n";
		}
    }
	else
	{
		$command = "Remove-NetQosPolicy -AsJob | Wait-Job \n";
	}
    
    #If, type is WAN, Use current IP Address instead of SourceIP Stored in the database
    #This approach, shall protect against dealing with records, when IP Address changes or 
    #Or IP Address is allocated in dynamic manner for the CX Server
    
    my $unsetNetworkDevice = $networkDeviceName;
    
    #Logging 
	my ($message);
    my $msgPolicy = "Policy: " . $policyName. "[". $id. "]";    
    mylog("X--------------------X--------------------X");
    mylog("Event:Policy Changed");
    mylog($msgPolicy);
    mylog("BWM Invocations:");
    
	my @bwAllocations;
	my $bpm_allocation_data = $data_hash->{"bpm_alloc_data"};
	foreach my $bpm_alloc_id (keys %{$bpm_allocation_data})
	{
		if($id == $bpm_allocation_data->{$bpm_alloc_id}->{"Policyid"})
		{
			push(@bwAllocations, $bpm_allocation_data->{$bpm_alloc_id});
		}
	}
	
	if($BPM_OPTION eq 'CBQ')
	{
		if($is_linux)
		{
			##
			# Add qdisc and class replace for all targets
			##
			$command .= sprintf("tc qdisc add dev %s root handle 1: cbq bandwidth %s avpkt 1272\n",($unsetNetworkDevice,$BPM_LNK_SPEED));
			$command .= sprintf("tc class replace dev %s parent 1:0 classid 1:1 cbq bandwidth %s rate %s prio 1 maxburst 200 avpkt 1272 bounded isolated\n",($unsetNetworkDevice,$BPM_LNK_SPEED,$cumulativeBandwidth."kbit"));
		}
		
		##
		# Add classes for each target
		##
		my $index = 2;
		my $share_str = 'bounded isolated';
		if($share == 1)
		{
			$share_str = 'borrow';
		}
		
		$message = $id. ",". $policyName.",".$share.",".$cumulativeBandwidth;
		foreach my $allocation (@bwAllocations) 
		{
			if($is_linux)
			{
				##
				# Add class for each target
				##
				$command .= sprintf("tc class add dev %s parent 1:1 classid 1:%d cbq bandwidth %s rate %dkbit prio 1 maxburst 200 avpkt 1272 %s\n",($unsetNetworkDevice,$index,$BPM_LNK_SPEED,$allocation->{"AllocatedBandwidth"},$share_str));
			}
			##
			# Add classes filters for each target
			##
			#Get all IP's of the targetHost
			if(exists($allocation->{"VPNServerPort"}) && $allocation->{"VPNServerPort"} != 0)
			{
				my $vpn_port = $allocation->{"VPNServerPort"};
				if($vpn_port) 
				{
					$message = $message.";".$sourceIP.",".$vpn_port.",".$allocation->{"AllocatedBandwidth"};
					if($is_linux)
					{
						$command .= sprintf("tc filter add dev %s protocol ip parent 1:0 prio 1 u32 match ip src %s match ip dport %s 0xffff flowid 1:%d \n",($unsetNetworkDevice,$sourceIP,$vpn_port,$index));
					}
					else
					{
						my $throttle_rate_bits_per_sec = $allocation->{"AllocatedBandwidth"} * 1024;
						$command .= "New-NetQosPolicy -Name BPM_CXPS_POLICY_".$vpn_port." -ApplicationName cxps.exe -IPDstPort ".$vpn_port." -ThrottleRate ".$throttle_rate_bits_per_sec."\n";
					}
				}
			}
			else
			{
				foreach my $ip (split(",", $allocation->{"TargetIP"}))
				{
					if(($ip) && ($ip !~ /:/)) 
					{
						
						$message = $message.";".$sourceIP.",".$ip.",".$allocation->{"AllocatedBandwidth"};
						if($is_linux)
						{
							$command .= sprintf("tc filter add dev %s parent 1:0 protocol ip prio 1 u32 match ip dst %s/32 match ip src %s/32 flowid 1:%d \n",($unsetNetworkDevice,$ip,$sourceIP,$index));
						}
						else
						{						
							my $throttle_rate_bits_per_sec = $allocation->{"AllocatedBandwidth"} * 1024;
							$command .= "New-NetQosPolicy -Name BPM_CXPS_POLICY_".$ip." -ApplicationName cxps.exe -IPDstPrefix ".$ip."/32 -ThrottleRate ".$throttle_rate_bits_per_sec."\n";
						}
					}
				}
			}
			$index++;
		}
	}
	elsif($BPM_OPTION eq 'HTB')
	{
		if($is_linux)
		{
			##
			# Add qdisc and class replace for all targets
			##
			$command .= sprintf("tc qdisc add dev %s root handle 1: htb\n",($unsetNetworkDevice));
			$command .= sprintf("tc class add dev %s parent 1: classid 1:1 htb rate %s ceil %s\n",($unsetNetworkDevice,$cumulativeBandwidth."kbit",$cumulativeBandwidth."kbit"));
		}
		
		##
		# Add classes for each target
		##
		my $index = 11;
				
		$message = $id. ",". $policyName.",".$share.",".$cumulativeBandwidth;
		foreach my $allocation (@bwAllocations) {
					
			##
			# Add class for each target
			##
			if($is_linux)
			{
				if($share == 1)
				{
					$command .= sprintf("tc class add dev %s parent 1:1 classid 1:%d htb rate %s ceil %s \n",($unsetNetworkDevice,$index,$allocation->{"AllocatedBandwidth"}."kbit",$cumulativeBandwidth."kbit"));
				}
				else
				{
					my $ceil = $allocation->{"AllocatedBandwidth"} - (ceil(($allocation->{"AllocatedBandwidth"})/50));
					$command .= sprintf("tc class add dev %s parent 1:1 classid 1:%d htb rate %s ceil %s \n",($unsetNetworkDevice,$index,$allocation->{"AllocatedBandwidth"}."kbit",$ceil."kbit"));
				}
			}
			
			##
			# Add classes filters for each target
			##			
			#Get all IP's of the targetHost
			if(exists($allocation->{"VPNServerPort"}) && $allocation->{"VPNServerPort"} != 0)
			{
				my $vpn_port = $allocation->{"VPNServerPort"};
				if($vpn_port) 
				{
					$message = $message.";".$sourceIP.",".$vpn_port.",".$allocation->{"AllocatedBandwidth"};
					if($is_linux)
					{
						$command .= sprintf("tc filter add dev %s protocol ip parent 1:0 prio 1 u32 match ip src %s match ip dport %s 0xffff flowid 1:%d \n",($unsetNetworkDevice,$sourceIP,$vpn_port,$index));
					}
					else
					{
						my $throttle_rate_bits_per_sec = $allocation->{"AllocatedBandwidth"} * 1024;
						$command .= "New-NetQosPolicy -Name BPM_CXPS_POLICY_".$vpn_port." -ApplicationName cxps.exe -IPDstPort ".$vpn_port." -ThrottleRate ".$throttle_rate_bits_per_sec."\n";
					}
				}
				
			}
			else
			{
				foreach my $ip (split(",", $allocation->{"TargetIP"}))
				{
					if(($ip) && ($ip !~ /:/)) {
						
						$message = $message.";".$sourceIP.",".$ip.",".$allocation->{"AllocatedBandwidth"};
						
						if($is_linux)
						{
							$command .= sprintf("tc filter add dev %s protocol ip parent 1:0 prio 1 u32 match ip dst %s match ip src %s flowid 1:%d \n",($unsetNetworkDevice,$ip,$sourceIP,$index));
						}
						else
						{
							my $throttle_rate_bits_per_sec = $allocation->{"AllocatedBandwidth"} * 1024;
							$command .= "New-NetQosPolicy -Name BPM_CXPS_POLICY_".$ip." -ApplicationName cxps.exe -IPDstPrefix ".$ip."/32 -ThrottleRate ".$throttle_rate_bits_per_sec."\n";
						}
					}
				}
			}
			$index++;
		}
	}
	
	my $enable_file_path = $SVSYSTEM_BIN."/".$bpm_rule_file;
	if(-e $enable_file_path)
	{
		my $status = unlink $enable_file_path;
	}
		
	if($is_linux)
	{
		open(BPM_CONF, ">> ".$enable_file_path);
		print BPM_CONF $command;
		close(BPM_CONF);
		my $permMode = 0755;   
		chdir($SVSYSTEM_BIN);	
		chmod $permMode, $bpm_rule_file;
		system("/bin/sh", $enable_file_path);
	}
	else
	{
		open(BPM_CONF, ">> ".$enable_file_path);
		print BPM_CONF $command;
		close(BPM_CONF);
		
		my $powershell_path = `where powershell.exe`;
		system($powershell_path." -ExecutionPolicy ByPass -File ".$enable_file_path);
	}
	
	if($? == 0)
	{
		my $mail_data = "Event: Enable Bandwidth Shaping\n";
		$mail_data .= "Details: Bandwidth policy [".$policyName."] Enabled on ".$host;
		
		mylog("Policy Summary:");
		mylog($message);
		mylog("Policy Changed");
		mylog("X--------------------X--------------------X");

		my $err_summary = "Bandwidth Shaping";
		my $err_message = $mail_data;
		my $err_id = "ENABLE_BANDWIDTH_".$host;
		$err_message =~ s/\n/ /g;
		
		my $args;
		$args->{"error_id"} = $err_id;
		$args->{"summary"} = $err_summary;
		$args->{"err_temp_id"} = "CXBPM_ALERT";
		$args->{"err_msg"} = $err_message;
		my $alert_info = serialize($args);
		
		my $arguments;
		$arguments->{"err_temp_id"} = "CXBPM_ALERT";
		$arguments->{"err_msg"} = $err_message;
		my $trap_info = serialize($arguments);
		&tmanager::add_error_message($alert_info, $trap_info);
	}
	else
	{
		mylog("Failed to execute the Policy with Error: $!");
		mylog("X--------------------X--------------------X");
		$result = 0;
	}
    return $result;
}

##
# For debugging purposes only. Generates a trace of important events
# in /var/bpmtrace.log. Takes a string argument, no return value.
##
sub mylog ($)
{
    my $message = $_[0];
    eval 
    {
        open(LOG, ">>$BASE_DIR///var/bpmtrace.log");
    };    
    if ($@)
    {
        mylog("Couldn't open trace log: $@\n");    
        return;
    }    

    my $currdatetime = getFormatedDateTime();
    $message = $currdatetime ."  :  ".$message;
    #Write to Underlying Log file
	print LOG $message, "\n";
    #Close the log file
    close(LOG);
}
