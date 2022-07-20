use warnings;
use winbldroutines;
use strict;
use Time::HiRes;
use Time::Duration;
use sigtrap qw{handler sigHandler normal-signals error-signals};

# -----------------------------------------------------------------------
# Useful functions - prototypes
# -----------------------------------------------------------------------
sub usage;
sub ParseCmdLine;
sub GetPlan;
sub ValidateOptions;
sub main;

# -----------------------------------------------------------------------
# Global variable declarations
# -----------------------------------------------------------------------

my @OPTIONSET;
my $proper_usage_flag = 0;

my %permissible_arguments = (
							 'products' => 1,
                             'config' => 1,
                             'branch' => 1,
                             'buildphase' => 1,
							 'majorversion' => 1,
 							 'minorversion' => 1,
							 'patchsetversion' => 1,
							 'patchversion' => 1,
                             'mailto' => 1,
                             'buildquality' => 1,
                             'cvsupdate' => 1,
                             'partner' => 1,
                             'incremental' => 1,
                             'srcpath' => 1,
                             'buildcopypath' => 1,
							 'loadsymbols' => 1,
			                 'symbolserverpath' => 1,
							 'signing' => 1,
                             'file' => 1
		     	    );

my %permissible_products = ('cx' => 1, 'ucx' => 1, 'cx_tp' => 1, 'vx' => 1, 'fx' => 1, 'ua' => 1, 'asrua' => 1, 'asrsetup' => 1, 'axiom' => 1, 'ua_win2k' => 1, 'pi' => 1, 'br1' => 1, 'bx' => 1, 'scoutvcon' => 1, 'scoutcloudvcon' => 1, 'psmsi' => 1);

my %permissible_config = ('debug' => 1, 'release' => 1);

my %permissible_branch = (
			   'develop' => 1,
			   'release' => 1,
               'master' => 1
			 );

# -----------------------------------------------------------------------
# Subroutines used in the program
# -----------------------------------------------------------------------

sub sigHandler
{
  my $sig = shift;
  print "Caught a SIG$sig !\n";
  die;
}

sub ParseConfigurationFile
{
 # This function is only for parsing a SINGLE configuration file.
 # It fills @OPTIONSET elements with hashrefs to option sets.
 # A single configuration file can accommodate multiple option sets.
 # An option set begins with a BEGIN and ends with an END.

 print "DEBUG: in ParseConfigurationFile()\n";

 my ($start_parsing,$counter,$lines_processed)=(3,0,0);
 my (%begin_line_number,%end_line_number);
 my $ConfigFileName = shift;

 &usage("Configuration file name suffix must be 'bcf' and prefix must contain only alphanumeric characters and underscores!\n") unless $ConfigFileName =~ /^[\w_]+\.bcf$/i;

 open(CNF_FILE,"<$ConfigFileName") or die "Cannot open configuration file $ConfigFileName for reading!\n";

 while(<CNF_FILE>)
 {
   if (/^\s*BEGIN\s*$/i)
   { $start_parsing = 1; $counter++; $begin_line_number{$counter} = $.; }
   if (/^\s*END\s*$/i)
   { $start_parsing = 0; $counter++; $end_line_number{$counter} = $. ; }
   # counter is supposed to have odd values for BEGIN and even values for END.
   # So if a BEGIN has even counter, it means there was no END for the previous BEGIN, or it's a nested BEGIN-END.
   # Similarly, if an END has odd counter, it means there was no corresponding BEGIN or it's a nested BEGIN-END.
   # Do not allow this.

   if ($start_parsing == 0)
   {
    die "Configuration file has a missing BEGIN for the END at line $end_line_number{$counter} (or has a nested BEGIN-END which is not allowed)!\n" if $counter%2==1;
    die "Configuration file has an empty BEGIN-END block ending at line $end_line_number{$counter} (not allowed)!\n" if $end_line_number{$counter} == $begin_line_number{$counter-1} + 1;
   }

   if ($start_parsing == 1)
   {
     die "Configuration file has a missing END for the BEGIN at line $begin_line_number{$counter - 1} (or has a nested BEGIN-END which is not allowed)!\n" if $counter%2 == 0; 
     # Current line is either a BEGIN or something after that and before an END.
     # Ignore lines that are beginning with # (i.e. a comment) or that are empty!
     $lines_processed++ and next if /^\s*#\s*/ or /^\s*$/;
     # Consider lines that are of the form <spaces>something<spaces>=<spaces>something_else<spaces>
     if (/^\s*(\w+)\s*=\s*(.+?)\s*$/)
     {
       &usage("Inappropriate argument $1 entered!\n") unless exists $permissible_arguments{lc($1)};
       $lines_processed++;
       # Ignore lines that mention another configuration file in a "file" argument!
       next if lc($1) eq "file";
       # Fill the OPTIONSET hashref here
       # The block iteration value is ($counter+1)/2. Subtract 1 from this since arrays index from 0.
       $OPTIONSET[($counter+1)/2 - 1]->{lc($1)} = $2;
     }
   }
 }
 # Final check is whether file has an END at its end. In that case, counter is even.
 die "The last BEGIN in the configuration file has an END missing!\n" if $counter%2==1;
 die "The configuration file has no BEGIN or END!\n" if $start_parsing == 3;
 return;
}

