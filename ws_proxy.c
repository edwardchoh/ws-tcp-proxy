//
//  ws_proxy.c
//  libuv-ws
//
//  Created by Edward Choh on 1/13/2014.
//  Copyright (c) 2014 Edward Choh. All rights reserved.
//

#include <stdio.h>
#include <openssl/ssl.h>
#include "ws_proxy.h"
#include "sha1.h"

/* settings */

static struct sockaddr_in local_addr;
static struct sockaddr_in remote_addr;

#ifdef DEBUG
#define DEBUG_PRINT(fmt, args...) fprintf(stderr, fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...) 
#endif

uv_loop_t *loop;
uv_tcp_t server;
static const char* wshash = "                        258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

extern http_parser_settings settings;
static void on_local_close(uv_handle_t* peer);
static void on_remote_close(uv_handle_t* peer);
static void ws_handshake_complete_cb(_context *ctx, char *buf, int len);
static void on_remote_connection(uv_connect_t *req, int status);
static void after_local_write(uv_write_t* req, int status);

int ws_header_cb(ws_parser* p) {
    DEBUG_PRINT("on_header: %llu, fin: %u, op: %u\n", p->index, p->header.fin, p->header.opcode);
    print_ws_header(&p->header);
    if (p->header.opcode == CLOSE) {
        /* close both connections on CLOSE frame*/
        _context *ctx = (_context*)p->data;
        uv_close((uv_handle_t*)ctx->local, on_local_close);
    }
    return 0;
}

int ws_chunk_cb(ws_parser* p, const char* at, size_t len) {
    DEBUG_PRINT("on_chunk: %llu\t%zu\n", p->index, len);
    xxdprint(at, 0, len);

    /* forward to remote */
    _context *ctx = (_context*)p->data;
    write_req_t *wr;
    wr = malloc(sizeof(write_req_t));
    char *b = malloc(len);
    memcpy(b, at, len);
    wr->buf = uv_buf_init(b, (unsigned int)len);
    uv_write(&wr->req, ctx->remote, &wr->buf, 1, after_local_write);

    return 0;
}

int ws_complete_cb(ws_parser* p) {
    DEBUG_PRINT("on_complete: %llu\n", p->index);
    return 0;
}

void ws_write(_context *ctx, char *buf, size_t len, unsigned int opcode) {
    char *header = malloc(sizeof(char) * 4);
    int hdr_len = ws_encode_bin_hdr(buf, len, header, opcode);
    if (hdr_len) {
        write_req_t *wr;
        wr = malloc(sizeof(write_req_t));
        wr->buf = uv_buf_init(header, hdr_len);
        uv_write(&wr->req, ctx->local, &wr->buf, 1, after_local_write);
        wr = malloc(sizeof(write_req_t));
        wr->buf = uv_buf_init(buf, (unsigned int)len);
        uv_write(&wr->req, ctx->local, &wr->buf, 1, after_local_write);
    }
}

static ws_settings wssettings = {
    .on_header = ws_header_cb,
    .on_chunk = ws_chunk_cb,
    .on_complete = ws_complete_cb,
};

void context_init (uv_stream_t* handle) {
    _context* context = malloc(sizeof(_context));
    context->parser = malloc(sizeof(http_parser));
    context->request = malloc(sizeof(request));
    strcpy(context->request->wskey, wshash);
    context->wsparser = NULL;
    context->request->id = 0;
    context->request->handshake = 0;
    context->local = handle;
    handle->data = context;
    http_parser_init(context->parser, HTTP_REQUEST);
    context->ws_handshake_complete_cb = ws_handshake_complete_cb;
    context->parser->data = context;
}

void context_free (uv_handle_t* handle) {
    _context* context = handle->data;
    if(context) {
        free(context->request);
        free(context->parser);
        free(context->wsparser);
        free(context->pending_response.base);
        if (context->remote)
            uv_close((uv_handle_t*)context->remote, on_remote_close);
        free(context);
    }
    free(handle);
}

void ws_handshake_complete_cb(_context *ctx, char *buf, int len) {
    char *b = malloc(len);
    memcpy(b, buf, len);
    ctx->pending_response = uv_buf_init(b, len);
    
    /* connect to remote */
    ctx->remote = malloc(sizeof(uv_tcp_t));
    if (uv_tcp_init(loop, (uv_tcp_t*)ctx->remote)) {
        fprintf(stderr, "Socket creation error: %s\n", uv_err_name(uv_last_error(loop)));
        return;
    }
    ctx->remote->data = ctx;
    
    uv_connect_t *cr = malloc(sizeof(uv_connect_t));
    if (uv_tcp_connect(cr, (uv_tcp_t*)ctx->remote, remote_addr, on_remote_connection)) {
        fprintf(stderr, "Socket creation error: %s\n", uv_err_name(uv_last_error(loop)));
        return;
    }
    
    ctx->wsparser = malloc(sizeof(ws_parser));
    ws_init(ctx->wsparser);
    ctx->wsparser->data = ctx;
    if(!http_should_keep_alive(ctx->parser)) {
        uv_close((uv_handle_t*)ctx->local, on_local_close);
    }
}

