
Function Get-AzureBlobInfo
{
<#
.Synopsis
   Get-AzureBlobInfo lists all blob content stored in Azure
.DESCRIPTION
   Get-AzureBlobInfo lists all blob content stored witin all or the specified
   storage account. 
.EXAMPLE
   Get-AzureBlobInfo
 
   List all storage blobs stored within all available storage accounts
 
.EXAMPLE
  Get-AzureBlobInfo -StorageAccountName rg2disks444
 
  List all storage blobs stored wtihin the storage account rg2disks444
 
EXAMPLE
  Get-AzureBlobInfo | Select-Object Name,STorageaccount,LeaseStatus
 
  List all storage blobs and output the lease status
 
.EXAMPLE
  Get-AzureBlobInfo | Where-Object {($_.name).split(".")[-1] -like "vhd" } | Select-Object Name,STorageaccount,LeaseStatus
 
.EXAMPLE
  Get-AzureBlobInfo | Where-Object {($_.name).split(".")[-1] -like "vhd" } | Select-Object Name,STorageaccount,LeaseStatus,{($_.StorageAccountKey)[0].value} | fl
 
  List all vhd files and the storage account key, so you have all information required for removal
 
 
 
 
#>
[CmdletBinding()]
Param(
        # Name of the Storage Account
        [Parameter(Mandatory=$false,
                   ValueFromPipelineByPropertyName=$true,
                   Position=0)]
        $StorageAccountName
)
 
Begin{
 
    If ($PSBoundParameters.ContainsKey("StorageAccountName"))
    {
        Write-Verbose "StorageAccount provided: $($StorageAccountName)"
        $storageaccounts = Get-AzureRmStorageAccount | Where-Object {$_.StorageAccountName -like "$StorageAccountName"}
 
        If ($storageaccounts -eq $null)
        {
            Write-Error "invalid Storage account: $($StorageAccountName)"
        }
    }
    Else
    {
        $storageaccounts = Get-AzureRmStorageAccount 
    }
}
 
Process{
 
$blobinfo = @()
 
ForEach ($sa in $storageaccounts)
{
 
   $StorageKey = Get-AzureRmStorageAccountKey -ResourceGroupName $sa.ResourceGroupName -Name $sa.StorageAccountName 
   $containers =  $sa | Get-AzureStorageContainer 
 
   ForEach ($cont in $containers)
   {
        $blobcontent = $cont  |  Get-AzureStorageBlob 
        If ($blobcontent -eq $null)
        {
            write-verbose "Container: $($cont.name) has no blob content"
        }
           ForEach ($blob in $blobcontent)
           {
                $object = New-Object -TypeName PSObject
                # StorageAccount Info
                $object | Add-Member -MemberType NoteProperty -Name StorageAccount -Value $($sa.StorageAccountName)
                $object | Add-Member -MemberType NoteProperty -Name Location -Value $($sa.Location)
                $object | Add-Member -MemberType NoteProperty -Name SKU -Value $($sa.Sku.Name)
                $object | Add-Member -MemberType NoteProperty -Name StorageAccountKey -Value $StorageKey
                # Container Info
                $object | Add-Member -MemberType NoteProperty -Name ContainerName -Value $($cont.Name)
                $object | Add-Member -MemberType NoteProperty -Name ContainerInfo -Value $($cont)
                #Blob info
                $object | Add-Member -MemberType NoteProperty -Name BlobType -Value $($blob.BlobType)
                $object | Add-Member -MemberType NoteProperty -Name ICloudBlob -Value $($blob.ICloudBlob)       
                $object | Add-Member -MemberType NoteProperty -Name Name -Value $($blob.Name)       
                $object | Add-Member -MemberType NoteProperty -Name LeaseStatus -Value $($blob.ICloudBlob.Properties.LeaseStatus)  
                $object | Add-Member -MemberType NoteProperty -Name LeaseState -Value $($blob.ICloudBlob.Properties.LeaseState)  
                $object | Add-Member -MemberType NoteProperty -Name Properties -Value $($blob.ICloudBlob.Properties) 
                $object | Add-Member -MemberType NoteProperty -Name LastModified -Value $($blob.LastModified) 
                $blobinfo += $object
           }
        }
}
}
 
End{
    $blobinfo
}
}

