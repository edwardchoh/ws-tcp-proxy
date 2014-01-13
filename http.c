//
//  http.c
//  libuv-ws
//
//  Created by Edward Choh on 1/12/2014.
//  Copyright (c) 2014 Edward Choh. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include "http_parser.h"
#include "ws_proxy.h"
#include "sha1.h"

static char r101[] = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept:                             \r\n\r\n";

static void print_request(request* req) {
#ifdef DEBUG
    fprintf(stderr, "---------------------------------------------------\n");
    fprintf(stderr, "id: %llu\n", req->id);
    fprintf(stderr, "method: %i\n", req->method);
    fprintf(stderr, "major: %i\n", req->http_major);
    fprintf(stderr, "minor: %i\n", req->http_minor);
    fprintf(stderr, "url: %s\n", req->url);
    fprintf(stderr, "upgrade: %i\n", req->upgrade);
    fprintf(stderr, "keepalive: %i\n", req->keepalive);
    fprintf(stderr, "wskey: %s\n", req->wskey);
    fprintf(stderr, "handshake: %i\n", req->handshake);
    fprintf(stderr, "headers: %i\n", req->num_headers);
    int i=0;
    for(i=0; i<req->num_headers; i++) {
        fprintf(stderr, "\tname: %s\n", req->headers[i][0]);
        fprintf(stderr, "\tvalue: %s\n", req->headers[i][1]);
    }
    fprintf(stderr, "---------------------------------------------------\n");
#endif
}

int message_begin_cb (http_parser *p) {
    _context* ctx = p->data;
    int i=0;
    for(i=0; i < MAX_HEADERS; i++) {
        ctx->request->headers[i][0][0] = 0;
        ctx->request->headers[i][1][0] = 0;
    }
    ctx->request->id++;
    ctx->request->num_headers = 0;
    ctx->request->last_header_element = NONE;
    ctx->request->upgrade = 0;
    ctx->request->handshake = 0;
    ctx->request->keepalive = 1;
    ctx->request->url[0] = 0;
    return 0;
}

int url_cb (http_parser *p, const char *buf, size_t len) {
    _context* ctx = p->data;
    strncat(ctx->request->url, buf, len);
    return 0;
}

int header_field_cb (http_parser *p, const char *buf, size_t len) {
    _context* ctx = p->data;
    request* req = ctx->request;
    if(req->last_header_element != FIELD) {
        req->num_headers++;
    }
    strncat(req->headers[req->num_headers-1][0], buf, len);
    req->last_header_element = FIELD;
    return 0;
}

int header_value_cb (http_parser *p, const char *buf, size_t len) {
    _context* ctx = p->data;
    request* req = ctx->request;
    strncat(req->headers[req->num_headers-1][1], buf, len);
    req->last_header_element = VALUE;
    return 0;
}

int headers_complete_cb (http_parser *p) {
    _context* ctx = p->data;
    request* req = ctx->request;
    
    req->keepalive = http_should_keep_alive(p);
    req->http_major = p->http_major;
    req->http_minor = p->http_minor;
    req->method = p->method;
    req->upgrade = p->upgrade;
    return 0;
}

int message_complete_cb (http_parser *p) {
    _context* ctx = p->data;
    request* req = ctx->request;
    int i;
    for(i=0; i<req->num_headers; i++) {
        if(strncasecmp(req->headers[i][0], "Sec-WebSocket-Key", 17) == 0) {
            strncpy(req->wskey, req->headers[i][1], 24);
            break;
        }
    }
    shacalc(req->wskey, r101 + 97);
    if (ctx->ws_handshake_complete_cb) {
        (*ctx->ws_handshake_complete_cb)(ctx, r101, (unsigned int)strlen(r101));
    }
    print_request(ctx->request);
    req->handshake = 1;
    free(ctx->parser);
    ctx->parser = NULL;
    return 0;
}

http_parser_settings settings =
{
    .on_message_begin = message_begin_cb,
    .on_header_field = header_field_cb,
    .on_header_value = header_value_cb,
    .on_url = url_cb,
    .on_headers_complete = headers_complete_cb,
    .on_message_complete = message_complete_cb
};