uv_buf_t alloc_buffer(uv_handle_t* handle, size_t suggested_size) {
    return uv_buf_init(malloc(suggested_size), (unsigned int)suggested_size);
}

void on_local_close(uv_handle_t* peer) {
    DEBUG_PRINT("local close\n");
    context_free(peer);
}

void on_remote_close(uv_handle_t* peer) {
    /* context does not belong to context, so will not free that here */
    DEBUG_PRINT("remote close\n");
    free(peer);
}

void after_shutdown(uv_shutdown_t* req, int status) {
    uv_close((uv_handle_t*)req->handle, on_local_close);
    free(req);
}

void after_remote_read(uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
    _context *ctx = handle->data;
    if (nread < 0) {
        /* disassociate remote connection from context */
        ctx->remote = NULL;
        uv_close((uv_handle_t*)handle, on_remote_close);
        /* close local as well */
        uv_close((uv_handle_t*)ctx->local, on_local_close);
    } else if (nread == 0) {
        /* disassociate remote connection from context */
        ctx->remote = NULL;
        uv_close((uv_handle_t*)handle, on_remote_close);
        /* close local as well */
        uv_close((uv_handle_t*)ctx->local, on_local_close);
    } else {
        /* forward to local and encode as ws frames */
        ws_write(ctx, buf.base, nread, BIN);
        /* buf.base is now queued for sending, do not remove here */
        return;
    }
    free(buf.base);
    
}

void on_remote_connection(uv_connect_t *req, int status) {
    _context *ctx = req->handle->data;
    if (status == -1) {
        // error connecting to remote, disconnect local */
        fprintf(stderr, "Remote connect error: %s\n", uv_err_name(uv_last_error(loop)));
        uv_close((uv_handle_t*)ctx->local, on_local_close);
        free(req);
        return;
    }
    uv_read_start(req->handle, alloc_buffer, after_remote_read);
    
    /* write the pending_response to local */
    if (ctx->pending_response.base) {
        write_req_t *wr;
        wr = malloc(sizeof(write_req_t));
        wr->buf = ctx->pending_response;
        uv_write(&wr->req, ctx->local, &wr->buf, 1, after_local_write);
        /* pending_response.base now belongs to wr->buf.base */
        ctx->pending_response.base = NULL;
    }
    free(req);
}

void after_local_write(uv_write_t* req, int status) {
    write_req_t* wr;
    if (status == -1) {
        fprintf(stderr, "Socket write error: %s\n", uv_err_name(uv_last_error(loop)));
    }
    wr = (write_req_t*) req;
    free(wr->buf.base);
    free(wr);
}

void after_local_read(uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
    if (nread < 0) {
        uv_close((uv_handle_t*)handle, on_local_close);
    } else if (nread == 0) {
        uv_close((uv_handle_t*)handle, on_local_close);
    } else {
        _context *ctx = handle->data;
        if (ctx->request->handshake == 0) {
            size_t np = http_parser_execute(ctx->parser, &settings, buf.base, nread);
            if(np != nread) {
                uv_shutdown_t* req;
                req = (uv_shutdown_t*) malloc(sizeof *req);
                uv_shutdown(req, handle, after_shutdown);
            }
        } else {
            size_t np = ws_execute(ctx->wsparser, &wssettings, buf.base, 0, nread);
            if(np != nread) {
                uv_shutdown_t* req;
                req = (uv_shutdown_t*) malloc(sizeof *req);
                uv_shutdown(req, handle, after_shutdown);
            }
        }
    }
    free(buf.base);
}

void on_local_connection(uv_stream_t *handle, int status) {
    if (status == -1) {
        fprintf(stderr, "Socket connect error: %s\n", uv_err_name(uv_last_error(loop)));
        return;
    }
    uv_stream_t *stream = malloc(sizeof(uv_tcp_t));
    if (uv_tcp_init(loop, (uv_tcp_t*)stream)) {
        fprintf(stderr, "Socket creation error: %s\n", uv_err_name(uv_last_error(loop)));
        return;
    }
    if (uv_accept(handle, stream) == 0) {
        context_init(stream);
        uv_read_start(stream, alloc_buffer, after_local_read);
    } else {
        uv_close((uv_handle_t*)stream, NULL);
    }
}

int server_start() {
    if (uv_tcp_init(loop, &server)) {
        fprintf(stderr, "Socket creation error: %s\n", uv_err_name(uv_last_error(loop)));
        return 1;
    }
    if (uv_tcp_bind(&server, local_addr)) {
        fprintf(stderr, "Socket bind error: %s\n", uv_err_name(uv_last_error(loop)));
        return 1;
    }
    if (uv_listen((uv_stream_t*)&server, BACKLOG, on_local_connection)) {
        fprintf(stderr, "Socket listen error: %s\n", uv_err_name(uv_last_error(loop)));
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    /* initialize settings */
    local_addr = uv_ip4_addr("0.0.0.0", 8080);
    remote_addr = uv_ip4_addr("192.168.0.144", 1883);

    loop = uv_default_loop();
    if (server_start())
        return 1;
#if UV_VERSION_MINOR <= 6
    uv_run(loop);
#else
    uv_run(loop, UV_RUN_DEFAULT);
#endif
    return 0;
}
