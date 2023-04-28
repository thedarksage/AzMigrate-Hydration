mkdir -p output
rm -rf output/*
g++ -g3 -I. -I../../thirdparty/openssl-1.0.2p/include -I../../host/inmsafeint -I../../host/inmsafeint/unix -I../../host/common -I../../host/inmsafecapis -I../../host/inmsafecapis/unix generate.cpp -o output/inmgenerate -lcrypto
g++ -g3 -I. -DSV_UNIX=1 -I../../host/common/unix  -I../../host/inmsafeint -I../../host/inmsafeint/unix -I../../host/common -I../../host/inmsafecapis -I../../host/inmsafecapis/unix ../../host/config/svconfig.cpp -I../../thirdparty/openssl-1.0.2p/include license.cpp -o output/inmlicense -lcrypto
