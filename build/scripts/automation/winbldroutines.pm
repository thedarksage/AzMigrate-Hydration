package winbldroutines;

use Time::HiRes;
use strict;
use Time::Duration;
use Mail::Sender;
use IO::Socket;
use Sys::Hostname;
use File::Path;
use File::Find;
use File::Touch;
use JSON;

# -----------------------------------------------------------------------------------------------------------------------------
# ------------------------ BEGIN: Class constructor, destructor and attribute APIs --------------------------------------------------
# -----------------------------------------------------------------------------------------------------------------------------

sub new
{
  my ($pkg,$optionsetref) = @_;
  # Do not allow instance constructors
  die "Instance-based constructors are not allowed. Please use the class constructor! Stopped" if ref($pkg);
  # Since optionsetref is a reference, we should not directly bless it.
  # Create a "data copy" of that reference locally here and then bless it.
  my $self = {};
  $self->{$_} = $optionsetref->{$_} foreach (keys %{$optionsetref});
  $self->{datewisedir} = undef;
  $self->{date} = undef;
  $self->{month} = undef;
  $self->{year} = undef;
  $self->{timeinsecs} = undef;
  chomp($self->{pwd} = `cd`);
  return bless $self, $pkg;
}

sub DatewiseDir
{
  my $this = shift;
  @_ ? $this->{datewisedir} = shift : $this->{datewisedir};
}

sub Date
{
  my $this = shift;
  @_ ? $this->{date} = shift : $this->{date};
}

sub Month
{
  my $this = shift;
  @_ ? $this->{month} = shift : $this->{month};
}

sub Year
{
  my $this = shift;
  @_ ? $this->{year} = shift : $this->{year};
}

sub TimeInSecs
{
  my $this = shift;
  @_ ? $this->{timeinsecs} = shift : $this->{timeinsecs};
}

sub SrcPath
{
  my $this = shift;
  @_ ? $this->{srcpath} = shift : $this->{srcpath};
}

sub Partner
{
  my $this = shift;
  @_ ? $this->{partner} = shift : $this->{partner};
}

sub Branch
{
  my $this = shift;
  @_ ? $this->{branch} = shift : $this->{branch};
}

sub BuildQuality
{
  my $this = shift;
  @_ ? $this->{buildquality} = shift : $this->{buildquality};
}

sub BuildCopyPath
{
  my $this = shift;
  @_ ? $this->{buildcopypath} = shift : $this->{buildcopypath};
}

sub ShouldLoadSymbols
{
  my $this = shift;
  return $this->{loadsymbols};
}

sub SymbolStorePath
{
  my $this = shift;
  @_ ? $this->{symbolserverpath} = shift : $this->{symbolserverpath};
}

sub BuildPhase
{
  my $this = shift;
  @_ ? $this->{buildphase} = shift : $this->{buildphase};
}

sub MajorVersion
{
  my $this = shift;
  @_ ? $this->{majorversion} = shift : $this->{majorversion};
}

sub MinorVersion
{
  my $this = shift;
  @_ ? $this->{minorversion} = shift : $this->{minorversion};
}

sub PatchSetVersion
{
  my $this = shift;
  @_ ? $this->{patchsetversion} = shift : $this->{patchsetversion};
}

sub PatchVersion
{
  my $this = shift;
  @_ ? $this->{patchversion} = shift : $this->{patchversion};
}

sub ProductList
{
  my $this = shift;
  return $this->{products};
}

sub Signing
{
  my $this = shift;
  return $this->{signing};
}

sub ConfigList
{
  my $this = shift;
  return $this->{config};
}

sub ShouldUpdateSource
{
  my $this = shift;
  return $this->{cvsupdate} =~ /yes/i ? 1 : 0;
}

sub ShouldCleanPrevBuild
{
  my $this = shift;
  return $this->{incremental} =~ /no/i ? 1 : 0;
}

sub GetThisScriptDir
{
  my $this = shift;
  return $this->{pwd};
}

sub DESTROY
{
  print "DEBUG: in DESTROY()\n";
  my $this = shift;
  my $this_script_dir = $this->GetThisScriptDir();
  chdir "$this_script_dir";
  my $srcpath = $this->SrcPath();
  # Undo branding changes if any
  my @files_to_be_deleted = (
	  
	  "$srcpath\\host\\common\\version.h.orig",
	  "$srcpath\\host\\common\\version.cs.orig",
	  "$srcpath\\server\\tm\\version.orig",	     
	  "$srcpath\\host\\setup\\version.iss.orig",
	  "$srcpath\\server\\windows\\ProcessServerMSI\\Version.wxi"
	   		);

  foreach my $file (@files_to_be_deleted)
  {
    $file =~ /^.+\\(.+)$/;
	print "DESTRUCTOR: Deleting file $file \n";
	unlink($file);
  }  
}

# -----------------------------------------------------------------------------------------------------------------------------
# ------------------------ END: Class constructor, destructor and attribute APIs --------------------------------------------------
# -----------------------------------------------------------------------------------------------------------------------------





# -----------------------------------------------------------------------------------------------------------------------------
# ------------------------ BEGIN: Class variables and any associated APIs --------------------------------------------------
# -----------------------------------------------------------------------------------------------------------------------------

my $PowerShellPath = 'C:\WINDOWS\system32\WindowsPowerShell\v1.0\powershell.exe';

my $mvss_exe = 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\Common7\IDE\devenv.com';

my $msbuild_exe = 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\MSBuild.exe';

my $ISCC_exe = 'C:\Program Files (x86)\Inno Setup 5\ISCC.exe';

# Don't delete or change the path of EsrpClient.exe. UnifiedAgentCustomActions.dll and UnifiedAgentCustomActions.CA.dll 
# signing happens only when EsrpClient.exe file exists at C:\esrpclient.1.2.25\tools.
my $signtool = 'C:\esrpclient.1.2.25\tools\EsrpClient.exe';

my $authfile = "C:\\esrpclientconfig\\Auth.json";

my $policyfile = "C:\\esrpclientconfig\\Policy.json";
		
my $zip_exe = 'C:\Program Files (x86)\7-Zip\7z';

my $pdbload_exe = 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\symstore.exe';

my $pdbcheck_exe = 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\symchk.exe';

my $sender = new Mail::Sender {smtp => 'cloudmail.microsoft.com', from => 'mabldadm@microsoft.com' };

my $failure_alert_ids = 'inmiet@microsoft.com' ;

my $month_details = {	
		'Jan' => 1, 'Feb' => 2, 'Mar' => 3, 'Apr' => 4, 'May' => 5, 'Jun' => 6,
		'Jul' => 7, 'Aug' => 8, 'Sep' => 9, 'Oct' => 10, 'Nov' => 11, 'Dec' => 12
		};
my $host = hostname; 

my ($addr)      =  inet_ntoa((gethostbyname($host))[4]);


my $partner_details = {
		'inmage' => {
				'string' => 'InMage',
				'company' => 'InMage Systems',
				'code' => '1'
			}
		};

my $pdb_filename_details = {
				'ua' => 'pdb_list_ua.txt',
				'cx' => 'pdb_list_cx.txt',
				'ucx' => 'pdb_list_cx.txt',
				'asrua' => 'pdb_list_asrua.txt',
				'asrsetup' => 'pdb_list_asrsetup.txt',
				'pi' => 'pdb_list_pi.txt',
				"psmsi" => 'pdb_list_psmsi.txt'
			   };

my $branch_details = {
			'develop'  => {
							'refsecs' => 1104517800,
							'smrrefsecs' => 1337193000,
						},	
			'release'  => {
							'refsecs' => 1104517800,
							'smrrefsecs' => 1337193000,
						},
			'master'  => {
							'refsecs' => 1104517800,
							'smrrefsecs' => 1337193000,
						}						
			};

sub GetPartnerString
{
  my $this = shift;
  my $partner = $this->Partner();
  die "$partner is not supported, stopped" if not exists ${$partner_details}{$partner};
  return $partner_details->{$partner}->{string};
}

sub GetPartnerCompanyString
{
  my $this = shift;
  my $partner = $this->Partner();
  die "$partner is not supported, stopped" if not exists ${$partner_details}{$partner};
  return $partner_details->{$partner}->{company};
}

sub GetPartnerCode
{
  my $this = shift;
  my $partner = $this->Partner();
  die "$partner is not supported, stopped" if not exists ${$partner_details}{$partner};
  return $partner_details->{$partner}->{code};
}

sub GetProductPDBListFileName
{
  my $this = shift;
  my $product = shift;
  return $pdb_filename_details->{$product};
}

sub GetRefSecs
{
  my $this = shift;
  my $branch = $this->Branch();
  my $partnerstring = $this->GetPartnerString();
  die "$branch is not supported, stopped" if not exists ${$branch_details}{$branch};
  return $branch_details->{$branch}->{refsecs};
}

sub GetSMRRefSecs
{
  my $this = shift;
  my $branch = $this->Branch();
  my $partnerstring = $this->GetPartnerString();
  die "$branch is not supported, stopped" if not exists ${$branch_details}{$branch};
  return $branch_details->{$branch}->{smrrefsecs};
}

sub GetMajorVersion
{
  my $this = shift;
  my $branch = $this->Branch();
  my $partnerstring = $this->GetPartnerString();
  my $MajorVersion = $this->MajorVersion();
  die "$branch is not supported, stopped" if not exists ${$branch_details}{$branch};
  return $MajorVersion;
}

sub GetMinorVersion
{
  my $this = shift;
  my $branch = $this->Branch();
  my $partnerstring = $this->GetPartnerString();
  my $MinorVersion = $this->MinorVersion();
  die "$branch is not supported, stopped" if not exists ${$branch_details}{$branch};
  return $MinorVersion;
}

sub GetPatchSetVersion
{
  my $this = shift;
  my $branch = $this->Branch();
  my $partnerstring = $this->GetPartnerString();
  my $PatchSetVersion = $this->PatchSetVersion();  
  die "$branch is not supported, stopped" if not exists ${$branch_details}{$branch};
  return $PatchSetVersion; 
}

sub GetPatchVersion
{
  my $this = shift;
  my $branch = $this->Branch();
  my $partnerstring = $this->GetPartnerString();
  my $PatchVersion = $this->PatchVersion();  
  die "$branch is not supported, stopped" if not exists ${$branch_details}{$branch};
  return $PatchVersion; 
}


