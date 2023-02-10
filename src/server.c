#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/file.h>
#include <fcntl.h>
#include "net.h"
#include "files.h"
#include "mime.h"
#include "lru.h"
#include "server.h"
#include "picohttpparser.h"

#define DEFAULT_PORT "8080"
#define DEFAULT_MAX_RESPONSE_SIZE 64 * 1024 * 1024 // 64 MB
#define DEFAULT_SERVER_FILE_PATH "./serverfiles"
#define DEFAULT_SERVER_ROOT "./serverroot"

http_server *create_server(int port, int cache_size, int hashsize, char *root_dir, long max_request_size, long max_response_size){
    http_server *server = (http_server*)malloc(sizeof(*server));
    if (server == NULL){
        fprintf(stderr, "Error while allocating memory to server on port %d\n", port);
        exit(1);
    }
    server->server_logs = (http_server_logs*)malloc(sizeof(server->server_logs));
    if (server->server_logs == NULL){
        fprintf(stderr, "Error while allocating memory to logs for server on port %d\n", port);
        exit(1);
    }
    server->port = port;
    
    if (cache_size == 0){
        server->cache = NULL;
    }
    else{
        server->cache = lru_create(cache_size, hashsize);
        if (server->cache == NULL){
            fprintf(stderr, "Error while creating cache for server on port %d\n", port);
            exit(1);
        }
    }

    server->route_table = route_create();
    if (server->route_table == NULL){
        fprintf(stderr, "Error while creating route tables for server on port %d\n", port);
        exit(1);
    }

    server->max_response_size = max_response_size ? max_response_size : DEFAULT_MAX_RESPONSE_SIZE;
    server->max_request_size = max_request_size ? max_request_size : DEFAULT_MAX_RESPONSE_SIZE;
    server->server_logs->max_cache_size = cache_size;
    server->server_logs->num_bytes_sent = 0;
    server->server_logs->num_get_requests = 0;
    server->server_logs->num_post_requests = 0;
    server->server_logs->num_requests_served = 0;
    server->server_logs->num_routes = 0;

    if (root_dir!=NULL){
        server->server_root_dir = root_dir;
    }
    else{
        server->server_root_dir = DEFAULT_SERVER_ROOT;
    }

    int port_length = snprintf( NULL, 0, "%d", port );
    char *port_string = malloc(port_length + 1);
    snprintf(port_string, port_length + 1, "%d", port);
    server->socket_fd = get_listener_socket(port_string);
    if (server->socket_fd < 0){
        fprintf(stderr, "Error while binding to port %d\n", port);
        exit(1);
    }
    free(port_string);
    port_string = NULL;
    return server;
}

void destroy_server(http_server *server, int print_logs){
    if (print_logs)
        print_server_logs(server);
    
    printf("Destroying Server running on port: %d\n",server->port);
    free(server->server_logs);
    destroy_cache(server->cache);
    route_destroy(server->route_table);
    close(server->socket_fd);
    free(server);
    server = NULL;
}   

void print_server_logs(http_server *server){
    fprintf(stdout, "Server Port: %d\n", server->port);
    fprintf(stdout, "Server Cache enabled (Y/n): %c\n", server->cache == NULL? 'n' : 'Y');
    if (server->cache != NULL){
        fprintf(stdout, "Server Cache size: %d\n", server->cache->max_size);
        fprintf(stdout, "Server Cache Hashtable size: %d\n", server->cache->table->size);
        fprintf(stdout, "Server Cache Hashtable load: %f\n", server->cache->table->load);
    }
    fprintf(stdout, "Number of Routes: %d\n", server->route_table->num_routes);
    fprintf(stdout, "Number of Bytes Sent: %ld\n", server->server_logs->num_bytes_sent);
    fprintf(stdout, "Number of GET Requests Received: %d\n", server->server_logs->num_get_requests);
    fprintf(stdout, "Number of POST Requests Received: %d\n", server->server_logs->num_post_requests);
    fprintf(stdout, "Number of Requests Served: %d\n", server->server_logs->num_requests_served);
}

