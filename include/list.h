#ifndef _LIST_H_
#define _LIST_H_

typedef struct list_node{
    void * data;
    struct list_node * next;
    struct list_node * prev;
} node;

typedef struct linked_list{
    node * head;
    node * tail;
    int count;
} list;

extern list *list_create(void);
extern node *list_create_node(void);
extern void *list_get_head(list *list_ptr);
extern void *list_get_tail(list *list_ptr);
extern void *list_insert(list *list_ptr, void *data);
extern void *list_append(list *list_ptr, void *data);
extern void list_destroy(list *list_ptr);
extern int list_length(list *list_ptr);
extern void *list_find(list *list_ptr, void *data, int (*cmp_fn)(void *, void *));
extern void *list_delete(list *list_ptr, void *data, int (*cmp_fn)(void *, void *));
extern void list_foreach(list *list_ptr, void (*fn)(void *, void *), void *arg);
extern void **list_array_get(list *list_ptr);
extern void list_array_free(void **data_array);
#endif //_LIST_H_