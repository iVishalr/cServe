#include <stdio.h>
#include <stdlib.h>
#include "list.h"
#include "utils.h"

list *list_create(void){
    list *list_ptr = (list*)malloc(sizeof(list));
    list_ptr->count = 0;
    list_ptr->head = NULL;
    list_ptr->tail = NULL;
    return list_ptr;
}

node *list_create_node(void){
    node * node_ptr = (node*)malloc(sizeof(node));
    node_ptr->data = NULL;
    node_ptr->next = NULL;
    node_ptr->prev = NULL;
}

/**!
 * @brief Insert at the head of a linked list
 * @param *list_ptr Pointer to list 
*/
void *list_insert(list *list_ptr, void *data){
    if (list_ptr == NULL){
        assert(0, "list_ptr is NULL. Create a list using list_create before inserting data.\n");
        exit(1);
    }

    // create a new node
    node * new_node = list_create_node();

    if (new_node == NULL) return NULL;

    new_node->data = data;

    if (list_ptr->head == NULL){
        list_ptr->head = new_node;
        list_ptr->tail = new_node;
        list_ptr->count++;
        return data;
    }

    new_node->next = list_ptr->head;
    list_ptr->head = new_node;
    list_ptr->head->prev = new_node;
    list_ptr->count++;
    return data;
}

/**!
 * @brief Insert at the tail of a linked list
 * @param *list_ptr Pointer to list 
*/
void *list_append(list *list_ptr, void *data){
    if (list_ptr == NULL){
        assert(0, "list_ptr is NULL. Create a list using list_create before inserting data.\n");
        exit(1);
    }

    node * new_node = list_create_node();

    if (new_node == NULL) return NULL;

    new_node->data = data;

    if (list_ptr->tail == NULL){
        list_ptr->tail = new_node;
        list_ptr->head = new_node;
        list_ptr->count++;
        return data;
    }

    list_ptr->tail->next = new_node;
    new_node->prev = list_ptr->tail;
    list_ptr->tail = new_node;
    list_ptr->count++;
    return data;
}

void list_destroy(list *list_ptr){
    node * temp = list_ptr->head;
    node * delNode = NULL;
    while (temp != NULL){
        delNode = temp;
        temp = temp->next;
        free(delNode);
    }

    free(list_ptr);
    list_ptr = NULL;
    delNode = NULL;
    return;
}

int list_length(list *list_ptr){
    return list_ptr->count;
}

void *list_find(list *list_ptr, void *data, int (*cmp_fn)(void *, void *)){
    node * temp = list_ptr->head;

    if (temp == NULL){
        return NULL;
    }
    
    int flag = 1;
    
    if (cmp_fn(data, temp->data) == 0){
        // head of linkedlist matches with data
        flag = 0;
    }
    
    if (flag){
        temp = list_ptr->tail;
        if (cmp_fn(data, temp->data) == 0){
            // tail of linkedlist matches with data
            flag = 0;
        }
    }

    if (flag){
        temp = list_ptr->head->next;
        while (temp != NULL){
            if (cmp_fn(data, temp->data) == 0){
                break;
            }
            temp = temp->next;
        }
    }

    // now either we have reached the end of linkedlist or we are at the data
    if (temp == NULL){
        return NULL;
    }
    else{
        return temp->data;
    }
}

void *list_delete(list *list_ptr, void *data, int (*cmp_fn)(void *, void *)){
    node * temp = list_ptr->head;
    node * prev = NULL;

    if (cmp_fn(data, temp->data) == 0){
        // delete head node
        void *data = temp->data;

        if (temp->next == NULL){
            list_ptr->head = NULL;
            list_ptr->tail = NULL;
        }
        else{
            list_ptr->head = temp->next;
            temp->next->prev = NULL;
        }

        free(temp);
        temp = NULL;
        list_ptr->count--;
        return data;
    }

    temp = list_ptr->tail;
    prev = temp->prev;

    if (cmp_fn(data, temp->data) == 0){
        // delete tail node
        void *data = temp->data;

        if (prev == NULL){
            list_ptr->head = NULL;
            list_ptr->tail = NULL;
        }
        else{
            prev->next = NULL;
            list_ptr->tail = prev;
        }

        free(temp);
        temp = NULL;
        list_ptr->count--;
        return data;
    }

    // if node to be deleted is not at head or tail of linkedlist.
    temp = list_ptr->head->next;
    prev = list_ptr->head;

    while (temp!=NULL){
        if (cmp_fn(data, temp->data) == 0){
            // we are on the node to delete
            void *data = temp->data;
            prev->next = temp->next;
            temp->next->prev = prev;
            free(temp);
            temp = NULL;
            list_ptr->count--;
            return data;
        }
        else{
            prev = temp;
            temp = temp->next;
        }
    }

    return NULL;
}

void list_foreach(list *list_ptr, void (*fn)(void *, void *), void *arg){
    node * temp = list_ptr->head;
    node * next = NULL;
    while (temp!=NULL){
        next = temp->next; // if fn frees temp, we will lose next node ptr
        fn(temp->data, arg);
        temp = next;
    }
}

void *list_get_head(list *list_ptr){
    if (list_ptr->head == NULL){
        return NULL;
    }

    return list_ptr->head->data;
}

void *list_get_tail(list *list_ptr){
    if (list_ptr->tail == NULL){
        return NULL;
    }

    return list_ptr->tail->data;
}

void **list_array_get(list *list_ptr){
    if (list_ptr->head == NULL){
        return NULL;
    }

    void **data_array = malloc(sizeof(*data_array) * list_ptr->count + 1); //+1 for NULL termination
    node * temp = list_ptr->head;
    int i = 0;

    while (temp!=NULL){
        data_array[i] = temp->data;
        temp = temp->next;
        i++;
    }

    data_array[i] = NULL;
    return data_array;
}

void list_array_free(void **data_array){
	free(data_array);
}
