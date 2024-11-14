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
#define SERVER_PORT 4242
/*DEFINIZIONE STRUTTURE*/

struct Device {
    char username[50];
    char password[50];
    int id;                     //identificatore univoco, parte da 0 e viene incrementato ad ogni registrazione
    int port;
    bool online;
    bool busy; 
    char login_time[9];
    char logout_time[9];
    int sd;                     //socket di comundicazione con altro dispositivo
    bool notifica;
    int destID;
    int chat;
};

int i;

struct Device devices[MAX_DEVICES];
int registeredDevices = 0;
int connectedDevices = 0;

int serverPort;

struct messaggio_pendente{
    int num;                //numero del messaggio
    char timestamp[9];      //timestamp del messaggio
};

//matrice di messaggi pendenti, in cui ogni riga rappresenta un dispositivo che invia e ogni colonna rappresenta un dispositivo che riceve
struct messaggio_pendente messaggi_pendenti[MAX_DEVICES][MAX_DEVICES];

//utilizzo gli fd_set per il multiplexing delle operazioni di I/O, per gestire la comunicazione tra il server e più client diversi 
fd_set master;          //main set: gestita con le macro
fd_set read_fds;        //read set: gestita dalla select()
int fdmax;

//struttura per la gestione dei file
struct stat st = {0};

/*DICHIARAZIONE FUNZIONI*/
void comando_server();
void comando_client(int new_sd);
void help();
void list();
void esc();
int username_check(char username[1024]);
void signup(int sd);
int check_credenziali(char username[1024], char password[1024]);
void update_info(int id, int port, int sd);
void login(int sd);
void registra_login_logout(bool login, int id, char timestamp[9]);
void online_list(int sd);
void offline_chat(int sd);
void show(int sd);
int salvataggio_messaggi_pendenti(int sourceID, int destID, char message[1024]);
int out(int sd);
void hanging(int sd);
int device_info(int sd);
int add(int sd);
void server_info(FILE* fp);
void ripristino_messaggi(FILE* fp);
void salva_device();
void salva_messaggi();
void notifica_uscita_server();
void aggiorna_logout(int id, int sd);



/*MAIN*/
int main(int argc, char *argv[]) {

//inizializzo variabili
    int ret, sd, new_sd, i, j;
    struct sockaddr_in my_addr, cl_addr;
    socklen_t addrlen;

system("clear");
printf("***************************** SERVER STARTED *********************************\n");

if(argc==1){
    serverPort = SERVER_PORT;
}else if(argc==2){
    serverPort = atoi(argv[1]);
}else{
    printf("[ERROR]: errore di sintassi nella chiamata\n");
    exit(EXIT_FAILURE);
}
//ripristino il server e i messaggi pendenti
FILE* fp = fopen("server_info.txt", "r");
if(fp==NULL){
    printf("primo accesso al server\n");
    registeredDevices=connectedDevices=0;
}else{
    server_info(fp);
    fclose(fp);
}
fp = fopen("messaggi_pendenti.txt", "r");
if(fp){
    ripristino_messaggi(fp);
    fclose(fp);
}else{
    for(i=0; i<MAX_DEVICES; i++){
        for(j=0; j<MAX_DEVICES; j++){
            messaggi_pendenti[i][j].num=0;
            strcpy(messaggi_pendenti[i][j].timestamp, "00:00:00");
        }
    }
}




//creo il socket di ascolto a cui potranno scrivere i dispositivi

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        perror("[ERROR]: bind() didn't work as expected\n");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        perror("[ERROR]: setsockopt() didn't work as expected\n");
    }

    memset(&my_addr, 0, sizeof(my_addr)); // pulizia 
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(serverPort);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(sd, (struct sockaddr*)&my_addr, sizeof(my_addr));
    if (ret == -1) {
        perror("[ERROR]: bind() didn't work as expected\n");
        exit(EXIT_FAILURE);
    }

    ret = listen(sd, 10);
    if (ret < 0) {
        perror("[ERROR]: listen() didn't work as expected\n");
        exit(EXIT_FAILURE);
    }

    //inizializzo gli fd_set read e master
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(0, &master); //aggiunge lo standard input ai file del master
    FD_SET(sd, &master);
    fdmax = sd;         //questo è il socket di ascolto a cui potranno scrivere i dispositivi

