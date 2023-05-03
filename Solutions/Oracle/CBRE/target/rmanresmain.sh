#!/bin/sh 
BACKUP_DIR=/home/oracle/oradump/$ORACLE_SID

chown oracle:dba $BACKUP_DIR/*

su - oracle -c /home/oracle/scripts/rmanres.sh
exit 0

