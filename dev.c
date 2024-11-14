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
#include "functions.c"

#define MAX_DEVICES 10

//STRUTTURE DATI

struct Device {
    char username[50];
    int port;
    int id;                     //identificatore univoco
    bool registered;
    bool logged;
    int sd;                     //socket di comundicazione con altro dispositivo
    struct sockaddr_in addr;
};

struct Server {
    int sd;
    int port;
};

struct Device devices[MAX_DEVICES];

struct Device current_device;

struct Server server;

int listening_socket;

int chatDevices;

fd_set master;          //main set: gestita con le macro
fd_set read_fds;        //read set: gestita dalla select()
int fdmax;

int i;

//DICHIARAZIONE FUNZIONI
void comando();
void richiesta();
void signup();
void login();
void hanging();
void show();
void chat();
void out();
void initialize_device(int id, const char* usr, const char* pswd);
void mostra_comandi();
void new_chat();
void gestore_chat(int id);  
int leggi_input(int i);
int leggi_messaggi(int i, int id);
void send_to_server();
void chat_history(int id);
void add();
void help();
void quit();
void salva_logout();
int leggi_codice(char* msg);
int dispositivo_crashato(int sd);
void join_chat();
void share();
void ricevi_file(int i);
void clear();
void user();
void creacartellahistory();
void clear_previous_line();
int check_rubrica(char* username);
void share_recv(int i);
void aggiornalogout();




//MAIN
int main(int argc, char *argv[]) {
    int i;
    if (argc != 2) {
        printf("[ERRORE]:utilizzare il formato ./device <port>\n");
        return 1;
    }

    int porta = atoi(argv[1]);
    current_device.port=porta;
    current_device.logged=false;

    for (i = 0; i < MAX_DEVICES; i++) {
        devices[i].sd = -1;
    }
     
    //inizializzo l'fd_set e ci aggiungo il listening_socket del dispositivo
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(0, &master);
    FD_SET(listening_socket, &master);
    fdmax=listening_socket;
    clear();

    for(;;) {
        mostra_comandi();                      
        read_fds = master;
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("[device] error select() ");
            exit(EXIT_FAILURE);
        }
        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i==0) {                        //standard input, leggo l'input da tastiera
                    comando();
                }
                else if (i == listening_socket) {   //ad ogni dispositivo è associato un listening_socket dal quale il dispositivo riceve le richieste da altri dispositivi o dal server
                    new_chat();
                }
            }
        }
    }
}

//funzione che gestisce la chat, il parametro che passo è l'id del dispositivo con cui entro in chat
void gestore_chat(int id) {
    
    int i;
    int ret;

    //questo numero serve a tenere traccia del numero di utenti in chat, così da eventualmente rendere
    //il current_device nuovamente disponibile per una nuova chat nel caso in cui rimanga solo o da aggiungere
    //i messaggi inviati in cronologia nel caso in cui sia una chat tra due soli utenti
    chatDevices = 1;

    //notifico al server che sono occupato
    server.sd = new_socket(server.port);
    send_int(BUSY, server.sd);

    send_int(current_device.id, server.sd);
    close(server.sd);

    for (;;) {

        read_fds = master;
        ret = select(fdmax + 1, &read_fds, NULL, NULL, NULL);
        if (ret == -1) {
            perror("[ERRORE]: errore durante la select()\n");
            exit(-1);
        }
        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == 0) {               //standard input, leggo l'input da tastiera, gli passo l'id del dispositivo con cui chatto
                    if(leggi_input(id)){
                        return;
                    }
                    
                } else if (i == listening_socket) { //in questo caso c'è qualcuno che sta tentando di unirsi alla chat
                    join_chat();
                    chatDevices++;
                } else if (i != listening_socket) { //sto ricevendo un messaggio
                    
                   ret=leggi_messaggi(i, id);
                     if(ret==ERR_CODE){
                          return;
                    
                   }
                   if(ret==CONTINUE){
                          continue;
                   }}}}
}
}
    

