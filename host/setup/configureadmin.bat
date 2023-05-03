@echo off

reg add "HKCU\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers" /v %1 /t REG_SZ /d RUNASADMIN /f
reg add "HKCU\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers" /v %2 /t REG_SZ /d RUNASADMIN /f




