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
#include <pthread.h>
#include "net.h"
#include "files.h"
#include "mime.h"
#include "lru.h"
#include "routes.h"
#include "server.h"
#include "picohttpparser.h"
#include "queues.h"

#define DEFAULT_PORT "8080"
#define DEFAULT_MAX_RESPONSE_SIZE 64 * 1024 * 1024 // 64 MB
#define DEFAULT_SERVER_FILE_PATH "./serverfiles"
#define DEFAULT_SERVER_ROOT "./serverroot"
#define DEFAULT_BACKLOG 10
#define DEFAULT_THREAD_POOL_SIZE 12
#define DEFAULT_BLOCK_DIM 2

volatile sig_atomic_t status;

/* msleep(): Sleep for the requested number of milliseconds. */
int msleep(long msec)
{
    struct timespec ts;
    int res;
    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }
    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;
    do
    {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);
    return res;
}

double get_time_difference(struct timespec *start, struct timespec *end)
{
    double tdiff = (end->tv_sec - start->tv_sec) + 1e-9 * (end->tv_nsec - start->tv_nsec);
    return tdiff;
}

void calculate_size(long bytes, long *arr)
{
    long gbs, mbs, kbs;
    gbs = (int)(bytes / 1024 / 1024 / 1024);
    bytes -= gbs * 1024 * 1024 * 1024;
    mbs = (int)(bytes / 1024 / 1024);
    bytes -= mbs * 1024 * 1024;
    kbs = (int)(bytes / 1024);
    bytes -= kbs * 1024;
    arr[0] = gbs;
    arr[1] = mbs;
    arr[2] = kbs;
    arr[3] = bytes;
}

void server_route(http_server *server, const char *key, const char *value, char **methods, size_t method_len, const char *route_dir, void (*route_fn)(void *, int, const char *, void *), void *fn_args)
{
    if (server == NULL)
    {
        fprintf(stderr, "Server object not created.\n");
        return;
    }
    register_route(server->route_table, key, value, methods, method_len, route_dir, route_fn, fn_args);
}

cache_node *server_cache_resource_handler(http_server *server, char *key, char *content_type, void *data, size_t content_length)
{
    if (key == NULL || data == NULL)
    {
        return NULL;
    }
    if (server->cache == NULL)
    {
        // fprintf(stderr, "[Server:%d] Caching is not enabled.\n", server->port);
        return NULL;
    }

    cache_node *node = cache_put(server->cache, key, content_type, data, content_length);
    if (node == NULL)
    {
        fprintf(stderr, "[Server:%d] An error occured while adding the resource to cache.\n", server->port);
        return NULL;
    }
    return node;
}

cache_node *server_cache_retreive_handler(http_server *server, char *key)
{
    if (key == NULL)
        return NULL;
    if (server->cache == NULL)
    {
        // fprintf(stderr, "[Server:%d] Caching is not enabled.\n", server->port);
        return NULL;
    }
    cache_node *node = cache_get(server->cache, key);
    if (node == NULL)
    {
        fprintf(stderr, "[Server:%d] Key=%s not present in cache.\n", server->port, key);
        return NULL;
    }
    return node;
}

cache_node *server_cache_manager(http_server *server, int opcode, char *key, char *content_type, void *data, size_t content_length)
{
    if (server == NULL)
    {
        fprintf(stderr, "Server ptr is NULL\n");
        return NULL;
    }
    if (server->cache == NULL)
    {
        // fprintf(stderr, "[Server:%d] Caching is not enabled.\n", server->port);
        return NULL;
    }
    if (key == NULL)
    {
        fprintf(stderr, "[Server:%d] A key must be provided for resource being cached!\n", server->port);
        return NULL;
    }
    cache_node *node = NULL;
    switch (opcode)
    {
    case 0:
        // add resource to cache
        if (data == NULL)
        {
            fprintf(stderr, "[Server:%d] No data provided for key=%s. Key not added to cache.", server->port, key);
            return NULL;
        }
        if (content_type == NULL)
        {
            // replace with default content type
            content_type = "text/html";
        }
        pthread_mutex_lock(&server->cache->mutex);
        node = server_cache_resource_handler(server, key, content_type, data, content_length);
        if (node)
        {
            server->server_logs->cache_miss += 1;
        }
        pthread_mutex_unlock(&server->cache->mutex);
        break;
    case 1:
        // retreive item from cache
        pthread_mutex_lock(&server->cache->mutex);
        node = server_cache_retreive_handler(server, key);
        if (node)
        {
            server->server_logs->cache_hits += 1;
        }
        pthread_mutex_unlock(&server->cache->mutex);
        break;
    default:
        fprintf(stderr, "[Server:%d] Invalid opcode=%d", server->port, opcode);
    }
    return node;
}

