$BranchName=$args[0]

Write-Host "Removing I:\Source_Code\$BranchName"
Remove-Item -Recurse -Force –Path "I:\Source_Code\$BranchName"
New-Item -Path "I:\Source_Code\$BranchName" -ItemType directory
Set-Location -Path "I:\Source_Code\$BranchName"
Write-Host "Checking out code at I:\Source_Code\$BranchName"

&git clone https://msazure.visualstudio.com/DefaultCollection/One/_git/InMage-Azure-SiteRecovery
Set-Location -Path "I:\Source_Code\$BranchName\InMage-Azure-SiteRecovery"
&git checkout $BranchName

Exit-PSSession 