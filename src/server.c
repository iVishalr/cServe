#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
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
#define DEFAULT_BACKLOG 10

volatile sig_atomic_t status;

/* msleep(): Sleep for the requested number of milliseconds. */
int msleep(long msec){
    struct timespec ts;
    int res;
    if (msec < 0){
        errno = EINVAL;
        return -1;
    }
    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;
    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);
    return res;
}

double get_time_difference(struct timespec *start, struct timespec *end){
    double tdiff = (end->tv_sec - start->tv_sec) + 1e-9 * (end->tv_nsec - start->tv_nsec);
    return tdiff;
}

http_server *create_server(int port, int cache_size, int hashsize, char *root_dir, long max_request_size, long max_response_size, int backlog){
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
    server->backlog = backlog ? backlog : DEFAULT_BACKLOG;
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
    server->socket_fd = get_listener_socket(port_string, server->backlog);
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
        fprintf(stdout, "Server Cache #Hits: %d\n", server->server_logs->cache_hits);
        fprintf(stdout, "Server Cache #Miss: %d\n", server->server_logs->cache_miss);
    }
    
    fprintf(stdout, "Server Directory: %s\n", server->server_root_dir);
    fprintf(stdout, "Number of Routes: %d\n", server->route_table->num_routes);
    fprintf(stdout, "Max Request size: %ld\n", server->max_request_size);
    fprintf(stdout, "Max Response size: %ld\n", server->max_response_size);
    fprintf(stdout, "Server Backlog: %d\n", server->backlog);

    long gbs, mbs, kbs, bytes;
    bytes = server->server_logs->num_bytes_sent;
    gbs = (int)(bytes / 1024 / 1024 / 1024);
    bytes -= gbs * 1024 * 1024 * 1024;
    mbs = (int)(bytes / 1024 / 1024);
    bytes -= mbs * 1024 * 1024;
    kbs = (int)(bytes / 1024);
    bytes -= kbs * 1024;

    fprintf(stdout, "Total Bytes Sent: %ld\t", server->server_logs->num_bytes_sent);
    fprintf(stdout, "(%ld GB; %ld MB; %ld KB; %ld B)\n", gbs, mbs, kbs, bytes);
    fprintf(stdout, "Number of GET Requests Received: %d\n", server->server_logs->num_get_requests);
    fprintf(stdout, "Number of Requests Served: %d\n", server->server_logs->num_requests_served);
}

cache_node *server_cache_resource_handler(http_server *server, char *key, char *content_type, void *data, size_t content_length){
    if (key == NULL || data == NULL){
        return NULL;
    }
    if (server->cache == NULL){
        fprintf(stderr, "[Server:%d] Caching is not enabled.\n", server->port);
        return NULL;
    }

    cache_node *node = cache_put(server->cache, key, content_type, data, content_length);
    if (node == NULL){
        fprintf(stderr, "[Server:%d] An error occured while adding the resource to cache.\n", server->port);
        return NULL;
    }
}

cache_node *server_cache_retreive_handler(http_server *server, char *key){
    if (key == NULL) return NULL;
    if (server->cache == NULL){
        fprintf(stderr, "[Server:%d] Caching is not enabled.\n", server->port);
        return NULL;
    }
    cache_node *node = cache_get(server->cache, key);
    if (node == NULL){
        fprintf(stderr, "[Server:%d] Key=%s not present in cache.\n", server->port, key);
        return NULL;
    }
    return node;
}

cache_node *server_cache_manager(http_server *server, int opcode, char *key, char *content_type, void *data, size_t content_length){
    if (server == NULL) return NULL;
    if (server->cache == NULL){
        fprintf(stderr, "[Server:%d] Caching is not enabled.\n", server->port);
        return NULL;
    }
    if (key == NULL){
        fprintf(stderr, "[Server:%d] A key must be provided for resource being cached!\n", server->port);
        return NULL;
    }
    cache_node *node = NULL;
    switch (opcode){
    case 0:
        // add resource to cache
        if (data == NULL){
            fprintf(stderr, "[Server:%d] No data provided for key=%s. Key not added to cache.", server->port, key);
            return NULL;
        }
        if (content_type == NULL){
            //replace with default content type
            content_type = "text/html";
        }
        node = server_cache_resource_handler(server, key, content_type, data, content_length);
        server->server_logs->cache_miss += 1;
        break;
    case 1:
        // retreive item from cache
        node = server_cache_retreive_handler(server, key);
        if (node){
            server->server_logs->cache_hits += 1;
        }
        break;
    default:
        fprintf(stderr, "[Server:%d] Invalid opcode=%d", server->port, opcode);
    }
    return node;
}

void server_cache_resource(http_server *server, char *key, char *content_type, void *data, size_t content_length){
    cache_node *node = server_cache_manager(server, 0, key, content_type, data, content_length);
    if (node == NULL){
        fprintf(stderr, "[Server:%d] An error occured while adding the resource to cache.\n", server->port);
    }
    return;
}

