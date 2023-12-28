#Powershell script to delete the old logs/files of TRUNK Build.
#Deleting the old symbols from cache directory.

#Declaring the branch variables and reading the values from perl module.
$Branch=$args[0]


Set-Location I:\SRC\$Branch\InMage-Azure-SiteRecovery\server\windows

Remove-Item BuildLog*.txt

Remove-Item PackagingLog*.txt

Remove-Item PDBZipLog*.txt

Remove-Item SymbolMatchingLog*.txt

Remove-Item SymbolLoadLog*.txt

Remove-Item DriverSigningLog*.txt

Remove-Item *.test.user

Remove-Item InMage_*.zip

Set-Location I:\SRC\$Branch\InMage-Azure-SiteRecovery\host\setup

Remove-Item BuildLog*.txt

Remove-Item PackagingLog*.txt

Remove-Item PDBZipLog*.txt

Remove-Item SymbolMatchingLog*.txt

Remove-Item SymbolLoadLog*.txt

Remove-Item DriverSigningLog*.txt

Remove-Item InMage_*.zip

Remove-Item *.test.user

Set-Location I:\SRC\$Branch\InMage-Azure-SiteRecovery\build\scripts\pushinstall

Remove-Item BuildLog*.txt

Remove-Item PackagingLog*.txt

Remove-Item PDBZipLog*.txt

Remove-Item SymbolMatchingLog*.txt

Remove-Item SymbolLoadLog*.txt

Remove-Item DriverSigningLog*.txt

Remove-Item InMage_*.zip

Remove-Item *.test.user

Remove-Item  C:\symbols\$Branch -recurse -Force