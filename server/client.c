#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9000
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = "Hello from\n client";

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Send data to the server
    if (send(sock, buffer, strlen(buffer), 0) < 0) {
        perror("send");
    } else {
        printf("Sent to server: %s\n", buffer);
    }

    // Receive response from server
    ssize_t bytes_received = read(sock, buffer, BUFFER_SIZE);
    if (bytes_received < 0) {
        perror("read");
    } else {
        buffer[bytes_received] = '\0';  // Null-terminate the received data
        printf("Received from server: %s\n", buffer);
    }

    // Clean up
    close(sock);

    return 0;
}
