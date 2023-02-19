CC=gcc
CPP=g++
CFLAGS=-O3 -I include/

SRC=src
BUILD=build
CSRCS=$(wildcard $(SRC)/*.c)
CPPSRCS=$(wildcard $(SRC)/*.cpp)
COBJS=$(patsubst $(SRC)/%.c, $(BUILD)/%.o, $(CSRCS))
CPPOBJS=$(patsubst $(SRC)/%.cpp, $(BUILD)/%.o, $(CPPSRCS))
LDFLAGS=-lm -lpthread

all:
	@if ! test -d $(BUILD); \
		then echo "\033[93msetting up build directory...\033[0m"; mkdir -p build;\
  	fi
	@$(MAKE) server

server: $(COBJS)
	$(CC) $(CFLAGS) main.c -o $@ $^ $(LDFLAGS)

$(BUILD)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

# $(BUILD)/%.o: $(SRC)/%.cpp
# 	$(CPP) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) a.out server.out 