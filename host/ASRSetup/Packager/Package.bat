echo %1

set BOXSTUB=..\..\packages\SetupTools.0.0.10\Tools\boxstub_sfx.exe
set BOXTOOL=..\..\packages\SetupTools.0.0.10\Tools\boxtool.exe
set VEREDIT=..\..\packages\SetupTools.0.0.10\Tools\VerEdit.exe

echo F | copy /b /v /y %BOXSTUB% boxstub.exe
echo F | copy /b /v /y %BOXTOOL% boxtool.exe
echo F | copy /b /v /y %VEREDIT% VerEdit.exe

%1\VerEdit.exe %1\boxstub.exe %1\PropertySheet.txt
cscript CreateManifest.vbs %1\Layout
%1\boxtool.exe -i %1\manifest.xml -o %1\Package\MicrosoftAzureSiteRecoveryUnifiedSetup.exe -s %1\boxstub.exe