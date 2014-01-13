OPT_FAST=-Wall -O3 -I./http-parser

LFLAGS := -lm -lpthread -luv
CC?=gcc

websock: 
	$(CC) $(OPT_FAST) -o ws_proxy ws_proxy.c http.c sha1.c http-parser/http_parser.c wsparser.c $(LFLAGS)


clean:
	rm -f http-parser/http_parser.o
	rm -f ws_proxy
