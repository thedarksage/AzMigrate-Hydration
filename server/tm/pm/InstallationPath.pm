#!/usr/bin/perl

package InstallationPath;
use strict;

sub getInstallationPath()
{
    my $csInstallDrivePath;
    my $csInstallPath = '';    
    $csInstallDrivePath = join('\\', ($ENV{'ProgramData'}, 'Microsoft Azure Site Recovery', "Config", "App.conf"));
    my $appconf_contents;
    if (-e $csInstallDrivePath) 
    {		
        open (FILEH, "<$csInstallDrivePath") || die "Unable to locate $csInstallDrivePath:$!\n";
        my @file_contents = <FILEH>;
        close (FILEH);
        
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
                if($arr[0])
                {
                    $appconf_contents->{$arr[0]} = $arr[1];
                }
            }
        }

        if($appconf_contents->{'INSTALLATION_PATH'}) 
        {
            $csInstallPath = $appconf_contents->{'INSTALLATION_PATH'};
        }
    }

    return $csInstallPath;
}

1;