/*=====================================
Assignment 6 Submission
Name: G SAI SHASANK
Roll number: 22Cs10025
(Updated with file and shared memory locks)
======================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>      /* For file locking operations */
#include <sys/file.h>   /* For flock() */
#include <sys/mman.h>   /* For shared memory operations */
#include <sys/ipc.h>    /* For IPC mechanisms */
#include <sys/sem.h>    /* For semaphore operations */
#include <errno.h>      /* For error handling */
#include <signal.h>     /* For signal handling */

#define BUFFER_SIZE 1024
#define MAILBOX_PATH "mailbox/"
#define SEM_KEY 12345  /* Key for semaphore to protect shared data */

/* Shared memory structure to track active connections */
typedef struct {
    int active_connections;
    pid_t connection_pids[100];  /* Store up to 100 connection PIDs */
} connection_tracker_t;

/* Global variables */
connection_tracker_t *conn_tracker = NULL;  /* Shared memory pointer */
int shared_mem_fd = -1;                     /* Shared memory file descriptor */
int sem_id = -1;                            /* Semaphore ID for sync */

/* Semaphore operations */
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

/* Initialize semaphore */
int init_semaphore() {
    int id = semget(SEM_KEY, 1, 0666 | IPC_CREAT);
    if (id == -1) {
        perror("semget failed");
        return -1;
    }
    
    union semun arg;
    arg.val = 1;  /* Initialize semaphore to 1 (available) */
    if (semctl(id, 0, SETVAL, arg) == -1) {
        perror("semctl failed");
        return -1;
    }
    
    return id;
}

/* Lock semaphore (P operation) */
void sem_lock(int sem_id) {
    struct sembuf sb = {0, -1, 0};  /* Decrement by 1 */
    if (semop(sem_id, &sb, 1) == -1) {
        perror("semop lock failed");
        exit(1);
    }
}

/* Unlock semaphore (V operation) */
void sem_unlock(int sem_id) {
    struct sembuf sb = {0, 1, 0};  /* Increment by 1 */
    if (semop(sem_id, &sb, 1) == -1) {
        perror("semop unlock failed");
        exit(1);
    }
}

/* Initialize shared memory */
void init_shared_memory() {
    /* Create or open shared memory object */
    shared_mem_fd = shm_open("/smtp_connections", O_CREAT | O_RDWR, 0666);
    if (shared_mem_fd == -1) {
        perror("shm_open failed");
        exit(1);
    }
    
    /* Set the size of the shared memory object */
    if (ftruncate(shared_mem_fd, sizeof(connection_tracker_t)) == -1) {
        perror("ftruncate failed");
        exit(1);
    }
    
    /* Map the shared memory object into this process's memory space */
    conn_tracker = mmap(NULL, sizeof(connection_tracker_t), 
                         PROT_READ | PROT_WRITE, MAP_SHARED, 
                         shared_mem_fd, 0);
    
    if (conn_tracker == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    
    /* Initialize tracker values */
    sem_lock(sem_id);
    conn_tracker->active_connections = 0;
    memset(conn_tracker->connection_pids, 0, sizeof(conn_tracker->connection_pids));
    sem_unlock(sem_id);
}

/* Cleanup shared memory */
void cleanup_shared_memory() {
    if (conn_tracker != NULL) {
        munmap(conn_tracker, sizeof(connection_tracker_t));
    }
    
    if (shared_mem_fd != -1) {
        close(shared_mem_fd);
        shm_unlink("/smtp_connections");
    }
    
    if (sem_id != -1) {
        semctl(sem_id, 0, IPC_RMID);
    }
}

/* Register a new connection in shared memory */
void register_connection(pid_t pid) {
    if (conn_tracker == NULL) return;
    
    sem_lock(sem_id);
    
    if (conn_tracker->active_connections < 100) {
        conn_tracker->connection_pids[conn_tracker->active_connections] = pid;
        conn_tracker->active_connections++;
        printf("New connection registered. Total active: %d\n", 
               conn_tracker->active_connections);
    }
    
    sem_unlock(sem_id);
}

/* Unregister a connection from shared memory */
void unregister_connection(pid_t pid) {
    if (conn_tracker == NULL) return;
    
    sem_lock(sem_id);
    
    for (int i = 0; i < conn_tracker->active_connections; i++) {
        if (conn_tracker->connection_pids[i] == pid) {
            /* Shift remaining elements to fill the gap */
            for (int j = i; j < conn_tracker->active_connections - 1; j++) {
                conn_tracker->connection_pids[j] = conn_tracker->connection_pids[j+1];
            }
            conn_tracker->active_connections--;
            printf("Connection unregistered. Total active: %d\n", 
                   conn_tracker->active_connections);
            break;
        }
    }
    
    sem_unlock(sem_id);
}

/* Signal handler for child process termination */
void sigchld_handler(int sig) {
    pid_t pid;
    int status;
    
    /* Reap all terminated children */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Child process %d terminated\n", pid);
        unregister_connection(pid);
    }
}

