#!/bin/bash -e

log()
{
    echo $@
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
# Copy all configurations from old file to new
#
configure_file()
{
    local _keyval=""
    local _key=""
    local _val=""

    echo "Configuring $1"

    cat $install_dir/$1 | sed -n "/Start Configuration/,/End Configuration/p" |\
    grep -v "^#" | grep "=" |\
    while read _keyval; do
        echo "Update $_keyval"
        _key=$(echo $_keyval | cut -f 1 -d "=")
        _val=$(echo $_keyval | cut -f 2 -d "=" | sed "s/\"//g")
        _val=$(echo $_val | sed 's/\//\\\//g')

        sed -i "s/$_key=\".*\"/$_key=\"${_val}\"/" "$common/$1"        
    done
}

#
# Recursively copy directory
#
copy_dir()
{
    local dpat="$(echo $1 | sed 's/\//\\\//g')\/"

    find $1 -type f |\
    while read file; do
        file=$(echo $file | sed "s/^$dpat//")
        
        test -f $install_dir/$file && {
            echo "Updating $install_dir/$file"
            cp $install_dir/$file $install_dir/$file.org
        } || {
            echo "Copying $install_dir/$file"
        }
        
        cp $1/$file $install_dir/$file || return 1
    done
}

exit_error()
{
    echo "FAIL: $@"
}

common="common"
files_to_configure="bin/check_drivers.sh"

update_vxconf=".vx_version"
installed_vxconf="/usr/local/.vx_version"

if [ $EUID -ne 0 ]; then
   exit_error "This script must be run as root" 
fi

installed_vx_version=$(extract_vxversion VERSION)
update_vx_version=$(extract_vxversion VERSION $update_vxconf)

if [ "$installed_vx_version" != "$update_vx_version" ]; then
    exit_error "This package should be applied with $update_vx_version" \
               "version of ASR agent. Installed agent is $installed_vx_version"
fi

distro=$(extract_vxversion OS_BUILT_FOR)
install_dir=$(extract_vxversion INSTALLATION_DIR)

log "Distro: $distro"
log "Install Location: $install_dir"

test -f .cleanup && {
    for file in $files_to_configure; do
        fname=$(basename $file)
        test -f ${fname}.org && mv ${fname}.org $common/${file}
    done
}

log "Configuring"
touch .cleanup
for file in $files_to_configure; do
    fname=$(basename $file)
    cp $common/${file} ${fname}.org
    configure_file $file
done

if [ ! -d $distro ]; then
    log "No new supported kernels"
else
    log "Copying drivers"
    copy_dir $distro || exit_error "Cannot copy drivers"
    sleep 1
fi

log "Copying common utilities"
copy_dir $common || exit_error "Cannot copy common utilities"

install_dir/bin/check_drivers.sh

for file in $files_to_configure; do
    fname=$(basename $file)
    test -f ${fname}.org && mv ${fname}.org $common/${file}
done

rm .cleanup
