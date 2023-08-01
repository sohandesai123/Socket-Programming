#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <threads.h>
#include <pthread.h>
#include <semaphore.h>

sem_t semawhore;
int opened = 0;
int connected = 1;
FILE *fp;
char names[10][100];
int sockets[10];
int client_number = 0;

typedef struct
{
    int client_socket;
    int client_number;
    char ip[INET_ADDRSTRLEN];
    uint16_t port;
} params;

char *remove_substring(char *string, const char *substring)
{
    char *substring_start = strstr(string, substring);
    if (substring_start == NULL)
        return string;

    size_t substring_length = strlen(substring);
    memmove(substring_start, substring_start + substring_length, strlen(substring_start + substring_length) + 1);
    return string;
}

void clientreader(void *argv)
{
    
}

void clientsender(void *argv)
{

}

void serverhandler(void *argv)
{
    int instance_socket = ((params *)argv)->client_socket;
    // int client_number = ((params *)argv)->client_number;
    uint16_t port = ((params *)argv)->port;

    int *args = (int *)argv;
    int clientsocket = args[0];
    char socket[50];
    sprintf(socket, "%s", ((params *)argv)->ip);
    char message[1000] = {'\0'};
    char name[1000] = {'\0'};
    memset(message, 0, sizeof(message));
    read(clientsocket, message, 1000);
    if (strcmp("HELO", message) == 0)
    {
        send(clientsocket, "MESG What is your name?", 24, 0);
        memset(message, 0, sizeof(message));
        read(clientsocket, name, 1000);
        printf("%s \n", name);
        if (opened == 0)
        {
            fp = fopen("user.txt", "w");
            opened = 1;
        }
        else
        {
            fp = fopen("user.txt", "a");
        }

        while (connected == 0)
            ;
        connected = 0;
        printf("%s \n", name);
        memmove(name, name + 5, strlen(name));
        strcat(name, "@");
        strcat(name, socket);
        strcat(name, " ");
        fputs(name, fp);
        fclose(fp);
        connected = 1;
    }
    while (1)
    {
        memset(message, 0, sizeof(message));
        read(clientsocket, message, 1000);
        if (strcmp(message, "MESG EXIT") == 0)
        {
            printf("Client disconnected\n");
            while (connected == 0)
                ;
            connected = 0;
            FILE *temp = fopen("user.txt", "r");
            FILE *delete = fopen("delete.tmp", "w");
            memset(message, 0, sizeof(message));
            while (fscanf(temp, "%s", message) >= 0)
            {
                printf("%s \n", message);
                printf("%s \n", name);
                printf("%ld %ld \n", strlen(message), strlen(name));
                if (strstr(name, message) == NULL)
                {
                    fputs(message, delete);
                }
            }
            fclose(temp);
            fclose(delete);
            remove("user.txt");
            rename("delete.tmp", "user.txt");
            connected = 1;
            break;
        }
        else if (strcmp(message, "MESG LIST") == 0)
        {
            FILE *temp = fopen("user.txt", "r");
            memset(message, 0, sizeof(message));
            while (fgets(message, 1000, temp) != NULL)
            {
                send(clientsocket, message, strlen(message), 0);
            }
        }
        else if (strstr(message, "@") != NULL)
        {
            memmove(message, message + 5, strlen(message));
            char buffer[1000];
            int found = 0;
            FILE *temp = fopen("user.txt", "r");
            while (fgets(buffer, 1000, temp) != NULL)
            {
                printf("%s \n", buffer);
                printf("%s \n", message);
                if (strstr(buffer, message) != NULL)
                {
                    found = 1;
                    break;
                }
            }
            if (found)
            {
                send(clientsocket, "201 available", 14, 0);
            }
            else
                send(clientsocket, "404 not found", 14, 0);
        }
    }
}

