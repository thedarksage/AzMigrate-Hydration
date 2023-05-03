#!/usr/bin/perl
use strict;
use warnings;
use FindBin qw( $Bin );
use lib $FindBin::Bin;
use Common_Utils;
use HostOpts;
use DatacenterOpts;
use VmOpts;
use DatastoreOpts;
use ResourcePoolOpts;
use FolderOpts;
use ComputeResourceOpts;

my %customopts = (
				  	ostype 			=> {type => "=s", help => "Operating System type to protect [Windows|Linux]",required => 0,},
				  	incremental_disk 	=> {type => "=s", help => "Performing the Incremental_disks (yes/no)?", required =>0,},
				  	retrieve 	=> {type => "=s", help => "To get the selective information (hosts/datastores/networks/luns)", required =>0,},
				  	usernameintext 	=> {type => "", help => "To pass the username/password in text(not encrypted).",},
				  	guuid => { type => "=s", help => "To get the uuids when gust os name is not defined(yes/no)", required => 0,},
				  	validate => { type => "", help => "To validate the vCenter/ ESX credentials (yes/no)",},
				  	resize 	=> {type => "=s", help => "Checking for vmdk resize (yes/no)?", required =>0,},
				  	xml 	=> {type => "=s", help => "Output into a xml file or serialized string (yes/no)?", required =>0,},
				  	cxip	=> {type => "=s", help => "CX server ip and port number in ip:port format.", required =>0,},
				  	https => {type => "=s", help => "CX communication protocol http/https.", required =>0,},
				  	vcliversion => {type => "", help => "To Get the vCLI version.",},
				  );

#####GLOBAL VARIABLES#####
my $hostViews			= "";
my $vmViews 			= "";
my $datacenterViews		= "";
my $resourcePoolViews	= "";
my $folderViews			= "";
my $computeResourceViews= "";
#####GLOBAL VARIABLES#####

