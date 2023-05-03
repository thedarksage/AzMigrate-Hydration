all:    main.o WalTest.o verifier.o
	cc main.o WalTest.o verifier.o -o WalTest -lpthread

main.o: main.c WalTest.h verifier.h safecapismajor.h
	cc -I . -c main.c

WalTest.o: WalTest.c WalTest.h svdparse.h inmtypes.h verifier.h safecapismajor.h
	cc -I . -c -w WalTest.c 

verifier.o: verifier.c WalTest.h verifier.h
	cc -I . -c -w verifier.c
clean:
	rm -rf *.o
	rm WalTest
