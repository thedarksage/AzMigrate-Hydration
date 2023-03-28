#!$ORACLE_HOME/perl/bin/perl -w
#=================================================================
# FILENAME
#    oracleappdiscovery.pl
#
# DESCRIPTION
#     Connect and get the volume information from an Oracle db.
#
# HISTORY
#     <28/06/2010>  Vishnu Created.
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================

use DBI();
use Socket;
use strict;
use Data::Dumper;
use LWP::UserAgent;
use File::Basename;

my $db;
my $outputFile;
my $ORACLE_SID;
my $ORACLE_HOME;
my $SRC_CONF_FILE;
my $isAsm=0;
my %diskGrp;
my $dbKeyCnt=0;
my %databases;
my $asmDbKeyCnt=0;
my %asmDatabases;
my $isCluster;
my $numDbNodes=0;
my $spfile;
my $recoveryLogEnabled;
my @uniqueDirs;
my $isASMLib=0;

sub Usage
{
    print "\n";
    print "Usage: \n";
    print "\n";
    print "$0 <ORACLE_SID> <ORACLE_HOME> <OUTPUT_FILE> <OUTPUT_CONFIG_FILE> \n";
    print "Note: set ORACLE_SID, ORACLE_HOME, PATH from the shell calling $0\n";
    print "\n";
}

sub ParseArgs
{
    my $numArgs = $#ARGV + 1;
    #print "Num of args: $numArgs\n";

    if ($numArgs ne 4)
    {
        &Usage;
        exit 1;
    }

    #foreach my $argnum (0 .. $#ARGV) {
    #    print "arg[$argnum]: $ARGV[$argnum]\n";
    #}

    $ORACLE_SID=$ARGV[0];
    $ORACLE_HOME=$ARGV[1];
    $outputFile=$ARGV[2];
    $SRC_CONF_FILE=$ARGV[3];

    # verify the shell variables are set
    my $oSid=`echo \$ORACLE_SID`;
    my $oHome=`echo \$ORACLE_HOME`;
    chomp $oSid;
    chomp $oHome;

    if (($oSid eq '') || ($oHome eq ''))
    {
        &Usage;
        exit 1;
    }

    # check if the SID for ASM
    if ( $ORACLE_SID =~ /\+ASM/ )
    {
        $isAsm=1;
        #print "Connecting to ASM db\n";
    }
}

sub GetDbHandle
{

    #print "Connecting to oracle sys database\n";

    my $connect_mode = 2;   # 2=SYSDBA
    $db = DBI->connect_cached('dbi:Oracle:', '', '',
        {ora_session_mode => $connect_mode, PrintError => 0 } ) or die $DBI::errstr;

    #print "Connected to oracle sys database\n";

    return;
}

sub GetAsmDisks
{
    my $sqlDiskInfo = "SELECT NAME, PATH FROM V\$ASM_DISK";

    my $sqlStmntDisk = $db->prepare($sqlDiskInfo) or die $DBI::errstr;
    $sqlStmntDisk->execute() or die $db->errstr;

    open (FILE, "> $outputFile") || die "Unable to open output file $outputFile:$!\n";

    print FILE "UsingAsmDg=TRUE\n";
    my $diskKeyCount=0;
    while (my $refStmntDisk = $sqlStmntDisk->fetchrow_hashref())
    {
        my $diskNumber = $refStmntDisk->{DISK_NUMBER};
        my $diskName = $refStmntDisk->{NAME};
        my $diskPath = $refStmntDisk->{PATH};
	    if("ORCL:" eq substr($diskPath, 0, 5))
	    {
		    my $asmlibDisk = `echo $diskPath | cut -d: -f2`;
		    chomp $asmlibDisk;
		    $diskPath = "/dev/oracleasm/disks/".$asmlibDisk;
	    }
        #my %diskDetails = ( diskName => $diskName, diskPath => $diskPath);
        #$disks{$diskKeyCount} = \%diskDetails;
        ++$diskKeyCount;
        print FILE "$diskPath";
	print FILE "\n";
    }
    $sqlStmntDisk->finish();

    close FILE;
}

