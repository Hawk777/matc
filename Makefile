CFLAGS=-Wall -Wextra -march=native -O2 -Wstrict-aliasing=2 -fstrict-aliasing -fomit-frame-pointer -D_GNU_SOURCE

world: atcd/atcd atcc/atcc

include atcd/Makefile.inc
include atcc/Makefile.inc
include shared/Makefile.inc

.PHONY: clean
clean:
	-rm -f atcc/*.o atcd/*.o shared/*.o atcc/atcc atcd/atcd

.PHONY: install
install: atcc/atcc atcd/atcd
	install -m0755 atcc/atcc /usr/local/bin/
	install -m0755 atcd/atcd /usr/local/bin/