void server_cache_resource(http_server *server, char *key, char *content_type, void *data, size_t content_length)
{
    cache_node *node = server_cache_manager(server, 0, key, content_type, data, content_length);
    if (node == NULL)
    {
        fprintf(stderr, "[Server:%d] An error occured while adding the resource to cache.\n", server->port);
    }
    return;
}

cache_node *server_cache_retreive(http_server *server, char *key)
{
    cache_node *node = server_cache_manager(server, 1, key, NULL, NULL, 0);
    if (node == NULL)
    {
        fprintf(stderr, "[Server:%d] An error occured while retreiving the resource from cache.\n", server->port);
        return NULL;
    }
    return node;
}

int send_http_response(http_server *server, int new_socket_fd, char *header, char *content_type, char *body, size_t content_length)
{
    const long max_response_size = server->max_response_size;
    // char *response = (char *)malloc(sizeof(char) * 4096);
    char response[4096];
    long response_length;
    long body_size = content_length;

    sprintf(
        response,
        "%s\n"
        "Content-Length: %ld\n"
        "Content-Type: %s\n"
        "Connection: close\n"
        "\n",
        header, body_size, content_type);

    response_length = strlen(response);
    long rv_header = write(
        new_socket_fd,
        response,
        response_length);

    long rv = send(
        new_socket_fd,
        body,
        content_length, 0);

    if (rv < 0)
    {
        perror("Could not send response.");
    }
    // clean up response buffers
    // free(response);
    // response = NULL;
    return rv + rv_header;
}

int send_stream_http_response(http_server *server, int new_socket_fd, char *header, char *content_type, int *fd_ptr, size_t content_length)
{
    const long max_response_size = 4096;
    char *response = (char *)malloc(sizeof(char) * max_response_size);
    long response_length;
    long body_size = content_length;
    int fd = *fd_ptr;
    sprintf(
        response,
        "%s\n"
        "Content-Length: %ld\n"
        "Content-Type: %s\n"
        "Connection: close\n"
        "\n",
        header, body_size, content_type);

    response_length = strlen(response);

    long rv = write(
        new_socket_fd,
        response,
        response_length);

    int loc = lseek(fd, 0, SEEK_SET);
    long bytes = sendfile(new_socket_fd, fd, NULL, content_length);

    if (rv < 0)
    {
        perror("Could not send response.");
    }
    // clean up response buffers
    free(response);
    response = NULL;
    return rv + bytes;
}

int send_image_response(http_server *server, int new_socket_fd, char *header, char *content_type, char *data, size_t content_length)
{
    const long max_response_size = server->max_response_size;
    char *response = (char *)malloc(sizeof(char) * max_response_size);
    long response_length, body_size;
    body_size = content_length;

    sprintf(
        response,
        "%s\n"
        "Content-Length: %ld\n"
        "Content-Type: %s\n"
        "Connection: close\n"
        "\n",
        header, body_size, content_type);

    response_length = strlen(response);

    long rv = write(
        new_socket_fd,
        response,
        response_length);

    long img_bytes = send(new_socket_fd, data, content_length, 0);

    if (rv < 0 || img_bytes < 0)
    {
        perror("Could not send response.\n");
    }

    free(response);
    response = NULL;
    return rv + img_bytes;
}

