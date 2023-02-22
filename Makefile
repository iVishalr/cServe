CC=gcc
CPP=g++
CFLAGS=-O2 -I include/ -fPIC

SRC=src
BUILD=build
INCLUDE=include
CSRCS=$(wildcard $(SRC)/*.c)
OBJS=$(patsubst $(SRC)/%.c, $(BUILD)/%.o, $(CSRCS))
LDFLAGS=-lm -lpthread
LIBRARY=/usr/local/lib
HEADERS=/usr/local/include 
LDNAME=libcserve.so
LDNAME_VERSION=libcserve.so.0.1.0
LD_NAME_MAJOR=libcserve.so.0

all:
	@if ! test -d $(BUILD); \
		then echo "\033[93msetting up build directory...\033[0m"; mkdir -p build;\
  	fi
	@$(MAKE) server

server: $(OBJS)
	$(CC) $(LDFLAGS) -o $(LDNAME) $(OBJS) -shared

$(BUILD)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD) a.out server libcserve.so