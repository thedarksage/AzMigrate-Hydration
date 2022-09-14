# ----------------------------------------------------------------------
# configuration
# debug=[yes|no] (default: yes)
# ----------------------------------------------------------------------

ifeq (no,$(debug))
	CFLAGS_MODE := -O3 -DNDEBUG -pthread
	X_CONFIGURATION := release
else
	CFLAGS_MODE := -g3 -O0 -DDEBUG -pthread
	X_CONFIGURATION := debug
endif

X_ARCH := $(shell uname -ms | sed -e s"/ /_/g")

SRC = \
	sqlite3x_command.cpp \
	sqlite3x_connection.cpp \
	sqlite3x_exception.cpp \
	sqlite3x_reader.cpp \
	sqlite3x_transaction.cpp \

OBJS = \
 sqlite3x_command.o \
 sqlite3x_connection.o \
 sqlite3x_exception.o \
 sqlite3x_reader.o \
 sqlite3x_transaction.o \

CFLAGS = \
	-mno-sse2 \
	$(CFLAGS_MODE) \
	-I../../sqlite-3.3.5/sqlite \
	-I../../boost/boost_1_33_0 \
	-pthread

all: clean lib/$(X_ARCH)/$(X_CONFIGURATION)/libsqlite3x.a

%.o:: %.cpp
	g++ -c $(CFLAGS) -o '$@' '$<'

lib/$(X_ARCH)/$(X_CONFIGURATION)/libsqlite3x.a: $(OBJS)
	ar r '$@' $^
	ranlib '$@'

clean:
	rm -f *.o lib/$(X_ARCH)/$(X_CONFIGURATION)/*.a