int response_404(http_server *server, int new_socket_fd)
{
    char body[] = "<h1>404 Page Not Found</h1>";
    char header[] = "HTTP/1.1 404 NOT FOUND";
    char content_type[] = "text/html";
    int bytes_sent = send_http_response(
        server, new_socket_fd, header, content_type, body, strlen(body));
    return bytes_sent;
}

int file_response_handler(http_server *server, int new_socket_fd, char *path)
{
    file_data *filedata;
    char *mime_type, *file_type = NULL;
    int cached = 0;
    int bytes_sent = 0;

    // fprintf(stdout, "[Server:%d] [cache manager] retreiving key=%s from cache\n", server->port, path);
    cache_node *node = server_cache_manager(server, 1, path, NULL, NULL, 0);

    if (node == NULL)
    {
        mime_type = mime_type_get(path);
        file_type = strchr(mime_type, '/');
        file_type += 1;

        // pthread_mutex_lock(&server->lock);
        // server->server_logs->cache_miss += 1;
        // pthread_mutex_unlock(&server->lock);

        // printf("[Server:%d] Loading data from %s\n", server->port, path);
        filedata = file_load(path);
        // filedata = read_file_fd(path);
    }
    else
    {
        mime_type = node->content_type;
        filedata = (file_data *)malloc(sizeof(file_data));
        filedata->data = node->content;
        filedata->size = node->content_length;
        filedata->filename = node->key;
        mime_type = mime_type_get(filedata->filename);
        file_type = strchr(mime_type, '/');
        file_type += 1;
        cached = 1;
    }

    if (filedata == NULL)
    {
        fprintf(stderr, "[Server:%d] File %.*s not found on server!\n", server->port, (int)strlen(path), path);
        char body[1024];
        sprintf(body, "File %.*s not found on Server!\n", (int)strlen(path), path);
        bytes_sent = send_http_response(server, new_socket_fd, HEADER_404, "text/html", body, strlen(body));
        return bytes_sent;
    }

    bytes_sent = send_http_response(server, new_socket_fd, HEADER_OK, mime_type, filedata->data, filedata->size);
    // bytes_sent = send_stream_http_response(server, new_socket_fd, HEADER_OK, mime_type, (int *)filedata->data, filedata->size);

    if (server->cache == NULL)
        file_free(filedata);
    else
    {
        if (!cached)
        {
            // fprintf(stdout, "[Server:%d] [cache manager] Adding key=%s to cache\n", server->port, path);
            if (
                server_cache_manager(server, 0, path, mime_type, filedata->data, filedata->size) == NULL)
            {
                fprintf(stderr, "[Server:%d] [cache manager] An error occured while storing data in cache.\n", server->port);
                return bytes_sent;
            }
        }
        free(filedata); // file data is now inside the cache. Free filedata struct
    }
    filedata = NULL;
    return bytes_sent;
}

struct thread_payload
{
    http_server *server;
    http_server_logs *logs;
    int new_socket_fd;
};

