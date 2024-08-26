#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdlib.h>
#include <netinet/in.h>

#define PORT "9000"
#define BACKLOG 10

//Fd variables
int sockfd;
int new_sockfd;
int txtfd;

void CLOSE(int n);

int main() 
{
// Variables for server
int status;
struct addrinfo hints;
struct addrinfo *servinfo, *p;

// Variables for client
struct sockaddr *addr;
struct sockaddr_in client_addr;
socklen_t client_addrlen = sizeof(client_addr);
char client_ip[INET_ADDRSTRLEN]={0};

//Receive & sent data 
char buffer[1024];
ssize_t bytes_received, bytes_written, bytes_read, bytes_sent;

// Starting syslog logger
openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
syslog(LOG_INFO, "%s", "aesdsocket application started...");

memset(&hints, 0, sizeof(hints));
hints.ai_flags = AI_PASSIVE;
hints.ai_family = AF_INET;
hints.ai_socktype = SOCK_STREAM;

if ( (status = getaddrinfo (NULL, PORT, &hints, &servinfo)) != 0 ) {
	syslog(LOG_ERR, "%s", "getaddrinfo() failed...returning from here.");		
	CLOSE(1);
	return status;
}
syslog(LOG_INFO,"%s","getaddrinfo() succeed");

// Loop through all the results and bind to the first we can
for (p = servinfo; p != NULL; p = p->ai_next) {
        // Create a socket
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
		syslog(LOG_WARNING, "%s","socket creation failed");		
            	continue;
        }

        // Bind the socket to the address provided by getaddrinfo
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
		syslog(LOG_WARNING, "%s","socket binding to address failed");		
        	close(sockfd);
            	continue;
        }

        break; // If we get here, we have successfully bound the socket
}

if (p == NULL)
{
	syslog(LOG_ERR, "%s","Failed to bind socket...returning from here.");
	CLOSE(2);
	return -1;
}
syslog(LOG_INFO,"%s","binding succeed");

freeaddrinfo(servinfo);

// Listen for incoming connections
if (listen(sockfd, BACKLOG) == -1) {
	syslog(LOG_ERR, "%s","Failed to listen on socket...returning from here.");
	CLOSE(2);
	return -1;
}

syslog(LOG_INFO, "Server is listening on port %s",PORT);

new_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addrlen);
if ( new_sockfd == -1 ) {
	syslog(LOG_ERR, "%s", "Failed to accept connection...returning from here.");
        CLOSE(2);
	return -1;
}

// Convert the IP address to a human-readable form
inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
printf("Client connected from IP: %s, port: %d\n", client_ip, ntohs(client_addr.sin_port));

syslog(LOG_INFO, "Accepted connection from %s",client_ip);

//receive data
bytes_received = recv(new_sockfd, buffer, sizeof(buffer)-1, 0);
if (bytes_received < 0) {
	syslog(LOG_ERR,"%s","Receive data failed...returning from here.");
        CLOSE(3);
	return -1;
}
syslog(LOG_INFO,"%s","Data received");

FILE *file;
file = fopen("/var/tmp/aesdsocketdata","a+");

if ( file == NULL) {
	syslog(LOG_ERR, "%s","/var/tmp/aesdsocketdata file opening error...returning from here.");
	CLOSE(4);
	return -1;
}
printf("Debug %s\n",buffer);
strcat(buffer,"\n");
fputs(buffer,file);
syslog(LOG_INFO,"%s","Data written to file");

fclose(file);

txtfd = open("/var/tmp/aesdsocketdata", O_RDONLY);
if (txtfd == -1) {
	syslog(LOG_ERR, "%s", "File opening error...returning from here.");
	CLOSE(3);
	return -1;
}

// Read and send the file
while ((bytes_read = read(txtfd, (char *)buffer, sizeof(buffer))) > 0) {
        bytes_sent = send(sockfd, buffer, bytes_read, 0);
        if (bytes_sent < 0) {
	        syslog(LOG_ERR, "%s", "Data sent failuer!!");
        	CLOSE(4);
		return -1;
        }
}

if (bytes_read < 0) {
       	syslog(LOG_ERR, "%s", "file read failed!!");
	CLOSE(4);
	return -1;
}

syslog(LOG_INFO,"%s","Data sent");

CLOSE(0);
return 0;
}


//Exit function
void CLOSE(int n) {
	switch(n){
		case 1:
			closelog();
			break;
		case 2:
			close(sockfd);
			closelog();
			break;
		case 3:
        		close(new_sockfd);
			close(sockfd);
			closelog();
			break;
		case 4:
			close(txtfd);
        		close(new_sockfd);
			close(sockfd);
			closelog();
			break;
		case 0:
			close(txtfd);
        		close(new_sockfd);
			close(sockfd);
			closelog();
			break;
		default:
			break;
	}
}
