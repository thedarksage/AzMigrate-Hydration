#! /bin/sh

if [ `uname` = 'SunOS' ]; then
        # Export LD_LIBRARY_PATH
	LD_LIBRARY_PATH=RUNTIME_INSTALL_PATH/Pillar/lib
        export LD_LIBRARY_PATH
fi

RUNTIME_INSTALL_PATH/Pillar/bin1/`basename $0` "$@"
