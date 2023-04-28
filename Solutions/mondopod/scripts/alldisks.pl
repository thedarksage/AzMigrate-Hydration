#!/usr/bin/perl
use Time::HiRes qw( usleep ualarm gettimeofday tv_interval nanosleep clock_gettime clock_getres c      lock_nanosleep clock stat );

@disklist = `iostat -ien`;
shift @disklist;
shift @disklist;

$skipwrites = "yes";
$capacityinkb = 2 * 1000 * 1000 * 1000;
$blocksize = 128;
$blocksizek = "128k";
$count= 8192;
$count= 256;
$maxregions =2;
$skipchunk = int ($capacityinkb/($maxregions * $blocksize));
@results;
#print "$skipchunk \n";
open(FC, ">", "formatcommands") ;
print FC "inquiry \n" ;
print FC "cache \n";
print FC "write \n";
print FC "display\n";
print FC "q\n";
print FC "read\n";
print FC "display\n";
print FC "q\n";
print FC "q\n";
print FC "q\n";
close FC;

foreach my $line (@disklist) {
    my @column = split(/\s+/,$line);
    chomp (@column);
    $disk = $column[5];
# assumes 2tb disk for now
    $chunkin128kblocks = $skipchunk;
    $region = 0;
    $skipblocks=0;
    $diskinfo = `format -e -d $disk -f formatcommands`;
    print "Benching disk, $diskinfo  \n";
    while ($region <$maxregions) {
        $timestart = gettimeofday();
        $out = `dd if=/dev/rdsk/$disk of=/dev/null iseek=$skipblocks bs=$blocksizek count=$count 2>/dev/      null`;
        $timeend = gettimeofday();
        $readkbpersec = ($count * $blocksize) /($timeend - $timestart);
        $readmbpersec = $readkbpersec/1024;
        print "$disk, region $region, read  1gb at $readmbpersec  MB/s\n";

        $timestart = gettimeofday();
        if ($skipwrites eq "no" ) {
                $out = `dd of=/dev/rdsk/$disk if=/dev/zero oseek=$skipblocks bs=$blocksizek count=$count       2>/dev/null`;
                $timeend = gettimeofday();
                $writekbpersec = ($count * $blocksize) /($timeend - $timestart);
                $writembpersec = $writekbpersec/1024;
                print "$disk, region $region, wrote 1gb at $writembpersec MB/s \n";
        }

        push (@results, "$disk, $region, $readkbpersec, $writekbpersec \n");

        $skipblocks += $chunkin128kblocks;
        $region++;
    }
}

