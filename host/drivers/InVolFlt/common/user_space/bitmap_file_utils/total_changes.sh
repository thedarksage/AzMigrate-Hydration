#!/bin/bash
vol_name=$1;
bmap_fname=`echo ${vol_name//\//_}`;
bmap_fname=`echo "/root/InMage-$bmap_fname.VolumeLog"`;
bitmap_changes=`./bitmap_read $1 | grep "total changes in bitmap" | awk '{print $5}'`
change=`/usr/local/InMage/Vx/bin/inm_dmit --get_volume_stat $vol_name | grep "Pending Changes/bytes" | awk '{print $4}'`
byte_changes=`echo ${change##*/}`
echo "in memory changes in bytes are "$byte_changes;
echo "in bitmap changes in bytes are "$bitmap_changes;
bitmap_changes=$((bitmap_changes+byte_changes));
echo $bitmap_changes;
