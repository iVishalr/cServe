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

#define DEFAULT_PORT "8080"
#define DEFAULT_SERVER_FILE_PATH "./serverfiles"
#define DEFAULT_SERVER_ROOT "./serverroot"

