#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT "9000"
#define BACKLOG 10

//Fd variables
int sockfd;
int new_sockfd;
int txtfd;

void CLOSE(int n);
void signal_handler(int signum);
void daemonize();

int main() 
{
	struct sigaction sa;
	sa.sa_handler = signal_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGINT, &sa, NULL) == -1) printf("SIGINT signal actions failed to run!!\n");
	if (sigaction(SIGTERM, &sa, NULL) == -1) printf("SIGTERM: signal actions failed to run!!\n");

	// Variables for server
	int status;
	struct addrinfo hints;
	struct addrinfo *servinfo, *p;

	// Variables for client
	struct sockaddr_in client_addr;
	socklen_t client_addrlen = sizeof(client_addr);
	char client_ip[INET_ADDRSTRLEN]={0};

	//Receive & sent data 
	char buffer[1024] = {0};
	ssize_t bytes_received, bytes_written, bytes_read, bytes_sent;

	// Starting syslog logger
	openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
	syslog(LOG_INFO,"%s","aesdsocket application started...");

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if ( (status = getaddrinfo (NULL, PORT, &hints, &servinfo)) != 0 ) {
		syslog(LOG_ERR,"%s","getaddrinfo() failed...returning from here.");		
		CLOSE(1);
		return status;
	}
	syslog(LOG_INFO,"%s","getaddrinfo() succeed");

	// Loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
			// Create a socket
			if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			syslog(LOG_WARNING,"%s","socket creation failed");		
					continue;
			}

			// Bind the socket to the address provided by getaddrinfo
			if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			syslog(LOG_WARNING,"%s","socket binding to address failed");		
				close(sockfd);
					continue;
			}

			break; // If we get here, we have successfully bound the socket
	}

	if (p == NULL)
	{
		syslog(LOG_ERR,"%s","Failed to bind socket...returning from here.");
		CLOSE(2);
	}
	syslog(LOG_INFO,"%s","binding succeed");
	freeaddrinfo(servinfo);

	daemonize();

	while (1) {
		// Listen for incoming connections
		if (listen(sockfd, BACKLOG) == -1) {
			syslog(LOG_ERR,"%s","Failed to listen on socket...returning from here.");
			CLOSE(2);
		}

		syslog(LOG_INFO,"Server is listening on port %s",PORT);

		if ((new_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addrlen)) == -1) {
			syslog(LOG_ERR,"%s","Failed to accept connection...returning from here.");
				CLOSE(2);
		}

		// Convert the IP address to a human-readable form
		inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
		printf("Client connected from IP: %s, port: %d\n", client_ip, ntohs(client_addr.sin_port));

		syslog(LOG_INFO,"Accepted connection from %s",client_ip);

		// Receive 
		if ((bytes_received = recv(new_sockfd, buffer, sizeof(buffer)-1, 0)) == -1) {
			syslog(LOG_ERR,"%s","Receive data failed...returning from here.");
			CLOSE(3);
		} else if (bytes_received == 0) {
			printf("Connection closed!!\n");
			CLOSE(3);
		}
		syslog(LOG_INFO,"bytes_received = %ld | buffer = %s", bytes_received, buffer);

		// Open
		if ((txtfd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT , 0664)) == -1) {
			syslog(LOG_ERR,"%s","File opening error...returning from here.");
			CLOSE(3);
		}

		// Rrite
		if ((bytes_written = write(txtfd, buffer, bytes_received)) < 0) {
			syslog(LOG_ERR,"%s","writing to /var/tmp/aesdsocketdata failed!!");
			CLOSE(4);
		}
		syslog(LOG_INFO,"bytes_written = %ld | buffer = %s", bytes_written, buffer);

		// lseek
		if ( lseek(txtfd, 0, SEEK_SET) < 0 ) {
			syslog(LOG_ERR,"%s","lseek failed to move to begining of the file");
			CLOSE(4);
		}

		// Read 
		if ((bytes_read = read(txtfd, buffer, bytes_received)) < 0) {
			syslog(LOG_ERR,"%s","file read failed!!");
			CLOSE(4);
		}
		syslog(LOG_INFO,"bytes_read = %ld | buffer = %s", bytes_read, buffer);

		// Sent
		if ((bytes_sent = send(sockfd, buffer, bytes_read, 0)) < 0) {
			printf("Boos");
			syslog(LOG_ERR,"%s","Data sent failuer!!");
			CLOSE(4);
		}
		syslog(LOG_INFO,"bytes_sent = %ld | buffer = %s", bytes_sent, buffer);

		syslog(LOG_INFO,"%s","Data sent");

		CLOSE(0);
		syslog(LOG_INFO,"Close connection from %s",client_ip);
		sleep(10);
	}
}

//Exit function
void CLOSE(int n) {
	printf("closing ");
	switch(n){
		case 0:
		case 4:
			printf(" txtfd,");
			close(txtfd);
		case 3:
			printf(" new_sockfd,");
        	close(new_sockfd);
		case 2:
			printf(" sockfd,");
			close(sockfd);
		case 1:
			printf(" syslog.\n");
			closelog();
		default:
			if (n) exit(n);
			break;
	}
}

void signal_handler(int signum) {
	printf("signal handler called\n");
	switch(signum) {
		case SIGINT:
			printf("SIGINT: ");
			CLOSE(4);
			break;
		case SIGTERM:
			printf("SIGTERM: ");
			CLOSE(4);
			break;
	}
}

void daemonize() {
    pid_t pid, sid;

    // Fork the process
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    // Terminate the parent process
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // Create a new session
    sid = setsid();
    if (sid < 0) {
        perror("setsid");
        exit(EXIT_FAILURE);
    }

    // Fork again to ensure that the daemon cannot acquire a controlling terminal
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    // Terminate the parent process
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // // Change the working directory to root to avoid potential issues
    // if (chdir("/") < 0) {
    //     perror("chdir");
    //     exit(EXIT_FAILURE);
    // }

    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
	
    // The daemon is now running in the background
}