void *handle_http_request(void *arg)
{
    struct thread_payload *payload = (struct thread_payload *)arg;
    http_server *server = payload->server;
    http_server_logs *logs = payload->logs;
    int new_socket_fd = payload->new_socket_fd;

    struct timespec
        req_parse_start,
        req_parse_end,
        search_start,
        search_end,
        res_start,
        res_end;

    const long request_buffer_size = 4096;
    char *request = (char *)malloc(request_buffer_size);
    if (request == NULL)
    {
        fprintf(stderr, "[Server:%d] Did not receive any bytes in the request.\n", server->port);
        free(request);
        shutdown(new_socket_fd, SHUT_RDWR);
        close(new_socket_fd);
        return NULL;
    }
    char *p;

    int bytes_received = recv(new_socket_fd, request, request_buffer_size - 1, 0);

    if (bytes_received < 0)
    {
        fprintf(stderr, "[Server:%d] Did not receive any bytes in the request.\n", server->port);
        free(request);
        shutdown(new_socket_fd, SHUT_RDWR);
        close(new_socket_fd);
        return NULL;
    }
    else
    {
        logs->num_bytes_received += bytes_received;
    }

    request[bytes_received] = '\0';
    clock_gettime(CLOCK_MONOTONIC, &req_parse_start);

    const char *method, *path;
    int pret, minor_version;
    struct phr_header headers[100];
    size_t buflen = 0, method_len, path_len, num_headers;

    num_headers = sizeof(headers) / sizeof(headers[0]);
    pret = phr_parse_request(
        request, bytes_received, &method, &method_len, &path, &path_len,
        &minor_version, headers, &num_headers, 0);

    if (pret == -1)
    {
        fprintf(stderr, "[Server:%d] Could not parse the headers in the request.\n", server->port);
        free(request);
        shutdown(new_socket_fd, SHUT_RDWR);
        close(new_socket_fd);
        return NULL;
    }
    // now we have parsed the request obtained.
    // determine if the path request is a registered path
    char get[] = "GET";
    char post[] = "POST";

    if (path == NULL)
    {
        free(request);
        shutdown(new_socket_fd, SHUT_RDWR);
        close(new_socket_fd);
        return NULL;
    }

    char *path_split = strchr(path, ' ');
    *path_split = '\0';

    char *params_split = strchr(path, '?');
    char *params = NULL;
    if (params_split)
    {
        params = params_split + 1;
        *params_split = '\0';
    }

    path_len = strlen(path);
    char *search_path = (char *)malloc(path_len + 1);
    sprintf(search_path, "%.*s", (int)path_len, path);

    char *search_method = (char *)malloc(method_len + 1);
    sprintf(search_method, "%.*s", (int)method_len, method);

    if (strcmp(get, search_method) == 0)
    {
        logs->num_get_requests += 1;
    }
    else
    {
        fprintf(stderr, "%*.s PATH=%*.s\n", (int)method_len, method, (int)path_len, path);
    }

    clock_gettime(CLOCK_MONOTONIC, &req_parse_end); // request parsing completed.
    int bytes_sent = 0;
    if (strstr(search_path, ".") != NULL)
    {
        clock_gettime(CLOCK_MONOTONIC, &search_start);
        clock_gettime(CLOCK_MONOTONIC, &search_end);

        // if request path is a resource on server and not a route, then respond with a file
        char resource_path[4096];
        sprintf(resource_path, "%s/%s", server->server_root_dir, search_path);
        clock_gettime(CLOCK_MONOTONIC, &res_start);
        bytes_sent = file_response_handler(server, new_socket_fd, resource_path);
        clock_gettime(CLOCK_MONOTONIC, &res_end);
    }
    else
    {
        clock_gettime(CLOCK_MONOTONIC, &search_start);
        route_node *req_route = route_search(server->route_table, search_path);
        clock_gettime(CLOCK_MONOTONIC, &search_end);
        if (req_route == NULL || !route_check_method(req_route, search_method))
        {
            fprintf(stderr, "[Server:%d] 404 Page Not found!\n", server->port);
            clock_gettime(CLOCK_MONOTONIC, &res_start);
            bytes_sent = response_404(server, new_socket_fd);
            clock_gettime(CLOCK_MONOTONIC, &res_end);
        }
        else if (req_route->value != NULL)
        {
            char file_path[4096];
            sprintf(file_path, "%s/%s", server->server_root_dir, req_route->value);
            clock_gettime(CLOCK_MONOTONIC, &res_start);
            bytes_sent = file_response_handler(server, new_socket_fd, file_path);
            clock_gettime(CLOCK_MONOTONIC, &res_end);
        }
        else
        {
            char dir_path[4096];
            sprintf(dir_path, "%s/%s", server->server_root_dir, req_route->route_dir);
            clock_gettime(CLOCK_MONOTONIC, &res_start);
            req_route->route_fn(server, new_socket_fd, dir_path, req_route->fn_args);
            clock_gettime(CLOCK_MONOTONIC, &res_end);
        }
    }

    logs->num_requests_served += 1;
    logs->num_bytes_sent += bytes_sent;
    shutdown(new_socket_fd, SHUT_RDWR);
    close(new_socket_fd);

    if (search_path)
        free(search_path);
    if (request)
        free(request);
    if (search_method)
        free(search_method);

    free(payload);
    payload = NULL;
    request = NULL;
    search_path = NULL;
    search_method = NULL;

    double request_time = get_time_difference(&req_parse_start, &req_parse_end) * 1000;
    double search_time = get_time_difference(&search_start, &search_end) * 1000;
    double response_time = get_time_difference(&res_start, &res_end) * 1000;
    // fprintf(stdout, "[Server:%d] Request Parsing: %lfms; Method Search: %lfms; Response: %lfms; Total Time: %lfms\n", server->port, request_time, search_time, response_time, request_time + search_time + response_time);
    return NULL;
}

