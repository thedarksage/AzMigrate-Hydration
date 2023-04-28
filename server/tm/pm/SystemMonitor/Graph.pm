#=================================================================
# FILENAME
#   Graph.pm
#
# DESCRIPTION
#    This perl module is used to graph the rrd data passed to it
#=================================================================
package SystemMonitor::Graph;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
#use lib qw( /home/svsystems/rrd/lib/perl/ /home/svsystems/pm );
use Data::Dumper;
use RRDs;
use Common::Log;

my $logging_obj = new Common::Log();
$logging_obj->set_event_name("SYSTEMMONITOR");

# 
# Function Name: new
# Description: Constructor for the Monitor class.
# Parameters: None 
# Return Value:None
# 	
sub new
{
    my ($class, %args) = @_; 
	my $self = {};
    my $amethyst_vars = Utilities::get_ametheyst_conf_parameters();
	$self->{'rrd'} = $args{'rrd'};
	$self->{'suffix'} = $args{'suffix'};
	$self->{'st_time'} = $args{'st_time'};
	$self->{'end_time'} = $args{'end_time'};
	$self->{'base_dir'} = $amethyst_vars->{"INSTALLATION_DIR"};
	$self->{'web_root'} = $amethyst_vars->{"WEB_ROOT"};
	$self->{'amethyst_vars'} = $amethyst_vars;	
	bless ($self, $class);
}

