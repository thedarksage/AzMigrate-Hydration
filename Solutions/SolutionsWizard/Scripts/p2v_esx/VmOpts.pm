=head
	PackageName : VirtualMachine_Operations.pm
	1. Power ON / Power Off
	2. Virtual Disk Creation/Addition/Removal.
	3. Checking for VMware tools status.
	4. Collecting any other information at VM level.
=cut

package VmOpts;

use strict;
use warnings;
use Common_Utils;
use Data::Dumper;

#####GetVmViews#####
##Description 		:: Gets VM views on vCenter/vSphere.
##Input 			:: None.
##Output 			:: Returns vmView on SUCCESS else Failure.
#####GetVmViews#####
sub GetVmViews
{
	my $vmViews 	= Vim::find_entity_views( view_type => 'VirtualMachine' );
	
	unless ( @$vmViews )
	{
		Common_Utils::WriteMessage("ERROR :: No Virtual machines found on vCenter/vSphere. Please check any VMs exists or not.",3);
		Common_Utils::WriteMessage("ERROR :: Failed to get virtual machine views from vCenter/vSphere.",3);
		return ( $Common_Utils::FAILURE , $vmViews );
	}
	
	return ( $Common_Utils::SUCCESS , $vmViews  );
}

#####GetVmViewsByProps#####
##Description 		:: Gets VM views on vCenter/vSphere.
##Input 			:: None.
##Output 			:: Returns vmView on SUCCESS else Failure.
#####GetVmViewsByProps#####
sub GetVmViewsByProps
{
	my $vmViews;
	eval
	{
		$vmViews 	= Vim::find_entity_views( view_type => 'VirtualMachine', properties => GetVmProps() );
	};
	if( $@ )
	{
		Common_Utils::WriteMessage("Failed to get virtual machine views by poperties from vCenter/vSphere : \n $@.",3);
		return ( $Common_Utils::FAILURE , $vmViews );
	}
	
	unless ( @$vmViews )
	{
		Common_Utils::WriteMessage("ERROR :: No virtual machines found on vCenter/vSphere .",3);
		return ( $Common_Utils::NOVMSFOUND, $vmViews );
	}
	
	return ( $Common_Utils::SUCCESS , $vmViews  );
}

#####GetVmProps#####
##Description 	:: To get the Virtual Machine properties to be collected.
##Input 		:: None.
##Output 		:: Virtual Machine properties.
#####GetVmProps#####
sub GetVmProps
{
	my @vmProps = ( "name", "config.extraConfig", "config.guestFullName", "config.hardware.device","config.version",
					"guest.guestState","guest.hostName", "guest.ipAddress", "guest.net", "layout.disk", "parent", 
					"resourcePool", "snapshot", "summary.config.ftInfo", "summary.config.guestFullName", 'summary.config.instanceUuid',
					"summary.config.guestId", "summary.config.memorySizeMB", "summary.config.numCpu", "summary.config.numVirtualDisks",
					"summary.config.numEthernetCards", "summary.config.template", "summary.config.uuid", "summary.config.vmPathName", 
					"summary.guest.toolsStatus", "summary.runtime.connectionState", "summary.runtime.host", "summary.runtime.powerState");
	
	my $serviceContent	= Vim::get_service_content();
	my $apiVersion		= $serviceContent->about->apiVersion;
	
	if( $apiVersion ge "4.1" )
	{
		push @vmProps, ( "guest.ipStack", "parentVApp" );
	}
	
	if( $apiVersion ge "5.0")
	{
		push @vmProps, ( "config.firmware" );
	}
	return \@vmProps;
}

#####AddVmdk#####
##Description 	:: Adds VMDK which is already created.
##Input 		:: Disk information like, diskMode , vmdk name , thin or thick , Size , SCSI Contoller Key and Unit number.
##Output 		:: Returns SUCCESS or FAILURE.
#####AddVmdk#####
sub AddVmdk2
{
 	my %args 				= @_;
 	
 	Common_Utils::WriteMessage("Adding disk $args{fileName} of size $args{fileSize} at SCSI unit number $args{unitNumber} of type $args{diskType} in mode $args{diskMode}.",2);
	my $diskBackingInfo		= VirtualDiskFlatVer2BackingInfo->new( diskMode => $args{diskMode} , fileName => $args{fileName} ,
																	thinProvisioned =>$args{diskType} );
	if(( exists $args{clusterDisk} ) && ("yes" eq $args{clusterDisk} ))
	{
		$diskBackingInfo	= VirtualDiskFlatVer2BackingInfo->new( diskMode => $args{diskMode}, fileName => $args{fileName},
																	eagerlyScrub => "true", thinProvisioned => "false" );
	}																	
	my $connectionInfo		= VirtualDeviceConnectInfo->new( allowGuestControl => "false" , connected => "true" ,startConnected => "true" );
	my $virtualDisk			= VirtualDisk->new( controllerKey => $args{controllerKey} , unitNumber => $args{unitNumber} ,key => -1, 
												backing => $diskBackingInfo, capacityInKB => $args{fileSize} );
												
	my $virtualDeviceSpec 	= VirtualDeviceConfigSpec->new(operation => VirtualDeviceConfigSpecOperation->new('add'),device => $virtualDisk,);
	
	my $vmConfigSpec 		= VirtualMachineConfigSpec->new( deviceChange => [$virtualDeviceSpec] );
	
	for ( my $i = 0 ;; $i++ )
	{
		eval
		{
			$args{vmView}->ReconfigVM( spec => $vmConfigSpec );
		};
		if ( $@ )
		{
			if ( ref($@->detail) eq 'TaskInProgress' )
			{
				Common_Utils::WriteMessage("AddVmdk2 :: waiting for 3 secs as another task is in progress.",2);
				sleep(3);
				next;
			}
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			Common_Utils::WriteMessage("ERROR :: Failed to add disk $args{fileName}.",3);
			return ( $Common_Utils::FAILURE );	
		}
		last;
	}
	Common_Utils::WriteMessage("Successfully added disk $args{fileName}.",2);
	return ( $Common_Utils::SUCCESS );
}

#####IsItvCenter#####
##Description 		:: This function determines whether view is from a vCenter or from vSphere. So using this we can differentiate 
##						between a vCenter and vSphere.
##Input 			:: VM view.
##Output 			:: Returns yes if view is from vCenter, else no.
#####IsItvCenter#####
sub IsItvCenter
{
	my $IsItvCenter	= "No";
	my $serviceContent	= Vim::get_service_content();
	
	if ( $serviceContent->about->apiType =~ /VirtualCenter/ )
	{
		$IsItvCenter	= "Yes";
	}		
	my $vCenterVersion	= $serviceContent->about->version;
	Common_Utils::WriteMessage("IsItvCenter = $IsItvCenter, version = $vCenterVersion.",2);
	return ( $IsItvCenter, $vCenterVersion );
}

#####GetVmInfoOnHost#####
##Description 		:: Gets basic VM info of all the machines on vSphere host.
##Input 			:: Virtual Machine view of the vSphere host.
##Output 			:: Return list of virtual machine info on SUCCESS else FAILURE.
#####GetVmInfoOnHost#####
sub GetVmInfoOnHost
{
	my $vmViews 	= shift;
	my @vmInfo 		= ();
	
	foreach my $vmView ( @$vmViews )
	{
		if ( $vmView->name =~ /^unknown$/i )
		{
			next;
		}
		my %vmInfo 					= ();
		$vmInfo{displayName}		= $vmView->name;
		$vmInfo{vmPathName}			= Common_Utils::GetViewProperty( $vmView,'summary.config.vmPathName');
		$vmInfo{uuid}				= Common_Utils::GetViewProperty( $vmView,'summary.config.uuid');
		$vmInfo{numVirtualDisks}	= Common_Utils::GetViewProperty( $vmView,'summary.config.numVirtualDisks');	
		$vmInfo{numEthernetCards}	= Common_Utils::GetViewProperty( $vmView,'summary.config.numEthernetCards');
		$vmInfo{sameControllerTypes}= "Yes";
		
		my $differentControllers	= IsVmHavingDifferentControllerTypes( $vmView );
		if ( $differentControllers )
		{
			$vmInfo{sameControllerTypes}	= "No";
		}
		push @vmInfo , \%vmInfo;
	} 
	
	return ( $Common_Utils::SUCCESS  , \@vmInfo );
}

#####GetVmViewByVmNumber#####
##Description 		:: Finds a VM using VM number as its parameter.
##Input				:: VM number.
##Output 			:: Returns SUCCESS or Failure.
#####GetVmViewByVmNumber#####
sub GetVmViewByVmNumber
{
	my $vmNumber 		= shift;
	
	my ( $returnCode , $vmViews )	= GetVmViews();
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	foreach my $vm ( @$vmViews )
	{
		if ( ( defined ( $vm->{mo_ref}->{value} ) ) && ( $vmNumber eq $vm->{mo_ref}->{value} ) )
		{
			return ( $Common_Utils::SUCCESS , $vm );
		}
	}
	
	Common_Utils::WriteMessage("ERROR :: Failed to find virtual machine having vm number $vmNumber.",3);
	return ( $Common_Utils::SUCCESS );
}

#####GetVmViewByUUIDandHostName#####
##Description		:: Retrives virtual machine view using UUID and Host name as additional check parameter.
##Input 			:: UUID, host name of Virtual Machine.
##Output 			:: Returns vmView on SUCCESS else FAILURE.
#####GetVmViewByUUIDandHostName#####
sub GetVmViewByUUIDandHostName
{
	my $uuid 		= shift;
	my $hostName  	= shift;
	
	my $vmViews 	= Vim::find_entity_views( view_type => "VirtualMachine", properties => GetVmProps(),
										filter => { 'summary.config.uuid' => $uuid , 'guest.hostName' => $hostName } );
										
	if ( ! $vmViews ) 
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find virtual machine view(host name = \"$hostName\").",3);
		return $Common_Utils::FAILURE;
	}
	
	my @tempArr	= @$vmViews;
	if( $#tempArr > 0 )
	{
		Common_Utils::WriteMessage("ERROR :: Found multiple machines with UUID=\"$uuid\" and host name = \"$hostName\".",3);
		return $Common_Utils::FAILURE;
		#shall we print all the Machines over here?
	}
	
	return ( $Common_Utils::SUCCESS , $$vmViews[0] );
	
} 

#####GetVmViewByUuid#####
##Description		:: Gets the Virtual machine view on the basis of UUID.
##Input 			:: UUID of the machine.
##Output 			:: Returns SUCCESS or FAILURE.
#####GetVmViewByUuid#####
sub GetVmViewByUuid
{
	my $uuid 		= shift;
	
	my $vmViews 	= Vim::find_entity_views( view_type => "VirtualMachine", properties => GetVmProps(),
												filter => { 'summary.config.uuid' => $uuid } );
										
	unless ( @$vmViews ) 
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find virtual machine view(uuid = \"$uuid\").",3);
		return ( $Common_Utils::FAILURE );
	}
	 my @tempVmViews = @$vmViews;
	if( $#tempVmViews > 0 )
	{
		Common_Utils::WriteMessage("ERROR :: Found multiple machines with UUID=\"$uuid\".",3);
		return $Common_Utils::FAILURE;
	}
	
	return ( $Common_Utils::SUCCESS , $$vmViews[0] );
	
}

#####GetVmViewOnHost#####
##Description 		:: Gets specific VM view on specified Host.
##Input 			:: Display name, Host name of machine and HostView on which this information has to be retrieved.
##Output 			:: Returns vmView on SUCCESS else FAILURE.
#####GetVmViewOnHost#####
sub GetVmViewOnHost 
{
	my $displayName 	= shift;
	my $hostName 		= shift;
	my $hostView 		= shift;
	
	my $vSphereHostName = $hostView->name;
	my $vmView 			= Vim::find_entity_view( view_type => 'VirtualMachine' , begin_entity => $hostView , properties => GetVmProps(),
												filter => { 'name' => $displayName , 'guest.hostName' => $hostName } );
	
	if ( ! $vmView )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find view for machine with display name = \"$displayName\"( host name = \"$hostName\"). on vSphere host \"$vSphereHostName\".",3);
		return ( $Common_Utils::FAILURE );
	}
	
	return ( $Common_Utils::SUCCESS , $vmView );												
}  

#####GetVmViewsOnHost#####
##Description 		:: Gets VM views on an vSphere host.
##Input 			:: Host view on which we have to begin search.
##Output 			:: Returns vmViews on SUCCESS else Failure.
#####GetVmViewsOnHost#####
sub GetVmViewsOnHost
{
	my $hostView 	= shift;
	
	my $vmViews 	= Vim::find_entity_views( view_type => 'VirtualMachine' , begin_entity => $hostView );
	
	if ( !$vmViews )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find virtual machine view from vSphere.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	return ( $Common_Utils::SUCCESS , $vmViews );
}

#####AnswerQuestion#####
##description : this is for the anwering of the question after we make the machine up.
##input : as of now we are going with the default options.
##output : anwer's the question and returns 0 on success.
#####AnswerQuestion#####
sub AnswerQuestion
{
	my $vm_view = shift;
   
	if (!defined $vm_view->runtime->question) 
	{
		Common_Utils::WriteMessage("ERROR :: The question to be answered at the time of powering on the recovered virtual machine is Un-Defined.",3);
		Common_Utils::WriteMessage("INFO ::Please check for the error and answer the question manually",3);
		return ( $Common_Utils::FAILURE );
	}
		
	my $choice = $vm_view->runtime->question->choice->choiceInfo;
	my $selected = $Common_Utils::PowerOnAnswer;
	Common_Utils::WriteMessage("Selecting the Option $selected",2);
		   
	foreach ( @$choice )
	{
		if ( $selected eq $_->key )
		{
			Common_Utils::WriteMessage(" and the Description of Option was ",2);
			my $description = $_->label;
			Common_Utils::WriteMessage(" $description.",2);
		}
	}
	
	eval {
	            $vm_view->AnswerVM(questionId => $vm_view->runtime->question->id,
                              			answerChoice => $selected);
		 };
	 if ($@) 
		 {
		 	if (ref($@) eq 'SoapFault') 
			{
		    	if (ref($@->detail) eq 'ConcurrentAccess')
				{
					Common_Utils::WriteMessage("ERROR :: The question has been or is being answered by another thread or user.",3);
		        }
		        elsif (ref($@->detail) eq 'InvalidArgument') 
				{
					Common_Utils::WriteMessage("ERROR :: Question Id does not apply to this virtual machine.",3);
		        }
		        else 
				{
		        	Common_Utils::WriteMessage("ERROR :: Fault: $@.",3); 
		        }
			}
			else 
			{
			     Common_Utils::WriteMessage("ERROR ::Fault: $@.",3);
			}
		    return ( $Common_Utils::FAILURE );
		}
	
   	return ( $Common_Utils::SUCCESS );
}

#####ShutdownGuest#####
##Description 		:: Uses shutdowGuest() to shut down a machine. This is called when VMware tools are running.
##Input				:: VMView.
##Output 			:: Retruns SUCCESS else FAILURE.
#####ShutdownGuest#####
sub ShutdownGuest
{
	my $vmView 		= shift;
	my $hostName 	= shift;
	my $vmName 		= $vmView->name;
	
	Common_Utils::WriteMessage("Shutting down machine $vmName on host $hostName.",2);
	eval
	{
		$vmView->ShutdownGuest();
	};
	if ( $@ )
	{
		if( ref($@) eq 'SoapFault' )
	    {
	    	if( ref($@->detail) eq 'NotSupported' ) 
	        {
	        	Common_Utils::WriteMessage("ERROR :: Virtual machine \"$vmName\" is marked as a template. It can't be powered off.",3);
	        }
	        elsif( ref($@->detail) eq 'InvalidPowerState' ) 
	        {
	        	Common_Utils::WriteMessage("ERROR :: The attempted operation cannot be performed in the current state of Virtual Machine \"$vmName\". Check vmx file version of this VM and check it's compatibility with ESX.",3);
	        }
	        elsif( ref($@->detail) eq 'InvalidState' ) 
	        {
	        	Common_Utils::WriteMessage("ERROR :: Current State of the virtual machine \"$vmName\" is not supported for this operation.",3);
	        }
	        else
	        {
	        	Common_Utils::WriteMessage("ERROR :: Virtual Machine \"$vmName\" can't be powered off because of \n $@.",3);
	        }
	    }
	    else 
	    {
	    	Common_Utils::WriteMessage("ERROR :: Virtual Machine \"$vmName\" can't be powered off because of \n $@.",3);
	    }
	    return ( $Common_Utils::FAILURE );
	}
	
	$vmView->ViewBase::update_view_data( GetVmProps() );
	for ( my $i = 0 ; ; $i = $i+5  )
	{
		if ( "poweredoff" eq lc( Common_Utils::GetViewProperty( $vmView,'summary.runtime.powerState')->val ) )
		{
			last;
		}
		
		if ( $i < 900 )
		{
			sleep(5);
			$vmView->ViewBase::update_view_data( GetVmProps() );
			next;
		}
		Common_Utils::WriteMessage("ERROR :: Failed to shut down machine $vmName.\n",3);
		return ( $Common_Utils::FAILURE );
	}
	
	Common_Utils::WriteMessage("Successfully shut down machine $vmName.\n",2); 
	return ( $Common_Utils::SUCCESS );
}

#####PowerOffVm#####
##Description 		:: Uses command PowerOffVm() to power off the machine. This is used when VmWare tools are not running.
##Input 			:: VmView.
##Output 			:: Returns SUCCESS else FAILURE.
#####PowerOffVm#####
sub PowerOffVm
{
	my $vmView		= shift;
	my $hostname 	= shift;
	my $vmName 		= $vmView->name;
	my $ReturnCode 	= $Common_Utils::SUCCESS;
	
	Common_Utils::WriteMessage("Powering off machine $vmName.",2);
	eval 
	{
		my $task_ref 	= $vmView->PowerOffVM_Task();
	    my $task_view 	= Vim::get_view(mo_ref => $task_ref);
	    while ( 1 ) 
	    {
	    	
	        my $info 	= $task_view->info;
	        if( $info->state->val eq 'running' )
	        {
	        	Common_Utils::WriteMessage("Powering off machine $vmName.",2);
	        } 
	        elsif( $info->state->val eq 'success' ) 
	        {
	           Common_Utils::WriteMessage("virtual machine \"$vmName\" under host $hostname powered off.",2);
	           last;
	        } 
	        elsif( $info->state->val eq 'error' ) 
	        {
	        	my $soap_fault 	= SoapFault->new;
	            my $errorMesg 	= $soap_fault->fault_string($info->error->localizedMessage);
	            Common_Utils::WriteMessage("ERROR :: Power-off failed on machine \"$vmName\" due to error \"$errorMesg\".",3);
	            $ReturnCode 	= $Common_Utils::FAILURE;
	            last;
	        }
	        sleep 2;
	        $task_view->ViewBase::update_view_data();
	     }
	};
	if($@)
	{
		if( ref($@) eq 'SoapFault' )
	    {
	    	if( ref($@->detail) eq 'NotSupported' ) 
	        {
	        	Common_Utils::WriteMessage("ERROR :: Virtual machine \"$vmName\" is marked as a template. It can't be powered off.",3);
	        }
	        elsif( ref($@->detail) eq 'InvalidPowerState' ) 
	        {
	        	Common_Utils::WriteMessage("ERROR :: The attempted operation cannot be performed in the current state of Virtual Machine \"$vmName\". Check vmx file version of this VM and check it's compatibility with ESX.",3);
	        }
	        elsif( ref($@->detail) eq 'InvalidState' ) 
	        {
	        	Common_Utils::WriteMessage("ERROR :: Current State of the virtual machine \"$vmName\" is not supported for this operation.",3);
	        }
	        else
	        {
	        	Common_Utils::WriteMessage("ERROR :: Virtual Machine \"$vmName\" can't be powered off because of \n $@.",3);
	        }
	    }
	    else 
	    {
	    	Common_Utils::WriteMessage("ERROR :: Virtual Machine \"$vmName\" can't be powered off because of \n $@.",3 );
	    }
	    $ReturnCode 	= $Common_Utils::FAILURE;
	}

	return ( $ReturnCode );
}

#####PowerOff#####
##description		:: Powers off Virtual Machine.
##Input 			:: Machine UUID for which power on has to be triggered.
##Output			:: Returns SUCCESS else FAILURE.
#####PowerOff#####
sub PowerOff
{
	my $vmView 		= shift;
	my $returnCode	= $Common_Utils::SUCCESS;

	my $vm_name		= $vmView->name;
	my $mor_host 	= Common_Utils::GetViewProperty( $vmView, 'summary.runtime.host');
	my $hostname 	= Vim::get_view( mo_ref => $mor_host, properties => ['name'] )->name;
	
	if ( Common_Utils::GetViewProperty( $vmView,'summary.guest.toolsStatus')->val eq "toolsOk" )
	{
		$returnCode = ShutdownGuest( $vmView , $hostname );
	}
	else
	{
		$returnCode = PowerOffVm( $vmView , $hostname );
	}
	return ( $returnCode );
}

#####PowerOn#####
##description		:: Powers on Virtual Machine and if a question is raised it will answer as well.
##Input 			:: Machine UUID for which power on has to be triggered.
##Output			:: Returns SUCCESS else FAILURE.
#####PowerOn#####
sub PowerOn
{
	my $vmView 		= shift;
	my $ReturnCode	= $Common_Utils::SUCCESS;
	my $vm_name		= $vmView->name;
	my $mor_host 	= Common_Utils::GetViewProperty( $vmView, 'summary.runtime.host');
	my $hostname 	= Vim::get_view( mo_ref => $mor_host, properties => ['name'] )->name;
	eval 
	{
		my $task_ref 	= $vmView->PowerOnVM_Task();
	    my $task_view 	= Vim::get_view(mo_ref => $task_ref);
	    while ( 1 ) 
	    {
	        my $info 	= $task_view->info;
	        if( $info->state->val eq 'running' )
	        {
	        	my $entity_view = Vim::get_view(mo_ref => $info->entity);
	            my $question 	= $entity_view->runtime->question;
	            if ( defined $question ) 
	            {
	            	my $response = AnswerQuestion($entity_view);
	                if ( $response ne $Common_Utils::SUCCESS )
	                {
	                	$ReturnCode	= $Common_Utils::FAILURE;
	                	last;
	                }
	             }
	         } 
	         elsif( $info->state->val eq 'success' ) 
	         {
	            Common_Utils::WriteMessage("virtual machine \"$vm_name\" under host $hostname powered on.",2);
	            last;
	         } 
	         elsif( $info->state->val eq 'error' ) 
	         {
	         	my $soap_fault 	= SoapFault->new;
	            my $errorMesg 	= $soap_fault->fault_string($info->error->localizedMessage);
	            Common_Utils::WriteMessage("ERROR :: Power-on failed on machine \"$vm_name\" due to error \"$errorMesg\".",3);
	            $ReturnCode	= $Common_Utils::FAILURE;
	            last;
	         }
	         sleep 2;
	         $task_view->ViewBase::update_view_data();
	     }
	};
	if($@)
	{
		if( ref($@) eq 'SoapFault' )
	    {
	    	if( ref($@->detail) eq 'NotSupported' ) 
	        {
	        	Common_Utils::WriteMessage("Virtual machine \"$vm_name\" is marked as a template. It can't be powered on.",3);
	        }
	        elsif( ref($@->detail) eq 'InvalidPowerState' ) 
	        {
	        	Common_Utils::WriteMessage("The attempted operation cannot be performed in the current state of Virtual Machine \"$vm_name\". Check vmx file version of this VM and check it's compatibility with ESX.",3);
	        }
	        elsif( ref($@->detail) eq 'InvalidState' ) 
	        {
	        	Common_Utils::WriteMessage("Current State of the virtual machine \"$vm_name\" is not supported for this operation.",3);
	        }
	        else
	        {
	        	Common_Utils::WriteMessage("Virtual Machine \"$vm_name\" can't be powered on because of \n $@.",3);
	        }
	    }
	    else 
	    {
	    	Common_Utils::WriteMessage("Virtual Machine \"$vm_name\" can't be powered on because of \n $@.",3 );
	    }
	    $ReturnCode	= $Common_Utils::FAILURE;
	}

	return ( $ReturnCode );
}

#####AreMachinesBooted#####
##Description		:: Checks whether host name of machine is populated in a span of 3 Mins for each of the machine in list.
##Input 			:: List of machine on which this check has to be performed.
##Output 			:: Returns SUCCESS if every machine is booted else FAILURE.
#####AreMachinesBooted#####
sub AreMachinesBooted
{
	my $machinesList	= shift;
	my $returnCode		= $Common_Utils::SUCCESS;
	
	foreach my $machine ( @$machinesList )
	{
		if( $Common_Utils::SUCCESS ne CheckHostName( $machine ) )
		{
			$returnCode	= $Common_Utils::FAILURE;
		}
	}
	Common_Utils::WriteMessage("AreMachinesBooted : returncode = $returnCode.",2);	
	return $returnCode;
}