int leggi_input(int i) {
    int j;
    int ret;
    int codice;
    char formatted_msg[1024]; //per aggiungere il nome utente di chi invia il messaggio
    char msg[1024];

    fgets(msg, 1024, stdin);
    if (msg[0] == '\n') {
        return 0; // nessun input gestito, continua il ciclo principale
    }
    codice = leggi_codice(msg);

    //qui seleziono tutti i device che partecipano alla chat
    for (j = 0; j < MAX_DEVICES; j++) {
        if (devices[j].sd != -1) {
            send_int(codice, devices[j].sd);
        }
    }

    //mando un messaggio
    if (codice == MSG) {
        sprintf(formatted_msg, "%s: %s", current_device.username, msg);
        strncpy(msg, formatted_msg, sizeof(msg));
        
        //uso la seguente funzione cosi che anche a chi manda il messaggio appaia il messaggio con a fianco il suo nome utente, per maggiore chiarezza
        clear_previous_line();
        printf("%s", msg);
        for (j = 0; j < MAX_DEVICES; j++) {
            if (devices[j].sd != -1) {
                ret = send_msg(msg, devices[j].sd);
                if (ret == ERR_CODE) {
                    devices[j].sd = -1;
                }
            }
        }
        
        //poiché abbiamo la cronologia delle chat solo per le chat utente a utente e non per quelle di gruppo,
        //mi avvalgo della variabile chatDevices per capire se è una chat di gruppo o meno ed eventualmente aggiungere il messaggio alla cronologia
        if(chatDevices==1){
            char file[1024];
            sprintf(file, "./chat/%d/%d.txt", current_device.id, i);
            FILE *f = fopen(file, "a");
            if (f == NULL) {
                printf("Prima volta che avete una conversazione, cronologia vuota\n");
            }
            fprintf(f, "%s", msg);
            fclose(f);
        }
        return 0; 
    } else if (codice == QUIT) {
        server.sd = new_socket(server.port);
        send_int(NOT_BUSY, server.sd);
        send_int(current_device.id, server.sd);
        close(server.sd);
        quit();
        return 1; 
    } else if (codice == HELP) {
        help();
        return 0; 
    } else if (codice == USER) {
        user();
        return 0; 
    } else if (codice == CLEAR) {
        clear();
        return 0; 
    } else if (codice == ADD) {
        add();
        return 0; 
    } else if (codice == SHARE) {
        share();
        return 0; 
    } else {
        printf("[CHAT]: Comando non valido\n");
        return 0; 
    }
}

int leggi_messaggi(int i, int id){
    int ret;
    int codice;
    int destID;
    int dest_port;
    char msg[1024];
        
        codice = recv_int(i);

        //se c'è un errore vuol dire che qualcuno è crashato, allora chiamo la funzione che mi dice se la chat va terminata o meno
        if (codice == ERR_CODE) {
            if (dispositivo_crashato(i)) {
                            return  ERR_CODE;
                }
                return CONTINUE;
                 }

                    if (codice == MSG) {
                        ret = recv_msg(msg, i);
                        if (ret == ERR_CODE) {
                            if (dispositivo_crashato(i)) {
                                return ERR_CODE;
                            }
                            return CONTINUE;
                        }
                        printf("%s", msg);
                        
                        //come prima, mia avvalgo della variabile chatDevices e se è una chat singola aggiungo i messaggi alla cronologia
                        if(chatDevices==1){
                            
                            char file[1024];

                            sprintf(file, "./chat/%d/%d.txt", current_device.id, id);
                            FILE *f = fopen(file, "a");
                            if (f == NULL) {
                                printf("Prima conversazione, cronologia vuota\n");
                            }
                            if(f){
                            fprintf(f, "%s", msg);
                            fclose(f); }
                            
                        }
                        
                        return 1;
                    } else if (codice == QUIT) { //ho ricevuto una richiesta di uscita dalla chat
                        destID = recv_int(i);    //ricevo l'id di chi vuole uscire
                        if (destID == ERR_CODE) {
                            if (dispositivo_crashato(i)) {
                                return ERR_CODE;
                            }
                            return CONTINUE;
                        }
                        // rimuovo il dispositivo dalla chat
                        chatDevices--;
                        devices[destID].sd = -1; //cancello l'sd del dispositivo che esce dalla chat
                        FD_CLR(i, &master);
                        close(i);
                        
                        //notifico al server che sono libero
                        if(chatDevices==0){
                            server.sd = new_socket(server.port);
                            send_int(NOT_BUSY, server.sd);
                            send_int(current_device.id, server.sd);
                            close(server.sd);
                            system("clear");
                            printf("Sono pronto per una nuova chat\n");
                            return ERR_CODE;
                        }
                        return 1;
                    } else if(codice == ADD){
                        //qualcuno vuole aggiungere un utente alla chat, ne ricevo l'id e la porta e gestisco eventuali errori come crash
                        //dopodiché creo un socket con questo dispositivo e lo aggiungo al set master
                        destID = recv_int(i);

                        if (destID == ERR_CODE) {
                            if (dispositivo_crashato(i)) {
                                return ERR_CODE;
                            }
                            return CONTINUE;
                        }
                        if(destID==ADD_FAIL){
                            printf("Errore durante l'aggiunta dell'utente\n");
                            return CONTINUE;
                        }

                        dest_port = recv_int(i);
                        if (dest_port == ERR_CODE) {
                            if (dispositivo_crashato(i)) {
                                return ERR_CODE;
                            }
                            return CONTINUE;
                        }

                    
                        devices[destID].sd = new_socket(dest_port);
                        if (devices[destID].sd == ERR_CODE) {
                            printf("Errore durante la connessione al dispositivo %d\n", destID);
                            
                        }
                        FD_SET(devices[destID].sd, &master);
                        if (devices[destID].sd > fdmax) {
                            fdmax = devices[destID].sd;
                        }
                        

                        printf("Qualcuno si è unito alla chat\n");
                        
                        //invio il mio id al dispositivo appena aggiunto
                        send_int(current_device.id, devices[destID].sd);
                    
                        chatDevices++;
                        return 1;

       } else if(codice == SHARE){
        //ricevo il file condiviso
            share_recv(i);
        return 1;
        }else if(codice == USER || codice == HELP){
            //se qualcuno chiama la user o la help gestisce tutto il server e non mi interessa 
            return 1;
    }
    return 0;
}
//FUNZIONE CHE LEGGE I COMANDI
void comando() {
    char comando[20];
    // accetta i comandi a seconda dello stato del device
    if (current_device.logged==false) {
        scanf("%s", comando);
        if (strncmp(comando, "signup", 6)==0) {
            signup();
            return;
        }
        if (strncmp(comando, "in", 2)==0) {
            login();
            return;
        }
        printf("Comando non valido\n");
    }
    else {
        scanf("%s", comando);
        
        if (strncmp(comando, "hanging", 7)==0) {
            hanging();
            return;
        }

        if (strncmp(comando, "chat", 4)==0) {
            chat();
            return;
        }

        if (strncmp(comando, "show", 4)==0) {
            show();
            return;
        }

        if (strncmp(comando, "out", 3)==0) {
            out();
            return;
        }
        printf("Comando non valido\n");
    }
}

