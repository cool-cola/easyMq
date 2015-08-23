#!/bin/bash
#set -x

cd ../
BASESMCP=`pwd | xargs -i basename {}`
cd ./bin

FILENAME=`basename $0`
MCP="easy_mq_mcp"
SCC="easy_mq_scc"
CCS="easy_mq_ccs"

CPU_NUM=`cat /proc/cpuinfo |grep -c processor`

############### config configure file #########

###############################################################
	NUM=`ps -fu ${USER} |grep $SCC" ../etc/"$SCC".cfg" |grep -v grep|grep -v ${FILENAME}|wc -l`
	if [ $NUM -le 0 ]
	then
		echo "./$SCC ../etc/$SCC.cfg"
	./$SCC ../etc/$SCC.cfg
	#	./send_restart_msg ${SCC}	
	fi

###############################################################
	NUM=`ps -fu ${USER} |grep $MCP" ../etc/"$MCP".cfg" |grep -v grep|grep -v ${FILENAME}|wc -l`
	if [ $NUM -le 0 ]
	then
		echo "./$MCP ../etc/${MCP}.cfg"
	./$MCP ../etc/${MCP}.cfg
	#	./send_restart_msg ${MCP}	
	fi

###############################################################
	NUM=`ps -fu ${USER} |grep $CCS" ../etc/"$CCS".cfg" |grep -v grep|grep -v ${FILENAME}|wc -l`
	if [ $NUM -le 0 ]
	then
		echo "./$CCS ../etc/${CCS}.cfg"
	./$CCS ../etc/${CCS}.cfg
	#	./send_restart_msg ${CCS}	
	fi
###############################################################
