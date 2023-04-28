@echo off

ECHO "Stopping Services"

net stop "INMAGE-AppScheduler"
net stop "InMage PushInstall"
net stop tmansvc
net stop WAS /Y
net stop MySQL