#####CheckHostName#####
##Description 		:: checks whether the Host name is populated for the specified machine.
##Input 			:: UUID of the machine for which host name has to checked.
##Output			:: Returns SUCCESS on success else FAILURE.
#####CheckHostName#####
sub CheckHostName
{
	my $uuid					= shift;
	my ( $returnCode , $vmView )= VmOpts::GetVmViewByUuid( $uuid );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find view of machine \"$uuid\".",3);
		return ( $Common_Utils::FAILURE );
	}
	
	for ( my $i = 0 ; $i < 300 ; )
	{
		if ( ( ! defined( Common_Utils::GetViewProperty( $vmView,'guest.hostName') ) ) || ( "" eq Common_Utils::GetViewProperty( $vmView,'guest.hostName') ) )
		{
			sleep(5);
			$i+= 5;
			$vmView->ViewBase::update_view_data( GetVmProps() );
		}
		else
		{
			my $hostName 		= Common_Utils::GetViewProperty( $vmView,'guest.hostName');
			Common_Utils::WriteMessage("Machine booted successfully and host name = $hostName",2);
			return ( $Common_Utils::SUCCESS );
		}
	}
	
	my $displayName 		= $vmView->name;
	Common_Utils::WriteMessage("ERROR :: Host name is not populated for \"$displayName\".",3);
	#18757 -- Marking as success even when host name of machine is not populated.
	return ( $Common_Utils::SUCCESS );
}

#####GetDummyDiskInfo#####
##Description 		:: Find all the dummy disk and populate arrays @dummy_disk_virtual_node and @dummy_disk_names
##Input 			:: Master Target Name.
##Output 			:: Return's 0 on success else 1.
#####GetDummyDiskInfo#####
sub GetDummyDiskInfo
{
	my $vmView			= shift;
	my $devices 		= Common_Utils::GetViewProperty( $vmView,'config.hardware.device');
	my @dummydDiskInfo	= ();
	
	foreach my $device (@$devices)
	{
		if($device->deviceInfo->label =~ /Hard Disk/i)
		{
			my $key_file 				= $device->key;	
			my $disk_name 				=  $device->backing->fileName;
			my $diskNameComp			= "DummyDisk_";
			if( $key_file>=2000 && $key_file<=2100 )
			{
				my $controller_number 	= $device->controllerKey - 1000;
				my $unit_number 		= ($key_file - 2000)%16;
				$diskNameComp			= $diskNameComp."$controller_number$unit_number.vmdk";
			}
			if( $key_file>=16000 && $key_file<=16120 )
			{
				my $controller_number 	= $device->controllerKey - 15000 + 4;
				my $unit_number 		= ($key_file - 16000)%30;
				$diskNameComp			= $diskNameComp."$controller_number$unit_number.vmdk";
			}
			if( $disk_name =~ /$diskNameComp$/i)
			{
				push @dummydDiskInfo, $device;
			}
		}
	}
	return ( $Common_Utils::SUCCESS , \@dummydDiskInfo );
}

#####RemoveDummyDisk#####
##Description 		:: Deletes dummy disk from Master Target.
##Input 			:: Target Node.
##Output 			:: Returns SUCCESS on deletion else FAILURE.
#####RemoveDummyDisk#####
sub RemoveDummyDisk
{
	my $mtView 	= shift;
	my $vmName 	= $mtView->name;		
	
	my( $returnCode , $dummyDiskInfo  )	= GetDummyDiskInfo( $mtView );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to find dummy disk information.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my @tempArr	= @$dummyDiskInfo;
	if ( -1 != $#tempArr )
	{
		if( $Common_Utils::SUCCESS != DeleteDummyDisk( $dummyDiskInfo , $mtView ) )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to delete dummy disks from $vmName.",3);
			Common_Utils::WriteMessage("ERROR :: Delete all dummy disks( vmdk name contains 'dummy')from $vmName and re-run.",3);
			return ( $Common_Utils::FAILURE );
		}
	}
	Common_Utils::WriteMessage("Successfully removed dummy disk from $vmName.",2);
	return ( $Common_Utils::SUCCESS );
} 

#####GetVmData#####
##Description 		:: Collects all the information regarding the Virtual machines on Host parameter passed.
##Input 			:: vmView , Host Group of vSphere.
##Output 			:: Reference to list of VirtualMachines in host specified on SUCCESS else FAILURE.
#####GetVmData#####
sub GetVmData
{
	my %args 			= @_;
	my @vmInfo			= ();
	my $returnCode		= 0;
	my $diskList		= GetDiskInfo( $args{vmViews} , $args{hostGroup} );
	
	foreach my $vmView ( @{ $args{vmViews} } )
	{
		my %vmInfo		= ();
		eval 
		{
			no warnings 'exiting';
			my $hostGroupVM	= Common_Utils::GetViewProperty( $vmView, 'summary.runtime.host')->value;
			my $machineName = $vmView->name;
			if ( $args{hostGroup} ne $hostGroupVM )
			{
				next;	
			}
			#|| ( $vmView->guest->guestState =~ /^unknown$/i )
			if ( ( $vmView->name =~ /^unknown$/i ) || ( Common_Utils::GetViewProperty( $vmView, 'summary.runtime.connectionState')->val !~ /^connected$/i ) )
			{
				Common_Utils::WriteMessage("Discovery of machine \"$machineName\" skipped as it is in orphaned/disconnected/inaccessible state.",3);
				next;
			}
			
			$vmInfo{template}	= "false";
			if ( Common_Utils::IsPropertyValueTrue( $vmView,'summary.config.template' ) )
			{
				Common_Utils::WriteMessage("\"$machineName\" is a template.",2);
				$vmInfo{template}	= "true";
			} 
		
			if ( ( $args{osType} ne "all" ) && ( Common_Utils::GetViewProperty( $vmView,'summary.config.guestFullName' ) =~ /^$/ ) )
			{
				$vmInfo{uuid}			= Common_Utils::GetViewProperty( $vmView, 'summary.config.uuid' );
				if( ( $args{guuid} eq "yes" ) && ( $vmInfo{uuid} !~ /^$/ ) )
				{
					$vmInfo{displayName}	= $machineName;
					if ( Common_Utils::GetViewProperty( $vmView, 'config.guestFullName') !~ /$args{osType}/i )
					{
						my $operatingSystem 	= Common_Utils::GetViewProperty( $vmView,'config.guestFullName' );
						if ( ( $args{osType} !~ /^Linux$/i ) )
						{
							Common_Utils::WriteMessage("Skipped machine $machineName( OS = $operatingSystem ).",2);
							next;
						}
						elsif( ( $args{osType} =~ /^Linux$/i ) && ( Common_Utils::GetViewProperty( $vmView,'config.guestFullName') !~ /^CentOS/i ) )
						{
							Common_Utils::WriteMessage("Skipped machine $machineName( OS = $operatingSystem ).",2);
							next;
						}
					}
					push @vmInfo , \%vmInfo;
				}
				else
				{
					Common_Utils::WriteMessage("Guest OS type is not defined for machine \"$machineName\".",3);
					Common_Utils::WriteMessage("1. Check whether summary of virtual machine displays GuestOS information.",3);
					Common_Utils::WriteMessage("2. If it is not displayed click on EditSettings and say OK.",3);
					Common_Utils::WriteMessage("3. If displayed, report this issue to InMage Support Team.",3);
				}				
				next;
			}	
			
			if  ( ( $args{osType} ne "all" ) && ( Common_Utils::GetViewProperty( $vmView,'summary.config.guestFullName' ) !~ /$args{osType}/i ) )
			{
				my $operatingSystem 	= Common_Utils::GetViewProperty( $vmView,'summary.config.guestFullName' );
				if ( ( $args{osType} !~ /^Linux$/i ) )
				{
					Common_Utils::WriteMessage("Skipped machine $machineName( OS = $operatingSystem ).",2);
					next;
				}
				elsif( ( $args{osType} =~ /^Linux$/i ) && ( Common_Utils::GetViewProperty( $vmView,'summary.config.guestFullName' ) !~ /^CentOS/i ) )
				{
					Common_Utils::WriteMessage("Skipped machine $machineName( OS = $operatingSystem ).",2);
					next;
				}
			}
			
			$vmInfo{displayName}	= $vmView->name;
			Common_Utils::WriteMessage("###Collecting details of virtual machine $vmInfo{displayName}###.",2);
			if( $vmInfo{template} ne "true" )
			{
				$vmInfo{resourcePool}	= $vmView->resourcePool->value;
				$vmInfo{resourcePoolGrp}= $vmView->resourcePool->value;
			}
			$vmInfo{powerState}		= Common_Utils::GetViewProperty( $vmView,'summary.runtime.powerState')->val;
			$vmInfo{operatingSystem}= Common_Utils::GetViewProperty( $vmView,'summary.config.guestFullName');
			$vmInfo{uuid}			= Common_Utils::GetViewProperty( $vmView,'summary.config.uuid');
			$vmInfo{instanceUuid}	= Common_Utils::GetViewProperty( $vmView,'summary.config.instanceUuid');
			$vmInfo{vmPathName}		= Common_Utils::GetViewProperty( $vmView,'summary.config.vmPathName');
			$vmInfo{numVirtualDisks}= Common_Utils::GetViewProperty( $vmView,'summary.config.numVirtualDisks');	
	        $vmInfo{numCpu}         = Common_Utils::GetViewProperty( $vmView,'summary.config.numCpu');
	        $vmInfo{memorySizeMB} 	= Common_Utils::GetViewProperty( $vmView,'summary.config.memorySizeMB');
	        $vmInfo{vSphereHostName}= $args{vSphereHostName};
	        $vmInfo{dataCenter}		= "";
	        $vmInfo{hostVersion}	= $args{hostVersion};
	        $vmInfo{vmxVersion}		= Common_Utils::GetViewProperty( $vmView,'config.version');
	        $vmInfo{guestId}		= Common_Utils::GetViewProperty( $vmView,'summary.config.guestId');
			$vmInfo{efi}			= "false";
			$vmInfo{diskEnableUuid}	= "na";
			$vmInfo{vmConsoleUrl}	= GetVmWebConsoleUrl( $vmView );
						
			if(( defined Common_Utils::GetViewProperty( $vmView,'config.firmware') ) && ( "efi" eq Common_Utils::GetViewProperty( $vmView,'config.firmware') ))
			{
				$vmInfo{efi}	= "true";
			}
			
			my $extraConfig	= Common_Utils::GetViewProperty( $vmView, 'config.extraConfig');
			foreach my $config ( @$extraConfig )
			{
				if ( $config->key =~ /^disk\.enableuuid$/i )
				{
					$vmInfo{diskEnableUuid}	= ( $config->value =~ /^true$/i ) ? "yes" : "no";
					last; 
				}
			}
		
			if ( defined $vmView->parent )
			{
				my $parentView 		= $vmView;
				while( $parentView->parent->type !~ /Datacenter/i )
				{	   
					$parentView = Vim::get_view( mo_ref => ($parentView->parent), properties => ["parent"] );
			    }
			   $vmInfo{dataCenter}	= Vim::get_view( mo_ref => ($parentView->parent), properties => ["name"] )->name;
			}
			elsif( defined $vmView->parentVApp )
			{
				my $parentView		= $vmView;
				$parentView 		= Vim::get_view( mo_ref => ($parentView->parentVApp) );
				while( $parentView->parent->type !~ /Datacenter/i )
				{	   
					$parentView = Vim::get_view( mo_ref => ($parentView->parent), properties => ["parent"] );
			    }
			   $vmInfo{dataCenter}	= Vim::get_view( mo_ref => ($parentView->parent), properties => ["name"] )->name;
			}
				
		    $vmInfo{toolsStatus}	= "";
	        if ( defined Common_Utils::GetViewProperty( $vmView,'summary.guest.toolsStatus') )
	        {
		        if ( Common_Utils::GetViewProperty( $vmView,'summary.guest.toolsStatus')->val eq "toolsOk" ) 
				{
					$vmInfo{toolsStatus}= "OK";
				}
		        elsif ( Common_Utils::GetViewProperty( $vmView,'summary.guest.toolsStatus')->val eq "toolsNotInstalled" ) 
				{
					$vmInfo{toolsStatus}= "NotInstalled";
				}
				elsif ( Common_Utils::GetViewProperty( $vmView,'summary.guest.toolsStatus')->val eq "toolsNotRunning")
				{
					$vmInfo{toolsStatus}= "NotRunning";
				}
				elsif ( Common_Utils::GetViewProperty( $vmView,'summary.guest.toolsStatus')->val eq "toolsOld" )
				{
					$vmInfo{toolsStatus}= "OutOfdate";
				}
	        }
	            
	        my @datastore 			= split( /\[|\]/,Common_Utils::GetViewProperty( $vmView,'summary.config.vmPathName') );
	        my $final_slash 		= rindex( $datastore[2] , "/" );
	        $vmInfo{datastore}		= $datastore[1];
			$vmInfo{folderPath}		= substr( $datastore[2] , 1 , $final_slash );
			$vmInfo{hostName} 		= "NOT SET";
			if ( defined ( Common_Utils::GetViewProperty( $vmView,'guest.hostName') ) and  "" ne Common_Utils::GetViewProperty( $vmView,'guest.hostName') )
			{
				$vmInfo{hostName} 	= Common_Utils::GetViewProperty( $vmView,'guest.hostName');
			}
				
			( $returnCode , $vmInfo{ipAddress} , $vmInfo{networkInfo} ) = FindNetworkDetails( $vmView , $args{dvPortGroupInfo} );
			if( $Common_Utils::SUCCESS != $returnCode )
			{
				Common_Utils::WriteMessage("ERROR :: Unable to find Network information of Virtual Machine $vmInfo{displayName}.",3);
			}				
	            
	        ( $returnCode , $vmInfo{ideCount} , $vmInfo{rdm} , $vmInfo{cluster} , $vmInfo{diskInfo} , $vmInfo{floppyDevices} ) = FindDiskDetails( $vmView, $diskList ); 
	        if( $Common_Utils::SUCCESS != $returnCode )
			{
				Common_Utils::WriteMessage("ERROR :: Unable to get details of disk for machine \"$vmInfo{displayName}\".",3);
			}
		};
		if ( $@ )
		{
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			if(( exists $vmInfo{displayName} ) && ( "" ne $vmInfo{displayName} ))
			{
				Common_Utils::WriteMessage("ERROR :: Failed to discover virtual machine $vmInfo{displayName}.",3);
			}			
			next;
		}    
		push @vmInfo , \%vmInfo;
	}
	
	return ( $Common_Utils::SUCCESS , \@vmInfo );
}

#####GetDiskInfo#####
##Description 		:: collects disk information for verifying cluster disk .
##Input 			:: virtual machine list.
##Output 			:: returns a array list which has disk information.
####GetDiskInfo#####
sub GetDiskInfo
{
	my $vmViews 	= shift;
	my $hostGroup 	= shift;
	my @disk_list 	= ();
	
	foreach my $vmView ( @$vmViews ) 
	{
		eval
		{
			my $hostGroupVM		= Common_Utils::GetViewProperty( $vmView, 'summary.runtime.host')->value;
			if ( ( $hostGroup eq $hostGroupVM ) && ( $vmView->name !~ /^unknown$/i ) && (!( Common_Utils::IsPropertyValueTrue( $vmView,'summary.config.template' ) ) ) && ( Common_Utils::GetViewProperty( $vmView, 'summary.runtime.connectionState')->val !~ /^orphaned$/i ) 
					&& defined( Common_Utils::GetViewProperty( $vmView,'config.hardware.device') ) && ( Common_Utils::GetViewProperty( $vmView, 'guest.guestState') !~ /^unknown$/i ) ) 
			{
				my $devices 	= Common_Utils::GetViewProperty( $vmView,'config.hardware.device');
				foreach my $device ( @$devices )
				{
					if ( $device->deviceInfo->label =~ /Hard Disk/i )
					{
						my $vmdkUuid		= $device->backing->uuid;
						if ( ( exists $device->{backing}->{compatibilityMode} ) && ( 'physicalmode'eq lc( $device->backing->compatibilityMode ) ) )
						{
							$vmdkUuid	= $device->backing->lunUuid;
						}
	#					my $disk_name 	=  $device->backing->fileName;
	#					if(defined $vmView->snapshot)
	#					{
	#						my $final_slash_index 	= rindex($disk_name, "/");
	#						my $filename 			= substr($disk_name,$final_slash_index + 1,length($disk_name));
	#						if ( $filename =~ /-[0-9]{6}\.vmdk$/ )
	#						{
	#							my $final_dash_index =  rindex($filename, "-");
	#							if ( $final_dash_index  != -1 )
	#							{
	#								$filename 	= substr($filename,0,$final_dash_index);
	#								$disk_name 	= substr($disk_name,0,$final_slash_index + 1);
	#								$disk_name 	= $disk_name .  $filename . ".vmdk";
	#							}
	#						}
	#					}					
						push @disk_list , $vmdkUuid;
					}
				}
			}
		};
		if ($@)
		{
			Common_Utils::WriteMessage("ERROR :: GetDiskInfo failed for VM $vmView->{name} due to error $@.",2);
		}
	}
	return ( \@disk_list );
}