my $product_details = {
		'UA' => {
			  'outputfile' => 'win_unified_setup.exe',
			  'proj' => {
				 	'build' => 'unified_agent',
					'packaging' => 'package_unified'
				    },		
			  'issdir' => qw(host\setup),
			  'soln' => qw(host\host.sln)
			},
		'CX_TP' => {
			  'outputfile' => 'cx_thirdparty_setup.exe',
			  'proj' => {
					'packaging' => 'setup_cxthirdparty'
				    },
			  'issdir' => qw(server\windows),
			  'soln' => qw(server\server.sln)
			},
		'UCX' => {
			  'outputfile' => 'ucx_server_setup.exe',
			  'proj' => {
					'packaging' => 'setup_ucxserver'
				    },
			  'issdir' => qw(server\windows),
			  'soln' => qw(server\server.sln)
			},
		'ASRUA' => {
			  'outputfile' => 'MicrosoftAzureSiteRecoveryUnifiedAgent.exe',
			   'proj' => {
					'build' => 'UnifiedAgentInstaller'					
				    },
			  'issdir' => qw(host\setup),
			  'soln' => qw(host\ASRSetup\ASRSetup.sln)				  
			},
		'ASRSETUP' => {
			  'outputfile' => 'MicrosoftAzureSiteRecoveryUnifiedSetup.exe',
			   'proj' => {
					'build' => 'UnifiedSetup'					
				    },
			  'issdir' => qw(server\windows),	
			  'soln' => qw(host\ASRSetup\ASRSetup.sln)				  
			},			
		'CX' => {
			  'outputfile' => 'cx_server_setup.exe',
			   'proj' => {
					'build' => 'build_cxserver',
					'packaging' => 'setup_cxserver'
				    },	
			  'issdir' => qw(server\windows),
			  'soln' => qw(server\server.sln)
			},
		'PI' => {
			  'outputfile' => 'win_pushserver_setup.exe',
			  'proj' => {
					'build' => 'build_push',
					'packaging' => 'setup_push'
				    },	
			  'issdir' => qw(build\scripts\pushinstall),
			  'soln' => qw(host\host.sln)			
			},
		'PSMSI' => {
			 'outputfile' => 'ProcessServer.msi',
			 'proj' => {
					'build' => 'ProcessServerMSI',
					'packaging' => 'ProcessServerMSI'
			 },
			 'issdir' => qw(server\windows),
			 'soln' => qw(server\server.sln)
			}
		};

sub GetSolnFileName
{
  my ($this,$product) = @_;
  $product = uc($product);
  die "Product $product not supported, stopped" if not exists ${$product_details}{$product};
  return $product_details->{$product}->{soln};
}

sub GetBuildProjectName
{
  my ($this,$product) = @_;
  $product = uc($product);
  die "Product $product not supported, stopped" if not exists ${$product_details}{$product};
  if (ref($product_details->{$product}->{proj}) eq "HASH") {
	  return $product_details->{$product}->{proj}->{build};
  } 
}

sub GetPackagingProjectName
{
  my ($this,$product) = @_;
  $product = uc($product);
  die "Product $product not supported, stopped" if not exists ${$product_details}{$product};
  if (ref($product_details->{$product}->{proj}) eq "HASH") {
	  return $product_details->{$product}->{proj}->{packaging};
  } 
}

sub GetBuildIssDir
{
  my ($this,$product) = @_;
  $product = uc($product);
  die "Product $product not supported, stopped" if not exists ${$product_details}{$product};
  return $product_details->{$product}->{issdir};
}

sub GetOutputFileName
{
  my ($this,$product) = @_;
  $product = uc($product);
  die "Product $product not supported, stopped" if not exists ${$product_details}{$product};
  return $product_details->{$product}->{outputfile};
}

# -----------------------------------------------------------------------------------------------------------------------------
# ------------------------ END: Class variables and any associated APIs --------------------------------------------------
# -----------------------------------------------------------------------------------------------------------------------------








# -----------------------------------------------------------------------------------------------------------------------------
# ------------------------ BEGIN: Useful functions ----------------------------------------------------------------------------
# -----------------------------------------------------------------------------------------------------------------------------

sub Log_Do
{ 
  my $cmd = shift;
  print "\n\nDEBUG: EXECUTING ----- $cmd\n\n";
  system($cmd);
}

sub send_mail
{
  my ($to,$subject,$msg,$priority) = @_;
  print "\n\nDEBUG: In send_mail()\n\n";
  if (ref($to) eq "ARRAY") {
	foreach my $to_id (@{$to}) {
		print "\nSending e-mail to id $to_id ...\n";
		unless ($sender->MailMsg({ to => $to_id , subject => $subject , msg => $msg , priority => $priority })) {
			print "\nError while sending mail to $to_id - $Mail::Sender::Error\n";
		}
	}
  } elsif (not ref($to)) {
	print "\n\nSending e-mail to id $to\n\n";
	unless ($sender->MailMsg({ to => $to , subject => $subject , msg => $msg , priority => $priority })) {
		print "\nError while sending mail to $to - $Mail::Sender::Error\n";
	}
  } else {
	$sender->MailMsg({ to => 'inmiet@microsoft.com' , subject => 'Windows build: Usage' , msg => 'Wrong format passed for "to"!' , priority => 1 });
  }
}

