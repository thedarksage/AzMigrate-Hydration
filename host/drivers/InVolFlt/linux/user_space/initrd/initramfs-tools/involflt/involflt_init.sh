#!/bin/sh
#%stage: boot
#%depends: start
#%provides: start
#%programs: logger 
#%modules: involflt

. /bin/involflt_init_lib.sh || {
    log "ASR module disabled"
    return 0
}

if [ ! -c /dev/involflt ]; then
    load_driver && {
        log "ASR Module loaded"
    } || {
        log "Failed to load ASR module"
        debug_shell
    }
fi

# Called by udev
is_disk $1 && stack_disk $1 || {
    log "Ignoring $1"
}

exit 0