/* Signal handler for server termination */
void sigterm_handler(int sig) {
    printf("Server shutting down...\n");
    cleanup_shared_memory();
    exit(0);
}

/* Utility function to send responses */
void send_response(int clientfd, const char *message) {
    write(clientfd, message, strlen(message));
    printf("Sent to client: %s", message);
}

/* Get current date in string format */
void get_date(char *date_str, int size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(date_str, size, "%d-%m-%Y", t);
}

/* Ensure mailbox directory exists */
void ensure_mailbox_dir() {
    struct stat st = {0};
    if (stat(MAILBOX_PATH, &st) == -1) {
        mkdir(MAILBOX_PATH, 0700);
    }
}

/* Extract username from email address (before the @) */
void extract_username(const char *email, char *username, int size) {
    strncpy(username, email, size);
    username[size-1] = '\0';  /* Ensure null-termination */
    
    char *at_sign = strchr(username, '@');
    if (at_sign) {
        *at_sign = '\0';  /* Truncate at the @ sign */
    }
}

/* Get lock on a file for concurrent access control */
FILE* get_locked_file(const char *path, const char *mode, int *lock_fd) {
    /* Open file for both reading and writing to enable locking */
    *lock_fd = open(path, O_CREAT | O_RDWR, 0666);
    if (*lock_fd == -1) {
        perror("File open for locking failed");
        return NULL;
    }
    
    /* Try to acquire exclusive lock */
    if (flock(*lock_fd, LOCK_EX) == -1) {
        perror("flock failed");
        close(*lock_fd);
        return NULL;
    }
    
    /* Open the file stream with the requested mode */
    FILE *file = fopen(path, mode);
    if (!file) {
        perror("File open failed after lock");
        flock(*lock_fd, LOCK_UN);  /* Release lock */
        close(*lock_fd);
        return NULL;
    }
    
    return file;
}

/* Release lock on a file */
void release_file_lock(FILE *file, int lock_fd) {
    if (file) {
        fclose(file);
    }
    
    if (lock_fd != -1) {
        flock(lock_fd, LOCK_UN);  /* Release lock */
        close(lock_fd);
    }
}

/* Save a new email to the recipient's mailbox */
void save_email(const char *recipient, const char *sender, const char *data) {
    char mailbox_path[BUFFER_SIZE];
    snprintf(mailbox_path, sizeof(mailbox_path), "%s%s.txt", MAILBOX_PATH, recipient);
    
    /* Get current date */
    char date[20];
    get_date(date, sizeof(date));
    
    int lock_fd = -1;
    FILE *file = get_locked_file(mailbox_path, "a", &lock_fd);
    
    if (file) {
        fprintf(file, "From: %s\nDate: %s\n%s\n----END_OF_MAIL----\n", 
                sender, date, data);
        release_file_lock(file, lock_fd);
        printf("Email written to %s with proper locking\n", mailbox_path);
    } else {
        printf("Failed to acquire lock or open file %s\n", mailbox_path);
    }
}

