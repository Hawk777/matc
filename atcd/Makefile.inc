atcd/atcd: atcd/atcd.o atcd/auth.o atcd/atcproc.o shared/sockpath.o

atcd/atcd.o: atcd/auth.h atcd/atcproc.h atcd/list.h shared/sockpath.h

atcd/auth.o: atcd/auth.h

atcd/atcproc.o: atcd/atcproc.c atcd/atcproc.h