//FUNZIONE CHE RICHIEDE LA DISCONNESSIONE DAL NETWORK

//FUNZIONE CHE GESTISCE LE RICHIESTE IN ARRIVO
void new_chat() {
    int new_sd, sender_id;
    int chat_id;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    new_sd = accept(listening_socket, (struct sockaddr*)&client_addr, &client_len);
    if (new_sd == -1) {
        perror("<ERROR> Something went wrong during accept()\n");
        exit(-1);
    }
    //ricevo l'id del dispositivo che mi invia la richiesta
    sender_id = recv_int(new_sd);
    
    if (sender_id == ERR_CODE) {
        printf("[ERRORE] Errore durante la ricezione dell'ID del mittente\n");
        close(new_sd);
        return;
    }
    //qui è il server che mi manda SHOW per notificarmi che qualcuno ha letto i miei messaggi pendenti
    if (sender_id == SHOW) {
        printf("qualcuno ha letto i tuoi messaggi pendenti\n");
        close(new_sd);
        return;
    }
    devices[sender_id].sd = new_sd;
    FD_SET(new_sd, &master);
    if (new_sd > fdmax) { 
        fdmax = new_sd; 
    }
    chat_id = recv_int(new_sd);
    if (chat_id == ERR_CODE) {
        printf("[ERRORE] Errore durante la ricezione dell'ID della chat\n");
        close(new_sd);
        return;
    }
    clear();
    
    printf("\n********************************* [CHAT] *********************************\n\n");
    printf("Benvenuto nella chat, invia un messaggio o digita /h per la lista dei comandi\n\n");

    //se è una chat singola, stampo la cronologia
    if (chat_id == SINGLE_CHAT){
        chat_history(sender_id);
    }
    gestore_chat(sender_id);
    

    close(new_sd);
}


void join_chat() {
    int new_sd, sender_id;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    new_sd = accept(listening_socket, (struct sockaddr*)&client_addr, &client_len);
    if (new_sd == -1) {
        perror("<ERROR> Something went wrong during accept()\n");
        exit(-1);
    }

    //id del dispositivo che mi ha aggiunto alla chat
    sender_id = recv_int(new_sd);

    if (sender_id == SHOW) {
        printf("qualcuno ha letto i tuoi messaggi pendenti\n");
        close(new_sd);
        return;
    }
    if (sender_id == ERR_CODE) {
        printf("[ERRORE] Errore durante la ricezione dell'ID del mittente\n");
        close(new_sd);
        return;
    }

    devices[sender_id].sd = new_sd;
    FD_SET(new_sd, &master);
    if (new_sd > fdmax) { 
        fdmax = new_sd; 
    }
}


