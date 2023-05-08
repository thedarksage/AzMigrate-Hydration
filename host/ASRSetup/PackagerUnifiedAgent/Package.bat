set BOXSTUB=..\..\packages\SetupTools.0.0.10\Tools\boxstub_sfx.exe
set BOXTOOL=..\..\packages\SetupTools.0.0.10\Tools\boxtool.exe
set VEREDIT=..\..\packages\SetupTools.0.0.10\Tools\VerEdit.exe

echo F | copy /b /v /y %BOXSTUB% boxstub.exe
echo F | copy /b /v /y %BOXTOOL% boxtool.exe
echo F | copy /b /v /y %VEREDIT% VerEdit.exe

VerEdit.exe boxstub.exe PropertySheet.txt

cscript CreateManifest.vbs Layout
boxtool.exe -i manifest.xml -o Package\MicrosoftAzureSiteRecoveryUnifiedAgent.exe -s boxstub.exe