sub prepare_signing_files
{
	my ($inputfileslist, $filessection, $inputtempfile, $inputfile, $sourcerootdir,$destrootdir) = @_;
	
	# Open Files.json
	my $filejson;
	{
		local $/; #Enable 'slurp' mode
		open my $fh, "<", "$inputfileslist";
		$filejson = <$fh>;
		close $fh;
	}
	my $filesdata = JSON->new->pretty->utf8->decode($filejson);
	
	# Construct InputSign.json with list of files to be signed.
	my @filelist = @{$filesdata->{$filessection}};		
	my $filestobesigned = "";		
	foreach (@filelist) {
		$filestobesigned = $filestobesigned . "\t\t\t{\n\t\t\t\t\"SourceLocation\" : \"".$_."\"\n\t\t\t},\n";
	}
	
	open(IFILE,"<$inputtempfile") || die "can't open file for read\n"; 
	open(OFILE,">$inputfile") || die "can't open file for writing\n"; 
	while (<IFILE>){
		if(/\"SignRequestFiles\": \[/)
		{
			print OFILE $_;
			print OFILE $filestobesigned;
		}
		elsif(/\"SourceRootDirectory\": \"/)
		{
			print OFILE $sourcerootdir;
		}
		elsif(/\"DestinationRootDirectory\": \"/)
		{
			print OFILE $destrootdir;
		}			
		else 
		{
			print OFILE $_;
		}
	}
	close(IFILE);
	close(OFILE);
}

sub	check_for_signing_errors
{ 
	my ($outputfile) = @_;
	
	open(FILE, "<$outputfile") or die "Unable to open: $outputfile\n";
	while(<FILE>) { 
		if(grep(/statusCode/, $_)) {					
			unless (grep(/pass/, $_)) {
				print $_;
				close(FILE);
				return 1;
			}
		}
	}
	close(FILE);
	return 0;
}

sub submit_files_for_signing
{
	my ($product, $config, $inputfile, $outputfile, $verboselogfile) = @_;
	
	if (Log_Do("$signtool sign -a $authfile -p $policyfile -i $inputfile -o $outputfile -l verbose -f $verboselogfile") == 0){
		if (check_for_signing_errors("$outputfile")){				
			#send_mail($failure_alert_ids,'Windows build failure',"Signing has failed for $product for $config configuration in $addr",1);	
			die "\nSigning has failed for $product. Please refer following files for more details - $outputfile and $verboselogfile.\n";
		}		
		print "\nSigning completed successfully for $product for $config configuration in $addr\n\n";
	}
	else 
	{
		#send_mail($failure_alert_ids,'Windows build failure',"Signing has failed for $product for $config configuration in $addr",1);
		die "\n\nSigning has failed for $product. Please refer following files for more details - $outputfile and $verboselogfile.\n\n";
	}		
}

sub ChangeModifiedTime
{	 
	# Figure out the list of files in the input directory.
	my @dirpath = @_;
	my @files;
	find(sub {
		push @files,$File::Find::name if (-f $File::Find::name);
	}, @dirpath);
	my $nooffiles = @files;
	if ( $nooffiles >= 1)
	{
		print "\nFound $nooffiles files in @dirpath.";
	}
	else
	{
		die "\nDidn't find any files under @dirpath\n";
	}	
	
	# Set modified time of all files 48 hours back.
	my $secsperday = 24*60*60;
	my $modtime = time() - 2 * $secsperday;
	my $ref = File::Touch->new( mtime => $modtime);
	my $filestouched = $ref->touch(@files);
	if ( $nooffiles ==  $filestouched)
	{
		print "\nSuccessfully set modified time for $filestouched files under @dirpath.\n";
	}
	else
	{
		die "\nFailed to set modified time for files unnder @dirpath.\n";
	}
}
	
# -----------------------------------------------------------------------------------------------------------------------------
# ------------------------ END: Useful functions ----------------------------------------------------------------------------
# -----------------------------------------------------------------------------------------------------------------------------



# *******************************************************************************************************************************************



# -----------------------------------------------------------------------------------------------------------------------------
# ----------------------- BEGIN: HELPER FUNCTIONS OF THIS CLASS -----------------------------------------------------------------
# -----------------------------------------------------------------------------------------------------------------------------

sub CreateDatewiseDirStructure
{
  print "DEBUG: in CreateDatewiseDirStructure()\n";
  
  my $this = shift;
  my $date = $this->Date();
  my $month = $this->Month();
  my $year = $this->Year();
  my $outputdir = $this->BuildCopyPath();
  my $configlist = $this->ConfigList();

  my $datewisedir = "${outputdir}\\${date}_${month}_${year}";
  $this->DatewiseDir($datewisedir);
  
  foreach my $config (@{$configlist})
  {
    Log_Do("if not exist ${datewisedir}\\${config}\\logs md ${datewisedir}\\${config}\\logs");
  }
}

sub DoCleanup
{
  print "DEBUG: in DoCleanup()\n";

  my $this = shift;

  my $srcpath = $this->SrcPath();
  my $configlist = $this->ConfigList();
  my $productlist = $this->ProductList();
  my %solnfilehash = ();
  my $temp;
  my $ProductName;
  
  foreach my $product (@{$productlist})
  {
    $temp = $this->GetSolnFileName($product);
    $solnfilehash{$temp} = 1 if not exists $solnfilehash{$temp};
	$ProductName = uc($product)
  }

  foreach my $config (@{$configlist})
  {
    foreach my $solnfile (keys %solnfilehash)
    {
		if (Log_Do("\"$mvss_exe\" ${srcpath}\\$solnfile /clean \"$config|Win32\"") == 0) {
		print "\n\n$solnfile clean successful for configuration $config for $ProductName in $addr\n\n";
		} else {
		send_mail($failure_alert_ids,'Windows build failure',"$solnfile clean failed for configuration $config for $ProductName in $addr",1);
		die "\n\n$solnfile clean failed for configuration $config for $ProductName in $addr";
		}
    }
  }
  
  # Clean up UnifiedAgentMSI project for both x86 and x64 configurations.
  foreach my $prod (@{$productlist})
  {
    if (uc($prod) eq "ASRUA")
    {
    	foreach my $proc ("x86", "x64")
    	{
    		if (Log_Do("\"$mvss_exe\" ${srcpath}\\host\\ASRSetup\\ASRSetup.sln /project UnifiedAgentMSI /clean \"release|$proc\"") == 0) {
    		  print "\n\nClean successful for UnifiedAgentMSI project for configuration release for $prod in $addr\n\n";
    		} else {
    		  send_mail($failure_alert_ids,'Windows build failure',"Clean failed for configuration release for $prod in $addr",1);
    		  die "\n\nClean failed UnifiedAgentMSI for configuration release for $prod in $addr";
    		}
    	}
    }
  }
  
  foreach my $prod (@{$productlist})
  {
    if (uc($prod) eq "PSMSI")
    {
    	foreach my $proc ("x64")
    	{
    		if (Log_Do("\"$mvss_exe\" ${srcpath}\\server\\server.sln /project ProcessServerMSI /clean \"release|$proc\"") == 0) {
    		  print "\n\nClean successful for ProcessServerMSI project for configuration release for $prod in $addr\n\n";
    		} else {
    		  send_mail($failure_alert_ids,'Windows build failure',"Clean failed for configuration release for $prod in $addr",1);
    		  die "\n\nClean failed ProcessServerMSI for configuration release for $prod in $addr";
    		}
    	}
    }
  }
}

sub DoBrandingChanges
{
  print "DEBUG: in DoBrandingChanges()\n";

  my $this = shift;
  my ($srcpath,$Prod_Version,$Intel_Delete_Path,$Win2k_Delete_Path,$partner,$cx_date,$config,$branchname,$bldquality,$bldphase,$MajorVersion,$MinorVersion,$PatchSetVersion,$refsecs,$smrrefsecs,$PatchVersion,$tagstring,$partnerstring,$partnercode,$dlybldnum,$smrdlybldnum,$pvt);
  my ($date,$month,$year,$timeinsecs);
  my $configlist = $this->ConfigList();
  my $productlist = $this->ProductList();

  $srcpath = $this->SrcPath();
  $partner = $this->Partner();
  $branchname = $this->Branch();
  $bldquality = $this->BuildQuality();
  $bldphase = $this->BuildPhase();
  $MajorVersion = $this->GetMajorVersion();
  $MinorVersion = $this->GetMinorVersion();
  $PatchSetVersion = $this->GetPatchSetVersion();
  $refsecs = $this->GetRefSecs();
  $smrrefsecs = $this->GetSMRRefSecs();
  $PatchVersion = $this->GetPatchVersion();
  $partnerstring = $this->GetPartnerString();
  $partnercode = $this->GetPartnerCode();
  
  $pvt = 1;
  
  $date = $this->Date();
  $month = $this->Month();
  $year = $this->Year();
  $timeinsecs = $this->TimeInSecs();
  $Prod_Version="${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}";
  $dlybldnum = int(($timeinsecs - $refsecs)/86400);
  $smrdlybldnum = int(($timeinsecs - $smrrefsecs)/86400);
  
  foreach my $product (@{$productlist})
  { 
	system($PowerShellPath ' -File "delete_files.ps1"' , $branchname);
  }
  $tagstring = "${bldquality}_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_${bldphase}_${dlybldnum}_${month}_${date}_${year}";

  eval {
	
	# Rename them to .orig
	Log_Do("rename $srcpath\\host\\common\\version.h version.h.orig");
	Log_Do("rename $srcpath\\host\\common\\version.cs version.cs.orig");
	Log_Do("rename $srcpath\\server\\tm\\version version.orig");
	
  };
  if ($@ =~ /cannot/) {
	send_mail($failure_alert_ids,'Windows build failure',"Failure in branding\n\n$@",1);
	die "DEBUG: One of the above commands failed. Stopped";
  }

  
  # host\common\version.h

  open(FH_W_VH,">$srcpath\\host\\common\\version.h") or die "Could not open $srcpath\\host\\common\\version.h";
  my $version_h_contents = "#define INMAGE_PRODUCT_VERSION_STR \"${tagstring}\"\n";
  $version_h_contents = $version_h_contents . "#define INMAGE_PRODUCT_VERSION ${MajorVersion},${MinorVersion},${dlybldnum},${pvt}\n";
  $version_h_contents = $version_h_contents . "#define INMAGE_PRODUCT_VERSION_MAJOR ${MajorVersion}\n";
  $version_h_contents = $version_h_contents . "#define INMAGE_PRODUCT_VERSION_MINOR ${MinorVersion}\n";
  $version_h_contents = $version_h_contents . "#define INMAGE_PRODUCT_VERSION_BUILDNUM ${dlybldnum}\n";
  $version_h_contents = $version_h_contents . "#define INMAGE_PRODUCT_VERSION_PRIVATE ${pvt}\n";
  $version_h_contents = $version_h_contents . "#define INMAGE_HOST_AGENT_CONFIG_CAPTION \"${tagstring}\"\n";
  $version_h_contents = $version_h_contents . "#define PROD_VERSION \"${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}\"\n";
  $version_h_contents = $version_h_contents . "#define INMAGE_COPY_RIGHT \"\\xa9 ${year} Microsoft Corp. All rights reserved.\"\n";
  $version_h_contents = $version_h_contents . "#define INMAGE_PRODUCT_NAME \"Microsoft Azure Site Recovery\"\n";
  print FH_W_VH $version_h_contents;
  close(FH_W_VH);

  # host\common\version.cs
  open(FH_W_VCS,">$srcpath\\host\\common\\version.cs") or die "Could not open $srcpath\\host\\common\\version.cs";
  my $version_cs_contents = "using System.Reflection\;\n";
  $version_cs_contents = $version_cs_contents . "\n";
  $version_cs_contents = $version_cs_contents . "[assembly: AssemblyFileVersion(\"${MajorVersion}.${MinorVersion}.${dlybldnum}.${pvt}\")]\n";
  $version_cs_contents = $version_cs_contents . "[assembly: AssemblyCompany(\"Microsoft Corporation\")]\n";
  $version_cs_contents = $version_cs_contents . "[assembly: AssemblyProduct(\"Microsoft Azure Site Recovery\")]\n";
  $version_cs_contents = $version_cs_contents . "[assembly: AssemblyInformationalVersion(\"${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}\")]\n";
  $version_cs_contents = $version_cs_contents . "[assembly: AssemblyCopyright(\"\\u00A9 ${year} Microsoft Corp. All rights reserved.\")]\n";
  $version_cs_contents = $version_cs_contents . "[assembly: AssemblyTrademark(\"Microsoft\\u00AE is a registered trademark of Microsoft Corporation.\")]\n";
  print FH_W_VCS $version_cs_contents;
  close(FH_W_VCS);

  # server\tm\version

  open(FH_W_V,">$srcpath\\server\\tm\\version") or die "Could not open $srcpath\\server\\tm\\version";
  my $version_contents = "$tagstring\n${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_${bldphase}_${dlybldnum}\nVERSION=${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}\nPROD_VERSION=${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}";
  print FH_W_V $version_contents;
  close(FH_W_V);
  
  open(FH_W_V,">$srcpath\\host\\ASRSetup\\UnifiedAgentMSI\\version.wxi") or die "Could not open $srcpath\\host\\ASRSetup\\UnifiedAgentMSI\\version.wxi";
  my $version_wxi_contents = "<Include>\n";
  $version_wxi_contents = $version_wxi_contents . "<?define BuildVersion = \"${MajorVersion}.${MinorVersion}.${dlybldnum}.${pvt}\" ?>\n";
  $version_wxi_contents = $version_wxi_contents . "<?define ProductVersion = \"${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}\" ?>\n";
  $version_wxi_contents = $version_wxi_contents . "<?define BuildPhase = \"${bldphase}\" ?>\n";
  $version_wxi_contents = $version_wxi_contents . "<?define BuildTag = \"${tagstring}\" ?>\n";
  $version_wxi_contents = $version_wxi_contents . "<?define BuildNumber = \"${dlybldnum}\" ?>\n";
  $version_wxi_contents = $version_wxi_contents . "<?define CompanyName = \"Microsoft Corporation\" ?>\n";
  $version_wxi_contents = $version_wxi_contents . "</Include>\n";
  print FH_W_V $version_wxi_contents;
  close(FH_W_V);
  
  open(FH_W_V,">$srcpath\\server\\windows\\ProcessServerMSI\\Version.wxi") or die "Could not open $srcpath\\server\\windows\\ProcessServerMSI\\Version.wxi";
  my $version_pswxi_contents = "<Include>\n";
  $version_pswxi_contents = $version_pswxi_contents . "<?define BuildVersion = \"${MajorVersion}.${MinorVersion}.${dlybldnum}.${pvt}\" ?>\n";
  $version_pswxi_contents = $version_pswxi_contents . "<?define ProductVersion = \"${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}\" ?>\n";
  $version_pswxi_contents = $version_pswxi_contents . "<?define BuildPhase = \"${bldphase}\" ?>\n";
  $version_pswxi_contents = $version_pswxi_contents . "<?define BuildTag = \"${tagstring}\" ?>\n";
  $version_pswxi_contents = $version_pswxi_contents . "<?define BuildNumber = \"${dlybldnum}\" ?>\n";
  $version_pswxi_contents = $version_pswxi_contents . "<?define CompanyName = \"Microsoft Corporation\" ?>\n";
  $version_pswxi_contents = $version_pswxi_contents . "</Include>\n";
  print FH_W_V $version_pswxi_contents;
  close(FH_W_V);
  
  print "version.h contents are:\n\n$version_h_contents\n\n";
  print "version.cs contents are:\n\n$version_cs_contents\n\n";
  print "version contents are:\n\n$version_contents\n\n";
  print "version.wxi contents are:\n\n$version_wxi_contents\n\n";
  print "version.wxi contents for PSMSI are:\n\n$version_pswxi_contents\n\n";
    
  
}

sub DoProductBuilds
{
  print "DEBUG: in DoProductBuilds()\n";
  my $this = shift;
  my $srcpath = $this->SrcPath();
  my $partner = $this->Partner();
  my $configlist = $this->ConfigList();
  my $productlist = $this->ProductList();
  my $datewisedir = $this->DatewiseDir();
  my $branchname = $this->Branch();
  my $MajorVersion = $this->GetMajorVersion();
  my $MinorVersion = $this->GetMinorVersion();
  my $buildversion = "${MajorVersion}.${MinorVersion}";
  my $pwd = $this->GetThisScriptDir();

  my ($solnfile,$project,$buildoutputdir,$logfile,$host_solnfile,$vacp_bldlogfile,$Log_bldlogfile,$hostagenthelpers_bldlogfile);
  
  foreach my $config (reverse sort @{$configlist})
  {
	my $draReturnCode = system($PowerShellPath ' -File "Copy_DRA_Binaries.ps1"', $branchname, $config );
  $draReturnCode = $draReturnCode >> 8;
    if ( $draReturnCode == 1 )
    {
        die "Failed executing Copy_DRA_Binaries.ps1.\n";
    }
	system($PowerShellPath ' -File "Copy_Vista_Drivers.ps1"', $branchname, $buildversion, $config );

	chdir "${srcpath}\\host";
	if (Log_Do("\"$msbuild_exe\" /t:restore /p:RestorePackagesConfig=true /p:Configuration=Release /p:Platform=Win32") != 0) {
		die "Failed to restore nuget pacakges for host.\n";
	}

    chdir "${srcpath}\\server";
	if (Log_Do("\"$msbuild_exe\" /t:restore /p:RestorePackagesConfig=true /p:Configuration=Release /p:Platform=x64") != 0) {
		die "Failed to restore nuget pacakges for server.\n";
	}
	chdir "$pwd";

	if (Log_Do("\"$mvss_exe\" ${srcpath}\\thirdparty\\sqlite3x\\sqlite3x.sln /project sqlite3x /build \"Release|Win32\" /out $logfile") != 0) {
		die "Failed to build Win32 sqlite3x.\n";
	}

	if (Log_Do("\"$mvss_exe\" ${srcpath}\\thirdparty\\sqlite3x\\sqlite3x.sln /project sqlite3x /build \"Release|X64\" /out $logfile") != 0) {
		die "Failed to build X64 sqlite3x.\n";
	}
	
	system("mkdir ${srcpath}\\host\\packages\\sqlite3x\\lib\\win32\\release\\");
	system("mkdir ${srcpath}\\host\\packages\\sqlite3x\\lib\\x64\\release\\");
	
	# workaround to generate the sqlite3x builds and copy to packages. this should be avoided by referring the libs from build path
	Log_Do("copy /Y ${srcpath}\\thirdparty\\sqlite3x\\sqlite3x\\lib\\win32\\Release\\sqlite3x.lib ${srcpath}\\host\\packages\\sqlite3x\\lib\\win32\\release\\");
	Log_Do("copy /Y ${srcpath}\\thirdparty\\sqlite3x\\sqlite3x\\lib\\win32\\Release\\sqlite3x.pdb ${srcpath}\\host\\packages\\sqlite3x\\lib\\win32\\release\\");
	Log_Do("copy /Y ${srcpath}\\thirdparty\\sqlite3x\\sqlite3x\\lib\\x64\\Release\\sqlite3x.lib ${srcpath}\\host\\packages\\sqlite3x\\lib\\x64\\release\\");
	Log_Do("copy /Y ${srcpath}\\thirdparty\\sqlite3x\\sqlite3x\\lib\\x64\\Release\\sqlite3x.pdb ${srcpath}\\host\\packages\\sqlite3x\\lib\\x64\\release\\");

    foreach my $product (@{$productlist})
    {
	$project = $this->GetBuildProjectName($product);
	$buildoutputdir = $this->GetBuildIssDir($product);
	$logfile = "${srcpath}\\${buildoutputdir}\\BuildLog_${product}_${config}_${partner}.txt";
	$solnfile = $this->GetSolnFileName($product);
	unlink "$logfile" if -e "$logfile";
	
	if (uc($product) eq "CX")
	{
		foreach my $projectname ("CSAuthModule")
		{
			if (Log_Do("\"$mvss_exe\" ${srcpath}\\$solnfile /project $projectname /clean \"$config|x64\" /out $logfile") == 0) {
			print "\n\n $projectname project clean successful for configuration $config for $product in $addr\n\n";
			} else {
			send_mail($failure_alert_ids,'Windows build failure',"$projectname project clean failed for configuration $config for $product in $addr",1);
			die "\n\n $projectname project clean failed for configuration $config for $product in $addr";
			}
			if (Log_Do("\"$mvss_exe\" ${srcpath}\\$solnfile /project $projectname /build \"$config|x64\" /out $logfile") == 0 ) {
				Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
				print "\n\n$config build from $branchname is successful for $product for $projectname in $addr\n\n";
			} else {
				Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
				send_mail($failure_alert_ids,'Windows build failure',"$config build from $branchname is failed for $product for $projectname in $addr",1);
				die "\n\n$config build from $branchname is failed for $product for $projectname in $addr";
			}
		}
		
	}
	
	if (uc($product) ne "PSMSI")
	{
		# Main product build
	if (Log_Do("\"$mvss_exe\" ${srcpath}\\$solnfile /project $project /build \"$config|Win32\" /out $logfile") == 0 ) {
		Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
		Log_Do("if exist $vacp_bldlogfile copy /Y $vacp_bldlogfile ${datewisedir}\\${config}\\logs");
		Log_Do("if exist $Log_bldlogfile copy /Y $Log_bldlogfile ${datewisedir}\\${config}\\logs");
		Log_Do("if exist $hostagenthelpers_bldlogfile copy /Y $hostagenthelpers_bldlogfile ${datewisedir}\\${config}\\logs");
		print "\n\n$config build from $branchname is successful for $product in $addr\n\n";			
	} else {
		Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
		Log_Do("if exist $vacp_bldlogfile copy /Y $vacp_bldlogfile ${datewisedir}\\${config}\\logs");
		Log_Do("if exist $Log_bldlogfile copy /Y $Log_bldlogfile ${datewisedir}\\${config}\\logs");
		Log_Do("if exist $hostagenthelpers_bldlogfile copy /Y $hostagenthelpers_bldlogfile ${datewisedir}\\${config}\\logs");
		send_mail($failure_alert_ids,'Windows build failure',"$config build from $branchname is failed for $product in $addr",1);
		die "\n\n$config build from $branchname is failed for $product in $addr";
	}
	}
	

	if (uc($product) eq "UA") {
			
			foreach my $projectname ("DiskFlt")
			{
				foreach my $configuration ("Win7", "Win8", "Win8.1")
				{
					Log_Do("\"$mvss_exe\" ${srcpath}\\$solnfile /project $projectname /clean \"$configuration $config|Win32\" /out $logfile");
					if (Log_Do("\"$mvss_exe\" ${srcpath}\\$solnfile /project $projectname /build \"$configuration $config|Win32\" /out $logfile") == 0 ) {
						Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
						print "\n\n$config build from $branchname is successful for $product for $projectname in $addr\n\n";
					} else {
						Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
						send_mail($failure_alert_ids,'Windows build failure',"$config build from $branchname is failed for $product for $projectname in $addr",1);
					}
					
					Log_Do("\"$mvss_exe\" ${srcpath}\\$solnfile /project $projectname /clean \"$configuration $config|x64\" /out $logfile");
					if (Log_Do("\"$mvss_exe\" ${srcpath}\\$solnfile /project $projectname /build \"$configuration $config|x64\" /out $logfile") == 0 ) {
						Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
						print "\n\n$config build from $branchname is successful for $product for $projectname in $addr\n\n";
					} else {
						Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
						send_mail($failure_alert_ids,'Windows build failure',"$config build from $branchname is failed for $product for $projectname in $addr",1);
					}
				}
			}
			
			foreach my $projectname ("cxps", "vacp", "InMageVssProvider")
			{
				if (Log_Do("\"$mvss_exe\" ${srcpath}\\$solnfile /project $projectname /clean \"$config|x64\" /out $logfile") == 0) {
                  print "\n\n $projectname project clean successful for configuration $config for $product in $addr\n\n";
                } else {
                  send_mail($failure_alert_ids,'Windows build failure',"$projectname project clean failed for configuration $config for $product in $addr",1);
                  die "\n\n $projectname project clean failed for configuration $config for $product in $addr";
                }
				if (Log_Do("\"$mvss_exe\" ${srcpath}\\$solnfile /project $projectname /build \"$config|x64\" /out $logfile") == 0 ) {
						Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
						print "\n\n$config build from $branchname is successful for $product for $projectname in $addr\n\n";
					} else {
						Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
						send_mail($failure_alert_ids,'Windows build failure',"$config build from $branchname is failed for $product for $projectname in $addr",1);
						die "\n\n$config build from $branchname is failed for $product for $projectname in $addr";
				}
	}
	
			my $ComponentDriverDITestsolnfile = 'ComponentDriverDITest.sln';
			Log_Do("\"$mvss_exe\" ${srcpath}\\host\\tests\\$ComponentDriverDITestsolnfile /clean \"$config|Win32\" /out $logfile");
			if (Log_Do("\"$mvss_exe\" ${srcpath}\\host\\tests\\$ComponentDriverDITestsolnfile /build \"$config|Win32\" /out $logfile") == 0 ) {
				Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
				print "\n\n$config build from $branchname is successful for $product for $ComponentDriverDITestsolnfile in $addr\n\n";
			} else {
				Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
				send_mail($failure_alert_ids,'Windows build failure',"$config build from $branchname is failed for $product for $ComponentDriverDITestsolnfile in $addr",1);
				die "\n\n$config build from $branchname is failed for $product for $ComponentDriverDITestsolnfile in $addr";
			}
			my $DITestsolnfile = 'DITests.sln';
			Log_Do("\"$mvss_exe\" ${srcpath}\\host\\tests\\DITests\\$DITestsolnfile /clean \"$config|Win32\" /out $logfile");
			if (Log_Do("\"$mvss_exe\" ${srcpath}\\host\\tests\\DITests\\$DITestsolnfile /build \"$config|Win32\" /out $logfile") == 0 ) {
				Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
				print "\n\n$config build from $branchname is successful for $product for $DITestsolnfile in $addr\n\n";
			} else {
				Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
				send_mail($failure_alert_ids,'Windows build failure',"$config build from $branchname is failed for $product for $DITestsolnfile in $addr",1);
				die "\n\n$config build from $branchname is failed for $product for $DITestsolnfile in $addr";
			}
    }
  }
}
}
sub DoDriverSigning
{
  print "DEBUG: in DoDriverSigning()\n";
  my $this = shift;
  my $srcpath = $this->SrcPath();
  my $partner = $this->Partner();
  my $configlist = $this->ConfigList();
  my $productlist = $this->ProductList();
  my $datewisedir = $this->DatewiseDir();
  my $branchname = $this->Branch();
  my $signing = $this->Signing();

  my ($buildoutputdir,$logfile);
  
  foreach my $config (reverse sort @{$configlist})
  {
    foreach my $product (@{$productlist})
    {
	
 	next if uc($product) ne "UA" and uc($product) ne "CX" and uc($product) ne "PI" and uc($product) ne "ASRUA" and uc($product) ne "ASRSETUP";

	$buildoutputdir = $this->GetBuildIssDir($product);
	$logfile = "${srcpath}\\${buildoutputdir}\\DriverSigningLog_${product}_${config}_${partner}.txt";
	my $bldlogfile = "${srcpath}\\${buildoutputdir}\\BuildLog_${product}_${config}_${partner}.txt";
	my $solnfile = $this->GetSolnFileName($product);
	unlink "$logfile" if -e "$logfile";
    	
	my ($weekday,$month,$sdate,$hour,$min,$sec,$year) = ( localtime =~ /^(\w{3})\s+(\w{3})\s+(\d{1,2})\s+(\d{2}):(\d{2}):(\d{2})\s+(\d{4})$/ );
	$sdate = sprintf("%02d", $sdate);
	my $sdate = "${sdate}_${month}_${year}";
	my $product = uc($product);
	
	# Delete signing related files if they exists.	
	my @signfilestoberemoved = ("InputSign.json", "OutputSign.json", "OutputSignVerbose.log");
	foreach (@signfilestoberemoved) {
		Log_Do("if exist I:\\Signing\\$branchname\\$sdate\\$product\\$_ del /F I:\\Signing\\$branchname\\$sdate\\$product\\$_");
	}	

	# Variables specific to signing.
	my $inputfileslist = "$srcpath\\build\\scripts\\automation\\CodeSignScripts\\Files.json";
	my $sourcerootdir = "\t\t\"SourceRootDirectory\":" . " \"I:\\\\Signing\\\\$branchname\\\\$sdate\\\\$product\\\\Unsigned\\\\$config\",\n";
	my $destrootdir = "\t\t\"DestinationRootDirectory\":" . " \"I:\\\\Signing\\\\$branchname\\\\$sdate\\\\$product\\\\Signed\\\\$config\",\n";
	my $inputtempfile = "$srcpath\\build\\scripts\\automation\\CodeSignScripts\\InputSignTemplate.json";
	my $inputfile = "I:\\Signing\\$branchname\\$sdate\\$product\\InputSign.json";
	my $outputfile = "I:\\Signing\\$branchname\\$sdate\\$product\\OutputSign.json";
	my $verboselogfile = "I:\\Signing\\$branchname\\$sdate\\$product\\OutputSignVerbose.log";
	
    if (uc($product) eq "UA") 
	{		
		# Copy unsigned binaries/files to submission path.
		system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Unsinged_Binaries_To_Submission_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);
		
		# Prepare files for signing.
		my $filessection = "UA_SIGN_FILES";
		prepare_signing_files ("$inputfileslist", "$filessection", "$inputtempfile", "$inputfile", "$sourcerootdir", "$destrootdir");

		# Submit binaries/files for signing.
		submit_files_for_signing("$product", "$config", "$inputfile", "$outputfile", "$verboselogfile");
		
		# Copy signed binaries/files back to build path.
		system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Signed_Binaries_To_Build_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);	
	}

	if (uc($product) eq "ASRUA") 
	{
		# Copy managed binaries to submission path.
		system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Unsinged_Binaries_To_Submission_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);

		# Prepare files for signing.
		my $filessection = "ASRUA_SIGN_FILES";
		prepare_signing_files ("$inputfileslist", "$filessection", "$inputtempfile", "$inputfile", "$sourcerootdir", "$destrootdir");
	
		# Submit binaries/files for signing.
		submit_files_for_signing("$product", "$config", "$inputfile", "$outputfile", "$verboselogfile");
		
		# Copy managed binaries to build path.
		system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Signed_Binaries_To_Build_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);	

		if (Log_Do("\"$msbuild_exe\" ${srcpath}\\$solnfile /t:UnifiedAgentCustomActions:Rebuild /p:Configuration=$config /p:Platform=\"Any CPU\" /p:Signing=True") == 0 ) {
			Log_Do("if exist $bldlogfile copy /Y $bldlogfile ${datewisedir}\\${config}\\logs");
			print "\n\n$config build from $branchname is successful for $product for UnifiedAgentCustomActions in $addr\n\n";
		}
		else 
		{
			Log_Do("if exist $bldlogfile copy /Y $bldlogfile ${datewisedir}\\${config}\\logs");
			send_mail($failure_alert_ids,'Windows build failure',"$config build from $branchname is failed for $product for UnifiedAgentCustomActions in $addr",1);
			die "\n\n$config build from $branchname is failed for $product for UnifiedAgentCustomActions in $addr";
		}
	}

	if (uc($product) eq "ASRSETUP") 
	{
		# Copy managed binaries to submission path.
		system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Unsinged_Binaries_To_Submission_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);

		# Prepare files for signing.
		my $filessection = "ASRUS_SIGN_FILES";
		prepare_signing_files ("$inputfileslist", "$filessection", "$inputtempfile", "$inputfile", "$sourcerootdir", "$destrootdir");
	
		# Submit binaries/files for signing.
		submit_files_for_signing("$product", "$config", "$inputfile", "$outputfile", "$verboselogfile");
		
		# Copy managed binaries to build path.
		system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Signed_Binaries_To_Build_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);			
	}
	
	if (uc($product) eq "PI") 
	{
		# Copy unsigned binaries to submission path.
		system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Unsinged_Binaries_To_Submission_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);
		
		# Prepare files for signing.
		my $filessection = "PI_SIGN_FILES";
		prepare_signing_files ("$inputfileslist", "$filessection", "$inputtempfile", "$inputfile", "$sourcerootdir", "$destrootdir");
		
		# Submit binaries/files for signing.
		submit_files_for_signing("$product", "$config", "$inputfile", "$outputfile", "$verboselogfile");
		
		# Copy signed binaries to build path.
		system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Signed_Binaries_To_Build_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);			
	}
	
	if (uc($product) eq "CX") 
	{	
		# Copy binaries/files to submission path.
		system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Unsinged_Binaries_To_Submission_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);
		
		# Prepare files for signing.
		my $filessection = "CX_SIGN_FILES";
		prepare_signing_files ("$inputfileslist", "$filessection", "$inputtempfile", "$inputfile", "$sourcerootdir", "$destrootdir");

		# Submit binaries/files for signing.
		submit_files_for_signing("$product", "$config", "$inputfile", "$outputfile", "$verboselogfile");
	
		# Copy signed binaries/files to build path.
		system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Signed_Binaries_To_Build_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);
	}	
	else
	{
		next;
	}
 	Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
    }
  }
}