for(;;){
    //scrivo lo stdout 
    printf("Scegli un comando:\n");
    printf("- help\n");
    printf("- list\n");
    printf("- esc\n\n");

    read_fds=master;

    ret=select(fdmax + 1, &read_fds, NULL, NULL, NULL);
    
    if(ret==-1){
            perror("[ERROR] select() didn't work as expected");
            exit(EXIT_FAILURE);
        }
    
    for (i = 0; i <= fdmax; i++) {
    if (!FD_ISSET(i, &read_fds)) {      
        continue;
    }

    if (i == 0) {               // standard input
        comando_server();
    } 
    else if (i == sd) {         // richiesta da un client
        addrlen = sizeof(cl_addr);
        new_sd = accept(sd, (struct sockaddr*)&cl_addr, &addrlen); //quì creo un nuovo socket di comunicazione con il client dove passeranno i dati
        comando_client(new_sd);                                    //quì, se il client vuole chattare con un dispositivo offline, viene aggiunto un socket al master
    } else {                
        offline_chat(i);
    }
    }
    }
}

/*FUNZIONI*/
void comando_server(){
    char command[30];
    scanf("%s", command);
    if(strncmp(command, "help", 4)==0){
        help();
        return;
    }

    if(strncmp(command, "list", 4)==0){
        list();
        return;
    }
    
    if(strncmp(command, "esc", 3)==0){
        esc();
        return;
    }
    printf("[SERVER]: comando non valido. \n"
        );
}
 
void comando_client(int new_sd) {
    int command;
    int dev;
    int sourceID;
    command = recv_int(new_sd);

    if (command == ERR_CODE) {
        printf("[ERRORE]: Qualcosa è andato storto durante l'esecuzione del comando...\n");
        close(new_sd);
        return;
    }

    if(command==SIGNUP){
    printf("Comando ricevuto: [SIGNUP]\n");
    signup(new_sd);
    return;

    }else if(command==LOGIN){
    printf("Comando ricevuto: [LOGIN]\n");
    login(new_sd);
    return;

    }else if(command==HANGING){
    printf("Comando ricevuto: [HANGING]\n");
    hanging(new_sd);
    return;


    }else if(command==CHAT){

    printf("Comando ricevuto: [CHAT]\n");
    dev = device_info(new_sd);
    //se il dispositivo è offline o busy, devo gestire la chat offline, altrimenti i client comunicano direttamente tra loro e non entro nell'if

    if (dev != ERR_CODE) {

                                        // il dispositivo è offline o busy quindi devo gestire la chat offline
    FD_SET(new_sd, &master);            //questo socket verrà filtrato dalla select e passato alla funzione offline_chat
    if (new_sd > fdmax){ 
        fdmax = new_sd; }

    sourceID = recv_int(new_sd);
    if (sourceID == ERR_CODE) {
        printf("<ERROR> Something wrong happened...\n");
        close(new_sd);
        return;
    }

    devices[sourceID].chat = new_sd;
    devices[sourceID].destID = dev;
    devices[sourceID].busy = true;
    }
    return;


    }else if(command==SHOW){
    printf("Comando ricevuto: [SHOW]\n");
    show(new_sd);
    return;

    }else if(command==OUT){
    printf("Comando ricevuto: [OUT]\n");
    out(new_sd);
    return;

    }else if(command==ADD){
    printf("Comando ricevuto: [ADD]\n");
    add(new_sd);
    return;

    }else if(command==ONLINE_LIST){
        printf("Comando ricevuto : [ONLINE_LIST]\n"); // /u
        online_list(new_sd);
    return;
    }else if(command==USER_OFFLINE){
        printf("Comando ricevuto : [DEVICE CRASHED]]\n");
        dev = out(new_sd);
        if (dev == ERR_CODE)
            return;
        
    FD_SET(new_sd, &master);
    if (new_sd > fdmax) 
    { fdmax = new_sd; }
    sourceID = recv_int(new_sd);
    printf("sourceID: %d\n", sourceID);
    if (sourceID == ERR_CODE) {
        printf("[ERRORE]: errore durante la ricezione dell'ID del dispositivo\n");
        close(new_sd);
        return;
    }
    devices[sourceID].chat = new_sd;
    devices[sourceID].destID = dev;
    devices[sourceID].busy = true;
    return;

    }else if(command==BUSY){
        dev = recv_int(new_sd);
        if (dev == ERR_CODE) {
            printf("[ERRORE]: errore durante la ricezione dell'ID del dispositivo\n");
            close(new_sd);
            return;
        }
        devices[dev].busy = true;
        printf("Comando ricevuto : %d è ora occupato\n", dev);
    return;
    }else if(command==NOT_BUSY){
        dev = recv_int(new_sd);
        if (dev == ERR_CODE) {
            printf("[ERRORE]: errore durante la ricezione dell'ID del dispositivo\n");
            close(new_sd);
            return;
        }
        devices[dev].busy = false;
        printf("Comando ricevuto : %d non è più occupato\n", dev);
    return;
    }else{
        printf("Comando non valido\n");
    } 
}


