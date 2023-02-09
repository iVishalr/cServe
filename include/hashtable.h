#include "list.h"

#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

typedef struct hashtable{
    int size;
    int num_entries;
    float load;
    list **bucket;
    int (*hash_fn)(void *data, int data_size, int bucket_count);
} hashtable;

extern hashtable *hashtable_create(int size, int (*hash_fn)(void *, int, int));
extern void hashtable_destroy(hashtable *table);
extern void *hashtable_put(hashtable *table, char *key, void *data);
extern void *hashtable_put_bin(hashtable *table, void *key, int key_size, void *data);
extern void *hashtable_get(hashtable *table, char *key);
extern void *hashtable_get_bin(hashtable *table, void *key, int key_size);
extern void *hashtable_delete(hashtable *table, char *key);
extern void *hashtable_delete_bin(hashtable *table, void *key, int key_size);
extern void hashtable_foreach(hashtable *table, void (*fn)(void *, void *), void *arg);

#endif //_HASHTABLE_H_