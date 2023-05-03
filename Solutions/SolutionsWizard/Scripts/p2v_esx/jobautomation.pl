use FindBin qw( $Bin );
use lib $FindBin::Bin;
use Common_Utils;
use Getopt::Long;
use XmlFunctions;

my $plan_name = "";
my $batch_resync = "";
my $failback;
my $VconMT;
my $scsiUnit;

my $optsValidate = GetOptions( 'failback=s'=>\$failback, 'vconmt=s'=>\$VconMT, 'scsiunit=s'=>\$scsiUnit,"help" => sub { HelpMessage() } );

#####getDesireddataForRequiredHostids#####
##description :: to get required data from cx output by comaring with esx.xml.
##input :: hostids, name of nodes , vx installation path and fx installation path from cxcli call.
##output :: returns  success along with hostids, name of nodes , vx installation path 
##			and fx installation path for rquired data else $Common_Utils::FAILURE.
#####getDesireddataForRequiredHostids#####
sub getDesireddataForRequiredHostids
{
	my $src_node_data		= shift;
	my $nodesInCx			= shift;
	my @requiredData		= ();
	my %clusterMap			= ();
	
	foreach my $cxNode ( @$nodesInCx )
	{
		foreach my $srcNode ( @$src_node_data )
		{
			my %requiredData	= ();
			if( $$cxNode{hostId} eq $$srcNode{hostId})
			{
				$requiredData{hostId}		= $$cxNode{hostId};
				$requiredData{hostName}		= $$cxNode{hostName};
				$requiredData{osType}		= $$srcNode{osType};
				$requiredData{guestId}		= $$srcNode{guestId};
				$requiredData{machineType}	= $$srcNode{machineType};
				$requiredData{consistencyTime} = $$srcNode{consistencyTime};
				$requiredData{cluster}		= $$srcNode{cluster};
						
				if( $$srcNode{cluster} )
				{					
					my $clusterName				= $$srcNode{clusterName};
					$requiredData{ipAddress}	= $$cxNode{ipAddress};
					$requiredData{clusterName}	= $clusterName;
					
					my $nodeIp	= $$cxNode{ipAddress};
					my @ipAddrs = split( ",", $$cxNode{ipAddresses} );
					foreach my $ipAddr ( @ipAddrs )
					{
						if( $nodeIp =~ /$$srcNode{virtualIps}/)
						{
							$nodeIp	= $ipAddr;
							next;
						}
						last;
					}
					
					unless( defined $clusterMap{$clusterName} )
					{
						$clusterMap{$clusterName}{masterIp}	= $nodeIp;						
					}
					else
					{
						$clusterMap{$clusterName}{clientIps}	.= $nodeIp . ";";
					}
					$requiredData{masterIp}		= $clusterMap{$clusterName};				
				}
				
				if(( $$cxNode{vxPath} ne "" ) && ( $$cxNode{fxPath} ne "" ))
				{
					$requiredData{vxPath}	= $$cxNode{vxPath};
					$requiredData{fxPath}	= $$cxNode{fxPath};
				}
				else
				{
					Common_Utils::WriteMessage("ERROR ::Failed to find Vx/Fx install path for \"$$cxNode{hostName}\" .",3);
					return $Common_Utils::FAILURE;
				}
				push @requiredData, \%requiredData;
				last;
			}
		 }		 
	}
	return ($Common_Utils::SUCCESS, \@requiredData, \%clusterMap ); 	
}

