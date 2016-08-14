#!/bin/bash
echo $1
dir=$1
for i in *
do
    echo $i
    if test -f $i
    then
	echo $i
	iconv -f gbk -t utf-8 $i -o $i 
    fi
done
