#!/bin/sh
if [ $# -ne 1 ]; then
        echo "usage: $0 [shm_len] #0 is all"
        exit
fi

shm_length=$1

shmid=`ipcs -m|grep 0x|grep $LOGNAME|awk '{if($6==0) print $2}'`

if [ ${shm_length} -ne 0 ]; then
	shmid=`ipcs -m|grep 0x|grep $LOGNAME|grep ${shm_length}|awk '{if($6==0) print $2}'`
fi

echo "ipcrm shm  $shmid"

#ipcrm shm $shmid