sub GetAsmDetails
{
    my $sqlDbInfo = "SELECT GROUP_NUMBER, DB_NAME, SOFTWARE_VERSION, INSTANCE_NAME FROM V\$ASM_CLIENT";

    my $sqlStmntDb = $db->prepare($sqlDbInfo) or die $DBI::errstr;
    $sqlStmntDb->execute() or die $db->errstr;

    $asmDbKeyCnt=0;
    while (my $refStmntDb = $sqlStmntDb->fetchrow_hashref())
    {
        my $dbName = $refStmntDb->{DB_NAME};
        my $dbInstanceName = $refStmntDb->{INSTANCE_NAME};
        my $dbVersion = $refStmntDb->{SOFTWARE_VERSION};
        my $dgNumber = $refStmntDb->{GROUP_NUMBER};

        my $sqlGrpInfo = "SELECT NAME, STATE FROM V\$ASM_DISKGROUP WHERE GROUP_NUMBER = $dgNumber";

        my $sqlStmntGrp = $db->prepare($sqlGrpInfo) or die $DBI::errstr;
        $sqlStmntGrp->execute() or die $db->errstr;

        my $dgKeyCount=0;
        my %diskGroups;

        while (my $refStmntGrp = $sqlStmntGrp->fetchrow_hashref())
        {
            my $groupName = $refStmntGrp->{NAME};
            my $groupState = $refStmntGrp->{STATE};

            my %disks;
            my $diskKeyCount=0;

            my $sqlDiskInfo = "SELECT DISK_NUMBER, NAME, PATH FROM V\$ASM_DISK WHERE GROUP_NUMBER = $dgNumber";

            my $sqlStmntDisk = $db->prepare($sqlDiskInfo) or die $DBI::errstr;
            $sqlStmntDisk->execute() or die $db->errstr;

            while (my $refStmntDisk = $sqlStmntDisk->fetchrow_hashref())
            {
                my $diskNumber = $refStmntDisk->{DISK_NUMBER};
                my $diskName = $refStmntDisk->{NAME};
                my $diskPath = $refStmntDisk->{PATH};	
                my %diskDetails = ( diskName => $diskName, diskNumber => $diskNumber, diskPath => $diskPath);
                $disks{$diskKeyCount} = \%diskDetails;
                ++$diskKeyCount;
            }

            $sqlStmntDisk->finish();

            my %dgDetails = ( dgName => $groupName, dgNumber => $dgNumber, dgState => $groupState, disks => \%disks);
            $diskGroups{$dgKeyCount} = \%dgDetails;
            ++$dgKeyCount;
        }

        $sqlStmntGrp->finish();
        my %databaseDetails = ( dbName => $dbName, dbInstanceName => $dbInstanceName, dbVersion => $dbVersion, asmDiskGroups => \%diskGroups );
        $asmDatabases{$asmDbKeyCnt} = \%databaseDetails;
        ++$asmDbKeyCnt;
    }

    $sqlStmntDb->finish();

    #print Dumper(\%asmDatabases);
}

