#
#    Copyright (C) 2014 bo.shen. All Rights Reserved.
#    Author: bo.shen <sanpoos@gmail.com>
#

#
# add files append to [src]
#
src = $(wildcard *.c core/*.c http/*.c lib/*.c project/*.c)
obj = $(patsubst %.c, %.o, $(src))

#
# include
#
PUB_INC		:= ./include/
LIB_INC		:= ./lib/
CORE_INC	:= ./core
HTTP_INC	:= ./http

#
# library
#
LIB = -ldl

CC = gcc

CFLAGS = -pipe -Wall -O2 -g #-pg
CFLAGS += -I$(PUB_INC) -I$(LIB_INC) -I$(CORE_INC) -I$(HTTP_INC)

ifeq ($(shell arch), i686)
	CFLAGS += -march=i686
endif

ifneq ($(shell grep -c ^processor /proc/cpuinfo 2>/dev/null), 1)
	CFLAGS += -DCONFIG_SMP
endif

#
# bin file
#
TARGET = shark

all:: $(TARGET)

${TARGET}:$(obj)
	${CC} -o $@ ${CFLAGS} $^ $(LIB)

%.o: %.c
	${CC} ${CFLAGS} -c -o $@ $<

install:
	@rm -rf ../bin/$(TARGET)
	@cp -f $(TARGET) ../bin

clean:
	@rm -rf ../bin/$(TARGET)
	@rm -rf $(obj) $(TARGET)
