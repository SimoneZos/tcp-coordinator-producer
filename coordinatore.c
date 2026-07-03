#include <stdio.h>      // I/O standard
#include <stdlib.h>     // Utility standard (exit, macro EXIT_SUCCESS/FAILURE)
#include <string.h>     // Manipolazione stringhe e buffer
#include <unistd.h>     // System call POSIX (close, read, write, fork, alarm)
#include <sys/types.h>  // Tipi di base del sistema (pid_t, socklen_t)
#include <sys/socket.h> // System call per socket (socket, bind, listen, accept, setsockopt)
#include <netinet/in.h> // Strutture per indirizzi di rete (sockaddr_in, htons)
#include <fcntl.h>      // Manipolazione file descriptor (open, e fcntl per i lock)
#include <time.h>       // Gestione tempo per il timestamp del log
#include <sys/wait.h>   // System call per sincronizzazione processi (wait, waitpid)
#include <sys/stat.h>   // Recupero metadati sui file (stat per dimensione log)
#include <signal.h>     // Gestione asincrona dei segnali (signal, kill)

#define PORT 8080
#define BACKLOG 5
#define MAX_LOG_SIZE 1024
#define ALARM_INTERVAL 1

// Variabili globali necessarie per gli handler dei segnali
int server_fd_global;  
int client_id_corrente;  


typedef struct {
    int id_mittente;
    int dato_numerico;
} Messaggio;

// Handler SIGINT: Terminazione controllata
void handle_sigint(int sig) {
    printf("\n[Coordinatore] Ricevuto SIGINT. Avvio terminazione controllata...\n");
    
    // Chiude il socket in stato LISTENING
    close(server_fd_global); 
    
    // Attende la terminazione di tutti i figli (processi in scrittura)
    while(wait(NULL) > 0); 
    
    printf("[Coordinatore] Tutti i processi terminati. Chiusura file di log completata.\n");
    exit(EXIT_SUCCESS);
}

// Handler SIGALRM: Controllo periodico e rotazione del log
void handle_sigalrm(int sig) {
    struct stat st;
    if (stat("log.txt", &st) == 0) {
        if (st.st_size >= MAX_LOG_SIZE) {
            rename("log.txt", "vecchio_log.txt");
            printf("[Coordinatore] File log archiviato (dimensione massima raggiunta).\n");
        }
    }
    // Riavvia il timer
    alarm(ALARM_INTERVAL);
}

// Handler SIGPIPE: Gestione disconnessione client
void handle_sigpipe(int sig) {
    printf("[Produttore %d] Connessione interrotta (SIGPIPE).\n", client_id_corrente);
    
    int log_fd = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd >= 0) {
        // Setup del lock bloccante in scrittura
        struct flock fl;
        fl.l_type = F_WRLCK;
        fl.l_whence = SEEK_END;
        fl.l_start = 0;
        fl.l_len = 0;
        
        fcntl(log_fd, F_SETLKW, &fl); 
        
        time_t now = time(NULL);
        char *time_str = ctime(&now);
        time_str[strlen(time_str) - 1] = '\0'; // Rimuove \n sostituendolo
        
        char buffer[256];
        int n_bytes = snprintf(buffer, sizeof(buffer), "[%s, %d, \"DISCONNECT\"]\n", time_str, client_id_corrente);
        write(log_fd, buffer, n_bytes);
        
        // Rilascio lock
        fl.l_type = F_UNLCK;
        fcntl(log_fd, F_SETLK, &fl); 
        
        close(log_fd);
    }
    // Uscita con FAILURE perché la disconnessione improvvisa è considerata un'anomalia
    exit(EXIT_FAILURE);
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    // Registrazione handler per i segnali
    signal(SIGINT, handle_sigint);
    signal(SIGALRM, handle_sigalrm);
    signal(SIGPIPE, handle_sigpipe); 

    // Avvio timer periodico per il controllo dimensione log
    alarm(ALARM_INTERVAL);

    // Creazione socket TCP (IPv4, Stream)
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { 
        perror("Errore socket"); 
        exit(EXIT_FAILURE); 
    }

    // Configurazione socket per riuso rapido IP/Porta
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Errore setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(PORT);      

    // Associazione socket all'indirizzo e porta
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Errore bind"); 
        exit(EXIT_FAILURE);
    }

    // Server in stato di listening
    if (listen(server_fd, BACKLOG) < 0) {
        perror("Errore listen"); 
        exit(EXIT_FAILURE);
    }

    server_fd_global = server_fd; 
    printf("Coordinatore avviato. In ascolto sulla porta %d...\n", PORT);

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // ciclo infinito: Il server attende connessioni e le gestisce
    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("Errore accept");
            continue; 
        }

        // Delegazione della connessione a un processo figlio
        pid_t pid = fork();
        if (pid < 0) {
            perror("Errore fork");
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            // PROCESSO FIGLIO: Gestisce il client
            close(server_fd); // Chiude la copia del socket di ascolto

            Messaggio msg;
            int n = read(client_fd, &msg, sizeof(Messaggio));
            
            if (n > 0) {
                client_id_corrente = msg.id_mittente;
                
                int log_fd = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (log_fd < 0) {
                    perror("Errore apertura log");
                    exit(EXIT_FAILURE);
                }

                // configurazione del lock
                struct flock fl;
                fl.l_type = F_WRLCK;     
                fl.l_whence = SEEK_END;  
                fl.l_start = 0;          
                fl.l_len = 0;            
                fcntl(log_fd, F_SETLKW, &fl);

                time_t now = time(NULL);
                char *time_str = ctime(&now); 
                time_str[strlen(time_str) - 1] = '\0'; 

                char buffer[256];
                int n_bytes = snprintf(buffer, sizeof(buffer), "[%s, %d, %d]\n", 
                                       time_str, msg.id_mittente, msg.dato_numerico);

                write(log_fd, buffer, n_bytes);

                // Rilascio del lock
                fl.l_type = F_UNLCK;
                fcntl(log_fd, F_SETLK, &fl);
                close(log_fd);
                
                // Attesa chiusura connessione (EOF) per simulare l'anomalia SIGPIPE
                int n2 = read(client_fd, &msg, sizeof(Messaggio));
                if (n2 <= 0) {
                    kill(getpid(), SIGPIPE);
                }
                
            } else {
                // Se la prima read fallisce subito
                kill(getpid(), SIGPIPE); 
            }

            close(client_fd);
            exit(EXIT_SUCCESS);
        } else {
            // PROCESSO PADRE: Chiude il socket del client
            close(client_fd);
            while(waitpid(-1, NULL, WNOHANG) > 0);
        }
    }

    return EXIT_SUCCESS; 
}