#####setConsitencyJobForProtectedVms#####
##description : to set the consistency fx job for selected source hosts.
##input : host ids , hostnames, fxpaths, vxpaths and mthostname. 
##output : returns xml srting on success else failure.
#####setConsitencyJobForProtectedVms#####
sub setConsitencyJobForProtectedVms
{
	my $requiredData 			= shift;
	my $MT_Host_Name			= shift;
	my $clusterMap				= shift;
	my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
	my $job_Group 				= $plan_name."-Consistency".$sec.$min;
	my $inmageInstPath			= "";
	my $SourceSideDirectoryPath = "";
	my $sourceId 				= "";
	my $preScript				= "";
	my $job_Name 				= "";
	my $XML_Msg 				= "";
	
	sleep(1); #Sleeping for uniqueness
	foreach my $reqData ( @$requiredData )
	{
		$sourceId 		 			= $$reqData{hostId};			
		if( "windows" eq lc($$reqData{osType}) )
		{
			$inmageInstPath  		 = $$reqData{fxPath};
			$preScript 			 	 = "\"$inmageInstPath\\vacp.exe\" -systemlevel";
			$job_Name  			 	 = $$reqData{hostName}." --- Consistency";
			$SourceSideDirectoryPath = $inmageInstPath.'Failover\\Data';
			if( $$reqData{guestId} =~ /winXP/i )
			{
				$preScript 			 	 = "\"$inmageInstPath\\vacpxp.exe\" -systemlevel";
			}
			if( $$reqData{guestId} =~ /win2000/i )
			{
				$preScript 			 	 = "\"$inmageInstPath\\vacp2k.exe\" -systemlevel";
			}
			
			if( $$reqData{cluster} )
			{
				$preScript 			 	 = "\"$inmageInstPath\\vacp.exe\" -systemlevel -distributed -mn \"" . $$clusterMap{$$reqData{clusterName}}{masterIp} . 
												"\" -cn \"" . $$clusterMap{$$reqData{clusterName}}{clientIps} . "\"";
			}
		}
		elsif( "solaris" eq lc($$reqData{osType}) )
		{
			$inmageInstPath = $$reqData{vxPath};
			$preScript = "\"$inmageInstPath"."/bin/vacp\" -systemlevel";
			$job_Name  =  $$reqData{hostName}." --- Consistency";
			$SourceSideDirectoryPath = $$reqData{fxPath}."/failover_data";
		}
		else
		{
			$inmageInstPath = $$reqData{vxPath};
			$preScript = "\"$inmageInstPath"."/bin/EsxUtil\" -applytag -t $MT_Host_Name -tag \"$plan_name\"";
			$job_Name  =  $$reqData{hostName}." --- Consistency";
			if( "physicalmachine" eq lc( $$reqData{machineType} ))
			{
				$preScript = "\"$inmageInstPath"."/bin/EsxUtil\" -applytag -t $MT_Host_Name -tag \"$plan_name\" -p2v";
			}
			$SourceSideDirectoryPath = $$reqData{fxPath}."/failover_data";
		}
		
		my $XML_Msg_local 			 = createXML(
												JobName	=> $job_Name, SourceDir => $SourceSideDirectoryPath, TargetDir => $SourceSideDirectoryPath , 
												SourceId => $sourceId, TargetId => $sourceId, ScheduleType => "run-every",RunOrder => 0 ,
												Schedule_Time => $$reqData{consistencyTime}, IncludeFiles => "", ExcludeFiles => "*", FileDeletionAtDestination=> "No",
												PreScriptSrc => $preScript, PostScriptSrc => "", PreSciptTgt => "", PostScriptTgt =>"" , load_utilization_on_host =>"push" ,
												JobType => "Consistency");	 
		$preScript 					 = "";
		$sourceId			 		 = "";
		$SourceSideDirectoryPath	 = "";
		$inmageInstPath				 = "";	
		$XML_Msg 					 = $XML_Msg.$XML_Msg_local;
		
    }
    #here call the create_complete_xml_message to create the consistency message.
   	$XML_Msg 							 = create_complete_xml_message(2,$job_Group,$XML_Msg);
   	
   	return $XML_Msg;
}

#####create_complete_xml_message#####
##description : append's plan name and application information to Fx job's message.
##input : job-group,String saying that it is protection,consistency,recovery.
##output : returns complete xml as string.
#####create_complete_xml_message#####
sub create_complete_xml_message
{
	my $job				= shift;
	my $job_Group 		= shift;
	my $partial_message = shift;
	#here add the plan name and application group to message.
	my $message = "<plan>
				   <header>
					 <parameters>
					   <param name='name'>".$plan_name."</param>
					   <param name='type'>DR Machines</param>
					   <param name='version'>1.1</param>
					 </parameters>
					</header>
					<!-- Give the tag value \"N \" to activate later -->
					<activate_now></activate_now>
					   <data>
						 <![CDATA[ Serialized String ]]>
					   </data>
					 <application_options name=\"".$job_Group."\">
						 <batch_resync>".$batch_resync."</batch_resync>
					</application_options>
					   <failover_protection>
						 <replication>".$partial_message." </replication>
					   </failover_protection>
				 </plan>";
						
	return $message;
}
 
