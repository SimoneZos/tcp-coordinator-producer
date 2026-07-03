#include <stdio.h>      
#include <stdlib.h>     
#include <unistd.h>     
#include <sys/types.h>  
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>  // Funzioni conversione indirizzi IP (inet_aton)

#define PORT 8080 

// Struttura dati condivisa con il server
typedef struct {
    int id_mittente;
    int dato_numerico;
} Messaggio;

int main(int argc, char *argv[]) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    Messaggio msg;

    // Utilizzo del PID del processo come identificatore univoco
    msg.id_mittente = getpid(); 
    
    // Assegnazione del dato (da argomento o default)
    if (argc > 1) {
        msg.dato_numerico = atoi(argv[1]);
    } else {
        msg.dato_numerico = 42;
    }

    // Creazione socket TCP
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Errore nella creazione del socket");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Conversione IP localhost nel formato binario di rete
    if (inet_aton("127.0.0.1", &serv_addr.sin_addr) == 0) {
        fprintf(stderr, "Indirizzo IP non valido\n");
        exit(EXIT_FAILURE);
    }

    // Instaurazione del Three-way Handshake TCP
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Errore di connessione al Coordinatore");
        exit(EXIT_FAILURE);
    }

    printf("[Produttore %d] Connesso al Coordinatore. Invio dato: %d\n", msg.id_mittente, msg.dato_numerico);

    // Scrittura della struct sul socket
    if (write(sock, &msg, sizeof(Messaggio)) < 0) {
        perror("Errore nell'invio del messaggio");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("[Produttore %d] Messaggio inviato con successo. Disconnessione...\n", msg.id_mittente);

    // Chiusura del socket
    close(sock);

    return EXIT_SUCCESS;
}
