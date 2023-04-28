=head
	PackageName : Common_Utils.pm
		1. WriteMessage() -- for log writing.
		2. Validate IP address.
		3. Open/Close Log files.
		4. Query Local IP / CX IP.
		5. Parse XML from CX.
		6. Post XML to CX.
  
=cut

#----------------------
package Common_Utils;
#----------------------

use strict;

$| 	= 1;
require Exporter;
use IO::File;
use XML::LibXML;
use VMware::VIRuntime;
use VMware::VILib;
use VMware::VIExt;
use File::Path;
use File::Copy;
use File::Basename;
use Cwd;
use threads;
use LWP::UserAgent;
use HTTP::Request::Common;
if( $^O =~ /MSWin32/i )
{
	require Win32::Registry;
}
use MIME::Base64;
use Data::Dumper;
use Digest::MD5;
use Archive::Zip qw( AZ_OK COMPRESSION_DEFLATED );
use Encode;
use utf8;
use HTTP::Request::Common qw(POST);
use POSIX qw(ceil floor);
use Fcntl qw(:flock);
use vars qw( $VERSION @ISA @EXPORT_OK @EXPORT );
use Time::Local qw( timelocal );

our ( $ESX_LOG , $ESX_LOG_UI , $SUCCESS , $FAILURE , $EXIST , $NOTFOUND , $EsxXmlFileName , $RecoveryXmlFileName , $RemoveXmlFileName,
		,$INVALIDIP , $INVALIDCREDENTIALS ,$DrDrillXmlFileName , $vContinuumInstDirPath , $vContinuumLogsPath , $vContinuumScriptsPath , 
		$vContinuumMetaFilesPath , $vContinuumPlanFilesPath, $vContinuumPlanDir, $planTempLogFile , %taskInfo , $CxIpPortNumber, $CxIp, 
		$CxPortNumber, $PowerOnAnswer, $ResizeXmlFileName, $ServerIp, $vxAgentInstallationDir, $urlProtocol, $HostId, $PathForFiles,
		$INVALIDARGS, $NOVMSFOUND, $COMPONENT );
@ISA 					= qw(Exporter);
@EXPORT_OK 				= qw( );
@EXPORT 				= qw( $ESX_LOG $ESX_LOG_UI );

$SUCCESS 				= 0;
$FAILURE 				= 1;
$INVALIDIP				= 2;
$INVALIDCREDENTIALS		= 3;
$EXIST 					= 4;
$NOTFOUND				= -1;
$INVALIDARGS			= 5;
$NOVMSFOUND				= 6;

$vContinuumInstDirPath	= File::Basename::dirname(File::Path::getcwd());
$vContinuumLogsPath		= "$vContinuumInstDirPath/logs";
$vContinuumScriptsPath	= "$vContinuumInstDirPath/Scripts";
$vContinuumMetaFilesPath= "$vContinuumInstDirPath/latest";
$vContinuumPlanFilesPath= File::Basename::dirname($vContinuumInstDirPath)."/failover/data" ;
$vxAgentInstallationDir = File::Basename::dirname($vContinuumInstDirPath);
$vContinuumPlanDir		= "";
$planTempLogFile		= "$vContinuumLogsPath/Scripts_Plan_Temp.log";
$PathForFiles			= "";

#fileNames
$EsxXmlFileName			= "Esx.xml";
$RecoveryXmlFileName	= "Recovery.xml";
$RemoveXmlFileName		= "Remove.xml";
$DrDrillXmlFileName		= "Snapshot.xml";
$ResizeXmlFileName		= "Resize.xml";

%taskInfo 				= (	
							TaskName 	=> "",
							Description	=> "",
							TaskStatus	=> "",
							ErrorMessage=> "",
							FixSteps	=> "",
							LogPath		=> ""
						);
$CxIpPortNumber			= "";
$CxIp					= "";
$CxPortNumber			= "";
$PowerOnAnswer			= 1;
$urlProtocol			= "http";
$HostId					= "";
$COMPONENT				= "vcon";

#vCenter or ESX ip
$ServerIp				= "";

#####Connect#####
##Description 		:: It makes a connection with vCenter/vSphere after validating ESX IP passed.
##Input 			:: ESX_IP, Username and Password.
##Output 			:: Returns 2 on invalid IP, returns 3 if failed to establish connection else returns 1 on success.
#####Connect#####
sub Connect
{
	my %Connect_args 	= @_;
	
	my $returnCode	= HostOpts::EstablishConnection( $Connect_args{Server_IP} , $Connect_args{UserName} , $Connect_args{Password} );
	if ( $returnCode != $SUCCESS )
	{
		WriteMessage("ERROR :: Failed to connect with vCenter/vSphere server $Connect_args{ESX_IP}.",3);
		return ( $returnCode );
	}
	return ( $SUCCESS );
}

#####WriteMessage#####
##Description 		:: Generic function which writes messages on to different files based on log levels.
##Input 			:: Message to be witten and it's log level.LOG level's:1->STDOUT ,2->Complete_log file, 3-> STDOUT,UI_LOG and the complete_log file.
##Output			:: none.
#####WriteMessage#####
sub WriteMessage( $$ )
{
	my $line 		= shift;
	my $log_level 	= shift;
	my $time 		= localtime(time);
	
	if ( 1 == $log_level )
	{
		syswrite STDOUT , "$line\n";	
	}
	elsif ( 2 == $log_level )
	{
		$ESX_LOG->printflush("$time : INFO :: DEBUG :: $line\n");
	}
	elsif ( 3 == $log_level )
	{
		syswrite STDOUT , "$line\n";	
		
		$ESX_LOG->printflush("$time : INFO :: DEBUG :: $line\n");
		
		$ESX_LOG_UI->printflush("$line\n");
	}
}

#####CreateLogFiles#####
##Description 		:: Creates log files for logging the Sript execution and as well for UI in case of Errors. 
##Input 			:: Name of the files to be created.
##Output 			:: LOG file handles are assigned to GLOBAL variables which are exported.Should we really export?
#####CreateLogFiles#####
sub CreateLogFiles
{
	my @logFileNames 		= @_;
	
	$ESX_LOG 				= new IO::File $logFileNames[0] , "w";
	binmode $ESX_LOG, ':encoding(UTF-8)';
	if( !defined( $ESX_LOG ) )
	{
		syswrite STDOUT , "Failed to create the file \"$logFileNames[0]\".\n";
	}
	$ESX_LOG_UI 			= new IO::File $logFileNames[1] , "w";
	binmode $ESX_LOG_UI, ':encoding(UTF-8)';
	if( !defined( $ESX_LOG_UI ) )
	{
		syswrite STDOUT , "Failed to create the file \"$logFileNames[1]\".\n";
	}
}

