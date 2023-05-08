#
# We use the inm_dmit command line option parser
#
all: copy_files notify_sys_shutdown.o inm_cmd_parser.o
	gcc -o inmshutnotify *.o

copy_files:
	cp ../../../linux/user_space/inmdmit/inm_cmd_parser.* .

%.o: 
	gcc -D_GNU_SOURCE -g -I. -c -o $@ $(@:.o=.c)

clean:
	rm -f inmshutnotify inm_cmd_parser.*
	rm -f *.o
	
	
