#!/bin/sh

##
# Defining the Usage
##
USAGE="Usage:   ./bld_env_setup.sh <Top Build Dir> <type of the agent build>"
[ -z "$1" ] && echo $USAGE && exit 65
[ -z "$2" ] && echo $USAGE && exit 65

##
# Making top dir for build
##
if [ ! -d "$1" ]
then
	mkdir -p $1

	if [ $? -eq 0 ]
	then
		if [ "$2" = "both" ]; then
			mkdir -p ${1}/source ${1}/logs/VX ${1}/logs/FX ${1}/binaries/VX ${1}/binaries/FX > /dev/null 2>&1
			echo "Created directory ${1} and subdirectories source, logs and binaries under it..."
		elif [ "$2" = "FX" ]; then
			mkdir -p ${1}/source ${1}/logs/FX ${1}/binaries/FX > /dev/null 2>&1
			echo "Created directory ${1} and subdirectories source, logs and binaries under it..."
		elif [ "$2" = "VX" ]; then
			mkdir -p ${1}/source ${1}/logs/VX ${1}/binaries/VX > /dev/null 2>&1
			echo "Created directory ${1} and subdirectories source, logs and binaries under it..."
		else
			echo "Directory creation failed for ${1}!"
			exit 66
		fi
	fi
else
	echo "Directory $1 already exists. Attempting to create subdirectories source, logs and binaries under it..." 
	CUR_W_D=`pwd`

	cd $1

	if [ $? -eq 0 ]
	then
		if [ "$2" = "both" ]; then
			mkdir -p source logs/VX binaries/VX logs/FX binaries/FX > /dev/null 2>&1
			cd $CUR_W_D
		elif [ "$2" = "FX" ]; then
			mkdir -p source logs/FX binaries/FX > /dev/null 2>&1
			cd $CUR_W_D
		elif [ "$2" = "VX" ]; then
			mkdir -p source logs/VX binaries/VX > /dev/null 2>&1
			cd $CUR_W_D
		fi
	else
		echo "Cannot change directories to ${1}!"
		cd $CUR_W_D
		exit 67
	fi
fi
