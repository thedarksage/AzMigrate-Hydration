@echo on

rem This script validates the Exchange consistency for latest tag point using vsnap proces.
rem The following values needs modified.
rem tgtvolume1 should be the replicated target drive here target dire is "S"
rem tgtvolume2 should be the replicated target drive here target drive is "R" 
rem If more target volumes specified then for each drive an extra command needs to be added [say <set tgtvolume3=T> to specify Thrid target volume]
rem exhdbpath1 and exhdhpath2 are FULL database paths
rem tgt_vol1_ret and tgt_vol2_ret are FULL Retention LOG paths
rem unqid is the unique is specified this id can be given as user defined also.


SETLOCAL
REM Set PATH
set PATH=%PATH%;%~dp0\..;%~dp0;

set cdpclipath=cdpcli.exe
set tgtvolume1=L
set tgtvolume2=M
set exhdbpath1="L:\Priv1.edb"
set exhdbpath2="L:\Pub1.edb"
set tgt_vol1_ret="V:\LL2GB\f171f03db6\3EFFE144-B94B-3549-9E820DF54333E67D\L\cdpv1.db"
set tgt_vol2_ret="V:\MM2GB\8c69c51a24\3EFFE144-B94B-3549-9E820DF54333E67D\M\cdpv1.db"
set unqid=exch_1156328354

rem removes the status files

del "%~dp0\*_Success.status"
del "%~dp0\*_Failed.status"


rem sleep time of 240 minutes

svsleep 2

cd %~dp0

CScript "%~dp0\ExchangeVerify.vbs" /CDPCLIPath:%cdpclipath% /targetVolumeDB:%tgtvolume1% /exchangeDBFilePath:%exhdbpath1% /retentionlogDB:%tgt_vol1_ret% /targetVolumeLOG:%tgtvolume2% /exchangeLogFilePath:"M:\" /retentionlogLOG:%tgt_vol2_ret% /chkFilePrefix:E00 /seq:%unqid%
CScript "%~dp0\ExchangeVerify.vbs" /CDPCLIPath:%cdpclipath% /targetVolumeDB:%tgtvolume1% /exchangeDBFilePath:%exhdbpath2% /retentionlogDB:%tgt_vol1_ret% /targetVolumeLOG:%tgtvolume2% /exchangeLogFilePath:"M:\" /retentionlogLOG:%tgt_vol2_ret% /chkFilePrefix:E00 /seq:%unqid%


rd /S /Q "%~dp0\VSNAPMounts"
