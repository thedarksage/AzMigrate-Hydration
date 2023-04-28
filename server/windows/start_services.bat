@echo off

ECHO "Starting Services"

net start "INMAGE-AppScheduler"
net start "InMage PushInstall"
net start WAS /Y
net start MySQL
net start tmansvc

