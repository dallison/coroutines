
CC = clang
CFLAGS = -g
OBJS = coroutines/coroutine.o coroutines/vector.o coroutines/bitset.o coroutines/list.o coroutines/map.o coroutines/buffer.o coroutines/dstring.o

LIB = libco.so
EXE = co

default: $(EXE)

$(LIB): $(OBJS)
	$(CC) -o $(LIB) $(OBJS) -shared


$(EXE) : $(LIB) coroutines/main.o
	$(CC) -o $(EXE) coroutines/main.o $(LIB)

clean:
	$(RM) -f $(OBJS) $(LIB) $(EXE)
