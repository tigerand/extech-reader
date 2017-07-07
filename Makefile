#
# Makefile for extech reader program
#
# Copyright 2017, Low Power Company, Inc.
# Copyright 2017, Andrew Sharp

CFLAGS+=-O2
CC=gcc

.c.o:
	$(CC) $(CFLAGS) -c $^

MAIN=extech_rdr

OBJS=\
	extech.o		\
	measurement.o	\
	$(MAIN).o

$(MAIN): $(OBJS)
	$(CC) $(OBJS) -lpthread -o $(MAIN)