void signup(){
    char username[1024];
    char password[1024];
    int device_id;

    printf("[DEVICE]: registrazione in corso.\n");

    scanf("%d", &server.port);
    scanf("%s", username);
    scanf("%s", password);

    server.sd = new_socket(server.port);

    if(server.sd==ERR_CODE){
        printf("[ERRORE] Errore durante la creazione di un socket\n");
        return;
    }
    send_int(SIGNUP, server.sd);
    sleep(1);

    send_msg(username, server.sd);
    send_msg(password, server.sd);    

    device_id = recv_int(server.sd);
    if(device_id == ERR_CODE){
        printf("[ERRORE] Errore durante la ricezione dell'ID del dispositivo\n");
        close(server.sd);
        return;
    }
    if(device_id == USED_USERNAME){
        printf("[ERRORE] Errore durante la signup: l'username '%s' non è disponibile!\n", username);
        close(server.sd);
        return;
    }
    if(device_id == DEVICES_FULL){
        printf("[ERRORE] Errore durante la signup: il server ha raggiunto il numero massimo di dispositivi registrati!\n");
        close(server.sd);
        return;
    }else{
        printf("[DEVICE]: registrazione del device %d avvenuta con successo\n", device_id);
    }


    //non salvo il device lato client perché mi basta che le informazioni siano nel server, 
    //infatti se un device vuole comunicare con un altro device, il server gli dirà chi è il device con cui vuole comunicare
    close(server.sd);
}

void login() {                                                                                  
    int dev_id, ret;
    char username[1024];
    char password[1024];

    printf("[DEVICE]: login in corso...\n");
    scanf("%d", &server.port);
    scanf("%s", username);
    scanf("%s", password);

    server.sd = new_socket(server.port);

    if(server.sd==ERR_CODE){
        printf("[ERRORE] Errore durante la creazione di un socket\n");
        return;
    }

    send_int(LOGIN, server.sd); // invio il comando di login

    sleep(1);

    send_msg(username, server.sd);
    send_msg(password, server.sd);
    send_int(current_device.port, server.sd);
    dev_id = recv_int(server.sd);
    printf("id ricevuto %d\n", dev_id);
    if (dev_id == ERR_CODE) {
        printf("[ERRORE]: Errore durante il login\n");
        close(server.sd);
        return;
    }
    else {
        clear();
        printf("[LOGIN] Login del device %d avvenuto con successo\n\n", dev_id);
        sleep(1);
        current_device.logged = true;
        current_device.id = dev_id;
        strcpy(current_device.username, username);
    //creo un listening_socket per il device corrente e lo aggiungo al master set. 
    //il listening_socket è il socket dal quale il device riceve le richieste da altri dispositivi o dal server

        listening_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listening_socket == -1) {
        perror("<ERROR> Something went wrong during socket()\n");
        exit(-1);
    }

    // imposta l'opzione SO_REUSEADDR per consentire al socket di riutilizzare un indirizzo e un numero di porta già in uso,
    // accelerando il riavvio del socket in caso di riavvio del programma 
    if (setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        perror("<WARNING> Something went wrong during setsocket()\n");
    }
    memset(&current_device.addr, 0, sizeof(current_device.addr));
    current_device.addr.sin_family = AF_INET;
    current_device.addr.sin_port = htons(current_device.port);
    current_device.addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(listening_socket, (struct sockaddr*)&current_device.addr, sizeof(current_device.addr));
    if (ret == -1) {
        perror("<ERROR> Something went wrong during bind()\n");
        exit(-1);
    }
    listen(listening_socket, 10);
    FD_SET(listening_socket, &master);
    if (listening_socket > fdmax){ 
        fdmax = listening_socket; }
    }
    //creo la cartella della cronologia se non esiste già
    creacartellahistory();
    //la aggiornalogout aggiorna il logout se l'ultima volta che il current_device ha fatto logout, il server era offline
    aggiornalogout();
    devices[dev_id].logged = true;
}

void aggiornalogout(){
    char file[1024];
    sprintf(file, "./logout/%d.txt", current_device.id);
    FILE *f = fopen(file, "r");
    if(f==NULL){
        send_int(OK_CODE, server.sd);
        return;
    }
    printf("Aggiornando il logout...\n");
    send_int(LOGOUT, server.sd);
    send_file(f, server.sd);
    fclose(f);
    remove(file);
    close(server.sd);
    printf("Logout aggiornato\n");
}

void creacartellahistory(){
    struct stat st = {0};
    char path[1024];
    char file[1024];
    sprintf(path, "./chat");
    if (stat(path, &st) == -1) {                    //se la cartella non esiste la creo
        mkdir(path, 0700);
    }
    sprintf(file, "%s/%d", path, current_device.id);
    if (stat(file, &st) == -1) {                    //se la cartella con il nome di current_device.id non esiste la creo
        mkdir(file, 0700);
    }
}