sub graphRrd($$$$) 
{
	my $self = shift;
	my ($rrd,$suffix,$st_time,$end_time) = ($self->{'rrd'},$self->{'suffix'},$self->{'st_time'},$self->{'end_time'}); 
	#print "rrd::$rrd \n suffix::$suffix \n st_time::$st_time \n end_time::end_time\n";
	my $label_string = "";
	my $v_label = "";
	my $width = "400";
	if($suffix eq "volsync") 
	{
		$v_label = "Num Of Instances";
		$width = "350";
	}
	if($suffix eq "freespace" or $suffix eq "memusage" or $suffix eq "cpuload") 
	{
		$v_label = "percentage";
	}
	if($suffix eq "diskactivity") 
	{
		$v_label = "MB / sec";
	}
	my @name = split(/\./,$rrd);
	my @rrd_split = split(/-/,$name[0]);
	pop(@rrd_split); 
	my $rrd_name = join("-",@rrd_split); 
	$rrd_name = $rrd_name . "-". $suffix;
	my $png_path = $self->{'web_root'}."/trends/";
	my $rrd_path = $self->{'base_dir'}."/SystemMonitorRrds/";
	$rrd = $rrd_path . $rrd;
	my $png_custom = $png_path . $rrd_name . "-custom.png";
	my $png_custom2 = $png_path . $rrd_name . "2-custom.png";
	if($suffix eq "apache") 
	{
		$logging_obj->log("EXCEPTION","$png_custom --start=$st_time --end=$end_time --lower-limit=0 --units-exponent=0 DEF:a2=$rrd:rps:AVERAGE LINE2:a2#FF00FF:RequestPerSec");
		RRDs::graph ($png_custom,"--start=$st_time","--end=$end_time","--lower-limit=0","--units-exponent=0","DEF:a2=$rrd:rps:AVERAGE","LINE2:a2#FF00FF:RequestPerSec");
	}
	elsif( $suffix eq "mysql") 
	{
		RRDs::graph ($png_custom,"--start=$st_time","--end=$end_time","DEF:a1=$rrd:ques:AVERAGE","CDEF:as=a1,PREV(a1),-","LINE2:as#0000FF:DeltaQueries");
		RRDs::graph ($png_custom2,"--start=$st_time","--end=$end_time","DEF:a4=$rrd:thrcon:AVERAGE","LINE2:a4#3399FF:ThreadsConnected","DEF:a5=$rrd:thrun:AVERAGE","LINE2:a5#6600FF:ThreadsRunning");
	}
	elsif($suffix eq "monitor" or $suffix eq "monitor_ps" or $suffix eq "monitor_disks" or $suffix eq "scheduler" or $suffix eq "httpd" or $suffix eq "pushinstalld" or $suffix eq "heartbeat" or $suffix eq "gentrends" or $suffix eq "mysqld" or $suffix eq "bpm" or $suffix eq "cxps" or $suffix eq "wintc" or $suffix eq "ex-service1" or $suffix eq "ex-service2") 
	{
		RRDs::graph($png_custom,"--start=$st_time","--end=$end_time","--units-length=2","--y-grid=none","DEF:ds1=$rrd:$suffix:AVERAGE","CDEF:good=ds1,1,EQ,1,0,IF","CDEF:bad=ds1,1,LT,1,0,IF","CDEF:na=ds1,0,LT,1,0,IF","AREA:good#33CC00:Running","AREA:bad#CC3300:Stopped","AREA:na#ffffff:Unknown","HRULE:0#000000","HRULE:1#000000");
	}
	else 
	{
		RRDs::graph ($png_custom,"--start=$st_time","--end=$end_time","--width=$width","DEF:graph=$rrd:$suffix:AVERAGE","LINE2:graph#FF00FF$label_string","--lower-limit=0","--units-exponent=0","-v $v_label");
	}
	my $ERR=RRDs::error;
	#$logging_obj->log("EXCEPTION","ERROR while creating graph of $rrd_name: $ERR\n"); if $ERR;
	if($ERR)
	{
	$logging_obj->log("EXCEPTION","ERROR while creating graph of $rrd_name: $ERR\n");
	}
}
sub graphRrdToPhp($$$) 
{
	my $self = shift;
	my ($rrd,$suffix,$time_period) = ($self->{'rrd'},$self->{'suffix'},$self->{'st_time'});
	my $label_string = "";
	my $v_label = "";
	my $width = "400";
	if ($suffix eq "apache") 
	{
		$self->graphApache($rrd,$time_period);
		return;
	}
	if ($suffix eq "mysql") 
	{
		$self->graphMySql($rrd,$time_period);
		return;
	}
	if($suffix eq "monitor" or $suffix eq "monitor_ps" or $suffix eq "monitor_disks" or $suffix eq "scheduler" or $suffix eq "httpd" or $suffix eq "pushinstalld" or $suffix eq "heartbeat" or $suffix eq "gentrends" or $suffix eq "mysqld" or $suffix eq "bpm" or $suffix eq "cxps" or $suffix eq "wintc" or $suffix eq "ex-service1" or $suffix eq "ex-service2") 
	{
		$self->graphService($rrd,$suffix,$time_period);
		return;
		#$label_string = ":1 = Running 0 = Stopped";
	}
	if($suffix eq "volsync") 
	{
		$v_label = "Num Of Instances";
		$width = "350";
	}
	if($suffix eq "freespace" or $suffix eq "memusage" or $suffix eq "cpuload") 
	{
		$v_label = "percentage";
	}
	if($suffix eq "diskactivity") 
	{
		$v_label = "MB / sec";
	}
	my @rrds = split(/\./,$rrd);
	my @rrd_split = split(/-/,$rrds[0]);
	pop(@rrd_split); 
	my $rrd_name = join("-",@rrd_split);
	$rrd_name .= "-" . $suffix;
	my $png_path = $self->{'web_root'}."/trends/";
	my $rrd_path = $self->{'base_dir'}."/SystemMonitorRrds/";
	$rrd = $rrd_path . $rrd;
	my $png_name;
	if($time_period eq "last hour") 
	{
		$png_name = $png_path . $rrd_name . ".png";
		print $png_name.",--start=N-3600,--end=N,DEF:graph=".$rrd.":".$suffix.":AVERAGE,LINE2:graph#FF00FF".$label_string.",".$v_label;
		RRDs::graph ($png_name,"--start=N-3600","--end=N","--width=$width","DEF:graph=$rrd:$suffix:AVERAGE","LINE2:graph#FF00FF$label_string","--lower-limit=0","--units-exponent=0","-v $v_label");
		my $ERR=RRDs::error;
		die "ERROR while creating graph of $rrd_name: $ERR\n" if $ERR;
	}
	elsif($time_period eq "last day") 
	{
		$png_name = $png_path . $rrd_name . "-last-day.png";
		RRDs::graph ($png_name,"--start=N-86400","--end=N","--width=$width","DEF:graph=$rrd:$suffix:AVERAGE","LINE2:graph#FF00FF$label_string","--lower-limit=0","--units-exponent=0","-v $v_label");
		my $ERR=RRDs::error;
		die "ERROR while creating graph of $rrd_name: $ERR\n" if $ERR;
	}
	elsif($time_period eq "last week") 
	{
		$png_name = $png_path . $rrd_name . "-last-week.png";
		RRDs::graph ($png_name,"--start=N-604800","--end=N","--width=$width","DEF:graph=$rrd:$suffix:AVERAGE","LINE2:graph#FF00FF$label_string","--lower-limit=0","--units-exponent=0","-v $v_label");
		my $ERR=RRDs::error;
		die "ERROR while creating graph of $rrd_name: $ERR\n" if $ERR;
	}
	elsif($time_period eq "last month") 
	{
		$png_name = $png_path . $rrd_name . "-last-month.png";
		RRDs::graph ($png_name,"--start=N-2678400","--end=N","--width=$width","DEF:graph=$rrd:$suffix:AVERAGE","LINE2:graph#FF00FF$label_string","--lower-limit=0","--units-exponent=0","-v $v_label");
		my $ERR=RRDs::error;
		die "ERROR while creating graph of $rrd_name: $ERR\n" if $ERR;
	}
}

