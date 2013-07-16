#!/bin/sh

set -e 

CLI=./texr-cli

if [ ! -e ~/.texr.cert ]; then
    TD=$(mktemp -d)
    openssl genrsa 2048 > ${TD}/key.priv 2> /dev/null
    openssl rsa -in ${TD}/key.priv -pubout -out ${TD}/key.pub 2>/dev/null
    for xn in $(seq 3) 
    do
        printf "Login: "
        read TUSER
        printf "password: "
        read -s TPASS
        printf "\n"
        wget texr.net/api/cert --http-user=${TUSER} --http-password=${TPASS}  \
            --post-file ${TD}/key.pub \
            -O ${TD}/texr.cert \
            --header 'Accept: application/x-x509-user-cert' 2>/dev/null || true
        [ -e ${TD}/texr.cert ] && \
            (grep -q CERT ${TD}/texr.cert 2>/dev/null >/dev/null) && break
    done
    openssl x509 -in ${TD}/texr.cert -subject -noout
    mv ${TD}/texr.cert ~/.texr.cert
    cat ${TD}/key.priv >> ~/.texr.cert
    rm -rf ${TD}
fi

${CLI} ~/.texr.cert