void hanging() {
    // funzione chiamata in risposta al comando HANGING
    // permette al dispositivo di controllare i messaggi pendenti
    int i;
    int count=0;
    int num;
    int ret;
    char username[1024];
    char timestamp[1024];
    clear();
    server.sd = new_socket(server.port);
    if(server.sd==ERR_CODE){
        printf("[ERRORE] Server disconnesso\n");
        return;
    }
    send_int(HANGING, server.sd); // invio il comando di hanging
    send_int(current_device.id, server.sd);

    //riceve dal server il numero di dispositivi che fanno parte della lista
    count = recv_int(server.sd);
    if(count==ERR_CODE){
        printf("[ERRORE] Errore durante la ricezione del numero di messaggi pendenti\n");
        close(server.sd);
        return;
    }
    if(count==0){
        printf("Nessun messaggio pendente\n");
        close(server.sd);
        return;
    }else{
    printf("Hai messaggi pendenti da %d device\n", count);}
    for(i=0; i<count; i++){
        ret = recv_msg(username, server.sd);
        if(ret==ERR_CODE){
            printf("[ERRORE] Errore durante la ricezione dell'username\n");
            close(server.sd);
            return;
        }
        num = recv_int(server.sd);
        if(num==ERR_CODE){
            printf("[ERRORE] Errore durante la ricezione del numero di messaggi pendenti\n");
            close(server.sd);
            return;
        }
        ret = recv_msg(timestamp, server.sd);
        if(ret==ERR_CODE){
            printf("[ERRORE] Errore durante la ricezione del timestamp\n");
            close(server.sd);
            return;
        }
        printf("Hai %d messaggi da pendenti da %s, ultimo messaggio ricevuto alle %s\n", num, username, timestamp);
    }
    close(server.sd);
}

void out() {
    // funzione chiamata in risposta al comando OUT
    // permette al dispositivo di andare offline
    server.sd = new_socket(server.port);
    if(server.sd==ERR_CODE){
        printf("[ERRORE] Server disconnesso\n");
        salva_logout();
        close(server.sd);
        current_device.logged = false;
        printf("Dispositivo disconnesso\n");
        close(server.sd);
        close(listening_socket);
        FD_CLR(listening_socket, &master);
        return;
    }
    send_int(OUT, server.sd); // invio il comando di logout
    send_int(current_device.id, server.sd);

    printf("Dispositivo %d disconnesso\n", current_device.id);
    current_device.logged = false;
    close(server.sd);
    close(listening_socket);
    FD_CLR(listening_socket, &master);
}

//funzione che slava il logout cosi da comuincarlo al server la prossima volta che eseguo il login
void salva_logout(){
    struct stat st = {0};
    char path[1024];
    char file[1024];
    char* timestamp=tempo_attuale();
    int ret;
    sprintf(path, "./logout");
    if (stat(path, &st) == -1) {                    //se la cartella non esiste la creo
        mkdir(path, 0700);
    }
    sprintf(file, "%s/%d.txt", path, current_device.id);
    FILE *f = fopen(file, "w");
    if (f == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }
    
    //salva la stringa di 9 caratteri restituita da tempo_attuale() come logout
    ret = fprintf(f, "%s", timestamp);
    if(ret<0){
        printf("Errore durante il salvataggio del logout\n");
        return;
    }
    free(timestamp);
    fclose(f);
    printf("Logout salvato\n");
}

void show() {
    char username[1024];
    int ret;

    server.sd = new_socket(server.port);

    if(server.sd==ERR_CODE){
        printf("[ERRORE] Server disconnesso\n");
        return;
    }
    scanf("%s", username);
    send_int(SHOW, server.sd); // invio il comando di show
    send_int(current_device.id, server.sd);
    send_msg(username, server.sd);;
    ret = recv_int(server.sd);
    if(ret==ERR_CODE){
        printf("[ERRORE] Errore durante la ricezione del numero di messaggi pendenti\n");
        close(server.sd);
        return;
    }
        //gestisco la lettura dei messaggi pendenti ricevendoli in un file e leggendo riga per riga
        clear();
        recv_file(server.sd, "txt");
        FILE *fp = fopen("recv.txt", "r");
        if(fp){
            char buffer[1024];
                while(fgets(buffer, 1024, fp)!= NULL){
                printf("%s", buffer);}
            fclose(fp);
        } else {
            printf("[ERRORE] Errore durante l'apertura del file, niente da leggere\n");
        }
        
        printf("Messaggi pendenti controllati\n");
        sleep(2);
        close(server.sd);
        
    
}

