#!/bin/bash

export BIN_PATH=/usr/local/shark/bin

ulimit -c unlimited
ulimit -s unlimited

cd $BIN_PATH/

killall -9 shark
./shark