#####is_a_valid_ip_address#####
##Description 		:: validates whether the given IP is valid or not.
##Input 			:: ip value.
##Output 			:: if valid returns 1 else returns IP address by triming  all leading zero's.
#####is_a_valid_ip_address#####
sub is_a_valid_ip_address
{
	my %args			= @_;
	my $new_ip 			= "";
	
	WriteMessage("IP address received for validation : $args{ip}.",2);
	##some times we might receive host name instead of IP. So we need to check whether it contains a 
	
	if( $args{ip} =~ /(\d+)\.(\d+)\.(\d+)\.(\d+)(:(\d+))?/ )
	{
		if( $1 > 255 || $2 > 255 || $3 > 255 || $4 > 255)
		{
			WriteMessage("ERROR :: Invalid IP : \"$args{ip}\".Please enter a valid IPv4 address.",3);
			return $FAILURE;
		}
		if( ( defined $6) && ( $6 > 65535 ))
		{
			WriteMessage("ERROR :: Invalid PortNumber : \"$args{ip}\".Please enter a valid IPv4 address.",3);
			return $FAILURE;
		}
		my @subparts	= ( $1, $2, $3, $4);
		for( my $i = 0; $i < 4 ; $i++ )
		{
			$subparts[$i] =~ s/^(0+)//g;
			if ( !length ( $subparts[$i] ) )
			{
				$subparts[$i]	= 0;
			}
			if ( !length ( $new_ip ) )
			{
				$new_ip = $subparts[$i];
				next;					
			}
			$new_ip = $new_ip . "." . $subparts[$i];
		}
		
		$ServerIp	= $new_ip;
		
		if( defined $6)
		{
			my $port = $6;
			$port =~ s/^(0+)//g;
			if ( !length ( $port ) )
			{
				$port = 0;
			}			
			$new_ip	= $new_ip . ":" . $port;
		}
	}
	elsif( $args{ip} =~ /\w/)
	{
		WriteMessage("Treating the given server ip \"$args{ip}\" as ipv6 or fqdn host name.",2);
		$ServerIp	= $args{ip};
		return( $SUCCESS, $args{ip} );
	}
	else
	{
		WriteMessage("ERROR :: Invalid IP or PortNumber : \"$args{ip}\".Please enter a valid IPv4 address.",3);
		return $FAILURE;
	}
	
	WriteMessage("Returning IP address $new_ip.",2);
	return( $SUCCESS, $new_ip);
}

#####FindCXIP#####
##Description 		:: Finds the CX IP and Its Port Number.
##Input 			:: None.
##Output 			:: Returns SUCCESS and CxIp:PortNumber else FAILURE.
#####FindCXIP#####
sub FindCXIP
{
	my $os_type 			= $^O;
	my $am_I_a_linux_box 	= 0;
	
	WriteMessage("THE OS TYPE of vContinuum machine = $os_type.",2);
	if($os_type =~ /linux/i)
	{
		$am_I_a_linux_box = 1;
		my $confFile = File::Basename::dirname($vContinuumInstDirPath)."/Vx/etc/drscout.conf";
		if ( !open( CONFFILE , $confFile ) )
		{
			WriteMessage("ERROR :: Failed to open \"$confFile\". please check whether user has permission to access it.",3);
			return ( $FAILURE );
		} 
	
		my @lines 		= <CONFFILE>;
		foreach my $line ( @lines )
		{
			if($line =~ /^$/)
			{
			}
			else
			{
				$line =~ s/\n//;
				$line =~ s/\r//;
				$line =~ s/\s+//g; 
				
				if( lc($line) =~ /^port/ ) 
				{
				 	$CxPortNumber = (split("=",$line))[1];
				}
				if( lc($line) =~ /^hostname/ )
				{
					$CxIp = (split("=",$line))[1];
				}			
			}
		}
		WriteMessage("CX Ip and Port number: $CxIp:$CxPortNumber .",2);
	}

	if(0 == $am_I_a_linux_box)
	{
		my %RegType = (
					0 => 'REG_0',
					1 => 'REG_SZ',
					2 => 'REG_EXPAND_SZ',
					3 => 'REG_BINARY',
					4 => 'REG_DWORD',
					5 => 'REG_DWORD_BIG_ENDIAN',
					6 => 'REG_LINK',
					7 => 'REG_MULTI_SZ',
					8 => 'REG_RESOURCE_LIST',
					9 => 'REG_FULL_RESOURCE_DESCRIPTION',
					10 => 'REG_RESSOURCE_REQUIREMENT_MAP');

		my $Register = "SOFTWARE\\SV Systems";
		my $RegType;
		my $RegValue;
		my $RegKey;
		my $value;
		my %vals;
		my @key_list;
		my $hkey;

		if( ! $::HKEY_LOCAL_MACHINE->Open( $Register, $hkey ) )
		{
			WriteMessage("ERROR :: Cannot find the Registry Key $Register..Cannot find the CX Ip Address.",3);
			return ( $Common_Utils::FAILURE );
		}

		$hkey->GetValues(\%vals);

		foreach $value (keys %vals)
		{
			$RegType 	= $vals{$value}->[1];
			$RegValue 	= $vals{$value}->[2];
			$RegKey 	= $vals{$value}->[0];
			$RegKey 	= 'Default' if ($RegKey eq '');
			
			if($RegKey eq "ServerName")
			{
				if(1 != $RegType)
				{
					WriteMessage("ERROR : Registry Key \"ServerName\" is not of type REG_SZ.Considering this as an ERROR.",3);
					return ( $Common_Utils::FAILURE );
				}
				$CxIp = $RegValue;
			}
						
			if($RegKey eq "ServerHttpPort")
			{
				if(4 != $RegType)
				{
					WriteMessage("ERROR : Registry Key \"ServerHttpPort\" is not of type REG_DWORD.Considering this as an ERROR.",3);
					return ( $Common_Utils::FAILURE );
				}
				$CxPortNumber = $RegValue;				
			}
			
			if($RegKey eq "Https")
			{
				if(4 != $RegType)
				{
					WriteMessage("ERROR : Registry Key \"Https\" is not of type REG_DWORD.Considering this as an Warning and proceeding with http protocol.",2);
				}
				elsif( $RegValue == 1)
				{
					$urlProtocol = "https";
					WriteMessage("Https is enabled.",2);				
				}			
			}
			
			if( $RegKey	eq "HostId" )
			{
				if(1 != $RegType)
				{
					WriteMessage("ERROR : Registry Key \"HostId\" is not of type REG_SZ.Considering this as an ERROR.",3);
					return ( $Common_Utils::FAILURE );
				}
				$HostId = $RegValue;
			}
		}
		$hkey->Close();
		WriteMessage("CX Ip and Port number: $CxIp:$CxPortNumber .",2);
		
		$Register = "SOFTWARE\\SV Systems\\vContinuum";
		if( ! $::HKEY_LOCAL_MACHINE->Open( $Register, $hkey ) )
		{
			WriteMessage("ERROR :: Cannot find the Registry Key $Register..Cannot find the vContinuum paths.",3);
			return ( $Common_Utils::FAILURE );
		}
		
		$hkey->GetValues(\%vals);
		foreach $value (keys %vals)
		{
			$RegType 	= $vals{$value}->[1];
			$RegValue 	= $vals{$value}->[2];
			$RegKey 	= $vals{$value}->[0];
			$RegKey 	= 'Default' if ($RegKey eq '');
			
			if($RegKey eq "PathForFiles")
			{
				if(1 != $RegType)
				{
					WriteMessage("ERROR : Registry Key \"PathForFiles\" is not of type REG_SZ.Considering this as an ERROR.",3);
					return ( $Common_Utils::FAILURE );
				}
				$PathForFiles = $RegValue;
			}
		}
		$hkey->Close();
	}
	$CxIpPortNumber = "$CxIp:$CxPortNumber";
	return ( $SUCCESS , $CxIpPortNumber );
}

