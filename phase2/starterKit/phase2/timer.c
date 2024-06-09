
unsigned int getTOD() {
    unsigned int tmp;
    STCK(tmp);
    return tmp;
}

// aggiorna p_time del processo p
void updateCPUtime(pcb_t *p) {
    int end = getTOD();
    p->p_time += (end - start);
    start = end;
}

// funzione che imposta il valore dell'interval timer
void setIntervalTimer(unsigned int t) {
    LDIT(t);
}

// funzione che imposta il valore del processor local timer
void setPLT(unsigned int t) {
    setTIMER(t);
}

// funzione che permette di ottenere il valore del processor local timer
unsigned int getPLT() {
    return getTIMER();
}
