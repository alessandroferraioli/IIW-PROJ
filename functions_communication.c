#include <sys/time.h>
#include "basic.h"
#include "manage_io.h"
#include "timer_functions.h"
#include "Server.h"
#include "Client.h"
#include "functions_communication.h"
#include "dynamic_list.h"
//commentare il primo o secondo blocco di funzioni print
// in modo tale da avere o non avere le stampe relative all'invio o alla ricezione dei pacchetti


void print_rcv_message(struct temp_buf temp_buff){
    (void)temp_buff;
    return;
}
void print_msg_sent(struct temp_buf temp_buff){
    (void)temp_buff;
    return;
}
void print_msg_resent(struct temp_buf temp_buff){
    (void)temp_buff;
    return;
}
void print_msg_sent_and_lost(struct temp_buf temp_buff){
    (void)temp_buff;
    return;
}
void print_msg_resent_and_lost(struct temp_buf temp_buff){
    (void)temp_buff;
    return;
}

/*void print_rcv_message(struct temp_buf temp_buff){
    printf("pacchetto ricevuto con ack %d seq %d command %d lap %d\n", temp_buff.ack,
           temp_buff.seq,
           temp_buff.command, temp_buff.lap);
    return;
}
void print_msg_sent(struct temp_buf temp_buff){
    printf(CYAN"pacchetto inviato con ack %d seq %d command %d lap %d\n"RESET, temp_buff.ack, temp_buff.seq,
           temp_buff.command, temp_buff.lap);
    return;
}
void print_msg_resent(struct temp_buf temp_buff){
    printf(YELLOW"pacchetto ritrasmesso con ack %d seq %d command %d lap %d\n"RESET, temp_buff.ack, temp_buff.seq,
           temp_buff.command, temp_buff.lap);
    return;
}
void print_msg_sent_and_lost(struct temp_buf temp_buff){
    printf(BLUE"pacchetto con ack %d, seq %d command %d lap %d perso\n"RESET, temp_buff.ack, temp_buff.seq,
           temp_buff.command, temp_buff.lap);
    return;
}
void print_msg_resent_and_lost(struct temp_buf temp_buff){
    printf(YELLOW"pacchetto ritrasmesso con ack %d, seq %d command %d lap %d perso\n" RESET, temp_buff.ack,
           temp_buff.seq, temp_buff.command, temp_buff.lap);
    return;
}*/

//funzioni per la comunicazione tra 2 host senza protocollo selective repeat

//inizializza pacchetto e manda messaggio
void send_message(int sockfd, struct sockaddr_in *address
, socklen_t len, struct temp_buf temp_buff, char *data,
                  char command, double loss_prob) {
    better_strcpy(temp_buff.payload, data);
    temp_buff.ack = NOT_AN_ACK;
    temp_buff.seq = NOT_A_PKT;
    temp_buff.command = command;
    temp_buff.lap = NO_LAP;
    //niente ack e sequenza
    if (flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) address
, len) ==
            -1) {
            handle_error_with_exit("error in sendto\n");
        }
        print_msg_sent(temp_buff);
    } else {
        print_msg_sent_and_lost(temp_buff);
    }
    return;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//ritrasmetti messaggio precedentemente inviato
void resend_message(int sockfd, struct temp_buf *temp_buff, struct sockaddr_in *address
, socklen_t len, double loss_prob) {
    if (flip_coin(loss_prob)) {
        if (sendto(sockfd, temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) address
, len) ==
            -1) {
            handle_error_with_exit("error in sendto\n");
        }
        print_msg_resent(*temp_buff);
    } else {
       print_msg_resent_and_lost(*temp_buff);
    }
    return;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//manda messaggio di syn
void send_syn(int sockfd, struct sockaddr_in *serv_addr, socklen_t len, double loss_prob) {
    struct temp_buf temp_buff;
    temp_buff.seq = NOT_A_PKT;
    temp_buff.command = SYN;
    temp_buff.ack = NOT_AN_ACK;
    temp_buff.lap = NO_LAP;
    better_strcpy(temp_buff.payload, "SYN");
    if (flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) ==
            -1) {
            handle_error_with_exit("error in syn sendto\n");
        }
    }
    return;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//manda messaggio di syn ack
