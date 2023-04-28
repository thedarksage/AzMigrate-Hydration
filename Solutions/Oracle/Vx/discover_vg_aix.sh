#!/bin/sh

if [ $# -ne 2 ]; then
	echo "Insufficient number of arguments"
	echo "Usage: <script> <vg nmage> <list of PVs separated by commas>"
	exit 1
fi

vg_name=$1
pv_list=$2

pv_name_list=""
for pv in `echo $pv_list | sed "s/,/ /g"`
do
	pv_name=`basename $pv`
	pv_name_list=`echo "${pv_name_list} ${pv_name}"`
done

recreatevg -Y NA -y $vg_name $pv_name_list
exit 0
