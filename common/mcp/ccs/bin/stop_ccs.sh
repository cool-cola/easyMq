#!/bin/bash

echo "stop ccs!"

TCP_PIDS=`ps -ef | grep $USER |grep ccs | grep -v grep | awk '{print $2}'`
for TCP_PID in $TCP_PIDS
do
        kill -s USR2 $TCP_PID
done
echo "stop ccs ok!"