sub DoGetPDBList
{
  print "DEBUG: in DoGetPDBList()\n";
  my $this = shift;
  my $config = shift;
  my $product = shift;
  my $srcpath = $this->SrcPath();
  my $pwd = $this->GetThisScriptDir();
  my $pdblistfilename = $this->GetProductPDBListFileName($product);
  my $branchname = $this->Branch();
  my @pdbfilelist = ();
  print "\n\n$pwd $pdblistfilename values \n\n";
  open my $fh,"<${pwd}\\${pdblistfilename}" or die "Could not open pdb list file ${pwd}\\${pdblistfilename} in $addr for $branchname, stopped";
  while (<$fh>)
  {
    chomp;
    next if /^\s*\t*$/;

    if (/\\CONFIG\\/) {

        if (/\\SPECIAL_NAME/) {
	}

	s/CONFIG/ucfirst $config/e;
	push @pdbfilelist, "$_";

    } elsif (/\\CONFIG_DEBUG_ONLY\\/) {

	next if $config eq "release";
	s/CONFIG_DEBUG_ONLY/Debug/;
	push @pdbfilelist, "$_";

    } elsif (/\\CONFIG_RELEASE_IS_SPECIAL\\/) {

	next if $config eq "debug";
	s/CONFIG_RELEASE_IS_SPECIAL/ReleaseMinDependency/;
	push @pdbfilelist, "$_";

    } elsif (/\\DRIVERCONFIG_/) {

	if ($config eq "debug") {
	  s/DRIVERCONFIG/objchk/;
	} else {
	  s/DRIVERCONFIG/objfre/;
	}

	push @pdbfilelist, "$_";

    } else {
	push @pdbfilelist, "$_";
    }
  }

  return @pdbfilelist;
}

