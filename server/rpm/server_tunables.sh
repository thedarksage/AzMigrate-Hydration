#!/bin/sh

SVSHOME="/home/svsystems"

[ ! -e ${SVSHOME}/etc/amethyst.conf ] && echo "File ${SVSHOME}/etc/amethyst.conf not found! Please check your installation!" && exit 1

date_time_suffix=$(date +%d%b%Y_%H_%M_%S)

# Back up the existing amethyst.conf
if cp ${SVSHOME}/etc/amethyst.conf ${SVSHOME}/etc/amethyst.conf.$date_time_suffix ; then
	echo "Backed up ${SVSHOME}/etc/amethyst.conf to ${SVSHOME}/etc/amethyst.conf.$date_time_suffix ..."
else
	echo "Could not take backup of ${SVSHOME}/etc/amethyst.conf ..."
	echo "Aborting!"
	exit 2
fi

# So far:
# 1. Set MAX_DIFF_FILES_THRESHOLD = 45GB

sed -e "s|MAX_DIFF_FILES_THRESHOLD[ ]*=[ ]*\"[0-9]*\"$|MAX_DIFF_FILES_THRESHOLD = \"48318382080\"|g" ${SVSHOME}/etc/amethyst.conf > ${SVSHOME}/etc/amethyst.conf.new

mv ${SVSHOME}/etc/amethyst.conf.new ${SVSHOME}/etc/amethyst.conf
chmod +rwx ${SVSHOME}/etc/amethyst.conf

if grep -q "MAX_DIFF_FILES_THRESHOLD[ ]*=[ ]*\"48318382080\"" ${SVSHOME}/etc/amethyst.conf ; then
	echo "Modified MAX_DIFF_FILES_THRESHOLD to 45 GB in ${SVSHOME}/etc/amethyst.conf ..."
fi
