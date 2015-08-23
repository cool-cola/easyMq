#!/bin/bash

PROC="easy_mq"

echo "stoping ${PROC}...!"

PIDS=`pidof easy_mq_mcp easy_mq_ccs easy_mq_scc`

KILL=0
for PID in $PIDS
do
        kill -s USR2 $PID
		KILL=1
done

if [ $KILL = 1 ]
then
	echo "stop ${PROC} ok!"
else
    echo "Nothing stop!"
fi

