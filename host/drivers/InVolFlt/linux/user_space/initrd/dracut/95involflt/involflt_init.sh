#!/bin/sh
# -*- mode: shell-script; indent-tabs-mode: nil; sh-basic-offset: 4; -*-
# ex: ts=8 sw=4 sts=4 et filetype=sh

. /sbin/involflt_init_lib.sh || {
    log "ASR module disabled"
    return 0
}

if [ "$1" != "flt_stack" ]; then
    # Called by dracut hook
    load_driver && {
        log "ASR Module loaded"
        stack_all_disks
    } || {
        log "Failed to load ASR module"
        debug_shell
    }
    # dracut expects a return code
    return 0
else
    # Called by udev
    is_disk $2 && stack_disk $2 || {
        log "Ignoring $2"
    }
    # udev expects exit code
    exit 0
fi

