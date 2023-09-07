#! /bin/sh

################
#DEFAULT VALUES#
################

DIR=`pwd`
cd ${DIR}

###################
# CHECKING FOR ACE#
###################

echo "ACE" > thirdparty_components_check.log
echo "---------" >> thirdparty_components_check.log
find ${DIR}/ace-6.4.6/ACE_wrappers -name "*.a" -print >> thirdparty_components_check.log
echo " " >> thirdparty_components_check.log

##############################################
# CHECKING WHETHER ACE_HAS_AIO_CALLS SET TO 1#
##############################################

echo "CHECKING WHETHER ACE_HAS_AIO_CALLS SET TO 1 OR NOT FOR RELEASE" >> thirdparty_components_check.log
echo "-------------------------------------------------------------- " >> thirdparty_components_check.log
cat ${DIR}/ace-6.4.6/ACE_wrappers/ace/config.h | grep "ACE_HAS_AIO_CALLS*" >> thirdparty_components_check.log
echo " " >> thirdparty_components_check.log

echo "CHECKING WHETHER ACE_HAS_AIO_CALLS SET TO 1 OR NOT FOR DEBUG" >> thirdparty_components_check.log
echo "-------------------------------------------------------------- " >> thirdparty_components_check.log
cat ${DIR}/ace-6.4.6/ACE_wrappers/ace/config.h | grep "ACE_HAS_AIO_CALLS*" >> thirdparty_components_check.log
echo " " >> thirdparty_components_check.log

#####################
# CHECKING FOR BOOST#
#####################

echo "BOOST" >> thirdparty_components_check.log
echo "---------" >> thirdparty_components_check.log
find ${DIR}/boost -name "*.a" -print >> thirdparty_components_check.log
echo " " >> thirdparty_components_check.log

###################
# CHECKING FOR CDK#
###################

echo "CDK" >> thirdparty_components_check.log
echo "---------" >> thirdparty_components_check.log
find ${DIR}/cdk-5.0.4 -name "*.a" -print >> thirdparty_components_check.log
echo " " >> thirdparty_components_check.log

####################
# CHECKING FOR CURL#
####################

echo "CURL" >> thirdparty_components_check.log
echo "---------" >> thirdparty_components_check.log
find ${DIR}/curl-7.59.0 -name "*.a" -print >> thirdparty_components_check.log
echo " " >> thirdparty_components_check.log

#######################
# CHECKING FOR OPENSSL#
#######################

echo "OPENSSL" >> thirdparty_components_check.log
echo "---------" >> thirdparty_components_check.log
find ${DIR}/openssl-1.0.2p -name "*.a" -print >> thirdparty_components_check.log
echo " " >> thirdparty_components_check.log

######################
# CHECKING FOR SQLITE#
######################

echo "SQLITE" >> thirdparty_components_check.log
echo "---------" >> thirdparty_components_check.log
find ${DIR}/sqlite-3.8.2 -name "*.a" -print >> thirdparty_components_check.log
echo " " >> thirdparty_components_check.log

#########################
# CHECKING FOR SQLITE_3X#
#########################

echo "SQLITE3X" >> thirdparty_components_check.log
echo "---------" >> thirdparty_components_check.log
find ${DIR}/sqlite3x/ -name "*.a" -print >> thirdparty_components_check.log
echo " " >> thirdparty_components_check.log

####################
# CHECKING FOR ZLIB#
####################

echo "ZLIB" >> thirdparty_components_check.log
echo "---------" >> thirdparty_components_check.log
find ${DIR}/zlib-1.2.8 -name "*.a" -print >> thirdparty_components_check.log
echo " " >> thirdparty_components_check.log

#######################
# CHECKING FOR LIBSSH2#
#######################

echo "LIBSSH" >> thirdparty_components_check.log
echo "---------" >> thirdparty_components_check.log
find ${DIR}/libssh2-1.8.0 -name "*.a" -print >> thirdparty_components_check.log
echo " " >> thirdparty_components_check.log

#######################
# CHECKING FOR LIBXML2#
#######################

echo "LIBXML" >> thirdparty_components_check.log
echo "---------" >> thirdparty_components_check.log
find ${DIR}/libxml2/libxml2-2.9.1 -name "*.a" -print >> thirdparty_components_check.log