struct queue_manager_ctx
{
    queues **multi_queue;
    int num_queues;
    int grid_dim;
    int block_dim;
    int num_complete_blocks;
    int padded_block_dim;
    long generator;
    long generator_reset_multiple;
    pthread_mutex_t lock;
};

struct queue_manager_ctx *queue_manager_ctx_initializer(int grid_dim, int block_dim)
{
    struct queue_manager_ctx *ctx = (struct queue_manager_ctx *)malloc(sizeof(struct queue_manager_ctx));

    if (ctx == NULL)
    {
        fprintf(stderr, "Error allocating memory when creating queue_manager_ctx.\n");
        return NULL;
    }

    int num_blocks = 0;
    int num_complete_blocks = (int)(grid_dim / block_dim);
    if (num_complete_blocks == 0)
    {
        fprintf(stderr, "Number of blocks is zero for the given grid_dim and block_dim.\n");
        return NULL;
    }
    int padded_block_dim = grid_dim - (block_dim * num_complete_blocks);
    num_blocks = padded_block_dim == 0 ? num_complete_blocks : num_complete_blocks + 1;
    ctx->num_queues = num_blocks;
    ctx->num_complete_blocks = num_complete_blocks;
    ctx->padded_block_dim = padded_block_dim;
    ctx->block_dim = block_dim;
    ctx->grid_dim = grid_dim;
    ctx->generator = 0L;
    ctx->generator_reset_multiple = num_blocks * 1000;

    ctx->multi_queue = (queues **)malloc(sizeof(queues *) * ctx->num_queues);
    for (int i = 0; i < ctx->num_queues; i++)
    {
        ctx->multi_queue[i] = queue_create();
    }

    ctx->lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    fprintf(stdout, "Number of Queues : %d\n", ctx->num_queues);
    fprintf(stdout, "Number of Complete Blocks : %d\n", ctx->num_complete_blocks);
    fprintf(stdout, "Padded Block Dim : %d\n", ctx->padded_block_dim);
    fprintf(stdout, "Block Dim : %d\n", ctx->block_dim);
    fprintf(stdout, "Grid Dim : %d\n", ctx->grid_dim);

    return ctx;
}

long ctx_idx_gen(struct queue_manager_ctx *ctx)
{
    long val = ctx->generator;
    if ((ctx->generator + 1) % ctx->generator_reset_multiple == 0)
    {
        ctx->generator = 0;
    }
    else
    {
        ctx->generator++;
    }
    return val;
}

struct thread_payload *queue_manager(struct queue_manager_ctx *ctx, int opcode, struct thread_payload *data, int rank)
{
    if (ctx == NULL)
    {
        fprintf(stderr, "Queue manager context(ctx) is not initialized.\n");
        return NULL;
    }

