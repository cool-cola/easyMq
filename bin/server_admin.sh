#!/bin/bash
#set -x

doStart()
{
	echo start
	FILENAME=`basename $0`
	MCP="easy_mq_mcp"
	SCC="easy_mq_scc"
	CCS="easy_mq_ccs"

	############### config configure file #########

	###############################################################
	NUM=`ps -fu ${USER} |grep $SCC" ../etc/"$SCC".cfg" |grep -v grep|grep -v ${FILENAME}|wc -l`
	if [ $NUM -le 0 ]
	then
		echo "./$SCC ../etc/$SCC.cfg"
		./$SCC ../etc/$SCC.cfg
		./setlog ./$SCC debug
		#	./send_restart_msg ${SCC}
	fi

	###############################################################
	NUM=`ps -fu ${USER} |grep $MCP" ../etc/"$MCP".cfg" |grep -v grep|grep -v ${FILENAME}|wc -l`
	if [ $NUM -le 0 ]
	then
		echo "./$MCP ../etc/${MCP}.cfg"
		./$MCP ../etc/${MCP}.cfg
		./setlog ./$MCP debug
		#	./send_restart_msg ${MCP}
	fi

	###############################################################
	NUM=`ps -fu ${USER} |grep $CCS" ../etc/"$CCS".cfg" |grep -v grep|grep -v ${FILENAME}|wc -l`
	if [ $NUM -le 0 ]
	then
		echo "./$CCS ../etc/${CCS}.cfg"
		./$CCS ../etc/${CCS}.cfg
		./setlog $CCS debug
		#	./send_restart_msg ${CCS}
	fi
	###############################################################
}

doStart()
{
	echo start
	FILENAME=`basename $0`
	MCP="easy_mq_mcp"
	SCC="easy_mq_scc"
	CCS="easy_mq_ccs"

	############### config configure file #########

	###############################################################
	NUM=`ps -fu ${USER} |grep $SCC" ../etc/"$SCC".cfg" |grep -v grep|grep -v ${FILENAME}|wc -l`
	if [ $NUM -le 0 ]
	then
		echo "./$SCC ../etc/$SCC.cfg"
		./$SCC ../etc/$SCC.cfg
		./setlog ./$SCC debug
		#	./send_restart_msg ${SCC}
	fi

	###############################################################
	NUM=`ps -fu ${USER} |grep $MCP" ../etc/"$MCP".cfg" |grep -v grep|grep -v ${FILENAME}|wc -l`
	if [ $NUM -le 0 ]
	then
		echo "./$MCP ../etc/${MCP}.cfg"
		./$MCP ../etc/${MCP}.cfg
		./setlog ./$MCP debug
		#	./send_restart_msg ${MCP}
	fi

	###############################################################
	NUM=`ps -fu ${USER} |grep $CCS" ../etc/"$CCS".cfg" |grep -v grep|grep -v ${FILENAME}|wc -l`
	if [ $NUM -le 0 ]
	then
		echo "./$CCS ../etc/${CCS}.cfg"
		./$CCS ../etc/${CCS}.cfg
		./setlog $CCS debug
		#	./send_restart_msg ${CCS}
	fi
	###############################################################
}
doStop()
{
	echo stop
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

	########## clear shm #######
	./clearshm.sh 0
}

doStatus()
{
	echo status
	PROC="easy_mq"

	ps -ef | grep ${PROC} | grep -v grep
}

opt=$1
case "$opt" in
	"start" )
		doStart
		;;
	"stop" )
		echo stop
		doStop
		;;
	"status" )
		doStatus
		;;
	"restart" )
		doStop
		doStart
	;;
	#TODO: other options
esac

