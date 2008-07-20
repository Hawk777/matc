CFLAGS=-Wall -O2

world: atcd/atcd atcc/atcc

include atcd/Makefile.inc
include atcc/Makefile.inc
include shared/Makefile.inc

.PHONY: clean
clean:
	-rm -f atcc/*.o atcd/*.o shared/*.o atcc/atcc atcd/atcd
