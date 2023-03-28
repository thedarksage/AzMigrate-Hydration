logfile="/root/testlog.txt"

datadir="/root /data1 /data2"

exec 1>>$logfile
exec 2>&1

get_rclocal()
{
        local _file=""

        for _file in /etc/rc.d/rc.local /etc/rc.local; do
                if [ -f $_file ]; then
                        rclocal="$_file"
                        chmod +x "$_file"
                        break
                fi
        done
}

del_rclocal()
{
        sed -i '$ d' $rclocal
}

add_rclocal()
{
        echo "`readlink -e $1` $2 &" >> $rclocal
}

generate()
{
        local _dir="$1/$2"
        local totalSize=0

        mkdir $_dir

        while [ "$totalSize" -le $((100*1024)) ]; do
                let size=$(($(($RANDOM * $RANDOM)) % 1024))
                if [[ $size -ne 0 ]]; then
                        dd if=/dev/urandom of=$_dir/$RANDOM bs=1024 count=$size &> /dev/null
                fi
                let "totalSize += size"
        done

        md5sum $_dir/* > $_dir/cksum
}

delete()
{
        local _dir="$1/$2"

        rm -rf $_dir/*
        rm -rf $_dir
}

# Main

get_rclocal

if [ "$1" = "" ]; then
        > $logfile
        sn=2
        echo "Starting iteration $sn: `date`"
else
        sn=$1
        del_rclocal
        echo "Starting iteration $sn: `date`"
        dmesg | grep invol
fi

if [ "$sn" = "0" ]; then
        exit
fi


for dir in $datadir; do
        generate $dir $sn &
done

wait
echo "Written: `date`"

echo "Sleeping"
sleep 900

((sn--))
add_rclocal $0 $sn
((sn++))

for dir in $datadir; do
        delete $dir $sn &
done

echo "Reboot"
reboot &

