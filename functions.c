#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <sys/stat.h>

#define SIGNUP       1
#define LOGIN        2
#define HANGING      3
#define SHOW         4
#define CHAT         5
#define OUT          7

#define UPDATE       8
#define ESC          9
#define BUSY         10
#define NOT_BUSY     11

#define USED_USERNAME 12
#define DEVICES_FULL  13


#define MSG          17

#define ERR_CODE            65535
#define OK_CODE             65534
#define QUIT                65533
#define USER                65532
#define ADD                 65531
#define SHARE               65530
#define HELP                65529
#define CLEAR               65528
#define LOGOUT              65527

#define ONLINE_LIST         65526
#define SERVER_COMMAND      65525
#define USER_OFFLINE        65524
#define ADD_FAIL            65523

#define CONTINUE            65522
#define USER_NOT_FOUND      65521
#define USER_BUSY           65520
#define OFFLINE             65519

#define SINGLE_CHAT         0
#define GROUP_CHAT          1

#define PRESENTE            1
#define ASSENTE             0

#define ERRORE_SHARE        -1
//funzione che restituisce il tempo attuale in formato stringa
char* tempo_attuale() {
    time_t rawtime;
    struct tm* timeinfo;
    char* tempo_stringa = (char*)malloc(sizeof(char) * 9); // dimensione per "hh:mm:ss" + 1 per il terminatore '\0'

    if (tempo_stringa == NULL) {
        fprintf(stderr, "Errore nell'allocazione di memoria per la stringa tempo_stringa\n");
        exit(EXIT_FAILURE);
    }

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(tempo_stringa, 9, "%X", timeinfo);
    return tempo_stringa;
}

//funzione che crea un nuovo socket e lo connette al server, in particolare ai new_sd di comunicazione
int new_socket(int port){
    int sd, ret;
    struct sockaddr_in server_addr;
    printf("[SOCKET] Connessione in corso con %d\n", port);
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        perror("[ERRORE] Errore durante l'esecuzione di socket()\n");
        exit(EXIT_FAILURE);
    }

    ret=setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    if (ret==-1) {
        perror("[ERRORE] Errore durante l'esecuzione della setsockopt()\n");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    ret=connect(sd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret==-1) {
        printf("[ERRORE] Il dispositivo alla porta %d è offline\n", port);
        return ERR_CODE;
    }
    return sd;
}

//funzione che invia un intero
void send_int(int num, int sd){
    int ret;
    uint16_t network_number = htons(num); 

    // se il dest si disconnette durante il trasferimento dei dati, MSG_NOSIGNAL impedisce che il mio processo si interrompa
    ret=send(sd, (void*)&network_number, sizeof(uint16_t), MSG_NOSIGNAL);
    if (ret == -1) {
        perror("[ERRORE]: errore durante l'invio di un intero\n");
        exit(EXIT_FAILURE);
    }
}

//funzione che riceve un intero
int recv_int(int sd){
    uint16_t network_number;
    int num, ret;
    
    ret=recv(sd, (void*)&network_number, sizeof(uint16_t), 0);
    if(ret==0){
        perror("[ERRORE]: il socket remoto si è chiuso\n");
        return ERR_CODE;
    }

    if(ret==-1){
        perror("[ERRORE]: errore durante la ricezione di un intero");
        return ERR_CODE;
    }
    
    num = ntohs(network_number);
    return num;
}

//funzione che invia una stringa
int send_msg(char *str, int sd){
    int ret;
    int len = strlen(str);
    
    char buffer[len];
    strcpy(buffer, str);
    send_int(len, sd);

    ret=send(sd, (void*)buffer, strlen(buffer), MSG_NOSIGNAL);
    if(ret==-1){
        perror("[ERRORE]: errore durante l'invio di una stringa\n");
        return ERR_CODE;
    }

    return 1;
}

//funzione che riceve una stringa
int recv_msg(char *str, int sd) {
    int len, ret;

    len = recv_int(sd);
    if (len == ERR_CODE) {
        perror("[ERRORE]: Errore durante la ricezione della lunghezza della stringa\n");
        exit(EXIT_FAILURE);
    }

    char buffer[len+1]; // +1 per il terminatore di stringa
    ret = recv(sd, (void*)buffer, len, 0);
    if (ret == -1) {
        perror("[ERRORE]: errore durante la ricezione di una stringa\n");
        return ERR_CODE;
    }

    buffer[len] = '\0'; // si assicura che la stringa
    strcpy(str, buffer); // copia il contenuto del buffer in str

    return 1;
}

//funzione che invia un file
int send_file(FILE *fp, int sd){
    char buffer[1024] = {0};
    char* ret;

    for(;;){
        ret=fgets(buffer, 4096, fp);
        if(ret!=NULL){
            send_int(OK_CODE, sd);
            if(send(sd, buffer, sizeof(buffer), 0) == -1) {
                perror("[ERRORE]: errore durante l'invio del file\n");
                return ERR_CODE;
            }
            bzero(buffer, 1024);
        }
        else{
            send_int(ERR_CODE, sd);
            return 0;
        }
    }
}

//funzione che riceve un file
void recv_file(int sd, char type[1024]){
    FILE *fp;
    char buffer[1024];
    char file[1024];
    int codice;
    int ret;

    sprintf(file, "recv.%s", type);
    fp = fopen(file, "w");

    for(;;){
        
        codice = recv_int(sd);
        if(codice == OK_CODE){
            ret = recv(sd, buffer, 1024, 0);
            if(ret == -1){
                perror("[ERRORE]: errore durante la ricezione del file\n");
            }
            fprintf(fp, "%s", buffer);
            bzero(buffer, 1024);
        }
        else{
            fclose(fp);
            return;
        }
    }
}