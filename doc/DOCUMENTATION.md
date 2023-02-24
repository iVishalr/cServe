<p align="center">
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="./images/logo-dark.png">
  <source media="(prefers-color-scheme: light)" srcset="./images/logo-light.png">
  <img alt="cServe Logo" src="./doc/images/logo-light.png">
</picture>
</p>

<hr>

cServe is a minimal multithreaded HTTP server that can be used to serve static content. Currently, cServer supports only GET requests and there is no support for other methods. This document provides additional details on how you can use cServe to serve your static files on internet.

## Documentation

Following sections demonstrates how to use cServe for hosting your static content on the web.

### What to include to use cServe?

`#include "server.h"` must be used to include cServe. It is worth noting that you should have compiled the library before hand and must have the `libcserve.so` file on your system along with the header files associated with cServe.

### Creating a HTTP server object

_Prototype_:

```C
http_server *create_server(int port, int cache_size, int hashsize, char *root_dir, long max_request_size, long max_response_size, int backlog);
```

`port` - Specify the port on which the server must listen on for accepting connections

`cache_size` - Specify the size of the LRU cache you would like to use. Provide `0` to disable cache

`hashsize` - Specify the size to be same as `cache_size`.

`root_dir` - Specifies the Server's directory. The server can see only the files present in the value provided to `root_dir`.

`max_request_size` - Specifies the maximum size of request. Provide `0` to use default values.

`max_response_size` - Specifies the maximum size of response. Provide `0` to use default values.

`backlog` - Number of connections to queue up on port specified. Provide a high number (>100).

_Example_:

```C
http_server *server = create_server(8080, 0, 0, "static-website-example", 0, 0, 1000);
```

Here server runs on port 8080. Cache is disabled. Server root directory is a path to the folder with name "static-website-example". Files outside this folder will not be visible to server. max request/response size is set to default values by passing `0` and backlog is set to 1000 connections.

### Adding routes to your server

This section deals with adding routes to your server.

_Prototype_:

```C
void server_route(http_server *server, const char *key, const char *value, char **methods, size_t method_len, const char *route_dir, void (*route_fn)(void *, int, const char *, void *), void *fn_args);
```

`*server` - Pass the pointer to the server object as returned by `create_server()`.

`*key` - Pass a string to define the route end point. Example - "/about" , "/contact" and so on. '/' is a must to include in your key.

`*value` - Pass a filename to associate file to the defined route. If you want to use custom functions, pass NULL to value. Note that the filename passed to this argument needs to be in server's directory. Otherwise server will not be able to find the file with given filename.

`**methods` - Pass an array of strings that specify the supported methods for the route.

`method_len` - Number of methods supported by the route.

`route_dir` - Pass a folder name to this argument if you want to isolate the route to a certain directory in server's directory. If you want access to all files in server's directory, pass `/`. Note that this argument will be used only when you are working with custom functions.

`*route_fn` - Pass your custom defined function to this argument. cServe will internally call this function to handle requests.

`*fn_args` - Pass your custom defined arguments to your custom function. Pass NULL if your custom defined function does not take any argument.

More on custom defined functions is explained later in the documentation.

_Example_:

```C
server_route(server, "/", "index.html", methods, method_len, NULL, NULL, NULL);
server_route(server, "/about", NULL, methods, method_len, "/", custom_fn, NULL);
```

### Start listening for connections

_Prototype_:

```C
void server_start(http_server *server, int close_server, int print_logs);
```

`*server` - Pass the pointer to the server object as returned by `create_server()`.

`close_server` - Pass `1` if you want to close the server automatically when server is interrupted. Pass `0` if you do not want the server to be closed automatically.

`print_logs` - Pass `1` if you want to print the server statistics when you interrupt the server. Pass `0` to disable printing.

_Example_:

```C
server_start(server, 1, 1);
```

### Destroying Server

_Prototype_:

```C
void destroy_server(http_server *server, int print_logs);
```

`*server` - Pass pointer to the server object as returned by `create_server()`.

`print_logs` - Pass `1` if you want to print the server statistics when you interrupt the server. Pass `0` to disable printing.

_Example_:

```C
destroy_server(server, 1);
```

### Print Server logs

To be used to manually print server logs.

_Prototype_:

```C
void print_server_logs(http_server *server);
```

`*server` - Pass pointer to the server object as returned by `create_server()`.

Prints logs like shown below

```console
^CServer Port: 8082
Server Cache enabled (Y/n): n
Server Directory: static-website-example
Number of Routes: 2
Max Request size: 67108864
Max Response size: 67108864
Server Backlog: 100000
Total Bytes Received: 8309	(0 GB; 0 MB; 8 KB; 117 B)
Total Bytes Sent: 2340762	(0 GB; 2 MB; 237 KB; 922 B)
Number of GET Requests Received: 14
Number of Requests Served: 14
```

### Writing Custom Functions for handling HTTP requests

This feature of cServe allows you to define your own functions to handle the HTTP request the way you want. Several helper routines are made available to make it easy to send responses to requests. However, there's a format in which custom functions are to be written. The format is given below

```C
void custom_fn(void *server, int new_socket_fd, const char *path, void *args);
```

Each custom function must accept a pointer to server object as the first argument. The second argument would be a socket file descriptor that's associated with a connection. `path` provides access to server's directory or a `route_dir` inside server's directory. `*args` will contain all the other arguments you need to have access to inside the function.

