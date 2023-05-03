#================================================================= 
# FILENAME
#   ParseLog.pm 
#
# DESCRIPTION
#    This perl module is responsible for parsing the FTP 
#    log file on the process server. It returns a hash of
#    with the list of host id's and the incoming and outgoing
#    ip parsed from the xferlog with the data change rates
#    against each entry in the log file.
#
# HISTORY
#     12 Dec 2008  hpatel    Created.
#
#=================================================================
#                 Copyright (c) InMage Systems                    
#=================================================================
package ProcessServer::FTP::ParseLog;
use strict;
use File::Copy;
use File::Compare;
use Data::Dumper;

#
# Create a new object of the class and instantiate
# it with required arguments.
#
sub new 
{
	my ($class, %args) = @_;
	
	my $self = {}; 
      
	$self->{'diff'}      		= $args{'diff'};
	$self->{'log_file'}        	= $args{'log_file'};
	$self->{'log_file_old'}    	= $args{'log_file_old'};
	$self->{'log_file_diff'}   	= $args{'log_file_diff'};
   $self->{'log_dir'} 		= $args{'log_dir'};
   $self->{'cxps_xfer'} 		= $args{'cxps_xfer'};
        
	bless ($self, $class);
}

#
# Parse the xferlog file
#
sub parse_xfer_diff
{
	my($xfer_logs) = @_;
	my %xfer_logs = %$xfer_logs;
	my $diff_file = '';
	my $log_file = $xfer_logs->{'log_file'};
	
	$log_file = Utilities::makePath($log_file);
	unless (-e $log_file)
	{
		return;
	}
	
	my $log_file_old = $xfer_logs->{'log_file_old'};
	$log_file_old = Utilities::makePath($log_file_old);
	
	if (-e $log_file_old)
	{	
		my $old_filesize = -s "$log_file_old";
		my $new_filesize = -s "$log_file";
		
		if($old_filesize > $new_filesize)
		{
			$diff_file = $log_file;
		}
		else
		{	
			&get_diff($log_file,$log_file_old,$xfer_logs->{'log_file_diff'});
		
			my $log_file_diff = $xfer_logs->{'log_file_diff'};
			$log_file_diff = Utilities::makePath($log_file_diff);
			
			$diff_file = $log_file_diff;
		}
	}
	else
	{
		$diff_file = $log_file;
	}
	
	my %file_hash;

	if ($xfer_logs->{'cxps_xfer'}) {
		%file_hash = &parse_cxps_xferlog($diff_file);
	} 
	else {
		%file_hash = &parse_xferlog($diff_file);
	}
   copy($log_file,$log_file_old);
	
	return %file_hash;
}

#
# Parse the xferlog file and retrive all statistics of 
# a host from the log file.
#
sub parse_xferlog
{
	my ($filename) = @_;
	
	my @lines = &read_file($filename);
	my %file_hash = ();
	my $incoming_count = 1;
	my $outgoing_count = 1;

	foreach my $line(@lines)
	{
		chomp $line;
		my @arr = split(/\s+/,$line);
		if ($arr[11] eq "i")
		{
			my $filepath = $arr[8];
			my @files = split(/\//,$filepath);
			my $myhostid = $files[3];
			

	        push (@{$file_hash{$myhostid}{'incoming'}},
					 {
						'transfer_time' => $arr[5],
						'remote_host'   => $arr[6],
						'file_name'     => $arr[8],
						'file_size'     => $arr[7],
						'action_flag'   => $arr[10],
						'access_mode'   => $arr[12],
						'username'      => $arr[13],
						'service'       => $arr[14],
						'auth_method'   => $arr[15],
						'auth_userid'   => $arr[16],
						'status'        => $arr[17]
					 }
					);
		}
		elsif ($arr[11] eq "o")
		{
			my $filepath = $arr[8];
			my @files = split(/\//,$filepath);
			my $myhostid = $files[3];
	                push (@{$file_hash{$myhostid}{'outgoing'}},
					{  
                                                'transfer_time' => $arr[5],
                                                'remote_host'   => $arr[6],
                                                'file_name'     => $arr[8],
                                                'file_size'     => $arr[7],
                                                'action_flag'   => $arr[10],
                                                'access_mode'   => $arr[12],
                                                'username'      => $arr[13],
                                                'service'       => $arr[14],
                                                'auth_method'   => $arr[15],
                                                'auth_userid'   => $arr[16],
                                                'status'        => $arr[17]
			                 }
					);
		}
	}
    
	return (%file_hash);
}

#
# parses cxps xfer log file
#
sub parse_cxps_xferlog
{
	my ($filename) = @_;

    my @lines = &read_file($filename);
    my %file_hash = ();

    foreach my $line (@lines) {
        my $direction = "";
        chomp $line;
        my @arr = split(/\s+/,$line);

        if ($arr[2] eq "/putfile") {
            $direction = "incomming";
        } elsif ($arr[2] eq "/getfile") {
            $direction = "outgoing";
        } else {
            # non data xfer entry skip it
            next;
        }

        if ($direction) {
            push (@{$file_hash{$arr[3]}{$direction}},
                  {
                      'transfer_time' => $arr[7],
                      'remote_host'   => $arr[4],
                      'file_name'     => $arr[5],
                      'file_size'     => $arr[6],
                      'action_flag'   => "_",
                      'access_mode'   => "na",
                      'username'      => "na",
                      'service'       => "na",
                      'auth_method'   => "na",
                      'auth_userid'   => "na",
                      'status'        => ($arr[9] eq "success" ? "c" : "i")
                     }
                 );
        }
    }

    return (%file_hash);
}

#
# Read xferlog and return a list 
#
sub read_file 
{
    my $filename = shift;
	my @lines;
	if(-e $filename)
	{
		open (FILEH, "< $filename") || die "Error reading $filename: $!\n";
		@lines = <FILEH>;
		close (FILEH);
    }
    return @lines;
}

#
# Clean the xferlog diff after the entries in it
# have been processed.
#
sub clean_xferlog_diff
{
	my ($filename) = @_;
		
	my @lines = &read_file($filename);
	my @newarr = ();
	foreach my $line(@lines)
	{
		chomp $line;
		if ($line =~ /^([0-9]+.*)/)
		{
			$line =~ s/$1//g;
		}
		elsif ($line =~ /^(\<\s+)(.*?)$/)
		{
			$line =~ s/$1//g;
		}
		push @newarr, $line if (($line) && ($line ne ""));
	}
	
	open (FILEH, "> $filename") || die "Error writing to $filename: $!\n";
	foreach my $line(@newarr)
	{
		print FILEH "$line\n";
	}
	close FILEH;
}

sub get_diff()
{
	my($file,$bak_file,$diff_file) = @_;
	
	if(-e $diff_file)
	{
		unlink($diff_file);
	}
	
	if (compare($file,$bak_file))
	{
		my $line_count = 0;
		my $tmp_cnt = 0;
		open(FH,$bak_file) || "Error while opening file $bak_file. $! \n";
		my @lines = <FH>;
		my $line_count =  @lines;
		close FH;

		open(FH,$file) || "Error while opening file $file. $! \n";
		open(DIFF_FH, ">>".$diff_file);
		while (my $line = <FH>)
		{
			$tmp_cnt++;
			if($tmp_cnt > $line_count)
			{
				print DIFF_FH $line ;
			}
		}
		close DIFF_FH;
		close FH;
	}
}

1;