#####send_combined_message_to_cx#####
##description : post the xml string to CX for setting fx job.
##input : cxip with portno and xml string. 
##output : returns success else failure.
#####send_combined_message_to_cx#####
sub send_combined_message_to_cx
{
	my $CX_IP 				= shift;
	my $concatenated_msg 	= shift;
	
	my ( $returnCode, $persistData)	= Common_Utils::PostCxCli( form => { "operation" => 42, Content_Type => 'xml', xml => $concatenated_msg } );
	if( $returnCode == $Common_Utils::FAILURE )
	{
		Common_Utils::WriteMessage("ERROR :: GetHostNodes CX CLI call is failed.",3);
		return ( $Common_Utils::FAILURE );		
	}

	if( $persistData =~ /^$/i )
	{
		Common_Utils::WriteMessage("ERROR :: Empty response from CX_CLI call.",3);
		return ( $Common_Utils::FAILURE );
	}
	elsif( $persistData =~ /<response_code>FAIL<response_code>/i  || $persistData =~ /Failed to Create/i || $persistData =~ /Failed to Configure/i )
	{
		Common_Utils::WriteMessage("ERROR :: Response code from CX CLI is",3);
		Common_Utils::WriteMessage($persistData,3);
		return ( $Common_Utils::FAILURE );
	}
	
	return $Common_Utils::SUCCESS;
}

#####read_data_from_xml#####
##description : Reads data from ESX.xml file.
##input : esx.xml file name with full path. 
##output : returns success else failure.
#####read_data_from_xml#####
sub read_data_from_xml
{		
	my $file 		= shift;
	my $parser 		= XML::LibXML->new();
	my $tree 		= $parser->parse_file($file);
	my $root 		= $tree->getDocumentElement;
	$plan_name 		= $root->getAttribute('plan');
	$batch_resync 	= $root->getAttribute('batchresync');
	$Common_Utils::vContinuumPlanDir= $root->getAttribute('xmlDirectoryName');
	my @SRC_ESX 	= $root->getElementsByTagName('SRC_ESX');

	if( $plan_name eq "" )
	{
		Common_Utils::WriteMessage("ERROR :: Value of attribte plan_name in XML is empty.",3);
		Common_Utils::WriteMessage("ERROR :: XML is in Un-expected format.",3);
		return $Common_Utils::FAILURE;
	}
	
	my @src_nodes_data 			= ();
	my %mapClusterVirtualIps	= ();
	foreach  (@SRC_ESX) 
	{
		my @child_host_SRC_ESX = $_->getElementsByTagName('host');
		foreach  (@child_host_SRC_ESX) 
		{
			if( "yes" eq $_->getAttribute('cluster') )
			{
				my $clusterName	= $_->getAttribute('cluster_name');
				my @networkCards	= $_->getElementsByTagName('nic');
				foreach my $networkAdapter ( @networkCards )
				{
					$mapClusterVirtualIps{$clusterName}	.= $networkAdapter->getAttribute('virtual_ips') . ",";				
				}
			}			
		}
		foreach  (@child_host_SRC_ESX) 
		{
			my %src_node_data		= ();
			$src_node_data{displayName} = $_->getAttribute('display_name');
			$src_node_data{hostId}		= $_->getAttribute('inmage_hostid');
			if( !(defined $src_node_data{hostId}) || ( $src_node_data{hostId} =~ /^$/ ) )
			{
				Common_Utils::WriteMessage("ERROR :: Unable to set Consistency job for \"$src_node_data{displayName}\" as inmage_hostid is not defined .",3);
				return ( $Common_Utils::FAILURE );
			}
			$src_node_data{machineType}	= $_->getAttribute('machinetype');
			$src_node_data{osType}		= $_->getAttribute('os_info');
			$src_node_data{guestId}		= $_->getAttribute('alt_guest_name');
			my @consistency_schedule 	= $_->getElementsByTagName('consistency_schedule');
			my $days 	= $consistency_schedule[0]->getAttribute('days');
			my $hours 	= $consistency_schedule[0]->getAttribute('hours');
			my $minutes = $consistency_schedule[0]->getAttribute('mins');
			if( 1 == length($days))
			{
				$days 	= "0".$days;
			}
			if( 1 == length($hours))
			{
				$hours 	= "0".$hours;
			}
			if( 1 == length($minutes))
			{
				$minutes= "0".$minutes;
			}
			$src_node_data{consistencyTime}	= $days.":".$hours.":".$minutes;
			
			$src_node_data{cluster}	= 0;
			if( "yes" eq $_->getAttribute('cluster') )
			{
				$src_node_data{cluster}	= 1;
				$src_node_data{clusterName}	= $_->getAttribute('cluster_name');
				$src_node_data{virtualIps}	= $mapClusterVirtualIps{ $src_node_data{clusterName} };
			}
			push @src_nodes_data, \%src_node_data;
		}
		
	}
	my @TARGET_ESX 	= $root->getElementsByTagName('TARGET_ESX');
	my @child_host_TARGET_ESX = $TARGET_ESX[0]->getElementsByTagName('host');
	my @subparts = split('\.', lc($child_host_TARGET_ESX[0]->getAttribute('hostname')));
	my $master_target_hostname = $subparts[0];
	return ( $Common_Utils::SUCCESS, \@src_nodes_data, $master_target_hostname );
}

