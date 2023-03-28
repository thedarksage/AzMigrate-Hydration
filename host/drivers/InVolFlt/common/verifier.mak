all:
	cc -g -c verifier_user.c -I.
	cc -g -c verifier.c -I.
	cc -g -o verifier verifier_user.o verifier.o -I.
	rm -f *.o

clean:
	rm -f *.o verifier
