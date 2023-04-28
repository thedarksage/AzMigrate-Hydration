@echo off

ECHO "Starting Services"

net start "INMAGE-AppScheduler"
net start cxprocessserver
