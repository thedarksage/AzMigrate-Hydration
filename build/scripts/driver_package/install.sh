#!/bin/bash -e

log()
{
    # We use the same log as agent install
    # so all information is in one place
    echo $@ | tee -a /var/log/ua_install.log
}

log_quiet()
{
    # dont echo on screen
    echo $@ >> /var/log/ua_install.log
}

#
# Revert files on error
#
revert_files()
{
    local _file=""

    cat $revert_file |\
    while read _file; do
        log "Revert: $_file"
        mv ${_file}.bak ${_file} || log "Revert Failed: $_file"
    done

    log "Remove: $revert_file"
    rm -f $revert_file
}

#
# Extract value based on key provided from vx_version
#
extract_vxversion()
{
    local vxvf=$installed_vxconf

    if [ ! -z "$2" ]; then
        vxvf="$2"
    fi

    grep -w ^$1 $vxvf | cut -f 2 -d "="
}

#
# Take backup of file and copies new version of file
# if the files differ.
#
backup_copy_file()
{
    local _src="$1"
    local _dest="$2"

    log "Copy: $_src -> $_dest"
    diff -q $_src $_dest > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        log "Backup: ${_dest}.bak"
        echo "${_dest}" >> $revert_file
        cp -f "${_dest}" "${_dest}.bak" || \
            exit_error "Backup Failed: $_dest"
        
        log "Copying: ${_dest}"
        cp -f "$_src" "${_dest}" || \
            exit_error "Copy Failed: $_dest"
    else
        log "Skipping as same: $_dest"
    fi
}

#
# Copy all configurations from old file to new
#
configure_check_drivers_script()
{
    local _file="$1"
    local _cfgfile="$2"
    local _bfile=$(echo $_file | cut -f 2- -d "/")
    local _keyval=""
    local _key=""
    local _val=""

    log "Configuring: $_bfile"

    cp "$_file" "$_cfgfile" || \
        exit_error "Copy Failed: $_cfgfile" 

    cat "$install_dir/$(echo $_file | cut -f 2- -d "/")" |\
    sed -n "/Configuration/,/Configuration/p" |\
    grep -v "^#" | grep "=" |\
    while read _keyval; do
        _key=$(echo $_keyval | cut -f 1 -d "=")
        _val=$(echo $_keyval | cut -f 2 -d "=" | sed "s/\"//g")

        log "Update: $_key = $_val"
        _val=$(echo $_val | sed 's/\//\\\//g') # sed safe
        sed -i "s/$_key=\".*\"/$_key=\"${_val}\"/" "$_cfgfile" || {
            rm -f "$_cfgfile"
            log "Update Failed: $_cfgfile"
            return 1
        }
    done

    log "Diff: $_bfile:"
    diff "$_cfgfile" "$install_dir/$_bfile"
}


copy_configure_file()
{
    local _file=""
    local _cfgfile=""
    local _src=""
    local _dest=""
    
    for _file in $(find common -type f); do
        
	_src=$(basename $_file)
        _dest=$install_dir/$(echo $_file | cut -f 2- -d "/")

        if [ "$_src" = "check_drivers.sh" ]; then 
            _src="${_file}.cfg"
            configure_check_drivers_script $_file $_src || {
                test -f $_src && rm -f "$_src"
                exit_error "Config Failed: $_src" 
            }
        else
            _src="$_file"
        fi
        
        backup_copy_file $_src $_dest 
        
        if [ "$_src" != "$_file" ]; then
            rm -f "$_src"
        fi
    done
    
    return 0
}

copy_drivers()
{
    local _dir="$1"
    local _overwrite="$2"
    local _src=""
    local _dest=""

    for _src in $(find $_dir -type f -name involflt.*); do
        _dest=$install_dir/$(echo $_src | cut -f 2- -d "/")
        
        test -f $_dest && {
            log_quiet "Skipping: $_dest"
        } || {
            log "Copying: $_dest"
            cp -n $_src $_dest || error_exit "Copy Failed: $_dest"
        }
    done

    for _src in $(find $_dir -type f -name supported_kernels); do
        _dest=$(echo $_src | cut -f 2- -d "/")
        _dest="$install_dir/bin/drivers/$_dest"
        
        # On some distros, this file is not copied to install dir
        test -f $_dest && backup_copy_file $_src $_dest
    done

    return 0
}

exit_error()
{
    log "FAIL: $@"
    revert_files
    exit -1
}

common="common"
update_vxconf=".vx_version"
installed_vxconf="/usr/local/.vx_version"
revert_file="backup.log"

> $revert_file

if [ $EUID -ne 0 ]; then
   exit_error "This script must be run as root" 
fi

installed_vx_version=$(extract_vxversion VERSION)
update_vx_version=$(extract_vxversion VERSION $update_vxconf)

log "Installing driver package: $update_vx_version"

if [ "$installed_vx_version" != "$update_vx_version" ]; then
    exit_error "This package should be applied with $update_vx_version" \
               "version of ASR agent. Installed agent is $installed_vx_version"
fi

distro=$(extract_vxversion OS_BUILT_FOR)
install_dir=$(extract_vxversion INSTALLATION_DIR)

log "Distro: $distro"
log "Install Directory: $install_dir"

echo "Press Enter to continue"
read a

log "Copying drivers"
copy_drivers $distro || exit_error "Cannot copy drivers"

log "Copying common utilities"
copy_configure_file || exit_error "Cannot copy common files"

log "Installing drivers"
$install_dir/bin/check_drivers.sh

log "Remove: $revert_file"
rm -f $revert_file

log "Driver successfully installed"