void send_syn_ack(int sockfd, struct sockaddr_in *serv_addr, socklen_t len, double loss_prob) {
    struct temp_buf temp_buff;
    temp_buff.seq = NOT_A_PKT;
    temp_buff.command = SYN_ACK;
    temp_buff.ack = NOT_AN_ACK;
    temp_buff.lap = NO_LAP;
    better_strcpy(temp_buff.payload, "SYN_ACK");
    if (flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) ==
            -1) {
            handle_error_with_exit("error in sendto\n");
        }
    }
    return;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//funzioni per la comunicazione tra 2 host con protocollo selective repeat

    //manda parte di lista al receiver usando protocollo selective repeat:
    //imposta riscontrato =0;
    //incrementa pkt_fly(pkt fly deve essere sempre compreso tra 0 e w-1)
    //salva la lista in finestra (per poi eventualmente ritrasmetterla)
    //aumenta byte_sent(byte sent deve essere <=dimensione lista)
    //inserisci le informazioni del pacchetto in una lista dinamica,
    // cosi il thread ritrasmettitore sa quanto aspettare e se ritrasmettere
void send_list_in_window(struct temp_buf temp_buff,struct sel_repeat *shm) {
    temp_buff.command = DATA;
    temp_buff.ack = NOT_AN_ACK;
    temp_buff.seq = shm->seq_to_send;
    lock_mtx(&(shm->mtx));
    shm->win_buf_snd[shm->seq_to_send].acked = 0;

    if ((shm->dimension - (shm->byte_sent)) < (int)(MAXPKTSIZE - OVERHEAD)) {//byte mancanti da inviare
        copy_buf2_in_buf1(temp_buff.payload, shm->list, MAXPKTSIZE - OVERHEAD);
        copy_buf2_in_buf1(shm->win_buf_snd[shm->seq_to_send].payload, shm->list, MAXPKTSIZE - OVERHEAD);
        shm->byte_sent += (shm->dimension - (shm->byte_sent));
        shm->list += shm->dimension - (shm->byte_sent);
    } else {
        copy_buf2_in_buf1(temp_buff.payload, shm->list, (MAXPKTSIZE - OVERHEAD));
        shm->byte_sent += (MAXPKTSIZE - OVERHEAD);
        copy_buf2_in_buf1(shm->win_buf_snd[shm->seq_to_send].payload, shm->list, (MAXPKTSIZE - OVERHEAD));
        shm->list += (MAXPKTSIZE - OVERHEAD);
    }

    shm->win_buf_snd[shm->seq_to_send].command = DATA;
    (shm->win_buf_snd[shm->seq_to_send].lap) += 1;
    temp_buff.lap = (shm->win_buf_snd[shm->seq_to_send].lap);

    if (clock_gettime(CLOCK_MONOTONIC, &(shm->win_buf_snd[shm->seq_to_send].time)) != 0) {
        handle_error_with_exit("error in get_time\n");
    }
    //Inserisco in lista il pkt aggiunto alla finestra 
    insert_ordered(shm->seq_to_send, shm->win_buf_snd[shm->seq_to_send].lap, shm->win_buf_snd[shm->seq_to_send].time, shm->param.timer_ms,
                   &(shm->head), &(shm->tail));
    //lista non piu vuota-->rilascio la condizione sul thread
    unlock_thread_on_a_condition(&(shm->list_not_empty));

    //invio pkt
    if (flip_coin(shm->param.loss_prob)) {
        if (sendto(shm->address
.sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) &(shm->address
.dest_addr), shm->address
.len) ==
            -1) {
            handle_error_with_exit("error in sendto\n");
        }
        print_msg_sent(temp_buff);
    } else {
        print_msg_sent_and_lost(temp_buff);
    }
    shm->seq_to_send = ((shm->seq_to_send) + 1) % (2 * shm->param.window);
    (shm->pkt_fly)++;
    unlock_mtx(&(shm->mtx));
    return;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//manda parte di file al receiver usando protocollo selective repeat
    //imposta riscontrato =0;
    //incrementa pkt_fly(pkt fly deve essere sempre compreso tra 0 e w-1)
    //salva il file in finestra (per poi eventualmente ritrasmetterla)
    //aumenta byte_sent(byte sent deve essere <=dimensione lista)
    //inserisci le informazioni del pacchetto in una lista dinamica,
    //cosi il thread ritrasmettitore sa quanto aspettare e se ritrasmettere