#####GetHostNodesInCX#####
##Description		:: Finds the List of Nodes registered/ponting to the CX. They might not be licensed.
##Input 			:: CX IP and CX port Number.
##Output 			:: Returns SUCCESS else FAILURE.
#####GetHostNodesInCX#####
sub GetHostNodesInCX
{
	my $cxIpAndPortNum 	= shift;
	my $nodesInCx		= shift;
	
	my ( $returnCode, $persistData)	= PostCxCli( form => { "operation" => 3 });
	if( $returnCode == $FAILURE )
	{
		WriteMessage("ERROR :: GetHostNodes CX CLI call is failed.",3);
		return ( $Common_Utils::FAILURE );		
	}
	
	my $parser 			= XML::LibXML->new();
	my $tree 			= $parser->parse_string( $persistData );
	my $root 			= $tree->getDocumentElement;
	my @host 			= $root->getElementsByTagName('host');
	
	foreach ( @host ) 
	{
		my %nodeInfoInCx= ();
		
		eval
		{
			my $id	= $_->getAttribute('id');
			WriteMessage("HostId of node = $id.",2);
			$nodeInfoInCx{hostId} = $id;
		};
		if($@)
		{
			$nodeInfoInCx{hostId} = "";
		}
		
		my @name 		= $_->getElementsByTagName('name');
		my $name 		= "";
		eval
		{
			$name 		= $name[0]->getFirstChild->getData;
			WriteMessage("Name of node = $name.",2);
			$nodeInfoInCx{hostName} = $name;
		};
		if($@)
		{
			$nodeInfoInCx{hostName} = "";
		}
		
		my @ip_address = $_->getElementsByTagName('ip_address');
		eval
		{
			my $ip_address = $ip_address[0]->getFirstChild->getData;
			WriteMessage("IP address = $ip_address.",2);
			$nodeInfoInCx{ipAddress} = $ip_address;
		};
		if($@)
		{
			WriteMessage("ERROR :: The IP address received for one of the Node : $name is invalid. Please check on CX with what IP this node is registered.",2);
			$nodeInfoInCx{ipAddress} = "";
		}
		
		my @ip_addresses = $_->getElementsByTagName('ip_addresses');
		eval
		{
			my $ip_addresses = $ip_addresses[0]->getFirstChild->getData;
			WriteMessage("IP address = $ip_addresses.",2);
			$nodeInfoInCx{ipAddresses} = $ip_addresses;
		};
		if($@)
		{
			WriteMessage("ERROR :: The IP address received for one of the Node : $name is invalid. Please check on CX with what IP this node is registered.",2);
			$nodeInfoInCx{ipAddresses} = "";
		}
		
		my @os_type = $_->getElementsByTagName('os_type');
		eval
		{
			my $os_type = $os_type[0]->getFirstChild->getData;
			WriteMessage("os_type = $os_type.",2);
			$nodeInfoInCx{osType}	= $os_type;
		};
		if($@)
		{
			WriteMessage("ERROR :: OS type is not determined for $name.",3);
			$nodeInfoInCx{osType}	= "";
		}
		
		my @vx_agent_path = $_->getElementsByTagName('vx_agent_path');
		eval
		{
			my $vx_agent_path = $vx_agent_path[0]->getFirstChild->getData;
			WriteMessage("VX-agent_path = $vx_agent_path.",2);
			$nodeInfoInCx{vxPath}	= $vx_agent_path;
		};
		if($@)
		{
			$nodeInfoInCx{vxPath}	= "";
		}
		
		my @fx_agent_path = $_->getElementsByTagName('fx_agent_path');
		eval
		{
			my $fx_agent_path = $fx_agent_path[0]->getFirstChild->getData;
			WriteMessage("Fx_agent_path = $fx_agent_path.",2);
			$nodeInfoInCx{fxPath}	= $fx_agent_path;
		};
		if($@)
		{
			$nodeInfoInCx{fxPath}	="";
		}
		push @$nodesInCx , \%nodeInfoInCx;
	}
	
	return ( $SUCCESS , $nodesInCx );
}

