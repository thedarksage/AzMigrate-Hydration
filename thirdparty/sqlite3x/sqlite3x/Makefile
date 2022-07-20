
ifeq (yes,$(debug))
	CFLAGS_MODE := -g -DDEBUG
	X_CONFIGURATION := debug
else
	CFLAGS_MODE := -O2 -DNDEBUG
	X_CONFIGURATION := release
endif

ifeq (yes,$(use_64bit))
	CFLAGS_MODE += -m64
endif

ifeq (yes,$(use_64bit_aix))
    CFLAGS_MODE += -maix64
endif


ifeq (yes,$(use_nosse2))
	CFLAGS_MODE += -mno-sse2
endif

ifeq (SunOS,$(shell uname))
	CFLAGS_MODE += -pthreads
else
	CFLAGS_MODE += -pthread
endif




SRC = \
	sqlite3x_command.cpp \
	sqlite3x_connection.cpp \
	sqlite3x_exception.cpp \
	sqlite3x_reader.cpp \
	sqlite3x_transaction.cpp \

OBJS = \
 config_$(X_CONFIGURATION)/sqlite3x_command.o \
 config_$(X_CONFIGURATION)/sqlite3x_connection.o \
 config_$(X_CONFIGURATION)/sqlite3x_exception.o \
 config_$(X_CONFIGURATION)/sqlite3x_reader.o \
 config_$(X_CONFIGURATION)/sqlite3x_transaction.o \

CFLAGS = \
	$(CFLAGS_MODE) \
	-I../../sqlite-3.36.0 \
	-I../../boost/boost_1_78_0 \
	-D_LARGEFILE_SOURCE=1 \
	-D_LARGEFILE64_SOURCE=1 \
	-D_FILE_OFFSET_BITS=64 \

all: config_$(X_CONFIGURATION) config_$(X_CONFIGURATION)/libsqlite3x.a

config_$(X_CONFIGURATION):
	mkdir -p config_$(X_CONFIGURATION)

config_$(X_CONFIGURATION)/%.o:: %.cpp
	g++ -c $(CFLAGS) -o '$@' '$<'

config_$(X_CONFIGURATION)/libsqlite3x.a: $(OBJS)
	ar r '$@' $^
	ranlib '$@'

clean: clean_debug clean_release

clean_debug:
	rm -Rf config_debug

clean_release:
	rm -Rf config_release
