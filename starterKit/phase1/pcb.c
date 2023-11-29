#include "./headers/pcb.h"

static pcb_t pcbTable[MAXPROC];
LIST_HEAD(pcbFree_h);
static int next_pid = 1;

void initPcbs()
{
}

void freePcb(pcb_t *p)
{
}

pcb_t *allocPcb()
{
}

void mkEmptyProcQ(struct list_head *head)
{
}

int emptyProcQ(struct list_head *head)
{
}

void insertProcQ(struct list_head *head, pcb_t *p)
{
    list_add_tail(&p->p_list, head);
}

pcb_t *headProcQ(struct list_head *head)
{
    if (emptyProcQ(head))
        return NULL;
    else
        container_of(head->next, pcb_t, p_list);
}

pcb_t *removeProcQ(struct list_head *head)
{
    if (emptyProcQ(head))
        return NULL;
    pcb_t *ptr_to_dequeue = headProcQ(head);
    list_del(head->next);
    return ptr_to_dequeue;
}
/*
    1)controllo che head appartenga a p->p_list
    2)se non appartiene, ritorno NULL
    3)altrimenti, rimuovo p dalla lista e ritorno p
*/
pcb_t *outProcQ(struct list_head *head, pcb_t *p)
{
    struct list_head *iter;
    list_for_each(iter, &(p->p_list))
    {
        if (iter == head)
        {
            list_del(iter);
            return p;
        }
    }
    return NULL;
}

int emptyChild(pcb_t *p)
{
    return p == NULL || emptyProcQ(&p->p_child);
}

void insertChild(pcb_t *prnt, pcb_t *p)
{
}

pcb_t *removeChild(pcb_t *p)
{
}

pcb_t *outChild(pcb_t *p)
{
}
