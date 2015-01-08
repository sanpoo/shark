#!/bin/bash

export BIN_PATH=/usr/local/shark/bin
export PID_FILE=/usr/local/shark/log/shark.pid


if [ -e $PID_PATH ]; then
	pid=`cat $PID_FILE`
	n=`ps aux|grep 'shark: master process'|grep -v grep|grep $pid|wc -l`

	echo $n

	if [ $n -eq 1 ]; then
	        echo "already running!"
	else
	        DT=`date`
	        $BIN_PATH/start.sh
	fi
else
	$BIN_PATH/start.sh
fi


