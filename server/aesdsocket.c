#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>
#include <time.h>

#define PORT            9000
#define BUFFER_SIZE     1024
#define MAX_CLIENT      100
#define TIMESTAMP_LEN   43

/* 
    (...) known as ellipsis.
    It is used in variadic function. Which accept list of elements seperated by comma.
    It can be use in function and as macro like below.
*/
#define DEBUG_PRINT(msg,...) printf( msg "\n" , ##__VA_ARGS__)
/* 
For function there are total 4 funciotns
    See below function which has all 4 variadic family funciotns
    void printNumbers(int count, ...) {
    (1) va_list args; // Declare argument list
    (2) va_start(args, count); // Initialize argument list with the last known parameter
        for (int i = 0; i < count; i++) {
    (3)     int num = va_arg(args, int); // Get next argument
            printf("%d ", num);
        }
    (4) va_end(args); // Clean up
        printf("\n");
    }
*/

int debug = 0;
const char *dataFILE = "/var/tmp/aesdsocketdata";

// Initialize the mutex
static pthread_mutex_t lockFILE = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lockLIST = PTHREAD_MUTEX_INITIALIZER;

// Socket data structure
#pragma pack(push, 1) // set memory alignment to 1 byte to avoid padding
typedef struct client_data {
    char IP[INET_ADDRSTRLEN];
    int port;
    int ID;
    pthread_t TID; //thread id
    int workDone;
} client_data;
#pragma pack(pop) // restore default memory alignment

// Self referencial struct for linked list
// Define the structure for a linked list node
typedef struct Node{
    client_data data;
    struct Node* next;
} Node;

// Initialize the head of the client list
Node* clientList = NULL;

// Function to append a new node at the end of the list
void append_client(Node** head_ref, client_data new_data) {
/*
    This function appends a new node at the end of the client list
*/
    // Allocate memory for the new node
    Node* new_node = (Node*)malloc(sizeof(Node));
    Node* last = *head_ref;

    // Assign data to the new node and set its next pointer to NULL
    new_node->data = new_data;
    new_node->next = NULL;

    // If the linked list is empty, make the new node the head
    if (*head_ref == NULL) {
        *head_ref = new_node;
        return;
    }

    // Otherwise, traverse to the end of the list
    while (last->next != NULL) {
        last = last->next;
    }

    // Append the new node at the end
    last->next = new_node;
}