sub graphApache($) 
{ 
	my $self = shift;
	my($rrd,$time_period) = ($self->{'rrd'},$self->{'st_time'});
	my $suffix = "apache";
	my @rrds = split(/\./,$rrd);
	my @rrd_split = split(/-/,$rrds[0]);
	pop(@rrd_split); 
	my $rrd_name = join("-",@rrd_split);
	$rrd_name .= "-" . $suffix;
	my $png_path = $self->{'web_root'}."/trends/";
	my $rrd_path = $self->{'base_dir'}."/SystemMonitorRrds/";
	$rrd = $rrd_path . $rrd;
	my $png_name;
	if($time_period eq "last hour") 
	{
		$png_name = $png_path . $rrd_name . ".png";
		RRDs::graph ($png_name,"--start=N-3600","--end=N","DEF:a2=$rrd:rps:AVERAGE","LINE2:a2#FF00FF:RequestPerSec","--lower-limit=0","--units-exponent=0");
		my $ERR=RRDs::error;
		die "ERROR while creating graph of $rrd_name: $ERR\n" if $ERR;
	}
	elsif($time_period eq "last day") 
	{
		$png_name = $png_path . $rrd_name . "-last-day.png";
		RRDs::graph ($png_name,"--start=N-86400","--end=N","DEF:a2=$rrd:rps:AVERAGE","LINE2:a2#FF00FF:RequestPerSec","--lower-limit=0","--units-exponent=0");
		my $ERR=RRDs::error;
		die "ERROR while creating graph of $rrd_name: $ERR\n" if $ERR;
	}
	elsif($time_period eq "last week") 
	{
		$png_name = $png_path . $rrd_name . "-last-week.png";
		RRDs::graph ($png_name,"--start=N-604800","--end=N","DEF:a2=$rrd:rps:AVERAGE","LINE2:a2#FF00FF:RequestPerSec","--lower-limit=0","--units-exponent=0");
		my $ERR=RRDs::error;
		die "ERROR while creating graph of $rrd_name: $ERR\n" if $ERR;
	}
	elsif($time_period eq "last month") 
	{
		$png_name = $png_path . $rrd_name . "-last-month.png";
		RRDs::graph ($png_name,"--start=N-2678400","--end=N","DEF:a2=$rrd:rps:AVERAGE","LINE2:a2#FF00FF:RequestPerSec","--lower-limit=0","--units-exponent=0");
		my $ERR=RRDs::error;
		die "ERROR while creating graph of $rrd_name: $ERR\n" if $ERR;
	}
}

