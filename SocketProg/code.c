#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include <time.h>

#define LIMIT 1024
#define MAX_CLIENTS 126

void *send_message_client(void *usr);
void *receive_message_client(void *usr);

int sockid;
uint16_t seq = 0;
char usr[2][LIMIT];
struct client
{
    int sockid;
    char username[LIMIT];
};
struct client clients[MAX_CLIENTS];

struct Header
{
    u_int8_t src;
    u_int8_t dest;
    u_int8_t len;
    u_int8_t flag;
    u_int8_t type;
    u_int16_t seq;
    u_int8_t chksum;
    u_int8_t unused;
    u_int8_t data[1024];
};

int getClientIndex(char *arg)
{
    char buf[LIMIT];
    strcpy(buf, arg);
    char *token = strtok(buf, " ");
    token = strtok(NULL, " ");
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (strcmp(clients[i].username, token) == 0)
        {
            return i;
        }
    }
    return -1;
}
void printPacket(struct Header *packet)
{
    printf("//////////////////////\n");
    printf("// PACKET RECEIVED  //\n");
    printf("//////////////////////\n");
    printf("// SRC     0x%02X\n", packet->src);
    printf("// DEST    0x%02X\n", packet->dest);
    printf("// LEN     0x%02X\n", packet->len);
    printf("// Flag    0x%02X\n", packet->flag);
    printf("// TYPE    0x%02X\n", packet->type);
    printf("// SEQ     0x%04X\n", packet->seq);
    printf("// Unused  0x%02X\n", packet->unused);
    printf("// CHKSUM  0x%02X\n", packet->chksum);
    printf("// Data    \n\n%s\n", packet->data);
    printf("\n//////////////////////\n");
}
void generatePacket(struct Header *packet, int src, int dest, int len, int type, int seq, int chksum, char data[])
{

    packet->src = src;
    packet->dest = dest;
    // packet->len = strlen(data);
    packet->type = type;
    packet->seq = seq;
    packet->flag = 0;
    packet->unused = 0;
    packet->chksum = chksum;
    bzero(packet->data, sizeof(packet->data));
    strcpy(packet->data, data);
    // calculate length of entire packet
    packet->len = strlen(data)+8;
    // calculate checksum, check overflow at each step and add to checksum
    packet->chksum = 0;
    for (int i = 0; i < len; i++)
    {
        packet->chksum += packet->data[i];
        if (packet->chksum < packet->data[i])
        {
            packet->chksum++;
        }
    }
    packet->chksum += packet->src;
    if (packet->chksum < packet->src)
    {
        packet->chksum++;
    }
    packet->chksum += packet->dest;
    if (packet->chksum < packet->dest)
    {
        packet->chksum++;
    }
    packet->chksum += packet->len;
    if (packet->chksum < packet->len)
    {
        packet->chksum++;
    }
    packet->chksum += packet->flag;
    if (packet->chksum < packet->flag)
    {
        packet->chksum++;
    }
    packet->chksum += packet->type;
    if (packet->chksum < packet->type)
    {
        packet->chksum++;
    }
    packet->chksum += packet->seq;
    if (packet->chksum < packet->seq)
    {
        packet->chksum++;
    }
    packet->chksum += packet->unused;
    if (packet->chksum < packet->unused)
    {
        packet->chksum++;
    }
    packet->chksum = ~packet->chksum;
}

