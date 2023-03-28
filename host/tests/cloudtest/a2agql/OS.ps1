# Validate using GetOsList.ps1

$global:distros = @{
    # Found using az vm image list --output table --all
    # Note : Create VM supports pattern matching to create VMs for mutiple distro-flavours in one go(for e.g. WTT-less runs)
    # The key name should be unique for WTT-based single distro-flavor GQLs and also it should match distro run for WTT-less GQLS(where all flavors of distro can run in one go)
    # for e.g : For OEL, it has to be OEL80(not OEL8), so that this key is unique compared to OEL81 and also scales for all flavors of distro.
    # Linux
    # Key         = @("VmOSType", "PublisherName", "Offer", "Sku")
    UB2204        = @("Linux", "Canonical", "0001-com-ubuntu-server-jammy", "22_04-lts")
    UB2004        = @("Linux", "Canonical", "0001-com-ubuntu-server-focal", "20_04-lts")
    UB1804        = @("Linux", "Canonical", "UbuntuServer", "18.04-LTS")
    UB1604        = @("Linux", "Canonical", "UbuntuServer", "16.04-LTS")
    UB1404        = @("Linux", "Canonical", "UbuntuServer", "14.04.5-LTS")
    SLES15SP4     = @("Linux", "SUSE", "sles-15-sp4", "gen2")
    SLES15SP3     = @("Linux", "SUSE", "sles-15-sp3-basic", "gen2")
    SLES15SP2     = @("Linux", "SUSE", "sles-15-sp2", "gen2")
    SLES15SP1     = @("Linux", "SUSE", "sles-15-sp1-byos", "gen1")
    SLES12SP5     = @("Linux", "SUSE", "SLES-12-SP5", "gen1")
    SLES11SP4     = @("Linux", "SUSE", "SLES-BYOS", "11-SP4")
    RHEL91        = @("Linux", "RedHat", "RHEL", "9_1")
    RHEL90        = @("Linux", "RedHat", "RHEL", "9_0")
    RHEL87        = @("Linux", "RedHat", "RHEL", "8_7")
    RHEL86        = @("Linux", "RedHat", "RHEL", "8_6")
    RHEL84        = @("Linux", "RedHat", "RHEL", "8_4")
    RHEL83        = @("Linux", "RedHat", "RHEL", "8_3")
    RHEL82        = @("Linux", "RedHat", "RHEL", "8.2")
    RHEL81        = @("Linux", "RedHat", "RHEL", "8.1")
    RH8           = @("Linux", "RedHat", "RHEL", "8")
    RHEL79        = @("Linux", "RedHat", "RHEL", "7_9")
    RHEL78        = @("Linux", "RedHat", "RHEL", "7.8")
    RHEL77        = @("Linux", "RedHat", "RHEL", "7.7")
    RHEL6         = @("Linux", "RedHat", "RHEL", "6.10")
    OEL90         = @("Linux", "Oracle", "Oracle-Linux", "ol9-lvm")
    OEL87         = @("Linux", "Oracle", "Oracle-Linux", "ol87-lvm")
    OEL86         = @("Linux", "Oracle", "Oracle-Linux", "ol86-lvm")
    OEL84         = @("Linux", "Oracle", "Oracle-Linux", "ol84-lvm")
    OEL81         = @("Linux", "Oracle", "Oracle-Linux", "81")
    OEL80         = @("Linux", "Oracle", "Oracle-Linux", "8")
    OEL79         = @("Linux", "Oracle", "Oracle-Linux", "79-gen2")
    OEL78         = @("Linux", "Oracle", "Oracle-Linux", "78")
    OEL7          = @("Linux", "Oracle", "Oracle-Linux", "77")
    OEL6          = @("Linux", "Oracle", "Oracle-Linux", "6.10")
    DEBIAN8       = @("Linux", "credativ", "Debian", "8")
    DEBIAN9       = @("Linux", "credativ", "Debian", "9")
    DEBIAN10      = @("Linux", "Debian", "Debian-10", "10-backports")
    DEBIAN11      = @("Linux", "Debian", "Debian-11", "11")
    # Windows
    W2K22          = @("Windows", "MicrosoftWindowsServer", "WindowsServer", "2022-datacenter")
    W2K19          = @("Windows", "MicrosoftWindowsServer", "WindowsServer", "2019-Datacenter")
    W2K16          = @("Windows", "MicrosoftWindowsServer", "WindowsServer", "2016-Datacenter")
    W2K12R2        = @("Windows", "MicrosoftWindowsServer", "WindowsServer", "2012-R2-Datacenter")
    W2012          = @("Windows", "MicrosoftWindowsServer", "WindowsServer", "2012-Datacenter")
    W2K8R2         = @("Windows", "MicrosoftWindowsServer", "WindowsServer", "2008-R2-SP1-zhcn")
    W10            = @("Windows", "MicrosoftWindowsDesktop", "Windows-10", "19h1-ent")
    W10P           = @("Windows", "MicrosoftWindowsDesktop", "Windows-10", "21h1-pro")
}