sub graphMySql($) 
{
	my $self = shift;
	my($rrd,$time_period) = ($self->{'rrd'},$self->{'st_time'});
	my $suffix = "mysql";
	my @rrds = split(/\./,$rrd);
	my @rrd_split = split(/-/,$rrds[0]);
	pop(@rrd_split); 
	my $rrd_name = join("-",@rrd_split);
	$rrd_name .= "-" . $suffix;
	my $png_path = $self->{'web_root'}."/trends/";
	my $rrd_path = $self->{'base_dir'}."/SystemMonitorRrds/";
	$rrd = $rrd_path . $rrd;
	my ($png_name,$png_name2);
	if($time_period eq "last hour") 
	{
		$png_name = $png_path . $rrd_name . ".png";
		$png_name2 = $png_path . $rrd_name . "2.png";
		RRDs::graph ($png_name,"--start=N-3600","--end=N","DEF:a1=$rrd:ques:AVERAGE","CDEF:as=a1,PREV(a1),-","LINE2:as#0000FF:DeltaQueries");
		RRDs::graph ($png_name2,"--start=N-3600","--end=N","DEF:a4=$rrd:thrcon:AVERAGE","LINE2:a4#3399FF:ThreadsConnected","DEF:a5=$rrd:thrun:AVERAGE","LINE2:a5#6600FF:ThreadsRunning");
		my $ERR=RRDs::error;
		die "ERROR while creating graph of $rrd_name: $ERR\n" if $ERR;
	}
	elsif($time_period eq "last day") 
	{
		$png_name = $png_path . $rrd_name . "-last-day.png";
		$png_name2 = $png_path . $rrd_name . "2-last-day.png";
		RRDs::graph ($png_name,"--start=N-86400","--end=N","DEF:a1=$rrd:ques:AVERAGE","CDEF:as=a1,PREV(a1),-","LINE2:as#0000FF:DeltaQueries");
		RRDs::graph ($png_name2,"--start=N-86400","--end=N","DEF:a4=$rrd:thrcon:AVERAGE","LINE2:a4#3399FF:ThreadsConnected","DEF:a5=$rrd:thrun:AVERAGE","LINE2:a5#6600FF:ThreadsRunning");
		my $ERR=RRDs::error;
		die "ERROR while creating graph of $rrd_name: $ERR\n" if $ERR;
	}
	elsif($time_period eq "last week") 
	{
		$png_name = $png_path . $rrd_name . "-last-week.png";
		$png_name2 = $png_path . $rrd_name . "2-last-week.png";
		RRDs::graph ($png_name,"--start=N-604800","--end=N","DEF:a1=$rrd:ques:AVERAGE","CDEF:as=a1,PREV(a1),-","LINE2:as#0000FF:DeltaQueries");
		RRDs::graph ($png_name2,"--start=N-604800","--end=N","DEF:a4=$rrd:thrcon:AVERAGE","LINE2:a4#3399FF:ThreadsConnected","DEF:a5=$rrd:thrun:AVERAGE","LINE2:a5#6600FF:ThreadsRunning");
		my $ERR=RRDs::error;
		die "ERROR while creating graph of $rrd_name: $ERR\n" if $ERR;
	}
	elsif($time_period eq "last month") 
	{
		$png_name = $png_path . $rrd_name . "-last-month.png";
		$png_name2 = $png_path . $rrd_name . "2-last-month.png";
		RRDs::graph ($png_name,"--start=N-2678400","--end=N","DEF:a1=$rrd:ques:AVERAGE","CDEF:as=a1,PREV(a1),-","LINE2:as#0000FF:DeltaQueries");
		RRDs::graph ($png_name2,"--start=N-2678400","--end=N","DEF:a4=$rrd:thrcon:AVERAGE","LINE2:a4#3399FF:ThreadsConnected","DEF:a5=$rrd:thrun:AVERAGE","LINE2:a5#6600FF:ThreadsRunning");
		my $ERR=RRDs::error;
		die "ERROR while creating graph of $rrd_name: $ERR\n" if $ERR;
	}
}

