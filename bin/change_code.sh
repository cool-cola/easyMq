#!/bin/bash
for i in $1/*.cpp 
do
    if test -f $i
    then
	tmp=${i##*/}
	echo $tmp
	iconv -f gbk -t utf-8 $i > /tmp/$tmp.new
	#cp /tmp/$tmp.new $i
	#rm /tmp/$tmp.new
    fi
done 

for i in $1/*.h 
do
    if test -f $i
    then
	tmp=${i##*/}
	echo $tmp
	iconv -f gbk -t utf-8 $i > /tmp/$tmp.new
	#cp /tmp/$tmp.new $i
	#rm /tmp/$tmp.new
    fi
done
