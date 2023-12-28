#!/bin/sh

echo "Building thirdparty links to shared libraries and binaries"
. ../build/scripts/general/OS_details.sh

TOP_DIR=`dirname $PWD`
OS_BIN_DIR="${TOP_DIR}/thirdparty/bin/bin"
X_ARCH=`uname -ms | sed -e s"/ /_/g"`

if [ ! -h "$OS_BIN_DIR" ]; then
	mkdir -p `dirname "$OS_BIN_DIR"`
	ln -s "$TOP_DIR/thirdparty/bin/`uname | tr A-Z a-z`/$OS" "$OS_BIN_DIR"
fi
if [ ! -f "$X_ARCH/branding/platform-$OS" ]; then
	mkdir -p "$X_ARCH/branding"
	rm -f "$X_ARCH"/branding/platform-*
	touch "$X_ARCH/branding/platform-$OS"
fi