#####GetVmInfo#####
##Description 		:: Get's the VM Info from vCenter/vSphere.
##Input 			:: None.
##Ouptut 			:: Returns SUCCESS else Failure.
#####GetVmInfo#####
sub GetVmInfo
{
	my %args 			= @_;
	my $returnCode 		= $Common_Utils::SUCCESS;
	my @vmList			= ();
	my $guuid			= "no";
	if( Opts::option_is_set('guuid') )
	{
		$guuid	= Opts::get_option('guuid');
	}
		
	foreach my $hostView ( @$hostViews )
	{
		my $hostGroup			= $hostView->{mo_ref}->{value}; 
		my $vSphereHostName		= $hostView->name;
		my $hostVersion 		= Common_Utils::GetViewProperty( $hostView , 'summary.config.product.version');
		Common_Utils::WriteMessage("---Collecting details of Virtual Machines residing on host : $vSphereHostName, hostVersion : $hostVersion.---",2);
		( $returnCode , my $dvPortGroupInfo ) 	= HostOpts::GetDvPortGroupsOfHost( $vSphereHostName, $args{dvpgInfo} );
		
		( $returnCode , my $vmInfo ) 	= VmOpts::GetVmData( vmViews => $vmViews , hostGroup => $hostGroup , 
															 vSphereHostName => $vSphereHostName , hostVersion => $hostVersion ,
											 				 osType => $args{osType}, dvPortGroupInfo	=> $dvPortGroupInfo,
											 				 guuid => $guuid
											 			  );
		my @vmInfo						= @$vmInfo;
		push @vmList , @vmInfo; 
	}
	
	( $returnCode , my $mapResourceGroupName ) = ResourcePoolOpts::MapResourcePool( $resourcePoolViews ); 
	if ( $Common_Utils::SUCCESS  != $returnCode )
	{
		return $Common_Utils::FAILURE;
	}
	for( my $i = 0 ; $i <= $#vmList ; $i++ )
	{
		$vmList[$i]->{resourcePool} 	= $$mapResourceGroupName{ $vmList[$i]->{resourcePool} } if exists $vmList[$i]->{resourcePool};
	}

	return ( $Common_Utils::SUCCESS , \@vmList );  
}

#####FindNewDiskInformation#####
##Description		:: Find newly added disk information.
##Input 			:: File Path which contain details of the already protected machine.
##Output			:: Returns SUCCESS else FAILURE.
#####FindNewDiskInformation#####
sub FindNewDiskInformation
{
	my %args 		= @_;
	
	Common_Utils::WriteMessage("Collecting information for Addition of disk.",2);
	
	my( $returnCode , $vmViews )	= VmOpts::GetVmViewsByProps();
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	( $returnCode , my $docNode )	= XmlFunctions::GetPlanNodes( $args{file} );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get root node of xml file $args{file}.",2);
		return ( $Common_Utils::FAILURE );
	}
	
	my $planNodes 					= $docNode->getElementsByTagName('root');
	if ( "array" ne lc ( ref( $planNodes ) ) )
	{
		$planNodes 					= [$docNode];
	}
	
	foreach my $plan ( @$planNodes )
	{
		my $planName 				= $plan->getAttribute('plan');
		my @sourceNodes				= $plan->getElementsByTagName('SRC_ESX');
		my @sourceHosts				= $sourceNodes[0]->getElementsByTagName('host');
		foreach my $sourceHost( @sourceHosts )
		{
			my $updated 		= 0;
			if ( ( defined $sourceHost->getAttribute('source_uuid') ) && ( "" ne $sourceHost->getAttribute('source_uuid') ) )
			{
				foreach my $vmView ( @$vmViews )
				{
					if ( ( Common_Utils::GetViewProperty( $vmView,'summary.config.guestId') ne $sourceHost->getAttribute('alt_guest_name') ) 
							|| ( Common_Utils::IsPropertyValueTrue( $vmView,'summary.config.template' ) )
							|| ( "poweredOff" eq Common_Utils::GetViewProperty( $vmView,'summary.runtime.powerState')->val ) || ( $vmView->name =~ /UnKnown/i ) )
					{
						next;		
					}
					
					if ( Common_Utils::GetViewProperty( $vmView,'summary.config.uuid') ne $sourceHost->getAttribute('source_uuid') )
					{
						next;
					}
					
					$returnCode 	= XmlFunctions::UpdateNode( $sourceHost , $vmView );
					if ( $returnCode ne $Common_Utils::SUCCESS )
					{
						return ( $Common_Utils::FAILURE );
					} 
					$updated 		= 1;
					last;
				}
			}
			
			if ( !$updated )
			{
				Common_Utils::WriteMessage("Failed to find the machine on the basis of UUID. Going to find the machine on basis of vmdk comparision.",2);
				foreach my $vmView ( @$vmViews )
				{
					my $disksInfo		= $sourceHost->getElementsByTagName('disk');
					if ( ( Common_Utils::GetViewProperty( $vmView,'summary.config.guestId') ne $sourceHost->getAttribute('alt_guest_name') ) 
							|| ( Common_Utils::IsPropertyValueTrue( $vmView,'summary.config.template' ) )
							|| ( "poweredOff" eq Common_Utils::GetViewProperty( $vmView,'summary.runtime.powerState')->val ) || ( $vmView->name =~ /UnKnown/i ) )
					{
						next;
					}
					
					my $vSanFolderPath	= "";
					if ( ( defined( $sourceHost->getAttribute('vsan_folder') ) ) && ( "" ne $sourceHost->getAttribute('vsan_folder') ) )
					{
						$vSanFolderPath	= $sourceHost->getAttribute('vsan_folder');
					} 
					
					$returnCode 		= VmOpts::CompareVmdk( $disksInfo , $vmView, "no" , "", $vSanFolderPath );
					if ( $returnCode eq $Common_Utils::EXIST )
					{
						$returnCode 	= XmlFunctions::UpdateNode( $sourceHost , $vmView );
						if ( $returnCode ne $Common_Utils::SUCCESS )
						{
							return ( $Common_Utils::FAILURE );
						} 
						$updated 		= 1;
						last;
					}
				}
			}
			if ( $updated == 0 )
			{
				my $display_name	= $sourceHost->getAttribute('display_name');
				Common_Utils::WriteMessage("ERROR :: Failed to find the machine \"$display_name\" .",3);
				return ( $Common_Utils::FAILURE );
			}			
		}
	}
	
	my @docElements = $docNode;
	$returnCode		= XmlFunctions::SaveXML( \@docElements , $args{file} );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	return ( $Common_Utils::SUCCESS );
}