#####PostCxCli#####
##description 		:: Make's an FTP call to CX using CX_CLI.
##input 			:: none.
##output 			:: on success return's 1 else 0.
#####PostCxCli#####
sub PostCxCli
{
	my %args			= @_;
	my $version 		= "1.0";
	my $functionName	= "cx_cli_no_upload";
	my $accessFile 		= "/cli/cx_cli_no_upload.php";
	my $requestMethod 	= "POST";
	my $requestID		= substr( "ts:". Time::Local::timelocal( localtime() ) . "-" . generateUuid() , 0, 32 );
	
	my $ua 		= LWP::UserAgent->new();
	my $url 	= "$urlProtocol://".$CxIpPortNumber."/cli/cx_cli_no_upload.php";
	WriteMessage("url is = $url",2);
	$ua->timeout(300);
	$ua->env_proxy;
	
	my $caFile;
	if( $urlProtocol eq "https" )
	{
		$caFile	= "$PathForFiles/certs/" . $CxIp . "_" . $CxPortNumber . ".crt";
	}
	local $ENV{HTTPS_CA_FILE}	= $caFile if( $urlProtocol eq "https");
	
	my ( $returnCode, $accessSignature )= GenerateAccessSignature( 	requestMethod	=> $requestMethod,
																	accessFile		=> $accessFile,
																	functionName	=> $functionName,
																	requestID		=> $requestID,
																	version			=> $version,
																);
	if( $returnCode == $FAILURE )
	{
		WriteMessage("ERROR :: Failed to generate Access signature.",3);
		return ( $FAILURE );
	}
											
	$ua->default_header( 	"HTTP-AUTH-SIGNATURE"	=> $accessSignature,
							"HTTP-AUTH-REQUESTID"	=> $requestID,
							"HTTP-AUTH-PHPAPI-NAME"	=> $functionName,
							"HTTP-AUTH-VERSION"		=> $version, 
						);
	
	my $response		= "";
	eval
	{
		$response		= $ua->post( $url, $args{form} ) ;
	};
	if ( $@ )
	{
		WriteMessage("ERROR :: $@.",3);
		WriteMessage("ERROR :: CX CLI call is failed.",3);
		return ( $FAILURE );
	}
	
	my $persistData		 = "";
	if ($response->is_success)
	{
		$persistData 	 =  $response->decoded_content;
		$persistData 	 =~ s/^\s+//;
		$persistData 	 =~ s/\s+$//;
	}
	else
	{
		 WriteMessage("ERROR :: CX CLI response : $response->{status_line}",3);
	     return ( $FAILURE );
	}
	
	WriteMessage("CX CLI response : $persistData.",2);	
	return ( $SUCCESS, $persistData );
}

#####parseXMLFile#####
##description 		:: parse's XML file which is received against query nodes through the CX-CLI.
##input 			:: file name.
##output 			:: returns 1 on success else 0.
#####parseXMlFile#####
sub parseXMlFile
{
	my $file 	= shift;
	my $parser 	= XML::LibXML->new();
	my $tree 	= $parser->parse_file($file);
	my $root 	= $tree->getDocumentElement;
	
	my @host 	= $root->getElementsByTagName('host');
	
	foreach ( @host ) 
	{
		my @ip_address = $_->getElementsByTagName('ip_adress');
		my $ip_address = $ip_address[0]->getFirstChild->getData;
		WriteMessage("IP address read from XML(fx_automate.xml) = $ip_address.",2);
	}
	return 1;
}

#####MergeLoggedData#####
##Description 		:: closes the file handles opened for the logs.
##Input 			:: Temporary log file path and path to main log file.
##Output 			:: none.
#####MergeLoggedData#####
sub MergeLoggedData
{
	my %args	= @_;
		
	my $FH 		= new IO::File $args{tempLogFilePath} , "r";				
	
	my $LOG		= new IO::File $args{mainLogFilePath} , "a";
		
	for ( my $i = 0 ;  $i < 5 ; $i++ )
	{
		if ( flock ( $LOG , LOCK_EX|LOCK_NB ) )
		{
			$LOG->print("-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-\n");
			foreach ( $FH->getlines )
			{
				$LOG->print("$_");;
			}
			$LOG->print("-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-\n");
			flock ( $LOG , LOCK_UN );
			last;
		}
		else
		{
			sleep(5);
		}
	}
		
	$FH->close;
	$LOG->close;
}

#####LoggedData#####
##Description 		:: logging according to plan based
##Input 			:: Temporary log file path and plan log directory and if additionalflag
##Output 			:: none.
#####LoggedData#####
sub LoggedData
{
	my %args	= @_;
	$ESX_LOG->close;
	$ESX_LOG_UI->close;
	if(defined $args{planLog} && $args{planLog} ne "" && -d "$vContinuumPlanFilesPath/$args{planLog}")
	{
		if(-e $planTempLogFile && !(-f "$vContinuumPlanFilesPath/$args{planLog}/vContinuum_Scripts.log"))
		{
			MergeLoggedData( tempLogFilePath => $planTempLogFile , mainLogFilePath => "$vContinuumPlanFilesPath/$args{planLog}/vContinuum_Scripts.log");
			unlink $planTempLogFile;
		}
		MergeLoggedData( tempLogFilePath => $args{tempLog} , mainLogFilePath => "$vContinuumPlanFilesPath/$args{planLog}/vContinuum_Scripts.log");
	}
	elsif( $COMPONENT ne "cx" )
	{
		MergeLoggedData( tempLogFilePath => $args{tempLog} , mainLogFilePath => $planTempLogFile);
		LogRoation( $planTempLogFile );
	}
	MergeLoggedData( tempLogFilePath => $args{tempLog} , mainLogFilePath => "$vContinuumLogsPath/vContinuum_Scripts.log");	
	unlink $args{tempLog};
	
	LogRoation( "$vContinuumLogsPath/vContinuum_Scripts.log" );
}

