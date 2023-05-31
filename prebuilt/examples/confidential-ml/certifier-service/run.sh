#!/bin/sh

ARCH=$1
HOST="0.0.0.0"
if [ "$2" ]; then
  HOST=$2
fi

cp -f ../certifier-data/* .
./simpleserver.${ARCH} --policyFile=policy.bin --readPolicy=true --host="${HOST}"