int send_http_response(http_server *server, int new_socket_fd, char *header, char *content_type, char *body){
    const long max_response_size = server->max_response_size;
    char *response = (char*)malloc(sizeof(char)*max_response_size);
    long response_length; 
    long content_length = strlen(body);
    sprintf(
        response,
        "%s\n"
        "Content-Length: %ld\n"
        "Content-Type: %s\n"
        "Connection: close\n"
        "\n"
        "%s",
        header, content_length, content_type, body
    );
    response_length = strlen(response);
    int rv = send(
        new_socket_fd,
        response,
        response_length,
        0
    );
    if (rv < 0){
        perror("Could not send response.");
    }
    // clean up response buffers
    free(response);
    server->server_logs->num_bytes_sent += (long)rv;
    return rv;
}

void response_404(http_server *server, int new_socket_fd){
    char body[] = "<h1>404 Page Not Found</h1>";
    char header[] = "HTTP/1.1 404 NOT FOUND";
    char content_type[] = "text/html";
    int bytes_sent = send_http_response(
        server, new_socket_fd, header, content_type, body
    );
}

void response_file(http_server *server, int new_socket_fd, const char *path){
    char filepath[4096];
    file_data *filedata;
    char *mime_type;

    snprintf(filepath, sizeof(filepath), "%s/%s", server->server_root_dir, path);
    filedata = file_load(filepath);

    if (filedata == NULL){
        fprintf(stderr, "File %.*s not found on server!\n",(int)strlen(filepath) ,filepath);
        char body[1024];
        sprintf(body, "File %.*s not found on Server!\n",(int)strlen(filepath) ,filepath);
        send_http_response(server, new_socket_fd, "HTTP/1.1 404 NOT FOUND", "text/html", body);
        return;
    }

    mime_type = mime_type_get(filepath);
    char header[] = "HTTP/1.1 404 NOT FOUND";
    char content_type[] = "text/html";
    send_http_response(server, new_socket_fd, "HTTP/1.1 200 OK", mime_type, filedata->data);
    file_free(filedata);
    filedata = NULL;
    return;
}

void handle_http_request(http_server *server, int new_socket_fd){
    const long request_buffer_size = server->max_request_size;
    char *request = (char*)malloc(request_buffer_size);
    char *p;

    int bytes_received = recv(new_socket_fd, request, request_buffer_size - 1, 0);

    if (bytes_received < 0){
        fprintf(stderr, "Did not receive any bytes in the request.\n");
        return;
    }
    else{
        server->server_logs->num_bytes_received += bytes_received;
    }

    request[bytes_received] = '\0';
    char *request_body = strstr(request, "\r\n\r\n");
    
    if (request_body != NULL){
        request_body += 4; // shift ptr by 4
    }
    
    const char *method, *path;
    int pret, minor_version;
    struct phr_header headers[100];
    size_t buflen = 0, method_len, path_len, num_headers;

    num_headers = sizeof(headers) / sizeof(headers[0]);
    pret = phr_parse_request(
        request, bytes_received, &method, &method_len, &path, &path_len, 
        &minor_version, headers, &num_headers, 0
    );
    if (pret == -1){
        fprintf(stderr, "Could not parse the headers in the request.\n");
        return;
    }
    if (bytes_received >= sizeof(request)){
        fprintf(stderr, "Request execeeds MAX_REQUEST_SIZE=%ld bytes.\n", server->max_request_size);
        return;
    }

    // now we have parsed the request obtained. 
    // determine if the path request is a registered path
    char get[] = "GET";
    char post[] = "POST";

    route_node *req_route = route_search(server->route_table, path);
    if (req_route == NULL){
        response_404(server, new_socket_fd);
    }
    else{
        response_file(server, new_socket_fd, path);
    }
}