#####GetViews#####
##Description 		:: It gets view of VirtualMachine, Host, DataCenter, ResourcePool.
##Input 			:: NONE.
##Output 			:: Returns SUCCESS or FAILURE.
#####GetViews#####
sub GetViews
{
	my( $returnCode , $vmViews )			= VmOpts::GetVmViewsByProps();
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		return ( $returnCode );
	}
	
	( $returnCode , my $hostViews )			= HostOpts::GetHostViewsByProps();
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		return ( $returnCode );
	}
	
	( $returnCode , my $datacenterViews )	= DatacenterOpts::GetDataCenterViewsByProps();
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	( $returnCode , my $resourcePoolViews )	= ResourcePoolOpts::GetResourcePoolViewsByProps();
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		return ( $Common_Utils::FAILURE );
	}
		
	WriteMessage("Successfully discovered all views.",2);
	return ( $Common_Utils::SUCCESS , $vmViews , $hostViews , $datacenterViews , $resourcePoolViews );
}

#####PostRequestXml#####
##Description		:: Posts a request XML to CX and returns response XML.
##Input 			:: Request XML.
##Output 			:: Return SUCCESS and Response XML else FAILURE.
#####PostRequestXml#####
sub PostRequestXml
{
	my %args 		= @_;
	my $ReturnCode 	= $SUCCESS;
	my $responseXml = "";
	
	my $version 		= "1.0";
	my $functionName	= $args{functionName};
	my $accessFile 		= "/ScoutAPI/CXAPI.php";
	my $requestMethod 	= "POST";
	my $requestID		= substr( "ts:". Time::Local::timelocal( localtime() ) . "-" . generateUuid() , 0, 32 );
	
	my $userAgent	= LWP::UserAgent->new();
	$userAgent->timeout(30);
	
	my $url 		= "$urlProtocol://".$args{cxIp}."/ScoutAPI/CXAPI.php";
	WriteMessage("PostRequestXml :: url is = $url",2);
	
	my $caFile;
	if( $urlProtocol eq "https")
	{
		$caFile	= "$PathForFiles/certs/" . $CxIp . "_" . $CxPortNumber . ".crt";
	}
	local $ENV{HTTPS_CA_FILE}	= $caFile if( $urlProtocol eq "https");
	
	my ( $returnCode, $accessSignature )= GenerateAccessSignature( 	requestMethod	=> $requestMethod,
																	accessFile		=> $accessFile,
																	functionName	=> $functionName,
																	requestID		=> $requestID,
																	version			=> $version,
																);
	if( $returnCode == $FAILURE )
	{
		WriteMessage("ERROR :: Failed to generate Access signature.",3);
		return ( $FAILURE );
	}
											
	$userAgent->default_header( 	"HTTP-AUTH-SIGNATURE"	=> $accessSignature,
									"HTTP-AUTH-REQUESTID"	=> $requestID,
									"HTTP-AUTH-PHPAPI-NAME"	=> $functionName,
									"HTTP-AUTH-VERSION"		=> $version, 
								);
	
	my $response 	= $userAgent->request( POST $url , content => $args{requestXml} );
	if ($response->is_success)
	{
		$responseXml	=  $$response{_content};
		$responseXml 	=~ s/^\s+//;
		$responseXml 	=~ s/\s+$//;
	}
	else
	{
		my $errorMesg	= $response->error_as_HTML;
		WriteMessage("Post Request XML :: ERROR :: $errorMesg.",3);
		$ReturnCode 	= $FAILURE;
	}
	undef $userAgent;
	return ( $ReturnCode , $responseXml )
}

#####DownloadUsingCxps#####
##Description	:: Download files from CX using CXPS.
##Input 		:: File(with complete path), Directory on CX from where file has to be downloaded.
##Output 		:: Return SUCCESS else FAILURE.
#####DownloadUsingCxps#####
sub DownloadUsingCxps
{
	my %args 		= @_;
	my $ReturnCode  = $SUCCESS;
	
	my $command 	= "\"$vxAgentInstallationDir/cxpsclient.exe\" -i \"$args{cxIp}\" --cscreds --get \"$args{filePath}/$args{file}\" -L \"$args{dirPath}/$args{file}\" -C --secure -c \"$vxAgentInstallationDir/transport/client.pem\" ";
	WriteMessage("Executing Command $command.",3);
	my $returnCode 	= system ( $command );
	$returnCode 	= $returnCode >> 8;
	if ( 0 != $returnCode )
	{
		WriteMessage("ERROR :: Failed to download file( $args{filePath}/$args{file} ) from CX ( CX IP = $args{cxIp} ).",3);
		$ReturnCode	= $FAILURE;
	}
	return ( $ReturnCode );
}

sub DownloadFilesFromCX
{
	my %args		= @_;
	my $returnCode 	= $SUCCESS;
	
	#first we will download file. 
	#if download Sub Files option is choosen, file will be read and each sub file will be downloaded.
	#if any failure in either downloading the file or sub-files will be treated as failure.
	my $rCode 		= DownloadUsingCxps( 'file' => "$args{file}" , 'filePath' => "$args{filePath}", 'dirPath' => "$args{dirPath}" , 'cxIp' => $args{cxIp} );
	if ( $rCode != $SUCCESS )
	{
		$returnCode	= $FAILURE;
	}
	
	if ( ( $returnCode == $SUCCESS ) && ( "yes" eq lc( $args{downloadSubFiles} ) ) )
	{
		#Now we need to check whether File exists in the path? Not Zero in size.
		if (!( ( -e "$args{dirPath}/$args{file}" ) && ( -f "$args{dirPath}/$args{file}" ) && ( ! -z "$args{dirPath}/$args{file}" ) ) )
		{
			$returnCode	= $FAILURE;
		}
		else
		{
			#now we need to read file, download each and every file from CX.
			if ( !open( SUBFILES , "$args{dirPath}/$args{file}" ) )
			{
				Common_Utils::WriteMessage("ERROR :: Failed to open \"$args{dirPath}/$args{file}\". please check whether user has permission to access it.",3);
				$returnCode = $FAILURE;
			} 
			else
			{
				my @subFiles 	= <SUBFILES>;
				foreach my $file ( @subFiles )
				{
					if($file =~ /^$/)
					{
						next;
					}
					$file =~ s/\n//;
					$file =~ s/\r//;
					$file =~ s/\s+//g;
					$rCode 	= DownloadUsingCxps( 'file' => "$file" , 'filePath' => "$args{filePath}" , 'dirPath' => "$args{dirPath}" , 'cxIp' => $args{cxIp} );
					if ( $rCode != $SUCCESS )
					{
						$returnCode	= $FAILURE;
						last;
					}
				}
			}
		}
	}
	
	return $returnCode;
}

