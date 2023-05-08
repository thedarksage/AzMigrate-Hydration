CC=cc

#DEBUGFLAGS = -Wall -g3 -gdwarf-2 -O0  -DDEBUG_MMAPPED_DATA
#DEBUGFLAGS = -Wall -g3 -gdwarf-2 -O0 
#OPTFLAGS= -O2 
CFLAGS  = -DINM_LINUX -Wwrite-strings -Wall -D_GNU_SOURCE -D_LARGEFILE_SOURCE \
		-D_FILE_OFFSET_BITS=64 -DDB_NOTIFY $(OPTFLAGS) $(DEBUGFLAGS)
INM_REPLICATOR_OBJS=inm_main.o \
			inm_subr.o 	\
			inm_subr_v1.o	\
			inm_subr_v2.o	\
			inm_subr_v3.o	\
			inm_cmd_parser.o

#PROGS = inm_replicator
PROGS = inm_dmit

#	QUIET_CC        = @echo '   ' CC $@;
#	QUIET_DEP       = @echo '   ' DEP $@;


all: ${PROGS}

#all:${INM_REPLICATOR_OBJS}
#	${CC} -o inm_replicator ${INM_REPLICATOR_OBJS}

%.o: %.c
	${QUIET_CC} ${CC} -I../../../../../inmsafecapis/unix/ -o $*.o -c ${CFLAGS} $<

#.c.o:
#	$(CC) -c ${CFLAGS} -I. $< -o $@

${PROGS}: ${INM_REPLICATOR_OBJS}
	${QUIET_CC} ${CC} -o $@ $(filter %.o,$^)
	
depend:

all: ${PROGS}
	@rm -f *.o > /dev/null


clean:
	rm -f inm_dmit
	rm -f *.o core.*
	rm -f tags cscope.out