sub GetDbDetails
{

    my $sqlDbInfo = "SELECT INSTANCE_NAME, VERSION FROM V\$INSTANCE";

    my $sqlStmntDb = $db->prepare($sqlDbInfo) or die $DBI::errstr;
    $sqlStmntDb->execute() or die $db->errstr;

    my %database;

    my $sqlStmntHashref = $sqlStmntDb->fetchrow_hashref();
    my $dbVersion = $sqlStmntHashref->{VERSION};
    my $dbInstanceName = $sqlStmntHashref->{INSTANCE_NAME};

    $sqlStmntDb->finish();

    if ( $dbVersion =~ /^9\./ )
    {
        my %databaseDetails = ( dbName => $dbInstanceName, dbInstanceName => $dbInstanceName, dbVersion => $dbVersion);
        $database{$dbKeyCnt} = \%databaseDetails;
    }
    else
    {

        $sqlDbInfo = "SELECT DB_UNIQUE_NAME FROM V\$DATABASE";

        $sqlStmntDb = $db->prepare($sqlDbInfo) or die $DBI::errstr;
        $sqlStmntDb->execute() or die $db->errstr;

        my $dbName = $sqlStmntDb->fetchrow_hashref()->{DB_UNIQUE_NAME};

        $sqlStmntDb->finish();

        my %databaseDetails = ( dbName => $dbName, dbInstanceName => $dbInstanceName, dbVersion => $dbVersion);
        $database{$dbKeyCnt} = \%databaseDetails;

    }


    #print Dumper(\%database);

    if ($dbKeyCnt > 1)
    {
        print "Error: more than one db instance\n";
        exit 1;
    }

    #my $dbName = lc($database{0}{"dbName"});

    #print "DB: $dbName  Instance: $dbInstanceName\n";

    my $sqlClusterInfo = "SELECT PARALLEL FROM V\$INSTANCE";
    my $sqlClusStmnt = $db->prepare($sqlClusterInfo) or die $DBI::errstr;
    $sqlClusStmnt->execute() or die $db->errstr;

    $isCluster = $sqlClusStmnt->fetchrow_hashref()->{PARALLEL};
    $sqlClusStmnt->finish();

    my $sqlDbNodesInfo = "select value from v\$parameter where name='cluster_database_instances'";
    my $sqlDbNodeStmnt = $db->prepare($sqlDbNodesInfo) or die $DBI::errstr;
    $sqlDbNodeStmnt->execute() or die $db->errstr;

    $numDbNodes = $sqlDbNodeStmnt->fetchrow_hashref()->{VALUE};
    $sqlDbNodeStmnt->finish();

    my $sqlSpfileInfo = "select value from v\$parameter where name='spfile'";
    my $sqlSpfileStmnt = $db->prepare($sqlSpfileInfo) or die $DBI::errstr;
    $sqlSpfileStmnt->execute() or die $db->errstr;

    $spfile = $sqlSpfileStmnt->fetchrow_hashref()->{VALUE};
    $sqlDbNodeStmnt->finish();

    my $archiveLogInfo = "SELECT LOG_MODE FROM V\$DATABASE";
    my $archiveLogStmnt = $db->prepare($archiveLogInfo) or die $DBI::errstr;
    $archiveLogStmnt->execute() or die $db->errstr;

    $recoveryLogEnabled = $archiveLogStmnt->fetchrow_hashref()->{LOG_MODE};
    $archiveLogStmnt->finish();

    if (defined($spfile))
    {
        #print "spfile $spfile.\n";
        my $createPfile = "CREATE PFILE='/tmp/oracledb_pfile.ora' FROM SPFILE";
        my $createPfileStmnt = $db->prepare($createPfile) or die $DBI::errstr;
        $createPfileStmnt->execute() or die $db->errstr;
        $createPfileStmnt->finish();
    }

    my $sqlDbFiles = "SELECT NAME FROM V\$DATAFILE UNION SELECT MEMBER FROM V\$LOGFILE UNION SELECT NAME FROM V\$CONTROLFILE";

    $sqlStmntDb = $db->prepare($sqlDbFiles) or die $DBI::errstr;
    $sqlStmntDb->execute() or die $db->errstr;

    my @dirs;
    my @files;

    while (my $refStmntDb = $sqlStmntDb->fetchrow_hashref())
    {
        my $fileName = $refStmntDb->{NAME};
        my $dirName = dirname($fileName);
        push(@files, $fileName);
        push(@dirs, $dirName);
    }

    $sqlStmntDb->finish();

    my %hfiles;
    undef @hfiles{@files};
    my @uniqueFiles = keys %hfiles;

    my %hdirs;
    undef @hdirs{@dirs};
    my @uniqueDirs = keys %hdirs;

    #print "@uniqueDirs\n";

    my $isAsmVolume=0;
    foreach my $dir (@uniqueDirs)
    {
        if ( $dir =~ /^\+/ )
        {
            $isAsmVolume=1;
            #print "Found ASM volume\n";
        }
    }

    if ($isAsmVolume)
    {

        #print "Collecting ASM detials....\n";
        
        my $sqlDbInfo = "SELECT GROUP_NUMBER FROM V\$ASM_CLIENT";

        my $sqlStmntDb = $db->prepare($sqlDbInfo) or die $DBI::errstr;
        $sqlStmntDb->execute() or die $db->errstr;

        my $dgKeyCount=0;
        my %diskGroups;

        while (my $refStmntDb = $sqlStmntDb->fetchrow_hashref())
        {
            my $dgNumber = $refStmntDb->{GROUP_NUMBER};
        
            #print "Disk group number: $dgNumber\n";

            my $sqlGrpInfo = "SELECT NAME, STATE FROM V\$ASM_DISKGROUP WHERE GROUP_NUMBER = $dgNumber";

            my $sqlStmntGrp = $db->prepare($sqlGrpInfo) or die $DBI::errstr;
            $sqlStmntGrp->execute() or die $db->errstr;

            while (my $refStmntGrp = $sqlStmntGrp->fetchrow_hashref())
            {
                my $groupName = $refStmntGrp->{NAME};
                my $groupState = $refStmntGrp->{STATE};

                my %disks;
                my $diskKeyCount=0;

                my $sqlDiskInfo = "SELECT DISK_NUMBER, NAME, PATH FROM V\$ASM_DISK WHERE GROUP_NUMBER = $dgNumber";

                my $sqlStmntDisk = $db->prepare($sqlDiskInfo) or die $DBI::errstr;
                $sqlStmntDisk->execute() or die $db->errstr;

                while (my $refStmntDisk = $sqlStmntDisk->fetchrow_hashref())
                {
                    my $diskNumber = $refStmntDisk->{DISK_NUMBER};
                    my $diskName = $refStmntDisk->{NAME};
                    my $diskPath = $refStmntDisk->{PATH};
                    my $asmlibDisk = $diskPath;
                    if("ORCL:" eq substr($diskPath, 0, 5))
                    {
                        $asmlibDisk = `echo $diskPath | cut -d: -f2`;
                        chomp $asmlibDisk;
                        $diskPath = "/dev/oracleasm/disks/".$asmlibDisk;
                        $isASMLib=1;
                    }
                    if("/dev/oracleasm/disks/" eq substr($diskPath, 0, 21))
                    {
                        $asmlibDisk = `echo $diskPath | cut -d/ -f5`;
                        chomp $asmlibDisk;
                        $isASMLib=1;
                    }

                    my %diskDetails = ( diskName => $diskName, diskNumber => $diskNumber, diskPath => $diskPath , diskLabel => $asmlibDisk);
                    $disks{$diskKeyCount} = \%diskDetails;
                    ++$diskKeyCount;
                }

                $sqlStmntDisk->finish();

                #print Dumper(\%disks);

                my %dgDetails = ( dgName => $groupName, dgNumber => $dgNumber, dgState => $groupState, diskCount => $diskKeyCount, disks => \%disks);

                $diskGroups{$dgKeyCount} = \%dgDetails;

                #print "Disk group $dgKeyCount details:\n";
                #print Dumper(\%dgDetails);

                ++$dgKeyCount;
            }

            $sqlStmntGrp->finish();

            #print "ASM Disk group details: \n";
            #print Dumper(\%diskGroups);
        }
        $sqlStmntDb->finish();

        $databases{$dbKeyCnt} = { dbName => $database{0}{"dbName"}, dbInstanceName => $database{0}{"dbInstanceName"}, dbVersion => $database{0}{"dbVersion"}, isCluster => $isCluster, totalDbNodes => $numDbNodes, spfile => $spfile, recoveryLogEnabled => $recoveryLogEnabled, dbFiles => \@uniqueFiles, dbFileLoc => \@uniqueDirs, usingAsmDg => 'TRUE', dgCount => $dgKeyCount, asmDiskGroups => \%diskGroups};

    }
    else
    {
        $databases{$dbKeyCnt} = { dbName => $database{0}{"dbName"}, dbInstanceName => $database{0}{"dbInstanceName"}, dbVersion => $database{0}{"dbVersion"}, isCluster => $isCluster, totalDbNodes => $numDbNodes, spfile => $spfile, recoveryLogEnabled => $recoveryLogEnabled, dbFiles => \@uniqueFiles, dbFileLoc => \@uniqueDirs, usingAsmDg => 'FALSE'};
    }

    ++$dbKeyCnt;

    #print Dumper(\%databases);

}

