# Regola predefinita: compila entrambi i file sorgente e genera gli eseguibili
all: serv dev

# Regola per compilare il file sorgente del server
serv: serv.o
	gcc -Wall serv.o -o serv

# Regola per compilare il file sorgente del client
dev: dev.o
	gcc -Wall dev.o -o dev

# Regola per compilare il file sorgente del server in un file oggetto
serv.o: serv.c
	gcc -c -Wall serv.c -o serv.o

# Regola per compilare il file sorgente del client in un file oggetto
dev.o: dev.c
	gcc -c -Wall dev.c -o dev.o

# Regola per eseguire il debugger su entrambi i file sorgente
gdb:
	gcc -g -Wall dev.c -o dev
	gcc -g -Wall serv.c -o serv

# Pulisci i file oggetto e gli eseguibili generati
clean:
	rm -f *.o serv dev
#Francesco Moretti