void help() {
    printf("[SERVER]: comandi disponibli:\n");
    printf("  help - Mostra i comandi disponibili\n");
    printf("  list - Lista dei device connessi\n");
    printf("  esc  - Termina il server\n\n");
}

void list() {
    int i;
    int count=0;
    struct Device* dev;

    //avrei potuto utilizzare la variabile connectedDevices, ho preferito fare un ulteriore controllo per evitare eventuali errori
    for (i = 0; i < MAX_DEVICES; i++) {
        dev=&devices[i];
        if(dev->online==true){
            count++;
        }
    }
    if(count==0){
        printf("[SERVER]: Nessun device è connesso\n\n");
        return;
    }

    printf("[SERVER] Device connessi:\n");
    printf("|username|\t|porta|\t|timestamp|\n");
    for (i = 0; i < registeredDevices; i++) {
        dev=&devices[i];
        if(dev->online==true){
        printf("%s \t\t %d \t %s\n", devices[i].username, devices[i].port, devices[i].login_time);
        }
    }
}

void esc() {
    //la esc salva i device, i messaggi e dopodiche notifica a tutti che sta andando offline
    salva_device();
    salva_messaggi();
    notifica_uscita_server();
    printf("[SERVER]: Terminando il server...\n");
    sleep(2);
    exit(0);
}

//la salva_device e la salva_messaggi sono info salvate per il ripristino, quando il server verrà riavviato
void salva_device() {
    FILE* fp = fopen("server_info.txt", "w");
    if (fp == NULL) {
        perror("Errore nell'apertura del file");
        return;
    }

    fprintf(fp, "%d\n", registeredDevices); 

    for (i = 0; i < registeredDevices; i++) {
        fprintf(fp, "%s %s %d %d %d %s %d\n", devices[i].username, devices[i].password, devices[i].id, devices[i].port, devices[i].online ? 1 : 0, devices[i].login_time, devices[i].notifica ? 1 : 0);
    }
    
    fclose(fp);
    printf("[SERVER]: Informazioni salvate\n");
}

