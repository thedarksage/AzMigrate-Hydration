
Param
(
  [parameter(Mandatory=$true)]
  [String]
  $VMName,
   
  [parameter(Mandatory=$true)]
  [String]
  $ResourceGroupName,

  [parameter(Mandatory=$true)]
  [String]
  $UserMSI,
  
  [parameter(Mandatory=$true)]
  [String]
  $DIToolPath,
  
  [parameter(Mandatory=$true)]
  [String]
  $Container 
)

Remove-AzVMExtension -ResourceGroupName $ResourceGroupName -Name "EnableWinRM_HTTPS" -VMName $VMName -Force

Write-Output "Running DI Commands and upload source checksum file to storage blob"
$vm = Get-AzVM -ResourceGroupName $ResourceGroupName -Name $VMName
$script = "https://a2agqldi.blob.core.windows.net/ditool/DiCommands_Uploadfiles_To_Storage.ps1"
$runCommand = "DiCommands_Uploadfiles_To_Storage.ps1 $UserMSI $DIToolPath $Container"
Set-AzVMCustomScriptExtension -FileUri $script -Run $runCommand -VMName $VMName -ResourceGroupName $ResourceGroupName -Name "EnableWinRM_HTTPS" -Location $vm.Location
$vmex = Get-AzVMExtension -ResourceGroupName $ResourceGroupName -VMName $VMName -Name  "EnableWinRM_HTTPS" -Status
$result = $vmex.SubStatuses[0].Message
Write-Output $result