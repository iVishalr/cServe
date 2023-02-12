#ifndef _SERVER_H_
#define _SERVER_H_

#include "lru.h"
#include "routes.h"
#include <signal.h>

#define HEADER_OK "HTTP/1.1 200 OK"
#define HEADER_404 "HTTP/1.1 404 NOT FOUND"

typedef struct http_server_logs{
    int num_requests_served;
    int num_get_requests;
    int num_routes;
    long num_bytes_sent;
    long num_bytes_received;
    int max_cache_size;
} http_server_logs;

typedef struct http_server{
    int socket_fd; //listening socket for server
    int port; // port number for the server
    lru *cache; // cache ptr to LRU cache
    route_map *route_table; // store list of supported routes 
    http_server_logs *server_logs;
    char *server_root_dir;
    long max_response_size;
    long max_request_size;
    int backlog;
} http_server;

extern http_server *create_server(int port, int cache_size, int hashsize, char *root_dir, long max_request_size, long max_response_size, int backlog);
extern void destroy_server(http_server *server, int print_logs);
extern void print_server_logs(http_server *server);
extern void handle_http_request(http_server *server, int new_socket_fd);
extern int send_http_response(http_server *server, int new_socket_fd, char *header, char *content_type, char *body);
extern void file_response_handler(http_server *server, int new_socket_fd, char *path);
extern void server_start(http_server *server, int close_server, int print_logs);
#endif