/* List all emails for a given recipient */
void list_emails(int clientfd, const char *recipient) {
    /* Extract username from recipient email */
    char username[100];
    extract_username(recipient, username, sizeof(username));
    
    char mailbox_path[BUFFER_SIZE];
    snprintf(mailbox_path, sizeof(mailbox_path), "%s%s.txt", MAILBOX_PATH, recipient);
    
    int lock_fd = -1;
    FILE *file = get_locked_file(mailbox_path, "r", &lock_fd);
    
    if (!file) {
        send_response(clientfd, "401 NOT FOUND\n");
        return;
    }
    
    send_response(clientfd, "200 OK\n");
    
    char line[BUFFER_SIZE];
    int mail_id = 0;
    char sender[100], date[20];
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "From: ", 6) == 0) {
            mail_id++;
            sscanf(line, "From: %s", sender);
            
            /* Get date from next line */
            if (fgets(line, sizeof(line), file) && strncmp(line, "Date: ", 6) == 0) {
                sscanf(line, "Date: %s", date);
                
                /* Send mail info */
                char mail_info[BUFFER_SIZE];
                snprintf(mail_info, sizeof(mail_info), "%d: Email from %s (%s)\n", 
                         mail_id, sender, date);
                send_response(clientfd, mail_info);
                
                /* Skip until end of mail marker */
                while (fgets(line, sizeof(line), file) && strncmp(line, "----END_OF_MAIL----", 19) != 0) {
                    /* Do nothing, just consuming lines */
                }
            }
        }
    }

    send_response(clientfd,"200 OK\n");    /* to indicate end of list */
    release_file_lock(file, lock_fd);
}

/* Get a specific email by ID */
void get_email(int clientfd, const char *recipient, int mail_id) {
    /* Extract username from recipient email */
    char username[100];
    extract_username(recipient, username, sizeof(username));
    
    char mailbox_path[BUFFER_SIZE];
    snprintf(mailbox_path, sizeof(mailbox_path), "%s%s.txt", MAILBOX_PATH, recipient);
    
    int lock_fd = -1;
    FILE *file = get_locked_file(mailbox_path, "r", &lock_fd);
    
    if (!file) {
        send_response(clientfd, "401 NOT FOUND\n");
        return;
    }
    
    char line[BUFFER_SIZE];
    int current_id = 0;
    int found = 0;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "From: ", 6) == 0) {
            current_id++;
            
            if (current_id == mail_id) {
                found = 1;
                send_response(clientfd, "200 OK\n");
                send_response(clientfd, line); /* From line */
                
                /* Read and send until end of mail marker */
                while (fgets(line, sizeof(line), file) && 
                       strncmp(line, "----END_OF_MAIL----", 19) != 0) {
                    send_response(clientfd, line);
                }
                send_response(clientfd,"200 OK\n");
                break;
            } else {
                /* Skip until end of mail marker */
                while (fgets(line, sizeof(line), file) && 
                       strncmp(line, "----END_OF_MAIL----", 19) != 0) {
                    /* Do nothing, just consuming lines */
                }
            }
        }
    }
    
    if (!found) {
        send_response(clientfd, "401 NOT FOUND\n");
    }
    
    release_file_lock(file, lock_fd);
}

