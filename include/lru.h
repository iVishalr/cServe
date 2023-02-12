#ifndef _LRU_H_
#define _LRU_H_

#include "hashtable.h"

typedef struct cache_node{
    char *key; // key of endpoint. Acts as a key for cache
    char *content_type;
    int content_length;
    void *content;

    struct cache_node * next;
    struct cache_node * prev;
} cache_node;

typedef struct lru{
    hashtable *table;
    cache_node * head;
    cache_node * tail;
    int max_size; 
    int current_size;
}lru;

extern cache_node *allocate_node(char *key, char *content_path, void *content, int content_length);
extern void free_cache_node(cache_node *node);
extern lru *lru_create(int max_size, int hashsize);
extern void destroy_cache(lru *lru_cache);
extern cache_node *cache_put(lru *lru_cache, char *key, char *content_type, void *content, int content_length);
extern cache_node *cache_get(lru *lru_cache, char *key);

#endif 