CC=gcc
CFLAGS=-O2 -I include/

SRC=src
BUILD=build
SRCS=$(wildcard $(SRC)/*.c)
OBJS=$(patsubst $(SRC)/%.c, $(BUILD)/%.o, $(SRCS))
LDFLAGS=-lm -lpthread

all:
	@if ! test -d $(BUILD); \
		then echo "\033[93msetting up build directory...\033[0m"; mkdir -p build;\
  	fi
	@$(MAKE) server

server: $(OBJS)
	$(CC) $(CFLAGS) main.c -o $@ $^ $(LDFLAGS)

$(BUILD)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD) a.out server.out 