#####GetInfo#####
##Description		:: Get information in respective to parameter set. parameter can be set for following
##						1. Collection of Target Details( collecting RDM device info is optional ).
##						2. Finding added disk information( either Virtual/Physical machine ).
##						3. Collecting VMX files.
##						4. Resume Operation( most of the task is similar to Get VMX files ).
## 						By default, it collect details of Source vCenter/vSphere.
##Input 			:: target,incrementalDisk,getVmxFiles, resume. 
##Output			:: Returns SUCCESS or FAILURE.
#####GetInfo#####
sub GetInfo
{
	my %args 			= @_;
	
	unless( $args{osType} )
	{
		$args{osType}	= "Windows";
	}
	
	( my $returnCode , $vmViews , $hostViews , $datacenterViews , $resourcePoolViews )
									= Common_Utils::GetViews();
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		return $returnCode;
	}
	
	my ( $IsItvCenter, $vcVersion )			= VmOpts::IsItvCenter();
	
	( $returnCode , my $dvPortGroupInfo ) 	= HostOpts::GetDvPortGroups( $hostViews );
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		return ( $Common_Utils::FAILURE );
	}
		
	( $returnCode , my $vmList )			= GetVmInfo( osType => $args{osType}, dvpgInfo => $dvPortGroupInfo );
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		return $Common_Utils::FAILURE;
	}
		
	( $returnCode , my $resourcePoolInfo )	= ResourcePoolOpts::FindResourcePoolInfo( $resourcePoolViews , $hostViews );
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		return $Common_Utils::FAILURE;
	}
	#removed luns discovery
		
	( $returnCode , my $listDataStoreInfo )	= DatastoreOpts::GetDatastoreInfo( $hostViews , $datacenterViews );
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		return $Common_Utils::FAILURE;
	}
		
	( $returnCode , my $portGroupInfo ) 	= HostOpts::GetHostPortGroups( $hostViews );
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		return ( $Common_Utils::FAILURE );
	}

	( $returnCode , my $networkInfo ) 		= HostOpts::MergeNetworkInfo( $portGroupInfo, $dvPortGroupInfo );
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		return $Common_Utils::FAILURE;
	}
		
	( $returnCode , my $listConfigInfo ) 	= HostOpts::GetHostConfigInfo( $hostViews );
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		return $Common_Utils::FAILURE;
	}
	
	if($args{xml} eq "no")
	{
		require PHP::Serialization; # qw(serialize unserialize);
		my $return_hash = {vmList => $vmList ,osType => $args{osType}, IsItvCenter => $IsItvCenter, version => $vcVersion, datastoreInfo => $listDataStoreInfo , networkInfo => $networkInfo ,configInfo => $listConfigInfo ,resourcePoolInfo => $resourcePoolInfo,scsiDiskInfo => ""};
		my $hash_str = PHP::Serialization::serialize($return_hash);		
		my $disc_file_name = $Common_Utils::vContinuumMetaFilesPath . "/" . $Common_Utils::ServerIp ."__discovery.txt";		
		Common_Utils::WriteMessage("$disc_file_name.",2);
		open(FH,">$disc_file_name");
		print FH $hash_str;
		close(FH);		
		return($returnCode);
	}
	else
	{

		$returnCode								= XmlFunctions::MakeXML( 
														vmList => $vmList , osType => $args{osType} , IsItvCenter => $IsItvCenter , version => $vcVersion,
													   	datastoreInfo => $listDataStoreInfo , networkInfo => $networkInfo ,configInfo => $listConfigInfo ,
													   	resourcePoolInfo => $resourcePoolInfo,scsiDiskInfo => "",  
													  );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return $Common_Utils::FAILURE;
		}
	}
	
	return $returnCode;
}

