package HyperVAPI;

use strict;
use warnings;
use Win32::OLE('in');
use XML::Simple;
use Data::Dumper;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use Common::Log;
use Utilities;

# Constants for boolean values
use constant
{
  FALSE => 0,
  TRUE => 1
};

# Constants for VM states
use constant
{  
  VM_STATE_ENABLED => 2,
  VM_STATE_DISABLED => 3
};

# Constant for VM Operational status
use constant VM_OPERATIONAL_STATUS_OK => 2;

# Constants for WMI connection
use constant WBEM_IMPERSONATIONLEVEL_IMPERSONATE => 3;
use constant WBEM_AUTHENTICATIONLEVEL_PACKET_PRIVACY => 6;

# Logging object
my $logging_obj = new Common::Log();
$logging_obj->set_event_name("HyperV");

my $objHyperVWMI = undef;

#------------------------------------------------------------------------------
# Connect to Hyper-V server
#------------------------------------------------------------------------------
sub Connect
{
	my ($computerName, $user, $pwd) = @_;
	my $retVal = FALSE;
	my ($retStatus, $osVersion) = GetWinOSVersion($computerName, $user, $pwd);
	if($retStatus && defined($osVersion))
	{		
		my $ns = undef;
		my $major_minor_version = substr($osVersion, 0, 3);
		if($major_minor_version eq "6.3")
		{
			$ns = "root\\virtualization\\v2";
		}
		elsif($major_minor_version eq "6.2" || $major_minor_version eq "6.1" || $major_minor_version eq "6.0")
		{
			$ns = "root\\virtualization";
		}
		else
		{			
			$logging_obj->log("EXCEPTION","Operating System not supported");
			return $retVal;
		}		
		($retVal, $objHyperVWMI) = ConnectRemoteWMI("computer" => $computerName, "namespace" => $ns, "username" => $user, "pwd" => $pwd);
	}
	else
	{
		$logging_obj->log("EXCEPTION","Cannot connect to HyperV server without OS details");
	}
	return $retVal;
}

#------------------------------------------------------------------------------
# Connect to WMI of a remote machine
#------------------------------------------------------------------------------
sub ConnectRemoteWMI
{
	my (%params) = @_;
	my $retVal = FALSE;
	my $computer = $params{computer};
	my $namespace = $params{namespace};
	my $username = $params{username};
	my $pwd = $params{pwd};
	my $wmi = undef;	

	$logging_obj->log("DEBUG", "Connecting to $namespace WMI provider of $computer");
	eval
	{
		my $objWbemLocator = undef;
		$objWbemLocator = Win32::OLE->CreateObject("WbemScripting.SWbemLocator") or $logging_obj->log("EXCEPTION","Failed to create WMI instance. Error:" . Win32::OLE->LastError());
		if(defined($objWbemLocator))
		{
			$wmi = $objWbemLocator->ConnectServer($computer, $namespace, $username, $pwd) or $logging_obj->log("EXCEPTION","WMI connection failed. Error: " . Win32::OLE->LastError());
	
			if(defined($wmi))
			{
				# To let objects use the caller's credentials.
				$wmi->Security_->{ImpersonationLevel} = WBEM_IMPERSONATIONLEVEL_IMPERSONATE;
				# To sign and encrypt each data packet so that it can protect the entire communication between client and server.				
				$wmi->Security_->{AuthenticationLevel} = WBEM_AUTHENTICATIONLEVEL_PACKET_PRIVACY;
				$logging_obj->log("DEBUG","Connected successfully.");
				$retVal = TRUE;
			}
		}
	};
	if($@)
	{
		$logging_obj->log("EXCEPTION","Failed to connect to the HyperV server");		
	}
	return ($retVal, $wmi);
}