void salva_messaggi(){
    int i,j;
    FILE* fp = fopen("messaggi_pendenti.txt", "w");
    for (i = 0; i < MAX_DEVICES; i++) {
        for (j = 0; j < MAX_DEVICES; j++) {

            fprintf(fp, "%d %s\n", messaggi_pendenti[i][j].num, messaggi_pendenti[i][j].timestamp);
        }            
            
    }
    fclose(fp);
    printf("[SERVER]: Messaggi pendenti salvati\n");
}
void notifica_uscita_server(){                                                     
    int i;
    for (i = 0; i < registeredDevices; i++) {
        if (devices[i].online == true) {
            send_int(ERR_CODE, devices[i].sd);   //invio un errore per notificare la chiusura del server
            send_int(ESC, devices[i].sd); //invio il codice per notificare la chiusura del server
        }
    }
    printf("[SERVER]: Notifica inviata\n");
}

//la server_info e la ripristino_messaggi ripristinano le informazioni del server, a seconda di come erano state salvate nella salva_device e nella salva_messaggi
void server_info(FILE* fp) {
    fscanf(fp, "%d\n", &registeredDevices);
    for (i = 0; i < registeredDevices; i++) {
        int online_int, notifica_int;
        fscanf(fp, "%s %s %d %d %d %s %d\n", devices[i].username, devices[i].password, &devices[i].id, &devices[i].port, &online_int, devices[i].login_time, &notifica_int);
        if(online_int != 0){
            devices[i].online = true; 
        }
        else{
            devices[i].online = false;
        }
        if(notifica_int != 0){
            devices[i].notifica = true; 
        }
        else{
            devices[i].notifica = false;
        }
    }
    printf("[SERVER]: Informazioni caricate\n");
}

void ripristino_messaggi(FILE* fp) {
    int num;
    char timestamp[9];
    int sourceID, destID;

    while (fscanf(fp, "%d %d %d %8s", &sourceID, &destID, &num, timestamp) == 4) {
        
        if (sourceID >= MAX_DEVICES || destID >= MAX_DEVICES) {
            printf("Errore: Indici di dispositivi non validi.\n");
            continue;
        }

        messaggi_pendenti[sourceID][destID].num = num;
        strcpy(messaggi_pendenti[sourceID][destID].timestamp, timestamp);
    }
}

//controlla che l'username non esista già
int username_check(char username[1024]) {
    struct Device* dev;
    int i;
    for (i = 0; i < registeredDevices; i++) {
        dev = &devices[i];
        if (strcmp(dev->username, username)==0)
            return 1;
    }
    return 0;
}

void signup(int sd) {
    int ret;
    if (registeredDevices < MAX_DEVICES) {
        char username[1024];
        char password[1024];

        ret = recv_msg(username, sd);
        if(ret==ERR_CODE){
            printf("[ERROR]: errore durante la ricezione del nome utente\n");
            close(sd);
            return;
        }

        ret = recv_msg(password, sd);
        if(ret==ERR_CODE){
            printf("[ERROR]: errore durante la ricezione della password\n");
            close(sd);
            return;
        }

        if (username_check(username)) {
            printf("Server: Username %s già in uso. Registrazione fallita.\n", username);
            send_int(USED_USERNAME, sd);
            close(sd);
            return;
        }

        else {
            send_int(registeredDevices, sd);
            struct Device newDevice;
            strcpy(newDevice.username, username);
            strcpy(newDevice.password, password);
            newDevice.id = registeredDevices; // assegnamento di un ID univoco
            newDevice.online = false;
            strcpy(newDevice.login_time, "00:00:00"); // imposta il momento in cui ha fatto il login, a "00.00.00" perchPé verrà impostato quando ci sarà un effettivo login
            devices[registeredDevices] = newDevice;
            printf("Server: Registrazione del dispositivo %s avvenuta con successo, ID: %d\n", username, devices[registeredDevices].id);
            registeredDevices++;
        }
    } else {
        printf("Server: Numero massimo di dispositivi raggiunto. Impossibile registrare nuovo dispositivo.\n");
        send_int(DEVICES_FULL, sd);   
    }
}

