@echo off

net stop tmansvc /Y
net start tmansvc
net start "INMAGE-AppScheduler"