sub graphService($) 
{
	my $self = shift;
	my($rrd,$suffix,$time_period) = ($self->{'rrd'},$self->{'suffix'},$self->{'st_time'});
	my @rrds = split(/\./,$rrd);
	my @rrd_split = split(/-/,$rrds[0]);
	pop(@rrd_split);
	my $rrd_name = join("-",@rrd_split);
	$rrd_name .= "-" . $suffix;
	my $png_path = $self->{'web_root'}."/trends/";
	my $rrd_path = $self->{'base_dir'}."/SystemMonitorRrds/";
	$rrd = $rrd_path . $rrd;
	my $png_name;
	if($time_period eq "last hour") 
	{
		$png_name = $png_path . $rrd_name . ".png";
		RRDs::graph($png_name,"--start=N-3600","--end=N","--units-length=2","--y-grid=none","DEF:ds1=$rrd:$suffix:AVERAGE","CDEF:good=ds1,1,EQ,1,0,IF","CDEF:bad=ds1,1,LT,1,0,IF","CDEF:na=ds1,0,LT,1,0,IF","AREA:good#33CC00:Running","AREA:bad#CC3300:Stopped","AREA:na#ffffff:Unknown","HRULE:0#000000","HRULE:1#000000");
		my $ERR=RRDs::error;
		die "ERROR while creating graph of $rrd_name: $ERR\n" if $ERR;
	}
	elsif($time_period eq "last day") 
	{
		$png_name = $png_path . $rrd_name . "-last-day.png";
		RRDs::graph($png_name,"--start=N-86400","--end=N","--units-length=2","--y-grid=none","DEF:ds1=$rrd:$suffix:AVERAGE","CDEF:good=ds1,1,EQ,1,0,IF","CDEF:bad=ds1,1,LT,1,0,IF","CDEF:na=ds1,0,LT,1,0,IF","AREA:good#33CC00:Running","AREA:bad#CC3300:Stopped","AREA:na#ffffff:Unknown","HRULE:0#000000","HRULE:1#000000");
		my $ERR=RRDs::error;
		die "ERROR while creating graph of $rrd_name: $ERR\n" if $ERR;
	}
	elsif($time_period eq "last week") 
	{
		$png_name = $png_path . $rrd_name . "-last-week.png";
		RRDs::graph($png_name,"--start=N-604800","--end=N","--units-length=2","--y-grid=none","DEF:ds1=$rrd:$suffix:AVERAGE","CDEF:good=ds1,1,EQ,1,0,IF","CDEF:bad=ds1,1,LT,1,0,IF","CDEF:na=ds1,0,LT,1,0,IF","AREA:good#33CC00:Running","AREA:bad#CC3300:Stopped","AREA:na#ffffff:Unknown","HRULE:0#000000","HRULE:1#000000");
		my $ERR=RRDs::error;
		die "ERROR while creating graph of $rrd_name: $ERR\n" if $ERR;
	}
	elsif($time_period eq "last month") 
	{
		$png_name = $png_path . $rrd_name . "-last-month.png";
		RRDs::graph($png_name,"--start=N-2678400","--end=N","--units-length=2","--y-grid=none","DEF:ds1=$rrd:$suffix:AVERAGE","CDEF:good=ds1,1,EQ,1,0,IF","CDEF:bad=ds1,1,LT,1,0,IF","CDEF:na=ds1,0,LT,1,0,IF","AREA:good#33CC00:Running","AREA:bad#CC3300:Stopped","AREA:na#ffffff:Unknown","HRULE:0#000000","HRULE:1#000000");
		my $ERR=RRDs::error;
		die "ERROR while creating graph of $rrd_name: $ERR\n" if $ERR;
	}
}

1;
