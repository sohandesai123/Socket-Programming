#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
 
// NOTE: NO exit handling has been done
 
int client(char* ip, int port);
int server(int port);
 
int main(int argc, char **argv) {
    char *ip = argv[1];
    int port = atoi(argv[2]);
    char mode = argv[3][0];
 
    int status;
 
    if(mode == 'c') {
        status = client(ip, port);
    }
    else {
        status = server(port);
    }
 
    return status;
}
 
void *client_handler(void *args) {
    int* sock_details = (int*)args;
    int curr_client_socket = sock_details[0];
    int other_client_socket = sock_details[1];
 
    char buffer[1024] = {0};
 
    while(1) {
        memset(buffer, 0, 1024);
        read(curr_client_socket, buffer, 1024);
        printf("Client %d: %s", curr_client_socket, buffer);
        send(other_client_socket, buffer, strlen(buffer), 0);
    }
}
 
int server(int port) {
    int server_socket;
    struct sockaddr_in address;
 
    int opt = 1;
    int addrlen = sizeof(address);
 
    // Creating socket file descriptor
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
 
    int manual_SO_REUSEPORT = 15; // Intellisense issue
 
    // Forcefully attaching socket to the port
    if (setsockopt(server_socket, SOL_SOCKET,
                   SO_REUSEADDR | manual_SO_REUSEPORT, &opt,
                   sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
 
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
 
    // Binding socket to the port
    if (bind(server_socket, (struct sockaddr*)&address,
            sizeof(address))
        < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
 
    if (listen(server_socket, 1) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
 
    printf("\nServer is listening on port: %d\n\n", port);
 
 
    int client_socket_1, client_socket_2;
 
    // Accepting connections
    if ((client_socket_1 = accept(server_socket, (struct sockaddr*)&address,
                                (socklen_t*)&addrlen))
        < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
 
    printf("Client 1 connected\n");
 
    if ((client_socket_2 = accept(server_socket, (struct sockaddr*)&address,
                                (socklen_t*)&addrlen))
        < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
 
    printf("Client 2 connected\n");
 
    pthread_t client_handler_1, client_handler_2;
 
    int sock_details_1[2] = {client_socket_1, client_socket_2};
    int sock_details_2[2] = {client_socket_2, client_socket_1};
 
    pthread_create(&client_handler_1, NULL, client_handler, (void*)sock_details_1);
    pthread_create(&client_handler_2, NULL, client_handler, (void*)sock_details_2);
 
    pthread_join(client_handler_1, NULL);
    pthread_join(client_handler_2, NULL);
 
 
    // Shutdown the server
    printf("Server is shutting down...\n");
    shutdown(server_socket, SHUT_RDWR);
    return 0;
}
 
void *reading_thread(void *args) {
    int server_socket = *(int*)args;
    char buffer[1024] = {0};
 
    while(1) {
        memset(buffer, 0, 1024);
        read(server_socket, buffer, 1024);
        printf("Server: %s", buffer);
    }
}
 
void *sending_thread(void *args) {
    int server_socket = *(int*)args;
    char buffer[1024] = {0};
 
    while(1) {
        memset(buffer, 0, 1024);
        fgets(buffer, 1024, stdin);
        send(server_socket, buffer, strlen(buffer), 0);
    }
}
 
int client(char* ip, int port) {
    int server_socket, client_fd;
    struct sockaddr_in address;
 
    if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
 
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
 
    if(inet_pton(AF_INET, ip, &address.sin_addr) <= 0) {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }
 
    if((client_fd = connect(server_socket, (struct sockaddr*)&address, sizeof(address))) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
 
    printf("Connected to server\n");
 
    pthread_t reading_thread_id, sending_thread_id;
 
    pthread_create(&reading_thread_id, NULL, reading_thread, (void*)&server_socket);
    pthread_create(&sending_thread_id, NULL, sending_thread, (void*)&server_socket);
 
    pthread_join(reading_thread_id, NULL);
    pthread_join(sending_thread_id, NULL);
 
    return 0;
}