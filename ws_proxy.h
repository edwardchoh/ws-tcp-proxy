//
//  websock.h
//  libuv-ws
//
//  Created by Edward Choh on 1/12/2014.
//  Copyright (c) 2014 Edward Choh. All rights reserved.
//

#ifndef libuv_ws_websock_h
#define libuv_ws_websock_h

#include <uv.h>
#include "wsparser.h"
#include "http_parser.h"

#define MAX_HEADERS 20
#define MAX_ELEMENT_SIZE 500
#define MAX_URL_SIZE 255
#define BACKLOG 2048

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

typedef struct request request;
struct request {
    uint64_t id;
    char url[MAX_URL_SIZE];
    enum http_method method;
    unsigned short http_major;
    unsigned short http_minor;
    int num_headers;
    enum { NONE=0, FIELD, VALUE } last_header_element;
    char headers [MAX_HEADERS][2][MAX_ELEMENT_SIZE];
    char wskey[61];
    uint8_t upgrade;
    uint8_t handshake;
    uint8_t keepalive;
};


typedef struct _context _context;
typedef void (*ws_handshake_complete_cb_t)(_context *ctx, char *buf, int len);

struct _context {
    int fd;
    http_parser *parser;
    ws_parser *wsparser;
    request *request;
    uv_stream_t *local, *remote;
    ws_handshake_complete_cb_t ws_handshake_complete_cb;
    uv_buf_t pending_response;
};

void print_ws_header(ws_header* header);
void xxdprint(const char *buff, uint64_t offset, uint64_t size);

#endif
