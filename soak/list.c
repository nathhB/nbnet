#include <stdlib.h>
#include <stdbool.h>

#include "list.h"

static Soak_ListNode *Soak_List_CreateNode(void *);
static void *Soak_List_RemoveNodeFromList(Soak_List *, Soak_ListNode *);

Soak_List *Soak_List_Create()
{
    Soak_List *list = malloc(sizeof(Soak_List));

    if (list == NULL)
        return NULL;

    list->head = NULL;
    list->tail = NULL;
    list->count = 0;

    return list;
}

void Soak_List_Destroy(Soak_List *list, bool free_items, Soak_List_FreeItemFunc free_item_func)
{
    Soak_ListNode *current_node = list->head;

    while (current_node != NULL)
    {
        Soak_ListNode *next_node = current_node->next;

        if (free_items)
        {
            if (free_item_func)
                free_item_func(current_node->data);
            else
                free(current_node->data);
        }

        free(current_node);

        current_node = next_node;
    }

    free(list);
}

void Soak_List_PushBack(Soak_List *list, void *data)
{
    Soak_ListNode *node = Soak_List_CreateNode(data);

    if (list->count == 0)
    {
        node->next = NULL;
        node->prev = NULL;

        list->head = node;
        list->tail = node;
    }
    else
    {
        node->next = NULL;
        node->prev = list->tail;

        list->tail->next = node;
        list->tail = node;
    }

    list->count++;
}

void *Soak_List_Remove(Soak_List *list, void *data)
{
    Soak_ListNode *current_node = list->head;

    for (int i = 0; current_node != NULL && current_node->data != data; i++)
        current_node = current_node->next;

    if (current_node != NULL)
    {
        return Soak_List_RemoveNodeFromList(list, current_node);
    }

    return NULL;
}

static Soak_ListNode *Soak_List_CreateNode(void *data)
{
    Soak_ListNode *node = malloc(sizeof(Soak_ListNode));

    node->data = data;

    return node;
}

static void *Soak_List_RemoveNodeFromList(Soak_List *list, Soak_ListNode *node)
{
    if (node == list->head)
    {
        Soak_ListNode *new_head = node->next;

        if (new_head != NULL)
            new_head->prev = NULL;
        else
            list->tail = NULL;

        list->head = new_head;

        void *data = node->data;

        free(node);
        list->count--;

        return data;
    }

    if (node == list->tail)
    {
        Soak_ListNode *new_tail = node->prev;

        new_tail->next = NULL;
        list->tail = new_tail;

        void *data = node->data;

        free(node);
        list->count--;

        return data;
    }

    node->prev->next = node->next;
    node->next->prev = node->prev;

    void *data = node->data;

    free(node);
    list->count--;

    return data;
}
