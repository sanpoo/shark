#!/bin/bash

export BIN_PATH=`pwd`
cd $BIN_PATH/

ulimit -c unlimited
ulimit -s unlimited

killall -9 shark
./shark