cache_node *server_cache_retreive(http_server *server, char *key){
    cache_node *node = server_cache_manager(server, 1, key, NULL, NULL, 0);
    if (node == NULL){
        fprintf(stderr, "[Server:%d] An error occured while retreiving the resource from cache.\n", server->port);
        return NULL;
    }
    return node;
}

int send_http_response(http_server *server, int new_socket_fd, char *header, char *content_type, char *body, size_t content_length){
    const long max_response_size = server->max_response_size;
    char *response = (char*)malloc(sizeof(char)*max_response_size);
    long response_length; 
    long body_size = content_length;
    sprintf(
        response,
        "%s\n"
        "Content-Length: %ld\n"
        "Content-Type: %s\n"
        "Connection: close\n"
        "\n"
        "%s",
        header, body_size, content_type, body
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

int send_stream_http_response(http_server *server, int new_socket_fd, char *header, char *content_type, int fd, size_t content_length){
    const long max_response_size = server->max_response_size;
    char *response = (char*)malloc(sizeof(char)*max_response_size);
    long response_length; 
    long body_size = content_length;
    
    sprintf(
        response,
        "%s\n"
        "Content-Length: %ld\n"
        "Content-Type: %s\n"
        "Connection: close\n"
        "\n",
        header, body_size, content_type
    );

    response_length = strlen(response);
    
    long rv = write(
        new_socket_fd,
        response,
        response_length
    );

    int loc = lseek(fd, 0, SEEK_SET);
    long img_bytes = sendfile(new_socket_fd, fd, NULL, content_length);

    if (rv < 0){
        perror("Could not send response.");
    }
    // clean up response buffers
    free(response);
    server->server_logs->num_bytes_sent += rv + img_bytes;
    printf("Sent %ld bytes.\n", img_bytes);
    return rv + img_bytes;
}

void response_404(http_server *server, int new_socket_fd){
    char body[] = "<h1>404 Page Not Found</h1>";
    char header[] = "HTTP/1.1 404 NOT FOUND";
    char content_type[] = "text/html";
    int bytes_sent = send_http_response(
        server, new_socket_fd, header, content_type, body, strlen(body)
    );
}

void file_response_handler(http_server *server, int new_socket_fd, char *path){
    file_data *filedata;
    char *mime_type, *file_type = NULL;
    int cached = 0, cache_resource = 0;

    fprintf(stdout, "[Server:%d] [cache manager] retreiving key=%s from cache\n", server->port, path);
    cache_node *node = server_cache_manager(server, 1, path, NULL, NULL, 0);
    
    if (node == NULL){
        mime_type = mime_type_get(path);
        file_type = strstr(mime_type, "/");
        file_type += 1;
        
        printf("[Server:%d] Loading data from %s\n", server->port, path);
        if (strcmp(file_type, "jpg") == 0 || strcmp(file_type, "jpeg") == 0 || strcmp(file_type, "png") == 0){
            filedata = read_image(path);
        }
        else{
            filedata = file_load(path);
            cache_resource = 1;
            sleep(1); // temporarily slow down reads
        }
    }
    else{
        mime_type = node->content_type;
        filedata = (file_data*)malloc(sizeof(file_data));
        filedata->data = node->content;
        filedata->size = node->content_length;
        filedata->filename = node->key;
        mime_type = mime_type_get(filedata->filename);
        file_type = strstr(mime_type, "/");
        file_type += 1;
        cached = 1;
    }

    if (filedata == NULL){
        fprintf(stderr, "[Server:%d] File %.*s not found on server!\n",server->port, (int)strlen(path) ,path);
        char body[1024];
        sprintf(body, "File %.*s not found on Server!\n",(int)strlen(path) ,path);
        send_http_response(server, new_socket_fd, HEADER_404, "text/html", body, strlen(body));
        return;
    }

    struct timespec start, end;
    
    if (file_type && (strcmp(file_type, "jpg") == 0 || strcmp(file_type, "jpeg") == 0 || strcmp(file_type, "png") == 0)){
        
        printf("File Size : %d\n", filedata->size);
        printf("File Type : %s\n", mime_type);
        printf("File Descriptor : %d\n", *((int*)filedata->data));

        close(*((int*)filedata->data));

        int fd = get_image_fd(filedata->filename);
        if (fd < 0)
            response_404(server, new_socket_fd);
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        send_stream_http_response(server, new_socket_fd, HEADER_OK, mime_type, fd, filedata->size);
        clock_gettime(CLOCK_MONOTONIC, &end);
        filedata->data = NULL;
        close(fd);
    }
    else{
        clock_gettime(CLOCK_MONOTONIC, &start);
        send_http_response(server, new_socket_fd, HEADER_OK, mime_type, filedata->data, filedata->size);
        clock_gettime(CLOCK_MONOTONIC, &end);
    }

    fprintf(stdout, "[Server:%d] Sending response took %lf seconds.\n", server->port, get_time_difference(&start, &end));
    
    if (server->cache == NULL)
        file_free(filedata);
    else{
        if (!cached){
            fprintf(stdout, "[Server:%d] [cache manager] Adding key=%s to cache\n", server->port, path);
            if ( cache_resource && 
                server_cache_manager(server, 0, path, mime_type, filedata->data, filedata->size) == NULL
            ){
                fprintf(stderr, "[Server:%d] [cache manager] An error occured while storing data in cache.\n", server->port);
                return;
            }
        }
        free(filedata); // file data is now inside the cache. Free filedata struct
    }
    filedata = NULL;
    return;
}

void handle_http_request(http_server *server, int new_socket_fd){
    const long request_buffer_size = server->max_request_size;
    char *request = (char*)malloc(request_buffer_size);
    char *p;

    int bytes_received = recv(new_socket_fd, request, request_buffer_size - 1, 0);

    if (bytes_received < 0){
        fprintf(stderr, "[Server:%d] Did not receive any bytes in the request.\n", server->port);
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
        fprintf(stderr, "[Server:%d] Could not parse the headers in the request.\n", server->port);
        free(request);
        return;
    }
    // now we have parsed the request obtained. 
    // determine if the path request is a registered path
    char get[] = "GET";
    char post[] = "POST";
    

    char *path_split = strchr(path, ' ');
    *path_split = '\0';
    // printf("%ld\n", strlen(path));
    // printf("%s\n", path);

    char *params_split = strchr(path, '?');
    char *params = NULL;
    if (params_split){
        params = params_split+1;
        *params_split = '\0';
    }
    
    // printf("%ld\n", strlen(path));
    // printf("%s\n", path);

    path_len = strlen(path);
    char *search_path = (char*)malloc(path_len + 1);
    sprintf(search_path, "%.*s", (int)path_len, path);

    char *search_method = (char*)malloc(method_len+1);
    snprintf(search_method, method_len + 1, "%s" ,method);
    
    if (strcmp(get, search_method) == 0){
        server->server_logs->num_get_requests += 1;
    }

    if (strstr(search_path, ".") != NULL){
        // if request path is a resource on server and not a route, then respond with a file
        char resource_path[4096];
        sprintf(resource_path, "%s/%s", server->server_root_dir, search_path);
        file_response_handler(server, new_socket_fd, resource_path);
    }
    else{
        route_node *req_route = route_search(server->route_table, search_path);
        if (req_route == NULL || !route_check_method(req_route, search_method)){
            fprintf(stderr, "[Server:%d] 404 Page Not found!\n", server->port);
            response_404(server, new_socket_fd);
        }
        else if (req_route->value != NULL){
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

    server->server_logs->num_requests_served += 1;
    free(search_path);
    free(request);
    free(search_method);
    request = NULL;
    search_path = NULL;
    search_method = NULL;
}

void stop_server(){
    status = 0;
}

void server_start(http_server *server, int close_server, int print_logs){
    int new_socket_fd;
    int port = server->port;
    struct sockaddr_storage client_addr;
    char s[INET6_ADDRSTRLEN];
    
    // set the server listening socket to be non-blocking.
    int flags_before = fcntl(server->socket_fd, F_GETFL);
    if (flags_before == -1){
        fprintf(stderr, "[Server:%d] Could not obtain the flags for server listening TCP socket.\n", server->port);
        destroy_server(server, 0);
        exit(EXIT_FAILURE);
    }
    else{
        int set_nonblocking = fcntl(server->socket_fd, F_SETFL, flags_before | O_NONBLOCK);
        if (set_nonblocking == -1){
            fprintf(stderr, "[Server:%d] Could not set server TCP socket to NON_BLOCKING.\n", server->port);
            destroy_server(server, 0);
            exit(EXIT_FAILURE);
        }
    }
    
    signal(SIGINT, stop_server);
    
    while(status){
        socklen_t sin_size = sizeof(client_addr);
        
        new_socket_fd = accept(server->socket_fd, (struct sockaddr *)&client_addr, &sin_size);
        if (new_socket_fd == -1){
            if (errno == EWOULDBLOCK){
                if (!status){
                    break;
                }else{
                    msleep(0.5);
                    continue;
                }
            }
            else{
                fprintf(stderr, "[Server:%d] An error occured while accepting a connection.\n", server->port);
                continue;
            }
        }

        inet_ntop(client_addr.ss_family, get_internet_address((struct sockaddr *)&client_addr), s, sizeof(s));
        fprintf(stdout, "[Server:%d] Got connection from %s\n", port, s);
        handle_http_request(server, new_socket_fd);
        close(new_socket_fd);
    }

    // restore socket to be blocking
    if (fcntl(server->socket_fd, F_SETFL, flags_before) == -1){
        fprintf(stderr, "[Server:%d] Failed to gracefully shutdown server. Force Shutdown.\n", server->port);
        destroy_server(server, print_logs);
        exit(EXIT_FAILURE);
    }

    if (close_server){
        fprintf(stdout, "[Server:%d] Stopping server...\n", port);
        destroy_server(server, print_logs);
        fprintf(stdout, "[Server:%d] Stopped.\n", port);
    }
}