void send_data_in_window(struct temp_buf temp_buff,struct sel_repeat *shm) {
   
    ssize_t readed = 0;
    temp_buff.command = DATA;
    temp_buff.ack = NOT_AN_ACK;
    temp_buff.seq = shm->seq_to_send;
    lock_mtx(&(shm->mtx));
    shm->win_buf_snd[shm->seq_to_send].acked = 0;
    
    //leggo cosa inviare 
    if ((shm->dimension - (shm->byte_sent)) < (int)(MAXPKTSIZE - OVERHEAD)) {//byte mancanti da inviare
        readed = read_nbytes(shm->fd, temp_buff.payload, (size_t) (shm->dimension - (shm->byte_sent)));
        if (readed < shm->dimension - (shm->byte_sent)) {
            handle_error_with_exit("error in read 2\n");
        }
        shm->byte_sent += (shm->dimension - (shm->byte_sent));
    } else {
        readed = read_nbytes(shm->fd, temp_buff.payload, (MAXPKTSIZE - OVERHEAD));
        if (readed < (int)(MAXPKTSIZE - OVERHEAD)) {
            handle_error_with_exit("error in read 3\n");
        }
        shm->byte_sent += (MAXPKTSIZE - OVERHEAD);
    }
    //copio nel buffer 
    copy_buf2_in_buf1(shm->win_buf_snd[shm->seq_to_send].payload, temp_buff.payload, (MAXPKTSIZE - OVERHEAD));
    shm->win_buf_snd[shm->seq_to_send].command = DATA;
    (shm->win_buf_snd[shm->seq_to_send].lap) += 1;
    temp_buff.lap = (shm->win_buf_snd[shm->seq_to_send].lap);
    if (clock_gettime(CLOCK_MONOTONIC, &(shm->win_buf_snd[shm->seq_to_send].time)) != 0) {
        handle_error_with_exit("error in get_time\n");
    }

    //inserisco in lista cosa ho inviato cosi posso gestire eventuali rtx
    insert_ordered(shm->seq_to_send, shm->win_buf_snd[shm->seq_to_send].lap, shm->win_buf_snd[shm->seq_to_send].time, shm->param.timer_ms,
                   &(shm->head), &(shm->tail));
    unlock_thread_on_a_condition(&(shm->list_not_empty));//libero la condizione sulla lista--> non piu vuota 

    //invio pkt
    if (flip_coin(shm->param.loss_prob)) {
        if (sendto(shm->address
.sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) &(shm->address
.dest_addr), shm->address
.len) ==
            -1) {
            handle_error_with_exit("error in sendto\n");
        }
        print_msg_sent(temp_buff);
    } else {
        print_msg_sent_and_lost(temp_buff);
    }
    shm->seq_to_send = ((shm->seq_to_send) + 1) % (2 * shm->param.window);
    (shm->pkt_fly)++;
    unlock_mtx(&(shm->mtx));
    return;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//manda messaggio(non list non file)al receiver usando protocollo selective repeat
    //imposta riscontrato =0;
    //incrementa pkt_fly(pkt fly deve essere sempre compreso tra 0 e w-1)
    //salva il messaggio in finestra (per poi eventualmente ritrasmetterla)
    //aumenta byte_sent(byte sent deve essere <=dimensione lista)
    //inserisci le informazioni del pacchetto in una lista dinamica,
    // cosi il thread ritrasmettitore sa quanto aspettare e se ritrasmettere
void send_message_in_window(struct temp_buf temp_buff,struct sel_repeat *shm,char command,char*message) {
    //prepara pacchetto da inviare
    temp_buff.command = command;
    temp_buff.ack = NOT_AN_ACK;
    temp_buff.seq = shm->seq_to_send;
    better_strcpy(temp_buff.payload, message);

    //prendo il mtx sulla shared memory sel reapet-->ci lavoro indisturbato
    lock_mtx(&(shm->mtx));

    //copia in finestra il pacchetto cosi puoi ritrasmetterlo
    copy_buf2_in_buf1(shm->win_buf_snd[shm->seq_to_send].payload, temp_buff.payload, MAXPKTSIZE - OVERHEAD);
    
    //segno come ancora non riscontrato
    shm->win_buf_snd[shm->seq_to_send].command = command;
    shm->win_buf_snd[shm->seq_to_send].acked = 0;
    
    if (clock_gettime(CLOCK_MONOTONIC, &(shm->win_buf_snd[shm->seq_to_send].time)) != 0) {
        handle_error_with_exit("error in get_time\n");
    }
    
    (shm->win_buf_snd[shm->seq_to_send].lap) += 1;
    temp_buff.lap = shm->win_buf_snd[shm->seq_to_send].lap;
    
    //inserisci le informazioni del pacchetto in una lista dinamica ordinata in base al tempo 
    insert_ordered(shm->seq_to_send, shm->win_buf_snd[shm->seq_to_send].lap, shm->win_buf_snd[shm->seq_to_send].time, shm->param.timer_ms,
                   &(shm->head), &(shm->tail));
    //rilascio la cond sul thread della lista non vuota 
    unlock_thread_on_a_condition(&(shm->list_not_empty));

    //invio il pkt 
    if (flip_coin(shm->param.loss_prob)) {
        if (sendto(shm->address
.sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) &shm->address
.dest_addr,shm->address
.len) ==
            -1) {
            handle_error_with_exit("error in sendto\n");
        }
        print_msg_sent(temp_buff);
    } else {
        print_msg_sent_and_lost(temp_buff);
    }

    //incrementa seq_to_send e pkt_fly
    shm->seq_to_send = ((shm->seq_to_send) + 1) % (2 *shm->param.window);
    (shm->pkt_fly)++;


    unlock_mtx(&(shm->mtx));
    return;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//riceve un messaggio già ricevuto rimanda semplicemente l'ack del messaggio
