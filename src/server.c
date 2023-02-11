#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
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
#include <poll.h>
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

volatile sig_atomic_t status;

http_server *create_server(int port, int cache_size, int hashsize, char *root_dir, long max_request_size, long max_response_size){
    http_server *server = (http_server*)malloc(sizeof(*server));
    if (server == NULL){
        fprintf(stderr, "Error while allocating memory to server on port %d\n", port);
        exit(EXIT_FAILURE);
    }
    server->server_logs = (http_server_logs*)malloc(sizeof(http_server_logs));
    if (server->server_logs == NULL){
        fprintf(stderr, "Error while allocating memory to logs for server on port %d\n", port);
        destroy_server(server, 0);       
        exit(EXIT_FAILURE);
    }
    server->port = port;
    
    if (cache_size == 0){
        server->cache = NULL;
    }
    else{
        server->cache = lru_create(cache_size, hashsize);
        if (server->cache == NULL){
            fprintf(stderr, "Error while creating cache for server on port %d\n", port);
            destroy_server(server, 0);
            exit(EXIT_FAILURE);
        }
    }

    server->route_table = route_create();
    if (server->route_table == NULL){
        fprintf(stderr, "Error while creating route tables for server on port %d\n", port);
        exit(EXIT_FAILURE);
    }

    server->max_response_size = max_response_size ? max_response_size : DEFAULT_MAX_RESPONSE_SIZE;
    server->max_request_size = max_request_size ? max_request_size : DEFAULT_MAX_RESPONSE_SIZE;
    server->server_logs->max_cache_size = cache_size;
    server->server_logs->num_bytes_sent = 0;
    server->server_logs->num_get_requests = 0;
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
        destroy_server(server, 0);
        exit(EXIT_FAILURE);
    }
    free(port_string);
    port_string = NULL;
    status = 1;
    return server;
}

void destroy_server(http_server *server, int print_logs){
    if (print_logs)
        print_server_logs(server);
    
    printf("Destroying Server running on port: %d\n",server->port);
    if (server->server_logs)
        free(server->server_logs);
    if (server->cache)
        destroy_cache(server->cache);
    if (server->route_table)
        route_destroy(server->route_table);
    close(server->socket_fd);
    if (server)
        free(server);
    server = NULL;
}   

void print_server_logs(http_server *server){
    fprintf(stdout, "Server Port: %d\n", server->port);
    fprintf(stdout, "Server Cache enabled (Y/n): %c\n", server->cache == NULL? 'n' : 'Y');
    if (server->cache != NULL){
        fprintf(stdout, "Server Cache size: %d\n", server->cache->max_size);
        fprintf(stdout, "Server Cache Hashtable size: %d\n", server->cache->table->size);
        fprintf(stdout, "Server Cache Hashtable load: %.2f\n", server->cache->table->load*100);
    }
    fprintf(stdout, "Server Directory: %s\n", server->server_root_dir);
    fprintf(stdout, "Number of Routes: %d\n", server->route_table->num_routes);
    fprintf(stdout, "Max Request size: %ld\n", server->max_request_size);
    fprintf(stdout, "Max Response size: %ld\n", server->max_response_size);
    fprintf(stdout, "Number of Bytes Sent: %ld\n", server->server_logs->num_bytes_sent);
    fprintf(stdout, "Number of GET Requests Received: %d\n", server->server_logs->num_get_requests);
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

void file_response_handler(http_server *server, int new_socket_fd, const char *path){
    char filepath[4096];
    file_data *filedata;
    char *mime_type;

    snprintf(filepath, sizeof(filepath), "%s", path);
    printf("Loading data from %s\n", filepath);
    filedata = file_load(filepath);

    if (filedata == NULL){
        fprintf(stderr, "File %.*s not found on server!\n",(int)strlen(filepath) ,filepath);
        char body[1024];
        sprintf(body, "File %.*s not found on Server!\n",(int)strlen(filepath) ,filepath);
        send_http_response(server, new_socket_fd, "HTTP/1.1 404 NOT FOUND", "text/html", body);
        return;
    }

    mime_type = mime_type_get(filepath);
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
        free(request);
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
        free(request);
        return;
    }

    printf("%s\n", request);

    // now we have parsed the request obtained. 
    // determine if the path request is a registered path
    char get[] = "GET";
    char post[] = "POST";
    
    char *search_path = (char*)malloc(path_len + 1);
    snprintf(search_path, path_len + 1, "%s" ,path);
    // printf("searching for path: %.*s\n", (int)path_len, path);
    // printf("searching for path: %s\n", search_path);

    if (strstr(search_path, ".") != NULL){
        // if request path is a resource on server and not a route, then respond with a file
        char resource_path[4096];
        sprintf(resource_path, "%s/%s", server->server_root_dir, search_path);
        file_response_handler(server, new_socket_fd, resource_path);
    }
    else{
        route_inorder_traversal(server->route_table);
        route_node *req_route = route_search(server->route_table, search_path);
        if (req_route == NULL){
            fprintf(stderr, "404 Page Not found!\n");
            response_404(server, new_socket_fd);
        }
        else if (req_route->value != NULL){
            printf("Key : %s\n", req_route->key);
            printf("value : %s\n", req_route->value);
            char file_path[4096];
            sprintf(file_path, "%s/%s", server->server_root_dir, req_route->value);
            file_response_handler(server, new_socket_fd, file_path);
        }
        else{
            char dir_path[4096];
            sprintf(dir_path, "%s/%s", server->server_root_dir, req_route->route_dir);
            req_route->route_fn(server, new_socket_fd, dir_path, req_route->fn_args);
        }
    }

    char *search_method = (char*)malloc(method_len+1);
    snprintf(search_method, method_len + 1, "%s" ,method);
    if (strcmp(get, search_method) == 0){
        server->server_logs->num_get_requests += 1;
    }else{
        // do for post
    }

    server->server_logs->num_requests_served += 1;
    free(search_path);
    free(request);
    request = NULL;
    search_path = NULL;
}