    if (opcode < 0)
    {
        return NULL;
    }
    struct thread_payload *payload = NULL;
    if (opcode == 0)
    {
        if (data == NULL)
        {
            return NULL;
        }
        // enqueue the incomming request to the appropriate queue
        long queue_index = ctx_idx_gen(ctx) % ctx->num_queues;
        queues *queue_ptr = ctx->multi_queue[queue_index];
        payload = enqueue(queue_ptr, data);
    }
    else if (opcode == 1)
    {
        // get the correct queue for the given thread's rank
        long queue_index = (int)(rank / ctx->block_dim);
        queues *queue_ptr = ctx->multi_queue[queue_index];
        payload = dequeue(queue_ptr);
    }
    else
    {
        fprintf(stderr, "Invalid opcode provided to queue_manager()");
        return NULL;
    }
    return payload;
}

void queue_manager_ctx_destroy(struct queue_manager_ctx *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    for (int i = 0; i < ctx->num_queues; i++)
    {
        queue_destroy(ctx->multi_queue[i]);
        ctx->multi_queue[i] = NULL;
    }
    free(ctx->multi_queue);
    ctx->multi_queue = NULL;
    free(ctx);
    ctx = NULL;
}

struct thread_function_payload
{
    struct queue_manager_ctx *ctx;
    queues *queue;
    http_server *server;
    void *(*fn)(void *);
    int rank;
};

void *thread_function_wrapper(void *arg)
{
    struct thread_function_payload *payload = (struct thread_function_payload *)arg;
    struct queue_manager_ctx *ctx = payload->ctx;
    int rank = payload->rank;
    queues *queue = payload->queue;
    http_server *server = payload->server;
    http_server_logs *thread_logs = (http_server_logs *)malloc(sizeof(http_server_logs));
    struct thread_payload *client_payload = NULL;

    while (status)
    {
        // pthread_mutex_lock(&queue->mutex);
        // client_payload = (struct thread_payload *)dequeue(queue);
        // pthread_mutex_unlock(&queue->mutex);
        client_payload = (struct thread_payload *)queue_manager(ctx, 1, NULL, rank);

        if (client_payload != NULL)
        {
            // printf("[Server:%d] [Thread:%d] Picked up request.\n", server->port, rank);
            client_payload->logs = thread_logs; // attach thread logs to the request
            payload->fn(client_payload);
        }
        else
            msleep(0.5);
    }

    pthread_mutex_lock(&server->lock);
    server->server_logs->num_bytes_received += thread_logs->num_bytes_received;
    server->server_logs->num_bytes_sent += thread_logs->num_bytes_sent;
    server->server_logs->num_get_requests += thread_logs->num_get_requests;
    server->server_logs->num_requests_served += thread_logs->num_requests_served;
    pthread_mutex_unlock(&server->lock);
    free(payload);
    free(thread_logs);
    payload = NULL;
    thread_logs = NULL;
}

http_server *create_server(int port, int cache_size, int hashsize, char *root_dir, long max_request_size, long max_response_size, int backlog)
{
    http_server *server = (http_server *)malloc(sizeof(*server));
    if (server == NULL)
    {
        fprintf(stderr, "Error while allocating memory to server on port %d\n", port);
        exit(EXIT_FAILURE);
    }
    server->server_logs = (http_server_logs *)malloc(sizeof(http_server_logs));
    if (server->server_logs == NULL)
    {
        fprintf(stderr, "Error while allocating memory to logs for server on port %d\n", port);
        destroy_server(server, 0);
        exit(EXIT_FAILURE);
    }
    server->port = port;

    if (cache_size == 0)
    {
        server->cache = NULL;
    }
    else
    {
        server->cache = lru_create(cache_size, hashsize);
        if (server->cache == NULL)
        {
            fprintf(stderr, "Error while creating cache for server on port %d\n", port);
            destroy_server(server, 0);
            exit(EXIT_FAILURE);
        }
    }

    server->route_table = route_create();
    if (server->route_table == NULL)
    {
        fprintf(stderr, "Error while creating route tables for server on port %d\n", port);
        exit(EXIT_FAILURE);
    }

    server->max_response_size = max_response_size ? max_response_size : DEFAULT_MAX_RESPONSE_SIZE;
    server->max_request_size = max_request_size ? max_request_size : DEFAULT_MAX_RESPONSE_SIZE;
    server->lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    server->backlog = backlog ? backlog : DEFAULT_BACKLOG;
    server->server_logs->max_cache_size = cache_size;
    server->server_logs->num_bytes_sent = 0L;
    server->server_logs->num_bytes_received = 0L;
    server->server_logs->num_get_requests = 0;
    server->server_logs->num_requests_served = 0;
    server->server_logs->num_routes = 0;
    server->server_logs->cache_hits = 0;
    server->server_logs->cache_miss = 0;

    if (root_dir != NULL)
    {
        server->server_root_dir = root_dir;
    }
    else
    {
        server->server_root_dir = DEFAULT_SERVER_ROOT;
    }

    int port_length = snprintf(NULL, 0, "%d", port);
    char *port_string = malloc(port_length + 1);
    snprintf(port_string, port_length + 1, "%d", port);
    server->socket_fd = get_listener_socket(port_string, server->backlog);
    if (server->socket_fd < 0)
    {
        fprintf(stderr, "Error while binding to port %d\n", port);
        destroy_server(server, 0);
        exit(EXIT_FAILURE);
    }
    free(port_string);
    port_string = NULL;
    status = 1;
    return server;
}