# Adding the supported distros in the list, this list will be used for enabling auto patching
# Refer https://learn.microsoft.com/en-us/azure/virtual-machines/automatic-vm-guest-patching#supported-os-images for the supported distro's list.
$AutoPatchSupportedDistros = @(
    'UB2204'
    'UB2004'
    'UB1804'
    'UB1604'
    'UB1404'
    'SLES15SP2'
    'SLES12SP5'
    'RHEL84'
    'RHEL83'
    'RHEL82'
    'RHEL81'
    'RH8'
    'RHEL79'
    'RHEL78'
    'RHEL77'
    'W2K22'
    'W2K19'
    'W2K16'
    'W2K12R2'
)
$AutoPatchSupportedDistros

# OS group tags - Upper case only
$global:tags = @{
	MON = @("W2K19", "UB1604")
	TUE = @("W10", "OEL7")
	WED = @("W2K12R2", "UB1804")
	THU = @("W2K16", "RHEL81")
	FRI = @("W2012", "SLES15SP2")
}

Function LogError()
{
	param([string] $msg)
	
	Write-Host "ERROR: $msg" -Foregroundcolor Red
}

Function LogErrorExit()
{
	param([string] $msg)
	
	Write-Host "FATAL: $msg" -Foregroundcolor Red
    throw "FATAL: $msg"
}

Function LogWarn()
{
	param([string] $msg)
	
	Write-Host "WARN: $msg" -Foregroundcolor Yellow
}


Function LogDebug()
{
	param([string] $msg)
	
	if ($Debug -eq 1) {
		Write-Host "DEBUG: $msg" -Foregroundcolor Yellow
    }
}

Function Log()
{
	param([string] $msg)
	
	Write-Host "$msg"
}


Function GenTags()
{
	# Validate the tags refer to defined distros
	foreach ($tag in $tags.keys) {
		foreach ($distro in $tags[$tag]) {
			if ($distros.ContainsKey($distro)) {
				continue
			}
					
			LogErrorExit "Distro $distro referred by tag $tag not defined"
		}		
	}

    # Add ALL tag
	$tags.add("ALL", $distros.keys)

    # Add LINUX and WINDOWS tags
    $llist = @()  
    $wlist = @()
    # Add individual distro based on OS type to LINUX/WINDOWS tags
    foreach ($distro in $distros.keys ) {
        if ($distros[$distro][0] -eq "Linux") {
            $llist += $distro
        } else {
            $wlist += $distro
        }
    }

    $tags.add("LINUX", $llist)
    $tags.add("WINDOWS", $wlist)
  

	# Add individual distros as tags
	foreach ($distro in $distros.keys ) {
		$key = $distro | % ToString
		$key = $key.ToUpper()
		$tags.add($key, $key)
	}
}

Function GetOsList()
{
	param([Parameter(Mandatory=$True)][string] $TagName)

    GenTags
	
    $TagName = $TagName.ToUpper()
    $dlist = @()

    # Check for all distros matching the input tag
    foreach ($tag in $tags.keys) {
	    $key = $tag | % ToString
	    if ($key -match "$TagName") {
		    foreach ($distro in $tags[$tag]) {
			    # Add each distro only once
			    if ($dlist -contains $distro) {
				    continue
			    }
				
			    $dlist += $distro
		    }
	    }		
    }

	return $dlist
}