#------------------------------------------------------------------------------
# Get the details of all Virtual Machines running in a Hyper-V
#------------------------------------------------------------------------------
sub GetVirtualMachines
{
	my $query;
	my $kvpExchangeComponents;
	my $kvpExchangeComponent;
	my $kvpExchangeDataItem;
	my $propertyName;
	my $propertyValue;
	my $vms_hash;
	my $retVal = TRUE;
	if(!$objHyperVWMI)
	{		
		$logging_obj->log("EXCEPTION","GetVirtualMachines : Could not get the WMI Object");		
		return (FALSE, undef);
	}
	my $objVMs = $objHyperVWMI->ExecQuery("select * from Msvm_ComputerSystem where Caption = 'Virtual Machine'") 
	or do
	{
		$logging_obj->log("EXCEPTION", "WMI query to get VM list has failed. Error: " . Win32::OLE->LastError());		
		return (FALSE, undef);
	};
	if($objVMs->{Count} == 0)
	{
		$logging_obj->log("DEBUG", "No VMs found in the HyperV server");
	}
	else
	{
		foreach my $objVM (in $objVMs)
		{
			my $vm_details;
			if(IsVMRunning($objVM) != TRUE)
			{
				#$logging_obj->log("DEBUG","Virtual Machine $objVM->{ElementName} is not running");
				next;
			}	
			$logging_obj->log("DEBUG","=============================================");
			$logging_obj->log("DEBUG","Virtual Machine Name: ".$objVM->{ElementName});
			$logging_obj->log("DEBUG","GUID: ".$objVM->{Name});     
			
			$query = "ASSOCIATORS OF {$objVM->{Path_}->{Path}} WHERE resultClass = Msvm_KvpExchangeComponent";
			$kvpExchangeComponents = $objHyperVWMI->ExecQuery($query);
		
			if(!defined($kvpExchangeComponents) || $kvpExchangeComponents->{Count} != 1)
			{
				$logging_obj->log("EXCEPTION", "Not found Msvm_KvpExchangeComponent instance for " .$objVM->{ElementName});
				next;
			}
			$kvpExchangeComponent = $kvpExchangeComponents->ItemIndex(0);
			foreach my $kvpExchangeDataItem (@{$kvpExchangeComponent->{GuestIntrinsicExchangeItems}})
			{
				if(!defined($kvpExchangeDataItem) && $kvpExchangeDataItem eq "")			
				{
					next;
				}
				#$logging_obj->log("DEBUG","GuestIntrinsicExchangeItems: ".$kvpExchangeDataItem);
				my $doc = XMLin($kvpExchangeDataItem);
				#$logging_obj->log("DEBUG","\n" . Dumper($doc));

				$propertyName = "";
				$propertyValue = "";
				foreach my $property (@{$doc->{PROPERTY}})
				{
					if($property->{NAME} eq "Name")
					{
						#$logging_obj->log("DEBUG","NAME: " . $property->{VALUE});
						$propertyName = $property->{VALUE};
					}
					elsif($property->{NAME} eq "Data")
					{
						#$logging_obj->log("DEBUG","Data: " . $property->{VALUE});
						$propertyValue = $property->{VALUE};
					}
				}
				if($propertyName eq "")
				{
					next;
				}
				if($propertyName eq "OSName")
				{
					if(!$propertyValue)
					{
						$vm_details = {};
						next;
					}
					$vm_details->{operatingSystem} = $propertyValue;
					$logging_obj->log("DEBUG","OSName: $propertyValue");
				}
				elsif($propertyName eq "FullyQualifiedDomainName")
				{
					$vm_details->{hostName} = $propertyValue;
					$logging_obj->log("DEBUG","FQDN: $propertyValue");
				}
				elsif($propertyName eq "NetworkAddressIPv4")
				{
					$vm_details->{ipAddress} = $propertyValue;
					$logging_obj->log("DEBUG","IPAddress: $propertyValue");
					# Remove loopback IP address and also remove leading and trailing semicolon in the IP address list
					$vm_details->{ipAddress} =~ s/127\.0\.0\.1//g;
					$vm_details->{ipAddress} =~ s/;+$//;
					$vm_details->{ipAddress} =~ s/^;+//;
					$vm_details->{ipAddress} =~ s/;;+/;/g;
				}
			}				
			$vm_details->{uuid} = $objVM->{Name};
			$vm_details->{displayName} = $objVM->{ElementName};
			$vm_details->{powerState} = "On";	
			push(@{$vms_hash}, $vm_details);    
		}
		if(!defined($vms_hash) || scalar @{$vms_hash} == 0)
		{
			$logging_obj->log("DEBUG", "There are no VMs running in the HyperV server");   
		}
	}
	return ($retVal, $vms_hash);
}

#------------------------------------------------------------------------------
# Check if virtual machine is running
#------------------------------------------------------------------------------
sub IsVMRunning
{
  my $computerSystem = $_[0];
  my $retVal = FALSE;
  if($computerSystem->EnabledState == VM_STATE_ENABLED)
  {
    my $operationalStatus;
	foreach $operationalStatus (in ($computerSystem->OperationalStatus))
    {
	  if($operationalStatus == VM_OPERATIONAL_STATUS_OK)
      {
        $retVal = TRUE;
      }
    }
  }
  return $retVal;
}

#------------------------------------------------------------------------------
# Print virtual machines
#------------------------------------------------------------------------------
sub PrintVirtualMachines
{
  my ($vms_hash) = @_;
  foreach my $vm (in $vms_hash)
  {
    print "VM details of $vm->{displayName}: \n";
    foreach my $vm_param (keys %{$vm})
    {
      print "$vm_param = $vm->{$vm_param}\n";
    }
    print "\n";
  }
}

#------------------------------------------------------------------------------
# Get OS Version of a given Computer
#------------------------------------------------------------------------------
sub GetWinOSVersion
{
  my ($computerName, $user, $pwd) = @_;
  my $osVersion = undef;
  
  my ($retVal, $wmi) = ConnectRemoteWMI("computer" => $computerName, "namespace" => "root\\cimv2", "username" => $user, "pwd" => $pwd);  

  if($retVal && defined($wmi))
  {
    my $wmiResult = $wmi->ExecQuery("SELECT version FROM Win32_OperatingSystem") or $logging_obj->log("EXCEPTION", "Failed to get OS version of $computerName. Error: ".Win32::OLE->LastError());
    if(defined($wmiResult) && $wmiResult->{Count} != 0)
    {
      foreach my $osObj (in $wmiResult)
      {
        $osVersion = $osObj->{Version};
		$logging_obj->log("DEBUG", "OS version of $computerName is $osVersion");
        $retVal = TRUE;
      }
    }
  }
  else
  {
	$logging_obj->log("EXCEPTION", "Unable to find OS version of $computerName");
  }
  return ($retVal, $osVersion);
}

1;

__END__
