
usage()
{
    $DBG
    echo "$1"
    echo "Usage: $0 install|uninstall <ASR Install Directory> <vmware/azure>"
    exit -1
}

set_param()
{
    sed -i "s/$1=\".*\"/$1=\"${2}\"/" "$_CFG"
}


is_distro()
{
    $DBG
    echo $_OS | grep -qi $1 && return 0
    return 1
}

install_cleanup()
{
    $DBG
    echo "Cleanup"
}

install_init()
{
    $DBG

    _OS="`$INSTALL_DIR/scripts/vCon/OS_details.sh print`"
    echo "Generating initrd for $_OS"

    if [ -f "${_CFG}" ]; then
        rm -f "${_CFG}" 
    fi

    cp "${_LIB}" "${_CFG}" || return 1
}

$DBG
# Can be used by other scripts
V2A_MODULES="hv_storvsc mptspi mptsas"
INSTALL_DIR="$1"
INITRD_DIR="${INSTALL_DIR}/scripts/initrd"

# Private to this script
_LIB="$INITRD_DIR/involflt_init_lib.sh"
_CFG="${_LIB}.cfg"
_OS=""
