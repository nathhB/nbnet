#pragma once

#pragma region Soak_List

typedef struct Soak_ListNode
{
    void *data;
    struct Soak_ListNode *next;
    struct Soak_ListNode *prev;
} Soak_ListNode;

typedef struct Soak_List
{
    Soak_ListNode *head;
    Soak_ListNode *tail;
    unsigned int count;
} Soak_List;

typedef void (*Soak_List_FreeItemFunc)(void *);

Soak_List *Soak_List_Create();
void Soak_List_Destroy(Soak_List *, bool, Soak_List_FreeItemFunc);
void Soak_List_PushBack(Soak_List *, void *);
void *Soak_List_Remove(Soak_List *, void *);