//controlla le credenziali
int check_credenziali(char username[1024], char password[1024]) {
    int i;
    for (i = 0; i < registeredDevices; i++) {
        if ((strcmp(devices[i].username, username) == 0)&& (strcmp(devices[i].password, password)==0)) {
            return i;
        }
    }
    return -1;
}

// funzione chiamata alla ricezione dei comandi CHAT e \a
// riceve in ingresso un username ed invia i valori dell'id e della porta del dispositivo relativo
int device_info(int sd) {
    int i;
    char username[1024];
    int ret;
    ret = recv_msg(username, sd);
    if(ret==ERR_CODE){
        printf("[ERROR]: errore durante la ricezione del nome utente\n");
        close(sd);
        return ERR_CODE;
    }
    for (i = 0; i <= registeredDevices; i++) {
        if (strcmp(devices[i].username, username) == 0) {
            send_int(devices[i].id, sd);
    printf("ID: %d\n", devices[i].id);
            if(devices[i].online==false){
                send_int(OFFLINE, sd);
    printf("Device offline\n");
                return devices[i].id;
            }
            if(devices[i].busy==true){
                send_int(USER_BUSY, sd);
                printf("Device busy\n");
                return devices[i].id;
            }
            
            send_int(devices[i].port, sd);
            printf("Port: %d\n", devices[i].port);
            return ERR_CODE;
        }
    }
    printf("[ERRORE]: Dispositivo non trovato\n");
    send_int(USER_NOT_FOUND, sd);
    send_int(USER_NOT_FOUND, sd);
    return ERR_CODE;
}

//alias per leggibilità
int add(int sd){
 return device_info(sd);
}

//chiamata dal login, aggiorna le info del device
void update_info(int id, int port, int sd) {                                       
    int notificaSD;
    int codice;
    devices[id].port = port;
    devices[id].online = true;
    devices[id].sd = sd;
    devices[id].busy = false;
    strcpy(devices[id].login_time, tempo_attuale());


    if(devices[id].notifica==true){
    notificaSD = new_socket(devices[id].port);
    if (notificaSD == ERR_CODE) {
        printf("[SHOW] %d is offline, I'll notify the show when it comes back\n", id);
        devices[id].notifica = true;
    } else {
        send_int(SHOW, notificaSD);
        close(notificaSD);
    }
    }
    devices[id].notifica = false;

    codice=recv_int(sd);
    if(codice==ERR_CODE){
        printf("[ERROR]: errore durante la ricezione del codice\n");
        close(sd);
        return;
    }
    //se il dispositivo aveva effettuato il logout mentre il server era online, me lo notifica
    if(codice==LOGOUT){
        aggiorna_logout(id, sd);
    }
                                                                                       

    connectedDevices++;
    registra_login_logout(true, id, devices[id].login_time);
}

void aggiorna_logout(int id, int sd){
    char buffer[1024];
    char timestamp[10];
    printf("Sto aggiornando il logout del dispositivo %d\n", id);  
    recv_file(sd, "txt");
    FILE *fp = fopen("./recv.txt", "r");
    if (fp == NULL) {
        perror("[ERROR]: errore durante l'apertura del file\n");
        close(sd);
        return;
    }
    fgets(buffer, 1024, fp);
    printf("il file contiene: %s\n", buffer);
    strncpy(timestamp, buffer, 9);
    timestamp[9] = '\0';
    printf("timestamp: %s\n", timestamp);
    registra_login_logout(false, id, timestamp);
}

