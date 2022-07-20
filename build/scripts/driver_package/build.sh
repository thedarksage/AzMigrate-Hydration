#!/bin/bash -e

DBG=""

wdir=/tmp
build_dir="/media/builds/db"

pkg_files="scripts/vCon/OS_details.sh bin/check_drivers.sh"

dbg()
{
    if [ ! -z $debug ]; then
        echo $@
    fi
}

get_distro()
{
    basename $1 | cut -f 4 -d "_"
}

extract_tar()
{
    $DBG
    dbg "Extracting $1"
    tar xf $1 -C $2 >/dev/null 2>&1 || {
        # Master build server has old version of tar
        # which does not auto-detect lzma archive
        tar xJf $1 -C $2 >/dev/null 2>&1 || {
            echo "Cannot extract $1"
            return 1
        }
    }

    return 0
}

extract_rpm()
{
    $DBG
    cd $2 > /dev/null 2>&1
    rpm2cpio $1 | cpio --quiet -idm 
    cd - > /dev/null 2>&1
}

find_extraction_base_dir()
{
    echo "$(dirname $(find $1 -name bin -type d))"
}

copy_common_files()
{
    $DBG
    local _vxdir=$(find_extraction_base_dir $1)
    local _file=""
    local _dir=""
    
    for _file in $pkg_files; do
        _dir=$(dirname $_file)
        mkdir -p $2/common/$_dir || return 1
        echo "Copying common/$_file"
        cp -a $_vxdir/$_file $2/common/$_file || return 1
    done
}

copy_drivers()
{
    $DBG
    local _vxdir=$(find_extraction_base_dir $1)
    local _dir=""
    local _module=""
    
    test -d $_vxdir/bin/drivers && _dir=bin/drivers || _dir=bin
   
    mkdir -p $2/$_dir

    for _module in $(find $_vxdir/$_dir -name involflt*.ko*); do
	    echo "Copying $(basename $_module)"
        cp $_module $2/$_dir/ || return 1
    done
    
    test -f $1/supported_kernels && {
        echo "Copying supported_kernels"
        cp $1/supported_kernels $2/ || return 1
    }

    return 0
}

make_drv_pkg()
{
    $DBG
    local _ret=1

    echo "VERSION: $(grep PROD_VERSION $1/.vx_version | cut -f 2 -d "=")" 
    
    while [ $_ret -eq 1 ]; do
        echo -n "PACKAGE: "
        # Extract the archive with driver bianries
        test -f $1/*Vx*rpm && {
            echo "RPM"
            extract_rpm $1/*Vx*rpm $1 || break
        } || { 
            echo "TAR"
            extract_tar $1/*VX*tar $1 || break
        }

        copy_drivers $@ || break

        _ret=0
    done

    return $_ret
}

error_exit()
{
    echo "Error: $1"
    exit 1
}

$DBG

debug=""
if [ "$1" = "-d" ]; then
    debug=1
    shift
fi

bdate=$(date +%d_%b_%Y)

version=`ls $build_dir | grep -e ^[9] | sort -rn | sed -n 3p`
agent_dir=$build_dir/$version/HOST/$bdate/UnifiedAgent_Builds/release
if [ ! -d $agent_dir ]; then
    version=`ls $bdir | grep -e ^[9] | sort -rn | sed -n 2p`
    agent_dir=$build_dir/$version/HOST/$bdate/UnifiedAgent_Builds/release
    if [ ! -d $agent_dir ]; then
        echo "Not Found: $build_dir/HOST/$bdate/UnifiedAgent_Builds/release"
        exit -1
    fi
fi

echo "Agent Path: $agent_dir"

endir=$wdir/drivers # Directory where new agents are extracted
dadir=$wdir/${version}_${bdate}_drivers # Archive directory for all new drivers

for dir in $endir $dadir; do
    rm -rf $dir
    mkdir -p $dir
done

ls $agent_dir/*.tar.gz |\
while read pkg
do
    distro=$(get_distro $pkg)
    
    echo "$distro: Clean Old Dir"
    sleep 1
    rm -rf $endir || error_exit "Cannot cleanup tmp dirs"

    echo "$distro: Create New Dir"
    sleep 1
    mkdir -p $endir || error_exit "Cannot create tmp dirs"
    
    echo "$distro: Extract agent"
    sleep 1
    extract_tar $pkg $endir || error_exit "Cannot extract agent"
    
    echo "$distro: Extract drivers"
    sleep 1
    make_drv_pkg $endir $dadir/$distro || error_exit "Cannot create diff pkg"
done

copy_common_files $endir $dadir || error_exit "Cannot copy common files"

cp $endir/.vx_version $dadir || error_exit "Cannot copy .vx_version"
cp `dirname $0`/install.sh $dadir || error_exit "Cannot copy install script"

echo "Creating tar archive"
tar -C $wdir -cJf ${version}_${bdate}_drivers.tar.xz ${version}_${bdate}_drivers || error_exit "Cannot create driver archive"

echo "Copying tostaging dir"
mv ${version}_${bdate}_drivers.tar.xz $agent_dir/

exit 0