#####GetRequiredInfo#####
##Description		:: Getting required information only based on arguments. 
##Input 			:: None. 
##Output			:: Returns SUCCESS or FAILURE.
#####GetRequiredInfo#####
sub GetRequiredInfo
{
	my %args 			= @_;
	my $options			= Opts::get_option('retrieve');
	if( $options =~ /^$/ )
	{
		Common_Utils::WriteMessage("Please give valid options.",3);
		return ( $Common_Utils::FAILURE );
	}
	my @options			= split( "!@!@!", $options );		
	Common_Utils::WriteMessage("selected options are @options.",2);
	
	my $returnCode			= $Common_Utils::SUCCESS;	
	my @temp 				= ();
	my $vmList				= \@temp;
	my $listDataStoreInfo	= \@temp;
	my $networkInfo			= \@temp; 
	my $listConfigInfo		= \@temp;
	my $scsiDiskInfo		= \@temp;
	my $resourcePoolInfo	= \@temp;
	my $dvPortGroupInfo		= "";
	
	unless( $args{osType} )
	{
		$args{osType}	= "Windows";
	}
	
	my %options				= ();
	foreach my $option ( @options )
	{
		$options{$option}	= "selected";
	}
	
	( $returnCode , $hostViews )			= HostOpts::GetHostViewsByProps();
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		return ( $returnCode );
	}
	my ( $IsItvCenter, $vcVersion )			= VmOpts::IsItvCenter();
	
	if( defined $options{"hosts"} || defined $options{"networks"})
	{
		( $returnCode , $dvPortGroupInfo ) 	= HostOpts::GetDvPortGroups( $hostViews );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return ( $Common_Utils::FAILURE );
		}		
	}
	
	if( defined $options{"hosts"} || defined $options{"resourcepools"})
	{
		( $returnCode , $resourcePoolViews )= ResourcePoolOpts::GetResourcePoolViewsByProps();
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return ( $Common_Utils::FAILURE );
		}
	}
	if( defined $options{"hosts"} )
	{
		( $returnCode , $vmViews )	= VmOpts::GetVmViewsByProps();
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return ( $returnCode );
		}
		
		( $returnCode , $vmList )	= GetVmInfo( osType => $args{osType},  dvpgInfo => $dvPortGroupInfo );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return $Common_Utils::FAILURE;
		}
	}
	if( defined $options{"datastores"} )
	{	
		( $returnCode , $datacenterViews )	= DatacenterOpts::GetDataCenterViewsByProps();
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return ( $Common_Utils::FAILURE );
		}
		( $returnCode , $listDataStoreInfo )= DatastoreOpts::GetDatastoreInfo( $hostViews , $datacenterViews );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return $Common_Utils::FAILURE;
		}
	}	
	if( defined $options{"networks"} )
	{	
		( $returnCode , my $portGroupInfo ) = HostOpts::GetHostPortGroups( $hostViews );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return ( $Common_Utils::FAILURE );
		}

		( $returnCode , $networkInfo ) 	= HostOpts::MergeNetworkInfo( $portGroupInfo, $dvPortGroupInfo );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return $Common_Utils::FAILURE;
		}
	}
	if( defined $options{"resourcepools"} )
	{
		( $returnCode , $resourcePoolInfo )	= ResourcePoolOpts::FindResourcePoolInfo( $resourcePoolViews , $hostViews );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return $Common_Utils::FAILURE;
		}
	}
	if( defined $options{"luns"} )
	{
		( $returnCode , $computeResourceViews )= ComputeResourceOpts::GetComputeResourceViewsByProps();
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return ( $Common_Utils::FAILURE );
		}
		( $returnCode , $scsiDiskInfo ) 		= HostOpts::GetScsiDiskInfo( $hostViews  , $computeResourceViews );
		if ( $Common_Utils::SUCCESS != $returnCode )
		{
			return $Common_Utils::FAILURE;
		}
	}	

	if($args{xml} eq "no")
	{
		require PHP::Serialization; # qw(serialize unserialize);
		my $return_hash = {	vmList => $vmList ,osType => $args{osType}, IsItvCenter => $IsItvCenter, version => $vcVersion,, 
							datastoreInfo => $listDataStoreInfo , networkInfo => $networkInfo ,
							configInfo => $listConfigInfo ,resourcePoolInfo => $resourcePoolInfo, scsiDiskInfo => $scsiDiskInfo	};
		my $hash_str = PHP::Serialization::serialize($return_hash);
		my $disc_file_name = $Common_Utils::vContinuumMetaFilesPath . "/" . $Common_Utils::ServerIp ."__discovery.txt";		
		Common_Utils::WriteMessage("$disc_file_name.",2);
		open(FH,">$disc_file_name");
		print FH $hash_str;
		close(FH);
		return($returnCode);
	}
	else
	{
		$returnCode								= XmlFunctions::MakeXML( 
																vmList => $vmList , osType => $args{osType} , IsItvCenter => $IsItvCenter , version => $vcVersion,
															   	datastoreInfo => $listDataStoreInfo , networkInfo => $networkInfo ,configInfo => $listConfigInfo ,
															   	scsiDiskInfo => $scsiDiskInfo, resourcePoolInfo => $resourcePoolInfo,  
															  );
	}
	
	return $returnCode;
	
}