void login(int sd) {                
    int ret;
    char username[1024];
    char password[1024];
    int id, port;
    ret = recv_msg(username, sd);
    if(ret==ERR_CODE){
        printf("[ERROR]: errore durante la ricezione del nome utente\n");
        close(sd);
        return;
    }
    ret = recv_msg(password, sd);
    if(ret==ERR_CODE){
        printf("[ERROR]: errore durante la ricezione della password\n");
        close(sd);
        return;
    }
    ret = recv_int(sd);
    if(ret==ERR_CODE){
        printf("[ERROR]: errore durante la ricezione della porta\n");
        close(sd);
        return;
    }
    port = ret;
    printf("Username: %s, Password: %s, Port: %d\n", username, password, port);

    id = check_credenziali(username, password);
    printf("Id: %d\n", id);

    if (id == -1) {
        printf("[IN] Fail\n");
        send_int(ERR_CODE, sd);
    }
    else {
        //se il login ha successo, chiamo la update_info che mi aggiorna la struttura dati
        printf("[IN] Success\n");
        printf("Socket: %d, Id: %d, Port: %d\n", sd, id, port);
        send_int(id, sd);
        update_info(id, port, sd);
    }
}

//funzione che mi salva i login e i logout nell'apposito registro
void registra_login_logout(bool login, int id, char timestamp[9]){
    char dir_path[1024];
    char file_path[1024];
    sprintf(dir_path, "./registrologin");
    if(stat(dir_path, &st) == -1){
        mkdir(dir_path, 0700);
    }
    sprintf(file_path, "%s/%d.txt", dir_path, id);
    FILE* file = fopen(file_path, "a");
    if(login){
        fprintf(file, "Port: %d, Login: %s\n", devices[id].port, timestamp);
    }else{
        fprintf(file, "Logout: %s\n", timestamp);
    }
    fclose(file);
    printf("Registro login/logout aggiornato\n");
}

void online_list(int sd){
    int i;
    //qui mando anche il numero di dispositivi connessi, così il client sa quanti messaggi ricevere
    int count=0;                                            
    for (i = 0; i < registeredDevices; i++) {
        if (devices[i].online == true) {
            count++;
        }
    }
    send_int(count, sd);
    for (i = 0; i < registeredDevices; i++) {
        if (devices[i].online == true) {
            send_msg(devices[i].username, sd);
        }
    }
    printf("Lista inviata\n");
}

void offline_chat(int sd) {
    //questa funzione viene chiamata se qualcuno voleva chattare con un device che però è offline
    //il server si occupa di salvare i messaggi nei messaggi pendenti
    int sourceID;
    int destID;
    int i; 
    int ret;
    char message[1024];

    for(i=0; i<registeredDevices; i++){
        if(FD_ISSET(devices[i].chat, &read_fds)&&(devices[i].chat==sd)){
            sourceID = i;
            printf("sourceID: %d\n", sourceID);
            break;
        }
    }
    destID = devices[sourceID].destID;
    
    ret = recv_msg(message, sd);
    if(ret==ERR_CODE){
        printf("[ERROR]: errore durante la ricezione del messaggio\n");
        FD_CLR(devices[sourceID].chat, &master);
        close(devices[sourceID].chat);
        close(sd);
        return;
    }
    
    if(strcmp(message, "/q\n")==0){
        printf("Chat terminata\n");
        FD_CLR(devices[sourceID].chat, &master);
        close(devices[sourceID].chat);
        devices[sourceID].busy = false;
        devices[sourceID].chat = -1;
        return;
    }

    printf("Messaggio ricevuto: %s\n", message);
    printf("Mittente: %d, Destinatario: %d\n", sourceID, destID);
    ret = salvataggio_messaggi_pendenti(sourceID, destID, message);
    if(ret==ERR_CODE){
        perror("[ERRORE]: errore durante il salvataggio del messaggio\n");
        close(sd);
        exit(-1);
    }
    messaggi_pendenti[sourceID][destID].num++;
    strcpy(messaggi_pendenti[sourceID][destID].timestamp, tempo_attuale());
}

