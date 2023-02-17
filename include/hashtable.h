
#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#include "list.h"

#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct hashtable
    {
        int size;
        int num_entries;
        float load;
        list **bucket;
        int (*hash_fn)(void *data, int data_size, int bucket_count);
    } hashtable;
    hashtable *hashtable_create(int size, int (*hash_fn)(void *, int, int));
    void hashtable_destroy(hashtable *table);
    void *hashtable_put(hashtable *table, char *key, void *data);
    void *hashtable_put_bin(hashtable *table, char *key, void *data);
    void *hashtable_get(hashtable *table, char *key);
    void *hashtable_get_bin(hashtable *table, char *key, int key_size);
    void *hashtable_delete(hashtable *table, char *key);
    void *hashtable_delete_bin(hashtable *table, char *key, int key_size);
    void hashtable_foreach(hashtable *table, void (*fn)(void *, void *), void *arg);
#ifdef __cplusplus
}
#endif

#endif //_HASHTABLE_H_