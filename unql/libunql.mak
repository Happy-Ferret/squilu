#------------------------------------------------------------------------------#
# This makefile was generated by 'cbp2make' tool rev.135                       #
#------------------------------------------------------------------------------#


WORKDIR = `pwd`

CC = gcc
CXX = g++
AR = ar
LD = g++
WINDRES = windres

INC =  -Isrc -I/home/mingo/dev/dadbiz++/third-party/unql/
CFLAGS =  -DTHREADSAFE=1 -DSQLITE_DEFAULT_FILE_FORMAT=4 -DSQLITE_DEFAULT_AUTOVACUUM=1 -DSQLITE_DEFAULT_FOREIGN_KEYS=1 -DSQLITE_ENABLE_COLUMN_METADATA=1 -DSQLITE_ENABLE_EXTENSION_FUNCTIONS=1 -DSQLITE_ENABLE_FTS4=1 -DSQLITE_ENABLE_FTS3_PARENTHESIS=1 -DSQLITE_ENABLE_UNLOCK_NOTIFY=1 -DSQLITE_ENABLE_RTREE=1 -DSQLITE_ENABLE_STAT4=1 -DSQLITE_HAS_CODEC=1 -DSQLITE_OMIT_TCL_VARIABLE=1 -DSQLITE_USE_URI=1 -DSQLITE_SOUNDEX=1
RESINC = 
LIBDIR = 
LIB = 
LDFLAGS = 

INC_DEBUG =  $(INC)
CFLAGS_DEBUG =  $(CFLAGS) -Wall -g
RESINC_DEBUG =  $(RESINC)
RCFLAGS_DEBUG =  $(RCFLAGS)
LIBDIR_DEBUG =  $(LIBDIR)
LIB_DEBUG = $(LIB)
LDFLAGS_DEBUG =  $(LDFLAGS)
OBJDIR_DEBUG = obj/Debug
DEP_DEBUG = 
OUT_DEBUG = unql.a

INC_RELEASE =  $(INC)
CFLAGS_RELEASE =  $(CFLAGS) -O2 -Wall
RESINC_RELEASE =  $(RESINC)
RCFLAGS_RELEASE =  $(RCFLAGS)
LIBDIR_RELEASE =  $(LIBDIR)
LIB_RELEASE = $(LIB)
LDFLAGS_RELEASE =  $(LDFLAGS) -s
OBJDIR_RELEASE = obj/Release
DEP_RELEASE = 
OUT_RELEASE = unql.a

OBJ_DEBUG = $(OBJDIR_DEBUG)/src/update.o $(OBJDIR_DEBUG)/src/trace.o $(OBJDIR_DEBUG)/src/tokenize.o $(OBJDIR_DEBUG)/src/string.o $(OBJDIR_DEBUG)/src/stmt.o $(OBJDIR_DEBUG)/src/query.o $(OBJDIR_DEBUG)/src/pragma.o $(OBJDIR_DEBUG)/src/complete.o $(OBJDIR_DEBUG)/src/parse.o $(OBJDIR_DEBUG)/src/memory.o $(OBJDIR_DEBUG)/src/json.o $(OBJDIR_DEBUG)/src/func.o $(OBJDIR_DEBUG)/src/expr.o $(OBJDIR_DEBUG)/src/delete.o $(OBJDIR_DEBUG)/src/datasrc.o $(OBJDIR_DEBUG)/src/context.o $(OBJDIR_DEBUG)/src/conn.o

OBJ_RELEASE = $(OBJDIR_RELEASE)/src/update.o $(OBJDIR_RELEASE)/src/trace.o $(OBJDIR_RELEASE)/src/tokenize.o $(OBJDIR_RELEASE)/src/string.o $(OBJDIR_RELEASE)/src/stmt.o $(OBJDIR_RELEASE)/src/query.o $(OBJDIR_RELEASE)/src/pragma.o $(OBJDIR_RELEASE)/src/complete.o $(OBJDIR_RELEASE)/src/parse.o $(OBJDIR_RELEASE)/src/memory.o $(OBJDIR_RELEASE)/src/json.o $(OBJDIR_RELEASE)/src/func.o $(OBJDIR_RELEASE)/src/expr.o $(OBJDIR_RELEASE)/src/delete.o $(OBJDIR_RELEASE)/src/datasrc.o $(OBJDIR_RELEASE)/src/context.o $(OBJDIR_RELEASE)/src/conn.o

all: debug release

clean: clean_debug clean_release

before_debug: 
	test -d $(OBJDIR_DEBUG)/src || mkdir -p $(OBJDIR_DEBUG)/src

after_debug: 

debug: before_debug out_debug after_debug

out_debug: $(OBJ_DEBUG) $(DEP_DEBUG)
	$(AR) rcs $(OUT_DEBUG) $(OBJ_DEBUG)

$(OBJDIR_DEBUG)/src/update.o: src/update.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/update.c -o $(OBJDIR_DEBUG)/src/update.o

$(OBJDIR_DEBUG)/src/trace.o: src/trace.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/trace.c -o $(OBJDIR_DEBUG)/src/trace.o

$(OBJDIR_DEBUG)/src/tokenize.o: src/tokenize.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/tokenize.c -o $(OBJDIR_DEBUG)/src/tokenize.o