void *connection_handler(void *num)
{
    int index = (int)num;

    struct Header recv_buffer;
    struct Header send_buffer;

    // process requests

    while (1)
    {
        recv(clients[index].sockid, &recv_buffer, sizeof(recv_buffer), 0);
        printPacket(&recv_buffer);
        if (strncmp(recv_buffer.data, "LIST", 4) == 0)
        {
            printf("%s: LIST\n", clients[index].username);
            char msg[LIMIT];
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (clients[i].sockid != -1)
                {
                    strncat(msg, clients[i].username, strlen(clients[i].username));
                    strcat(msg, "\n");
                }
            }
            generatePacket(&send_buffer, 0x80, index, 0, 0, seq, 0, msg);
            send(clients[index].sockid, &send_buffer, sizeof(send_buffer), 0);
            seq++;
        }
        else if (strncmp(recv_buffer.data, "CONN", 4) == 0)
        {
            printf("%s: %s\n", clients[index].username, recv_buffer.data);
            int i;
            if (recv_buffer.data[strlen(recv_buffer.data) - 2] != ':')
            {
                i = getClientIndex(recv_buffer.data);
            }
            else
            {
                for (i = 0; i < MAX_CLIENTS; i++)
                {
                    if (strncmp(clients[i].username, recv_buffer.data + 5, strlen(recv_buffer.data) - 7) == 0)
                    {
                        break;
                    }
                }
                if (i == MAX_CLIENTS)
                    i = -1;
            }
            if (i == -1)
            {
                generatePacket(&send_buffer, 0x80, index, 0, 0, seq, 3, "Invalid Username");
                send(clients[index].sockid, &send_buffer, sizeof(send_buffer), 0);
                seq++;
                // send(clients[index].sockid, "Invalid Username", sizeof("Invalid Username"), 0);
                break;
            }
            else
            {
                char msg[LIMIT];
                // bzero(send_buffer, sizeof(send_buffer));
                strcpy(msg, "CONN ");
                strcat(msg, clients[index].username);
                strcat(msg, recv_buffer.data + 5 + strlen(clients[i].username));
                generatePacket(&send_buffer, 0x80, i, 0, 1, seq, 0, msg);
                // printf("%s: %s\n", clients[index].username, send_buffer.data);
                send(clients[i].sockid, &send_buffer, sizeof(send_buffer), 0);
            }
        }
        else if (strncmp(recv_buffer.data, "MESG", 4) == 0)
        {
            // printf("%s: %s\n", clients[index].username, recv_buffer.data);
            int i = getClientIndex(recv_buffer.data);
            if (i == -1)
            {
                generatePacket(&send_buffer, 0x80, index, 0, 3, seq, 0, "Invalid Username");
                send(clients[index].sockid, &send_buffer, sizeof(send_buffer), 0);
                seq++;
            }
            else
            {
                if (strcmp(recv_buffer.data + 4 + strlen(clients[i].username) + 2, "EXIT") == 0)
                {
                    char msg[LIMIT];
                    strcpy(msg, "EXIT");
                    generatePacket(&send_buffer, 0x80, index, 0, 7, seq, 0, msg);
                    // printf("[%s]\n",send_buffer.data);
                    send(clients[i].sockid, &send_buffer, sizeof(send_buffer), 0);
                    seq++;
                    // send(clients[i].sockid, "EXIT", sizeof("EXIT"), 0);
                    printf("%s: EXIT MESSAGE SENT TO %s\n", clients[index].username, clients[i].username);
                    clients[index].sockid = -1;
                    bzero(clients[index].username, sizeof(clients[index].username));
                    return 0;
                }
                FILE *fp;
                fp = fopen("clientid1_clientid2.txt", "a");
                fprintf(fp, "%s: %s\n", clients[index].username, recv_buffer.data + 4 + strlen(clients[i].username) + 1);
                fclose(fp);
                char msg[LIMIT];
                strcpy(msg, "MESG ");
                strcat(msg, clients[index].username);
                strcat(msg, " ");
                strcat(msg, recv_buffer.data + 4 + strlen(clients[i].username) + 1);
                generatePacket(&send_buffer, 0x80, i, 0, 2, seq, 0, msg);
                send(clients[i].sockid, &send_buffer, sizeof(send_buffer), 0);
                seq++;
                printf("%s: MESSAGE SENT TO %s\n", clients[index].username, clients[i].username);
            }
        }
        else
        {
            generatePacket(&send_buffer, 0x80, index, 0, 3, seq, 0, "Invalid Username");
            send(clients[index].sockid, &send_buffer, sizeof(send_buffer), 0);
            seq++;
            // send(clients[index].sockid, "Invalid Command", sizeof("Invalid Command"), 0);
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{

    if (argc != 4)
    {
        printf("Usage: %s IP Port Mode\n", argv[0]);
        return -1;
    }
    struct sockaddr_in server_addr;
    sockid = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sockid, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    if (sockid == -1)
    {
        printf("Error: Socket cannot be created.\n");
        return -1;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    if (strcmp(argv[3], "s") == 0)
    {
        if (bind(sockid, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        {
            printf("[Server] Error: bind");
            close(sockid);
            return -1;
        }

        if (listen(sockid, 1) == -1)
        {
            printf("[Server] Error: Unable to start the server\n");
            return -1;
        }

        printf("Server is listening on %s:%s\n", inet_ntoa(server_addr.sin_addr), argv[2]);

        FILE *fp;
        fp = fopen("clientid1_clientid2.txt", "w");
        fclose(fp);

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            clients[i].sockid = -1;
        }

        socklen_t len = sizeof(server_addr);
        pthread_t client_threads[MAX_CLIENTS];
        struct Header recv_buffer;
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            clients[i].sockid = accept(sockid, (struct sockaddr *)&server_addr, (socklen_t *)&len);
            // get the username of the client
            recv(clients[i].sockid, &recv_buffer, sizeof(recv_buffer), 0);
            printPacket(&recv_buffer);
            // printf("Data received : %s\n",recv_buffer.data);
            int j;
            for (j = 0; j < strlen(recv_buffer.data); j++)
            {
                if (recv_buffer.data[j] == ':')
                {
                    break;
                }
            }
            strcpy(clients[i].username, (recv_buffer.data) + j + 2);
            printf("Client connected: %s\n", clients[i].username);
            generatePacket(&recv_buffer, 0x80, i, 0x00, 2, seq, 0x00, clients[i].username);
            // printf("%x\n", recv_buffer.src);
            send(clients[i].sockid, &recv_buffer, sizeof(recv_buffer), 0);
            seq++;
            // handle the client
            pthread_create(&client_threads[i], NULL, connection_handler, i);
        }
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            pthread_join(client_threads[i], NULL);
        }
    }
    else if (strcmp(argv[3], "c") == 0)
    {

        //--------------------------------------------------------------------------------

        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        getsockname(sockid, (struct sockaddr *)&client_addr, &len);

        if (connect(sockid, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        {
            printf("[Client] Error: Unable to connect to server.\n");
            return -1;
        }
        printf("[Client] %s:%d is connected to %s:%d\n", inet_ntoa(client_addr.sin_addr), (int)ntohs(client_addr.sin_port), inet_ntoa(server_addr.sin_addr), (int)ntohs(server_addr.sin_port));

        //--------------------------------------------------------------------------------

        // send the username to the server

        char buffer[LIMIT];
        printf("Enter username\n");
        scanf("%[^\n]%*c", buffer);
        strcpy(usr[0], buffer);
        char usr1[LIMIT];
        bzero(usr1, sizeof(usr1));
        strcpy(usr1, "MESG SERVER username: ");
        strcat(usr1, buffer);
        struct Header send_packet;
        generatePacket(&send_packet, 0x00, 0x80, 0x00, 2, seq, 0x00, usr1);
        send(sockid, &send_packet, sizeof(send_packet), 0);
        seq++;
        struct Header receive_packet;
        recv(sockid, &receive_packet, sizeof(receive_packet), 0);
        // printf("%x\n",receive_packet.src);
        // printf("%s\n",receive_packet.data);
        //--------------------------------------------------------------------------------

        pthread_t thread1;
        pthread_create(&thread1, NULL, send_message_client, receive_packet.dest);

        pthread_t thread2;
        pthread_create(&thread2, NULL, receive_message_client, receive_packet.dest);

        pthread_join(thread1, NULL);
        pthread_join(thread2, NULL);
    }
    close(sockid);
    return 0;
}

// implement the send message thread
void *send_message_client(void *arg)
{
    int src = arg;
    char mes[LIMIT];
    char mes1[LIMIT];
    struct Header send_packet;
    while (1)
    {
        bzero(mes, sizeof(mes));
        scanf("%[^\n]%*c", mes);
        if (strncmp(mes, "CONN", 4) == 0)
        {
            generatePacket(&send_packet, src, 0x80, 0x00, 1, seq, 0x00, mes);
            // send(sockid, mes, sizeof(mes), 0);
            send(sockid, &send_packet, sizeof(send_packet), 0);
            seq++;
        }
        else if (strncmp(mes, "LIST", 4) == 0)
        {
            generatePacket(&send_packet, src, 0x80, 0x00, 0, seq, 0x00, mes);
            send(sockid, &send_packet, sizeof(send_packet), 0);
            // send(sockid, mes, sizeof(mes), 0);
            seq++;
        }
        else if (strlen(usr[1]) != 0)
        {

            bzero(mes1, sizeof(mes1));
            strcpy(mes1, "MESG ");
            strcat(mes1, usr[1]);
            strcat(mes1, " ");
            strcat(mes1, mes);
            generatePacket(&send_packet, src, 0x80, 0x00, 2, seq, 0x00, mes1);
            // printf("%s\n", send_packet.data);
            send(sockid, &send_packet, sizeof(send_packet), 0);
            seq++;
            // send(sockid, mes1, sizeof(mes1), 0);
            if (strcmp(mes, "EXIT") == 0)
            {
                exit(0);
            }
        }
        else if (strcmp(mes, "EXIT") == 0)
        {
            exit(0);
        }
    }
}

// implement the receive message thread
void *receive_message_client(void *arg)
{
    struct Header buffer;
    while (1)
    {
        bzero(&buffer.data, sizeof(buffer.data));
        recv(sockid, &buffer, sizeof(buffer), 0);
        printPacket(&buffer);
        // printf("%s\n", buffer.data);
        if (strncmp(buffer.data, "EXIT",4) == 0)
        {
            printf("%s disconnected\n", usr[1]);
            bzero(usr[1], sizeof(usr[1]));
        }
        else if (strncmp(buffer.data, "CONN", 4) == 0)
        {

            if (strncmp(buffer.data + strlen(buffer.data) - 2, ":Y", 2) == 0)
            {

                strncpy(usr[1], buffer.data + 5, strlen(buffer.data) - 7);
                printf("Connection established with %s\n", usr[1]);
                continue;
            }
            else if (strncmp(buffer.data + strlen(buffer.data) - 2, ":N", 2) == 0)
            {
                printf("Connection failed\n");
                continue;
            }
            else if (strlen(usr[1]) == 0)
            {
                strcpy(usr[1], buffer.data + 5);
                char mes[LIMIT];
                bzero(mes, sizeof(mes));
                strcpy(mes, buffer.data);
                strcat(mes, ":Y");
                printf("Establising connection with %s\n", usr[1]);
                struct Header send_packet;
                generatePacket(&send_packet, buffer.dest, 0x80, 0x00, 1, seq, 0x00, mes);
                send(sockid, &send_packet, sizeof(send_packet), 0);
                bzero(buffer.data, sizeof(buffer.data));
                seq++;
                // send(sockid, buffer.data, sizeof(buffer), 0);
            }
            else
            {
                strcat(buffer.data, ":N");
                printf("Already connected to %s\n", usr[1]);
                struct Header send_packet;
                generatePacket(&send_packet, buffer.dest, 0x80, 0x00, 1, seq, 0x00, buffer.data);
                send(sockid, &send_packet, sizeof(send_packet), 0);
                seq++;
                bzero(buffer.data, sizeof(buffer.data));

                // send(sockid, buffer, sizeof(buffer), 0);
            }
            continue;
        }
        else if (strncmp(buffer.data, "MESG", 4) == 0)
        {

            time_t mytime = time(NULL);
            char *time_str = ctime(&mytime);
            time_str[strlen(time_str) - 1] = '\0';

            int index = 4 + strlen(usr[1]) + 2;
            printf("[%s] %s: %s\n", time_str, usr[1], buffer.data + index);
            bzero(buffer.data, sizeof(buffer.data));
        }
        else
        {
            printf("%s\n", buffer.data);
            bzero(buffer.data, sizeof(buffer.data));
        }
    }
}