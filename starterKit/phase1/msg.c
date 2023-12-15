#include "./headers/msg.h"

static msg_t msgTable[MAXMESSAGES];
LIST_HEAD(msgFree_h);

void initMsgs() {
}

//inserisci l'elemento puntato da m nella lista msgFree (lo metto in testa)
void freeMsg(msg_t *m) {
    list_del(&m->m_list);
    list_add_tail(&m->m_list,&msgFree_h);
}
//ritorna NULL se la msgFree list è vuota, sennò rimuovi
//un elemento dalla lista (assicurati di dare un valore a tutti 
//i campi del messaggio e poi ritorna un puntatore all'elem rimosso)
msg_t *allocMsg() {
    if(msgFree_h==NULL)
        return NULL;
    else{
        msg_t *tmp=container_of(msgFree_h.next,msg_t,*msg_PTR);
        list_del(msgFree_h.next);
        list_add(&tmp->m_list,&msgTable);
        return tmp;
    }
}

// Inizializza una variabile come puntatore alla testa di una coda di messaggi vuota
void mkEmptyProcQ(struct list_head *head) {
    INIT_LIST_HEAD(head);
}

// Verifica se una coda di messaggi è vuota
int emptyMessageQ(struct list_head *head) {
    return list_empty(head);
}

// Inserisce un messaggio alla fine (coda) di una coda di messaggi
void insertMessage(struct list_head *head, msg_t *m) {
    list_add_tail(&m->m_list, head);
}

// Inserisce un messaggio all'inizio di una coda di messaggi
void pushMessage(struct list_head *head, msg_t *m) {
    list_add(&m->m_list, head);
}

// Rimuove e restituisce il primo messaggio dalla coda, eventualmente corrispondente al mittente
msg_t *popMessage(struct list_head *head, pcb_t *p_ptr) {
    if (list_empty(head)) {
        return NULL; // Restituisce NULL se la coda dei messaggi è vuota
    }

    struct list_head *pos;
    msg_t *msg;

    // Itera attraverso la coda dei messaggi
    list_for_each(pos, head) {
        msg = container_of(pos, msg_t, m_list);

        // Verifica se il mittente corrisponde a p_ptr (se p_ptr non è NULL)
        if (p_ptr == NULL || msg->m_sender == p_ptr) {
            list_del(&msg->m_list); // Rimuove il messaggio dalla coda
            INIT_LIST_HEAD(&msg->m_list); // Inizializza il nodo della lista
            return msg;
        }
    }

    return NULL; // Restituisce NULL se nessun messaggio da p_ptr è stato trovato
}


msg_t *headMessage(struct list_head *head) {
}
