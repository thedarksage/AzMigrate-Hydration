#
# This file contains the list of services that should be started or 
# stopped in the event of a failover. The failover service will 
# run a start / stop for each of the resources listed below from 
# right to left under /etc/init.d directory. For example the order
# of execution for the 
#
# START METHOD SEQUENCE:
#
# /etc/init.d/tmanagerd start
# /etc/init.d/setresync start
# /etc/init.d/errdbflush start
# /etc/init.d/fxjobflush start
#
# STOP METHOD SEQUENCE:
#
# /etc/init.d/fxjobflush stop
# /etc/init.d/errdbflush stop
# /etc/init.d/setresync stop
# /etc/init.d/tmanagerd stop 
# 
# NOTE: The list of resources to start/stop is specific for 
# remote recovery cases only
#
tmanagerd setresync errdbflush fxjobupdate