#####FindResizeDiskInformation#####
##Description		:: Finding resize vmdks on the source.
##Input 			:: File Path which contain details of the already protected machine.
##Output			:: Returns SUCCESS else FAILURE.
#####FindResizeDiskInformation#####
sub FindResizeDiskInformation
{
	my %args 		= @_;
	
	Common_Utils::WriteMessage("Collecting information for Resize.",2);
	
	my ( $returnCode , $docNode )	= XmlFunctions::GetPlanNodes( $args{file} );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get root node of xml file $args{file}.",2);
		return ( $Common_Utils::FAILURE );
	}
	
	my $planNodes 					= $docNode->getElementsByTagName('root');
	if ( "array" ne lc ( ref( $planNodes ) ) )
	{
		$planNodes 					= [$docNode];
	}
	
	foreach my $plan ( @$planNodes )
	{
		my $planName 				= $plan->getAttribute('plan');
		my @sourceNodes				= $plan->getElementsByTagName('SRC_ESX');
		my @sourceHosts				= $sourceNodes[0]->getElementsByTagName('host');
		foreach my $sourceHost( @sourceHosts )
		{
			my $displayName	= $sourceHost->getAttribute('display_name');
			my $vmUuid		= $sourceHost->getAttribute('source_uuid');
			if ( ( defined $vmUuid ) && ( "" ne $vmUuid ) )
			{								
				( $returnCode,my $vmView )= VmOpts::GetVmViewByUuid( $vmUuid );
				if ( $returnCode != $Common_Utils::SUCCESS )
				{
					return ( $Common_Utils::FAILURE );
				}
				
				my $vmDevices	= Common_Utils::GetViewProperty( $vmView, 'config.hardware.device');
				my @disksInfo 	= $sourceHost->getElementsByTagName('disk');									
				
				foreach my $disk ( @disksInfo )
				{
					my $diskUuid	= $disk->getAttribute('source_disk_uuid');
					my $diskName	= $disk->getAttribute('disk_name');
					my $diskSizeXml	= $disk->getAttribute('size');
					my $found		= 0;
					
					foreach my $device ( @$vmDevices )
					{
						if( $device->deviceInfo->label !~ /Hard Disk/i )
						{
							next;
						}
						
						my $diskSizeVm	= $device->capacityInKB;	
						my $vmdkName 	= $device->backing->fileName;
						my $vmdkUuid	= $device->backing->uuid;
						if ( ( exists $device->{backing}->{compatibilityMode} ) && ( 'physicalmode'eq lc( $device->backing->compatibilityMode ) ) )
						{
							$vmdkUuid	= $device->backing->lunUuid;
						}
						
						if ( ( defined $diskUuid ) && ( $diskUuid ne "" ) && ( $vmdkUuid eq $diskUuid ) )
						{
							$found	= 1;
							if( $diskSizeVm > $diskSizeXml)
							{
								Common_Utils::WriteMessage("Resize found disk $vmdkName of machine $displayName of old size $diskSizeXml to new size $diskSizeVm based on Uuid.",2);
								$disk->setAttribute( 'size', $diskSizeVm );
							}
							elsif( $diskSizeVm < $diskSizeXml)
							{
								Common_Utils::WriteMessage("ERROR :: Resize found on disk $vmdkName of machine $displayName of old size $diskSizeXml to new size $diskSizeVm based on Uuid.",3);
								Common_Utils::WriteMessage("ERROR :: Vmdk shrink is not supported.",3);
								return( $Common_Utils::FAILURE );
							}
							last;
						}						
						elsif ( ( !defined $diskUuid ) || ( $diskUuid eq "" ) ) 
						{
							my @splitDiskName 		= split( /\[|\]/ , $diskName );
							my $key_file 				= $device->key;		
							my $disk_name 				= $device->backing->fileName;
							if ( ( defined $vmView->snapshot ) && ( Common_Utils::GetViewProperty( $vmView, 'layout.disk') ) )
							{
								my $diskLayoutInfo 		= Common_Utils::GetViewProperty( $vmView, 'layout.disk');
								foreach ( @$diskLayoutInfo )
								{
									if ( $_->key != $key_file )
									{
										next;
									}
									my @baseDiskInfo = @{ $_->diskFile };
									foreach my $disk ( @baseDiskInfo )
									{
										if( $disk =~ /-[0-9]{6}\.vmdk$/)
										{
											next;
										}
										$disk_name = $disk;
										Common_Utils::WriteMessage("Base disk information collected for key( $key_file ) = $disk_name.",2);
									}
								}
							}
								
							my @splitDisk_Name 	= split( /\[|\]/ , $disk_name );
							if( $splitDiskName[-1] eq $splitDisk_Name[-1] )
							{
								$found	= 1;
								if( $diskSizeVm > $diskSizeXml)
								{
									Common_Utils::WriteMessage("Resize found disk $vmdkName of machine $displayName of old size $diskSizeXml to new size $diskSizeVm based on name.",2);
									$disk->setAttribute( 'size', $diskSizeVm );
								}
								elsif( $diskSizeVm < $diskSizeXml)
								{
									Common_Utils::WriteMessage("ERROR :: Resize found on disk $vmdkName of machine $displayName of old size $diskSizeXml to new size $diskSizeVm based on name.",3);
									Common_Utils::WriteMessage("ERROR :: Vmdk shrink is not supported.",3);
									return( $Common_Utils::FAILURE );
								}
								last;								
							}
						}
					}
					if( ! $found )
					{
						Common_Utils::WriteMessage("ERROR :: Vmdk $diskName of machine $displayName not found.",3);
						Common_Utils::WriteMessage("ERROR :: Please check any vmdks removed from $displayName.",3);
						return( $Common_Utils::FAILURE );
					}		
				}
			}			
			else
			{
				Common_Utils::WriteMessage("ERROR :: Failed to find the machine \"$displayName\" .",3);
				return ( $Common_Utils::FAILURE );
			}			
		}
	}
	
	my @docElements = $docNode;
	$returnCode		= XmlFunctions::SaveXML( \@docElements , $args{file} );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	return ( $Common_Utils::SUCCESS );
}