function Cleanup-AzureRmPublicIPAddress
{
<#
.Synopsis
   Cleanup-AzureRmPublicIPAddress removes Public IP Addresses that are not linked
   to an Azure VirtualMachine
.DESCRIPTION
   Use the Cleanup-AzureRmPublicIPAddress to remove Azure Public IP Addresses that 
   are not linked to an existing Azure VirtualMachine based on the IPConfiguration data
   being empty.
.PARAMETER ResourceGrup
   Specifies the name of the resource group from which Public IP Addresses are
   to be retrieved.
.PARAMETER ListOnly
  Only lists Azure Public IP Addresses that are not linked to an existing Azure Virtual Machine
 
.EXAMPLE
   Cleanup-AzureRmPublicIPAddress -ResourceGroup RG_2
.EXAMPLE
   Cleanup-AzureRmPublicIPAddress -ResourceGroup RG_2 -ListOnly
 
   Lists all Public IP Addresses that have no association to a virtual machine.
 
    Name    ResourceGuid                        
    ----    ------------                        
    vm01-ip b5c0f73b-abda-4a24-b3bd-2722b08aabe0
    VM2-ip  f03360f2-887e-44fe-a5ad-396195cd8efc
    VM3-ip  5db8d1fa-f551-4794-a9c0-27cd005b4742
.NOTES
    Alex Verboon, version 1.0, 01.10.2016
#>
 
   [CmdletBinding(SupportsShouldProcess=$true,
   ConfirmImpact="High")]
    Param
    (
        # Specifies the name of the resource group from which Public IP Addresses are to be retrieved.
        [Parameter(Mandatory=$true,
                   ValueFromPipelineByPropertyName=$true,
                   Position=0)]
        [string]$ResourceGroup,
        # Only lists Azure Network Interfaces that are not linked to an existing Azure Virtual Machine
        [switch]$ListOnly
    )
    Begin
    {
        If (AzureRmResourceGroup -Name $ResourceGroup -ErrorAction SilentlyContinue )
        {        
            $az_publicipaddress = Get-AzureRmPublicIpAddress -ResourceGroupName $ResourceGroup
            $RemAzPublicIP = $az_publicipaddress |  Where-Object {$_.IpConfiguration -eq $null}
        }
        Else
        {
            Write-Error "Provided resource group does not exist: $ResourceGroup"
            Throw
        }
    }
    Process
    {
        $removed = @()
        If ($PSBoundParameters.ContainsKey("ListOnly"))
        {
            $RemAzPublicIP | Select-Object Name,ResourceGuid
        }
        Else
        {
            ForEach($pi in $RemAzPublicIP)
            {
                if ($pscmdlet.ShouldProcess("Deleting NetworkInterface $($pi.Name)"))
                {
                   Write-Output "Removing Public IP Address without Virtual Machine association: $($pi.Name)"
                   Remove-AzureRmPublicIpAddress -Name "$($pi.name)" -ResourceGroupName $ResourceGroup 
                   $object = New-Object -TypeName PSObject
                   $object | Add-Member -MemberType NoteProperty -Name Name -Value $($pi.Name)
                   $object | Add-Member -MemberType NoteProperty -Name ResourceGuid -Value $($pi.ResourceGuid)
                   $removed += $object
                }
            }
        }
    }
    End
    {
        # List the removed objects
        $removed 
    }
}

function Cleanup-AzureRmNetworkInterfaces
{
<#
.Synopsis
   Cleanup-AzureRmNetworkInterfaces removes Network Interfaces that are not linked
   to an Azure VirtualMachine
.DESCRIPTION
   Use the Cleanup-AzureRmNetworkInterfaces to remove Azure Network Interfaces that 
   are not linked to an existing Azure VirtualMachine. 
.PARAMETER ResourceGrup
   Specifies the name of the resource group from which network interfaces are
   to be retrieved.
.PARAMETER ListOnly
  Only lists Azure Network Interfaces that are not linked to an existing Azure Virtual Machine
 
.EXAMPLE
   Cleanup-AzureRmNetworkInterfaces -ResourceGroup RG_2
.EXAMPLE
   Cleanup-AzureRmNetworkInterfaces -ResourceGroup RG_2 -ListOnly
 
   Name   ResourceGuid                        
   ----   ------------                        
   vm3872 7d17b843-e9fb-4838-bce5-428817a95037
.NOTES
    Alex Verboon, version 1.0, 01.10.2016
#>
 
   [CmdletBinding(SupportsShouldProcess=$true,
   ConfirmImpact="High")]
    Param
    (
        # Specifies the name of the resource group from which network interfaces are to be retrieved.
        [Parameter(Mandatory=$true,
                   ValueFromPipelineByPropertyName=$true,
                   Position=0)]
        [string]$ResourceGroup,
        # Only lists Azure Network Interfaces that are not linked to an existing Azure Virtual Machine
        [switch]$ListOnly
    )
    Begin
    {
        If (AzureRmResourceGroup -Name $ResourceGroup -ErrorAction SilentlyContinue )
        {        
            $az_networkinterfaces = Get-AzureRmNetworkInterface -ResourceGroupName $ResourceGroup
            $RemAzNetworkInterface = $az_networkinterfaces |  Where-Object {$_.VirtualMachine -eq $null}
        }
        Else
        {
            Write-Error "Provided resource group does not exist: $ResourceGroup"
            Throw
        }
 
 
    }
    Process
    {
        $removed = @()
        If ($PSBoundParameters.ContainsKey("ListOnly"))
        {
            $RemAzNetworkInterface | Select-Object Name,ResourceGuid
        }
        Else
        {
            ForEach($ni in $RemAzNetworkInterface)
            {
                if ($pscmdlet.ShouldProcess("Deleting NetworkInterface $($ni.Name)"))
                {
                   Write-Output "Removing NetworkInterface without Virtual Machine association: $($ni.Name)"
                   Remove-AzureRmNetworkInterface -Name "$($ni.name)" -ResourceGroupName $ResourceGroup 
                   $object = New-Object -TypeName PSObject
                   $object | Add-Member -MemberType NoteProperty -Name Name -Value $($ni.Name)
                   $object | Add-Member -MemberType NoteProperty -Name ResourceGuid -Value $($ni.ResourceGuid)
                   $removed += $object
                }
            }
        }
    }
    End
    {
        # List the removed objects
        $removed 
    }
}