void rcv_msg_re_send_ack_in_window(struct temp_buf temp_buff,struct sel_repeat *shm) {
    //il messaggio era già memorizzato in finestra,rinvia ack
    temp_buff.ack = temp_buff.seq;
    temp_buff.seq = NOT_A_PKT;
    better_strcpy(temp_buff.payload, "ACK");
    //lascia invariato il tipo di comando e il lap
    if (flip_coin(shm->param.loss_prob)) {
        if (sendto(shm->address
.sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) &shm->address
.dest_addr, shm->address
.len) ==
            -1) {
            handle_error_with_exit("error in sendto\n");
        }
        print_msg_sent(temp_buff);
    } else {
        print_msg_sent_and_lost(temp_buff);
    }
    return;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//riceve parte di lista e invia il corrispondente ack
void rcv_list_send_ack_in_window(struct temp_buf temp_buff,struct sel_repeat *shm) {
    struct temp_buf ack_buff;
    if (shm->win_buf_rcv[temp_buff.seq].received == 0) {
        if ((shm->win_buf_rcv[temp_buff.seq].lap) == (temp_buff.lap - 1)) {
            shm->win_buf_rcv[temp_buff.seq].lap = temp_buff.lap;
            shm->win_buf_rcv[temp_buff.seq].command = temp_buff.command;
            copy_buf2_in_buf1(shm->win_buf_rcv[temp_buff.seq].payload, temp_buff.payload, (MAXPKTSIZE - OVERHEAD));
            shm->win_buf_rcv[temp_buff.seq].received = 1;
        } else {
            //ignora pacchetto
        }
    }
    ack_buff.ack = temp_buff.seq;
    ack_buff.seq = NOT_A_PKT;
    better_strcpy(ack_buff.payload, "ACK");
    ack_buff.command = DATA;
    ack_buff.lap = temp_buff.lap;
    if (flip_coin(shm->param.loss_prob)) {
        if (sendto(shm->address
.sockfd, &ack_buff, MAXPKTSIZE, 0, (struct sockaddr *) &shm->address
.dest_addr, shm->address
.len) ==
            -1) {
            handle_error_with_exit("error in sendto\n");
        }
        print_msg_sent(ack_buff);
    } else {
        print_msg_sent_and_lost(ack_buff);
    }
    if (temp_buff.seq == shm->window_base_rcv) {//se pacchetto riempie un buco
        // scorro la finestra fino al primo ancora non ricevuto
        while (shm->win_buf_rcv[shm->window_base_rcv].received == 1) {
            if (shm->dimension - shm->byte_written >= (int)(MAXPKTSIZE - OVERHEAD)) {
                copy_buf2_in_buf1(shm->list, temp_buff.payload,
                                  (MAXPKTSIZE - OVERHEAD));//scrivo in list la parte di lista
                shm->byte_written += (MAXPKTSIZE - OVERHEAD);
                shm->list += (MAXPKTSIZE - OVERHEAD);
            } else {
                copy_buf2_in_buf1(shm->list, temp_buff.payload, shm->dimension - shm->byte_written);
                shm->byte_written += shm->dimension - shm->byte_written;
                shm->list += shm->dimension - shm->byte_written;
            }
            shm->win_buf_rcv[shm->window_base_rcv].received = 0;//segna pacchetto come non ricevuto
            shm->window_base_rcv = ((shm->window_base_rcv) + 1) % (2 * shm->param.window);//avanza la finestra con modulo di 2W
        }
    }
    return;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//riceve parte di file e invia il corrispondente ack
//segna file in finestra,segna che è stato ricevuto e verifica se la finestra può essere traslata
void rcv_data_send_ack_in_window(struct temp_buf temp_buff,struct sel_repeat *shm) {
    struct temp_buf ack_buff;
    int written = 0;
    if (shm->win_buf_rcv[temp_buff.seq].received == 0) {//pkt ricevuto non ancora riscontrato
        if ((shm->win_buf_rcv[temp_buff.seq].lap) == (temp_buff.lap - 1)) {
            shm->win_buf_rcv[temp_buff.seq].lap = temp_buff.lap;
            shm->win_buf_rcv[temp_buff.seq].command = temp_buff.command;
            copy_buf2_in_buf1(shm->win_buf_rcv[temp_buff.seq].payload, temp_buff.payload,
                              (MAXPKTSIZE - OVERHEAD));
            shm->win_buf_rcv[temp_buff.seq].received = 1;
        } else {
            //ignora pacchetto
        }
    }
    ack_buff.ack = temp_buff.seq;
    ack_buff.seq = NOT_A_PKT;
    better_strcpy(ack_buff.payload, "ACK");
    ack_buff.command = DATA;
    ack_buff.lap = temp_buff.lap;

    //invio ack
    if (flip_coin(shm->param.loss_prob)) {
        if (sendto(shm->address
.sockfd, &ack_buff, MAXPKTSIZE, 0, (struct sockaddr *) &(shm->address
.dest_addr), shm->address
.len) ==
            -1) {
            handle_error_with_exit("error in sendto\n");
        }
        print_msg_sent(ack_buff);
    } else {
        print_msg_sent_and_lost(ack_buff);
    }


    if (temp_buff.seq == shm->window_base_rcv) {//se pacchetto riempie un buco

  
        while (shm->win_buf_rcv[shm->window_base_rcv].received == 1) {//scorro la finestra fino al primo non ricevuto

            //per ogni elemento della finestra ricevuto 
            if (shm->dimension - shm->byte_written >= (int)(MAXPKTSIZE - OVERHEAD)) {
                written = (int) write_nbytes(shm->fd, shm->win_buf_rcv[shm->window_base_rcv].payload, (MAXPKTSIZE - OVERHEAD));
                if (written < (int)(MAXPKTSIZE - OVERHEAD)) {
                    handle_error_with_exit("error in write\n");
                }
                //aggiorno quanto ho scritto
                shm->byte_written += (MAXPKTSIZE - OVERHEAD);
            } else {//se ho gia finito di scrivere sul file tutta l dim della finestra che mi permettava
                written = (int) write_nbytes(shm->fd, shm->win_buf_rcv[shm->window_base_rcv].payload, (size_t) shm->dimension - shm->byte_written);//finisco di scrivere
                if (written < shm->dimension - shm->byte_written) {
                    handle_error_with_exit("error in write\n");
                }
                shm->byte_written += shm->dimension - shm->byte_written;
            }


            shm-> win_buf_rcv[shm->window_base_rcv].received = 0;//segna pacchetto come non ricevuto
            shm->window_base_rcv = ((shm->window_base_rcv) + 1) % (2 * shm->param.window);//avanza la finestra con modulo di 2W
        }
    }
    return;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//riceve messaggio e manda ack del messaggio.
//segna messaggio in finestra,segna che è stato ricevuto e verifica se la finestra può essere traslata
void rcv_msg_send_ack_in_window(struct temp_buf temp_buff,struct sel_repeat *shm) {
    struct temp_buf ack_buff;//pkt di ack da inviare
    int written;

    if (shm->win_buf_rcv[temp_buff.seq].received == 0) {    //accedo al campo temp_buf.seq contenente la seq del pkt(msg)ricevuto e vedo se il corrispondente nella finestra di rcv
                                                            //e'marcato come non ricevuto

        if ((shm->win_buf_rcv[temp_buff.seq].lap) == (temp_buff.lap - 1)) {
            shm-> win_buf_rcv[temp_buff.seq].lap = temp_buff.lap;
            shm->win_buf_rcv[temp_buff.seq].command = temp_buff.command;
            better_strcpy(shm->win_buf_rcv[temp_buff.seq].payload, temp_buff.payload);
            shm->win_buf_rcv[temp_buff.seq].received = 1;//segno messaggio nella finestra come ricevuto
        } else {
            //ignora pacchetto
        }
    }

    //riempio la struttura della coda condivisa
    ack_buff.ack = temp_buff.seq;//ack = al valore di seq
    ack_buff.seq = NOT_A_PKT;//e'un ack
    better_strcpy(ack_buff.payload, "ACK");//metto l'ack nel buff 
    ack_buff.command = temp_buff.command;
    ack_buff.lap = temp_buff.lap;

    if (flip_coin(shm->param.loss_prob)) {//mando l'ack sulla finestra 
        if (sendto(shm->address
.sockfd, &ack_buff, MAXPKTSIZE, 0, (struct sockaddr *) &(shm->address
.dest_addr), shm->address
.len) ==
            -1) {
            handle_error_with_exit("error in sendto\n");
        }
        print_msg_sent(ack_buff);
    } else {
        print_msg_sent_and_lost(ack_buff);
    }

    if (temp_buff.seq == shm->window_base_rcv) {//se pacchetto riempie un buco-->ovvero sta alla base della finestra, posso scorrere in avanti la finestra
        //Scorro finestra in avanti
        while (shm->win_buf_rcv[shm->window_base_rcv].received == 1) {//finhe ne trovo di riscontrati nella finestra vado a avanti a scorrere
            if (shm->win_buf_rcv[shm->window_base_rcv].command == DATA) {//DATA == 0 
                if (shm->dimension - shm->byte_written >= (int)(MAXPKTSIZE - OVERHEAD)) {
                    written = (int) write_nbytes(shm->fd, shm->win_buf_rcv[shm->window_base_rcv].payload, (MAXPKTSIZE - OVERHEAD));//Scrivo n byte sul pkt ( sul file) se ho abbastanza spazio(vedi if su)
                    if (written < (int)(MAXPKTSIZE - OVERHEAD)) {
                        handle_error_with_exit("error in write\n");
                    }
                    shm->byte_written += (MAXPKTSIZE - OVERHEAD);
                } else {
                    written = (int) write_nbytes(shm->fd, shm->win_buf_rcv[shm->window_base_rcv].payload, (size_t) shm->dimension - shm->byte_written);
                    if (written < shm->dimension - shm->byte_written) {
                        handle_error_with_exit("error in write\n");
                    }
                    shm->byte_written += shm->dimension - shm->byte_written;
                }
            }
            shm->win_buf_rcv[shm->window_base_rcv].received = 0;//segna pacchetto come non ricevuto
            shm->window_base_rcv = ((shm->window_base_rcv) + 1) % (2 * shm->param.window);//avanza la finestra con modulo di 2W
        }
    }
    return;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//ricevi ack,segna quel messaggio come riscontrato e verifica se puoi traslare la finestra
// diminuendo cosi pkt_fly
void rcv_ack_in_window(struct temp_buf temp_buff,struct sel_repeat *shm) {
    

    lock_mtx(&(shm->mtx));//Come sempre prendo prima il lock sulla shm sel repeat

    if (temp_buff.lap == shm->win_buf_snd[temp_buff.ack].lap) {
        if(shm->win_buf_snd[temp_buff.ack].acked==1){//Gia riscontrato
            unlock_mtx(&(shm->mtx));
            return;
        }
        if (shm->win_buf_snd[temp_buff.ack].acked==0) {
            if (shm->adaptive) {
                adaptive_timer(shm, temp_buff.ack);
            }

            //Caso non riscontrato , e timer  non adattativo 
            shm-> win_buf_snd[temp_buff.ack].acked = 1;
            shm->win_buf_snd[temp_buff.ack].time.tv_nsec = 0;
            shm-> win_buf_snd[temp_buff.ack].time.tv_sec = 0;

            if (temp_buff.ack == shm->window_base_snd) {//ricevuto ack del primo pacchetto non riscontrato->avanzo finestra
                while (shm->win_buf_snd[shm->window_base_snd].acked == 1) { //finquando ho pacchetti riscontrati
                                                                            //avanzo la finestra
                    if (shm->win_buf_snd[shm->window_base_snd].command == DATA) {
                        if (shm->dimension - shm->byte_read
 >= (int)(MAXPKTSIZE - OVERHEAD)) {
                            shm->byte_read
 += (MAXPKTSIZE - OVERHEAD);
                        } else {
                            shm->byte_read
 += shm->dimension - shm->byte_read
;
                        }
                    }
                    shm->win_buf_snd[shm->window_base_snd].acked = 2;//resetta quando scorri finestra
                    shm->window_base_snd = ((shm->window_base_snd) + 1) % (2 * shm->param.window);//avanza la finestra
                    (shm->pkt_fly)--;//pkt acked 
                }
            }
        }
        else {
            //ignora
        }
    }
    else {
        //ignora
    }
    unlock_mtx(&(shm->mtx));
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

//ricevi ack di un pacchetto contentente parte di file.
// Segna quel messaggio come riscontrato e verifica se puoi traslare la finestra
// diminuendo cosi pkt_fly
void rcv_ack_file_in_window(struct temp_buf temp_buff,struct sel_repeat *shm) {
    //tempbuff.command deve essere uguale a data
    lock_mtx(&(shm->mtx));
    if (temp_buff.lap ==shm-> win_buf_snd[temp_buff.ack].lap) {
        if(shm->win_buf_snd[temp_buff.ack].acked==1){//gia mandato ack
            unlock_mtx(&(shm->mtx));
            return;
        }
        if (shm->win_buf_snd[temp_buff.ack].acked == 0) {
            if (shm->adaptive) {
                adaptive_timer(shm, temp_buff.ack);
            }
            shm-> win_buf_snd[temp_buff.ack].acked = 1;
            shm->win_buf_snd[temp_buff.ack ].time.tv_nsec = 0;
            shm->win_buf_snd[temp_buff.ack].time.tv_sec = 0;
            if (temp_buff.ack == shm->window_base_snd) {//ricevuto ack del primo pacchetto non riscontrato->avanzo finestra
                while (shm->win_buf_snd[shm->window_base_snd].acked == 1) {//finquando ho pacchetti riscontrati
                    //avanzo la finestra
                    shm->win_buf_snd[shm->window_base_snd].acked = 2;//resetta quando scorri finestra
                    shm->window_base_snd = ((shm->window_base_snd) + 1) % (2 * shm->param.window);//avanza la finestra
                    (shm->pkt_fly)--;
                    if (shm->dimension - shm->byte_read
 >= (int)(MAXPKTSIZE - OVERHEAD)) {
                        shm->byte_read
 += (MAXPKTSIZE - OVERHEAD);
                    } else {
                        shm->byte_read
 += shm->dimension - shm->byte_read
;
                    }
                }
            }
        } else {
            //ignora
        }
    } else {
        //ignora
    }
    unlock_mtx(&(shm->mtx));
    return;
}


/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//ricevi ack di un pacchetto contentente parte di lista.
// Segna quel messaggio come riscontrato e verifica se puoi traslare la finestra
// diminuendo cosi pkt_fly
void rcv_ack_list_in_window(struct temp_buf temp_buff,struct sel_repeat *shm) {//ack di un messaggio contenente
    // parte di lista,tempbuff.command deve essere uguale a data
    lock_mtx(&(shm->mtx));
    if (temp_buff.lap == shm->win_buf_snd[temp_buff.ack].lap) {
        if(shm->win_buf_snd[temp_buff.ack].acked==1){
            unlock_mtx(&(shm->mtx));
            return;
        }
        if (shm->win_buf_snd[temp_buff.ack].acked == 0) {
            if (shm->adaptive) {
                adaptive_timer(shm, temp_buff.ack);
            }
            shm->win_buf_snd[temp_buff.ack].acked = 1;
            shm->win_buf_snd[temp_buff.ack].time.tv_nsec = 0;
            shm->win_buf_snd[temp_buff.ack].time.tv_sec = 0;
            if (temp_buff.ack == shm->window_base_snd) {//ricevuto ack del primo pacchetto non riscontrato->avanzo finestra
                while (shm->win_buf_snd[shm->window_base_snd].acked == 1) {//finquando ho pacchetti riscontrati
                    //avanzo la finestra
                    shm-> win_buf_snd[shm->window_base_snd].acked = 2;//resetta quando scorri finestra
                    shm->window_base_snd = ((shm->window_base_snd) + 1) % (2 * shm->param.window);//avanza la finestra
                    (shm->pkt_fly)--;
                    if (shm->dimension - shm->byte_read
 >= (int)(MAXPKTSIZE - OVERHEAD)) {
                        shm->byte_read
 += (MAXPKTSIZE - OVERHEAD);
                    } else {
                        shm->byte_read
 += shm->dimension - shm->byte_read
;
                    }
                }
            }
        }
        else {
            //ignora
        }
    }
    else {
        //ignora
    }
    unlock_mtx(&(shm->mtx));
    return;

}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*  Thread ritrasmettitore
    Ha il compito di scorrere la lista e vedere i pkt da ritrasmettere in base al timeout 

*/
void *rtx_job(void *arg) {
    struct sel_repeat *shm=arg;
    struct temp_buf temp_buff;
    struct node*node=NULL;
    long timer_ns_left;
    char to_rtx;
    struct timespec sleep_time;

    
    block_signal(SIGALRM);//il thread rtx non viene bloccato dal segnale di timeout
    
    node = alloca(sizeof(struct node));//lista 

    for(;;) {
        lock_mtx(&(shm->mtx));//Prendo il lock sulla memoria condivisa 

        while (1) {
            if(delete_head(&shm->head,node)==-1){//rimuovi nodo dalla lista,
                
                // se lista vuota aspetta sulla condizione
                wait_on_a_condition(&(shm->list_not_empty),&shm->mtx);//list_not_empty variabile che definisce se la lista sia vuota o meno
                                                                      //Finche non é vera attendo con il thread
            }
            else{//ho rimosso la testa
                if(!to_resend(shm, *node)){//se non è da ritrasmettere rimuovi un altro nodo dalla lista(riparto dal while)
                    continue;
                }
                else{
                    //se è da ritrasmettere memorizza il nodo e continua dopo il while
                    break;
                }
            }
        }
        //pkt da ritrasmettere
        unlock_mtx(&(shm->mtx));

        timer_ns_left=calculate_time_left(*node);//calcola quanto tempo manca per far scadere il timeout
        
        if(timer_ns_left<=0){//tempo già scaduto ,verifica ancora se è da ritrasmettere[se e'da ritrasmettere invialo subito]
            lock_mtx(&(shm->mtx));
            to_rtx = to_resend(shm, *node);
            unlock_mtx(&(shm->mtx));
            if(!to_rtx){
                //se non è da ritrasmettere togli un altro nodo dalla lista-->riparte da for(;;)
                continue;
            }
            else{
                //è da ritrasmettere immediatamente
                temp_buff.ack = NOT_AN_ACK;//pkt non é un ack in realtá
                temp_buff.seq = node->seq;
                temp_buff.lap=node->lap;
                
                lock_mtx(&(shm->mtx));
                copy_buf2_in_buf1(temp_buff.payload, shm->win_buf_snd[node->seq].payload, MAXPKTSIZE - OVERHEAD);
                temp_buff.command=shm->win_buf_snd[node->seq].command;
                
                resend_message(shm->address
.sockfd,&temp_buff,&shm->address
.dest_addr,shm->address
.len,shm->param.loss_prob);
                if(clock_gettime(CLOCK_MONOTONIC, &shm->win_buf_snd[node->seq].time)!=0){
                    handle_error_with_exit("error in get_time\n");
                }
                //dopo averlo ritrasmesso viene riaggiunto alla lista dei pacchetti inviati[inviata in ordine crescente di timeout-->la testa sará sempre la prima ad essere con timeout minore ]
                insert_ordered(node->seq,node->lap,shm->win_buf_snd[node->seq].time,shm->param.timer_ms,&shm->head,&shm->tail);
                unlock_mtx(&(shm->mtx));
            }
        }
        else{
            //se timer>0 dormi per tempo rimanente poi riverifica se è da mandare-->tanto i nodi sono ordinati in base al tempo ancora di vita
            //se quello che ho controllato non é ancora scaduto di sicuro gli altri scadranno tra un tempo maggiore
            sleep_struct(&sleep_time, timer_ns_left);
            nanosleep(&sleep_time , NULL);
            
            //quando mi risveglio ricontrollo come sopra
            lock_mtx(&(shm->mtx));
            to_rtx = to_resend(shm, *node);
            unlock_mtx(&(shm->mtx));
            if(!to_rtx){
                continue;
            }
            else{
                //pacchetto è da ritrasmettere,ritrasmettilo e rinseriscilo nella lista dinamica
                temp_buff.ack = NOT_AN_ACK;
                temp_buff.seq = node->seq;
                temp_buff.lap=node->lap;
                lock_mtx(&(shm->mtx));
                copy_buf2_in_buf1(temp_buff.payload, shm->win_buf_snd[node->seq].payload, MAXPKTSIZE - OVERHEAD);
                temp_buff.command=shm->win_buf_snd[node->seq].command;
                resend_message(shm->address
.sockfd,&temp_buff,&shm->address
.dest_addr,shm->address
.len,shm->param.loss_prob);
                if(clock_gettime(CLOCK_MONOTONIC, &shm->win_buf_snd[node->seq].time)!=0){
                    handle_error_with_exit("error in get_time\n");
                }
                insert_ordered(node->seq,node->lap,shm->win_buf_snd[node->seq].time,shm->param.timer_ms,&shm->head,&shm->tail);//inserisco in lista come inviato
                unlock_mtx(&(shm->mtx));
            }
        }
    }
    return NULL;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/