void stop_server()
{
    status = 0;
}

void server_start(http_server *server, int close_server, int print_logs)
{

    int port = server->port;
    struct sockaddr_storage client_addr;
    char s[INET6_ADDRSTRLEN];

    // set the server listening socket to be non-blocking.
    int flags_before = fcntl(server->socket_fd, F_GETFL);
    if (flags_before == -1)
    {
        fprintf(stderr, "[Server:%d] Could not obtain the flags for server listening TCP socket.\n", server->port);
        destroy_server(server, 0);
        exit(EXIT_FAILURE);
    }
    else
    {
        int set_nonblocking = fcntl(server->socket_fd, F_SETFL, flags_before | O_NONBLOCK);
        if (set_nonblocking == -1)
        {
            fprintf(stderr, "[Server:%d] Could not set server TCP socket to NON_BLOCKING.\n", server->port);
            destroy_server(server, 0);
            exit(EXIT_FAILURE);
        }
    }

    signal(SIGINT, stop_server);

    // setup queue for storing incoming connections
    queues *queue = queue_create();
    struct queue_manager_ctx *ctx = queue_manager_ctx_initializer(DEFAULT_THREAD_POOL_SIZE, DEFAULT_BLOCK_DIM);
    // create the default thread_function_payload arg
    // struct thread_function_payload fn_payload[DEFAULT_THREAD_POOL_SIZE];

    // setup thread pool using the DEFAULT_THREAD_POOL_SIZE
    pthread_t thread_pool[DEFAULT_THREAD_POOL_SIZE];
    for (int i = 0; i < DEFAULT_THREAD_POOL_SIZE; i++)
    {
        struct thread_function_payload *fn_payload = (struct thread_function_payload *)malloc(sizeof(struct thread_function_payload));
        fn_payload->fn = handle_http_request;
        fn_payload->ctx = ctx;
        fn_payload->queue = queue;
        fn_payload->server = server;
        fn_payload->rank = i;
        pthread_create(&thread_pool[i], NULL, thread_function_wrapper, fn_payload);
    }

    while (status)
    {
        socklen_t sin_size = sizeof(client_addr);
        int new_socket_fd;
        new_socket_fd = accept(server->socket_fd, (struct sockaddr *)&client_addr, &sin_size);
        if (new_socket_fd == -1)
        {
            if (errno == EWOULDBLOCK)
            {
                if (!status)
                {
                    // free(new_socket_fd);
                    break;
                }
                else
                {
                    // free(new_socket_fd);
                    msleep(0.5);
                    continue;
                }
            }
            else
            {
                fprintf(stderr, "[Server:%d] An error occured while accepting a connection.\n", server->port);
                continue;
            }
        }

        inet_ntop(client_addr.ss_family, get_internet_address((struct sockaddr *)&client_addr), s, sizeof(s));
        // fprintf(stdout, "[Server:%d] Got connection from %s\n", port, s);

        // create thread to handle the new request
        struct thread_payload *payload = (struct thread_payload *)malloc(sizeof(struct thread_payload));
        payload->new_socket_fd = new_socket_fd;
        payload->server = server;

        // enqueue the new connection to the shared queue
        queue_manager(ctx, 0, payload, -1);
        // pthread_mutex_lock(&queue->mutex);
        // enqueue(queue, payload);
        // pthread_mutex_unlock(&queue->mutex);
    }

    // wait on all threads to complete before stopping.
    for (int i = 0; i < DEFAULT_THREAD_POOL_SIZE; i++)
    {
        pthread_join(thread_pool[i], NULL);
    }

    // destroy queue and fn_payload
    // free(fn_payload);
    // fn_payload = NULL;
    // printf("Destroying queue\n");
    queue_destroy(queue);
    queue = NULL;
    queue_manager_ctx_destroy(ctx);

    // restore socket to be blocking
    if (fcntl(server->socket_fd, F_SETFL, flags_before) == -1)
    {
        fprintf(stderr, "[Server:%d] Failed to gracefully shutdown server. Force Shutdown.\n", server->port);
        destroy_server(server, print_logs);
        exit(EXIT_FAILURE);
    }

    if (close_server)
    {
        // fprintf(stdout, "[Server:%d] Stopping server...\n", port);
        destroy_server(server, print_logs);
        // fprintf(stdout, "[Server:%d] Stopped.\n", port);
    }
}

