#!/bin/bash

export BIN_PATH=`pwd`
export PID_FILE=$BIN_PATH/../conf/shark.pid

if [ -e $PID_PATH ]; then
	pid=`cat $PID_FILE`
	n=`ps aux|grep 'shark: master process'|grep -v grep|grep $pid|wc -l`

	echo $n

	if [ $n -eq 1 ]; then
	        echo "already running!"
	else
	        $BIN_PATH/start.sh
	fi
else
	$BIN_PATH/start.sh
fi


