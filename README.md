Websocket to TCP Proxy
====

This is a simple Websocket to TCP proxy, built on libuv.

Building
-----

    git clone git@github.com:edwardchoh/ws-tcp-proxy.git
    cd ws-tcp-proxy
    make -j 4

    On Mac, use MacOSX/ws-tcp-proxy.xcodeproj

Running
-----

The server accepts the following arguments:

    ./ws-tcp-proxy --local 0.0.0.0:8080 --remote 127.0.0.1:5000

Features
-----

  * supports parsing of all websocket control and data frames from a stream
  * evented io handled by libuv
  * does not support HTTP beyond what is required for performing a websocket
    handshake
  * builds on mac
  * at startup only consumes 600KB of RAM on linux
  * very small overhead per connection (need to measure)
  * handshake performed using joyent http-parser which is very low overhead
    and fast
  * only supports http://tools.ietf.org/html/rfc6455. is not backwards 
    compatible
  * proxies websocket to a remote host/port
  * does not do any utf8 decoding for text frames. chunks of frame payloads
    are returned as pointers to char buffers whether they are binary or text
  
Future
-----

  * support pausing of parser
  * plan to add SSL support
  * !!!!handle overflows on headers and header sizes!!!!
  * allow user definable limits on headers and header sizes
  * allow configuration of tcp backlog, nodelay, keepalive, connection
    limits, bandwidth throttling, 
  * daemonization
  * websocket extension support
  * older websocket version support (include base64)
  * higher level api to deal with control frames and fragmented messages
  * possibly add support for static file serving and http 
    user modules
  * tests
  * benchmarks
  * improve the make file to build and run tests
  * clean and small websocket api for servers and clients
  

Credits
-----

This project is inspired/copied from ws-uv (https://github.com/billywhizz/ws-uv.git)
