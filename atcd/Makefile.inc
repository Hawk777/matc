atcd/atcd: atcd/atcd.o atcd/auth.o atcd/atcproc.o shared/sockpath.o

atcd/atcd.o: atcd/auth.h atcd/atcproc.h shared/sockpath.h shared/sockaddr_union.h

atcd/auth.o: atcd/auth.h

atcd/atcproc.o: atcd/atcproc.c atcd/atcproc.h
