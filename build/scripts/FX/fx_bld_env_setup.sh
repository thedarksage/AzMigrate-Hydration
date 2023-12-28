#!/bin/sh -x

##
# Defining the Usage
##
USAGE="Usage:   ./fx_bld_env_setup.sh <Top Build Dir> <CVS Username>"
[ -z $1 ] && echo $USAGE && exit 65
[ -z $2 ] && echo $USAGE && exit 65

##
# Making top dir for build
##
if [ ! -d $1 ]
then
	mkdir -p $1

	if [ $? -eq 0 ]
	then
		mkdir -p ${1}/source  ${1}/logs/FX   ${1}/binaries/FX
		echo "Created directory ${1} and subdirectories source, logs and binaries under it..."
	else
		exit 66
	fi
else
	echo "Directory $1 already exists. Attempting to create subdirectories source, logs and binaries under it..." 
	CUR_W_D=`pwd`

	cd $1

	if [ $? -eq 0 ]
	then
		mkdir -p source logs/FX binaries/FX > /dev/null 2>&1
		cd $CUR_W_D
	else
		cd $CUR_W_D
		exit 67
	fi
fi
