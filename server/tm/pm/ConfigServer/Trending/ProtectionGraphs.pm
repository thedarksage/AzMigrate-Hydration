#!/usr/bin/perl
#$Header: /src/server/tm/pm/ConfigServer/Trending/ProtectionGraphs.pm,v 1.2 2015/11/25 13:44:08 prgoyal Exp $
#================================================================= 
# FILENAME
#   ProtectionGraphs.pm 
#
# DESCRIPTION
#    This perl module is responsible for generating protection 
#    report graphs and a HTML report for a configured time 
#    interval derived from amethyst.conf 
#
# HISTORY
#     23 Jul 2008  chpadh    Created.
#
#=================================================================
#                 Copyright (c) InMage Systems                    
#=================================================================
package ConfigServer::Trending::ProtectionGraphs;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use File::Basename;
use DBI();
use Common::Log;
my $logging_obj = new Common::Log();

sub new 
{
      my ($class, %args) = @_; 
      my $self = {};
	  
	  $self->{'base_dir'} = $args{'base_dir'};
	  $self->{'web_root'} = $args{'web_root'};
	  $self->{'installation_dir'} = $args{'installation_dir'};
      $self->{'trends_dir'} = $args{'trends_dir'};
      $self->{'rrd_command'} = $args{'rrd_command'};
	  $self->{'device_names'} = $args{'device_names'};
	  $self->{'report_generator'} = $args{'report_generator'};
	  $self->{'rrd'} = "";
	  $self->{'hostname'} = "";
	  $self->{'hostid'} = "";
	  $self->{'device'} = "";

      bless ($self, $class);
}


1;