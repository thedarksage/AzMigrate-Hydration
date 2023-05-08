#!/bin/bash -e

DBG=""

wdir=/tmp
files="scripts/vCon/OS_details.sh bin/check_drivers.sh"

dbg()
{
    if [ ! -z $debug ]; then
        echo $@
    fi
}

get_distro()
{
    echo $1 | cut -f 4 -d "_"
}

extract_tar()
{
    $DBG
    dbg "Extracting $1"
    tar xf $1 -C $2 || {
        echo "Cannot extract $1"
        return 1
    }

    return 0
}

extract_rpm()
{
    $DBG
    rpm2cpio $1 | cpio --quiet -D $2 -idm 
}

find_extraction_base_dir()
{
    echo "$(dirname $(find $1 -name bin -type d))"
}

copy_common_files()
{
    $DBG
    local _vxdir=$(find_extraction_base_dir $1)
    local _ovxdir=$(find_extraction_base_dir $2)
    local _file=""
    local _dir=""
    
    for _file in $files; do
        _dir=$(dirname $_file)
        mkdir -p $3/common/$_dir || return 1
        echo "Copying common/$file"
        diff $_vxdir/$_file $_ovxdir/$_file
        cp -a $_vxdir/$_file $3/common/$_file || return 1
    done
}

copy_diff_drivers()
{
    $DBG
    local _vxdir=$(find_extraction_base_dir $1)
    local _kver=""
    local _dir=""
    local _drv=""

    echo "$(grep PROD_VERSION $1/.vx_version | cut -f 2 -d "=")" \
         ":"                                                   \
         "$(grep PROD_VERSION $2/.vx_version | cut -f 2 -d "=")"
    
    echo "New Drivers:" \
         "$(diff $1/supported_kernels $2/supported_kernels | grep \< | wc -l)"

    # Assert all drivers in old package are also in new
    diff $1/supported_kernels $2/supported_kernels | grep -q \> && return 1
    # Return if no new drivers
    diff $1/supported_kernels $2/supported_kernels | grep -q \< || return 0

    # log the diff
    test -z $debug || diff $1/supported_kernels $2/supported_kernels | grep \<
    
    test -d $_vxdir/bin/drivers && _dir=bin/drivers || _dir=bin
    mkdir -p $3/$_dir || return 1

    diff $1/supported_kernels $2/supported_kernels | grep \< |\
    while read _kver; do
        _drv=$_dir/involflt.ko.$(echo $_kver | cut -f 2 -d " ")
        cp $_vxdir/$_drv $3/$_drv || return 1
    done

    return 0
}

generate_supported_kernels()
{
    $DBG
    local _vxdir=$(find_extraction_base_dir $1)

    echo "Generating supported_kernels"
    find $_vxdir/bin -name involflt.ko* | awk -F "/" '{print $NF}' |\
        cut -f 3- -d "." > $1/supported_kernels
}

make_drv_pkg()
{
    $DBG
    local _ret=1

    while [ $_ret -eq 1 ]; do
        echo -n "PACKAGE: "
        # Extract the archive with driver bianries
        test -f $1/*Vx*rpm && {
            echo "RPM"
            extract_rpm $1/*Vx*rpm $1 || break
            extract_rpm $2/*Vx*rpm $2 || break
        } || { 
            echo "TAR"
            extract_tar $1/*VX*tar $1 || break
            extract_tar $2/*VX*tar $2 || break
        }

        # Generate supported kernels list if not present
        test -f $1/supported_kernels || {
            generate_supported_kernels $1 || break
            generate_supported_kernels $2 || break
        }
    
        copy_diff_drivers $@ || break

        _ret=0
    done

    return $_ret
}

error_exit()
{
    log "\nError: $1"
    exit 1
}

$DBG

debug=""
if [ "$1" = "-d" ]; then
    debug=1
    shift
fi

new=$1 # Build directory with new agents
old=$2 # Build directory with old agents

endir=$wdir/new # Directory where new agents are extracted
eodir=$wdir/old # Directory where old agents are extracted

dadir=$wdir/darchive # Archive directory for all new drivers

for dir in $endir $eodir $dadir; do
    rm -rf $dir
    mkdir -p $dir
done

ls $new/*.tar.gz |\
while read pkg
do
    distro=$(get_distro $pkg)
    printf "$distro: Clean Old Dir\r"
    sleep 1
    rm -rf $endir $eodir || error_exit "Cannot cleanup tmp dirs"
    printf "$distro: Create New Dir\r"
    sleep 1
    mkdir -p $endir $eodir || error_exit "Cannot create tmp dirs"
    printf "$distro: Extract new agent\r"
    sleep 1
    extract_tar $pkg $endir || error_exit "Cannot extract new agent"
    printf "$distro: Extract old agent\r"
    sleep 1
    extract_tar $old/*$distro* $eodir || error_exit "Cannot extract old agent"
    printf "$distro: Extract new drivers\n"
    sleep 1
    make_drv_pkg $endir $eodir $dadir/$distro || error_exit "Cannot create diff pkg"
done

copy_common_files $endir $eodir $dadir || error_exit "Cannot copy common files"

cp $endir/.vx_version $dadir || error_exit "Cannot copy .vx_version"
cp `dirname $0`/install.sh $dadir || error_exit "Cannot copy install script"

exit 0
