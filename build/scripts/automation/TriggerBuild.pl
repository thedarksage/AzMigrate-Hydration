use warnings;
use strict;
use Getopt::Long;
use File::Path;

# Parase arguments passed to the script.
GetOptions( 'branchname=s' => \my $branchname, 
            'incremental=s' => \my $incremental,
            'srcpath=s' => \my $srcpath,
			'config=s' => \my $config,
);

# Usage
sub Usage
{
	my $message = shift;
	print "\n\n$message";
	print <<TOC;	
Usage
=====
$0 -b <branchname> -i <yes|no> -s <sourcepath>
TOC

  exit 1;
}

# Print usage when invalid arguments are passed.
&Usage("Please provide branchname as an argument.\n") unless defined $branchname;
&Usage("Value of incremental switch can be either yes or no.\n") unless $incremental =~ /^(yes|no)$/i;
&Usage("Please provide source path as an argument.\n") unless defined $srcpath;
&Usage("Value of config switch can be either release or debug.\n") unless $config =~ /^(release|debug)$/i;

unless (-e $srcpath and -d $srcpath) {
    print "$srcpath directory does not exist. Please provide an existing source path.\n";
	exit 1;
}

# Print values passed to all arguments passed.
my $blddir = "${srcpath}\\${branchname}";
print "branchname value: $branchname\n";
print "incremental value: $incremental\n";
print "srcpath value: $srcpath\n";
print "blddir value: $blddir\n";
print "config value: $config\n";

# Declare global variables.
my $pwspath = "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
my $appdatatempapth = "C:\\Users\\mabldact\\AppData\\Local\\Temp";

# Lot of cab files are accumulating here and so removing all these
sub RemoveAppDataTempPath
{
	if ( -d "$appdatatempapth") {
		unless (rmtree "$appdatatempapth") {
			print "Failed to remove $appdatatempapth.\n";
			exit 1;		
		}
	}
}

sub CheckoutOrUpdateCode
{
	# In case of incremental build, change to source code directory, update source code and switch to the branch specified.
	if (lc($incremental) eq "yes") {		
		chdir "$blddir\\InMage-Azure-SiteRecovery";
		system("git pull");
		if ($? == 0) {
			print "Updated git repo successfuly.\n";
			SwitchBranch();
		} else {
			print "Failed to update git repo.\n";
			exit 1;
		}		
	}else{		
		# In case of clean build remove build directory, create it afresh and change to build directory.
		if ( -d "$blddir") {
			system("rmdir $blddir /s /q");
			if ($? != 0) {
				print "Failed to remove $blddir.\n";
				exit 1;		
			}
		}
		unless (mkdir "$blddir") {
			print "Failed to make $blddir.\n";
			exit 1;		
		}		
		unless (chdir "$blddir") {
			print "Failed to switch to $blddir.\n";
			exit 1;		
		}		
		
		# Clone git repo and switch to the branch specified.
		system("git clone https://msazure.visualstudio.com/DefaultCollection/One/_git/InMage-Azure-SiteRecovery");
		if ($? == 0) {
			print "Cloned git repo successfuly.\n";
			SwitchBranch();
		} else {
			print "Failed to clone git repo.\n";
			exit 1;
		}
	}
}

sub SwitchBranch
{
	chdir "$blddir\\InMage-Azure-SiteRecovery";
	system("git checkout $branchname");
	if ($? == 0) {
		print "Switched to $branchname successfuly.\n";
	} else {
		print "Failed to switch to $branchname.\n";
		exit 1;
	}	
}

sub InvokeBuilds
{
	chdir "${blddir}\\InMage-Azure-SiteRecovery\\build\\scripts\\automation";
	system("${pwspath}", "-command", "${blddir}\\InMage-Azure-SiteRecovery\\build\\scripts\\automation\\InvokeBuild.ps1", "${branchname}", "${incremental}", "${blddir}\\InMage-Azure-SiteRecovery", "${config}");	
}

sub LogoutAllUsers
{
	print "DEBUG: in LogoutAllUsers()\n";
	my $sessions= `query session`;
	print $sessions;
	my @sessionlines = split /\n/, $sessions;
	foreach my $sessionline (@sessionlines) {

		if ($sessionline =~ m/Disc/) {
			$sessionline =~ s/^\s+//;
			my @disctokens = split /\s+/, $sessionline;

			# ignore session zero
			if ($disctokens[1] eq '0') {
				next;
			}

			# ignore console
			if ($sessionline =~ m/console/) {
				next;
			}

			print "$sessionline\n";
			print "logoff $disctokens[1]\n";
			system("logoff $disctokens[1]");
			next;
		}

		if ($sessionline !~ m/rdp/) {
			next;
		}
		if ($sessionline =~ m/Listen/) {
			next;
		}
		$sessionline =~ s/\s+//;
		my @tokens = split /\s+/, $sessionline;
		print "$sessionline\n";
		print "logoff $tokens[2]\n";
		system("logoff $tokens[2]");
	}
}

sub main
{
	print "DEBUG: in main()\n";
	RemoveAppDataTempPath();
	LogoutAllUsers();
	CheckoutOrUpdateCode();
	InvokeBuilds()
}

# -----------------------------------------------------------------------
# Entry point in the program
# -----------------------------------------------------------------------

&main();
