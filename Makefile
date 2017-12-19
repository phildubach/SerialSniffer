CC=gcc
CFLAGS=-g
CFLAGS+=$(shell pkg-config --cflags libftdi1)

.PHONY: all
all: ssniff ftdisniff

ssniff: ssniff.c

ftdisniff.o: ftdisniff.c

ftdisniff: ftdisniff.o
	gcc -o $@ $< $(shell pkg-config --libs libftdi1)
