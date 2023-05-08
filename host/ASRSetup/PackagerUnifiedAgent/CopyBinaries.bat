@echo on
rmdir Package /q /s
rmdir Layout /q /s
mkdir Package
echo %1
echo %2
echo %3
robocopy %1\ASRResources\bin\%3 %2Layout\ ASRResources.dll
robocopy %1\ASRResources\bin\%3\en %2Layout\en ASRResources.resources.dll
robocopy %1\SetupFramework\bin\%3 %2Layout\ ASRSetupFramework.dll
robocopy %1\SetupFramework\bin\%3 %2Layout\ Newtonsoft.Json.dll
robocopy %1\..\csgetfingerprint\%3 %2Layout\ csgetfingerprint.exe
robocopy %1\..\cxcli\%3 %2Layout\ cxcli.exe
robocopy %1\..\cxpsclient\%3 %2Layout\ cxpsclient.exe
robocopy %1\UnifiedAgentMSI\x64\bin\%3 %2Layout\ UnifiedAgentMSI.msi
robocopy %1\UnifiedAgentMSI\x86\bin\%3 %2Layout\x86 UnifiedAgentMSI.msi
robocopy %1\WrapperUnifiedAgent\bin\%3 %2Layout\ UnifiedAgent.exe
robocopy %1\WrapperUnifiedAgent\bin\%3 %2Layout\ UnifiedAgent.exe.config
robocopy %1\UnifiedAgentInstaller\bin\%3 %2Layout\ UnifiedAgentInstaller.exe
robocopy %1\UnifiedAgentInstaller\bin\%3 %2Layout\ UnifiedAgentInstaller.exe.config
robocopy %1\UnifiedAgentUpgrader\bin\%3 %2Layout\ UnifiedAgentUpgrader.exe
robocopy %1\UnifiedAgentUpgrader\bin\%3 %2Layout\ UnifiedAgentUpgrader.exe.config
robocopy %1\UnifiedAgentInstaller\bin\%3 %2Layout\ Interop.COMAdmin.dll
robocopy %1\UnifiedAgentInstaller\bin\%3 %2Layout\ Interop.WindowsInstaller.dll
robocopy %1\AzureVMAgent %2Layout\ WindowsAzureVmAgent.msi
robocopy %1\StorageLibrary\bin\%3 %2Layout\ StorageLibrary.dll
robocopy %1\StorageLibrary\bin\%3 %2Layout\ StorageLibrary.dll.config
robocopy %1\SystemRequirementValidator\bin\%3 %2Layout\ SystemRequirementValidator.dll
robocopy %1\SystemRequirementValidator\bin\%3 %2Layout\ SystemRequirementValidator.dll.config
robocopy %1\MobilityServiceValidator\bin\%3 %2Layout\ MobilityServiceValidator.exe
robocopy %1\MobilityServiceValidator\bin\%3 %2Layout\ MobilityServiceValidator.exe.config
robocopy %1\..\setup %2Layout\ a2a-mobilityservice-requirements.json
robocopy %1\..\setup %2Layout\ v2a-mobilityservice-requirements.json
robocopy %1\..\setup %2Layout\ spv.json
robocopy %1\..\InMageVssProvider\InstallScripts %2Layout\ InMageVSSProvider_Install.cmd
robocopy %1\..\InMageVssProvider\InstallScripts %2Layout\ InMageVssProvider_Register.vbs
robocopy %1\..\InMageVssProvider\InstallScripts %2Layout\ InMageVSSProvider_Uninstall.cmd
robocopy %1\..\InMageVssProvider\x64\Release %2Layout\ InMageVssProvider_X64.dll
rename %2Layout\InMageVssProvider_X64.dll InMageVssProvider.dll

exit /b 0