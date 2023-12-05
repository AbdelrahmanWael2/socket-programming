/**
 * @file client.c 
 * @brief Client interface for interacting with a server using sockets
 * @authors Abdelrahman Wael, Ahmed Hesham
 * @date 2023-12-01
 *
 * This file contains the implementation of a simple client program
 * that communicates with a server using sockets. It includes functions
 * for sending GET and POST requests, handling file transfers, and
 * managing the communication with the server.
 */

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#define MIN_BUFFER_SIZE 256
#define MAX_BUFFER_SIZE 1024

//Global Variables
int port, read_size, server_socket = 0;
struct sockaddr_in serv_addr;
char server_msg[MAX_BUFFER_SIZE] = {0};

// Forward declarations
void startConnection();
void readServerMsg();
bool isOK();
void parseCommand(char *command, char *request, char *type, char *path);
void createRequest(char *request, char *type, char *path, char *header, char *body);
void sendRequests();
void readServerMsg();
void sendGetRequest(char* request, char *path);
void sendPostRequest(char* request, char *path);
void receiveFile(char *path);
void sendFile(char *path);



void startConnection()
{
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        exit(EXIT_FAILURE);
    }
    printf("Socket created...\n");

    //using localhost for port
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf("Invalid Address for connection \n");
    }
    if (connect(server_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("Connection Failed :<\n");
        exit(EXIT_FAILURE);
    }
    printf("Connection established successfully :>\n");
    readServerMsg();

	//empty buffer for server message
    memset(server_msg, 0, MAX_BUFFER_SIZE);

}



bool isOK()
{
    return (strstr(server_msg, "OK") != NULL);
}

void parseCommand(char *command, char *request, char *type, char *path)
{

	//parse the command line arguments (tokens:method, path)
    char *m = strtok(command, " ");
    char *p = strtok(NULL, " ");
    
    if (strstr(m, "get") != NULL){
        strcpy(type, "GET");
    }else{
        strcpy(type, "POST");
    }
	strcpy(path, p);
    createRequest(request, type, path, "", "");
}

void createRequest(char *request, char *type, char *path, char *header, char *body)
{
    sprintf(request, "%s %s HTTP/1.1\r\n %s\r\n %s", type, path, header, body);
}

void sendRequests()
{
    //open the file containing commands
    FILE *fp = fopen("commands.txt", "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);

    ssize_t readResult;
	char *line = NULL;
    size_t len = 0;
    char request[MAX_BUFFER_SIZE], type[MIN_BUFFER_SIZE], *path = malloc(256);

    //read commands from the file line by line
    while ((readResult = getline(&line, &len, fp)) != -1)
    {
        //parse the command
        parseCommand(line, request, type, path);

        if (strcmp(type, "GET") == 0)
        {
            sendGetRequest(request, path);
        }
        else if (strcmp(type, "POST") == 0)
        {
            sendPostRequest(request, path);
        }

        //clear buffers and variables for the next iteration
        memset(request, 0, MAX_BUFFER_SIZE);
        memset(server_msg, 0, MAX_BUFFER_SIZE);
        memset(type, 0, MIN_BUFFER_SIZE);
        memset(path, 0, MIN_BUFFER_SIZE);
    }

    //send a close request to the server
    createRequest(request, "CLOSE", "", "", "");
    send(server_socket, request, strlen(request), 0);

    fclose(fp);
    if (line)
        free(line);

    exit(EXIT_SUCCESS);
}

void readServerMsg()
{
    read_size = read(server_socket, server_msg, MAX_BUFFER_SIZE);
    if (read_size == -1)
    {
        perror("recv failed");
    }
    printf("\nServer: %s\n", server_msg);
}

void sendGetRequest(char* request, char *path )
{
    send(server_socket, request, strlen(request), 0);
    readServerMsg();
    if(isOK()){
        receiveFile(path);
    }
}

void sendPostRequest(char* request, char *path)
{
    send(server_socket, request, strlen(request), 0);
    readServerMsg();
    if(isOK()){
        sendFile(path);
        readServerMsg();
        if(!isOK()){
            sendPostRequest(request,path);
        }
    }
}

void receiveFile(char *path)
{
    FILE *file_pointer = fopen(path, "w");
    if (file_pointer == NULL) {
        perror("\nFile open failed.");
        exit(EXIT_FAILURE);
    }

    char *rcv_buffer = (char *)malloc(MAX_BUFFER_SIZE * sizeof(char));
    if (rcv_buffer == NULL) {
        perror("\nMemory allocation failed.");
        exit(EXIT_FAILURE);
    }

    int rcv_bytes;
    while ((rcv_bytes = read(server_socket, rcv_buffer, MAX_BUFFER_SIZE)) > 0)
    {
        if ((fwrite(rcv_buffer, sizeof(char), rcv_bytes, file_pointer)) < rcv_bytes)
        {
            perror("\nFile write failed.");
            exit(EXIT_FAILURE);
        }

        printf("%.*s", rcv_bytes, rcv_buffer);
        
        memset(rcv_buffer, 0, MAX_BUFFER_SIZE);

        if (rcv_bytes != MAX_BUFFER_SIZE){
            break;
        }
    }

    fclose(file_pointer);
    free(rcv_buffer);

    printf("\nFile Received Successfully\n");
}

void sendFile(char *path)
{
    printf("\nSending File %s...\n", path);

    FILE *fs = fopen(path, "r");
    if (fs == NULL) {
        perror("\nFile open failed.");
        exit(EXIT_FAILURE);
    }

    char *send_buffer = (char *)malloc(MAX_BUFFER_SIZE * sizeof(char));
    if (send_buffer == NULL) {
        perror("\nMemory allocation failed.");
        exit(EXIT_FAILURE);
    }

    int read_bytes;
    while ((read_bytes = fread(send_buffer, sizeof(char), MAX_BUFFER_SIZE, fs)) > 0)
    {
        int send_bytes = send(server_socket, send_buffer, read_bytes, 0);
        if (send_bytes < 0)
        {
            perror("Failed to send file");
            exit(EXIT_FAILURE);
        }

        memset(send_buffer, 0, MAX_BUFFER_SIZE);

        if (feof(fs))
        {
            break;
        }
    }

    fclose(fs);
    free(send_buffer);

    printf("\nFile Sent Successfully.\n");
}

int main(int argc, char **argv)
{
	port = atoi(argv[2]); //8080
	//setting up socket connection (ip address and port)
	serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

	startConnection();
	sendRequests();

	return 0;
}
