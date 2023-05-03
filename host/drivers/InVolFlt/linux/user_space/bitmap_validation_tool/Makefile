all:
	cc -g -c main.c -I.
	cc -g -c bitmap_operations.c -I.
	cc -g -o bmapt main.o bitmap_operations.o -I.
	rm -f *.o

clean:
	rm -f *.o bmapt