//funzione chiamata in risposta alla show da parte di un device
void show(int sd) {
    int sourceID;
    int destID;
    int i;
    int notificaSD;
    char source_username[1024];
    char file[1024];

    destID = recv_int(sd);
    recv_msg(source_username, sd);

    for (i = 0; i < registeredDevices; i++) {
        if (strcmp(devices[i].username, source_username) == 0) {
            sourceID = i;
            break;
        }
    }

    sprintf(file, "./messaggi_pendenti/%d/%d.txt", destID, sourceID);
    printf("File: %s\n", file);
    FILE* fp = fopen(file, "r");
    if (fp == NULL) {
        perror("[ERROR]: errore durante l'apertura del file\n");
        send_int(ERR_CODE, sd);
        return;
    }
    send_int(MSG, sd); 
    
    send_file(fp, sd);
    printf("File inviato\n");
    fclose(fp);
    remove(file);
    printf("File rimosso\n");

    //resetto i messaggi pendenti
    messaggi_pendenti[sourceID][destID].num = 0;
    strcpy(messaggi_pendenti[sourceID][destID].timestamp, "00:00:00");
 
    // notifico il dispositivo che aveva inviato i messaggi che i messaggi sono stati letti
    sleep(2);  // aggiungo un breve ritardo per evitare problemi di sincronizzazione
    notificaSD = new_socket(devices[sourceID].port);
    if (notificaSD == ERR_CODE) {
        printf("[SHOW] %d is offline, I'll notify the show when it comes back\n", sourceID);
        devices[sourceID].notifica = true;
    } else {
        send_int(SHOW, notificaSD);
        close(notificaSD);
    }
}

//salvo i messaggi pendengti
int salvataggio_messaggi_pendenti(int sourceID, int destID, char message[1024]){
    char path[1024];
    char file[1024];
    FILE* fp;
    struct stat st;

    sprintf(path, "./messaggi_pendenti");
    if(stat(path, &st) == -1){
        if(mkdir(path, 0700) != 0){
            printf("[ERROR]: Impossibile creare la directory %s\n", path);
            return ERR_CODE;
        }
    }

    sprintf(path, "./messaggi_pendenti/%d", destID);
    if(stat(path, &st) == -1){
        if(mkdir(path, 0700) != 0){
            printf("[ERROR]: Impossibile creare la directory %s\n", path);
            return ERR_CODE;
        }
    }

    sprintf(file, "%s/%d.txt", path, sourceID);
    printf("File: %s\n", file);

    fp = fopen(file, "a");
    if(fp == NULL){
        printf("[ERROR]: errore durante l'apertura del file dei messaggi pendenti\n");
        return ERR_CODE;
    }
    fprintf(fp, "%s", message);
    fclose(fp);
    printf("Messaggio salvato\n");
    return 1;
}

//funzione che permette il logout, si salva il logout_time
int out(int sd) {
    int dev_id;
   dev_id = recv_int(sd);
    if (dev_id == ERR_CODE) {
        printf("[ERROR]: errore durante la ricezione dell'ID del dispositivo\n");
        close(sd);
        return ERR_CODE;
    }
    devices[dev_id].online = false;
    strcpy(devices[dev_id].logout_time, tempo_attuale());
    connectedDevices--;
    registra_login_logout(false, dev_id, devices[dev_id].logout_time);
    printf("Device %d disconnesso\n", dev_id);
    return dev_id;
}

//risponde alla hanging
void hanging(int sd) {
    int i, id;
    int count = 0;

    id= recv_int(sd);
    if (id == ERR_CODE) {
        printf("[ERROR]: errore durante la ricezione dell'ID del dispositivo\n");
        close(sd);
        return;
    }

    for (i = 0; i < registeredDevices; i++) {
        if (messaggi_pendenti[i][id].num > 0) {
            count++;
        }
    }
    //mando il count cosi da permettere di gestire correttamente il for
    send_int(count, sd);
    for (i = 0; i < registeredDevices; i++) {
        if (messaggi_pendenti[i][id].num > 0) {
            send_msg(devices[i].username, sd);
            send_int(messaggi_pendenti[i][id].num, sd);
            send_msg(messaggi_pendenti[i][id].timestamp, sd);
        }
    }
    printf("Lista inviata\n");
}
