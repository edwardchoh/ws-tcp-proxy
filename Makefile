OPT_FAST=-Wall -O3 -I./http-parser -I./libuv/include

LFLAGS := -lm -lpthread -lrt
CC?=gcc

ws_proxy: libuv/uv.a
	$(CC) $(OPT_FAST) -o ws_proxy ws_proxy.c http.c sha1.c http-parser/http_parser.c wsparser.c libuv/uv.a $(LFLAGS)

libuv/uv.a:
	$(MAKE) -C libuv

clean:
	rm -f libuv/uv.a
	rm -f http-parser/http_parser.o
	rm -f ws_proxy