function Cleanup-RmNetworkSecurityGroup
{
<#
.Synopsis
   Cleanup-AzureRmSecurityGroup removes Azure Network Security Groups that are not associated with
   a Subnet or a Network interface
.DESCRIPTION
   Use the Cleanup-AzureRmSecurityGroup to remove Azure Network Security Groups that are not 
   associated with any Subnet or Network interface
   being empty.
.PARAMETER ResourceGrup
   Specifies the name of the resource group from which Public IP Addresses are
   to be retrieved.
.PARAMETER ListOnly
  Only lists Azure Network Security Groups that are not associated with a subnet or network interface
 
.EXAMPLE
   Cleanup-RmNetworkSecurityGroup -ResourceGroup RG_2
 
.EXAMPLE
   Cleanup-RmNetworkSecurityGroup -ResourceGroup RG_2 -ListOnly
 
   Lists all Azure Network Security Groups that are not associated with a subnet or network interface
 
 
.NOTES
    Alex Verboon, version 1.0, 01.10.2016
#>
 
   [CmdletBinding(SupportsShouldProcess=$true,
   ConfirmImpact="High")]
    Param
    (
        # Specifies the name of the resource group from which Public IP Addresses are to be retrieved.
        [Parameter(Mandatory=$true,
                   ValueFromPipelineByPropertyName=$true,
                   Position=0)]
        [string]$ResourceGroup,
        # Only lists Azure Network Interfaces that are not linked to an existing Azure Virtual Machine
        [switch]$ListOnly
    )
    Begin
    {
        If (AzureRmResourceGroup -Name $ResourceGroup -ErrorAction SilentlyContinue )
        {        
 
            $az_nsg = Get-AzureRmNetworkSecurityGroup -ResourceGroupName $ResourceGroup
            $RemAzSecurityGroup = $az_nsg |  Select-Object Name, Subnets,Networkinterfaces | Where-Object {$_.subnets.id -eq $null -and $_.networkinterfaces.id -eq $null}
        }
        Else
        {
            Write-Error "Provided resource group does not exist: $ResourceGroup"
            Throw
        }
    }
    Process
    {
        $removed = @()
        If ($PSBoundParameters.ContainsKey("ListOnly"))
        {
            $RemAzSecurityGroup | Select-Object Name
        }
        Else
        {
            ForEach($sg in $RemAzSecurityGroup)
            {
                if ($pscmdlet.ShouldProcess("Deleting NetworkInterface $($sg.Name)"))
                {
                   Write-Output "Removing Azurer Network Security Group: $($sg.Name)"
                   Remove-AzureRmNetworkSecurityGroup -Name "$($sg.name)" -ResourceGroupName $ResourceGroup 
                   $object = New-Object -TypeName PSObject
                   $object | Add-Member -MemberType NoteProperty -Name Name -Value $($sg.Name)
                   $removed += $object
                }
            }
        }
    }
    End
    {
        # List the removed objects
        $removed 
    }
}
<#
Get-AzureBlobInfo | Where-Object {($_.name).split(".")[-1] -like "vhd" } | Select-Object Name,STorageaccount,LeaseStatus,{($_.StorageAccountKey)[0].value} | fl
$ctx = New-AzureStorageContext -StorageAccountName saverboon1 -StorageAccountKey 5JcWTPE6PeU3BTWpkKzAw55eFRb8A0KZyb
Remove-AzureStorageBlob -Blob OSDDisk_ALEX04.vhd -Container vhds -Context $ctx

#>	
Cleanup-AzureRmNetworkInterfaces -ResourceGroup A2A-GQL-EA-RG -ListOnly
Cleanup-AzureRmPublicIPAddress -ResourceGroup A2A-GQL-EA-RG -ListOnly