// Function to remove a client from the linked list by clientID
void remove_client(Node** head_ref, int clientID) {
/*
*   This function removes a client from the client list
*/
    Node* temp = *head_ref;
    Node* prev = NULL;

    // If clientID was not present in the list
    if (temp == NULL) return;

    // If the head node itself holds the clientID to be deleted
    if (temp != NULL && temp->data.ID == clientID) {
        *head_ref = temp->next; // Changed head
        free(temp);             // Free old head
        return;
    }

    // Search for the clientID to be deleted, keep track of the previous node
    while (temp != NULL && temp->data.ID != clientID) {
        prev = temp;
        temp = temp->next;
    }

    // Unlink the node from the linked list
    prev->next = temp->next;

    free(temp); // Free memory
}
static void daemonize() {
/*
*   This function used to run this application as daemon
*/

    // daemon(0, 0); // This is direct way to run as daemon. But we are going it manually

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

static void signal_handler(int signum) {
/*
*   This fucntion handles the signal
*   It will also close the syslog and remove the dataFILE
*   exit(0) will close all fds and cleanup buffers before exiting
*/
    if(debug>=1) 
        DEBUG_PRINT("\nReceived signal: %d (%s)", signum, strsignal(signum));
    closelog();
    remove(dataFILE);
    exit(0);

    // We can setup different actions for different signals using switch case
} //signal_handler

static void run_options(int argc, char *argv[]) {
    int options;
    while ((options = getopt(argc, argv, "dv:h")) != -1) {
        switch (options) {
            case 'd':
                daemonize();
                break;
            case 'v':
                debug = 1; // By default debug depth with verbose
                debug = atoi(optarg); // Convert debug value to integer
                break;
            case 'h':
                printf( "Usage: ./aesdsocket [OPTION]\n\n"
                       "Description:\n"
                       "This program implements a socket server that listens for incoming connections,"
                       "accepts data from clients, and writes it to a file. It also reads data from the file"
                       "and sends it back to the clients. The server can run in daemon mode and supports"
                       "debugging options to control the verbosity of log messages.\n\n"
                       "Examples:\n"
                       "To run the server as a daemon:\n"
                       "./aesdsocket -d\n\n"
                       "To set the debug level to 1(default: 1, max: 3):\n"
                       "./aesdsocket -v 1\n\n"
                       "To show this help message:\n"
                       "./aesdsocket -h\n");
                break;
        }
    }
} //run_options

static void* timestamping() {
/*
*   This function prints timestamp to dataFILE every 10s
*/
    time_t sysTIME;
    struct tm* timeINFO;
    char bufferTIME[TIMESTAMP_LEN];
    int fileFD;

    // Open dataFILE file
    if ((fileFD = open(dataFILE, O_RDWR | O_SYNC | O_APPEND | O_CREAT , 0664)) < 0) {
        perror("[socket_comm] open");
        raise(SIGTERM);
    }

    while(1) {
        time(&sysTIME);
        timeINFO = localtime(&sysTIME);
        memset(bufferTIME, 0, TIMESTAMP_LEN); // Clear the entire buffer
        strftime(bufferTIME, 80, "timestamp: %a, %d %b %Y %H:%M:%S %z\n", timeINFO);
        
        pthread_mutex_lock(&lockFILE);
        {
            // Writes time data to dataFILE file
            if ((write(fileFD, bufferTIME, TIMESTAMP_LEN)) < 0) {
                perror("write");
                raise(SIGTERM);
            }
        }
        pthread_mutex_unlock(&lockFILE);
        sleep(10); // Sleep for 10 seconds
    }

    return NULL;
} //timestamping

static void* client_status(void *arg) {
/*
*   This function checks the status of each client in client list
*   If workDone is 1, it will close the client connection
*   and remove the client from the list
*/
// Node** clientList = (Node**)arg; // This way we can pass clinetList if we want to

    while (1) {
        Node* temp = clientList;
        pthread_mutex_lock(&lockFILE);
        while (temp != NULL) {
            if (temp->data.workDone == 1) {
                if (debug >= 3) DEBUG_PRINT("Client %d work done", temp->data.ID);
                pthread_join(temp->data.TID, NULL);
                close(temp->data.ID);
                if (debug >= 1) DEBUG_PRINT("Closed client %d connection from IP: %s", temp->data.ID, temp->data.IP);
                syslog(LOG_INFO, "Closed connection from IP: %s", temp->data.IP);
                remove_client(&clientList, temp->data.ID);
            } else {
                if (debug >= 3) DEBUG_PRINT("Client %d work not done", temp->data.ID);
            }
            temp = temp->next;
        }
        pthread_mutex_unlock(&lockFILE);
        sleep(2); // Sleep for 2 second
    }

    return NULL;
} //client_status

static void* socket_comm(void* args) {
/*
*   This function performs socket communication tasks
*   like reading data from client, writing to data file,
*   reading from data file and sending data to client
*/

    client_data* clientData = (client_data*)args;

    ssize_t bytesRECV;
    ssize_t bytesWRITE;
    ssize_t bytesREAD;
    ssize_t bytesSEND;
    
    char bufferRECV[BUFFER_SIZE];
    char bufferSEND[BUFFER_SIZE];
    
    int fileFD;

    // Open dataFILE file
    if ((fileFD = open(dataFILE, O_RDWR | O_SYNC | O_APPEND | O_CREAT , 0664)) < 0) {
        perror("[socket_comm] open");
        raise(SIGTERM);
    }

    // Read data from client
    if (debug>=2) DEBUG_PRINT("\n-\n-recv\n-\n");
    memset(bufferRECV, 0, BUFFER_SIZE); // Clear the entire buffer
    bytesRECV = recv(clientData->ID, bufferRECV, BUFFER_SIZE, 0);
    if (bytesRECV == -1) {
        perror("recv");
        raise(SIGTERM);
    }
    if (debug>=2) DEBUG_PRINT("bytesRECV = %ld\nReceived from client = %s", bytesRECV, bufferRECV);

    pthread_mutex_lock(&lockFILE);
    {
        // Write to aesdsocket
        if (debug>=2) DEBUG_PRINT("\n-\n-write\n-\n");
        if ((bytesWRITE = write(fileFD, bufferRECV, bytesRECV)) < 0) {
            perror("write");
            raise(SIGTERM);
        }
        if (debug>=2) DEBUG_PRINT("bytesWRITE = %ld", bytesWRITE);
        
        // Read from aesdsocket
        // pread function starts reading from position passed. last argument.
        if (debug>=2) DEBUG_PRINT("\n-\n-read\n-\n");
        if ((bytesREAD = pread(fileFD, bufferSEND, BUFFER_SIZE, 0)) < 0) {
            perror("read");
            raise(SIGTERM);
        }
        if (debug>=2) DEBUG_PRINT("bytesREAD = %ld\nRead from data_file = %s", bytesREAD, bufferSEND);
    }
    pthread_mutex_unlock(&lockFILE);

    fileFD = close(fileFD);
    if (fileFD < 0) {
        perror("close");
        raise(SIGTERM);
    }

    // Send data to client
    if (debug>=2) DEBUG_PRINT("\n-\n-send\n-\n");
    if ((bytesSEND = send(clientData->ID, bufferSEND, bytesREAD, 0)) < 0){
        perror("send");
        raise(SIGTERM);
    }
    if (debug>=2) DEBUG_PRINT("bytesSEND = %ld\nSent to the client = %s", bytesSEND, bufferSEND);

    Node* temp = clientList;
    while (temp != NULL) {
        if (temp->data.ID == clientData->ID) temp->data.workDone = 1;
        temp = temp->next;
    }

    return NULL;
} //socket_comm

static void start_server(int *serverFD, struct sockaddr_in address, int addrlen) {
/*
*   This function will perform below tasks
*   Initialize the head of the client list
*   Start timestamping thread
*   Accepts connection from client
*   Creates thread for each client
*   Starts socket communication
*   Remove client from list who completed task
*/

    // // Listen for incoming connections.
    // // It can queue up to 5 connections
    // if (listen(*serverFD, 5) < 0) {
    //     perror("listen");
    //     raise(SIGTERM);
    // }
    // DEBUG_PRINT("Server listening on port %d", PORT);

    // // Initialize the head of the client list
    // Node* clientList = NULL;

    // Clinet data
    int cnt = 0; // Max value: MAX_CLIENT
    client_data clientData[MAX_CLIENT];
    memset(clientData, 0, sizeof(clientData)); // Clear the entire array

    // Start timestamping thread
    // This will stamp time to dataFILE every 10s
    pthread_t timeStampTID;
    if (pthread_create(&timeStampTID, NULL, timestamping, NULL) !=0) {
        perror("pthread_create");
        raise(SIGTERM);
    }

    // Start client status thread
    // This will check the status of each client in linked list
    pthread_t clientStatusTID;
    if (pthread_create(&clientStatusTID, NULL, client_status, NULL) !=0) {
        perror("pthread_create");
        raise(SIGTERM);
    }

    // Accept connection from client
    // Create new thread for each clients
    // Max 100 clients can connect. then I will close socket
    while (cnt < MAX_CLIENT) {
        // Accept a connection
        clientData[cnt].ID = accept(*serverFD, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (clientData[cnt].ID == -1) {
            perror("accept");
            raise(SIGTERM);
        }
        
        // Convert the IP address to a human-readable form
        inet_ntop(AF_INET, &(address.sin_addr), clientData[cnt].IP, INET_ADDRSTRLEN);
        clientData[cnt].port = ntohs(address.sin_port);
        if(debug>=1) DEBUG_PRINT("Client %d connected from IP: %s, port: %d", \
                                    clientData[cnt].ID, \
                                    clientData[cnt].IP, \
                                    clientData[cnt].port);
        syslog(LOG_INFO,"Accepted connection from %s",clientData[cnt].IP);

        // Create new thread
        // Not passing direct ID since, 
        // pthread_create is taking reference it can conflict with other thread
        if (pthread_create(&clientData[cnt].TID, NULL, socket_comm, &clientData[cnt]) !=0) {
            perror("pthread_create");
            raise(SIGTERM);
        }
        pthread_mutex_lock(&lockLIST);
        append_client(&clientList, clientData[cnt]);
        pthread_mutex_unlock(&lockLIST);
        
        cnt++; // Max value: MAX_CLIENT

    /* 
    * One of ptread_join or pthread_detach thread has to use when creating thread.
    * If you are using detach thread make sure to close the cliendID in thread function.
    * join thread will wait for the thread to finish.
    * Check client status function.
    */  
    }
} //start_server

int main(int argc, char *argv[]) {
/*  
*   This is main funciton of programm. It will perform below tasks
*   Starts syslogger
*   Intialise signal handler        
*   Creates socket, bind it with IP and start listening
*   Starts server for accepting connection
*/

    // Parse command-line arguments (d: demonize, v: verbose(debug), h: help)
    run_options(argc, argv);

    // syslog looger
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO,"%s","aesdsocket application started...");

    // Intialise signal handler
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
	if (sigaction(SIGINT, &sa, NULL) == -1) DEBUG_PRINT("SIGINT signal actions failed to run!!");
	if (sigaction(SIGTERM, &sa, NULL) == -1) DEBUG_PRINT("SIGTERM: signal actions failed to run!!");
    if (sigaction(SIGSEGV, &sa, NULL) == -1) DEBUG_PRINT("SIGSEGV: signal actions failed to run!!");
    
    // File descriptor
    int serverFD = 0;
    
    // Set up address structure
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Create socket - serverFD
    if ((serverFD = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        raise(SIGTERM);
    }

    /* 
    * (Warning on terminal) bind failed: Address already in use
    * Sometime OS keep socket in TIME_WAIT state to handle delayed packets properly.
    * SO_REUSEADDR option helps to bind socket even it is in TIME_WAIT state
    * opt = 1, enables this option.
    */
    int opt = 1;
    // Set the socket options to reuse the address and port
    if (setsockopt(serverFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Error setting socket options");
        raise(SIGTERM);
    }

    // Bind socket - bind serverFD with IP address & PORT
    if (bind(serverFD, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind failed");
        raise(SIGTERM);
    }

    // Listen for incoming connections.
    // It can queue up to 5 connections
    if (listen(serverFD, 5) < 0) {
        perror("listen");
        raise(SIGTERM);
    }
    if(debug>=1) DEBUG_PRINT("Server listening on port %d", PORT);

    start_server(&serverFD, address, addrlen);
    return 0;
}