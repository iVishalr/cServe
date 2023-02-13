#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hashtable.h"

#define DEFAULT_SIZE 128
#define DEFAULT_GROW_FACTOR 2

typedef struct ht_entry{
    char *key;
    int key_size;
    int hashed_key;
    void *data;
} ht_entry;

void add_entry_count(hashtable *table, int d){
    table->num_entries += d;
    table->load = (float)table->num_entries / table->size;
}

int default_hash_fn(void *data, int data_size, int bucket_count){
    const int R = 31;
    int hash = 0;
    unsigned char *p = data;

    for (int i=0; i<data_size; i++){
        hash = (R * hash + p[i]) % bucket_count;
    }

    return hash;
}

hashtable *hashtable_create(int size, int (*hash_fn)(void *, int, int)){
    if (size < 1){
        size = DEFAULT_SIZE;
    }

    if (hash_fn == NULL){
        hash_fn = default_hash_fn;
    }

    hashtable *table = (hashtable*)malloc(sizeof(hashtable));

    if (table == NULL){
        fprintf(stderr, "hashtable: Error allocating memory to table.\n");
        return NULL;
    }

    table->size = size;
    table->num_entries = 0;
    table->load = 0.0f;
    table->bucket = (list**)malloc(sizeof(list*)*size);

    if (table->bucket == NULL){
        fprintf(stderr, "hashtable->bucket: Error allocating memory to table.\n");
        return NULL;
    }

    table->hash_fn = hash_fn;

    for (int i=0; i<size; i++){
        table->bucket[i] = list_create();
        if (table->bucket[i] == NULL){
            fprintf(stderr, "table->bucket[%d]: Could not create linked list for this bucket.\n", i+1);
            return NULL;
        }
    }

    return table;
}

void *hashtable_put(hashtable *table, char *key, void *data){
    return hashtable_put_bin(
        table, key, data
    );
}

void *hashtable_put_bin(hashtable *table, char *key, void *data){
    /*
    * Hash the key passed to the function using hash_fn of hashtable
    * Index into the array of linkedlists to obtain the correct bin
    * Create a Hash Table entry structure and append it to the selected linkedlist
    * Update Hash Table count by 1
    */
    int key_size = strlen(key);
    printf("KEY = %s LEN=%d\n", key, key_size);
    int index = table->hash_fn(key, key_size, table->size);

    list * list_ptr = table->bucket[index];

    ht_entry * entry = (ht_entry*)malloc(sizeof(ht_entry));

    entry->key = (char*)malloc(key_size*sizeof(char));
    sprintf(entry->key, "%s", key);
    entry->key_size = key_size;
    entry->hashed_key = index;
    entry->data = data;

    printf("*%s*\n", entry->key);
    printf("hashtable_put_bin: key=%s hashed_key=%d\n", entry->key, entry->hashed_key);

    printf("Num elements in the list - %d\n",list_ptr->count);

    if (list_append(list_ptr, entry) == NULL){
        printf("Error inserting to linked list.\n");
        free(entry->key);
        entry->key = NULL;
        free(entry);
        entry = NULL;
        return NULL;
    }

    printf("adding entry count\n");

    add_entry_count(table, 1);
    return data;
}

int hash_entry_cmp_fn(void *a, void *b){
    ht_entry *A = a;
    ht_entry *B = b;
    int size_diff = B->key_size - A->key_size;
    if (size_diff) return size_diff;

    return memcmp(A->key, B->key, A->key_size);
}

void *hashtable_get(hashtable *table, char *key){
    return hashtable_get_bin(
        table, key, strlen(key)
    );
}

void *hashtable_get_bin(hashtable *table, char *key, int key_size){
    int index = table->hash_fn(key, key_size, table->size);
    list *list_ptr = table->bucket[index];

    ht_entry cmp_entry;
    cmp_entry.key = key;
    cmp_entry.key_size = key_size;

    ht_entry *temp = list_find(
        list_ptr, &cmp_entry, hash_entry_cmp_fn
    );

    if (temp == NULL){
        return NULL;
    }
    else{
        return temp->data;
    }
}

void *hashtable_delete(hashtable *table, char *key){
    return hashtable_delete_bin(
        table, key, strlen(key)
    );
}

void *hashtable_delete_bin(hashtable *table, char *key, int key_size){
    int index = table->hash_fn(key, key_size, table->size);
    list *list_ptr = table->bucket[index];

    ht_entry cmp_entry;
    cmp_entry.key = key;
    cmp_entry.key_size = key_size;

    ht_entry *temp = list_delete(
        list_ptr, &cmp_entry, hash_entry_cmp_fn
    );

    if (temp == NULL){
        return NULL;
    }
    void *data = temp->data;
    free(temp);
    temp = NULL;
    add_entry_count(table, -1);
    return data;
}

struct foreach_callback_payload{
    void *arg;
    void (*fn)(void *, void *);
};

void ht_entry_free(void *ht_entry, void *arg){
    (void)arg;
    free(ht_entry);
}

void hashtable_destroy(hashtable *table){
    for (int i = 0; i<table->size; i++){
        list *list_ptr = table->bucket[i];
        list_foreach(list_ptr, ht_entry_free, NULL);
        list_destroy(list_ptr);
        list_ptr = NULL;
    }
    free(table);
}

void foreach_callback(void *vent, void *vpayload){
	ht_entry *entry = vent;
	struct foreach_callback_payload *payload = vpayload;

	payload->fn(entry->data, payload->arg);
}

void hashtable_foreach(hashtable *table, void (*fn)(void *, void *), void *arg){
    struct foreach_callback_payload payload;
    payload.fn = fn;
    payload.arg = arg;

    for (int i = 0; i < table->size; i++){
        list *list_ptr = table->bucket[i];
        list_foreach(list_ptr, foreach_callback, &payload);
    }
}