sub DoCreatePDBZip
{
  print "DEBUG: in DoCreatePDBZip()\n";
  my $this = shift;
  my $srcpath = $this->SrcPath();
  my $partner = $this->Partner();
  my $configlist = $this->ConfigList();
  my $productlist = $this->ProductList();
  my $datewisedir = $this->DatewiseDir();
  my $partnerstring = $this->GetPartnerString();
  my $MajorVersion = $this->GetMajorVersion();
  my $MinorVersion = $this->GetMinorVersion();
  my $PatchSetVersion = $this->GetPatchSetVersion();
  my $PatchVersion = $this->GetPatchVersion();
  my $branchname = $this->Branch();
  my $date = $this->Date();
  my $month = $this->Month();
  my $year = $this->Year();
  my $pwd = $this->GetThisScriptDir();
  my $buildphase = $this->BuildPhase();
  my @pdbfilelist = ();

  my ($buildoutputdir,$logfile,$pdbzipfilebasename,$pdbzipfilefullpath,$pdbzipdirfullpath);
  
  chdir "$srcpath";
  foreach my $config (reverse sort @{$configlist})
  {
    foreach my $product (@{$productlist})
    {
	    next if $product eq "cx_tp" ;
	    @pdbfilelist = $this->DoGetPDBList($config,$product);

	    $buildoutputdir = $this->GetBuildIssDir($product);
	    $logfile = "${srcpath}\\${buildoutputdir}\\PDBZipLog_${product}_${config}_${partner}.txt";
		$pdbzipfilebasename = "${partnerstring}_" . uc($product) . "_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_Windows_${buildphase}_${date}${month}${year}_${config}_symbols";
	    $pdbzipdirfullpath = "${srcpath}\\${buildoutputdir}\\$pdbzipfilebasename";
		$pdbzipfilefullpath = "${srcpath}\\${buildoutputdir}\\$pdbzipfilebasename.zip";
	    unlink "$logfile" if -e "$logfile";
		
		if (rmtree $pdbzipdirfullpath) {
		print "\nSuccessfully deleted $pdbzipdirfullpath";
		}
		unlink "$pdbzipfilefullpath";
		Log_Do("if not exist $pdbzipdirfullpath md $pdbzipdirfullpath");
	    
		foreach (@pdbfilelist) {
		my $lastIndex = rindex ($_, '\\');
		my $first = substr($_,0,$lastIndex);
		if (Log_Do("xcopy /Y /F $_ $pdbzipdirfullpath\\$first\ >> $logfile 2>&1") != 0) {
		    print "FAILURE: $_ could not be added to $pdbzipdirfullpath for $branchname in $addr\n";
		}
	    }
		
		if (Log_Do("\"$zip_exe\" a -mx5 $pdbzipfilefullpath $pdbzipdirfullpath\\* >> $logfile 2>&1") != 0) {
		    print "FAILURE: could not create $pdbzipfilefullpath for $branchname in $addr\n";
		}

	    if (Log_Do("\"$zip_exe\" t $pdbzipfilefullpath >> $logfile 2>&1") == 0) {
		Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
		Log_Do("if exist $pdbzipfilefullpath copy /Y $pdbzipfilefullpath ${datewisedir}\\${config}");
		print "\n\n$pdbzipfilefullpath has been tested for its integrity and it passed...\n\n";
	    } else {
		Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
		Log_Do("if exist $pdbzipfilefullpath rename $pdbzipfilefullpath ${pdbzipfilebasename}.checkfailed");
		print "\n\n$pdbzipfilefullpath has been tested for its integrity and it FAILED...\n\n";
		send_mail($failure_alert_ids,'Windows build failure',"$pdbzipfilefullpath has been tested for its integrity and it failed for $branchname in $addr",1);
		die "\n\n$pdbzipfilefullpath has been renamed to ${pdbzipfilebasename} for $branchname in $addr.checkfailed ...\n\n";
	    }
		
		if (rmtree $pdbzipdirfullpath) {
		print "\nSuccessfully deleted $pdbzipdirfullpath";
		}
    }
  }
  chdir "$pwd";
}

