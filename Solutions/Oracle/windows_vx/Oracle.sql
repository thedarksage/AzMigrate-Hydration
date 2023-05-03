
conn /as sysdba
set head off;
set term off;
spool c:\temp\orabegin_temp.sql;
select 'alter tablespace ' || tablespace_name ||' begin backup;' from dba_tablespaces where contents <>'TEMPORARY';
spool c:\temp\oraend_temp.sql;
select 'alter tablespace ' || tablespace_name ||' end backup;' from dba_tablespaces where contents <>'TEMPORARY';
spool c:\temp\drivelist_temp.txt
select unique( substr( file_name, 1,3) )  drive from (  select file_name from   dba_data_files union select member from v$logfile union select name from v$controlfile);
spool off;
exit;
