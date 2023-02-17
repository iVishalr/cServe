#ifndef _LIST_H_
#define _LIST_H_

#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct list_node
    {
        void *data;
        struct list_node *next;
        struct list_node *prev;
    } node;

    typedef struct linked_list
    {
        node *head;
        node *tail;
        int count;
    } list;
    list *list_create(void);
    node *list_create_node(void);
    void *list_get_head(list *list_ptr);
    void *list_get_tail(list *list_ptr);
    void *list_insert(list *list_ptr, void *data);
    void *list_append(list *list_ptr, void *data);
    void list_destroy(list *list_ptr);
    int list_length(list *list_ptr);
    void *list_find(list *list_ptr, void *data, int (*cmp_fn)(void *, void *));
    void *list_delete(list *list_ptr, void *data, int (*cmp_fn)(void *, void *));
    void list_foreach(list *list_ptr, void (*fn)(void *, void *), void *arg);
    void **list_array_get(list *list_ptr);
    void list_array_free(void **data_array);
#ifdef __cplusplus
}
#endif

#endif //_LIST_H_