void print_server_logs(http_server *server)
{
    fprintf(stdout, "Server Port: %d\n", server->port);
    fprintf(stdout, "Server Cache enabled (Y/n): %c\n", server->cache == NULL ? 'n' : 'Y');

    if (server->cache != NULL)
    {
        fprintf(stdout, "Server Cache size: %d\n", server->cache->max_size);
        fprintf(stdout, "Server Cache Hashtable size: %d\n", server->cache->table->size);
        fprintf(stdout, "Server Cache Hashtable load: %.2f\n", server->cache->table->load * 100);
        fprintf(stdout, "Server Cache #Hits: %d\n", server->server_logs->cache_hits);
        fprintf(stdout, "Server Cache #Miss: %d\n", server->server_logs->cache_miss);
    }

    fprintf(stdout, "Server Directory: %s\n", server->server_root_dir);
    fprintf(stdout, "Number of Routes: %d\n", server->route_table->num_routes);
    fprintf(stdout, "Max Request size: %ld\n", server->max_request_size);
    fprintf(stdout, "Max Response size: %ld\n", server->max_response_size);
    fprintf(stdout, "Server Backlog: %d\n", server->backlog);

    long received_bytes[4] = {0, 0, 0, 0}, sent_bytes[4] = {0, 0, 0, 0};
    calculate_size(server->server_logs->num_bytes_received, received_bytes);
    calculate_size(server->server_logs->num_bytes_sent, sent_bytes);

    fprintf(stdout, "Total Bytes Received: %ld\t", server->server_logs->num_bytes_received);
    fprintf(stdout, "(%ld GB; %ld MB; %ld KB; %ld B)\n", received_bytes[0], received_bytes[1], received_bytes[2], received_bytes[3]);
    fprintf(stdout, "Total Bytes Sent: %ld\t", server->server_logs->num_bytes_sent);
    fprintf(stdout, "(%ld GB; %ld MB; %ld KB; %ld B)\n", sent_bytes[0], sent_bytes[1], sent_bytes[2], sent_bytes[3]);
    fprintf(stdout, "Number of GET Requests Received: %d\n", server->server_logs->num_get_requests);
    fprintf(stdout, "Number of Requests Served: %d\n", server->server_logs->num_requests_served);
}

void destroy_server(http_server *server, int print_logs)
{
    if (print_logs)
        print_server_logs(server);

    // printf("Destroying Server running on port: %d\n", server->port);
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