void server(int port_number, char *ip)
{
    int serversocket, new_client;
    if ((serversocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket\n");
        exit(1);
    }
    printf("%d %s\n", port_number, ip);
    struct sockaddr_in serverport;
    serverport.sin_family = AF_INET;
    serverport.sin_port = htons(port_number);
    serverport.sin_addr.s_addr = inet_addr(ip);
    if (bind(serversocket, (struct sockaddr *)&serverport, sizeof(serverport)) < 0)
    {
        perror("Bind\n");
        exit(1);
    }

    if (listen(serversocket, 5) < 0)
    {
        perror("Listen\n");
        exit(1);
    }
    printf("Server is listening on port: %d\n", port_number);
    int len = sizeof(serverport);
    while (1)
    {
        if ((new_client = accept(serversocket, (struct sockaddr *)&serverport, &len)) >= 0)
        {
            pthread_t thread;
            void *args = malloc(1 * sizeof(int));
            ((int *)args)[0] = new_client;
            
            params *p = malloc(sizeof(params));
            p->client_socket = new_client;
            // p->client_number = i;
            strcpy(p->ip, ip);
            p->port = ntohs(port_number);

            pthread_create(&thread, NULL, (void *)serverhandler, (void *)p);
        }
    }
}

void client(int port_number, char *ip)
{
    int serversocket;
    if ((serversocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket\n");
        exit(1);
    }
    struct sockaddr_in serverport;
    unsigned int port;
    serverport.sin_family = AF_INET;
    serverport.sin_port = htons(port_number);
    serverport.sin_addr.s_addr = inet_addr(ip);
    if (connect(serversocket, (struct sockaddr *)&serverport, sizeof(serverport)) < 0)
    {
        perror("Could not find server\n");
        exit(1);
    }
    send(serversocket, "HELO", 5, 0);
    char message[1000] = {'\0'};
    memset(message, 0, sizeof(message));
    read(serversocket, message, 1000);
    char name[1000] = {'\0'};
    char socket[50];
    sprintf(socket, "%s", ip);
    if (strcmp(message, "MESG What is your name?") == 0)
    {
        printf("Please enter your username: ");
        memset(message, 0, sizeof(message));
        fgets(message, sizeof(message), stdin);
        memcpy(name, message, strlen(message) - 1);
        strcat(name, "@");
        strcat(name, socket);
        printf("%s \n", name);
        memmove(message + 5, message, strlen(message) + 1);
        memcpy(message, "MESG ", 5);
        message[strlen(message) - 1] = '\0';
        size_t len = strlen(message);
        send(serversocket, message, len, 0);
        printf("A\n");
    }
    while (1)
    {
        memset(message, 0, sizeof(message));
        printf("Enter message: ");
        fgets(message, sizeof(message), stdin);
        memmove(message + 5, message, strlen(message) + 1);
        memcpy(message, "MESG ", 5);
        message[strlen(message) - 1] = '\0';
        size_t len = strlen(message);
        printf("B\n");
        if (strcmp(message, "MESG EXIT") == 0)
        {
            send(serversocket, message, len, 0);
            printf("Disconnected\n");
            break;
        }
        else if (strcmp(message, "MESG LIST") == 0)
        {
            send(serversocket, message, len, 0);
            memset(message, 0, sizeof(message));
            read(serversocket, message, 1000);
            remove_substring(message, name);
            printf("%s \n", message);
            continue;
        }
        else if (strstr(message, "@") != NULL)
        {
            send(serversocket, message, len, 0);
            memset(message, 0, sizeof(message));
            read(serversocket, message, 1000);
            printf("%s \n", message);
            if (strcmp(message, "201 available") == 0)
            {
                // pthread_t thread1;
                // pthread_t thread2;

                // pthread_create(&thread1, NULL, clientreader, (void *)argv1);
                // pthread_create(&thread2, NULL, clientsender, (void *)argv2);
            }
        }
        else
        {
            if (send(serversocket, message, len, 0) < 0)
            {
                perror("Send");
                exit(1);
            }
        }

        /*memset(message, 0, sizeof(message));
        read(serversocket, message, 1000);
        printf("%s", message);*/
    }
}
int main(int argc, char *argv[])
{
    if (*argv[3] == 's')
    {
        server(atoi(argv[2]), argv[1]);
    }
    else if (*argv[3] == 'c')
    {
        client(atoi(argv[2]), argv[1]);
    }
    else
    {
        printf("Invalid argument\n");
    }
    return 0;
}
