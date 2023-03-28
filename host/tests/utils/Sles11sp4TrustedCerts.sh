#!/bin/bash

# Shell script for below TSG
#https://docs.microsoft.com/en-us/azure/site-recovery/azure-to-azure-troubleshoot-errors#trusted-root-certificates-error-code-151066
# Note that trusted certificates are already available as part of gallery image
# download is not required

cd /etc/ssl/certs
if [ -f VeriSign_Class_3_Public_Primary_Certification_Authority_G5.pem ];then
    echo "VeriSign_Class_3_Public_Primary_Certification_Authority_G5.pem exists"
	cp VeriSign_Class_3_Public_Primary_Certification_Authority_G5.pem b204d74a.0
fi

if [ -f Baltimore_CyberTrust_Root.pem ];then
    echo "Baltimore_CyberTrust_Root.pem exists"
	cp Baltimore_CyberTrust_Root.pem 653b494a.0
fi

if [ -f DigiCert_Global_Root_CA.pem ];then
    echo "DigiCert_Global_Root_CA.pem exists"
	cp DigiCert_Global_Root_CA.pem 3513523f.0
fi
   
test ! -f /etc/ssl/certs/b204d74a.0 && echo "File /etc/ssl/certs/b204d74a.0 not found."
test ! -f /etc/ssl/certs/653b494a.0 && echo "File /etc/ssl/certs/653b494a.0 not found."
test ! -f /etc/ssl/certs/3513523f.0 && echo "File /etc/ssl/certs/3513523f.0 not found."   