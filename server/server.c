#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>

#define PORT 9000
#define BUFFER_SIZE 1024

int server_fd;
int new_socket;
int txtfd;

void signal_handler(int signum);

int main() {
    // syslog looger
	openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
	syslog(LOG_INFO,"%s","aesdsocket application started...");

    struct sigaction sa;
	sa.sa_handler = signal_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGINT, &sa, NULL) == -1) printf("SIGINT signal actions failed to run!!\n");
	if (sigaction(SIGTERM, &sa, NULL) == -1) printf("SIGTERM: signal actions failed to run!!\n");

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    char client_ip[INET_ADDRSTRLEN]={0};
    // const char *response = "Hello from server";

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
    while (1) {
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
        if ((txtfd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT , 0664)) < 0) {
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
        ssize_t bytes_read = read(txtfd, buffer, bytes_written);
        if (bytes_read < 0) {
            perror("read");
            close(server_fd);
            close(txtfd);
            exit(EXIT_FAILURE);
        }

        // Send response to client
        if (send(new_socket, buffer, bytes_read, 0) < 0) {
            perror("send");
        } else {
            printf("Sent to client: %s\n", buffer);
        }

        close(new_socket);
        syslog(LOG_INFO,"Closed connection from %s",client_ip);
    }
    return 0;
}

void signal_handler(int signum) {
	printf("signal handler called\n");
	switch(signum) {
		case SIGINT:
			printf("SIGINT: Closing application\n");
            close(new_socket);
            close(server_fd);
            closelog();
			break;
		case SIGTERM:
			printf("SIGTERM: Closing application\n");
            close(new_socket);
            close(server_fd);
            closelog();
			break;
	}
}