#####UploadUsingCsps#####
##Description	:: Uploads files to CX using CSPS.
##Input 		:: File(with complete path), Directory on CX where file has to be placed.
##Output 		:: Return SUCCESS else FAILURE.
#####UploadUsingCsps#####
sub UploadUsingCsps
{
	my %args 		= @_;
	my $ReturnCode  = $SUCCESS;
	
	my $command 	= "\"$vxAgentInstallationDir/cxpsclient.exe\" -i $args{cxIp} --cscreds --put \"$args{file}\" -d \"$args{dirPath}\" -C --secure -c \"$vxAgentInstallationDir/transport/client.pem\" ";
	WriteMessage("Executing Command $command.",3);
	my $returnCode 	= system ( $command );
	$returnCode 	= $returnCode >> 8;
	if ( 0 != $returnCode )
	{
		WriteMessage("ERROR :: Failed to upload file( $args{file} ) to CX ( CX IP = $args{cxIp} ).",3);
		$ReturnCode	= $FAILURE;
	}
	return ( $ReturnCode );
}

#####UpdateTasksStatus#####
##Description	:: Updates task status.
##Input 		:: Task information fetched from CX and updated Task information.
##Output 		:: Return Updated Hash.
#####UpdateTasksStatus#####
sub UpdateTasksStatus
{
	my %args 	= @_;
	
	foreach my $update ( sort keys %{$args{updatedTasksStatus}} )
	{
		foreach my $updateIn ( sort keys %{$args{origTasksStatus}} )
		{
			if ( lc( $update ) ne lc( $updateIn ) )
			{
				next;
			}
			if ( ref( $args{updatedTasksStatus}->{$update} ) eq ref( $args{origTasksStatus}->{$updateIn} ) )
			{
				my $updatedHash = UpdateHash( update => $args{updatedTasksStatus}->{$update} , tobeUpdatedIn => $args{origTasksStatus}->{$updateIn} );
				$args{origTasksStatus}->{$updateIn} 	= $updatedHash;
			}
			elsif ( "" eq ref( $args{updatedTasksStatus}->{$update} ) )
			{
				$args{origTasksStatus}->{$update} = $args{updatedTasksStatus}->{$update};
			}
		}
	}
	return ( $SUCCESS , $args{origTasksStatus} );
}

#####UpdateHash#####
##Description 	:: Compares two hashes passed and updates all the changes/updates if a hash variable has meaningful data.
##					All the updates will be overwitten in Original hash.
##Input 		:: Hash to which updates are to be posted and Updates Hash.
##Output 		:: Returns Updated Hash.
#####UpdateHash#####
sub UpdateHash
{
	my %args = @_;
	my %hash = ();
	foreach my $update ( sort keys %{$args{update}} )
	{
		foreach my $updateIn ( sort keys %{$args{tobeUpdatedIn}} )
		{
			if ( lc( $update ) ne lc( $updateIn ) )
			{
				next;
			}
			if ( ( ref( $args{update}->{$update} ) eq ref( $args{tobeUpdatedIn}->{$updateIn} ) ) && ( "HASH" eq ref( $args{update}->{$update} ) ) )
			{
				my $updatedHash = UpdateHash( update => $args{update}->{$update} , tobeUpdatedIn => $args{tobeUpdatedIn}->{$updateIn} );
				$args{tobeUpdatedIn}->{$updateIn} 	= $updatedHash;
			}
			elsif ( "" eq ref( $args{update}->{$update} ) )
			{
				$args{tobeUpdatedIn}->{$update} = $args{update}->{$update};
			}
		}
	}
	return ( $args{tobeUpdatedIn} );
} 

#####FindTask#####
##Description	:: It finds the complete path for a task from the hash.
##Ex			:: Step1->TaskList->Task4{taskName => "Power On"}.
##Input 		:: All Steps information read in response of CX query, 
##					and Task information for which Path has to be found out.
##Output 		:: Retuns a HASH on SUCCESS else FAILURE.   
#####FindTask#####
sub FindTask
{
	my %args 				= @_;
	my %stepWiseTaskInfo 	= ();
	my %tasksList 			= %{$args{stepsInfo}};
	my %taskUpdate			= %{$args{taskInfo}};

	if( ( exists $tasksList{Step1}{TaskList} ))
	{
		foreach my $hash ( sort keys %{$tasksList{Step1}{TaskList}} )
		{				
			if ( ( exists $tasksList{Step1}{TaskList}{$hash}{TaskName} ) && ( lc($taskUpdate{TaskName}) eq lc($tasksList{Step1}{TaskList}{$hash}{TaskName}) ) )
			{
				$stepWiseTaskInfo{Step1}{TaskList}{$hash}	= \%taskUpdate;
				WriteMessage("Task number = $hash.",2);
			}
		}
	}
	return ( $SUCCESS, \%stepWiseTaskInfo);	
}

#####generateUuid#####
##Description	:: generate the 128-bit UUID as a hexadecimal string ("12345678-abcd-1234-cdef-123456789abc").
##Input 		:: None.
##Output 		:: Return uuid srting.
#####generateUuid#####
sub generateUuid
{	
	my $uuid =	Digest::MD5::md5_hex( rand() );
	return ( $uuid );
}

#####GetViewProperty#####
##Description	:: To get the view property.
##Input 		:: view and path.
##Output 		:: Return the value of the property.
#####GetViewProperty#####
sub GetViewProperty
{
	my ($view, $link) = @_;
	my @subLinks = ();
	my $val;
	while ($link) 
	{
		if (exists $view->{$link}) 
		{
			$val = $view->{$link};
			last;
			
		} 
		elsif ($link =~ /^(.+)\.([^.]+)$/) 
		{
			$link = $1;
			unshift @subLinks, $2;
		}
		else
		{
			$val = undef;
			last;
		}
	}
	if (defined($val)) 
	{
		foreach (@subLinks) 
		{
			$val = $val->{$_};
		}
	}
	return $val;
}

#####IsPropertyValueTrue#####
##Description 		:: This function determines whether view property value is true or false.
##Input 			:: view and property.
##Output 			:: Returns 1 if true , else 0.
#####IsPropertyValueTrue#####
sub IsPropertyValueTrue
{
	my $view		= shift;
	my $property	= shift;
	my $isItTrue	= 0;
	my $propVal		= GetViewProperty( $view, $property );
	my %trueHash	= ( "true" => 1, 1 => 1 );
	if ( (defined( $propVal )) && ( exists $trueHash{$propVal} ) )
	{
		$isItTrue	= 1;
	}		
	
	return ( $isItTrue );
}