void chat(){
    char username[1024];
    int destID;
    int dest_port;
    int ret;

    scanf("%s", username);

    printf("Cerco di chattare con %s\n", username);
    if(strcmp(username, current_device.username)==0){
        clear();
        printf("Non puoi chattare con te stesso\n");
        return;
    }

    //controllo che l'utente con cui voglio chattare sia nella mia rubrica
    if(check_rubrica(username)==ASSENTE){
        printf("L'utente %s non è nella tua rubrica\n", username);
        return;
    }
                                                
    server.sd = new_socket(server.port);

    send_int(CHAT, server.sd); // invio il comando di chat

    //qui lato server c'è la device_info, che mi invia i dati dell'utente con cui voglio chattare
    ret = send_msg(username, server.sd);
    if(ret==ERR_CODE){
        printf("[ERRORE] Errore durante l'invio dell'username\n");
        close(server.sd);
        return;
    }
    destID = recv_int(server.sd);
    if(destID==ERR_CODE){
        printf("[ERRORE] Errore durante la ricezione dell'ID del destinatario\n");
        close(server.sd);
        return;
    }
    dest_port = recv_int(server.sd);
    if(dest_port==ERR_CODE){
        printf("[ERRORE] Errore durante la ricezione della porta del destinatario\n");
        close(server.sd);
        return;
    }

    if((destID==USER_NOT_FOUND)&&(dest_port==USER_NOT_FOUND)){
        printf("L'utente %s non esiste\n", username);
        close(server.sd);
        return;
    }
    //se l'utente è offline o busy mando i messaggi al server

    if(dest_port==OFFLINE){                                             
        printf("L'utente %s è offline\n", username);
        //mando i messaggi al server che li renderà pendenti
        send_to_server(server.sd);
        sleep(1);
        close(server.sd);
        return;
    }

    if(dest_port==USER_BUSY){
        printf("L'utente %s è occupato\n", username);
        //mando i messaggi al server che li renderà pendenti
        send_to_server(server.sd);
        sleep(1);
        close(server.sd);
        return;
    }
    devices[destID].sd = new_socket(dest_port);


    //dispositivo è crashato, lo notifico al server
    if(devices[destID].sd==ERR_CODE){
        server.sd = new_socket(server.port);
        send_int(OFFLINE, server.sd);   
        send_int(destID, server.sd);
        sleep(1);
        send_to_server(server.sd);
        return;
    }

    //invio il mio id all'utente con cui avvio la chat
    send_int(current_device.id, devices[destID].sd);
    printf("id inviato\n");
    send_int(SINGLE_CHAT, devices[destID].sd); // notifico al destinatario che voglio fare una chat singola

    FD_SET(devices[destID].sd, &master);
    if (devices[destID].sd > fdmax) { 
        fdmax = devices[destID].sd; 
    }
    clear();
    printf("\n********************************* [CHAT] *********************************\n\n");
    printf("Benvenuto nella chat, invia un messaggio o digita /h per la lista dei comandi\n\n");

    chat_history(destID);
    gestore_chat(destID);
    

    server.sd = new_socket(server.port);
    if(server.sd==ERR_CODE){
        printf("[ERRORE] Server disconnesso\n");
        return;
    }
    //se sono arrivato qui significa che sono uscito dal ciclo della gestore_chat, segnalo al server che non sono più occupato
    send_int(NOT_BUSY, server.sd); 
    send_int(destID, server.sd);
    close(server.sd);
}

//stampo la cronologia, parametro passato è l'id dell'utente con cui ho la chat
void chat_history(int i){
    char file[1024];
    sprintf(file, "./chat/%d/%d.txt", current_device.id, i);
    FILE *f = fopen(file, "r");
    if (f == NULL) {
        printf("Prima conversazione, cronologia vuota\n");
    }
    if(f){
    char buffer[1024];
    while(fgets(buffer, 1024, f)){
        printf("%s", buffer);
    }
    fclose(f);
    }
}

//funzione chiamata se l'utente con cui voglio chattare è offline, allora mando il messaggio al server che lo salva tra i pendenti
void send_to_server(){
    char formatted_msg[1024]; //per aggiungere il nome utente di chi invia il messaggio
    char msg[1024];
    int codice;
    int ret;

    system("clear");
    printf("L'utente con cui vuoi chattare è offline o occupato.\n Invia i messaggi al server. Una volta terminato, digita /q\n");
    send_int(current_device.id, server.sd);
    
    for(;;){
        
    fgets(msg, 1024, stdin);
    while (msg[0] == '\n') {
       fgets(msg, 1024, stdin);
    }

    codice = leggi_codice(msg);
    //invio il messaggio al server solo se è un messaggio o un comando valido

    if (codice == MSG || codice == QUIT) {
        if (codice == MSG) {
    sprintf(formatted_msg, "%s: %s", current_device.username, msg);
    strncpy(msg, formatted_msg, sizeof(msg));
        }

        ret = send_msg(msg, server.sd);
        if(ret==ERR_CODE){
            printf("[ERRORE] Errore durante l'invio del messaggio\n");
            close(server.sd);
            return;
        }

        if (codice == QUIT) {
            close(server.sd);
            return;
        }
    } else {
        printf("Comando non valido\n");
    }
    }
}

int leggi_codice(char* msg){
    if(msg[0]=='/'){
        switch(msg[1]){
            case 'q':
                return QUIT;
            case 'h':
                return HELP;
            case 'u':
                return USER;
            case 'c':
                return CLEAR;
            case 'a':
                return ADD;
            case 's':
                return SHARE;
            default:
                return MSG;
        }
    }else{
        return MSG;
    }
}

void help(){
    printf("Comandi disponibili:\n"
            "/q --> esci dalla chat\n"
            "/u --> mostra l'elenco degli utenti connessi\n"
            "/c --> pulisci la schermata\n"
            "/a --> aggiungi un utente alla chat\n"
            "/s --> condividi un file con gli utenti della chat\n");
}