sub DoCheckSymbolMatching
{
  print "DEBUG: in DoCheckSymbolMatching()\n";
  my $this = shift;
  my $srcpath = $this->SrcPath();
  my $partner = $this->Partner();
  my $symserverpath = $this->SymbolStorePath();
  my $configlist = $this->ConfigList();
  my $productlist = $this->ProductList();
  my $datewisedir = $this->DatewiseDir();
  my $pwd = $this->GetThisScriptDir();
  my $branchname = $this->Branch();
  my @pdbfilelist = ();

  my ($buildoutputdir,$logfile);
  
  chdir "$srcpath";
  foreach my $config (reverse sort @{$configlist})
  {
    foreach my $product (@{$productlist})
    {
	    next if $product eq "cx_tp" ;

	    @pdbfilelist = $this->DoGetPDBList($config,$product);

	    $buildoutputdir = $this->GetBuildIssDir($product);
	    $logfile = "${srcpath}\\${buildoutputdir}\\SymbolMatchingLog_${product}_${config}_${partner}.txt";
	    unlink "$logfile" if -e "$logfile";

	    foreach (@pdbfilelist) {

		# Replace .pdb in pdb file name with .exe/.sys to get executable's name
		# This makes sense because we have to check symbol matching only for those executable that have pdb
		if (/(InDskFlt)[.]pdb$/) {
		    s/[.]pdb$/.sys/;
		} elsif (/(VimClient)[.]pdb$/) {
		    s/[.]pdb$/.dll/;
		} elsif (/(CSAuthModule)[.]pdb$/) {
		    s/[.]pdb$/.dll/;
		} elsif (/(inmmessage)[.]pdb$/) {
		    s/[.]pdb$/.dll/;
		} elsif (/(configtool)[.]pdb$/) {
		    s/[.]pdb$/.dll/;
		} elsif (/(httpclient)[.]pdb$/) {
		    s/[.]pdb$/.dll/;
		} elsif (/(InMageAPILibrary)[.]pdb$/) {
		    s/[.]pdb$/.dll/;
		} elsif (/(UnifiedAgentCustomActions)[.]pdb$/) {
		    s/[.]pdb$/.dll/;
		} elsif (/(ProcessServerCustomActions)[.]pdb$/) {
		    s/[.]pdb$/.dll/;
		} elsif (/(diskgeneric)[.]pdb$/) {
		    s/[.]pdb$/.dll/;
		} elsif (/(InMageVssProvider)[.]pdb$/) {
		    s/[.]pdb$/.dll/;
		} elsif (/(InMageVssProvider_X64)[.]pdb$/) {
		    s/[.]pdb$/.dll/;
		} elsif (/(ASRResources)[.]pdb$/) {
		    s/[.]pdb$/.dll/;
		} elsif (/(ASRSetupFramework)[.]pdb$/) {
		    s/[.]pdb$/.dll/;
		} else {
		    s/[.]pdb$/.exe/;
		}

		if (Log_Do("\"$pdbcheck_exe\" /v $_ /s srv*C:\\symbols\\$branchname*http://localhost/symserv/$branchname/ >> $logfile 2>&1") != 0) {
			my $failure_msg = "FAILURE: $_ could not find a symbol in the symbol server! Symbol matching failed from $branchname for product ${product} and configuration ${config} for company ${partner} in $addr ...";
			print "${failure_msg}\n";
			send_mail($failure_alert_ids,'Windows build failure',"$failure_msg",1);
			Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
			die "${failure_msg}\n";
		}
	    }

	    Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");

    }
  }
  chdir "$pwd";
}

sub DoLoadPDBToServer
{
  print "DEBUG: in DoLoadPDBToServer()\n";

  my $this = shift;
  return if $this->ShouldLoadSymbols() eq "no";
 
  my $srcpath = $this->SrcPath();
  my $symserverpath = $this->SymbolStorePath();
  my $partner = $this->Partner();
  my $configlist = $this->ConfigList();
  my $productlist = $this->ProductList();
  my $datewisedir = $this->DatewiseDir();
  my $MajorVersion = $this->GetMajorVersion();
  my $MinorVersion = $this->GetMinorVersion();
  my $branchname = $this->Branch();
  my $PatchSetVersion = $this->GetPatchSetVersion();
  my $PatchVersion = $this->GetPatchVersion();
  my @pdbfilelist = ();

  my ($buildoutputdir,$logfile);
  
  foreach my $config (reverse sort @{$configlist})
  {
    foreach my $product (@{$productlist})
    {
	    next if $product eq "cx_tp" ;

	    @pdbfilelist = $this->DoGetPDBList($config,$product);

	    $buildoutputdir = $this->GetBuildIssDir($product);
	    $logfile = "${srcpath}\\${buildoutputdir}\\SymbolLoadLog_${product}_${config}_${partner}.txt";
	    unlink "$logfile" if -e "$logfile";

	    foreach (@pdbfilelist) {
		my $pdbfullname = "${srcpath}\\$_";
		if (Log_Do("\"$pdbload_exe\" add /f $pdbfullname /s $symserverpath /t \"DrScout${MajorVersion}\.${MinorVersion}\.${PatchSetVersion}\.${PatchVersion}\" >> $logfile 2>&1") == 0) {
		    print "$pdbfullname has been successfully loaded to symbol server at path $symserverpath\n";
		} else {
		    Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
		    send_mail($failure_alert_ids,'Windows build failure',"$pdbfullname could not be successfully loaded to symbol server",1);
		    die "$pdbfullname could not be successfully loaded to symbol server for $branchname in $addr, stopped";
		}
	    }

	    Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
    }
  }
  
}

