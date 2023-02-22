#ifndef _SERVER_H_
#define _SERVER_H_

#include "lru.h"
#include "routes.h"

#define HEADER_OK "HTTP/1.1 200 OK"
#define HEADER_404 "HTTP/1.1 404 NOT FOUND"

#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct http_server_logs
    {
        int num_requests_served;
        int num_get_requests;
        int num_routes;
        long num_bytes_sent;
        long num_bytes_received;
        int max_cache_size;
        int cache_hits;
        int cache_miss;
    } http_server_logs;

    typedef struct http_server
    {
        int socket_fd;          // listening socket for server
        int port;               // port number for the server
        lru *cache;             // cache ptr to LRU cache
        route_map *route_table; // store list of supported routes
        http_server_logs *server_logs;
        char *server_root_dir;
        long max_response_size;
        long max_request_size;
        int backlog;
        pthread_mutex_t lock;
    } http_server;

    http_server *create_server(int port, int cache_size, int hashsize, char *root_dir, long max_request_size, long max_response_size, int backlog);
    void server_start(http_server *server, int close_server, int print_logs);
    void destroy_server(http_server *server, int print_logs);
    void print_server_logs(http_server *server);

    void server_route(http_server *server, const char *key, const char *value, char **methods, size_t method_len, const char *route_dir, void (*route_fn)(void *, int, const char *, void *), void *fn_args);
    void *handle_http_request(void *arg);
    int send_http_response(http_server *server, int new_socket_fd, char *header, char *content_type, char *body, size_t content_length);
    int file_response_handler(http_server *server, int new_socket_fd, char *path);
    cache_node *server_cache_retreive(http_server *server, char *key);
    void server_cache_resource(http_server *server, char *key, char *content_type, void *data, size_t content_length);
#ifdef __cplusplus
}
#endif

#endif