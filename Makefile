
CC = clang
CFLAGS = -g -Icoroutines
LIB_OBJS = coroutines/coroutine.o coroutines/vector.o coroutines/bitset.o coroutines/list.o coroutines/map.o coroutines/buffer.o coroutines/dstring.o

STATIC_LIB = libco.a
DYNAMIC_LIB = libco.so
TEST = test
HTTP_SERVER = http_server
HTTP_CLIENT = http_client

TEST_OBJS = coroutines/main.o
HTTP_SERVER_OBJS = http/main.o
HTTP_CLIENT_OBJS = client/main.o

default: $(TEST) $(HTTP_SERVER) $(HTTP_CLIENT) $(DYNAMIC_LIB)

$(STATIC_LIB): $(LIB_OBJS)
	$(AR) ruv $(STATIC_LIB) $(LIB_OBJS)

$(DYNAMIC_LIB): $(LIB_OBJS)
	$(CC) -o $(DYNAMIC_LIB) $(LIB_OBJS) -shared


$(TEST) : $(STATIC_LIB) $(TEST_OBJS)
	$(CC) -o $(TEST) $(TEST_OBJS) $(STATIC_LIB)

$(HTTP_SERVER) : $(STATIC_LIB) $(HTTP_SERVER_OBJS)
	$(CC) -o $(HTTP_SERVER) $(HTTP_SERVER_OBJS) $(STATIC_LIB)

$(HTTP_CLIENT) : $(STATIC_LIB) $(HTTP_CLIENT_OBJS) 
	$(CC) -o $(HTTP_CLIENT) $(HTTP_CLIENT_OBJS) $(STATIC_LIB)

clean:
	$(RM) -f $(LIB_OBJS) $(STATIC_LIB) $(DYNAMIC_LIB) $(TEST) $(HTTP_SERVER) $(HTTP_CLIENT) $(TEST_OBJS) $(HTTP_SERVER_OBJS) $(HTTP_CLIENT_OBJS)
