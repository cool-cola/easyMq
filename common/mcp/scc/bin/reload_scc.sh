#!/bin/sh

TCP_PIDS=`ps -ef | grep $USER |grep scc | grep -v grep | awk '{print $2}'`

for TCP_PID in $TCP_PIDS
do
	kill -s USR1 $TCP_PID
done

echo "scc Config refreshed OK"
