/**
 * @file server.c 
 * @brief Server implementation
 * @authors Abdelrahman Wael, Ahmed Hesham
 * @date 2023-12-01
 *
 * This file contains the implementation of a simple server program
 *  using sockets. Mulithreading is used for fast communication and creation
 *  and synchronized with mutex locks
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/time.h> 
#define MIN_BUFFER_SIZE 256
#define MAX_BUFFER_SIZE 1024

//Global Variables
pthread_mutex_t lock;
float counter = 0;
fd_set fds;
struct timeval timeout;

char *connected_msg = "Connected to Server..";
char *ok_msg = "HTTP/1.1 200 OK\r\n";
char *NOTFOUND_MSG = "HTTP/1.1 404 Not Found\r\n";
char buffer[MAX_BUFFER_SIZE] = {0};

int master_socket, addrlen, port;

struct sockaddr_in address;


void createMasterSocket()
{
    int opt = 1;
    // create a master socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
                   sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // bind the socket to localhost and input port
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("\nListening on port %d \n", port);

    //  waits for the client to request initiating connection
    // set maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // accept the incoming connection
    puts("\nWaiting for connections ...");
}

int acceptConnection()
{
    int new_socket;
    if ((new_socket = accept(master_socket,(struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)                         
    {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    // inform user of socket number - used in send and receive commands
    printf("\nA new connection established ,Socket fd is %d , IP : %s At Port : %d \n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

    // send new connection message
    if (send(new_socket, connected_msg, strlen(connected_msg), 0) != strlen(connected_msg))
    {
        perror("send");
    }

    printf("\nResponse sent to client %d\n", new_socket);
    return new_socket;
}

void closeConnection(int sd)
{
    //A client triggered a close
    getpeername(sd, (struct sockaddr *)&address,
                (socklen_t *)&addrlen);
    printf("\nHost disconnected , IP: %s , Port #: %d \n",
           inet_ntoa(address.sin_addr), ntohs(address.sin_port));
    close(sd);
}


void sendMessage(int sd, char *msg)
{
    if (send(sd, msg, strlen(msg), 0) != strlen(msg))
    {
        perror("send");
    }
}


bool isCloseMessage(char *msg)
{
    return (strstr(msg, "CLOSE") != NULL);
}

void parseMessage(char *msg, char *type, char *path)
{
    char *m = strtok(msg, " ");
    char *p = strtok(NULL, " ");
    strcpy(type, m);
    strcpy(path, p);
}

void receiveFile(char *path, int sd)
{
    FILE *file_pointer = fopen(path, "w");
    char *rcv_buffer, *file_buffer;
    rcv_buffer = (char *)malloc(MAX_BUFFER_SIZE * sizeof(char));
    file_buffer = (char *)malloc(MAX_BUFFER_SIZE * sizeof(char));
    int rcv_bytes;
    memset(rcv_buffer, '0', sizeof(rcv_buffer));
    while ((rcv_bytes = read(sd, rcv_buffer, MAX_BUFFER_SIZE)) > 0)
    {

        if ((fwrite(rcv_buffer, sizeof(char), rcv_bytes, file_pointer)) < rcv_bytes){
            perror("File write failed");
            exit(EXIT_FAILURE);
        }
        strcpy(file_buffer, rcv_buffer);
        memset(rcv_buffer, '0', sizeof(rcv_buffer));

        if (rcv_bytes != MAX_BUFFER_SIZE){
            break;
        }
    }

    fclose(file_pointer);
    printf("\nFile Recieved Successfully\n");
    sendMessage(sd, ok_msg);
}

void sendFile(char *path, int sd)
{
    FILE *fs;
    if ((fs = fopen(path, "r")) == 0)
    {
        sendMessage(sd, NOTFOUND_MSG);
        return;
    }
    sendMessage(sd, ok_msg);
    printf("\nSending File %s...\n", path);

    char *rcv_buffer;
    int rcv_bytes;
    rcv_buffer = (char *)malloc(MAX_BUFFER_SIZE * sizeof(char));
    while ((rcv_bytes = fread(rcv_buffer, sizeof(char), MAX_BUFFER_SIZE, fs)) > 0)
    {
        int send_bytes = send(sd, rcv_buffer, rcv_bytes, 0);
        if (send_bytes < 0)
        {
            perror("Failed to send file");
            exit(EXIT_FAILURE);
        }

        memset(rcv_buffer, '0', sizeof(rcv_buffer));
        if (feof(fs))
        {
            free(rcv_buffer);
            break;
        }
    }
    printf("\nFile Sent Successfully.\n");
}

void handleGetRequest(char *path, int sd)
{
    sendFile(path, sd);
}

void handlePostRequest(char *path, int sd)
{
    sendMessage(sd, ok_msg);
    receiveFile(path, sd);
}

void *handle_connection(void *socket_desc)
{
    pthread_mutex_lock(&lock);
    //if(counter > 1){
        timeout.tv_sec =  10/counter; //congestion dependant
    //}else{
       // timeout.tv_sec = 1;
    //}
    pthread_mutex_unlock(&lock);
    int sd = *((int *)socket_desc); //get socket descriptor
    int read_size;
    char client_message[MAX_BUFFER_SIZE], *type = malloc(256), *path = malloc(256);

    while (1)
    {   
        int t = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
        if (t == 0){
            printf("\nTimeout\n");
            closeConnection(sd);
            return 0;
        }
        read_size = read(sd, client_message, MAX_BUFFER_SIZE);
        if (read_size == -1){
            perror("recv failed");
        }
        printf("\nClient %d : %s \n", sd, client_message);
        if (isCloseMessage(client_message)){
            closeConnection(sd);
            break;
        }
        parseMessage(client_message, type, path);
        if (strcmp(type, "POST") == 0){
            handlePostRequest(path, sd);
        }
        else if (strcmp(type, "GET") == 0){
            handleGetRequest(path, sd);
        }
        memset(client_message, 0, MAX_BUFFER_SIZE);memset(type, 0, 256);memset(path, 0, 256);
    }
    // pthread_mutex_lock(&lock);
    // counter--;
    // timeout.tv_sec = 3 / counter;
    // pthread_mutex_unlock(&lock);
    return 0;
}

int main(int argc, char const *argv[])
{
    //storing IP and Port of the server socket
    port = atoi(argv[1]);
    address.sin_family = AF_INET;

    address.sin_addr.s_addr = INADDR_ANY;

    address.sin_port = htons(port);
    addrlen = sizeof(address);
    FD_ZERO(&fds);

    createMasterSocket();

    while (1)
    {
        int new_socket = acceptConnection();

        //this is atomic to count the number of sockets correctly
        pthread_mutex_lock(&lock);
        FD_SET(new_socket, &fds);
        counter++;
        pthread_mutex_unlock(&lock);
        
        //a new thread will handle new connections
        pthread_t thread_id;
        int *pclient = malloc(sizeof(int));
        *pclient = new_socket;
        pthread_create(&thread_id, NULL, handle_connection, pclient);
    }
}