//mi permette di uscire, informo tutti quale è il mio id e tolgo tutti i socket degli altri dispositivi da quelli che ascolto
void quit(){
    int i;
    for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            send_int(current_device.id, devices[i].sd);
            FD_CLR(devices[i].sd, &master);
            close(devices[i].sd);
            devices[i].sd = -1;
        }}
    chatDevices = 0;
    return;
}

//mi permette di aggiungere un dispositivo alla chat
void add(){
    int i;
    char username[1024];
    int ret;
    int dest_port;
    int destID;

    printf("Inserisci l'username dell'utente con cui vuoi chattare\n");
    scanf("%s", username);
    if(strcmp(username, current_device.username)==0){

        printf("Non puoi chattare con te stesso\n");
        return;
    }

    if(check_rubrica(username)==ASSENTE){
        printf("L'utente %s non è nella tua rubrica\n", username);
        for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            send_int(ADD_FAIL, devices[i].sd);
        }
    }
        return;
    }
    server.sd = new_socket(server.port);
    //se server offline notifico agli altri il fallimento della add
    if(server.sd==ERR_CODE){
        printf("[ERRORE] Server disconnesso\n");
        for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            send_int(ADD_FAIL, devices[i].sd);
        }
    }
        return;
    }

    send_int(ADD, server.sd); // invio il comando di add
    ret = send_msg(username, server.sd);
    //qui, lato server c'è la deviceInfo che mi invia le informazioni del dispositivo così da poter aggiungerlo
    //nel caso in cui qualcosa vada storto, notifico agli altri il fallimento della add
    if(ret==ERR_CODE){
        printf("[ERRORE] Errore durante l'invio dell'username\n");
        close(server.sd);
        for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            send_int(ADD_FAIL, devices[i].sd);
        }
    }
        return;
    }
    destID = recv_int(server.sd);
    if(destID==ERR_CODE){
        printf("[ERRORE] Errore durante la ricezione dell'ID del destinatario\n");
        close(server.sd);
        for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            send_int(ADD_FAIL, devices[i].sd);
        }
    }
        return;
    }
    dest_port = recv_int(server.sd);
    if(dest_port==ERR_CODE){
        printf("[ERRORE] Errore durante la ricezione della porta del destinatario\n");
        close(server.sd);
        for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            send_int(ADD_FAIL, devices[i].sd);
        }
    }
        return;
    }
    if(ret==ERR_CODE){
        printf("[ERRORE] Errore durante l'aggiunta dell'utente\n");
        close(server.sd);
       for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            send_int(ADD_FAIL, devices[i].sd);
        }
    }
        return;
    }
    if(destID==USER_NOT_FOUND){
        printf("L'utente %s non esiste\n", username);
        for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            send_int(ADD_FAIL, devices[i].sd);
        }
    }
        return;
    }
    if(dest_port==USER_BUSY){
        printf("L'utente %s è occupato\n", username);
        for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            send_int(ADD_FAIL, devices[i].sd);
        }
    }
        return;
    }
    if(dest_port==OFFLINE){
        printf("L'utente %s è offline\n", username);
        for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            send_int(ADD_FAIL, devices[i].sd);
        }
    }
        return;
    }
    
    //invio le informazioni del destinatario a tutti i dispositivi della chat cosi che possano creare un socket e connettersici
    for(i=0; i<MAX_DEVICES; i++){
        if(devices[i].sd !=- 1){  
           send_int(destID, devices[i].sd);
              send_int(dest_port, devices[i].sd);
        }
    }

    //invio al device appena aggiunto il mio id
    devices[destID].sd = new_socket(dest_port);
    if(devices[destID].sd==ERR_CODE){
        printf("Errore durante la connessione al dispositivo %d\n", destID);
        return;
    }
    send_int(current_device.id, devices[destID].sd);

    
    printf("L'utente [%s] si è unito alla chat\n", username);

    send_int(GROUP_CHAT, devices[destID].sd); // notifico al destinatario che voglio fare una chat di gruppo
    FD_SET(devices[destID].sd, &master); //aggiungo il suo socket al master
    if (devices[destID].sd > fdmax) { 
        fdmax = devices[destID].sd; 
    }

    chatDevices++;
    return;

}

void user(){
    int ret;
    int count;
    char username[1024];
    server.sd = new_socket(server.port);
    if(server.sd==ERR_CODE){
        printf("[ERRORE] Server disconnesso\n");
        return;
    }
    send_int(ONLINE_LIST, server.sd); // invio il comando di user

    count = recv_int(server.sd); //il server per prima cosa mi invia il numero di utenti cosi da permettermi di chiudere il for
    for(i=0; i<count; i++){
        ret = recv_msg(username, server.sd);
        if(ret==ERR_CODE){
            printf("[ERRORE] Errore durante la ricezione dell'username\n");
            close(server.sd);
            return;
        }
        printf("%s\n", username);
    }
}