sub ParseCommandLineArgs
{
 # This function is only for parsing command-line arguments.
 # It fills a hashref residing in $OPTIONSET[0].
 # Command-line mode can accommodate only 1 option set.

 print "DEBUG: in ParseCommandLineArgs()\n";
 
 foreach (@ARGV)
 {
   /(.+)=(.+)/;
   $OPTIONSET[0]->{lc($1)} = $2;
 }
}

sub ParseCmdLine
{
  # This is the main parsing function.

  print "DEBUG: in ParseCmdLine()\n";

  usage("Please enter some arguments!\n") unless defined $ARGV[0];

  # Do an initial loop over the arguments. If "file" is found, call ParseConfigurationFile.
  # Else, call ParseCommandLineArgs.

  foreach (@ARGV)
  {
    /(.+)=(.+)/;
    usage("Inappropriate argument $1 entered!\n") unless exists $permissible_arguments{lc($1)};
    if (lc($1) eq "file")
    {
     ParseConfigurationFile($2);
     return;
    }
  }

  # If control remain till here, it means that all arguments are permissible and none of them is "file".
  ParseCommandLineArgs();
  return;
}

sub ValidateOptions
{

 # This is a generic function which loops over the option sets and validates each 1 of the arguments.

 print "DEBUG: in ValidateOptions()\n";

 foreach my $i (0..$#OPTIONSET) # Beginning of 'for' loop over OPTIONSET elements
 {
  # Product

  print "DEBUG: parsing option set $i\n";
 
  if (defined $OPTIONSET[$i]->{products})
  {
    my @products_to_build;
    while( $OPTIONSET[$i]->{products} =~ /(\w+)(,?)/g )
    {
      push @products_to_build, lc($1) if defined $permissible_products{lc($1)};
    }
    $OPTIONSET[$i]->{products} = \@products_to_build;
  }
  else
  {
    $OPTIONSET[$i]->{products} = [qw(cx ucx cx_tp fx vx ua asrua asrsetup axiom ua_win2k pi br1 bx scoutvcon scoutcloudvcon)] ;
  }

  &usage("In option set $i: component can be one or more (comma-separated) of cx, ucx, cx_tp, vx, fx, pi, ua, asrua, asrsetup, ua_win2k, br1, bx, scoutvcon, scoutcloudvcon or axiom (default is cx,asrsetup,ucx,cx_tp,fx,vx,ua,asrua,ua_win2k,pi,br1,bx,scoutvcon,scoutcloudvcon,axiom). If you enter anything else, it will be silently ignored!\n") unless exists $OPTIONSET[$i]->{products}->[0];

  # Configuration
  
  if (defined $OPTIONSET[$i]->{config})
  {
    my @configs_to_build;
    while( $OPTIONSET[$i]->{config} =~ /(\w+)(,?)/g )
    {
      push @configs_to_build, lc($1) if defined $permissible_config{lc($1)};
    }
    $OPTIONSET[$i]->{config} = \@configs_to_build;
  }
  else
  {
    $OPTIONSET[$i]->{config} = [qw(release)] ;
  }

  &usage("In option set $i: config can be one or more (comma-separated) of debug and release (default is release). If you enter anything else, it will be silently ignored!\n") unless exists $OPTIONSET[$i]->{config}->[0];

  # Other options

  $OPTIONSET[$i]->{branch} = "TRUNK" unless defined $OPTIONSET[$i]->{branch};
  &usage("In option set $i: branch not supported!\n") unless exists $permissible_branch{$OPTIONSET[$i]->{branch}};

  $OPTIONSET[$i]->{buildphase} = "DIT" unless defined $OPTIONSET[$i]->{buildphase};

  $OPTIONSET[$i]->{mailto} = 'inmiet@microsoft.com' unless defined $OPTIONSET[$i]->{mailto};

  $OPTIONSET[$i]->{buildquality} = "DAILY" unless defined $OPTIONSET[$i]->{buildquality};

  $OPTIONSET[$i]->{cvsupdate} = "yes" unless defined $OPTIONSET[$i]->{cvsupdate};
  &usage("In option set $i: cvs update mode can be either yes or no (default is yes)!\n") unless $OPTIONSET[$i]->{cvsupdate} =~ /^(yes|no)$/i;
 
  $OPTIONSET[$i]->{partner}   = "inmage" unless defined $OPTIONSET[$i]->{partner};
  &usage("In option set $i: partner can be either inmage or xiotech or quantum (default is inmage)!\n") unless $OPTIONSET[$i]->{partner} =~ /^(inmage|xiotech|INTEL|quantum)$/i;

  $OPTIONSET[$i]->{incremental} = "no" unless defined $OPTIONSET[$i]->{incremental};
  &usage("In option set $i: incremental can be either yes or no (default is no)!\n") unless $OPTIONSET[$i]->{incremental} =~ /^(yes|no)$/i;
  
  $OPTIONSET[$i]->{loadsymbols} = "no" unless defined $OPTIONSET[$i]->{loadsymbols};
  &usage("In option set $i: loadsymbols can be either yes or no (default is no)!\n") unless $OPTIONSET[$i]->{loadsymbols} =~ /^(yes|no)$/i;

  &usage("In option set $i: symbolserverpath has to be defined since loadsymbols is yes!\n") if defined $OPTIONSET[$i]->{loadsymbols} and not defined $OPTIONSET[$i]->{symbolserverpath};
  
  $OPTIONSET[$i]->{signing} = "no" unless defined $OPTIONSET[$i]->{signing};
  &usage("In option set $i: signing can be either complete or mandatory (default is complete)!\n") unless $OPTIONSET[$i]->{signing} =~ /^(complete|mandatory)$/i;
  
  # B. Mandatory parameters

  &usage("In option set $i: Please enter the top source directory!\n") unless defined $OPTIONSET[$i]->{srcpath};

  &usage("In option set $i: Please enter the target directory where built binaries shall be copied!\n") unless defined $OPTIONSET[$i]->{buildcopypath};

  # If control reaches till here, it means all arguments are entered properly.

  $proper_usage_flag = 1;
  if ($proper_usage_flag == 1)
  {
    print "\nPrinting set $i now...\n\n";
    print "$_ = $OPTIONSET[$i]->{$_}\n" foreach (keys %{$OPTIONSET[$i]});
  }

 } # End of 'for' loop over OPTIONSET elements

}

sub Build
{
  my @PerOptBldObjects;
  my $NumOfPerOptBldObjects = $#OPTIONSET + 1;
 
  foreach my $i (0..$#OPTIONSET)
  {
    $PerOptBldObjects[$i] = winbldroutines->new($OPTIONSET[$i]);
    $PerOptBldObjects[$i]->Start();
  }
  
  return;
}

sub usage
{

  my $message = shift;
  print "\n\n$message";

  print <<TOC;

$0: Usage details
====================================
$0 [products=cx|ucx|cx_tp|vx|fx|ua|ua_win2k|pi|scoutvcon|scoutcloudvcon|axiom|bx|br1|all] [cvsupdate=yes|no] [partner=inmage|INTEL|xiotech] [branch=<CVSBranch>] [config=debug|release|both] [srcpath=<SrcCodePath>] [file=<ConfigurationFile>] [buildcopypath=<DirForCopyingBinaries>] [incremental=yes|no] [buildphase=<3-letter build phase>] [mailto=<CommaSepEmailIDs>] [buildquality=<QualityOfBuild-like BETA/ALPHA/GA> [singing=complete|mandatory]]

Notes:
======

1. You can specify more than 1 product value (comma-separated). Permitted products are cx, ucx, cx_tp, vx, fx, ua, scoutvcon,scoutcloudvcon, axiom, ua_win2k, bx  br1 or all. Anything else will be silently ignored.

2. If a configuration file is specified as value for the 'file' parameter, other command-line arguments (including other 'file' arguments) are silently ignored.

3. The configuration file can have multiple option sets defined within BEGIN-END blocks. Option sets are made of the same parameters as on a command-line. If a 'file' parameter is entered in these option sets, it will be silently ignored as recursion is not permitted in such cases. Option sets are counted beginning with 0 (so a command-line option set is also the 0th set!)

4. Nested and empty BEGIN-END blocks are not permitted in the configuration file. Comments (beginning with a #) and blank lines are silently ignored.

TOC

  exit 1;

}

sub main
{
  print "DEBUG: in main()\n";
  ParseCmdLine();
  ValidateOptions();
  my $program_start_time = time();
  Build();
  print "Total runtime for builds = ", duration(time() - $program_start_time), ".\n";
}

# -----------------------------------------------------------------------
# Entry point in the program
# -----------------------------------------------------------------------

&main();
