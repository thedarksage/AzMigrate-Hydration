#!/usr/bin/sh

cd /hcqa/oraindex/hcqa/oradump
chown -R orhcprd:dba hcprd

cd /hcqa/oraarch/hcqa/oradump
chown -R orhcprd:dba hcprd

cd /hcqa/oradata/hcqa/oradump
chown -R orhcprd:dba hcprd

cd /hcprd/oraarch/hcprd/oradump
chown -R orhcprd:dba hcprd

cd /hcprd/oradata/hcprd/oradump
chown -R orhcprd:dba hcprd


su - orhcprd -c /usr/local/InMage/Fx/scripts/rmanres_hcprd.sh
exit 0