void clear(){
    system("clear");
}

void clear_previous_line() {
    printf("\033[1A"); 
    printf("\033[K");  
}

/*LOGICA PER LO SHARE FILE*/
void condividi_file(FILE* fp, char* type) {
    int i, ret;
    for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            //invio l'ok code
            send_int(OK_CODE, devices[i].sd);
            //invio il type
            send_msg(type, devices[i].sd);
            ret = send_file(fp, devices[i].sd);
            //invio il file
            if (ret == ERR_CODE) {
                printf("[ERRORE]: Errore durante l'invio del file\n");
                return;
            }
            sleep(1);
            // resetta il cursore di lettura del file all'inizio, in modo che la prossima lettura parta dall'inizio del file
            fseek(fp, 0, SEEK_SET);
        }
    }
}

void share() {
    //prima mostro i file che ci sono nella seguente directory, dopodiché condivido quello scelto dall'utente
    char file_name[1024];
    system("ls");
    printf("\t\t^                                           ^\n"
           "\t\t|                                           |\n"
           "\t\t|   quale di questi file vuoi condividere?  |\n");
                               
    scanf("%s", file_name);

    //se c'è un errore durante l'apertura lo notifico agli altri
    FILE* fp = fopen(file_name, "r");
    if (fp == NULL) {
        printf("[SHARE] File '%s' does not exists!\n", file_name);
        for(i=0; i<MAX_DEVICES; i++){
            if(devices[i].sd !=- 1){  
                send_int(ERRORE_SHARE, devices[i].sd);
            }
        }
        return;
    }

    
    char* name = strtok(file_name, ".");
    char* type = strtok(NULL, ".");
    printf("sto mandando il file %s.%s\n", name, type);

    //invio l'ok code, il file e il tipo
    condividi_file(fp, type);

    printf("File condiviso con successo\n");
}

//riceve il file in recv.txt
void share_recv(int sd) {
    printf("Qualcuno ti sta inviando un file\n");
    int ret;
    char type[10];
    ret = recv_int(sd);
    //comunicazione con il dispositivo che sta inviando il file per sapere se procede correttamente
    if (ret == ERRORE_SHARE) {
        printf("[ERRORE] Il trasferimento del file è fallito\n");
        return;
    }

    
    ret = recv_msg(type, sd);
    if (ret == ERR_CODE) {
         printf("[ERRORE] Il trasferimento del file è fallito, tipo non valido\n");
        return;
    }

    printf("stai ricevendo un file di tipo %s\n", type);
    recv_file(sd, type);
    struct stat st;
    stat("recv.txt", &st);
    int size = st.st_size;
    printf("File ricevuto con successo all'interno di recv.txt, dimensione: %d bytes\n", size);
    
}
/***********/

//questa funzione viene chianmata ogni volta che c'è un sospetto di crash, 
//ossia quando si verifica un errore in fase di ricezione di un parametro o di un messaggio
int dispositivo_crashato(int sd){
    FD_CLR(sd, &master);
    close(sd);
    chatDevices--;

    if(chatDevices==0){
        server.sd = new_socket(server.port);
        if(server.sd==ERR_CODE){
            printf("[ERRORE] Server disconnesso\n");
            return 1;
        }
        send_int(NOT_BUSY, server.sd);
        send_int(current_device.id, server.sd);
        close(server.sd);
        printf("Sono pronto per una nuova chat\n");
        return 1;
    }
    return 0;
}

int check_rubrica(char* username){
    char file[1024];
    char buffer[1024];
    sprintf(file, "rubrica/%s.txt", current_device.username);
    FILE *f = fopen(file, "r");
    if (f == NULL) {
        printf("Errore durante l'apertura della rubrica\n");
        return ASSENTE;
    }
    while(fgets(buffer, 1024, f)){
        if(strncmp(buffer, username,strlen(username))==0){
            fclose(f);
            return PRESENTE;
        }
    }
    printf("L'utente %s non è nella tua rubrica\n", username);
    return ASSENTE;
    fclose(f);

}

//la funzione mostra_comandi mostra due set di comandi diversi 
//a seconda se l'utente abbia effettuato il login o meno
void mostra_comandi() {
    if (current_device.logged) {
        printf("\nScegli operazione:\n"
                "1) hanging --> controlla i messaggi pendenti\n"
                "2) show <username> --> leggi i messaggi pendenti di un utente\n"
                "3) chat <username> --> avvia una chat con un utente\n"
                "4) out --> disconnettiti\n");
    } else {
        printf("***************************** DEVICE %d *********************************\n", current_device.port);
        printf("Effettua il login o crea un account\n");
        printf("- Signup: signup <serverport> <username> <password>\n");
        printf("- Login:  in     <serverport> <username> <password>\n");
    }
}