/* Handle a client connection */
void handle_client(int clientfd) {
    char buffer[BUFFER_SIZE];
    char sender[100] = "";
    char recipient[100] = "";
    char mail_data[BUFFER_SIZE * 10] = ""; /* Larger buffer for email content */
    char domain[100] = "";    /* for one connection we need to store the domain name */
    int helo_received = 0;    /* Flag to track if HELO has been received */
    
    /* Register this connection in shared memory */
    pid_t my_pid = getpid();
    register_connection(my_pid);
    
    send_response(clientfd, "220 My_SMTP Server Ready\n");
    
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = read(clientfd, buffer, BUFFER_SIZE - 1);
        
        if (bytes_read <= 0) {
            printf("Client disconnected.\n");
            unregister_connection(my_pid);
            close(clientfd);
            exit(0);
        }
        
        buffer[bytes_read] = '\0';
        printf("Received: %s", buffer);
        
        /* HELO Command */
        if (strncmp(buffer, "HELO", 4) == 0) {
            if (sscanf(buffer, "HELO %s", domain) == 1) {
                printf("HELO received from domain %s\n", domain);
                helo_received = 1;
                send_response(clientfd, "200 OK\n");
            } else {
                send_response(clientfd, "400 ERR Invalid command syntax\n");
            }
        }
        
        /* MAIL FROM Command */
        else if (strncmp(buffer, "MAIL FROM:", 10) == 0 || strncmp(buffer, "MAIL FROM: ", 11) == 0) {
            /* Check if HELO has been received */
            if (!helo_received) {
                send_response(clientfd, "400 ERR HELO command required first\n");
                continue;
            }
            
            if (sscanf(buffer, "%*[^:]: %s", sender) == 1) {
                printf("MAIL FROM: %s\n", sender);
                
                /* Check if sender contains @ and extract domain */
                char *at_sign = strchr(sender, '@');
                if (at_sign == NULL) {
                    send_response(clientfd, "400 ERR Invalid email format\n");
                    continue;
                }
                
                char *sender_domain = at_sign + 1;
                if (strcmp(sender_domain, domain) != 0) {
                    send_response(clientfd, "400 ERR Invalid domain\n");
                    continue;
                } else {
                    send_response(clientfd, "200 OK\n");
                }
            } else {
                send_response(clientfd, "400 ERR Invalid command syntax\n");
            }
        }
        
        /* RCPT TO Command */
        else if (strncmp(buffer, "RCPT TO:", 8) == 0 || strncmp(buffer, "RCPT TO: ", 9) == 0) {
            /* Check if HELO has been received */
            if (!helo_received) {
                send_response(clientfd, "400 ERR HELO command required first\n");
                continue;
            }
            
            /* Check if MAIL FROM has been received */
            if (strlen(sender) == 0) {
                send_response(clientfd, "400 ERR MAIL FROM command required first\n");
                continue;
            }
            
            if (sscanf(buffer, "%*[^:]: %s", recipient) == 1) {
                printf("RCPT TO: %s\n", recipient);
                
                /* Check if recipient contains @ and extract domain */
                char *at_sign = strchr(recipient, '@');
                if (at_sign == NULL) {
                    send_response(clientfd, "400 ERR Invalid email format\n");
                    continue;
                }
                
                char *recipient_domain = at_sign + 1;
                if (strcmp(recipient_domain, domain) != 0) {
                    send_response(clientfd, "400 ERR Invalid domain\n");
                    continue;
                } else {
                    ensure_mailbox_dir(); /* Make sure mailbox directory exists */
                    send_response(clientfd, "200 OK\n");
                }
            } else {
                send_response(clientfd, "400 ERR Invalid command syntax\n");
            }
        }
        
        /* DATA Command */
        else if (strncmp(buffer, "DATA", 4) == 0) {
            if (strlen(sender) == 0 || strlen(recipient) == 0) {
                send_response(clientfd, "403 FORBIDDEN Need MAIL FROM and RCPT TO first\n");
                continue;
            }
            
            send_response(clientfd, "200 OK\n");
            
            /* Clear mail data buffer */
            memset(mail_data, 0, sizeof(mail_data));
            
            /* Read mail content until single dot */
            char line[BUFFER_SIZE];
            int data_size = 0;
            
            while (1) {
                memset(line, 0, sizeof(line));
                int line_read = read(clientfd, line, sizeof(line) - 1);
                
                if (line_read <= 0) {
                    break;
                }
                
                line[line_read] = '\0';
                
                /* Check if this is the end of message */
                if (strcmp(line, ".\n") == 0 || strcmp(line, ".\r\n") == 0) {
                    break;
                }
                
                /* Append to mail data if there's room */
                if (data_size + line_read < sizeof(mail_data) - 1) {
                    strcat(mail_data, line);
                    data_size += line_read;
                }
            }
            
            /* Save the email with proper locking */
            save_email(recipient, sender, mail_data);
            printf("Email from %s to %s saved.\n", sender, recipient);
            send_response(clientfd, "200 Message stored successfully\n");
            
            /* Reset sender and recipient for next email */
            memset(sender, 0, sizeof(sender));
            memset(recipient, 0, sizeof(recipient));
        }
        
        /* LIST Command */
        else if (strncmp(buffer, "LIST", 4) == 0) {
            char email[100];
            if (sscanf(buffer, "LIST %s", email) == 1) {
                printf("LIST command for %s\n", email);
                list_emails(clientfd, email);
            } else {
                send_response(clientfd, "400 ERR Invalid command syntax\n");
            }
        }
        
        /* GET_MAIL Command */
        else if (strncmp(buffer, "GET_MAIL", 8) == 0) {
            char email[100];
            int mail_id;
            
            if (sscanf(buffer, "GET_MAIL %s %d", email, &mail_id) == 2) {
                printf("GET_MAIL command for %s, ID: %d\n", email, mail_id);
                get_email(clientfd, email, mail_id);
            } else {
                send_response(clientfd, "400 ERR Invalid command syntax\n");
            }
        }
        
        /* QUIT Command */
        else if (strncmp(buffer, "QUIT", 4) == 0) {
            send_response(clientfd, "200 Goodbye\n");
            printf("Client requested to quit.\n");
            unregister_connection(my_pid);
            close(clientfd);
            exit(0);
        }
        
        /* Unknown Command */
        else {
            send_response(clientfd, "400 ERR Invalid command\n");
        }
    }
}

