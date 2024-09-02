#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <stdarg.h>
#include <arpa/inet.h>

#define PORT 9000

int server_fd=0;
int new_socket=0;
int txtfd=0;
int debug=0;

static void signal_handler(int signum);
static void daemonize();
static void socket_comm();
static void closefds(int fd);

struct sigaction sa;

struct sockaddr_in address;
int addrlen = sizeof(address);
char client_ip[INET_ADDRSTRLEN]={0};

static size_t buffer_size = 2048;
char *buffer;

int main(int argc, char *argv[]) {
    // syslog looger
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    //Set up signal action structure
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    
    //Signal handler call
    if (sigaction(SIGINT, &sa, NULL) == -1) printf("SIGINT signal actions failed to run!!\n");
    if (sigaction(SIGTERM, &sa, NULL) == -1) printf("SIGTERM: signal actions failed to run!!\n");

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set the socket options to reuse the address and port
    /* (Warning on terminal) bind failed: Address already in use
     * Sometime OS keep socket in TIME_WAIT state to handle delayed packets properly.
     * SO_REUSEADDR option helps to bind socket even it is in TIME_WAIT state
     * opt = 1, enables this option.
    */
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Error setting socket options");
        closefds(2);
    }

    // Set up address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        closefds(2);
    }

    if (argc > 1) {
        if (!strcmp(argv[1],"-d")) {
            daemonize();
            while (1) {
                socket_comm();
            }
        } else if (!strcmp(argv[1],"-v")) {
            debug = 1;
            while (1) {
                socket_comm();
            }
        }
		else printf("[OPTION]: [USAGE]\n\n \
                    -d\t: run socket application as daemon\n \
                    -v\t: print messages\n \
                    ./test -d\n\n");
	}
	else {
        printf("No argument specified\n");
        while (1) {
            socket_comm();
        }
    }

    return 0;
} //main


static void socket_comm() {
    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        closefds(2);
    }
    if (debug) printf("Server listening on port %d\n\n", PORT);

    // Accept a connection
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        closefds(2);
    }

    // Convert the IP address to a human-readable form
    inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
    if (debug) printf("Client connected from IP: %s, port: %d\n\n", client_ip, ntohs(address.sin_port));

    syslog(LOG_INFO,"Accepted connection from %s",client_ip);

    // Open
    if ((txtfd = open("/var/tmp/aesdsocketdata", O_RDWR | O_APPEND | O_CREAT , 0664)) < 0) {
        perror("open");
        closefds(3);
    }

    //Allocatin memory to buffer
    buffer = (char *)malloc(buffer_size);
    if (buffer == NULL) {
        perror("malloc");
        closefds(3);
    }

    //Read
    ssize_t bytes_received;
    while ((bytes_received = recv(new_socket, buffer, buffer_size, 0)) > 0) {
        if (debug) printf(">> %ld bytes received from client\n", bytes_received);
        // Write
        ssize_t bytes_written = write(txtfd, buffer, bytes_received);
        if (bytes_written < 0) {
            perror("write");
            closefds(4);
        }
        if (debug) printf(">> %ld bytes append to /var/tmp/aesdsocketdata\n", bytes_written);

        if (bytes_received == (buffer_size)) {
            // Incresing buffer size can be one solution but can be problemcatic with big data
            // buffer_size += buffer_size;
            // char *new_buffer = realloc(buffer, buffer_size);
            // if (new_buffer == NULL) {
            //     perror("malloc");
            //     free(buffer);
            //     closefds(3);
            // }
            // buffer = new_buffer;
            memset(buffer, 0, buffer_size-1);
            continue;
        }
        else {
            break;
        }
    }
    
    // Move the file pointer to the end of the file
    off_t fileSize = lseek(txtfd, 0, SEEK_END);
    if (fileSize == -1) {
        perror("lseek1");
        closefds(4);
    }
    // lseek
    if ( lseek(txtfd, 0, SEEK_SET) < 0 ) {
        perror("lseek2");
        closefds(4);
    }


    /* If I use realloc to increase size of buffer it may cause memory overwrite issue
     * When enough heap memory not available.
     * So, I am using same buffer with 2 MB size and writing data to file in ...
     * read from file sent 2 MB by 2 MB back to client to eliminate heap issue
     * n_bytes declare to do read from file so no extra bytes sent to client.
    */
    size_t n_bytes;
    if (fileSize <= (buffer_size) ) n_bytes = fileSize;
    else n_bytes = buffer_size;

    ssize_t bytes_read;
    while ((bytes_read = read(txtfd, buffer, n_bytes)) > 0) {
        if (debug) printf(">> %ld bytes read from /var/tmp/aesdsocketdata\n", bytes_read);

        ssize_t bytes_sent = send(new_socket, buffer, bytes_read, 0);
        if (bytes_sent < 0) {
            perror("send");
            closefds(4);
        }
        if (debug) printf(">> %ld bytes sent to client\n\n", bytes_sent);

        fileSize -= bytes_read;
        if (fileSize == 0) break;
        if (fileSize <= (buffer_size) ) n_bytes = fileSize;
        else n_bytes = buffer_size;
    }

    if (debug) printf("Connection closed from %s\n",client_ip);
    syslog(LOG_INFO,"Closed connection from %s\n\n",client_ip);

    close(new_socket);
    close(txtfd);
    free(buffer);
} //socket_comm


static void signal_handler(int signum) {
	printf("signal handler called\n");
	switch(signum) {
		case SIGINT:
            printf("\nSIGINT called, closing aesdsocket application.\n\n");
            system("rm -f /var/tmp/aesdsocketdata");
            closefds(4);
            break;
		case SIGTERM:
            syslog(LOG_NOTICE, "%s", "SIGTERM called, closing aesdsocket application.");
            system("rm -f /var/tmp/aesdsocketdata");
            closefds(4);
            break;
	}
}

static void daemonize() {
    pid_t pid;

    // Fork off the parent process
    pid = fork();

    if (pid < 0) {
        // Fork failed
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        // Parent process, exit
        exit(EXIT_SUCCESS);
    }

    // Child process continues here

    // Create a new session ID for the child process
    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }

    // Fork again to ensure the process is not a session leader
    pid = fork();

    if (pid < 0) {
        // Fork failed
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        // Exit the second parent process
        exit(EXIT_SUCCESS);
    }

    // Close all open file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Optionally, redirect file descriptors 0, 1, and 2 to /dev/null
    open("/dev/null", O_RDWR); // stdin (fd 0)
    dup(0);                    // stdout (fd 1)
    dup(0);                    // stderr (fd 2)

    // The process is now daemonized
} //daemonize

static void closefds(int fd) {
    switch (fd) {
        case 4:
            close(txtfd);
        case 3:
            close(new_socket);
        case 2:
            close(server_fd);
        case 1:
            closelog();
        default:
            sleep(1); // Sometime OS keep socket in TIME_WAIT state to handle delayed packets properly.
            exit(fd);
    }
}