#####Help#####
##Description 		:: List's all possible calls for this Script in different scenarios.
##Input 			:: None.
##Output			:: None.
#####Help#####
sub Help
{
	Common_Utils::WriteMessage("***** Usage of GetInfo.pl *****",3);
	Common_Utils::WriteMessage("-------------------------------",3);
	Common_Utils::WriteMessage("Get Virtual Machine information on vCenter/vSphere --> GetInfo.pl -server Server_IP -username 'USERNAME' -password 'PASSWORD' -ostype 'Windows/Linux'.",3);
}

#####MAIN#####
BEGIN:

my $ReturnCode				= $Common_Utils::SUCCESS;
my ( $sec, $min, $hour )	= localtime(time);
my $logFileName 		 	= "vContinuum_Temp_$sec$min$hour.log";

Opts::add_options(%customopts);
Opts::parse();

my $serverIp	= Opts::get_option('server');

if( Opts::option_is_set('version') )
{
	Opts::validate();
}

if( Opts::option_is_set('vcliversion') )
{
	print Common_Utils::FindvCliVersion();
	exit( $ReturnCode );
}

my $xmlOrNot	= "yes";
if ( Opts::option_is_set('xml') )
{
	$xmlOrNot	= Opts::get_option('xml');
	Common_Utils::SetLogPaths("cx");
	$Common_Utils::CxIpPortNumber	= Opts::get_option('cxip');
	( $Common_Utils::ServerIp , my $port )	= split( /:/, $serverIp );
	if( ( Opts::option_is_set('https') ) && ( "yes" eq Opts::get_option('https') ) )
	{
		$Common_Utils::urlProtocol		= "https";
	}
	$Common_Utils::COMPONENT	= "cx";
}