int main(int argc, char *argv[]) {
    int port = 2525; /* Default port */
    
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    /* Setup signal handlers */
    signal(SIGCHLD, sigchld_handler);
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);
    
    /* Initialize synchronization primitives */
    sem_id = init_semaphore();
    if (sem_id == -1) {
        fprintf(stderr, "Failed to initialize semaphore\n");
        exit(EXIT_FAILURE);
    }
    
    /* Initialize shared memory */
    init_shared_memory();
    
    struct sockaddr_in server_addr, client_addr;
    int server_fd, client_fd;
    socklen_t addr_len = sizeof(client_addr);
    
    /* Create socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        cleanup_shared_memory();
        exit(EXIT_FAILURE);
    }
    
    /* Allow reuse of address/port */
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        cleanup_shared_memory();
        exit(EXIT_FAILURE);
    }
    
    /* Prepare the sockaddr_in structure */
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    /* Bind */
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        cleanup_shared_memory();
        exit(EXIT_FAILURE);
    }
    
    /* Listen */
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        cleanup_shared_memory();
        exit(EXIT_FAILURE);
    }
    
    /* Ensure mailbox directory exists */
    ensure_mailbox_dir();
    
    printf("My_SMTP Server listening on port %d...\n", port);
    printf("Server initialized with shared memory and semaphore support\n");
    
    /* Main server loop */
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("Client connected: %s\n", client_ip);
        
        /* Fork a child process to handle the client */
        pid_t pid = fork();
        
        if (pid == 0) {
            /* Child process */
            close(server_fd);  /* Child doesn't need the listening socket */
            handle_client(client_fd);
            exit(0);
        } else if (pid > 0) {
            /* Parent process */
            close(client_fd);  /* Parent doesn't need the client socket */
        } else {
            /* Fork failed */
            perror("Fork failed");
            close(client_fd);
        }
    }
    
    /* Clean up resources */
    cleanup_shared_memory();
    close(server_fd);
    return 0;
}