#####createXML#####
##Description 		:: Creates an XML message for FX which will be posted to CX.
##Input 			:: General argumets required by CX in  an FX job.
##Output 			:: Returns the XML message.
#####createXML#####
sub createXML
{
	my %arg 		= @_; #Converts the arguments list in to hash.
	
	my $XML_Msg		= "<job type=\"fx\">
						<server_hostid type=\"source\">".$arg{SourceId}."</server_hostid>
                		<server_hostid type=\"target\">".$arg{TargetId}."</server_hostid>
						<job_name>".$arg{JobName}."</job_name>
						<source_dir>".$arg{SourceDir}."</source_dir>
						<target_dir>".$arg{TargetDir}."</target_dir>
						<schedule type=\"".$arg{ScheduleType}."\" run_order=\"".$arg{RunOrder}."\">".$arg{Schedule_Time}."</schedule>
						<job_options>
							<file_directory_options copy_to_sub_dir=\"direct\" checksum_block_size=\"8192\" whole_files=\"Yes\" backup_directory=\"\" backup_suffix=\"\" compress_files=\"No\"></file_directory_options>
							<inclusion_exclusion_options update_only=\"y\" update_files_exist=\"No\" ignore_files_exist=\"No\" ignore_same_size_tmstmp=\"No\" ignore_same_size_files=\"No\"
								excl_matching_patt=\"".$arg{ExcludeFiles}."\" subset_exclude_list=\"".$arg{IncludeFiles}."\"></inclusion_exclusion_options>
							<file_deletion_options file_not_exist_src=\"".$arg{FileDeletionAtDestination}."\" del_excl_files_src=\"No\" del_files_after_trf=\"No\" keep_part_trf_files=\"No\"></file_deletion_options>
							<link_options preserve_symlink_copy=\"contents\" exclude_symlink_deep_copy=\"ignore\"></link_options>
							<file_detail_options preserve_permissions=\"\" preserve_owner=\"\" preserve_group=\"\" preserve_devices=\"\" preserve_times=\"Yes\"></file_detail_options>
							<misc_options>
								<create_temp_files_dir></create_temp_files_dir>
								<cross_filesystem_boundary></cross_filesystem_boundary>
								<io_timeout></io_timeout>
								<port>874</port>
								<bandwidth_limit></bandwidth_limit>
								<load_utilization_on_host>".$arg{load_utilization_on_host}."</load_utilization_on_host>
								<cpu_throttle>0</cpu_throttle>
								<rpo_alert_interval>0</rpo_alert_interval>
								<email_alert_interval>5</email_alert_interval>
								<pre_script_src>".$arg{PreScriptSrc}."</pre_script_src>
								<post_script_src>".$arg{PostScriptSrc}."</post_script_src>
								<pre_script_tgt>".$arg{PreScriptTgt}."</pre_script_tgt>
								<post_script_tgt>".$arg{PostScriptTgt}."</post_script_tgt>
								<power_user_mode>super</power_user_mode>
							</misc_options>
						</job_options>
						<alert_category>".$arg{JobType}."</alert_category>
					    <alert_on_success>Y</alert_on_success> 
						<alert_on_failure>Y</alert_on_failure>
						</job>";
	
	return $XML_Msg;
}