sub DoProductPackaging
{
  print "DEBUG: in DoProductPackaging()\n";

  my $this = shift;
  my $srcpath = $this->SrcPath();
  my $partner = $this->Partner();
  my $partnerstring = $this->GetPartnerString();
  my $configlist = $this->ConfigList();
  my $productlist = $this->ProductList();
  my $datewisedir = $this->DatewiseDir();
  my $MajorVersion = $this->GetMajorVersion();
  my $MinorVersion = $this->GetMinorVersion();
  my $PatchSetVersion = $this->GetPatchSetVersion();
  my $PatchVersion = $this->GetPatchVersion();
  my $branchname = $this->Branch();
  my $partnercode = $this->GetPartnerCode();
  my $bldphase = $this->BuildPhase();
  my $pvt = 1;
  my $date = $this->Date();
  my $month = $this->Month();
  my $year = $this->Year();
  my $buildphase = $this->BuildPhase();
  my $signing = $this->Signing();
  my $buildpath = "${MajorVersion}.${MinorVersion}";

  my ($outputfilename,$targetfilename,$solnfile,$buildproject,$packagingproject,$packagingoutputdir,$logfile);
  
  foreach my $config (reverse sort @{$configlist})
  {
    foreach my $product (@{$productlist})
    {	
		# Skip packaging if the product is UA/CX/PI as we don't need to generate installers for them.
		next if uc($product) eq "UA" or uc($product) eq "CX";
		
		$buildproject = $this->GetBuildProjectName($product);
		$packagingproject = $this->GetPackagingProjectName($product);
		$packagingoutputdir = $this->GetBuildIssDir($product);
		$logfile = "${srcpath}\\${packagingoutputdir}\\PackagingLog_${product}_${config}_${partner}.txt";
		$solnfile = $this->GetSolnFileName($product);
		unlink "$logfile" if -e "$logfile";	
		my $product = uc($product);
		
		# Populate version.iss file when product is CX_TP or UCX.
		if (uc($product) eq "CX_TP" or uc($product) eq "UCX")
		{
			Log_Do("rename $srcpath\\host\\setup\\version.iss version.iss.orig");
			my $productname = uc($product);
			my $timeinsecs = $this->TimeInSecs();
			my $refsecs = $this->GetRefSecs();
			my $smrrefsecs = $this->GetSMRRefSecs();
			my $dlybldnum = int(($timeinsecs - $refsecs)/86400);
			my $smrdlybldnum = int(($timeinsecs - $smrrefsecs)/86400);
			my $bldquality = $this->BuildQuality();
			my $tagstring = "${bldquality}_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_${bldphase}_${dlybldnum}_${month}_${date}_${year}_${partnerstring}";
			open(FH_W_V_ISS,">$srcpath\\host\\setup\\version.iss") or die "Could not open $srcpath\\host\\setup\\version.iss";
			my $version_iss_contents = "#define VERSION \"${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}\"\n";
			$version_iss_contents = $version_iss_contents . "#define PRODUCTVERSION \"${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}\"\n";
			$version_iss_contents = $version_iss_contents . "#define APPVERSION \"${MajorVersion}.${MinorVersion}.${dlybldnum}.${partnercode}\"\n";
			$version_iss_contents = $version_iss_contents . "#define PRODUCTNAME \"${productname}\"\n";
			$version_iss_contents = $version_iss_contents . "#define BUILDTAG \"${tagstring}\"\n";
			$version_iss_contents = $version_iss_contents . "#define BUILDNUMBER \"${dlybldnum}\"\n";
			$version_iss_contents = $version_iss_contents . "#define BUILDPHASE  \"${bldphase}\"\n";
			$version_iss_contents = $version_iss_contents . "#define PARTNERCODE \"${partnercode}\"\n";
			$version_iss_contents = $version_iss_contents . "#define BUILDDATE \"${date}${month}${year}\"\n";
			$version_iss_contents = $version_iss_contents . "#define SMRBUILDNUMBER \"${smrdlybldnum}\"\n";
			$version_iss_contents = $version_iss_contents . "#define COPYRIGHT \"(c) ${year} Microsoft Corp. All rights reserved.\"\n";
			print FH_W_V_ISS $version_iss_contents;
			close(FH_W_V_ISS);
			print "version.iss contents are:\n\n$version_iss_contents\n\n";
	    }

		# Populate manifest.txt and copy Linux agents, push clients, DRA and MARS agents when product is UCX.
		if (uc($product) eq "UCX")
		{		
			# Populate host\setup\manifest.txt
			open(FH_W_V_ISS,">$srcpath\\host\\setup\\manifest.txt") or die "Could not open $srcpath\\host\\setup\\manifest.txt";
			my $manifest_contents = "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_Windows_${bldphase}_${date}${month}${year}_${config}.exe,1,Windows,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_RHEL5-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,RHEL5-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_RHEL6-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,RHEL6-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_RHEL7-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,RHEL7-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_RHEL8-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,RHEL8-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_OL6-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,OL6-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_OL7-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,OL7-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_OL8-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,OL8-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_SLES11-SP3-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,SLES11-SP3-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_SLES11-SP4-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,SLES11-SP4-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_SLES12-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,SLES12-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_SLES15-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,SLES15-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_UBUNTU-14.04-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,UBUNTU-14.04-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_UBUNTU-16.04-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,UBUNTU-16.04-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_UBUNTU-18.04-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,UBUNTU-18.04-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_UBUNTU-20.04-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,UBUNTU-20.04-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_DEBIAN7-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,DEBIAN7-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_DEBIAN8-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,DEBIAN8-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_DEBIAN9-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,DEBIAN9-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_DEBIAN10-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,DEBIAN10-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			$manifest_contents = $manifest_contents . "Microsoft-ASR_UA_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_DEBIAN11-64_${bldphase}_${date}${month}${year}_${config}.tar.gz,2,DEBIAN11-64,,${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion},Upgrade,no\n";
			
			print FH_W_V_ISS $manifest_contents;
			close(FH_W_V_ISS);
			print "manifest.txt contents are:\n\n$manifest_contents\n\n";
			
			# Copy Linux agents, push clients, DRA and MARS agent from their corresponding staging servers.
			system($PowerShellPath ' -File "Copy_RHEL.ps1"', $branchname, $buildpath, $config );
	    }
		
		# Set modified timestamp of CX_TP Perl modules 48 hours back. 
		if (uc($product) eq "CX_TP")
		{	
			foreach my $dirname ("Win32-0.39", "Win32-OLE-0.1709", "Win32-Process-0.14", "Win32-Service-0.06")
			{			
				my $dirpath = $srcpath . "\\thirdparty\\server\\" . $dirname;
				ChangeModifiedTime($dirpath);
			}
		}
		
		# Set modified timestamp of UCX Perl modules 48 hours back.
		if (uc($product) eq "UCX")
		{	
			foreach my $dirname ("Win32-UTCFileTime-1.50")
			{			
				my $dirpath = $srcpath . "\\thirdparty\\server\\" . $dirname;
				ChangeModifiedTime($dirpath);
			}
		}
		
		if (uc($product) eq "PSMSI") 
		{
			# Generate PSMSI package
			if (Log_Do("\"$mvss_exe\" ${srcpath}\\$solnfile /project ProcessServerMSI /build \"$config|x64\" /out $logfile") == 0 ) {
				Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
				print "\n\n$config build from $branchname is successful for $product for ProcessServerMSI in $addr\n\n";
			} 
			else 
			{
				Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
				send_mail($failure_alert_ids,'Windows build failure',"$config build from $branchname is failed for $product for ProcessServerMSI in $addr",1);
				die "\n\n$config build from $branchname is failed for $product for ProcessServerMSI in $addr";
			}
		}
		
		if (uc($product) eq "ASRUA") 
		{		
			foreach my $proc ("x86", "x64")
			{
				# Generate both 32-bit and 64-bit MSIs.
				if (Log_Do("\"$mvss_exe\" ${srcpath}\\$solnfile /project UnifiedAgentMSI /build \"$config|$proc\" /out $logfile") == 0 ) {
					Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
					print "\n\n$config build from $branchname is successful for $product for UnifiedAgentMSI in $addr\n\n";				
				} 
				else 
				{
					Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
					send_mail($failure_alert_ids,'Windows build failure',"$config build from $branchname is failed for $product for UnifiedAgentMSI in $addr",1);
					die "\n\n$config build from $branchname is failed for $product for UnifiedAgentMSI in $addr";
				}
			}
			
			# Sign both 32-bit and 64-bit MSIs.
			my ($weekday,$month,$sdate,$hour,$min,$sec,$year) = ( localtime =~ /^(\w{3})\s+(\w{3})\s+(\d{1,2})\s+(\d{1,2}):(\d{2}):(\d{2})\s+(\d{4})$/ );
			$sdate = sprintf("%02d", $sdate);
			my $sdate = "${sdate}_${month}_${year}";
		
			# Copy unmanaged binaries to submission path.
			system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Unsinged_Binaries_To_Submission_Path.ps1"' , $config , "ASRUAMSI" , $srcpath, $branchname, $signing);
		
			# Delete signing related files if they exists.	
			my @signfilestoberemoved = ("InputSign.json", "OutputSign.json", "OutputSignVerbose.log");
			foreach (@signfilestoberemoved) {
				Log_Do("if exist I:\\Signing\\$branchname\\$sdate\\ASRUAMSI\\$_ del /F I:\\Signing\\$branchname\\$sdate\\ASRUAMSI\\$_");
			}	
		
			# Variables related to signing.
			my $inputfileslist = "$srcpath\\build\\scripts\\automation\\CodeSignScripts\\Files.json";
			my $sourcerootdir = "\t\t\"SourceRootDirectory\":" . " \"I:\\\\Signing\\\\$branchname\\\\$sdate\\\\ASRUAMSI\\\\Unsigned\\\\$config\",\n";
			my $destrootdir = "\t\t\"DestinationRootDirectory\":" . " \"I:\\\\Signing\\\\$branchname\\\\$sdate\\\\ASRUAMSI\\\\Signed\\\\$config\",\n";
			my $inputtempfile = "$srcpath\\build\\scripts\\automation\\CodeSignScripts\\InputSignTemplate.json";
			my $inputfile = "I:\\Signing\\$branchname\\$sdate\\ASRUAMSI\\InputSign.json";
			my $outputfile = "I:\\Signing\\$branchname\\$sdate\\ASRUAMSI\\OutputSign.json";
			my $verboselogfile = "I:\\Signing\\$branchname\\$sdate\\ASRUAMSI\\OutputSignVerbose.log";
		
			# Prepare files for signing.
			my $filessection = "ASRUA_MSI_SIGN_FILES";
			prepare_signing_files ("$inputfileslist", "$filessection", "$inputtempfile", "$inputfile", "$sourcerootdir", "$destrootdir");

			# Submit MSIs for signing.
			submit_files_for_signing("$product", "$config", "$inputfile", "$outputfile", "$verboselogfile");
			
			# Copy unmanaged binaries to build path.
			system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Signed_Binaries_To_Build_Path.ps1"' , $config , "ASRUAMSI" , $srcpath, $branchname, $signing);			
			
			# Generate UnifiedAgent package
			if (Log_Do("\"$mvss_exe\" ${srcpath}\\$solnfile /project PackagerUnifiedAgent /build \"$config|Any\ CPU\" /out $logfile") == 0 ) {
				Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
				print "\n\n$config build from $branchname is successful for $product for PackagerUnifiedAgent in $addr\n\n";
			} 
			else 
			{
				Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
				send_mail($failure_alert_ids,'Windows build failure',"$config build from $branchname is failed for $product for PackagerUnifiedAgent in $addr",1);
				die "\n\n$config build from $branchname is failed for $product for PackagerUnifiedAgent in $addr";
			}
		}
		elsif (uc($product) eq "ASRSETUP") 
		{
			my $product = uc($product);              
			# Build UnifiedSetup Packager
			if (Log_Do("\"$mvss_exe\" ${srcpath}\\$solnfile /project Packager /build \"$config|Any\ CPU\" /out $logfile") == 0 ) {
				Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
				print "\n\n$config build from $branchname is successful for $product for packager in $addr\n\n";
			} 
			else 
			{
				Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
				send_mail($failure_alert_ids,'Windows build failure',"$config build from $branchname is failed for $product for packager in $addr",1);
				die "\n\n$config build from $branchname is failed for $product for packager in $addr";
			}   
		}
		elsif (uc($product) eq "PI")
		{
			my $product = uc($product);
			print "\n\nSkipping packaging as product is $product\n\n";
		}
		else
		{
			if (Log_Do("\"$mvss_exe\" ${srcpath}\\$solnfile /project $packagingproject /build \"$config|Win32\" /out $logfile") == 0) {
				Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
				print "\n\n$config packaging for $branchname is successful for $product in $addr\n\n";
			} else {
				Log_Do("if exist $logfile copy /Y $logfile ${datewisedir}\\${config}\\logs");
				send_mail($failure_alert_ids,'Windows build failure',"$config packaging for $branchname is failed for $product in $addr",1);
				die "\n\n$config packaging for $branchname is failed for $product in $addr";
			}
		}

		# Sign installers.
		my ($weekday,$month,$sdate,$hour,$min,$sec,$year) = ( localtime =~ /^(\w{3})\s+(\w{3})\s+(\d{1,2})\s+(\d{1,2}):(\d{2}):(\d{2})\s+(\d{4})$/ );
		$sdate = sprintf("%02d", $sdate);
		my $sdate = "${sdate}_${month}_${year}";	
		
		# Delete signing related files if they exists.	
		my @signfilestoberemoved = ("InputSign.json", "OutputSign.json", "OutputSignVerbose.log");
		foreach (@signfilestoberemoved) {
			Log_Do("if exist I:\\Signing\\$branchname\\$sdate\\${product}_INSTALLER\\$_ del /F I:\\Signing\\$branchname\\$sdate\\${product}_INSTALLER\\$_");
		}	
		
		# Variables specific to signing.
		my $inputfileslist = "$srcpath\\build\\scripts\\automation\\CodeSignScripts\\Files.json";
		my $sourcerootdir = "\t\t\"SourceRootDirectory\":" . " \"I:\\\\Signing\\\\$branchname\\\\$sdate\\\\${product}_INSTALLER\\\\Unsigned\\\\$config\",\n";
		my $destrootdir = "\t\t\"DestinationRootDirectory\":" . " \"I:\\\\Signing\\\\$branchname\\\\$sdate\\\\${product}_INSTALLER\\\\Signed\\\\$config\",\n";
		my $inputtempfile = "$srcpath\\build\\scripts\\automation\\CodeSignScripts\\InputSignTemplate.json";
		my $inputfile = "I:\\Signing\\$branchname\\$sdate\\${product}_INSTALLER\\InputSign.json";
		my $outputfile = "I:\\Signing\\$branchname\\$sdate\\${product}_INSTALLER\\OutputSign.json";
		my $verboselogfile = "I:\\Signing\\$branchname\\$sdate\\${product}_INSTALLER\\OutputSignVerbose.log";
		
		if (uc($product) eq "CX_TP") 
		{
			# Copy installer to submission path.
			system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Unsinged_Installers_To_Submission_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);
			
			# Prepare files for signing.
			my $filessection = "CXTP_SIGN_INSTALLER";
			prepare_signing_files ("$inputfileslist", "$filessection", "$inputtempfile", "$inputfile", "$sourcerootdir", "$destrootdir");
		
			# Submit installer or signing.
			submit_files_for_signing("$product", "$config", "$inputfile", "$outputfile", "$verboselogfile");
		
			# Copy installer to build path.
			system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Singed_Installers_To_Build_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);
		}
		
		if (uc($product) eq "UCX") 
		{
			# Copy installer to submission path.
			system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Unsinged_Installers_To_Submission_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);
	
			# Prepare files for signing.
			my $filessection = "UCX_SIGN_INSTALLER";
			prepare_signing_files ("$inputfileslist", "$filessection", "$inputtempfile", "$inputfile", "$sourcerootdir", "$destrootdir");
		
			# Submit installer for signing.
			submit_files_for_signing("$product", "$config", "$inputfile", "$outputfile", "$verboselogfile");
			
			# Copy installer to build path.
			system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Singed_Installers_To_Build_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);			
		}
	
		if (uc($product) eq "ASRUA") 
		{
			$packagingoutputdir = qw(host\ASRSetup\PackagerUnifiedAgent\bin);	
	
			# Copy installer to submission path.
			system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Unsinged_Installers_To_Submission_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);
	
			# Prepare files for signing.
			my $filessection = "ASRUA_SIGN_INSTALLER";
			prepare_signing_files ("$inputfileslist", "$filessection", "$inputtempfile", "$inputfile", "$sourcerootdir", "$destrootdir");

			# Submit installer for signing.
			submit_files_for_signing("$product", "$config", "$inputfile", "$outputfile", "$verboselogfile");
			
			# Copy installer to build path.
			system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Singed_Installers_To_Build_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);
		}   

		if (uc($product) eq "PSMSI") 
		{	
			$packagingoutputdir = qw(\server\windows\ProcessServerMSI\x64\bin);
		
			# Copy installer to submission path.
			system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Unsinged_Installers_To_Submission_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);
	
			# Prepare files for signing.
			my $filessection = "PSMSI_SIGN_FILES";
			prepare_signing_files ("$inputfileslist", "$filessection", "$inputtempfile", "$inputfile", "$sourcerootdir", "$destrootdir");

			# Submit installer or signing.
			submit_files_for_signing("$product", "$config", "$inputfile", "$outputfile", "$verboselogfile");
			
			# Copy installer to build path.
			system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Singed_Installers_To_Build_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);
		}
		
		if (uc($product) eq "ASRSETUP") 
		{
			$packagingoutputdir = qw(host\ASRSetup\Packager\bin);        
			
			# Copy installer to submission path.
			system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Unsinged_Installers_To_Submission_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);
	
			# Prepare files for signing.
			my $filessection = "ASRSETUP_SIGN_INSTALLER";
			prepare_signing_files ("$inputfileslist", "$filessection", "$inputtempfile", "$inputfile", "$sourcerootdir", "$destrootdir");

			# Submit installer for signing.
			submit_files_for_signing("$product", "$config", "$inputfile", "$outputfile", "$verboselogfile");
					
			# Copy installer to build path.
			system($PowerShellPath ' -File ".\\CodeSignScripts\\Copy_Singed_Installers_To_Build_Path.ps1"' , $config , $product , $srcpath, $branchname, $signing);
		}
		
		if (uc($product) eq "PI")
		{
			my $product = uc($product);
			print "\n\nSkipping exe file packaging as product is $product and build is disabled for this.\n\n";
		}
		else
		{
			$outputfilename = "${srcpath}\\${packagingoutputdir}\\${config}\\" . $this->GetOutputFileName($product);
			die "\n\nCannot find outputfilename - $outputfilename" unless -e $outputfilename;
		}
	
		if (uc($product) eq "UCX")
		{
			$targetfilename = "${datewisedir}\\${config}\\Microsoft-ASR_" . "CX" . "_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_Windows_${buildphase}";
		}
		elsif (uc($product) eq "ASRUA")
		{
			$targetfilename = "${datewisedir}\\${config}\\Microsoft-ASR_UA" . "_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_Windows_${buildphase}";
		}
		else
		{
			$targetfilename = "${datewisedir}\\${config}\\Microsoft-ASR_" . uc($product) . "_${MajorVersion}.${MinorVersion}.${PatchSetVersion}.${PatchVersion}_Windows_${buildphase}";
		}
		$targetfilename = $targetfilename . "_${date}${month}${year}_${config}.exe";	
		
		if (uc($product) eq "ASRSETUP") 
		{
			$targetfilename = "${datewisedir}\\${config}\\" . $this->GetOutputFileName($product);
		}
		elsif (uc($product) eq "PSMSI")
		{
			$targetfilename = "${datewisedir}\\${config}\\" . $this->GetOutputFileName($product);
		}
		
		my $prodversion = "${MajorVersion}.${MinorVersion}";
		if (Log_Do("copy /Y $outputfilename $targetfilename")==0) {
			print "\n\nPackaged $targetfilename successful\n\n";
			system($PowerShellPath ' -File "TRUNK_Builds_Copy.ps1"' , $config , uc($product), $branchname, $buildpath, $prodversion);
		} else {
			send_mail($failure_alert_ids,'Windows build failure',"Packaging failure from $branchname for $product for $config in $addr",1);
			die "\n\nPackaging failure from $branchname for $product for $config";
		}
    }
  }
}

