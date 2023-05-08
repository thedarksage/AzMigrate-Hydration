@echo on
rmdir Package /q /s
rmdir Layout /q /s
mkdir Package
echo %1
echo %2
echo %3
robocopy %1\WrapperSetup\bin\x64\%3 %2Layout\ UnifiedSetup.exe
robocopy %1\ASRResources\bin\%3 %2Layout\ ASRResources.dll
robocopy %1\ASRResources\bin\%3\en %2Layout\en ASRResources.resources.dll
robocopy %1\SetupFramework\bin\%3 %2Layout\ ASRSetupFramework.dll
robocopy %1\..\cxcli\%3 %2Layout\ cxcli.exe
robocopy %1\..\cxpsclient\%3 %2Layout\ cxpsclient.exe
robocopy %1\RemoveUnifiedSetupConfiguration\bin\x64\%3 %2Layout\ RemoveUnifiedSetupConfiguration.dll
robocopy %1\..\setup\DRA %2Layout\ AzureSiteRecoveryProvider.exe
robocopy %1\..\setup\DRA %2Layout\ DRResources.dll
robocopy %1\..\setup\DRA %2Layout\ SetupFramework.dll
robocopy %1\..\setup\DRA %2Layout\ IntegrityCheck.dll
robocopy %1\..\setup\DRA %2Layout\ Newtonsoft.Json.dll
robocopy %1\..\setup\DRA %2Layout\ Microsoft.ApplicationInsights.dll
robocopy %1\..\setup\DRA %2Layout\ IdMgmtApiClientLib.dll
robocopy %1\..\setup\DRA %2Layout\ SrsRestApiClientLib.dll
robocopy %1\..\setup\DRA %2Layout\ AccessControl2.S2S.dll
robocopy %1\..\setup\DRA %2Layout\ TelemetryInterface.dll
robocopy %1\..\setup\DRA %2Layout\ ErrorCodeUtils.dll
robocopy %1\..\setup\DRA %2Layout\ IdMgmtInterface.dll
robocopy %1\..\setup\DRA %2Layout\ CloudSharedInfra.dll
robocopy %1\..\setup\DRA %2Layout\ AsyncInterface.dll
robocopy %1\..\setup\DRA %2Layout\ CatalogCommon.dll
robocopy %1\..\setup\DRA %2Layout\ CloudCommonInterface.dll
robocopy %1\..\setup\DRA %2Layout\ Microsoft.IdentityModel.dll
robocopy %1\..\setup\DRA %2Layout\ EndpointsConfig.xml
robocopy %1\..\setup\DRA %2Layout\ Microsoft.Identity.Client.dll
robocopy %1\..\setup\DRA %2Layout\ Microsoft.IdentityModel.Abstractions.dll
robocopy %1\..\setup\DRA %2Layout\ AzureSiteRecoveryConfigurationManager.msi
robocopy %1\..\csharphttpclient\bin\%3 %2Layout\ httpclient.dll
robocopy %1\..\InMageAPILibrary\bin %2Layout\ InMageAPILibrary.dll
robocopy %1\WrapperUnifiedAgent\bin\%3 %2Layout\ UnifiedAgent.exe
robocopy %1\WrapperUnifiedAgent\bin\%3 %2Layout\ UnifiedAgent.exe.config
robocopy %1\UnifiedAgentInstaller\bin\%3 %2Layout\ UnifiedAgentInstaller.exe
robocopy %1\UnifiedAgentInstaller\bin\%3 %2Layout\ UnifiedAgentInstaller.exe.config
robocopy %1\UnifiedAgentInstaller\bin\%3 %2Layout\ Interop.COMAdmin.dll
robocopy %1\UnifiedAgentMSI\x64\bin\%3 %2Layout\ UnifiedAgentMSI.msi
robocopy %1\..\setup\MARS %2Layout\ MARSAgentInstaller.exe
robocopy %1\SetMarsProxy\bin\x64\%3 %2Layout\ SetMarsProxy.exe
robocopy %1\..\csgetfingerprint\%3 %2Layout\ csgetfingerprint.exe
robocopy %1\..\..\Server\Windows\%3 %2Layout\ cx_thirdparty_setup.exe
robocopy %1\..\..\Server\Windows\%3 %2Layout\ cx_thirdparty_setup-*.bin
robocopy %1\..\..\Server\Windows\%3 %2Layout\ ucx_server_setup.exe
robocopy %1\..\..\Server\Windows\%3 %2Layout\ ucx_server_setup-*.bin

exit /b 0