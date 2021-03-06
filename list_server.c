#include "basic.h"
#include "timer_functions.h"
#include "Server.h"
#include "list_server.h"
#include "functions_communication.h"

void close_list(struct temp_buf temp_buff,struct sel_repeat *shm) {
//dopo aver riscontrato tutti i pacchetti manda fin non in finestra
// senza sequenza e ack e chiudi
    alarm(0);
    send_message(shm->address
.sockfd, &shm->address
.dest_addr, shm->address
.len, temp_buff, "FIN",
                 FIN, shm->param.loss_prob);
    printf(GREEN"List correctly sent\n"RESET);
    pthread_cancel(shm->tid);
    pthread_exit(NULL);
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//messaggio start ricevuto,thread pronto alla trasmissione
void send_list( struct temp_buf temp_buff, struct sel_repeat *shm) {
    char *temp_list;//creare la lista e poi inviarla in parti

    temp_list = shm->list;
    alarm(TIMEOUT);//setto il timeout
    while (1) {
        if (((shm->pkt_fly) < (shm->param.window)) && ((shm->byte_sent) < (shm->dimension))) {  //se non sto eccedendo per numero di pkt in viaggio in base alla dim
                                                                                                //della mia finestra e non ho riempito la dimensione byte invio in lista
            send_list_in_window(temp_buff, shm);
        }
        while(recvfrom(shm->address
.sockfd, &temp_buff,MAXPKTSIZE, MSG_DONTWAIT,
                     (struct sockaddr *) &shm->address
.dest_addr, &shm->address
.len) !=
            -1) {//non devo bloccarmi sulla ricezione,se ne trovo uno leggo finquando posso
            print_rcv_message(temp_buff);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }

            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {
                    if (temp_buff.command == DATA) {//se è un messaggio contenente dati
                        rcv_ack_list_in_window(temp_buff, shm);
                        if (shm->byte_read
 == shm->dimension) {
                            close_list(temp_buff, shm);
                            free(temp_list);//liberazione memoria della lista,il puntatore di list è stato spostato per ricevere la lista
                            shm->list = NULL;
                            return;
                        }
                    } else {//se è un messaggio speciale
                        rcv_ack_in_window(temp_buff, shm);
                        if(shm->byte_read
==shm->dimension){
                            //se tutti i pacchetti sono stati riscontrati vai in chiusura della trasmissione
                            close_list(temp_buff,shm);
                            free(temp_list);//liberazione memoria della lista,il puntatore di list è stato spostato per ricevere la lista
                            shm->list = NULL;
                            return;
                        }
                    }
                } else {
                    //ack duplicato
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_in_window(temp_buff, shm);
                alarm(TIMEOUT);
            } else {
                handle_error_with_exit("Internal error\n");
            }
        }
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK && errno != 0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {//se è scaduto il timer termina i 2 thread della trasmissione
            great_alarm_serv = 0;
            printf(RED"Client not available,list command\n"RESET);
            alarm(0);
            pthread_cancel(shm->tid);
            pthread_exit(NULL);
        }
    }
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//dopo aver ricevuto messaggio list manda la dimensione della lista e aspetta start
void wait_list_start(struct sel_repeat *shm, struct temp_buf temp_buff) {
    char dim[15];
    lock_sem(shm->mtx_file);

    //Calcolo dimensione file list 
    shm->dimension = count_char_dir(dir_server);
    if (shm->dimension != 0) {
        shm->list = files_in_dir(dir_server, shm->dimension);
        unlock_sem(shm->mtx_file);
        sprintf(dim, "%ld", shm->dimension);
        //Invio la dimensione 
        send_message_in_window(temp_buff,shm , DIMENSION,dim);
    
    } else {
        unlock_sem(shm->mtx_file);
        send_message_in_window(temp_buff ,shm, ERROR,"la lista è vuota");
    }

    //attendo ack dim -->rimetto un timeout
    errno = 0;
    alarm(TIMEOUT);
    while (1) {
        if (recvfrom(shm->address.sockfd, &temp_buff,MAXPKTSIZE, 0,(struct sockaddr *) &shm->address.dest_addr, &shm->address.len) !=-1) {  //attendo risposta del client,
                                                                                                                                            //aspetto finquando non arriva la risposta o scade il timeout
           
            print_rcv_message(temp_buff);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {//Voglio un dim ack
                continue;//ignora pacchetto
            } else {
                alarm(0);//In caso contrario di sicuro ho ricevuto qualcosa che mi piace-->disattivo il timeout
            }
            if (temp_buff.command == FIN) {//se ricevi fin manda fin_ack e chiudi
                send_message(shm->address.sockfd, &shm->address.dest_addr, shm->address.len,temp_buff, "FIN_ACK", FIN_ACK, shm->param.loss_prob);
                alarm(0);
                pthread_cancel(shm->tid);
                printf(GREEN"Empty list sent\n"RESET);
                pthread_exit(NULL);
            }
            else if (temp_buff.command == START) {//se ricevi start vai in send_list , é arrivato prima dell'ack 
                rcv_msg_send_ack_in_window(temp_buff, shm);
                send_list(temp_buff,shm);
                return;
            }


            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {//se è in finestra
                    rcv_ack_in_window(temp_buff, shm);
                } else {
                    //ack duplicato
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                
                //se non è un ack e non è in finestra-->msg gia ricevuto, rimando l'ack
                rcv_msg_re_send_ack_in_window(temp_buff, shm);
                alarm(TIMEOUT);
            }  else {
               handle_error_with_exit("Internal error\n");
            }
        }
         else if (errno != EINTR) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if(great_alarm_serv == 1){//se è scaduto il timer termina i 2 thread della trasmissione
            great_alarm_serv = 0;
            printf(RED"Client not available,list command\n"RESET);
            alarm(0);
            pthread_cancel(shm->tid);
            pthread_exit(NULL);
        }
    }
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//thread trasmettitore e ricevitore
void *list_server_job(void *arg) {
    struct sel_repeat *shm= arg;
    struct temp_buf temp_buff;
    wait_list_start(shm, temp_buff);
    return NULL;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

void list_server(struct sel_repeat *shm) {//crea i 2 thread:
   
    pthread_t tid_snd,tid_rtx; //trasmettitore e ricevitore  +  ritrasmettitore
    if(pthread_create(&tid_rtx,NULL,rtx_job,shm)!=0){//rtx_job uguale per ogni processo 
        handle_error_with_exit("error in create thread list_server_rtx\n");
    }
    shm->tid=tid_rtx;
    if(pthread_create(&tid_snd,NULL,list_server_job,shm)!=0){
        handle_error_with_exit("error in create thread list_server\n");
    }
    block_signal(SIGALRM);//il thread principale non viene interrotto dal segnale di timeout[vale solo per il thread che la chiama la block]
                          //il thread principale aspetta che i 2 thread finiscano i compiti
    
    //attendo che finiscano
    if(pthread_join(tid_snd,NULL)!=0){
        handle_error_with_exit("error in pthread_join\n");
    }
    if(pthread_join(tid_rtx,NULL)!=0){
        handle_error_with_exit("error in pthread_join\n");
    }
    unlock_signal(SIGALRM);
    return;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void exe_list(struct temp_buf temp_buff,struct sel_repeat *shm) {
    //verifica che il file (con filename scritto dentro temp_buf esiste)
    // ,manda la dimensione, aspetta lo start e inizia a mandare il file,
    // temp_buff contiene il pacchetto con comando get+filename
    rcv_msg_send_ack_in_window(temp_buff, shm);
    list_server(shm);
    return;
}