#####GetHostInfoFromCx#####
##Description	:: To Get the host info from the CX based on hostid.
##Input 		:: host id.
##Output 		:: Returns SUCCESS on successful getting info from CX and hostinfo, else FAILURE.  					
#####GetHostInfoFromCx#####
sub GetHostInfoFromCx
{
	my %args 		= @_;

	if ( "" eq $CxIpPortNumber )
	{
		my( $returnCode , $cxIpPortNum )= FindCXIP();
		if ( $returnCode != $SUCCESS )
		{
			WriteMessage("ERROR :: Failed to find CX IP and Port Number.",3);
			return ( $FAILURE );
		}
	}
		
	my %subParameters	= ( InformationType => "all", HostGUID => $args{hostId} );
	my( $returnCode	, $requestXml )	= XmlFunctions::ConstructXmlForCxApi( functionName => "GetHostInfo" , subParameters => \%subParameters );
	if ( $returnCode != $SUCCESS )
	{
		WriteMessage("ERROR :: Failed to prepare XML file.",3);
		return ( $FAILURE );
	}

	( $returnCode ,my $responseXml )= PostRequestXml( requestXml => $requestXml , cxIp => $CxIpPortNumber, functionName => "GetHostInfo" );
	if ( $returnCode != $SUCCESS )
	{
		return ( $FAILURE );
	}

	( $returnCode , my $hostInfo )	= XmlFunctions::ReadResponseOfCxApi( response => $responseXml , parameter =>"yes" , parameterGroup => 'all' );		
	if ( $returnCode != $SUCCESS )
	{
		WriteMessage("ERROR :: Failed to read response of CX API.",3);
		return ( $FAILURE );
	}
	
	return ( $SUCCESS, $hostInfo );
}

#####UpdateViewProperties#####
##Description 		:: It will update the object/view with the given properties.
##Input 			:: view and properties.
##Output 			:: Returns updated view.
#####UpdateViewProperties#####
sub UpdateViewProperties
{
	my %args 		= @_;
	
	if( ref($args{view}) eq "HostSystem" )
	{
		my $hostProps	= HostOpts::GetHostProps();
		my @props		= @$hostProps;
		push @props,@{$args{properties}};
		$args{view}->ViewBase::update_view_data( \@props );
	}
	else
	{
		WriteMessage("Updating view implementation is required for ref($args{view}).",2);
	}
	return $args{view};
}

#####ReadSingleLineFile#####
##Description 		:: It will read the given file and return the content.
##Input 			:: file name with full path.
##Output 			:: Returns content with return code.
#####UpdateViewProperties#####
sub ReadSingleLineFile
{
	my $fileName	= shift;
	
	if ( !open( FH , $fileName ) )
	{
		WriteMessage("ERROR :: Failed to open \" $fileName \" .",3);
		return ( $FAILURE );
	} 
		
	my @lines 		= <FH>;
	close FH;
	
	my $word	= "";
	foreach my $line ( @lines )
	{
		if($line =~ /^$/)
		{
		}
		else
		{
			$line 	=~ s/\n//;
			$line 	=~ s/\r//;
			$word	= $line;
			last;
		}
	}
	
	if( $word =~ /^$/ )
	{
		WriteMessage("ERROR :: file $fileName is empty.",3);
		return ( $FAILURE );
	}
	
	return ( $SUCCESS, $word );
}

#####ReadSingleLineFile#####
##Description 		:: It will the permissions to the given file.
##Input 			:: file name with full path.
##Output 			:: Returns Success or Failure.
#####UpdateViewProperties#####
sub SetFilePermissions
{
	my $fileName	= shift;
	my $ReturnCode  = $SUCCESS;
	
	my $command 	= 'icacls.exe "' . $fileName . '" /inheritance:r /grant:r "BUILTIN\Administrators:F" /grant:r "NT AUTHORITY\SYSTEM:F" ';
	WriteMessage("Executing Command $command.",2);
	my $output 	= `$command`;
	WriteMessage("$output.",2);
	if ( 0 != $? )
	{
		WriteMessage("ERROR :: Failed to give permissions to the file $fileName.",3);
		$ReturnCode	= $FAILURE;
	}
	return ( $ReturnCode );
}

#####SetLogPaths#####
##Description 		:: File paths for different components like CX.
##Input 			:: file paths.
##Output 			:: Returns None.
#####SetLogPaths#####
sub SetLogPaths
{
	my $component	= shift;
	
	if( $component == "cx" )
	{
		my $parentDir 			= File::Basename::dirname(File::Path::getcwd());		
		$vContinuumInstDirPath	= File::Basename::dirname( $parentDir );#"/home/svsystems";
		$vContinuumLogsPath		= "$vContinuumInstDirPath/var/vcon/logs";
		$vContinuumScriptsPath	= "$vContinuumInstDirPath/pm/Cloud/Scripts";
		$vContinuumMetaFilesPath= "$vContinuumInstDirPath/var/esx_discovery_data";
		$vContinuumPlanFilesPath= "$vContinuumInstDirPath/var/vcon/logs" ;
		$vxAgentInstallationDir = "$vContinuumInstDirPath/bin";
		$planTempLogFile		= "$vContinuumLogsPath/Scripts_Plan_Temp.log";
	}	
}

#####EncodeNameSpecialChars#####
##Description 		:: For Entity names having special characters, while retrieving getting with decoded characters,
##						Have to be encoded to query.
##Input 			:: file paths.
##Output 			:: Returns None.
#####EncodeNameSpecialChars#####
sub EncodeNameSpecialChars
{
	my $name	= shift;
	
	$name	=~ s/%25/%/g ;
	$name	=~ s/&amp;/\&/g ;
	$name	=~ s/%2f/\//g ;
	$name	=~ s/%5c/\\/g ;
	$name	=~ s/&lt;/</g ;
	$name	=~ s/&gt;/>/g ;
	$name	=~ s/&quot;/\"/g ;
	
	return $name;
}

