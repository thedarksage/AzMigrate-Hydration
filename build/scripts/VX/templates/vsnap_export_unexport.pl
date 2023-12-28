#!/usr/bin/perl
use strict;

sub log
{
    my($message) = @_;
    print "vsnap_export_unexport :".$message;
}
sub export_vsnaps()
{
    my $vsnap_cmd = `/usr/local/InMage/Vx/bin/cdpcli --vsnap --op=remount`;
    &log('Issued the command "cdpcli --vsnap --op=remount" and OP is '.$vsnap_cmd.'\n');
    &log("Exporting Vsnaps!\n");
    &handle_vsnaps("1");
}

sub unexport_vsnaps()
{
    my $vsnap_cmd = "for devfiles in `ls /proc/scsi_tgt/groups/*/devices `; do grep -l inmvsnap \$devfiles;cat \$devfiles |grep inmvsnap; done >/usr/local/.vsnaps";
    `$vsnap_cmd`;
    &log("Un Exporting Vsnaps!\n");
    &handle_vsnaps("0");
}

sub handle_vsnaps
{
    my ($export) = @_;
    my $op = "add";
    &log("export is $export\n");
    if ($export == 0 or $export eq "0")
    {
        $op = "del";
    }
    my $vsnap_file = "/usr/local/.vsnaps";

    # Read mount file
    # File opernations for mount file
    unless ( open(FH , "<$vsnap_file") )
    {
        &log("Cannot open $vsnap_file for reading!\n");
        return;
    }

    local $/ = undef;
    my $vsnaps = <FH>;
    close(FH);
    # Constructing array out of vsnap file
    my @contents = split("\n",$vsnaps);

    my $devices_file;

    foreach my $line (@contents)
    {
        if($line !~ /scsi_tgt/)
        {
            &log ("$op Vsnap $line to $devices_file\n");
            my $cmd="echo \"$op $line\" >$devices_file";
            my $cmd_ret=`$cmd`;
            my $cmd_status = `echo $?`;
            &log("$cmd returned $cmd_ret and return status is $cmd_status\n");	
        }
        else
        {
            {
                $devices_file = $line;
                print "\nfile $line\n";
            }
        }
    }
}


sub main()
{
    my $arg=$ARGV[0];
    if ($arg eq "unexport")
    {
        &unexport_vsnaps();
        &log("Unexport...\n");
    }
    elsif ($arg eq "export")
    {
        &export_vsnaps();
        &log("Export...\n");
    }
    else
    {
        &log("Please supply valid option to vsnap_export_unexport.pl.\n");
        &log("Usage: perl vsnap_export_unexport.pl <export/unexport>.\n");
    }
}
&main;
