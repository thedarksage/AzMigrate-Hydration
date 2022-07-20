#! /bin/sh

if [ `uname` = 'SunOS' ]; then
        # Export LD_LIBRARY_PATH
        LD_LIBRARY_PATH=RUNTIME_INSTALL_PATH/lib64:
        export LD_LIBRARY_PATH
fi

if [ `uname` = 'AIX' ]; then
        # Export LIBPATH
        LIBPATH=SET_LIBPATH
        export LIBPATH
fi

/usr/local/InMage/Fx/bin1/`basename $0` "$@"
