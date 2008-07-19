CFLAGS=-Wall -O2

world: atcd/atcd atcc/atcc

include atcd/Makefile.inc
include atcc/Makefile.inc
