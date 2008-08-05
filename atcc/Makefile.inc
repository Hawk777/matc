atcc/atcc: atcc/atcc.o atcc/commands.o shared/sockpath.o -lncurses

atcc/atcc.o: atcc/commands.h shared/sockpath.h shared/sockaddr_union.h

atcc/commands.o: atcc/commands.h
