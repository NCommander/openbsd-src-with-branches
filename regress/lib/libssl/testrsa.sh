#!/bin/sh

#Test RSA certificate generation of openssl

echo 
echo RSA paramters test - NOTE THAT THIS WILL ONLY WORK IF YOU HAVE
echo compiled libssl with the src-patent tree, currently living in 
echo ~beck/src-patent.tar.gz on cvs. 
echo
echo This will *not* work with what\'s in the tree, rsa is not in that.
echo
sleep 3
 
cd $1

# Generate RSA private key
openssl genrsa -out rsakey.pem
if [ $? != 0 ]; then
        exit 1;
fi


# Denerate an RSA certificate
openssl req -config $2/openssl.cnf -key rsakey.pem -new -x509 -days 365 -out rsacert.pem
if [ $? != 0 ]; then
        exit 1;
fi


# Now check the certificate
openssl x509 -text -in rsacert.pem
if [ $? != 0 ]; then
        exit 1;
fi

exit 0
