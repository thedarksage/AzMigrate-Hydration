#!/bin/bash
#%stage: boot
#%depends: start
#%provides: start
#%programs: logger 
#%modules: involflt

. /bin/involflt_init_lib.sh || {
    log "ASR module disabled"
    return 0
}

if [ "$1" != "flt_stack" ]; then
    load_driver && {
        log "ASR Module loaded"
        stack_all_disks
    } || {
        log "Failed to load ASR module"
        debug_shell
    }
    return 0
else
    # Called by udev
    is_disk $2 && stack_disk $2 || {
        log "Ignoring $2"
    }
    # udev expects exit code
    exit 0
fi



