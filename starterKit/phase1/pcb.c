#include "./headers/pcb.h"

static pcb_t pcbTable[MAXPROC];
LIST_HEAD(pcbFree_h);
static int next_pid = 1;

/*
    Inizializza la lista pcb
*/
void initPcbs() {
  INIT_LIST_HEAD (&pcbFree_h);
  for (int i = 0; i<MAXPROC; i++)
    {
        list_add (&pcbTable[i].p_list, &pcbFree_h);
    }
}

/*
    inserisce l'elemento puntato da p nella lista pcbFree in testa
*/
void freePcb(pcb_t *p) {
    list_add (&p->p_list, &pcbFree_h);
}

/*
    ritorna NULL se la pcbFree list è vuota, sennò rimuovi
    un elemento dalla lista e ritorna un puntatore all'elem rimosso
*/
pcb_t *allocPcb() {
    if (list_empty (&pcbFree_h))
        return NULL;
    else
    {
        pcb_t *new_pcb = container_of(pcbFree_h.next, pcb_t, p_list);
        list_del(pcbFree_h.next);
        new_pcb->p_parent = NULL;
        new_pcb->p_supportStruct = NULL;
        new_pcb->p_pid = next_pid++;
        new_pcb->p_time = 0;
        INIT_LIST_HEAD(&new_pcb->p_child);
        INIT_LIST_HEAD(&new_pcb->p_sib);
        INIT_LIST_HEAD(&new_pcb->msg_inbox);
        return new_pcb;
    }  
}

/*
    Inizializza una variabile come puntatore alla testa di una coda di pcb vuota
*/
void mkEmptyProcQ(struct list_head *head) {
    INIT_LIST_HEAD (head);
}

/*
    Verifica se una coda di pcb è vuota
*/
int emptyProcQ(struct list_head *head) {

    return list_empty (head);

}

/*
    Inserisce un elemento alla fine della coda di pcb
*/
void insertProcQ(struct list_head *head, pcb_t *p)
{
    list_add_tail(&p->p_list, head);
}

/*
    dato un puntatore list_head, restituisce il puntatore all'istanza della struttura che lo contiene
    se la lista è vuota, ritorna NULL
*/
pcb_t *headProcQ(struct list_head *head)
{
    if (emptyProcQ(head))
        return NULL;
    else
        return container_of(head->next, pcb_t, p_list);

}

/*
    rimuovi il primo elemento della coda di processi puntata da head e ritornalo
    se la coda è vuota, ritorna NULL
*/
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
    if (emptyProcQ(head))
        return NULL;
    struct list_head *pos;
    list_for_each(pos, head){
        pcb_t *current= container_of(pos,pcb_t, p_list);
        if(current == p){
            list_del(pos);
            return p;
        }
    }
    return NULL;
}

/*
    retorna true se il pcb puntato da p non ha figli, false altrimenti
*/
int emptyChild(pcb_t *p)
{
    return p == NULL || emptyProcQ(&p->p_child);
}

//rendi la pcb puntata da p figlia del pcb puntato da prnt
void insertChild(pcb_t *prnt, pcb_t *p) {
    //inserisco in coda
    list_add_tail(&p->p_sib, &prnt->p_child);
    p->p_parent=prnt;
}

//rimuovi il primo figlio della pcb puntata da p e ritornalo, se p non ha figli ritorna NULL 
pcb_t *removeChild(pcb_t *p) {
    if(emptyProcQ(&p->p_child))
        return NULL;
    else{
        //salvo figlio prima di rimuoverlo per poi returnarlo
        pcb_t* C= container_of(p->p_child.next, pcb_t, p_sib);
        //rimozione figlio dalla lista
        list_del(p->p_child.next);
        return C;
    }
}

//rendi la pcb puntata da p non più figlia di suo padre e ritornalo, se p non ha padre ritorna NULL
pcb_t *outChild(pcb_t *p) {
    if(p->p_parent==NULL)
        return NULL;
    else{
        struct list_head *pos;
        list_for_each(pos, &p->p_parent->p_child){
            pcb_t *current= container_of(pos,pcb_t, p_sib);
            if(current == p){
                list_del(pos);
                return p;
            }
        }
        return NULL;
    }
}
