@echo off

ECHO "Stopping Services"

net stop "INMAGE-AppScheduler"
net stop tmansvc
net stop WAS /Y
net stop MySQL
net stop cxprocessserver
