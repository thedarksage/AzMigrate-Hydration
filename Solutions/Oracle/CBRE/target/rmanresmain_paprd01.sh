#!/usr/bin/sh

cd /paprd/oraback/paprd01/
chown -R orpaprd:dba oradump

cd /paprd/oraindex/paprd01/
chown -R orpaprd:dba oradump

cd /paprd/oraarch/paprd01/
chown -R orpaprd:dba oradump

cd /paprd/oradata/paprd01/
chown -R orpaprd:dba oradump


su - orpaprd -c /usr/local/InMage/Fx/scripts/rmanres_paprd01.sh

exit 0

