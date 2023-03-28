
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
  $DIToolPath                
)

Remove-AzVMExtension -ResourceGroupName $ResourceGroupName -Name "EnableWinRM_HTTPS" -VMName $VMName -Force

Write-Output "Copy and Unzip DITestBinaries"
$vm = Get-AzVM -ResourceGroupName $ResourceGroupName -Name $VMName
$script1 = "https://a2agqldi.blob.core.windows.net/ditool/DITestBinRoot.zip"
$script2 = "https://a2agqldi.blob.core.windows.net/ditool/Extract_Install.ps1"
$runCommand = "Extract_Install.ps1 $DIToolPath"
Set-AzVMCustomScriptExtension -FileUri $script1, $script2 -Run $runCommand -VMName $VMName -ResourceGroupName $ResourceGroupName -Name "EnableWinRM_HTTPS" -Location $vm.Location
$vmex = Get-AzVMExtension -ResourceGroupName $ResourceGroupName -VMName $VMName -Name  "EnableWinRM_HTTPS" -Status
$result = $vmex.SubStatuses[0].Message
Write-Output $result