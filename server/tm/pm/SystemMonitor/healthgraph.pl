#!/usr/bin/perl
#$Header: /src/server/tm/systemmonitor.pl
#=================================================================
# FILENAME
#    systemmonitor.pl
#
# DESCRIPTION
#    This file create an event for systemmonitor thread to run every 60 seconds.
#
# HISTORY
#     <23/04/2008>  Srinivas K    Created.
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================


use strict;
my $systemDrive = $ENV{'SystemDrive'};
use lib($systemDrive."\\ProgramData\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use SystemMonitor::Monitor;
use SystemMonitor::Graph;
use SystemMonitor::Update;
use Data::Dumper;
use Common::Log;
my $mon_obj = new SystemMonitor::Monitor();
my %args = (
				'rrd' => $ARGV[1],
				'suffix' => $ARGV[2],
				'st_time' => $ARGV[3],
				'end_time' => $ARGV[4]
			);
my $graph_obj = new SystemMonitor::Graph(%args);
my $update_obj = new SystemMonitor::Update();

my $log_obj = new Common::Log();
$log_obj->set_event_name("SYSTEMMONITOR");

my $argc = $#ARGV + 1;
if($argc >= 1) {
  if($ARGV[0] eq "GRAPH") { 
	$graph_obj->graphRrd();
  }
  elsif($ARGV[0] eq "GRAPHPHP") {
	$graph_obj->graphRrdToPhp();
  }
  elsif($ARGV[0] eq "ADDPS") {
    $mon_obj->maintainPsIps($ARGV[1]);
  }
  elsif($ARGV[0] eq "PERFALL") {
    my $perf_params = $mon_obj->getPerfAll();
    print "$perf_params";
  }
  elsif($ARGV[0] eq "ALL") {
	my $per_params = $mon_obj->getAll();
    print "$per_params";
  }
  elsif($ARGV[0] eq "SERVALL") {
    my $serv_params = $mon_obj->getServiceStatus();
	print "$serv_params";
  }
  elsif($ARGV[0] eq "PERFPS") {
    my $ps_params = $mon_obj->getPerfPs();
        print "$ps_params";
  }
  elsif($ARGV[0] eq "CS80CMPCPU") {
    my $cpu_info = $mon_obj->getCpuInfo();
	print "$cpu_info";
  }
  elsif($ARGV[0] eq "CS80CMPMEM") {
    my $memory = $mon_obj->getTotalMem();
    print "$memory";
  }
}