`*server` pointer must be typecasted to `http_server *` as the first step inside the function.

#### Helper functions

##### Loading files from disk

To load a file from disk, `file_load(path)` can be used. Include the header `#include "files.h"`. The prototype of the function is given below.

_Prototype_:

```C
file_data *file_load(char *filename);
```

`*filename` - Pass the path of the file to be loaded.

The function returns a file_data struct whose prototype is shown below

```C
typedef struct file_data
{
    int size; // stores the size of the file
    void *data; // stores file's data in bytes
    char *filename; // stores the name of the file
} file_data;
```

##### Get MIME Type of the file

HTTP responses require the Content Type of the file being sent via the response. To make this easier, `mime_type_get(filename)` can be used. Include the header `#include "mime.h"`.

_Prototype_:

```C
char *mime_type_get(char *filename);
```

Returns a string containing the mime type of the filename passed.

##### HTTP Headers

`HEADER_OK` - A macro for sending 200 OK as HTTP header

`HEADER_404` - A macro for sending 404 NOT FOUND as HTTP headeer.

##### Sending HTTP Response

To send HTTP response to a request, a helper function called `send_http_response()` is defined.

_Prototype_:

```C
int send_http_response(http_server *server, int new_socket_fd, char *header, char *content_type, char *body, size_t content_length);
```

`*server` - - Pass pointer to the server object as returned by `create_server()`.

`new_socket_fd` - Pass the socket file descriptor passed to your custom function by cServe.

`*header` - Use HTTP Header Macros: HEADER_OK, HEADER_404

`*content_type` - Pass the string returned by `mime_type_get()`.

`*body` - Pass the body of the response. Example, `filedata->data`.

`content_length` - Pass the size or length of your body.

Returns the number of bytes send via the socket.

_Example_:

```C
send_http_response(server, new_socket_fd, HEADER_OK, mime_type, filedata->data, filedata->size);
```

```C
char body[] = "Resource not Found!";
send_http_response(server, new_socket_fd, HEADER_404, "text/html", body, strlen(body));
```

##### Want to cache the resource being sent?

To store frequently accessed data in cache, cServe makes provisions for it via two functions. One for retreiving from the cache and other for storing data in cache.

1. To retreive data from cache

_Prototype_:

```C
cache_node *server_cache_retreive(http_server *server, char *key);
```

2. To store data to cache

_prototype_:

```C
void server_cache_resource(http_server *server, char *key, char *content_type, void *data, size_t content_length);
```

Arguments to these functions are self explanatory when `send_http_response()` is understood.

The structure of `cache_node` is as follows:

```C
typedef struct cache_node
{
    char *key; // key of endpoint. Acts as a key for cache
    char *content_type;
    int content_length;
    void *content;

    struct cache_node *next;
    struct cache_node *prev;
} cache_node;
```

Accessing to the cache is done by cServe's Cache Manager. Hence you need not worry about locking the cache while accessing.

##### Example Custom Defined Function

```C
void custom_fn(void *server, int new_socket_fd, const char *path, void *args)
{
    http_server *s = (http_server *)server;
    char filepath[4096];
    sprintf(filepath, "%s/bio.html", path);
    file_data *filedata = file_load(filepath);
    char *mime_type = mime_type_get(filepath);
    if (filedata != NULL && mime_type != NULL)
    {
        send_http_response(
            server, new_socket_fd, HEADER_OK, mime_type, filedata->data, filedata->size);
    }
    else
    {
        char body[] = "Resource not Found!";
        send_http_response(server, new_socket_fd, HEADER_404, "text/html", body, strlen(body));
    }
}
```

## Sample Code to Summarize this documentation

main.c

```C
#include <stdio.h>
#include <string.h>
#include "files.h"
#include "mime.h"
#include "server.h"

void custom_fn(void *server, int new_socket_fd, const char *path, void *args)
{
    http_server *s = (http_server *)server;
    char filepath[4096];
    sprintf(filepath, "%s/bio.html", path);
    file_data *filedata = file_load(filepath);
    char *mime_type = mime_type_get(filepath);
    if (filedata != NULL && mime_type != NULL)
    {
        send_http_response(
            server, new_socket_fd, HEADER_OK, mime_type, filedata->data, filedata->size);
    }
    else
    {
        char body[] = "Resource not Found!";
        send_http_response(server, new_socket_fd, HEADER_404, "text/html", body, strlen(body));
    }
}

int main()
{
    size_t method_len = 1;
    char *methods[1] = {"GET"};
    http_server *server = create_server(8080, 0, 0, "static-website-example", 0, 0, 1000);
    server_route(server, "/", "index.html", methods, method_len, NULL, NULL, NULL);
    server_route(server, "/about", NULL, methods, method_len, "/", custom_fn, NULL);
    printf("Starting server\n");
    server_start(server, 1, 1);
    return 0;
}
```

Compile and run using the following command

```bash
gcc -O2 -I include/ main.c -o server -lcserve -L ./
```

Assumes that you are in the project directory, where include/ is the path to header files of cServe and `-L ./` provides the path to `libcserve.so` file.

To run the server,

```bash
./server
```

```console
server listening on port 8080...
Added Route - / with value index.html
Added Route - /about with value (null)
Starting server
Number of Queues : 6
Number of Complete Blocks : 6
Padded Block Dim : 0
Block Dim : 2
Grid Dim : 12
```
