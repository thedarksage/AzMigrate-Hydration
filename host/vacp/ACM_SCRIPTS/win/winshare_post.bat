regedit /s "..\Application Data\replshares\shares.reg"
net stop lanmanserver /yes
net start lanmanserver
