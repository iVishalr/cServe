#ifndef _LRU_H_
#define _LRU_H_

#include <pthread.h>
#include "hashtable.h"

#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct cache_node
    {
        char *key; // key of endpoint. Acts as a key for cache
        char *content_type;
        int content_length;
        void *content;

        struct cache_node *next;
        struct cache_node *prev;
    } cache_node;

    typedef struct lru
    {
        hashtable *table;
        cache_node *head;
        cache_node *tail;
        int max_size;
        int current_size;
        pthread_mutex_t mutex;
    } lru;
    cache_node *allocate_node(char *key, char *content_path, void *content, int content_length);
    void free_cache_node(cache_node *node);
    lru *lru_create(int max_size, int hashsize);
    void destroy_cache(lru *lru_cache);
    cache_node *cache_put(lru *lru_cache, char *key, char *content_type, void *content, int content_length);
    cache_node *cache_get(lru *lru_cache, char *key);
    void cache_print(lru *lru_cache);
#ifdef __cplusplus
}
#endif

#endif