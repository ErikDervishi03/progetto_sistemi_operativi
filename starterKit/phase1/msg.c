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

void mkEmptyMessageQ(struct list_head *head) {
}

int emptyMessageQ(struct list_head *head) {
}

void insertMessage(struct list_head *head, msg_t *m) {
}

void pushMessage(struct list_head *head, msg_t *m) {
}

msg_t *popMessage(struct list_head *head, pcb_t *p_ptr) {
}

msg_t *headMessage(struct list_head *head) {
}
