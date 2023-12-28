#!/bin/sh

# usage: <progname> <platform>
# eg: <progname> Linux_i686

#platform=$1
platform=`uname -ms | sed -e s"/ /_/g"`

#build debug library:

make -f sqlite3x.mak debug=yes

#build release library:
make -f sqlite3x.mak debug=no