sub WriteToConfigFile
{
    open (FILE, "+> $outputFile") || die "Unable to open output file $outputFile:$!\n";

    # my $SRC_CONF_FILE=$ARGV[4];
    open (CONF_FILE, "+>> $SRC_CONF_FILE") || die "Unable to open conf file $SRC_CONF_FILE:$!\n";

    my $ORACLE_HOME=$ARGV[1];
    if ($isAsm)
    {
        
    }
    else
    {
        for (my $dbCnt=0; $dbCnt < $dbKeyCnt; $dbCnt++)
        {
            my $levelCnt = 1;
            #print FILE "DBInstanceName=$databases{$dbCnt}{\"dbInstanceName\"}\n";
            print FILE "DBVersion=$databases{$dbCnt}{\"dbVersion\"}\n";
            print CONF_FILE "DBVersion=$databases{$dbCnt}{\"dbVersion\"}\n";
            print FILE "isCluster=$databases{$dbCnt}{\"isCluster\"}\n";
            print CONF_FILE "isCluster=$databases{$dbCnt}{\"isCluster\"}\n";
            print CONF_FILE "TotalDbNodes=$databases{$dbCnt}{\"totalDbNodes\"}\n";
            if (defined($spfile))
            {
                print CONF_FILE "SPFile=$databases{$dbCnt}{\"spfile\"}\n";
            }
            print FILE "RecoveryLogEnabled=$databases{$dbCnt}{\"recoveryLogEnabled\"}\n";
            print FILE "InstallPath=$ORACLE_HOME\n";
            print FILE "usingAsmDg=$databases{$dbCnt}{\"usingAsmDg\"}\n";
            print FILE "dbFileLoc=@{$databases{$dbCnt}{\"dbFileLoc\"}}\n";
            if ($databases{$dbCnt}{"usingAsmDg"} =~ /TRUE/)
            {
                print CONF_FILE "isASM=YES\n";
                if($isASMLib)
                {
                    print CONF_FILE "isASMLib=YES\n";
                    print CONF_FILE "isASMRaw=NO\n";
                }
                else
                {
                    print CONF_FILE "isASMLib=NO\n";
                    print CONF_FILE "isASMRaw=YES\n";
                }
            }
            else
            {
                print CONF_FILE "isASM=NO\n";
            }
            print FILE "DBName=$databases{$dbCnt}{\"dbName\"}\n";
            print CONF_FILE "OracleDBName=$databases{$dbCnt}{\"dbName\"}\n";
            #print FILE "DBInstanceName=$databases{$dbCnt}{\"dbInstanceName\"}\n";
            #print FILE "DBVersion=$databases{$dbCnt}{\"dbVersion\"}\n";
            #print FILE "isCluster=$databases{$dbCnt}{\"isCluster\"}\n";
            #print FILE "dbFileLoc=@{$databases{$dbCnt}{\"dbFileLoc\"}}\n";
            #print FILE "usingAsmDg=$databases{$dbCnt}{\"usingAsmDg\"}\n";
            if ($databases{$dbCnt}{"usingAsmDg"} =~ /TRUE/)
            {
                my $dgKeyCount = $databases{$dbCnt}{"dgCount"};
                print CONF_FILE "DiskGroupName=";
                for (my $dgCnt=0; $dgCnt < $dgKeyCount; $dgCnt++)
                {
                    print CONF_FILE "$databases{$dbCnt}{\"asmDiskGroups\"}{$dgCnt}{\"dgName\"},";
                    print FILE "DiskGroupName=$databases{$dbCnt}{\"asmDiskGroups\"}{$dgCnt}{\"dgName\"}(ASMDiskGroupName),$levelCnt\n";
                    $levelCnt++;
                    #print FILE "DiskGroupNumber=$databases{$dbCnt}{\"asmDiskGroups\"}{$dgCnt}{\"dgNumber\"}\n";
                    my $diskKeyCount = $databases{$dbCnt}{"asmDiskGroups"}{$dgCnt}{"diskCount"};
                    for (my $dskCnt=0; $dskCnt < $diskKeyCount; $dskCnt++)
                    {
                        #print FILE "DiskNumber=$databases{$dbCnt}{\"asmDiskGroups\"}{$dgCnt}{\"disks\"}{$dskCnt}{\"diskNumber\"}\n";
                        print FILE "DiskName=$databases{$dbCnt}{\"asmDiskGroups\"}{$dgCnt}{\"disks\"}{$dskCnt}{\"diskName\"}(ASMDiskName),$levelCnt\n";
                        $levelCnt++;
                        #print FILE "DiskLabel=$databases{$dbCnt}{\"asmDiskGroups\"}{$dgCnt}{\"disks\"}{$dskCnt}{\"diskLabel\"}(ASMDiskLabel),$levelCnt\n";
                        #$levelCnt++;
                        print FILE "DiskPath=$databases{$dbCnt}{\"asmDiskGroups\"}{$dgCnt}{\"disks\"}{$dskCnt}{\"diskPath\"},$levelCnt\n";
                        #$levelCnt--;
                        $levelCnt--;
                    }
                    $levelCnt--;
                }
                print CONF_FILE "\n";
            }
            if ($databases{$dbCnt}{"usingAsmDg"} =~ /TRUE/)
            {
                {
                   my $dgKeyCount = $databases{$dbCnt}{"dgCount"};
                   print CONF_FILE "ASMDiskNames=";
                   for (my $dgCnt=0; $dgCnt < $dgKeyCount; $dgCnt++)
                   {
                       #print FILE "DiskGroupNumber=$databases{$dbCnt}{\"asmDiskGroups\"}{$dgCnt}{\"dgNumber\"}\n";
                       my $diskKeyCount = $databases{$dbCnt}{"asmDiskGroups"}{$dgCnt}{"diskCount"};
                       for (my $dskCnt=0; $dskCnt < $diskKeyCount; $dskCnt++)
                       {
                           #print FILE "DiskNumber=$databases{$dbCnt}{\"asmDiskGroups\"}{$dgCnt}{\"disks\"}{$dskCnt}{\"diskNumber\"}\n";
                           print CONF_FILE "$databases{$dbCnt}{\"asmDiskGroups\"}{$dgCnt}{\"disks\"}{$dskCnt}{\"diskName\"},";
                       }
                   }
                   print CONF_FILE "\n";
                }
                
                if($isASMLib)
                {
                    my $dgKeyCount = $databases{$dbCnt}{"dgCount"};
                    print CONF_FILE "ASMLibDiskLabels=";
                    for (my $dgCnt=0; $dgCnt < $dgKeyCount; $dgCnt++)
                    {
                        #print FILE "DiskGroupNumber=$databases{$dbCnt}{\"asmDiskGroups\"}{$dgCnt}{\"dgNumber\"}\n";
                        my $diskKeyCount = $databases{$dbCnt}{"asmDiskGroups"}{$dgCnt}{"diskCount"};
                        for (my $dskCnt=0; $dskCnt < $diskKeyCount; $dskCnt++)
                        {
                            #print FILE "DiskNumber=$databases{$dbCnt}{\"asmDiskGroups\"}{$dgCnt}{\"disks\"}{$dskCnt}{\"diskNumber\"}\n";
                            print CONF_FILE "$databases{$dbCnt}{\"asmDiskGroups\"}{$dgCnt}{\"disks\"}{$dskCnt}{\"diskLabel\"},";
                        }
                    }
                    print CONF_FILE "\n";
                }
                else
                {
                    my $dgKeyCount = $databases{$dbCnt}{"dgCount"};
                    print CONF_FILE "ASMRawDiskPaths=";
                    for (my $dgCnt=0; $dgCnt < $dgKeyCount; $dgCnt++)
                    {
                        #print FILE "DiskGroupNumber=$databases{$dbCnt}{\"asmDiskGroups\"}{$dgCnt}{\"dgNumber\"}\n";
                        my $diskKeyCount = $databases{$dbCnt}{"asmDiskGroups"}{$dgCnt}{"diskCount"};
                        for (my $dskCnt=0; $dskCnt < $diskKeyCount; $dskCnt++)
                        {
                           #print FILE "DiskNumber=$databases{$dbCnt}{\"asmDiskGroups\"}{$dgCnt}{\"disks\"}{$dskCnt}{\"diskNumber\"}\n";
                            print CONF_FILE "$databases{$dbCnt}{\"asmDiskGroups\"}{$dgCnt}{\"disks\"}{$dskCnt}{\"diskPath\"},";
                        }
                     }
                     print CONF_FILE "\n";
                }
            }
        }
    }

    close CONF_FILE;
    close FILE;
}


######################   M A I N #########################


&ParseArgs;

&GetDbHandle;

if ($isAsm)
{
    &GetAsmDetails;
    &WriteToConfigFile;
}
else
{
    &GetDbDetails;
    &WriteToConfigFile;
}

$db->disconnect;

# write to the config file
#    &WriteToConfigFile;

exit 0;
