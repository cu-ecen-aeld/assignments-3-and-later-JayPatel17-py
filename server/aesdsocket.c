#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>

#define PORT 9000
#define BUFFER_SIZE 1024*4

int server_fd;
int new_socket;
int txtfd;

void signal_handler(int signum);
void daemonize();
static void socket_comm();

struct sigaction sa;

struct sockaddr_in address;
int addrlen = sizeof(address);
char buffer[BUFFER_SIZE] = {0};
char client_ip[INET_ADDRSTRLEN]={0};

int main(int argc, char *argv[]) {
    // syslog looger
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO,"%s","aesdsocket application started...");

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

    // Set up address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (argc > 1) {
		if (!strcmp(argv[1],"-d")) {
            daemonize();
            while (1) {
                socket_comm();
            }
        }
		else printf("[OPTION]: [USAGE]\n\n \
                    -d\t: run socket application as daemon\n \
                    ./test -d\n\n");
	}
	else {
        printf("No argument specified\n");
        while (1) {
            socket_comm();
        }
    }

    return 0;
}

static void socket_comm() {
    printf("Starting socket communication\n");
    
    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    // Accept a connection
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Convert the IP address to a human-readable form
    inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
    printf("Client connected from IP: %s, port: %d\n", client_ip, ntohs(address.sin_port));

    syslog(LOG_INFO,"Accepted connection from %s",client_ip);
    
    // Read data from client
    ssize_t bytes_received = read(new_socket, buffer, BUFFER_SIZE);
    if (bytes_received < 0) {
        perror("read");
    } else {
        buffer[bytes_received] = '\0';  // Null-terminate the received data
        printf("Received from client: %s\n", buffer);
    }

    // Open
    if ((txtfd = open("/var/tmp/aesdsocketdata", O_RDWR | O_APPEND | O_CREAT , 0664)) < 0) {
        perror("open");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // Write
    ssize_t bytes_written = write(txtfd, buffer, bytes_received);
    if (bytes_written < 0) {
        perror("write");
        close(server_fd);
        close(txtfd);
        exit(EXIT_FAILURE);
    }
    
    // lseek
    if ( lseek(txtfd, 0, SEEK_SET) < 0 ) {
        perror("lseel");
        close(server_fd);
        close(txtfd);
        exit(EXIT_FAILURE);
    }

    // Read 
    ssize_t bytes_read;
    while ((bytes_read = read(txtfd, buffer, BUFFER_SIZE)) > 0) {
        // Write the read data to standard output (or process it as needed)
        write(STDOUT_FILENO, buffer, bytes_read);
    }
    if (bytes_read < 0) {
        perror("read");
        close(server_fd);
        close(txtfd);
        exit(EXIT_FAILURE);
    }
    buffer[bytes_read] = '\n';

    // Send response to client
    if (send(new_socket, buffer, bytes_received, 0) < 0) {
        perror("send");
    } else {
        printf("Sent to client: %s\n", buffer);
    }

    syslog(LOG_INFO,"Closed connection from %s",client_ip);
    close(new_socket);

    printf("Connection closed\n");
}

void signal_handler(int signum) {
	printf("signal handler called\n");
	switch(signum) {
		case SIGINT:
            printf("SIGINT called, closing aesdsocket application.\n");
            close(new_socket);
            close(server_fd);
            closelog();
			break;
		case SIGTERM:
            syslog(LOG_NOTICE, "%s", "SIGTERM called, closing aesdsocket application.");
            close(new_socket);
            close(server_fd);
            closelog();
			break;
	}
}

void daemonize() {
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

    // // Set the working directory to the root directory
    // if (chdir("/") < 0) {
    //     exit(EXIT_FAILURE);
    // }

    // Close all open file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // // Optionally, redirect file descriptors 0, 1, and 2 to /dev/null
    // open("/dev/null", O_RDWR); // stdin (fd 0)
    // dup(0);                    // stdout (fd 1)
    // dup(0);                    // stderr (fd 2)

    // The process is now daemonized
}