$(OBJDIR_DEBUG)/src/string.o: src/string.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/string.c -o $(OBJDIR_DEBUG)/src/string.o

$(OBJDIR_DEBUG)/src/stmt.o: src/stmt.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/stmt.c -o $(OBJDIR_DEBUG)/src/stmt.o

$(OBJDIR_DEBUG)/src/query.o: src/query.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/query.c -o $(OBJDIR_DEBUG)/src/query.o

$(OBJDIR_DEBUG)/src/pragma.o: src/pragma.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/pragma.c -o $(OBJDIR_DEBUG)/src/pragma.o

$(OBJDIR_DEBUG)/src/complete.o: src/complete.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/complete.c -o $(OBJDIR_DEBUG)/src/complete.o

$(OBJDIR_DEBUG)/src/parse.o: src/parse.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/parse.c -o $(OBJDIR_DEBUG)/src/parse.o

$(OBJDIR_DEBUG)/src/memory.o: src/memory.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/memory.c -o $(OBJDIR_DEBUG)/src/memory.o

$(OBJDIR_DEBUG)/src/json.o: src/json.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/json.c -o $(OBJDIR_DEBUG)/src/json.o

$(OBJDIR_DEBUG)/src/func.o: src/func.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/func.c -o $(OBJDIR_DEBUG)/src/func.o

$(OBJDIR_DEBUG)/src/expr.o: src/expr.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/expr.c -o $(OBJDIR_DEBUG)/src/expr.o

$(OBJDIR_DEBUG)/src/delete.o: src/delete.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/delete.c -o $(OBJDIR_DEBUG)/src/delete.o

$(OBJDIR_DEBUG)/src/datasrc.o: src/datasrc.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/datasrc.c -o $(OBJDIR_DEBUG)/src/datasrc.o

$(OBJDIR_DEBUG)/src/context.o: src/context.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/context.c -o $(OBJDIR_DEBUG)/src/context.o

$(OBJDIR_DEBUG)/src/conn.o: src/conn.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/conn.c -o $(OBJDIR_DEBUG)/src/conn.o

clean_debug: 
	rm -f $(OBJ_DEBUG) $(OUT_DEBUG)
	rm -rf $(OBJDIR_DEBUG)/src

before_release: 
	test -d $(OBJDIR_RELEASE)/src || mkdir -p $(OBJDIR_RELEASE)/src

after_release: 

release: before_release out_release after_release

out_release: $(OBJ_RELEASE) $(DEP_RELEASE)
	$(AR) rcs $(OUT_RELEASE) $(OBJ_RELEASE)

$(OBJDIR_RELEASE)/src/update.o: src/update.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/update.c -o $(OBJDIR_RELEASE)/src/update.o

$(OBJDIR_RELEASE)/src/trace.o: src/trace.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/trace.c -o $(OBJDIR_RELEASE)/src/trace.o

$(OBJDIR_RELEASE)/src/tokenize.o: src/tokenize.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/tokenize.c -o $(OBJDIR_RELEASE)/src/tokenize.o

$(OBJDIR_RELEASE)/src/string.o: src/string.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/string.c -o $(OBJDIR_RELEASE)/src/string.o

$(OBJDIR_RELEASE)/src/stmt.o: src/stmt.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/stmt.c -o $(OBJDIR_RELEASE)/src/stmt.o

$(OBJDIR_RELEASE)/src/query.o: src/query.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/query.c -o $(OBJDIR_RELEASE)/src/query.o

$(OBJDIR_RELEASE)/src/pragma.o: src/pragma.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/pragma.c -o $(OBJDIR_RELEASE)/src/pragma.o

$(OBJDIR_RELEASE)/src/complete.o: src/complete.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/complete.c -o $(OBJDIR_RELEASE)/src/complete.o

$(OBJDIR_RELEASE)/src/parse.o: src/parse.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/parse.c -o $(OBJDIR_RELEASE)/src/parse.o

$(OBJDIR_RELEASE)/src/memory.o: src/memory.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/memory.c -o $(OBJDIR_RELEASE)/src/memory.o

$(OBJDIR_RELEASE)/src/json.o: src/json.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/json.c -o $(OBJDIR_RELEASE)/src/json.o

$(OBJDIR_RELEASE)/src/func.o: src/func.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/func.c -o $(OBJDIR_RELEASE)/src/func.o

$(OBJDIR_RELEASE)/src/expr.o: src/expr.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/expr.c -o $(OBJDIR_RELEASE)/src/expr.o

$(OBJDIR_RELEASE)/src/delete.o: src/delete.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/delete.c -o $(OBJDIR_RELEASE)/src/delete.o

$(OBJDIR_RELEASE)/src/datasrc.o: src/datasrc.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/datasrc.c -o $(OBJDIR_RELEASE)/src/datasrc.o

$(OBJDIR_RELEASE)/src/context.o: src/context.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/context.c -o $(OBJDIR_RELEASE)/src/context.o

$(OBJDIR_RELEASE)/src/conn.o: src/conn.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/conn.c -o $(OBJDIR_RELEASE)/src/conn.o

clean_release: 
	rm -f $(OBJ_RELEASE) $(OUT_RELEASE)
	rm -rf $(OBJDIR_RELEASE)/src

.PHONY: before_debug after_debug clean_debug before_release after_release clean_release

