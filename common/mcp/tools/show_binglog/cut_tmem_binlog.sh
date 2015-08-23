#!/bin/sh
#set -x

if [ $# != 1 ]; then
	echo "usage: /data/trmem_binlog/$0 [endtime]"
	echo "exp:"
	echo "/data/trmem_binlog/$0 1302676862"
	exit 0
fi

endtime=$1

ID=0
while [ $ID -le 57 ]
do

	./show_binlog cutdir cache$ID $endtime&

	ID=`expr ${ID} + 1`
done

