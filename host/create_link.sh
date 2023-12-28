#! /bin/sh

rm -rf ./drivers/InVolFlt/sun
rm -rf ./drivers/lib/sun
cd ./drivers/InVolFlt; ln -sf solaris sun; cd ../..
cd ./drivers/lib; ln -sf solaris sun; cd ../..
