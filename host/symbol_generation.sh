#!/bin/sh

COMPANY=${1:-InMage}
X_VERSION_DOTTED=${2:-9.00.1}
X_VERSION_PHASE=${3:-DIT}
X_CONFIGURATION=${4:-release}

. ../build/scripts/general/OS_details.sh

X_OS=$OS

tar -cvzf ${COMPANY}_UA_${X_VERSION_DOTTED}_${X_OS}_${X_VERSION_PHASE}_`date "+%d%h%Y"`_${X_CONFIGURATION}_symbols.tar.gz `find . -name *.dbg -o -name *.dbg.gz`
