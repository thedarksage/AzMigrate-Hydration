all:    main.o WalTest.o verifier.o
	cc -lpthread main.o WalTest.o verifier.o -o WalTest

main.o: main.c WalTest.h verifier.h
	cc -I . -c -g -DEBUG_LOG main.c

WalTest.o: WalTest.c WalTest.h svdparse.h inmtypes.h verifier.h
	cc -I .  -c -g -DEBUG_LOG -w WalTest.c 

verifier.o: verifier.c WalTest.h verifier.h
	cc -I .  -c -g -DEBUG_LOG -w verifier.c
clean:
	rm -rf *.o
	rm WalTest