void stop_server(){
    status = 0;
}

void server_start(http_server *server){
    int new_socket_fd;
    int port = server->port;
    struct sockaddr_storage client_addr;
    char s[INET6_ADDRSTRLEN];
    
    // set the server listening socket to be non-blocking.
    // int flags_before = fcntl(server->socket_fd, F_GETFL);
    // if (flags_before == -1){
    //     fprintf(stderr, "Could not obtain the flags for server listening TCP socket.\n");
    //     destroy_server(server, 0);
    //     exit(EXIT_FAILURE);
    // }
    // else{
    //     int set_nonblocking = fcntl(server->socket_fd, F_SETFL, flags_before | O_NONBLOCK);
    //     if (set_nonblocking == -1){
    //         fprintf(stderr, "Could not set server TCP socket to NON_BLOCKING.\n");
    //         destroy_server(server, 0);
    //         exit(EXIT_FAILURE);
    //     }
    // }
    
    signal(SIGINT, stop_server);
    
    // fd_set set;
    // struct timeval timeout;
    // int rv;
    // FD_ZERO(&set);
    // FD_SET(server->socket_fd, &set);
    // timeout.tv_sec = 10;
    // timeout.tv_usec = 0;
    
    while(status){
        socklen_t sin_size = sizeof(client_addr);
        
        new_socket_fd = accept(server->socket_fd, (struct sockaddr *)&client_addr, &sin_size);
        if (new_socket_fd == -1){
            if (errno == EWOULDBLOCK){
                if (!status){
                    break;
                }else{
                    continue;
                }
            }
            else{
                fprintf(stderr, "An error occured while accepting a connection.\n");
                continue;
            }
        }

        inet_ntop(client_addr.ss_family, get_internet_address((struct sockaddr *)&client_addr), s, sizeof(s));
        fprintf(stdout, "Server:%d - Got connection from %s\n", port, s);
        handle_http_request(server, new_socket_fd);
        close(new_socket_fd);
    }

    // restore socket to be blocking
    // if (fcntl(server->socket_fd, F_SETFL, flags_before) == -1){
    //     fprintf(stderr, "Failed to gracefully shutdown server.\n");
    //     destroy_server(server, 0);
    //     exit(EXIT_FAILURE);
    // }

    fprintf(stdout, "Stopping server running on port %d...\n", port);
    destroy_server(server, 1);
    fprintf(stdout, "Server:%d - Stopped.\n", port);
    exit(EXIT_SUCCESS);
}