# Create Nuget packages.
sub CreateNugetPackages
{
	print "Creating Nuget packages.";
	my $this = shift;
	my $branchname = $this->Branch();
	my $srcpath = $this->SrcPath();
	my $MajorVersion = $this->GetMajorVersion();
	my $MinorVersion = $this->GetMinorVersion();
    my $Prod_Version="${MajorVersion}.${MinorVersion}";
	my $NugetCreationFolder = "$srcpath\\build\\NugetCreation";
	my $NugetCreationScript = "$NugetCreationFolder\\NugetCreation.ps1";

	my $ret_val = system($PowerShellPath " -File \"$NugetCreationScript\"" , $branchname, $Prod_Version, $NugetCreationFolder); 
	if ($ret_val eq 0)
	{
		print "Successfully created Nuget packages.";
	}
	else
	{
		print "Nuget packages creation failed.";
	}

	my $MarsNugetCreationFolder = "$srcpath\\build\\MarsNugetCreation";
	my $MarsNugetCreationScript = "$MarsNugetCreationFolder\\MarsNugetCreation.ps1";

	my $ret_val = system($PowerShellPath " -File \"$MarsNugetCreationScript\"" , $Prod_Version, $Prod_Version, $MarsNugetCreationFolder); 
	if ($ret_val eq 0)
	{
		print "Successfully created Mars Nuget packages.";
	}
	else
	{
		print "Mars Nuget packages creation failed.";
	}
}

sub DoPSGalleryImageBuild
{
	print "Inside DoPSGalleryImageBuild function";
	my $this = shift;
	my $branchname = $this->Branch();
	
	if (uc($branchname) eq "RELEASE")
	{
		my $datewisedir = $this->DatewiseDir();
		my $MajorVersion = $this->GetMajorVersion();
		my $MinorVersion = $this->GetMinorVersion();
		my $buildpath = "${MajorVersion}.${MinorVersion}";
		my $srcpath = $this->SrcPath();
		my $PSConfigPath = "$srcpath\\build\\PsDeployment\\PsConfig.xml";
		my $PSConfigBuilderPath = "$srcpath\\build\\PsDeployment\\PSConfigBuilder.ps1";
		my $PSDeploymentScriptPath = "$srcpath\\build\\PsDeployment\\PSDeployment.ps1";
		my $PSGISourceLogPath = "$srcpath\\build\\PsDeployment\\PSLogs\\DeployPS.log";
		my $PSGITargetLogPath = "${datewisedir}\\DeployPS.log";
		my $PSCredsPath = "C:\\PS_CERT_DO_NOT_DELETE\\Credentials.xml";
		
		system($PowerShellPath " -File \"$PSConfigBuilderPath\"" , $MajorVersion , $MinorVersion, $PSConfigPath, $PSCredsPath); 
		
		my $ret_val = system($PowerShellPath " -File \"$PSDeploymentScriptPath\"");
		if ($ret_val eq 0 )
		{
			print "Successfully created PS Gallery Image.";
		}
		else
		{
			print "PS GAllery Image Creation failed.";
		}

		# Copy log to build and staging paths.
		Log_Do("copy /Y $PSGISourceLogPath $PSGITargetLogPath");
		system($PowerShellPath ' -File "TRUNK_Builds_Copy.ps1"' , "release", "PSGI", $branchname, $buildpath);
	}
}

# -----------------------------------------------------------------------------------------------------------------------------
# ----------------------- END : HELPER FUNCTIONS OF THIS CLASS -----------------------------------------------------------------
# -----------------------------------------------------------------------------------------------------------------------------





# -----------------------------------------------------------------------------------------------------------------------------
# ----------------------- BEGIN: MAIN FUNCTION OF THIS CLASS -----------------------------------------------------------------
# -----------------------------------------------------------------------------------------------------------------------------

sub Start
{
  print "DEBUG: in Start()\n";

  my $this = shift;

  # Since there is a possibility of date change between start and end of this function,
  # it is better to capture time stats during start and use them uniformly everywhere

  my ($weekday,$month,$date,$hour,$min,$sec,$year) = ( localtime =~ /^(\w{3})\s+(\w{3})\s+(\d{1,2})\s+(\d{2}):(\d{2}):(\d{2})\s+(\d{4})$/ );

  if(length($date) == 1) {
		$date = "0" . $date;
	}
	
  $this->Date($date);
  $this->Month($month);
  $this->Year($year);
  $this->TimeInSecs(time);

  # Create the buildoutputdir and datewise structure under it now
  $this->CreateDatewiseDirStructure();

  # Clean previous builds if desired
  $this->DoCleanup() if $this->ShouldCleanPrevBuild();
  
  # Make branding changes
  $this->DoBrandingChanges();

  # Start the builds
  $this->DoProductBuilds();

  # Do the driver signing
  $this->DoDriverSigning();

  # Load pdb to sym server
  $this->DoLoadPDBToServer();

  # Create the zip of PDBs
  $this->DoCreatePDBZip();

  # Check symbol matching
  $this->DoCheckSymbolMatching();
  
  # Start the packaging
  $this->DoProductPackaging();
  
  # Create Nuget packages.
  $this->CreateNugetPackages();
  
  # Create PS Gallery Image
  $this->DoPSGalleryImageBuild();
}

# -----------------------------------------------------------------------------------------------------------------------------
# ----------------------- END: MAIN FUNCTION OF THIS CLASS -----------------------------------------------------------------
# -----------------------------------------------------------------------------------------------------------------------------

1;