#####FindDiskDetails#####
##Description 			:: Find the disk information. DisK name , Size , Disk type and it's mode etc.
##Input 				:: Vm View.
##Output 				:: Returns array of dik information on SUCCESS else FAILURE.
#####FindDiskDetails#####
sub FindDiskDetails
{
	my $vmView				= shift;
	my $diskList			= shift;
	my @diskList			= @$diskList;
	my $rdm 				= "FALSE";	
	my $cluster 			= "no";
	my %mapofKeyandLabel	= ();
	my %mapControllerKeyType= ();
	my %mapofKeyandMode		= ();
	my $ideCount		 	= 0;
	my $floppyDeviceCount	= 0;
	my @VmDiskList			= ();
	my $devices 			= Common_Utils::GetViewProperty( $vmView,'config.hardware.device');
	my $vmPathName 			= Common_Utils::GetViewProperty( $vmView,'summary.config.vmPathName');

	foreach my $device_temp ( @$devices )
	{
		if ( $device_temp->deviceInfo->label =~ /SCSI controller/i )
		{
			if ( defined ( $device_temp->scsiCtlrUnitNumber ) )
			{
				$mapofKeyandLabel{$device_temp->key} 	= $device_temp->deviceInfo->label;
				$mapControllerKeyType{$device_temp->key}= ref ( $device_temp );
				$mapofKeyandMode{$device_temp->key}		= $device_temp->sharedBus->val;
			}
		}
		elsif ( $device_temp->deviceInfo->label =~ /SATA controller/i )
		{
			$mapofKeyandLabel{$device_temp->key} 	= $device_temp->deviceInfo->label;
			$mapControllerKeyType{$device_temp->key}= ref ( $device_temp );
		}
		elsif ( $device_temp->deviceInfo->label =~ /^IDE [\d]$/i )
		{
			if ( defined ( $device_temp->device ) )
			{
				my $ide_count = $device_temp->device;
				my @ide_count = @$ide_count;
				if( -1 != $#ide_count )
				{
					$ideCount++;
				}
				$mapControllerKeyType{$device_temp->key} = ref( $device_temp ); 
			}
		}
		elsif ( $device_temp->deviceInfo->label =~ /SIO controller/i )
		{
			$floppyDeviceCount++;
		}
	}

	foreach my $device ( @$devices )
	{
		my %diskData					= ();
		if ( $device->deviceInfo->label =~ /Hard Disk/i )
		{
			my $key_file 				= $device->key;		
			my $disk_name 				= $device->backing->fileName;
			if ( defined $vmView->snapshot )
			{
				my $diskLayoutInfo 		= Common_Utils::GetViewProperty( $vmView,'layout.disk');
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
			
			my $fname  				=  $disk_name;
			my $rindexVmPathName 	= rindex( $vmPathName , "/" );
			my $rindexDiskName		= rindex( $disk_name , "/" );
			my $vmDatastorePath	 	= substr( $vmPathName , 0 , $rindexVmPathName );
			my $diskDatastorePath	= substr( $disk_name , 0 , $rindexDiskName );								

			my $fsize  = $device->capacityInKB;
						
			if ( exists $device->{backing}->{compatibilityMode} )
			{
				$rdm 								= "TRUE";
				$diskData{diskType}					= $device->backing->compatibilityMode;
				$diskData{diskMode}					= "Mapped Raw LUN";
				$diskData{independentPersistent}	= "persistent";
				$diskData{diskUuid}					= $device->backing->lunUuid;
				if ( 'virtualmode'eq lc( $device->backing->compatibilityMode ) )
				{
					$diskData{diskUuid}				= $device->backing->uuid;
				}
				if ( $device->backing->diskMode =~ /independent_persistent/i )
				{
					$diskData{independentPersistent}= "independent_persistent";
				}
			}
			elsif ( $device->backing->diskMode =~ /independent_nonpersistent/i || $device->backing->diskMode =~ /independent_persistent/i )
			{
				$diskData{diskType}					= $device->backing->diskMode;
				$diskData{diskMode}					= "Virtual Disk";
				$diskData{independentPersistent}	= "independent_persistent";
				$diskData{diskUuid}					= $device->backing->uuid;
			}
			else
			{
				$diskData{diskType}					= "persistent";
				$diskData{diskMode}					= "Virtual Disk";
				$diskData{independentPersistent}	= "persistent";
				$diskData{diskUuid}					= $device->backing->uuid;
			}
											
			my $scsi_controller_number 		= "";
			my $controllerkey 				= $device->controllerKey;
			
			if ( $key_file>=2000 && $key_file<=2100 )
			{
				my $controller_number 		= Common_Utils::floor(($key_file - 2000)/16);
				my $unit_number 			= ($key_file - 2000)%16;
				$scsi_controller_number 	= $controller_number.":".$unit_number;
				$diskData{scsiVmx}			= $scsi_controller_number;
				$diskData{ideOrScsi}		= "scsi";
				$diskData{controllerType}	= $mapControllerKeyType{$controllerkey};	
			}
			elsif ( $key_file>=16000 && $key_file<=16120 )
			{
				my $controller_number 		= Common_Utils::floor(($key_file - 16000)/30) + 4;
				my $unit_number 			= ($key_file - 16000)%30;
				$scsi_controller_number		= $controller_number.":".$unit_number;
				$diskData{scsiVmx}			= $scsi_controller_number;
				$diskData{ideOrScsi}		= "sata";
				$diskData{controllerType}	= $mapControllerKeyType{$controllerkey};	
			}
			elsif ( $key_file>=3000 && $key_file<=3100 )
			{
				my $controller_number 		= Common_Utils::floor(($key_file - 3000)/16);
				my $unit_number 			= ($key_file - 3000)%16;
				$scsi_controller_number		= $controller_number.":".$unit_number;
				$diskData{scsiVmx}			= $scsi_controller_number;
				$diskData{ideOrScsi}		= "ide";
				$diskData{controllerType}	= $mapControllerKeyType{$controllerkey};	
			}
			
			if ( defined $mapofKeyandLabel{$controllerkey} )
			{									
				my $scsi_controller_label 	= $mapofKeyandLabel{$controllerkey};
				my $scsi_val 				= substr($scsi_controller_label,length($scsi_controller_label)-1,length($scsi_controller_label));
				if( $scsi_controller_label =~ /SATA/i )
				{
					$scsi_val += 4;
				}
				$diskData{scsiHost}			= $scsi_controller_number;		
				if ( !( $scsi_controller_number =~ /^$scsi_val/ ) )
				{
					my @scsiControllerSplit = split(/:/,$scsi_controller_number);
					$scsi_controller_number = $scsi_val.":".$scsiControllerSplit[1];
					$diskData{scsiHost}		= $scsi_controller_number;
				}
			}
				
			$diskData{clusterDisk}			= "no";
			if( ( exists $mapofKeyandMode{$controllerkey} ) && ( $mapofKeyandMode{$controllerkey} =~ /^virtualSharing$/i || $mapofKeyandMode{$controllerkey} =~ /^physicalSharing$/i ) )
			{
				$diskData{clusterDisk}	= "yes";
			}
			
			if( $cluster ne "yes")
			{
				my $clusterDisk 				= grep $_ eq $diskData{diskUuid}, @diskList;
				if ( ( $clusterDisk > 1 ) && ( $diskData{clusterDisk} eq "yes" ) )
				{
					$cluster 					= "yes";												
				}
			}
			
			if( ( $cluster eq "yes" ) && ( $diskData{clusterDisk} eq "yes" ) )
			{
				my $vmdkName 		= substr( $disk_name , $rindexDiskName , length ( $disk_name ) );
				my $dotIndex		= rindex( $vmdkName , "." );
				my $vmdkFileName 	= substr( $vmdkName , 0 , $dotIndex );
				$disk_name 			= $vmDatastorePath.$vmdkFileName.".vmdk";
				Common_Utils::WriteMessage("cluster disk name = $disk_name.",2);
			}
			elsif ( $vmDatastorePath ne $diskDatastorePath )
			{
				my $vmdkName 		= substr( $disk_name , $rindexDiskName , length ( $disk_name ) );
				my $dotIndex		= rindex( $vmdkName , "." );
				my $vmdkFileName 	= substr( $vmdkName , 0 , $dotIndex );
				$disk_name 			= $vmDatastorePath.$vmdkFileName."_InMage_NC_".$key_file.".vmdk";
				Common_Utils::WriteMessage("disk name = $disk_name.",2);
			}

			$diskData{diskName}				= $disk_name;
			$diskData{diskSize}				= $fsize;
			$diskData{controllerMode}		= $mapofKeyandMode{$controllerkey};
						
			if( exists $device->{diskObjectId} )
			{
				$diskData{diskObjectId}			= $device->diskObjectId;
			}			
			
			if( exists $device->{capacityInBytes} )
			{
				$diskData{capacityInBytes}			= $device->capacityInBytes;
			}
			
			push @VmDiskList , \%diskData;
		}
	}
	return ( $Common_Utils::SUCCESS , $ideCount , $rdm , $cluster , \@VmDiskList , $floppyDeviceCount );
}

#####FindNetworkDetails#####
##Description	 		:: Populates arrays Network Name, Mac address and IP values to that NIC.
##Input 				:: Managed Object Reference to a Virtual Machine.
##Output 				:: Returns array of Network information on SUCCESS else FAILURE.
#####FindNetworkDetails#####
sub FindNetworkDetails
{
	my $vmView 			= shift;
	my $dvPortGroupInfo	= shift;
	my $ip_address		= "";
	my %Map_KeyNic		= ();
	my @networkList 	= ();
	my $queryDVS 		= 0;
	my %macAddress		= ();
	my %macInformationFetched 	= ();
	
	my $display_name	= $vmView->name; 
	my $devices 		= Common_Utils::GetViewProperty( $vmView,'config.hardware.device');
	foreach my $device ( @$devices )
	{
		if( $device->deviceInfo->label =~ /^Network adapter/i )
		{
			$macAddress{$device->macAddress}		= $device->deviceInfo->label;
			$Map_KeyNic{$device->key}{label}		= $device->deviceInfo->label;
			$Map_KeyNic{$device->key}{addressType}	= $device->addressType;
			if ( "virtualethernetcarddistributedvirtualportbackinginfo" eq lc ( ref( $device->backing ) ) )
			{
				$Map_KeyNic{$device->key}{portgroupKey}	= $device->backing->{port}->portgroupKey;
			}
			$Map_KeyNic{$device->key}{backingType}	= ref( $device->backing );	
			$Map_KeyNic{$device->key}{adapterType}	= ref ( $device );
		}	
	}
	
	if( defined(Common_Utils::GetViewProperty( $vmView,'guest.net') ) )
	{
		my $net 				 = Common_Utils::GetViewProperty( $vmView,'guest.net');
		foreach my $net_bless(@$net)
		{
			my %networkData 	 = ();
			my $dns_ip 			 = "";
			my $ip_address_array = $net_bless->ipAddress;
			
			$networkData{macAddress}	= $net_bless->macAddress;
			if($net_bless->deviceConfigId < 0)
			{
				Common_Utils::WriteMessage("ERROR :: Unable to find network adapter for \"$networkData{macAddress}\" .",2);
				next;
			}
			$networkData{networkLabel} 	= $Map_KeyNic{$net_bless->deviceConfigId}{label}; 
			$networkData{addressType}	= $Map_KeyNic{$net_bless->deviceConfigId}{addressType};
			$networkData{adapterType}	= $Map_KeyNic{$net_bless->deviceConfigId}{adapterType};
			$macInformationFetched{$net_bless->macAddress} = $networkData{networkLabel};			
			if( defined( $net_bless->network ) )
			{
				$networkData{network} 	= $net_bless->network;
				if ( "virtualethernetcarddistributedvirtualportbackinginfo" eq lc ( $Map_KeyNic{$net_bless->deviceConfigId}{backingType} ) )
				{
					foreach my $dvPortGroup ( @$dvPortGroupInfo )
					{
						if ( $dvPortGroup->{portgroupKey} eq $Map_KeyNic{$net_bless->deviceConfigId}{portgroupKey} )
						{
							$networkData{network}	= $dvPortGroup->{portgroupNameDVPG};
							last;
						}
					}
				}
			}
			else
			{
				my @tempArr	= @$dvPortGroupInfo;
				if ( ! exists $Map_KeyNic{$net_bless->deviceConfigId}{portgroupKey} )
				{
					Common_Utils::WriteMessage("ERROR :: Unable to determine distributed port group name to which network adapter \"$networkData{networkLabel}\" is attached to.",2);
					$networkData{network}	= "";
				}
				elsif ( -1 == $#tempArr )
				{
					Common_Utils::WriteMessage("ERROR :: Found empty details in array dvPortGroupInfo.",2);
					$networkData{network}	= "";
				}
				else
				{
					foreach my $dvPortGroup ( @$dvPortGroupInfo )
					{
						if ( $dvPortGroup->{portgroupKey} eq $Map_KeyNic{$net_bless->deviceConfigId}{portgroupKey} )
						{
							$networkData{network}	= $dvPortGroup->{portgroupNameDVPG};
							last;
						}
					}
				}
			}
			
			my $Mac_IP 			= "";
			foreach my $ip ( @$ip_address_array )
			{
				if ( $ip !~ /::/ )
				{
					if ( !length ( $ip_address ) )
					{
						$ip_address = $ip;
						$Mac_IP 	= $ip;
						next;
					}
					$ip_address 	= $ip_address.",".$ip;
					
					if ( !length ( $Mac_IP ) )
					{
						$Mac_IP 	= $ip;
						next;
					}
					$Mac_IP			= $Mac_IP.",".$ip;
				}
			}
			$networkData{macAssociatedIp}	= $Mac_IP;
			
			if ( defined $net_bless->dnsConfig )
			{
				$networkData{isDhcp}= $net_bless->dnsConfig->dhcp;	
				my $DNS_ip_array 	= $net_bless->dnsConfig->ipAddress;
				foreach my $dns_ip_val ( @$DNS_ip_array )
				{
					if ( !length( $dns_ip ) )
					{
						$dns_ip 	= $dns_ip_val;
						next;
					}
					$dns_ip			= $dns_ip.",".$dns_ip_val;	
				} 
				$networkData{dnsIp}	= $dns_ip;	
			}	
			else
			{
				#this is an ESX of version less than ESX 4.1 
				$networkData{isDhcp}= "";	
				$networkData{dnsIp}	= "";	
			}
			push @networkList , \%networkData;
		}
	}
	else
	{
		my $dns_ip 			= "";
		my $dhcp 			= "";
		if ( defined( Common_Utils::GetViewProperty( $vmView,'guest.ipAddress') ) && ( Common_Utils::GetViewProperty( $vmView,'guest.ipAddress')  ne "" ) )
		{
			$ip_address 	= Common_Utils::GetViewProperty( $vmView,'guest.ipAddress') ;
			if( defined( Common_Utils::GetViewProperty( $vmView,'guest.ipStack') ) )
			{
				 my $ipStack= Common_Utils::GetViewProperty( $vmView,'guest.ipStack');
				 foreach ( @$ipStack )
				 {
				 	$dhcp				= $_->dnsConfig->dhcp;
				 	my $DNS_ip_array 	= $_->dnsConfig->ipAddress;
					foreach my $dns_ip_val ( @$DNS_ip_array )
					{
						if ( !length( $dns_ip ) )
						{
							$dns_ip 	= $dns_ip_val;
							next;
						}
						$dns_ip			= $dns_ip.",".$dns_ip_val;	
					} 						
				 }				 
			}
			
			foreach my $device ( @$devices )
			{
				if ( $device->deviceInfo->label =~ /^Network adapter/i )
				{
					my %networkData 	= ();
					$networkData{isDhcp}= $dhcp;
					$networkData{dnsIp}	= $dns_ip;
					$networkData{macAssociatedIp}= $ip_address;
					$networkData{macAddress}	= $device->macAddress;
					$networkData{networkLabel} 	= $device->deviceInfo->label;
					$networkData{adapterType}	= ref( $device );
					$macInformationFetched{$device->macAddress} = $networkData{networkLabel};
					if( $device->deviceInfo->summary =~ /DistributedVirtualPortBacking/ )
					{
						my @tempArr	= @$dvPortGroupInfo;
						if ( -1 == $#tempArr )
						{
							Common_Utils::WriteMessage("ERROR :: Found empty list in array dvPortGroupInfo.",2);
							$networkData{network}	= "";
						}
						foreach my $dvPortGroup ( @$dvPortGroupInfo )
						{
							if ( $device->backing->{port}->portgroupKey eq $dvPortGroup->{portgroupKey} )
							{
								$networkData{network}	= $dvPortGroup->{portgroupNameDVPG};
							}
						}
					}
					else
					{
						$networkData{network} 	= $device->deviceInfo->summary;
					}
					push @networkList , \%networkData;
				}
			}			
		}
	}
	
	foreach my $mac ( sort keys %macAddress )
	{
		if ( exists $macInformationFetched{$mac} )
		{
			next;
		}
		my %networkData					= ();
		$networkData{networkLabel}		= $macAddress{$mac};
		$networkData{network}			= "";
		$networkData{macAddress}		= $mac;
		$networkData{macAssociatedIP}	= "";
		$networkData{dnsIp}				= "";
		$networkData{isDhcp}			= "";
		push @networkList , \%networkData;
	} 
			
	if (!length($ip_address))
	{
		$ip_address = "NOT SET";
	}
	return ( $Common_Utils::SUCCESS , $ip_address , \@networkList );
}

#####IsVmHavingDifferentControllerTypes#####
##Description	:: Checks if the VM is having controller og Different types. having different controllers is a problem at
##					booting time.
##Input			:: VM View in which we need to check.
##Output		:: Returns Whether it has different Controller or not. Different Controllers = 1, if not = 0.
#####IsVmHavingDifferentControllerTypes#####
sub IsVmHavingDifferentControllerTypes
{
	my $vmView 			= shift;
	my $deviceInfo	 	= Common_Utils::GetViewProperty( $vmView,'config.hardware.device');
	my $differentCont	= 0;
	my $controller_name = "";
	my $vmName 			= $vmView->name;
	foreach my $device ( @$deviceInfo )
	{
		if ( exists $device->{scsiCtlrUnitNumber} )
		{
			if (! length( $controller_name) )
			{
				$controller_name 	 	= $device->deviceInfo->summary;
			}
			my $compare_to_controller 	= $device->deviceInfo->summary;
			if( $controller_name ne $compare_to_controller )
			{
				$differentCont			= 1;
				Common_Utils::WriteMessage("Multiple controller types exist in machine $vmName.",2);
			}
		}
	}
	return ( $differentCont );
}

#####FindOccupiedScsiIds#####
##Description 	:: Finds the Disk information. It maps SCSI ID and Disk Name.
##Input 		:: Master Target View and address of Hash.
##Output		:: Returns SUCCESS or FAILURE.
#####FindOccupiedScsiIds#####
sub FindOccupiedScsiIds
{
	my $mtView 		= shift;
	my $MapDiskInfo	= shift;
	
	my $devices 	= Common_Utils::GetViewProperty( $mtView,'config.hardware.device');
	foreach my $device ( @$devices )
	{
		$$MapDiskInfo{ $device->key } = $device->deviceInfo->label;
		if ( $device->deviceInfo->label =~ /Hard Disk/i )
		{
			$$MapDiskInfo{ $device->key } = $device->backing->fileName;
			$$MapDiskInfo{ $device->backing->fileName } = $device->key;
		}
	}
	return ( $Common_Utils::SUCCESS );
}


#####FindScsiController#####
##Description 	:: Finds SCSI controller Information in machine View.
##Input 		:: Machine View and SCSI controller key we need to search for.
##Output 		:: Returns SUCCESS and Controller Key of SCSI Controller else FAILURE.
#####FindScsiController#####
sub FindScsiController
{
	my ( $vmView , $key ) 	= @_;
   
    my $devices 			= Common_Utils::GetViewProperty( $vmView,'config.hardware.device');

    foreach my $device (@$devices)
	{
		if ( $key == $device->key )
		{
			Common_Utils::WriteMessage("Found SCSI controller with Key $key.",2);
			return ( $Common_Utils::SUCCESS );
		}
	}
    return ( $Common_Utils::FAILURE );
}

#####RemoveDisk#####
##Description	:: Removes VMDK from a VM.
##Input 		:: Disk information(Its disk name) and machine view from which disk has to be removed.
##Output 		:: Returns either SUCCESS or FAILURE.
#####RemoveDisk#####
sub RemoveDisk
{
	my %args				= @_;
	my $failed 				= $Common_Utils::SUCCESS;
	my $machineView 		= $args{vmView};
	my $devices				= Common_Utils::GetViewProperty( $machineView,'config.hardware.device');
	my $displayName 		= $machineView->name;
	
	foreach my $diskToBeRemoved ( @{$args{diskInfo}} )
	{
		foreach my $device ( @$devices )
		{
			if ( $device->deviceInfo->label !~ /Hard Disk/i )
			{
				next;
			}
			
			if ( $diskToBeRemoved ne ( $device->backing->fileName ) )
			{
				next;
			}
			
			my $returnCode	= RemoveVmdk(
											fileName 		=> $device->backing->fileName,
											fileSize		=> $device->capacityInKB,
											diskMode		=> $device->backing->diskMode,
											controllerKey	=> $device->controllerKey,
											key				=> $device->key,
											operation		=> $args{operation},
											vmView			=> $machineView,
											displayName		=> $displayName,
										);
			if ( $returnCode ne $Common_Utils::SUCCESS )
			{
				$failed		= $Common_Utils::FAILURE;
			}
		}	
	} 
	return ( $failed );
} 

#####CreateDiskRemovalSpec#####
##Description 		:: Creates a Disk removal spec. it creates spec to removes/delete a VMDK.
##Input 			:: Disk Name, Disk Size, disk Mode, Controller Key, Key and operation=> Remove/Delete.
##Output 			:: Return SUCCESS else FAILURE.
#####CreateDiskRemovalSpec#####
sub CreateDiskRemovalSpec
{
	my %args 			= @_;
	Common_Utils::WriteMessage("$args{operation}: disk $args{fileName} from machine $args{machineName} of size $args{fileSize} at SCSI controller number $args{controllerKey} and unit number $args{key} in mode $args{diskMode}.",2);
	my $diskBackingInfo = VirtualDiskFlatVer2BackingInfo->new( diskMode => $args{diskMode},fileName => $args{fileName} );
	my $disk 			= VirtualDisk->new( controllerKey => $args{controllerKey},key => $args{key},backing => $diskBackingInfo,capacityInKB => $args{fileSize});
	my $devspec			= "";
	if ( "destroy" eq lc( $args{operation} ) )
	{	
		$devspec 		= VirtualDeviceConfigSpec->new(operation => VirtualDeviceConfigSpecOperation->new('remove'),device => $disk, 
														fileOperation => VirtualDeviceConfigSpecFileOperation->new($args{operation}));
	}
	else
	{
		$devspec 		= VirtualDeviceConfigSpec->new(operation => VirtualDeviceConfigSpecOperation->new('remove'),device => $disk );
		
	}
	
	return ( $Common_Utils::SUCCESS , $devspec );
}

#####RemoveVmdk#####
##Description		:: Removes vmdk form virtual machine. At any case this is an individual function which removes VMDK from virtal machine.
##Input 			:: Spec to remove VMDK, what is the VMDK file Name, Size , Controller Key etc.
##Output 			:: Returns SUCCESS else FAILURE.
#####RemoveVmdk#####
sub RemoveVmdk
{
	my %args 			= @_;
	Common_Utils::WriteMessage("Removing disk $args{fileName} from machine $args{displayName} of size $args{fileSize} at SCSI controller number $args{controllerKey} and unit number $args{key} in mode $args{diskMode}.",2);
	my $diskBackingInfo = VirtualDiskFlatVer2BackingInfo->new(diskMode => $args{diskMode},fileName => $args{fileName});
	my $disk 			= VirtualDisk->new( controllerKey => $args{controllerKey},key => $args{key},backing => $diskBackingInfo,capacityInKB => $args{fileSize});
	my $devspec			= "";
	if ( "destroy" eq lc( $args{operation} ) )
	{	
		$devspec 		= VirtualDeviceConfigSpec->new(operation => VirtualDeviceConfigSpecOperation->new('remove'),device => $disk, 
														fileOperation => VirtualDeviceConfigSpecFileOperation->new($args{operation}));
	}
	else
	{
		$devspec 		= VirtualDeviceConfigSpec->new(operation => VirtualDeviceConfigSpecOperation->new('remove'),device => $disk );
		
	}
	
	my $vmspec 			= VirtualMachineConfigSpec->new( deviceChange => [$devspec] );
	
	for ( my $i = 0 ;; $i++ )
	{	
		eval 
		{			
			$args{vmView}->ReconfigVM( spec => $vmspec );
		};
		if ($@)
		{
			if ( ref($@->detail) eq 'TaskInProgress' )
			{
				Common_Utils::WriteMessage("RemoveVmdk :: waiting for 3 secs as another task is in progress.",2);
				sleep(3);
				next;
			}
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			Common_Utils::WriteMessage("ERROR :: Unable to detach disk \"$args{fileName}\" from master target $args{displayName}.",3);
			return ( $Common_Utils::FAILURE );
		}
		last;
	}
	Common_Utils::WriteMessage("Successfully removed \"$args{fileName}\" from master target $args{displayName}.",2);
	return ( $Common_Utils::SUCCESS );
}

#####CreateVmdk#####
##Description 	:: Creates VMDK, this is a substitute for AddVmdk functions.Even in future we need to use this.
##Input 		:: Disk information like, diskMode , vmdk name , thin or thick , Size , SCSI Contoller Key and Unit number.
##Output 		:: Returns SUCCESS or FAILURE.
#####CreateVmdk#####
sub CreateVmdk
{
 	my %args 				= @_;
 	
 	Common_Utils::WriteMessage("Creating disk $args{fileName} of size $args{fileSize} at SCSI unit number $args{unitNumber} of type $args{diskType} in mode $args{diskMode}.",2);
	my $diskBackingInfo		= VirtualDiskFlatVer2BackingInfo->new( diskMode => $args{diskMode} , fileName => $args{fileName} ,
																	thinProvisioned =>$args{diskType} );
	if(( exists $args{clusterDisk} ) && ("yes" eq $args{clusterDisk} ))
	{
		$diskBackingInfo	= VirtualDiskFlatVer2BackingInfo->new( diskMode => $args{diskMode}, fileName => $args{fileName},
																	eagerlyScrub => "true", thinProvisioned => "false" );
	}
																	
	my $connectionInfo		= VirtualDeviceConnectInfo->new( allowGuestControl => "false" , connected => "true" ,startConnected => "true" );
	my $virtualDisk			= VirtualDisk->new( controllerKey => $args{controllerKey} , unitNumber => $args{unitNumber} ,key => -1, 
												backing => $diskBackingInfo, capacityInKB => $args{fileSize} );
												
	my $virtualDeviceSpec 	= VirtualDeviceConfigSpec->new(operation => VirtualDeviceConfigSpecOperation->new('add'),device => $virtualDisk,
																fileOperation => VirtualDeviceConfigSpecFileOperation->new('create'));
	
	my $vmConfigSpec 		= VirtualMachineConfigSpec->new( deviceChange => [$virtualDeviceSpec] );
	
	for ( my $i = 0 ;; $i++ )
	{
		eval
		{
			$args{vmView}->ReconfigVM( spec => $vmConfigSpec );
		};
		if ( $@ )
		{
			if ( ref($@->detail) eq 'TaskInProgress' )
			{
				Common_Utils::WriteMessage("CreateVmdk :: waiting for 3 secs as another task is in progress.",2);
				sleep(3);
				next;
			}
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			Common_Utils::WriteMessage("ERROR :: Failed to create disk",3);
			return ( $Common_Utils::FAILURE );	
		}
		last;
	}
	Common_Utils::WriteMessage("Successfully created disk $args{fileName}.\n",2);
	return ( $Common_Utils::SUCCESS );
}

#####DeleteDummyDisk#####
##Description 	:: Delete dummy disk from a machine. In this file this will be called when there is failure while creating
##					dummy disk.Will delete a dummy disk which has "Created = yes" in dummy disk information.
##Input 		:: Array of hashes. Each of hash contains dummy disk information.
##Output 		:: Returns SUCCESS on Deletion else FAILURE.
#####DeleteDummyDisk#####
sub DeleteDummyDisk
{
	my $dummyDiskDevices	= shift;
	my $masterTargetView	= shift;
	
	my @deviceChange	= ();
	foreach my $device ( @$dummyDiskDevices )
	{
		Common_Utils::WriteMessage("Removing disk $device->{backing}->{fileName} of size $device->{capacityInKB} at SCSI unit number $device->{key} .",2);
		
		my $devspec			= VirtualDeviceConfigSpec->new(operation => VirtualDeviceConfigSpecOperation->new('remove'),device => $device, 
															fileOperation => VirtualDeviceConfigSpecFileOperation->new("destroy"));
		
		push @deviceChange, $devspec;
	}
	
	my $returnCode	= ReconfigVm( vmView => $masterTargetView, changeConfig => { deviceChange => \@deviceChange } );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	Common_Utils::WriteMessage("Successfully deleted all the dummy disks from $masterTargetView->{name}.",2);
	return $Common_Utils::SUCCESS;								 
}

#####GetVmViewInDatacenter#####
##Description		:: Gets the view of all the machines in a datacemter.
##Input 			:: DatacenterView in which we need to all the machine view.
##Output 			:: Returns SUCCESS else FAILURE.
#####GetVmViewInDatacenter#####
sub GetVmViewInDatacenter
{
	my $datacenterView 		= shift;
	my $datacenterName 		= $datacenterView->name;
	
	my $vmViews				= Vim::find_entity_views( view_type => 'VirtualMachine' , begin_entity => $datacenterView, properties => GetVmProps() );
	if ( ! $vmViews )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get virtual machine views under datacenter \"$datacenterName\".",3);
		return ( $Common_Utils::FAILURE );
	}

	return ( $Common_Utils::SUCCESS , $vmViews );
}

#####AddSCSIController#####
##Description 	:: Adds an empty SCSI controller to a Machine.
##Input 		:: Virtual Machine View, SCSI controller Key value.
##Output 		:: Returns SUCCESS on addition of controller else FAILURE.
#####AddSCSIController#####
sub AddSCSIController
{
	my %args 			= @_; 
	
	my $connectionInfo	= VirtualDeviceConnectInfo->new( allowGuestControl => "false" , connected => "true" ,startConnected => "true" );
	my $controller 		= "";
	if( "virtualahcicontroller" eq lc($args{controllerType}) )
	{
		$controller		= $args{controllerType}->new( key => -1,device => [0],busNumber => $args{busNumber},connectable => $connectionInfo);
	}
	else
	{
		$controller		= $args{controllerType}->new( key => -1,device => [0],busNumber => $args{busNumber},connectable => $connectionInfo,
                          									sharedBus => VirtualSCSISharing->new($args{sharing}));
	}
   	my $controllerspec = VirtualDeviceConfigSpec->new(	device => $controller,
                     												operation => VirtualDeviceConfigSpecOperation->new('add'));
                     
	my $vmConfigSpec 	= VirtualMachineConfigSpec->new( deviceChange => [$controllerspec] );    
	
	for ( my $i = 0 ;; $i++ )
	{
		eval
		{
			$args{vmView}->ReconfigVM( spec => $vmConfigSpec );
		};
		if ( $@ )
		{
			if ( ref($@->detail) eq 'TaskInProgress' )
			{
				Common_Utils::WriteMessage("AddScsiController :: waiting for 3 secs as another task is in progress.",2);
				sleep(3);
				next;
			}
			Common_Utils::WriteMessage("ERROR :: $@",3);
			Common_Utils::WriteMessage("ERROR :: Failed to add SCSI controller.",3);
			return ( $Common_Utils::FAILURE );	
		}
		last;
	}
	
	Common_Utils::WriteMessage("Successfully added an Empty SCSI Controller. Bus Number = $args{busNumber}",2);        
	return ( $Common_Utils::SUCCESS );
} 

#####findDatacenterNameOfVm#####
##Description	:: finds the datacenter name in whcih the VM exists.
##Input			:: Machine UUID.
##Output 		:: Returns SUCCESS and DC NAME or FAILURE.
#####findDatacenterNameOfVm#####
sub findDatacenterNameOfVm
{
	my $vmView 				= shift;
	my $datacenterName		= "";
	my $machineName 		= $vmView->name;
	Common_Utils::WriteMessage("Find the datacenter name for machine $machineName.",2);
	
	my( $returnCode , $datacenterViews )		= DatacenterOpts::GetDataCenterViews();
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		return $Common_Utils::FAILURE;
	}
	
	( $returnCode , my $folderViews ) 			= FolderOpts::GetFolderViews();
	if ( $Common_Utils::SUCCESS != $returnCode )
	{
		return $Common_Utils::FAILURE;
	}
	
	( $returnCode , my $mapFolderToChildFolder )= FolderOpts::MapFolderToItsChildFolder( $folderViews ); 
	if ( $Common_Utils::SUCCESS  != $returnCode )
	{
		return $Common_Utils::FAILURE;
	}
	my %mapFolderToChildFolder					= %$mapFolderToChildFolder;
	
	( $returnCode , my $mapDataCenterGroupName )= DatacenterOpts::MapDataCenterGroupName( $datacenterViews ); 
	if ( $Common_Utils::SUCCESS  != $returnCode )
	{
		return $Common_Utils::FAILURE;
	}
	my %mapDataCenterGroupName	= %$mapDataCenterGroupName;

	my $folderGroup				= $vmView->parent->value;
	$datacenterName				= $mapDataCenterGroupName{ $folderGroup };
	if ( ! defined ( $datacenterName ) )
	{
		$datacenterName			= $mapDataCenterGroupName{ $mapFolderToChildFolder{ $folderGroup } };
	}
	Common_Utils::WriteMessage("Machine \"$machineName\" is in datacenter $datacenterName.",2);
	return ( $Common_Utils::SUCCESS , $datacenterName );
}

#####CompareVmdk#####
##Description 		:: Compares VMDK of the machine with the information we have and maps to the VM. This is used to find machine
##						when the UUID is blank or if we have multiple machines with same UUID.
##Input 			:: Disks Info read from XML and vmView with which we need to compare.
##Output 			:: Returns SUCCESS else FAILURE.
#####CompareVmdk#####
sub CompareVmdk
{
	my $disksInfo 		= shift;
	my $vmView 			= shift;
	my $additionOfDisk 	= shift;
	my $drFolderPath	= shift;
	my $vSanFolderPath	= shift;
	
	my @vmdkFound		= ();
	my %fileNames 		= ();
	my $machineName 	= $vmView->name;
	my $vmPathName 		= Common_Utils::GetViewProperty( $vmView,'summary.config.vmPathName');
	
	my $uuidComparision	= 0;
	foreach my $vmdk ( @$disksInfo )
	{
		my $diskUuid = $vmdk->getAttribute('target_disk_uuid');
		if( ( defined $diskUuid) && ( $diskUuid ne "" ) )
		{
			Common_Utils::WriteMessage("Proceeding with disk uuid comparision.",2);
			$uuidComparision	= 1;
			last;
		}
	}
	
	foreach my $vmdk ( @$disksInfo )
	{
		my $found		= 0;
		my $vmdkName 	= $vmdk->getAttribute('disk_name');
		my @vmdkNameXml = split( /\[|\]/ , $vmdkName );
			
		my $vSanFileName= "";
		if ( defined( $drFolderPath ) && ( "" ne $drFolderPath ) )
		{
			Common_Utils::WriteMessage("drFolderPath = $drFolderPath.",2);
			$vmdkNameXml[ $#vmdkNameXml ]	=~ s/^\s+//;
			$vmdkNameXml[ $#vmdkNameXml ]	= " $drFolderPath/$vmdkNameXml[ $#vmdkNameXml ]";
		}
		
		if ( defined( $vSanFolderPath ) && ( "" ne $vSanFolderPath ) )
		{
			Common_Utils::WriteMessage("vSanFolderPath = $vSanFolderPath.",2);
			my @diskPath	= split( /\// , $vmdkName );
			$vSanFileName	= " $vSanFolderPath" . "$diskPath[-1]";
		}
				
		my $devices 	= Common_Utils::GetViewProperty( $vmView,'config.hardware.device');
		my $diskUuid 	= $vmdk->getAttribute('target_disk_uuid');
		if( $uuidComparision )
		{
			if( ( defined $diskUuid) && ( $diskUuid ne "" ) )
			{
				foreach my $device ( @$devices )
				{
					if ( $device->deviceInfo->label =~ /Hard Disk/i )
					{
						my $vmdkUuid		= $device->backing->uuid;
						if ( ( exists $device->{backing}->{compatibilityMode} ) && ( 'physicalmode'eq lc( $device->backing->compatibilityMode ) ) )
						{
							$vmdkUuid	= $device->backing->lunUuid;
						}
						if( $vmdkUuid eq $diskUuid )
						{
							$found				= 1;
							my $diskName 		= $device->backing->fileName;
							my @vmdkNameTarget 	= split(/\[|\]/,$diskName);
							Common_Utils::WriteMessage("Found disk $diskName based on uuid.",2);
							
							if ( "yes" eq lc ( $additionOfDisk ) )
							{
								$vmdk->setAttribute('protected','yes');
								Common_Utils::WriteMessage("Written protected = yes for $vmdkName.",2);
							}
							$vmdk->setAttribute('datastore_selected', $vmdkNameTarget[1] );
							push @vmdkFound , $vmdkName;
							last;
						}
					}
				}
			}
			else
			{
				Common_Utils::WriteMessage("Uuid not found for disk $vmdkName.",2);
			}
		}
		else
		{
			foreach my $device ( @$devices )
			{
				if ( $device->deviceInfo->label =~ /Hard Disk/i )
				{
					my $key_file = $device->key;		
					my $diskName =  $device->backing->fileName;
					if( defined $vmView->snapshot )
					{
						my $diskLayoutInfo 		= Common_Utils::GetViewProperty( $vmView,'layout.disk');
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
								$diskName = $disk;
								Common_Utils::WriteMessage("Base disk information collected for key( $key_file ) = $diskName.",2);
							}
						}
					}
	
					my @vmdkNameTarget 		= split(/\[|\]/,$diskName);
					if ( !exists $fileNames{$diskName} )
					{
						$fileNames{$diskName}	= $machineName;
					}
					
					$vmdkNameXml[ $#vmdkNameXml ]		=~ s/_InMage_NC_(\d)+//;
					$vmdkNameTarget[ $#vmdkNameTarget ]	=~ s/_InMage_NC_(\d)+//;
					$vSanFileName						=~ s/_InMage_NC_(\d)+//;
					Common_Utils::WriteMessage("Comparing $vmdkNameXml[-1] or $vSanFileName and $vmdkNameTarget[-1].",2);
					if( ( $vmdkNameXml[ $#vmdkNameXml ] eq $vmdkNameTarget[ $#vmdkNameTarget ] ) || ( $vSanFileName eq $vmdkNameTarget[ $#vmdkNameTarget ]) )
					{
						$found				= 1;
						if ( "yes" eq lc ( $additionOfDisk ) )
						{
							$vmdk->setAttribute('protected','yes');
							Common_Utils::WriteMessage("Written protected = yes for $vmdkNameXml[ $#vmdkNameXml ].",2);
						}
						$vmdk->setAttribute('datastore_selected', $vmdkNameTarget[ $#vmdkNameTarget - 1 ] );
						push @vmdkFound , $vmdkName;
						last;
					}
				}
			}
		}
		if( ! $found )
		{
			if ( "yes" eq lc ($additionOfDisk ) )
			{
				$vmdk->setAttribute('protected','no');
			}
		}					
	}
	
	if( -1 != $#vmdkFound )
	{
		return ( $Common_Utils::EXIST );
	}

	return ( $Common_Utils::NOTFOUND );
}

#####CreateVmdkSpecs#####
##Description 	:: Create multiple VMDKs, this is a substitute for AddVmdk functions.Even in future we need to use this.
##Input 		:: Disk information like, diskMode , vmdk name , thin or thick , Size , SCSI Contoller Key and Unit number.
##Output 		:: Returns SUCCESS or FAILURE.
#####CreateVmdkSpecs#####
sub CreateVmdkSpecs
{
 	my %args 				= @_;
 	my $vmName				= $args{vmView}->name;
 	my @deviceChange		= ();
 	my $key					= -1;
 	
 	foreach my $diskInfo ( @{ $args{disksInfo} } )
 	{
	 	Common_Utils::WriteMessage("Creating disk $$diskInfo{fileName} of size $$diskInfo{fileSize} at SCSI unit number $$diskInfo{unitNumber} of type $$diskInfo{diskType} in mode $$diskInfo{diskMode}.",2);
		my $diskBackingInfo		= VirtualDiskFlatVer2BackingInfo->new( diskMode => $$diskInfo{diskMode} , fileName => $$diskInfo{fileName} ,
																		thinProvisioned =>$$diskInfo{diskType} );
		if(( exists $$diskInfo{clusterDisk} ) && ("yes" eq $$diskInfo{clusterDisk} ))
		{
			$diskBackingInfo	= VirtualDiskFlatVer2BackingInfo->new( diskMode => $$diskInfo{diskMode}, fileName => $$diskInfo{fileName},
																		eagerlyScrub => "true", thinProvisioned => "false" );
		}
																		
		my $connectionInfo		= VirtualDeviceConnectInfo->new( allowGuestControl => "false" , connected => "true" ,startConnected => "true" );
		my $virtualDisk			= VirtualDisk->new( controllerKey => $$diskInfo{controllerKey} , unitNumber => $$diskInfo{unitNumber} ,key => $key, 
													backing => $diskBackingInfo, capacityInKB => $$diskInfo{fileSize} );
													
		my $virtualDeviceSpec 	= VirtualDeviceConfigSpec->new(operation => VirtualDeviceConfigSpecOperation->new('add'),device => $virtualDisk,
																	fileOperation => VirtualDeviceConfigSpecFileOperation->new('create'));
		push @deviceChange, $virtualDeviceSpec;
		$key--;
	}
	
	return ( $Common_Utils::SUCCESS, \@deviceChange);
}

#####GetVmViewByVmPathName#####
##Description 		:: Retrives virtual machine view using VM path name.
##Input 			:: VM Path Name.
##Output 			:: Returns SUCCESS or FAILURE.
#####GetVmViewByVmPathName#####
sub GetVmViewByVmPathName
{
	my $vmPathName 		= shift;
	my $hostView		= shift;
	
	my $vmViews			= Vim::find_entity_views( view_type => "VirtualMachine" , begin_entity => $hostView , properties => GetVmProps(),
													filter => { 'summary.config.vmPathName' => $vmPathName } );
	
	if ( ! $vmViews )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find view of machine with vmx path name as \"$vmPathName\".",3);
		return $Common_Utils::FAILURE;
	}
	
	my @tempArr	= @$vmViews;
	if ( ( $#tempArr ) > 0 )
	{
		Common_Utils::WriteMessage("ERROR :: Found multiple views with vmx path \"$vmPathName\".",2);
		return $Common_Utils::FAILURE;
	}
	  
	return ( $Common_Utils::SUCCESS , $$vmViews[0] );
}

#####OnOffMachine#####
##Description		:: It will power on and off the virtual machine. This is used only in the case of Linux machines.
##Input 			:: machine UUID.
##Output 			:: Returns SUCCESS else FAILURE.
#####OnOffMachine#####
sub OnOffMachine
{
	my $vmView 		= shift;
	my $returnCode	= $Common_Utils::SUCCESS;
	
	for ( my $i = 1 ; $i <=3 ; $i++ )
	{
		$returnCode = PowerOn( $vmView );
		if ( ( $returnCode == $Common_Utils::SUCCESS ) || ( $i == 3 ) )
		{
			last;
		}
		sleep( $i*10 );
	}
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}

	for ( my $i = 1 ; $i <=3 ; $i++ )
	{
		$returnCode = PowerOff( $vmView );
		if ( ( $returnCode == $Common_Utils::SUCCESS ) || ( $i == 3 ) )
		{
			last;
		}
		sleep( $i*10 );
	}
	
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	return ( $Common_Utils::SUCCESS ); 
}

#####RegisterVm#####
##Description		:: Registers a virtual machine to the inventory.
##Input 			:: vmPath Name , VM name , folder view, resourcepool view , host view.
##Output 			:: Returns SUCCESS else FAILURE.
#####RegisterVm#####
sub RegisterVm
{
	my %args 			= @_;
	my $registeredView	= "";
	my $ReturnCode		= $Common_Utils::SUCCESS;
	if ( ( !defined $args{hostView} ) || ( "" eq $args{hostView} ) )
	{
		Common_Utils::WriteMessage("ERROR :: An unblessed reference is sent for parameter 'hostView'.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	if ( ( !defined $args{resourcepoolView} ) || ( "" eq $args{resourcepoolView} ) )
	{
		Common_Utils::WriteMessage("ERROR ::  An unblessed reference is sent for parameter 'resourcepoolView'",3);
		return ( $Common_Utils::FAILURE );
	}
	
	if ( ( !defined $args{folderView} ) || ( "" eq $args{folderView} ) )
	{
		Common_Utils::WriteMessage("ERROR :: An unblessed reference is sent as 'folderView'.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	eval
	{
		my $task 		= $args{folderView}->RegisterVM_Task(	
															path	 	=> $args{vmPathName},
															name		=> $args{vmName},
															asTemplate	=> 'false',
															pool		=> $args{resourcepoolView},
															host		=> $args{hostView}
														);
		my $task_view 	= Vim::get_view( mo_ref => $task );
	    while ( 1 ) 
	    {
	        my $info 	= $task_view->info;
	        if( $info->state->val eq 'running' )
	        {
	        	Common_Utils::WriteMessage("registering of machine $args{vmName} in progress.",2);
	        } 
	        elsif( $info->state->val eq 'success' ) 
	        {
	           Common_Utils::WriteMessage("Virtual machine \"$args{vmName}\" is registered to vSphere.",2);
	           $registeredView	= Vim::get_view( mo_ref => $info->result );
	           last;
	        } 
	        elsif( $info->state->val eq 'error' ) 
	        {
	        	my $soap_fault 	= SoapFault->new;
	            my $errorMesg 	= $soap_fault->fault_string($info->error->localizedMessage);
	            Common_Utils::WriteMessage("ERROR :: Registering a virtual machine \"$args{vmName}\" failed with error \"$errorMesg\".",3);
	            $ReturnCode 	= $Common_Utils::FAILURE;
	            last;
	        }
	        sleep 2;
	        $task_view->ViewBase::update_view_data();
	    }														
	};
	if ( @$ )
	{
		if (ref($@) eq 'SoapFault') 
		{
	        if (ref($@->detail) eq 'AlreadyExists') 
	        {
	        	Common_Utils::WriteMessage("ERROR :: The specified key, name, or identifier already exists.",3);
			}
	        elsif (ref($@->detail) eq 'OutOfBounds') 
	        {
	            Common_Utils::WriteMessage("ERROR ::Maximum Number of Virtual Machines has been exceeded.",3);
			}
	        elsif (ref($@->detail) eq 'InvalidArgument') 
	        {
	            Common_Utils::WriteMessage("ERROR :: A specified parameter was not correct.",3);
	        }
	        elsif (ref($@->detail) eq 'DatacenterMismatch') 
	        {
	            Common_Utils::WriteMessage("ERROR :: Datacenter Mismatch: The input arguments had entities,that did not belong to the same datacenter.",3);
	        }
	        elsif (ref($@->detail) eq "InvalidDatastore") 
	        {
	            Common_Utils::WriteMessage("ERROR :: Invalid datastore path: $args{vmPathName}.",3);
	        }
	        elsif (ref($@->detail) eq 'NotSupported') 
	        {
	            Common_Utils::WriteMessage("ERROR :: Operation is not supported.",3);
	        }
	        elsif (ref($@->detail) eq 'InvalidState') 
	        {
	            Common_Utils::WriteMessage("ERROR :: The operation is not allowed in the current state.",3);
	        }
	        else 
	        {
	            Common_Utils::WriteMessage("ERROR :: $@ .",3);
	        }
	        $ReturnCode 	= $Common_Utils::FAILURE;
       	}
       	else 
       	{
       		Common_Utils::WriteMessage("ERROR :: $@ .",3);
       		$ReturnCode 	= $Common_Utils::FAILURE;
       	}
	}
	Common_Utils::WriteMessage("Successfully registered $args{vmName} to the inventory.",2);
	return ( $ReturnCode, $registeredView );
}

#####UnregisterVm#####
##Description		:: Unregisters a machine from ESX/vSphere.
##Input				:: Virtual Machine view.
##Output			:: Returns SUCCESS else FAILURE.
#####UnregisterVm#####
sub UnregisterVm
{
	my %args		= @_;
	my $vmName 		= $args{vmView}->name;
	eval 
	{
         $args{vmView}->UnregisterVM();
   	};
    if ($@) 
    {
    	if (ref($@) eq 'SoapFault') 
    	{
    		if (ref($@->detail) eq 'InvalidPowerState') 
    		{
    			Common_Utils::WriteMessage("ERROR :: Virtual Machine '$vmName' must be powered off.",3);
            }
            elsif (ref($@->detail) eq 'HostNotConnected') 
            {
            	Common_Utils::WriteMessage("ERROR :: Unable to communicate with the remote host.",3);
            }
            else
            {
            	Common_Utils::WriteMessage("ERROR :: \"  $@  \".",3);
            }
        }
        else 
        {
        	Common_Utils::WriteMessage("ERROR :: \"  $@ \" .",3);
        }
		Common_Utils::WriteMessage("ERROR :: Unregistration failed for Virtual Machine $vmName.",3);
	}
	Common_Utils::WriteMessage("Successfully unregistered machine $vmName.",3);
	return ( $Common_Utils::SUCCESS );
}

#####MergeSnapshots#####
##Description 		:: Merges Snapshots at the time of Failback and Resume.
##Input 			:: vmView.
##Output 			:: Returns 0 on successful merging else failure.
#####MergeSnapshots#####
sub MergeSnapshots
{
	my $vmView 		= shift;
	my $vmName		= $vmView->name;
	my $ReturnCode	= $Common_Utils::SUCCESS;
	
	eval
	{
		my $task 		= $vmView->RemoveAllSnapshots_Task();
		my $task_view 	= Vim::get_view( mo_ref => $task );
	    while ( 1 ) 
	    {
	    	
	        my $info 	= $task_view->info;
	        if( $info->state->val eq 'running' )
	        {
	        } 
	        elsif( $info->state->val eq 'success' ) 
	        {
	           Common_Utils::WriteMessage("\nMerging snapshots on machine $vmName.. in progress.",2);
	           last;
	        } 
	        elsif( $info->state->val eq 'error' ) 
	        {
	        	my $soap_fault 	= SoapFault->new;
	            my $errorMesg 	= $soap_fault->fault_string($info->error->localizedMessage);
	            Common_Utils::WriteMessage("ERROR :: Failed to merge snapshots on machine $vmName, due to error \"$errorMesg\".",3);
	            $ReturnCode		= $Common_Utils::FAILURE;
	            last;
	        }
	        sleep 2;
	        $task_view->ViewBase::update_view_data();
	     }
		
	};
	if($@)
	{
		Common_Utils::WriteMessage("ERROR :: Unable to merge snapshots on machine \"$vmName\".",3);
		Common_Utils::WriteMessage("ERROR :: $@.",3);
		$ReturnCode		= $Common_Utils::FAILURE;
	}
	
	return ( $ReturnCode );
}

#####ReconfigVm#####
##Description		:: Reconfigures virtual machine.
##Input 			:: args are vmview and configSpecs in a hash, where key is property and value is property value
##Output 			:: Return SUCCESS or FAILURE.
#####ReconfigVm#####
sub ReconfigVm
{
	my %args 						= @_;	
	my $vmConfigSpec 				= VirtualMachineConfigSpec->new();
	
	foreach my $key ( keys %{$args{changeConfig}} )
	{
		$vmConfigSpec->{$key}	= $args{changeConfig}{$key};
	}
	
	#todo.. shall we implement time for 5 minutes?
	for ( my $i = 0 ;; $i++  )
	{
		eval 
		{			
			$args{vmView}->ReconfigVM( spec => $vmConfigSpec );
		};
		if ( $@ )
		{
			if ( ref($@->detail) eq 'TaskInProgress' )
			{
				Common_Utils::WriteMessage("ReconfigVM:: waiting for 3 secs as another task is in progress.",2);
				sleep(2);
				next;
			}
			Common_Utils::WriteMessage( Dumper( $vmConfigSpec ) ,3);
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			return ( $Common_Utils::FAILURE );
		}
		last;
	}									
	
	return ( $Common_Utils::SUCCESS );	
}

#####RenameVM#####
##Descritption 		:: Renames Virtual Machine.
##Input 			:: Virtual machine view and New name.
##Output 			:: Returns SUCCESS else FAILURE.
#####RenameVM#####
sub RenameVM
{
	my %args		= @_;
	my $oldName 	= $args{vmView}->name;
	my $task		= "";
	my $ReturnCode	= $Common_Utils::SUCCESS;
	
	eval
	{
		$task 		= $args{vmView}->Rename_Task( newName => $args{newName} );
	};
	if ( $@ )
	{
		Common_Utils::WriteMessage("Failed to rename virtual machine \"$oldName\" to \"$args{newName}\".",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my $taskView 				= Vim::get_view(mo_ref => $task);
	my $vmView 					= "";
	while( 1 )
	{
		my $taskInfo			= $taskView->info;
		if ( $taskInfo->state->val eq "success" )
		{
			last;
		}
		if ( $taskInfo->state->val 	eq "error" )
		{
			my $soap_fault 		= SoapFault->new;
	        my $errorMesg 		= $soap_fault->fault_string($taskInfo->error->localizedMessage);
	        my $faultType 		= $taskInfo->error->fault;
	        my @errorMsgDetail	= $faultType->faultMessage;
	        Common_Utils::WriteMessage("ERROR :: $soap_fault.",3);
	        $ReturnCode		= $Common_Utils::FAILURE;
	        last;
		}
		sleep(1);
		$taskView->ViewBase::update_view_data();
	}
	return ( $ReturnCode );
}

#####ConfigureDisks#####
##Description	:: It either creates Disks/uses an existing disk.
##					1. In all cases it check whether the SCSI controller exists or not. IF does not exist, creates it.
##					2. If disk is to be re-used, it checks for its size as well.
##					3. If size has to be extended, it extends.
##					4. If size has to be decremented, it deleted exisitng disk and re-creates it.
##					5. It updates the virtual machine view from time-time.
##Input 		:: Configuration of machine to be created( XML::Element ) and Virtual Machine View to which disks are to be configured.
##Output 		:: Returns SUCCESS if everything is smooth else in any case it returns FAILURE.
#####ConfigureDisks#####
sub ConfigureDisks
{
	my %args 		= @_;
	my $machineName = $args{vmView}->name;
	my $createdDisks= $args{createdDisks};
	my $failed 		= $Common_Utils::SUCCESS;
	my @deviceChange= ();
	Common_Utils::WriteMessage("Configuring disks of machine \"$machineName\".",2);
	
	my $clusterNodes	= "";
	my $inmageHostId	= $args{sourceHost}->getAttribute('inmage_hostid');
	if($args{isCluster})
	{
		$clusterNodes	= $args{sourceHost}->getAttribute('clusternodes_inmageguids');
	}
		
	my @disksInfo	= ();
	eval
	{
		@disksInfo 	= $args{sourceHost}->getElementsByTagName('disk');
	};
	if ( $@ )
	{
		Common_Utils::WriteMessage("ERROR :: $@.",3);
		Common_Utils::WriteMessage("ERROR :: Failed to configure disks of machine \"$machineName\".",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my( $returnCode , $ideCount , $rdm , $cluster , $existingDisksInfo , $floppyDevices ) = FindDiskDetails( $args{vmView}, [] );
    if( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get disks information of machine \"$machineName\".",3);
		return ( $Common_Utils::FAILURE );
	}

	my %scsiControllerDefined 	= ();
	my $diskKey 				= -10;
	foreach my $disk ( @disksInfo )
	{
		my $diskExist			= $Common_Utils::NOTFOUND;
		my $vSanFileName		= ""; 
		my $existingDiskInfo 	= "";
		my $diskName			= $disk->getAttribute('disk_name');
		my @diskPath			= split( /\[|\]/ , $diskName );
		$diskPath[-1]			=~ s/^\s+//;
		if ( defined( $args{sourceHost}->getAttribute('vmDirectoryPath') ) && ( "" ne $args{sourceHost}->getAttribute('vmDirectoryPath') ) )
		{ 
			$diskPath[-1]	= $args{sourceHost}->getAttribute('vmDirectoryPath')."/$diskPath[-1]";
		}
		elsif ( ( defined( $args{sourceHost}->getAttribute('vsan_folder') ) ) && ( "" ne $args{sourceHost}->getAttribute('vsan_folder') ) )
		{
			@diskPath		= split( /\// , $diskName );
			$diskPath[-1]	= $args{sourceHost}->getAttribute('vsan_folder')."$diskPath[-1]";
		}
		
		$diskName				= "[".$disk->getAttribute('datastore_selected')."] $diskPath[-1]";
		
		my @tempArr	= @$existingDisksInfo;
		if ( -1 != $#tempArr )
		{
			my $tgtDiskUuid	= $disk->getAttribute('target_disk_uuid');
			foreach my $existingDisk ( @$existingDisksInfo )
			{
				if( (defined $tgtDiskUuid) && ( $tgtDiskUuid ne "" ) )
				{
					if( $tgtDiskUuid ne $existingDisk->{diskUuid})
					{
						next;
					}
				}
				elsif ( $diskName ne $existingDisk->{diskName} )
				{
					next;
				}
				$existingDiskInfo= $existingDisk;
				$diskExist		= $Common_Utils::EXIST; 
				last;
			}
		}
		
		my $diskUuid	= $disk->getAttribute('source_disk_uuid');
		if ( "physicalmachine" eq lc( $args{sourceHost}->getAttribute('machinetype') ) )
		{
			$diskUuid	= $disk->getAttribute('disk_signature');
		}
		
		if ( $diskExist	== $Common_Utils::EXIST )
		{
			Common_Utils::WriteMessage("Already exisitng Diskname = $diskName.",2);
			
			if( ( defined $$createdDisks{$diskUuid} ) )
			{
				my $clusterDisk	= $$createdDisks{$diskUuid};
				if ( ( defined $disk->getAttribute('protected') ) && ( "yes" eq lc ( $disk->getAttribute('protected') ) ) )
				{				
				}
			    elsif( $clusterNodes =~ /$$clusterDisk{inmageHostId}/i )
				{
					$disk->setAttribute( 'selected', "no" );
					Common_Utils::WriteMessage("Setting as selected for cluster disk = $diskName.",2);
				}
			}
			
			if(  "yes" eq $disk->getAttribute('cluster_disk') )
			{
				$$createdDisks{$diskUuid}	= { diskName => $diskName, inmageHostId => $inmageHostId };
			}
			
			
			if ( "Mapped Raw LUN" eq $existingDiskInfo->{diskMode} )
			{
				next;
			}
			if ( ( $disk->getAttribute('size') <= $existingDiskInfo->{diskSize} ) && ( $disk->getAttribute('scsi_mapping_vmx') eq $existingDiskInfo->{scsiVmx} ) )
			{
				next;
			}
		}
		my $operation		= "create";	
		my $sourceScsiId	= $disk->getAttribute( 'scsi_mapping_vmx' );
		my @splitController = split ( /:/ , $sourceScsiId );
		my $controllerkey	= ( $splitController[0] < 4 ) ? ( 1000 + $splitController[0] ) : ( 15000 + $splitController[0] - 4 );
		if ( exists $scsiControllerDefined{$splitController[0]}{Defined} )
		{
			$controllerkey 	= $scsiControllerDefined{$splitController[0]}{Defined};
		}
		elsif( exists $scsiControllerDefined{$splitController[0]}{Exists} )
		{
		}
		elsif ( $Common_Utils::FAILURE eq FindScsiController( $args{vmView} , $controllerkey ) ) 
		{
			my $sharing 		= $disk->getAttribute('controller_mode');
			my $ControllerType 	= $disk->getAttribute('controller_type');
			my $scsiContKey		= -101 + -$splitController[0];
			
			if( $splitController[0] < 4 )
			{
				$scsiContKey	= 1000 + $splitController[0];
				( $returnCode , my $scsiControllerSpec ) = NewSCSIControllerSpec( busNumber => $splitController[0] , sharing => $sharing , controllerType => $ControllerType , controllerKey => $scsiContKey );
				if ( $returnCode ne $Common_Utils::SUCCESS ) 
				{
					$failed 		=  $Common_Utils::FAILURE;
					last;
				}
				push @deviceChange , $scsiControllerSpec;				
			}
			else
			{
				$scsiContKey	= 15000 + $splitController[0] - 4;
				my $busNumber	= $splitController[0] - 4;
				( $returnCode , my $scsiControllerSpec ) = NewSATAControllerSpec( busNumber => $busNumber , controllerType => $ControllerType , controllerKey => $scsiContKey );
				if ( $returnCode ne $Common_Utils::SUCCESS ) 
				{
					$failed 		=  $Common_Utils::FAILURE;
					last;
				}
				push @deviceChange , $scsiControllerSpec;
			}
			
			$scsiControllerDefined{$splitController[0]}{Defined}	= $scsiContKey;
		}
		else
		{
			$scsiControllerDefined{$splitController[0]}{Exists}		= "yes";
		}
				
		my $rootFolderPath 		= "";
		if ( ( defined( $args{sourceHost}->getAttribute('vmDirectoryPath') ) )&& ( "" ne $args{sourceHost}->getAttribute('vmDirectoryPath') ) )
		{
			$rootFolderPath		= $args{sourceHost}->getAttribute('vmDirectoryPath');
			Common_Utils::WriteMessage("RootFolderPath = $rootFolderPath.",2);
		}
			
		$diskKey		= ( $splitController[0] < 4 ) ? ( 2000 + ($splitController[0] * 16) + $splitController[1] ) : ( 16000 + ( ($splitController[0]-4) * 30) + $splitController[1] );
		if ( defined ( $existingDiskInfo ) && ( "" ne $existingDiskInfo )  )
		{
			$operation 	= "edit";
			my @scsi	= split ( /:/ , $existingDiskInfo->{scsiVmx} );
			$diskKey 	= ( $scsi[0] < 4 ) ? ( 2000 + ( $scsi[0] * 16 ) + $scsi[1] ) : ( 16000 + ( ($splitController[0]-4) * 30 ) + $scsi[1] );
		}
		elsif( ( defined $$createdDisks{$diskUuid} ) )
		{
			my $clusterDisk	= $$createdDisks{$diskUuid};
		    if( $clusterNodes =~ /$$clusterDisk{inmageHostId}/i )
			{
				$operation 	= "add";
				$diskName	= $$clusterDisk{diskName};
				$disk->setAttribute( 'selected', "no" );
				Common_Utils::WriteMessage("Adding cluster disk, Setting as selected for cluster disk = $diskName.",2);
			}
		}
		
		my ( $returnCode , $virtualDeviceSpec )= VirtualDiskSpec( diskConfig => $disk , controllerKey => $controllerkey , unitNumber => $splitController[1] , 
																	diskKey => $diskKey , rootFolderName => $rootFolderPath , operation => $operation,
																	diskName => $diskName );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$failed 	= $Common_Utils::FAILURE;
			last;
		}
		
		if(  "yes" eq $disk->getAttribute('cluster_disk') )
		{
			$$createdDisks{$diskUuid}	= { diskName => $diskName, inmageHostId => $inmageHostId };
		}
		
		push @deviceChange , $virtualDeviceSpec;
	}		
	
	if ( $failed )
	{
		Common_Utils::WriteMessage("ERROR :: Either creation of virtual disk spec/creation of new controller spec has failed.",2);
		return ( $Common_Utils::FAILURE );
	}
	
	$returnCode		= ReconfigVm( vmView => $args{vmView} , changeConfig => {deviceChange => \@deviceChange });
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	return ( $Common_Utils::SUCCESS );
}

#####NewSCSIControllerSpec#####
##Description		:: Creates a New SCSI controller spec.
##Input 			:: SCSI controller number , SCSI controller sharing , Type of Bus.
##Output 			:: Returns SUCCESS and SCSI Controller Spec else FAILURE.
#####NewSCSIControllerSpec#####
sub NewSCSIControllerSpec
{
	my %args 			= @_; 
	
	Common_Utils::WriteMessage("Creating a new scsi controller for bus number $args{busNumber},controllerType = $args{controllerType} in $args{sharing} basis.",2);
	my $connectionInfo	= VirtualDeviceConnectInfo->new( allowGuestControl => "false" , connected => "true" ,startConnected => "true" );
	my $controller 		= $args{controllerType}->new( key => $args{controllerKey},busNumber => $args{busNumber},connectable => $connectionInfo,
                          								controllerKey => 100, sharedBus => VirtualSCSISharing->new($args{sharing}));
   
   	my $controllerspec = VirtualDeviceConfigSpec->new(	device => $controller,
                     												operation => VirtualDeviceConfigSpecOperation->new('add'));
                     												
	return ( $Common_Utils::SUCCESS , $controllerspec );                     											
}

#####NewSATAControllerSpec#####
##Description		:: Creates a New SATA controller spec.
##Input 			:: SATA controller number ,  Bus number.
##Output 			:: Returns SUCCESS and SATA Controller Spec else FAILURE.
#####NewSATAControllerSpec#####
sub NewSATAControllerSpec
{
	my %args 			= @_; 
	
	Common_Utils::WriteMessage("Creating a new sata controller for bus number $args{busNumber},controllerType = $args{controllerType} .",2);
	my $connectionInfo	= VirtualDeviceConnectInfo->new( allowGuestControl => "false" , connected => "true" ,startConnected => "true" );
	my $controller 		= $args{controllerType}->new( key => $args{controllerKey},busNumber => $args{busNumber},connectable => $connectionInfo,
                          								controllerKey => 100);
   
   	my $controllerspec = VirtualDeviceConfigSpec->new(	device => $controller,
                     												operation => VirtualDeviceConfigSpecOperation->new('add'));
                    												
	return ( $Common_Utils::SUCCESS , $controllerspec );                     											
}

#####VirtualDiskSpec#####
##Description 		:: Create/edit a new Virtual Disk spec. Disk can be either VMDK/RDM/RDMP.
##Input 			:: Disk Information (XML::ELEMENT of disk).
##Output 			:: Returns SUCCESS and New Disk Spec else FAILURE.
#####VirtualDiskSpec#####
sub VirtualDiskSpec
{
	my %args 				=  @_;
	
	my @diskPath			= split( /\[|\]/ , $args{diskConfig}->getAttribute('disk_name') );
	$diskPath[-1]			=~ s/^\s+//;
	if ( ( defined $args{rootFolderName} ) && ( "" ne $args{rootFolderName} ) )
	{
		Common_Utils::WriteMessage("root Folder Name = $args{rootFolderName}.",2);
		$diskPath[-1]		= "$args{rootFolderName}/$diskPath[-1]";
	}
	my $fileName			= "[".$args{diskConfig}->getAttribute('datastore_selected')."] $diskPath[-1]";
	my $fileSize 			= $args{diskConfig}->getAttribute('size');
	my $diskType 			= $args{diskConfig}->getAttribute('thin_disk');
	my $diskMode			= $args{diskConfig}->getAttribute('independent_persistent');
	
	if ( ( defined $args{diskName} ) && ( "" ne $args{diskName} ) )
	{
		$fileName		= $args{diskName};
	}
	
	Common_Utils::WriteMessage("Creating/Reconfiguring($args{operation}) disk $fileName of size $fileSize at SCSI unit number $args{unitNumber} of type $diskType in mode $diskMode.",2);
	my $diskBackingInfo		= VirtualDiskFlatVer2BackingInfo->new( diskMode => $diskMode , fileName => $fileName ,
																	thinProvisioned =>$diskType );
	if( ( "yes" eq $args{diskConfig}->getAttribute('cluster_disk')) || ( $args{diskConfig}->getAttribute('controller_mode') !~ /noSharing/i ) )
	{
		$diskBackingInfo	= VirtualDiskFlatVer2BackingInfo->new( diskMode => $diskMode , fileName => $fileName ,
																	eagerlyScrub => "true", thinProvisioned => "false" );
	}

	if ( "Mapped Raw LUN" eq $args{diskConfig}->getAttribute('disk_mode') && ( "false" eq lc( $args{diskConfig}->getAttribute('convert_rdm_to_vmdk') ) ) )
	{
		my $compatibilityMode	= $args{diskConfig}->getAttribute('disk_type');
		my $deviceName			= $args{diskConfig}->getAttribute('devicename');
		$fileSize 				= $args{diskConfig}->getAttribute('capacity_in_kb');
		Common_Utils::WriteMessage("RDM device name = $deviceName,New size = $fileSize, compatibility mode = $compatibilityMode.",2);
		$diskBackingInfo 	= VirtualDiskRawDiskMappingVer1BackingInfo->new( compatibilityMode => $compatibilityMode, diskMode =>$diskMode,
																			 deviceName =>$deviceName, fileName => $fileName );
	}
																	
	my $connectionInfo		= VirtualDeviceConnectInfo->new( allowGuestControl => "false" , connected => "true" ,startConnected => "true" );
	my $virtualDisk			= VirtualDisk->new( controllerKey => $args{controllerKey} , unitNumber => $args{unitNumber} ,
												key => $args{diskKey}, backing => $diskBackingInfo, capacityInKB => $fileSize );
	
	my $virtualDeviceSpec 	= VirtualDeviceConfigSpec->new(operation => VirtualDeviceConfigSpecOperation->new('add'),device => $virtualDisk,
																fileOperation => VirtualDeviceConfigSpecFileOperation->new('create'));
	if( ( "edit" eq $args{operation} ) || ( "add" eq $args{operation} ) )
	{
		$virtualDeviceSpec 	= VirtualDeviceConfigSpec->new(operation => VirtualDeviceConfigSpecOperation->new( $args{operation} ), device => $virtualDisk );
	}
	return ( $Common_Utils::SUCCESS , $virtualDeviceSpec );
}

#####ReuseExistingDiskSpec#####
##Description	:: It re-uses the exisitng Disk information and add them to Master Target.
##Input 		:: Master Target View, Virtual machine view whose disks are to be added to master target.
##Ouput 		:: Returns SUCCESS upon successfull adding disks to MT else FAILURE.
#####ReuseExisitngDiskSpec#####
sub ReuseExistingDiskSpec
{
	my %args 					= @_;
	my @virtualDiskSpecs		= ();
	my %disksInfoInMT			= ();
	my $returnCode				= FindOccupiedScsiIds( $args{mtView} , \%disksInfoInMT );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	my $vmxVersion 		= Common_Utils::GetViewProperty($args{mtView}, 'config.version');
	my $devices 		= Common_Utils::GetViewProperty( $args{vmView}, 'config.hardware.device');
	foreach my $device ( @$devices )
	{
		if ( $device->deviceInfo->label !~ /Hard Disk/i )
		{
			next;
		}
		my $fileName 			= $device->backing->fileName;
		if ( exists $disksInfoInMT{$fileName} )
		{
			Common_Utils::WriteMessage("$fileName already added to master Target.",2);
			next;
		}
		
		my $fileSize  			= $device->capacityInKB;
		my $diskMode			= $device->backing->diskMode;
		my $diskType 			= "";
		my $compatibilityMode	= "";
		my $lunUuid				= "";
		if ( exists $device->{backing}->{compatibilityMode} )
		{
			$compatibilityMode	= $device->{backing}->{compatibilityMode};
			$lunUuid			= $device->backing->lunUuid;		
		}
		else
		{
			 $diskType 			= $device->backing->thinProvisioned;
		}			
		my $controllerKey		= "";
		my $unitNumber			= "";
		my $ScsiId				= "";
		my $key					= "";
		
		for ( my $i = 0 ; $i <= 3 ; $i++ )
		{
			for ( my $j = 0 ; $j <= 15 ; $j++ )
			{
				if ( 7 == $j )
				{
					next;
				}
				my $unitNumCheck= 2000 + ( $i * 16 ) + $j;
				if ( exists $disksInfoInMT{$unitNumCheck} )
				{
					next;
				}
				$unitNumber		= ( $unitNumCheck - 2000 ) % 16;
				$controllerKey	= 1000 + $i;
				$ScsiId			= "$i:$j";
				$key			= $unitNumCheck;
				last;			
			}	
			if ( ( "" ne $unitNumber )&& ( "" ne $controllerKey ) )
			{
				last;
			}
		}
		
		if( ( ( "" eq $unitNumber ) || ( "" eq $controllerKey ) ) && ( lc($vmxVersion) ge 'vmx-10') && 
			( Common_Utils::GetViewProperty( $args{mtView},'summary.config.guestFullName') =~ /Windows/i ) )
		{
			for ( my $i = 0 ; $i <= 3 ; $i++ )
			{
				for ( my $j = 0 ; $j <= 29 ; $j++ )
				{
					my $unitNumCheck= 16000 + ( $i * 30 ) + $j;
					if ( exists $disksInfoInMT{$unitNumCheck} )
					{
						next;
					}
					$unitNumber		= ( $unitNumCheck - 16000 ) % 30;
					$controllerKey	= 15000 + $i;
					$ScsiId			= "$i:$j";
					$key			= $unitNumCheck;
					last;			
				}	
				if ( ( "" ne $unitNumber )&& ( "" ne $controllerKey ) )
				{
					last;
				}
			}			
		}
				
		Common_Utils::WriteMessage("Attaching disk $fileName of size $fileSize at SCSI unit number $unitNumber of type $diskType in mode $diskMode.",2);
		my $diskBackingInfo		= VirtualDiskFlatVer2BackingInfo->new( diskMode => $diskMode , fileName => $fileName ,
																	thinProvisioned =>$diskType );
		
		if ( defined $compatibilityMode && "" ne $compatibilityMode )
		{
			$diskBackingInfo 	= VirtualDiskRawDiskMappingVer1BackingInfo->new( compatibilityMode => $compatibilityMode, diskMode =>$diskMode,
																			 deviceName =>$lunUuid, fileName => $fileName );
			Common_Utils::WriteMessage("DeviceName = $lunUuid , CompatibilityMode => $compatibilityMode.",2);																			 
		}
																	
		my $connectionInfo		= VirtualDeviceConnectInfo->new( allowGuestControl => "false" , connected => "true" ,startConnected => "true" );
		my $virtualDisk			= VirtualDisk->new( controllerKey => $controllerKey , unitNumber => $unitNumber ,key => -1, 
													backing => $diskBackingInfo, capacityInKB => $fileSize );
													
		my $virtualDeviceSpec 	= VirtualDeviceConfigSpec->new(operation => VirtualDeviceConfigSpecOperation->new('add'),device => $virtualDisk );
		push @virtualDiskSpecs	, $virtualDeviceSpec;
		
		$disksInfoInMT{$fileName}	= $key;
		$disksInfoInMT{$key}		= $fileName; 
		${$args{CreatedVmInfo}}{$fileName}	= $ScsiId;		
		Common_Utils::WriteMessage("Successfully created re-use existing disk spec for $fileName.\n",2);
	}
	
	if ( -1 != $#virtualDiskSpecs )
	{
		$returnCode		= ReconfigVm( vmView => $args{mtView} , changeConfig => { deviceChange => \@virtualDiskSpecs } );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
	}
	
	return ( $Common_Utils::SUCCESS );
}

#####UpdateVmView#####
##Description		:: Updates virtual machine views/view.
##Input 			:: View/Views.
##Output 			:: Returns SUCCESS else FAILURE.
#####UpdateVmView#####
sub UpdateVmView
{
	my $updateView	= shift;
	
	eval
	{
		$updateView->ViewBase::update_view_data( GetVmProps() );
	};
	if ( $@ )
	{
		Common_Utils::WriteMessage("ERROR :: Failed while updating view and error is ",3);
		Common_Utils::WriteMessage("ERROR :: $@.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	return ( $Common_Utils::SUCCESS , $updateView );
}

#####GetVmViewByNameInDatacenter#####
##Description	:: Gets virtual machine view under a specified datacenter with specified name.
##Input 		:: virtual machine name and datacenter View.
##Output 		:: Returns virtual machine view, success message if vm resides else FAILURE.
#####GetVmViewByNameInDatacenter#####
sub GetVmViewByNameInDatacenter
{
	my %args 		= @_;
	my $dcName		= $args{datacenterView}->name;
	
	my $vmViews		= Vim::find_entity_views( view_type => "VirtualMachine" , begin_entity => $args{datacenterView} , 
												filter => { 'name' => "$args{machineName}" }, properties => GetVmProps() );
	
	unless ( @$vmViews )
	{
		Common_Utils::WriteMessage("Unable to get view of virtual machine \"$args{machineName}\" under datacenter \"$dcName\".",2);
		return ( $Common_Utils::NOTFOUND );
	}
	
	my @tempArr	= @$vmViews;
	if ( 1 <= $#tempArr )
	{
		Common_Utils::WriteMessage("Got multiple views of virtual machines with name as \"$args{machineName}\" under datacenter \"$dcName\".",3);
		return ( $Common_Utils::FAILURE );
	}
	return ( $Common_Utils::SUCCESS , $$vmViews[0] );
}

#####GetVmViewByVmPathNameInDatacenter#####
##Description	:: Gets virtual machine view under a specified datacenter with specified vmPathName.
##Input 		:: vmPathName and datacenterView.\
##Output 		:: Returns virtual machine view, success message if vm resides under datacenter else FAILURE.
#####GetVmViewByVmPathNameInDatacenter#####
sub GetVmViewByVmPathNameInDatacenter
{
	my %args 		= @_;
	my $dcName 		= $args{datacenterView}->name;
	
	my $vmViews		= Vim::find_entity_views( view_type => "VirtualMachine" , begin_entity => $args{datacenterView} , 
											filter => { 'summary.config.vmPathName' => "$args{vmPathName}" }, properties => GetVmProps() );
	
	unless ( @$vmViews )
	{
		Common_Utils::WriteMessage("Unable to get view of virtual machine with vmPathName as \"$args{vmPathName}\" under datacenter \"$dcName\".",2);
		return ( $Common_Utils::NOTFOUND );
	}
	
	my @tempArr	= @$vmViews;
	if ( 1 <= $#tempArr )
	{
		Common_Utils::WriteMessage("Got multiple views of virtual machine with vmPathName as \"$args{vmPathName}\" under datacenter \"$dcName\".",3);
		return ( $Common_Utils::FAILURE );
	}
	return ( $Common_Utils::SUCCESS , $$vmViews[0] );
}

#####UpdateVm#####
##Description 		:: Updates virtual machines which is already exisitng. Things which needs a check are
##						1. Display Name of Virtual Machine.
##						2. RAM and CPU configuration.
##						3. Number of IDE devices, Floppy Devices.
##						4. Virtual Network Cards.
##Input 			:: Virtual Machine UUID.
##Output 			:: Returns SUCCESS on Successful update else FAILURE.
#####UpdateVm#####
sub UpdateVm
{
	my %args 			= @_;
	
	my $displayName 	= $args{sourceHost}->getAttribute('display_name');
	if ( ( defined ( $args{sourceHost}->getAttribute('new_displayname') ) ) && ( "" ne $args{sourceHost}->getAttribute('new_displayname') ) )
	{
		$displayName 	= $args{sourceHost}->getAttribute('new_displayname');
	}
	
	my $vmConfigSpec 	= VirtualMachineConfigSpec->new();
	
	if ( $displayName ne $args{vmView}->name )
	{
		$vmConfigSpec->{name}	= $displayName;
	}
	
	my $fileInfo 					= VirtualMachineFileInfo->new();
	$fileInfo->{vmPathName}			= Common_Utils::GetViewProperty( $args{vmView}, 'summary.config.vmPathName');
	my $rindex						= rindex( Common_Utils::GetViewProperty( $args{vmView}, 'summary.config.vmPathName') , "/" );
	my $CreatedDsPath				= substr( Common_Utils::GetViewProperty( $args{vmView}, 'summary.config.vmPathName') , 0 , $rindex + 1 );
	$fileInfo->{logDirectory}		= $CreatedDsPath;
	$fileInfo->{snapshotDirectory}	= $CreatedDsPath;
	$fileInfo->{suspendDirectory}	= $CreatedDsPath;
	$vmConfigSpec->{files} 			= $fileInfo;			
	Common_Utils::WriteMessage("Setting log directory path to $CreatedDsPath.",2);
	
	if( Common_Utils::GetViewProperty( $args{vmView},'summary.config.memorySizeMB') != $args{sourceHost}->getAttribute('memsize') && $args{sourceHost}->getAttribute('memsize') ne "" )
	{
		$vmConfigSpec->{memoryMB} 	= $args{sourceHost}->getAttribute('memsize');
	}
						
	if ( Common_Utils::GetViewProperty( $args{vmView},'summary.config.numCpu') != $args{sourceHost}->getAttribute('cpucount') && $args{sourceHost}->getAttribute('cpucount') ne "" )
	{
		$vmConfigSpec->{numCPUs} 	= $args{sourceHost}->getAttribute('cpucount');
		
		my $serviceContent	= Vim::get_service_content();
		my $apiVersion		= $serviceContent->about->apiVersion;
		if( $apiVersion ge "5.0")
		{
			$vmConfigSpec->{numCoresPerSocket}	= 1;
		}
	}
	
	my( $returnCode , $cdRomDeviceSpec )	= CreateCdRomDeviceSpec( $args{sourceHost}->getAttribute('ide_count') );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to create CD-ROM device configuration for machine \"$displayName\".",3);
		return $Common_Utils::FAILURE;
	}
	my @cdRomDevSpecs						= @$cdRomDeviceSpec;
	
	( $returnCode , my $floppyDeviceSpec )	= CreateFloppyDeviceSpec( $args{sourceHost} );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to create floppy device configuration for machine \"$displayName\".",3);
		return $Common_Utils::FAILURE;
	}
	my @floppyDeviceSpec					= @$floppyDeviceSpec;
	
	( $returnCode , my $networkDeviceSpec )	= CreateNetworkAdapterSpec( sourceNode => $args{sourceHost} , isItvCenter => $args{isItvCenter} 
														,displayName => $displayName , portgroupInfo => $args{dvPortGroupInfo} );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to create network adapter configuration for machine \"$displayName\".",3);
		return ( $Common_Utils::FAILURE );
	}
	my @networkDeviceSpecs 					= @$networkDeviceSpec; 
	
	
	$vmConfigSpec->{deviceChange}			= [ @cdRomDevSpecs , @floppyDeviceSpec , @networkDeviceSpecs ];
	
	if ( ( defined $args{sourceHost}->getAttribute('diskenableuuid') ) && ( $args{sourceHost}->getAttribute('diskenableuuid') ne "na" ) )
	{
		my $diskEnableUuid	= ( 'yes' eq $args{sourceHost}->getAttribute('diskenableuuid') ) ? "true" : "false";
		Common_Utils::WriteMessage("Setting disk.enableuuid value as $diskEnableUuid to $displayName.",2);
		my @optionValues 	= ();		
		my $opt_conf = OptionValue->new( key => "disk.enableuuid", value => $diskEnableUuid );
		push( @optionValues, $opt_conf);
		$vmConfigSpec->{extraConfig}	= \@optionValues;
	}
	
	for ( my $i = 0 ;; $i++  )
	{
		eval 
		{			
			$args{vmView}->ReconfigVM( spec => $vmConfigSpec );
		};
		if ( $@ )
		{
			if ( ref($@->detail) eq 'TaskInProgress' )
			{
				Common_Utils::WriteMessage("ReconfigVM:: waiting for 3 secs as another task is in progress.",2);
				sleep(2);
				next;
			}
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			return ( $Common_Utils::FAILURE );
		}
		last;
	}	
	
	return ( $Common_Utils::SUCCESS );
}

#####ByNetworkLabel#####
##Description	:: Sorts network nodes on basis of Network Label which includes adapter number.
##Input 		:: 'sort' function itself will implicitly calls this function passing a couple of Nodes.
##					1. return 1, to say that 'a' value has to be considered.
##					2. return -1, to say that 'b' value has to be considered.
##					3. return 0 , to say use either.
##Output 		:: Returns 1,-1,0 depending which needs to be sorted first.
#####ByNetworkLabel#####
sub ByNetworkLabel
{
	my $a_networkLabel = $a->getAttribute('network_label');
	my $b_networkLabel = $b->getAttribute('network_label');
	
	if ( $a_networkLabel gt $b_networkLabel )
	{
		return 1;
	}
	elsif ( $a_networkLabel lt $b_networkLabel )
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

#####CreateCdRomDeviceSpec#####
##Description 	:: Creates CD-ROM device configuration.
##Input 		:: Number of IDE controllers to be added.
##Output		:: Returns SUCCESS else FAILURE.
#####CreateCdRomDeviceSpec#####
sub CreateCdRomDeviceSpec
{
	#cd drive can be ide/sata
	my $ideCount 		= shift;
	my @cdRomDevSpecs	= ();
	
	my @keyInfo			= ([201,0],[200,0],[200,1],[201,1]);
	
	for ( my $i = 0 ; $i < $ideCount ; $i++  )
	{
		my $ControllerKey	= $keyInfo[$i][0];
		my $unitNumber 		= $keyInfo[$i][1];
		
		my $CdromBacking	= VirtualCdromRemotePassthroughBackingInfo->new( deviceName => "" , exclusive => "false" );
		
		my $connectionInfo	= VirtualDeviceConnectInfo->new( allowGuestControl => "true" , connected => "false" , startConnected =>"false" );
		
		my $virtualCDRom 	= VirtualCdrom->new( backing => $CdromBacking , connectable => $connectionInfo,
														controllerKey => $ControllerKey, key => -44, unitNumber => $unitNumber ); 
											
		my $cdRomDevice 	= VirtualDeviceConfigSpec->new( device => $virtualCDRom , operation => VirtualDeviceConfigSpecOperation->new('add') );
		
		push @cdRomDevSpecs, $cdRomDevice;
	}										
	
	return ( $Common_Utils::SUCCESS , \@cdRomDevSpecs );
}

#####CreateFloppyDeviceSpec#####
##Description 	:: Creates floppy device specification.
##Input 		:: Source Node.
##Output 		:: Returns SUCCESS or FAILURE.
#####CreateFloppyDeviceSpec#####
sub CreateFloppyDeviceSpec
{
	my $sourceNode		= shift;
	my @floppyDeviceSpec= ();
	my $displayName 	= $sourceNode->getAttribute('display_name');
	
	my $floppyDevCount	= $sourceNode->getAttribute('floppy_device_count');
	for ( my $i = 0 ; $i < $floppyDevCount ; $i++ )
	{
		my $ControllerKey		= 400;
		my $unitNumber			= $i;
		
		my $floppyDriveBacking 	= VirtualFloppyRemoteDeviceBackingInfo->new( deviceName => "" , useAutoDetect => "false" );
	
		my $connectionInfo		= VirtualDeviceConnectInfo->new( allowGuestControl => "true" , connected => "false" , startConnected =>"false");
	
		my $floppyDrive 		= VirtualFloppy->new( backing => $floppyDriveBacking , connectable => $connectionInfo , controllerKey => $ControllerKey, key => -66 , unitNumber => $unitNumber );
	
		my $floppyDevice 		= VirtualDeviceConfigSpec->new( device => $floppyDrive , operation => VirtualDeviceConfigSpecOperation->new('add'));
		
		push @floppyDeviceSpec	, $floppyDevice; 
	}
	return ( $Common_Utils::SUCCESS , \@floppyDeviceSpec );
}

#####CreateNetworkAdapterSpec#####
##Description 	:: Created Network adapter configuration.
##Input 		:: Source host details i.e xml node element which contains all details of source host node.
##Output 		:: Returns SUCCESS else FAILURE.
#####CreateNetworkAdapterSpec#####
sub CreateNetworkAdapterSpec
{
	my %args				= @_;
	
	my @networkCardsSpec	= ();
	my @networkCards		= $args{sourceNode}->getElementsByTagName('nic');
	my $nwAdapterCounter	= 1;
	
	foreach my $networkAdapter ( sort ByNetworkLabel @networkCards )
	{
		my $networkName 	= $networkAdapter->getAttribute('network_name');
		my $controllerKey	= 100; 
		my $addressType 	= $networkAdapter->getAttribute('address_type');
		my $macAddress 		= $networkAdapter->getAttribute('nic_mac');
		my $adapterType		= $networkAdapter->getAttribute('adapter_type');
		
		if ( ( defined ( $networkAdapter->getAttribute('new_network_name') ) ) && ( "" ne $networkAdapter->getAttribute('new_network_name') ) )
		{
			$networkName	= $networkAdapter->getAttribute('new_network_name');
		}
		
		my ( $switchUuid , $portgroupKey )	= ( "" , "" );
		if ( $networkName =~ /\[DVPG\]$/i )
		{
			foreach my $portgroup ( @{$args{portgroupInfo}} )
			{
				if ( ( "no" eq lc ( $args{isItvCenter} ) ) && ( "ephemeral" ne $$portgroup{portgroupType} ) )
				{
					next;
				}
				if ( $$portgroup{portgroupNameDVPG} ne $networkName )
				{
					next;
				}
				my $pgName		= $$portgroup{portgroupName};
				$switchUuid		= $$portgroup{switchUuid};
				$portgroupKey	= $$portgroup{portgroupKey};
				Common_Utils::WriteMessage("$args{displayName},$pgName : switch UUID = $switchUuid , port key = $portgroupKey.",2);
				last;
			}
		}
		
		my $virtualEthernetCard	= "";
		if ( ( "" eq $switchUuid ) || ( "" eq $portgroupKey ) )
		{
			$virtualEthernetCard= VirtualEthernetCardNetworkBackingInfo->new( deviceName => $networkName , useAutoDetect => "true" );
		}
		else
		{
			my $DVPG			= DistributedVirtualSwitchPortConnection->new( portgroupKey => $portgroupKey , switchUuid => $switchUuid );
			$virtualEthernetCard= VirtualEthernetCardDistributedVirtualPortBackingInfo->new( port=> $DVPG );
		}
		
		my $connectionInfo		= VirtualDeviceConnectInfo->new( allowGuestControl => "true" , connected => "false" , startConnected =>"true");
		
		my $NicCard 			= $adapterType->new( backing => $virtualEthernetCard , connectable => $connectionInfo , controllerKey => $controllerKey , key => -55 , unitNumber => 1 ,
										addressType =>'generated' , macAddress=> "" , wakeOnLanEnabled => "true" );
		
		my $EthernetDevice 		= VirtualDeviceConfigSpec->new( device => $NicCard , operation => VirtualDeviceConfigSpecOperation->new('add'));										
		
		push @networkCardsSpec , $EthernetDevice;
		$networkAdapter->setAttribute( 'network_label', "Network adapter $nwAdapterCounter" );
		$nwAdapterCounter++;
	}
		
	return ( $Common_Utils::SUCCESS , \@networkCardsSpec );		
}

#####RemoveDevices#####
##Description	:: It removes IDE, floppy and Network adapters from a virtual machine.
##Input 		:: Virtual machine view.
##Ouput 		:: Returns SUCCESS else FAILURE.
#####RemoveDevices#####
sub RemoveDevices
{
	my %args 		= @_;
	
	if ( ! defined ( Common_Utils::GetViewProperty( $args{vmView},'config.hardware.device') ) )
	{
		return ( $Common_Utils::SUCCESS );
	}
	my @deviceChanges	= ();
	my $devices 		= Common_Utils::GetViewProperty( $args{vmView},'config.hardware.device');
	foreach my $device ( @$devices )
	{
		if( $device->deviceInfo->label =~ /^Network adapter/i )
		{
			my $virtualEthernetCard = VirtualEthernetCardNetworkBackingInfo->new( deviceName => "" , useAutoDetect => "true" );
			my $NicCard 			= ref($device)->new( backing => $virtualEthernetCard , controllerKey => $device-> controllerKey , key => $device->key , 
														unitNumber => $device->unitNumber );
	
			my $EthernetDevice 		= VirtualDeviceConfigSpec->new( device => $NicCard , operation => VirtualDeviceConfigSpecOperation->new('remove') );
			push @deviceChanges , $EthernetDevice;
		}
		elsif( "VirtualCdrom" eq ref( $device ) )
		{
			my $CdromBacking	= VirtualCdromRemotePassthroughBackingInfo->new( deviceName => "" , exclusive => "false" );
		
			my $virtualCDRom 	= VirtualCdrom->new( backing => $CdromBacking , controllerKey => $device->controllerKey, key => $device->key , unitNumber => $device->unitNumber ); 
												
			my $cdRomDevice 	= VirtualDeviceConfigSpec->new( device => $virtualCDRom , operation => VirtualDeviceConfigSpecOperation->new('remove') );
			push @deviceChanges	, $cdRomDevice;
		}
		elsif( "VirtualFloppy" eq ref( $device ) )
		{
			my $floppyDriveBacking 	= VirtualFloppyRemoteDeviceBackingInfo->new( deviceName => "" , useAutoDetect => "false" );
	
			my $floppyDrive 		= VirtualFloppy->new( backing => $floppyDriveBacking , controllerKey => $device->controllerKey, key => $device->key , unitNumber => $device->unitNumber );
		
			my $floppyDevice 		= VirtualDeviceConfigSpec->new( device => $floppyDrive , operation => VirtualDeviceConfigSpecOperation->new('remove'));
			push @deviceChanges	, $floppyDevice;
		}
	}
	
	if ( -1 != $#deviceChanges )
	{
		my $returnCode 	= ReconfigVm( vmView => $args{vmView} , changeConfig => { deviceChange => \@deviceChanges } );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );	
		}
	}
	return ( $Common_Utils::SUCCESS );
}

#####GetVmScsiControllerName#####
##Description 		:: to get the VM ScsiControllerName 
##Input 			:: Virtual Machine view
##Output 			:: Return list of virtual machine ScsiControllerName
#####GetVmScsiControllerName#####
sub GetVmScsiControllerName
{
	my $vmView 	= shift;
	my $scsiControllerName = "";
	my $devices 		= Common_Utils::GetViewProperty( $vmView,'config.hardware.device');
	foreach my $device_temp ( @$devices )
	{
		if($device_temp->deviceInfo->label =~ /SCSI controller/i)
		{
			$scsiControllerName = ref ( $device_temp );
			last;
		}
	}	
	return $scsiControllerName;
}

#####MapScsiInfo#####
##Description 		:: It maps SCSI information at ESX level to Guest OS level information.
##Input 			:: Source Host Node and it's corresponding disk information from CX.
##Output			:: Returns SUCCESS on Complete Mapping Else Failure.
#####MapScsiInfo#####
sub MapScsiInfo
{
	my %args 		= @_;
	my $ReturnCode 	= $Common_Utils::SUCCESS;
	
	my %mapofPortNumbers	= ();
	my %mapPortNumHstToESX	= ();
	my %mapSataScsiIds		= ();
	my @disksInfo			= $args{host}->getElementsByTagName('disk');
	my $diskList			= ${$args{diskList}}{DiskList};
	foreach my $diskInfo ( @disksInfo )
	{
		my @portNumbers	 	= ();
		my $diskName 		= $diskInfo->getAttribute('disk_name');
		my $scsiNumAtEsx 	= $diskInfo->getAttribute('scsi_mapping_vmx');
		my $diskSizeinBytes	= $diskInfo->getAttribute('size') * 1024;
		my ( $portNumEsx , $unitNumEsx ) = split( /:/ , $scsiNumAtEsx );
		
		if( $args{host}->getAttribute('alt_guest_name') =~ /solaris10/i )
		{
			$diskSizeinBytes = $diskSizeinBytes - 512;
		}
		
		if ( ( exists $mapofPortNumbers{$portNumEsx} ) && ( $portNumEsx < 4 ) )
		{
			next;
		}
		
		my $found	= 0;
		foreach  my $disk ( sort keys %$diskList )
		{
			my $diskUuid	= $diskInfo->getAttribute('source_disk_uuid');
			if( $diskUuid eq "")
			{
				next;
			}
			$diskUuid =~ s/-//g;
			
			my $scsiPortOrScsiBusNum	= ( "windows" eq lc( $args{host}->getAttribute('os_info') ) ) ? ${$diskList}{$disk}{ScsiPort} : ${$diskList}{$disk}{ScsiBusNumber};
			if ( ( ${$diskList}{$disk}{ScsiId} =~ /$diskUuid/i ) && ( ! defined $mapPortNumHstToESX{$scsiPortOrScsiBusNum} ) )
			{
				$found	= 1;
				Common_Utils::WriteMessage("Found scsi id based on uuid.",2);
				push @portNumbers , $scsiPortOrScsiBusNum;
				$mapPortNumHstToESX{$scsiPortOrScsiBusNum}	= undef;
			}
		}
		
		unless($found)
		{
			foreach  my $disk ( sort keys %$diskList )
			{
				if( ${$diskList}{$disk}{DiskName} =~ /\/dev\/mapper\//i)
				{
					Common_Utils::WriteMessage("Skipping multipath device ${$diskList}{$disk}{DiskName}.",2);
					next;
				}			
				if( $portNumEsx < 4 )
				{
					Common_Utils::WriteMessage("Disk Size = $diskSizeinBytes , ${$diskList}{$disk}{Size}.",2);
					my $scsiPortOrScsiBusNum	= ( "windows" eq lc( $args{host}->getAttribute('os_info') ) ) ? ${$diskList}{$disk}{ScsiPort} : ${$diskList}{$disk}{ScsiBusNumber};
					if ( ( ${$diskList}{$disk}{ScsiTargetId} == $unitNumEsx ) && ( ${$diskList}{$disk}{Size} == $diskSizeinBytes ) && ( ! defined $mapPortNumHstToESX{$scsiPortOrScsiBusNum} ) )
					{
						push @portNumbers , $scsiPortOrScsiBusNum;
						$mapPortNumHstToESX{$scsiPortOrScsiBusNum}	= undef;
					}
				}
				else
				{
					Common_Utils::WriteMessage("Disk Size = $diskSizeinBytes , ${$diskList}{$disk}{Size}.",2);
					if( "windows" eq lc( $args{host}->getAttribute('os_info') ) )
					{
						my $scsiPortOrScsiBusNum	= ${$diskList}{$disk}{ScsiPort};
						if ( ( ${$diskList}{$disk}{ScsiBusNumber} == $unitNumEsx ) && ( ${$diskList}{$disk}{Size} == $diskSizeinBytes ) && ( ! defined $mapPortNumHstToESX{$scsiPortOrScsiBusNum} ) )
						{
							push @portNumbers , $scsiPortOrScsiBusNum;
							$mapPortNumHstToESX{$scsiPortOrScsiBusNum}	= undef;
						}					
					}
					else
					{
						my $scsiPortOrScsiBusNum	= (($portNumEsx - 4) * 30) + $unitNumEsx + 1;
						if ( ( ${$diskList}{$disk}{ScsiBusNumber} == $scsiPortOrScsiBusNum ) && ( ${$diskList}{$disk}{Size} == $diskSizeinBytes ) && ( ! defined $mapPortNumHstToESX{$scsiPortOrScsiBusNum} ) )
						{
							push @portNumbers , $scsiPortOrScsiBusNum;
							$mapPortNumHstToESX{$scsiPortOrScsiBusNum}	= undef;
						}
					}				
				}			
			}
		}
		
		if ( -1 == $#portNumbers )
		{
			Common_Utils::WriteMessage("ERROR :: Expecting a single match for disk \"$diskName\".",2);
			$mapofPortNumbers{$diskName}	= \@portNumbers;
		}
		elsif ( 0 == $#portNumbers )
		{
			Common_Utils::WriteMessage("Found unique information through disk \"$diskName\". Mapping to controller number at \"$portNumbers[0]\" at Guest OS level",2);
			$mapofPortNumbers{$portNumEsx} 	= $portNumbers[0];
			$mapPortNumHstToESX{$portNumbers[0]} = $portNumEsx;
			$mapSataScsiIds{$portNumbers[0]}= $unitNumEsx;
		}
		else
		{
			Common_Utils::WriteMessage("Couldn't map \"$diskName\" to a unique SCSI controller at Guest OS level.",2);
			$mapofPortNumbers{$diskName}	= \@portNumbers;		
		}
	}
	
	foreach my $diskInfo ( @disksInfo )
	{
		my $diskName 		= $diskInfo->getAttribute('disk_name');
		my $scsiNumAtEsx 	= $diskInfo->getAttribute('scsi_mapping_vmx');
		my ($portNumEsx , $unitNumEsx  ) = split( /:/ , $scsiNumAtEsx );
		if ( ( $portNumEsx < 4) && ( ! exists $mapofPortNumbers{$portNumEsx} ) && ( exists $mapofPortNumbers{$diskName} ) )
		{
			Common_Utils::WriteMessage("Mapping SCSI controller to ambiguous disk $diskName.",2);
			my @portNumbers	= @{$mapofPortNumbers{$diskName}};
			my @ambigPorts	= ();
			foreach my $portNumAtHost ( @portNumbers )
			{
				if ( defined $mapPortNumHstToESX{$portNumAtHost} )
				{
					next;
				}
				push @ambigPorts , $portNumAtHost; 
			}
			
			my $tempPortNumAtHost  = $portNumEsx + $args{host}->getAttribute('ide_count');
			if( ( $args{host}->getAttribute('hostversion') ge "4.1.0" ) && ( "windows" eq lc( $args{host}->getAttribute('os_info') ) ) )
			{
				$tempPortNumAtHost = $portNumEsx + 2;
			}
			
			if ( ( 0 < $#ambigPorts ) && ( defined $mapPortNumHstToESX{$tempPortNumAtHost} ) ) 
			{
				Common_Utils::WriteMessage("Still left with ambiguity even after excluding mapped port numbers.",2);
				my $leastDist 	= abs($tempPortNumAtHost - $ambigPorts[0]);
				my $portNumLT	= $ambigPorts[0];
				foreach my $portNum ( @ambigPorts )
				{
					if ( ( abs( $leastDist ) > abs( $tempPortNumAtHost - $portNum ) ) && ( $portNumLT > $portNum  ) )
					{
						$portNumLT	=  $portNum;
					}
				}
				$mapofPortNumbers{$portNumEsx}	= $portNumLT;
				$mapPortNumHstToESX{$portNumLT} = $portNumEsx;
			}
			elsif( 0 == $#ambigPorts )
			{
				$mapofPortNumbers{$portNumEsx}	= $ambigPorts[0];
				$mapPortNumHstToESX{$ambigPorts[0]} = $portNumEsx;
				Common_Utils::WriteMessage("Found unique information through disk \"$diskName\". Mapping to controller number at \"$ambigPorts[0]\" at Guest OS level",2);
			}
			else
			{
				$mapofPortNumbers{$portNumEsx}	= $tempPortNumAtHost;
				$mapPortNumHstToESX{$tempPortNumAtHost} = $portNumEsx;
			}
		}
		
		my $scsiMappedInfo	= "$mapofPortNumbers{$portNumEsx}:$unitNumEsx";
		
		if ( ( $portNumEsx >= 4) && ( exists $mapofPortNumbers{$portNumEsx} ) )
		{
			Common_Utils::WriteMessage("Mapping SATA controller ID to disk $diskName.",2);
			my $tempUnit	= $mapSataScsiIds{$mapofPortNumbers{$portNumEsx}};
			$scsiMappedInfo	= ( $mapofPortNumbers{$portNumEsx} - $tempUnit + $unitNumEsx ) . ":0";
		}
		elsif ( ( $portNumEsx >= 4) && ( ! exists $mapofPortNumbers{$portNumEsx} ) )
		{
			for( my $i=4; $i<=7; $i++ )
			{
				if( $i == $portNumEsx )
				{
					next;
				}
				if(exists $mapofPortNumbers{$i} )
				{
					my $portNumDiff	= $i - $portNumEsx;
					my $tempUnit	= $mapSataScsiIds{$mapofPortNumbers{$i}};
					my $tempPort	= $mapofPortNumbers{$i} - $tempUnit + $unitNumEsx + ($portNumDiff * 30);
					$scsiMappedInfo	= $tempPort . ":0";
					$mapofPortNumbers{$portNumEsx} 	= $tempPort;
					$mapPortNumHstToESX{$tempPort} 	= $portNumEsx;
					$mapSataScsiIds{$tempPort}		= $unitNumEsx;
					last;
				}				
			}
			if( ! exists $mapofPortNumbers{$portNumEsx} )
			{
				$scsiMappedInfo	= ( ( ($portNumEsx - 4) * 30 ) + 2 + $unitNumEsx ) . ":0";				
			}
		}
				
		$diskInfo->setAttribute( 'scsi_mapping_host' , $scsiMappedInfo );
		Common_Utils::WriteMessage("scsi_mapping_host for \"$diskName\" : $scsiMappedInfo.",2);
	}
	
	return ( $ReturnCode );
}

#####AddSCSIControllersToVM#####
##Description	:: Adds all controller's to a virtual machine.
##Input 		:: virtual machine view for which controllers are to be added.
##Output 		:: Returns SUCCESS else FAILURE.
#####AddSCSIControllersToVM#####
sub AddSCSIControllersToVM( % )
{
	my %args			= @_;
	my $ReturnCode		= $Common_Utils::SUCCESS;
	my %ControllerInfo	= ();
	my %controllerType	= ();	
	my @deviceChange 	= ();
	
	my $vmxVersion 		= Common_Utils::GetViewProperty($args{vmView}, 'config.version');
	my $numEthernetCard	= Common_Utils::GetViewProperty($args{vmView}, 'summary.config.numEthernetCards');
	
	my $ControllerCount	= 4;#Max nunmber of SCSI controllers allowed till now are 4 per vm.But this value differs if vmxVersion < 7.
	if ( lc($vmxVersion) lt "vmx-07" )
	{
		$ControllerCount= 5 - $numEthernetCard; 
	}	
	if( ( lc($vmxVersion) ge "vmx-10" ) && ( Common_Utils::GetViewProperty( $args{vmView},'summary.config.guestFullName') =~ /Windows/i ) )
	{
		$ControllerCount= 8; #including SATA controllers
	}
	
	my $devices 		= Common_Utils::GetViewProperty( $args{vmView},'config.hardware.device');
	foreach my $device ( @$devices )
	{
		if ( $device->deviceInfo->label =~ /SCSI controller/i )
		{
			$controllerType{scsi}	= ref ( $device );		
			$ControllerCount--;
			$ControllerInfo{ $device->key }	= ref ( $device );
		}
		elsif ( $device->deviceInfo->label =~ /SATA controller/i )
		{
			$controllerType{sata}	= ref ( $device );
			$ControllerCount--;
			$ControllerInfo{ $device->key }	= ref ( $device );
		}
	}
	
	if ( 0 == $ControllerCount )
	{
		Common_Utils::WriteMessage("SCSI controllers are already added to Master Target.",2);
	}
	else
	{
		for ( my $i = 0; ( ( $i < 4 ) && ( $ControllerCount != 0 ) ) ; $i++ )
		{
			my $key 	= 1000 + $i;
			if ( exists $ControllerInfo{$key} )
			{
				next;
			} 
			Common_Utils::WriteMessage("Constructing new SCSI Controller specification : busNumber => $i , controllerType => $controllerType{scsi} , controllerKey => $key.",2);
			my( $returnCode , $scsiControllerSpec ) = NewSCSIControllerSpec( busNumber => $i , sharing => "noSharing" , controllerType => $controllerType{scsi} , controllerKey => $key );
			if ( $returnCode != $Common_Utils::SUCCESS ) 
			{
				$ReturnCode =  $Common_Utils::FAILURE;
				last;
			}
			push @deviceChange , $scsiControllerSpec;
		}
		
		if( ( lc($vmxVersion) ge "vmx-10" ) && ( Common_Utils::GetViewProperty( $args{vmView},'summary.config.guestFullName') =~ /Windows/i ) )
		{
			for ( my $i = 0; ( ( $i < 4 ) && ( $ControllerCount != 0 ) ) ; $i++ )
			{
				my $key 	= 15000 + $i;
				if ( exists $ControllerInfo{$key} )
				{
					next;
				} 
				Common_Utils::WriteMessage("Constructing new SATA Controller specification : busNumber => $i , controllerType => $controllerType{sata} , controllerKey => $key.",2);
				my( $returnCode , $scsiControllerSpec ) = NewSATAControllerSpec( busNumber => $i , controllerType => $controllerType{sata} , controllerKey => $key );
				if ( $returnCode != $Common_Utils::SUCCESS ) 
				{
					$ReturnCode =  $Common_Utils::FAILURE;
					last;
				}
				push @deviceChange , $scsiControllerSpec;
			}
		}
		
		my $returnCode	= ReconfigVm( vmView => $args{vmView} , changeConfig => { deviceChange => \@deviceChange } );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$ReturnCode	= $Common_Utils::FAILURE;
		}	
	}																	
	
	return ( $ReturnCode );
}

#####MapSourceScsiNumbersOfHost#####
##Description			:: It Maps the each and every disk selected for protection with it's corresponding SCSI number feteched through CX-API.
##Input 				:: Source Host Node. It uses source_hostid and collects information.
##Output 				:: Returns SUCCESS else FAILURE.
#####MapSourceScsiNumbersOfHost#####
sub MapSourceScsiNumbersOfHost
{
	my %args 			= @_;
	my $ReturnCode		= $Common_Utils::SUCCESS;
	
	if ( "physicalmachine" eq lc( $args{host}->getAttribute('machinetype') ) )
	{
		return ( $ReturnCode );
	}
	
	my $diskSignMap		= $args{diskSignMap};
	if ( ! defined $args{host}->getAttribute('inmage_hostid') )
	{
		Common_Utils::WriteMessage("ERROR :: Host Id value is not set for machine $args{machineName}.",3);
		$ReturnCode		= $Common_Utils::FAILURE;
		return ( $ReturnCode );
	}

	my $hostId 			= $args{host}->getAttribute('inmage_hostid');
	my %subParameters	= ( InformationType => "storage", HostGUID => $hostId );
	my( $returnCode	, $requestXml )	= XmlFunctions::ConstructXmlForCxApi( functionName => "GetHostInfo" , subParameters => \%subParameters );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to prepare XML file.",3);
		$ReturnCode		= $Common_Utils::FAILURE;
		return ( $ReturnCode );
	}

	( $returnCode ,my $responseXml )= Common_Utils::PostRequestXml( requestXml => $requestXml , cxIp => $args{cxIpAndPortNum}, functionName => "GetHostInfo" );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		$ReturnCode		= $Common_Utils::FAILURE;
		return( $ReturnCode );	
	}

	( $returnCode , my $diskList )	= XmlFunctions::ReadResponseOfCxApi( response => $responseXml , parameter =>"no" , parameterGroup => 'DiskList' );		
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to read response of CX API.",3);
		$ReturnCode		= $Common_Utils::FAILURE;
		return ( $ReturnCode );
	}	
	
	$returnCode 		= MapScsiInfo( host => $args{host} , diskList => $diskList );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to Map SCSI information for machine \"$args{machineName}\".",3);
		$ReturnCode		= $Common_Utils::FAILURE;
		return ( $ReturnCode );
	}
	
	$returnCode 		= MapDeviceName( host => $args{host} , diskList => $diskList, diskSignMap => $diskSignMap );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to Map Device names for machine \"$args{machineName}\".",3);
		$ReturnCode		= $Common_Utils::FAILURE;
		return ( $ReturnCode );
	}
		
	return ( $ReturnCode );
}

#####MapDeviceName#####
##Description 		:: Maps device name with SCSI id Mapped to Guest OS level.
##Input 			:: Host Node and Disk information from CX corresponding to the Host.
##Output 			:: Returns SUCCESS else FAILURE.
#####MapDeviceName#####
sub MapDeviceName
{
	my %args 		= @_;
	my $ReturnCode 	= $Common_Utils::SUCCESS;
	
	my $diskSignMap	= $args{diskSignMap};
	my @disksInfo	= $args{host}->getElementsByTagName('disk');
	my $diskList	= ${$args{diskList}}{DiskList};
	foreach my $diskInfo ( @disksInfo )
	{
		my $diskName 	= $diskInfo->getAttribute('disk_name');
		my $scsiNumAtEsx= $diskInfo->getAttribute('scsi_mapping_host');
		my $diskScsiId	= $diskInfo->getAttribute('disk_scsi_id');
		my $diskNameMap	= "";
		my $volumeList	= "";
		my $diskSignature = "";
		my $volumeCount	= 0;
		my $diskTypeOs	= "";
		foreach  my $disk ( sort keys %$diskList )
		{
			my $scsiPortOrScsiBusNum	= ( "windows" eq lc( $args{host}->getAttribute('os_info') ) ) ? ${$diskList}{$disk}{ScsiPort} : ${$diskList}{$disk}{ScsiBusNumber};
			if ( ( $args{host}->getAttribute('machinetype') =~ /^VirtualMachine$/i ) && ( $scsiNumAtEsx eq "$scsiPortOrScsiBusNum:${$diskList}{$disk}{ScsiTargetId}" ) )
			{
				$diskNameMap= ${$diskList}{$disk}{DiskName};
				
				if( ( "windows" eq lc( $args{host}->getAttribute('os_info') ) )  && ( ( !defined ($diskScsiId) ) || ( $diskScsiId eq "" ) ) )
				{			
					if( ${$diskList}{$disk}{DiskLayout} =~ /MBR/i)
					{
						$diskSignature	= uc(sprintf("%x", ${$diskList}{$disk}{DiskGuid}) );
					}
					else
					{
						$diskSignature	= ${$diskList}{$disk}{DiskGuid};
					}
					my @volumes	= ();
					foreach my $partition ( keys %{ $$diskList{$disk} } )
					{
						if( $partition =~ /partition/i )
						{
							foreach my $logicalVolume ( keys %{ $$diskList{$disk}{$partition} })
							{
								if( $logicalVolume =~ /LogicalVolume/i )
								{
									push @volumes, $$diskList{$disk}{$partition}{$logicalVolume}{Name};
									$volumeCount++;
								}
							}
						}			
					}
					$volumeList	= join( ",", @volumes);
					$diskTypeOs	= ${$diskList}{$disk}{DiskType};
					
					$diskInfo->setAttribute('disk_signature', $diskSignature);
					$diskInfo->setAttribute('volumelist', $volumeList );
					$diskInfo->setAttribute('volume_count', $volumeCount );
					$diskInfo->setAttribute('disk_type_os', $diskTypeOs );		
				}
			}
			elsif( ( $args{host}->getAttribute('machinetype') =~ /^PhysicalMachine$/i ) && ( $scsiNumAtEsx eq "${$diskList}{$disk}{ScsiPort}:${$diskList}{$disk}{ScsiTargetId}:${$diskList}{$disk}{ScsiLogicalUnit}" ) )
			{
				$diskNameMap= ${$diskList}{$disk}{DiskName};
			}
		}
		
		if ( "" eq $diskNameMap )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to find disk name at SCSI slot $scsiNumAtEsx( DiskName = $diskName ).",2);
			next;
		}
		$diskInfo->setAttribute('src_devicename', $diskNameMap);
		
		my $diskUuid	= $diskInfo->getAttribute('source_disk_uuid');
		my $diskSign	= $diskInfo->getAttribute('disk_signature');
		if( ( $diskSign ne "" ) && ( $diskSign ne "0" ) && ( $diskUuid ne "" ) )
		{
			$$diskSignMap{$diskUuid}{diskSign}	= $diskSign;
		}
	}
		
	return ( $ReturnCode );
}

#####RemoveUnProtectedDisks#####
##Description 		:: Removes disks from vm which are un protected.
##Input 			:: Host Node and Vm view.
##Output 			:: Returns SUCCESS else FAILURE.
#####RemoveUnProtectedDisks#####
sub RemoveUnProtectedDisks
{
	my $machineInfo 	= shift;
	my $vmView 			= shift;
	my @diskToBeRemoved	= ();
	my $vmPathName 		= Common_Utils::GetViewProperty( $vmView,'summary.config.vmPathName');
	
	my @disksInfo 		= $machineInfo->getElementsByTagName('disk');
	my $devices 		= Common_Utils::GetViewProperty( $vmView,'config.hardware.device');
	foreach my $device ( @$devices )
	{
		my $found 		= 0;
		if ( $device->deviceInfo->label =~ /Hard Disk/i )
		{
			my $key_file = $device->key;		
			my $diskName =  $device->backing->fileName;
			if( defined $vmView->snapshot )
			{
				my $diskLayoutInfo 		= Common_Utils::GetViewProperty( $vmView,'layout.disk');
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
						$diskName = $disk;
						Common_Utils::WriteMessage("Base disk information collected for key( $key_file ) = $diskName.",2);
					}
				}
			}
			my $diskNameToRemove	= $diskName;
			my $rindexVmPathName 	= rindex( $vmPathName , "/" );
			my $rindexDiskName		= rindex( $diskName , "/" );
			my $vmDatastorePath	 	= substr( $vmPathName , 0 , $rindexVmPathName );
			my $diskDatastorePath	= substr( $diskName , 0 , $rindexDiskName );
			Common_Utils::WriteMessage("Comparing $vmDatastorePath and $diskDatastorePath.",2);
			if ( $vmDatastorePath ne $diskDatastorePath )
			{
				my $vmdkName 		= substr( $diskName , $rindexDiskName , length ( $diskName ) );
				my $dotIndex		= rindex( $vmdkName , "." );
				my $vmdkFileName 	= substr( $vmdkName , 0 , $dotIndex );
				$diskName 			= $vmDatastorePath.$vmdkFileName."_InMage_NC_".$key_file.".vmdk";
				Common_Utils::WriteMessage("diskName = $diskName.",2);
			}
			
			my $vmdkUuid	= $device->backing->uuid;
			if ( ( exists $device->{backing}->{compatibilityMode} ) && ( 'physicalmode'eq lc( $device->backing->compatibilityMode ) ) )
			{
				$vmdkUuid	= $device->backing->lunUuid;
			}
							
			my @vmdkNameTarget 		= split( /\[|\]|\//,$diskName);
			foreach my $vmdk ( @disksInfo )
			{
				my $vmdkName 	= $vmdk->getAttribute('disk_name');
				my @vmdkNameXml = split( /\[|\]|\// , $vmdkName );
				
				my $tgtDiskUuid	= $vmdk->getAttribute('target_disk_uuid');			
				if( (defined $tgtDiskUuid) && ( $tgtDiskUuid ne "" ) )
				{
					if( $tgtDiskUuid eq $vmdkUuid )
					{
						$found				= 1;
						last;
					}
				}
				else
				{
					Common_Utils::WriteMessage("Comparing $vmdkNameXml[-1] and $vmdkNameTarget[-1].",2);
					if ( $vmdkNameXml[ $#vmdkNameXml ] eq $vmdkNameTarget[ $#vmdkNameTarget ] )				
					{
						$found				= 1;
						last;
					}
				}
			}
			
			if( !$found )
			{
				Common_Utils::WriteMessage("$diskNameToRemove will be removed from source machine.",2);
				push @diskToBeRemoved, $diskNameToRemove;
			}
		}
	}
	
	my $returnCode 		= VmOpts::RemoveDisk(
									 			diskInfo 	=> \@diskToBeRemoved,
									 			vmView  	=> $vmView,
									 			operation 	=> "remove",
											);
								 
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		return $Common_Utils::FAILURE;
	}
	
	return ( $Common_Utils::SUCCESS );
}

#####UpgradeVmwareTools#####
##Description		:: Upgrade VMware tools to guest machine.
##Input 			:: list of vm views.
##Output 			:: Returns SUCCESS or FAILURE.
#####UpgradeVmwareTools#####
sub UpgradeVmwareTools
{
	my $vmViewsList		= shift;
	my @task_views 		= ();
	my @machineNames	= ();
	my $ReturnCode		= $Common_Utils::SUCCESS;
	foreach my $vmView ( @$vmViewsList )
	{
		my $machineName	= $vmView->name;
		my $toolsStatus = Common_Utils::GetViewProperty( $vmView,'summary.guest.toolsStatus')->val;
		if ( $toolsStatus ne "toolsOld" )
		{
			Common_Utils::WriteMessage("ERROR :: Upgrading VMware Tools is not supported for $machineName as tools status is \"$toolsStatus\".",3);
			$ReturnCode 	= $Common_Utils::FAILURE;
			next;
		}
		Common_Utils::WriteMessage("Upgrading VMware Tools in machine $machineName.",2);
		eval
		{
			my $task	= $vmView->UpgradeTools_Task();
			my $task_view 	= Vim::get_view( mo_ref => $task );
			push @task_views,$task_view;
			push @machineNames,$machineName;
		};
		if ( $@ )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to Upgrade VMware Tools in machine $machineName.\n $@",3);
			$ReturnCode 	= $Common_Utils::FAILURE;
		}
	}
		
	for(my $i = 0; $i <= $#machineNames ; $i++)
   	{
   		my $machineName = $machineNames[$i];
   		eval
		{
	   		while(1)
	   		{
	   			my $task_view 	= $task_views[$i];
			   	my $info 	= $task_view->info;
		        if( $info->state->val eq 'running' )
		        {
#		        	Common_Utils::WriteMessage("Upgrading VMware Tools of machine $machineName in progress.",2);
		        } 
		        elsif( $info->state->val eq 'success' ) 
		        {
		           Common_Utils::WriteMessage("Upgrading VMware Tools of Virtual machine \"$machineName\" is Success.",2);
		           last;
		        } 
		        elsif( $info->state->val eq 'error' ) 
		        {
		        	my $soap_fault 	= SoapFault->new;
		            my $errorMesg 	= $soap_fault->fault_string($info->error->localizedMessage);
		            Common_Utils::WriteMessage("ERROR :: Upgrading VMware Tools of virtual machine \"$machineName\" failed with error \"$errorMesg\".",3);
		            $ReturnCode 	= $Common_Utils::FAILURE;
		            last;
		        }
		        sleep 2;
		        $task_view->ViewBase::update_view_data();
	   		}
		};
		if ( $@ )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to Upgrade VMware Tools in machine $machineName.\n $@",3);
			$ReturnCode 	= $Common_Utils::FAILURE;
		}
    }
	return ( $ReturnCode );
}
#####CheckGuestOsFullName#####
##Description 		:: checks whether the guest os parameter is set or not.
##Input 			:: vmview of the machine.
##Output			:: Returns SUCCESS on success else FAILURE.
#####CheckGuestOsFullName#####
sub CheckGuestOsFullName
{
	my $vmView		= shift;
	my $guestId		= shift;
	my $displayName = $vmView->name;
	my $ReturnCode	= $Common_Utils::SUCCESS;
	
	for ( my $i = 0 ; $i < 5; $i++ )
	{
		if ( Common_Utils::GetViewProperty( $vmView,'summary.config.guestFullName') =~ /^$/ )
		{
			Common_Utils::WriteMessage("ERROR :: Guest OS name is not populated for \"$displayName\" , So reconfiguring it.",2);
			my @deviceChange 	= ();
			my $returnCode		= ReconfigVm( vmView => $vmView , changeConfig => { guestId => $guestId } );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				Common_Utils::WriteMessage("Reconfigure of \"$displayName\" is failed.",3);
				$ReturnCode	= $Common_Utils::FAILURE;
				last;
			}
			( $returnCode, $vmView )= VmOpts::UpdateVmView( $vmView );
			if ( $returnCode != $Common_Utils::SUCCESS )
			{
				Common_Utils::WriteMessage("After Reconfigure, Update vmView of \"$displayName\" is failed.",3);
				$ReturnCode	= $Common_Utils::FAILURE;
				last;
			}
		}
		else
		{
			Common_Utils::WriteMessage("Guest OS name is populated for \"$displayName\".",2);
			$ReturnCode	= $Common_Utils::SUCCESS;
			last;
		}
	}
	return ( $ReturnCode );
}

#####PowerOnMultiVM#####
##Description		:: Poweron the multiple vms at a time.
##Input 			:: list of vm views.
##Output 			:: Returns SUCCESS or FAILURE with poweron status based on uuids.
#####PowerOnMultiVM#####
sub PowerOnMultiVM
{
	my $vmViewsList		= shift;
	my %mapUuidVmName	= ();
	my %mapUuidTask		= ();
	my %vmPoweronInfo	= ();
	my $ReturnCode		= $Common_Utils::SUCCESS;
	my $vmCount			= 0;
	foreach my $vmView ( @$vmViewsList )
	{
		my $machineName	= $vmView->name;
		my $uuid		= Common_Utils::GetViewProperty( $vmView, 'summary.config.uuid' );
		Common_Utils::WriteMessage("Powering on virtual machine $machineName.",2);
		eval
		{
			my $task		= $vmView->PowerOnVM_Task();
			my $task_view 	= Vim::get_view( mo_ref => $task );
			$mapUuidTask{$uuid}		= $task_view;
			$mapUuidVmName{$uuid}	= $machineName;
			$vmCount++;
		};
		if ( $@ )
		{
			Common_Utils::WriteMessage("Virtual Machine \"$machineName\" can't be powered on because of \n $@.",3 );
			$ReturnCode	= $Common_Utils::FAILURE;
			$vmPoweronInfo{$uuid}	= "no";
		}
	}
	
	while($vmCount)
	{	
		foreach my $uuid ( keys %mapUuidTask )
	   	{
	   		my $machineName = $mapUuidVmName{$uuid};
	   		eval
			{
				no warnings 'exiting';
	   			my $task_view 	= $mapUuidTask{$uuid};
			   	my $info 		= $task_view->info;
		        if( $info->state->val eq 'running' )
		        {
		        	my $entity_view = Vim::get_view(mo_ref => $info->entity);
		            my $question 	= $entity_view->runtime->question;
		            if ( defined $question ) 
		            {
		            	my $response = AnswerQuestion($entity_view);
		                if ( $response ne $Common_Utils::SUCCESS )
		                {
		                	$ReturnCode	= $Common_Utils::FAILURE;
		                	$vmPoweronInfo{$uuid}	= "no";
		                	delete $mapUuidTask{$uuid};
		                	$vmCount--;
		                	next;
		                }
		            }
		        } 
		        elsif( $info->state->val eq 'success' ) 
		        {
		            Common_Utils::WriteMessage("virtual machine \"$machineName\" is powered on.",2);
		            $vmPoweronInfo{$uuid}	= "yes";
		            delete $mapUuidTask{$uuid};
		            $vmCount--;
		            next;
		        }  
		        elsif( $info->state->val eq 'error' ) 
		        {
		        	my $soap_fault 	= SoapFault->new;
		            my $errorMesg 	= $soap_fault->fault_string($info->error->localizedMessage);
		            Common_Utils::WriteMessage("ERROR :: Power-on failed on machine \"$machineName\" due to error \"$errorMesg\".",3);
		            $ReturnCode	= $Common_Utils::FAILURE;
		            $vmPoweronInfo{$uuid}	= "no";
		            delete $mapUuidTask{$uuid};
		            $vmCount--;
		            next;
		        }
		        $mapUuidTask{$uuid}->ViewBase::update_view_data();
			};
			if ( $@ )
			{
				Common_Utils::WriteMessage("ERROR :: Failed to power on the machine $machineName.\n $@",3);
				$ReturnCode 	= $Common_Utils::FAILURE;
				$vmPoweronInfo{$uuid}	= "no";
				delete $mapUuidTask{$uuid};
		        $vmCount--;
			}
	    }
	}
	return ( $ReturnCode, \%vmPoweronInfo );
}

#####SetVmExtraConfigParam#####
##Description	:: set the configuation parameters into vmx file .
##Input 		:: VM View to which option value hat to set.
##Output 		:: Returns SUCCESS else FAILURE.
#####SetVmExtraConfigParam#####
sub SetVmExtraConfigParam
{
	my %args 		= @_;	
	my $machineName	= $args{vmView}->name;
	
	my $extraConfig	= Common_Utils::GetViewProperty( $args{vmView}, 'config.extraConfig');
	foreach my $config ( @$extraConfig )
	{
		if ( $config->key =~ /^$args{paramName}$/i )
		{
			if ( $args{paramValue} eq lc( $config->value ) )
			{
				return ( $Common_Utils::SUCCESS );
			} 
		}
	}
		
	Common_Utils::WriteMessage("Setting $args{paramName} value as $args{paramValue} to $machineName.",2);
	
	my @optionValues 	= ();		
	my $opt_conf = OptionValue->new( key => $args{paramName}, value => $args{paramValue});
	push( @optionValues, $opt_conf);
	
	my $returnCode	= ReconfigVm( vmView => $args{vmView} , changeConfig => { extraConfig => \@optionValues } );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	return ( $Common_Utils::SUCCESS );
}

#####ChangeDisksUuid#####
##Description	:: set/change all the vmdks uuid of a vm.
##Input 		:: VM View and datacenter view.
##Output 		:: Returns SUCCESS else FAILURE.
#####ChangeDisksUuid#####
sub ChangeDisksUuid
{
	my %args 		= @_;
	my $diskUuidMap	= $args{diskUuidMap};
	my $machineName	= $args{sourceHost}->getAttribute('display_name');
	
	my @disksInfo	= ();
	eval
	{
		@disksInfo 	= $args{sourceHost}->getElementsByTagName('disk');
	};
	if ( $@ )
	{
		Common_Utils::WriteMessage("ERROR :: $@.",3);
		Common_Utils::WriteMessage("ERROR :: Failed to configure disks of machine \"$machineName\".",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my $serviceContent 		= Vim::get_service_content();
	my $virtualDiskManager	= "";
	eval 
	{
    	$virtualDiskManager = Vim::get_view( mo_ref => $serviceContent->virtualDiskManager, properties => [] );
	};
	if ( $@ )
	{
		Common_Utils::WriteMessage("ERROR :: $@",3);
		Common_Utils::WriteMessage("ERROR :: Failed to get Virtual Disk Manager.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	foreach my $disk ( @disksInfo )
	{
		if( defined ($disk->getAttribute('selected') ) && ( "no" eq $disk->getAttribute('selected') ) )
		{
			next;
		}
		
		my $diskUuid	= $disk->getAttribute('source_disk_uuid');
		if ( "physicalmachine" eq lc( $args{sourceHost}->getAttribute('machinetype') ) )
		{
			$diskUuid	= $disk->getAttribute('disk_signature');
		}
		
		if( defined $$diskUuidMap{$diskUuid} )
		{
			next;	
		}
				
		my $diskName			= $disk->getAttribute('disk_name');
		my @diskPath			= split( /\[|\]/ , $diskName );
		$diskPath[-1]			=~ s/^\s+//;
		if ( defined( $args{sourceHost}->getAttribute('vmDirectoryPath') ) && ( "" ne $args{sourceHost}->getAttribute('vmDirectoryPath') ) )
		{ 
			$diskPath[-1]	= $args{sourceHost}->getAttribute('vmDirectoryPath')."/$diskPath[-1]";
		}		
		elsif ( ( defined( $args{sourceHost}->getAttribute('vsan_folder') ) ) && ( "" ne $args{sourceHost}->getAttribute('vsan_folder') ) )
		{
			@diskPath		= split( /\// , $diskName );
			$diskPath[-1]	= $args{sourceHost}->getAttribute('vsan_folder')."$diskPath[-1]";
		} 
		$diskName				= "[".$disk->getAttribute('datastore_selected')."] $diskPath[-1]";
		
		if(  "yes" eq $disk->getAttribute('cluster_disk') )
		{
			$$diskUuidMap{$diskUuid}	= $diskName;
		}
		
		if ( "Mapped Raw LUN" eq $disk->getAttribute('disk_mode') && ( "false" eq lc( $disk->getAttribute('convert_rdm_to_vmdk') ) ) )
		{
			next;
		}		
		my $uuidGen		= Common_Utils::generateUuid();
		my $validUuid	= "6000C29";
		if( ( "" ne $disk->getAttribute('source_disk_uuid') ) && ( "Mapped Raw LUN" ne $disk->getAttribute('disk_mode') ) )
		{
			$validUuid = $disk->getAttribute('source_disk_uuid');
		}
		$validUuid	= substr($validUuid,0,7);
		$uuidGen 	= $validUuid . substr($uuidGen,7);
		my $vdUuid	= $uuidGen;
		$uuidGen 	=~ s/-//g;
		
		my $uuid 	= "";
		for( my $i=0; $i <= length($uuidGen); $i++ )
		{
			if( $i == 16 )
			{
				$uuid .= "-";
			}
			elsif (( $i != 0) && ( $i != length($uuidGen) ) && ( $i % 2 == 0 ))
			{
				$uuid .= " ";
			}
			$uuid .= substr($uuidGen, $i, 1);
		}
		eval
		{
			$virtualDiskManager->SetVirtualDiskUuid( name => $diskName, uuid => $uuid, datacenter => $args{datacenter} );
		};
		if($@)
		{
			Common_Utils::WriteMessage("ERROR :: $@",3);
			Common_Utils::WriteMessage("ERROR :: Failed to set disks uuid of machine \"$machineName\".",3);
			return ( $Common_Utils::FAILURE );
		}
		Common_Utils::WriteMessage("Uuid \"$vdUuid\" is set to disk \"$diskName\" of machine \"$machineName\".",2);	
	}
	 
	return ( $Common_Utils::SUCCESS );
}

#####UpgradeVm#####
##Description		:: Upgrade VMware version.
##Input 			:: list of vm views.
##Output 			:: Returns SUCCESS or FAILURE.
#####UpgradeVm#####
sub UpgradeVms
{
	my $vmViewsList		= shift;
	my @task_views 		= ();
	my @machineNames	= ();
	my $ReturnCode		= $Common_Utils::SUCCESS;
	foreach my $vmView ( @$vmViewsList )
	{
		my $machineName	= $vmView->name;		
		Common_Utils::WriteMessage("Upgrading VM hardware of machine $machineName.",2);
		
		eval
		{
			my $task	= $vmView->UpgradeVM_Task();
			my $task_view 	= Vim::get_view( mo_ref => $task );
			push @task_views,$task_view;
			push @machineNames,$machineName;
		};
		if ( $@ )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to Upgrade VM hardware of machine $machineName.\n $@",3);
			$ReturnCode 	= $Common_Utils::FAILURE;
		}
	}
		
	for(my $i = 0; $i <= $#machineNames ; $i++)
   	{
   		my $machineName = $machineNames[$i];
   		eval
		{
	   		while(1)
	   		{
	   			my $task_view 	= $task_views[$i];
			   	my $info 	= $task_view->info;
		        if( $info->state->val eq 'running' )
		        {
#		        	Common_Utils::WriteMessage("Upgrading VM hardware of machine $machineName in progress.",2);
		        } 
		        elsif( $info->state->val eq 'success' ) 
		        {
		           Common_Utils::WriteMessage("Upgrading VM hardware of Virtual machine \"$machineName\" is Success.",2);
		           last;
		        } 
		        elsif( $info->state->val eq 'error' ) 
		        {
		        	my $soap_fault 	= SoapFault->new;
		            my $errorMesg 	= $soap_fault->fault_string($info->error->localizedMessage);
		            if(ref($info->error->fault) eq "AlreadyUpgraded")
		            {
		            	Common_Utils::WriteMessage("ERROR :: Virtual machine \"$machineName\"  hardware is already up-to-date.",2);
		            	last;
		            }
		            Common_Utils::WriteMessage("ERROR :: Upgrading VM hardware of virtual machine \"$machineName\" failed with error \"$errorMesg\".",3);
		            $ReturnCode 	= $Common_Utils::FAILURE;
		            last;
		        }
		        sleep 2;
		        $task_view->ViewBase::update_view_data();
	   		}
		};
		if ( $@ )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to Upgrade VM hardware of machine $machineName.\n $@",3);
			$ReturnCode 	= $Common_Utils::FAILURE;
		}
    }
	return ( $ReturnCode );
}

#####SetVmUuid#####
##Description	:: set/change the  vm uuid.
##Input 		:: VM View and 128-bit SMBIOS UUID as a hexadecimal string ("12345678-abcd-1234-cdef-123456789abc").
##Output 		:: Returns SUCCESS else FAILURE.
#####SetVmUuid#####
sub SetVmUuid
{
	my %args 		= @_;	
	my $machineName	= $args{vmView}->name;
	
	my $returnCode		= ReconfigVm( vmView => $args{vmView} , changeConfig => { uuid => $args{uuid} } );
	Common_Utils::WriteMessage("Changed the uuid of \"$machineName\" to $args{uuid}.",2);
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("Reconfigure of \"$machineName\" is failed.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	return ( $Common_Utils::SUCCESS );
}

#####UpdateVmdks#####
##Description	:: It update the vmdks properties by detaching and attaching the vmdk.
##Input 		:: Virtual Machine View and vmdk uuids to which disks are to be updated.
##Output 		:: Returns SUCCESS if everything is smooth else in any case it returns FAILURE.
#####UpdateVmdks#####
sub UpdateVmdks
{
	my %args 				= @_;
	my $machineName 		= $args{vmView}->name;
	my %mapUuidAndNewSize 	= %{ $args{uuidMap} };
	my %sataUuids			= %{ $args{sataUuid} };
	my $clusterDisks		= $args{clusterDisks};
	
	Common_Utils::WriteMessage("Updating vmdks of machine \"$machineName\".",2);
		
	my $vmDevices		= Common_Utils::GetViewProperty( $args{vmView}, 'config.hardware.device');
	my @deviceChange	= ();	
	foreach my $device ( @$vmDevices )
	{
		if( $device->deviceInfo->label !~ /Hard Disk/i )
		{
			next;
		}	
		my $disk_name 	= $device->backing->fileName;		
		my $vmdkUuid	= $device->backing->uuid;
		if ( ( exists $device->{backing}->{compatibilityMode} ) && ( 'physicalmode'eq lc( $device->backing->compatibilityMode ) ) )
		{
			$vmdkUuid	= $device->backing->lunUuid;
		}
		
		if(( defined $sataUuids{$vmdkUuid} ) && ( ! defined $$clusterDisks{$vmdkUuid} ) )
		{
			$device->{capacityInKB}	= $sataUuids{$vmdkUuid};
		}
		Common_Utils::WriteMessage("Updating disk $disk_name of machine $machineName.",2);
		my $deviceSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('edit'), device => $device );
		push @deviceChange, $deviceSpec;
	}	
	
	Common_Utils::WriteMessage("Reconfiguring the VM \"$machineName\".",2);
	my $returnCode		= VmOpts::ReconfigVm( vmView => $args{vmView} , changeConfig => {deviceChange => \@deviceChange });
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	( $returnCode, $args{vmView} )= VmOpts::UpdateVmView( $args{vmView} );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: vmview updation failed for \"$machineName\".",2);
	}
		
	return ( $Common_Utils::SUCCESS );
}

#####RemoveMultipleVmdks#####
##Description	:: It will remove the multiple vmdks for the given VM.
##Input 		:: Virtual Machine View and vmdk uuids to which disks are to be remove.
##Output 		:: Returns SUCCESS if everything is smooth else in any case it returns FAILURE.
#####RemoveMultipleVmdks#####
sub RemoveMultipleVmdks
{
	my %args 				= @_;
	my $machineName 		= $args{vmView}->name;
	my %mapUuidAndNewSize 	= %{ $args{uuidMap} };
	
	Common_Utils::WriteMessage("Removing the vmdks of machine \"$machineName\".",2);
		
	my $vmDevices		= Common_Utils::GetViewProperty( $args{vmView}, 'config.hardware.device');
	my @deviceChange	= ();	
	foreach my $device ( @$vmDevices )
	{
		if( $device->deviceInfo->label !~ /Hard Disk/i )
		{
			next;
		}	
		my $disk_name 	= $device->backing->fileName;		
		my $vmdkUuid	= $device->backing->uuid;
		if ( ( exists $device->{backing}->{compatibilityMode} ) && ( 'physicalmode'eq lc( $device->backing->compatibilityMode ) ) )
		{
			$vmdkUuid	= $device->backing->lunUuid;
		}
		
		if( defined $mapUuidAndNewSize{$vmdkUuid})
		{
			Common_Utils::WriteMessage("removing disk $disk_name of machine $machineName.",2);
			my $deviceSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('remove'), device => $device );
			push @deviceChange, $deviceSpec;
		}
	}	
	
	Common_Utils::WriteMessage("Reconfiguring the VM \"$machineName\".",2);
	my $returnCode		= VmOpts::ReconfigVm( vmView => $args{vmView} , changeConfig => {deviceChange => \@deviceChange });
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	( $returnCode, $args{vmView} )= VmOpts::UpdateVmView( $args{vmView} );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: vmview updation failed for \"$machineName\".",2);
	}
		
	return ( $Common_Utils::SUCCESS );
}

#####GetVmWebConsoleUrl#####
##Description 		:: Get virtual machine web console url.
##Input 			:: VM view.
##Output 			:: Returns url for vm console.
#####GetVmWebConsoleUrl#####
sub GetVmWebConsoleUrl
{
	my $vmView		= shift;
	
	my $serviceContent	= Vim::get_service_content();	
	if ( $serviceContent->about->apiType !~ /VirtualCenter/ )
	{
		return ( "NA" );
	}
	
	my $vcVersion 		= $serviceContent->about->version;
	my $vcInstanceUUID 	= Vim::get_service_content()->about->instanceUuid;
	my $vmMoRef 		= $vmView->{'mo_ref'}->{'value'};
		
	my $consoleURL = "";
	if( $vcVersion ge "5.1.0" ) 
	{
		$consoleURL = "https://$Common_Utils::ServerIp:9443/vsphere-client/vmrc/vmrc.jsp?vm=urn:vmomi:VirtualMachine:$vmMoRef:$vcInstanceUUID";
	} 
	elsif($vcVersion eq "5.0.0") 
	{		
		$consoleURL = "https://$Common_Utils::ServerIp:9443/vsphere-client/vmrc/vmrc.jsp?vm=$vcInstanceUUID:VirtualMachine:$vmMoRef";
	}
	else
	{
		return ( "NA" );
	}
	return ( $consoleURL );	
}
#####ConfigureClusterDisks#####
##Description	:: It either creates Disks/uses an existing disk.
##					1. In all cases it check whether the SCSI controller exists or not. IF does not exist, creates it.
##					2. If disk is to be re-used, it checks for its size as well.
##					3. If size has to be extended, it extends.
##					4. If size has to be decremented, it deleted exisitng disk and re-creates it.
##					5. It updates the virtual machine view from time-time.
##Input 		:: Configuration of machine to be created( XML::Element ) and Virtual Machine View to which disks are to be configured.
##Output 		:: Returns SUCCESS if everything is smooth else in any case it returns FAILURE.
#####ConfigureClusterDisks#####
sub ConfigureClusterDisks
{
	my %args 		= @_; 
	my $sourceHosts = $args{sourceHosts};
	my $diskSignMap	= $args{diskSignMap};
	my $failed 		= $Common_Utils::SUCCESS;
	
	foreach my $hostNode ( @$sourceHosts )
	{
		if( "no" eq $hostNode->getAttribute('cluster') )
		{
			next;
		}
		
		if( "virtualmachine" eq lc( $hostNode->getAttribute('machinetype') ) )
		{
			my @disksInfo	= $hostNode->getElementsByTagName('disk');
					
			foreach my $disk ( @disksInfo )
			{
				if ( ( defined $disk->getAttribute('protected') ) && ( "yes" eq lc ( $disk->getAttribute('protected') ) ) )
				{
					next;				
				}
			  
				my $diskUuid	= $disk->getAttribute('source_disk_uuid');
				if( ( !defined $disk->getAttribute('disk_signature') ) ||  ( "0" eq $disk->getAttribute('disk_signature') ) )
				{					
					$disk->setAttribute( 'disk_signature', $$diskSignMap{$diskUuid}{diskSign} );
				}
				if( ( !defined $disk->getAttribute('target_disk_uuid') ) || ( "" eq $disk->getAttribute('target_disk_uuid') ) )
				{
					$disk->setAttribute('target_disk_uuid' , $$diskSignMap{$diskUuid}{target_disk_uuid} );
				}
				if( ( !defined $disk->getAttribute('scsi_id_onmastertarget') ) || ( "" eq $disk->getAttribute('scsi_id_onmastertarget') ) )
				{
					$disk->setAttribute( 'scsi_id_onmastertarget', $$diskSignMap{$diskUuid}{scsi_id_onmastertarget} );
				}
			}
			next;
		}
		
		my $displayName 		= $hostNode->getAttribute('display_name');
		my $CreatedVmUuid		= $hostNode->getAttribute('target_uuid');
		
		my( $returnCode , $vmView )= VmOpts::GetVmViewByUuid( $CreatedVmUuid );
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			$failed				= $Common_Utils::FAILURE;
			last;
		}
		
		my $machineName = $vmView->name;
		my @deviceChange= ();
		Common_Utils::WriteMessage("Configuring disks of machine \"$machineName\".",2);
		
		my %disksInfoInVm	= ();
		$returnCode			= FindOccupiedScsiIds( $vmView , \%disksInfoInVm );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
				
		my $clusterNodes	= $hostNode->getAttribute('clusternodes_inmageguids');
		my $inmageNodeId	= $hostNode->getAttribute('inmage_hostid');
		my %appendedNodes	= ();
		foreach my $sourceHost	( @$sourceHosts )
		{
			my $inmageHostId	= $sourceHost->getAttribute('inmage_hostid');
			my $targetVMuuid	= $sourceHost->getAttribute('target_uuid');
			
			if( $CreatedVmUuid eq $targetVMuuid )
			{
				next;
			}	
			
			if( ( $clusterNodes !~ /$inmageHostId/i ) && ( $inmageNodeId eq $inmageHostId ) )
			{
				next;
			}
			
			my @disksInfo	= ();
			eval
			{
				@disksInfo 	= $sourceHost->getElementsByTagName('disk');
			};
			if ( $@ )
			{
				Common_Utils::WriteMessage("ERROR :: $@.",3);
				Common_Utils::WriteMessage("ERROR :: Failed to configure disks of machine \"$machineName\".",3);
				return ( $Common_Utils::FAILURE );
			}
		
			foreach my $disk ( @disksInfo )
			{
				if(  "no" eq $disk->getAttribute('cluster_disk') )
				{
					next;
				}
				
				if ( ( defined $disk->getAttribute('protected') ) && ( "yes" eq lc ( $disk->getAttribute('protected') ) ) )
				{
					next;			
				}
			    				
				my $diskExist			= $Common_Utils::NOTFOUND; 
				my $existingDiskInfo 	= "";
				my $diskName			= $disk->getAttribute('disk_name');
				my $targetDiskUuid		= $disk->getAttribute('target_disk_uuid');
				
				if( defined $appendedNodes{$targetDiskUuid} )
				{
					next;
				}
				
				my @diskPath			= split( /\[|\]/ , $diskName );
				$diskPath[-1]			=~ s/^\s+//;
				if ( defined( $sourceHost->getAttribute('vmDirectoryPath') ) && ( "" ne $sourceHost->getAttribute('vmDirectoryPath') ) )
				{ 
					$diskPath[-1]	= $sourceHost->getAttribute('vmDirectoryPath')."/$diskPath[-1]";
				}
				$diskName				= "[".$disk->getAttribute('datastore_selected')."] $diskPath[-1]";
				
				if ( exists $disksInfoInVm{$diskName} )
				{
					Common_Utils::WriteMessage("$diskName already added to the machine \"$machineName\".",2);
					
					if( ( defined $args{operation} ) && ( $args{operation} eq "array" ) )
					{											
						my $diskTemp	= $disk->cloneNode(0);
						$diskTemp->setAttribute( 'selected', "no" );
						Common_Utils::WriteMessage("Setting as selected for temp cluster disk = $diskName.",2);
						$hostNode->appendChild($diskTemp);
						$appendedNodes{ $diskTemp->getAttribute('target_disk_uuid') } = $diskName;
					}
					next;
				}
						
				my $controllerKey		= "";
				my $unitNumber			= "";
				
				for ( my $i = 0 ; $i <= 3 ; $i++ )
				{
					for ( my $j = 0 ; $j <= 15 ; $j++ )
					{
						if ( 7 == $j )
						{
							next;
						}
						my $unitNumCheck= 2000 + ( $i * 16 ) + $j;
						if ( exists $disksInfoInVm{$unitNumCheck} )
						{
							next;
						}
						$unitNumber		= ( $unitNumCheck - 2000 ) % 16;
						$controllerKey	= 1000 + $i;
						if( ( defined $disksInfoInVm{$controllerKey} ) && ( lc($disksInfoInVm{$controllerKey}) eq "nosharing") )
						{
							Common_Utils::WriteMessage("$controllerKey controller already exists, but nosharing mode.",2);
							$unitNumber		= "";
							$controllerKey	= "";
							last;
						}						
						last;			
					}	
					if ( ( "" ne $unitNumber )&& ( "" ne $controllerKey ) )
					{
						last;
					}
				}
		
				my $operation		= "add";				
				
				if ( ! exists $disksInfoInVm{$controllerKey} ) 
				{
					my $sharing 		= $disk->getAttribute('controller_mode');
					my $ControllerType 	= $disk->getAttribute('controller_type');
					my $busNumber		= $controllerKey - 1000;
					( $returnCode , my $scsiControllerSpec ) = NewSCSIControllerSpec( busNumber => $busNumber, sharing => $sharing, controllerType => $ControllerType, controllerKey => $controllerKey );
					if ( $returnCode ne $Common_Utils::SUCCESS ) 
					{
						$failed 		=  $Common_Utils::FAILURE;
						last;
					}
					$disksInfoInVm{$controllerKey}	= $disk->getAttribute('controller_type');
					push @deviceChange , $scsiControllerSpec;
				}
				
				my $fileSize 	= $disk->getAttribute('size');
				my $diskType 	= $disk->getAttribute('thin_disk');
				my $diskMode	= $disk->getAttribute('independent_persistent');
				 
				Common_Utils::WriteMessage("Attaching disk $diskName of size $fileSize at SCSI unit number $unitNumber of type $diskType in mode $diskMode.",2);
				my $diskBackingInfo		= VirtualDiskFlatVer2BackingInfo->new( diskMode => $diskMode , fileName => $diskName ,
																			thinProvisioned =>$diskType );
				
				if( ( "yes" eq $disk->getAttribute('cluster_disk') ) || ( $disk->getAttribute('controller_mode') !~ /noSharing/i ) )
				{
					$diskBackingInfo	= VirtualDiskFlatVer2BackingInfo->new( diskMode => $diskMode , fileName => $diskName ,
																				eagerlyScrub => "true", thinProvisioned => "false" );
				}
	
				if ( "Mapped Raw LUN" eq $disk->getAttribute('disk_mode') && ( "false" eq lc( $disk->getAttribute('convert_rdm_to_vmdk') ) ) )
				{
					my $compatibilityMode	= $disk->getAttribute('disk_type');
					my $deviceName			= $disk->getAttribute('devicename');
					$fileSize 				= $disk->getAttribute('capacity_in_kb');
					Common_Utils::WriteMessage("RDM device name = $deviceName,New size = $fileSize, compatibility mode = $compatibilityMode.",2);
					$diskBackingInfo 	= VirtualDiskRawDiskMappingVer1BackingInfo->new( compatibilityMode => $compatibilityMode, diskMode =>$diskMode,
																						 deviceName =>$deviceName, fileName => $diskName );
				}
																			
				my $connectionInfo		= VirtualDeviceConnectInfo->new( allowGuestControl => "false" , connected => "true" ,startConnected => "true" );
				my $virtualDisk			= VirtualDisk->new( controllerKey => $controllerKey , unitNumber => $unitNumber ,key => -1, 
															backing => $diskBackingInfo, capacityInKB => $fileSize );
															
				my $virtualDeviceSpec 	= VirtualDeviceConfigSpec->new(operation => VirtualDeviceConfigSpecOperation->new('add'),device => $virtualDisk );
				push @deviceChange , $virtualDeviceSpec;
				
				my $key						= 2000 + ( ( $controllerKey % 1000 ) * 16 ) + $unitNumber;
				$disksInfoInVm{$diskName}	= $key;
				$disksInfoInVm{$key}		= $diskName; 
				
				my $diskTemp	= $disk->cloneNode(0);
				my $scsi		= ( ( $controllerKey ) % 1000 ).":". $unitNumber;
				$diskTemp->setAttribute( 'scsi_mapping_vmx' , $scsi );
				$diskTemp->setAttribute( 'selected', "no" );
				Common_Utils::WriteMessage("Setting as selected for temp cluster disk = $diskName.",2);
				$hostNode->appendChild($diskTemp);
				$appendedNodes{ $diskTemp->getAttribute('target_disk_uuid') } = $diskName;
				Common_Utils::WriteMessage("Successfully attached $diskName to the machine \"$machineName\".\n",2);				
			}
		}		
		
		if ( $failed )
		{
			Common_Utils::WriteMessage("ERROR :: Either creation of virtual disk spec/creation of new controller spec has failed.",2);
			return ( $Common_Utils::FAILURE );
		}
		
		$returnCode		= ReconfigVm( vmView => $vmView , changeConfig => {deviceChange => \@deviceChange });
		if ( $returnCode != $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
	}
	return ( $Common_Utils::SUCCESS );
}

#####ResizeClusterDisks#####
##Description	:: It update the vmdks properties by detaching and attaching the vmdk.
##Input 		:: Virtual Machine View and vmdk uuids to which disks are to be updated.
##Output 		:: Returns SUCCESS if everything is smooth else in any case it returns FAILURE.
#####ResizeClusterDisks#####
sub ResizeClusterDisks
{
	my %args 				= @_;
	my $machineName 		= $args{vmView}->name;
	my %mapUuidAndNewSize 	= %{ $args{uuidMap} };	
	my $ReturnCode			= $Common_Utils::SUCCESS;
	my $clusterDisks		= $args{clusterDisks};
	
	Common_Utils::WriteMessage("Eager zeroing vmdks of the machine \"$machineName\".",2);
	
	my $serviceContent		= Vim::get_service_content();	
	my $virtualDiskManager 	= Vim::get_view( mo_ref => $serviceContent->virtualDiskManager, properties => [] );
			
	my $vmDevices		= Common_Utils::GetViewProperty( $args{vmView}, 'config.hardware.device');
	my @deviceChange	= ();
	my %mapUuidTask		= ();
	my %mapUuidVmdkName	= ();
	my $taskCount 		= 0;
	foreach my $device ( @$vmDevices )
	{
		if( $device->deviceInfo->label !~ /Hard Disk/i )
		{
			next;
		}	
		my $disk_name 	= $device->backing->fileName;		
		my $vmdkUuid	= $device->backing->uuid;
		if( exists $device->{backing}->{compatibilityMode} )
		{
			next;
		}
		
		if( defined $$clusterDisks{$vmdkUuid} )
		{
			next;
		}
#		
#		if( (! exists $device->{backing}->{eagerlyScrub} ) || (! $device->backing->eagerlyScrub ) )
#		{
#			next;
#		}
		
		if( defined $mapUuidAndNewSize{$vmdkUuid})
		{
			Common_Utils::WriteMessage("Eager zeroing the disk $disk_name of the machine $machineName.",2);
			my $deviceSpec	= VirtualDeviceConfigSpec->new( operation => VirtualDeviceConfigSpecOperation->new('edit'), device => $device );
			push @deviceChange, $deviceSpec;
			$$clusterDisks{$vmdkUuid}	= $disk_name;
			eval
			{
				my $task	= $virtualDiskManager->EagerZeroVirtualDisk_Task( name => $device->backing->fileName,
																			  datacenter => $args{datacenter},																	
																			);
				my $taskView 	= Vim::get_view( mo_ref => $task );
				$mapUuidTask{$vmdkUuid}		= $taskView;				
				$mapUuidVmdkName{$vmdkUuid}	= $device->backing->fileName;
				$taskCount++;
			}		
		}
	}
	
	while($taskCount)
	{	
		foreach my $uuid ( keys %mapUuidTask )
	   	{
	   		my $diskName = $mapUuidVmdkName{$uuid};
	   		eval
			{
				no warnings 'exiting';
	   			my $task_view 	= $mapUuidTask{$uuid};
			   	my $info 		= $task_view->info;
		        if( $info->state->val eq 'running' )
		        {
		        	#task is running
		        } 
		        elsif( $info->state->val eq 'success' ) 
		        {
		            Common_Utils::WriteMessage("VMDK eager zeroing success for \"$diskName\".",2);
		            delete $mapUuidTask{$uuid};
		            $taskCount--;
		            next;
		        }  
		        elsif( $info->state->val eq 'error' ) 
		        {
		        	my $soap_fault 	= SoapFault->new;
		            my $errorMesg 	= $soap_fault->fault_string($info->error->localizedMessage);
		            Common_Utils::WriteMessage("ERROR :: VMDK eager zeroing failed for \"$diskName\" due to error \"$errorMesg\".",3);
		            $ReturnCode	= $Common_Utils::FAILURE;
		            delete $mapUuidTask{$uuid};
		            $taskCount--;
		            next;
		        }
		        $mapUuidTask{$uuid}->ViewBase::update_view_data();
			};
			if ( $@ )
			{
				Common_Utils::WriteMessage("ERROR :: VMDK eager zeroing failed for $diskName.\n $@",3);
				$ReturnCode 	= $Common_Utils::FAILURE;
				delete $mapUuidTask{$uuid};
		        $taskCount--;
			}
	    }
	}
				
	
	Common_Utils::WriteMessage("Reconfiguring the VM \"$machineName\".",2);
	my $returnCode		= VmOpts::ReconfigVm( vmView => $args{vmView} , changeConfig => {deviceChange => \@deviceChange });
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	( $returnCode, $args{vmView} )= VmOpts::UpdateVmView( $args{vmView} );
	if ( $returnCode != $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("ERROR :: vmview updation failed for \"$machineName\".",2);
	}
		
	return ( $ReturnCode );
}

1;