#####LogRoation#####
##Description 		:: Moving the log if exceeding 10MB to zip folder.
##Input 			:: file name.
##Output 			:: Returns None.
#####LogRoation#####
sub LogRoation
{
	my $fileName		= shift;
	my $zipFolderName	= "$vContinuumLogsPath/vContinuum_Scripts.zip";
	
	unless ( -f $fileName )
	{
#		WriteMessage( "$fileName not found for log rotation.",2);
		return;
	}
	
	my $fileSize = -s $fileName;
		
	unless( $fileSize > 10485760 )
	{
#		WriteMessage( "$fileName size is $fileSize bytes.",2);
		return;
	}
	
# For vcon keeping logs for 100 days, for CX keeping 15days	
	my $daysLimit	= 86400 * 100;
	if( $COMPONENT eq "cx" )
	{
		$daysLimit	= 86400 * 15;
	}
	
	my $slashIndex		= rindex( $fileName , "/" );
	my @splitFileName	= split( /\./, substr( $fileName,  $slashIndex + 1 ));
	my $newFileName 	= $splitFileName[0] . "_" . Time::Local::timelocal( localtime() ) . "." . $splitFileName[1];
	
	eval
	{			
		my $zipObj = Archive::Zip->new();
		
		unless( -f $zipFolderName)
		{
			unless ( $zipObj->writeToFileNamed($zipFolderName) == AZ_OK ) 
			{
#				   WriteMessage("$zipFolderName write error.",2);
				   return;
			}
		}
		
		unless ( $zipObj->read( $zipFolderName ) == AZ_OK ) 
		{
#			WriteMessage("$zipFolderName read error.",2);
			return;
		}
				
		my @zippedFiles = $zipObj->memberNames();
		foreach my $memFile ( @zippedFiles )
		{
			my $usIndex			= rindex( $memFile , "_" );
			my $dotIndex		= rindex( $memFile, "." );
			my @timeStamp		= split( /\./, substr( $memFile,  $usIndex + 1, $dotIndex + 1 ) );
			my $currentTS		= Time::Local::timelocal( localtime() );
			if( ( $currentTS - $timeStamp[0] ) > $daysLimit )
			{
				$zipObj->removeMember( $memFile );
			}
		}

		$zipObj->addFile( $fileName, $newFileName );
			
		$zipObj->overwrite();
		
		unlink $fileName;
	};
	if( $@ )
	{
#		WriteMessage("Log rotation failed.",2);
	}
}

#####GenerateAccessSignature#####
##Description 		:: Generating access signature for CX API.
##Input 			:: requestMethod, accessFile, functionName, requestID and version.
##Output 			:: Returns Access signature on Success else Failure.
#####GenerateAccessSignature#####
sub GenerateAccessSignature
{
	my %args 			= @_;
		
#	my $delimiter 	= ':';
#	
#	my ( $returnCode, $passPhrase )		= ReadSingleLineFile( "$PathForFiles/private/connection.passphrase" );
#	if( $returnCode == $FAILURE )
#	{
#		WriteMessage("ERROR :: Failed to read passphrase file.",3);
#		return ( $FAILURE );
#	}
#	
#	my $stringToSign	= $args{requestMethod}.$delimiter.$args{accessFile}.$delimiter.$args{functionName}.$delimiter.$args{requestID}.$delimiter.$args{version};
#	
#	require Digest::SHA; 
#	Digest::SHA->import( qw( sha256_hex hmac_sha256_hex ) );
#	
#	my $passPhraseHash	= Digest::SHA::sha256_hex($passPhrase);
#	
#	my $fingerPrintHMAC = '';
#	if( $Common_Utils::urlProtocol eq "https" )
#	{
#		my $fingerPrintFileName	= $CxIp . "_" . $CxPortNumber . ".fingerprint";
#		( $returnCode, my $fingerPrint )	= ReadSingleLineFile( "$PathForFiles/fingerprints/$fingerPrintFileName" );
#		if( $returnCode == $FAILURE )
#		{
#			WriteMessage("ERROR :: Failed to read fingerprint file.",3);
#			return ( $FAILURE );
#		}
#	
#		$fingerPrintHMAC = Digest::SHA::hmac_sha256_hex( $fingerPrint, $passPhraseHash );
#	}
#	
#	my $stringToSignHMAC = Digest::SHA::hmac_sha256_hex($stringToSign, $passPhraseHash);
#	
#	my $signature	= $fingerPrintHMAC.$delimiter. $stringToSignHMAC;
#	
#	my $accessSignature = Digest::SHA::hmac_sha256_hex($signature, $passPhraseHash );

	my $signFile	=  "$vxAgentInstallationDir/AccessSignTemp_" . Time::Local::timelocal( localtime() ) . ".txt";
	my $command 	= "\"$vxAgentInstallationDir/EsxUtil.exe\" -generatesig -cxapifuncname $args{functionName} -file \"$signFile\" -requestmethod $args{requestMethod} -cxapiaccessfile $args{accessFile} -requestid $args{requestID} -version $args{version} ";
#	WriteMessage("Executing Command $command.",3);
	my $returnCode 	= system ( $command );
	$returnCode 	= $returnCode >> 8;
	if ( 0 != $returnCode )
	{
		WriteMessage("ERROR :: Access signature generating is failed.",3);
		return ( $FAILURE );
	}
	
	( $returnCode, my $accessSignature )	= ReadSingleLineFile( $signFile );
	if( $returnCode == $FAILURE )
	{
		WriteMessage("ERROR :: Failed to read file $signFile.",3);
		return ( $FAILURE );
	}
	
	unlink $signFile;
	
	return ( $SUCCESS, $accessSignature );
}

#####FindvCliVersion#####
##Description 		:: Finds the vCli Version.
##Input 			:: None.
##Output 			:: vCli version.
#####FindvCliVersion#####
sub FindvCliVersion
{
	my $vCliVersion	= $VMware::VIRuntime::VERSION;
	my $perlVersion	= $];
	my $version		= substr( $vCliVersion, 0, rindex( $vCliVersion, "." ) );
	if( ( $vCliVersion ge "5.5.0" ) && ( $vCliVersion lt "6.0.0" ) && ( $perlVersion ge "5.009" ) )
	{
		$version	.= "U2";
	}
	
	return $version;
}

1;