#####set_consistency_FX_jobs#####
##Description		:: Set's jobs for Consistency.
##Input				:: NONE.
##Output			:: return SUCCESS on successful setting of jobs else Failure.
#####set_consistency_FX_jobs#####
sub set_consistency_FX_jobs
{
	my $xmlFilePath			= "$Common_Utils::vContinuumMetaFilesPath/ESX.xml";
	my ( $return_code, $src_node_data, $master_target_hostname)	= read_data_from_xml($xmlFilePath);
	if ( $Common_Utils::SUCCESS != $return_code )
	{
		return $Common_Utils::FAILURE;
	}
	
	( $return_code, my $CxIpAndPort )	= Common_Utils::FindCXIP();
	if(  $Common_Utils::FAILURE == $return_code || $CxIpAndPort eq "" )
	{
		return $Common_Utils::FAILURE;
	}
	my @nodesInCx		= ();
	( $return_code )	= Common_Utils::GetHostNodesInCX( $CxIpAndPort, \@nodesInCx );
	if( $Common_Utils::SUCCESS != $return_code)
	{
		return $Common_Utils::FAILURE;
	}
	
	( $return_code, my $requiredData, my $clusterMap )	= getDesireddataForRequiredHostids( $src_node_data, \@nodesInCx );
	if( $Common_Utils::SUCCESS != $return_code )
	{
		return $Common_Utils::FAILURE;
	}
	
	my $XML_Msg_Consistency = setConsitencyJobForProtectedVms( $requiredData, $master_target_hostname, $clusterMap );
	Common_Utils::WriteMessage("XML for Consistency : $XML_Msg_Consistency.",2);
	if( $Common_Utils::SUCCESS != send_combined_message_to_cx( $CxIpAndPort, $XML_Msg_Consistency ))
	{
		return $Common_Utils::FAILURE;
	}
	
	Common_Utils::WriteMessage("SUCCESS :: Successfully set Consistency FX Jobs.Please go to CX UI and check for jobs under Monitor->$plan_name",2);	
	return ( $Common_Utils::SUCCESS );
}

#####read_data_from_xml#####
##description : To show the Usage of jobautomation.pl.
##input : none. 
##output : none.
#####read_data_from_xml#####
sub HelpMessage
{
	Common_Utils::WriteMessage("***** Usage of jobautomation.pl *****",1);
	Common_Utils::WriteMessage("-------------------------------",1);
	Common_Utils::WriteMessage("For Consistency Fx jobs --> jobautomation.pl [--vconmt \"yes\"] [--failback \"yes\"] .",1);
	exit( 0 );
}

BEGIN:

unless($optsValidate)
{
	Common_Utils::WriteMessage("ERROR :: Please Enter Valid Options. For help use --help",3);
	exit(1);
}

my $ReturnCode 				= $Common_Utils::FAILURE;
my ( $sec, $min, $hour )	= localtime(time);
my $logFileName 		 	= "vContinuum_Temp_$sec$min$hour.log";

Common_Utils::CreateLogFiles( "$Common_Utils::vContinuumLogsPath/$logFileName" , "$Common_Utils::vContinuumLogsPath/vContinuum_ESX.log" );

if( defined($scsiUnit) )
{
	my $file	= "$Common_Utils::vContinuumMetaFilesPath/$Common_Utils::RemoveXmlFileName";
	$ReturnCode	= XmlFunctions::WriteInmageScsiUnitsFile( file => $file, operation => "remove" );
	if ( $ReturnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to Write InMage SCSI units file.",3);
	}
}
else
{
	$ReturnCode		= set_consistency_FX_jobs();	
	if( $Common_Utils::SUCCESS != $ReturnCode )
	{
		$ReturnCode		= $Common_Utils::FAILURE;
		Common_Utils::WriteMessage("ERROR :: Failed to set Consistency FX jobs.",3);
	}
}

Common_Utils::LoggedData( tempLog => "$Common_Utils::vContinuumLogsPath/$logFileName" , planLog => $Common_Utils::vContinuumPlanDir);
exit( $ReturnCode );

__END__