Common_Utils::CreateLogFiles( "$Common_Utils::vContinuumLogsPath/$logFileName" , "$Common_Utils::vContinuumLogsPath/vContinuum_ESX.log" );

unless( Opts::option_is_set('usernameintext') )
{
	($ReturnCode, $serverIp,my $userName,my $password )	= HostOpts::getEsxCreds( $serverIp );
	Opts::set_option('username', $userName);
	Opts::set_option('password', $password );
}
Opts::validate();

if($ReturnCode	!= $Common_Utils::SUCCESS )
{
	Common_Utils::WriteMessage("ERROR :: Failed to get credentials from CX.",3);
}
elsif ( ! ( Opts::option_is_set('username') && Opts::option_is_set('password') && Opts::option_is_set('server') ) )
{
	Common_Utils::WriteMessage("ERROR :: vCenter/vSphere IP,Username and Password values are the basic parameters required.",3);
	Common_Utils::WriteMessage("ERROR :: One or all of the above parameters are not set/passed to script.",3);
	$ReturnCode	= $Common_Utils::INVALIDARGS;
}
else
{	
	$ReturnCode		= Common_Utils::Connect( Server_IP => $serverIp, UserName => Opts::get_option('username'), Password => Opts::get_option('password') );
	
	if ( ( !Opts::option_is_set('validate')) && ( $Common_Utils::SUCCESS == $ReturnCode ) )
	{
		eval
		{
			if ( Opts::option_is_set('retrieve'))
			{
				$ReturnCode = GetRequiredInfo(
												osType => Opts::get_option('ostype'),
												xml				=> $xmlOrNot, 
											);
			}
			elsif ( ( Opts::option_is_set('incremental_disk') ) && ( "yes" eq Opts::get_option('incremental_disk') ) )
			{
				my $file 		= "$Common_Utils::vContinuumMetaFilesPath/$Common_Utils::EsxXmlFileName";
				$ReturnCode		= FindNewDiskInformation( file => $file );
			}
			elsif ( ( Opts::option_is_set('resize') ) && ( "yes" eq Opts::get_option('resize') ) )
			{
				my $file 		= "$Common_Utils::vContinuumMetaFilesPath/$Common_Utils::ResizeXmlFileName";
				$ReturnCode		= FindResizeDiskInformation( file => $file );
			}
			else
			{				
				$ReturnCode = GetInfo(
									osType			=> Opts::get_option('ostype'),
									xml				=> $xmlOrNot, 
								  );
			}
		};
		if ( $@ )
		{
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			$ReturnCode	= $Common_Utils::FAILURE;
		}
	}
}

Common_Utils::LoggedData( tempLog => "$Common_Utils::vContinuumLogsPath/$logFileName" , planLog => "" );

exit( $ReturnCode );							 

__END__